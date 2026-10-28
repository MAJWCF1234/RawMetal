#include "../../Game.h"
#include "ScheduleTables.h"
#include "../Perception.h"
#include "../Navigation.h"
#include "../RouteCache.h"
#include "../../../core/FrameProfile.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <bit>
#include <cstring>
#include <random>
#include <tuple>
namespace retro {
bool Game::testCreatureBehaviour(){
 const auto started=std::chrono::steady_clock::now();
 std::ofstream report("creature-behaviour-test.txt");bool passed=true;
 auto check=[&](bool ok,const char*name,const std::string&values){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<values<<'\n';report.flush();passed&=ok;};
 auto near=[](float a,float b){return std::fabs(a-b)<.0001f;};
 constexpr float dt=.01f;
 // Use the existing validation room's open intake aisle; updateEnemies isolates
 // creature decisions from player input, scripts and unrelated simulation.
 auto scene=[](Enemy::Kind kind,float distance){auto g=validationScene(kind);
  g.m_player.pos={3.5f,4.5f};g.m_player.z=0;g.m_player.angle=0;g.m_player.pitch=0;
  auto&e=g.m_enemies[0];e.pos={3.5f+distance,4.5f};e.z=0;e.home=e.lastKnown=e.pos;e.heading=kPi;e.awareness=0;e.moving=false;
  g.m_sounds.clear();return g;};
 auto visible=[](const Game&g){const auto&e=g.m_enemies[0];return aiCanSee(g.m_world,e.pos,e.z+.7f,{std::cos(e.heading),std::sin(e.heading)},g.m_player.pos,g.m_player.z+g.m_player.eye,11.f,-.25f,2.5f);};
 for(int i=0;i<5;++i){const auto kind=Enemy::Kind(i);const auto&t=creatureTraits(kind);
  constexpr float health[]={110,85,280,320,180},reach[]={1.05f,1.05f,1.25f,1.1f,1.05f};
  std::ostringstream v;v<<t.name<<" health="<<t.health<<" reach="<<t.reach<<" gaitPerMetre="<<t.gaitPerMetre<<" painDecay="<<t.painDecay<<" deathSeconds="<<t.deathSeconds<<" idleSeconds="<<t.idleSeconds<<" rushSpeed="<<t.rushSpeed;
  check(near(t.health,health[i])&&near(t.reach,reach[i])&&near(t.gaitPerMetre,7)&&near(t.painDecay,5)&&near(t.deathSeconds,1.15f)&&near(t.idleSeconds,2.5f)&&near(t.rushSpeed,i==3?3.8f:0),"1 traits",v.str());
 }
 const StateId wake=stateId("creature_behaviour_test_f7c4b08_wake");
 auto dormant=[&]{auto g=scene(Enemy::Kind::Snatcher,3);g.m_player.angle=kPi;g.m_enemies.clear();
  CreatureSpawn spawn{Enemy::Kind::Snatcher,{6.5f,4.5f},0};spawn.wakeState=wake;g.spawnCreature(spawn,0,false);
  // Offset the heading while keeping the player visible, so a turn is detectable.
  g.m_enemies[0].heading=kPi-.3f;return g;};
 auto still=[&](Game&g,float&movement,float&turn,float&awareness){auto&e=g.m_enemies[0];auto p=e.pos;float h=e.heading;movement=turn=awareness=0;
  for(int i=0;i<300;++i){g.updateEnemies(dt);movement=std::max(movement,length(e.pos-p));turn=std::max(turn,std::fabs(wrapAngle(e.heading-h)));awareness=std::max(awareness,e.awareness);}
 };
 {auto g=dormant();auto&e=g.m_enemies[0];bool sight=visible(g);float movement,turn,awareness;still(g,movement,turn,awareness);
  std::ostringstream v;v<<"player visible="<<sight<<" 3s max displacement="<<movement<<" turn="<<turn<<" awareness="<<awareness<<" wakeState="<<e.wakeState;
  check(sight&&movement<.0001f&&turn<.0001f&&awareness==0&&e.wakeState==wake,"2 dormancy",v.str());
  float before=length(e.pos-g.m_player.pos),awake=-1;g.setState(wake,1);
  for(int i=1;i<=200;++i){g.updateEnemies(dt);if(awake<0&&e.awareness>0&&!e.wakeState)awake=i*dt;}
  float after=length(e.pos-g.m_player.pos);std::ostringstream w;w<<"awareness at="<<awake<<"s distance="<<before<<" -> "<<after<<" in 2s";
  check(awake>0&&awake<=.1f&&after<before-.05f,"2 state wakes and approaches",w.str());
 }
 {auto g=dormant();auto&e=g.m_enemies[0];bool sight=visible(g);e.hp-=1;float awake=-1;
  for(int i=1;i<=10;++i){g.updateEnemies(dt);if(awake<0&&e.awareness>0&&!e.wakeState)awake=i*dt;}
  std::ostringstream v;v<<"hp="<<e.hp<<" maxHp="<<e.maxHp<<" awareness="<<e.awareness<<" awake at="<<awake<<"s";
  check(sight&&awake>0&&awake<=.1f,"3 damage wakes",v.str());
 }
 // The ambusher checks replace the former stare/chase and seeded Rush tests.
 passed=testSnatcherAmbush(report)&&passed;
 {auto g=scene(Enemy::Kind::Chimera,1.4f);auto&e=g.m_enemies[0];float begin=-1,first=-1,second=-1,windupSeconds=0,damage=0;int hits=0;bool positive=visible(g);
  for(int i=1;i<=400;++i){float hp=g.m_player.health;g.updateEnemies(dt);float t=i*dt;
   if(begin<0&&e.windup>0)begin=t;
   if(first<0&&begin>=0&&e.windup<=0&&g.m_player.health==hp)positive=false;
   if(g.m_player.health<hp){++hits;if(first<0){first=t;damage=hp-g.m_player.health;windupSeconds=first-begin;}else if(second<0)second=t;}
   if(first>=0&&second<0&&t>=first+1.8f&&hits!=1)positive=false;
  }
  std::ostringstream v;v<<"windup start="<<begin<<"s duration="<<windupSeconds<<"s first strike="<<first<<"s damage="<<damage<<" second strike="<<second<<"s interval="<<(second-first)<<"s hits over 4s="<<hits<<" distance="<<length(e.pos-g.m_player.pos);
  check(begin>=0&&positive&&windupSeconds>=.8f&&windupSeconds<=1.f&&near(damage,28)&&first>=0&&(second<0||second-first>=1.8f),"6 Chimera windup hit and cooldown",v.str());
 }
 {auto source=dormant();source.m_enemies[0].scriptSpawned=true;Game loaded;bool decoded=loaded.decodeSave(source.encodeSave());
  // Loading restores the authored population too. Identify the dynamic test
  // spawn by kind, ownership and home, then isolate it for the update probe.
  int matches=0;Enemy saved{};
  if(decoded)for(const auto&e:loaded.m_enemies)if(e.kind==Enemy::Kind::Snatcher&&e.scriptSpawned&&lengthSq(e.home-Vec2{6.5f,4.5f})<.0001f){++matches;saved=e;}
  bool found=matches==1;if(found)loaded.m_enemies={saved};
  float movement=-1,turn=-1,awareness=-1,after=-1;StateId gate=found?saved.wakeState:0;
  if(found)still(loaded,movement,turn,awareness);
  bool sleeping=found&&gate==wake&&loaded.state(wake)==0&&movement<.0001f&&turn<.0001f&&awareness==0;
  if(found){auto p=loaded.m_enemies[0].pos;loaded.setState(wake,1);for(int i=0;i<200;++i)loaded.updateEnemies(dt);after=length(loaded.m_enemies[0].pos-p);}
  std::ostringstream v;v<<"decode="<<decoded<<" Snatcher found="<<found<<" saved wakeState="<<gate<<" max displacement/turn/awareness over 3s="<<movement<<'/'<<turn<<'/'<<awareness<<" displacement after state +2s="<<after;
  check(sleeping&&after>.05f,"7 save/load dormancy and state wake",v.str());
 }
 // The route cache keeps each deck graph and terrain cell grid per exact
 // collision state; what it hands a planner must be the live probe to the bit.
 // Every multi-deck campaign chunk and every fourth one-floor chunk (a guard's
 // fallback walks the same graph there), seeded start/goal pairs on real spans,
 // one hull per state (a creature's or a standing or crouched man's) so later
 // pairs walk edges the earlier ones kept, each pair asked twice; doors as
 // built, half open, open, and shut under an operable door plan; lift, cargo
 // lift and platform moved; a crate shoved; all put back.
 {std::mt19937 random(0x7c4b08u);const auto counted=perf::counts;int compared=0,differ=0,chunks=0,states=0;std::string first;bool unowned=false,unownedAsked=false;
  std::vector<float> hulls{1.78f,1.78f*.85f};for(int k=0;k<7;++k)hulls.push_back(creatureTraits(CreatureKind(k)).hull);
  auto pick=[&](float low,float high){return std::uniform_real_distribution<float>(low,high)(random);};
  auto bits=[](Vec2 a,Vec2 b){return std::bit_cast<uint32_t>(a.x)==std::bit_cast<uint32_t>(b.x)&&std::bit_cast<uint32_t>(a.y)==std::bit_cast<uint32_t>(b.y);};
  auto compare=[&](const World&w,int level,const char*state,int pairs){++states;const float height=hulls[random()%hulls.size()];
   for(int i=0;i<pairs;++i){bool human=random()%2==0;
    auto spot=[&](Vec2&p,float&z){p={pick(1.f,23.f),pick(1.f,23.f)};auto spans=w.spansAt(int(p.x),int(p.y));z=spans.empty()?0.f:spans[random()%spans.size()].floor;};
    Vec2 start,goal;float startZ,goalZ;spot(start,startZ);spot(goal,goalZ);
    ai::RouteCache::enabled=false;Vec2 live=aiDeckWaypoint(w,start,startZ,goal,goalZ,height,human);ai::RouteCache::enabled=true;
    for(int ask=0;ask<2;++ask){Vec2 kept=aiDeckWaypoint(w,start,startZ,goal,goalZ,height,human);++compared;
     if(!bits(live,kept)){++differ;if(first.empty()){std::ostringstream at;at<<" first: level "<<level<<' '<<state<<" hull "<<height<<" human "<<human<<" ("<<start.x<<','<<start.y<<','<<startZ<<")->("<<goal.x<<','<<goal.y<<','<<goalZ<<") live "<<live.x<<','<<live.y<<" kept "<<kept.x<<','<<kept.y;first=at.str();}}}
   }};
  for(int level=0;level<worldChunkCount(WorldId::Campaign);++level){World w(level,WorldId::Campaign);if(w.hasTerrain()||(w.layers().size()<2&&level%4))continue;++chunks;
   // An authored World no Owner names keeps nothing.
   if(w.authored()&&!unownedAsked){bool fresh=false;unownedAsked=true;unowned=!ai::RouteCache::deck(w,1.78f,fresh);}
   ai::RouteCache::Owner owner(w,w.m_mapData);const int pairs=w.layers().size()>1?3:2;const auto built=w.doors();
   compare(w,level,"as built",pairs);
   for(size_t d=0;d<built.size();++d)w.setDoor(int(d),built[d].maxOpen*.5f,false);if(!built.empty())compare(w,level,"doors half open",pairs);
   for(size_t d=0;d<built.size();++d)w.setDoor(int(d),built[d].maxOpen,false);if(!built.empty())compare(w,level,"doors open",pairs);
   for(size_t d=0;d<built.size();++d)w.setDoor(int(d),0,false);
   if(!built.empty()){std::vector<char> operable(built.size(),0);for(size_t d=0;d<built.size();d+=2)operable[d]=1;World::DoorPlan plan(w,operable);compare(w,level,"doors shut, a door plan through half",pairs);}
   if(w.hasLift()){const float lift=w.m_liftHeight;w.m_liftHeight=lift+2.5f;compare(w,level,"lift raised",pairs);w.m_liftHeight=lift;}
   if(w.cargoLift().x2>w.cargoLift().x1){const float cargo=w.cargoLiftHeight();w.setCargoLiftHeight(w.cargoLift().upper);compare(w,level,"cargo lift up",pairs);w.setCargoLiftHeight(w.cargoLift().lower);compare(w,level,"cargo lift down",pairs);w.setCargoLiftHeight(cargo);}
   for(size_t i=0;i<w.actorTracks().size();++i)if(w.actorTracks()[i].platform){const auto pose=w.actorPose(i);auto moved=pose;moved.position.x+=.6f;moved.z+=.4f;w.setActorPose(i,moved);compare(w,level,"platform moved",pairs);w.setActorPose(i,pose);break;}
   if(!w.m_pushCrates.empty()){const auto crate=w.m_pushCrates[0];w.m_pushCrates[0].position.x+=.7f;compare(w,level,"crate shoved",pairs);w.m_pushCrates[0]=crate;}
   w.restoreDoors(built);compare(w,level,"put back",pairs);
  }
  // Ashfall terrain: every cell edge for a creature's step (no sweep) and the
  // walk a guard's fallback takes (swept), each asked twice, then whole
  // terrain floods from seeded points.
  int edges=0,edgeDiffer=0,floods=0,floodDiffer=0;
  for(int level:{0,7}){World w(level,WorldId::Ashfall);if(!w.hasTerrain())continue;ai::RouteCache::Owner owner(w,w.m_mapData);
   for(auto [height,step,sweep]:{std::tuple{creatureTraits(CreatureKind::Brute).hull,creatureTraits(CreatureKind::Brute).step,false},std::tuple{1.78f,.34f,true}})
    for(int ask=0;ask<2;++ask)for(int cell=0;cell<24*24;++cell)for(int to:{cell+1,cell-1,cell+24,cell-24}){
     if(to<0||to>=24*24||((to==cell+1||to==cell-1)&&to/24!=cell/24))continue;
     ai::RouteCache::enabled=false;bool live=aiCellEdge(w,cell,to,height,step,sweep);ai::RouteCache::enabled=true;
     ++edges;edgeDiffer+=aiCellEdge(w,cell,to,height,step,sweep)!=live;}
   for(int i=0;i<4;++i){Vec2 start{pick(1.f,23.f),pick(1.f,23.f)},goal{pick(1.f,23.f),pick(1.f,23.f)};float feet=w.supportBelow(start.x,start.y,float(World::TerrainMaxZ+1)),goalFeet=w.supportBelow(goal.x,goal.y,float(World::TerrainMaxZ+1));
    ai::RouteCache::enabled=false;Vec2 live=aiWalkWaypoint(w,start,feet,goal,goalFeet,1.78f);ai::RouteCache::enabled=true;
    for(int ask=0;ask<2;++ask){++floods;floodDiffer+=!bits(live,aiWalkWaypoint(w,start,feet,goal,goalFeet,1.78f));}}
  }
  // Whole creature runs, kept against live, frame by frame: six Huntsmen held
  // on the player across the Foundry's decks, and a Brute over Ashfall ground.
  auto trace=[&](bool keep,bool terrain){ai::RouteCache::enabled=keep;std::vector<float> out;
   if(terrain){Game g(WorldId::Ashfall);g.m_enemies.clear();g.m_sounds.clear();g.m_player.pos={20.5f,12.5f};g.m_player.z=g.world().supportBelow(20.5f,12.5f,float(World::TerrainMaxZ+1));
    Enemy brute{};brute.kind=Enemy::Kind::Brute;brute.pos={6.5f,12.5f};brute.z=g.world().supportBelow(6.5f,12.5f,float(World::TerrainMaxZ+1));brute.home=brute.pos;brute.lastKnown=g.m_player.pos;brute.lastKnownZ=g.m_player.z;brute.awareness=40.f;brute.state=Enemy::State::Investigate;g.m_enemies.push_back(brute);
    for(int f=0;f<600;++f){g.updateEnemies(1.f/120.f);for(const auto&e:g.m_enemies)out.insert(out.end(),{e.pos.x,e.pos.y,e.z,e.waypoint.x,e.waypoint.y});}}
   else{auto g=perfScene(0,{NAN,NAN},-999,0,0);g.perfClearCreatures();g.perfSpawn(CreatureKind::Huntsman,6,4.f);
    for(int f=0;f<240;++f){g.perfWake(false);g.updateEnemies(1.f/60.f);for(const auto&e:g.m_enemies)out.insert(out.end(),{e.pos.x,e.pos.y,e.z,e.waypoint.x,e.waypoint.y,e.repathTimer});}}
   ai::RouteCache::enabled=true;return out;};
  bool runsSame=true;for(bool terrain:{false,true}){auto live=trace(false,terrain),kept=trace(true,terrain);runsSame&=live.size()==kept.size()&&!live.empty()&&std::memcmp(live.data(),kept.data(),live.size()*sizeof(float))==0;}
  const auto hits=perf::counts[perf::RouteCacheHits]-counted[perf::RouteCacheHits],misses=perf::counts[perf::RouteCacheMisses]-counted[perf::RouteCacheMisses];
  std::ostringstream v;v<<"chunks="<<chunks<<" states="<<states<<" deck waypoints="<<compared<<" differ="<<differ<<" terrain edges="<<edges<<" differ="<<edgeDiffer<<" floods="<<floods<<" differ="<<floodDiffer
   <<" whole runs identical="<<runsSame<<" cache hits="<<hits<<" misses="<<misses<<" unowned kept nothing="<<unowned<<first;
  check(chunks>40&&compared>500&&differ==0&&edges>=8000&&edgeDiffer==0&&floods>0&&floodDiffer==0&&runsSame&&hits>0&&misses>0&&unowned,"8 route cache answers are the live planner's to the bit",v.str());
 }
 report<<"runtime_seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<'\n';
 return passed&&bool(report);
}

bool Game::testCreatureSchedule(){
 std::ofstream report("creature-schedule-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};

 {bool valid=validCreatureScheduleCatalog()&&creatureScheduleCatalog.size()==7,distinct=true;
  for(size_t id=0;id<creatureScheduleCatalog.size();++id){const auto&row=creatureScheduleDefinition(CreatureSchedule(id));valid&=row.schedule.id==int(id)&&ai::validScheduleDefinition(row.schedule)&&*row.schedule.name;
   for(size_t prior=0;prior<id;++prior)distinct&=std::string_view(row.schedule.name)!=creatureScheduleCatalog[prior].schedule.name;}
  check(valid&&distinct,"Creature schedule catalog exposes seven valid distinct named task lists with explicit failure destinations");
  const auto&idle=creatureScheduleDefinition(CreatureSchedule::Idle);const auto&chase=creatureScheduleDefinition(CreatureSchedule::Chase);const auto&attack=creatureScheduleDefinition(CreatureSchedule::Attack);const auto&search=creatureScheduleDefinition(CreatureSchedule::Search);
  check(idle.schedule.tasks.size()==3&&idle.schedule.tasks[0].operation==ai::TaskOperation::Prepare&&idle.schedule.tasks[1].operation==ai::TaskOperation::Move&&idle.schedule.tasks[2].operation==ai::TaskOperation::ExecuteWork,
   "Idle walks home through Prepare/Move before Hold work");
  check(chase.schedule.tasks.size()==3&&chase.work==(creatureTaskBit(CreatureTask::Hold)|creatureTaskBit(CreatureTask::Strike))&&attack.schedule.tasks.size()==1&&attack.schedule.tasks[0].operation==ai::TaskOperation::ExecuteWork&&attack.work==creatureTaskBit(CreatureTask::Strike),
   "Chase owns approach work while Attack is a dedicated melee ExecuteWork list");
  check(search.schedule.tasks.size()==2&&search.schedule.tasks[1].operation==ai::TaskOperation::Wait&&search.schedule.failureSchedule==int(CreatureSchedule::Idle),
   "Search holds in place with a Wait task and recovers to Idle");
 }

 {struct Adapter{std::vector<int> starts,runs;int charges=0;
  void start(const ai::ScheduleTask&task,ai::SchedulerState&){starts.push_back(int(task.operation));}
  ai::TaskStatus run(const ai::ScheduleTask&task,ai::SchedulerState&state){
   runs.push_back(int(task.operation));
   if(task.operation==ai::TaskOperation::Prepare)return ai::TaskStatus::Complete;
   if(task.operation==ai::TaskOperation::Move)return ai::TaskStatus::Complete;
   if(task.operation==ai::TaskOperation::Wait)return state.elapsedMs>=task.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
   ++charges;return state.elapsedMs>=50?ai::TaskStatus::Complete:ai::TaskStatus::Running;
  }};
  Adapter actor;ai::SchedulerState state;const auto&definition=creatureScheduleDefinition(CreatureSchedule::Chase).schedule;
  auto result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::Running&&state.cursor==2&&state.status==ai::TaskStatus::Running&&actor.starts.size()==3,
   "Creature Chase MaintainSchedule advances Prepare and Move synchronously into Strike work");
  state.elapsedMs=50;result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::ScheduleDone&&actor.charges>=1,
   "Creature Chase Strike completes through the shared bounded executor");
 }

