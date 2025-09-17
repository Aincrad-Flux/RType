cmake_minimum_required(VERSION 3.20)
project(RType CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Conan generated files (CMakeDeps/CMakeToolchain)
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

add_subdirectory(src/common)
add_subdirectory(src/engine)
add_subdirectory(src/server)
add_subdirectory(src/client)
