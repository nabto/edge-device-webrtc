
set(src
    rtp_repacketizer.cpp
)

add_library( rtp_repacketizers "${src}")

find_package(LibDataChannel REQUIRED)

target_link_libraries(rtp_repacketizers
    LibDataChannel::LibDataChannelStatic
    nabto_device_webrtc
)

target_include_directories(rtp_repacketizers
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(rtp_repacketizers PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        h264_repacketizer.hpp
        rtp_repacketizer.hpp
)
