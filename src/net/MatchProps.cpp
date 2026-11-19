#include "Match.h"
#include "MatchProtocol.h"
#include "Wire.h"
#include "../game/Game.h"
#include "../game/physics/PropBodies.h"
#include <algorithm>
#include <cmath>
// Props in a match (docs/Systems/multiplayer.md "Props"): the host's are the truth, in deathmatch and co-op.
//  - The host simulates every prop. Fifteen times a second each guest is sent, for the maps it stands on or beside,
//    every prop that moved since it was last sent (and three times more once it stops, against loss), and a few
//    more in turn whether they moved or not, so a guest who joined late or lost a packet catches up. Breaks, props
//    the host spawned and panes it lost go reliably.
//  - A guest poses its props from that, 100 ms behind the host's clock (as it draws players), and its solver leaves
//    them asleep. What its own hand holds (or the Physgloves pull) it moves itself and reports (PropCarry); the host
//    poses that prop from the reports and leaves it out of its solver. Let go (dropped, punted, thrown), the guest
//    sends where it is and how it moves (PropThrow): the host carries on from there, and the guest's solver carries
//    on alone with that one prop until the host's word on it is newer than the throw, then eases onto it.
//  - A guest's blow on a prop (a punch, a kick, a VR fist) and its damage are sent (PropKnock) and dealt there; a
//    guest breaks nothing itself. A guest walking into a prop pushes it on the host (Clutter.cpp, the guests' bodies).
//  - Shards and gibs are each machine's own (Clutter::NetDebris): the break is the same, the pieces are not sent.
namespace retro::net {
using namespace protocol;
namespace {
constexpr float PropInterval=1.f/15,CarryInterval=1.f/30,PropDelay=.1f,CarryTimeout=1.f,PredictSeconds=2.5f,CatchUp=.15f;
constexpr int RestSends=3,RefreshPerTick=8;
constexpr std::uint32_t SpawnedBase=0x8000;
using Q=std::array<float,4>;
std::uint32_t millis(float s){return std::uint32_t(std::max(0.f,s)*1000.f);}
bool named(const Clutter& c){return c.netId!=0&&c.netId!=Clutter::NetDebris;}
int namedOn(std::uint32_t id){return int(id>>16)-1;}
Q quatOf(const Clutter& c){const auto q=vectra::propOrientation(c);return {q.w,q.x,q.y,q.z};}
// Smallest three: which of the four is largest (2 bits) and the other three in [-1/sqrt2, 1/sqrt2] (10 bits each).
std::uint32_t packQuat(Q q){
 int big=0;for(int i=1;i<4;++i)if(std::fabs(q[size_t(i)])>std::fabs(q[size_t(big)]))big=i;
 const float sign=q[size_t(big)]<0?-1.f:1.f;std::uint32_t v=std::uint32_t(big);int shift=2;
 for(int i=0;i<4;++i){if(i==big)continue;const float x=std::clamp(q[size_t(i)]*sign*1.41421356f,-1.f,1.f);v|=std::uint32_t(std::lround((x*.5f+.5f)*1023.f))<<shift;shift+=10;}
 return v;}
Q unpackQuat(std::uint32_t v){
 const int big=int(v&3u);Q q{};float sum=0;int shift=2;
 for(int i=0;i<4;++i){if(i==big)continue;const float x=(float((v>>shift)&1023u)/1023.f*2.f-1.f)/1.41421356f;q[size_t(i)]=x;sum+=x*x;shift+=10;}
 q[size_t(big)]=std::sqrt(std::max(0.f,1.f-sum));return q;}
Q nlerp(Q a,Q b,float t){float d=a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3];if(d<0)for(auto& x:b)x=-x;
 Q r;for(size_t i=0;i<4;++i)r[i]=a[i]+(b[i]-a[i])*t;const float l=std::sqrt(r[0]*r[0]+r[1]*r[1]+r[2]*r[2]+r[3]*r[3]);if(l>1e-6f)for(auto& x:r)x/=l;return r;}
void setPose(Clutter& c,Point3 at,Q q){
 c.pos={at.x,at.y};c.z=at.z;const vectra::Quat v=vectra::Quat{q[0],q[1],q[2],q[3]}.normalized();
 c.orient={v.w,v.x,v.y,v.z};v.toEuler(c.yaw,c.pitch,c.roll);c.orientEuler={c.yaw,c.pitch,c.roll};}
void still(Clutter& c){c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=true;c.projectile=false;c.restTime=0;}
std::uint8_t flagsOf(const Clutter& c,int level){
 return std::uint8_t((c.sleeping?1u:0u)|(c.broken?2u:0u)|(c.netHolder>=0?4u:0u)|(c.liquid.holds()?8u:0u)|(namedOn(c.netId)!=level?16u:0u));}
int liquidOf(const Clutter& c){return c.liquid.holds()?int(std::lround(std::clamp(c.liquid.level,0.f,1.f)*255.f)):-1;}
void writeId(wire::Writer& w,int level,std::uint32_t id){w.u8(unsigned(std::clamp(level,0,255))).u8(unsigned(std::clamp(namedOn(id),0,255))).u16(id&0xFFFFu);}
std::uint32_t readId(wire::Reader& r,int& level){level=int(r.u8());const int on=int(r.u8());return (std::uint32_t(on+1)<<16)|r.u16();}
void writePose(wire::Writer& w,Point3 at,Q q){w.q16(at.x,256).q16(at.y,256).q16(at.z,256).u32(packQuat(q));}
}

