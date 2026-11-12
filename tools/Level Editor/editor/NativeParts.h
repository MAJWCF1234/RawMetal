#pragma once
// Picture tiles for the Campaign Builder: the parts browser (models and
// shapes) and the paint palette (materials and room finishes). Every picture
// is rendered once by the engine (--editor-shots) and cached under
// .build/thumbs, which is build output and never committed.
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace parts {
namespace fs = std::filesystem;

// ------------------------------------------------------------- thumbnails
struct ThumbCache {
  fs::path folder;
  std::map<std::string, HBITMAP> loaded;
  std::set<std::string> missing;
  ~ThumbCache() {
    for (auto &[key, bitmap] : loaded)
      DeleteObject(bitmap);
  }
  bool exists(const std::string &key) const {
    std::error_code ec;
    return fs::exists(folder / (key + ".ppm"), ec);
  }
  HBITMAP get(const std::string &key) {
    if (auto it = loaded.find(key); it != loaded.end())
      return it->second;
    if (missing.count(key))
      return nullptr;
    std::ifstream in(folder / (key + ".ppm"), std::ios::binary);
    std::string magic;
    int w = 0, h = 0, depth = 0;
    if (!(in >> magic >> w >> h >> depth) || magic != "P6" || w <= 0 || h <= 0 || w > 512 || h > 512) {
      missing.insert(key);
      return nullptr;
    }
    in.get();
    std::vector<unsigned char> rgb(size_t(w) * h * 3);
    if (!in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()))) {
      missing.insert(key);
      return nullptr;
    }
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void *bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bitmap)
      return nullptr;
    auto *px = static_cast<unsigned char *>(bits);
    for (size_t i = 0; i < size_t(w) * h; ++i)
      px[i * 4] = rgb[i * 3 + 2], px[i * 4 + 1] = rgb[i * 3 + 1], px[i * 4 + 2] = rgb[i * 3], px[i * 4 + 3] = 255;
    loaded[key] = bitmap;
    return bitmap;
  }
  // New pictures may have arrived: look again for the ones that were missing.
  void rescan() { missing.clear(); }
};

// ------------------------------------------------------------- catalogue
struct Item {
  std::string key;   // thumbnail name
  std::wstring name; // shown under the tile
  int value = 0;     // model slot, shape kind or material id
  int group = 0;     // tab or palette section
};

inline bool has(const std::string &name, std::initializer_list<const char *> words) {
  for (auto w : words)
    if (name.find(w) != std::string::npos)
      return true;
  return false;
}
inline std::wstring nice(std::string name) {
  // res_crystal_l..res_rock_low (slots 374-380): resonance crystals and bedrock outcrops.
  if (name.rfind("res_crystal_", 0) == 0 || name.rfind("res_rock_", 0) == 0) {
    bool crystal = name.rfind("res_crystal_", 0) == 0;
    std::string size = name.substr(crystal ? 12 : 9);
    size = size == "l" ? "large" : size == "m" ? "medium" : size == "s" ? "small" : size;
    name = (crystal ? "resonance_crystal_" : "bedrock_") + size;
  }
  for (auto prefix : {"lab_", "med_", "pwr_", "ind_", "cave_", "mine_", "scrap_", "psx_", "anl_", "office_",
                      "prop_", "item_", "art_", "clinic_", "urb_"})
    if (name.rfind(prefix, 0) == 0 && name.size() > strlen(prefix) + 2) {
      name = name.substr(strlen(prefix));
      break;
    }
  std::wstring out;
  bool upper = true;
  for (unsigned char c : name) {
    if (c == '_') {
      out += L' ';
      continue;
    }
    out += wchar_t(upper ? towupper(c) : c);
    upper = false;
  }
  return out;
}
// The browser's tabs. Favourites first, Shapes as a first-class page.
inline const wchar_t *const partTabs[] = {L"★ Saved", L"Furniture", L"Machines", L"Lab",  L"Lights",
                                          L"Signs",        L"Props",     L"Shapes",   L"Nature", L"Caves",
                                          L"Mine",         L"Scrap",     L"Big",    L"Stamps"};
