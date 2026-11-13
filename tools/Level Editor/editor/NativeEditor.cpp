#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "../src/world/ModelIds.h"
#include "NativeCommands.h"
#include "NativeDocument.h"
#include "NativeObjects.h"
#include "NativeReferences.h"
#include "NativeParts.h"
#include "NativeCreation.h"
#include "NativeManipulation.h"
#include "NativeCorridorRoute.h"
#include "../src/core/EditorView.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cwctype>
#include <initializer_list>
#include <iterator>
#include <commctrl.h>
#include <commdlg.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <map>
#include <set>
#include <optional>
#include <array>
#include <memory>
#include <regex>
#include <shellapi.h>
#include <thread>
#include <windowsx.h>
#include <mmsystem.h>
#include <cstring>
#include <fstream>
#ifdef RAWMETAL_UNIFIED
#include "../src/platform/EditorPreview.h"
#endif
#include "../src/core/CampaignAdmin.h" // Open from campaign: admin unlock
#include "../src/platform/EditorChannel.h"  // the picture's control and status, in memory when it runs in this process
#include "../src/renderer/SignArt.h"        // sign plates: the Sign designer previews the engine's own pixels
// Sign pictures (PNG, JPEG, BMP, TGA) for the Sign designer; static so it never
// meets the engine's own copy when the builder is linked into RawMetal.exe.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#pragma warning(push, 0)
#include "../src/ThirdParty/stb/stb_image.h"
#pragma warning(pop)

namespace fs = std::filesystem;
struct EditorDoor {
  bool fresh = true;
  fs::path project;
};
static_assert(L'·' == L'\u00b7',
              "Build this editor with UTF-8 source encoding");
static std::filesystem::path sessionTrace;
static void trace(const std::string &step) {
  if (sessionTrace.empty())
    return;
  std::ofstream out(sessionTrace, std::ios::app);
  out << GetTickCount64() << ' ' << step << '\n';
}
// --perf-trace (or --editor-perf-test): what each interaction costs on the
// window's thread, in milliseconds, summed into the session trace every two
// seconds. Off unless asked for; a scope then costs two counter reads.
namespace edperf {
inline bool on = false;
// --perf-baseline (with --editor-perf-test): the paths as they were before the
// speed-up, for before/after numbers from one harness: every document query a
// full scan, the plan reading tiles one by one, the picture fed through files
// and reparsing (and building) the whole campaign per edit, the old throttles.
inline bool baseline = false;
struct Stat {
  int n = 0;
  double sum = 0, max = 0;
};
inline std::map<std::string, Stat> stats;
inline ULONGLONG flushedAt = 0;
inline double now() {
  static const double scale = [] {
    LARGE_INTEGER f{};
    QueryPerformanceFrequency(&f);
    return 1000.0 / double(f.QuadPart);
  }();
  LARGE_INTEGER c{};
  QueryPerformanceCounter(&c);
  return double(c.QuadPart) * scale;
}
inline void note(const char *what, double ms) {
  if (!on)
    return;
  auto &s = stats[what];
  ++s.n, s.sum += ms, s.max = std::max(s.max, ms);
}
inline void flush(bool force = false) {
  if (!on || stats.empty() || (!force && GetTickCount64() - flushedAt < 2000))
    return;
  flushedAt = GetTickCount64();
  for (auto &[what, s] : stats) {
    std::ostringstream line;
    line.setf(std::ios::fixed);
    line.precision(2);
    line << "perf " << what << " n " << s.n << " avg " << s.sum / s.n << " max " << s.max << " ms";
    trace(line.str());
  }
  stats.clear();
}
// Consecutive sections of one function: each lap notes the time since the last.
struct Laps {
  double at = on ? now() : 0;
  void lap(const char *what) {
    if (!on)
      return;
    double t = now();
    note(what, t - at);
    at = t;
  }
};
struct Scope {
  const char *what;
  double start;
  explicit Scope(const char *w) : what(w), start(on ? now() : 0) {}
  ~Scope() {
    if (on)
      note(what, now() - start);
  }
};
} // namespace edperf
static void textAt(HDC dc, int x, int y, const wchar_t *text) {
  TextOutW(dc, x, y, text, lstrlenW(text));
}
// The look: a 90s plan (black, crisp grid, bright colours by kind) inside a
// warm, soft frame of cream, sand, sage and terracotta.
namespace ui {
// Frame and panels.
constexpr COLORREF Frame = RGB(0xF2, 0xEA, 0xDB),     // warm cream behind everything
    Header = RGB(0xE7, 0xDA, 0xC2),                     // sand band along the top
    HeaderLine = RGB(0xD5, 0xC3, 0xA4),
    Paper = RGB(0xFB, 0xF7, 0xEF),                      // cards: left tools, right inspector
    PaperEdge = RGB(0xE2, 0xD4, 0xBC),
    Field = RGB(0xFF, 0xFD, 0xF8),                      // text boxes and lists
    FieldEdge = RGB(0xC9, 0xB6, 0x98),
    Ink = RGB(0x3B, 0x31, 0x29),                        // gentle dark text
    InkSoft = RGB(0x7A, 0x6B, 0x5C),                    // captions
    InkFaint = RGB(0xB4, 0xA6, 0x93),                   // nothing to undo
    Sand = RGB(0xEA, 0xDD, 0xC6),                       // idle button
    SandEdge = RGB(0xCB, 0xB6, 0x95),
    SandLip = RGB(0xD3, 0xC1, 0xA2),
    Sage = RGB(0x9F, 0xC2, 0x92),                       // chosen tool, mode, page
    SageEdge = RGB(0x5F, 0x8F, 0x57),
    SageLip = RGB(0x7C, 0xA3, 0x70),
    SageInk = RGB(0x1F, 0x33, 0x1B),
    SageWash = RGB(0xE2, 0xEE, 0xDB),                   // list pick
    Terracotta = RGB(0xDE, 0x74, 0x4C),                 // Play
    TerracottaEdge = RGB(0xB0, 0x52, 0x31),
    TerracottaLip = RGB(0xB5, 0x57, 0x36),
    Brick = RGB(0xB8, 0x4E, 0x3A),                      // Play while playing (Stop)
    Peach = RGB(0xF4, 0xCD, 0xB6),                      // Play from start
    PeachEdge = RGB(0xD8, 0x9A, 0x78),
    PeachLip = RGB(0xE0, 0xAE, 0x92),
    Glow = RGB(0xEE, 0xB8, 0x5C),                       // soft honey hover ring
    Bezel = RGB(0x3A, 0x32, 0x2B);                      // warm frame around the black plan
// Icon chips on the tool tiles, one tint per page.
constexpr COLORREF PageTint[] = {RGB(0xF3, 0xC9, 0xB3), RGB(0xF4, 0xDF, 0xA8), RGB(0xCD, 0xE2, 0xC2),
                                 RGB(0xDD, 0xCE, 0xEC), RGB(0xC6, 0xDF, 0xEB)};
// The plan, 90s editor style.
constexpr COLORREF PlanBlack = RGB(0, 0, 0), GridMinor = RGB(0x2A, 0x2C, 0x30),
                   GridMajor = RGB(0x5C, 0x5E, 0x62), GridEdge = RGB(0x80, 0x80, 0x80),
                   FloorFill = RGB(0x15, 0x18, 0x1F), WallFill = RGB(0x5E, 0x5E, 0x5E),
                   WallLine = RGB(0xFF, 0xFF, 0xFF), DeckFill = RGB(0x1A, 0x2C, 0x44),
                   CrateFill = RGB(0x3A, 0x2C, 0x12), CrateInk = RGB(0xFF, 0xC8, 0x64),
                   PlanText = RGB(0xDD, 0xD6, 0xC8),
                   Wall = RGB(0xFF, 0xFF, 0xFF), Door = RGB(0xFF, 0xE0, 0x1A),
                   Lamp = RGB(0x00, 0xE5, 0xFF), People = RGB(0x2E, 0xE0, 0x5A),
                   Creature = RGB(0xFF, 0x30, 0x30), Hazard = RGB(0xFF, 0x8A, 0x00),
                   Water = RGB(0x3C, 0x7C, 0xFF), Logic = RGB(0xFF, 0x3C, 0xF0),
                   Darkness = RGB(0x8C, 0x7C, 0xFF), Ground = RGB(0xB8, 0xD0, 0x50),
                   Flatten = RGB(0x6F, 0x92, 0xB4), Prop = RGB(0xA9, 0xB6, 0xC4),
                   Start = RGB(0xFF, 0xFF, 0xFF), Selected = RGB(0xC8, 0xFF, 0x00),
                   Hover = RGB(0xFF, 0xFF, 0xFF);
// Film cameras on the plan (NativeFilmTool.inc).
inline constexpr COLORREF Film = RGB(0x40, 0xE0, 0xC8);
// Colour by record kind on the plan.
inline COLORREF kindColor(const std::string &k) {
  if (k == "LIGHT")
    return Lamp;
  if (k.starts_with("SCRIPT") || k == "ZONE" || k == "TERMINAL" || k == "TIMED_SEQUENCE")
    return Logic;
  if (k.starts_with("ACTOR") || k == "PICKUP")
    return People;
  if (k == "CREATURE")
    return Creature;
  if (k == "HAZARD" || k == "COMPACTOR")
    return Hazard;
  if (k == "WATER")
    return Water;
  if (k == "DOOR")
    return Door;
  if (k == "STRUCT" || k == "PRIM" || k == "STAIR" || k == "BREAKABLE")
    return Wall;
  if (k == "DARK")
    return Darkness;
  if (k == "GROUND")
    return Ground;
  if (k == "TERRAIN_FLAT")
    return Flatten;
  return Prop;
}
inline HPEN dashedPen(int width, COLORREF color) {
  LOGBRUSH brush{BS_SOLID, color, 0};
  DWORD dashes[] = {DWORD(width * 3 + 2), DWORD(width * 2 + 1)};
  return ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT | PS_JOIN_MITER, DWORD(width),
                      &brush, 2, dashes);
}
inline void roundBox(HDC dc, RECT r, COLORREF fill, COLORREF edge, int radius, int edgeWidth = 1) {
  auto brush = CreateSolidBrush(fill);
  auto pen = CreatePen(PS_INSIDEFRAME, edgeWidth, edge);
  auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, pen);
  RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(brush);
  DeleteObject(pen);
}
enum class Look { Plain, Chosen, Play, Playing, PlayFrom, Idle };
// A chunky, rounded button: a darker lip underneath that it sinks into when
// pressed, and a soft honey ring while the pointer is over it.
inline RECT chunkyButton(HDC dc, RECT r, COLORREF under, Look look, bool hot, bool pressed, int radius = 12) {
  auto fillUnder = CreateSolidBrush(under);
  FillRect(dc, &r, fillUnder);
  DeleteObject(fillUnder);
  COLORREF fill = Sand, edge = SandEdge, lip = SandLip;
  switch (look) {
  case Look::Chosen: fill = Sage, edge = SageEdge, lip = SageLip; break;
  case Look::Play: fill = Terracotta, edge = TerracottaEdge, lip = TerracottaLip; break;
  case Look::Playing: fill = Brick, edge = RGB(0x8C, 0x36, 0x26), lip = RGB(0x8C, 0x36, 0x26); break;
  case Look::PlayFrom: fill = Peach, edge = PeachEdge, lip = PeachLip; break;
  case Look::Idle: fill = RGB(0xEF, 0xE7, 0xD9), edge = RGB(0xDD, 0xD0, 0xBB), lip = fill; break;
  default: break;
  }
  if (hot && look != Look::Idle && !pressed)
    fill = RGB(std::min(255, GetRValue(fill) + 10), std::min(255, GetGValue(fill) + 10),
               std::min(255, GetBValue(fill) + 10));
  if (pressed)
    fill = RGB(GetRValue(fill) * 9 / 10, GetGValue(fill) * 9 / 10, GetBValue(fill) * 9 / 10);
  const int lipHeight = look == Look::Idle ? 0 : 3;
  RECT body{r.left, r.top, r.right, r.bottom - lipHeight};
  if (pressed)
    OffsetRect(&body, 0, lipHeight - 1);
  if (lipHeight && !pressed)
    roundBox(dc, RECT{r.left, r.top + lipHeight, r.right, r.bottom}, lip, edge, radius);
  roundBox(dc, body, fill, hot && look != Look::Idle ? Glow : edge, radius, hot && look != Look::Idle ? 2 : 1);
  return body;
}
inline COLORREF lookInk(Look look) {
  return look == Look::Idle             ? InkFaint
         : look == Look::Chosen         ? SageInk
         : look == Look::Play || look == Look::Playing ? RGB(0xFF, 0xFC, 0xF6)
                                         : Ink;
}
// Hover tracking and soft field borders for the standard controls.
inline HWND hotControl = nullptr;
inline void (*clickSound)() = [] {};
inline void (*hoverSound)(HWND) = [](HWND) {};
static LRESULT CALLBACK cosyControlProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto base = reinterpret_cast<WNDPROC>(GetPropW(h, L"RMCosyBase"));
  int kind = int(reinterpret_cast<INT_PTR>(GetPropW(h, L"RMCosyKind")));
  if (!base)
    return DefWindowProcW(h, m, w, l);
  if (m == WM_NCDESTROY) {
    SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(base));
    RemovePropW(h, L"RMCosyBase");
    RemovePropW(h, L"RMCosyKind");
    if (hotControl == h)
      hotControl = nullptr;
    return CallWindowProcW(base, h, m, w, l);
  }
  if (kind == 1) {
    if (m == WM_LBUTTONDOWN)
      clickSound();
    if (m == WM_MOUSEMOVE && hotControl != h) {
      HWND old = hotControl;
      hotControl = h;
      hoverSound(h);
      if (old && IsWindow(old))
        InvalidateRect(old, nullptr, FALSE);
      InvalidateRect(h, nullptr, FALSE);
      TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, h, 0};
      TrackMouseEvent(&track);
    } else if (m == WM_MOUSELEAVE && hotControl == h) {
      hotControl = nullptr;
      InvalidateRect(h, nullptr, FALSE);
    } else if (m == WM_ERASEBKGND)
      return 1;
    return CallWindowProcW(base, h, m, w, l);
  }
  if (kind == 2 && (m == WM_SETFOCUS || m == WM_KILLFOCUS)) {
    auto result = CallWindowProcW(base, h, m, w, l);
    RedrawWindow(h, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    return result;
  }
  if (kind == 2 && m == WM_NCPAINT) {
    // Scroll bars live in the frame: let them paint, then soften the border.
    auto result = CallWindowProcW(base, h, m, w, l);
    HDC dc = GetWindowDC(h);
    RECT r{};
    GetWindowRect(h, &r);
    OffsetRect(&r, -r.left, -r.top);
    auto brush = CreateSolidBrush(GetFocus() == h ? SageEdge : FieldEdge);
    FrameRect(dc, &r, brush);
    DeleteObject(brush);
    ReleaseDC(h, dc);
    return result;
  }
  if (kind == 3 && m == WM_PAINT) {
    // The classic drop-down keeps its behaviour; only its frame and arrow are redrawn.
    auto result = CallWindowProcW(base, h, m, w, l);
    COMBOBOXINFO info{sizeof(info)};
    if (GetComboBoxInfo(h, &info)) {
      HDC dc = GetDC(h);
      RECT r{};
      GetClientRect(h, &r);
      auto field = CreateSolidBrush(Field);
      RECT ring = r;
      for (int i = 0; i < 2; ++i) {
        FrameRect(dc, &ring, field);
        InflateRect(&ring, -1, -1);
      }
      DeleteObject(field);
      RECT button = info.rcButton;
      button.left -= 1;
      auto sand = CreateSolidBrush(Sand);
      FillRect(dc, &button, sand);
      DeleteObject(sand);
      auto line = CreateSolidBrush(SandEdge);
      RECT divider{button.left, r.top + 1, button.left + 1, r.bottom - 1};
      FillRect(dc, &divider, line);
      DeleteObject(line);
      // A soft chevron.
      int cx = (button.left + button.right) / 2, cy = (button.top + button.bottom) / 2;
      auto pen = CreatePen(PS_SOLID, 2, Ink);
      auto oldPen = SelectObject(dc, pen);
      MoveToEx(dc, cx - 4, cy - 2, nullptr);
      LineTo(dc, cx, cy + 2);
      LineTo(dc, cx + 5, cy - 3);
      SelectObject(dc, oldPen);
      DeleteObject(pen);
      auto edge = CreateSolidBrush(GetFocus() == h || GetFocus() == info.hwndList ? SageEdge : FieldEdge);
      FrameRect(dc, &r, edge);
      DeleteObject(edge);
      ReleaseDC(h, dc);
    }
    return result;
  }
  return CallWindowProcW(base, h, m, w, l);
}
// kind 1: owner-drawn button (hover glow), 2: bordered edit or list, 3: drop-down.
inline void cosy(HWND h, int kind) {
  if (!h || GetPropW(h, L"RMCosyBase"))
    return;
  SetPropW(h, L"RMCosyKind", reinterpret_cast<HANDLE>(INT_PTR(kind)));
  SetPropW(h, L"RMCosyBase",
           reinterpret_cast<HANDLE>(SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(cosyControlProc))));
}
// One list or drop-down row: sage wash when picked, gentle ink always.
inline void drawListRow(const DRAWITEMSTRUCT *d, HFONT font) {
  if (d->itemID == UINT(-1) && !(d->itemState & ODS_COMBOBOXEDIT)) {
    auto brush = CreateSolidBrush(Field);
    FillRect(d->hDC, &d->rcItem, brush);
    DeleteObject(brush);
    return;
  }
  bool edit = d->itemState & ODS_COMBOBOXEDIT;
  bool picked = (d->itemState & ODS_SELECTED) && !edit;
  bool focused = edit && (d->itemState & ODS_FOCUS);
  auto brush = CreateSolidBrush(picked ? Sage : focused ? SageWash : Field);
  FillRect(d->hDC, &d->rcItem, brush);
  DeleteObject(brush);
  std::wstring text;
  bool combo = d->CtlType == ODT_COMBOBOX;
  if (d->itemID != UINT(-1)) {
    auto n = SendMessageW(d->hwndItem, combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN, d->itemID, 0);
    if (n > 0) {
      text.resize(size_t(n) + 1);
      SendMessageW(d->hwndItem, combo ? CB_GETLBTEXT : LB_GETTEXT, d->itemID,
                   reinterpret_cast<LPARAM>(text.data()));
      text.resize(size_t(n));
    }
  }
  SetBkMode(d->hDC, TRANSPARENT);
  SetTextColor(d->hDC, picked ? SageInk : Ink);
  auto old = SelectObject(d->hDC, font);
  RECT box = d->rcItem;
  box.left += 6;
  box.right -= 4;
  DrawTextW(d->hDC, text.c_str(), int(text.size()), &box,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
  SelectObject(d->hDC, old);
}
} // namespace ui
// Soft building sounds: the Creating Constructs clips, cut by PrepareSounds.py
// into .build/sounds. Each play nudges pitch and level so nothing repeats.
#pragma comment(lib, "winmm.lib")
namespace sfx {
enum Clip { PlaceStone, PlaceWood, PlaceMetal, BreakStone, BreakWood, BreakMetal, Click, Hover, Back, Slider, MusicKey, Count };
inline const char *const names[] = {"placeStone", "placeWood", "placeMetal", "breakStone", "breakWood", "breakMetal",
                                    "click", "hover", "back", "slider", "musicKey"};
struct Pcm {
  std::vector<int16_t> samples;
  int rate = 16000;
};
inline Pcm bank[Count];
inline bool muted = false, enabled = false;
inline unsigned rng = 0x9e3779b9u;
inline float random01() {
  rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
  return float(rng & 0xffffff) / float(0xffffff);
}
inline void load(const std::filesystem::path &folder) {
  for (int i = 0; i < Count; ++i) {
    std::ifstream in(folder / (std::string(names[i]) + ".wav"), std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes.size() < 44 || bytes.compare(0, 4, "RIFF") != 0)
      continue;
    size_t at = 12;
    while (at + 8 <= bytes.size()) {
      std::string id = bytes.substr(at, 4);
      uint32_t size = 0;
      memcpy(&size, bytes.data() + at + 4, 4);
      if (id == "fmt " && at + 16 <= bytes.size()) {
        uint32_t rate = 0;
        memcpy(&rate, bytes.data() + at + 12, 4);
        bank[i].rate = int(rate);
      } else if (id == "data") {
        size = std::min<uint32_t>(size, uint32_t(bytes.size() - at - 8));
        bank[i].samples.resize(size / 2);
        memcpy(bank[i].samples.data(), bytes.data() + at + 8, bank[i].samples.size() * 2);
        break;
      }
      at += 8 + size + (size & 1);
    }
  }
  enabled = !bank[Click].samples.empty();
}
// pitch and gain spread around 1; the clip is resampled into a fresh WAV.
inline void play(Clip clip, float gain = 1.f, float spread = .07f) {
  if (!enabled || muted || bank[clip].samples.empty())
    return;
  edperf::Scope perfScope("sound");
  static std::vector<char> ring[4];
  static int next = 0;
  auto &out = ring[next++ % 4];
  const auto &in = bank[clip].samples;
  float pitch = 1.f + (random01() * 2 - 1) * spread;
  float level = gain * (.78f + .22f * random01()) * .8f;
  size_t frames = size_t(in.size() / pitch);
  out.assign(44 + frames * 2, 0);
  auto put32 = [&](size_t at, uint32_t v) { memcpy(out.data() + at, &v, 4); };
  auto put16 = [&](size_t at, uint16_t v) { memcpy(out.data() + at, &v, 2); };
  memcpy(out.data(), "RIFF", 4), put32(4, uint32_t(36 + frames * 2)), memcpy(out.data() + 8, "WAVEfmt ", 8);
  put32(16, 16), put16(20, 1), put16(22, 1), put32(24, uint32_t(bank[clip].rate)),
      put32(28, uint32_t(bank[clip].rate * 2)), put16(32, 2), put16(34, 16);
  memcpy(out.data() + 36, "data", 4), put32(40, uint32_t(frames * 2));
  auto *pcm = reinterpret_cast<int16_t *>(out.data() + 44);
  for (size_t i = 0; i < frames; ++i) {
    float t = i * pitch;
    size_t a = std::min(size_t(t), in.size() - 1), b = std::min(a + 1, in.size() - 1);
    float v = (in[a] + (in[b] - in[a]) * (t - float(a))) * level;
    pcm[i] = int16_t(std::clamp(v, -32768.f, 32767.f));
  }
  PlaySoundW(reinterpret_cast<LPCWSTR>(out.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
// What a thing is made of decides how it lands.
inline int material(const std::string &kind) {
  if (kind == "PIPE" || kind == "LEAD" || kind == "LIGHT" || kind == "TERMINAL" || kind == "COMPACTOR" ||
      kind == "HAZARD" || kind == "EMITTER" || kind == "BREAKABLE")
    return 2;
  if (kind == "FIXTURE" || kind == "SIGN" || kind == "CLUTTER" || kind == "DOOR" || kind.starts_with("ACTOR") ||
      kind == "PICKUP" || kind == "CREATURE")
    return 1;
  return 0;
}
} // namespace sfx
static bool replaceEditorFile(const fs::path &source, const fs::path &target) {
  for (int attempt = 0; attempt < 25; ++attempt) {
    if (MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return true;
    auto error = GetLastError();
    if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED)
      return false;
    Sleep(2);
  }
  return false;
}
static std::wstring wide(const std::string &s) {
  if (s.empty())
    return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring out(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n);
  return out;
}
static std::string utf8(const std::wstring &s) {
  if (s.empty())
    return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0,
                              nullptr, nullptr);
  std::string out(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n,
                      nullptr, nullptr);
  return out;
}
static const wchar_t *named(const std::string &key,
                            std::initializer_list<std::pair<const char *, const wchar_t *>> table) {
  for (auto [k, v] : table)
    if (key == k)
      return v;
  return nullptr;
}
static std::wstring plainKind(const std::string &kind) {
  if (auto *n = named(kind, {{"ACTOR", L"Person"},
                             {"CLUTTER", L"Loose prop"},
                             {"CREATURE", L"Creature"},
                             {"DOOR", L"Door"},
                             {"FIXTURE", L"Equipment"},
                             {"GROUND", L"Ground cover"},
                             {"HAZARD", L"Hazard"},
                             {"LAYER", L"Floor"},
                             {"LEAD", L"Cable"},
                             {"LIGHT", L"Lamp"},
                             {"MAP", L"Map"},
                             {"PIPE", L"Pipe"},
                             {"PRIM", L"Architecture"},
                             {"SIGN", L"Sign"},
                             {"AMBIENT", L"Sound"},
                             {"REVERB", L"Room reverb"},
                             {"STAIR", L"Stairs"},
                             {"STRUCT", L"Block"},
                             {"SURFACE", L"Room surfaces"},
                             {"TERMINAL", L"Computer"},
                             {"TERRAIN_FLAT", L"Level pad"},
                             {"WATER", L"Water"},
                             {"ZONE", L"Place name"},
                             {"SCRIPT_EVENT", L"Trigger"},
                             {"SCRIPT_ACTION", L"Trigger step"},
                             {"SCRIPT_REQUIRE", L"Trigger condition"},
                             {"SCRIPT_TRIGGER", L"How the trigger fires"},
                             {"DARK", L"Darkness"},
                             {"EMITTER", L"Emitter"},
                             {"# EDITOR_FILM", L"Film camera"},
                             {"COMPACTOR", L"Crusher"},
                             {"BREAKABLE", L"Glass"},
                             {"PICKUP", L"Health or ammo"},
                             {"ACTOR_KEY", L"Person"},
                             {"ACTOR_AI", L"Person"},
                             {"ACTOR_PATROL", L"Guard post"},
                             {"TIMED_SEQUENCE", L"Clock"},
                             {"TERRAIN", L"Land"},
                             {"HORIZON", L"Tree line"},
                             {"CAMPAIGN", L"Campaign"},
                             {"ROW", L"Tiles"}}))
    return n;
  return wide(kind);
}
static std::wstring plainField(const std::string &name) {
  if (auto *n = named(name, {{"yaw", L"Turn"},
                             {"pitch", L"Tilt"},
                             {"base", L"Lift"},
                             {"kelvin", L"Warmth"},
                             {"intensity", L"Brightness"},
                             {"range", L"Reach"},
                             {"model", L"Model"},
                             {"material", L"Material"},
                             {"width", L"Width"},
                             {"depth", L"Depth"},
                             {"height", L"Height"},
                             {"solid", L"Solid"},
                             {"x", L"X"},
                             {"y", L"Y"},
                             {"z", L"Z"},
                             {"title", L"Title"},
                             {"name", L"Name"}}))
    return n;
  auto shown = wide(name);
  if (!shown.empty() && shown[0] >= L'a' && shown[0] <= L'z')
    shown[0] = wchar_t(towupper(shown[0]));
  return shown;
}
static std::wstring label(HWND h) {
  int n = GetWindowTextLengthW(h);
  std::wstring s(n + 1, 0);
  GetWindowTextW(h, s.data(), n + 1);
  s.resize(n);
  return s;
}
static std::string decoded(std::string s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      char *e = nullptr;
      auto hex = s.substr(i + 1, 2);
      long n = strtol(hex.c_str(), &e, 16);
      if (e == hex.c_str() + 2) {
        out += char(n);
        i += 2;
        continue;
      }
    }
    out += s[i];
  }
  return out;
}
static std::string encoded(const std::string &s) {
  std::string out;
  const char *h = "0123456789ABCDEF";
  for (unsigned char c : s) {
    if (c == '%' || c == '|' || c == '\r' || c == '\n') {
      out += '%';
      out += h[c >> 4];
      out += h[c & 15];
    } else
      out += char(c);
  }
  return out;
}
static std::string html(std::string s) {
  for (auto [a, b] :
       std::vector<std::pair<std::string, std::string>>{{"&lt;", "<"},
                                                        {"&gt;", ">"},
                                                        {"&quot;", "\""},
                                                        {"&#x27;", "'"},
                                                        {"&amp;", "&"}}) {
    size_t p = 0;
    while ((p = s.find(a, p)) != std::string::npos) {
      s.replace(p, a.size(), b);
      p += b.size();
    }
  }
  return std::regex_replace(s, std::regex("<[^>]*>"), "");
}
struct Schema {
  std::string kind, description;
  std::vector<std::string> fields;
  int required = 0;
  bool scoped = true;
};
enum {
  New = 101,
  Open,
  Save,
  Export,
  Undo,
  Redo,
  Starter,
  Walk,
  Dual,
  PlanOnly,
  ViewOnly,
  AddMap,
  AddFloor,
  Furnish,
  Outdoors,
  SurfaceList,
  Apply,
  Remove,
  Duplicate,
  NewObject,
  MapList,
  LayerList,
  KindList,
  RecordList,
  ModelList,
  MaterialList,
  RoleList,
  GroundList,
  HeightList,
  MusicList,
  ListenMusic,
  SnapList,
  ModelSearch,
  MapSearch,
  Advanced,
  FullGame,
  PlayStart,
  HazardList,
  PickupList,
  CreatureList,
  TileList,
  PrimList,
  BrushList,
  BehaviourList,
  ActionList,
  ClutterList,
  LinkList,
  AutoDetail,
  AutoDressing,
  TurnSelected,
  SoundToggle,
  FlickerToggle,
  BrightSlider,
  ReachSlider,
  WarmSlider,
  StartFresh,
  FromCampaign = 186, // Open from campaign
  IntoGame,
  BackToOriginal,
  ExportChanges,
  IdeaBase = 190,
  MoodBase = 200,
  OutlineWalls = 204,
  OutlineBlock,
  OutlineTerraces,
  ExtrudeList,
  HeightModeList,
  PartTabBase = 210,
  SwatchBase = 230,
  GroupBase = 250,
  ToolBase = 300,
  MapNorth=580,MapSouth,MapWest,MapEast,MapPassage,UpperFloorTiles,
  InspectorTab=520,LayerTab,LayerStack,LayerPreview,LayerMenu,LayerVisible,LayerLock,
  LayerName,LayerElevationField,LayerThicknessField,LayerOpacity,LayerGroupField,LayerCommit,LayerReferences,
  LayerNew,LayerFromVisible,LayerNewGroup,LayerDuplicate,LayerMerge,LayerDelete,LayerRaise,LayerLower,
  LayerMaskArea,LayerMaskInvert,LayerMaskClear,LayerOpaque,LayerHalf,LayerGhost,LayerRotate,LayerFlipX,LayerFlipY,LayerHeight,LayerFootprint,
  SoundList=640,PreyList,HunterList,
  DoorToolList=660,DoorKindList,DoorAxisList,DoorSwingList,DoorHingeList,DoorLockList,DoorStrengthList,DoorOpenSlider,
  EmitterPresetList=690,EmitterKindList,EmitterRestyleList,EmitterHeadingSlider,EmitterStrengthSlider,
  LightViewButton=700,ThirdPersonToggle,FilmWrite,FilmRender,FilmMaker,
  LightMenuBase=710
};
// Every tool keeps its historical number (tests and saved habits use them).
// shape: s select, t tile paint, R room, w window, c copy, v paste, a action,
// p click a point, r drag a box, l drag a line, d door, g player start.
struct ToolInfo {
  const wchar_t *name, *glyph, *hint;
  int group;
  const char *kind;
  char shape;
};
static constexpr ToolInfo toolInfo[] = {
    {L"Select", L"✥", L"Click a thing to pick it. Drag to move. R turns it. Delete removes it.", 0, "", 's'},
    {L"Room", L"▢", L"Drag a box. Walls, a lamp and a door appear by themselves.", 0, "", 'R'},
    {L"Floor", L"▦", L"Paint floor. Choose brush, box or fill on the right.", 0, "", 't'},
    {L"Wall", L"▩", L"Paint walls. Choose brush, box or fill on the right.", 0, "", 't'},
    {L"Erase", L"⌫", L"Paint empty space: no floor and no wall.", 0, "", 't'},
    {L"Balcony", L"☰", L"Paint walkable deck on an upper floor.", 0, "", 't'},
    {L"Lamp", L"☀", L"Click to hang a lamp.", 1, "LIGHT", 'p'},
    {L"Sign", L"✎", L"Click a wall to put up the sign shown in the Sign designer. Click a sign to change it.", 1, "SIGN", 'p'},
    {L"Furniture", L"⌂", L"Pick a model on the right, then click to place it.", 1, "FIXTURE", 'p'},
    {L"Shape", L"▲", L"Click to place an arch, pillar or dome. Pick which on the right.", 0, "PRIM", 'p'},
    {L"Pipe", L"═", L"Drag from one end of the pipe to the other.", 1, "PIPE", 'l'},
    {L"Water", L"≈", L"Drag a pool of water.", 1, "WATER", 'r'},
    {L"Stairs", L"⇗", L"Drag a box. The stairs climb the long way. R turns them.", 0, "STAIR", 'r'},
    {L"Person", L"☺", L"Pick who on the right, then click to place them.", 2, "", 'p'},
    {L"Loose prop", L"◆", L"Click to drop something you can knock about.", 1, "CLUTTER", 'p'},
    {L"Computer", L"▣", L"Click to place a computer. Its screen words are on the right.", 3, "TERMINAL", 'p'},
    {L"Copy area", L"◰", L"Drag over an area to copy everything in it.", 4, "", 'c'},
    {L"Paste area", L"◲", L"Click to paste the copied area.", 4, "", 'v'},
    {L"Turn copy", L"⟳", L"Turns the copied area a quarter.", 4, "", 'a'},
    {L"Flip copy", L"⇋", L"Mirrors the copied area.", 4, "", 'a'},
    {L"Block", L"■", L"Drag a solid block: a plinth, platform or wall piece.", 0, "STRUCT", 'r'},
    {L"Door", L"⌸", L"Click in a gap in a wall, or drag along it, to fit a door. It takes the wall's direction. R turns it.", 0, "DOOR", 'd'},
    {L"Hazard", L"☢", L"Drag an area that hurts. Pick fire, gas or shock on the right.", 3, "HAZARD", 'r'},
    {L"Grass & dirt", L"▒", L"Drag to paint the ground outdoors.", 4, "GROUND", 'r'},
    {L"Flatten", L"▤", L"Drag to level the ground for a road or a building.", 4, "TERRAIN_FLAT", 'r'},
    {L"Player start", L"◉", L"Click where the player begins.", 2, "", 'g'},
    {L"Room look", L"◐", L"Pick the wall and floor finish on the right.", 4, "SURFACE", 'a'},
    {L"Window", L"▥", L"Drag across a wall to cut an opening.", 0, "", 'w'},
    {L"Place name", L"⊞", L"Drag an area. Its name shows when you walk in.", 3, "ZONE", 'r'},
    {L"Trigger", L"✚", L"Drag an area. Something happens when you step in.", 3, "SCRIPT_EVENT", 'r'},
    {L"Health & ammo", L"✚", L"Click to leave health or ammo. Pick which on the right.", 2, "PICKUP", 'p'},
    {L"Cable", L"∿", L"Drag from one end of the cable to the other.", 1, "LEAD", 'l'},
    {L"Emitter", L"♨", L"Pick a preset on the right, then click where it comes from: steam, smoke, sparks, dust, drips, spray, fire, gas or the glow round live current. R points a picked one.", 1, "EMITTER", 'p'},
    {L"Darkness", L"●", L"Drag an area of true darkness.", 1, "DARK", 'r'},
    {L"Crusher", L"⊡", L"Drag an area under a crushing ceiling.", 3, "COMPACTOR", 'r'},
    {L"Glass", L"◇", L"Drag a glass pane that shatters when shot.", 1, "BREAKABLE", 'r'},
    {L"Crates", L"☗", L"Paint crate, barrel or tank tiles.", 1, "", 't'},
    {L"Creature", L"✪", L"Click to place a creature.", 2, "CREATURE", 'p'},
    {L"Paint", L"▧", L"Pick a paint on the right, then click or drag over walls, floors, blocks and shapes. Alt+click picks a paint up.", 0, "", 'P'},
    {L"Draw shape", L"✐", L"Paint any outline, then make it walls, a block or terraces on the right. Alt rubs out.", 0, "", 'F'},
    {L"Corridor", L"⇄", L"Drag from one room to another. A lit corridor joins them.", 0, "", 'K'},
    {L"Raise & lower", L"⇕", L"Paint to raise the floor into platforms, lower it again, or climb in steps. Pick how on the right.", 4, "", 'H'},
    {L"Sound", L"♪", L"Pick a sound on the right, then click where it comes from. Loops hum on; the rest play now and then.", 1, "AMBIENT", 'p'},
    {L"Film camera", L"✇", L"Click to place a named camera, or drag from where it stands toward what it sees. R turns it. Render films them all with the engine.", 4, "# EDITOR_FILM", 'l'},
};
static constexpr int ToolCount = int(std::size(toolInfo)), WindowTool = 27,
                     CrateTool = 36, CreatureTool = 37, PaintTool = 38, ShapeDrawTool = 39, CorridorTool = 40,
                     HeightTool = 41, SoundTool = 42, DoorTool = 21, EmitterTool = 32, FilmTool = 43;
static constexpr const wchar_t *groupNames[] = {L"Build", L"Decor", L"People", L"Logic", L"World"};
// The few fields a person ever needs, in plain words. Everything else waits behind Advanced.
struct Friendly {
  const char *kind, *field;
  const wchar_t *label;
};
static constexpr Friendly friendlyFields[] = {
    {"CAMPAIGN", "name", L"Campaign name"},
    {"MAP", "name", L"Map name"},
    {"MAP", "ceiling", L"Ceiling height (m)"},
    {"MAP", "skybox", L"Sky"},
    {"LAYER", "name", L"Floor name"},
    {"LAYER", "elevation", L"Floor height (m)"},
    {"LIGHT", "intensity", L"Brightness"},
    {"LIGHT", "range", L"Reach (m)"},
    {"LIGHT", "kelvin", L"Warmth (2000 warm - 6500 cold)"},
    {"AMBIENT", "sound", L"Sound (bank name)"},
    {"AMBIENT", "gain", L"Loudness"},
    {"AMBIENT", "radius", L"Heard within (m)"},
    {"AMBIENT", "mode", L"0 loop, 1 now and then, 2 on a switch"},
    {"AMBIENT", "every", L"Seconds between"},
    {"AMBIENT", "spread", L"...and up to this many more"},
    {"AMBIENT", "state", L"Switch (any name)"},
    {"REVERB", "wet", L"Reverb amount (0 - 1)"},
    {"REVERB", "seconds", L"Rings for (seconds)"},
    {"SIGN", "title", L"Big words"},
    {"SIGN", "subtitle", L"Small words"},
    {"TERMINAL", "title", L"Screen title"},
    {"TERMINAL", "line1", L"Line 1"},
    {"TERMINAL", "line2", L"Line 2"},
    {"TERMINAL", "activateState", L"Switch it flips (any name)"},
    {"ZONE", "name", L"Place name"},
    {"ZONE", "title", L"Big title when you enter"},
    {"SCRIPT_EVENT", "id", L"Trigger name"},
    {"SCRIPT_EVENT", "requireState", L"Only when this switch..."},
    {"SCRIPT_EVENT", "requireValue", L"...is set to"},
    {"SCRIPT_ACTION", "STATE", L"Does"},
    {"SCRIPT_ACTION", "name", L"Words or switch name"},
    {"SCRIPT_ACTION", "value", L"Seconds or value"},
    {"SCRIPT_REQUIRE", "name", L"Switch or counter"},
    {"SCRIPT_REQUIRE", "op", L"Is (=, !=, >=, <)"},
    {"SCRIPT_REQUIRE", "value", L"Value"},
    {"SCRIPT_TRIGGER", "where", L"Where (box, map, campaign)"},
    {"SCRIPT_TRIGGER", "match", L"Needs (all, any)"},
    {"SCRIPT_TRIGGER", "repeat", L"Again and again (1 yes)"},
    {"DOOR", "state", L"Opens when switch..."},
    {"DOOR", "value", L"...is set to"},
    {"DOOR", "leaf", L"Leaf (0 standard, 1 grille, 2 panel)"},
    {"DOOR", "label", L"Name in the prompt (GATE)"},
    {"DOOR", "locked", L"Says when locked"},
    {"DOOR", "maxOpen", L"Opens this far (0-1)"},
    {"CREATURE", "wakeState", L"Sleeps until switch"},
    {"CREATURE", "scale", L"Size (0.5 - 2)"},
    {"HAZARD", "damage", L"Hurts per second"},
    {"STRUCT", "top", L"Top (m)"},
    {"STRUCT", "bottom", L"Bottom (m)"},
    {"FIXTURE", "base", L"Lift off the floor (m)"},
    {"ACTOR", "height", L"Size"},
    {"ACTOR_KEY", "yaw", L"Facing"},
    {"STAIR", "top", L"Top (m)"},
    {"STAIR", "steps", L"Steps"},
    {"WATER", "surface", L"Water height (m)"},
    {"WATER", "murk", L"Murk (0 clear - 1 black)"},
    {"PIPE", "z", L"Height (m)"},
    {"PIPE", "endZ", L"Far end height (m)"},
    {"LEAD", "live", L"Shocks (1 yes)"},
    {"BREAKABLE", "health", L"Toughness"},
    {"EMITTER", "z", L"Height it leaves at (m)"},
    {"EMITTER", "driftX", L"Carried east (m; dust, gas: half width)"},
    {"EMITTER", "driftY", L"Carried south (m; dust, gas: half depth)"},
    {"EMITTER", "rise", L"Rises (m; dust: height, gas: depth)"},
    {"EMITTER", "rate", L"Rate: 1 / seconds each lives (0 - 5)"},
    {"EMITTER", "radius", L"Size as it leaves (m; fire: half width)"},
    {"EMITTER", "growth", L"Swells by (fire: flame height)"},
    {"EMITTER", "count", L"How many at once (1 - 64)"},
    {"EMITTER", "kind", L"What: 1 smoke, 2 steam, 3 sparks, 4 dust, 5 grit, 6 drips, 7 spray, 8 fire, 9 gas, 10 current"},
    {"EMITTER", "strength", L"Thickness or brightness (0 - 4)"},
    {"EMITTER", "state", L"Only while switch..."},
    {"EMITTER", "value", L"...is set to"},
    {"EMITTER", "invert", L"Turned round: off while it is (1 yes)"},
    {"EMITTER", "period", L"Repeats every (s; 0 always on)"},
    {"EMITTER", "onTime", L"On for (s of each repeat)"},
    {"EMITTER", "phase", L"Starts this far in (s)"},
    {"# EDITOR_FILM", "name", L"Camera name (its pictures' name)"},
    {"# EDITOR_FILM", "az", L"Lens height (m)"},
    {"# EDITOR_FILM", "turn", L"Turn (degrees; 0 east, 90 south)"},
    {"# EDITOR_FILM", "tilt", L"Tilt (degrees; up is more)"},
    {"# EDITOR_FILM", "seconds", L"Moves for (s; 0 a still)"},
    {"# EDITOR_FILM", "bx", L"...to east (m)"},
    {"# EDITOR_FILM", "by", L"...to south (m)"},
    {"# EDITOR_FILM", "bz", L"...to lens height (m)"},
    {"# EDITOR_FILM", "bturn", L"...turned to (degrees)"},
    {"# EDITOR_FILM", "btilt", L"...tilted to (degrees)"},
    {"# EDITOR_FILM", "claire", L"Turned on Claire at the start (1 yes)"},
    {"COMPACTOR", "period", L"Seconds per crush"},
    {"PRIM", "h", L"Height (0 = usual)"},
    {"SURFACE", "wall", L"Walls (-1 to 12)"},
    {"SURFACE", "floor", L"Floor (-1 to 12)"},
    {"SURFACE", "ceiling", L"Ceiling (-1 to 12)"},
    {"GROUND", "edge", L"Soft edge (m)"},
    {"TERRAIN_FLAT", "height", L"Ground height (m)"},
    {"TERRAIN", "amplitude", L"Hills (0 - 12)"},
    {"HORIZON", "depth", L"Woods depth (m)"},
    {"TIMED_SEQUENCE", "startState", L"Clock name"},
};
static const wchar_t *friendlyLabel(const std::string &kind, const std::string &field) {
  for (auto &f : friendlyFields)
    if (kind == f.kind && field == f.field)
      return f.label;
  return nullptr;
}
// layout() runs on every pick and tool switch and moves ~150 controls; a
// MoveWindow costs about a millisecond even when nothing moves. A control
// already in place is left alone (layout() repaints every pane at its end).
// A drop-down's height is its open list's, not the box's: the last height
// asked for counts as in place while the box itself has not moved.
// Per session: a new editor window may be handed an old window's handle.
static std::map<HWND, int> placedHeights;
static void placeControl(HWND h, int x, int y, int width, int height, BOOL repaint) {
  if (!h)
    return;
  auto &asked = placedHeights;
  RECT now{};
  GetWindowRect(h, &now);
  MapWindowPoints(HWND_DESKTOP, GetParent(h), reinterpret_cast<POINT *>(&now), 2);
  auto it = asked.find(h);
  if (now.left == x && now.top == y && now.right - now.left == width &&
      (now.bottom - now.top == height || (it != asked.end() && it->second == height)))
    return;
  MoveWindow(h, x, y, width, height, repaint);
  asked[h] = height;
}
// The inspector refills its boxes on every pick; a box that already says the
// right thing is not told again (each SetWindowText repaints and notifies).
static void setControlText(HWND h, const wchar_t *text) {
  wchar_t now[512];
  int length = GetWindowTextLengthW(h);
  if (length < int(std::size(now)) && GetWindowTextW(h, now, int(std::size(now))) == length && wcscmp(now, text) == 0)
    return;
  SetWindowTextW(h, text);
}
// Shown or hidden as asked, without a call when it already is.
static void showControl(HWND h, bool shown) {
  if (!h || ((GetWindowLongPtrW(h, GWL_STYLE) & WS_VISIBLE) != 0) == shown)
    return;
  ShowWindow(h, shown ? SW_SHOW : SW_HIDE);
}
struct Editor;
static void drawPlan(Editor *e, HDC dc, const RECT &r);
struct Editor {
  fs::path root, session, pack, control, status, path;
  NativeDocument doc;
  HWND window = nullptr, plan = nullptr, viewport = nullptr, form = nullptr,
       toolShelf = nullptr,
       mapList = nullptr, layerList = nullptr, kindList = nullptr,
       recordList = nullptr, modelList = nullptr, materialList = nullptr,
       roleList = nullptr, groundList = nullptr, surfaceList = nullptr,
       heightList = nullptr,
       musicList = nullptr,
       snapList = nullptr, modelSearch = nullptr, mapSearch = nullptr,
       statusText = nullptr;
  std::vector<std::string> musicNames;
  bool auditionMusic = false;
  std::vector<HWND> fieldInputs, fieldLabels, buttons, tools;
  std::vector<bool> fieldShown;
  std::vector<Schema> schemas;
  std::vector<std::pair<int, std::string>> models;
  std::vector<std::pair<int, std::string>> structureMaterials;
  HFONT font = nullptr, titleFont = nullptr, buttonFont = nullptr, playFont = nullptr,
        headingFont = nullptr, chipFont = nullptr;
  HBRUSH background = CreateSolidBrush(ui::Frame),
         panel = CreateSolidBrush(ui::Paper),
         fieldBrush = CreateSolidBrush(ui::Field),
         planBrush = CreateSolidBrush(ui::PlanBlack);
  PROCESS_INFORMATION engine{};
  std::thread preview;
#ifdef RAWMETAL_UNIFIED
  // The picture window, created on this thread. See startEngine.
  retro::Win32Window* previewWindow = nullptr;
#endif
  long long revision = 0;
  int map = 0, layer = 0, kind = 0, record = -1, tool = 0, formScroll = 0,
      toolScroll = 0,
      viewMode = 0, structureMaterial = 15, groundKind = 5, placeHeight = 0,
      pickSerial = -1;
  bool viewDrag = false, viewRect = false;
  float viewGrabX = 0, viewGrabY = 0;
  int viewX0 = 0, viewY0 = 0, viewX1 = 0, viewY1 = 0;
  std::vector<std::pair<size_t, std::string>> dragMates;
  bool backgroundTest = false, layoutTest = false, editingTest = false, funTest = false, menuTest = false;
  int menuTestAction = 0; // 0 close, 1 play the full game from the editor
  bool menuShown = false;
  int testStage = 0;
  unsigned long long testFrame = 0;
  ULONGLONG testStarted = 0;
  bool playing = false, closed = false, dirty = false, drag = false, quitLoop = false;
  bool playExit = false;
  fs::path *playOut = nullptr;
  POINT anchor{};
  float snap = .25f, dragX = 0, dragY = 0;
  bool objectDrag = false, objectMoved = false;
  std::string objectOriginal, dragDocument;
  std::vector<std::string> objectFields;
  ULONGLONG lastDragPublish = 0;
  long long renderedRevision = -1;
  std::vector<std::string> history, future;
  NativeStamp regionStamp;
  int rotation = 0, mirror = 0;
  std::wstring notice =
      L"Drag on the plan or the picture to draw a room. Right mouse looks. WASD moves.";
  // Friendly state: which palette page, brush shape, hover/ghost, autosave, play.
  // The Sound tool's bank (src/audio/SoundBank.h, read like the model list): name, label, loop, gain, radius.
  struct BankRow { std::string name; std::wstring label; bool loop; std::string gain, radius; };
  std::vector<BankRow> bankSounds;
  HWND soundList = nullptr;
  HWND preyList = nullptr, hunterList = nullptr; // a picked creature's quarry and hunter name (NativeCreatureMethods.inc)
  int soundPick = -1; // tests: the bank row to place, instead of the list's
  HWND hazardList = nullptr, pickupList = nullptr, creatureList = nullptr,
       tileList = nullptr, primList = nullptr, brushList = nullptr,
       behaviourList = nullptr, actionList = nullptr, clutterList = nullptr, linkList = nullptr,
       tooltip = nullptr;
  HFONT glyphFont = nullptr, smallFont = nullptr;
  std::vector<HWND> groupTabs;
  std::vector<std::pair<RECT, std::wstring>> captions;
  size_t headingCaption = size_t(-1);
  // Rich detail (props and a floor in every new room) and smart snapping
  // (furniture backs onto walls). The tool-replay tests turn both off.
  bool richDetail = true, smartSnap = true, autoDressingEnabled = true;
  static unsigned roomHash(int a, int b, int c, int d, int e) {
    unsigned h = 2166136261u;
    for (int v : {a, b, c, d, e})
      h = (h ^ unsigned(v + 7)) * 16777619u;
    return h;
  }
#include "NativeCreationMethods.inc"
#include "NativeDressingMethods.inc"
#include "NativeLayersMethods.inc"
#include "NativeFloorPaintMethods.inc"
#include "NativeSelectionMethods.inc"
#include "NativeGizmoMethods.inc"
#include "NativeLayerDockMethods.inc"
#include "NativeCampaignMethods.inc"
#include "NativeCorridorTests.inc"
#include "NativePersistenceRegression.inc"
#include "NativeSelectionRegression.inc"
#include "NativeLayerRegression.inc"
#include "NativeDressingRegression.inc"
#include "NativeInteractionTests.inc"
#include "NativeSignDesigner.inc"
#include "NativeCreatureMethods.inc"
#include "NativeDoorMethods.inc"
#include "NativeSoundTool.inc"
#include "NativeEmitterTool.inc"
#include "NativeLightView.inc"
#include "NativeFilmTool.inc"
#include "NativeFilmmaker.inc"
  // Furniture dropped near a wall backs onto it and faces the room.
  int snapToWall(int index) {
    auto f = NativeDocument::fields(doc.lines[size_t(index)]);
    if (f.size() < 10 || f[0] != "FIXTURE")
      return index;
    float x = number(f[3], 0), y = number(f[4], 0), d = number(f[7], .5f);
    int cx = int(std::floor(x)), cy = int(std::floor(y));
    struct Option {
      float gap;
      int side;
    } best{.75f, -1};
    auto walls = barriers(); // wall tiles and walls that became blocks
    auto wallAt = [&](int tx, int ty) { return tx >= 0 && ty >= 0 && tx < 24 && ty < 24 && walls[size_t(ty * 24 + tx)] == 1; };
    if (wallAt(cx, cy - 1) && y - cy < best.gap) best = {y - cy, 0};
    if (wallAt(cx, cy + 1) && cy + 1 - y < best.gap) best = {cy + 1 - y, 1};
    if (wallAt(cx - 1, cy) && x - cx < best.gap) best = {x - cx, 2};
    if (wallAt(cx + 1, cy) && cx + 1 - x < best.gap) best = {cx + 1 - x, 3};
    if (best.side < 0)
      return index;
    float w=number(f[6],.5f),h=number(f[8],.5f),z=number(f[5],layerElevation());
    creation::Box box{x,y,w,d,z,z+h,creation::wallYaw(int(number(f[2],0)),best.side)};
    if(best.side==0)box.y=cy+box.halfY()+.08f;
    if(best.side==1)box.y=cy+1-box.halfY()-.08f;
    if(best.side==2)box.x=cx+box.halfX()+.08f;
    if(best.side==3)box.x=cx+1-box.halfX()-.08f;
    auto previous=doc;
    auto automatic=doc.records("FIXTURE",map);
    try{for(auto it=automatic.rbegin();it!=automatic.rend();++it)if(int(*it)!=index&&doc.automatic(*it,layer)){
      auto other=creationBox(NativeDocument::fields(doc.lines[*it]));
      if(other&&creation::overlaps(box,*other,.08f))eraseNativeObject(doc,*it);
    }}catch(...){doc=std::move(previous);return index;}
    // This is the newly placed fixture, always last in native fixture order.
    int movedIndex=int(doc.records("FIXTURE",map).back());
    if(!creationFits(box,nullptr,movedIndex)){doc=std::move(previous);return index;}
    index=movedIndex;
    f[3]=nativeNumber(box.x);f[4]=nativeNumber(box.y);f[9]=nativeNumber(box.yaw);
    doc.replace(size_t(index), f);
    return index;
  }
  // V: another part of the same kind in the same spot.
  void varySelected() {
    if (record < 0 || record >= int(doc.lines.size()))
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (f[0] != "FIXTURE" || f.size() < 9) {
      message(L"V swaps furniture for another of its kind. Pick a piece of furniture first.");
      return;
    }
    int now = int(number(f[2], 0));
    std::string name;
    for (auto &[id, n] : models)
      if (id == now)
        name = n;
    int tab = parts::tabOf(name);
    std::vector<int> same;
    for (auto &[id, n] : models)
      if (id != now && parts::tabOf(n) == tab && id < int(std::size(retro::model::size)) && retro::model::size[id][2] > 0)
        same.push_back(id);
    if (same.empty())
      return;
    int next = same[(roomHash(now, int(changeCount), int(GetTickCount64() & 0xffff), 0, 0)) % same.size()];
    remember();
    f[2] = std::to_string(next);
    f[6] = nativeNumber(retro::model::size[next][0]), f[7] = nativeNumber(retro::model::size[next][1]),
    f[8] = nativeNumber(retro::model::size[next][2]);
    doc.replace(size_t(record), f);
    publish();
    fillFields(false), fieldLayout();
    for (auto &[id, n] : models)
      if (id == next)
        message(parts::nice(n) + L". Press V again for another.");
    sfx::play(sfx::PlaceWood, .8f);
  }
  // ---- Free-form shapes, heights, corridors, lighting, moods, stamps, ideas and celebrations.
  std::vector<char> outline = std::vector<char>(576, 0);
  static constexpr float extrudeHeights[] = {.5f, 1.f, 2.f, 3.f};
  std::set<int> strokeCells;
  float strokeLevel = -1.f;
  ULONGLONG lastStrokeTick = 0;
  int lastRoomX = -1, lastRoomY = -1, completeRooms = 0;
  struct Idea {
    std::wstring label;
    int action;
  };
  std::vector<Idea> ideas;
  std::vector<HWND> ideaButtons, moodButtons;
  HWND historySlider = nullptr, timeSlider = nullptr, extrudeList = nullptr, heightModeList = nullptr;
  bool scrubbing = false;
  static constexpr const wchar_t *moodNames[] = {L"Cosy", L"Eerie", L"Alarm", L"Cold lab"};
  static constexpr const wchar_t *timeNames[] = {L"Night", L"Dawn", L"Day", L"Dusk"};
  int cellOf(float v) const { return std::clamp(int(std::floor(v)), 0, 23); }
  float wallTop() const {
    float ceiling = 4.5f;
    for (auto index : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      ceiling = number(f.size() > 9 ? f[9] : "", 4.5f);
    }
    return ceiling;
  }
  float lampHeight() const {
    return std::max(layerElevation()+.05f,creationCeiling()-.2f);
  }
  // What closes a room: wall tiles, doors and tall blocks.
  std::vector<char> barriers() const {
    std::vector<char> b(576, 0);
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x)
        b[size_t(y * 24 + x)] = doc.tile(map, layer, x, y) == '#';
    for(const auto& a:floorLayers())if(a.elevation<layerElevation()-.01f)for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(doc.tile(map,a.id,x,y)=='#')b[size_t(y*24+x)]=1;
    for (auto i : doc.records("DOOR", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 6 || !onCreationFloor(number(f[5], -999)+bottomLayerElevation()))
        continue;
      // The door's cells along its own axis: a row, or a column for a door along Y.
      auto door = *nativeDoor(f);
      int row = cellOf(door.line);
      for (int x = int(std::floor(door.left)); x < int(std::ceil(door.right)); ++x)
        if (x >= 0 && x < 24)
          b[size_t(door.alongY ? x * 24 + row : row * 24 + x)] = 2;
    }
    float floor = layerElevation();
    for (auto i : doc.records("STRUCT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 8 || number(f[6], 0) >= creationCeiling() || number(f[7], 0) - std::max(floor, number(f[6], 0)) < std::min(2.f,creationCeiling()-floor-.01f))
        continue;
      for (int y = cellOf(number(f[3], 0)); y < std::min(24, int(std::ceil(number(f[5], 0)))); ++y)
        for (int x = cellOf(number(f[2], 0)); x < std::min(24, int(std::ceil(number(f[4], 0)))); ++x)
          b[size_t(y * 24 + x)] = 1;
    }
    return b;
  }
  // The room around a cell, or nothing when that space runs out to the map's edge.
  std::vector<int> roomAt(int x, int y, const std::vector<char> &b) const {
    if (x < 0 || y < 0 || x > 23 || y > 23 || b[size_t(y * 24 + x)])
      return {};
    auto supported=nativeWalkableCells();if(!supported[size_t(y*24+x)])return {};
    std::vector<char> seen(576, 0);
    std::vector<int> stack{y * 24 + x}, cells;
    seen[size_t(y * 24 + x)] = 1;
    while (!stack.empty()) {
      int c = stack.back();
      stack.pop_back();
      cells.push_back(c);
      int cx = c % 24, cy = c / 24;
      if (cx == 0 || cy == 0 || cx == 23 || cy == 23)
        return {};
      for (auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
        int n = (cy + dy) * 24 + cx + dx;
        if (!seen[size_t(n)] && !b[size_t(n)] && supported[size_t(n)])
          seen[size_t(n)] = 1, stack.push_back(n);
      }
    }
    return cells;
  }
  std::vector<int> roomAt(int x, int y) const { return roomAt(x, y, barriers()); }
  std::vector<int> lastRoom() const { return roomAt(lastRoomX, lastRoomY); }
  bool cellIn(const std::vector<int> &cells, float x, float y) const {
    int c = cellOf(y) * 24 + cellOf(x);
    return std::find(cells.begin(), cells.end(), c) != cells.end();
  }
  std::vector<size_t> lightsIn(const std::vector<int> &cells) const {
    std::vector<size_t> out;
    for (auto i : doc.records("LIGHT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 4 && onCreationFloor(number(f[4], -999)) && cellIn(cells, number(f[2], -1), number(f[3], -1)))
        out.push_back(i);
    }
    return out;
  }
  // Auto-lighting: the room's lamps taken down and hung again, evenly.
  int lightRoom(const std::vector<int>& cells,const char* kelvin="3400") {
    clearAutomaticRoom(cells,"LIGHT");return addCreationLights(cells,kelvin);
  }
  // One click sets a room's whole feeling: lamps, haze and floor together.
  void moodRoom(int which) {
    auto cells = lastRoom();
    if (cells.empty()) {
      message(L"Click inside a room first, then pick its mood.");
      return;
    }
    remember();
    if (lightsIn(cells).empty())
      lightRoom(cells);
    struct Mood {
      const char *intensity, *kelvin, *flicker, *r, *g, *b;
      const char *hazeR, *hazeG, *hazeB, *haze;
      int floor;
    };
    static constexpr Mood moods[] = {
        {"1.2", "2600", "0", "1", "0.7", "0.42", "0.85", "0.62", "0.45", "0.02", 51},
        {"0.55", "5200", "0.35", "0.55", "1", "0.62", "0.35", "0.55", "0.42", "0.07", 26},
        {"1.4", "3000", "0.6", "1", "0.16", "0.1", "0.75", "0.18", "0.14", "0.05", 46},
        {"1.6", "6500", "0", "1", "1", "1", "0.72", "0.82", "0.95", "0.015", 27}};
    const auto &m = moods[std::clamp(which, 0, 3)];
    for (auto i : lightsIn(cells)) {
      if(!doc.automatic(i,layer))continue;
      auto f = NativeDocument::fields(doc.lines[i]);
      f.resize(std::max<size_t>(f.size(), 25));
      f[7] = m.intensity, f[9] = m.kelvin, f[17] = m.flicker, f[22] = m.r, f[23] = m.g, f[24] = m.b;
      doc.replace(i, f,true);
    }
    for (auto i : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      static const char *const tail[] = {"-1", "0", "0", "1.570796", "0", "0"};
      for (size_t k = f.size(); k < 23; ++k)
        f.push_back(k >= 17 ? tail[k - 17] : "0");
      f.resize(std::max<size_t>(f.size(), 27));
      f[23] = m.hazeR, f[24] = m.hazeG, f[25] = m.hazeB, f[26] = m.haze;
      doc.replace(i, f);
    }
    clearAutomaticRoom(cells,"STRUCT");addCreationFloor(cells,m.floor);
    publish();
    sfx::play(sfx::Slider, .8f);
    message(std::wstring(moodNames[which]) + L": lamps, haze and floor set for this room. Press F5 to feel it.");
  }
  // A floor of its own: thin slabs over the room, replacing any slab it had.
  void paintRoomFloor(const std::vector<int>& cells,int material) {
    for(int y=0;y<24;++y)for(int x=0;x<24;){if(!cellIn(cells,x+.5f,y+.5f)){++x;continue;}int start=x;while(x<24&&cellIn(cells,x+.5f,y+.5f))++x;paintNativeFloor(float(start),float(y),float(x),float(y+1),material);}
  }
  bool paintWallRun(int cx, int cy, int material) {
    if (doc.tile(map, layer, cx, cy) != '#')
      return false;
    bool across = (cx > 0 && doc.tile(map, layer, cx - 1, cy) == '#') || (cx < 23 && doc.tile(map, layer, cx + 1, cy) == '#');
    int a = across ? cx : cy, b = a;
    auto wallAt = [&](int v) { return across ? doc.tile(map, layer, v, cy) == '#' : doc.tile(map, layer, cx, v) == '#'; };
    while (a > 0 && wallAt(a - 1))
      --a;
    while (b < 23 && wallAt(b + 1))
      ++b;
    for (int v = a; v <= b; ++v)
      doc.paint(map, layer, across ? v : cx, across ? cy : v, '.');
    addAuthored({"STRUCT", std::to_string(map), std::to_string(across ? a : cx), std::to_string(across ? cy : a),
             std::to_string(across ? b + 1 : cx + 1), std::to_string(across ? cy + 1 : b + 1), nativeNumber(layerElevation()),
             nativeNumber(wallTop()), "0", std::to_string(material)});
    return true;
  }
  // ---- Draw shape: the outline becomes walls, a block or terraces.
  bool inOutline(int x, int y) const { return x >= 0 && y >= 0 && x < 24 && y < 24 && outline[size_t(y * 24 + x)]; }
  int outlineCells() const { return int(std::count(outline.begin(), outline.end(), 1)); }
  void outlineDepth(std::vector<int> &depth) const {
    depth.assign(576, 0);
    std::vector<int> queue;
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x)
        if (inOutline(x, y) && (!inOutline(x - 1, y) || !inOutline(x + 1, y) || !inOutline(x, y - 1) || !inOutline(x, y + 1)))
          depth[size_t(y * 24 + x)] = 1, queue.push_back(y * 24 + x);
    for (size_t q = 0; q < queue.size(); ++q) {
      int c = queue[q], x = c % 24, y = c / 24;
      for (auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
        if (inOutline(x + dx, y + dy) && !depth[size_t((y + dy) * 24 + x + dx)])
          depth[size_t((y + dy) * 24 + x + dx)] = depth[size_t(c)] + 1, queue.push_back((y + dy) * 24 + x + dx);
    }
  }
  void makeFromOutline(int how) {
    if (!outlineCells()) {
      message(L"Paint an outline on the plan first.");
      return;
    }
    remember();
    std::vector<int> depth;
    outlineDepth(depth);
    float floor = layerElevation(), height = extrudeHeights[std::clamp(listIndex(extrudeList), 0, 3)];
    int pieces = 0;
    if (how == 0) {
      for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
          if (inOutline(x, y))
            doc.paint(map, layer, x, y, depth[size_t(y * 24 + x)] == 1 ? '#' : '.'), ++pieces;
    } else
      for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24;) {
          int level = inOutline(x, y) ? (how == 2 ? depth[size_t(y * 24 + x)] : 1) : 0;
          if (!level) {
            ++x;
            continue;
          }
          int start = x;
          while (x < 24 && inOutline(x, y) && (how == 2 ? depth[size_t(y * 24 + x)] : 1) == level)
            ++x;
          float top = how == 2 ? floor + height * level / 3.f : floor + height;
          addAuthored({"STRUCT", std::to_string(map), std::to_string(start), std::to_string(y), std::to_string(x),
                   std::to_string(y + 1), nativeNumber(floor), nativeNumber(top), "0", std::to_string(paintMaterial())});
          ++pieces;
        }
    std::fill(outline.begin(), outline.end(), 0);
    publish();
    InvalidateRect(plan, nullptr, FALSE);
    message(how == 0 ? L"Walls raised round your shape." : how == 1 ? L"Your shape is a solid block now. Paint it with P."
                                                                     : L"Terraces climb to the middle of your shape.");
  }
  // ---- Raise & lower: one-metre cells of floor, up or down, or climbing in steps.
  void paintHeight(int cx, int cy) {
    if (GetTickCount64() - lastStrokeTick > 400)
      strokeCells.clear(), strokeLevel = -1.f;
    lastStrokeTick = GetTickCount64();
    if (!strokeCells.insert(cy * 24 + cx).second)
      return;
    float floor = layerElevation(), top = floor;
    int found = -1;
    for (auto i : doc.records("STRUCT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 7 && number(f[2], -1) == cx && number(f[3], -1) == cy && number(f[4], -1) == cx + 1 &&
          number(f[5], -1) == cy + 1 && std::abs(number(f[6], -9) - floor) < .01f)
        found = int(i), top = number(f[7], floor);
    }
    int mode = std::clamp(listIndex(heightModeList), 0, 2);
    float next = mode == 0 ? top + .5f : mode == 1 ? top - .5f : (strokeLevel < 0 ? top : strokeLevel) + .25f;
    if (mode == 2)
      strokeLevel = next;
    next = std::min(next, wallTop() - .2f);
    if (next <= floor + .01f) {
      if (found >= 0)
        eraseNativeObject(doc, size_t(found));
    } else if (found >= 0) {
      auto f = NativeDocument::fields(doc.lines[size_t(found)]);
      f[7] = nativeNumber(next);
      doc.replace(size_t(found), f);
    } else
      addAuthored({"STRUCT", std::to_string(map), std::to_string(cx), std::to_string(cy), std::to_string(cx + 1),
               std::to_string(cy + 1), nativeNumber(floor), nativeNumber(next), "0", std::to_string(paintMaterial())});
    ++changeCount;
    if (!quiet())
      sfx::play(mode == 1 ? sfx::BreakStone : sfx::PlaceStone, .5f, .15f);
    if (liveTurn(150)) {
      publish();
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }
#include "NativeCorridorMethods.inc"
  // ---- Stamps: a copied area kept for good and stamped again from the Stamps tab.
  fs::path stampFolder() const { return root / L".build" / L"stamps"; }
  int stampCount() const {
    int n = 0;
    std::error_code ec;
    while (fs::exists(stampFolder() / (L"stamp-" + std::to_wstring(n + 1) + L".txt"), ec))
      ++n;
    return n;
  }
  void saveStamp() {
    if (regionStamp.width <= 0) {
      message(L"Copy an area first, then save it as a stamp.");
      return;
    }
    std::error_code ec;
    fs::create_directories(stampFolder(), ec);
    int n = stampCount() + 1;
    std::ofstream out(stampFolder() / (L"stamp-" + std::to_wstring(n) + L".txt"));
    out << regionStamp.width << ' ' << regionStamp.height << ' ' << regionStamp.tiles.size() << ' '
        << regionStamp.objects.size() << '\n';
    for (auto &[x, y, c] : regionStamp.tiles)
      out << x << ' ' << y << ' ' << int(c) << '\n';
    auto joined = [](const std::vector<std::string> &v) {
      std::string all;
      for (auto &x : v)
        all += x + '\x1f';
      return all;
    };
    for (auto &o : regionStamp.objects)
      out << o.sourceFixture << ' ' << o.sourceClutter << '\t' << joined(o.names) << '\t' << joined(o.fields) << '\n';
    sfx::play(sfx::MusicKey, .7f, .02f);
    message(L"Saved as stamp " + std::to_wstring(n) + L". Find it under Stamps in the parts browser.");
  }
  bool loadStamp(int n) {
    std::ifstream in(stampFolder() / (L"stamp-" + std::to_wstring(n) + L".txt"));
    NativeStamp stamp;
    size_t tiles = 0, objects = 0;
    if (!(in >> stamp.width >> stamp.height >> tiles >> objects))
      return false;
    for (size_t i = 0; i < tiles; ++i) {
      int x, y, c;
      in >> x >> y >> c;
      stamp.tiles.push_back({x, y, char(c)});
    }
    std::string line;
    std::getline(in, line);
    auto split = [](const std::string &all) {
      std::vector<std::string> v;
      size_t at = 0;
      for (size_t next; (next = all.find('\x1f', at)) != std::string::npos; at = next + 1)
        v.push_back(all.substr(at, next - at));
      return v;
    };
    for (size_t i = 0; i < objects && std::getline(in, line); ++i) {
      NativeStamp::Object o;
      auto tab1 = line.find('\t'), tab2 = line.find('\t', tab1 + 1);
      std::istringstream head(line.substr(0, tab1));
      head >> o.sourceFixture >> o.sourceClutter;
      o.names = split(line.substr(tab1 + 1, tab2 - tab1 - 1));
      o.fields = split(line.substr(tab2 + 1));
      stamp.objects.push_back(o);
    }
    regionStamp = std::move(stamp);
    rotation = mirror = 0;
    return true;
  }
  // ---- Ideas: two or three next steps, one click each.
  void ideasFor(int after) {
    ideas.clear();
    if (after == 1 || after == CorridorTool)
      ideas = {{L"Light it evenly", 1}, {L"Make it cosy", 6}, {L"Join with a corridor", 4}};
    else if (after == 6)
      ideas = {{L"Light the room evenly", 1}, {L"Make it eerie", 7}, {L"Cold lab light", 9}};
    else if (after == 16)
      ideas = {{L"Save as a stamp", 10}, {L"Paste it", 14}};
    else if (after == ShapeDrawTool)
      ideas = {{L"Make walls", 11}, {L"Make a block", 12}, {L"Make terraces", 13}};
    else if (after == 13 || after == CreatureTool)
      ideas = {{L"Alarm red", 8}, {L"Furnish as a lab", 2}};
    else if (after == 8 || after == 20)
      ideas = {{L"Furnish as a lab", 2}, {L"Furnish as an office", 3}};
    for (size_t i = 0; i < ideaButtons.size(); ++i)
      if (i < ideas.size())
        SetWindowTextW(ideaButtons[i], (L"\u2192 " + ideas[i].label).c_str());
    relayout();
  }
  void runIdea(int action) {
    auto cells = lastRoom();
    switch (action) {
    case 1:
      if (cells.empty())
        return message(L"Click inside a room first.");
      remember();
      message(L"Lit evenly: " + std::to_wstring(lightRoom(cells)) + L" lamps.");
      publish();
      break;
    case 2:
    case 3:
      if (cells.empty())
        return message(L"Click inside a room first.");
      remember();
      furnishCells(cells, action == 2 ? 3 : 0);
      publish();
      message(action == 2 ? L"Furnished as a lab." : L"Furnished as an office.");
      break;
    case 4:
      SendMessageW(window, WM_COMMAND, ToolBase + CorridorTool, 0);
      break;
    case 6: case 7: case 8: case 9:
      moodRoom(action - 6);
      break;
    case 10:
      saveStamp();
      break;
    case 14:
      SendMessageW(window, WM_COMMAND, ToolBase + 17, 0);
      break;
    case 11: case 12: case 13:
      makeFromOutline(action - 11);
      break;
    }
    celebrate();
  }
  void furnishCells(const std::vector<int>& cells,int theme) {dressRoomCells(cells,theme);}
  int forcedTheme = -1;
  // ---- Celebrations: a soft chime when a room is lit and has a way in.
  int countCompleteRooms() const {
    auto b = barriers();
    std::vector<char> seen(576, 0);
    int done = 0;
    for (int c = 0; c < 576; ++c) {
      if (seen[size_t(c)] || b[size_t(c)])
        continue;
      auto cells = roomAt(c % 24, c / 24, b);
      if (cells.empty()) {
        seen[size_t(c)] = 1;
        continue;
      }
      bool door = false;
      for (int r : cells) {
        seen[size_t(r)] = 1;
        int x = r % 24, y = r / 24;
        for (auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
          door |= x + dx >= 0 && x + dx < 24 && y + dy >= 0 && y + dy < 24 && b[size_t((y + dy) * 24 + x + dx)] == 2;
      }
      done += door && !lightsIn(cells).empty();
    }
    return done;
  }
  void celebrate() {
    int now = countCompleteRooms();
    if (now > completeRooms && !quiet()) {
      sfx::play(sfx::MusicKey, .9f, .03f);
      auto cells = lastRoom();
      if (!cells.empty()) {
        float cx = 0, cy = 0;
        for (int c : cells)
          cx += c % 24 + .5f, cy += c / 24 + .5f;
        pop(cx / cells.size(), cy / cells.size(), ui::Selected);
      }
      message(L"\u2713 A finished room: lit, with a way in. " + std::to_wstring(now) + (now == 1 ? L" so far." : L" so far."));
    }
    completeRooms = now;
  }
  // ---- Time of day: the map's light and haze, from night to dusk.
  void setTimeOfDay(int stop) {
    static constexpr const char *ambient[] = {"0.12", "0.3", "0.55", "0.26"};
    static constexpr const char *haze[][4] = {{"0.06", "0.08", "0.14", "0.03"}, {"0.92", "0.62", "0.48", "0.03"},
                                              {"0.75", "0.8", "0.86", "0.012"}, {"0.82", "0.42", "0.28", "0.04"}};
    stop = std::clamp(stop, 0, 3);
    for (auto i : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      static const char *const tail[] = {"-1", "0", "0", "1.570796", "0", "0"};
      for (size_t k = f.size(); k < 23; ++k)
        f.push_back(k >= 17 ? tail[k - 17] : "0");
      f.resize(std::max<size_t>(f.size(), 27));
      f[10] = ambient[stop];
      for (int k = 0; k < 4; ++k)
        f[size_t(23 + k)] = haze[stop][k];
      doc.replace(i, f);
    }
    publish();
    message(std::wstring(timeNames[stop]) + L". The light and haze follow the dial.");
  }
  // ---- History: every step back and forth on one slider.
  void scrubTo(int target) {
    if (scrubbing)
      return;
    target = std::clamp(target, 0, int(history.size() + future.size()));
    if(target==int(history.size()))return;
    scrubbing = true;
    while (int(history.size()) > target) {
      future.push_back(doc.text());
      doc.loadText(history.back());
      history.pop_back();
    }
    while (int(history.size()) < target && !future.empty()) {
      history.push_back(doc.text());
      doc.loadText(future.back());
      future.pop_back();
    }
    selectedRecords.clear();record=-1;
    publish(true,false);
    refresh();
    scrubbing = false;
    message(L"Step " + std::to_wstring(history.size()) + L" of " + std::to_wstring(history.size() + future.size()) +
            L". Carry on building from here, or slide on.");
  }
  // Work brought back from an autosave; Start fresh returns to this text.
  std::string restoredFrom;
  long long restoredAt = -1;
  void startFresh() {
    if (restoredFrom.empty())
      return;
    waitAutosave();
    std::error_code ec;
    fs::remove(autosavePath(), ec);
    doc.loadText(restoredFrom);
    restoredFrom.clear();
    history.clear(), future.clear();
    dirty = false;
    publish(false);
    showTitle();
    refresh();
    ShowWindow(GetDlgItem(window, StartFresh), SW_HIDE);
    relayout();
    message(L"Started fresh.");
  }
  // ---- Parts browser and paint palette (pictures from .build/thumbs).
  parts::ThumbCache thumbs;
  parts::Grid partsGrid, paintGrid;
  std::vector<HWND> partTabButtons;
  int partTab = parts::Furniture, browserTool = -1;
  bool paintShown = false;
  std::set<std::string> savedParts;
  parts::Item paintChoice{"mat-5", L"Concrete", 5, parts::Concrete};
  std::vector<parts::Item> recentPaint;
  PROCESS_INFORMATION shotsProcess{};
  ULONGLONG lastThumbScan = 0;
  float pickZ = -1000.f;
  int pickSurface = 0; // 3D paint: 2 wall, 3 floor, 4 ceiling
  static constexpr const wchar_t *paintSections[] = {L"Concrete & stone", L"Metal", L"Tile", L"Painted", L"Wood",
                                                     L"Special", L"Room finish (the whole map)", L"Recently used"};
  fs::path thumbFolder() const { return root / L".build" / L"thumbs"; }
  static std::string lowered(std::string text) {
    for (auto &c : text)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
  }
  std::vector<parts::Item> allParts() const {
    std::vector<parts::Item> out;
    for (auto &[id, name] : models)
      out.push_back({"model-" + std::to_string(id), parts::nice(name), id, parts::tabOf(name)});
    for (int k = 0; k < 10; ++k)
      out.push_back({"prim-" + std::to_string(k), wide(parts::primNames[k]), k, parts::Shapes});
    for (int n = 1, count = stampCount(); n <= count; ++n)
      out.push_back({"stamp-" + std::to_string(n), L"Stamp " + std::to_wstring(n), n, parts::Stamps});
    return out;
  }
  bool browsing() const { return tool == 8 || tool == 9 || (tool == 14 && listIndex(clutterList) == 7); }
  void refreshParts() {
    if (!partsGrid.hwnd)
      return;
    auto query = lowered(modelSearch ? utf8(label(modelSearch)) : std::string{});
    std::string current = tool == 9 ? "prim-" + std::to_string(listIndex(primList)) : "model-" + std::to_string(selectedModel());
    std::vector<parts::Item> shown;
    int pick = -1;
    for (auto &item : allParts()) {
      bool in = partTab == parts::Saved ? savedParts.count(item.key) > 0 : item.group == partTab;
      if (!query.empty()) // a search looks through every tab
        in = lowered(utf8(item.name)).find(query) != std::string::npos || item.key.find(query) != std::string::npos;
      if (!in)
        continue;
      if (item.key == current)
        pick = int(shown.size());
      shown.push_back(item);
    }
    partsGrid.set(std::move(shown), pick);
    for (HWND b : partTabButtons)
      InvalidateRect(b, nullptr, FALSE);
  }
  void saveSavedParts() {
    std::error_code ec;
    fs::create_directories(root / L".build", ec);
    std::ofstream out(root / L".build" / L"parts-saved.txt");
    for (auto &key : savedParts)
      out << key << '\n';
  }
  void pickPart(int index, bool keep) {
    if (index < 0 || index >= int(partsGrid.items.size()))
      return;
    auto item = partsGrid.items[size_t(index)];
    if (keep) {
      bool had = savedParts.erase(item.key) > 0;
      if (!had)
        savedParts.insert(item.key);
      saveSavedParts();
      if (partTab == parts::Saved)
        refreshParts();
      InvalidateRect(partsGrid.hwnd, nullptr, FALSE);
      message(had ? item.name + L" is no longer saved." : item.name + L" saved. Find it under \u2605 Saved.");
      return;
    }
    if (item.group == parts::Stamps) {
      if (loadStamp(item.value)) {
        SendMessageW(window, WM_COMMAND, ToolBase + 17, 0);
        message(item.name + L": click where it should go. Turn copy and Flip copy work on it too.");
      }
      return;
    }
    if (item.group == parts::Shapes) {
      if (tool != 9)
        SendMessageW(window, WM_COMMAND, ToolBase + 9, 0);
      SendMessageW(primList, CB_SETCURSEL, item.value, 0);
    } else {
      if (tool != 8 && !(tool == 14 && listIndex(clutterList) == 7))
        SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
      auto find = [&] {
        for (LRESULT i = 0, n = SendMessageW(modelList, CB_GETCOUNT, 0, 0); i < n; ++i)
          if (SendMessageW(modelList, CB_GETITEMDATA, i, 0) == item.value)
            return i;
        return LRESULT(-1);
      };
      auto at = find();
      if (at < 0 && modelSearch) {
        SetWindowTextW(modelSearch, L"");
        filterModels();
        at = find();
      }
      if (at >= 0) {
        SendMessageW(modelList, CB_SETCURSEL, at, 0);
        useModelSelection();
      }
    }
    refreshParts();
    InvalidateRect(plan, nullptr, FALSE);
    message(item.name + L": click the plan or the picture to place it. R turns it.");
  }
  void refreshPaint() {
    if (!paintGrid.hwnd)
      return;
    std::vector<parts::Item> items;
    for (auto r : recentPaint)
      r.group = 7, items.push_back(r);
    for (int g = 0; g <= 5; ++g)
      for (auto &[id, name] : structureMaterials)
        if (parts::paintable(name) && parts::paintGroupOf(name) == g)
          items.push_back({"mat-" + std::to_string(id), parts::nice(name), id, g});
    for (int i = 0; i < 13; ++i)
      items.push_back({"surf-" + std::to_string(i), wide(parts::finishNames[i]), i, parts::Finish});
    int pick = -1;
    for (size_t i = 0; i < items.size() && pick < 0; ++i)
      if (items[i].key == paintChoice.key)
        pick = int(i);
    paintGrid.set(std::move(items), pick);
  }
  void choosePaint(parts::Item item) {
    item.group = item.key.rfind("surf-", 0) == 0 ? parts::Finish : parts::Concrete;
    paintChoice = item;
    recentPaint.erase(std::remove_if(recentPaint.begin(), recentPaint.end(),
                                     [&](const parts::Item &r) { return r.key == item.key; }),
                      recentPaint.end());
    recentPaint.insert(recentPaint.begin(), item);
    if (recentPaint.size() > 6)
      recentPaint.resize(6);
    refreshPaint();
  }
  void pickPaint(int index) {
    if (index < 0 || index >= int(paintGrid.items.size()))
      return;
    choosePaint(paintGrid.items[size_t(index)]);
    if (tool != PaintTool)
      SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
    message(paintChoice.name + L": click or drag over what you want painted.");
  }
  bool paintIsFinish() const { return paintChoice.key.rfind("surf-", 0) == 0; }
  int paintMaterial() const { return paintIsFinish() ? parts::finishMaterial[std::clamp(paintChoice.value, 0, 12)] : paintChoice.value; }
  int paintFinish() const { return paintIsFinish() ? paintChoice.value : parts::finishForMaterial(paintChoice.value); }
  std::wstring materialName(int id) const {
    for (auto &[m, name] : structureMaterials)
      if (m == id)
        return parts::nice(name);
    return L"Material " + std::to_wstring(id);
  }
  // The shape or block at a spot (z < -999: anywhere up the column, from the plan).
  bool thingAt(float x, float y, float z, size_t &index, size_t &field) const {
    auto num = [](const std::vector<std::string> &f, size_t i, float d) { return i < f.size() ? number(f[i], d) : d; };
    for (auto i : doc.records("PRIM", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      float cx = num(f, 3, 0), cy = num(f, 4, 0), z0 = num(f, 5, 0), w = num(f, 6, 1), d = num(f, 7, 1), h = num(f, 8, 1);
      float r = std::max(w, d) * .5f + .05f;
      if (std::abs(x - cx) <= r && std::abs(y - cy) <= r && (z < -999 || (z >= z0 - .1f && z <= z0 + h + .1f)))
        return index = i, field = 10, true;
    }
    for (auto i : doc.records("STRUCT", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      float x1 = num(f, 2, 0), y1 = num(f, 3, 0), x2 = num(f, 4, 0), y2 = num(f, 5, 0), b = num(f, 6, 0), t = num(f, 7, 1);
      if (x >= std::min(x1, x2) - .05f && x <= std::max(x1, x2) + .05f && y >= std::min(y1, y2) - .05f &&
          y <= std::max(y1, y2) + .05f && (z < -999 || (z >= std::min(b, t) - .1f && z <= std::max(b, t) + .1f)))
        return index = i, field = 9, true;
    }
    return false;
  }
  // Paints (or with pickUp, picks the paint up from) what is at one spot.
  void paintSpot(float x, float y, float z, int surface, bool pickUp) {
    if(!pickUp&&floorPaintTarget(x,y,z,surface)){
      if(brush==2){auto cells=roomAt(cellOf(x),cellOf(y));if(cells.empty())cells.push_back(cellOf(y)*24+cellOf(x));paintRoomFloor(cells,paintMaterial());}
      else paintNativeFloor(float(cellOf(x)),float(cellOf(y)),float(cellOf(x)+1),float(cellOf(y)+1),paintMaterial());
      message(L"Floor painted with "+materialName(paintMaterial())+L". Other floors and surrounding materials are preserved.");return;}

    size_t index = 0, field = 0;
    if (thingAt(x, y, z, index, field)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      if(!pickUp&&f[0]=="STRUCT"&&f.size()>9&&std::abs(number(f[6],-99)-layerElevation())<.03f&&number(f[7],99)<=layerElevation()+.05f){
        auto cells=roomAt(cellOf(x),cellOf(y));
        if(!cells.empty()){paintRoomFloor(cells,paintMaterial());message(L"Room floor painted; neighboring rooms and floors are preserved.");return;}
      }
      if (pickUp) {
        int id = field < f.size() ? int(number(f[field], 0)) : 0;
        choosePaint({"mat-" + std::to_string(id), materialName(id), id, 0});
        sfx::play(sfx::Click, .7f);
        message(L"Picked up " + paintChoice.name + L". Click to paint with it.");
        return;
      }
      if (f.size() <= field)
        f.resize(field + 1);
      f[field] = std::to_string(paintMaterial());
      doc.replace(index, f);
      message(L"Painted " + plainKind(f[0]) + L": " + materialName(paintMaterial()) + L".");
      return;
    }
    if (!surface) {
      char c = doc.tile(map, layer, std::clamp(int(std::floor(x)), 0, 23), std::clamp(int(std::floor(y)), 0, 23));
      surface = c == '#' ? 2 : 3;
    }
    // A block paint on a wall paints that one wall; on a floor, that one room.
    // Room finishes (and ceilings) still dress the whole map.
    if (!pickUp && !paintIsFinish() && surface != 4) {
      int cx = cellOf(x), cy = cellOf(y);
      if (surface == 2 && paintWallRun(cx, cy, paintMaterial())) {
        message(L"This wall is " + materialName(paintMaterial()) + L" now. The others stay as they were.");
        return;
      }
      if (surface == 3) {
        auto cells = roomAt(cx, cy);
        if (!cells.empty()) {
          paintRoomFloor(cells, paintMaterial());
          message(L"This room's floor is " + materialName(paintMaterial()) + L" now.");
          return;
        }
      }
    }
    static constexpr const wchar_t *which[] = {L"", L"", L"walls", L"floors", L"ceilings"};
    auto found = doc.records("SURFACE", map);
    if (pickUp) {
      int set = 0;
      if (!found.empty()) {
        auto f = NativeDocument::fields(doc.lines[found.front()]);
        set = std::max(0, int(number(size_t(surface) < f.size() ? f[size_t(surface)] : "", 0)));
      }
      choosePaint({"surf-" + std::to_string(set), wide(parts::finishNames[std::clamp(set, 0, 12)]), set, parts::Finish});
      sfx::play(sfx::Click, .7f);
      message(L"Picked up " + paintChoice.name + L". Click to paint with it.");
      return;
    }
    if (found.empty()) {
      addAuthored({"SURFACE", std::to_string(map), "-1", "-1", "-1"});
      found = doc.records("SURFACE", map);
    }
    auto f = NativeDocument::fields(doc.lines[found.front()]);
    if (f.size() < 5)
      f.resize(5, "-1");
    f[size_t(surface)] = std::to_string(paintFinish());
    doc.replace(found.front(), f);
    message(L"All " + std::wstring(which[surface]) + L" on this map are now " + wide(parts::finishNames[paintFinish()]) + L".");
  }
  void paintGesture(float ax, float ay, float bx, float by) {
    bool pickUp = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    float z = pickZ;
    int surface = pickSurface;
    pickZ = -1000.f, pickSurface = 0;
    if (!pickUp)
      remember();
    if (std::abs(ax - bx) < .3f && std::abs(ay - by) < .3f) {
      paintSpot(bx, by, z, surface, pickUp);
    } else {
      if(floorPaintTarget(ax,ay,z,surface)){
        float x0=std::min(ax,bx),x1=std::max(ax,bx),y0=std::min(ay,by),y1=std::max(ay,by);if(x1-x0<.05f){x0-=.125f;x1+=.125f;}if(y1-y0<.05f){y0-=.125f;y1+=.125f;}
        paintNativeFloor(x0,y0,x1,y1,paintMaterial());publish();InvalidateRect(plan,nullptr,FALSE);return;}
      // A drag paints every block and shape it covers, else the floor or walls where it began.
      float x0 = std::min(ax, bx), x1 = std::max(ax, bx), y0 = std::min(ay, by), y1 = std::max(ay, by);
      int painted = 0;
      for (const char *k : {"PRIM", "STRUCT"})
        for (auto i : doc.records(k, map)) {
          auto f = NativeDocument::fields(doc.lines[i]);
          float cx = k[0] == 'P' ? number(f.size() > 3 ? f[3] : "", -1) : (number(f.size() > 2 ? f[2] : "", -1) + number(f.size() > 4 ? f[4] : "", -1)) * .5f;
          float cy = k[0] == 'P' ? number(f.size() > 4 ? f[4] : "", -1) : (number(f.size() > 3 ? f[3] : "", -1) + number(f.size() > 5 ? f[5] : "", -1)) * .5f;
          if (cx < x0 || cx > x1 || cy < y0 || cy > y1)
            continue;
          size_t field = k[0] == 'P' ? 10 : 9;
          if (f.size() <= field)
            f.resize(field + 1);
          f[field] = std::to_string(paintMaterial());
          doc.replace(i, f);
          ++painted;
        }
      if (painted)
        message(L"Painted " + std::to_wstring(painted) + L" pieces " + materialName(paintMaterial()) + L".");
      else
        paintSpot(ax, ay, -1000.f, 0, false);
    }
    if (!pickUp)
      publish();
    InvalidateRect(plan, nullptr, FALSE);
  }
  // A click in the picture: walk the ray to the first block, shape, wall, floor or ceiling.
  void paintRay(float ox, float oy, float oz, float dx, float dy, float dz) {
    auto [mx, my] = mapOrigin();
    float floorZ = layerElevation(), ceiling = 4.5f;
    for (auto index : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      ceiling = number(f.size() > 9 ? f[9] : "", 4.5f);
    }
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f)
      return;
    dx /= len, dy /= len, dz /= len;
    for (float t = .05f; t < 60.f; t += .03f) {
      float x = ox + dx * t, y = oy + dy * t, z = oz + dz * t;
      if (x < 0 || y < 0 || x >= 24 || y >= 24)
        continue;
      size_t index = 0, field = 0;
      int surface = thingAt(x, y, z, index, field) ? 0
                    : z <= floorZ + .01f            ? 3
                    : z >= ceiling - .01f  ? 4
                    : doc.tile(map, layer, int(x), int(y)) == '#' ? 2
                                                                  : -1;
      if (surface < 0)
        continue;
      pickZ = surface == 0 ? z : -1000.f, pickSurface = surface;
      gesture(x, y, x, y);
      return;
    }
  }
  // The engine draws every missing picture once, in the background.
  void makeThumbnails(bool wait) {
    if (shotsProcess.hProcess)
      return;
    edperf::Scope perfScope("thumbnails_list");
    auto folder = thumbFolder();
    std::error_code ec;
    fs::create_directories(folder / L"packs", ec);
    std::ofstream list(folder / L"shots.txt");
    int count = 0;
    NativeDocument base;
    base.newCampaign("Picture studio");
    base.addRoom("Studio");
    for (auto index : base.records("MAP", 0)) {
      auto f = NativeDocument::fields(base.lines[index]);
      f[9] = "9", f[10] = "0.75"; // tall and softly lit all round
      base.replace(index, f);
    }
    auto needed = [&](const std::string &key) {
      return !thumbs.exists(key) && !fs::exists(folder / (wide(key) + L".failed"), ec);
    };
    auto shot = [&](const std::string &key, const NativeDocument &d, float x, float y, float eye, float yaw, float targetZ, float dist) {
      if (!needed(key))
        return;
      auto pack = folder / L"packs" / (wide(key) + L".rmc");
      d.save(pack);
      float pitch = std::atan2(targetZ - eye, std::max(.3f, dist)) * 140.f;
      list << utf8(pack.wstring()) << '|' << utf8((folder / (wide(key) + L".ppm")).wstring()) << '|' << x << '|' << y
           << '|' << eye << '|' << yaw << '|' << pitch << '\n';
      ++count;
    };
    for (auto &[id, name] : models) {
      if (id < 0 || id >= int(std::size(retro::model::size)))
        continue;
      float w = retro::model::size[id][0], d = retro::model::size[id][1], h = retro::model::size[id][2];
      // The oldest built-in pieces carry no size table and the studio cannot
      // frame them yet; their tiles keep a soft placeholder.
      if (w <= 0 || d <= 0 || h <= 0)
        continue;
      if (!needed("model-" + std::to_string(id)))
        continue;
      NativeDocument doc2 = base;
      // Paper-thin pieces (a web, a sheet, a card) keep a centimetre: "0" is not a size the engine takes.
      doc2.add({"FIXTURE", "0", std::to_string(id), "12", "12", "0", trimmed(std::max(w, .01f)), trimmed(std::max(d, .01f)),
                trimmed(std::max(h, .01f)), "0", "0"});
      // Small things close up, big things from further back.
      float s = std::max({w, d, h}), dist = std::clamp(s * 1.45f + .3f, .55f, 10.5f);
      doc2.add({"LIGHT", "0", "14", "10", trimmed(std::max(3.f, h + 1.5f)), "0", "0", "1.8", "18", "4400"});
      doc2.add({"LIGHT", "0", trimmed(12 + dist * .6f), trimmed(12 + dist * .6f), trimmed(std::max(2.5f, h + .5f)), "0", "0", "1.2", "14", "5200"});
      float eye = std::clamp(h * .55f + dist * .3f, .25f, 8.f);
      shot("model-" + std::to_string(id), doc2, 12 + dist * .7071f, 12 + dist * .7071f, eye, 225, h * .45f, dist);
    }
    for (int k = 0; k < 10; ++k) {
      if (!needed("prim-" + std::to_string(k)))
        continue;
      NativeDocument doc2 = base;
      // An arch must clear a standing player: the engine refuses a 2 m one, so it is shown at its usual 2.4 x 3.4 m.
      bool arch = std::string(primKinds[k]) == "arch";
      doc2.add({"PRIM", "0", primKinds[k], "12", "12", "0", arch ? "2.4" : "2", arch ? "0.6" : "2", arch ? "3.4" : "2", "0", "5"});
      doc2.add({"LIGHT", "0", "14", "10", "4", "0", "0", "1.8", "14", "4400"});
      doc2.add({"LIGHT", "0", "14.4", "14.4", "3", "0", "0", "1.2", "12", "5200"});
      float away = arch ? 6.6f : 3.6f;
      shot("prim-" + std::to_string(k), doc2, 12 + away * .7071f, 12 + away * .7071f, arch ? 2.6f : 2.3f, 225, arch ? 1.7f : 1.f, away);
    }
    for (auto &[id, name] : structureMaterials) {
      if (!parts::paintable(name))
        continue;
      if (!needed("mat-" + std::to_string(id)))
        continue;
      NativeDocument doc2 = base;
      doc2.add({"STRUCT", "0", "10", "10", "14", "14", "0", "3", "0", std::to_string(id)});
      doc2.add({"LIGHT", "0", "12", "16", "2.6", "0", "0", "1.4", "8", "4500"});
      shot("mat-" + std::to_string(id), doc2, 12, 15.3f, 1.5f, 270, 1.5f, 1.3f);
    }
    for (int i = 0; i < 13; ++i) {
      if (!needed("surf-" + std::to_string(i)))
        continue;
      NativeDocument doc2 = base;
      doc2.add({"SURFACE", "0", std::to_string(i), std::to_string(i), std::to_string(i)});
      doc2.add({"LIGHT", "0", "12", "20.5", "3", "0", "0", "1.4", "8", "4500"});
      shot("surf-" + std::to_string(i), doc2, 12, 21.7f, 1.5f, 90, 1.5f, 1.3f);
    }
    list.close();
    if (!count)
      return;
    fs::path exe = engineExecutable();
    if (!fs::exists(exe, ec)) {
      wchar_t self[32768]{};
      GetModuleFileNameW(nullptr, self, 32768);
      exe = self;
    }
    std::wstring args = L"\"" + exe.wstring() + L"\" --editor-shots=\"" + (folder / L"shots.txt").wstring() + L"\"";
    STARTUPINFOW si{sizeof(si)};
    if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
                        nullptr, root.c_str(), &si, &shotsProcess)) {
      shotsProcess = {};
      return;
    }
    CloseHandle(shotsProcess.hThread);
    shotsProcess.hThread = nullptr;
    if (wait) {
      WaitForSingleObject(shotsProcess.hProcess, INFINITE);
      CloseHandle(shotsProcess.hProcess);
      shotsProcess = {};
      markFailedShots();
    }
  }
  // A picture the engine could not draw is not asked for again on every start.
  void markFailedShots() {
    std::error_code ec;
    if (!fs::exists(thumbFolder() / L"shots.txt.done", ec))
      return;
    std::ifstream list(thumbFolder() / L"shots.txt");
    for (std::string line; std::getline(list, line);) {
      auto bar = line.find('|'), next = line.find('|', bar + 1);
      if (bar == std::string::npos || next == std::string::npos)
        continue;
      fs::path out = fs::u8path(line.substr(bar + 1, next - bar - 1));
      if (!fs::exists(out, ec))
        std::ofstream(fs::path(out).replace_extension(L".failed")) << "engine could not draw this\n";
    }
  }
  void pollThumbnails() {
    if (!shotsProcess.hProcess || GetTickCount64() - lastThumbScan < 1500)
      return;
    lastThumbScan = GetTickCount64();
    edperf::Scope perfScope("thumbnails_poll");
    bool done = WaitForSingleObject(shotsProcess.hProcess, 0) == WAIT_OBJECT_0;
    thumbs.rescan();
    for (auto *g : {&partsGrid, &paintGrid})
      if (g->hwnd && IsWindowVisible(g->hwnd))
        InvalidateRect(g->hwnd, nullptr, FALSE);
    if (done)
      CloseHandle(shotsProcess.hProcess), shotsProcess = {}, markFailedShots();
  }
  // The little pop where something lands, fading over a third of a second.
  float popX = 0, popY = 0;
  COLORREF popColor = 0;
  ULONGLONG popAt = 0;
  void pop(float x, float y, COLORREF color) {
    popX = x, popY = y, popColor = color, popAt = GetTickCount64();
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
  }
  bool quiet() const { return backgroundTest || demoMode || ghosting || remakeTest || editingTest; }
  // Live lamp controls: brightness, reach, warmth, colour gel and flicker.
  HWND brightSlider = nullptr, reachSlider = nullptr, warmSlider = nullptr;
  std::vector<HWND> swatches;
  bool lightDrag = false, lightPending = false;
  // While a drag, stroke or slider is held the picture follows it: a change
  // goes to the engine as soon as the one before is in the picture (it says so
  // at once, WM_APP + 41), so one update is in flight and none queue up. One
  // that has to wait is sent when the picture catches up.
  bool livePending = false;
  bool statusTimer = false; // the 50 ms engine status tick (timer 1) is running
  bool liveTurn(int before = 200) {
    if (renderedRevision == revision && GetTickCount64() - lastDragPublish >= ULONGLONG(edperf::baseline ? before : 16))
      return true;
    livePending = true;
    return false;
  }
  struct Gel {
    const wchar_t *name;
    float r, g, b;
    COLORREF shown;
  };
  static constexpr Gel gels[] = {{L"White", 1, 1, 1, RGB(0xFF, 0xFB, 0xF0)},
                                 {L"Amber", 1, .62f, .3f, RGB(0xF6, 0xB0, 0x5C)},
                                 {L"Red", 1, .16f, .1f, RGB(0xE0, 0x4A, 0x3A)},
                                 {L"Green", .35f, 1, .45f, RGB(0x6C, 0xC8, 0x6E)},
                                 {L"Blue", .35f, .55f, 1, RGB(0x5C, 0x86, 0xE0)},
                                 {L"Violet", .75f, .4f, 1, RGB(0xA8, 0x72, 0xE6)}};
  bool lightPicked() const {
    return record >= 0 && record < int(doc.lines.size()) && NativeDocument::fields(doc.lines[size_t(record)])[0] == "LIGHT";
  }
  std::string lightField(size_t index) const {
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    return index < f.size() ? f[index] : std::string{};
  }
  static float number(const std::string &text, float fallback) {
    try {
      return text.empty() ? fallback : std::stof(text);
    } catch (...) {
      return fallback;
    }
  }
  static std::string trimmed(float v) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << v;
    auto text = out.str();
    while (text.find('.') != std::string::npos && (text.back() == '0' || text.back() == '.'))
      text.pop_back();
    return text.empty() ? "0" : text;
  }
  // Writes lamp fields and relights the picture at once (throttled while dragging).
  void setLight(std::initializer_list<std::pair<size_t, std::string>> changes, bool final) {
    if (!lightPicked())
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    for (auto &[index, value] : changes) {
      if (f.size() <= index)
        f.resize(index + 1);
      f[index] = value;
    }
    doc.replace(size_t(record), f);
    if (final || liveTurn(90)) {
      publish();
      lastDragPublish = GetTickCount64();
      lightPending = false;
    } else {
      lightPending = true;
      ++changeCount;
      dirty = true;
    }
    if (final)
      fillFields(false), fieldLayout();
  }
  void syncLightControls() {
    if (!brightSlider || !lightPicked())
      return;
    SendMessageW(brightSlider, TBM_SETPOS, TRUE, LPARAM(std::lround(number(lightField(7), 1) * 100)));
    SendMessageW(reachSlider, TBM_SETPOS, TRUE, LPARAM(std::lround(number(lightField(8), 8))));
    SendMessageW(warmSlider, TBM_SETPOS, TRUE, LPARAM(std::lround(number(lightField(9), 3400))));
  }
  // The plan's camera: fitted by default, wheel zooms, middle or Space drag pans, Home fits.
  float planZoom = 1.f, planPanX = 0.f, planPanY = 0.f;
  bool panning = false;
  POINT panFrom{};
  struct PlanView {
    float cell;
    int ox, oy;
  };
  PlanView planView() const {
    RECT r{};
    if (plan)
      GetClientRect(plan, &r);
    // Between the hint line on top and the help line underneath.
    float fit = std::max(1.f, std::min((r.right - 20) / 24.f, (r.bottom - 64) / 24.f));
    float cell = fit * planZoom;
    return {cell, int(std::lround((r.right - cell * 24) / 2 + planPanX)),
            int(std::lround(32 + ((r.bottom - 64) - cell * 24) / 2 + planPanY))};
  }
  // Zoom about the pointer: the spot under it stays under it.
  void zoomPlan(float factor, int mx, int my) {
    auto before = planView();
    float wx = (mx - before.ox) / before.cell, wy = (my - before.oy) / before.cell;
    planZoom = std::clamp(planZoom * factor, .5f, 8.f);
    auto after = planView();
    planPanX += mx - (after.ox + wx * after.cell);
    planPanY += my - (after.oy + wy * after.cell);
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
  }
  void fitPlan() {
    planZoom = 1.f, planPanX = planPanY = 0.f;
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
  }
  int group = 0, brush = 0, playView = -1, hoverRecord = -1;
  bool engineLive = false, advanced = false, autoDetail = true, hoverInside = false, brushing = false,
       remakeTest = false, demoMode = false;
  float hoverX = 0, hoverY = 0, dragFX = 0, dragFY = 0;
  long long changeCount = 0, autosavedAt = 0;
  int playSpot = 0;
  bool ghosting = false, ghostShown = false;
  ULONGLONG lastGhost = 0, playStarted = 0;
  float playX = 0, playY = 0, playZ = 0;
  std::vector<size_t> objectCache;
  std::tuple<int, int, size_t, long long, size_t, size_t, const void *> objectKey{-1, -1, 0, 0, 0, 0, nullptr};
  // The plan's map layer (drawPlan), kept between paints.
  struct PlanLayerKey {
    std::uint64_t version = 0;
    size_t lines = 0;
    int map = -1, layer = -1;
    LONG width = 0, height = 0;
    float cell = 0;
    int ox = 0, oy = 0;
    std::uint64_t outline = 0;
    bool operator==(const PlanLayerKey &) const = default;
  };
  HBITMAP planLayer = nullptr;
  PlanLayerKey planLayerKey;
  ULONGLONG noticeUntil=0;
  void message(std::wstring s,bool engineUpdate=false) {
    if(ghosting)return;
    if(engineUpdate&&GetTickCount64()<noticeUntil)return;
    if(!engineUpdate)noticeUntil=GetTickCount64()+5000;
    notice = std::move(s);
    if (statusText)
      SetWindowTextW(statusText, notice.c_str());
  }
  void remember() {
    if (ghosting)
      return;
    edperf::Scope perfScope("remember");
    history.push_back(doc.text());
    if (history.size() > 50)
      history.erase(history.begin());
    future.clear();
  }
  // The engine picture's control and status "files": memory when the picture
  // runs in this process (RawMetal.exe), small files for a separate engine.
  bool inProcess = false;
  std::string channelText(const fs::path &file) const { return retro::editorchannel::text(file, inProcess); }
  void channelPut(const fs::path &file, const std::string &text) {
    if (inProcess) {
      retro::editorchannel::write(file, text);
      return;
    }
    auto temp = file;
    temp += L".tmp";
    {
      std::ofstream out(temp);
      out << text;
    }
    if (!replaceEditorFile(temp, file))
      throw std::runtime_error("Cannot update engine controls");
  }
  void writeControl(bool stop = false) {
    std::ostringstream out;
    // spot: 0 play from the camera, 1 from the map's start, 2 at playX/playY.
    out << revision << ' ' << map << ' ' << int(playing) << ' ' << int(stop)
        << ' ' << int(auditionMusic) << ' ' << playSpot << ' ' << playX
        << ' ' << playY << ' ' << playZ << '\n';
    out<<editorViewControl();
    channelPut(control, out.str());
  }
  void showTitle() {
    if (historySlider && !scrubbing) {
      SendMessageW(historySlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, int(history.size() + future.size())));
      SendMessageW(historySlider, TBM_SETPOS, TRUE, LPARAM(history.size()));
    }
    if (!window)
      return;
    std::wstring name = path.empty() ? L"New map" : path.stem().wstring();
    SetWindowTextW(window, (name + (dirty ? L" • not saved yet" : L"") + L"  -  Raw Metal Campaign Builder" + adminBadge()).c_str());
    for (int id : {Undo, Redo})
      if (HWND b = GetDlgItem(window, id))
        InvalidateRect(b, nullptr, FALSE);
  }
  // edit=false only refreshes the engine: opening or checking is not a change.
  std::map<long long, double> publishedAt; // --perf-trace: revision -> when it left for the engine
  void writePack() {
    edperf::Scope perfScope("publish_write");
    if (inProcess) {
      // The same process: the engine takes the text from memory, no disk.
      retro::editorchannel::write(pack, doc.text());
    } else {
      auto temp = pack;
      temp += L".tmp";
      doc.save(temp);
      if (!replaceEditorFile(temp, pack))
        throw std::runtime_error("Could not update native engine document");
    }
    ++revision;
    writeControl();
    if (edperf::on)
      publishedAt[revision] = edperf::now();
  }
  void publish(bool edit = true, bool adjustDressing = true) {
    livePending = false;
    // A 3D preview publishes its scratch copy and nothing else.
    if (ghosting) {
      writePack();
      return;
    }
    // Repair managed rooms at commit, never on a drag preview, Undo or load.
    // Keep the current native selection when earlier generated records move.
    if (edit && adjustDressing && !objectDrag && !brushing && !scrubbing) {
      auto selectedLine = record >= 0 && size_t(record) < doc.lines.size() ? doc.lines[size_t(record)] : std::string{};
      std::vector<std::string> selectedLines;for(int i:selectedRecords)if(i>=0&&size_t(i)<doc.lines.size())selectedLines.push_back(doc.lines[size_t(i)]);
      auto beforeDressing = doc.version;
      reconcileDressing();
      if(doc.version!=beforeDressing){selectedRecords.clear();for(const auto& line:selectedLines){auto found=std::find(doc.lines.begin(),doc.lines.end(),line);if(found!=doc.lines.end())selectedRecords.push_back(int(found-doc.lines.begin()));}}
      if (doc.version != beforeDressing && !selectedLine.empty()) {
        auto found = std::find(doc.lines.begin(), doc.lines.end(), selectedLine);
        record = found == doc.lines.end() ? -1 : int(found - doc.lines.begin());
      }
    }
    if (edit)
      dirty = true, ++changeCount;
    showTitle();
    // With no engine attached (tests, scripts) an edit only waits for the next
    // check, which publishes; writing every keystroke to disk is just slow.
    if (session.empty() || (edit && !engineLive)) {
      if (plan)
        InvalidateRect(plan, nullptr, FALSE);
      return;
    }
    writePack();
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
  }
  void loadSchemas() {
    fs::path guide = root / L"custom maps" / L"GUIDE.html";
    std::ifstream in(guide);
    std::string text(std::istreambuf_iterator<char>(in), {});
    std::regex row("<td><code>(.*?)</code></td><td>(.*?)</td>");
    for (std::sregex_iterator it(text.begin(), text.end(), row), end; it != end;
         ++it) {
      auto spec = html((*it)[1]);
      std::string description = html((*it)[2]);
      size_t from = 0;
      for (;;) {
        size_t next = spec.find("  +  ", from);
        auto part = spec.substr(from, next - from);
        int required =
            int(std::count(
                part.begin(),
                part.begin() +
                    std::ptrdiff_t(part.find('[') == std::string::npos
                                       ? part.size()
                                       : part.find('[')),
                '|')) +
            1;
        part.erase(std::remove(part.begin(), part.end(), '['), part.end());
        part.erase(std::remove(part.begin(), part.end(), ']'), part.end());
        auto f = NativeDocument::fields(part);
        // A worked example in prose ("TERMINAL|...|1|LEVER") is not a schema;
        // taking it as one made computers unselectable on the plan.
        if (f.size() > 2 &&
            std::find(f.begin(), f.end(), "...") == f.end() &&
            std::all_of(f[0].begin(), f[0].end(),
                        [](unsigned char c) { return isupper(c) || c == '_'; }))
          schemas.push_back(
              {f[0], description, f, required, f[1] == "map" || f[0] == "MAP"});
        if (next == std::string::npos)
          break;
        from = next + 5;
      }
    }
    schemas.erase(std::remove_if(schemas.begin(), schemas.end(),
                                 [](auto &s) { return s.kind == "LEAD"; }),
                  schemas.end());
    schemas.push_back(
        {"LEAD",
         "Cable endpoints, length, insulation, live state and attachments.",
         NativeDocument::fields(
             "LEAD|map|ax|ay|az|bx|by|bz|length|radius|material|live|state|"
             "prop|ox|oy|oz|loose|panel|socket|terminal"),
         10, true});
    // Film cameras are the builder's own lines (NativeFilmTool.inc); the game skips them.
    schemas.push_back({FilmKind, "A film camera: where it stands and looks, and where it moves to.",
                       NativeDocument::fields(std::string(FilmKind) + "|map|name|ax|ay|az|turn|tilt|seconds|bx|by|bz|bturn|btilt|claire"),
                       15, true});
    if (std::none_of(schemas.begin(), schemas.end(), [](auto &s) { return s.kind == "CAMPAIGN"; }))
      schemas.push_back({"CAMPAIGN", "Campaign name and the map you start on.",
                         {"CAMPAIGN", "name", "startMap"}, 3, false});
    std::stable_sort(schemas.begin(), schemas.end(),
                     [](auto &a, auto &b) { return a.kind < b.kind; });
    {
      std::ifstream bankFile(root / L"src" / L"audio" / L"SoundBank.h");
      std::string bankText(std::istreambuf_iterator<char>(bankFile), {});
      std::regex bankRow("\\{Sound::\\w+,\"([a-z0-9_]+)\",(true|false),([0-9.]+)f?,([0-9.]+)f?,\"([^\"]+)\"\\}");
      for (std::sregex_iterator it(bankText.begin(), bankText.end(), bankRow), end; it != end; ++it)
        bankSounds.push_back({(*it)[1], wide((*it)[5]), (*it)[2] == "true", (*it)[3], (*it)[4]});
    }
    std::ifstream ids(root / L"src" / L"world" / L"ModelIds.h");
    std::string catalog(std::istreambuf_iterator<char>(ids), {});
    std::regex model("inline constexpr int ([A-Za-z0-9_]+)=([0-9]+);");
    for (std::sregex_iterator it(catalog.begin(), catalog.end(), model), end;
         it != end; ++it)
      models.emplace_back(std::stoi((*it)[2]), (*it)[1]);
    std::ifstream definitions(root / L"src" / L"world" / L"WorldDefinition.h");
    std::string native(std::istreambuf_iterator<char>(definitions), {});
    std::smatch cues;
    if (!std::regex_search(native, cues,
                           std::regex("enum class MusicCue \\{([^}]+)\\}")))
      throw std::runtime_error("Native soundtrack catalogue was not found");
    std::istringstream names(cues[1].str());
    std::string name;
    while (std::getline(names, name, ',')) {
      name = std::regex_replace(name, std::regex("^\\s+|\\s+$"), "");
      if (name == "Count")
        break;
      musicNames.push_back(
          name == "Default" ? "Original soundtrack"
                            : std::regex_replace(
                                  name, std::regex("([a-z])([A-Z])"), "$1 $2"));
    }
    auto heading = text.find("STRUCT materials");
    auto tableEnd =
        heading == std::string::npos ? heading : text.find("</table>", heading);
    if (heading != std::string::npos && tableEnd != std::string::npos) {
      auto slice = text.substr(heading, tableEnd - heading);
      std::regex cell("<td>([0-9]+)</td><td>([^<]+)</td>");
      for (std::sregex_iterator it(slice.begin(), slice.end(), cell), end;
           it != end; ++it)
        structureMaterials.emplace_back(std::stoi((*it)[1]), html((*it)[2]));
    }
    if (structureMaterials.empty())
      throw std::runtime_error("GUIDE structure materials were not found");
  }
  std::string initial(const Schema &s, int i) {
    auto n = s.fields[size_t(i)];
    if (s.scoped && i == 1)
      return std::to_string(map);
    if (i >= s.required)
      return "";
    if (!n.empty() && std::all_of(n.begin(), n.end(), [](unsigned char c) {
          return isdigit(c) || isupper(c) || c == '_';
        }))
      return n;
    if (n == "x" || n == "x1" || n == "ax" || n == "left")
      return "10";
    if (n == "y" || n == "y1" || n == "ay")
      return "10";
    if (n == "x2" || n == "bx" || n == "right")
      return "14";
    if (n == "y2" || n == "by")
      return "14";
    if (n == "24 characters")
      return std::string(24, '.');
    if (n == "name" || n == "title")
      return "New exhibit";
    if (n == "skybox")
      return "industrial_night";
    if (n == "kind" && s.kind == "PRIM")
      return "arch";
    if (n == "radius")
      return s.kind == "LEAD" ? "0.008" : "0.09";
    if (n == "length")
      return "6";
    if (n == "endZ" || n == "az" || n == "bz" || n == "top" || n == "raised")
      return "3";
    if (n == "surface")
      return "0.4";
    if (n == "range")
      return "8";
    if (n == "kelvin")
      return "4000";
    if (n == "period")
      return "4";
    if (n == "count")
      return "16";
    if (n == "rate")
      return "2";
    if (n == "growth")
      return "0.2";
    if (n == "rise")
      return "0.5";
    if (n == "steps")
      return "17";
    if (n == "line1")
      return "Welcome";
    if (n == "line2")
      return "Custom campaign";
    if (n == "width" || n == "depth" || n == "height" || n == "solid" ||
        n == "scale" || n == "pitch" && s.kind == "TIMED_SEQUENCE")
      return "1";
    if (n == "w" || n == "d" || n == "h")
      return s.kind == "PRIM" ? "0" : "1";
    if (n == "alongY" || n == "ascending")
      return "1";
    if (n == "colour")
      return "3968216";
    if (n == "damage")
      return "2";
    if (n.find("State") != std::string::npos || n == "id" || n == "event")
      return n == "requireState" ? "" : "editor_" + n;
    return "0";
  }
  void normalize(std::vector<std::string> &f) {
    if (f[0] == "TERMINAL" && f.size() > 10 && f[10].empty())
      f[10] = "0";
    if (f[0] == "FIXTURE" && f.size() > 11 && f[11].empty())
      f[11] = "1";
    if (f[0] == "MAP")
      for (int i = 17; i <= 22 && i < int(f.size()); i++)
        if (f[size_t(i)].empty())
          f[size_t(i)] = i == 17 ? "-1" : "0";
  }
  void fieldLayout() {
    edperf::Scope perfScope("ui_fieldLayout");
    RECT r{};
    GetClientRect(form, &r);
    int first = schemas.empty() ? 2 : schemas[size_t(kind)].scoped ? 2 : 1;
    int row = 0;
    for (size_t i = 0; i < fieldInputs.size(); i++) {
      bool shown = i < fieldShown.size() && fieldShown[i];
      showControl(fieldLabels[i], shown);
      showControl(fieldInputs[i], shown);
      if (!shown)
        continue;
      int y = row++ * 54 - formScroll;
      placeControl(fieldLabels[i], 10, y + 6, std::max(50L, r.right - 36), 20, TRUE);
      placeControl(fieldInputs[i], 10, y + 27, std::max(50L, r.right - 36), 26,
                 TRUE);
    }
    SCROLLINFO s{sizeof(s),
                 SIF_RANGE | SIF_PAGE | SIF_POS,
                 0,
                 row * 54 + 8,
                 UINT(r.bottom),
                 formScroll,
                 0};
    SetScrollInfo(form, SB_VERT, &s, TRUE);
    (void)first;
  }
  HWND controlWindow(const wchar_t *cls, const wchar_t *text, DWORD style,
                     int id, HWND parent = nullptr) {
    // Lists and drop-downs draw their own rows so a pick is sage, not system blue.
    if (_wcsicmp(cls, L"COMBOBOX") == 0)
      style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    else if (_wcsicmp(cls, L"LISTBOX") == 0)
      style |= LBS_OWNERDRAWFIXED | LBS_HASSTRINGS;
    HWND h = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0,
                             10, 10, parent ? parent : window,
                             reinterpret_cast<HMENU>(INT_PTR(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (_wcsicmp(cls, L"BUTTON") == 0 && (style & BS_OWNERDRAW))
      ui::cosy(h, 1);
    else if (_wcsicmp(cls, L"COMBOBOX") == 0)
      ui::cosy(h, 3);
    else if (style & WS_BORDER)
      ui::cosy(h, 2);
    return h;
  }
  HWND button(const wchar_t *text, int id) {
    auto h = controlWindow(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, id);
    buttons.push_back(h);
    return h;
  }
  // Field boxes are pooled: an edit control costs milliseconds to create, and
  // picking things must feel instant.
  std::vector<HWND> poolLabels, poolInputs;
  void fillFields(bool fresh) {
    edperf::Scope perfScope("ui_fillFields");
    edperf::Laps laps;
    // Boxes past this record's last field are hidden below; fieldLayout shows
    // or hides the rest (hiding them all first only made them flicker).
    fieldInputs.clear();
    fieldLabels.clear();
    fieldShown.clear();
    formScroll = 0;
    if (schemas.empty()) {
      for (size_t i = 0; i < poolInputs.size(); ++i)
        ShowWindow(poolLabels[i], SW_HIDE), ShowWindow(poolInputs[i], SW_HIDE);
      return;
    }
    auto &s = schemas[size_t(kind)];
    auto f = !fresh && record >= 0
                 ? NativeDocument::fields(doc.lines[size_t(record)])
                 : std::vector<std::string>{};
    int start = s.scoped ? 2 : 1;
    for (int i = start; i < int(std::max(s.fields.size(), f.size())); i++) {
      std::string name = i < int(s.fields.size())
                             ? s.fields[size_t(i)]
                             : "Engine field " + std::to_string(i);
      auto *nice = friendlyLabel(s.kind, name);
      fieldShown.push_back(advanced || (nice && record >= 0));
      auto value = i < int(f.size()) ? decoded(f[size_t(i)])
                   : fresh           ? initial(s, i)
                                     : "";
      size_t slot = fieldInputs.size();
      if (slot >= poolInputs.size()) {
        poolLabels.push_back(controlWindow(L"STATIC", L"", 0, 0, form));
        poolInputs.push_back(controlWindow(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                                           1000 + int(slot), form));
      }
      setControlText(poolLabels[slot], nice && !advanced ? nice : plainField(name).c_str());
      setControlText(poolInputs[slot], wide(value).c_str());
      fieldLabels.push_back(poolLabels[slot]);
      fieldInputs.push_back(poolInputs[slot]);
    }
    laps.lap("fields_texts");
    for (size_t i = fieldInputs.size(); i < poolInputs.size(); ++i)
      showControl(poolLabels[i], false), showControl(poolInputs[i], false);
    laps.lap("fields_hide");
    fieldLayout();
    laps.lap("fields_layout");
    auto modelField = std::find(s.fields.begin(), s.fields.end(), "model");
    if (modelList && modelField != s.fields.end()) {
      size_t index = size_t(modelField - s.fields.begin());
      if (fresh)
        useModelSelection();
      else if (index < f.size()) {
        int slot = std::stoi(f[index]);
        LRESULT found = -1;
        for (LRESULT i = 0; i < SendMessageW(modelList, CB_GETCOUNT, 0, 0); ++i)
          if (SendMessageW(modelList, CB_GETITEMDATA, i, 0) == slot) {
            found = i;
            break;
          }
        if (found < 0)
          for (const auto &[id, name] : models)
            if (id == slot) {
              found = SendMessageW(
                  modelList, CB_ADDSTRING, 0,
                  reinterpret_cast<LPARAM>(
                      wide(std::to_string(id) + ": " + name).c_str()));
              SendMessageW(modelList, CB_SETITEMDATA, found, id);
              break;
            }
        SendMessageW(modelList, CB_SETCURSEL, found, 0);
      }
    }
    auto materialField =
        std::find(s.fields.begin(), s.fields.end(), "material");
    if (materialList && materialField != s.fields.end()) {
      size_t index = size_t(materialField - s.fields.begin());
      int id = structureMaterial;
      if (!fresh && index < f.size()) {
        try {
          id = std::stoi(f[index]);
        } catch (...) {
        }
      }
      for (LRESULT i = 0; i < SendMessageW(materialList, CB_GETCOUNT, 0, 0);
           ++i)
        if (SendMessageW(materialList, CB_GETITEMDATA, i, 0) == id) {
          SendMessageW(materialList, CB_SETCURSEL, i, 0);
          structureMaterial = id;
          break;
        }
    }
  }
  void useModelSelection() {
    auto selected = SendMessageW(modelList, CB_GETCURSEL, 0, 0);
    if (selected < 0)
      return;
    int slot = int(SendMessageW(modelList, CB_GETITEMDATA, selected, 0));
    if (slot < 0)
      return;
    if (std::find(schemas[size_t(kind)].fields.begin(),
                  schemas[size_t(kind)].fields.end(),
                  "model") == schemas[size_t(kind)].fields.end()) {
      if (schemas[size_t(kind)].kind == "CLUTTER") {
        for (size_t i = 0; i < schemas.size(); ++i)
          if (schemas[i].kind == "CLUTTER" && schemas[i].fields.size() > 2 &&
              schemas[i].fields[2] == "7") {
            kind = int(i);
            SendMessageW(kindList, CB_SETCURSEL, i, 0);
            refreshRecords();
            return;
          }
      }
      tool = 8;
      chooseKind("FIXTURE");
      for (auto button : tools)
        InvalidateRect(button, nullptr, FALSE);
      return;
    }
    auto &s = schemas[size_t(kind)];
    for (size_t i = s.scoped ? 2 : 1; i < s.fields.size(); ++i) {
      auto field = s.fields[i];
      std::string value;
      if (field == "model")
        value = std::to_string(slot);
      if (slot < int(std::size(retro::model::size))) {
        auto size = retro::model::size[slot];
        if (field == "width" && size[0] > 0)
          value = nativeNumber(size[0]);
        if (field == "depth" && size[1] > 0)
          value = nativeNumber(size[1]);
        if (field == "height" && size[2] > 0)
          value = nativeNumber(size[2]);
      }
      if (!value.empty())
        SetWindowTextW(fieldInputs[i - (s.scoped ? 2 : 1)],
                       wide(value).c_str());
    }
  }
  // The record list (Advanced) as it stands: what refreshRecords last put in it.
  std::vector<std::pair<LPARAM, std::wstring>> recordRows;
  // fields=false: the caller fills the inspector itself straight after.
  void refreshRecords(bool fields = true) {
    edperf::Scope perfScope("ui_refreshRecords");
    std::vector<std::pair<LPARAM, std::wstring>> rows;
    if (!schemas.empty()) {
      auto &s = schemas[size_t(kind)];
      for (auto index : doc.records(s.kind, s.scoped ? map : -1)) {
        auto f = NativeDocument::fields(doc.lines[index]);
        std::string text = s.kind + ": ";
        for (size_t i = s.scoped ? 2 : 1;
             i < std::min(f.size(), size_t(s.scoped ? 7 : 6)); i++) {
          if (i > (s.scoped ? 2 : 1))
            text += "  ";
          text += decoded(f[i]);
        }
        rows.emplace_back(LPARAM(index), wide(text));
      }
    }
    // Drawing held off while it changes: a list that redraws (and resizes its
    // scroll bar and frame) after every row took ~80 ms for a few hundred.
    // Only the rows that differ are replaced, so a pick or a nudge touches one
    // row instead of refilling them all; the list ends exactly as a refill
    // leaves it (same rows, nothing picked, scrolled to the top).
    SendMessageW(recordList, WM_SETREDRAW, FALSE, 0);
    if (rows.size() != recordRows.size() || rows.size() != size_t(SendMessageW(recordList, LB_GETCOUNT, 0, 0))) {
      SendMessageW(recordList, LB_RESETCONTENT, 0, 0);
      SendMessageW(recordList, LB_INITSTORAGE, rows.size(), rows.size() * 64);
      for (auto &[index, text] : rows) {
        auto n = SendMessageW(recordList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(recordList, LB_SETITEMDATA, n, index);
      }
    } else {
      for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].second != recordRows[i].second) {
          SendMessageW(recordList, LB_DELETESTRING, i, 0);
          SendMessageW(recordList, LB_INSERTSTRING, i, reinterpret_cast<LPARAM>(rows[i].second.c_str()));
        }
        if (rows[i].first != recordRows[i].first || rows[i].second != recordRows[i].second)
          SendMessageW(recordList, LB_SETITEMDATA, i, rows[i].first);
      }
      SendMessageW(recordList, LB_SETCURSEL, WPARAM(-1), 0);
      SendMessageW(recordList, LB_SETTOPINDEX, 0, 0);
    }
    recordRows = std::move(rows);
    SendMessageW(recordList, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(recordList, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    record = -1;
    if (fields)
      fillFields(true);
  }
  void refreshMaps() {
    if (!mapList)
      return;
    auto query = mapSearch ? utf8(label(mapSearch)) : "";
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    SendMessageW(mapList, LB_RESETCONTENT, 0, 0);
    for (auto i : doc.records("MAP")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 3)
        continue;
      auto searchable = f[1] + ": " + decoded(f[2]);
      std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                     [](unsigned char c) { return char(std::tolower(c)); });
      if (!query.empty() && searchable.find(query) == std::string::npos)
        continue;
      auto item =
          SendMessageW(mapList, LB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(
                           wide("Map " + f[1] + ": " + decoded(f[2])).c_str()));
      SendMessageW(mapList, LB_SETITEMDATA, item, std::stoi(f[1]));
      if (std::stoi(f[1]) == map)
        SendMessageW(mapList, LB_SETCURSEL, item, 0);
    }
  }
  void filterModels() {
    if (!modelList)
      return;
    auto query = modelSearch ? utf8(label(modelSearch)) : "";
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    auto selected = SendMessageW(modelList, CB_GETCURSEL, 0, 0);
    auto slot = selected >= 0
                    ? SendMessageW(modelList, CB_GETITEMDATA, selected, 0)
                    : -1;
    SendMessageW(modelList, CB_RESETCONTENT, 0, 0);
    LRESULT next = 0;
    for (const auto &[id, name] : models) {
      auto text = std::to_string(id) + ": " + name;
      auto searchable = text;
      std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                     [](unsigned char c) { return char(std::tolower(c)); });
      if (!query.empty() && searchable.find(query) == std::string::npos)
        continue;
      auto item = SendMessageW(modelList, CB_ADDSTRING, 0,
                               reinterpret_cast<LPARAM>(wide(text).c_str()));
      SendMessageW(modelList, CB_SETITEMDATA, item, id);
      if (id == slot)
        next = item;
    }
    SendMessageW(modelList, CB_SETCURSEL, next, 0);
  }
  void refresh() {
    if (!window)
      return;
    bool changedMap = false;
    if (doc.records("MAP", map).empty()) {
      auto maps = doc.records("MAP");
      if (!maps.empty()) {
        map = std::stoi(NativeDocument::fields(doc.lines[maps.front()])[1]);
        layer = 0;
        changedMap = true;
      }
    }
    if (changedMap && !session.empty())
      writeControl();
    refreshMaps();
    {auto floors=floorLayers();if(std::none_of(floors.begin(),floors.end(),[&](const auto& a){return a.id==layer;}))layer=floors.empty()?0:floors.front().id;if(selectedLayerGroup>=0&&floorGroup(selectedLayerGroup).id<0)selectedLayerGroup=-1;}
    if(layersDock)refreshLayerDock();
    SendMessageW(layerList, CB_RESETCONTENT, 0, 0);
    for (auto i : doc.records("LAYER", map)) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 5)
        continue;
      auto item =
          SendMessageW(layerList, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(
                           wide(decoded(f[3]) + "  (" + f[4] + " m)").c_str()));
      SendMessageW(layerList, CB_SETITEMDATA, item, std::stoi(f[2]));
      if (std::stoi(f[2]) == layer)
        SendMessageW(layerList, CB_SETCURSEL, item, 0);
    }
    refreshRecords();
    if (musicList) {
      int cue = 0;
      auto maps = doc.records("MAP", map);
      if (!maps.empty()) {
        auto fields = NativeDocument::fields(doc.lines[maps.front()]);
        if (fields.size() > 19 && !fields[19].empty())
          cue = std::stoi(fields[19]);
      }
      SendMessageW(musicList, CB_SETCURSEL, cue, 0);
    }
    InvalidateRect(plan, nullptr, FALSE);
  }
  void selectMusic(int cue) {
    if (cue < 0 || cue >= int(musicNames.size()))
      return;
    auto maps = doc.records("MAP", map);
    if (maps.empty())
      throw std::runtime_error("Select a map before setting its OST");
    auto fields = NativeDocument::fields(doc.lines[maps.front()]);
    // Fill the preceding optional native fields only when extending an old MAP.
    if (fields.size() < 18)
      fields.resize(18, "");
    if (fields[17].empty())
      fields[17] = "-1";
    if (fields.size() < 19)
      fields.push_back("0");
    if (fields[18].empty())
      fields[18] = "0";
    if (fields.size() < 20)
      fields.resize(20);
    fields[19] = std::to_string(cue);
    remember();
    doc.replace(maps.front(), fields);
    publish();
    refresh();
    message(L"Map music saved: " + wide(musicNames[size_t(cue)]));
  }
  void chooseKind(const std::string &name) {
    for (size_t i = 0; i < schemas.size(); i++)
      if (schemas[i].kind == name) {
        kind = int(i);
        if (kindList)
          SendMessageW(kindList, CB_SETCURSEL, i, 0);
        if (window)
          refreshRecords();
        return;
      }
  }
  int schemaFor(const std::vector<std::string> &fields) const {
    int best = -1, score = -1;
    for (size_t i = 0; i < schemas.size(); ++i) {
      const auto &s = schemas[i];
      if (fields.empty() || s.kind != fields[0])
        continue;
      if (!s.scoped) {
        if (score < 0) {
          best = int(i);
          score = 0;
        }
        continue;
      }
      int candidate = 0;
      bool matches = true;
      for (size_t j = 2; j < std::min(fields.size(), s.fields.size()); ++j) {
        const auto &name = s.fields[j];
        if (!name.empty() &&
            std::all_of(name.begin(), name.end(),
                        [](unsigned char c) { return std::isdigit(c); })) {
          if (name != fields[j]) {
            matches = false;
            break;
          }
          candidate += 100;
        }
      }
      if (matches && candidate > score) {
        best = int(i);
        score = candidate;
      }
    }
    return best;
  }
  std::optional<NativeHandle> handleFor(size_t index) const {
    auto f = NativeDocument::fields(doc.lines.at(index));
    if (f.size() < 2 || f[1] != std::to_string(map))
      return {};
    int schema = schemaFor(f);
    if (schema < 0 || !schemas[size_t(schema)].scoped)
      return {};
    return nativeHandle(schemas[size_t(schema)].fields, f);
  }
  // Hover repaints the plan at mouse rate; Starter has 16k lines, so the
  // object index list is cached until the document changes shape.
  std::vector<size_t> objects() const {
    auto key = std::tuple{map, layer, doc.lines.size(), changeCount, history.size(), future.size(), (const void *)doc.lines.data()};
    if (key == objectKey)
      return objectCache;
    std::vector<size_t> result;
    // Only lines on this map can have a handle here (handleFor checks the map first).
    for (size_t i : doc.onMap(map))
      if (handleFor(i))
        result.push_back(i);
    const_cast<Editor *>(this)->objectCache = result;
    const_cast<Editor *>(this)->objectKey = key;
    return result;
  }
  void selectRecord(int index, bool preserveSelection = false) {
    edperf::Scope perfScope("ui_selectRecord");
    if (ghosting)
      return;
    if(index>=0&&size_t(index)<doc.lines.size()&&handleFor(size_t(index)))layersDock=false;
    if(index!=record||!preserveSelection){mapHeightSelection=false;floorTransformSelection=false;}
    selectRecordOnly(index, preserveSelection);
    if(!session.empty()&&engineLive)writeControl();
    refreshLinks();
    relayout();
  }
  void selectRecordOnly(int index, bool preserveSelection = false) {
    if (!preserveSelection) { selectedRecords.clear(); if(index >= 0 && size_t(index) < doc.lines.size() && handleFor(size_t(index))) selectedRecords.push_back(index); }
    if (index < 0 || index >= int(doc.lines.size())) {
      record = -1;
      fillFields(true);
      InvalidateRect(plan, nullptr, FALSE);
      return;
    }
    int schema = schemaFor(NativeDocument::fields(doc.lines[size_t(index)]));
    if (schema < 0)
      return;
    kind = schema;
    SendMessageW(kindList, CB_SETCURSEL, kind, 0);
    refreshRecords(false); // the inspector is filled for this record just below
    record = index;
    auto count = SendMessageW(recordList, LB_GETCOUNT, 0, 0);
    for (LRESULT i = 0; i < count; ++i)
      if (SendMessageW(recordList, LB_GETITEMDATA, i, 0) == record) {
        SendMessageW(recordList, LB_SETCURSEL, i, 0);
        break;
      }
    fillFields(false);
    InvalidateRect(plan, nullptr, FALSE);
    if (!ghosting && window) signSelectionChanged();
  }
  int hitObject(float x, float y) const {
    int hit = -1;
    float best = 1e10f;
    for (auto index : objects()) {
      if (!activeFloorRecord(index)) continue;
      auto h = *handleFor(index);
      float px=x-h.x,py=y-h.y,w=h.width,d=h.depth;
      if(auto box=creationBox(NativeDocument::fields(doc.lines[index]))){float c=std::cos(box->yaw),s=std::sin(box->yaw),vx=x-box->x,vy=y-box->y;px=vx*c-vy*s;py=vx*s+vy*c;w=box->w;d=box->d;}
      float dx=std::max(0.f,std::abs(px)-w*.5f),dy=std::max(0.f,std::abs(py)-d*.5f);
      if (std::hypot(dx, dy) > .3f)
        continue;
      float score = std::hypot(x - h.x, y - h.y) + h.width * h.depth * .01f;
      if (score < best) {
        hit = int(index);
        best = score;
      }
    }
    return hit;
  }
  void updateInspector() {
    edperf::Scope perfScope("ui_updateInspector");
    if (record < 0)
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    size_t first = schemas[size_t(kind)].scoped ? 2 : 1;
    for (size_t i = 0; i < fieldInputs.size(); ++i)
      SetWindowTextW(
          fieldInputs[i],
          wide(i + first < f.size() ? decoded(f[i + first]) : "").c_str());
  }
  void moveSelected(float dx, float dy) {
    if(record>=0&&!editableRecord(size_t(record))){message(L"Unlock and show this floor before editing its content.");return;}
    if(selectedRecords.size()>1){moveMultiple(dx,dy);return;}
    edperf::Scope perfScope("ui_moveSelected");
    if (record < 0 || (dx == 0 && dy == 0))
      return;
    auto before = NativeDocument::fields(doc.lines[size_t(record)]);
    auto f = before;
    if (!translateNative(f, schemas[size_t(kind)].fields, dx, dy))
      return;
    int selected = record;
    remember();
    doc.replace(size_t(record), f);
    shiftActorCompanions(before, dx, dy);
    publish();
    selectRecord(record);
  }
  void rotateSelected() {
    if(record>=0&&!editableRecord(size_t(record))){message(L"Unlock and show this floor before editing its content.");return;}
    if(selectedRecords.size()>1){rotateMultiple();return;}
    if (record < 0)
      return;
    if (rotateDoor())
      return;
    if (emitterPicked()) {
      turnEmitter(45.f, false); // an eighth round: the way it is carried
      return;
    }
    if (filmPicked()) {
      turnFilm();
      return;
    }
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    auto &names = schemas[size_t(kind)].fields;
    auto yaw = std::find(names.begin(), names.end(), "yaw");
    auto turnField = [&](const char *name) {
      auto it = std::find(names.begin(), names.end(), name);
      return it == names.end() || size_t(it - names.begin()) >= f.size() ? size_t(0) : size_t(it - names.begin());
    };
    // Stairs turn by climbing the other way; shapes by their quarter facing.
    if (size_t along = turnField("alongY"), up = turnField("ascending"); along && up) {
      int state = (f[along] == "1" ? 0 : 1) + (f[up] == "1" ? 0 : 2);
      state = (state + 1) % 4;
      f[along] = state & 1 ? "0" : "1", f[up] = state & 2 ? "0" : "1";
    } else if (size_t facing = turnField("facing")) {
      f[facing] = std::to_string(((f[facing].empty() ? 0 : std::stoi(f[facing])) + 1) % 4);
    } else if (yaw == names.end()) {
      message(L"This one has no front to turn. Drag its ends instead.");
      return;
    }
    if (yaw == names.end()) {
      int selected = record;
      remember();
      doc.replace(size_t(record), f);
      publish();
      selectRecord(record);
      return;
    }
    size_t index = size_t(yaw - names.begin());
    if (f.size() <= index)
      f.resize(index + 1);
    float angle = f[index].empty() ? 0 : std::stof(f[index]);
    f[index] =
        nativeNumber(std::remainder(angle + 1.57079632679f, 6.28318530718f));
    normalize(f);
    int selected = record;
    remember();
    auto before = NativeDocument::fields(doc.lines[size_t(record)]);
    doc.replace(size_t(record), f);
    relink(before, f);
    publish();
    selectRecord(record);
  }
  void copySelected() {
    if(record>=0&&!editableRecord(size_t(record))){message(L"Unlock and show this floor before editing its content.");return;}
    if(selectedRecords.size()>1){copyMultiple();return;}
    if (record < 0)
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (f[0] == "ACTOR_KEY" || f[0].starts_with("SCRIPT_") ||
        !translateNative(f, schemas[size_t(kind)].fields,
                         snap > 0 ? snap : .25f, snap > 0 ? snap : .25f)) {
      message(L"Copy supports placed objects; linked script and actor records "
              L"use their native fields.");
      return;
    }
    remember();
    addAuthored(f);
    int created = -1;
    for (size_t i = 0; i < doc.lines.size(); ++i)
      if (doc.lines[i].starts_with("--- CUSTOM_CAMPAIGN_DATA_END ---")) {
        created = int(i) - 1;
        break;
      }
    record = created;
    publish();
    selectRecord(record);
  }
  void removeSelected() {
    if(record>=0&&!editableRecord(size_t(record))){message(L"Unlock and show this floor before editing its content.");return;}
    if(selectedRecords.size()>1){removeMultiple();return;}
    if (record < 0)
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (f[0] == "MAP" || f[0] == "ROW" || f[0] == "LAYER" ||
        f[0] == "CAMPAIGN") {
      message(
          L"Maps and floors stay. Paint over tiles to change them.");
      return;
    }
    auto next = doc;
    eraseNativeObject(next, size_t(record));
    remember();
    doc = std::move(next);
    publish();
    refreshRecords();
    selectRecord(-1);
    if (!quiet())
      sfx::play(sfx::Clip(sfx::BreakStone + sfx::material(f[0])), .8f);
    message(L"Deleted. Ctrl+Z brings it back.");
  }
  NativeStamp captureRegion(int x0, int y0, int x1, int y1) const {
    if (x0 > x1)
      std::swap(x0, x1);
    if (y0 > y1)
      std::swap(y0, y1);
    NativeStamp stamp;
    stamp.width = x1 - x0 + 1;
    stamp.height = y1 - y0 + 1;
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x)
        stamp.tiles.emplace_back(x - x0, y - y0, doc.tile(map, layer, x, y));
    float left = float(x0), top = float(y0), right = float(x1 + 1),
          bottom = float(y1 + 1);
    auto fixtures = doc.records("FIXTURE", map);
    auto clutter = doc.records("CLUTTER", map);
    for (auto index : objects()) {
      if(!visibleFloorRecord(index))continue;
      auto handle = *handleFor(index);
      float hx0 = handle.x - handle.width * .5f,
            hx1 = handle.x + handle.width * .5f;
      float hy0 = handle.y - handle.depth * .5f,
            hy1 = handle.y + handle.depth * .5f;
      if (hx1 <= left || hx0 >= right || hy1 <= top || hy0 >= bottom)
        continue;
      auto f = NativeDocument::fields(doc.lines[index]);
      int schema = schemaFor(f);
      if (schema < 0 || f[0] == "ACTOR_KEY" || f[0].starts_with("SCRIPT_"))
        continue;
      if (!translateNative(f, schemas[size_t(schema)].fields, -float(x0),
                           -float(y0)))
        continue;
      if (!clipBlock(f, float(stamp.width), float(stamp.height)))
        continue;
      NativeStamp::Object object;
      object.names = schemas[size_t(schema)].fields;
      object.fields = std::move(f);
      if (object.fields[0] == "FIXTURE")
        object.sourceFixture = int(std::find(fixtures.begin(), fixtures.end(),
                                             index) -
                                   fixtures.begin());
      if (object.fields[0] == "CLUTTER")
        object.sourceClutter = int(std::find(clutter.begin(), clutter.end(),
                                             index) -
                                   clutter.begin());
      stamp.objects.push_back(std::move(object));
    }
    return stamp;
  }
  void pasteRegion(int tileX, int tileY) {
    std::map<int, int> fixtures, clutter;
    int nextFixture = int(doc.records("FIXTURE", map).size());
    int nextClutter = int(doc.records("CLUTTER", map).size());
    for (const auto &object : regionStamp.objects) {
      if (object.sourceFixture >= 0)
        fixtures[object.sourceFixture] = nextFixture++;
      if (object.sourceClutter >= 0)
        clutter[object.sourceClutter] = nextClutter++;
    }
    for (auto [dx, dy, ch] : regionStamp.tiles) {
      auto [x, y] = stampCell(dx, dy, regionStamp.width, regionStamp.height,
                              rotation, mirror);
      doc.paint(map, layer, tileX + x, tileY + y, ch);
    }
    for (const auto &object : regionStamp.objects) {
      auto f = object.fields;
      stampInto(f, object.names, regionStamp.width, regionStamp.height,
                rotation, mirror, float(tileX), float(tileY));
      if (f.size() > 1)
        f[1] = std::to_string(map);
      auto rewrite = [&](size_t field, const std::map<int, int> &table,
                         std::initializer_list<size_t> companions) {
        if (field >= f.size() || f[field].empty())
          return;
        int reference = std::stoi(f[field]);
        auto found = table.find(reference);
        if (found == table.end()) {
          f[field].clear();
          for (auto companion : companions)
            if (companion < f.size())
              f[companion].clear();
        } else
          f[field] = std::to_string(found->second);
      };
      if (f[0] == "LEAD") {
        rewrite(13, clutter, {20});
        rewrite(18, fixtures, {19});
      }
      if (f[0] == "BREAKABLE" && f.size() >= 15 && !f[13].empty()) {
        int reference = std::stoi(f[13]);
        auto found = fixtures.find(reference);
        if (found == fixtures.end())
          f.resize(13);
        else
          f[13] = std::to_string(found->second);
      }
      while (f.size() > 2 && f.back().empty())
        f.pop_back();
      if (!clipBlock(f, 24.f, 24.f))
        continue;
      normalize(f);
      addAuthored(f);
    }
  }
  // A block that runs past the copied area (a room's floor slab spans the
  // room) keeps only the part inside it, and a paste near the edge keeps only
  // the part inside the map. False when nothing of the block is left.
  static bool clipBlock(std::vector<std::string> &f, float width, float depth) {
    if (f.empty() || f[0] != "STRUCT" || f.size() < 6)
      return true;
    for (size_t i : {size_t(2), size_t(3), size_t(4), size_t(5)}) {
      float v = number(f[i], 0), limit = i % 2 == 0 ? width : depth, kept = std::clamp(v, 0.f, limit);
      if (kept != v)
        f[i] = nativeNumber(kept);
    }
    return number(f[4], 0) > number(f[2], 0) && number(f[5], 0) > number(f[3], 0);
  }
  std::pair<float, float> mapOrigin() const {
    for (auto index : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      if (f.size() > 4)
        return {std::stof(f[3]), std::stof(f[4])};
    }
    return {float(map) * 24.f, 0.f};
  }
  bool prepareOutdoors() {
    bool opened = false;
    if (doc.records("TERRAIN").empty()) {
      addAuthored({"TERRAIN", "2026", "1", "2.5", "30", "0.45"});
      opened = true;
    }
    for (auto index : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      if (f.size() < 12)
        continue;
      if (f[8] == "0" && f[11] == "industrial_night") {
        f[8] = "1";
        f[11] = "brutal_wasteland";
        doc.replace(index, f);
        opened = true;
      }
    }
    return opened;
  }
  void placePlayer(float x, float y) {
    x = std::clamp(x, 0.f, 23.9f);
    y = std::clamp(y, 0.f, 23.9f);
    auto maps = doc.records("MAP", map);
    if (maps.empty())
      throw std::runtime_error("Select a map before placing the player");
    auto f = NativeDocument::fields(doc.lines[maps.front()]);
    if (f.size() < 7)
      throw std::runtime_error("This map has no spawn fields");
    f[5] = nativeNumber(x);
    f[6] = nativeNumber(y);f[7]=nativeNumber(layerElevation());
    remember();
    doc.replace(maps.front(), f);
    publish();
    selectRecord(int(maps.front()));
    message(L"Player start is now on this map. Walk / play begins here.");
  }
  void authorSurfaces() {
    auto existing = doc.records("SURFACE", map);
    if (!existing.empty()) {
      selectRecord(int(existing.front()));
      message(L"This room already has surfaces. Wall, floor and ceiling use "
              L"-1 through 12.");
      return;
    }
    remember();
    addAuthored({"SURFACE", std::to_string(map), "0", "0", "0"});
    publish();
    auto added = doc.records("SURFACE", map);
    if (!added.empty())
      selectRecord(int(added.back()));
    message(L"Room surfaces added: concrete wall, floor and ceiling. -1 keeps "
            L"the engine default.");
  }
  void useMaterial() {
    if (!materialList)
      return;
    auto selected = SendMessageW(materialList, CB_GETCURSEL, 0, 0);
    if (selected < 0)
      return;
    structureMaterial =
        int(SendMessageW(materialList, CB_GETITEMDATA, selected, 0));
    if(tool==2){floorPaintMaterial=structureMaterial;floorMaterialExplicit=true;message(L"Floor material: "+materialName(floorPaintMaterial));return;}
    if (record < 0) {
      message(L"New blocks use this finish. Select a block or part to change "
              L"one that is already placed.");
      return;
    }
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    int schema = schemaFor(f);
    if (schema < 0)
      return;
    auto &names = schemas[size_t(schema)].fields;
    auto field = std::find(names.begin(), names.end(), "material");
    if (field == names.end()) {
      message(L"The selected record has no material field.");
      return;
    }
    size_t index = size_t(field - names.begin());
    if (f.size() <= index)
      f.resize(index + 1);
    f[index] = std::to_string(structureMaterial);
    remember();
    doc.replace(size_t(record), f);
    publish();
    fillFields(false);
    message(L"Material written into the native record.");
  }
  void authorRect(int x0, int y0, int x1, int y1) {
    if (x0 > x1)
      std::swap(x0, x1);
    if (y0 > y1)
      std::swap(y0, y1);
    authorBox(float(x0), float(y0), float(x1 + 1), float(y1 + 1));
  }
  // Blocks, doors, hazards, ground and pads in map metres; drags need not sit on tiles.
  void authorBox(float left, float top, float right, float bottom) {
    std::vector<std::string> f;
    bool outdoors = tool == 23 || tool == 24;
    if (tool == 20)
      f = {"STRUCT",
           std::to_string(map),
           nativeNumber(left),
           nativeNumber(top),
           nativeNumber(right),
           nativeNumber(bottom),
           nativeNumber(layerElevation()),
           nativeNumber(layerElevation() + 3),
           "0",
           std::to_string(structureMaterial)};
    else if (tool == 21)
      f = {"DOOR",
           std::to_string(map),
           nativeNumber(left),
           nativeNumber(right),
           nativeNumber((top + bottom) * .5f),
           "0",
           "0",
           "0",
           "0",
           "0",
           "-1"};
    else if (tool == 22)
      f = {"HAZARD",
           std::to_string(map),
           std::to_string(listIndex(hazardList)),
           nativeNumber(left),
           nativeNumber(top),
           nativeNumber(right),
           nativeNumber(bottom),
           nativeNumber(layerElevation()),
           nativeNumber(layerElevation() + 2),
           "4"};
    else if (outdoors) {
      auto [ox, oy] = mapOrigin();
      f = {tool == 23 ? "GROUND" : "TERRAIN_FLAT",
           nativeNumber(ox + left),
           nativeNumber(oy + top),
           nativeNumber(ox + right),
           nativeNumber(oy + bottom)};
      if (tool == 23)
        f.insert(f.end(), {std::to_string(groundKind), "1"});
      else
        f.insert(f.end(), {"0", "2"});
    } else
      return;
    remember();
    bool opened = outdoors && prepareOutdoors();
    addAuthored(f);
    int scope = outdoors ? -1 : map;
    auto added = doc.records(f[0], scope);
    record = added.empty() ? -1 : int(added.back());
    publish();
    if (record >= 0)
      selectRecord(record);
    const wchar_t *openedNote =
        opened ? L" The map is outdoors now, under an open sky."
               : L"";
    message(std::wstring(tool == 20 ? L"Block placed. Pick another finish on the right if you like."
                         : tool == 21 ? L"Door fitted."
                         : tool == 22 ? L"Hazard placed. It hurts anyone who stands in it."
                         : tool == 23 ? L"Ground painted."
                                      : L"Ground levelled.") +
            openedNote);
  }
  struct PersonRole {
    const wchar_t *label;
    int visual;
    int mode;
  };
  static constexpr PersonRole people[] = {
      {L"Player start", -1, 0},      {L"Worker", 0, 1},
      {L"Scientist", 10, 1},         {L"Female scientist", 12, 1},
      {L"Doctor", 13, 1},            {L"Lab tech", 14, 1},
      {L"Investigator", 15, 1},      {L"Cargo", 3, 1},
      {L"Cleanup", 4, 1},            {L"Security guard", 4, 3},
      {L"Senior scientist", 11, 1},  {L"Cleanup engineer", 5, 1},
      {L"Cleanup sharpshooter", 6, 1}, {L"Cleanup heavy", 7, 1},
      {L"Cleanup medic", 8, 1},      {L"Cleanup rusher", 9, 1},
      {L"Wasp body", 1, 1},          {L"Huntsman body", 2, 1},
  };
  int personRole() const {
    if (!roleList)
      return 1;
    auto selected = SendMessageW(roleList, CB_GETCURSEL, 0, 0);
    if (selected < 0 || selected >= int(std::size(people)))
      return 1;
    return int(selected);
  }
  // People share the map's clock; a map that already has one keeps it.
  std::string ensureClock() {
    for (auto index : doc.records("TIMED_SEQUENCE", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      if (f.size() > 2 && !f[2].empty())
        return f[2];
    }
    addAuthored({"TIMED_SEQUENCE", std::to_string(map), "editor_clock",
             "editor_clock_done", "0", "0", "24", "24", "-100", "100", "60000",
             "60000", "0", "0", "0", "0", "1", "1"});
    return "editor_clock";
  }
  void placePerson(float x, float y) {
    x = std::clamp(x, .5f, 23.5f);
    y = std::clamp(y, .5f, 23.5f);
    auto role = people[personRole()];
    if (role.visual == -1) {
      placePlayer(x, y);
      return;
    }
    remember();
    auto clock = ensureClock();
    auto mi = std::to_string(map);
    auto index = std::to_string(doc.records("ACTOR", map).size());
    auto z = nativeNumber(layerElevation());
    addAuthored({"ACTOR", mi, clock, std::to_string(role.visual), "1", "0",
             "0", index});
    addAuthored({"ACTOR_KEY", mi, index, "0", nativeNumber(x), nativeNumber(y), z,
             "0", "0", "0"});
    addAuthored({"ACTOR_KEY", mi, index, "60000", nativeNumber(x), nativeNumber(y),
             z, "0", "0", "1"});
    // "Stands still" leaves the person on their two idle keys (no AI record).
    if (role.mode == 3 || listIndex(behaviourList) == 0)
      addAuthored({"ACTOR_AI", mi, index, std::to_string(role.mode), "", "1.4", "2",
               "6"});
    if (role.mode == 3) {
      addAuthored({"ACTOR_GUARD", mi, index, "12", "0.5", "600", "4000", ""});
      addAuthored({"ACTOR_WEAPON", mi, index, "2", "650", "1000", "9", "18"});
      addAuthored({"ACTOR_PATROL", mi, index, nativeNumber(x), nativeNumber(y), "0",
               "1000"});
    }
    publish();
    auto keys = doc.records("ACTOR_KEY", map);
    if (keys.size() >= 2)
      selectRecord(int(keys[keys.size() - 2]));
    message(std::wstring(L"Placed ") + role.label +
            L". Moving either pose key moves the whole person.");
  }
  void openWindow(int x0, int y0, int x1, int y1) {
    if (x0 > x1)
      std::swap(x0, x1);
    if (y0 > y1)
      std::swap(y0, y1);
    remember();
    int opened = 0;
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x)
        if (doc.tile(map, layer, x, y) == '#') {
          doc.paint(map, layer, x, y, '.');
          ++opened;
        }
    publish();
    message(opened ? L"Window opening cut through the wall."
                   : L"Windows cut through wall tiles. Paint a wall there first.");
  }
  void captureMates() {
    captureSelectionDrag();
    dragMates.clear();
    if (objectFields.size() < 3 ||
        (objectFields[0] != "ACTOR_KEY" && objectFields[0] != "ACTOR_PATROL"))
      return;
    for (const char *kind : {"ACTOR_KEY", "ACTOR_PATROL"})
      for (auto index : doc.records(kind, map)) {
        if (int(index) == record)
          continue;
        auto fields = NativeDocument::fields(doc.lines[index]);
        if (fields.size() > 2 && fields[1] == objectFields[1] &&
            fields[2] == objectFields[2])
          if(std::none_of(dragSelection.begin(),dragSelection.end(),[&](const auto& member){return member.first==index;}))dragMates.emplace_back(index, doc.lines[index]);
      }
  }
  void shiftActorCompanions(const std::vector<std::string> &source, float dx,
                            float dy) {
    if (source.size() < 3 ||
        (source[0] != "ACTOR_KEY" && source[0] != "ACTOR_PATROL") ||
        (dx == 0 && dy == 0))
      return;
    for (const char *kindName : {"ACTOR_KEY", "ACTOR_PATROL"})
      for (auto index : doc.records(kindName, map)) {
        if (int(index) == record)
          continue;
        auto fields = NativeDocument::fields(doc.lines[index]);
        if (fields.size() <= 2 || fields[1] != source[1] || fields[2] != source[2])
          continue;
        if (fields[0] == "ACTOR_PATROL" && fields.size() > 4) {
          fields[3] = nativeNumber(std::stof(fields[3]) + dx);
          fields[4] = nativeNumber(std::stof(fields[4]) + dy);
          doc.replace(index, fields);
          continue;
        }
        int schema = schemaFor(fields);
        if (schema >= 0 &&
            translateNative(fields, schemas[size_t(schema)].fields, dx, dy))
          doc.replace(index, fields);
      }
  }
  void applyDragDelta(float dx, float dy) {
    for(auto& member:dragSelection){auto f=NativeDocument::fields(member.second);int schema=schemaFor(f);
      if(schema>=0&&translateNative(f,schemas[size_t(schema)].fields,dx,dy))doc.replace(member.first,f);}
    auto moved = objectFields;
    if (!translateNative(moved, schemas[size_t(kind)].fields, dx, dy))
      return;
    doc.replace(size_t(record), moved);
    for (auto &mate : dragMates) {
      auto fields = NativeDocument::fields(mate.second);
      if (fields.size() > 4 && fields[0] == "ACTOR_PATROL") {
        fields[3] = nativeNumber(std::stof(fields[3]) + dx);
        fields[4] = nativeNumber(std::stof(fields[4]) + dy);
        doc.replace(mate.first, fields);
        continue;
      }
      int schema = schemaFor(fields);
      if (schema >= 0 &&
          translateNative(fields, schemas[size_t(schema)].fields, dx, dy))
        doc.replace(mate.first, fields);
    }
    objectMoved = doc.lines[size_t(record)] != objectOriginal;
    for (auto &mate : dragMates)
      objectMoved = objectMoved || doc.lines[mate.first] != mate.second;
    updateInspector();
  }
  int selectAlongRay(float ox,float oy,float oz,float dx,float dy,float dz) const {
    int best=-1;float bestT=1e9f;
    if(dx*dx+dy*dy+dz*dz<.000001f)return -1;
    for(auto i:objects())if(activeFloorRecord(i)){
      auto f=NativeDocument::fields(doc.lines[i]);auto box=creationBox(f);
      if(!box){auto h=handleFor(i);if(!h)continue;float bottom=layerElevation(),top=bottom+.3f;
        if(f[0]=="STRUCT"&&f.size()>7){bottom=number(f[6],bottom);top=number(f[7],top);}
        box=creation::Box{h->x,h->y,h->width,h->depth,bottom,top};}
      float t=0;if(rayNativeBox(ox,oy,oz,dx,dy,dz,box->x,box->y,box->w,box->d,box->bottom,box->top,box->yaw,t)&&t<bestT){bestT=t;best=int(i);}
    }
    // A wall is opaque to selection too. Props behind it must not light up.
    for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(doc.tile(map,layer,x,y)=='#'){
      float t=0;if(rayNativeBox(ox,oy,oz,dx,dy,dz,x+.5f,y+.5f,1,1,layerElevation(),ceilingHeight(),0,t)&&t+.03f<bestT){bestT=t;best=-1;}}
    return best;
  }
  bool floorPoint(float ox, float oy, float oz, float dx, float dy, float dz,
                  float plane, float &x, float &y) const {
    if (std::fabs(dz) < 1e-5f)
      return false;
    float t = (plane - oz) / dz;
    if (!(t > 0))
      return false;
    x = ox + dx * t;
    y = oy + dy * t;
    return std::isfinite(x) && std::isfinite(y);
  }
  // ---- Friendly authoring. One click or drag makes one native record with
  // defaults that already look right, so nobody has to type a number.
  int listIndex(HWND list, int fallback = 0) const {
    if (!list)
      return fallback;
    auto s = SendMessageW(list, CB_GETCURSEL, 0, 0);
    return s < 0 ? fallback : int(s);
  }
  float snapped(float v) const { return snap > 0 ? std::round(v / snap) * snap : v; }
  float ceilingHeight() const {
    for (auto index : doc.records("MAP", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      try {
        if (f.size() > 9)
          return std::stof(f[9]);
      } catch (...) {
      }
    }
    return 4.5f;
  }
  float nextLayerElevation() const {
    float best = 1e9f, here = layerElevation();
    for (auto index : doc.records("LAYER", map)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      try {
        if (f.size() > 4 && std::stof(f[4]) > here + .1f)
          best = std::min(best, std::stof(f[4]));
      } catch (...) {
      }
    }
    return best < 1e8f ? best : here + 3.5f;
  }
  int schemaOf(const std::string &name, const char *variant = nullptr) const {
    int found = -1;
    for (size_t i = 0; i < schemas.size(); ++i)
      if (schemas[i].kind == name) {
        bool match = variant ? schemas[i].fields.size() > 2 && schemas[i].fields[2] == variant
                             : !(name == "CLUTTER" && schemas[i].fields.size() > 2 && schemas[i].fields[2] == "7");
        if (match)
          return int(i);
        if (found < 0)
          found = int(i);
      }
    return found;
  }
  std::string uniqueName(const char *kindName, size_t field, const std::string &stem) const {
    for (int n = 1;; ++n) {
      auto name = stem + std::to_string(n);
      bool used = false;
      for (auto index : doc.records(kindName, map)) {
        auto f = NativeDocument::fields(doc.lines[index]);
        used |= f.size() > field && f[field] == name;
      }
      if (!used)
        return name;
    }
  }
  int selectedModel() const {
    auto s = modelList ? SendMessageW(modelList, CB_GETCURSEL, 0, 0) : -1;
    return s < 0 ? 0 : int(SendMessageW(modelList, CB_GETITEMDATA, s, 0));
  }
  static constexpr const char *primKinds[] = {"box", "pillar", "stairs", "ramp", "column",
                                              "arch", "vault", "dome", "doorframe", "window"};
  // Builds one record of this kind from a gesture. Box kinds take the dragged
  // corners, line kinds the two ends, point kinds the release point.
  int authorKind(const std::string &kindName, float ax, float ay, float bx, float by) {
    bool model = kindName == "CLUTTER" && listIndex(clutterList) == 7;
    int si = schemaOf(kindName, model ? "7" : nullptr);
    if (si < 0)
      throw std::runtime_error("This part is missing from the map guide");
    const Schema s = schemas[size_t(si)];
    std::vector<std::string> f(s.fields.size());
    f[0] = s.kind;
    for (int i = 1; i < s.required && i < int(f.size()); ++i)
      f[size_t(i)] = initial(s, i);
    auto set = [&](const char *name, const std::string &value) {
      auto it = std::find(s.fields.begin(), s.fields.end(), name);
      if (it == s.fields.end())
        return;
      size_t i = size_t(it - s.fields.begin());
      if (f.size() <= i)
        f.resize(i + 1);
      f[i] = value;
    };
    auto n = [](float v) { return nativeNumber(v); };
    float lx = std::min(ax, bx), hx = std::max(ax, bx), ly = std::min(ay, by), hy = std::max(ay, by);
    // A click with a box tool makes one tile, never a sliver.
    if (hx - lx < .01f)
      lx = std::floor(bx), hx = lx + 1;
    if (hy - ly < .01f)
      ly = std::floor(by), hy = ly + 1;
    static constexpr float heights[] = {0.f, .9f, 1.5f, 2.1f};
    float floorZ = layerElevation(), lift = floorZ + heights[std::clamp(placeHeight, 0, 3)];
    set("x", n(bx)), set("y", n(by));
    set("x1", n(lx)), set("y1", n(ly)), set("x2", n(hx)), set("y2", n(hy));
    auto sized = [&](int slot) {
      float w = 1, d = 1, h = 1;
      if (slot >= 0 && slot < int(std::size(retro::model::size))) {
        auto size = retro::model::size[slot];
        if (size[0] > 0) w = size[0];
        if (size[1] > 0) d = size[1];
        if (size[2] > 0) h = size[2];
      }
      set("model", std::to_string(slot)), set("width", n(w)), set("depth", n(d)), set("height", n(h));
    };
    const auto &k = s.kind;
    if (k == "LIGHT") {
      float z = std::max(floorZ + .1f, std::min(floorZ + 3.2f, creationCeiling() - .2f));
      set("z", n(z)), set("mount", "0"), set("yaw", "0"), set("intensity", "1"), set("range", "8"), set("kelvin", "3200");
    } else if (k == "AMBIENT") {
      // The sound picked in the Sound tool's list, with its own loudness and reach: a bed loops, the rest play now and then.
      if (bankSounds.empty())
        throw std::runtime_error("The sound bank (src/audio/SoundBank.h) was not found");
      const auto &pick = bankSounds[size_t(std::clamp(soundPick >= 0 ? soundPick : listIndex(soundList), 0, int(bankSounds.size()) - 1))];
      set("sound", pick.name), set("z", n(floorZ + 1.5f)), set("gain", pick.gain), set("radius", pick.radius);
      set("mode", pick.loop ? "0" : "1"), set("every", "20"), set("spread", "20");
    } else if (k == "SIGN") {
      // The Sign designer's design, on the nearest wall (NativeSignDesigner.inc).
      authorSign(f, bx, by, floorZ);
    } else if (k == "FIXTURE") {
      sized(selectedModel());
      set("base", n(lift-bottomLayerElevation())), set("yaw", "0"), set("solid", "1");
    } else if (k == "CLUTTER") {
      set("kind", std::to_string(std::min(listIndex(clutterList), 7))), set("z", n(lift)), set("yaw", "0");
      if (model)
        sized(selectedModel()), set("pitch", "0"), set("roll", "0"), set("breakState", "");
    } else if (k == "PRIM") {
      set("kind", primKinds[std::clamp(listIndex(primList), 0, int(std::size(primKinds)) - 1)]);
      set("z", n(lift)), set("w", "0"), set("d", "0"), set("h", "0"), set("facing", "0");
      set("material", std::to_string(structureMaterial));
    } else if (k == "TERMINAL") {
      set("z", n(lift)), set("control", "0"), set("title", "COMPUTER"), set("line1", "HELLO."), set("line2", "");
    } else if (k == "CREATURE") {
      set("kind", std::to_string(listIndex(creatureList))), set("z", n(floorZ));
    } else if (k == "PICKUP") {
      set("kind", std::to_string(listIndex(pickupList)));
    } else if (k == "EMITTER") {
      // The preset picked in the Emitter tool's list, as its kit helper writes it (NativeEmitterTool.inc).
      f = emitterFields(emitterPresets[std::clamp(emitterPreset, 0, EmitterPresetCount - 1)], bx, by);
    } else if (k == FilmKind) {
      authorFilm(f, ax, ay, bx, by);
    } else if (k == "PIPE") {
      set("x1", n(ax)), set("y1", n(ay)), set("x2", n(bx)), set("y2", n(by));
      // A click makes a riser from the floor up; a drag runs along the ceiling.
      bool riser = std::hypot(bx - ax, by - ay) < .01f;
      set("z", n(riser ? floorZ : floorZ + 2.6f)), set("radius", "0.09"), set("endZ", n(floorZ + 2.6f)), set("material", "0");
    } else if (k == "LEAD") {
      float length = std::max(.5f, std::hypot(bx - ax, by - ay) * 1.15f);
      set("ax", n(ax)), set("ay", n(ay)), set("az", n(floorZ + 1.3f));
      set("bx", n(bx)), set("by", n(by)), set("bz", n(floorZ + 1.3f));
      set("length", n(length)), set("radius", "0.008"), set("material", "8"), set("live", "0");
    } else if (k == "WATER") {
      set("bed", n(floorZ)), set("surface", n(floorZ + .4f));
    } else if (k == "STAIR") {
      float top = nextLayerElevation();
      set("bottom", n(floorZ)), set("top", n(top));
      set("steps", std::to_string(std::max(3, int(std::round((top - floorZ) / .19f)))));
      set("alongY", hy - ly >= hx - lx ? "1" : "0"), set("ascending", "1");
    } else if (k == "ZONE") {
      set("name", "NEW PLACE");
    } else if (k == "SCRIPT_EVENT") {
      set("id", uniqueName("SCRIPT_EVENT", 2, "trigger_"));
      set("bottom", n(floorZ - 1)), set("top", n(floorZ + 5)), set("requireState", ""), set("requireValue", "1"), set("needClear", "0");
    } else if (k == "COMPACTOR") {
      set("bed", n(floorZ)), set("raised", n(floorZ + 2.6f)), set("period", "5");
    } else if (k == "BREAKABLE") {
      set("bottom", n(floorZ)), set("top", n(floorZ + 2.2f)), set("material", "17"), set("health", "20");
      set("holds", "0"), set("brokenState", uniqueName("BREAKABLE", 11, "glass_broken_")), set("triggerState", "");
    } else if (k == "DARK") {
    }
    while (f.size() > size_t(std::max(2, s.required)) && f.back().empty())
      f.pop_back();
    normalize(f);
    addAuthored(f);
    // A new trigger already says something, so stepping in shows it works.
    if (k == "SCRIPT_EVENT" && autoDetail)
      addAuthored({"SCRIPT_ACTION", std::to_string(map), f[2], "MESSAGE", "SOMETHING STIRS.", "4"});
    auto added = doc.records(k, map);
    return added.empty() ? -1 : int(added.back());
  }
  void repaintTools() {
    if (ghosting)
      return;
    for (auto button : tools)
      InvalidateRect(button, nullptr, FALSE);
    relayout();
  }
  // Hidden test windows skip the panel shuffle; the shown window always lays out.
  void relayout() {
    if (toolShelf && IsWindowVisible(window))
      layout();
  }
  char tileFor(int t) const {
    if(t==2&&layer>0&&upperFloorVoid)return '.';
    static constexpr char crates[] = {'C', 'B', 'T'};
    return t == 3 ? '#' : t == 4 ? '_' : t == 5 || (layer > 0 && (t == 1 || t == 2)) ? '=' : t == CrateTool ? crates[std::clamp(listIndex(tileList), 0, 2)] : '.';
  }
  // Paint-program fill: every touching tile of the same kind changes.
  void fillTiles(int x,int y,char ch) {
    char from=doc.tile(map,layer,x,y);bool floorEdit=tool==4||(tool==2&&(layer>0||floorMaterialExplicit));if(from==ch&&!floorEdit)return;
    std::array<bool,576> visited{};std::vector<std::pair<int,int>> stack{{x,y}};
    while(!stack.empty()){auto [cx,cy]=stack.back();stack.pop_back();if(cx<0||cy<0||cx>=24||cy>=24||visited[size_t(cy*24+cx)]||doc.tile(map,layer,cx,cy)!=from)continue;visited[size_t(cy*24+cx)]=true;
      if(ch=='#')clearCreationWalls({cy*24+cx});doc.paint(map,layer,cx,cy,ch);
      if(floorEdit){if(tool==4||(layer>0&&upperFloorVoid))clearNativeFloor(float(cx),float(cy),float(cx+1),float(cy+1));else paintNativeFloor(float(cx),float(cy),float(cx+1),float(cy+1),floorPaintMaterial);}
      stack.insert(stack.end(),{{cx+1,cy},{cx-1,cy},{cx,cy+1},{cx,cy-1}});
    }
  }
  // Tiny Glade: a dragged room arrives finished, with lamps and a way in.
  void detailRoom(int x0, int y0, int x1, int y1) {
    std::vector<int> cells;
    for(int y=y0+1;y<y1;++y)for(int x=x0+1;x<x1;++x)cells.push_back(y*24+x);
    int lamps=addCreationLights(cells);
    int w=x1-x0-1;
    int wall = y1 < 23 ? y1 : y0 > 0 ? y0 : -1;
    if (richDetail) {
      auto opensOnto = [&](int row, int beyond) {
        if (row <= 0 || row >= 23)
          return 0;
        int floor = 0;
        for (int x = x0 + 1; x < x1; ++x)
          floor += doc.tile(map, layer, x, beyond) == '.';
        return floor;
      };
      int south = opensOnto(y1, y1 + 1), north = opensOnto(y0, y0 - 1);
      if (north > south && y0 > 0)
        wall = y0;
    }
    if (wall >= 0 && w >= 1) {
      int span = w >= 4 ? 2 : 1, left = x0 + 1 + (w - span) / 2;
      for (int x = left; x < left + span; ++x)
        doc.paint(map, layer, x, wall, layer > 0 ? '=' : '.');
      addAuthored({"DOOR", std::to_string(map), nativeNumber(float(left)), nativeNumber(float(left + span)),
               nativeNumber(wall + .5f), nativeNumber(layerElevation()), "0", "0", "0", "0", "-1"});
    }
    int things = richDetail ? furnishRoom(x0, y0, x1, y1) : 0;
    message(L"Room built with " + std::to_wstring(lamps) + (lamps == 1 ? L" lamp" : L" lamps") +
            (things ? L", " + std::to_wstring(things) + L" things to start with" : L"") +
            (wall >= 0 ? L" and a door." : L".") + L" Drag another room next to it.");
  }
  // Every plan drag, 3D drag and test drives this one entry with map metres.
  // Every gesture lands with a sound and a pop: what was made, where it was made.
  void gesture(float ax, float ay, float bx, float by) {
    size_t before = doc.lines.size();
    auto text = quiet() ? std::string{} : doc.text();
    int used = tool;
    gestureNow(ax, ay, bx, by);
    if (quiet() || doc.text() == text)
      return;
    const auto &info = toolInfo[std::clamp(used, 0, ToolCount - 1)];
    std::string kind = info.kind;
    if (used == 13)
      kind = "ACTOR";
    else if (used == CrateTool)
      kind = "CLUTTER";
    int m = sfx::material(kind);
    bool erased = doc.lines.size() < before || used == 4;
    sfx::play(sfx::Clip((erased ? sfx::BreakStone : sfx::PlaceStone) + m), erased ? .7f : 1.f);
    char shape = info.shape;
    bool area = shape == 'r' || shape == 'R' || shape == 't' || shape == 'w' || shape == 'l' || shape == 'd';
    pop(area ? (ax + bx) * .5f : bx, area ? (ay + by) * .5f : by,
        erased ? ui::Hazard : kind.empty() ? ui::Wall : ui::kindColor(kind));
    // Remember the room this happened in, offer what could come next, and cheer a finished room.
    float rx = shape == 'K' ? ax : area ? (ax + bx) * .5f : bx, ry = shape == 'K' ? ay : area ? (ay + by) * .5f : by;
    if (!roomAt(cellOf(rx), cellOf(ry)).empty())
      lastRoomX = cellOf(rx), lastRoomY = cellOf(ry);
    ideasFor(used);
    celebrate();
  }
  void gestureNow(float ax, float ay, float bx, float by) {
    if(effectiveFloorView(floorLayer(layer)).locked){message(L"Unlock the active floor before editing it.");return;}
    const auto &info = toolInfo[std::clamp(tool, 0, ToolCount - 1)];
    if (info.shape == 'P') {
      paintGesture(ax, ay, bx, by);
      return;
    }
    if (info.shape == 'K') {
      corridor(ax, ay, bx, by);
      return;
    }
    auto cell = [](float v) { return std::clamp(int(std::floor(v)), 0, 23); };
    int x0 = cell(ax), y0 = cell(ay), x1 = cell(bx), y1 = cell(by);
    int tx0 = std::min(x0, x1), tx1 = std::max(x0, x1), ty0 = std::min(y0, y1), ty1 = std::max(y0, y1);
    switch (info.shape) {
    case 's':
      return;
    case 'a':
      if (tool == 26)
        authorSurfaces();
      return;
    case 'g':
      placePlayer(bx, by);
      return;
    case 'w':
      openWindow(tx0, ty0, tx1, ty1);
      return;
    case 'c':
      regionStamp = captureRegion(tx0, ty0, tx1, ty1);
      tool = 17;
      repaintTools();
      message(L"Copied. Click where it should go. Turn copy and Flip copy change it.");
      return;
    case 'v':
      if (regionStamp.width == 0) {
        tool = 16;
        repaintTools();
        message(L"Nothing copied yet. Drag over an area first.");
        return;
      }
      remember();
      pasteRegion(x1, y1);
      publish();
      message(L"Pasted. Click again for another copy.");
      return;
    case 't':
    case 'R': {
      char ch = tileFor(tool);
      remember();
      if(info.shape=='R'||ch=='#'){
        std::vector<int> walls;
        for(int y=ty0;y<=ty1;++y)for(int x=tx0;x<=tx1;++x)
          if(info.shape!='R'||x==tx0||x==tx1||y==ty0||y==ty1)walls.push_back(y*24+x);
        clearCreationWalls(walls);
      }
      if (info.shape == 't' && brush == 2)
        fillTiles(x1, y1, ch);
      else
        for (int y = ty0; y <= ty1; ++y)
          for (int x = tx0; x <= tx1; ++x)
            doc.paint(map, layer, x, y, info.shape == 'R' && (x == tx0 || x == tx1 || y == ty0 || y == ty1) ? '#' : ch);
      if((tool==2||tool==4)&&brush!=2){if(tool==4||(layer>0&&upperFloorVoid))clearNativeFloor(float(tx0),float(ty0),float(tx1+1),float(ty1+1));else if(layer>0||floorMaterialExplicit)paintNativeFloor(float(tx0),float(ty0),float(tx1+1),float(ty1+1),floorPaintMaterial);}
      if (info.shape == 'R' && autoDetail && autoDressingEnabled && tx1 - tx0 >= 2 && ty1 - ty0 >= 2)
        detailRoom(tx0, ty0, tx1, ty1);
      else
        message(info.shape == 'R' ? L"Room drawn." : L"Painted. Ctrl+Z takes it back.");
      publish();
      return;
    }
    case 'd':
      authorDoor(plannedDoor(ax, ay, bx, by));
      return;
    case 'p':
      if (tool == 13) {
        placePerson(snapped(bx), snapped(by));
        return;
      }
      [[fallthrough]];
    case 'r':
    case 'l': {
      if (info.shape == 'p')
        ax = bx = snapped(bx), ay = by = snapped(by);
      else
        ax = snapped(ax), ay = snapped(ay), bx = snapped(bx), by = snapped(by);
      if (tool == 20 || tool == 22 || tool == 23 || tool == 24) {
        float l = std::min(ax, bx), r = std::max(ax, bx), t = std::min(ay, by), b = std::max(ay, by);
        if (r - l < .01f)
          l = std::floor(bx), r = l + 1;
        if (b - t < .01f)
          t = std::floor(by), b = t + 1;
        authorBox(l, t, r, b);
        return;
      }
      if (info.shape == 'l' && tool != 10 && tool != FilmTool && std::hypot(bx - ax, by - ay) < .1f) {
        message(L"Drag from one end to the other.");
        return;
      }
      remember();
      int added = authorKind(info.kind, ax, ay, bx, by);
      if (smartSnap && added >= 0 && std::string(info.kind) == "FIXTURE")
        added=snapToWall(added);
      record = added;
      publish();
      selectRecord(record);
      message(std::wstring(info.name) + L" placed. Drag it to move, or change it on the right.");
      return;
    }
    }
  }
  // A 3D drag shows the real result while the button is held: the gesture
  // runs on a scratch copy that only the engine sees; the document is untouched.
  void previewGesture(float ax, float ay, float bx, float by) {
    char shape = toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape;
    if (!engineLive || ghosting || shape == 'c' || shape == 'v' || shape == 'a' || shape == 's' ||
        renderedRevision != revision || GetTickCount64() - lastGhost < 120)
      return;
    auto keep = doc;
    int keepRecord = record, keepTool = tool, keepKind = kind;
    ghosting = true;
    try {
      gesture(ax, ay, bx, by);
    } catch (...) {
    }
    ghosting = false;
    doc = std::move(keep);
    record = keepRecord, tool = keepTool, kind = keepKind;
    lastGhost = GetTickCount64();
    ghostShown = true;
  }
  // Puts the engine back on the real document after an abandoned preview.
  void dropGhost() {
    if (ghostShown)
      ghostShown = false, publish(false);
  }
  // Brush strokes paint as the pointer passes; one undo covers the stroke.
  void brushCell(float x, float y) {
    if(effectiveFloorView(floorLayer(layer)).locked)return;
    int cx = std::clamp(int(std::floor(x)), 0, 23), cy = std::clamp(int(std::floor(y)), 0, 23);
    char brushShape = toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape;
    if (brushShape == 'F') {
      // The outline lives beside the map until it becomes walls, a block or terraces.
      if (!history.empty() && history.back() == doc.text())
        history.pop_back();
      outline[size_t(cy * 24 + cx)] = (GetAsyncKeyState(VK_MENU) & 0x8000) ? 0 : 1;
      if (ideas.empty() || ideas[0].action != 11)
        ideasFor(ShapeDrawTool);
      InvalidateRect(plan, nullptr, FALSE);
      return;
    }
    if (brushShape == 'H') {
      paintHeight(cx, cy);
      return;
    }
    if (tool!=4&&!(tool==2&&(layer>0||floorMaterialExplicit))&&doc.tile(map, layer, cx, cy) == tileFor(tool))
      return;
    if(tileFor(tool)=='#')clearCreationWalls({cy*24+cx});
    doc.paint(map, layer, cx, cy, tileFor(tool));
    if(tool==4||(tool==2&&layer>0&&upperFloorVoid))clearNativeFloor(float(cx),float(cy),float(cx+1),float(cy+1));
    else if(tool==2&&(layer>0||floorMaterialExplicit))paintNativeFloor(float(cx),float(cy),float(cx+1),float(cy+1),floorPaintMaterial);
    ++changeCount;
    static ULONGLONG lastStroke = 0;
    if (!quiet() && GetTickCount64() - lastStroke > 70) {
      lastStroke = GetTickCount64();
      sfx::play(tileFor(tool) == '_' ? sfx::BreakStone : sfx::PlaceStone, tileFor(tool) == '_' ? .45f : .6f, .12f);
    }
    if (liveTurn(150)) {
      publish();
      lastDragPublish = GetTickCount64();
    }
    InvalidateRect(plan, nullptr, FALSE);
  }
  bool brushTool() const {
    char s = toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape;
    return (s == 't' && brush == 0) || s == 'F' || s == 'H';
  }
  bool fillTool() const { return toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape == 't' && brush == 2; }
  bool dragTool() const {
    char s = toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape;
    return s == 'r' || s == 'l' || s == 'R' || s == 'w' || s == 'c' || s == 'd' || s == 'P' || s == 'K' || (s == 't' && brush == 1);
  }
  bool mapIsBlank() const {
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x)
        if (doc.tile(map, layer, x, y) == '#')
          return false;
    for (const char *kindName :
         {"STRUCT", "DOOR", "FIXTURE", "CLUTTER", "SIGN", "PRIM", "PIPE",
          "WATER", "STAIR", "CREATURE", "TERMINAL", "HAZARD", "ACTOR"})
      if (!doc.records(kindName, map).empty())
        return false;
    return true;
  }
  void welcomeIfBlank() {
    if (!mapIsBlank())
      return;
    tool = 1;
    for (auto button : tools)
      InvalidateRect(button, nullptr, TRUE);
    message(L"Drag a box to make a room. Lamps and a door appear by themselves.");
  }
  void furnish() {
    float ceiling = 4.5f;
    std::string title = "ROOM";
    for (auto index : doc.records("MAP", map)) {
      auto fields = NativeDocument::fields(doc.lines[index]);
      if (fields.size() > 9) {
        try {
          ceiling = std::stof(fields[9]);
        } catch (...) {
        }
      }
      if (fields.size() > 2 && !fields[2].empty())
        title = fields[2];
    }
    for (auto &ch : title)
      ch = char(std::toupper(static_cast<unsigned char>(ch)));
    remember();int added=0;auto barriersNow=barriers();std::vector<char> wanted(576,0);
    for(int y=0;y<24;++y)for(int x=0;x<24;++x){char ch=doc.tile(map,layer,x,y);
      wanted[size_t(y*24+x)]=(ch=='.'||ch=='=')&&!barriersNow[size_t(y*24+x)];}
    for(int c=0;c<576;++c)if(wanted[size_t(c)]){
      std::vector<int> cells,stack{c};wanted[size_t(c)]=0;
      while(!stack.empty()){int at=stack.back();stack.pop_back();cells.push_back(at);
        for(auto [dx,dy]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){int x=at%24+dx,y=at/24+dy;
          if(x<0||y<0||x>=24||y>=24)continue;int next=y*24+x;
          if(wanted[size_t(next)]){wanted[size_t(next)]=0;stack.push_back(next);}}}
      added+=addCreationLights(cells);
    }
    int signs = 0;
    for (auto index : doc.records("DOOR", map)) {
      auto door = NativeDocument::fields(doc.lines[index]);
      if (door.size() < 6 || !onCreationFloor(number(door[5],-999)))
        continue;
      float x = 0, y = 0;
      bool alongY = nativeFlag(door, 21);
      try {
        x = (std::stof(door[2]) + std::stof(door[3])) * .5f;
        y = std::stof(door[4]);
      } catch (...) {
        continue;
      }
      if (alongY)
        std::swap(x, y);
      bool signedAlready = false;
      for (auto signIndex : doc.records("SIGN", map)) {
        auto sign = NativeDocument::fields(doc.lines[signIndex]);
        if (sign.size() > 4 && onCreationFloor(number(sign[4],-999)))
          try {
            if (std::hypot(std::stof(sign[2]) - x, std::stof(sign[3]) - y) <
                1.6f)
              signedAlready = true;
          } catch (...) {
          }
      }
      if (signedAlready)
        continue;
      doc.addAutomatic({"SIGN", std::to_string(map), nativeNumber(x), nativeNumber(y),
               nativeNumber(layerElevation()+2.15f), "1.6", "0.55", alongY ? "1.5707964" : "0", title, "", "14238785"},layer);
      ++signs;
    }
    publish();
    message(L"Furnished this room with " + std::to_wstring(added) +
            L" lamp(s) and " + std::to_wstring(signs) +
            L" door sign(s). Drag anything that is in the way.");
  }
  void applyOutdoors() {
    remember();
    bool opened = prepareOutdoors();
    if (doc.records("HORIZON").empty())
      addAuthored({"HORIZON", "110", "5", "9", "121,122,121,118,150,120"});
    publish();
    auto terrain = doc.records("TERRAIN");
    if (!terrain.empty())
      selectRecord(int(terrain.front()));
    message(opened
                ? L"Outdoors on. Terrain and the tree line are in the fields; this map now uses the wasteland sky."
                : L"Terrain and the tree line are selected. Edit the numbers, or paint ground and pads on the plan.");
  }
  void applySurfaceSet(int id) {
    if (id < -1 || id > 12)
      return;
    auto value = std::to_string(id);
    auto existing = doc.records("SURFACE", map);
    remember();
    if (existing.empty())
      addAuthored({"SURFACE", std::to_string(map), value, value, value});
    else {
      auto fields = NativeDocument::fields(doc.lines[existing.front()]);
      if (fields.size() < 5)
        fields.resize(5);
      fields[2] = fields[3] = fields[4] = value;
      doc.replace(existing.front(), fields);
    }
    publish();
    auto written = doc.records("SURFACE", map);
    if (!written.empty())
      selectRecord(int(written.front()));
    message(L"Wall, floor and ceiling now use this surface set. The fields can still differ.");
  }
  void applyPick(const std::string &line) {
    std::istringstream in(line);
    std::string tag;
    int serial = 0, held = 0;
    float ox, oy, oz, dx, dy, dz;
    if (!(in >> tag >> serial >> held >> ox >> oy >> oz >> dx >> dy >> dz) ||
        tag != "PICK" || playing)
      return;
    if (serial != pickSerial) {
      pickSerial = serial;
      if (objectDrag)
        finishDrag(false);
      if (brushing)
        brushing = false, publish();
      if (viewRect)
        viewRect = false, dropGhost();
      viewDrag = false;
      viewRect = false;
      if (!held)
        return;
      if (tool == PaintTool) {
        paintRay(ox, oy, oz, dx, dy, dz);
        return;
      }
      if (tool != 0) {
        float fx, fy;
        if (!floorPoint(ox, oy, oz, dx, dy, dz, layerElevation(), fx, fy))
          return;
        fx = std::clamp(fx, 0.f, 23.99f), fy = std::clamp(fy, 0.f, 23.99f);
        // The same tools as the plan: brush paints under the pointer, drag
        // tools stretch from the press, clicks place at once.
        if (brushTool()) {
          remember();
          brushing = true;
          brushCell(fx, fy);
          return;
        }
        char shape = toolInfo[std::clamp(tool, 0, ToolCount - 1)].shape;
        if (dragTool() || shape == 'p' || shape == 'g') {
          viewRect = true;
          dragFX = fx, dragFY = fy;
          viewX0 = viewX1 = int(fx), viewY0 = viewY1 = int(fy);
          hoverX = fx, hoverY = fy;
          return;
        }
        gesture(fx, fy, fx, fy);
        return;
      }
      int modifiers=0;in>>modifiers;bool extend=(modifiers&1)!=0;
      if(!extend&&(beginHeightDrag(ox,oy,oz,dx,dy,dz)||beginFootprintDrag3D(ox,oy,oz,dx,dy,dz)))return;
      int hit = selectAlongRay(ox, oy, oz, dx, dy, dz);
      if(hit<0){pickMapSurface(ox,oy,oz,dx,dy,dz);return;}
      if(!extend&&selected(hit))selectRecord(hit,true);else selectionPick(hit,extend);
      if(extend&&!selected(hit))return;
      beginBodyDrag3D(ox,oy,oz,dx,dy,dz);
      return;
    }
    if(held&&bodyDragging&&objectDrag){applyBodyDrag3D(ox,oy,oz,dx,dy,dz);if(objectMoved&&liveTurn()){publish();lastDragPublish=GetTickCount64();}return;}
    if(held&&verticalDragging&&objectDrag){float z;if(verticalRay(ox,oy,oz,dx,dy,dz,z)){applyHeightDelta(z-verticalStart);if(objectMoved&&liveTurn())publish();}return;}
    if(held&&viewDrag&&objectDrag&&resizeHandle>=0){float fx,fy;if(floorPoint(ox,oy,oz,dx,dy,dz,resizePlane,fx,fy)){applyCornerDrag(fx-viewGrabX,fy-viewGrabY);if(objectMoved&&liveTurn())publish();}return;}
    if (held && viewDrag && objectDrag) {
      float fx, fy;
      if (!floorPoint(ox, oy, oz, dx, dy, dz, 0, fx, fy))
        return;
      float ax = fx - viewGrabX, ay = fy - viewGrabY;
      if (snap > 0) {
        ax = std::round(ax / snap) * snap;
        ay = std::round(ay / snap) * snap;
      }
      auto original = nativeHandle(schemas[size_t(kind)].fields, objectFields);
      if (!original)
        return;
      applyDragDelta(ax - original->x, ay - original->y);
      InvalidateRect(plan, nullptr, FALSE);
      if (objectMoved && liveTurn()) {
        publish();
        lastDragPublish = GetTickCount64();
      }
    } else if (!held && viewDrag) {
      finishDrag(false);
      viewDrag = false;
    } else if (brushing) {
      float fx, fy;
      if (held && floorPoint(ox, oy, oz, dx, dy, dz, layerElevation(), fx, fy))
        brushCell(std::clamp(fx, 0.f, 23.99f), std::clamp(fy, 0.f, 23.99f));
      if (!held) {
        brushing = false;
        publish();
      }
    } else if (viewRect) {
      float fx = hoverX, fy = hoverY;
      if (floorPoint(ox, oy, oz, dx, dy, dz, layerElevation(), fx, fy)) {
        fx = std::clamp(fx, 0.f, 23.99f), fy = std::clamp(fy, 0.f, 23.99f);
        hoverX = fx, hoverY = fy;
        viewX1 = int(fx), viewY1 = int(fy);
        InvalidateRect(plan, nullptr, FALSE);
      }
      if (held)
        previewGesture(dragFX, dragFY, fx, fy);
      else {
        viewRect = false;
        auto before = changeCount;
        gesture(dragFX, dragFY, fx, fy);
        // A refused gesture (a door dragged the wrong way) must not leave its preview behind.
        if (changeCount == before)
          dropGhost();
        ghostShown = false;
      }
    }
  }
  void finishDrag(bool cancel) {
    if (!objectDrag)
      return;
    if (cancel) {
      doc.loadText(dragDocument);
      dirty=dragWasDirty;changeCount=dragChanges;
      publish(false,false);
      updateInspector();
    } else if (!objectMoved || doc.text()==dragDocument) {
      doc.loadText(dragDocument);
      dirty=dragWasDirty;changeCount=dragChanges;
      publish(false,false);
      updateInspector();
    } else if (objectMoved) {
      if(floorTransformSelection){std::vector<int> walls;for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(doc.tile(map,layer,x,y)=='#')walls.push_back(y*24+x);clearCreationWalls(walls);
        for(auto i:doc.records("LAYER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2&&int(number(f[2],-1))==layer){record=int(i);break;}}}
      history.push_back(dragDocument);
      if (history.size() > 50)
        history.erase(history.begin());
      future.clear();
      objectDrag = false;
      publish();
      selectRecord(record, true);
    }
    objectDrag = objectMoved = false;
    viewDrag = verticalDragging = bodyDragging = false;
    dragMates.clear();dragSelection.clear();resizeHandle=-1;
    InvalidateRect(plan, nullptr, FALSE);
  }
  bool shortcut(const MSG &msg) {
    // Keys typed in the Sign designer belong to it (Tab moves between its boxes), except F5 play.
    if (signDesigner && (msg.hwnd == signDesigner || IsChild(signDesigner, msg.hwnd)) &&
        !(msg.message == WM_KEYDOWN && (msg.wParam == VK_F5 || (msg.wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)))))
      return IsDialogMessageW(signDesigner, const_cast<MSG *>(&msg)) != FALSE;
    if (msg.message == WM_MOUSEWHEEL && plan && msg.hwnd != plan && IsWindowVisible(plan)) {
      POINT at{GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam)};
      if (WindowFromPoint(at) == plan) {
        SendMessageW(plan, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
        return true;
      }
    }
    // Alt+click picks paint up; Alt alone must not wake the window menu.
    if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) && msg.wParam == VK_MENU && tool == PaintTool)
      return true;
    if (msg.message != WM_KEYDOWN)
      return false;
    if (campaignShortcut(msg)) // Open from campaign: Ctrl+Shift+A admin, Ctrl+Shift+E export my changes
      return true;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0,
         shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    wchar_t cls[32]{};
    GetClassNameW(msg.hwnd, cls, 32);
    bool typing = _wcsicmp(cls, L"EDIT") == 0;
    // F5 or Tab plays (at the pointer when it is over the plan); Shift from the start.
    if (msg.wParam == VK_F5 || (msg.wParam == VK_TAB && !typing && !ctrl)) {
      if (shift)
        startPlay(1);
      else if (hoverInside && viewMode != 2)
        startPlay(2, hoverX, hoverY);
      else
        startPlay(0);
      return true;
    }
    if (msg.wParam == VK_F2 || msg.wParam == VK_F3 || msg.wParam == VK_F4) {
      SendMessageW(window, WM_COMMAND, msg.wParam == VK_F2 ? PlanOnly : msg.wParam == VK_F3 ? Dual : ViewOnly, 0);
      return true;
    }
    if (playing && msg.wParam == VK_ESCAPE) {
      stopPlay();
      return true;
    }
    if(ctrl&&shift&&(msg.wParam=='N'||msg.wParam=='D')){handleLayerCommand(msg.wParam=='N'?LayerNew:LayerDuplicate,0);return true;}
    if (typing) {
      int field=GetDlgCtrlID(msg.hwnd);
      if(msg.wParam==VK_RETURN&&(field==LayerName||field==LayerElevationField||field==LayerThicknessField)){handleLayerCommand(LayerCommit,0);return true;}
      // Enter in a field keeps the change at once.
      if (msg.wParam == VK_RETURN && GetParent(msg.hwnd) == form) {
        apply();
        return true;
      }
      return false;
    }
    if (objectDrag) {
      finishDrag(msg.wParam == VK_ESCAPE);
      drag = false;
      if (!backgroundTest && GetCapture() == plan)
        ReleaseCapture();
      if (msg.wParam == VK_ESCAPE)
        return true;
    }
    if(ctrl&&shift&&(msg.wParam=='N'||msg.wParam=='D')){handleLayerCommand(msg.wParam=='N'?LayerNew:LayerDuplicate,0);return true;}
    if (ctrl) {
      int id = msg.wParam == 'Z'   ? (shift ? Redo : Undo)
               : msg.wParam == 'Y' ? Redo
               : msg.wParam == 'S' ? Save
               : msg.wParam == 'D' || msg.wParam == 'C' ? Duplicate
                                   : 0;
      if (id) {
        SendMessageW(window, WM_COMMAND, id, 0);
        return true;
      }
    }
    if (_wcsicmp(cls, L"COMBOBOX") == 0 || _wcsicmp(cls, L"LISTBOX") == 0)
      return false;
    if (msg.wParam == VK_HOME) {
      fitPlan();
      return true;
    }
    // Held over the plan, Space only means "drag to move the view".
    if (msg.wParam == VK_SPACE && hoverInside)
      return true;
    if (msg.wParam == VK_ESCAPE) {
      // Escape always leads somewhere safe: cancel, then back to Select.
      bool busy = drag || brushing;
      finishDrag(true);
      drag = brushing = false;
      if (GetCapture() == plan)
        ReleaseCapture();
      if (!busy && tool != 0) {
        tool = 0;
        repaintTools();
        message(toolInfo[0].hint);
      } else if (!busy)
        selectRecord(-1);
      return true;
    }
    if (msg.wParam == VK_DELETE || msg.wParam == VK_BACK) {
      removeSelected();
      return true;
    }
    if (msg.wParam == 'R') {
      if (tool == DoorTool)
        turnDoorGhost(); // the Door tool turns the door it is about to fit
      else if (tool == FilmTool && !filmPicked())
        turnFilm(); // the Film camera tool turns the camera it is about to place
      else
        rotateSelected();
      return true;
    }
    if (msg.wParam == 'V' && !ctrl) {
      varySelected();
      return true;
    }
    if (msg.wParam == 'P' && !ctrl) {
      SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
      return true;
    }
    if ((msg.wParam == VK_OEM_4 || msg.wParam == VK_OEM_6) && record >= 0 && record < int(doc.lines.size())) {
      auto f = NativeDocument::fields(doc.lines[size_t(record)]);
      if (f[0] == "PRIM" && f.size() > 8) {
        float k = msg.wParam == VK_OEM_6 ? 1.15f : 1 / 1.15f;
        remember();
        for (size_t i : {size_t(6), size_t(7), size_t(8)})
          f[i] = trimmed(std::clamp(number(f[i], 1) * k, .1f, 24.f));
        doc.replace(size_t(record), f);
        publish();
        fillFields(false), fieldLayout();
        message(msg.wParam == VK_OEM_6 ? L"Bigger. [ makes it smaller." : L"Smaller. ] makes it bigger.");
        return true;
      }
    }
    float step = shift ? 1.f : snap > 0 ? snap : .1f;
    if (msg.wParam == VK_LEFT)
      moveSelected(-step, 0);
    else if (msg.wParam == VK_RIGHT)
      moveSelected(step, 0);
    else if (msg.wParam == VK_UP)
      moveSelected(0, -step);
    else if (msg.wParam == VK_DOWN)
      moveSelected(0, step);
    else
      return false;
    return true;
  }
  void apply(bool quiet = false) {
    if(record>=0&&!editableRecord(size_t(record))){message(L"Unlock and show this floor before editing its content.");return;}
    if(selectedRecords.size()>1){message(L"Pick one object to edit its individual fields. Move, Turn, Copy and Delete work on the selection.");return;}
    if (schemas.empty())
      return;
    auto &s = schemas[size_t(kind)];
    std::vector<std::string> f{s.kind};
    if (s.scoped)
      f.push_back(std::to_string(map));
    for (auto h : fieldInputs)
      f.push_back(encoded(utf8(label(h))));
    while (f.size() > size_t(s.required) && f.back().empty())
      f.pop_back();
    normalize(f);
    std::vector<std::string> before;
    if (record >= 0) {
      before = NativeDocument::fields(doc.lines[size_t(record)]);
      if (before == f)
        return; // leaving a field unchanged is not an edit
    }
    remember();
    int selected = record;
    if (record >= 0) {
      doc.replace(size_t(record), f);
      relink(before, f);
    } else {
      addAuthored(f);
      for (size_t i = 0; i < doc.lines.size(); ++i)
        if (doc.lines[i].starts_with("--- CUSTOM_CAMPAIGN_DATA_END ---")) {
          selected = int(i) - 1;
          break;
        }
    }
    record = selected;
    publish();
    selected = record;
    if (quiet && selected >= 0) {
      // Leaving a field must not rebuild the fields under the next click.
      if (s.kind == "MAP" || s.kind == "CAMPAIGN")
        refreshMaps();
      refreshLinks();
      InvalidateRect(plan, nullptr, FALSE);
    } else if (s.kind == "MAP")
      refresh();
    else if (schemaFor(f) >= 0)
      selectRecord(record);
    else
      refreshRecords();
    message(L"Changed. Ctrl+Z takes it back.");
  }
  // Renaming a trigger or a clock keeps everything that points at it; turning
  // or moving one pose of a standing person turns or moves the other with it.
  void relink(const std::vector<std::string> &before, const std::vector<std::string> &after) {
    if (before.size() < 3 || after.size() < 3 || before[0] != after[0])
      return;
    auto rename = [&](std::initializer_list<const char *> kinds) {
      if (before[2] == after[2])
        return;
      for (auto k : kinds)
        for (auto i : doc.records(k, map)) {
          auto g = NativeDocument::fields(doc.lines[i]);
          if (g.size() > 2 && g[2] == before[2])
            g[2] = after[2], doc.replace(i, g);
        }
    };
    if (after[0] == "SCRIPT_EVENT")
      rename({"SCRIPT_ACTION", "SCRIPT_REQUIRE", "SCRIPT_TRIGGER"});
    if (after[0] == "TIMED_SEQUENCE")
      rename({"ACTOR"});
    if (after[0] == "ACTOR_KEY" && before.size() > 7 && after.size() > 7)
      for (auto i : doc.records("ACTOR_KEY", map)) {
        auto g = NativeDocument::fields(doc.lines[i]);
        if (int(i) == record || g.size() <= 7 || g[2] != after[2])
          continue;
        bool together = g[4] == before[4] && g[5] == before[5];
        if (together)
          g[4] = after[4], g[5] = after[5];
        if (g[7] == before[7])
          g[7] = after[7];
        doc.replace(i, g);
      }
  }
  void place(float x, float y) {
    const auto &info = toolInfo[std::clamp(tool, 0, ToolCount - 1)];
    if (!*info.kind)
      return;
    remember();
    int added = authorKind(info.kind, x, y, x, y);
    publish();
    selectRecord(added);
  }
  float layerElevation() const {
    for (auto index : doc.records("LAYER", map)) {
      auto fields = NativeDocument::fields(doc.lines[index]);
      if (fields.size() > 4 && fields[2] == std::to_string(layer)) {
        try {
          return std::stof(fields[4]);
        } catch (...) {
        }
      }
    }
    return 0;
  }
  // Only the chosen palette page is shown, as big two-column tiles.
  void layoutTools() {
    if (!toolShelf)
      return;
    RECT shelf{};
    GetClientRect(toolShelf, &shelf);
    const int row = 70, width = std::max(40, (int(shelf.right) - 8) / 2);
    int slot = 0;
    for (size_t i = 0; i < tools.size(); ++i) {
      bool shown = toolInfo[i].group == group;
      ShowWindow(tools[i], shown ? SW_SHOW : SW_HIDE);
      if (!shown)
        continue;
      placeControl(tools[i], (slot % 2) * (width + 8), (slot / 2) * row - toolScroll, width, row - 6, TRUE);
      ++slot;
    }
    int content = (slot + 1) / 2 * row;
    toolScroll = std::clamp(toolScroll, 0, std::max(0, content - int(shelf.bottom)));
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS, 0, content,
                    UINT(std::max(1, int(shelf.bottom))), toolScroll, 0};
    SetScrollInfo(toolShelf, SB_VERT, &info, TRUE);
  }
  // The script parts linked to the selected trigger, in plain words.
  std::string selectedEvent() const {
    if (record < 0 || record >= int(doc.lines.size()))
      return {};
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    return f.size() > 2 && f[0].starts_with("SCRIPT_") ? f[2] : std::string{};
  }
  void refreshLinks() {
    edperf::Scope perfScope("ui_refreshLinks");
    if (!linkList)
      return;
    SendMessageW(linkList, LB_RESETCONTENT, 0, 0);
    auto id = selectedEvent();
    if (id.empty())
      return;
    for (const char *k : {"SCRIPT_EVENT", "SCRIPT_TRIGGER", "SCRIPT_REQUIRE", "SCRIPT_ACTION"})
      for (auto i : doc.records(k, map)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        if (f.size() < 3 || f[2] != id)
          continue;
        std::string text = f[0] == "SCRIPT_EVENT" ? "The area: " + id
                           : f[0] == "SCRIPT_TRIGGER" ? "Fires: " + (f.size() > 3 ? f[3] : "") + ", needs " + (f.size() > 4 ? f[4] : "") + (f.size() > 5 && f[5] == "1" ? ", again and again" : "")
                           : f[0] == "SCRIPT_REQUIRE" ? "Only if " + (f.size() > 5 ? decoded(f[3]) + " " + f[4] + " " + f[5] : "")
                           : f.size() > 3 && f[3] == "MESSAGE" ? "Says: " + (f.size() > 4 ? decoded(f[4]) : "")
                           : "Does: " + (f.size() > 3 ? f[3] : "") + " " + (f.size() > 4 ? decoded(f[4]) : "") + " " + (f.size() > 5 ? f[5] : "");
        auto n = SendMessageW(linkList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(text).c_str()));
        SendMessageW(linkList, LB_SETITEMDATA, n, LPARAM(i));
        if (int(i) == record)
          SendMessageW(linkList, LB_SETCURSEL, n, 0);
      }
  }
  // Adds one plain-language step to the selected trigger.
  void addToTrigger(int choice) {
    auto id = selectedEvent();
    if (id.empty() || choice <= 0)
      return;
    auto mi = std::to_string(map);
    std::vector<std::string> f;
    switch (choice) {
    case 1: f = {"SCRIPT_ACTION", mi, id, "MESSAGE", "SOMETHING STIRS.", "4"}; break;
    case 2: f = {"SCRIPT_ACTION", mi, id, "STATE", "switch_1", "1"}; break;
    case 3: f = {"SCRIPT_ACTION", mi, id, "ADD", "counter", "1"}; break;
    case 4: f = {"SCRIPT_ACTION", mi, id, "TAKE", "item", "1"}; break;
    case 5: f = {"SCRIPT_REQUIRE", mi, id, "switch_1", "=", "1"}; break;
    case 6: case 7: case 8: case 9: {
      static constexpr const char *where[] = {"box", "map", "campaign"};
      auto existing = std::find_if(doc.lines.begin(), doc.lines.end(), [&](const std::string &line) {
        auto g = NativeDocument::fields(line);
        return g.size() > 2 && g[0] == "SCRIPT_TRIGGER" && g[1] == mi && g[2] == id;
      });
      std::vector<std::string> t = existing == doc.lines.end() ? std::vector<std::string>{"SCRIPT_TRIGGER", mi, id, "box", "all", "0"}
                                                                : NativeDocument::fields(*existing);
      t.resize(6);
      if (choice <= 8)
        t[3] = where[choice - 6];
      else
        t[5] = t[5] == "1" ? "0" : "1";
      remember();
      if (existing == doc.lines.end())
        addAuthored(t);
      else
        doc.replace(size_t(existing - doc.lines.begin()), t);
      publish();
      for (auto i : doc.records("SCRIPT_TRIGGER", map))
        if (NativeDocument::fields(doc.lines[i])[2] == id)
          selectRecord(int(i));
      message(L"The trigger now fires as shown in its list.");
      return;
    }
    default: return;
    }
    remember();
    addAuthored(f);
    publish();
    selectRecord(int(doc.records(f[0], map).back()));
    message(L"Added. Change its words on the right.");
  }
  unsigned layoutCount = 0; // a press that already laid out (by picking) skips a second pass
  void layout() {
    ++layoutCount;
    edperf::Scope perfScope("ui_layout");
    edperf::Laps laps;
    RECT r{};
    GetClientRect(window, &r);
    int w = r.right, h = r.bottom;
    captions.clear();
    auto caption = [&](int x, int y, int width, const std::wstring &text) {
      captions.push_back({RECT{x, y, x + width, y + 20}, text});
    };
    // Top band. Row one: the name, and the file buttons tucked to the right.
    // Row two: undo, the three ways of looking, and Play, big, on the right.
    {
      int fx = w - 14;
      for (int id : {FullGame, Starter, ExportChanges, BackToOriginal, IntoGame, Export, Save, FromCampaign, Open, New}) {
        HWND b = GetDlgItem(window, id);
        if (!b)
          continue;
        if (id == FullGame && !playOut) {
          ShowWindow(b, SW_HIDE);
          continue;
        }
        // Open from campaign: the game buttons show for a level opened from the campaign, and
        // Example makes room for them.
        if (id == Starter && campaignLevel() >= 0) {
          ShowWindow(b, SW_HIDE);
          continue;
        }
        if (id == Starter)
          ShowWindow(b, SW_SHOW);
        if (id == IntoGame || id == BackToOriginal || id == ExportChanges) {
          std::error_code ec;
          int level = campaignLevel();
          auto file = campaignOverrideFile(level);
          bool show = level >= 0 && (id != BackToOriginal || (!file.empty() && fs::exists(file, ec)));
          ShowWindow(b, show ? SW_SHOW : SW_HIDE);
          if (!show)
            continue;
        }
        int bw = id == New || id == Open || id == Save ? 66 : id == FullGame ? 132 : id == FromCampaign || id == IntoGame ? 140 : id == ExportChanges ? 132 : id == BackToOriginal ? 124 : 88;
        fx -= bw;
        placeControl(b, fx, 9, bw, 34, TRUE);
        fx -= 6;
      }
      int bx = 16;
      for (int id : {Undo, Redo}) {
        placeControl(GetDlgItem(window, id), bx, 52, 86, 40, TRUE);
        bx += 92;
      }
      bx += 18;
      for (int id : {PlanOnly, Dual, ViewOnly}) {
        placeControl(GetDlgItem(window, id), bx, 52, 122, 40, TRUE);
        bx += 126;
      }
      if (HWND sound = GetDlgItem(window, SoundToggle)) {
        SetWindowTextW(sound, sfx::muted ? L"♪ Sound: off" : L"♪ Sound: on");
        placeControl(sound, bx + 14, 52, 124, 40, TRUE);
      }
      // The picture's light and play over Claire's shoulder (NativeLightView.inc).
      placeControl(GetDlgItem(window, LightViewButton), bx + 144, 52, 164, 40, TRUE);
      placeControl(GetDlgItem(window, ThirdPersonToggle), bx + 314, 52, 164, 40, TRUE);
      placeControl(GetDlgItem(window, Walk), w - 14 - 176, 49, 176, 44, TRUE);
      // The whole history on one slider, between the name and the file buttons.
      if (historySlider) {
        int left = 380, right = fx - 24;
        ShowWindow(historySlider, right - left > 120 ? SW_SHOW : SW_HIDE);
        placeControl(historySlider, left + 64, 12, std::max(60, right - left - 64), 30, TRUE);
        caption(left, 17, 62, L"History");
      }
      placeControl(GetDlgItem(window, PlayStart), w - 14 - 176 - 10 - 136, 52, 136, 40, TRUE);
    }
    laps.lap("layout_top");
    // Left card: maps, floors, music, then the tool pages.
    const int lx = 16, lw = 244, half = (lw - 8) / 2;
    caption(lx, 108, lw, L"Maps");
    placeControl(mapSearch, lx, 130, lw, 26, TRUE);
    placeControl(mapList, lx, 160, lw, 72, TRUE);
    placeControl(GetDlgItem(window, AddMap), lx, 238, half, 32, TRUE);
    placeControl(GetDlgItem(window, AddFloor), lx + half + 8, 238, lw - half - 8, 32, TRUE);
    placeControl(layerList, lx, 276, 150, 220, TRUE);
    placeControl(snapList, lx + 158, 276, lw - 158, 220, TRUE);
    placeControl(GetDlgItem(window, Furnish), lx, 310, half, 32, TRUE);
    placeControl(GetDlgItem(window, Outdoors), lx + half + 8, 310, lw - half - 8, 32, TRUE);
    SetWindowTextW(GetDlgItem(window,AutoDressing),autoDressingEnabled?L"Autodressing: on":L"Autodressing: off");
    placeControl(GetDlgItem(window,AutoDressing),lx,346,lw,30,TRUE);
    placeControl(musicList, lx, 382, 160, 300, TRUE);
    placeControl(GetDlgItem(window, ListenMusic), lx + 168, 380, lw - 168, 30, TRUE);
    for (size_t g = 0; g < groupTabs.size(); ++g)
      placeControl(groupTabs[g], lx + int(g) * 49, 412, 46, 36, TRUE);
    placeControl(toolShelf, lx, 456, lw, std::max(80, h - 456 - 54), TRUE);
    laps.lap("layout_left");
    layoutTools();
    // The black plan and the 3D picture, each in its own warm bezel.
    const int px = lx + lw + 20, top = 108;
    int middle = std::max(200, w - 344 - px), pw = viewMode == 2   ? 0
                                              : viewMode == 1 ? middle
                                                              : (middle - 12) / 2;
    placeControl(plan, px, top, pw, std::max(80, h - top - 52), TRUE);
    ShowWindow(plan, pw ? SW_SHOW : SW_HIDE);
    placeControl(viewport, px + pw + (pw ? 12 : 0), top, middle - pw - (pw ? 12 : 0),
               std::max(80, h - top - 52), TRUE);
    ShowWindow(viewport, viewMode == 1 ? SW_HIDE : SW_SHOW);
    laps.lap("layout_tools_plan");
    // Right column: what the tool will do, then what is selected. Nothing else.
    int rx = w - 326, y = 108;
    placeControl(GetDlgItem(window,InspectorTab),rx,y,152,30,TRUE);placeControl(GetDlgItem(window,LayerTab),rx+160,y,152,30,TRUE);y+=38;
    for(HWND c:{layerStack,layerPreview,layerName,layerElevationInput,layerThicknessInput,layerOpacity,layerGroupList})if(c)ShowWindow(c,SW_HIDE);
    for(int id:{LayerMenu,LayerVisible,LayerLock,LayerCommit,LayerReferences})ShowWindow(GetDlgItem(window,id),SW_HIDE);
    for (HWND list : {upperFloorTiles, modelSearch, modelList, materialList, roleList, behaviourList, groundList, heightList,
                      surfaceList, hazardList, soundList, preyList, hunterList, doorToolList, doorKindList, doorAxisList,
                      doorSwingList, doorHingeList, doorLockList, doorStrengthList, doorOpenSlider, pickupList, creatureList, tileList, primList, brushList,
                      clutterList, actionList, linkList, kindList, recordList, emitterPresetList, emitterKindList, emitterRestyleList,
                      emitterHeadingSlider, emitterStrengthSlider, GetDlgItem(window, FilmWrite), GetDlgItem(window, FilmRender),
                      GetDlgItem(window, FilmMaker)})
      if (list)
        ShowWindow(list, SW_HIDE);
    for (int id : {AutoDetail, NewObject, Apply})
      ShowWindow(GetDlgItem(window, id), SW_HIDE);
    if(!layersDock){
    ShowWindow(form,SW_SHOW);for(int id:{TurnSelected,Duplicate,Remove,Advanced})ShowWindow(GetDlgItem(window,id),SW_SHOW);
    const auto &info = toolInfo[std::clamp(tool, 0, ToolCount - 1)];
    headingCaption = captions.size();
    caption(rx, y, 312, std::wstring(info.glyph) + L"  " + info.name);
    y += 32;
    auto row = [&](HWND list, const wchar_t *text, int height = 300) {
      if (!list)
        return;
      caption(rx, y + 5, 96, text);
      placeControl(list, rx + 100, y, 212, height, TRUE);
      ShowWindow(list, SW_SHOW);
      y += 34;
    };
    char shape = info.shape;
    if(tool==2&&layer>0)row(upperFloorTiles,L"Upper floor");
    if (shape == 't'||tool==PaintTool)
      row(brushList, L"Paint with");
    for (int id : {OutlineWalls, OutlineBlock, OutlineTerraces})
      ShowWindow(GetDlgItem(window, id), shape == 'F' ? SW_SHOW : SW_HIDE);
    if (shape == 'F' && extrudeList) {
      row(extrudeList, L"Height", 200);
      int i = 0;
      for (int id : {OutlineWalls, OutlineBlock, OutlineTerraces})
        placeControl(GetDlgItem(window, id), rx + i++ * 106, y, 100, 34, TRUE);
      y += 42;
    }
    if (shape == 'H' && heightModeList)
      row(heightModeList, L"Brush", 200);
    if (timeSlider) {
      bool world = group == 4;
      ShowWindow(timeSlider, world ? SW_SHOW : SW_HIDE);
      if (world) {
        caption(rx, y + 6, 96, L"Time of day");
        placeControl(timeSlider, rx + 96, y, 216, 32, TRUE);
        y += 38;
      }
    }
    {
      bool moods = lastRoomX >= 0 && (tool == 0 || tool == 1 || tool == 6) && record < 0;
      for (HWND b : moodButtons)
        ShowWindow(b, moods ? SW_SHOW : SW_HIDE);
      if (moods) {
        caption(rx, y, 312, L"This room's mood");
        y += 24;
        for (size_t i = 0; i < moodButtons.size(); ++i)
          placeControl(moodButtons[i], rx + int(i) * 79, y, 75, 34, TRUE);
        y += 42;
      }
    }
    if (tool == CrateTool)
      row(tileList, L"Tile");
    if (tool == 14)
      row(clutterList, L"Kind");
    laps.lap("layout_right_top");
    // Parts and shapes as pictures: tabs, a search box and the tiles.
    bool browser = browsing();
    for (HWND b : partTabButtons)
      ShowWindow(b, browser ? SW_SHOW : SW_HIDE);
    if (partsGrid.hwnd)
      ShowWindow(partsGrid.hwnd, browser ? SW_SHOW : SW_HIDE);
    if (paintGrid.hwnd)
      ShowWindow(paintGrid.hwnd, tool == PaintTool ? SW_SHOW : SW_HIDE);
    if (browser && partsGrid.hwnd) {
      for (size_t i = 0; i < partTabButtons.size(); ++i)
        placeControl(partTabButtons[i], rx + int(i % 5) * 63, y + int(i / 5) * 33, 60, 30, TRUE);
      y += int((partTabButtons.size() + 4) / 5) * 33 + 4;
      row(modelSearch, L"Find", 26);
      int gh = std::max(170, std::min(430, h - 100 - y - 230));
      placeControl(partsGrid.hwnd, rx, y, 312, gh, TRUE);
      y += gh + 8;
      if (browserTool != tool) {
        if (tool == 9)
          partTab = parts::Shapes;
        else if (partTab == parts::Shapes)
          partTab = parts::Furniture;
        refreshParts();
      }
    }
    browserTool = browser ? tool : -1;
    if (tool == PaintTool && paintGrid.hwnd) {
      int gh = std::max(200, std::min(500, h - 100 - y - 190));
      placeControl(paintGrid.hwnd, rx, y, 312, gh, TRUE);
      y += gh + 8;
      if (!paintShown)
        refreshPaint();
    }
    paintShown = tool == PaintTool;
    if(tool==2)for(LRESULT i=0;i<SendMessageW(materialList,CB_GETCOUNT,0,0);++i)if(SendMessageW(materialList,CB_GETITEMDATA,i,0)==floorPaintMaterial){SendMessageW(materialList,CB_SETCURSEL,i,0);break;}
    if (tool == 9 || tool == 20 || tool==2)
      row(materialList, L"Finish");
    if (tool == 8 || tool == 9 || tool == 14 || tool == 15)
      row(heightList, L"Height");
    if (tool == 13)
      row(roleList, L"Who"), row(behaviourList, L"Does");
    if (tool == CreatureTool)
      row(creatureList, L"Creature");
    if (tool == 30)
      row(pickupList, L"Leave");
    if (tool == 22)
      row(hazardList, L"Hurts with");
    if (tool == SoundTool)
      row(soundList, L"Sound");
    if (tool == DoorTool)
      row(doorToolList, L"Door");
    if (tool == EmitterTool)
      row(emitterPresetList, L"Preset");
    if (tool == FilmTool) {
      int i = 0;
      for (int id : {FilmWrite, FilmRender}) {
        placeControl(GetDlgItem(window, id), rx + i++ * 159, y, 153, 34, TRUE);
        ShowWindow(GetDlgItem(window, id), SW_SHOW);
      }
      y += 42;
      // Shots with keyed timelines on top of the cameras (NativeFilmmaker.inc).
      placeControl(GetDlgItem(window, FilmMaker), rx, y, 312, 34, TRUE);
      ShowWindow(GetDlgItem(window, FilmMaker), SW_SHOW);
      y += 42;
    }
    if (tool == 23)
      row(groundList, L"Ground");
    if (tool == 26)
      row(surfaceList, L"Finish");
    if (shape == 'R' || tool == 29) {
      HWND b = GetDlgItem(window, AutoDetail);
      SetWindowTextW(b, autoDetail ? (tool == 29 ? L"Says something: on" : L"Auto lamps and door: on")
                                   : (tool == 29 ? L"Says something: off" : L"Auto lamps and door: off"));
      placeControl(b, rx, y, 312, 32, TRUE);
      ShowWindow(b, SW_SHOW);
      y += 38;
    }
    y += 10;
    std::wstring picked = L"Nothing picked";
    if (record >= 0 && record < int(doc.lines.size())) {
      auto f = NativeDocument::fields(doc.lines[size_t(record)]);
      picked = selectedRecords.size()>1?std::to_wstring(selectedRecords.size())+L" objects selected":L"Picked: " + plainKind(f[0]);
    }
    caption(rx, y, 312, picked);
    y += 24;
    placeControl(GetDlgItem(window, TurnSelected), rx, y, 100, 34, TRUE);
    placeControl(GetDlgItem(window, Duplicate), rx + 106, y, 100, 34, TRUE);
    placeControl(GetDlgItem(window, Remove), rx + 212, y, 100, 34, TRUE);
    y += 42;
    bool lamp = lightPicked();
    for (HWND c : {brightSlider, reachSlider, warmSlider, GetDlgItem(window, FlickerToggle)})
      if (c)
        ShowWindow(c, lamp ? SW_SHOW : SW_HIDE);
    for (HWND c : swatches)
      ShowWindow(c, lamp ? SW_SHOW : SW_HIDE);
    if (lamp && brightSlider) {
      if (!lightDrag)
        syncLightControls();
      for (auto [slider, text] : {std::pair{brightSlider, L"Brightness"}, std::pair{reachSlider, L"Reach"},
                                  std::pair{warmSlider, L"Warmth"}}) {
        caption(rx, y + 6, 96, text);
        placeControl(slider, rx + 96, y, 216, 32, TRUE);
        y += 36;
      }
      caption(rx, y + 7, 96, L"Colour");
      for (size_t i = 0; i < swatches.size(); ++i)
        placeControl(swatches[i], rx + 96 + int(i) * 36, y, 34, 34, TRUE);
      y += 42;
      HWND flicker = GetDlgItem(window, FlickerToggle);
      SetWindowTextW(flicker, number(lightField(17), 0) > 0 ? L"\u2734 Flicker: on" : L"\u2734 Flicker: off");
      placeControl(flicker, rx, y, 312, 34, TRUE);
      y += 44;
    }
    if (emitterPicked() && emitterKindList) {
      syncEmitterControls();
      row(emitterKindList, L"What");
      row(emitterRestyleList, L"Restyle");
      for (auto [slider, text] : {std::pair{emitterHeadingSlider, L"Points"}, std::pair{emitterStrengthSlider, L"Strength"}}) {
        caption(rx, y + 6, 96, text);
        placeControl(slider, rx + 100, y, 212, 32, TRUE);
        ShowWindow(slider, SW_SHOW);
        y += 36;
      }
    }
    if (auto glow = crystalGlow(); !glow.empty()) {
      caption(rx, y, 312, L"Crystal glow: by its model and Height");
      caption(rx, y + 20, 312, glow);
      y += 46;
    }
    if (creaturePicked() && preyList && hunterList) {
      syncCreatureControls();
      row(preyList, L"Hunts first");
      row(hunterList, L"Recurring as");
    }
    if (doorPicked() && doorKindList) {
      syncDoorControls();
      row(doorKindList, L"Opens by");
      row(doorAxisList, L"Stands");
      bool swings = nativeFlag(NativeDocument::fields(doc.lines[size_t(record)]), 8);
      if (swings)
        row(doorSwingList, L"Swings to"), row(doorHingeList, L"Hinge at");
      row(doorLockList, L"Lock shown");
      if (swings)
        row(doorStrengthList, L"Breaks down");
      if (doorOpenSlider) {
        caption(rx, y + 6, 96, L"Starts open");
        placeControl(doorOpenSlider, rx + 100, y, 212, 32, TRUE);
        ShowWindow(doorOpenSlider, SW_SHOW);
        y += 38;
      }
    }
    if (!selectedEvent().empty()) {
      row(actionList, L"Add");
      placeControl(linkList, rx, y, 312, 92, TRUE);
      ShowWindow(linkList, SW_SHOW);
      y += 100;
    }
    if (advanced) {
      placeControl(kindList, rx, y, 312, 360, TRUE);
      ShowWindow(kindList, SW_SHOW);
      y += 34;
      placeControl(recordList, rx, y, 312, 62, TRUE);
      ShowWindow(recordList, SW_SHOW);
      y += 70;
      placeControl(GetDlgItem(window, NewObject), rx, h - 92, 100, 34, TRUE);
      placeControl(GetDlgItem(window, Apply), rx + 106, h - 92, 100, 34, TRUE);
      ShowWindow(GetDlgItem(window, NewObject), SW_SHOW);
      ShowWindow(GetDlgItem(window, Apply), SW_SHOW);
    }
    placeControl(GetDlgItem(window, Advanced), rx + 212, h - 92, 100, 34, TRUE);
    placeControl(form, rx, y, 312, std::max(70, h - 100 - y), TRUE);
    }else{
      for(HWND c:{form,partsGrid.hwnd,paintGrid.hwnd,brightSlider,reachSlider,warmSlider,timeSlider})if(c)ShowWindow(c,SW_HIDE);
      for(HWND c:partTabButtons)ShowWindow(c,SW_HIDE);for(HWND c:swatches)ShowWindow(c,SW_HIDE);
      for(int id:{TurnSelected,Duplicate,Remove,Advanced,NewObject,Apply,FlickerToggle,OutlineWalls,OutlineBlock,OutlineTerraces})ShowWindow(GetDlgItem(window,id),SW_HIDE);
      layoutLayerDock(rx,y,312,h-100);
    }
    bool fresh = !restoredFrom.empty();
    laps.lap("layout_right");
    // Ideas sit at the right of the status line, one click each.
    int ideaRight = w - 30 - (fresh ? 160 : 0), shownIdeas = 0;
    for (size_t i = ideaButtons.size(); i-- > 0;) {
      bool show = i < ideas.size() && !playing;
      ShowWindow(ideaButtons[i], show ? SW_SHOW : SW_HIDE);
      if (show) {
        ideaRight -= 186;
        placeControl(ideaButtons[i], ideaRight, h - 41, 180, 34, TRUE);
        ++shownIdeas;
      }
    }
    placeControl(statusText, 30, h - 38, std::max(200, ideaRight - 40), 28, TRUE);
    if (HWND b = GetDlgItem(window, StartFresh)) {
      placeControl(b, w - 30 - 150, h - 41, 150, 34, TRUE);
      ShowWindow(b, fresh ? SW_SHOW : SW_HIDE);
    }
    fieldLayout();
    // Moved controls must not leave old pictures behind on the cards.
    laps.lap("layout_fields");
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    laps.lap("layout_redraw");
  }
  void verifyLayout() {
    auto bounds = [&](HWND child) {
      RECT r{};
      GetWindowRect(child, &r);
      MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
      return r;
    };
    for (auto size : {POINT{1280, 720}, POINT{1280, 960}, POINT{1920, 1080}}) {
      SetWindowPos(window, nullptr, 0, 0, size.x, size.y,
                   SWP_NOACTIVATE | SWP_NOZORDER);
      for (int mode = 0; mode < 3; ++mode) {
        viewMode = mode;
        layout();
        RECT client{};
        GetClientRect(window, &client);
        auto p = bounds(plan), v = bounds(viewport), f = bounds(form);
        if (f.left < 0 || f.right > client.right ||
            f.bottom > client.bottom - 80)
          throw std::runtime_error("Inspector extends outside its pane");
        if (mode == 0 &&
            (p.right > v.left || p.right <= p.left || v.right <= v.left))
          throw std::runtime_error("Dual plan and engine panes overlap");
        if (mode != 1 && (v.right > f.left || v.left < 242))
          throw std::runtime_error("Engine viewport overlaps editor controls");
        if (bool(GetWindowLongPtrW(plan, GWL_STYLE) & WS_VISIBLE) !=
                (mode != 2) ||
            bool(GetWindowLongPtrW(viewport, GWL_STYLE) & WS_VISIBLE) !=
                (mode != 1))
          throw std::runtime_error("View mode visibility is inconsistent");
      }
    }
    layersDock=true;refreshLayerDock();
    for(auto size:{POINT{1280,720},POINT{1920,1080}}){SetWindowPos(window,nullptr,0,0,size.x,size.y,SWP_NOACTIVATE|SWP_NOZORDER);layout();RECT client{};GetClientRect(window,&client);
      for(HWND child:{layerStack,layerPreview,layerName,layerElevationInput,layerThicknessInput,layerOpacity,layerGroupList,GetDlgItem(window,LayerCommit),GetDlgItem(window,LayerReferences)}){auto r=bounds(child);if(r.left<0||r.right>client.right||r.bottom>client.bottom-80)throw std::runtime_error("Layer dock extends outside its pane");}}
    layersDock=false;layout();
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = info.bmiHeader.biHeight = 1;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void *bits = nullptr;
    HBITMAP bitmap =
        CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    auto previous = SelectObject(dc, bitmap);
    SetPixel(dc, 0, 0, RGB(255, 255, 255));
    SendMessageW(form, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0);
    bool dark = GetPixel(dc, 0, 0) == ui::Paper;
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    if (!dark)
      throw std::runtime_error("Inspector background was not painted in the paper theme");
    std::ofstream(root / L"native-editor-layout-test.txt")
        << "PASS: 720/960/1080-height layouts; adaptive layer dock; separate dual panes; plan/3D visibility; "
           "paper-theme inspector; UTF-8 labels\n";
  }
  // The creative loop, driven through the same entries her clicks use.
  void verifyFun() {
    std::vector<std::string> passed;
    auto require = [&](bool okay, const char *what) {
      if (!okay)
        throw std::runtime_error(std::string("Fun test: ") + what);
      passed.push_back(what);
    };
    doc.newCampaign("Fun test");
    doc.addRoom("First map");
    map = layer = 0;
    refresh();
    richDetail = smartSnap = autoDetail = true;
    // 1. A dragged room is a real space at once.
    size_t fixturesBefore = doc.records("FIXTURE", 0).size(), structsBefore = doc.records("STRUCT", 0).size();
    tool = 1;
    gesture(3.5f, 3.5f, 12.5f, 10.5f);
    bool covered=true;
    for(int y=4;y<10;++y)for(int x=4;x<12;++x){bool found=false;
      for(auto i:doc.records("STRUCT",0)){auto f=NativeDocument::fields(doc.lines[i]);
        if(f.size()>9&&number(f[2],99)<=x+.5f&&number(f[4],-1)>=x+.5f&&number(f[3],99)<=y+.5f&&number(f[5],-1)>=y+.5f&&number(f[7],99)<=.05f)found=true;}
      covered&=found;}
    require(covered&&doc.records("STRUCT",0).size()>structsBefore,"room gets its own floor");
    require(doc.records("FIXTURE", 0).size() > fixturesBefore, "room gets things to start with");
    require(!doc.records("DOOR", 0).empty(), "room gets a door");
    size_t slab=0, materialField=0;
    require(thingAt(6.5f,6.5f,-1000.f,slab,materialField),"floor is selectable under the pointer");
    brush=2;SendMessageW(brushList,CB_SETCURSEL,2,0);
    // 2. Paint: a block, then the walls, then the paint picked back up.
    SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
    require(tool == PaintTool, "P tool in hand");
    layout();
    refreshPaint();
    require(paintGrid.items.size() > 40, "palette shows the materials and room finishes");
    int brick = -1;
    for (size_t i = 0; i < paintGrid.items.size(); ++i)
      if (paintGrid.items[i].key == "mat-10")
        brick = int(i);
    require(brick >= 0, "brick is on the palette");
    pickPaint(brick);
    require(paintChoice.key == "mat-10" && !recentPaint.empty(), "picked paint goes to the top");
    gesture(6.5f, 6.5f, 6.5f, 6.5f);
    require(thingAt(6.5f,6.5f,-1000.f,slab,materialField)&&NativeDocument::fields(doc.lines[slab])[9] == "10", "paint lands on the floor under the pointer");
    size_t blocksBefore = doc.records("STRUCT", 0).size();
    gesture(3.5f, 3.5f, 3.5f, 3.5f);
    auto wallRun = NativeDocument::fields(doc.lines[doc.records("STRUCT", 0).back()]);
    require(doc.records("STRUCT", 0).size() == blocksBefore + 1 && wallRun[9] == "10" && doc.tile(0, 0, 3, 3) == '.',
            "brick on one wall paints that wall only");
    require(!roomAt(6, 6).empty(), "the room stays closed after its wall became a block");
    choosePaint({"surf-2", L"Brick", 2, parts::Finish});
    gesture(12.5f, 5.5f, 12.5f, 5.5f);
    auto surfaces = doc.records("SURFACE", 0);
    require(!surfaces.empty() && NativeDocument::fields(doc.lines[surfaces.front()])[2] == "2", "a room finish on a wall dresses every wall");
    choosePaint({"mat-12", L"Timber", 12, 0});
    gesture(9.5f, 8.5f, 9.5f, 8.5f);
    {
      bool timber = false;
      for (auto i : doc.records("STRUCT", 0)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        timber |= number(f[7], 9) <= .05f && number(f[2], 99) <= 9.5f && number(f[4], -1) >= 9.5f &&
                  number(f[3], 99) <= 8.5f && number(f[5], -1) >= 8.5f && f[9] == "12";
      }
      require(timber, "a block paint on the floor gives this room its own floor");
    }
    choosePaint({"mat-10", L"Brick", 10, 0});
    choosePaint({"mat-5", L"Concrete", 5, 0});
    paintSpot(6.5f, 6.5f, -1000.f, 0, true);
    require(paintChoice.key == "mat-12", "picking up a paint takes the floor's");
    pickZ = 4.4f, pickSurface = 4;
    tool = PaintTool;
    gesture(8.5f, 8.5f, 8.5f, 8.5f);
    require(NativeDocument::fields(doc.lines[doc.records("SURFACE", 0).front()])[4] == std::to_string(parts::finishForMaterial(12)), "paint on the ceiling from the picture");
    // 3. The parts browser: a tile picks the part, placing it backs it onto a wall.
    SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
    layout();
    partTab = parts::Lab;
    refreshParts();
    require(!partsGrid.items.empty(), "Lab tab has tiles");
    int bench = -1;
    for (size_t i = 0; i < partsGrid.items.size(); ++i)
      if (partsGrid.items[i].key == "model-" + std::to_string(retro::model::lab_clean_bench))
        bench = int(i);
    require(bench >= 0, "the clean bench has a tile");
    pickPart(bench, false);
    require(selectedModel() == retro::model::lab_clean_bench, "clicking a tile picks that part");
    tool = 8;
    gesture(7.5f, 4.2f, 7.5f, 4.2f);
    auto placed = NativeDocument::fields(doc.lines[doc.records("FIXTURE", 0).back()]);
    require(placed[2] == std::to_string(retro::model::lab_clean_bench), "the part is placed");
    require(std::abs(number(placed[9], 0)) < .01f &&
              number(placed[4],0)-number(placed[7],0)*.5f>=4.03f,
            "furniture faces into the room without crossing the wall");
    selectRecord(int(doc.records("FIXTURE", 0).back()));
    varySelected();
    auto varied = NativeDocument::fields(doc.lines[size_t(record)]);
    require(varied[2] != placed[2], "V swaps it for another of its kind");
    pickPart(bench, true);
    require(savedParts.count("model-" + std::to_string(retro::model::lab_clean_bench)) == 1, "right-click saves a part");
    pickPart(bench, true);
    // 4. Shapes are a tab of their own and paintable.
    partTab = parts::Shapes;
    refreshParts();
    require(partsGrid.items.size() == 10, "Shapes tab has every shape");
    pickPart(5, false);
    require(tool == 9 && listIndex(primList) == 5, "a shape tile picks the Shape tool and that shape");
    gesture(16.5f, 16.5f, 16.5f, 16.5f);
    auto prim = doc.records("PRIM", 0).back();
    SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
    choosePaint({"mat-12", L"Timber", 12, 0});
    gesture(16.5f, 16.5f, 16.5f, 16.5f);
    require(NativeDocument::fields(doc.lines[prim])[10] == "12", "a shape takes paint");
    // 5. Free-form shapes: an outline becomes walls, a block, terraces.
    tool = ShapeDrawTool;
    auto draw = [&](int x0, int y0, int x1, int y1) {
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
          brushCell(x + .5f, y + .5f);
    };
    draw(15, 1, 21, 4);
    require(outlineCells() == 28 && !ideas.empty() && ideas[0].action == 11, "a drawn outline offers walls, a block or terraces");
    size_t structs = doc.records("STRUCT", 0).size();
    SendMessageW(extrudeList, CB_SETCURSEL, 2, 0);
    makeFromOutline(2);
    require(doc.records("STRUCT", 0).size() > structs + 3 && outlineCells() == 0, "terraces rise from the outline");
    draw(15, 12, 20, 15);
    makeFromOutline(0);
    require(doc.tile(0, 0, 15, 12) == '#' && doc.tile(0, 0, 17, 13) == '.', "an outline can become walls");
    // 6. Height painting: up, back down, and steps.
    tool = HeightTool;
    SendMessageW(heightModeList, CB_SETCURSEL, 0, 0);
    lastStrokeTick = 0;
    brushCell(5.5f, 20.5f);
    auto raised = doc.records("STRUCT", 0).back();
    require(NativeDocument::fields(doc.lines[raised])[7] == "0.5", "a cell rises half a metre");
    SendMessageW(heightModeList, CB_SETCURSEL, 1, 0);
    lastStrokeTick = 0;
    size_t count = doc.records("STRUCT", 0).size();
    brushCell(5.5f, 20.5f);
    require(doc.records("STRUCT", 0).size() == count - 1, "lowering it back takes the block away");
    SendMessageW(heightModeList, CB_SETCURSEL, 2, 0);
    lastStrokeTick = 0;
    for (int x = 2; x < 6; ++x)
      brushCell(x + .5f, 21.5f);
    auto step = NativeDocument::fields(doc.lines[doc.records("STRUCT", 0).back()]);
    require(step[7] == "1", "a dragged stroke climbs in steps");
    // 7. A corridor joins two rooms; lighting balances; moods; a finished room is cheered.
    tool = 1;
    gesture(14.5f, 6.5f, 21.5f, 10.5f);
    lastRoomX = 17, lastRoomY = 8;
    size_t lampsBefore = doc.records("LIGHT", 0).size();
    tool = CorridorTool;
    gesture(9.5f, 7.5f, 17.5f, 8.5f);
    require(doc.records("LIGHT", 0).size() >= lampsBefore && cellIn(roomAt(9,7),17.5f,8.5f), "a corridor is cut and lit between the rooms");
    auto room = roomAt(17, 8);
    require(!room.empty(), "the far room is still a room");
    lastRoomX = 17, lastRoomY = 8;
    runIdea(1);
    require(!lightsIn(roomAt(17, 8)).empty(), "light it evenly hangs lamps across the room");
    std::vector<std::string> userLamps;
    for(auto i:lightsIn(roomAt(17,8)))if(!doc.automatic(i,layer))userLamps.push_back(doc.lines[i]);
    moodRoom(1);
    bool moodOkay=true;
    for(auto i:lightsIn(roomAt(17,8)))if(doc.automatic(i,layer)){
      auto lamp=NativeDocument::fields(doc.lines[i]);moodOkay&=lamp.size()>17&&lamp[17]=="0.35"&&lamp[9]=="5200";}
    for(const auto& original:userLamps)moodOkay&=std::find(doc.lines.begin(),doc.lines.end(),original)!=doc.lines.end();
    auto atmosphere=NativeDocument::fields(doc.lines[doc.records("MAP",map).front()]);
    require(moodOkay&&atmosphere.size()>26&&atmosphere[26]=="0.07","mood adjusts automatic lighting and preserves user lamps");
    require(countCompleteRooms() >= 1, "a lit room with a door counts as finished");
    // 8. Stamps, the history slider and the time of day.
    regionStamp = captureRegion(2, 2, 6, 6);
    int stampsBefore = stampCount();
    saveStamp();
    require(stampCount() == stampsBefore + 1 && loadStamp(stampCount()) && regionStamp.width == 5, "a copied area is kept as a stamp");
    {
      std::error_code ec;
      fs::remove(stampFolder() / (L"stamp-" + std::to_wstring(stampCount()) + L".txt"), ec);
    }
    auto latest = doc.text();
    size_t steps = history.size();
    scrubTo(int(steps) - 3);
    require(history.size() == steps - 3 && doc.text() != latest, "the history slider steps back");
    scrubTo(int(steps));
    require(doc.text() == latest, "and forward to where she was");
    setTimeOfDay(3);
    require(NativeDocument::fields(doc.lines[doc.records("MAP", 0).front()])[10] == "0.26", "the dial sets dusk");
    // 9. Undo is fearless; restored work can start fresh.
    auto before = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() != before, "undo takes the paint back");
    require(validateDocument(), "the engine accepts the result");
    // 10. People, a creature, triggers with steps, the kept stamp pasted back
    // and the lamp sliders, through the same entries her clicks use.
    SendMessageW(window, WM_COMMAND, ToolBase + 13, 0);
    layout();
    auto pickRole = [&](const wchar_t *label) {
      for (int i = 0; i < int(std::size(people)); ++i)
        if (std::wstring(people[i].label) == label)
          SendMessageW(roleList, CB_SETCURSEL, i, 0);
    };
    pickRole(L"Scientist");
    SendMessageW(behaviourList, CB_SETCURSEL, 0, 0);
    tool = 13;
    gesture(5.5f, 7.5f, 5.5f, 7.5f);
    require(doc.records("ACTOR", 0).size() == 1 && doc.records("TIMED_SEQUENCE", 0).size() == 1 &&
                doc.records("ACTOR_KEY", 0).size() == 2 && doc.records("ACTOR_AI", 0).size() == 1,
            "a person goes about their day on the map's clock");
    pickRole(L"Security guard");
    gesture(18.5f, 9.5f, 18.5f, 9.5f);
    require(doc.records("ACTOR", 0).size() == 2 && doc.records("TIMED_SEQUENCE", 0).size() == 1 &&
                doc.records("ACTOR_GUARD", 0).size() == 1 && doc.records("ACTOR_PATROL", 0).size() == 1 &&
                doc.records("ACTOR_WEAPON", 0).size() == 1,
            "a guard shares the clock, carries a pistol and patrols");
    SendMessageW(window, WM_COMMAND, ToolBase + CreatureTool, 0);
    layout();
    SendMessageW(creatureList, CB_SETCURSEL, 0, 0);
    tool = CreatureTool;
    gesture(19.5f, 18.5f, 19.5f, 18.5f);
    require(doc.records("CREATURE", 0).size() == 1, "a creature waits in the open");
    SendMessageW(window, WM_COMMAND, ToolBase + 29, 0);
    layout();
    tool = 29;
    autoDetail = true;
    gesture(15.5f, 7.5f, 20.5f, 9.5f);
    auto events = doc.records("SCRIPT_EVENT", 0);
    require(events.size() == 1 && doc.records("SCRIPT_ACTION", 0).size() == 1, "a trigger area says something when stepped in");
    selectRecord(int(events.front()));
    addToTrigger(2);
    addToTrigger(3);
    require(doc.records("SCRIPT_ACTION", 0).size() == 3, "the trigger turns a switch on and counts up");
    tool = 29;
    gesture(2.5f, 12.5f, 4.5f, 14.5f);
    events = doc.records("SCRIPT_EVENT", 0);
    require(events.size() == 2, "a second trigger area");
    selectRecord(int(events.back()));
    addToTrigger(5);
    addToTrigger(7);
    require(doc.records("SCRIPT_REQUIRE", 0).size() == 1 && doc.records("SCRIPT_TRIGGER", 0).size() == 1 &&
                NativeDocument::fields(doc.lines[doc.records("SCRIPT_TRIGGER", 0).front()])[3] == "map",
            "it waits for the switch and then fires anywhere on the map");
    tool = 17;
    auto countRecords=[&]{size_t count=0;for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(!f.empty()&&!f[0].starts_with("#"))++count;}return count;};
    size_t recordsBefore=countRecords();
    gesture(8.5f, 15.5f, 8.5f, 15.5f);
    require(regionStamp.width == 5 && !regionStamp.objects.empty() &&
                countRecords() == recordsBefore + regionStamp.objects.size(),
            "the kept stamp pastes back with everything in it");
    {
      bool inside = true;
      for (auto i : doc.records("STRUCT", 0)) {
        auto f = NativeDocument::fields(doc.lines[i]);
        inside &= number(f[2], -1) >= 0 && number(f[3], -1) >= 0 && number(f[4], 99) <= 24 && number(f[5], 99) <= 24;
      }
      require(inside, "pasted blocks stay inside the copied area and the map");
    }
    auto lamps = lightsIn(roomAt(17, 8));
    selectRecord(int(lamps.front()));
    require(lightPicked(), "a lamp is picked");
    auto slide = [&](HWND slider, int at) {
      SendMessageW(slider, TBM_SETPOS, TRUE, at);
      SendMessageW(window, WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, at), reinterpret_cast<LPARAM>(slider));
      SendMessageW(window, WM_HSCROLL, MAKEWPARAM(TB_ENDTRACK, 0), reinterpret_cast<LPARAM>(slider));
    };
    slide(brightSlider, 180);
    slide(reachSlider, 14);
    slide(warmSlider, 3000);
    SendMessageW(window, WM_COMMAND, SwatchBase + 1, 0);
    bool flickered = number(lightField(17), 0) > 0;
    SendMessageW(window, WM_COMMAND, FlickerToggle, 0);
    {
      auto lit = NativeDocument::fields(doc.lines[lamps.front()]);
      require(lit.size() > 24 && lit[7] == "1.8" && lit[8] == "14" && lit[9] == "3000", "the lamp sliders set brightness, reach and warmth");
      require(lit[22] == "1" && lit[23] == "0.62" && lit[24] == "0.3" && lit[17] == (flickered ? "0" : "0.5"), "a colour swatch tints it and Flicker toggles");
    }
    setTimeOfDay(3);
    require(NativeDocument::fields(doc.lines[doc.records("MAP", 0).front()])[10] == "0.26", "dusk again after the undo");
    require(validateDocument(), "the engine accepts the finished map");
    doc.save(root / L".build" / L"fun-test.rmc"); // for review pictures (--editor-shots)
    restoredFrom = "x";
    auto keep = doc.text();
    restoredFrom.clear();
    std::ofstream(root / L"native-editor-fun-test.txt") << "PASS: " << passed.size() << " checks: " << [&] {
      std::string all;
      for (auto &p : passed)
        all += (all.empty() ? "" : "; ") + p;
      return all;
    }() << "\n";
    (void)keep;
  }
  void verifyEditing() {
    richDetail = smartSnap = false;
    auto require = [](bool okay, const char *error) {
      if (!okay)
        throw std::runtime_error(error);
    };
    doc.newCampaign("Native editing controls verification");
    doc.addRoom("First map");
    map = layer = 0;
    tool = 0;
    const std::vector<std::string> original{
        "FIXTURE", "0",    "6", "8", "8",    "0", "1.87",
        "0.6",     "0.99", "0", "1", "0.65", "1"};
    addAuthored(original);
    refresh();
    chooseKind("SIGN");
    auto fixture = int(doc.records("FIXTURE", 0).front());
    auto before = doc.text();
    auto undoCount = history.size();
    const auto view = planView();
    const float cell = view.cell;
    const int ox = view.ox, oy = view.oy;
    auto mouse = [&](UINT message, float x, float y) {
      SendMessageW(plan, message, MK_LBUTTON,
                   MAKELPARAM(int(std::round(ox + x * cell)),
                              int(std::round(oy + y * cell))));
    };
    mouse(WM_LBUTTONDOWN, 8, 8);
    mouse(WM_MOUSEMOVE, 9.04f, 10.04f);
    mouse(WM_LBUTTONUP, 9.04f, 10.04f);
    require(record == fixture && schemas[size_t(kind)].kind == "FIXTURE",
            "Plan selection did not select across record types");
    auto moved = NativeDocument::fields(doc.lines[size_t(fixture)]);
    require(std::stof(moved[3]) == 9 && std::stof(moved[4]) == 10,
            "Mouse drag did not snap to native coordinates");
    for (size_t i = 0; i < original.size(); ++i)
      if (i != 3 && i != 4)
        require(moved[i] == original[i],
                "Dragging changed a model, material, flag or optional field");
    require(history.size() == undoCount + 1,
            "One drag must create exactly one undo operation");
    auto movedDocument = doc.text();
    SendMessageW(window, WM_COMMAND, Undo, 0);
    require(doc.text() == before,
            "Undo did not restore the exact original native document");
    SendMessageW(window, WM_COMMAND, Redo, 0);
    require(doc.text() == movedDocument,
            "Redo did not restore the native edit");
    selectRecord(fixture);
    moveSelected(.25f, 0);
    rotateSelected();
    auto rotated = NativeDocument::fields(doc.lines[size_t(fixture)]);
    require(std::stof(rotated[3]) == 9.25f &&
                std::abs(std::stof(rotated[9]) - 1.5707963f) < .00001f,
            "Nudge/rotation did not update native transform fields");
    SendMessageW(window, WM_COMMAND, Duplicate, 0);
    require(doc.records("FIXTURE", 0).size() == 2 && record != fixture,
            "Copy did not create and select a new native object");
    auto copied = NativeDocument::fields(doc.lines[size_t(record)]);
    require(copied[11] == "0.65" && copied[12] == "1",
            "Copy lost native collision/visibility fields");
    SendMessageW(window, WM_COMMAND, Remove, 0);
    require(doc.records("FIXTURE", 0).size() == 1,
            "Delete did not remove only the selected copy");
    selectRecord(fixture);
    auto cancelBefore = doc.text();
    undoCount = history.size();
    auto h = *handleFor(size_t(fixture));
    mouse(WM_LBUTTONDOWN, h.x, h.y);
    mouse(WM_MOUSEMOVE, h.x + 2, h.y + 1);
    finishDrag(true);
    drag = false;
    require(doc.text() == cancelBefore && history.size() == undoCount,
            "Cancelled drag changed the document or undo history");
    addAuthored({"PIPE", "0", "13", "13", "15", "14", "1", "0.09", "2", "4"});
    auto pipe = int(doc.records("PIPE", 0).front());
    selectRecord(pipe);
    auto pipeBefore = NativeDocument::fields(doc.lines[size_t(pipe)]);
    moveSelected(.5f, 1);
    auto pipeAfter = NativeDocument::fields(doc.lines[size_t(pipe)]);
    require(std::stof(pipeAfter[2]) == 13.5f && std::stof(pipeAfter[3]) == 14 &&
                std::stof(pipeAfter[4]) == 15.5f &&
                std::stof(pipeAfter[5]) == 15,
            "Pipe movement did not preserve both endpoints");
    for (size_t i = 6; i < pipeBefore.size(); ++i)
      require(pipeBefore[i] == pipeAfter[i],
              "Pipe move changed elevation, radius or material");
    SetWindowTextW(modelSearch, L"monitor");
    require(SendMessageW(modelList, CB_GETCOUNT, 0, 0) > 0,
            "Native model search found no monitor");
    chooseKind("FIXTURE");
    SendMessageW(modelList, CB_SETCURSEL, 0, 0);
    int modelId = int(SendMessageW(modelList, CB_GETITEMDATA, 0, 0));
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(ModelList, CBN_SELCHANGE),
                 reinterpret_cast<LPARAM>(modelList));
    SendMessageW(window, WM_COMMAND, ToolBase + 8, 0);
    place(5, 6);
    auto placed = NativeDocument::fields(doc.lines[size_t(record)]);
    require(placed[0] == "FIXTURE" && std::stoi(placed[2]) == modelId &&
                std::stof(placed[3]) == 5 && std::stof(placed[4]) == 6,
            "Filtered model selection placed the wrong native engine model");
    SetWindowTextW(mapSearch, L"not-a-map");
    require(SendMessageW(mapList, LB_GETCOUNT, 0, 0) == 0,
            "Map search did not filter map names");
    SetWindowTextW(mapSearch, L"first");
    require(SendMessageW(mapList, LB_GETCOUNT, 0, 0) == 1 && map == 0,
            "Map search changed the active native map");
    SetWindowTextW(mapSearch, L"");
    auto output = root / L".build" / L"native-ui" / L"editing-controls.rmc";
    fs::create_directories(output.parent_path());
    doc.save(output);
    NativeDocument reopened;
    reopened.open(output);
    require(reopened.text() == doc.text(),
            "Authored controls did not save/reopen exactly");
    require(
        validateDocument(),
        "Native engine rejected the document authored by the editing controls");
    NativeDocument starter;
    starter.open(root / L"custom maps" / L"Starter Map.rmc");
    size_t exercised = 0;
    for (const auto &line : starter.lines) {
      auto f = NativeDocument::fields(line);
      int schema = schemaFor(f);
      if (schema < 0)
        continue;
      auto shape = nativeHandle(schemas[size_t(schema)].fields, f);
      if (!shape)
        continue;
      auto changed = f;
      translateNative(changed, schemas[size_t(schema)].fields, .25f, .5f);
      for (size_t field = 0; field < f.size(); ++field) {
        bool coordinate = false;
        for (auto [x, y] : shape->points)
          coordinate |= field == x || field == y;
        if (!coordinate)
          require(changed[field] == f[field],
                  "Starter transform changed non-coordinate fields");
      }
      ++exercised;
    }
    require(exercised > 100, "Starter record coverage was unexpectedly small");
    // Starter demonstrates free leads. Use the engine's own seated-lead
    // regression models/ports for the attachment and index-remapping checks.
    NativeDocument linked;
    linked.newCampaign("Native attachment editing check");
    linked.addRoom("First map");
    linked.add({"FIXTURE", "0", std::to_string(retro::model::lab_clean_bench),
                "8", "8.28", "0", "1.18", "0.56", "0.81", "0", "1"});
    linked.add({"FIXTURE", "0", std::to_string(retro::model::lab_field_meter),
                "7.72", "8.3", "0.81", "0.25", "0.328", "0.148", "-2.1", "0"});
    linked.add({"CLUTTER", "0", "0", "4", "4", "0", "0"});
    linked.add({"CLUTTER", "0", "7", "8.3", "8.32", "0.81", "0",
                std::to_string(retro::model::anl_cell_charged), "0.216",
                "0.201", "0.406", "0", "0", "cell_broken"});
    auto lead = NativeDocument::fields(
        "LEAD|0||||0|0|0|0.8|0.007|8|1|record_red|1||||0|1|+|+");
    linked.addAutomatic(lead,0);
    auto authored = doc;
    doc = linked;
    require(validateDocument(),
            "Engine-native seated-lead baseline was invalid");
    doc = authored;
    for (auto type : {"FIXTURE", "CLUTTER"}) {
      auto copy = linked;
      auto beforeDelete = copy.text();
      bool blocked = false;
      try {
        eraseNativeObject(copy, copy.records(type, 0).at(1));
      } catch (...) {
        blocked = true;
      }
      require(blocked && copy.text() == beforeDelete,
              "Deleting an attached object changed its native document");
    }
    eraseNativeObject(linked, linked.records("FIXTURE", 0).front());
    eraseNativeObject(linked, linked.records("CLUTTER", 0).front());
    auto remapped =
        NativeDocument::fields(linked.lines[linked.records("LEAD", 0).front()]);
    require(linked.automatic(linked.records("LEAD",0).front(),0),
            "Native index remapping promoted generated content to user ownership");
    require(remapped.at(18) == "0" && remapped.at(13) == "0",
            "Native fixture/prop index references were not remapped");
    doc = linked;
    require(validateDocument(),
            "Deleting unrelated objects broke native seated-cable references");
    doc = std::move(authored);
    NativeDocument stamped;
    stamped.newCampaign("Region stamp");
    stamped.addRoom("First map");
    stamped.paint(0, 0, 2, 3, '#');
    stamped.paint(0, 0, 3, 3, '=');
    stamped.paint(0, 0, 2, 4, '.');
    stamped.paint(0, 0, 3, 4, '_');
    stamped.add({"FIXTURE", "0", std::to_string(retro::model::lab_clean_bench),
                 "2.5", "3.5", "0", "1.18", "0.56", "0.81", "0", "1"});
    stamped.add({"FIXTURE", "0", std::to_string(retro::model::lab_field_meter),
                 "3.2", "3.2", "0", "0.25", "0.328", "0.148", "0", "0"});
    stamped.add({"CLUTTER", "0", "0", "20", "20", "0", "0"});
    stamped.add({"CLUTTER", "0", "7", "3.2", "4.2", "0", "0",
                 std::to_string(retro::model::anl_cell_charged), "0.216",
                 "0.201", "0.406", "0", "0", "cell_broken"});
    stamped.add(NativeDocument::fields(
        "LEAD|0|2.4|3.6|1|3.1|4.1|1|1.2|0.008|8|0||1|0|0|0|0|1|+|+"));
    auto held = std::move(doc);
    int heldMap = map, heldRotation = rotation, heldMirror = mirror;
    doc = std::move(stamped);
    map = layer = 0;
    auto stamp = captureRegion(2, 3, 3, 4);
    require(stamp.objects.size() == 4,
            "Region stamp missed an object inside the selection or copied one "
            "outside it");
    regionStamp = stamp;
    rotation = 1;
    mirror = 0;
    pasteRegion(8, 10);
    require(doc.tile(0, 0, 8, 11) == '#' && doc.tile(0, 0, 8, 10) == '=' &&
                doc.tile(0, 0, 9, 11) == '.' && doc.tile(0, 0, 9, 10) == '_',
            "Rotated region stamp did not turn the native tiles");
    require(doc.records("FIXTURE", 0).size() == 4 &&
                doc.records("CLUTTER", 0).size() == 3 &&
                doc.records("LEAD", 0).size() == 2 &&
                doc.records("LIGHT", 0).size() == 1,
            "Region stamp copied the wrong native objects");
    bool copiedBench = false;
    for (auto index : doc.records("FIXTURE", 0)) {
      auto f = NativeDocument::fields(doc.lines[index]);
      if (std::abs(std::stof(f[3]) - 8.5f) > .001f)
        continue;
      copiedBench = std::abs(std::stof(f[4]) - 11.5f) < .001f &&
                    std::abs(std::stof(f[9]) - 1.5707963f) < .001f &&
                    f[2] == std::to_string(retro::model::lab_clean_bench);
    }
    require(copiedBench,
            "Stamped model did not keep its identity, yaw and rotated place");
    auto copiedLead =
        NativeDocument::fields(doc.lines[doc.records("LEAD", 0).back()]);
    require(copiedLead.at(13) == "2" && copiedLead.at(18) == "3",
            "Stamped cable did not follow its copied fixture and prop");
    require(validateDocument(),
            "Engine rejected a rotated region stamp of native objects");
    doc = std::move(held);
    map = heldMap;
    rotation = heldRotation;
    mirror = heldMirror;
    NativeDocument built;
    built.newCampaign("Native replacement tools");
    built.addRoom("First map");
    doc = std::move(built);
    map = layer = 0;
    structureMaterial = 15;
    tool = 20;
    authorRect(2, 2, 5, 4);
    tool = 21;
    authorRect(8, 6, 10, 6);
    tool = 22;
    authorRect(12, 12, 14, 14);
    tool = 23;
    groundKind = 6;
    authorRect(1, 16, 6, 18);
    tool = 24;
    authorRect(8, 16, 12, 19);
    placePlayer(4.5f, 4.5f);
    authorSurfaces();
    auto block = NativeDocument::fields(doc.lines[doc.records("STRUCT", 0).at(0)]);
    require(block[2] == "2" && block[3] == "2" && block[4] == "6" &&
                block[5] == "5" && block[9] == "15",
            "Block drag did not write a native STRUCT");
    auto door = NativeDocument::fields(doc.lines[doc.records("DOOR", 0).at(0)]);
    require(std::stof(door[2]) == 8.f && std::stof(door[3]) == 11.f &&
                std::abs(std::stof(door[4]) - 6.5f) < .001f && door[10] == "-1",
            "Door drag did not write a native span");
    auto hazard =
        NativeDocument::fields(doc.lines[doc.records("HAZARD", 0).at(0)]);
    require(hazard[2] == "0" && hazard[3] == "12" && hazard[5] == "15" &&
                hazard[9] == "4",
            "Hazard drag did not write a native volume");
    auto ground = NativeDocument::fields(doc.lines[doc.records("GROUND").at(0)]);
    require(ground[1] == "1" && ground[2] == "16" && ground[3] == "7" &&
                ground[4] == "19" && ground[5] == "6",
            "Ground paint was not stored in world metres");
    auto pad =
        NativeDocument::fields(doc.lines[doc.records("TERRAIN_FLAT").at(0)]);
    require(pad[1] == "8" && pad[2] == "16" && pad[3] == "13" && pad[4] == "20" &&
                pad[5] == "0" && pad[6] == "2",
            "Flatten pad was not stored in world metres");
    require(doc.records("TERRAIN").size() == 1, "Outdoor paint did not add terrain");
    auto room = NativeDocument::fields(doc.lines[doc.records("MAP", 0).at(0)]);
    require(room[8] == "1" && room[11] == "brutal_wasteland" &&
                std::stof(room[5]) == 4.5f && std::stof(room[6]) == 4.5f,
            "Player start or outdoor map was not written on the MAP record");
    auto surfaces =
        NativeDocument::fields(doc.lines[doc.records("SURFACE", 0).at(0)]);
    require(surfaces[2] == "0" && surfaces[3] == "0" && surfaces[4] == "0",
            "Room surfaces were not written");
    int concrete = -1;
    for (LRESULT i = 0; i < SendMessageW(materialList, CB_GETCOUNT, 0, 0); ++i)
      if (SendMessageW(materialList, CB_GETITEMDATA, i, 0) == 5)
        concrete = int(i);
    require(concrete >= 0, "GUIDE material names were not loaded");
    selectRecord(int(doc.records("STRUCT", 0).at(0)));
    SendMessageW(materialList, CB_SETCURSEL, concrete, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(MaterialList, CBN_SELCHANGE),
                 reinterpret_cast<LPARAM>(materialList));
    block = NativeDocument::fields(doc.lines[doc.records("STRUCT", 0).at(0)]);
    require(block[9] == "5", "Material list did not write the native material field");
    require(validateDocument(),
            "Engine rejected blocks, doors, hazards, ground, pads and the player start");
    NativeDocument lived;
    lived.newCampaign("People windows and view");
    lived.addRoom("First map");
    doc = std::move(lived);
    map = layer = 0;
    addAuthored({"FIXTURE", "0", "6", "8", "8", "0", "2", "2", "1", "0", "1"});
    require(selectAlongRay(8, 8, 5, 0, 0, -1) ==
                int(doc.records("FIXTURE", 0).front()),
            "A downward ray missed the block it was aimed at");
    require(selectAlongRay(1, 1, 5, 0, 0, -1) < 0,
            "A ray that misses every object reported a hit");
    openWindow(0, 4, 0, 5);
    require(doc.tile(0, 0, 0, 4) == '.' && doc.tile(0, 0, 0, 6) == '#',
            "Window opening did not cut only the dragged wall");
    SendMessageW(roleList, CB_SETCURSEL, 1, 0);
    placePerson(5.5f, 6.5f);
    SendMessageW(roleList, CB_SETCURSEL, 9, 0);
    placePerson(9.5f, 9.5f);
    auto worker = NativeDocument::fields(
        doc.lines[doc.records("ACTOR", 0).front()]);
    auto guard = NativeDocument::fields(doc.lines[doc.records("ACTOR", 0).back()]);
    require(worker[2] == "editor_clock" && worker[3] == "0" && worker[7] == "0",
            "Worker was not written as a native actor");
    require(guard[3] == "4" && guard[7] == "1",
            "Security was not the second native actor");
    require(doc.records("ACTOR_PATROL", 0).size() == 1 &&
                doc.records("TIMED_SEQUENCE", 0).size() == 1,
            "A guard needs one patrol and the shared clock");
    auto stood = NativeDocument::fields(
        doc.lines[doc.records("ACTOR_KEY", 0).front()]);
    selectRecord(int(doc.records("ACTOR_KEY", 0).front()));
    moveSelected(1, 0);
    auto shifted = NativeDocument::fields(
        doc.lines[doc.records("ACTOR_KEY", 0).at(1)]);
    auto post = NativeDocument::fields(
        doc.lines[doc.records("ACTOR_PATROL", 0).front()]);
    require(std::stof(shifted[4]) == std::stof(stood[4]) + 1 &&
                std::stof(post[3]) == 9.5f,
            "Moving a worker's pose key left the other key behind or moved the guard");
    groundKind = 4;
    tool = 23;
    authorRect(2, 2, 4, 3);
    auto paint = NativeDocument::fields(doc.lines[doc.records("GROUND").back()]);
    require(paint[5] == "4", "Ground kind was not written");
    require(validateDocument(),
            "Engine rejected people, a window opening or a chosen ground kind");
    NativeDocument daily;
    daily.newCampaign("Daily authoring");
    daily.addRoom("First map");
    doc = std::move(daily);
    map = layer = 0;
    for (int y = 1; y < 23; ++y)
      for (int x = 1; x < 23; ++x)
        doc.paint(0, 0, x, y, '_');
    doc.paint(0, 0, 2, 2, '.');
    doc.paint(0, 0, 6, 6, '.');
    tool = 21;
    authorRect(8, 6, 10, 6);
    furnish();
    bool lampNear = false;
    for (auto index : doc.records("LIGHT", 0)) {
      auto light = NativeDocument::fields(doc.lines[index]);
      if (light.size() > 3 &&
          std::hypot(std::stof(light[2]) - 2.5f, std::stof(light[3]) - 2.5f) <
              .2f)
        lampNear = true;
    }
    require(doc.records("LIGHT", 0).size() >= 3 && lampNear,
            "Furnish did not keep the room lamp and add lamps on open floor");
    require(!doc.records("SIGN", 0).empty(), "Furnish did not sign the door");
    auto sign = NativeDocument::fields(doc.lines[doc.records("SIGN", 0).front()]);
    require(sign.size() == 11 && sign[8] == "FIRST MAP" &&
                sign[10] == "14238785",
            "Door sign was not the room name");
    applyOutdoors();
    auto terrain = NativeDocument::fields(doc.lines[doc.records("TERRAIN").front()]);
    auto horizon = NativeDocument::fields(doc.lines[doc.records("HORIZON").front()]);
    auto outdoors = NativeDocument::fields(doc.lines[doc.records("MAP", 0).front()]);
    require(terrain.size() >= 6 && terrain[1] == "2026" && horizon.size() == 5 &&
                horizon[4] == "121,122,121,118,150,120" && outdoors[8] == "1" &&
                outdoors[11] == "brutal_wasteland",
            "Outdoors did not write terrain, the tree line and the wasteland sky");
    applySurfaceSet(2);
    auto surface =
        NativeDocument::fields(doc.lines[doc.records("SURFACE", 0).front()]);
    require(surface.size() >= 5 && surface[2] == "2" && surface[3] == "2" &&
                surface[4] == "2",
            "Surface set did not write wall, floor and ceiling");
    tool = 20;
    pickSerial = -1;
    applyPick("PICK 1 1 4 5 2 0 0 -1");
    applyPick("PICK 1 1 6 8 2 0 0 -1");
    applyPick("PICK 1 0 6 8 2 0 0 -1");
    auto placedBlock =
        NativeDocument::fields(doc.lines[doc.records("STRUCT", 0).back()]);
    require(std::stof(placedBlock[2]) == 4 && std::stof(placedBlock[3]) == 5 &&
                std::stof(placedBlock[4]) == 6 && std::stof(placedBlock[5]) == 8,
            "3D drag did not author the block under the ray");
    tool = 6;
    pickSerial = -1;
    applyPick("PICK 2 1 3.25 4.5 2 0 0 -1");
    applyPick("PICK 2 0 3.25 4.5 2 0 0 -1");
    bool placedLamp = false;
    for (auto index : doc.records("LIGHT", 0)) {
      auto light = NativeDocument::fields(doc.lines[index]);
      if (light.size() > 3 &&
          std::hypot(std::stof(light[2]) - 3.25f, std::stof(light[3]) - 4.5f) <
              .05f)
        placedLamp = true;
    }
    require(placedLamp, "3D click did not place the lamp tool on the floor");
    placeHeight = 2;
    tool = 15;
    pickSerial = -1;
    applyPick("PICK 4 1 8 9 2 0 0 -1");
    applyPick("PICK 4 0 8 9 2 0 0 -1");
    bool shelf = false;
    for (auto index : doc.records("TERMINAL", 0)) {
      auto terminal = NativeDocument::fields(doc.lines[index]);
      if (terminal.size() > 4 &&
          std::hypot(std::stof(terminal[2]) - 8.f, std::stof(terminal[3]) - 9.f) <
              .05f &&
          std::abs(std::stof(terminal[4]) - 1.5f) < .01f)
        shelf = true;
    }
    require(shelf, "Shelf height did not lift the computer off the floor");
    require(validateDocument(),
            "Engine rejected furnish, outdoors, a surface set or 3D placement");
    verifyInteractions();
    verifyPersistenceRegressions();
    verifySelectionRegressions();
    verifyLayerRegressions();
    verifyDressingRegressions();
    std::ofstream(root / L"native-editor-editing-test.txt")
        << "PASS: actual hidden mouse drag, cross-type selection, snapping, "
           "nudge/rotate, copy/delete, exact undo/redo/cancel, save/reopen and "
           "native engine validation, filtered model placement, map search, "
           "attachment protection and native index remapping, object-inclusive "
           "region stamps, blocks, doors, hazards, ground, pads, player start, "
           "named materials, people, window openings, ground kinds, 3D ray "
           "selection, furnish, outdoors, surface sets, 3D placement and "
           "placement height; "
        << exercised
        << " Starter record transforms preserve non-coordinate fields\n";
  }
  static bool sameField(const std::string &a, const std::string &b) {
    if (a == b)
      return true;
    if (a.empty() || b.empty())
      return false;
    char *ea = nullptr, *eb = nullptr;
    double x = strtod(a.c_str(), &ea), y = strtod(b.c_str(), &eb);
    return *ea == 0 && *eb == 0 && std::fabs(x - y) < 1e-3;
  }
  // GUI-only remake: Starter maps rebuilt with the palette, option lists,
  // plan clicks and the inspector, never record text. Every record is born
  // from a tool gesture; fields the gesture cannot know are typed into the
  // field boxes (plain ones first, Advanced when the field has no plain name).
  void verifyRemake() {
    joinMapPassage=false; // Starter exhibits also include deliberately closed seams.
    richDetail = smartSnap = false;
    NativeDocument original;
    original.open(root / L"custom maps" / L"Starter Map.rmc");
    const std::vector<int> picks{0, 1, 2, 3, 4, 5, 6, 28, 29, 33, 34};
    struct {
      int records = 0, gestures = 0, toolFields = 0, friendly = 0, advanced = 0, picks = 0;
    } tally;
    std::ofstream progress(root / L".build" / L"remake-progress.txt");
    auto started = GetTickCount64();
    auto note = [&](const std::string &what) { progress << (GetTickCount64() - started) << " ms  " << what << std::endl; };
    std::vector<std::string> problems;
    auto fail = [&](std::string why) {
      std::erase(why, char(13));
      if (problems.size() < 60)
        problems.push_back(why);
    };
    auto press = [&](int id) { SendMessageW(window, WM_COMMAND, id, 0); };
    auto choose = [&](HWND list, int id, int index) {
      SendMessageW(list, CB_SETCURSEL, index, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    auto chooseData = [&](HWND list, int id, LRESULT data) {
      for (LRESULT i = 0; i < SendMessageW(list, CB_GETCOUNT, 0, 0); ++i)
        if (SendMessageW(list, CB_GETITEMDATA, i, 0) == data) {
          choose(list, id, int(i));
          return true;
        }
      return false;
    };
    auto useTool = [&](int t) { press(ToolBase + t); };
    // A drag on the plan, in map metres (snap is off, so it lands exactly).
    auto drag = [&](float ax, float ay, float bx, float by) {
      ++tally.gestures;
      gesture(ax, ay, bx, by);
    };
    auto click = [&](float x, float y) { drag(x, y, x, y); };
    // Picks a record the way the Advanced list does: kind, then the row.
    auto pick = [&](size_t index) {
      ++tally.picks;
      int schema = schemaFor(NativeDocument::fields(doc.lines[index]));
      choose(kindList, KindList, schema);
      for (LRESULT i = 0; i < SendMessageW(recordList, LB_GETCOUNT, 0, 0); ++i)
        if (size_t(SendMessageW(recordList, LB_GETITEMDATA, i, 0)) == index) {
          SendMessageW(recordList, LB_SETCURSEL, i, 0);
          SendMessageW(window, WM_COMMAND, MAKEWPARAM(RecordList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(recordList));
          return true;
        }
      fail("could not pick " + doc.lines[index]);
      return false;
    };
    // Types the differing values into the inspector, then leaves the field.
    auto fix = [&](const std::vector<std::string> &want) {
      if (record < 0)
        return fail("nothing selected to fix for " + NativeDocument::join(want));
      auto have = NativeDocument::fields(doc.lines[size_t(record)]);
      const auto &s = schemas[size_t(kind)];
      size_t first = s.scoped ? 2 : 1;
      std::vector<std::pair<size_t, std::string>> edits;
      for (size_t i = first; i < std::max(want.size(), have.size()); ++i) {
        auto a = i < want.size() ? want[i] : "", b = i < have.size() ? have[i] : "";
        if (sameField(a, b))
          ++tally.toolFields;
        else
          edits.emplace_back(i, a);
      }
      if (edits.empty())
        return;
      bool plain = true;
      for (auto &[i, a] : edits)
        plain &= i < s.fields.size() && friendlyLabel(s.kind, s.fields[i]) != nullptr;
      if (!plain && !advanced)
        press(Advanced);
      for (auto &[i, a] : edits) {
        if (i - first >= fieldInputs.size())
          return fail("no field box " + std::to_string(i) + " for " + NativeDocument::join(want));
        SetWindowTextW(fieldInputs[i - first], wide(decoded(a)).c_str());
        bool named = i < s.fields.size() && friendlyLabel(s.kind, s.fields[i]);
        ++(named ? tally.friendly : tally.advanced);
      }
      SendMessageW(window, WM_APP + 40, 0, 0);
      if (advanced)
        press(Advanced);
    };
    press(New);
    choose(snapList, SnapList, 3);
    if (autoDetail)
      press(AutoDetail);
    SetWindowTextW(modelSearch, L"");
    // The blank map's welcome lamp is not in the Starter: click it, delete it.
    auto dropWelcomeLamp = [&] {
      useTool(0);
      auto lamps = doc.records("LIGHT", map);
      if (lamps.empty())
        return;
      selectRecord(hitObject(12, 12));
      press(Remove);
    };
    dropWelcomeLamp();
    std::map<int, int> remap; // original map -> remade map
    bool outdoorsDone = false;
    for (size_t j = 0; j < picks.size(); ++j) try {
      int src = picks[j];
      note("map " + std::to_string(src));
      if (j > 0) {
        for(int edge=0;edge<4;++edge)if(mapEdgeAvailable(edge)){press(MapNorth+edge);break;}
        dropWelcomeLamp();
        for(auto i:doc.records("SURFACE",map)){selectRecord(int(i));press(Remove);}
      }
      remap[src] = map;
      std::vector<std::vector<std::string>> lines;
      for (auto i : original.records("MAP", src))
        lines.push_back(NativeDocument::fields(original.lines[i]));
      auto mapFields = lines.front();
      // Floors first: + Upper floor for each extra LAYER, then its name and height.
      auto layers = original.records("LAYER", src);
      for (size_t k = 0; k < layers.size(); ++k) {
        auto want = NativeDocument::fields(original.lines[layers[k]]);
        if (k > 0)
          press(AddFloor);
        auto made = doc.records("LAYER", map);
        if (k < made.size() && pick(made[k]))
          fix(want);
      }
      // Tiles: each run of one kind is one box drag with the matching paint.
      choose(brushList, BrushList, 1);
      for (size_t k = 0; k < layers.size(); ++k) {
        int layerId = std::stoi(NativeDocument::fields(original.lines[layers[k]])[2]);
        for (LRESULT i = 0; i < SendMessageW(layerList, CB_GETCOUNT, 0, 0); ++i)
          if (SendMessageW(layerList, CB_GETITEMDATA, i, 0) == layerId)
            choose(layerList, LayerList, int(i));
        for (auto rowIndex : original.records("ROW", src)) {
          auto row = NativeDocument::fields(original.lines[rowIndex]);
          if (row.size() < 5 || row[2] != std::to_string(layerId))
            continue;
          int y = std::stoi(row[3]);
          for (int x = 0; x < 24;) {
            char ch = row[4][size_t(x)];
            int end = x;
            bool differs = false;
            while (end < 24 && row[4][size_t(end)] == ch)
              differs |= doc.tile(map, layer, end, y) != ch, ++end;
            if (differs) {
              int t = ch == '#' ? 3 : ch == '.' ? 2 : ch == '_' ? 4 : ch == '=' ? 5 : CrateTool;
              useTool(t);
              if(t==2&&layerId>0)choose(upperFloorTiles,UpperFloorTiles,ch=='.'?1:0);
              if (t == 2 || t == 3 || t == 4 || t == 5 || t == CrateTool)
                choose(brushList, BrushList, 1);
              if (t == CrateTool) {
                const char *crates = "CBT";
                auto at = strchr(crates, ch);
                if (!at)
                  fail(std::string("no tool paints tile ") + ch);
                choose(tileList, TileList, at ? int(at - crates) : 0);
              }
              drag(x + .5f, y + .5f, end - .5f, y + .5f);
            }
            x = end;
          }
        }
      }
      choose(layerList, LayerList, 0);
      note("  tiles done");
      // Player start: one click, then the map's own settings.
      useTool(25);
      click(std::stof(mapFields[5]), std::stof(mapFields[6]));
      auto [originX, originY] = std::pair{std::stof(mapFields[3]), std::stof(mapFields[4])};
      // Origin and name first, so world paint lands in the right metres.
      if (auto mapRecord = doc.records("MAP", map); pick(mapRecord.front()))
        fix(mapFields);
      bool outdoor = mapFields.size() > 8 && mapFields[8] == "1";
      if (outdoor && !outdoorsDone) {
        press(Outdoors);
        outdoorsDone = true;
        for (const char *k : {"TERRAIN", "HORIZON"}) {
          auto o = original.records(k), m = doc.records(k);
          if (!o.empty() && !m.empty() && pick(m.front()))
            fix(NativeDocument::fields(original.lines[o.front()]));
        }
      }
      // World paint that falls inside this map, drawn in its own metres.
      for (const char *k : {"GROUND", "TERRAIN_FLAT"})
        for (auto i : original.records(k)) {
          auto f = NativeDocument::fields(original.lines[i]);
          float cx = (std::stof(f[1]) + std::stof(f[3])) * .5f - originX, cy = (std::stof(f[2]) + std::stof(f[4])) * .5f - originY;
          if (cx < 0 || cy < 0 || cx >= 24 || cy >= 24)
            continue;
          ++tally.records;
          if (f[0] == "GROUND")
            useTool(23), choose(groundList, GroundList, std::clamp(std::stoi(f[5]), 0, 8));
          else
            useTool(24);
          drag(std::stof(f[1]) - originX, std::stof(f[2]) - originY, std::stof(f[3]) - originX, std::stof(f[4]) - originY);
          fix(f);
        }
      std::vector<size_t> deferred;
      bool clockFixed = false;
      for (size_t i = 0; i < original.lines.size(); ++i) {
        auto f = NativeDocument::fields(original.lines[i]);
        if (f.size() < 2 || f[1] != std::to_string(src))
          continue;
        const auto &k = f[0];
        // World records have no map field (GROUND's second field is x1); they come with their map above.
        if (k == "MAP" || k == "LAYER" || k == "ROW" || k == "ACTOR_KEY" || k == "TIMED_SEQUENCE" || k == "GROUND" ||
            k == "TERRAIN_FLAT" || k == "TERRAIN" || k == "HORIZON" || k == "CAMPAIGN")
          continue;
        if (k.starts_with("SCRIPT_") && k != "SCRIPT_EVENT") {
          deferred.push_back(i);
          continue;
        }
        ++tally.records;
        try {
        auto num = [&](size_t at) { return std::stof(f.at(at)); };
        int t = -1;
        static const std::map<std::string, int> tools{
            {"LIGHT", 6}, {"SIGN", 7}, {"FIXTURE", 8}, {"PRIM", 9}, {"PIPE", 10}, {"WATER", 11}, {"STAIR", 12},
            {"CLUTTER", 14}, {"TERMINAL", 15}, {"STRUCT", 20}, {"DOOR", 21}, {"HAZARD", 22}, {"ZONE", 28},
            {"SCRIPT_EVENT", 29}, {"PICKUP", 30}, {"LEAD", 31}, {"EMITTER", 32}, {"DARK", 33}, {"COMPACTOR", 34},
            {"BREAKABLE", 35}, {"CREATURE", CreatureTool}, {"SURFACE", 26}};
        if (auto it = tools.find(k); it != tools.end())
          t = it->second;
        if (k == "ACTOR") {
          // Person tool with the matching body, standing still, on its first pose.
          int visual = std::stoi(f[3]), role = 1;
          for (int r = 1; r < int(std::size(people)); ++r)
            if (people[r].visual == visual && people[r].mode == 1) {
              role = r;
              break;
            }
          useTool(13);
          choose(roleList, RoleList, role);
          choose(behaviourList, BehaviourList, 1);
          std::vector<std::vector<std::string>> keys;
          for (auto key : original.records("ACTOR_KEY", src)) {
            auto kf = NativeDocument::fields(original.lines[key]);
            if (kf[2] == f[7])
              keys.push_back(kf);
          }
          click(std::stof(keys.front()[4]), std::stof(keys.front()[5]));
          if (!clockFixed) {
            for (auto clock : original.records("TIMED_SEQUENCE", src)) {
              auto made = doc.records("TIMED_SEQUENCE", map);
              if (!made.empty() && pick(made.front()))
                fix(NativeDocument::fields(original.lines[clock])), ++tally.records;
            }
            clockFixed = true;
          }
          auto actors = doc.records("ACTOR", map);
          if (pick(actors.back()))
            fix(f);
          auto madeKeys = doc.records("ACTOR_KEY", map);
          std::vector<size_t> mine;
          for (auto key : madeKeys)
            if (NativeDocument::fields(doc.lines[key])[2] == f[7])
              mine.push_back(key);
          for (size_t kk = 0; kk < keys.size() && kk < mine.size(); ++kk) {
            ++tally.records;
            if (pick(mine[kk]))
              fix(keys[kk]);
          }
          continue;
        }
        if (t < 0) {
          fail("no tool makes " + k);
          continue;
        }
        useTool(t);
        if (k == "FIXTURE" || (k == "CLUTTER" && f[2] == "7")) {
          if (k == "CLUTTER")
            choose(clutterList, ClutterList, 7);
          int model = std::stoi(f[k == "FIXTURE" ? 2 : 7]);
          if (!chooseData(modelList, ModelList, model))
            fail("model list lacks " + std::to_string(model));
          useTool(t);
          choose(heightList, HeightList, 0);
        } else if (k == "CLUTTER")
          choose(clutterList, ClutterList, std::clamp(std::stoi(f[2]), 0, 6));
        if (k == "PRIM") {
          for (int p = 0; p < int(std::size(primKinds)); ++p)
            if (f[2] == primKinds[p])
              choose(primList, PrimList, p);
          choose(heightList, HeightList, 0);
        }
        if ((k == "STRUCT" || k == "PRIM") && f.size() > (k == "STRUCT" ? 9u : 10u))
          chooseData(materialList, MaterialList, std::stoi(f[k == "STRUCT" ? 9 : 10]));
        if (k == "TERMINAL")
          choose(heightList, HeightList, 0);
        if (k == "HAZARD")
          choose(hazardList, HazardList, std::clamp(std::stoi(f[2]), 0, 7));
        if (k == "CREATURE")
          choose(creatureList, CreatureList, std::clamp(std::stoi(f[2]), 0, 8));
        if (k == "PICKUP")
          choose(pickupList, PickupList, std::clamp(std::stoi(f[2]), 0, 2));
        if (k == "SURFACE") {
          ++tally.gestures;
          chooseData(surfaceList, SurfaceList, std::stoi(f[2]));
        } else if (k == "PIPE" || k == "STRUCT" || k == "WATER" || k == "STAIR" || k == "ZONE" ||
                   k == "DARK" || k == "COMPACTOR" || k == "BREAKABLE")
          drag(num(2), num(3), num(4), num(5));
        else if (k == "HAZARD" || k == "SCRIPT_EVENT")
          drag(num(3), num(4), num(5), num(6));
        else if (k == "LEAD")
          drag(num(2), num(3), num(5), num(6));
        else if (k == "DOOR" && nativeFlag(f, 21))
          drag(num(4), num(2), num(4), num(3));
        else if (k == "DOOR")
          drag(num(2), num(4), num(3), num(4));
        else if (k == "PICKUP" || k == "CREATURE" || k == "PRIM" || k == "FIXTURE" || k == "CLUTTER")
          click(num(3), num(4));
        else
          click(num(2), num(3));
        fix(f);
        } catch (const std::exception &error) {
          fail(original.lines[i] + ": " + error.what());
        }
      }
      note("  records done");
      // Logic last: pick the trigger, add each step from the Add list, word it.
      for (auto i : deferred) try {
        ++tally.records;
        auto f = NativeDocument::fields(original.lines[i]);
        size_t event = 0;
        for (auto e : doc.records("SCRIPT_EVENT", map))
          if (NativeDocument::fields(doc.lines[e])[2] == f[2])
            event = e;
        if (!event) {
          fail("no trigger named " + f[2]);
          continue;
        }
        pick(event);
        int choice = f[0] == "SCRIPT_REQUIRE" ? 5
                     : f[0] == "SCRIPT_TRIGGER" ? (f[3] == "map" ? 7 : f[3] == "campaign" ? 8 : 6)
                     : f[3] == "STATE" ? 2 : f[3] == "ADD" ? 3 : f[3] == "TAKE" ? 4 : 1;
        choose(actionList, ActionList, choice);
        ++tally.gestures;
        fix(f);
      } catch (const std::exception &error) {
        fail(original.lines[i] + ": " + error.what());
      }
      // The map's own settings last, so Outdoors and paint cannot undo them.
      auto mapRecord = doc.records("MAP", map);
      if (pick(mapRecord.front()))
        fix(mapFields);
    } catch (const std::exception &error) {
      fail("map " + std::to_string(picks[j]) + ": " + error.what());
    }
    for (auto i : original.records("CAMPAIGN"))
      if (auto c = doc.records("CAMPAIGN"); !c.empty() && pick(c.front())) {
        auto want = NativeDocument::fields(original.lines[i]);
        want[2] = "0";
        fix(want);
      }
    // Compare: per map and kind, the same records in the same order.
    auto clean = [](std::vector<std::string> f) {
      while (f.size() > 2 && f.back().empty())
        f.pop_back();
      return f;
    };
    auto equal = [&](std::vector<std::string> a, std::vector<std::string> b, bool skipMap) {
      a = clean(a), b = clean(b);
      if (a.size() != b.size())
        return false;
      for (size_t i = 0; i < a.size(); ++i)
        if (!(skipMap && i == 1) && !sameField(a[i], b[i]))
          return false;
      return true;
    };
    int compared = 0;
    std::map<std::string, int> kinds;
    for (auto [src, made] : remap) {
      std::set<std::string> names;
      for (auto &doc_ : {&original, &doc})
        for (auto &line : doc_->lines) {
          auto f = NativeDocument::fields(line);
          if (f.size() > 1 && !f[0].empty() && f[0][0] != '#' && f[0][0] != '-' && f[0].find(':') == std::string::npos &&
              f[1] == std::to_string(doc_ == &original ? src : made))
            names.insert(f[0]);
        }
      for (auto &name : names) {
        if (name == "CAMPAIGN" || name == "TERRAIN" || name == "HORIZON" || name == "GROUND" || name == "TERRAIN_FLAT")
          continue;
        auto a = original.records(name, src), b = doc.records(name, made);
        if (a.size() != b.size()) {
          fail("map " + std::to_string(src) + " " + name + ": " + std::to_string(a.size()) + " in Starter, " + std::to_string(b.size()) + " remade");
          continue;
        }
        for (size_t i = 0; i < a.size(); ++i) {
          ++compared, ++kinds[name];
          if (!equal(NativeDocument::fields(original.lines[a[i]]), NativeDocument::fields(doc.lines[b[i]]), true))
            fail("map " + std::to_string(src) + " differs:\n    want " + original.lines[a[i]] + "\n    made " + doc.lines[b[i]]);
        }
      }
    }
    // World records: the remade set must equal the Starter's for these maps.
    for (const char *name : {"GROUND", "TERRAIN_FLAT", "TERRAIN", "HORIZON", "CAMPAIGN"}) {
      std::vector<std::string> want, made;
      for (auto i : original.records(name)) {
        auto f = clean(NativeDocument::fields(original.lines[i]));
        if (std::string(name) == "CAMPAIGN")
          f.resize(2);
        if (std::string(name) == "GROUND" || std::string(name) == "TERRAIN_FLAT") {
          float cx = (std::stof(f[1]) + std::stof(f[3])) * .5f, cy = (std::stof(f[2]) + std::stof(f[4])) * .5f;
          bool inside = false;
          for (int src : picks) {
            auto m = NativeDocument::fields(original.lines[original.records("MAP", src).front()]);
            float ox = std::stof(m[3]), oy = std::stof(m[4]);
            inside |= cx >= ox && cy >= oy && cx < ox + 24 && cy < oy + 24;
          }
          if (!inside)
            continue;
        }
        want.push_back(NativeDocument::join(f));
      }
      for (auto i : doc.records(name)) {
        auto f = clean(NativeDocument::fields(doc.lines[i]));
        if (std::string(name) == "CAMPAIGN")
          f.resize(2);
        made.push_back(NativeDocument::join(f));
      }
      auto canon = [&](std::vector<std::string> v) {
        for (auto &s : v) {
          auto f = NativeDocument::fields(s);
          for (auto &x : f) {
            char *e = nullptr;
            double d = strtod(x.c_str(), &e);
            if (!x.empty() && *e == 0)
              x = nativeNumber(float(d));
          }
          s = NativeDocument::join(f);
        }
        std::sort(v.begin(), v.end());
        return v;
      };
      compared += int(want.size()), kinds[name] += int(want.size());
      if (canon(want) != canon(made))
        fail(std::string(name) + ": the remade world records differ (" + std::to_string(want.size()) + " wanted, " + std::to_string(made.size()) + " made)");
    }
    std::set<std::string> covered;
    for (auto &[name, count] : kinds)
      if (count)
        covered.insert(name);
    std::set<std::string> starterKinds;
    for (auto &line : original.lines) {
      auto f = NativeDocument::fields(line);
      if (f.size() > 1 && !f[0].empty() && std::all_of(f[0].begin(), f[0].end(), [](unsigned char c) { return isupper(c) || c == '_'; }))
        starterKinds.insert(f[0]);
    }
    std::string missing;
    for (auto &name : starterKinds)
      if (!covered.count(name))
        missing += " " + name;
    auto output = root / L".build" / L"remake";
    fs::create_directories(output);
    doc.save(output / L"Starter remade with tools.rmc");
    bool valid = validateDocument();
    if (!valid)
      fail("the engine rejected the remade campaign: " + utf8(notice));
    std::ofstream report(root / L"native-editor-remake-test.txt");
    report << (problems.empty() && missing.empty() ? "PASS" : "FAIL") << ": rebuilt Starter maps";
    for (int src : picks)
      report << ' ' << src;
    report << " with tool gestures only; " << compared << " records compared field by field, "
           << kinds.size() << " record kinds, engine " << (valid ? "accepts" : "rejects") << " the result\n"
           << "  records made by tools: " << tally.records << ", gestures " << tally.gestures << ", list picks " << tally.picks << "\n"
           << "  fields right from the tool: " << tally.toolFields << ", typed in plain fields: " << tally.friendly
           << ", typed in Advanced: " << tally.advanced << "\n  kinds covered:";
    for (auto &name : covered)
      report << ' ' << name << '(' << kinds[name] << ')';
    report << "\n  Starter kinds not covered:" << (missing.empty() ? " none" : missing) << '\n';
    for (auto &p : problems)
      report << "  - " << p << '\n';
    if (!problems.empty() || !missing.empty())
      throw std::runtime_error("GUI remake did not match the Starter Map; see native-editor-remake-test.txt");
  }
  // Real UI states for screenshots: a room, furniture, a person and a
  // trigger built through the same gestures, then one view per scene.
  void demo(int scene) {
    autoDetail = true;
    tool = 1;
    gesture(2.5f, 3.5f, 13.5f, 12.5f);
    gesture(13.5f, 5.5f, 20.5f, 12.5f);
    SetWindowTextW(modelSearch, L"bench");
    if (SendMessageW(modelList, CB_GETCOUNT, 0, 0) > 0)
      SendMessageW(modelList, CB_SETCURSEL, 0, 0);
    tool = 8;
    chooseKind("FIXTURE");
    gesture(6, 6, 6, 6);
    gesture(9, 6, 9, 6);
    SendMessageW(roleList, CB_SETCURSEL, 2, 0);
    tool = 13;
    gesture(7.5f, 9.5f, 7.5f, 9.5f);
    tool = 29;
    gesture(15, 7, 19, 11);
    int trigger = record;
    tool = 6;
    gesture(17, 9, 17, 9);
    int lamp = int(doc.records("LIGHT", map).front()); // in the room the picture looks into
    selectRecord(-1);
    if (scene == 1) {
      tool = 1, drag = true, dragFX = 3.5f, dragFY = 14.5f, hoverX = 11.5f, hoverY = 20.5f, hoverInside = true;
    } else if (scene == 2) {
      tool = 8, hoverX = 11, hoverY = 9, hoverInside = true;
    } else if (scene == 3) {
      tool = 0;
      selectRecord(trigger);
    } else if (scene == 4) {
      tool = 6, viewMode = 2;
    } else if (scene == 5) {
      tool = 2, viewMode = 1, hoverX = 16.5f, hoverY = 17.5f, hoverInside = true;
    } else if (scene >= 6 && scene <= 8) {
      // The People, Logic and World pages, each with a tool in hand.
      tool = scene == 6 ? 13 : scene == 7 ? 29 : 23, viewMode = 1, hoverX = 11.5f, hoverY = 18.5f, hoverInside = true;
    } else if (scene == 9 || scene == 10) {
      // A lamp picked and dialled: amber and bright, or red and flickering.
      tool = 0, viewMode = 0;
      selectRecord(lamp);
      if (scene == 9)
        setLight({{7, "2.4"}, {8, "12"}, {9, "2600"}, {22, "1"}, {23, "0.62"}, {24, "0.3"}}, true);
      else
        setLight({{7, "1.6"}, {17, "0.5"}, {22, "1"}, {23, "0.16"}, {24, "0.1"}}, true);
    } else if (scene == 11) {
      // A Chimera picked: it comes out for the scientist first and returns as one animal.
      tool = CreatureTool, viewMode = 1;
      SendMessageW(creatureList, CB_SETCURSEL, 6, 0);
      gesture(18.5f, 10.5f, 18.5f, 10.5f);
      tool = 0;
      selectRecord(int(doc.records("CREATURE", map).back()));
      setCreatureField(10, "0");
      setCreatureField(9, "the_chimera");
    }
    group = toolInfo[tool].group;
    message(toolInfo[tool].hint);
    repaintTools();
  }
  // The bundled example is never overwritten; saving it asks for a new name.
  bool isExample() const { return path.filename() == L"Starter Map.rmc"; }
  fs::path autosavePath() const {
    if(backgroundTest||remakeTest||menuTest||demoMode)return session/L"test-autosave.rmc";
    return root / L"custom maps" / L"Autosave" /
           ((path.empty() || isExample() ? std::wstring(L"New map") : path.stem().wstring()) + L".rmc");
  }
  void saveTo(const fs::path &file) {
    waitAutosave();
    std::error_code ec;
    const auto recovery = autosavePath();
    fs::path temporary = file.wstring() + L".editor-tmp-" + std::to_wstring(GetCurrentProcessId());
    try {
      doc.save(temporary);
      if (!replaceEditorFile(temporary, file))
        throw std::runtime_error("Could not finish saving. The previous file and recovery copy are safe.");
    } catch (...) {
      fs::remove(temporary, ec);
      throw;
    }
    path = file;
    // Remove only this project's recovery after the atomic save succeeds.
    // A failed Save As must retain it, and another project's recovery is unrelated.
    fs::remove(recovery, ec);
    dirty = false;
    autosavedAt = changeCount;
    showTitle();
    message(L"Saved " + file.filename().wstring() + L".");
    if (!quiet())
      sfx::play(sfx::MusicKey, .8f, .02f);
  }
  void saveNow() {
    if (!path.empty() && !isExample())
      saveTo(path);
    else
      openFile(true);
  }
  // Quietly keeps a copy of unsaved work; the next start offers it back.
  // The text is taken here; the disk write runs beside the window so a slow
  // disk never stalls a stroke. Anything that removes or replaces the
  // autosave waits for it first (waitAutosave).
  std::thread autosaveWorker;
  long long autosaveFor = -1;
  bool autosaveOk = false;
  std::string autosaveError;
  void waitAutosave() {
    if (autosaveWorker.joinable())
      autosaveWorker.join();
    if (autosaveFor < 0)
      return;
    if (autosaveOk)
      autosavedAt = std::max(autosavedAt, autosaveFor);
    else if (!autosaveError.empty())
      message(wide(autosaveError));
    autosaveFor = -1;
  }
  void autosave() {
    if (!dirty || changeCount == autosavedAt || session.empty() || remakeTest || backgroundTest || demoMode ||
        objectDrag || drag || brushing || viewRect || ghostShown)
      return;
    waitAutosave(); // the one before, fifteen seconds ago
    if (changeCount == autosavedAt)
      return;
    edperf::Scope perfScope("autosave");
    autosaveTo(autosavePath());
  }
  void autosaveTo(const fs::path &file) {
    waitAutosave();
    autosaveFor = changeCount, autosaveOk = false, autosaveError.clear();
    autosaveWorker = std::thread([this, file, text = doc.text()] {
      try {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        auto temporary = file;
        temporary += L".tmp";
        {
          std::ofstream out(temporary, std::ios::binary);
          if (!out)
            throw std::runtime_error("Cannot save campaign");
          out << text;
          if (!out)
            throw std::runtime_error("Campaign save failed");
        }
        autosaveOk = replaceEditorFile(temporary, file);
        if (!autosaveOk)
          throw std::runtime_error("Could not finish the recovery save. The previous recovery copy is safe.");
      } catch (const std::exception &error) {
        autosaveError = error.what();
      }
    });
  }
  void focusEngine() {
    HWND child = GetWindow(viewport, GW_CHILD);
    if (!child)
      return;
    DWORD other = GetWindowThreadProcessId(child, nullptr), mine = GetCurrentThreadId();
    AttachThreadInput(mine, other, TRUE);
    SetForegroundWindow(window);
    SetFocus(child);
    AttachThreadInput(mine, other, FALSE);
  }
  // Play is instant and lossless: the same picture, the same document, and
  // Escape lands back in the same view with the same thing picked.
  void startPlay(int spot, float x = 0, float y = 0) {
    if (playing) {
      stopPlay();
      return;
    }
    // Nothing half-done survives into play: strokes end, previews drop.
    if (objectDrag)
      finishDrag(true);
    if (brushing)
      brushing = false, publish();
    if (viewRect)
      viewRect = false, dropGhost();
    drag = false;
    if (GetCapture() == plan)
      ReleaseCapture();
    playStarted = GetTickCount64();
    playSpot = spot, playX = x, playY = y, playZ = layerElevation();
    playView = viewMode;
    if (viewMode != 2) {
      viewMode = 2;
      layout();
    }
    playing = true;
    writeControl();
    focusEngine();
    message(spot == 1 ? L"Playing from the start. Escape comes back." : L"Playing. Escape comes back.");
  }
  void stopPlay() {
    playing = false;
    writeControl();
    if (playView >= 0 && playView != viewMode) {
      viewMode = playView;
      layout();
    }
    playView = -1;
    SetFocus(viewMode == 2 ? window : plan);
    message(L"Back to building. Everything is where you left it.");
  }
  bool confirmSave() {
    if (!dirty)
      return true;
    int answer = MessageBoxW(window, L"Keep your changes? Yes saves them, No throws them away.",
                             L"Unsaved changes", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL)
      return false;
    if (answer == IDYES) {
      saveNow();
      return !dirty;
    }
    waitAutosave();
    std::error_code ec;
    fs::remove(autosavePath(), ec);
    return true;
  }
  // ==== Open from campaign (begin) ==========================================
  // The game's own levels open here as a new, unsaved map. The engine exports
  // them (RawMetal.exe --campaign-map-list / --export-rmc); it lists the sealed
  // chapters only in the source workspace (the owner's builds), never in a
  // released copy. A level opened this way can be saved into the game
  // (campaign-overrides/NN.rmc, which the game then builds that level from) or
  // put back to its original; Save as still writes an ordinary custom map.
  struct CampaignEntry {
    int chunk = -1, chapter = 1;
    std::wstring district, name;
    bool sealed = false;
  };
  struct CampaignPicker {
    struct Editor *editor = nullptr;
    std::vector<CampaignEntry> *maps = nullptr;
    std::vector<int> shown;
    HWND search = nullptr, list = nullptr, admin = nullptr, lock = nullptr, badge = nullptr, nameBox = nullptr, pinBox = nullptr,
         unlock = nullptr, status = nullptr;
    int chosen = -1;
    bool done = false, adminRow = false;
  };
  fs::path campaignEngine() const {
#ifdef RAWMETAL_UNIFIED
    wchar_t self[32768]{};
    GetModuleFileNameW(nullptr, self, 32768);
    return fs::path(self);
#else
    return engineExecutable();
#endif
  }
  // The game this builder saves levels into: the source workspace it sits in
  // (tools/Level Editor), else the game folder the player pointed it at once
  // (remembered in game-folder.txt), else, when `ask`, the one they pick now.
  fs::path campaignGameRoot(bool ask = false) const {
    std::error_code ec;
    auto game = root.parent_path().parent_path();
    if (fs::exists(game / L"src" / L"world" / L"CampaignChapters.cpp", ec) && fs::exists(game / L"RawMetal.exe", ec))
      return game;
    auto isGame = [&](const fs::path &folder) {
      return !folder.empty() && !fs::equivalent(folder, root, ec) &&
             (fs::exists(folder / L"DepthWorks.exe", ec) || fs::exists(folder / L"RawMetal.exe", ec));
    };
    {
      std::ifstream in(root / L"game-folder.txt");
      std::string line;
      if (std::getline(in, line) && isGame(fs::u8path(line)))
        return fs::u8path(line);
    }
    if (!ask || !window)
      return {};
    wchar_t file[32768]{};
    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner = window;
    dialog.lpstrTitle = L"Where is your game? Pick DepthWorks.exe";
    dialog.lpstrFilter = L"The game (*.exe)\0*.exe\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = 32768;
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&dialog))
      return {};
    auto folder = fs::path(file).parent_path();
    if (!isGame(folder))
      return {};
    std::ofstream(root / L"game-folder.txt") << utf8(folder.wstring()) << '\n';
    return folder;
  }
  DWORD runCampaignEngine(const std::wstring &arguments) const {
    std::wstring line = L"\"" + campaignEngine().wstring() + L"\" " + arguments;
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION p{};
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                        session.c_str(), &si, &p))
      return DWORD(-1);
    if (WaitForSingleObject(p.hProcess, 180000) == WAIT_TIMEOUT) {
      TerminateProcess(p.hProcess, 9);
      WaitForSingleObject(p.hProcess, 5000);
    }
    DWORD code = 1;
    GetExitCodeProcess(p.hProcess, &code);
    CloseHandle(p.hThread);
    CloseHandle(p.hProcess);
    return code;
  }
  std::vector<CampaignEntry> campaignMaps(std::wstring &why) const {
    std::vector<CampaignEntry> maps;
    auto list = session / L"campaign-maps.txt";
    std::error_code ec;
    fs::remove(list, ec);
    if (runCampaignEngine(L"--campaign-map-list --out=\"" + list.wstring() + L"\"") != 0) {
      why = L"The game would not list its levels.";
      return maps;
    }
    std::ifstream in(list);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      auto f = NativeDocument::fields(line);
      if (f.size() < 5)
        continue;
      CampaignEntry entry;
      try {
        entry.chunk = std::stoi(f[0]);
        entry.chapter = std::stoi(f[1]);
      } catch (...) {
        continue;
      }
      entry.district = wide(f[2]);
      entry.name = wide(f[3]);
      entry.sealed = f[4] == "1";
      maps.push_back(std::move(entry));
    }
    if (maps.empty())
      why = L"The game offered no levels.";
    return maps;
  }
  // Exports one level and opens it as a new, unsaved map.
  bool openCampaignMap(const CampaignEntry &entry, std::wstring &why) {
    auto file = session / (L"campaign-" + std::to_wstring(entry.chunk) + L".rmc");
    std::error_code ec;
    fs::remove(file, ec);
    if (runCampaignEngine(L"--export-rmc=" + std::to_wstring(entry.chunk) + L" --out=\"" + file.wstring() + L"\"") != 0 ||
        !fs::exists(file, ec)) {
      std::ifstream in(session / L"export-rmc.txt");
      why = L"The game could not export that level. " + wide(std::string(std::istreambuf_iterator<char>(in), {}));
      return false;
    }
    NativeDocument next;
    next.open(file);
    adoptDocument(std::move(next), {}, true);
    return true;
  }
  // The campaign level this map came from (its export header), or -1.
  int campaignLevel(bool *sealed = nullptr, std::wstring *name = nullptr) const {
    for (size_t i = 0; i < doc.lines.size() && i < 60; ++i) {
      const auto &line = doc.lines[i];
      auto at = line.find(" (campaign chunk ");
      if (!line.starts_with("# ==== Map 0: ") || at == std::string::npos)
        continue;
      if (sealed)
        *sealed = line.find(", sealed)") != std::string::npos;
      if (name)
        *name = wide(line.substr(14, at - 14));
      return std::atoi(line.c_str() + at + 17);
    }
    return -1;
  }
  fs::path campaignOverrideFile(int level, bool ask = false) const {
    auto game = level < 0 ? fs::path{} : campaignGameRoot(ask);
    return game.empty() || level < 0 ? fs::path{} : game / L"campaign-overrides" / (std::to_wstring(level) + L".rmc");
  }
  // Save as first offers a campaign level its number and name, in a folder that never ships
  // for a sealed one ("Campaign edits") and where the game finds custom maps otherwise.
  bool campaignSaveHint(fs::path &folder, std::wstring &name) const {
    bool sealed = false;
    std::wstring title;
    int level = campaignLevel(&sealed, &title);
    if (level < 0)
      return false;
    wchar_t number[8]{};
    swprintf_s(number, L"%03d ", level);
    name = number + title;
    for (auto &c : name)
      if (wcschr(L"\\/:*?\"<>|", c))
        c = L'-';
    auto game = campaignGameRoot();
    folder = sealed ? root / L"Campaign edits" : !game.empty() ? game / L"custom maps" : root / L"custom maps";
    return true;
  }
  void saveIntoGame() {
    int level = campaignLevel();
    if (level < 0) {
      message(L"Save into the game is for a level opened from the campaign.");
      return;
    }
    auto file = campaignOverrideFile(level, true);
    if (file.empty()) {
      message(L"No game picked, so this is saved as a custom map instead.");
      openFile(true);
      return;
    }
    if (!validateDocument())
      return;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    saveTo(file);
    message(L"Saved into the game: level " + std::to_wstring(level) + L" now plays your version. Back to original undoes it.");
    relayout();
  }
  void backToOriginal() {
    int level = campaignLevel();
    auto file = campaignOverrideFile(level);
    std::error_code ec;
    if (file.empty() || !fs::exists(file, ec)) {
      message(L"This level already plays its original.");
      return;
    }
    if (MessageBoxW(window, (L"Put level " + std::to_wstring(level) + L" back to its original in the game?\n\nYour saved version (campaign-overrides\\" +
                             file.filename().wstring() + L") is deleted. What is open here stays open.").c_str(),
                    L"Back to original", MB_YESNO | MB_ICONQUESTION) != IDYES)
      return;
    fs::remove(file, ec);
    if (ec) {
      message(L"Could not delete " + file.wstring() + L".");
      return;
    }
    if (path == file) {
      path.clear();
      dirty = true;
      showTitle();
    }
    message(L"Level " + std::to_wstring(level) + L" plays its original again. Your version is still open here, not saved.");
    relayout();
  }
  static std::wstring campaignRow(const CampaignEntry &m) {
    wchar_t number[8]{};
    swprintf_s(number, L"%03d", m.chunk);
    return L"Chapter " + std::to_wstring(m.chapter) + L"  ·  " + m.district + L"     " + number + L"  " + m.name;
  }
  static void fillCampaignPicker(CampaignPicker &p) {
    wchar_t text[256]{};
    GetWindowTextW(p.search, text, 256);
    std::wstring needle = text;
    for (auto &c : needle)
      c = wchar_t(towlower(c));
    SendMessageW(p.list, LB_RESETCONTENT, 0, 0);
    p.shown.clear();
    for (int i = 0; i < int(p.maps->size()); ++i) {
      auto row = campaignRow((*p.maps)[size_t(i)]);
      auto folded = row;
      for (auto &c : folded)
        c = wchar_t(towlower(c));
      if (!needle.empty() && folded.find(needle) == std::wstring::npos)
        continue;
      SendMessageW(p.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
      p.shown.push_back(i);
    }
    SendMessageW(p.list, LB_SETCURSEL, 0, 0);
  }
  // The corner: "Admin" (or the badge and Lock once unlocked), and the unlock row when asked for.
  static void showCampaignAdmin(CampaignPicker &p) {
    auto name = retro::admin::unlocked();
    ShowWindow(p.admin, name.empty() ? SW_SHOW : SW_HIDE);
    ShowWindow(p.lock, name.empty() ? SW_HIDE : SW_SHOW);
    SetWindowTextW(p.badge, name.empty() ? L"" : (L"Admin: " + wide(name)).c_str());
    for (HWND h : {p.nameBox, p.pinBox, p.unlock})
      ShowWindow(h, p.adminRow && name.empty() ? SW_SHOW : SW_HIDE);
    if (p.adminRow && name.empty())
      SetFocus(p.nameBox);
  }
  static void reloadCampaignPicker(CampaignPicker &p) {
    std::wstring why;
    *p.maps = p.editor->campaignMaps(why);
    fillCampaignPicker(p);
    showCampaignAdmin(p);
    p.editor->showTitle();
  }
  static LRESULT CALLBACK campaignPickerProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto p = reinterpret_cast<CampaignPicker *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (p && m == WM_COMMAND) {
      switch (LOWORD(w)) {
      case 11: // the search box
        if (HIWORD(w) == EN_CHANGE)
          fillCampaignPicker(*p);
        return 0;
      case 3: // Admin
        p->adminRow = true;
        showCampaignAdmin(*p);
        return 0;
      case 4: // Lock
        retro::admin::lock();
        p->adminRow = false;
        SetWindowTextW(p->status, L"Locked again.");
        reloadCampaignPicker(*p);
        return 0;
      case 7: { // Unlock
        wchar_t name[64]{}, pin[32]{};
        GetWindowTextW(p->nameBox, name, 64);
        GetWindowTextW(p->pinBox, pin, 32);
        SetWindowTextW(p->pinBox, L"");
        if (retro::admin::unlock(utf8(name), utf8(pin))) {
          p->adminRow = false;
          SetWindowTextW(p->status, L"Unlocked: every chapter is listed.");
          reloadCampaignPicker(*p);
        } else
          SetWindowTextW(p->status, L"That name and PIN do not match.");
        return 0;
      }
      case 9: { // Export my changes
        std::wstring why;
        p->editor->exportMyChanges(true, why);
        SetWindowTextW(p->status, why.c_str());
        return 0;
      }
      case IDOK:
      case 12: // the list
        if (LOWORD(w) == 12 && HIWORD(w) != LBN_DBLCLK)
          return 0;
        if (GetFocus() == p->nameBox || GetFocus() == p->pinBox) {
          SendMessageW(h, WM_COMMAND, 7, 0);
          return 0;
        }
        {
          auto selected = int(SendMessageW(p->list, LB_GETCURSEL, 0, 0));
          if (selected >= 0 && selected < int(p->shown.size())) {
            p->chosen = p->shown[size_t(selected)];
            p->done = true;
          }
        }
        return 0;
      case IDCANCEL:
        p->done = true;
        return 0;
      }
    }
    if (p && m == WM_CLOSE) {
      p->done = true;
      return 0;
    }
    return DefWindowProcW(h, m, w, l);
  }
  // A plain list of chapters and levels with a search box; the index picked, or -1.
  int pickCampaignMap(std::vector<CampaignEntry> &maps, bool adminRow) {
    static bool registered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registered) {
      WNDCLASSW wc{};
      wc.lpfnWndProc = campaignPickerProc;
      wc.hInstance = instance;
      wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
      wc.lpszClassName = L"RawMetalCampaignPicker";
      RegisterClassW(&wc);
      registered = true;
    }
    RECT owner{};
    GetWindowRect(window, &owner);
    const int width = 700, height = 700;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"RawMetalCampaignPicker", L"Open from campaign",
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU, owner.left + (owner.right - owner.left - width) / 2,
                                  owner.top + (owner.bottom - owner.top - height) / 2, width, height, window, nullptr, instance, nullptr);
    if (!dialog)
      return -1;
    CampaignPicker p;
    p.maps = &maps;
    p.editor = this;
    p.adminRow = adminRow;
    SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&p));
    RECT c{};
    GetClientRect(dialog, &c);
    auto child = [&](const wchar_t *kind, const wchar_t *text, DWORD style, int id, int x, int y, int w, int h, HFONT face = nullptr) {
      HWND made = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, dialog,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
      SendMessageW(made, WM_SETFONT, reinterpret_cast<WPARAM>(face ? face : font), TRUE);
      return made;
    };
    child(L"STATIC", L"Pick one of the game's levels. It opens as a new map; nothing is saved until you choose to.", 0, -1, 16, 14,
          c.right - 32, 40);
    p.search = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 11, 16, 56, c.right - 32, 28);
    SendMessageW(p.search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Find by name, number or chapter"));
    p.list = child(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP, 12, 16, 94, c.right - 32,
                   c.bottom - 94 - 132);
    // The unlock row, hidden until Admin (or Ctrl+Shift+A) asks for it.
    const int row = c.bottom - 126;
    p.nameBox = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 5, 16, row, 170, 28);
    SendMessageW(p.nameBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Name"));
    p.pinBox = child(L"EDIT", L"", WS_BORDER | ES_PASSWORD | ES_NUMBER | WS_TABSTOP, 6, 194, row, 90, 28);
    SendMessageW(p.pinBox, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"PIN"));
    p.unlock = child(L"BUTTON", L"Unlock", WS_TABSTOP, 7, 292, row - 1, 90, 30);
    p.status = child(L"STATIC", L"", 0, -1, 16, c.bottom - 88, c.right - 32, 34, smallFont);
    // The corner: Admin (small), or the badge and Lock; and the edits to send back.
    p.admin = child(L"BUTTON", L"Admin", WS_TABSTOP, 3, 16, c.bottom - 46, 70, 28, smallFont);
    p.lock = child(L"BUTTON", L"Lock", WS_TABSTOP, 4, 16, c.bottom - 46, 70, 28, smallFont);
    p.badge = child(L"STATIC", L"", SS_CENTERIMAGE, -1, 94, c.bottom - 46, 120, 28, smallFont);
    child(L"BUTTON", L"Export my changes", WS_TABSTOP, 9, 220, c.bottom - 50, 168, 34);
    child(L"BUTTON", L"Open", BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, c.right - 16 - 224, c.bottom - 50, 108, 34);
    child(L"BUTTON", L"Cancel", WS_TABSTOP, IDCANCEL, c.right - 16 - 108, c.bottom - 50, 108, 34);
    fillCampaignPicker(p);
    EnableWindow(window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetFocus(p.search);
    showCampaignAdmin(p);
    MSG msg{};
    bool quit = false;
    while (!p.done) {
      int got = GetMessageW(&msg, nullptr, 0, 0);
      if (got <= 0) {
        quit = got == 0;
        break;
      }
      // Up and down in the search box walk the list.
      if (msg.hwnd == p.search && msg.message == WM_KEYDOWN && (msg.wParam == VK_DOWN || msg.wParam == VK_UP)) {
        auto at = int(SendMessageW(p.list, LB_GETCURSEL, 0, 0)) + (msg.wParam == VK_DOWN ? 1 : -1);
        if (at >= 0 && at < int(p.shown.size()))
          SendMessageW(p.list, LB_SETCURSEL, WPARAM(at), 0);
        continue;
      }
      // Ctrl+Shift+A here too.
      if (msg.message == WM_KEYDOWN && msg.wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
        p.adminRow = true;
        showCampaignAdmin(p);
        continue;
      }
      if (!IsDialogMessageW(dialog, &msg)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    EnableWindow(window, TRUE);
    DestroyWindow(dialog);
    SetForegroundWindow(window);
    showTitle();
    if (quit)
      PostQuitMessage(int(msg.wParam));
    return p.chosen;
  }
  void openFromCampaign(bool adminRow = false) {
    if (!confirmSave())
      return;
    std::wstring why;
    message(L"Asking the game for its levels...");
    auto maps = campaignMaps(why);
    if (maps.empty() && !adminRow) {
      message(why);
      return;
    }
    int pick = pickCampaignMap(maps, adminRow);
    if (pick < 0 || pick >= int(maps.size())) {
      message(L"Nothing opened.");
      return;
    }
    const auto entry = maps[size_t(pick)];
    message(L"Opening " + entry.name + L"...");
    if (!openCampaignMap(entry, why)) {
      message(why);
      return;
    }
    message(L"Opened " + entry.name + L" from the game as a new map. Save into the game makes the game play it; Save as keeps a copy.");
  }
  // ---- Open from campaign: admin unlock, and sending edits back ----
  // A public builder lists sealed chapters only for a whitelisted admin who
  // unlocked them on this machine (src/core/CampaignAdmin.h: a salted hash,
  // remembered per Windows user). Ctrl+Shift+A opens the unlock row.
  static std::wstring adminBadge() {
    auto name = retro::admin::unlocked();
    return name.empty() ? std::wstring() : L"  ·  Admin: " + wide(name);
  }
  // Who the edits are from: the unlocked admin, else the Windows user.
  static std::wstring campaignAuthor() {
    if (auto name = retro::admin::unlocked(); !name.empty())
      return wide(name);
    auto typed = retro::admin::read(L"Author");
    if (!typed.empty())
      return typed;
    wchar_t user[256]{};
    DWORD size = 256;
    return GetUserNameW(user, &size) && user[0] ? std::wstring(user) : std::wstring(L"Builder");
  }
  // SHA-256 (hex) of a file's records, as the engine's rmcDataHash: the data section
  // without comments or blank lines. It names the level an edit started from.
  static std::string campaignDataHash(const std::string &text) {
    auto start = text.find("--- CUSTOM_CAMPAIGN_DATA_START ---"), end = text.find("--- CUSTOM_CAMPAIGN_DATA_END ---");
    if (start == std::string::npos || end == std::string::npos || end < start)
      return {};
    std::string canonical, line;
    std::istringstream lines(text.substr(start, end - start));
    std::getline(lines, line);
    while (std::getline(lines, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      canonical += line;
      canonical += '\n';
    }
    auto digest = retro::assetHash(std::span<const unsigned char>(reinterpret_cast<const unsigned char *>(canonical.data()), canonical.size()));
    static const char *hex = "0123456789abcdef";
    std::string out;
    for (auto b : digest)
      out += hex[b >> 4], out += hex[b & 15];
    return out;
  }
  // The CAMPAIGN_BASE line an exported level carries: chunk, version and hash.
  struct CampaignBase {
    int chunk = -1;
    std::string version, hash, name;
  };
  static CampaignBase campaignBase(const std::string &text) {
    CampaignBase base;
    std::istringstream lines(text);
    std::string line;
    for (int n = 0; n < 80 && std::getline(lines, line); ++n) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.starts_with("META_CAMPAIGN_NAME: "))
        base.name = line.substr(20);
      if (!line.starts_with("# CAMPAIGN_BASE|"))
        continue;
      for (auto &field : NativeDocument::fields(line.substr(2))) {
        auto eq = field.find('=');
        if (eq == std::string::npos)
          continue;
        auto key = field.substr(0, eq), value = field.substr(eq + 1);
        if (key == "chunk")
          base.chunk = std::atoi(value.c_str());
        else if (key == "version")
          base.version = value;
        else if (key == "hash")
          base.hash = value;
      }
    }
    return base;
  }
  static std::uint32_t zipCrc(const std::string &s) {
    static std::uint32_t table[256]{};
    if (!table[1])
      for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k)
          c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[i] = c;
      }
    std::uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : s)
      c = table[(c ^ b) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
  }
  // A plain (stored) zip: any unzip tool and Python's zipfile read it.
  static bool writeZip(const fs::path &file, const std::vector<std::pair<std::string, std::string>> &entries) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    const std::uint16_t time = std::uint16_t(now.wHour << 11 | now.wMinute << 5 | now.wSecond / 2),
                        date = std::uint16_t((now.wYear - 1980) << 9 | now.wMonth << 5 | now.wDay);
    std::string out, central;
    auto u16 = [](std::string &s, std::uint32_t v) { s += char(v & 255), s += char(v >> 8 & 255); };
    auto u32 = [&](std::string &s, std::uint32_t v) { u16(s, v & 0xffff), u16(s, v >> 16); };
    for (const auto &[name, data] : entries) {
      const auto offset = std::uint32_t(out.size()), crc = zipCrc(data), size = std::uint32_t(data.size());
      u32(out, 0x04034b50), u16(out, 20), u16(out, 0x0800), u16(out, 0), u16(out, time), u16(out, date);
      u32(out, crc), u32(out, size), u32(out, size), u16(out, std::uint32_t(name.size())), u16(out, 0);
      out += name, out += data;
      u32(central, 0x02014b50), u16(central, 20), u16(central, 20), u16(central, 0x0800), u16(central, 0), u16(central, time), u16(central, date);
      u32(central, crc), u32(central, size), u32(central, size), u16(central, std::uint32_t(name.size()));
      u16(central, 0), u16(central, 0), u16(central, 0), u16(central, 0), u32(central, 0), u32(central, offset);
      central += name;
    }
    const auto at = std::uint32_t(out.size());
    out += central;
    u32(out, 0x06054b50), u16(out, 0), u16(out, 0), u16(out, std::uint32_t(entries.size())), u16(out, std::uint32_t(entries.size()));
    u32(out, std::uint32_t(central.size())), u32(out, at), u16(out, 0);
    std::ofstream zip(file, std::ios::binary);
    zip << out;
    return bool(zip);
  }
  // Every campaign level changed from where it started (saved into the game, saved
  // in Campaign edits, or open now), the newest of each, with the level it started
  // from when this game still has it, in one zip; Explorer opens on it.
  fs::path exportMyChanges(bool reveal, std::wstring &why) {
    struct Change {
      CampaignBase base;
      std::string text, edit;
      fs::file_time_type when{};
    };
    std::map<int, Change> changes;
    auto consider = [&](const std::string &text, fs::file_time_type when) {
      auto base = campaignBase(text);
      auto edit = campaignDataHash(text);
      if (base.chunk < 0 || base.hash.empty() || edit.empty() || edit == base.hash)
        return;
      auto &slot = changes[base.chunk];
      if (slot.text.empty() || when >= slot.when)
        slot = {base, text, edit, when};
    };
    std::vector<fs::path> folders{root / L"Campaign edits"};
    if (auto game = campaignGameRoot(); !game.empty())
      folders.push_back(game / L"campaign-overrides");
    std::error_code ec;
    for (const auto &folder : folders)
      for (const auto &entry : fs::directory_iterator(folder, ec))
        if (entry.is_regular_file() && entry.path().extension() == L".rmc") {
          std::ifstream in(entry.path(), std::ios::binary);
          consider(std::string(std::istreambuf_iterator<char>(in), {}), entry.last_write_time(ec));
        }
    if (campaignLevel() >= 0)
      consider(doc.text(), fs::file_time_type::clock::now());
    if (changes.empty()) {
      why = L"No campaign level has changed yet: open one from the campaign, change it and save it.";
      return {};
    }
    auto author = campaignAuthor();
    for (auto &c : author)
      if (wcschr(L"\\/:*?\"<>| ", c))
        c = L'-';
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t date[16]{};
    swprintf_s(date, L"%04d-%02d-%02d", now.wYear, now.wMonth, now.wDay);
    std::ostringstream manifest;
    manifest << "# Campaign edits from the Raw Metal Campaign Builder; tools/campaign/merge_edits.py installs them.\n";
    manifest << "author|" << utf8(campaignAuthor()) << "\nexported|" << utf8(date) << "\n";
    manifest << "# level|chunk|base version|base hash|edit hash|edit file|base file\n";
    std::vector<std::pair<std::string, std::string>> entries;
    for (auto &[chunk, change] : changes) {
      char number[8]{};
      std::snprintf(number, sizeof(number), "%03d", chunk);
      auto name = std::string(number) + " " + change.base.name;
      for (auto &c : name)
        if (std::strchr("\\/:*?\"<>|", c))
          c = '-';
      std::string baseFile;
      // The level it started from, exported again here: kept when it still matches.
      auto exported = session / (L"base-" + std::to_wstring(chunk) + L".rmc");
      fs::remove(exported, ec);
      if (runCampaignEngine(L"--export-rmc=" + std::to_wstring(chunk) + L" --out=\"" + exported.wstring() + L"\"") == 0) {
        std::ifstream in(exported, std::ios::binary);
        std::string text(std::istreambuf_iterator<char>(in), {});
        if (campaignDataHash(text) == change.base.hash)
          baseFile = "base/" + name + ".rmc", entries.push_back({baseFile, text});
      }
      entries.push_back({name + ".rmc", change.text});
      manifest << "level|" << chunk << '|' << change.base.version << '|' << change.base.hash << '|' << change.edit << '|' << name << ".rmc|" << baseFile << '\n';
    }
    entries.insert(entries.begin(), {"manifest.txt", manifest.str()});
    auto folder = root / L"Campaign edits";
    fs::create_directories(folder, ec);
    auto file = folder / (author + L"-campaign-edits-" + date + L".zip");
    if (!writeZip(file, entries)) {
      why = L"Could not write " + file.wstring() + L".";
      return {};
    }
    if (reveal)
      ShellExecuteW(window, L"open", L"explorer.exe", (L"/select,\"" + file.wstring() + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    why = L"Exported " + std::to_wstring(changes.size()) + L" changed level" + (changes.size() == 1 ? L"" : L"s") + L" to " + file.filename().wstring() + L". Send that file.";
    return file;
  }
  void exportMyChangesNow() {
    std::wstring why;
    exportMyChanges(true, why);
    message(why);
  }
  bool campaignShortcut(const MSG &msg) {
    if (msg.message != WM_KEYDOWN || !(GetKeyState(VK_CONTROL) & 0x8000) || !(GetKeyState(VK_SHIFT) & 0x8000))
      return false;
    if (msg.wParam == 'A')
      openFromCampaign(true);
    else if (msg.wParam == 'E')
      exportMyChangesNow();
    else
      return false;
    return true;
  }
  // ==== Open from campaign (end) ============================================
  void openFile(bool save, bool validate = false) {
    if (!save && !confirmSave())
      return;
    wchar_t file[32768]{};
    if (save && !path.empty())
      wcsncpy_s(file, path.c_str(), _TRUNCATE);
    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner = window;
    dialog.lpstrFilter =
        L"Raw Metal campaigns (*.rmc;*.txt)\0*.rmc;*.txt\0All files\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = 32768;
    dialog.lpstrDefExt = L"rmc";
    auto exportDirectory = root / L"custom maps";
    auto gameRoot = root.parent_path().parent_path();
    if (validate && fs::exists(gameRoot / L"RawMetal.exe") &&
        fs::is_directory(gameRoot / L"custom maps"))
      exportDirectory = gameRoot / L"custom maps";
    // Open from campaign: a level from the game is offered its own number, name and folder.
    fs::path campaignFolder;
    std::wstring campaignName;
    if (save && path.empty() && campaignSaveHint(campaignFolder, campaignName)) {
      std::error_code ec;
      fs::create_directories(campaignFolder, ec);
      exportDirectory = campaignFolder;
    }
    dialog.lpstrInitialDir = exportDirectory.c_str();
    if (save && (path.empty() || isExample())) {
      auto name = std::wstring(L"My map");
      for (auto index : doc.records("CAMPAIGN")) {
        auto f = NativeDocument::fields(doc.lines[index]);
        if (f.size() > 1 && !f[1].empty() && f[1] != "New custom campaign" && f[1] != "STARTER MAP")
          name = wide(decoded(f[1]));
      }
      for (auto &c : name)
        if (wcschr(L"\\/:*?\"<>|", c))
          c = L'-';
      if (!campaignName.empty())
        name = campaignName;
      wcsncpy_s(file, name.c_str(), _TRUNCATE);
    }
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)))
      return;
    if (save) {
      if (validate && !validateDocument())
        return;
      saveTo(fs::path(file));
      if (validate)
        message(L"Saved a copy the game can open.");
    } else {
      NativeDocument next;
      next.open(file);
      int clocks = repairRetiredEditorPeople(next);
      adoptDocument(std::move(next), fs::path(file), clocks > 0);
      if (clocks)
        message(L"People from the old editor were missing their clock or guard post. Both are back; save to keep them.");
    }
  }
  bool validateDocument() {
    publish(false);
#ifdef RAWMETAL_UNIFIED
    try {
      retro::validateEditorCampaignText(doc.text());
    } catch (const std::exception &error) {
      message(L"Engine validation: " + wide(error.what()));
      return false;
    }
    message(L"Check passed.");
    return true;
#else
    auto report = session / L"validation.txt";
    std::wstring args = L"\"" + engineExecutable().wstring() +
                        L"\" --editor-validate=\"" + pack.wstring() + L"\"";
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log =
        CreateFileW(report.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = log;
    si.hStdError = log;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION p{};
    if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, root.c_str(), &si, &p)) {
      CloseHandle(log);
      throw std::runtime_error("Cannot start native validation");
    }
    auto waited = WaitForSingleObject(p.hProcess, 60000);
    if (waited == WAIT_TIMEOUT) {
      TerminateProcess(p.hProcess, 9);
      WaitForSingleObject(p.hProcess, 5000);
    }
    DWORD code = 1;
    GetExitCodeProcess(p.hProcess, &code);
    CloseHandle(p.hThread);
    CloseHandle(p.hProcess);
    CloseHandle(log);
    if (code) {
      std::ifstream in(report);
      std::string error(std::istreambuf_iterator<char>(in), {});
      message(L"Engine validation: " + wide(error));
      return false;
    }
    message(L"Check passed.");
    return true;
#endif
  }
  fs::path engineExecutable() const {
    return root / L".build" / L"engine" / L"RawMetal.exe";
  }
  // ---- --editor-perf-test: the builder's own timing run. The Starter Map,
  // hidden, with the live engine picture; every gesture goes through the real
  // window procedures. native-editor-perf-test.txt gets the numbers.
  fs::path perfFile; // the preview's --perf-trace lines
  bool perfTest = false;
  void verifyPerf() {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    auto pumpOnce = [&] {
      MSG msg{};
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    };
    auto pump = [&](int ms, auto done) {
      const double until = edperf::now() + ms;
      while (edperf::now() < until) {
        pumpOnce();
        if (done())
          return true;
        Sleep(1);
      }
      return false;
    };
    auto summary = [&](const std::string &what, std::vector<double> v) {
      if (v.empty()) {
        out << what << ": no samples\n";
        return 0.0;
      }
      std::sort(v.begin(), v.end());
      double sum = 0;
      for (double x : v)
        sum += x;
      out << what << ": n " << v.size() << ", avg " << sum / double(v.size()) << ", median " << v[v.size() / 2]
          << ", p95 " << v[std::min(v.size() - 1, v.size() * 95 / 100)] << ", max " << v.back() << " ms\n";
      return sum / double(v.size());
    };
    auto timed = [&](auto &&f) {
      double t = edperf::now();
      f();
      return edperf::now() - t;
    };
    // The old paths take seconds a gesture: fewer samples of each.
    auto count = [](int n) { return edperf::baseline ? std::max(2, n / 10) : n; };
    out << (edperf::baseline ? "BASELINE (the paths before the speed-up)\n" : "CURRENT\n");
    MoveWindow(window, 0, 0, 1920, 1080, FALSE);
    double opened = timed([&] {
      doc.open(root / L"custom maps" / L"Starter Map.rmc");
      path = root / L"custom maps" / L"Starter Map.rmc";
      map = 0, layer = 0, record = -1;
      refresh();
      layout();
    });
    out << "Starter Map: " << doc.lines.size() << " lines\n";
    out << "open + refresh + layout: " << opened << " ms\n";
    RECT pr{};
    GetClientRect(plan, &pr);
    out << "plan area: " << pr.right << " x " << pr.bottom << "\n";
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP canvas = CreateCompatibleBitmap(screen, std::max(1L, pr.right), std::max(1L, pr.bottom));
    auto oldCanvas = SelectObject(dc, canvas);
    ReleaseDC(nullptr, screen);
    // What a repaint costs: the plan window is hidden here, so its WM_PAINT
    // is drawn into the same kind of off-screen DC the window uses.
    auto paint = [&] { return timed([&] { drawPlan(this, dc, pr); }); };
    // The cached map layer must give the very picture a full draw gives.
    int planChecks = 0, planMismatches = 0;
    auto samePlan = [&] {
      if (edperf::baseline)
        return;
      auto grab = [&] {
        BITMAPINFO bi{};
        bi.bmiHeader = {sizeof(BITMAPINFOHEADER), pr.right, -pr.bottom, 1, 32, BI_RGB};
        std::vector<std::uint32_t> pixels(size_t(pr.right) * size_t(pr.bottom));
        SelectObject(dc, oldCanvas);
        GetDIBits(dc, canvas, 0, UINT(pr.bottom), pixels.data(), &bi, DIB_RGB_COLORS);
        SelectObject(dc, canvas);
        return pixels;
      };
      drawPlan(this, dc, pr);
      auto cached = grab();
      edperf::baseline = true;
      drawPlan(this, dc, pr);
      edperf::baseline = false;
      ++planChecks, planMismatches += cached != grab();
    };
    auto view = planView();
    auto at = [&](float x, float y) { return MAKELPARAM(int(view.ox + x * view.cell), int(view.oy + y * view.cell)); };
    auto send = [&](UINT m, float x, float y, WPARAM w = 0) { return timed([&] { SendMessageW(plan, m, w, at(x, y)); }); };
    {
      std::vector<double> early;
      for (int i = 0; i < count(10); ++i)
        early.push_back(timed([&] { layout(); }));
      summary("layout of every pane, before the engine picture exists", early);
    }
    // ---- engine, with the window's own 50 ms status poll running as in a session
    double engineAt = edperf::now();
    startEngine();
    SetTimer(window, 1, 50, nullptr);
    statusTimer = true;
    auto statusRevision = [&](unsigned long long *frame = nullptr) { return engineStatusRevision(frame); };
    bool ready = pump(240000, [&] {
      unsigned long long f = 0;
      return statusRevision(&f) == revision && f >= 3;
    });
    out << "engine start to first picture: " << edperf::now() - engineAt << " ms" << (ready ? "" : " (NOT READY)") << "\n";
    // The preview's own lines: picture times per revision, frame stats.
    auto previewLines = [&] {
      std::ifstream in(perfFile);
      std::vector<std::string> lines;
      for (std::string line; std::getline(in, line);)
        lines.push_back(line);
      return lines;
    };
    auto pictureAt = [&](long long rev) {
      std::string key = " preview rev " + std::to_string(rev) + " ";
      for (auto &line : previewLines())
        if (line.find(key) != std::string::npos) {
          auto p = line.find("picture at ");
          if (p != std::string::npos)
            return std::stod(line.substr(p + 11));
        }
      return -1.0;
    };
    // ---- plan paint
    tool = 0;
    std::vector<double> paints;
    for (int i = 0; i < count(30); ++i)
      paints.push_back(paint());
    summary("plan paint, whole map (Select tool)", paints);
    samePlan();
    // ---- hover
    std::vector<double> hover, hoverPaint;
    for (int i = 0; i < count(120); ++i) {
      float x = 1.5f + float(i % 40) * .5f, y = 2.f + float(i / 40) * 6.f + float(i % 7) * .3f;
      hover.push_back(send(WM_MOUSEMOVE, x, y));
      hoverPaint.push_back(hover.back() + paint());
    }
    samePlan();
    summary("hover (Select): mouse move handler", hover);
    summary("hover (Select): mouse move + repaint", hoverPaint);
    // ---- click select: a piece of furniture on map 0, then empty floor
    int fixture = -1;
    NativeHandle fh{};
    // Furniture if the map has any, else the first small thing well inside it.
    for (const char *wanted : {"FIXTURE|", "SIGN|", "LIGHT|", ""}) {
      for (auto index : objects()) {
        auto hnd = *handleFor(index);
        if (doc.lines[index].rfind(wanted, 0) == 0 && hnd.x > 2 && hnd.y > 2 && hnd.x < 22 && hnd.y < 22 &&
            hnd.width < 4 && hnd.depth < 4) {
          fixture = int(index), fh = hnd;
          break;
        }
      }
      if (fixture >= 0)
        break;
    }
    out << "thing picked for clicks and drags: " << (fixture >= 0 ? doc.lines[size_t(fixture)].substr(0, 40) : "none")
        << " at " << fh.x << ", " << fh.y << "\n";
    std::vector<double> press, release, clickPaint;
    for (int i = 0; i < count(10) && fixture >= 0; ++i) {
      float x = i % 2 ? 12.5f : fh.x, y = i % 2 ? 12.5f : fh.y;
      press.push_back(send(WM_LBUTTONDOWN, x, y, MK_LBUTTON));
      release.push_back(send(WM_LBUTTONUP, x, y));
      clickPaint.push_back(press.back() + release.back() + paint());
    }
    samePlan();
    summary("click to pick: press", press);
    summary("click to pick: release", release);
    summary("click to pick: press + release + repaint", clickPaint);
    // ---- tool switch
    std::vector<double> toolSwitch;
    int switches = 0;
    for (int t : {1, 2, 6, 8, 20, 0, 3, 11, 0, 29, 2, 0}) {
      if (++switches > count(12))
        break;
      double kindMs = 0;
      toolSwitch.push_back(timed([&] {
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(ToolBase + t, BN_CLICKED), reinterpret_cast<LPARAM>(tools[size_t(t)]));
        // A shown window lays its panes out again for the new tool's options.
        layout();
        drawPlan(this, dc, pr);
      }));
      if (*toolInfo[t].kind)
        kindMs = timed([&] { chooseKind(toolInfo[t].kind); });
      out << "  tool " << utf8(toolInfo[t].name) << ": " << toolSwitch.back() << " ms (its record list alone " << kindMs << " ms)\n";
    }
    summary("tool switch + layout + repaint", toolSwitch);
    std::vector<double> layouts;
    for (int i = 0; i < count(10); ++i)
      layouts.push_back(timed([&] { layout(); }));
    summary("layout of every pane (shown window: each pick and tool switch)", layouts);
    // ---- object drag on the plan
    std::vector<double> dragMove, dragRelease;
    if (fixture >= 0) {
      tool = 0;
      send(WM_LBUTTONDOWN, fh.x, fh.y, MK_LBUTTON);
      for (int i = 1; i <= count(40); ++i) {
        dragMove.push_back(send(WM_MOUSEMOVE, fh.x + i * .05f, fh.y + i * .02f, MK_LBUTTON) + paint());
        pump(8, [] { return false; });
      }
      dragRelease.push_back(send(WM_LBUTTONUP, fh.x + 2.f, fh.y + .8f) + paint());
      SendMessageW(window, WM_COMMAND, Undo, 0);
    }
    samePlan();
    summary("drag furniture: each move + repaint", dragMove);
    summary("drag furniture: release (lands, history, inspector) + repaint", dragRelease);
    // ---- brush stroke: the Wall tool, Brush, over the map
    std::vector<double> strokeMove, strokeEnds;
    {
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(ToolBase + 3, BN_CLICKED), reinterpret_cast<LPARAM>(tools[3]));
      brush = 0;
      strokeEnds.push_back(send(WM_LBUTTONDOWN, 3.5f, 20.5f, MK_LBUTTON) + paint());
      for (int i = 0; i < count(36); ++i) {
        strokeMove.push_back(send(WM_MOUSEMOVE, 3.5f + float(i % 18) * 1.f, 20.5f + float(i / 18), MK_LBUTTON) + paint());
        pump(8, [] { return false; });
      }
      strokeEnds.push_back(send(WM_LBUTTONUP, 20.5f, 21.5f) + paint());
      SendMessageW(window, WM_COMMAND, Undo, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(ToolBase + 0, BN_CLICKED), reinterpret_cast<LPARAM>(tools[0]));
    }
    samePlan();
    summary("paint stroke (Wall brush): each cell + repaint", strokeMove);
    summary("paint stroke: press and release + repaint", strokeEnds);
    // ---- edit to 3D: a nudge publishes; how long until the engine has drawn it
    pump(3000, [&] { return statusRevision() == revision; });
    std::vector<double> publishCost, editTo3d, editSeen;
    if (fixture >= 0)
      for (int i = 0; i < count(10) + (edperf::baseline ? 1 : 0); ++i) {
        selectRecordOnly(fixture);
        double t0 = edperf::now();
        publishCost.push_back(timed([&] { moveSelected(i % 2 ? -.25f : .25f, 0); }));
        long long rev = revision;
        double seen = -1;
        bool drawn = pump(edperf::baseline ? 120000 : 20000, [&] {
          if (seen < 0 && statusRevision() >= rev)
            seen = edperf::now() - t0;
          return pictureAt(rev) > 0;
        });
        if (seen >= 0)
          editSeen.push_back(seen);
        if (drawn)
          editTo3d.push_back(pictureAt(rev) - t0);
        pump(150, [] { return false; });
      }
    summary("edit (nudge) on the window thread: change, history, publish", publishCost);
    summary("edit to engine loaded (status READY)", editSeen);
    summary("edit to 3D picture drawn", editTo3d);
    // ---- another map from the map list, and back: how long until the picture shows it
    {
      auto pictureOf = [&](long long rev, int m, size_t from) {
        std::string key = " preview rev " + std::to_string(rev) + " map " + std::to_string(m) + ":";
        auto lines = previewLines();
        for (size_t i = from; i < lines.size(); ++i)
          if (lines[i].find(key) != std::string::npos) {
            auto p = lines[i].find("picture at ");
            if (p != std::string::npos)
              return std::stod(lines[i].substr(p + 11));
          }
        return -1.0;
      };
      std::vector<double> switches, switchUi;
      for (int target : {1, 0, 2, 0}) {
        auto item = SendMessageW(mapList, LB_GETCOUNT, 0, 0);
        LRESULT at = -1;
        for (LRESULT i = 0; i < item; ++i)
          if (SendMessageW(mapList, LB_GETITEMDATA, i, 0) == target)
            at = i;
        if (at < 0)
          continue;
        size_t from = previewLines().size();
        double t0 = edperf::now();
        switchUi.push_back(timed([&] {
          SendMessageW(mapList, LB_SETCURSEL, at, 0);
          SendMessageW(window, WM_COMMAND, MAKEWPARAM(MapList, LBN_SELCHANGE), reinterpret_cast<LPARAM>(mapList));
        }));
        if (pump(edperf::baseline ? 180000 : 60000, [&] { return pictureOf(revision, target, from) > 0; }))
          switches.push_back(pictureOf(revision, target, from) - t0);
        out << "  map " << target << ": picture after " << (switches.empty() ? -1.0 : switches.back()) << " ms\n";
      }
      summary("map switch on the window thread", switchUi);
      summary("map switch to its 3D picture", switches);
    }
    // ---- preview frame times: idle, then while dragging furniture continuously
    auto frameStats = [&](size_t from, const std::string &what) {
      auto lines = previewLines();
      std::vector<double> fps, work;
      for (size_t i = from; i < lines.size(); ++i) {
        auto &line = lines[i];
        auto p = line.find(" preview frames ");
        if (p == std::string::npos)
          continue;
        auto num = [&](const char *key) {
          auto q = line.find(key);
          return q == std::string::npos ? -1.0 : std::stod(line.substr(q + strlen(key)));
        };
        fps.push_back(num(" ("));
        work.push_back(num("work avg "));
        out << "  " << what << ": " << line.substr(p + 9) << "\n";
      }
      summary(what + ": preview fps per 2 s", fps);
      summary(what + ": preview work per frame", work);
    };
    size_t mark = previewLines().size();
    pump(6500, [] { return false; });
    frameStats(mark, "idle");
    mark = previewLines().size();
    if (fixture >= 0) {
      tool = 0;
      selectRecordOnly(fixture);
      auto h0 = *handleFor(size_t(fixture));
      send(WM_LBUTTONDOWN, h0.x, h0.y, MK_LBUTTON);
      double until = edperf::now() + 6500;
      int i = 0;
      std::vector<double> liveMove;
      while (edperf::now() < until) {
        float dx = std::sin(float(i) * .05f) * 2.f;
        liveMove.push_back(send(WM_MOUSEMOVE, h0.x + dx, h0.y, MK_LBUTTON) + paint());
        ++i;
        pump(16, [] { return false; });
      }
      send(WM_LBUTTONUP, h0.x, h0.y);
      SendMessageW(window, WM_COMMAND, Undo, 0);
      summary("continuous drag: each move + repaint", liveMove);
    }
    frameStats(mark, "editing");
    // ---- other window-thread work
    std::vector<double> rememberCost, saveCost, thumbCost, soundCost;
    for (int i = 0; i < 5; ++i) {
      rememberCost.push_back(timed([&] { remember(); }));
      history.pop_back();
      // Before: the whole map written on the window's thread. Now: the text
      // taken, the write on its own thread (waited for outside the timing).
      if (edperf::baseline)
        saveCost.push_back(timed([&] { doc.save(session / L"perf-autosave.rmc"); }));
      else {
        saveCost.push_back(timed([&] { autosaveTo(session / L"perf-autosave.rmc"); }));
        waitAutosave();
      }
    }
    thumbCost.push_back(timed([&] { thumbs.folder = thumbFolder(); makeThumbnails(false); }));
    {
      // A silent clip through the same PlaySound call the builder's sounds use.
      std::vector<char> wav(44 + 3200, 0);
      auto put32 = [&](size_t a, uint32_t v) { memcpy(wav.data() + a, &v, 4); };
      auto put16 = [&](size_t a, uint16_t v) { memcpy(wav.data() + a, &v, 2); };
      memcpy(wav.data(), "RIFF", 4), put32(4, uint32_t(36 + 3200)), memcpy(wav.data() + 8, "WAVEfmt ", 8);
      put32(16, 16), put16(20, 1), put16(22, 1), put32(24, 16000), put32(28, 32000), put16(32, 2), put16(34, 16);
      memcpy(wav.data() + 36, "data", 4), put32(40, 3200);
      for (int i = 0; i < 5; ++i) {
        soundCost.push_back(timed([&] { PlaySoundW(reinterpret_cast<LPCWSTR>(wav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT); }));
        Sleep(150);
      }
      PlaySoundW(nullptr, nullptr, 0);
    }
    samePlan();
    if (!edperf::baseline)
      out << "plan map layer from the cache matches a full draw: " << planChecks - planMismatches << " of " << planChecks << " checks\n";
    summary("undo snapshot (remember)", rememberCost);
    summary("autosave, time on the window thread", saveCost);
    summary("thumbnail list (start of session)", thumbCost);
    summary("PlaySound (silent clip)", soundCost);
    if (shotsProcess.hProcess)
      TerminateProcess(shotsProcess.hProcess, 0), CloseHandle(shotsProcess.hProcess), shotsProcess = {};
    SelectObject(dc, oldCanvas);
    DeleteObject(canvas);
    DeleteDC(dc);
    edperf::flush(true);
    std::ofstream(root / (edperf::baseline ? L"native-editor-perf-baseline.txt" : L"native-editor-perf-test.txt"))
        << (ready ? "DONE" : "ENGINE NOT READY") << "\n" << out.str();
    dirty = false;
  }
  // READY revision from the engine's status (frame count optional), -1 when not ready.
  long long engineStatusRevision(unsigned long long *frame = nullptr) const {
    std::istringstream in(channelText(status));
    std::string tag;
    long long rev = -1;
    unsigned long long f = 0;
    int m = 0, p = 0;
    if (!(in >> tag >> rev >> m >> p >> f) || tag != "READY")
      return -1LL;
    if (frame)
      *frame = f;
    return rev;
  }
  void startEngine() {
#ifdef RAWMETAL_UNIFIED
    // One process: the picture's engine runs on its own thread here and the
    // document, control and status pass in memory (EditorChannel.h).
    inProcess = retro::editorchannel::active = !edperf::baseline;
#endif
    publish(false);
    writeLook(); // the picture's light, air and play options (NativeLightView.inc)
    engineLive = true;
#ifdef RAWMETAL_UNIFIED
    // On this thread, before the picture thread starts. A child window created
    // over there is parented across threads, and Windows will close this
    // window: the editor disappears and the campaign menu comes back.
    previewWindow = retro::createEditorViewport(viewport, !(backgroundTest || (menuTest && !menuShown)));
    preview = std::thread([this] {
      retro::EditorPreviewOptions options;
      options.pack = pack;
      options.control = control;
      options.status = status;
      options.parent = viewport;
      options.sharedWindow = previewWindow;
      options.hidden = backgroundTest || (menuTest && !menuShown);
      options.perf = perfFile;
      options.inProcess = inProcess;
      options.legacyReload = edperf::baseline;
      try {
        retro::runEditorPreview(options);
      } catch (...) {
      }
    });
#else
    std::wstring args =
        L"\"" + engineExecutable().wstring() + L"\" --editor-pack=\"" +
        pack.wstring() + L"\" --editor-control=\"" + control.wstring() +
        L"\" --editor-status=\"" + status.wstring() + L"\" --editor-host=" +
        std::to_wstring(reinterpret_cast<UINT_PTR>(viewport));
    if (backgroundTest)
      args += L" --editor-hidden";
    if (!perfFile.empty())
      args += L" --editor-perf=\"" + perfFile.wstring() + L"\"";
    STARTUPINFOW si{sizeof(si)};
    if (!CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, root.c_str(), &si, &engine))
      throw std::runtime_error("Cannot start the copied native engine");
#endif
  }
  ~Editor() {
    if (autosaveWorker.joinable())
      autosaveWorker.join();
    if (preview.joinable()) {
      try {
        writeControl(true);
      } catch (...) {
      }
      preview.join();
      if (inProcess)
        retro::editorchannel::clear(), retro::editorchannel::active = false;
    } else if (engine.hProcess) {
      try {
        writeControl(true);
      } catch (...) {
      }
      if (WaitForSingleObject(engine.hProcess, 4000) == WAIT_TIMEOUT)
        TerminateProcess(engine.hProcess, 0);
      CloseHandle(engine.hThread);
      CloseHandle(engine.hProcess);
    }
#ifdef RAWMETAL_UNIFIED
    // The picture thread has left. Destroy its window here, on the thread that created it.
    if (previewWindow) {
      retro::destroyEditorViewport(previewWindow);
      previewWindow = nullptr;
    }
#endif
    if (font)
      DeleteObject(font);
    if (titleFont)
      DeleteObject(titleFont);
    DeleteObject(background);
    DeleteObject(panel);
    DeleteObject(fieldBrush);
    DeleteObject(planBrush);
    if (planLayer)
      DeleteObject(planLayer);
  }
};
// The plan, drawn into an off-screen DC the size of the plan's client area.
// Two layers. The map itself (grid, tiles, walls, boxes, things, doors, the
// start) changes only when the document, the floor or the view does, so it is
// drawn once into a cached bitmap; every paint copies it and draws what moves
// with the pointer on top: the hover and pick rings and labels, the ghost of
// the tool, the pop, the hint line and the help line.
static void drawPlan(Editor *e, HDC target, const RECT &r) {
  const auto view = e->planView();
  const float cell = view.cell;
  const int ox = view.ox, oy = view.oy;
  const auto &info = toolInfo[std::clamp(e->tool, 0, ToolCount - 1)];
  auto pen = [&](int width, COLORREF color, int style = PS_SOLID) {
    return CreatePen(style, width, color);
  };
  // What the tool in hand will make, in its plan colour.
  COLORREF ghost = info.kind[0]                      ? ui::kindColor(info.kind)
                   : e->tool == 13                   ? ui::People
                   : e->tool == 25                   ? ui::Start
                   : e->tool == 16 || e->tool == 17  ? ui::Logic
                                                     : ui::Wall;
  if (ghost == ui::Prop)
    ghost = ui::Wall; // ghosts are always bright
  auto faded=[&](COLORREF color,float opacity){auto mix=[&](int a,int b){return int(a*opacity+b*(1-opacity));};return RGB(mix(GetRValue(color),GetRValue(ui::PlanBlack)),mix(GetGValue(color),GetGValue(ui::PlanBlack)),mix(GetBValue(color),GetBValue(ui::PlanBlack)));};
  // A door the way a plan draws one, whichever way it stands (NativeDoor): a
  // sliding door is its leaf line; a swinging door is the opening (dotted), the
  // leaf standing open from its hinge, the arc its latch sweeps, the hinge block.
  auto drawDoor = [&](HDC dc, const NativeDoor &d, COLORREF color, bool dashed) {
    auto at = [&](std::pair<float, float> p) { return POINT{int(ox + p.first * cell), int(oy + p.second * cell)}; };
    auto line = [&](std::pair<float, float> a, std::pair<float, float> b, HPEN p) {
      auto old = SelectObject(dc, p);
      POINT pa = at(a), pb = at(b);
      MoveToEx(dc, pa.x, pa.y, nullptr);
      LineTo(dc, pb.x, pb.y);
      SelectObject(dc, old);
      DeleteObject(p);
    };
    auto a = d.world(d.left, d.line), b = d.world(d.right, d.line);
    // The lock it shows, on the wall beside the latch post: a keycard reader
    // (a box with its card slot and red lamp), a key (a round cylinder) or a
    // power release (a box with its amber lamp).
    if (d.lock >= 1 && d.lock <= 3) {
      auto latch = d.latch(), hinge = d.hinge();
      float run = std::hypot(latch.first - hinge.first, latch.second - hinge.second);
      float ux = run > 0 ? (latch.first - hinge.first) / run : 1.f, uy = run > 0 ? (latch.second - hinge.second) / run : 0.f;
      POINT c = at({latch.first + ux * .3f, latch.second + uy * .3f});
      int s = std::max(4, int(cell * .14f));
      auto body = CreateSolidBrush(color);
      auto dark = CreateSolidBrush(ui::PlanBlack);
      RECT plate{c.x - s, c.y - s, c.x + s + 1, c.y + s + 1};
      if (d.lock == 2) {
        auto oldBrush = SelectObject(dc, body);
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, plate.left, plate.top, plate.right + 1, plate.bottom + 1);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        RECT hole{c.x - 1, c.y - s / 2, c.x + 2, c.y + s / 2 + 1};
        FillRect(dc, &hole, dark);
      } else {
        FillRect(dc, &plate, body);
        RECT slot{c.x - s + 2, c.y - 1, c.x + s - 1, c.y + 1};
        if (d.lock == 1)
          FillRect(dc, &slot, dark);
        auto lamp = CreateSolidBrush(d.lock == 1 ? RGB(0xE0, 0x30, 0x30) : RGB(0xFF, 0x9A, 0x1A));
        RECT bulb{c.x - 2, c.y - s + 1, c.x + 2, c.y - s + 5};
        FillRect(dc, &bulb, lamp);
        DeleteObject(lamp);
      }
      DeleteObject(body);
      DeleteObject(dark);
    }
    if (!d.swinging) {
      line(a, b, dashed ? ui::dashedPen(3, color) : pen(4, color));
      return;
    }
    float width = std::abs(d.right - d.left), open = std::clamp(d.maxOpen, .05f, 1.f);
    // An authored-ajar leaf stands at its own angle; a shut one is drawn open, as plans do.
    float leafAt = d.open > 0 ? std::clamp(d.open, .02f, open) : open;
    auto h = d.hinge();
    auto tip = [&](float t) {
      auto v = d.leafAxis(t);
      return std::pair{h.first + v.first * width, h.second + v.second * width};
    };
    line(a, b, pen(1, color, PS_DOT));
    std::vector<POINT> arc;
    for (int i = 0; i <= 18; ++i)
      arc.push_back(at(tip(open * float(i) / 18.f)));
    auto p = pen(1, color);
    auto old = SelectObject(dc, p);
    Polyline(dc, arc.data(), int(arc.size()));
    SelectObject(dc, old);
    DeleteObject(p);
    // A leaf that breaks down is drawn broken: dashed, with a crack across its middle.
    line(h, tip(leafAt), dashed || d.breakable() ? ui::dashedPen(3, color) : pen(3, color));
    if (d.breakable() && !dashed) {
      auto v = d.leafAxis(leafAt);
      auto mid = std::pair{h.first + v.first * width * .55f, h.second + v.second * width * .55f};
      float nx = -v.second, ny = v.first, k = .12f;
      std::array<POINT, 4> crack{at({mid.first + nx * k, mid.second + ny * k}),
                                 at({mid.first + v.first * .08f, mid.second + v.second * .08f}),
                                 at({mid.first - v.first * .08f, mid.second - v.second * .08f}),
                                 at({mid.first - nx * k, mid.second - ny * k})};
      auto crackPen = pen(2, color);
      auto before = SelectObject(dc, crackPen);
      Polyline(dc, crack.data(), int(crack.size()));
      SelectObject(dc, before);
      DeleteObject(crackPen);
    }
    POINT hp = at(h);
    int s = std::max(3, int(cell * .1f));
    RECT block{hp.x - s, hp.y - s, hp.x + s + 1, hp.y + s + 1};
    auto brush = CreateSolidBrush(color);
    FillRect(dc, &block, brush);
    DeleteObject(brush);
  };
  auto drawBase = [&](HDC dc) {
  FillRect(dc, &r, e->planBrush);
  SetBkMode(dc, TRANSPARENT);
  SelectObject(dc, e->font);
  // A faint endless grid behind the map, like the classic editors.
  {
    auto p = pen(1, RGB(0x12, 0x13, 0x15));
    auto old = SelectObject(dc, p);
    for (float x = std::fmod(float(ox), cell); x < r.right; x += cell)
      MoveToEx(dc, int(x), 0, nullptr), LineTo(dc, int(x), r.bottom);
    for (float y = std::fmod(float(oy), cell); y < r.bottom; y += cell)
      MoveToEx(dc, 0, int(y), nullptr), LineTo(dc, r.right, int(y));
    SelectObject(dc, old);
    DeleteObject(p);
  }
  const int gx0 = int(ox), gy0 = int(oy), gx1 = int(ox + 24 * cell), gy1 = int(oy + 24 * cell);
  // The floor's tiles, read once for this paint.
  char tiles[24][24];
  for (int y = 0; y < 24; y++)
    for (int x = 0; x < 24; x++)
      tiles[y][x] = e->doc.tile(e->map, e->layer, x, y);
  // --perf-baseline: ask the document again for every look, as the plan used to.
  auto activeLayer=e->effectiveFloorView(e->floorLayer(e->layer));
  auto tileAt = [&](int x, int y) {
    if(!activeLayer.visible||activeLayer.opacity<=0||!(activeLayer.mask[size_t(y)]&(1u<<x)))return '_';
    char ch=edperf::baseline?e->doc.tile(e->map,e->layer,x,y):tiles[y][x];
    return e->layer>0&&ch=='.'?'_':ch;
  };
  auto at = [&](int i) { return int(ox + i * cell); };
  auto atY = [&](int i) { return int(oy + i * cell); };
  // Cells: floor a dark slate, walls a grey mass, empty space stays black.
  for (int y = 0; y < 24; y++)
    for (int x = 0; x < 24; x++) {
      char c = tileAt(x, y);
      if (c == '_')
        continue;
      COLORREF color = c == '#'   ? ui::WallFill
                       : c == '=' ? ui::DeckFill
                       : c == '.' ? ui::FloorFill
                                  : ui::CrateFill;
      auto brush = CreateSolidBrush(faded(color,activeLayer.opacity));
      RECT q{at(x), atY(y), at(x + 1), atY(y + 1)};
      FillRect(dc, &q, brush);
      DeleteObject(brush);
    }
  // A crisp dim grid, brighter every four metres.
  for (int i = 0; i <= 24; ++i) {
    COLORREF color = i == 0 || i == 24 ? ui::GridEdge : i % 4 == 0 ? ui::GridMajor : ui::GridMinor;
    auto p = pen(1, color);
    auto old = SelectObject(dc, p);
    MoveToEx(dc, at(i), gy0, nullptr);
    LineTo(dc, at(i), gy1 + 1);
    MoveToEx(dc, gx0, atY(i), nullptr);
    LineTo(dc, gx1 + 1, atY(i));
    SelectObject(dc, old);
    DeleteObject(p);
  }
  // Wall masses outlined in white where they meet open space.
  {
    auto p = pen(2, ui::WallLine);
    auto old = SelectObject(dc, p);
    auto wall = [&](int x, int y) {
      return x >= 0 && y >= 0 && x < 24 && y < 24 && tileAt(x, y) == '#';
    };
    for (int y = 0; y < 24; y++)
      for (int x = 0; x < 24; x++) {
        if (!wall(x, y))
          continue;
        if (!wall(x, y - 1))
          MoveToEx(dc, at(x), atY(y), nullptr), LineTo(dc, at(x + 1), atY(y));
        if (!wall(x, y + 1))
          MoveToEx(dc, at(x), atY(y + 1), nullptr), LineTo(dc, at(x + 1), atY(y + 1));
        if (!wall(x - 1, y))
          MoveToEx(dc, at(x), atY(y), nullptr), LineTo(dc, at(x), atY(y + 1));
        if (!wall(x + 1, y))
          MoveToEx(dc, at(x + 1), atY(y), nullptr), LineTo(dc, at(x + 1), atY(y + 1));
      }
    SelectObject(dc, old);
    DeleteObject(p);
  }
  // The outline being drawn: lime cells, a bright edge.
  if (e->outlineCells()) {
    auto shade = CreateSolidBrush(RGB(0x2A, 0x40, 0x08));
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x)
        if (e->inOutline(x, y)) {
          RECT q{at(x) + 1, atY(y) + 1, at(x + 1), atY(y + 1)};
          FillRect(dc, &q, shade);
        }
    DeleteObject(shade);
    auto p = pen(2, ui::Selected);
    auto old = SelectObject(dc, p);
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 24; ++x) {
        if (!e->inOutline(x, y))
          continue;
        if (!e->inOutline(x, y - 1)) MoveToEx(dc, at(x), atY(y), nullptr), LineTo(dc, at(x + 1), atY(y));
        if (!e->inOutline(x, y + 1)) MoveToEx(dc, at(x), atY(y + 1), nullptr), LineTo(dc, at(x + 1), atY(y + 1));
        if (!e->inOutline(x - 1, y)) MoveToEx(dc, at(x), atY(y), nullptr), LineTo(dc, at(x), atY(y + 1));
        if (!e->inOutline(x + 1, y)) MoveToEx(dc, at(x + 1), atY(y), nullptr), LineTo(dc, at(x + 1), atY(y + 1));
      }
    SelectObject(dc, old);
    DeleteObject(p);
  }
  // Crate tiles keep their letter.
  SelectObject(dc, e->smallFont);
  SetTextColor(dc, ui::CrateInk);
  for (int y = 0; y < 24; y++)
    for (int x = 0; x < 24; x++) {
      char c = tileAt(x, y);
      if (c == '.' || c == '#' || c == '_' || c == '=')
        continue;
      RECT q{at(x), atY(y), at(x + 1), atY(y + 1)};
      wchar_t text[2] = {wchar_t(c), 0};
      DrawTextW(dc, text, 1, &q, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
  SelectObject(dc, e->font);
  IntersectClipRect(dc, ox - 1, oy - 1, int(ox + 24 * cell) + 2, int(oy + 24 * cell) + 2);
  auto [originX, originY] = e->mapOrigin();
  auto outline = [&](float x1, float y1, float x2, float y2, COLORREF color, int width = 2, int style = PS_SOLID) {
    auto p = style == PS_DASH ? ui::dashedPen(width, color) : pen(width, color, style);
    auto oldPen = SelectObject(dc, p);
    auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, int(ox + x1 * cell), int(oy + y1 * cell),
              int(ox + x2 * cell), int(oy + y2 * cell));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(p);
  };
  auto boxed = [&](const std::vector<std::string> &f, int xAt, float dx,
                   float dy, COLORREF color, int width = 2) {
    if (int(f.size()) <= xAt + 3)
      return;
    try {
      float x1 = std::stof(f[size_t(xAt)]) - dx, y1 = std::stof(f[size_t(xAt + 1)]) - dy,
            x2 = std::stof(f[size_t(xAt + 2)]) - dx, y2 = std::stof(f[size_t(xAt + 3)]) - dy;
      // World paint from other maps would draw stray lines across this one.
      if (x2 < 0 || y2 < 0 || x1 > 24 || y1 > 24)
        return;
      outline(x1, y1, x2, y2, color, width);
    } catch (...) {
    }
  };
  for (auto index : e->doc.records("GROUND"))
    boxed(NativeDocument::fields(e->doc.lines[index]), 1, originX, originY, ui::Ground, 1);
  for (auto index : e->doc.records("TERRAIN_FLAT"))
    boxed(NativeDocument::fields(e->doc.lines[index]), 1, originX, originY, ui::Flatten, 1);
  for (auto index : e->doc.records("STRUCT", e->map))if(e->visibleFloorRecord(index))
    boxed(NativeDocument::fields(e->doc.lines[index]), 2, 0, 0, faded(ui::Wall,activeLayer.opacity));
  for (auto index : e->doc.records("HAZARD", e->map))
    boxed(NativeDocument::fields(e->doc.lines[index]), 3, 0, 0, ui::Hazard);
  // Reference contours remain visible over the active floor without adding
  // lower-floor tiles to the editable selection or changing the map.
  if(e->floorReferenceEnabled())for(auto reference:e->floorLayers()){
    reference=e->effectiveFloorView(reference);
    if(reference.id==e->layer||!reference.visible||reference.opacity<=0||reference.elevation>=e->layerElevation())continue;
    auto p=ui::dashedPen(1,RGB(74,132,146));auto old=SelectObject(dc,p);
    auto tile=[&](int x,int y){if(x<0||x>=24||y<0||y>=24||!(reference.mask[size_t(y)]&(1u<<x)))return '_';return e->doc.tile(e->map,reference.id,x,y);};
    auto floor=[&](int x,int y){char ch=tile(x,y);return ch=='='||(reference.id==0&&ch!='#'&&ch!='_');};
    for(int y=0;y<24;++y)for(int x=0;x<24;++x){char ch=tile(x,y);if(ch!='#'&&!floor(x,y))continue;
      auto same=[&](int xx,int yy){return ch=='#'?tile(xx,yy)=='#':floor(xx,yy);};
      if(!same(x-1,y)){MoveToEx(dc,int(ox+x*cell),int(oy+y*cell),nullptr);LineTo(dc,int(ox+x*cell),int(oy+(y+1)*cell));}
      if(!same(x+1,y)){MoveToEx(dc,int(ox+(x+1)*cell),int(oy+y*cell),nullptr);LineTo(dc,int(ox+(x+1)*cell),int(oy+(y+1)*cell));}
      if(!same(x,y-1)){MoveToEx(dc,int(ox+x*cell),int(oy+y*cell),nullptr);LineTo(dc,int(ox+(x+1)*cell),int(oy+y*cell));}
      if(!same(x,y+1)){MoveToEx(dc,int(ox+x*cell),int(oy+(y+1)*cell),nullptr);LineTo(dc,int(ox+(x+1)*cell),int(oy+(y+1)*cell));}
    }SelectObject(dc,old);DeleteObject(p);
  }
  auto ring = [&](int x0, int y0, int x1, int y1, int width, COLORREF color) {
    auto p = pen(width, color);
    auto o = SelectObject(dc, p), b = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, x0, y0, x1, y1);
    SelectObject(dc, b);
    SelectObject(dc, o);
    DeleteObject(p);
  };
  // Things: an outline and a solid centre in their kind's colour. The
  // picked one is lime with handles; the one under the pointer gets white
  // (both drawn over the map below).
  for (auto index : e->objects()) {
    if(!e->visibleFloorRecord(index))continue;
    auto object = *e->handleFor(index);
    auto f = NativeDocument::fields(e->doc.lines[index]);
    int x = int(ox + object.x * cell), y = int(oy + object.y * cell);
    int hw = int(std::max(4.f, object.width * cell * .5f)),
        hd = int(std::max(4.f, object.depth * cell * .5f));
    // Emitters and film cameras draw their own glyphs (drawEmitterGlyphs, drawFilmCameras).
    if (f[0] == "EMITTER" || f[0] == "# EDITOR_FILM")
      continue;
    COLORREF color = faded(ui::kindColor(f[0]),e->effectiveFloorView(e->floorLayer(e->recordFloor(index))).opacity);
    if (object.width >= 23.5f && object.depth >= 23.5f)
      color = RGB(GetRValue(color) * 3 / 10, GetGValue(color) * 3 / 10, GetBValue(color) * 3 / 10);
    bool dotted = f[0] == "ZONE" || f[0] == "SCRIPT_EVENT";
    auto p = pen(1, color, dotted ? PS_DOT : PS_SOLID);
    auto oldPen = SelectObject(dc, p),
         oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, x - hw, y - hd, x + hw + 1, y + hd + 1);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(p);
    auto dot = CreateSolidBrush(color);
    RECT centre{x - 2, y - 2, x + 3, y + 3};
    FillRect(dc, &centre, dot);
    DeleteObject(dot);
    // A sign is drawn as its plate in its own colour, with a tick toward the side it faces.
    if (f[0] == "SIGN" && f.size() > 10) {
      float sx = Editor::number(f[2], 0), sy = Editor::number(f[3], 0), half = Editor::number(f[5], 1) * .5f, yaw = Editor::number(f[7], 0);
      unsigned accent = unsigned(Editor::number(f[10], 0));
      COLORREF plate = faded(RGB((accent >> 16) & 255, (accent >> 8) & 255, accent & 255), e->effectiveFloorView(e->floorLayer(e->recordFloor(index))).opacity);
      float ax = std::cos(yaw), ay = -std::sin(yaw), nx = std::sin(yaw), ny = std::cos(yaw);
      auto bar = pen(std::max(3, int(cell * .12f)), plate);
      auto old = SelectObject(dc, bar);
      MoveToEx(dc, int(ox + (sx - ax * half) * cell), int(oy + (sy - ay * half) * cell), nullptr);
      LineTo(dc, int(ox + (sx + ax * half) * cell), int(oy + (sy + ay * half) * cell));
      SelectObject(dc, old);
      DeleteObject(bar);
      auto tick = pen(1, ui::PlanText);
      old = SelectObject(dc, tick);
      MoveToEx(dc, int(ox + sx * cell), int(oy + sy * cell), nullptr);
      LineTo(dc, int(ox + (sx + nx * .35f) * cell), int(oy + (sy + ny * .35f) * cell));
      SelectObject(dc, old);
      DeleteObject(tick);
    }
  }
  // The air's emitters and the film cameras (NativeEmitterTool.inc, NativeFilmTool.inc).
  e->drawEmitterGlyphs(dc, ox, oy, cell);
  e->drawFilmCameras(dc, ox, oy, cell);
  for (auto index : e->doc.records("DOOR", e->map)) {
    if(!e->visibleFloorRecord(index))continue;
    auto f = NativeDocument::fields(e->doc.lines[index]);
    if (auto door = nativeDoor(f))
      drawDoor(dc, *door, faded(ui::Door, activeLayer.opacity), false);
  }
  // A creature that comes out for a person: a dashed line in its colour from
  // the creature to where that person stands, ringed at their end.
  {
    std::map<int, std::pair<float, float>> standing; // actor index -> first pose
    for (auto index : e->doc.records("ACTOR_KEY", e->map)) {
      auto f = NativeDocument::fields(e->doc.lines[index]);
      if (f.size() > 5)
        standing.emplace(int(Editor::number(f[2], -1)), std::pair{Editor::number(f[4], 0), Editor::number(f[5], 0)});
    }
    for (auto index : e->doc.records("CREATURE", e->map)) {
      if (!e->visibleFloorRecord(index))
        continue;
      auto f = NativeDocument::fields(e->doc.lines[index]);
      if (f.size() < 11 || f[10].empty())
        continue;
      auto it = standing.find(int(Editor::number(f[10], -1)));
      if (it == standing.end())
        continue;
      int x0 = int(ox + Editor::number(f[3], 0) * cell), y0 = int(oy + Editor::number(f[4], 0) * cell);
      int x1 = int(ox + it->second.first * cell), y1 = int(oy + it->second.second * cell);
      auto p = ui::dashedPen(2, ui::Creature);
      auto old = SelectObject(dc, p);
      auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
      MoveToEx(dc, x0, y0, nullptr);
      LineTo(dc, x1, y1);
      int ringSize = std::max(6, int(cell * .45f));
      Ellipse(dc, x1 - ringSize, y1 - ringSize, x1 + ringSize + 1, y1 + ringSize + 1);
      SelectObject(dc, oldBrush);
      SelectObject(dc, old);
      DeleteObject(p);
    }
  }
  // The player start: a bright white diamond with a green heart.
  for (auto index : e->doc.records("MAP", e->map)) {
    auto f = NativeDocument::fields(e->doc.lines[index]);
    if (f.size() < 7)
      continue;
    try {
      int x = int(ox + std::stof(f[5]) * cell),
          y = int(oy + std::stof(f[6]) * cell);
      POINT diamond[4] = {{x, y - 10}, {x + 10, y}, {x, y + 10}, {x - 10, y}};
      auto brush = CreateSolidBrush(ui::Start);
      auto p = pen(2, ui::PlanBlack);
      auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, p);
      Polygon(dc, diamond, 4);
      SelectObject(dc, oldPen);
      SelectObject(dc, oldBrush);
      DeleteObject(p);
      DeleteObject(brush);
      auto heart = CreateSolidBrush(ui::People);
      RECT inner{x - 3, y - 3, x + 4, y + 4};
      FillRect(dc, &inner, heart);
      DeleteObject(heart);
    } catch (...) {
    }
  }
  SelectClipRgn(dc, nullptr);
  };
  // The cached map layer, redrawn when the document (its version counts every
  // change), the map, the floor, the view, the size or the drawn outline changes.
  // --perf-baseline draws it on every paint, as before.
  HDC dc = target;
  {
    std::uint64_t outlineHash = 1469598103934665603ull;
    for (char c : e->outline)
      outlineHash = (outlineHash ^ std::uint64_t(static_cast<unsigned char>(c))) * 1099511628211ull;
    outlineHash ^= e->outlineCells() ? 1 : 2;
    Editor::PlanLayerKey key{e->doc.version, e->doc.lines.size(), e->map, e->layer, r.right, r.bottom, cell, ox, oy, outlineHash};
    if (edperf::baseline)
      drawBase(dc);
    else {
      HDC layer = CreateCompatibleDC(target);
      if (!e->planLayer || !(key == e->planLayerKey)) {
        if (!e->planLayer || e->planLayerKey.width != r.right || e->planLayerKey.height != r.bottom) {
          if (e->planLayer)
            DeleteObject(e->planLayer);
          e->planLayer = CreateCompatibleBitmap(target, std::max(1L, r.right), std::max(1L, r.bottom));
        }
        auto old = SelectObject(layer, e->planLayer);
        drawBase(layer);
        SelectObject(layer, old);
        e->planLayerKey = key;
      }
      auto old = SelectObject(layer, e->planLayer);
      BitBlt(dc, 0, 0, r.right, r.bottom, layer, 0, 0, SRCCOPY);
      SelectObject(layer, old);
      DeleteDC(layer);
    }
  }
  SetBkMode(dc, TRANSPARENT);
  SelectObject(dc, e->font);
  IntersectClipRect(dc, ox - 1, oy - 1, int(ox + 24 * cell) + 2, int(oy + 24 * cell) + 2);
  auto outline = [&](float x1, float y1, float x2, float y2, COLORREF color, int width = 2, int style = PS_SOLID) {
    auto p = style == PS_DASH ? ui::dashedPen(width, color) : pen(width, color, style);
    auto oldPen = SelectObject(dc, p);
    auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, int(ox + x1 * cell), int(oy + y1 * cell),
              int(ox + x2 * cell), int(oy + y2 * cell));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(p);
  };
  auto ring = [&](int x0, int y0, int x1, int y1, int width, COLORREF color) {
    auto p = pen(width, color);
    auto o = SelectObject(dc, p), b = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, x0, y0, x1, y1);
    SelectObject(dc, b);
    SelectObject(dc, o);
    DeleteObject(p);
  };
  // The picked thing and the one under the pointer, where the map layer drew them.
  int pickedX = 0, pickedY = 0, pickedW = 0, pickedD = 0, hoverX = 0, hoverY = 0;
  std::wstring pickedName, hoverName;
  auto locate = [&](int index, int &x, int &y, int &hw, int &hd, std::wstring &name) {
    if (index < 0 || index >= int(e->doc.lines.size()))
      return false;
    auto object = e->handleFor(size_t(index));
    if (!object)
      return false;
    x = int(ox + object->x * cell), y = int(oy + object->y * cell);
    hw = int(std::max(4.f, object->width * cell * .5f)), hd = int(std::max(4.f, object->depth * cell * .5f));
    name = plainKind(NativeDocument::fields(e->doc.lines[size_t(index)])[0]);
    return true;
  };
  locate(e->record, pickedX, pickedY, pickedW, pickedD, pickedName);
  if (e->hoverRecord != e->record) {
    int hw = 0, hd = 0;
    if (locate(e->hoverRecord, hoverX, hoverY, hw, hd, hoverName))
      ring(hoverX - hw - 3, hoverY - hd - 3, hoverX + hw + 4, hoverY + hd + 4, 2, ui::Hover);
  }
  // Labels sit on a black plate so they read over any line.
  auto tag = [&](int x, int y, const std::wstring &text, COLORREF color) {
    SelectObject(dc, e->buttonFont);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
    RECT plate{x - 3, y - 1, x + size.cx + 3, y + size.cy + 1};
    FillRect(dc, &plate, e->planBrush);
    SetTextColor(dc, color);
    textAt(dc, x, y, text.c_str());
    SelectObject(dc, e->font);
  };
  if (!hoverName.empty())
    tag(hoverX + 10, hoverY + 8, hoverName, ui::Hover);
  if (!pickedName.empty()) {
    ring(pickedX - pickedW - 3, pickedY - pickedD - 3, pickedX + pickedW + 4, pickedY + pickedD + 4, 3,
         ui::Selected);
    tag(pickedX + pickedW + 10, pickedY + 8, pickedName, ui::Selected);
  }
  for(int index:e->selectedRecords)if(index!=e->record){int x=0,y=0,hw=0,hd=0;std::wstring name;
    if(locate(index,x,y,hw,hd,name))ring(x-hw-2,y-hd-2,x+hw+3,y+hd+3,2,ui::Selected);}
  if(e->floorTransformSelection){auto b=e->tileBounds();outline(float(b[0]),float(b[1]),float(b[2]),float(b[3]),ui::Selected,2);
    auto brush=CreateSolidBrush(ui::Selected);for(int x:{b[0],b[2]})for(int y:{b[1],b[3]}){int px=int(ox+x*cell),py=int(oy+y*cell);RECT q{px-5,py-5,px+6,py+6};FillRect(dc,&q,brush);}DeleteObject(brush);}
  if(e->record>=0&&e->selectedRecords.size()<=1){
    auto f=NativeDocument::fields(e->doc.lines[size_t(e->record)]);int schema=e->schemaFor(f);
    if(schema>=0){auto corners=nativeCorners(e->schemas[size_t(schema)].fields,f);auto brush=CreateSolidBrush(ui::Selected);
      for(size_t i=0;i<corners.size();++i){auto test=f;if(!resizeNative(test,e->schemas[size_t(schema)].fields,int(i),corners[i].first,corners[i].second,0))continue;
        int x=int(ox+corners[i].first*cell),y=int(oy+corners[i].second*cell);RECT q{x-5,y-5,x+6,y+6};FillRect(dc,&q,brush);}
      DeleteObject(brush);}}
  if(e->selectingArea)outline(std::min(e->areaX,e->areaEndX),std::min(e->areaY,e->areaEndY),std::max(e->areaX,e->areaEndX),std::max(e->areaY,e->areaEndY),ui::Selected,1,PS_DASH);
  // Ghost of what the tool will make: the stretched box or line while
  // dragging (here or in 3D), else a marker under the pointer.
  if (e->tool != 0 && (e->hoverInside || e->drag || e->viewRect)) {
    char shape = info.shape;
    bool stretching = (e->drag || e->viewRect) && !e->brushing && e->dragTool();
    float hx = e->hoverX, hy = e->hoverY;
    auto tileBox = [&](float ax, float ay, float bx, float by) {
      float x0 = std::floor(std::min(ax, bx)), y0 = std::floor(std::min(ay, by));
      float x1 = std::floor(std::max(ax, bx)) + 1, y1 = std::floor(std::max(ay, by)) + 1;
      outline(x0, y0, x1, y1, ghost, 2, PS_DASH);
      if (shape == 'R' && e->autoDetail)
        outline(x0 + 1, y0 + 1, x1 - 1, y1 - 1, ui::Lamp, 1, PS_DOT);
    };
    if (shape == 'd')
      // The door this click or drag fits, in the wall's direction, as R has turned it.
      drawDoor(dc, stretching ? e->plannedDoor(e->dragFX, e->dragFY, hx, hy) : e->plannedDoor(hx, hy, hx, hy), ghost, true);
    else if (stretching && (shape == 't' || shape == 'R' || shape == 'w' || shape == 'c'))
      tileBox(e->dragFX, e->dragFY, hx, hy);
    else if (stretching && shape == 'l') {
      auto p = ui::dashedPen(3, ghost);
      auto old = SelectObject(dc, p);
      MoveToEx(dc, int(ox + e->snapped(e->dragFX) * cell), int(oy + e->dragFY * cell), nullptr);
      LineTo(dc, int(ox + e->snapped(hx) * cell), int(oy + e->snapped(hy) * cell));
      SelectObject(dc, old);
      DeleteObject(p);
    } else if (stretching)
      outline(e->snapped(std::min(e->dragFX, hx)), e->snapped(std::min(e->dragFY, hy)),
              e->snapped(std::max(e->dragFX, hx)), e->snapped(std::max(e->dragFY, hy)), ghost, 2, PS_DASH);
    else if (shape == 't' || shape == 'v' || shape == 'w' || shape == 'R' || shape == 'c' || shape == 'r' || shape == 'd' ||
             shape == 'F' || shape == 'H') {
      if (shape == 'v' && e->regionStamp.width > 0) {
        int sw = e->rotation % 2 ? e->regionStamp.height : e->regionStamp.width;
        int sh = e->rotation % 2 ? e->regionStamp.width : e->regionStamp.height;
        outline(std::floor(hx), std::floor(hy), std::floor(hx) + sw, std::floor(hy) + sh, ghost, 2, PS_DASH);
      } else
        tileBox(hx, hy, hx, hy);
    } else {
      float gw = .6f, gd = .6f;
      if (e->tool == 8 || (e->tool == 14 && e->listIndex(e->clutterList) == 7)) {
        int slot = e->selectedModel();
        if (slot >= 0 && slot < int(std::size(retro::model::size)))
          gw = std::max(.3f, retro::model::size[slot][0]), gd = std::max(.3f, retro::model::size[slot][1]);
      }
      float gx = e->snapped(hx), gy = e->snapped(hy);
      outline(gx - gw * .5f, gy - gd * .5f, gx + gw * .5f, gy + gd * .5f, ghost, 2, PS_DASH);
      SetTextColor(dc, ghost);
      SelectObject(dc, e->glyphFont);
      textAt(dc, int(ox + (gx + gw * .5f) * cell) + 5, int(oy + gy * cell) - 12, info.glyph);
      SelectObject(dc, e->font);
    }
  }
  // The pop: a ring that grows and fades where something just landed.
  if (ULONGLONG age = GetTickCount64() - e->popAt; e->popAt && age < 380) {
    float t = age / 380.f, fade = 1 - t;
    COLORREF c = RGB(int(GetRValue(e->popColor) * fade), int(GetGValue(e->popColor) * fade),
                     int(GetBValue(e->popColor) * fade));
    int px = int(ox + e->popX * cell), py = int(oy + e->popY * cell), radius = int(6 + 26 * std::sqrt(t));
    auto p = pen(std::max(1, int(4 * fade)), c);
    auto o = SelectObject(dc, p), b = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(dc, px - radius, py - radius, px + radius + 1, py + radius + 1);
    SelectObject(dc, b);
    SelectObject(dc, o);
    DeleteObject(p);
  }
  SelectClipRgn(dc, nullptr);
  {
    RECT strip{0, 0, r.right, 28};
    FillRect(dc, &strip, e->planBrush);
    SetTextColor(dc, e->tool == 0 ? ui::Selected : ghost);
    SelectObject(dc, e->glyphFont);
    textAt(dc, 10, 2, info.glyph);
    SelectObject(dc, e->buttonFont);
    SetTextColor(dc, ui::PlanText);
    std::wstring hint = std::wstring(info.name) + L":  ";
    textAt(dc, 40, 6, hint.c_str());
    SIZE named{};
    GetTextExtentPoint32W(dc, hint.c_str(), int(hint.size()), &named);
    SelectObject(dc, e->font);
    SetTextColor(dc, RGB(0xA8, 0xA2, 0x96));
    RECT rest{40 + named.cx, 6, r.right - 8, 28};
    DrawTextW(dc, info.hint, -1, &rest, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
  }
  {
    // How to look around, always in sight at the foot of the plan.
    static constexpr wchar_t help[] = L"Wheel: zoom     Middle-drag or Space-drag: move     Home: fit";
    SelectObject(dc, e->smallFont);
    SIZE size{};
    GetTextExtentPoint32W(dc, help, int(std::size(help)) - 1, &size);
    RECT foot{0, r.bottom - size.cy - 8, r.right, r.bottom};
    FillRect(dc, &foot, e->planBrush);
    SetTextColor(dc, RGB(0x8A, 0x84, 0x78));
    textAt(dc, 12, r.bottom - size.cy - 5, help);
    SelectObject(dc, e->font);
  }
}
static LRESULT CALLBACK planProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    e = static_cast<Editor *>(
        reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (!e)
    return DefWindowProcW(h, m, w, l);
  std::optional<edperf::Scope> perfScope;
  if (edperf::on && (m == WM_PAINT || m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_LBUTTONUP))
    perfScope.emplace(m == WM_PAINT ? "plan_paint" : m == WM_MOUSEMOVE ? "plan_mouse_move" : m == WM_LBUTTONDOWN ? "plan_press" : "plan_release");
  try {
    RECT r{};
    GetClientRect(h, &r);
    const auto view = e->planView();
    const float cell = view.cell;
    const int ox = view.ox, oy = view.oy;
    auto worldPoint = [&](LPARAM value) {
      return std::pair{
          std::clamp((GET_X_LPARAM(value) - ox) / cell, 0.f, 23.999f),
          std::clamp((GET_Y_LPARAM(value) - oy) / cell, 0.f, 23.999f)};
    };
    auto inside = [&](LPARAM value) {
      return GET_X_LPARAM(value) >= ox && GET_Y_LPARAM(value) >= oy &&
             GET_X_LPARAM(value) < ox + 24 * cell && GET_Y_LPARAM(value) < oy + 24 * cell;
    };
    if (m == WM_ERASEBKGND)
      return 1;
    if (m == WM_MOUSEWHEEL) {
      POINT at{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
      ScreenToClient(h, &at);
      e->zoomPlan(GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1.25f : 0.8f, at.x, at.y);
      return 0;
    }
    bool spaceHeld = !e->backgroundTest && (GetKeyState(VK_SPACE) & 0x8000) != 0;
    if (m == WM_MBUTTONDOWN || (m == WM_LBUTTONDOWN && spaceHeld)) {
      e->panning = true;
      e->panFrom = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
      SetCapture(h);
      SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
      return 0;
    }
    if (e->panning && m == WM_MOUSEMOVE) {
      e->planPanX += float(GET_X_LPARAM(l) - e->panFrom.x);
      e->planPanY += float(GET_Y_LPARAM(l) - e->panFrom.y);
      e->panFrom = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
      SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
      InvalidateRect(h, nullptr, FALSE);
      return 0;
    }
    if (e->panning && (m == WM_MBUTTONUP || m == WM_LBUTTONUP || m == WM_CAPTURECHANGED)) {
      e->panning = false;
      if (m != WM_CAPTURECHANGED && GetCapture() == h)
        ReleaseCapture();
      return 0;
    }
    if (m == WM_RBUTTONDOWN) {
      // Right click is "put the tool down": cancel, then back to Select.
      if (e->objectDrag)
        e->finishDrag(true);
      e->drag = e->brushing = false;
      if (GetCapture() == h)
        ReleaseCapture();
      if (e->tool != 0) {
        e->tool = 0;
        e->repaintTools();
        e->message(toolInfo[0].hint);
      }
      InvalidateRect(h, nullptr, FALSE);
      return 0;
    }
    if (m == WM_MOUSELEAVE) {
      e->hoverInside = false;
      e->hoverRecord = -1;
      InvalidateRect(h, nullptr, FALSE);
      return 0;
    }
    if (m == WM_LBUTTONDOWN) {
      if (!inside(l))
        return 0;
      auto [x, y] = worldPoint(l);
      if (!e->roomAt(e->cellOf(x), e->cellOf(y)).empty()) {
        bool changed = e->lastRoomX < 0;
        e->lastRoomX = e->cellOf(x), e->lastRoomY = e->cellOf(y);
        if (changed)
          e->relayout();
      }
      e->anchor = {int(x), int(y)};
      e->dragFX = x, e->dragFY = y;
      e->drag = true;
      if (!e->backgroundTest) {
        SetFocus(h);
        SetCapture(h);
      }
      if (e->brushTool()) {
        e->remember();
        e->brushing = true;
        e->brushCell(x, y);
        return 0;
      }
      if (e->tool == 0) {
        const unsigned laidOut = e->layoutCount;
        bool extend=(w & MK_SHIFT)!=0;
        int corner=extend?-1:e->cornerAt(x,y);
        int hit=corner>=0?e->record:e->hitObject(x,y);
        if(hit<0){
          e->selectingArea=true;e->areaAdd=extend;e->areaX=e->areaEndX=x;e->areaY=e->areaEndY=y;
          return 0;
        }
        if(corner<0){
          if(!extend&&e->selected(hit))e->selectRecord(hit,true);
          else e->selectionPick(hit,extend);
        }
        if(extend&&!e->selected(hit)){e->drag=false;return 0;}
        e->resizeHandle=corner;
        if (hit >= 0) {
          e->objectDrag = true;
          e->objectMoved = false;
          e->dragX = x;
          e->dragY = y;
          e->objectOriginal = e->doc.lines[size_t(hit)];
          e->objectFields = NativeDocument::fields(e->objectOriginal);
          e->captureDragState();
          e->lastDragPublish = GetTickCount64();
          e->viewDrag = false;
          e->captureMates();
        }
        if (e->layoutCount == laidOut)
          e->layout();
      }
      return 0;
    }
    if (m == WM_MOUSEMOVE) {
      if (!e->hoverInside && !e->backgroundTest) {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, h, 0};
        TrackMouseEvent(&track);
      }
      e->hoverInside = inside(l);
      auto [x, y] = worldPoint(l);
      e->hoverX = x, e->hoverY = y;
      if(e->drag&&e->selectingArea){e->areaEndX=x;e->areaEndY=y;InvalidateRect(h,nullptr,FALSE);return 0;}
      if (e->drag && e->brushing) {
        e->brushCell(x, y);
        return 0;
      }
      if (e->drag && e->objectDrag) {
        try {
          if(e->resizeHandle>=0)e->applyCornerDrag(x,y);
          else {auto original=nativeHandle(e->schemas[size_t(e->kind)].fields,e->objectFields);if(!original)return 0;
            float dx=x-e->dragX,dy=y-e->dragY;
            if(e->snap>0){dx=std::round(dx/e->snap)*e->snap;dy=std::round(dy/e->snap)*e->snap;}
            e->applyDragDelta(dx,dy);}

          if (e->objectMoved && e->liveTurn()) {
            e->publish();
            e->lastDragPublish = GetTickCount64();
          }
        } catch (const std::exception &error) {
          e->message(wide(error.what()));
          e->finishDrag(true);
        }
      } else if (e->tool == 0) {
        int hover = e->hoverInside ? e->hitObject(x, y) : -1;
        if (hover == e->hoverRecord && !e->drag)
          return 0;
        e->hoverRecord = hover;
      }
      InvalidateRect(h, nullptr, FALSE);
      return 0;
    }
    if (m == WM_CAPTURECHANGED && (e->objectDrag || e->brushing) && !e->backgroundTest) {
      if (e->objectDrag)
        e->finishDrag(false);
      if (e->brushing)
        e->brushing = false, e->publish();
      e->drag = false;
      return 0;
    }
    if (m == WM_LBUTTONUP && e->drag) {
      e->drag = false;
      if (!e->backgroundTest && GetCapture() == h)
        ReleaseCapture();
      if(e->selectingArea){
        e->selectingArea=false;auto [x,y]=worldPoint(l);
        if(std::hypot(x-e->areaX,y-e->areaY)>.2f)e->selectArea(e->areaX,e->areaY,x,y,e->areaAdd);
        else if(!e->areaAdd){auto maps=e->doc.records("MAP",e->map);e->selectRecord(maps.empty()?-1:int(maps.front()));}
        InvalidateRect(h,nullptr,FALSE);return 0;
      }
      if (e->brushing) {
        e->brushing = false;
        e->publish();
        return 0;
      }
      if (e->objectDrag) {
        e->finishDrag(false);
        return 0;
      }
      auto [x, y] = worldPoint(l);
      try {
        e->gesture(e->dragFX, e->dragFY, x, y);
      } catch (const std::exception &x) {
        e->message(wide(x.what()));
      }
      InvalidateRect(h, nullptr, FALSE);
      return 0;
    }
    if (m == WM_PRINTCLIENT) {drawPlan(e,reinterpret_cast<HDC>(w),r);return 0;}
    if (m == WM_PAINT) {
      PAINTSTRUCT ps{};
      HDC target = BeginPaint(h, &ps);
      // Drawn off screen first: hover repaints must not flicker.
      HDC dc = CreateCompatibleDC(target);
      HBITMAP canvas = CreateCompatibleBitmap(target, std::max(1L, r.right), std::max(1L, r.bottom));
      auto oldCanvas = SelectObject(dc, canvas);
      drawPlan(e, dc, r);
      BitBlt(target, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
      SelectObject(dc, oldCanvas);
      DeleteObject(canvas);
      DeleteDC(dc);
      EndPaint(h, &ps);
      return 0;
    }
    return DefWindowProcW(h, m, w, l);
  } catch (const std::exception &error) {
    e->objectDrag = false;
    e->drag = false;
    if (!e->backgroundTest && GetCapture() == h)
      ReleaseCapture();
    e->message(wide(error.what()));
    return 0;
  }
}
// The picture tiles of the parts browser and the paint palette.
static LRESULT CALLBACK gridProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *g = reinterpret_cast<parts::Grid *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    g = static_cast<parts::Grid *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g));
  }
  if (!g || !g->hwnd)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_ERASEBKGND:
    return 1;
  case WM_SIZE:
    g->layoutSpots();
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(h, &ps);
    RECT r{};
    GetClientRect(h, &r);
    g->paint(dc, r, ui::Paper, ui::Field, ui::PaperEdge, ui::SageEdge, ui::Glow, ui::Ink, ui::InkSoft);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    int at = g->at(GET_X_LPARAM(l), GET_Y_LPARAM(l));
    if (at != g->hot) {
      g->hot = at;
      InvalidateRect(h, nullptr, FALSE);
      if (at >= 0 && g->onHover)
        g->onHover(at);
      TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, h, 0};
      TrackMouseEvent(&track);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    g->hot = -1;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN: {
    SetFocus(h);
    int at = g->at(GET_X_LPARAM(l), GET_Y_LPARAM(l));
    if (at >= 0 && g->onPick) {
      sfx::play(sfx::Click, .6f, .05f);
      g->onPick(at, m == WM_RBUTTONDOWN);
    }
    return 0;
  }
  case WM_MOUSEWHEEL:
    g->scroll -= GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 70;
    g->layoutSpots();
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_VSCROLL: {
    SCROLLINFO info{sizeof(info), SIF_ALL};
    GetScrollInfo(h, SB_VERT, &info);
    switch (LOWORD(w)) {
    case SB_LINEUP: g->scroll -= 40; break;
    case SB_LINEDOWN: g->scroll += 40; break;
    case SB_PAGEUP: g->scroll -= int(info.nPage); break;
    case SB_PAGEDOWN: g->scroll += int(info.nPage); break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: g->scroll = info.nTrackPos; break;
    }
    g->layoutSpots();
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  }
  }
  return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK formProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    e = static_cast<Editor *>(
        reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (e && m == WM_ERASEBKGND) {
    RECT bounds{};
    GetClientRect(h, &bounds);
    FillRect(reinterpret_cast<HDC>(w), &bounds, e->panel);
    return 1;
  }
  if (e && m == WM_PAINT) {
    PAINTSTRUCT ps{};
    auto dc = BeginPaint(h, &ps);
    FillRect(dc, &ps.rcPaint, e->panel);
    EndPaint(h, &ps);
    return 0;
  }
  if (e && (m == WM_VSCROLL || m == WM_MOUSEWHEEL)) {
    SCROLLINFO s{sizeof(s), SIF_ALL};
    GetScrollInfo(h, SB_VERT, &s);
    int p = s.nPos;
    if (m == WM_MOUSEWHEEL)
      p -= GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 72;
    else
      switch (LOWORD(w)) {
      case SB_LINEUP:
        p -= 32;
        break;
      case SB_LINEDOWN:
        p += 32;
        break;
      case SB_PAGEUP:
        p -= int(s.nPage);
        break;
      case SB_PAGEDOWN:
        p += int(s.nPage);
        break;
      case SB_THUMBTRACK:
        p = s.nTrackPos;
        break;
      }
    e->formScroll = std::clamp(p, 0, std::max(0, s.nMax - int(s.nPage)));
    e->fieldLayout();
    return 0;
  }
  if (e && (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLOREDIT)) {
    bool edit = m == WM_CTLCOLOREDIT;
    SetTextColor(reinterpret_cast<HDC>(w), edit ? ui::Ink : ui::InkSoft);
    SetBkColor(reinterpret_cast<HDC>(w), edit ? ui::Field : ui::Paper);
    return reinterpret_cast<LRESULT>(edit ? e->fieldBrush : e->panel);
  }
  // A changed field keeps its value as soon as you leave it: no Apply step.
  // Applied at once (the quiet path keeps the boxes), before a click elsewhere
  // can pick something new and refill them.
  if (e && m == WM_COMMAND && HIWORD(w) == EN_KILLFOCUS && e->record >= 0)
    try {
      e->apply(true);
    } catch (const std::exception &error) {
      e->message(wide(error.what()));
    }
  return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK shelfProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    e = static_cast<Editor *>(
        reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (e && m == WM_ERASEBKGND) {
    RECT bounds{};
    GetClientRect(h, &bounds);
    FillRect(reinterpret_cast<HDC>(w), &bounds, e->panel);
    return 1;
  }
  if (e && (m == WM_VSCROLL || m == WM_MOUSEWHEEL)) {
    int p = e->toolScroll;
    if (m == WM_MOUSEWHEEL)
      p -= GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 52;
    else
      switch (LOWORD(w)) {
      case SB_LINEUP:
        p -= 26;
        break;
      case SB_LINEDOWN:
        p += 26;
        break;
      case SB_PAGEUP:
        p -= 120;
        break;
      case SB_PAGEDOWN:
        p += 120;
        break;
      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
        p = HIWORD(w);
        break;
      }
    e->toolScroll = p;
    e->layoutTools();
    return 0;
  }
  if (m == WM_COMMAND || m == WM_DRAWITEM)
    return SendMessageW(GetParent(h), m, w, l);
  return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK mainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    e = static_cast<Editor *>(
        reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    e->window = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (!e)
    return DefWindowProcW(h, m, w, l);
  std::optional<edperf::Scope> perfScope;
  if (edperf::on && (m == WM_COMMAND || m == WM_TIMER || m == WM_SIZE || m == WM_PAINT || m == WM_HSCROLL))
    perfScope.emplace(m == WM_COMMAND ? "window_command" : m == WM_TIMER ? "window_timer" : m == WM_SIZE ? "window_layout" : m == WM_PAINT ? "window_paint" : "window_slider");
  try {
    switch (m) {
    case WM_SIZE:
      if (e->mapList)
        e->layout();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_CTLCOLORSTATIC:
      SetTextColor(reinterpret_cast<HDC>(w), ui::Ink);
      SetBkColor(reinterpret_cast<HDC>(w), ui::Paper);
      return reinterpret_cast<LRESULT>(e->panel);
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
      SetTextColor(reinterpret_cast<HDC>(w), ui::Ink);
      SetBkColor(reinterpret_cast<HDC>(w), ui::Field);
      return reinterpret_cast<LRESULT>(e->fieldBrush);
    case WM_MEASUREITEM: {
      auto *measure = reinterpret_cast<MEASUREITEMSTRUCT *>(l);
      if (measure->CtlType == ODT_COMBOBOX || measure->CtlType == ODT_LISTBOX)
        measure->itemHeight = measure->CtlType == ODT_COMBOBOX && measure->itemID == UINT(-1) ? 22 : 24;
      return TRUE;
    }
    case WM_DRAWITEM: {
      auto *d = reinterpret_cast<DRAWITEMSTRUCT *>(l);
      HDC dc = d->hDC;
      if(d->CtlID==LayerPreview){e->drawLayerPreview(dc,d->rcItem);return TRUE;}
      if (d->CtlType == ODT_COMBOBOX || d->CtlType == ODT_LISTBOX) {
        ui::drawListRow(d, e->font);
        return TRUE;
      }
      int id = int(d->CtlID);
      bool isTool = id >= ToolBase && id < ToolBase + ToolCount;
      bool active =
          id == Walk || (id == Advanced && e->advanced) ||
          (id == Dual && e->viewMode == 0) ||
          (id == PlanOnly && e->viewMode == 1) ||
          (id == ViewOnly && e->viewMode == 2) ||
          (id >= GroupBase && id < GroupBase + 5 && id - GroupBase == e->group) ||
          (isTool && id - ToolBase == e->tool);
      bool idle = (id == Undo && e->history.empty()) || (id == Redo && e->future.empty());
      if (id == FlickerToggle && e->lightPicked() && Editor::number(e->lightField(17), 0) > 0)
        active = true;
      if (id == SoundToggle && !sfx::muted)
        active = true;
      if ((id == ThirdPersonToggle && e->thirdPersonPlay) || (id == LightViewButton && e->lightView != 0))
        active = true;
      if (id >= SwatchBase && id < SwatchBase + int(std::size(Editor::gels))) {
        const auto &gel = Editor::gels[id - SwatchBase];
        bool chosen = e->lightPicked() && std::abs(Editor::number(e->lightField(22), 1) - gel.r) < .02f &&
                      std::abs(Editor::number(e->lightField(23), 1) - gel.g) < .02f &&
                      std::abs(Editor::number(e->lightField(24), 1) - gel.b) < .02f;
        RECT body = ui::chunkyButton(dc, d->rcItem, ui::Paper, chosen ? ui::Look::Chosen : ui::Look::Plain,
                                     ui::hotControl == d->hwndItem, d->itemState & ODS_SELECTED, 10);
        InflateRect(&body, -5, -5);
        ui::roundBox(dc, body, gel.shown, ui::SandEdge, 8);
        return TRUE;
      }
      if (id >= PartTabBase && id < PartTabBase + parts::TabCount)
        active = e->partTab == id - PartTabBase;
      bool tab = (id >= GroupBase && id < GroupBase + 5) || (id >= PartTabBase && id < PartTabBase + parts::TabCount);
      ui::Look look = id == Walk          ? (e->playing ? ui::Look::Playing : ui::Look::Play)
                      : id == PlayStart   ? ui::Look::PlayFrom
                      : idle              ? ui::Look::Idle
                      : active            ? ui::Look::Chosen
                                          : ui::Look::Plain;
      // What the button sits on: the sand top band or a paper card.
      RECT placed = d->rcItem;
      MapWindowPoints(d->hwndItem, h, reinterpret_cast<POINT *>(&placed), 2);
      COLORREF under = GetParent(d->hwndItem) == h && placed.top < 98 ? ui::Header : ui::Paper;
      bool hot = ui::hotControl == d->hwndItem, pressed = d->itemState & ODS_SELECTED;
      RECT body = ui::chunkyButton(dc, d->rcItem, under, look, hot, pressed,
                                   isTool ? 16 : tab ? 14 : id == Walk ? 18 : 12);
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, ui::lookInk(look));
      auto text = label(d->hwndItem);
      RECT box = body;
      if (isTool) {
        // A big friendly icon on a tinted chip, the plain name under it.
        const auto &t = toolInfo[id - ToolBase];
        int cx = (body.left + body.right) / 2;
        RECT chip{cx - 21, body.top + 5, cx + 21, body.top + 38};
        ui::roundBox(dc, chip, ui::PageTint[std::clamp(t.group, 0, 4)],
                     active ? ui::SageEdge : ui::SandEdge, 12);
        SelectObject(dc, e->chipFont);
        SetTextColor(dc, ui::Ink);
        DrawTextW(dc, t.glyph, -1, &chip, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(dc, ui::lookInk(look));
        RECT name{body.left + 4, chip.bottom + 1, body.right - 4, body.bottom - 2};
        SelectObject(dc, e->smallFont);
        DrawTextW(dc, text.c_str(), int(text.size()), &name,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        return TRUE;
      }
      SelectObject(dc, id == Walk ? e->playFont : tab ? e->smallFont : e->buttonFont);
      DrawTextW(dc, text.c_str(), int(text.size()), &box,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
      return TRUE;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      auto target = BeginPaint(h, &ps);
      RECT r{};
      GetClientRect(h, &r);
      // Off screen first, so a relayout never flashes.
      HDC dc = CreateCompatibleDC(target);
      HBITMAP canvas = CreateCompatibleBitmap(target, std::max(1L, r.right), std::max(1L, r.bottom));
      auto oldCanvas = SelectObject(dc, canvas);
      FillRect(dc, &r, e->background);
      RECT band{0, 0, r.right, 98};
      auto header = CreateSolidBrush(ui::Header);
      FillRect(dc, &band, header);
      DeleteObject(header);
      RECT line{0, 98, r.right, 99};
      auto lineBrush = CreateSolidBrush(ui::HeaderLine);
      FillRect(dc, &line, lineBrush);
      DeleteObject(lineBrush);
      SetBkMode(dc, TRANSPARENT);
      // The engine's own editor, by name.
      SelectObject(dc, e->titleFont);
      SetTextColor(dc, ui::TerracottaEdge);
      textAt(dc, 16, 10, L"Raw Metal");
      SIZE first{};
      GetTextExtentPoint32W(dc, L"Raw Metal ", 10, &first);
      SetTextColor(dc, ui::Ink);
      SelectObject(dc, e->headingFont);
      textAt(dc, 16 + first.cx, 15, L"Campaign Builder");
      // Two paper cards, the plan and picture in warm bezels, the status pill.
      ui::roundBox(dc, RECT{8, 102, 268, r.bottom - 46}, ui::Paper, ui::PaperEdge, 18);
      ui::roundBox(dc, RECT{r.right - 334, 102, r.right - 8, r.bottom - 46}, ui::Paper, ui::PaperEdge, 18);
      for (HWND pane : {e->plan, e->viewport})
        if (pane && IsWindowVisible(pane)) {
          RECT q{};
          GetWindowRect(pane, &q);
          MapWindowPoints(nullptr, h, reinterpret_cast<POINT *>(&q), 2);
          InflateRect(&q, 5, 5);
          ui::roundBox(dc, q, ui::Bezel, ui::Bezel, 14);
        }
      ui::roundBox(dc, RECT{16, r.bottom - 41, r.right - 16, r.bottom - 7}, ui::Paper, ui::PaperEdge, 16);
      for (size_t i = 0; i < e->captions.size(); ++i) {
        auto &[box, text] = e->captions[i];
        bool heading = i == e->headingCaption;
        RECT q = box;
        if (heading)
          q.bottom += 8;
        SelectObject(dc, heading ? e->headingFont : e->buttonFont);
        SetTextColor(dc, heading ? ui::Ink : ui::InkSoft);
        DrawTextW(dc, text.c_str(), int(text.size()), &q, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
      }
      BitBlt(target, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
             ps.rcPaint.bottom - ps.rcPaint.top, dc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
      SelectObject(dc, oldCanvas);
      DeleteObject(canvas);
      DeleteDC(dc);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_TIMER: {
      edperf::flush();
      if (w == 2) {
        e->autosave();
        return 0;
      }
      if (w == 9) {
        KillTimer(h, 9);
        trace(std::string("menu test: ") + (e->renderedRevision >= 0 ? "engine picture ready" : "no engine picture yet") +
              (e->menuTestAction ? ", playing the full game" : ", closing"));
        e->dirty = false;
        if (e->menuTestAction)
          SendMessageW(h, WM_COMMAND, FullGame, 0);
        else
          SendMessageW(h, WM_CLOSE, 0, 0);
        return 0;
      }
      e->pollThumbnails();
      e->readLook();
      if (!e->restoredFrom.empty() && e->restoredAt >= 0 && e->changeCount > e->restoredAt) {
        e->restoredFrom.clear(); // she carried on: the restored work is simply her map now
        e->relayout();
      }
      if (e->popAt && e->plan) {
        InvalidateRect(e->plan, nullptr, FALSE);
        if (GetTickCount64() - e->popAt > 420)
          e->popAt = 0;
      }
      // A lamp drag that was throttled lands as soon as the picture is ready.
      if (e->lightPending && e->renderedRevision == e->revision)
        e->publish(), e->lightPending = false;
      // So does a drag or stroke that moved while the picture was busy.
      if (e->livePending && e->renderedRevision == e->revision && (e->drag || e->brushing || e->objectDrag || e->viewDrag))
        e->publish(), e->lastDragPublish = GetTickCount64();
      if (e->backgroundTest && !e->perfTest && GetTickCount64() - e->testStarted > 90000) {
        std::ofstream(e->root / L"native-editor-ui-test.txt")
            << "FAIL: engine viewport timeout\n";
        e->dirty = false;
        DestroyWindow(h);
        return 0;
      }
      std::istringstream in(e->channelText(e->status));
      std::string state;
      long long rev;
      int map, play;
      unsigned long long frame;
      if (in >> state >> rev >> map >> play >> frame) {
        if (state == "READY")
          e->renderedRevision = rev;
        if (edperf::on && state == "READY")
          while (!e->publishedAt.empty() && e->publishedAt.begin()->first <= rev) {
            edperf::note("edit_to_3d_seen", edperf::now() - e->publishedAt.begin()->second);
            e->publishedAt.erase(e->publishedAt.begin());
          }
        std::string rest;
        std::getline(in, rest);
        std::getline(in, rest);
        if(state=="READY")e->applyHoverRay(e->channelText(e->session/L"hover.txt"));
        if (state == "READY" && rest.rfind("PICK ", 0) == 0) {
          try {
            e->applyPick(rest);
          } catch (const std::exception &x) {
            e->message(wide(x.what()));
          }
        }
        if (state == "ERROR") {
          std::string error = rest, line;
          while (std::getline(in, line))
            error += line;
          if (e->playing && GetTickCount64() - e->playStarted > 1500)
            e->stopPlay();
          e->message(L"This map cannot be shown yet: " + wide(error));
        } else if (rev == e->revision) {
          // The engine is the judge of play: if it never started (a map it
          // cannot load) or stopped on its own, come back to building.
          if (e->playing && !play && GetTickCount64() - e->playStarted > 1500)
            e->stopPlay();
          if (e->backgroundTest && !e->perfTest && state == "READY" && frame >= 2) {
            if (e->testStage == 0) {
              e->testStage = 1;
              e->testFrame = frame;
              SendMessageW(e->musicList, CB_SETCURSEL, 7, 0);
              SendMessageW(h, WM_COMMAND, MAKEWPARAM(MusicList, CBN_SELCHANGE),
                           reinterpret_cast<LPARAM>(e->musicList));
              e->chooseKind("SIGN");
              e->apply();
              e->moveSelected(.25f, 0);
              return 0;
            }
            if (frame <= e->testFrame + 2)
              return 0;
            auto out =
                e->root / L".build" / L"native-ui" / L"background-ui.rmc";
            e->doc.save(out);
            NativeDocument reopened;
            reopened.open(out);
            std::ofstream(e->root / L"native-editor-ui-test.txt")
                << (reopened.text() == e->doc.text() &&
                            NativeDocument::fields(
                                reopened
                                    .lines[reopened.records("MAP", 0).front()])
                                    .at(19) == "7" &&
                            SendMessageW(e->musicList, CB_GETCURSEL, 0, 0) == 7
                        ? "PASS: hidden native UI authored, engine "
                          "reloaded/rendered, map OST selection retained, "
                          "save/reopen exact\n"
                        : "FAIL: saved document differs\n");
            e->dirty = false;
            DestroyWindow(h);
            return 0;
          }
          e->message(e->playing        ? L"Playing. Escape comes back to building."
                     : e->viewMode == 1 ? toolInfo[std::clamp(e->tool, 0, ToolCount - 1)].hint
                                        : L"3D view: hold right mouse to look, WASD to fly, left click builds. F5 plays.",
                     true);
        }
      }
      return 0;
    }
    case WM_APP + 41:
      // Only where the 50 ms status tick runs (a session, the timing run): the
      // hidden tests that poll the engine themselves must not get its work.
      if (edperf::baseline || !e->statusTimer)
        return 0;
      // The engine has a change in the picture: take its status now rather
      // than at the next 50 ms tick (the same work as that tick), then send
      // whatever a held drag, stroke or slider did meanwhile.
      SendMessageW(h, WM_TIMER, 1, 0);
      if (e->lightPending && e->renderedRevision == e->revision)
        e->publish(), e->lightPending = false;
      if (e->livePending && e->renderedRevision == e->revision && (e->drag || e->brushing || e->objectDrag || e->viewDrag))
        e->publish(), e->lastDragPublish = GetTickCount64();
      return 0;
    case WM_APP + 52:
      // A film render finished (NativeFilmTool.inc): frames made, the folder to open.
      e->filmRendered(int(w), reinterpret_cast<std::wstring *>(l));
      return 0;
    case WM_APP + 53:
      // The Filmmaker's shots are filmed (NativeFilmmaker.inc): frames made, what was made.
      e->shotsRendered(int(w), reinterpret_cast<std::wstring *>(l));
      return 0;
    case WM_APP + 31:
      // The engine left play on Escape; restore the view she was building in.
      if (e->playing || e->playView >= 0)
        e->playing = true, e->stopPlay();
      return 0;
    case WM_APP + 40:
      if (e->record >= 0)
        e->apply(true);
      return 0;
    case WM_HSCROLL: {
      HWND slider = reinterpret_cast<HWND>(l);
      if(slider==e->layerOpacity){e->layerOpacityChanged(LOWORD(w)!=TB_THUMBTRACK);return 0;}
      if(slider==e->doorOpenSlider){e->doorOpenChanged(LOWORD(w));return 0;}
      if(slider==e->emitterHeadingSlider||slider==e->emitterStrengthSlider){e->emitterSliderChanged(slider,LOWORD(w));return 0;}
      if (slider == e->historySlider) {
        e->scrubTo(int(SendMessageW(slider, TBM_GETPOS, 0, 0)));
        return 0;
      }
      if (slider == e->timeSlider) {
        if (LOWORD(w) == TB_ENDTRACK || LOWORD(w) == TB_THUMBPOSITION) {
          e->remember();
          e->setTimeOfDay(int(SendMessageW(slider, TBM_GETPOS, 0, 0)));
        }
        return 0;
      }
      if (!e->lightPicked() || !(slider == e->brightSlider || slider == e->reachSlider || slider == e->warmSlider))
        return 0;
      int code = LOWORD(w);
      bool final = code == TB_ENDTRACK;
      if (final && !e->lightDrag)
        return 0;
      if (!e->lightDrag)
        e->remember(), e->lightDrag = true;
      int at = int(SendMessageW(slider, TBM_GETPOS, 0, 0));
      if (slider == e->brightSlider)
        e->setLight({{7, Editor::trimmed(at / 100.f)}}, final);
      else if (slider == e->reachSlider)
        e->setLight({{8, std::to_string(at)}}, final);
      else
        e->setLight({{9, std::to_string(at / 50 * 50)}}, final);
      static ULONGLONG lastTick = 0;
      if (!final && !e->quiet() && GetTickCount64() - lastTick > 70)
        lastTick = GetTickCount64(), sfx::play(sfx::Slider, .7f, .1f);
      if (final)
        e->lightDrag = false, e->message(L"Lit. Drag again, or press F5 to walk under it.");
      return 0;
    }
    case WM_COMMAND: {
      int id = LOWORD(w), notification = HIWORD(w);
      if (id >= SwatchBase && id < SwatchBase + int(std::size(Editor::gels)) && e->lightPicked()) {
        const auto &gel = Editor::gels[id - SwatchBase];
        e->remember();
        e->setLight({{22, Editor::trimmed(gel.r)}, {23, Editor::trimmed(gel.g)}, {24, Editor::trimmed(gel.b)}}, true);
        e->message(std::wstring(gel.name) + L" light.");
        for (HWND c : e->swatches)
          InvalidateRect(c, nullptr, FALSE);
        return 0;
      }
      if (id == MapSearch && notification == EN_CHANGE) {
        e->refreshMaps();
        return 0;
      }
      if (id >= IdeaBase && id < IdeaBase + 3) {
        if (size_t(id - IdeaBase) < e->ideas.size())
          e->runIdea(e->ideas[size_t(id - IdeaBase)].action);
        return 0;
      }
      if (id >= MoodBase && id < MoodBase + 4) {
        e->moodRoom(id - MoodBase);
        return 0;
      }
      if (id >= PartTabBase && id < PartTabBase + parts::TabCount) {
        e->partTab = id - PartTabBase;
        if (e->modelSearch && GetWindowTextLengthW(e->modelSearch))
          SetWindowTextW(e->modelSearch, L"");
        e->refreshParts();
        return 0;
      }
      if (id == ModelSearch && notification == EN_CHANGE) {
        e->filterModels();
        e->refreshParts();
        return 0;
      }
      if (id == SnapList && notification == CBN_SELCHANGE) {
        auto choice = SendMessageW(e->snapList, CB_GETCURSEL, 0, 0);
        const float sizes[] = {.25f, .5f, 1.f, 0.f};
        if (choice >= 0 && choice < 4)
          e->snap = sizes[choice];
        return 0;
      }
      if (id == MusicList && notification == CBN_SELCHANGE) {
        e->selectMusic(int(SendMessageW(e->musicList, CB_GETCURSEL, 0, 0)));
        return 0;
      }
      if (id == ListenMusic) {
        e->auditionMusic = !e->auditionMusic;
        SetWindowTextW(GetDlgItem(h, ListenMusic), e->auditionMusic
                                                       ? L"■ Stop"
                                                       : L"♫ Listen");
        e->writeControl();
        return 0;
      }
      if (id == MapList && notification == LBN_SELCHANGE) {
        auto n = SendMessageW(e->mapList, LB_GETCURSEL, 0, 0);
        e->map = int(SendMessageW(e->mapList, LB_GETITEMDATA, n, 0));
        e->layer = 0;
        e->writeControl();
        e->refresh();
        return 0;
      }
      if(id>=InspectorTab&&id<=LayerFootprint){e->handleLayerCommand(id,notification);return 0;}
      if (id == LayerList && notification == CBN_SELCHANGE) {
        auto n = SendMessageW(e->layerList, CB_GETCURSEL, 0, 0);
        e->layer = int(SendMessageW(e->layerList, CB_GETITEMDATA, n, 0));
        e->selectRecord(-1);e->writeControl();
        InvalidateRect(e->plan, nullptr, FALSE);
        return 0;
      }
      if (id == KindList && notification == CBN_SELCHANGE) {
        e->kind = int(SendMessageW(e->kindList, CB_GETCURSEL, 0, 0));
        e->refreshRecords();
        return 0;
      }
      if (id == RecordList && notification == LBN_SELCHANGE) {
        auto n = SendMessageW(e->recordList, LB_GETCURSEL, 0, 0);
        e->record = int(SendMessageW(e->recordList, LB_GETITEMDATA, n, 0));
        e->fillFields(false);
        InvalidateRect(e->plan, nullptr, FALSE);
        return 0;
      }
      if (id == ModelList && notification == CBN_SELCHANGE) {
        e->record = -1;
        e->useModelSelection();
        e->tool = e->schemas[size_t(e->kind)].kind == "CLUTTER" ? 14 : 8;
        for (auto button : e->tools)
          InvalidateRect(button, nullptr, FALSE);
        e->message(L"Native model selected. Click the plan to place it; engine "
                   L"dimensions are loaded.");
        return 0;
      }
      if (id == MaterialList && notification == CBN_SELCHANGE) {
        e->useMaterial();
        return 0;
      }
      if (id == HeightList && notification == CBN_SELCHANGE) {
        auto selected = SendMessageW(e->heightList, CB_GETCURSEL, 0, 0);
        if (selected >= 0 && selected <= 3)
          e->placeHeight = int(selected);
        e->message(selected == 0
                       ? L"New models, shapes, loose props and computers stand on the floor."
                       : L"New models, shapes, loose props and computers use this height. Lamps stay on the ceiling.");
        return 0;
      }
      if (id == GroundList && notification == CBN_SELCHANGE) {
        auto selected = SendMessageW(e->groundList, CB_GETCURSEL, 0, 0);
        if (selected >= 0 && selected <= 8)
          e->groundKind = int(selected);
        e->message(L"Ground cover uses this paint. Drag Ground cover on the plan.");
        return 0;
      }
      if (id == SurfaceList && notification == CBN_SELCHANGE) {
        auto selected = SendMessageW(e->surfaceList, CB_GETCURSEL, 0, 0);
        if (selected >= 0)
          e->applySurfaceSet(
              int(SendMessageW(e->surfaceList, CB_GETITEMDATA, selected, 0)));
        return 0;
      }
      if (id == RoleList && notification == CBN_SELCHANGE) {
        e->tool = 13;
        e->message(L"Click the plan to place this person. Security stands guard. "
                   L"Creature places a huntsman.");
        return 0;
      }
      if (id >= GroupBase && id < GroupBase + 5) {
        e->group = id - GroupBase;
        e->toolScroll = 0;
        e->layoutTools();
        for (auto tab : e->groupTabs)
          InvalidateRect(tab, nullptr, FALSE);
        return 0;
      }
      if ((id == HazardList || id == PickupList || id == CreatureList || id == TileList || id == PrimList ||
           id == BrushList || id == BehaviourList || id == ClutterList) && notification == CBN_SELCHANGE) {
        if (id == BrushList)
          e->brush = e->listIndex(e->brushList);
        if (id == ClutterList)
          e->relayout();
        e->message(id == BrushList ? (e->brush == 0 ? L"Brush: hold and paint over tiles." : e->brush == 1 ? L"Box: drag a rectangle." : L"Fill: click to fill a whole area.")
                                   : L"Got it. Click or drag on the map.");
        return 0;
      }
      if (id == ActionList && notification == CBN_SELCHANGE) {
        e->addToTrigger(e->listIndex(e->actionList));
        SendMessageW(e->actionList, CB_SETCURSEL, 0, 0);
        return 0;
      }
      if (id == PreyList || id == HunterList) {
        e->creatureControlChanged(id, notification);
        return 0;
      }
      if (id >= DoorToolList && id <= DoorStrengthList) {
        e->doorControlChanged(id, notification);
        return 0;
      }
      if (id >= EmitterPresetList && id <= EmitterRestyleList) {
        e->emitterControlChanged(id, notification);
        return 0;
      }
      if (id == LinkList && notification == LBN_SELCHANGE) {
        auto n = SendMessageW(e->linkList, LB_GETCURSEL, 0, 0);
        if (n >= 0)
          e->selectRecord(int(SendMessageW(e->linkList, LB_GETITEMDATA, n, 0)));
        return 0;
      }
      if(id==UpperFloorTiles&&notification==CBN_SELCHANGE){e->upperFloorVoid=e->listIndex(e->upperFloorTiles)==1;return 0;}
      if (id >= ToolBase && id < ToolBase + int(e->tools.size())) {
        e->layersDock=false;
        int selected = id - ToolBase;
        if (selected == 18 || selected == 19) {
          if (selected == 18)
            e->rotation = (e->rotation + 1) % 4;
          else
            e->mirror = (e->mirror + 1) % 4;
          if (e->regionStamp.width > 0)
            e->tool = 17;
          e->message(e->regionStamp.width > 0 ? L"Copy turned. Click where it should go." : L"Copy an area first.");
        } else {
          e->tool = selected;
          if (*toolInfo[selected].kind)
            e->chooseKind(toolInfo[selected].kind);
          // The Sign tool brings its designer; another tool puts it away unless a sign is picked.
          if (selected == 7)
            e->openSignDesigner();
          else if (e->signDesigner && !e->pickedSign())
            e->closeSignDesigner();
          if (selected == 8 || (selected == 14 && e->listIndex(e->clutterList) == 7))
            e->useModelSelection();
          e->message(toolInfo[selected].hint);
        }
        e->group = toolInfo[e->tool].group;
        e->repaintTools();
        for (auto tab : e->groupTabs)
          InvalidateRect(tab, nullptr, FALSE);
        InvalidateRect(e->plan, nullptr, FALSE);
        return 0;
      }
      switch (id) {
      case New:
        if (!e->confirmSave())
          break;
        {
          NativeDocument next;
          next.newCampaign("New custom campaign");
          next.addBlankMap("Map");
          e->adoptDocument(std::move(next), {}, false);
        }
        e->welcomeIfBlank();
        break;
      case Open:
        e->openFile(false);
        break;
      case FromCampaign:
        e->openFromCampaign();
        break;
      case IntoGame:
        e->saveIntoGame();
        break;
      case BackToOriginal:
        e->backToOriginal();
        break;
      case ExportChanges:
        e->exportMyChangesNow();
        break;
      case Save:
        e->saveNow();
        break;
      case Advanced:
        e->advanced = !e->advanced;
        e->fillFields(e->record < 0);
        e->relayout();
        e->message(e->advanced ? L"Advanced: every number the game reads. Advanced again hides them." : L"Back to the simple view.");
        break;
      case AutoDetail:
        e->autoDetail = !e->autoDetail;
        e->relayout();
        break;
      case AutoDressing:
        e->autoDressingEnabled=!e->autoDressingEnabled;
        if(!e->quiet()){std::ofstream(e->root/L".build"/L"autodressing.setting")<<(e->autoDressingEnabled?"on":"off");}
        e->relayout();
        e->message(e->autoDressingEnabled?L"Autodressing on: new rooms get managed decor; user content stays yours.":L"Autodressing off: room creation adds no automatic furniture, finishes, lamps or doors.");
        break;
      case TurnSelected:
        e->rotateSelected();
        break;
      case PlayStart:
        e->startPlay(1);
        break;
      case Export:
        e->openFile(true, true);
        break;
      case Starter:
        if (!e->confirmSave())
          break;
        {
          NativeDocument next;
          next.open(e->root / L"custom maps" / L"Starter Map.rmc");
          e->adoptDocument(std::move(next), {}, false);
        }
        e->message(L"The finished example. Look around, change anything; saving asks for a new name.");
        break;
      case FlickerToggle:
        if (e->lightPicked()) {
          e->remember();
          bool on = Editor::number(e->lightField(17), 0) > 0;
          e->setLight({{17, on ? std::string("0") : std::string("0.5")}}, true);
          e->message(on ? L"Steady light." : L"The lamp flickers. Look at the picture.");
          e->relayout();
        }
        break;
      case StartFresh:
        e->startFresh();
        break;
      case OutlineWalls:
      case OutlineBlock:
      case OutlineTerraces:
        e->makeFromOutline(id - OutlineWalls);
        e->celebrate();
        break;
      case LightViewButton:
        e->chooseLightView();
        break;
      case ThirdPersonToggle:
        e->setThirdPersonPlay(!e->thirdPersonPlay);
        InvalidateRect(GetDlgItem(h, ThirdPersonToggle), nullptr, FALSE);
        break;
      case FilmWrite:
        e->writeFilmButton();
        break;
      case FilmRender:
        e->renderFilmButton();
        break;
      case FilmMaker:
        e->openFilmmaker();
        break;
      case SoundToggle: {
        sfx::muted = !sfx::muted;
        std::error_code ec;
        auto flag = e->root / L".build" / L"editor-sound-muted";
        if (sfx::muted)
          std::ofstream(flag) << "1\n";
        else
          fs::remove(flag, ec);
        e->relayout();
        InvalidateRect(GetDlgItem(h, SoundToggle), nullptr, FALSE);
        break;
      }
      case Undo:
        if (!e->history.empty() && !e->quiet())
          sfx::play(sfx::Back, .8f);
        if (!e->history.empty()) {
          e->future.push_back(e->doc.text());
          e->doc.loadText(e->history.back());
          e->history.pop_back();e->selectedRecords.clear();e->record=-1;
          e->publish(true, false);
          e->refresh();
        }
        break;
      case Redo:
        if (!e->future.empty() && !e->quiet())
          sfx::play(sfx::Click, .8f);
        if (!e->future.empty()) {
          e->history.push_back(e->doc.text());
          e->doc.loadText(e->future.back());
          e->future.pop_back();e->selectedRecords.clear();e->record=-1;
          e->publish(true, false);
          e->refresh();
        }
        break;
      case Walk:
        e->startPlay(0);
        break;
      case FullGame:
        if (e->playOut) {
          try {
            e->publish();
            bool own = !e->path.empty() && !e->isExample();
            fs::path file = own ? e->path : e->pack;
            if (own) {
              fs::path temporary = file.wstring() + L".editor-tmp-" +
                                   std::to_wstring(GetCurrentProcessId());
              e->doc.save(temporary);
              if (!replaceEditorFile(temporary, file))
                throw std::runtime_error("Could not save the campaign for play");
              e->dirty = false;
            }
            *e->playOut = file;
            e->playExit = true;
            trace("Play in game: " + utf8(file.wstring()));
            DestroyWindow(e->window);
          } catch (const std::exception &error) {
            e->message(wide(error.what()));
          }
        }
        break;
      case Dual:
        e->viewMode = 0;
        e->layout();
        break;
      case PlanOnly:
        e->viewMode = 1;
        e->layout();
        break;
      case ViewOnly:
        e->viewMode = 2;
        e->layout();
        break;
      case MapPassage:e->joinMapPassage=!e->joinMapPassage;break;
      case AddMap:e->chooseMapEdge();break;
      case MapNorth:case MapSouth:case MapWest:case MapEast:e->addAdjacentMap(id-MapNorth);break;
      case Furnish:
        e->furnish();
        break;
      case Outdoors:
        e->applyOutdoors();
        break;
      case AddFloor: {
        e->remember();
        e->createFloorLayer();e->selectedLayerGroup=-1;e->layersDock=true;
        e->publish();
        e->refresh();
        break;
      }
      case Apply:
        e->apply();
        break;
      case NewObject:
        e->record = -1;
        e->fillFields(true);
        InvalidateRect(e->plan, nullptr, FALSE);
        break;
      case Duplicate:
        e->copySelected();
        break;
      case Remove:
        e->removeSelected();
        break;
      }
      return 0;
    }
    case WM_CLOSE:
      trace("window close asked");
      if (!e->confirmSave()) {
        trace("close cancelled");
        return 0;
      }
      e->closed = true;
      DestroyWindow(h);
      return 0;
    case WM_DESTROY:
      trace(e->playExit ? "window gone: playing the map in the game" : "window gone");
      // Opened from the campaign menu: do not post a thread quit. That quit
      // is still in the queue when the menu's window starts, so the menu
      // sees it and the program exits instead of coming back.
      e->quitLoop = true;
      if (!e->playOut)
        PostQuitMessage(0);
      return 0;
    case WM_QUERYENDSESSION:
      trace("Windows is signing out");
      break;
    }
  } catch (const std::exception &x) {
    e->message(wide(x.what()));
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// ---- Simulate (F6): the toggle button, its keys, the plan's live marks and --simulate-test ----
#include "NativeSimulate.h"
// ---- end Simulate ----

static int editorSession(HINSTANCE instance, const EditorDoor *door,
                          fs::path *playOut = nullptr) {
  try {
    Editor e;
    e.playOut = playOut;
    placedHeights.clear();
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr, executable, 32768);
    e.root = fs::path(executable).parent_path();
    {
      std::error_code ec;
      fs::create_directories(e.root / L".build", ec);
      sessionTrace = e.root / L".build" / L"editor-session-trace.txt";
      std::ofstream(sessionTrace) << "session " << (playOut ? "from the game menu" : "standalone") << ", fresh "
                                  << (door ? int(door->fresh) : -1) << '\n';
      // --perf-trace (or RAWMETAL_EDITOR_PERF=1): interaction costs in this trace,
      // the engine picture's reload and frame costs in editor-preview-trace.txt.
      int count = 0;
      auto args = CommandLineToArgvW(GetCommandLineW(), &count);
      for (int i = 1; args && i < count; ++i) {
        std::wstring a = args[i];
        if (a == L"--perf-trace")
          edperf::on = true;
        if (a == L"--editor-perf-test")
          edperf::on = e.perfTest = true;
        if (a == L"--perf-baseline")
          edperf::baseline = NativeDocument::scanOnly = true;
      }
      if (args)
        LocalFree(args);
      wchar_t flag[8]{};
      if (GetEnvironmentVariableW(L"RAWMETAL_EDITOR_PERF", flag, 8) && flag[0] == L'1')
        edperf::on = true;
      if (edperf::on) {
        e.perfFile = e.root / L".build" / L"editor-preview-trace.txt";
        std::ofstream(e.perfFile) << "preview trace\n";
        trace("perf trace on");
      }
    }
    e.loadSchemas();
    trace("schemas loaded");
    if (e.schemas.empty())
      throw std::runtime_error("Custom map guide schemas were not found");
    e.doc.newCampaign("New custom campaign");
    e.doc.addBlankMap("Map");
    if (door && !door->fresh) {
      e.doc.open(door->project);
      e.path = door->project;
      e.dirty = repairRetiredEditorPeople(e.doc) > 0;
    }
    wchar_t temp[32768]{};
    GetTempPathW(32768, temp);
    e.session = fs::path(temp) / (L"RawMetalNativeEditor-" +
                                  std::to_wstring(GetCurrentProcessId()) +
                                  L"-" + std::to_wstring(GetTickCount64()));
    fs::create_directories(e.session);
    e.pack = e.session / L"working.rmc";
    e.control = e.session / L"control.txt";
    e.status = e.session / L"status.txt";
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    // The test modes check every document query against a full scan of the lines.
    if (argc > 1)
      for (auto mode : {L"--selftest", L"--background-test", L"--layout-test", L"--editing-test", L"--remake-test",
                        L"--fun-test", L"--simulate-test", L"--script"})
        if (std::wstring(argv[1]) == mode)
          NativeDocument::verify = true;
    if (argc > 1 && std::wstring(argv[1]) == L"--selftest") {
      auto preserve = [&](const fs::path &path, int maps, const char *label) {
        NativeDocument campaign;
        campaign.open(path);
        if (int(campaign.records("MAP").size()) != maps)
          throw std::runtime_error(std::string(label) +
                                   " no longer has the expected maps");
        auto original = campaign.text();
        campaign.save(e.pack);
        NativeDocument reopened;
        reopened.open(e.pack);
        if (reopened.text() != original)
          throw std::runtime_error(std::string(label) +
                                   " changed when saved and reopened");
        e.doc = std::move(campaign);
        if (!e.validateDocument())
          throw std::runtime_error(std::string(label) +
                                   " failed engine validation");
      };
      preserve(e.root / L"custom maps" / L"Starter Map.rmc", 150, "Starter");
      preserve(e.root / L"custom maps" / L"RawMetal-Ashfall-Exclusion-Zone.txt",
               20, "Ashfall");
      // Every other campaign shipped beside the editor, and beside the game
      // when the editor sits in its workspace, reads and writes back exactly
      // and passes the engine.
      std::string others;
      std::vector<fs::path> folders{e.root / L"custom maps"};
      if (fs::exists(e.root.parent_path().parent_path() / L"RawMetal.exe"))
        folders.push_back(e.root.parent_path().parent_path() / L"custom maps");
      for (const auto &folder : folders) {
        std::error_code ec;
        std::vector<fs::path> files;
        for (const auto &entry : fs::directory_iterator(folder, ec))
          if (entry.is_regular_file() && entry.path().extension() == L".rmc" &&
              entry.path().filename() != L"Starter Map.rmc")
            files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        for (const auto &file : files) {
          NativeDocument campaign;
          campaign.open(file);
          auto label = file.filename().u8string();
          std::string name(label.begin(), label.end());
          preserve(file, int(campaign.records("MAP").size()), name.c_str());
          others += (others.empty() ? "" : ", ") + name;
        }
      }
      // A file from the retired editor (people on a clock it never wrote,
      // guards with no post) gets what the Person tool writes on open.
      {
        NativeDocument legacy;
        legacy.newCampaign("Legacy people");
        legacy.addRoom("Room");
        legacy.add({"ACTOR", "0", "editor_clock", "10", "1", "0", "0", "0"});
        legacy.add({"ACTOR_KEY", "0", "0", "0", "5", "5", "0", "0", "0", "0"});
        legacy.add({"ACTOR_KEY", "0", "0", "60000", "5", "5", "0", "0", "0", "1"});
        legacy.add({"ACTOR_AI", "0", "0", "1", "", "1.4", "2", "6"});
        legacy.add({"ACTOR", "0", "editor_clock", "4", "1", "0", "0", "1"});
        legacy.add({"ACTOR_KEY", "0", "1", "0", "18", "4", "0", "0", "0", "0"});
        legacy.add({"ACTOR_KEY", "0", "1", "60000", "18", "4", "0", "0", "0", "1"});
        legacy.add({"ACTOR_AI", "0", "1", "3", "", "1.4", "2", "6"});
        legacy.add({"ACTOR_WEAPON", "0", "1", "2", "650", "1000", "9", "18"});
        auto broken = legacy.text();
        if (repairRetiredEditorPeople(legacy) != 3 ||
            repairRetiredEditorPeople(legacy) != 0 ||
            legacy.records("TIMED_SEQUENCE", 0).size() != 1 ||
            legacy.records("ACTOR_GUARD", 0).size() != 1 ||
            legacy.records("ACTOR_PATROL", 0).size() != 1)
          throw std::runtime_error("Old editor people were not completed exactly once");
        e.doc = std::move(legacy);
        if (!e.validateDocument())
          throw std::runtime_error("Completed old editor people failed engine validation");
        NativeDocument untouched;
        untouched.loadText(broken);
        untouched.lines.insert(untouched.lines.end() - 1,
                               "TIMED_SEQUENCE|0|editor_clock|editor_clock_done|0|0|24|24|-100|100|60000|60000|0|0|0|0|1|1");
        untouched.lines.insert(untouched.lines.end() - 1, "ACTOR_GUARD|0|1|12|0.5|600|4000|");
        untouched.lines.insert(untouched.lines.end() - 1, "ACTOR_PATROL|0|1|18|4|0|1000");
        untouched.touch();
        auto good = untouched.text();
        if (repairRetiredEditorPeople(untouched) != 0 || untouched.text() != good)
          throw std::runtime_error("A file with complete people was changed on open");
      }
      std::ofstream(e.root / L"native-editor-selftest.txt")
          << "PASS: native document preserves Starter and Ashfall "
             "byte-for-byte; engine validates every map\n"
          << "PASS: also byte-for-byte and engine-valid: " << others << "\n"
          << "PASS: old editor people get their missing clock and guard post on open; complete files stay untouched\n";
      LocalFree(argv);
      return 0;
    }
    // RawMetalEditor.exe --filmmaker-render "PROJECT.rmc" [MAP]: film every Filmmaker shot (NativeFilmmaker.inc).
    if (argc > 2 && std::wstring(argv[1]) == L"--filmmaker-render") {
      int code = e.renderProjectShots(fs::path(argv[2]), argc > 3 ? _wtoi(argv[3]) : -1);
      LocalFree(argv);
      return code;
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--background-test") {
      e.backgroundTest = true;
      e.doc.open(e.root / L"custom maps" / L"Starter Map.rmc");
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--layout-test") {
      e.backgroundTest = e.layoutTest = true;
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--editing-test")
      e.backgroundTest = e.editingTest = true;
    if (argc > 1 && std::wstring(argv[1]) == L"--remake-test")
      e.backgroundTest = e.remakeTest = true;
    if (e.perfTest)
      e.backgroundTest = true;
    // The game menu's way in, with no window shown: RawMetal.exe --menu-editor-test[=play]
    if (argc > 1 && std::wstring(argv[1]).rfind(L"--menu-editor-test", 0) == 0) {
      e.menuTest = true;
      e.menuTestAction = std::wstring(argv[1]).find(L"play") != std::wstring::npos ? 1 : 0;
      // "shown": the real window and live engine view (run it on a hidden desktop).
      e.menuShown = std::wstring(argv[1]).find(L"shown") != std::wstring::npos;
      trace("menu test");
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--fun-test")
      e.backgroundTest = e.funTest = true;
    if (argc > 1 && std::wstring(argv[1]) == L"--simulate-test") // Simulate (F6), NativeSimulate.h
      e.backgroundTest = simulate::testRun = true;
    int demoScene = argc > 2 && std::wstring(argv[1]) == L"--demo" ? _wtoi(argv[2]) : 0;
    if (argc > 1 && std::wstring(argv[1]) == L"--make-thumbs") {
      e.thumbs.folder = e.thumbFolder();
      e.makeThumbnails(true);
      size_t made = 0, wanted = 0;
      for (auto &item : e.allParts()) {
        if (item.group != parts::Shapes && (item.value >= int(std::size(retro::model::size)) || retro::model::size[item.value][2] <= 0))
          continue; // no size table: a placeholder tile
        ++wanted, made += e.thumbs.exists(item.key);
      }
      for (auto &[id, name] : e.structureMaterials)
        if (parts::paintable(name))
          ++wanted, made += e.thumbs.exists("mat-" + std::to_string(id));
      for (int i = 0; i < 13; ++i)
        ++wanted, made += e.thumbs.exists("surf-" + std::to_string(i));
      std::ofstream(e.root / L"native-editor-thumbs.txt")
          << (made == wanted ? "PASS: " : "PARTIAL: ") << made << " of " << wanted << " pictures in .build/thumbs\n";
      LocalFree(argv);
      return made == wanted ? 0 : 1;
    }
    // The Sound tool, headless: a bed and a single sound placed from the bank, and a room reverb, through the engine.
    if (argc > 1 && std::wstring(argv[1]) == L"--sound-tool-test") {
      e.backgroundTest = true;
      std::string report;
      bool ok = false;
      try {
        ok = e.soundToolTest(report);
      } catch (const std::exception &error) {
        report += std::string("FAIL: ") + error.what() + "\n";
      }
      std::ofstream(e.root / L"native-editor-sound-test.txt") << (ok ? "PASS: sound tool\n" : "") << report;
      LocalFree(argv);
      return ok ? 0 : 1;
    }
    // The Sign designer, headless: snapping, a designed sign through the engine, file round trips.
    if (argc > 1 && std::wstring(argv[1]) == L"--sign-designer-test") {
      e.backgroundTest = true;
      std::string report;
      bool ok = false;
      try {
        ok = e.signDesignerTest(report);
      } catch (const std::exception &error) {
        report += std::string("FAIL: ") + error.what() + "\n";
      }
      std::ofstream(e.root / L"native-editor-sign-test.txt") << (ok ? "PASS: sign designer\n" : "") << report;
      LocalFree(argv);
      return ok ? 0 : 1;
    }
    if (argc > 2 && std::wstring(argv[1]) == L"--script") {
      std::ofstream(e.root / L"native-editor-cli.txt") << "RUNNING\n";
      try {
        runNativeCommands(e.doc, argv[2]);
      } catch (const std::exception &error) {
        std::ofstream(e.root / L"native-editor-cli.txt")
            << "FAIL: " << error.what() << '\n';
        throw;
      }
      bool valid = e.validateDocument();
      std::ofstream(e.root / L"native-editor-cli.txt")
          << (valid ? "PASS: authored native campaign validates\n"
                    : "FAIL: native engine rejected campaign\n");
      LocalFree(argv);
      return valid ? 0 : 1;
    }
    // Open from campaign, headless: --campaign-open-test <level> <out.rmc> lists the game's levels,
    // opens one exactly as the button does, has the engine validate it and saves it to <out.rmc>.
    if (argc > 3 && std::wstring(argv[1]) == L"--campaign-open-test") {
      std::ofstream report(e.root / L"native-editor-campaign-test.txt");
      std::wstring why;
      auto maps = e.campaignMaps(why);
      int level = _wtoi(argv[2]);
      bool sealedListed = std::any_of(maps.begin(), maps.end(), [](const Editor::CampaignEntry &m) { return m.sealed; });
      auto found = std::find_if(maps.begin(), maps.end(), [&](const Editor::CampaignEntry &m) { return m.chunk == level; });
      bool ok = found != maps.end() && e.openCampaignMap(*found, why);
      fs::path folder;
      std::wstring name;
      bool hinted = ok && e.campaignSaveHint(folder, name) && e.campaignLevel() == level;
      bool valid = ok && e.validateDocument();
      if (ok)
        e.doc.save(argv[3]);
      report << (ok && hinted && valid ? "PASS" : "FAIL") << ": " << maps.size() << " levels listed (sealed chapters "
             << (sealedListed ? "listed" : "hidden") << "); level " << level << (found == maps.end() ? " not offered" : ok ? " opened" : " failed")
             << "; engine " << (valid ? "validates it" : "rejects it") << "; Save as offers " << utf8((folder / name).wstring())
             << "; into the game: " << utf8(e.campaignOverrideFile(level).wstring()) << (why.empty() ? "" : "; " + utf8(why)) << '\n';
      LocalFree(argv);
      return ok && hinted && valid ? 0 : 1;
    }
    // Export my changes, headless: --campaign-export-test writes the zip and reports it.
    if (argc > 1 && std::wstring(argv[1]) == L"--campaign-export-test") {
      std::wstring why;
      auto file = e.exportMyChanges(false, why);
      std::ofstream(e.root / L"native-editor-campaign-export.txt") << (file.empty() ? "FAIL: " : "PASS: ") << utf8(why) << '\n' << utf8(file.wstring()) << '\n';
      LocalFree(argv);
      return file.empty() ? 1 : 0;
    }
    if (argc > 2 && std::wstring(argv[1]) == L"--open") {
      e.doc.open(argv[2]);
      e.path = argv[2];
      e.dirty = repairRetiredEditorPeople(e.doc) > 0;
    }
    LocalFree(argv);
    // Unsaved work from a session that ended badly comes back on request.
    e.demoMode = demoScene != 0;
    if(!e.backgroundTest&&!e.demoMode){std::ifstream setting(e.root/L".build"/L"autodressing.setting");
      std::string value;if(setting>>value)e.autoDressingEnabled=value!="off";}
    if (!e.backgroundTest && !e.remakeTest && !e.demoMode) {
      std::error_code ec;
      auto rescued = e.autosavePath();
      if (fs::exists(rescued, ec) &&
          (e.path.empty() || !fs::exists(e.path, ec) || fs::last_write_time(rescued, ec) > fs::last_write_time(e.path, ec))) {
        NativeDocument saved;
        saved.open(rescued);
        // No question: the work simply comes back, and a quiet button offers a fresh start.
        if (saved.text() != e.doc.text()) {
          e.restoredFrom = e.doc.text();
          e.doc = std::move(saved);
          e.dirty = true;
        } else
          fs::remove(rescued, ec);
      }
    }
    // Friendly type, a little larger than the Windows default.
    auto typeface = [](int size, int weight, const wchar_t *face = L"Segoe UI") {
      return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                         CLEARTYPE_QUALITY, 0, face);
    };
    e.font = typeface(15, FW_NORMAL);
    e.buttonFont = typeface(15, FW_SEMIBOLD);
    e.headingFont = typeface(19, FW_SEMIBOLD);
    e.playFont = typeface(18, FW_BOLD);
    e.titleFont = typeface(26, FW_BOLD);
    e.glyphFont = typeface(21, FW_NORMAL, L"Segoe UI Symbol");
    e.chipFont = typeface(25, FW_NORMAL, L"Segoe UI Symbol");
    e.smallFont = typeface(13, FW_SEMIBOLD);
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpfnWndProc = mainProc;
    wc.lpszClassName = L"RawMetalNativeEditor";
    RegisterClassW(&wc);
    wc.lpfnWndProc = planProc;
    wc.lpszClassName = L"RawMetalNativePlan";
    RegisterClassW(&wc);
    wc.lpfnWndProc = formProc;
    wc.lpszClassName = L"RawMetalNativeForm";
    RegisterClassW(&wc);
    wc.lpfnWndProc = shelfProc;
    wc.lpszClassName = L"RawMetalToolShelf";
    RegisterClassW(&wc);
    e.window = CreateWindowExW(
        0, L"RawMetalNativeEditor", L"Raw Metal Campaign Builder",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        1680, 980, nullptr, nullptr, instance, &e);
    if (!e.window)
      throw std::runtime_error("Cannot create native editor");
    trace("window created");
    // The menu is already gone. Show the blank editor before the parts list
    // and its pictures are filled in, or the click looks like a return home.
    if (!e.backgroundTest && (!e.menuTest || e.menuShown)) {
      ShowWindow(e.window, SW_SHOWMAXIMIZED);
      UpdateWindow(e.window);
    }
    e.toolShelf = CreateWindowExW(
        0, L"RawMetalToolShelf", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10,
        e.window, nullptr, instance, &e);
    if (!e.toolShelf)
      throw std::runtime_error("Cannot create the build list");
    for (auto [id, text] : std::vector<std::pair<int, const wchar_t *>>{
             {New, L"New"},
             {Open, L"Open"},
             {FromCampaign, L"Open from campaign"},
             {IntoGame, L"Save into the game"},
             {BackToOriginal, L"Back to original"},
             {ExportChanges, L"Export my changes"},
             {Save, L"Save"},
             {Undo, L"↶ Undo"},
             {Redo, L"↷ Redo"},
             {Walk, L"▶ Play  F5"},
             {PlayStart, L"▶ From start"},
             {PlanOnly, L"2D paint  F2"},
             {Dual, L"2D + 3D  F3"},
             {ViewOnly, L"3D edit  F4"},
             {Starter, L"Example"},
             {Export, L"Save as..."},
             {FullGame, L"Play in game \u2197"},
             {AddMap, L"+ Map"},
             {AddFloor, L"+ Upper floor"},
             {Furnish, L"Add lamps"},
             {Outdoors, L"Make outdoors"},
             {Apply, L"Apply"},
             {Remove, L"Delete"},
             {Duplicate, L"Copy"},
             {TurnSelected, L"⟳ Turn"},
             {Advanced, L"Advanced"},
             {AutoDetail, L"Auto lamps and door: on"},
             {AutoDressing,L"Autodressing: on"},
             {NewObject, L"New object"}})
      e.button(text, id);
    for (int g = 0; g < 5; ++g)
      e.groupTabs.push_back(e.button(groupNames[g], GroupBase + g));
    e.mapList = e.controlWindow(L"LISTBOX", L"",
                                LBS_NOTIFY | WS_VSCROLL | WS_BORDER, MapList);
    e.mapSearch = e.controlWindow(
        L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, MapSearch);
    SendMessageW(e.mapSearch, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"Find map by name or number"));
    for(auto [id,text]:{std::pair{InspectorTab,L"Inspector"},std::pair{LayerTab,L"Floors / layers"},std::pair{LayerMenu,L"Layer ▾"},std::pair{LayerVisible,L"Show / hide"},std::pair{LayerLock,L"Lock / unlock"},std::pair{LayerCommit,L"Apply floor"},std::pair{LayerReferences,L"Other floors: outlines on"}})e.button(text,id);
    e.layerStack=e.controlWindow(L"LISTBOX",L"",LBS_NOTIFY|LBS_EXTENDEDSEL|WS_VSCROLL|WS_BORDER,LayerStack);
    e.layerPreview=e.controlWindow(L"STATIC",L"",SS_OWNERDRAW|SS_NOTIFY,LayerPreview);
    e.layerName=e.controlWindow(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,LayerName);
    e.layerElevationInput=e.controlWindow(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,LayerElevationField);
    e.layerThicknessInput=e.controlWindow(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,LayerThicknessField);
    e.layerGroupList=e.controlWindow(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,LayerGroupField);
    e.layerOpacity=e.controlWindow(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,LayerOpacity);SendMessageW(e.layerOpacity,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    e.layerList = e.controlWindow(L"COMBOBOX", L"",
                                  CBS_DROPDOWNLIST | WS_VSCROLL, LayerList);
    e.snapList = e.controlWindow(L"COMBOBOX", L"",
                                 CBS_DROPDOWNLIST | WS_VSCROLL, SnapList);
    for (auto label : {L"25 cm", L"50 cm", L"1 m", L"Free"})
      SendMessageW(e.snapList, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label));
    SendMessageW(e.snapList, CB_SETCURSEL, 0, 0);
    e.musicList = e.controlWindow(L"COMBOBOX", L"",
                                  CBS_DROPDOWNLIST | WS_VSCROLL, MusicList);
    for (const auto &name : e.musicNames)
      SendMessageW(e.musicList, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(wide(name).c_str()));
    e.button(L"♫ Listen", ListenMusic);
    e.kindList = e.controlWindow(L"COMBOBOX", L"",
                                 CBS_DROPDOWNLIST | WS_VSCROLL, KindList);
    for (auto &s : e.schemas) {
      auto shown = plainKind(s.kind);
      if (s.fields.size() > 2 && s.fields[2] == "7")
        shown += L" · loose model";
      SendMessageW(e.kindList, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(shown.c_str()));
    }
    SendMessageW(e.kindList, CB_SETCURSEL, 0, 0);
    e.modelList = e.controlWindow(L"COMBOBOX", L"",
                                  CBS_DROPDOWNLIST | WS_VSCROLL, ModelList);
    e.modelSearch = e.controlWindow(
        L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, ModelSearch);
    SendMessageW(e.modelSearch, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"Find model..."));
    e.filterModels();
    e.materialList = e.controlWindow(
        L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, MaterialList);
    for (const auto &[id, name] : e.structureMaterials) {
      auto item = SendMessageW(
          e.materialList, CB_ADDSTRING, 0,
          reinterpret_cast<LPARAM>(
              wide(std::to_string(id) + "  " + name).c_str()));
      SendMessageW(e.materialList, CB_SETITEMDATA, item, id);
      if (id == e.structureMaterial)
        SendMessageW(e.materialList, CB_SETCURSEL, item, 0);
    }
    e.roleList = e.controlWindow(L"COMBOBOX", L"",
                                 CBS_DROPDOWNLIST | WS_VSCROLL, RoleList);
    for (auto role : Editor::people)
      SendMessageW(e.roleList, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(role.label));
    SendMessageW(e.roleList, CB_SETCURSEL, 1, 0);
    e.groundList = e.controlWindow(L"COMBOBOX", L"",
                                   CBS_DROPDOWNLIST | WS_VSCROLL, GroundList);
    for (auto label : {L"Short grass", L"Leafy grass", L"Clover", L"Wild grass",
                       L"Dry dirt", L"Forest litter", L"Gravel", L"Mud",
                       L"Meadow"})
      SendMessageW(e.groundList, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label));
    SendMessageW(e.groundList, CB_SETCURSEL, e.groundKind, 0);
    e.heightList = e.controlWindow(L"COMBOBOX", L"",
                                   CBS_DROPDOWNLIST | WS_VSCROLL, HeightList);
    for (auto label : {L"On the floor", L"Table height", L"On a shelf",
                       L"Up high"})
      SendMessageW(e.heightList, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label));
    SendMessageW(e.heightList, CB_SETCURSEL, 0, 0);
    e.surfaceList = e.controlWindow(
        L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, SurfaceList);
    struct SurfaceChoice {
      int id;
      const wchar_t *name;
    };
    static constexpr SurfaceChoice surfaces[] = {
        {-1, L"Game default"},      {0, L"Concrete"},
        {1, L"Pressure works"},     {2, L"Brick"},
        {3, L"Clinic tile"},        {4, L"Lab panel"},
        {5, L"Rock"},               {6, L"Corrugated"},
        {7, L"Office plaster"},     {8, L"Mine timber"},
        {9, L"Ancient stone"},      {10, L"Clinic paint teal"},
        {11, L"Clinic paint green"}, {12, L"Clinic paint maroon"},
    };
    for (auto choice : surfaces) {
      auto item = SendMessageW(e.surfaceList, CB_ADDSTRING, 0,
                               reinterpret_cast<LPARAM>(choice.name));
      SendMessageW(e.surfaceList, CB_SETITEMDATA, item, choice.id);
    }
    SendMessageW(e.surfaceList, CB_SETCURSEL, 0, 0);
    auto choices = [&](int id, std::initializer_list<const wchar_t *> items, int selected = 0) {
      HWND list = e.controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, id);
      for (auto item : items)
        SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
      SendMessageW(list, CB_SETCURSEL, selected, 0);
      return list;
    };
    // Plain names in the engine's own order: the list position is the native kind.
    e.hazardList = choices(HazardList, {L"Electricity", L"Steam", L"Crusher", L"Toxic gas", L"Fire", L"Falling debris", L"Pressure", L"Anomaly"});
    e.soundList = choices(SoundList, {});
    for (const auto &row : e.bankSounds)
      SendMessageW(e.soundList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>((row.label + (row.loop ? L"  (loop)" : L"")).c_str()));
    SendMessageW(e.soundList, CB_SETCURSEL, 0, 0);
    // A picked creature: who it comes out for (a person on its map) and its recurring-animal name.
    e.preyList = choices(PreyList, {});
    e.hunterList = e.controlWindow(L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, HunterList);
    // The Door tool's kind of door, and a picked door's boxes (NativeDoorMethods.inc).
    e.doorToolList = choices(DoorToolList, {L"Slides", L"Swings on a hinge"});
    e.doorKindList = choices(DoorKindList, {});
    e.doorAxisList = choices(DoorAxisList, {});
    e.doorSwingList = choices(DoorSwingList, {});
    e.doorHingeList = choices(DoorHingeList, {});
    e.doorLockList = choices(DoorLockList, {});
    e.doorStrengthList = e.controlWindow(L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, DoorStrengthList);
    // The Emitter tool's presets, and a picked emitter's boxes and sliders (NativeEmitterTool.inc).
    e.emitterPresetList = choices(EmitterPresetList, {});
    e.emitterRestyleList = choices(EmitterRestyleList, {L"Pick a preset..."});
    for (const auto &preset : Editor::emitterPresets)
      SendMessageW(e.emitterPresetList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(preset.name)),
          SendMessageW(e.emitterRestyleList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(preset.name));
    SendMessageW(e.emitterPresetList, CB_SETCURSEL, 0, 0);
    SendMessageW(e.emitterRestyleList, CB_SETCURSEL, 0, 0);
    e.emitterKindList = choices(EmitterKindList, {});
    for (auto name : Editor::emitterKindNames)
      SendMessageW(e.emitterKindList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    for (auto [slider, id] : {std::pair{&e.emitterHeadingSlider, int(EmitterHeadingSlider)}, std::pair{&e.emitterStrengthSlider, int(EmitterStrengthSlider)}})
      *slider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, id);
    e.pickupList =choices(PickupList, {L"Health", L"Shotgun shells", L"Handgun bullets"});
    e.creatureList = choices(CreatureList, {L"Huntsman", L"Wasp", L"Brute", L"Warden", L"Mutant", L"Snatcher", L"Chimera", L"Transformed", L"Cursed equipment"});
    e.upperFloorTiles=choices(UpperFloorTiles,{L"Floor",L"Open space"});
    e.tileList = choices(TileList, {L"Crate", L"Barrel (explodes)", L"Tank"});
    e.primList = choices(PrimList, {L"Box", L"Pillar", L"Stairs", L"Ramp", L"Column", L"Arch", L"Vault roof", L"Dome", L"Door frame", L"Window"}, 5);
    e.brushList = choices(BrushList, {L"Brush (hold and paint)", L"Box (drag a rectangle)", L"Fill (click an area)"});
    e.behaviourList = choices(BehaviourList, {L"Goes about their day", L"Stands still"});
    e.clutterList = choices(ClutterList, {L"Paper", L"Cardboard", L"Bottle", L"Electronics", L"More electronics", L"Plastic", L"Wood", L"A model (pick below)"});
    e.actionList = choices(ActionList, {L"Pick something to add...", L"Show a message", L"Turn a switch on", L"Count up", L"Take an item",
                                        L"Only if a switch is on", L"Fire when stepped in", L"Fire anywhere on this map",
                                        L"Fire anywhere in the campaign", L"Fire again and again (on/off)"});
    e.linkList = e.controlWindow(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_NOINTEGRALHEIGHT, LinkList);
    e.recordList = e.controlWindow(
        L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_BORDER, RecordList);
    e.form =
        CreateWindowExW(0, L"RawMetalNativeForm", L"",
                        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN, 0,
                        0, 10, 10, e.window, nullptr, instance, &e);
    e.plan =
        CreateWindowExW(0, L"RawMetalNativePlan", L"", WS_CHILD | WS_VISIBLE, 0,
                        0, 10, 10, e.window, nullptr, instance, &e);
    e.viewport = e.controlWindow(L"STATIC", L"", WS_CLIPCHILDREN, 0);
    e.statusText = e.controlWindow(L"STATIC", e.notice.c_str(), SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS, 0);
    for (int i = 0; i < ToolCount; i++) {
      auto tool = e.controlWindow(L"BUTTON", toolInfo[i].name,
                                  BS_OWNERDRAW | WS_TABSTOP, ToolBase + i,
                                  e.toolShelf);
      e.buttons.push_back(tool);
      e.tools.push_back(tool);
    }
    // One short line on hover for every button: the tool hints, then the bar.
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_WIN95_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&common);
    // Parts browser and paint palette: picture tiles from the engine's own renders.
    {
      WNDCLASSW pictures{};
      pictures.hInstance = instance;
      pictures.hCursor = LoadCursorW(nullptr, IDC_HAND);
      pictures.lpfnWndProc = gridProc;
      pictures.lpszClassName = L"RawMetalPictures";
      RegisterClassW(&pictures);
      e.thumbs.folder = e.thumbFolder();
      for (auto *g : {&e.partsGrid, &e.paintGrid}) {
        g->cache = &e.thumbs;
        g->font = e.smallFont;
        g->hwnd = CreateWindowExW(0, L"RawMetalPictures", L"", WS_CHILD | WS_VSCROLL, 0, 0, 10, 10, e.window,
                                  nullptr, instance, g);
      }
      e.partsGrid.saved = &e.savedParts;
      e.partsGrid.cell = 100;
      e.paintGrid.cell = 76;
      e.paintGrid.showGroups = true;
      e.paintGrid.groupNames = Editor::paintSections;
      e.partsGrid.onPick = [&e](int i, bool keep) { e.pickPart(i, keep); };
      e.paintGrid.onPick = [&e](int i, bool) { e.pickPaint(i); };
      e.partsGrid.onHover = [&e](int i) { e.message(e.partsGrid.items[size_t(i)].name + L". Click to use it, right-click to save it."); };
      e.paintGrid.onHover = [&e](int i) { e.message(e.paintGrid.items[size_t(i)].name); };
      e.partsGrid.placeholder = [](const parts::Item &item) { return ui::PageTint[size_t(item.group) % 5]; };
      e.paintGrid.placeholder = [](const parts::Item &item) { return ui::PageTint[size_t(item.group) % 5]; };
      for (int i = 0; i < parts::TabCount; ++i)
        e.partTabButtons.push_back(e.button(parts::partTabs[i], PartTabBase + i));
      for (HWND b : e.partTabButtons)
        ShowWindow(b, SW_HIDE);
      std::ifstream saved(e.root / L".build" / L"parts-saved.txt");
      for (std::string key; std::getline(saved, key);)
        if (!key.empty())
          e.savedParts.insert(key);
    }
    // Lamp controls: drag and the picture relights.
    e.brightSlider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, BrightSlider);
    // A picked door's Starts open (NativeDoorMethods.inc).
    e.doorOpenSlider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, DoorOpenSlider);
    SendMessageW(e.doorOpenSlider, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
    e.reachSlider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, ReachSlider);
    e.warmSlider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, WarmSlider);
    SendMessageW(e.brightSlider, TBM_SETRANGE, FALSE, MAKELPARAM(0, 400));
    SendMessageW(e.reachSlider, TBM_SETRANGE, FALSE, MAKELPARAM(1, 30));
    SendMessageW(e.warmSlider, TBM_SETRANGE, FALSE, MAKELPARAM(2000, 6500));
    for (size_t i = 0; i < std::size(Editor::gels); ++i)
      e.swatches.push_back(e.button(Editor::gels[i].name, SwatchBase + int(i)));
    e.button(L"Flicker: off", FlickerToggle);
    e.button(L"Sound: on", SoundToggle);
    e.button(L"\u2600 Light: lit \u25BE", LightViewButton);
    e.button(L"Play: first person", ThirdPersonToggle);
    e.button(L"Write film list", FilmWrite);
    e.button(L"\u25B6 Render films", FilmRender);
    e.button(L"\u2707 Raw Metal Filmmaker...", FilmMaker);
    e.button(L"↺ Start fresh", StartFresh);
    for (int i = 0; i < 3; ++i)
      e.ideaButtons.push_back(e.button(L"", IdeaBase + i));
    for (int i = 0; i < 4; ++i)
      e.moodButtons.push_back(e.button(Editor::moodNames[i], MoodBase + i));
    e.button(L"Make walls", OutlineWalls);
    e.button(L"Make a block", OutlineBlock);
    e.button(L"Terraces", OutlineTerraces);
    e.extrudeList = e.controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, ExtrudeList);
    for (auto text : {L"Kerb (0.5 m)", L"Waist (1 m)", L"Wall (2 m)", L"Tall (3 m)"})
      SendMessageW(e.extrudeList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(e.extrudeList, CB_SETCURSEL, 1, 0);
    e.heightModeList = e.controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, HeightModeList);
    for (auto text : {L"Raise (up half a metre)", L"Lower (down again)", L"Steps (climb as you drag)"})
      SendMessageW(e.heightModeList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(e.heightModeList, CB_SETCURSEL, 0, 0);
    e.historySlider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, 0);
    e.timeSlider = e.controlWindow(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS, 0);
    SendMessageW(e.timeSlider, TBM_SETRANGE, FALSE, MAKELPARAM(0, 3));
    SendMessageW(e.timeSlider, TBM_SETPOS, TRUE, 0);
    for (HWND c : e.ideaButtons)
      ShowWindow(c, SW_HIDE);
    for (HWND c : e.moodButtons)
      ShowWindow(c, SW_HIDE);
    for (int id : {OutlineWalls, OutlineBlock, OutlineTerraces})
      ShowWindow(GetDlgItem(e.window, id), SW_HIDE);
    ShowWindow(e.extrudeList, SW_HIDE);
    ShowWindow(e.heightModeList, SW_HIDE);
    ShowWindow(e.timeSlider, SW_HIDE);
    for (HWND c : {e.brightSlider, e.reachSlider, e.warmSlider, GetDlgItem(e.window, FlickerToggle)})
      ShowWindow(c, SW_HIDE);
    for (HWND c : e.swatches)
      ShowWindow(c, SW_HIDE);
    if (!e.backgroundTest && !e.demoMode) {
      sfx::load(e.root / L".build" / L"sounds");
      std::error_code ec;
      sfx::muted = fs::exists(e.root / L".build" / L"editor-sound-muted", ec);
      ui::clickSound = [] { sfx::play(sfx::Click, .6f, .05f); };
      ui::hoverSound = [](HWND h) {
        int id = GetDlgCtrlID(h);
        if ((id >= ToolBase && id < ToolBase + ToolCount) || (id >= GroupBase && id < GroupBase + 5))
          sfx::play(sfx::Hover, .5f, .08f);
      };
    }
    e.tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, e.window, nullptr, instance, nullptr);
    auto tip = [&](HWND control, const wchar_t *text) {
      TOOLINFOW info{sizeof(info)};
      info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
      info.hwnd = GetParent(control);
      info.uId = reinterpret_cast<UINT_PTR>(control);
      info.lpszText = const_cast<wchar_t *>(text);
      SendMessageW(e.tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
    };
    if (e.tooltip) {
      for (int i = 0; i < ToolCount; ++i)
        tip(e.tools[size_t(i)], toolInfo[i].hint);
      for (auto [id, text] : std::initializer_list<std::pair<int, const wchar_t *>>{
               {New, L"Start a new, empty map."}, {Open, L"Open a map you made before."},
               {FromCampaign, L"Open one of the game's own levels to change it."},
               {IntoGame, L"Make the game play this level as you have it now."},
               {BackToOriginal, L"Make the game play this level's original again."},
               {ExportChanges, L"Put every campaign level you changed in one file to send. Ctrl+Shift+E."},
               {Save, L"Keep your work. Ctrl+S does the same."}, {Undo, L"Take back the last change. Ctrl+Z."},
               {Redo, L"Put it back again. Ctrl+Y."}, {Walk, L"Walk your map right now. Escape comes back. Over the plan, F5 starts at the pointer."},
               {PlayStart, L"Play from where the player begins. Shift+F5."}, {PlanOnly, L"Paint on the map from above."},
               {Dual, L"Paint from above and see it in 3D."}, {ViewOnly, L"Build straight in the 3D picture."},
               {Starter, L"Open the finished example to look around."}, {Export, L"Save under a new name."}, {FullGame, L"Leave the builder and play this map in the real game. The game's menu brings you back here."},
               {AddMap, L"Choose the edge to join a new map. Occupied edges are disabled."}, {AddFloor, L"Add a floor above this one."},
               {Furnish, L"Hang lamps over every open floor and sign each door."}, {Outdoors, L"Open the sky and grow land around this map."},
               {Remove, L"Delete what is picked. Delete key."}, {Duplicate, L"Copy what is picked. Ctrl+D."},
               {TurnSelected, L"Turn what is picked a quarter. R key."}, {Advanced, L"Show every number the game reads."},
               {ListenMusic, L"Hear this map's music."}, {AutoDetail, L"Click to switch."}})
        if (HWND control = GetDlgItem(e.window, id))
          tip(control, text);
      for (int g = 0; g < 5; ++g)
        tip(e.groupTabs[size_t(g)], groupNames[g]);
    }
    trace("controls created");
    e.chooseKind("FIXTURE");
    SendMessageW(e.modelList, CB_SETCURSEL, 0, 0);
    e.refresh();
    e.layout();
    trace("laid out");
    if (!e.backgroundTest && !e.editingTest && !e.layoutTest && !e.remakeTest)
      e.makeThumbnails(false);
    trace("thumbnails listed");
    if (!e.restoredFrom.empty()) {
      e.restoredAt = e.changeCount;
      e.message(L"Brought back your unsaved work from last time. Start fresh?");
    }
    if (!e.backgroundTest && !e.editingTest && !e.layoutTest)
      e.welcomeIfBlank();
    if (e.editingTest) {
      e.verifyEditing();
      DestroyWindow(e.window);
      return 0;
    }
    if (e.remakeTest) {
      e.verifyRemake();
      DestroyWindow(e.window);
      return 0;
    }
    if (e.funTest) {
      try {
        e.verifyFun();
      } catch (const std::exception &error) {
        std::ofstream(e.root / L"native-editor-fun-test.txt") << "FAIL: " << error.what() << "\n";
        e.dirty=false;DestroyWindow(e.window);return 1;
      }
      e.dirty = false;
      DestroyWindow(e.window);
      return 0;
    }
    // ---- Simulate (F6): the toggle button and its handler (NativeSimulate.h) ----
    simulate::install(e);
    if (simulate::testRun)
      return simulate::runTest(e);
    // ---- end Simulate ----
    if (demoScene)
      e.demo(demoScene);
    if (e.layoutTest) {
      e.verifyLayout();
      DestroyWindow(e.window);
      return 0;
    }
    if (e.perfTest) {
      e.verifyPerf();
      DestroyWindow(e.window);
      return 0;
    }
    trace("controls ready");
    if (!e.backgroundTest && (!e.menuTest || e.menuShown)) {
      ShowWindow(e.window, SW_SHOWMAXIMIZED);
      UpdateWindow(e.window);
    }
    trace("window shown");
    e.testStarted = GetTickCount64();
    e.startEngine();
    trace("engine started");
    SetTimer(e.window, 1, 50, nullptr);
    e.statusTimer = true;
    SetTimer(e.window, 2, 15000, nullptr);
    if (e.menuTest)
      SetTimer(e.window, 9, 15000, nullptr);
    trace("message loop");
    MSG message{};
    while (!e.quitLoop && GetMessageW(&message, nullptr, 0, 0) > 0) {
      try {
        if (e.shortcut(message))
          continue;
      } catch (const std::exception &error) {
        e.message(wide(error.what()));
        continue;
      }
      if (!IsDialogMessageW(e.window, &message)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
      if (e.quitLoop)
        break;
    }
    trace(std::string("left the editor, code ") + (e.playExit ? "2" : "0"));
    return e.playExit ? 2 : 0;
  } catch (const std::exception &x) {
    trace(std::string("error: ") + x.what());
    std::ofstream("native-editor-error.txt") << x.what();
    return 1;
  }
}

int runEditorFromMenu(HINSTANCE instance, bool fresh, const wchar_t *project,
                      wchar_t *playFile, int playFileChars) {
  EditorDoor door;
  door.fresh = fresh;
  if (project && *project)
    door.project = project;
  if (door.project.empty())
    door.fresh = true;
  fs::path played;
  int code = editorSession(instance, &door, &played);
  if (code == 2 && playFile && playFileChars > 1)
    wcsncpy_s(playFile, static_cast<size_t>(playFileChars), played.c_str(),
              _TRUNCATE);
  return code;
}

struct FrontDoor {
  std::vector<fs::path> projects;
  int choice = 0;
  int selected = -1;
};
static LRESULT CALLBACK frontProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto *door = reinterpret_cast<FrontDoor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    door = static_cast<FrontDoor *>(
        reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(door));
  }
  if (!door)
    return DefWindowProcW(h, m, w, l);
  if (m == WM_COMMAND) {
    int id = LOWORD(w);
    HWND list = GetDlgItem(h, 40);
    if (id == 40 && HIWORD(w) == LBN_SELCHANGE)
      door->selected = int(SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (id == 40 && HIWORD(w) == LBN_DBLCLK)
      id = 2;
    if (id == 1)
      door->choice = 1;
    if (id == 2 && door->selected >= 0 &&
        door->selected < int(door->projects.size()))
      door->choice = 2;
    if (id == 3)
      door->choice = 3;
    if (id == 4)
      door->choice = 4;
    if (door->choice)
      DestroyWindow(h);
    return 0;
  }
  if (m == WM_MEASUREITEM) {
    reinterpret_cast<MEASUREITEMSTRUCT *>(l)->itemHeight = 26;
    return TRUE;
  }
  if (m == WM_DRAWITEM) {
    auto *d = reinterpret_cast<DRAWITEMSTRUCT *>(l);
    auto font = reinterpret_cast<HFONT>(SendMessageW(d->hwndItem, WM_GETFONT, 0, 0));
    if (d->CtlType == ODT_LISTBOX) {
      ui::drawListRow(d, font);
      return TRUE;
    }
    int id = int(d->CtlID);
    auto look = id == 3 ? ui::Look::Play : id == 1 ? ui::Look::Chosen : ui::Look::Plain;
    RECT body = ui::chunkyButton(d->hDC, d->rcItem, ui::Paper, look, ui::hotControl == d->hwndItem,
                                 d->itemState & ODS_SELECTED, 14);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, ui::lookInk(look));
    auto old = SelectObject(d->hDC, font);
    auto text = label(d->hwndItem);
    DrawTextW(d->hDC, text.c_str(), int(text.size()), &body, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(d->hDC, old);
    return TRUE;
  }
  if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLORLISTBOX) {
    static HBRUSH paper = CreateSolidBrush(ui::Paper), field = CreateSolidBrush(ui::Field);
    bool list = m == WM_CTLCOLORLISTBOX;
    SetTextColor(reinterpret_cast<HDC>(w), list ? ui::Ink : ui::InkSoft);
    SetBkColor(reinterpret_cast<HDC>(w), list ? ui::Field : ui::Paper);
    return reinterpret_cast<LRESULT>(list ? field : paper);
  }
  if (m == WM_PAINT) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(h, &ps);
    RECT r{};
    GetClientRect(h, &r);
    RECT band{0, 0, r.right, 58};
    auto header = CreateSolidBrush(ui::Header);
    FillRect(dc, &band, header);
    DeleteObject(header);
    SetBkMode(dc, TRANSPARENT);
    auto title = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                             CLEARTYPE_QUALITY, 0, L"Segoe UI");
    auto rest = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Segoe UI");
    auto old = SelectObject(dc, title);
    SetTextColor(dc, ui::TerracottaEdge);
    textAt(dc, 16, 14, L"Raw Metal");
    SIZE first{};
    GetTextExtentPoint32W(dc, L"Raw Metal ", 10, &first);
    SelectObject(dc, rest);
    SetTextColor(dc, ui::Ink);
    textAt(dc, 16 + first.cx, 19, L"Campaign Builder");
    SelectObject(dc, old);
    DeleteObject(title);
    DeleteObject(rest);
    EndPaint(h, &ps);
    return 0;
  }
  if (m == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  if (m == WM_CLOSE) {
    door->choice = 4;
    DestroyWindow(h);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static int frontDoor(HINSTANCE instance, const fs::path &root, EditorDoor &out) {
  FrontDoor door;
  std::error_code ec;
  fs::path maps = root / L"custom maps";
  if (fs::is_directory(maps, ec)) {
    for (const auto &entry : fs::directory_iterator(maps, ec)) {
      if (!entry.is_regular_file(ec))
        continue;
      auto ext = entry.path().extension().wstring();
      for (auto &c : ext)
        c = wchar_t(towlower(c));
      if (ext == L".rmc" || ext == L".txt")
        door.projects.push_back(entry.path());
    }
  }
  std::sort(door.projects.begin(), door.projects.end(),
            [](const fs::path &a, const fs::path &b) {
              bool as = a.filename() == L"Starter Map.rmc";
              bool bs = b.filename() == L"Starter Map.rmc";
              if (as != bs)
                return as;
              return a.filename().wstring() < b.filename().wstring();
            });
  WNDCLASSW wc{};
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = CreateSolidBrush(ui::Paper);
  wc.lpfnWndProc = frontProc;
  wc.lpszClassName = L"RawMetalFrontDoor";
  RegisterClassW(&wc);
  HWND window = CreateWindowExW(
      0, wc.lpszClassName, L"Raw Metal Campaign Builder", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
      CW_USEDEFAULT, CW_USEDEFAULT, 460, 640, nullptr, nullptr, instance, &door);
  HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                           L"Segoe UI");
  auto add = [&](const wchar_t *text, int id, int x, int y, int w, int h,
                 const wchar_t *cls, DWORD style) {
    HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x,
                                 y, w, h, window,
                                 reinterpret_cast<HMENU>(INT_PTR(id)), instance,
                                 nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (_wcsicmp(cls, L"BUTTON") == 0)
      ui::cosy(child, 1);
    else if (style & WS_BORDER)
      ui::cosy(child, 2);
    return child;
  };
  add(L"New campaign starts blank. Open a project to keep editing it. Play runs the game.",
      0, 16, 72, 412, 44, L"STATIC", SS_LEFT);
  add(L"New campaign", 1, 16, 122, 412, 42, L"BUTTON", WS_TABSTOP | BS_OWNERDRAW);
  add(L"Projects", 0, 16, 176, 412, 22, L"STATIC", SS_LEFT);
  HWND list = add(L"", 40, 16, 200, 412, 280, L"LISTBOX",
                  LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
  for (const auto &project : door.projects)
    SendMessageW(list, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(project.filename().c_str()));
  if (!door.projects.empty())
    SendMessageW(list, LB_SETCURSEL, 0, 0), door.selected = 0;
  add(L"Open project", 2, 16, 492, 200, 42, L"BUTTON", WS_TABSTOP | BS_OWNERDRAW);
  add(L"▶ Play", 3, 228, 492, 200, 42, L"BUTTON", WS_TABSTOP | BS_OWNERDRAW);
  add(L"Quit", 4, 16, 546, 412, 36, L"BUTTON", WS_TABSTOP | BS_OWNERDRAW);
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  MSG message{};
  // Play used to post a thread quit when its window closed. Ignore that
  // until this door itself is closing, or quitting the game leaves the program.
  for (;;) {
    if (GetMessageW(&message, nullptr, 0, 0) < 0)
      break;
    if (message.message == WM_QUIT) {
      if (!door.choice)
        continue;
      break;
    }
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  DeleteObject(font);
  if (door.choice == 1) {
    out.fresh = true;
    return 1;
  }
  if (door.choice == 2) {
    out.fresh = false;
    out.project = door.projects[size_t(door.selected)];
    return 1;
  }
  if (door.choice == 3)
    return 2;
  return 0;
}

int runNativeEditor(HINSTANCE instance) {
  int argc = 0;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  bool interactive = argv && argc <= 1;
  if (argv)
    LocalFree(argv);
  if (!interactive)
    return editorSession(instance, nullptr);
  wchar_t executable[32768]{};
  GetModuleFileNameW(nullptr, executable, 32768);
  fs::path root = fs::path(executable).parent_path();
  for (;;) {
    EditorDoor door;
    int choice = frontDoor(instance, root, door);
    if (choice == 0)
      return 0;
    if (choice == 2)
      return 2;
    if (editorSession(instance, &door) != 0) {
      std::ifstream in("native-editor-error.txt");
      std::string error(std::istreambuf_iterator<char>(in), {});
      MessageBoxA(nullptr,
                  error.empty() ? "The editor closed with an error."
                                : error.c_str(),
                  "Raw Metal Campaign Builder", MB_OK | MB_ICONERROR);
    }
  }
}

#ifndef RAWMETAL_UNIFIED
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  return runNativeEditor(instance);
}
#endif
