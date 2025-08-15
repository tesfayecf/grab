
# CMake version
cmake_minimum_required(VERSION 3.31 FATAL_ERROR)

# Define the project name and version
project(
    Grab # Project Name
    VERSION 0.1.0 # Project Version
    LANGUAGES CXX # Language (C++)
)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Compiler options
# add_compile_options(-Wall -Wextra -pedantic)

# Load user environment variables
set(ENV{BASH_ENV} "$HOME/.bashrc")

# Compiler options based on build type
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    add_compile_options(-Wall -Wextra -pedantic)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -march=native -mtune=native")
    # Enable Link Time Optimization for better performance
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Specify runtime dependencies will be searched for on the same folder, only has effect on Linux
# set(CMAKE_INSTALL_RPATH "$ORIGIN;/usr/local/bin;/usr/local;${PROJECT_SOURCE_DIR}/.deps/onnxruntime-1.20.1/lib")
# set(CMAKE_INSTALL_RPATH "$ORIGIN;${CMAKE_INSTALL_RPATH};${PROJECT_SOURCE_DIR}/.deps/opencv-4.10.0/lib")
# set(CMAKE_INSTALL_RPATH "$ORIGIN;${CMAKE_INSTALL_RPATH};${PROJECT_SOURCE_DIR}/.deps/boost-1.71.1/libs")

# Add the auxiliary CMake modules to the build
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Find external dependencies
find_package(Boost 1.71.0 REQUIRED)
find_package(OpenSSL REQUIRED) # Automically adds OpenSSL_INCLUDE_DIR

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  C++ Compiler:      ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "  C++ Flags:         ${CMAKE_CXX_FLAGS}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  Source Directory:  ${PROJECT_SOURCE_DIR}")
message(STATUS "  Build Directory:   ${CMAKE_BINARY_DIR}")
message(STATUS "  Install Prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Boost Version:     ${Boost_VERSION}")
message(STATUS "  OpenSSL Version:   ${OpenSSL_VERSION}")

# Add the source code
add_subdirectory(src)