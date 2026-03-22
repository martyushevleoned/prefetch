#include "sqlite3_consumer.hpp"
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace prefetch::consumer
{

namespace
{

const auto CREATE_TABLE_SQL = "CREATE TABLE IF NOT EXISTS prefetch_records ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                              "executable_name TEXT, "
                              "pf_file_name TEXT, "
                              "file_size INTEGER, "
                              "volume_serial INTEGER, "
                              "volume_create_time TEXT, "
                              "run_count INTEGER, "
                              "run_times TEXT, "
                              "file_list TEXT, "
                              "dir_list TEXT"
                              ");";

const auto INSERT_SQL = "INSERT INTO prefetch_records ("
                        "executable_name, pf_file_name, file_size, volume_serial, "
                        "volume_create_time, run_count, run_times, file_list, dir_list"
                        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

static std::string join(const std::vector<std::string>& vec, char delimiter = '\n')
{
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (i != 0)
            oss << delimiter;
        oss << vec[i];
    }
    return oss.str();
}

} // namespace

Sqlite3PfConsumer::Sqlite3PfConsumer(const std::string& filename)
    : db_(nullptr), insert_stmt_(nullptr), filename_(filename)
{
    // Открыть или создать базу данных
    int rc = sqlite3_open(filename_.c_str(), &db_);
    check(rc, "Failed to open database");

    // Создать таблицу, если её нет
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, CREATE_TABLE_SQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::string error = "Failed to create table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        sqlite3_close(db_);
        throw std::runtime_error(error);
    }

    // Подготовить INSERT-выражение
    rc = sqlite3_prepare_v2(db_, INSERT_SQL, -1, &insert_stmt_, nullptr);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db_);
        check(rc, "Failed to prepare INSERT statement");
    }
}

Sqlite3PfConsumer::~Sqlite3PfConsumer()
{
    if (insert_stmt_)
        sqlite3_finalize(insert_stmt_);
    if (db_)
        sqlite3_close(db_);
}

void Sqlite3PfConsumer::check(int rc, const std::string& msg) const
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE)
    {
        std::string error = msg + ": " + sqlite3_errmsg(db_);
        throw std::runtime_error(error);
    }
}

void Sqlite3PfConsumer::consume(const Record& record)
{
    if (!db_ || !insert_stmt_)
        throw std::runtime_error("Database not initialized");

    // Сбросить предыдущее состояние подготовленного выражения
    sqlite3_reset(insert_stmt_);
    sqlite3_clear_bindings(insert_stmt_);

    // Привязать параметры (индексация с 1)
    int rc;

    rc = sqlite3_bind_text(insert_stmt_, 1, record.executableName.c_str(), -1, SQLITE_TRANSIENT);
    check(rc, "bind executableName");

    rc = sqlite3_bind_text(insert_stmt_, 2, record.pfFileName.c_str(), -1, SQLITE_TRANSIENT);
    check(rc, "bind pfFileName");

    rc = sqlite3_bind_int(insert_stmt_, 3, static_cast<int>(record.fileSize));
    check(rc, "bind fileSize");

    rc = sqlite3_bind_int(insert_stmt_, 4, static_cast<int>(record.volumeSerial));
    check(rc, "bind volumeSerial");

    rc = sqlite3_bind_text(insert_stmt_, 5, record.volumeCreateTime.c_str(), -1, SQLITE_TRANSIENT);
    check(rc, "bind volumeCreateTime");

    rc = sqlite3_bind_int(insert_stmt_, 6, static_cast<int>(record.runCount));
    check(rc, "bind runCount");

    rc = sqlite3_bind_text(insert_stmt_, 7, join(record.runTimes).c_str(), -1, SQLITE_TRANSIENT);
    check(rc, "bind runTimes");

    rc = sqlite3_bind_text(insert_stmt_, 8, join(record.fileList).c_str(), -1, SQLITE_TRANSIENT);
    check(rc, "bind fileList");

    rc = sqlite3_bind_text(insert_stmt_, 9, join(record.dirList).c_str(), -1, SQLITE_TRANSIENT);
    check(rc, "bind dirList");

    // Выполнить вставку
    rc = sqlite3_step(insert_stmt_);
    if (rc != SQLITE_DONE)
    {
        check(rc, "Failed to execute INSERT");
    }
}

} // namespace prefetch::consumer
