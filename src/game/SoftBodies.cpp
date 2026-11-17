#include "Game.h"
#include "SoftBodies.h"
#include "../world/ModelIds.h"
#include "../core/Tune.h"
#include <cmath>
namespace retro {
namespace soft {
bool isSoft(int model){return model>=0&&model<int(std::size(model::soft))&&model::soft[model][1]>0;}
}
float SoftPad::depthMesh(float mx,float mz)const{
 if(nx<2||ny<2)return 0;const float w=rect[1]-rect[0],h=rect[3]-rect[2];if(w<=0||h<=0)return 0;
 const float u=(mx-rect[0])/w,v=(mz-rect[2])/h;if(u<0||u>1||v<0||v>1)return 0;
 float fx=u*float(nx-1),fy=v*float(ny-1);const int i=std::min(int(fx),nx-2),j=std::min(int(fy),ny-2);fx-=float(i);fy-=float(j);
 auto at=[&](int a,int b){return depth[size_t(b*nx+a)];};
 const float d=(at(i,j)*(1-fx)+at(i+1,j)*fx)*(1-fy)+(at(i,j+1)*(1-fx)+at(i+1,j+1)*fx)*fy;
 const float edge=std::min(std::min(u,1-u)*w*scaleX,std::min(v,1-v)*h*scaleZ);
 return d*std::clamp(edge/.05f,0.f,1.f);
}
float SoftPad::peak()const{float m=0;for(float d:depth)m=std::max(m,std::fabs(d));return m;}
namespace {
struct Press {Vec2 at;float radius,load;};
struct Kick {Vec2 at;float radius,speed;};
bool frameFixture(const World& w,const Fixture& f,SoftPad& p){
 const auto& own=model::propSize[f.model];if(own[0]<=0||own[1]<=0||own[2]<=0)return false;const auto& k=model::soft[f.model];
 const float c=std::cos(f.yaw),s=std::sin(f.yaw),up=f.height/own[2];
 p.origin=f.position;p.axisX={c,-s};p.axisZ={s,c};p.scaleX=f.width/own[0];p.scaleZ=f.depth/own[1];
 for(int i=0;i<4;++i)p.rect[i]=k[2+i];p.top=w.floorHeight(f.position.x,f.position.y)+f.base+k[0]*up;p.sink=k[1]*up;return true;
}
bool frameClutter(const Clutter& c,SoftPad& p){
 if(c.broken||std::fabs(c.pitch)+std::fabs(c.roll)>.1f)return false;const auto& own=model::propSize[c.model];if(own[0]<=0||own[1]<=0||own[2]<=0)return false;
 const auto& k=model::soft[c.model];const float co=std::cos(c.yaw),si=std::sin(c.yaw),up=c.dims[2]/own[2];
 p.origin=c.pos;p.axisX={co,si};p.axisZ={-si,co};p.scaleX=c.dims[0]/own[0];p.scaleZ=c.dims[1]/own[1];
 for(int i=0;i<4;++i)p.rect[i]=k[2+i];p.top=c.z+k[0]*up;p.sink=k[1]*up;return true;
}
Vec2 nodeAt(const SoftPad& p,int i,int j){
 const float mx=p.rect[0]+(p.rect[1]-p.rect[0])*float(i)/float(p.nx-1),mz=p.rect[2]+(p.rect[3]-p.rect[2])*float(j)/float(p.ny-1);
 return p.origin+p.axisX*(mx*p.scaleX)+p.axisZ*(mz*p.scaleZ);
}
void allocate(SoftPad& p,const World& w){
 const float W=(p.rect[1]-p.rect[0])*p.scaleX,H=(p.rect[3]-p.rect[2])*p.scaleZ;
 p.nx=std::clamp(int(std::lround(W/.08f))+1,5,33);p.ny=std::clamp(int(std::lround(H/.08f))+1,5,33);
 const size_t n=size_t(p.nx*p.ny);p.depth.assign(n,0);p.speed.assign(n,0);p.pinned.assign(n,0);
 // A strap, rail or bar laid across the padding (MapAudit rule K's straps) holds it down where it lies:
 // the mattress gives round it, never out from under it.
 for(const auto& s:w.structures()){if(s.top-s.bottom>1.f||s.bottom>p.top+.03f||s.top<p.top-.02f)continue;   // and a blanket folded on it
  for(int j=0;j<p.ny;++j)for(int i=0;i<p.nx;++i){const Vec2 q=nodeAt(p,i,j);if(q.x>=s.x1-.03f&&q.x<=s.x2+.03f&&q.y>=s.y1-.03f&&q.y<=s.y2+.03f)p.pinned[size_t(j*p.nx+i)]=1;}}
}
float falloff(float q){if(q>=1)return 0;const float a=1-q*q;return a*a;}
}
float Game::softGive(const Clutter& c){
 if(!soft::isSoft(c.model)||c.broken||std::fabs(c.pitch)+std::fabs(c.roll)>.1f)return 0;const auto& own=model::propSize[c.model];if(own[2]<=0)return 0;
 const auto& k=model::soft[c.model];const float up=c.dims[2]/own[2];return c.height()-k[0]*up+k[1]*up+.01f;
}
const SoftPad* Game::softPad(int level,int fixture,int clutter)const{
 for(const auto& p:m_softPads)if(p.level==level&&p.fixture==fixture&&p.clutter==clutter)return &p;return nullptr;
}
float Game::softSinkAt(int level,Vec2 p,float surface)const{
 for(const auto& pad:m_softPads)if(pad.level==level&&std::fabs(pad.top-surface)<.03f&&pad.covers(p))return std::max(0.f,pad.depthAt(p));
 return 0;
}
float Game::softGround(Vec2 p,float feet,float height,float radius)const{
 // A pad under a fixture's padding presses the floor it gives.
 {auto local=p;const auto& w=worldAt(local);height-=softSinkAt(w.level(),local,height);}
 // A soft prop on the floor is walked on (stepped up onto from its edge, as a hull's corner first finds it): its pressed top.
 for(int i=0;i<int(m_clutter.size());++i){const auto& c=m_clutter[size_t(i)];if(!soft::isSoft(c.model)||c.broken)continue;
  SoftPad frame;if(!frameClutter(c,frame)||frame.top>feet+.025f)continue;const auto* pad=softPad(m_level,-1,i);
  for(Vec2 o:{Vec2{0,0},Vec2{-radius,-radius},Vec2{radius,-radius},Vec2{-radius,radius},Vec2{radius,radius}}){const Vec2 q=p+o;if(!frame.covers(q))continue;
   height=std::max(height,frame.top-(pad?std::max(0.f,pad->depthAt(q)):0.f));}}
 return height;
}
void Game::updateSoftBodies(float dt){
 if(m_softLevel!=m_level){m_softPads.clear();m_softLevel=m_level;}
 // The player's landing: the speed of the fall that just ended.
 const bool grounded=m_player.grounded,landed=grounded&&!m_softGrounded;const float landSpeed=m_softAirSpeed;
 if(!grounded)m_softAirSpeed=std::max(0.f,-m_player.verticalVelocity);else if(!landed)m_softAirSpeed=0;m_softGrounded=grounded;
 const float range=retro::tune("soft.range",12.f),gain=retro::tune("soft.gain",2.4f),K=retro::tune("soft.k",520.f),couple=retro::tune("soft.couple",900.f);
 const float pressDamp=retro::tune("soft.damp",16.f),recoverDamp=retro::tune("soft.recover",44.f);
 const World& w=m_world;const Vec2 player=m_player.pos;
 // A prop's pad follows the prop; a pad whose prop is gone, broken, tipped or moved off goes.
 std::erase_if(m_softPads,[&](SoftPad& p){if(p.level!=m_level||lengthSq(p.origin-player)>(range+2)*(range+2))return true;if(p.clutter<0)return false;
  if(size_t(p.clutter)>=m_clutter.size()||m_clutter[size_t(p.clutter)].model!=p.model)return true;
  SoftPad frame;if(!frameClutter(m_clutter[size_t(p.clutter)],frame))return true;
  p.origin=frame.origin;p.axisX=frame.axisX;p.axisZ=frame.axisZ;p.top=frame.top;return false;});
 const float moving=length(m_velocity);const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)},side{-forward.y,forward.x};
 auto gather=[&](const SoftPad& f,int selfClutter,std::vector<Press>& press,std::vector<Kick>& kick,std::vector<int>& resting,bool& playerOn){
  const float low=f.top-f.sink*1.6f-.03f,high=f.top+.04f;
  // The player: standing, walking (a foot at a time), kneeling, lying, landing.
  if(grounded&&f.covers(player,.12f)&&m_player.z>=low&&m_player.z<=high){playerOn=true;
   if(m_player.prone)press.push_back({player,.42f,.75f});
   else if(moving>.4f){const float phase=m_elapsed*8.5f,s=std::sin(phase);press.push_back({player+side*(s>0?.1f:-.1f)+forward*(.08f*std::cos(phase)),.15f,1.f+.3f*std::fabs(s)});}
   else press.push_back({player,m_player.crouched?.17f:.2f,m_player.crouched?1.1f:1.f});
   if(landed&&landSpeed>.5f)kick.push_back({player,.32f,std::min(landSpeed,9.f)*.45f});}
  // Props lying on it, and props landing on it.
  for(int i=0;i<int(m_clutter.size());++i){if(i==selfClutter||i==m_heldClutter)continue;const auto& v=m_clutter[size_t(i)];if(v.model<0||v.broken||v.z<low||v.z>high+.05f||!f.covers(v.pos))continue;
   const float r=std::clamp(v.footprint()*.5f,.06f,.5f),load=std::clamp(v.mass()/70.f,.02f,1.5f);
   if(v.z<=high){press.push_back({v.pos,r,load});resting.push_back(i);}
   if(v.vz<-1.f)kick.push_back({v.pos,r,std::min(-v.vz,9.f)*std::min(1.f,v.mass()/25.f)*.4f});}
  // Creatures walking over it, and the dead lying on it.
  for(const auto& e:m_enemies){if(!f.covers(e.pos,.1f)||e.z>f.top+.12f||e.z<f.top-.45f)continue;
   if(e.alive){const float s=e.moving?std::fabs(std::sin(m_elapsed*7.f+e.pos.x)):0.f;press.push_back({e.pos,.22f*e.scale,.9f*e.scale*e.scale*e.scale*(1+.3f*s)});}
   else press.push_back({e.pos,.45f*e.scale,.8f*e.scale*e.scale*e.scale});}
  for(size_t a=0;a<m_friendlyActors.size();++a){const auto& npc=m_friendlyActors[a];if(npc.level!=m_level||!f.covers(npc.pose.position,.1f)||npc.pose.z>f.top+.12f||npc.pose.z<f.top-.45f)continue;
   press.push_back(npc.pose.clip==4?Press{npc.pose.position,.45f,.8f}:Press{npc.pose.position,.2f,1.f});}
 };
 auto step=[&](SoftPad& p,const std::vector<Press>& press,const std::vector<Kick>& kick){
  const size_t n=p.depth.size();std::vector<float> force(n,0.f),accel(n,0.f);
  for(int j=0;j<p.ny;++j)for(int i=0;i<p.nx;++i){const Vec2 q=nodeAt(p,i,j);float f=0;const size_t at=size_t(j*p.nx+i);
   // A load over a small patch (a kneel, a foot) presses deeper than the same load lying spread out.
   for(const auto& c:press)f+=c.load*std::clamp(.2f/c.radius,.3f,2.f)*falloff(length(q-c.at)/c.radius);
   force[at]=f*K*p.sink*gain;
   for(const auto& c:kick)p.speed[at]+=c.speed*falloff(length(q-c.at)/c.radius);}
  const int count=std::max(1,int(std::ceil(dt*120)));const float h=dt/float(count);
  for(int t=0;t<count;++t){
   for(int j=0;j<p.ny;++j)for(int i=0;i<p.nx;++i){const size_t at=size_t(j*p.nx+i);const float d=p.depth[at];
    auto nb=[&](int a,int b){a=std::clamp(a,0,p.nx-1);b=std::clamp(b,0,p.ny-1);return p.depth[size_t(b*p.nx+a)];};
    const float lap=nb(i-1,j)+nb(i+1,j)+nb(i,j-1)+nb(i,j+1)-4*d;const float v=p.speed[at];
    accel[at]=force[at]-K*d+couple*lap-(v>0?pressDamp:recoverDamp)*v;}
   for(size_t at=0;at<n;++at){if(p.pinned[at]){p.depth[at]=p.speed[at]=0;continue;}p.speed[at]+=accel[at]*h;p.depth[at]+=p.speed[at]*h;
    const float lo=-.3f*p.sink,hi=1.35f*p.sink;if(p.depth[at]<lo||p.depth[at]>hi){p.depth[at]=std::clamp(p.depth[at],lo,hi);p.speed[at]=0;}}
  }
 };
 // Settled under an unchanged load it holds its shape without stepping; flat and untouched it sleeps.
 auto signature=[](const std::vector<Press>& press){std::uint64_t h=1469598103934665603ull;auto mix=[&](float v){h^=std::uint64_t(std::int64_t(std::lround(v*100)));h*=1099511628211ull;};
  for(const auto& c:press){mix(c.at.x);mix(c.at.y);mix(c.load);mix(c.radius);}return h;};
 auto run=[&](SoftPad frame,int fixture,int clutter){
  std::vector<Press> press;std::vector<Kick> kick;std::vector<int> resting;bool playerOn=false;gather(frame,clutter,press,kick,resting,playerOn);
  SoftPad* pad=nullptr;for(auto& p:m_softPads)if(p.level==m_level&&p.fixture==fixture&&p.clutter==clutter)pad=&p;
  if(!pad){if(press.empty()&&kick.empty())return;frame.level=m_level;frame.fixture=fixture;frame.clutter=clutter;frame.model=fixture>=0?w.fixtures()[size_t(fixture)].model:m_clutter[size_t(clutter)].model;
   allocate(frame,w);m_softPads.push_back(std::move(frame));pad=&m_softPads.back();}
  const auto sig=signature(press);if(!(pad->signature==sig&&pad->still>.3f&&kick.empty()))step(*pad,press,kick);
  float fastest=0;for(float v:pad->speed)fastest=std::max(fastest,std::fabs(v));const bool calm=fastest<.003f;
  pad->still=calm&&sig==pad->signature&&kick.empty()?pad->still+dt:0.f;pad->signature=sig;
  // The player standing on it goes down and up with it (the walk's 3 cm step-down allowance would hold him over it).
  if(playerOn){const float z=pad->top-std::max(0.f,pad->depthAt(player));if(std::fabs(m_player.z-z)<pad->sink*1.6f+.03f)m_player.z=z;}
  // What lies asleep on it goes down and up with it; what moves finds the pressed top (softSinkAt).
  for(int i:resting){auto& v=m_clutter[size_t(i)];if(!v.sleeping)continue;const float z=pad->top-std::max(0.f,pad->depthAt(v.pos));if(std::fabs(v.z-z)>.002f&&std::fabs(v.z-z)<pad->sink*1.6f+.03f)v.z=z;}
  pad->quiet=press.empty()&&kick.empty()&&calm&&pad->peak()<.0008f?pad->quiet+dt:0.f;if(pad->quiet>.25f)pad->quiet=-1;   // flat and untouched: asleep
 };
 const auto& fixtures=w.fixtures();
 for(int i=0;i<int(fixtures.size());++i){const auto& f=fixtures[size_t(i)];if(!f.visible||!soft::isSoft(f.model)||lengthSq(f.position-player)>range*range)continue;SoftPad frame;if(frameFixture(w,f,frame))run(frame,i,-1);}
 for(int i=0;i<int(m_clutter.size());++i){const auto& c=m_clutter[size_t(i)];if(!soft::isSoft(c.model)||lengthSq(c.pos-player)>range*range)continue;SoftPad frame;if(frameClutter(c,frame))run(frame,-1,i);}
 std::erase_if(m_softPads,[](const SoftPad& p){return p.quiet<0;});
}
}
