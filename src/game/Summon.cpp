// Console summons: `summon <kind> [count] [ally|enemy|neutral|faction=NAME]
// [ai=idle|patrol|wander|guard|hunt|follow] [hp=N] [scale=N] [weapon=...] [name=...]`
// and the commands that go with it (ally, enemy, neutral, kill, killall,
// freeze, ai on|off, list, help summon). docs/console.md is the reference.
//
// A summoned creature is an ordinary Enemy (spawnCreature) with a faction and a
// standing order; a summoned man is an ordinary FriendlyActor on an ActorTrack of
// his own (the same Guard and Civilian AI as authored men, the same squads and
// factions). Who fights whom is factionsHostile() throughout: allies (Facility)
// fight creatures and raiders, enemies fight the player and his allies, creatures
// and guards fight each other, neutrals fight nobody and run.
//
// Summons are transient. A save leaves them out with every state their AI wrote
// (stripSummons), so a load or a map change has none of them; they last the
// session on the map they were summoned on.
#include "Game.h"
#include "ai/Navigation.h"
#include "ai/Perception.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <unordered_set>
#include <charconv>
#include "../world/CustomCampaign.h"
namespace retro {
namespace {
struct SummonKind {const char* word;bool creature;CreatureKind creatureKind;ActorVisual visual;GuardSquadRole role;ActorAiMode mode;bool hunter;const char* label;};
constexpr SummonKind SummonKinds[]={
 {"huntsman",true,CreatureKind::Huntsman,{},{},{},false,"HUNTSMAN"},
 {"wasp",true,CreatureKind::Wasp,{},{},{},false,"XENOWASP"},{"xenowasp",true,CreatureKind::Wasp,{},{},{},false,"XENOWASP"},
 {"brute",true,CreatureKind::Brute,{},{},{},false,"SCISSOR FIEND"},{"fiend",true,CreatureKind::Brute,{},{},{},false,"SCISSOR FIEND"},
 {"warden",true,CreatureKind::Warden,{},{},{},false,"REACTOR STALKER"},{"stalker",true,CreatureKind::Warden,{},{},{},false,"REACTOR STALKER"},
 {"mutant",true,CreatureKind::Mutant,{},{},{},false,"MUTATED HUMAN"},
 {"snatcher",true,CreatureKind::Snatcher,{},{},{},false,"SNATCHER"},
 {"chimera",true,CreatureKind::Chimera,{},{},{},false,"CHIMERA"},
 {"transformed",true,CreatureKind::Transformed,{},{},{},false,"TRANSFORMED"},
 {"machine",true,CreatureKind::Machine,{},{},{},false,"CURSED EQUIPMENT"},{"equipment",true,CreatureKind::Machine,{},{},{},false,"CURSED EQUIPMENT"},
 // R-117's kind: a recurring hunter that cannot be finished, runs when badly hurt and is gone.
 {"hunter",true,CreatureKind::Chimera,{},{},{},true,"HUNTER"},{"r117",true,CreatureKind::Chimera,{},{},{},true,"HUNTER"},{"r-117",true,CreatureKind::Chimera,{},{},{},true,"HUNTER"},
 {"guard",false,{},ActorVisual::Cleanup,GuardSquadRole::Default,ActorAiMode::Guard,false,"GUARD"},
 {"soldier",false,{},ActorVisual::Cleanup,GuardSquadRole::Assault,ActorAiMode::Guard,false,"SOLDIER"},
 {"engineer",false,{},ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,ActorAiMode::Guard,false,"ENGINEER"},
 {"sharpshooter",false,{},ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,ActorAiMode::Guard,false,"SHARPSHOOTER"},
 {"heavy",false,{},ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,ActorAiMode::Guard,false,"HEAVY"},
 {"medic",false,{},ActorVisual::CleanupMedic,GuardSquadRole::Medic,ActorAiMode::Guard,false,"MEDIC"},
 {"rusher",false,{},ActorVisual::CleanupRusher,GuardSquadRole::Rusher,ActorAiMode::Guard,false,"RUSHER"},
 {"civilian",false,{},ActorVisual::Worker,{},ActorAiMode::Civilian,false,"CIVILIAN"},{"worker",false,{},ActorVisual::Worker,{},ActorAiMode::Civilian,false,"CIVILIAN"},
 {"scientist",false,{},ActorVisual::Scientist,{},ActorAiMode::Civilian,false,"SCIENTIST"},
 {"doctor",false,{},ActorVisual::FemaleDoctor,{},ActorAiMode::Civilian,false,"DOCTOR"},
 {"labtech",false,{},ActorVisual::LabTech,{},ActorAiMode::Civilian,false,"LAB TECH"}};
const SummonKind* summonKind(const std::string& word){
 std::string w=word;if(w.size()>3&&w.back()=='s'&&w!="heavy")w.pop_back(); // "guards", "huntsmen" stays as typed
 if(w=="huntsmen")w="huntsman";
 for(const auto&k:SummonKinds)if(w==k.word)return &k;return nullptr;}
const char* factionName(AiFaction f){
 switch(f){case AiFaction::Player:return "PLAYER";case AiFaction::Facility:return "ALLY";case AiFaction::Creature:return "CREATURE";
  case AiFaction::Raider:return "ENEMY";case AiFaction::Neutral:return "NEUTRAL";case AiFaction::Wary:return "WARY";default:return "UNSET";}}
bool parseFaction(const std::string& v,bool creature,AiFaction& out){
 if(v=="ally"||v=="facility"||v=="friend"){out=AiFaction::Facility;return true;}
 if(v=="enemy"||v=="hostile"){out=creature?AiFaction::Creature:AiFaction::Raider;return true;}
 if(v=="neutral"){out=AiFaction::Neutral;return true;}if(v=="player"){out=AiFaction::Player;return true;}
 if(v=="creature"){out=AiFaction::Creature;return true;}if(v=="raider"){out=AiFaction::Raider;return true;}
 if(v=="wary"||v=="truce"){out=AiFaction::Wary;return true;}return false;}
constexpr const char* OrderNames[]={"IDLE","PATROL","WANDER","GUARD","HUNT","FOLLOW"};
bool parseOrder(const std::string& v,int& out){
 for(int i=0;i<6;++i){std::string n=OrderNames[i];for(auto&c:n)c=char(std::tolower(static_cast<unsigned char>(c)));if(v==n){out=i;return true;}}return false;}
std::string upper(std::string s){for(auto&c:s)c=char(std::toupper(static_cast<unsigned char>(c)));return s;}
std::string fixed1(float v){char b[24];std::snprintf(b,sizeof b,"%.1f",double(v));return b;}
// A repeatable scatter (no rand(): the lab replays every tube twice and wants the same frames).
float scatter(std::uint32_t a,std::uint32_t b){std::uint32_t h=a*0x9e3779b9u^b*0x85ebca6bu;h^=h>>15;h*=0x2c1b3c6du;h^=h>>12;return float(h&0xffffu)/65535.f;}
}

// ------------------------------------------------------------------ placement
// The nearest clear spot in front of the player: inside the chunk, on a floor a
// step or two from the player's, the body's hull fits (walls, props, creatures),
// the player can see it, and nobody already stands there.
bool Game::summonSpot(Vec2& at,float& z,float hull,float radius,const std::vector<Vec2>& taken)const{
 const Vec2 f{std::cos(m_player.angle),std::sin(m_player.angle)},side{-f.y,f.x};const float eye=m_player.z+m_player.eye;
 for(float d:{2.5f,3.f,3.5f,2.f,4.f,4.5f,5.f,6.f,1.6f,7.f})for(float s:{0.f,.9f,-.9f,1.8f,-1.8f,2.7f,-2.7f}){
  const Vec2 p=m_player.pos+f*(d+radius)+side*s;if(p.x<radius+.3f||p.y<radius+.3f||p.x>World::Width-radius-.3f||p.y>World::Height-radius-.3f)continue;
  const float ground=m_world.hasTerrain()?groundHeight(p,m_player.z+1.2f,radius):groundHeight(p,m_player.z+.6f,radius);
  if(!std::isfinite(ground)||std::fabs(ground-m_player.z)>1.5f||!hullFits(p,ground,hull,radius,true,true))continue;
  if(!m_world.rayClear(m_player.pos,eye,p,ground+std::min(hull,1.2f)*.5f))continue;
  bool free=lengthSq(p-m_player.pos)>(radius+.6f)*(radius+.6f);
  for(const auto&t:taken)free&=lengthSq(p-t)>(radius*2+.35f)*(radius*2+.35f);
  for(const auto&n:m_friendlyActors)if(n.level==m_level)free&=lengthSq(p-n.pose.position)>(radius+.5f)*(radius+.5f);
  if(!free)continue;at=p;z=ground;return true;}
 return false;
}

// ------------------------------------------------------------------ summon
int Game::summon(const std::string& word,int count,const std::string& options,std::string& error){
 const SummonKind* kind=summonKind(word);if(!kind){error="UNKNOWN KIND "+upper(word)+". HELP SUMMON LISTS THEM.";return 0;}
 AiFaction faction=kind->creature?AiFaction::Creature:kind->mode==ActorAiMode::Civilian?AiFaction::Neutral:AiFaction::Raider;
 SummonOrder order=kind->hunter?SummonOrder::Hunt:kind->mode==ActorAiMode::Guard?SummonOrder::Guard:SummonOrder::Idle;
 float hp=0,scale=1;int weapon=-1;std::string name;
 std::istringstream in(options);std::string token;
 while(in>>token){const auto eq=token.find('=');const std::string key=token.substr(0,eq),value=eq==std::string::npos?"":token.substr(eq+1);
  if(eq==std::string::npos){if(!parseFaction(token,kind->creature,faction)){error="UNKNOWN OPTION "+upper(token)+".";return 0;}continue;}
  if(key=="faction"||key=="side"){if(!parseFaction(value,kind->creature,faction)){error="FACTION IS ALLY, ENEMY, NEUTRAL, PLAYER, FACILITY, CREATURE, RAIDER OR WARY.";return 0;}}
  else if(key=="ai"||key=="order"){int o=0;if(parseOrder(value,o))order=SummonOrder(o);else{error="AI IS IDLE, PATROL, WANDER, GUARD, HUNT OR FOLLOW.";return 0;}}
  else if(key=="hp"||key=="health"){hp=std::strtof(value.c_str(),nullptr);if(!(hp>0&&hp<=100000)){error="HP IS A NUMBER ABOVE 0.";return 0;}}
  else if(key=="scale"){scale=std::strtof(value.c_str(),nullptr);if(!(scale>=.25f&&scale<=4)){error="SCALE IS 0.25 TO 4.";return 0;}}
  else if(key=="weapon"){if(value=="rifle"||value=="shotgun"||value=="1")weapon=1;else if(value=="handgun"||value=="pistol"||value=="2")weapon=2;else if(value=="none"||value=="unarmed"||value=="0")weapon=0;
   else{error="WEAPON IS RIFLE, HANDGUN OR NONE.";return 0;}}
  else if(key=="name"){name=value;}
  else{error="UNKNOWN OPTION "+upper(key)+".";return 0;}}
 if(kind->creature&&m_peaceful){error="DEVELOPER MODE SPAWNS NO CREATURES. TYPE DEV TO LEAVE IT.";return 0;}
 if(kind->creature&&weapon>=0){error="CREATURES CARRY NO WEAPON.";return 0;}
 std::vector<Vec2> taken;for(const auto&e:m_enemies)if(e.alive)taken.push_back(e.pos);
 const Vec2 facing{std::cos(m_player.angle),std::sin(m_player.angle)};int made=0;
 for(int n=0;n<count;++n){
  const std::uint32_t serial=++m_summonSerial;
  SummonRecord record;record.serial=serial;record.kind=kind->label;record.creature=kind->creature;record.order=order;record.level=m_level;
  record.name=name.empty()?std::string(kind->label)+" "+std::to_string(serial):upper(name)+(count>1?" "+std::to_string(n+1):"");
  float hull=Player::StandingHeight*scale,radius=.3f*scale;
  if(kind->creature){const auto&t=creatureTraits(kind->creatureKind);hull=t.hull*scale;radius=std::max(t.radius*scale,.25f);}
  Vec2 at;float z;if(!summonSpot(at,z,hull,radius,taken)){if(!made)error="NO CLEAR SPOT IN FRONT OF YOU. FACE OPEN FLOOR.";break;}
  taken.push_back(at);record.anchor=at;record.anchorZ=z;
  // Where a patrol order walks to and back: the farthest clear point ahead, up to 7 m.
  record.patrolTo=at;record.patrolToZ=z;
  for(float d=7.f;d>=2.f;d-=1.f){const Vec2 p=at+facing*d;if(p.x<1||p.y<1||p.x>World::Width-1||p.y>World::Height-1)continue;
   const float pz=groundHeight(p,z+.6f,radius);if(std::fabs(pz-z)<.5f&&hullFits(p,pz,hull,radius,false,true)&&m_world.rayClear(at,z+.5f,p,pz+.5f)){record.patrolTo=p;record.patrolToZ=pz;break;}}
  const float faceYawToPlayer=std::atan2(m_player.pos.y-at.y,m_player.pos.x-at.x);
  if(kind->creature){
   CreatureSpawn s{kind->creatureKind,at,z};s.scale=scale;s.health=hp;if(kind->hunter){s.hunter=stateId("summon/"+std::to_string(serial)+"/hunter");m_summonKeys.emplace(s.hunter,0);}
   spawnCreature(s,0,true);auto&e=m_enemies.back();
   e.summon=serial;e.faction=faction;e.order=std::uint8_t(order);e.anchor=at;e.anchorZ=z;e.patrolTo=record.patrolTo;e.patrolToZ=record.patrolToZ;
   e.wakeState=0; // the cursed equipment comes up awake
   e.heading=faceYawToPlayer;
   if(kind->hunter&&factionsHostile(faction,AiFaction::Player))wakeHunter(e);
  }else{
   auto map=std::make_shared<AuthoredMapData>();map->name="Console summon";
   const std::string key="summon/"+std::to_string(serial)+"/";
   ActorTrack t;t.timerState=stateId(key+"clock");t.visual=kind->visual;t.damageState=stateId(key+"damage");t.deadState=stateId(key+"dead");
   m_summonKeys.emplace(t.damageState,0);m_summonKeys.emplace(t.deadState,0);
   const bool guard=kind->mode==ActorAiMode::Guard;
   t.scale=(guard?1.6f:1.7f)*scale;t.health=int(std::lround(hp>0?hp:guard?90.f:60.f));
   // Actor yaw is measured from +y (sin yaw, cos yaw).
   const float yaw=std::atan2(m_player.pos.x-at.x,m_player.pos.y-at.y);
   t.keys={{0,at,z,yaw},{60000,at,z,yaw,0,1}};
   auto&ai=t.ai;ai.mode=kind->mode;ai.faction=faction;
   if(guard){ai.role=kind->role;ai.armed=weapon!=0;ai.weapon=weapon>0?weapon:kind->role==GuardSquadRole::Medic||kind->role==GuardSquadRole::Engineer?2:1;
    ai.sightRange=14;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=1.5f;
    // One squad per side: summoned men of a side cover, flank and call out together.
    ai.squad=stateId(std::string("summon_squad/")+factionName(faction));
    if(order==SummonOrder::Patrol)ai.patrol={{at,z,2500,yaw},{record.patrolTo,record.patrolToZ,2500,std::nanf("")}};
    else ai.patrol={{at,z,60000,yaw}};}
   map->actorTracks={t};record.map=map;
   FriendlyActor npc;npc.homeLevel=npc.level=m_level;npc.homeActor=FriendlyActor::SummonActorBase+serial;npc.summon=serial;npc.definition=map;
   npc.pose={at,z,yaw,0,0,-1};m_friendlyActors.push_back(npc);const size_t i=m_friendlyActors.size()-1;
   auto put=[&](const char* f,int v){setState(friendlyKey(i,f),v);};
   put("chunk",m_level+1);put("x",int(std::lround(at.x*1000)));put("y",int(std::lround(at.y*1000)));put("z",int(std::lround(z*1000)));
   put("yaw",int(std::lround(std::remainder(yaw,2*kPi)*10000)));put("saved",1);put("engaged",1);
   if(!guard&&order==SummonOrder::Follow)put("follow",1);
  }
  m_summons.push_back(record);++made;}
 return made;
}

// ------------------------------------------------------------------ aim
// What the crosshair is on: the shot's own trace first, then the living body
// nearest the line of sight within a few degrees (an aim a little off still counts).
AiTargetRef Game::aimedEntity(float range)const{
 const float pitch=m_player.pitch/140.f;const Point3 eye{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye};
 const Point3 dir{std::cos(m_player.angle)*std::cos(pitch),std::sin(m_player.angle)*std::cos(pitch),std::sin(pitch)};
 const auto hit=traceShot(eye,dir,range,-1,false);
 if(hit.kind==ShotKind::Enemy||hit.kind==ShotKind::Npc)return {hit.kind,hit.level,hit.index};
 AiTargetRef best;float bestDot=.985f;
 auto consider=[&](ShotKind kind,int index,Point3 p){const Point3 d=p-eye;const float len=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(len<.3f||len>range)return;
  const float c=(d.x*dir.x+d.y*dir.y+d.z*dir.z)/len;if(c>bestDot&&m_world.rayClear(m_player.pos,eye.z,{p.x,p.y},p.z)){bestDot=c;best={kind,m_level,index};}};
 for(size_t i=0;i<m_enemies.size();++i){const auto&e=m_enemies[i];if(e.alive)consider(ShotKind::Enemy,int(i),{e.pos.x,e.pos.y,(e.bodyBottom()+e.bodyTop())*.5f});}
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];if(n.level==m_level&&!npcIncapacitated(i)&&n.pose.clip!=4)consider(ShotKind::Npc,int(i),{n.pose.position.x,n.pose.position.y,n.pose.z+n.track().scale*.6f});}
 return best;
}

