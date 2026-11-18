#pragma once
// Liquid in bottles and jars (Liquids.cpp): a fill level, a kind (its colour, density and how freely it runs), and a
// surface that sloshes, pours, leaks from a hole and spills when the bottle breaks. Held in the prop (Clutter::liquid).
//  - The surface is a plane in the bottle's own frame: its normal is a damped spring toward the way the effective
//    gravity (gravity less the bottle's acceleration) points, the bottle's turn carrying the old surface with it by
//    the liquid's viscosity; where it cuts the bottle is found from the volume (the fill times the inner volume).
//  - The inner volume is a cylinder (0.85 of the bottle's radius, from just over its base to its mouth).
//  - Past the mouth, it pours: out at sqrt(2 g h) through the mouth by the head over it, times the kind's run.
//    A hole (a round through it) leaks the same way while the surface is over it.
//  - Drops fly and land: on standing water they splash into it (WaterFx), on a floor they pool (Game::m_puddles).
//  - Its mass is the bottle's (Clutter::mass), so a full bottle is heavier, sinks deeper and throws shorter.
// Authored with a CLUTTER record's custom properties: p.liquid=water|oil|chemical and p.fill=0..1.
#include "../core/Math.h"
#include <algorithm>
#include <array>
#include <cstdint>
namespace retro::liquid {
enum class Kind:int {None=-1,Water=0,Oil=1,Chemical=2,Count=3};
struct Traits {const char* name;float r,g,b,density,run,hertz,damping,follow;float glow;};
// run: how freely it pours (water 1); hertz, damping: the slosh spring; follow: how much of the bottle's turn the
// surface takes at once (a thick oil turns with its bottle, water stays level and sways).
inline constexpr Traits traits[]={{"water",.46f,.66f,.80f,1000,1.f,2.4f,.07f,.25f,0},{"oil",.36f,.22f,.05f,900,.32f,1.5f,.35f,.6f,0},{"chemical",.32f,.92f,.22f,1100,.85f,2.2f,.12f,.3f,.6f}};
inline const Traits& of(Kind k){return traits[std::clamp(int(k),0,2)];}
struct State {
 Kind kind=Kind::None;float level=0;                 // fill, 0..1 of the inner volume
 Point3 normal{0,0,1},rate{};float plane=0;          // the surface in the bottle frame: dot(normal, p) = plane; the normal's turning rate
 Point3 hole{};bool holed=false;                     // a round's hole (bottle frame, box metres from its middle)
 Point3 lastX{},lastV{};std::array<float,4> lastQ{1,0,0,0};bool primed=false;
 float pouring=0;                                    // litres a second running out now (mouth and hole)
 bool holds()const{return kind!=Kind::None;}
};
// Can a model hold liquid (bottles, jars)? Its inner volume (m^3) at a size.
bool vessel(int model);
// The inner shape in the bottle's frame (box metres about its middle): a body cylinder of radius r from its floor up
// to its shoulder, then a neck of radius neck up to the mouth (a jar has no neck: shoulder = top).
struct Inner {float r=0,neck=0,bottom=0,shoulder=0,top=0;};
Inner innerOf(int model,std::array<float,3> dims);
float capacity(int model,std::array<float,3> dims);
Kind kindNamed(const char* name);
}
