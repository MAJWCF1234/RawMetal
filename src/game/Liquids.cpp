// Liquid in bottles (Liquids.h): the slosh, the pour, the leak, the spill, the drops and puddles, and the save.
#include "Game.h"
#include "physics/PropBodies.h"
#include "../world/ModelIds.h"
#include <cmath>
#include <cstring>
namespace retro {
namespace liquid {
bool vessel(int m){return m==model::glass_bottle||m==model::med_specimen_jar||m==model::med_pill_bottle;}
// The inside: 0.85 of the radius from 4% over the base; a bottle's shoulder at 62% of its height and a neck of 0.35
// of its radius to the mouth (its top).
Inner innerOf(int m,std::array<float,3> d){Inner in;in.r=.85f*.5f*std::min(d[0],d[1]);in.bottom=-.46f*d[2];in.top=.5f*d[2];
 const bool necked=m==model::glass_bottle;in.shoulder=necked?-.5f*d[2]+.62f*d[2]:in.top;in.neck=necked?.35f*.5f*std::min(d[0],d[1]):in.r;return in;}
float capacity(int m,std::array<float,3> d){if(!vessel(m))return 0;const Inner in=innerOf(m,d);return kPi*(in.r*in.r*(in.shoulder-in.bottom)+in.neck*in.neck*(in.top-in.shoulder));}
Kind kindNamed(const char* n){for(int k=0;k<3;++k)if(std::strcmp(n,traits[k].name)==0)return Kind(k);return Kind::None;}
}
namespace {
using vectra::V3;using vectra::Quat;
V3 v3(Point3 p){return {p.x,p.y,p.z};}
Point3 p3(V3 v){return {v.x,v.y,v.z};}
constexpr float Gravity=14.f;   // the props' own (vectra::Body::gravity)
constexpr int MaxDrops=700,MaxPuddles=48;
// Points through the inner cylinder (unit radius and height, about its middle): the volume below a plane is the share
// of them under it. Deterministic, stratified.
const std::vector<V3>& unitCylinder(){static const std::vector<V3> pts=[]{std::vector<V3> out;for(int k=0;k<10;++k)for(int i=0;i<24;++i){const float r=std::sqrt((float(i%6)+.5f)/6.f),a=(float(i/6)+.37f*float(k%4)+.5f)*kPi*.5f+float(i%6)*.7f;
   out.push_back({r*std::cos(a),r*std::sin(a),(float(k)+.5f)/10.f-.5f});}return out;}();return pts;}
using liquid::Inner;
Inner innerOf(const Clutter& c){return liquid::innerOf(c.model,c.size());}
// The plane offset that leaves `level` of the inside under it, for a unit normal n (bottle frame): the sample points
// over the inside's height, each at the radius there and weighted by that height's cross-section.
float planeFor(const Inner& in,V3 n,float level){const auto& pts=unitCylinder();const float mid=(in.bottom+in.top)*.5f,h=in.top-in.bottom;
 auto at=[&](const V3& u,float& weight){const float z=mid+u.z*h,r=z>in.shoulder?in.neck:in.r;weight=r*r;return V3{u.x*r,u.y*r,z};};
 float lo=1e9f,hi=-1e9f,total=0;for(const auto& u:pts){float wt;const float s=vectra::dot(n,at(u,wt));lo=std::min(lo,s);hi=std::max(hi,s);total+=wt;}lo-=.001f;hi+=.001f;
 if(level<=0)return lo;if(level>=1)return hi;
 for(int it=0;it<16;++it){const float m=(lo+hi)*.5f;float under=0;for(const auto& u:pts){float wt;if(vectra::dot(n,at(u,wt))<m)under+=wt;}if(under/total<level)lo=m;else hi=m;}
 return (lo+hi)*.5f;}
}
bool Game::fillLiquid(Clutter& c,liquid::Kind kind,float level){if(!liquid::vessel(c.model)||kind==liquid::Kind::None){c.liquid={};return false;}
 c.liquid=liquid::State{};c.liquid.kind=kind;c.liquid.level=std::clamp(level,0.f,1.f);c.liquid.plane=planeFor(innerOf(c),{0,0,1},c.liquid.level);return true;}
void Game::seedLiquid(Clutter& c){if(!c.traits)return;std::string kind;float fill=.75f;
 for(const auto&[k,v]:c.traits->properties){if(k=="liquid")kind=v;else if(k=="fill"){try{fill=std::stof(v);}catch(...){}}}
 if(!kind.empty())fillLiquid(c,liquid::kindNamed(kind.c_str()),fill);}
void Game::holeLiquid(Clutter& c,Point3 at){if(!c.liquid.holds()||c.liquid.holed)return;const Quat q=vectra::propOrientation(c);const V3 l=q.unrotate(v3(at)-vectra::propCentre(c));
 // On the inner wall where the round went in, at its height.
 const Inner in=innerOf(c);const float r=std::sqrt(l.x*l.x+l.y*l.y);const float wall=l.z>in.shoulder?in.neck:in.r;const float k=r>1e-4f?wall/r:0.f;c.liquid.hole={l.x*k,l.y*k,std::clamp(l.z,in.bottom+.01f,in.top-.01f)};c.liquid.holed=true;
 c.sleeping=false;c.restTime=0;}
void Game::emitLiquid(Point3 at,Point3 velocity,liquid::Kind kind,float volume,int drops){
 if(drops<=0||volume<=0)return;const float each=volume/float(drops);
 for(int i=0;i<drops&&int(m_liquidDrops.size())<MaxDrops;++i){const float h=float((m_liquidDrops.size()*2654435761u+unsigned(i)*40503u)%1000)/1000.f,a=h*6.283f;
  const Point3 jitter{std::cos(a)*.18f*h,std::sin(a)*.18f*h,(h-.5f)*.2f};m_liquidDrops.push_back({at,velocity+jitter,kind,each});}}
void Game::spillLiquid(int level,Clutter& c,Point3 velocity,float share){
 if(!c.liquid.holds()||c.liquid.level<=0||level!=m_level)return;const float volume=c.liquid.level*share*liquid::capacity(c.model,c.size());const V3 m=vectra::propCentre(c);
 for(int i=0;i<24;++i){const float a=float(i)*2.399963f,r=.6f+.25f*float(i%4);emitLiquid(p3(m),velocity+Point3{std::cos(a)*r,std::sin(a)*r,.8f+.15f*float(i%5)},c.liquid.kind,volume/24.f,1);}
 c.liquid.level*=1-share;if(c.liquid.level<.002f)c.liquid.level=0;}
void Game::updateLiquids(float dt){
 if(dt<=0)return;
 for(int index=0;index<int(m_clutter.size());++index){auto& c=m_clutter[size_t(index)];auto& L=c.liquid;if(!L.holds()||c.broken)continue;
  const auto&tr=liquid::of(L.kind);const Quat q=vectra::propOrientation(c);const V3 x=vectra::propCentre(c);const Inner in=innerOf(c);
  // The bottle's acceleration from its middle's own path (held, carried, thrown or knocked alike).
  const V3 v=L.primed?(x-v3(L.lastX))*(1/dt):V3{};V3 a=L.primed?(v-v3(L.lastV))*(1/dt):V3{};const float al=vectra::len(a);if(al>60)a=a*(60/al);
  const Quat last{L.lastQ[0],L.lastQ[1],L.lastQ[2],L.lastQ[3]};
  V3 n=v3(L.normal);
  if(L.primed){
   // The bottle turned: the surface stays level in the world but for what the liquid's thickness carries along.
   const V3 kept=q.unrotate(last.rotate(n));n=vectra::norm(n*tr.follow+kept*(1-tr.follow),n);
   const V3 want=q.unrotate(vectra::norm(V3{-a.x,-a.y,Gravity-a.z},{0,0,1}));   // against the effective gravity (gravity less its acceleration)
   const float w=2*kPi*tr.hertz;V3 rate=v3(L.rate);const V3 err=vectra::cross(n,want);
   rate=rate+(err*(w*w)-rate*(2*tr.damping*w))*dt;const float rl=vectra::len(rate);if(rl>12)rate=rate*(12/rl);
   if(vectra::len(rate)*dt>1e-6f)n=vectra::norm(Quat::axisAngle(rate,vectra::len(rate)*dt).rotate(n),n);
   // Not further than 70 degrees off where it wants to be.
   if(vectra::dot(n,want)<.34f)n=vectra::norm(want*.7f+n*.3f,want);
   L.rate=p3(rate);}
  else n=q.unrotate(V3{0,0,1});
  L.normal=p3(n);L.lastX=p3(x);L.lastV=p3(v);L.lastQ={q.w,q.x,q.y,q.z};L.primed=true;
  L.plane=planeFor(in,n,L.level);
  // Out of the mouth and the hole: the head of liquid over each, along the surface's normal.
  L.pouring=0;if(L.level<=0)continue;const float cap=liquid::capacity(c.model,c.size());
  auto run=[&](V3 at,float radius,V3 outward){const float head=L.plane-vectra::dot(n,at);if(head<=.002f)return;
   const float speed=std::sqrt(2*Gravity*std::min(head,.3f))*tr.run;const float flow=kPi*radius*radius*speed*.6f;   // m^3/s
   float out=std::min(flow*dt,L.level*cap);L.level-=out/std::max(cap,1e-6f);L.pouring+=flow*1000.f;
   const V3 wAt=x+q.rotate(at),wOut=q.rotate(outward);const V3 wv=v+wOut*(speed*.8f);
   // A few drops a frame along the stream (fewer for a thick oil): what they carry is what left.
   const int drops=std::clamp(int(flow*1e5f*dt)+1,1,4);emitLiquid(p3(wAt),p3(wv),L.kind,out,drops);};
  const float mouth=std::max(.008f,in.neck*.8f);run({0,0,in.top},mouth,{0,0,1});
  if(L.holed){const V3 h=v3(L.hole);run(h,.006f,vectra::norm(V3{h.x,h.y,0},{1,0,0}));}
  if(L.level<.002f)L.level=0;
  if(L.pouring>0){c.sleeping=false;c.restTime=0;}}
 // The drops: flying, then into water (a splash) or onto a floor (a puddle).
 for(size_t i=0;i<m_liquidDrops.size();){auto& d=m_liquidDrops[i];d.v.z-=Gravity*dt;const Point3 next=d.p+d.v*dt;bool gone=false;
  const float floor=m_world.supportBelow(next.x,next.y,d.p.z+.02f,true),water=m_world.waterSurface(next.x,next.y);
  if(water>-999.f&&next.z<=water&&water>floor){if(d.volume>2e-5f)raiseWaterSplash(m_level,{next.x,next.y,water},d.v,.25f,WaterSplashKind::Step);gone=true;}
  else if(next.z<=floor+.002f){
   // Pooled where it fell: into the nearest puddle of its kind within reach, or a new one.
   Puddle* into=nullptr;float best=1e9f;for(auto& p:m_puddles){if(p.kind!=d.kind||std::fabs(p.at.z-floor)>.05f)continue;const float dd=length(Vec2{p.at.x-next.x,p.at.y-next.y});if(dd<p.radius+.12f&&dd<best){best=dd;into=&p;}}
   if(!into&&int(m_puddles.size())<MaxPuddles){m_puddles.push_back({{next.x,next.y,floor},d.kind,0,0});into=&m_puddles.back();}
   if(into){const float total=into->volume+d.volume;const float k=d.volume/std::max(total,1e-9f);into->at.x+=(next.x-into->at.x)*k*.5f;into->at.y+=(next.y-into->at.y)*k*.5f;into->volume=total;
    const float thick=d.kind==liquid::Kind::Oil?.004f:.0025f;into->radius=std::min(1.2f,std::sqrt(total/(kPi*thick))+.03f);}
   gone=true;}
  else if(next.z<-50)gone=true;
  if(gone){d=m_liquidDrops.back();m_liquidDrops.pop_back();continue;}
  d.p=next;++i;}
}
void Game::registerLiquidSave(){
 // "liquids", after "props" (it indexes their lists) and "physics": per chunk, each prop holding liquid by its place:
 // kind, level, the surface's normal (bottle frame) and offset, and a hole if it has one. A save without the section
 // (older) fills the map's bottles as authored.
 registerSaveSection({.name="liquids",.version=1,
  .write=[](Game& g,SaveOut& out){std::vector<std::pair<int,std::vector<int>>> chunks;
   for(int level=0;level<g.chunkCount();++level){const auto& list=g.m_chunks[size_t(level)].clutter;std::vector<int> held;for(int i=0;i<int(list.size());++i)if(list[size_t(i)].liquid.holds())held.push_back(i);if(!held.empty())chunks.push_back({level,held});}
   if(chunks.empty())return;int n=int(chunks.size());out(n);
   for(auto& [level,held]:chunks){int l=level,count=int(held.size());out(l,count);for(int i:held){auto L=g.m_chunks[size_t(level)].clutter[size_t(i)].liquid;int index=i,kind=int(L.kind);out(index,kind,L.level,L.normal.x,L.normal.y,L.normal.z,L.plane,L.holed);if(L.holed)out(L.hole.x,L.hole.y,L.hole.z);}}},
  .read=[](Game& g,SaveIn& in,int){for(int level=0;level<g.chunkCount();++level)for(auto& c:g.m_chunks[size_t(level)].clutter)c.liquid={};
   int n=0;in(n);if(n<0||n>g.chunkCount())throw std::runtime_error("invalid liquid chunk count");
   for(int k=0;k<n;++k){int level=0,count=0;in(level,count);if(level<0||level>=g.chunkCount())throw std::runtime_error("invalid liquid chunk");auto& list=g.m_chunks[size_t(level)].clutter;if(count<1||size_t(count)>list.size())throw std::runtime_error("invalid liquid count");
    for(int j=0;j<count;++j){int index=0,kind=0;liquid::State L;in(index,kind,L.level,L.normal.x,L.normal.y,L.normal.z,L.plane,L.holed);if(L.holed)in(L.hole.x,L.hole.y,L.hole.z);
     if(index<0||size_t(index)>=list.size()||kind<0||kind>2||!(L.level>=0&&L.level<=1))throw std::runtime_error("invalid liquid");
     L.kind=liquid::Kind(kind);const float nl=std::sqrt(L.normal.x*L.normal.x+L.normal.y*L.normal.y+L.normal.z*L.normal.z);L.normal=nl>.5f?L.normal*(1/nl):Point3{0,0,1};list[size_t(index)].liquid=L;}}},
  .absent=[](Game& g){for(int level=0;level<g.chunkCount();++level)for(auto& c:g.m_chunks[size_t(level)].clutter)if(!c.liquid.holds())g.seedLiquid(c);}});
}
namespace {const bool liquidSave=[]{Game::registerLiquidSave();return true;}();}
}