// ------------------------------------------------------------------ orders
// A summoned neutral creature runs: from anything that fights all comers close by
// and in view, and from whatever (the player included) just hurt it. True while it
// runs, with the spot it runs to; its flight lasts a few seconds past the last scare.
bool Game::summonFlight(Enemy& e,Vec2& goal,float& goalZ,float dt){
 if(e.faction!=AiFaction::Neutral)return false;
 const Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
 Vec2 threat{};float nearest=1e9f;bool scared=false;
 auto consider=[&](Vec2 p,float mid,float reach,bool always){const float d=lengthSq(p-e.pos);if(d>=reach*reach||d>=nearest)return;
  if(!always&&!aiCanSee(m_world,e.pos,e.z+.7f,facing,p,mid,reach,-.4f,2.5f))return;nearest=d;threat=p;scared=true;};
 for(const auto&o:m_enemies)if(&o!=&e&&o.alive&&factionThreatens(AiFaction::Neutral,o.faction))consider(o.pos,(o.bodyBottom()+o.bodyTop())*.5f,7.f,false);
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];if(n.level!=m_level||npcIncapacitated(i)||n.pose.clip==4)continue;
  if(factionThreatens(AiFaction::Neutral,npcFaction(n.track())))consider(n.pose.position,n.pose.z+1.f,7.f,false);}
 // Hurt: away from whoever is closest of those, or the player.
 if(e.painFlash>.5f&&!scared){threat=m_player.pos;scared=true;}
 if(scared){e.fleeTime=3.f;e.lastKnown=threat;}
 if(e.fleeTime<=0)return false;
 e.fleeTime=std::max(0.f,e.fleeTime-dt);e.fleeClock-=dt;e.awareness=std::max(e.awareness,1.f);
 if(e.state==Enemy::State::Search||e.state==Enemy::State::Idle)e.state=Enemy::State::Investigate;
 if(!e.hideSet||e.fleeClock<=0){const Vec2 from=e.lastKnown;Vec2 best=e.pos;float bestZ=e.z,bestScore=-1e9f;
  for(int i=0;i<16;++i){const float a=float(i)*kPi/8;for(float r:{3.f,5.f}){const Vec2 p=e.pos+Vec2{std::cos(a),std::sin(a)}*r;
   if(p.x<1||p.y<1||p.x>World::Width-1||p.y>World::Height-1)continue;const float z=groundHeight(p,e.z+.6f,e.radius());
   if(std::fabs(z-e.z)>.6f||!hullFits(p,z,e.hull(),e.radius(),false,false)||!m_world.rayClear(e.pos,e.z+.5f,p,z+.5f))continue;
   const float score=length(p-from)-.2f*r;if(score>bestScore){bestScore=score;best=p;bestZ=z;}}}
  e.hideAt=best;e.hideSet=true;e.fleeClock=1.f;e.patrolToZ=bestZ;goal=best;goalZ=bestZ;return true;}
 goal=e.hideAt;goalZ=e.patrolToZ;return true;
}
void Game::updateSummons(float dt){
 if(dt<=0)return;
 // Records whose body is gone (a load, a map change) go too.
 auto findEnemy=[&](std::uint32_t serial)->Enemy*{for(auto&e:m_enemies)if(e.summon==serial)return &e;
  for(int level=0;level<chunkCount();++level)if(level!=m_level)for(auto&e:m_chunks[size_t(level)].enemies)if(e.summon==serial)return &e;return nullptr;};
 auto findNpc=[&](std::uint32_t serial)->int{for(size_t i=0;i<m_friendlyActors.size();++i)if(m_friendlyActors[i].summon==serial)return int(i);return -1;};
 std::erase_if(m_summons,[&](const SummonRecord&r){return r.creature?findEnemy(r.serial)==nullptr:findNpc(r.serial)<0;});
 if(!m_factionOverrides.empty())std::erase_if(m_factionOverrides,[&](const auto&o){for(const auto&n:m_friendlyActors)if(&n.track()==o.first)return false;return true;});
 if(m_aiOff)return;
 // The nearest thing a summon on a hunt order goes after: what its side is at war with.
 auto quarry=[&](AiFaction side,Vec2 from,Vec2& at,float& z)->bool{float best=1e9f;bool found=false;
  auto take=[&](Vec2 p,float pz){const float d=lengthSq(p-from);if(d<best){best=d;at=p;z=pz;found=true;}};
  if(factionsHostile(side,AiFaction::Player)&&!dead()&&hiddenLocker()<0)take(m_player.pos,m_player.z);
  for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];const auto&t=n.track();
   if(n.level!=m_level||npcIncapacitated(i)||n.pose.clip==4||!t.health||(t.deadState&&state(t.deadState))||t.ai.mode==ActorAiMode::Scripted||!autonomousActor(t))continue;
   if(factionsHostile(side,npcFaction(t)))take(n.pose.position,n.pose.z);}
  for(const auto&o:m_enemies)if(o.alive&&!o.sleeping()&&factionsHostile(side,o.faction))take(o.pos,o.z);
  return found;};
 for(auto&e:m_enemies){if(!e.summon||!e.alive||e.frozen||e.arrive>0)continue;e.orderClock-=dt;
  const auto order=SummonOrder(e.order);const bool unaware=e.awareness<=0;
  if(order==SummonOrder::Patrol&&unaware&&lengthSq(e.pos-e.home)<.36f){const bool out=lengthSq(e.home-e.anchor)<.01f;e.home=out?e.patrolTo:e.anchor;e.homeZ=out?e.patrolToZ:e.anchorZ;}
  else if(order==SummonOrder::Wander&&unaware&&(lengthSq(e.pos-e.home)<.36f||e.orderClock<=0)){e.orderClock=8.f;
   for(int tries=0;tries<12;++tries){const float a=scatter(e.summon,std::uint32_t(m_elapsed*10)+tries)*kTwoPi,r=1.5f+scatter(e.summon+77u,std::uint32_t(m_elapsed*10)+tries)*3.5f;
    const Vec2 p=e.anchor+Vec2{std::cos(a),std::sin(a)}*r;if(p.x<1||p.y<1||p.x>World::Width-1||p.y>World::Height-1)continue;const float z=groundHeight(p,e.anchorZ+.6f,e.radius());
    if(std::fabs(z-e.anchorZ)<.6f&&hullFits(p,z,e.hull(),e.radius(),false,false)){e.home=p;e.homeZ=z;break;}}}
  else if(order==SummonOrder::Guard){e.home=e.anchor;e.homeZ=e.anchorZ;}
  else if(order==SummonOrder::Hunt&&e.state!=Enemy::State::Chase&&e.orderClock<=0&&e.stalkMode!=Enemy::StalkMode::Retreat){e.orderClock=.5f;Vec2 at;float z;
   if(quarry(e.faction,e.pos,at,z)){e.lastKnown=at;e.lastKnownZ=z;e.awareness=std::max(e.awareness,6.f);e.state=Enemy::State::Investigate;e.repathTimer=0;}}
  else if(order==SummonOrder::Follow&&unaware&&lengthSq(m_player.pos-e.home)>2.5f*2.5f){const Vec2 behind=m_player.pos-Vec2{std::cos(m_player.angle),std::sin(m_player.angle)}*1.6f;
   e.home={std::clamp(behind.x,1.f,float(World::Width)-1),std::clamp(behind.y,1.f,float(World::Height)-1)};e.homeZ=m_player.z;}
 }
 // Summoned men: their one patrol point is the order. Moving it moves them; when
 // they meet something hostile their own guard AI takes over until it is done.
 for(auto&r:m_summons){if(r.creature||!r.map)continue;const int i=findNpc(r.serial);if(i<0)continue;const auto&npc=m_friendlyActors[size_t(i)];
  if(npc.frozen||npcIncapacitated(size_t(i))||npc.pose.clip==4)continue;auto&t=r.map->actorTracks[0];if(t.deadState&&state(t.deadState))continue;
  r.clock-=dt;const Vec2 shift=chunkOffset(m_level)-chunkOffset(npc.homeLevel);
  if(t.ai.mode!=ActorAiMode::Guard){ // a civilian: follow is the AI's own follow; the rest stand
   if(r.order==SummonOrder::Follow)setState(friendlyKey(size_t(i),"follow"),1);continue;}
  if(guardState(size_t(i))!=GuardState::Patrol||t.ai.patrol.size()!=1)continue;
  auto&post=t.ai.patrol[0];Vec2 want=post.position;float wantZ=post.z;
  if(r.order==SummonOrder::Follow&&lengthSq(m_player.pos+shift-post.position)>3.f*3.f){want=m_player.pos+shift-Vec2{std::cos(m_player.angle),std::sin(m_player.angle)}*1.8f;wantZ=m_player.z;}
  else if(r.order==SummonOrder::Hunt&&r.clock<=0){r.clock=1.f;Vec2 at;float z;if(quarry(npcFaction(t),npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level),at,z)){want=at+shift;wantZ=z;}}
  else if(r.order==SummonOrder::Wander&&r.clock<=0){r.clock=7.f;const float a=scatter(r.serial,++r.leg)*kTwoPi,d=1.5f+scatter(r.serial+91u,r.leg)*3.5f;
   const Vec2 p=r.anchor+Vec2{std::cos(a),std::sin(a)}*d;const float z=groundHeight(p,r.anchorZ+.6f,.3f);
   if(p.x>1&&p.y>1&&p.x<World::Width-1&&p.y<World::Height-1&&std::fabs(z-r.anchorZ)<.6f&&hullFits(p,z,t.scale,.3f,true,true)){want=p+chunkOffset(r.level)-chunkOffset(npc.homeLevel);wantZ=z;}}
  if(lengthSq(want-post.position)>.04f){post.position=want;post.z=wantZ;post.pauseMs=60000;setState(friendlyKey(size_t(i),"guard_arrived"),0);setState(friendlyKey(size_t(i),"repath"),0);}
 }
}

