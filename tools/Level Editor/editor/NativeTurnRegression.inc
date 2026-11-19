  // ---- Material turns (NativeMaterialTurn.inc), driven as the artist drives them: a room's floor
  // painted with planks, R turning the stroke just painted (one Undo with the paint), the brush
  // turned before painting a block, the Turn box on a picked block, stair and storey, the fine
  // angle under Advanced, a ceiling's room finish, the Flooring tool laying turned and R turning
  // what it laid; then saved and opened again byte for byte and passed by the engine. With
  // pictures (--turn-test), the engine draws the plank floor as laid and turned a quarter from
  // above (.build/interaction/turn-*.bmp) and the planks must run the other way.
  // native-editor-turn-test.txt; --editing-test runs it without the pictures.
  void verifyMaterialTurns(bool pictures) {
    std::ofstream report(root / L"native-editor-turn-test.txt");
    auto pass = [&](const std::string &what) { report << "PASS: " << what << '\n' << std::flush; };
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Turn regression: " + message);
    };
    NativeDocument blank;
    blank.newCampaign("Turns");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    const float savedSnap = snap;
    snap = .25f;
    paintTurn = 0;
    auto pick = [&](int t) { SendMessageW(window, WM_COMMAND, ToolBase + t, 0); };
    auto key = [&](WPARAM k) { MSG m{plan, WM_KEYDOWN, k, 0}; shortcut(m); };
    auto planks = [&] {
      std::vector<size_t> out;
      for (auto i : doc.records("STRUCT", map))
        if (NativeDocument::fields(doc.lines[i]).size() > 9 && NativeDocument::fields(doc.lines[i])[9] == "51") out.push_back(i);
      return out;
    };
    // 1. Paint the room's floor with planks, then R: the stroke just painted turns, one Undo with it.
    pick(PaintTool);
    choosePaint({"mat-51", L"Plank floor", 51, 0});
    const auto unpainted = doc.text();
    pickZ = layerElevation(), pickSurface = 3;
    brush = 2; // the whole room
    paintGesture(8.5f, 8.5f, 8.5f, 8.5f);
    require(!planks().empty(), "painting the room's floor laid no plank floor");
    for (auto i : planks()) require(namedValue(doc.lines[i], "turn").empty(), "a floor painted as laid has a turn");
    key('R');
    require(paintTurn == 90, "R did not turn the paint a quarter");
    for (auto i : planks()) require(namedValue(doc.lines[i], "turn") == "90", "R did not turn the floor just painted");
    const auto turnedFloor = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == unpainted, "one Undo does not take back the paint and its turn together");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == turnedFloor, "Redo does not bring the turned floor back");
    pass("the room's floor painted with planks, R turns the stroke just painted a quarter; one Undo takes paint and turn back, Redo brings them");
    // 2. The brush turned first: a block painted with it takes its turn; Shift+R turns it back.
    pick(20);
    std::vector<std::string> block{"STRUCT", std::to_string(map), "15", "4", "18", "6", "0", "1", "0", "15"};
    addAuthored(block);
    publish();
    pick(PaintTool);
    pickZ = .5f, pickSurface = 0;
    paintGesture(16.5f, 5.f, 16.5f, 5.f);
    auto blockAt = [&] { for (auto i : doc.records("STRUCT", map)) { auto f = NativeDocument::fields(doc.lines[i]); if (f[2] == "15" && f[3] == "4") return i; } return size_t(-1); };
    require(blockAt() != size_t(-1) && NativeDocument::fields(doc.lines[blockAt()])[9] == "51" && namedValue(doc.lines[blockAt()], "turn") == "90",
            "a block painted with the turned brush did not take its material and turn");
    lastPainted.clear();
    turnPaint(-90.f);
    require(paintTurn == 0, "Shift+R did not turn the paint back");
    pickZ = .5f, pickSurface = 0;
    paintGesture(16.5f, 5.f, 16.5f, 5.f);
    require(namedValue(doc.lines[blockAt()], "turn").empty(), "painting as laid did not take the turn off");
    pass("a block painted with the turned brush takes its turn; Shift+R turns the brush back and painting as laid takes the turn off");
    // 3. The Turn box on a picked block, a stair and a storey; the fine angle under Advanced.
    pick(0);
    selectRecord(int(blockAt()));
    require(turnShown() && turnRecord() == record, "the Turn box is not shown for a picked block");
    SendMessageW(turnList, CB_SETCURSEL, 2, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(TurnList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(turnList));
    require(namedValue(doc.lines[blockAt()], "turn") == "180", "the Turn box did not turn the picked block half round");
    advanced = true;
    SetWindowTextW(turnFineBox, L"15");
    applyFineTurn();
    advanced = false;
    require(namedValue(doc.lines[blockAt()], "turn") == "15", "the fine angle under Advanced did not take");
    addAuthored({"STAIR", std::to_string(map), "4", "16", "6", "20", "0", "2", "10", "1", "1", "", "", "", "tread=51"});
    publish();
    selectRecord(int(doc.records("STAIR", map).back()));
    chooseTurn(90);
    require(namedValue(doc.lines[doc.records("STAIR", map).back()], "treadturn") == "90", "the Turn box did not turn a stair's step tops");
    for (auto i : doc.records("LAYER", map)) {
      selectRecord(int(i));
      break;
    }
    chooseTurn(270);
    require(namedValue(doc.lines[doc.records("LAYER", map).front()], "deckturn") == "270", "the Turn box did not turn a storey's balcony floor");
    pass("the Turn box turns a picked block, a stair's step tops and a storey's balconies; Advanced takes any angle");
    // 4. A ceiling's room finish painted with the turned brush: the map's ceilingturn.
    pick(PaintTool);
    choosePaint({"surf-8", L"Mine timber", 8, parts::Finish});
    paintTurn = 90;
    pickZ = ceilingHeight(), pickSurface = 4;
    paintGesture(10.f, 10.f, 10.f, 10.f);
    require(!doc.records("SURFACE", map).empty() && namedValue(doc.lines[doc.records("SURFACE", map).front()], "ceilingturn") == "90",
            "the ceilings painted with the turned brush have no ceilingturn");
    pass("ceilings painted with a room finish and the turned brush: SURFACE ceilingturn=90");
    // 5. The Flooring tool lays turned; R turns what it just laid.
    pick(FlooringTool);
    paintTurn = 90;
    {
      auto v = planView();
      auto at = [&](float x, float y) { return MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))); };
      hoverInside = true;
      SendMessageW(plan, WM_LBUTTONDOWN, MK_LBUTTON, at(12, 14));
      SendMessageW(plan, WM_MOUSEMOVE, MK_LBUTTON, at(14, 15));
      SendMessageW(plan, WM_MOUSEMOVE, MK_LBUTTON, at(16, 17));
      SendMessageW(plan, WM_LBUTTONUP, MK_LBUTTON, at(16, 17));
    }
    auto laid = [&] { auto all = doc.records("STRUCT", map); return all.empty() ? std::string() : doc.lines[all.back()]; };
    require(namedValue(laid(), "turn") == "90", "the Flooring tool did not lay its floor turned");
    key('R');
    require(namedValue(laid(), "turn") == "180", "R did not turn the flooring just laid");
    pass("the Flooring tool lays its floor turned; R turns the floor just laid");
    // 6. Saved and opened again byte for byte; the engine reads it.
    const auto file = root / L".build" / L"turns-saved.rmc";
    fs::create_directories(file.parent_path());
    doc.save(file);
    NativeDocument again;
    again.open(file);
    require(again.text() == doc.text(), "the turned map did not open again byte for byte");
    require(doc.format() >= 2, "a map with turns is not FORMAT|2");
    require(validateDocument(), "the engine refused the turned map");
    pass("saved, opened again byte for byte, and passed by the engine");
    if (pictures)
      turnPictures(report);
    snap = savedSnap;
    paintTurn = 0;
    pick(0);
    dirty = false;
  }
  // The engine's picture of a plank floor from above, as laid and turned a quarter. The planks
  // must run the other way: the picture changes more across x than y on one, the opposite on the other.
  void turnPictures(std::ofstream &report) {
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    NativeDocument blank;
    blank.newCampaign("Turned planks");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0); // Select: the Turn box turns the picked floor
    addAuthored({"STRUCT", std::to_string(map), "7", "7", "17", "17", "0", "0.03", "0", "51"});
    publish();
    auto lean = [&](const fs::path &ppm, int &w, int &h) {
      std::ifstream in(ppm, std::ios::binary);
      std::string magic;
      int depth = 0;
      if (!(in >> magic >> w >> h >> depth)) return 0.0;
      in.get();
      std::vector<unsigned char> rgb(size_t(w) * size_t(h) * 3);
      in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()));
      auto l = [&](int x, int y) { const auto *p = &rgb[(size_t(y) * size_t(w) + size_t(x)) * 3]; return .2126 * p[0] + .7152 * p[1] + .0722 * p[2]; };
      double gx = 0, gy = 0;
      for (int y = h / 4; y < h * 3 / 4; ++y)
        for (int x = w / 4; x < w * 3 / 4; ++x) gx += std::abs(l(x + 1, y) - l(x, y)), gy += std::abs(l(x, y + 1) - l(x, y));
      return gx > 0 && gy > 0 ? std::log(gx / gy) : 0.0;
    };
    auto shoot = [&](const std::wstring &name) {
      const auto ppm = ghostEngine(name, 12, 12, 4.2f, 90.f, -89.f); // under the 4.5 m ceiling, looking down
      if (ppm.empty()) throw std::runtime_error("Turn regression: the engine drew no picture (" + utf8(name) + ")");
      windowPicture(folder / (name + L".bmp"), ppm);
      return ppm;
    };
    int w = 0, h = 0;
    const double before = lean(shoot(L"turn-before"), w, h);
    selectRecord(int(doc.records("STRUCT", map).back()));
    chooseTurn(90);
    if (namedValue(doc.lines[doc.records("STRUCT", map).back()], "turn") != "90")
      throw std::runtime_error("Turn regression: the Turn box did not turn the floor for the picture");
    const double after = lean(shoot(L"turn-after"), w, h);
    report << "turn pictures: " << w << "x" << h << " engine frames; lean as laid " << before << ", turned " << after << '\n';
    if (!(before * after < 0 && std::abs(before) > .1 && std::abs(after) > .1))
      throw std::runtime_error("Turn regression: the turned planks do not run the other way in the engine's picture (lean " + std::to_string(before) + " against " + std::to_string(after) + ")");
    report << "PASS: the engine draws the turned planks running the other way (.build/interaction/turn-before.bmp, turn-after.bmp)\n";
  }
  // ---- Keyboard nudges (NativeNudge.inc): the arrow keys and Page Up / Down on a picked desk on
  // the plan and in the 3D view (the arrows following the camera), the fine and big steps, the
  // floor stopping a lowering once, a held key's repeats one Undo, each press otherwise one, a
  // footing floor taking what stands on it, nothing picked keeping the old keys; saved and opened
  // again. native-editor-nudge-test.txt; --editing-test runs it.
  void verifyNudges() {
    std::ofstream report(root / L"native-editor-nudge-test.txt");
    auto pass = [&](const std::string &what) { report << "PASS: " << what << '\n' << std::flush; };
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Nudge regression: " + message);
    };
    NativeDocument blank;
    blank.newCampaign("Nudges");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    const float savedSnap = snap;
    snap = .25f;
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    addAuthored({"FIXTURE", std::to_string(map), "24", "8", "8", "0", "1.4", "0.7", "0.75", "0", "1"});
    publish();
    const size_t desk = doc.records("FIXTURE", map).back();
    selectRecord(int(desk));
    auto field = [&](size_t k) { return number(NativeDocument::fields(doc.lines[size_t(record)])[k], -999); };
    auto press = [&](WPARAM k, bool repeat = false) { MSG m{plan, WM_KEYDOWN, k, LPARAM(repeat ? 0x40000000 : 0)}; return shortcut(m); };
    const auto start = doc.text();
    const size_t history0 = history.size();
    pointerIn3D = false, hoverInside = true;
    press(VK_RIGHT);
    require(std::abs(field(3) - 8.25f) < 1e-4f, "Right did not move the desk a grid step east on the plan");
    press(VK_UP);
    require(std::abs(field(4) - 7.75f) < 1e-4f, "Up did not move the desk north on the plan");
    press(VK_PRIOR);
    require(std::abs(field(5) - .25f) < 1e-4f, "Page Up did not raise the desk a step");
    require(notice.find(L"up") != std::wstring::npos && notice.find(L"Now at") != std::wstring::npos, "the status line does not say where it went in plain words");
    require(history.size() == history0 + 3, "each press is not one Undo");
    pass("on the plan: Right east, Up north, Page Up up, a grid step each, one Undo each, the status line saying where it is");
    // The floor stops a lowering once; pressed again it sinks.
    nudgeKey(MSG{plan, WM_KEYDOWN, VK_PRIOR, 0}, false, true); // up 1 cm: 0.26
    require(std::abs(field(5) - .26f) < 1e-4f, "Shift did not step a centimetre");
    press(VK_NEXT);
    require(std::abs(field(5) - .01f) < 1e-4f, "Page Down did not lower the desk a step");
    press(VK_NEXT);
    require(std::abs(field(5)) < 1e-4f && notice.find(L"stopped at the floor") != std::wstring::npos, "lowering did not stop at the floor");
    press(VK_NEXT);
    require(std::abs(field(5) + .25f) < 1e-4f, "lowering again from the floor did not sink the desk");
    pass("Shift steps a centimetre; the floor stops a lowering once, and the next press sinks it on purpose");
    nudgeKey(MSG{plan, WM_KEYDOWN, VK_LEFT, 0}, true, false);
    require(std::abs(field(3) - 7.25f) < 1e-4f, "Ctrl did not step a metre");
    // A held key: its repeats are one Undo.
    const size_t held = history.size();
    press(VK_DOWN), press(VK_DOWN, true), press(VK_DOWN, true);
    require(std::abs(field(4) - 8.5f) < 1e-4f && history.size() == held + 1, "a held key's repeats are not one Undo");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    selectRecord(int(desk));
    require(std::abs(field(4) - 7.75f) < 1e-4f, "one Undo did not take back the held key's moves together");
    pass("Ctrl steps a metre; a held key's repeats are one Undo");
    // In 3D the arrows follow the camera: Up moves it away from you on screen.
    pointerIn3D = true, hoverInside = false;
    channelPut(control.parent_path() / L"film-status.txt", "VIEW 4 8 1.7 90 0 0\n"); // looking toward +y
    float y0 = field(4), x0 = field(3);
    press(VK_UP);
    require(std::abs(field(4) - (y0 + .25f)) < 1e-4f && std::abs(field(3) - x0) < 1e-4f, "in 3D, Up did not move the desk away from the camera (+y)");
    press(VK_RIGHT);
    require(std::abs(field(3) - (x0 - .25f)) < 1e-4f, "in 3D, Right did not move the desk to the camera's right (-x when looking +y)");
    channelPut(control.parent_path() / L"film-status.txt", "VIEW 4 8 1.7 200 0 0\n"); // nearest axis -x
    x0 = field(3);
    press(VK_UP);
    require(std::abs(field(3) - (x0 - .25f)) < 1e-4f, "in 3D, Up did not follow the axis nearest the camera's facing");
    pointerIn3D = false;
    pass("in 3D the arrows go along the map axis nearest the camera: Up away from you, Right to your right");
    // A footing floor takes what stands on it.
    addAuthored({"STRUCT", std::to_string(map), "12", "12", "16", "16", "0", "0.1", "0", "51", "footing=pad", "snap=fixed"});
    doc.ensureFormat2();
    publish();
    addAuthored({"FIXTURE", std::to_string(map), "24", "14", "14", "0.1", "1.4", "0.7", "0.75", "0", "1"});
    publish();
    size_t footing = size_t(-1);
    for (auto i : doc.records("STRUCT", map))
      if (footingOf(doc.lines[i])) footing = i;
    require(footing != size_t(-1), "no footing to nudge");
    selectRecord(int(footing));
    press(VK_RIGHT);
    bool carried = false;
    for (auto i : doc.records("FIXTURE", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      carried |= std::abs(number(f[3], 0) - 14.25f) < 1e-4f && std::abs(number(f[4], 0) - 14) < 1e-4f;
    }
    require(carried, "the footing floor moved without the desk standing on it");
    pass("a footing floor nudged takes what stands on it along");
    // Nothing picked: the arrows keep their old work (nothing moves).
    selectRecord(-1);
    const auto unpicked = doc.text();
    press(VK_LEFT), press(VK_PRIOR);
    require(doc.text() == unpicked, "with nothing picked the arrows changed the map");
    // Saved and opened again.
    const auto file = root / L".build" / L"nudges-saved.rmc";
    fs::create_directories(file.parent_path());
    doc.save(file);
    NativeDocument again;
    again.open(file);
    require(again.text() == doc.text() && doc.text() != start, "the nudged map did not open again byte for byte");
    require(validateDocument(), "the engine refused the nudged map");
    pass("with nothing picked nothing moves; saved and opened again byte for byte, passed by the engine");
    snap = savedSnap;
    dirty = false;
  }
