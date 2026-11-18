  // ---- Staying up, and coming back (stage d of the 2026-11-17 artist pass).
  // The last things done are kept in a small ring; a crash writes them, the
  // exception and where it happened to custom maps/Autosave/last-crash.txt and
  // saves the open map beside the recovery copy. The next start brings the work
  // back (any map's recovery, not only a new map's) and says what happened.
  // --soak-test [steps] [seed] drives random valid edits through the real tools
  // and checks the document after every one (NativeDocument::verify, undo/redo,
  // save/reopen, the engine's own validation every so often).
  static inline std::array<std::string, 24> recentActions{};
  static inline std::atomic<unsigned> recentCount{0};
  static inline Editor *crashEditor = nullptr;
  static inline wchar_t crashLogPath[MAX_PATH]{}, crashSavePath[MAX_PATH]{}, crashSourcePath[MAX_PATH]{};
  static inline char crashSource[MAX_PATH * 3]{}; // the map's own file, UTF-8, for the .source beside the save
  static void noteAction(std::string what) {
    if (what.size() > 160) what.resize(160);
    const unsigned n = recentCount.fetch_add(1);
    recentActions[n % recentActions.size()] = std::to_string(GetTickCount64()) + " " + what;
  }
  static std::string recentText() {
    std::string out;
    const unsigned n = recentCount.load();
    for (unsigned i = n > recentActions.size() ? n - unsigned(recentActions.size()) : 0; i < n; ++i)
      out += "  " + recentActions[i % recentActions.size()] + "\r\n";
    return out;
  }
  static void writeRaw(const wchar_t *file, const std::string &text) {
    HANDLE h = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(h, text.data(), DWORD(text.size()), &wrote, nullptr);
    CloseHandle(h);
  }
  static void emergencySaveImpl() {
    if (crashEditor && crashEditor->dirty && crashSavePath[0]) {
      writeRaw(crashSavePath, crashEditor->doc.text());
      if (crashSourcePath[0]) writeRaw(crashSourcePath, crashSource);
    }
  }
  static void emergencySave() {
    __try { emergencySaveImpl(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  static void writeCrashLog(const char *what, unsigned code, const void *address) {
    if (!crashLogPath[0]) return;
    char head[512];
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    SYSTEMTIME t{};
    GetLocalTime(&t);
    std::snprintf(head, sizeof head, "The Campaign Builder stopped: %s\r\n%04d-%02d-%02d %02d:%02d:%02d, code 0x%08X at RawMetalEditor.exe+0x%llX\r\n",
                  what, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, code,
                  static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(address) - base));
    std::string text = head;
    text += "The last things done, oldest first:\r\n" + recentText();
    text += crashEditor && crashEditor->dirty ? "The map was saved beside its recovery copy (.crash.rmc); the next start brings it back.\r\n"
                                              : "Nothing was unsaved.\r\n";
    writeRaw(crashLogPath, text);
  }
  static LONG WINAPI crashFilter(EXCEPTION_POINTERS *p) {
    static std::atomic<bool> once{false};
    if (!once.exchange(true)) {
      emergencySave();
      __try {
        writeCrashLog("a fault", p && p->ExceptionRecord ? unsigned(p->ExceptionRecord->ExceptionCode) : 0u, p && p->ExceptionRecord ? p->ExceptionRecord->ExceptionAddress : nullptr);
      } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return EXCEPTION_CONTINUE_SEARCH; // Windows still writes its dump
  }
  static void crashTerminate() {
    emergencySave();
    writeCrashLog("an error nothing caught", 0xE06D7363u, _ReturnAddress());
    std::abort();
  }
  // Called once the window is up (not in the test modes, which report their own failures).
  void armCrashLog() {
    crashEditor = this;
    auto folder = root / L"custom maps" / L"Autosave";
    std::error_code ec;
    fs::create_directories(folder, ec);
    wcsncpy_s(crashLogPath, (folder / L"last-crash.txt").c_str(), _TRUNCATE);
    refreshCrashSavePath();
    SetUnhandledExceptionFilter(crashFilter);
    std::set_terminate(crashTerminate);
  }
  void refreshCrashSavePath() {
    if (crashEditor != this) return;
    auto file = autosavePath();
    file.replace_extension(L".crash.rmc");
    wcsncpy_s(crashSavePath, file.c_str(), _TRUNCATE);
    wcsncpy_s(crashSourcePath, (file.wstring() + L".source").c_str(), _TRUNCATE);
    strncpy_s(crashSource, utf8(path.wstring()).c_str(), _TRUNCATE);
  }
  // A recovery copy names the map it came from (path.rmc.source), so it can come back to it.
  static fs::path recoverySource(const fs::path &recovery) {
    std::ifstream in(recovery.wstring() + L".source", std::ios::binary);
    std::string line;
    std::getline(in, line);
    return line.empty() ? fs::path() : fs::path(wide(line));
  }
  // The newest recovery copy (or crash save) in the Autosave folder.
  fs::path newestRecovery() const {
    std::error_code ec;
    fs::path best;
    fs::file_time_type when{};
    for (auto it = fs::directory_iterator(root / L"custom maps" / L"Autosave", ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
      const auto &p = it->path();
      if (p.extension() != L".rmc") continue;
      auto t = fs::last_write_time(p, ec);
      if (!ec && (best.empty() || t > when)) best = p, when = t;
    }
    return best;
  }
  // At the start (no file asked for): the newest work left unsaved comes back, on its own
  // map's path when it had one; a crash is said plainly. Returns true when something came back.
  bool startupRecovery() {
    std::error_code ec;
    auto crash = root / L"custom maps" / L"Autosave" / L"last-crash.txt";
    const bool crashed = fs::exists(crash, ec);
    auto rescued = newestRecovery();
    if (rescued.empty()) {
      if (crashed) fs::rename(crash, crash.parent_path() / L"crash-seen.txt", ec);
      return false;
    }
    auto source = recoverySource(rescued);
    if (!source.empty() && fs::exists(source, ec) && fs::last_write_time(source, ec) >= fs::last_write_time(rescued, ec)) {
      // Saved since: the copy is old news.
      fs::remove(rescued, ec), fs::remove(rescued.wstring() + L".source", ec);
      return false;
    }
    NativeDocument saved;
    try { saved.open(rescued); } catch (...) { return false; }
    NativeDocument original;
    if (!source.empty() && fs::exists(source, ec)) {
      try { original.open(source); path = source; } catch (...) {}
    }
    restoredFrom = original.lines.empty() ? doc.text() : original.text();
    if (!original.lines.empty()) doc = std::move(original);
    if (saved.text() == doc.text()) { restoredFrom.clear(); fs::remove(rescued, ec); return false; }
    doc = std::move(saved);
    dirty = true;
    recoveredFile = rescued;
    if (crashed) {
      crashNotice = true;
      fs::rename(crash, crash.parent_path() / L"crash-seen.txt", ec);
    }
    return true;
  }
  fs::path recoveredFile;
  bool crashNotice = false;
  // --crash-test / --recovery-test: the Builder stopped with unsaved work, then started again.
  bool crashDrill = false;
  int drillStage = 0;
  int crashDrillRun() {
    auto folder = root / L"custom maps" / L"Autosave";
    auto mapFile = root / L".build" / L"crash-drill.rmc";
    std::ofstream report(root / L"native-editor-recovery-test.txt", std::ios::app);
    std::error_code ec;
    if (drillStage == 1) {
      fs::remove(folder / L"last-crash.txt", ec), fs::remove(folder / L"crash-seen.txt", ec);
      NativeDocument blank;
      blank.newCampaign("Crash drill");
      blank.addRoom("Hall");
      blank.save(mapFile);
      adoptDocument(std::move(blank), mapFile, false);
      autoDressingEnabled = false;
      tool = 1;
      gesture(4.5f, 4.5f, 9.5f, 9.5f); // unsaved work: a room
      SetWindowTextW(window, L"crash drill");
      doc.add({"# crash drill marker"});
      dirty = true, ++changeCount;
      armCrashLog();
      noteAction("crash drill: about to stop");
      SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
      volatile int *nothing = nullptr;
      *nothing = 1; // the fault
      return 2;
    }
    bool ok = true;
    auto check = [&](bool good, const std::string &what) { ok = ok && good; report << (good ? "PASS: " : "FAIL: ") << what << '\n'; };
    const bool back = startupRecovery();
    check(back && doc.text().find("# crash drill marker") != std::string::npos, "the next start brings back the work the crash left unsaved");
    check(fs::weakly_canonical(path, ec) == fs::weakly_canonical(mapFile, ec), "it comes back on its own map's path, so Save goes there (" + utf8(path.wstring()) + ")");
    check(crashNotice && fs::exists(folder / L"crash-seen.txt", ec), "the start says the Builder stopped, and keeps what happened (custom maps/Autosave/crash-seen.txt)");
    std::ifstream log(folder / L"crash-seen.txt");
    std::string text((std::istreambuf_iterator<char>(log)), {});
    check(text.find("0xC0000005") != std::string::npos && text.find("crash drill: about to stop") != std::string::npos && text.find("gesture Room") != std::string::npos,
          "the crash log names the fault and the last things done (the Room drag, the drill)");
    // Start fresh returns to the map as saved; the copy is gone.
    startFresh();
    check(doc.text().find("# crash drill marker") == std::string::npos && !fs::exists(recoveredFile, ec), "Start fresh goes back to the saved map and drops the copy");
    fs::remove(recoveredFile.wstring() + L".source", ec);
    report << (ok ? "PASS" : "FAIL") << ": recovery after a crash\n";
    return ok ? 0 : 1;
  }
  // Opening a map that has newer unsaved work: the work comes back (Start fresh returns to the file).
  void recoverOnOpen() {
    std::error_code ec;
    auto rescued = autosavePath();
    for (auto candidate : {rescued, fs::path(rescued).replace_extension(L".crash.rmc")}) {
      if (!fs::exists(candidate, ec) || path.empty() || fs::last_write_time(candidate, ec) <= fs::last_write_time(path, ec)) continue;
      auto source = recoverySource(candidate);
      if (!source.empty() && fs::weakly_canonical(source, ec) != fs::weakly_canonical(path, ec)) continue;
      NativeDocument saved;
      try { saved.open(candidate); } catch (...) { continue; }
      if (saved.text() == doc.text()) continue;
      restoredFrom = doc.text();
      doc = std::move(saved);
      dirty = true;
      restoredAt = changeCount;
      publish(false, false), showTitle(), refresh(), relayout();
      ShowWindow(GetDlgItem(window, StartFresh), SW_SHOW);
      message(L"Brought back unsaved work on this map from last time. Start fresh to open it as it was saved.");
      return;
    }
  }

  // ---- The pick follows its record: the line it was, wherever it went; gone, nothing is picked.
  // (The soak found Wall, Room, Erase and Floor leaving the pick past the end of the map, or
  // on another record, after they took lines out: the inspector then wrote to the wrong line.)
  std::string pickedText;
  bool compactTop = false; // the top bar's short labels under about 1500 px (layout)
  void repairSelection() {
    std::erase_if(selectedRecords, [&](int i) { return i < 0 || size_t(i) >= doc.lines.size(); });
    if (record < 0) return;
    if (size_t(record) < doc.lines.size() && (pickedText.empty() || doc.lines[size_t(record)] == pickedText)) return;
    if (!pickedText.empty()) {
      auto found = std::find(doc.lines.begin(), doc.lines.end(), pickedText);
      if (found != doc.lines.end()) {
        const int now = int(found - doc.lines.begin());
        for (auto &s : selectedRecords) if (s == record) s = now;
        record = now;
        return;
      }
      // Edited where it stands (the inspector, a drag): the same kind of record on the same map.
      if (size_t(record) < doc.lines.size()) {
        auto was = NativeDocument::fields(pickedText), is = NativeDocument::fields(doc.lines[size_t(record)]);
        if (was.size() > 1 && is.size() > 1 && was[0] == is[0] && was[1] == is[1]) return;
      }
    }
    std::erase(selectedRecords, record);
    record = -1;
  }
  // ---- The soak: random valid edits, checked after every one.
  struct SoakResult {
    int steps = 0, failures = 0, validated = 0;
    std::map<std::string, int> done;
    std::vector<std::string> problems;
  };
  void soakInvariants(const char *after) {
    auto require = [&](bool ok, const std::string &why) { if (!ok) throw std::runtime_error(std::string("after ") + after + ": " + why); };
    require(!doc.lines.empty() && doc.lines.front().size() < 100000, "the document is empty");
    require(record < int(doc.lines.size()), "the picked record is past the end of the map");
    for (auto r : selectedRecords) require(r >= 0 && r < int(doc.lines.size()), "a selected record is past the end of the map");
    std::set<int> mapIds;
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      require(f.size() > 11, "a MAP record lost fields: " + doc.lines[i]);
      mapIds.insert(int(number(f[1], -1)));
    }
    require(mapIds.count(map) == 1, "the map being edited is gone");
    std::map<std::pair<int, int>, int> rows;
    for (auto i : doc.records("ROW")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      require(f.size() == 5 && f[4].size() == 24, "a ROW is not 24 tiles: " + doc.lines[i]);
      ++rows[{int(number(f[1], -1)), int(number(f[2], -1))}];
    }
    for (auto &[key, n] : rows) require(n == 24, "map " + std::to_string(key.first) + " floor " + std::to_string(key.second) + " has " + std::to_string(n) + " rows");
    for (auto &a : floorLayers()) require(rows.count({map, a.id}) == 1, "floor " + std::to_string(a.id) + " has no rows");
    require(layer == 0 || rows.count({map, layer}) == 1, "the floor being edited is gone");
  }
  SoakResult soak(int steps, unsigned seed, const fs::path &folder) {
    SoakResult out;
    std::mt19937 rng(seed);
    auto rnd = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    auto pick = [&](int n) { return int(rng() % unsigned(std::max(1, n))); };
    NativeDocument blank;
    blank.newCampaign("Soak");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false, autoDetail = false;
    joinMapPassage = true;
    addAdjacentMap(3);
    SendMessageW(window, WM_COMMAND, Outdoors, 0);
    std::string lastGood = doc.text();
    std::atomic<ULONGLONG> stepStarted{GetTickCount64()};
    std::atomic<bool> running{true};
    std::string current;
    // A step that takes more than two minutes is a hang: say which, and stop.
    std::thread watchdog([&] {
      while (running) {
        Sleep(500);
        if (running && GetTickCount64() - stepStarted > 120000) {
          std::ofstream hang(folder / (L"soak-hang-" + std::to_wstring(seed) + L".txt"));
          hang << "HANG in step " << out.steps << ": " << current << "\n" << recentText();
          for (auto &p : out.problems) hang << "PROBLEM: " << p << "\n";
          hang.close();
          TerminateProcess(GetCurrentProcess(), 3);
        }
      }
    });
    static const char *names[] = {"room", "floor brush", "floor box", "wall", "erase", "flooring", "stairs", "lamp", "furniture", "paint 3D",
                                  "select and delete", "undo/redo", "view switch", "floor layer", "map switch", "terrain", "footing", "raise",
                                  "save and reopen", "balcony", "plan paint"};
    for (int step = 0; step < steps; ++step) {
      const int action = pick(int(std::size(names)));
      current = names[action];
      stepStarted = GetTickCount64();
      noteAction(std::string("soak ") + current);
      const float x0 = rnd(1, 20), y0 = rnd(1, 20), x1 = x0 + rnd(1, 6), y1 = y0 + rnd(1, 6);
      try {
        switch (action) {
        case 0: tool = 1, gesture(x0, y0, std::min(x1, 22.5f), std::min(y1, 22.5f)); break;
        case 1: tool = 2, brush = 0, floorPaintMaterial = 50 + pick(2), floorMaterialExplicit = true; gesture(x0, y0, x0, y0); break;
        case 2: tool = 2, brush = 1, floorPaintMaterial = 42, floorMaterialExplicit = true; gesture(x0, y0, std::min(x1, 23.f), std::min(y1, 23.f)); break;
        case 3: tool = 3, brush = 1, gesture(x0, y0, std::min(x1, 23.f), y0); break;
        case 4: tool = 4, brush = 1, gesture(x0, y0, std::min(x1, 23.f), std::min(y1, 23.f)); break;
        case 5: flooringLook = pick(int(std::size(floorLooks))), flooringTrim = pick(2), flooringLevel = pick(2); tool = FlooringTool;
          gesture(x0, y0, std::min(x1, 23.f), std::min(y1, 23.f)); break;
        case 6: tool = 12, gesture(x0, y0, std::min(x0 + 1.2f, 23.f), std::min(y0 + 3.5f, 23.f)); break;
        case 7: tool = 6, gesture(x0, y0, x0, y0); break;
        case 8: tool = 8, gesture(x0, y0, x0, y0); break;
        case 9: {
          SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
          const int m = std::array<int, 6>{50, 51, 31, 33, 23, 42}[size_t(pick(6))];
          choosePaint({"mat-" + std::to_string(m), materialName(m), m, 0});
          remember();
          paintRay(x0, y0, groundAt(x0, y0) + 2.f, rnd(-1, 1), rnd(-1, 1), -1.f);
          publish();
          break;
        }
        case 10: {
          if (doc.lines.size() > 12) {
            auto all = doc.records("STRUCT", map);
            if (!all.empty()) {
              tool = 0;
              selectRecord(int(all[size_t(pick(int(all.size())))]));
              removeSelected();
            }
          }
          break;
        }
        case 11: {
          const auto before = doc.text();
          const int n = 1 + pick(3);
          int undone = 0;
          for (int i = 0; i < n && !history.empty(); ++i) SendMessageW(window, WM_COMMAND, Undo, 0), ++undone;
          for (int i = 0; i < undone; ++i) SendMessageW(window, WM_COMMAND, Redo, 0);
          if (doc.text() != before) throw std::runtime_error("undo then redo did not come back to the same map");
          break;
        }
        case 12: SendMessageW(window, WM_COMMAND, std::array<int, 3>{Dual, PlanOnly, ViewOnly}[size_t(pick(3))], 0); break;
        case 13: {
          auto floors = floorLayers();
          if (floors.size() < 3 && pick(3) == 0) createFloorLayer(), refresh();
          floors = floorLayers();
          layer = floors[size_t(pick(int(floors.size())))].id;
          refresh();
          break;
        }
        case 14: {
          const int target = pick(2);
          SendMessageW(mapList, LB_SETCURSEL, target, 0);
          SendMessageW(window, WM_COMMAND, MAKEWPARAM(MapList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(mapList));
          break;
        }
        case 15: {
          if (!onTerrain() || layer != 0) break;
          tool = TerrainTool;
          SendMessageW(terrainModeList, CB_SETCURSEL, pick(4), 0);
          remember();
          beginOutdoorStroke(x0, y0, 'T');
          for (int i = 0; i < 5; ++i) terrainDab(x0 + i * .4f, y0 + i * .3f);
          endOutdoorStroke();
          publish();
          break;
        }
        case 16: case 17: {
          size_t at = 0;
          for (auto i : doc.records("STRUCT", map))
            if (footingOf(doc.lines[i])) at = i;
          if (!at) break;
          tool = 0;
          selectRecord(int(at));
          relayout();
          if (action == 16) {
            SendMessageW(footingModeList, CB_SETCURSEL, pick(5), 0);
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(FootingModeList, CBN_SELCHANGE), reinterpret_cast<LPARAM>(footingModeList));
          } else {
            footingAdvanced = true;
            relayout();
            SetWindowTextW(footingRaiseBox, std::to_wstring(pick(4) * .3f).c_str());
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(FootingRaiseBox, EN_KILLFOCUS), reinterpret_cast<LPARAM>(footingRaiseBox));
          }
          break;
        }
        case 18: {
          auto file = folder / (L"soak-" + std::to_wstring(seed) + L".rmc");
          const auto text = doc.text();
          saveTo(file);
          NativeDocument back;
          back.open(file);
          if (back.text() != text) throw std::runtime_error("save and reopen changed the map");
          const int keepMap = map, keepLayer = layer;
          adoptDocument(std::move(back), file, false);
          map = keepMap, layer = keepLayer;
          refresh();
          break;
        }
        case 19: {
          auto floors = floorLayers();
          if (floors.size() < 2) break;
          layer = floors.back().id;
          refresh();
          tool = 5, brush = 1;
          gesture(x0, y0, std::min(x1, 23.f), std::min(y0 + 1, 23.f));
          break;
        }
        case 20: {
          SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
          choosePaint({"mat-31", materialName(31), 31, 0});
          remember();
          pickZ = -1000.f, pickSurface = 0;
          gesture(x0, y0, x0, y0);
          break;
        }
        }
        soakInvariants(names[action]);
        if (step % 40 == 39) {
          ++out.validated;
          if (!validateDocument()) throw std::runtime_error("the engine refused the map: " + utf8(notice));
        }
        lastGood = doc.text();
        ++out.done[names[action]];
      } catch (const std::exception &error) {
        ++out.failures;
        std::string why = std::string(names[action]) + " (step " + std::to_string(step) + ", map " + std::to_string(map) + ", floor " + std::to_string(layer) + "): " + error.what();
        out.problems.push_back(why);
        auto bad = folder / (L"soak-" + std::to_wstring(seed) + L"-failure-" + std::to_wstring(out.failures) + L".rmc");
        doc.save(bad);
        // Carry on from the last good map.
        doc.loadText(lastGood);
        record = -1, selectedRecords.clear();
        objectDrag = drag = brushing = false;
        auto floors = floorLayers();
        if (std::none_of(floors.begin(), floors.end(), [&](auto &a) { return a.id == layer; })) layer = 0;
        publish(false, false), refresh();
      }
      out.steps = step + 1;
    }
    running = false;
    watchdog.join();
    return out;
  }
