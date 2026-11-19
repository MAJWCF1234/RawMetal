  // ---- Kits: ready-made rooms, corridors, stairs, docks and yards (NativeKits.inc, made by
  // tools/campaign/kits.py). A kit is a PREFAB: picking it on the Kits tab holds it, its ghost follows
  // the pointer on the plan (and the 3D drag preview shows it for real), R turns it a quarter, a click
  // sets it down as one PREFAB_PLACE (one Ctrl+Z), its block written once above the first map. "Take
  // apart" makes a placed kit's pieces loose, each to move, change or remove on its own; the pieces
  // land where the game drew them (src/world/PrefabTransform.h does both).
#include "NativeKits.inc"
  int kitInHand = -1, kitTurn = 0;
  static int kitIndex(const std::string &key) {
    for (int k = 0; k < KitCount; ++k)
      if (key == kitDefs[k].key)
        return k;
    return -1;
  }
  // A kit's records with each "$name" made that model's slot (ModelIds.h); a model the build lacks throws.
  std::vector<std::string> kitBody(int k) const {
    std::vector<std::string> out;
    std::istringstream lines(kitDefs[k].body);
    for (std::string line; std::getline(lines, line);) {
      if (line.empty())
        continue;
      auto f = NativeDocument::fields(line);
      for (auto &v : f)
        if (v.size() > 1 && v[0] == '$') {
          auto it = std::find_if(models.begin(), models.end(), [&](const auto &m) { return m.second == v.substr(1); });
          if (it == models.end())
            throw std::runtime_error("kit " + std::string(kitDefs[k].key) + " needs the model " + v.substr(1) + ", which this build lacks");
          v = std::to_string(it->first);
        }
      out.push_back(NativeDocument::join(f));
    }
    return out;
  }
  // The records of a document's PREFAB block, as written (empty when there is none).
  static std::vector<std::string> prefabBody(const NativeDocument &d, const std::string &name) {
    std::vector<std::string> out;
    bool in = false;
    for (const auto &raw : d.lines) {
      std::string line = !raw.empty() && raw.back() == '\r' ? raw.substr(0, raw.size() - 1) : raw;
      if (line.starts_with("PREFAB|")) {
        in = decoded(NativeDocument::fields(line)[1]) == name;
        continue;
      }
      if (line.starts_with("PREFAB_END")) {
        if (in)
          return out;
        continue;
      }
      if (in && !line.empty() && line[0] != '#')
        out.push_back(line);
    }
    return out;
  }
  // Writes kit k's PREFAB block above the first MAP of d unless one of its name is there already.
  void ensureKitBlock(NativeDocument &d, int k) const {
    if (!prefabBody(d, kitDefs[k].key).empty())
      return;
    auto body = kitBody(k);
    d.ensureFormat2();
    size_t at = d.lines.size();
    for (size_t i = 0; i < d.lines.size(); ++i)
      if (d.lines[i].starts_with("MAP|")) {
        at = i;
        break;
      }
    const std::string cr = d.crlf ? "\r" : "";
    std::vector<std::string> block{"PREFAB|" + encoded(kitDefs[k].key) + cr};
    for (auto &b : body)
      block.push_back(b + cr);
    block.push_back("PREFAB_END" + cr);
    d.lines.insert(d.lines.begin() + std::ptrdiff_t(at), block.begin(), block.end());
    d.touch();
  }
  // The kit's footprint turned `quarters`, and where its middle may stand so all of it is on the map.
  std::pair<float, float> kitSize(int k, int quarters) const {
    return quarters % 2 ? std::pair{kitDefs[k].d, kitDefs[k].w} : std::pair{kitDefs[k].w, kitDefs[k].d};
  }
  std::pair<float, float> kitSpot(int k, int quarters, float x, float y) const {
    auto [w, d] = kitSize(k, quarters);
    // Soft snapping: the middle on the half metre, the whole kit kept on the map.
    auto fit = [](float v, float half) { return std::clamp(std::round(v * 2) / 2, std::min(12.f, half), std::max(12.f, 24 - half)); };
    return {fit(x, w * .5f), fit(y, d * .5f)};
  }
  // Sets kit k down at (x, y) turned `quarters`: one PREFAB_PLACE (and the block, the first time).
  bool placeKit(int k, float x, float y, int quarters) {
    if (k < 0 || k >= KitCount)
      return false;
    if (effectiveFloorView(floorLayer(layer)).locked) {
      message(L"Unlock the active floor before editing it.");
      return false;
    }
    auto [px, py] = kitSpot(k, quarters, x, y);
    std::vector<std::string> place{"PREFAB_PLACE", std::to_string(map), encoded(kitDefs[k].key), nativeNumber(px), nativeNumber(py),
                                   nativeNumber(layerElevation()), nativeNumber(std::remainder(float(quarters & 3) * 1.57079632679f, 6.28318530718f))};
    remember();
    try {
      ensureKitBlock(doc, k);
    } catch (const std::exception &e) {
      if (!history.empty())
        history.pop_back();
      message(wide(e.what()));
      return false;
    }
    addAuthored(place);
    {
      auto places = doc.records("PREFAB_PLACE", map);
      if (places.empty())
        throw std::runtime_error("the kit's PREFAB_PLACE is not on the map");
      record = int(places.back());
    }
    selectedRecords.clear();
    if (!ghosting) {
      popX = px, popY = py, popAt = GetTickCount64(), popColor = ui::Lamp;
    }
    publish();
    selectRecord(record);
    message(std::wstring(kitDefs[k].title) + L" placed. R turns it; Take apart frees its pieces; Ctrl+Z takes it back.");
    return true;
  }
  // The Kits tab: hold a kit; the next click on the plan sets it down.
  void holdKit(int k) {
    if (tool != 8)
      SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
    kitInHand = k, kitTurn = 0;
    InvalidateRect(plan, nullptr, FALSE);
    message(std::wstring(kitDefs[k].title) + L" (" + kitDefs[k].note + L"): click where it should go. R turns it.");
  }
  void turnKitInHand() {
    kitTurn = (kitTurn + 1) & 3;
    static constexpr const wchar_t *ways[] = {L"south", L"west", L"north", L"east"};
    message(std::wstring(kitDefs[kitInHand].title) + L" turned: its front faces " + ways[kitTurn] + L".");
    InvalidateRect(plan, nullptr, FALSE);
  }
  // A kit or prefab's pieces where a placement at (x, y) turned `yaw` puts them: boxes on the plan.
  std::vector<std::array<float, 4>> prefabOutlines(const std::vector<std::string> &body, float x, float y, float yaw) const {
    std::vector<std::array<float, 4>> out;
    for (auto &line : body) {
      auto g = NativeDocument::fields(line);
      if (g.size() < 2)
        continue;
      g[1] = std::to_string(map);
      try {
        retro::prefab::placeRecord(g, {x, y, 0, yaw});
      } catch (...) {
        continue;
      }
      auto at = [&](size_t i, float d) { return i < g.size() ? number(g[i], d) : d; };
      if (g[0] == "STRUCT")
        out.push_back({at(2, 0), at(3, 0), at(4, 0), at(5, 0)});
      else if (g[0] == "FIXTURE" || g[0] == "PRIM") {
        const bool fixture = g[0] == "FIXTURE";
        float w = std::max(.1f, at(6, 1)), d = std::max(.1f, at(7, 1)), a = fixture ? at(9, 0) : float(int(at(9, 0)) & 1) * 1.5708f;
        const float c = std::abs(std::cos(a)), s = std::abs(std::sin(a));
        const float hw = (w * c + d * s) * .5f, hd = (w * s + d * c) * .5f;
        out.push_back({at(3, 0) - hw, at(4, 0) - hd, at(3, 0) + hw, at(4, 0) + hd});
      } else if (g.size() > 3) {
        const size_t ix = g[0] == "LIGHT" || g[0] == "EMITTER" ? 2 : 3;
        out.push_back({at(ix, 0) - .15f, at(ix + 1, 0) - .15f, at(ix, 0) + .15f, at(ix + 1, 0) + .15f});
      }
    }
    return out;
  }
  // The held kit's ghost under the pointer: its footprint and every piece, where a click puts them.
  void drawKitGhost(HDC dc, float ox, float oy, float cell, float hx, float hy, COLORREF ghost) {
    if (kitInHand < 0 || kitInHand >= KitCount)
      return;
    std::vector<std::string> body;
    try {
      body = kitBody(kitInHand);
    } catch (...) {
      return;
    }
    auto [px, py] = kitSpot(kitInHand, kitTurn, hx, hy);
    auto [w, d] = kitSize(kitInHand, kitTurn);
    auto box = [&](float x0, float y0, float x1, float y1, int width, int style) {
      auto p = CreatePen(style, width, ghost);
      auto o = SelectObject(dc, p), b = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
      Rectangle(dc, int(ox + x0 * cell), int(oy + y0 * cell), int(ox + x1 * cell) + 1, int(oy + y1 * cell) + 1);
      SelectObject(dc, b), SelectObject(dc, o);
      DeleteObject(p);
    };
    for (auto &r : prefabOutlines(body, px, py, float(kitTurn) * 1.57079632679f))
      box(r[0], r[1], r[2], r[3], 1, PS_SOLID);
    box(px - w * .5f, py - d * .5f, px + w * .5f, py + d * .5f, 2, PS_DASH);
    // Its front: a small arrow on the side the doorway faces.
    const float fx[] = {0, -1, 0, 1}, fy[] = {1, 0, -1, 0};
    const float ax = px + fx[kitTurn] * (kitTurn % 2 ? w : d) * .5f, ay = py + fy[kitTurn] * (kitTurn % 2 ? w : d) * .5f;
    auto p = CreatePen(PS_SOLID, 3, ghost);
    auto o = SelectObject(dc, p);
    MoveToEx(dc, int(ox + px * cell), int(oy + py * cell), nullptr);
    LineTo(dc, int(ox + ax * cell), int(oy + ay * cell));
    SelectObject(dc, o);
    DeleteObject(p);
    SetTextColor(dc, ghost);
    SetBkMode(dc, TRANSPARENT);
    std::wstring title = kitDefs[kitInHand].title;
    TextOutW(dc, int(ox + (px - w * .5f) * cell) + 4, int(oy + (py - d * .5f) * cell) - 18, title.c_str(), int(title.size()));
  }
  // Every placed kit and prefab on this map, its pieces in dots, so the plan shows what it holds.
  void drawPlacedPrefabs(HDC dc, float ox, float oy, float cell) {
    auto p = CreatePen(PS_DOT, 1, ui::Lamp);
    auto o = SelectObject(dc, p), b = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    for (auto i : doc.records("PREFAB_PLACE", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 7)
        continue;
      for (auto &r : prefabOutlines(prefabBody(doc, decoded(f[2])), number(f[3], 0), number(f[4], 0), number(f[6], 0)))
        Rectangle(dc, int(ox + r[0] * cell), int(oy + r[1] * cell), int(ox + r[2] * cell) + 1, int(oy + r[3] * cell) + 1);
    }
    SelectObject(dc, b), SelectObject(dc, o);
    DeleteObject(p);
  }
  bool prefabPlacePicked() const {
    return record >= 0 && size_t(record) < doc.lines.size() && doc.lines[size_t(record)].starts_with("PREFAB_PLACE|");
  }
  // Take apart: the picked kit (or prefab) placement becomes its pieces, loose and picked, in one
  // Ctrl+Z; its block goes too once nothing else on the file places it.
  bool takeKitApart() {
    if (!prefabPlacePicked()) {
      message(L"Pick a placed kit first, then Take apart frees its pieces.");
      return false;
    }
    if (!editableRecord(size_t(record))) {
      message(L"Unlock and show this floor before editing its content.");
      return false;
    }
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (f.size() < 7)
      return false;
    const auto name = decoded(f[2]);
    const auto body = prefabBody(doc, name);
    if (body.empty()) {
      message(L"This placement's prefab block is missing.");
      return false;
    }
    const retro::prefab::Placement at{number(f[3], 0), number(f[4], 0), number(f[5], 0), number(f[6], 0)};
    const std::string prefix = f.size() > 7 ? decoded(f[7]) : std::string();
    std::vector<std::vector<std::string>> loose;
    try {
      for (auto &line : body) {
        auto g = NativeDocument::fields(line);
        g[1] = std::to_string(map);
        retro::prefab::placeRecord(g, at);
        for (auto [k, v] : NativeDocument::named(line))
          g.push_back(k + "=" + (k == "name" ? encoded(prefix + decoded(v)) : v));
        loose.push_back(std::move(g));
      }
    } catch (const std::exception &e) {
      message(wide(e.what()));
      return false;
    }
    remember();
    eraseNativeObject(doc, size_t(record));
    bool used = false;
    for (auto i : doc.records("PREFAB_PLACE"))
      used |= decoded(NativeDocument::fields(doc.lines[i])[2]) == name;
    if (!used)
      for (size_t i = 0; i < doc.lines.size(); ++i)
        if (doc.lines[i].starts_with("PREFAB|") && decoded(NativeDocument::fields(doc.lines[i])[1]) == name) {
          size_t j = i;
          while (j < doc.lines.size() && !doc.lines[j].starts_with("PREFAB_END"))
            ++j;
          doc.lines.erase(doc.lines.begin() + std::ptrdiff_t(i), doc.lines.begin() + std::ptrdiff_t(std::min(j + 1, doc.lines.size())));
          doc.touch();
          break;
        }
    auto endMarker = [&] {
      for (size_t i = 0; i < doc.lines.size(); ++i)
        if (doc.lines[i].starts_with("--- CUSTOM_CAMPAIGN_DATA_END ---"))
          return i;
      throw std::runtime_error("Missing native campaign end marker");
    };
    const size_t first = endMarker();
    for (auto &g : loose)
      addAuthored(g);
    const size_t last = endMarker();
    selectedRecords.clear();
    for (size_t i = first; i < last; ++i)
      if (!doc.lines[i].starts_with("#"))
        selectedRecords.push_back(int(i));
    record = selectedRecords.empty() ? -1 : selectedRecords.front();
    publish();
    selectRecord(record, true);
    message(L"Taken apart: " + std::to_wstring(loose.size()) + L" pieces to move, change or remove one by one. Ctrl+Z puts the kit back.");
    return true;
  }
