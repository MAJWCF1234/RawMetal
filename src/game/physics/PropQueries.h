#pragma once
// Props through Vectra's queries (Queries.h): a prop as the bodies it is made of where it stands now (one, or a
// chained pad's three segments, PropBodies.h), and a ray, a shape cast or an overlap against it in its true
// shape and turn. Coordinates are the prop list's own (Clutter::pos, z).
//  - Gunfire (Game::traceShot) asks each prop with rayCastProp: a round past a turned crate's corner passes it,
//    one through the gap under a chair's seat passes the chair.
//  - A blow at the player is shielded by what he holds only if its way meets the held prop (Game::shieldBlow,
//    boxCastProp).
// Each query first tests a sphere round the prop's box (no shape is looked up for a prop the cast is nowhere near).
#include "Queries.h"
#include "PropBodies.h"
namespace retro::vectra {
// The bodies a prop is made of as it stands (shape, centre of mass and turn only; no mass): its count.
int propBodies(const Clutter& c,std::array<Body,3>& out);
// A sphere holding the whole prop (a chained one however it bends).
void propBounds(const Clutter& c,V3& centre,float& radius);
bool rayCastProp(const Clutter& c,V3 origin,V3 direction,float maxDistance,QueryHit& hit);
bool sphereCastProp(const Clutter& c,V3 origin,float radius,V3 direction,float maxDistance,QueryHit& hit);
bool boxCastProp(const Clutter& c,V3 origin,V3 half,Quat turn,V3 direction,float maxDistance,QueryHit& hit);
bool overlapSphereProp(const Clutter& c,V3 centre,float radius);
}
