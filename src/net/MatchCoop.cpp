#include "Match.h"
#include "MatchProtocol.h"
#include "Wire.h"
#include "../game/Game.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
// Co-op's one world (docs/Systems/multiplayer.md "One path"). The game's minds read Game::players(); on a co-op
// host that list holds every guest's body as well as its own player, kept here each frame from what the guests
// report (their feet, stance, torch, locker, health as the host knows it), with their footsteps made into sounds
// the minds hear and their light measured as the local player's is. What the minds then do reaches the guests:
//  - a blow a creature or a guard lands on a guest (Game::m_netHurts) is taken off that guest here (damage);
//  - script effects (Game::m_scriptFx): a sound, a shake, a caption, a release, the ending, an item for one guest;
//  - the state store (StateSync): named states, objectives, entity properties; doors already come as Doors;
//  - the people (Actors): guards and civilians think on the host only and are posed on guests from this.
// A guest's hand on a terminal (a switch, a lever, a lift call) comes to the host as Use and is worked here.
namespace retro::net {
using namespace protocol;
namespace {
constexpr float SyncInterval=.2f,FullSyncInterval=10.f,ActorInterval=.1f,ClockResend=1.f;
constexpr int ClockSlack=500;   // ms a guest's own running clock may differ from the host's before it is set
// States that are each player's own, never the world's: their torch switch and the locker they hide in.
constexpr StateId Personal[]={stateId("flashlight_on"),stateId("player_locker")};
// The minds' own working states (actorAiState, the top bit) stay on the host: the people come as Actors.
bool shared(std::uint32_t id){if(id&0x80000000u)return false;for(auto p:Personal)if(p==id)return false;return true;}
// What a guest draws a person by besides the pose (Combat.cpp npcWeaponPose, the cards): sent when not zero.
constexpr const char* ActorFields[]={"crouched","restraint","walking","weapon_raise","weapon_target","weapon_x","weapon_y","weapon_z",
 "weapon_flash","weapon_recoil","weapon_reload","weapon_reloading","weapon_action","weapon_action_ms","weapon_bash","weapon_bash_pose",
 "grenade_windup","guard_mode","guard_recognition","guard_confirmed","guard_seen","guard_hold_ms","guard_haul","guard_target_kind",
 "headless","support_ms","engineer_pad_ms","engineer_pad_stage","drone_busy","drone_windup","drone_repair_target","guard_danger_ms"};
constexpr int ActorFieldCount=int(sizeof(ActorFields)/sizeof(ActorFields[0]));
void varint(wire::Writer& w,int v){std::uint32_t z=(std::uint32_t(v)<<1)^std::uint32_t(v>>31);while(z>=0x80){w.u8((z&0x7Fu)|0x80u);z>>=7;}w.u8(z);}
int readVarint(wire::Reader& r){std::uint32_t z=0;for(int shift=0;shift<35;shift+=7){const unsigned b=r.u8();z|=std::uint32_t(b&0x7Fu)<<shift;if(!(b&0x80u))break;}return int(z>>1)^-int(z&1u);}
void writeAction(wire::Writer& w,const ScriptAction& a){w.u8(unsigned(a.type)).u32(a.id).u32(std::uint32_t(a.value)).u16(unsigned(a.sound)).f32(a.amount).str(a.text);}
ScriptAction readAction(wire::Reader& r){ScriptAction a;a.type=ScriptAction::Type(r.u8());a.id=r.u32();a.value=int(r.u32());a.sound=Sound(r.u16());a.amount=r.f32();a.text=r.str();return a;}
// The states that tick every frame: sequence clocks, people's idle and step counters, the lift, the storm.
void findClocks(const Game& g,std::vector<std::uint32_t>& out){
 for(int level=0;level<g.chunkCount();++level){const World& w=g.chunkWorld(level);
  for(const auto& s:w.timedSequences())out.push_back(s.timerState);
  for(size_t i=0;i<w.actorTracks().size();++i)for(const char* f:{"idle_ms","step_mm","step_take","flinch_ms"})out.push_back(actorTrackState(w.actorTracks()[i].timerState,i,f));}
 out.push_back(g.chunkWorld(g.level()).cargoLift().positionState);out.push_back(stateId("storm_clock"));out.push_back(0);
 std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());
}
std::uint32_t millis(float seconds){return std::uint32_t(std::max(0.f,seconds)*1000.f);}
using Values=std::vector<std::pair<std::uint32_t,int>>;
// What changed from sent to now (both sorted by id): new and changed values, and 0 for what is gone.
template<class Keep> void diff(const Values& now,const Values& sent,Values& out,Keep keep){
 size_t a=0,b=0;
 while(a<now.size()||b<sent.size()){
  if(b>=sent.size()||(a<now.size()&&now[a].first<sent[b].first)){if(keep(now[a].first,0,now[a].second))out.push_back(now[a]);++a;}
  else if(a>=now.size()||sent[b].first<now[a].first){if(keep(sent[b].first,sent[b].second,0))out.push_back({sent[b].first,0});++b;}
  else{if(now[a].second!=sent[b].second&&keep(now[a].first,sent[b].second,now[a].second))out.push_back(now[a]);++a;++b;}
 }
}
}

