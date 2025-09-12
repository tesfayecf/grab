# CMake module to search for JsonNlohmann
#
# Once done this will define:
#   JsonNlohmann_INCLUDE_DIR          	- The library's include directories
#   JsonNlohmann_VERSION_MAJOR        	- Major version number
#   JsonNlohmann_VERSION_MINOR        	- Minor version number
#   JsonNlohmann_VERSION_PATCH        	- Patch version number
#   JsonNlohmann_VERSION   			  	- Full version with format <major.minor.patch>

# Locate ONNX include directory
set(JsonNlohmann_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/.deps/json-nlohmann-3.7.3)

mark_as_advanced(JsonNlohmann_INCLUDE_DIR)

string(REGEX MATCH "json-nlohmann-([0-9]+\\.[0-9]+\\.[0-9]+)" VERSION_MATCH "${JsonNlohmann_INCLUDE_DIR}")
if(VERSION_MATCH)
    set(JsonNlohmann_VERSION "${CMAKE_MATCH_1}")
else()
    set(JsonNlohmann_VERSION "unknown")
endif()

# Print message with found version information
message("-- Found ${CMAKE_FIND_PACKAGE_NAME} ${JsonNlohmann_VERSION}: ${JsonNlohmann_INCLUDE_DIR}")