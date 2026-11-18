  // Stair and balcony finishes (--editing-test, --finish-test; NativeFinishMethods.inc):
  // the inspector's Part and Finish lists on a picked stair and under the balcony tool,
  // the Paint tool on a tread, a riser and a balcony deck in the 3D view and on the plan,
  // undo/redo, save/reopen, and the engine reading every finish.
  void verifyFinishes() {
    auto require = [](bool okay, const std::string& why) { if (!okay) throw std::runtime_error("Finish regression: " + why); };
    std::ofstream report(root / L"native-editor-finish-test.txt");
    NativeDocument blank;
    blank.newCampaign("Finishes");
    blank.addRoom("Room");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    // A gallery at 3 m (a deck along x 14-20, y 10-12) and a stair climbing to it.
    remember();
    doc.add({"LAYER", "0", "1", "Gallery", "3", "0.3"});
    for (int y = 0; y < 24; ++y)
      doc.add({"ROW", "0", "1", std::to_string(y), y >= 10 && y <= 12 ? std::string(14, '.') + std::string(6, '=') + std::string(4, '.') : std::string(24, '.')});
    doc.addOnLayer({"STAIR", "0", "6", "10", "12", "12", "0", "3", "12", "0", "1", "0", "0.12", "1"}, 0);
    publish();
    const auto plainText = doc.text();
    require(doc.format() == 1, "a new map should start as format 1");
    auto stairLine = [&] { return doc.lines[doc.records("STAIR", 0).back()]; };
    auto layerLine = [&] { return doc.lines[size_t(layerRecord(1))]; };
    auto send = [&](HWND list, int id, LRESULT data) {
      for (LRESULT i = 0; i < SendMessageW(list, CB_GETCOUNT, 0, 0); ++i)
        if (SendMessageW(list, CB_GETITEMDATA, i, 0) == data || (list == finishPartList && i == data)) {
          SendMessageW(list, CB_SETCURSEL, i, 0);
          SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
          return true;
        }
      return false;
    };
    // ---- The inspector: pick the stair, choose a part, choose a finish.
    selectRecord(int(doc.records("STAIR", 0).back()));
    layout();
    require(finishSelection() && shownControl(finishPartList) && shownControl(materialList), "a picked stair shows no Part and Finish lists");
    captureEditorWindow(root / L".build" / L"interaction" / L"finish-inspector.bmp");
    require(send(finishPartList, FinishPartList, 0) && send(materialList, MaterialList, 46), "the Finish list has no material 46");
    require(doc.format() == 2 && stairLine().ends_with("|tread=46"), "choosing a tread finish did not write tread=46 and FORMAT|2: " + stairLine());
    require(finishSelection(), "the picked stair was lost when FORMAT|2 went in above it");
    send(finishPartList, FinishPartList, 1);
    send(materialList, MaterialList, 15);
    send(finishPartList, FinishPartList, 2);
    send(materialList, MaterialList, 18);
    require(stairLine().ends_with("|tread=46|riser=15|stringer=18"), "riser and stringer finishes went wrong: " + stairLine());
    require(validateDocument(), "the engine refused the stair finishes");
    report << "PASS: a picked stair's Part and Finish lists write tread=, riser= and stringer= (FORMAT|2 put in), and the engine reads them\n" << std::flush;
    // ---- Undo and redo.
    const auto dressed = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(stairLine().ends_with("|riser=15"), "undo did not take back the stringer finish alone: " + stairLine());
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == dressed, "redo did not bring the stringer finish back exactly");
    report << "PASS: each finish is one Ctrl+Z\n" << std::flush;
    // ---- The balcony tool: Part and Finish write the floor's LAYER.
    int balcony = -1;
    for (int i = 0; i < ToolCount; ++i)
      if (std::wstring(toolInfo[i].name) == L"Balcony") balcony = i;
    require(balcony >= 0, "no Balcony tool");
    selectRecord(-1);
    layer = 1;
    SendMessageW(window, WM_COMMAND, ToolBase + balcony, 0);
    layer = 1;
    layout();
    require(balconyTool() && shownControl(finishPartList), "the balcony tool shows no Part list");
    send(finishPartList, FinishPartList, 0);
    send(materialList, MaterialList, 24);
    send(finishPartList, FinishPartList, 1);
    send(materialList, MaterialList, 12);
    require(layerLine().ends_with("|deck=24|edge=12"), "the balcony tool's finishes did not reach the LAYER: " + layerLine());
    report << "PASS: the balcony tool's Part and Finish lists write the floor's deck= and edge=\n" << std::flush;
    // ---- The Paint tool in the 3D view: a ray onto a tread, onto a riser, onto the deck.
    layer = 0;
    SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
    choosePaint({"mat-44", materialName(44), 44, 0});
    remember();
    // Straight down onto the 6th step (x 8.5-9, top 1.5 m).
    paintRay(8.75f, 11, 3.4f, 0, 0, -1);
    require(stairLine().find("|tread=44") != std::string::npos, "a 3D paint click on a tread did not paint the tread: " + stairLine());
    // Level, from the low side, into the front of the 3rd step (top .75 m) at .4 m.
    choosePaint({"mat-31", materialName(31), 31, 0});
    remember();
    paintRay(3, 11, .4f, 1, 0, 0);
    require(stairLine().find("|riser=31") != std::string::npos, "a 3D paint click on a step front did not paint the riser: " + stairLine());
    // Down onto the gallery deck while the ground floor is the one edited.
    choosePaint({"mat-47", materialName(47), 47, 0});
    remember();
    const auto groundStructs = doc.records("STRUCT", 0).size();
    paintRay(17, 11, 3.4f, 0, 0, -1);
    require(layerLine().find("|deck=47") != std::string::npos, "a 3D paint click on a balcony did not paint its deck: " + layerLine());
    require(doc.records("STRUCT", 0).size() == groundStructs, "painting the balcony laid a floor on the ground floor under it");
    // Under the deck, looking up: the fascia.
    choosePaint({"mat-32", materialName(32), 32, 0});
    remember();
    paintRay(17, 11, 1.5f, 0, 0, 1);
    require(layerLine().find("|edge=32") != std::string::npos, "a 3D paint click under a balcony did not paint its edge: " + layerLine());
    // Alt+click takes a finish up.
    paintFinishAt(8.75f, 11, 1.5f, true);
    require(paintChoice.value == 44, "picking up a tread's paint gave " + std::to_string(paintChoice.value));
    // On the plan, a click on the stair paints its treads.
    choosePaint({"mat-45", materialName(45), 45, 0});
    remember();
    paintSpot(9.2f, 11, -1000.f, 0, false);
    publish();
    require(stairLine().find("|tread=45") != std::string::npos, "a plan paint click on a stair did not paint its treads: " + stairLine());
    require(validateDocument(), "the engine refused the painted finishes");
    report << "PASS: the Paint tool paints a tread, a riser, a deck and its edge in 3D, the treads on the plan, and Alt takes a finish up\n" << std::flush;
    // ---- A room-finish paint is refused on a stair, and leaves the map's surfaces alone.
    choosePaint({"surf-3", L"Clinic tile", 3, parts::Finish});
    const auto beforeSurface = doc.text();
    paintRay(8.75f, 11, 3.4f, 0, 0, -1);
    require(doc.text() == beforeSurface, "a room finish painted on a stair changed the map");
    report << "PASS: a room finish on a stair is refused without touching the map's surfaces\n" << std::flush;
    // ---- Save and reopen keep every byte; an old map is untouched.
    auto folder = session / L"finishes";
    fs::create_directories(folder);
    const auto file = folder / L"finishes.rmc";
    const auto before = doc.text();
    saveTo(file);
    NativeDocument reopened;
    reopened.open(file);
    require(reopened.text() == before, "save/reopen changed a map with finishes");
    require(plainText.find("tread=") == std::string::npos && plainText.find("FORMAT|") == std::string::npos, "a stair without finishes wrote finish fields");
    report << "PASS: save/reopen is exact; a map without finishes is written as before\n" << std::flush;
    captureEditorWindow(root / L".build" / L"interaction" / L"finish-after.bmp");
  }
