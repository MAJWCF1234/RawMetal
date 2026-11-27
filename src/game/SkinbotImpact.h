#pragma once

#include "../core/Math.h"
#include "../world/SkinbotWalker.h"

namespace retro::skinbot {
constexpr float ImpactRadius=.59f, ImpactBottom=.48f, ImpactHeight=1.82f;
constexpr float RecoveryRush=3.6f;

inline bool reactToProp(Walker& walker,float impact,Vec2 direction){
 auto& pose=walker.curse;
 if(impact<2.f||(pose.freeze>0&&pose.rush>=RecoveryRush))return false;
 const float strength=std::clamp(impact/18.f,.3f,1.f);
 const float side=dot(direction,{std::cos(walker.yaw),-std::sin(walker.yaw)});
 pose.sagSpeed-=1.6f+1.2f*strength;
 pose.swaySpeed+=side*(1.5f+1.3f*strength);
 pose.pitchSpeed-=3.f+3.f*strength;
 pose.quiver=1.f;
 pose.freeze=.24f+.18f*strength;
 pose.rush=RecoveryRush;
 pose.nextRush=6.f;
 return true;
}
}
