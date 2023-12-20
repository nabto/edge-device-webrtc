set(src
    main.cpp
    nabto_device.cpp
    iam_util.cpp
)

add_executable(edge_device_webrtc "${src}")

find_package(CURL REQUIRED)

target_link_libraries(edge_device_webrtc
    nabto_device_static
    nm_iam
    nabto_device_webrtc
    datachannel-static
    CURL::libcurl
    jwt-cpp::jwt-cpp
    webrtc_util
    event_queue_impl
    rtp_client
)

install(TARGETS edge_device_webrtc
    RUNTIME DESTINATION bin
)

install_dependencies(edge_device_webrtc)
