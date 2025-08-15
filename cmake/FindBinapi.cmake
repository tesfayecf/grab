# CMake module to search for Binapi
#
# Once done this will define:
#   Binapi_INCLUDE_DIR          	- The library's include directories
#   Binapi_VERSION   			  	- Full version with format <major.minor.patch>

# Locate ONNX include directory
set(Binapi_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/.deps/binapi/include)

mark_as_advanced(Binapi_INCLUDE_DIR)

set(Binapi_VERSION "unknown")

# Print message with found version information
message("-- Found ${CMAKE_FIND_PACKAGE_NAME} ${Binapi_VERSION}: ${Binapi_INCLUDE_DIR}")