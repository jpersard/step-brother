// sb-bench - M0-Benchmark fuer step Brother.
//
// Liest eine STEP-Datei mit Open CASCADE (XCAF) und misst je Phase Zeit und Speicher:
//   1. Parsen (STEP-Text -> Entitaetenmodell)
//   2. Transfer (Entitaeten -> B-Rep-Formen im XCAF-Dokument)
//   3. Strukturanalyse (Wurzeln, Baugruppen, Prototypen, Instanzen)
//   4. Freigabe des Readers (wie viel Speicher bleibt nach dem Parsen uebrig?)
//   5. Vernetzung je Prototyp bei mehreren Toleranzen (Zeit, Dreiecke)
//
// Die Ausgabe ist JSON ohne Bauteil- oder Dateinamen, damit Messwerte geteilt werden koennen.

#include "json.h"
#include "memory.h"
#include "platform.h"

#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <DE_ShapeFixParameters.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_Printer.hxx>
#include <OSD_Parallel.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <ShapeProcess.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------
// Optionen

enum class Healing { Default, Off, NoShellOrientation };

struct Options {
  std::string file;            // UTF-8
  std::string jsonOut;         // leer = stdout
  Healing healing = Healing::Default;
  bool perRoot = false;        // TransferOneRoot je Wurzel statt Transfer()
  bool allAttributes = false;  // auch Layer, Eigenschaften, GD&T, Material, Ansichten lesen
  bool names = true;
  bool colors = true;
  bool mesh = true;
  bool meshParallelFaces = false;  // Parallelitaet innerhalb eines Teils statt ueber Teile
  std::vector<double> deflections{0.01, 0.003, 0.001};  // relativ zur Teilgroesse
  double angularDeflection = 0.5;                       // rad
};

void printUsage() {
  std::cerr <<
R"(sb-bench - STEP-Benchmark fuer step Brother

Aufruf:  sb-bench <datei.stp> [Optionen]

Optionen:
  --json <datei>          Ergebnis als JSON in Datei schreiben (sonst auf stdout)
  --healing <modus>       default | off | no-shell-orient   (Standard: default)
  --per-root              Wurzeln einzeln mit TransferOneRoot uebertragen
  --all-attributes        auch Layer, Eigenschaften, GD&T, Material, Ansichten lesen
  --no-names              keine Namen lesen
  --no-colors             keine Farben lesen
  --no-mesh               Vernetzung auslassen
  --deflection <liste>    relative Vernetzungstoleranzen, z. B. 0.01,0.003,0.001
  --angular <rad>         Winkeltoleranz der Vernetzung (Standard 0.5)
  --mesh-parallel-faces   innerhalb eines Teils parallel vernetzen statt ueber Teile
  -h, --help              diese Hilfe
)";
}

std::vector<double> parseList(const std::string& s) {
  std::vector<double> result;
  std::size_t pos = 0;
  while (pos < s.size()) {
    std::size_t end = s.find(',', pos);
    if (end == std::string::npos) end = s.size();
    result.push_back(std::stod(s.substr(pos, end - pos)));
    pos = end + 1;
  }
  return result;
}

