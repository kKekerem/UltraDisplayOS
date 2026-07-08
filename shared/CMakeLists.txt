add_library(shared STATIC
    util/log.cpp
    audio/audio_pipeline.cpp
    transport/congestion_gcc.cpp
    transport/fec.cpp
    transport/multipath.cpp
    transport/pacer.cpp
)

target_include_directories(shared PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(shared PUBLIC spdlog::spdlog opus)

if(UNIX AND NOT APPLE)
    target_link_libraries(shared PUBLIC pthread)
endif()
