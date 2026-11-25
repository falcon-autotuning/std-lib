vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO falcon-autotuning/falcon-typing
    REF v${VERSION}
    SHA512 fb3cf4dcb2ea59d4fdb6d6d9a496ea5ad946f1298645d93953e6b5f2fd8e60e53beec59f74f6830d052a6e3b5e5008d90e07adfef2617b5fac5f7095d175773c
)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup()
file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
     RENAME copyright)
vcpkg_copy_pdbs()
