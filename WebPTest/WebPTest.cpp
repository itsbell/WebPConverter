// jpeg_to_webp_turbo_webpencode.cpp
// Windows, C++14 예제
// 설명: libjpeg-turbo로 JPEG을 디코딩하고 libwebp의 WebPEncodeRGB 시그니처에 맞춰 손실 WebP로 저장
// 필요 라이브러리: libjpeg-turbo (turbojpeg.h / turbojpeg.lib), libwebp (webp/encode.h / webp.lib)
// 빌드 시: turbojpeg.lib, webp.lib 링크; 실행 시 필요한 DLL이 있으면 exe와 동일 폴더에 배포

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <turbojpeg.h>    // libjpeg-turbo TurboJPEG API
#include <webp/encode.h>  // libwebp 인코더 (WebPEncodeRGB, WebPFree 등)

// 파일을 바이너리로 읽어 vector에 저장
static bool ReadFileToMemory(const std::string& strPath, std::vector<uint8_t>& vecOut)
{
    std::ifstream ifs(strPath, std::ios::binary | std::ios::ate); // 파일 이진 모드로 열고 읽기 포인터를 파일 끝(ate)으로 위치시킴(이렇게 하면 곧바로 파일 크기를 획득)
    if (!ifs)
        return false; // 파일 열기에 실패시 false 반환

    std::streamsize sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    
    if (sz <= 0)
    {
        vecOut.clear();
        return true;
    }
    
    vecOut.resize(static_cast<size_t>(sz)); // 벡터 크기를 파일 크기만큼 조절해 읽기 버퍼를 준비합니다. (sz를 size_t로 캐스트)
    if (!ifs.read(reinterpret_cast<char*>(vecOut.data()), sz))
        return false;

    return true;
}

// 메모리(포인터+크기)를 파일에 이진으로 저장
static bool WriteMemoryToFile(const std::string& path, const uint8_t* data, size_t size) {
    std::ofstream ofs(path, std::ios::binary);
    
    if (!ofs)
        return false;
    
    ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    
    return !!ofs;
}

// JPEG -> WebP 변환 (손실) : libjpeg-turbo로 RGB 디코딩 후 WebPEncodeRGB 호출
bool ConvertJpegToWebP_Lossy_TurboJPEG_WebPEncodeRGB(const std::string& jpegPath, const std::string& webpPath, float quality)
{
    // 1) JPEG 파일을 메모리로 읽기
    std::vector<uint8_t> vecJpegData;
    if (!ReadFileToMemory(jpegPath, vecJpegData))
    {
        std::cerr << "Error: JPEG 파일을 읽지 못했습니다: " << jpegPath << "\n";
        return false;
    }

    // 2) TurboJPEG 디코더 초기화
    tjhandle tj = tjInitDecompress(); // TurboJPEG 디코더 핸들 생성
    if (!tj)
    {
        std::cerr << "Error: tjInitDecompress 실패: " << tjGetErrorStr() << "\n";
        return false;
    }

    int nWidth = 0, nHeight = 0, nSubSamp = 0, nColorSpace = 0;
    
    //  메모리에 로드된 JPEG 바이트(jpegData)의 헤더를 파싱하여 width/height/subsamp/colorspace 값을 얻습니다.
    if (tjDecompressHeader3(tj, vecJpegData.data(), static_cast<unsigned long>(vecJpegData.size()), &nWidth, &nHeight, &nSubSamp, &nColorSpace) != 0)
    {
        std::cerr << "Error: tjDecompressHeader3 실패: " << tjGetErrorStr() << "\n";
        tjDestroy(tj);
        return false;
    }

    if (nWidth <= 0 || nHeight <= 0) {
        std::cerr << "Error: 잘못된 이미지 크기: " << nWidth << "x" << nHeight << "\n";
        tjDestroy(tj);
        return false;
    }

    // 3) RGB 버퍼 할당 (3바이트 per pixel)
    const int nPixelSize = 3;
    const int nStride = nWidth * nPixelSize;
    size_t nNeeded = static_cast<size_t>(nWidth) * static_cast<size_t>(nHeight) * static_cast<size_t>(nPixelSize);
    // 간단한 오버플로우 검사
    if (nNeeded == 0 || nNeeded / nPixelSize / static_cast<size_t>(nHeight) != static_cast<size_t>(nWidth))
    {
        std::cerr << "Error: 이미지 크기 계산 오류(오버플로우 가능)\n";
        tjDestroy(tj);
        return false;
    }

    uint8_t* pszRgbBuffer = static_cast<uint8_t*>(std::malloc(nNeeded));
    if (!pszRgbBuffer)
    {
        std::cerr << "Error: RGB 버퍼 할당 실패\n";
        tjDestroy(tj);
        return false;
    }
    std::memset(pszRgbBuffer, 0, nNeeded);

    // 4) JPEG -> RGB 디코딩 (TJPF_RGB: R,G,B 순서)
    if (tjDecompress2(tj, vecJpegData.data(), static_cast<unsigned long>(vecJpegData.size()), pszRgbBuffer, nWidth, nStride, nHeight, TJPF_RGB, 0 /* flags */) != 0)
    {
        std::cerr << "Error: tjDecompress2 실패: " << tjGetErrorStr() << "\n";
        std::free(pszRgbBuffer);
        tjDestroy(tj);
        return false;
    }

    // 더 이상 TurboJPEG 핸들 필요 없음
    tjDestroy(tj);

    // 5) WebPEncodeRGB 호출
    uint8_t* pszWebpData = nullptr;
    size_t nWebpSize = WebPEncodeRGB(pszRgbBuffer, nWidth, nHeight, nStride, quality, &pszWebpData);
    if (nWebpSize == 0 || pszWebpData == nullptr) {
        std::cerr << "Error: WebPEncodeRGB 실패 (결과 크기 0 또는 NULL 포인터)\n";
        std::free(pszRgbBuffer);
        // webpData가 할당되었더라도 webpSize==0인 경우 WebPFree 안전하게 호출
        if (pszWebpData)
            WebPFree(pszWebpData);

        return false;
    }

    // 6) 결과를 파일로 저장
    bool wrote = WriteMemoryToFile(webpPath, pszWebpData, nWebpSize);
    if (!wrote) {
        std::cerr << "Error: WebP 파일 쓰기 실패: " << webpPath << "\n";
        WebPFree(pszWebpData);
        std::free(pszRgbBuffer);
        return false;
    }

    // 7) 정리
    WebPFree(pszWebpData);  // libwebp가 할당한 메모리 해제
    std::free(pszRgbBuffer);

    return true;
}

int main(int argc, char** argv)
{
    std::string inPath = "Crater.jpg";
    std::string outPath = "Output.webp";
    float quality = 80.0f;

    bool ok = ConvertJpegToWebP_Lossy_TurboJPEG_WebPEncodeRGB(inPath, outPath, quality);
    if (ok) {
        std::cout << "변환 성공: " << outPath << "\n";
        return 0;
    }
    else {
        std::cerr << "변환 실패\n";
        return 2;
    }
}