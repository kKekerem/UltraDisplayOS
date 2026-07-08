add_executable(ultradisplay_server
    main.cpp
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
)

target_include_directories(ultradisplay_server PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(ultradisplay_server PRIVATE 
    shared
    d3d11
    dxgi
    ws2_32
)
