#include "../../Game.h"
#include "../../CreatureAnimation.h"
#include "../../EngineRegressionScene.h"
#include "../Perception.h"
#include "../../../renderer/Mesh.h"
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
namespace retro {
// The cursed equipment (CreatureTraits::dormant): the plant's own machinery with the ore's current in
// it. It stands among the real equipment frozen, its lamp dark, until something wakes it: power coming
// on in its room (its spawn's wake state), a terminal worked near it, a torch shone in its face, a loud
// noise, a wound. Woken, its lamp flickers up and its motors grind before it moves. It hunts by ear and
// toward light, and with nothing to go on it walks to the nearest live lamp or live circuit. Rounds
// mostly spark off it (Hitscan, CreatureTraits::armour). What kills it is the plant: live current (an
// Electricity hazard while it is on, a torn live lead's end) seizes it, and a little over half a second of
// it (one strike of the electrodes) shorts it out.
namespace {
// The state a dormant kind sleeps on when its spawn names none: nothing ever sets it.
constexpr StateId Asleep=stateId("machine_asleep");
// Seconds of live current that short it out; how long it stays seized after the current lets go;
// its start-up once woken; how close a terminal, a torch or a noise has to be to wake it.
constexpr float ShortSeconds=.55f,Seize=1.2f,StartUp=1.4f,TerminalReach=8.f,TorchReach=5.5f,NoiseReach=11.f;
}
StateId Game::machineAsleepState(){return Asleep;}
// Where its lamp hangs: out in front of the vessel on its chain, on its left, at chest height.
Point3 Game::machineLamp(const Enemy& e)const{
 const Vec2 f{std::cos(e.heading),std::sin(e.heading)},left{-f.y,f.x};const Vec2 at=e.pos+f*(.42f*e.scale)+left*(.3f*e.scale);
 return {at.x,at.y,e.z+1.45f*e.scale};
}
bool Game::machineCurrent(const Enemy& e,Point3& contact)const{
 const float r=e.radius();
 for(const auto&h:m_world.hazards()){
  if(h.kind!=Hazard::Kind::Electricity||!hazardActive(h))continue;
  if(e.pos.x<h.x1-r||e.pos.x>h.x2+r||e.pos.y<h.y1-r||e.pos.y>h.y2+r||e.bodyTop()<h.bottom||e.z>h.top)continue;
  contact={std::clamp(e.pos.x,h.x1,h.x2),std::clamp(e.pos.y,h.y1,h.y2),std::max(h.bottom,e.z)+.05f};return true;
 }
 // A torn live lead's end against its body.
 const auto&leads=m_world.leads();const auto&ropes=m_world.leadRopes();
 for(size_t i=0;i<leads.size()&&i<ropes.size();++i){const auto&rope=ropes[i];if(!leads[i].live||!rope.torn||rope.points.empty())continue;
  const Point3 tip=rope.points.back();if(lengthSq(Vec2{tip.x,tip.y}-e.pos)<(r+.35f)*(r+.35f)&&tip.z>e.z-.2f&&tip.z<e.bodyTop()){contact=tip;return true;}}
 return false;
}
void Game::wakeMachine(Enemy& e,Vec2 toward,float towardZ,bool knows){
 if(!e.alive||!e.wakeState)return;
 e.wakeState=0;e.stalkMode=Enemy::StalkMode::Recover;e.stalkTimer=StartUp;e.awareness=std::max(e.awareness,6.f);e.state=Enemy::State::Investigate;
 e.lastKnown=toward;e.lastKnownZ=towardZ;if(knows)e.recognition=1;e.repathTimer=0;
 // The lamp catching, the motors taking up.
 const float mid=(e.bodyBottom()+e.bodyTop())*.5f;
 m_sounds.push_back({Sound::LiftMotor,e.pos,.75f,.62f,true,18,mid});m_sounds.push_back({Sound::OneMetalMoan,e.pos,.55f,.8f,true,18,mid});
 m_sounds.push_back({Sound::LeadArc,e.pos,.4f,1.3f,true,12,machineLamp(e).z});
 sparkBurst(m_level,machineLamp(e),{0,0,1},.9f,10,false);
}
void Game::wakeMachinesNear(Vec2 at,float z,float radius){
 for(auto&e:m_enemies)if(e.sleeping()&&lengthSq(e.pos-at)<radius*radius&&std::fabs(e.z-z)<3.f)wakeMachine(e,at,z,false);
}
void Game::shortMachine(Enemy& e,Point3 contact){
 const Point3 body{e.pos.x,e.pos.y,(e.bodyBottom()+e.bodyTop())*.5f},lamp=machineLamp(e);
 e.shorted=true;
 if(!applyEnemyDamage(e,e.hp+1000.f,false,false))return;
 // It goes over the way it was walking, the lamp blows, current runs out of it to the floor.
 const Vec2 f{std::cos(e.heading),std::sin(e.heading)};e.deathPush={f.x*.6f,f.y*.6f,.3f};
 sparkBurst(m_level,body,{0,0,1},1.f,64,true);sparkBurst(m_level,lamp,{f.x,f.y,.4f},1.f,40,true);sparkBurst(m_level,contact,{0,0,1},1.f,32,false);
 strikeArc(m_level,body,contact,.45f,1.f,true);strikeArc(m_level,lamp,{body.x,body.y,e.z+.05f},.3f,.9f,true);
 m_sounds.push_back({Sound::LeadArc,e.pos,1.f,.85f,true,22,body.z});m_sounds.push_back({Sound::OneGlassBang,e.pos,.7f,1.4f,true,18,lamp.z});
 m_sounds.push_back({Sound::OneMetalFall,e.pos,.95f,.7f,true,22,e.z+.4f});
 const float floor=m_world.supportBelow(e.pos.x,e.pos.y,e.z+.3f);
 m_bulletImpacts.push_back({e.pos,floor+.012f,{0,0},m_elapsed,m_level,false,-2,1.f});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
}
void Game::machineAttraction(const Enemy& e,Vec2& goal,float& goalZ)const{
 // Nothing to go on: the nearest live circuit (to stand at its edge) or live lamp (to stand under it).
 float best=14.f*14.f;
 for(const auto&h:m_world.hazards()){if(h.kind!=Hazard::Kind::Electricity||!hazardActive(h))continue;
  const Vec2 inside{std::clamp(e.pos.x,h.x1,h.x2),std::clamp(e.pos.y,h.y1,h.y2)};Vec2 away=e.pos-inside;const float d=length(away);
  const Vec2 edge=d>.01f?inside+away*((e.radius()+1.f)/d):inside;const float d2=lengthSq(edge-e.pos);
  if(d2<best&&std::fabs(h.bottom-e.z)<2.5f){best=d2;goal=edge;goalZ=e.z;}}
 for(const auto&light:m_world.lights()){if(light.intensity<=0||lampPower(light)<.5f)continue;
  const Vec2 under{light.position.x,light.position.y};const float floor=m_world.supportBelow(under.x,under.y,e.z+.5f);
  if(std::fabs(floor-e.z)>.3f)continue;const float d2=lengthSq(under-e.pos);
  if(d2<best&&m_world.rayClear(e.pos,e.z+1.2f,under,floor+1.2f)){best=d2;goal=under;goalZ=floor;}}
}
bool Game::machineUpdate(Enemy& e,const std::vector<AiStimulus>& stimuli,float dt){
 const auto&t=creatureTraits(e.kind);const float mid=(e.bodyBottom()+e.bodyTop())*.5f;
 // Live current: it seizes, sparks and arcs, and a second of it shorts it out.
 Point3 contact;
 if(machineCurrent(e,contact)){
  if(e.sleeping())e.wakeState=0;
  e.shock+=dt;e.painFlash=1;e.windup=0;e.strike=0;e.moving=false;e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=0;
  e.hp=std::min(e.hp,e.maxHp*std::max(.01f,1.f-e.shock/ShortSeconds));
  if(e.sparkClock<=0||e.sparkClock>.09f){e.sparkClock=.09f;const Point3 body{e.pos.x,e.pos.y,mid};
   sparkBurst(m_level,contact,{0,0,1},1.f,8,false);sparkBurst(m_level,body,{std::cos(e.heading),std::sin(e.heading),.5f},.95f,6,false);
   strikeArc(m_level,contact,body,.1f,.85f,true);}
  else e.sparkClock-=dt;
  if(std::fmod(e.shock,.3f)<dt)m_sounds.push_back({Sound::LeadArc,e.pos,.8f,.9f+.2f*float(int(e.shock*10)%3),true,18,mid});
  if(e.shock>=ShortSeconds){shortMachine(e,contact);return true;}
  return true;
 }
 // Let go, it stands seized a while, fizzing.
 if(e.shock>0&&e.sparkClock>-Seize){e.sparkClock-=dt;e.moving=false;e.painFlash=std::max(e.painFlash,.7f);
  if(int((e.sparkClock+Seize)*6.f)!=int((e.sparkClock+dt+Seize)*6.f))sparkBurst(m_level,{e.pos.x,e.pos.y,mid},{0,0,1},.8f,4,false);
  return true;}
 if(e.sleeping()){
  e.moving=false;
  const Vec2 toPlayer=e.pos-m_player.pos;const float far=length(toPlayer);
  if(state(e.wakeState)){wakeMachine(e,m_player.pos,m_player.z,false);return true;}             // its room's power came on
  if(e.hp<e.maxHp){wakeMachine(e,m_player.pos,m_player.z,true);return true;}                    // hurt
  // A torch in its face, close.
  if(flashlightOn()&&far<TorchReach&&far>.01f&&std::fabs(m_player.z-e.z)<2.f){const Vec2 look{std::cos(m_player.angle),std::sin(m_player.angle)};
   if(dot(look,toPlayer*(1.f/far))>.9f&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,e.pos,mid)){wakeMachine(e,m_player.pos,m_player.z,true);return true;}}
  // A loud noise near it: a shot, a crash, a blast.
  for(const auto&s:stimuli){if(s.step||!(s.loud||(s.categories&(ai::SoundWeapon|ai::SoundDanger))))continue;
   const float d=length(s.source-e.pos);if(d>.5f&&d<std::min(NoiseReach,s.radius*t.hearingScale)&&std::fabs(s.z-e.z)<3.f){
    const auto local=s.source;wakeMachine(e,local,s.z-.5f,false);return true;}}
  return true;
 }
 // Starting up: the lamp catching, the motors grinding, turning toward what woke it.
 if(e.stalkMode==Enemy::StalkMode::Recover){
  e.stalkTimer=std::max(0.f,e.stalkTimer-dt);e.moving=false;
  const Vec2 to=e.lastKnown-e.pos;if(lengthSq(to)>.01f)e.heading+=wrapAngle(std::atan2(to.y,to.x)-e.heading)*std::min(1.f,dt*1.5f);
  if(e.stalkTimer<=0)e.stalkMode=Enemy::StalkMode::Watch;
  return true;
 }
 // Light draws it: a torch on it from across the room is as good as a sound.
 if(flashlightOn()&&std::fabs(m_player.z-e.z)<2.f){const Vec2 toPlayer=e.pos-m_player.pos;const float far=length(toPlayer);const Vec2 look{std::cos(m_player.angle),std::sin(m_player.angle)};
  if(far>.01f&&far<12.f&&dot(look,toPlayer*(1.f/far))>.8f&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,e.pos,mid)){
   e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;e.awareness=std::max(e.awareness,6.f);if(e.state==Enemy::State::Idle||e.state==Enemy::State::Search)e.state=Enemy::State::Investigate;}}
 // Its motors as it raises the lantern: a long grind under the wind-up.
 if(e.windup>t.windup-dt*1.5f&&e.windup>0)m_sounds.push_back({Sound::LiftMotor,e.pos,.6f,.8f,true,16,mid});
 return false;
}
Game Game::machineScene(int which){
 if(which==0){
  // A lit test floor: a band of live floor (the electrodes' beat, .6 s in 2.7) between the unit and the player.
  auto m=regression::room();m->definition.ceiling=4.f;m->definition.ambient=.2f;m->definition.playerStart={5.f,4.2f};
  m->lights.push_back({{9.f,6.5f},3.6f,WorldLightMount::Ceiling,0,1.f,10});m->lights.push_back({{13.f,9.f},3.6f,WorldLightMount::Ceiling,0,.7f,8});
  Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=9;h.x2=10.4f;h.y1=3;h.y2=10;h.bottom=-.6f;h.top=2.2f;h.period=2.7f;h.onTime=.6f;h.phase=1.4f;m->hazards.push_back(h);
  Game g(regression::campaign(m));g.m_enemies.clear();g.m_god=true;g.m_inspectionCamera=true;
  g.m_player.pos={5.2f,4.4f};g.m_player.z=0;g.m_player.angle=std::atan2(2.1f,5.8f);g.m_player.pitch=-6;
  CreatureSpawn s{CreatureKind::Machine,{13.f,6.5f},0};g.spawnCreature(s,0,false);auto&e=g.m_enemies.back();e.heading=kPi;
  g.wakeMachine(e,g.m_player.pos,0,true);return g;}
 // Experimental Power: the unit parked in the exit, the player north of the split feed with a torch on it.
 auto g=mapInspection({12.4f,16.6f},kPi*.5f,-10,92,false,-16,false);g.m_god=true;
 std::erase_if(g.m_enemies,[](const Enemy&e){return e.kind!=CreatureKind::Machine;});
 if(!g.m_enemies.empty()){auto&e=g.m_enemies[0];g.wakeMachine(e,g.m_player.pos,g.m_player.z,true);}
 return g;
}
// --each-test=machine: asleep until woken, every way it wakes, rounds that spark off it, the plant's
// current that kills it, the oil it bleeds, and a save of one asleep and one awake.
bool Game::testMachine(){
 std::ofstream report("machine-test.txt");bool passed=true;
 auto check=[&](bool ok,const std::string&name,const std::string&v){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<v<<'\n';report.flush();passed&=ok;};
 constexpr float dt=1.f/60;const StateId power=stateId("machine_test_power");
 auto scene=[&](std::function<void(AuthoredMapData&)> setup){auto m=regression::room();m->definition.playerStart={4.f,6.5f};
  m->terminals.push_back({{12.f,6.5f},"PANEL","A","B"});
  if(setup)setup(*m);
  auto g=std::make_unique<Game>(regression::campaign(m));g->m_enemies.clear();g->m_player.pos={4.f,6.5f};g->m_player.z=0;g->m_player.angle=kPi;return g;};
 auto place=[&](Game&g,Vec2 at,StateId wake=0){CreatureSpawn s{CreatureKind::Machine,at,0};s.wakeState=wake;g.spawnCreature(s,0,false);auto&e=g.m_enemies.back();e.heading=kPi;e.scriptSpawned=true;return &e;};
 // A round from the player's eye at a point, as the shotgun's hitscan resolves it.
 auto fire=[&](Game&g,Point3 at){const Point3 eye{g.m_player.pos.x,g.m_player.pos.y,g.m_player.z+g.m_player.eye};Point3 d=at-eye;const float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);d=d*(1.f/l);
  auto hit=g.traceShot(eye,d,24);g.resolveShot(hit,eye,d,weaponDef(WeaponId::Shotgun).hitscan,true);return hit.kind==ShotKind::Enemy;};
 auto tick=[&](Game&g,float seconds,InputState input={}){for(int i=0;i<int(seconds/dt);++i)g.update(input,dt);};
 auto awake=[](const Enemy&e){return e.alive&&!e.sleeping();};
 // 1. Asleep it does nothing: the player walks round it, it neither moves nor hears footsteps.
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{9.f,6.5f});const Vec2 at=e->pos;
  for(int i=0;i<int(3.f/dt);++i){g.m_player.pos={6.f+std::sin(i*.03f),6.5f+std::cos(i*.03f)};g.update({},dt);}
  const auto sample=creatureAnimationSample(*e,g.elapsed());
  check(e->sleeping()&&length(e->pos-at)<.001f&&sample.clip==0&&sample.phase==0,"1 asleep among the plant it stands frozen","moved="+std::to_string(length(e->pos-at))+" clip="+std::to_string(sample.clip)+" phase="+std::to_string(sample.phase));}
 // 2. Every way it wakes, and that it starts up before it moves.
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{9.f,6.5f},power);tick(g,1.f);const bool before=e->sleeping();g.setState(power,1);g.update({},dt);
  const bool starting=awake(*e)&&e->stalkMode==Enemy::StalkMode::Recover;const Vec2 at=e->pos;tick(g,1.f);const float still=length(e->pos-at);tick(g,3.f);
  check(before&&starting&&still<.01f&&length(e->pos-at)>.3f,"2 power coming on in its room wakes it; it starts up, then comes","starting="+std::to_string(starting)+" moved in start-up="+std::to_string(still)+" after="+std::to_string(length(e->pos-at)));}
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{16.f,6.5f});g.m_player.pos={12.f,5.3f};g.m_player.angle=kPi*.5f;
  InputState use{};use.use=true;g.update(use,dt);g.update({},dt);
  auto far=scene(nullptr);auto*distant=place(*far,{22.f,20.f});far->m_player.pos={12.f,5.3f};far->m_player.angle=kPi*.5f;far->update(use,dt);far->update({},dt);
  check(awake(*e)&&distant->sleeping(),"2 a terminal worked 4 m away wakes it; 15 m away it sleeps on","near="+std::to_string(awake(*e))+" far="+std::to_string(!distant->sleeping()));}
 for(bool lit:{false,true}){auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{8.f,6.5f});g.giveQuestItem(Flashlight);g.setState(stateId("flashlight_on"),lit?1:0);g.m_player.angle=0;
  tick(g,.5f);check(lit?awake(*e):e->sleeping(),lit?"2 a torch in its face at 4 m wakes it":"2 the same, torch off: it sleeps on","awake="+std::to_string(awake(*e)));}
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{9.f,6.5f});g.m_player.angle=kPi*.5f;tick(g,.5f);const bool before=e->sleeping();g.sound(Sound::Shot);g.updateEnemies(dt);tick(g,.2f);
  check(before&&awake(*e)&&e->hp>=e->maxHp,"2 a shot fired 5 m away (not at it) wakes it","awake="+std::to_string(awake(*e))+" hp="+std::to_string(e->hp));}
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{9.f,6.5f});g.applyEnemyDamage(*e,1,false,false);g.update({},dt);
  check(awake(*e)&&e->recognition>=1,"2 a wound wakes it knowing where the player is","");}
 // 3. Rounds spark off it: a shotgun at 3 m takes little, throws sparks and oil, and it does not flinch.
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{7.f,6.5f});g.m_player.angle=0;
  const float hp=e->hp;const size_t sparks=g.m_sparkBursts.size();
  const bool struck=fire(g,{7.f,6.5f,1.f});const float taken=hp-e->hp;const float full=hitscanDamageAt(weaponDef(WeaponId::Shotgun).hitscan,3.f);
  bool oil=false;for(const auto&b:g.hitBursts())oil|=b.material==ImpactMaterial::Flesh&&b.species==int(CreatureKind::Machine);
  std::ostringstream v;v<<"taken "<<taken<<" of "<<full<<", sparks "<<sparks<<" -> "<<g.m_sparkBursts.size()<<", oil "<<oil<<", flinch "<<e->painFlash;
  check(struck&&taken>0&&taken<=full*.2f&&g.m_sparkBursts.size()>sparks&&oil&&e->painFlash<.5f,"3 a round mostly sparks off: an eighth gets through, it bleeds oil and keeps coming",v.str());
  int shots=0;while(e->alive&&shots<40){fire(g,{e->pos.x,e->pos.y,1.f});++shots;}
  check(shots>=20,"3 it takes twenty and more rounds to stop it","shots="+std::to_string(shots)+" alive="+std::to_string(e->alive));}
 // 4. The plant's current kills it: a live floor shorts it out in about a second, its lamp blown.
 {auto held=scene([&](AuthoredMapData&m){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=11;h.x2=13;h.y1=5.5f;h.y2=7.5f;h.bottom=-.5f;h.top=2.f;h.enabledFlag=power;m.hazards.push_back(h);});
  auto&g=*held;auto*e=place(g,{12.f,6.5f});tick(g,.5f);const float hp=e->hp;const bool slept=e->sleeping();g.setState(power,1);
  float died=-1;for(int i=0;i<int(4.f/dt);++i){g.update({},dt);if(!e->alive){died=float(i)*dt;break;}}
  std::ostringstream v;v<<"dead after "<<died<<" s from "<<hp<<" hp, shorted "<<e->shorted<<", sparks "<<g.m_sparkBursts.size()<<", kills "<<g.m_kills;
  check(slept&&died>.4f&&died<2.f&&e->shorted&&g.m_kills==1&&g.m_sparkBursts.size()>=3,"4 live current shorts it out in a second or two",v.str());
  const auto sample=creatureAnimationSample(*e,g.elapsed());tick(g,2.f);
  check(sample.clip==4&&!e->alive&&e->visible(),"4 it goes down in its death clip and lies as a body","clip="+std::to_string(sample.clip));}
 // ...and the electrodes' strike (on .6 s in 2.7) seize it where it stands until the second strike finishes it.
 {auto held=scene([&](AuthoredMapData&m){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=9;h.x2=10.4f;h.y1=1;h.y2=23;h.bottom=-.6f;h.top=2.2f;h.period=2.7f;h.onTime=.6f;h.phase=2.f;m.hazards.push_back(h);});
  auto&g=*held;g.m_player.pos={4.f,6.5f};auto*e=place(g,{13.f,6.5f});e->wakeState=0;e->awareness=8;e->state=Enemy::State::Investigate;e->lastKnown=g.m_player.pos;e->recognition=0;
  float died=-1;for(int i=0;i<int(14.f/dt);++i){g.update({},dt);if(!e->alive){died=float(i)*dt;break;}}
  check(!e->alive&&e->shorted&&e->pos.x>8.5f,"4 coming at the player through the electrodes, it never crosses them","died at "+std::to_string(died)+" s, x="+std::to_string(e->pos.x));}
 // 5. A blast hurts it properly.
 {auto held=scene(nullptr);auto&g=*held;auto*e=place(g,{9.f,6.5f});e->wakeState=0;const float hp=e->hp;
  g.spawnTacticalGrenade({9.4f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,3.f);
  // Through its hide no blast could take more than 64 x armour; it takes more than that.
  check(hp-e->hp>64.f*creatureTraits(CreatureKind::Machine).armour+1.f,"5 a grenade's blast is not a round: it takes it in full","taken="+std::to_string(hp-e->hp));}
 // 6. Saves keep one asleep and one awake.
 {auto held=scene(nullptr);auto&g=*held;place(g,{9.f,6.5f});auto*b=place(g,{15.f,15.f});g.applyEnemyDamage(*b,1,false,false);tick(g,2.f);
  Game loaded(g.m_customCampaign);const bool ok=loaded.decodeSave(g.encodeSave());
  const bool kept=ok&&loaded.m_enemies.size()==2&&loaded.m_enemies[0].kind==CreatureKind::Machine&&loaded.m_enemies[0].sleeping()&&!loaded.m_enemies[1].sleeping()&&loaded.m_enemies[1].alive;
  check(kept,"6 a save keeps the sleeper asleep and the woken one awake","decode="+std::to_string(ok)+" count="+std::to_string(loaded.m_enemies.size()));
  if(kept){loaded.update({},dt);tick(loaded,1.f);check(loaded.m_enemies[0].sleeping(),"6 after the load the sleeper sleeps on","");}}
 // 7. Its mesh as the renderer sizes it: the bind stands as its idle does (the renderer scales the bind to
 // its traits' top), and every clip keeps it on the floor and under the top it was built to.
 {Mesh mesh(2441,"",2443);const float bind=mesh.maximum.y-mesh.minimum.y;float lowest=1e9f,highest=-1e9f;
  for(int clip=0;clip<5;++clip)for(float phase:{0.f,.33f,.66f,1.f}){mesh.poseCreature(clip,phase);for(const auto&t:mesh.triangles)for(const auto&v:t.v){lowest=std::min(lowest,v.p.y);highest=std::max(highest,v.p.y);}}
  std::ostringstream v;v<<"bind "<<mesh.minimum.y<<".."<<mesh.maximum.y<<" ("<<bind<<" tall), clips "<<lowest<<".."<<highest<<", "<<mesh.triangles.size()<<" triangles";
  check(bind>1.95f&&bind<2.2f&&lowest>-.02f&&highest<2.2f,"7 the mesh stands about 2.05 m on its bind, and its clips keep to the floor",v.str());}
 report<<(passed?"ALL PASSED\n":"FAILED\n");return passed;
}
}
