# Toolchain fuer vcpkg-Ports: clang-cl statt cl, sonst identisch mit vcpkgs Windows-Toolchain.
# Voraussetzung: Visual Studio / Build Tools mit der Komponente "C++-Clang-Tools fuer Windows"
# oder ein installiertes LLVM.
find_program(SB_CLANG_CL clang-cl
  HINTS "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin" "$ENV{ProgramFiles}/LLVM/bin"
  REQUIRED)
set(CMAKE_C_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")
# Ressourcen (.rc) mit dem rc.exe des Windows SDK uebersetzen - sonst nimmt CMake clang-cl.
find_program(SB_RC rc HINTS "$ENV{WindowsSdkVerBinPath}/x64" REQUIRED)
set(CMAKE_RC_COMPILER "${SB_RC}" CACHE FILEPATH "")

# vcpkg uebergibt sein Wurzelverzeichnis je nach Funktion als _VCPKG_ROOT_DIR
# (vcpkg_cmake_configure) oder Z_VCPKG_ROOT_DIR; die Umgebungsvariable ist Rueckfall.
if(DEFINED _VCPKG_ROOT_DIR)
  set(SB_VCPKG_ROOT "${_VCPKG_ROOT_DIR}")
elseif(DEFINED Z_VCPKG_ROOT_DIR)
  set(SB_VCPKG_ROOT "${Z_VCPKG_ROOT_DIR}")
elseif(DEFINED ENV{VCPKG_ROOT})
  set(SB_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
else()
  message(FATAL_ERROR "vcpkg-Wurzelverzeichnis unbekannt (weder Z_VCPKG_ROOT_DIR noch VCPKG_ROOT)")
endif()
# Auch in try_compile-Projekten verfuegbar machen
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES _VCPKG_ROOT_DIR Z_VCPKG_ROOT_DIR SB_VCPKG_ROOT)
include("${SB_VCPKG_ROOT}/scripts/toolchains/windows.cmake")
