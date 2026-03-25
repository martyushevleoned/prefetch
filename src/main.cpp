#include "consumer/consumer.hpp"
#include "parser/parser.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unistd.h>

using namespace prefetch;

// Prints usage information to stderr
void usage(const char* programName)
{
    std::cerr << "Usage: " << programName << " -d <path> [-o <output>]" << std::endl;
    std::cerr << "  -d <path>           Path to directory with .pf files" << std::endl;
    std::cerr << "  -o <output>         Optional: file to save results" << std::endl;
}

int main(int argc, char* argv[])
{
    std::string dirPath;
    std::string outputPath;
    int opt;

    while ((opt = getopt(argc, argv, "d:o:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dirPath = optarg;
            break;
        case 'o':
            outputPath = optarg;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (dirPath.empty())
    {
        usage(argv[0]);
        return 1;
    }

    std::unique_ptr<consumer::PfConsumer> consumer = consumer::make_consumer(outputPath);

    if (!consumer)
    {
        usage(argv[0]);
        return 1;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath))
    {
        if (entry.path().extension().string() == ".pf")
        {
            std::optional<Record> data = parser::parseFile(entry.path().string());
            if (data)
            {
                consumer->consume(*data);
            }
            else
            {
                std::cerr << "cannot parse " << entry.path().string() << std::endl;
            }
        }
    }

    return 0;
}
