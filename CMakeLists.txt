cmake_minimum_required(VERSION 3.20)

include(CheckCXXSymbolExists)

option(NABTO_WEBRTC_USE_VCPKG "Use vcpkg for providing dependencies" ON)
option(NABTO_WEBRTC_BUILD_TESTS "Build code meant for testing" ON)

if (NABTO_WEBRTC_USE_VCPKG)
  set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_SOURCE_DIR}/build-scripts/vcpkg/ports")
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(EdgeDeviceWebrtc
    VERSION 0.0.1
    LANGUAGES CXX)

option(NABTO_WEBRTC_PREFER_SYSTEM_LIB "Prefer system libraries over fetchcontent libraries" ON)
option(NABTO_WEBRTC_USE_SYSTEM_PLOG "Use the system provided plog library" ${NABTO_WEBRTC_PREFER_SYSTEM_LIB})
option(NABTO_WEBRTC_USE_SYSTEM_LIBDATACHANNEL "Use the system provided libdatachannel library" ${NABTO_WEBRTC_PREFER_SYSTEM_LIB})
option(NABTO_WEBRTC_USE_SYSTEM_JWTCPP "Use the system provided jwt-cpp library" ${NABTO_WEBRTC_PREFER_SYSTEM_LIB})
option(NABTO_WEBRTC_USE_SYSTEM_NABTOEMBEDDEDSDK "Use the system provided NabtoEmbeddedSDK library" ${NABTO_WEBRTC_PREFER_SYSTEM_LIB})
option(NABTO_WEBRTC_USE_SYSTEM_NLOHMANNJSON "Use the system provided nlohmann_json library" ${NABTO_WEBRTC_PREFER_SYSTEM_LIB})
option(NABTO_WEBRTC_BUILD_EXAMPLES "Build nabto webrtc examples" ON)
option(NABTO_WEBRTC_BUILD_TESTS "Build nabto webrtc tests" OFF)
option(NABTO_WEBRTC_BUILD_WITH_VCPKG_DEPENDENCIES "build with vcpkg dependencies, which lacks mbedtls and tinycbor package configuration files" ${NABTO_WEBRTC_USE_VCPKG})
option(NABTO_WEBRTC_BUILD_TESTS "Build tests" OFF)
option(NABTO_WEBRTC_BUILD_EXAMPLES "Build tests" ON)

if (NABTO_WEBRTC_BUILD_WITH_VCPKG_DEPENDENCIES)
  # vcpkg dependencies for mbedtls 2 and tinycbor does not provide find_package scripts for these libraries
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vcpkg;${CMAKE_MODULE_PATH}")
endif()

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT NABTO_WEBRTC_USE_SYSTEM_NABTOEMBEDDEDSDK)
  set(DEVICE_BUILD_EXAMPLES OFF CACHE BOOL "build nabto embedded sdk examples")
  set(DEVICE_BUILD_TESTS OFF CACHE BOOL "build nabto embedded sdk tests")
  set(DEVICE_BUILD_APPS OFF CACHE BOOL "build nabto embedded sdk apps")

  FetchContent_Declare(
    NabtoEmbeddedSDK
    GIT_REPOSITORY https://github.com/nabto/nabto-embedded-sdk.git
    GIT_TAG e160f00e9ba69f24805d8c1c0cbf47acde545ed8
    OVERRIDE_FIND_PACKAGE
  )
  FetchContent_MakeAvailable(NabtoEmbeddedSDK)
endif()

if (NOT NABTO_WEBRTC_USE_SYSTEM_NLOHMANNJSON)
  FetchContent_Declare(nlohmann_json
      URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
      URL_MD5 127794b2c82c0c5693805feaa2a703e2
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()



set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "build jwt cpp examples")

if (NOT NABTO_WEBRTC_USE_SYSTEM_JWTCPP)
  FetchContent_Declare(jwt-cpp GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
                         GIT_TAG v0.7.0-rc.0)
  FetchContent_MakeAvailable(jwt-cpp)
else()
  find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/base.h")
  add_library(jwt_cpp_header_only INTERFACE)
  add_library(jwt-cpp::jwt-cpp ALIAS jwt_cpp_header_only)
  target_link_libraries(jwt_cpp_header_only INTERFACE ${JWT_CPP_INCLUDE_DIRS})
endif()

if (NOT NABTO_WEBRTC_USE_SYSTEM_LIBDATACHANNEL)
  set(NO_TESTS ON CACHE BOOL "disable tests")
  set(NO_EXAMPLES ON CACHE BOOL "disable examples")

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

include_directories(include)
include_directories(src/modules)

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

if (NABTO_WEBRTC_BUILD_EXAMPLES)
  add_subdirectory(examples/webrtc-demo)
  add_subdirectory(examples/simple-webrtc)
endif()

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
