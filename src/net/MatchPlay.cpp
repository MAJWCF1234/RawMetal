#include "Match.h"
#include "MatchProtocol.h"
#include "Wire.h"
#include "../game/Game.h"
#include "../game/Combat.h"
#include "../game/CreatureAnimation.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cmath>
#include <optional>
namespace retro::net {
using namespace protocol;
namespace {
constexpr float SnapshotInterval=1.f/20,GuestInterval=1.f/30,CreatureInterval=1.f/15,DoorInterval=.2f;
constexpr float InterpolationDelay=.1f,HistorySeconds=1.5f,MaxRewind=.3f;
constexpr float RespawnDeathmatch=3,RespawnCoop=10,RestartAfter=10,FeedSeconds=6;
std::uint32_t millis(float seconds){return std::uint32_t(std::max(0.f,seconds)*1000.f);}
float stanceHeight(const NetPlayerState& s){return s.prone?Player::ProneHeight:s.crouched?Player::CrouchedHeight:Player::StandingHeight;}
float distance2(Point3 a,Point3 b){const float x=a.x-b.x,y=a.y-b.y;return std::sqrt(x*x+y*y);}
void writeVec(wire::Writer& w,Point3 p,float scale){w.q16(p.x,scale).q16(p.y,scale).q16(p.z,scale);}
Point3 readVec(wire::Reader& r,float scale){Point3 p;p.x=r.q16(scale);p.y=r.q16(scale);p.z=r.q16(scale);return p;}
void writeUnit(wire::Writer& w,Point3 p){w.q8(p.x,127).q8(p.y,127).q8(p.z,127);}
Point3 readUnit(wire::Reader& r){Point3 p;p.x=r.q8(127);p.y=r.q8(127);p.z=r.q8(127);return p;}
}

// ----- Player state on the wire: 18 bytes (19 with health), plus 60 for a VR avatar -----
void writePlayerState(wire::Writer& w,const NetPlayerState& s,Vec2 o,bool withHealth){
 w.u8(unsigned(std::clamp(s.level,0,255))).q16(s.world.x-o.x,256).q16(s.world.y-o.y,256).q16(s.world.z,256).angle(s.angle).q16(s.pitch,10000)
  .q8(s.velocity.x,16).q8(s.velocity.y,16)
  .u8((s.crouched?1u:0u)|(s.prone?2u:0u)|(s.grounded?4u:0u)|(s.vr?8u:0u)|(s.alive?16u:0u)|(unsigned(std::clamp(s.weapon,0,3))<<5))
  .u8(s.shots).u8(s.reloadAge>=25?255u:unsigned(std::lround(std::max(0.f,s.reloadAge)*10))).u8(s.life).u8((s.torch?1u:0u)|(s.hidden?2u:0u)|(s.pushing?4u:0u));
 if(withHealth)w.u8(unsigned(std::clamp(int(std::ceil(s.health)),0,255)));
 if(!s.vr)return;
 const auto& p=s.pose;
 writeVec(w,p.head,4096);w.angle(p.headYaw).q16(p.headPitch,10000).angle(p.bodyYaw);writeVec(w,p.root,4096);
 w.u8(unsigned(std::clamp(int(std::lround(p.eye*100)),0,255))).u8(unsigned(std::lround(std::fmod(std::fabs(p.gait),kTwoPi)/kTwoPi*255)));
 // Slots hold WeaponId+1 (the MAC-11 is 4): three bits each, the hip's top bit in the flags byte.
 w.u8((p.crouched?1u:0u)|(p.prone?2u:0u)|(p.grounded?4u:0u)|(p.support?8u:0u)|(p.gunSide?16u:0u)|(p.active?32u:0u)|((p.hip&4u)<<4));
 w.u8((p.inHand&7u)|((p.back&7u)<<3)|((p.hip&3u)<<6)).u8(unsigned(std::clamp(int(std::lround(p.pump*1000)),0,255))).unit8(p.reload);
 for(const auto& h:p.hands){w.u8(h.tracked?1:0);writeVec(w,h.wrist,4096);writeUnit(w,h.forward);writeUnit(w,h.palm);for(float c:h.curls)w.unit8(c);}
}
bool readPlayerState(wire::Reader& r,NetPlayerState& s,bool withHealth){
 s.level=int(r.u8());s.world.x=r.q16(256);s.world.y=r.q16(256);s.world.z=r.q16(256);s.angle=r.angle();s.pitch=r.q16(10000);
 s.velocity.x=r.q8(16);s.velocity.y=r.q8(16);
 const unsigned flags=r.u8();s.crouched=flags&1;s.prone=flags&2;s.grounded=flags&4;s.vr=flags&8;s.alive=flags&16;s.weapon=int(flags>>5&3);
 s.shots=std::uint8_t(r.u8());const unsigned reload=r.u8();s.reloadAge=reload==255?99.f:float(reload)/10.f;s.life=std::uint8_t(r.u8());
 {const unsigned extra=r.u8();s.torch=extra&1;s.hidden=(extra&2)!=0;s.pushing=(extra&4)!=0;}
 if(withHealth)s.health=float(r.u8());
 s.pose={};
 if(s.vr){auto& p=s.pose;
  p.head=readVec(r,4096);p.headYaw=r.angle();p.headPitch=r.q16(10000);p.bodyYaw=r.angle();p.root=readVec(r,4096);
  p.eye=float(r.u8())/100.f;p.gait=float(r.u8())/255.f*kTwoPi;
  const unsigned bits=r.u8();p.crouched=bits&1;p.prone=bits&2;p.grounded=bits&4;p.support=bits&8;p.gunSide=std::uint8_t(bits&16?1:0);p.active=bits&32;
  const unsigned slots=r.u8();p.inHand=std::uint8_t(slots&7);p.back=std::uint8_t(slots>>3&7);p.hip=std::uint8_t((slots>>6&3)|(bits>>4&4));p.pump=float(r.u8())/1000.f;p.reload=r.unit8();
  for(auto& h:p.hands){h.tracked=r.u8()!=0;h.wrist=readVec(r,4096);h.forward=readUnit(r);h.palm=readUnit(r);for(float& c:h.curls)c=r.unit8();}
  p.velocity=s.velocity;
 }
 return !r.bad;
}
std::size_t Match::snapshotBytes(int players,bool vr){
 wire::Writer w;w.u8(Snapshot).u32(0).u8(0).i16(0).u8(0).u8(unsigned(players));
 NetPlayerState s;s.vr=vr;for(int i=0;i<players;++i){w.u8(unsigned(i));writePlayerState(w,s,{},true);}return w.b.size();
}
std::size_t Match::guestStateBytes(bool vr){
 wire::Writer w;w.u8(GuestState).u16(0).u32(0).u16(0);NetPlayerState s;s.vr=vr;writePlayerState(w,s,{},true);return w.b.size();
}

// ----- Frames, places, clocks -----
Vec2 Match::origin(int level)const{const int n=m_game.chunkCount();return n>0?m_game.chunkOffset(std::clamp(level,0,n-1)):Vec2{};}
int Match::levelAt(Vec2 world,int prefer)const{
 if(prefer>=0&&prefer<m_game.chunkCount()){const auto o=origin(prefer);if(world.x>=o.x&&world.x<o.x+World::Width&&world.y>=o.y&&world.y<o.y+World::Height)return prefer;}
 for(int level=0;level<m_game.chunkCount();++level){const auto o=origin(level);
  if(world.x>=o.x&&world.x<o.x+World::Width&&world.y>=o.y&&world.y<o.y+World::Height)return level;}
 return m_game.m_level;
}
const World* Match::worldOf(int level)const{
 if(level==m_game.m_level)return &m_game.m_world;
 return m_game.chunkResident(level)?&m_game.m_chunks[size_t(level)].world:nullptr;
}
float Match::hostNow()const{return m_host?m_clock:m_clock+m_hostOffset;}
float Match::renderTime()const{return hostNow()-InterpolationDelay;}

NetPlayerState Match::localState()const{
 const Game& g=m_game;const auto& p=g.m_player;NetPlayerState s;
 s.level=g.m_level;const Vec2 o=origin(g.m_level);s.world={p.pos.x+o.x,p.pos.y+o.y,p.z};
 s.angle=p.angle;s.pitch=p.pitch/140.f;s.velocity=g.m_velocity;
 s.crouched=p.crouched&&!p.prone;s.prone=p.prone;s.grounded=p.grounded;s.alive=!g.dead();
 s.weapon=g.m_weaponEquipped&&!g.unarmed()?int(g.m_equippedWeapon):0;s.shots=m_shotCounter;
 s.reloadAge=g.m_reloadTimer>0?g.m_reloadLength-g.m_reloadTimer:99.f;s.health=p.health;
 s.life=m_host&&m_selfSlot>=0?m_slots[size_t(m_selfSlot)].life:m_lifeLocal;
 s.vr=g.m_vrInputActive&&g.m_netAvatar.active;s.torch=g.flashlightOn();s.hidden=g.hiddenLocker()>=0;
 s.pushing=g.m_propPushAttempt&&length(g.m_velocity)<.3f;
 if(s.vr){s.pose=g.m_netAvatar;const Point3 feet{p.pos.x,p.pos.y,p.z};s.pose.head=s.pose.head-feet;s.pose.root=s.pose.root-feet;for(auto& h:s.pose.hands)h.wrist=h.wrist-feet;}
 return s;
}
// A player as they stood at host time t: between the two samples around it (a jump of more than 4 m is a
// respawn or a correction, not a walk), or carried on by their velocity for at most 0.2 s past the newest.
bool Match::sampleAt(const Slot& slot,float t,NetPlayerState& out)const{
 const auto& h=slot.history;if(h.empty())return false;
 if(t<=h.front().t){out=h.front().s;return true;}
 if(t>=h.back().t){out=h.back().s;if(out.alive){const float ahead=std::min(t-h.back().t,.2f);out.world.x+=out.velocity.x*ahead;out.world.y+=out.velocity.y*ahead;}return true;}
 size_t i=0;while(i+2<h.size()&&h[i+1].t<=t)++i;
 const auto& a=h[i];const auto& b=h[i+1];const float f=b.t>a.t?std::clamp((t-a.t)/(b.t-a.t),0.f,1.f):1.f;
 out=f<.5f?a.s:b.s;
 if(distance2(a.s.world,b.s.world)>4.f||(a.s.level!=b.s.level&&std::fabs(a.s.world.z-b.s.world.z)>3.f))return true;
 out.world={a.s.world.x+(b.s.world.x-a.s.world.x)*f,a.s.world.y+(b.s.world.y-a.s.world.y)*f,a.s.world.z+(b.s.world.z-a.s.world.z)*f};
 out.velocity=a.s.velocity+(b.s.velocity-a.s.velocity)*f;
 out.angle=a.s.angle+std::remainder(b.s.angle-a.s.angle,kTwoPi)*f;
 out.pitch=a.s.pitch+(b.s.pitch-a.s.pitch)*f;
 return true;
}

// ----- The host -----
void Match::hostFrame(float dt){
 Game& g=m_game;
 // Members who left: their slot frees.
 const auto members=m_session->members();
 for(int i=0;i<MaxSlots;++i){auto& s=m_slots[size_t(i)];if(!s.used||i==m_selfSlot)continue;
  if(std::find(members.begin(),members.end(),s.peer)==members.end()){m_notice=s.name+" left";m_noticeTime=3;s=Slot{};m_rosterDirty=true;}}
 // The host's own state, as the guests' come in: into the history the shots are rewound through.
 auto& me=m_slots[size_t(m_selfSlot)];
 if(me.alive&&g.dead())registerDeath(m_selfSlot,-1,0);
 if(me.alive)me.health=g.m_player.health;
 NetPlayerState own=localState();own.health=me.health;own.alive=me.alive;
 if(own.shots!=me.lastShots){me.lastShotAt=m_clock;me.lastShots=own.shots;}
 me.latest=own;me.haveState=true;me.acceptedAt=m_clock;
 if(me.history.empty()||me.history.back().t<m_clock)me.history.push_back({m_clock,own});
 for(auto& s:m_slots){
  while(s.history.size()>2&&s.history.front().t<m_clock-HistorySeconds)s.history.pop_front();
  s.correctCooldown-=dt;s.spawnGrace-=dt;
 }
 for(int i=0;i<MaxSlots;++i){auto& s=m_slots[size_t(i)];
  if(s.used&&s.ready&&!s.alive&&s.respawnIn>=0&&!m_over&&(s.respawnIn-=dt)<=0)respawn(i);}
 trackLevels();   // the maps the guests stand on: kept and run (co-op)
 hostProps(dt);   // the props, the host's the truth (MatchProps.cpp)
 if(m_mode==Mode::Deathmatch){
  clearCreatures();m_game.m_remoteBodies.clear();
  if(!m_over){
   m_roundClock+=dt;m_timeLeft=m_timeLimit>0?std::max(0.f,m_timeLimit-m_roundClock):-1.f;
   bool limit=m_timeLimit>0&&m_timeLeft<=0;for(const auto& s:m_slots)limit|=s.used&&m_fragLimit>0&&s.frags>=m_fragLimit;
   if(limit)roundOver();
  }else if((m_restartIn-=dt)<=0)newRound();
  trackPickups();pickupRespawns(dt);   // the guns lie on the map: what one takes goes from every screen, and comes back
 }else{
  if((m_creatureTimer-=dt)<=0){m_creatureTimer=CreatureInterval;sendCreatures();}
  if((m_doorTimer-=dt)<=0){m_doorTimer=DoorInterval;sendDoors();}
  trackPickups();
  coopHostFrame(dt);   // the guests among the minds' players; what the minds do, to the guests (MatchCoop.cpp)
  // The host went somewhere new (a crossing, a teleport, a respawn): said to everyone. Every map a player stands on
  // runs, so nobody is left on a frozen map; only with follow on (console COOPFOLLOW) do the guests on the map it
  // left come along.
  if(g.m_level!=m_lastLevel){const int left=m_lastLevel;m_lastLevel=g.m_level;const Vec2 o=origin(g.m_level);
   wire::Writer w;w.u8(Travel).u8(unsigned(g.m_level)).f32(own.world.x-o.x).f32(own.world.y-o.y).f32(own.world.z).angle(own.angle).u8(unsigned(std::clamp(left,0,255))).u8(m_followHost?1:0);broadcast(Events,w,true);
   if(m_followHost&&left>=0)for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(i==m_selfSlot||!s.used||!s.ready||!s.spawned||!s.haveState||!s.alive||s.latest.level!=left)continue;
    const Spawn sp=besideHost(i);placePlayer(i,sp.level,sp.world);}}
 }
 if((m_stateTimer-=dt)<=0){m_stateTimer=std::max(0.f,m_stateTimer+SnapshotInterval);sendSnapshot();}
 int count=0;for(const auto& s:m_slots)count+=s.used;m_session->setLobbyPlayers(count);
}

