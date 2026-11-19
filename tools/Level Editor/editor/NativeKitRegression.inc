  // ---- Kits regression (NativeKitMethods.inc): RawMetalEditor.exe --kit-test (with pictures);
  // --editing-test runs it without them. Writes native-editor-kit-test.txt.
  //  1. every kit resolves its models, places as one PREFAB_PLACE with its block, and the engine takes
  //     all of them on one map; one Ctrl+Z takes each back whole;
  //  2. through the tools: the Kits tab tile holds the kit, R turns it, a click on the plan sets it down
  //     on the half metre turned; Ctrl+Z and Ctrl+Y;
  //  3. R turns the placed kit a quarter (one Ctrl+Z);
  //  4. save and open again: byte for byte, and the engine reads it;
  //  5. Take apart: the pieces loose where the game draws them (a desk checked by hand), the block gone,
  //     the engine reads it, one Ctrl+Z puts the kit back.
  void verifyKits(bool pictures) {
    fitPlan();
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Kit regression: " + message);
    };
    std::ofstream report(root / L"native-editor-kit-test.txt");
    auto pass = [&](const std::string &line) { report << "PASS: " << line << "\n" << std::flush; };
    NativeDocument blank;
    blank.newCampaign("Kits");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    const auto empty = doc.text();
    // 1. Every kit, all on one map, then each taken back.
    require(KitCount >= 21, "fewer kits than the 21 offered");
    for (int k = 0; k < KitCount; ++k) {
      require(!kitBody(k).empty(), std::string("kit ") + kitDefs[k].key + " has no pieces");
      require(placeKit(k, 12, 12, k & 3), std::string("kit ") + kitDefs[k].key + " was not placed");
      require(doc.records("PREFAB_PLACE", map).size() == size_t(k + 1), std::string("kit ") + kitDefs[k].key + " is not one PREFAB_PLACE");
      require(prefabBody(doc, kitDefs[k].key).size() == kitBody(k).size(), std::string("kit ") + kitDefs[k].key + "'s block was not written whole");
    }
    require(validateDocument(), "the engine refused the kits on one map");
    for (int k = KitCount - 1; k >= 0; --k) {
      SendMessageW(window, WM_COMMAND, Undo, 0);
      require(doc.records("PREFAB_PLACE", map).size() == size_t(k) && prefabBody(doc, kitDefs[k].key).empty(),
              std::string("one Ctrl+Z did not take kit ") + kitDefs[k].key + " back whole");
    }
    require(doc.text() == empty, "undoing every kit did not give the empty map back byte for byte");
    pass("all " + std::to_string(KitCount) + " kits resolve their models, each sets down as one PREFAB_PLACE with its block, the engine takes them all on one map, and one Ctrl+Z takes each back whole");
    // 2. The Kits tab, R and a click.
    const int office = kitIndex("kit_office");
    require(office >= 0, "the office kit is missing");
    SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
    layout();
    partTab = parts::Kits;
    refreshParts();
    require(int(partsGrid.items.size()) == KitCount, "the Kits tab does not show every kit");
    int tile = -1;
    for (size_t i = 0; i < partsGrid.items.size(); ++i)
      if (partsGrid.items[i].key == "kit-kit_office")
        tile = int(i);
    require(tile >= 0, "the office kit has no tile");
    pickPart(tile, false);
    require(tool == 8 && kitInHand == office && kitTurn == 0, "the office tile did not put the kit in hand");
    {
      MSG key{plan, WM_KEYDOWN, 'R', 0};
      shortcut(key);
    }
    require(kitTurn == 1, "R did not turn the held kit");
    const auto beforeClick = doc.text();
    gesture(12.3f, 11.8f, 12.3f, 11.8f);
    auto places = doc.records("PREFAB_PLACE", map);
    require(places.size() == 1, "a click did not set the held kit down");
    auto f = NativeDocument::fields(doc.lines[places[0]]);
    require(decoded(f[2]) == "kit_office" && number(f[3], 0) == 12.5f && number(f[4], 0) == 12.f && std::abs(number(f[6], 0) - 1.5708f) < 1e-3f,
            "the kit did not land on the half metre, turned a quarter (" + doc.lines[places[0]] + ")");
    require(record == int(places[0]), "the placed kit is not picked");
    require(validateDocument(), "the engine refused the placed office");
    const auto placed = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == beforeClick, "Ctrl+Z did not take the clicked kit back");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == placed, "Ctrl+Y did not put it back");
    pass("the Kits tab's tile holds the office, R turns it, a click sets it down on the half metre turned a quarter and picked; Ctrl+Z and Ctrl+Y");
    // 3. R turns the placed kit.
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    selectRecord(int(doc.records("PREFAB_PLACE", map)[0]));
    rotateSelected();
    f = NativeDocument::fields(doc.lines[doc.records("PREFAB_PLACE", map)[0]]);
    require(std::abs(std::abs(number(f[6], 0)) - 3.14159f) < 1e-3f, "R did not turn the placed kit a quarter (" + f[6] + ")");
    require(validateDocument(), "the engine refused the turned office");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == placed, "one Ctrl+Z did not undo the turn");
    pass("R turns a placed kit a quarter, the engine reads it, one Ctrl+Z turns it back");
    // 4. Save and open again.
    {
      const auto file = root / L".build" / L"kit-test.rmc";
      fs::create_directories(file.parent_path());
      doc.save(file);
      NativeDocument again;
      again.open(file);
      require(again.text() == doc.text(), "the map with a kit did not come back byte for byte");
      require(again.records("PREFAB_PLACE", map).size() == 1 && !prefabBody(again, "kit_office").empty(), "the reopened map lost its kit");
      require(validateDocument(), "the engine refused the saved map");
      pass("save and open again: the kit's block and placement come back byte for byte; the engine reads the file");
    }
    // 5. Take apart.
    selectRecord(int(doc.records("PREFAB_PLACE", map)[0]));
    require(prefabPlacePicked(), "the placed kit cannot be picked");
    const auto pieces = kitBody(office).size();
    const auto fixturesBefore = doc.records("FIXTURE", map).size();
    require(takeKitApart(), "Take apart refused the office");
    require(doc.records("PREFAB_PLACE", map).empty() && prefabBody(doc, "kit_office").empty(), "Take apart left the placement or its block");
    require(selectedRecords.size() == pieces, "Take apart did not pick all " + std::to_string(pieces) + " pieces (" + std::to_string(selectedRecords.size()) + ")");
    // The desk at (-1.45, -0.55) in the kit, turned a quarter at (12.5, 12): x' = -y, y' = x.
    bool desk = false;
    for (auto i : doc.records("FIXTURE", map)) {
      auto g = NativeDocument::fields(doc.lines[i]);
      desk |= g[2] == std::to_string(retro::model::desk) && std::abs(number(g[3], 0) - 13.05f) < 1e-3f &&
              std::abs(number(g[4], 0) - 10.55f) < 1e-3f && std::abs(number(g[9], 0) - 1.5708f) < 1e-3f;
    }
    require(desk, "the office's first desk is not where the game draws it (13.05, 10.55, a quarter turn)");
    require(doc.records("FIXTURE", map).size() > fixturesBefore, "Take apart made no loose furniture");
    require(validateDocument(), "the engine refused the loose pieces");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == placed, "one Ctrl+Z did not put the kit back together");
    pass("Take apart: " + std::to_string(pieces) + " loose pieces, picked, where the game draws them (the desk checked by hand), the block gone, the engine reads them; one Ctrl+Z puts the kit back");
    if (pictures)
      kitPictures(report);
    dirty = false;
  }
  // ---- Pictures (.build/interaction, --kit-test): the Kits tab with a kit held over the plan, and
  // the new parts' and the kits' pages of the browser at full size.
  void kitPictures(std::ofstream &report) {
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    // The pictures of the kits and the 1.5.6 parts first, drawn by the engine as on a first start.
    makeThumbnails(true);
    thumbs.rescan();
    auto bmp = [&](const fs::path &file, int w, int h, std::vector<unsigned char> &bits) {
      BITMAPINFOHEADER info{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
      std::ofstream out(file, std::ios::binary);
      BITMAPFILEHEADER head{};
      head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), head.bfSize = DWORD(head.bfOffBits + bits.size());
      out.write(reinterpret_cast<const char *>(&head), sizeof head);
      out.write(reinterpret_cast<const char *>(&info), sizeof info);
      out.write(reinterpret_cast<const char *>(bits.data()), std::streamsize(bits.size()));
    };
    // A browser page alone, every tile at 150 px.
    auto page = [&](const std::wstring &name, int tab) {
      partTab = tab;
      refreshParts();
      RECT was{};
      GetWindowRect(partsGrid.hwnd, &was);
      MapWindowPoints(nullptr, GetParent(partsGrid.hwnd), reinterpret_cast<POINT *>(&was), 2);
      const int gw = 1500;
      partsGrid.cell = 150;
      SetWindowPos(partsGrid.hwnd, nullptr, 0, 0, gw, 4000, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
      partsGrid.scroll = 0;
      partsGrid.layoutSpots();
      const int gh = std::clamp(partsGrid.content, 200, 4000);
      HDC screen = GetDC(partsGrid.hwnd), dc = CreateCompatibleDC(screen);
      auto bitmap = CreateCompatibleBitmap(screen, gw, gh);
      auto old = SelectObject(dc, bitmap);
      RECT r{0, 0, gw, gh};
      partsGrid.paint(dc, r, ui::Paper, ui::Field, ui::PaperEdge, ui::SageEdge, ui::Glow, ui::Ink, ui::InkSoft);
      BITMAPINFO info{};
      info.bmiHeader = {sizeof(BITMAPINFOHEADER), gw, -gh, 1, 32, BI_RGB};
      std::vector<unsigned char> bits(size_t(gw) * size_t(gh) * 4);
      SelectObject(dc, old);
      GetDIBits(dc, bitmap, 0, UINT(gh), bits.data(), &info, DIB_RGB_COLORS);
      bmp(folder / (name + L".bmp"), gw, gh, bits);
      DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(partsGrid.hwnd, screen);
      partsGrid.cell = 100;
      SetWindowPos(partsGrid.hwnd, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
      partsGrid.layoutSpots();
    };
    SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
    layout();
    for (auto [name, tab] : std::initializer_list<std::pair<const wchar_t *, int>>{
             {L"kits-page", parts::Kits}, {L"parts-page-furniture", parts::Furniture}, {L"parts-page-kitchen", parts::Kitchen},
             {L"parts-page-storage", parts::Storage}, {L"parts-page-lights", parts::Lights}, {L"parts-page-building", parts::Building},
             {L"parts-page-outdoor", parts::Outdoor}, {L"parts-page-horror", parts::Horror}, {L"parts-page-props", parts::Props},
             {L"parts-page-machines", parts::Machines}, {L"parts-page-lab", parts::Lab}, {L"parts-page-nature", parts::Nature}})
      page(name, tab);
    report << "kits-page.bmp, parts-page-*.bmp: the Kits tab and the parts tabs alone, every tile at 150 px\n";
    // The whole Builder at 2300 x 1000: the Kits tab, the office held over the plan, its 3D pane the
    // engine's picture of the map with the kits already set down.
    const auto keep = doc.text();
    const int k1 = kitIndex("kit_lab"), k2 = kitIndex("kit_kitchen");
    placeKit(k1, 5, 12, 0);
    placeKit(k2, 19, 12, 0);
    for (auto i : doc.records("MAP", map)) { // a tall hall, so the picture looks down into the rooms
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 9)
        f[9] = "9", doc.replace(i, f);
    }
    std::wstring ppm;
    if (fs::exists(engineExecutable())) {
      auto session = folder / L"kits-builder-session";
      fs::create_directories(session);
      doc.save(session / L"working.rmc");
      std::ofstream(session / L"control.txt") << "1 " << map << " 0 0 0 0 0 0 0\n" << editorViewControl();
      std::ofstream(session / L"look.txt") << "LOOK 0 0 0 0\n";
      std::ofstream(session / L"film.txt") << "FILM 1 12 23 8.2 -90 -38 84\n";
      const auto picture = folder / L"kits-builder-3d.ppm";
      std::error_code ec;
      fs::remove(picture, ec);
      std::wstring line = L"\"" + engineExecutable().wstring() + L"\" \"--editor-pack=" + (session / L"working.rmc").wstring() + L"\" \"--editor-control=" +
                          (session / L"control.txt").wstring() + L"\" \"--editor-status=" + (session / L"status.txt").wstring() +
                          L"\" --editor-frames=60 \"--editor-capture=" + picture.wstring() + L"\" --editor-hidden";
      STARTUPINFOW si{sizeof(si)};
      PROCESS_INFORMATION p{};
      if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, session.c_str(), &si, &p)) {
        if (WaitForSingleObject(p.hProcess, 300000) == WAIT_TIMEOUT)
          TerminateProcess(p.hProcess, 9);
        CloseHandle(p.hThread), CloseHandle(p.hProcess);
      }
      if (fs::exists(picture))
        ppm = picture.wstring();
    }
    partTab = parts::Kits;
    holdKit(kitIndex("kit_office"));
    turnKitInHand();
    hoverX = 12.f, hoverY = 4.5f, hoverInside = true;
    {
      RECT was{};
      GetWindowRect(window, &was);
      SetWindowPos(window, nullptr, -6000, 0, 2300, 1000, SWP_NOZORDER | SWP_NOACTIVATE);
      ShowWindow(window, SW_SHOWNOACTIVATE);
      layout();
      refreshParts();
      RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
      for (ULONGLONG until = GetTickCount64() + 400; GetTickCount64() < until;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
          TranslateMessage(&msg), DispatchMessageW(&msg);
        Sleep(10);
      }
      RECT r{};
      GetClientRect(window, &r);
      HDC screen = GetDC(window), dc = CreateCompatibleDC(screen);
      auto bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom);
      auto old = SelectObject(dc, bitmap);
      PrintWindow(window, dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
      BITMAPINFO info{};
      info.bmiHeader = {sizeof(BITMAPINFOHEADER), r.right, -r.bottom, 1, 32, BI_RGB};
      std::vector<unsigned char> bits(size_t(r.right) * size_t(r.bottom) * 4);
      SelectObject(dc, old);
      GetDIBits(dc, bitmap, 0, UINT(r.bottom), bits.data(), &info, DIB_RGB_COLORS);
      std::ifstream in(fs::path(ppm), std::ios::binary);
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
      bmp(folder / L"kits-builder.bmp", r.right, r.bottom, bits);
      DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(window, screen);
      ShowWindow(window, SW_HIDE);
      SetWindowPos(window, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    report << "kits-builder.bmp: the Builder with the Kits tab open, the office held over the plan turned a quarter (its ghost and front arrow), a lab and a kitchen already set down (dotted), the 3D pane the engine's picture of them\n";
    kitInHand = -1;
    doc.loadText(keep);
    publish(false);
  }
