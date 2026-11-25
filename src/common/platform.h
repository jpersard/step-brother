#pragma once

// Plattform-Hilfen ohne OCCT-Abhaengigkeit. Windows-Code steckt bewusst in einer eigenen
// Uebersetzungseinheit: OCCTs Standard_Macro.hxx definiert NONLS, NOUSER usw., und
// windows.h vor OCCT einzubinden fuehrt zu Makro-Kollisionen.

#include <string>
#include <vector>

namespace sb {

#if defined(_WIN32)
// Kommandozeile aus wmain() als UTF-8 - so funktionieren Pfade mit Umlauten.
std::vector<std::string> utf8Args(int argc, wchar_t** argv);
#endif

// Konsole auf UTF-8 umstellen (nur Windows, sonst ohne Wirkung).
void enableUtf8Console();

}  // namespace sb
