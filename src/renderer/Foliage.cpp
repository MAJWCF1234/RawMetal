#include "Foliage.h"
#include "../core/Tune.h"
#include "../game/Game.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
namespace retro {
namespace {
constexpr float Cell=2.f,Tick=1.f/60,LookReach=40.f;
std::uint64_t cellKey(int level,int cx,int cy){return std::uint64_t(std::uint32_t(level))<<40^std::uint64_t(std::uint32_t(cx+(1<<19))&0xfffffu)<<20^std::uint64_t(std::uint32_t(cy+(1<<19))&0xfffffu);}
int cellOf(float v){return int(std::floor(v/Cell));}
float hashUnit(unsigned v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return float(v&0xffffu)/65535.f;}
}
bool PlantBones::enabled(){static const bool on=[]{const char* v=std::getenv("RM_PLANTS");return !v||v[0]!='0';}();return on;}
void PlantBones::clear(){const bool held=still;*this=PlantBones{};still=held;}
int PlantBones::add(int level,int fixture,Point3 root,float height,float radius,bool hanging){
 if(auto it=m_slots.find(key(level,fixture));it!=m_slots.end()){auto&i=m_info[size_t(it->second)];i.root=root;i.height=std::max(.05f,height);i.radius=radius;i.hanging=hanging;
  float* g=m_gpu.data()+8+size_t(it->second)*8;g[4]=root.z;g[5]=i.height;g[7]=hanging?1.f:0.f;return it->second;}
 if(int(m_info.size())>=MaxPlants)return -1;
 const int slot=int(m_info.size());Info i;i.level=level;i.root=root;i.height=std::max(.05f,height);i.radius=std::max(.1f,radius);i.hanging=hanging;
 // Grass gives most and swings fastest; a bush less; a tree hardly, and slowly.
 i.flex=i.height<1.3f?1.f:i.height<3.f?.6f:.2f;i.omega=6.2831853f*std::clamp(1.6f/std::sqrt(i.height),.35f,2.8f);
 m_info.push_back(i);m_state.emplace_back();m_slots[key(level,fixture)]=slot;
 // A tall canopy spreads over the cells round its root.
 const int r=int(std::ceil(i.radius/Cell));for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx)m_grid[cellKey(level,cellOf(root.x)+dx,cellOf(root.y)+dy)].push_back(slot);
 m_gpu.resize(8+size_t(m_info.size())*8,0.f);float* g=m_gpu.data()+8+size_t(slot)*8;g[4]=root.z;g[5]=i.height;g[6]=i.flex;g[7]=hanging?1.f:0.f;
 return slot;}
template<class F> void PlantBones::around(int level,float x,float y,float reach,F&& f)const{
 // A canopy is listed in every cell it spreads over: each plant is visited once a look.
 if(m_seen.size()<m_info.size())m_seen.resize(m_info.size(),0);if(++m_stamp==0){std::fill(m_seen.begin(),m_seen.end(),0u);m_stamp=1;}
 const int x0=cellOf(x-reach),x1=cellOf(x+reach),y0=cellOf(y-reach),y1=cellOf(y+reach);
 for(int cy=y0;cy<=y1;++cy)for(int cx=x0;cx<=x1;++cx){auto it=m_grid.find(cellKey(level,cx,cy));if(it==m_grid.end())continue;
  for(int slot:it->second){if(m_seen[size_t(slot)]==m_stamp)continue;m_seen[size_t(slot)]=m_stamp;f(slot,m_info[size_t(slot)]);}}}
