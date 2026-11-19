  // ---- Ghosts: what a placing tool is about to make, made by the placement itself.
  // The plan's dashed ghost and the 3D view's outline under the pointer are the records the real
  // click or drag would write: the tool's gesture run on a scratch copy of the document (ghostOf),
  // with the same snapping, wall snap, size, facing, R turns and height as the placement that
  // follows. A drag held in 3D (or on the plan) shows the scratch copy whole in the picture
  // (previewGesture), so the picture is the placement. Nothing a ghost does reaches the document,
  // the history, the selection or the engine; switching tools, maps or floors, Undo and Redo put
  // any preview away (cancelPlacement), and a placement hides the ghost until the pointer moves.
  bool ghostDryRun = false;          // publish() writes nothing while a ghost is worked out
  bool ghostHold = false;            // just placed: no ghost until the pointer moves on
  float ghostHoldX = -1, ghostHoldY = -1;
  bool viewGhostOn = false;          // the pointer is over the 3D view with a placing tool
  float viewGhostX = 0, viewGhostY = 0;
  bool placeInCave = false;          // the 3D pointer is on the floor of a cave (cave=1, CaveField.h)
  bool viewGhostCave = false;
  std::string ghostKey;              // what ghostLines were worked out for
  std::vector<std::string> ghostLines;      // the records the next click or drag writes
  std::vector<retro::EditorGuide> ghostGuideList; // their outlines for the 3D view (hover guides)
  // The previewGesture last asked for and last shown: a throttled one is caught up on the tick.
  std::array<float, 4> previewWanted{-1, -1, -1, -1}, previewShown{-2, -2, -2, -2};
  bool previewHeld = false;

  // The tools that place a record where the pointer is (a point, a box or a line, the player start).
  bool ghostTool() const {
    if (tool <= 0 || tool >= ToolCount || playing) return false;
    const char s = toolInfo[size_t(tool)].shape;
    if (s != 'p' && s != 'r' && s != 'l' && s != 'g' && s != 'd') return false;
    if (tool == WaterTool && waterMode() != 0) return false; // a click fills the hollow: no box to show
    return true;
  }
  // Every choice a placement reads besides the pointer: a change of any makes a new ghost.
  std::string ghostSettings() const {
    std::ostringstream k;
    k << tool << ' ' << map << ' ' << layer << ' ' << doc.version << ' ' << selectedModel() << ' ' << structureMaterial << ' ' << placeHeight << ' '
      << smartSnap << ' ' << snap << ' ' << primFacing << ' ' << doorTurn << ' ' << emitterPreset << ' ' << soundPick << ' ' << flooringLook << ' '
      << flooringTrim << ' ' << flooringLevel << ' ' << placeInCave << ' ' << connectPending << ' ' << connectFromX << ' ' << connectFromY << ' '
      << paintTurn;
    for (HWND list : {primList, clutterList, creatureList, pickupList, hazardList, soundList, roleList, behaviourList, connectKindList, doorToolList})
      k << ' ' << (list ? SendMessageW(list, CB_GETCURSEL, 0, 0) : -1);
    for (const auto &group : {glazingControls(), breachControls(), clothControls()})
      for (HWND list : group)
        k << ' ' << (list ? SendMessageW(list, CB_GETCURSEL, 0, 0) : -1);
    return k.str();
  }
  // The records the gesture (ax, ay)-(bx, by) of the tool in hand would write, worked out on a
  // scratch copy and the copy thrown away: the lines new or changed against the document.
  std::vector<std::string> ghostOf(float ax, float ay, float bx, float by) {
    std::ostringstream key;
    key.precision(9);
    key << ax << ' ' << ay << ' ' << bx << ' ' << by << '|' << ghostSettings();
    if (key.str() == ghostKey) return ghostLines;
    NativeDocument keep = doc;
    const int keepRecord = record, keepKind = kind, keepTool = tool;
    const auto keepSelection = selectedRecords;
    const auto keepPicked = pickedText;
    const bool keepDirty = dirty, keepGhosting = ghosting, keepPending = connectPending, keepGlassWindow = glassWindow;
    const auto keepChanges = changeCount;
    const auto keepGlassHeader = glassHeader;
    const float keepFromX = connectFromX, keepFromY = connectFromY;
    ghosting = ghostDryRun = true;
    try {
      gestureNow(ax, ay, bx, by);
    } catch (...) {
    }
    ghosting = keepGhosting, ghostDryRun = false;
    auto made = newLines(keep.lines, doc.lines);
    doc = std::move(keep);
    record = keepRecord, kind = keepKind, tool = keepTool, selectedRecords = keepSelection, pickedText = keepPicked;
    dirty = keepDirty, changeCount = keepChanges, connectPending = keepPending, glassWindow = keepGlassWindow, glassHeader = keepGlassHeader;
    connectFromX = keepFromX, connectFromY = keepFromY;
    ghostKey = key.str();
    return made;
  }
  // The lines of `after` that `before` does not have (new or changed), in order: the records a
  // gesture wrote, wherever it wrote them. The editor's floor and automatic markers, FORMAT and
  // the rows of tiles are its bookkeeping, not things to show.
  static std::vector<std::string> newLines(const std::vector<std::string> &before, const std::vector<std::string> &after) {
    std::unordered_map<std::string_view, int> had;
    had.reserve(before.size());
    for (const auto &line : before) ++had[line];
    std::vector<std::string> made;
    for (const auto &line : after) {
      auto it = had.find(line);
      if (it != had.end() && it->second > 0) {
        --it->second;
        continue;
      }
      auto f = NativeDocument::fields(line);
      if (f.empty() || f[0].empty() || f[0] == "# EDITOR_LAYER" || f[0] == "# EDITOR_AUTO" || f[0] == "FORMAT" || f[0] == "ROW") continue;
      made.push_back(line);
    }
    return made;
  }
  // Where the next placement's gesture runs from and to: the drag held, the Connect tool's first
  // click and the pointer, or a click at the pointer (on the plan, or in the 3D view).
  std::optional<std::array<float, 4>> ghostGesture() const {
    if (!ghostTool()) return std::nullopt;
    const bool stretching = (drag || viewRect) && !brushing && !objectDrag && dragTool();
    const bool onPlan = hoverInside || drag;
    if (!onPlan && !viewRect && !viewGhostOn) return std::nullopt;
    const float hx = onPlan || viewRect ? hoverX : viewGhostX, hy = onPlan || viewRect ? hoverY : viewGhostY;
    if (tool == ConnectTool && connectKind() != ConnectLamp && !stretching) {
      if (!connectPending) return std::nullopt; // the first click only picks the thing
      return std::array<float, 4>{connectFromX, connectFromY, hx, hy};
    }
    if (stretching) return std::array<float, 4>{dragFX, dragFY, hx, hy};
    return std::array<float, 4>{hx, hy, hx, hy};
  }
  // The outline of one record as the editor shows it picked: a shape's real turned outline, a
  // box's turned corners, a line's ends. closed false: a line.
  struct GhostShape {
    std::vector<std::pair<float, float>> points;
    bool closed = true;
    float bottom = 0, top = 0;
  };
  std::optional<GhostShape> ghostShapeOf(const std::string &line) const {
    auto f = NativeDocument::fields(line);
    const int schema = schemaFor(f);
    if (schema < 0) return std::nullopt;
    GhostShape g;
    if (f[0] == "MAP" && f.size() > 7) { // the player start: where they stand, a person tall
      const float x = number(f[5], 0), y = number(f[6], 0), z = number(f[7], 0) + (onTerrain() && layer == 0 ? groundAt(std::clamp(x, 0.f, 23.99f), std::clamp(y, 0.f, 23.99f)) : 0.f);
      g.points = {{x - .25f, y - .25f}, {x + .25f, y - .25f}, {x + .25f, y + .25f}, {x - .25f, y + .25f}};
      g.bottom = z, g.top = z + 1.75f;
      return g;
    }
    if (f[0] == "PRIM" && f.size() > 9) {
      const auto s = primSize(f);
      g.points = primOutline(f[2], number(f[3], 0), number(f[4], 0), int(number(f[9], 0)) & 3, s.w, s.d);
      const float z = number(f[5], 0) + liftOf(f);
      g.bottom = z, g.top = z + s.h;
    } else {
      g.points = nativeCorners(schemas[size_t(schema)].fields, f);
      g.closed = !nativeLineKind(f[0]) && g.points.size() > 2;
      if (auto box = worldBoxLine(line)) g.bottom = box->bottom, g.top = box->top;
      else g.bottom = layerElevation(), g.top = g.bottom + .1f;
    }
    if (g.points.empty()) return std::nullopt;
    return g;
  }
  // A cave=1 record stands on the floor of the cave under it (World::settleExclusionFacilityOnTerrain),
  // not the land on top: its box measured from there.
  static bool caveKind(const std::string &k) { return k == "FIXTURE" || k == "CLUTTER" || k == "CREATURE" || k == "PICKUP" || k == "LIGHT"; }
  std::optional<creation::Box> worldBoxLine(const std::string &line) const {
    auto f = NativeDocument::fields(line);
    auto box = worldBox(f);
    if (!box || f.empty() || !onTerrain() || !caveKind(f[0]) || namedValue(line, "cave") != "1") return box;
    auto [ox, oy] = mapOrigin();
    const float rise = caveFloorUnder(box->x + ox, box->y + oy) - groundAt(box->x, box->y);
    box->bottom += rise, box->top += rise;
    return box;
  }
  // Brings the ghost up to date with the pointer and the tool; tells the 3D view when it changed.
  void refreshGhost() {
    std::vector<std::string> lines;
    if (auto g = ghostGesture()) {
      if (ghostHold && (std::abs((*g)[2] - ghostHoldX) > .02f || std::abs((*g)[3] - ghostHoldY) > .02f)) ghostHold = false;
      if (!ghostHold) {
        const bool keepCave = placeInCave;
        if (!hoverInside && !drag && !viewRect) placeInCave = viewGhostCave;
        lines = ghostOf((*g)[0], (*g)[1], (*g)[2], (*g)[3]);
        placeInCave = keepCave;
      }
    }
    std::vector<retro::EditorGuide> guides;
    for (const auto &line : lines)
      if (auto s = ghostShapeOf(line)) {
        const size_t n = s->points.size();
        for (size_t i = 0; i < n; ++i) {
          if (!s->closed && i + 1 == n) break;
          auto a = s->points[i], b = s->points[(i + 1) % n];
          guides.push_back({a.first, a.second, s->top, b.first, b.second, s->top, 4});
          guides.push_back({a.first, a.second, s->bottom, b.first, b.second, s->bottom, 4});
          guides.push_back({a.first, a.second, s->bottom, a.first, a.second, s->top, 4});
        }
        if (!s->closed) {
          auto b = s->points.back();
          guides.push_back({b.first, b.second, s->bottom, b.first, b.second, s->top, 4});
        }
      }
    if (lines == ghostLines && guides.size() == ghostGuideList.size()) return;
    ghostLines = std::move(lines);
    ghostGuideList = std::move(guides);
    if (!session.empty() && !ghosting) writeControl();
    if (plan) InvalidateRect(plan, nullptr, FALSE);
  }
  void clearGhost() {
    ghostKey.clear();
    if (ghostLines.empty() && ghostGuideList.empty()) return;
    ghostLines.clear(), ghostGuideList.clear();
    if (!session.empty() && !ghosting) writeControl();
    if (plan) InvalidateRect(plan, nullptr, FALSE);
  }
  // A placement made: its ghost goes until the pointer moves on.
  void holdGhost(float x, float y) {
    ghostHold = true, ghostHoldX = x, ghostHoldY = y;
    clearGhost();
  }
  // Switching tools, maps or floors, Undo and Redo: a drag held on the plan or in 3D is dropped
  // (a release must not place with another tool or on another map) and its preview put away.
  void cancelPlacement() {
    const bool held = viewRect || (drag && !objectDrag && !brushing && !selectingArea);
    if (held) {
      viewRect = false, drag = false;
      if (!backgroundTest && plan && GetCapture() == plan) ReleaseCapture();
    }
    previewHeld = false, previewWanted = {-1, -1, -1, -1};
    placeInCave = false;
    dropGhost();
    ghostHold = false;
    clearGhost();
  }
  // A preview the throttle held back (the picture busy, or the last one under 120 ms ago) is
  // shown once it can be: the pointer standing still must not leave an older preview up.
  void catchUpPreview() {
    if (!previewHeld || previewWanted == previewShown || ghosting) return;
    if (!viewRect && !(drag && !objectDrag && !brushing)) { previewHeld = false; return; }
    previewGesture(previewWanted[0], previewWanted[1], previewWanted[2], previewWanted[3]);
  }
  // Where a ray from the 3D view meets the floor of a cave under the land (the camera in a cave,
  // or looking into its mouth). False: no cave there.
  bool caveRay(float ox, float oy, float oz, float dx, float dy, float dz, float &x, float &y) const {
    if (layer != 0 || !onTerrain()) return false;
    const auto *shape = campaignTerrain();
    if (!shape || !shape->caveVolume) return false;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f) return false;
    dx /= len, dy /= len, dz /= len;
    auto [mx, my] = mapOrigin();
    bool underground = false;
    for (float t = .05f; t < 80.f; t += .08f) {
      const float px = ox + dx * t, py = oy + dy * t, pz = oz + dz * t;
      if (px < 0 || py < 0 || px >= 24 || py >= 24) continue;
      const float land = groundAt(px, py), floor = caveFloorUnder(px + mx, py + my);
      if (pz > land) { if (underground) return false; continue; } // out in the open: the land's ray
      underground = true;
      if (floor < land - .5f && pz <= floor) { x = px, y = py; return true; }
      if (floor >= land - .5f) return false; // solid rock, no cave here
    }
    return false;
  }
  // The point a 3D placing click lands on: a cave's floor when the ray is underground, else the
  // ground or the floor's plane (pointOnFloor). cave says which.
  bool placementPoint(float ox, float oy, float oz, float dx, float dy, float dz, float &x, float &y, bool &cave) const {
    cave = false;
    if (onTerrain() && layer == 0 && oz < groundAt(std::clamp(ox, 0.f, 23.99f), std::clamp(oy, 0.f, 23.99f)) - .1f && caveRay(ox, oy, oz, dx, dy, dz, x, y))
      return cave = true;
    return pointOnFloor(ox, oy, oz, dx, dy, dz, layerElevation(), x, y);
  }
  // The 3D view's pointer with a placing tool (hover.txt): the ghost follows it there too.
  void ghostHover(const std::string &text) {
    if (!ghostTool() || objectDrag || brushing) {
      if (viewGhostOn) viewGhostOn = false, refreshGhost();
      return;
    }
    std::istringstream in(text);
    std::string tag;
    float ox, oy, oz, dx, dy, dz, x, y;
    bool cave = false;
    const bool on = in >> tag >> ox >> oy >> oz >> dx >> dy >> dz && tag == "HOVER" && placementPoint(ox, oy, oz, dx, dy, dz, x, y, cave);
    if (on) {
      viewGhostX = std::clamp(x, 0.f, 23.99f), viewGhostY = std::clamp(y, 0.f, 23.99f), viewGhostCave = cave;
      if (!hoverInside && !drag && plan) InvalidateRect(plan, nullptr, FALSE);
    }
    viewGhostOn = on;
    refreshGhost();
  }
  // The plan's ghost: the outlines of what the next click or drag writes. False: nothing drawn.
  bool drawPlacementGhost(HDC dc, float ox, float oy, float cell, COLORREF colour) {
    if (ghostLines.empty()) return false;
    bool drawn = false;
    for (const auto &line : ghostLines) {
      auto f = NativeDocument::fields(line);
      if (f[0] == "PRIM" && f.size() > 9) {
        const auto s = primSize(f);
        drawPrimShape(dc, ox, oy, cell, f[2], number(f[3], 0), number(f[4], 0), int(number(f[9], 0)) & 3, s.w, s.d, colour,
                                                  PS_DASH, true, s.h);
        drawn = true;
        continue;
      }
      auto s = ghostShapeOf(line);
      if (!s) continue;
      std::vector<POINT> poly;
      for (auto [x, y] : s->points) poly.push_back({int(ox + x * cell), int(oy + y * cell)});
      if (s->closed) poly.push_back(poly.front());
      auto pen = ui::dashedPen(2, colour);
      auto old = SelectObject(dc, pen);
      LONG l = poly[0].x, r = l, t = poly[0].y, b = t;
      for (auto p : poly) l = std::min(l, p.x), r = std::max(r, p.x), t = std::min(t, p.y), b = std::max(b, p.y);
      if (r - l < 4 && b - t < 4) { // a point (a lamp, a sound): a ring where it goes
        auto brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Ellipse(dc, (l + r) / 2 - 6, (t + b) / 2 - 6, (l + r) / 2 + 7, (t + b) / 2 + 7);
        SelectObject(dc, brush);
      } else
        Polyline(dc, poly.data(), int(poly.size()));
      SelectObject(dc, old);
      DeleteObject(pen);
      drawn = true;
    }
    return drawn;
  }
