
set(TARGET_NAME grab)

# Gather source files (using CONFIGURE_DEPENDS for automatic reconfiguration)
file(GLOB PROJECT_SOURCES 
    "main.cpp"
    "ticker.cpp"
    "connection.cpp"
    "./graph/graph.cpp"
)

# Gather header files (using CONFIGURE_DEPENDS for automatic reconfiguration)
file(GLOB PROJECT_HEADERS "*.hpp")

# Create the executable target using all the source files (including main.cpp)
add_executable(${TARGET_NAME} ${PROJECT_SOURCES} ${VERSION_FILES_OUTPUT_VAR})

# Specify include directories.
target_include_directories(${TARGET_NAME} PRIVATE ${PROJECT_HEADERS})

# Import LibTorch (private dependency)
target_include_directories(${TARGET_NAME} PRIVATE ${Boost_INCLUDE_DIR})

# Import OpenSSL (private dependency)
target_include_directories(${TARGET_NAME} PRIVATE ${OpenSSL_INCLUDE_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE ${OpenSSL_LIBS})

# Import JsonNlohmann (private dependency)
target_include_directories(${TARGET_NAME} PRIVATE ${JsonNlohmann_INCLUDE_DIR})

# Set target rpath
set(CMAKE_INSTALL_RPATH "/usr/lib;/usr/local/lib;")
set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_RPATH}")

# Disable warnings
target_compile_options(${TARGET_NAME} PRIVATE -w)

# Install the executable
install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION bin)