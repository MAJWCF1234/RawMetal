#pragma once
// Soft padding: mattresses, the bedroll, ward beds, cushions (model::soft: the padding's top, how far
// it gives under a person, its rect in mesh x/z). A pad wakes where something loads it near the
// player: a springy grid of downward offsets over the rect, pressed by contacts (the player standing,
// walking, crouching, landing; props, creatures and the dead lying on it), coupled to its neighbours,
// recovering over time, and asleep (dropped) once flat and untouched. What rests on it sinks with it
// (Game::softSinkAt); the renderer bends the model's top by it (SoftMesh.h).
#include <cstdint>
#include <vector>
#include "../core/Math.h"
namespace retro {
struct SoftPad {
 int level=0,fixture=-1,clutter=-1,model=-1,nx=0,ny=0;
 // Frame: the model's mesh centre on the floor plan (level-local), the world directions of mesh x and
 // mesh z and the metres a mesh metre spans along each, the rect in mesh metres, the padding's top
 // (world z) and how far it gives under a person (metres).
 Vec2 origin{},axisX{1,0},axisZ{0,1};float scaleX=1,scaleZ=1,rect[4]{},top=0,sink=0;
 std::vector<float> depth,speed;std::vector<unsigned char> pinned;
 // Seconds flat and untouched (asleep past a quarter), seconds settled under an unchanged load (then it
 // holds its shape without stepping), and that load's fingerprint.
 float quiet=0,still=0;std::uint64_t signature=0;
 // Bumped every step it moves: the renderer's cached padding is rebuilt when it changes.
 std::uint32_t version=0;
 Vec2 toMesh(Vec2 p)const{const Vec2 d=p-origin;return {dot(d,axisX)/scaleX,dot(d,axisZ)/scaleZ};}
 bool covers(Vec2 p,float margin=0)const{const Vec2 m=toMesh(p);return m.x>=rect[0]-margin/scaleX&&m.x<=rect[1]+margin/scaleX&&m.y>=rect[2]-margin/scaleZ&&m.y<=rect[3]+margin/scaleZ;}
 // Depth (metres, down) at a mesh point: the grid read bilinearly, eased to nothing at the rect's edge
 // so the padding's sides stay joined to its top.
 float depthMesh(float mx,float mz)const;
 float depthAt(Vec2 p)const{const Vec2 m=toMesh(p);return depthMesh(m.x,m.y);}
 float peak()const;
 float rise()const;   // the highest the pushed-aside rim stands over the top
 // The pressed surface's world normal at a mesh point (straight up where it lies flat).
 Point3 normalMesh(float mx,float mz)const;
};
struct Clutter;
namespace soft {
bool isSoft(int model);
// A prop: its model's softness, unless its CLUTTER record says soft=0 (map format 2).
bool isSoft(const Clutter& c);
}
}
