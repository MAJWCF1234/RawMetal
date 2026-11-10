#pragma once
#include "SoftwareRenderer.h"
#include <functional>

namespace retro {
struct GoreStats {int bursts=0,quads=0;};
// Draws what hits throw off (HitBurst) through the scene's own quad. Flesh: a puff of mist where a round
// goes in and a wider one where it comes out, drops thrown back at the shooter and a spray on through,
// each drop landing as a spot on the floor that dries and goes; a cut stump pumps for a second. Everything
// else throws its own stuff: concrete and rock chips and grey dust, tile shards, a cloud of plaster, wood
// splinters, clods of dirt, glass, lint. Metal throws only sparks, which ElectricFx draws. Nothing is
// stored: every drop and chip is worked out from the burst's place, time and seed each frame, so a frame
// asked twice is the same frame. All of it is lit by the room.
struct GoreFx {
 using Texture=SoftwareRenderer::Texture;
 using Quad=std::function<void(Point3,Point3,Point3,Point3,const Texture&,float)>;
 // Whether a box (low and high corners) is wholly hidden from the eye; asked once a burst.
 using Hidden=std::function<bool(Point3,Point3)>;
 static GoreStats draw(const SoftwareRenderer& renderer,const std::vector<HitBurst>& bursts,int level,float now,Point3 eye,const Quad& quad,const Hidden& hidden={});
};
}
