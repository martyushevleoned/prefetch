#include "pf_consumer.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

// ============================================================================
// PfCsvConsumer implementation
// ============================================================================

PfCsvConsumer::PfCsvConsumer(const std::string& filename) {
    // Open file in binary mode to avoid newline translation issues on Windows
    fileStream.open(filename, std::ios::out | std::ios::binary);
    if (!fileStream) {
        throw std::runtime_error("Failed to open CSV file: " + filename);
    }
    // Write UTF-8 BOM for Excel compatibility (optional but recommended)
    fileStream << "\xEF\xBB\xBF";
}

void PfCsvConsumer::writeHeader() {
    if (headersWritten) return;
    
    // CSV header with semicolon as list-item separator noted in comments
    fileStream 
        << "executable_name"
        << ";pf_file_name"
        << ";file_size"
        << ";volume_serial"
        << ";volume_create_time"
        << ";run_count"
        << ";run_times"
        << ";file_list"
        << ";dir_list"
        << "\n";
    
    headersWritten = true;
}

std::string PfCsvConsumer::escapeCsvField(const std::string& field) {
    // RFC 4180: if field contains comma, double-quote, or newline, wrap in quotes
    // and double any internal quotes
    bool needsQuotes = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    
    if (!needsQuotes) {
        return field;
    }
    
    std::string result = "\"";
    for (char c : field) {
        if (c == '"') {
            result += "\"\"";  // escape quote by doubling
        } else {
            result += c;
        }
    }
    result += "\"";
    return result;
}

std::string PfCsvConsumer::joinEscaped(const std::vector<std::string>& items, 
                                        const std::string& separator) {
    if (items.empty()) return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << separator;
        oss << escapeCsvField(items[i]);
    }
    return oss.str();
}

void PfCsvConsumer::consume(const PfData& data) {
    if (!fileStream.is_open()) {
        return;  // silently ignore if file failed to open
    }
    
    if (!headersWritten) {
        writeHeader();
    }
    
    // Join run_times, filtering empty entries for cleaner output
    std::vector<std::string> nonEmptyRuns;
    for (const auto& t : data.runTimes) {
        if (!t.empty()) {
            nonEmptyRuns.push_back(t);
        }
    }
    
    fileStream 
        << escapeCsvField(data.executableName) << ";"
        << escapeCsvField(data.pfFileName) << ";"
        << data.fileSize << ";"
        << data.volumeSerial << ";"
        << escapeCsvField(data.volumeCreateTime) << ";"
        << data.runCount << ";"
        << joinEscaped(nonEmptyRuns, " | ") << ";"
        << joinEscaped(data.fileList, " | ") << ";"
        << joinEscaped(data.dirList, " | ")
        << "\n";
    
    // Flush periodically to avoid data loss on crash
    if (fileStream.tellp() % 4096 < 100) {  // approx every 4KB
        fileStream.flush();
    }
}

// ============================================================================
// PfConsoleConsumer implementation
// ============================================================================

PfConsoleConsumer::PfConsoleConsumer() : out(std::cout) {}

void PfConsoleConsumer::printField(const std::string& label, const std::string& value) {
    if (!value.empty()) {
        out << "  " << label << ": " << value << "\n";
    }
}

void PfConsoleConsumer::printList(const std::string& label, 
                                   const std::vector<std::string>& items) {
    if (items.empty()) return;
    
    out << "  " << label << " (" << items.size() << "):\n";
    for (const auto& item : items) {
        out << "    • " << item << "\n";
    }
}

void PfConsoleConsumer::consume(const PfData& data) {
    if (entryCounter > 0) {
        out << "\n" << std::string(60, '-') << "\n\n";
    }
    
    out << "【" << ++entryCounter << "】 " << data.executableName << "\n";
    out << "  Prefetch file: " << data.pfFileName << "\n";
    out << "  File size: " << data.fileSize << " bytes\n";
    
    if (data.volumeSerial != 0) {
        out << "  Volume serial: 0x" << std::hex << data.volumeSerial << std::dec << "\n";
    }
    
    printField("Volume created", data.volumeCreateTime);
    out << "  Run count: " << data.runCount << "\n";
    
    // Print non-empty run times
    bool hasRuns = false;
    for (size_t i = 0; i < data.runTimes.size(); ++i) {
        if (!data.runTimes[i].empty()) {
            if (!hasRuns) {
                out << "  Run times:\n";
                hasRuns = true;
            }
            out << "    [" << i + 1 << "] " << data.runTimes[i] << "\n";
        }
    }
    
    printList("Files accessed", data.fileList);
    printList("Directories accessed", data.dirList);
    
    out.flush();
}

// ============================================================================
// Factory implementation
// ============================================================================

std::unique_ptr<PfConsumer> make_consumer(const std::string& fileName) {
    namespace fs = std::filesystem;
    
    // Empty filename → console output
    if (fileName.empty()) {
        return std::make_unique<PfConsoleConsumer>();
    }
    
    // Check extension (case-insensitive)
    std::string ext = fs::path(fileName).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (ext == ".csv") {
        return std::make_unique<PfCsvConsumer>(fileName);
    }
    
    // Unsupported extension
    std::cerr << "Warning: Unsupported output format '" << ext 
              << "'. Use empty string for console or .csv for file output.\n";
    return nullptr;
}
