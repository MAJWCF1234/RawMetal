  // The Growth brush (NativeGrowthTool.inc) driven through the plan's mouse and its boxes, as a person
  // paints: ivy along a wall of # tiles (every piece on the wall's face, turned out from it), one Undo
  // for the stroke and an exact Redo, the same typed seed growing the same ivy again, creepers from the
  // ceiling and over a ledge's edge (their tops at the ceiling and the lip), grass on the floor and on
  // the ledge's top and never in the wall, wall moss low on the wall, Erase clearing only what is under
  // the brush, the engine reading it all, and save and reopen byte for byte. --editing-test runs it;
  // --growth-test runs it alone (native-editor-growth-test.txt, pictures in .build/interaction).
  void verifyGrowthRegressions(bool pictures) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Growth regression: " + message);
    };
    std::ofstream report(root / L"native-editor-growth-test.txt");
    NativeDocument blank;
    blank.newCampaign("Growth");
    blank.addRoom("Atrium");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    remember();
    // A wall of # tiles across the room at row 12; a low planter wall (a ledge) at x 4-8, y 4-4.6.
    for (int x = 1; x < 23; ++x)
      doc.paint(map, 0, x, 12, '#');
    addAuthored({"STRUCT", std::to_string(map), "4", "4", "8", "4.6", "0", "1.2", "0", "3"});
    publish();
    const float floor = layerElevation(), ceiling = creationCeiling(), below = bottomLayerElevation();
    auto mouse = [&](UINT message, float x, float y) {
      auto v = planView();
      SendMessageW(plan, message, MK_LBUTTON, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto stroke3 = [&](float ax, float ay, float bx, float by) {
      mouse(WM_LBUTTONDOWN, ax, ay);
      for (int i = 1; i <= 8; ++i)
        mouse(WM_MOUSEMOVE, ax + (bx - ax) * i / 8.f, ay + (by - ay) * i / 8.f);
      mouse(WM_LBUTTONUP, bx, by);
    };
    auto click = [&](float x, float y) { mouse(WM_LBUTTONDOWN, x, y), mouse(WM_LBUTTONUP, x, y); };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    struct Piece {
      int model;
      float x, y, base, w, d, h, yaw;
    };
    auto pieces = [&](auto keep) {
      std::vector<Piece> out;
      for (auto i : doc.records("FIXTURE", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (f.size() < 11 || !growthModel(int(number(f[2], -1))))
          continue;
        Piece p{int(number(f[2], -1)), number(f[3], 0), number(f[4], 0), number(f[5], 0) + below, number(f[6], 0), number(f[7], 0), number(f[8], 0), number(f[9], 0)};
        require(f[10] == "0", "a growth piece is solid: " + doc.lines[i]);
        if (keep(p))
          out.push_back(p);
      }
      return out;
    };
    auto all = [](const Piece &) { return true; };
    using namespace retro::model;
    SendMessageW(window, WM_COMMAND, ToolBase + GrowthTool, 0);
    require(tool == GrowthTool, "the Growth tool did not take");
    require(IsWindowVisible(growthKindList) || !IsWindowVisible(window), "the Growth tool shows no boxes");
    require(std::abs(brushRadius() - growthRadius()) < 1e-4f, "the brush ring is not the Growth brush's size");
    // ---- Ivy along the wall's north face (y 12, facing north), seed 42 ----
    box(growthKindList, GrowthKindList, GrowIvy);
    box(growthModeList, GrowthModeList, 0);
    box(growthSizeList, GrowthSizeList, 2); // 2 m
    box(growthDensityList, GrowthDensityList, 1);
    box(growthScaleList, GrowthScaleList, 1);
    SetWindowTextW(growthSeedBox, L"42");
    const auto before = doc.text();
    const size_t undo = history.size();
    stroke3(5.f, 11.f, 12.f, 11.f);
    auto ivy = pieces(all);
    require(ivy.size() >= 3, "a stroke of ivy along the wall grew " + std::to_string(ivy.size()) + " pieces");
    require(history.size() == undo + 1, "a stroke of ivy is not one Undo (" + std::to_string(history.size() - undo) + ")");
    for (const auto &p : ivy) {
      require(p.model == ivy_wall_a || p.model == ivy_wall_b || p.model == growth_vine_sheet || p.model == creeper, "the ivy brush grew model " + std::to_string(p.model));
      // On the face at y 12: its back against the wall, turned out (north, yaw pi).
      require(std::abs(p.y + p.d * .5f + .01f - 12.f) < .02f && std::abs(std::abs(p.yaw) - 3.14159f) < .01f,
              "an ivy patch is not flat on the wall's face, turned out: y " + shortNumber(p.y) + " yaw " + shortNumber(p.yaw));
      require(p.base >= floor - .01f && p.base + p.h <= ceiling + .01f && p.x - p.w * .5f >= .99f && p.x + p.w * .5f <= 23.01f,
              "an ivy patch sticks out of the wall's face");
    }
    const auto grown = doc.text();
    report << "PASS: Ivy: a stroke along a wall of tiles grows " << ivy.size() << " patches flat on its face, turned out, inside the wall's height and length; one Undo\n" << std::flush;
    if (pictures)
      captureEditorWindow(root / L".build" / L"interaction" / L"growth-ivy.bmp");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == before, "Undo did not take the whole ivy stroke back");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == grown, "Redo did not bring the ivy stroke back exactly");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    stroke3(5.f, 11.f, 12.f, 11.f);
    require(doc.text() == grown, "the same seed did not grow the same ivy");
    SetWindowTextW(growthSeedBox, L"7");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    stroke3(5.f, 11.f, 12.f, 11.f);
    require(doc.text() != grown && !pieces(all).empty(), "another seed grew the same ivy");
    report << "PASS: Undo takes the stroke back whole, Redo restores it exactly; seed 42 grows the same ivy again, seed 7 other ivy\n" << std::flush;
    // ---- Wall moss on the wall's south face (y 13, facing south): low on the wall ----
    box(growthKindList, GrowthKindList, GrowWallMoss);
    stroke3(14.f, 13.6f, 19.f, 13.6f);
    auto moss = pieces([](const Piece &p) { return p.model == growth_moss_wall; });
    require(!moss.empty(), "the wall moss brush grew nothing");
    for (const auto &p : moss)
      require(std::abs(p.y - p.d * .5f - .01f - 13.f) < .02f && std::abs(p.yaw) < .01f && p.base <= floor + 1.45f, "wall moss is not low on the wall's south face");
    report << "PASS: Wall moss: " << moss.size() << " patches low on the wall's other face, turned out from it\n" << std::flush;
    // ---- Creepers from the ceiling, and over the planter's edge ----
    box(growthKindList, GrowthKindList, GrowCreepers);
    click(6.f, 6.f);
    auto hung = pieces([](const Piece &p) { return p.model == growth_hanging_vine || p.model == ivy_strand || p.model == growth_ivy_creeper; });
    int fromCeiling = 0, overLedge = 0;
    for (const auto &p : hung) {
      fromCeiling += std::abs(p.base + p.h - ceiling) < .02f;
      overLedge += std::abs(p.base + p.h - 1.24f) < .02f && p.model == ivy_strand;
      require(p.base > floor + .05f, "a creeper reaches the floor");
    }
    require(fromCeiling >= 2 && overLedge >= 1, "creepers: " + std::to_string(fromCeiling) + " from the ceiling, " + std::to_string(overLedge) + " over the planter's lip");
    report << "PASS: Creepers: " << fromCeiling << " hang from the ceiling (tops at " << shortNumber(ceiling) << " m) and " << overLedge
           << " over the planter's lip; none reaches the floor\n" << std::flush;
    // ---- Grass: on the floor, never in the wall, and on the planter's top ----
    box(growthKindList, GrowthKindList, GrowGrass);
    box(growthDensityList, GrowthDensityList, 2);
    click(18.f, 12.f);
    click(6.f, 4.3f);
    auto grass = pieces([](const Piece &p) { return p.model != growth_hanging_vine && p.model != ivy_strand && p.model != growth_ivy_creeper && p.model != growth_moss_wall && p.model != ivy_wall_a && p.model != ivy_wall_b && p.model != growth_vine_sheet && p.model != creeper; });
    int onFloor = 0, onLedge = 0;
    for (const auto &p : grass) {
      require(!(p.y > 12.f && p.y < 13.f), "grass grew in the wall");
      onFloor += std::abs(p.base - floor) < .01f;
      onLedge += std::abs(p.base - 1.2f) < .01f && p.x > 4 && p.x < 8 && p.y > 4 && p.y < 4.6f;
    }
    require(onFloor >= 6 && onLedge >= 1, "grass: " + std::to_string(onFloor) + " on the floor, " + std::to_string(onLedge) + " on the planter");
    report << "PASS: Grass: " << onFloor << " tufts on the floor either side of the wall and none in it, " << onLedge << " on the planter's top\n" << std::flush;
    require(validateDocument(), "the engine refused the growth");
    // ---- Erase: only what is under the brush ----
    const size_t total = pieces(all).size();
    const auto ivyLeft = pieces([](const Piece &p) { return p.model == ivy_wall_a || p.model == ivy_wall_b || p.model == growth_vine_sheet || p.model == creeper; }).size();
    box(growthModeList, GrowthModeList, 1);
    const size_t erasedUndo = history.size();
    click(18.f, 12.f);
    const size_t after = pieces(all).size();
    require(after < total && history.size() == erasedUndo + 1, "Erase cleared nothing, or was not one Undo");
    require(pieces([](const Piece &p) { return p.model == ivy_wall_a || p.model == ivy_wall_b || p.model == growth_vine_sheet || p.model == creeper; }).size() == ivyLeft,
            "Erase over the grass took ivy two metres away");
    for (const auto &p : pieces(all))
      require(std::hypot(p.x - 18.f, p.y - 12.f) > growthRadius() - 1e-3f, "Erase left a piece under the brush");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(pieces(all).size() == total, "Undo did not bring the erased growth back");
    box(growthModeList, GrowthModeList, 0);
    report << "PASS: Erase clears the " << (total - after) << " pieces under the brush and nothing else; one Undo brings them back\n" << std::flush;
    // ---- Overgrown: all of it in one dab ----
    box(growthKindList, GrowthKindList, GrowMix);
    box(growthSizeList, GrowthSizeList, 4);
    SetWindowTextW(growthSeedBox, L"");
    const size_t mixBefore = pieces(all).size();
    click(18.f, 8.f);
    require(pieces(all).size() > mixBefore + 5 && validateDocument(), "Overgrown grew too little, or the engine refused it");
    report << "PASS: Overgrown: one dab grows ivy, moss, creepers and grass together (" << (pieces(all).size() - mixBefore) << " pieces); the engine reads it\n" << std::flush;
    if (pictures)
      captureEditorWindow(root / L".build" / L"interaction" / L"growth-tool.bmp");
    // ---- Save and reopen ----
    auto folder = session / L"growth";
    fs::create_directories(folder);
    const auto file = folder / L"growth.rmc";
    const auto saved = doc.text();
    saveTo(file);
    NativeDocument reopened;
    reopened.open(file);
    require(reopened.text() == saved, "save/reopen changed the growth map");
    adoptDocument(std::move(reopened), file, true);
    require(validateDocument(), "the engine refused the reopened growth map");
    report << "PASS: save and reopen keep every byte; the engine reads the reopened map\n" << std::flush;
    dirty = false;
  }
