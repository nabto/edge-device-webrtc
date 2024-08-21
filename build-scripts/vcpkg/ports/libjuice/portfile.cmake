vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO paullouisageneau/libjuice
    REF "1a6c2396aab2639636fbf12255a018396d6f6359"
    SHA512 430b5682a3a9ec4d1c7f91af4553897fb1570a342320fe67d8a5277cb15a54654a493c7967e58618b4283dc652617963d4e3e6a9223dfff34d14975b57c92137
    HEAD_REF master
    PATCHES
        fix-for-vcpkg.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        nettle USE_NETTLE
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DNO_TESTS=ON
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_cmake_config_fixup(PACKAGE_NAME libjuice CONFIG_PATH lib/cmake/LibJuice)
vcpkg_fixup_pkgconfig()

file(READ "${CURRENT_PACKAGES_DIR}/share/libjuice/LibJuiceConfig.cmake" DATACHANNEL_CONFIG)
file(WRITE "${CURRENT_PACKAGES_DIR}/share/libjuice/LibJuiceConfig.cmake" "
include(CMakeFindDependencyMacro)
find_dependency(Threads)
${DATACHANNEL_CONFIG}")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
