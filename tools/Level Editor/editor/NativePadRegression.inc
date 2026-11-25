  // --pad-test (and --editing-test): the whole Builder from a controller, through the pad's own input path (padStep)
  // with synthetic pad states, as the timer hands them over: the cursor (speed, dead zone, acceleration, precise
  // mode), A to pick, X to grab and drop (toggle and hold), B to cancel, Y and the turn step, the right stick's smooth
  // turn and raise, the triggers' whole-model scale, LB/RB Undo/Redo, the catalogue on the D-pad and A to place, the
  // controller menu, the panels (a ring's worth of controls, values changed by left/right), the held-button repeat
  // and the settings kept on disk. native-editor-pad-test.txt.
  void verifyPad() {
    std::ofstream report(root / L"native-editor-pad-test.txt");
    auto pass = [&](const std::string &what) { report << "PASS: " << what << '\n' << std::flush; };
    auto require = [](bool okay, const std::string &message) {
      if (!okay)
        throw std::runtime_error("Pad regression: " + message);
    };
    const PadSettings savedSettings = padSettings;
    padSettings = {};
    padPrecise = padGrab = padMenuOpen = padPanels = padTurning = padScaling = false;
    padPrevious = 0;
    padHeld.fill(0);
    NativeDocument blank;
    blank.newCampaign("Pad");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false;
    const float savedSnap = snap;
    snap = .25f;
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    addAuthored({"FIXTURE", std::to_string(map), "24", "8", "8", "0", "1.4", "0.7", "0.75", "0", "1"});
    publish();
    const size_t desk = doc.records("FIXTURE", map).back();
    selectRecord(-1);
    const float dt = 1.f / 60.f;
    auto run = [&](PadSample s, int frames) { for (int i = 0; i < frames; ++i) padStep(s, dt); };
    auto tap = [&](WORD button) { PadSample s; s.buttons = button; padStep(s, dt); padStep(PadSample{}, dt); };
    auto field = [&](size_t i, size_t k) { return number(NativeDocument::fields(doc.lines[i])[k], -999); };
    padLog.clear();

    // The cursor: a full stick for one second at the medium speed is 4 m; inside the dead zone nothing moves.
    padSettings.accel = 0;
    padX = 4, padY = 12;
    tap(0);   // shows the cursor
    PadSample stick;
    stick.lx = 32767;
    run(stick, 60);
    require(std::abs(padX - 8.f) < .15f, "a full left stick for a second at medium speed did not move the cursor 4 m (" + std::to_string(padX) + ")");
    require(controllerHints && padCursorShown, "the pad did not switch the hints to the controller and show its cursor");
    stick.lx = SHORT(32767 * .15f);
    const float still = padX;
    run(stick, 60);
    require(padX == still, "a thumb resting inside the dead zone moved the cursor");
    padSettings.speed = 0;
    stick.lx = 32767;
    padX = 4;
    run(stick, 60);
    require(std::abs(padX - 5.5f) < .15f, "VERY SLOW did not move 1.5 m in a second");
    padSettings.speed = 2;
    padPrecise = true;
    padX = 4;
    run(stick, 60);
    require(std::abs(padX - 4.8f) < .1f, "precise mode did not slow the cursor to a fifth");
    padPrecise = false;
    padSettings.accel = 3;
    padX = 2;
    run(stick, 120);
    require(padX > 2 + 8 * 1.5f, "acceleration did not speed a long full push");
    padSettings.accel = 1;
    pass("cursor: speed, dead zone, very slow, precise and acceleration");

    // A on the desk picks it; X grabs it, the stick carries it, X drops it: one Undo; LB/RB undo and redo.
    padX = 8, padY = 8;
    tap(PadA);
    require(record == int(desk), "A on the desk did not pick it");
    const auto before = doc.text();
    const size_t history0 = history.size();
    tap(PadX);
    require(padGrab && objectDrag, "X did not grab the picked desk");
    stick = {};
    stick.lx = 32767;
    padSettings.accel = 0;
    run(stick, 15);   // a metre east
    tap(PadX);
    require(!padGrab && !objectDrag, "X again did not drop it");
    require(std::abs(field(desk, 3) - 9.f) < .13f && std::abs(field(desk, 4) - 8.f) < .01f, "the grabbed desk did not travel with the cursor (x " + std::to_string(field(desk, 3)) + ")");
    require(history.size() == history0 + 1, "a grab and drop was not one Undo");
    const auto moved = doc.text();
    tap(PadLB);
    require(doc.text() == before, "LB did not undo the move");
    tap(PadRB);
    require(doc.text() == moved, "RB did not redo the move");
    // B while grabbed puts it back.
    selectRecord(int(desk));
    padX = field(desk, 3), padY = field(desk, 4);
    tap(PadX);
    run(stick, 20);
    tap(PadB);
    require(!padGrab && doc.text() == moved, "B while grabbed did not put it back where it was");
    // Hold mode: X held carries, let go drops.
    padSettings.holdGrab = true;
    selectRecord(int(desk));
    padX = field(desk, 3), padY = field(desk, 4);
    const float heldFrom = field(desk, 3);
    PadSample hold;
    hold.buttons = PadX;
    padStep(hold, dt);
    require(padGrab, "in hold mode a held X did not grab");
    hold.lx = 32767;
    run(hold, 15);
    padStep(PadSample{}, dt);
    require(!padGrab && field(desk, 3) > heldFrom + .7f, "in hold mode letting go of X did not drop it where it was carried");
    padSettings.holdGrab = false;
    pass("A picks, X grabs and drops (toggle and hold), B cancels, LB/RB undo and redo");

    // Y turns by the turn step; the menu's Turn step row makes it 45 degrees.
    selectRecord(int(desk));
    const float yaw0 = field(desk, 9);
    tap(PadY);
    require(std::abs(std::remainder(field(desk, 9) - yaw0 - 1.5707963f, 6.2831853f)) < 1e-3f, "Y did not turn a quarter");
    const int step0 = turnStepIndex;
    tap(PadBack);
    require(padMenuOpen, "View did not open the controller menu");
    for (int i = 0; i < PadRowTurn; ++i) tap(PadDown);
    require(padMenuRow == PadRowTurn, "the D-pad did not walk the menu's rows");
    tap(PadA);
    require(turnStepIndex == (step0 + 1) % 4, "the Turn step row did not call nextTurnStep");
    tap(PadB);
    require(!padMenuOpen, "B did not close the controller menu");
    const float yaw1 = field(desk, 9);
    tap(PadY);
    require(std::abs(std::remainder(field(desk, 9) - yaw1 - TurnSteps[size_t(turnStepIndex)], 6.2831853f)) < 1e-3f, "Y did not turn by the new step");
    turnStepIndex = step0;
    // The right stick turns smoothly: half a second at full is 0.8 radians, one Undo.
    const size_t history1 = history.size();
    const float yaw2 = field(desk, 9);
    PadSample spin;
    spin.rx = 32767;
    run(spin, 30);
    padStep(PadSample{}, dt);
    const float turned = std::remainder(field(desk, 9) - yaw2, 6.2831853f);
    require(turned > .7f && turned < .9f, "the right stick did not turn the desk smoothly (" + std::to_string(turned) + " rad)");
    require(history.size() == history1 + 1, "a smooth turn was not one Undo");
    // Up on the right stick raises it a step.
    const float z0 = field(desk, 5);
    PadSample lift;
    lift.ry = 32767;
    padStep(lift, dt);
    padStep(PadSample{}, dt);
    require(std::abs(field(desk, 5) - z0 - .25f) < 1e-3f, "the right stick up did not raise the desk a step");
    pass("Y turns by the turn step (menu: Turn step), the right stick turns smoothly and raises");

    // RT grows the desk whole (width, depth and height together) about its middle; LT shrinks it.
    const float w0 = field(desk, 6), d0 = field(desk, 7), h0 = field(desk, 8), x0 = field(desk, 3), y0 = field(desk, 4);
    const size_t history2 = history.size();
    PadSample grow;
    grow.rt = 255;
    padStep(grow, dt);
    padStep(PadSample{}, dt);
    require(std::abs(field(desk, 6) / w0 - 1.05f) < .01f && std::abs(field(desk, 7) / d0 - 1.05f) < .01f && std::abs(field(desk, 8) / h0 - 1.05f) < .01f,
            "RT did not scale the desk whole by 5% (" + std::to_string(field(desk, 6) / w0) + ", " + std::to_string(field(desk, 8) / h0) + ")");
    require(std::abs(field(desk, 3) - x0) < .01f && std::abs(field(desk, 4) - y0) < .01f, "scaling moved the desk off its middle");
    PadSample shrink;
    shrink.lt = 255;
    padStep(shrink, dt);
    padStep(PadSample{}, dt);
    require(std::abs(field(desk, 6) - w0) < .01f, "LT did not shrink it back");
    require(history.size() == history2 + 2, "each trigger press was not one Undo");
    // Held, the trigger waits (the repeat delay) before it repeats: a second gives the press and two repeats.
    const float w1 = field(desk, 6);
    run(grow, 60);
    padStep(PadSample{}, dt);
    const float growth = field(desk, 6) / w1;
    require(growth > 1.05f * 1.05f * 1.04f && growth < 1.05f * 1.05f * 1.05f * 1.05f * 1.01f, "a held RT did not repeat after the delay, slowly (" + std::to_string(growth) + ")");
    pass("RT and LT scale the model whole about its middle; a held trigger repeats after the long delay");

    // The catalogue: D-pad right changes the tab, down picks a part; A places it at the cursor.
    tap(PadRight);
    require(browsing(), "D-pad right did not open the catalogue");
    tap(PadDown);
    require(partsGrid.selected >= 0 || !partsGrid.items.empty(), "D-pad down did not choose a part");
    const size_t lines0 = doc.lines.size();
    padX = 16.5f, padY = 16.5f;
    padStep(PadSample{}, dt);
    tap(PadA);
    require(doc.lines.size() > lines0, "A did not place the chosen part at the cursor");
    tap(PadLB);
    require(doc.lines.size() == lines0, "LB did not take the placed part back");
    tap(PadB);   // put the tool down
    pass("the catalogue on the D-pad, A places, LB takes it back, B puts the tool down");

    // The panels: every control in reading order; down moves on, left/right changes, B comes back to the map.
    tap(PadBack);
    tap(PadA);   // the first row: Panels
    require(padPanels && !padMenuOpen, "the menu's Panels row did not go to the panels");
    const auto controls = padPanelControls();
    require(controls.size() > 8, "the panels found too few controls (" + std::to_string(controls.size()) + ")");
    tap(PadDown);
    require(padPanel == 1, "D-pad down did not move to the next control");
    // Find a list and change it with right.
    int listAt = -1;
    for (size_t i = 0; i < controls.size(); ++i) {
      wchar_t name[64]{};
      GetClassNameW(controls[i], name, 64);
      if (std::wstring(name) == L"ComboBox" && SendMessageW(controls[i], CB_GETCOUNT, 0, 0) > 1 && SendMessageW(controls[i], CB_GETCURSEL, 0, 0) == 0) { listAt = int(i); break; }
    }
    if (listAt >= 0) {
      while (padPanel != listAt) tap(PadDown);
      tap(PadRight);
      require(SendMessageW(controls[size_t(listAt)], CB_GETCURSEL, 0, 0) == 1, "D-pad right did not choose the next entry of a list");
      tap(PadLeft);
      require(SendMessageW(controls[size_t(listAt)], CB_GETCURSEL, 0, 0) == 0, "D-pad left did not choose it back");
    }
    tap(PadB);
    require(!padPanels, "B did not come back from the panels to the map");
    require(listAt >= 0, "the panels found no list to change");
    pass("panels: " + std::to_string(controls.size()) + " controls in reading order, the D-pad walks them, left/right changes a list (control " + std::to_string(listAt) + "), B returns");

    // A held D-pad repeats only after the delay: half a second is one press, a second and a half several.
    padLog.clear();
    PadSample held;
    held.buttons = PadDown;
    run(held, 30);
    const auto items = std::count(padLog.begin(), padLog.end(), "item");
    run(held, 60);
    padStep(PadSample{}, dt);
    const auto more = std::count(padLog.begin(), padLog.end(), "item");
    require(items == 1 && more >= 3 && more <= 6, "the held D-pad's repeat is not generous (" + std::to_string(items) + " then " + std::to_string(more) + ")");
    tap(PadB);
    pass("a held button repeats only after " + std::to_string(int(PadRepeatDelays[1] * 1000)) + " ms, then every " + std::to_string(int(PadRepeatEvery * 1000)) + " ms");

    // No chord: every action above was one button at a time (the log holds them all).
    // The settings survive a save and load.
    padSettings = {4, 3, 2, 0, true};
    padSaveSettings();
    padSettings = {};
    padLoadSettings();
    require(padSettings.speed == 4 && padSettings.dead == 3 && padSettings.accel == 2 && padSettings.repeat == 0 && padSettings.holdGrab, "the controller settings did not survive a save and load");
    std::error_code ec;
    fs::remove(padSettingsFile(), ec);
    pass("stick speed, dead zone, acceleration, repeat delay and hold/toggle are saved and read back");

    padSettings = savedSettings;
    snap = savedSnap;
    padCursorShown = false;
    setInputDevice(false);
    selectRecord(-1);
  }