// Co-op: the maps the guests stand on. The host keeps each resident with the maps beside it (Game::m_netKeep) and
// runs each that is not its own in a frame of its own (m_netForeign, Game::updateForeignChunks).
void Match::trackLevels(){
 Game& g=m_game;const int n=g.chunkCount();std::vector<char> foreign(size_t(std::max(0,n)),0),keep(size_t(std::max(0,n)),0);bool any=false,kept=false;
 if(m_mode==Mode::Coop)for(int i=0;i<MaxSlots;++i){if(i==m_selfSlot)continue;const auto& s=m_slots[size_t(i)];if(!s.used||!s.ready||!s.spawned||!s.haveState)continue;
  const int level=s.latest.level;if(level<0||level>=n)continue;
  keep[size_t(level)]=1;kept=true;for(int l=0;l<n;++l)if(g.stitchedTo(l,level))keep[size_t(l)]=1;
  if(level!=g.m_level){foreign[size_t(level)]=1;any=true;}}
 if(any)g.m_netForeign=std::move(foreign);else g.m_netForeign.clear();
 if(kept)g.m_netKeep=std::move(keep);else g.m_netKeep.clear();
}

void Match::sendSnapshot(){
 std::vector<int> list;for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(s.used&&s.ready&&s.spawned&&s.haveState)list.push_back(i);}
 // One packet under EOS's 1170 bytes: sixteen VR players split in two.
 size_t at=0;std::size_t total=0;
 while(at<list.size()||(at==0&&list.empty())){
  wire::Writer w;w.u8(Snapshot).u32(millis(m_clock)).u8(unsigned(m_round&255)).i16(m_timeLeft<0?-1:int(m_timeLeft*10)).u8(m_over?1:0);
  const size_t countAt=w.b.size();w.u8(0);int n=0;
  while(at<list.size()&&w.b.size()<1000){const int i=list[at++];const auto& s=m_slots[size_t(i)];NetPlayerState st=s.latest;st.health=s.health;st.alive=s.alive;st.life=s.life;
   w.u8(unsigned(i));writePlayerState(w,st,origin(st.level),true);++n;}
  w.b[countAt]=std::uint8_t(n);total+=w.b.size();broadcast(State,w,false);
  if(list.empty())break;
 }
 m_lastSnapshotBytes=total;
}

