set(REF "b720dc43170d5f20582c3e18dec0fff286e76a7d")

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nabto/nabto-common
    REF ${REF}
    SHA512 3c3fafbedd9e66a33a24b5121bfa5ac7a26836453ebb1af0f06418be83b1dc323284c70f23b26438a580ac0f8ccf8790f1077c19c47ee7836dd05a5927f8b79f
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
