  // ---- Breach: a hole broken through a wall (BREACH; the engine's src/world/Openings.cpp) ----
  // Drag along a wall (a run of # tiles or a wall STRUCT, read as the Window wall tool reads it). One
  // BREACH record goes on the wall's box; the engine cuts the hole when it plays the map (the tiles in
  // the document never change), builds the wall back round it, chips its edge, hangs rebar from the
  // lip and drops rubble either side. You walk through it, see and shoot through it, light comes
  // through it, and the creatures' paths go through it. Delete the record and the wall is whole.
  HWND breachShapeList = nullptr, breachEdgeList = nullptr, breachSizeList = nullptr, breachRubbleList = nullptr;
  // The breach's named fields from the tool boxes; false (and a message) when the drag is on no wall.
  bool breachFields(std::vector<std::string> &f, float ax, float ay, float bx, float by, float floorZ) {
    auto w = wallRunAt(ax, ay, bx, by);
    if (w.kind != WallRun::Tiles && w.kind != WallRun::Box) {
      message(L"A breach goes through a wall: drag along one (# tiles or a wall block).");
      return false;
    }
    const float top = creationCeiling(), height = top - floorZ;
    glazingBox(f, w, floorZ, top);
    static constexpr const char *shapes[] = {"ragged", "arch", "rect"};
    static constexpr const char *edges[] = {"concrete", "concrete", "brick", "clean"};
    // Sizes: walk through, crawl through, a window's hole, floor to ceiling.
    static constexpr float sills[] = {0.f, 0.f, 1.f, 0.f}, heads[] = {2.3f, 1.2f, 2.1f, 99.f};
    static constexpr int rubble[] = {0, 10, 24};
    const int size = std::clamp(listIndex(breachSizeList), 0, 3), edge = std::clamp(listIndex(breachEdgeList), 0, 3);
    std::vector<std::pair<std::string, std::string>> named;
    named.push_back({"shape", shapes[std::clamp(listIndex(breachShapeList), 0, 2)]});
    if (edge)
      named.push_back({"edge", edges[edge]});
    if (edge == 1)
      named.push_back({"rebar", "0"});
    if (sills[size] > 0)
      named.push_back({"sill", shortNumber(sills[size])});
    named.push_back({"head", shortNumber(std::min(heads[size], height - .05f))});
    const int rubbleCount = rubble[std::clamp(listIndex(breachRubbleList, 1), 0, 2)];
    if (rubbleCount != 10)
      named.push_back({"rubble", std::to_string(rubbleCount)});
    named.push_back({"seed", std::to_string(1 + int(doc.records("BREACH", map).size()))});
    doc.ensureFormat2();
    for (auto &[k, v] : named)
      f.push_back(k + "=" + v);
    return true;
  }
  void breachControlChanged(int id, int notification) {
    if (notification != CBN_SELCHANGE)
      return;
    if (id == BreachShapeList) {
      const wchar_t *how[] = {L"Ragged: knocked through, tall in the middle, falling away to the sides.", L"Arch: a round-topped hole.",
                              L"Square: a clean rectangular hole."};
      message(how[std::clamp(listIndex(breachShapeList), 0, 2)]);
    } else if (id == BreachEdgeList)
      message(L"The broken edge: concrete with rebar, bare concrete, brick, or a clean cut.");
    relayout();
  }
  template <class Row> void breachRows(Row &&row) {
    row(breachShapeList, L"Shape");
    row(breachSizeList, L"Size");
    row(breachEdgeList, L"Edge");
    row(breachRubbleList, L"Rubble");
  }
  std::vector<HWND> breachControls() const { return {breachShapeList, breachEdgeList, breachSizeList, breachRubbleList}; }
  template <class Choices> void makeBreachControls(Choices &&choices) {
    breachShapeList = choices(BreachShapeList, {L"Ragged", L"Arch", L"Square"}, 0);
    breachSizeList = choices(BreachSizeList, {L"Walk through (2.3 m)", L"Crawl through (1.2 m)", L"Window hole (1 - 2.1 m)", L"Floor to ceiling"}, 0);
    breachEdgeList = choices(BreachEdgeList, {L"Concrete and rebar", L"Bare concrete", L"Brick", L"Clean cut"}, 0);
    breachRubbleList = choices(BreachRubbleList, {L"No rubble", L"Some rubble", L"Lots of rubble"}, 1);
  }
  // Whether a drag lies along a wall a breach can go through.
  bool breachOnWall(float ax, float ay, float bx, float by) const {
    auto w = wallRunAt(ax, ay, bx, by);
    return w.kind == WallRun::Tiles || w.kind == WallRun::Box;
  }