// A guest's report: taken unless it moves too fast or through a wall, then the host sends them back.
void Match::acceptGuestState(int slot,const NetPlayerState& s){
 auto& sl=m_slots[size_t(slot)];
 if(!sl.spawned||s.life!=sl.life||!sl.alive)return;
 if(sl.acceptedAt>=0&&sl.haveState){
  const auto& prev=sl.latest;const float elapsed=std::max(1.f/60,m_clock-sl.acceptedAt);
  const float horizontal=distance2(s.world,prev.world),dz=s.world.z-prev.world.z;
  bool bad=horizontal>9.f*elapsed+1.f||dz>5.f*elapsed+1.5f||dz<-(40.f*elapsed+3.f);
  if(!bad&&horizontal>.05f&&s.level==prev.level)if(const World* w=worldOf(s.level)){const Vec2 o=origin(s.level);
   // From inside solid (an authored spawn in a wall, a door shut on them) every way out crosses it: a player who
   // was not standing in the open is let out, or the host would send them back into the wall forever.
   if(w->fits(prev.world.x-o.x,prev.world.y-o.y,prev.world.z,stanceHeight(prev),false))
   bad=!w->rayClear({prev.world.x-o.x,prev.world.y-o.y},prev.world.z+std::min(.9f,stanceHeight(prev)*.6f),{s.world.x-o.x,s.world.y-o.y},s.world.z+std::min(.9f,stanceHeight(s)*.6f),false,false);}
  if(bad){
   if(sl.correctCooldown<=0){sl.correctCooldown=.5f;++sl.life;const Vec2 o=origin(prev.level);
    wire::Writer w;w.u8(Correct).u8(sl.life).u8(unsigned(prev.level)).f32(prev.world.x-o.x).f32(prev.world.y-o.y).f32(prev.world.z);send(sl.peer,Events,w,true);}
   return;
  }
 }
 // What their own world did to them (a fall, a hazard) the host takes; its own hits it already counted.
 if(sl.spawnGrace<=0&&s.health<sl.health-.5f){sl.health=std::max(0.f,s.health);if(sl.health<=0)registerDeath(slot,-1,0);}
 if(s.shots!=sl.lastShots){sl.lastShotAt=m_clock;sl.lastShots=s.shots;}
 sl.latest=s;sl.latest.health=sl.health;sl.latest.alive=sl.alive;sl.haveState=true;sl.acceptedAt=m_clock;
 if(sl.history.empty()||sl.history.back().t<m_clock)sl.history.push_back({m_clock,sl.latest});
}

// Every shot of the match, the host's own included: traced in the host's world against every player as the
// shooter saw them (rewound to the time their view showed, at most 0.3 s), then its damage and its marks.
// Weapon 0 is fists: the same host path, at melee range, so punches hurt other players in deathmatch and co-op.
void Match::resolveShot(int shooter,int weapon,int level,Point3 at,Point3 direction,float viewTime){
 Game& g=m_game;
 if(m_over||weapon<0||weapon>3||shooter<0||shooter>=MaxSlots)return;
 auto& sh=m_slots[size_t(shooter)];if(!sh.used||!sh.alive)return;
 const bool melee=weapon==0;const auto& gun=playerWeaponDef(WeaponId(std::max(weapon,1)));const auto& fists=kUnarmedMelee;
 // A shot on another map is traced in that map's frame (it swapped in as the active chunk for the while).
 std::optional<Game::ChunkFrame> frame;if(level!=g.m_level&&level>=0&&level<g.chunkCount()&&g.chunkResident(level)&&!g.stitchedTo(level,g.m_level))frame.emplace(g,level);
 const Vec2 lo=origin(level),ao=origin(g.m_level);const Point3 o{at.x+lo.x-ao.x,at.y+lo.y-ao.y,at.z};
 const float size=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
 if(!std::isfinite(size)||size<.0001f||!std::isfinite(o.x)||!std::isfinite(o.y)||!std::isfinite(o.z))return;
 direction=direction*(1/size);
 if(shooter!=m_selfSlot&&sh.haveState){const Point3 feet=sh.latest.world;const float dz=o.z-feet.z;
  if(distance2({o.x+ao.x,o.y+ao.y,o.z},feet)>(melee?2.5f:3.f)||dz<-1.f||dz>3.5f)return;}   // a shot from where they are not
 const float t=shooter==m_selfSlot?m_clock:std::clamp(viewTime,m_clock-MaxRewind,m_clock);
 std::vector<Game::NetBody> bodies;
 if(m_mode==Mode::Deathmatch||m_friendlyFire)
  for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(i==shooter||!s.used||!s.ready||!s.spawned||!s.alive||!s.haveState)continue;
   NetPlayerState st;if(!sampleAt(s,t,st))continue;bodies.push_back({i,{st.world.x-ao.x,st.world.y-ao.y},st.world.z,stanceHeight(st)});}
 const float range=melee?fists.range:gun.hitscan.maxRange;
 g.m_netBodies=&bodies;const ShotHit hit=g.traceShot(o,direction,range,-1,false,melee?nullptr:&gun.hitscan);g.m_netBodies=nullptr;
 const bool player=hit.kind==ShotKind::Player&&hit.index>=1;
 const bool flesh=player||hit.kind==ShotKind::Enemy||hit.kind==ShotKind::Npc||hit.kind==ShotKind::Actor;
 const Point3 end=hit.kind==ShotKind::None?o+direction*range:hit.point;
 {wire::Writer w;w.u8(ShotFx).u8(unsigned(shooter)).u8(unsigned(weapon)).u8(unsigned(g.m_level)).f32(o.x).f32(o.y).f32(o.z).f32(end.x).f32(end.y).f32(end.z)
   .u8(flesh?1:hit.kind==ShotKind::None?2:0);writeUnit(w,hit.normal);broadcast(Events,w,true);}
 if(shooter!=m_selfSlot&&!melee)g.m_sounds.push_back({gun.cycle.fireSound,{o.x,o.y},.9f,1,true,17,o.z});
 if(player){
  if(!melee){g.m_shotRays.push_back({o,direction,hit.distance,g.m_elapsed,0,0,g.m_level});if(g.m_shotRays.size()>24)g.m_shotRays.erase(g.m_shotRays.begin());}
  g.hitBurst(g.m_level,hit.point,direction,ImpactMaterial::Flesh,-1,melee?.45f:.9f,false);
  if(melee&&shooter==m_selfSlot)g.sound(Sound::PunchHit,.65f);
  damage(hit.index-1,shooter,melee?fists.damage:hitscanDamageAt(gun.hitscan,hit.damageDistance),weapon);
 }else if(melee){   // fists on creatures, glass and props: the same path punchImpact uses alone
  HitscanDef blow{};blow.maxRange=fists.range;blow.dmgNear=blow.dmgMid=blow.dmgFar=fists.damage;blow.actorDamage=fists.damage;blow.propImpulse=fists.propImpulse;
  const float flash=g.m_hitFlash;g.m_shooter=shooter==m_selfSlot?0:shooter;
  if(hit.kind==ShotKind::Enemy||hit.kind==ShotKind::Npc||hit.kind==ShotKind::Actor||hit.kind==ShotKind::Prop||hit.breakable>=0)
   g.resolveShot(hit,o,direction,blow,true);
  g.m_shooter=0;if(shooter!=m_selfSlot)g.m_hitFlash=flash;
 }else{   // creatures, glass, props, the wall's mark: a player's shot, whoever fired it (Game::m_shooter)
  const float flash=g.m_hitFlash;g.m_shooter=shooter==m_selfSlot?0:shooter;
  g.resolveShot(hit,o,direction,gun.hitscan,true);g.m_shooter=0;
  if(shooter!=m_selfSlot)g.m_hitFlash=flash;}   // the hit marker is the shooter's own
}

void Match::damage(int victim,int attacker,float amount,int weapon){
 Game& g=m_game;if(victim<0||victim>=MaxSlots)return;auto& v=m_slots[size_t(victim)];
 if(!v.used||!v.alive||amount<=0)return;
 if(m_mode==Mode::Coop&&!m_friendlyFire&&attacker>=0&&attacker!=victim)return;
 v.health=std::max(0.f,v.health-amount);
 if(victim==m_selfSlot){if(g.m_god)v.health=g.m_player.health;else g.m_player.health=v.health;g.m_damageFlash=1;g.sound(Sound::Hurt,.7f,.92f);}
 if(attacker==m_selfSlot&&victim!=m_selfSlot)g.m_hitFlash=1;
 wire::Writer w;w.u8(Hit).u8(unsigned(victim)).u8(attacker<0?255u:unsigned(attacker)).u8(unsigned(std::clamp(int(std::lround(amount)),0,255))).u8(unsigned(std::clamp(int(std::ceil(v.health)),0,255)));
 broadcast(Events,w,true);
 if(v.health<=0)registerDeath(victim,attacker,weapon);
}

