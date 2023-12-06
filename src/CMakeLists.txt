include(llvm_sanitizers.cmake)

set(src
    util.cpp
    event-queue/event_queue_impl.cpp
    nabto-device/nabto_device.cpp
    nabto-device/iam_util.cpp
    media-streams/rtp_client.cpp
    media-streams/rtp_stream.cpp
    media-streams/rtsp_client.cpp
    media-streams/rtsp_stream.cpp
    signaling-stream/signaling_stream.cpp
    signaling-stream/signaling_stream_manager.cpp
    webrtc-connection/webrtc_connection.cpp
    webrtc-connection/webrtc_coap_channel.cpp
    webrtc-connection/webrtc_stream_channel.cpp
    api/nabto_device_webrtc.cpp
    api/nabto_device_webrtc_impl.cpp
)

add_library( nabto_device_webrtc SHARED "${src}")

target_link_libraries(nabto_device_webrtc
    nabto_device_static
    datachannel-static
    nm_iam
    CURL::libcurl
    jwt-cpp::jwt-cpp
)

set(ex_src
    main.cpp
)

add_executable(edge_device_webrtc "${ex_src}")

find_package(CURL REQUIRED)

target_link_libraries(edge_device_webrtc
    nabto_device_static
    nm_iam
    nabto_device_webrtc
    CURL::libcurl
    jwt-cpp::jwt-cpp
)

install(TARGETS edge_device_webrtc
    RUNTIME DESTINATION bin
)

install_dependencies(edge_device_webrtc)
