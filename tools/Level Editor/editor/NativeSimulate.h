// Simulate (F6): the map comes alive in the builder's 3D picture without
// entering play, like UnrealEd's Simulate. Guards walk their beats, creatures
// move, people keep their routines, clocks tick, lamps flicker and doors work.
// The camera is a ghost nothing sees or hunts. Box triggers fire when the
// camera flies into them, and clicking a trigger on the plan fires it. The plan
// shows a dot for each person and creature and a pulse on a trigger as it fires.
// Stopping (the button again, F6 or Escape) puts the authored scene back
// exactly; an edit, another map or Play stops it first.
//
// The engine half is src/platform/EditorPreview.cpp (Game::setEditorObserver).
// The two talk through two small files beside the preview's control file:
//   simulate.txt         editor -> engine  "on fireSerial x1 y1 x2 y2"
//   simulate-status.txt  engine -> editor  "SIM on seconds camX camY camZ", then one line per
//                        person "P x y alert", creature "C x y" and recently
//                        fired trigger "F x1 y1 x2 y2 age".
//
// NativeEditor.cpp includes this once, after the Editor and its window
// procedures, and calls install() when the window is built. The button, the
// keys and the plan marks hook the windows from here, so the editor's own
// procedures stay as they are.
#pragma once
#include <array>

