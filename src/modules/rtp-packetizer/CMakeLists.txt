
set(src
    h264_packetizer.cpp
)

add_library( rtp_packetizers "${src}")

find_package(LibDataChannel REQUIRED)

target_link_libraries(rtp_packetizers
    LibDataChannel::LibDataChannelStatic
    nabto_device_webrtc

)
