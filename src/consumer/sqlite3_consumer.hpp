#pragma once

#include "consumer.hpp"
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace prefetch::consumer
{

class Sqlite3PfConsumer : public PfConsumer
{
public:
    explicit Sqlite3PfConsumer(const std::string& filename);
    ~Sqlite3PfConsumer() override;
    void consume(const Record& record) override;
private:
    void check(int rc, const std::string& msg) const;
private:
    sqlite3* db_;
    sqlite3_stmt* insert_stmt_;
    std::string filename_;
};

} // namespace prefetch::consumer
