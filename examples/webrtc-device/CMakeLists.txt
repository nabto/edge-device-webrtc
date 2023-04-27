set(src
  main.cpp
  )

add_executable(webrtc_device "${src}")
target_link_libraries(webrtc_device nabto_webrtc)

install(TARGETS webrtc_device
  RUNTIME DESTINATION bin
  )