void Match::registerDeath(int victim,int killer,int weapon){
 Game& g=m_game;auto& v=m_slots[size_t(victim)];if(!v.alive)return;
 v.alive=false;v.health=0;++v.deaths;v.deathAt=m_clock;v.latest.alive=false;v.latest.health=0;
 if(killer>=0&&killer<MaxSlots&&killer!=victim&&m_slots[size_t(killer)].used)++m_slots[size_t(killer)].frags;
 v.respawnIn=m_mode==Mode::Deathmatch?RespawnDeathmatch:RespawnCoop;
 if(victim==m_selfSlot)g.m_player.health=0;
 pushFeed(killer,victim,weapon);
 wire::Writer w;w.u8(Death).u8(unsigned(victim)).u8(killer<0?255u:unsigned(killer)).u8(unsigned(std::clamp(weapon,0,255))).u8(unsigned(std::lround(v.respawnIn*10)));
 broadcast(Events,w,true);m_rosterDirty=true;
}
void Match::pushFeed(int killer,int victim,int weapon){
 FeedLine line;line.killer=killer>=0&&killer<MaxSlots&&killer!=victim?m_slots[size_t(killer)].name:std::string();
 line.victim=victim>=0&&victim<MaxSlots?m_slots[size_t(victim)].name:std::string();line.weapon=weapon;
 m_feed.push_back(line);if(m_feed.size()>5)m_feed.erase(m_feed.begin());
}

void Match::respawn(int slot){
 const Spawn sp=m_mode==Mode::Deathmatch?chooseSpawn(slot):besideHost(slot);
 auto& s=m_slots[size_t(slot)];++s.life;s.alive=true;s.health=100;s.respawnIn=-1;s.spawned=true;s.spawnGrace=1.f;s.deathAt=-1;s.correctCooldown=0;
 NetPlayerState st=s.latest;st.level=sp.level;st.world=sp.world;st.angle=sp.yaw;st.pitch=0;st.velocity={};st.alive=true;st.health=100;st.life=s.life;st.crouched=st.prone=false;st.reloadAge=99;
 s.latest=st;s.haveState=true;s.acceptedAt=m_clock;s.history.clear();s.history.push_back({m_clock,st});
 if(slot==m_selfSlot){m_lifeLocal=s.life;applySpawn(sp.level,sp.world,sp.yaw);}
 const Vec2 o=origin(sp.level);
 wire::Writer w;w.u8(Respawn).u8(unsigned(slot)).u8(s.life).u8(unsigned(sp.level)).f32(sp.world.x-o.x).f32(sp.world.y-o.y).f32(sp.world.z).angle(sp.yaw);
 broadcast(Events,w,true);m_rosterDirty=true;
}
// To a place on a given map. A custom campaign's maps need not sit side by side on one grid (their offsets may
// coincide), so another map is loaded outright, carrying health and rounds, rather than walked into: placing the
// player at the other map's coordinates inside this one put them in its walls.
void Match::teleportLocal(int level,Point3 world){
 Game& g=m_game;level=std::clamp(level,0,std::max(0,g.chunkCount()-1));
 if(level!=g.m_level)g.enterChunk(level);   // the map left keeps its state: someone may still be on it
 const Vec2 o=origin(level);
 g.m_player.pos={world.x-o.x,world.y-o.y};g.m_player.z=world.z;g.m_player.verticalVelocity=0;g.m_velocity={};
}
void Match::placePlayer(int slot,int level,Point3 world){
 if(slot<0||slot>=MaxSlots||!m_host)return;auto& s=m_slots[size_t(slot)];if(!s.used)return;
 if(slot==m_selfSlot){teleportLocal(level,world);return;}
 ++s.life;s.latest.world=world;s.latest.level=level;s.latest.life=s.life;s.acceptedAt=m_clock;s.correctCooldown=0;s.history.clear();s.history.push_back({m_clock,s.latest});
 const Vec2 o=origin(level);wire::Writer w;w.u8(Correct).u8(s.life).u8(unsigned(level)).f32(world.x-o.x).f32(world.y-o.y).f32(world.z);send(s.peer,Events,w,true);
}
// Full health, standing, at the spawn. The loadout: a fresh map's shotgun in co-op; in a deathmatch nothing at all
// (fists, no gun, no rounds): every gun is found lying on the map.
void Match::applySpawn(int level,Point3 world,float yaw){
 Game& g=m_game;teleportLocal(level,world);auto& p=g.m_player;
 const bool bare=m_mode==Mode::Deathmatch;
 p.health=100;p.pools=bare?decltype(p.pools){}:decltype(p.pools){AmmoPool{},AmmoPool{72,6},AmmoPool{}};p.angle=yaw;p.pitch=0;p.grounded=true;p.crouched=p.prone=false;p.eye=Player::StandingEye;
 if(bare){while(g.hasQuestItem(Game::HandgunItem))g.takeQuestItem(Game::HandgunItem,1);while(g.hasQuestItem(Game::MachinePistolItem))g.takeQuestItem(Game::MachinePistolItem,1);}
 g.m_shotgunOwned=!bare;
 g.m_reloadTimer=0;g.m_shotCooldown=0;g.m_damageFlash=0;g.m_won=false;g.m_handsLowered=false;g.equipWeapon(bare?WeaponId::Fists:WeaponId::Shotgun);g.m_holster=0;g.m_suppressFire=true;
 m_selfRespawnIn=-1;
}

