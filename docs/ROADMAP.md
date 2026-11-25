# Roadmap

step Brother soll große STEP-Anlagen (500 MB bis 1 GB) auf einem Windows-PC schnell
öffnen und flüssig darstellen. Diese Roadmap beschreibt die Meilensteine bis
Version 1.0 und was danach kommt.

## Ziele

- **Öffnen:** Eine 1-GB-Anlage nach dem ersten Import in **unter 10 Sekunden** öffnen.
- **Darstellen:** Die ganze Anlage mit **60 fps** drehen.
- **Offline:** Keine Netzwerkzugriffe, keine Telemetrie, keine Cloud.
- **Portabel:** Ein Ordner mit `stepbrother.exe`, keine Installation, keine Adminrechte.

Die Zielwerte sind Annahmen. Die Benchmarks in M0 und M1 müssen sie bestätigen.

## Grundidee

Zwei Programme teilen sich einen Cache:

1. **Importer** (`sbc-import-step.exe`): Er liest die STEP-Datei **einmal** mit
   Open CASCADE, vernetzt sie und schreibt einen kompakten Cache (`.sbc`). Er läuft
   als eigener Prozess. Stürzt OCCT ab oder läuft der Speicher voll, bleibt der
   Viewer stehen.
2. **Viewer** (`stepbrother.exe`): Er liest nie STEP, sondern immer nur den Cache.
   Den Cache kann er direkt in den Speicher abbilden (memory-mapped). So öffnet er
   die Datei in Sekunden statt Minuten.

Gleichteile übernimmt der Importer aus der STEP-Produktstruktur. Jedes Teil wird nur
einmal vernetzt und gespeichert und per GPU-Instancing gezeichnet. Nur der Importer
und die Messschicht (`sb-core`) kennen OCCT. Renderer und Oberfläche arbeiten mit
eigenen Datentypen.

**Technik:**

| Bereich | Wahl |
| --- | --- |
| Sprache und STEP-Kern | C++17, Open CASCADE 8.0.x (XCAF) |
| Oberfläche | Qt 6 Widgets |
| 3D-Ansicht | eigener OpenGL-4.5-Renderer |
| Build | CMake + vcpkg |
| Windows-Builds | automatisch über GitHub Actions |

## M0 – Fundament & Benchmark *(in Arbeit)*

- [x] Build-Konfiguration: CMake Presets, vcpkg mit fester Baseline, OCCT 8.0.1
- [x] Windows-Builds über GitHub Actions mit MSVC, clang-cl und MinGW-w64,
      Binär-Cache und Rauchtest
- [x] Lizenzdateien (`LICENSE`, `THIRD_PARTY_LICENSES.txt`)
- [x] `sb-bench`: Benchmark auf der Kommandozeile
  - Parse- und Transfer-Zeit getrennt
  - Speicher je Phase, auch nach Freigabe des STEP-Modells
  - Zahl der Wurzeln, Baugruppen, Prototypen und Instanzen
  - Vernetzungszeit und Dreieckszahl bei mehreren Toleranzen
  - Ausdehnung der Anlage
- [x] `sb-gen-step`: synthetische Test-Anlage mit Gleichteilen
- [x] Erster Befund: OCCTs Transfer wächst quadratisch mit der Wiederverwendung
      eines Teils, siehe [M0-Befunde](m0-befunde.md)
- [ ] Benchmark mit echter Testanlage: alle drei Compiler, Healing an und aus,
      Transfer am Stück und je Wurzel
- [ ] Entscheidung zum Transfer-Engpass: OCCT patchen, eigener Struktur-Leser oder
      nichts tun

**Abnahme:** Die Messwerte liegen vor, ein Compiler ist festgelegt, und die
Speicherspitze beim Parsen ist bekannt.

## M1 – MVP

- **Importer:** Baumstruktur, Namen und Farben zuerst, danach die Geometrie.
  - Prototypen werden parallel vernetzt.
  - Healing ist weitgehend aus.
  - Zwischenstrukturen werden sofort freigegeben.
- **Cache `.sbc`:**
  - Header mit Magic, Formatversion, Importer-Version, Größe und Änderungsdatum der
    Quelldatei sowie einem schnellen Hash. Bei Abweichung wird der Cache neu gebaut.
  - Aufbau aus Blöcken (Chunks) mit Typ und Länge, damit später neue Datentypen
    dazukommen können.
  - Quantisierte Positionen und 16-Bit-Normalen.