enum PartTab { Saved, Furniture, Machines, Lab, Lights, Signs, Props, Shapes, Nature, Caves, Mine, Scrap, Big, Stamps, TabCount };
inline int tabOf(const std::string &n) {
  if (has(n, {"lamp", "light", "lantern", "candle", "brazier"}))
    return Lights;
  if (has(n, {"sign", "billboard", "notice", "metro_map", "status_display", "pa_horn", "painting", "clock"}))
    return Signs;
  // Resonance crystals and the bedrock they grow from belong underground.
  if (n.rfind("cave_", 0) == 0 || n.rfind("res_", 0) == 0)
    return Caves;
  if (n.rfind("mine_", 0) == 0)
    return Mine;
  if (n.rfind("scrap_", 0) == 0)
    return Scrap;
  if (n.rfind("lab_", 0) == 0 || n.rfind("med_", 0) == 0 || n.rfind("anl_", 0) == 0 || has(n, {"microscope", "test_machine"}))
    return Lab;
  if (has(n, {"tree", "oak", "pine", "aspen", "grass", "fern", "bush", "leafy", "rock", "log", "stump", "sapling",
              "mushroom", "wheat", "goldenrod", "aster", "bracken", "sumac", "dogbane", "clover", "creeper", "ivy"}))
    return Nature;
  if (has(n, {"car_", "van", "bus_", "forklift", "metro_car", "train", "cottage", "warehouse", "hall", "ind_long",
              "ind_works", "ind_tower", "ind_silo", "ind_hut", "ind_shed", "ind_annex", "ind_block", "gas_store",
              "gas_canopy", "headframe", "shelter", "reservoir", "cooling", "container", "shack", "foundry", "entrance"}))
    return Big;
  if (has(n, {"desk", "chair", "table", "bench", "bookcase", "couch", "armchair", "cabinet", "locker", "shelf", "bed",
              "mattress", "sink", "toilet", "stall", "mirror", "cooler", "coffee", "vending", "trash", "file_",
              "binder", "mug", "phone", "keyboard", "monitor", "pc_tower", "computer", "crt", "tv"}))
    return Furniture;
  if (has(n, {"generator", "compressor", "pump", "tank", "turbine", "transformer", "valve", "pipe", "switch",
              "breaker", "panel", "console", "mainframe", "terminal", "fan", "vent", "converter", "flue", "genset",
              "air_handler", "ammeter", "gauge", "machine", "flue", "casing", "riser", "duct", "cable", "dish",
              "aerial", "electrical", "wall_box", "control", "meter", "reader", "scanner", "fridge"}))
    return Machines;
  return Props;
}
inline const char *const primNames[] = {"Box", "Pillar", "Stairs", "Ramp", "Column", "Arch", "Vault roof", "Dome",
                                        "Door frame", "Window"};

// The paint palette: STRUCT materials grouped, then the room finishes.
inline const wchar_t *const paintGroups[] = {L"Concrete & stone", L"Metal", L"Tile", L"Painted", L"Wood",
                                             L"Special", L"Room finish"};
enum PaintGroup { Concrete, Metal, Tile, Painted, Wood, Special, Finish };
inline int paintGroupOf(const std::string &n) {
  if (has(n, {"concrete", "formwork", "board-form", "weathered", "stained", "asphalt", "rock", "stone", "brick", "default wall"}))
    return Concrete;
  if (has(n, {"steel", "rust", "grating", "tread", "copper", "corrugated", "ribbed", "roofing"}))
    return Metal;
  if (has(n, {"tile", "lab panel", "carpet", "drop ceiling"}))
    return Tile;
  if (has(n, {"paint", "dado", "office wall", "wall finish", "hazard", "curtain"}))
    return Painted;
  if (has(n, {"timber", "plank"}))
    return Wood;
  return Special;
}
inline bool paintable(const std::string &n) { return !has(n, {"hidden", "invisible"}); }
// Room finishes (SURFACE sets) and the nearest block material, both ways.
inline const char *const finishNames[] = {"Concrete",     "Pressure works", "Brick",           "Clinic tile",
                                          "Lab panel",    "Rock",           "Corrugated",      "Office plaster",
                                          "Mine timber",  "Ancient stone",  "Clinic teal",     "Clinic green",
                                          "Clinic maroon"};
inline constexpr int finishMaterial[] = {5, 2, 10, 27, 28, 26, 29, 21, 32, 33, 36, 37, 38};
inline int finishForMaterial(int m) {
  for (int i = 0; i < 13; ++i)
    if (finishMaterial[i] == m)
      return i;
  switch (m) {
  case 31: return 2;
  case 50: case 24: return 3;
  case 45: case 48: return 6;
  case 3: case 22: return 7;
  case 12: case 51: return 8;
  case 2: case 4: case 7: case 8: case 11: case 19: case 20: case 46: case 47: return 1;
  default: return 0;
  }
}

