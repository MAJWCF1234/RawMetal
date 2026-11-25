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
      {"WET", {{}, {}, {}, {}, {}, {}}},                     {"DOOR", {{}, {}, {}, {}, {}, {}}}};   // DOOR: its own rule (placeRecord)
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
  if (g[0] == "DOOR") {
    // DOOR|map|left|right|y|z|entry|transfer|swinging|...|swing(14)|...|axis(21)|hinge(22): a leaf spanning left..right
    // along X in a wall at y (along Y with axis 1: y is the wall's x). Turned by quarters, its two ends turn with the
    // prefab and say which way it now runs; the hinge stays the end it was, and a swinging leaf still opens to the
    // side it did (swing +1 is toward +y along X, toward +x along Y). A seam door (entry, transfer) joins two maps
    // and has no place in a prefab.
    if (!square)
      throw std::runtime_error("a DOOR in a prefab turns only by quarter turns (yaw a multiple of 1.5708)");
    if ((g.size() > 6 && g[6] == "1") || (g.size() > 7 && g[7] == "1"))
      throw std::runtime_error("a seam DOOR (entry or transfer) cannot be in a prefab");
    const bool alongY = g.size() > 21 && g[21] == "1", hingeRight = g.size() > 22 && g[22] == "1";
    const float l = value(2), r = value(3), wall = value(4);
    auto turn = [&](float along) {
      const float x = alongY ? wall : along, y = alongY ? along : wall;
      return std::pair<float, float>{at.x + x * c - y * s, at.y + x * s + y * c};
    };
    const auto a = turn(l), b = turn(r), hinge = hingeRight ? b : a;
    const bool nowY = std::fabs(a.first - b.first) < 1e-3f;
    const float lo = nowY ? std::fmin(a.second, b.second) : std::fmin(a.first, b.first);
    const float hi = nowY ? std::fmax(a.second, b.second) : std::fmax(a.first, b.first);
    float swing = g.size() > 14 && !g[14].empty() ? value(14) : 1.f;
    const float nx = alongY ? (swing < 0 ? -1.f : 1.f) : 0.f, ny = alongY ? 0.f : (swing < 0 ? -1.f : 1.f);
    const float tx = nx * c - ny * s, ty = nx * s + ny * c;   // the side it opens to, turned
    swing = std::fabs(swing) * ((nowY ? tx : ty) < 0 ? -1.f : 1.f);
    if (g.size() < 23)
      g.resize(23);
    set(2, lo), set(3, hi), set(4, nowY ? a.first : a.second);
    if (!g[5].empty())
      set(5, value(5) + at.z);
    if (g.size() > 8 && g[8] == "1")
      set(14, swing);
    g[21] = nowY ? "1" : "0";
    g[22] = (nowY ? hinge.second : hinge.first) > lo + 1e-3f ? "1" : "0";
    return;
  }
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
