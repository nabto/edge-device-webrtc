cmake_minimum_required(VERSION 3.7)

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

include(FetchContent)
FetchContent_Declare(jwt-cpp GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
                         GIT_TAG v0.7.0-rc.0)
FetchContent_MakeAvailable(jwt-cpp)


set(DEVICE_BUILD_EXAMPLES OFF)
set(DEVICE_BUILD_TESTS OFF)
set(DEVICE_BUILD_APPS OFF)

add_subdirectory(nabto-embedded-sdk)

set(NO_TESTS ON CACHE BOOL "disable tests")
set(NO_EXAMPLES ON)
set(USE_SYSTEM_JSON ON)
add_subdirectory(3rdparty/libdatachannel)

include_directories(include)
include_directories(src)
include_directories(nabto-embedded-sdk/include)
include_directories(nabto-embedded-sdk/src)
include_directories(3rdparty/libdatachannel/deps/json/include)
include_directories(3rdparty/)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/optional/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/string-view-lite/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/variant-lite/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/byte-lite/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libdatachannel/deps/shared_mutex/include)

macro(install_dependencies TARGET)
    install(FILES ${CMAKE_SOURCE_DIR}/src/nabto-device/nabto.png DESTINATION .)
    install(FILES ${CMAKE_SOURCE_DIR}/src/nabto-device/iam_config.json DESTINATION .)
endmacro()

add_subdirectory(src)
add_subdirectory(example)
