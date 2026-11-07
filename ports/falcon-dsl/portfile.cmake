vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO falcon-autotuning/falcon-dsl
    REF v${VERSION}
    SHA512 08ab6477bf817f1c6f890286ea99c2166e222c80f30b44f85096ec5dc04c9b6af9c6606e334fcb86b8b695c71a6c972aac8ab6c2298d2ee417017198c804d9af
)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS -DBUILD_TESTS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup()
file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
     RENAME copyright)
vcpkg_copy_pdbs()
