#pragma once
#include <array>
#include <vector>
#include "Carcass.h"
namespace retro::humanoid {
// A man's mesh in its rest pose (standing, y up, arms down or out) cut into the parts a ragdoll turns
// at: pelvis, belly, chest, head (with the neck), each upper arm and forearm (with the hand), each
// thigh and shin (with the foot). Found from the shape alone (the baked banks carry no bones): the
// arms are what the band between hip and armpit leaves apart from the trunk, the joints sit where
// people bend (knee at 28% of the height, hip at 46%, elbow 45% down the arm).
enum Part {Pelvis,Belly,Chest,Head,UpperArmL,UpperArmR,ForearmL,ForearmR,ThighL,ThighR,ShinL,ShinR,Count};
const char* name(int part);
struct Labels {std::vector<int> part; // per triangle of the rest mesh
 Point3 forward{0,0,1};float height=1,hip=.46f,knee=.28f;bool ok=false;};
// `arms`: where the shoulders, elbows and wrists are in the rest pose ([side][0..2]), when a rig says so.
using Arms=std::array<std::array<Point3,3>,2>;
Labels label(const std::vector<MeshTriangle>& bind,const Arms* arms=nullptr);
// The rig a Carcass is built with: `faces` names the rest triangle each posed triangle is.
Carcass::Rig rig(const Labels& labels,const std::vector<MeshTriangle>& bind,const std::vector<int>& faces);
// Each part's joint to the part it hangs from: the part, how far it bends forward (+) and back,
// sideways and about its own length (radians), and whether bending it moves it forward (elbow,
// hip, spine) or back (knee).
Carcass::Limit limit(int part);
}
