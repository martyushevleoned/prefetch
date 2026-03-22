include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(prefetch)
target_sources(prefetch PRIVATE main.cpp)

add_subdirectory(consumer)
add_subdirectory(parser)
