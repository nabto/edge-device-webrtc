
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

target_link_libraries(rtsp_client PUBLIC
    track_negotiators
    rtp_client
    rtp_repacketizers
    LibDataChannel::LibDataChannelStatic
    CURL::libcurl
    webrtc_util
)
if (RTSP_HAS_BASIC_AUTH)
    find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/base.h")
    target_include_directories(rtsp_client PRIVATE ${JWT_CPP_INCLUDE_DIRS})
endif()


target_include_directories(rtsp_client
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(rtsp_client PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        rtcp_client.hpp
        rtsp_client.hpp
        rtsp_stream.hpp
        tcp_rtp_client.hpp
)