- **Renderer, strikt minimal:**
  - Dreiecks-Instancing und Multi-Draw-Indirect
  - Basis-Shading und Kanten aus der B-Rep
  - Picking über eine ID-Textur
  - Frustum-Culling
- **Genauigkeit:** Instanz-Transformationen werden auf der CPU in `double` gehalten
  und relativ zur Kamera gerendert. So zittert und verspringt nichts bei großen
  Anlagen weit weg vom Ursprung.
- **Modellbaum:**
  - Virtuelles Qt-Model für über 100.000 Knoten
  - Ein- und Ausblenden, Isolieren
  - Suche nach Namen im Hintergrund
  - Auswahl synchron mit der 3D-Ansicht
- **Navigation:**
  - Drehen, Verschieben, Zoomen, Zoom auf Auswahl
  - Ansichtswürfel und Standardansichten
  - Perspektive und orthogonal
  - Maussteuerung wahlweise wie SolidWorks oder Inventor
- **Darstellung:** schattiert, schattiert mit Kanten, Drahtgitter, Originalfarben
- **Messen (Basis):** Abstand Punkt–Punkt, Koordinaten eines Punkts, Bounding Box

**Abnahme:** Die Testanlage öffnet, der zweite Start aus dem Cache dauert Sekunden,
und das Drehen ist flüssig.

## M2 – Messwerkzeuge

- Exakte Teilgeometrie (B-Rep) je Prototyp im Cache. Sie wird beim Messen nur für das
  angeklickte Teil geladen.
- Analytische Flächen: Ebene, Zylinder, Kreis(bogen), Gerade
- Fang auf Kanten und Kreise
- Durchmesser und Radius, Abstand Kante–Kante und Fläche–Fläche, Winkel,
  Kreismittelpunkt, Mindestabstand zweier Teile, Flächeninhalt

**Abnahme:** Die Messwerte stimmen an einem Prüfteil mit den CAD-Werten überein.

## M3 – Schnitt & Darstellung

- Bis zu 3 Schnittebenen in X, Y und Z als Clip-Planes, frei verschiebbar, zunächst
  ohne Füllung
- Transparenz
- Size-Culling, LOD-Stufen nach Bildschirmgröße, reduzierte Qualität beim Drehen

**Abnahme:** Ein Schnitt durch die ganze Anlage läuft ohne Ruckeln.

## M4 – Feinschliff & Version 1.0

- Gefüllte Schnittflächen (Capping)
- Explosionsansicht, Screenshots, gespeicherte Ansichten
- Bauteil-Eigenschaften: Name, Größe, Anzahl Instanzen
- Einstellungen, Tastenkürzel, zuletzt geöffnete Dateien
- Dateizuordnung `.stp`/`.step`, Cache-Verwaltung, portabler Ordner

**Abnahme:** Im Alltag nutzbar.

## Danach

- Volumen und Gewicht, Teileliste als CSV, PMI-Anzeige
- 2D-Modus mit DXF und DWG, jeweils als eigener Importer, der denselben Cache
  schreibt. GPL-Bibliotheken wie LibreDWG kommen nur in ein separates
  Importer-Programm, nie in den Viewer.
- Weitere Formate wie IGES und JT
- Geometrie-Hash für Gleichteile aus schlecht exportierten Dateien

## Risiken

| Risiko | Gegenmaßnahme |
| --- | --- |
| Speicherspitze beim Parsen zu hoch | In M0 messen, Importer als eigener Prozess, Speicher sofort freigeben |
| Erster Import bleibt langsam | Cache für jedes weitere Öffnen, Fortschrittsanzeige, Import im Voraus |
| Eigener Renderer wächst zu stark | M1 strikt minimal, alles Weitere erst nach bestandenem Benchmark |
| Falsche Maße bei Freiformflächen | Analytisch nur einfache Flächen, sonst exakte B-Rep |
| Gleichteile ohne STEP-Referenzen | Geometrie-Hash nur bei Bedarf nachrüsten |
| Veralteter Cache | Header mit Versionen, Größe, Datum, Hash |
| Fehlerhafte STEP-Dateien | Fehlerliste im Viewer, Healing pro Teil zuschaltbar |
