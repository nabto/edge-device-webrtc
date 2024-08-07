cmake_minimum_required(VERSION 3.20)

include(CheckCXXSymbolExists)

option(NABTO_WEBRTC_USE_VCPKG "Use vcpkg for providing dependencies" ON)
option(NABTO_WEBRTC_BUILD_TESTS "Build code meant for testing" ON)

if (NABTO_WEBRTC_USE_VCPKG)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(EdgeDeviceWebrtc
    VERSION 0.0.1
    LANGUAGES CXX)

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if (NABTO_WEBRTC_USE_VCPKG)
  set(NABTO_WEBRTC_DEFAULT_PROVIDER "package")
endif()

set(NABTO_WEBRTC_NABTOEMBEDDEDSDK_PROVIDER "module" CACHE STRING "Provider of NabtoEmbeddedSDK")
set_property(CACHE NABTO_WEBRTC_NABTOEMBEDDEDSDK_PROVIDER PROPERTY STRINGS "module" "package")

set(NABTO_WEBRTC_NLOHMANNJSON_PROVIDER ${NABTO_WEBRTC_DEFAULT_PROVIDER} CACHE STRING "Provider of nlohmann_json")
set_property(CACHE NABTO_WEBRTC_NLOHMANNJSON_PROVIDER PROPERTY STRINGS "module" "package")

set(NABTO_WEBRTC_JWTCPP_PROVIDER ${NABTO_WEBRTC_DEFAULT_PROVIDER} CACHE STRING "Provider of jwt-cpp")
set_property(CACHE NABTO_WEBRTC_JWTCPP_PROVIDER PROPERTY STRINGS "module" "package")

set(NABTO_WEBRTC_LIBDATACHANNEL_PROVIDER ${NABTO_WEBRTC_DEFAULT_PROVIDER} CACHE STRING "Provider of libdatachannel")
set_property(CACHE NABTO_WEBRTC_LIBDATACHANNEL_PROVIDER PROPERTY STRINGS "module" "package")


if (NABTO_WEBRTC_NABTOEMBEDDEDSDK_PROVIDER STREQUAL "module")
  set(DEVICE_BUILD_EXAMPLES OFF CACHE BOOL "build nabto embedded sdk examples")
  set(DEVICE_BUILD_TESTS OFF CACHE BOOL "build nabto embedded sdk tests")
  set(DEVICE_BUILD_APPS OFF CACHE BOOL "build nabto embedded sdk apps")

  FetchContent_Declare(
    NabtoEmbeddedSDK
    GIT_REPOSITORY https://github.com/nabto/nabto-embedded-sdk.git
    GIT_TAG 98b360fca7b7d01512cf147e35a61790c439256b
    OVERRIDE_FIND_PACKAGE
  )
  FetchContent_MakeAvailable(NabtoEmbeddedSDK)
endif()

if (NABTO_WEBRTC_NLOHMANNJSON_PROVIDER STREQUAL "module")
  FetchContent_Declare(nlohmann_json
      URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
      URL_MD5 127794b2c82c0c5693805feaa2a703e2
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()



set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "build jwt cpp examples")

if (NABTO_WEBRTC_JWTCPP_PROVIDER STREQUAL "module")
  FetchContent_Declare(jwt-cpp GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
                         GIT_TAG v0.7.0-rc.0)
  FetchContent_MakeAvailable(jwt-cpp)
else()
  find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/base.h")
  add_library(jwt_cpp_header_only INTERFACE)
  add_library(jwt-cpp::jwt-cpp ALIAS jwt_cpp_header_only)
  target_link_libraries(jwt_cpp_header_only INTERFACE ${JWT_CPP_INCLUDE_DIRS})
endif()

if (NABTO_WEBRTC_LIBDATACHANNEL_PROVIDER STREQUAL "module")
  FetchContent_Declare(
    LibDataChannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.21.2
    OVERRIDE_FIND_PACKAGE)
  FetchContent_MakeAvailable(LibDataChannel)
endif()