 {auto g=validationScene(Enemy::Kind::Huntsman);auto&e=g.m_enemies[0];
  e.pos={4.5f,9.5f};e.home=e.lastKnown=e.pos;e.heading=-kPi*.5f;g.m_player.pos={4.5f,6.5f};g.m_sounds.clear();
  g.m_sounds.push_back({Sound::Concrete1,{6.5f,9.5f},1.f,1,true});g.updateEnemies(.01f);
  check(e.state==Enemy::State::Investigate&&e.schedule==int(CreatureSchedule::Investigate)&&e.scheduleCursorSaved&&e.scheduleStep>=1,
   "Hearing adopts Investigate through the creature schedule executor");
 }

 {auto g=validationScene(Enemy::Kind::Mutant);auto&body=g.m_enemies[0];body.pos=g.m_player.pos+Vec2{.7f,0};body.heading=kPi;
  g.updateEnemies(.01f);
  check(body.windup>0&&(body.schedule==int(CreatureSchedule::Attack)||g.creatureTaskAllows(body,CreatureTask::Strike)),
   "Close mutant contact enters Attack/Strike ownership before the committed swing");
  for(int i=0;i<72;++i)g.updateEnemies(1.f/120.f);
  check(g.player().health<100&&g.player().health>=70,"Scheduled mutant melee still lands contact damage");
 }

