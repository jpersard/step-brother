#include "platform.h"

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace sb {

#if defined(_WIN32)
std::vector<std::string> utf8Args(int argc, wchar_t** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(len > 0 ? len - 1 : 0), '\0');
    if (len > 1) WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, s.data(), len, nullptr, nullptr);
    args.push_back(std::move(s));
  }
  return args;
}

void enableUtf8Console() { SetConsoleOutputCP(CP_UTF8); }

void hardExit(int code) {
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
  std::_Exit(code);  // nicht erreicht
}
#else
void enableUtf8Console() {}

void hardExit(int code) { std::_Exit(code); }
#endif

}  // namespace sb
