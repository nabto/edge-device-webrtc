
set(src
    fifo_file_client.cpp
)

add_library( fifo_file_client "${src}")

target_link_libraries(fifo_file_client
    track_negotiators
    nabto_device_webrtc_static
)
