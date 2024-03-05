
set(src
    h264.cpp
    pcmu.cpp
    opus.cpp
    vp8.cpp
)

add_library( rtp_codecs "${src}")

target_link_libraries(rtp_codecs
    nabto_device_webrtc_static
)
