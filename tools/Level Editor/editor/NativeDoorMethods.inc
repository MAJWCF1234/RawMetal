// Doors that stand either way: the Door tool's fit and ghost, R to turn, and
// the picked door's boxes (how it opens, which way it stands, the side it
// swings to, the end it hangs from). The frame is NativeDoor (NativeObjects.h),
// the engine's own Door (src/world/World.h): DOOR field 21 axis, 22 hinge.
// Included inside struct Editor.
int doorTurn = 0;            // quarter turns R gave the Door tool's next door
bool doorToolSwings = false; // the Door tool fits a swinging door (else sliding)
HWND doorToolList = nullptr, doorKindList = nullptr, doorAxisList = nullptr, doorSwingList = nullptr,
     doorHingeList = nullptr, doorLockList = nullptr, doorStrengthList = nullptr, doorOpenSlider = nullptr;

// The door a Door click or drag fits. A drag runs along the gap: mostly down
// the plan, the door stands along Y. A click finds the gap under the pointer:
// open cells with wall tiles at both ends, up to four wide, left and right (a
// wall running east-west: along X) or above and below (along Y); the narrower
// wins. A click on a wall tile follows that wall. Then R's quarter turns.
NativeDoor plannedDoor(float ax, float ay, float bx, float by) const {
  NativeDoor d;
  d.swinging = doorToolSwings;
  auto inside = [](int x, int y) { return x >= 0 && y >= 0 && x < 24 && y < 24; };
  auto wall = [&](int x, int y) { return !inside(x, y) || doc.tile(map, layer, x, y) == '#'; };
  float dx = std::abs(bx - ax), dy = std::abs(by - ay);
  if (std::max(dx, dy) >= .9f) {
    d.alongY = dy > dx;
    float a0 = d.alongY ? std::min(ay, by) : std::min(ax, bx), a1 = d.alongY ? std::max(ay, by) : std::max(ax, bx);
    d.left = snapped(a0), d.right = snapped(a1);
    if (d.right - d.left < .9f)
      d.left = std::floor(a0), d.right = d.left + 1;
    d.line = std::floor(d.alongY ? ax : ay) + .5f;
  } else {
    int cx = cellOf(ax), cy = cellOf(ay);
    auto gap = [&](bool alongY, int &lo, int &hi) {
      auto open = [&](int v) { return alongY ? !wall(cx, v) : !wall(v, cy); };
      lo = hi = alongY ? cy : cx;
      if (!open(lo))
        return 99;
      while (hi - lo < 4 && open(lo - 1))
        --lo;
      while (hi - lo < 4 && open(hi + 1))
        ++hi;
      return !open(lo - 1) && !open(hi + 1) && hi - lo < 4 ? hi - lo + 1 : 99;
    };
    int xl = 0, xh = 0, yl = 0, yh = 0, across = gap(false, xl, xh), down = gap(true, yl, yh);
    if (across < 99 || down < 99) {
      d.alongY = down < across;
      d.left = float(d.alongY ? yl : xl), d.right = float((d.alongY ? yh : xh) + 1);
      d.line = (d.alongY ? cx : cy) + .5f;
    } else {
      bool rowWall = (inside(cx - 1, cy) && wall(cx - 1, cy)) || (inside(cx + 1, cy) && wall(cx + 1, cy));
      bool columnWall = (inside(cx, cy - 1) && wall(cx, cy - 1)) || (inside(cx, cy + 1) && wall(cx, cy + 1));
      d.alongY = wall(cx, cy) && columnWall && !rowWall;
      d.left = float(d.alongY ? cy : cx), d.right = d.left + 1, d.line = (d.alongY ? cx : cy) + .5f;
    }
  }
  for (int k = 0; k < (doorTurn & 3); ++k)
    d = turnedDoor(d);
  return d;
}
// Makes the planned door: one DOOR record on this floor, picked, one Undo.
void authorDoor(const NativeDoor &d) {
  if (d.right - d.left < .5f)
    return;
  if (d.alongY && d.seam) {
    message(L"An entry or exit door stays in a wall that runs east-west.");
    return;
  }
  std::vector<std::string> f = {"DOOR", std::to_string(map), "0", "1", "0",
                                nativeNumber(layerElevation() - bottomLayerElevation()), "0", "0", "0", "0", "-1"};
  storeNativeDoor(f, d);
  remember();
  addAuthored(f);
  auto added = doc.records("DOOR", map);
  record = added.empty() ? -1 : int(added.back());
  publish();
  if (record >= 0)
    selectRecord(record);
  message(std::wstring(L"Door fitted in a wall that runs ") + (d.alongY ? L"north-south." : L"east-west.") +
          L" R turns it; the boxes on the right choose how it opens.");
}
// R with the Door tool: the next door turns a quarter.
void turnDoorGhost() {
  doorTurn = (doorTurn + 1) & 3;
  if (plan)
    InvalidateRect(plan, nullptr, FALSE);
  message(doorTurn == 0 ? L"Door turned back: it follows the wall again."
                        : L"The next door turns a quarter (" + std::to_wstring(doorTurn * 90) +
                              L" degrees from the wall's way). R again turns it on.");
}
bool doorPicked() const {
  return record >= 0 && record < int(doc.lines.size()) && selectedRecords.size() <= 1 &&
         NativeDocument::fields(doc.lines[size_t(record)])[0] == "DOOR";
}
// Writes the picked door's new frame as one Undo; false when nothing changed.
bool setPickedDoor(const NativeDoor &d, const wchar_t *said) {
  if (!doorPicked())
    return false;
  if (!editableRecord(size_t(record))) {
    message(L"Unlock and show this floor before editing its content.");
    return false;
  }
  if (d.alongY && d.seam) {
    message(L"An entry or exit door stays in a wall that runs east-west: the game joins maps through it.");
    syncDoorControls();
    return false;
  }
  auto before = NativeDocument::fields(doc.lines[size_t(record)]);
  auto f = before;
  storeNativeDoor(f, d);
  if (f == before)
    return false;
  int picked = record;
  remember();
  doc.replace(size_t(picked), f);
  publish();
  selectRecord(picked);
  message(said);
  return true;
}
// R or Turn with a door picked: a quarter turn about its centre. Hinge and
// swing side turn with it, so four turns bring it home.
bool rotateDoor() {
  if (!doorPicked())
    return false;
  auto d = *nativeDoor(NativeDocument::fields(doc.lines[size_t(record)]));
  setPickedDoor(turnedDoor(d), L"Door turned a quarter. R turns it on; Ctrl+Z takes it back.");
  return true;
}
// Fills the picked door's boxes; the swing and hinge names follow its axis.
void syncDoorControls() {
  if (!doorKindList || !doorPicked())
    return;
  auto d = *nativeDoor(NativeDocument::fields(doc.lines[size_t(record)]));
  auto fill = [](HWND list, std::initializer_list<const wchar_t *> items, int selected) {
    SendMessageW(list, CB_RESETCONTENT, 0, 0);
    for (auto item : items)
      SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    SendMessageW(list, CB_SETCURSEL, selected, 0);
  };
  fill(doorKindList, {L"Slides", L"Swings"}, d.swinging ? 1 : 0);
  fill(doorAxisList, {L"Along X (east-west wall)", L"Along Y (north-south wall)"}, d.alongY ? 1 : 0);
  if (d.alongY)
    fill(doorSwingList, {L"Opens east (+x)", L"Opens west (-x)"}, d.swing < 0 ? 1 : 0),
        fill(doorHingeList, {L"North end (left)", L"South end (right)"}, d.hingeRight ? 1 : 0);
  else
    fill(doorSwingList, {L"Opens south (+y)", L"Opens north (-y)"}, d.swing < 0 ? 1 : 0),
        fill(doorHingeList, {L"West end (left)", L"East end (right)"}, d.hingeRight ? 1 : 0);
  if (doorLockList)
    fill(doorLockList, {L"None", L"Keycard reader", L"Key", L"Power release"}, std::clamp(d.lock, 0, 3));
  // A number typed in the strength box is left alone until the box is left.
  HWND focus = GetFocus();
  if (doorStrengthList && !(focus && (focus == doorStrengthList || GetParent(focus) == doorStrengthList))) {
    fill(doorStrengthList, {L"Never", L"150", L"400", L"1000"}, -1);
    setControlText(doorStrengthList, d.strength > 0 ? wide(trimmed(d.strength)).c_str() : L"Never");
  }
  if (doorOpenSlider) {
    SendMessageW(doorOpenSlider, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
    SendMessageW(doorOpenSlider, TBM_SETPOS, TRUE, LPARAM(std::lround(std::clamp(d.open, 0.f, 1.f) * 100)));
  }
}
// The Starts open slider, 0-100 % of a quarter turn (kept within Opens this
// far): written when the thumb is let go, one Undo. An ajar door does not start opening.
void doorOpenChanged(int code) {
  if (code != TB_ENDTRACK || !doorPicked())
    return;
  auto d = *nativeDoor(NativeDocument::fields(doc.lines[size_t(record)]));
  auto n = d;
  n.open = std::min(float(SendMessageW(doorOpenSlider, TBM_GETPOS, 0, 0)) / 100.f, std::clamp(d.maxOpen, 0.f, 1.f));
  if (n.open > 0)
    n.opening = false;
  setPickedDoor(n, n.open > 0 ? L"It starts ajar; the plan draws the leaf where it stands. Ctrl+Z takes it back."
                              : L"It starts shut. Ctrl+Z takes it back.");
}
void doorControlChanged(int id, int notification) {
  // Breaks down: a choice from the list, or a number typed and the box left.
  if (id == DoorStrengthList && (notification == CBN_SELCHANGE || notification == CBN_KILLFOCUS) && doorPicked()) {
    std::wstring text;
    if (notification == CBN_SELCHANGE) {
      auto n = SendMessageW(doorStrengthList, CB_GETCURSEL, 0, 0);
      if (n < 0)
        return;
      text.resize(size_t(std::max<LRESULT>(0, SendMessageW(doorStrengthList, CB_GETLBTEXTLEN, n, 0))) + 1);
      SendMessageW(doorStrengthList, CB_GETLBTEXT, n, reinterpret_cast<LPARAM>(text.data()));
      text.resize(wcslen(text.c_str()));
    } else
      text = label(doorStrengthList);
    auto d = *nativeDoor(NativeDocument::fields(doc.lines[size_t(record)]));
    auto n = d;
    n.strength = std::clamp(number(utf8(text), 0), 0.f, 10000.f);
    if (n.strength > 0 && !n.swinging)
      message(L"Only a swinging door is broken down. Set Opens by to Swings.");
    else
      setPickedDoor(n, n.strength > 0 ? L"A Brute, a Chimera or the Machine can break it down now. Ctrl+Z takes it back."
                                      : L"Nothing breaks it down now. Ctrl+Z takes it back.");
    syncDoorControls();
    return;
  }
  if (notification != CBN_SELCHANGE)
    return;
  if (id == DoorToolList) {
    doorToolSwings = listIndex(doorToolList) == 1;
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
    message(doorToolSwings ? L"The next door swings on a hinge. R turns it." : L"The next door slides. R turns it.");
    return;
  }
  if (!doorPicked())
    return;
  auto d = *nativeDoor(NativeDocument::fields(doc.lines[size_t(record)]));
  auto n = d;
  if (id == DoorKindList) {
    n.swinging = listIndex(doorKindList) == 1;
    setPickedDoor(n, n.swinging ? L"It swings on its hinge now. Ctrl+Z takes it back." : L"It slides now. Ctrl+Z takes it back.");
  } else if (id == DoorAxisList) {
    // The other way about its centre, the same width; hinge end and swing side keep their numbers.
    n.alongY = listIndex(doorAxisList) == 1;
    if (n.alongY != d.alongY) {
      auto c = d.centre();
      float half = (d.right - d.left) * .5f;
      n.left = n.along(c) - half, n.right = n.along(c) + half, n.line = n.across(c);
    }
    setPickedDoor(n, n.alongY ? L"It stands in a north-south wall now. Ctrl+Z takes it back."
                              : L"It stands in an east-west wall now. Ctrl+Z takes it back.");
  } else if (id == DoorSwingList) {
    float side = listIndex(doorSwingList) == 1 ? -1.f : 1.f;
    n.swing = side * std::abs(d.swing == 0 ? 1.f : d.swing);
    setPickedDoor(n, L"It opens to the other side now. Ctrl+Z takes it back.");
  } else if (id == DoorLockList) {
    n.lock = listIndex(doorLockList);
    setPickedDoor(n, n.lock == 0 ? L"No lock shown. Ctrl+Z takes it back."
                                 : L"The lock shows beside the latch: the plan marks it. Ctrl+Z takes it back.");
  } else if (id == DoorHingeList) {
    n.hingeRight = listIndex(doorHingeList) == 1;
    setPickedDoor(n, L"It hangs from the other end now. Ctrl+Z takes it back.");
  }
  syncDoorControls();
}