 {auto g=validationScene(Enemy::Kind::Huntsman);auto&e=g.m_enemies[0];e.pos={4.5f,9.5f};e.home=e.pos;e.lastKnown=e.pos;e.heading=-kPi*.5f;g.m_player.pos={4.5f,6.5f};
  for(int i=0;i<30;++i)g.updateEnemies(1.f/60.f);
  check(e.state==Enemy::State::Idle&&e.schedule==int(CreatureSchedule::Idle)&&e.scheduleStep==2&&e.scheduleTask==int(CreatureTask::Hold),
   "Idle creatures settle into Hold work after the shared Prepare/Move list");
  InputState fire{};fire.fire=true;g.update(fire,.02f);
  check(e.state==Enemy::State::Investigate&&e.schedule==int(CreatureSchedule::Investigate),"Gunfire interrupts Idle Hold into Investigate ownership");
 }

 {Game rough(WorldId::Ashfall);rough.m_enemies.clear();rough.m_sounds.clear();
  rough.m_player.pos={20.5f,12.5f};rough.m_player.z=rough.world().supportBelow(20.5f,12.5f,float(World::TerrainMaxZ+1));rough.m_player.angle=0;
  Enemy climber{};climber.kind=Enemy::Kind::Brute;climber.pos={6.5f,12.5f};climber.z=rough.world().supportBelow(6.5f,12.5f,float(World::TerrainMaxZ+1));climber.home=climber.pos;climber.lastKnown=rough.m_player.pos;climber.lastKnownZ=rough.m_player.z;climber.awareness=40.f;climber.state=Enemy::State::Investigate;climber.heading=0;
  rough.m_enemies.push_back(climber);
  for(int i=0;i<3600&&length(rough.m_enemies[0].pos-rough.m_player.pos)>1.5f;++i)rough.updateEnemies(1.f/120.f);
  auto&done=rough.m_enemies[0];float support=rough.world().supportBelow(done.pos.x,done.pos.y,float(World::TerrainMaxZ+1));
  report<<"brute full dist="<<length(done.pos-rough.m_player.pos)<<" z="<<done.z<<" support="<<support<<" state="<<int(done.state)<<" schedule="<<done.schedule<<'\n';
  check(length(done.pos-rough.m_player.pos)<=1.8f&&std::fabs(done.z-support)<=.05f,"Brute completes Ashfall rough-terrain pursuit under MaintainSchedule");
 }

 report<<"checks "<<checks<<'\n';return passed;
}
}
