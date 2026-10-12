#include "../../../Game.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testGuardDefense(){
 std::ofstream report("guard-defense-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard defense validation";campaign->key=882744;
  auto map=std::make_shared<AuthoredMapData>();map->name="Protected defense room";map->definition={{0,0},{10.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);map->structures.push_back({7.1f,7.1f,7.8f,9,0,3});
  TimedSequence timer;timer.timerState=stateId("defense_clock");timer.finishState=stateId("defense_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{60000,{6.5f,6.5f},0,kPi*.5f}};guard.scale=1.6f;guard.health=70;guard.damageState=stateId("defense_guard_damage");guard.deadState=stateId("defense_guard_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.armed=true;guard.ai.patrol={{{6.5f,6.5f},0,1000}};map->actorTracks.push_back(guard);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int frames,int rate){for(int i=0;i<frames;++i)g.update({},1.f/rate);};
 for(int rate:{60,120}){
  auto g=scene();g.damageFriendly(0,50,g.player().pos,{ShotKind::Player,0,0});tick(g,rate,rate);
  check(g.guardState(0)==GuardState::Investigate&&g.state(g.friendlyKey(0,"guard_under_fire"))>0&&g.guardCovered(0,g.npcActors()[0].pose,g.guardTarget(0)),"Recent injury preserves a completed retreat and a protected unseen position");
  g.m_player.pos={18.5f,18.5f};Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"An injured guard holding cover can be saved during its transient damage pause");
  tick(g,rate*20,rate);if(decoded)tick(restored,rate*20,rate);
  check(g.guardState(0)==GuardState::Patrol&&!g.guardTarget(0).valid()&&!g.state(g.friendlyKey(0,"guard_confirmed"))&&g.state("defense_guard_damage")==50,"Permanent low health cannot retain a hidden hostile forever after transient defense expires");
  if(decoded)check(restored.guardState(0)==g.guardState(0)&&!restored.guardTarget(0).valid()&&length(restored.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.02f&&restored.player().health==g.player().health,"Saved and uninterrupted low-health defense both search, forget and resume patrol");
 }
 {auto g=scene();tick(g,60,60);g.setState(g.friendlyKey(0,"weapon_rounds"),0);tick(g,60,60);
  check(g.state(g.friendlyKey(0,"weapon_reload"))>0&&g.state(g.friendlyKey(0,"weapon_reloading"))==1&&g.guardState(0)==GuardState::Investigate&&g.guardCovered(0,g.npcActors()[0].pose,g.guardTarget(0)),"A legitimate empty-tube reload retains cover despite losing personal visibility");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"A canonical partial reload survives save validation");tick(g,30,60);if(decoded)tick(restored,30,60);
  check(g.state(g.friendlyKey(0,"weapon_reload"))>0&&g.guardCovered(0,g.npcActors()[0].pose,g.guardTarget(0))&&g.player().health==100,"Transient reload defense remains protected while its timer is active");
  if(decoded)check(restored.state(restored.friendlyKey(0,"weapon_reload"))==g.state(g.friendlyKey(0,"weapon_reload"))&&length(restored.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.02f,"Saved and uninterrupted protected reloads retain the same remaining clock and pose");
 }
 for(const char*field:{"weapon_saved","weapon_reloading"})for(int value:{-1,2,std::numeric_limits<int>::max()}){auto g=scene();tick(g,1,60);g.setState(g.friendlyKey(0,field),value);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"A valid-checksum save with a malformed weapon boolean is rejected");}
 for(auto values:{std::array<int,4>{1,1,6,0},std::array<int,4>{1,1,6,700},std::array<int,4>{1,1,0,0},std::array<int,4>{1,0,0,700},std::array<int,4>{0,1,0,700},std::array<int,4>{0,0,6,0}}){auto g=scene();tick(g,1,60);const char*fields[]={"weapon_saved","weapon_reloading","weapon_rounds","weapon_reload"};for(int i=0;i<4;++i)g.setState(g.friendlyKey(0,fields[i]),values[i]);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"A valid-checksum save with contradictory reload flags, ammunition or clocks is rejected");}
 {auto g=scene();tick(g,1,60);g.setState(g.friendlyKey(0,"weapon_reloading"),1);tick(g,1,60);check(g.state(g.friendlyKey(0,"weapon_reloading"))==0&&g.state(g.friendlyKey(0,"weapon_reload"))==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==6,"A stale in-memory reload flag on a loaded tube is normalized without an infinite cover hold");}
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