void Match::clearProps(){
 m_propTracks.clear();m_propSent.clear();m_propCursor.clear();m_panesSeen.clear();m_spawnedProps.clear();m_carried.clear();
 m_propTimer=m_carryTimer=0;m_lastPropMs=0;m_drivenId=0;m_drivenLevel=-1;
 Game& g=m_game;g.m_netPropIntents.clear();g.m_netPropEvents.clear();
}

// A prop by its match name: where it was seeded (an authored prop sits at its record's index unless something
// moved it), else wherever it is now (a prop carried over a seam is in the next map's list).
Clutter* Match::propById(int level,std::uint32_t id,int* found,std::size_t* index){
 Game& g=m_game;if(!id||id==Clutter::NetDebris)return nullptr;
 auto look=[&](int l)->Clutter*{if(l<0||l>=g.chunkCount()||!g.chunkResident(l))return nullptr;auto& list=l==g.m_level?g.m_clutter:g.m_chunks[size_t(l)].clutter;
  const std::uint32_t low=id&0xFFFFu;
  if(namedOn(id)==l&&low>=1&&low<SpawnedBase&&low-1<list.size()&&list[low-1].netId==id){if(found)*found=l;if(index)*index=low-1;return &list[low-1];}
  for(std::size_t i=0;i<list.size();++i)if(list[i].netId==id){if(found)*found=l;if(index)*index=i;return &list[i];}
  return nullptr;};
 if(auto* c=look(level))return c;
 for(int l=0;l<g.chunkCount();++l)if(l!=level)if(auto* c=look(l))return c;
 return nullptr;
}