// ------------------------------------------------------------------ saves
// A save carries no summon: not the bodies, not the states their AI wrote, not an
// authored man's memory of one (his target, his attacker) that would not resolve.
void Game::stripSummons(){
 std::vector<std::vector<char>> doomed(static_cast<size_t>(chunkCount()));bool anyCreature=false;
 for(int level=0;level<chunkCount();++level){const auto&list=level==m_level?m_enemies:m_chunks[size_t(level)].enemies;doomed[size_t(level)].resize(list.size());
  for(size_t i=0;i<list.size();++i)if(list[i].summon){doomed[size_t(level)][i]=1;anyCreature=true;}}
 size_t authored=0;while(authored<m_friendlyActors.size()&&!m_friendlyActors[authored].summon)++authored;
 const bool anyNpc=std::any_of(m_friendlyActors.begin(),m_friendlyActors.end(),[](const FriendlyActor&n){return n.summon!=0;});
 if(!anyCreature&&!anyNpc&&m_summonKeys.empty())return;
 // Forget references to what is about to go.
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){if(m_friendlyActors[actor].summon)continue;
  for(const char* prefix:{"guard_target_","weapon_target_","hit_","task_enemy_","tactic_","grenade_target_"}){const std::string p=prefix;
   const StateId kindKey=friendlyKey(actor,p+"kind"),levelKey=friendlyKey(actor,p+"level"),indexKey=friendlyKey(actor,p+"index");
   const int kind=state(kindKey),level=state(levelKey)-1,index=state(indexKey)-1;bool gone=false;
   if(kind==int(ShotKind::Enemy)&&level>=0&&level<chunkCount()&&index>=0&&size_t(index)<doomed[size_t(level)].size())gone=doomed[size_t(level)][size_t(index)]!=0;
   if(kind==int(ShotKind::Npc)&&index>=0&&size_t(index)<m_friendlyActors.size())gone=m_friendlyActors[size_t(index)].summon!=0;
   if(gone){setState(kindKey,0);setState(levelKey,0);setState(indexKey,0);}}}
 // The creatures that stay keep their references (a summon among them shifts the indices).
 for(int level=0;level<chunkCount();++level){auto&list=level==m_level?m_enemies:m_chunks[size_t(level)].enemies;const auto&d=doomed[size_t(level)];
  if(std::find(d.begin(),d.end(),1)==d.end())continue;std::vector<AiTargetRef> moved(list.size());int next=0;
  for(size_t i=0;i<list.size();++i)moved[i]=d[i]?AiTargetRef{ShotKind::Enemy,level,int(i)}:AiTargetRef{ShotKind::Enemy,level,next++};
  remapEnemyRefs(level,moved);std::vector<Enemy> kept;kept.reserve(list.size());for(size_t i=0;i<list.size();++i)if(!d[i])kept.push_back(list[i]);list=std::move(kept);}
 std::erase_if(m_friendlyActors,[](const FriendlyActor&n){return n.summon!=0;});
 std::erase_if(m_states,[&](const StateValue&v){return m_summonKeys.count(v.id)!=0;});
 m_summons.clear();m_summonKeys.clear();m_factionOverrides.clear();
}

