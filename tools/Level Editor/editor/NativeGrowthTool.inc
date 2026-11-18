  // ---- Growth brush: a building going back to green ----
  // Ivy and moss on the walls, creepers hanging from the ceiling and over the edges of ledges, grass and
  // moss through the floor and on ledge tops. Every piece is an ordinary FIXTURE of a purchased model
  // (the low-poly ivy pack's patches, strands and crossed creepers, the LRT texture pack's moss and cave
  // vines, PSX Nature's grass and Virginia creeper), so the
  // engine, the inspector, copy, paste and prefabs treat it like any other. Hanging creepers are plants
  // with physbones (renderer/Foliage.h): they drift, part round whoever walks through and flinch when
  // shot. A stroke is one Ctrl+Z (the brush's remember() at the press, NativeOutdoorMethods.inc's
  // stroke machinery for its seed and spacing).
  HWND growthKindList = nullptr, growthModeList = nullptr, growthSizeList = nullptr, growthDensityList = nullptr, growthScaleList = nullptr,
       growthSeedBox = nullptr;
  enum GrowthKind { GrowIvy, GrowWallMoss, GrowCreepers, GrowGrass, GrowFloorMoss, GrowMix };
  static constexpr float growthSizes[] = {.5f, 1.f, 2.f, 3.f, 5.f};
  float growthRadius() const { return growthSizes[std::clamp(listIndex(growthSizeList, 2), 0, int(std::size(growthSizes)) - 1)]; }
  int growthKind() const { return std::clamp(listIndex(growthKindList), 0, int(GrowMix)); }
  static bool growthModel(int slot) {
    using namespace retro::model;
    for (int m : {ivy_wall_a, ivy_wall_b, ivy_strand, growth_ivy_creeper, creeper, growth_vine_sheet, growth_moss_wall, growth_moss_floor, growth_hanging_vine, grass_a, grass_b, grass_c,
                  grass_l, tall_grass_m, clover_patch})
      if (slot == m)
        return true;
    return false;
  }
  // A face of wall the brush can grow on: a vertical line from a0 to a1 along x (alongX) or y, at `at`
  // across, facing out along (nx, ny), from z0 to z1 (absolute metres).
  struct GrowthFace {
    bool alongX = true;
    float at = 0, a0 = 0, a1 = 0, nx = 0, ny = 1, z0 = 0, z1 = 3;
  };
  // The top of a block standing clear of the floor and the ceiling (a ledge, a parapet, a planter, a
  // partition's top): grass and moss on it, creepers over its edges.
  struct GrowthLedge {
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0, top = 0;
  };
  // Walls of the active floor near (x, y): runs of # tiles facing open floor (merged along each row and
  // column, so a patch of ivy spans tiles), and the sides of tall wall blocks.
  std::vector<GrowthFace> growthFaces(float x, float y, float reach) const {
    std::vector<GrowthFace> out;
    const float floor = layerElevation(), top = creationCeiling();
    auto wall = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < 24 && cy < 24 && doc.tile(map, layer, cx, cy) == '#'; };
    auto open = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < 24 && cy < 24 && doc.tile(map, layer, cx, cy) != '#'; };
    // Faces along x (facing north or south), row by row; then along y.
    for (int along = 0; along < 2; ++along)
      for (int line = 0; line < 24; ++line)
        for (int side = -1; side <= 1; side += 2) {
          int start = -1;
          for (int a = 0; a <= 24; ++a) {
            const bool here = a < 24 && (along == 0 ? wall(a, line) && open(a, line + side) : wall(line, a) && open(line + side, a));
            if (here && start < 0)
              start = a;
            if (!here && start >= 0) {
              GrowthFace f;
              f.alongX = along == 0, f.a0 = float(start), f.a1 = float(a), f.z0 = floor, f.z1 = top;
              f.at = float(side > 0 ? line + 1 : line);
              (f.alongX ? f.ny : f.nx) = float(side), (f.alongX ? f.nx : f.ny) = 0;
              out.push_back(f);
              start = -1;
            }
          }
        }
    for (auto i : doc.records("STRUCT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 10 || f[8] == "1")
        continue;
      const float x1 = number(f[2], 0), y1 = number(f[3], 0), x2 = number(f[4], 0), y2 = number(f[5], 0), bottom = number(f[6], 0), high = number(f[7], 0);
      const int material = int(number(f[9], 0));
      if (high - bottom < 1.f || bottom > floor + .3f || high < floor + 1.f || material == 17 || material == 52 || material == 30)
        continue;
      const float z0 = std::max(bottom, floor), z1 = std::min(high, top);
      for (int s = 0; s < 4; ++s) {
        GrowthFace g;
        g.alongX = s < 2, g.z0 = z0, g.z1 = z1;
        g.a0 = g.alongX ? x1 : y1, g.a1 = g.alongX ? x2 : y2;
        g.at = s == 0 ? y1 : s == 1 ? y2 : s == 2 ? x1 : x2;
        g.nx = s == 2 ? -1.f : s == 3 ? 1.f : 0.f, g.ny = s == 0 ? -1.f : s == 1 ? 1.f : 0.f;
        // A side against a wall of tiles is no face.
        const float mx = g.alongX ? (g.a0 + g.a1) * .5f : g.at + g.nx * .05f, my = g.alongX ? g.at + g.ny * .05f : (g.a0 + g.a1) * .5f;
        if (wall(int(std::floor(mx)), int(std::floor(my))) || g.a1 - g.a0 < .2f)
          continue;
        out.push_back(g);
      }
    }
    std::erase_if(out, [&](const GrowthFace &f) {
      const float dx = f.alongX ? std::clamp(x, f.a0, f.a1) - x : f.at - x, dy = f.alongX ? f.at - y : std::clamp(y, f.a0, f.a1) - y;
      return std::hypot(dx, dy) > reach;
    });
    return out;
  }
  std::vector<GrowthLedge> growthLedges(float x, float y, float reach) const {
    std::vector<GrowthLedge> out;
    const float floor = layerElevation(), top = creationCeiling();
    for (auto i : doc.records("STRUCT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 10 || f[8] == "1")
        continue;
      GrowthLedge l{number(f[2], 0), number(f[3], 0), number(f[4], 0), number(f[5], 0), number(f[7], 0)};
      const int material = int(number(f[9], 0));
      if (l.top < floor + .5f || l.top > top - .4f || l.x2 - l.x1 < .2f || l.y2 - l.y1 < .2f || material == 17 || material == 52)
        continue;
      if (std::hypot(std::clamp(x, l.x1, l.x2) - x, std::clamp(y, l.y1, l.y2) - y) <= reach)
        out.push_back(l);
    }
    return out;
  }
  // One piece: a FIXTURE of `slot` scaled by k, its footprint centre at (px, py), its base at absolute z.
  void growthPiece(int slot, float k, float px, float py, float z, float yaw) {
    const auto &size = retro::model::size[size_t(slot)];
    const float w = std::max(.02f, size[0] * k), d = std::max(.005f, size[1] * k), h = std::max(.005f, size[2] * k);
    addAuthored({"FIXTURE", std::to_string(map), std::to_string(slot), shortNumber(px), shortNumber(py), shortNumber(z - bottomLayerElevation()),
                 shortNumber(w), shortNumber(d), shortNumber(h), shortNumber(yaw), "0"});
    growthPlaced.push_back({px, py, z + h * .5f});
    ++stroke.planted;
  }
  // What this stroke has already grown (and what was there), to keep pieces apart.
  std::vector<std::array<float, 3>> growthPlaced;
  bool growthCrowded(float x, float y, float z, float apart) const {
    return std::any_of(growthPlaced.begin(), growthPlaced.end(),
                       [&](const auto &p) { return std::hypot(p[0] - x, p[1] - y) < apart && std::abs(p[2] - z) < apart * 1.2f; });
  }
  void growthDab(float x, float y) {
    if (effectiveFloorView(floorLayer(layer)).locked)
      return;
    const bool first = !stroke.active || stroke.tool != 'G';
    if (first) {
      beginOutdoorStroke(x, y, 'G');
      // A typed seed makes the stroke repeatable: the same seed, the same growth.
      wchar_t typed[32]{};
      if (growthSeedBox && GetWindowTextW(growthSeedBox, typed, 32) > 0) {
        wchar_t *end = nullptr;
        const unsigned long seed = std::wcstoul(typed, &end, 10);
        if (end != typed)
          stroke.seed = unsigned(seed);
      }
      growthPlaced.clear();
      for (auto i : doc.records("FIXTURE", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (f.size() > 8 && growthModel(int(number(f[2], -1))))
          growthPlaced.push_back({number(f[3], 0), number(f[4], 0), number(f[5], 0) + bottomLayerElevation() + number(f[8], 0) * .5f});
      }
    }
    const float r = growthRadius();
    if (!first && std::hypot(x - stroke.lastX, y - stroke.lastY) < std::max(.4f, r * .5f))
      return;
    stroke.lastX = x, stroke.lastY = y;
    const bool erase = listIndex(growthModeList) == 1 || (!backgroundTest && (GetKeyState(VK_MENU) & 0x8000));
    if (erase) {
      const float floor = layerElevation(), top = creationCeiling();
      std::vector<size_t> victims;
      for (auto i : doc.records("FIXTURE", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (f.size() < 9 || !growthModel(int(number(f[2], -1))))
          continue;
        const float base = number(f[5], 0) + bottomLayerElevation();
        if (std::hypot(number(f[3], 0) - x, number(f[4], 0) - y) <= r && base >= floor - .3f && base <= top + .1f)
          victims.push_back(i);
      }
      for (auto it = victims.rbegin(); it != victims.rend(); ++it)
        eraseNativeObject(doc, *it);
      stroke.erased += int(victims.size());
      std::erase_if(growthPlaced, [&](const auto &p) { return std::hypot(p[0] - x, p[1] - y) <= r; });
    } else
      growthGrow(x, y, r);
    ++changeCount;
    if (!quiet())
      sfx::play(erase ? sfx::BreakStone : sfx::PlaceStone, .3f, .2f);
    if (liveTurn(150)) {
      publish();
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }
  void growthGrow(float x, float y, float r) {
    using namespace retro::model;
    std::mt19937 random(stroke.seed + unsigned(stroke.planted * 7919 + int(x * 131) + int(y * 977)));
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    static constexpr float densities[] = {1.6f, 1.f, .65f}, scales[][2] = {{.55f, .8f}, {.8f, 1.1f}, {1.1f, 1.45f}};
    const float density = densities[std::clamp(listIndex(growthDensityList, 1), 0, 2)];
    const auto scale = scales[std::clamp(listIndex(growthScaleList, 1), 0, 2)];
    auto k = [&] { return scale[0] + (scale[1] - scale[0]) * unit(random); };
    auto pick = [&](std::initializer_list<int> from) { return *(from.begin() + std::min(int(from.size()) - 1, int(unit(random) * float(from.size())))); };
    const int kind = growthKind();
    const bool mix = kind == GrowMix;
    const float floor = layerElevation(), ceiling = creationCeiling();
    // ---- On the walls: ivy patches and vine sheets that hug the face, moss low down where it is damp ----
    auto onWalls = [&](bool ivy, float spacing) {
      for (const auto &face : growthFaces(x, y, r)) {
        // The stretch of this face inside the brush.
        const float off = face.alongX ? face.at - y : face.at - x;
        if (std::abs(off) > r)
          continue;
        const float half = std::sqrt(std::max(0.f, r * r - off * off)), centre = face.alongX ? x : y;
        const float lo = std::max(face.a0, centre - half), hi = std::min(face.a1, centre + half);
        if (hi - lo < .15f)
          continue;
        const int wanted = std::clamp(int((hi - lo) * (face.z1 - face.z0) / (spacing * spacing) + unit(random)), ivy ? 1 : 1, 24);
        for (int n = 0, tries = 0; n < wanted && tries < wanted * 4; ++tries) {
          // A tall wall takes the PSX pack's Virginia creeper too, climbing from the floor.
          const bool tall = face.z1 - face.z0 >= 3.2f;
          const int slot = !ivy ? growth_moss_wall : tall && unit(random) < .25f ? creeper : pick({ivy_wall_a, ivy_wall_b, ivy_wall_a, growth_vine_sheet});
          const auto &size = retro::model::size[size_t(slot)];
          // Never wider than the face it is on.
          const float s = std::min(k(), (face.a1 - face.a0) / std::max(.05f, size[0]) * .95f), w = size[0] * s, h = size[2] * s;
          if (h > face.z1 - face.z0 - .05f)
            continue;
          const float along = std::clamp(lo + (hi - lo) * unit(random), face.a0 + w * .5f, face.a1 - w * .5f);
          // Ivy anywhere up the wall (most from the foot and from the top); moss low, where the damp is.
          const float u = unit(random), room = face.z1 - face.z0 - h;
          const float z = slot == creeper ? face.z0 : face.z0 + (ivy ? (u < .5f ? room * u * u * 2.f : room * (1.f - (1.f - u) * (1.f - u) * 2.f)) : std::min(room, 1.4f) * u * u);
          const float out = size[1] * s * .5f + .01f;
          const float px = face.alongX ? along : face.at + face.nx * out, py = face.alongX ? face.at + face.ny * out : along;
          if (growthCrowded(px, py, z + h * .5f, std::max(.35f, std::min(w, h) * .55f)))
            continue;
          growthPiece(slot, s, px, py, z, std::atan2(face.nx, face.ny));
          ++n;
        }
      }
    };
    // ---- Hanging: creepers from the ceiling, and over the edges of ledges ----
    auto creepers = [&](float spacing) {
      const bool roofed = ceiling - floor <= 8.f && !onTerrain();
      const int wanted = std::clamp(int(3.14159f * r * r / (spacing * spacing) + unit(random)), 1, 40);
      for (int n = 0, tries = 0; roofed && n < wanted && tries < wanted * 3; ++tries) {
        const float a = unit(random) * 6.2831853f, d = r * std::sqrt(unit(random)), px = x + std::cos(a) * d, py = y + std::sin(a) * d;
        if (natureBlocked(px, py, .15f))
          continue;
        const int slot = pick({growth_ivy_creeper, growth_ivy_creeper, growth_ivy_creeper, growth_hanging_vine});
        const float s = std::min(k() * (.7f + .5f * unit(random)), (ceiling - floor - .4f) / retro::model::size[size_t(slot)][2]);
        const float h = retro::model::size[size_t(slot)][2] * s;
        if (s < .2f || growthCrowded(px, py, ceiling - h * .5f, .35f))
          continue;
        growthPiece(slot, s, px, py, ceiling - h, unit(random) * 6.2831853f);
        ++n;
      }
      // Over every open edge of a ledge in reach: strands lying down its face, their tops at its lip.
      for (const auto &l : growthLedges(x, y, r))
        for (int side = 0; side < 4; ++side) {
          const bool alongX = side < 2;
          const float nx = side == 2 ? -1.f : side == 3 ? 1.f : 0.f, ny = side == 0 ? -1.f : side == 1 ? 1.f : 0.f;
          const float a0 = alongX ? l.x1 : l.y1, a1 = alongX ? l.x2 : l.y2, at = side == 0 ? l.y1 : side == 1 ? l.y2 : side == 2 ? l.x1 : l.x2;
          const float probeX = alongX ? (a0 + a1) * .5f : at + nx * .3f, probeY = alongX ? at + ny * .3f : (a0 + a1) * .5f;
          if (natureBlocked(probeX, probeY, 0.f))
            continue; // that side is against a wall
          for (float t = a0 + .2f + spacing * .4f * unit(random); t < a1 - .2f; t += spacing * (.45f + .5f * unit(random))) {
            const float px0 = alongX ? t : at, py0 = alongX ? at : t;
            if (std::hypot(px0 - x, py0 - y) > r)
              continue;
            const int slot = ivy_strand;
            const float s = std::min(k(), (l.top - floor - .3f) / retro::model::size[size_t(slot)][2]);
            if (s < .25f)
              continue;
            const auto &size = retro::model::size[size_t(slot)];
            const float h = size[2] * s, out = size[1] * s * .5f + .015f;
            const float px = px0 + nx * out, py = py0 + ny * out;
            if (growthCrowded(px, py, l.top - h * .5f, .3f))
              continue;
            growthPiece(slot, s, px, py, l.top + .04f - h, std::atan2(nx, ny));
          }
        }
    };
    // ---- Underfoot: grass tufts and moss through the floor, and on the tops of ledges ----
    auto underfoot = [&](bool grass, float spacing) {
      const auto ledges = growthLedges(x, y, r);
      const int wanted = std::clamp(int(3.14159f * r * r / (spacing * spacing) + unit(random)), 1, 90);
      for (int n = 0, tries = 0; n < wanted && tries < wanted * 3; ++tries) {
        const float a = unit(random) * 6.2831853f, d = r * std::sqrt(unit(random)), px = x + std::cos(a) * d, py = y + std::sin(a) * d;
        float z = floor;
        bool onLedge = false;
        for (const auto &l : ledges)
          if (px > l.x1 + .08f && px < l.x2 - .08f && py > l.y1 + .08f && py < l.y2 - .08f && l.top > z)
            z = l.top, onLedge = true;
        if (!onLedge && natureBlocked(px, py, .12f))
          continue;
        const int slot = grass ? pick({grass_a, grass_b, grass_c, grass_l, grass_l, tall_grass_m}) : growth_moss_floor;
        if (growthCrowded(px, py, z + .1f, spacing * .7f))
          continue;
        // Indoor tufts stand a little smaller than the field grass they are.
        growthPiece(slot, k() * (grass ? .85f : 1.f), px, py, z, unit(random) * 6.2831853f);
        ++n;
      }
    };
    switch (mix ? -1 : kind) {
    case GrowIvy: onWalls(true, 1.3f * density); break;
    case GrowWallMoss: onWalls(false, .75f * density); break;
    case GrowCreepers: creepers(.8f * density); break;
    case GrowGrass: underfoot(true, .42f * density); break;
    case GrowFloorMoss: underfoot(false, .7f * density); break;
    default:
      onWalls(true, 1.6f * density), onWalls(false, 1.1f * density), creepers(1.4f * density);
      underfoot(true, .55f * density), underfoot(false, 1.2f * density);
    }
  }
  void growthControlChanged(int id, int notification) {
    if (notification != CBN_SELCHANGE)
      return;
    if (id == GrowthKindList) {
      const wchar_t *how[] = {L"Ivy: patches that hug the wall, climbing from the floor and hanging from the top.",
                              L"Wall moss: low on the wall, where the damp is.",
                              L"Creepers: hanging from the ceiling, and over the edges of ledges. They sway, and part as you walk through.",
                              L"Grass: tufts through the floor, and on the tops of ledges.",
                              L"Floor moss: flat patches on the floor and on ledges.",
                              L"Overgrown: all of it at once, ivy and moss on the walls, creepers, grass and moss."};
      message(how[growthKind()]);
    } else if (id == GrowthModeList)
      message(listIndex(growthModeList) == 1 ? L"Erase: brush over growth to clear it (Alt does it too)." : L"Grow: brush where it should grow.");
    relayout();
  }
  template <class Row> void growthRows(Row &&row) {
    row(growthKindList, L"Grow");
    row(growthModeList, L"Brush");
    row(growthSizeList, L"Brush size");
    row(growthDensityList, L"Density");
    row(growthScaleList, L"Size");
    row(growthSeedBox, L"Seed", 26);
  }
  std::vector<HWND> growthControls() const { return {growthKindList, growthModeList, growthSizeList, growthDensityList, growthScaleList, growthSeedBox}; }
  template <class Choices> void makeGrowthControls(Choices &&choices) {
    growthKindList = choices(GrowthKindList, {L"Ivy on walls", L"Moss on walls", L"Hanging creepers", L"Grass through the floor", L"Moss on the floor", L"Overgrown (all of it)"}, 0);
    growthModeList = choices(GrowthModeList, {L"Grow", L"Erase"}, 0);
    growthSizeList = choices(GrowthSizeList, {L"0.5 m", L"1 m", L"2 m", L"3 m", L"5 m"}, 2);
    growthDensityList = choices(GrowthDensityList, {L"Sparse", L"Normal", L"Dense"}, 1);
    growthScaleList = choices(GrowthScaleList, {L"Small", L"Medium", L"Large"}, 1);
    growthSeedBox = controlWindow(L"EDIT", L"", ES_AUTOHSCROLL | ES_NUMBER | WS_BORDER | WS_TABSTOP, GrowthSeedBox);
    SendMessageW(growthSeedBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"random (type a number to repeat)"));
  }
