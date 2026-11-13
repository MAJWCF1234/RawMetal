// The Emitter tool: the air's own stuff as EMITTER records (src/world/CustomCampaign.cpp
// reads them, src/renderer/Particles.cpp draws them). Its presets are the kit's helpers
// (src/world/CampaignKit.h: steamLeak, steamJet, smokeColumn, sparkShower, dustMotes,
// dripping, waterSpray, fire, toxicPool, ceilingGrit, arcGlow) written as the record each
// makes. A picked emitter's boxes say what it is, restyle it from a preset, point it and
// set how strong it is; its numbers, its switch and its timed window are the fields below.
// The plan draws each kind's glyph, its size and the way it is carried.
// Included inside struct Editor.
HWND emitterPresetList = nullptr, emitterKindList = nullptr, emitterRestyleList = nullptr,
     emitterHeadingSlider = nullptr, emitterStrengthSlider = nullptr;
int emitterPreset = 0; // the preset the next click places
HFONT airFont = nullptr; // the plan's glyphs, made for the plan's cell size
int airFontSize = 0;
// A kit helper as numbers: kind (World.h ParticleKind), where it leaves (metres above the
// floor, or below the ceiling), where a puff is carried over its life (travel), how long
// it lives, how many are in the air at once, its size as it leaves, its swell, strength.
struct EmitterPreset {
  const wchar_t *name;
  int kind;
  float lift;
  bool fromCeiling;
  float travelX, travelY, travelZ, life;
  int count;
  float size, swell, strength;
};
static constexpr EmitterPreset emitterPresets[] = {
    {L"Steam leak", 2, 1.2f, false, .8f, 0, .2f, 1.8f, 32, .04f, .5f, .7f},
    {L"Steam jet", 2, 1.f, false, 2.f, 0, .3f, 1.1f, 34, .05f, .7f, 1.f},
    {L"Smoke column", 1, .1f, false, .15f, .08f, 2.5f, 6.f, 22, .25f, 1.3f, .7f},
    {L"Spark shower", 3, 2.f, false, .3f, 0, -.4f, 5.f, 24, .01f, 0, 1.f},
    {L"Dust motes", 4, .3f, false, 1.5f, 1.5f, 2.f, 12.f, 40, .013f, .004f, .8f},
    {L"Drips", 6, .1f, true, 0, 0, 0, 2.f, 2, .008f, 0, 1.f},
    {L"Spray", 7, 1.f, false, 1.f, 0, .3f, 1.2f, 24, .04f, .45f, .8f},
    {L"Fire", 8, 0, false, 0, 0, .6f, 1.f, 20, .3f, .6f, 1.f},
    {L"Toxic pool", 9, 0, false, 1.5f, 1.5f, .5f, 7.f, 13, .3f, .8f, .8f},
    {L"Ceiling grit", 5, .05f, true, 0, 0, 0, 2.f, 20, .02f, 0, .8f},
    {L"Arc glow", 10, 1.2f, false, 0, 0, .3f, 1.2f, 8, .3f, .1f, .8f},
    {L"Old plume", 0, .3f, false, .2f, .1f, 2.4f, 1.f / .3f, 7, .1f, .5f, 1.f},
};
static constexpr int EmitterPresetCount = int(std::size(emitterPresets));
// ParticleKind's order, so a list position is the record's kind field.
static constexpr const wchar_t *emitterKindNames[] = {
    L"Old plume (steam in, smoke out)", L"Smoke", L"Steam", L"Sparks", L"Dust motes", L"Ceiling grit",
    L"Drips", L"Spray", L"Fire and embers", L"Toxic gas", L"Glow round live current"};
static constexpr const wchar_t *emitterGlyphs[] = {L"♨", L"☁", L"≈", L"✶", L"∴", L"⁘",
                                                    L"⋮", L"≋", L"▲", L"☣", L"ϟ"};
