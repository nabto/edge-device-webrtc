
set(src
    h264_packetizer.cpp
    pcmu_packetizer.cpp
)

add_library( rtp_packetizers "${src}")

find_package(LibDataChannel REQUIRED)

target_link_libraries(rtp_packetizers
    LibDataChannel::${NABTO_WEBRTC_LIBDATACHANNEL_LIBRARY_NAME}
    nabto_device_webrtc

)

target_include_directories(rtp_packetizers
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(rtp_packetizers PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        h264_packetizer.hpp
        rtp_packetizer.hpp
)
