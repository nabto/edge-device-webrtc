

set(src
    util.cpp
)

add_library(webrtc_util "${src}")

find_package(CURL REQUIRED)
find_package(NabtoEmbeddedSDK)

target_link_libraries(webrtc_util
    NabtoEmbeddedSDK::nabto_device
    CURL::libcurl
)


target_include_directories(webrtc_util
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(webrtc_util PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        util.hpp
)
