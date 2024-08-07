

set(src
    util.cpp
)

add_library(webrtc_util "${src}")

find_package(CURL REQUIRED)
find_package(NabtoEmbeddedSDK::nabto_device)

target_link_libraries(webrtc_util
    NabtoEmbeddedSDK::nabto_device
    CURL::libcurl
)
