# Toolchain fuer das eigene Projekt (Preset windows-clang-cl): dasselbe clang-cl aus
# Visual Studio wie in cmake/toolchains/clang-cl.cmake (vcpkg-Ports), aber ohne vcpkgs
# Port-Einstellungen. Wird von vcpkg.cmake per VCPKG_CHAINLOAD_TOOLCHAIN_FILE geladen.
# Getrennte Datei, weil jede Aenderung an der Port-Toolchain einen OCCT-Neubau ausloest.
find_program(SB_CLANG_CL clang-cl
  HINTS "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin" "$ENV{ProgramFiles}/LLVM/bin"
  REQUIRED)
set(CMAKE_C_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")