// ----- The host -----
void Match::hostProps(float dt){
 Game& g=m_game;const int maps=g.chunkCount();
 if(int(m_spawnedProps.size())!=maps){m_spawnedProps.assign(size_t(maps),0);m_panesSeen.assign(size_t(maps),std::size_t(-1));}
 for(int l=0;l<maps;++l){if(!g.chunkResident(l))continue;auto& list=l==g.m_level?g.m_clutter:g.m_chunks[size_t(l)].clutter;
  // Props that came into being here (not shards or gibs): named, and announced.
  for(auto& c:list){if(c.netId)continue;if(c.kind==6||c.model<0){c.netId=Clutter::NetDebris;continue;}
   c.netId=(std::uint32_t(l+1)<<16)|(SpawnedBase+std::min<std::uint32_t>(m_spawnedProps[size_t(l)]++,0x7FFF));
   const auto s=c.size();wire::Writer w;w.u8(PropSpawn).u8(unsigned(l)).u16(c.netId&0xFFFFu).u8(unsigned(std::clamp(c.kind,0,255))).i16(c.model)
    .u16(unsigned(std::clamp(int(s[0]*1000),0,65535))).u16(unsigned(std::clamp(int(s[1]*1000),0,65535))).u16(unsigned(std::clamp(int(s[2]*1000),0,65535)));
   writePose(w,{c.pos.x,c.pos.y,c.z},quatOf(c));broadcast(Events,w,true);m_propBytes+=w.b.size();}
  // Panes and walls the host's world lost.
  const auto& broken=g.chunkWorld(l).brokenBreakables();auto& seen=m_panesSeen[size_t(l)];
  if(seen==std::size_t(-1)||seen>broken.size())seen=broken.size();
  for(;seen<broken.size();++seen){wire::Writer w;w.u8(PaneBreak).u8(unsigned(l)).u16(broken[seen]);broadcast(Events,w,true);m_propBytes+=w.b.size();}
 }
 // The breaks of this frame.
 for(const auto& e:g.m_netPropEvents){wire::Writer w;w.u8(PropBreak);writeId(w,e.level,e.id);
  const float l=std::sqrt(e.way.x*e.way.x+e.way.y*e.way.y+e.way.z*e.way.z),k=l>1?1/l:1;w.q8(e.way.x*k,127).q8(e.way.y*k,127).q8(e.way.z*k,127);broadcast(Events,w,true);m_propBytes+=w.b.size();}
 g.m_netPropEvents.clear();
 // What a guest carries stays where its hand last put it; a hand gone quiet (or a guest gone) lets go.
 for(auto it=m_carried.begin();it!=m_carried.end();){const auto& s=m_slots[size_t(it->slot)];
  const bool live=s.used&&s.carryId==it->id&&m_clock-s.carryAt<CarryTimeout;
  if(live){++it;continue;}
  if(auto* c=propById(it->level,it->id)){c->netHolder=-1;c->sleeping=false;c->restTime=0;}
  if(s.used&&s.carryId==it->id)m_slots[size_t(it->slot)].carryId=0;
  it=m_carried.erase(it);}
 for(const auto& k:m_carried)if(auto* c=propById(k.level,k.id)){c->velocity={};c->vz=c->spin=c->pitchSpeed=c->rollSpeed=0;c->sleeping=true;}
 if((m_propTimer-=dt)<=0){m_propTimer=std::max(0.f,m_propTimer+PropInterval);sendPropStates();}
}

bool Match::seesLevel(int slot,int level)const{
 const auto& s=m_slots[size_t(slot)];if(!s.used||!s.ready||!s.spawned||!s.haveState)return false;
 return level==s.latest.level||(m_game.chunkResident(level)&&m_game.stitchedTo(level,s.latest.level));
}

