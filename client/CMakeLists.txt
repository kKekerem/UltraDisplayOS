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
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(EVDEV REQUIRED libevdev)
pkg_check_modules(VA REQUIRED libva)
pkg_check_modules(VADRM REQUIRED libva-drm)
pkg_check_modules(URING REQUIRED liburing)
pkg_check_modules(ALSA REQUIRED alsa)

target_include_directories(ultradisplay_client PRIVATE 
    ${CMAKE_SOURCE_DIR} 
    ${DRM_INCLUDE_DIRS} 
    ${EVDEV_INCLUDE_DIRS}
    ${VA_INCLUDE_DIRS}
    ${VADRM_INCLUDE_DIRS}
    ${URING_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(ultradisplay_client PRIVATE 
    shared
    ${DRM_LIBRARIES}
    ${EVDEV_LIBRARIES}
    ${VA_LIBRARIES}
    ${VADRM_LIBRARIES}
    ${URING_LIBRARIES}
    ${ALSA_LIBRARIES}
    ftxui::screen
    ftxui::dom
    ftxui::component
)
