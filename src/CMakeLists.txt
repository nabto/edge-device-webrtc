set(src
    main.cpp
    nabto-device/nabto_device.cpp
    media-streams/rtp_client.cpp
    media-streams/rtp_stream.cpp
    media-streams/rtsp_client.cpp
    media-streams/rtsp_stream.cpp
    signaling-stream/signaling_stream.cpp
    signaling-stream/signaling_stream_manager.cpp
    webrtc-connection/webrtc_connection.cpp
    webrtc-connection/webrtc_coap_channel.cpp
    webrtc-connection/webrtc_stream_channel.cpp
)

add_executable(edge_device_webrtc "${src}")

find_package(CURL REQUIRED)

target_link_libraries(edge_device_webrtc
    nabto_device_static
    nm_iam
    datachannel-static
    CURL::libcurl
)

install(TARGETS edge_device_webrtc
    RUNTIME DESTINATION bin
)

install_dependencies(edge_device_webrtc)
