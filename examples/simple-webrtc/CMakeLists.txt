set(src
    main.cpp
    coap_listener.cpp
)

add_executable(simple_webrtc_device "${src}")

find_package(CURL REQUIRED)
find_package(NabtoEmbeddedSDK)
find_package(cxxopts)

target_link_libraries(simple_webrtc_device
    NabtoEmbeddedSDK::nabto_device
    nabto_device_webrtc
    event_queue_impl
    rtp_client
    CURL::libcurl
    webrtc_util
    cxxopts::cxxopts
)

install(TARGETS simple_webrtc_device
    RUNTIME DESTINATION bin
)
