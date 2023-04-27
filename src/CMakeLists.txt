set(src
    webrtc.cpp
)

add_library(nabto_webrtc STATIC "${src}")

target_link_libraries(nabto_webrtc nabto_device LibDataChannel::LibDataChannel)