// The spawn farthest from every living opponent; with none alive, the next in turn.
Match::Spawn Match::chooseSpawn(int slot){
 if(m_spawns.empty())buildSpawns();
 if(m_spawns.empty()){const Vec2 o=origin(m_game.m_level);return {m_game.m_level,{m_game.m_player.pos.x+o.x,m_game.m_player.pos.y+o.y,m_game.m_player.z},m_game.m_player.angle};}
 std::vector<Point3> rivals;
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(i!=slot&&s.used&&s.spawned&&s.alive&&s.haveState)rivals.push_back(s.latest.world);}
 if(rivals.empty())return m_spawns[size_t(m_spawnTurn++)%m_spawns.size()];
 size_t best=0;float bestDistance=-1;
 for(size_t k=0;k<m_spawns.size();++k){float nearest=1e9f;for(const auto& r:rivals){const float dz=m_spawns[k].world.z-r.z;nearest=std::min(nearest,std::sqrt(distance2(m_spawns[k].world,r)*distance2(m_spawns[k].world,r)+dz*dz));}
  if(nearest>bestDistance+.01f){bestDistance=nearest;best=k;}}
 return m_spawns[best];
}
// Co-op: beside the host, on open floor it can see (the host itself beside a living guest).
bool Match::freeSpot(int level,Point3& world)const{
 const World* w=worldOf(level);if(!w)return false;const Vec2 o=origin(level);const float px=world.x-o.x,py=world.y-o.y;
 if(w->fits(px,py,world.z,Player::StandingHeight,false))return true;
 for(float r=.25f;r<=2.f;r+=.25f)for(int k=0;k<16;++k){const float a=float(k)*kPi/8,x=px+std::cos(a)*r,y=py+std::sin(a)*r;
  const float f=w->hasTerrain()?w->supportBelow(x,y,world.z+1.f):w->floorHeight(x,y);
  if(!std::isfinite(f)||std::fabs(f-world.z)>.6f||!w->fits(x,y,f,Player::StandingHeight,false))continue;
  world={x+o.x,y+o.y,f};return true;}
 return false;
}
std::string Match::consoleTeleport(const std::string& who,const std::string& to){
 if(!m_host)return "ONLY THE HOST CAN MOVE PLAYERS.";
 auto lower=[](std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;};
 auto find=[&](const std::string& n)->int{if(n.empty()||n=="me")return m_selfSlot;int hit=-1;
  for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(!s.used||lower(s.name).rfind(lower(n),0)!=0)continue;if(hit>=0)return -2;hit=i;}return hit;};
 const int a=find(who),b=find(to);
 if(a==-2||b==-2)return "MORE THAN ONE PLAYER'S NAME STARTS THAT WAY. TYPE MORE OF IT.";
 if(a<0||b<0)return "NO PLAYER IN THIS GAME HAS THAT NAME.";
 if(a==b)return "THAT IS THE SAME PLAYER.";
 const Game& g=m_game;int level;Point3 at;float facing;
 if(b==m_selfSlot){const Vec2 o=origin(g.m_level);level=g.m_level;at={g.m_player.pos.x+o.x,g.m_player.pos.y+o.y,g.m_player.z};facing=g.m_player.angle;}
 else{const auto& t=m_slots[size_t(b)];if(!t.haveState)return "THAT PLAYER HAS NOT ARRIVED YET.";level=t.latest.level;at=t.latest.world;facing=t.latest.angle;}
 Point3 spot{at.x+std::cos(facing)*1.f,at.y+std::sin(facing)*1.f,at.z};   // a step in front of them, never inside them
 if(!freeSpot(level,spot)){spot=at;freeSpot(level,spot);}
 placePlayer(a,level,spot);
 return "MOVED "+m_slots[size_t(a)].name+" TO "+m_slots[size_t(b)].name+".";
}
Match::Spawn Match::besideHost(int slot){
 int anchor=m_selfSlot;
 if(slot==m_selfSlot){anchor=-1;for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(i!=slot&&s.used&&s.alive&&s.spawned&&s.haveState){anchor=i;break;}}}
 const NetPlayerState at=anchor>=0?m_slots[size_t(anchor)].latest:m_slots[size_t(slot)].latest;
 for(int k=0;k<8;++k){const float a=float(k+slot)*kPi*.25f;const Point3 c{at.world.x+std::cos(a)*1.2f,at.world.y+std::sin(a)*1.2f,at.world.z};
  const int level=levelAt({c.x,c.y},at.level);const World* w=worldOf(level);if(!w)continue;const Vec2 o=origin(level);
  const Vec2 from{at.world.x-o.x,at.world.y-o.y},to{c.x-o.x,c.y-o.y};
  if(w->fits(to.x,to.y,c.z,Player::StandingHeight,false)&&std::fabs(w->floorHeight(to.x,to.y)-c.z)<.4f&&w->rayClear(from,at.world.z+.9f,to,c.z+.9f,true,false))return {level,c,at.angle};}
 return {at.level,at.world,at.angle};
}
// The map's SPAWN records, else open floor of the match's map, spread out: the cell nearest its start, then
// each next the farthest from those chosen.
void Match::buildSpawns(){
 m_spawns.clear();if(!m_campaign)return;
 const int level=m_mode==Mode::Deathmatch?m_mapLevel:std::clamp(m_campaign->startMap,0,int(m_campaign->maps.size())-1);
 for(const auto& sp:m_campaign->multiplayer.spawns){if(sp.map!=level)continue;const World* w=worldOf(sp.map);if(!w)continue;
  float z=w->supportBelow(sp.x,sp.y,sp.z+1.f);if(!std::isfinite(z)||z<sp.z-2.f)z=w->floorHeight(sp.x,sp.y);
  const Vec2 o=origin(sp.map);Point3 at{sp.x+o.x,sp.y+o.y,z};freeSpot(sp.map,at);   // an authored spawn in a wall moves to the open floor beside it
  m_spawns.push_back({sp.map,at,sp.yaw*kPi/180.f});}
 if(!m_spawns.empty())return;
 const World* w=worldOf(level);if(!w)return;
 std::vector<Point3> cells;
 for(int y=1;y<World::Height-1;++y)for(int x=1;x<World::Width-1;++x){const float fx=x+.5f,fy=y+.5f;
  const float f=w->hasTerrain()?w->supportBelow(fx,fy,float(World::TerrainMaxZ+1)):w->floorHeight(fx,fy);if(!std::isfinite(f))continue;
  bool open=w->fits(fx,fy,f,Player::StandingHeight,false);
  for(int k=0;k<8&&open;++k){const float a=float(k)*kPi*.25f,nx=fx+std::cos(a)*.5f,ny=fy+std::sin(a)*.5f;open=w->fits(nx,ny,f,Player::StandingHeight,false)&&std::fabs(w->floorHeight(nx,ny)-f)<.3f;}
  if(open)cells.push_back({fx,fy,f});}
 if(cells.empty())return;
 const Vec2 start=w->definition().playerStart;std::vector<Point3> chosen;
 size_t first=0;float nearest=1e9f;for(size_t i=0;i<cells.size();++i){const float d=std::hypot(cells[i].x-start.x,cells[i].y-start.y);if(d<nearest){nearest=d;first=i;}}
 chosen.push_back(cells[first]);
 while(chosen.size()<std::min<size_t>(12,cells.size())){size_t pick=0;float far=-1;
  for(size_t i=0;i<cells.size();++i){float d=1e9f;for(const auto& c:chosen)d=std::min(d,distance2(cells[i],c));if(d>far){far=d;pick=i;}}
  if(far<2.f)break;chosen.push_back(cells[pick]);}
 const Vec2 o=origin(level);
 for(const auto& c:chosen)m_spawns.push_back({level,{c.x+o.x,c.y+o.y,c.z},std::atan2(start.y-c.y,start.x-c.x)});
}

void Match::roundOver(){
 m_over=true;m_restartIn=RestartAfter;int best=-1;
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(!s.used)continue;
  if(best<0||s.frags>m_slots[size_t(best)].frags||(s.frags==m_slots[size_t(best)].frags&&s.deaths<m_slots[size_t(best)].deaths))best=i;}
 m_winner=best>=0?m_slots[size_t(best)].name:std::string();
 wire::Writer w;w.u8(RoundOver).str(m_winner);broadcast(Events,w,true);m_rosterDirty=true;
}
void Match::newRound(){
 m_over=false;m_round=(m_round+1)&255;m_roundClock=0;m_timeLeft=m_timeLimit>0?m_timeLimit:-1.f;m_winner.clear();m_feed.clear();m_restartIn=-1;
 for(auto& s:m_slots)if(s.used){s.frags=s.deaths=0;}
 {wire::Writer w;w.u8(RoundStart).u8(unsigned(m_round));broadcast(Events,w,true);}
 for(int i=0;i<MaxSlots;++i)if(m_slots[size_t(i)].used&&m_slots[size_t(i)].ready)respawn(i);
 m_rosterDirty=true;
}

void Match::clearCreatures(){Game& g=m_game;g.m_enemies.clear();for(auto& c:g.m_chunks)c.enemies.clear();}

// Co-op: the host's creatures on its map (alive, or dead and still lying there), fifty to a packet.
// Co-op: each map's creatures (alive, or dead and still lying there), fifty to a packet, to the guests on that map
// or a map stitched beside it (the host runs them all: Game::updateForeignChunks).
void Match::sendCreatures(){
 const Game& g=m_game;
 for(int level=0;level<g.chunkCount();++level){
  std::vector<int> to;for(int i=0;i<MaxSlots;++i)if(i!=m_selfSlot&&seesLevel(i,level))to.push_back(i);
  if(to.empty()||!g.chunkResident(level))continue;
  const auto& list=g.chunkEnemies(level);std::vector<int> alive;
  for(size_t i=0;i<list.size()&&i<1024;++i)if(list[i].alive||list[i].deathTime<Enemy::CorpseLifetime+1)alive.push_back(int(i));
  size_t at=0;
  do{wire::Writer w;w.u8(Creatures).u32(millis(m_clock)).u8(unsigned(level)).u16(unsigned(std::min<size_t>(list.size(),1024)));const size_t countAt=w.b.size();w.u16(0);unsigned n=0;
  for(;at<alive.size()&&n<50;++at,++n){const auto& e=list[size_t(alive[at])];const auto& t=creatureTraits(e.kind);
   w.u16(unsigned(alive[at])).u8(unsigned(e.kind)).q16(e.pos.x,256).q16(e.pos.y,256).q16(e.z,256).u8(unsigned(std::lround((e.heading/kTwoPi-std::floor(e.heading/kTwoPi))*256))&255u)
    .u8((e.alive?1u:0u)|(e.moving?2u:0u)|(unsigned(std::clamp(e.surfaceMode,0,3))<<2)).u8(unsigned(std::lround(std::fmod(std::fabs(e.gait),kTwoPi)/kTwoPi*255)))
    .unit8(t.windup>0?e.windup/t.windup:0).unit8(e.strike).unit8(e.painFlash).u16(unsigned(std::clamp(int(std::ceil(e.hp)),0,65535)))
    .u8(unsigned(std::clamp(int(e.deathTime*10),0,255))).u8(unsigned(e.stalkMode)).u8(unsigned(std::clamp(int(e.stalkTimer*50),0,255)));}
  w.b[countAt]=std::uint8_t(n&255);w.b[countAt+1]=std::uint8_t(n>>8);for(int i:to)send(m_slots[size_t(i)].peer,State,w,false);
  }while(at<alive.size());
 }
}
void Match::sendDoors(){
 const Game& g=m_game;
 for(int level=0;level<g.chunkCount();++level){
  if(!g.chunkResident(level))continue;const auto& doors=g.chunkWorld(level).doors();if(doors.empty())continue;
  std::vector<int> to;for(int i=0;i<MaxSlots;++i)if(i!=m_selfSlot&&seesLevel(i,level))to.push_back(i);if(to.empty())continue;
  wire::Writer w;w.u8(Doors).u8(unsigned(level));const unsigned n=unsigned(std::min<size_t>(doors.size(),255));w.u8(n);
  for(unsigned i=0;i<n;++i)w.unit8(doors[i].open).u8(doors[i].opening?1:0);
  for(int i:to)send(m_slots[size_t(i)].peer,State,w,false);
 }
}
// Pickups that went this frame, sent on (the host to everyone, a guest to the host).
void Match::trackPickups(){
 Game& g=m_game;m_pickupSeen.resize(size_t(g.chunkCount()));
 for(int level=0;level<g.chunkCount();++level){if(!g.chunkResident(level))continue;
  const auto& list=g.chunkPickups(level);auto& seen=m_pickupSeen[size_t(level)];
  if(seen.size()!=list.size()){seen.resize(list.size());for(size_t i=0;i<list.size();++i)seen[i]=list[i].active;continue;}
  for(size_t i=0;i<list.size();++i){if(seen[i]&&!list[i].active){wire::Writer w;w.u8(PickupTaken).u8(unsigned(level)).u16(unsigned(i));broadcast(Events,w,true);if(m_host&&m_mode==Mode::Deathmatch)markPickupGone(level,int(i));}seen[i]=list[i].active;}
 }
}
// Deathmatch: a pickup taken is back after PickupRespawn seconds, on the host's word (PickupBack to everyone).
void Match::markPickupGone(int level,int index){
 for(const auto& g:m_pickupGone)if(g.level==level&&g.index==index)return;
 m_pickupGone.push_back({level,index,0.f});
}
void Match::pickupRespawns(float dt){
 Game& g=m_game;m_pickupSeen.resize(size_t(g.chunkCount()));
 for(size_t k=0;k<m_pickupGone.size();){
  auto& gone=m_pickupGone[k];gone.age+=dt;
  if(gone.age<PickupRespawn){++k;continue;}
  if(gone.level>=0&&gone.level<g.chunkCount()&&g.chunkResident(gone.level)){
   auto& list=gone.level==g.m_level?g.m_pickups:g.m_chunks[size_t(gone.level)].pickups;
   if(gone.index>=0&&size_t(gone.index)<list.size()){list[size_t(gone.index)].active=true;auto& seen=m_pickupSeen[size_t(gone.level)];if(size_t(gone.index)<seen.size())seen[size_t(gone.index)]=1;
    wire::Writer w;w.u8(PickupBack).u8(unsigned(gone.level)).u16(unsigned(gone.index));broadcast(Events,w,true);}
  }
  m_pickupGone.erase(m_pickupGone.begin()+std::ptrdiff_t(k));
 }
}
// A guest's own hand on a door: the host is asked to do the same.
void Match::trackDoors(float){
 const auto& doors=m_game.m_world.doors();
 if(m_doorLevel!=m_game.m_level||m_doorSeen.size()!=doors.size()){m_doorLevel=m_game.m_level;m_doorSeen.resize(doors.size());m_doorTouched.assign(doors.size(),-99.f);for(size_t i=0;i<doors.size();++i)m_doorSeen[i]=doors[i].opening;return;}
 for(size_t i=0;i<doors.size();++i)if(bool(m_doorSeen[i])!=doors[i].opening){
  m_doorSeen[i]=doors[i].opening;m_doorTouched[i]=m_clock;
  wire::Writer w;w.u8(DoorUse).u8(unsigned(m_game.m_level)).u16(unsigned(i)).u8(doors[i].opening?1:0);send(m_hostId,Events,w,true);}
}
void Match::smoothCreatures(float dt){
 Game& g=m_game;if(m_creatureLevel!=g.m_level)return;const float k=std::min(1.f,dt*12.f);
 for(size_t i=0;i<m_creatureTargets.size()&&i<g.m_enemies.size();++i){const auto& t=m_creatureTargets[i];if(!t.set)continue;auto& e=g.m_enemies[i];
  e.pos=e.pos+(t.pos-e.pos)*k;e.z+=(t.z-e.z)*k;e.heading+=std::remainder(t.heading-e.heading,kTwoPi)*k;}
}