void Match::sendPropStates(){
 Game& g=m_game;const int maps=g.chunkCount();
 if(int(m_propSent.size())!=maps){m_propSent.assign(size_t(maps),{});m_propCursor.assign(size_t(maps),0);}
 const std::uint32_t ms=millis(m_clock);
 for(int l=0;l<maps;++l){
  bool wanted=false;for(int i=0;i<MaxSlots&&!wanted;++i)wanted=i!=m_selfSlot&&seesLevel(i,l);
  if(!wanted||!g.chunkResident(l))continue;
  const auto& list=g.chunkClutter(l);auto& memo=m_propSent[size_t(l)];
  std::vector<std::size_t> pick;
  for(std::size_t i=0;i<list.size();++i){const auto& c=list[i];if(!named(c))continue;
   auto& m=memo[c.netId];const Point3 at{c.pos.x,c.pos.y,c.z};const Q q=quatOf(c);const auto flags=flagsOf(c,l);const int liquid=liquidOf(c);
   // First sight: what every machine seeded alike is not news (the slow round below corrects a late joiner).
   if(!m.known){m={at,q,flags,liquid,0,true};continue;}
   const float turn=1-std::fabs(q[0]*m.q[0]+q[1]*m.q[1]+q[2]*m.q[2]+q[3]*m.q[3]);
   const bool moved=std::fabs(at.x-m.at.x)>.003f||std::fabs(at.y-m.at.y)>.003f||std::fabs(at.z-m.at.z)>.003f||turn>2e-5f||flags!=m.flags||std::abs(liquid-m.liquid)>1;
   if(moved)m.restSends=RestSends+1;
   if(m.restSends>0){--m.restSends;pick.push_back(i);m.at=at;m.q=q;m.flags=flags;m.liquid=liquid;}
  }
  // And a few in turn, moved or not.
  if(!list.empty())for(int k=0;k<RefreshPerTick&&k<int(list.size());++k){auto& cur=m_propCursor[size_t(l)];cur%=list.size();const std::size_t i=cur++;
   if(named(list[i])&&std::find(pick.begin(),pick.end(),i)==pick.end())pick.push_back(i);}
  if(pick.empty())continue;
  for(int slot=0;slot<MaxSlots;++slot){if(slot==m_selfSlot||!seesLevel(slot,l))continue;
   std::size_t at=0;
   while(at<pick.size()){
    wire::Writer w;w.u8(PropState).u32(ms).u8(unsigned(l));const std::size_t countAt=w.b.size();w.u8(0);unsigned n=0;
    for(;at<pick.size()&&w.b.size()<1100&&n<255;++at){const auto& c=list[pick[at]];if(c.netHolder==slot)continue;   // its own hand has it
     const auto flags=flagsOf(c,l);w.u16(c.netId&0xFFFFu).u8(flags);if(flags&16)w.u8(unsigned(std::clamp(namedOn(c.netId),0,255)));
     writePose(w,{c.pos.x,c.pos.y,c.z},quatOf(c));if(flags&8)w.u8(unsigned(liquidOf(c)));++n;}
    if(!n)continue;w.b[countAt]=std::uint8_t(n);send(m_slots[size_t(slot)].peer,State,w,false);m_propBytes+=w.b.size();}
  }
 }
}