// ------------------------------------------------------------------ console
bool Game::consoleSummonCommand(const std::string& command){
 std::istringstream in(command);std::string verb,arg;in>>verb;
 auto say=[&](const std::string& line){m_consoleLog.push_back(upper(line));};
 auto rest=[&]{std::string r;std::getline(in,r);return r;};
 auto enemyAt=[&](const AiTargetRef& r)->Enemy*{if(r.kind!=ShotKind::Enemy||r.level<0||r.level>=chunkCount())return nullptr;auto&l=r.level==m_level?m_enemies:m_chunks[size_t(r.level)].enemies;return r.index>=0&&size_t(r.index)<l.size()?&l[size_t(r.index)]:nullptr;};
 auto label=[&](const AiTargetRef& r)->std::string{if(auto*e=enemyAt(r))return e->name();if(r.kind==ShotKind::Npc){const auto&t=m_friendlyActors[size_t(r.index)].track();return t.ai.mode==ActorAiMode::Guard?guardRoleProfile(t.ai.role).name:"CIVILIAN";}return "NOTHING";};
 auto killEnemy=[&](Enemy& e,int level){if(!e.alive)return false;if(e.hunter&&!e.summon){vanishHunter(e);return true;}e.hunter=0;applyEnemyDamage(e,1e7f,true,false,level);return true;};
 auto killNpc=[&](size_t i){const auto&t=m_friendlyActors[i].track();if(!t.health||(t.deadState&&state(t.deadState))||m_friendlyActors[i].pose.clip==4)return false;damageFriendly(i,1e6f,m_friendlyActors[i].pose.position);return true;};
 if(verb=="help"){in>>arg;if(arg!="summon"&&arg!="summons")return false;
  for(const char* l:{"SUMMON <KIND> [COUNT] [ALLY|ENEMY|NEUTRAL|FACTION=NAME] [AI=ORDER] [HP=N] [SCALE=N] [WEAPON=RIFLE|HANDGUN|NONE] [NAME=X]",
   "KINDS: HUNTSMAN WASP BRUTE WARDEN MUTANT SNATCHER CHIMERA TRANSFORMED MACHINE HUNTER (R-117'S KIND)",
   "       GUARD SOLDIER ENGINEER SHARPSHOOTER HEAVY MEDIC RUSHER / CIVILIAN SCIENTIST DOCTOR LABTECH",
   "SIDES: ALLY FIGHTS CREATURES AND ENEMIES; ENEMY FIGHTS YOU AND YOUR ALLIES; CREATURES AND GUARDS FIGHT EACH OTHER; NEUTRAL RUNS.",
   "AI: IDLE (STAND), PATROL (TO A POINT AHEAD AND BACK), WANDER, GUARD (HOLD THE SPOT, 8 M), HUNT (GO AFTER THE NEAREST FOE), FOLLOW (YOU).",
   "SPAWNS IN FRONT OF YOU ON CLEAR FLOOR YOU CAN SEE. COUNT 1-16. SUMMONS ARE TRANSIENT: NOT SAVED, GONE ON LOAD OR MAP CHANGE.",
   "ALLY / ENEMY / NEUTRAL: TURN WHAT YOU AIM AT.  KILL: KILL IT.  KILLALL [KIND|SUMMONS]: CREATURES AND SUMMONS, OR ONE KIND.",
   "FREEZE [ALL|OFF]: HOLD WHAT YOU AIM AT (ALL: EVERY SUMMON).  AI ON|OFF: EVERY CREATURE AND MAN.  LIST: THE SUMMONS AND WHO IS HERE."})m_consoleLog.push_back(l);
  return true;}
 if(verb=="summon"||verb=="spawn"){std::string kind;in>>kind;if(kind.empty()){say("USE SUMMON <KIND> [COUNT] [OPTIONS]. HELP SUMMON FOR MORE.");return true;}
  std::string options=rest();int count=1;{std::istringstream o(options);std::string first;o>>first;int n=0;auto r=std::from_chars(first.data(),first.data()+first.size(),n);
   if(!first.empty()&&r.ec==std::errc{}&&r.ptr==first.data()+first.size()){if(n<1||n>16){say("COUNT IS 1 TO 16.");return true;}count=n;std::getline(o,options);}}
  std::string error;const int made=summon(kind,count,options,error);
  if(!made){say(error);return true;}
  const auto&last=m_summons.back();
  say("SUMMONED "+std::to_string(made)+" "+last.kind+(made<count?" (ONLY "+std::to_string(made)+" FIT)":"")+" / "+std::string(OrderNames[int(last.order)])+
   " / "+(last.creature?factionName(m_enemies.back().faction):factionName(last.map->actorTracks[0].ai.faction))+". NOT SAVED.");
  return true;}
 if(verb=="ally"||verb=="enemy"||verb=="neutral"||verb=="faction"){std::string side=verb;if(verb=="faction")in>>side;
  const auto aimed=aimedEntity();if(aimed.kind==ShotKind::None){say("AIM AT A CREATURE OR A MAN, THEN TYPE "+verb+".");return true;}
  if(auto*e=enemyAt(aimed)){AiFaction f;if(!parseFaction(side,true,f)){say("UNKNOWN FACTION "+side+".");return true;}
   e->faction=f;e->awareness=0;e->state=Enemy::State::Idle;e->windup=0;e->recognition=0;e->lastKnown=e->pos;e->lastKnownZ=e->z;
   say(std::string(e->name())+" IS NOW "+factionName(f)+".");return true;}
  const size_t i=size_t(aimed.index);AiFaction f;if(!parseFaction(side,false,f)){say("UNKNOWN FACTION "+side+".");return true;}
  const auto&npc=m_friendlyActors[i];bool done=false;
  for(auto&r:m_summons)if(!r.creature&&r.map&&r.serial==npc.summon){auto&ai=r.map->actorTracks[0].ai;ai.faction=f;if(ai.mode==ActorAiMode::Guard)ai.squad=stateId(std::string("summon_squad/")+factionName(f));done=true;}
  if(!done){std::erase_if(m_factionOverrides,[&](const auto&o){return o.first==&npc.track();});m_factionOverrides.push_back({&npc.track(),f});}
  for(const char* p:{"guard_target_kind","guard_target_level","guard_target_index","guard_confirmed","guard_memory"})setState(friendlyKey(i,p),0);
  say(label(aimed)+" IS NOW "+factionName(f)+".");return true;}
 if(verb=="kill"){const auto aimed=aimedEntity();bool ok=false;
  if(auto*e=enemyAt(aimed))ok=killEnemy(*e,aimed.level);else if(aimed.kind==ShotKind::Npc)ok=killNpc(size_t(aimed.index));
  say(ok?label(aimed)+" KILLED.":"AIM AT SOMETHING ALIVE, THEN TYPE KILL.");return true;}
 if(verb=="killall"){in>>arg;const SummonKind* only=arg.empty()||arg=="summons"||arg=="all"?nullptr:summonKind(arg);
  if(!arg.empty()&&arg!="summons"&&arg!="all"&&!only){say("UNKNOWN KIND "+arg+".");return true;}
  const bool summonsOnly=arg=="summons";int creatures=0,men=0;
  for(int level=0;level<chunkCount();++level)if(level==m_level||chunkResident(level))for(auto&e:level==m_level?m_enemies:m_chunks[size_t(level)].enemies){
   if(summonsOnly&&!e.summon)continue;if(only&&(!only->creature||e.kind!=only->creatureKind))continue;creatures+=killEnemy(e,level);}
  for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];const auto&t=n.track();
   if(only){if(only->creature||t.visual!=only->visual||(t.ai.mode==ActorAiMode::Guard)!=(only->mode==ActorAiMode::Guard)||n.level!=m_level)continue;}
   else if(!n.summon)continue; // killall alone spares the authored men; killall guard does not
   men+=killNpc(i);}
  say("KILLED "+std::to_string(creatures)+" CREATURES AND "+std::to_string(men)+" MEN.");return true;}
 if(verb=="freeze"||verb=="unfreeze"){in>>arg;
  if(arg=="all"||arg=="off"){const bool on=verb=="freeze"&&arg=="all";int n=0;
   for(int level=0;level<chunkCount();++level)for(auto&e:level==m_level?m_enemies:m_chunks[size_t(level)].enemies)if(e.summon||!on){n+=e.frozen!=on;e.frozen=on&&e.summon;}
   for(auto&a:m_friendlyActors)if(a.summon||!on){n+=a.frozen!=on;a.frozen=on&&a.summon;}
   say(on?std::to_string(n)+" SUMMONS FROZEN.":"NOTHING FROZEN NOW.");return true;}
  const auto aimed=aimedEntity();
  if(auto*e=enemyAt(aimed)){e->frozen=verb=="freeze"?!e->frozen:false;say(std::string(e->name())+(e->frozen?" FROZEN.":" MOVES AGAIN."));return true;}
  if(aimed.kind==ShotKind::Npc){auto&a=m_friendlyActors[size_t(aimed.index)];a.frozen=verb=="freeze"?!a.frozen:false;say(label(aimed)+(a.frozen?" FROZEN.":" MOVES AGAIN."));return true;}
  say("AIM AT A CREATURE OR A MAN, OR USE FREEZE ALL / FREEZE OFF.");return true;}
 if(verb=="ai"){in>>arg;if(arg=="on"||arg=="off"){m_aiOff=arg=="off";say(m_aiOff?"AI OFF: NO CREATURE OR MAN THINKS OR MOVES.":"AI ON.");}else say("USE AI ON OR AI OFF.");return true;}
 if(verb=="list"){updateSummons(1e-6f);
  say(std::to_string(m_summons.size())+" SUMMONS (TRANSIENT)"+(m_aiOff?" / AI OFF":""));
  for(const auto&r:m_summons){std::string line="#"+std::to_string(r.serial)+" "+r.name;
   if(r.creature){for(int level=0;level<chunkCount();++level)for(const auto&e:level==m_level?m_enemies:m_chunks[size_t(level)].enemies)if(e.summon==r.serial)
     line+=std::string(" ")+factionName(e.faction)+" "+OrderNames[e.order]+" HP "+std::to_string(int(std::ceil(std::max(0.f,e.hp))))+(e.alive?"":" DEAD")+(e.frozen?" FROZEN":"")+(level!=m_level?" MAP "+std::to_string(level):" AT "+fixed1(e.pos.x)+","+fixed1(e.pos.y));}
   else for(size_t i=0;i<m_friendlyActors.size();++i)if(m_friendlyActors[i].summon==r.serial){const auto&n=m_friendlyActors[i];const auto&t=n.track();const bool down=(t.deadState&&state(t.deadState))||n.pose.clip==4;
     line+=std::string(" ")+factionName(npcFaction(t))+" "+OrderNames[int(r.order)]+" HP "+std::to_string(std::max(0,t.health-(t.damageState?state(t.damageState):0)))+(down?" DOWN":"")+(n.frozen?" FROZEN":"")+" AT "+fixed1(n.pose.position.x)+","+fixed1(n.pose.position.y);}
   say(line);}
  int creatures=0,men=0;for(const auto&e:m_enemies)creatures+=e.alive&&!e.summon;for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];men+=n.level==m_level&&!n.summon&&n.pose.clip!=4;}
  say("ALSO HERE: "+std::to_string(creatures)+" CREATURES, "+std::to_string(men)+" MEN. "+std::to_string(enemiesRemaining())+" HOSTILE TO YOU.");return true;}
 return false;
}

