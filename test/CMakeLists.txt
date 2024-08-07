
include(${CMAKE_CURRENT_SOURCE_DIR}/gstreamer-tests/gstreamer.cmake)

set(test_src
  unit_test.cpp
  signaling-tests/signaling_tests.cpp
  util-tests/util_tests.cpp
  )

if (HAS_GST)
  message("FOUND GST_RTSP_SERVER")
  set(test_src
    ${test_src}
    gstreamer-tests/gstreamer_tests.cpp
  )
endif()

add_executable(webrtc_unit_test "${test_src}")

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

target_link_libraries(webrtc_unit_test
    Boost::boost
    Boost::unit_test_framework
    nabto_device_webrtc
    event_queue_impl
    datachannel-static
    rtsp_client
)

if (HAS_GST)
  message("FOUND GST_RTSP_SERVER")
  target_link_libraries(webrtc_unit_test GST_RTSP_SERVER)
endif()

install(TARGETS webrtc_unit_test
    RUNTIME DESTINATION bin
)