std::optional<Options> parseArgs(const std::vector<std::string>& args) {
  Options opt;
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= args.size()) throw std::runtime_error("Wert fehlt nach " + a);
      return args[++i];
    };
    if (a == "-h" || a == "--help") {
      return std::nullopt;
    } else if (a == "--json") {
      opt.jsonOut = next();
    } else if (a == "--healing") {
      const std::string m = next();
      if (m == "default") opt.healing = Healing::Default;
      else if (m == "off") opt.healing = Healing::Off;
      else if (m == "no-shell-orient") opt.healing = Healing::NoShellOrientation;
      else throw std::runtime_error("Unbekannter Healing-Modus: " + m);
    } else if (a == "--per-root") {
      opt.perRoot = true;
    } else if (a == "--all-attributes") {
      opt.allAttributes = true;
    } else if (a == "--no-names") {
      opt.names = false;
    } else if (a == "--no-colors") {
      opt.colors = false;
    } else if (a == "--no-mesh") {
      opt.mesh = false;
    } else if (a == "--deflection") {
      opt.deflections = parseList(next());
    } else if (a == "--angular") {
      opt.angularDeflection = std::stod(next());
    } else if (a == "--mesh-parallel-faces") {
      opt.meshParallelFaces = true;
    } else if (!a.empty() && a[0] == '-') {
      throw std::runtime_error("Unbekannte Option: " + a);
    } else if (opt.file.empty()) {
      opt.file = a;
    } else {
      throw std::runtime_error("Mehr als eine Datei angegeben");
    }
  }
  if (opt.file.empty()) throw std::runtime_error("Keine STEP-Datei angegeben");
  // Grob -> fein, damit jede Stufe sauber neu vernetzt wird.
  std::sort(opt.deflections.begin(), opt.deflections.end(), std::greater<double>());
  return opt;
}

const char* healingName(Healing h) {
  switch (h) {
    case Healing::Off: return "off";
    case Healing::NoShellOrientation: return "no-shell-orient";
    default: return "default";
  }
}

// ---------------------------------------------------------------------------
// Messhilfen

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

constexpr double kMiB = 1024.0 * 1024.0;

void log(const std::string& msg) {
  std::cerr << "[sb-bench] " << msg << std::endl;
}

std::string mib(std::uint64_t bytes) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f MiB", static_cast<double>(bytes) / kMiB);
  return buf;
}

struct Phase {
  std::string name;
  double seconds = 0.0;
  sb::MemoryStats memAfter;
};

void writeMemory(sb::JsonWriter& j, const char* key, const sb::MemoryStats& m) {
  j.beginObject(key);
  j.value("current_mib", m.current / kMiB);
  j.value("peak_mib", m.peak / kMiB);
  if (m.commit) j.value("commit_mib", m.commit / kMiB);
  j.endObject();
}

// ---------------------------------------------------------------------------
// Strukturanalyse des XCAF-Dokuments

struct Structure {
  int roots = 0;                  // Wurzeln im STEP-Modell (NbRootsForTransfer)
  int freeShapes = 0;             // oberste Formen im Dokument
  int assemblies = 0;             // Baugruppen-Definitionen
  int prototypes = 0;             // eindeutige Teile (Nicht-Baugruppen auf oberster Ebene)
  std::uint64_t components = 0;   // Einbau-Referenzen in Baugruppen-Definitionen
  std::uint64_t partInstances = 0;  // Teile-Vorkommen in der aufgeloesten Anlage
  int maxDepth = 0;
  // Groesste Zahl direkter Einbau-Referenzen auf dieselbe Definition. OCCTs Transfer
  // (STEPControl_ActorRead fuer NAUO) waechst damit quadratisch - siehe docs/m0-befunde.md.
  std::uint64_t maxDirectReuse = 0;
  std::uint64_t maxOccurrences = 0;  // haeufigstes Teil in der aufgeloesten Anlage
  std::uint64_t prototypeFaces = 0;
  std::uint64_t prototypeEdges = 0;
  std::uint64_t instancedFaces = 0;  // Flaechen, wenn jede Instanz einzeln gespeichert waere
  std::vector<TDF_Label> prototypeLabels;
  std::vector<std::uint64_t> prototypeOccurrences;  // parallel zu prototypeLabels
};

