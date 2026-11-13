// Raw Metal Filmmaker: the Film camera tool grown into shots with a timeline, in the
// spirit of Source Filmmaker but smaller. A shot is a length of time on one map; on its
// timeline sit keys for the camera (where it stands, turn, tilt and lens), for the map's
// people and creatures (where they stand, which way they face, which clip they play) and
// for map states (a door, the lights, a switch: an --film op at that moment). Between two
// keys a track moves in a straight line, or eased (slow out, slow in) when the first key
// says so. The live 3D view plays the shot from the shot's camera with its people posed;
// Render films the shot list with the engine (RawMetal.exe --film) and, when ffmpeg is on
// PATH, encodes each shot and the whole list to MP4 beside the project.
//
// Builder lines the game skips, kept in the map like any record (Undo, save and load):
//   # EDITOR_SHOT|map|name|seconds|fps
//   # EDITOR_SHOT_KEY|map|shot|track|time|ease|a|b|c|d|e|f
// track "cam": a b the camera's spot (metres), c its eye height (absolute), d turn and
// e tilt (degrees; 0 east, 90 south, up is more), f the lens across in degrees (0: the
// game's own, about 75). "aK" person K (its ACTOR index) and "eK" creature K (its CREATURE
// order on the map): a b where it stands, c its feet, d its facing (degrees), e its clip
// (game/FilmPuppet.h). "set": a an --film op ("set NAME=V", "door I OPEN", "lights S",
// "ambient A", "torch", "shut", "dark"), percent-encoded. A shot with no camera keys
// films from the map's player start.
// Included inside struct Editor (after NativeFilmTool.inc, whose film list, render and
// folder it shares).
static constexpr const char *ShotKind = "# EDITOR_SHOT", *ShotKeyKind = "# EDITOR_SHOT_KEY";
struct Shot {
  size_t line = 0;
  std::string name;
  float seconds = 3;
  int fps = 30;
};
struct ShotKey {
  size_t line = 0;
  std::string shot, track, op;
  float time = 0;
  bool ease = false;
  std::array<float, 6> v{};
};
// One sample of a track at a time: the six values and, for people and creatures, the
// clip's phase.
struct ShotSample {
  std::array<float, 6> v{};
  float phase = 0;
  bool keyed = false;
};
static int shotFps(float fps) {
  for (int f : {60, 30, 20, 15, 12, 10, 6, 5, 4, 3, 2, 1})
    if (fps >= float(f) - .01f)
      return f; // a whole number of 60 Hz ticks a frame
  return 1;
}
static Shot shotOf(const std::vector<std::string> &f) {
  Shot s;
  s.name = f.size() > 2 && !f[2].empty() ? decoded(f[2]) : "shot";
  s.seconds = std::clamp(f.size() > 3 ? number(f[3], 3) : 3.f, .1f, 600.f);
  s.fps = shotFps(f.size() > 4 ? number(f[4], 30) : 30.f);
  return s;
}
static ShotKey shotKeyOf(const std::vector<std::string> &f) {
  ShotKey k;
  k.shot = f.size() > 2 ? decoded(f[2]) : "";
  k.track = f.size() > 3 ? f[3] : "cam";
  k.time = std::max(0.f, f.size() > 4 ? number(f[4], 0) : 0.f);
  k.ease = f.size() > 5 && number(f[5], 0) > 0;
  for (size_t i = 0; i < 6; ++i)
    k.v[i] = f.size() > 6 + i && k.track != "set" ? number(f[6 + i], 0) : 0.f;
  if (k.track == "set" && f.size() > 6)
    k.op = decoded(f[6]);
  return k;
}
std::vector<Shot> shots(int on) const {
  std::vector<Shot> out;
  for (auto i : doc.records(ShotKind, on)) {
    auto s = shotOf(NativeDocument::fields(doc.lines[i]));
    s.line = i;
    out.push_back(s);
  }
  return out;
}
std::optional<Shot> findShot(int on, const std::string &name) const {
  for (auto &s : shots(on))
    if (s.name == name)
      return s;
  return {};
}
// A shot's keys (one track, or all), in time order.
std::vector<ShotKey> shotKeys(int on, const std::string &shot, const std::string &track = "") const {
  std::vector<ShotKey> out;
  for (auto i : doc.records(ShotKeyKind, on)) {
    auto k = shotKeyOf(NativeDocument::fields(doc.lines[i]));
    if (k.shot != shot || (!track.empty() && k.track != track))
      continue;
    k.line = i;
    out.push_back(k);
  }
  std::stable_sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.time < b.time; });
  return out;
}
// The tracks a shot keys, camera first, then people, creatures and the map's states.
std::vector<std::string> shotTracks(int on, const std::string &shot) const {
  std::vector<std::string> out;
  for (auto &k : shotKeys(on, shot))
    if (std::find(out.begin(), out.end(), k.track) == out.end())
      out.push_back(k.track);
  auto rank = [](const std::string &t) {
    return t == "cam" ? std::pair{0, 0} : t == "set" ? std::pair{3, 0} : std::pair{t[0] == 'a' ? 1 : 2, std::atoi(t.c_str() + 1)};
  };
  std::sort(out.begin(), out.end(), [&](auto &a, auto &b) { return rank(a) < rank(b); });
  return out;
}
// A track at time t: the key before it and the next, a straight line or eased between
// them (turns the short way round); a clip holds until the next key.
static ShotSample sampleShotTrack(const std::vector<ShotKey> &keys, float t) {
  ShotSample s;
  if (keys.empty())
    return s;
  s.keyed = true;
  size_t b = 0;
  while (b < keys.size() && keys[b].time <= t)
    ++b;
  const auto &a = keys[b == 0 ? 0 : b - 1];
  const auto &c = keys[b < keys.size() ? b : keys.size() - 1];
  float u = c.time > a.time && b > 0 && b < keys.size() ? std::clamp((t - a.time) / (c.time - a.time), 0.f, 1.f) : 0.f;
  if (a.ease)
    u = u * u * (3 - 2 * u);
  for (size_t i = 0; i < 6; ++i)
    s.v[i] = a.v[i] + (c.v[i] - a.v[i]) * u;
  s.v[3] = a.v[3] + std::remainder(c.v[3] - a.v[3], 360.f) * u;
  return s;
}
// A person's or creature's clip and phase at t. A walk's phase follows the ground it has
// covered since the walk began (a stride a cycle, so the feet do not slide), standing still
// it steps in place; a punch, a flinch or a fall plays once from its key; idles loop slowly.
static ShotSample sampleShotActor(const std::vector<ShotKey> &keys, float t, bool creature) {
  auto s = sampleShotTrack(keys, t);
  if (keys.empty())
    return s;
  size_t at = 0;
  for (size_t i = 0; i < keys.size(); ++i)
    if (keys[i].time <= t)
      at = i;
  int clip = int(std::lround(keys[at].v[4]));
  s.v[4] = float(clip);
  size_t from = at;
  while (from > 0 && int(std::lround(keys[from - 1].v[4])) == clip)
    --from;
  float since = std::max(0.f, t - keys[from].time), start = keys[from].time;
  if (keys[0].time > t)
    since = t, start = 0;
  if (clip == 1) {
    float walked = 0;
    auto prev = sampleShotTrack(keys, start);
    for (float x = start + 1.f / 60; x <= t + 1e-4f; x += 1.f / 60) {
      auto here = sampleShotTrack(keys, x);
      walked += std::hypot(here.v[0] - prev.v[0], here.v[1] - prev.v[1]);
      prev = here;
    }
    float stride = creature ? 1.2f : 1.45f;
    float cycles = walked > .02f ? walked / stride : since / 1.1f;
    s.phase = cycles - std::floor(cycles);
  } else if (clip == 2 || clip == 3)
    s.phase = std::clamp(since / .9f, 0.f, 1.f);
  else if (clip == 4)
    s.phase = std::clamp(since / 1.4f, 0.f, 1.f);
  else
    s.phase = since / 3.f - std::floor(since / 3.f);
  return s;
}
// The map's player start, the camera of a shot with no camera keys (eye 1.6 m).
std::array<float, 6> shotStartCamera(int on) const {
  std::array<float, 6> c{12, 12, 1.6f, 0, 0, 0};
  for (auto i : doc.records("MAP", on)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    c[0] = f.size() > 5 ? number(f[5], 12) : 12, c[1] = f.size() > 6 ? number(f[6], 12) : 12;
    c[2] = (f.size() > 7 ? number(f[7], 0) : 0) + 1.6f;
    c[3] = f.size() > 20 ? number(f[20], 0) * 180.f / 3.14159265f : 0;
  }
  return c;
}
static std::string shotFileName(std::string s) {
  for (auto &c : s)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
      c = '_';
  return s.empty() ? std::string("shot") : s;
}
// An --film op a state key may carry: only the stage's own (no input, no play).
static bool shotOpAllowed(const std::string &op) {
  std::istringstream in(op);
  std::string word;
  in >> word;
  for (auto ok : {"set", "door", "lights", "ambient", "torch", "shut", "dark", "lamp"})
    if (word == ok)
      return true;
  return false;
}
// The pose lines for every person and creature the shot keys, at t.
std::string shotPoses(int on, const std::string &shot, float t) const {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  for (auto &track : shotTracks(on, shot)) {
    if (track[0] != 'a' && track[0] != 'e')
      continue;
    auto s = sampleShotActor(shotKeys(on, shot, track), t, track[0] == 'e');
    out << "pose " << track << ' ' << s.v[0] << ' ' << s.v[1] << ' ' << s.v[2] << ' ' << s.v[3] << ' ' << int(std::lround(s.v[4]))
        << ' ' << s.phase << '\n';
  }
  return out.str();
}
std::array<float, 6> shotCamera(int on, const std::string &shot, float t) const {
  auto keys = shotKeys(on, shot, "cam");
  if (keys.empty())
    return shotStartCamera(on);
  return sampleShotTrack(keys, t).v;
}
// The --film list for one shot: the map's stage, then every frame's states, poses and
// camera ("f NAME_NNNN TICKS X Y FEET YAW PITCH FOV").
std::string shotListText(int on, const Shot &shot, const fs::path &pack) const {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  out << filmStageLine(on, pack);
  int frames = std::clamp(int(std::lround(shot.seconds * float(shot.fps))), 1, 36000);
  out << "# Shot " << shot.name << " of map " << on << " (Raw Metal Filmmaker): " << shot.seconds << " s, " << shot.fps << " fps, "
      << frames << " frames\n";
  auto states = shotKeys(on, shot.name, "set");
  size_t nextState = 0;
  std::string name = shotFileName(shot.name);
  for (int i = 0; i < frames; ++i) {
    float t = float(i) / float(shot.fps);
    for (; nextState < states.size() && states[nextState].time <= t + .5f / float(shot.fps); ++nextState)
      if (shotOpAllowed(states[nextState].op))
        out << states[nextState].op << '\n';
    out << shotPoses(on, shot.name, t);
    auto c = shotCamera(on, shot.name, t);
    char frame[16];
    std::snprintf(frame, sizeof frame, "_%04d", i);
    out << "f " << name << frame << ' ' << 60 / shot.fps << ' ' << c[0] << ' ' << c[1] << ' ' << c[2] - FilmEye << ' ' << c[3] << ' '
        << c[4] * 3.14159265f / 180.f * 140.f << ' ' << c[5] << '\n';
  }
  return out.str();
}
// Where a shot's list, frames and movie go: "NAME film" beside the project.
fs::path shotPictures() const { return filmFolder() / (filmStem() + L" film"); }
struct ShotJob {
  FilmJob film;
  std::string shot;
  int fps = 30;
  fs::path movie;
};
// Writes the lists for the named shots (all of the map's when empty). A map with unsaved
// changes is filmed from a snapshot of the document, as the Film cameras are.
std::vector<ShotJob> writeShotLists(int on, const std::vector<std::string> &names) {
  std::vector<ShotJob> jobs;
  auto all = shots(on);
  if (all.empty())
    return jobs;
  auto pictures = shotPictures();
  fs::create_directories(pictures);
  fs::path pack = path;
  if (path.empty() || dirty) {
    pack = pictures / L"scene.rmc";
    doc.save(pack);
  }
  for (auto &s : all) {
    if (!names.empty() && std::find(names.begin(), names.end(), s.name) == names.end())
      continue;
    ShotJob job;
    job.shot = s.name, job.fps = s.fps, job.film.map = on;
    auto stem = wide(shotFileName(s.name));
    job.film.list = pictures / (stem + L".film.txt");
    job.film.folder = pictures / stem;
    job.movie = pictures / (stem + L".mp4");
    std::error_code ec;
    fs::remove_all(job.film.folder, ec);
    fs::create_directories(job.film.folder);
    std::ofstream(job.film.list, std::ios::binary) << shotListText(on, s, pack);
    jobs.push_back(job);
  }
  return jobs;
}
static fs::path ffmpegPath() {
  wchar_t found[MAX_PATH]{};
  return SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr) ? fs::path(found) : fs::path();
}
static int runHidden(std::wstring args, const fs::path &folder, DWORD wait = 600000) {
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION p{};
  if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, folder.c_str(), &si, &p))
    return -1;
  if (WaitForSingleObject(p.hProcess, wait) == WAIT_TIMEOUT)
    TerminateProcess(p.hProcess, 9), WaitForSingleObject(p.hProcess, 5000);
  DWORD code = 1;
  GetExitCodeProcess(p.hProcess, &code);
  CloseHandle(p.hThread);
  CloseHandle(p.hProcess);
  return int(code);
}
// One shot's frames to an H.264 MP4 (MPEG-4 if this ffmpeg has no x264).
static bool encodeShot(const fs::path &ffmpeg, const ShotJob &job) {
  auto input = (job.film.folder / (wide(shotFileName(job.shot)) + L"_%04d.bmp")).wstring();
  for (auto codec : {L"-c:v libx264 -pix_fmt yuv420p -crf 18", L"-c:v mpeg4 -q:v 2 -pix_fmt yuv420p"}) {
    std::wstring args = L"\"" + ffmpeg.wstring() + L"\" -y -loglevel error -framerate " + std::to_wstring(job.fps) + L" -i \"" + input +
                        L"\" " + codec + L" \"" + job.movie.wstring() + L"\"";
    std::error_code ec;
    if (runHidden(args, job.film.folder) == 0 && fs::exists(job.movie, ec) && fs::file_size(job.movie, ec) > 0)
      return true;
  }
  return false;
}
// The whole list, shot after shot, as one movie (NAME.mp4 beside the shots).
static bool joinShots(const fs::path &ffmpeg, const std::vector<ShotJob> &jobs, const fs::path &movie) {
  auto list = movie.parent_path() / L"film-cut.txt";
  {
    std::ofstream out(list, std::ios::binary);
    for (auto &j : jobs)
      out << "file '" << utf8(j.movie.wstring()) << "'\n";
  }
  std::wstring args = L"\"" + ffmpeg.wstring() + L"\" -y -loglevel error -f concat -safe 0 -i \"" + list.wstring() +
                      L"\" -c:v libx264 -pix_fmt yuv420p -crf 18 -r 30 \"" + movie.wstring() + L"\"";
  std::error_code ec;
  return runHidden(args, movie.parent_path()) == 0 && fs::exists(movie, ec);
}
// Films the jobs with the engine, then encodes. Returns frames made; says what it made.
static int renderShotJobs(const fs::path &engine, const std::vector<ShotJob> &jobs, const fs::path &cut, std::wstring &said) {
  int frames = 0, movies = 0, failed = 0;
  auto ffmpeg = ffmpegPath();
  std::vector<ShotJob> encoded;
  for (auto &job : jobs) {
    int made = 0;
    int code = filmRender(engine, job.film, made);
    frames += made;
    if (code != 0 || made == 0) {
      ++failed;
      continue;
    }
    if (!ffmpeg.empty() && encodeShot(ffmpeg, job))
      ++movies, encoded.push_back(job);
  }
  bool joined = !ffmpeg.empty() && encoded.size() > 1 && joinShots(ffmpeg, encoded, cut);
  said = std::to_wstring(frames) + L" frames from " + std::to_wstring(jobs.size() - size_t(failed)) + L" shot" +
         (jobs.size() == 1 ? L"" : L"s");
  if (ffmpeg.empty())
    said += L" (no ffmpeg on PATH: the frames are .bmp pictures)";
  else
    said += L", " + std::to_wstring(movies) + L" MP4" + (joined ? L" and the cut " + cut.filename().wstring() : L"");
  if (failed)
    said += L"; " + std::to_wstring(failed) + L" shot(s) failed (film-log.txt in their folders)";
  return frames;
}

