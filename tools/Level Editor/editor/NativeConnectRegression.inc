  // The Connect tool (NativeConnectTool.inc) driven through the plan's mouse and its box: a crate hung by a rope
  // dragged to the ceiling, a chain made by two clicks, a plank put on a hinge where it balances (a seesaw), a
  // gate on an upright pin, two crates over a pulley, a lamp hung by a click; the engine reading every step;
  // undo and redo exact; a crate deleted taking its joints and renumbering the rest; save and reopen byte for
  // byte. --editing-test runs it; --connect-test runs it alone with the pictures (native-editor-connect-test.txt,
  // .build/interaction/connect-*.bmp: the plan's ghost line mid-drag, and the 3D pane the engine's picture of the
  // joint the drag makes, as the live pane's drag preview shows it).
  void verifyConnectRegressions(bool pictures) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Connect regression: " + message);
    };
    std::ofstream report(root / L"native-editor-connect-test.txt");
    NativeDocument blank;
    blank.newCampaign("Connect");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    remember();
    const std::string m = std::to_string(map);
    auto prop = [&](int slot, float x, float y, float w, float d, float h) {
      addAuthored({"CLUTTER", m, "7", nativeNumber(x), nativeNumber(y), "0", "0", std::to_string(slot), nativeNumber(w), nativeNumber(d), nativeNumber(h), "0", "0", ""});
    };
    prop(retro::model::wooden_crate, 6, 6, .5f, .5f, .5f);       // 0: crate A
    prop(retro::model::wooden_crate, 10, 6, .5f, .5f, .5f);      // 1: crate B
    prop(retro::model::packing_crate, 6, 12, 2, .4f, .05f);      // 2: a plank
    prop(retro::model::packing_crate, 14, 12, 1, .05f, 1);       // 3: a gate panel
    publish();
    require(validateDocument(), "the engine refused the props");
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
    auto click = [&](float x, float y) { mouse(WM_LBUTTONDOWN, x, y), mouse(WM_LBUTTONUP, x, y); };
    auto choose = [&](int kind) {
      SendMessageW(connectKindList, CB_SETCURSEL, kind, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(ConnectKindList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(connectKindList));
    };
    auto joints = [&] { return doc.records("JOINT", map); };
    auto last = [&] { return NativeDocument::fields(doc.lines[joints().back()]); };
    auto named = [&](const std::string &key) {
      for (auto &[k, v] : NativeDocument::named(doc.lines[joints().back()]))
        if (k == key)
          return v;
      return std::string("-");
    };
    const auto start = doc.text();
    SendMessageW(window, WM_COMMAND, ToolBase + ConnectTool, 0);
    require(tool == ConnectTool, "the Connect tool did not take");
    // A rope: from crate A to a point under the ceiling.
    choose(ConnectRope);
    drag(6, 6, 6.5f, 8);
    require(joints().size() == 1, "a drag from a crate made " + std::to_string(joints().size()) + " JOINT records");
    {auto f = last();require(f.size() == 11 && f[2] == "rope" && f[3] == "0" && f[4] == "-1" && f[8] == "6.5" && f[9] == "8" && std::fabs(std::stof(f[10]) - creationCeiling()) < .01f,
      "the rope does not run from crate A's top to the ceiling over the drag's end: " + doc.lines[joints().back()]);}
    require(validateDocument(), "the engine refused the rope");
    report << "PASS: a drag from a crate to open floor hangs it by a rope from the ceiling there; the engine reads it\n" << std::flush;
    // A chain: click crate B, then click where it hangs from.
    choose(ConnectChain);
    click(10, 6);
    require(connectPending && joints().size() == 1, "a click on crate B should only start a connection");
    click(11, 7);
    require(!connectPending && joints().size() == 2 && last()[2] == "chain" && last()[3] == "1", "the second click did not hang crate B on a chain: " + doc.lines[joints().back()]);
    require(validateDocument(), "the engine refused the chain");
    report << "PASS: a click on a crate and a click under the ceiling hang it on a chain\n" << std::flush;
    // A hinge across the plank where it balances: a seesaw.
    choose(ConnectHinge);
    drag(5.4f, 12, 6.0f, 12);
    {auto f = last();require(f[2] == "hinge" && f[3] == "2" && f[4] == "-1" && named("axis") == "0,1,0" && named("min") == "-20",
      "the plank's hinge is not under it, across its length: " + doc.lines[joints().back()]);}
    // A gate on an upright pin at its end nearest the drag's end.
    drag(14, 12, 14.7f, 12.4f);
    {auto f = last();require(f[2] == "hinge" && f[3] == "3" && named("axis") == "z" && std::fabs(std::stof(f[5]) - 14.5f) < .01f,
      "the gate's pin is not upright at its east end: " + doc.lines[joints().back()]);}
    require(validateDocument(), "the engine refused the hinges");
    report << "PASS: a hinge dragged across a plank balances it (a seesaw); one dragged off a standing panel stands its pin at that end (a gate)\n" << std::flush;
    // A pulley between the two crates; refused from a crate to nothing.
    choose(ConnectPulley);
    {const auto text = doc.text();drag(6, 6, 8, 9);require(doc.text() == text, "a pulley dragged from a crate to nothing should write nothing");}
    drag(6, 6, 10, 6);
    require(last()[2] == "pulley" && last()[3] == "0" && last()[4] == "1" && named("over") != "-" && named("over2") != "-", "the pulley is not between the crates over two wheels: " + doc.lines[joints().back()]);
    require(validateDocument(), "the engine refused the pulley");
    report << "PASS: a pulley joins two crates over a wheel above each; from a crate to nothing it is refused\n" << std::flush;
    // A lamp hung by a click.
    choose(ConnectLamp);
    const size_t lights = doc.records("LIGHT", map).size(), props = doc.records("CLUTTER", map).size();
    click(18, 6);
    require(doc.records("CLUTTER", map).size() == props + 1 && doc.records("LIGHT", map).size() == lights + 1 && last()[2] == "chain" && last()[3] == std::to_string(props),
      "a click with Hanging lamp did not hang a lamp on a chain");
    {auto light = NativeDocument::fields(doc.lines[doc.records("LIGHT", map).back()]);require(light.size() == 27 && light[26] == std::to_string(props), "the lamp's light does not ride the lamp");}
    require(validateDocument(), "the engine refused the hanging lamp");
    report << "PASS: Hanging lamp: a click under the ceiling hangs a caged lamp on a chain, its light riding it\n" << std::flush;
    // Undo and redo, exactly.
    const auto made = doc.text();
    for (int k = 0; k < 24 && doc.text() != start; ++k)
      SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == start, "undo did not take every joint back");
    for (int k = 0; k < 24 && doc.text() != made; ++k)
      SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == made, "redo did not restore the joints exactly");
    report << "PASS: undo and redo step through every joint exactly\n" << std::flush;
    // Delete crate A: its rope and its pulley go; crate B's chain and the rest follow the renumbering.
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    selectRecord(int(doc.records("CLUTTER", map)[0]));
    SendMessageW(window, WM_COMMAND, Remove, 0);
    {auto js = joints();bool chainB = false, ropeA = false;
     for (auto i : js) {auto f = NativeDocument::fields(doc.lines[i]);chainB |= f[2] == "chain" && f[3] == "0";ropeA |= f[2] == "rope" || f[2] == "pulley";}
     require(js.size() == 4 && chainB && !ropeA, "deleting crate A did not take its rope and pulley and renumber the rest");}
    require(validateDocument(), "the engine refused the map after the crate was deleted");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == made, "undo did not bring the crate and its joints back");
    report << "PASS: deleting a crate takes its joints with it and renumbers the rest; undo brings them back\n" << std::flush;
    auto folder = session / L"connect";
    fs::create_directories(folder);
    const auto file = folder / L"connect.rmc";
    const auto saved = doc.text();
    saveTo(file);
    NativeDocument reopened;
    reopened.open(file);
    require(reopened.text() == saved, "save/reopen changed the joints");
    report << "PASS: save and reopen keep every byte\n" << std::flush;
    dirty = false;
    if (pictures)
      connectPictures(report);
  }
  // The Connect tool on the Starter Map's SHOWCASE: PHYSICS, mid-drag from a crate by the seesaw to the ceiling:
  // the plan's dashed ghost line, and the 3D pane the engine's picture of the chain the drag makes (the live
  // pane's drag preview). As atriumPictures (NativeEditorPictures.inc) takes its pictures.
  void connectPictures(std::ofstream &report) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Connect pictures: " + message);
    };
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    NativeDocument starter;
    starter.open(root / L"custom maps" / L"Starter Map.rmc");
    adoptDocument(std::move(starter), root / L"custom maps" / L"Starter Map.rmc", true);
    autoDressingEnabled = false;
    int room = -1;
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2 && f[2].find("Showcase Physics") != std::string::npos)
        room = std::stoi(f[1]);
    }
    require(room >= 0, "the Starter Map has no physics room");
    map = room, layer = 0;
    refresh();
    auto mouse = [&](UINT message, float x, float y) {
      auto v = planView();
      SendMessageW(plan, message, MK_LBUTTON, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto engine = [&](const std::wstring &name, float x, float y, float eye, float yaw, float pitch) {
      auto session = folder / (name + L"-session");
      fs::create_directories(session);
      doc.save(session / L"working.rmc");
      std::ofstream(session / L"control.txt") << "1 " << map << " 0 0 0 0 0 0 0\n0\n0\n";
      std::ofstream(session / L"look.txt") << "LOOK 0 0 0 0\n";
      std::ofstream(session / L"film.txt") << "FILM 1 " << x << ' ' << y << ' ' << eye << ' ' << yaw << ' ' << pitch << " 70\n";
      const auto ppm = folder / (name + L".ppm");
      std::error_code ec;
      fs::remove(ppm, ec);
      std::wstring line = L"\"" + engineExecutable().wstring() + L"\" \"--editor-pack=" + (session / L"working.rmc").wstring() + L"\" \"--editor-control=" +
                          (session / L"control.txt").wstring() + L"\" \"--editor-status=" + (session / L"status.txt").wstring() +
                          L"\" --editor-frames=60 \"--editor-capture=" + ppm.wstring() + L"\" --editor-hidden";
      STARTUPINFOW si{};
      si.cb = sizeof si;
      PROCESS_INFORMATION p{};
      require(CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, session.c_str(), &si, &p) != 0, "the engine did not start");
      if (WaitForSingleObject(p.hProcess, 300000) == WAIT_TIMEOUT)
        TerminateProcess(p.hProcess, 9);
      DWORD code = 1;
      GetExitCodeProcess(p.hProcess, &code);
      CloseHandle(p.hThread), CloseHandle(p.hProcess);
      require(code == 0 && fs::exists(ppm), "the engine picture failed for " + utf8(name));
      return ppm;
    };
    auto shot = [&](const std::wstring &name, const fs::path &ppm) {
      RECT was{};
      GetWindowRect(window, &was);
      SetWindowPos(window, nullptr, -6000, 0, 2300, 1000, SWP_NOZORDER | SWP_NOACTIVATE);
      ShowWindow(window, SW_SHOWNOACTIVATE);
      layout();
      RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
      for (ULONGLONG until = GetTickCount64() + 500; GetTickCount64() < until;) {
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
      info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = r.right, info.bmiHeader.biHeight = -r.bottom;
      info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
      std::vector<unsigned char> bits(size_t(r.right) * size_t(r.bottom) * 4);
      SelectObject(dc, old);
      GetDIBits(dc, bitmap, 0, UINT(r.bottom), bits.data(), &info, DIB_RGB_COLORS);
      std::ifstream in(ppm, std::ios::binary);
      std::string magic;
      int w = 0, h = 0, depth = 0;
      in >> magic >> w >> h >> depth;
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
      std::ofstream out(folder / (name + L".bmp"), std::ios::binary);
      BITMAPFILEHEADER head{};
      head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), head.bfSize = DWORD(head.bfOffBits + bits.size());
      out.write(reinterpret_cast<const char *>(&head), sizeof head);
      out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
      out.write(reinterpret_cast<const char *>(bits.data()), std::streamsize(bits.size()));
      DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(window, screen);
      ShowWindow(window, SW_HIDE);
      SetWindowPos(window, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    SendMessageW(window, WM_COMMAND, ToolBase + ConnectTool, 0);
    SendMessageW(connectKindList, CB_SETCURSEL, ConnectChain, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(ConnectKindList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(connectKindList));
    // The crate by the seesaw (1.4, 13.8), chained up to the ceiling over (2.2, 13.2).
    const auto keep = doc.text();
    remember();
    authorKind("JOINT", 1.4f, 13.8f, 3.4f, 11.f);
    require(!doc.records("JOINT", map).empty() && doc.text() != keep, "the drag from the crate made no joint");
    const auto ppm = engine(L"connect-chain-3d", 6.6f, 16.4f, 1.7f, -135.f, 6.f);
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == keep, "undo did not take the picture's chain back");
    mouse(WM_LBUTTONDOWN, 1.4f, 13.8f);
    for (int i = 1; i <= 5; ++i)
      mouse(WM_MOUSEMOVE, 1.4f + 2.f * float(i) / 5.f, 13.8f - 2.8f * float(i) / 5.f);
    hoverX = 3.4f, hoverY = 11.f, hoverInside = true;
    InvalidateRect(plan, nullptr, FALSE);
    shot(L"connect-tool-drag", ppm);
    mouse(WM_LBUTTONUP, 3.4f, 11.f);
    if (doc.text() != keep)
      SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == keep, "the picture's drag changed the map");
    report << "connect-tool-drag.bmp: the Connect tool mid-drag from a crate to the ceiling, Chain picked on the right, the plan's dashed ghost line, the 3D pane the engine's picture of the chain it makes\n";
    dirty = false;
  }
