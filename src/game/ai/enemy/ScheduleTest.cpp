#include "../../Game.h"
#include "ScheduleTables.h"
#include "../Perception.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>
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
 {auto g=scene(Enemy::Kind::Snatcher,6);auto&e=g.m_enemies[0];bool sight=visible(g),watch=true,progress=false;float firstOther=-1;int last=-1;std::ostringstream timeline;
  const char*names[]={"Watch","Flank","Rush","Retreat"};
  for(int i=1;i<=600;++i){g.updateEnemies(dt);int mode=int(e.stalkMode);
   if(mode!=last){timeline<<i*dt<<"s="<<names[mode]<<" ";last=mode;}
   if(i<=100&&e.stalkMode!=Enemy::StalkMode::Watch)watch=false;
   if(e.stalkMode!=Enemy::StalkMode::Watch&&firstOther<0)firstOther=i*dt;
   progress|=length(e.pos-g.m_player.pos)<=creatureTraits(e.kind).reach||e.stalkMode==Enemy::StalkMode::Flank||e.stalkMode==Enemy::StalkMode::Rush;
  }
  std::ostringstream v;v<<"player visible="<<sight<<" Watch for first 1s="<<watch<<" first non-Watch="<<firstOther<<"s distance at 6s="<<length(e.pos-g.m_player.pos)<<" timeline="<<timeline.str();
  check(sight&&watch&&progress,"4 Snatcher stalks under continuous gaze",v.str());
 }
 {auto g=scene(Enemy::Kind::Snatcher,6);auto&e=g.m_enemies[0];
  // Seed an already active Rush to measure its pace independently of entry rules.
  e.awareness=12;e.lastKnown=g.m_player.pos;e.stalkMode=Enemy::StalkMode::Rush;e.stalkTimer=1.65f;
  auto p=e.pos;bool rush=true;for(int i=0;i<25;++i){g.updateEnemies(dt);rush&=e.stalkMode==Enemy::StalkMode::Rush;}
  float displacement=length(e.pos-p),speed=displacement/.25f;std::ostringstream v;v<<"Rush throughout="<<rush<<" displacement="<<displacement<<"m / 0.25s speed="<<speed<<"m/s (3.8..5.2)";
  check(rush&&speed>=3.8f&&speed<=5.2f,"5 Snatcher Rush pace",v.str());
 }
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
