include("${CMAKE_CURRENT_SOURCE_DIR}/../../nabto-embedded-sdk/cmake-scripts/nabto_version.cmake")
cmake_policy(SET CMP0007 NEW)

nabto_version(version_out version_error)

if (NOT version_out)
  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp)

    message(FATAL_ERROR "No file ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp exists and it cannot be auto generated. ${version_error}")
  endif()
else()

  set(VERSION "#include <nabto/nabto_device_webrtc.hpp>\n
#include <string>\n
static const std::string version_str = \"${version_out}\"\n;
const std::string nabto::NabtoDeviceWebrtc::version() { return version_str; }\n")

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp VERSION_)
  else()
    set(VERSION_ "")
  endif()

  if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp "${VERSION}")
  endif()
endif()
