

set(src
    event_queue_impl.cpp
)

add_library(event_queue_impl "${src}")


target_link_libraries(event_queue_impl
    nabto_device_static
)
