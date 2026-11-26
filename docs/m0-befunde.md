# M0-Befunde

Erkenntnisse aus dem Benchmark. Abschnitt 0 ist eine echte Anlage auf dem Ziel-PC,
die übrigen Abschnitte stammen aus synthetischen Dateien (`sb-gen-step`), gemessen mit
OCCT 8.0.1 und GCC 13 unter Linux auf 4 Kernen.

## 0. Erste echte Anlage (509 MB, MSVC, 20 Threads, ohne Vernetzung)

| Messwert | Ergebnis |
| --- | --- |
| Parsen | 37 s (13,8 MB/s), RAM-Spitze 3,2 GB |
| Transfer | **385 s**, RAM-Spitze 3,4 GB |
| Nach Freigabe des Readers | 1,65 GB |
| Struktur | 1 Wurzel, 1.565 Baugruppen, 2.813 Prototypen, 8.447 Instanzen, Tiefe 12 |
| Wiederverwendung | Faktor 3,0; `max_direct_reuse` 82 |
| Flächen / Kanten (Prototypen) | 193.000 / 511.000 |

**Folgerungen:**

- **Speicher ist unkritisch:** 3,5 GB Spitze bei 36 GB RAM. Transfer je Wurzel oder
  zusätzliche Sparmaßnahmen sind für diese Anlage nicht nötig.
- **Der Transfer ist der Engpass** (89 % der Zeit), aber nicht wegen des quadratischen
  NAUO-Problems (Abschnitt 1): Bei 82 direkten Einbauten spielt es keine Rolle. Die Zeit
  steckt in der Umwandlung der Geometrie, vermutlich zu großen Teilen im Shape-Healing.
  Wird geprüft mit `--healing off`.
- **Wenig Gleichteile:** Instancing spart hier nur den Faktor 3. Der Renderer muss also
  auch mit vielen verschiedenen Teilen schnell sein.
- **Unbegrenzte Elemente:** Die Datei enthält mindestens ein Element ohne endliche
  Ausdehnung (Bounding Box 2e100 in Z). Wahrscheinlich ist das eine Konstruktionsachse oder eine
  Fläche ohne Randkanten. `sb-bench` zählt solche Elemente jetzt (`unbounded_pieces`)
  und lässt sie bei der Ausdehnung weg. Der Importer muss sie später ebenfalls
  aussortieren.

### Healing aus und Vernetzung

| Lauf | Parsen | Transfer | Gesamt |
| --- | ---: | ---: | ---: |
| Healing an (`default`), Lauf 1 | 37 s | 385 s | 433 s |
| Healing an (`default`), Lauf 2 | 35 s | 494 s | 534 s ohne Netz |
| **Healing aus (`off`)** | 66 s | **35 s** | 106 s |

Healing kostet 90 % der Transferzeit. Ohne Healing ändert sich die Struktur kaum
(192.937 statt 192.973 Flächen, 505.940 statt 510.667 Kanten). Die Parse-Zeit schwankt
bei gleichen Einstellungen zwischen 35 und 66 s, vermutlich durch den Datei-Cache von
Windows oder den Virenscanner. Vergleiche brauchen daher mehrere Läufe.

Vernetzung (mit Healing, parallel über Prototypen, 20 Threads):

| Relative Toleranz | Zeit | Dreiecke Prototypen | Dreiecke alle Instanzen | Flächen ohne Netz |
| ---: | ---: | ---: | ---: | ---: |
| 0,01 | 51 s | 5,5 Mio. | 9,8 Mio. | 98 |
| 0,003 | 58 s | 8,8 Mio. | 15,7 Mio. | 98 |
| 0,001 | 81 s | 18,3 Mio. | 33,1 Mio. | 142 |

**Vorläufige Folgerungen:**

- Healing im Importer standardmäßig aus. Ob die Netzqualität ohne Healing reicht, zeigt
  die Zahl der Flächen ohne Netz. Reicht sie nicht, werden nur die fehlerhaften
  Prototypen gezielt nachgeheilt.
- Erstimport geschätzt 2–2,5 min statt 12 min.
- 10–16 Mio. Dreiecke für die ganze Anlage sind für den Renderer machbar. Kandidaten
  für die LOD-Stufen: 0,003 (volle Qualität) und 0,01 (beim Drehen).

### Compiler-Vergleich (Healing aus, nur Lesen, 20 Threads)

| Variante | Lauf | Parsen | Transfer | Gesamt |
| --- | --- | ---: | ---: | ---: |
| MSVC 19.52 | 2 | 36,5 s | 15,5 s | 57 s |
| clang-cl 22.1 | 1 | 45,8 s | 21,1 s | 73 s |
| clang-cl 22.1 | 2 | 35,3 s | 16,5 s | 57 s |
| MinGW GCC 16.2 | 1 | 31,1 s | 14,2 s | 50 s |
| MinGW GCC 16.2 | 2 | 32,0 s | 16,1 s | 53 s |

Die Unterschiede liegen bei rund 10 % und damit nahe am Messrauschen. Der früher
berichtete Faktor 2–3 zugunsten von GCC (OCCT-Issue 32014) tritt mit OCCT 8 nicht auf.
**Empfehlung: MSVC.** MinGW wäre beim Parsen etwas schneller, bringt aber den Hänger
beim Programmende (Abschnitt 4), zusätzliche Laufzeit-DLLs und einen aufwendigeren
Build mit. clang-cl bietet keinen messbaren Vorteil.

### Offener Punkt: unbegrenzte Elemente ohne Healing

Ohne Healing zählt `sb-bench` 155.506 Flächen und Kanten ohne endliche Ausdehnung.
Vermutung: Vielen Flächen fehlen die 2D-Randkurven (pcurves), die das Healing sonst
ergänzt. Ob die Vernetzung darunter leidet, zeigt die Messung mit Healing aus und
Vernetzung (`faces_without_mesh`). Falls ja, braucht der Importer ein gezieltes,
schnelles Teil-Healing statt des vollen `FixShape`.

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
mit `TerminateProcess`. `std::_Exit` reicht nicht, weil es unter Windows `ExitProcess`
aufruft und die DLL-Entladeroutinen damit trotzdem laufen. Für den Viewer in M1 ist das keine Lösung. Bleibt MinGW nach dem
Compiler-Vergleich im Rennen, muss der Thread-Pool vor dem Programmende sauber
heruntergefahren werden.
