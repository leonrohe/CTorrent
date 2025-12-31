cmake_minimum_required(VERSION 3.16)

project(ctorrent_cmake C)

# Use a modern C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# Build sources
file(GLOB SOURCES src/*.c)
add_executable(ctorrent ${SOURCES})

# Make headers in include/ available to the target
target_include_directories(ctorrent PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Compiler Warnings
target_compile_options(ctorrent PRIVATE -Wall -Wextra -pedantic)