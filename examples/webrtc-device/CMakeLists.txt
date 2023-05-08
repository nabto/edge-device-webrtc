set(src
  main.cpp
  )

add_executable(webrtc_device "${src}")
target_link_libraries(webrtc_device nabto_webrtc curl)

install(TARGETS webrtc_device
  RUNTIME DESTINATION bin
  )
