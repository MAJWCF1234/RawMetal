  // ---- Outdoors: the campaign's terrain, sampled exactly as the engine
  // samples it (src/world/TerrainField.h), so picking, placing, the plan and
  // the 3D guides all stand on the ground the game builds. Then the Terrain
  // brushes (TERRAIN_SCULPT, GROUND), the Nature brush (FIXTURE) and water
  // that finds its own level (WATER, WET).
  // Numbers as a person writes them: millimetres at most, no float noise ("0.7", not "0.69999999").
  static std::string shortNumber(float v, int places = 3) {
    char text[48];
    std::snprintf(text, sizeof text, "%.*f", places, double(v));
    std::string out = text;
    if (out.find('.') != std::string::npos) {
      while (out.back() == '0')
        out.pop_back();
      if (out.back() == '.')
        out.pop_back();
    }
    return out == "-0" ? "0" : out;
  }
  mutable std::uint64_t terrainShapeVersion = ~0ull;
  mutable std::optional<retro::TerrainShape> terrainShapeCache;
  // The TERRAIN, TERRAIN_FLAT, TERRAIN_SCULPT and GROUND records as the engine reads them.
  const retro::TerrainShape *campaignTerrain() const {
    if (terrainShapeVersion == doc.version)
      return terrainShapeCache ? &*terrainShapeCache : nullptr;
    terrainShapeVersion = doc.version;
    terrainShapeCache.reset();
    auto terrain = doc.records("TERRAIN");
    if (terrain.empty())
      return nullptr;
    retro::TerrainShape s;
    auto t = NativeDocument::fields(doc.lines[terrain.front()]);
    if (t.size() < 6)
      return nullptr;
    s.seed = std::uint32_t(std::strtoul(t[1].c_str(), nullptr, 10));
    s.base = number(t[2], 2), s.amplitude = number(t[3], 3), s.scale = number(t[4], 36), s.roughness = number(t[5], .45f);
    for (auto i : doc.records("TERRAIN_FLAT")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() >= 7)
        s.flats.push_back({number(f[1], 0), number(f[2], 0), number(f[3], 0), number(f[4], 0), number(f[5], 0), number(f[6], 0)});
    }
    for (auto i : doc.records("TERRAIN_SCULPT")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 6)
        continue;
      retro::TerrainSculpt sculpt{int(number(f[1], 0)), int(number(f[2], 0)), int(number(f[3], 0)), int(number(f[4], 0)), {}};
      std::string why;
      if (sculpt.cols > 0 && sculpt.rows > 0 && sculpt.cols * sculpt.rows <= retro::TerrainSculptMaxCells &&
          retro::parseTerrainSculptValues(f[5], size_t(sculpt.cols * sculpt.rows), sculpt.offsets, why))
        s.sculpts.push_back(std::move(sculpt));
    }
    for (auto i : doc.records("GROUND")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() >= 7)
        s.ground.push_back({number(f[1], 0), number(f[2], 0), number(f[3], 0), number(f[4], 0), int(number(f[5], 8)), number(f[6], 1)});
    }
    planFootings(&s); // footings measure the land as it is, then cut their pads in (NativeGroundMethods.inc)
    terrainShapeCache = std::move(s);
    return &*terrainShapeCache;
  }
  std::pair<float, float> originOf(int m) const {
    for (auto index : doc.records("MAP", m)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      if (f.size() > 4)
        return {number(f[3], 0), number(f[4], 0)};
    }
    return {float(m) * 24.f, 0.f};
  }
  // A map the engine builds terrain for: outdoors, under a terrain sky, with a TERRAIN record.
  bool terrainMapAt(int m) const {
    auto maps = doc.records("MAP", m);
    if (maps.empty())
      return false;
    auto f = NativeDocument::fields(doc.lines[maps.front()]);
    return f.size() > 11 && f[8] == "1" && retro::terrainSkybox(decoded(f[11])) && campaignTerrain();
  }
  bool onTerrain() const { return terrainMapAt(map); }
  // The walked ground at map metres (x, y) on this map (the floor elsewhere).
  float groundAt(float x, float y) const {
    if (!onTerrain())
      return layerElevation();
    // Inside a footing, the ground is its floor (the engine's World::floorHeight).
    if (const auto *part = footingPartAt(map, x, y))
      return part->surface;
    auto [ox, oy] = mapOrigin();
    return retro::authoredTerrainSurface(*campaignTerrain(), ox, oy, x, y);
  }
  float groundHigh(float x1, float y1, float x2, float y2, bool high) const {
    float best = high ? -1e9f : 1e9f;
    int nx = std::clamp(int(std::ceil((x2 - x1) * 2)), 1, 12), ny = std::clamp(int(std::ceil((y2 - y1) * 2)), 1, 12);
    for (int j = 0; j <= ny; ++j)
      for (int i = 0; i <= nx; ++i) {
        float z = groundAt(x1 + (x2 - x1) * float(i) / nx, y1 + (y2 - y1) * float(j) / ny);
        best = high ? std::max(best, z) : std::min(best, z);
      }
    return best;
  }
  // Heights that follow the ground outdoors (World::settleExclusionFacilityOnTerrain):
  // a box rises by the ground under its footprint, everything else by the ground at its spot.
  static bool groundRelativeKind(const std::string &k) {
    static const std::set<std::string> kinds = {"STRUCT", "PRIM", "STAIR", "BREAKABLE", "FIXTURE", "CLUTTER", "HAZARD", "EMITTER",
                                                "CREATURE", "PICKUP", "SIGN", "LEAD", "LIGHT", "TERMINAL", "DOOR", "PROP",
                                                "ACTOR_KEY", "ACTOR_IDLE_KEY", "SHELTER", "HUNG_SUIT", "NAV_HINT", "TIMED_SEQUENCE"};
    return kinds.count(k) != 0;
  }
  std::optional<creation::Box> worldBox(const std::vector<std::string> &f) const {
    auto box = creationBox(f);
    if (!box || f.empty() || !onTerrain() || !groundRelativeKind(f[0]))
      return box;
    if (f[0] == "STRUCT" || f[0] == "BREAKABLE") {
      box->bottom += groundHigh(box->x - box->w * .5f, box->y - box->d * .5f, box->x + box->w * .5f, box->y + box->d * .5f, false);
      box->top += groundHigh(box->x - box->w * .5f, box->y - box->d * .5f, box->x + box->w * .5f, box->y + box->d * .5f, true);
    } else {
      float z = groundAt(box->x, box->y);
      box->bottom += z, box->top += z;
    }
    return box;
  }
  float liftOf(const std::vector<std::string> &f) const {
    auto plain = creationBox(f), lifted = worldBox(f);
    return plain && lifted ? lifted->bottom - plain->bottom : 0.f;
  }
  // Where a ray from the 3D view meets this map's ground (false: it misses).
  bool terrainRay(float ox, float oy, float oz, float dx, float dy, float dz, float &x, float &y, float &z) const {
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f || !onTerrain())
      return false;
    dx /= len, dy /= len, dz /= len;
    float previous = -1;
    for (float t = .05f; t < 160.f; t += .12f) {
      float px = ox + dx * t, py = oy + dy * t, pz = oz + dz * t;
      if (px < 0 || py < 0 || px >= 24 || py >= 24) {
        previous = -1;
        continue;
      }
      if (pz <= groundAt(px, py)) {
        float a = previous < 0 ? t - .12f : previous, b = t;
        for (int i = 0; i < 18; ++i) {
          float m = (a + b) * .5f;
          float mx = ox + dx * m, my = oy + dy * m;
          if (mx >= 0 && my >= 0 && mx < 24 && my < 24 && oz + dz * m <= groundAt(mx, my))
            b = m;
          else
            a = m;
        }
        x = ox + dx * b, y = oy + dy * b, z = oz + dz * b;
        return true;
      }
      previous = t;
    }
    return false;
  }
  // The floor point under a 3D pick: the ground on a terrain map's ground floor, the floor plane elsewhere.
  bool pointOnFloor(float ox, float oy, float oz, float dx, float dy, float dz, float plane, float &x, float &y) const {
    float z = 0;
    if (layer == 0 && terrainRay(ox, oy, oz, dx, dy, dz, x, y, z))
      return true;
    return floorPoint(ox, oy, oz, dx, dy, dz, plane, x, y);
  }
  // How far along a ray the ground hides what lies behind it.
  float terrainOcclusion(float ox, float oy, float oz, float dx, float dy, float dz) const {
    float x, y, z;
    if (!terrainRay(ox, oy, oz, dx, dy, dz, x, y, z))
      return 1e9f;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    return std::sqrt((x - ox) * (x - ox) + (y - oy) * (y - oy) + (z - oz) * (z - oz)) / std::max(len, 1e-5f);
  }

  // ---- Terrain tool: raise, lower, smooth, flatten, ramp and paint ground.
  enum TerrainMode { TerrainRaise, TerrainLower, TerrainSmooth, TerrainFlatten, TerrainRamp, TerrainPaint };
  HWND terrainModeList = nullptr, terrainSizeList = nullptr, terrainStrengthList = nullptr, terrainHeightBox = nullptr;
  static constexpr float terrainSizes[] = {1.f, 2.f, 3.f, 5.f, 8.f, 12.f};
  static constexpr float terrainStrengths[] = {.1f, .25f, .5f, 1.f};
  int terrainMode() const { return std::clamp(listIndex(terrainModeList), 0, 5); }
  float brushRadius() const { return terrainSizes[std::clamp(listIndex(terrainSizeList, 2), 0, int(std::size(terrainSizes)) - 1)]; }
  float brushStrength() const { return terrainStrengths[std::clamp(listIndex(terrainStrengthList, 1), 0, int(std::size(terrainStrengths)) - 1)]; }
  // The stroke in hand: the sculpt grids it touches (24 x 24 metres each, aligned to this map),
  // where it began, the last dab, and for ground paint the cells it has covered.
  struct OutdoorStroke {
    bool active = false;
    char tool = 0;
    float startX = 0, startY = 0, lastX = -1e9f, lastY = -1e9f, startHeight = 0, endHeight = 0;
    std::map<std::pair<int, int>, std::vector<float>> grids, original;
    std::set<std::pair<int, int>> painted;
    size_t paintLines = 0;
    std::vector<std::pair<float, float>> plants;
    unsigned seed = 1;
    int planted = 0, erased = 0;
  } stroke;
  std::pair<int, int> sculptBlock(int wx, int wy) const {
    auto [ox, oy] = mapOrigin();
    int bx = int(std::floor((wx - ox) / 24.f)), by = int(std::floor((wy - oy) / 24.f));
    return {int(ox) + bx * 24, int(oy) + by * 24};
  }
  int sculptRecord(int x, int y) const {
    for (auto i : doc.records("TERRAIN_SCULPT")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() >= 6 && int(number(f[1], 1e9f)) == x && int(number(f[2], 1e9f)) == y && f[3] == "24" && f[4] == "24")
        return int(i);
    }
    return -1;
  }
  std::vector<float> &strokeGrid(int wx, int wy) {
    auto key = sculptBlock(wx, wy);
    auto found = stroke.grids.find(key);
    if (found != stroke.grids.end())
      return found->second;
    std::vector<float> grid(576, 0.f);
    int at = sculptRecord(key.first, key.second);
    if (at >= 0) {
      std::string why;
      retro::parseTerrainSculptValues(NativeDocument::fields(doc.lines[size_t(at)])[5], 576, grid, why);
      if (grid.size() != 576)
        grid.assign(576, 0.f);
    }
    stroke.original[key] = grid;
    return stroke.grids[key] = grid;
  }
  float &strokeOffset(int wx, int wy) {
    auto key = sculptBlock(wx, wy);
    return strokeGrid(wx, wy)[size_t((wy - key.second) * 24 + (wx - key.first))];
  }
  // The grids back into the document: one TERRAIN_SCULPT a block, none where it is all zero.
  void writeSculptGrids() {
    for (auto &[key, grid] : stroke.grids) {
      int at = sculptRecord(key.first, key.second);
      bool flat = std::all_of(grid.begin(), grid.end(), [](float v) { return std::lround(v * 10) == 0; });
      std::vector<std::string> f{"TERRAIN_SCULPT", std::to_string(key.first), std::to_string(key.second), "24", "24",
                                 retro::formatTerrainSculptValues(grid)};
      if (flat && at >= 0)
        doc.eraseRecord(size_t(at));
      else if (!flat && at >= 0) {
        if (doc.lines[size_t(at)] != NativeDocument::join(f) + (doc.lines[size_t(at)].ends_with('\r') ? "\r" : ""))
          doc.replace(size_t(at), f);
      } else if (!flat)
        doc.add(f);
    }
  }
  // The unsculpted height of a world metre cell, and with this stroke's offsets.
  float baseHeight(int wx, int wy) const { return retro::authoredTerrainBaseHeight(*campaignTerrain(), wx + .5f, wy + .5f); }
  float strokeHeight(int wx, int wy) { return baseHeight(wx, wy) + strokeOffset(wx, wy); }
  void beginOutdoorStroke(float x, float y, char tool) {
    stroke = OutdoorStroke{};
    stroke.active = true, stroke.tool = tool, stroke.startX = x, stroke.startY = y;
    stroke.seed = unsigned(GetTickCount64() & 0xffffffu) ^ unsigned(changeCount * 2654435761u);
    if (backgroundTest)
      stroke.seed = unsigned(changeCount + 17);
  }
  // A stroke of the Terrain tool: one dab under the pointer, spaced along the drag.
  void terrainDab(float x, float y) {
    if (effectiveFloorView(floorLayer(layer)).locked)
      return;
    bool first = !stroke.active || stroke.tool != 'T';
    if (first) {
      bool opened = prepareOutdoors();
      if (opened)
        message(L"The map is outdoors now: open sky and land to shape.");
      beginOutdoorStroke(x, y, 'T');
      stroke.paintLines = 0;
    }
    auto shape = campaignTerrain();
    if (!shape || !onTerrain()) {
      message(L"Choose a terrain sky (Map sky) to shape the land here.");
      return;
    }
    const float r = brushRadius(), strength = brushStrength();
    const int mode = terrainMode();
    if (!first && mode != TerrainRamp && std::hypot(x - stroke.lastX, y - stroke.lastY) < std::max(.5f, r * .35f))
      return;
    stroke.lastX = x, stroke.lastY = y;
    auto [ox, oy] = mapOrigin();
    float wx = ox + x, wy = oy + y;
    if (first) {
      // The level under the press: its voxel top for Flatten (the ground as it shows), the
      // smooth height for a ramp's ends.
      float h = strokeHeight(int(std::floor(wx)), int(std::floor(wy)));
      stroke.startHeight = mode == TerrainRamp ? std::round(h * 10) / 10 : std::round(h);
      std::wstring typed(32, L'\0');
      if (terrainHeightBox) {
        typed.resize(size_t(GetWindowTextW(terrainHeightBox, typed.data(), 32)));
        wchar_t *end = nullptr;
        double value = wcstod(typed.c_str(), &end);
        if (mode == TerrainFlatten && end != typed.c_str())
          stroke.startHeight = float(value);
      }
    }
    if (mode == TerrainPaint) {
      paintGroundDab(wx, wy, r);
    } else if (mode == TerrainRamp) {
      // A ramp is laid from the stroke's start every time it moves: the grids go back first,
      // and its far end is read from the ground as it was.
      stroke.grids = stroke.original;
      stroke.endHeight = std::round(strokeHeight(int(std::floor(wx)), int(std::floor(wy))) * 10) / 10;
      float ax = ox + stroke.startX, ay = oy + stroke.startY, bx = wx, by = wy;
      float length = std::hypot(bx - ax, by - ay);
      if (length > .5f) {
        float ux = (bx - ax) / length, uy = (by - ay) / length;
        int x0 = int(std::floor(std::min(ax, bx) - r - 1)), x1 = int(std::ceil(std::max(ax, bx) + r + 1));
        int y0 = int(std::floor(std::min(ay, by) - r - 1)), y1 = int(std::ceil(std::max(ay, by) + r + 1));
        for (int cy = y0; cy <= y1; ++cy)
          for (int cx = x0; cx <= x1; ++cx) {
            float px = cx + .5f - ax, py = cy + .5f - ay, along = px * ux + py * uy, side = std::abs(px * uy - py * ux);
            if (along < -r * .5f || along > length + r * .5f || side > r)
              continue;
            float t = std::clamp(along / length, 0.f, 1.f), target = stroke.startHeight + (stroke.endHeight - stroke.startHeight) * t;
            float w = std::clamp((r - side) / std::max(.5f, r * .5f), 0.f, 1.f);
            if (along < 0 || along > length)
              w *= std::clamp(1 - std::abs(along < 0 ? along : along - length) / std::max(.5f, r * .5f), 0.f, 1.f);
            w = w * w * (3 - 2 * w);
            float &off = strokeOffset(cx, cy);
            float h = baseHeight(cx, cy) + off;
            off = std::clamp(off + (target - h) * w, -retro::TerrainSculptMaxOffset, retro::TerrainSculptMaxOffset);
          }
      }
      writeSculptGrids();
    } else {
      int x0 = int(std::floor(wx - r)), x1 = int(std::ceil(wx + r)), y0 = int(std::floor(wy - r)), y1 = int(std::ceil(wy + r));
      std::map<std::pair<int, int>, float> heights;
      if (mode == TerrainSmooth)
        for (int cy = y0 - 1; cy <= y1 + 1; ++cy)
          for (int cx = x0 - 1; cx <= x1 + 1; ++cx)
            heights[{cx, cy}] = strokeHeight(cx, cy);
      for (int cy = y0; cy <= y1; ++cy)
        for (int cx = x0; cx <= x1; ++cx) {
          float d = std::hypot(cx + .5f - wx, cy + .5f - wy);
          if (d > r)
            continue;
          float w = std::clamp(1 - d / r, 0.f, 1.f);
          w = r <= 1.01f ? 1.f : w * w * (3 - 2 * w);
          float &off = strokeOffset(cx, cy);
          float h = baseHeight(cx, cy) + off, change = 0;
          if (mode == TerrainRaise)
            change = strength * w;
          else if (mode == TerrainLower)
            change = -strength * w;
          else if (mode == TerrainFlatten)
            change = (stroke.startHeight - h) * std::min(1.f, w * strength * 2.f + (strength >= 1 ? 1.f : 0.f));
          else if (mode == TerrainSmooth) {
            float sum = 0;
            for (int j = -1; j <= 1; ++j)
              for (int i = -1; i <= 1; ++i)
                sum += heights[{cx + i, cy + j}];
            change = (sum / 9.f - h) * std::min(1.f, w * (.35f + strength));
          }
          float top = float(retro::TerrainFieldHighestTop) - .5f, low = float(retro::TerrainFieldLowestTop) + .5f;
          off = std::clamp(off + std::clamp(h + change, low, top) - h, -retro::TerrainSculptMaxOffset, retro::TerrainSculptMaxOffset);
        }
      writeSculptGrids();
    }
    ++changeCount;
    if (!quiet())
      sfx::play(mode == TerrainLower ? sfx::BreakStone : sfx::PlaceStone, .45f, .15f);
    if (liveTurn(150)) {
      publish();
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }
  // Ground paint: the cells the stroke has covered, written as the fewest GROUND boxes.
  void paintGroundDab(float wx, float wy, float r) {
    for (int cy = int(std::floor(wy - r)); cy <= int(std::ceil(wy + r)); ++cy)
      for (int cx = int(std::floor(wx - r)); cx <= int(std::ceil(wx + r)); ++cx)
        if (std::hypot(cx + .5f - wx, cy + .5f - wy) <= std::max(.6f, r))
          stroke.painted.insert({cx, cy});
    // This stroke's boxes are the last GROUND lines; they are written again each dab.
    auto ground = doc.records("GROUND");
    for (size_t k = 0; k < stroke.paintLines && !ground.empty(); ++k) {
      doc.eraseRecord(ground.back());
      ground.pop_back();
    }
    auto rects = mergeCells(stroke.painted);
    int kind = std::clamp(groundKind, 0, 8);
    for (auto &q : rects)
      doc.add({"GROUND", shortNumber(float(q[0])), shortNumber(float(q[1])), shortNumber(float(q[2])), shortNumber(float(q[3])),
               std::to_string(kind), r >= 2 ? "0.8" : "0.4"});
    stroke.paintLines = rects.size();
  }
  // Cells as boxes: runs along each row, stacked down while the run stays the same.
  static std::vector<std::array<int, 4>> mergeCells(const std::set<std::pair<int, int>> &cells) {
    std::map<int, std::vector<std::pair<int, int>>> rows; // y -> runs [x0, x1)
    std::set<std::pair<int, int>> byRow;
    for (auto [x, y] : cells)
      byRow.insert({y, x});
    for (auto it = byRow.begin(); it != byRow.end();) {
      int y = it->first, x0 = it->second, x1 = x0 + 1;
      ++it;
      while (it != byRow.end() && it->first == y && it->second == x1)
        ++x1, ++it;
      rows[y].push_back({x0, x1});
    }
    std::vector<std::array<int, 4>> out;
    std::map<std::pair<int, int>, size_t> open; // run -> box still growing down
    int lastY = INT_MIN;
    for (auto &[y, runs] : rows) {
      std::map<std::pair<int, int>, size_t> next;
      for (auto run : runs) {
        auto found = open.find(run);
        if (found != open.end() && lastY == y - 1) {
          out[found->second][3] = y + 1;
          next[run] = found->second;
        } else {
          out.push_back({run.first, y, run.second, y + 1});
          next[run] = out.size() - 1;
        }
      }
      open = std::move(next);
      lastY = y;
    }
    return out;
  }
  // The stroke is over: say what it did.
  void endOutdoorStroke() {
    if (!stroke.active)
      return;
    char tool = stroke.tool;
    stroke.active = false;
    if (!history.empty() && history.back() == doc.text())
      history.pop_back(); // nothing changed: no empty Undo
    if (tool == 'T') {
      const wchar_t *done[] = {L"Raised the ground.", L"Lowered the ground.", L"Smoothed the ground.", L"Flattened the ground.",
                               L"Laid a ramp.", L"Painted the ground."};
      std::wstring note = done[terrainMode()];
      if (terrainMode() == TerrainFlatten)
        note += L" Level " + wide(shortNumber(stroke.startHeight)) + L" m.";
      if (terrainMode() == TerrainRamp)
        note += L" From " + wide(shortNumber(stroke.startHeight)) + L" m to " + wide(shortNumber(stroke.endHeight)) + L" m.";
      message(note + L" Ctrl+Z takes the stroke back.");
    } else if (tool == 'N')
      message(stroke.erased ? L"Cleared " + std::to_wstring(stroke.erased) + L" plants." : L"Planted " + std::to_wstring(stroke.planted) + L".");
    stroke.grids.clear(), stroke.original.clear(), stroke.painted.clear(), stroke.plants.clear();
  }

  // ---- Nature tool: trees, bushes, grass, rocks and plants from the purchased nature pack.
  struct NatureSet {
    const wchar_t *name;
    std::vector<std::pair<int, float>> models; // slot, weight
    float spacing, scaleLow, scaleHigh;
    bool solid;
    float core;
    bool upright; // trees stand straight up; the rest lie along the slope
  };
  static const std::vector<NatureSet> &natureSets() {
    using namespace retro::model;
    static const std::vector<NatureSet> sets = {
        {L"Trees", {{pine_a, 4}, {pine_b, 4}, {oak_a, 2}, {oak_b, 2}, {oak_c, 1}, {leafy_a, 3}, {leafy_b, 3}, {aspen_a, 1.5f}, {aspen_b, 1.5f}, {dead_tree_b, .6f}},
         4.5f, .75f, 1.15f, true, .1f, true},
        {L"Bushes", {{bush_a, 2}, {bush_b, 2}, {bush_c, 2}, {bush_d, 1}, {sumac, 2}, {grass_bush, 1}}, 1.6f, .8f, 1.25f, false, 1, false},
        {L"Grass", {{grass_a, 2}, {grass_b, 2}, {grass_c, 2}, {grass_l, 3}, {tall_grass_l, 3}, {tall_grass_m, 3}, {clover_patch, 1}, {grass_patch, 1}},
         1.f, .8f, 1.3f, false, 1, false},
        {L"Rocks", {{rock_mossy, 3}, {rock_small, 2}, {rock_mid, 1}, {rock_big, .6f}, {stump, .5f}, {retro::model::log, .4f}}, 3.f, .6f, 1.05f, true, 1, false},
        {L"Plants & ferns", {{fern_a, 3}, {fern_big, 1}, {bracken, 3}, {goldenrod, 2}, {aster, 2}, {dogbane, 1}, {mushrooms, .5f}}, 1.3f, .8f, 1.25f, false, 1, false},
        {L"Woodland mix", {{pine_a, 1.2f}, {leafy_a, 1}, {oak_b, .8f}, {bush_a, 2}, {bush_c, 2}, {fern_a, 3}, {bracken, 3}, {grass_l, 3}, {rock_mossy, .7f}},
         1.6f, .75f, 1.2f, false, 1, false},
    };
    return sets;
  }
  static bool natureModel(int slot) {
    for (const auto &set : natureSets())
      for (auto [m, w] : set.models)
        if (m == slot)
          return true;
    return slot == retro::model::fallen_pine || slot == retro::model::sapling || slot == retro::model::dead_tree_a;
  }
  static bool treeModel(int slot) {
    using namespace retro::model;
    for (int t : {pine_a, pine_b, oak_a, oak_b, oak_c, leafy_a, leafy_b, aspen_a, aspen_b, dead_tree_a, dead_tree_b, sapling})
      if (slot == t)
        return true;
    return false;
  }
  HWND natureSetList = nullptr, natureDensityList = nullptr, natureScaleList = nullptr, natureModeList = nullptr;
  bool natureAlign = true;
  // Where nothing grows: walls, blocks, shapes, stairs, doors, furniture that is not itself a plant, and water.
  // (Water counts as a box with no turn; a neighbour's collision copy of a building blocks too.)
  mutable std::uint64_t natureObstacleVersion = ~0ull;
  mutable int natureObstacleMap = -1;
  mutable std::vector<creation::Box> natureObstacles;
  bool natureBlocked(float x, float y, float clearance) const {
    if (x < .25f || y < .25f || x > 23.75f || y > 23.75f)
      return true;
    if (doc.tile(map, layer, int(x), int(y)) == '#')
      return true;
    if (natureObstacleVersion != doc.version || natureObstacleMap != map) {
      natureObstacleVersion = doc.version, natureObstacleMap = map;
      natureObstacles.clear();
      for (auto i : doc.onMap(map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        const auto &k = f[0];
        if (k == "WATER" && f.size() >= 6) {
          float x1 = number(f[2], 0), y1 = number(f[3], 0), x2 = number(f[4], 0), y2 = number(f[5], 0);
          natureObstacles.push_back({(x1 + x2) * .5f, (y1 + y2) * .5f, std::abs(x2 - x1) + .6f, std::abs(y2 - y1) + .6f, 0, 1, 0});
          continue;
        }
        if (k == "FIXTURE" && f.size() > 2 && natureModel(int(number(f[2], -1))))
          continue;
        if (k != "STRUCT" && k != "PRIM" && k != "STAIR" && k != "DOOR" && k != "FIXTURE" && k != "TERMINAL" && k != "BREAKABLE" && k != "COMPACTOR")
          continue;
        if (auto box = creationBox(f))
          natureObstacles.push_back(*box);
      }
    }
    for (const auto &box : natureObstacles) {
      float c = std::cos(box.yaw), s = std::sin(box.yaw), vx = x - box.x, vy = y - box.y;
      float px = vx * c - vy * s, py = vx * s + vy * c;
      if (std::abs(px) <= box.w * .5f + clearance && std::abs(py) <= box.d * .5f + clearance)
        return true;
    }
    return false;
  }
  void natureDab(float x, float y) {
    if (effectiveFloorView(floorLayer(layer)).locked)
      return;
    bool first = !stroke.active || stroke.tool != 'N';
    if (first) {
      beginOutdoorStroke(x, y, 'N');
      for (auto i : doc.records("FIXTURE", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (f.size() > 4 && natureModel(int(number(f[2], -1))))
          stroke.plants.push_back({number(f[3], 0), number(f[4], 0)});
      }
    }
    const float r = brushRadius();
    if (!first && std::hypot(x - stroke.lastX, y - stroke.lastY) < std::max(.6f, r * .5f))
      return;
    stroke.lastX = x, stroke.lastY = y;
    bool erase = listIndex(natureModeList) == 1 || (!backgroundTest && (GetKeyState(VK_MENU) & 0x8000));
    if (erase) {
      std::vector<size_t> victims;
      for (auto i : doc.records("FIXTURE", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (f.size() > 4 && natureModel(int(number(f[2], -1))) && std::hypot(number(f[3], 0) - x, number(f[4], 0) - y) <= r)
          victims.push_back(i);
      }
      // Their collision copies in the neighbouring maps go with them.
      auto [ox, oy] = mapOrigin();
      for (size_t v : std::vector<size_t>(victims)) {
        auto f = NativeDocument::fields(doc.lines[v]);
        float wx = ox + number(f[3], 0), wy = oy + number(f[4], 0);
        for (auto i : doc.records("FIXTURE")) {
          auto g = NativeDocument::fields(doc.lines[i]);
          if (g.size() > 12 && g[12] == "0" && g[2] == f[2] && g[1] != f[1]) {
            auto [nx, ny] = originOf(int(number(g[1], -1)));
            if (std::abs(nx + number(g[3], 0) - wx) < .02f && std::abs(ny + number(g[4], 0) - wy) < .02f)
              victims.push_back(i);
          }
        }
      }
      std::sort(victims.begin(), victims.end());
      victims.erase(std::unique(victims.begin(), victims.end()), victims.end());
      for (auto it = victims.rbegin(); it != victims.rend(); ++it)
        eraseNativeObject(doc, *it);
      stroke.erased += int(victims.size());
      std::erase_if(stroke.plants, [&](auto p) { return std::hypot(p.first - x, p.second - y) <= r; });
    } else {
      const auto &set = natureSets()[size_t(std::clamp(listIndex(natureSetList), 0, int(natureSets().size()) - 1))];
      static constexpr float density[] = {1.7f, 1.f, .65f};
      static constexpr float spread[] = {0.f, .5f, 1.f};
      float spacing = set.spacing * density[std::clamp(listIndex(natureDensityList, 1), 0, 2)];
      float variety = spread[std::clamp(listIndex(natureScaleList, 1), 0, 2)];
      std::mt19937 random(stroke.seed + unsigned(stroke.planted * 7919 + int(x * 131) + int(y * 977)));
      std::uniform_real_distribution<float> unit(0.f, 1.f);
      float total = 0;
      for (auto [m, w] : set.models)
        total += w;
      int wanted = std::clamp(int(3.14159f * r * r / (spacing * spacing)), 1, 150), tries = std::max(2, int(wanted * 2.2f));
      bool terrain = onTerrain();
      float floorBase = layerElevation() - bottomLayerElevation();
      for (int attempt = 0, made = 0; attempt < tries && made < wanted; ++attempt) {
        float a = unit(random) * 6.2831853f, d = r * std::sqrt(unit(random));
        float px = x + std::cos(a) * d, py = y + std::sin(a) * d;
        float pick = unit(random) * total;
        int slot = set.models.back().first;
        for (auto [m, w] : set.models)
          if ((pick -= w) <= 0) {
            slot = m;
            break;
          }
        bool tree = treeModel(slot);
        float own = tree ? std::max(spacing, 3.5f) : spacing;
        if (natureBlocked(px, py, tree ? 1.f : .15f))
          continue;
        if (std::any_of(stroke.plants.begin(), stroke.plants.end(), [&](auto p) { return std::hypot(p.first - px, p.second - py) < own * .8f; }))
          continue;
        float gx = 0, gy = 0;
        if (terrain) {
          gx = (groundAt(std::min(px + .5f, 23.99f), py) - groundAt(std::max(px - .5f, 0.f), py));
          gy = (groundAt(px, std::min(py + .5f, 23.99f)) - groundAt(px, std::max(py - .5f, 0.f)));
          if (tree && std::hypot(gx, gy) > .9f)
            continue; // trees do not grow on a cliff
        }
        auto size = retro::model::size[size_t(slot)];
        float mid = (set.scaleLow + set.scaleHigh) * .5f, half = (set.scaleHigh - set.scaleLow) * .5f * variety;
        float scale = mid + (unit(random) * 2 - 1) * half;
        float w = std::max(.05f, size[0] * scale), dd = std::max(.05f, size[1] * scale), h = std::max(.05f, size[2] * scale);
        float yaw = unit(random) * 6.2831853f, base = floorBase;
        std::vector<std::string> f{"FIXTURE", std::to_string(map), std::to_string(slot), shortNumber(px), shortNumber(py), "0",
                                   shortNumber(w), shortNumber(dd), shortNumber(h), shortNumber(yaw), tree || (set.solid && slot != retro::model::rock_small) ? "1" : "0",
                                   shortNumber(tree ? set.core : 1.f)};
        if (natureAlign && terrain) {
          float slope = std::hypot(gx, gy), reach = std::max(w, dd) * .5f;
          if (tree || set.upright)
            base -= std::min(.6f, slope * reach * .5f); // roots into the hillside, trunk upright
          else if (slope > .02f) {
            f.insert(f.end(), {"1", "0", shortNumber(std::clamp(gx, -2.f, 2.f)), shortNumber(std::clamp(gy, -2.f, 2.f))});
            base -= .03f;
          }
        }
        f[5] = shortNumber(base);
        addAuthored(f);
        natureObstacleVersion = doc.version; // a plant is no obstacle: the walls and water stand as they were
        stroke.plants.push_back({px, py});
        ++stroke.planted, ++made;
      }
    }
    ++changeCount;
    if (!quiet())
      sfx::play(erase ? sfx::BreakStone : sfx::PlaceStone, .35f, .2f);
    if (liveTurn(150)) {
      publish();
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }

  // ---- Water: pools that find their level on the land.
  HWND waterModeList = nullptr, waterMurkSlider = nullptr, waterWetSlider = nullptr;
  int waterMode() const { return std::clamp(listIndex(waterModeList), 0, 2); }
  // A dragged pool outdoors: its surface just under the lowest point of its bank, its bed under the ground.
  void groundWater(std::vector<std::string> &f) {
    if (!onTerrain() || f.size() < 8)
      return;
    float x1 = number(f[2], 0), y1 = number(f[3], 0), x2 = number(f[4], 0), y2 = number(f[5], 0);
    float bank = 1e9f;
    for (float t = 0; t <= 1.0001f; t += .05f) {
      float sx = x1 + (x2 - x1) * t, sy = y1 + (y2 - y1) * t;
      for (auto [px, py] : {std::pair{sx, y1}, std::pair{sx, y2}, std::pair{x1, sy}, std::pair{x2, sy}})
        bank = std::min(bank, groundAt(std::clamp(px, 0.f, 23.99f), std::clamp(py, 0.f, 23.99f)));
    }
    float floor = groundHigh(x1, y1, x2, y2, false);
    float surface = bank - .1f > floor + .15f ? bank - .1f : floor + .4f;
    f[6] = shortNumber(std::round((floor - .6f) * 100) / 100), f[7] = shortNumber(std::round(surface * 100) / 100);
  }
  // Fill the hollow under a click: the water rises until it would spill out, and covers every
  // metre below that level that the click's metre reaches.
  int fillBasin(float x, float y) {
    if (!onTerrain()) {
      message(L"Fill a hollow works on land: give this map a terrain sky first.");
      return 0;
    }
    float h[24][24];
    for (int cy = 0; cy < 24; ++cy)
      for (int cx = 0; cx < 24; ++cx)
        h[cy][cx] = groundAt(cx + .5f, cy + .5f);
    int sx = std::clamp(int(x), 0, 23), sy = std::clamp(int(y), 0, 23);
    using Cell = std::pair<float, int>;
    std::priority_queue<Cell, std::vector<Cell>, std::greater<Cell>> open;
    std::array<bool, 576> seen{};
    open.push({h[sy][sx], sy * 24 + sx});
    seen[size_t(sy * 24 + sx)] = true;
    float level = h[sy][sx], spill = 1e9f;
    while (!open.empty()) {
      auto [height, at] = open.top();
      open.pop();
      level = std::max(level, height);
      int cx = at % 24, cy = at / 24;
      if (cx == 0 || cy == 0 || cx == 23 || cy == 23) {
        spill = level;
        break;
      }
      for (auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
        int nx = cx + dx, ny = cy + dy, n = ny * 24 + nx;
        if (!seen[size_t(n)])
          seen[size_t(n)] = true, open.push({h[ny][nx], n});
      }
    }
    if (spill > 1e8f)
      spill = level;
    float surface = spill - .15f;
    if (surface < h[sy][sx] + .1f) {
      message(L"No hollow here: water would run straight off. Lower the ground first (Terrain, Lower).");
      return 0;
    }
    std::set<std::pair<int, int>> cells;
    std::vector<int> todo{sy * 24 + sx};
    std::array<bool, 576> in{};
    in[size_t(todo[0])] = true;
    while (!todo.empty()) {
      int at = todo.back();
      todo.pop_back();
      int cx = at % 24, cy = at / 24;
      cells.insert({cx, cy});
      for (auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
        int nx = cx + dx, ny = cy + dy;
        if (nx < 0 || ny < 0 || nx > 23 || ny > 23 || in[size_t(ny * 24 + nx)] || h[ny][nx] >= surface)
          continue;
        in[size_t(ny * 24 + nx)] = true;
        todo.push_back(ny * 24 + nx);
      }
    }
    remember();
    float murk = 0;
    if (record >= 0 && size_t(record) < doc.lines.size()) {
      auto picked = NativeDocument::fields(doc.lines[size_t(record)]);
      if (picked[0] == "WATER" && picked.size() > 8)
        murk = number(picked[8], 0);
    }
    int first = -1, made = 0;
    for (auto &q : mergeCells(cells)) {
      float low = 1e9f;
      for (int cy = q[1]; cy < q[3]; ++cy)
        for (int cx = q[0]; cx < q[2]; ++cx)
          low = std::min(low, h[cy][cx]);
      std::vector<std::string> f{"WATER", std::to_string(map), std::to_string(q[0]), std::to_string(q[1]), std::to_string(q[2]), std::to_string(q[3]),
                                 shortNumber(std::round((low - .8f) * 100) / 100), shortNumber(std::round(surface * 100) / 100)};
      if (murk > 0)
        f.push_back(shortNumber(murk));
      addAuthored(f);
      if (first < 0)
        first = int(doc.records("WATER", map).back());
      ++made;
    }
    ++changeCount;
    publish();
    if (first >= 0)
      selectRecord(first);
    message(L"Filled the hollow: " + std::to_wstring(cells.size()) + L" square metres at " + wide(shortNumber(std::round(surface * 100) / 100)) +
            L" m, in " + std::to_wstring(made) + L" water boxes.");
    return made;
  }
  // The picked water's surface where the bank was clicked: the ground there, every box of that pool.
  bool waterSurfaceFromBank(float x, float y, float z = -1e9f) {
    if (record < 0 || size_t(record) >= doc.lines.size() || NativeDocument::fields(doc.lines[size_t(record)])[0] != "WATER") {
      message(L"Pick a pool of water first, then click its bank.");
      return false;
    }
    float level = z > -1e8f ? z : groundAt(std::clamp(x, 0.f, 23.99f), std::clamp(y, 0.f, 23.99f));
    auto picked = NativeDocument::fields(doc.lines[size_t(record)]);
    remember();
    for (auto i : doc.records("WATER", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      // The boxes of one pool share its surface.
      if (i != size_t(record) && (f.size() < 8 || f[7] != picked[7]))
        continue;
      f[7] = shortNumber(std::round(level * 100) / 100);
      if (number(f[6], 0) > level - .2f)
        f[6] = shortNumber(std::round((level - .6f) * 100) / 100);
      doc.replace(i, f);
    }
    ++changeCount;
    publish();
    selectRecord(record, true);
    message(L"Water surface set to the bank: " + wide(shortNumber(std::round(level * 100) / 100)) + L" m.");
    return true;
  }
  // The tool boxes: a new brush shows its own rows; Align to slope turns over.
  void outdoorControlChanged(int id, int notification) {
    if (id == NatureAlignToggle) {
      natureAlign = !natureAlign;
      message(natureAlign ? L"Plants lie along the slope; trees stand upright with their roots in the hill."
                          : L"Plants stand level, as placed.");
      relayout();
      return;
    }
    if (notification != CBN_SELCHANGE)
      return;
    if (id == TerrainModeList) {
      const wchar_t *how[] = {L"Raise: each dab lifts the ground by the strength.", L"Lower: each dab digs the ground down.",
                              L"Smooth: dabs even out bumps and steps.", L"Flatten: the ground under the brush goes to one level (where you press, or the level typed).",
                              L"Ramp: press at one level and drag to another; a slope is laid between them.",
                              L"Paint: the ground cover picked below, where you brush."};
      message(how[terrainMode()]);
    } else if (id == NatureModeList)
      message(listIndex(natureModeList) == 1 ? L"Erase: brush over plants, trees and rocks to clear them." : L"Plant: brush to grow what is picked.");
    else if (id == WaterModeList) {
      const wchar_t *how[] = {L"Drag a pool. Outdoors its surface sits just under its lowest bank.", L"Click in a hollow: the water rises until it would spill.",
                              L"Pick a pool, then click its bank: the surface goes to the ground there."};
      message(how[waterMode()]);
    }
    relayout();
  }
  bool waterPicked() const {
    return (tool == 0 || tool == WaterTool) && record >= 0 && size_t(record) < doc.lines.size() && selectedRecords.size() <= 1 &&
           NativeDocument::fields(doc.lines[size_t(record)])[0] == "WATER";
  }
  // The WET box round a pool: its banks, 1.5 m out.
  int wetFor(const std::vector<std::string> &water) const {
    float x1 = number(water[2], 0) - 1.5f, y1 = number(water[3], 0) - 1.5f, x2 = number(water[4], 0) + 1.5f, y2 = number(water[5], 0) + 1.5f;
    for (auto i : doc.records("WET", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() == 8 && std::abs(number(f[2], 0) - x1) < .02f && std::abs(number(f[3], 0) - y1) < .02f && std::abs(number(f[4], 0) - x2) < .02f &&
          std::abs(number(f[5], 0) - y2) < .02f)
        return int(i);
    }
    return -1;
  }
  void syncWaterControls() {
    if (!waterPicked() || !waterMurkSlider)
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    SendMessageW(waterMurkSlider, TBM_SETPOS, TRUE, LPARAM(std::lround((f.size() > 8 ? number(f[8], 0) : 0) * 100)));
    int wet = wetFor(f);
    SendMessageW(waterWetSlider, TBM_SETPOS, TRUE, wet < 0 ? 0 : LPARAM(std::lround(number(NativeDocument::fields(doc.lines[size_t(wet)])[6], 0) * 100)));
  }
  std::string waterSliderStart;
  void waterSliderChanged(HWND slider, int code) {
    if (!waterPicked())
      return;
    if (waterSliderStart.empty())
      waterSliderStart = doc.text();
    float value = float(SendMessageW(slider, TBM_GETPOS, 0, 0)) / 100.f;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    auto keep = doc.lines[size_t(record)];
    if (slider == waterMurkSlider) {
      if (value > 0) {
        f.resize(std::max<size_t>(f.size(), 9));
        f[8] = shortNumber(value);
      } else if (f.size() > 8)
        f.resize(8);
      doc.replace(size_t(record), f);
      message(value > 0 ? L"Murk " + wide(shortNumber(value)) + L": still, dark standing water." : L"Clear water.");
    } else {
      int wet = wetFor(f);
      std::vector<std::string> patch{"WET", std::to_string(map), shortNumber(number(f[2], 0) - 1.5f), shortNumber(number(f[3], 0) - 1.5f),
                                     shortNumber(number(f[4], 0) + 1.5f), shortNumber(number(f[5], 0) + 1.5f), shortNumber(value), "1"};
      if (value <= 0 && wet >= 0)
        doc.eraseRecord(size_t(wet));
      else if (value > 0 && wet >= 0)
        doc.replace(size_t(wet), patch);
      else if (value > 0)
        addAuthored(patch);
      // The WET line may sit before the water: keep the pick on the water.
      auto found = std::find(doc.lines.begin(), doc.lines.end(), keep);
      if (found != doc.lines.end())
        record = int(found - doc.lines.begin()), selectedRecords = {record};
      message(value > 0 ? L"Banks wet to " + wide(shortNumber(value)) + L": puddles gather beside the water." : L"Dry banks.");
    }
    if (code == TB_ENDTRACK || code == TB_THUMBPOSITION) {
      if (doc.text() != waterSliderStart) {
        history.push_back(waterSliderStart);
        if (history.size() > 50)
          history.erase(history.begin());
        future.clear();
        ++changeCount;
      }
      waterSliderStart.clear();
      publish();
    } else if (liveTurn(150)) {
      publish(false);
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }

  // ---- The plan outdoors: the land shaded by its height and cover, contours every metre,
  // water filled, and the brush ring under the pointer.
  static COLORREF groundColour(int kind) {
    static const COLORREF colours[] = {RGB(0x4E, 0x6E, 0x2E), RGB(0x3E, 0x62, 0x28), RGB(0x55, 0x74, 0x34), RGB(0x6A, 0x70, 0x3A),
                                       RGB(0x7A, 0x66, 0x44), RGB(0x55, 0x48, 0x30), RGB(0x76, 0x74, 0x6C), RGB(0x4A, 0x3C, 0x2C)};
    return colours[std::clamp(kind, 0, 7)];
  }
  void drawTerrainPlan(HDC dc, int ox, int oy, float cell) const {
    if (!onTerrain() || layer != 0)
      return;
    auto shape = campaignTerrain();
    auto [mx, my] = mapOrigin();
    // Two samples a metre: hill shade from the north-west, cover from the GROUND paint.
    const int n = 48;
    std::vector<float> h(size_t((n + 1) * (n + 1)));
    for (int j = 0; j <= n; ++j)
      for (int i = 0; i <= n; ++i)
        h[size_t(j * (n + 1) + i)] = groundAt(std::min(23.999f, i * .5f), std::min(23.999f, j * .5f));
    float low = *std::min_element(h.begin(), h.end()), high = *std::max_element(h.begin(), h.end());
    for (int j = 0; j < n; ++j)
      for (int i = 0; i < n; ++i) {
        float a = h[size_t(j * (n + 1) + i)], b = h[size_t(j * (n + 1) + i + 1)], c = h[size_t((j + 1) * (n + 1) + i)];
        float shade = std::clamp(1.f + ((a - b) + (a - c)) * .9f, .45f, 1.45f);
        float lift = high > low ? (a - low) / (high - low) : .5f;
        int kind = shape->ground.empty() ? -1 : retro::authoredGroundKind(*shape, mx + i * .5f + .25f, my + j * .5f + .25f);
        COLORREF base = kind >= 0 ? groundColour(kind) : RGB(0x5A, 0x60, 0x3E);
        float k = shade * (.75f + .45f * lift);
        auto mix = [&](int v) { return std::clamp(int(v * k), 0, 255); };
        auto brush = CreateSolidBrush(RGB(mix(GetRValue(base)), mix(GetGValue(base)), mix(GetBValue(base))));
        RECT q{int(ox + i * .5f * cell), int(oy + j * .5f * cell), int(ox + (i + 1) * .5f * cell) + 1, int(oy + (j + 1) * .5f * cell) + 1};
        FillRect(dc, &q, brush);
        DeleteObject(brush);
      }
    // Contours: a line where the ground crosses each whole metre.
    auto p = CreatePen(PS_SOLID, 1, RGB(0x2A, 0x30, 0x1E));
    auto old = SelectObject(dc, p);
    for (int j = 0; j < n; ++j)
      for (int i = 0; i < n; ++i) {
        float a = h[size_t(j * (n + 1) + i)], b = h[size_t(j * (n + 1) + i + 1)], c = h[size_t((j + 1) * (n + 1) + i)];
        int px = int(ox + (i + 1) * .5f * cell), py = int(oy + (j + 1) * .5f * cell);
        if (std::floor(a) != std::floor(b))
          MoveToEx(dc, px, int(oy + j * .5f * cell), nullptr), LineTo(dc, px, py);
        if (std::floor(a) != std::floor(c))
          MoveToEx(dc, int(ox + i * .5f * cell), py, nullptr), LineTo(dc, px, py);
      }
    SelectObject(dc, old);
    DeleteObject(p);
  }
  void drawWaterPlan(HDC dc, int ox, int oy, float cell) const {
    auto brush = CreateSolidBrush(RGB(0x16, 0x3A, 0x7A));
    auto murky = CreateSolidBrush(RGB(0x1C, 0x2C, 0x34));
    for (auto i : doc.records("WATER", map)) {
      if (!visibleFloorRecord(i))
        continue;
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 8)
        continue;
      RECT q{int(ox + std::min(number(f[2], 0), number(f[4], 0)) * cell), int(oy + std::min(number(f[3], 0), number(f[5], 0)) * cell),
             int(ox + std::max(number(f[2], 0), number(f[4], 0)) * cell), int(oy + std::max(number(f[3], 0), number(f[5], 0)) * cell)};
      FillRect(dc, &q, f.size() > 8 && number(f[8], 0) > .5f ? murky : brush);
    }
    DeleteObject(brush);
    DeleteObject(murky);
  }
  void drawBrushRing(HDC dc, int ox, int oy, float cell, float x, float y) const {
    char shape = toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape;
    if (shape != 'T' && shape != 'N')
      return;
    float r = brushRadius();
    COLORREF colour = shape == 'N' ? (listIndex(natureModeList) == 1 ? RGB(0xFF, 0x60, 0x40) : RGB(0x60, 0xE0, 0x60))
                                   : RGB(0xFF, 0xD0, 0x40);
    auto p = CreatePen(PS_SOLID, 2, colour);
    auto old = SelectObject(dc, p), oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(dc, int(ox + (x - r) * cell), int(oy + (y - r) * cell), int(ox + (x + r) * cell), int(oy + (y + r) * cell));
    if (shape == 'T' && terrainMode() == TerrainRamp && stroke.active && stroke.tool == 'T') {
      MoveToEx(dc, int(ox + stroke.startX * cell), int(oy + stroke.startY * cell), nullptr);
      LineTo(dc, int(ox + x * cell), int(oy + y * cell));
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, old);
    DeleteObject(p);
  }
  // Where a terrain map opens onto an indoor one, the land would stand metres above the
  // floor at the seam. Level it at -1 (walked at -0.5) along the seam and lay concrete steps
  // up to the indoor floor at 0 across every open metre: the guide's recipe for a doorway.
  int joinSeams(int m) {
    if (!terrainMapAt(m))
      return 0;
    auto mf = NativeDocument::fields(doc.lines[doc.records("MAP", m).front()]);
    auto [ox, oy] = originOf(m);
    int laid = 0;
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      int other = int(number(f[1], -1));
      if (other == m || f.size() < 16 || terrainMapAt(other) || f[8] == "1")
        continue;
      float nx = number(f[3], 0), ny = number(f[4], 0);
      int edge = std::abs(nx - ox) < .01f ? (std::abs(ny - (oy - 24)) < .01f ? 0 : std::abs(ny - (oy + 24)) < .01f ? 1 : -1)
                 : std::abs(ny - oy) < .01f ? (std::abs(nx - (ox - 24)) < .01f ? 2 : std::abs(nx - (ox + 24)) < .01f ? 3 : -1) : -1;
      if (edge < 0 || mf[size_t(12 + edge)] != "1")
        continue;
      // The flat: the whole seam, six metres into the land and a metre under the floor beyond.
      float x1 = edge == 2 ? ox - 1 : edge == 3 ? ox + 18 : ox, x2 = edge == 2 ? ox + 6 : edge == 3 ? ox + 25 : ox + 24;
      float y1 = edge == 0 ? oy - 1 : edge == 1 ? oy + 18 : oy, y2 = edge == 0 ? oy + 6 : edge == 1 ? oy + 25 : oy + 24;
      std::vector<std::string> flat{"TERRAIN_FLAT", shortNumber(x1), shortNumber(y1), shortNumber(x2), shortNumber(y2), "-1", "6"};
      bool have = false;
      for (auto j : doc.records("TERRAIN_FLAT"))
        have = have || NativeDocument::fields(doc.lines[j]) == flat;
      if (have)
        continue;
      doc.add(flat);
      // Steps on the open metres of the seam: a landing level with the floor, then two treads.
      for (int a = 0; a < 24;) {
        auto open = [&](int k) {
          int tx = edge == 2 ? 0 : edge == 3 ? 23 : k, ty = edge == 0 ? 0 : edge == 1 ? 23 : k;
          int ux = edge == 2 ? 23 : edge == 3 ? 0 : k, uy = edge == 0 ? 23 : edge == 1 ? 0 : k;
          return doc.tile(m, 0, tx, ty) != '#' && doc.tile(other, 0, ux, uy) != '#';
        };
        if (!open(a)) {
          ++a;
          continue;
        }
        int b = a;
        while (b < 24 && open(b))
          ++b;
        for (int step = 0; step < 3; ++step) {
          float d0 = float(step), d1 = float(step + 1), top = step == 0 ? .5f : step == 1 ? .33f : .17f;
          float sx1 = edge == 2 ? d0 : edge == 3 ? 24 - d1 : float(a), sx2 = edge == 2 ? d1 : edge == 3 ? 24 - d0 : float(b);
          float sy1 = edge == 0 ? d0 : edge == 1 ? 24 - d1 : float(a), sy2 = edge == 0 ? d1 : edge == 1 ? 24 - d0 : float(b);
          addAuthored({"STRUCT", std::to_string(m), shortNumber(sx1), shortNumber(sy1), shortNumber(sx2), shortNumber(sy2), "0", shortNumber(top), "0", "15"});
        }
        a = b;
      }
      ++laid;
    }
    return laid;
  }
  int joinAllSeams() {
    int laid = 0;
    for (auto i : doc.records("MAP"))
      laid += joinSeams(int(number(NativeDocument::fields(doc.lines[i])[1], -1)));
    if (laid)
      message(L"Levelled the land at " + std::to_wstring(laid) + L" seam(s) with an indoor map and laid steps up to its floor.");
    return laid;
  }
  // The floor bands the 3D view ghosts by: on a terrain map they stand on the ground
  // under the upper floors, and the top floor's band reaches the sky, so the land
  // itself is never ghosted or hidden as if it were another storey.
  float floorBandGround() const {
    if (!onTerrain())
      return 0;
    float sum = 0;
    int count = 0;
    for (auto &a : floorLayers())
      if (a.id != 0)
        for (int y = 0; y < 24; ++y)
          for (int x = 0; x < 24; ++x) {
            char c = doc.tile(map, a.id, x, y);
            if (c == '=' || c == '#')
              sum += groundAt(x + .5f, y + .5f), ++count;
          }
    return count ? sum / float(count) : groundAt(12, 12);
  }
