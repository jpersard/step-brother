# Toolchain fuer vcpkg-Ports: clang-cl statt cl, sonst identisch mit vcpkgs Windows-Toolchain.
# Voraussetzung: Visual Studio / Build Tools mit der Komponente "C++-Clang-Tools fuer Windows"
# oder ein installiertes LLVM.
find_program(SB_CLANG_CL clang-cl
  HINTS "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin" "$ENV{ProgramFiles}/LLVM/bin"
  REQUIRED)
set(CMAKE_C_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")

# vcpkg uebergibt sein Wurzelverzeichnis als Z_VCPKG_ROOT_DIR; die Umgebungsvariable ist Rueckfall.
if(DEFINED Z_VCPKG_ROOT_DIR)
  set(SB_VCPKG_ROOT "${Z_VCPKG_ROOT_DIR}")
elseif(DEFINED ENV{VCPKG_ROOT})
  set(SB_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
else()
  message(FATAL_ERROR "vcpkg-Wurzelverzeichnis unbekannt (weder Z_VCPKG_ROOT_DIR noch VCPKG_ROOT)")
endif()
# Auch in try_compile-Projekten verfuegbar machen
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES Z_VCPKG_ROOT_DIR SB_VCPKG_ROOT)
include("${SB_VCPKG_ROOT}/scripts/toolchains/windows.cmake")
