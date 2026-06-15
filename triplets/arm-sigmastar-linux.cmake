set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Skip the debug variant of every port — halves disk + time and is what we
# actually link against in MinSizeRel builds anyway.
set(VCPKG_BUILD_TYPE release)

set(VCPKG_C_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard -ffunction-sections -fdata-sections")
set(VCPKG_CXX_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard -ffunction-sections -fdata-sections")

# Optimize ports for size, not speed.
set(VCPKG_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(VCPKG_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(VCPKG_LINKER_FLAGS_RELEASE "-Wl,--gc-sections -Wl,-s")

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/arm-sigmastar-linux.toolchain.cmake")
