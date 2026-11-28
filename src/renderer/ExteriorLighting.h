#pragma once
#include "../core/Math.h"
#include <array>
namespace retro {
// Ambient-cube interpolation keeps diffuse and gloss continuous while a face
// rotates across the old nearest-probe boundaries. Squared positive cosines
// reproduce each axis exactly and fade a probe with zero slope at its horizon.
inline std::array<float,6> exteriorLightingWeights(const std::array<Point3,6>& axes,Point3 normal){
 std::array<float,6> weights{};float total=0;
 for(size_t i=0;i<6;++i){const auto n=axes[i];float w=std::max(0.f,normal.x*n.x+normal.y*n.y+normal.z*n.z);weights[i]=w*w;total+=weights[i];}
 if(!(total>1.e-20f)||!std::isfinite(total)){weights.fill(1.f/6.f);return weights;}
 for(auto& w:weights)w/=total;return weights;
}
}