// ---- The Filmmaker window ----------------------------------------------------------
HWND fm = nullptr, fmTimeline = nullptr, fmShots = nullptr, fmTrackList = nullptr, fmClipList = nullptr, fmName = nullptr,
     fmSeconds = nullptr, fmRate = nullptr, fmOp = nullptr, fmEase = nullptr, fmPreview = nullptr, fmClock = nullptr,
     fmStatus = nullptr, fmPlay = nullptr;
std::array<HWND, 6> fmValues{}, fmValueLabels{};
std::string fmShot, fmTrack = "cam", fmKeyTrack;
float fmTime = 0, fmKeyTime = -1, fmDragTime = -1;
bool fmPlaying = false, fmLive = true, fmDragging = false, fmScrubbing = false;
ULONGLONG fmPlayedAt = 0;
std::uint64_t fmSeenVersion = 0;
size_t fmSeenLines = 0;
int fmSeenMap = -1;
std::string fmPreviewSent;
std::vector<std::string> fmTrackIds;
HANDLE fmWorker = nullptr;
enum FmId {
  FmShotList = 1, FmNewShot, FmDeleteShot, FmName, FmSeconds, FmRate, FmApplyShot,
  FmPlay = 10, FmRewind, FmPreview, FmRenderShot, FmRenderAll,
  FmTrack = 20, FmValue0 = 21, FmClip = 27, FmOp, FmEase, FmSetKey = 30, FmKeyView, FmDeleteKey
};
static constexpr const wchar_t *fmClipNames[] = {L"0 idle", L"1 walk", L"2 punch / attack", L"3 hit / flinch", L"4 death",
                                                 L"5 crouch", L"6 radio", L"7 reading", L"8 pointing", L"9 directing"};
