#include "consumer.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace prefetch::consumer
{

void PfConsoleConsumer::consume(const Record& record)
{
    std::cout << std::endl << "===== PREFETCH RECORD =====" << std::endl;
    std::cout << "PF File:        " << record.pfFileName << std::endl;
    std::cout << "Executable:     " << record.executableName << std::endl;
    std::cout << "File Size:      " << record.fileSize << std::endl;
    std::cout << "Volume Serial:  " << record.volumeSerial << std::endl;
    std::cout << "Volume Created: " << record.volumeCreateTime << std::endl;
    std::cout << "Run Count:      " << record.runCount << std::endl;

    std::cout << "Run Times:      " << record.runTimes.size() << std::endl;
    for (const auto& time : record.runTimes)
        std::cout << '\t' << time << std::endl;

    std::cout << "Files:          " << record.fileList.size() << std::endl;
    for (const auto& file : record.fileList)
        std::cout << '\t' << file << std::endl;

    std::cout << "Directories:    " << record.dirList.size() << std::endl;
    for (const auto& dir : record.dirList)
        std::cout << '\t' << dir << std::endl;
}

std::unique_ptr<PfConsumer> make_consumer(const std::string& output)
{
    return std::make_unique<PfConsoleConsumer>();
}

} // namespace prefetch::consumer
