#pragma once
#include "World.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace retro {
// PRIM|map|kind|x|y|z|w|d|h|facing|material[|solid]
// A primitive stands on z, centred on (x,y). w runs across it, d along its
// facing (0 +y, 1 +x, 2 -y, 3 -x), h up. A size of 0 takes the kind's default,
// scaled to the player: 0.17 m risers, 2.2 m doors, 1 m rails. Ramps, columns,
// arches and vaults are drawn as true shapes over collision-proxy boxes.
// The player the defaults are sized for (game/Game.h Player, Movement.cpp):
// 1.6 m standing, 0.2 m radius, a 0.215 m automatic step, a 1.04 m jump
// (a crouch-jump clears 1.1 m). Openings meant to be walked through clear
// her with 0.15 m to spare; screens leave gaps narrower than she is.
inline constexpr float PlayerHeight=1.6f,PlayerRadius=.2f,PlayerStep=.215f,PlayerHeadroom=PlayerHeight+.15f,GuardHeight=1.1f;
// An arch of width w and height h: piers, a round-topped opening of half
// width `half` springing at `spring` above its base, and a 0.35 m crown.
struct ArchOpening {float pier,half,spring;};
inline ArchOpening archOpening(float w,float h){
 const float pier=std::clamp(w*.18f,.3f,.9f),half=w*.5f-pier;return {pier,half,h-.35f-half};}
// A barrel vault's shell thickness; its outer curve is the half ellipse
// spanning w and rising h, the inner one this much smaller.
inline constexpr float VaultShell=.3f;
// A strut's square section: its width, or half its rise if that is less.
inline float strutSection(float w,float h){return std::min(w,h*.5f);}
// A roundel's round opening: radius and the height of its centre above the base.
struct RoundelOpening {float radius,centre;};
inline RoundelOpening roundelOpening(float w,float h){return {.32f*std::min(w,h),h*.5f};}
// The headroom over a player walking through an arch's middle, measured at
// her shoulder, a radius off the centre line (0 if she does not fit at all).
inline float archClearance(const ArchOpening& a){
 return a.half<=PlayerRadius?0.f:a.spring+std::sqrt(a.half*a.half-PlayerRadius*PlayerRadius);}
struct Primitive {std::string_view kind;float x=0,y=0,z=0,w=0,d=0,h=0;int facing=0,material=40;bool solid=true;};
// Appends the primitive's structures; throws std::runtime_error naming the
// problem for an unknown kind, a facing outside 0-3 or a non-positive size.
void expandPrimitive(const Primitive& primitive,std::vector<Structure>& out);
bool testPrimitives();
}
