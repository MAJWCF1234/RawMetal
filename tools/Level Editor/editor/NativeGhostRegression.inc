  // ---- Ghosts against what is placed (NativeGhost.inc), driven through the plan's mouse and the
  // 3D view's hover and pick rays as a person drives them. For every placing tool: the ghost
  // shown with the pointer where the click (or the drag's end) will be, then the click or
  // release; the records placed must be the ghost's, field for field (positions within 1.5 mm,
  // turns within 0.1 degree), and their outlines and heights the ghost's. No ghost is left
  // after a placement, Undo, or a change of tool; a drag held while the tool changes places
  // nothing. In 3D: on the floor, on the land, in a footing room, on an upper floor, in a cave.
  // --ghost-test runs it alone with pictures (.build/interaction/ghost-*.bmp) and writes
  // native-editor-ghost-test.txt; --editing-test runs it without the pictures.
  bool ghostLegacyDraw = false; // pictures only: the plan's ghost as v1.5.6 drew it (by hand, at the pointer)
  void verifyGhosts(bool pictures) {
    std::ofstream report(root / L"native-editor-ghost-test.txt");
    auto pass = [&](const std::string &what) { report << "PASS: " << what << '\n' << std::flush; };
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Ghost regression: " + message);
    };
    NativeDocument blank;
    blank.newCampaign("Ghosts");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    const float savedSnap = snap;
    const bool savedSmart = smartSnap;
    snap = .25f, smartSnap = true;
    // A wall across the hall at y 12 with a gap at x 10-11 (the Door tool's), as tiles.
    for (int x = 1; x <= 22; ++x)
      if (x != 10 && x != 11)
        doc.paint(map, layer, x, 12, '#');
    publish();
    auto mouse = [&](UINT message, float x, float y, WPARAM keys = MK_LBUTTON) {
      auto v = planView();
      SendMessageW(plan, message, keys, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto pick = [&](int t) { SendMessageW(window, WM_COMMAND, ToolBase + t, 0); };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    // The lines a step added or changed (the editor's own bookkeeping left out), as ghostOf finds them.
    auto madeBy = [&](const std::vector<std::string> &before) { return newLines(before, doc.lines); };
    // Field for field: numbers within 1.5 mm (and so within 0.1 degree for a turn in radians), words equal.
    auto same = [&](const std::string &name, const std::vector<std::string> &ghost, const std::vector<std::string> &placed) {
      if (ghost.empty() && placed.empty()) { // the tool refuses here: the ghost rightly shows nothing
        report << "NOTE: " << name << ": the tool refuses this spot; no ghost, and nothing placed\n";
        return;
      }
      require(!ghost.empty(), name + ": no ghost was shown");
      require(!placed.empty(), name + ": nothing was placed");
      require(ghost.size() == placed.size(), name + ": the ghost shows " + std::to_string(ghost.size()) + " records, " + std::to_string(placed.size()) + " were placed");
      for (size_t i = 0; i < ghost.size(); ++i) {
        auto g = NativeDocument::fields(ghost[i]), p = NativeDocument::fields(placed[i]);
        require(g.size() == p.size() && g[0] == p[0], name + ": the ghost is a " + g[0] + ", a " + p[0] + " was placed");
        for (size_t k = 0; k < g.size(); ++k) {
          char *ge = nullptr, *pe = nullptr;
          const double gv = std::strtod(g[k].c_str(), &ge), pv = std::strtod(p[k].c_str(), &pe);
          const bool numbers = !g[k].empty() && !p[k].empty() && ge && pe && !*ge && !*pe;
          require(numbers ? std::abs(gv - pv) <= .0015 : g[k] == p[k],
                  name + ": field " + std::to_string(k) + " of the " + g[0] + " is " + g[k] + " in the ghost, " + p[k] + " placed");
        }
        require(NativeDocument::named(ghost[i]) == NativeDocument::named(placed[i]), name + ": the named fields differ");
        auto gs = ghostShapeOf(ghost[i]), ps = ghostShapeOf(placed[i]);
        require(gs.has_value() == ps.has_value(), name + ": one outline is missing");
        if (gs) {
          require(std::abs(gs->bottom - ps->bottom) <= .0015f && std::abs(gs->top - ps->top) <= .0015f,
                  name + ": the ghost stands at " + std::to_string(gs->bottom) + ", the thing at " + std::to_string(ps->bottom));
          require(gs->points.size() == ps->points.size(), name + ": the outlines differ");
          for (size_t k = 0; k < gs->points.size(); ++k)
            require(std::hypot(gs->points[k].first - ps->points[k].first, gs->points[k].second - ps->points[k].second) <= .0015f, name + ": the outlines differ");
        }
      }
    };
    struct Case {
      const char *name;
      int tool;
      float ax, ay, bx, by; // a click when a == b
      std::function<void()> setup;
    };
    int placedCount = 0;
    auto run = [&](const Case &c) {
      pick(c.tool);
      if (c.setup) c.setup();
      std::vector<std::string> ghost;
      const bool click = c.ax == c.bx && c.ay == c.by;
      // Away first: a placement just made hides its ghost until the pointer moves on.
      hoverInside = true;
      mouse(WM_MOUSEMOVE, c.bx + .6f, c.by + .6f, 0);
      if (click) {
        mouse(WM_MOUSEMOVE, c.bx, c.by, 0);
        ghost = ghostLines;
      } else {
        mouse(WM_MOUSEMOVE, c.ax, c.ay, 0);
        mouse(WM_LBUTTONDOWN, c.ax, c.ay);
        for (int i = 1; i <= 5; ++i)
          mouse(WM_MOUSEMOVE, c.ax + (c.bx - c.ax) * i / 5.f, c.ay + (c.by - c.ay) * i / 5.f);
        ghost = ghostLines;
      }
      const bool drawable = std::any_of(ghost.begin(), ghost.end(), [&](const std::string &line) { return ghostShapeOf(line).has_value(); });
      require(!drawable || !ghostGuideList.empty(), std::string(c.name) + ": the 3D view was not given the ghost");
      const auto before = doc.lines;
      if (click) mouse(WM_LBUTTONDOWN, c.bx, c.by);
      mouse(WM_LBUTTONUP, c.bx, c.by);
      same(c.name, ghost, madeBy(before));
      require(ghostLines.empty() && ghostGuideList.empty() && !ghostShown, std::string(c.name) + ": a ghost was left after placing");
      ++placedCount;
    };
    const std::vector<Case> cases = {
        {"Lamp", 6, 5.3f, 5.6f, 5.3f, 5.6f, {}},
        {"Furniture against a wall (wall snap)", 8, 1.9f, 7.1f, 1.9f, 7.1f, [&] {
           for (LRESULT i = 0, n = SendMessageW(modelList, CB_GETCOUNT, 0, 0); i < n; ++i)
             if (SendMessageW(modelList, CB_GETITEMDATA, i, 0) == 24) { box(modelList, ModelList, int(i)); break; }
         }},
        {"Furniture in the open", 8, 8.4f, 8.2f, 8.4f, 8.2f, {}},
        {"Loose prop", 14, 6.f, 9.f, 6.f, 9.f, {}},
        {"Computer", 15, 15.2f, 3.3f, 15.2f, 3.3f, {}},
        {"Health & ammo", 30, 16.7f, 4.4f, 16.7f, 4.4f, {}},
        {"Creature", CreatureTool, 18.2f, 8.1f, 18.2f, 8.1f, {}},
        {"Emitter", EmitterTool, 19.f, 5.f, 19.f, 5.f, {}},
        {"Person", 13, 4.1f, 14.6f, 4.1f, 14.6f, {}},
        {"Player start", 25, 3.2f, 20.3f, 3.2f, 20.3f, {}},
        {"Sign on the nearest wall", 7, 21.4f, 6.2f, 21.4f, 6.2f, {}},
        {"Shape, a click", 9, 7.f, 17.f, 7.f, 17.f, {}},
        {"Shape, dragged", 9, 9.f, 15.f, 12.4f, 17.6f, {}},
        {"Shape, turned with R", 9, 14.f, 16.f, 14.f, 16.f, [&] { MSG r{plan, WM_KEYDOWN, 'R', 0}; shortcut(r); }},
        {"Block", 20, 16.2f, 14.f, 18.9f, 15.7f, {}},
        {"Stairs", 12, 19.f, 17.f, 21.f, 21.f, {}},
        {"Water", WaterTool, 2.f, 2.f, 4.f, 3.5f, {}},
        {"Hazard", 22, 6.f, 2.f, 8.f, 3.f, {}},
        {"Place name", 28, 9.f, 2.f, 12.f, 4.f, {}},
        {"Trigger", 29, 13.f, 6.f, 15.f, 8.f, {}},
        {"Darkness", 33, 2.f, 9.f, 3.5f, 10.5f, {}},
        {"Crusher", 34, 10.f, 6.f, 11.5f, 7.5f, {}},
        {"Glass pane", GlassTool, 4.f, 18.f, 6.f, 18.f, {}},
        {"Pipe", 10, 11.f, 9.f, 14.f, 9.f, {}},
        {"Cable", 31, 11.f, 10.f, 15.f, 11.f, {}},
        {"Film camera", FilmTool, 2.5f, 15.f, 6.f, 13.5f, {}},
        {"Flooring (its soft snap)", FlooringTool, 13.3f, 18.1f, 16.8f, 21.2f, {}},
        {"Window wall along a wall", GlazingTool, 2.2f, 12.5f, 7.6f, 12.5f, {}},
        {"Breach through a wall", BreachTool, 14.f, 12.5f, 17.f, 12.5f, {}},
        {"Door in a gap", DoorTool, 10.5f, 12.5f, 10.5f, 12.5f, {}},
        {"Hang cloth: a curtain", ClothTool, 18.f, 11.f, 21.f, 11.f, {}},
        {"Connect: a rope from a loose thing", ConnectTool, 6.f, 9.f, 6.5f, 10.5f, [&] { box(connectKindList, ConnectKindList, ConnectRope); }},
        {"Connect: a hanging lamp", ConnectTool, 9.f, 10.f, 9.f, 10.f, [&] { box(connectKindList, ConnectKindList, ConnectLamp); }},
    };
    for (const auto &c : cases) run(c);
    if (!bankSounds.empty()) run({"Sound", SoundTool, 12.f, 20.f, 12.f, 20.f, {}});
    pass(std::to_string(placedCount) + " placing tools on the plan: the ghost is what is placed, field for field and outline for outline, and none is left after placing");

    // ---- A drag held while the tool changes: nothing placed, no preview left ----
    {
      pick(20);
      hoverInside = true;
      mouse(WM_MOUSEMOVE, 3, 5, 0);
      mouse(WM_LBUTTONDOWN, 3, 5);
      mouse(WM_MOUSEMOVE, 5, 6.5f);
      require(!ghostLines.empty(), "the Block drag shows no ghost");
      const auto before = doc.text();
      pick(6); // the Lamp, mid-drag
      // The Lamp's own ghost may show at the pointer; the Block's must be gone.
      const bool blockGhost = std::any_of(ghostLines.begin(), ghostLines.end(), [](const std::string &line) { return line.rfind("STRUCT|", 0) == 0; });
      require(!drag && !blockGhost && !ghostShown, "a drag held across a change of tool kept its ghost or its preview");
      mouse(WM_LBUTTONUP, 5, 6.5f);
      require(doc.text() == before, "a drag held across a change of tool placed something on release");
      pass("a drag held while the tool changes places nothing and leaves no ghost or preview");
    }
    // ---- A drag preview the throttle held back is shown once it can be, and is the ghost ----
    {
      pick(20);
      const bool keepLive = engineLive;
      const auto keepRendered = renderedRevision;
      engineLive = true;
      viewRect = true, dragFX = 3, dragFY = 15, hoverX = 5.1f, hoverY = 16.4f;
      renderedRevision = revision, lastGhost = GetTickCount64(); // the last preview went out just now
      previewGesture(3, 15, 5.1f, 16.4f);
      require(previewHeld && previewWanted != previewShown, "the throttle did not hold the preview back");
      lastGhost = 0, renderedRevision = revision; // the picture caught up
      catchUpPreview();
      require(previewWanted == previewShown && ghostShown, "a held preview was not shown once it could be");
      auto strip = [](std::string s) { s.erase(std::remove(s.begin(), s.end(), '\r'), s.end()); return s; };
      const auto shown = strip(channelText(pack));
      const auto ghost = ghostOf(3, 15, 5.1f, 16.4f);
      require(!ghost.empty() && shown.find(strip(ghost.front())) != std::string::npos, "the 3D preview is not the ghost's record");
      require(strip(doc.text()).find(strip(ghost.front())) == std::string::npos, "the preview reached the document");
      cancelPlacement();
      require(!ghostShown && !viewRect && strip(channelText(pack)) == strip(doc.text()), "putting the preview away left it in the 3D view");
      engineLive = keepLive, renderedRevision = keepRendered;
      pass("a drag preview the throttle held back is shown once the picture catches up, it is the ghost's record, and it goes when put away");
    }
    // ---- Undo, Redo and Select put the ghost away ----
    {
      pick(6);
      mouse(WM_MOUSEMOVE, 7.3f, 4.2f, 0);
      require(!ghostLines.empty(), "the Lamp shows no ghost");
      SendMessageW(window, WM_COMMAND, Undo, 0);
      require(ghostLines.empty() && ghostGuideList.empty() && !ghostShown, "Undo left the ghost up");
      mouse(WM_MOUSEMOVE, 7.6f, 4.2f, 0);
      require(!ghostLines.empty(), "the ghost did not come back when the pointer moved");
      SendMessageW(window, WM_COMMAND, Redo, 0);
      require(ghostLines.empty() && !ghostShown, "Redo left the ghost up");
      mouse(WM_MOUSEMOVE, 7.3f, 4.2f, 0);
      pick(0);
      require(ghostLines.empty() && ghostGuideList.empty(), "the ghost stayed after switching to Select");
      pass("Undo, Redo and switching tools put the ghost away in the plan and the 3D view");
    }
    // ---- In 3D: the hover ghost, then a press and release along the same ray ----
    int serial = 7000;
    auto in3d = [&](const std::string &name, int t, float x, float y, float eye, std::function<void()> setup = {}) {
      pick(t);
      if (setup) setup();
      hoverInside = false, drag = false;
      std::ostringstream ray;
      ray << "HOVER " << x << ' ' << y << ' ' << eye << " 0 0 -1";
      ghostHover("CLEAR");
      ghostHover(ray.str());
      const auto ghost = ghostLines;
      const auto guides = ghostGuideList;
      require(!ghost.empty() && !guides.empty(), name + ": the 3D view shows no ghost");
      const auto before = doc.lines;
      std::ostringstream press, release;
      press << "PICK " << ++serial << " 1 " << x << ' ' << y << ' ' << eye << " 0 0 -1 0";
      release << "PICK " << serial << " 0 " << x << ' ' << y << ' ' << eye << " 0 0 -1 0";
      applyPick(press.str());
      applyPick(release.str());
      const auto placed = madeBy(before);
      same(name, ghost, placed);
      // The 3D outline stands where the placed thing stands.
      auto s = ghostShapeOf(placed.front());
      float low = 1e9f, high = -1e9f;
      for (const auto &g : guides) low = std::min({low, g.az, g.bz}), high = std::max({high, g.az, g.bz});
      require(s && std::abs(low - s->bottom) <= .0015f && std::abs(high - s->top) <= .0015f,
              name + ": the 3D ghost spans " + std::to_string(low) + " to " + std::to_string(high) + ", the thing " + std::to_string(s ? s->bottom : 0) + " to " + std::to_string(s ? s->top : 0));
      require(ghostLines.empty() && ghostGuideList.empty(), name + ": a ghost was left after placing in 3D");
      return placed;
    };
    in3d("3D: a lamp on the floor", 6, 6.6f, 6.4f, 3.f);
    in3d("3D: furniture against a wall", 8, 21.6f, 4.2f, 3.f);
    pass("3D: the hover ghost is the placement, its outline at the placed thing's height, and none is left after");
    // An upper floor: what is placed there stands on it, ghost and all.
    {
      const int upper = createFloorLayer("Upper");
      layer = upper;
      publish();
      auto placed = in3d("3D: a lamp on an upper floor", 8, 5.2f, 18.3f, layerElevation() + 2.5f);
      require(ghostShapeOf(placed.front())->bottom >= layerElevation() - .001f, "the thing on the upper floor stands below it");
      layer = 0;
      publish();
      pass("3D: on an upper floor the ghost and the thing stand on that floor");
    }
    // On the land: a hill, a footing room on it, a cave under it.
    {
      NativeDocument land;
      land.newCampaign("Ghosts outside");
      land.addRoom("Field");
      adoptDocument(std::move(land), {}, true);
      autoDressingEnabled = false;
      prepareOutdoors();
      publish();
      auto placed = in3d("3D: furniture on a hillside", 8, 6.3f, 15.4f, 30.f);
      require(std::abs(ghostShapeOf(placed.front())->bottom - groundAt(6.3f, 15.4f)) < .3f, "the thing on the hill does not stand on the ground");
      // A room on the land: the Room tool lays a footing under it; a thing inside stands on its floor.
      pick(1);
      hoverInside = true;
      mouse(WM_MOUSEMOVE, 13, 5, 0);
      mouse(WM_LBUTTONDOWN, 13, 5);
      mouse(WM_MOUSEMOVE, 19, 10);
      mouse(WM_LBUTTONUP, 19, 10);
      const auto *part = footingPartAt(map, 16, 7.5f);
      require(part != nullptr, "the Room tool laid no footing on the land");
      placed = in3d("3D: furniture in a footing room", 8, 15.6f, 7.2f, 30.f);
      require(std::abs(ghostShapeOf(placed.front())->bottom - footingPartAt(map, 15.6f, 7.2f)->surface) < .01f, "the thing in the footing room does not stand on its floor");
      pass("3D on the land: on a hillside and in a footing room, the ghost stands where the thing lands");
      // A cave under the land: from inside it, a click on its floor places there (cave=1).
      auto [ox, oy] = mapOrigin();
      const float top = groundAt(12, 19);
      doc.add({"CAVE", "carve", nativeNumber(ox + 6), nativeNumber(oy + 19), nativeNumber(top - 6), nativeNumber(ox + 18), nativeNumber(oy + 19), nativeNumber(top - 6), "2.5"});
      publish();
      const float floor = caveFloorUnder(ox + 12, oy + 19);
      if (floor < top - 2) {
        placed = in3d("3D: a lamp in a cave", 6, 12.2f, 19.1f, top - 5.5f);
        require(namedValue(placed.front(), "cave") == "1", "the lamp placed on the cave's floor is not marked cave=1");
        const float at = ghostShapeOf(placed.front())->bottom;
        require(at > floor - .05f && at < top - 1.f, "the lamp in the cave stands at " + std::to_string(at) + ", the cave's floor is " + std::to_string(floor) + " under land at " + std::to_string(top));
        pass("3D in a cave: the ghost and the lamp stand on the cave's floor, not the hill over it");
      } else
        report << "NOTE: the carved cave had no floor below the land here; the cave case was not run\n";
    }
    if (pictures)
      ghostPictures();
    snap = savedSnap, smartSnap = savedSmart;
    pick(0);
    dirty = false;
  }
  // ---- Pictures (.build/interaction): a chair placed against a wall, the plan's ghost at the
  // pointer as v1.5.6 drew it beside where the chair went, and the ghost now; the 3D view the
  // engine's picture with the ghost's outline before the click and the chair in it after.
  fs::path ghostEngine(const std::wstring &name, float x, float y, float eye, float yaw, float pitch) {
    if (!fs::exists(engineExecutable())) return {};
    const auto folder = root / L".build" / L"interaction";
    auto session = folder / (name + L"-session");
    fs::create_directories(session);
    doc.save(session / L"working.rmc");
    std::ofstream(session / L"control.txt") << "1 " << map << " 0 0 0 0 0 0 0\n" << editorViewControl();
    std::ofstream(session / L"look.txt") << "LOOK 0 0 0 0\n";
    std::ofstream(session / L"film.txt") << "FILM 1 " << x << ' ' << y << ' ' << eye << ' ' << yaw << ' ' << pitch << " 70\n";
    const auto ppm = folder / (name + L".ppm");
    std::error_code ec;
    fs::remove(ppm, ec);
    std::wstring line = L"\"" + engineExecutable().wstring() + L"\" \"--editor-pack=" + (session / L"working.rmc").wstring() + L"\" \"--editor-control=" +
                        (session / L"control.txt").wstring() + L"\" \"--editor-status=" + (session / L"status.txt").wstring() +
                        L"\" --editor-frames=60 \"--editor-capture=" + ppm.wstring() + L"\" --editor-hidden";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION p{};
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, session.c_str(), &si, &p)) return {};
    if (WaitForSingleObject(p.hProcess, 300000) == WAIT_TIMEOUT) TerminateProcess(p.hProcess, 9);
    CloseHandle(p.hThread), CloseHandle(p.hProcess);
    return fs::exists(ppm) ? ppm : fs::path{};
  }
  // The whole window at 2300 x 1000, the engine's picture (a .ppm) in the 3D pane, as a .bmp.
  void windowPicture(const fs::path &file, const fs::path &ppm) {
    RECT was{};
    GetWindowRect(window, &was);
    SetWindowPos(window, nullptr, -6000, 0, 2300, 1000, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(window, SW_SHOWNOACTIVATE);
    layout();
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    for (ULONGLONG until = GetTickCount64() + 300; GetTickCount64() < until;) {
      MSG msg;
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) TranslateMessage(&msg), DispatchMessageW(&msg);
      Sleep(10);
    }
    RECT r{};
    GetClientRect(window, &r);
    HDC screen = GetDC(window), dc = CreateCompatibleDC(screen);
    auto bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom);
    auto old = SelectObject(dc, bitmap);
    PrintWindow(window, dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = r.right, info.bmiHeader.biHeight = -r.bottom;
    info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
    std::vector<unsigned char> bits(size_t(r.right) * size_t(r.bottom) * 4);
    SelectObject(dc, old);
    GetDIBits(dc, bitmap, 0, UINT(r.bottom), bits.data(), &info, DIB_RGB_COLORS);
    std::ifstream in(ppm, std::ios::binary);
    std::string magic;
    int w = 0, h = 0, depth = 0;
    if (!ppm.empty() && in >> magic >> w >> h >> depth) {
      in.get();
      std::vector<unsigned char> rgb(size_t(std::max(0, w * h * 3)));
      in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()));
      RECT v{};
      GetWindowRect(viewport, &v);
      MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&v), 2);
      v.left = std::clamp<LONG>(v.left, 0, r.right), v.right = std::clamp<LONG>(v.right, 0, r.right);
      v.top = std::clamp<LONG>(v.top, 0, r.bottom), v.bottom = std::clamp<LONG>(v.bottom, 0, r.bottom);
      const int vw = int(v.right - v.left), vh = int(v.bottom - v.top);
      if (w > 0 && h > 0 && vw > 0 && vh > 0) {
        const float s = std::max(float(vw) / float(w), float(vh) / float(h));
        const float ox = (float(w) * s - float(vw)) * .5f, oy = (float(h) * s - float(vh)) * .5f;
        for (int y = 0; y < vh; ++y)
          for (int x = 0; x < vw; ++x) {
            const int sx = std::clamp(int((float(x) + ox) / s), 0, w - 1), sy = std::clamp(int((float(y) + oy) / s), 0, h - 1);
            const unsigned char *src = rgb.data() + (size_t(sy) * size_t(w) + size_t(sx)) * 3;
            unsigned char *dst = bits.data() + (size_t(v.top + y) * size_t(r.right) + size_t(v.left + x)) * 4;
            dst[0] = src[2], dst[1] = src[1], dst[2] = src[0], dst[3] = 255;
          }
      }
    }
    std::ofstream out(file, std::ios::binary);
    BITMAPFILEHEADER head{};
    head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), head.bfSize = DWORD(head.bfOffBits + bits.size());
    out.write(reinterpret_cast<const char *>(&head), sizeof head);
    out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
    out.write(reinterpret_cast<const char *>(bits.data()), std::streamsize(bits.size()));
    DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(window, screen);
    ShowWindow(window, SW_HIDE);
    SetWindowPos(window, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
  }
  void ghostPictures() {
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    NativeDocument blank;
    blank.newCampaign("Ghost pictures");
    blank.addRoom("Office");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false, snap = .25f, smartSnap = true;
    SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
    for (LRESULT i = 0, n = SendMessageW(modelList, CB_GETCOUNT, 0, 0); i < n; ++i)
      if (SendMessageW(modelList, CB_GETITEMDATA, i, 0) == 24) {
        SendMessageW(modelList, CB_SETCURSEL, i, 0);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(ModelList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(modelList));
        break;
      }
    auto mouse = [&](UINT message, float x, float y, WPARAM keys) {
      auto v = planView();
      SendMessageW(plan, message, keys, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    // The window's own size first: the plan's cells must be the picture's.
    RECT was{};
    GetWindowRect(window, &was);
    SetWindowPos(window, nullptr, -6000, 0, 2300, 1000, SWP_NOZORDER | SWP_NOACTIVATE);
    layout();
    const float px = 2.15f, py = 9.4f; // a step off the west wall: the desk goes to the wall
    hoverInside = true;
    // After: the ghost the placement makes, in the plan and the 3D view (before the click).
    mouse(WM_MOUSEMOVE, px + 1, py, 0);
    mouse(WM_MOUSEMOVE, px, py, 0);
    windowPicture(folder / L"ghost-after-hover.bmp", ghostEngine(L"ghost-after-hover", 6.5f, 9.4f, 2.4f, 180.f, -18.f));
    mouse(WM_LBUTTONDOWN, px, py, MK_LBUTTON);
    mouse(WM_LBUTTONUP, px, py, MK_LBUTTON);
    mouse(WM_MOUSEMOVE, px + 1, py, 0);
    mouse(WM_MOUSEMOVE, px, py, 0); // the next desk's ghost lies exactly over the one placed
    windowPicture(folder / L"ghost-after-placed.bmp", ghostEngine(L"ghost-after-placed", 6.5f, 9.4f, 2.4f, 180.f, -18.f));
    // Before: the same desk, the plan's ghost drawn the v1.5.6 way (a box at the pointer).
    ghostLegacyDraw = true;
    InvalidateRect(plan, nullptr, FALSE);
    windowPicture(folder / L"ghost-before.bmp", {});
    ghostLegacyDraw = false;
    SetWindowPos(window, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
    std::ofstream(root / L"native-editor-ghost-test.txt", std::ios::app)
        << "PICTURES: .build/interaction/ghost-before.bmp (v1.5.6's plan ghost, a box at the pointer, beside the desk the click put against the wall), "
           "ghost-after-hover.bmp (the ghost the placement makes, in the plan and the 3D view, before the click), "
           "ghost-after-placed.bmp (after the click: the next ghost lies exactly on the desk placed)\n";
    dirty = false;
  }
