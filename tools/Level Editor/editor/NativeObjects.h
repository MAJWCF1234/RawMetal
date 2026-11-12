#pragma once
#include "NativeDocument.h"
#include <array>
#include <cmath>
#include <optional>

// Plan handles describe native record coordinates only. They do not build a
// renderable scene; the engine remains the only owner of world geometry.
struct NativeHandle {
  float x = 0, y = 0, width = .5f, depth = .5f;
  std::vector<std::pair<size_t, size_t>> points;
};

// A DOOR's frame, the engine's own (src/world/World.h Door). Along X the leaf
// spans x left..right in a wall at y (field 4); axis 1 (field 21) spans y
// left..right in a wall at x = field 4. A swinging leaf (field 8) hangs from
// left, or from right with hinge 1 (field 22), and opens toward +across when
// swing (field 14, blank 1) is positive: +y along X, +x along Y. Entry and
// transfer doors (fields 6, 7) stay along X.
inline bool nativeFlag(const std::vector<std::string> &f, size_t i) {
  return i < f.size() && !f[i].empty() && f[i] != "0";
}
// Play features: open (16) how far it starts open, opening (17) whether it
// starts opening, strength (23, blank never) the battering a swinging leaf
// takes before a Brute, Chimera or Machine breaks it down, lock (24) the lock
// it shows: 0 none, 1 keycard reader, 2 key, 3 power release.
struct NativeDoor {
  float left = 0, right = 1, line = .5f, swing = 1, maxOpen = 1, open = 0, strength = 0;
  int lock = 0;
  bool alongY = false, hingeRight = false, swinging = false, seam = false, opening = false;
  bool breakable() const { return swinging && strength > 0 && !seam; }
  std::pair<float, float> world(float along, float across) const {
    return alongY ? std::pair{across, along} : std::pair{along, across};
  }
  float along(std::pair<float, float> p) const { return alongY ? p.second : p.first; }
  float across(std::pair<float, float> p) const { return alongY ? p.first : p.second; }
  std::pair<float, float> centre() const { return world((left + right) * .5f, line); }
  std::pair<float, float> hinge() const { return world(hingeRight ? right : left, line); }
  std::pair<float, float> latch() const { return world(hingeRight ? left : right, line); }
  // Hinge to latch at `open` (0 shut, 1 a quarter turn to the swing side).
  std::pair<float, float> leafAxis(float open) const {
    float a = open * 1.57079632679f * swing;
    return world((hingeRight ? -1.f : 1.f) * std::cos(a), std::sin(a));
  }
};
inline std::optional<NativeDoor> nativeDoor(const std::vector<std::string> &f) {
  if (f.size() < 5 || f[0] != "DOOR")
    return {};
  auto num = [&](size_t i, float fallback) {
    if (i >= f.size() || f[i].empty())
      return fallback;
    try {
      return std::stof(f[i]);
    } catch (...) {
      return fallback;
    }
  };
  NativeDoor d;
  d.left = num(2, 0), d.right = num(3, 1), d.line = num(4, .5f);
  d.swing = num(14, 1), d.maxOpen = num(15, 1);
  d.alongY = nativeFlag(f, 21), d.hingeRight = nativeFlag(f, 22);
  d.swinging = nativeFlag(f, 8), d.seam = nativeFlag(f, 6) || nativeFlag(f, 7);
  d.open = num(16, 0), d.opening = nativeFlag(f, 17), d.strength = num(23, 0), d.lock = int(num(24, 0));
  return d;
}
inline std::string nativeNumber(float value);
// Writes the frame back, touching only what changed: a field that already
// holds the value keeps its exact text, a blank stays blank for a default, and
// an untouched short record is not padded. Turning off axis or hinge clears it.
inline void storeNativeDoor(std::vector<std::string> &f, const NativeDoor &d) {
  std::vector<size_t> wrote;
  auto put = [&](size_t i, const std::string &v) {
    if (i >= f.size()) {
      if (v.empty())
        return;
      f.resize(i + 1);
    }
    f[i] = v;
    wrote.push_back(i);
  };
  auto putNumber = [&](size_t i, float v) {
    if (i < f.size() && !f[i].empty())
      try {
        if (std::abs(std::stof(f[i]) - v) < 1e-5f)
          return;
      } catch (...) {
      }
    put(i, nativeNumber(v));
  };
  auto flag = [&](size_t i, bool on) {
    if (on != nativeFlag(f, i))
      put(i, on ? "1" : i < 11 ? "0" : "");
  };
  putNumber(2, d.left), putNumber(3, d.right), putNumber(4, d.line);
  // A default is blank, as the engine's writer leaves it (swing 1, open 0,
  // strength 0 never, lock 0 none); a field that already says it keeps its text.
  auto optional = [&](size_t i, float v, float blank) {
    if (v != blank)
      return putNumber(i, v);
    if (i < f.size() && !f[i].empty())
      try {
        if (std::stof(f[i]) != blank)
          put(i, "");
      } catch (...) {
        put(i, "");
      }
  };
  optional(14, d.swing, 1), optional(16, d.open, 0), optional(23, d.strength, 0), optional(24, float(d.lock), 0);
  flag(8, d.swinging), flag(17, d.opening), flag(21, d.alongY), flag(22, d.hingeRight);
  if (!d.seam)
    flag(6, false), flag(7, false);
  // A changed record ends at its last given field, as the engine's writer
  // (CampaignExport.cpp) ends it: trailing blanks are defaults. An unchanged
  // one keeps its exact text.
  while (!wrote.empty() && f.size() > 11 && f.back().empty())
    f.pop_back();
}
// The door carried through a plan transform (a turn, a mirror, a move): its
// hinge, latch and swing side go through `map` and are read back as a frame,
// so the axis follows the wall. A sliding door keeps its hinge and swing.
template <class Map> inline NativeDoor transformedDoor(const NativeDoor &d, Map map) {
  auto C = map(d.centre().first, d.centre().second);
  auto H = map(d.hinge().first, d.hinge().second);
  auto L = map(d.latch().first, d.latch().second);
  auto side = d.world((d.left + d.right) * .5f, d.line + (d.swing < 0 ? -1.f : 1.f));
  auto S = map(side.first, side.second);
  NativeDoor out = d;
  out.alongY = std::abs(L.second - H.second) > std::abs(L.first - H.first);
  float a = out.along(H), b = out.along(L);
  out.left = std::min(a, b), out.right = std::max(a, b), out.line = out.across(C);
  if (d.swinging) {
    out.hingeRight = a > b;
    out.swing = (out.across(S) < out.across(C) ? -1.f : 1.f) * std::abs(d.swing);
  }
  return out;
}
// A quarter turn about the door's centre, the way Turn and Turn copy go.
inline NativeDoor turnedDoor(const NativeDoor &d) {
  auto [cx, cy] = d.centre();
  return transformedDoor(d, [&](float x, float y) { return std::pair{cx + (y - cy), cy - (x - cx)}; });
}
// The door's footprint: centre, then `extra` longer than the span and `depth` across, in x and y.
inline std::array<float, 4> nativeDoorBox(const NativeDoor &d, float extra, float depth) {
  auto [x, y] = d.centre();
  float span = std::abs(d.right - d.left) + extra;
  return d.alongY ? std::array<float, 4>{x, y, depth, span} : std::array<float, 4>{x, y, span, depth};
}
inline std::optional<NativeHandle>
nativeHandle(const std::vector<std::string> &names,
             const std::vector<std::string> &fields) {
  auto index = [&](const std::string &name) -> size_t {
    auto it = std::find(names.begin(), names.end(), name);
    return size_t(it - names.begin());
  };
  auto number = [&](const std::string &name, float fallback) {
    auto i = index(name);
    if (i >= fields.size() || fields[i].empty())
      return fallback;
    try {
      return std::stof(fields[i]);
    } catch (...) {
      return fallback;
    }
  };
  NativeHandle handle;
  for (auto pair : {std::pair{"x", "y"},
                    {"x1", "y1"},
                    {"x2", "y2"},
                    {"ax", "ay"},
                    {"bx", "by"}}) {
    auto x = index(pair.first), y = index(pair.second);
    if (x < fields.size() && y < fields.size())
      handle.points.emplace_back(x, y);
  }
  if(handle.points.empty()&&!fields.empty()&&fields[0]=="DOOR"){
    auto left=index("left"),right=index("right"),y=index("y");if(left<fields.size()&&right<fields.size()&&y<fields.size())handle.points={{left,y},{right,y}};
    // A door along Y: its wall line (the y field) is the x of both ends.
    if(nativeFlag(fields,21)&&!handle.points.empty())handle.points={{y,left},{y,right}};
  }
  if (handle.points.empty())
    return {};
  try {
    auto [xi, yi] = handle.points.front();
    float minX = std::stof(fields[xi]), maxX = minX,
          minY = std::stof(fields[yi]), maxY = minY;
    for (auto [x, y] : handle.points) {
      float px = std::stof(fields[x]), py = std::stof(fields[y]);
      if (!std::isfinite(px) || !std::isfinite(py))
        return {};
      minX = std::min(minX, px);
      maxX = std::max(maxX, px);
      minY = std::min(minY, py);
      maxY = std::max(maxY, py);
    }
    handle.x = (minX + maxX) * .5f;
    handle.y = (minY + maxY) * .5f;
    handle.width = std::max(.35f, maxX - minX);
    handle.depth = std::max(.35f, maxY - minY);
    if (handle.points.size() == 1) {
      handle.width = std::max(.35f, number("width", number("w", .5f)));
      handle.depth = std::max(.35f, number("depth", number("d", .5f)));
    }
    return handle;
  } catch (...) {
    return {};
  }
}
inline std::string nativeNumber(float value) {
  if (!std::isfinite(value))
    throw std::runtime_error("Object coordinate must be finite");
  std::ostringstream out;
  out.precision(8);
  out << value;
  return out.str();
}
inline bool translateNative(std::vector<std::string> &fields,
                            const std::vector<std::string> &names, float dx,
                            float dy) {
  auto handle = nativeHandle(names, fields);
  if (!handle)
    return false;
  // Only coordinate fields change. Materials, models, flags, identifiers and
  // unrecognized optional fields retain their exact native strings.
  std::vector<size_t> changed;
  for (auto [x, y] : handle->points) {
    if (dx != 0&&std::find(changed.begin(),changed.end(),x)==changed.end()){fields[x]=nativeNumber(std::stof(fields[x])+dx);changed.push_back(x);}
    if (dy != 0&&std::find(changed.begin(),changed.end(),y)==changed.end()){fields[y]=nativeNumber(std::stof(fields[y])+dy);changed.push_back(y);}
  }
  return true;
}

