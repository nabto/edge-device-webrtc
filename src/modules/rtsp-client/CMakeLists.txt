
set(src
    rtsp_client.cpp
    rtsp_stream.cpp
    tcp_rtp_client.cpp
)

option(RTSP_HAS_BASIC_AUTH "disable RTSP client basic auth" ON)
option(RTSP_HAS_DIGEST_AUTH "disable RTSP client digest auth" ON)

if (RTSP_HAS_BASIC_AUTH)
    add_definitions(-DNABTO_RTSP_HAS_BASIC_AUTH)
endif()
if (RTSP_HAS_DIGEST_AUTH)
    add_definitions(-DNABTO_RTSP_HAS_DIGEST_AUTH)
endif()

add_library( rtsp_client "${src}")

find_package(CURL REQUIRED)
find_package(LibDataChannel REQUIRED)

target_link_libraries(rtsp_client
    track_negotiators
    rtp_client
    rtp_repacketizers
    LibDataChannel::LibDataChannelStatic
    CURL::libcurl
    webrtc_util
)
if (RTSP_HAS_BASIC_AUTH)
    target_link_libraries(rtsp_client
    jwt-cpp::jwt-cpp
    )
endif()
