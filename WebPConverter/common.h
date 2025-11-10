#ifndef __COMMON_H__
#define __COMMON_H__

// C++ Header
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <chrono>
#include <memory>
#include <future>
#include <thread>
#include <mutex>
#include <regex>
#include <fstream>

// gLib

// Data structure

// common
#include "TemplateManager.h"

// Third-party

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
static bool WriteMemoryToFile(const std::string& path, const uint8_t* data, size_t size)
{
    std::ofstream ofs(path, std::ios::binary);

    if (!ofs)
        return false;

    ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));

    return !!ofs;
}


#define FLOAT_EPSILON 0.00001
static bool CompareFloatValue(float a, float b, float epsilon = FLOAT_EPSILON)
{
    return (fabs(a - b) <= epsilon) ? true : false;
}

// Common global variable

#endif // __COMMON_H__
