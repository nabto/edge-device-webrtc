
set(src
    h264_packetizer.cpp
)

add_library( rtp_packetizers "${src}")

target_link_libraries(rtp_packetizers
    datachannel-static
    nabto_device_webrtc

)
