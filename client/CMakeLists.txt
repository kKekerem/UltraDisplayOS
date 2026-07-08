add_executable(ultradisplay_client
    main.cpp
    audio/alsa_playback.cpp
    compositor/sync_engine.cpp
    compositor/drm_display.c
    decoder/vaapi_decoder.cpp
    input/input_forwarder.cpp
    network/client_transport.cpp
    network/net_optimizer.cpp
    system/hw_optimizer.cpp
    tui/app.cpp
    tui/screens/connection_screen.cpp
    tui/screens/discovery_screen.cpp
    tui/screens/wifi_screen.cpp
)

target_include_directories(ultradisplay_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ultradisplay_client PRIVATE 
    shared
    drm
    va
    va-drm
    uring
    asound
)
