cmake_minimum_required(VERSION 3.20)
project(FolkValleyRadar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/cs2_process_source.cpp
    src/normalizer.cpp
    src/ipc_client.cpp
    src/source2_overhead.cpp
    src/embedded_runtime.cpp
    src/app.rc
)

# Header files (for IDE organization)
set(HEADERS
    include/data_source.h
    include/snapshot.h
    include/normalizer.h
    include/ipc_client.h
    compatibility/OFFSETS.h
    compatibility/PATTERNS.h
)

# Create executable
add_executable(folk_valley_radar ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(folk_valley_radar PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/compatibility
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(folk_valley_radar PRIVATE 
        winhttp
        psapi
        iphlpapi
        ws2_32
        windowscodecs
        ole32
    )
endif()

# Compiler options
if(MSVC)
    target_compile_definitions(folk_valley_radar PRIVATE
        $<$<CONFIG:Release>:FOLK_VALLEY_RELEASE>
    )
    target_link_options(folk_valley_radar PRIVATE
        /MANIFEST:NO
    )
    target_compile_options(folk_valley_radar PRIVATE 
        /W4 
        /permissive-
        /MP  # Multi-processor compilation
    )
    # Set runtime library to static for easier deployment
    set_property(TARGET folk_valley_radar PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(folk_valley_radar PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
    )
endif()

# Set output directory
set_target_properties(folk_valley_radar PROPERTIES
    OUTPUT_NAME "Folk Valley Radar"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

# Print configuration info
message(STATUS "Folk Valley Radar")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}")