namespace simulate {
constexpr int ButtonId = 960;
constexpr wchar_t OffLabel[] = L"● Simulate  F6", OnLabel[] = L"■ Stop  F6";
constexpr wchar_t Running[] =
    L"Simulating: your map runs on its own and nobody can see you. Click a trigger to fire it. "
    L"F6 or Escape puts everything back.";
struct Mark {
  float x = 0, y = 0;
  int kind = 0; // 0 a person, 1 a creature, 2 a guard who is not calm
};
struct Pulse {
  float x1 = 0, y1 = 0, x2 = 0, y2 = 0, age = 0;
};
struct State {
  Editor *editor = nullptr;
  HWND button = nullptr;
  WNDPROC mainBase = nullptr, planBase = nullptr;
  HHOOK keys = nullptr;
  bool on = false, engineOn = false;
  ULONGLONG toggledAt = 0;
  unsigned long long frameAtToggle = 0;
  long long revision = 0;
  int map = 0, fireSerial = 0;
  float seconds = 0;
  std::string seen;
  std::vector<Mark> marks;
  std::vector<Pulse> pulses;
};
inline State state;
inline bool testRun = false; // --simulate-test

inline fs::path askFile(const Editor &e) { return e.control.parent_path() / L"simulate.txt"; }
inline fs::path statusFile(const Editor &e) { return e.control.parent_path() / L"simulate-status.txt"; }
inline unsigned long long engineFrame(const Editor &e) {
  std::istringstream in(e.channelText(e.status));
  std::string tag;
  long long rev = 0;
  int map = 0, play = 0;
  unsigned long long frame = 0;
  return in >> tag >> rev >> map >> play >> frame ? frame : 0;
}
inline void ask(Editor &e, const float *box = nullptr) {
  if (e.session.empty())
    return;
  std::ostringstream out;
  out << int(state.on) << ' ' << state.fireSerial;
  for (int i = 0; i < 4; ++i)
    out << ' ' << (box ? box[i] : 0.f);
  out << '\n';
  e.channelPut(askFile(e), out.str());
}
inline void showButton(Editor &e) {
  if (!state.button)
    return;
  SetWindowTextW(state.button, state.on ? OnLabel : OffLabel);
  InvalidateRect(state.button, nullptr, FALSE);
  if (e.plan)
    InvalidateRect(e.plan, nullptr, FALSE);
}
inline void set(Editor &e, bool on, const wchar_t *why = nullptr) {
  if (on && e.playing)
    return;
  state.on = on;
  state.toggledAt = GetTickCount64();
  state.frameAtToggle = engineFrame(e);
  state.revision = e.revision;
  state.map = e.map;
  if (!on)
    state.marks.clear(), state.pulses.clear();
  ask(e);
  showButton(e);
  e.message(why ? why : on ? Running : L"Simulation stopped. Everything is back where you built it.");
}
// The trigger box under a plan point: the smallest SCRIPT_EVENT on this map
// that fires by its box (not a map- or campaign-wide one).
inline bool triggerAt(const Editor &e, float x, float y, float box[4], std::string *name = nullptr) {
  std::set<std::string> wide;
  for (auto index : e.doc.records("SCRIPT_TRIGGER", e.map)) {
    auto f = NativeDocument::fields(e.doc.lines[index]);
    if (f.size() > 3 && !f[3].empty() && f[3] != "box")
      wide.insert(f[2]);
  }
  float best = 1e9f;
  for (auto index : e.doc.records("SCRIPT_EVENT", e.map)) {
    auto f = NativeDocument::fields(e.doc.lines[index]);
    if (f.size() < 7 || wide.count(f[2]))
      continue;
    float b[4];
    try {
      for (int i = 0; i < 4; ++i)
        b[i] = std::stof(f[size_t(3 + i)]);
    } catch (...) {
      continue;
    }
    float area = (b[2] - b[0]) * (b[3] - b[1]);
    if (x < b[0] || x > b[2] || y < b[1] || y > b[3] || area >= best)
      continue;
    best = area;
    std::copy(b, b + 4, box);
    if (name)
      *name = decoded(f[2]);
  }
  return best < 1e9f;
}
inline void fire(Editor &e, const float box[4]) {
  ++state.fireSerial;
  ask(e, box);
}
// Reads what the engine says. Called on the editor's 50 ms tick.
inline void poll(Editor &e) {
  if (!e.window)
    return;
  // An edit, another map or play ends it; the engine has already put the scene back.
  if (state.on && (e.revision != state.revision || e.map != state.map || e.playing))
    set(e, false, e.playing ? L"Playing. Escape comes back to building."
                            : L"Simulation stopped for your change. Press F6 to watch again.");
  std::string text = e.channelText(statusFile(e));
  if (text != state.seen && !text.empty()) {
    state.seen = text;
    state.marks.clear(), state.pulses.clear();
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
      std::istringstream f(line);
      std::string tag;
      f >> tag;
      if (tag == "SIM") {
        int on = 0;
        f >> on >> state.seconds;
        state.engineOn = on != 0;
      } else if (tag == "P" || tag == "C") {
        Mark m;
        int alert = 0;
        if (f >> m.x >> m.y) {
          f >> alert;
          m.kind = tag == "C" ? 1 : alert ? 2 : 0;
          state.marks.push_back(m);
        }
      } else if (tag == "F") {
        Pulse p;
        if (f >> p.x1 >> p.y1 >> p.x2 >> p.y2 >> p.age)
          state.pulses.push_back(p);
      }
    }
    if (state.on && e.plan)
      InvalidateRect(e.plan, nullptr, FALSE);
  }
  // The engine is the judge, as with play: F6 or Escape in the picture, or a
  // map it cannot run. Give it a moment (and some frames) to answer first.
  if (state.on != state.engineOn && GetTickCount64() - state.toggledAt > 1500 &&
      engineFrame(e) > state.frameAtToggle + 10) {
    state.on = state.engineOn;
    state.toggledAt = GetTickCount64();
    state.revision = e.revision, state.map = e.map;
    showButton(e);
    e.message(state.on ? Running : L"Simulation stopped. Everything is back where you built it.");
  } else if (state.on)
    e.message(Running, true);
}
inline void place(Editor &e) {
  if (!state.button)
    return;
  RECT r{};
  GetClientRect(e.window, &r);
  int right = r.right - 14 - 176 - 10 - 136;
  if (HWND from = GetDlgItem(e.window, PlayStart)) {
    RECT q{};
    GetWindowRect(from, &q);
    MapWindowPoints(nullptr, e.window, reinterpret_cast<POINT *>(&q), 2);
    if (q.right > q.left)
      right = q.left;
  }
  MoveWindow(state.button, right - 10 - 136, 52, 136, 40, TRUE);
}
// The live marks, drawn over the plan (r is its client area) once it has painted.
inline void overlay(Editor &e, HDC dc, RECT r) {
  const auto view = e.planView();
  auto px = [&](float x) { return int(std::lround(view.ox + x * view.cell)); };
  auto py = [&](float y) { return int(std::lround(view.oy + y * view.cell)); };
  auto mix = [](COLORREF a, COLORREF b, float t) {
    return RGB(int(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t), int(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               int(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
  };
  HRGN clip = CreateRectRgn(0, 28, r.right, r.bottom - 26);
  SelectClipRgn(dc, clip);
  SetBkMode(dc, TRANSPARENT);
  auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  // A trigger that just fired flashes and rings outward for a second and a half.
  for (const auto &p : state.pulses) {
    const float t = std::clamp(p.age / 1.5f, 0.f, 1.f);
    RECT box{px(p.x1), py(p.y1), px(p.x2), py(p.y2)};
    if (t < .4f) {
      auto hatch = CreateHatchBrush(HS_DIAGCROSS, mix(ui::Logic, ui::PlanBlack, t * 1.5f));
      FillRect(dc, &box, hatch);
      DeleteObject(hatch);
    }
    const int grow = int(t * view.cell * 1.2f);
    auto ring = CreatePen(PS_SOLID, std::max(1, int(5 * (1 - t))), mix(ui::Logic, ui::PlanBlack, t));
    auto oldPen = SelectObject(dc, ring);
    Rectangle(dc, box.left - grow, box.top - grow, box.right + grow, box.bottom + grow);
    SelectObject(dc, oldPen);
    DeleteObject(ring);
  }
  // Where everyone is right now: a bright dot in a white ring, apart from the authored marks.
  const int radius = std::max(5, int(view.cell * .38f));
  auto edge = CreatePen(PS_SOLID, 2, ui::Hover);
  auto oldPen = SelectObject(dc, edge);
  for (const auto &m : state.marks) {
    auto fill = CreateSolidBrush(m.kind == 1 ? ui::Creature : m.kind == 2 ? ui::Hazard : ui::People);
    SelectObject(dc, fill);
    const int x = px(m.x), y = py(m.y);
    Ellipse(dc, x - radius, y - radius, x + radius + 1, y + radius + 1);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    DeleteObject(fill);
  }
  SelectObject(dc, oldPen);
  DeleteObject(edge);
  SelectClipRgn(dc, nullptr);
  DeleteObject(clip);
  // A pill at the foot of the plan says what is going on and how to stop it.
  {
    static constexpr wchar_t text[] = L"LIVE    F6 or Esc stops    Click a trigger to fire it";
    auto oldFont = SelectObject(dc, e.smallFont);
    SIZE size{};
    GetTextExtentPoint32W(dc, text, int(std::size(text)) - 1, &size);
    RECT pill{r.right - size.cx - 40, r.bottom - size.cy - 9, r.right - 8, r.bottom - 3};
    ui::roundBox(dc, pill, ui::Sage, ui::SageEdge, 14);
    const bool blink = (GetTickCount64() / 500) % 2 == 0;
    auto dot = CreateSolidBrush(blink ? ui::Brick : ui::SageEdge);
    auto none = CreatePen(PS_NULL, 0, 0);
    auto oldDotPen = SelectObject(dc, none);
    SelectObject(dc, dot);
    const int cy = (pill.top + pill.bottom) / 2;
    Ellipse(dc, pill.left + 10, cy - 4, pill.left + 19, cy + 5);
    SelectObject(dc, oldDotPen);
    DeleteObject(none);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    DeleteObject(dot);
    SetTextColor(dc, ui::SageInk);
    textAt(dc, pill.left + 26, pill.top + (pill.bottom - pill.top - size.cy) / 2, text);
    SelectObject(dc, oldFont);
  }
  SelectObject(dc, oldBrush);
}
// Plan point under the pointer, or false off the map.
inline bool planPoint(const Editor &e, LPARAM l, float &x, float &y) {
  const auto view = e.planView();
  x = (GET_X_LPARAM(l) - view.ox) / view.cell, y = (GET_Y_LPARAM(l) - view.oy) / view.cell;
  return x >= 0 && y >= 0 && x < 24 && y < 24;
}
inline LRESULT CALLBACK planHook(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (!state.editor || !state.planBase)
    return DefWindowProcW(h, m, w, l);
  Editor &e = *state.editor;
  if (m == WM_PAINT) {
    auto result = CallWindowProcW(state.planBase, h, m, w, l);
    if (state.on) {
      HDC dc = GetDC(h);
      RECT r{};
      GetClientRect(h, &r);
      overlay(e, dc, r);
      ReleaseDC(h, dc);
    }
    return result;
  }
  const bool selecting = state.on && e.tool == 0 && !(!e.backgroundTest && (GetKeyState(VK_SPACE) & 0x8000));
  float x = 0, y = 0, box[4];
  if (m == WM_SETCURSOR && selecting && LOWORD(l) == HTCLIENT) {
    POINT at{};
    GetCursorPos(&at);
    ScreenToClient(h, &at);
    if (planPoint(e, MAKELPARAM(at.x, at.y), x, y) && triggerAt(e, x, y, box)) {
      SetCursor(LoadCursorW(nullptr, IDC_HAND));
      return TRUE;
    }
  }
  // While it runs, a click on a trigger fires it; anywhere else picks as usual.
  std::string name;
  if (m == WM_LBUTTONDOWN && selecting && planPoint(e, l, x, y) && triggerAt(e, x, y, box, &name)) {
    fire(e, box);
    e.pop((box[0] + box[2]) * .5f, (box[1] + box[3]) * .5f, ui::Logic);
    e.message(L"Fired \"" + wide(name) + L"\". " + Running);
    return 0;
  }
  return CallWindowProcW(state.planBase, h, m, w, l);
}
inline LRESULT CALLBACK mainHook(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (!state.editor || !state.mainBase)
    return DefWindowProcW(h, m, w, l);
  Editor &e = *state.editor;
  WNDPROC base = state.mainBase;
  if (m == WM_COMMAND && LOWORD(w) == ButtonId) {
    set(e, !state.on);
    return 0;
  }
  if (m == WM_DRAWITEM && reinterpret_cast<DRAWITEMSTRUCT *>(l)->CtlID == UINT(ButtonId)) {
    auto *d = reinterpret_cast<DRAWITEMSTRUCT *>(l);
    // A mode button: sand while off, sage while the map is live.
    const auto look = state.on ? ui::Look::Chosen : ui::Look::Plain;
    RECT body = ui::chunkyButton(d->hDC, d->rcItem, ui::Header, look, ui::hotControl == d->hwndItem,
                                 (d->itemState & ODS_SELECTED) != 0, 12);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, ui::lookInk(look));
    SelectObject(d->hDC, e.buttonFont);
    const wchar_t *text = state.on ? OnLabel : OffLabel;
    DrawTextW(d->hDC, text, -1, &body, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return TRUE;
  }
  if (m == WM_NCDESTROY) {
    if (state.keys)
      UnhookWindowsHookEx(state.keys);
    SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(base));
    state = State{};
    return CallWindowProcW(base, h, m, w, l);
  }
  auto result = CallWindowProcW(base, h, m, w, l);
  if (m == WM_SIZE)
    place(e);
  if (m == WM_TIMER && w == 1 && IsWindow(h))
    poll(e);
  return result;
}
// F6 anywhere in the builder window starts or stops it; Escape stops it. The
// keys are taken before the editor's own shortcuts see them.
inline LRESULT CALLBACK keyHook(int code, WPARAM w, LPARAM l) {
  auto *msg = reinterpret_cast<MSG *>(l);
  if (code == HC_ACTION && w == PM_REMOVE && state.editor && msg->message == WM_KEYDOWN &&
      (msg->wParam == VK_F6 || (msg->wParam == VK_ESCAPE && state.on)) && msg->hwnd &&
      GetAncestor(msg->hwnd, GA_ROOT) == state.editor->window && !state.editor->playing) {
    if (!(msg->lParam & (1 << 30))) // not a key repeat
      set(*state.editor, msg->wParam == VK_F6 ? !state.on : false);
    msg->message = WM_NULL;
  }
  return CallNextHookEx(nullptr, code, w, l);
}
inline void install(Editor &e) {
  state = State{};
  state.editor = &e;
  state.button = e.button(OffLabel, ButtonId);
  if (e.tooltip) {
    TOOLINFOW info{sizeof(info)};
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = e.window;
    info.uId = reinterpret_cast<UINT_PTR>(state.button);
    info.lpszText = const_cast<wchar_t *>(L"Watch your map come alive without playing: guards walk, triggers fire, "
                                          L"lamps flicker. Nobody sees you. F6 or Escape puts everything back.");
    SendMessageW(e.tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
  }
  state.mainBase = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(e.window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&mainHook)));
  state.planBase = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(e.plan, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&planHook)));
  state.keys = SetWindowsHookExW(WH_GETMESSAGE, &keyHook, nullptr, GetCurrentThreadId());
  place(e);
  // A stale request from an earlier session must not start this one running.
  ask(e);
}

