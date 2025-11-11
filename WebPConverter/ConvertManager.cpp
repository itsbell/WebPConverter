#include "pch.h"
#include "ConvertManager.h"
#include "Common.h"
#include <webp/encode.h>  // libwebp 인코더 (WebPEncodeRGB, WebPFree 등)
#include <nvjpeg.h>
#include <cuda_runtime.h>
#include <afxdlgs.h>


ConvertManager::ConvertManager()
    : TemplateManager()
{
}

ConvertManager::~ConvertManager()
{
}

void ConvertManager::Load(LOAD_MODE eLoadMode)
{
    if (eLoadMode == LOAD_MODE_FILE)
    {
        CFileDialog fDlg(true);
        if (fDlg.DoModal() == IDOK)
        {
            m_vecImgPathList.clear();
            m_vecImgPathList.emplace_back(fDlg.GetPathName());
        }
    }
    else if (eLoadMode == LOAD_MODE_FOLDER)
    {
        CFolderPickerDialog Picker(NULL, OFN_FILEMUSTEXIST, NULL, 0);
        if (Picker.DoModal() == IDOK)
        {
            m_vecImgPathList.clear();

            CFileFind finder;
            CString strFilePath;
            CString strFileName;
            
            const CString strFolderPath = Picker.GetPathName() + _T("\\*.jpg");
            BOOL bWorking = finder.FindFile(strFolderPath);

            while (bWorking)
            {
                bWorking = finder.FindNextFile();

                if (finder.IsDirectory() || finder.IsDots())
                    continue;

                strFileName = finder.GetFileName();
                strFilePath = finder.GetFilePath();

                if (strFileName == _T("Thumbs.db"))
                    continue;

                if (strFileName.Find(_T(".DOCX")) >= 0)
                {
                    continue;
                }

                m_vecImgPathList.push_back(strFilePath);
            }
        }
    }
}

void ConvertManager::Convert()
{
    if (m_eJpegDecodeModule == TURBO_JPEG)
        Convert_CPU();
    else if(m_eJpegDecodeModule == NV_JPEG)
        Convert_GPU();
    else {}
}

