#pragma once
#include <vector>
#include <string>
#include <array>
#include "../ThirdParty/ufbx/ufbx.h"
namespace retro {
struct Point3 {
 float x=0,y=0,z=0;
 Point3 operator+(Point3 b)const{return {x+b.x,y+b.y,z+b.z};}
 Point3 operator-(Point3 b)const{return {x-b.x,y-b.y,z-b.z};}
 Point3 operator*(float s)const{return {x*s,y*s,z*s};}
};
struct MeshVertex {Point3 p;float u=0,v=0,light=1;};
struct MeshTriangle {MeshVertex v[3];int part=0;};
class Mesh {
public:
 explicit Mesh(int resource,const char* nodeFilter="");
 ~Mesh();
 Mesh(const Mesh&)=delete;
 Mesh& operator=(const Mesh&)=delete;
 void pose(float phase,float recoil);
 bool poseAction(const char* action,float phase);
 void poseAttached(Point3 right,Point3 left,float elbowSwing,float pitch,float yaw,float phase,float recoil);
 Point3 bonePosition(const char* name)const;
 std::vector<MeshTriangle> triangles;
 std::vector<std::vector<MeshVertex>> openRings;
 Point3 minimum,maximum;
 size_t bones=0;
 std::string description;
 std::vector<std::string> materialNames;
private:
 struct CachedTriangle {uint32_t node;std::array<uint32_t,3> corners;MeshTriangle prototype;};
 struct CachedRing {uint32_t node;std::vector<uint32_t> corners;};
 std::vector<CachedTriangle> m_cachedTriangles;
 std::vector<CachedRing> m_cachedRings;
 bool m_topologyReady=false;
 bool m_materialParts=false;
 std::string m_nodeFilter;
 void extract(ufbx_scene* scene);
 void grip(Point3 right={-.025f,1.55f,.223f},Point3 left={-.025f,1.60f,.49f},float swing=0,float pitch=0,float yaw=0);
 ufbx_scene* m_scene=nullptr;
 ufbx_scene* m_bindScene=nullptr;
 std::vector<ufbx_transform_override> m_fingerGrip;
};
}
