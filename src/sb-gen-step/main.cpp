// sb-gen-step - erzeugt eine synthetische STEP-Anlage fuer Tests.
//
// Aufbau: ein Raster aus N x N Modulen. Jedes Modul ist eine Baugruppe aus wenigen
// Prototypen (Lochplatte, Stuetze, Rohr), die mehrfach eingebaut werden. Damit testet
// die Datei genau das, worauf step Brother setzt: Gleichteile ueber die STEP-Produktstruktur.

#include "platform.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_Printer.hxx>
#include <Quantity_Color.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <Standard_Failure.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <NCollection_List.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>


namespace {

// Platte 400 x 300 x 20 mm mit holes x holes Bohrungen.
TopoDS_Shape makePlate(int holes) {
  TopoDS_Shape plate = BRepPrimAPI_MakeBox(400.0, 300.0, 20.0).Shape();
  NCollection_List<TopoDS_Shape> tools;
  for (int i = 0; i < holes; ++i) {
    for (int k = 0; k < holes; ++k) {
      const double x = 400.0 * (i + 0.5) / holes;
      const double y = 300.0 * (k + 0.5) / holes;
      tools.Append(BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, -1.0), gp::DZ()), 6.0, 22.0).Shape());
    }
  }
  NCollection_List<TopoDS_Shape> args;
  args.Append(plate);
  BRepAlgoAPI_Cut cut;
  cut.SetArguments(args);
  cut.SetTools(tools);
  cut.Build();
  return cut.IsDone() ? cut.Shape() : plate;
}

TopLoc_Location at(double x, double y, double z) {
  gp_Trsf t;
  t.SetTranslation(gp_Vec(x, y, z));
  return TopLoc_Location(t);
}

int run(const std::vector<std::string>& args) {
  int grid = 10;
  int holes = 4;
  double offset = 0.0;
  bool force = false;
  bool noPCurves = false;
  std::string out;
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--grid" && i + 1 < args.size()) grid = std::stoi(args[++i]);
    else if (a == "--holes" && i + 1 < args.size()) holes = std::stoi(args[++i]);
    else if (a == "--offset" && i + 1 < args.size()) offset = std::stod(args[++i]);
    else if (a == "--force") force = true;
    else if (a == "--no-pcurves") noPCurves = true;
    else if (a[0] != '-' && out.empty()) out = a;
    else {
      std::cerr << "Unbekanntes Argument: " << a << "\n";
      out.clear();
      break;
    }
  }
  if (out.empty() || grid < 1 || holes < 1) {
    std::cerr << "Aufruf: sb-gen-step <ausgabe.stp> [--grid N] [--holes N] [--offset mm] [--no-pcurves] [--force]\n"
                 "  --grid N     N x N Module (Standard 10)\n"
                 "  --holes N    N x N Bohrungen je Platte (Standard 4)\n"
                 "  --offset mm  ganze Anlage vom Ursprung wegschieben (Float-Genauigkeit testen)\n"
                 "  --no-pcurves 2D-Randkurven weglassen (wie manche CAD-Exporte; testet Healing)\n"
                 "  --force      vorhandene Datei ueberschreiben\n"
                 "Zum Messen einer STEP-Datei ist sb-bench da, nicht dieses Programm.\n";
    return 2;
  }
  // Schutz vor Verwechslung mit sb-bench: niemals eine vorhandene (echte) STEP-Datei ueberschreiben.
  {
    std::error_code ec;
    if (!force && std::filesystem::exists(std::filesystem::u8path(out), ec)) {
      std::cerr << "Abbruch: " << out << " existiert bereits und wird nicht ueberschrieben.\n"
                   "Zum Messen einer STEP-Datei: sb-bench " << out << "\n"
                   "Zum Ueberschreiben einer Testdatei: --force angeben.\n";
      return 2;
    }
  }

  // OCCT-Statistikausgaben unterdruecken
  for (const occ::handle<Message_Printer>& printer : Message::DefaultMessenger()->Printers()) {
    printer->SetTraceLevel(Message_Fail);
  }

  occ::handle<TDocStd_Document> doc;
  XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", doc);
  const occ::handle<XCAFDoc_ShapeTool> shapes = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  const occ::handle<XCAFDoc_ColorTool> colors = XCAFDoc_DocumentTool::ColorTool(doc->Main());

  auto addPart = [&](const TopoDS_Shape& s, const char* name, const Quantity_Color& c) {
    const TDF_Label l = shapes->AddShape(s, false);
    TDataStd_Name::Set(l, name);
    colors->SetColor(l, c, XCAFDoc_ColorGen);
    return l;
  };

  // Prototypen - jeder nur einmal in der Datei.
  const TDF_Label plate = addPart(makePlate(holes), "Lochplatte",
                                  Quantity_Color(0.7, 0.7, 0.75, Quantity_TOC_RGB));
  const TDF_Label post = addPart(BRepPrimAPI_MakeBox(40.0, 40.0, 500.0).Shape(), "Stuetze",
                                 Quantity_Color(0.2, 0.4, 0.8, Quantity_TOC_RGB));
  const TDF_Label pipe = addPart(
      BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp::DX()), 25.0, 400.0).Shape(), "Rohr",
      Quantity_Color(0.9, 0.5, 0.1, Quantity_TOC_RGB));

  // Modul-Baugruppe: 4 Stuetzen, 1 Platte, 2 Rohre.
  const TDF_Label module = shapes->NewShape();
  TDataStd_Name::Set(module, "Modul");
  shapes->AddComponent(module, post, at(0, 0, 0));
  shapes->AddComponent(module, post, at(360, 0, 0));
  shapes->AddComponent(module, post, at(0, 260, 0));
  shapes->AddComponent(module, post, at(360, 260, 0));
  shapes->AddComponent(module, plate, at(0, 0, 500));
  shapes->AddComponent(module, pipe, at(0, 150, 300));
  shapes->AddComponent(module, pipe, at(0, 150, 700));

  // Anlage: grid x grid Module.
  const TDF_Label plant = shapes->NewShape();
  TDataStd_Name::Set(plant, "Anlage");
  for (int i = 0; i < grid; ++i) {
    for (int k = 0; k < grid; ++k) {
      shapes->AddComponent(plant, module, at(offset + i * 600.0, offset + k * 500.0, 0));
    }
  }
  shapes->UpdateAssemblies();

  STEPCAFControl_Controller::Init();
  if (noPCurves) Interface_Static::SetIVal("write.surfacecurve.mode", 0);
  STEPCAFControl_Writer writer;
  writer.SetNameMode(true);
  writer.SetColorMode(true);
  if (!writer.Transfer(doc, STEPControl_AsIs)) {
    std::cerr << "Transfer fehlgeschlagen\n";
    return 1;
  }
  if (writer.Write(out.c_str()) != IFSelect_RetDone) {
    std::cerr << "Schreiben fehlgeschlagen: " << out << "\n";
    return 1;
  }
  std::cerr << "Geschrieben: " << out << " (" << grid * grid << " Module, " << grid * grid * 7
            << " Teile-Instanzen, 3 Prototypen)\n";
  return 0;
}

}  // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
  try {
    return run(sb::utf8Args(argc, argv));
  } catch (const Standard_Failure& e) {
    std::cerr << "OCCT-Fehler: " << e.what() << "\n";
    return 1;
  }
}
#else
int main(int argc, char** argv) {
  try {
    return run(std::vector<std::string>(argv, argv + argc));
  } catch (const Standard_Failure& e) {
    std::cerr << "OCCT-Fehler: " << e.what() << "\n";
    return 1;
  }
}
#endif
