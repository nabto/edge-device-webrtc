include(llvm_sanitizers.cmake)

include_directories(.)

set(src
    nabto-listeners/nabto_listeners.cpp
    signaling-stream/signaling_stream.cpp
    signaling-stream/signaling_stream_manager.cpp
    webrtc-connection/webrtc_connection.cpp
    webrtc-connection/webrtc_coap_channel.cpp
    webrtc-connection/webrtc_stream_channel.cpp
    api/nabto_device_webrtc.cpp
    api/nabto_device_webrtc_impl.cpp
    api/media_track_impl.cpp
)

add_library( nabto_device_webrtc SHARED "${src}")

target_link_libraries(nabto_device_webrtc
    nabto_device_static
    datachannel-static
)

