cmake_minimum_required(VERSION 3.25)

project(step_brother
  VERSION 0.0.1
  DESCRIPTION "step Brother - schneller STEP-Viewer fuer grosse Anlagen"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Alle Programme und DLLs landen in einem Ordner - so ist das Ergebnis direkt portabel.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra)
endif()

# Open CASCADE wird dynamisch gelinkt (LGPL 2.1).
find_package(OpenCASCADE CONFIG REQUIRED)
message(STATUS "Open CASCADE ${OpenCASCADE_VERSION}")
if(OpenCASCADE_VERSION VERSION_LESS 8.0)
  message(FATAL_ERROR "step Brother braucht Open CASCADE 8.0 oder neuer")
endif()

add_subdirectory(src/common)
add_subdirectory(src/sb-bench)
add_subdirectory(src/sb-gen-step)
