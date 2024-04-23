cmake_minimum_required(VERSION 3.7)

option(NABTO_WEBRTC_USE_VCPKG "Use vcpkg for providing dependencies" ON)
option(NABTO_WEBRTC_BUILD_TESTS "Build code meant for testing" ON)

if (NABTO_WEBRTC_USE_VCPKG)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(edge-device-webrtc
    VERSION 0.0.1
    LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    URL_MD5 127794b2c82c0c5693805feaa2a703e2
)
FetchContent_MakeAvailable(nlohmann_json)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "build jwt cpp examples")

include(FetchContent)
FetchContent_Declare(jwt-cpp GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
                         GIT_TAG v0.7.0-rc.0)
FetchContent_MakeAvailable(jwt-cpp)

set(DEVICE_BUILD_EXAMPLES OFF CACHE BOOL "build nabto embedded sdk examples")
set(DEVICE_BUILD_TESTS OFF CACHE BOOL "build nabto embedded sdk tests")
set(DEVICE_BUILD_APPS OFF CACHE BOOL "build nabto embedded sdk apps")

add_subdirectory(nabto-embedded-sdk)

set(NO_TESTS ON CACHE BOOL "disable tests")
set(NO_EXAMPLES ON CACHE BOOL "disable examples")
set(USE_SYSTEM_JSON ON CACHE BOOL "system json")
set(USE_SYSTEM_PLOG ON CACHE BOOL "system plog")
set(NO_WEBSOCKET ON CACHE BOOL "Disable WebSocket support")
add_subdirectory(3rdparty/libdatachannel)

include_directories(include)
include_directories(src/modules)
include_directories(nabto-embedded-sdk/include)
include_directories(nabto-embedded-sdk/src)
include_directories(3rdparty/libdatachannel/deps/json/include)
include_directories(3rdparty/)

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
add_subdirectory(examples/webrtc-demo)
add_subdirectory(examples/simple-webrtc)

if (NABTO_WEBRTC_BUILD_TESTS)
  add_subdirectory(test-apps/simple-webrtc-two-streams)
  add_subdirectory(test-apps/simple-webrtc-send-audio-from-browser)
  add_subdirectory(test-apps/simple-webrtc-race-condition)
  add_subdirectory(test-apps/rtsp-tester)
  add_subdirectory(test)
endif()
