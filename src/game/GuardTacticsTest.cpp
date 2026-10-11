#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testGuardTactics(){
 std::ofstream report("guard-tactics-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](bool armed=true){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard repositioning validation";campaign->key=882742;
  auto map=std::make_shared<AuthoredMapData>();map->name="Cover and side-gap room";map->definition={{0,0},{10.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("tactics_clock");timer.finishState=stateId("tactics_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{30000,{6.5f,6.5f},0,kPi*.5f}};
  guard.scale=1.6f;guard.health=70;guard.damageState=stateId("tactics_guard_damage");guard.deadState=stateId("tactics_guard_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.armed=armed;guard.ai.patrol={{{6.5f,6.5f},0,1000}};
  map->actorTracks.push_back(guard);map->structures.push_back({8,6,8.15f,7,0,1.3f});campaign->maps.push_back(map);return Game(campaign);
 };
 auto changed=[](Game&g,auto edit){auto campaign=std::make_shared<CustomCampaign>(*g.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);edit(*map);campaign->maps[0]=map;return Game(campaign);};
 auto tick=[](Game&g,int frames,int rate=60){int shots=0;for(int i=0;i<frames;++i){g.update({},1.f/rate);for(const auto&sound:g.sounds())shots+=sound.sound==Sound::Shot;}return shots;};
 auto safeShot=[](Game&g,size_t actor){const auto&npc=g.npcActors()[actor];auto weapon=g.npcWeaponPose(actor);auto shift=g.chunkOffset(npc.level)-g.chunkOffset(g.level());
  Point3 grip=weapon.grip+Point3{shift.x,shift.y,0},muzzle=weapon.muzzle+Point3{shift.x,shift.y,0};
  auto clear=[&](Point3 start,Point3 end){auto delta=end-start;float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);return distance>.0001f&&g.traceShot(start,delta,distance,int(actor),false).kind==ShotKind::None;};
  auto observer=npc.pose.position+shift;
  return clear({observer.x,observer.y,grip.z},grip)&&clear(grip,muzzle)&&g.traceShot(muzzle,weapon.direction,npc.track().ai.attackRange,int(actor),true).kind==ShotKind::Player;
 };
 {auto g=scene();auto pose=g.npcActors()[0].pose;auto weapon=g.npcWeaponPoseAt(0,pose,{10.5f,6.5f,.96f},1);
  check(g.soundRayClear(pose.position,1.36f,g.player().pos,Player::StandingEye)&&g.traceShot(weapon.muzzle,weapon.direction,9,0,true).kind==ShotKind::Solid,"Low-cover fixture has a visible head and a blocked raised muzzle line");
 }
 for(int rate:{60,120}){auto g=scene();const Vec2 start=g.npcActors()[0].pose.position;float greatestMove=0;int shots=0,frame=0;bool safe=true,earlyShot=false,lowReady=true;
  for(;frame<rate*10&&g.player().health==100;++frame){int fired=tick(g,1,rate);shots+=fired;greatestMove=std::max(greatestMove,length(g.npcActors()[0].pose.position-start));if(g.npcActors()[0].pose.clip==1)lowReady&=g.npcWeaponPose(0).raise<.01f;if(fired){safe&=safeShot(g,0);earlyShot|=float(frame+1)/rate<1.2f;}}
  report<<"cover first-fire rate="<<rate<<" seconds="<<float(frame)/rate<<" displacement="<<greatestMove<<" pose="<<g.npcActors()[0].pose.position.x<<','<<g.npcActors()[0].pose.position.y<<'\n';
  check(greatestMove>.2f,"Armed guard physically moves around low cover at 60/120 Hz");
  check(shots==1&&g.player().health==82&&g.state(g.friendlyKey(0,"weapon_rounds"))==5&&frame<=rate*10,"Guard finds a firing position and damages the player within ten seconds at 60/120 Hz");
  check(safe&&!earlyShot,"Repositioned shot clears the body, barrel and intervening cover after its readable aim delay");
  check(lowReady,"Guard keeps the weapon low while repositioning and raises it after stopping at 60/120 Hz");
 }
 {auto g=scene();int frames=0;while(frames<300&&length(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})<.2f&&g.player().health==100){tick(g,1);++frames;}
  check(length(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})>=.2f&&g.player().health==100&&g.npcActors()[0].pose.clip==1,"Save fixture interrupts actual cover movement before the first shot");
  Game loaded(g.m_customCampaign);bool restored=loaded.decodeSave(g.encodeSave());
  bool sameClocks=restored;for(const char*field:{"weapon_aim","weapon_recovery","weapon_rounds","weapon_raise","guard_cover","guard_cover_wait"})sameClocks&=loaded.state(loaded.friendlyKey(0,field))==g.state(g.friendlyKey(0,field));
  check(restored&&length(loaded.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.002f&&loaded.player().health==g.player().health&&sameClocks,"Save restores moving pose, health, ammunition and interrupted tactical/weapon timing");
  if(restored){int firstOriginal=0,firstLoaded=0,shotsOriginal=0,shotsLoaded=0;for(int i=1;i<=360;++i){int a=tick(g,1),b=tick(loaded,1);shotsOriginal+=a;shotsLoaded+=b;if(a&&!firstOriginal)firstOriginal=i;if(b&&!firstLoaded)firstLoaded=i;}
   report<<"save continuation first-fire frames="<<firstOriginal<<','<<firstLoaded<<" position-error="<<length(loaded.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<<'\n';
   check(firstOriginal>0&&std::abs(firstOriginal-firstLoaded)<=1&&shotsOriginal==shotsLoaded&&g.player().health==loaded.player().health&&g.state(g.friendlyKey(0,"weapon_rounds"))==loaded.state(loaded.friendlyKey(0,"weapon_rounds")),"Loaded and uninterrupted guards complete the same detour and firing cadence within one simulation frame");
   check(length(loaded.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.03f&&std::abs(loaded.state(loaded.friendlyKey(0,"weapon_recovery"))-g.state(g.friendlyKey(0,"weapon_recovery")))<=17,"Save continuation retains the firing position and remaining recovery timing");
  }
 }
 {auto original=scene();auto g=changed(original,[](auto&map){map.structures[0]={8,0,8.15f,24,0,1.3f};});int shots=0;float furthest=0;bool inside=true;
  for(int i=0;i<600;++i){shots+=tick(g,1);auto pose=g.npcActors()[0].pose;furthest=std::max(furthest,pose.position.x);inside&=pose.position.x>=.35f&&pose.position.y>=.35f&&pose.position.x<=23.65f&&pose.position.y<=23.65f;}
  report<<"uncrossable cover furthest-x="<<furthest<<'\n';
  check(shots==0&&g.player().health==100&&g.state(g.friendlyKey(0,"weapon_rounds"))==6,"Continuous low cover prevents muzzle fire and ammunition use throughout ten seconds of attempted repositioning");
  check(furthest<8&&inside,"Guard never crosses an uncrossable thin wall or leaves the authored chunk");
 }
 {auto original=scene();auto g=changed(original,[](auto&map){map.structures.clear();auto worker=map.actorTracks[0];worker.ai={};worker.health=0;worker.damageState=stateId("tactics_bystander_damage");worker.deadState=stateId("tactics_bystander_dead");for(auto&key:worker.keys)key.position={8.5f,6.5f};map.actorTracks.push_back(worker);});
  int shots=0,frames=0;bool safe=true;while(frames<600&&g.player().health==100){int fired=tick(g,1);shots+=fired;if(fired)safe&=safeShot(g,0);++frames;}
  auto guard=g.npcActors()[0].pose;auto worker=g.world().actorPose(1);
  check(shots==1&&g.player().health==82&&std::fabs(guard.position.y-6.5f)>.3f&&safe,"Guard sidesteps an intervening invulnerable worker and fires only after a clear line exists");
  check(g.state("tactics_bystander_damage")==0&&g.state("tactics_bystander_dead")==0&&worker.clip!=4&&length(worker.position-Vec2{8.5f,6.5f})<.001f,"Repositioning preserves the living bystander without damage or forced movement");
 }
 {auto original=scene();auto g=changed(original,[](auto&map){map.structures[0]={8,0,8.15f,24,0,1.3f};auto prototype=map.actorTracks[0];map.actorTracks.clear();for(int i=0;i<9;++i){auto guard=prototype;Vec2 position{6.5f,4.5f+float(i)*.5f};for(auto&key:guard.keys)key.position=position;guard.ai.patrol={{position,0,1000}};guard.damageState=stateId("tactics_budget_damage/"+std::to_string(i));guard.deadState=stateId("tactics_budget_dead/"+std::to_string(i));map.actorTracks.push_back(guard);}});
  unsigned serviced=0;bool bounded=true,spent=false;int shots=0;for(int i=0;i<180;++i){shots+=tick(g,1);bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;spent|=g.m_npcNavBudget==0;for(size_t actor=0;actor<9;++actor)if(g.state(g.friendlyKey(actor,"guard_cover_wait"))>0)serviced|=1u<<actor;}
  report<<"unreachable tactical searches serviced-mask="<<serviced<<'\n';
  check(bounded&&spent,"Tactical searches and ordinary navigation retain the shared four-search frame budget");
  check(serviced==511,"All nine unreachable guards receive a tactical search and back off without starving later actors");
  check(shots==0&&g.player().health==100,"Budget-limited unreachable guards keep shots blocked by the continuous cover");
 }
 {auto original=scene(false);auto g=changed(original,[](auto&map){map.structures.clear();map.definition.playerStart={18.5f,18.5f};map.actorTracks[0].ai.patrol={{{6.5f,6.5f},0,1000},{{9.5f,6.5f},0,600},{{9.5f,9.5f},0,600},{{6.5f,9.5f},0,600}};});tick(g,20);bool paused=length(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})<.001f;int shots=tick(g,360);
  check(paused&&g.state(g.friendlyKey(0,"guard_point"))>0&&length(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})>1,"Existing unarmed guards preserve authored pauses and patrol movement");
  check(shots==0&&g.player().health==100&&!g.npcWeaponPose(0).armed&&g.state(g.friendlyKey(0,"guard_cover"))==0,"Existing unarmed guards acquire neither weapons nor tactical cover movement");
 }
 for(const char*field:{"guard_cover","guard_cover_wait","guard_cover_x","guard_cover_y","guard_cover_z"}){auto g=scene();g.setState(g.friendlyKey(0,field),std::numeric_limits<int>::max());Game loaded(g.m_customCampaign);check(!loaded.decodeSave(g.encodeSave()),"Malformed saved repositioning state is rejected safely");}
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
