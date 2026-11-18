  // ---- --shots SCRIPT: pictures of the real Builder, its 3D view live (for reviews and the
  // guide). One command a line, run in order while the window and the engine work as usual:
  //   open PATH            open a map file          map N        edit map N
  //   layer N              edit floor N             tool N       pick tool N
  //   select TEXT          pick the first record whose line contains TEXT
  //   film X Y EYE YAW PITCH   put the 3D view's camera there (degrees)
  //   view N               0 plan and 3D, 1 plan, 2 3D        wait MS
  //   shot OUT.bmp         the whole window, as on screen      quit
  std::vector<std::string> shotScript;
  size_t shotLine = 0;
  ULONGLONG shotWaitUntil = 0;
  std::ofstream shotLog;
  void startShots(const fs::path &script) {
    std::ifstream in(script);
    for (std::string line; std::getline(in, line);)
      if (!line.empty() && line[0] != '#') shotScript.push_back(line);
    shotLog.open(root / L"native-editor-shots.txt");
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 1600, 940, SWP_SHOWWINDOW);
    SetTimer(window, 7777, 150, nullptr);
  }
  void writeFilm(const std::string &line) {
    std::ofstream(session / L"film.txt") << line << '\n';
  }
  void shotTick() {
    if (GetTickCount64() < shotWaitUntil) return;
    if (shotLine >= shotScript.size()) { KillTimer(window, 7777); PostMessageW(window, WM_CLOSE, 0, 0); return; }
    std::istringstream in(shotScript[shotLine++]);
    std::string verb;
    in >> verb;
    shotLog << verb << '\n' << std::flush;
    try {
      if (verb == "open") {
        std::string rest;
        std::getline(in >> std::ws, rest);
        NativeDocument next;
        next.open(fs::path(wide(rest)));
        adoptDocument(std::move(next), fs::path(wide(rest)), false);
        shotWaitUntil = GetTickCount64() + 2500;
      } else if (verb == "map") {
        int m = 0;
        in >> m;
        SendMessageW(mapList, LB_SETCURSEL, m, 0);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(MapList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(mapList));
        shotWaitUntil = GetTickCount64() + 2500;
      } else if (verb == "layer") {
        in >> layer;
        refresh(), relayout(), publish(false, false);
      } else if (verb == "tool") {
        int t = 0;
        in >> t;
        SendMessageW(window, WM_COMMAND, ToolBase + t, 0);
      } else if (verb == "select") {
        std::string text;
        std::getline(in >> std::ws, text);
        for (size_t i = 0; i < doc.lines.size(); ++i)
          if (doc.lines[i].find(text) != std::string::npos) { SendMessageW(window, WM_COMMAND, ToolBase + 0, 0); selectRecord(int(i)); relayout(); publish(false, false); break; }
      } else if (verb == "film") {
        float x, y, eye, yaw, pitch;
        in >> x >> y >> eye >> yaw >> pitch;
        std::ostringstream line;
        line << "FILM 1 " << x << ' ' << y << ' ' << eye << ' ' << yaw << ' ' << pitch << " 0";
        writeFilm(line.str());
        shotWaitUntil = GetTickCount64() + 2500;
      } else if (verb == "view") {
        int v = 0;
        in >> v;
        SendMessageW(window, WM_COMMAND, std::array<int, 3>{Dual, PlanOnly, ViewOnly}[size_t(std::clamp(v, 0, 2))], 0);
        shotWaitUntil = GetTickCount64() + 800;
      } else if (verb == "wait") {
        int ms = 0;
        in >> ms;
        shotWaitUntil = GetTickCount64() + ULONGLONG(std::max(0, ms));
      } else if (verb == "shot") {
        std::string out;
        std::getline(in >> std::ws, out);
        SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        RECT r{};
        GetWindowRect(window, &r);
        const int w = r.right - r.left, h = r.bottom - r.top;
        HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
        auto old = SelectObject(dc, bitmap);
        BitBlt(dc, 0, 0, w, h, screen, r.left, r.top, SRCCOPY | CAPTUREBLT);
        SelectObject(dc, old);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = w, info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
        std::vector<char> bits(size_t(w) * size_t(h) * 4);
        GetDIBits(dc, bitmap, 0, UINT(h), bits.data(), &info, DIB_RGB_COLORS);
        DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(nullptr, screen);
        fs::path file = fs::path(wide(out));
        fs::create_directories(file.parent_path());
        std::ofstream f(file, std::ios::binary);
        BITMAPFILEHEADER head{};
        head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), head.bfSize = DWORD(head.bfOffBits + bits.size());
        f.write(reinterpret_cast<const char *>(&head), sizeof head);
        f.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
        f.write(bits.data(), std::streamsize(bits.size()));
        shotLog << "  wrote " << out << " " << w << "x" << h << '\n';
      } else if (verb == "quit") {
        shotLine = shotScript.size();
      }
    } catch (const std::exception &error) {
      shotLog << "  FAILED: " << error.what() << '\n';
    }
  }