// ----- The guest -----
void Match::guestProps(float dt){
 Game& g=m_game;
 // A hand or a fist on a prop: to the host.
 for(const auto& in:g.m_netPropIntents){
  wire::Writer w;w.u8(PropKnock);writeId(w,in.level,in.id);w.u8(unsigned(in.type)).f32(in.at.x).f32(in.at.y).f32(in.at.z).f32(in.v.x).f32(in.v.y).f32(in.v.z).f32(in.amount);
  send(m_hostId,Events,w,true);m_propBytes+=w.b.size();}
 g.m_netPropIntents.clear();
 // What this guest's hand drives: the prop it carries, or the one the Physgloves are pulling to it.
 const int driven=g.m_heldClutter>=0?g.m_heldClutter:g.m_physglove.flying;
 const std::uint32_t id=driven>=0&&size_t(driven)<g.m_clutter.size()&&named(g.m_clutter[size_t(driven)])?g.m_clutter[size_t(driven)].netId:0;
 if(id&&(m_carryTimer-=dt)<=0){m_carryTimer=CarryInterval;const auto& c=g.m_clutter[size_t(driven)];
  wire::Writer w;w.u8(PropCarry);writeId(w,g.m_level,id);writePose(w,{c.pos.x,c.pos.y,c.z},quatOf(c));send(m_hostId,State,w,false);m_propBytes+=w.b.size();}
 if(m_drivenId&&m_drivenId!=id){
  // Let go: where it is and how it goes, exactly; then this solver carries it on until the host's word is newer.
  int level=-1;if(auto* c=propById(m_drivenLevel,m_drivenId,&level)){
   const auto spin=vectra::propAngular(*c);
   wire::Writer w;w.u8(PropThrow);writeId(w,level,m_drivenId);w.f32(c->pos.x).f32(c->pos.y).f32(c->z).u32(packQuat(quatOf(*c))).f32(c->velocity.x).f32(c->velocity.y).f32(c->vz).f32(spin.x).f32(spin.y).f32(spin.z);
   send(m_hostId,Events,w,true);m_propBytes+=w.b.size();
   auto& t=m_propTracks[m_drivenId];t.level=level;t.n=0;t.done=false;t.blend=0;t.predictUntil=m_clock+PredictSeconds;t.throwAt=hostNow();}
 }
 if(!id)m_carryTimer=0;
 m_drivenId=id;m_drivenLevel=g.m_level;
 // The host's props where the host has them, 100 ms behind its clock.
 const float t=renderTime();
 for(auto it=m_propTracks.begin();it!=m_propTracks.end();){auto& tr=it->second;
  if(tr.done||it->first==id){++it;continue;}
  int level=-1;Clutter* c=propById(tr.level,it->first,&level);if(!c){it=m_propTracks.erase(it);continue;}
  if(tr.predictUntil>m_clock){
   // Thrown here: until the host's word on it is newer than the throw, this solver has it.
   if(!(tr.n>0&&tr.s[size_t(tr.n-1)].t>=tr.throwAt+CatchUp&&t>=tr.throwAt+CatchUp)){++it;continue;}
   tr.predictUntil=-1;tr.blend=.3f;}
  else if(tr.predictUntil>=0){tr.predictUntil=-1;tr.blend=.3f;}
  if(tr.n==0){++it;continue;}
  PropSample at=tr.s[0];
  if(t>=tr.s[size_t(tr.n-1)].t)at=tr.s[size_t(tr.n-1)];
  else if(t>tr.s[0].t){int i=0;while(i+2<tr.n&&tr.s[size_t(i+1)].t<=t)++i;const auto& a=tr.s[size_t(i)];const auto& b=tr.s[size_t(i+1)];
   const float f=b.t>a.t?std::clamp((t-a.t)/(b.t-a.t),0.f,1.f):1.f;at.at={a.at.x+(b.at.x-a.at.x)*f,a.at.y+(b.at.y-a.at.y)*f,a.at.z+(b.at.z-a.at.z)*f};at.q=nlerp(a.q,b.q,f);at.rest=false;}
  // The sample is in its map's frame (tr.level); the prop may be held in a neighbour's list.
  const Vec2 shift=g.chunkOffset(tr.level)-g.chunkOffset(level);at.at.x+=shift.x;at.at.y+=shift.y;
  if(tr.blend>0){tr.blend-=dt;const float k=std::min(1.f,dt*14.f);const Point3 now{c->pos.x,c->pos.y,c->z};
   at.at={now.x+(at.at.x-now.x)*k,now.y+(at.at.y-now.y)*k,now.z+(at.at.z-now.z)*k};at.q=nlerp(quatOf(*c),at.q,k);}
  setPose(*c,at.at,at.q);still(*c);
  if(tr.blend<=0&&t>=tr.s[size_t(tr.n-1)].t&&tr.s[size_t(tr.n-1)].rest)tr.done=true;
  ++it;}
 // Whatever this solver woke that the host did not (a prop its own body or a thrown prop struck): back where the
 // host last had it, asleep.
 for(int l=0;l<g.chunkCount();++l){if(!g.inPlayerFrame(l))continue;auto& list=l==g.m_level?g.m_clutter:g.m_chunks[size_t(l)].clutter;
  for(std::size_t i=0;i<list.size();++i){auto& c=list[i];if(c.sleeping||!named(c)||c.broken||(l==g.m_level&&int(i)==driven))continue;
   const auto it=m_propTracks.find(c.netId);
   if(it!=m_propTracks.end()&&it->second.predictUntil>m_clock)continue;
   if(it!=m_propTracks.end()&&it->second.n>0&&it->second.done){const auto& last=it->second.s[size_t(it->second.n-1)];const Vec2 shift=g.chunkOffset(it->second.level)-g.chunkOffset(l);
    setPose(c,{last.at.x+shift.x,last.at.y+shift.y,last.at.z},last.q);}
   still(c);}}
}