void fmSay(const std::wstring &s) {
  message(s);
  if (fmStatus)
    SetWindowTextW(fmStatus, s.c_str());
}
std::optional<Shot> fmCurrent() const { return fmShot.empty() ? std::nullopt : findShot(map, fmShot); }
float fmFrame() const {
  auto s = fmCurrent();
  return s ? 1.f / float(s->fps) : 1.f / 30;
}
float fmSnap(float t) const {
  auto s = fmCurrent();
  float f = fmFrame();
  return std::clamp(std::round(t / f) * f, 0.f, s ? s->seconds : 0.f);
}
// Opens the window (or brings it forward). Its button sits under the Film camera tool.
void openFilmmaker() {
  if (fm) {
    ShowWindow(fm, SW_SHOWNORMAL);
    SetForegroundWindow(fm);
    return;
  }
  static bool registered = false;
  HINSTANCE instance = GetModuleHandleW(nullptr);
  if (!registered) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = filmmakerProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(ui::Paper);
    wc.lpszClassName = L"RawMetalFilmmaker";
    RegisterClassW(&wc);
    WNDCLASSW tl{};
    tl.lpfnWndProc = timelineProc;
    tl.hInstance = instance;
    tl.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    tl.style = CS_DBLCLKS;
    tl.lpszClassName = L"RawMetalFilmTimeline";
    RegisterClassW(&tl);
    registered = true;
  }
  RECT owner{};
  GetWindowRect(window, &owner);
  const int width = 1100, height = 640;
  fm = CreateWindowExW(WS_EX_CONTROLPARENT, L"RawMetalFilmmaker", L"Raw Metal Filmmaker", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                       owner.left + 40, owner.top + 80, width, height, window, nullptr, instance, this);
  if (!fm)
    return;
  auto child = [&](const wchar_t *cls, const wchar_t *text, DWORD style, int id) {
    HWND h = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, fm, reinterpret_cast<HMENU>(INT_PTR(id)),
                             instance, nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return h;
  };
  child(L"STATIC", L"Shots", 0, 101);
  fmShots = child(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP, FmShotList);
  child(L"BUTTON", L"+ New shot", BS_PUSHBUTTON | WS_TABSTOP, FmNewShot);
  child(L"BUTTON", L"Delete shot", BS_PUSHBUTTON | WS_TABSTOP, FmDeleteShot);
  child(L"STATIC", L"Name", 0, 102);
  fmName = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, FmName);
  child(L"STATIC", L"Length (s)", 0, 103);
  fmSeconds = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, FmSeconds);
  child(L"STATIC", L"Frames / s", 0, 104);
  fmRate = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, FmRate);
  child(L"BUTTON", L"Apply to shot", BS_PUSHBUTTON | WS_TABSTOP, FmApplyShot);
  fmPlay = child(L"BUTTON", L"▶ Play", BS_PUSHBUTTON | WS_TABSTOP, FmPlay);
  child(L"BUTTON", L"⏮ Start", BS_PUSHBUTTON | WS_TABSTOP, FmRewind);
  fmClock = child(L"STATIC", L"", 0, 105);
  fmPreview = child(L"BUTTON", L"Show in 3D view", BS_AUTOCHECKBOX | WS_TABSTOP, FmPreview);
  SendMessageW(fmPreview, BM_SETCHECK, fmLive ? BST_CHECKED : BST_UNCHECKED, 0);
  child(L"BUTTON", L"Render shot", BS_PUSHBUTTON | WS_TABSTOP, FmRenderShot);
  child(L"BUTTON", L"▶ Render all shots", BS_PUSHBUTTON | WS_TABSTOP, FmRenderAll);
  child(L"STATIC", L"Track", 0, 106);
  fmTrackList = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, FmTrack);
  fmEase = child(L"BUTTON", L"Ease out of this key", BS_AUTOCHECKBOX | WS_TABSTOP, FmEase);
  static constexpr const wchar_t *labels[] = {L"East", L"South", L"Eye", L"Turn", L"Tilt", L"Lens"};
  for (int i = 0; i < 6; ++i) {
    fmValueLabels[size_t(i)] = child(L"STATIC", labels[i], SS_RIGHT, 110 + i);
    fmValues[size_t(i)] = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, FmValue0 + i);
  }
  fmClipList = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, FmClip);
  for (auto c : fmClipNames)
    SendMessageW(fmClipList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c));
  fmOp = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, FmOp);
  SendMessageW(fmOp, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"set NAME=1  |  door 0 1  |  lights 0.5"));
  child(L"BUTTON", L"◆ Set key here", BS_PUSHBUTTON | WS_TABSTOP, FmSetKey);
  child(L"BUTTON", L"Key the 3D view", BS_PUSHBUTTON | WS_TABSTOP, FmKeyView);
  child(L"BUTTON", L"Delete key", BS_PUSHBUTTON | WS_TABSTOP, FmDeleteKey);
  fmTimeline = CreateWindowExW(0, L"RawMetalFilmTimeline", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10, fm,
                               reinterpret_cast<HMENU>(INT_PTR(40)), instance, this);
  fmStatus = child(L"STATIC", L"Click or drag the ruler to scrub. Click a key to pick it; drag it to retime. Space plays.", 0, 107);
  fmLayout();
  if (fmShot.empty() || !findShot(map, fmShot)) {
    auto all = shots(map);
    fmShot = all.empty() ? std::string() : all.front().name;
  }
  fmRefresh();
  SetTimer(fm, 2, 200, nullptr);
  ShowWindow(fm, quiet() ? SW_HIDE : SW_SHOWNORMAL);
}
void closeFilmmaker() {
  if (!fm)
    return;
  fmPlaying = false;
  fmSendPreview(false);
  DestroyWindow(fm);
}
void fmLayout() {
  RECT c{};
  GetClientRect(fm, &c);
  auto at = [&](int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(fm, id), x, y, w, h, TRUE); };
  const int lx = 12, lw = 230, rx = lx + lw + 16, rw = std::max(400, int(c.right) - rx - 12);
  at(101, lx, 12, lw, 20);
  at(FmShotList, lx, 34, lw, std::max(120, int(c.bottom) - 34 - 210));
  int y = std::max(160, int(c.bottom) - 168);
  at(FmNewShot, lx, y, lw / 2 - 3, 30);
  at(FmDeleteShot, lx + lw / 2 + 3, y, lw / 2 - 3, 30);
  y += 38;
  for (auto [label, box] : {std::pair{102, FmName}, {103, FmSeconds}, {104, FmRate}}) {
    at(label, lx, y + 4, 86, 20);
    at(box, lx + 90, y, lw - 90, 26);
    y += 30;
  }
  at(FmApplyShot, lx, y + 2, lw, 30);
  int x = rx;
  y = 12;
  at(FmPlay, x, y, 90, 30), x += 96;
  at(FmRewind, x, y, 90, 30), x += 100;
  at(105, x, y + 6, 170, 22), x += 176;
  at(FmPreview, x, y + 4, 150, 24), x += 156;
  at(FmRenderShot, rx + rw - 300, y, 120, 30);
  at(FmRenderAll, rx + rw - 174, y, 174, 30);
  y = 52;
  at(106, rx, y + 4, 50, 20);
  at(FmTrack, rx + 54, y, 300, 400);
  at(FmEase, rx + 370, y + 2, 200, 24);
  y = 88;
  x = rx;
  for (int i = 0; i < 6; ++i) {
    at(110 + i, x, y + 4, 44, 20);
    at(FmValue0 + i, x + 48, y, 66, 26);
    x += 122;
  }
  y = 122;
  at(FmClip, rx, y, 220, 400);
  at(FmOp, rx, y, 360, 26);
  at(FmSetKey, rx + rw - 430, y - 2, 140, 30);
  at(FmKeyView, rx + rw - 284, y - 2, 150, 30);
  at(FmDeleteKey, rx + rw - 128, y - 2, 128, 30);
  MoveWindow(fmTimeline, rx, 162, rw, std::max(80, int(c.bottom) - 162 - 40), TRUE);
  at(107, rx, int(c.bottom) - 30, rw, 22);
}
// The people and creatures of the map, as tracks.
std::vector<std::pair<std::string, std::wstring>> fmTrackChoices() const {
  std::vector<std::pair<std::string, std::wstring>> out{{"cam", L"Camera"}};
  for (auto &p : mapPeople(map))
    out.push_back({"a" + std::to_string(p.index), L"Person " + std::to_wstring(p.index) + L"  " + p.name});
  int k = 0;
  for (auto i : doc.records("CREATURE", map)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    out.push_back({"e" + std::to_string(k), L"Creature " + std::to_wstring(k) + L"  (kind " + wide(f.size() > 2 ? f[2] : "?") + L")"});
    ++k;
  }
  out.push_back({"set", L"Map state (door, lights, a switch)"});
  return out;
}
static std::wstring fmTrackLabel(const std::string &t, const std::vector<std::pair<std::string, std::wstring>> &choices) {
  for (auto &c : choices)
    if (c.first == t)
      return c.second;
  return wide(t);
}
// Everything from the document: the shot list, the shot's boxes, the tracks, the fields.
void fmRefresh() {
  if (!fm)
    return;
  fmSeenVersion = doc.version, fmSeenLines = doc.lines.size(), fmSeenMap = map;
  auto all = shots(map);
  if (!fmShot.empty() && !findShot(map, fmShot))
    fmShot = all.empty() ? std::string() : all.front().name;
  SendMessageW(fmShots, LB_RESETCONTENT, 0, 0);
  int pick = -1;
  for (size_t i = 0; i < all.size(); ++i) {
    auto keys = shotKeys(map, all[i].name);
    std::wostringstream row;
    row.setf(std::ios::fixed);
    row.precision(1);
    row << wide(all[i].name) << L"   " << all[i].seconds << L" s, " << keys.size() << L" keys";
    SendMessageW(fmShots, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.str().c_str()));
    if (all[i].name == fmShot)
      pick = int(i);
  }
  SendMessageW(fmShots, LB_SETCURSEL, pick, 0);
  auto s = fmCurrent();
  SetWindowTextW(fmName, s ? wide(s->name).c_str() : L"");
  SetWindowTextW(fmSeconds, s ? wide(trimmed(s->seconds)).c_str() : L"");
  SetWindowTextW(fmRate, s ? std::to_wstring(s->fps).c_str() : L"");
  if (s)
    fmTime = std::clamp(fmTime, 0.f, s->seconds);
  auto choices = fmTrackChoices();
  SendMessageW(fmTrackList, CB_RESETCONTENT, 0, 0);
  fmTrackIds.clear();
  int chosen = 0;
  for (auto &[id, label] : choices) {
    if (id == fmTrack)
      chosen = int(fmTrackIds.size());
    fmTrackIds.push_back(id);
    SendMessageW(fmTrackList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
  }
  fmTrack = fmTrackIds[size_t(chosen)];
  SendMessageW(fmTrackList, CB_SETCURSEL, chosen, 0);
  SetWindowTextW(fm, (L"Raw Metal Filmmaker  -  map " + std::to_wstring(map) + (s ? L"  -  " + wide(s->name) : std::wstring())).c_str());
  fmFields();
  fmMoved();
}
// The key under the playhead on the picked track, if there is one.
std::optional<ShotKey> fmKeyHere() const {
  if (fmShot.empty())
    return {};
  float half = fmFrame() * .5f;
  for (auto &k : shotKeys(map, fmShot, fmTrack))
    if (std::abs(k.time - fmTime) < half)
      return k;
  return {};
}
// The boxes show the key under the playhead, or the track as it stands there.
void fmFields() {
  if (!fm)
    return;
  bool cam = fmTrack == "cam", state = fmTrack == "set", body = !cam && !state;
  for (int i = 0; i < 6; ++i) {
    bool shown = cam || (body && i < 4);
    ShowWindow(fmValues[size_t(i)], shown ? SW_SHOW : SW_HIDE);
    ShowWindow(fmValueLabels[size_t(i)], shown ? SW_SHOW : SW_HIDE);
  }
  SetWindowTextW(fmValueLabels[2], cam ? L"Eye" : L"Feet");
  ShowWindow(fmClipList, body ? SW_SHOW : SW_HIDE);
  ShowWindow(fmOp, state ? SW_SHOW : SW_HIDE);
  auto here = fmKeyHere();
  SendMessageW(fmEase, BM_SETCHECK, here && here->ease ? BST_CHECKED : BST_UNCHECKED, 0);
  if (state) {
    SetWindowTextW(fmOp, here ? wide(here->op).c_str() : L"");
    return;
  }
  std::array<float, 6> v{};
  int clip = 0;
  if (here)
    v = here->v, clip = int(std::lround(here->v[4]));
  else if (cam)
    v = fmShot.empty() ? shotStartCamera(map) : shotCamera(map, fmShot, fmTime);
  else {
    auto keys = fmShot.empty() ? std::vector<ShotKey>{} : shotKeys(map, fmShot, fmTrack);
    if (!keys.empty()) {
      auto s = sampleShotActor(keys, fmTime, fmTrack[0] == 'e');
      v = s.v, clip = int(std::lround(s.v[4]));
    } else
      v = fmAuthoredPose(fmTrack);
  }
  for (int i = 0; i < 6; ++i)
    SetWindowTextW(fmValues[size_t(i)], wide(trimmed(v[size_t(i)])).c_str());
  SendMessageW(fmClipList, CB_SETCURSEL, std::clamp(clip, 0, 9), 0);
}
// Where a person or creature stands as authored (their first key, the creature's spot).
std::array<float, 6> fmAuthoredPose(const std::string &track) const {
  std::array<float, 6> v{};
  int index = std::atoi(track.c_str() + 1);
  if (track[0] == 'a') {
    for (auto i : doc.records("ACTOR_KEY", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 7 && int(number(f[2], -1)) == index) {
        v = {number(f[4], 0), number(f[5], 0), number(f[6], 0), number(f[7], 0) * 180.f / 3.14159265f, 0, 0};
        break;
      }
    }
  } else {
    auto list = doc.records("CREATURE", map);
    if (index >= 0 && size_t(index) < list.size()) {
      auto f = NativeDocument::fields(doc.lines[list[size_t(index)]]);
      v = {f.size() > 3 ? number(f[3], 0) : 0, f.size() > 4 ? number(f[4], 0) : 0, f.size() > 5 ? number(f[5], 0) : 0, 0, 0, 0};
    }
  }
  return v;
}
// After the playhead moves: the clock, the boxes, the timeline and the 3D view.
void fmMoved() {
  if (!fm)
    return;
  auto s = fmCurrent();
  wchar_t clock[64];
  std::swprintf(clock, 64, L"%.2f s / %.2f s  (frame %d)", fmTime, s ? s->seconds : 0.f, int(std::lround(fmTime * float(s ? s->fps : 30))));
  SetWindowTextW(fmClock, clock);
  if (!fmPlaying)
    fmFields();
  InvalidateRect(fmTimeline, nullptr, FALSE);
  fmSendPreview(fmLive && s.has_value());
}
// The shot at the playhead, to the live 3D view (EditorPreview.cpp reads film.txt).
std::string fmPreviewText() const {
  if (fmShot.empty())
    return "FILM 0\n";
  auto c = shotCamera(map, fmShot, fmTime);
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  out << "FILM 1 " << c[0] << ' ' << c[1] << ' ' << c[2] << ' ' << c[3] << ' ' << c[4] << ' ' << c[5] << '\n' << shotPoses(map, fmShot, fmTime);
  return out.str();
}
void fmSendPreview(bool on) {
  if (control.empty())
    return;
  auto text = on ? fmPreviewText() : std::string("FILM 0\n");
  if (text == fmPreviewSent)
    return;
  fmPreviewSent = text;
  try {
    channelPut(control.parent_path() / L"film.txt", text);
  } catch (...) {
  }
}
// The free camera of the live 3D view (film-status.txt): X Y EYE TURN TILT.
std::optional<std::array<float, 5>> fmViewCamera() const {
  if (control.empty())
    return {};
  std::istringstream in(channelText(control.parent_path() / L"film-status.txt"));
  std::string tag;
  std::array<float, 5> v{};
  if (in >> tag >> v[0] >> v[1] >> v[2] >> v[3] >> v[4] && tag == "VIEW")
    return v;
  return {};
}
// ---- Edits: each one Undo -------------------------------------------------------------
void fmPublish() {
  publish();
  fmRefresh();
}
// A new shot: from the picked Film camera (its still or its move), else from the 3D view,
// else from the map's start. Three seconds at 30 frames a second.
std::string newShot(std::string name = "") {
  if (name.empty())
    name = uniqueName(ShotKind, 2, "shot_");
  float seconds = 3;
  std::vector<std::array<float, 7>> cams; // time, x y eye turn tilt fov
  if (filmPicked()) {
    auto c = filmCamera(NativeDocument::fields(doc.lines[size_t(record)]));
    cams.push_back({0, c.ax, c.ay, c.az, c.yaw, c.pitch, 0});
    if (c.seconds > 0) {
      seconds = std::max(1.f, c.seconds);
      cams.push_back({c.seconds, c.bx, c.by, c.bz, c.byaw, c.bpitch, 0});
    }
  } else if (auto v = fmViewCamera(); v && !editingTest)
    cams.push_back({0, (*v)[0], (*v)[1], (*v)[2], (*v)[3], (*v)[4], 0});
  remember();
  doc.add({ShotKind, std::to_string(map), encoded(name), nativeNumber(seconds), "30"});
  for (auto &c : cams)
    doc.add({ShotKeyKind, std::to_string(map), encoded(name), "cam", nativeNumber(c[0]), "1", nativeNumber(c[1]), nativeNumber(c[2]),
             nativeNumber(c[3]), nativeNumber(std::round(c[4])), nativeNumber(std::round(c[5])), "0"});
  fmShot = name, fmTime = 0, fmTrack = "cam";
  fmPublish();
  fmSay(L"New shot " + wide(name) + (cams.empty() ? L". Fly the 3D view and press Key the 3D view, or type a camera and Set key."
                                                   : L", its camera keyed from " + std::wstring(filmPicked() ? L"the picked Film camera." : L"the 3D view.")));
  return name;
}
void deleteShot() {
  auto s = fmCurrent();
  if (!s)
    return;
  remember();
  auto keys = shotKeys(map, s->name);
  std::vector<size_t> gone{s->line};
  for (auto &k : keys)
    gone.push_back(k.line);
  std::sort(gone.rbegin(), gone.rend());
  for (auto i : gone)
    doc.eraseRecord(i);
  fmSay(L"Shot " + wide(s->name) + L" deleted. Ctrl+Z brings it back.");
  fmShot.clear();
  fmPublish();
}
// Name, length and rate from the boxes; a rename carries the shot's keys.
bool applyShot(const std::string &name, float seconds, int fps) {
  auto s = fmCurrent();
  if (!s || name.empty())
    return false;
  if (name != s->name && findShot(map, name)) {
    fmSay(L"Another shot is called " + wide(name) + L".");
    return false;
  }
  remember();
  for (auto &k : shotKeys(map, s->name)) {
    auto f = NativeDocument::fields(doc.lines[k.line]);
    f[2] = encoded(name);
    doc.replace(k.line, f);
  }
  doc.replace(s->line, {ShotKind, std::to_string(map), encoded(name), nativeNumber(std::clamp(seconds, .1f, 600.f)), std::to_string(shotFps(float(fps)))});
  fmShot = name;
  fmPublish();
  fmSay(L"Shot " + wide(name) + L": " + wide(trimmed(seconds)) + L" s at " + std::to_wstring(shotFps(float(fps))) + L" frames a second.");
  return true;
}
// A key at the playhead on a track (replacing the one there). values: the six numbers, or
// op for a state key.
void setShotKey(const std::string &track, float time, const std::array<float, 6> &values, bool ease, const std::string &op = "") {
  auto s = fmCurrent();
  if (!s)
    return;
  time = std::clamp(std::round(time * float(s->fps)) / float(s->fps), 0.f, s->seconds);
  std::vector<std::string> f{ShotKeyKind, std::to_string(map), encoded(s->name), track, nativeNumber(time), ease ? "1" : "0"};
  if (track == "set")
    f.push_back(encoded(op));
  else
    for (size_t i = 0; i < 6; ++i)
      f.push_back(nativeNumber(i == 4 && track != "cam" ? std::round(values[i]) : values[i]));
  remember();
  bool replaced = false;
  for (auto &k : shotKeys(map, s->name, track))
    if (std::abs(k.time - time) < .5f / float(s->fps)) {
      doc.replace(k.line, f);
      replaced = true;
      break;
    }
  if (!replaced)
    doc.add(f);
  fmTrack = track, fmTime = time;
  fmPublish();
  fmSay(std::wstring(replaced ? L"Key changed" : L"Key set") + L" at " + wide(trimmed(time)) + L" s. Ctrl+Z takes it back.");
}
void deleteShotKey() {
  auto here = fmKeyHere();
  if (!here) {
    fmSay(L"No key under the playhead on this track. Click a key on the timeline first.");
    return;
  }
  remember();
  doc.eraseRecord(here->line);
  fmPublish();
  fmSay(L"Key deleted. Ctrl+Z brings it back.");
}
void retimeShotKey(const ShotKey &k, float time) {
  auto s = fmCurrent();
  if (!s)
    return;
  time = std::clamp(std::round(time * float(s->fps)) / float(s->fps), 0.f, s->seconds);
  if (std::abs(time - k.time) < .5f / float(s->fps))
    return;
  remember();
  auto f = NativeDocument::fields(doc.lines[k.line]);
  f[4] = nativeNumber(time);
  doc.replace(k.line, f);
  fmTime = time;
  fmPublish();
  fmSay(L"Key moved to " + wide(trimmed(time)) + L" s.");
}
// Set key from the boxes.
void fmSetKeyFromBoxes() {
  if (!fmCurrent()) {
    fmSay(L"Make a shot first: + New shot.");
    return;
  }
  bool ease = SendMessageW(fmEase, BM_GETCHECK, 0, 0) == BST_CHECKED;
  if (fmTrack == "set") {
    wchar_t text[256]{};
    GetWindowTextW(fmOp, text, 256);
    auto op = utf8(text);
    if (!shotOpAllowed(op)) {
      fmSay(L"A map state is an --film op: set NAME=V, door I OPEN, lights S, ambient A, torch, shut, dark or lamp.");
      return;
    }
    setShotKey("set", fmTime, {}, false, op);
    return;
  }
  std::array<float, 6> v{};
  for (int i = 0; i < 6; ++i) {
    wchar_t text[64]{};
    GetWindowTextW(fmValues[size_t(i)], text, 64);
    v[size_t(i)] = number(utf8(text), 0);
  }
  if (fmTrack != "cam")
    v[4] = float(std::max(0, int(SendMessageW(fmClipList, CB_GETCURSEL, 0, 0)))), v[5] = 0;
  setShotKey(fmTrack, fmTime, v, ease);
}
void fmKeyTheView() {
  auto v = fmViewCamera();
  if (!v) {
    fmSay(L"The 3D view has not said where it is yet. Show the 3D view and try again.");
    return;
  }
  wchar_t lens[64]{};
  GetWindowTextW(fmValues[5], lens, 64);
  fmTrack = "cam";
  setShotKey("cam", fmTime, {(*v)[0], (*v)[1], (*v)[2], std::round((*v)[3]), std::round((*v)[4]), number(utf8(lens), 0)},
             SendMessageW(fmEase, BM_GETCHECK, 0, 0) == BST_CHECKED);
}
void fmRender(bool all) {
  if (fmWorker) {
    fmSay(L"A film is still rendering. Its folder opens when it is done.");
    return;
  }
  if (!all && fmShot.empty()) {
    fmSay(L"Make a shot first: + New shot.");
    return;
  }
  std::vector<ShotJob> jobs;
  try {
    jobs = writeShotLists(map, all ? std::vector<std::string>{} : std::vector<std::string>{fmShot});
  } catch (const std::exception &error) {
    fmSay(L"Could not write the film list: " + wide(error.what()));
    return;
  }
  if (jobs.empty()) {
    fmSay(L"No shots on this map yet: + New shot.");
    return;
  }
  struct Work {
    fs::path engine, cut;
    std::vector<ShotJob> jobs;
    HWND window;
  };
  auto *work = new Work{filmEngine(), shotPictures() / (filmStem() + L".mp4"), jobs, window};
  fmSay(L"Filming " + std::to_wstring(jobs.size()) + L" shot" + (jobs.size() == 1 ? L"" : L"s") + L" with the engine...");
  fmWorker = CreateThread(
      nullptr, 0,
      [](void *raw) -> DWORD {
        std::unique_ptr<Work> w(static_cast<Work *>(raw));
        auto *said = new std::wstring;
        int frames = renderShotJobs(w->engine, w->jobs, w->cut, *said);
        *said += L"\n" + w->cut.parent_path().wstring();
        PostMessageW(w->window, WM_APP + 53, WPARAM(frames), reinterpret_cast<LPARAM>(said));
        return 0;
      },
      work, 0, nullptr);
  if (!fmWorker) {
    delete work;
    fmSay(L"Could not start the render.");
  }
}
// The render's end (WM_APP + 53 to the main window): say what was made and open the folder.
void shotsRendered(int frames, std::wstring *said) {
  std::unique_ptr<std::wstring> owned(said);
  if (fmWorker)
    CloseHandle(fmWorker), fmWorker = nullptr;
  auto cut = owned->find(L'\n');
  std::wstring folder = cut == std::wstring::npos ? std::wstring() : owned->substr(cut + 1);
  fmSay((frames > 0 ? L"Filmed: " : L"The engine made no pictures: ") + owned->substr(0, cut) + L".");
  if (frames > 0 && !quiet() && !folder.empty())
    ShellExecuteW(window, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
// ---- The timeline ----------------------------------------------------------------------
struct FmLanes {
  RECT r{};
  int labelW = 170, ruler = 26, lane = 24;
  float seconds = 3;
  std::vector<std::string> rows;
  int x(float t) const { return labelW + int(std::lround(t / std::max(.1f, seconds) * float(std::max(10, int(r.right) - labelW - 14)))); }
  float time(int px) const { return float(px - labelW) / float(std::max(10, int(r.right) - labelW - 14)) * seconds; }
  int top(size_t row) const { return ruler + 6 + int(row) * lane; }
};
FmLanes fmLanes() const {
  FmLanes l;
  GetClientRect(fmTimeline, &l.r);
  auto s = fmCurrent();
  l.seconds = s ? s->seconds : 3;
  l.rows = {"cam"};
  if (s)
    for (auto &t : shotTracks(map, s->name))
      if (t != "cam" && t != "set")
        l.rows.push_back(t);
  if (fmTrack != "cam" && fmTrack != "set" && std::find(l.rows.begin(), l.rows.end(), fmTrack) == l.rows.end())
    l.rows.push_back(fmTrack);
  l.rows.push_back("set");
  return l;
}
void paintTimeline(HDC dc) {
  auto l = fmLanes();
  const int w = l.r.right, h = l.r.bottom;
  HDC mem = CreateCompatibleDC(dc);
  HBITMAP bmp = CreateCompatibleBitmap(dc, std::max(1, w), std::max(1, h));
  auto oldBmp = SelectObject(mem, bmp);
  auto fill = [&](RECT rc, COLORREF c) {
    auto b = CreateSolidBrush(c);
    FillRect(mem, &rc, b);
    DeleteObject(b);
  };
  fill({0, 0, w, h}, RGB(0x14, 0x16, 0x1B));
  fill({0, 0, w, l.ruler}, RGB(0x24, 0x27, 0x2E));
  SetBkMode(mem, TRANSPARENT);
  auto oldFont = SelectObject(mem, smallFont ? smallFont : font);
  auto s = fmCurrent();
  auto choices = fmTrackChoices();
  // The ruler: a tick each tenth of the length's step, a number each step.
  float step = l.seconds <= 1 ? .1f : l.seconds <= 4 ? .5f :l.seconds <= 12 ? 1.f : l.seconds <= 40 ? 5.f : 10.f;
  auto grid = CreatePen(PS_SOLID, 1, RGB(0x34, 0x38, 0x40)), tick = CreatePen(PS_SOLID, 1, RGB(0x90, 0x94, 0x9A));
  auto oldPen = SelectObject(mem, grid);
  SetTextColor(mem, RGB(0xC8, 0xC4, 0xBA));
  for (float t = 0; t <= l.seconds + 1e-3f; t += step) {
    int x = l.x(t);
    SelectObject(mem, grid);
    MoveToEx(mem, x, l.ruler, nullptr);
    LineTo(mem, x, h);
    SelectObject(mem, tick);
    MoveToEx(mem, x, l.ruler - 8, nullptr);
    LineTo(mem, x, l.ruler);
    wchar_t label[16];
    std::swprintf(label, 16, step < 1 ? L"%.1f" : L"%.0f", t);
    TextOutW(mem, x + 3, 4, label, int(wcslen(label)));
  }
  if (s && s->fps > 0 && l.x(1.f / float(s->fps)) - l.x(0) >= 4) // frames, when there is room to see them
    for (int f = 0; float(f) / float(s->fps) <= l.seconds; ++f) {
      int x = l.x(float(f) / float(s->fps));
      SelectObject(mem, tick);
      MoveToEx(mem, x, l.ruler - 3, nullptr);
      LineTo(mem, x, l.ruler);
    }
  // The lanes: a name, a band, and the keys (a diamond; a dot when it eases out).
  for (size_t row = 0; row < l.rows.size(); ++row) {
    const auto &t = l.rows[row];
    int y = l.top(row);
    bool picked = t == fmTrack;
    fill({0, y, w, y + l.lane - 2}, picked ? RGB(0x23, 0x2C, 0x26) : RGB(0x1B, 0x1E, 0x24));
    COLORREF tint = t == "cam" ? ui::Film : t == "set" ? ui::Logic : t[0] == 'a' ? ui::People : ui::Creature;
    fill({0, y, 4, y + l.lane - 2}, tint);
    SetTextColor(mem, picked ? RGB(0xFF, 0xFF, 0xFF) : RGB(0xC8, 0xC4, 0xBA));
    auto name = fmTrackLabel(t, choices);
    RECT text{10, y + 3, l.labelW - 6, y + l.lane - 2};
    DrawTextW(mem, name.c_str(), int(name.size()), &text, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (!s)
      continue;
    auto keys = shotKeys(map, s->name, t);
    // A person's or creature's clips, as a strip under its keys.
    if (t != "cam" && t != "set")
      for (size_t k = 0; k < keys.size(); ++k) {
        int a = l.x(keys[k].time), b = k + 1 < keys.size() ? l.x(keys[k + 1].time) : l.x(l.seconds);
        int clip = int(std::lround(keys[k].v[4]));
        COLORREF band = clip == 1 ? RGB(0x2E, 0x5E, 0x3A) : clip == 4 ? RGB(0x5E, 0x2A, 0x2A) : RGB(0x30, 0x36, 0x40);
        fill({a, y + l.lane - 8, std::max(a + 1, b), y + l.lane - 4}, band);
      }
    for (auto &k : keys) {
      float when = fmDragging && fmKeyTrack == t && std::abs(k.time - fmKeyTime) < 1e-4f ? fmDragTime : k.time;
      int x = l.x(when), cy = y + (l.lane - 2) / 2 - 2;
      bool sel = picked && std::abs(k.time - fmTime) < fmFrame() * .5f;
      auto pen = CreatePen(PS_SOLID, sel ? 2 : 1, sel ? ui::Selected : RGB(0x10, 0x10, 0x10));
      auto brush = CreateSolidBrush(tint);
      auto op = SelectObject(mem, pen);
      auto ob = SelectObject(mem, brush);
      if (k.ease)
        Ellipse(mem, x - 6, cy - 6, x + 7, cy + 7);
      else {
        POINT d[4] = {{x, cy - 7}, {x + 7, cy}, {x, cy + 7}, {x - 7, cy}};
        Polygon(mem, d, 4);
      }
      SelectObject(mem, ob);
      SelectObject(mem, op);
      DeleteObject(brush);
      DeleteObject(pen);
    }
  }
  // The playhead.
  if (s) {
    int x = l.x(fmTime);
    auto head = CreatePen(PS_SOLID, 2, ui::Terracotta);
    SelectObject(mem, head);
    MoveToEx(mem, x, 0, nullptr);
    LineTo(mem, x, h);
    POINT cap[3] = {{x - 6, 0}, {x + 6, 0}, {x, 9}};
    auto b = CreateSolidBrush(ui::Terracotta);
    auto ob = SelectObject(mem, b);
    Polygon(mem, cap, 3);
    SelectObject(mem, ob);
    DeleteObject(b);
    SelectObject(mem, oldPen);
    DeleteObject(head);
  } else {
    SetTextColor(mem, RGB(0xC8, 0xC4, 0xBA));
    const wchar_t *hint = L"No shot yet. + New shot makes one from the picked Film camera, the 3D view or the map's start.";
    TextOutW(mem, l.labelW, l.ruler + 40, hint, int(wcslen(hint)));
  }
  SelectObject(mem, oldPen);
  DeleteObject(grid);
  DeleteObject(tick);
  SelectObject(mem, oldFont);
  BitBlt(dc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
  SelectObject(mem, oldBmp);
  DeleteObject(bmp);
  DeleteDC(mem);
}
// A press on the timeline: on a key picks it (and a drag retimes it); on a lane picks the
// track; anywhere scrubs.
void timelinePress(int px, int py) {
  auto l = fmLanes();
  auto s = fmCurrent();
  if (!s)
    return;
  SetFocus(fmTimeline);
  fmPlaying = false;
  SetWindowTextW(fmPlay, L"▶ Play");
  if (py > l.ruler)
    for (size_t row = 0; row < l.rows.size(); ++row) {
      int y = l.top(row);
      if (py < y || py >= y + l.lane)
        continue;
      if (fmTrack != l.rows[row]) {
        fmTrack = l.rows[row];
        int i = int(std::find(fmTrackIds.begin(), fmTrackIds.end(), fmTrack) - fmTrackIds.begin());
        SendMessageW(fmTrackList, CB_SETCURSEL, i < int(fmTrackIds.size()) ? i : -1, 0);
      }
      for (auto &k : shotKeys(map, s->name, fmTrack))
        if (std::abs(l.x(k.time) - px) <= 7) {
          fmTime = k.time;
          fmDragging = true, fmKeyTrack = fmTrack, fmKeyTime = fmDragTime = k.time;
          SetCapture(fmTimeline);
          fmMoved();
          return;
        }
    }
  if (px >= l.labelW - 4) {
    fmScrubbing = true;
    SetCapture(fmTimeline);
    fmTime = fmSnap(l.time(px));
  }
  fmMoved();
}
void timelineDrag(int px) {
  auto l = fmLanes();
  if (fmDragging) {
    fmDragTime = fmSnap(l.time(px));
    fmTime = fmDragTime;
    fmMoved();
  } else if (fmScrubbing) {
    fmTime = fmSnap(l.time(px));
    fmMoved();
  }
}
void timelineRelease() {
  if (fmDragging) {
    fmDragging = false;
    for (auto &k : shotKeys(map, fmShot, fmKeyTrack))
      if (std::abs(k.time - fmKeyTime) < 1e-4f) {
        retimeShotKey(k, fmDragTime);
        break;
      }
  }
  fmScrubbing = false;
  ReleaseCapture();
  fmMoved();
}
void fmTogglePlay() {
  auto s = fmCurrent();
  if (!s)
    return;
  fmPlaying = !fmPlaying;
  if (fmPlaying && fmTime >= s->seconds - 1e-3f)
    fmTime = 0;
  fmPlayedAt = GetTickCount64();
  SetWindowTextW(fmPlay, fmPlaying ? L"⏸ Pause" : L"▶ Play");
  if (fmPlaying)
    SetTimer(fm, 1, 33, nullptr);
  else
    KillTimer(fm, 1), fmTime = fmSnap(fmTime), fmMoved();
}
void fmTick() {
  auto s = fmCurrent();
  if (!fmPlaying || !s) {
    KillTimer(fm, 1);
    return;
  }
  auto now = GetTickCount64();
  fmTime += float(now - fmPlayedAt) / 1000.f;
  fmPlayedAt = now;
  if (fmTime >= s->seconds)
    fmTime = 0; // it loops while it plays
  fmMoved();
}
static LRESULT CALLBACK timelineProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    e = static_cast<Editor *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (!e)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    e->paintTimeline(dc);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1;
  case WM_LBUTTONDOWN:
    e->timelinePress(GET_X_LPARAM(l), GET_Y_LPARAM(l));
    return 0;
  case WM_MOUSEMOVE:
    if (GetCapture() == h)
      e->timelineDrag(GET_X_LPARAM(l));
    return 0;
  case WM_LBUTTONUP:
    if (GetCapture() == h)
      e->timelineRelease();
    return 0;
  case WM_GETDLGCODE:
    return DLGC_WANTARROWS | DLGC_WANTCHARS;
  case WM_KEYDOWN:
    if (w == VK_SPACE)
      e->fmTogglePlay();
    else if (w == VK_LEFT || w == VK_RIGHT)
      e->fmTime = e->fmSnap(e->fmTime + (w == VK_LEFT ? -1 : 1) * e->fmFrame()), e->fmMoved();
    else if (w == 'K')
      e->fmSetKeyFromBoxes();
    else if (w == VK_DELETE)
      e->deleteShotKey();
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK filmmakerProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    e = static_cast<Editor *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (!e)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_SIZE:
    if (e->fm)
      e->fmLayout();
    return 0;
  case WM_GETMINMAXINFO:
    reinterpret_cast<MINMAXINFO *>(l)->ptMinTrackSize = {980, 520};
    return 0;
  case WM_CTLCOLORSTATIC: {
    static HBRUSH paper = CreateSolidBrush(ui::Paper);
    SetBkColor(reinterpret_cast<HDC>(w), ui::Paper);
    SetTextColor(reinterpret_cast<HDC>(w), ui::Ink);
    return reinterpret_cast<LRESULT>(paper);
  }
  case WM_TIMER:
    if (w == 1)
      e->fmTick();
    else if (w == 2 && (e->doc.version != e->fmSeenVersion || e->doc.lines.size() != e->fmSeenLines || e->map != e->fmSeenMap))
      e->fmRefresh(); // Undo, Redo, another map, an edit elsewhere
    return 0;
  case WM_COMMAND: {
    int id = LOWORD(w), code = HIWORD(w);
    auto text = [&](HWND box) {
      wchar_t t[256]{};
      GetWindowTextW(box, t, 256);
      return utf8(t);
    };
    switch (id) {
    case FmShotList:
      if (code == LBN_SELCHANGE) {
        auto all = e->shots(e->map);
        int i = int(SendMessageW(e->fmShots, LB_GETCURSEL, 0, 0));
        if (i >= 0 && i < int(all.size()))
          e->fmShot = all[size_t(i)].name, e->fmTime = 0, e->fmRefresh();
      }
      return 0;
    case FmNewShot:
      e->newShot();
      return 0;
    case FmDeleteShot:
      e->deleteShot();
      return 0;
    case FmApplyShot:
      e->applyShot(text(e->fmName), number(text(e->fmSeconds), 3), int(number(text(e->fmRate), 30)));
      return 0;
    case FmPlay:
      e->fmTogglePlay();
      return 0;
    case FmRewind:
      e->fmTime = 0, e->fmMoved();
      return 0;
    case FmPreview:
      e->fmLive = SendMessageW(e->fmPreview, BM_GETCHECK, 0, 0) == BST_CHECKED;
      e->fmSendPreview(e->fmLive && e->fmCurrent().has_value());
      e->fmSay(e->fmLive ? L"The 3D view shows the shot at the playhead." : L"The 3D view is yours again: fly it, then Key the 3D view.");
      return 0;
    case FmRenderShot:
    case FmRenderAll:
      e->fmRender(id == FmRenderAll);
      return 0;
    case FmTrack:
      if (code == CBN_SELCHANGE) {
        int i = int(SendMessageW(e->fmTrackList, CB_GETCURSEL, 0, 0));
        if (i >= 0 && i < int(e->fmTrackIds.size()))
          e->fmTrack = e->fmTrackIds[size_t(i)], e->fmFields(), InvalidateRect(e->fmTimeline, nullptr, FALSE);
      }
      return 0;
    case FmSetKey:
      e->fmSetKeyFromBoxes();
      return 0;
    case FmKeyView:
      e->fmKeyTheView();
      return 0;
    case FmDeleteKey:
      e->deleteShotKey();
      return 0;
    }
    break;
  }
  case WM_CLOSE:
    e->closeFilmmaker();
    return 0;
  case WM_DESTROY:
    KillTimer(h, 1), KillTimer(h, 2);
    e->fm = e->fmTimeline = nullptr;
    e->fmPlaying = false;
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
// ---- --filmmaker-render: every shot of a project, from the command line ----------------
//   RawMetalEditor.exe --filmmaker-render "PROJECT.rmc" [MAP]
// films each map's shots (or one map's) beside the project and writes
// native-editor-filmmaker-render.txt. Exit 0 when every shot made its frames.
int renderProjectShots(const fs::path &project, int only) {
  std::ofstream report(root / L"native-editor-filmmaker-render.txt");
  try {
    doc.open(project);
    path = project;
    dirty = false;
    std::set<int> maps;
    for (auto i : doc.records(ShotKind))
      maps.insert(int(number(NativeDocument::fields(doc.lines[i])[1], -1)));
    int frames = 0, shotCount = 0;
    for (int m : maps) {
      if (m < 0 || (only >= 0 && m != only))
        continue;
      map = m;
      auto jobs = writeShotLists(m, {});
      std::wstring said;
      int made = renderShotJobs(filmEngine(), jobs, shotPictures() / (filmStem() + L".mp4"), said);
      frames += made, shotCount += int(jobs.size());
      report << "map " << m << ": " << utf8(said) << '\n';
      for (auto &j : jobs)
        report << "  " << j.shot << ": " << utf8(j.film.list.wstring()) << " -> " << utf8(j.film.folder.wstring())
               << (fs::exists(j.movie) ? " + " + utf8(j.movie.wstring()) : std::string()) << '\n';
    }
    report << (frames > 0 && shotCount > 0 ? "PASS" : "FAIL") << ": " << shotCount << " shots, " << frames << " frames\n";
    return frames > 0 && shotCount > 0 ? 0 : 1;
  } catch (const std::exception &error) {
    report << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
// --editing-test (from verifyInteractions): the window's own buttons make a shot, two
// camera keys and a person's key; the timeline scrubs and retimes; Undo; save and reopen
// keep every key; the film list holds every frame's pose, camera and lens; the engine
// films a short take of it. filmmaker-window.bmp and "Filmmaker test film" under
// .build/interaction are what it made.
template <class Reset> void verifyFilmmaker(std::vector<std::string> &passed, Reset &reset) {
  auto require = [](bool okay, const std::string &message) {
    if (!okay)
      throw std::runtime_error("Filmmaker: " + message);
  };
  auto approx = [](float a, float b, float tolerance = .01f) { return std::abs(a - b) <= tolerance; };
  auto exportPath = path;
  auto folder = root / L".build" / L"interaction";
  fs::create_directories(folder);
  reset();
  // Someone to direct: a Worker standing on the map with its own clock (as the Starter
  // Map's display people stand).
  doc.add({"TIMED_SEQUENCE", "0", "film_clock", "film_clock_done", "0", "0", "24", "24", "-100", "100", "60000", "60000", "0", "0",
           "0", "0", "1", "1"});
  doc.add({"ACTOR", "0", "film_clock", "0", "1.75", "0", "0", "0"});
  doc.add({"ACTOR_KEY", "0", "0", "0", "6", "12", "0", "0", "0", "0"});
  doc.add({"ACTOR_KEY", "0", "0", "60000", "6", "12", "0", "0", "0", "1"});
  require(validateDocument(), "the engine refused the test map's person");
  selectRecord(-1);
  openFilmmaker();
  require(fm && fmTimeline && fmShots && fmTrackList, "the window did not open");
  auto historyCount = history.size();
  SendMessageW(fm, WM_COMMAND, FmNewShot, 0);
  auto all = shots(0);
  require(all.size() == 1 && all[0].name == "shot_1" && all[0].fps == 30 && approx(all[0].seconds, 3) && fmShot == "shot_1",
          "+ New shot did not make shot_1, 3 s at 30 fps");
  require(SendMessageW(fmShots, LB_GETCOUNT, 0, 0) == 1 && SendMessageW(fmShots, LB_GETCURSEL, 0, 0) == 0, "the shot list does not show it");
  auto boxes = [&](std::array<float, 6> v) {
    for (int i = 0; i < 6; ++i)
      SetWindowTextW(fmValues[size_t(i)], wide(trimmed(v[size_t(i)])).c_str());
  };
  auto pickTrack = [&](const std::string &t) {
    int i = int(std::find(fmTrackIds.begin(), fmTrackIds.end(), t) - fmTrackIds.begin());
    require(i < int(fmTrackIds.size()), "no track " + t + " to pick");
    SendMessageW(fmTrackList, CB_SETCURSEL, i, 0);
    SendMessageW(fm, WM_COMMAND, MAKEWPARAM(FmTrack, CBN_SELCHANGE), reinterpret_cast<LPARAM>(fmTrackList));
    require(fmTrack == t, "the track list did not pick " + t);
  };
  // Two camera keys: 0 s eased out, then 2 s; the lens narrows from 60 to 40 degrees.
  pickTrack("cam");
  fmTime = 0, fmMoved();
  boxes({4, 10, 1.6f, 90, -5, 60});
  SendMessageW(fmEase, BM_SETCHECK, BST_CHECKED, 0);
  SendMessageW(fm, WM_COMMAND, FmSetKey, 0);
  fmTime = 2, fmMoved();
  boxes({8, 10, 1.8f, 120, 0, 40});
  SendMessageW(fmEase, BM_SETCHECK, BST_UNCHECKED, 0);
  SendMessageW(fm, WM_COMMAND, FmSetKey, 0);
  // The person: picked from the track list, the playhead scrubbed to 1 s on the
  // timeline's ruler, keyed walking at 6, 13 facing south.
  pickTrack("a0");
  auto lanes = fmLanes();
  timelinePress(lanes.x(1.f), 8);
  timelineRelease();
  require(approx(fmTime, 1.f, .02f), "scrubbing the ruler did not put the playhead at 1 s");
  boxes({6, 13, 0, 90, 0, 0});
  SendMessageW(fmClipList, CB_SETCURSEL, 1, 0);
  SendMessageW(fm, WM_COMMAND, FmSetKey, 0);
  auto cams = shotKeys(0, "shot_1", "cam"), person = shotKeys(0, "shot_1", "a0");
  require(cams.size() == 2 && approx(cams[0].time, 0) && cams[0].ease && approx(cams[0].v[5], 60) && approx(cams[1].time, 2) &&
              !cams[1].ease && approx(cams[1].v[0], 8) && approx(cams[1].v[3], 120),
          "the camera keys are not as set");
  require(person.size() == 1 && approx(person[0].time, 1) && approx(person[0].v[0], 6) && approx(person[0].v[1], 13) &&
              int(person[0].v[4]) == 1,
          "the person's key is not as set");
  require(history.size() == historyCount + 4, "a shot and three keys are not four Undo");
  // Between the camera keys it eases out of the first (a straight line would be at 5).
  auto mid = shotCamera(0, "shot_1", .5f);
  require(approx(mid[0], 4.625f) && approx(mid[5], 56.875f), "the camera does not ease between its keys");
  // Retime: drag the person's key from 1 s to 1.5 s on its lane; Undo puts it back.
  lanes = fmLanes();
  size_t row = size_t(std::find(lanes.rows.begin(), lanes.rows.end(), "a0") - lanes.rows.begin());
  require(row < lanes.rows.size(), "the timeline has no lane for the person");
  timelinePress(lanes.x(1.f), lanes.top(row) + 8);
  require(fmDragging, "pressing the person's key did not pick it");
  timelineDrag(lanes.x(1.5f));
  timelineRelease();
  require(approx(shotKeys(0, "shot_1", "a0")[0].time, 1.5f), "dragging the key did not retime it");
  SendMessageW(window, WM_COMMAND, Undo, 0);
  fmRefresh();
  require(approx(shotKeys(0, "shot_1", "a0")[0].time, 1.f), "Undo did not put the key back");
  // The live view's text: the shot's camera at the playhead and the person posed.
  fmTime = 1, fmMoved();
  auto preview = fmPreviewText();
  require(preview.starts_with("FILM 1 ") && preview.find("\npose a0 6.000 13.000 0.000 90.000 1 ") != std::string::npos,
          "the 3D view preview is not the shot: " + preview);
  // Save, reopen: every shot and key comes back exactly.
  auto project = folder / L"Filmmaker test.rmc";
  saveTo(project);
  auto before = doc.text();
  NativeDocument reopened;
  reopened.open(project);
  require(reopened.text() == before, "the saved project differs from the document");
  doc = std::move(reopened);
  fmRefresh();
  require(shots(0).size() == 1 && shotKeys(0, "shot_1").size() == 3 && shotKeys(0, "shot_1", "a0")[0].v[4] == 1,
          "the reopened project lost its shot or keys");
  require(validateDocument(), "the engine refused a map with Filmmaker shots");
  // The film list: 90 frames, each with the person's pose and the camera with its lens.
  auto jobs = writeShotLists(0, {});
  auto pictures = folder / L"Filmmaker test film";
  require(jobs.size() == 1 && jobs[0].film.list == pictures / L"shot_1.film.txt" && jobs[0].film.folder == pictures / L"shot_1",
          "the shot's film list is not beside the project");
  std::vector<std::string> lines;
  {
    std::ifstream list(jobs[0].film.list);
    for (std::string l; std::getline(list, l);)
      lines.push_back(l);
  }
  int frames = 0, poses = 0;
  std::string first, sixty;
  for (auto &l : lines) {
    if (l.starts_with("f shot_1_")) {
      ++frames;
      if (frames == 1)
        first = l;
      if (frames == 61)
        sixty = l;
    }
    if (l.starts_with("pose a0 "))
      ++poses;
  }
  require(!lines.empty() && lines[0].starts_with("custom 0 ") && lines[0].ends_with(utf8(project.wstring())),
          "the list does not stage the project's map");
  require(frames == 90 && poses == 90, "the list has " + std::to_string(frames) + " frames and " + std::to_string(poses) + " poses, not 90");
  require(first == "f shot_1_0000 2 4.000 10.000 0.180 90.000 -12.217 60.000", "frame 0 is not the first key: " + first);
  require(sixty.starts_with("f shot_1_0060 2 8.000 10.000 0.380 120.000 0.000 40.000"), "frame 60 is not the second key: " + sixty);
  // A short take of it through the engine (two frames, renamed: the keys follow the name).
  require(applyShot("opening", .2f, 10) && shotKeys(0, "opening").size() == 3 && shotKeys(0, "shot_1").empty(),
          "renaming the shot did not carry its keys");
  jobs = writeShotLists(0, {"opening"});
  std::wstring said;
  int made = renderShotJobs(filmEngine(), jobs, pictures / L"Filmmaker test.mp4", said);
  require(made == 2 && fs::exists(pictures / L"opening" / L"opening_0000.bmp") && fs::exists(pictures / L"opening" / L"opening_0001.bmp"),
          "the engine did not film the take (" + utf8(said) + ")");
  {
    std::ifstream log(pictures / L"opening" / L"film-log.txt");
    std::string text((std::istreambuf_iterator<char>(log)), {});
    require(text.find("pose: unreadable") == std::string::npos, "the engine could not read a pose line");
  }
  bool movie = !ffmpegPath().empty();
  if (movie)
    require(fs::exists(jobs[0].movie) && fs::file_size(jobs[0].movie) > 1000, "ffmpeg is on PATH but the take has no MP4");
  // The window as drawn, for a look.
  {
    RECT was{};
    GetWindowRect(fm, &was);
    SetWindowPos(fm, nullptr, -6000, 0, 1180, 640, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(fm, SW_SHOWNOACTIVATE);
    fmLayout();
    RedrawWindow(fm, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    RECT r{};
    GetClientRect(fm, &r);
    HDC screen = GetDC(fm), dc = CreateCompatibleDC(screen);
    auto bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom);
    auto old = SelectObject(dc, bitmap);
    PrintWindow(fm, dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = r.right, info.bmiHeader.biHeight = -r.bottom;
    info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32;
    std::vector<char> bits(size_t(r.right) * size_t(r.bottom) * 4);
    SelectObject(dc, old);
    GetDIBits(dc, bitmap, 0, UINT(r.bottom), bits.data(), &info, DIB_RGB_COLORS);
    std::ofstream out(folder / L"filmmaker-window.bmp", std::ios::binary);
    BITMAPFILEHEADER head{};
    head.bfType = 0x4D42, head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    head.bfSize = DWORD(head.bfOffBits + bits.size());
    out.write(reinterpret_cast<const char *>(&head), sizeof head);
    out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof info.bmiHeader);
    out.write(bits.data(), std::streamsize(bits.size()));
    DeleteObject(bitmap), DeleteDC(dc), ReleaseDC(fm, screen);
    ShowWindow(fm, SW_HIDE);
    SetWindowPos(fm, nullptr, was.left, was.top, was.right - was.left, was.bottom - was.top, SWP_NOZORDER | SWP_NOACTIVATE);
  }
  closeFilmmaker();
  require(fm == nullptr, "the window did not close");
  path = exportPath;
  passed.push_back(std::string("Raw Metal Filmmaker: + New shot, two camera keys (eased, lens 60 to 40) and a walking person's key through the "
                               "window's boxes and track list; ruler scrub, key drag retimes, Undo; save/reopen exact; engine-valid; "
                               "90-frame film list with poses and lens; rename carries keys; the engine filmed a 2-frame take") +
                   (movie ? " and ffmpeg made its MP4" : " (no ffmpeg on PATH)"));
}
