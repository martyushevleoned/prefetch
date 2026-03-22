#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace prefetch
{

struct Record
{
    std::string executableName;
    std::string pfFileName;
    uint32_t fileSize;
    uint32_t volumeSerial;
    std::string volumeCreateTime;
    uint32_t runCount;
    std::vector<std::string> runTimes;
    std::vector<std::string> fileList;
    std::vector<std::string> dirList;
};

} // namespace prefetch