// ----- Messages -----
bool Match::handlePropState(int slot,unsigned type,wire::Reader& r){
 Game& g=m_game;
 if(m_host){
  if(type!=PropCarry)return false;
  int level=0;const std::uint32_t id=readId(r,level);Point3 at;at.x=r.q16(256);at.y=r.q16(256);at.z=r.q16(256);const Q q=unpackQuat(r.u32());
  if(r.bad||slot<0||level>=g.chunkCount())return true;
  int found=-1;std::size_t index=0;Clutter* c=propById(level,id,&found,&index);if(!c||c->broken)return true;
  if(found==g.m_level&&int(index)==g.m_heldClutter)return true;   // the host's own hand has it
  const Vec2 shift=g.chunkOffset(level)-g.chunkOffset(found);setPose(*c,{at.x+shift.x,at.y+shift.y,at.z},q);still(*c);c->netHolder=slot;
  auto& s=m_slots[size_t(slot)];s.carryId=id;s.carryAt=m_clock;
  if(std::none_of(m_carried.begin(),m_carried.end(),[&](const PropCarried& k){return k.id==id;}))m_carried.push_back({found,id,slot});
  return true;
 }
 if(type!=PropState)return false;
 const std::uint32_t ms=r.u32();const int level=int(r.u8());const int count=int(r.u8());if(r.bad||level>=g.chunkCount())return true;
 const float t=float(ms)/1000.f;
 for(int k=0;k<count;++k){
  const std::uint32_t low=r.u16();const unsigned flags=r.u8();const int on=flags&16?int(r.u8()):level;
  Point3 at;at.x=r.q16(256);at.y=r.q16(256);at.z=r.q16(256);const Q q=unpackQuat(r.u32());const int liquid=flags&8?int(r.u8()):-1;
  if(r.bad)break;
  const std::uint32_t id=(std::uint32_t(on+1)<<16)|low;if(id==m_drivenId)continue;   // its own hand has it
  int found=-1;Clutter* c=propById(level,id,&found);if(!c)continue;
  if((flags&2)&&!c->broken){c->broken=true;still(*c);}   // broken while this guest was elsewhere: the break was not seen
  if(liquid>=0&&c->liquid.holds())c->liquid.level=float(liquid)/255.f;
  auto& tr=m_propTracks[id];
  if(tr.level!=level){tr.n=0;tr.level=level;}
  if(tr.n>0&&t<=tr.s[size_t(tr.n-1)].t)continue;   // older than what came already
  if(tr.n==int(tr.s.size())){for(size_t i=1;i<tr.s.size();++i)tr.s[i-1]=tr.s[i];--tr.n;}
  tr.s[size_t(tr.n++)]={t,at,q,(flags&1)!=0};tr.done=false;
 }
 return true;
}