void ConvertManager::Convert_CPU()
{
    tjhandle tj = tjInitDecompress(); // TurboJPEG 디코더 핸들 생성
    if (!tj)
    {
        std::cerr << "Error: tjInitDecompress 실패: " << tjGetErrorStr() << "\n";
        return;
    }

    for (size_t i = 0; i < m_vecImgPathList.size(); ++i)
    {
        do
        {
            std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
            std::chrono::time_point<std::chrono::high_resolution_clock> endTime;

            std::string strInPath = std::string(CT2A(m_vecImgPathList[i]));
            std::string strOutPath;

            int nPos = m_vecImgPathList[i].ReverseFind('.');
            if (nPos != -1)
                strOutPath = std::string(CT2A(m_vecImgPathList[i].Left(nPos) + _T(".webp")));
            
            // 1) JPEG 파일을 메모리로 읽기
            std::vector<uint8_t> vecJpegData;
            if (!ReadFileToMemory(strInPath, vecJpegData))
            {
                std::cerr << "Error: JPEG 파일을 읽지 못했습니다: " << strInPath << "\n";
                break;
            }

            // 2) 헤더 파싱
            uint8_t* pszRgbBuffer = nullptr;
            int nWidth = 0, nHeight = 0, nSubSampling = 0, nColorSpace = 0;
            int nStride;
            if (m_eJpegDecodeModule == TURBO_JPEG)
            {
                if (tjDecompressHeader3(tj, vecJpegData.data(), static_cast<unsigned long>(vecJpegData.size()), &nWidth, &nHeight, &nSubSampling, &nColorSpace) != 0)
                {
                    std::cerr << "Error: tjDecompressHeader3 실패: " << tjGetErrorStr() << "\n";
                    break;
                }
                
                if (nWidth <= 0 || nHeight <= 0)
                {
                    std::cerr << "Error: 잘못된 이미지 크기: " << nWidth << "x" << nHeight << "\n";
                    break;
                }
                
                // 현재 코드 경로는 Gray 전용으로 짜여 있으므로 gray가 아니면 건너뛰게 함.
                if (nSubSampling != TJSAMP_GRAY)
                {
                    std::cerr << "Info: 입력이 그레이스케일이 아니므로 이 경로는 권장되지 않습니다. nSubSampling=" << nSubSampling << "\n";
                    break;
                }
            }

            // 3) Y 평면 디코딩 (TJPF_GRAY)
            const int nYStride = nWidth;
            size_t nYSize = static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight);
            uint8_t* pszYPlane = static_cast<uint8_t*>(std::malloc(nYSize));
            if (!pszYPlane)
            {
                std::cerr << "Error: Y_plane 할당 실패\n";
                break;
            }
            startTime = std::chrono::high_resolution_clock::now();

            if (tjDecompress2(tj, vecJpegData.data(), static_cast<unsigned long>(vecJpegData.size()), pszYPlane, nWidth, nYStride, nHeight, TJPF_GRAY, 0) != 0)
            {
                std::cerr << "Error: tjDecompress2(TJPF_GRAY) 실패: " << tjGetErrorStr() << "\n";
                std::free(pszYPlane);
                break;
            }
            endTime = std::chrono::high_resolution_clock::now();

            // 4) (중요) Full-range Y(0..255) -> Limited-range Y(16..235) 로 매핑
            //    limited = round(y * 219/255) + 16
            for (size_t p = 0; p < nYSize; ++p)
            {
                int y = pszYPlane[p];
                // 정수산술로 반올림 처리 ((y*219 + 127) / 255)
                int y_limited = (y * 219 + 127) / 255;
                y_limited += 16;
                if (y_limited < 0)
                    y_limited = 0;
                if (y_limited > 255)
                    y_limited = 255;

                pszYPlane[p] = static_cast<uint8_t>(y_limited);
            }

            // 5) U/V 평면 준비 (여기서는 4:2:0으로 제공)
            const int uv_w = (nWidth + 1) / 2;
            const int uv_h = (nHeight + 1) / 2;
            size_t uv_size = static_cast<size_t>(uv_w) * static_cast<size_t>(uv_h);

            uint8_t* u_plane = static_cast<uint8_t*>(std::malloc(uv_size));
            uint8_t* v_plane = static_cast<uint8_t*>(std::malloc(uv_size));
            if (!u_plane || !v_plane)
            {
                std::cerr << "Error: UV plane 할당 실패\n";
                std::free(pszYPlane);
                if (u_plane)
                    std::free(u_plane);
                if (v_plane)
                    std::free(v_plane);
                break;
            }
            // 중성값(128)로 채움 (이미 limited-range에서는 128이 여전히 중성값)
            std::memset(u_plane, 128, uv_size);
            std::memset(v_plane, 128, uv_size);

            // 6) WebPPicture 설정 (planar YUV 직접 제공)
            WebPPicture picture;
            if (!WebPPictureInit(&picture)) {
                std::cerr << "Error: WebPPictureInit 실패\n";
                std::free(pszYPlane); std::free(u_plane); std::free(v_plane);
                break;
            }
            picture.width = nWidth;
            picture.height = nHeight;
            picture.use_argb = 0; // 0 = YUV 입력, 1 = ARGB 입력

            // 포인터와 스트라이드 설정
            picture.y = pszYPlane;
            picture.y_stride = nYStride;
            picture.u = u_plane;
            picture.v = v_plane;
            picture.uv_stride = uv_w;

            // 7) WebPMemoryWriter 준비 (인코딩 결과를 메모리로 받음)
            WebPMemoryWriter writer;
            WebPMemoryWriterInit(&writer);
            picture.writer = WebPMemoryWrite;
            picture.custom_ptr = &writer;

            // 8) WebPConfig 설정
            WebPConfig config;
            if (!WebPConfigInit(&config)) {
                std::cerr << "Error: WebPConfigInit 실패\n";
                WebPMemoryWriterClear(&writer);
                WebPPictureFree(&picture);
                std::free(pszYPlane); std::free(u_plane); std::free(v_plane);
                break;
            }

            config.lossless = 0;
            config.quality = m_fQuality;
            // 옵션 예: config.method = 4; // 품질-속도 균형
            // config.target_size = 0; // 품질 기반
            

            if (!WebPValidateConfig(&config))
            {
                std::cerr << "Error: WebPConfig 검증 실패\n";
                WebPMemoryWriterClear(&writer);
                WebPPictureFree(&picture);
                std::free(pszYPlane); std::free(u_plane); std::free(v_plane);
                break;
            }

            // 9) 인코딩 실행 (시간 측정은 인코딩 구간만)
            bool ok = false;
            if (!WebPEncode(&config, &picture))
            {
                std::cerr << "Error: WebPEncode 실패 (메시지를 확인하세요)\n";
            }
            else
            {
                ok = true;
            }

            // 10) 결과 저장 및 정리
            if (ok)
            {
                if (!WriteMemoryToFile(strOutPath, writer.mem, writer.size))
                {
                    std::cerr << "Error: 결과 파일 저장 실패: " << strOutPath << "\n";
                    ok = false;
                }
                else
                {
                    std::cout << "인코딩 성공: " << strOutPath << " (size=" << writer.size << " bytes)\n";
                }
            }

            WebPMemoryWriterClear(&writer);
            WebPPictureFree(&picture);

            // 메모리 해제
            std::free(pszYPlane);
            std::free(u_plane);
            std::free(v_plane);

            m_durationDecode = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime); // 경과 시간 계산 (밀리초)

        } while (false);

        TRACE("decoding time: %lld ms\n", m_durationDecode.count());
    }

    tjDestroy(tj); // TurboJPEG 핸들 반환
}

