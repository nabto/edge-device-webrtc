set(src
    main.cpp
    coap_listener.cpp
)

add_executable(simple_webrtc_device_audio "${src}")

target_link_libraries(simple_webrtc_device_audio
    nabto_device_static
    nabto_device_webrtc_static
    datachannel-static
    event_queue_impl
    rtp_client
)

install(TARGETS simple_webrtc_device_audio
    RUNTIME DESTINATION bin
)
