# sb-gen-step: erzeugt eine synthetische STEP-Anlage mit Gleichteilen fuer Tests und CI.
add_executable(sb-gen-step main.cpp)

target_link_libraries(sb-gen-step PRIVATE
  sb-common
  TKernel TKMath TKBRep TKG3d TKTopAlgo TKPrim TKBO
  TKLCAF TKXCAF TKXSBase TKDESTEP)

if(MINGW)
  target_link_options(sb-gen-step PRIVATE -municode)
endif()

install(TARGETS sb-gen-step RUNTIME DESTINATION .)
