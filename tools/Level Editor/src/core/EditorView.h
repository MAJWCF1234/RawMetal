#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace retro {
// Editor-only draw controls. Geometry, materials and collision remain native.
struct EditorFloorView {
  float bottom=0,top=0,opacity=1;
  std::array<std::uint32_t,24> mask{};
};
struct EditorGuide {
  float ax=0,ay=0,az=0,bx=0,by=0,bz=0;
  int kind=0; // 0 outline, 1 upper edge, 2 lower edge, 3 footprint corner
};
}