static COLORREF emitterColor(int kind) {
  static constexpr COLORREF colors[] = {RGB(0xC8, 0xC8, 0xC8), RGB(0x9A, 0x9A, 0x9A), RGB(0xE8, 0xF0, 0xFF),
                                        RGB(0xFF, 0xC8, 0x3C), RGB(0xD8, 0xC8, 0xA0), RGB(0xB0, 0x8A, 0x60),
                                        RGB(0x5A, 0xA0, 0xFF), RGB(0x8C, 0xD8, 0xFF), RGB(0xFF, 0x6A, 0x20),
                                        RGB(0x7C, 0xE0, 0x40), RGB(0x50, 0xE0, 0xFF)};
  return colors[std::clamp(kind, 0, 10)];
}
// Smoke, steam, sparks, spray and the old plume are carried one way; dust and gas fill a
// box (the drift is its half sizes); fire, grit, drips and the glow stand where they are.
static bool emitterPoints(int kind) { return kind == 0 || kind == 1 || kind == 2 || kind == 3 || kind == 7; }
static bool emitterFillsBox(int kind) { return kind == 4 || kind == 9; }
static int emitterKindOf(const std::vector<std::string> &f) {
  return f.size() > 12 && !f[12].empty() ? std::clamp(int(number(f[12], 0)), 0, 10) : 0;
}
// An emitter's numbers to six figures, so a float's tail (0.30000001) never shows.
static std::string airNumber(float v) {
  std::ostringstream out;
  out.precision(6);
  out << (std::abs(v) < 1e-6f ? 0.f : v);
  return out.str();
}
// The record a preset makes at (x, y) on this floor: EMITTER|map|x|y|z|driftX|driftY|rise|
// rate|radius|growth|count|kind|strength.
std::vector<std::string> emitterFields(const EmitterPreset &p, float x, float y) const {
  float floorZ = layerElevation();
  float z = p.fromCeiling ? std::max(floorZ + .5f, creationCeiling() - p.lift) : floorZ + p.lift;
  return {"EMITTER", std::to_string(map), nativeNumber(x), nativeNumber(y), airNumber(z),
          airNumber(p.travelX), airNumber(p.travelY), airNumber(p.travelZ),
          airNumber(1.f / std::max(.2f, p.life)), airNumber(p.size), airNumber(p.swell),
          std::to_string(std::clamp(p.count, 1, 64)), std::to_string(p.kind), airNumber(p.strength)};
}
// The first preset of a kind (a kind picked in the box takes its numbers).
static int presetOfKind(int kind) {
  for (int i = 0; i < EmitterPresetCount; ++i)
    if (emitterPresets[i].kind == kind)
      return i;
  return 0;
}
bool emitterPicked() const {
  return record >= 0 && record < int(doc.lines.size()) && selectedRecords.size() <= 1 &&
         NativeDocument::fields(doc.lines[size_t(record)])[0] == "EMITTER";
}
// The picked emitter rewritten as one Undo; false when nothing changed.
bool setPickedEmitter(std::vector<std::string> f, const std::wstring &said) {
  if (!emitterPicked())
    return false;
  if (!editableRecord(size_t(record))) {
    message(L"Unlock and show this floor before editing its content.");
    return false;
  }
  while (f.size() > 12 && f.back().empty())
    f.pop_back();
  auto before = NativeDocument::fields(doc.lines[size_t(record)]);
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
// A preset's numbers on the picked emitter: where it stands, its height (unless the preset
// hangs from the ceiling), its switch and its timed window stay; a carried kind keeps its heading.
void restyleEmitter(int preset, const wchar_t *how) {
  if (!emitterPicked() || preset < 0 || preset >= EmitterPresetCount)
    return;
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  if (f.size() < 12)
    f.resize(12);
  const auto &p = emitterPresets[preset];
  auto made = emitterFields(p, number(f[2], 0), number(f[3], 0));
  if (!p.fromCeiling)
    made[4] = f[4];
  float dx = number(f[5], 0), dy = number(f[6], 0);
  if (emitterPoints(emitterKindOf(f)) && emitterPoints(p.kind) && std::hypot(dx, dy) > .02f) {
    float heading = std::atan2(dy, dx), reach = std::hypot(p.travelX, p.travelY);
    made[5] = airNumber(std::cos(heading) * reach), made[6] = airNumber(std::sin(heading) * reach);
  }
  for (size_t i = 14; i < f.size(); ++i)
    made.push_back(f[i]);
  setPickedEmitter(made, std::wstring(how) + L" " + p.name + L": its numbers are below. Ctrl+Z takes it back.");
  syncEmitterControls();
}
// R, or the Points slider: the way a carried kind goes, its reach kept.
bool turnEmitter(float degrees, bool absolute) {
  if (!emitterPicked())
    return false;
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  if (f.size() < 12)
    return false;
  int kind = emitterKindOf(f);
  if (!emitterPoints(kind)) {
    message(emitterFillsBox(kind) ? L"Dust and gas fill a box (the drift is its half sizes): they point nowhere."
                                  : L"Fire, grit, drips and the glow stand where they are: they point nowhere.");
    syncEmitterControls();
    return true;
  }
  float dx = number(f[5], 0), dy = number(f[6], 0), reach = std::hypot(dx, dy);
  if (reach < .02f)
    reach = .5f, dx = 1, dy = 0;
  float heading = absolute ? degrees * 3.14159265f / 180.f : std::atan2(dy, dx) + degrees * 3.14159265f / 180.f;
  f[5] = airNumber(std::round(std::cos(heading) * reach * 1000.f) / 1000.f);
  f[6] = airNumber(std::round(std::sin(heading) * reach * 1000.f) / 1000.f);
  setPickedEmitter(f, L"It points " + std::to_wstring(int(std::lround(emitterHeading(f)))) +
                          L" degrees now (0 east, 90 south). R turns it on; Ctrl+Z takes it back.");
  syncEmitterControls();
  return true;
}
static float emitterHeading(const std::vector<std::string> &f) {
  float d = std::atan2(number(f.size() > 6 ? f[6] : "", 0), number(f.size() > 5 ? f[5] : "", 1)) * 180.f / 3.14159265f;
  return d < 0 ? d + 360.f : d;
}
void syncEmitterControls() {
  if (!emitterKindList || !emitterPicked())
    return;
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  SendMessageW(emitterKindList, CB_SETCURSEL, emitterKindOf(f), 0);
  SendMessageW(emitterRestyleList, CB_SETCURSEL, 0, 0);
  SendMessageW(emitterHeadingSlider, TBM_SETRANGE, FALSE, MAKELPARAM(0, 359));
  SendMessageW(emitterHeadingSlider, TBM_SETPOS, TRUE, LPARAM(std::lround(emitterHeading(f)) % 360));
  EnableWindow(emitterHeadingSlider, emitterPoints(emitterKindOf(f)));
  SendMessageW(emitterStrengthSlider, TBM_SETRANGE, FALSE, MAKELPARAM(5, 400));
  SendMessageW(emitterStrengthSlider, TBM_SETPOS, TRUE,
               LPARAM(std::lround(std::clamp(number(f.size() > 13 ? f[13] : "", 1), .05f, 4.f) * 100)));
}
void emitterControlChanged(int id, int notification) {
  if (notification != CBN_SELCHANGE)
    return;
  if (id == EmitterPresetList) {
    emitterPreset = std::clamp(listIndex(emitterPresetList), 0, EmitterPresetCount - 1);
    message(std::wstring(L"Next: ") + emitterPresets[emitterPreset].name + L". Click where it comes from.");
    if (plan)
      InvalidateRect(plan, nullptr, FALSE);
    return;
  }
  if (!emitterPicked())
    return;
  if (id == EmitterKindList) {
    int kind = listIndex(emitterKindList);
    if (kind != emitterKindOf(NativeDocument::fields(doc.lines[size_t(record)])))
      restyleEmitter(presetOfKind(kind), L"It is now");
  } else if (id == EmitterRestyleList) {
    int choice = listIndex(emitterRestyleList);
    if (choice > 0)
      restyleEmitter(choice - 1, L"Restyled as");
  }
}
// The sliders write when the thumb is let go: one Undo each.
void emitterSliderChanged(HWND slider, int code) {
  if (code != TB_ENDTRACK || !emitterPicked())
    return;
  int at = int(SendMessageW(slider, TBM_GETPOS, 0, 0));
  if (slider == emitterHeadingSlider) {
    turnEmitter(float(at), true);
    return;
  }
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  if (f.size() < 14)
    f.resize(14);
  if (f[12].empty())
    f[12] = "0";
  f[13] = airNumber(std::clamp(at, 5, 400) / 100.f);
  setPickedEmitter(f, L"Strength " + wide(trimmed(at / 100.f)) + L" (1 usual, up to 4). Ctrl+Z takes it back.");
  syncEmitterControls();
}
// The plan: each emitter's glyph in its kind's colour, its size as it leaves (a fire its
// width), the box dust and gas fill, an arrow the way it is carried, and a dotted ring when
// a switch or a timed window gates it.
void drawEmitterGlyphs(HDC dc, int ox, int oy, float cell) {
  int glyphSize = std::max(13, int(cell * .8f));
  if (airFont && glyphSize != airFontSize)
    DeleteObject(airFont), airFont = nullptr;
  airFontSize = glyphSize;
  if (!airFont)
    airFont = CreateFontW(-glyphSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI Symbol");
  auto oldFont = SelectObject(dc, airFont);
  SetBkMode(dc, TRANSPARENT);
  for (auto index : doc.records("EMITTER", map)) {
    if (!visibleFloorRecord(index))
      continue;
    auto f = NativeDocument::fields(doc.lines[index]);
    if (f.size() < 12)
      continue;
    int kind = emitterKindOf(f);
    COLORREF color = emitterColor(kind);
    float x = number(f[2], 0), y = number(f[3], 0), dx = number(f[5], 0), dy = number(f[6], 0);
    int px = int(ox + x * cell), py = int(oy + y * cell);
    {
      // A dark disc under it, so the glyph reads over the grid.
      int r = std::max(6, int(cell * .36f));
      auto disc = CreateSolidBrush(ui::PlanBlack);
      auto oldBrush = SelectObject(dc, disc), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
      Ellipse(dc, px - r, py - r, px + r + 1, py + r + 1);
      SelectObject(dc, oldPen), SelectObject(dc, oldBrush);
      DeleteObject(disc);
    }
    auto line = [&](int x0, int y0, int x1, int y1, HPEN p) {
      auto old = SelectObject(dc, p);
      MoveToEx(dc, x0, y0, nullptr);
      LineTo(dc, x1, y1);
      SelectObject(dc, old);
      DeleteObject(p);
    };
    auto hollow = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    if (emitterFillsBox(kind)) {
      auto p = ui::dashedPen(1, color);
      auto old = SelectObject(dc, p);
      Rectangle(dc, int(ox + (x - std::abs(dx)) * cell), int(oy + (y - std::abs(dy)) * cell),
                int(ox + (x + std::abs(dx)) * cell) + 1, int(oy + (y + std::abs(dy)) * cell) + 1);
      SelectObject(dc, old);
      DeleteObject(p);
    } else {
      int r = std::max(5, int(number(f[9], .1f) * cell));
      auto p = CreatePen(PS_SOLID, 1, color);
      auto old = SelectObject(dc, p);
      Ellipse(dc, px - r, py - r, px + r + 1, py + r + 1);
      SelectObject(dc, old);
      DeleteObject(p);
    }
    bool gated = (f.size() > 14 && !f[14].empty()) || (f.size() > 17 && number(f[17], 0) > 0);
    if (gated) {
      int r = std::max(9, int(cell * .45f));
      auto p = CreatePen(PS_DOT, 1, color);
      auto old = SelectObject(dc, p);
      Ellipse(dc, px - r, py - r, px + r + 1, py + r + 1);
      SelectObject(dc, old);
      DeleteObject(p);
    }
    SelectObject(dc, hollow);
    float reach = std::hypot(dx, dy);
    if (emitterPoints(kind) && reach > .02f) {
      float ux = dx / reach, uy = dy / reach, length = std::clamp(reach * cell, cell * .7f, cell * 3.f);
      int tx = px + int(ux * length), ty = py + int(uy * length);
      line(px, py, tx, ty, CreatePen(PS_SOLID, 2, color));
      float head = std::max(5.f, cell * .22f);
      for (float side : {-1.f, 1.f}) {
        float hx = -ux * .8f - uy * side * .6f, hy = -uy * .8f + ux * side * .6f;
        line(tx, ty, tx + int(hx * head), ty + int(hy * head), CreatePen(PS_SOLID, 2, color));
      }
    }
    SetTextColor(dc, color);
    RECT box{px - int(cell), py - int(cell), px + int(cell), py + int(cell)};
    DrawTextW(dc, emitterGlyphs[kind], -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
  }
  SelectObject(dc, oldFont);
}
// The picked emitter in the 3D view: the way a puff is carried (and how far up), or the box
// that dust and gas fill.
void emitterGuides(std::vector<retro::EditorGuide> &out) const {
  if (!emitterPicked())
    return;
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  if (f.size() < 12)
    return;
  int kind = emitterKindOf(f);
  float x = number(f[2], 0), y = number(f[3], 0), z = number(f[4], 0), dx = number(f[5], 0), dy = number(f[6], 0),
        rise = number(f[7], 0);
  if (emitterFillsBox(kind)) {
    float bottom = kind == 9 ? z : z, top = z + std::max(.05f, rise);
    std::pair<float, float> c[4] = {{x - std::abs(dx), y - std::abs(dy)}, {x + std::abs(dx), y - std::abs(dy)},
                                    {x + std::abs(dx), y + std::abs(dy)}, {x - std::abs(dx), y + std::abs(dy)}};
    for (int i = 0; i < 4; ++i) {
      auto a = c[i], b = c[(i + 1) % 4];
      out.push_back({a.first, a.second, bottom, b.first, b.second, bottom, 2});
      out.push_back({a.first, a.second, top, b.first, b.second, top, 1});
      out.push_back({a.first, a.second, bottom, a.first, a.second, top, 0});
    }
    return;
  }
  out.push_back({x, y, z, x, y, z, 3});
  if (emitterPoints(kind) || std::abs(rise) > .02f)
    out.push_back({x, y, z, x + (emitterPoints(kind) ? dx : 0), y + (emitterPoints(kind) ? dy : 0), z + rise, 1});
}
