#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Parsed data from a single .pf file
struct PfData
{
    std::string executableName;        // Name of the executable (e.g., "NOTEPAD.EXE")
    std::string pfFileName;            // Name of the .pf file itself
    uint32_t fileSize;                 // Size of the .pf file on disk
    uint32_t volumeSerial;             // Volume serial number
    std::string volumeCreateTime;      // Formatted: "YYYY.MM.DD hh:mm:ss"
    uint32_t runCount;                 // Number of times the program was executed
    std::vector<std::string> runTimes; // 8 formatted timestamps (empty string if zero)
    std::vector<std::string> fileList; // List of accessed files (UTF-8)
    std::vector<std::string> dirList;  // List of accessed directories (UTF-8)
};