bool InitNvJpeg(nvjpegHandle_t& handle, nvjpegJpegState_t& state, cudaStream_t& stream)
{
    if (cudaStreamCreate(&stream) != cudaSuccess)
        return false;

    if (nvjpegCreate(NVJPEG_BACKEND_DEFAULT, nullptr, &handle) != NVJPEG_STATUS_SUCCESS)
        return false;

    if (nvjpegJpegStateCreate(handle, &state) != NVJPEG_STATUS_SUCCESS)
        return false;

    return true;
}

bool DecodeGrayJpegNvJpeg(nvjpegHandle_t handle, nvjpegJpegState_t state, cudaStream_t stream, const uint8_t* jpegData, size_t jpegSize, uint8_t** outYPlane, int& width, int& height)
{
    nvjpegChromaSubsampling_t subsampling;
    int nComponents;

    // nvjpegGetImageInfo 는 width/height "배열" 필요.
    int widths[NVJPEG_MAX_COMPONENT] = { 0 };
    int heights[NVJPEG_MAX_COMPONENT] = { 0 };

    if (nvjpegGetImageInfo(handle, jpegData, jpegSize, &nComponents, &subsampling, widths, heights) != NVJPEG_STATUS_SUCCESS)
    {
        std::cerr << "nvJPEG GetImageInfo failed\n";
        return false;
    }

    if (subsampling != NVJPEG_CSS_GRAY || nComponents != 1)
    {
        std::cerr << "Gray JPEG expected, got nComponents=" << nComponents
            << ", subsampling=" << subsampling << "\n";
        return false;
    }

    width = widths[0];
    height = heights[0];

    size_t ySize = static_cast<size_t>(width) * static_cast<size_t>(height);

    // *outYPlane = static_cast<uint8_t*>(std::malloc(ySize));
    // if (!*outYPlane)
    //     return false;

    // pinned memory 할당
    if (cudaMallocHost(outYPlane, ySize) != cudaSuccess)
    {
        std::cerr << "cudaMallocHost failed\n";
        return false;
    }

    nvjpegImage_t nvImage;
    memset(&nvImage, 0, sizeof(nvImage));

    nvImage.pitch[0] = width;
    uint8_t* d_yPlane = nullptr;
    if (cudaMalloc((void**)&d_yPlane, ySize) != cudaSuccess)
    {
        // std::free(*outYPlane);
        cudaFreeHost(*outYPlane);
        return false;
    }
    nvImage.channel[0] = d_yPlane;

    if (nvjpegDecode(handle, state, jpegData, jpegSize, NVJPEG_OUTPUT_Y, &nvImage, stream) != NVJPEG_STATUS_SUCCESS)
    {
        std::cerr << "nvJPEG decode failed\n";
        cudaFree(d_yPlane);
        // std::free(*outYPlane);
        cudaFreeHost(*outYPlane);
        *outYPlane = nullptr;
        return false;
    }

    cudaMemcpyAsync(*outYPlane, d_yPlane, ySize, cudaMemcpyDeviceToHost, stream);
    cudaStreamSynchronize(stream);
    cudaFree(d_yPlane);

    // full-range -> limited-range
    for (size_t i = 0; i < ySize; ++i)
    {
        int y = (*outYPlane)[i];
        int y_limited = (y * 219 + 127) / 255 + 16;
        if (y_limited < 0) y_limited = 0;
        if (y_limited > 255) y_limited = 255;
        (*outYPlane)[i] = static_cast<uint8_t>(y_limited);
    }

    return true;
}

void CreateGrayUVPlane(int width, int height, uint8_t** u_plane, uint8_t** v_plane)
{
    int uv_w = (width + 1) / 2;
    int uv_h = (height + 1) / 2;
    size_t uv_size = static_cast<size_t>(uv_w) * static_cast<size_t>(uv_h);

    *u_plane = static_cast<uint8_t*>(std::malloc(uv_size));
    *v_plane = static_cast<uint8_t*>(std::malloc(uv_size));
    
    if (*u_plane)
        std::memset(*u_plane, 128, uv_size);

    if (*v_plane)
        std::memset(*v_plane, 128, uv_size);
}

