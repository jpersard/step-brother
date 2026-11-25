# M0-Befunde

Erkenntnisse aus dem Benchmark. Die Messwerte stammen aus synthetischen Dateien
(`sb-gen-step`), gemessen mit OCCT 8.0.1 und GCC 13 unter Linux auf 4 Kernen. Die
Messung mit einer echten Anlage steht noch aus.

## 1. Transfer wächst quadratisch mit der Wiederverwendung eines Teils

**Beobachtung:** Ein Modul (eine Baugruppe aus 7 Teilen) wird N-mal in eine Anlage
eingebaut. Das Parsen wächst linear mit N, der Transfer dagegen quadratisch.

| Einbauten (N) | Dateigröße | Parsen | Transfer |
| ---: | ---: | ---: | ---: |
| 225 | 0,2 MB | 0,02 s | 0,03 s |
| 900 | 0,6 MB | 0,05 s | 0,30 s |
| 3.600 | 2,3 MB | 0,15 s | 3,1 s |
| 8.100 | 5,1 MB | 0,33 s | 18,6 s |

Bei 2,25-mal so vielen Einbauten steigt die Transferzeit um das Sechsfache. Namen
und Farben haben darauf keinen Einfluss (`--no-names --no-colors` ändert nichts).

**Ursache:** Die Stichproben-Profile zeigen auf
`STEPControl_ActorRead::TransferEntity(NextAssemblyUsageOccurrence)`.

- Für jede Einbau-Referenz (NAUO) ruft die Funktion `graph.Sharings(rep)` für die
  Darstellung des eingebauten Teils auf und geht die Ergebnisliste durch.
- Diese Liste enthält **alle** Einbau-Beziehungen, die auf diese Darstellung
  verweisen. Ein Teil, das N-mal eingebaut ist, kostet also N Schritte je Einbau,
  zusammen N².
- `STEPConstruct_Assembly::CheckSRRReversesNAUO` macht dasselbe noch einmal.

Der Code ist in OCCT 8.0.1 und auf dem aktuellen Master (Stand 11.11.2026)
unverändert.

**Bedeutung für step Brother:** Anlagen bauen genau so: Schrauben, Muttern, Profile
und Normteile werden tausend- bis zehntausendfach eingebaut. Hochgerechnet würde
ein Teil mit 50.000 Einbauten allein im Transfer **Stunden** kosten, unabhängig von
der Dateigröße. Ob die echte Anlage betroffen ist, zeigt `sb-bench` mit dem Wert
`structure.max_direct_reuse`. Ab einigen tausend wird es teuer.

**Optionen, Entscheidung nach dem Benchmark mit der echten Anlage:**

1. **OCCT patchen:** Die Zuordnung „Darstellung → Produktdefinition“ einmal vorab
   berechnen statt bei jedem Einbau neu. Das ginge über einen vcpkg-Overlay-Port;
   die Änderung wird gemäß LGPL offengelegt und an Open Cascade gemeldet.
2. **Eigener Struktur-Leser im Importer:** Die Produktstruktur (NAUO,
   Platzierungen) liest der Importer selbst mit linearen Zuordnungstabellen aus dem
   STEP-Entitätenmodell. OCCT überträgt nur noch die Geometrie der Prototypen. Das
   passt zum Cache-Konzept (Prototypen plus Instanzen) und spart zusätzlich den
   Aufbau des XCAF-Dokuments, ist aber mehr Eigenbau.
3. **Nichts tun,** falls die echte Anlage nur geringe direkte Wiederverwendung hat.

## 2. Healing lässt sich gezielt abschalten

`STEPControl_Reader` führt standardmäßig nur die Operation `FixShape` aus.

- `--healing off` setzt eine leere Operationsliste, dann wird gar nicht geheilt.
- `--healing no-shell-orient` lässt das Healing an und schaltet nur
  `FixShellOrientationMode` ab.

Beide Wege funktionieren mit OCCT 8.0.1. Wie stark sie die Zeit beeinflussen, zeigt
erst eine echte Datei; die synthetischen Teile brauchen kein Healing.

## 3. Kein eingebauter Parallelmodus für das STEP-Lesen

In OCCT 8.0.1 gibt es keinen Schalter, der das Parsen oder den Transfer einer
einzelnen Datei parallelisiert. OCCT stellt nur sicher, dass mehrere Reader in
eigenen Threads gleichzeitig laufen können. Parallel arbeitet nur die Vernetzung:
`sb-bench` vernetzt die Prototypen parallel.

## 4. MinGW: Prozess hängt beim Beenden

Mit MinGW-w64 erledigt `sb-bench` die gesamte Arbeit, der Prozess beendet sich danach
aber nicht. Wahrscheinliche Ursache: OCCTs Thread-Pool (`OSD_ThreadPool`, genutzt von
der parallelen Vernetzung) wartet beim Entladen der DLLs auf seine Threads, während
Windows die Loader-Sperre hält. Mit MSVC und unter Linux tritt das nicht auf.

`sb-bench` beendet sich unter MinGW deshalb nach dem Schreiben der Ergebnisse direkt
mit `std::_Exit`. Für den Viewer in M1 ist das keine Lösung. Bleibt MinGW nach dem
Compiler-Vergleich im Rennen, muss der Thread-Pool vor dem Programmende sauber
heruntergefahren werden.
