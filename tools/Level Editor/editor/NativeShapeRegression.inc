  // The Shape tool (NativeShapeTool.inc) driven as a person drives it: the whole palette in its
  // groups with a picture each, a click for the usual size, a drag to size with soft snapping, R
  // turning the next shape, picking on a shape's real turned outline, the 3D handles (a side, the
  // top, the lift, the turn), each drag one undo step, every new kind read by the engine, and the
  // map saved and opened again byte for byte. --editing-test runs it; --shape-test runs it alone
  // with pictures (.build/interaction/shape-*.bmp) and writes native-editor-shape-test.txt.
  void verifyShapes(bool pictures) {
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Shape regression: " + message);
    };
    std::ofstream report(root / L"native-editor-shape-test.txt");
    auto pass = [&](const std::string &what) { report << "PASS: " << what << '\n' << std::flush; };
    NativeDocument blank;
    blank.newCampaign("Shapes");
    blank.addRoom("Yard");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    const float savedSnap = snap;
    snap = .25f;
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
    auto pick = [&](int t) { SendMessageW(window, WM_COMMAND, ToolBase + t, 0); };
    auto choose = [&](const char *kind) {
      SendMessageW(primList, CB_SETCURSEL, primKindIndex(kind), 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(PrimList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(primList));
    };
    auto last = [&] { return NativeDocument::fields(doc.lines[doc.records("PRIM", map).back()]); };
    auto close = [](const std::string &v, float want) { return std::abs(number(v, -999) - want) < 1e-3f; };

    // ---- The palette: every kind once, in its group, with its picture's name ----
    {
      std::vector<int> seen(size_t(retro::prim::kindCount), 0);
      int sections[retro::prim::SectionCount]{};
      for (const auto &item : allParts())
        if (item.group == parts::Shapes) {
          require(item.key == "prim-" + std::to_string(item.value) && item.value >= 0 && item.value < retro::prim::kindCount, "a shape tile names no kind");
          ++seen[size_t(item.value)];
          require(item.section >= 0 && item.section < retro::prim::SectionCount, "a shape tile has no group");
          ++sections[item.section];
          require(item.name == wide(retro::prim::kinds[size_t(item.value)].label), "a shape tile is not in plain words");
        }
      for (int k = 0; k < retro::prim::kindCount; ++k)
        require(seen[size_t(k)] == 1, std::string("the palette shows ") + primKindName(k) + " " + std::to_string(seen[size_t(k)]) + " times");
      for (int s = 0; s < retro::prim::SectionCount; ++s)
        require(sections[s] > 0, std::string("the palette's group ") + retro::prim::sectionNames[s] + " is empty");
      require(sections[retro::prim::NewShapes] == 10, "the new shapes' group holds " + std::to_string(sections[retro::prim::NewShapes]));
      require(SendMessageW(primList, CB_GETCOUNT, 0, 0) == retro::prim::kindCount, "the shape list is not the whole palette");
      pick(9);
      partTab = parts::Shapes;
      if (modelSearch) // a search an earlier check left would narrow the page
        SetWindowTextW(modelSearch, L"");
      refreshParts();
      require(partsGrid.showSections && partsGrid.items.size() == size_t(retro::prim::kindCount), "the Shapes page does not show every shape in its groups");
      if (partsGrid.hwnd)
        require(partsGrid.headings.size() == size_t(retro::prim::SectionCount), "the Shapes page has " + std::to_string(partsGrid.headings.size()) + " group headings");
      int spiral = -1;
      for (size_t i = 0; i < partsGrid.items.size(); ++i)
        if (partsGrid.items[i].key == "prim-" + std::to_string(primKindIndex("spiral")))
          spiral = int(i);
      require(spiral >= 0, "the spiral stair is not on the Shapes page");
      pickPart(spiral, false);
      require(tool == 9 && listIndex(primList) == primKindIndex("spiral"), "a shape's picture does not pick the Shape tool and that shape");
      pass("palette: all " + std::to_string(retro::prim::kindCount) + " kinds, each once, in six groups in plain words (the ten new ones in New shapes); a picture picks the tool and the shape");
    }
    // ---- A click places the usual size; a drag sizes it, snapping softly ----
    {
      const size_t steps = history.size();
      drag(6, 6, 6, 6);
      auto f = last();
      require(f[2] == "spiral" && close(f[3], 6) && close(f[4], 6) && f[6] == "0" && f[7] == "0" && f[8] == "0" && f[9] == "0", "a click did not place a spiral stair at its usual size: " + doc.lines[doc.records("PRIM", map).back()]);
      require(history.size() == steps + 1, "a click is not one undo step");
      choose("arcwall");
      drag(8, 4, 10.5f, 5);
      f = last();
      require(f[2] == "arcwall" && close(f[6], 2.5f) && close(f[7], 1) && close(f[3], 9.25f) && close(f[4], 4.5f) && f[9] == "0",
              "a drag did not size a curved wall 2.5 x 1 from its corner: " + doc.lines[doc.records("PRIM", map).back()]);
      require(history.size() == steps + 2, "a drag is not one undo step");
      choose("pyramid");
      drag(2, 14, 4.25f, 16.25f); // 2.25 m snaps to the 2.2 m of a door's height
      f = last();
      require(close(f[6], 2.2f) && close(f[7], 2.2f), "soft snapping did not take 2.25 m to 2.2 m: " + f[6] + " x " + f[7]);
      MSG turn{plan, WM_KEYDOWN, 'R', 0};
      shortcut(turn);
      require(primFacing == 1, "R did not turn the next shape");
      choose("catwalk");
      drag(12, 8, 13, 10.5f);
      f = last();
      require(f[9] == "1" && close(f[6], 2.5f) && close(f[7], 1), "a turned drag did not run the catwalk east (w across north-south): " + doc.lines[doc.records("PRIM", map).back()]);
      MSG back{plan, WM_KEYDOWN, 'R', 0};
      for (int i = 0; i < 3; ++i)
        shortcut(back);
      require(primFacing == 0, "four turns are not back where they started");
      pass("placing: a click is the usual size (0s), a drag sizes it from the corner pressed (2.5 x 1 m), 2.25 m snaps to 2.2 m, R turns the next shape and its drag swaps across and along; one undo step each");
    }
    // ---- Picking on the real turned outline ----
    {
      pick(0);
      const int arc = int(doc.records("PRIM", map)[1]);
      require(hitObject(9.25f, 4.85f) == arc, "a point on the curved wall does not pick it");
      require(hitObject(9.25f, 4.1f) != arc, "a point behind the curved wall's bow picks it (its bounding box, not its outline)");
      choose("doorframe");
      pick(9);
      drag(16, 12, 16, 12);
      pick(0);
      const int frame = int(doc.records("PRIM", map).back());
      require(hitObject(16.68f, 12.f) == frame, "the door frame's jamb, outside its 1.2 m opening, does not pick it");
      const int catwalk = int(doc.records("PRIM", map)[3]);
      require(hitObject(12.5f, 10.4f) == catwalk && hitObject(12.5f, 7.6f) != catwalk, "the turned catwalk is not picked on its own outline");
      pass("picking: on the curved wall and not in the hollow behind its bow, on the door frame's jamb, on the turned catwalk's outline");
    }
    // ---- The 3D handles ----
    auto rayAt = [&](float x, float y, float z, float &ox, float &oy, float &oz, float &dx, float &dy, float &dz) {
      ox = x - 2.4f, oy = y - 3.2f, oz = z + 1.6f, dx = x - ox, dy = y - oy, dz = z - oz;
    };
    auto handleDrag = [&](int index, int id, float mx, float my, float mz) {
      selectRecord(index);
      auto f = NativeDocument::fields(doc.lines[size_t(index)]);
      PrimHandle h{};
      for (const auto &c : primHandles(f))
        if (c.id == id)
          h = c;
      float ox, oy, oz, dx, dy, dz;
      rayAt(h.x, h.y, h.z, ox, oy, oz, dx, dy, dz);
      require(beginPrimHandle3D(ox, oy, oz, dx, dy, dz) && primHandle == id, "handle " + std::to_string(id) + " was not taken");
      for (int i = 1; i <= 4; ++i) {
        const float t = i / 4.f, tx = h.x + mx * t, ty = h.y + my * t, tz = h.z + mz * t;
        applyPrimHandle3D(ox, oy, oz, tx - ox, ty - oy, tz - oz);
      }
      finishDrag(false);
      return NativeDocument::fields(doc.lines[size_t(index)]);
    };
    {
      const int arc = int(doc.records("PRIM", map)[1]);
      const std::string before = doc.lines[size_t(arc)];
      const size_t steps = history.size();
      auto f = handleDrag(arc, 0, .6f, 0, 0);
      require(close(f[6], 3.1f) && close(f[3], 9.55f) && close(f[4], 4.5f) && close(f[7], 1), "dragging the east side did not stretch it to 3.1 m with the west side kept: " + doc.lines[size_t(arc)]);
      require(history.size() == steps + 1, "a side drag is not one undo step");
      SendMessageW(window, WM_COMMAND, Undo, 0);
      require(doc.lines[size_t(arc)] == before, "undo did not take the stretch back");
      SendMessageW(window, WM_COMMAND, Redo, 0);
      require(close(NativeDocument::fields(doc.lines[size_t(arc)])[6], 3.1f), "redo did not put the stretch back");
      f = handleDrag(arc, 2, 0, .5f, 0);
      require(close(f[7], 1.5f) && close(f[4], 4.75f), "dragging the front did not deepen it toward its facing: " + doc.lines[size_t(arc)]);
      f = handleDrag(arc, 4, 0, 0, .5f);
      require(close(f[8], 3.5f), "dragging the top did not raise it to 3.5 m: " + doc.lines[size_t(arc)]);
      f = handleDrag(arc, 5, 0, 0, .5f);
      require(close(f[5], .5f), "the lift handle did not raise it 0.5 m off the floor: " + doc.lines[size_t(arc)]);
      f = handleDrag(arc, 6, 1.8f, -1.2f, 0);
      require(f[9] == "1", "the turn handle did not turn it to face east: " + doc.lines[size_t(arc)]);
      require(history.size() == steps + 5, "five handle drags are not five undo steps");
      // A shape at its usual size (0s) gets its sizes written out by the drag, and back by undo.
      const int spiral = int(doc.records("PRIM", map)[0]);
      const std::string usual = doc.lines[size_t(spiral)];
      f = handleDrag(spiral, 4, 0, 0, .25f);
      require(close(f[8], 3.25f) && close(f[6], 2.4f), "the spiral stair's usual 3 m did not grow from its top: " + doc.lines[size_t(spiral)]);
      SendMessageW(window, WM_COMMAND, Undo, 0);
      require(doc.lines[size_t(spiral)] == usual, "undo did not bring back the usual sizes (0s)");
      std::vector<retro::EditorGuide> guides;
      selectRecord(arc);
      require(primGuides(guides) && guides.size() > 20, "a picked shape shows no handles in 3D");
      pass("3D handles: a side stretches it with the far side kept, the front deepens it, the top raises it, the lift handle lifts it, the turn handle turns it; one undo step each; usual sizes written out and undone");
    }
    // ---- Every new kind is a record the engine reads ----
    {
      pick(9);
      float x = 3;
      for (int k = 0; k < retro::prim::kindCount; ++k) {
        if (retro::prim::kinds[size_t(k)].section != retro::prim::NewShapes)
          continue;
        SendMessageW(primList, CB_SETCURSEL, k, 0);
        drag(x, 20, x, 20);
        require(last()[2] == primKindName(k), std::string("the Shape tool did not place a ") + primKindName(k));
        x += 2;
      }
      require(validateDocument(), "the engine refused the new shapes");
      pass("the ten new kinds placed with the Shape tool and read by the engine");
    }
    // ---- Saved and opened again, byte for byte ----
    {
      const auto file = root / L".build" / L"interaction" / L"shape-roundtrip.rmc";
      fs::create_directories(file.parent_path());
      doc.save(file);
      NativeDocument again;
      again.open(file);
      require(again.text() == doc.text(), "the map did not come back byte for byte");
      pass("save and open again: the shapes come back byte for byte");
    }
    if (pictures)
      shapePictures();
    snap = savedSnap;
    pick(0);
    dirty = false;
  }
  // Pictures at 2300 x 1000 (.build/interaction): the Shape palette with a drag in progress, and a
  // shape's 3D handles mid-drag, the 3D pane the engine's picture of the document with the guides.
  void shapePictures() {
    const auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    auto big = [&](const std::wstring &name, const fs::path &ppm) {
      RECT was{};
      GetWindowRect(window, &was);
      SetWindowPos(window, nullptr, -6000, 0, 2300, 1000, SWP_NOZORDER | SWP_NOACTIVATE);
      ShowWindow(window, SW_SHOWNOACTIVATE);
      layout();
      if (partsGrid.hwnd && partsGrid.showSections) { // the palette scrolled to its New shapes group
        partsGrid.layoutSpots();
        if (!partsGrid.headings.empty()) {
          RECT gr{};
          GetClientRect(partsGrid.hwnd, &gr);
          partsGrid.scroll = std::clamp(int(partsGrid.headings.back().first.top) - 4, 0, std::max(0, partsGrid.content - int(gr.bottom)));
          InvalidateRect(partsGrid.hwnd, nullptr, FALSE);
        }
      }
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
    // The engine's picture of the document, its guides (the picked shape's handles) drawn.
    auto engine = [&](const std::wstring &name, float x, float y, float eye, float yaw, float pitch) -> fs::path {
      if (!fs::exists(engineExecutable()))
        return {};
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
      if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, session.c_str(), &si, &p))
        return {};
      if (WaitForSingleObject(p.hProcess, 300000) == WAIT_TIMEOUT)
        TerminateProcess(p.hProcess, 9);
      CloseHandle(p.hThread), CloseHandle(p.hProcess);
      return fs::exists(ppm) ? ppm : fs::path{};
    };
    std::ofstream report(root / L"native-editor-shape-test.txt", std::ios::app);
    // A small building of shapes to look at: a stone plinth, a brick wall prim, a concrete arch, a copper dome.
    remember();
    for (std::vector<std::string> f : std::vector<std::vector<std::string>>{
             {"PRIM", std::to_string(map), "box", "12", "15", "0", "6", "4", "0.5", "0", "33"},
             {"PRIM", std::to_string(map), "box", "10.2", "15", "0.4", "1.6", "0.4", "3", "0", "31"},
             {"PRIM", std::to_string(map), "box", "13.8", "15", "0.4", "1.6", "0.4", "3", "0", "31"},
             {"PRIM", std::to_string(map), "arch", "12", "15", "0.4", "2.6", "0.7", "3.4", "0", "42"},
             {"PRIM", std::to_string(map), "dome", "12", "17", "0.4", "3", "3", "1.8", "0", "19"}})
      doc.add(f);
    publish();
    // ---- The palette, a drag in progress on the plan ----
    // The shapes' pictures first, drawn by the engine as the Builder draws them on its first start.
    thumbsOnly = "prim-";
    makeThumbnails(true);
    thumbsOnly.clear();
    thumbs.rescan();
    SendMessageW(window, WM_COMMAND, ToolBase + 9, 0);
    partTab = parts::Shapes;
    SendMessageW(primList, CB_SETCURSEL, primKindIndex("spiral"), 0);
    refreshParts();
    {
      auto v = planView();
      auto at = [&](float x, float y) { return MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))); };
      const auto keep = doc.text();
      SendMessageW(plan, WM_LBUTTONDOWN, MK_LBUTTON, at(4, 8));
      SendMessageW(plan, WM_MOUSEMOVE, MK_LBUTTON, at(6.4f, 10.4f));
      hoverX = 6.4f, hoverY = 10.4f, hoverInside = true;
      big(L"shape-palette", engine(L"shape-palette-3d", 12.f, 6.4f, 3.2f, 90.f, -18.f));
      SendMessageW(plan, WM_LBUTTONUP, MK_LBUTTON, at(6.4f, 10.4f));
      if (doc.text() != keep)
        SendMessageW(window, WM_COMMAND, Undo, 0);
      report << "shape-palette.bmp: the Shape tool's palette in its groups, a spiral stair's ghost dragged out on the plan with its size and facing\n";
    }
    // ---- The whole Shapes page on its own, wide enough for every group at once ----
    if (partsGrid.hwnd) {
      RECT was{};
      GetWindowRect(partsGrid.hwnd, &was);
      MapWindowPoints(nullptr, GetParent(partsGrid.hwnd), reinterpret_cast<POINT *>(&was), 2);
      const int gw = 1640, gh = 1900;
      SetWindowPos(partsGrid.hwnd, nullptr, 0, 0, gw, gh, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
      partsGrid.cell = 160;
      partsGrid.scroll = 0;
      partsGrid.layoutSpots();
      HDC screen = GetDC(partsGrid.hwnd), dc = CreateCompatibleDC(screen);
      auto bitmap = CreateCompatibleBitmap(screen, gw, gh);
      auto old = SelectObject(dc, bitmap);
      RECT r{0, 0, gw, gh};
      partsGrid.paint(dc, r, ui::Paper, ui::Field, ui::PaperEdge, ui::SageEdge, ui::Glow, ui::Ink, ui::InkSoft);
      BITMAPINFO info{};
      info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = gw, info.bmiHeader.biHeight = -gh;
      info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
      std::vector<unsigned char> bits(size_t(gw) * size_t(gh) * 4);
      SelectObject(dc, old);
      GetDIBits(dc, bitmap, 0, UINT(gh), bits.data(), &info, DIB_RGB_COLORS);
      std::ofstream out(folder / L"shape-palette-page.bmp", std::ios::binary);
      BITMAPFILEHEADER head{};
      head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), head.bfSize = DWORD(head.bfOffBits + bits.size());
      out.write(reinterpret_cast<const char *>(&head), sizeof head);
      out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
      out.write(reinterpret_cast<const char *>(bits.data()), std::streamsize(bits.size()));
      DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(partsGrid.hwnd, screen);
      partsGrid.cell = 100;
      SetWindowPos(partsGrid.hwnd, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
      partsGrid.layoutSpots();
      report << "shape-palette-page.bmp: the Shapes page alone at 1640 x 1900, every shape's picture in its six groups\n";
    }
    // ---- The arch picked, its east side being stretched in 3D ----
    {
      SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
      const int arch = int(doc.records("PRIM", map)[size_t(doc.records("PRIM", map).size() - 2)]);
      selectRecord(arch);
      auto f = NativeDocument::fields(doc.lines[size_t(arch)]);
      PrimHandle h{};
      for (const auto &c : primHandles(f))
        if (c.id == 0)
          h = c;
      const float ox = h.x - 2.4f, oy = h.y - 3.2f, oz = h.z + 1.6f;
      beginPrimHandle3D(ox, oy, oz, h.x - ox, h.y - oy, h.z - oz);
      applyPrimHandle3D(ox, oy, oz, h.x + .5f - ox, h.y - oy, h.z - oz);
      fillFields(false);
      // The engine leaves the guides out of a film camera's picture, so the picked shape's handles are
      // drawn over it here as the live pane draws them (SoftwareRenderer::drawEditorGuides), from the same camera.
      const float cx = 6.6f, cy = 8.6f, eye = 3.9f, yaw = 50.f, pitch = -26.f, fov = 70.f; // under the yard's 4.5 m ceiling
      const auto ppm = engine(L"shape-handle-drag-3d", cx, cy, eye, yaw, pitch);
      if (!ppm.empty()) {
        std::ifstream in(ppm, std::ios::binary);
        std::string magic;
        int w = 0, h = 0, depth = 0;
        in >> magic >> w >> h >> depth;
        in.get();
        std::vector<unsigned char> rgb(size_t(std::max(0, w * h * 3)));
        in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()));
        in.close();
        const float a = yaw * 3.14159265f / 180.f, p = pitch * 3.14159265f / 180.f, ca = std::cos(a), sa = std::sin(a), cp = std::cos(p), sp = std::sin(p);
        const float focal = float(w) * .5f / std::tan(fov * .5f * 3.14159265f / 180.f);
        auto project = [&](float x, float y, float z, float &sx, float &sy) {
          const float dx = x - cx, dy = y - cy, dz = z - eye, forward = dx * ca + dy * sa, depthAlong = forward * cp + dz * sp;
          if (depthAlong < .05f)
            return false;
          sx = w * .5f + focal * (-dx * sa + dy * ca) / depthAlong, sy = h * .5f - focal * (dz * cp - forward * sp) / depthAlong;
          return true;
        };
        auto dot = [&](int x, int y, int size, unsigned colour) {
          for (int j = -size; j <= size; ++j)
            for (int i = -size; i <= size; ++i)
              if (x + i >= 0 && x + i < w && y + j >= 0 && y + j < h) {
                unsigned char *px = rgb.data() + (size_t(y + j) * size_t(w) + size_t(x + i)) * 3;
                px[0] = (colour >> 16) & 255, px[1] = (colour >> 8) & 255, px[2] = colour & 255;
              }
        };
        std::vector<retro::EditorGuide> guides;
        primGuides(guides);
        for (const auto &g : guides) {
          float x0, y0, x1, y1;
          if (!project(g.ax, g.ay, g.az, x0, y0) || !project(g.bx, g.by, g.bz, x1, y1))
            continue;
          const unsigned colour = g.kind == 1 ? 0xffc66du : g.kind == 2 ? 0x78dfefu : g.kind == 4 ? 0x67cfe4u : 0xa2d5adu;
          const int steps = std::min(4096, std::max(1, int(std::max(std::abs(x1 - x0), std::abs(y1 - y0)))));
          for (int s = 0; s <= steps; ++s)
            dot(int(x0 + (x1 - x0) * s / steps), int(y0 + (y1 - y0) * s / steps), 1, colour);
          if (g.kind >= 1 && g.kind <= 3)
            for (auto [px, py] : {std::pair{x0, y0}, std::pair{x1, y1}})
              dot(int(px), int(py), 5, 0x182428u), dot(int(px), int(py), 3, colour);
        }
        std::ofstream out(ppm, std::ios::binary);
        out << "P6\n" << w << ' ' << h << "\n255\n";
        out.write(reinterpret_cast<const char *>(rgb.data()), std::streamsize(rgb.size()));
      }
      big(L"shape-handle-drag", ppm);
      finishDrag(true);
      report << "shape-handle-drag.bmp: the concrete arch picked, its east side being stretched by its green handle in 3D, live sizes in the status line\n";
    }
  }
