add_executable(ultradisplay_server WIN32
    main.cpp
    gui/tray_app.cpp
    gui/discovery.cpp
    audio/wasapi_capture.cpp
    capture/dxgi_capture.cpp
    capture/scene_analyzer.cpp
    encoder/adaptive_controller.cpp
    encoder/amf_encoder.cpp
    encoder/d3d11_color_convert.cpp
    encoder/encoder_factory.cpp
    encoder/nvenc_encoder.cpp
    encoder/qsv_encoder.cpp
    input/virtual_input.cpp
    network/server_transport.cpp
    service/win_service.cpp
)

target_include_directories(ultradisplay_server PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${nvenc_SOURCE_DIR}/include/ffnvcodec
    ${amf_SOURCE_DIR}/amf/public/include
    ${vpl_SOURCE_DIR}/api
)

# WIN32_LEAN_AND_MEAN prevents windows.h from including winsock.h (conflicts with winsock2.h)
# NOMINMAX prevents windows.h from defining min/max macros (conflicts with std::min/max)
# UNICODE/_UNICODE ensures all Win32 APIs use wide-char variants consistently
target_compile_definitions(ultradisplay_server PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

target_link_libraries(ultradisplay_server PRIVATE 
    shared
    d3d11
    dxgi
    ws2_32
    comctl32
    iphlpapi
    VPL
)
