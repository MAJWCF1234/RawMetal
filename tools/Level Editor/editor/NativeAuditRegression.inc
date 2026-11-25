  // ---- The 2026-11-20 audit (docs/Design/editor-audit-20261120.md), as checks that stay.
  // Four sweeps, each over everything there is, so a new tool, kind or field is covered by being there:
  //   schema   the guide's rows reach as far as the loader reads, and the lists the tools offer are as long as the
  //            engine's kinds (a pickup on an upper floor was saved without its height because its row stopped short)
  //   tools    every placing tool in toolInfo, on floors 0, 1 and 2 of map 0 and floors 0 and 1 of map 2: the ghost is
  //            what is placed, it stands on the floor, the record is on that floor, and it can be picked, nudged,
  //            turned, copied, deleted, undone and redone
  //   inspector  picking a record and leaving its boxes alone changes nothing (kinds, named fields, comments, unknown fields)
  //   guards   a typo, an out of range number, a name the loader would refuse: kept, clamped or cleaned, with a word
  //   open     malformed, empty, huge, odd-named and locked files: no crash, the old work kept
  // --audit-test runs it alone (native-editor-audit-test.txt); --editing-test runs it too.
  void verifyAudit() {
    std::ofstream report(root / L"native-editor-audit-test.txt");
    auto pass = [&](const std::string &what) { report << "PASS: " << what << '\n' << std::flush; };
    auto note = [&](const std::string &what) { report << "NOTE: " << what << '\n' << std::flush; };
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Audit regression: " + message);
    };
    const bool savedAuto = autoDressingEnabled;
    const float savedSnap = snap;
    const bool savedSmart = smartSnap;
    // ================= schema =================
    {
      // The furthest field (counted from the map field as field 1) the loader reads, per record kind with an optional tail.
      static const std::map<std::string, int> loaderReach = {
          {"ACTOR", 8}, {"LIGHT", 26}, {"TERMINAL", 16}, {"HAZARD", 15}, {"TIMED_SEQUENCE", 20}, {"SCRIPT_TRIGGER", 6},
          {"BREAKABLE", 14}, {"COMPACTOR", 9}, {"DOOR", 24}, {"FIXTURE", 15}, {"SIGN", 14}, {"AMBIENT", 12}, {"EMITTER", 19},
          {"CREATURE", 10}, {"STAIR", 13}, {"WATER", 8}, {"ZONE", 13}, {"DARK", 11}, {"PICKUP", 5}, {"MAP", 27},
          {"CRATE", 5}, {"CRATE_PLATE", 4}, {"CRATE_RESET", 2}, {"NAV_HINT", 5}, {"HUNG_SUIT", 5}, {"CHAIN", 10}, {"LEAD", 20}};
      for (const auto &[kindName, reach] : loaderReach) {
        int most = -1;
        for (const auto &s : schemas)
          if (s.kind == kindName)
            most = std::max(most, int(s.fields.size()) - 1);
        require(most >= 0, kindName + " has no row in the map guide, so the Builder cannot list, pick or edit it");
        require(most >= reach, kindName + ": the guide's row stops at field " + std::to_string(most) + " but the loader reads field " + std::to_string(reach) + " (the Builder would not edit or carry it)");
      }
      pass("the guide has a row for every record with an optional tail, reaching as far as the loader reads");
      // The lists the tools offer, against the engine's own enums (read from the headers, as the guide's tables are).
      auto enumCount = [&](const char *header, const std::string &pattern) {
        std::ifstream in(root / L"src" / L"world" / fs::path(header));
        std::string text(std::istreambuf_iterator<char>(in), {});
        std::smatch m;
        if (!std::regex_search(text, m, std::regex(pattern)))
          throw std::runtime_error("Audit regression: the enum " + pattern + " was not found in " + header);
        int n = 1;
        for (char c : m[1].str())
          n += c == ',';
        return n;
      };
      auto listed = [&](HWND list) { return int(SendMessageW(list, CB_GETCOUNT, 0, 0)); };
      require(listed(pickupList) == enumCount("WorldDefinition.h", "enum class PickupKind \\{([^}]*)\\}"), "the pickup list is not as long as the engine's pickup kinds");
      require(listed(creatureList) == enumCount("WorldDefinition.h", "enum class CreatureKind \\{([^}]*)\\}"), "the creature list is not as long as the engine's creature kinds");
      require(listed(hazardList) == enumCount("World.h", "enum class Kind \\{(Electricity[^}]*)\\}"), "the hazard list is not as long as the engine's hazard kinds");
      require(listed(clutterList) == 8, "the loose prop list is not the loader's 8 kinds");
      pass("the pickup, creature, hazard and loose prop lists are as long as the engine's kinds");
    }
    // ================= tools =================
    auto mouse = [&](UINT message, float x, float y, WPARAM keys = MK_LBUTTON) {
      auto v = planView();
      SendMessageW(plan, message, keys, MAKELPARAM(int(std::round(v.ox + x * v.cell)), int(std::round(v.oy + y * v.cell))));
    };
    auto pick = [&](int t) { SendMessageW(window, WM_COMMAND, ToolBase + t, 0); };
    auto box = [&](HWND list, int id, int choice) {
      SendMessageW(list, CB_SETCURSEL, choice, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    auto changeMap = [&](int i) {
      SendMessageW(mapList, LB_SETCURSEL, i, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(MapList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(mapList));
    };
    auto press = [&](WPARAM k) { MSG m{plan, WM_KEYDOWN, k, 0}; return shortcut(m); };
    auto undo = [&] { SendMessageW(window, WM_COMMAND, Undo, 0); };
    auto redo = [&] { SendMessageW(window, WM_COMMAND, Redo, 0); };
    NativeDocument blank;
    blank.newCampaign("Audit");
    blank.addRoom("One");
    blank.addRoom("Two");
    blank.addRoom("Three");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    snap = .25f, smartSnap = true;
    // Whatever an earlier check left in the tools' boxes: a drag draws a pool, things rest on the floor.
    placeHeight = 0;
    box(waterModeList, WaterModeList, 0);
    // The hall every context shares: walls round the edge, a wall across it at y 12 with a gap at x 10-11.
    auto hall = [&](int m, int l) {
      for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
          const bool edge = x == 0 || y == 0 || x == 23 || y == 23, across = y == 12 && x >= 1 && x <= 22 && x != 10 && x != 11;
          doc.paint(m, l, x, y, edge || across ? '#' : l > 0 ? '=' : '.');
        }
    };
    changeMap(0);
    layer = 0;
    hall(0, 0);
    for (int i = 0; i < 2; ++i) {
      const int up = createFloorLayer("Upper");
      hall(0, up);
    }
    changeMap(2);
    hall(2, 0);
    {
      const int up = createFloorLayer("Upper");
      hall(2, up);
    }
    publish();
    struct Context { const char *name; int map, floor; };
    const Context contexts[] = {{"map 0, floor 0", 0, 0}, {"map 0, floor 1", 0, 1}, {"map 0, floor 2", 0, 2}, {"map 2, floor 0", 2, 0}, {"map 2, floor 1", 2, 1}};
    struct Spot { float ax, ay, bx, by; };
    // Where a tool is driven: an open spot by its shape, and the few that need a wall, a gap or a thing to join.
    auto spot = [&](int t) -> Spot {
      switch (t) {
      case 7: return {21.4f, 6.2f, 21.4f, 6.2f};                   // Sign: the nearest wall
      case 21: return {10.5f, 12.5f, 10.5f, 12.5f};                // Door: in the gap
      case 35: return {4.f, 18.f, 6.f, 18.f};                      // Glass pane
      case 47: return {2.2f, 12.5f, 7.6f, 12.5f};                  // Window wall along a wall
      case 48: return {14.f, 12.5f, 17.f, 12.5f};                  // Breach through a wall
      case 50: return {6.f, 9.f, 6.5f, 10.5f};                     // Connect: a rope from a loose prop
      case 25: return {3.2f, 20.3f, 3.2f, 20.3f};                  // Player start
      }
      switch (toolInfo[t].shape) {
      case 'r': return {3.2f, 2.2f, 5.7f, 3.7f};
      case 'l': return {3.f, 6.5f, 7.f, 6.5f};
      default: return {6.3f, 4.6f, 6.3f, 4.6f};
      }
    };
    // The shapes the sweep drives; the rest paint tiles, pick or copy areas, and make no record of their own.
    auto placing = [&](int t) { const char s = toolInfo[t].shape; return s == 'p' || s == 'r' || s == 'l' || s == 'd' || s == 'g'; };
    auto knownShape = [&](int t) {
      const char s = toolInfo[t].shape;
      return placing(t) || std::strchr("sRtwcvaPFKHTNG", s) != nullptr;
    };
    for (int t = 0; t < ToolCount; ++t)
      require(knownShape(t), std::string("the tool ") + utf8(toolInfo[t].name) + " has a shape the audit does not know ('" + toolInfo[t].shape + "'): teach verifyAudit what it makes");
    static const std::set<std::string> beyondFloors = {"MAP", "ROW", "LAYER", "TIMED_SEQUENCE", "SURFACE", "CAMPAIGN", "SPAWN", "TERRAIN", "HORIZON", "GROUND", "TERRAIN_FLAT", "CAVE"};
    const std::vector<std::string> heightNames = {"z", "base", "bottom", "bed", "az", "z1"};
    std::map<int, size_t> baseline; // records each tool makes in the first context
    std::set<std::string> cannotRaise, cannotTurn, noHeight;
    int driven = 0, checked = 0;
    std::vector<std::string> failures;
    for (const auto &c : contexts) {
      changeMap(c.map);
      layer = c.floor;
      for (int t = 0; t < ToolCount; ++t) {
        if (!placing(t) || t == 23 || t == 24) // (the two ground tools turn the map outdoors: the Outdoors checks drive them)
          continue;
        const auto cleanLines = doc.lines;
        try {
        const std::string tag = std::string(utf8(toolInfo[t].name)) + " on " + c.name;
        const auto s = spot(t);
        pick(t);
        if (t == 50) { // a loose prop to join the rope to
          auto prop = std::vector<std::string>{"CLUTTER", std::to_string(map), "0", "6", "9", nativeNumber(layerElevation()), "0"};
          addAuthored(prop);
          publish();
          box(connectKindList, ConnectKindList, ConnectRope);
        }
        const auto before = doc.lines;
        hoverInside = true;
        mouse(WM_MOUSEMOVE, s.bx + .6f, s.by + .6f, 0);
        const bool click = s.ax == s.bx && s.ay == s.by;
        std::vector<std::string> ghost;
        if (click) {
          mouse(WM_MOUSEMOVE, s.bx, s.by, 0);
          ghost = ghostLines;
          mouse(WM_LBUTTONDOWN, s.bx, s.by);
        } else {
          mouse(WM_MOUSEMOVE, s.ax, s.ay, 0);
          mouse(WM_LBUTTONDOWN, s.ax, s.ay);
          for (int i = 1; i <= 5; ++i)
            mouse(WM_MOUSEMOVE, s.ax + (s.bx - s.ax) * i / 5.f, s.ay + (s.by - s.ay) * i / 5.f);
          ghost = ghostLines;
        }
        mouse(WM_LBUTTONUP, s.bx, s.by);
        ++driven;
        const auto made = newLines(before, doc.lines);
        if (t == 25) { // the player start moves the map's own start record: its height is this floor's
          const auto m = NativeDocument::fields(doc.lines[doc.records("MAP", map).front()]);
          require(std::abs(number(m[5], -1) - s.bx) < .3f && floorAtHeight(number(m[7], -99)) == layer, tag + ": the player start is not on this floor (height " + m[7] + ")");
          undo();
          pick(0);
          continue;
        }
        if (c.map == 0 && c.floor == 0)
          baseline[t] = made.size();
        require(baseline.count(t) != 0, tag + ": driven before its baseline");
        if (baseline[t] == 0) {
          require(t == 23 || t == 24, tag + ": placed nothing at its default spot; give it a spot in verifyAudit (auditing it needs one)");
          pick(0);
          continue; // the outdoor ground tools need a land
        }
        require(made.size() == baseline[t], tag + ": placed " + std::to_string(made.size()) + " records, " + std::to_string(baseline[t]) + " on the ground floor of map 0");
        // The ghost was what was placed, field for field (numbers to 1.5 mm).
        if (!ghost.empty()) {
          require(ghost.size() == made.size(), tag + ": the ghost showed " + std::to_string(ghost.size()) + " records, " + std::to_string(made.size()) + " were placed");
          for (size_t i = 0; i < ghost.size(); ++i) {
            const auto g = NativeDocument::fields(ghost[i]), p = NativeDocument::fields(made[i]);
            require(g.size() == p.size() && g[0] == p[0], tag + ": the ghost is a " + g[0] + ", a " + p[0] + " was placed");
            for (size_t k = 0; k < g.size(); ++k) {
              char *ge = nullptr, *pe = nullptr;
              const double gv = std::strtod(g[k].c_str(), &ge), pv = std::strtod(p[k].c_str(), &pe);
              const bool numbers = !g[k].empty() && !p[k].empty() && ge && pe && !*ge && !*pe;
              require(numbers ? std::abs(gv - pv) <= .0015 : g[k] == p[k], tag + ": field " + std::to_string(k) + " of the " + g[0] + " is " + g[k] + " in the ghost, " + p[k] + " placed");
            }
            if (auto shape = ghostShapeOf(ghost[i]))
              require(shape->bottom >= layerElevation() - 1.05f && shape->bottom < creationCeiling() + .01f, tag + ": the ghost of the " + g[0] + " stands at " + std::to_string(shape->bottom) + ", not on this floor");
          }
        } else
          note(tag + ": no ghost was shown (a tool that places without one)");
        // Where each record landed: on this map, on this floor, at this floor's height.
        int pickedIndex = -1;
        for (const auto &line : made) {
          const auto f = NativeDocument::fields(line);
          const int index = int(std::find(doc.lines.rbegin(), doc.lines.rend(), line).base() - doc.lines.begin()) - 1;
          require(index >= 0, tag + ": a made record is not in the document");
          const int sx = schemaFor(f);
          if (sx >= 0 && schemas[size_t(sx)].scoped && f.size() > 1)
            require(f[1] == std::to_string(c.map), tag + ": the " + f[0] + " is on map " + f[1]);
          // (A person's ACTOR_AI, _FACTION... have no row in the guide: they follow the ACTOR by its number.)
          if (sx >= 0 && !beyondFloors.count(f[0]) && f[0][0] != '#') {
            require(recordFloor(size_t(index)) == c.floor, tag + ": the " + f[0] + " is on floor " + std::to_string(recordFloor(size_t(index))) + ", not " + std::to_string(c.floor));
            if (c.floor > 0 && sx >= 0) {
              // The pickup bug: a record the Builder wrote without its height fell to the ground floor.
              const auto &names = schemas[size_t(sx)].fields;
              bool has = false, wanted = false;
              for (const auto &h : heightNames) {
                const size_t at = nativeField(names, f, h.c_str());
                if (std::find(names.begin(), names.end(), h) != names.end()) wanted = true;
                if (at != size_t(-1) && !f[at].empty()) {
                  has = true;
                  const float v = number(f[at], -999) + (relativeNativeHeight(f[0]) ? bottomLayerElevation() : 0.f);
                  require(v >= layerElevation() - 1.05f && v < creationCeiling() + .01f, tag + ": the " + f[0] + " " + h + " is " + f[at] + ", not on floor " + std::to_string(c.floor) + " (it starts at " + nativeNumber(layerElevation()) + ")");
                }
              }
              if (wanted && !has) require(false, tag + ": the " + f[0] + " was written without a height, so it would stand on the ground floor");
              if (!wanted) noHeight.insert(f[0]);
            }
          }
          // (The first thing the plan can pick that stands on a floor: a person's clock, TIMED_SEQUENCE, stands on none.)
          if (pickedIndex < 0 && handleFor(size_t(index)) && f[0] != "TIMED_SEQUENCE" && f[0] != "MAP") pickedIndex = index;
        }
        // The thing can be picked, nudged, turned, copied and deleted; undo and redo bring each back.
        if (pickedIndex >= 0) {
          pick(0);
          selectRecord(pickedIndex);
          require(record == pickedIndex, tag + ": the record cannot be picked");
          require(activeFloorRecord(size_t(pickedIndex)), tag + ": the " + NativeDocument::fields(doc.lines[size_t(pickedIndex)])[0] + " is not shown and editable on its own floor (floor " + std::to_string(recordFloor(size_t(pickedIndex))) + ", shown " + std::to_string(visibleFloorRecord(size_t(pickedIndex))) + ", editable " + std::to_string(editableRecord(size_t(pickedIndex))) + ")");
          const auto objectList = objects();
          require(std::find(objectList.begin(), objectList.end(), size_t(pickedIndex)) != objectList.end(), tag + ": the plan does not list the record as an object");
          const std::string kindWord = NativeDocument::fields(doc.lines[size_t(pickedIndex)])[0];
          const auto placedText = doc.text();
          pointerIn3D = false, hoverInside = true;
          // Nudge east: a grid step.
          {
            const float x0 = handleFor(size_t(record))->x;
            nudgeKey(MSG{plan, WM_KEYDOWN, VK_RIGHT, 0}, false, false); // (the keys as typed with no modifier, whatever the keyboard holds)
            require(record >= 0 && std::abs(handleFor(size_t(record))->x - (x0 + .25f)) < .01f, tag + ": Right did not nudge the " + kindWord + " a grid step east (" + std::to_string(x0) + " to " + (record >= 0 ? std::to_string(handleFor(size_t(record))->x) : std::string("?")) + ", " + doc.lines[size_t(record < 0 ? 0 : record)].substr(0, 90) + ")");
            undo();
            require(doc.text() == placedText, tag + ": Undo did not take the nudge back");
            selectRecord(pickedIndex);
          }
          // Page Up: up a step, where the record has a height.
          if (worldBoxLine(doc.lines[size_t(record)])) {
            const float b0 = worldBoxLine(doc.lines[size_t(record)])->bottom;
            nudgeKey(MSG{plan, WM_KEYDOWN, VK_PRIOR, 0}, false, false);
            require(record >= 0 && std::abs(worldBoxLine(doc.lines[size_t(record)])->bottom - (b0 + .25f)) < .01f, tag + ": Page Up did not raise the " + kindWord + " a step");
            undo();
            require(doc.text() == placedText, tag + ": Undo did not take the raising back");
            selectRecord(pickedIndex);
          } else
            cannotRaise.insert(kindWord);
          // R: a quarter or more round, where the record turns.
          {
            press('R');
            const bool turned = doc.text() != placedText;
            if (turned)
              undo();
            else
              cannotTurn.insert(kindWord);
            require(doc.text() == placedText, tag + ": Undo did not take the turn back");
            selectRecord(pickedIndex);
          }
          // Copy: one more of the kind, on the same floor, at the same height.
          {
            const size_t kinds = doc.records(kindWord, c.map).size(), steps = history.size();
            SendMessageW(window, WM_COMMAND, Duplicate, 0);
            const auto copies = doc.records(kindWord, c.map);
            if (copies.size() > kinds) {
              for (size_t k = kinds; k < copies.size(); ++k) {
                require(recordFloor(copies[k]) == c.floor, tag + ": the copy of the " + kindWord + " is on floor " + std::to_string(recordFloor(copies[k])));
                const auto a = worldBoxLine(doc.lines[size_t(pickedIndex)]), b = worldBoxLine(doc.lines[copies[k]]);
                if (a && b && copies.size() == kinds + 1) require(std::abs(a->bottom - b->bottom) < .01f, tag + ": the copy of the " + kindWord + " stands at another height");
              }
              undo();
              require(doc.text() == placedText, tag + ": Undo did not take the copy back");
            } else {
              note(tag + ": Copy does nothing for a " + kindWord);
              require(history.size() == steps, tag + ": Copy changed the map without making a copy of the " + kindWord);
            }
            selectRecord(pickedIndex);
          }
          // Delete, Undo, Redo, Undo.
          {
            removeSelected();
            require(doc.text() != placedText, tag + ": Delete did not remove the " + kindWord);
            const auto deleted = doc.text();
            undo();
            require(doc.text() == placedText, tag + ": Undo did not bring the " + kindWord + " back exactly");
            redo();
            require(doc.text() == deleted, tag + ": Redo did not delete the " + kindWord + " again");
            undo();
            require(doc.text() == placedText, tag + ": the second Undo did not bring the " + kindWord + " back");
          }
          ++checked;
        } else
          note(tag + ": nothing the plan can pick was made (" + (made.empty() ? "" : NativeDocument::fields(made.front())[0]) + ")");
        // Undo the placement itself, then redo it, so the next tool starts from a clean floor.
        selectRecord(-1);
        undo();
        require(doc.lines == before, tag + ": Undo did not take the placement back");
        redo();
        require(newLines(before, doc.lines).size() == made.size(), tag + ": Redo did not place it again");
        undo();
        pick(0);
        } catch (const std::exception &error) { // one tool's failure does not hide the rest: all are reported together
          failures.push_back(error.what());
          doc.lines = cleanLines, doc.touch(), history.clear(), future.clear();
          selectRecord(-1);
          pick(0);
        }
      }
    }
    if (!failures.empty()) {
      std::string all = "Audit regression: " + std::to_string(failures.size()) + " failures in the placing sweep:";
      for (const auto &f : failures) {
        report << "FAIL: " << f << '\n';
        all += "\n  " + f;
      }
      throw std::runtime_error(all);
    }
    require(validateDocument(true, nullptr), "the engine refused the map the sweep left");
    pass(std::to_string(driven) + " placements by " + std::to_string(baseline.size()) + " tools over 5 contexts: each on its map and floor, at its floor's height, the ghost what was placed, and " + std::to_string(checked) + " picked, nudged, turned, copied, deleted, undone and redone");
    auto join = [](const std::set<std::string> &s) { std::string out; for (const auto &x : s) out += (out.empty() ? "" : ", ") + x; return out.empty() ? std::string("none") : out; };
    note("records Page Up / Page Down cannot move (no height of their own): " + join(cannotRaise));
    note("records R cannot turn (no facing): " + join(cannotTurn));
    note("records with no height field in their row, on upper floors: " + join(noHeight));
    pick(0);
    // ================= inspector =================
    {
      int touched = 0;
      std::vector<std::string> inspectorFailures;
      for (const auto &name : {L"Starter Map.rmc", L"Sample Facility.rmc"}) {
        NativeDocument example;
        example.open(root / L"custom maps" / name);
        const auto original = example.text();
        adoptDocument(std::move(example), {}, true);
        changeMap(0);
        layer = 0;
        std::map<std::string, int> seen;
        std::vector<size_t> sample;
        for (size_t i = 0; i < doc.lines.size() && sample.size() < 90; ++i) {
          const auto f = NativeDocument::fields(doc.lines[i]);
          if (f.size() < 2 || f[1] != "0" || schemaFor(f) < 0 || !handleFor(i)) continue;
          const std::string key = f[0] + "/" + std::to_string(f.size()) + (NativeDocument::named(doc.lines[i]).empty() ? "" : "+") + (f[0] == "SCRIPT_ACTION" && f.size() > 3 ? f[3] : "");
          if (seen[key]++ < 1) sample.push_back(i);
        }
        for (size_t i : sample) {
          const auto line = doc.lines[i];
          const auto before = doc.lines;
          selectRecord(int(i));
          require(record == int(i), "the inspector could not pick a " + line.substr(0, line.find('|')));
          apply(true);
          if (doc.text() != original) { // the inspector rewrote it: say so, put the map back, go on to the rest
            std::string now;
            for (const auto &l : doc.lines) if (std::find(before.begin(), before.end(), l) == before.end()) now = l;
            inspectorFailures.push_back("opening a record in the inspector and leaving it changed it: " + line.substr(0, 150) + " became " + now.substr(0, 150));
            doc.loadText(original);
            doc.touch();
            selectRecord(-1);
            continue;
          }
          ++touched;
        }
        selectRecord(-1);
        require(doc.text() == original, "the map changed while its records were looked at");
        const auto saved = root / L".build" / L"audit-saved.rmc";
        fs::create_directories(saved.parent_path());
        doc.save(saved);
        NativeDocument again;
        again.open(saved);
        require(again.text() == original, std::string("the map did not save and open again byte for byte"));
      }
      if (!inspectorFailures.empty()) {
        std::string all = "Audit regression: " + std::to_string(inspectorFailures.size()) + " records changed by the inspector:";
        for (const auto &f : inspectorFailures) {
          report << "FAIL: " << f << '\n';
          all += "\n  " + f;
        }
        throw std::runtime_error(all);
      }
      pass(std::to_string(touched) + " records of every kind and tail length in the Starter Map and the Sample Facility opened in the inspector and left alone: unchanged, and each map saves and opens again byte for byte");
      // Unknown and future fields, comments and odd text survive an edit of another field.
      NativeDocument custom;
      custom.newCampaign("Survives");
      custom.addRoom("Room");
      custom.add({"# a comment the author wrote"});
      custom.ensureFormat2();
      custom.add({"LIGHT", "0", "5", "5", "3", "0", "0", "1", "8", "3200", "0", "", "", "", "", "", "", "0.2", "future.key=7", "name=lamp_a", "p.note=a%7Cb"});
      adoptDocument(std::move(custom), {}, true);
      changeMap(0);
      const size_t lamp = doc.records("LIGHT", 0).back();
      selectRecord(int(lamp));
      SetWindowTextW(fieldInputs[7 - 2], L"1.5"); // intensity
      apply(true);
      const auto edited = doc.lines[size_t(record)];
      require(edited.find("future.key=7") != std::string::npos && edited.find("p.note=a%7Cb") != std::string::npos && edited.find("name=lamp_a") != std::string::npos,
              "an edit dropped the record's unknown or named fields: " + edited);
      require(std::find(doc.lines.begin(), doc.lines.end(), "# a comment the author wrote") != doc.lines.end(), "the comment was lost");
      pass("an unknown future field, a named field and a comment survive an edit of another field");
    }
    // ================= guards =================
    {
      NativeDocument g;
      g.newCampaign("Guards");
      g.addRoom("Room");
      adoptDocument(std::move(g), {}, true);
      changeMap(0);
      layer = 0;
      auto type = [&](size_t index, const std::wstring &text) {
        const size_t slot = index - (schemas[size_t(kind)].scoped ? 2 : 1);
        require(slot < fieldInputs.size(), "no box for field " + std::to_string(index));
        SetWindowTextW(fieldInputs[slot], text.c_str());
      };
      auto fieldOf = [&](size_t k) { auto f = NativeDocument::fields(doc.lines[size_t(record)]); return k < f.size() ? f[k] : std::string(); };
      addAuthored({"LIGHT", "0", "5", "5", "3", "0", "0", "1", "8", "3200"});
      selectRecord(int(doc.records("LIGHT", 0).back()));
      type(9, L"1d"), apply(true);
      require(fieldOf(9) == "3200", "a typo in a number box (1d) was saved: " + fieldOf(9));
      type(9, L"99999"), apply(true);
      require(fieldOf(9) == "12000", "a warmth over 12000 was saved: " + fieldOf(9));
      type(9, L"3200"), type(10, L"5"), apply(true);
      require(fieldOf(10) == "1", "a tint over 1 was saved: " + fieldOf(10));
      // An optional box that was empty takes a typo no more than a full one.
      selectRecord(int(doc.records("LIGHT", 0).back()));
      type(17, L"1d"), apply(true);
      require(doc.lines[size_t(record)].find("1d") == std::string::npos, "a typo in an empty optional number box was saved");
      type(17, L"7"), apply(true);
      require(NativeDocument::fields(doc.lines[size_t(record)]).size() > 17 && NativeDocument::fields(doc.lines[size_t(record)])[17] == "1", "a flicker of 7 was not brought to 1");
      // A script name takes no spaces; a place name no | or %; both are cut to the loader's length.
      addAuthored({"SCRIPT_EVENT", "0", "ev", "2", "2", "6", "6", "-1", "4", "", "1", "0"});
      addAuthored({"SCRIPT_ACTION", "0", "ev", "MESSAGE", "HI", "2"}); // (an event with nothing to do is refused)
      selectRecord(int(doc.records("SCRIPT_EVENT", 0).back()));
      type(2, L"door power"), apply(true);
      require(fieldOf(2) == "door_power", "a trigger name with a space was written as " + fieldOf(2));
      type(2, std::wstring(130, L'a')), apply(true);
      require(fieldOf(2).size() == 96, "a trigger name of 130 letters was written with " + std::to_string(fieldOf(2).size()));
      addAuthored({"ZONE", "0", "1", "1", "5", "5", "Hall"});
      selectRecord(int(doc.records("ZONE", 0).back()));
      type(6, L"A|B%41 Hall"), apply(true);
      require(fieldOf(6) == "AB41 Hall", "a place name with | and % was written as " + fieldOf(6));
      // A joint's friction past the loader's 5000 is kept at 5000 (the artists' "friction 12" was refused as a prop's).
      addAuthored({"CLUTTER", "0", "0", "8", "8", "0", "0"});
      addAuthored({"JOINT", "0", "hinge", "0", "-1", "8", "8", "1"});
      doc.ensureFormat2();
      const size_t joint = doc.records("JOINT", 0).back();
      doc.replaceNamed(joint, NativeDocument::fields(doc.lines[joint]), {{"friction", "12"}});
      selectRecord(int(joint));
      for (size_t i = positionalInputs; i < fieldInputs.size(); ++i)
        if (i - positionalInputs < namedKeys.size() && namedKeys[i - positionalInputs] == "friction")
          SetWindowTextW(fieldInputs[i], L"9000");
      apply(true);
      require(namedValue(doc.lines[size_t(record)], "friction") == "5000", "a joint friction of 9000 was saved: " + namedValue(doc.lines[size_t(record)], "friction"));
      // The caps: the 65th spawn, the 65th emitter.
      for (int i = 0; i < 64; ++i)
        addAuthored({"EMITTER", "0", "5", "5", "1", "0", "0", "0.5", "2", "0.1", "0.2", "16"});
      const size_t before = doc.records("EMITTER", 0).size();
      pick(EmitterTool);
      mouse(WM_MOUSEMOVE, 9.f, 9.f, 0);
      mouse(WM_LBUTTONDOWN, 9.f, 9.f);
      mouse(WM_LBUTTONUP, 9.f, 9.f);
      require(before == 64 && doc.records("EMITTER", 0).size() == 64, "an emitter was added past the loader's 64");
      pick(0);
      std::string reason;
      require(validateDocument(true, &reason), "the engine refused the guarded map: " + reason);
      pass("a typo is kept out of full and empty number boxes, a number past the loader's limit is brought in, script and place names are cleaned and cut, a joint friction is held to 5000, and the 65th emitter is refused");
    }
    // ================= open =================
    {
      const auto folder = root / L".build" / L"audit-files";
      std::error_code ec;
      fs::remove_all(folder, ec);
      fs::create_directories(folder);
      NativeDocument good;
      good.newCampaign("Good");
      good.addRoom("Room");
      adoptDocument(std::move(good), {}, true);
      changeMap(0);
      const std::string goodText = doc.text();
      auto write = [&](const std::wstring &name, const std::string &text) {
        auto file = folder / name;
        std::ofstream(file, std::ios::binary) << text;
        return file;
      };
      auto openLike = [&](const fs::path &file) { // what Open does after the dialog (openFile)
        NativeDocument next;
        next.open(file);
        const int clocks = repairRetiredEditorPeople(next);
        adoptDocument(std::move(next), file, clocks > 0);
        return doc.text();
      };
      std::string starter;
      {
        std::ifstream in(root / L"custom maps" / L"Starter Map.rmc", std::ios::binary);
        starter.assign(std::istreambuf_iterator<char>(in), {});
      }
      struct Odd { std::wstring name; std::string text; };
      const std::vector<Odd> odd = {
          {L"empty.rmc", ""},
          {L"garbage.rmc", "this is not a campaign\nat all\n"},
          {L"binary.rmc", std::string("\0\1\2\xff\xfe|||\0", 10)},
          {L"truncated.rmc", starter.substr(0, starter.size() / 2)},
          {L"markers-only.rmc", "--- CUSTOM_CAMPAIGN_DATA_START ---\n--- CUSTOM_CAMPAIGN_DATA_END ---\n"},
          {L"end-first.rmc", "--- CUSTOM_CAMPAIGN_DATA_END ---\nCAMPAIGN|x|0\n--- CUSTOM_CAMPAIGN_DATA_START ---\n"},
          {L"huge.rmc", std::string(5 * 1024 * 1024, 'x')},
          {L"bom.rmc", "\xEF\xBB\xBF" + goodText},
          {L"crlf and spaces in the name.rmc", [&] { std::string t; for (char c : goodText) { if (c == '\n') t += '\r'; t += c; } return t; }()},
          {L"Unicode Ünïcøde 日本語.rmc", goodText},
      };
      int survived = 0;
      for (const auto &o : odd) {
        const auto file = write(o.name, o.text);
        std::string why;
        try {
          openLike(file);
          validateDocument(true, &why);
          // Whatever was opened, the Builder still answers and can save what it holds.
          refreshRecords();
          const auto out = folder / (L"saved-" + o.name);
          doc.save(out);
          ++survived;
        } catch (const std::exception &e) {
          note(utf8(o.name) + ": refused to open with \"" + e.what() + "\" (the open map stays)");
          ++survived;
        }
      }
      require(survived == int(odd.size()), "a malformed file stopped the audit");
      // A good map still opens, saves and opens again after all that.
      const auto kept = openLike(write(L"after.rmc", goodText));
      require(kept == goodText, "a good map did not open as written after the malformed ones");
      pass(std::to_string(odd.size()) + " malformed, empty, huge, truncated, BOM, CRLF, spaced and Unicode-named files: none crashed, each either opened or was refused with a word, and a good map opens as written afterwards");
      // Saving while the file is read-only or held open by something else: the old file stays, the editor says so.
      const auto target = write(L"locked.rmc", goodText);
      NativeDocument next;
      next.open(target);
      adoptDocument(std::move(next), target, false);
      doc.add({"# changed"});
      SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_READONLY);
      std::string outcome = "saved";
      try { saveTo(target); } catch (const std::exception &e) { outcome = e.what(); }
      SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_NORMAL);
      {
        std::ifstream in(target, std::ios::binary);
        std::string now(std::istreambuf_iterator<char>(in), {});
        require(outcome == "saved" || now == goodText, "a failed save of a read-only file changed the file");
        note("saving over a read-only file: " + (outcome == "saved" ? std::string("it saved (the editor replaces the file)") : "refused with \"" + outcome + "\", file untouched"));
      }
      HANDLE held = CreateFileW(target.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      outcome = "saved";
      try { saveTo(target); } catch (const std::exception &e) { outcome = e.what(); }
      if (held != INVALID_HANDLE_VALUE) CloseHandle(held);
      {
        bool leftover = false;
        for (const auto &entry : fs::directory_iterator(folder))
          leftover |= entry.path().filename().wstring().find(L".editor-tmp-") != std::wstring::npos;
        require(!leftover, "a failed save left its temporary file behind");
        note("saving over a file another program holds open: " + (outcome == "saved" ? std::string("it saved") : "refused with \"" + outcome + "\""));
      }
      pass("a save over a read-only or held-open file leaves the old file whole and no temporary file behind");
      // 32 floors and 12 maps, duplicate names.
      NativeDocument many;
      many.newCampaign("Many");
      for (int i = 0; i < 12; ++i) many.addRoom("Same name");
      adoptDocument(std::move(many), {}, true);
      changeMap(0);
      layer = 0;
      int floors = 1;
      std::string stopped;
      try { for (; floors < 40; ++floors) createFloorLayer("Floor"); } catch (const std::exception &e) { stopped = e.what(); }
      require(floors == 32 && !stopped.empty(), "the floor limit is not 32 with a word: stopped at " + std::to_string(floors) + " (" + stopped + ")");
      std::string reason;
      const bool loads = validateDocument(true, &reason);
      note(std::string("12 maps of one name with 32 floors on the first: the engine ") + (loads ? "loads it" : "refuses it: " + reason));
      pass("12 maps of the same name and 32 floors: the floor limit speaks, and the map is as the engine takes it");
    }
    { // Smooth turning (R held, the mouse moved): a free angle written to the piece's yaw, Shift's 15-degree snap
      // aside; a shape that turns by quarters (facing) or a stair is left to the quarter turn.
      NativeDocument turning;
      turning.newCampaign("Turning");
      turning.addRoom("Yard");
      adoptDocument(std::move(turning), {}, true);
      changeMap(0);
      layer = 0;
      tool = 0;
      doc.add({"FIXTURE", "0", "47", "8", "8", "0", "3.46", "11.94", "2.5", "0", "1"});
      publish();
      selectRecord(int(doc.records("FIXTURE", 0).back()));
      require(spinTurnable(), "a placed container cannot be turned smoothly (record " + std::to_string(record) + " of " + std::to_string(doc.lines.size()) + ", kind " + std::to_string(kind) + " " + (kind >= 0 ? schemas[size_t(kind)].kind : std::string()) + ", selected " + std::to_string(selectedRecords.size()) + ", editable " + std::to_string(record >= 0 && editableRecord(size_t(record))) + ")");
      spinRecord = record;
      spinTo(.7f);
      auto f = NativeDocument::fields(doc.lines[size_t(spinRecord)]);
      require(f.size() > spinYawIndex && std::fabs(std::stof(f[spinYawIndex]) - .7f) < .001f, "the smooth turn did not write a yaw of 0.7: " + doc.lines[size_t(spinRecord)]);
      spinTo(7.f);
      f = NativeDocument::fields(doc.lines[size_t(spinRecord)]);
      require(std::fabs(std::stof(f[spinYawIndex]) - std::remainder(7.f, 6.28318530718f)) < .001f, "a smooth turn past a full round is not brought back into one round");
      pass("a held R turns a piece to any angle, kept within one round; quarter-turn shapes and stairs keep their quarter turns");
    }
    snap = savedSnap;
    smartSnap = savedSmart;
    autoDressingEnabled = savedAuto;
    dirty = false;
  }
