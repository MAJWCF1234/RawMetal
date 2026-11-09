#pragma once
#include "NativeDocument.h"
#include <cmath>
#include <optional>

// Plan handles describe native record coordinates only. They do not build a
// renderable scene; the engine remains the only owner of world geometry.
struct NativeHandle {
  float x = 0, y = 0, width = .5f, depth = .5f;
  std::vector<std::pair<size_t, size_t>> points;
};
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
