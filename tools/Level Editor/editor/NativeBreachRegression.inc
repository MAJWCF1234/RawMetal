  // The Breach tool (NativeBreachTool.inc) driven through the plan's mouse and its boxes: a ragged
  // breach dragged along a wall of # tiles, its choices as named fields, an edit in the inspector,
  // undo and redo, a drag on open floor refused, delete leaving the wall whole, save and reopen byte
  // for byte, the engine reading every step. --editing-test runs it; --breach-test runs it alone
  // (native-editor-breach-test.txt).
  void verifyBreachRegressions(bool pictures) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Breach regression: " + message);
    };
    std::ofstream report(root / L"native-editor-breach-test.txt");
    NativeDocument blank;
    blank.newCampaign("Breach");
    blank.addRoom("Yard");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    remember();
    for (int y = 1; y < 23; ++y)
      doc.paint(map, 0, 12, y, '#');
    publish();
    auto tiles = [&] {
      std::string s;
      for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
          s += doc.tile(map, 0, x, y);
      return s;
    };
    const auto before = tiles();
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
    auto named = [&](size_t line, const std::string &key) {
      for (auto &[k, v] : NativeDocument::named(doc.lines[line]))
        if (k == key)
          return v;
      return std::string("-");
    };
    SendMessageW(window, WM_COMMAND, ToolBase + BreachTool, 0);
    require(tool == BreachTool, "the Breach tool did not take");
    box(breachShapeList, BreachShapeList, 0);
    box(breachEdgeList, BreachEdgeList, 0);
    box(breachSizeList, BreachSizeList, 0);
    box(breachRubbleList, BreachRubbleList, 2);
    // Along the wall (it runs north-south at x 12): y 6 to 9.
    drag(12.5f, 6.1f, 12.5f, 8.9f);
    auto breaches = doc.records("BREACH", map);
    require(breaches.size() == 1, "a drag along the wall made " + std::to_string(breaches.size()) + " BREACH records");
    const size_t b = breaches[0];
    auto f = NativeDocument::fields(doc.lines[b]);
    require(f.size() == 8 && f[2] == "12" && f[4] == "13" && f[3] == "6" && f[5] == "9", "the breach is not on the wall's box x 12-13, y 6-9: " + doc.lines[b]);
    require(named(b, "shape") == "ragged" && named(b, "head") == "2.3" && named(b, "rubble") == "24" && named(b, "edge") == "-", "the boxes' choices are not its named fields: " + doc.lines[b]);
    require(tiles() == before, "placing a breach changed the wall's tiles (the engine cuts the hole)");
    require(validateDocument(), "the engine refused the breach");
    report << "PASS: a drag along a wall writes one ragged BREACH on the wall's box with the boxes' shape, size and rubble; the tiles stay; the engine reads it\n" << std::flush;
    if (pictures)
      captureEditorWindow(root / L".build" / L"interaction" / L"breach-tool.bmp");
    // Open floor: nothing to breach.
    const auto text = doc.text();
    drag(4.f, 4.f, 7.f, 4.f);
    require(doc.text() == text, "a breach dragged on open floor should write nothing");
    report << "PASS: a breach dragged on open floor is refused with a message\n" << std::flush;
    // The inspector: an arch.
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    selectRecord(int(doc.records("BREACH", map)[0]));
    fillFields(false);
    size_t row = 0;
    for (; row < namedKeys.size() && namedKeys[row] != "shape"; ++row) {}
    require(row < namedKeys.size(), "the inspector does not list shape");
    SetWindowTextW(fieldInputs[size_t(positionalInputs) + row], L"arch");
    apply(true);
    require(named(size_t(record), "shape") == "arch" && validateDocument(), "editing the shape to arch failed: " + doc.lines[size_t(record)]);
    report << "PASS: the inspector edits the breach's shape in place\n" << std::flush;
    const auto edited = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.records("BREACH", map).empty() && tiles() == before, "undo did not take the breach back");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == edited, "redo did not restore the breach and its edit exactly");
    report << "PASS: undo and redo step through the breach and its edit exactly\n" << std::flush;
    selectRecord(int(doc.records("BREACH", map)[0]));
    SendMessageW(window, WM_COMMAND, Remove, 0);
    require(doc.records("BREACH", map).empty() && tiles() == before, "deleting the breach changed the wall");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    report << "PASS: deleting a breach leaves the wall exactly as it was\n" << std::flush;
    auto folder = session / L"breach";
    fs::create_directories(folder);
    const auto file = folder / L"breach.rmc";
    const auto saved = doc.text();
    saveTo(file);
    NativeDocument reopened;
    reopened.open(file);
    require(reopened.text() == saved, "save/reopen changed the breach map");
    report << "PASS: save and reopen keep every byte\n" << std::flush;
    dirty = false;
  }
