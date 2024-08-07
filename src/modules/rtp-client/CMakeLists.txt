
set(src
    rtp_client.cpp
)

add_library( rtp_client "${src}")

target_link_libraries(rtp_client
    track_negotiators
    rtp_repacketizers
    nabto_device_webrtc
)


target_include_directories(rtp_client
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(rtp_client PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        rtp_client.hpp
        rtp_track.hpp
)
