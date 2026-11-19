  // ---- Nudging the picked thing with the keyboard.
  // The arrow keys move it along the floor, Page Up and Page Down raise and lower it, a grid step
  // a press (the grid box; 25 cm when the grid is off). Shift: a fine step (1 cm). Ctrl: a big one
  // (1 m). On the plan, Up is north. In the 3D view the arrows follow the camera: Up moves it
  // away from you on screen, along the map's axis nearest the way the camera looks. It goes the
  // way a drag moves it (applyDragDelta, applyBodyElevation): whatever is picked with it, a
  // person's poses, and what stands on a footing floor go too, and a footing measures the ground
  // again where it lands. Each press is one Undo; a held key's repeats are one. The floor (or the
  // ground) stops a lowering once, softly: press again and it sinks.
  ULONGLONG lastNudgeAt = 0;
  int lastNudgeRecord = -1;
  std::string lastNudgeLine;
  bool pointerIn3D = false; // hover.txt: the pointer is over the 3D view
  bool nudgeIn3D() const { return viewMode == 2 || (pointerIn3D && !hoverInside); }
  // The camera's turn in the 3D view (film-status.txt, degrees), if the view has said.
  std::optional<float> viewYaw() const {
    if (auto v = fmViewCamera()) return (*v)[3];
    return std::nullopt;
  }
  // The map axis nearest a direction: (±1, 0) or (0, ±1).
  static std::pair<float, float> nearestAxis(float x, float y) {
    if (std::abs(x) >= std::abs(y)) return {x < 0 ? -1.f : 1.f, 0.f};
    return {0.f, y < 0 ? -1.f : 1.f};
  }
  float nudgeStep(bool ctrl, bool shift) const {
    if (ctrl) return 1.f;
    if (shift) return .01f;
    return snap > 0 ? snap : .25f;
  }
  static std::wstring centimetres(float metres) {
    const float cm = std::round(std::abs(metres) * 1000.f) / 10.f;
    wchar_t text[32];
    if (cm >= 100) swprintf(text, 32, L"%.2f m", double(cm / 100.f));
    else if (cm == std::floor(cm)) swprintf(text, 32, L"%.0f cm", double(cm));
    else swprintf(text, 32, L"%.1f cm", double(cm));
    return text;
  }
  // The floor or ground under the thing (where a lowering stops once).
  float nudgeFloor(float x, float y) const { return onTerrain() && layer == 0 ? groundAt(std::clamp(x, 0.f, 23.99f), std::clamp(y, 0.f, 23.99f)) : layerElevation(); }
  // Arrow keys and Page Up / Page Down with something picked. False: not a nudge (nothing picked,
  // or another key), and the key does what it did before.
  bool nudgeKey(const MSG &msg, bool ctrl, bool shift) {
    const auto key = msg.wParam;
    if (key != VK_LEFT && key != VK_RIGHT && key != VK_UP && key != VK_DOWN && key != VK_PRIOR && key != VK_NEXT) return false;
    if (record < 0 || size_t(record) >= doc.lines.size() || floorTransformSelection || mapHeightSelection) return false;
    if (!handleFor(size_t(record))) return false;
    if (!editableRecord(size_t(record))) {
      message(L"Unlock and show this floor before moving its content.");
      return true;
    }
    const float step = nudgeStep(ctrl, shift);
    float dx = 0, dy = 0, dz = 0;
    std::wstring way;
    if (key == VK_PRIOR) dz = step, way = L"up";
    else if (key == VK_NEXT) dz = -step, way = L"down";
    else {
      // Up: away from you. On the plan that is north (-y); in 3D the camera's forward.
      std::pair<float, float> forward{0.f, -1.f}, right{1.f, 0.f};
      bool view = false;
      if (nudgeIn3D())
        if (auto yaw = viewYaw()) {
          const float a = *yaw * 3.14159265f / 180.f;
          forward = nearestAxis(std::cos(a), std::sin(a));
          right = nearestAxis(-std::sin(a), std::cos(a));
          view = true;
        }
      auto [ux, uy] = key == VK_UP ? forward : key == VK_DOWN ? std::pair{-forward.first, -forward.second} : key == VK_RIGHT ? right : std::pair{-right.first, -right.second};
      dx = ux * step, dy = uy * step;
      if (view) way = key == VK_UP ? L"away" : key == VK_DOWN ? L"toward you" : key == VK_RIGHT ? L"right" : L"left";
      else way = uy < 0 ? L"north" : uy > 0 ? L"south" : ux > 0 ? L"east" : L"west";
    }
    // Lowering stops at the floor once (softly): from there another press sinks it on purpose.
    const auto line = doc.lines[size_t(record)];
    auto box = worldBoxLine(line);
    bool stopped = false;
    if (dz < 0 && box) {
      const float floor = nudgeFloor(box->x, box->y);
      if (box->bottom > floor + .001f && box->bottom + dz < floor - .001f) dz = floor - box->bottom, stopped = true;
    }
    // A held key's repeats join the press before them in one Undo.
    const bool repeat = (msg.lParam & 0x40000000) != 0 && lastNudgeRecord == record && lastNudgeLine == line && GetTickCount64() - lastNudgeAt < 700;
    const size_t historyBefore = history.size();
    objectOriginal = line;
    objectFields = NativeDocument::fields(line);
    kind = schemaFor(objectFields);
    if (kind < 0) return false;
    captureDragState();
    objectDrag = true, objectMoved = false, resizeHandle = -1;
    viewDrag = bodyDragging = verticalDragging = false;
    captureMates();
    // A footing floor takes what stands on it along (things whose spot is on its footprint).
    if (footingOf(line) && selectedRecords.size() <= 1) {
      const auto f = objectFields;
      const float x1 = std::min(number(f[2], 0), number(f[4], 0)), x2 = std::max(number(f[2], 0), number(f[4], 0));
      const float y1 = std::min(number(f[3], 0), number(f[5], 0)), y2 = std::max(number(f[3], 0), number(f[5], 0));
      for (auto i : objects()) {
        if (int(i) == record || !activeFloorRecord(i)) continue;
        auto h = handleFor(i);
        if (!h || h->x < x1 || h->x > x2 || h->y < y1 || h->y > y2) continue;
        if (footingOf(doc.lines[i])) continue;
        if (std::none_of(dragSelection.begin(), dragSelection.end(), [&](auto &m) { return m.first == i; })) dragSelection.emplace_back(i, doc.lines[i]);
      }
    }
    try {
      if (dx != 0 || dy != 0) applyDragDelta(dx, dy);
      if (dz != 0) {
        if (dx != 0 || dy != 0) objectOriginal = doc.lines[size_t(record)];
        applyBodyElevation(dz);
      }
    } catch (const std::exception &error) {
      finishDrag(true);
      message(wide(error.what()));
      return true;
    }
    if (!objectMoved) {
      finishDrag(true);
      message(L"It does not move that way.");
      return true;
    }
    finishDrag(false);
    if (repeat && history.size() == historyBefore + 1 && history.size() >= 2) history.pop_back();
    lastNudgeAt = GetTickCount64(), lastNudgeRecord = record;
    lastNudgeLine = record >= 0 && size_t(record) < doc.lines.size() ? doc.lines[size_t(record)] : std::string();
    // Where it is now, in plain words.
    std::wstring where;
    if (record >= 0 && size_t(record) < doc.lines.size()) {
      auto h = handleFor(size_t(record));
      auto b = worldBoxLine(doc.lines[size_t(record)]);
      wchar_t text[96];
      if (h && b) swprintf(text, 96, L" Now at %.2f, %.2f, %.2f m up.", double(h->x), double(h->y), double(b->bottom - nudgeFloor(h->x, h->y)));
      else if (h) swprintf(text, 96, L" Now at %.2f, %.2f.", double(h->x), double(h->y));
      else text[0] = 0;
      where = text;
    }
    const float moved = dz != 0 ? std::abs(dz) : step;
    message(L"Moved " + way + L" " + centimetres(moved) + L"." + where +
            (stopped ? L" It stopped at the floor; press again to sink it." : L""));
    return true;
  }
