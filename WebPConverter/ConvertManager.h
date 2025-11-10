#pragma once

#include "TemplateManager.h"
#include <vector>
#include <turbojpeg.h>    // libjpeg-turbo TurboJPEG API

#define CONVERT_MGR ConvertManager::GetInstance()

enum LOAD_MODE
{
    LOAD_MODE_FILE = 0,
    LOAD_MODE_FOLDER
};

enum JPEG_DECODE_MODULE
{
    TURBO_JPEG = 0,
    NV_JPEG
};

enum DECODE_COLOR
{
    COLOR_RGB = 0,
    COLOR_YUV
};

class ConvertManager : public TemplateManager<ConvertManager>
{
public:
    ConvertManager();
    ~ConvertManager();

private:
    std::vector<CString> m_vecImgPathList;
    JPEG_DECODE_MODULE m_eJpegDecodeModule = TURBO_JPEG;
    DECODE_COLOR m_eDecodeColor = COLOR_RGB;
    std::chrono::milliseconds m_durationDecode;
    
    float m_fQuality = 80.0f;

    void LoadImagePathInDirectory(const std::string &strImgFolder);

public:
    void Load(LOAD_MODE eLoadMode);
    void Convert();
    void Convert_CPU();
    void Convert_GPU();

    void SetJpegDecodeModule(JPEG_DECODE_MODULE val) { m_eJpegDecodeModule = val; }
    void SetDecodeColor(DECODE_COLOR val) { m_eDecodeColor = val; }
    void SetQuality(float val) { m_fQuality = val; }

};
