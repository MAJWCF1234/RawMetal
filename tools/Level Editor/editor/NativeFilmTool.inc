// Film cameras: named cameras the engine films (--film, src/main.cpp). A camera is a
// builder line the game skips, kept in the map like any record (Undo, Copy, Delete,
// the inspector):
//   # EDITOR_FILM|map|name|ax|ay|az|turn|tilt|seconds|bx|by|bz|bturn|btilt|claire
// It stands at ax, ay with its lens at az (metres, absolute), turned `turn` degrees (0 east,
// 90 south) and tilted `tilt` degrees (up positive). seconds 0 is a still; more moves it to
// bx, by, bz, bturn, btilt over that time. claire 1 makes it a replay camera turned on
// Claire, who stands at the map's player start (she is seen from outside). Turn and tilt
// are degrees, not a `yaw` field, so the plan's group and copy turns (which turn yaw in
// radians) leave them be.
// Write film list puts an --film list beside the project ("custom MAP ..." then the
// frames); Render runs RawMetal.exe on it and opens the folder of pictures. Filmmaker
// opens the Raw Metal Filmmaker (NativeFilmmaker.inc): shots with keyed timelines.
// Included inside struct Editor.
static constexpr const char *FilmKind = "# EDITOR_FILM";
static constexpr int FilmFps = 10;        // a moving shot's frames a second (Claire's: 30, the engine's rec)
static constexpr float FilmEye = 1.42f;   // Game::StandingEye: the list gives feet, the camera is the eye
int filmTurn = 0;                         // eighth turns R gave the next camera
HANDLE filmWorker = nullptr;              // a render running
struct FilmCamera {
  std::string name;
  float ax = 0, ay = 0, az = 0, yaw = 0, pitch = 0, seconds = 0, bx = 0, by = 0, bz = 0, byaw = 0, bpitch = 0;
  bool claire = false;
};
static FilmCamera filmCamera(const std::vector<std::string> &f) {
  FilmCamera c;
  auto at = [&](size_t i, float fallback) { return i < f.size() ? number(f[i], fallback) : fallback; };
  c.name = f.size() > 2 && !f[2].empty() ? decoded(f[2]) : "shot";
  c.ax = at(3, 0), c.ay = at(4, 0), c.az = at(5, 1.6f), c.yaw = at(6, 0), c.pitch = at(7, 0), c.seconds = std::max(0.f, at(8, 0));
  c.bx = at(9, c.ax), c.by = at(10, c.ay), c.bz = at(11, c.az), c.byaw = at(12, c.yaw), c.bpitch = at(13, c.pitch);
  c.claire = at(14, 0) > 0;
  return c;
}
bool filmPicked() const {
  return record >= 0 && record < int(doc.lines.size()) && selectedRecords.size() <= 1 &&
         NativeDocument::fields(doc.lines[size_t(record)])[0] == FilmKind;
}
// The Film camera tool's click (a still facing the way R turned it) or drag (from where it
// stands toward what it sees).
void authorFilm(std::vector<std::string> &f, float ax, float ay, float bx, float by) {
  float yaw = std::hypot(bx - ax, by - ay) >= .4f ? std::atan2(by - ay, bx - ax) * 180.f / 3.14159265f : float(filmTurn * 45);
  if (yaw < 0)
    yaw += 360;
  std::string z = nativeNumber(layerElevation() + 1.6f), turn = nativeNumber(std::round(yaw));
  f = {FilmKind, std::to_string(map), uniqueName(FilmKind, 2, "shot_"), nativeNumber(ax), nativeNumber(ay), z, turn, "0", "0",
       nativeNumber(ax), nativeNumber(ay), z, turn, "0", "0"};
}
// R: the picked camera an eighth round (its end too), or the next one.
bool turnFilm() {
  if (filmPicked()) {
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (f.size() < 15)
      f.resize(15);
    auto turned = [](const std::string &v) {
      float a = std::fmod(number(v, 0) + 45.f, 360.f);
      return nativeNumber(a < 0 ? a + 360 : a);
    };
    f[6] = turned(f[6]), f[12] = turned(f[12].empty() ? f[6] : f[12]);
    int picked = record;
    remember();
    doc.replace(size_t(picked), f);
    publish();
    selectRecord(picked);
    message(L"Camera turned to " + wide(f[6]) + L" degrees (0 east, 90 south). Ctrl+Z takes it back.");
    return true;
  }
  if (tool != FilmTool)
    return false;
  filmTurn = (filmTurn + 1) & 7;
  if (plan)
    InvalidateRect(plan, nullptr, FALSE);
  message(L"The next camera faces " + std::to_wstring(filmTurn * 45) + L" degrees (0 east, 90 south). A drag aims it instead.");
  return true;
}
std::vector<std::pair<size_t, FilmCamera>> filmCameras(int on) const {
  std::vector<std::pair<size_t, FilmCamera>> out;
  for (auto i : doc.records(FilmKind, on))
    out.push_back({i, filmCamera(NativeDocument::fields(doc.lines[i]))});
  return out;
}
// Where a project's film lists and pictures go: beside the project, named after it.
fs::path filmFolder() const { return (path.empty() ? root / L"custom maps" : path.parent_path()); }
std::wstring filmStem() const { return path.empty() ? std::wstring(L"Untitled") : path.stem().wstring(); }
// The --film list for one map's cameras, the campaign read from pack. Frames are named after
// the camera (NAME, or NAME_0000 on for a moving shot or one of Claire).
// The stage comes first (the engine reads line one before any other); its anchor is where
// Claire stands while the cameras film: the map's player start. The Filmmaker's shots
// (NativeFilmmaker.inc) start from the same line.
std::string filmStageLine(int on, const fs::path &pack) const {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  float sx = 12, sy = 12, sz = 0, syaw = 0;
  for (auto i : doc.records("MAP", on)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    sx = f.size() > 5 ? number(f[5], sx) : sx, sy = f.size() > 6 ? number(f[6], sy) : sy, sz = f.size() > 7 ? number(f[7], 0) : 0;
    syaw = f.size() > 20 ? number(f[20], 0) * 180.f / 3.14159265f : 0;
  }
  out << "custom " << on << ' ' << sx << ' ' << sy << ' ' << sz << ' ' << syaw << " 0 " << utf8(pack.wstring()) << '\n';
  return out.str();
}
std::string filmListText(int on, const fs::path &pack) const {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  auto clean = [](std::string s) {
    for (auto &c : s)
      if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
        c = '_';
    return s.empty() ? std::string("shot") : s;
  };
  out << filmStageLine(on, pack);
  out << "# Film cameras of map " << on << " (Campaign Builder). RawMetal.exe --film=\"this list\" [--film-out=FOLDER]\n";
  auto pitchUnits = [](float degrees) { return degrees * 3.14159265f / 180.f * 140.f; };
  auto turnBetween = [](float a, float b, float t) {
    float d = std::remainder(b - a, 360.f);
    return a + d * t;
  };
  for (const auto &[index, c] : filmCameras(on)) {
    std::string name = clean(c.name);
    int frames = c.seconds > 0 ? std::clamp(int(std::lround(c.seconds * (c.claire ? 30 : FilmFps))), 1, 600) : 0;
    auto lens = [&](int i, float &x, float &y, float &z, float &yaw, float &pitch) {
      float t = frames > 0 ? float(i) / float(frames) : 0.f;
      x = c.ax + (c.bx - c.ax) * t, y = c.ay + (c.by - c.ay) * t, z = c.az + (c.bz - c.az) * t;
      yaw = turnBetween(c.yaw, c.byaw, t), pitch = c.pitch + (c.bpitch - c.pitch) * t;
    };
    out << "# " << name << (c.claire ? " (turned on Claire)" : "") << (frames ? ", moving " + std::to_string(c.seconds) + " s" : ", still")
        << '\n';
    if (c.claire) {
      out << "rec " << name << '\n';
      for (int i = 0; i <= frames; ++i) {
        float x, y, z, yaw, pitch;
        lens(i, x, y, z, yaw, pitch);
        out << "cam " << x << ' ' << y << "\ntick 2\n";
      }
      out << "stop\ncam off\n";
      continue;
    }
    for (int i = 0; i <= frames; ++i) {
      float x, y, z, yaw, pitch;
      lens(i, x, y, z, yaw, pitch);
      char frame[16];
      std::snprintf(frame, sizeof frame, "_%04d", i);
      out << "f " << name << (frames ? frame : "") << ' ' << 60 / FilmFps << ' ' << x << ' ' << y << ' ' << z - FilmEye << ' ' << yaw
          << ' ' << pitchUnits(pitch) << '\n';
    }
  }
  return out.str();
}
// The maps that have cameras, each with its list beside the project and its folder of
// pictures. A map with unsaved changes is filmed from a snapshot of the document.
struct FilmJob {
  int map = 0;
  fs::path list, folder;
};
std::vector<FilmJob> writeFilmLists() {
  std::vector<int> maps;
  for (auto i : doc.records(FilmKind)) {
    int m = int(number(NativeDocument::fields(doc.lines[i])[1], -1));
    if (m >= 0 && std::find(maps.begin(), maps.end(), m) == maps.end())
      maps.push_back(m);
  }
  std::sort(maps.begin(), maps.end());
  std::vector<FilmJob> jobs;
  if (maps.empty())
    return jobs;
  auto dir = filmFolder(), pictures = dir / (filmStem() + L" film");
  fs::create_directories(pictures);
  fs::path pack = path;
  if (path.empty() || dirty) {
    pack = pictures / L"scene.rmc";
    doc.save(pack);
  }
  for (int m : maps) {
    FilmJob job;
    job.map = m;
    bool alone = maps.size() == 1;
    job.list = dir / (filmStem() + (alone ? L"" : L" map " + std::to_wstring(m)) + L".film.txt");
    job.folder = alone ? pictures : pictures / (L"map " + std::to_wstring(m));
    fs::create_directories(job.folder);
    std::ofstream(job.list, std::ios::binary) << filmListText(m, pack);
    jobs.push_back(job);
  }
  return jobs;
}
// RawMetal.exe in the builder's folder (the one Build.cmd publishes), else the engine build.
fs::path filmEngine() const {
  std::error_code ec;
  return fs::exists(root / L"RawMetal.exe", ec) ? root / L"RawMetal.exe" : engineExecutable();
}
// Films one list into its folder and makes the frames pictures Windows opens (.bmp).
// Returns the engine's exit code; frames counts the pictures made.
static int filmRender(const fs::path &engine, const FilmJob &job, int &frames) {
  frames = 0;
  std::wstring args = L"\"" + engine.wstring() + L"\" \"--film=" + job.list.wstring() + L"\" \"--film-out=" + job.folder.wstring() + L"\"";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION p{};
  if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, job.folder.c_str(), &si, &p))
    return -1;
  if (WaitForSingleObject(p.hProcess, 600000) == WAIT_TIMEOUT)
    TerminateProcess(p.hProcess, 9), WaitForSingleObject(p.hProcess, 5000);
  DWORD code = 1;
  GetExitCodeProcess(p.hProcess, &code);
  CloseHandle(p.hThread);
  CloseHandle(p.hProcess);
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(job.folder, ec)) {
    if (entry.path().extension() != L".ppm")
      continue;
    std::ifstream in(entry.path(), std::ios::binary);
    std::string magic;
    int w = 0, h = 0, depth = 0;
    in >> magic >> w >> h >> depth;
    in.get();
    if (magic != "P6" || w <= 0 || h <= 0 || depth != 255)
      continue;
    std::vector<unsigned char> rgb(size_t(w) * size_t(h) * 3);
    in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()));
    in.close();
    size_t stride = (size_t(w) * 3 + 3) & ~size_t(3);
    BITMAPFILEHEADER head{};
    BITMAPINFOHEADER info{};
    head.bfType = 0x4D42;
    head.bfOffBits = sizeof head + sizeof info;
    head.bfSize = DWORD(head.bfOffBits + stride * size_t(h));
    info.biSize = sizeof info, info.biWidth = w, info.biHeight = h, info.biPlanes = 1, info.biBitCount = 24;
    auto bmp = entry.path();
    bmp.replace_extension(L".bmp");
    std::ofstream out(bmp, std::ios::binary);
    out.write(reinterpret_cast<const char *>(&head), sizeof head);
    out.write(reinterpret_cast<const char *>(&info), sizeof info);
    std::vector<unsigned char> row(stride, 0);
    for (int y = h - 1; y >= 0; --y) {
      for (int x = 0; x < w; ++x) {
        const unsigned char *s = &rgb[(size_t(y) * size_t(w) + size_t(x)) * 3];
        row[size_t(x) * 3] = s[2], row[size_t(x) * 3 + 1] = s[1], row[size_t(x) * 3 + 2] = s[0];
      }
      out.write(reinterpret_cast<const char *>(row.data()), std::streamsize(stride));
    }
    if (out) {
      out.close();
      fs::remove(entry.path(), ec);
      ++frames;
    }
  }
  return int(code);
}
void writeFilmButton() {
  try {
    auto jobs = writeFilmLists();
    if (jobs.empty()) {
      message(L"Place a camera first: the Film camera tool, then a click or a drag on the plan.");
      return;
    }
    message(L"Film list written: " + jobs.front().list.filename().wstring() + (jobs.size() > 1 ? L" and the other maps'." : L"."));
  } catch (const std::exception &error) {
    message(L"Could not write the film list: " + wide(error.what()));
  }
}
// Render: the lists, then the engine on each in the background; the folder opens when done.
void renderFilmButton() {
  if (filmWorker) {
    message(L"A film is still rendering. Its folder opens when it is done.");
    return;
  }
  std::vector<FilmJob> jobs;
  try {
    jobs = writeFilmLists();
  } catch (const std::exception &error) {
    message(L"Could not write the film list: " + wide(error.what()));
    return;
  }
  if (jobs.empty()) {
    message(L"Place a camera first: the Film camera tool, then a click or a drag on the plan.");
    return;
  }
  struct Work {
    fs::path engine;
    std::vector<FilmJob> jobs;
    HWND window;
  };
  auto *work = new Work{filmEngine(), jobs, window};
  message(L"Rendering the film cameras with the engine...");
  filmWorker = CreateThread(
      nullptr, 0,
      [](void *raw) -> DWORD {
        std::unique_ptr<Work> w(static_cast<Work *>(raw));
        int frames = 0, worst = 0;
        for (const auto &job : w->jobs) {
          int made = 0;
          int code = filmRender(w->engine, job, made);
          frames += made;
          if (code != 0)
            worst = code;
        }
        auto *folder = new std::wstring(w->jobs.front().folder.parent_path() == w->jobs.front().list.parent_path()
                                            ? w->jobs.front().folder.wstring()
                                            : w->jobs.front().folder.parent_path().wstring());
        PostMessageW(w->window, WM_APP + 52, WPARAM(frames), reinterpret_cast<LPARAM>(folder));
        (void)worst;
        return 0;
      },
      work, 0, nullptr);
  if (!filmWorker) {
    delete work;
    message(L"Could not start the render.");
  }
}
// The render's end (WM_APP + 52): say so and open the pictures.
void filmRendered(int frames, std::wstring *folder) {
  std::unique_ptr<std::wstring> owned(folder);
  if (filmWorker)
    CloseHandle(filmWorker), filmWorker = nullptr;
  if (frames <= 0) {
    message(L"The engine made no pictures. Its film-log.txt is in " + *owned + L".");
    return;
  }
  message(std::to_wstring(frames) + L" film pictures in " + *owned + L".");
  if (!quiet())
    ShellExecuteW(window, L"open", owned->c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
// The plan: each camera a wedge the way it looks, its name beside it; a moving one a dashed
// path to its end wedge; one turned on Claire a dotted line to where she stands.
void drawFilmCameras(HDC dc, int ox, int oy, float cell) {
  const COLORREF color = ui::Film;
  float sx = -1, sy = -1;
  for (auto i : doc.records("MAP", map)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 6)
      sx = number(f[5], -1), sy = number(f[6], -1);
  }
  auto at = [&](float x, float y) { return POINT{int(ox + x * cell), int(oy + y * cell)}; };
  auto wedge = [&](float x, float y, float yaw, bool solid) {
    float a = yaw * 3.14159265f / 180.f, spread = .45f, reach = .9f;
    POINT p[3] = {at(x, y), at(x + std::cos(a - spread) * reach, y + std::sin(a - spread) * reach),
                  at(x + std::cos(a + spread) * reach, y + std::sin(a + spread) * reach)};
    auto pen = CreatePen(PS_SOLID, solid ? 2 : 1, color);
    auto brush = solid ? CreateHatchBrush(HS_DIAGCROSS, color) : nullptr;
    auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, solid ? brush : GetStockObject(HOLLOW_BRUSH));
    Polygon(dc, p, 3);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    if (brush)
      DeleteObject(brush);
    RECT body{p[0].x - 3, p[0].y - 3, p[0].x + 4, p[0].y + 4};
    auto fill = CreateSolidBrush(color);
    FillRect(dc, &body, fill);
    DeleteObject(fill);
  };
  SetBkMode(dc, TRANSPARENT);
  auto oldFont = SelectObject(dc, smallFont ? smallFont : font);
  for (auto index : doc.records(FilmKind, map)) {
    if (!visibleFloorRecord(index))
      continue;
    auto c = filmCamera(NativeDocument::fields(doc.lines[index]));
    if (c.seconds > 0) {
      auto pen = ui::dashedPen(2, color);
      auto old = SelectObject(dc, pen);
      POINT a = at(c.ax, c.ay), b = at(c.bx, c.by);
      MoveToEx(dc, a.x, a.y, nullptr);
      LineTo(dc, b.x, b.y);
      SelectObject(dc, old);
      DeleteObject(pen);
      wedge(c.bx, c.by, c.byaw, false);
    }
    if (c.claire && sx >= 0) {
      auto pen = CreatePen(PS_DOT, 1, color);
      auto old = SelectObject(dc, pen);
      POINT a = at(c.ax, c.ay), b = at(sx, sy);
      MoveToEx(dc, a.x, a.y, nullptr);
      LineTo(dc, b.x, b.y);
      SelectObject(dc, old);
      DeleteObject(pen);
    }
    wedge(c.ax, c.ay, c.yaw, true);
    SetTextColor(dc, color);
    POINT p = at(c.ax, c.ay);
    auto name = wide(c.name);
    TextOutW(dc, p.x + 8, p.y + 6, name.c_str(), int(name.size()));
  }
  SelectObject(dc, oldFont);
}
// The picked camera in the 3D view: what its lens takes in (a frame 1.2 m out), and its path.
void filmGuides(std::vector<retro::EditorGuide> &out) const {
  if (!filmPicked())
    return;
  auto c = filmCamera(NativeDocument::fields(doc.lines[size_t(record)]));
  auto frustum = [&](float x, float y, float z, float yaw, float pitch, int kind) {
    float a = yaw * 3.14159265f / 180.f, p = pitch * 3.14159265f / 180.f, d = 1.2f;
    float fx = std::cos(a) * std::cos(p), fy = std::sin(a) * std::cos(p), fz = std::sin(p);
    float rx = -std::sin(a), ry = std::cos(a), ux = -std::cos(a) * std::sin(p), uy = -std::sin(a) * std::sin(p), uz = std::cos(p);
    float w = d * .58f, h = d * .33f; // the engine's 16:9 view, about 60 degrees across
    std::array<std::array<float, 3>, 4> corner;
    int k = 0;
    for (float sy : {-1.f, 1.f})
      for (float sx : {-1.f, 1.f})
        corner[size_t(k++)] = {x + fx * d + rx * w * sx + ux * h * sy, y + fy * d + ry * w * sx + uy * h * sy, z + fz * d + uz * h * sy};
    for (auto &q : corner)
      out.push_back({x, y, z, q[0], q[1], q[2], kind});
    for (auto [i, j] : {std::pair{0, 1}, {1, 3}, {3, 2}, {2, 0}})
      out.push_back({corner[size_t(i)][0], corner[size_t(i)][1], corner[size_t(i)][2], corner[size_t(j)][0], corner[size_t(j)][1],
                     corner[size_t(j)][2], kind});
  };
  frustum(c.ax, c.ay, c.az, c.yaw, c.pitch, 2);
  if (c.seconds > 0) {
    out.push_back({c.ax, c.ay, c.az, c.bx, c.by, c.bz, 1});
    frustum(c.bx, c.by, c.bz, c.byaw, c.bpitch, 0);
  }
}