bool Match::handlePropEvent(int slot,unsigned type,wire::Reader& r){
 Game& g=m_game;
 if(m_host){
  if(type==PropThrow){
   int level=0;const std::uint32_t id=readId(r,level);Point3 at;at.x=r.f32();at.y=r.f32();at.z=r.f32();const Q q=unpackQuat(r.u32());
   Point3 v;v.x=r.f32();v.y=r.f32();v.z=r.f32();vectra::V3 w;w.x=r.f32();w.y=r.f32();w.z=r.f32();
   if(r.bad||slot<0||level>=g.chunkCount())return true;
   int found=-1;Clutter* c=propById(level,id,&found);
   m_carried.erase(std::remove_if(m_carried.begin(),m_carried.end(),[&](const PropCarried& k){return k.id==id;}),m_carried.end());
   if(m_slots[size_t(slot)].carryId==id)m_slots[size_t(slot)].carryId=0;
   if(!c||c->broken)return true;
   auto clampV=[](float x,float limit){return std::isfinite(x)?std::clamp(x,-limit,limit):0.f;};
   const Vec2 shift=g.chunkOffset(level)-g.chunkOffset(found);setPose(*c,{at.x+shift.x,at.y+shift.y,at.z},q);
   c->netHolder=-1;c->velocity={clampV(v.x,20),clampV(v.y,20)};c->vz=clampV(v.z,20);vectra::setPropAngular(*c,{clampV(w.x,30),clampV(w.y,30),clampV(w.z,30)});
   c->sleeping=false;c->restTime=0;c->soundOwner=-1;c->projectile=v.x*v.x+v.y*v.y+v.z*v.z>4.f;
   return true;
  }
  if(type==PropKnock){
   int level=0;const std::uint32_t id=readId(r,level);const int kind=int(r.u8());Point3 at,v;at.x=r.f32();at.y=r.f32();at.z=r.f32();v.x=r.f32();v.y=r.f32();v.z=r.f32();const float amount=r.f32();
   if(r.bad||level>=g.chunkCount())return true;
   int found=-1;std::size_t index=0;Clutter* c=propById(level,id,&found,&index);if(!c||c->broken)return true;
   const Vec2 shift=g.chunkOffset(level)-g.chunkOffset(found);at.x+=shift.x;at.y+=shift.y;
   const float size=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(!std::isfinite(size)||!std::isfinite(amount))return true;
   if(kind==0){if(size>200)v=v*(200/size);g.knockClutter(found,index,at,v);}
   else g.damageClutter(found,index,std::clamp(amount,0.f,500.f),at,v);
   return true;
  }
  return false;
 }
 if(type==PropBreak){
  int level=0;const std::uint32_t id=readId(r,level);Point3 way;way.x=r.q8(127);way.y=r.q8(127);way.z=r.q8(127);if(r.bad)return true;
  int found=-1;std::size_t index=0;if(Clutter* c=propById(level,id,&found,&index);c&&!c->broken){m_propTracks.erase(id);g.m_netPropApply=true;g.breakClutter(found,index,way);g.m_netPropApply=false;}
  return true;
 }
 if(type==PropSpawn){
  const int level=int(r.u8());const std::uint32_t low=r.u16();const int kind=int(r.u8());const int model=r.i16();std::array<float,3> size{};for(auto& s:size)s=float(r.u16())/1000.f;
  Point3 at;at.x=r.q16(256);at.y=r.q16(256);at.z=r.q16(256);const Q q=unpackQuat(r.u32());
  if(r.bad||level>=g.chunkCount()||!g.chunkResident(level))return true;
  const std::uint32_t id=(std::uint32_t(level+1)<<16)|low;if(propById(level,id))return true;
  Clutter c;c.kind=kind;c.model=model;if(model>=0||kind==6)c.dims=size;setPose(c,at,q);still(c);c.netId=id;
  (level==g.m_level?g.m_clutter:g.m_chunks[size_t(level)].clutter).push_back(c);
  return true;
 }
 if(type==PaneBreak){
  const int level=int(r.u8());const int pane=int(r.u16());if(r.bad||level>=g.chunkCount()||!g.chunkResident(level))return true;
  const World& w=g.chunkWorld(level);if(pane<0||size_t(pane)>=w.breakables().size()||!w.breakableIntact(pane))return true;
  const auto& b=w.breakables()[size_t(pane)];const Point3 middle{(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,(b.bottom+b.top)*.5f};
  g.m_netPropApply=true;for(int k=0;k<8&&g.chunkWorld(level).breakableIntact(pane);++k)g.damagePane(level,pane,1e6f,middle,{0,0,-1});g.m_netPropApply=false;   // (its map's own frame)
  return true;
 }
 return false;
}
}
