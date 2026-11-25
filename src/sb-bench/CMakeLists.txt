# sb-bench: M0-Benchmark. Liest eine STEP-Datei und misst Zeit und Speicher je Phase.
add_executable(sb-bench
  main.cpp
  memory.cpp
  memory.h
  json.h)

target_link_libraries(sb-bench PRIVATE
  sb-common
  TKernel TKMath TKBRep TKG3d TKTopAlgo TKMesh
  TKLCAF TKXCAF TKXSBase TKDESTEP)

if(WIN32)
  target_link_libraries(sb-bench PRIVATE psapi)
endif()
if(MINGW)
  # wmain() als Einstiegspunkt (Unicode-Dateinamen)
  target_link_options(sb-bench PRIVATE -municode)
endif()

target_compile_definitions(sb-bench PRIVATE
  SB_VERSION="${PROJECT_VERSION}"
  SB_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

install(TARGETS sb-bench RUNTIME DESTINATION .)
