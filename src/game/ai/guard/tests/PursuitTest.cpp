#include "../../../Game.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testGuardPursuit(){
 std::ofstream report("guard-pursuit-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 // Distinct residency groups exercise real unloading; map IDs deliberately
 // do not describe spatial order. The distant map must never be pulled in.
 auto scene=[](int layout,bool paired=false,bool locked=false,bool raised=false){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard stitched pursuit validation";campaign->key=918043;
  Vec2 home=layout==0?Vec2{22.3f,6.5f}:layout==1?Vec2{6.5f,22.3f}:Vec2{22.3f,20.5f};
  Vec2 seen=layout==0?Vec2{26.5f,6.5f}:layout==1?Vec2{6.5f,26.5f}:Vec2{26.5f,25.5f};
  int count=layout==2?4:3;
  for(int level=0;level<count;++level){auto map=std::make_shared<AuthoredMapData>();map->name="Pursuit area "+std::to_string(level);
   Vec2 origin=level==0?Vec2{}:level==count-1?Vec2{240,240}:layout==0?Vec2{24,0}:layout==1?Vec2{0,24}:level==1?Vec2{24,24}:Vec2{24,0};
   map->definition={origin,{18.5f,18.5f},0,Environment::Interior,false,8,.3f,100+level};
   AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');if(raised&&level==1)layer.elevation=3;map->layers.push_back(layer);
   if(level<count-1){
    if(paired){Door gate{};gate.left=5;gate.right=8;gate.y=level?0.f:24.f;gate.open=1;gate.opening=true;gate.entry=level!=0;gate.transfer=level==0;if(locked)gate.requireState=stateId("pursuit_gate_permission");map->doors.push_back(gate);}
    else if(layout==0){map->openEast=level==0;map->openWest=level==1;}
    else if(layout==1){map->openSouth=level==0;map->openNorth=level==1;}
    else{map->openEast=level==0;map->openWest=level==2;map->openSouth=level==2;map->openNorth=level==1;}
   }
   if(level==0){TimedSequence timer;timer.timerState=stateId("pursuit_clock");timer.finishState=stateId("pursuit_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
    ActorTrack guard;guard.timerState=timer.timerState;float yaw=std::atan2(seen.x-home.x,seen.y-home.y);guard.keys={{0,home,0,yaw},{60000,home,0,yaw}};
    guard.scale=1.6f;guard.health=70;guard.damageState=stateId("pursuit_damage");guard.deadState=stateId("pursuit_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.patrol={{home,0,1000}};map->actorTracks.push_back(guard);
   }
   campaign->maps.push_back(map);
  }
  return Game(campaign);
 };
 auto tick=[](Game&g,int frames,int rate){for(int frame=0;frame<frames;++frame){g.updateStreaming(1.f/rate);g.update({},1.f/rate);}};
 auto global=[](const Game&g){const auto&npc=g.npcActors()[0];return npc.pose.position+g.chunkOffset(npc.level);};
 auto observe=[&](Game&g,int layout,int rate){Vec2 seen=layout==0?Vec2{26.5f,6.5f}:layout==1?Vec2{6.5f,26.5f}:Vec2{26.5f,25.5f};
  g.setState(stateId("pursuit_gate_permission"),1);g.m_player.pos=seen-g.chunkOffset(g.level());g.crossChunkBoundary();g.updateStreaming(0);
  for(int frame=0;frame<rate*2&&g.guardState(0)!=GuardState::Alert;++frame)tick(g,1,rate);
  bool confirmed=g.guardState(0)==GuardState::Alert&&g.state(g.friendlyKey(0,"guard_seen"))&&g.state(g.friendlyKey(0,"guard_confirmed"));
  g.m_player.pos={18.5f,18.5f};tick(g,1,rate);return confirmed;
 };
 for(int rate:{60,120})for(int layout:{0,1,2}){
  auto g=scene(layout);check(observe(g,layout,rate),"Own visual recognition across an open east/south/two-axis seam supplies pursuit knowledge");
  int knownX=g.state(g.friendlyKey(0,"guard_x")),knownY=g.state(g.friendlyKey(0,"guard_y"));bool safe=true,unchanged=true,sawMiddle=false,sawDestination=false,body=false;
  std::unique_ptr<Game> restored;bool decoded=false;int away=g.chunkCount()-1;int maxResident=0;
  for(int frame=0;frame<rate*9;++frame){Vec2 before=global(g);tick(g,1,rate);if(restored)tick(*restored,1,rate);auto after=global(g);const auto&npc=g.npcActors()[0];
   safe&=length(after-before)<=npc.track().ai.speed/rate+.005f&&g.npcActors().size()==1&&npc.pose.position.x>=0&&npc.pose.position.y>=0&&npc.pose.position.x<24&&npc.pose.position.y<24;
   safe&=(!g.chunkResident(npc.level)||g.friendlyWorld(npc.level).fits(npc.pose.position.x,npc.pose.position.y,npc.pose.z,npc.track().scale))&&!g.chunkResident(away);
   int resident=0;for(int level=0;level<g.chunkCount();++level)resident+=g.chunkResident(level);maxResident=std::max(maxResident,resident);
   if(g.state(g.friendlyKey(0,"guard_memory")))unchanged&=knownX==g.state(g.friendlyKey(0,"guard_x"))&&knownY==g.state(g.friendlyKey(0,"guard_y"))&&g.state(g.friendlyKey(0,"guard_seen"))==0;
   sawMiddle|=layout==2&&npc.level==2;sawDestination|=npc.level==1;
   if(npc.level==1)body|=!g.hullFits(npc.pose.position,npc.pose.z,Player::StandingHeight)&&g.hullFits(npc.pose.position,npc.pose.z+3,Player::StandingHeight);
   if(!restored&&npc.level==(layout==2?2:1)&&g.guardState(0)==GuardState::Investigate){restored=std::make_unique<Game>(g.m_customCampaign);decoded=restored->decodeSave(g.encodeSave());if(!decoded)restored.reset();}
  }
  report<<"layout="<<layout<<" rate="<<rate<<" maxResident="<<maxResident<<" physical="<<g.npcActors()[0].level<<'\n';
  check(safe&&maxResident<g.chunkCount(),"Pursuit moves by the authored speed through real hull checks without duplication or distant chunk loading");
  check(unchanged&&g.player().health==100,"Hidden target movement never changes the observed pursuit coordinates or grants visibility");
  check(sawDestination&&(layout!=2||sawMiddle),"Guard physically traverses the nonsequential stitched route and reaches its last-known area");
  check(body,"Migrated guard body blocks its destination floor and leaves the floor above clear");
  check(decoded&&restored&&restored->npcActors().size()==1&&restored->npcActors()[0].homeLevel==0&&restored->npcActors()[0].level==g.npcActors()[0].level&&length(global(*restored)-global(g))<.04f&&restored->guardState(0)==g.guardState(0),"Save during a seam pursuit preserves authored identity, physical ownership and continuation");
  tick(g,rate*13,rate);if(restored)tick(*restored,rate*13,rate);const auto&npc=g.npcActors()[0];
  check(npc.level==0&&length(npc.pose.position-npc.track().ai.patrol[0].position)<.3f&&g.guardState(0)==GuardState::Patrol&&!g.state(g.friendlyKey(0,"guard_memory"))&&!g.guardTarget(0).valid(),"Expired search forgets the threat and physically returns to its authored patrol home");
  check(!g.chunkResident(0)&&!g.chunkResident(away),"Home geometry is released again after the returned guard resumes patrol");
 }
 auto gate=[&](Game&g,float open,bool opening){g.ensureChunk(0);g.friendlyWorld(0).setDoor(0,open,opening);g.friendlyWorld(1).setDoor(0,open,opening);};
 for(int rate:{60,120})for(bool locked:{false,true}){
  auto g=scene(1,true,locked);check(observe(g,1,rate),"Paired transfer gate permits a real visual sighting while open and authorised");
  int knownX=g.state(g.friendlyKey(0,"guard_x")),knownY=g.state(g.friendlyKey(0,"guard_y"));if(locked)g.setState(stateId("pursuit_gate_permission"),0);else gate(g,0,false);
  bool held=true,unchanged=true;for(int frame=0;frame<rate*14;++frame){tick(g,1,rate);held&=g.npcActors()[0].level==0&&global(g).y<24;
   if(g.state(g.friendlyKey(0,"guard_memory")))unchanged&=knownX==g.state(g.friendlyKey(0,"guard_x"))&&knownY==g.state(g.friendlyKey(0,"guard_y"));}
  check(held&&unchanged&&(locked||!g.friendlyWorld(1).doors()[0].opening),"Closed or newly locked paired gate holds pursuit without wall crossing or forcing transfer leaves");
  check(g.guardState(0)==GuardState::Patrol&&!g.state(g.friendlyKey(0,"guard_memory")),"An inaccessible stitched pursuit abandons its old knowledge after the bounded investigation/search");
 }
 for(int rate:{60,120}){
  auto g=scene(1,true);check(observe(g,1,rate),"Interrupted-route fixture starts from confirmed sight rather than a fabricated target");tick(g,rate/3,rate);gate(g,0,false);tick(g,rate,rate);
  check(g.npcActors()[0].level==0&&g.guardState(0)==GuardState::Investigate&&g.state(g.friendlyKey(0,"guard_memory"))&&!g.friendlyWorld(1).doors()[0].opening,"Closing a pending route stops the guard safely while retaining bounded pursuit memory");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"A blocked stitched route saves its pending recovery and last-known destination");
  gate(g,1,true);if(decoded)gate(restored,1,true);tick(g,rate*5,rate);if(decoded)tick(restored,rate*5,rate);
  check(g.npcActors()[0].level==1,"A reopened paired gate recovers the interrupted route using the existing portal planner");
  check(decoded&&restored.npcActors()[0].level==g.npcActors()[0].level&&length(global(restored)-global(g))<.04f&&restored.guardState(0)==g.guardState(0),"Saved and uninterrupted route recovery cross the same gate and retain equivalent state");
 }
 {auto g=scene(1,false,false,true);g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Investigate));g.setState(g.friendlyKey(0,"guard_memory"),1);g.setState(g.friendlyKey(0,"guard_x"),6500);g.setState(g.friendlyKey(0,"guard_y"),26500);g.setState(g.friendlyKey(0,"guard_z"),3000);g.m_player.pos={18.5f,18.5f};tick(g,480,60);
  check(g.npcActors()[0].level==0&&std::fabs(g.npcActors()[0].pose.z)<.001f,"Remembered pursuit cannot bypass an unsafe three-metre elevation discontinuity");}
 for(const char*field:{"guard_route_next","guard_route_target","guard_route_wait","guard_portal_x","guard_portal_y","guard_portal_z"}){auto g=scene(0);g.setState(g.friendlyKey(0,"guard_route_next"),2);g.setState(g.friendlyKey(0,"guard_portal_x"),24350);g.setState(g.friendlyKey(0,"guard_portal_y"),6500);g.setState(g.friendlyKey(0,field),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);
  check(!restored.decodeSave(g.encodeSave()),"Corrupted saved pursuit references and portal coordinates are rejected");}
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
