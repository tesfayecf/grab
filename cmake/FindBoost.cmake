# CMake module to search for Boost
#
# Once done this will define:
#   Boost_INCLUDE_DIR          	- The library's include directories
#   Boost_VERSION_MAJOR        	- Major version number
#   Boost_VERSION_MINOR        	- Minor version number
#   Boost_VERSION_PATCH        	- Patch version number
#   Boost_VERSION   			  	- Full version with format <major.minor.patch>

# Locate ONNX include directory
set(Boost_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/.deps/boost-1.71.0)

mark_as_advanced(Boost_INCLUDE_DIR)

string(REGEX MATCH "boost-([0-9]+\\.[0-9]+\\.[0-9]+)" VERSION_MATCH "${Boost_INCLUDE_DIR}")
if(VERSION_MATCH)
    set(Boost_VERSION "${CMAKE_MATCH_1}")
else()
    set(Boost_VERSION "unknown")
endif()

# Print message with found version information
message("-- Found ${CMAKE_FIND_PACKAGE_NAME} ${Boost_VERSION}: ${Boost_INCLUDE_DIR}")