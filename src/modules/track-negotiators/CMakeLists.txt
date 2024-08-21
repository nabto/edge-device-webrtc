
set(src
    h264.cpp
    pcmu.cpp
    opus.cpp
    vp8.cpp
)

add_library( track_negotiators "${src}")

target_link_libraries(track_negotiators
    nabto_device_webrtc

)

target_include_directories(track_negotiators
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(track_negotiators PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        h264.hpp
        opus.hpp
        pcmu.hpp
        track_negotiator.hpp
        vp8.hpp
)
