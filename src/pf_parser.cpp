#include "pf_parser.hpp"
#include <fstream>
#include <cstring>
#include <codecvt>
#include <locale>
#include <iomanip>
#include <sstream>
#include <filesystem>

// ============================================================================
// Helper functions
// ============================================================================

// Read little-endian uint32 from buffer
inline uint32_t readUint32LE(const uint8_t* data, size_t offset) {
    if (offset + 4 > 0xFFFFFFFF) return 0; // overflow guard
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

// Read little-endian uint64 from buffer
inline uint64_t readUint64LE(const uint8_t* data, size_t offset) {
    uint64_t low = readUint32LE(data, offset);
    uint64_t high = readUint32LE(data, offset + 4);
    return (high << 32) | low;
}

// Convert Windows FILETIME (100-ns intervals since 1601-01-01) to formatted string
std::string fileTimeToString(uint64_t filetime) {
    if (filetime == 0) return {};

    // Convert FILETIME to Unix timestamp (seconds since 1970-01-01)
    const uint64_t WINDOWS_EPOCH = 116444736000000000ULL;
    if (filetime < WINDOWS_EPOCH) return {};

    time_t unixTime = static_cast<time_t>((filetime - WINDOWS_EPOCH) / 10000000ULL);

    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &unixTime);
#else
    localtime_r(&unixTime, &timeinfo);
#endif

    std::ostringstream oss;
    oss << std::setfill('0')
        << (timeinfo.tm_year + 1900) << "."
        << std::setw(2) << (timeinfo.tm_mon + 1) << "."
        << std::setw(2) << timeinfo.tm_mday << " "
        << std::setw(2) << timeinfo.tm_hour << ":"
        << std::setw(2) << timeinfo.tm_min << ":"
        << std::setw(2) << timeinfo.tm_sec;
    return oss.str();
}

// Convert UTF-16LE string (from prefetch) to UTF-8
std::string utf16leToUtf8(const uint8_t* data, size_t offset, size_t maxBytes) {
    if (offset >= maxBytes) return {};

    // Find null terminator (2 bytes for UTF-16)
    size_t len = 0;
    while (offset + len + 1 < maxBytes && 
           (data[offset + len] != 0 || data[offset + len + 1] != 0)) {
        len += 2;
        if (len > 0x1000) break; // safety limit
    }

    if (len == 0) return {};

    try {
        std::u16string utf16;
        utf16.reserve(len / 2);
        for (size_t i = 0; i < len; i += 2) {
            char16_t ch = static_cast<char16_t>(
                data[offset + i] | (data[offset + i + 1] << 8));
            utf16.push_back(ch);
        }

        // Convert to UTF-8 using standard library
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
        return converter.to_bytes(utf16);
    } catch (...) {
        return {};
    }
}

// Extract filename from full path (handle both \ and /)
std::string extractFilename(const std::string& path) {
    size_t pos1 = path.find_last_of('/');
    size_t pos2 = path.find_last_of('\\');
    size_t pos = std::max(pos1, pos2);
    if (pos != std::string::npos && pos + 1 < path.size()) {
        return path.substr(pos + 1);
    }
    return path;
}

// ============================================================================
// Main parser for SCCA format (Windows XP / Vista / 7)
// ============================================================================

std::optional<PfData> parseSccaFormat(const uint8_t* data, size_t dataSize, 
                                       const std::string& pfFileName) {
    if (dataSize < 0x100) return std::nullopt;

    PfData result;
    result.pfFileName = pfFileName;
    result.fileSize = static_cast<uint32_t>(dataSize);

    // --- Header parsing (SCCA format) ---
    
    // Executable name at offset 0x10, max 60 UTF-16 chars (120 bytes)
    result.executableName = utf16leToUtf8(data, 0x10, dataSize);
    if (!result.executableName.empty()) {
        // Keep only filename, not full path
        result.executableName = extractFilename(result.executableName);
    }

    // Volume information offset and count (at 0x6C and 0x70)
    uint32_t volInfoOffset = readUint32LE(data, 0x6C);
    uint32_t volInfoCount = readUint32LE(data, 0x70);
    
    if (volInfoCount > 0 && volInfoOffset + 8 <= dataSize) {
        // Each volume entry: 8 bytes (4 serial + 4 offset to path string)
        uint32_t volDataOffset = volInfoOffset;
        result.volumeSerial = readUint32LE(data, volDataOffset);
        
        // Volume path string offset (relative to string table)
        uint32_t volPathStrOffset = readUint32LE(data, volDataOffset + 4);
        uint32_t strTableOffset = readUint32LE(data, 0x74);
        
        if (strTableOffset < dataSize && volPathStrOffset < 0x10000) {
            result.volumeCreateTime = fileTimeToString(
                readUint64LE(data, volDataOffset + 8)); // creation time at +8
        }
    }

    // Run count and timestamps (location varies by version)
    // For version 17 (Win8) it's at 0x90, for older at 0x94
    uint32_t version = readUint32LE(data, 0x04);
    size_t runCountOffset = (version >= 17) ? 0x90 : 0x94;
    
    if (runCountOffset + 4 <= dataSize) {
        result.runCount = readUint32LE(data, runCountOffset);
    }

    // Run times: 8 FILETIME values starting after run count
    result.runTimes.resize(8);
    size_t runTimesOffset = runCountOffset + 4;
    for (int i = 0; i < 8 && (runTimesOffset + 8 <= dataSize); ++i, runTimesOffset += 8) {
        uint64_t ft = readUint64LE(data, runTimesOffset);
        result.runTimes[i] = fileTimeToString(ft);
    }

    // --- File and Directory lists ---
    
    // String table location
    uint32_t stringTableOffset = readUint32LE(data, 0x64);
    uint32_t stringTableSize = readUint32LE(data, 0x68);
    
    if (stringTableOffset >= dataSize) {
        stringTableOffset = 0; // fallback
    }
    size_t stringTableEnd = std::min(static_cast<size_t>(stringTableOffset + stringTableSize), dataSize);

    // File metrics array
    uint32_t fileMetricsOffset = readUint32LE(data, 0x54);
    uint32_t fileMetricsCount = readUint32LE(data, 0x58);
    
    // Each file metric entry is 0x30 bytes in v17+, 0x2C in older
    size_t fileMetricSize = (version >= 17) ? 0x30 : 0x2C;
    
    for (uint32_t i = 0; i < fileMetricsCount && fileMetricsOffset + fileMetricSize <= dataSize; ++i) {
        size_t entryOffset = fileMetricsOffset + i * fileMetricSize;
        
        // String offset is at +0x04 in the entry
        uint32_t strOffset = readUint32LE(data, entryOffset + 4);
        
        if (strOffset > 0 && stringTableOffset + strOffset + 2 <= stringTableEnd) {
            std::string fullPath = utf16leToUtf8(data, stringTableOffset + strOffset, stringTableEnd);
            if (!fullPath.empty()) {
                result.fileList.push_back(fullPath);
            }
        }
    }

    // Directory metrics array
    uint32_t dirMetricsOffset = readUint32LE(data, 0x5C);
    uint32_t dirMetricsCount = readUint32LE(data, 0x60);
    
    // Directory metric entry is 0x08 bytes: [4: str_offset][4: ???]
    for (uint32_t i = 0; i < dirMetricsCount && dirMetricsOffset + 8 <= dataSize; ++i) {
        size_t entryOffset = dirMetricsOffset + i * 8;
        uint32_t strOffset = readUint32LE(data, entryOffset);
        
        if (strOffset > 0 && stringTableOffset + strOffset + 2 <= stringTableEnd) {
            std::string dirPath = utf16leToUtf8(data, stringTableOffset + strOffset, stringTableEnd);
            if (!dirPath.empty()) {
                result.dirList.push_back(dirPath);
            }
        }
    }

    return result;
}

