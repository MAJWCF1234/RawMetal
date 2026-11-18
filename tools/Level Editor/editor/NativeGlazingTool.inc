  // ---- Glass and window walls (GLAZING, BREAKABLE glass; the engine's src/world/Openings.cpp) ----
  // Both tools read the wall under a drag: a run of # tiles, a wall box (STRUCT), or an opening
  // (open tiles between two walls). The Window wall tool writes one GLAZING record there: the
  // engine cuts the wall from sill to head, builds it back above and below, and stands a framed
  // grid of breakable panes in the middle of the wall; deleting the record gives the wall back.
  // The Glass tool writes a single window into a solid wall the same way (one bay), and a thin
  // pane, centred, into an opening or across open floor, with the wall closed over it.
  HWND glazingBaysList = nullptr, glazingRowsList = nullptr, glazingHeightList = nullptr, glazingLookList = nullptr,
       glazingPaneList = nullptr, glazingDoorList = nullptr;
  struct WallRun {
    bool alongX = true;
    float a0 = 0, a1 = 0;     // the run along the wall
    float c0 = 0, c1 = 0;     // across: the wall's thickness (or the pane's line, free-standing)
    enum Kind { Free, Tiles, Box, Opening } kind = Free;
  };
  // What the drag from (ax, ay) to (bx, by) lies along. The drag's longer direction is the wall's.
  WallRun wallRunAt(float ax, float ay, float bx, float by) const {
    WallRun w;
    w.alongX = std::abs(bx - ax) >= std::abs(by - ay);
    float lo = w.alongX ? std::min(ax, bx) : std::min(ay, by), hi = w.alongX ? std::max(ax, bx) : std::max(ay, by);
    if (hi - lo < .6f) {
      float mid = (lo + hi) * .5f;
      lo = mid - .5f, hi = mid + .5f;
    }
    lo = std::clamp(lo, 0.f, 23.f), hi = std::clamp(hi, lo + .6f, 24.f);
    const float across = w.alongX ? (ay + by) * .5f : (ax + bx) * .5f;
    auto tile = [&](int along, int line) { return w.alongX ? doc.tile(map, layer, along, line) : doc.tile(map, layer, line, along); };
    auto wallCells = [&](int line) {
      int n = 0;
      if (line < 0 || line > 23)
        return 0;
      for (int a = int(std::floor(lo + .01f)); a <= int(std::floor(hi - .01f)); ++a)
        n += tile(a, line) == '#';
      return n;
    };
    // # tiles under the drag, or in the row beside it (a drag along a wall's face).
    int line = std::clamp(int(std::floor(across)), 0, 23), best = -1, bestCells = 0;
    for (int l : {line, line - 1, line + 1}) {
      int n = wallCells(l);
      if (n > bestCells)
        best = l, bestCells = n;
      if (l == line && n > 0)
        break;
    }
    if (best >= 0 && bestCells * 2 >= int(std::ceil(hi - lo - .02f))) {
      w.kind = WallRun::Tiles, w.c0 = float(best), w.c1 = float(best + 1);
      // Whole tiles along, unless the drag was shorter than one.
      w.a0 = std::floor(lo + .01f), w.a1 = std::max(w.a0 + 1, std::ceil(hi - .01f));
      return w;
    }
    // A wall box the drag runs along: tall, thin across, long along.
    for (auto i : doc.records("STRUCT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 10 || f[8] == "1")
        continue;
      float x1 = number(f[2], 0), y1 = number(f[3], 0), x2 = number(f[4], 0), y2 = number(f[5], 0), bottom = number(f[6], 0), top = number(f[7], 0);
      int material = int(number(f[9], 0));
      if (top - bottom < 1.5f || material == 17 || material == 52 || material == 30 || bottom > layerElevation() + .3f)
        continue;
      float s0 = w.alongX ? x1 : y1, s1 = w.alongX ? x2 : y2, t0 = w.alongX ? y1 : x1, t1 = w.alongX ? y2 : x2;
      if (t1 - t0 > 1.05f || s1 - s0 < t1 - t0 || across < t0 - .35f || across > t1 + .35f || s1 < lo + .3f || s0 > hi - .3f)
        continue;
      w.kind = WallRun::Box, w.c0 = t0, w.c1 = t1, w.a0 = std::max(lo, s0), w.a1 = std::min(hi, s1);
      return w;
    }
    // An opening: open tiles between two walls, along the line the drag crosses.
    if (tile(int(std::floor((lo + hi) * .5f)), line) != '#') {
      int mid = int(std::floor((lo + hi) * .5f)), left = mid, right = mid;
      while (left > 0 && tile(left - 1, line) != '#' && mid - left < 8)
        --left;
      while (right < 23 && tile(right + 1, line) != '#' && right - mid < 8)
        ++right;
      bool walled = left > 0 && right < 23 && tile(left - 1, line) == '#' && tile(right + 1, line) == '#';
      // The wall continues beyond both ends: it is a gap in a wall, not a room.
      if (walled && right - left + 1 <= 8) {
        w.kind = WallRun::Opening, w.c0 = float(line), w.c1 = float(line + 1), w.a0 = float(left), w.a1 = float(right + 1);
        return w;
      }
    }
    // Free-standing: a thin line where the drag went.
    w.a0 = lo, w.a1 = hi, w.c0 = across - .1f, w.c1 = across + .1f;
    return w;
  }
  static constexpr const char *glazingPaneLetters = "cdtfwmxe";
  // The tool's choices as GLAZING named fields; `window` is the Glass tool's one-bay window.
  void glazingFields(std::vector<std::string> &f, const WallRun &w, float floorZ, float top, bool window) {
    const float length = w.a1 - w.a0, height = top - floorZ;
    // Heights: storefront, window, floor to ceiling, clerestory.
    static constexpr float sills[] = {.45f, .9f, 0.f, 1.8f}, heads[] = {2.6f, 2.1f, 99.f, 2.7f};
    int heightPick = window ? 1 : std::clamp(listIndex(glazingHeightList), 0, 3);
    float sill = sills[heightPick], head = std::min(heads[heightPick], height - .1f);
    if (head < sill + .4f)
      sill = std::max(0.f, head - .8f);
    std::vector<std::pair<std::string, std::string>> named;
    auto put = [&](const char *k, const std::string &v) { named.push_back({k, v}); };
    int bays = window ? std::max(1, int(std::lround(length / 1.6f))) : listIndex(glazingBaysList) <= 0 ? 0 : listIndex(glazingBaysList);
    if (bays > 0)
      put("cols", std::to_string(bays));
    if (!window) {
      int rows = std::clamp(listIndex(glazingRowsList), 0, 2) + 1;
      if (rows > 1)
        put("rows", std::to_string(rows));
    }
    put("sill", shortNumber(sill)), put("head", shortNumber(head));
    if (!window) {
      static constexpr const char *looks[] = {"aluminium", "metal", "steel", "wood"};
      int look = std::clamp(listIndex(glazingLookList), 0, 3);
      if (look)
        put("look", looks[look]);
      int panes = std::clamp(listIndex(glazingPaneList), 0, 8);
      if (panes == 8)
        put("panes", "cdcmcddmcxcd"), put("pane", "c"); // a mix, as a long-shut shopfront has
      else if (panes)
        put("pane", std::string(1, glazingPaneLetters[panes]));
      int doorPick = std::clamp(listIndex(glazingDoorList), 0, 3);
      if (doorPick) {
        int count = bays > 0 ? bays : std::max(1, int(std::lround(length / 1.2f)));
        int door = doorPick == 1 ? count / 2 : doorPick == 2 ? 0 : count - 1;
        if (count >= 2 || doorPick)
          put("door", std::to_string(door));
        if (head < 2.3f)
          put("head", shortNumber(std::min(height - .1f, 2.6f))), named.erase(std::find_if(named.begin(), named.end(), [](auto &n) { return n.first == "head"; }));
      }
    }
    if (w.kind == WallRun::Opening || w.kind == WallRun::Free)
      put("cut", "0");
    doc.ensureFormat2();
    for (auto &[k, v] : named)
      f.push_back(k + "=" + v);
  }
  // A GLAZING record's positional fields: the wall box, its floor and the wall's top.
  void glazingBox(std::vector<std::string> &f, const WallRun &w, float floorZ, float top) {
    f.resize(std::max<size_t>(f.size(), 8));
    f[2] = shortNumber(w.alongX ? w.a0 : w.c0), f[3] = shortNumber(w.alongX ? w.c0 : w.a0);
    f[4] = shortNumber(w.alongX ? w.a1 : w.c1), f[5] = shortNumber(w.alongX ? w.c1 : w.a1);
    f[6] = shortNumber(floorZ), f[7] = shortNumber(top);
    f.resize(8);
  }
  // Set while the Glass tool writes its one-bay window through authorKind("GLAZING").
  bool glassWindow = false;
  // The Glass tool: a window cut into a solid wall (a one-bay GLAZING), or a thin pane in an opening
  // or across the floor, the wall closed over it. Returns true when it wrote a GLAZING instead.
  bool glassIntoWall(float ax, float ay, float bx, float by) const {
    auto w = wallRunAt(ax, ay, bx, by);
    return w.kind == WallRun::Tiles || w.kind == WallRun::Box;
  }
  void glassPaneFields(std::vector<std::string> &f, float ax, float ay, float bx, float by, float floorZ) {
    auto w = wallRunAt(ax, ay, bx, by);
    const float ceiling = creationCeiling();
    // A pane is 24 mm of glass in the middle of the wall's line: floor to 2.4 m in an opening (the wall
    // is closed over it), a 2.2 m screen standing on open floor.
    const float mid = (w.c0 + w.c1) * .5f, head = std::min(ceiling - floorZ, w.kind == WallRun::Opening ? 2.4f : 2.2f);
    f[2] = shortNumber(w.alongX ? w.a0 : mid - .012f), f[3] = shortNumber(w.alongX ? mid - .012f : w.a0);
    f[4] = shortNumber(w.alongX ? w.a1 : mid + .012f), f[5] = shortNumber(w.alongX ? mid + .012f : w.a1);
    f[6] = shortNumber(floorZ), f[7] = shortNumber(floorZ + head);
    glassHeader = {};
    if (w.kind == WallRun::Opening && ceiling - (floorZ + head) > .05f)
      glassHeader = {"STRUCT", std::to_string(map), shortNumber(w.alongX ? w.a0 : w.c0), shortNumber(w.alongX ? w.c0 : w.a0),
                     shortNumber(w.alongX ? w.a1 : w.c1), shortNumber(w.alongX ? w.c1 : w.a1), shortNumber(floorZ + head), shortNumber(ceiling), "0", "3"};
  }
  std::vector<std::string> glassHeader;
  void glazingControlChanged(int id, int notification) {
    if (notification != CBN_SELCHANGE)
      return;
    if (id == GlazingHeightList) {
      const wchar_t *how[] = {L"Storefront: glass from 0.45 m to 2.6 m.", L"Window: glass from 0.9 m to 2.1 m.",
                              L"Floor to ceiling: glass from the floor to just under the top.", L"Clerestory: a strip of glass from 1.8 m to 2.7 m."};
      message(how[std::clamp(listIndex(glazingHeightList), 0, 3)]);
    } else if (id == GlazingPaneList)
      message(L"Every pane breaks on its own. Mesh is chain-link: it stops you, not light or rounds. Wired glass holds together.");
    else if (id == GlazingDoorList)
      message(L"A doorway bay is open to the floor, with a pane over it.");
    relayout();
  }
  // The tool boxes on the right, under the tool's name.
  template <class Row> void glazingRows(Row &&row) {
    row(glazingHeightList, L"Glass");
    row(glazingBaysList, L"Bays");
    row(glazingRowsList, L"Rows");
    row(glazingPaneList, L"Panes");
    row(glazingLookList, L"Frame");
    row(glazingDoorList, L"Doorway");
  }
  std::vector<HWND> glazingControls() const {
    return {glazingBaysList, glazingRowsList, glazingHeightList, glazingLookList, glazingPaneList, glazingDoorList};
  }
  template <class Choices> void makeGlazingControls(Choices &&choices) {
    glazingHeightList = choices(GlazingHeightList, {L"Storefront (0.45 - 2.6 m)", L"Window (0.9 - 2.1 m)", L"Floor to ceiling", L"Clerestory (1.8 - 2.7 m)"}, 0);
    glazingBaysList = choices(GlazingBaysList, {L"Auto (a bay every 1.2 m)", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"}, 0);
    glazingRowsList = choices(GlazingRowsList, {L"1 row", L"2 rows", L"3 rows"}, 0);
    glazingPaneList = choices(GlazingPaneList, {L"Clear", L"Dirty", L"Tinted", L"Frosted", L"Wired (holds)", L"Chain-link mesh", L"Pre-cracked", L"Empty frame",
                                                L"Mixed: clear, dirty, mesh, cracked"}, 0);
    glazingLookList = choices(GlazingLookList, {L"Aluminium", L"Painted metal", L"Steel", L"Wood"}, 0);
    glazingDoorList = choices(GlazingDoorList, {L"No doorway", L"Doorway in the middle", L"Doorway at the start", L"Doorway at the end"}, 0);
  }
