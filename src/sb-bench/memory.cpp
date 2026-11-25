#include "memory.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <string>
#endif

namespace sb {

#if defined(_WIN32)

MemoryStats queryMemory() {
  MemoryStats stats;
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  if (GetProcessMemoryInfo(GetCurrentProcess(),
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                           sizeof(counters))) {
    stats.current = counters.WorkingSetSize;
    stats.peak = counters.PeakWorkingSetSize;
    stats.commit = counters.PrivateUsage;
  }
  return stats;
}

#else

namespace {
// Liest einen Wert wie "VmRSS:   12345 kB" aus /proc/self/status.
std::uint64_t readStatusKb(const std::string& key) {
  std::ifstream status("/proc/self/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.compare(0, key.size(), key) == 0) {
      return std::stoull(line.substr(key.size())) * 1024ULL;
    }
  }
  return 0;
}
}  // namespace

MemoryStats queryMemory() {
  MemoryStats stats;
  stats.current = readStatusKb("VmRSS:");
  stats.peak = readStatusKb("VmHWM:");
  return stats;
}

#endif

}  // namespace sb