// A region stamp is still native records. Tiles keep their cell offsets.
// Object coordinates are stored relative to the region's minimum corner.
struct NativeStamp {
  int width = 0, height = 0;
  std::vector<std::tuple<int, int, char>> tiles;
  struct Object {
    std::vector<std::string> names, fields;
    int sourceFixture = -1, sourceClutter = -1;
  };
  std::vector<Object> objects;
};

// Same turn the tile stamp uses: 90 degrees clockwise on the plan, then the
// optional horizontal (1) and vertical (2) mirrors. Object points use the
// continuous form so a model stays in the centre of its rotated cell.
inline std::pair<int, int> stampCell(int x, int y, int width, int height,
                                    int rotation, int mirror) {
  rotation &= 3;
  for (int k = 0; k < rotation; ++k) {
    int next = y;
    y = -x;
    x = next;
  }
  if (rotation == 1)
    y += width - 1;
  if (rotation == 2) {
    x += width - 1;
    y += height - 1;
  }
  if (rotation == 3)
    x += height - 1;
  int spanX = rotation % 2 ? height : width;
  int spanY = rotation % 2 ? width : height;
  if (mirror & 1)
    x = spanX - 1 - x;
  if (mirror & 2)
    y = spanY - 1 - y;
  return {x, y};
}
inline std::pair<float, float> stampPoint(float x, float y, int width,
                                          int height, int rotation,
                                          int mirror) {
  rotation &= 3;
  for (int k = 0; k < rotation; ++k) {
    float next = y;
    y = -x;
    x = next;
  }
  if (rotation == 1)
    y += float(width);
  if (rotation == 2) {
    x += float(width);
    y += float(height);
  }
  if (rotation == 3)
    x += float(height);
  int spanX = rotation % 2 ? height : width;
  int spanY = rotation % 2 ? width : height;
  if (mirror & 1)
    x = float(spanX) - x;
  if (mirror & 2)
    y = float(spanY) - y;
  return {x, y};
}
inline void stampInto(std::vector<std::string> &fields,
                      const std::vector<std::string> &names, int width,
                      int height, int rotation, int mirror, float originX,
                      float originY) {
  // A door turns and mirrors as a frame: a turned door stands along the other axis.
  if (auto door = nativeDoor(fields)) {
    auto moved = transformedDoor(*door, [&](float x, float y) {
      auto [nx, ny] = stampPoint(x, y, width, height, rotation, mirror);
      return std::pair{nx + originX, ny + originY};
    });
    if (moved.alongY)
      moved.seam = false; // the engine keeps entry and transfer doors along X
    storeNativeDoor(fields, moved);
    return;
  }
  auto handle = nativeHandle(names, fields);
  if (!handle)
    throw std::runtime_error("Stamp object has no native coordinates");
  const auto originalFields=fields;
  for (auto [x, y] : handle->points) {
    auto [nx, ny] = stampPoint(std::stof(originalFields[x]), std::stof(originalFields[y]),
                               width, height, rotation, mirror);
    fields[x] = nativeNumber(nx + originX);
    fields[y] = nativeNumber(ny + originY);
  }
  auto yaw = std::find(names.begin(), names.end(), "yaw");
  if (yaw == names.end())
    return;
  size_t index = size_t(yaw - names.begin());
  if (index >= fields.size() || fields[index].empty())
    return;
  float angle =
      std::stof(fields[index]) + float(rotation & 3) * 1.57079632679f;
  if (mirror & 1)
    angle = -angle;
  if (mirror & 2)
    angle = 3.14159265358979323846f - angle;
  fields[index] = nativeNumber(std::remainder(angle, 6.283185307179586f));
}
