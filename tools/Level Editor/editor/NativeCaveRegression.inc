  // The Cave tool (NativeCaveTool.inc) and the Terrain tool's Move brush, driven through the plan's mouse
  // and the tool boxes as a person digs: a map made outdoors, a cave mouth dragged in from the hillside, a
  // tunnel brushed on at depth, a cavern clicked, the wall pulled out, one Undo for each stroke and an
  // exact Redo, the carved field read through the engine's own header (air underground where the tunnel
  // runs, rock round it, the hillside open at the mouth), the land moved with the Move brush, and save
  // and reopen byte for byte. --cave-test runs it alone (native-editor-cave-test.txt); --editing-test too.
  void verifyCaveRegressions() {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Cave regression: " + message);
    };
    std::ofstream report(root / L"native-editor-cave-test.txt");
    doc.newCampaign("Caves");
    doc.addRoom("Hall");
    path.clear();
    map = layer = 0, record = -1, selectedRecords.clear(), history.clear(), future.clear(), tool = 0;
    refresh(), layout();
    autoDressingEnabled = false;
    joinMapPassage = true;
    addAdjacentMap(3);
    require(map == 1, "the new map was not chosen");
    SendMessageW(window, WM_COMMAND, Outdoors, 0);
    require(onTerrain(), "Make outdoors did not give the map land");
    fitPlan();
    auto mouse = [&](UINT message, float x, float y) {
      auto v = planView();
      SendMessageW(plan, message, MK_LBUTTON, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto stroke = [&](float ax, float ay, float bx, float by) {
      mouse(WM_LBUTTONDOWN, ax, ay);
      for (int i = 1; i <= 10; ++i)
        mouse(WM_MOUSEMOVE, ax + (bx - ax) * i / 10.f, ay + (by - ay) * i / 10.f);
      mouse(WM_LBUTTONUP, bx, by);
    };
    auto click = [&](float x, float y) { mouse(WM_LBUTTONDOWN, x, y), mouse(WM_LBUTTONUP, x, y); };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    auto [ox, oy] = mapOrigin();
    auto density = [&](float x, float y, float z) {
      const auto *shape = campaignTerrain();
      return shape ? retro::terrainDensityWithCaves(*shape, int(std::floor(ox + x)), int(std::floor(oy + y)), int(std::lround(z))) : 32;
    };
    SendMessageW(window, WM_COMMAND, ToolBase + CaveTool, 0);
    require(tool == CaveTool, "the Cave tool did not take");
    layout();
    require(IsWindowVisible(caveModeList) || !IsWindowVisible(window), "the Cave tool shows no boxes");
    // ---- A cave mouth from the west hillside in to the middle, at Deep.
    box(caveModeList, CaveModeList, CaveMouth);
    box(caveSizeList, CaveSizeList, 2);
    box(caveDepthList, CaveDepthList, 1);
    const std::string before = doc.text();
    stroke(3, 12, 12, 12);
    auto caves = doc.records("CAVE");
    require(caves.size() == 1, "a dragged cave mouth is not one CAVE record (" + std::to_string(caves.size()) + ")");
    auto mouth = caveStrokeOf(NativeDocument::fields(doc.lines[caves[0]]));
    require(mouth && mouth->op == retro::CaveCarve && mouth->z1 > mouth->z2 + 3, "the cave mouth does not slope down from the hillside: " + doc.lines[caves[0]]);
    const float top = groundAt(12, 12);
    require(density(12, 12, mouth->z2) < 0, "no air where the mouth's tunnel ends, under the land");
    require(density(12, 12, top - 1.2f) > 0, "the land over the tunnel's end is not rock");
    report << "PASS: a cave mouth dragged in from the hillside slopes down to the depth and leaves rock over it\n" << std::flush;
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == before && doc.records("CAVE").empty(), "Undo did not take the cave mouth back in one step");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.records("CAVE").size() == 1, "Redo did not bring the cave mouth back");
    report << "PASS: the mouth is one Undo, and Redo brings it back\n" << std::flush;
    // ---- A tunnel brushed north at the same depth, then a cavern clicked at its end.
    box(caveModeList, CaveModeList, CaveDig);
    const size_t had = doc.records("CAVE").size();
    stroke(12, 12, 12, 5);
    const size_t tunnel = doc.records("CAVE").size() - had;
    require(tunnel >= 3, "a brushed tunnel laid " + std::to_string(tunnel) + " pieces");
    const float axis = mouth->z2;
    require(density(12, 7, axis) < 0 && density(12, 8.5f, axis) < 0, "the brushed tunnel is not open at its depth");
    require(density(16.5f, 7, axis) > 0, "rock beside the tunnel was dug away");
    box(caveSizeList, CaveSizeList, 3);
    click(12, 5);
    require(doc.records("CAVE").size() == had + tunnel + 1, "a click did not dig one cavern");
    report << "PASS: a brushed tunnel stays at its depth with rock beside it; a click digs a cavern\n" << std::flush;
    // ---- Pull the cavern's east wall out.
    box(caveModeList, CaveModeList, CaveMoveWall);
    const float wallBefore = density(15.5f, 5, axis);
    stroke(14, 5, 16, 5);
    auto last = caveStrokeOf(NativeDocument::fields(doc.lines[doc.records("CAVE").back()]));
    require(last && last->op == retro::CaveMove, "Move the wall wrote no move");
    require(density(15.5f, 5, axis) < wallBefore, "pulling the wall out left the rock where it was");
    report << "PASS: Move the wall pulls the cavern's wall after the hand (one CAVE move record)\n" << std::flush;
    // ---- Smooth and flatten: one record a dab, both read back.
    box(caveModeList, CaveModeList, CaveSmoothWall);
    stroke(11, 6, 13, 6);
    box(caveModeList, CaveModeList, CaveFlattenFloor);
    stroke(11, 5, 13, 5);
    int smooth = 0, flat = 0;
    for (auto i : doc.records("CAVE"))
      if (auto c = caveStrokeOf(NativeDocument::fields(doc.lines[i])))
        smooth += c->op == retro::CaveSmooth, flat += c->op == retro::CaveFlatten;
    require(smooth >= 1 && flat >= 1, "Smooth or Flatten the floor wrote nothing");
    report << "PASS: Smooth and Flatten the floor write their strokes\n" << std::flush;
    // ---- Fill back the cavern: one stroke, one Undo.
    box(caveModeList, CaveModeList, CaveFillBack);
    const std::string dug = doc.text();
    click(12, 5);
    require(density(12, 5, axis) > 0, "Fill back left the cavern open");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == dug && density(12, 5, axis) < 0, "Undo did not take the fill back");
    report << "PASS: Fill back closes the cavern; Undo opens it again\n" << std::flush;
    // ---- The Terrain tool's Move brush: the hill under the press follows the pointer.
    SendMessageW(window, WM_COMMAND, ToolBase + TerrainTool, 0);
    box(terrainModeList, TerrainModeList, TerrainMove);
    box(terrainSizeList, TerrainSizeList, 3);
    box(terrainModeList, TerrainModeList, TerrainRaise);
    box(terrainStrengthList, TerrainStrengthList, 3);
    for (int k = 0; k < 3; ++k)
      click(19.5f, 19.5f);
    const float hill = groundAt(19.5f, 19.5f), low = groundAt(15.5f, 19.5f);
    require(hill > low + .8f, "Raise made no hill to move (" + shortNumber(hill) + " over " + shortNumber(low) + ")");
    box(terrainModeList, TerrainModeList, TerrainMove);
    stroke(19.5f, 19.5f, 15.5f, 19.5f);
    require(groundAt(15.5f, 19.5f) > low + .6f, "the Move brush did not carry the hill with the pointer (" + shortNumber(groundAt(15.5f, 19.5f)) + " where it was " + shortNumber(low) + ")");
    report << "PASS: the Terrain tool's Move brush carries the ground with the pointer\n" << std::flush;
    // ---- Save and reopen byte for byte; the engine's parser reads every CAVE record.
    auto file = root / L".build" / L"interaction" / L"cave-test.rmc";
    fs::create_directories(file.parent_path());
    doc.save(file);
    NativeDocument again;
    again.open(file);
    require(again.text() == doc.text(), "save and reopen changed the document");
    int records = 0;
    for (auto i : again.records("CAVE"))
      records += caveStrokeOf(NativeDocument::fields(again.lines[i])).has_value();
    require(records == int(doc.records("CAVE").size()) && records >= 8, "the reopened file lost CAVE records");
    report << "PASS: save and reopen byte for byte, every CAVE record read back (" << records << ")\n" << std::flush;
    tool = 0;
  }
