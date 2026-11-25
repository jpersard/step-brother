#pragma once

#include <cstdint>

namespace sb {

// Speicherstand des eigenen Prozesses in Bytes.
struct MemoryStats {
  std::uint64_t current = 0;  // aktuell belegter Arbeitsspeicher (Working Set / RSS)
  std::uint64_t peak = 0;     // Spitze seit Programmstart
  std::uint64_t commit = 0;   // zugesagter privater Speicher (Windows: inkl. Auslagerung), sonst 0
};

MemoryStats queryMemory();

}  // namespace sb