// ----- The guest -----
void Match::guestFrame(float dt){
 if((m_stateTimer-=dt)<=0){m_stateTimer=std::max(0.f,m_stateTimer+GuestInterval);sendGuestState();}
 if(m_selfRespawnIn>0)m_selfRespawnIn=std::max(0.f,m_selfRespawnIn-dt);
 if(m_over&&m_restartIn>0)m_restartIn=std::max(0.f,m_restartIn-dt);
 if(m_mode==Mode::Coop){trackPickups();trackDoors(dt);smoothCreatures(dt);coopGuestFrame(dt);}else{clearCreatures();trackPickups();}
 guestProps(dt);   // the host's props, posed; this hand's carry and throws, sent (MatchProps.cpp)
}
void Match::sendGuestState(){
 const NetPlayerState s=localState();
 wire::Writer w;w.u8(GuestState).u16(++m_stateSeq).u32(m_lastSnapshotMs).u16(unsigned(std::clamp(int((m_clock-m_lastSnapshotLocal)*1000),0,65535)));
 writePlayerState(w,s,origin(s.level),true);send(m_hostId,State,w,false);
}

bool Match::localShot(Point3 o,Point3 direction,int weapon){
 Game& g=m_game;if(m_phase!=Phase::Playing)return true;
 ++m_shotCounter;
 if(m_host){resolveShot(m_selfSlot,weapon,g.m_level,o,direction,m_clock);return true;}
 wire::Writer w;w.u8(Shot).u8(unsigned(std::clamp(weapon,0,255))).u8(unsigned(g.m_level)).f32(o.x).f32(o.y).f32(o.z).q16(direction.x,32767).q16(direction.y,32767).q16(direction.z,32767)
  .u32((std::uint32_t(m_stateSeq)<<16)^m_shotCounter).u32(millis(renderTime()));
 send(m_hostId,Events,w,true);
 // Guns: the tracer at once; the mark it leaves comes back from the host. Fists: no tracer.
 if(weapon>=1&&weapon<=3){const auto& gun=playerWeaponDef(WeaponId(weapon));const auto hit=g.traceShot(o,direction,gun.hitscan.maxRange);
  g.m_shotRays.push_back({o,direction,hit.kind==ShotKind::None?gun.hitscan.maxRange:hit.distance,g.m_elapsed,0,0,g.m_level});if(g.m_shotRays.size()>24)g.m_shotRays.erase(g.m_shotRays.begin());}
 return true;
}

