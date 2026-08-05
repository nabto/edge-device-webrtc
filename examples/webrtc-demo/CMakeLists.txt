
# In gcc 8 std::filesystem::create_directories was an experimental feature so it needed to be linked with stdc++fs
# In older gcc versions filesystem is not available at all.
# See https://gitlab.kitware.com/cmake/cmake/-/issues/17834 for a longer discussion.
try_compile(HAVE_FILESYSTEM_CREATE_DIRECTORIES ${CMAKE_BINARY_DIR}/compile_tests ${CMAKE_CURRENT_LIST_DIR}/filesystem_create_directories_test.cpp)

if(HAVE_FILESYSTEM_CREATE_DIRECTORIES)
  add_definitions(-DHAVE_FILESYSTEM_CREATE_DIRECTORIES)
else()
  message(
    INFO
    "Cannot compile a simple example using std::filesystem::create_directories, the demo will NOT create directories by itself"
  )
endif()

set(src main.cpp nabto_device.cpp iam_util.cpp)

add_executable(edge_device_webrtc "${src}")

find_package(CURL REQUIRED)
find_package(NabtoEmbeddedSDK REQUIRED)
find_package(cxxopts REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(
  edge_device_webrtc PRIVATE
  NabtoEmbeddedSDK::nabto_device
  NabtoEmbeddedSDK::nm_iam
  nabto_device_webrtc
  CURL::libcurl
  webrtc_util
  event_queue_impl
  rtp_client
  rtsp_client
  fifo_file_client
  rtp_packetizers
  cxxopts::cxxopts
  OpenSSL::Crypto
  )

install(TARGETS edge_device_webrtc RUNTIME DESTINATION bin)

if(CMAKE_BUILD_TYPE MATCHES Release)
  add_custom_command(
    TARGET edge_device_webrtc
    POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:edge_device_webrtc>
    COMMENT "Stripping release binary")
endif()
