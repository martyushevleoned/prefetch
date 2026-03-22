#include "ntdll_parser.hpp"
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

namespace prefetch::parser::ntdll
{

namespace
{
typedef NTSTATUS(NTAPI* RtlGetCompressionWorkSpaceSize_t)(USHORT, PULONG, PULONG);

typedef NTSTATUS(NTAPI* RtlDecompressBufferEx_t)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, PULONG,
                                                 PVOID);

RtlGetCompressionWorkSpaceSize_t getRtlGetCompressionWorkSpaceSize()
{
    static auto func = []()
    {
        auto ntdll = GetModuleHandleW(L"ntdll.dll");
        return ntdll ? reinterpret_cast<RtlGetCompressionWorkSpaceSize_t>(
                           GetProcAddress(ntdll, "RtlGetCompressionWorkSpaceSize"))
                     : nullptr;
    }();
    return func;
}

RtlDecompressBufferEx_t getRtlDecompressBufferEx()
{
    static auto func = []()
    {
        auto ntdll = GetModuleHandleW(L"ntdll.dll");
        return ntdll ? reinterpret_cast<RtlDecompressBufferEx_t>(
                           GetProcAddress(ntdll, "RtlDecompressBufferEx"))
                     : nullptr;
    }();
    return func;
}

uint32_t readUint32(const std::vector<uint8_t>& data, size_t offset)
{
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint64_t readUint64(const std::vector<uint8_t>& data, size_t offset)
{
    return static_cast<uint64_t>(data[offset]) | (static_cast<uint64_t>(data[offset + 1]) << 8) |
           (static_cast<uint64_t>(data[offset + 2]) << 16) |
           (static_cast<uint64_t>(data[offset + 3]) << 24) |
           (static_cast<uint64_t>(data[offset + 4]) << 32) |
           (static_cast<uint64_t>(data[offset + 5]) << 40) |
           (static_cast<uint64_t>(data[offset + 6]) << 48) |
           (static_cast<uint64_t>(data[offset + 7]) << 56);
}

std::string utf16ToUtf8(const std::u16string& u16str)
{
    if (u16str.empty())
        return "";

    int sizeNeeded =
        WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(u16str.c_str()),
                            static_cast<int>(u16str.size()), nullptr, 0, nullptr, nullptr);

    std::string utf8str(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(u16str.c_str()),
                        static_cast<int>(u16str.size()), utf8str.data(), sizeNeeded, nullptr,
                        nullptr);

    return utf8str;
}

std::string readStringUtf16(const std::vector<uint8_t>& data, size_t offset, size_t maxLength)
{
    std::u16string u16str;
    for (size_t i = 0; i < maxLength && offset + i * 2 + 1 < data.size(); ++i)
    {
        uint16_t ch = static_cast<uint16_t>(data[offset + i * 2]) |
                      (static_cast<uint16_t>(data[offset + i * 2 + 1]) << 8);
        if (ch == 0)
            break;
        u16str.push_back(static_cast<char16_t>(ch));
    }
    return utf16ToUtf8(u16str);
}

std::string filetimeToDateTime(uint64_t filetime)
{
    if (filetime == 0)
        return "";

    FILETIME ft{static_cast<DWORD>(filetime & 0xFFFFFFFF), static_cast<DWORD>(filetime >> 32)};
    SYSTEMTIME st;

    if (!FileTimeToSystemTime(&ft, &st))
        return "";

    std::ostringstream oss;
    oss << std::setfill('0') << st.wYear << "." << std::setw(2) << st.wMonth << "." << std::setw(2)
        << st.wDay << " " << std::setw(2) << st.wHour << ":" << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond;

    return oss.str();
}

bool decompressPf(const std::string& filePath, std::vector<uint8_t>& output)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

    if (fileSize < 8)
        return false;

    uint32_t signature = readUint32(fileData, 0);
    uint32_t decompressedSize = readUint32(fileData, 4);

    // Проверяем магическое число (MAM\0)
    if ((signature & 0x00FFFFFF) != 0x004d414d)
    {
        output = std::move(fileData);
        return true;
    }

