

set(src
    event_queue_impl.cpp
)

add_library(event_queue_impl "${src}")

find_package(plog)

target_link_libraries(event_queue_impl
    nabto_device_webrtc
    plog::plog
)

target_include_directories(event_queue_impl
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(event_queue_impl PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        event_queue_impl.hpp
)
