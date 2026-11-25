# Toolchain fuer vcpkg-Ports: clang-cl statt cl, sonst identisch mit vcpkgs Windows-Toolchain.
# Voraussetzung: Visual Studio / Build Tools mit der Komponente "C++-Clang-Tools fuer Windows"
# oder ein installiertes LLVM.
find_program(SB_CLANG_CL clang-cl
  HINTS "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin" "$ENV{ProgramFiles}/LLVM/bin"
  REQUIRED)
set(CMAKE_C_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${SB_CLANG_CL}" CACHE FILEPATH "")

if(NOT DEFINED ENV{VCPKG_ROOT})
  message(FATAL_ERROR "VCPKG_ROOT ist nicht gesetzt")
endif()
include("$ENV{VCPKG_ROOT}/scripts/toolchains/windows.cmake")
