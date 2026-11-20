#pragma once
// The limits the game's loader (src/world/CustomCampaign.cpp) puts on a field, kept in ONE table.
// The Builder's inspector clamps a typed number to its row (tools/Level Editor/editor/NativeEditor.cpp, apply) and the Spawn,
// Emitter, Darkness and Trigger tools stop at a record's cap (authorKind); the game-side test
// (src/game/MapFormatTest.cpp, "the Builder's limits") loads every row with the loader, so a
// limit that drifts from the loader's own fails there. Plain C++: no Windows, no engine.
//
// A field is named by its place in the record as the Builder reads it (index >= 0, positional; for
// SPAWN the Builder's own view: map x y z turn team) or by a named key (index -1, key set; SPAWN
// has both: the place for the Builder, the key for the file).
// base: one whole valid record. The test sets the field to lo and to hi (both must load) and, when
// exact, a step beyond each (both must be refused): a positional field replaces the base's field
// at index (empty fields fill the gap), a named one is added as key=value. A row that is not exact
// is narrower than the loader on purpose (the loader's own bound is exclusive).
#include <string>
#include <string_view>
struct FieldRange {
  const char *kind;
  int index;
  const char *key;
  double lo, hi;
  bool whole, exact;
  const char *base;
};
inline constexpr FieldRange fieldRanges[] = {
    {"MAP", 27, nullptr, 0, 1, false, true, "MAP|0|R|0|0|4|12|0|0|4|0.6|industrial_night|0|0|0|0|0|-1|0|0|1.570796|0|0"},
    {"MAP", 28, nullptr, 20, 1500, false, true, "MAP|0|R|0|0|4|12|0|0|4|0.6|industrial_night|0|0|0|0|0|-1|0|0|1.570796|0|0"},
    {"LIGHT", 5, nullptr, 0, 2, true, true, "LIGHT|0|5|5|3"},
    {"LIGHT", 9, nullptr, 1000, 12000, false, true, "LIGHT|0|5|5|3"},
    {"LIGHT", 10, nullptr, -1, 1, false, true, "LIGHT|0|5|5|3"},
    {"LIGHT", 17, nullptr, 0, 1, false, true, "LIGHT|0|5|5|3"},
    {"DOOR", 15, nullptr, .01, 1, false, false, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
    {"DOOR", 16, nullptr, 0, 1, false, true, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
    {"DOOR", 18, nullptr, 0, 2, true, true, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
    {"DOOR", 23, nullptr, 0, 10000, false, true, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
    {"DOOR", 24, nullptr, 0, 3, true, true, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
    {"CREATURE", 2, nullptr, 0, 8, true, true, "CREATURE|0|0|4|20|0"},
    {"CREATURE", 6, nullptr, .5, 2, false, true, "CREATURE|0|0|4|20|0"},
    {"CREATURE", 7, nullptr, 0, 5000, false, true, "CREATURE|0|0|4|20|0"},
    {"CREATURE", 10, nullptr, -1, 4096, true, true, "CREATURE|0|0|4|20|0"},
    {"PICKUP", 2, nullptr, 0, 6, true, true, "PICKUP|0|0|5|5"},
    {"HAZARD", 2, nullptr, 0, 7, true, true, "HAZARD|0|0|2|2|4|4|0|3|2"},
    {"SIGN", 11, nullptr, 0, 6, true, true, "SIGN|0|5|5|1.5|1|0.5|0|T|S|3968216"},
    {"SIGN", 12, nullptr, 0, 10, true, true, "SIGN|0|5|5|1.5|1|0.5|0|T|S|3968216"},
    {"AMBIENT", 6, nullptr, .05, 4, false, false, "AMBIENT|0|metal_bang|5|5|1|1|10|1|20|20"},
    {"AMBIENT", 7, nullptr, .05, 60, false, false, "AMBIENT|0|metal_bang|5|5|1|1|10|1|20|20"},
    {"TERMINAL", 13, nullptr, 0, 3, true, true, "TERMINAL|0|5|5|1|0|T|a|b||0"},
    {"FIXTURE", 11, nullptr, 0, 1, false, true, "FIXTURE|0|24|5|5|0|1.4|0.7|0.75|0|1"},
    {"SURFACE", 2, nullptr, -1, 12, true, true, "SURFACE|0|0|0|0"},
    {"SURFACE", 3, nullptr, -1, 12, true, true, "SURFACE|0|0|0|0"},
    {"SURFACE", 4, nullptr, -1, 12, true, true, "SURFACE|0|0|0|0"},
    {"REVERB", 2, nullptr, 0, 1, false, true, "REVERB|0|0.3|1"},
    {"REVERB", 3, nullptr, .1, 6, false, true, "REVERB|0|0.3|1"},
    {"GROUND", 5, nullptr, 0, 8, true, true, "GROUND|0|0|4|4|0|0"},
    {"GROUND", 6, nullptr, 0, 8, false, true, "GROUND|0|0|4|4|0|0"},
    {"TERRAIN_FLAT", 6, nullptr, 0, 60, false, true, "TERRAIN_FLAT|0|0|4|4|1|2"},
    {"CAVE", 8, nullptr, .5, 8, false, true, "CAVE|carve|10|10|0|12|12|0|2"},
    {"CAVE", 9, nullptr, 0, 1, false, true, "CAVE|carve|10|10|0|12|12|0|2"},
    {"CAVE", 10, nullptr, 0, 1, false, true, "CAVE|carve|10|10|0|12|12|0|2"},
    {"SPAWN", 2, "x", 0, 24, false, true, "SPAWN|map=0|y=5"},
    {"SPAWN", 3, "y", 0, 24, false, true, "SPAWN|map=0|x=5"},
    {"SPAWN", 4, "z", -100, 400, false, true, "SPAWN|map=0|x=5|y=5"},
    {"SPAWN", 5, "yaw", -360, 360, false, true, "SPAWN|map=0|x=5|y=5"},
    {"SPAWN", 6, "team", 0, 4, true, true, "SPAWN|map=0|x=5|y=5"},
    {"JOINT", -1, "length", 0, 30, false, true, "JOINT|0|rope|0|-1|5|5|1|5|5|3"},
    {"JOINT", -1, "cone", 0, 179, false, true, "JOINT|0|ball|0|-1|5|5|1"},
    {"JOINT", -1, "twist", 0, 179, false, true, "JOINT|0|ball|0|-1|5|5|1"},
    {"JOINT", -1, "stiffness", 0, 60, false, true, "JOINT|0|hinge|0|-1|5|5|1"},
    {"JOINT", -1, "damping", 0, 10, false, true, "JOINT|0|hinge|0|-1|5|5|1"},
    {"JOINT", -1, "radius", 0, .1, false, true, "JOINT|0|rope|0|-1|5|5|1|5|5|3"},
    {"JOINT", -1, "friction", 0, 5000, false, true, "JOINT|0|hinge|0|-1|5|5|1"},
    {"JOINT", -1, "ratio", .1, 10, false, true, "JOINT|0|hinge|0|-1|5|5|1"},
    {"CLOTH", -1, "drop", .1, 8, false, true, "CLOTH|0|strap|5|5|2"},
    {"CLOTH", -1, "length", .1, 20, false, true, "CLOTH|0|strap|5|5|2"},
    {"CLOTH", -1, "width", .01, .5, false, true, "CLOTH|0|strap|5|5|2"},
    {"CLOTH", -1, "radius", .003, .1, false, true, "CLOTH|0|strap|5|5|2"},
    {"CLOTH", -1, "mass", .01, 50, false, true, "CLOTH|0|strap|5|5|2"},
    {"CLOTH", -1, "stiffness", 1, 120, false, true, "CLOTH|0|strap|5|5|2"},
    {"STRUCT", -1, "trimwidth", .01, 1, false, true, "STRUCT|0|2|2|6|6|0|0.1|0|0|1|trim=2"},
};
// How many of a record the loader takes (per map, or in the whole campaign), and one to repeat
// ({n} counts the copies, for names that must differ).
struct RecordCap {
  const char *kind;
  int limit;
  bool perMap;
  const char *sample;
};
inline constexpr RecordCap recordCaps[] = {
    {"SPAWN", 64, false, "SPAWN|map=0|x=5|y=5"},
    {"EMITTER", 64, true, "EMITTER|0|5|5|1|0|0|0.5|2|0.1|0.2|16"},
    {"DARK", 64, true, "DARK|0|1|1|3|3"},
    {"SCRIPT_EVENT", 128, true, "SCRIPT_EVENT|0|cap_{n}|2|2|6|6|-10|10||1|0"},
    {"TERRAIN_FLAT", 512, false, "TERRAIN_FLAT|0|0|4|4|1|2"},
    {"GROUND", 1024, false, "GROUND|0|0|4|4|0|1"},
};
// Text the loader limits. mode 3: text the loader reads as written, never decoded (a ZONE name: no | or %); mode 1: a script name (letters, digits and _ - . / only, a leading @ for
// @kills, @health...); mode 2: printable ASCII; mode 0: any text. maxLen in bytes as the loader counts
// (ZONE's on the written text, the rest decoded). base as in FieldRange; the field is replaced by
// maxLen letters (loads), one more (refused), and for mode 1 and 2 by a name the mode forbids (refused).
struct TextRule {
  const char *kind;
  int index;
  int maxLen;
  int mode;
  const char *base;
};
inline constexpr TextRule textRules[] = {
    {"SCRIPT_EVENT", 2, 96, 1, "SCRIPT_EVENT|0|e|2|2|6|6|-10|10||1|0"},
    {"SCRIPT_EVENT", 9, 96, 1, "SCRIPT_EVENT|0|e|2|2|6|6|-10|10||1|0"},
    {"LIGHT", 11, 96, 1, "LIGHT|0|5|5|3"},
    {"OBJECTIVE_TEXT", 2, 96, 1, "OBJECTIVE_TEXT|0|obj|Text"},
    {"OBJECTIVE_TEXT", 3, 80, 2, "OBJECTIVE_TEXT|0|obj|Text"},
    {"ZONE", 6, 64, 3, "ZONE|0|1|1|5|5|Name"},
    {"ZONE", 9, 64, 3, "ZONE|0|1|1|5|5|Name|-10|10|T"},
    {"DOOR", 19, 24, 0, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
    {"DOOR", 20, 40, 0, "DOOR|0|4|6|8|0|0|0|1|0|-1"},
};
inline const TextRule *textRuleFor(std::string_view kind, int index) {
  for (const auto &t : textRules)
    if (kind == t.kind && index == t.index) return &t;
  return nullptr;
}
// What the Builder writes for a typed text: spaces become _, forbidden characters are left out, and the
// length is cut to what the loader takes. Returns the text unchanged when it already fits.
inline std::string fitText(const TextRule &rule, std::string text) {
  std::string out;
  for (unsigned char c : text) {
    if (rule.mode == 1) {
      if (c == ' ') out += '_';
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/' || (c == '@' && out.empty())) out += char(c);
    } else if (rule.mode == 2) {
      if (c >= 32 && c <= 126) out += char(c);
    } else if (rule.mode == 3) {
      if (c != '|' && c != '%' && c != '\r' && c != '\n') out += char(c);
    } else
      out += char(c);
  }
  if (out.size() > size_t(rule.maxLen)) out.resize(size_t(rule.maxLen));
  return out;
}
inline const FieldRange *fieldRangeFor(std::string_view kind, int index, std::string_view key = {}) {
  for (const auto &r : fieldRanges) {
    if (kind != r.kind) continue;
    if (!key.empty() ? (r.key && key == r.key) : (r.index == index && index >= 0)) return &r;
  }
  return nullptr;
}
inline const RecordCap *recordCapFor(std::string_view kind) {
  for (const auto &c : recordCaps)
    if (kind == c.kind) return &c;
  return nullptr;
}
