#pragma once
// Props as rigid bodies (Vectra.h): each model's collision shape and material, the switch between the
// Vectra and the legacy prop physics, and the mapping between a Clutter and a Body.
//
// Shapes come from the catalog (the prop's own dims, prop_physics.json's mass and container volume) with
// a table of overrides by model: an oriented box by default; a cylinder for barrels, drums, bottles,
// jars, cans and cylinders (a tyre and a pipe lie along their own axis); a sphere for a pumpkin;
// compound boxes for chairs (legs or base, seat, back on the side the model's front table says), tables,
// a wheelchair, a lamp's L; an open container (model::container) is its floor and four walls up to its
// rim, so what lies in it rests on the inner floor and pours out when it tips.
#include "Vectra.h"
#include "../Game.h"
namespace retro::vectra {
// Legacy (simulateClutter's yaw-mostly box) instead of Vectra. Legacy is the default (the scorecard in
// docs/Systems/physics.md); RM_PROPS=vectra or the knob props.legacy 0 runs Vectra, RM_PROPS=legacy forces
// legacy. forceLegacyProps overrides both (the lab's A/B tubes): 1 legacy, 0 Vectra, -1 off.
bool legacyProps();
void forceLegacyProps(int mode);
// The rigid material of a prop material. Index PropMaterial::Count is rubber (lab bodies only).
Material propMaterial(PropMaterial m);
Material rubber();
// A prop's own material: its material's, with the model's overrides (casters and wheels roll freely).
Material propMaterialOf(const Clutter& c);
// The shape for a model at a size (cached; the reference stays valid).
const Shape& propShape(int model,std::array<float,3> dims);
inline const Shape& propShape(const Clutter& c){return propShape(c.model,c.size());}
// How stiff a padded prop's top is (Body::softHertz), 0 for a hard one.
float propSoftHertz(const Clutter& c);
// The prop's orientation (its quaternion if yaw/pitch/roll are still the ones the core wrote, else
// from them) and angular velocity (world, from spin/pitchSpeed/rollSpeed), and back.
Quat propOrientation(const Clutter& c);
V3 propAngular(const Clutter& c);
void setPropAngular(Clutter& c,V3 w);
// A Body from a prop (pose, velocities, mass, inertia, shape, material) and the prop from its body.
void toBody(const Clutter& c,Body& b);
void fromBody(const Body& b,Clutter& c);
// The half-widths of the prop's own shape's world box about the middle of its prop box (x, y, up): what it
// really takes up (a jar turned 45 degrees is as wide as its round, not as its box's diagonal).
std::array<float,3> propExtent(const Clutter& c);
// The middle of the prop's box, world (list coordinates).
inline V3 propCentre(const Clutter& c){return {c.pos.x,c.pos.y,c.z+c.extent()[2]};}

// ---- Saves (the "physics" save section, SaveSections.h's PhysicsSaveHooks/PropBody) ---------------
// What a prop keeps through a save beyond its old fields: its orientation as a unit quaternion in
// x, y, z, w order (4 floats), its angular velocity in world axes, radians per second (3 floats), and
// the sleep island it rests in (int32, -1 awake). capture returns false for a prop with no solver state
// (junk, shards, a broken tombstone). restore puts them on the loaded prop: its yaw/pitch/roll and
// spin/pitchSpeed/rollSpeed are rewritten from them, so a save without the section still loads (the
// quaternion then comes from yaw/pitch/roll, the angular velocity from the three speeds).
// At merge: physicsSaveHooks()={[](const Clutter& c,PropBody& b){return captureProp(c,b.orientation,b.angularVelocity,b.island);},
//                              [](Clutter& c,const PropBody& b){restoreProp(c,b.orientation,b.angularVelocity,b.island);}};
// ---- Chained props: mattresses and pads (Clutter::bend) ----
// A soft prop long for its thickness is three segments along its longer side, each a body, joined by springy
// hinges (Hinge) about its width: it sags held by one end, flops when dropped, drapes over an edge and
// wobbles when struck. Segment k (0..2) lies at (k-1) thirds of its length from the middle.
bool chained(const Clutter& c);
struct Chain {int along=0;float length=0;std::array<Quat,3> turn{};std::array<V3,3> frame{};};   // each segment's whole-prop frame: its turn, the prop's middle as it would be
// The three segments' frames from the prop's frame (that of segment boneRoot) and its bends.
Chain chainOf(const Clutter& c);
// The segment's own box (a third of the prop along its length).
const Shape& segmentShape(const Clutter& c);
// A point in the prop's frame (box metres from its middle) carried by the bends: where the segment it lies
// in has turned it, blended over the joints (skinning for the renderer). Unbent props pass through.
Point3 chainSkin(const Clutter& c,Point3 local);
bool captureProp(const Clutter& c,std::array<float,4>& orientationXyzw,std::array<float,3>& angularVelocity,std::int32_t& island);
void restoreProp(Clutter& c,const std::array<float,4>& orientationXyzw,const std::array<float,3>& angularVelocity,std::int32_t island);
}