bool Match::clockState(std::uint32_t id)const{return std::binary_search(m_clocks.begin(),m_clocks.end(),id);}

void Match::clearCoop(){
 Game& g=m_game;g.m_remoteBodies.clear();g.m_netHurts.clear();g.m_netUses.clear();
 m_sentStates.clear();m_sentObjectives.clear();m_sentProps.clear();m_clockSent.clear();m_clocks.clear();m_actorTargets.clear();
 m_syncTimer=0;m_fullSyncTimer=FullSyncInterval;m_actorTimer=0;m_lastActorsMs=0;
}

// ----- The host -----
void Match::coopHostFrame(float dt){
 Game& g=m_game;
 if(m_clocks.empty())findClocks(g,m_clocks);
 syncBodies(dt);
 // Blows the minds landed on guests this frame: the host takes them off and tells everyone (Hit, Death).
 for(const auto& h:g.m_netHurts)damage(h.id,-1,h.amount,0);
 g.m_netHurts.clear();
 sendScriptFx();
 if((m_syncTimer-=dt)<=0){m_syncTimer=SyncInterval;const bool full=(m_fullSyncTimer-=SyncInterval)<=0;if(full)m_fullSyncTimer=FullSyncInterval;sendStateSync(full,nullptr);}
 if((m_actorTimer-=dt)<=0){m_actorTimer=ActorInterval;sendActors();}
}

// The guests' bodies as the minds see them: where each stands (the host's frame), stance, torch, locker, health.
void Match::syncBodies(float dt){
 Game& g=m_game;auto& bodies=g.m_remoteBodies;
 std::vector<Game::RemoteBody> next;
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(i==m_selfSlot||!s.used||!s.ready||!s.spawned||!s.haveState)continue;
  Game::RemoteBody rb;for(const auto& old:bodies)if(old.id==i)rb=old;
  const auto& st=s.latest;const Vec2 o=origin(g.m_level);auto& b=rb.body;
  rb.id=i;rb.level=st.level;b.pos={st.world.x-o.x,st.world.y-o.y};b.z=st.world.z;b.angle=st.angle;b.pitch=st.pitch*140.f;
  b.prone=st.prone;b.crouched=st.crouched||st.prone;b.grounded=st.grounded;b.eye=b.stanceEye();b.health=s.alive?std::max(s.health,.01f):0.f;
  rb.velocity=st.velocity;rb.hidden=st.hidden;rb.torch=st.torch;rb.pushing=st.pushing;
  if((rb.litTimer-=dt)<=0){rb.litTimer=.1f;rb.lit=rb.hidden?0.f:g.lightAt(b.pos,b.z+b.hullHeight()*.6f,rb.torch);}
  // Their footsteps, heard by the minds (and the host) as the local player's are: a stride of the gait, at the
  // floor's and the gait's reach (Game::stepHeard).
  const float speed=length(rb.velocity);const bool sprint=speed>4.2f&&!b.crouched;
  if(s.alive&&b.grounded&&!rb.hidden&&speed>.2f&&g.inPlayerFrame(rb.level)){rb.stride+=speed*dt;const float stride=b.prone?.6f:b.crouched?.85f:sprint?1.45f:1.22f;
   if(rb.stride>stride){rb.stride=0;const Sound base=Game::stepSoundAt(g.chunkWorld(std::clamp(rb.level,0,std::max(0,g.chunkCount()-1))),b.pos);
    SoundEvent step{base,b.pos,b.crouched?.045f:sprint?.65f:.48f,.97f,true,17,b.z+.04f};step.aiRadius=Game::stepHeard(base,b.crouched,sprint);g.m_sounds.push_back(step);}}
  next.push_back(rb);
 }
 bodies=std::move(next);
}