Structure analyze(const occ::handle<XCAFDoc_ShapeTool>& shapeTool) {
  Structure s;

  NCollection_Sequence<TDF_Label> top;
  shapeTool->GetShapes(top);

  NCollection_Sequence<TDF_Label> freeLabels;
  shapeTool->GetFreeShapes(freeLabels);
  s.freeShapes = freeLabels.Length();

  // Alle Formen auf oberster Ebene sind direkte Kinder des ShapeTool-Labels,
  // daher ist ihr Tag ein eindeutiger Schluessel.
  std::unordered_map<int, std::vector<int>> parents;  // Kind -> Baugruppen, die es einbauen
  std::unordered_map<int, TDF_Label> byTag;
  for (const TDF_Label& label : top) {
    byTag[label.Tag()] = label;
    if (XCAFDoc_ShapeTool::IsAssembly(label)) {
      ++s.assemblies;
      NCollection_Sequence<TDF_Label> comps;
      XCAFDoc_ShapeTool::GetComponents(label, comps, false);
      for (const TDF_Label& comp : comps) {
        TDF_Label ref;
        if (XCAFDoc_ShapeTool::GetReferredShape(comp, ref)) {
          ++s.components;
          parents[ref.Tag()].push_back(label.Tag());
        }
      }
    }
  }

  std::unordered_map<int, bool> isFree;
  for (const TDF_Label& label : freeLabels) isFree[label.Tag()] = true;

  // Vorkommen je Label = (frei ? 1 : 0) + Summe der Vorkommen aller Eltern-Baugruppen.
  std::unordered_map<int, std::uint64_t> occ;
  std::unordered_map<int, int> depth;
  std::function<std::uint64_t(int)> occurrences = [&](int tag) -> std::uint64_t {
    auto it = occ.find(tag);
    if (it != occ.end()) return it->second;
    occ[tag] = 0;  // Schutz gegen Zyklen in defekten Dateien
    std::uint64_t n = isFree.count(tag) ? 1 : 0;
    int d = 0;
    auto p = parents.find(tag);
    if (p != parents.end()) {
      for (int parent : p->second) {
        n += occurrences(parent);
        d = std::max(d, depth[parent] + 1);
      }
    }
    occ[tag] = n;
    depth[tag] = d;
    return n;
  };

  for (const auto& entry : parents) {
    s.maxDirectReuse = std::max<std::uint64_t>(s.maxDirectReuse, entry.second.size());
  }

  for (const TDF_Label& label : top) {
    const std::uint64_t n = occurrences(label.Tag());
    s.maxDepth = std::max(s.maxDepth, depth[label.Tag()]);
    if (XCAFDoc_ShapeTool::IsAssembly(label)) continue;

    ++s.prototypes;
    s.partInstances += n;
    s.maxOccurrences = std::max(s.maxOccurrences, n);
    s.prototypeLabels.push_back(label);
    s.prototypeOccurrences.push_back(n);

    const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(label);
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces, edges;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    s.prototypeFaces += static_cast<std::uint64_t>(faces.Extent());
    s.prototypeEdges += static_cast<std::uint64_t>(edges.Extent());
    s.instancedFaces += static_cast<std::uint64_t>(faces.Extent()) * n;
  }
  return s;
}

// ---------------------------------------------------------------------------
// Vernetzung

struct MeshResult {
  double deflection = 0.0;
  double seconds = 0.0;
  std::uint64_t prototypeTriangles = 0;
  std::uint64_t prototypeNodes = 0;
  std::uint64_t instancedTriangles = 0;  // was die GPU ohne Culling zeichnen muesste
  std::uint64_t facesWithoutMesh = 0;
  sb::MemoryStats memAfter;
};

