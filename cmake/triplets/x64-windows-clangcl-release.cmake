# Wie x64-windows-release, aber alle Abhaengigkeiten (OCCT) werden mit clang-cl gebaut.
# Nur so misst der Compiler-Vergleich in M0 wirklich den Code von OCCT.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/clang-cl.cmake")
set(VCPKG_ENV_PASSTHROUGH VCPKG_ROOT)