void ConvertManager::Convert_GPU()
{
    nvjpegHandle_t nvHandle = nullptr;
    nvjpegJpegState_t nvState = nullptr;
    cudaStream_t nvStream = nullptr;

    if (!InitNvJpeg(nvHandle, nvState, nvStream))
    {
        std::cerr << "nvJPEG initialization failed\n";
        return;
    }

    for (size_t i = 0; i < m_vecImgPathList.size(); ++i)
    {
        do
        {
            std::chrono::time_point<std::chrono::high_resolution_clock> startTime, endTime;
            std::string strInPath = std::string(CT2A(m_vecImgPathList[i]));
            std::string strOutPath;
            int nPos = m_vecImgPathList[i].ReverseFind('.');
            if (nPos != -1)
                strOutPath = std::string(CT2A(m_vecImgPathList[i].Left(nPos) + _T(".webp")));

            // JPEG -> 메모리
            std::vector<uint8_t> vecJpegData;
            if (!ReadFileToMemory(strInPath, vecJpegData))
            {
                std::cerr << "Error reading JPEG: " << strInPath << "\n";
                break;
            }

            // nvJPEG decode
            uint8_t* yPlane = nullptr;
            int width = 0, height = 0;
            startTime = std::chrono::high_resolution_clock::now();
            if (!DecodeGrayJpegNvJpeg(nvHandle, nvState, nvStream, vecJpegData.data(), vecJpegData.size(), &yPlane, width, height))
            {
                std::cerr << "nvJPEG decode failed: " << strInPath << "\n";
                break;
            }
            endTime = std::chrono::high_resolution_clock::now();

            // UV plane 준비
            uint8_t* uPlane = nullptr;
            uint8_t* vPlane = nullptr;
            CreateGrayUVPlane(width, height, &uPlane, &vPlane);

            // WebPPicture 설정
            WebPPicture picture;
            if (!WebPPictureInit(&picture))
            {
                std::cerr << "WebPPictureInit failed\n";
                // std::free(yPlane);
                cudaFreeHost(yPlane);
                std::free(uPlane);
                std::free(vPlane);
                break;
            }
            picture.width = width;
            picture.height = height;
            picture.use_argb = 0;
            picture.y = yPlane;
            picture.y_stride = width;
            picture.u = uPlane;
            picture.v = vPlane;
            picture.uv_stride = (width + 1) / 2;

            // WebPMemoryWriter
            WebPMemoryWriter writer;
            WebPMemoryWriterInit(&writer);
            picture.writer = WebPMemoryWrite;
            picture.custom_ptr = &writer;

            // WebPConfig
            WebPConfig config;
            if (!WebPConfigInit(&config))
            {
                std::cerr << "WebPConfigInit failed\n";
                WebPMemoryWriterClear(&writer);
                WebPPictureFree(&picture);
                // std::free(yPlane);
                cudaFreeHost(yPlane);
                std::free(uPlane);
                std::free(vPlane);
                break;
            }
            config.lossless = 0;
            config.quality = m_fQuality;

            if (!WebPValidateConfig(&config))
            {
                std::cerr << "WebPConfig validation failed\n";
                WebPMemoryWriterClear(&writer);
                WebPPictureFree(&picture);
                // std::free(yPlane);
                cudaFreeHost(yPlane);
                std::free(uPlane);
                std::free(vPlane);
                break;
            }

            // 인코딩
            bool ok = false;
            if (!WebPEncode(&config, &picture))
            {
                std::cerr << "WebPEncode failed\n";
            }
            else
            {
                ok = true;
            }

            // 결과 저장
            if (ok)
            {
                if (!WriteMemoryToFile(strOutPath, writer.mem, writer.size))
                {
                    std::cerr << "Error writing WebP: " << strOutPath << "\n";
                    ok = false;
                }
                else
                {
                    std::cout << "WebP encode success: " << strOutPath
                        << " (size=" << writer.size << " bytes)\n";
                }
            }

            WebPMemoryWriterClear(&writer);
            WebPPictureFree(&picture);
            // std::free(yPlane);
            cudaFreeHost(yPlane);
            std::free(uPlane);
            std::free(vPlane);

            m_durationDecode = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            TRACE("decode time: %lld ms\n", m_durationDecode.count());

        } while (false);
    }

    // nvJPEG cleanup
    if (nvState)
        nvjpegJpegStateDestroy(nvState);

    if (nvHandle)
        nvjpegDestroy(nvHandle);

    if (nvStream)
        cudaStreamDestroy(nvStream);
}
