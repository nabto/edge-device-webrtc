set(test_src
  unit_test.cpp
  signaling-tests/signaling_tests.cpp
  )


add_executable(webrtc_unit_test "${test_src}")

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

target_link_libraries(webrtc_unit_test
    Boost::boost
    Boost::unit_test_framework
    nabto_device_static
    nabto_device_webrtc_static
    event_queue_impl
    datachannel-static
)


