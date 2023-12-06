set(src
    main.cpp
)

add_executable(edge_device_webrtc2 "${src}")

find_package(CURL REQUIRED)

target_link_libraries(edge_device_webrtc2
    nabto_device_static
    nm_iam
    nabto_device_webrtc
    datachannel-static
    CURL::libcurl
    jwt-cpp::jwt-cpp
)

install(TARGETS edge_device_webrtc2
    RUNTIME DESTINATION bin
)

install_dependencies(edge_device_webrtc2)
