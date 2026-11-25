# sb-common: kleine Hilfen ohne OCCT (Plattform, UTF-8).
add_library(sb-common STATIC platform.cpp platform.h)
target_include_directories(sb-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
