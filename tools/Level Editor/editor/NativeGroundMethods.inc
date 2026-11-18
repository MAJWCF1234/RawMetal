  // ---- Footings: a room or a floor on the land, or raised off a floor (src/world/Footing.h).
  // A STRUCT with footing= is the footprint; the engine lays its slab, plinth, posts,
  // terraces and door steps and cuts the land under it. The builder plans the same
  // footing from the same ground (picks, the plan, the 3D handles), writes them from the
  // Room and Floor tools and the inspector's Footing rows, and paints their parts.
  struct EditorFooting {
    size_t line = 0;
    int map = 0;
    retro::FootingSpec spec;
    int base = -1, cap = -1;
    float capWidth = .1f;
    bool steps = true, land = false;
    std::vector<retro::FootingPart> parts;
  };
  // Shown by the layout (the window itself may be hidden: the test modes).
  static bool shownControl(HWND h) { return h && (GetWindowLongW(h, GWL_STYLE) & WS_VISIBLE) != 0; }
  mutable std::uint64_t footingVersion = ~0ull;
  mutable std::vector<EditorFooting> footingCache;
  mutable std::optional<retro::TerrainShape> naturalLand; // the land before any footing cut it
  static std::string namedValue(const std::string &line, const char *key) {
    for (auto &[k, v] : NativeDocument::named(line))
      if (k == key) return v;
    return {};
  }
  static std::optional<EditorFooting> footingOf(const std::string &line) {
    auto mode = namedValue(line, "footing");
    if (mode.empty()) return std::nullopt;
    auto f = NativeDocument::fields(line);
    if (f.size() < 10 || f[0] != "STRUCT") return std::nullopt;
    EditorFooting out;
    out.spec.mode = std::max(1, retro::footingModeOf(mode));
    out.spec.x1 = number(f[2], 0), out.spec.y1 = number(f[3], 0), out.spec.x2 = number(f[4], 1), out.spec.y2 = number(f[5], 1);
    out.spec.bottom = number(f[6], 0), out.spec.top = number(f[7], .1f);
    auto snap = namedValue(line, "snap");
    if (!snap.empty()) out.spec.snap = std::max(0, retro::footingSnapOf(snap));
    auto real = [&](const char *key, float fallback) { auto v = namedValue(line, key); return v.empty() ? fallback : number(v, fallback); };
    out.spec.raise = std::clamp(real("raise", 0), 0.f, retro::FootingMaxRaise), out.spec.terrace = std::clamp(real("terrace", 3), 1.f, 12.f);
    out.base = int(real("base", -1)), out.cap = int(real("cap", -1)), out.capWidth = real("capwidth", .1f);
    out.steps = namedValue(line, "steps") != "0";
    return out;
  }
  // Every footing in the campaign, planned on the land as it was before any of them cut it.
  // `land` (campaignTerrain's shape, being built) takes their pads, as the engine's does.
  void planFootings(retro::TerrainShape *land) const {
    footingCache.clear();
    footingVersion = doc.version;
    if (land) naturalLand = *land;
    else naturalLand.reset();
    std::vector<std::pair<int, size_t>> maps;
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 1) maps.push_back({int(number(f[1], 0)), i});
    }
    std::sort(maps.begin(), maps.end());
    std::vector<retro::TerrainFlat> pads;
    for (auto [m, at] : maps) {
      auto mf = NativeDocument::fields(doc.lines[at]);
      const bool outdoor = land && mf.size() > 11 && mf[8] == "1" && retro::terrainSkybox(decoded(mf[11]));
      const float ox = mf.size() > 4 ? number(mf[3], 0) : 0, oy = mf.size() > 4 ? number(mf[4], 0) : 0;
      for (auto i : doc.records("STRUCT", m)) {
        auto footing = footingOf(doc.lines[i]);
        if (!footing) continue;
        footing->line = i, footing->map = m, footing->land = outdoor;
        footing->parts = retro::planFooting(footing->spec, outdoor, [&](float x, float y) { return retro::authoredTerrainSurface(*naturalLand, ox, oy, x, y); });
        if (outdoor) {
          auto made = retro::footingPads(footing->spec, footing->parts, ox, oy);
          pads.insert(pads.end(), made.begin(), made.end());
        }
        footingCache.push_back(std::move(*footing));
      }
    }
    if (land) land->flats.insert(land->flats.end(), pads.begin(), pads.end());
  }
  const std::vector<EditorFooting> &footings() const {
    campaignTerrain();
    if (footingVersion != doc.version) planFootings(nullptr);
    return footingCache;
  }
  const EditorFooting *footingAtLine(size_t line) const {
    for (auto &f : footings())
      if (f.line == line) return &f;
    return nullptr;
  }
  // The level piece of a footing on map m under (x, y): its floor is the ground there.
  const retro::FootingPart *footingPartAt(int m, float x, float y) const {
    for (auto &f : footings())
      if (f.map == m)
        for (auto &p : f.parts)
          if (x >= p.x1 - .0005f && x <= p.x2 + .0005f && y >= p.y1 - .0005f && y <= p.y2 + .0005f) return &p;
    return nullptr;
  }
  // The natural land (before footings) at map metres on this map.
  float naturalGroundAt(float x, float y) const {
    if (!onTerrain() || !naturalLand) return layerElevation();
    auto [ox, oy] = mapOrigin();
    return retro::authoredTerrainSurface(*naturalLand, ox, oy, x, y);
  }
  // Sets (or with an empty value removes) a named field; FORMAT|2 goes in when needed.
  size_t setNamedValue(size_t at, const std::string &key, const std::string &value) {
    auto f = NativeDocument::fields(doc.lines[at]);
    auto n = NativeDocument::named(doc.lines[at]);
    bool found = false;
    for (auto it = n.begin(); it != n.end();)
      if (it->first == key) {
        found = true;
        if (value.empty()) it = n.erase(it);
        else it->second = value, ++it;
      } else ++it;
    if (!found && !value.empty()) n.emplace_back(key, value);
    const size_t now = doc.replaceNamed(at, f, n);
    if (now != at && record >= 0) record += int(now - at);
    return now;
  }

  // ---- The Footing rows: for a picked footing, or the Room (and, on the land, Floor) tool's next one.
  HWND footingModeList = nullptr, footingSnapList = nullptr, footingRaiseBox = nullptr, footingHeightBox = nullptr;
  int newFootingMode = 0, newFootingSnap = retro::FootingSnapMean; // 0: picked for the slope (footRoom)
  bool footingAdvanced = false; // exact heights: the floor's height and what it is measured from
  float newFootingRaise = 0;
  bool newFootingSteps = true, footingFilling = false;
  bool footingPicked() const { return record >= 0 && size_t(record) < doc.lines.size() && footingOf(doc.lines[size_t(record)]).has_value(); }
  bool footingPanelShown() const { return footingPicked() || (tool == 1 && layer == 0) || (tool == 2 && layer == 0 && onTerrain()); }
  void createFootingControls() {
    footingModeList = controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, FootingModeList);
    for (auto *text : {L"Best for the slope", L"Pad: the ground made level", L"Foundation: a base down to the ground", L"Stilts: posts, walk under it", L"Steps: terraces up the slope"})
      SendMessageW(footingModeList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    footingSnapList = controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, FootingSnapList);
    for (auto *text : {L"Lowest ground under it", L"Average ground under it", L"Highest ground under it", L"The height typed below"})
      SendMessageW(footingSnapList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    footingRaiseBox = controlWindow(L"EDIT", L"0", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, FootingRaiseBox);
    SendMessageW(footingRaiseBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"0 to 6 m"));
    footingHeightBox = controlWindow(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, FootingHeightBox);
    SendMessageW(footingHeightBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"from the ground"));
    button(L"Steps at raised doors: on", FootingStepsToggle);
    button(L"Exact heights...", FootingAdvancedToggle);
    fillFootingPanel();
  }
  std::vector<HWND> footingControls() const { return {footingModeList, footingSnapList, footingRaiseBox, footingHeightBox, GetDlgItem(window, FootingStepsToggle), GetDlgItem(window, FootingAdvancedToggle)}; }
  // The rows, in the inspector's layout (relayout's row()).
  template <class Row> void footingRows(Row &&row) {
    fillFootingPanel();
    if (!onTerrain()) {
      // Indoors there is no slope: only how high the floor is lifted on its base.
      row(footingRaiseBox, L"Raise the floor (m)", 26);
      row(GetDlgItem(window, FootingStepsToggle), L"", 30);
      return;
    }
    row(footingModeList, L"On the slope");
    row(GetDlgItem(window, FootingStepsToggle), L"", 30);
    row(GetDlgItem(window, FootingAdvancedToggle), L"", 30);
    // Exact numbers wait behind the button; the 3D view's edge handle raises it by eye.
    if (footingAdvanced) {
      row(footingRaiseBox, L"Raise (m)", 26);
      if (onTerrain()) row(footingSnapList, L"Floor sits on"), row(footingHeightBox, L"Ground height (m)", 26);
    }
  }
  void fillFootingPanel() {
    if (!footingModeList || footingFilling) return;
    footingFilling = true;
    int mode = newFootingMode, snap = newFootingSnap;
    float raise = newFootingRaise;
    bool steps = newFootingSteps;
    std::wstring height;
    if (footingPicked())
      if (auto *f = footingAtLine(size_t(record))) {
        mode = f->spec.mode, snap = f->spec.snap, raise = f->spec.raise, steps = f->steps;
        if (!f->parts.empty()) height = wide(shortNumber(f->parts.front().level, 2));
      }
    SendMessageW(footingModeList, CB_SETCURSEL, std::clamp(mode, 0, 4), 0);
    SetWindowTextW(GetDlgItem(window, FootingAdvancedToggle), footingAdvanced ? L"Hide exact heights" : L"Exact heights...");
    SendMessageW(footingSnapList, CB_SETCURSEL, std::clamp(snap, 0, 3), 0);
    if (GetFocus() != footingRaiseBox) SetWindowTextW(footingRaiseBox, wide(shortNumber(raise, 2)).c_str());
    if (GetFocus() != footingHeightBox) SetWindowTextW(footingHeightBox, height.c_str());
    SetWindowTextW(GetDlgItem(window, FootingStepsToggle), steps ? L"Steps at raised doors: on" : L"Steps at raised doors: off");
    footingFilling = false;
  }
  // One change to the picked footing: one Ctrl+Z.
  void editFooting(const std::string &key, const std::string &value, const std::wstring &said) {
    if (!footingPicked()) return;
    remember();
    setNamedValue(size_t(record), key, value);
    publish();
    fillFields(false);
    fillFootingPanel();
    InvalidateRect(plan, nullptr, FALSE);
    message(said);
  }
  static float typedNumber(HWND box, float fallback) {
    wchar_t text[32]{};
    GetWindowTextW(box, text, 32);
    wchar_t *end = nullptr;
    float v = std::wcstof(text, &end);
    return end != text && std::isfinite(v) ? v : fallback;
  }
  // WM_COMMAND from the Footing rows; true when it was one of them.
  bool footingCommand(int id, int notification) {
    if (footingFilling) return id == FootingModeList || id == FootingSnapList || id == FootingRaiseBox || id == FootingHeightBox;
    const bool picked = footingPicked();
    if (id == FootingModeList && notification == CBN_SELCHANGE) {
      int mode = std::clamp(int(SendMessageW(footingModeList, CB_GETCURSEL, 0, 0)), 0, 4);
      static const wchar_t *said[] = {L"", L"The ground under it is made level.", L"It stands on a foundation down to the ground.", L"It stands on posts; you can walk under it.",
                                      L"It steps up the slope in terraces."};
      if (picked) {
        if (mode == 0) {
          auto *f = footingAtLine(size_t(record));
          mode = f ? bestFooting(f->spec.x1, f->spec.y1, f->spec.x2, f->spec.y2) : retro::FootingPad;
        }
        editFooting("footing", retro::FootingModeNames[mode], said[mode]);
      } else newFootingMode = mode, message(mode ? std::wstring(said[mode]) : std::wstring(L"New rooms on a slope get what suits it: level ground, or a foundation where it is steep."));
      return true;
    }
    if (id == FootingAdvancedToggle) {
      footingAdvanced = !footingAdvanced;
      relayout();
      return true;
    }
    if (id == FootingSnapList && notification == CBN_SELCHANGE) {
      int snap = std::clamp(int(SendMessageW(footingSnapList, CB_GETCURSEL, 0, 0)), 0, 3);
      if (!picked) { newFootingSnap = snap; return true; }
      if (snap == retro::FootingSnapFixed) {
        // Typed: keep it where it stands now, then let the Height box move it.
        auto *f = footingAtLine(size_t(record));
        if (f && !f->parts.empty()) {
          remember();
          auto fields = NativeDocument::fields(doc.lines[size_t(record)]);
          const float thick = number(fields[7], .1f) - number(fields[6], 0), level = f->parts.front().level;
          fields[6] = nativeNumber(level), fields[7] = nativeNumber(level + thick);
          doc.replace(size_t(record), fields);
          setNamedValue(size_t(record), "snap", "fixed");
          publish(), fillFields(false), fillFootingPanel();
          message(L"The floor stays at " + wide(shortNumber(level, 2)) + L" m; type another height to move it.");
        }
      } else {
        auto fields = NativeDocument::fields(doc.lines[size_t(record)]);
        remember();
        if (namedValue(doc.lines[size_t(record)], "snap") == "fixed") {
          // Back to the ground: measured from it again (the slab's thickness kept).
          const float thick = number(fields[7], .1f) - number(fields[6], 0);
          fields[6] = "0", fields[7] = nativeNumber(thick);
          doc.replace(size_t(record), fields);
        }
        setNamedValue(size_t(record), "snap", retro::FootingSnapNames[snap]);
        publish(), fillFields(false), fillFootingPanel();
        static const wchar_t *said[] = {L"lowest", L"average", L"highest"};
        message(std::wstring(L"The floor stands on the ") + said[snap] + L" ground under it.");
      }
      return true;
    }
    if ((id == FootingRaiseBox || id == FootingHeightBox) && notification == EN_KILLFOCUS) {
      if (id == FootingRaiseBox) {
        float raise = std::clamp(typedNumber(footingRaiseBox, 0), 0.f, retro::FootingMaxRaise);
        if (!picked) { newFootingRaise = raise; fillFootingPanel(); return true; }
        auto *f = footingAtLine(size_t(record));
        if (f && std::abs(f->spec.raise - raise) > .0005f)
          editFooting("raise", raise > 0 ? shortNumber(raise, 3) : std::string(), L"Raised " + wide(shortNumber(raise, 2)) + L" m; a plinth fills under it.");
      } else if (picked) {
        auto *f = footingAtLine(size_t(record));
        float typed = typedNumber(footingHeightBox, f && !f->parts.empty() ? f->parts.front().level : 0);
        if (f && !f->parts.empty() && std::abs(typed - f->parts.front().level) > .0005f) {
          remember();
          auto fields = NativeDocument::fields(doc.lines[size_t(record)]);
          const float thick = number(fields[7], .1f) - number(fields[6], 0);
          fields[6] = nativeNumber(typed), fields[7] = nativeNumber(typed + thick);
          doc.replace(size_t(record), fields);
          setNamedValue(size_t(record), "snap", "fixed");
          publish(), fillFields(false), fillFootingPanel();
          message(L"The floor's ground is at " + wide(shortNumber(typed, 2)) + L" m now.");
        }
      }
      return true;
    }
    if (id == FootingStepsToggle) {
      if (!picked) { newFootingSteps = !newFootingSteps; fillFootingPanel(); return true; }
      auto *f = footingAtLine(size_t(record));
      const bool now = f ? !f->steps : false;
      editFooting("steps", now ? std::string() : "0", now ? L"Raised doors get steps down to the ground." : L"No steps at raised doors.");
      return true;
    }
    return false;
  }

  // ---- The Room and Floor tools: a footing for what they lay.
  // A room drawn on the land (or with a Raise on any floor) stands on a footing over its walls and floor.
  void footRoom(int tx0, int ty0, int tx1, int ty1) {
    if (layer != 0 || ghosting) return;
    const bool land = onTerrain();
    if (!land && newFootingRaise <= 0) return;
    const float z = land ? 0.f : layerElevation();
    std::vector<std::string> f{"STRUCT", std::to_string(map), std::to_string(tx0), std::to_string(ty0), std::to_string(tx1 + 1), std::to_string(ty1 + 1),
                               nativeNumber(z), nativeNumber(z + .1f), "0", std::to_string(floorPaintMaterial)};
    const int mode = newFootingMode > 0 ? newFootingMode : land ? bestFooting(float(tx0), float(ty0), float(tx1 + 1), float(ty1 + 1)) : retro::FootingFoundation;
    f.push_back(std::string("footing=") + retro::FootingModeNames[mode]);
    if (land && newFootingSnap != retro::FootingSnapMean) f.push_back(std::string("snap=") + retro::FootingSnapNames[std::clamp(newFootingSnap, 0, 2)]);
    if (newFootingRaise > 0) f.push_back("raise=" + shortNumber(newFootingRaise, 3));
    if (!newFootingSteps) f.push_back("steps=0");
    doc.ensureFormat2();
    doc.addOnLayer(f, layer);
  }
  // What suits the ground under a footprint: level ground where it is gentle, a foundation where it is steep.
  int bestFooting(float x1, float y1, float x2, float y2) const {
    if (!onTerrain()) return retro::FootingFoundation;
    float low = 1e9f, high = -1e9f;
    for (float y = std::min(y1, y2); y <= std::max(y1, y2) + .001f; y += .5f)
      for (float x = std::min(x1, x2); x <= std::max(x1, x2) + .001f; x += .5f) {
        const float z = naturalGroundAt(x, y);
        low = std::min(low, z), high = std::max(high, z);
      }
    return high - low <= 1.2f ? retro::FootingPad : retro::FootingFoundation;
  }
  // The Floor tool on the land, outside any footing: the floor is laid level, as a pad at
  // the level of a footing it adjoins (or the mean ground under the first cell of a stroke),
  // so a floor painted cell by cell up a slope is one floor. Inside a footing a floor is a
  // plain slab on its floor, as indoors. Returns true when it laid it.
  bool floorOnLand(float x0, float y0, float x1, float y1, int material) {
    if (layer != 0 || !onTerrain()) return false;
    if (footingPartAt(map, (x0 + x1) * .5f, (y0 + y1) * .5f)) return false;
    float level = 0;
    bool joined = false;
    for (auto &f : footings())
      if (f.map == map && !f.parts.empty())
        for (auto &p : f.parts)
          if (x0 <= p.x2 + .001f && x1 >= p.x1 - .001f && y0 <= p.y2 + .001f && y1 >= p.y1 - .001f) { level = p.level, joined = true; break; }
    if (!joined) {
      float sum = 0;
      int n = 0;
      for (float y = y0; y <= y1 + .001f; y += std::max(.25f, (y1 - y0) / 4))
        for (float x = x0; x <= x1 + .001f; x += std::max(.25f, (x1 - x0) / 4)) sum += naturalGroundAt(x, y), ++n;
      level = std::round(sum / float(std::max(1, n)) * 32.f) / 32.f;
    }
    std::vector<std::string> f{"STRUCT", std::to_string(map), nativeNumber(x0), nativeNumber(y0), nativeNumber(x1), nativeNumber(y1),
                               nativeNumber(level), nativeNumber(level + .1f), "0", std::to_string(material), "footing=pad", "snap=fixed"};
    doc.ensureFormat2();
    doc.addOnLayer(f, layer);
    return true;
  }
  // The Erase tool on the land takes up the footing floors it covers whole.
  void eraseFootings(float x0, float y0, float x1, float y1) {
    if (layer != 0) return;
    auto all = doc.records("STRUCT", map);
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
      auto fo = footingOf(doc.lines[*it]);
      if (fo && std::min(fo->spec.x1, fo->spec.x2) >= x0 - .001f && std::max(fo->spec.x1, fo->spec.x2) <= x1 + .001f &&
          std::min(fo->spec.y1, fo->spec.y2) >= y0 - .001f && std::max(fo->spec.y1, fo->spec.y2) <= y1 + .001f)
        doc.eraseRecord(*it);
    }
  }

  // ---- In the 3D view: what of a footing a point is on, and its Raise handle.
  // part: 0 the floor, 1 the plinth, posts and steps (base=), 2 the cap ring (cap=).
  bool footingAt(float x, float y, float z, size_t &at, int &part) const {
    for (auto &f : footings()) {
      if (f.map != map) continue;
      for (auto &p : f.parts) {
        if (x < p.x1 - .02f || x > p.x2 + .02f || y < p.y1 - .02f || y > p.y2 + .02f) continue;
        if (z < -999) return at = f.line, part = 0, true;
        if (z > p.surface + .06f) continue;
        const float edge = std::min({x - p.x1, p.x2 - x, y - p.y1, p.y2 - y});
        if (z >= p.surface - .05f) return at = f.line, part = f.cap >= 0 && edge < f.capWidth ? 2 : 0, true;
        if (z >= p.slab - .02f && f.cap >= 0) return at = f.line, part = 2, true;
        if (edge < .06f || f.spec.mode != retro::FootingStilts) return at = f.line, part = 1, true;
      }
    }
    return false;
  }
  // The selection outline of a picked footing stands where its floor is; its top edge
  // (kind 1) is the Raise handle.
  bool footingGuides(std::vector<retro::EditorGuide> &out) const {
    if (!footingPicked()) return false;
    auto *f = footingAtLine(size_t(record));
    if (!f || f->parts.empty()) return false;
    for (auto &p : f->parts) {
      const float foot = f->land ? std::min(p.level, groundHigh(p.x1, p.y1, p.x2, p.y2, false)) : p.level;
      const std::pair<float, float> c[4] = {{p.x1, p.y1}, {p.x2, p.y1}, {p.x2, p.y2}, {p.x1, p.y2}};
      for (int i = 0; i < 4; ++i) {
        auto a = c[i], b = c[(i + 1) % 4];
        out.push_back({a.first, a.second, p.surface, b.first, b.second, p.surface, 1});
        out.push_back({a.first, a.second, foot, b.first, b.second, foot, 0});
        out.push_back({a.first, a.second, foot, a.first, a.second, p.surface, 0});
      }
    }
    return true;
  }
  // A drag of that handle changes the Raise (0 to 6 m), the floor riding up on its plinth.
  bool footingHeightDelta(float delta) {
    auto original = footingOf(objectOriginal);
    if (!original) return false;
    float raise = std::clamp(original->spec.raise + delta, 0.f, retro::FootingMaxRaise);
    // A soft snap at heights that mean something: the ground, a step, two, half a storey, a storey.
    for (float stop : {0.f, .18f, .36f, 1.4f, 2.8f, 4.2f, 5.6f})
      if (std::abs(raise - stop) < (stop < .5f ? .06f : .14f)) raise = stop;
    raise = std::round(raise * 100.f) / 100.f;
    doc.loadText(dragDocument);
    if (std::abs(raise - original->spec.raise) > .0005f) setNamedValue(size_t(record), "raise", raise > 0 ? shortNumber(raise, 3) : std::string());
    objectMoved = doc.text() != dragDocument;
    updateInspector();
    const wchar_t *named = raise == 0 ? L" (on the ground)" : raise == .18f ? L" (a step)" : raise == .36f ? L" (two steps)" : raise == 1.4f ? L" (half a storey)" : raise == 2.8f ? L" (a storey)" : L"";
    message(L"Raised " + wide(shortNumber(raise, 2)) + L" m" + named + L". Let go to keep it; Esc cancels.");
    return true;
  }
