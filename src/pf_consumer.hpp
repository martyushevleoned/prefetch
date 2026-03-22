#pragma once

#include "pf_data.hpp"  // Ваш заголовок с определением PfData
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

// ============================================================================
// Abstract consumer interface
// ============================================================================

class PfConsumer
{
public:
    virtual ~PfConsumer() = default;
    virtual void consume(const PfData&) = 0;
};

// ============================================================================
// CSV output consumer
// ============================================================================

class PfCsvConsumer : public PfConsumer
{
public:
    explicit PfCsvConsumer(const std::string& filename);
    ~PfCsvConsumer() override = default;
    
    void consume(const PfData&) override;
    
    // Check if consumer is ready to write
    bool isOpen() const { return fileStream.is_open(); }

private:
    std::ofstream fileStream;
    bool headersWritten = false;
    
    // Write CSV header row
    void writeHeader();
    
    // Escape a field for CSV: quote if contains comma/quote/newline, double internal quotes
    static std::string escapeCsvField(const std::string& field);
    
    // Join vector of strings with separator, with proper CSV escaping
    static std::string joinEscaped(const std::vector<std::string>& items, const std::string& separator = "; ");
};

// ============================================================================
// Console output consumer
// ============================================================================

class PfConsoleConsumer : public PfConsumer
{
public:
    PfConsoleConsumer();
    ~PfConsoleConsumer() override = default;
    
    void consume(const PfData&) override;

private:
    std::ostream& out;
    int entryCounter = 0;
    
    // Helper to print a labeled field only if value is non-empty
    void printField(const std::string& label, const std::string& value);
    
    // Helper to print a list with label
    void printList(const std::string& label, const std::vector<std::string>& items);
};

// ============================================================================
// Factory function
// ============================================================================

/**
 * @brief Create appropriate consumer based on output filename
 * @param fileName 
 *   - empty string → console output
 *   - ends with ".csv" (case-insensitive) → CSV file output
 *   - other extensions → returns nullptr
 * @return unique_ptr to consumer, or nullptr if extension not supported
 */
std::unique_ptr<PfConsumer> make_consumer(const std::string& fileName);
