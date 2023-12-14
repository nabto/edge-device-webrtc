
set(src
    rtp_client.cpp
    # rtp_stream.cpp
    # rtsp_client.cpp
    # rtsp_stream.cpp
)

add_library( media_streams SHARED "${src}")

find_package(CURL REQUIRED)

# TODO: remove datachannel dependency
target_link_libraries(media_streams
    nabto_device_static
    datachannel-static
    CURL::libcurl
    webrtc_util
)
