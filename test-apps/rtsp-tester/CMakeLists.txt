set(src
    main.cpp
)

add_executable(rtsp_client_tester "${src}")

target_link_libraries(rtsp_client_tester
    rtsp_client
)

install(TARGETS rtsp_client_tester
    RUNTIME DESTINATION bin
)
