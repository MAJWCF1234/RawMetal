  // Outdoors, driven through the plan's mouse, the 3D view's pick rays, the tool boxes and
  // sliders, Undo and save: the Ashfall Exclusion Zone and a mixed indoor/outdoor campaign,
  // the Terrain, Nature and Water tools, and the engine's own validation. --outdoor-test adds
  // the builder's pictures before and after (.build/interaction/outdoor-*.bmp, outdoor-*.ppm)
  // and writes native-editor-outdoor-test.txt; --editing-test runs it without the pictures.
  std::vector<std::string> outdoorPassed;
  void verifyOutdoorRegressions(bool pictures) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error(message);
    };
    auto &passed = outdoorPassed;
    passed.clear();
    auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    auto keepPath = path;
    auto fieldsOf = [&](size_t i) { return NativeDocument::fields(doc.lines[i]); };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    auto mouse = [&](UINT message, float x, float y) {
      auto v = planView();
      SendMessageW(plan, message, MK_LBUTTON, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto click = [&](float x, float y) { mouse(WM_LBUTTONDOWN, x, y), mouse(WM_LBUTTONUP, x, y); };
    auto stroke3 = [&](float ax, float ay, float bx, float by) {
      mouse(WM_LBUTTONDOWN, ax, ay);
      for (int i = 1; i <= 8; ++i)
        mouse(WM_MOUSEMOVE, ax + (bx - ax) * i / 8.f, ay + (by - ay) * i / 8.f);
      mouse(WM_LBUTTONUP, bx, by);
    };
    int serial = 900;
    // A press and release in the 3D view along a ray from (ox, oy, oz) toward (tx, ty, tz).
    auto pick3d = [&](float ox, float oy, float oz, float tx, float ty, float tz) {
      std::ostringstream line;
      line << "PICK " << ++serial << " 1 " << ox << ' ' << oy << ' ' << oz << ' ' << tx - ox << ' ' << ty - oy << ' ' << tz - oz << " 0";
      applyPick(line.str());
      std::ostringstream up;
      up << "PICK " << serial << " 0 " << ox << ' ' << oy << ' ' << oz << ' ' << tx - ox << ' ' << ty - oy << ' ' << tz - oz << " 0";
      applyPick(up.str());
    };
    auto planShot = [&](const wchar_t *name) {
      InvalidateRect(plan, nullptr, TRUE);
      RECT r{};
      GetClientRect(plan, &r);
      HDC dc = CreateCompatibleDC(nullptr);
      BITMAPINFO info{};
      info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = r.right, info.bmiHeader.biHeight = -r.bottom;
      info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
      void *bits = nullptr;
      auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
      auto old = SelectObject(dc, bitmap);
      SendMessageW(plan, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
      GdiFlush();
      std::ofstream out(folder / name, std::ios::binary);
      BITMAPFILEHEADER head{};
      head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
      head.bfSize = DWORD(head.bfOffBits + size_t(r.right) * size_t(r.bottom) * 4);
      out.write(reinterpret_cast<const char *>(&head), sizeof head);
      out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
      out.write(static_cast<const char *>(bits), std::streamsize(size_t(r.right) * size_t(r.bottom) * 4));
      SelectObject(dc, old), DeleteObject(bitmap), DeleteDC(dc);
    };
    // The whole builder window, the right pane showing the tool's boxes.
    auto windowShot = [&](const wchar_t *name) {
      RECT was{};
      GetWindowRect(window, &was);
      SetWindowPos(window, nullptr, -6000, 0, 1600, 960, SWP_NOZORDER | SWP_NOACTIVATE);
      ShowWindow(window, SW_SHOWNOACTIVATE);
      layout();
      RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
      RECT r{};
      GetClientRect(window, &r);
      if (r.right > 0 && r.bottom > 0) {
        HDC screen = GetDC(window), dc = CreateCompatibleDC(screen);
        auto bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom);
        auto old = SelectObject(dc, bitmap);
        // Let the window settle on its off-screen spot before it is printed (the composited
        // copy can still be blank right after a long plan paint).
        for (ULONGLONG until = GetTickCount64() + 400; GetTickCount64() < until;) {
          MSG msg;
          while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            TranslateMessage(&msg), DispatchMessageW(&msg);
          Sleep(10);
        }
        if (!PrintWindow(window, dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT))
          PrintWindow(window, dc, PW_CLIENTONLY);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = r.right, info.bmiHeader.biHeight = -r.bottom;
        info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
        std::vector<char> bits(size_t(r.right) * size_t(r.bottom) * 4);
        SelectObject(dc, old);
        GetDIBits(dc, bitmap, 0, UINT(r.bottom), bits.data(), &info, DIB_RGB_COLORS);
        if (std::all_of(bits.begin(), bits.end(), [&](char c) { return c == bits[0]; })) {
          // A blank print: draw the window and its children through WM_PRINT instead.
          old = SelectObject(dc, bitmap);
          SendMessageW(window, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
          EnumChildWindows(window, [](HWND child, LPARAM target) -> BOOL {
            if (!(GetWindowLongW(child, GWL_STYLE) & WS_VISIBLE) || GetParent(child) != GetAncestor(child, GA_ROOT))
              return TRUE;
            RECT c{};
            GetWindowRect(child, &c);
            MapWindowPoints(HWND_DESKTOP, GetParent(child), reinterpret_cast<POINT *>(&c), 2);
            HDC into = reinterpret_cast<HDC>(target);
            POINT was{};
            SetViewportOrgEx(into, c.left, c.top, &was);
            PrintWindow(child, into, 0);
            SetViewportOrgEx(into, was.x, was.y, nullptr);
            return TRUE;
          }, reinterpret_cast<LPARAM>(dc));
          SelectObject(dc, old);
          GetDIBits(dc, bitmap, 0, UINT(r.bottom), bits.data(), &info, DIB_RGB_COLORS);
        }
        std::ofstream out(folder / name, std::ios::binary);
        BITMAPFILEHEADER head{};
        head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), head.bfSize = DWORD(head.bfOffBits + bits.size());
        out.write(reinterpret_cast<const char *>(&head), sizeof head);
        out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
        out.write(bits.data(), std::streamsize(bits.size()));
        DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(window, screen);
      }
      ShowWindow(window, SW_HIDE);
      SetWindowPos(window, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    // The builder's 3D picture: the engine run hidden on this document, the camera at a spot.
    auto picture = [&](const std::wstring &name, float x, float y, float eye, float yaw, float pitch) {
      auto session = folder / (name + L"-session");
      fs::create_directories(session);
      doc.save(session / L"working.rmc");
      std::ofstream(session / L"control.txt") << "1 " << map << " 0 0 0 0 0 0 0\n0\n0\n";
      std::ofstream(session / L"look.txt") << "LOOK 0 0 0 0\n";
      std::ofstream(session / L"film.txt") << "FILM 1 " << x << ' ' << y << ' ' << eye << ' ' << yaw << ' ' << pitch << " 70\n";
      std::error_code ec;
      fs::remove(folder / (name + L".ppm"), ec);
      std::wstring line = L"\"" + engineExecutable().wstring() + L"\" \"--editor-pack=" + (session / L"working.rmc").wstring() +
                          L"\" \"--editor-control=" + (session / L"control.txt").wstring() + L"\" \"--editor-status=" + (session / L"status.txt").wstring() +
                          L"\" --editor-frames=40 \"--editor-capture=" + (folder / (name + L".ppm")).wstring() + L"\" --editor-hidden";
      STARTUPINFOW si{sizeof(si)};
      PROCESS_INFORMATION p{};
      require(CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, session.c_str(), &si, &p) != 0,
              "The engine could not be started for the outdoor pictures");
      if (WaitForSingleObject(p.hProcess, 240000) == WAIT_TIMEOUT)
        TerminateProcess(p.hProcess, 9);
      DWORD code = 1;
      GetExitCodeProcess(p.hProcess, &code);
      CloseHandle(p.hThread), CloseHandle(p.hProcess);
      require(code == 0 && fs::exists(folder / (name + L".ppm")), "The engine picture failed for " + utf8(name));
    };
    auto pixels = [&](const std::wstring &name) {
      std::ifstream in(folder / (name + L".ppm"), std::ios::binary);
      std::string magic;
      int w = 0, h = 0, d = 0;
      in >> magic >> w >> h >> d;
      in.get();
      std::vector<unsigned char> rgb(size_t(std::max(0, w * h * 3)));
      in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()));
      return rgb;
    };
    auto differ = [&](const std::wstring &a, const std::wstring &b) {
      auto p = pixels(a), q = pixels(b);
      if (p.size() != q.size() || p.empty())
        return -1;
      int n = 0;
      for (size_t i = 0; i + 2 < p.size(); i += 3)
        n += std::abs(int(p[i]) - int(q[i])) + std::abs(int(p[i + 1]) - int(q[i + 1])) + std::abs(int(p[i + 2]) - int(q[i + 2])) > 24;
      return n;
    };
    auto roundTrip = [&](const wchar_t *name) {
      auto saved = folder / name;
      doc.save(saved);
      NativeDocument back;
      back.open(saved);
      require(back.text() == doc.text(), "Save/reopen changed " + utf8(name));
      require(validateDocument(), "The engine refused " + utf8(name) + ": " + utf8(notice));
    };
    auto openCampaign = [&](const fs::path &file) {
      doc.open(file);
      path.clear();
      map = layer = 0, record = -1;
      selectedRecords.clear(), history.clear(), future.clear();
      tool = 0, viewMode = 1, smartSnap = false, autoDressingEnabled = false, snap = 0;
      refresh(), layout();
    };
    auto selectMap = [&](int m) {
      SendMessageW(mapList, LB_SETCURSEL, m, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(MapList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(mapList));
      require(map == m, "The map list did not switch to map " + std::to_string(m));
    };

    // ---- 1. The Ashfall Exclusion Zone opens, saves byte for byte and validates.
    auto ashfall = root / L"custom maps" / L"RawMetal-Ashfall-Exclusion-Zone.txt";
    openCampaign(ashfall);
    if (pictures && GetEnvironmentVariableW(L"OUTDOOR_SHOT_ONLY", nullptr, 0) > 0) { // a quick look at the window pictures
      selectMap(1);
      tool = TerrainTool;
      group = 4, repaintTools(), windowShot(L"outdoor-terrain-tool-window.bmp");
      return;
    }
    auto pristine = doc.text();
    require(validateDocument(), "The engine refused the Ashfall zone as opened: " + utf8(notice));
    // The hilliest map: the one whose ground varies most.
    int hilly = 0;
    float spreadBest = -1;
    for (size_t i = 0; i < doc.records("MAP").size(); ++i) {
      map = int(i);
      if (!onTerrain())
        continue;
      float low = 1e9f, high = -1e9f;
      for (int y = 2; y < 22; y += 2)
        for (int x = 2; x < 22; x += 2)
          low = std::min(low, groundAt(x + .5f, y + .5f)), high = std::max(high, groundAt(x + .5f, y + .5f));
      if (high - low > spreadBest)
        spreadBest = high - low, hilly = int(i);
    }
    map = 0;
    selectMap(hilly);
    require(onTerrain() && spreadBest > 1, "No Ashfall map stands on terrain");
    if (pictures)
      planShot(L"outdoor-ashfall-plan-before.bmp");
    // The plan draws the land on a terrain map: its pixels are not the indoor floor's slate.
    {
      RECT r{};
      GetClientRect(plan, &r);
      HDC dc = CreateCompatibleDC(nullptr);
      auto bitmap = CreateCompatibleBitmap(GetDC(plan), r.right, r.bottom);
      auto old = SelectObject(dc, bitmap);
      SendMessageW(plan, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
      auto v = planView();
      int land = 0;
      for (int y = 1; y < 24; y += 3)
        for (int x = 1; x < 24; x += 3) {
          COLORREF c = GetPixel(dc, int(v.ox + (x + .25f) * v.cell), int(v.oy + (y + .25f) * v.cell));
          land += c != ui::FloorFill && GetGValue(c) > GetBValue(c);
        }
      SelectObject(dc, old), DeleteObject(bitmap), DeleteDC(dc);
      require(land > 40, "The plan does not draw the land on a terrain map (" + std::to_string(land) + " of 64 samples)");
    }
    passed.push_back("Ashfall opens and validates; map " + std::to_string(hilly) + " has " + shortNumber(spreadBest) +
                     " m of relief; the plan draws its land (hill shade, cover, contours) instead of an indoor floor");

    // ---- 2. 3D picks stand on the ground: a model placed by a ray lands where the ray meets the hill.
    {
      // A spot on high ground with nothing standing within reach (a bush there would rightly be picked first).
      float tx = -1, ty = -1;
      for (int y = 3; y < 21; ++y)
        for (int x = 3; x < 21; ++x)
          if ((tx < 0 || groundAt(x + .5f, y + .5f) > groundAt(tx, ty)) && !natureBlocked(x + .5f, y + .5f, .5f))
            tx = x + .5f, ty = y + .5f;
      require(tx > 0, "No open high ground on the hilly map");
      // Plants and props standing within reach are cleared first (Ctrl+Z restores them).
      {
        std::vector<size_t> closeBy;
        for (auto i : objects())
          if (auto h = handleFor(i); h && std::hypot(h->x - tx, h->y - ty) < std::max(h->width, h->depth) * .5f + 1.5f)
            closeBy.push_back(i);
        std::sort(closeBy.rbegin(), closeBy.rend());
        for (auto i : closeBy)
          eraseNativeObject(doc, i);
        refresh();
      }
      float g = groundAt(tx, ty);
      placeHeight = 0;
      tool = 8;
      layout();
      auto before = doc.records("FIXTURE", map).size();
      pick3d(tx - 7, ty - 3, g + 7, tx, ty, g);
      require(doc.records("FIXTURE", map).size() == before + 1, "A 3D click with Furniture placed nothing on the hill");
      auto f = fieldsOf(doc.records("FIXTURE", map).back());
      float dx = number(f[3], 0) - tx, dy = number(f[4], 0) - ty;
      // On the z = 0 plane the old pick landed several metres beyond the hill.
      require(std::hypot(dx, dy) < .6f, "A 3D click on the hill placed the model " + shortNumber(std::hypot(dx, dy)) + " m from where the ray met the ground");
      // The placed model is picked by a ray at its middle, where it stands on the ground.
      tool = 0;
      layout();
      float mx0 = number(f[3], 0), my0 = number(f[4], 0), mg = groundAt(mx0, my0), mid = mg + number(f[8], 1) * .5f;
      int hit = selectAlongRay(mx0 - 1.5f, my0 + .5f, mid + .5f, 1.5f, -.5f, -.5f);
      require(hit == int(doc.records("FIXTURE", map).back()), "A 3D ray at the model where it stands on the hill did not pick it (hit " + (hit >= 0 ? doc.lines[size_t(hit)] : std::string("nothing")) + "; model " + doc.lines[doc.records("FIXTURE", map).back()] + ", ground " + shortNumber(mg) + ")");
      selectRecord(hit);
      auto guides = selectionGuides();
      float lowest = 1e9f;
      for (auto &q : guides)
        lowest = std::min({lowest, q.az, q.bz});
      require(!guides.empty() && std::abs(lowest - mg) < .05f, "The picked model's 3D outline is not on the ground (" + shortNumber(lowest) + " vs " + shortNumber(mg) + ")");
      // A ray that meets the hill first does not pick what stands behind it.
      int behind = selectAlongRay(tx - 30, ty, g - 3, 30, 0, 3.5f);
      require(behind != hit || groundAt(tx - 3, ty) < g, "A model behind a hill lit up through it");
      // Triggers keep absolute heights outdoors: a new one spans the ground at its spot.
      tool = 29;
      autoDetail = true; // a trigger that says something (one with no step is refused)
      layout();
      gesture(tx - 1, ty - 1, tx + 1, ty + 1);
      auto t = fieldsOf(doc.records("SCRIPT_EVENT", map).back());
      require(number(t[7], 0) < g && number(t[8], 0) > g + 1.5f, "A trigger on the hill does not span the ground there (" + t[7] + " to " + t[8] + ", ground " + shortNumber(g) + ")");
      tool = 0;
      passed.push_back("3D view on terrain: a click places on the hill where the ray meets it (" + shortNumber(std::hypot(dx, dy)) + " m from the aim), a ray at the model picks it, its outline sits on the ground, and a new trigger spans the ground (absolute heights)");
    }

    // ---- 3. Terrain tool: raise, lower, smooth, flatten, ramp and paint; one Undo a stroke.
    tool = TerrainTool;
    layout();
    require((GetWindowLongW(terrainModeList, GWL_STYLE) & WS_VISIBLE) && (GetWindowLongW(terrainSizeList, GWL_STYLE) & WS_VISIBLE),
            "The Terrain tool shows no brush and size boxes");
    if (pictures)
      group = 4, repaintTools(), windowShot(L"outdoor-terrain-tool-window.bmp");
    box(terrainSizeList, TerrainSizeList, 3);     // 5 m
    box(terrainStrengthList, TerrainStrengthList, 3); // a full metre a dab
    float cx = 12.5f, cy = 12.5f;
    auto topAt = [&](float x, float y) { return groundAt(x, y) - .5f; }; // at whole metres: the voxel top of that cell
    float before = topAt(12, 12);
    auto textBefore = doc.text();
    size_t undo = history.size();
    box(terrainModeList, TerrainModeList, TerrainRaise);
    click(cx, cy);
    click(cx, cy);
    float raised = topAt(12, 12);
    require(std::abs(raised - (before + 2)) < .01f, "Two Raise dabs did not lift the ground two metres (" + shortNumber(before) + " to " + shortNumber(raised) + ")");
    require(history.size() == undo + 2 && !doc.records("TERRAIN_SCULPT").empty(), "Raise did not write TERRAIN_SCULPT as one Undo a stroke");
    require(topAt(16, 12) < raised - .5f && topAt(16, 12) > before - 1, "The raise brush has no falloff");
    box(terrainModeList, TerrainModeList, TerrainLower);
    click(cx, cy);
    require(topAt(12, 12) < raised - .5f, "Lower did not dig the ground down");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(std::abs(topAt(12, 12) - raised) < .01f, "Undo did not take the Lower stroke back");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == textBefore, "Undo did not take the raised ground back exactly");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(std::abs(topAt(12, 12) - raised) < .01f, "Redo did not bring the raised ground back");
    // A one-metre spike, then Smooth takes it down; Flatten levels to a typed level; Ramp lays a slope.
    box(terrainModeList, TerrainModeList, TerrainRaise);
    box(terrainSizeList, TerrainSizeList, 0);
    click(cx, cy), click(cx, cy);
    float spike = topAt(12, 12);
    require(std::abs(spike - (raised + 2)) < .01f, "A 1 m Raise did not lift one metre cell");
    box(terrainModeList, TerrainModeList, TerrainSmooth);
    box(terrainSizeList, TerrainSizeList, 2);
    box(terrainStrengthList, TerrainStrengthList, 2);
    click(cx, cy);
    require(topAt(12, 12) < spike - .9f, "Smooth did not wear the spike down (" + shortNumber(spike) + " to " + shortNumber(topAt(12, 12)) + ")");
    box(terrainSizeList, TerrainSizeList, 2);
    box(terrainModeList, TerrainModeList, TerrainFlatten);
    SetWindowTextW(terrainHeightBox, L"3");
    box(terrainStrengthList, TerrainStrengthList, 3);
    stroke3(5, 18, 9, 18);
    for (float x : {6.f, 7.f, 8.f})
      require(std::abs(topAt(x, 18) - 3) < .01f, "Flatten to 3 m did not level the ground: " + shortNumber(topAt(x, 18)) + " at x " + shortNumber(x));
    SetWindowTextW(terrainHeightBox, L"");
    box(terrainModeList, TerrainModeList, TerrainRamp);
    box(terrainSizeList, TerrainSizeList, 2);
    float rampLow = topAt(4, 4), rampHigh = topAt(20, 4);
    stroke3(4, 4, 20, 4);
    float a = topAt(8, 4), b = topAt(12, 4), c = topAt(16, 4);
    bool rising = rampHigh >= rampLow ? (a <= b + .6f && b <= c + .6f) : (a + .6f >= b && b + .6f >= c);
    require(rising, "Ramp did not lay a steady slope from " + shortNumber(rampLow) + " to " + shortNumber(rampHigh) + " (" + shortNumber(a) + ", " + shortNumber(b) + ", " + shortNumber(c) + ")");
    box(terrainModeList, TerrainModeList, TerrainPaint);
    layout();
    require(GetWindowLongW(groundList, GWL_STYLE) & WS_VISIBLE, "Paint the ground shows no cover list");
    box(groundList, GroundList, 6); // gravel
    auto groundBefore = doc.records("GROUND").size();
    stroke3(3, 21, 15, 21);
    auto shape = campaignTerrain();
    auto [mx, my] = mapOrigin();
    require(retro::authoredGroundKind(*shape, mx + 9.5f, my + 21.5f) == 6 && doc.records("GROUND").size() > groundBefore &&
                doc.records("GROUND").size() < groundBefore + 12,
            "Paint did not lay gravel as a few merged GROUND boxes (" + std::to_string(doc.records("GROUND").size() - groundBefore) + ")");
    roundTrip(L"outdoor-ashfall-terrain.rmc");
    passed.push_back("Terrain tool on Ashfall: Raise (+" + shortNumber(raised - before) + " m in two dabs, falloff), Lower, Smooth, Flatten to a typed 3 m, Ramp along a drag, Paint gravel as " +
                     std::to_string(doc.records("GROUND").size() - groundBefore) + " merged GROUND boxes; TERRAIN_SCULPT records; one Undo a stroke, exact Undo/Redo; saved byte for byte and engine-valid");

    // ---- 4. Nature tool: plants, spacing, slope, avoiding walls and water, erase; one Undo.
    tool = NatureTool;
    layout();
    require(GetWindowLongW(natureSetList, GWL_STYLE) & WS_VISIBLE, "The Nature tool shows no plant list");
    if (pictures)
      group = 4, repaintTools(), windowShot(L"outdoor-nature-tool-window.bmp");
    // A block and a pool in the brush's way.
    addAuthored({"STRUCT", std::to_string(map), "15", "8", "18", "11", "0", "2", "0", "15"});
    addAuthored({"WATER", std::to_string(map), "15", "13", "18", "16", "0", "0.4"});
    box(natureModeList, NatureModeList, 0);
    box(terrainSizeList, TerrainSizeList, 3); // 5 m
    box(natureSetList, NatureSetList, 2);  // grass
    box(natureDensityList, NatureDensityList, 2);
    auto fixturesBefore = doc.records("FIXTURE", map).size();
    undo = history.size();
    click(16.5f, 12.f);
    auto plants = doc.records("FIXTURE", map);
    int planted = int(plants.size() - fixturesBefore), sloped = 0;
    require(planted >= 20 && history.size() == undo + 1, "A Nature dab of grass planted " + std::to_string(planted) + " (one Undo expected)");
    for (size_t k = fixturesBefore; k < plants.size(); ++k) {
      auto f = fieldsOf(plants[k]);
      float px = number(f[3], 0), py = number(f[4], 0);
      require(!(px > 14.85f && px < 18.15f && py > 7.85f && py < 11.15f), "Grass grew inside the block");
      require(!(px > 14.7f && px < 18.3f && py > 12.7f && py < 16.3f), "Grass grew in the water");
      require(natureModel(int(number(f[2], -1))) && f[10] == "0", "The grass is not a non-solid nature model");
      sloped += f.size() == 16;
    }
    require(sloped > 0, "Align to slope wrote no slope on the hill");
    // A clearing first (Erase), then trees in it.
    box(natureModeList, NatureModeList, 1);
    box(terrainSizeList, TerrainSizeList, 4);
    click(6.f, 6.f);
    box(natureModeList, NatureModeList, 0);
    box(natureSetList, NatureSetList, 0); // trees
    box(natureDensityList, NatureDensityList, 1);
    auto treesBefore = doc.records("FIXTURE", map).size();
    click(6.f, 6.f);
    std::vector<std::pair<float, float>> trees;
    for (size_t k = treesBefore; k < doc.records("FIXTURE", map).size(); ++k) {
      auto f = fieldsOf(doc.records("FIXTURE", map)[k]);
      require(treeModel(int(number(f[2], -1))) && f[10] == "1" && number(f[11], 1) < .2f && f.size() <= 12, "A tree is not solid at its trunk, or leans");
      trees.push_back({number(f[3], 0), number(f[4], 0)});
    }
    require(!trees.empty(), "The tree brush planted nothing");
    for (size_t i = 0; i < trees.size(); ++i)
      for (size_t j = i + 1; j < trees.size(); ++j)
        require(std::hypot(trees[i].first - trees[j].first, trees[i].second - trees[j].second) > 2.5f, "Two trees stand closer than their spacing");
    require(validateDocument(), "The engine refused the planted grass and trees: " + utf8(notice));
    box(natureModeList, NatureModeList, 1);
    box(terrainSizeList, TerrainSizeList, 3);
    auto withPlants = doc.records("FIXTURE", map).size();
    click(16.5f, 12.f);
    auto afterErase = doc.records("FIXTURE", map).size();
    require(afterErase + size_t(planted) <= withPlants + 2 && afterErase < withPlants, "Erase did not clear the grass under the brush");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.records("FIXTURE", map).size() == withPlants, "Undo did not bring the erased plants back");
    box(natureModeList, NatureModeList, 0);
    roundTrip(L"outdoor-ashfall-nature.rmc");
    passed.push_back("Nature tool: " + std::to_string(planted) + " grass in one dab (" + std::to_string(sloped) + " lying along the slope, none in the block or the pool), " +
                     std::to_string(trees.size()) + " trees spaced apart with trunk collision and upright; Erase clears, Undo returns; engine-valid");

    // ---- 5. Water: a dragged pool sits under its bank; fill a hollow; the bank sets the surface; murk and wet banks; corners resize.
    tool = WaterTool;
    layout();
    box(waterModeList, WaterModeList, 0);
    mouse(WM_LBUTTONDOWN, 3, 9);
    mouse(WM_MOUSEMOVE, 6, 12);
    mouse(WM_LBUTTONUP, 6, 12);
    auto pool = fieldsOf(doc.records("WATER", map).back());
    float bank = 1e9f;
    for (float t = 0; t <= 1; t += .25f)
      bank = std::min({bank, groundAt(3 + 3 * t, 9), groundAt(3 + 3 * t, 12), groundAt(3, 9 + 3 * t), groundAt(6, 9 + 3 * t)});
    float poolFloor = groundHigh(3, 9, 6, 12, false);
    require(number(pool[7], 0) > number(pool[6], 0) && number(pool[7], -9) >= poolFloor + .1f && number(pool[7], -9) <= std::max(bank, poolFloor + .4f) + .05f,
            "A pool dragged outdoors is not under its bank and over its floor: " + doc.lines[doc.records("WATER", map).back()]);
    // A hollow dug with Lower, then filled with one click.
    tool = TerrainTool;
    box(terrainModeList, TerrainModeList, TerrainLower);
    box(terrainSizeList, TerrainSizeList, 3);
    box(terrainStrengthList, TerrainStrengthList, 3);
    for (int i = 0; i < 3; ++i)
      click(9.5f, 15.5f);
    tool = WaterTool;
    layout();
    box(waterModeList, WaterModeList, 1);
    auto waterBefore = doc.records("WATER", map).size();
    undo = history.size();
    click(9.5f, 15.5f);
    auto filled = doc.records("WATER", map);
    require(filled.size() > waterBefore && history.size() == undo + 1, "Fill a hollow made no water (one Undo)");
    float surface = number(fieldsOf(filled.back())[7], 0);
    require(surface > groundAt(9.5f, 15.5f) + .3f, "The filled water is not over the hollow's floor");
    for (size_t k = waterBefore; k < filled.size(); ++k) {
      auto f = fieldsOf(filled[k]);
      require(std::abs(number(f[7], 0) - surface) < .001f, "The boxes of one filled pool disagree on its surface");
      for (int y = int(number(f[3], 0)); y < int(number(f[5], 0)); ++y)
        for (int x = int(number(f[2], 0)); x < int(number(f[4], 0)); ++x)
          require(groundAt(x + .5f, y + .5f) < surface, "Fill a hollow flooded a metre above the water");
    }
    if (pictures)
      planShot(L"outdoor-ashfall-plan-after.bmp");
    // The bank: the surface goes to the ground where it is clicked.
    selectRecord(int(filled.back()));
    box(waterModeList, WaterModeList, 2);
    float bankX = 9.5f, bankY = 12.5f, level = groundAt(bankX, bankY);
    click(bankX, bankY);
    require(std::abs(number(fieldsOf(size_t(record))[7], 0) - std::round(level * 100) / 100) < .15f, "Clicking the bank did not set the surface there (" + doc.lines[size_t(record)] + " vs ground " + shortNumber(level) + "; " + utf8(notice) + ")");
    // Murk and wet banks on the picked pool, one Undo each.
    tool = 0;
    selectRecord(int(doc.records("WATER", map).back()));
    layout();
    require((GetWindowLongW(waterMurkSlider, GWL_STYLE) & WS_VISIBLE) && (GetWindowLongW(waterWetSlider, GWL_STYLE) & WS_VISIBLE), "A picked pool shows no Murk and Wet banks sliders");
    if (pictures)
      windowShot(L"outdoor-water-picked-window.bmp");
    undo = history.size();
    SendMessageW(waterMurkSlider, TBM_SETPOS, TRUE, 70);
    SendMessageW(window, WM_HSCROLL, MAKEWPARAM(TB_ENDTRACK, 0), reinterpret_cast<LPARAM>(waterMurkSlider));
    require(fieldsOf(size_t(record)).size() == 9 && fieldsOf(size_t(record))[8] == "0.7", "Murk did not write 0.7: " + doc.lines[size_t(record)]);
    SendMessageW(waterWetSlider, TBM_SETPOS, TRUE, 60);
    SendMessageW(window, WM_HSCROLL, MAKEWPARAM(TB_ENDTRACK, 0), reinterpret_cast<LPARAM>(waterWetSlider));
    require(wetFor(fieldsOf(size_t(record))) >= 0 && fieldsOf(size_t(wetFor(fieldsOf(size_t(record)))))[6] == "0.6", "Wet banks did not write a WET box round the pool");
    require(history.size() == undo + 2, "Murk and Wet banks are not one Undo each");
    // A corner handle resizes the pool.
    auto w = fieldsOf(size_t(record));
    float x2 = number(w[4], 0), y2 = number(w[5], 0);
    mouse(WM_LBUTTONDOWN, x2, y2);
    mouse(WM_MOUSEMOVE, x2 + 1, y2 + 1);
    mouse(WM_LBUTTONUP, x2 + 1, y2 + 1);
    w = fieldsOf(size_t(record));
    require(std::abs(number(w[4], 0) - (x2 + 1)) < .05f && std::abs(number(w[5], 0) - (y2 + 1)) < .05f, "The pool's corner handle did not resize it: " + doc.lines[size_t(record)]);
    roundTrip(L"outdoor-ashfall-water.rmc");
    passed.push_back("Water outdoors: a dragged pool sits under its lowest bank (surface " + shortNumber(number(pool[7], 0)) + "), Fill a hollow floods " +
                     std::to_string(filled.size() - waterBefore) + " boxes at one level, a bank click sets the surface, Murk and Wet banks sliders (WATER murk, a WET box) one Undo each, a corner handle resizes; engine-valid");

    if (pictures) {
      // Before and after in the engine: the zone as shipped, then the sculpted, planted and flooded map.
      float px = 3.f, py = 21.f;
      NativeDocument edited = doc;
      doc.loadText(pristine);
      picture(L"outdoor-ashfall-3d-before", px, py, groundAt(px, py) + 6.f, -45.f, -18.f);
      doc = std::move(edited);
      picture(L"outdoor-ashfall-3d-after", px, py, groundAt(px, py) + 6.f, -45.f, -18.f);
      int changed = differ(L"outdoor-ashfall-3d-before", L"outdoor-ashfall-3d-after");
      require(changed > 3000, "The engine picture did not change with the land, plants and water (" + std::to_string(changed) + " pixels)");
      passed.push_back("3D view: the engine draws the edited Ashfall map (" + std::to_string(changed) + " pixels changed); outdoor-ashfall-3d-before.ppm, outdoor-ashfall-3d-after.ppm");
    }

    // ---- 6. Indoors and outdoors in one campaign: a room on the land, a storey above it,
    // a map that opens onto an indoor one, its floor bands, picks and the seam.
    doc.newCampaign("Indoors and outdoors");
    doc.addRoom("Hall");
    path.clear();
    map = layer = 0, record = -1, selectedRecords.clear(), history.clear(), future.clear(), tool = 0;
    refresh(), layout();
    joinMapPassage = true;
    addAdjacentMap(3); // east, indoors like its neighbour
    require(map == 1, "The new map was not chosen");
    SendMessageW(window, WM_COMMAND, Outdoors, 0);
    require(onTerrain(), "Make outdoors did not give the east map land");
    // The seam: the land meets the hall's floor with a flat at -1 and steps up to 0.
    auto [ex, ey] = mapOrigin();
    bool seamFlat = false;
    for (auto i : doc.records("TERRAIN_FLAT")) {
      auto f = fieldsOf(i);
      seamFlat = seamFlat || (number(f[1], 0) <= ex + .01f && number(f[3], 0) >= ex + 6 && f[5] == "-1");
    }
    require(seamFlat, "Making the map outdoors did not level the land along its seam with the hall");
    float atSeam = groundAt(.5f, 12.5f);
    require(std::abs(atSeam - (-.5f)) < .3f, "The land at the seam is not walked at -0.5 (" + shortNumber(atSeam) + ")");
    int steps = 0;
    for (auto i : doc.records("STRUCT", 1)) {
      auto f = fieldsOf(i);
      steps += number(f[2], 9) < 3 && number(f[6], 1) == 0 && number(f[7], 0) <= .5f;
    }
    require(steps >= 3, "No steps were laid from the land up to the hall's floor");
    // A room on the land: walls on terrain, picked by a ray where they stand.
    tool = 1;
    autoDetail = false;
    gesture(8, 6, 15, 12);
    auto walls = doc.records("STRUCT", 1);
    require(walls.size() > size_t(steps) || [&] {
      for (int y = 6; y <= 12; ++y)
        for (int x = 8; x <= 15; ++x)
          if (doc.tile(1, 0, x, y) == '#')
            return true;
      return false;
    }(), "The Room tool made no walls on the land");
    tool = 0;
    // A storey above it: the floor bands stand on the ground under it, the land is never banded away.
    createFloorLayer();
    refresh();
    layer = 1;
    paintNativeFloor(9, 7, 14, 11, 15);
    for (int y = 7; y < 11; ++y)
      for (int x = 9; x < 14; ++x)
        doc.paint(1, 1, x, y, '=');
    layer = 0;
    refresh();
    std::istringstream bands(editorViewControl());
    int count = 0;
    bands >> count;
    std::vector<std::array<float, 3>> bandsRead(static_cast<size_t>(std::max(0, count)));
    for (auto &q : bandsRead) {
      bands >> q[0] >> q[1] >> q[2];
      unsigned mask;
      for (int r = 0; r < 24; ++r)
        bands >> mask;
    }
    float under = floorBandGround(), elevation = 0;
    for (auto &a : floorLayers())
      if (a.id == 1)
        elevation = a.elevation;
    require(count == 2 && std::abs(bandsRead[1][0] - (under + elevation)) < .05f && bandsRead[1][1] >= 999.f,
            "The 3D view's floor bands do not stand on the ground under the storey (" + shortNumber(bandsRead.size() > 1 ? bandsRead[1][0] : -1) + " vs " +
                shortNumber(under + elevation) + ")");
    float landTop = -1e9f;
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x)
        landTop = std::max(landTop, groundAt(x + .5f, y + .5f));
    require(bandsRead[0][1] > under || landTop < bandsRead[0][1], "The ground floor's band ends under the land");
    roundTrip(L"outdoor-mixed.rmc");
    // The hall (indoors) keeps its flat floor bands and plain picks.
    selectMap(0);
    require(!onTerrain() && std::abs(groundAt(5, 5) - layerElevation()) < .001f, "The indoor hall took terrain");
    float fx = 0, fy = 0;
    require(pointOnFloor(5, 5, 3, 1, 1, -1, 0, fx, fy) && std::abs(fx - 8) < .01f && std::abs(fy - 8) < .01f, "An indoor 3D pick no longer meets the floor plane");
    passed.push_back("Indoors and outdoors: + Map then Make outdoors levels the land at the seam (walked at " + shortNumber(atSeam) + ") with " +
                     std::to_string(steps) + " steps up to the hall's floor; a room built on the land; the storey's 3D floor band stands on the ground under it (" +
                     shortNumber(under + elevation) + " m) and the top band reaches the sky; the hall keeps flat picks; saved byte for byte and engine-valid");
    if (pictures) {
      selectMap(1);
      planShot(L"outdoor-mixed-plan.bmp");
      picture(L"outdoor-mixed-3d", 21.f, 21.f, groundAt(21, 21) + 5.f, -135.f, -15.f);
    }
    // Ashfall again as shipped: still byte for byte.
    openCampaign(ashfall);
    require(doc.text() == pristine, "Ashfall changed on a second open");
    path = keepPath;
  }
  int runOutdoorTest() {
    std::ofstream report(root / L"native-editor-outdoor-test.txt");
    try {
      verifyOutdoorRegressions(true);
      for (auto &line : outdoorPassed)
        report << "PASS: " << line << '\n';
      report << "PICTURES: .build/interaction/outdoor-*.bmp and outdoor-*.ppm\n";
      return 0;
    } catch (const std::exception &error) {
      for (auto &line : outdoorPassed)
        report << "PASS: " << line << '\n';
      report << "FAIL: " << error.what() << '\n';
      return 1;
    }
  }