void Match::sendScriptFx(){
 Game& g=m_game;
 for(const auto& fx:g.m_scriptFx){wire::Writer w;w.u8(ScriptFx);writeAction(w,fx.action);
  if(fx.target<0)broadcast(Events,w,true);
  else if(fx.target!=m_selfSlot&&fx.target<MaxSlots&&m_slots[size_t(fx.target)].used)send(m_slots[size_t(fx.target)].peer,Events,w,true);
  m_coopBytes+=w.b.size();}
 g.m_scriptFx.clear();
}

// The world's state store: what changed since the last (every 0.2 s), the whole of it every 10 s and to a guest
// who joins. A running clock is sent when it starts or stops and once a second between (guests run it too).
void Match::sendStateSync(bool full,const PeerId* to){
 Game& g=m_game;const float now=m_clock;
 Values states,objectives,props;
 for(const auto& s:g.m_states)if(shared(s.id))states.push_back({s.id,s.value});
 for(const auto& o:g.m_objectives)objectives.push_back({o.id,o.value});
 std::sort(objectives.begin(),objectives.end());
 for(const auto& [id,ref]:g.m_entityProperties){(void)ref;props.push_back({id,g.entityPropertyInt(id)});}
 std::sort(props.begin(),props.end());
 Values outStates,outObjectives,outProps;
 if(full){outStates=states;outObjectives=objectives;outProps=props;}
 else{
  diff(states,m_sentStates,outStates,[&](std::uint32_t id,int was,int is){
   if(!clockState(id))return true;
   auto it=std::find_if(m_clockSent.begin(),m_clockSent.end(),[&](const auto& c){return c.first==id;});
   const bool due=(was==0)!=(is==0)||it==m_clockSent.end()||now-it->second>=ClockResend;
   if(due){if(it==m_clockSent.end())m_clockSent.push_back({id,now});else it->second=now;}
   return due;});
  diff(objectives,m_sentObjectives,outObjectives,[](std::uint32_t,int,int){return true;});
  diff(props,m_sentProps,outProps,[](std::uint32_t,int,int){return true;});
  if(outStates.empty()&&outObjectives.empty()&&outProps.empty())return;
 }
 wire::Writer w;w.u8(StateSync).u8(full?1:0);
 w.u16(unsigned(std::min<size_t>(outStates.size(),65535)));for(size_t i=0;i<outStates.size()&&i<65535;++i)w.u32(outStates[i].first).u32(std::uint32_t(outStates[i].second));
 w.u8(unsigned(std::min<size_t>(outObjectives.size(),255)));for(size_t i=0;i<outObjectives.size()&&i<255;++i)w.u32(outObjectives[i].first).u8(unsigned(std::clamp(outObjectives[i].second,0,255)));
 w.u16(unsigned(std::min<size_t>(outProps.size(),65535)));for(size_t i=0;i<outProps.size()&&i<65535;++i)w.u32(outProps[i].first).u32(std::uint32_t(outProps[i].second));
 if(to){send(*to,Events,w,true);m_coopBytes+=w.b.size();return;}
 broadcast(Events,w,true);m_coopBytes+=w.b.size();
 // What the guests now have: everything sent; a clock held back keeps the value they last had.
 if(full){m_sentStates=states;m_sentObjectives=objectives;m_sentProps=props;return;}
 auto merge=[](Values& sent,const Values& out){for(const auto& [id,v]:out){auto it=std::lower_bound(sent.begin(),sent.end(),std::make_pair(id,INT32_MIN));
   if(it!=sent.end()&&it->first==id){if(v)it->second=v;else sent.erase(it);}else if(v)sent.insert(it,{id,v});}};
 merge(m_sentStates,outStates);merge(m_sentObjectives,outObjectives);merge(m_sentProps,outProps);
}

