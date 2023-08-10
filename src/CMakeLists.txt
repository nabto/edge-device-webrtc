set(src
    main.cpp
    nabto-device/nabto_device.cpp
    media-streams/rtp_client.cpp
    media-streams/rtsp_client.cpp
    signaling-stream/signaling_stream.cpp
    signaling-stream/signaling_stream_manager.cpp
    webrtc-connection/webrtc_connection.cpp
    webrtc-connection/webrtc_coap_channel.cpp
)

add_executable(edge_device_webrtc "${src}")

target_link_libraries(edge_device_webrtc
    nabto_device
    LibDataChannel::LibDataChannel
    curl
)

install(TARGETS edge_device_webrtc
    RUNTIME DESTINATION bin
)