MeshResult meshAll(const Structure& s, double deflection, const Options& opt) {
  MeshResult r;
  r.deflection = deflection;

  const std::size_t n = s.prototypeLabels.size();
  std::vector<TopoDS_Shape> shapes(n);
  for (std::size_t i = 0; i < n; ++i) {
    shapes[i] = XCAFDoc_ShapeTool::GetShape(s.prototypeLabels[i]);
    BRepTools::Clean(shapes[i]);  // vorherige Stufe entfernen
  }

  std::atomic<std::size_t> done{0};
  const auto start = Clock::now();
  auto meshOne = [&](int i) {
    try {
      BRepMesh_IncrementalMesh mesher(shapes[i], deflection, /*isRelative=*/true,
                                      opt.angularDeflection, opt.meshParallelFaces);
    } catch (const Standard_Failure&) {
      // Fehlerhafte Teile zaehlen unten als "ohne Netz".
    }
    ++done;
  };
  if (opt.meshParallelFaces) {
    for (std::size_t i = 0; i < n; ++i) meshOne(static_cast<int>(i));
  } else {
    OSD_Parallel::For(0, static_cast<int>(n), meshOne);
  }
  r.seconds = secondsSince(start);

  for (std::size_t i = 0; i < n; ++i) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(shapes[i], TopAbs_FACE, faces);
    std::uint64_t tris = 0;
    for (int f = 1; f <= faces.Extent(); ++f) {
      TopLoc_Location loc;
      const occ::handle<Poly_Triangulation> t =
          BRep_Tool::Triangulation(TopoDS::Face(faces(f)), loc);
      if (t.IsNull()) {
        ++r.facesWithoutMesh;
        continue;
      }
      tris += static_cast<std::uint64_t>(t->NbTriangles());
      r.prototypeNodes += static_cast<std::uint64_t>(t->NbNodes());
    }
    r.prototypeTriangles += tris;
    r.instancedTriangles += tris * s.prototypeOccurrences[i];
  }
  r.memAfter = sb::queryMemory();
  return r;
}

// ---------------------------------------------------------------------------

void quietOcctMessages() {
  const occ::handle<Message_Messenger>& messenger = Message::DefaultMessenger();
  for (const occ::handle<Message_Printer>& printer : messenger->Printers()) {
    printer->SetTraceLevel(Message_Fail);
  }
}

