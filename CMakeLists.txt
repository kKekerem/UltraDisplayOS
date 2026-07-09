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

if(WIN32)
    # NVIDIA Codec Headers
    FetchContent_Declare(
        nvenc
        GIT_REPOSITORY https://github.com/FFmpeg/nv-codec-headers.git
        GIT_TAG        n12.1.14.0
    )

    # AMD AMF SDK
    FetchContent_Declare(
        amf
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/AMF.git
        GIT_TAG        v1.4.33
    )

    # Intel oneVPL
    FetchContent_Declare(
        vpl
        GIT_REPOSITORY https://github.com/oneapi-src/oneVPL.git
        GIT_TAG        v2023.4.0
    )
endif()

# FTXUI (for Linux Client TUI)
if(UNIX AND NOT APPLE)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
        GIT_TAG        v5.0.0
    )
endif()

FetchContent_MakeAvailable(googletest spdlog opus)
if(WIN32)
    FetchContent_GetProperties(nvenc)
    if(NOT nvenc_POPULATED)
        FetchContent_Populate(nvenc)
    endif()
    
    FetchContent_GetProperties(amf)
    if(NOT amf_POPULATED)
        FetchContent_Populate(amf)
    endif()
    
    set(BUILD_TOOLS OFF CACHE BOOL "Disable oneVPL tools" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Disable oneVPL examples" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build oneVPL as static library" FORCE)
    FetchContent_MakeAvailable(vpl)
endif()
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
