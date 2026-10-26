#include "EngineRegressionScene.h"
#include <fstream>
#include <sstream>
#include <limits>

namespace retro {
bool Game::testSnatcherAmbush(std::ostream& report){
 bool passed=true;constexpr float dt=.01f;using Mode=Enemy::StalkMode;
 auto check=[&](bool ok,const char*name,const std::string&v){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<v<<'\n';report.flush();passed&=ok;};
 auto scene=[](float distance){Game g(regression::campaign(regression::room()));g.m_player.pos={5,12};g.m_player.angle=kPi;g.m_player.z=0;
  g.spawnCreature({Enemy::Kind::Snatcher,{5+distance,12},0},0,false);g.m_enemies[0].heading=kPi;g.m_enemies[0].scriptSpawned=true;return g;};
 struct Trace {int last=-1;bool tell=false;std::ostringstream text;
  void sample(const Enemy&e,float t){int mode=int(e.stalkMode);if(mode!=last||(e.windup>0)!=tell){const char*names[]={"Watch","Flank","Rush","Retreat","Grab","Recover"};text<<t<<"s="<<names[mode]<<"(windup="<<e.windup<<",dpos="<<e.pos.x<<","<<e.pos.y<<") ";last=mode;tell=e.windup>0;}}
 };
 auto saveMode=[&](const Game&g,Mode wanted,const std::string&timeline){Game loaded(g.m_customCampaign);bool decoded=loaded.decodeSave(g.encodeSave());int found=0,mode=-1;
  if(decoded)for(const auto&e:loaded.m_enemies)if(e.scriptSpawned&&e.kind==Enemy::Kind::Snatcher){++found;mode=int(e.stalkMode);}
  check(decoded&&found==1&&mode==int(wanted),wanted==Mode::Grab?"11h save during Grab":"11h save during Recover","decoded="+std::to_string(decoded)+" count="+std::to_string(found)+" mode="+std::to_string(mode)+" timeline="+timeline);
 };
 {auto g=scene(6);g.m_player.angle=0;auto&e=g.m_enemies[0];float closest=6,retreat=-1;Trace trace;bool sight=g.m_world.rayClear(g.m_player.pos,g.m_player.eye,e.pos,.85f);
  for(int i=1;i<=400;++i){g.m_sounds.clear();g.updateEnemies(dt);trace.sample(e,i*dt);closest=std::min(closest,length(e.pos-g.m_player.pos));if(retreat<0&&e.stalkMode==Mode::Retreat)retreat=i*dt;}
  float end=length(e.pos-g.m_player.pos);bool hidden=!g.m_world.rayClear(g.m_player.pos,g.m_player.eye,e.pos,e.z+.85f);
  std::ostringstream v;v<<"LOS="<<sight<<" min distance="<<closest<<" retreat at="<<retreat<<" end distance="<<end<<" hidden="<<hidden<<" timeline="<<trace.text.str();
  check(sight&&closest>=5&&retreat>=0&&retreat<=.5f&&(end>6.05f||hidden),"11a watched from 6m retreats",v.str());
 }
 {auto g=scene(6);auto&e=g.m_enemies[0];Trace trace;std::vector<Vec2> samples{e.pos};float maxWindow=0,tell=-1,tellDistance=-1;
  for(int i=1;i<=1200;++i){g.m_sounds.clear();g.updateEnemies(dt);trace.sample(e,i*dt);samples.push_back(e.pos);
   if(e.windup>0){tell=i*dt;tellDistance=length(e.pos-g.m_player.pos);break;}
   if(i>=50)maxWindow=std::max(maxWindow,length(samples[i]-samples[i-50])/.5f);
  }
  std::ostringstream v;v<<"max 0.5s speed="<<maxWindow<<" tell at="<<tell<<" distance="<<tellDistance<<" timeline="<<trace.text.str();
  check(maxWindow<=1.3f&&maxWindow>.1f&&tell>=0&&tellDistance<=2.8f,"11b unwatched creep and tell",v.str());
 }
 // Natural tell entry, followed by a dodge at Rush entry: measure the full missed lunge.
 {auto g=scene(2);auto&e=g.m_enemies[0];Trace trace;float tell=-1,rush=-1,rushEnd=-1,recover=-1,recoverEnd=-1,minSpeed=999,maxSpeed=0,heading=0,turn=0,damageBonus=-1;int rushSamples=0;bool dodged=false,saved=false;
  for(int i=1;i<=350;++i){auto before=e.pos;Mode old=e.stalkMode;g.m_sounds.clear();g.updateEnemies(dt);float time=i*dt;trace.sample(e,time);
   if(tell<0&&e.windup>0)tell=time;
   if(rush<0&&e.stalkMode==Mode::Rush){rush=time;heading=e.heading;g.m_player.pos.y+=1.6f;dodged=true;}
   if(old==Mode::Rush&&e.stalkMode==Mode::Rush){float speed=length(e.pos-before)/dt;minSpeed=std::min(minSpeed,speed);maxSpeed=std::max(maxSpeed,speed);turn=std::max(turn,std::fabs(wrapAngle(e.heading-heading)));++rushSamples;}
   if(rush>=0&&rushEnd<0&&e.stalkMode!=Mode::Rush)rushEnd=time;
   if(recover<0&&e.stalkMode==Mode::Recover){recover=time;saveMode(g,Mode::Recover,trace.text.str());saved=true;float hp=e.hp;g.applyEnemyDamage(e,10,false,false);damageBonus=hp-e.hp;}
   if(recover>=0&&e.stalkMode!=Mode::Recover){recoverEnd=time;break;}
  }
  std::ostringstream v;v<<"tell="<<rush-tell<<" rush="<<rushEnd-rush<<" speed min/max="<<minSpeed<<"/"<<maxSpeed<<" moving samples="<<rushSamples<<" heading drift="<<turn<<" timeline="<<trace.text.str();
  check(tell>=0&&rush>=0&&rush-tell>=.6f&&rush-tell<=.7f&&rushEnd>rush&&rushEnd-rush<=.5f&&rushSamples>0&&minSpeed>=4.2f&&maxSpeed<=5&&turn<.001f,"11c tell and straight lunge",v.str());
  std::ostringstream w;w<<"dodged="<<dodged<<" health="<<g.m_player.health<<" Recover duration="<<recoverEnd-recover<<" damage from 10hp="<<damageBonus<<" timeline="<<trace.text.str();
  check(dodged&&saved&&g.m_player.health==100&&recoverEnd-recover>=1.2f&&recoverEnd-recover<=1.3f&&std::fabs(damageBonus-15)<.001f,"11e sideways miss and vulnerable Recover",w.str());
 }
 {auto g=scene(2);auto&e=g.m_enemies[0];Trace trace;bool caught=false,fade=false;float held=0,maxOther=0,normalWalk=-1,heldWalk=-1;Vec2 playerStart=g.m_player.pos;float healthStart=g.m_player.health;
  for(int i=1;i<=250;++i){auto before=e.pos;Mode old=e.stalkMode;g.m_sounds.clear();g.updateEnemies(dt);trace.sample(e,i*dt);
   if(old!=Mode::Rush&&e.stalkMode!=Mode::Rush)maxOther=std::max(maxOther,length(e.pos-before)/dt);
   if(!caught&&e.stalkMode==Mode::Grab){caught=true;held=g.m_grabbed;saveMode(g,Mode::Grab,trace.text.str());
    auto slow=g,normal=g;slow.m_velocity=normal.m_velocity={};normal.m_grabbed=0;InputState input;input.forward=true;
    auto start=g.m_player.pos;for(int frame=0;frame<20;++frame){slow.updateMovement(input,dt);normal.updateMovement(input,dt);}heldWalk=length(slow.m_player.pos-start)/.2f;normalWalk=length(normal.m_player.pos-start)/.2f;
   }
   if(caught&&e.stalkMode==Mode::Retreat){fade=true;break;}
  }
  float haul=length(g.m_player.pos-playerStart);float toward=dot(g.m_player.pos-playerStart,normalized(e.pos-playerStart));
  std::ostringstream v;v<<"caught="<<caught<<" damage="<<healthStart-g.m_player.health<<" haul="<<haul<<" toward endpoint="<<toward<<" grabbed="<<held<<" walk held/free="<<heldWalk<<"/"<<normalWalk<<" fade="<<fade<<" max nonRush="<<maxOther<<" timeline="<<trace.text.str();
  check(caught&&std::fabs(healthStart-g.m_player.health-14)<.001f&&haul>=.4f&&toward>=.4f&&held>0&&heldWalk<normalWalk*.6f&&fade&&maxOther<=2.3f,"11d catch haul slow and fade",v.str());
 }
 {auto g=scene(2);auto&e=g.m_enemies[0];Trace trace;bool hit=false,recover=false,rush=false;float hp=0;
  for(int i=1;i<=220;++i){g.m_sounds.clear();g.updateEnemies(dt);trace.sample(e,i*dt);
   if(!hit&&e.windup>0){hp=e.hp;g.applyEnemyDamage(e,10,false,false);hit=true;}
   if(hit){recover|=e.stalkMode==Mode::Recover;rush|=e.stalkMode==Mode::Rush;}
  }
  std::ostringstream v;v<<"hit during tell="<<hit<<" enemy hp="<<hp<<" -> "<<e.hp<<" Recover="<<recover<<" Rush="<<rush<<" player health="<<g.m_player.health<<" timeline="<<trace.text.str();
  check(hit&&recover&&!rush&&g.m_player.health==100,"11f tell hit cancels lunge",v.str());
 }
 {auto g=scene(3);auto&e=g.m_enemies[0];g.m_player.pos={15,4};e.pos=e.home={18,4};e.lastKnown=g.m_player.pos;Trace trace;float start=length(e.pos-g.m_player.pos),closest=start,maxOther=0;bool attacked=false;
  // Two straight legs keep the entire 28.8m escape within the authored room.
  for(int i=1;i<=800;++i){Vec2 velocity=i<=300?Vec2{-3.6f,0}:Vec2{0,3.6f};g.m_player.pos+=velocity*dt;g.m_player.angle=std::atan2(velocity.y,velocity.x);auto before=e.pos;auto old=e.stalkMode;
   g.m_sounds.clear();g.updateEnemies(dt);trace.sample(e,i*dt);closest=std::min(closest,length(e.pos-g.m_player.pos));if(old!=Mode::Rush&&e.stalkMode!=Mode::Rush)maxOther=std::max(maxOther,length(e.pos-before)/dt);attacked|=e.stalkMode==Mode::Grab;
  }
  float end=length(e.pos-g.m_player.pos);std::ostringstream v;v<<"player speed=3.6 duration=8 start/end distance="<<start<<"/"<<end<<" min="<<closest<<" max nonRush="<<maxOther<<" grabbed="<<attacked<<" health="<<g.m_player.health<<" timeline="<<trace.text.str();
  check(end>start&&!attacked&&g.m_player.health==100&&maxOther<=2.3f,"11g cannot chase down running player",v.str());
 }
 return passed;
}
bool Game::testEngineRegression(){
 std::ofstream report("engine-regression-test.txt");bool passed=true;
 auto check=[&](bool ok,const char*name,const std::string&v){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<v<<'\n';report.flush();passed&=ok;};
 // Keep independent groups running after a failed assertion.
 passed=testEngineRegressionSaves(report)&&passed;
 {Game g(regression::campaign(regression::room()));g.m_peaceful=true;g.m_enemies.clear();ScriptAction a;a.enemyKind=Enemy::Kind::Mutant;a.position={9,9};
  g.spawnScriptEnemy(a);size_t empty=g.m_enemies.size();Enemy sentinel{};g.m_enemies.push_back(sentinel);g.spawnScriptEnemy(a);
  check(empty==0&&g.m_enemies.size()==1&&!g.m_enemies[0].scriptSpawned,"2a peaceful script spawn","empty count="+std::to_string(empty)+" sentinel count="+std::to_string(g.m_enemies.size())+" marked="+std::to_string(g.m_enemies[0].scriptSpawned));
 }
 {Game g(regression::campaign(regression::room()));CreatureSpawn s{Enemy::Kind::Mutant,{9,9},.5f};s.scale=1.5f;g.spawnCreature(s,0,false);const auto&e=g.m_enemies.back();const auto&t=creatureTraits(e.kind);
  float top=(e.bodyTop()-e.z)/t.top,hull=e.hull()/t.hull,reach=e.reach()/t.reach;
  check(std::fabs(top-1.5f)<.001f&&std::fabs(hull-1.5f)<.001f&&std::fabs(reach-1.5f)<.001f,"2b scaled creature","top/hull/reach ratios="+std::to_string(top)+"/"+std::to_string(hull)+"/"+std::to_string(reach));
 }
 {auto m=regression::room();TimedSequence t;t.timerState=stateId("reg_clock");t.finishState=stateId("reg_finish");t.durationMs=2000;t.finishAtMs=1;t.soundIntervalMs=0;m->timedSequences.push_back(t);
  for(int time:{0,1000,2000}){SequenceCue cue;cue.timerState=t.timerState;cue.timeMs=time;cue.position={6,6};cue.sound=Sound::Metal1;if(time==1000)cue.caption="regression caption";m->sequenceCues.push_back(cue);}
  Game g(regression::campaign(m));int counts[3]={};bool caption=false;int firstFinish=0;
  for(int frame=1;frame<=240;++frame){g.m_sounds.clear();g.m_elapsed+=.016f;g.updateMechanisms(.016f);if(frame==1)firstFinish=g.state(t.finishState);
   for(const auto&s:g.m_sounds)if(s.radius==35){ // Sequence cue events use the same range; distinguish by frame clock.
    int clock=g.state(t.timerState);++counts[clock<1000?0:clock<2000?1:2];}
   if(g.state(t.timerState)>=1000&&g.state(t.timerState)<1040)caption|=g.actorCaption()=="regression caption";
  }
  check(counts[0]==1,"3a cue at zero once","count="+std::to_string(counts[0]));
  check(counts[2]==1,"3b end cue once","count="+std::to_string(counts[2])+" timer="+std::to_string(g.state(t.timerState)));
  check(firstFinish==1,"3c finish at one first frame","finish="+std::to_string(firstFinish));
  check(caption&&g.actorCaption().empty(),"3d caption expires after 2.6s","shown="+std::to_string(caption)+" caption at 3.84s='"+std::string(g.actorCaption())+"'");
 }
 {auto m=regression::room();TimedSequence t;t.timerState=stateId("reg_sound");t.finishState=stateId("reg_sound_done");t.x1=t.y1=-2;t.x2=t.y2=-1;t.soundAt={3,18.8f};t.soundZ=1.3f;t.soundIntervalMs=32;t.soundUntilMs=1000;m->timedSequences.push_back(t);
  auto c=regression::campaign(regression::room());m->definition.origin={24,0};c->maps.push_back(m);Game g(c);g.setState(t.timerState,1);
  for(int i=0;i<3;++i){g.m_elapsed+=.016f;g.updateMechanisms(.016f);}int ticks=0;float error=999,z=-1;for(auto&s:g.m_sounds)if(s.sound==t.tickSound){++ticks;error=length(s.position-Vec2{27,18.8f});z=s.z;}
  check(ticks==1&&error<.01f&&std::fabs(z-1.3f)<.001f,"3e soundAt chunk offset","ticks="+std::to_string(ticks)+" planar error="+std::to_string(error)+" z="+std::to_string(z));
 }
 {auto g=mapInspection({6,18},0,0,55,true);g.m_sounds.clear();g.setState(stateId("mnt_lathe_ms"),1);int ticks=0;float error=999;Vec2 at{};
  for(int i=0;i<30;++i){g.updateMechanisms(.016f);}
  for(const auto&s:g.m_sounds)if(s.sound==Sound::Machine){++ticks;at=s.position;error=std::min(error,length(at-Vec2{3,18.8f}));}
  check(ticks>0&&error<=.5f,"3e Maintenance lathe tick","ticks="+std::to_string(ticks)+" position="+std::to_string(at.x)+","+std::to_string(at.y)+" error="+std::to_string(error));
 }
 for(int scenario=0;scenario<3;++scenario){auto m=regression::doorRoom();if(scenario==2)m->doors[0].requireState=stateId("reg_locked");
  Game g(regression::campaign(m));CreatureSpawn s{scenario==1?Enemy::Kind::Huntsman:Enemy::Kind::Mutant,{8.5f,6},0};g.spawnCreature(s,30,false);auto&e=g.m_enemies[0];e.lastKnown=g.m_player.pos;e.lastKnownZ=0;e.heading=kPi*.5f;
  bool blocking=g.m_world.doorBlocks(8.5f,8,0,1.85f),planned=false,restored=false;
  {std::vector<char> open{1};World::DoorPlan plan(g.m_world,open);planned=!g.m_world.doorBlocks(8.5f,8,0,1.85f);}
  restored=g.m_world.doorBlocks(8.5f,8,0,1.85f)&&!g.m_world.doorPlanActive();
  if(scenario==0)check(blocking&&planned&&restored,"4 DoorPlan lifetime","before/plan/after="+std::to_string(blocking)+"/"+std::to_string(planned)+"/"+std::to_string(restored));
  float opened=0;for(int i=0;i<500;++i){g.m_sounds.clear();g.updateEnemies(.016f);g.m_world.updateDoors(.016f);opened=std::max(opened,g.m_world.doors()[0].open);}
  bool crossed=e.pos.y>8.35f;
  check(scenario==0?crossed&&opened>.8f:!crossed&&opened==0,scenario==0?"4 Mutant opens and crosses within 8s":scenario==1?"4 Huntsman blocked by door":"4 state locked door stays closed","end="+std::to_string(e.pos.x)+","+std::to_string(e.pos.y)+" max open="+std::to_string(opened)+" crossed="+std::to_string(crossed));
 }
 {auto m=regression::doorRoom();AuthoredLayerData deck;deck.elevation=3;deck.thickness=.2f;for(auto&row:deck.rows)row=std::string(24,'.');for(int y=5;y<=11;++y)for(int x=6;x<=10;++x)deck.rows[y][x]='#';m->layers.push_back(deck);World w(0,m);
  float base=w.floorHeight(8.5f,8);float high=w.clearanceHeight(8.5f,8,3),low=w.clearanceHeight(8.5f,8,base);bool fits=w.fits(8.5f,8,3,1.85f);
  check(fits&&std::fabs(low-base-2.5f)<.001f&&!w.fits(8.5f,8,base,2.6f),"5 deck above door header","deck fits="+std::to_string(fits)+" high clearance="+std::to_string(high)+" floor="+std::to_string(base)+" low clearance="+std::to_string(low));
 }
 for(bool openFirst:{true,false}){auto m=regression::doorRoom();m->actorTracks.push_back(regression::guard({8.5f,5.5f},{8.5f,11}));Game g(regression::campaign(m));g.m_player.pos={20,20};
  const auto&graph=g.navGraph(0,1.6f);size_t links=graph.links.size();bool builtClosed=g.m_world.doors()[0].open==0;
  if(openFirst){g.m_world.openDoor(0);g.m_world.updateDoors(2);}
  float closest=99;for(int i=0;i<750;++i){g.update({},.016f);g.m_sounds.clear();closest=std::min(closest,length(g.m_friendlyActors[0].pose.position-Vec2{8.5f,11}));}
  auto p=g.m_friendlyActors[0].pose.position;
  check(builtClosed&&links>0&&closest<.6f,openFirst?"6 guard graph built closed then opened":"6 guard fallback opens closed door","links="+std::to_string(links)+" closest="+std::to_string(closest)+" end="+std::to_string(p.x)+","+std::to_string(p.y)+" open="+std::to_string(g.m_world.doors()[0].open));
 }
 // Exercise both migration entry points with actual Ashfall ownership and authored spawn identities.
 for(bool playerCrossing:{false,true}){Game g(WorldId::Ashfall);int next=-1;Vec2 shift{};
  for(int i=1;i<g.chunkCount();++i){auto d=g.chunkOffset(i)-g.chunkOffset(0);if(std::fabs(d.x-24)<.01f&&std::fabs(d.y)<.01f){next=i;shift={-24,0};break;}}
  bool fixture=next>=0&&!g.m_enemies.empty();if(!fixture){check(false,"7 Ashfall seam fixture","next="+std::to_string(next)+" enemies="+std::to_string(g.m_enemies.size()));continue;}
  // Ashfall's first chunk authors one creature. Surround that real spawn with
  // two dynamic sentinels so the migrating creature is the middle list entry.
  auto original=g.m_enemies[0];Enemy sentinel=original;sentinel.scriptSpawned=true;sentinel.home={10,12};Enemy last=sentinel;last.home={12,12};
  g.m_enemies={sentinel,original,last};auto target=g.m_enemies[2];g.ensureChunk(next);size_t destinationBefore=g.m_chunks[next].enemies.size();
  for(int i:{0,2}){g.m_enemies[i].pos={10.f+float(i),12};g.m_enemies[i].awareness=0;}
  g.m_enemies[1].pos={playerCrossing?23.8f:24.2f,12};g.m_enemies[1].awareness=10;
  auto def=regression::room();def->actorTracks={regression::guard({10,10},{10,10}),regression::guard({11,10},{11,10})};g.m_friendlyActors.clear();
  for(size_t i=0;i<2;++i){FriendlyActor a;a.definition=def;a.homeActor=i;a.pose.position={10,10};g.m_friendlyActors.push_back(a);g.setState(g.friendlyKey(i,"guard_target_kind"),int(ShotKind::Enemy));g.setState(g.friendlyKey(i,"guard_target_level"),1);g.setState(g.friendlyKey(i,"guard_target_index"),i==0?3:2);}
  if(playerCrossing){g.m_player.pos={24.1f,12};g.crossChunkBoundary();}else g.migrateEnemiesAcrossChunks();
  auto& staying=playerCrossing?g.m_chunks[0].enemies:g.m_enemies;auto& migrated=playerCrossing?g.m_enemies:g.m_chunks[next].enemies;
  bool stable=staying.size()==2&&lengthSq(staying[1].home-target.home)<.0001f&&g.state(g.friendlyKey(0,"guard_target_index"))==2&&g.state(g.friendlyKey(0,"guard_target_level"))==1;
  bool follows=g.state(g.friendlyKey(1,"guard_target_level"))==next+1&&g.state(g.friendlyKey(1,"guard_target_index"))==int(destinationBefore)+1;
  std::string values="playerCrossing="+std::to_string(playerCrossing)+" staying="+std::to_string(staying.size())+" guard0 level/index="+std::to_string(g.state(g.friendlyKey(0,"guard_target_level")))+"/"+std::to_string(g.state(g.friendlyKey(0,"guard_target_index")))+" guard1="+std::to_string(g.state(g.friendlyKey(1,"guard_target_level")))+"/"+std::to_string(g.state(g.friendlyKey(1,"guard_target_index")));
  check(stable&&follows,"7 seam references",values);
  // Save once alive and once dead; count the authored home as well as origin identities.
  for(bool kill:{false,true}){if(kill)g.applyEnemyDamage(migrated.back(),10000,false,false,next);Game loaded(WorldId::Ashfall);bool ok=loaded.decodeSave(g.encodeSave());int matches=0,living=0,atOrigin=0;
   if(ok)for(int level=0;level<loaded.chunkCount();++level){const auto&list=level==loaded.m_level?loaded.m_enemies:loaded.m_chunks[level].enemies;for(const auto&e:list)if(e.kind==original.kind&&((e.originLevel==0&&lengthSq(e.originHome-original.home)<.0001f)||(level==0&&lengthSq(e.home-original.home)<.0001f))){++matches;living+=e.alive;atOrigin+=level==0;}}
   check(ok&&matches==1&&living==int(!kill)&&atOrigin==0,kill?"7 migrated follower stays dead after load":"7 migrated follower no origin duplicate","playerCrossing="+std::to_string(playerCrossing)+" decoded="+std::to_string(ok)+" matches="+std::to_string(matches)+" alive="+std::to_string(living)+" atOrigin="+std::to_string(atOrigin));
  }
 }
 {auto m=regression::room();AuthoredLayerData deck;deck.elevation=3;deck.thickness=.15f;for(auto&row:deck.rows)row=std::string(24,'.');for(int y=10;y<16;++y)for(int x=4;x<12;++x)deck.rows[y][x]='#';m->layers.push_back(deck);
  m->stairs.push_back({4,4,7,10,0,3,20,true,true});Game g(regression::campaign(m));g.m_player.pos={20,20};g.spawnCreature({Enemy::Kind::Mutant,{5.5f,12},3},0,false);
  auto&e=g.m_enemies[0];e.home={5.5f,3};e.homeZ=0;e.lastKnown={6.5f,12};e.lastKnownZ=3;e.awareness=.016f;e.heading=-kPi*.5f;
  float start=e.z;for(int i=0;i<1250;++i){g.m_sounds.clear();g.updateEnemies(.016f);}float distance=length(e.pos-e.home);
  check(e.awareness==0&&std::fabs(e.z-e.homeZ)<.05f&&distance<.6f,"8 returns to home floor","startZ="+std::to_string(start)+" endZ="+std::to_string(e.z)+" homeZ="+std::to_string(e.homeZ)+" lastKnownZ="+std::to_string(e.lastKnownZ)+" distance="+std::to_string(distance));
 }
 for(bool hostile:{false,true}){auto m=regression::room();auto a=regression::guard({6,6},{6,6});a.ai.sightRange=1;m->actorTracks.push_back(a);a.damageState=stateId("reg_other_damage");a.deadState=stateId("reg_other_dead");a.keys[0].position=a.keys[1].position={10,6};a.ai.patrol={{{10,6},0,60000}};a.ai.faction=hostile?AiFaction::Raider:AiFaction::Facility;m->actorTracks.push_back(a);Game g(regression::campaign(m));g.m_player.pos={20,20};
  g.damageFriendly(0,10,{10,6},{ShotKind::Npc,0,1});for(int i=0;i<30;++i){g.update({},.016f);g.m_sounds.clear();}int kind=g.state(g.friendlyKey(0,"guard_target_kind")),index=g.state(g.friendlyKey(0,"guard_target_index"));
  check(hostile?(kind==int(ShotKind::Npc)&&index==2):kind!=int(ShotKind::Npc),hostile?"9 hostile damage adopts attacker":"9 same faction damage avoids target","kind="+std::to_string(kind)+" index="+std::to_string(index)+" damage="+std::to_string(g.state(m->actorTracks[0].damageState)));
 }
 {Game g(regression::campaign(regression::room()));g.spawnCreature({Enemy::Kind::Huntsman,{9,9},6},0,false);auto&e=g.m_enemies[0];e.surfaceMode=2;g.applyEnemyDamage(e,1000,false,false);for(int i=0;i<125;++i)g.updateEnemies(.016f);
  check(!e.alive&&e.surfaceMode==0&&std::fabs(e.z)<.01f,"10 dead ceiling Huntsman falls","alive="+std::to_string(e.alive)+" surface="+std::to_string(e.surfaceMode)+" z="+std::to_string(e.z));
 }
 // Developer mode is the testing mode: it must get through the enemy-clear
 // doors and script gates that ordinary play opens by clearing the room.
 for(bool developer:{false,true}){auto m=regression::room();
  auto a=regression::guard({10,6},{10,6},AiFaction::Raider);a.ai.armed=true;m->actorTracks.push_back(a);
  ScriptEvent cleared;cleared.id=stateId("reg_cleared");cleared.x1=cleared.y1=0;cleared.x2=cleared.y2=24;cleared.requireEnemiesClear=true;
  ScriptAction act;act.type=ScriptAction::Type::SetState;act.id=stateId("reg_clear_done");act.value=1;cleared.actions.push_back(act);
  m->scriptEvents.push_back(cleared);
  Game g(regression::campaign(m));if(developer)g.setDeveloperMode(true);
  g.spawnCreature({Enemy::Kind::Huntsman,{9,9},0},0,false);
  for(int i=0;i<60;++i){g.update({},.016f);g.m_sounds.clear();}
  Door gate{};gate.left=6;gate.right=9;gate.y=12;gate.requireEnemiesClear=true;
  int hostiles=g.enemiesRemaining();bool fired=g.state(stateId("reg_clear_done"))==1,locked=g.doorLocked(gate);
  bool guardLives=!g.npcActors().empty()&&g.npcActors()[0].pose.clip!=4;
  std::ostringstream values;values<<"hostiles="<<hostiles<<" creatures="<<g.enemies().size()<<" gate fired="<<fired<<" door locked="<<locked<<" guard present="<<guardLives;
  check(guardLives&&(developer?(hostiles==0&&fired&&!locked):(hostiles>0&&!fired&&locked)),developer?"12 developer mode opens enemy-clear gates":"12 enemy-clear gates hold in ordinary play",values.str());
 }
 passed=testSnatcherAmbush(report)&&passed;return passed&&bool(report);
}
}
