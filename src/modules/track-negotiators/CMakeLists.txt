
set(src
    h264.cpp
    h265.cpp
    pcmu.cpp
    opus.cpp
    vp8.cpp
)

add_library( track_negotiators "${src}")

target_link_libraries(track_negotiators
    nabto_device_webrtc_static
)
