#pragma once
#include "SoftwareRenderer.h"
#include <functional>

namespace retro {
struct ElectricStats {int sparks=0,bolts=0,pieces=0,quads=0;};
// Draws a chunk's electricity (Game::electricNow) through the scene's own quad: sparks as
// motion-stretched streaks cooling from white-hot to dull red with a warm glow, embers rolling to rest,
// a wisp of ozone at a break, and bolts -- a thin white core in a blue-white glow (violet off a cell)
// -- with hot spots where they leave and land. Sparks and bolts are self-lit (never darkened by the
// room) and bright enough for the Vulkan bloom, at least a pixel or two wide at any resolution, nearest
// first inside the frame's spark and bolt budgets; the smoke is lit by the room (light below 1.5, so the
// caller lights it as moving geometry). A shower wholly hidden is not simulated. The light it all
// throws is Game::weaponFlashes.
struct ElectricFx {
 using Texture=SoftwareRenderer::Texture;
 using Quad=std::function<void(Point3,Point3,Point3,Point3,const Texture&,float)>;
 // Whether a box (low and high corners) is wholly hidden from the eye; asked once a shower or bolt.
 using Hidden=std::function<bool(Point3,Point3)>;
 // `pixel`: metres a pixel spans one metre from the eye (1/focal length in pixels).
 static ElectricStats draw(const Game& game,int level,Point3 eye,float pixel,const Quad& quad,const Hidden& hidden={});
};
}
