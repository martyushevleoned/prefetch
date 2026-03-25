cmake_minimum_required(VERSION 3.25)
project(Prefetch)

# Standart
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
set(CMAKE_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# Conan toolchain
if(EXISTS ${CMAKE_BINARY_DIR}/conan_toolchain.cmake)
    include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)
endif()

add_subdirectory(src)
