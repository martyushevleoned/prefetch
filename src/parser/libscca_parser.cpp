#include "libscca_parser.hpp"
#include "data.hpp"
#include <cstring>
#include <ctime>
#include <filesystem>
#include <libbfio.h>
#include <libscca.h>
#include <optional>
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

// Helper to safely get a UTF-8 string from libscca
static std::string get_utf8_filename(libscca_file_t* file, int index, libscca_error_t** error)
{
    uint8_t buffer[4096];
    if (libscca_file_get_utf8_filename(file, index, buffer, sizeof(buffer), error) != 1)
    {
        return "";
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return reinterpret_cast<char*>(buffer);
}

static void handle_libscca_error(libscca_error_t** error)
{
    if (*error)
    {
        libscca_error_free(error);
        // TODO std::cerr <<
        *error = nullptr;
    }
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

    // Volume information (serial number only, creation time not available in original example)
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

    // Run count
    uint32_t run_count = 0;
    if (libscca_file_get_run_count(file, &run_count, &error) != 1)
    {
        libscca_file_free(&file, &error);
        handle_libscca_error(&error);
        return std::nullopt;
    }
    result.runCount = run_count;

    // TODO get run times

    // Number of filenames
    int num_filenames = 0;
    if (libscca_file_get_number_of_filenames(file, &num_filenames, &error) != 1)
    {
        handle_libscca_error(&error);
        return std::nullopt;
    }

    // result.executableName = get_utf8_filename(file, 0, &error); //wrong

    // Files
    for (int i = 0; i < num_filenames; ++i)
    {
        result.fileList.push_back(get_utf8_filename(file, i, &error));
        handle_libscca_error(&error);
    }

    // Clean up
    libscca_file_free(&file, &error);
    handle_libscca_error(&error);

    return result;
}

} // namespace prefetch::parser::libscca
