

set(src
    util.cpp
)

add_library(webrtc_util "${src}")

find_package(CURL REQUIRED)

target_link_libraries(webrtc_util
    nabto_device_static
    CURL::libcurl
)
