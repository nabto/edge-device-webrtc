include(CheckIncludeFileCXX)

CHECK_INCLUDE_FILE_CXX("filesystem" HAVE_FILESYSTEM_H)

if(HAVE_FILESYSTEM_H)
  add_definitions(-DHAVE_FILESYSTEM_H)
else()
  message(
    INFO
    "Missing std::filesystem header, the demo will NOT create directories by itself"
  )
endif()

set(src main.cpp nabto_device.cpp iam_util.cpp)

add_executable(edge_device_webrtc "${src}")

find_package(CURL REQUIRED)

target_link_libraries(
  edge_device_webrtc
  nabto_device_static
  nm_iam
  nabto_device_webrtc_static
  datachannel-static
  CURL::libcurl
  jwt-cpp::jwt-cpp
  webrtc_util
  event_queue_impl
  rtp_client
  rtsp_client
  fifo_file_client
  rtp_packetizers)

install(TARGETS edge_device_webrtc RUNTIME DESTINATION bin)

if(CMAKE_BUILD_TYPE MATCHES Release)
  add_custom_command(
    TARGET edge_device_webrtc
    POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:edge_device_webrtc>
    COMMENT "Stripping release binary")
endif()
