set(REF "a72ae1b05cdf71e96f2b4039eac10107d9772e9d")

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nabto/nabto-common
    REF ${REF}
    SHA512 3c819e1791bca152e270d0f11e3a5c9cb7917ad7ae7906c323e61574dc322eda58f407d232b9bb6123e3e51634ad0fbf70f3c2258543a2b240d2b1a8c8d5bf93
    HEAD_REF "master"
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
     -DNABTO_COMMON_VERSION=0.0.0+${REF}
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup(PACKAGE_NAME NabtoCommon CONFIG_PATH lib/cmake/NabtoCommon)

#vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSING.md")
