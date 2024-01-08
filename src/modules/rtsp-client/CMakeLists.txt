
set(src
    rtsp_client.cpp
    rtsp_stream.cpp
)

add_library( rtsp_client "${src}")

find_package(CURL REQUIRED)

target_link_libraries(rtsp_client
    nabto_device_static
    rtp_client
    datachannel-static
    CURL::libcurl
    webrtc_util
)
