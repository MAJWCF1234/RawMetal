  // ---- The controller (an Xbox-style pad on XInput) drives the whole Builder.
  // Made with an artist who has cerebral palsy: every action is ONE press (no button held with another), holds are
  // toggles unless the player asks for holds, the targets are big and the focus always shows, the sticks' speed,
  // dead zone and acceleration are the player's own (saved in .build/pad-settings.txt), a slow precise mode is a
  // toggle, and a held button repeats only after a generous pause.
  //
  // On the map (the plan, and the same cursor in the 3D view as a post):
  //   Left stick   the cursor (it carries what is grabbed)       Right stick   left/right turns the picked thing
  //   A            click: pick, place, use the tool               smoothly; up/down raises and lowers it
  //   X            grab / drop: move the picked thing, drag a     Y             turn by the turn step (90/45/15/5)
  //                room or wall out, pull a corner to scale       LT / RT       shrink / grow the picked model whole
  //   B            cancel; put the tool down                      LB / RB       Undo / Redo
  //   D-pad        left/right catalogue tabs, up/down items       Start         play from the 3D camera (Start again
  //   View         the controller menu: panels, views, turn step,              in play comes back)
  //                precise mode, the stick settings               Left stick click (optional): precise mode
  // In the menu's "Panels" every button, list, slider and inspector field is reached with the D-pad (a ring shows
  // which), left/right changes its value, A presses it, B comes back to the map.
  static constexpr UINT_PTR PadTimer = 4242;
  struct Vec2f { float x, y; };
  static constexpr WORD PadUp = 0x0001, PadDown = 0x0002, PadLeft = 0x0004, PadRight = 0x0008, PadStart = 0x0010, PadBack = 0x0020,
                        PadL3 = 0x0040, PadR3 = 0x0080, PadLB = 0x0100, PadRB = 0x0200, PadA = 0x1000, PadB = 0x2000, PadX = 0x4000, PadY = 0x8000;
  struct PadSample { bool connected = true; WORD buttons = 0; BYTE lt = 0, rt = 0; SHORT lx = 0, ly = 0, rx = 0, ry = 0; };
  // The player's settings: stick speed 0-4, dead zone 0-3, acceleration 0-3, repeat delay 0-2, grab held or toggled.
  struct PadSettings { int speed = 2, dead = 1, accel = 1, repeat = 1; bool holdGrab = false; };
  PadSettings padSettings;
  bool padPrecise = false, padGrab = false, padCursorShown = false, padMenuOpen = false, padPanels = false, padTurning = false, padScaling = false;
  float padX = 12, padY = 12, padAccelTime = 0, padTurnAngle = 0, padTurnAccum = 0;
  int padMenuRow = 0, padPanel = -1;
  WORD padPrevious = 0;
  bool padPrevLT = false, padPrevRT = false;
  std::array<float, 8> padHeld{}, padNextRepeat{};   // held time and next repeat: up, down, left, right, LT, RT, stick up, stick down
  ULONGLONG padLastTurnPublish = 0;
  ULONGLONG padLastTick = 0;
  std::vector<std::string> padLog;   // what the pad did, for the regression
  HWND padMenuWindow = nullptr, padRing = nullptr;
  HFONT padFont = nullptr;
  static constexpr float PadSpeeds[5] = {1.5f, 2.5f, 4.f, 6.f, 9.f};        // metres a second at full deflection
  static constexpr float PadDeadZones[4] = {.12f, .2f, .3f, .42f};           // share of the stick's travel ignored
  static constexpr float PadAccels[4] = {0.f, .6f, 1.2f, 2.f};               // extra speed per second held at full
  static constexpr float PadRepeatDelays[3] = {.45f, .7f, 1.f};              // before a held button repeats
  static constexpr float PadRepeatEvery = .22f;
  static const wchar_t *padSpeedName(int i) { static const wchar_t *n[5] = {L"Very slow", L"Slow", L"Medium", L"Quick", L"Fast"}; return n[std::clamp(i, 0, 4)]; }
  static const wchar_t *padDeadName(int i) { static const wchar_t *n[4] = {L"Small", L"Standard", L"Wide", L"Widest"}; return n[std::clamp(i, 0, 3)]; }
  static const wchar_t *padAccelName(int i) { static const wchar_t *n[4] = {L"Off", L"Gentle", L"Medium", L"Strong"}; return n[std::clamp(i, 0, 3)]; }
  static const wchar_t *padRepeatName(int i) { static const wchar_t *n[3] = {L"Short", L"Long", L"Longest"}; return n[std::clamp(i, 0, 2)]; }
  fs::path padSettingsFile() const { return backgroundTest ? session / L"pad-settings.txt" : root / L".build" / L"pad-settings.txt"; }   // a test never touches the player's own
  void padSaveSettings() const {
    std::error_code ec;
    fs::create_directories(padSettingsFile().parent_path(), ec);
    std::ofstream(padSettingsFile()) << padSettings.speed << ' ' << padSettings.dead << ' ' << padSettings.accel << ' ' << padSettings.repeat << ' ' << int(padSettings.holdGrab) << '\n';
  }
  void padLoadSettings() {
    std::ifstream in(padSettingsFile());
    int s = 2, d = 1, a = 1, r = 1, h = 0;
    if (in >> s >> d >> a >> r >> h)
      padSettings = {std::clamp(s, 0, 4), std::clamp(d, 0, 3), std::clamp(a, 0, 3), std::clamp(r, 0, 2), h != 0};
  }
  // XInput, loaded at run time (no import library), the first pad that answers.
  struct PadXInputState { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
  static bool padRead(PadSample &out) {
    using GetState = DWORD(WINAPI *)(DWORD, PadXInputState *);
    static GetState get = [] {
      for (const wchar_t *name : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"})
        if (HMODULE m = LoadLibraryW(name))
          if (FARPROC f = GetProcAddress(m, "XInputGetState")) return reinterpret_cast<GetState>(reinterpret_cast<void *>(f));
      return GetState(nullptr);
    }();
    static DWORD slot = 4;
    static ULONGLONG nextScan = 0;
    if (!get) return false;
    PadXInputState s{};
    if (slot < 4 && get(slot, &s) == 0) { out = {true, s.buttons, s.lt, s.rt, s.lx, s.ly, s.rx, s.ry}; return true; }
    slot = 4;
    if (GetTickCount64() < nextScan) return false;
    nextScan = GetTickCount64() + 1000;   // an empty port is slow to ask: once a second
    for (DWORD i = 0; i < 4; ++i)
      if (get(i, &s) == 0) { slot = i; out = {true, s.buttons, s.lt, s.rt, s.lx, s.ly, s.rx, s.ry}; return true; }
    return false;
  }
  // The timer: read the pad when the Builder has the keys (play hands the pad to the game, which reads it itself).
  void padTimer() {
    const ULONGLONG now = GetTickCount64();
    const float dt = padLastTick ? std::clamp(float(now - padLastTick) / 1000.f, 0.f, .1f) : 1.f / 60.f;
    padLastTick = now;
    PadSample s;
    if (!padRead(s) || playing) { padPrevious = s.connected ? s.buttons : 0; padHeld.fill(0); return; }
    HWND front = GetForegroundWindow();
    if (front != window && GetWindow(front, GW_OWNER) != window) { padPrevious = s.buttons; return; }
    padStep(s, dt);
  }
  // A stick's deflection past the dead zone, 0-1 along it, squared for a fine start.
  Vec2f padStick(SHORT x, SHORT y) const {
    float fx = float(x) / 32767.f, fy = float(y) / 32767.f, length = std::sqrt(fx * fx + fy * fy);
    const float dead = PadDeadZones[std::clamp(padSettings.dead, 0, 3)];
    if (length <= dead) return {0, 0};
    const float t = std::min(1.f, (std::min(length, 1.f) - dead) / (1.f - dead));
    return {fx / length * t * t, fy / length * t * t};
  }
  // A button's press and, held past the repeat delay, a fresh press every PadRepeatEvery.
  bool padRepeat(int slot, bool down, float dt) {
    if (!down) { padHeld[size_t(slot)] = 0; return false; }
    if (padHeld[size_t(slot)] == 0) { padHeld[size_t(slot)] = 1e-4f; padNextRepeat[size_t(slot)] = PadRepeatDelays[std::clamp(padSettings.repeat, 0, 2)]; return true; }
    padHeld[size_t(slot)] += dt;
    if (padHeld[size_t(slot)] >= padNextRepeat[size_t(slot)]) { padNextRepeat[size_t(slot)] += PadRepeatEvery; return true; }
    return false;
  }
  void padStep(const PadSample &s, float dt) {
    const WORD pressed = WORD(s.buttons & ~padPrevious), released = WORD(padPrevious & ~s.buttons);
    padPrevious = s.buttons;
    const Vec2f left = padStick(s.lx, s.ly), right = padStick(s.rx, s.ry);
    const bool lt = s.lt > 90, rt = s.rt > 90;
    if (s.buttons || lt || rt || left.x || left.y || right.x || right.y) {
      if (!controllerHints) setInputDevice(true);
      padCursorShown = true;
    }
    const bool up = padRepeat(0, s.buttons & PadUp, dt), down = padRepeat(1, s.buttons & PadDown, dt);
    const bool leftPress = padRepeat(2, s.buttons & PadLeft, dt), rightPress = padRepeat(3, s.buttons & PadRight, dt);
    if (aiManagerWindow && IsWindowVisible(aiManagerWindow) && GetForegroundWindow() == aiManagerWindow) {
      if ((aiChipField==-3||aiChipListed)&&IsWindowVisible(aiControl(AiTarget))) {   // a pick list: a door, a lamp's mode, a sound
        HWND combo=aiControl(AiTarget);
        const int count=int(SendMessageW(combo,CB_GETCOUNT,0,0));
        if(count>0&&(up||down||leftPress||rightPress)){
          const int at=int(SendMessageW(combo,CB_GETCURSEL,0,0));
          SendMessageW(combo,CB_SETCURSEL,std::clamp(at+(up||leftPress?-1:1),0,count-1),0);
        }
        if(pressed&PadA)SendMessageW(aiManagerWindow,WM_COMMAND,AiChipApply,0);
        if(pressed&PadB){
          aiChipLine=-1;ShowWindow(combo,SW_HIDE);ShowWindow(aiControl(AiChipApply),SW_HIDE);
          SetFocus(aiControl(AiSteps));
        }
        return;
      }
      if (aiPadTyping) {
        constexpr std::wstring_view alphabet = L" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-.'";
        const int length = int(alphabet.size());
        const int delta = (rightPress ? 1 : 0) - (leftPress ? 1 : 0) + (down ? 8 : 0) - (up ? 8 : 0);
        aiPadCharacter = (aiPadCharacter + delta + length) % length;
        const wchar_t letter = alphabet[size_t(aiPadCharacter)];
        HWND edit = aiControl(aiPadTyping);
        auto text = aiText(edit);
        if ((pressed & PadA) && text.size() < 80) { text += letter; SetWindowTextW(edit, text.c_str()); }
        if ((pressed & PadX) && !text.empty()) { text.pop_back(); SetWindowTextW(edit, text.c_str()); }
        if (pressed & PadB) {
          if (aiPadTyping == AiChipValue)
            SendMessageW(aiManagerWindow, WM_COMMAND, AiChipApply, 0);
          if (aiPadTyping == AiRuleTitle)
            SendMessageW(aiManagerWindow, WM_COMMAND, MAKEWPARAM(AiRuleTitle, EN_KILLFOCUS), 0);
          aiPadTyping = 0;
          SetWindowTextW(aiControl(20), L"◀ ▶ Panels   ▲ ▼ Rows   Ⓐ Pick / snap   Ⓑ Back");
        } else {
          auto hint = std::wstring(L"Pick letter  [") + letter + L"]    Ⓐ Add    Ⓧ Delete    Ⓑ Done";
          SetWindowTextW(aiControl(20), hint.c_str());
        }
        return;
      }
      if (aiHeldBlock >= 0) {
        const int count = int(SendMessageW(aiControl(AiSteps), LB_GETCOUNT, 0, 0));
        if (up || down) {
          aiSnapRow = std::clamp(aiSnapRow + (down ? 1 : -1), 0, std::max(0, count - 1));
          SendMessageW(aiControl(AiSteps), LB_SETCURSEL, aiSnapRow, 0);
          InvalidateRect(aiControl(AiSteps), nullptr, TRUE);
        }
        if (pressed & PadA) aiDropBlock();
        if (pressed & PadB) {
          aiHeldBlock = aiSnapRow = -1;
          InvalidateRect(aiControl(AiSteps), nullptr, TRUE);
        }
        return;
      }
      if (pressed & PadB) { aiManagerCommand(AiDone, 0); return; }
      if (pressed & (PadLB | PadRB)) { aiManagerCommand(pressed & PadLB ? AiUndo : AiRedo, 0); return; }
      constexpr int controls[] = {AiOwners, AiRules, AiRuleTitle, AiSteps, AiEntities, AiPalette, AiAddDirector,
                                   AiAddRule, AiAddStep, AiEdit, AiRemoveStep, AiUndo, AiRedo,
                                   AiSearch, AiName, AiRename, AiChipValue, AiChipApply, AiTest, AiLinkedDirector, AiDone};
      const int count = int(std::size(controls));
      if (leftPress || rightPress) do {
        aiManagerFocus = (aiManagerFocus + (rightPress ? 1 : count - 1)) % count;
      } while(!IsWindowVisible(aiControl(controls[aiManagerFocus])));
      if (leftPress || rightPress || up || down || (pressed & PadA)) {
        const int id = controls[aiManagerFocus];
        HWND focus = aiControl(id);
        SetFocus(focus);
        aiShowFocus(id);
        if (id <= AiPalette && (up || down)) {
          const int rows = int(SendMessageW(focus, LB_GETCOUNT, 0, 0));
          if (rows > 0) {
            const int row = int(SendMessageW(focus, LB_GETCURSEL, 0, 0));
            SendMessageW(focus, LB_SETCURSEL, std::clamp(row + (down ? 1 : -1), 0, rows - 1), 0);
            SendMessageW(aiManagerWindow, WM_COMMAND, MAKEWPARAM(id, LBN_SELCHANGE), LPARAM(focus));
          }
        }
        if ((pressed & PadA) && id == AiPalette) {
          const int choice = aiListRecord(AiPalette);
          if (choice <= -100) { aiToggleCategory(choice); return; }
          aiHeldBlock = int(SendMessageW(focus, LB_GETCURSEL, 0, 0));
          aiSnapRow = std::max(0, int(SendMessageW(aiControl(AiSteps), LB_GETCURSEL, 0, 0)));
          SetFocus(aiControl(AiSteps));
          InvalidateRect(aiControl(AiSteps), nullptr, TRUE);
        } else if (pressed & PadA) {
          if (id == AiSteps) {
            aiBeginChip(aiPadChip);
            aiPadChip ^= 1;
            if (aiChipLine >= 0&&IsWindowVisible(aiControl(AiChipValue))) { aiPadTyping = AiChipValue; aiPadCharacter = 0; }
          } else if (id == AiSearch || id == AiName || id == AiChipValue || id == AiRuleTitle) {
            aiPadTyping = id;
            aiPadCharacter = 0;
          } else {
            SendMessageW(aiManagerWindow, WM_COMMAND, MAKEWPARAM(id, id <= AiPalette ? LBN_DBLCLK : BN_CLICKED), LPARAM(focus));
            if ((id == AiOwners || id == AiRules) && aiRenameLine >= 0) {
              aiPadTyping = AiName;
              aiPadCharacter = 0;
            }
          }
        }
      }
      return;
    }
    if (padMenuOpen) { padMenuStep(pressed, up, down, leftPress, rightPress); return; }
    if (padPanels) { padPanelStep(pressed, up, down, leftPress, rightPress); return; }
    // The map.
    if (pressed & PadBack) { padOpenMenu(); return; }
    if (pressed & PadL3) padTogglePrecise();
    padMoveCursor(left, dt);
    if (padSettings.holdGrab) {
      if ((pressed & PadX) && !padGrab) padGrabStart();
      if ((released & PadX) && padGrab) padGrabEnd();
    } else if (pressed & PadX) {
      if (padGrab) padGrabEnd(); else padGrabStart();
    }
    if (pressed & PadA) { if (padGrab) padGrabEnd(); else padClick(); }
    if (pressed & PadB) padCancel();
    if (pressed & PadY) { padLog.push_back("turn"); rotateFromShortcut(false); }
    if (pressed & PadLB) { padLog.push_back("undo"); SendMessageW(window, WM_COMMAND, Undo, 0); }
    if (pressed & PadRB) { padLog.push_back("redo"); SendMessageW(window, WM_COMMAND, Redo, 0); }
    if (pressed & PadStart) { padLog.push_back("play"); if (!backgroundTest) startPlay(0); }
    if (leftPress || rightPress) { padLog.push_back("tab"); controllerAction(retro::EditorControllerCommand::CatalogTab, rightPress ? 1 : -1); }
    if (up || down) { padLog.push_back("item"); controllerAction(retro::EditorControllerCommand::CatalogItem, down ? 1 : -1); }
    // The triggers scale the picked model whole: a press is a step, held they repeat (one Undo for the run).
    const bool grow = padRepeat(5, rt, dt), shrink = padRepeat(4, lt, dt);
    if (!lt && !rt) padScaling = false;
    if (grow || shrink) padScale(grow ? (padPrecise ? 1.01f : 1.05f) : (padPrecise ? 1 / 1.01f : 1 / 1.05f));
    // The right stick: smooth turning sideways, raise and lower up and down.
    padSmoothTurn(right.x, dt);
    const bool raise = padRepeat(6, right.y > .5f, dt), lower = padRepeat(7, right.y < -.5f, dt);
    if (raise || lower) {
      MSG msg{plan, WM_KEYDOWN, WPARAM(raise ? VK_PRIOR : VK_NEXT), LPARAM(padHeld[raise ? 6 : 7] > .01f ? 0x40000000 : 0)};
      padLog.push_back(raise ? "raise" : "lower");
      nudgeKey(msg, false, padPrecise);
    }
  }
  void padTogglePrecise() {
    padPrecise = !padPrecise;
    message(padPrecise ? L"Precise mode on: the cursor, turning and scaling go slowly and finely. Press again (or the menu) for normal."
                       : L"Precise mode off.");
    padLog.push_back(padPrecise ? "precise on" : "precise off");
    padRepaint();
  }
  // ---- The cursor.
  POINT padCursorPoint() const {
    auto v = planView();
    return {LONG(std::lround(v.ox + padX * v.cell)), LONG(std::lround(v.oy + padY * v.cell))};
  }
  void padMoveCursor(Vec2f stick, float dt) {
    if (!padCursorShown) return;
    const float deflection = std::sqrt(stick.x * stick.x + stick.y * stick.y);
    padAccelTime = deflection > .8f ? padAccelTime + dt : 0.f;
    if (deflection <= 0) return;
    const float accel = 1.f + PadAccels[std::clamp(padSettings.accel, 0, 3)] * std::min(padAccelTime, 1.5f);
    const float speed = PadSpeeds[std::clamp(padSettings.speed, 0, 4)] * accel * (padPrecise ? .2f : 1.f);
    padX = std::clamp(padX + stick.x * speed * dt, 0.f, 23.999f);
    padY = std::clamp(padY - stick.y * speed * dt, 0.f, 23.999f);   // stick up is north on the plan
    padKeepInView();
    POINT p = padCursorPoint();
    SendMessageW(plan, WM_MOUSEMOVE, padGrab ? MK_LBUTTON : 0, MAKELPARAM(p.x, p.y));
    padRepaint();
  }
  // The plan follows the cursor: it is never off the edge.
  void padKeepInView() {
    if (!plan || backgroundTest) return;
    RECT r{};
    GetClientRect(plan, &r);
    POINT p = padCursorPoint();
    const int margin = 48;
    float dx = 0, dy = 0;
    if (p.x < margin) dx = float(margin - p.x);
    if (p.x > r.right - margin) dx = float(r.right - margin - p.x);
    if (p.y < 32 + margin) dy = float(32 + margin - p.y);
    if (p.y > r.bottom - 32 - margin) dy = float(r.bottom - 32 - margin - p.y);
    if (dx || dy) planPanX += dx, planPanY += dy;
  }
  void padRepaint() {
    if (plan && !backgroundTest) InvalidateRect(plan, nullptr, FALSE);
    if (!backgroundTest && !playing) writeControl();   // the cursor's post in the 3D view
  }
  void padPlanMouse(UINT message, WPARAM keys) {
    POINT p = padCursorPoint();
    SendMessageW(plan, message, keys, MAKELPARAM(p.x, p.y));
  }
  void padClick() {
    padLog.push_back("click");
    padPlanMouse(WM_LBUTTONDOWN, MK_LBUTTON);
    padPlanMouse(WM_LBUTTONUP, 0);
    padRepaint();
  }
  void padGrabStart() {
    padLog.push_back("grab");
    padGrab = true;
    padPlanMouse(WM_LBUTTONDOWN, MK_LBUTTON);
    message(padSettings.holdGrab ? L"Holding. Move the left stick; let go of X to drop it here." : L"Grabbed. Move the left stick, then press X (or A) to drop it. B cancels.");
    padRepaint();
  }
  void padGrabEnd() {
    padLog.push_back("drop");
    padGrab = false;
    padPlanMouse(WM_LBUTTONUP, 0);
    padRepaint();
  }
  void padCancel() {
    padLog.push_back("cancel");
    if (padGrab) {
      padGrab = false;
      if (objectDrag) finishDrag(true);
      drag = brushing = selectingArea = false;
      if (GetCapture() == plan) ReleaseCapture();
      message(L"Put back where it was.");
    } else
      padPlanMouse(WM_RBUTTONDOWN, 0);   // put the tool down, as a right click does
    padRepaint();
  }
  // ---- Turning and scaling the picked thing.
  void padSmoothTurn(float stick, float dt) {
    if (stick == 0) {
      if (padTurning) {
        padTurning = false;
        if (padTurnAccum != 0) spinTo(padTurnAngle);
        refreshGhost();
        message(L"Turned. LB takes it back.");
      }
      return;
    }
    if (!padTurning) {
      if (!spinTurnable()) return;
      auto f = NativeDocument::fields(doc.lines[size_t(record)]);
      spinRecord = record;
      padTurnAngle = f.size() > spinYawIndex && !f[spinYawIndex].empty() ? std::stof(f[spinYawIndex]) : 0.f;
      padTurnAccum = 0;
      padTurning = true;
      remember();   // the whole turn is one Undo
      padLog.push_back("spin");
    }
    const float rate = (padPrecise ? .35f : 1.6f) * stick;   // radians a second
    padTurnAngle += rate * dt;
    padTurnAccum += rate * dt;
    if (backgroundTest || GetTickCount64() - padLastTurnPublish > 40) {
      padLastTurnPublish = GetTickCount64();
      spinTo(padTurnAngle);
      padTurnAccum = 0;
    }
  }
  // The picked model scaled whole about its middle (resizeNative from a corner, then put back on its middle).
  void padScale(float factor) {
    if (record < 0 || selectedRecords.size() > 1 || size_t(record) >= doc.lines.size()) { message(L"Pick a model first: A on it."); return; }
    if (!editableRecord(size_t(record))) { message(L"Unlock and show this floor before editing its content."); return; }
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    const int schema = schemaFor(f);
    if (schema < 0) return;
    const auto &names = schemas[size_t(schema)].fields;
    auto before = nativeHandle(names, f);
    auto corners = nativeCorners(names, f);
    if (!before || corners.size() != 4) { message(L"This one does not scale."); return; }
    const auto fixed = corners[0], opposite = corners[2];
    auto copy = f;
    if (!resizeNative(copy, names, 2, fixed.first + (opposite.first - fixed.first) * factor, fixed.second + (opposite.second - fixed.second) * factor, 0)) { message(L"This one does not scale."); return; }
    if (auto after = nativeHandle(names, copy)) {
      const size_t xi = nativeField(names, copy, "x"), yi = nativeField(names, copy, "y");
      if (xi != size_t(-1) && yi != size_t(-1)) {
        copy[xi] = nativeNumber(std::clamp(number(copy[xi], 0) + before->x - after->x, 0.f, 24.f));
        copy[yi] = nativeNumber(std::clamp(number(copy[yi], 0) + before->y - after->y, 0.f, 24.f));
      }
    }
    if (copy == f) return;
    if (!padScaling) { remember(); padScaling = true; }   // a held trigger is one Undo
    padLog.push_back(factor > 1 ? "grow" : "shrink");
    doc.replace(size_t(record), copy);
    relink(f, copy);
    publish();
    selectRecord(record);
    message(factor > 1 ? L"Bigger. RT again grows it, LT shrinks it." : L"Smaller. LT again shrinks it, RT grows it.");
  }
  // ---- The controller menu (View): big rows, one press each.
  enum PadMenuRow { PadRowPanels, PadRowView, PadRowTurn, PadRowPrecise, PadRowGrab, PadRowSpeed, PadRowDead, PadRowAccel, PadRowRepeat, PadRowClose, PadRowCount };
  std::wstring padMenuText(int row) const {
    static const wchar_t *turns[4] = {L"90 degrees", L"45 degrees", L"15 degrees", L"5 degrees"};
    static const wchar_t *views[3] = {L"Plan and 3D", L"Plan", L"3D"};
    switch (row) {
    case PadRowPanels: return L"Panels: buttons, lists, sliders, the inspector";
    case PadRowView: return std::wstring(L"View: ") + views[std::clamp(viewMode, 0, 2)];
    case PadRowTurn: return std::wstring(L"Turn step (Y): ") + turns[turnStepIndex & 3];
    case PadRowPrecise: return std::wstring(L"Precise mode: ") + (padPrecise ? L"on" : L"off");
    case PadRowGrab: return std::wstring(L"Grab with X: ") + (padSettings.holdGrab ? L"hold the button" : L"press once, press again to drop");
    case PadRowSpeed: return std::wstring(L"Stick speed: ") + padSpeedName(padSettings.speed);
    case PadRowDead: return std::wstring(L"Stick dead zone: ") + padDeadName(padSettings.dead);
    case PadRowAccel: return std::wstring(L"Stick acceleration: ") + padAccelName(padSettings.accel);
    case PadRowRepeat: return std::wstring(L"Wait before a held button repeats: ") + padRepeatName(padSettings.repeat);
    default: return L"Close (B)";
    }
  }
  void padOpenMenu() {
    if (padGrab) padGrabEnd();
    padMenuOpen = true;
    padMenuRow = 0;
    padLog.push_back("menu");
    message(L"Controller menu: D-pad up/down chooses, A or left/right changes, B closes.");
    padShowMenu();
  }
  void padCloseMenu() { padMenuOpen = false; padShowMenu(); }
  void padMenuChange(int row, int step) {
    switch (row) {
    case PadRowPanels: if (step >= 0) { padCloseMenu(); padEnterPanels(); } return;
    case PadRowView: cycleControllerView(); break;
    case PadRowTurn: nextTurnStep(); break;
    case PadRowPrecise: padTogglePrecise(); break;
    case PadRowGrab: padSettings.holdGrab = !padSettings.holdGrab; break;
    case PadRowSpeed: padSettings.speed = step ? std::clamp(padSettings.speed + step, 0, 4) : (padSettings.speed + 1) % 5; break;
    case PadRowDead: padSettings.dead = (padSettings.dead + (step < 0 ? 3 : 1)) % 4; break;
    case PadRowAccel: padSettings.accel = (padSettings.accel + (step < 0 ? 3 : 1)) % 4; break;
    case PadRowRepeat: padSettings.repeat = (padSettings.repeat + (step < 0 ? 2 : 1)) % 3; break;
    default: if (step >= 0) padCloseMenu(); return;
    }
    if (row >= PadRowGrab) padSaveSettings();
    padLog.push_back("menu " + std::to_string(row));
    padShowMenu();
  }
  void padMenuStep(WORD pressed, bool up, bool down, bool left, bool right) {
    if (pressed & (PadB | PadBack)) { padCloseMenu(); return; }
    if (up) padMenuRow = (padMenuRow + PadRowCount - 1) % PadRowCount;
    if (down) padMenuRow = (padMenuRow + 1) % PadRowCount;
    if (pressed & PadA) padMenuChange(padMenuRow, 0);
    else if (left || right) {
      if (padMenuRow != PadRowPanels && padMenuRow != PadRowClose) { padMenuChange(padMenuRow, right ? 1 : -1); return; }
    }
    if (up || down || left || right) padShowMenu();
  }
  static LRESULT CALLBACK padMenuProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) { e = static_cast<Editor *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams); SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e)); }
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (e && m == WM_PAINT) {
      PAINTSTRUCT ps{};
      HDC dc = BeginPaint(h, &ps);
      RECT r{};
      GetClientRect(h, &r);
      HBRUSH back = CreateSolidBrush(RGB(0x1E, 0x1C, 0x19)), lit = CreateSolidBrush(RGB(0xE8, 0xA8, 0x20));
      FillRect(dc, &r, back);
      SetBkMode(dc, TRANSPARENT);
      SelectObject(dc, e->padFont);
      SetTextColor(dc, RGB(0xF2, 0xEC, 0xE0));
      RECT title{24, 12, r.right - 24, 52};
      DrawTextW(dc, L"CONTROLLER", -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
      for (int i = 0; i < PadRowCount; ++i) {
        RECT row{16, 60 + i * 48, r.right - 16, 60 + i * 48 + 44};
        if (i == e->padMenuRow) {
          FillRect(dc, &row, lit);
          SetTextColor(dc, RGB(0x10, 0x0E, 0x0C));
        } else SetTextColor(dc, RGB(0xF2, 0xEC, 0xE0));
        RECT text{row.left + 14, row.top, row.right - 14, row.bottom};
        DrawTextW(dc, e->padMenuText(i).c_str(), -1, &text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
      }
      DeleteObject(back);
      DeleteObject(lit);
      EndPaint(h, &ps);
      return 0;
    }
    return DefWindowProcW(h, m, w, l);
  }
  void padShowMenu() {
    if (backgroundTest) return;
    if (!padFont) padFont = CreateFontW(-24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    if (!padMenuWindow) {
      static bool registered = false;
      if (!registered) {
        WNDCLASSW c{};
        c.lpfnWndProc = padMenuProc;
        c.hInstance = GetModuleHandleW(nullptr);
        c.lpszClassName = L"RawMetalPadMenu";
        c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&c);
        registered = true;
      }
      padMenuWindow = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"RawMetalPadMenu", L"Controller", WS_POPUP | WS_BORDER,
                                      0, 0, 720, 60 + PadRowCount * 48 + 16, window, nullptr, GetModuleHandleW(nullptr), this);
    }
    if (!padMenuOpen) { ShowWindow(padMenuWindow, SW_HIDE); return; }
    RECT owner{};
    GetWindowRect(window, &owner);
    const int w = 720, h = 60 + PadRowCount * 48 + 16;
    SetWindowPos(padMenuWindow, HWND_TOPMOST, (owner.left + owner.right - w) / 2, (owner.top + owner.bottom - h) / 2, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(padMenuWindow, nullptr, TRUE);
  }
  // ---- Panels: every control of the Builder in reading order, a ring round the one in hand.
  std::vector<HWND> padPanelControls() const {
    struct Collect { const Editor *e; std::vector<HWND> out; };
    Collect c{this, {}};
    EnumChildWindows(window, [](HWND h, LPARAM l) -> BOOL {
      auto &c = *reinterpret_cast<Collect *>(l);
      if (h == c.e->plan || h == c.e->viewport || IsChild(c.e->viewport, h) || !IsWindowEnabled(h)) return TRUE;
      // Shown in the Builder (its own window may be hidden, as in a test): it and every parent below the window.
      for (HWND p = h; p && p != c.e->window; p = GetParent(p))
        if (!(GetWindowLongW(p, GWL_STYLE) & WS_VISIBLE)) return TRUE;
      wchar_t name[64]{};
      GetClassNameW(h, name, 64);
      const std::wstring k = name;
      if (k == L"Button" || k == L"Edit" || k == L"ComboBox" || k == L"msctls_trackbar32" || k == L"RawMetalPictures") {
        RECT r{};
        GetWindowRect(h, &r);
        if (r.right - r.left > 4 && r.bottom - r.top > 4) c.out.push_back(h);
      }
      return TRUE;
    }, reinterpret_cast<LPARAM>(&c));
    std::stable_sort(c.out.begin(), c.out.end(), [](HWND a, HWND b) {
      RECT ra{}, rb{};
      GetWindowRect(a, &ra);
      GetWindowRect(b, &rb);
      // Columns first (the tool shelf, the panel at the side), then top to bottom within one.
      const int ca = ra.left / 160, cb = rb.left / 160;
      if (ca != cb) return ca < cb;
      if (std::abs(ra.top - rb.top) > 6) return ra.top < rb.top;
      return ra.left < rb.left;
    });
    return c.out;
  }
  HWND padPanelControl() const {
    auto list = padPanelControls();
    return padPanel >= 0 && padPanel < int(list.size()) ? list[size_t(padPanel)] : nullptr;
  }
  void padEnterPanels() {
    padPanels = true;
    padPanel = 0;
    padLog.push_back("panels");
    message(L"Panels: D-pad up/down moves, left/right changes the value, A presses, B goes back to the map.");
    padShowRing();
  }
  void padLeavePanels() {
    padPanels = false;
    padLog.push_back("map");
    padShowRing();
    message(L"Back on the map.");
  }
  void padPanelStep(WORD pressed, bool up, bool down, bool left, bool right) {
    if (pressed & (PadB | PadBack)) { padLeavePanels(); return; }
    auto list = padPanelControls();
    if (list.empty()) { padLeavePanels(); return; }
    padPanel = std::clamp(padPanel, 0, int(list.size()) - 1);
    if (up) padPanel = (padPanel + int(list.size()) - 1) % int(list.size());
    if (down) padPanel = (padPanel + 1) % int(list.size());
    HWND c = list[size_t(padPanel)];
    if (left || right) padPanelValue(c, right ? 1 : -1);
    if (pressed & PadA) padPanelPress(c);
    if (up || down || left || right || (pressed & PadA)) padShowRing();
  }
  // Left/right on a control: a slider steps, a list chooses the next, a number field counts, a tick box flips,
  // the catalogue's pictures choose the next part.
  void padPanelValue(HWND c, int step) {
    wchar_t name[64]{};
    GetClassNameW(c, name, 64);
    const std::wstring k = name;
    HWND parent = GetParent(c);
    const int id = GetDlgCtrlID(c);
    padLog.push_back("value");
    if (k == L"msctls_trackbar32") {
      const int lo = int(SendMessageW(c, TBM_GETRANGEMIN, 0, 0)), hi = int(SendMessageW(c, TBM_GETRANGEMAX, 0, 0));
      const int delta = std::max(1, (hi - lo) / (padPrecise ? 100 : 20)) * step;
      const int pos = std::clamp(int(SendMessageW(c, TBM_GETPOS, 0, 0)) + delta, lo, hi);
      SendMessageW(c, TBM_SETPOS, TRUE, pos);
      SendMessageW(parent, WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, pos), reinterpret_cast<LPARAM>(c));
      SendMessageW(parent, WM_HSCROLL, MAKEWPARAM(TB_ENDTRACK, 0), reinterpret_cast<LPARAM>(c));
    } else if (k == L"ComboBox") {
      const int count = int(SendMessageW(c, CB_GETCOUNT, 0, 0));
      if (count <= 0) return;
      const int at = int(SendMessageW(c, CB_GETCURSEL, 0, 0));
      const int next = std::clamp(at < 0 ? 0 : at + step, 0, count - 1);
      if (next == at) return;
      SendMessageW(c, CB_SETCURSEL, next, 0);
      SendMessageW(parent, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(c));
    } else if (k == L"Edit") {
      wchar_t text[64]{};
      GetWindowTextW(c, text, 64);
      wchar_t *end = nullptr;
      const double v = std::wcstod(text, &end);
      if (end == text) return;   // words: A puts the keys in it
      const bool decimal = std::wcschr(text, L'.') != nullptr;
      const double by = padPrecise ? (decimal ? .01 : 1) : (decimal ? .1 : 1);
      wchar_t out[64];
      swprintf(out, 64, decimal ? L"%.2f" : L"%.0f", v + by * step);
      SetWindowTextW(c, out);
      SendMessageW(parent, WM_COMMAND, MAKEWPARAM(id, EN_CHANGE), reinterpret_cast<LPARAM>(c));
      SendMessageW(parent, WM_COMMAND, MAKEWPARAM(id, EN_KILLFOCUS), reinterpret_cast<LPARAM>(c));
    } else if (k == L"Button") {
      const LONG style = GetWindowLongW(c, GWL_STYLE) & BS_TYPEMASK;
      if (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX || style == BS_3STATE || style == BS_AUTO3STATE) padPanelPress(c);
    } else if (k == L"RawMetalPictures")
      controllerAction(retro::EditorControllerCommand::CatalogItem, step);
  }
  void padPanelPress(HWND c) {
    wchar_t name[64]{};
    GetClassNameW(c, name, 64);
    const std::wstring k = name;
    padLog.push_back("press");
    if (k == L"Button") SendMessageW(c, BM_CLICK, 0, 0);
    else if (k == L"Edit" || k == L"ComboBox") { if (!backgroundTest) SetFocus(c); if (k == L"ComboBox") SendMessageW(c, CB_SHOWDROPDOWN, !SendMessageW(c, CB_GETDROPPEDSTATE, 0, 0), 0); }
    else if (k == L"RawMetalPictures") { padLeavePanels(); message(L"Aim the cursor and press A to place it."); }
  }
  static LRESULT CALLBACK padRingProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
      PAINTSTRUCT ps{};
      HDC dc = BeginPaint(h, &ps);
      RECT r{};
      GetClientRect(h, &r);
      HBRUSH key = CreateSolidBrush(RGB(255, 0, 255)), ring = CreateSolidBrush(RGB(0xFF, 0xB0, 0x10));
      FillRect(dc, &r, ring);
      RECT inner{r.left + 5, r.top + 5, r.right - 5, r.bottom - 5};
      FillRect(dc, &inner, key);
      DeleteObject(key);
      DeleteObject(ring);
      EndPaint(h, &ps);
      return 0;
    }
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcW(h, m, w, l);
  }
  // The ring round the control in hand: a thick amber frame over it, which the mouse passes through.
  void padShowRing() {
    if (backgroundTest) return;
    if (!padRing) {
      static bool registered = false;
      if (!registered) {
        WNDCLASSW c{};
        c.lpfnWndProc = padRingProc;
        c.hInstance = GetModuleHandleW(nullptr);
        c.lpszClassName = L"RawMetalPadRing";
        RegisterClassW(&c);
        registered = true;
      }
      padRing = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"RawMetalPadRing", L"", WS_POPUP,
                                0, 0, 10, 10, window, nullptr, GetModuleHandleW(nullptr), nullptr);
      SetLayeredWindowAttributes(padRing, RGB(255, 0, 255), 0, LWA_COLORKEY);
    }
    HWND c = padPanels ? padPanelControl() : nullptr;
    if (!c) { ShowWindow(padRing, SW_HIDE); return; }
    RECT r{};
    GetWindowRect(c, &r);
    InflateRect(&r, 7, 7);
    SetWindowPos(padRing, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(padRing, nullptr, TRUE);
  }
  // The controller's popups drawn into a picture of the window (--shots): the menu where it sits, the ring round the
  // control in hand.
  void padPaintPopups(HDC dc) {
    POINT origin{0, 0};
    ClientToScreen(window, &origin);
    if (padMenuWindow && IsWindowVisible(padMenuWindow)) {
      RECT r{};
      GetWindowRect(padMenuWindow, &r);
      HDC menu = CreateCompatibleDC(dc);
      HBITMAP b = CreateCompatibleBitmap(dc, r.right - r.left, r.bottom - r.top);
      auto o = SelectObject(menu, b);
      PrintWindow(padMenuWindow, menu, 0);
      BitBlt(dc, r.left - origin.x, r.top - origin.y, r.right - r.left, r.bottom - r.top, menu, 0, 0, SRCCOPY);
      SelectObject(menu, o), DeleteObject(b), DeleteDC(menu);
    }
    if (padRing && IsWindowVisible(padRing)) {
      RECT r{};
      GetWindowRect(padRing, &r);
      OffsetRect(&r, -origin.x, -origin.y);
      HBRUSH ring = CreateSolidBrush(RGB(0xFF, 0xB0, 0x10));
      for (int i = 0; i < 5; ++i) { FrameRect(dc, &r, ring); InflateRect(&r, -1, -1); }
      DeleteObject(ring);
    }
  }
  // ---- Where the cursor shows: a big ring and cross on the plan, a post in the 3D view.
  void drawPadCursor(HDC dc, int ox, int oy, float cell) {
    if (!controllerHints || !padCursorShown || playing) return;
    const int x = int(std::lround(ox + padX * cell)), y = int(std::lround(oy + padY * cell));
    const COLORREF colour = padGrab ? RGB(0x40, 0xE0, 0xFF) : padPrecise ? RGB(0xFF, 0x60, 0xC0) : RGB(0xFF, 0xB0, 0x10);
    HPEN dark = CreatePen(PS_SOLID, 7, RGB(0x10, 0x0E, 0x0C)), bright = CreatePen(PS_SOLID, 3, colour);
    auto oldPen = SelectObject(dc, dark);
    auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    const int radius = padPrecise ? 10 : 16;
    for (HPEN p : {dark, bright}) {
      SelectObject(dc, p);
      Ellipse(dc, x - radius, y - radius, x + radius + 1, y + radius + 1);
      MoveToEx(dc, x - radius - 10, y, nullptr); LineTo(dc, x - 4, y);
      MoveToEx(dc, x + 4, y, nullptr); LineTo(dc, x + radius + 11, y);
      MoveToEx(dc, x, y - radius - 10, nullptr); LineTo(dc, x, y - 4);
      MoveToEx(dc, x, y + 4, nullptr); LineTo(dc, x, y + radius + 11);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(dark);
    DeleteObject(bright);
  }
  void padGuides(std::vector<retro::EditorGuide> &guides) const {
    if (!controllerHints || !padCursorShown) return;
    const float z = layerElevation(), s = padPrecise ? .2f : .45f;
    guides.push_back({padX - s, padY, z + .02f, padX + s, padY, z + .02f, 4});
    guides.push_back({padX, padY - s, z + .02f, padX, padY + s, z + .02f, 4});
    guides.push_back({padX, padY, z, padX, padY, z + 1.8f, 4});
  }
