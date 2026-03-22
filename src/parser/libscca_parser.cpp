#include "libscca_parser.hpp"
#include "data.hpp"
#include <cstring>
#include <ctime>
#include <filesystem>
#include <libbfio.h>
#include <libscca.h>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace prefetch::parser::libscca
{

namespace
{

// Convert Windows FILETIME (100-ns intervals since 1601-01-01) to UTC string
static std::string filetime_to_string(uint64_t filetime)
{
    const uint64_t epoch_diff = 116444736000000000ULL; // 1601 to 1970 in 100-ns
    if (filetime < epoch_diff)
        return "";
    uint64_t seconds = (filetime - epoch_diff) / 10000000;
    std::time_t t = static_cast<std::time_t>(seconds);
    std::tm* tm = std::gmtime(&t);
    if (!tm)
    {
        return "";
    }
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buf);
}

static void handle_libscca_error(libscca_error_t** error)
{
    if (*error)
    {
        libscca_error_free(error);
        *error = nullptr;
    }
}

// Helper to safely get a UTF-8 string from libscca
static std::string get_utf8_filename(libscca_file_t* file, int index)
{
    uint8_t buffer[4096];
    libscca_error_t* error;
    if (libscca_file_get_utf8_filename(file, index, buffer, sizeof(buffer), &error) != 1)
    {
        handle_libscca_error(&error);
        return "";
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return reinterpret_cast<char*>(buffer);
}

} // anonymous namespace

std::optional<Record> parseFile(const std::string& filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        return std::nullopt;
    }

    libscca_file_t* file = nullptr;
    libscca_error_t* error = nullptr;

    // Initialize libscca file handle
    if (libscca_file_initialize(&file, &error) != 1)
    {
        handle_libscca_error(&error);
        return std::nullopt;
    }

    // Open the prefetch file
    if (libscca_file_open(file, filePath.c_str(), LIBBFIO_OPEN_READ, &error) != 1)
    {
        libscca_file_free(&file, &error);
        handle_libscca_error(&error);
        return std::nullopt;
    }

    // Prepare result structure
    Record result;
    result.pfFileName = std::filesystem::path(filePath).filename().string();
    result.fileSize = std::filesystem::file_size(filePath);

    // Volume information (serial number and creation time)
    libscca_volume_information_t* volume_info = nullptr;
    if (libscca_file_get_volume_information(file, 0, &volume_info, &error) == 1)
    {
        uint32_t serial = 0;
        if (libscca_volume_information_get_serial_number(volume_info, &serial, &error) == 1)
        {
            result.volumeSerial = serial;
        }
        handle_libscca_error(&error);

        uint64_t create_time = 0;
        if (libscca_volume_information_get_creation_time(volume_info, &create_time, &error) == 1)
        {
            result.volumeCreateTime = filetime_to_string(create_time);
        }
        handle_libscca_error(&error);

        libscca_volume_information_free(&volume_info, &error);
        handle_libscca_error(&error);
    }
    handle_libscca_error(&error);

    // Get executable name
    uint8_t exec_name_buffer[256] = {0};
    if (libscca_file_get_utf8_executable_filename(file, exec_name_buffer, sizeof(exec_name_buffer),
                                                  &error) == 1)
    {
        result.executableName = reinterpret_cast<char*>(exec_name_buffer);
    }
    handle_libscca_error(&error);

    // Get run count
    uint32_t run_count = 0;
    if (libscca_file_get_run_count(file, &run_count, &error) != 1)
    {
        libscca_file_free(&file, &error);
        handle_libscca_error(&error);
        return std::nullopt;
    }
    result.runCount = run_count;

    // Get run times - using the direct API function
    // According to API, we can get last run times by index
    for (int i = 0; i < 8; ++i) // Usually 8 run times stored
    {
        uint64_t run_time = 0;
        if (libscca_file_get_last_run_time(file, i, &run_time, &error) == 1)
        {
            std::string time_str = filetime_to_string(run_time);
            if (!time_str.empty())
            {
                result.runTimes.push_back(time_str);
            }
        }
        else
        {
            // If we get an error, break the loop (no more run times)
            handle_libscca_error(&error);
            break;
        }
        handle_libscca_error(&error);
    }

    // Number of filenames
    int num_filenames = 0;
    if (libscca_file_get_number_of_filenames(file, &num_filenames, &error) != 1)
    {
        handle_libscca_error(&error);
        return std::nullopt;
    }

    // Separate files and directories
    std::set<std::string> unique_dirs;
    for (int i = 0; i < num_filenames; ++i)
    {
        std::string full_path = get_utf8_filename(file, i);
        handle_libscca_error(&error);
        result.fileList.push_back(full_path);
        if (size_t pos = full_path.rfind('\\'); pos != std::string::npos)
        {
            unique_dirs.insert(full_path.substr(0, pos));
        }
    }

    for (const std::string& dir : unique_dirs)
    {
        if (!dir.empty())
        {
            result.dirList.push_back(dir);
        }
    }

    // Clean up
    libscca_file_free(&file, &error);
    handle_libscca_error(&error);

    return result;
}

} // namespace prefetch::parser::libscca