void Match::handleState(const Packet& p){
 Game& g=m_game;wire::Reader r(p.data);const unsigned type=r.u8();
 if(m_phase!=Phase::Playing)return;
 if(m_host){
  const int sender=slotOf(p.from);if(sender>=0&&m_slots[size_t(sender)].ready&&handlePropState(sender,type,r))return;   // a guest's carry (MatchProps.cpp)
  if(type!=GuestState)return;const int slot=sender;if(slot<0||!m_slots[size_t(slot)].ready)return;
  const unsigned seq=r.u16();const std::uint32_t echo=r.u32();const unsigned hold=r.u16();NetPlayerState s;if(!readPlayerState(r,s,true))return;
  auto& sl=m_slots[size_t(slot)];if(sl.seqSeen&&std::int16_t(std::uint16_t(seq-sl.seq))<=0)return;sl.seq=std::uint16_t(seq);sl.seqSeen=true;
  if(echo){const float rtt=m_clock*1000.f-float(echo)-float(hold);if(rtt>=0&&rtt<5000)sl.pingMs=sl.pingMs<0?int(rtt):int(float(sl.pingMs)*.8f+rtt*.2f);}
  if(s.level>=g.chunkCount())return;const Vec2 o=origin(s.level);s.world.x+=o.x;s.world.y+=o.y;
  acceptGuestState(slot,s);return;
 }
 if(p.from!=m_hostId)return;
 if(handleCoopState(type,r))return;   // the people (MatchCoop.cpp)
 if(handlePropState(-1,type,r))return;   // the props (MatchProps.cpp)
 if(type==Snapshot){
  const std::uint32_t ms=r.u32();const int round=int(r.u8());const int left=r.i16();const bool over=r.u8()!=0;const int count=int(r.u8());if(r.bad)return;
  if(m_haveSnapshot&&std::int32_t(ms-m_lastSnapshotMs)<0)return;   // an older one, overtaken
  const float t=float(ms)/1000.f,sample=t-m_clock;
  // The host's clock: the least delayed snapshot sets it, and it eases down if the delay grows.
  if(!m_haveOffset||sample>m_hostOffset)m_hostOffset=sample;else m_hostOffset+=(sample-m_hostOffset)*.02f;
  m_haveOffset=true;m_lastSnapshotMs=ms;m_lastSnapshotLocal=m_clock;m_haveSnapshot=true;
  m_timeLeft=left<0?-1.f:float(left)/10.f;
  // The round's end and start come as events; a snapshot only fills in one that is late (never one from a round gone by).
  if(round==m_round){if(over&&!m_over){m_over=true;if(m_restartIn<0)m_restartIn=RestartAfter;}}
  else if(!over&&round==((m_round+1)&255)){m_round=round;m_over=false;m_winner.clear();m_restartIn=-1;}
  for(int k=0;k<count;++k){const int slot=int(r.u8());NetPlayerState s;if(!readPlayerState(r,s,true)||slot>=MaxSlots||s.level>=g.chunkCount())break;
   const Vec2 o=origin(s.level);s.world.x+=o.x;s.world.y+=o.y;if(slot==m_selfSlot)continue;
   auto& sl=m_slots[size_t(slot)];
   if(sl.haveState&&s.shots!=sl.lastShots){sl.lastShotAt=t;}sl.lastShots=s.shots;
   if(!s.alive&&sl.deathAt<0)sl.deathAt=t;if(s.alive)sl.deathAt=-1;
   sl.latest=s;sl.haveState=true;sl.health=s.health;sl.alive=s.alive;sl.spawned=true;
   if(sl.history.empty()||sl.history.back().t<t)sl.history.push_back({t,s});
   while(sl.history.size()>2&&sl.history.front().t<t-HistorySeconds)sl.history.pop_front();}
 }else if(type==Creatures&&m_mode==Mode::Coop){
  const std::uint32_t ms=r.u32();const int level=int(r.u8());const int total=int(r.u16());const int count=int(r.u16());if(r.bad||level>=g.chunkCount()||!g.chunkResident(level))return;
  if(std::int32_t(ms-m_lastCreaturesMs)<0)return;m_lastCreaturesMs=ms;
  auto& list=level==g.m_level?g.m_enemies:g.m_chunks[size_t(level)].enemies;
  if(list.size()>size_t(total))list.resize(size_t(total));   // none here the host has not (one this guest's crossing carried along)
  const bool here=level==g.m_level;if(here&&m_creatureLevel!=level){m_creatureTargets.clear();m_creatureLevel=level;}
  for(int k=0;k<count;++k){
   const int index=int(r.u16());const auto kind=CreatureKind(r.u8());Vec2 pos;pos.x=r.q16(256);pos.y=r.q16(256);const float z=r.q16(256);const float heading=float(r.u8())/256.f*kTwoPi;
   const unsigned flags=r.u8();const float gait=float(r.u8())/255.f*kTwoPi;const float windup=r.unit8(),strike=r.unit8(),pain=r.unit8();const float hp=float(r.u16());
   const float deathTime=float(r.u8())/10.f;const int stalk=int(r.u8());const float stalkTimer=float(r.u8())/50.f;
   if(r.bad||index>=1024||int(kind)>=CreatureKindCount)break;
   if(size_t(index)>=list.size())list.resize(size_t(index)+1);
   auto& e=list[size_t(index)];const auto& t=creatureTraits(kind);
   if(e.kind!=kind||(e.visualClip<0&&e.home.x==0&&e.home.y==0)){e=Enemy{};e.kind=kind;e.hp=e.maxHp=std::max(hp,1.f);e.pos=e.home=pos;e.z=z;e.heading=heading;}
   const bool wasAlive=e.alive;
   if(here){if(m_creatureTargets.size()<=size_t(index))m_creatureTargets.resize(size_t(index)+1);m_creatureTargets[size_t(index)]={true,pos,z,heading};
    if(std::hypot(e.pos.x-pos.x,e.pos.y-pos.y)>3.f){e.pos=pos;e.z=z;e.heading=heading;}}
   else{e.pos=pos;e.z=z;e.heading=heading;}
   e.alive=flags&1;e.moving=flags&2;e.surfaceMode=int(flags>>2&3);e.gait=gait;e.windup=windup*t.windup;e.strike=strike;e.painFlash=pain;e.hp=hp;e.maxHp=std::max(e.maxHp,hp);
   e.stalkMode=Enemy::StalkMode(std::clamp(stalk,0,int(Enemy::StalkMode::Recover)));e.stalkTimer=stalkTimer;
   if(!e.alive){if(wasAlive){++e.deathSerial;e.deathSurface=e.surfaceMode;e.deathTime=deathTime;}else if(std::fabs(e.deathTime-deathTime)>.5f&&deathTime<25.f)e.deathTime=deathTime;}
  }
 }else if(type==Doors&&m_mode==Mode::Coop){
  const int level=int(r.u8());const int count=int(r.u8());if(r.bad||level>=g.chunkCount()||!g.chunkResident(level))return;
  World& w=level==g.m_level?g.m_world:g.m_chunks[size_t(level)].world;
  for(int i=0;i<count;++i){const float open=r.unit8();const bool opening=r.u8()!=0;if(r.bad||size_t(i)>=w.doors().size())break;
   if(level==g.m_level&&size_t(i)<m_doorTouched.size()&&m_clock-m_doorTouched[size_t(i)]<.6f)continue;   // the guest's own hand, still on its way
   const auto& d=w.doors()[size_t(i)];
   if(d.opening!=opening)w.toggleDoor(i);
   if(std::fabs(d.open-open)>.35f)w.setDoor(i,open,opening);
   if(level==g.m_level&&size_t(i)<m_doorSeen.size())m_doorSeen[size_t(i)]=opening;}
 }
}

