  // ---- The Flooring tool: pick a floor look, drag it out. Indoors it is laid on the
  // floor (or a storey's deck); on the land it follows the ground (STRUCT drape=1) or is
  // made level (a footing pad, Footing.h), each with an optional edge trim (trim=, or the
  // pad's cap=). A click in a room floors the whole room. Everything it lays is a STRUCT:
  // picked, moved, painted (the Paint tool, or the Part list's Floor and Edge trim) and
  // saved like any other. While the drag is held the 3D view shows it laid (previewGesture).
  struct FloorLook {
    const wchar_t *name;
    int material, trim;
    float thickness;
  };
  static constexpr FloorLook floorLooks[] = {
      {L"Wood planks", 51, 32, .03f},    {L"Floor tiles", 50, 33, .02f},  {L"Clinic tiles", 27, 20, .02f},  {L"Office tiles", 24, 20, .02f},
      {L"Carpet", 23, 12, .02f},         {L"Concrete slab", 42, 15, .12f}, {L"Stone paving", 33, 31, .06f}, {L"Brick paving", 31, 33, .06f},
      {L"Metal grating", 7, 20, .04f},   {L"Tread plate", 46, 20, .03f},
  };
  HWND flooringLookList = nullptr, flooringSlopeList = nullptr;
  int flooringLook = 0;
  bool flooringTrim = true, flooringLevel = false; // on the land: follow the ground (false) or make it level
  void createFlooringControls() {
    flooringLookList = controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, FlooringLookList);
    for (auto &look : floorLooks) {
      auto at = SendMessageW(flooringLookList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(look.name));
      SendMessageW(flooringLookList, CB_SETITEMDATA, at, look.material);
    }
    SendMessageW(flooringLookList, CB_SETCURSEL, 0, 0);
    flooringSlopeList = controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, FlooringSlopeList);
    for (auto *text : {L"Follows the ground", L"Made level"})
      SendMessageW(flooringSlopeList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(flooringSlopeList, CB_SETCURSEL, 0, 0);
    button(L"Edge trim: on", FlooringTrimToggle);
  }
  template <class Row> void flooringRows(Row &&row) {
    SendMessageW(flooringLookList, CB_SETCURSEL, std::clamp(flooringLook, 0, int(std::size(floorLooks)) - 1), 0);
    SetWindowTextW(GetDlgItem(window, FlooringTrimToggle), flooringTrim ? L"Edge trim: on" : L"Edge trim: off");
    row(flooringLookList, L"Floor look");
    if (layer == 0 && onTerrain()) row(flooringSlopeList, L"On a slope");
    row(GetDlgItem(window, FlooringTrimToggle), L"", 30);
  }
  bool flooringCommand(int id, int notification) {
    if (id == FlooringLookList && notification == CBN_SELCHANGE) {
      flooringLook = std::clamp(int(SendMessageW(flooringLookList, CB_GETCURSEL, 0, 0)), 0, int(std::size(floorLooks)) - 1);
      message(std::wstring(floorLooks[size_t(flooringLook)].name) + L": drag it out over a floor or the ground. A click floors a whole room.");
      return true;
    }
    if (id == FlooringSlopeList && notification == CBN_SELCHANGE) {
      flooringLevel = SendMessageW(flooringSlopeList, CB_GETCURSEL, 0, 0) == 1;
      message(flooringLevel ? L"On a slope it is laid level: the ground is cut and filled under it." : L"On a slope it follows the ground, like a path.");
      return true;
    }
    if (id == FlooringTrimToggle) {
      flooringTrim = !flooringTrim;
      relayout();
      return true;
    }
    return false;
  }
  // A soft snap: half metres, and the inside face of a wall close by.
  float flooringSnap(float v, bool alongX, float across) const {
    float r = std::round(v * 2.f) * .5f;
    if (std::abs(r - v) < .2f) v = r;
    const int line = int(std::round(v));
    if (std::abs(v - float(line)) < .3f) {
      const int c = std::clamp(int(std::floor(across)), 0, 23);
      auto wall = [&](int cell) { return cell >= 0 && cell < 24 && (alongX ? doc.tile(map, layer, cell, c) : doc.tile(map, layer, c, cell)) == '#'; };
      if (wall(line - 1) != wall(line)) v = float(line);
    }
    return std::clamp(v, 0.f, 24.f);
  }
  // The flooring a drag (or a click in a room) lays. Returns the record, or -1.
  int layFlooring(float ax, float ay, float bx, float by) {
    const auto &look = floorLooks[size_t(std::clamp(flooringLook, 0, int(std::size(floorLooks)) - 1))];
    float x0 = std::min(ax, bx), x1 = std::max(ax, bx), y0 = std::min(ay, by), y1 = std::max(ay, by);
    if (x1 - x0 < .3f && y1 - y0 < .3f) {
      // A click: the room it is in, when the room is a rectangle.
      auto cells = roomAt(cellOf(ax), cellOf(ay));
      if (cells.empty()) { message(L"Drag the flooring out, or click inside a room."); return -1; }
      int cx0 = 24, cy0 = 24, cx1 = -1, cy1 = -1;
      for (int c : cells) cx0 = std::min(cx0, c % 24), cx1 = std::max(cx1, c % 24), cy0 = std::min(cy0, c / 24), cy1 = std::max(cy1, c / 24);
      if (int(cells.size()) != (cx1 - cx0 + 1) * (cy1 - cy0 + 1)) { message(L"This room is not a rectangle: drag the flooring out instead."); return -1; }
      x0 = float(cx0), y0 = float(cy0), x1 = float(cx1 + 1), y1 = float(cy1 + 1);
    } else {
      x0 = flooringSnap(x0, true, (y0 + y1) * .5f), x1 = flooringSnap(x1, true, (y0 + y1) * .5f);
      y0 = flooringSnap(y0, false, (x0 + x1) * .5f), y1 = flooringSnap(y1, false, (x0 + x1) * .5f);
    }
    if (x1 - x0 < .2f || y1 - y0 < .2f) { message(L"Drag it out a little wider."); return -1; }
    const bool land = layer == 0 && onTerrain() && !footingPartAt(map, (x0 + x1) * .5f, (y0 + y1) * .5f);
    const float z = land || (layer == 0 && onTerrain()) ? 0.f : layerElevation();
    std::vector<std::string> f{"STRUCT", std::to_string(map), nativeNumber(x0), nativeNumber(y0), nativeNumber(x1), nativeNumber(y1),
                               nativeNumber(z), nativeNumber(z + (land && flooringLevel ? std::max(.1f, look.thickness) : look.thickness)), "0", std::to_string(look.material)};
    if (land && flooringLevel) {
      f.push_back("footing=pad");
      if (flooringTrim) f.push_back("cap=" + std::to_string(look.trim)), f.push_back("capwidth=0.12");
    } else {
      if (land) f.push_back("drape=1");
      if (flooringTrim && x1 - x0 > .5f && y1 - y0 > .5f) f.push_back("trim=" + std::to_string(look.trim)), f.push_back("trimwidth=0.1");
    }
    remember();
    doc.ensureFormat2();
    doc.addOnLayer(f, layer);
    publish();
    const int at = int(doc.records("STRUCT", map).back());
    if (!ghosting) selectRecord(at);
    message(std::wstring(look.name) + L" laid" + (land ? (flooringLevel ? L", made level." : L", following the ground.") : L".") + L" Paint it or its edge trim to change them.");
    return at;
  }
  // A flooring laid with this tool (or a footing): painted whole, never cut into cells.
  static bool isFlooring(const std::string &line) {
    return !namedValue(line, "trim").empty() || !namedValue(line, "drape").empty() || !namedValue(line, "footing").empty();
  }
  // A picked flooring with an edge trim: the Part list's Floor and Edge trim.
  bool flooringPicked() const {
    if (record < 0 || size_t(record) >= doc.lines.size()) return false;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    return !f.empty() && f[0] == "STRUCT" && !namedValue(doc.lines[size_t(record)], "trim").empty();
  }
  // The Paint tool near a flooring's edge paints its trim; elsewhere its floor (paintSpot).
  bool paintTrimAt(float x, float y, float z, bool pickUp) {
    size_t at = 0, field = 0;
    if (z < -999 || !thingAt(x, y, z, at, field)) return false;
    const auto trim = namedValue(doc.lines[at], "trim");
    if (trim.empty()) return false;
    auto f = NativeDocument::fields(doc.lines[at]);
    const float width = std::max(.02f, number(namedValue(doc.lines[at], "trimwidth"), .1f));
    const float x1 = std::min(number(f[2], 0), number(f[4], 0)), x2 = std::max(number(f[2], 0), number(f[4], 0));
    const float y1 = std::min(number(f[3], 0), number(f[5], 0)), y2 = std::max(number(f[3], 0), number(f[5], 0));
    if (std::min({x - x1, x2 - x, y - y1, y2 - y}) > width + .02f) return false;
    if (pickUp) {
      const int id = int(number(trim, 0));
      choosePaint({"mat-" + std::to_string(id), materialName(id), id, 0});
      message(L"Picked up " + paintChoice.name + L". Click to paint with it.");
      return true;
    }
    if (paintIsFinish() || !finishAllowed(paintMaterial())) { message(L"Edge trim takes a material. Pick another."); return true; }
    setFinish(at, "trim", paintMaterial());
    message(L"Edge trim painted " + materialName(paintMaterial()) + L".");
    return true;
  }
