vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO fltk/fltk
    REF release-1.4.4
    SHA512 51ca9fd8d5903bbfb4d18145dd85a89f2e4281baf224d34d0a8b67591ff8b00b151cf1b353fd3871a1456d443848a7f69df8ecc6c959fbf6135d019d97099c28
    HEAD_REF master
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        opengl FLTK_BUILD_GL
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DFLTK_BUILD_EXAMPLES=OFF
        -DFLTK_BUILD_TEST=OFF
        -DFLTK_BUILD_FLUID=OFF
        -DFLTK_BUILD_FLTK_OPTIONS=OFF
        -DFLTK_BUILD_SHARED_LIBS=OFF
        -DFLTK_MSVC_RUNTIME_DLL=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH CMake)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
