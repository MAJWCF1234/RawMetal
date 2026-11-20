#pragma once
// Curtains, straps and cables (CLOTH records, World.h ClothSpec) as they play: Vectra bodies and joints.
//
// Each CLOTH is a grid of small round beads (a curtain: columns along its rail and rows down it; a strap or a cable: one
// row along it) in its own vectra::Space, kept for as long as the map is played (as a corpse keeps its own: Carcass), so
// warm starting, the joints' impulses and the islands' sleep carry from step to step:
//  - neighbouring beads are joined by ropes (Joint::Rope: held at most their rest apart, slack nearer, so it folds and
//    drapes), soft at ClothSpec::stiffness, and every other bead along its length by a weak spring (it bends, not kinks);
//  - what fixes it (a curtain's rail, a strap's top, a cable's ends) is kinematic beads where the map put them;
//  - the beads meet the world (the floor under them, walls) as a corpse's points do, and the props near it, the player and
//    the creatures as kinematic bodies of their own shapes (the props' Vectra shapes, people as capsules), so a curtain
//    sways when walked through and a cable drapes over a crate;
//  - a round through it knocks the beads it passes, and out under the sky the breeze the plants bend in pushes it;
//  - it sleeps when it settles, and costs nothing asleep unless something moving comes near.
// Saved in the physics section (layout 5): each bead's place and velocity, so a curtain shoved aside loads hanging as it was.
#include "physics/Vectra.h"
#include "../world/World.h"
#include <deque>
#include <memory>
#include <vector>
namespace retro {
struct ClothRun {
 ClothSpec spec{};int cols=1,rows=1;     // beads: cols along (a curtain's rail, a strap's or cable's length), rows down
 float bead=.03f,spacing=.2f;            // each bead's radius, and how far apart they hang
 vectra::Space space;std::deque<vectra::Shape> shapes;std::vector<std::unique_ptr<vectra::Joint>> joints;
 int beads=0;                            // the first `beads` bodies are its beads; the rest are this step's obstacles
 std::vector<Point3> safe;std::vector<float> floor;std::vector<Point3> probe;   // per bead: last place clear of walls, the floor under it and where it was asked
 std::vector<char> open;                 // per bead: no wall within a bead and 5 cm of where it was asked
 Point3 lo{},hi{};                       // its beads' box (last step)
 float carry=0;                          // time not yet stepped
 bool asleep()const{for(int i=0;i<beads;++i)if(!space.bodies[size_t(i)].sleeping&&!space.bodies[size_t(i)].kinematic)return false;return true;}
 Point3 at(int col,int row)const{const auto& x=space.bodies[size_t(row*cols+col)].x;return {x.x,x.y,x.z};}
};
}
