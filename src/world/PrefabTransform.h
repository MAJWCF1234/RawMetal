#pragma once
// Prefabs (map format 2): how a record standing in a PREFAB block moves to a PREFAB_PLACE. The game
// (CustomCampaign.cpp) reads every placement through this; the Campaign Builder takes a placed kit
// apart with it (NativeKitMethods.inc), so the loose pieces stand exactly where the game drew them.
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace retro::prefab {
// Where a record that may stand in a PREFAB keeps its place: points (x,y field pairs), boxes
// (x1,y1,x2,y2), heights raised with the placement, yaws turned with it (radians: `turn` +yaw for
// things facing (cos, sin), -yaw for SIGN and TERMINAL, which face (sin, cos)) and quarter-turn
// facings (PRIM, STRUCT).
struct Transform {
  std::vector<std::pair<int, int>> points;
  std::vector<std::array<int, 4>> boxes;
  std::vector<int> heights, turn, turnBack, quarters;
};
inline const Transform *transformFor(std::string_view tag) {
  static const std::map<std::string, Transform, std::less<>> table{
      {"FIXTURE", {{{3, 4}}, {}, {5}, {9}, {}, {}}},          {"CLUTTER", {{{3, 4}}, {}, {5}, {6}, {}, {}}},
      {"PROP", {{{3, 4}}, {}, {10}, {7}, {}, {}}},            {"LIGHT", {{{2, 3}}, {}, {4}, {6}, {}, {}}},
      {"CREATURE", {{{3, 4}}, {}, {5}, {}, {}, {}}},          {"PICKUP", {{{3, 4}}, {}, {5}, {}, {}, {}}},
      {"CRATE", {{{2, 3}}, {}, {4}, {}, {}, {}}},             {"CRATE_PLATE", {{{2, 3}}, {}, {}, {}, {}, {}}},
      {"NAV_HINT", {{{2, 3}}, {}, {4}, {}, {}, {}}},          {"TERMINAL", {{{2, 3}}, {}, {4}, {}, {12}, {}}},
      {"SIGN", {{{2, 3}}, {}, {4}, {}, {7}, {}}},             {"AMBIENT", {{{3, 4}}, {}, {5}, {}, {}, {}}},
      {"EMITTER", {{{2, 3}}, {}, {4}, {}, {}, {}}},           {"PIPE", {{{2, 3}, {4, 5}}, {}, {6, 8}, {}, {}, {}}},
      {"PRIM", {{{3, 4}}, {}, {5}, {}, {}, {9}}},             {"STRUCT", {{}, {{2, 3, 4, 5}}, {6, 7}, {}, {}, {12}}},
      {"HAZARD", {{}, {{3, 4, 5, 6}}, {7, 8}, {}, {}, {}}},   {"WATER", {{}, {{2, 3, 4, 5}}, {6, 7}, {}, {}, {}}},
      {"DARK", {{}, {{2, 3, 4, 5}}, {6, 7}, {}, {}, {}}},     {"ZONE", {{}, {{2, 3, 4, 5}}, {7, 8}, {}, {}, {}}},
      {"SHELTER", {{}, {{2, 3, 4, 5}}, {6, 7}, {}, {}, {}}},  {"COMPACTOR", {{}, {{2, 3, 4, 5}}, {6, 7}, {}, {}, {}}},
      {"WET", {{}, {}, {}, {}, {}, {}}}};
  auto it = table.find(tag);
  return it == table.end() ? nullptr : &it->second;
}
// Records that name others by their number: they cannot be copied about as a group.
inline bool references(std::string_view tag) {
  return tag.rfind("ACTOR", 0) == 0 || tag.rfind("SCRIPT_", 0) == 0 || tag.rfind("SEQUENCE_", 0) == 0 || tag == "LEAD" ||
         tag == "CHAIN" || tag == "BREAKABLE" || tag == "DIALOGUE" || tag == "DIALOGUE_REPLY" || tag == "TIMED_SEQUENCE";
}
struct Placement {
  float x = 0, y = 0, z = 0, yaw = 0;
};
inline std::string numberText(float v) {
  char buffer[48];
  auto r = std::to_chars(buffer, buffer + sizeof(buffer), v);
  return std::string(buffer, r.ptr);
}
// A field as the game reads numbers: all of it a finite number, at most 100 km.
inline float field(const std::vector<std::string> &g, size_t i, const char *label) {
  if (i >= g.size())
    throw std::runtime_error(std::string("missing ") + label);
  size_t used = 0;
  float value = 0;
  try {
    value = std::stof(g[i], &used);
  } catch (...) {
    throw std::runtime_error(std::string("invalid ") + label);
  }
  if (used != g[i].size() || !std::isfinite(value) || std::fabs(value) > 100000)
    throw std::runtime_error(std::string("invalid ") + label);
  return value;
}
inline int whole(const std::vector<std::string> &g, size_t i, const char *label) {
  if (i >= g.size())
    throw std::runtime_error(std::string("missing ") + label);
  size_t used = 0;
  int value = 0;
  try {
    value = std::stoi(g[i], &used);
  } catch (...) {
    throw std::runtime_error(std::string("invalid ") + label);
  }
  if (used != g[i].size())
    throw std::runtime_error(std::string("invalid ") + label);
  return value;
}
// One prefab record moved to its placement (its map field already set).
inline void placeRecord(std::vector<std::string> &g, const Placement &at) {
  const auto *t = transformFor(g[0]);
  if (!t)
    throw std::runtime_error("a PREFAB cannot hold " + g[0]);
  if (g[0] == "LIGHT" && g.size() > 26 && !g[26].empty())
    throw std::runtime_error("a LIGHT in a prefab cannot ride a CLUTTER record (its prop field)");
  const float c = std::cos(at.yaw), s = std::sin(at.yaw);
  const int quarters = int(std::lround(at.yaw / 1.57079632679f));
  const bool square = std::fabs(at.yaw - quarters * 1.57079632679f) < 1e-4f;
  auto value = [&](int i) { return field(g, size_t(i), "prefab field"); };
  auto set = [&](int i, float v) { g[size_t(i)] = numberText(v); };
  auto point = [&](int ix, int iy) {
    const float x = value(ix), y = value(iy);
    set(ix, at.x + x * c - y * s);
    set(iy, at.y + x * s + y * c);
  };
  if (g[0] == "WET") {
    // WET|map|x|y|radius|amount[|puddle] (round) or WET|map|x1|y1|x2|y2|amount|puddle (a box).
    if (g.size() == 8) {
      if (!square)
        throw std::runtime_error("a box record in a prefab turns only by quarter turns (yaw a multiple of 1.5708)");
      float ax = value(2), ay = value(3), bx = value(4), by = value(5);
      const float px = at.x + ax * c - ay * s, py = at.y + ax * s + ay * c, qx = at.x + bx * c - by * s, qy = at.y + bx * s + by * c;
      set(2, std::fmin(px, qx)), set(3, std::fmin(py, qy)), set(4, std::fmax(px, qx)), set(5, std::fmax(py, qy));
    } else
      point(2, 3);
    return;
  }
  for (auto [ix, iy] : t->points)
    if (size_t(ix > iy ? ix : iy) < g.size())
      point(ix, iy);
  for (auto b : t->boxes) {
    if (size_t(b[3]) >= g.size())
      continue;
    if (!square)
      throw std::runtime_error("a box record (" + g[0] + ") in a prefab turns only by quarter turns (yaw a multiple of 1.5708)");
    const float ax = value(b[0]), ay = value(b[1]), bx = value(b[2]), by = value(b[3]);
    const float px = at.x + ax * c - ay * s, py = at.y + ax * s + ay * c, qx = at.x + bx * c - by * s, qy = at.y + bx * s + by * c;
    set(b[0], std::fmin(px, qx)), set(b[1], std::fmin(py, qy)), set(b[2], std::fmax(px, qx)), set(b[3], std::fmax(py, qy));
  }
  for (int i : t->heights)
    if (size_t(i) < g.size() && !g[size_t(i)].empty()) {
      const float v = value(i);
      if (v != -999)
        set(i, v + at.z);
    }
  for (int i : t->turn)
    if (size_t(i) < g.size() && !g[size_t(i)].empty())
      set(i, std::remainder(value(i) + at.yaw, 6.28318530718f));
  for (int i : t->turnBack)
    if (size_t(i) < g.size() && !g[size_t(i)].empty())
      set(i, std::remainder(value(i) - at.yaw, 6.28318530718f));
  for (int i : t->quarters)
    if (size_t(i) < g.size() && !g[size_t(i)].empty()) {
      if (!square)
        throw std::runtime_error("a " + g[0] + " in a prefab turns only by quarter turns (yaw a multiple of 1.5708)");
      // Facing 0 +y, 1 +x, 2 -y, 3 -x: a quarter turn from +x toward +y takes +x to +y (1 to 0).
      g[size_t(i)] = std::to_string(((whole(g, size_t(i), "prefab facing") - quarters) % 4 + 4) % 4);
    }
}
} // namespace retro::prefab
