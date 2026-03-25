#include "libscca_parser.hpp"
#include "data.hpp"
#include <libscca.h>
#include <libbfio.h>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <cstdint>
#include <cstring>

namespace prefetch {
namespace parser {
namespace libscca {

static std::string filetime_to_string(uint64_t filetime) {
    const uint64_t epoch_diff = 116444736000000000ULL;
    if (filetime < epoch_diff) return "";
    uint64_t seconds = (filetime - epoch_diff) / 10000000;
    std::time_t t = static_cast<std::time_t>(seconds);
    std::tm* tm = std::gmtime(&t);
    if (!tm) return "";
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buf);
}

std::optional<Record> parseFile(const std::string& filePath) {
    libscca_file_t* file = nullptr;
    libscca_error_t* error = nullptr;

    if (libscca_file_open(file, filePath.c_str(), LIBBFIO_OPEN_READ, &error) != 1) {
        if (error) libscca_error_free(&error);
        return std::nullopt;
    }

    Record record;
    record.pfFileName = std::filesystem::path(filePath).filename().string();

    // --- Имя исполняемого файла (первая запись в списке имён) ---
    int num_filenames = 0;
    if (libscca_file_get_number_of_filenames(file, &num_filenames, &error) != 1) {
        libscca_file_free(&file, &error);
        return std::nullopt;
    }

    if (num_filenames > 0) {
        uint8_t buffer[4096];
        if (libscca_file_get_utf8_filename(file, 0, buffer, sizeof(buffer), &error) == 1) {
            // гарантируем нуль-терминатор
            buffer[sizeof(buffer)-1] = '\0';
            record.executableName = reinterpret_cast<char*>(buffer);
        } else {
            record.executableName = "";
        }
    } else {
        record.executableName = "";
    }

    // --- Размер исполняемого файла (недоступен) ---
    record.fileSize = 0;

    // --- Информация о томе ---
    libscca_volume_information_t* volume_info = nullptr;
    if (libscca_file_get_volume_information(file, 0, &volume_info, &error) != 1) {
        libscca_file_free(&file, &error);
        return std::nullopt;
    }

    // Если есть функции-геттеры, можно их использовать, иначе поля остаются нулевыми
    uint32_t serial = 0;
    uint64_t vol_time = 0;
    // serial = libscca_volume_information_get_serial_number(volume_info);
    // vol_time = libscca_volume_information_get_creation_time(volume_info);

    record.volumeSerial = serial;
    record.volumeCreateTime = filetime_to_string(vol_time);

    // Освобождаем volume_info, если есть соответствующая функция
    // libscca_volume_information_free(&volume_info, &error);

    // --- Количество запусков ---
    uint32_t run_count = 0;
    if (libscca_file_get_run_count(file, &run_count, &error) != 1) {
        libscca_file_free(&file, &error);
        return std::nullopt;
    }
    record.runCount = run_count;

    // --- Времена запусков (если функция недоступна, оставляем пустым) ---
    // (закомментировано, так как libscca_file_get_run_time не найдена)
    /*
    for (uint32_t i = 0; i < run_count; ++i) {
        uint64_t run_time = 0;
        if (libscca_file_get_run_time(file, i, &run_time, &error) != 1) {
            libscca_file_free(&file, &error);
            return std::nullopt;
        }
        record.runTimes.push_back(filetime_to_string(run_time));
    }
    */

    // --- Список файлов и директорий ---
    if (libscca_file_get_number_of_filenames(file, &num_filenames, &error) != 1) {
        libscca_file_free(&file, &error);
        return std::nullopt;
    }

    for (int i = 0; i < num_filenames; ++i) {
        uint8_t buffer[4096];
        if (libscca_file_get_utf8_filename(file, i, buffer, sizeof(buffer), &error) != 1) {
            continue;
        }
        buffer[sizeof(buffer)-1] = '\0';
        std::string path_str = reinterpret_cast<char*>(buffer);

        if (!path_str.empty() && (path_str.back() == '\\' || path_str.back() == '/')) {
            record.dirList.push_back(path_str);
        } else {
            record.fileList.push_back(path_str);
        }
    }

    libscca_file_free(&file, &error);
    return record;
}

} // namespace libscca
} // namespace parser
} // namespace prefetch
