#include "../../Game.h"
#include "../../CreatureAnimation.h"
#include "../../EngineRegressionScene.h"
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
namespace retro {
// A creature that bites (creatureTraits feedSeconds) takes the head of the man
// its strike kills and stays down over him; a recurring hunter (CreatureSpawn
// hunter) cannot be finished, runs when badly hurt and comes back.
bool Game::npcHeadless(size_t actor)const{return actor<m_friendlyActors.size()&&state(friendlyKey(actor,"headless"))!=0;}
float Game::npcHeadlessAge(size_t actor)const{
 if(!npcHeadless(actor))return -1;
 // Kept on the session clock; one that went back (a load) shows it long done.
 const float age=m_elapsed-state(friendlyKey(actor,"headless_at"))*.001f;return age<0?1e4f:age;
}
// The pool he lies in: under where his neck comes to rest as he goes over backwards. A blood
// mark, so it is drawn with the rest; a load lays it again (rebuildFriendlyActors).
void Game::headlessPool(size_t actor){
 if(!npcHeadless(actor))return;const auto&npc=m_friendlyActors[actor];if(npc.level<0||npc.level>=chunkCount()||!chunkResident(npc.level))return;
 const Vec2 at=npc.pose.position-Vec2{std::sin(npc.pose.yaw),std::cos(npc.pose.yaw)}*(.42f*npc.track().scale/1.6f);
 for(const auto&mark:m_bulletImpacts)if(mark.blood&&mark.level==npc.level&&lengthSq(mark.pos-at)<.04f&&mark.size>=.4f)return;
 m_bulletImpacts.push_back({at,friendlyWorld(npc.level).floorHeight(at.x,at.y)+.012f,{0,0},m_elapsed,npc.level,true,int(CreatureKind::Mutant),0,int(ImpactMaterial::Flesh),.58f});
 if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
}
int Game::hunterPrey(const Enemy& e)const{
 if(e.prey<0||e.wakeState)return -1;
 const int home=e.originLevel>=0?e.originLevel:m_level;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];
  if(npc.homeLevel!=home||int(npc.homeActor)!=e.prey)continue;
  const auto&track=npc.track();
  if(npc.level!=m_level||!track.health||(track.deadState&&state(track.deadState))||npc.pose.clip==4)return -1;
  return int(i);
 }
 return -1;
}
void Game::wakeHunter(Enemy& e){
 e.earTime=m_elapsed;
 // Out for one man: it goes straight for him, and has not made the player out.
 if(const int quarry=hunterPrey(e);quarry>=0){const auto&npc=m_friendlyActors[size_t(quarry)];e.lastKnown=npc.pose.position;e.lastKnownZ=npc.pose.z;e.recognition=0;e.state=Enemy::State::Chase;}
 if(!e.hunter)return;
 // Otherwise it comes out knowing where the player is: it is hunting them.
 if(hunterPrey(e)<0){e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;e.awareness=std::max(e.awareness,8.f);e.state=Enemy::State::Investigate;}
 // One animal: wherever else it was left awake, it has come here.
 for(int level=0;level<chunkCount();++level){auto&list=level==m_level?m_enemies:m_chunks[size_t(level)].enemies;
  for(auto&other:list)if(&other!=&e&&other.hunter==e.hunter&&other.alive&&!other.wakeState)vanishHunter(other);}
}
void Game::vanishHunter(Enemy& e){
 e.alive=false;e.hp=std::max(e.hp,1.f);e.deathTime=Enemy::CorpseLifetime+1;e.windup=e.strike=0;e.feeding=0;e.feedActor=-1;e.arrive=0;
 e.moving=false;e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=0;e.awareness=0;e.painFlash=0;
}
void Game::biteKill(Enemy& e,size_t actor){
 const auto&traits=creatureTraits(e.kind);const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();
 if(npcHeadless(actor))return;
 setState(friendlyKey(actor,"headless"),1);setState(friendlyKey(actor,"headless_at"),int(std::lround(m_elapsed*1000)));
 const Vec2 toward=normalized(npc.pose.position-e.pos);
 // The head comes away in its jaws: a burst at the neck and the stump's first pump.
 const Point3 neck{npc.pose.position.x,npc.pose.position.y,npc.pose.z+track.scale*.86f};
 hitBurst(m_level,neck,{toward.x*.5f,toward.y*.5f,.85f},ImpactMaterial::Flesh,-1,2.4f,true);
 hitBurst(m_level,neck,{toward.x*.3f,toward.y*.3f,1.f},ImpactMaterial::Flesh,-1,1.8f,false);if(!m_hitBursts.empty())m_hitBursts.back().stump=true;
 m_sounds.push_back({Sound::PunchHit,e.pos,1.f,.46f,true,17,neck.z});
 headlessPool(actor);
 enemySound(e,1,1.f,.82f);
 e.feeding=traits.feedSeconds;e.feedActor=int(actor);e.feedHp=e.hp;e.windup=0;e.strike=0;e.attackCooldown=traits.cooldown;
 if(lengthSq(toward)>.0001f)e.heading=std::atan2(toward.y,toward.x);
}
bool Game::updateFeeding(Enemy& e,float dt){
 const auto&traits=creatureTraits(e.kind);
 auto stop=[&](bool roused){e.feeding=0;e.feedActor=-1;
  // The tail of its attack lifts it off the body.
  e.strike=.47f;
  if(roused){e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;e.awareness=std::max(e.awareness,12.f);e.recognition=1;e.state=Enemy::State::Chase;e.attackCooldown=std::min(e.attackCooldown,.4f);enemySound(e,0,1.f,.85f);}
  return false;};
 // Hurt, it is off the body at once and after whoever did it.
 if(e.hp<e.feedHp-.01f||e.stalkMode==Enemy::StalkMode::Retreat)return stop(true);
 // The player close in its view rouses it; at its back it does not look up.
 const Vec2 to=m_player.pos-e.pos;const float d=length(to);const Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
 if(hiddenLocker()<0&&std::fabs(m_player.z-e.z)<1.6f&&d<3.f&&(d<1.1f||dot(to*(1.f/std::max(d,.001f)),facing)>traits.sightDot)&&m_world.rayClear(e.pos,e.z+.7f,m_player.pos,m_player.z+m_player.eye))return stop(true);
 e.feeding=std::max(0.f,e.feeding-dt);
 if(e.feeding<=0)return stop(false);
 e.moving=false;e.windup=0;e.strike=0;e.awareness=std::max(e.awareness,1.f);
 // A bite at each end of its worry: blood off the body and a wet sound for
 // the player's ears alone.
 const auto turn=[&](float t){return std::floor((t*4.4f+e.home.y)/kPi);};
 if(turn(m_elapsed)!=turn(m_elapsed-dt)){
  const float reach=1.05f*e.scale;Point3 mouth{e.pos.x+facing.x*reach,e.pos.y+facing.y*reach,m_world.floorHeight(e.pos.x+facing.x*reach,e.pos.y+facing.y*reach)+.28f};
  const unsigned beat=unsigned(turn(m_elapsed))&3u;
  hitBurst(m_level,mouth,{facing.x*-.4f,facing.y*-.4f,.7f},ImpactMaterial::Flesh,-1,.7f+.15f*float(beat&1u),false);
  SoundEvent chew{Sound::PunchHit,{mouth.x,mouth.y},.42f,.5f+.06f*float(beat),true,14,mouth.z};chew.aiRadius=-1;m_sounds.push_back(chew);
  if(beat==3u){SoundEvent growl{Sound::OneMonsterGrowl,e.pos,.35f,.82f,true,15,e.z+.6f};growl.aiRadius=-1;m_sounds.push_back(growl);}
 }
 return true;
}
bool Game::hunterFlight(Enemy& e,Vec2& goal,float& goalZ,float dt){
 e.fleeTime+=dt;e.awareness=std::max(e.awareness,4.f);if(e.state==Enemy::State::Search||e.state==Enemy::State::Idle)e.state=Enemy::State::Investigate;
 const float away=length(e.pos-m_player.pos);
 const bool watched=away<26.f&&hiddenLocker()<0&&(m_world.rayClear(m_player.pos,m_player.z+m_player.eye,e.pos,e.z+.55f*e.scale)||m_world.rayClear(m_player.pos,m_player.z+m_player.eye,e.pos,e.bodyTop()-.15f));
 // Out of sight and away (or back in its den, or long enough in the dark):
 // it is gone, and the hunter state counts one more escape.
 const bool den=length(e.pos-e.home)<.9f&&away>5.f;
 if(!watched&&(away>10.f||den||e.fleeTime>14.f)){
  setState(e.hunter,state(e.hunter)+1);
  SoundEvent scrape{Sound::OneMetalDrag,e.pos,.55f,.78f,true,18,e.z+.5f};scrape.aiRadius=-1;m_sounds.push_back(scrape);
  vanishHunter(e);return true;
 }
 // Where it runs: back to where it came from when that leads away from the
 // player, otherwise the place round about furthest from them and out of view.
 e.stalkTimer=std::max(0.f,e.stalkTimer-dt);
 if(!e.hideSet||e.stalkTimer<=0){
  const float step=m_world.hasTerrain()?creatureTraits(e.kind).step:.215f;
  Vec2 best=e.home;float bestZ=e.homeZ>-999?e.homeZ:e.z,bestScore=-1e9f;
  auto consider=[&](Vec2 p,float bonus){if(p.x<1||p.y<1||p.x>World::Width-1||p.y>World::Height-1)return;
   const float z=groundHeight(p,e.z+step+1.2f,e.radius());if(std::fabs(z-e.z)>1.3f||!hullFits(p,z,e.hull(),e.radius(),false))return;
   const Vec2 from=e.pos-m_player.pos,run=p-e.pos;const float spread=length(p-m_player.pos);
   float score=spread+bonus;
   if(!m_world.rayClear(m_player.pos,m_player.z+m_player.eye,p,z+.6f))score+=6.f;
   if(m_world.rayClear(e.pos,e.z+.5f,p,z+.5f))score+=2.f;
   // Never back past the player to get there.
   if(length(run)>.1f&&length(from)>.1f&&dot(normalized(run),normalized(from))<-.3f&&away<7.f)score-=12.f;
   if(score>bestScore){bestScore=score;best=p;bestZ=z;}};
  if(length(e.home-m_player.pos)>5.f)consider(e.home,9.f);
  for(int i=0;i<16;++i){const float a=float(i)*kPi/8;for(float r:{4.f,7.f})consider(e.pos+Vec2{std::cos(a),std::sin(a)}*r,0);}
  e.hideAt=best;e.hideSet=true;e.stalkTimer=1.2f;e.lastKnownZ=bestZ;
 }
 goal=e.hideAt;goalZ=e.lastKnownZ;return false;
}
void Game::hunterArrive(Enemy& e,float dt){
 const auto&traits=creatureTraits(e.kind);e.arrive=std::max(0.f,e.arrive-dt);
 const Vec2 to=e.waypoint-e.pos;const float d=length(to),step=std::min(d,traits.speed*1.3f*dt);
 if(d>.001f){e.heading=std::atan2(to.y,to.x);e.pos+=to*(step/d);e.moving=step>.0001f;e.gait+=step*traits.gaitPerMetre;}
 if(e.arrive>0)return;
 // In: at the crossing, or the nearest place beside it its body fits.
 Vec2 at=e.waypoint;float z=e.homeZ;bool placed=false;
 for(float r:{0.f,.35f,.7f,1.05f,1.4f}){for(int i=0;i<8&&!placed;++i){const float a=float(i)*kPi/4;const Vec2 p=e.waypoint+Vec2{std::cos(a),std::sin(a)}*r;const float g=groundHeight(p,e.homeZ+.3f,e.radius());
   if(std::fabs(g-e.homeZ)<.5f&&hullFits(p,g,e.hull(),e.radius(),false)){at=p;z=g;placed=true;}}if(placed)break;}
 e.pos=at;e.z=z;e.verticalVelocity=0;e.home=at;e.homeZ=z;e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;e.awareness=std::max(e.awareness,8.f);e.state=Enemy::State::Investigate;e.repathTimer=0;e.searchTime=0;e.moving=false;
 enemySound(e,0,.9f,.92f);
}
void Game::hearPlayerNoise(const std::vector<AiStimulus>& stimuli){
 for(const auto&s:stimuli){if(s.npc>=0)continue;
  const bool loud=s.loud||(s.categories&(ai::SoundWeapon|ai::SoundDanger))||((s.categories&ai::SoundImpact)&&s.gain>.75f)||(s.step&&s.radius>5.f);
  if(!loud||lengthSq(s.source-m_player.pos)>2.5f*2.5f)continue;
  m_playerNoise={s.source,m_player.z,m_elapsed};
 }
}
// --each-test=chimera-hunt: the staged bite kill, the meal, and the animal that
// gets away and comes back.
bool Game::testChimeraHunt(){
 std::ofstream report("chimera-hunt-test.txt");bool passed=true;
 auto check=[&](bool ok,const std::string&name,const std::string&v){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<v<<'\n';report.flush();passed&=ok;};
 const StateId wake=stateId("hunt_test_wake"),beast=stateId("hunt_test_r117"),dead=stateId("reg_guard_dead");constexpr float dt=1.f/60;
 // A room cut by a wall with a shut door: the man and the animal's den on the
 // near side, the player beyond the wall.
 auto scene=[&](bool withPrey,bool second){auto m=regression::doorRoom();
  if(withPrey){auto man=regression::guard({12.f,4.5f},{12.f,4.5f},AiFaction::Raider);m->actorTracks.push_back(man);}
  CreatureSpawn chimera{CreatureKind::Chimera,{4.f,4.f},0};chimera.wakeState=wake;chimera.hunter=beast;chimera.prey=withPrey?0:-1;m->creatureSpawns.push_back(chimera);
  if(second){CreatureSpawn back{CreatureKind::Chimera,{19.f,4.f},0};back.wakeState=beast;back.hunter=beast;m->creatureSpawns.push_back(back);}
  auto g=std::make_unique<Game>(regression::campaign(m));g->m_player.pos={20.5f,14.f};g->m_player.z=0;g->m_player.angle=kPi*.5f;return g;};
 auto tick=[&](Game&g,float seconds,const std::function<bool()>&until=nullptr){for(int i=0;i<int(seconds/dt);++i){g.update({},dt);if(until&&until())return true;}return false;};
 // 1. The bite: the man it came out for dies to its first strike, headless; a load keeps him so.
 {auto held=scene(true,false);auto&g=*held;g.setState(wake,1);
  const bool died=tick(g,12.f,[&]{return g.state(dead)!=0;});auto&e=g.m_enemies[0];
  std::ostringstream v;v<<"dead="<<died<<" headless="<<g.npcHeadless(0)<<" feeding="<<e.feeding<<" damage="<<g.state(stateId("reg_guard_damage"))<<" chimera at "<<e.pos.x<<','<<e.pos.y;
  check(died&&g.npcHeadless(0)&&e.feeding>0,"1 the strike that kills the man takes his head and it feeds",v.str());
  bool headBurst=false,stump=false;for(const auto&b:g.hitBursts()){headBurst|=b.material==ImpactMaterial::Flesh&&b.kill;stump|=b.stump;}
  check(headBurst&&stump,"1 the head comes away in a burst and the stump pumps","burst="+std::to_string(headBurst)+" stump="+std::to_string(stump));
  Game loaded(g.m_customCampaign);const bool ok=loaded.decodeSave(g.encodeSave());
  size_t man=0;for(size_t i=0;i<loaded.m_friendlyActors.size();++i)if(loaded.m_friendlyActors[i].homeActor==0)man=i;
  check(ok&&loaded.state(dead)&&loaded.npcHeadless(man)&&loaded.npcHeadlessAge(man)>=0&&loaded.m_friendlyActors[man].pose.clip==4,"1 after save and load he lies dead and headless","decode="+std::to_string(ok)+" dead="+std::to_string(loaded.state(dead))+" headless="+std::to_string(loaded.npcHeadless(man)));
  check(ok&&!loaded.m_enemies.empty()&&loaded.m_enemies[0].hunter==beast&&loaded.m_enemies[0].prey==0,"1 the save keeps the animal's name and quarry","");
  // 2. The meal holds: it does not move off the body for its span.
  const Vec2 at=e.pos;float drift=0;const float before=e.feeding;
  for(int i=0;i<int(3.f/dt);++i){g.update({},dt);drift=std::max(drift,length(e.pos-at));}
  {auto p=creatureAnimationSample(e,g.elapsed());std::ostringstream w;w<<"feeding "<<before<<" -> "<<e.feeding<<" drift="<<drift<<" clip="<<p.clip<<" phase="<<p.phase;
   check(e.feeding>0&&drift<.05f&&p.clip==2&&p.phase>=creatureTraits(e.kind).worryFrom-.001f&&p.phase<=creatureTraits(e.kind).worryTo+.001f,"2 it stays over the body worrying it",w.str());}
  // A wound ends it at once.
  g.applyEnemyDamage(e,20,false,false);g.update({},dt);
  check(e.feeding<=0&&e.awareness>=11.5f,"2 hurt, it is off the body at once","feeding="+std::to_string(e.feeding)+" awareness="+std::to_string(e.awareness));
 }
 // 2b. The player close in its view ends the meal; behind it, it feeds on.
 for(bool inView:{false,true}){auto held=scene(true,false);auto&g=*held;g.setState(wake,1);tick(g,12.f,[&]{return g.state(dead)!=0;});auto&e=g.m_enemies[0];
  tick(g,.5f);const Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
  g.m_player.pos=e.pos+(inView?facing*2.4f:facing*-2.4f);g.m_player.z=0;g.update({},dt);
  check(inView?e.feeding<=0:e.feeding>0,inView?"2 the player 2.4 m in front of it rouses it":"2 the player 2.4 m behind it does not","feeding="+std::to_string(e.feeding));
 }
 // 3. The recurring animal: past its threshold it runs, nothing kills it, out of
 // sight and away it is gone, the state counts it, the next spawn wakes on it.
 {auto held=scene(false,true);auto&g=*held;g.setState(wake,1);g.update({},dt);
  auto&e=g.m_enemies[0];const auto&second=g.m_enemies[1];e.pos={15.f,5.f};e.z=0;
  check(e.alive&&!e.wakeState&&second.wakeState==beast,"3 the first wakes, the return waits on the fled state","");
  g.applyEnemyDamage(e,e.maxHp*.65f,false,false);
  check(e.stalkMode==Enemy::StalkMode::Retreat,"3 past its threshold it runs","hp="+std::to_string(e.hp));
  g.applyEnemyDamage(e,5000,false,false);
  check(e.alive&&e.hp>=1&&g.m_kills==0,"3 no wound kills it","hp="+std::to_string(e.hp)+" kills="+std::to_string(g.m_kills));
  float closest=1e9f;const bool gone=tick(g,20.f,[&]{closest=std::min(closest,length(e.pos-e.home));return g.m_enemies[0].gone();});
  std::ostringstream v;v<<"gone="<<gone<<" state="<<g.state(beast)<<" kills="<<g.m_kills<<" closest to den "<<closest;
  check(gone&&g.state(beast)==1&&g.m_kills==0,"3 out of the player's sight it gets away and the state counts it",v.str());
  g.update({},dt);
  check(!g.m_enemies[1].wakeState&&g.m_enemies[1].alive&&g.m_enemies[1].awareness>0,"3 the next spawn wakes on the fled state","awareness="+std::to_string(g.m_enemies[1].awareness));
  Game loaded(g.m_customCampaign);const bool ok=loaded.decodeSave(g.encodeSave());
  check(ok&&loaded.m_enemies.size()==2&&loaded.m_enemies[0].gone()&&loaded.m_enemies[1].alive,"3 a load keeps the one that got away gone","");
 }
 // 4. One animal: when a later spawn of it wakes, the one still about leaves.
 {auto held=scene(false,true);auto&g=*held;g.setState(wake,1);g.update({},dt);g.setState(beast,1);g.update({},dt);
  check(g.m_enemies[0].gone()&&g.m_enemies[1].alive&&!g.m_enemies[1].wakeState,"4 the earlier one is gone when the return wakes","");
 }
 // 5. Between sightings it goes by ear to the player's last loud noise, however far.
 {auto held=scene(false,false);auto&g=*held;g.setState(wake,1);g.update({},dt);auto&e=g.m_enemies[0];
  for(int i=0;i<int(1.f/dt);++i){g.m_enemies[0].awareness=0;g.update({},dt);}
  g.m_player.pos={21.f,22.f};g.sound(Sound::Shot);g.updateEnemies(dt);
  std::ostringstream v;v<<"lastKnown "<<e.lastKnown.x<<','<<e.lastKnown.y<<" state "<<int(e.state)<<" distance "<<length(e.pos-g.m_player.pos);
  check(length(e.lastKnown-g.m_player.pos)<1.f&&e.state==Enemy::State::Investigate,"5 it drifts toward the player's last shot",v.str());
 }
 // 6. Chapter 2's R-117 where it is staged. Containment A: the player steps onto the floor, it is
 // heard, comes out of A4 for the man at the post and takes his head, in view of the way in.
 {auto g=chimeraStage(96,{4.6f,12.2f},-15,-.1f);const StateId postDead=stateId("cona_a_post_dead");
  size_t post=SIZE_MAX;for(size_t i=0;i<g.m_friendlyActors.size();++i){const auto&n=g.m_friendlyActors[i];if(n.homeLevel==96&&n.track().deadState==postDead)post=i;}
  int chimera=-1;for(int i=0;i<int(g.m_enemies.size());++i)if(g.m_enemies[size_t(i)].hunter==stateId("r117"))chimera=i;
  const bool dormant=chimera>=0&&g.m_enemies[size_t(chimera)].wakeState!=0;float wokeAt=-1,killAt=-1;
  for(int i=0;i<int(30.f/dt)&&killAt<0;++i){g.m_player.pos={4.6f,12.2f};g.update({},dt);if(wokeAt<0&&chimera>=0&&!g.m_enemies[size_t(chimera)].wakeState)wokeAt=i*dt;if(g.state(postDead))killAt=i*dt;}
  const auto&e=g.m_enemies[size_t(std::max(chimera,0))];const auto&body=post<g.m_friendlyActors.size()?g.m_friendlyActors[post].pose:ActorPose{};
  const bool seen=g.m_world.rayClear(g.m_player.pos,g.m_player.z+g.m_player.eye,body.position,body.z+.4f);
  std::ostringstream v;v<<"dormant="<<dormant<<" woke at "<<wokeAt<<" s, the post man dead at "<<killAt<<" s, headless="<<(post<g.m_friendlyActors.size()&&g.npcHeadless(post))<<" feeding="<<e.feeding<<" body at "<<body.position.x<<','<<body.position.y<<" in view of the way in="<<seen;
  check(dormant&&wokeAt>=3.f&&killAt>0&&post<g.m_friendlyActors.size()&&g.npcHeadless(post)&&e.feeding>0&&seen,"6 96: heard first, out of the service run, the post man's head taken in view of the floor",v.str());}
 // Its returns: each appears as the same animal and gets to the player from where it comes from.
 struct Return {int level;Vec2 player;float z;const char* set;const char* wake;Vec2 from;};
 for(const auto& r:{Return{99,{9.f,16.8f},-18,"cont_power","conb_sh_r117_out",{11.8f,21.9f}},Return{101,{12.f,10.6f},-18,"cont_power","conb_th_r117_out",{16.2f,21.6f}},
                    Return{103,{12.f,13.5f},-15,"cont_power,conb_ec_pressure,cont_barricade","",{20.35f,7.15f}},Return{107,{17.4f,16.2f},-25,"cont_power,cont_barricade,core_shutdown","",{11.6f,16.3f}}}){
  auto g=chimeraStage(r.level,r.player,r.z,0);std::string sets=r.set;for(size_t a=0;a<sets.size();){size_t b=sets.find(',',a);if(b==std::string::npos)b=sets.size();g.setState(stateId(sets.substr(a,b-a)),1);a=b+1;}
  if(*r.wake)g.setState(stateId(r.wake),1);
  float reached=-1,nearest=1e9f;int found=-1;
  for(int i=0;i<int(25.f/dt)&&reached<0;++i){g.m_player.pos=r.player;g.m_player.z=r.z;g.update({},dt);
   for(int k=0;k<int(g.m_enemies.size());++k)if(g.m_enemies[size_t(k)].hunter==stateId("r117")&&g.m_enemies[size_t(k)].alive)found=k;
   if(found>=0){const float d=length(g.m_enemies[size_t(found)].pos-r.player);nearest=std::min(nearest,d);if(d<3.f&&std::fabs(g.m_enemies[size_t(found)].z-r.z)<1.f)reached=i*dt;}}
  std::ostringstream v;v<<"level "<<r.level<<": r117 present="<<(found>=0)<<" came from "<<r.from.x<<','<<r.from.y<<" nearest "<<nearest<<" m, at the player after "<<reached<<" s";if(found>=0){const auto&q=g.m_enemies[size_t(found)];v<<" (ends at "<<q.pos.x<<','<<q.pos.y<<','<<q.z<<" stalk "<<int(q.stalkMode)<<" state "<<int(q.state)<<" awareness "<<q.awareness<<" hp "<<q.hp<<" waypoint "<<q.waypoint.x<<','<<q.waypoint.y<<" lost "<<q.lostPath<<")";}
  check(found>=0&&reached>0,"6 "+std::to_string(r.level)+": R-117 comes back and reaches the player",v.str());
 }
 // 7. Through the seam after the player: Containment A's service door into the service run (97).
 {auto g=chimeraStage(96,{12.f,23.2f},-15,kPi*.5f);int k=-1;for(int i=0;i<int(g.m_enemies.size());++i)if(g.m_enemies[size_t(i)].hunter==stateId("r117"))k=i;
  bool followed=false,inside=false;float arrive=0;
  if(k>=0){auto&e=g.m_enemies[size_t(k)];e.wakeState=0;e.prey=-1;e.pos={12.f,19.f};e.awareness=12;e.recognition=1;e.state=Enemy::State::Chase;e.lastKnown=g.m_player.pos;
   g.m_player.pos={12.f,24.05f};g.crossChunkBoundary();
   for(const auto&f:g.m_enemies)if(f.hunter==stateId("r117")&&f.alive){followed=g.level()==97;arrive=f.arrive;}
   for(int i=0;i<int(8.f/dt)&&!inside;++i){g.m_player.pos={12.f,3.f};g.update({},dt);for(const auto&f:g.m_enemies)if(f.hunter==stateId("r117")&&f.alive&&f.arrive<=0&&f.pos.y>0&&f.pos.y<24)inside=true;}}
  std::ostringstream v;v<<"level "<<g.level()<<" followed="<<followed<<" walk-in "<<arrive<<" s, in the run="<<inside;
  check(followed&&arrive>0&&inside,"7 it comes through the seam after the player and walks in",v.str());}
 report<<(passed?"ALL PASSED":"FAILED")<<'\n';return passed;
}
Game Game::chimeraStage(int level,Vec2 pos,float z,float yaw,float pitch){
 Game g;g.m_level=level;g.restart();
 for(const char* s:{"ere_great_door","chapter2_open","proc_clearance","lab_chem_seal","med_override","med_morgue_release"})g.setState(stateId(s),1);
 g.m_god=true;g.m_inspectionCamera=true;g.m_player.pos=pos;g.m_player.z=z;g.m_player.angle=yaw;g.m_player.pitch=pitch;g.m_player.grounded=true;return g;
}
// --chimera-kill-frames: the camera holds at the way in, then (close) stands over the post man.
void Game::chimeraFilm(bool close){
 if(!close)return;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];if(n.homeLevel!=m_level||n.track().deadState!=stateId("cona_a_post_dead")||!state(n.track().deadState))continue;
  const Vec2 at=n.pose.position,from=at+Vec2{-1.6f,-1.1f};m_player.pos=from;m_player.z=n.pose.z;m_player.angle=std::atan2(at.y-from.y,at.x-from.x);m_player.pitch=-75;
  if(!hasQuestItem(Flashlight))giveQuestItem(Flashlight,1);setState(stateId("flashlight_on"),1);return;}
}
std::string Game::chimeraFilmLine()const{
 std::ostringstream o;o.setf(std::ios::fixed);o.precision(2);o<<"t="<<m_elapsed;
 for(const auto&e:m_enemies)if(e.hunter)o<<" r117 "<<(e.alive?"alive":e.gone()?"gone":"dead")<<" at "<<e.pos.x<<','<<e.pos.y<<" wake="<<(e.wakeState!=0)<<" feeding="<<e.feeding<<" hp="<<e.hp;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&n=m_friendlyActors[i];if(n.homeLevel!=m_level||!n.track().health)continue;o<<" | actor "<<i<<" dead="<<(n.track().deadState&&state(n.track().deadState))<<" headless="<<npcHeadless(i)<<" at "<<n.pose.position.x<<','<<n.pose.position.y;}
 return o.str();
}
}