// ------------------------------------------------------------- picture grid
struct Grid {
  HWND hwnd = nullptr;
  std::vector<Item> items;
  std::set<std::string> *saved = nullptr;
  ThumbCache *cache = nullptr;
  HFONT font = nullptr;
  int selected = -1, hot = -1, scroll = 0, cell = 100, content = 0;
  bool showGroups = false; // section headings between groups (the paint palette)
  const wchar_t *const *groupNames = nullptr;
  std::function<void(int, bool)> onPick;
  std::function<void(int)> onHover;
  std::function<COLORREF(const Item &)> placeholder;
  struct Spot {
    RECT box;
    int item;
  };
  std::vector<Spot> spots;
  std::vector<std::pair<RECT, std::wstring>> headings;
  void layoutSpots() {
    RECT r{};
    GetClientRect(hwnd, &r);
    int cols = std::max(1, int(r.right) / cell);
    int w = std::max(40, int(r.right) / cols);
    spots.clear(), headings.clear();
    int x = 0, y = 4, last = -999;
    for (size_t i = 0; i < items.size(); ++i) {
      if (showGroups && items[i].group != last) {
        if (x)
          x = 0, y += w + 16;
        last = items[i].group;
        if (groupNames)
          headings.push_back({RECT{6, y, r.right - 4, y + 20}, groupNames[last]});
        y += 22;
      }
      spots.push_back({RECT{x * w + 3, y, (x + 1) * w - 3, y + w + 10}, int(i)});
      if (++x == cols)
        x = 0, y += w + 16;
    }
    content = y + (x ? w + 16 : 0) + 4;
    scroll = std::clamp(scroll, 0, std::max(0, content - int(r.bottom)));
    SCROLLINFO s{sizeof(s), SIF_RANGE | SIF_PAGE | SIF_POS, 0, content, UINT(std::max(1L, r.bottom)), scroll, 0};
    SetScrollInfo(hwnd, SB_VERT, &s, TRUE);
  }
  void set(std::vector<Item> next, int pick) {
    items = std::move(next);
    selected = pick, hot = -1, scroll = 0;
    layoutSpots();
    // Bring the picked tile into view.
    for (auto &s : spots)
      if (s.item == selected) {
        RECT r{};
        GetClientRect(hwnd, &r);
        if (s.box.bottom > r.bottom)
          scroll = s.box.top - 8;
      }
    layoutSpots();
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  int at(int x, int y) const {
    for (auto &s : spots)
      if (x >= s.box.left && x < s.box.right && y + scroll >= s.box.top && y + scroll < s.box.bottom)
        return s.item;
    return -1;
  }
  void paint(HDC target, const RECT &r, COLORREF paper, COLORREF field, COLORREF edge, COLORREF chosen,
             COLORREF glow, COLORREF ink, COLORREF soft) {
    HDC dc = CreateCompatibleDC(target);
    HBITMAP canvas = CreateCompatibleBitmap(target, std::max(1L, r.right), std::max(1L, r.bottom));
    auto oldCanvas = SelectObject(dc, canvas);
    auto bg = CreateSolidBrush(paper);
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    auto oldFont = SelectObject(dc, font);
    SetStretchBltMode(dc, HALFTONE);
    for (auto &[box, text] : headings) {
      RECT q = box;
      OffsetRect(&q, 0, -scroll);
      SetTextColor(dc, soft);
      DrawTextW(dc, text.c_str(), -1, &q, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    HDC pic = CreateCompatibleDC(dc);
    for (auto &s : spots) {
      RECT q = s.box;
      OffsetRect(&q, 0, -scroll);
      if (q.bottom < 0 || q.top > r.bottom)
        continue;
      const auto &item = items[size_t(s.item)];
      bool sel = s.item == selected, over = s.item == hot;
      int side = q.right - q.left;
      RECT card{q.left, q.top, q.right, q.top + side};
      auto fill = CreateSolidBrush(field);
      auto pen = CreatePen(PS_INSIDEFRAME, sel ? 3 : over ? 2 : 1, sel ? chosen : over ? glow : edge);
      auto ob = SelectObject(dc, fill), op = SelectObject(dc, pen);
      RoundRect(dc, card.left, card.top, card.right, card.bottom, 12, 12);
      SelectObject(dc, ob), SelectObject(dc, op);
      DeleteObject(fill), DeleteObject(pen);
      RECT img{card.left + 4, card.top + 4, card.right - 4, card.bottom - 4};
      if (HBITMAP b = cache ? cache->get(item.key) : nullptr) {
        auto old = SelectObject(pic, b);
        BITMAP info{};
        GetObject(b, sizeof(info), &info);
        StretchBlt(dc, img.left, img.top, img.right - img.left, img.bottom - img.top, pic, 0, 0, info.bmWidth,
                   info.bmHeight, SRCCOPY);
        SelectObject(pic, old);
      } else {
        // Until the engine has drawn it: a soft tile with the name's initial.
        auto tint = CreateSolidBrush(placeholder ? placeholder(item) : edge);
        FillRect(dc, &img, tint);
        DeleteObject(tint);
        SetTextColor(dc, ink);
        std::wstring initial = item.name.substr(0, 1);
        DrawTextW(dc, initial.c_str(), -1, &img, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      }
      if (saved && saved->count(item.key)) {
        SetTextColor(dc, glow);
        RECT star{card.right - 22, card.top + 2, card.right - 4, card.top + 20};
        DrawTextW(dc, L"★", -1, &star, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      }
      RECT name{q.left, card.bottom, q.right, q.bottom + 6};
      SetTextColor(dc, sel ? ink : soft);
      DrawTextW(dc, item.name.c_str(), -1, &name, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    DeleteDC(pic);
    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldCanvas);
    DeleteObject(canvas);
    DeleteDC(dc);
  }
};
} // namespace parts