set(DEVICE_BUILD_EXAMPLES OFF CACHE BOOL "build nabto embedded sdk examples")
set(DEVICE_BUILD_TESTS OFF CACHE BOOL "build nabto embedded sdk tests")
set(DEVICE_BUILD_APPS OFF CACHE BOOL "build nabto embedded sdk apps")

# Some toolchains does not come with std::round e.g. gcc 7.2 with uclibc for the T40 platform.
check_cxx_symbol_exists(std::round "cmath" HAVE_CXX_STD_ROUND)
if (NOT HAVE_CXX_STD_ROUND)
  add_definitions(-DNABTO_WEBRTC_CXX_STD_ROUND_FIX)
endif()

set(NO_TESTS ON CACHE BOOL "disable tests")
set(NO_EXAMPLES ON CACHE BOOL "disable examples")
set(USE_SYSTEM_JSON ON CACHE BOOL "system json")
set(USE_SYSTEM_PLOG ON CACHE BOOL "system plog")
set(NO_WEBSOCKET ON CACHE BOOL "Disable WebSocket support")
#add_subdirectory(3rdparty/libdatachannel)

include_directories(include)
include_directories(src/modules)
#include_directories(3rdparty/libdatachannel/deps/json/include)

#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/optional/include)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/string-view-lite/include)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/variant-lite/include)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/byte-lite/include)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/shared_mutex/include)

add_subdirectory(src/library)
add_subdirectory(src/modules/util)
add_subdirectory(src/modules/event-queue)
add_subdirectory(src/modules/track-negotiators)
add_subdirectory(src/modules/rtp-client)
add_subdirectory(src/modules/rtsp-client)
add_subdirectory(src/modules/fifo-file-client)
add_subdirectory(src/modules/media-streams)
add_subdirectory(src/modules/rtp-packetizer)
add_subdirectory(src/modules/rtp-repacketizer)
add_subdirectory(examples/webrtc-demo)
add_subdirectory(examples/simple-webrtc)

if (NABTO_WEBRTC_BUILD_TESTS)
  add_subdirectory(test-apps/simple-webrtc-two-streams)
  add_subdirectory(test-apps/simple-webrtc-send-audio-from-browser)
  add_subdirectory(test-apps/simple-webrtc-race-condition)
  add_subdirectory(test-apps/rtsp-tester)
  add_subdirectory(test)
endif()





include(GNUInstallDirs)
set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

include(CMakePackageConfigHelpers)

add_library(EdgeDeviceWebRTC::nabto_device_webrtc ALIAS nabto_device_webrtc)
add_library(EdgeDeviceWebRTC::event_queue_impl ALIAS event_queue_impl)
add_library(EdgeDeviceWebRTC::webrtc_util ALIAS webrtc_util)
add_library(EdgeDeviceWebRTC::media_streams ALIAS media_streams)
add_library(EdgeDeviceWebRTC::track_negotiators ALIAS track_negotiators)
add_library(EdgeDeviceWebRTC::rtp_packetizers ALIAS rtp_packetizers)
add_library(EdgeDeviceWebRTC::rtp_repacketizers ALIAS rtp_repacketizers)
add_library(EdgeDeviceWebRTC::rtp_client ALIAS rtp_client)
add_library(EdgeDeviceWebRTC::rtsp_client ALIAS rtsp_client)
add_library(EdgeDeviceWebRTC::jwt_cpp_header_only ALIAS jwt_cpp_header_only)
add_library(EdgeDeviceWebRTC::fifo_file_client ALIAS fifo_file_client)

install(
    TARGETS nabto_device_webrtc event_queue_impl webrtc_util media_streams track_negotiators rtp_packetizers rtp_repacketizers rtp_client rtsp_client jwt_cpp_header_only fifo_file_client
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILE_SET public_headers
)

write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "cmake-scripts/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/Foo/FooConfig.cmake
#   * <prefix>/lib/cmake/Foo/FooConfigVersion.cmake
install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/Foo/FooTargets.cmake
install(
    EXPORT "${TARGETS_EXPORT_NAME}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)
