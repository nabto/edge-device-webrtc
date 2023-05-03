cmake_minimum_required(VERSION 3.7)

project(edge-device-webrtc
    VERSION 0.0.1
    LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DEVICE_BUILD_EXAMPLES OFF)
set(DEVICE_BUILD_TESTS OFF)
set(DEVICE_BUILD_APPS OFF)

set(USE_MBEDTLS ON CACHE BOOL "use mbedtls in libdatachannel")
set(USE_WEBSOCKET OFF CACHE BOOL "use websocket in libdatachannel")
set(ENABLE_MBEDTLS ON CACHE BOOL "use mbedtls in libdatachannel")

add_subdirectory(3rdparty/mbedtls)

add_subdirectory(nabto-embedded-sdk)

add_subdirectory(3rdparty/libdatachannel)

include_directories(src)
include_directories(nabto-embedded-sdk/include)
include_directories(3rdparty/libdatachannel/deps/json/include)

add_subdirectory(src)
add_subdirectory(examples/webrtc-device)
