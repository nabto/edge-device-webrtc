
set(src
    rtp_repacketizer.cpp
)

add_library( rtp_repacketizers "${src}")

find_package(LibDataChannel REQUIRED)

target_link_libraries(rtp_repacketizers
    LibDataChannel::LibDataChannelStatic
    nabto_device_webrtc
)
