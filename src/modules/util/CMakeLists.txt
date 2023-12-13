

set(src
    util.cpp
)

add_library(webrtc_util SHARED "${src}")

target_link_libraries(webrtc_util
    nabto_device_static
)
