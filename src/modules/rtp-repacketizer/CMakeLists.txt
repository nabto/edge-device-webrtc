
set(src
    rtp_repacketizer.cpp
)

add_library( rtp_repacketizers "${src}")

target_link_libraries(rtp_repacketizers
    datachannel-static
    nabto_device_webrtc

)
