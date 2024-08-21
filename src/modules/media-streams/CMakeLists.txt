add_library(media_streams INTERFACE)

target_include_directories(media_streams
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(media_streams INTERFACE
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        media_stream.hpp
)
