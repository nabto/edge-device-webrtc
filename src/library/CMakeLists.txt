include(llvm_sanitizers.cmake)

add_custom_target(GENERATE_WEBRTC_VERSION ALL
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp
  COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
  )

include_directories(.)

find_package(plog REQUIRED)

set(src
    nabto-listeners/nabto_listeners.cpp
    signaling-stream/signaling_stream.cpp
    signaling-stream/signaling_stream_manager.cpp
    webrtc-connection/webrtc_connection.cpp
    webrtc-connection/webrtc_coap_channel.cpp
    webrtc-connection/webrtc_stream_channel.cpp
    api/nabto_device_webrtc.cpp
    api/nabto_device_webrtc_impl.cpp
    api/media_track_impl.cpp
    api/version.cpp
)

add_library( nabto_device_webrtc SHARED "${src}")
add_library( nabto_device_webrtc_static STATIC "${src}")

target_link_libraries(nabto_device_webrtc
    nabto_device_static
    datachannel-static
    plog::plog
)

target_link_libraries(nabto_device_webrtc_static
    nabto_device_static
    datachannel-static
    plog::plog
)

set_target_properties(nabto_device_webrtc PROPERTIES PUBLIC_HEADER "../../include/nabto/nabto_device_webrtc.hpp")

install(TARGETS nabto_device_webrtc
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/nabto
  )

