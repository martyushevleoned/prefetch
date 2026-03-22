option(SQLITE3_CONSUMER "enable sqlite3 consumer" OFF) # apt install -y libsqlite3-dev

target_sources(prefetch PRIVATE consumer.cpp console_consumer.cpp)

if(SQLITE3_CONSUMER)
    find_package(SQLite3 REQUIRED)
    target_link_libraries(prefetch PRIVATE SQLite::SQLite3)
    target_compile_definitions(prefetch PRIVATE SQLITE3_CONSUMER)
    target_sources(prefetch PRIVATE sqlite3_consumer.cpp)
endif()

