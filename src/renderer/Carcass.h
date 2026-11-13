#pragma once
#include <vector>
#include <cstdint>
#include "Mesh.h"
#include "../world/World.h"
namespace retro {
// What a creature leaves behind: its own mesh, in the pose it died in, made into a body that falls,
// folds at its joints and comes apart at them. Each section of it is a rigid body (the mass and spread
// of a cloud of points on its skin), stepped by position-based dynamics; every vertex is carried by its
// section's frame, blended with the next section's only on the seam between them. Joints hold, turn
// within limits and against friction; sections do not pass through one another; the floor grips what
// lies on it (RM_CARCASS=1 frees the limits, 4 the keeping apart, for comparison). Nothing need be authored per creature: the sections, the joints between them
// and the caps that close a cut are found from the triangles, so every kind the engine can draw can be
// dropped, shot to pieces or blown apart. A body can also be told its parts (a man's, Humanoid.h), and
// then its joints turn as that body's do.
#define RAWMETAL_CARCASS_RIG 1
class Carcass {
public:
 struct Ray {Point3 origin{},direction{};float range=0,damage=0,impulse=0;};
 // Blood for the renderer to throw: a hit, or the stump a severed section leaves.
 struct Event {Point3 point{},direction{};float power=1;bool stump=false;};
 // A part's joint to the part it hangs from: how far it bends (radians; + is the way it folds: forward
 // for an elbow or a hip, back for a knee), sideways either way, and about its own length either way.
 struct Limit {int parent=-1;float bendLo=-.8f,bendHi=.8f,side=.8f,twist=.45f;bool flexForward=true;};
 // A body told its parts: the part each posed triangle is, the same triangles in the rest pose the
 // limits are measured from, the way that rest pose faces, and per part its joint (by part index).
 struct Rig {std::vector<int> part;std::vector<MeshTriangle> bind;Point3 forward{0,0,1};std::vector<Limit> limits;};
 // `posed` is the mesh in world space as it was last drawn alive; `push` is the velocity the kill gave it.
 // `reach`: how far each of its points keeps off walls and the sides of steps, so the skin between
 // them does not go into them (a man's limbs; 0 holds only the points themselves out).
 void build(const std::vector<MeshTriangle>& posed,const World& world,Point3 push,float scale,unsigned seed,float reach=0,const Rig* rig=nullptr);
 // Steps to `toAge` seconds after death. Driven by the creature's own death clock, so asking twice is free.
 void advance(const World& world,float toAge);
 // A round through the body: shoves the section it meets, and takes it off once it has had enough.
 bool strike(const Ray& ray,std::vector<Event>& out,int* face=nullptr);
 // An explosion: every joint lets go.
 void blast(Point3 origin,float power,std::vector<Event>& out);
 // Same triangles, same order as `posed`, where the body is now.
 const std::vector<MeshTriangle>& triangles();
 // The faces that close each cut, both sides.
 const std::vector<MeshTriangle>& caps();
 bool built()const{return !m_triangles.empty();}
 Point3 centre()const{return m_centre;}
 float radius()const{return m_radius;}
 float age()const{return m_age;}
 bool asleep()const{return m_asleep;}
 float motion()const{return m_motion;}
 int pieces()const{return m_pieces;}
 int sections()const{return int(m_sections.size());}
 int joints(bool seamsOnly=false)const;
 // What the lab and the tests read: each joint as it is now against its limits (radians), the deepest
 // any section's skin points are inside another's (metres), and the section each triangle rides.
 struct JointState {int section=-1,parent=-1,part=-1;float bend=0,side=0,twist=0,bendLo=0,bendHi=0,sideMax=0,twistMax=0;bool cut=false;};
 std::vector<JointState> jointStates()const;
 float overlap()const;
 int sectionOf(int triangle)const{return triangle>=0&&size_t(triangle)<m_owner.size()?m_owner[size_t(triangle)]:-1;}
 int partOf(int section)const{return section>=0&&size_t(section)<m_sections.size()?m_sections[size_t(section)].part:-1;}
 static bool test();
private:
 struct Particle {Point3 x{},previous{},rest{},safe{},probe{};float floor=-100,roof=1e9f;int region=-1;std::uint32_t ignore=0;bool grounded=false,loose=false;};
 struct Section {std::vector<int> members;std::vector<float> weights;Point3 centre{},restCentre{},axes[3]{{1,0,0},{0,1,0},{0,0,1}};float q[4]{1,0,0,0},damage=0;int count=0;
  // Its joint to the section it hangs from, the rotation from the rest pose to the pose it died in,
  // its limits' axes in that rest pose, and the pose its joint died in.
  int parent=-1,joint=-1,part=-1;float bind[4]{1,0,0,0},rel0[4]{1,0,0,0},last[4]{1,0,0,0};Point3 twistAxis{0,0,1},bendAxis{1,0,0},sideAxis{0,1,0};Limit limit{};bool limited=false;
  // Its body for the others to keep out of: a capsule with an oval section, in rest coordinates.
  Point3 capA{},capB{},capU{1,0,0},capV{0,1,0};float ru=0,rv=0;
  // Where it was at the end of the last step: how much it moves is what says it has come to rest.
  Point3 was{};float wasQ[4]{1,0,0,0};
  // As a rigid body: its centre of mass (in the rest pose and now), velocity, spin, inverse inertia (rest
  // frame) and where it was when the substep began.
  Point3 com{},comRest{},vel{},spin{},prevCom{},prevAxes[3]{};float prevQ[4]{1,0,0,0},invInertia[9]{},mass=0,reach=0,capReach=0;};
 struct Joint {int a=0,b=0,particle=-1;bool seam=false,cut=false;Point3 rest{};};
 // A vertex rides its own section and, on a seam, the section across it.
 struct Bind {int home=0,other=-1,joint=-1;float blend=0;};
 struct Edge {int a=0,b=0,owner=0,other=-1;};
 struct Loop {int owner=0;std::vector<int> edges;Point3 centre{},out{},across{},up{};float reach=1;};
 void fit();
 void step(const World& world);
 void bodies();
 void settle(const World& world);
 float depthIn(const Particle& p,int section)const;
 void regroup();
 void measure();
 void skin();
 void relative(const Section& s,float out[4])const;
 std::vector<int> limb(int section,Point3 hit)const;
 void sever(const std::vector<int>& torn,Point3 direction,float speed,std::vector<Event>& out);
 Point3 place(int vertex,int viewer)const;
 Point3 carry(const Section& section,Point3 rest)const;
 bool neighbours(int a,int b)const;
 std::vector<Particle> m_particles;std::vector<Section> m_sections;std::vector<Joint> m_joints;std::vector<Bind> m_binds;
 std::vector<Point3> m_rest,m_posed;std::vector<int> m_corner,m_owner,m_piece;std::vector<Edge> m_edges;std::vector<Loop> m_loops;
 std::vector<MeshTriangle> m_triangles,m_caps;
 Point3 m_centre{};float m_radius=1,m_age=0,m_quiet=0,m_woke=0,m_scale=1,m_size=1,m_motion=0,m_reach=0;
 int m_root=0,m_pieces=1,m_body=0;unsigned m_seed=1;bool m_asleep=false,m_dirty=true,m_rigged=false,m_bodies=false;
};
}
