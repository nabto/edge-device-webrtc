
set(src
    fifo_file_client.cpp
)

add_library( fifo_file_client "${src}")

target_link_libraries(fifo_file_client
    track_negotiators
    nabto_device_webrtc
)


target_include_directories(fifo_file_client
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(fifo_file_client PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        fifo_file_client.hpp
)
