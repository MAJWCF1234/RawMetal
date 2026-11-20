  // ---- Multiplayer regression (NativeMultiplayer.inc): RawMetalEditor.exe --multiplayer-test (with
  // pictures in .build/interaction/multiplayer-*.bmp); --editing-test runs it without them. Writes
  // native-editor-multiplayer-test.txt.
  //  1. the Multiplayer page through the window's own commands: every switch and box, one Ctrl+Z
  //     each; the record written once, under CAMPAIGN, with FORMAT|2 put at the top;
  //  2. save and open again byte for byte, and the page reads the reopened file;
  //  3. the warning: deathmatch on and fewer than 4 spawns on its map says so, 4 and it goes;
  //  4. the Spawn tool through the plan's mouse: the ghost under the pointer (and under the 3D
  //     pointer) is the record a click writes, a figure in 3D; R turns it; the click writes the
  //     ghost's record field for field; one Ctrl+Z, Ctrl+Y;
  //  5. a picked spawn: R turns it, the team box, the arrows and Page Up nudge it, Delete removes
  //     it, each one Ctrl+Z;
  //  6. the engine reads the saved campaign: it validates it, and its exporter (--editor-export)
  //     writes the MULTIPLAYER and SPAWN records back as they are.
  // Earlier checks in --editing-test leave state behind (a parts search, the plan's fit, a tool), so
  // this starts from a known one: a new document, the plan fitted, the search cleared, Select.
  void verifyMultiplayer(bool pictures) {
    std::ofstream report(root / L"native-editor-multiplayer-test.txt");
    auto pass = [&](const std::string &line) { report << "PASS: " << line << "\n" << std::flush; };
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Multiplayer regression: " + message);
    };
    auto strip = [](std::string s) { if (!s.empty() && s.back() == '\r') s.pop_back(); return s; };
    NativeDocument blank;
    blank.newCampaign("Arena");
    blank.addRoom("Hall");
    blank.addRoom("Yard");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    fitPlan();
    if (modelSearch)
      SetWindowTextW(modelSearch, L"");
    const float savedSnap = snap;
    const bool savedSmart = smartSnap;
    snap = .25f, smartSnap = false;
    spawnTurn = 0, spawnTeam = 0;
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    selectRecord(-1);
    auto press = [&](int id) { SendMessageW(window, WM_COMMAND, id, 0); };
    auto choose = [&](HWND list, int id, int data) {
      for (LRESULT i = 0, n = SendMessageW(list, CB_GETCOUNT, 0, 0); i < n; ++i)
        if (SendMessageW(list, CB_GETITEMDATA, i, 0) == data) {
          SendMessageW(list, CB_SETCURSEL, i, 0);
          SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
          return true;
        }
      return false;
    };
    auto shown = [](HWND h) { return h && (GetWindowLongW(h, GWL_STYLE) & WS_VISIBLE) != 0; };
    auto recordText = [&] { const int at = multiplayerLine(); return at < 0 ? std::string() : strip(doc.lines[size_t(at)]); };

    // ---- 1. The page ----
    const auto empty = doc.text();
    press(MultiplayerTab);
    layout();
    require(multiplayerDock && shown(mpMapList) && shown(mpTimeList) && shown(GetDlgItem(window, MpDeathmatch)) && !shown(form),
            "the Multiplayer tab did not show the page in place of the inspector");
    require(multiplayerLine() < 0 && multiplayerChoice() == MultiplayerChoice{}, "a new map already has a MULTIPLAYER record");
    press(MpDeathmatch);
    const std::string off = "MULTIPLAYER|deathmatch=0|coop=1|dmmap=-1|maxplayers=8|fraglimit=20|timelimit=10|friendlyfire=0";
    require(recordText() == off, "turning deathmatch off wrote " + recordText());
    {
      std::string_view kind;
      const int at = multiplayerLine();
      require(at > 0 && NativeDocument::field(doc.lines[size_t(at - 1)], 0, kind) && kind == "CAMPAIGN", "the record is not right under CAMPAIGN");
      require(doc.format() == 2, "the record's named fields did not put FORMAT|2 in the file");
    }
    require(history.size() == 1, "one switch is not one Ctrl+Z");
    press(Undo);
    require(doc.text() == empty, "Ctrl+Z did not take the first change back byte for byte");
    press(Redo);
    require(recordText() == off, "Ctrl+Y did not put it back");
    press(MpDeathmatch);
    press(MpCoop);
    press(MpFriendlyFire);
    layout();
    require(choose(mpMapList, MpMapList, 1), "the deathmatch map list does not offer map 1 (Yard)");
    require(choose(mpPlayersList, MpPlayersList, 12), "the players list does not offer 12");
    require(choose(mpFragList, MpFragList, 30), "the frag list does not offer 30");
    require(choose(mpTimeList, MpTimeList, 150), "the time list does not offer 15 minutes");
    const std::string chosen = "MULTIPLAYER|deathmatch=1|coop=0|dmmap=1|maxplayers=12|fraglimit=30|timelimit=15|friendlyfire=1";
    require(recordText() == chosen, "the page wrote " + recordText());
    require(history.size() == 8, "every change on the page is not one Ctrl+Z (" + std::to_string(history.size()) + " steps for 8 changes)");
    require(std::count_if(doc.lines.begin(), doc.lines.end(), [](const std::string &l) { return l.starts_with("MULTIPLAYER"); }) == 1, "the page wrote a second record");
    press(Undo);
    require(recordText().find("|timelimit=10|") != std::string::npos, "Ctrl+Z did not take the time limit back");
    press(Redo);
    require(recordText() == chosen, "Ctrl+Y did not put the time limit back");
    require(SendMessageW(GetDlgItem(window, MpDeathmatch), WM_GETTEXTLENGTH, 0, 0) > 0 && multiplayerChoice().friendlyFire, "the switches do not say what is on");
    require(validateDocument(), "the engine refused the page's record");
    pass("the Multiplayer page: Deathmatch, Co-op and Friendly fire switches, the deathmatch map, players, frags and minutes lists; the record written once under CAMPAIGN with FORMAT|2; each change one Ctrl+Z; the engine reads it");

    // ---- 2. Save and open again ----
    const auto folder = root / L".build";
    fs::create_directories(folder);
    const auto file = folder / L"multiplayer-test.rmc";
    {
      doc.save(file);
      NativeDocument again;
      again.open(file);
      require(again.text() == doc.text(), "the map did not come back byte for byte");
      adoptDocument(std::move(again), file, false);
      press(MultiplayerTab);
      layout();
      auto data = [&](HWND list) { return int(SendMessageW(list, CB_GETITEMDATA, SendMessageW(list, CB_GETCURSEL, 0, 0), 0)); };
      require(data(mpMapList) == 1 && data(mpPlayersList) == 12 && data(mpFragList) == 30 && data(mpTimeList) == 150 && multiplayerChoice().friendlyFire &&
                  !multiplayerChoice().coop,
              "the page did not read the reopened file's settings");
    }
    pass("save and open again: byte for byte, and the page shows the reopened settings");

    // ---- 3. The warning, on the deathmatch map (map 1, Yard, with no spawns) ----
    auto warned = [&] {
      layout();
      return std::any_of(captions.begin(), captions.end(), [](const auto &c) { return c.second.starts_with(L"⚠"); });
    };
    require(!multiplayerWarning().empty() && warned(), "no warning with deathmatch on and no spawns on its map");
    press(MpDeathmatch);
    require(multiplayerWarning().empty() && !warned(), "a warning with deathmatch off");
    press(MpDeathmatch);
    require(choose(mpMapList, MpMapList, -1), "the deathmatch map list does not offer the start map");
    require(recordText().find("|dmmap=-1|") != std::string::npos, "the start map is not written as dmmap=-1");
    pass("the page warns when deathmatch is on and its map has fewer than 4 spawns, and not when it is off");

    // ---- 4. The Spawn tool on the plan ----
    press(MpPlaceSpawns);
    layout();
    require(tool == SpawnTool && !multiplayerDock, "Place spawns did not take up the Spawn tool");
    require(map == 0, "the test expects the start map on the plan");
    auto mouse = [&](UINT message, float x, float y, WPARAM keys = MK_LBUTTON) {
      auto v = planView();
      SendMessageW(plan, message, keys, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    hoverInside = true;
    mouse(WM_MOUSEMOVE, 9.f, 9.f, 0);
    mouse(WM_MOUSEMOVE, 6.3f, 7.2f, 0);
    require(ghostLines.size() == 1, "no spawn ghost under the pointer (" + std::to_string(ghostLines.size()) + " lines)");
    auto g = NativeDocument::fields(ghostLines[0]);
    require(g[0] == "SPAWN" && g[1] == "0" && number(g[2], 0) == 6.25f && number(g[3], 0) == 7.25f && g[5] == "0" && g[6] == "0",
            "the ghost is not the spawn the click makes: " + ghostLines[0]);
    require(ghostGuideList.size() == spawnSegments(0, 0, 0, 0).size(), "the 3D view was not given the ghost's figure (" + std::to_string(ghostGuideList.size()) + " lines)");
    require(editorViewControl().find(" 4\n") != std::string::npos, "the ghost's figure did not go to the 3D view");
    {
      MSG key{plan, WM_KEYDOWN, 'R', 0};
      shortcut(key);
    }
    require(spawnTurn == 1 && ghostLines.size() == 1 && NativeDocument::fields(ghostLines[0])[5] == "45", "R did not turn the ghost an eighth");
    // In 3D: a ray from above the hall straight down at (10, 10) puts the ghost there.
    hoverInside = false;
    ghostHover("HOVER 10 10 5 0 0 -1");
    require(ghostLines.size() == 1 && std::abs(number(NativeDocument::fields(ghostLines[0])[2], 0) - 10) < .3f &&
                std::abs(number(NativeDocument::fields(ghostLines[0])[3], 0) - 10) < .3f,
            "the ghost does not follow the 3D pointer");
    ghostHover("NONE");
    hoverInside = true;
    mouse(WM_MOUSEMOVE, 6.3f, 7.2f, 0);
    const auto ghost = ghostLines;
    require(ghost.size() == 1, "the ghost did not come back to the plan's pointer");
    const auto before = doc.lines;
    const auto beforeText = doc.text();
    mouse(WM_LBUTTONDOWN, 6.3f, 7.2f);
    mouse(WM_LBUTTONUP, 6.3f, 7.2f);
    auto made = newLines(before, doc.lines);
    require(made.size() == 1, "the click made " + std::to_string(made.size()) + " records");
    require(strip(made[0]) == strip(ghost[0]), "the click wrote " + made[0] + ", the ghost showed " + ghost[0]);
    require(strip(made[0]) == "SPAWN|map=0|x=6.25|y=7.25|z=0|yaw=45|team=0", "the spawn is not written as named fields: " + made[0]);
    require(spawnCount(0) == 1 && spawnPicked(), "the placed spawn is not picked");
    require(ghostLines.empty() && ghostGuideList.empty(), "a ghost was left after placing");
    const auto placed = doc.text();
    press(Undo);
    require(doc.text() == beforeText, "one Ctrl+Z did not take the spawn back");
    press(Redo);
    require(doc.text() == placed, "Ctrl+Y did not put the spawn back");
    pass("the Spawn tool: the ghost under the plan's pointer is the record a click writes (and a figure in the 3D view), it follows the 3D pointer, R turns it an eighth, the click writes it field for field as named fields; one Ctrl+Z, Ctrl+Y");

    // ---- 5. A picked spawn ----
    auto spawnAt = [&] { return doc.records("SPAWN", 0).front(); };
    selectRecord(int(spawnAt()));
    require(spawnPicked(), "the spawn cannot be picked");
    auto step = [&](auto &&act, const std::string &what, auto &&check) {
      const auto was = doc.text();
      const auto steps = history.size();
      act();
      require(check(NativeDocument::fields(doc.lines[spawnAt()])), what + " did not do it: " + doc.lines[spawnAt()]);
      require(history.size() == steps + 1, what + " is not one Ctrl+Z");
      press(Undo);
      require(doc.text() == was, "Ctrl+Z did not undo " + what);
      press(Redo);
      require(check(NativeDocument::fields(doc.lines[spawnAt()])), "Ctrl+Y did not redo " + what);
      selectRecord(int(spawnAt()));
    };
    step([&] { MSG key{plan, WM_KEYDOWN, 'R', 0}; shortcut(key); }, "R on the picked spawn", [](const std::vector<std::string> &f) { return f[5] == "90"; });
    step([&] { layout(); SendMessageW(spawnTeamList, CB_SETCURSEL, 2, 0); SendMessageW(window, WM_COMMAND, MAKEWPARAM(SpawnTeamList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(spawnTeamList)); },
         "the team box", [](const std::vector<std::string> &f) { return f[6] == "2"; });
    step([&] { MSG key{plan, WM_KEYDOWN, VK_RIGHT, 0}; shortcut(key); }, "the right arrow", [](const std::vector<std::string> &f) { return std::abs(number(f[2], 0) - 6.5f) < 1e-4f; });
    step([&] { MSG key{plan, WM_KEYDOWN, VK_PRIOR, 0}; shortcut(key); }, "Page Up", [](const std::vector<std::string> &f) { return std::abs(number(f[4], 0) - .25f) < 1e-4f; });
    require(strip(doc.lines[spawnAt()]) == "SPAWN|map=0|x=6.5|y=7.25|z=0.25|yaw=90|team=2", "the picked spawn is " + doc.lines[spawnAt()]);
    require(validateDocument(), "the engine refused the turned, teamed and nudged spawn");
    // Three more by clicks: four on the deathmatch map and the warning goes.
    press(ToolBase + SpawnTool);
    require(!multiplayerWarning().empty(), "no warning with one spawn on the deathmatch map");
    for (auto [x, y] : {std::pair{17.f, 7.f}, std::pair{17.f, 17.f}, std::pair{6.f, 17.f}}) {
      mouse(WM_MOUSEMOVE, x + .6f, y + .6f, 0);
      mouse(WM_MOUSEMOVE, x, y, 0);
      mouse(WM_LBUTTONDOWN, x, y);
      mouse(WM_LBUTTONUP, x, y);
    }
    require(spawnCount(0) == 4 && multiplayerWarning().empty(), "four spawns on the deathmatch map still warn (" + std::to_string(spawnCount(0)) + " spawns)");
    {
      selectRecord(int(doc.records("SPAWN", 0).back()));
      const auto was = doc.text();
      MSG key{plan, WM_KEYDOWN, VK_DELETE, 0};
      shortcut(key);
      require(spawnCount(0) == 3 && !multiplayerWarning().empty(), "Delete did not remove the picked spawn");
      press(Undo);
      require(doc.text() == was, "Ctrl+Z did not bring the deleted spawn back");
    }
    pass("a picked spawn: R turns it, the team box gives it a team, the right arrow and Page Up nudge it, Delete removes it, each one Ctrl+Z; four spawns and the warning goes");

    // ---- 6. The engine reads the saved campaign ----
    {
      saveTo(file);
      NativeDocument again;
      again.open(file);
      require(again.text() == doc.text(), "the map with spawns did not come back byte for byte");
      require(validateDocument(), "the engine refused the saved campaign");
      const auto exported = fs::path(file.wstring() + L".exported.rmc");
      std::error_code ec;
      fs::remove(exported, ec);
      std::wstring line = L"\"" + engineExecutable().wstring() + L"\" \"--editor-export=" + file.wstring() + L"\"";
      STARTUPINFOW si{sizeof(si)};
      PROCESS_INFORMATION p{};
      require(CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, folder.c_str(), &si, &p) != 0, "the engine could not be started");
      if (WaitForSingleObject(p.hProcess, 240000) == WAIT_TIMEOUT)
        TerminateProcess(p.hProcess, 9);
      DWORD code = 1;
      GetExitCodeProcess(p.hProcess, &code);
      CloseHandle(p.hThread), CloseHandle(p.hProcess);
      require(code == 0 && fs::exists(exported), "the engine could not read and export the saved campaign");
      NativeDocument back;
      back.open(exported);
      std::vector<std::string> mine, theirs;
      for (const auto &l : doc.lines)
        if (l.starts_with("MULTIPLAYER") || l.starts_with("SPAWN|"))
          mine.push_back(strip(l));
      for (const auto &l : back.lines)
        if (l.starts_with("MULTIPLAYER") || l.starts_with("SPAWN|"))
          theirs.push_back(strip(l));
      require(mine.size() == 5 && mine == theirs, "the engine's export wrote " + std::to_string(theirs.size()) + " multiplayer records, not the saved " + std::to_string(mine.size()));
      pass("the engine reads the saved campaign (it validates every value) and its exporter writes the MULTIPLAYER record and all 4 spawns back as they are");
    }
    if (pictures)
      multiplayerPictures(report);
    // A deathmatch starts with fists: the page says so until a gun (a MAC-11, handgun or shotgun drop) lies on its map.
    {
      require(multiplayerChoice().deathmatch && !gunWarning().empty(), "no warning for a deathmatch map with no gun lying on it");
      doc.add({"PICKUP", "0", "0", "8", "8"});
      require(!gunWarning().empty(), "a health box alone counts as a gun");
      doc.add({"PICKUP", "0", "5", "9", "8"});
      require(gunWarning().empty(), "a handgun drop on the map still warns that nobody has a gun");
      pass("the Multiplayer page warns that a deathmatch starts with fists only until a MAC-11, handgun or shotgun drop is on its map");
    }
    snap = savedSnap, smartSnap = savedSmart;
    multiplayerDock = false;
    spawnTurn = 0, spawnTeam = 0;
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    dirty = false;
  }
  // ---- Pictures (.build/interaction, --multiplayer-test): the whole Builder at 2300 x 1000 with the
  // engine's 3D picture in its pane: the Multiplayer page, then the Spawn tool with a ghost.
  void multiplayerPictures(std::ofstream &report) {
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    auto capture = [&](const std::wstring &name, const std::string &film) {
      std::wstring ppm;
      if (fs::exists(engineExecutable())) {
        auto session = folder / (name + L"-session");
        fs::create_directories(session);
        doc.save(session / L"working.rmc");
        std::ofstream(session / L"control.txt") << "1 " << map << " 0 0 0 0 0 0 0\n" << editorViewControl();
        std::ofstream(session / L"look.txt") << "LOOK 0 0 0 0\n";
        std::ofstream(session / L"film.txt") << film;
        const auto picture = folder / (name + L"-3d.ppm");
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
      RECT was{};
      GetWindowRect(window, &was);
      SetWindowPos(window, nullptr, -6000, 0, 2300, 1000, SWP_NOZORDER | SWP_NOACTIVATE);
      ShowWindow(window, SW_SHOWNOACTIVATE);
      layout();
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
      BITMAPINFOHEADER head{sizeof(BITMAPINFOHEADER), r.right, -r.bottom, 1, 32, BI_RGB};
      std::ofstream out(folder / (name + L".bmp"), std::ios::binary);
      BITMAPFILEHEADER file{};
      file.bfType = 0x4D42, file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), file.bfSize = DWORD(file.bfOffBits + bits.size());
      out.write(reinterpret_cast<const char *>(&file), sizeof file);
      out.write(reinterpret_cast<const char *>(&head), sizeof head);
      out.write(reinterpret_cast<const char *>(bits.data()), std::streamsize(bits.size()));
      DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(window, screen);
      ShowWindow(window, SW_HIDE);
      SetWindowPos(window, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    // The camera at the hall's south end, looking north and down over the spawns.
    // A film camera hides the guides, so the picture looks from the player start instead, moved for
    // it to the hall's south end facing north (the document is put back afterwards).
    const std::string film = "FILM 0\n";
    const auto keep = doc.text();
    for (auto i : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 20)
        f[5] = "11.5", f[6] = "22.5", f[20] = "-1.570796", doc.replace(i, f);
    }
    selectRecord(-1);
    press(MultiplayerTab);
    capture(L"multiplayer-page", film);
    report << "multiplayer-page.bmp: the Builder with the Multiplayer page open (modes, deathmatch map, players, frags, minutes, friendly fire, spawn count), the hall's spawns as figures on the plan and in the 3D pane\n";
    // The Spawn tool in hand, a ghost at the pointer turned by R, a placed spawn picked.
    multiplayerDock = false;
    press(ToolBase + SpawnTool);
    selectRecord(int(doc.records("SPAWN", 0).front()));
    spawnTurn = 6;
    hoverInside = true, hoverX = 12.f, hoverY = 12.f;
    refreshGhost();
    capture(L"multiplayer-spawn", film);
    report << "multiplayer-spawn.bmp: the Spawn tool: its team row and spawn count, a picked spawn (team 2), and the ghost figure at the pointer facing north (R), on the plan and in 3D\n";
    hoverInside = false;
    clearGhost();
    doc.loadText(keep);
    publish(false);
  }
  void press(int id) { SendMessageW(window, WM_COMMAND, id, 0); }
