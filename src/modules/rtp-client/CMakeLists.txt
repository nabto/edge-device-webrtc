
set(src
    rtp_client.cpp
)

add_library( rtp_client "${src}")

target_link_libraries(rtp_client
    track_negotiators
    nabto_device_webrtc_static
)
