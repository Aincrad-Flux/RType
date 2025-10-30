cmake_minimum_required(VERSION 3.20)
project(RType CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build options
option(BUILD_CLIENT "Build the client" ON)
option(BUILD_SERVER "Build the server" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(DEFAULT_BUILD_TYPE "Release")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "No build type specified. Defaulting to ${DEFAULT_BUILD_TYPE}.")
endif()

set(RTYPE_RUNTIME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}")
function(rtype_set_runtime_output target)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${RTYPE_RUNTIME_OUTPUT_DIR}"
        )
        foreach(_cfg IN ITEMS Release Debug RelWithDebInfo MinSizeRel)
            set_target_properties(${target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${RTYPE_RUNTIME_OUTPUT_DIR}"
            )
        endforeach()
    endif()
endfunction()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Conan Integration
# Check if conan_toolchain.cmake exists, if not, run conan install
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
    message(STATUS "Conan toolchain not found. Running conan install...")

    # Check if conan is available
    find_program(CONAN_CMD conan)
    if(NOT CONAN_CMD)
        message(FATAL_ERROR "Conan not found. Please install it: pip install --user conan")
    endif()

    # Detect conan profile if it doesn't exist
    if(NOT EXISTS "$ENV{HOME}/.conan2/profiles/default")
        message(STATUS "Detecting conan profile...")
        execute_process(
            COMMAND ${CONAN_CMD} profile detect
            RESULT_VARIABLE CONAN_PROFILE_RESULT
        )
    endif()

    # Determine build type to use for Conan (default to Release when unspecified)
    if(CMAKE_CONFIGURATION_TYPES)
        set(CONAN_BUILD_TYPE "Release")
    else()
        set(CONAN_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    endif()

    if(NOT CONAN_BUILD_TYPE)
        set(CONAN_BUILD_TYPE "Release")
    endif()

    # Run conan install (do NOT auto-install system packages here)
    execute_process(
        COMMAND ${CONAN_CMD} install ${CMAKE_SOURCE_DIR}
            -of=${CMAKE_BINARY_DIR}
            --build=missing
            -s build_type=${CONAN_BUILD_TYPE}
        RESULT_VARIABLE CONAN_RESULT
        OUTPUT_VARIABLE CONAN_OUTPUT
        ERROR_VARIABLE CONAN_ERROR
        TIMEOUT 600
    )

    if(NOT CONAN_RESULT EQUAL 0)
        message(WARNING "Conan install failed. Output:")
        message(WARNING "${CONAN_OUTPUT}")
        message(WARNING "${CONAN_ERROR}")
        message(WARNING "If system packages are required, run the install manually (may require sudo):")
    message(WARNING "  sudo ${CONAN_CMD} install ${CMAKE_SOURCE_DIR} -of=${CMAKE_BINARY_DIR} --build=missing -s build_type=${CONAN_BUILD_TYPE}")
        message(FATAL_ERROR "Conan dependency installation failed")
    endif()

    message(STATUS "Conan dependencies installed successfully")
endif()

# Locate and load the Conan toolchain file. Conan's cmake_layout can nest it
# under build configuration specific directories, so search common locations.
set(CONAN_TOOLCHAIN_CANDIDATES
    "${CMAKE_BINARY_DIR}/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE}/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/build/Release/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/build/Debug/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/build/RelWithDebInfo/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/build/MinSizeRel/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/Release/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/Debug/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/RelWithDebInfo/generators/conan_toolchain.cmake"
    "${CMAKE_BINARY_DIR}/MinSizeRel/generators/conan_toolchain.cmake"
)

set(CONAN_TOOLCHAIN_FILE "")
foreach(_candidate IN LISTS CONAN_TOOLCHAIN_CANDIDATES)
    if(EXISTS "${_candidate}")
        set(CONAN_TOOLCHAIN_FILE "${_candidate}")
        break()
    endif()
endforeach()

if(NOT CONAN_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Conan toolchain file not found under ${CMAKE_BINARY_DIR}. "
        "Try running 'conan install ${CMAKE_SOURCE_DIR} -of=${CMAKE_BINARY_DIR} --build=missing'.")
endif()

message(STATUS "Using Conan toolchain: ${CONAN_TOOLCHAIN_FILE}")
include(${CONAN_TOOLCHAIN_FILE})

# Add CMAKE_PREFIX_PATH for finding packages
list(APPEND CMAKE_PREFIX_PATH
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/build/Release/generators"
    "${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE}/generators"
)

# Add subdirectories
add_subdirectory(common)
add_subdirectory(engine)

if(BUILD_SERVER)
    add_subdirectory(server)
    rtype_set_runtime_output(r-type_server)
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
    rtype_set_runtime_output(r-type_client)
endif()

# Print build configuration
message(STATUS "=================================")
message(STATUS "R-Type Build Configuration")
message(STATUS "=================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build client: ${BUILD_CLIENT}")
message(STATUS "Build server: ${BUILD_SERVER}")
message(STATUS "=================================")