  // ---- Multiplayer (docs/Systems/multiplayer.md) ----
  // The Multiplayer page (the third tab on the right) sets the campaign's MULTIPLAYER record in
  // plain words: which modes are on, the map a deathmatch plays on, how many players, the frags
  // and the minutes that end a match, and whether co-op players can hurt each other. Each change
  // is one Ctrl+Z. The Spawn tool places SPAWN records, where a match player starts. Its ghost is
  // the record the click writes (NativeGhost.inc), drawn as a figure with an arrow the way it
  // faces, on the plan and in 3D; R turns it, a click sets it down (one Ctrl+Z), and a placed spawn
  // is picked, nudged, turned, given a team, copied and deleted like anything else. Both records
  // are named fields only; NativeDocument reads a SPAWN as the positional fields
  // map|x|y|z|turn|team (turn is the record's yaw=, in degrees: 0 east, 90 south).
  // --multiplayer-test (NativeMultiplayerRegression.inc); --editing-test runs it too.
  static constexpr int SpawnTeams = 5;      // 0 anyone, then teams 1 to 4 (the engine's range)
  static constexpr float SpawnTall = 1.75f; // the figure: a person's height
  int spawnTurn = 0;                        // eighth turns the next spawn faces (R)
  int spawnTeam = 0;                        // the team the next spawn is for
  bool multiplayerDock = false;             // the right column shows the Multiplayer page
  HWND spawnTeamList = nullptr, mpMapList = nullptr, mpPlayersList = nullptr, mpFragList = nullptr, mpTimeList = nullptr;
  struct MultiplayerChoice {
    bool deathmatch = true, coop = true, friendlyFire = false;
    int deathmatchMap = -1, maxPlayers = 8, fragLimit = 20; // -1: the start map; 0 frags: no limit
    float timeLimit = 10;                                   // minutes; 0: no limit
    bool operator==(const MultiplayerChoice &) const = default;
  };

  // ---- The MULTIPLAYER record ----
  // Its line, or -1 (a campaign without one plays every mode at the defaults).
  int multiplayerLine() const {
    for (size_t i = 0; i < doc.lines.size(); ++i) {
      std::string_view kind;
      if (NativeDocument::field(doc.lines[i], 0, kind) && kind == "MULTIPLAYER")
        return int(i);
    }
    return -1;
  }
  MultiplayerChoice multiplayerChoice() const {
    MultiplayerChoice c;
    const int at = multiplayerLine();
    if (at < 0)
      return c;
    for (const auto &[key, value] : NativeDocument::named(doc.lines[size_t(at)])) {
      const float v = number(value, -9999);
      if (v == -9999)
        continue;
      if (key == "deathmatch") c.deathmatch = v != 0;
      else if (key == "coop") c.coop = v != 0;
      else if (key == "dmmap") c.deathmatchMap = int(v);
      else if (key == "maxplayers") c.maxPlayers = int(v);
      else if (key == "fraglimit") c.fragLimit = int(v);
      else if (key == "timelimit") c.timeLimit = v;
      else if (key == "friendlyfire") c.friendlyFire = v != 0;
    }
    return c;
  }
  // The record as the Builder writes it: every field, in the order of the guide.
  static std::string multiplayerText(const MultiplayerChoice &c) {
    return "MULTIPLAYER|deathmatch=" + std::string(c.deathmatch ? "1" : "0") + "|coop=" + (c.coop ? "1" : "0") +
           "|dmmap=" + std::to_string(c.deathmatchMap) + "|maxplayers=" + std::to_string(c.maxPlayers) +
           "|fraglimit=" + std::to_string(c.fragLimit) + "|timelimit=" + nativeNumber(c.timeLimit) +
           "|friendlyfire=" + (c.friendlyFire ? "1" : "0");
  }
  // Writes the record (one Ctrl+Z): in place, or under CAMPAIGN (and its STORM) where the game's
  // exporter puts it too. Named fields need FORMAT|2 at the top of the file.
  void storeMultiplayer(const MultiplayerChoice &c, const std::wstring &said) {
    const int at = multiplayerLine();
    if (at < 0 && c == MultiplayerChoice{}) {
      message(said);
      return;
    }
    const std::string text = multiplayerText(c);
    const std::string cr = doc.crlf ? "\r" : "";
    if (at >= 0 && doc.lines[size_t(at)] == text + cr)
      return;
    remember();
    if (at >= 0) {
      doc.lines[size_t(at)] = text + (doc.lines[size_t(at)].ends_with('\r') ? "\r" : "");
      doc.touch();
    } else {
      doc.ensureFormat2();
      size_t under = doc.lines.size();
      for (size_t i = 0; i < doc.lines.size(); ++i) {
        std::string_view kind;
        if (NativeDocument::field(doc.lines[i], 0, kind) && kind == "CAMPAIGN") {
          under = i + 1;
          if (under < doc.lines.size() && NativeDocument::field(doc.lines[under], 0, kind) && kind == "STORM")
            ++under;
          break;
        }
      }
      if (under >= doc.lines.size())
        doc.add({text});
      else {
        doc.lines.insert(doc.lines.begin() + std::ptrdiff_t(under), text + cr);
        doc.touch();
      }
      repairSelection();
    }
    publish();
    syncMultiplayerPage();
    relayout();
    message(said + L" Ctrl+Z takes it back.");
  }
  // The map a deathmatch plays on: the record's, or the campaign's start map.
  int deathmatchMapIndex() const {
    const int chosen = multiplayerChoice().deathmatchMap;
    if (chosen >= 0)
      return chosen;
    for (auto i : doc.records("CAMPAIGN")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2)
        return int(number(f[2], 0));
    }
    return 0;
  }
  std::string mapNameOf(int index) const {
    for (auto i : doc.records("MAP", index)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2)
        return decoded(f[2]);
    }
    return "map " + std::to_string(index);
  }
  size_t spawnCount(int on = -1) const { return doc.records("SPAWN", on).size(); }
  // Fewer than 4 spawns on the deathmatch map: the game finds the rest on open floor itself.
  std::wstring multiplayerWarning() const {
    if (!multiplayerChoice().deathmatch)
      return {};
    const size_t n = spawnCount(deathmatchMapIndex());
    if (n >= 4)
      return {};
    return L"Deathmatch has " + std::to_wstring(n) + (n == 1 ? L" spawn" : L" spawns") +
           L" on its map. With fewer than 4 the game finds spots on open floor itself.";
  }

  // A deathmatch starts with fists and nothing else: the guns lie on its map. None there (a MAC-11, handgun or
  // shotgun pickup, kinds 4 to 6) and nobody can fight.
  std::wstring gunWarning() const {
    if (!multiplayerChoice().deathmatch)
      return {};
    for (auto i : doc.records("PICKUP", deathmatchMapIndex())) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2 && number(f[2], 0) >= 4)
        return {};
    }
    return L"Deathmatch starts with fists only. Place a Handgun, Shotgun or MAC-11 with the Health and ammo tool.";
  }

  // ---- The page ----
  template <class Choices> void makeMultiplayerControls(Choices &&choices) {
    spawnTeamList = choices(SpawnTeamList, {L"Anyone", L"Team 1", L"Team 2", L"Team 3", L"Team 4"});
    mpMapList = choices(MpMapList, {});
    mpPlayersList = choices(MpPlayersList, {});
    mpFragList = choices(MpFragList, {});
    mpTimeList = choices(MpTimeList, {});
    for (auto [id, text] : {std::pair{int(MultiplayerTab), L"Multiplayer"}, std::pair{int(MpDeathmatch), L"Deathmatch: on"},
                            std::pair{int(MpCoop), L"Co-op: on"}, std::pair{int(MpFriendlyFire), L"Friendly fire: off"},
                            std::pair{int(MpPlaceSpawns), L"Place spawns"}})
      button(text, id);
  }
  std::vector<HWND> multiplayerControls() const {
    std::vector<HWND> out{mpMapList, mpPlayersList, mpFragList, mpTimeList};
    if (window)
      for (int id : {MpDeathmatch, MpCoop, MpFriendlyFire, MpPlaceSpawns})
        out.push_back(GetDlgItem(window, id));
    return out;
  }
  // A list of numbers: the item whose data is `value`, added (as set by hand) when the list lacks it.
  void chooseValue(HWND list, int value, const std::wstring &handSet) {
    if (!list)
      return;
    for (LRESULT i = 0, n = SendMessageW(list, CB_GETCOUNT, 0, 0); i < n; ++i)
      if (SendMessageW(list, CB_GETITEMDATA, i, 0) == value) {
        SendMessageW(list, CB_SETCURSEL, i, 0);
        return;
      }
    auto row = SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(handSet.c_str()));
    SendMessageW(list, CB_SETITEMDATA, row, value);
    SendMessageW(list, CB_SETCURSEL, row, 0);
  }
  static void fillValues(HWND list, const std::vector<std::pair<int, std::wstring>> &items) {
    SendMessageW(list, CB_RESETCONTENT, 0, 0);
    for (auto &[value, text] : items) {
      auto row = SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
      SendMessageW(list, CB_SETITEMDATA, row, value);
    }
  }
  // The page's boxes from the document (after a change, Undo, or another file).
  void syncMultiplayerPage() {
    if (!mpMapList)
      return;
    const auto c = multiplayerChoice();
    std::vector<std::pair<int, std::wstring>> maps{{-1, L"The start map (" + wide(mapNameOf(deathmatchMapIndexFor(-1))) + L")"}};
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2)
        maps.push_back({int(number(f[1], 0)), L"Map " + wide(f[1]) + L": " + wide(decoded(f[2]))});
    }
    fillValues(mpMapList, maps);
    chooseValue(mpMapList, c.deathmatchMap, L"Map " + std::to_wstring(c.deathmatchMap) + L" (not in this campaign)");
    std::vector<std::pair<int, std::wstring>> players;
    for (int n = 2; n <= 16; ++n)
      players.push_back({n, std::to_wstring(n) + L" players"});
    fillValues(mpPlayersList, players);
    chooseValue(mpPlayersList, c.maxPlayers, std::to_wstring(c.maxPlayers) + L" players");
    std::vector<std::pair<int, std::wstring>> frags{{0, L"No frag limit"}};
    for (int n : {5, 10, 15, 20, 25, 30, 40, 50, 75, 100})
      frags.push_back({n, L"First to " + std::to_wstring(n) + L" frags"});
    fillValues(mpFragList, frags);
    chooseValue(mpFragList, c.fragLimit, L"First to " + std::to_wstring(c.fragLimit) + L" frags");
    // Minutes are kept in tenths so a hand-written 7.5 shows as it is.
    std::vector<std::pair<int, std::wstring>> minutes{{0, L"No time limit"}};
    for (int n : {5, 10, 15, 20, 30, 45, 60})
      minutes.push_back({n * 10, std::to_wstring(n) + L" minutes"});
    fillValues(mpTimeList, minutes);
    chooseValue(mpTimeList, int(std::lround(c.timeLimit * 10)), wide(nativeNumber(c.timeLimit)) + L" minutes");
    if (window) {
      SetWindowTextW(GetDlgItem(window, MpDeathmatch), c.deathmatch ? L"Deathmatch: on" : L"Deathmatch: off");
      SetWindowTextW(GetDlgItem(window, MpCoop), c.coop ? L"Co-op: on" : L"Co-op: off");
      SetWindowTextW(GetDlgItem(window, MpFriendlyFire), c.friendlyFire ? L"Friendly fire: on" : L"Friendly fire: off");
      for (int id : {MpDeathmatch, MpCoop, MpFriendlyFire})
        InvalidateRect(GetDlgItem(window, id), nullptr, FALSE);
    }
    EnableWindow(mpMapList, c.deathmatch);
    EnableWindow(mpFragList, c.deathmatch);
  }
  int deathmatchMapIndexFor(int chosen) const {
    if (chosen >= 0)
      return chosen;
    for (auto i : doc.records("CAMPAIGN")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2)
        return int(number(f[2], 0));
    }
    return 0;
  }
  void layoutMultiplayerPage(int x, int y, int width, int height) {
    (void)height;
    auto caption = [&](int x0, int y0, int span, const std::wstring &text) { captions.push_back({RECT{x0, y0, x0 + span, y0 + 20}, text}); };
    syncMultiplayerPage();
    const auto c = multiplayerChoice();
    caption(x, y, width, L"⚑  Multiplayer");
    y += 26;
    caption(x, y, width, L"How this campaign plays with others.");
    y += 26;
    placeControl(GetDlgItem(window, MpDeathmatch), x, y, (width - 6) / 2, 32, TRUE);
    placeControl(GetDlgItem(window, MpCoop), x + (width + 6) / 2, y, width - (width + 6) / 2, 32, TRUE);
    y += 40;
    auto row = [&](HWND list, const wchar_t *text) {
      caption(x, y + 5, 96, text);
      placeControl(list, x + 100, y, width - 100, 300, TRUE);
      y += 34;
    };
    row(mpMapList, L"Map");
    row(mpPlayersList, L"Players");
    row(mpFragList, L"Win at");
    row(mpTimeList, L"Time limit");
    placeControl(GetDlgItem(window, MpFriendlyFire), x, y, width, 32, TRUE);
    y += 36;
    caption(x, y, width, L"Co-op players' shots hurt each other.");
    y += 28;
    const int dm = deathmatchMapIndex();
    caption(x, y, width, std::to_wstring(spawnCount(dm)) + L" spawns on that map, " + std::to_wstring(spawnCount()) + L" in all.");
    y += 24;
    if (auto warning = multiplayerWarning(); !warning.empty()) {
      // Two lines: the count, then what the game does about it.
      const auto split = warning.find(L". ");
      caption(x, y, width, L"⚠ " + warning.substr(0, split + 1));
      y += 20;
      caption(x, y, width, warning.substr(split + 2));
      y += 26;
    }
    if (auto warning = gunWarning(); !warning.empty()) {
      const auto split = warning.find(L". ");
      caption(x, y, width, L"⚠ " + warning.substr(0, split + 1));
      y += 20;
      caption(x, y, width, warning.substr(split + 2));
      y += 26;
    }
    placeControl(GetDlgItem(window, MpPlaceSpawns), x, y, width, 32, TRUE);
    y += 40;
    for (HWND h : multiplayerControls())
      if (h)
        ShowWindow(h, SW_SHOW);
    (void)c;
  }
  void multiplayerCommand(int id, int notification) {
    auto c = multiplayerChoice();
    auto picked = [&](HWND list) { return int(SendMessageW(list, CB_GETITEMDATA, SendMessageW(list, CB_GETCURSEL, 0, 0), 0)); };
    if (id == MultiplayerTab) {
      multiplayerDock = true, layersDock = false;
      syncMultiplayerPage();
      relayout();
      message(L"Multiplayer: which modes, the deathmatch map, players and limits. The Spawn tool places where players start.");
      return;
    }
    if (id == MpDeathmatch || id == MpCoop || id == MpFriendlyFire) {
      if (id == MpDeathmatch) c.deathmatch = !c.deathmatch;
      if (id == MpCoop) c.coop = !c.coop;
      if (id == MpFriendlyFire) c.friendlyFire = !c.friendlyFire;
      storeMultiplayer(c, id == MpDeathmatch ? (c.deathmatch ? L"Deathmatch is on." : L"Deathmatch is off.")
                          : id == MpCoop     ? (c.coop ? L"Co-op is on." : L"Co-op is off.")
                                             : (c.friendlyFire ? L"Friendly fire is on: co-op players can hurt each other." : L"Friendly fire is off."));
      return;
    }
    if (id == MpPlaceSpawns) {
      multiplayerDock = false;
      SendMessageW(window, WM_COMMAND, ToolBase + SpawnTool, 0);
      return;
    }
    if (notification != CBN_SELCHANGE)
      return;
    if (id == SpawnTeamList) {
      setSpawnTeam(listIndex(spawnTeamList));
      return;
    }
    if (id == MpMapList) {
      c.deathmatchMap = picked(mpMapList);
      storeMultiplayer(c, L"Deathmatch plays on " + wide(mapNameOf(deathmatchMapIndexFor(c.deathmatchMap))) + L".");
    } else if (id == MpPlayersList) {
      c.maxPlayers = std::clamp(picked(mpPlayersList), 2, 16);
      storeMultiplayer(c, L"At most " + std::to_wstring(c.maxPlayers) + L" players.");
    } else if (id == MpFragList) {
      c.fragLimit = std::clamp(picked(mpFragList), 0, 999);
      storeMultiplayer(c, c.fragLimit ? L"The first to " + std::to_wstring(c.fragLimit) + L" frags wins." : std::wstring(L"No frag limit."));
    } else if (id == MpTimeList) {
      c.timeLimit = float(std::clamp(picked(mpTimeList), 0, 1200)) / 10.f;
      storeMultiplayer(c, c.timeLimit > 0 ? L"A match lasts " + wide(nativeNumber(c.timeLimit)) + L" minutes." : std::wstring(L"No time limit."));
    }
  }

  // ---- Spawns ----
  bool spawnPicked() const {
    return record >= 0 && record < int(doc.lines.size()) && selectedRecords.size() <= 1 &&
           NativeDocument::fields(doc.lines[size_t(record)])[0] == "SPAWN";
  }
  // A spawn's box: a person's height on its floor (z is the floor they stand on, absolute).
  std::optional<creation::Box> spawnBox(const std::vector<std::string> &f) const {
    if (f.size() < 4 || f[0] != "SPAWN")
      return std::nullopt;
    const float z = f.size() > 4 ? number(f[4], 0) : 0.f;
    return creation::Box{number(f[2], 0), number(f[3], 0), .5f, .5f, z, z + SpawnTall, 0};
  }
  // The Spawn tool's click: on the floor under it (the land outdoors), facing the way R turned the
  // next one, for the team picked.
  template <class Set> void authorSpawn(Set &&set, float x, float y) {
    const float z = onTerrain() && layer == 0 ? groundAt(std::clamp(x, 0.f, 23.99f), std::clamp(y, 0.f, 23.99f)) : layerElevation();
    set("z", nativeNumber(std::round(z * 1000.f) / 1000.f));
    set("turn", std::to_string((spawnTurn & 7) * 45));
    set("team", std::to_string(std::clamp(spawnTeam, 0, SpawnTeams - 1)));
  }
  // R: the picked spawn an eighth round (Shift: back), or the next one.
  bool turnSpawn(float degrees) {
    if (spawnPicked()) {
      auto f = NativeDocument::fields(doc.lines[size_t(record)]);
      float a = std::fmod(number(f[5], 0) + degrees, 360.f);
      if (a < 0)
        a += 360;
      if (a > 359.95f)
        a = 0;
      f[5] = nativeNumber(std::round(a * 100.f) / 100.f);
      const int picked = record;
      remember();
      doc.replace(size_t(picked), f);
      publish();
      selectRecord(picked);
      if (tool == SpawnTool && std::fmod(a, 45.f) == 0)
        spawnTurn = int(a / 45.f) & 7;
      message(L"Spawn turned to face " + wide(f[5]) + L" degrees (0 east, 90 south). Ctrl+Z takes it back.");
      return true;
    }
    if (tool != SpawnTool)
      return false;
    spawnTurn = (spawnTurn + (degrees < 0 ? 7 : 1)) & 7;
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
    message(L"The next spawn faces " + std::to_wstring(spawnTurn * 45) + L" degrees (0 east, 90 south).");
    return true;
  }
  // The team box: the picked spawn's team (one Ctrl+Z), or the next spawn's.
  void setSpawnTeam(int team) {
    team = std::clamp(team, 0, SpawnTeams - 1);
    if (spawnPicked()) {
      auto f = NativeDocument::fields(doc.lines[size_t(record)]);
      if (f[6] == std::to_string(team))
        return;
      f[6] = std::to_string(team);
      const int picked = record;
      remember();
      doc.replace(size_t(picked), f);
      publish();
      selectRecord(picked);
      message(team ? L"This spawn is for team " + std::to_wstring(team) + L". Ctrl+Z takes it back." : std::wstring(L"Anyone can start at this spawn. Ctrl+Z takes it back."));
      return;
    }
    spawnTeam = team;
    if (tool == SpawnTool)
      refreshGhost();
    message(team ? L"The next spawn is for team " + std::to_wstring(team) + L"." : std::wstring(L"The next spawn is for anyone."));
  }
  // The tool's panel (and a picked spawn's): its team, how many there are, and the warning.
  template <class Row, class Caption> void spawnRows(Row &&row, Caption &&caption, int x, int &y) {
    const bool picked = spawnPicked();
    if (spawnTeamList) {
      SendMessageW(spawnTeamList, CB_SETCURSEL, picked ? std::clamp(int(number(NativeDocument::fields(doc.lines[size_t(record)])[6], 0)), 0, SpawnTeams - 1) : spawnTeam, 0);
      row(spawnTeamList, picked ? L"Team" : L"For team");
    }
    if (tool != SpawnTool)
      return;
    caption(x, y, 312, std::to_wstring(spawnCount(map)) + L" spawns on this map.");
    caption(x, y + 20, 312, L"Deathmatch plays on " + wide(mapNameOf(deathmatchMapIndex())) + L".");
    y += 44;
    if (auto warning = multiplayerWarning(); !warning.empty()) {
      const auto split = warning.find(L". ");
      caption(x, y, 312, L"⚠ " + warning.substr(0, split + 1));
      caption(x, y + 20, 312, warning.substr(split + 2));
      y += 44;
    }
  }
  // The figure, facing: legs, body, arms, a head, and an arrow along the floor the way it faces.
  static std::vector<std::array<float, 6>> spawnSegments(float x, float y, float z, float yawDegrees) {
    const float a = yawDegrees * 3.14159265f / 180.f, fx = std::cos(a), fy = std::sin(a), rx = -fy, ry = fx;
    std::vector<std::array<float, 6>> s;
    auto line = [&](float ax, float ay, float az, float bx, float by, float bz) { s.push_back({ax, ay, az, bx, by, bz}); };
    const float hip = z + .9f, neck = z + 1.42f, head = z + 1.6f;
    line(x + rx * .14f, y + ry * .14f, z, x, y, hip);
    line(x - rx * .14f, y - ry * .14f, z, x, y, hip);
    line(x, y, hip, x, y, neck);
    line(x + rx * .26f, y + ry * .26f, neck - .04f, x - rx * .26f, y - ry * .26f, neck - .04f);
    const float r = .13f;
    line(x + fx * r, y + fy * r, head, x + rx * r, y + ry * r, head + r);
    line(x + rx * r, y + ry * r, head + r, x - fx * r, y - fy * r, head);
    line(x - fx * r, y - fy * r, head, x - rx * r, y - ry * r, head - r);
    line(x - rx * r, y - ry * r, head - r, x + fx * r, y + fy * r, head);
    const float floor = z + .04f, tip = .95f, back = .7f;
    line(x, y, floor, x + fx * tip, y + fy * tip, floor);
    line(x + fx * tip, y + fy * tip, floor, x + fx * back + rx * .16f, y + fy * back + ry * .16f, floor);
    line(x + fx * tip, y + fy * tip, floor, x + fx * back - rx * .16f, y + fy * back - ry * .16f, floor);
    return s;
  }
  void spawnFigure(std::vector<retro::EditorGuide> &out, const std::vector<std::string> &f, int kind) const {
    if (f.size() < 6 || f[0] != "SPAWN")
      return;
    for (auto &q : spawnSegments(number(f[2], 0), number(f[3], 0), number(f[4], 0), number(f[5], 0)))
      out.push_back({q[0], q[1], q[2], q[3], q[4], q[5], kind});
  }
  // The 3D view: every spawn on this map as its small figure (the picked one in the picked colour).
  void spawnGuides(std::vector<retro::EditorGuide> &out) const {
    for (auto i : doc.records("SPAWN", map))
      if (visibleFloorRecord(i))
        spawnFigure(out, NativeDocument::fields(doc.lines[i]), int(i) == record ? 1 : 0);
  }
  // The plan: a head, shoulders across the way it faces, an arrow, and its team beside it.
  void drawSpawnFigure(HDC dc, int ox, int oy, float cell, const std::vector<std::string> &f, COLORREF colour, bool dashed) const {
    if (f.size() < 6)
      return;
    const float x = number(f[2], 0), y = number(f[3], 0), a = number(f[5], 0) * 3.14159265f / 180.f;
    const float fx = std::cos(a), fy = std::sin(a), rx = -fy, ry = fx;
    auto at = [&](float px, float py) { return POINT{int(ox + px * cell), int(oy + py * cell)}; };
    auto pen = dashed ? ui::dashedPen(2, colour) : CreatePen(PS_SOLID, 2, colour);
    auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    POINT s1 = at(x + rx * .3f, y + ry * .3f), s2 = at(x - rx * .3f, y - ry * .3f);
    MoveToEx(dc, s1.x, s1.y, nullptr);
    LineTo(dc, s2.x, s2.y);
    const int r = std::max(3, int(cell * .14f));
    POINT c = at(x, y);
    Ellipse(dc, c.x - r, c.y - r, c.x + r + 1, c.y + r + 1);
    POINT tip = at(x + fx * .85f, y + fy * .85f), l = at(x + fx * .6f + rx * .16f, y + fy * .6f + ry * .16f), rr = at(x + fx * .6f - rx * .16f, y + fy * .6f - ry * .16f);
    MoveToEx(dc, c.x, c.y, nullptr);
    LineTo(dc, tip.x, tip.y);
    MoveToEx(dc, l.x, l.y, nullptr);
    LineTo(dc, tip.x, tip.y);
    LineTo(dc, rr.x, rr.y);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    if (const int team = int(number(f[6], 0)); team > 0) {
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, colour);
      auto oldFont = SelectObject(dc, smallFont ? smallFont : font);
      const std::wstring label = L"T" + std::to_wstring(team);
      TextOutW(dc, c.x + r + 3, c.y - r - 12, label.c_str(), int(label.size()));
      SelectObject(dc, oldFont);
    }
  }
  static constexpr COLORREF SpawnColour = RGB(0xF2, 0x9A, 0x3E);
  void drawSpawnMarkers(HDC dc, int ox, int oy, float cell) const {
    for (auto i : doc.records("SPAWN", map))
      if (visibleFloorRecord(i))
        drawSpawnFigure(dc, ox, oy, cell, NativeDocument::fields(doc.lines[i]), SpawnColour, false);
  }