// --simulate-test: a Starter map with a patrolling guard and two triggers,
// simulated for a few seconds in the hidden builder with a hidden engine.
inline int runTest(Editor &e) {
  std::vector<std::string> failed;
  std::ostringstream notes;
  notes.setf(std::ios::fixed);
  notes.precision(2);
  auto check = [&](bool ok, const std::string &what) {
    if (!ok)
      failed.push_back(what);
    return ok;
  };
  auto pump = [&](int ms, auto done) {
    const auto until = GetTickCount64() + ULONGLONG(ms);
    while (GetTickCount64() < until) {
      MSG msg{};
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      poll(e);
      if (done())
        return true;
      Sleep(15);
    }
    return false;
  };
  auto marks = [&](int kind) {
    std::vector<Mark> out;
    for (auto &m : state.marks)
      if ((m.kind == 1) == (kind == 1))
        out.push_back(m);
    return out;
  };
  auto people = [&] {
    std::string lines;
    std::istringstream all(state.seen);
    for (std::string line; std::getline(all, line);)
      if (line.rfind("P ", 0) == 0 || line.rfind("C ", 0) == 0)
        lines += line + '\n';
    return lines;
  };
  try {
    // The Starter, map 0 (Welcome), built on with the builder's own tools.
    e.doc.open(e.root / L"custom maps" / L"Starter Map.rmc");
    e.map = 0, e.layer = 0;
    int role = -1;
    for (int i = 0; i < int(std::size(Editor::people)); ++i)
      if (std::wstring(Editor::people[i].label) == L"Security guard")
        role = i;
    SendMessageW(e.roleList, CB_SETCURSEL, role, 0);
    e.placePerson(6, 12);
    auto guard = std::to_string(e.doc.records("ACTOR", 0).size() - 1);
    // A second post for his beat: he walks between the two.
    e.doc.add({"ACTOR_PATROL", "0", guard, "17", "12", "0", "1000"});
    int triggerTool = -1;
    for (int i = 0; i < ToolCount; ++i)
      if (std::string(toolInfo[i].kind) == "SCRIPT_EVENT")
        triggerTool = i;
    e.tool = triggerTool;
    e.gesture(10.2f, 1.2f, 13.8f, 3.8f); // round the start, where the camera is
    e.gesture(3.2f, 17.2f, 5.8f, 19.8f); // across the room, fired by a click on the plan
    e.tool = 0;
    auto events = e.doc.records("SCRIPT_EVENT", 0);
    check(events.size() >= 2, "the Trigger tool made two triggers");
    // The one round the camera waits for a switch that the clicked one turns on,
    // so it can only fire during the run, by itself, because the camera is in it.
    {
      auto around = NativeDocument::fields(e.doc.lines[events[events.size() - 2]]);
      auto clicked = NativeDocument::fields(e.doc.lines[events.back()]);
      around.resize(std::max<size_t>(around.size(), 12));
      around[9] = "sim_test_go", around[10] = "1";
      e.doc.replace(events[events.size() - 2], around);
      e.doc.add({"SCRIPT_ACTION", "0", clicked[2], "STATE", "sim_test_go", "1"});
      events = e.doc.records("SCRIPT_EVENT", 0);
    }
    auto boxOf = [&](size_t index) {
      std::array<float, 4> b{};
      auto f = NativeDocument::fields(e.doc.lines[index]);
      for (int i = 0; i < 4; ++i)
        b[size_t(i)] = std::stof(f[size_t(3 + i)]);
      return b;
    };
    const auto startBox = boxOf(events[events.size() - 2]), clickBox = boxOf(events.back());
    auto firedBox = [&](const std::array<float, 4> &b) {
      for (auto &p : state.pulses)
        if (std::fabs(p.x1 - b[0]) < .01f && std::fabs(p.y1 - b[1]) < .01f && std::fabs(p.x2 - b[2]) < .01f &&
            std::fabs(p.y2 - b[3]) < .01f)
          return true;
      return false;
    };
    // The button sits in the top bar beside the play buttons at both test sizes.
    for (auto size : {POINT{1280, 960}, POINT{1920, 1080}}) {
      MoveWindow(e.window, 0, 0, size.x, size.y, FALSE);
      RECT b{}, from{};
      GetWindowRect(state.button, &b);
      GetWindowRect(GetDlgItem(e.window, PlayStart), &from);
      bool clear = b.right <= from.left && b.right - b.left == 136;
      for (HWND other : e.buttons) {
        RECT o{}, overlap{};
        GetWindowRect(other, &o);
        // The test window is never shown, so ask the style rather than IsWindowVisible.
        if (other != state.button && (GetWindowLongPtrW(other, GWL_STYLE) & WS_VISIBLE) &&
            IntersectRect(&overlap, &o, &b))
          clear = false;
      }
      RECT client{};
      GetClientRect(e.window, &client);
      POINT top{b.left, b.top};
      ScreenToClient(e.window, &top);
      check(clear && top.y >= 49 && top.y + 40 <= 98 && top.x > client.right / 2,
            "Simulate button in the top bar beside Play at " + std::to_string(size.x));
    }

    e.startEngine();
    const bool ready = pump(180000, [&] {
      std::istringstream in(e.channelText(e.status));
      std::string tag;
      long long rev = -1;
      unsigned long long frame = 0;
      int map = 0, play = 0;
      // A few frames drawn: the picture is up, as she would see it before pressing F6.
      return in >> tag >> rev >> map >> play >> frame && tag == "READY" && rev == e.revision && frame >= 5 &&
             !state.seen.empty() && !state.engineOn;
    });
    if (!check(ready, "the engine loaded the map"))
      throw std::runtime_error("engine never became ready");
    const auto authoredPeople = people();
    auto guards = marks(0);
    const Mark *post = nullptr;
    for (auto &m : guards)
      if (std::hypot(m.x - 6, m.y - 12) < .6f)
        post = &m;
    check(post != nullptr, "the guard stands at his post before simulating");
    const Mark start = post ? *post : Mark{6, 12, 0};
    const auto document = e.doc.text();
    const auto revision = e.revision;
    const auto history = e.history.size();

    // Start with the button, exactly as a click does.
    SendMessageW(e.window, WM_COMMAND, MAKEWPARAM(ButtonId, BN_CLICKED), reinterpret_cast<LPARAM>(state.button));
    check(state.on, "the button starts it");
    const auto asked = GetTickCount64();
    check(pump(30000, [&] { return state.engineOn; }), "the engine runs the map");
    notes << "started in " << (GetTickCount64() - asked) << " ms; ";
    // The marks as the plan draws them mid-run, saved for a look (.build/simulate-plan.bmp).
    auto savePlan = [&] {
      RECT r{};
      GetClientRect(e.plan, &r);
      HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
      BITMAPINFO bmi{};
      bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), r.right, -r.bottom, 1, 32, BI_RGB};
      void *bits = nullptr;
      HBITMAP canvas = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      auto old = SelectObject(dc, canvas);
      FillRect(dc, &r, e.planBrush);
      overlay(e, dc, r);
      GdiFlush();
      BITMAPFILEHEADER file{0x4D42, DWORD(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + r.right * r.bottom * 4), 0, 0,
                            DWORD(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))};
      std::ofstream bmp(e.root / L".build" / L"simulate-plan.bmp", std::ios::binary);
      bmp.write(reinterpret_cast<const char *>(&file), sizeof(file));
      bmp.write(reinterpret_cast<const char *>(&bmi.bmiHeader), sizeof(bmi.bmiHeader));
      bmp.write(static_cast<const char *>(bits), std::streamsize(r.right) * r.bottom * 4);
      SelectObject(dc, old);
      DeleteObject(canvas);
      DeleteDC(dc);
      ReleaseDC(nullptr, screen);
    };
    bool sawStart = false, sawClick = false, clicked = false, calm = true, saved = false;
    const int picked = e.record;
    float walked = 0;
    pump(9000, [&] {
      if (!sawStart && firedBox(startBox))
        notes << "start trigger seen at " << state.seconds << " s; ";
      sawStart |= firedBox(startBox);
      sawClick |= clicked && firedBox(clickBox);
      if (sawClick && sawStart && !saved)
        savePlan(), saved = true;
      for (auto &m : marks(0)) {
        if (std::hypot(m.x - start.x, m.y - start.y) < 12)
          walked = std::max(walked, std::hypot(m.x - start.x, m.y - start.y));
        calm &= m.kind != 2;
      }
      if (!clicked && state.seconds > 1.f) {
        // A real click on the trigger, at its middle on the plan.
        const auto view = e.planView();
        const int x = int(view.ox + (clickBox[0] + clickBox[2]) * .5f * view.cell),
                  y = int(view.oy + (clickBox[1] + clickBox[3]) * .5f * view.cell);
        SendMessageW(e.plan, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        SendMessageW(e.plan, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        clicked = true;
      }
      return state.seconds > 4.5f && sawStart && sawClick;
    });
    notes << "guard walked " << walked << " m in " << state.seconds << " s";
    check(sawStart, "the trigger round the camera fired by itself once the clicked one opened its switch");
    check(sawClick, "clicking a trigger on the plan fired it");
    check(walked >= 1.f, "the guard walked his patrol");
    check(calm, "the guard stayed calm with the camera in sight (ghost observer)");
    check(e.record == picked && !e.objectDrag, "the click fired the trigger without picking it up to drag");

    // Escape stops it, through the same key path as the keyboard.
    MSG escape{e.plan, WM_KEYDOWN, VK_ESCAPE, 1};
    keyHook(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&escape));
    check(!state.on && escape.message == WM_NULL, "Escape stops it");
    const auto stopAsked = GetTickCount64();
    check(pump(10000, [&] { return !state.engineOn && state.marks.size() == guards.size(); }), "the engine stops");
    notes << "; stopped in " << (GetTickCount64() - stopAsked) << " ms";
    check(people() == authoredPeople, "everyone is back exactly where they were authored");
    check(e.doc.text() == document && e.revision == revision && e.history.size() == history,
          "the document is unchanged (no edit, no publish, no undo step)");

    // F6 starts it again; an edit stops it first and lands on the authored map.
    MSG f6{e.plan, WM_KEYDOWN, VK_F6, 1};
    keyHook(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&f6));
    check(state.on, "F6 starts it");
    check(pump(30000, [&] { return state.engineOn; }), "the engine runs it again");
    auto patrol = e.doc.records("ACTOR_PATROL", 0);
    const auto unedited = e.doc.text();
    e.selectRecord(int(patrol.back()));
    e.moveSelected(.5f, 0);
    poll(e);
    check(!state.on, "an edit stops it in the builder");
    check(pump(20000, [&] {
            std::istringstream in(e.channelText(e.status));
            std::string tag;
            long long rev = -1;
            return !state.engineOn && in >> tag >> rev && tag == "READY" && rev == e.revision;
          }),
          "an edit stops it in the engine, which then shows the edit");
    check(e.doc.text() != unedited && e.dirty, "the edit itself is kept");
  } catch (const std::exception &error) {
    failed.push_back(error.what());
  }
  std::ofstream out(e.root / L"native-editor-simulate-test.txt");
  if (failed.empty())
    out << "PASS: Simulate on a Starter map: the button and F6 start it, a patrolling guard walks (" << notes.str()
        << ") and stays calm with the camera in sight, the trigger round the camera fires by itself, a click on the "
           "plan fires another, Escape stops it and puts everyone back exactly, the document is unchanged, an edit "
           "stops it first\n";
  else {
    out << "FAIL:";
    for (auto &f : failed)
      out << ' ' << f << ';';
    out << " (" << notes.str() << ")\n";
  }
  e.dirty = false;
  DestroyWindow(e.window);
  return failed.empty() ? 0 : 1;
}
} // namespace simulate