int run(const std::vector<std::string>& args) {
  std::optional<Options> parsed;
  try {
    parsed = parseArgs(args);
  } catch (const std::exception& e) {
    std::cerr << "Fehler: " << e.what() << "\n\n";
    printUsage();
    return 2;
  }
  if (!parsed) {
    printUsage();
    return 0;
  }
  const Options& opt = *parsed;

  const fs::path path = fs::u8path(opt.file);
  std::error_code ec;
  const std::uint64_t fileSize = fs::file_size(path, ec);
  if (ec) {
    std::cerr << "Fehler: Datei nicht lesbar: " << opt.file << "\n";
    return 2;
  }

  quietOcctMessages();
  log("Datei: " + mib(fileSize) + ", Healing: " + healingName(opt.healing) +
      (opt.perRoot ? ", Transfer je Wurzel" : ""));

  std::vector<Phase> phases;
  const auto total = Clock::now();
  const sb::MemoryStats memStart = sb::queryMemory();

  occ::handle<TDocStd_Document> doc;
  XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", doc);

  Structure structure;
  bool ok = true;
  std::string error;

  try {
    // Reader in eigenem Block: Nach dem Block ist das STEP-Entitaetenmodell freigegeben.
    {
      auto reader = std::make_unique<STEPCAFControl_Reader>();
      reader->SetColorMode(opt.colors);
      reader->SetNameMode(opt.names);
      reader->SetLayerMode(opt.allAttributes);
      reader->SetPropsMode(opt.allAttributes);
      reader->SetMetaMode(opt.allAttributes);
      reader->SetGDTMode(opt.allAttributes);
      reader->SetMatMode(opt.allAttributes);
      reader->SetViewMode(opt.allAttributes);

      log("Phase 1/5: Parsen ...");
      auto t = Clock::now();
      const IFSelect_ReturnStatus status = reader->ReadFile(opt.file.c_str());
      phases.push_back({"parse", secondsSince(t), sb::queryMemory()});
      if (status != IFSelect_RetDone) {
        throw std::runtime_error("ReadFile fehlgeschlagen (Status " +
                                 std::to_string(static_cast<int>(status)) + ")");
      }
      log("  " + std::to_string(phases.back().seconds) + " s, RAM-Spitze " +
          mib(phases.back().memAfter.peak));

      // Healing nach dem Lesen setzen: Dann existiert der Actor sicher.
      if (opt.healing == Healing::Off) {
        reader->SetShapeProcessFlags(ShapeProcess::OperationsFlags{});
      } else if (opt.healing == Healing::NoShellOrientation) {
        DE_ShapeFixParameters fix;
        fix.FixShellOrientationMode = DE_ShapeFixParameters::FixMode::NotFix;
        reader->SetShapeFixParameters(fix);
      }

      structure.roots = reader->NbRootsForTransfer();
      log("Phase 2/5: Transfer (" + std::to_string(structure.roots) + " Wurzeln) ...");
      t = Clock::now();
      bool transferred = true;
      if (opt.perRoot) {
        for (int i = 1; i <= structure.roots; ++i) {
          transferred = reader->TransferOneRoot(i, doc) && transferred;
        }
      } else {
        transferred = reader->Transfer(doc);
      }
      phases.push_back({"transfer", secondsSince(t), sb::queryMemory()});
      if (!transferred) log("  Warnung: Transfer meldet Fehler, Ergebnis ist evtl. unvollstaendig");
      log("  " + std::to_string(phases.back().seconds) + " s, RAM-Spitze " +
          mib(phases.back().memAfter.peak));

      log("Phase 3/5: Reader freigeben ...");
      t = Clock::now();
      reader.reset();
      phases.push_back({"release_reader", secondsSince(t), sb::queryMemory()});
      log("  RAM danach " + mib(phases.back().memAfter.current));
    }

    log("Phase 4/5: Struktur analysieren ...");
    auto t = Clock::now();
    const int roots = structure.roots;
    structure = analyze(XCAFDoc_DocumentTool::ShapeTool(doc->Main()));
    structure.roots = roots;
    phases.push_back({"analyze", secondsSince(t), sb::queryMemory()});
    log("  " + std::to_string(structure.prototypes) + " Prototypen, " +
        std::to_string(structure.partInstances) + " Teile-Instanzen, " +
        std::to_string(structure.assemblies) + " Baugruppen");
  } catch (const Standard_Failure& e) {
    ok = false;
    error = std::string("OCCT: ") + e.what();
  } catch (const std::exception& e) {
    ok = false;
    error = e.what();
  }

  std::vector<MeshResult> meshes;
  Bnd_Box bbox;
  if (ok && opt.mesh) {
    for (double d : opt.deflections) {
      log("Phase 5/5: Vernetzen, Toleranz " + std::to_string(d) + " ...");
      try {
        meshes.push_back(meshAll(structure, d, opt));
        const MeshResult& m = meshes.back();
        log("  " + std::to_string(m.seconds) + " s, " + std::to_string(m.prototypeTriangles) +
            " Dreiecke (Prototypen), " + std::to_string(m.instancedTriangles) +
            " Dreiecke (alle Instanzen)");
      } catch (const Standard_Failure& e) {
        log(std::string("  Fehler: ") + e.what());
      }
    }
  }
  if (ok) {
    // Ausdehnung der Anlage - wichtig fuer die Float-Genauigkeit im Renderer.
    try {
      NCollection_Sequence<TDF_Label> freeLabels;
      XCAFDoc_DocumentTool::ShapeTool(doc->Main())->GetFreeShapes(freeLabels);
      for (const TDF_Label& label : freeLabels) {
        BRepBndLib::Add(XCAFDoc_ShapeTool::GetShape(label), bbox, /*useTriangulation=*/true);
      }
    } catch (const Standard_Failure&) {
    }
  }

  const double totalSeconds = secondsSince(total);
  const sb::MemoryStats memEnd = sb::queryMemory();

  // ---- JSON-Ausgabe ----
  sb::JsonWriter j;
  j.beginObject();
  j.value("tool", "sb-bench");
  j.value("tool_version", SB_VERSION);
  j.value("occt_version", OCC_VERSION_COMPLETE);
  j.value("compiler", SB_COMPILER);
  j.value("hardware_threads", static_cast<int>(std::thread::hardware_concurrency()));
  j.value("ok", ok);
  if (!ok) j.value("error", error);

  j.beginObject("options");
  j.value("healing", healingName(opt.healing));
  j.value("per_root", opt.perRoot);
  j.value("all_attributes", opt.allAttributes);
  j.value("names", opt.names);
  j.value("colors", opt.colors);
  j.value("mesh_parallel", opt.meshParallelFaces ? "faces" : "prototypes");
  j.value("angular_deflection", opt.angularDeflection);
  j.endObject();

  j.beginObject("file");
  j.value("size_mib", fileSize / kMiB);
  j.endObject();

  writeMemory(j, "memory_start", memStart);

  j.beginArray("phases");
  for (const Phase& p : phases) {
    j.beginObject();
    j.value("name", p.name);
    j.value("seconds", p.seconds);
    if (p.name == "parse" && p.seconds > 0) {
      j.value("mib_per_second", fileSize / kMiB / p.seconds);
    }
    writeMemory(j, "memory_after", p.memAfter);
    j.endObject();
  }
  j.endArray();

  j.beginObject("structure");
  j.value("roots", structure.roots);
  j.value("free_shapes", structure.freeShapes);
  j.value("assemblies", structure.assemblies);
  j.value("prototypes", structure.prototypes);
  j.value("components", structure.components);
  j.value("part_instances", structure.partInstances);
  j.value("max_depth", structure.maxDepth);
  j.value("max_direct_reuse", structure.maxDirectReuse);
  j.value("max_part_occurrences", structure.maxOccurrences);
  j.value("prototype_faces", structure.prototypeFaces);
  j.value("prototype_edges", structure.prototypeEdges);
  j.value("instanced_faces", structure.instancedFaces);
  if (structure.prototypes > 0) {
    j.value("reuse_factor",
            static_cast<double>(structure.partInstances) / structure.prototypes);
  }
  j.endObject();

  j.beginArray("mesh");
  for (const MeshResult& m : meshes) {
    j.beginObject();
    j.value("relative_deflection", m.deflection);
    j.value("seconds", m.seconds);
    j.value("prototype_triangles", m.prototypeTriangles);
    j.value("prototype_nodes", m.prototypeNodes);
    j.value("instanced_triangles", m.instancedTriangles);
    j.value("faces_without_mesh", m.facesWithoutMesh);
    writeMemory(j, "memory_after", m.memAfter);
    j.endObject();
  }
  j.endArray();

  if (!bbox.IsVoid()) {
    double x0, y0, z0, x1, y1, z1;
    bbox.Get(x0, y0, z0, x1, y1, z1);
    j.beginObject("extent");
    j.value("dx", x1 - x0);
    j.value("dy", y1 - y0);
    j.value("dz", z1 - z0);
    j.value("max_abs_coordinate",
            std::max({std::abs(x0), std::abs(y0), std::abs(z0), std::abs(x1), std::abs(y1),
                      std::abs(z1)}));
    j.endObject();
  }

  j.value("total_seconds", totalSeconds);
  writeMemory(j, "memory_end", memEnd);
  j.endObject();

  if (opt.jsonOut.empty()) {
    std::cout << j.str();
  } else {
    std::ofstream out(fs::u8path(opt.jsonOut), std::ios::binary);
    out << j.str();
    log("Ergebnis geschrieben: " + opt.jsonOut);
  }
  log("Fertig in " + std::to_string(totalSeconds) + " s, RAM-Spitze gesamt " + mib(memEnd.peak));
  return ok ? 0 : 1;
}

}  // namespace

// Unter MinGW haengt der Prozess nach getaner Arbeit beim Beenden: OCCTs Thread-Pool
// (parallele Vernetzung) wartet beim Entladen der DLLs auf seine Threads, waehrend Windows
// die Loader-Sperre haelt. Daher dort nach dem Rausschreiben sofort beenden.
// Siehe docs/m0-befunde.md, Abschnitt 4.
int finish(int code) {
  std::cout.flush();
  std::cerr.flush();
  std::fflush(nullptr);
#if defined(__MINGW32__)
  sb::hardExit(code);
#endif
  return code;
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
  sb::enableUtf8Console();
  return finish(run(sb::utf8Args(argc, argv)));
}
#else
int main(int argc, char** argv) {
  return finish(run(std::vector<std::string>(argv, argv + argc)));
}
#endif
