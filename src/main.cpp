#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unistd.h>

#include "pf_consumer.hpp"
#include "pf_data.hpp"
#include "pf_parser.hpp"

// Prints usage information to stderr
void usage(const char* programName)
{
    std::cerr << "Usage: " << programName << " -d <path> [-o <output_file>]\n";
    std::cerr << "  -d <path>           Path to directory with .pf files\n";
    std::cerr << "  -o <output_file>    Optional: file to save results\n";
}

int main(int argc, char* argv[])
{
    std::string dirPath{"./Prefetch"};
    std::string outputPath{""};
    int opt;

    // Parse command line arguments using getopt
    while ((opt = getopt(argc, argv, "d:o:")) != -1)
    {
        if (opt == 'd')
        {
            dirPath = optarg;
        }
        else if (opt == 'o')
        {
            outputPath = optarg;
        }
        else
        {
            usage(argv[0]);
            return 1;
        }
    }

    // Directory path is required
    if (dirPath.empty())
    {
        usage(argv[0]);
        return 1;
    }

    // Create consumer using factory
    std::unique_ptr<PfConsumer> consumer = make_consumer(outputPath);

    if (!consumer)
    {
        usage(argv[0]);
        return 1;
    }

    // Iterate through directory entries
    for (const auto& entry : std::filesystem::directory_iterator(dirPath))
    {
        if (entry.path().extension().string() == ".pf")
        {
            if (std::optional<PfData> data = parsePfFile(entry.path().string()))
            {
                consumer->consume(*data);
            }
            else
            {
                std::cout << "cannot parse " << entry.path().string() << std::endl;
            }
        }
    }

    return 0;
}