void PlantBones::wake(int slot){auto&s=m_state[size_t(slot)];s.quiet=0;if(!s.awake){s.awake=true;m_awake.push_back(slot);}}
Point3 PlantBones::tip(int slot)const{if(slot<0||size_t(slot)>=m_state.size())return {};const auto&s=m_state[size_t(slot)];return {s.x[0]+s.x2[0],s.x[1]+s.x2[1],0};}
void PlantBones::update(const Game& game,const std::vector<Pusher>& extra,float now){
 const auto started=std::chrono::steady_clock::now();m_stats=Stats{};m_stats.plants=int(m_info.size());
 if(m_gpu.size()<8)m_gpu.assign(8,0.f);
 if(!enabled()||still){std::fill(m_gpu.begin(),m_gpu.begin()+8,0.f);if(still)for(size_t i=0;i<m_info.size();++i)std::fill(m_gpu.begin()+8+std::ptrdiff_t(i)*8,m_gpu.begin()+12+std::ptrdiff_t(i)*8,0.f);return;}
 // The clock ran back (a load): nothing moving now was moving then.
 if(now+.001f<m_time){for(int slot:m_awake){m_state[size_t(slot)]=State{};std::fill(m_gpu.begin()+8+std::ptrdiff_t(slot)*8,m_gpu.begin()+12+std::ptrdiff_t(slot)*8,0.f);}m_awake.clear();m_rays=m_impacts=m_splashes=m_blasts=now;m_carry=0;}
 const float dt=m_time<0?0.f:std::clamp(now-m_time,0.f,.1f);m_time=now;
 if(m_rays<-1e29f)m_rays=m_impacts=m_splashes=m_blasts=now-.05f;
 // The wind: out under the sky a breeze that turns and gusts, harder in the storm; none indoors.
 {const bool open=game.world().zoneSky()||game.world().hasTerrain();const float storm=std::clamp(game.stormIntensity(),0.f,1.f);
  const float gust=.5f+.5f*std::sin(now*.37f)*std::sin(now*.11f+1.f);
  const float speed=open?tune("plants.wind",1.f)*(1.f+.35f*std::sin(now*.21f)+.25f*std::sin(now*.57f+1.3f))*(1.f+2.5f*storm):0.f;
  const float a=.7f+.35f*std::sin(now*.031f);
  m_gpu[0]=std::cos(a)*speed*.05f;m_gpu[1]=std::sin(a)*speed*.05f;m_gpu[2]=std::fmod(now,3600.f);m_gpu[3]=open?gust*(1.f+storm):0.f;
  // [1].x: the draft a hanging creeper drifts in (0 while held still).
  m_gpu[4]=1.f;}
 const Vec2 active=game.chunkOffset(game.level());
 const Point3 eye{game.player().pos.x,game.player().pos.y,game.player().z};
 // Who pushes through plants this frame, each in its own level's frame.
 std::vector<Pusher> pushers=extra;
 {const Player&p=game.player();pushers.push_back({game.level(),{p.pos.x,p.pos.y,p.z},.3f,p.hullHeight(),{}});}
 for(const auto&e:game.enemies())if(e.alive)pushers.push_back({game.level(),{e.pos.x,e.pos.y,e.z},std::max(.2f,e.hull()),std::max(.3f,e.bodyTop()-e.z),{}});
 for(const auto&n:game.npcActors())if(n.pose.clip!=4)pushers.push_back({n.level,{n.pose.position.x,n.pose.position.y,n.pose.z},.28f,n.track().scale,{}});
 for(const auto&c:game.clutter()){if(c.broken)continue;const auto e=c.extent();if(std::max(e[0],e[1])<.08f)continue;pushers.push_back({game.level(),{c.pos.x,c.pos.y,c.z},std::max(e[0],e[1]),c.height(),{c.velocity.x,c.velocity.y,c.vz}});}
 // The bends wanted this frame are found afresh.
 for(int slot:m_awake){auto&s=m_state[size_t(slot)];s.target[0]=s.target[1]=0;s.pushed=false;}
 auto lookable=[&](const Info&i){const Vec2 shift=game.chunkOffset(i.level)-active;return std::hypot(i.root.x+shift.x-eye.x,i.root.y+shift.y-eye.y)<LookReach;};
 const float give=tune("plants.give",.6f),kick=tune("plants.kick",.8f);
 for(const auto&p:pushers){
  around(p.level,p.feet.x,p.feet.y,p.radius+1.2f,[&](int slot,const Info&i){
   ++m_stats.looked;if(i.height>2.6f)return;
   const float reach=p.radius+i.radius*.75f;const float dx=i.root.x-p.feet.x,dy=i.root.y-p.feet.y,d=std::hypot(dx,dy);
   if(d>=reach||p.feet.z>i.high()||p.feet.z+p.height<i.low()-.05f)return;
   if(!lookable(i))return;
   // Bent away from the middle of what pushes through, the more the deeper it is in; trodden flat underfoot.
   float ux=dx,uy=dy;if(d<.02f){const float a=hashUnit(unsigned(slot)*2654435761u)*6.2831853f;ux=std::cos(a);uy=std::sin(a);}else{ux/=d;uy/=d;}
   const float deep=std::clamp((reach-d)/std::max(.1f,reach),0.f,1.f),amount=std::sqrt(deep)*i.height*give*i.flex;
   wake(slot);auto&s=m_state[size_t(slot)];s.target[0]+=ux*amount+p.velocity.x*.03f;s.target[1]+=uy*amount+p.velocity.y*.03f;s.pushed=true;});}
 // A kick: a round through it, a hit beside it, a splash, a blast.
 auto strike=[&](int slot,const Info&i,float ax,float ay,float power){if(!lookable(i))return;wake(slot);auto&s=m_state[size_t(slot)];const float k=power*kick*i.flex;
  s.v[0]+=ax*k;s.v[1]+=ay*k;const float r=hashUnit(unsigned(slot)*977u+unsigned(now*60.f))*6.2831853f;s.v2[0]+=std::cos(r)*k*1.5f;s.v2[1]+=std::sin(r)*k*1.5f;++m_stats.struck;};
 {float newest=m_rays;for(const auto&ray:game.shotRays()){if(ray.time<=m_rays)continue;newest=std::max(newest,ray.time);
   std::vector<int> hit;const float range=std::min(ray.range,LookReach);
   for(float t=0;t<=range+1.f;t+=.2f){const Point3 at=ray.origin+ray.direction*t;
    around(ray.level,at.x,at.y,1.f,[&](int slot,const Info&i){if(std::find(hit.begin(),hit.end(),slot)!=hit.end())return;
     if(std::hypot(at.x-i.root.x,at.y-i.root.y)>std::max(.3f,i.radius*.8f)||at.z<i.low()-.1f||at.z>i.high()+.1f)return;hit.push_back(slot);
     // Across the round's way, to the side of the stem it passed.
     float sx=-ray.direction.y,sy=ray.direction.x;if((i.root.x-at.x)*sx+(i.root.y-at.y)*sy>0){sx=-sx;sy=-sy;}const float l=std::max(.001f,std::hypot(sx,sy));
     strike(slot,i,(sx/l)*.5f+ray.direction.x,(sy/l)*.5f+ray.direction.y,1.f);});}}
  m_rays=newest;}
 {float newest=m_impacts;for(const auto&b:game.bulletImpacts()){if(b.time<=m_impacts)continue;newest=std::max(newest,b.time);
   around(b.level,b.pos.x,b.pos.y,1.f,[&](int slot,const Info&i){const float dx=i.root.x-b.pos.x,dy=i.root.y-b.pos.y,d=std::hypot(dx,dy);if(d>i.radius+.5f||b.z>i.high()+.3f||b.z<i.low()-.3f)return;
    strike(slot,i,d>.01f?dx/d:0.f,d>.01f?dy/d:0.f,.6f);});}
  m_impacts=newest;}
 {float newest=m_splashes;for(const auto&w:game.waterSplashes()){if(w.time<=m_splashes)continue;newest=std::max(newest,w.time);
   around(w.level,w.point.x,w.point.y,1.6f,[&](int slot,const Info&i){const float dx=i.root.x-w.point.x,dy=i.root.y-w.point.y,d=std::hypot(dx,dy);if(d>i.radius+1.2f)return;
    strike(slot,i,d>.01f?dx/d:0.f,d>.01f?dy/d:0.f,.7f*w.power);});}
  m_splashes=newest;}
 {float newest=m_blasts;for(const auto&b:game.barrelExplosions()){if(b.time<=m_blasts)continue;newest=std::max(newest,b.time);
   around(b.level,b.pos.x,b.pos.y,7.f,[&](int slot,const Info&i){const float dx=i.root.x-b.pos.x,dy=i.root.y-b.pos.y,d=std::hypot(dx,dy);if(d>7.f)return;
    strike(slot,i,d>.01f?dx/d:0.f,d>.01f?dy/d:0.f,5.f/(1.f+d));});}
  m_blasts=newest;}
 // The springs, at a fixed tick: the lower bone toward what is wanted, the upper whipping after it.
 m_carry=std::min(m_carry+dt,Tick*6);const float zeta=tune("plants.damping",.25f);
 for(;m_carry>=Tick;m_carry-=Tick)for(int slot:m_awake){auto&s=m_state[size_t(slot)];const auto&i=m_info[size_t(slot)];const float w=i.omega,w2=w*2.2f;
  for(int k=0;k<2;++k){const float a=-w*w*(s.x[k]-s.target[k])-2*zeta*w*s.v[k];s.v[k]+=a*Tick;s.x[k]+=s.v[k]*Tick;
   const float a2=-w2*w2*s.x2[k]-2*.14f*w2*s.v2[k]-.45f*a;s.v2[k]+=a2*Tick;s.x2[k]+=s.v2[k]*Tick;}
  auto clampTo=[](float* x,float* v,float most){const float l=std::hypot(x[0],x[1]);if(l>most){x[0]*=most/l;x[1]*=most/l;v[0]*=.5f;v[1]*=.5f;}};
  clampTo(s.x,s.v,.6f*i.height);clampTo(s.x2,s.v2,.25f*i.height);}
 // Out to the GPU, and to sleep when still.
 for(size_t n=0;n<m_awake.size();){const int slot=m_awake[n];auto&s=m_state[size_t(slot)];float* g=m_gpu.data()+8+size_t(slot)*8;
  const float motion=std::fabs(s.x[0])+std::fabs(s.x[1])+std::fabs(s.x2[0])+std::fabs(s.x2[1]),speed=std::fabs(s.v[0])+std::fabs(s.v[1])+std::fabs(s.v2[0])+std::fabs(s.v2[1]);
  if(!s.pushed&&motion<.004f&&speed<.03f)s.quiet+=dt;else s.quiet=0;
  if(s.quiet>.25f){s=State{};g[0]=g[1]=g[2]=g[3]=0;m_awake[n]=m_awake.back();m_awake.pop_back();continue;}
  g[0]=s.x[0];g[1]=s.x[1];g[2]=s.x2[0];g[3]=s.x2[1];m_stats.pushed+=s.pushed;++n;}
 m_stats.awake=int(m_awake.size());
 m_stats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}
}
