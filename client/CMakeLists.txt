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

find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(EVDEV REQUIRED IMPORTED_TARGET libevdev)
pkg_check_modules(VA REQUIRED IMPORTED_TARGET libva)
pkg_check_modules(VADRM REQUIRED IMPORTED_TARGET libva-drm)
pkg_check_modules(URING REQUIRED IMPORTED_TARGET liburing)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

target_include_directories(ultradisplay_client PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ultradisplay_client PRIVATE 
    shared
    PkgConfig::DRM
    PkgConfig::EVDEV
    PkgConfig::VA
    PkgConfig::VADRM
    PkgConfig::URING
    PkgConfig::ALSA
    ftxui::screen
    ftxui::dom
    ftxui::component
)
