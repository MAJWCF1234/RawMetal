  // ---- Cave tool: dig the underground into the land ----
  // Tunnels and caverns carved into the campaign's terrain, as CAVE records (src/world/CaveField.h): the
  // engine skins the carved field with rough rock, walks it and lights it, so the picture on the right is
  // the cave as it plays. Brush a tunnel at the depth picked, click for a cavern, drag a cave mouth in
  // from the hillside, fill back, pull a wall out, smooth it, level a floor. One stroke is one Ctrl+Z
  // (the brush's remember() at the press, NativeOutdoorMethods.inc's stroke machinery).
  HWND caveModeList = nullptr, caveSizeList = nullptr, caveDepthList = nullptr;
  enum CaveMode { CaveDig, CaveMouth, CaveFillBack, CaveMoveWall, CaveSmoothWall, CaveFlattenFloor };
  static constexpr float caveSizes[] = {1.f, 1.6f, 2.2f, 3.2f, 4.5f};
  static constexpr float caveDepths[] = {3.f, 6.f, 9.f, 11.f};
  int caveMode() const { return std::clamp(listIndex(caveModeList), 0, int(CaveFlattenFloor)); }
  float caveRadius() const { return caveSizes[std::clamp(listIndex(caveSizeList, 1), 0, int(std::size(caveSizes)) - 1)]; }
  float caveDepth() const { return caveDepths[std::clamp(listIndex(caveDepthList, 1), 0, int(std::size(caveDepths)) - 1)]; }
  // The CAVE records as the engine reads them (the shape the Terrain tool already parses).
  static std::optional<retro::CaveStroke> caveStrokeOf(const std::vector<std::string> &f) {
    if (f.size() < 9 || f[0] != "CAVE")
      return std::nullopt;
    retro::CaveStroke s;
    s.op = retro::caveOpFromWord(f[1]);
    if (s.op < 0)
      return std::nullopt;
    s.x1 = number(f[2], 0), s.y1 = number(f[3], 0), s.z1 = number(f[4], 0), s.x2 = number(f[5], 0), s.y2 = number(f[6], 0), s.z2 = number(f[7], 0);
    s.radius = std::clamp(number(f[8], 2), retro::CaveMinRadius, retro::CaveMaxRadius);
    if (f.size() > 9 && !f[9].empty())
      s.rough = std::clamp(number(f[9], .35f), 0.f, 1.f);
    if (f.size() > 10 && !f[10].empty())
      s.strength = std::clamp(number(f[10], 1), 0.f, 1.f);
    return s;
  }
  // The level the brush digs at, under the land at (wx, wy): the line through the middle of the tunnel.
  float caveAxis(float wx, float wy) const {
    const auto *shape = campaignTerrain();
    const float top = shape ? retro::authoredTerrainHeight(*shape, wx, wy) : 0.f;
    // Never so deep the floor (0.6 radius under the line) reaches the field's closed bottom.
    return std::max(top - caveDepth(), float(retro::CaveLowestZ) + .6f * caveRadius() + .4f);
  }
  // The floor of the cave under (wx, wy) as the engine finds it (World::caveFloor), or the axis's floor.
  float caveFloorUnder(float wx, float wy) const {
    const auto *shape = campaignTerrain();
    if (!shape || !shape->caveVolume)
      return caveAxis(wx, wy) - .6f * caveRadius();
    return retro::terrainCaveFloor(*shape, int(std::floor(wx)), int(std::floor(wy)));
  }
  void writeCave(const retro::CaveStroke &s, bool replaceLast) {
    auto f = retro::caveFields(s);
    auto caves = doc.records("CAVE");
    if (replaceLast && stroke.planted > 0 && !caves.empty())
      doc.replace(caves.back(), f);
    else
      doc.add(f), ++stroke.planted;
  }
  // A dab of the Cave tool under the pointer (map metres), spaced along the drag.
  void caveDab(float x, float y) {
    if (effectiveFloorView(floorLayer(layer)).locked)
      return;
    const bool first = !stroke.active || stroke.tool != 'C';
    if (first) {
      if (prepareOutdoors())
        message(L"The map is outdoors now: open sky and land to dig into.");
      beginOutdoorStroke(x, y, 'C');
    }
    if (!campaignTerrain() || !onTerrain()) {
      message(L"Choose a terrain sky (Map sky) to dig caves here.");
      return;
    }
    const int mode = caveMode();
    const float r = caveRadius();
    auto [ox, oy] = mapOrigin();
    const float wx = ox + x, wy = oy + y, sx = ox + stroke.startX, sy = oy + stroke.startY;
    const bool reapply = mode == CaveMouth || mode == CaveMoveWall;
    if (first) {
      stroke.startHeight = mode == CaveFlattenFloor ? caveFloorUnder(wx, wy) : caveAxis(wx, wy);
      stroke.lastX = x, stroke.lastY = y;
    } else if (!reapply && std::hypot(x - stroke.lastX, y - stroke.lastY) < std::max(.5f, r * .7f))
      return;
    retro::CaveStroke s;
    s.radius = r;
    switch (mode) {
    case CaveDig:
    case CaveFillBack: {
      // A tunnel from the last dab to this one, level at the depth under the press (a press alone: a cavern).
      s.op = mode == CaveDig ? retro::CaveCarve : retro::CaveFill;
      const float px = first ? wx : ox + stroke.lastX, py = first ? wy : oy + stroke.lastY;
      s.x1 = px, s.y1 = py, s.x2 = wx, s.y2 = wy, s.z1 = s.z2 = stroke.startHeight;
      writeCave(s, false);
      break;
    }
    case CaveMouth: {
      // From the ground at the press, down to the depth under the pointer: the hillside opens there.
      const auto *shape = campaignTerrain();
      s.op = retro::CaveCarve;
      s.x1 = sx, s.y1 = sy, s.z1 = retro::authoredTerrainHeight(*shape, sx, sy) + .4f * r;
      s.x2 = wx, s.y2 = wy, s.z2 = caveAxis(wx, wy);
      if (std::hypot(wx - sx, wy - sy) < .5f)
        return;
      writeCave(s, true);
      break;
    }
    case CaveMoveWall: {
      // The rock and air round the press follow the pointer, at the depth picked.
      s.op = retro::CaveMove;
      s.x1 = sx, s.y1 = sy, s.x2 = wx, s.y2 = wy, s.z1 = s.z2 = stroke.startHeight;
      s.radius = std::max(r, 1.5f);
      if (std::hypot(wx - sx, wy - sy) < .2f)
        return;
      writeCave(s, true);
      break;
    }
    case CaveSmoothWall:
      s.op = retro::CaveSmooth, s.x1 = s.x2 = wx, s.y1 = s.y2 = wy, s.z1 = s.z2 = stroke.startHeight, s.strength = .6f;
      writeCave(s, false);
      break;
    case CaveFlattenFloor:
      s.op = retro::CaveFlatten, s.x1 = s.x2 = wx, s.y1 = s.y2 = wy, s.z1 = s.z2 = stroke.startHeight;
      writeCave(s, false);
      break;
    }
    stroke.lastX = x, stroke.lastY = y;
    ++changeCount;
    if (!quiet())
      sfx::play(mode == CaveFillBack ? sfx::PlaceStone : sfx::BreakStone, .45f, .15f);
    if (liveTurn(150)) {
      publish();
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }
  std::wstring caveStrokeNote() const {
    const wchar_t *done[] = {L"Dug a tunnel.", L"Opened a cave mouth.", L"Filled the cave back.", L"Moved the cave wall.", L"Smoothed the cave.",
                             L"Levelled the cave floor."};
    std::wstring note = done[caveMode()];
    if (caveMode() == CaveDig && stroke.planted == 1)
      note = L"Dug a cavern.";
    return note + L" Ctrl+Z takes the stroke back.";
  }
  void caveControlChanged(int id, int notification) {
    if (notification != CBN_SELCHANGE)
      return;
    if (id == CaveModeList) {
      const wchar_t *how[] = {L"Dig: brush a tunnel at the depth picked; click for a cavern.",
                              L"Cave mouth: drag from the hillside inward. The ground opens there and the tunnel slopes down to the depth picked.",
                              L"Fill back: brush rock back into a cave.",
                              L"Move the wall: press beside a cave wall and drag; the rock follows your hand.",
                              L"Smooth: brush over rough walls to even them out.",
                              L"Flatten the floor: press on a cave floor and brush; the floor levels to where you pressed."};
      message(how[caveMode()]);
    }
    relayout();
  }
  template <class Row> void caveRows(Row &&row) {
    row(caveModeList, L"Brush");
    row(caveSizeList, L"Size");
    if (caveMode() != CaveFlattenFloor)
      row(caveDepthList, L"Depth");
  }
  std::vector<HWND> caveControls() const { return {caveModeList, caveSizeList, caveDepthList}; }
  template <class Choices> void makeCaveControls(Choices &&choices) {
    caveModeList = choices(CaveModeList, {L"Dig a tunnel or cavern", L"Cave mouth (drag in from the hillside)", L"Fill back", L"Move the wall",
                                          L"Smooth", L"Flatten the floor"}, 0);
    caveSizeList = choices(CaveSizeList, {L"Crawlway", L"Tunnel", L"Wide tunnel", L"Cavern", L"Great hall"}, 1);
    caveDepthList = choices(CaveDepthList, {L"Just under the ground", L"Deep", L"Deeper", L"Deepest"}, 1);
  }
