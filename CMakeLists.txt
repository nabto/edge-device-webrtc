cmake_minimum_required(VERSION 3.7)

CMAKE_POLICY(SET CMP0135 NEW)

option(USE_SYSTEM_MBEDTLS "use system mbedtls" OFF)
option(USE_SYSTEM_LIBDATACHANNEL "Use libdatachannel from the system" OFF)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")


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

# include(FetchContent)
# FetchContent_Declare(jwt-cpp GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
#                          GIT_TAG v0.7.0-rc.0)
# FetchContent_MakeAvailable(jwt-cpp)


set(DEVICE_BUILD_EXAMPLES OFF)
set(DEVICE_BUILD_TESTS OFF)
set(DEVICE_BUILD_APPS OFF)

add_subdirectory(nabto-embedded-sdk)

set(NO_TESTS ON CACHE BOOL "disable tests")
set(NO_EXAMPLES ON CACHE BOOL "disable examples")
set(USE_SYSTEM_JSON ON CACHE BOOL "system json")


if(NOT USE_SYSTEM_LIBDATACHANNEL)
  add_subdirectory(3rdparty/libdatachannel)
endif()

include_directories(include)
include_directories(src/modules)
include_directories(nabto-embedded-sdk/include)
include_directories(nabto-embedded-sdk/src)
include_directories(3rdparty/libdatachannel/deps/json/include)
include_directories(3rdparty/)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/optional/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/string-view-lite/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/variant-lite/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/byte-lite/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/shared_mutex/include)

add_subdirectory(src/library)
add_subdirectory(src/modules/util)
add_subdirectory(src/modules/event-queue)
add_subdirectory(src/modules/codecs)
add_subdirectory(src/modules/rtp-client)
#add_subdirectory(src/modules/rtsp-client)
#add_subdirectory(examples/webrtc-demo)
add_subdirectory(examples/simple-webrtc)

#add_subdirectory(test-apps/simple-webrtc-two-streams)
#add_subdirectory(test-apps/simple-webrtc-send-audio-from-browser)
#add_subdirectory(test-apps/simple-webrtc-race-condition)
if(NOT NO_TESTS)
  add_subdirectory(test)
endif()
