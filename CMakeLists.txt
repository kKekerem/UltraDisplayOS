cmake_minimum_required(VERSION 3.25)
project(UltraDisplayOS VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add standard warnings but do NOT treat them as errors globally, 
# as this breaks third-party dependencies like spdlog.
add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-pedantic>
)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Fetch dependencies
include(FetchContent)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)

# Opus
FetchContent_Declare(
    opus
    GIT_REPOSITORY https://github.com/xiph/opus.git
    GIT_TAG        v1.5.2
)

# FTXUI (for Linux Client TUI)
if(UNIX AND NOT APPLE)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
        GIT_TAG        v5.0.0
    )
endif()

FetchContent_MakeAvailable(googletest spdlog opus)
if(UNIX AND NOT APPLE)
    FetchContent_MakeAvailable(ftxui)
endif()

# Subdirectories
add_subdirectory(shared)

if(WIN32)
    add_subdirectory(server)
elseif(UNIX AND NOT APPLE)
    add_subdirectory(client)
endif()
