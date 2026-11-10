#pragma once
#include <vector>
#include <cstdint>
#include "Mesh.h"
#include "../world/World.h"
namespace retro {
// What a creature leaves behind: its own mesh, in the pose it died in, made into a body that falls,
// folds at its joints and comes apart at them. Nothing is authored per creature. The sections, the
// joints between them and the caps that close a cut are all found from the triangles, so every kind
// the engine can draw can be dropped, shot to pieces or blown apart.
class Carcass {
public:
 struct Ray {Point3 origin{},direction{};float range=0,damage=0,impulse=0;};
 // Blood for the renderer to throw: a hit, or the stump a severed section leaves.
 struct Event {Point3 point{},direction{};float power=1;bool stump=false;};
 // `posed` is the mesh in world space as it was last drawn alive; `push` is the velocity the kill gave it.
 void build(const std::vector<MeshTriangle>& posed,const World& world,Point3 push,float scale,unsigned seed);
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
 int pieces()const{return m_pieces;}
 int sections()const{return int(m_sections.size());}
 int joints(bool seamsOnly=false)const;
 static bool test();
private:
 struct Particle {Point3 x{},previous{},rest{},safe{},probe{};float floor=-100,roof=1e9f;int region=-1;bool grounded=false,loose=false;};
 struct Section {std::vector<int> members;std::vector<float> weights;Point3 centre{},restCentre{},axes[3]{{1,0,0},{0,1,0},{0,0,1}};float q[4]{1,0,0,0},damage=0;int count=0;};
 struct Joint {int a=0,b=0,particle=-1;bool seam=false,cut=false;Point3 rest{};};
 struct Bind {int particle[3]{-1,-1,-1};float weight[3]{};int home=0;};
 struct Edge {int a=0,b=0,owner=0,other=-1;};
 struct Loop {int owner=0;std::vector<int> edges;Point3 centre{},out{},across{},up{};float reach=1;};
 void fit();
 void step(const World& world);
 void regroup();
 void measure();
 void skin();
 std::vector<int> limb(int section,Point3 hit)const;
 void sever(const std::vector<int>& torn,Point3 direction,float speed,std::vector<Event>& out);
 Point3 place(int vertex,int viewer)const;
 Point3 carry(const Section& section,Point3 rest)const;
 std::vector<Particle> m_particles;std::vector<Section> m_sections;std::vector<Joint> m_joints;std::vector<Bind> m_binds;
 std::vector<Point3> m_rest,m_posed;std::vector<int> m_corner,m_owner,m_piece;std::vector<Edge> m_edges;std::vector<Loop> m_loops;
 std::vector<MeshTriangle> m_triangles,m_caps;
 Point3 m_centre{};float m_radius=1,m_age=0,m_quiet=0,m_woke=0,m_scale=1,m_size=1,m_motion=0;
 int m_root=0,m_pieces=1,m_body=0;unsigned m_seed=1;bool m_asleep=false,m_dirty=true;
};
}