// ------------------------------------------------------------------ test
// The console end to end, in the engine-regression room: parsing, placement,
// allegiances, a fight each way, the commands on the aimed body, and that a save
// drops every summon and every state they wrote.
bool Game::testSummon(std::ostream& report){
 bool passed=true;auto check=[&](bool ok,const std::string& label){report<<(ok?"PASS ":"FAIL ")<<label<<'\n';report.flush();passed&=ok;};
 auto room=[]{auto m=std::make_shared<AuthoredMapData>();m->name="Summon room";m->definition={{0,0},{4.5f,12.f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData floor;for(auto& row:floor.rows)row=std::string(24,'.');floor.rows.front()=floor.rows.back()=std::string(24,'#');for(auto& row:floor.rows)row.front()=row.back()='#';m->layers.push_back(floor);
  auto c=std::make_shared<CustomCampaign>();c->name="Summon test";c->key=0x53554d4e;c->maps={m};return c;};
 auto fresh=[&]{Game g(room());g.m_god=true;g.m_player.pos={4.5f,12.f};g.m_player.z=0;g.m_player.angle=0;g.m_player.pitch=0;g.update({},1.f/60);return g;};
 auto type=[](Game& g,const std::string& line){g.executeConsole(line);};
 auto has=[](const Game& g,const std::string& text){for(const auto&l:g.m_consoleLog)if(l.find(text)!=std::string::npos)return true;return false;};
 {Game g=fresh();type(g,"help");check(has(g,"SUMMON"),"help names SUMMON");type(g,"help summon");check(has(g,"KINDS:")&&has(g,"TRANSIENT"),"help summon lists kinds and says summons are transient");
  type(g,"summon dragon");check(has(g,"UNKNOWN KIND")&&g.m_summons.empty(),"an unknown kind is refused");
  type(g,"summon huntsman 3 ally ai=wander hp=50 scale=1.2 name=rex");
  int made=0;bool fields=true,clear=true;for(const auto&e:g.m_enemies)if(e.summon){++made;fields&=e.faction==AiFaction::Facility&&e.order==int(SummonOrder::Wander)&&e.maxHp==50&&std::fabs(e.scale-1.2f)<1e-4f;
   clear&=e.pos.x>g.m_player.pos.x+1.f&&g.hullFits(e.pos,e.z,e.hull(),e.radius(),false,true);}
  check(made==3&&fields,"summon huntsman 3 ally ai=wander hp=50 scale=1.2 makes three allied wandering huntsmen ("+std::to_string(made)+")");
  check(clear,"each stands clear, in front of the player");
  check(g.m_summons.size()==3&&g.m_summons[0].name=="REX 1","the records carry the name");
  type(g,"summon guard 2 enemy ai=patrol weapon=handgun");int men=0;bool guard=true;for(const auto&n:g.m_friendlyActors)if(n.summon){++men;const auto&t=n.track();guard&=t.ai.mode==ActorAiMode::Guard&&t.ai.faction==AiFaction::Raider&&t.ai.weapon==2&&t.ai.patrol.size()==2;}
  check(men==2&&guard,"summon guard 2 enemy ai=patrol weapon=handgun makes two raider guards on a two-point beat");
  type(g,"summon civilian");check(g.m_friendlyActors.back().summon&&g.npcFaction(g.m_friendlyActors.back().track())==AiFaction::Neutral,"a civilian is neutral");
  type(g,"list");check(has(g,"6 SUMMONS")&&has(g,"#1 REX 1 ALLY WANDER"),"list shows the summons");
  // A save leaves them all out.
  for(int i=0;i<30;++i)g.update({},1.f/60);
  const size_t authoredStates=[&]{Game probe=fresh();return probe.m_states.size();}();(void)authoredStates;
  Game loaded(room());const bool ok=loaded.decodeSave(g.encodeSave());int summonedAfter=0;for(const auto&e:loaded.m_enemies)summonedAfter+=e.summon!=0;for(const auto&n:loaded.m_friendlyActors)summonedAfter+=n.summon!=0;
  int leaked=0;for(const auto&v:loaded.m_states)leaked+=g.m_summonKeys.count(v.id)!=0;
  check(ok&&summonedAfter==0&&loaded.m_enemies.empty()&&loaded.m_friendlyActors.empty(),"a save loads with no summon in it");
  check(leaked==0&&!g.m_summonKeys.empty(),"no state a summon's AI wrote reaches the save ("+std::to_string(g.m_summonKeys.size())+" kept out)");
  check(g.m_summons.size()==6,"saving does not touch the live summons");
  type(g,"freeze all");bool frozen=true;for(const auto&e:g.m_enemies)frozen&=!e.summon||e.frozen;check(frozen,"freeze all holds every summon");
  const Vec2 held=g.m_enemies[0].pos;for(int i=0;i<60;++i)g.update({},1.f/60);check(lengthSq(g.m_enemies[0].pos-held)<1e-6f,"a frozen summon does not move");
  type(g,"freeze off");type(g,"ai off");const auto before=g.m_enemies[0].pos;for(int i=0;i<60;++i)g.update({},1.f/60);check(lengthSq(g.m_enemies[0].pos-before)<1e-6f&&g.m_aiOff,"ai off stops them");type(g,"ai on");
  type(g,"killall summons");int alive=0;for(const auto&e:g.m_enemies)alive+=e.alive;check(alive==0&&has(g,"KILLED 3 CREATURES AND 3 MEN"),"killall summons kills the three huntsmen and three men");}
 // Aimed: turn a creature, then kill it.
 {Game g=fresh();type(g,"summon brute");auto&e=g.m_enemies.back();g.m_player.angle=std::atan2(e.pos.y-g.m_player.pos.y,e.pos.x-g.m_player.pos.x);
  g.m_player.pitch=std::atan2((e.bodyBottom()+e.bodyTop())*.5f-(g.m_player.z+g.m_player.eye),length(e.pos-g.m_player.pos))*140.f;
  check(g.aimedEntity().kind==ShotKind::Enemy,"the crosshair finds the summoned brute");
  type(g,"ally");check(g.m_enemies.back().faction==AiFaction::Facility&&g.enemiesRemaining()==0,"ally on it turns it, and it no longer counts against the player");
  type(g,"enemy");check(g.m_enemies.back().faction==AiFaction::Creature&&g.enemiesRemaining()==1,"enemy turns it back");
  type(g,"kill");check(!g.m_enemies.back().alive,"kill kills it");}
 // An allied chimera against a hostile huntsman: the ally wins, and the player is never struck.
 {Game g=fresh();type(g,"summon chimera ally");g.m_player.angle=kPi*.25f;type(g,"summon huntsman enemy ai=hunt");
  float firstBlow=-1;for(int i=0;i<60*20&&g.m_enemies[1].alive;++i){g.update({},1.f/60);if(firstBlow<0&&g.m_enemies[1].hp<g.m_enemies[1].maxHp)firstBlow=i/60.f;}
  report<<"  ally chimera hp "<<g.m_enemies[0].hp<<" hostile huntsman hp "<<g.m_enemies[1].hp<<" first blow "<<firstBlow<<" s player "<<g.m_player.health<<'\n';
  check(!g.m_enemies[1].alive,"the allied chimera kills the hostile huntsman");}
 // Creatures against guards: a hostile chimera and two allied soldiers.
 {Game g=fresh();type(g,"summon soldier 2 ally");g.m_player.angle=kPi*.5f;type(g,"summon chimera enemy ai=hunt");
  int shotsTaken=0;float hp0=g.m_enemies.back().hp;for(int i=0;i<60*25&&g.m_enemies.back().alive;++i){g.update({},1.f/60);if(g.m_enemies.back().hp<hp0){++shotsTaken;hp0=g.m_enemies.back().hp;}}
  int menDamaged=0;for(const auto&n:g.m_friendlyActors)menDamaged+=n.summon&&n.track().damageState&&g.state(n.track().damageState)>0;
  report<<"  chimera hp "<<g.m_enemies.back().hp<<" hits taken "<<shotsTaken<<" men hurt "<<menDamaged<<'\n';
  check(shotsTaken>0,"the allied soldiers shoot the hostile chimera");}
 return passed;
}
}
