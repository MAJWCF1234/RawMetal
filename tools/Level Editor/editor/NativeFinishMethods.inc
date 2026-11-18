  // ---- Finishes on stairs and balconies (map format 2 named fields).
  // STAIR tread= riser= stringer= and LAYER deck= edge= railing= hold structure
  // materials (World.h finishMaterialAllowed); the engine reads them in
  // CustomCampaign.cpp and builds them in World::buildLayers. The Finish list
  // and the Part list write them for a picked stair or the balcony tool's
  // floor; the Paint tool writes them for the part it is clicked on.
  HWND finishPartList = nullptr;
  int finishPart = 0, finishPartsMode = -1; // 0 tread / deck, 1 riser / edge, 2 stringer / railing
  static const char* finishKey(bool stair, int part) {
    static const char* s[] = {"tread", "riser", "stringer"};
    static const char* l[] = {"deck", "edge", "railing"};
    return (stair ? s : l)[std::clamp(part, 0, 2)];
  }
  static bool finishAllowed(int m) { return m >= 0 && m <= 51 && m != 1 && m != 4 && m != 6 && m != 9 && m != 17 && m != 30; }
  // The STAIR under a point (z < -999: anywhere up the column, a plan click).
  bool stairAt(float x, float y, float z, size_t& at, int& part) const {
    for (auto i : doc.records("STAIR", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 11) continue;
      float x1 = number(f[2], 0), y1 = number(f[3], 0), x2 = number(f[4], 0), y2 = number(f[5], 0), b = number(f[6], 0), t = number(f[7], 0);
      if (x1 > x2) std::swap(x1, x2);
      if (y1 > y2) std::swap(y1, y2);
      if (x < x1 - .02f || x > x2 + .02f || y < y1 - .02f || y > y2 + .02f) continue;
      const int steps = std::max(1, int(number(f[8], 1)));
      const bool alongY = f[9] == "1", ascending = f[10] == "1", rails = f.size() > 13 && f[13] == "1";
      float u = alongY ? (y - y1) / std::max(.01f, y2 - y1) : (x - x1) / std::max(.01f, x2 - x1);
      u = std::clamp(u, 0.f, .9999f);
      const int slot = std::min(steps - 1, int(u * steps));
      if (!ascending) u = 1 - u;
      float stepTop = b + (t - b) * float(std::min(steps - 1, int(u * steps)) + 1) / float(steps);
      if (onTerrain()) {
        // Outdoors each step box rises by the ground under it (World::settleExclusionFacilityOnTerrain).
        const float a0 = float(slot) / float(steps), a1 = float(slot + 1) / float(steps);
        const float fx1 = alongY ? x1 : x1 + (x2 - x1) * a0, fx2 = alongY ? x2 : x1 + (x2 - x1) * a1;
        const float fy1 = alongY ? y1 + (y2 - y1) * a0 : y1, fy2 = alongY ? y1 + (y2 - y1) * a1 : y2;
        b += groundHigh(fx1, fy1, fx2, fy2, false), stepTop += groundHigh(fx1, fy1, fx2, fy2, true);
      }
      if (z < -999) { at = i, part = 0; return true; }
      if (rails && z > stepTop + .05f && z < stepTop + 1.1f) {
        const float side = alongY ? std::min(x - x1, x2 - x) : std::min(y - y1, y2 - y);
        if (side < .12f) { at = i, part = 2; return true; }
      }
      if (z >= b - .05f && z <= stepTop + .05f) { at = i, part = z >= stepTop - .05f ? 0 : 1; return true; }
    }
    return false;
  }
  // The balcony deck (an upper floor's '=' tile) under a point. z < -999: the
  // floor being edited only. part: 0 the walking face, 1 the fascia, 2 a rail.
  bool deckAt(float x, float y, float z, int& layerId, int& part) const {
    if (x < 0 || y < 0 || x >= 24 || y >= 24) return false;
    const int cx = cellOf(x), cy = cellOf(y);
    for (auto a : floorLayers()) {
      if (a.thickness <= 0 || !a.visible || doc.tile(map, a.id, cx, cy) != '=') continue;
      if (onTerrain() && z >= -999) {
        // Outdoors a deck run rises by the highest ground under it, as the engine builds it.
        int r0 = cx, r1 = cx;
        while (r0 > 0 && doc.tile(map, a.id, r0 - 1, cy) == '=') --r0;
        while (r1 < 23 && doc.tile(map, a.id, r1 + 1, cy) == '=') ++r1;
        const float low = groundHigh(float(r0), float(cy), float(r1 + 1), float(cy + 1), false), high = groundHigh(float(r0), float(cy), float(r1 + 1), float(cy + 1), true);
        a.thickness += high - low, a.elevation += high;
      }
      if (z < -999) {
        if (a.id != layer) continue;
        layerId = a.id, part = 0;
        return true;
      }
      if (z >= a.elevation - a.thickness - .03f && z <= a.elevation + .03f) {
        layerId = a.id, part = z >= a.elevation - .06f ? 0 : 1;
        return true;
      }
      if (z > a.elevation + .03f && z < a.elevation + .6f) {
        const float fx = x - float(cx), fy = y - float(cy);
        auto open = [&](int nx, int ny) { return nx < 0 || ny < 0 || nx > 23 || ny > 23 || doc.tile(map, a.id, nx, ny) != '='; };
        if ((fx < .08f && open(cx - 1, cy)) || (fx > .92f && open(cx + 1, cy)) || (fy < .08f && open(cx, cy - 1)) || (fy > .92f && open(cx, cy + 1))) {
          layerId = a.id, part = 2;
          return true;
        }
      }
    }
    return false;
  }
  long long layerRecord(int id) const {
    for (auto i : doc.records("LAYER", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 2 && f[2] == std::to_string(id)) return (long long)i;
    }
    return -1;
  }
  // The finish a record names for a key, or -1.
  int finishOf(size_t at, const char* key) const {
    for (auto& [k, v] : NativeDocument::named(doc.lines[at]))
      if (k == key) return int(number(v, -1));
    return -1;
  }
  // Writes key=material on the record (FORMAT|2 goes in if it is not there);
  // keeps the picked record pointing at the same line. Returns where it is now.
  size_t setFinish(size_t at, const std::string& key, int material) {
    auto f = NativeDocument::fields(doc.lines[at]);
    auto n = NativeDocument::named(doc.lines[at]);
    bool found = false;
    for (auto& [k, v] : n)
      if (k == key) v = std::to_string(material), found = true;
    if (!found) n.emplace_back(key, std::to_string(material));
    const size_t now = doc.replaceNamed(at, f, n);
    // FORMAT|2 went in under the start marker: every record line moved down by one.
    if (now != at && record >= 0) record += int(now - at);
    return now;
  }
  bool finishSelection() const {
    if (record < 0 || size_t(record) >= doc.lines.size()) return false;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    return !f.empty() && f[0] == "STAIR";
  }
  bool balconyTool() const { return std::wstring(toolInfo[std::clamp(tool, 0, ToolCount - 1)].name) == L"Balcony"; }
  // The Finish list changed while a stair is picked or the balcony tool is out.
  bool applyFinishChoice(int material) {
    const bool stair = finishSelection();
    if (!stair && !balconyTool()) return false;
    if (!finishAllowed(material)) {
      message(L"That finish cannot go on " + std::wstring(stair ? L"stairs" : L"a balcony") + L"; pick another.");
      return true;
    }
    remember();
    const char* key = finishKey(stair, finishPart);
    if (stair) setFinish(size_t(record), key, material);
    else {
      auto at = layerRecord(layer);
      if (at < 0) { message(L"This floor has no LAYER record to hold a balcony finish."); return true; }
      setFinish(size_t(at), key, material);
    }
    publish();
    if (stair) fillFields(false);
    InvalidateRect(plan, nullptr, FALSE);
    static const wchar_t* stairParts[] = {L"treads", L"risers", L"stringers"};
    static const wchar_t* deckParts[] = {L"balcony decks on this floor", L"balcony edges on this floor", L"balcony rails on this floor"};
    message(L"The " + std::wstring((stair ? stairParts : deckParts)[std::clamp(finishPart, 0, 2)]) + L" are " + materialName(material) + L" now.");
    return true;
  }
  // The Finish list shows what the picked stair (or this floor's balconies) has for the chosen part.
  void showFinishChoice() {
    if (!materialList) return;
    int id = -1;
    if (finishSelection()) id = finishOf(size_t(record), finishKey(true, finishPart));
    else if (balconyTool()) {
      auto at = layerRecord(layer);
      if (at >= 0) id = finishOf(size_t(at), finishKey(false, finishPart));
    }
    if (id < 0) return;
    for (LRESULT i = 0; i < SendMessageW(materialList, CB_GETCOUNT, 0, 0); ++i)
      if (SendMessageW(materialList, CB_GETITEMDATA, i, 0) == id) { SendMessageW(materialList, CB_SETCURSEL, i, 0); break; }
  }
  void fillFinishParts(bool stair) {
    if (!finishPartList) return;
    if (finishPartsMode == int(stair) && SendMessageW(finishPartList, CB_GETCOUNT, 0, 0) == 3) {
      SendMessageW(finishPartList, CB_SETCURSEL, std::clamp(finishPart, 0, 2), 0);
      return;
    }
    finishPartsMode = int(stair);
    static const wchar_t* s[] = {L"Treads", L"Risers", L"Stringers (side rails)"};
    static const wchar_t* l[] = {L"Deck", L"Deck edge", L"Railing"};
    SendMessageW(finishPartList, CB_RESETCONTENT, 0, 0);
    for (auto* text : stair ? s : l) SendMessageW(finishPartList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(finishPartList, CB_SETCURSEL, std::clamp(finishPart, 0, 2), 0);
  }
  // The Paint tool on a stair or a balcony: the part under the pointer takes
  // the paint (pickUp: takes it up). Returns false when there is none there.
  bool paintFinishAt(float x, float y, float z, bool pickUp) {
    size_t at = 0;
    int part = 0, deckLayer = 0;
    bool stair = stairAt(x, y, z, at, part);
    if (!stair) {
      if (!deckAt(x, y, z, deckLayer, part)) return false;
      auto found = layerRecord(deckLayer);
      if (found < 0) return false;
      at = size_t(found);
    }
    const char* key = finishKey(stair, part);
    if (pickUp) {
      int id = finishOf(at, key);
      if (id < 0) id = stair ? (part == 2 ? 2 : 0) : 0;
      choosePaint({"mat-" + std::to_string(id), materialName(id), id, 0});
      sfx::play(sfx::Click, .7f);
      message(L"Picked up " + paintChoice.name + L". Click to paint with it.");
      return true;
    }
    if (paintIsFinish() || !finishAllowed(paintMaterial())) {
      message(L"Stairs and balconies take a material, not a room finish or this paint. Pick another.");
      return true;
    }
    setFinish(at, key, paintMaterial());
    static const wchar_t* stairParts[] = {L"Treads", L"Risers", L"Stringers"};
    static const wchar_t* deckParts[] = {L"Balcony deck", L"Balcony edge", L"Balcony rails"};
    message(std::wstring((stair ? stairParts : deckParts)[part]) + L" painted " + materialName(paintMaterial()) +
            (stair ? L"." : L" (every balcony on that floor)."));
    return true;
  }
