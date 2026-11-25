# step Brother

Der verlässliche Bruder für deine STEP-Dateien: ein schneller Windows-Viewer für große
STEP-Anlagen (500 MB bis 1 GB), gebaut auf Open CASCADE 8.0, Qt 6 und einem eigenen
OpenGL-Renderer.

**Stand: M0 – Fundament & Benchmark.** Es gibt noch keinen Viewer, sondern zwei
Kommandozeilen-Werkzeuge, die klären, wie schnell und wie speicherhungrig das Lesen
deiner echten Anlagen ist. Wie es weitergeht, steht in der [Roadmap](docs/ROADMAP.md).

| Programm | Zweck |
| --- | --- |
| `sb-bench.exe` | Liest eine STEP-Datei und misst Parsen, Transfer, Speicher, Teile-/Prototypen-/Instanzzahl und Vernetzung. Ausgabe als JSON. |
| `sb-gen-step.exe` | Erzeugt eine synthetische Test-Anlage mit Gleichteilen (für CI und Skalierungstests). |

## Fertige Programme herunterladen

Jeder Push baut automatisch drei Windows-Varianten über GitHub Actions:

1. Im Repo auf **Actions** → **Build** → den neuesten grünen Lauf öffnen.
2. Unten unter **Artifacts** eine Variante laden:
   - `step-brother-m0-windows-msvc`
   - `step-brother-m0-windows-clang-cl`
   - `step-brother-m0-windows-mingw`
3. ZIP entpacken – der Ordner ist portabel, keine Installation nötig.

Der erste Lauf baut Open CASCADE und dauert ein bis zwei Stunden, danach liegen die
Bibliotheken im Cache und ein Lauf dauert nur noch Minuten.

## Benchmark ausführen (M0)

In der Eingabeaufforderung im entpackten Ordner:

```bat
sb-bench.exe "D:\Anlagen\anlage.stp" --json anlage-msvc.bench.json
```

Für den Compiler-Vergleich dieselbe Datei mit allen drei Varianten messen. Weitere
sinnvolle Läufe:

```bat
sb-bench.exe anlage.stp --healing off --json healing-off.bench.json
sb-bench.exe anlage.stp --per-root --no-mesh --json per-root.bench.json
```

Wichtige Optionen (`sb-bench.exe --help` zeigt alle):

| Option | Bedeutung |
| --- | --- |
| `--healing default\|off\|no-shell-orient` | Shape-Healing normal, ganz aus, oder nur `FixShellOrientationMode` aus |
| `--per-root` | Wurzeln einzeln mit `TransferOneRoot` übertragen |
| `--no-mesh` | Vernetzung auslassen (schneller, nur Lesen messen) |
| `--deflection 0.01,0.003,0.001` | relative Vernetzungstoleranzen |
| `--all-attributes` | auch Layer, Eigenschaften, GD&T, Material, Ansichten lesen |

**Datenschutz:** Die JSON-Datei enthält keine Datei-, Bauteil- oder Baugruppennamen,
nur Zahlen (Zeiten, Speicher, Anzahlen, Ausdehnung). Sie kann gefahrlos geteilt
werden. STEP-Dateien gehören nie ins Repo – `.gitignore` schließt sie aus.

### Was gemessen wird

- **Phasen:** `parse` (STEP-Text einlesen), `transfer` (B-Rep erzeugen),
  `release_reader` (Speicher nach Freigabe des STEP-Modells), `analyze`.
- **Speicher je Phase:** aktueller Wert, Spitze seit Start und (Windows) zugesagter
  Speicher inkl. Auslagerung. Die Spitze nach `parse` ist die wichtigste Zahl.
- **Struktur:** Wurzeln, Baugruppen, Prototypen (eindeutige Teile), Teile-Instanzen,
  Wiederverwendungsfaktor, Flächen und Kanten.
- **Vernetzung je Toleranz:** Zeit, Dreiecke der Prototypen und Dreiecke über alle
  Instanzen (das müsste die Grafikkarte ohne Culling zeichnen).
- **Ausdehnung:** Größe der Anlage und größte Koordinate – entscheidet, wie viel
  Genauigkeit der Renderer braucht.

## Selbst bauen (optional)

Voraussetzungen: Visual Studio 2022 Build Tools mit „Desktopentwicklung mit C++“
(für clang-cl zusätzlich „C++-Clang-Tools für Windows“), Git, und vcpkg:

```bat
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
setx VCPKG_ROOT C:\vcpkg
```

Dann in einer „x64 Native Tools Command Prompt for VS 2022“:

```bat
cmake --preset windows-msvc
cmake --build --preset windows-msvc
```

Die Programme liegen in `build\windows-msvc\bin`. In VS Code mit der Erweiterung
„CMake Tools“ lässt sich das Preset direkt auswählen.

## Projektaufbau

```
docs/ROADMAP.md     Meilensteine bis Version 1.0
src/sb-bench/       M0-Benchmark
src/sb-gen-step/    Generator für synthetische Test-Anlagen
cmake/triplets/     vcpkg-Triplet für clang-cl
cmake/toolchains/   zugehörige Toolchain
.github/workflows/  Windows-Builds (MSVC, clang-cl, MinGW) mit Rauchtest
```

## Lizenzen

Der Code von step Brother steht unter der [MIT-Lizenz](LICENSE).

step Brother nutzt Open CASCADE Technology (LGPL 2.1 mit Open-CASCADE-Ausnahme),
dynamisch gelinkt. Alle Lizenztexte stehen in
[`THIRD_PARTY_LICENSES.txt`](THIRD_PARTY_LICENSES.txt) und liegen jedem Build bei.