    auto RtlGetCompressionWorkSpaceSize = getRtlGetCompressionWorkSpaceSize();
    auto RtlDecompressBufferEx = getRtlDecompressBufferEx();

    if (!RtlGetCompressionWorkSpaceSize || !RtlDecompressBufferEx)
        return false;

    ULONG workspaceSize = 0, fragmentWorkspaceSize = 0;
    if (RtlGetCompressionWorkSpaceSize(COMPRESSION_FORMAT_LZNT1, &workspaceSize,
                                       &fragmentWorkspaceSize) != STATUS_SUCCESS)
        return false;

    std::vector<uint8_t> compressed(fileData.begin() + 8, fileData.end());
    std::vector<uint8_t> decompressed(decompressedSize);
    std::vector<uint8_t> workspace(fragmentWorkspaceSize);
    ULONG finalUncompressedSize = 0;

    NTSTATUS status = RtlDecompressBufferEx(
        COMPRESSION_FORMAT_LZNT1, decompressed.data(), decompressedSize, compressed.data(),
        static_cast<ULONG>(compressed.size()), &finalUncompressedSize, workspace.data());

    if (status != STATUS_SUCCESS)
    {
        // Пробуем альтернативный формат
        status = RtlDecompressBufferEx(4, decompressed.data(), decompressedSize, compressed.data(),
                                       static_cast<ULONG>(compressed.size()),
                                       &finalUncompressedSize, workspace.data());

        if (status != STATUS_SUCCESS)
            return false;
    }

    if (finalUncompressedSize == 0)
        return false;

    decompressed.resize(finalUncompressedSize);
    output = std::move(decompressed);
    return true;
}
} // anonymous namespace

std::optional<Record> parseFile(const std::string& filePath)
{
    std::vector<uint8_t> data;

    if (!decompressPf(filePath, data) || data.size() < 256)
        return std::nullopt;

    Record record;

    // Имя файла
    size_t lastSlash = filePath.find_last_of("/\\");
    record.pfFileName =
        (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

    // Основные поля
    record.executableName = readStringUtf16(data, 16, 60);
    record.fileSize = readUint32(data, 12);
    record.runCount = readUint32(data, 200);

    // Времена запусков
    for (int i = 0; i < 8; ++i)
    {
        size_t offset = 128 + i * 8;
        if (offset + 8 <= data.size())
        {
            std::string timeStr = filetimeToDateTime(readUint64(data, offset));
            if (!timeStr.empty())
                record.runTimes.push_back(timeStr);
        }
    }

    // Информация о томе
    uint32_t volumeOffset = readUint32(data, 108);
    if (volumeOffset + 24 <= data.size())
    {
        record.volumeCreateTime = filetimeToDateTime(readUint64(data, volumeOffset + 8));
        record.volumeSerial = readUint32(data, volumeOffset + 16);
    }

    // Список файлов
    uint32_t filenamesOffset = readUint32(data, 100);
    uint32_t filenamesSize = readUint32(data, 104);

    if (filenamesOffset + filenamesSize <= data.size())
    {
        std::u16string allFiles;
        for (size_t i = filenamesOffset; i < filenamesOffset + filenamesSize && i + 1 < data.size();
             i += 2)
        {
            uint16_t ch =
                static_cast<uint16_t>(data[i]) | (static_cast<uint16_t>(data[i + 1]) << 8);
            allFiles.push_back(ch ? static_cast<char16_t>(ch) : u'\0');
        }

        std::string utf8Str = utf16ToUtf8(allFiles);
        std::istringstream iss(utf8Str);
        std::string token;
        std::set<std::string> uniqueDirs;

        while (std::getline(iss, token, '\0'))
        {
            if (token.empty())
                continue;

            record.fileList.push_back(token);

            size_t lastSep = token.find_last_of("/\\");
            if (lastSep != std::string::npos)
            {
                std::string dir = token.substr(0, lastSep);
                if (!dir.empty())
                    uniqueDirs.insert(dir);
            }
        }

        record.dirList.assign(uniqueDirs.begin(), uniqueDirs.end());
    }

    return record;
}

} // namespace prefetch::parser::ntdll