// ============================================================================
// Parser for MAM format (Windows 8/10/11) - limited support
// Main body is XPress-compressed, but footer with run metadata is often plain
// ============================================================================

std::optional<PfData> parseMamFormat(const uint8_t* data, size_t dataSize,
                                      const std::string& pfFileName) {
    // MAM format: main body is compressed, but we can try to extract 
    // run count and timestamps from the END of the file (they're often uncompressed)
    
    PfData result;
    result.pfFileName = pfFileName;
    result.fileSize = static_cast<uint32_t>(dataSize);

    // Try to read executable name from early offset (sometimes present uncompressed)
    result.executableName = utf16leToUtf8(data, 0x10, dataSize);
    if (!result.executableName.empty()) {
        result.executableName = extractFilename(result.executableName);
    }

    // In MAM format, run metadata is typically at the END of file
    // Structure: [compressed body][run_count:4][last_run:8][history:7*8][volume_serial:4][volume_time:8]
    
    // Try to read from end: minimum footer size = 4 + 8*8 + 4 + 8 = 84 bytes
    if (dataSize < 84) return std::nullopt;
    
    size_t footerStart = dataSize - 84;
    
    // Read run count (4 bytes LE)
    result.runCount = readUint32LE(data, footerStart);
    
    // Read 8 run times (FILETIME)
    result.runTimes.resize(8);
    size_t offset = footerStart + 4;
    for (int i = 0; i < 8; ++i, offset += 8) {
        result.runTimes[i] = fileTimeToString(readUint64LE(data, offset));
    }
    
    // Volume serial (4 bytes)
    result.volumeSerial = readUint32LE(data, offset);
    offset += 4;
    
    // Volume creation time (FILETIME)
    result.volumeCreateTime = fileTimeToString(readUint64LE(data, offset));

    // Note: fileList and dirList cannot be extracted without XPress decompression
    // If you need full parsing, integrate Windows API DecompressRoutine or 
    // use a library like https://github.com/libyal/libprefetch

    return result;
}

// ============================================================================
// Main entry point
// ============================================================================

std::optional<PfData> parsePfFile(const std::string& filePath) {
    namespace fs = std::filesystem;
    
    // Check file exists
    if (!fs::exists(filePath)) {
        return std::nullopt;
    }
    
    // Read entire file into memory
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0 || fileSize > 100 * 1024 * 1024) { // 100MB safety limit
        return std::nullopt;
    }
    
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return std::nullopt;
    }
    file.close();
    
    if (buffer.size() < 8) {
        return std::nullopt;
    }
    
    // Check signature
    const uint8_t* data = buffer.data();
    std::string pfFileName = fs::path(filePath).filename().string();
    
    // SCCA signature: 0x53 0x43 0x41 0x41 ("SCCA")
    if (data[0] == 0x53 && data[1] == 0x43 && data[2] == 0x41 && data[3] == 0x41) {
        return parseSccaFormat(data, buffer.size(), pfFileName);
    }
    
    // MAM signature: 0x4D 0x41 0x4D 0x00 ("MAM\0")
    if (data[0] == 0x4D && data[1] == 0x41 && data[2] == 0x4D) {
        return parseMamFormat(data, buffer.size(), pfFileName);
    }
    
    // Unknown format
    return std::nullopt;
}
