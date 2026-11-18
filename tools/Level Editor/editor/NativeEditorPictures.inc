  // Pictures of the Window wall, Breach and Growth tools at work on the Starter Map's atrium
  // (--atrium-pictures; .build/interaction/atrium-*.bmp). The builder window is printed off screen at
  // 1600 x 960 with the tool's boxes on the right; headless, its 3D pane has no engine behind it, so the
  // pane is filled with the engine's own picture of the same document (the --editor-pack path the live
  // pane uses), with the gesture in hand applied as the live pane's drag preview applies it.
  void atriumPictures() {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Atrium pictures: " + message);
    };
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    NativeDocument starter;
    starter.open(root / L"custom maps" / L"Starter Map.rmc");
    adoptDocument(std::move(starter), root / L"custom maps" / L"Starter Map.rmc", true);
    autoDressingEnabled = false;
    int atrium = -1;
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2 && f[2].find("Showcase Atrium") != std::string::npos)
        atrium = std::stoi(f[1]);
    }
    require(atrium >= 0, "the Starter Map has no atrium");
    map = atrium, layer = 0;
    refresh();
    auto mouse = [&](UINT message, float x, float y) {
      auto v = planView();
      SendMessageW(plan, message, MK_LBUTTON, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    // The engine's picture of the document as it stands, from (x, y) at eye height `eye`, turned `yaw`
    // degrees (0 east, 90 south) and tilted `pitch` degrees.
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
      STARTUPINFOW si{sizeof(si)};
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
    // The builder window, its 3D pane filled from `ppm`.
    auto shot = [&](const std::wstring &name, const fs::path &ppm) {
      RECT was{};
      GetWindowRect(window, &was);
      SetWindowPos(window, nullptr, -6000, 0, 1600, 960, SWP_NOZORDER | SWP_NOACTIVATE);
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
      // The engine picture into the 3D pane, scaled to fill it.
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
        // Cover the pane: scale to its larger ratio and crop the middle.
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
    std::ofstream report(root / L"native-editor-atrium-pictures.txt");
    // ---- The Window wall tool mid-drag along the light court's wall (x 18, y 10 to 15) ----
    SendMessageW(window, WM_COMMAND, ToolBase + GlazingTool, 0);
    box(glazingHeightList, GlazingHeightList, 0);
    box(glazingBaysList, GlazingBaysList, 2);
    box(glazingRowsList, GlazingRowsList, 1);
    box(glazingPaneList, GlazingPaneList, 8);
    box(glazingLookList, GlazingLookList, 1);
    box(glazingDoorList, GlazingDoorList, 0);
    {
      const auto keep = doc.text();
      remember();
      authorKind("GLAZING", 17.6f, 10.1f, 17.6f, 14.9f);
      require(doc.records("GLAZING", map).size() == 2, "the drag along the court's wall made no storefront");
      const auto ppm = engine(L"atrium-window-wall-3d", 11.2f, 12.6f, 1.6f, 4.f, -3.f);
      SendMessageW(window, WM_COMMAND, Undo, 0);
      require(doc.text() == keep, "undo did not take the picture's storefront back");
      mouse(WM_LBUTTONDOWN, 17.6f, 10.1f);
      for (int i = 1; i <= 5; ++i)
        mouse(WM_MOUSEMOVE, 17.6f, 10.1f + 4.8f * float(i) / 5.f);
      shot(L"atrium-window-wall-drag", ppm);
      mouse(WM_LBUTTONUP, 17.6f, 14.9f);
      if (doc.text() != keep)
        SendMessageW(window, WM_COMMAND, Undo, 0);
      require(doc.text() == keep, "the picture's drag changed the map");
      report << "atrium-window-wall-drag.bmp: the Window wall tool mid-drag along the court's wall, its boxes on the right, the 3D pane the engine's picture of the storefront it makes\n";
    }
    // ---- The atrium's storefront picked: its named fields in the inspector ----
    {
      SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
      selectRecord(int(doc.records("GLAZING", map)[0]));
      fillFields(false);
      const auto ppm = engine(L"atrium-storefront-3d", 10.f, 9.8f, 1.6f, 90.f, -2.f);
      shot(L"atrium-storefront-inspector", ppm);
      report << "atrium-storefront-inspector.bmp: the shipped storefront picked, its cols, rows, sill, head, door, look and panes in the inspector\n";
    }
    // ---- The Breach tool and the Growth brush over the hall ----
    {
      SendMessageW(window, WM_COMMAND, ToolBase + BreachTool, 0);
      box(breachShapeList, BreachShapeList, 0);
      const auto ppm = engine(L"atrium-breach-3d", 13.6f, 6.9f, 1.6f, 0.f, 2.f);
      mouse(WM_MOUSEMOVE, 18.3f, 6.8f);
      shot(L"atrium-breach-tool", ppm);
      SendMessageW(window, WM_COMMAND, ToolBase + GrowthTool, 0);
      box(growthKindList, GrowthKindList, GrowMix);
      box(growthSizeList, GrowthSizeList, 3);
      const auto ppm2 = engine(L"atrium-growth-3d", 5.6f, 2.4f, 5.2f, 48.f, -14.f);
      mouse(WM_MOUSEMOVE, 14.f, 12.f);
      shot(L"atrium-growth-brush", ppm2);
      report << "atrium-breach-tool.bmp, atrium-growth-brush.bmp: the Breach tool and the Growth brush with their boxes\n";
    }
    dirty = false;
  }
