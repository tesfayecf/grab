# CMake module to search for OpenSSL
#
# Once done this will define:
#   OpenSSL_INCLUDE_DIR          	- The library's include directories
#   OpenSSL_VERSION_MAJOR        	- Major version number
#   OpenSSL_VERSION_MINOR        	- Minor version number
#   OpenSSL_VERSION_PATCH        	- Patch version number
#   OpenSSL_VERSION   			  	- Full version with format <major.minor.patch>

# Locate ONNX include directory
set(OpenSSL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/.deps/openssl-3.5.2/include)

mark_as_advanced(OpenSSL_INCLUDE_DIR)

# Collect all libraries in the specified paths
file(GLOB_RECURSE OpenSSL_LIBS
    PATHS ${PROJECT_SOURCE_DIR}/.deps/openssl-3.5.2/lib64/*.so.*
)

string(REGEX MATCH "openssl-([0-9]+\\.[0-9]+\\.[0-9]+)" VERSION_MATCH "${OpenSSL_INCLUDE_DIR}")
if(VERSION_MATCH)
    set(OpenSSL_VERSION "${CMAKE_MATCH_1}")
else()
    set(OpenSSL_VERSION "unknown")
endif()

# Print message with found version information
message("-- Found ${CMAKE_FIND_PACKAGE_NAME} ${OpenSSL_VERSION}: ${OpenSSL_INCLUDE_DIR}")