void Match::handleEvent(const Packet& p){
 Game& g=m_game;wire::Reader r(p.data);const unsigned type=r.u8();
 if(m_phase!=Phase::Playing)return;
 auto takePickup=[&](int level,int index){if(level<0||level>=g.chunkCount()||!g.chunkResident(level))return false;
  auto& list=level==g.m_level?g.m_pickups:g.m_chunks[size_t(level)].pickups;if(index<0||size_t(index)>=list.size())return false;
  list[size_t(index)].active=false;m_pickupSeen.resize(size_t(g.chunkCount()));auto& seen=m_pickupSeen[size_t(level)];if(size_t(index)<seen.size())seen[size_t(index)]=0;
  if(m_host&&m_mode==Mode::Deathmatch)markPickupGone(level,index);return true;};
 if(m_host){
  const int slot=slotOf(p.from);if(slot<0)return;
  if(handleCoopEvent(slot,type,r))return;   // a guest's hand on a terminal (MatchCoop.cpp)
  if(handlePropEvent(slot,type,r))return;   // a guest's hand, fist or throw on a prop (MatchProps.cpp)
  if(type==Shot){
   const int weapon=int(r.u8());const int level=int(r.u8());Point3 o;o.x=r.f32();o.y=r.f32();o.z=r.f32();const Point3 d=readVec(r,32767);r.u32();const std::uint32_t view=r.u32();
   if(r.bad||level>=g.chunkCount())return;resolveShot(slot,weapon,level,o,d,float(view)/1000.f);
  }else if(type==PickupTaken){
   const int level=int(r.u8());const int index=int(r.u16());
   if(!r.bad&&takePickup(level,index)){wire::Writer w;w.u8(PickupTaken).u8(unsigned(level)).u16(unsigned(index));broadcast(Events,w,true,slot);}
  }else if(type==DoorUse&&m_mode==Mode::Coop){
   const int level=int(r.u8());const int index=int(r.u16());const bool opening=r.u8()!=0;
   if(r.bad||level>=g.chunkCount()||!g.chunkResident(level))return;World& w=level==g.m_level?g.m_world:g.m_chunks[size_t(level)].world;
   if(size_t(index)<w.doors().size()&&w.doors()[size_t(index)].opening!=opening)w.toggleDoor(index);
  }
  return;
 }
 if(p.from!=m_hostId)return;
 switch(type){
 case ShotFx:{
  const int shooter=int(r.u8());const int weapon=int(r.u8());const int level=int(r.u8());Point3 o,e;o.x=r.f32();o.y=r.f32();o.z=r.f32();e.x=r.f32();e.y=r.f32();e.z=r.f32();
  const int kind=int(r.u8());const Point3 n=readUnit(r);if(r.bad||level>=g.chunkCount()||!g.inPlayerFrame(level))return;   // (a shot on another map is not seen here)
  const Vec2 shift=origin(level)-origin(g.m_level);o.x+=shift.x;o.y+=shift.y;e.x+=shift.x;e.y+=shift.y;
  Point3 d=e-o;const float range=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(range<.001f)return;d=d*(1/range);
  if(shooter!=m_selfSlot){
   if(weapon!=0){g.m_shotRays.push_back({o,d,range,g.m_elapsed,0,0,g.m_level});if(g.m_shotRays.size()>24)g.m_shotRays.erase(g.m_shotRays.begin());
    g.m_sounds.push_back({playerWeaponDef(WeaponId(std::clamp(weapon,1,3))).cycle.fireSound,{o.x,o.y},.9f,1,true,17,o.z});}
   else g.sound(Sound::PunchSwing,.35f);}
  if(kind==2)return;
  const Vec2 aw=origin(g.m_level);const int at=levelAt({e.x+aw.x,e.y+aw.y},level);const World* w=worldOf(at);if(!w)return;
  const Vec2 back=origin(g.m_level)-origin(at);const Point3 local{e.x+back.x,e.y+back.y,e.z};
  if(kind==1){g.hitBurst(at,local,d,ImpactMaterial::Flesh,-1,weapon==0?.45f:.9f,false);return;}
  const Point3 struck=local+n*.004f;const auto material=w->impactMaterial(struck,n);
  g.m_bulletImpacts.push_back({{struck.x,struck.y},struck.z,{n.x,n.y},g.m_elapsed,at,false,0,n.z,int(material)});if(g.m_bulletImpacts.size()>96)g.m_bulletImpacts.erase(g.m_bulletImpacts.begin());
  if(material!=ImpactMaterial::Metal)g.hitBurst(at,struck,n,material,-1,.8f,false);
  break;}
 case Hit:{
  const int victim=int(r.u8());const int attacker=int(r.u8());r.u8();const float after=float(r.u8());if(r.bad||victim>=MaxSlots)return;
  m_slots[size_t(victim)].health=after;
  if(victim==m_selfSlot&&!g.dead()){g.m_player.health=after;g.m_damageFlash=1;g.sound(Sound::Hurt,.7f,.92f);}
  if(attacker==m_selfSlot&&victim!=m_selfSlot)g.m_hitFlash=1;
  break;}
 case Death:{
  const int victim=int(r.u8());int killer=int(r.u8());const int weapon=int(r.u8());const float respawnIn=float(r.u8())/10.f;if(r.bad||victim>=MaxSlots)return;
  if(killer==255)killer=-1;pushFeed(killer,victim,weapon);
  auto& v=m_slots[size_t(victim)];v.alive=false;v.health=0;if(v.deathAt<0)v.deathAt=hostNow();
  if(victim==m_selfSlot){g.m_player.health=0;m_selfRespawnIn=respawnIn;}
  break;}
 case Respawn:{
  const int slot=int(r.u8());const std::uint8_t life=std::uint8_t(r.u8());const int level=int(r.u8());Point3 at;at.x=r.f32();at.y=r.f32();at.z=r.f32();const float yaw=r.angle();
  if(r.bad||slot>=MaxSlots||level>=g.chunkCount())return;const Vec2 o=origin(level);const Point3 world{at.x+o.x,at.y+o.y,at.z};
  auto& s=m_slots[size_t(slot)];s.alive=true;s.health=100;s.deathAt=-1;
  if(slot==m_selfSlot){m_lifeLocal=life;applySpawn(level,world,yaw);}
  else{s.history.clear();s.latest.world=world;s.latest.level=level;s.latest.alive=true;}
  break;}
 case Correct:{
  const std::uint8_t life=std::uint8_t(r.u8());const int level=int(r.u8());Point3 at;at.x=r.f32();at.y=r.f32();at.z=r.f32();if(r.bad||level>=g.chunkCount())return;
  const Vec2 o=origin(level);m_lifeLocal=life;teleportLocal(level,{at.x+o.x,at.y+o.y,at.z});break;}
 case PickupTaken:{const int level=int(r.u8());const int index=int(r.u16());if(!r.bad)takePickup(level,index);break;}
 case PickupBack:{const int level=int(r.u8());const int index=int(r.u16());   // (deathmatch) a taken pickup is lying there again
  if(!r.bad&&level>=0&&level<g.chunkCount()&&g.chunkResident(level)){auto& list=level==g.m_level?g.m_pickups:g.m_chunks[size_t(level)].pickups;
   if(index>=0&&size_t(index)<list.size()){list[size_t(index)].active=true;m_pickupSeen.resize(size_t(g.chunkCount()));auto& seen=m_pickupSeen[size_t(level)];if(size_t(index)<seen.size())seen[size_t(index)]=1;}}
  break;}
 case PickupsInit:{const int count=int(r.u16());for(int k=0;k<count&&!r.bad;++k){const int level=int(r.u8());const int index=int(r.u16());if(!r.bad)takePickup(level,index);}break;}
 case Travel:{
  const int level=int(r.u8());Point3 at;at.x=r.f32();at.y=r.f32();at.z=r.f32();const float angle=r.angle();const int left=int(r.u8());const bool follow=r.u8()!=0;
  if(r.bad||level>=g.chunkCount()||m_mode!=Mode::Coop)return;
  // Every map a player is on runs, so a guest stays where it is. With follow on the host itself places the guests
  // who were on the map it left beside it (a correction: a guest that moved itself would be sent back as too fast).
  (void)at;(void)angle;(void)left;(void)follow;
  break;}
 case RoundOver:{m_over=true;m_winner=r.str();m_restartIn=RestartAfter;break;}
 case RoundStart:{m_round=int(r.u8());m_over=false;m_winner.clear();m_feed.clear();m_restartIn=-1;break;}
 default:if(!handlePropEvent(-1,type,r))handleCoopEvent(-1,type,r);break;   // props (MatchProps.cpp); script effects, the state store (MatchCoop.cpp)
 }
}

// ----- What the presentation reads -----
void Match::publish(float dt){
 Game& g=m_game;auto& out=g.m_remotePlayers;out.clear();
 for(auto& f:m_feed)f.age+=dt;
 m_feed.erase(std::remove_if(m_feed.begin(),m_feed.end(),[](const FeedLine& f){return f.age>FeedSeconds;}),m_feed.end());
 if(m_noticeTime>0)m_noticeTime-=dt;
 if(m_phase==Phase::Playing){
  const float t=renderTime();const Vec2 here=origin(g.m_level);
  for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(i==m_selfSlot||!s.used||!s.haveState||!s.spawned)continue;
   NetPlayerState st;if(!sampleAt(s,t,st))continue;
   // Only someone on this map, or on a stitched neighbour of it (its own place on the grid): a player left on
   // another map of a custom campaign would otherwise be drawn at their coordinates there, inside these walls.
   if(!g.inPlayerFrame(st.level))continue;
   RemotePlayer rp;rp.slot=std::uint8_t(i);rp.name=s.name;rp.position={st.world.x-here.x,st.world.y-here.y,st.world.z};
   rp.angle=st.angle;rp.pitch=st.pitch;rp.velocity=st.velocity;rp.crouched=st.crouched;rp.prone=st.prone;rp.grounded=st.grounded;rp.weapon=st.weapon;
   rp.shotAge=std::max(0.f,t-s.lastShotAt);rp.reloadAge=st.reloadAge;rp.health=st.health;rp.alive=st.alive;
   rp.deathAge=!st.alive&&s.deathAt>=0?std::max(0.f,t-s.deathAt):0.f;
   rp.vr=st.vr;if(st.vr){rp.pose=st.pose;const Point3 feet=rp.position;rp.pose.head=rp.pose.head+feet;rp.pose.root=rp.pose.root+feet;for(auto& h:rp.pose.hands)h.wrist=h.wrist+feet;}
   rp.team=m_mode==Mode::Coop?1:0;rp.character=s.character;rp.pingMs=s.pingMs;
   out.push_back(std::move(rp));}
 }
 auto& v=g.m_matchView;const LobbyInfo& lobby=m_session->lobby();
 v.active=m_phase==Phase::Playing;v.mode=m_mode;v.host=m_host;v.map=m_mapName.empty()?m_mapFile:m_mapName;v.joinCode=lobby.joinCode;v.lobbyName=lobby.name;
 v.fragLimit=m_mode==Mode::Deathmatch?m_fragLimit:0;v.timeLeft=m_mode==Mode::Deathmatch?m_timeLeft:-1.f;
 v.scores.clear();
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(!s.used)continue;ScoreRow row;row.slot=std::uint8_t(i);row.name=s.name;row.frags=s.frags;row.deaths=s.deaths;
  row.pingMs=i==m_selfSlot&&m_host?0:s.pingMs;row.host=s.host||(m_host&&i==m_selfSlot);row.self=i==m_selfSlot;v.scores.push_back(row);}
 std::stable_sort(v.scores.begin(),v.scores.end(),[](const ScoreRow& a,const ScoreRow& b){return a.frags!=b.frags?a.frags>b.frags:a.deaths<b.deaths;});
 v.feed=m_feed;v.over=m_over;v.winner=m_winner;
 v.respawnIn=-1;
 if(m_phase==Phase::Playing&&m_selfSlot>=0&&g.dead())v.respawnIn=m_host?std::max(0.f,m_slots[size_t(m_selfSlot)].respawnIn):std::max(0.f,m_selfRespawnIn);
 auto seconds=[](float s){return std::to_string(int(std::ceil(std::max(0.f,s))));};
 if(m_phase==Phase::Joining)v.status="Joining "+(lobby.name.empty()?std::string("the game"):lobby.name);
 else if(m_phase==Phase::Downloading)v.status="Waiting for the map ("+std::to_string(m_downloadSize?int(m_downloadHave*100/m_downloadSize):0)+"%)";
 else if(m_over)v.status=(m_winner.empty()?std::string("Round over"):m_winner+" wins")+". Next round in "+seconds(m_restartIn);
 else if(v.respawnIn>=0)v.status=m_mode==Mode::Coop?"Down. Back beside the host in "+seconds(v.respawnIn):"Respawning in "+seconds(v.respawnIn);
 else if(m_noticeTime>0)v.status=m_notice;
 else v.status.clear();
}

}
