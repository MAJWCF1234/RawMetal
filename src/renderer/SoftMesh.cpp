#include "SoftMesh.h"
#include "../world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
namespace retro::softmesh {
namespace {
float band(int model){return std::min(std::max(.08f,model::soft[model][1]*1.5f),model::soft[model][0]*.9f);}
// Height weight of a mesh point: 1 at the padding's top, 0 a band below it or anywhere above it.
float weight(int model,const Mesh& mesh,const Point3& p){
 const auto& k=model::soft[model];const float y=p.y-mesh.minimum.y,b=band(model);
 if(y>k[0]+.01f||y<k[0]-b)return 0;return std::clamp((y-(k[0]-b))/b,0.f,1.f);
}
bool inRect(int model,const Mesh& mesh,const Point3& p,float margin){
 const auto& k=model::soft[model];const Point3 c=(mesh.minimum+mesh.maximum)*.5f;const float x=p.x-c.x,z=p.z-c.z;
 return x>=k[2]-margin&&x<=k[3]+margin&&z>=k[4]-margin&&z<=k[5]+margin;
}
MeshVertex mid(const MeshVertex& a,const MeshVertex& b){return {(a.p+b.p)*.5f,(a.u+b.u)*.5f,(a.v+b.v)*.5f,(a.light+b.light)*.5f};}
float edge(const Point3& a,const Point3& b){const Point3 d=a-b;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
void split(const MeshTriangle& t,int depth,std::vector<MeshTriangle>& out){
 const float longest=std::max({edge(t.v[0].p,t.v[1].p),edge(t.v[1].p,t.v[2].p),edge(t.v[2].p,t.v[0].p)});
 if(longest<=.09f||depth>=6){out.push_back(t);return;}
 const MeshVertex a=mid(t.v[0],t.v[1]),b=mid(t.v[1],t.v[2]),c=mid(t.v[2],t.v[0]);
 for(const auto& f:{MeshTriangle{{t.v[0],a,c},t.part},MeshTriangle{{a,t.v[1],b},t.part},MeshTriangle{{c,b,t.v[2]},t.part},MeshTriangle{{a,b,c},t.part}})split(f,depth+1,out);
}
}
const std::vector<MeshTriangle>& refined(int model,const Mesh& mesh){
 static std::mutex lock;static std::map<int,std::vector<MeshTriangle>> cache;
 std::lock_guard<std::mutex> hold(lock);auto it=cache.find(model);if(it!=cache.end())return it->second;
 std::vector<MeshTriangle> out;out.reserve(mesh.triangles.size()*4);
 for(const auto& t:mesh.triangles){bool padding=false;for(const auto& v:t.v)padding|=weight(model,mesh,v.p)>0&&inRect(model,mesh,v.p,.02f);
  if(padding)split(t,0,out);else out.push_back(t);}
 return cache.emplace(model,std::move(out)).first->second;
}
float bend(int model,const SoftPad& pad,const Mesh& mesh,const Point3& p){
 const float w=weight(model,mesh,p);if(w<=0)return 0;const Point3 c=(mesh.minimum+mesh.maximum)*.5f;
 const float depth=pad.depthMesh(p.x-c.x,p.z-c.z);if(depth==0)return 0;
 const float up=pad.sink/std::max(1e-4f,model::soft[model][1]);   // world metres per mesh metre, upward
 return depth*w/std::max(1e-4f,up);
}
float shade(int model,const SoftPad& pad,const Mesh& mesh,const Point3& p,float bent,Point3 light){
 const float w=weight(model,mesh,p);if(w<=0)return 1;const Point3 c=(mesh.minimum+mesh.maximum)*.5f;
 const Point3 bentNormal=pad.normalMesh(p.x-c.x,p.z-c.z);
 // The slope is drawn three times as steep for the light as it lies: a 6 cm hollow tilts its sides
 // some 15 degrees, which a lamp overhead barely tells from flat.
 Point3 n{bentNormal.x*w*3,bentNormal.y*w*3,1-w+bentNormal.z*w};const float l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);n=n*(1/l);
 const float flat=std::max(.15f,light.z),lit=std::max(.15f,n.x*light.x+n.y*light.y+n.z*light.z);
 const float hollow=1.f-.4f*std::clamp(bent/std::max(1e-4f,model::soft[model][1]),0.f,1.f);
 return std::clamp(lit/flat*hollow,.3f,1.6f);
}
}
