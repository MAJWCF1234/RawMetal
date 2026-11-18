  // Buildings on the land and raised floors (--ground-test; --editing-test runs it without pictures):
  // a hall indoors and the land east of it; a room drawn on the slope there, its floor painted
  // cell by cell, a storey above it, a 3D paint click on its floor; every way of standing on a
  // slope (pad, foundation, stilts, steps) and every floor height (lowest, average, highest,
  // typed) from the Footing rows; its edge pulled up in the 3D view (Raise, soft-snapped); the
  // foundation and edge trim painted; moved over the hill (it measures the ground again); a
  // patio laid on open land; a raised room indoors; one Ctrl+Z each, save/reopen exact, the
  // engine taking every step. GROUND_PROBE=1 reports the first part instead of judging it.
  // NativeGroundMethods.inc is what it checks; lab tube floors draws the same in the game.
  std::vector<std::string> groundPassed;
  // The engine's grounding of a box on a terrain map (World::settleExclusionFacilityOnTerrain).
  std::pair<float, float> settledSpan(const std::vector<std::string> &f) const {
    float x1 = number(f[2], 0), y1 = number(f[3], 0), x2 = number(f[4], 0), y2 = number(f[5], 0);
    return {number(f[6], 0) + groundHigh(x1, y1, x2, y2, false), number(f[7], 0) + groundHigh(x1, y1, x2, y2, true)};
  }
  // The land itself (footing pads and all), not a footing's floor over it.
  float landSurface(float x, float y) const {
    auto [ox, oy] = mapOrigin();
    return retro::authoredTerrainSurface(*campaignTerrain(), ox, oy, x, y);
  }
  void verifyGround(bool pictures) {
    auto require = [](bool okay, const std::string &why) { if (!okay) throw std::runtime_error("Ground regression: " + why); };
    auto &passed = groundPassed;
    passed.clear();
    auto folder = root / L".build" / L"interaction";
    fs::create_directories(folder);
    const bool probe = GetEnvironmentVariableW(L"GROUND_PROBE", nullptr, 0) > 0;
    std::ofstream probeReport;
    if (probe) probeReport.open(root / L"native-editor-ground-probe.txt");
    auto note = [&](const std::string &line) { if (probe) probeReport << line << '\n' << std::flush; };
    auto judge = [&](bool okay, const std::string &why) { if (probe) note(std::string(okay ? "ok   " : "BROKEN ") + why); else require(okay, why); };
    auto send = [&](HWND list, int id, int index) {
      SendMessageW(list, CB_SETCURSEL, index, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    auto type = [&](HWND box, int id, const std::wstring &text) {
      SetWindowTextW(box, text.c_str());
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, EN_KILLFOCUS), reinterpret_cast<LPARAM>(box));
    };
    auto shot = [&](const wchar_t *name) {
      if (!pictures) return;
      try { captureEditorWindow(folder / name); } catch (const std::exception &error) { passed.push_back(std::string("(no picture ") + utf8(name) + ": " + error.what() + ")"); }
    };

    NativeDocument blank;
    blank.newCampaign("Ground floors");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false, autoDetail = false, smartSnap = false, snap = 0;
    newFootingMode = 0, newFootingSnap = retro::FootingSnapMean, newFootingRaise = 0, newFootingSteps = true, footingAdvanced = false;
    map = layer = 0, record = -1, selectedRecords.clear();
    joinMapPassage = true;
    addAdjacentMap(3);
    require(map == 1, "the east map was not chosen");
    SendMessageW(window, WM_COMMAND, Outdoors, 0);
    require(onTerrain(), "Make outdoors gave the east map no land");
    // The steepest 7 x 6 footprint clear of the seam: where a building on a hill goes wrong.
    int bx = 6, by = 6;
    float worst = -1;
    for (int y = 2; y + 6 <= 22; ++y)
      for (int x = 6; x + 7 <= 22; ++x) {
        float spread = groundHigh(float(x + 1), float(y + 1), float(x + 6), float(y + 5), true) - groundHigh(float(x + 1), float(y + 1), float(x + 6), float(y + 5), false);
        if (spread > worst) worst = spread, bx = x, by = y;
      }
    note("footprint " + std::to_string(bx) + "," + std::to_string(by) + " ground spread " + shortNumber(worst));
    require(worst > .8f, "no slope on the land to build on (" + shortNumber(worst) + ")");

    // ---- 1. The Room tool on the slope, then the Floor tool with a material, a cell at a time.
    tool = 1;
    gesture(bx + .5f, by + .5f, bx + 6.5f, by + 5.5f);
    size_t footingLine = 0;
    for (auto i : doc.records("STRUCT", map))
      if (footingOf(doc.lines[i])) footingLine = i;
    judge(footingLine > 0, "the Room tool on the slope stands the room on a footing");
    tool = 2, brush = 0, floorPaintMaterial = 50, floorMaterialExplicit = true;
    for (int y = by + 1; y < by + 5; ++y)
      for (int x = bx + 1; x < bx + 6; ++x)
        gesture(x + .5f, y + .5f, x + .5f, y + .5f);
    publish();
    auto roomFloor = [&] {
      float low = 1e9f, high = -1e9f;
      int slabs = 0;
      for (auto i : doc.records("STRUCT", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (!nativeFloorSlab(f) || footingOf(doc.lines[i])) continue;
        float cx = (number(f[2], 0) + number(f[4], 0)) * .5f, cy = (number(f[3], 0) + number(f[5], 0)) * .5f;
        if (cx < bx + 1 || cx > bx + 6 || cy < by + 1 || cy > by + 5) continue;
        auto [b, t] = settledSpan(f);
        low = std::min(low, t), high = std::max(high, t), ++slabs;
      }
      return std::tuple<float, float, int>{low, high, slabs};
    };
    auto [low, high, slabs] = roomFloor();
    note("brush floor: " + std::to_string(slabs) + " slabs, settled tops " + shortNumber(low) + " to " + shortNumber(high));
    judge(slabs == 20 && high - low < .01f, "a floor painted cell by cell on the slope is one level (" + shortNumber(high - low) + " m of steps)");
    float above = -1e9f;
    for (float y = by + 1.1f; y < by + 5; y += .25f)
      for (float x = bx + 1.1f; x < bx + 6; x += .25f)
        above = std::max(above, landSurface(x, y) - high);
    judge(above < -.005f, "the land stays under the room's floor (" + shortNumber(above) + " m above it at most)");
    float wallLow = 1e9f, wallHigh = -1e9f;
    for (int y = by; y <= by + 5; ++y)
      for (int x = bx; x <= bx + 6; ++x)
        if (doc.tile(map, 0, x, y) == '#') {
          float top = groundAt(x + .5f, y + .5f) + 2.8f;
          wallLow = std::min(wallLow, top), wallHigh = std::max(wallHigh, top);
        }
    judge(wallHigh - wallLow < .01f, "the room's walls rise to one height (" + shortNumber(wallHigh - wallLow) + " m apart)");
    if (!probe) passed.push_back("A room drawn on a " + shortNumber(worst) + " m slope stands on a footing; its floor painted cell by cell is one level, the land stays under it, its walls rise to one height");

    // ---- 2. A 3D paint click on the room's floor paints the cell under the pointer.
    {
      const int cx = bx + 3, cy = by + 2;
      const float gz = groundAt(cx + .5f, cy + .5f);
      SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
      choosePaint({"mat-27", materialName(27), 27, 0});
      remember();
      const auto before = doc.text();
      paintRay(cx + .5f - 1.2f, cy + .5f - 1.2f, gz + 1.8f, 1.2f, 1.2f, -1.8f + .02f);
      publish();
      int hit = -1;
      for (auto i : doc.records("STRUCT", map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (nativeFloorSlab(f) && !footingOf(doc.lines[i]) && f[9] == "27") hit = int(std::floor(number(f[2], 0))) + 100 * int(std::floor(number(f[3], 0)));
      }
      judge(hit == cx + 100 * cy, "a 3D paint click on the floor paints that cell (aimed " + std::to_string(cx) + "," + std::to_string(cy) + ", painted " +
                                       (hit < 0 ? std::string("nothing") : std::to_string(hit % 100) + "," + std::to_string(hit / 100)) + ")");
      if (hit != cx + 100 * cy) doc.loadText(before), publish();
      if (!probe) passed.push_back("A 3D paint click on a floor on the land paints the cell under the pointer");
    }

    // ---- 3. A storey above the room: its decks stand at one height, where the 3D view bands it.
    tool = 0;
    createFloorLayer();
    refresh();
    layer = 1;
    for (int y = by + 1; y < by + 5; ++y)
      for (int x = bx + 1; x < bx + 4; ++x)
        doc.paint(map, 1, x, y, '=');
    publish();
    float storey = 0, deckLow = 1e9f, deckHigh = -1e9f;
    for (auto &a : floorLayers())
      if (a.id == 1) storey = a.elevation;
    for (int y = by + 1; y < by + 5; ++y) {
      float run = storey + groundHigh(float(bx + 1), float(y), float(bx + 4), float(y + 1), true);
      deckLow = std::min(deckLow, run), deckHigh = std::max(deckHigh, run);
    }
    judge(deckHigh - deckLow < .01f, "the storey's deck rows stand at one height (" + shortNumber(deckHigh - deckLow) + " m apart)");
    judge(std::abs(floorBandGround() + storey - deckHigh) < .05f, "the 3D view's band for the storey is where its deck is");
    layer = 0;
    refresh();
    if (probe) {
      doc.save(folder / L"ground-before.rmc");
      return;
    }
    passed.push_back("A storey over the room stands at one height (" + shortNumber(deckHigh) + " m), where the 3D view bands it");
    require(validateDocument(), "the engine refused the building on the land: " + utf8(notice));

    // ---- 4. The Footing rows on the picked footing: every way to stand on the slope.
    selectRecord(int(footingLine));
    layout();
    require(footingPicked() && shownControl(footingModeList), "a picked footing shows no Footing rows");
    shot(L"ground-footing-rows.bmp");
    auto planned = [&] { return *footingAtLine(size_t(record)); };
    const size_t history0 = history.size();
    const char *modeNames[] = {"", "pad", "foundation", "stilts", "stepped"};
    for (int mode : {2, 3, 4, 1}) {
      send(footingModeList, FootingModeList, mode);
      require(namedValue(doc.lines[size_t(record)], "footing") == modeNames[mode], std::string("choosing ") + modeNames[mode] + " did not write it: " + doc.lines[size_t(record)]);
      auto f = planned();
      require(!f.parts.empty(), "no plan for the footing");
      for (auto &p : f.parts) {
        // Nothing of the land above its floor's underside anywhere under it.
        float top = -1e9f, low = 1e9f;
        for (float y = p.y1 + .05f; y < p.y2; y += .3f)
          for (float x = p.x1 + .05f; x < p.x2; x += .3f) top = std::max(top, landSurface(x, y)), low = std::min(low, landSurface(x, y));
        require(top <= p.slab + .04f, std::string(modeNames[mode]) + ": the land rises " + shortNumber(top - p.slab) + " m into the floor");
        if (mode == 1)
          require(std::abs(low - p.level) < .04f && std::abs(top - p.level) < .04f, std::string(modeNames[mode]) + ": the pad is not level at its height (" + shortNumber(low) + " to " + shortNumber(top) + " for " + shortNumber(p.level) + ")");
      }
      if (mode == 4) require(f.parts.size() > 1, "steps made no terraces on a 7 m footprint");
      require(validateDocument(), std::string("the engine refused footing=") + modeNames[mode] + ": " + utf8(notice));
    }
    require(history.size() == history0 + 4, "each footing choice is not one Ctrl+Z");
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(namedValue(doc.lines[footingLine], "footing") == "stepped", "Ctrl+Z did not take back the last footing choice: " + doc.lines[footingLine]);
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(namedValue(doc.lines[footingLine], "footing") == "pad", "Ctrl+Y did not bring it back: " + doc.lines[footingLine]);
    selectRecord(int(footingLine));
    layout();
    passed.push_back("Pad, Foundation, Stilts and Steps from the footing list: written, planned with no land in any floor, the pad level at its height, terraces for steps; one Ctrl+Z each; engine-valid");

    // ---- 5. Where the floor stands: lowest, average, highest ground, or a typed height (Exact heights).
    SendMessageW(window, WM_COMMAND, FootingAdvancedToggle, 0);
    layout();
    require(footingAdvanced && shownControl(footingSnapList) && shownControl(footingHeightBox), "Exact heights did not show the height rows");
    float levels[3];
    for (int s = 0; s < 3; ++s) {
      send(footingSnapList, FootingSnapList, s);
      levels[s] = planned().parts.front().level;
    }
    require(levels[0] < levels[1] && levels[1] < levels[2], "lowest, average and highest ground do not order (" + shortNumber(levels[0]) + ", " + shortNumber(levels[1]) + ", " + shortNumber(levels[2]) + ")");
    type(footingHeightBox, FootingHeightBox, L"2.5");
    require(namedValue(doc.lines[size_t(record)], "snap") == "fixed" && std::abs(planned().parts.front().level - 2.5f) < .001f, "a typed height did not fix the floor's ground at 2.5: " + doc.lines[size_t(record)]);
    send(footingSnapList, FootingSnapList, 1);
    require(std::abs(planned().parts.front().level - levels[1]) < .001f, "back to the average ground, the footing did not measure it again");
    SendMessageW(window, WM_COMMAND, FootingAdvancedToggle, 0);
    passed.push_back("Floor height from the lowest (" + shortNumber(levels[0]) + "), average (" + shortNumber(levels[1]) + ") or highest (" + shortNumber(levels[2]) +
                     ") ground, or typed under Exact heights (2.5); and back");

    // ---- 6. Raise: the floor's edge pulled up in the 3D view, soft-snapped to half a storey.
    {
      std::vector<retro::EditorGuide> guides;
      require(footingGuides(guides), "a picked footing has no outline");
      const retro::EditorGuide *edge = nullptr;
      for (auto &g : guides)
        if (g.kind == 1) { edge = &g; break; }
      require(edge != nullptr, "a picked footing has no Raise handle");
      const float mx = (edge->ax + edge->bx) * .5f, my = (edge->ay + edge->by) * .5f, mz = edge->az;
      const float cx = (planned().spec.x1 + planned().spec.x2) * .5f, cy = (planned().spec.y1 + planned().spec.y2) * .5f;
      float ox = mx + (mx - cx) * .8f, oy = my + (my - cy) * .8f, oz = mz + 2.f;
      require(beginHeightDrag(ox, oy, oz, mx - ox, my - oy, mz - oz), "the Raise handle could not be grabbed in the 3D view");
      const size_t before = history.size();
      applyHeightDelta(1.33f);
      require(namedValue(doc.lines[size_t(record)], "raise") == "1.4", "pulling the edge up 1.33 m did not soft-snap to half a storey: " + doc.lines[size_t(record)]);
      finishDrag(false);
      require(history.size() == before + 1, "the pull is not one Ctrl+Z");
      require(validateDocument(), "the engine refused the raised footing: " + utf8(notice));
      shot(L"ground-raised-rows.bmp");
      passed.push_back("Raise: the floor's edge pulled up in the 3D view soft-snaps to half a storey (raise=1.4), one Ctrl+Z, engine-valid");
    }

    // ---- 7. The foundation and its edge trim, from the Part list and the Paint tool.
    {
      SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
      selectRecord(int(footingLine));
      layout();
      require(finishKind() == 2 && shownControl(finishPartList), "a picked footing shows no Part list");
      auto pick = [&](int material) {
        for (LRESULT i = 0; i < SendMessageW(materialList, CB_GETCOUNT, 0, 0); ++i)
          if (SendMessageW(materialList, CB_GETITEMDATA, i, 0) == material) { send(materialList, MaterialList, int(i)); return true; }
        return false;
      };
      send(finishPartList, FinishPartList, 1);
      require(pick(31) && namedValue(doc.lines[size_t(record)], "base") == "31", "the Part list did not make the foundation brick: " + doc.lines[size_t(record)]);
      send(finishPartList, FinishPartList, 2);
      require(pick(33) && namedValue(doc.lines[size_t(record)], "cap") == "33", "the Part list did not trim the edge in stone: " + doc.lines[size_t(record)]);
      shot(L"ground-foundation-parts.bmp");
      // The Paint tool on the plinth's side, from outside, under the floor.
      auto p = planned().parts.front();
      SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
      choosePaint({"mat-26", materialName(26), 26, 0});
      remember();
      paintRay(p.x1 - 2.f, (p.y1 + p.y2) * .5f, p.surface - .5f, 1.f, 0, 0);
      publish();
      require(namedValue(doc.lines[footingLine], "base") == "26", "a paint click on the foundation's side did not paint it: " + doc.lines[footingLine]);
      require(validateDocument(), "the engine refused the painted footing: " + utf8(notice));
      passed.push_back("The foundation (brick, then rock from a paint click on its side) and its edge trim (stone) from the Part list and the Paint tool");
    }

    // ---- 8. Moved over the hill, it measures the ground again.
    {
      SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
      selectRecord(int(footingLine));
      const float was = planned().parts.front().level;
      auto f = NativeDocument::fields(doc.lines[footingLine]);
      const auto keep = doc.text();
      float best = was;
      int shift = 0;
      for (int s = -5; s <= 5; ++s) {
        if (!s || number(f[2], 0) + s < 1 || number(f[4], 0) + s > 23) continue;
        auto moved = f;
        moved[2] = nativeNumber(number(f[2], 0) + s), moved[4] = nativeNumber(number(f[4], 0) + s);
        doc.replace(footingLine, moved);
        const float now = footingAtLine(footingLine)->parts.front().level;
        doc.loadText(keep);
        if (std::abs(now - was) > std::abs(best - was)) best = now, shift = s;
      }
      require(shift != 0, "moving the footing along the hill never changed its ground");
      remember();
      auto moved = f;
      moved[2] = nativeNumber(number(f[2], 0) + shift), moved[4] = nativeNumber(number(f[4], 0) + shift);
      doc.replace(footingLine, moved);
      publish();
      require(std::abs(footingAtLine(footingLine)->parts.front().level - best) < .001f, "the moved footing did not stand on the ground where it went");
      SendMessageW(window, WM_COMMAND, Undo, 0);
      require(std::abs(footingAtLine(footingLine)->parts.front().level - was) < .001f, "Ctrl+Z did not put the footing back on its ground");
      passed.push_back("Moved " + std::to_string(shift) + " m along the hill, the footing stands on the ground there (" + shortNumber(was) + " to " + shortNumber(best) + " m); Ctrl+Z puts it back");
    }

    // ---- 9. The Floor tool on open land: a patio laid level, cell by cell, away from the room.
    {
      selectRecord(-1);
      tool = 2, brush = 0, floorPaintMaterial = 42, floorMaterialExplicit = true;
      int px = bx > 10 ? 2 : 16, py = by > 10 ? 2 : 15;
      const size_t before = footings().size();
      for (int y = py; y < py + 3; ++y)
        for (int x = px; x < px + 4; ++x) gesture(x + .5f, y + .5f, x + .5f, y + .5f);
      publish();
      float lo = 1e9f, hi = -1e9f;
      int n = 0;
      for (auto &f : footings())
        if (f.map == map && f.spec.x1 >= px - .01f && f.spec.x2 <= px + 4.01f && f.spec.y1 >= py - .01f && f.spec.y2 <= py + 3.01f)
          for (auto &p : f.parts) lo = std::min(lo, p.surface), hi = std::max(hi, p.surface), ++n;
      require(footings().size() == before + 12 && n == 12 && hi - lo < .001f, "a patio painted cell by cell on the open land is not level (" + std::to_string(n) + " cells, " + shortNumber(hi - lo) + " m)");
      require(validateDocument(), "the engine refused the patio: " + utf8(notice));
      passed.push_back("The Floor tool on open land lays a patio level, cell by cell (12 cells at " + shortNumber(hi) + " m)");
    }

    // ---- 10. Indoors: a room drawn with a Raise stands on a base.
    {
      SendMessageW(mapList, LB_SETCURSEL, 0, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(MapList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(mapList));
      require(map == 0 && !onTerrain(), "the hall was not chosen");
      SendMessageW(window, WM_COMMAND, ToolBase + 1, 0);
      layout();
      layout();
      require(shownControl(footingRaiseBox), "the Room tool indoors shows no Raise row");
      type(footingRaiseBox, FootingRaiseBox, L"0.6");
      require(std::abs(newFootingRaise - .6f) < .001f, "the Raise row did not take 0.6");
      tool = 1;
      gesture(3.5f, 3.5f, 9.5f, 8.5f);
      size_t raised = 0;
      for (auto i : doc.records("STRUCT", 0))
        if (footingOf(doc.lines[i])) raised = i;
      require(raised > 0 && namedValue(doc.lines[raised], "raise") == "0.6", "a room drawn indoors with a Raise has no raised footing");
      auto f = *footingAtLine(raised);
      require(std::abs(f.parts.front().surface - (layerElevation() + .1f + .6f)) < .001f, "the raised room's floor is not 0.7 m up (" + shortNumber(f.parts.front().surface) + ")");
      newFootingRaise = 0;
      require(validateDocument(), "the engine refused the raised room: " + utf8(notice));
      passed.push_back("Indoors, a room drawn with Raise 0.6 stands on a base, its floor 0.7 m up; engine-valid");
    }

    // ---- 11. Save and reopen: every byte.
    auto saved = folder / L"ground-after.rmc";
    doc.save(saved);
    NativeDocument back;
    back.open(saved);
    require(back.text() == doc.text(), "save/reopen changed the buildings on the land");
    passed.push_back("Saved and reopened byte for byte (.build/interaction/ground-after.rmc)");
  }
