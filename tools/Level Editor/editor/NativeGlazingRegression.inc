  // The Window wall and Glass tools (NativeGlazingTool.inc) driven through the plan's mouse and the
  // tool boxes, as a person uses them: a storefront dragged along a wall of # tiles, its boxes'
  // choices written as named fields, an edit in the inspector, undo and redo, delete (the wall's
  // tiles were never touched, so the wall is whole), the Glass tool's window in a wall and its thin
  // pane in an opening with the wall closed over it, save and reopen byte for byte, and the engine
  // reading every step. --editing-test runs it; --glazing-test runs it alone and writes
  // native-editor-glazing-test.txt with the plan and window pictures in .build/interaction.
  void verifyGlazingRegressions(bool pictures) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Glazing regression: " + message);
    };
    std::ofstream report(root / L"native-editor-glazing-test.txt");
    NativeDocument blank;
    blank.newCampaign("Glazing");
    blank.addRoom("Atrium");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    // A wall of # tiles across the room at row 12, with a 3 m gap (x 15-17) in it.
    remember();
    for (int x = 1; x < 23; ++x)
      doc.paint(map, 0, x, 12, x >= 15 && x <= 17 ? '.' : '#');
    publish();
    const auto rowsBefore = [&] {
      std::string s;
      for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
          s += doc.tile(map, 0, x, y);
      return s;
    };
    const auto tiles = rowsBefore();
    auto mouse = [&](UINT message, float x, float y) {
      auto v = planView();
      SendMessageW(plan, message, MK_LBUTTON, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto drag = [&](float ax, float ay, float bx, float by) {
      mouse(WM_LBUTTONDOWN, ax, ay);
      for (int i = 1; i <= 6; ++i)
        mouse(WM_MOUSEMOVE, ax + (bx - ax) * i / 6.f, ay + (by - ay) * i / 6.f);
      mouse(WM_LBUTTONUP, bx, by);
    };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    auto pick = [&](int t) { SendMessageW(window, WM_COMMAND, ToolBase + t, 0); };
    auto named = [&](size_t line, const std::string &key) {
      for (auto &[k, v] : NativeDocument::named(doc.lines[line]))
        if (k == key)
          return v;
      return std::string("-");
    };
    // ---- The Window wall tool: a storefront along the wall's west part ----
    pick(GlazingTool);
    require(tool == GlazingTool, "the Window wall tool did not take");
    require(IsWindowVisible(glazingPaneList) || !IsWindowVisible(window), "the Window wall tool shows no boxes");
    box(glazingHeightList, GlazingHeightList, 0);
    box(glazingBaysList, GlazingBaysList, 4);
    box(glazingRowsList, GlazingRowsList, 1);
    box(glazingPaneList, GlazingPaneList, 8);
    box(glazingLookList, GlazingLookList, 1);
    box(glazingDoorList, GlazingDoorList, 1);
    const size_t linesBefore = doc.lines.size();
    drag(3.2f, 12.5f, 10.8f, 12.5f);
    auto glazings = doc.records("GLAZING", map);
    require(glazings.size() == 1, "dragging along the wall made " + std::to_string(glazings.size()) + " GLAZING records");
    const size_t g = glazings[0];
    auto f = NativeDocument::fields(doc.lines[g]);
    require(f.size() == 8 && f[2] == "3" && f[3] == "12" && f[4] == "11" && f[5] == "13", "the storefront is not the wall's box x 3-11, y 12-13: " + doc.lines[g]);
    require(named(g, "cols") == "4" && named(g, "rows") == "2" && named(g, "look") == "metal" && named(g, "door") == "2" && named(g, "panes") == "cdcmcddmcxcd" &&
                named(g, "sill") == "0.45" && named(g, "head") == "2.6" && named(g, "cut") == "-",
            "the tool boxes' choices are not the record's named fields: " + doc.lines[g]);
    require(doc.format() == 2, "a storefront did not put FORMAT|2 in");
    require(rowsBefore() == tiles, "placing the storefront changed the wall's tiles (the engine cuts its own opening)");
    require(doc.lines.size() == linesBefore + 2, "a storefront should be one record (and FORMAT|2), not " + std::to_string(doc.lines.size() - linesBefore));
    require(validateDocument(), "the engine refused the storefront");
    report << "PASS: Window wall: a drag along a wall of tiles writes one GLAZING on the wall's box with the tool's bays, rows, panes, frame and doorway; the tiles are untouched; the engine reads it\n" << std::flush;
    if (pictures) {
      // Mid-drag, the plan's rubber band over the wall; then the storefront picked, its fields in the inspector.
      mouse(WM_LBUTTONDOWN, 3.2f, 6.5f);
      mouse(WM_MOUSEMOVE, 7.f, 6.5f);
      captureEditorWindow(root / L".build" / L"interaction" / L"glazing-drag.bmp");
      mouse(WM_MOUSEMOVE, 3.2f, 6.5f);
      SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 0);
      mouse(WM_LBUTTONUP, 3.2f, 6.5f);
    }
    // ---- Edit it in the inspector: three bays ----
    pick(0);
    selectRecord(int(g));
    fillFields(false);
    size_t row = 0;
    for (; row < namedKeys.size() && namedKeys[row] != "cols"; ++row) {}
    require(row < namedKeys.size(), "the inspector does not list cols");
    require(label(fieldLabels[size_t(positionalInputs) + row]).find(L"Bays") != std::wstring::npos, "the cols row is not labelled Bays from the guide");
    if (pictures)
      captureEditorWindow(root / L".build" / L"interaction" / L"glazing-inspector.bmp");
    SetWindowTextW(fieldInputs[size_t(positionalInputs) + row], L"3");
    apply(true);
    require(named(size_t(record), "cols") == "3", "editing cols in the inspector did not write cols=3: " + doc.lines[size_t(record)]);
    require(validateDocument(), "the engine refused the edited storefront");
    report << "PASS: the inspector lists the storefront's named fields, labelled from the guide, and edits cols in place\n" << std::flush;
    // ---- Undo and redo ----
    const auto edited = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(named(doc.records("GLAZING", map)[0], "cols") == "4", "undo did not take the edit back");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.records("GLAZING", map).empty() && rowsBefore() == tiles, "undo did not take the storefront back with the wall whole");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == edited, "redo did not bring the storefront and its edit back exactly");
    report << "PASS: undo takes the edit and then the storefront back, one step each; redo restores them exactly\n" << std::flush;
    // ---- Delete: nothing of the wall goes with it ----
    selectRecord(int(doc.records("GLAZING", map)[0]));
    SendMessageW(window, WM_COMMAND, Remove, 0);
    require(doc.records("GLAZING", map).empty() && rowsBefore() == tiles && doc.records("STRUCT", map).empty(), "deleting the storefront left the wall changed");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.records("GLAZING", map).size() == 1, "undo did not bring the deleted storefront back");
    report << "PASS: deleting a storefront leaves the wall exactly as it was; undo brings it back\n" << std::flush;
    // ---- The Glass tool: a window in the wall, a thin pane in the gap ----
    pick(GlassTool);
    drag(19.2f, 12.4f, 21.8f, 12.4f);
    glazings = doc.records("GLAZING", map);
    require(glazings.size() == 2, "Glass dragged along a solid wall should cut a window (a one-bay GLAZING)");
    {
      auto w = NativeDocument::fields(doc.lines[glazings[1]]);
      require(w[2] == "19" && w[4] == "22" && named(glazings[1], "sill") == "0.9" && named(glazings[1], "head") == "2.1", "the Glass tool's window is not 0.9-2.1 m in the wall's box: " + doc.lines[glazings[1]]);
    }
    drag(15.2f, 12.5f, 17.8f, 12.5f);
    auto panes = doc.records("BREAKABLE", map);
    require(panes.size() == 1, "Glass dragged across the gap should make one pane");
    auto p = NativeDocument::fields(doc.lines[panes[0]]);
    const float x1 = number(p[2], 0), y1 = number(p[3], 0), x2 = number(p[4], 0), y2 = number(p[5], 0), bottom = number(p[6], 0), top = number(p[7], 0);
    require(std::abs(x1 - 15) < .01f && std::abs(x2 - 18) < .01f && std::abs((y1 + y2) * .5f - 12.5f) < .01f && y2 - y1 < .03f && p[8] == "17",
            "the pane is not a thin sheet across the gap, centred in the wall: " + doc.lines[panes[0]]);
    require(std::abs(bottom - layerElevation()) < .01f && std::abs(top - bottom - 2.4f) < .01f, "the pane in an opening should stand from the floor to 2.4 m");
    bool header = false;
    for (auto i : doc.records("STRUCT", map)) {
      auto s = NativeDocument::fields(doc.lines[i]);
      header |= s[2] == "15" && s[4] == "18" && std::abs(number(s[6], 0) - top) < .01f && std::abs(number(s[7], 0) - creationCeiling()) < .01f && s[9] == "3";
    }
    require(header, "the wall was not closed over the pane in the gap (a wall-finish STRUCT from the pane's top to the ceiling)");
    require(rowsBefore() == tiles, "the Glass tool changed the wall's tiles");
    require(validateDocument(), "the engine refused the Glass tool's window and pane");
    report << "PASS: Glass: along a solid wall it cuts a 0.9-2.1 m window; across a gap it stands a 24 mm pane from the floor, centred in the wall, with the wall closed over it\n" << std::flush;
    // ---- Free-standing glass: a thin screen ----
    drag(4.f, 18.5f, 8.f, 18.5f);
    panes = doc.records("BREAKABLE", map);
    require(panes.size() == 2, "Glass on open floor should make a pane");
    p = NativeDocument::fields(doc.lines[panes[1]]);
    require(number(p[5], 0) - number(p[3], 0) < .03f && std::abs(number(p[7], 0) - number(p[6], 0) - 2.2f) < .01f, "free-standing glass is not a thin 2.2 m screen: " + doc.lines[panes[1]]);
    report << "PASS: Glass on open floor is a thin 2.2 m screen, never a box\n" << std::flush;
    // ---- Save and reopen ----
    auto folder = session / L"glazing";
    fs::create_directories(folder);
    const auto file = folder / L"glazing.rmc";
    const auto before = doc.text();
    saveTo(file);
    NativeDocument reopened;
    reopened.open(file);
    require(reopened.text() == before, "save/reopen changed the text of the glazing map");
    adoptDocument(std::move(reopened), file, true);
    require(doc.records("GLAZING", map).size() == 2 && named(doc.records("GLAZING", map)[0], "cols") == "3" && validateDocument(),
            "the reopened map lost its glazing or the engine refused it");
    report << "PASS: save and reopen keep every byte; the reopened map has both window walls and the engine reads it\n" << std::flush;
    dirty = false;
  }