// The people who think (guards, civilians, escorts), as they stand on the host, fifty or so to a packet.
void Match::sendActors(){
 Game& g=m_game;const auto& list=g.m_friendlyActors;std::vector<size_t> who;
 for(size_t i=0;i<list.size()&&i<65535;++i){const auto& npc=list[i];if(npc.summon||!g.autonomousActor(npc.track())||!g.chunkResident(npc.level))continue;who.push_back(i);}
 if(who.empty())return;
 size_t at=0;
 while(at<who.size()){
  wire::Writer w;w.u8(Actors).u32(millis(m_clock));const size_t countAt=w.b.size();w.u8(0);unsigned n=0;
  while(at<who.size()&&w.b.size()<1000&&n<255){const size_t i=who[at++];const auto& npc=list[i];const auto& p=npc.pose;
   w.u16(unsigned(i)).u8(unsigned(std::clamp(npc.homeLevel,0,255))).u8(unsigned(std::clamp(npc.level,0,255))).q16(p.position.x,256).q16(p.position.y,256).q16(p.z,256)
    .u8(unsigned(std::lround((p.yaw/kTwoPi-std::floor(p.yaw/kTwoPi))*256))&255u).u8(unsigned(std::clamp(p.clip,0,255))).unit8(p.phase-std::floor(p.phase));
   const size_t fieldsAt=w.b.size();w.u8(0);unsigned k=0;
   for(int f=0;f<ActorFieldCount;++f){const int v=g.state(g.friendlyKey(i,ActorFields[f]));if(!v)continue;w.u8(unsigned(f));varint(w,v);++k;}
   w.b[fieldsAt]=std::uint8_t(k);++n;}
  w.b[countAt]=std::uint8_t(n);broadcast(State,w,false);m_coopBytes+=w.b.size();
 }
}

// ----- The guest -----
void Match::coopGuestFrame(float dt){
 Game& g=m_game;
 if(m_clocks.empty())findClocks(g,m_clocks);
 // The guest's hands on terminals: the host works them.
 for(const auto& u:g.m_netUses){wire::Writer w;w.u8(Use).u8(unsigned(std::clamp(u.level,0,255))).u16(unsigned(u.terminal));send(m_hostId,Events,w,true);}
 g.m_netUses.clear();
 smoothActors(dt);
}
void Match::smoothActors(float dt){
 Game& g=m_game;const float k=std::min(1.f,dt*12.f);
 for(size_t i=0;i<m_actorTargets.size()&&i<g.m_friendlyActors.size();++i){const auto& t=m_actorTargets[i];if(!t.set)continue;auto& p=g.m_friendlyActors[i].pose;
  p.position=p.position+(t.pos-p.position)*k;p.z+=(t.z-p.z)*k;p.yaw+=std::remainder(t.yaw-p.yaw,kTwoPi)*k;}
}

bool Match::handleCoopEvent(int slot,unsigned type,wire::Reader& r){
 Game& g=m_game;
 if(m_host){
  if(type!=Use||m_mode!=Mode::Coop)return false;
  const int level=int(r.u8());const int terminal=int(r.u16());
  if(!r.bad&&level==g.m_level)g.operateTerminal(terminal,slot);   // worked for them, as their own hand would
  else if(!r.bad&&level>=0&&level<g.chunkCount()&&g.chunkResident(level)){Game::ChunkFrame frame(g,level);g.operateTerminal(terminal,slot);}   // on another map: in its frame
  return true;
 }
 if(type==ScriptFx){
  const ScriptAction a=readAction(r);if(r.bad||int(a.type)>int(ScriptAction::Type::ShowMessage))return true;
  if(a.type==ScriptAction::Type::ShowMessage){g.m_pickupNotice=a.text;g.m_pickupNoticeTime=a.amount;}
  else if(a.type!=ScriptAction::Type::SpawnEnemy&&a.type!=ScriptAction::Type::Checkpoint&&a.type!=ScriptAction::Type::EnterMainCampaign)g.executeScriptAction(a);
  return true;
 }
 if(type==StateSync){
  const bool full=r.u8()!=0;
  std::vector<StateId> named;
  const unsigned n=r.u16();
  for(unsigned i=0;i<n&&!r.bad;++i){const std::uint32_t id=r.u32();const int v=int(r.u32());if(r.bad||!shared(id))continue;named.push_back(id);
   // A clock the guest runs itself is set only when it starts, stops or has drifted.
   if(clockState(id)){const int mine=g.state(id);if(mine!=0&&v!=0&&std::abs(mine-v)<=ClockSlack)continue;}
   g.setState(id,v);}
  if(full&&!r.bad){std::sort(named.begin(),named.end());std::vector<StateId> gone;
   for(const auto& s:g.m_states)if(shared(s.id)&&!std::binary_search(named.begin(),named.end(),s.id))gone.push_back(s.id);
   for(auto id:gone)g.setState(id,0);}
  const unsigned objectives=r.u8();std::vector<StateId> listed;
  for(unsigned i=0;i<objectives&&!r.bad;++i){const std::uint32_t id=r.u32();const int status=int(r.u8());if(r.bad)break;listed.push_back(id);g.setObjective(id,ObjectiveStatus(std::clamp(status,0,3)));}
  if(full&&!r.bad){std::vector<StateId> gone;for(const auto& o:g.m_objectives)if(std::find(listed.begin(),listed.end(),o.id)==listed.end())gone.push_back(o.id);for(auto id:gone)g.setObjective(id,ObjectiveStatus::Hidden);}
  const unsigned props=r.u16();
  for(unsigned i=0;i<props&&!r.bad;++i){const std::uint32_t id=r.u32();const int v=int(r.u32());if(!r.bad)g.setEntityPropertyInt(id,v);}
  return true;
 }
 return false;
}

bool Match::handleCoopState(unsigned type,wire::Reader& r){
 Game& g=m_game;
 if(type!=Actors||m_mode!=Mode::Coop)return false;
 const std::uint32_t ms=r.u32();const int count=int(r.u8());if(r.bad)return true;
 if(m_lastActorsMs&&std::int32_t(ms-m_lastActorsMs)<0)return true;m_lastActorsMs=ms;
 for(int k=0;k<count;++k){
  const size_t i=r.u16();const int home=int(r.u8()),level=int(r.u8());Vec2 pos;pos.x=r.q16(256);pos.y=r.q16(256);const float z=r.q16(256);
  const float yaw=float(r.u8())/256.f*kTwoPi;const int clip=int(r.u8());const float phase=r.unit8();
  const int fields=int(r.u8());int values[ActorFieldCount]={};
  for(int f=0;f<fields&&!r.bad;++f){const int field=int(r.u8());const int v=readVarint(r);if(field<ActorFieldCount)values[field]=v;}
  if(r.bad)break;
  if(i>=g.m_friendlyActors.size()||level>=g.chunkCount())continue;auto& npc=g.m_friendlyActors[i];if(npc.homeLevel!=home||npc.summon)continue;
  if(m_actorTargets.size()<=i)m_actorTargets.resize(i+1);auto& t=m_actorTargets[i];
  // A first sight, a change of map or a jump: put him there; otherwise he is eased toward it (smoothActors).
  if(!t.set||npc.level!=level||length(npc.pose.position-pos)>3.f){npc.pose.position=pos;npc.pose.z=z;npc.pose.yaw=yaw;}
  t={true,level,pos,z,yaw};npc.level=level;npc.pose.clip=clip;npc.pose.phase=phase;
  for(int f=0;f<ActorFieldCount;++f)g.setState(g.friendlyKey(i,ActorFields[f]),values[f]);
 }
 return true;
}
}
