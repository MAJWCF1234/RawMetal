#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
#include <filesystem>
#include <sstream>

namespace retro {
namespace {
auto exclusionCampaign(){auto path=std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt";return std::filesystem::exists(path)?loadCustomCampaignFile(path):loadEmbeddedExclusionZoneCampaign();}
}
Game Game::exclusionInspection(int stage){
 const bool endurance=stage==12;if(endurance)stage=2;
 if(stage<0||stage>11)throw std::runtime_error("Unknown Exclusion encounter view");
 if(stage>=10){
  Game g;g.loadLevel(14,false);g.updateStreaming(0);g.m_player.pos={1.4f,20};g.m_player.z=-25;
  std::vector<size_t> detail;for(size_t i=0;i<g.npcActors().size();++i)if(g.npcActors()[i].homeLevel==14&&g.npcActors()[i].track().ai.squad==stateId("receiving_cleanup"))detail.push_back(i);
  if(detail.size()!=6)throw std::runtime_error("Receiving breach preview requires the authored detail");
  // Stage the two opening player kills through the real damage path; the
  // remaining deaths and retreat are produced by ordinary combat/navigation.
  for(auto index:{detail[0],detail[2]})g.damageFriendly(index,1000,{7,22},{ShotKind::Player,14,0});
  g.updateScripts(0);for(int frame=0;frame<3600;++frame)g.update({},1.f/60);
  if(!g.state("receiving_retreat_sealed"))throw std::runtime_error("Receiving retreat did not complete");
  if(stage==10){g.m_player.pos={10.8f,21.8f};g.m_player.angle=std::atan2(16.f-21.8f,13.f-10.8f);g.m_player.pitch=-12;}
  else{g.loadLevel(15,false);g.m_player.pos={12,14};g.m_player.angle=kPi*.5f;g.m_player.pitch=-6;}
  g.m_player.z=-25;g.m_logTime=0;g.m_activeLog=-1;g.equipWeapon(WeaponId::Fists);return g;
 }
 if(stage==8){Game g;g.loadLevel(14,false);g.updateStreaming(0);g.m_player.pos={10.8f,21.8f};g.m_player.z=-25;g.m_player.angle=std::atan2(16.f-21.8f,13.f-10.8f);g.m_player.pitch=-6;g.equipWeapon(WeaponId::Fists);return g;}
 Game g(exclusionCampaign());
 if(stage==9){std::ostringstream trace;if(!g.walkExclusionEast(trace))throw std::runtime_error("East walking proof failed: "+trace.str());
  size_t actor=0;while(actor<g.npcActors().size()&&g.npcActors()[actor].track().ai.role!=GuardSquadRole::Rusher)++actor;
  float duration=cleanupAnimationDuration(g.npcActors()[actor].track().visual,4);for(int i=0;i<int((duration+.1f)*60);++i)g.update({},1.f/60);
  auto delta=g.npcActors()[actor].pose.position-g.player().pos;g.m_player.angle=std::atan2(delta.y,delta.x);g.m_player.pitch=-130;g.m_logTime=0;g.m_activeLog=-1;return g;}
 if(stage==7){g.m_player.pos={17.3f,20.5f};g.m_player.z=g.groundHeight(g.player().pos);g.m_player.angle=std::atan2(17.f-20.5f,21.f-17.3f);g.m_player.pitch=-6;g.equipWeapon(WeaponId::Fists);return g;}
 auto cache=[&](){InputState use;use.use=true;g.update(use,1.f/60);g.update({},1.f/60);g.update(use,1.f/60);g.update({},1.f/60);};
 if(stage==3||stage==4){
  size_t actor=0;while(actor<g.npcActors().size()&&g.npcActors()[actor].track().ai.role!=GuardSquadRole::Assault)++actor;
  if(actor==g.npcActors().size())throw std::runtime_error("Exclusion preview has no north patrol");
  auto pose=g.npcActors()[actor].pose;Vec2 forward{std::sin(pose.yaw),std::cos(pose.yaw)};
  g.m_player.pos=pose.position-forward*.95f;g.m_player.z=g.groundHeight(g.m_player.pos,float(World::TerrainMaxZ+1));g.m_player.angle=std::atan2(forward.y,forward.x);g.m_player.pitch=0;
  InputState use;use.use=true;g.update(use,1.f/60);g.update({},1.f/60);
  if(g.heldNpc()!=int(actor))throw std::runtime_error("Native rear grab did not capture the authored patrol");
  if(stage==4){InputState subdue;subdue.subdue=true;g.update(subdue,1.f/60);float duration=cleanupAnimationDuration(g.npcActors()[actor].track().visual,4);for(int i=0;i<int((duration+.1f)*60);++i)g.update({},1.f/60);
   g.m_player.pos={5.f,11.35f};g.m_player.z=g.groundHeight(g.m_player.pos,float(World::TerrainMaxZ+1));auto body=g.npcActors()[actor].pose.position-g.player().pos;g.m_player.angle=std::atan2(body.y,body.x);g.m_player.pitch=-58;
  }
  return g;
 }
 if(stage==6){cache();g.m_player.pos={4.3f,6.7f};g.m_player.z=g.groundHeight(g.m_player.pos,float(World::TerrainMaxZ+1));g.m_player.angle=kPi*.5f;InputState use;use.use=true;g.update(use,1.f/60);return g;}
 if(stage==5){g.giveQuestItem(Flashlight);g.setState(stateId("flashlight_on"),1);g.equipWeapon(WeaponId::Shotgun);g.m_player.angle=kPi*.5f;return g;}
 // These cameras observe the authored encounter, without changing its actors,
 // perception, health, cover or schedules.
 if(stage==1){
  cache();
  g.m_player.pos={4.3f,6.7f};g.m_player.z=g.groundHeight(g.m_player.pos);g.m_player.angle=kPi*.5f;
  InputState use;use.use=true;g.update(use,1.f/60);
  for(int i=0;i<240;++i)g.update({},1.f/60);
  g.m_logTime=0;g.m_activeLog=-1;
 }
 const Vec2 cameras[]={{4.1f,6.2f},{5.f,11.35f},{12.f,19.5f}};
 const Vec2 targets[]={{4.1f,5.1f},{9.6f,10.2f},{12.f,14.f}};
 g.m_player.pos=cameras[stage];g.m_player.z=g.groundHeight(cameras[stage]);
 g.m_player.angle=std::atan2(targets[stage].y-cameras[stage].y,targets[stage].x-cameras[stage].x);g.m_player.pitch=-6;
 if(!g.hullFits(g.player().pos,g.player().z,Player::StandingHeight))throw std::runtime_error("Exclusion preview camera intersects cover");
 g.equipWeapon(WeaponId::Fists);
 // Benchmark only: keep the target alive through a sustained squad encounter.
 if(endurance)g.m_player.health=100000;
 for(int i=0;i<12;++i)g.update({},1.f/60);
 return g;
}
// Shared native replay for both the regression and the rendered body proof.
// Only the player is staged at the previously authorized console; actors keep
// their authored poses, perception, collision and schedules throughout.
bool Game::walkExclusionEast(std::ostream& report){
  auto& east=*this;east.setState(stateId("exclusion_relay_secured"),1);
  east.m_player.pos={17.3f,20.5f};east.m_player.z=east.groundHeight(east.player().pos);east.m_player.angle=-kPi*.5f;
  InputState use;use.use=true;east.update(use,1.f/60);
  if(!east.state("exclusion_east_lure"))return false;
  east.update({},1.f/60);east.update(use,1.f/60);east.update({},1.f/60); // Close CRT through E before moving.
  size_t rusher=0;while(rusher<east.npcActors().size()&&east.npcActors()[rusher].track().ai.role!=GuardSquadRole::Rusher)++rusher;
  bool investigated=false;
  auto walk=[&](Vec2 target){for(int i=0;i<480&&length(east.player().pos-target)>.12f&&east.player().health==100;++i){auto d=target-east.player().pos;east.m_player.angle=std::atan2(d.y,d.x);InputState step;step.forward=true;step.crouch=true;east.update(step,1.f/60);if(rusher<east.npcActors().size())investigated|=(east.guardState(rusher)==GuardState::Investigate||east.guardState(rusher)==GuardState::Search)&&east.state(east.friendlyKey(rusher,"guard_x"))==21000&&east.state(east.friendlyKey(rusher,"guard_y"))==17000;}bool reached=length(east.player().pos-target)<.2f;report<<"east route "<<target.x<<','<<target.y<<" reached="<<reached<<" at="<<east.player().pos.x<<','<<east.player().pos.y<<" hp="<<east.player().health<<'\n';return reached;};
  bool staged=walk({16.45f,20.3f})&&walk({16.45f,18.f});bool rear=false;
  if(staged&&rusher<east.npcActors().size())for(int i=0;i<900&&east.player().health==100;++i){
   auto pose=east.npcActors()[rusher].pose;auto mode=east.guardState(rusher);
   if(i%60==0)report<<"east t="<<i<<" mode="<<int(mode)<<" g="<<pose.position.x<<','<<pose.position.y<<" yaw="<<pose.yaw<<" p="<<east.player().pos.x<<','<<east.player().pos.y<<'\n';investigated|=(mode==GuardState::Investigate||mode==GuardState::Search)&&east.state(east.friendlyKey(rusher,"guard_x"))==21000&&east.state(east.friendlyKey(rusher,"guard_y"))==17000;
   Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)};auto delta=pose.position-facing*.95f-east.player().pos;
   east.m_player.angle=std::atan2(pose.position.y-east.player().pos.y,pose.position.x-east.player().pos.x);
   if(east.canTakedown(rusher)){InputState q;q.crouch=true;q.subdue=true;east.update(q,1.f/60);rear=east.npcIncapacitated(rusher);break;}
   InputState step;step.crouch=true;
   if(length(pose.position-Vec2{21,17})<1.5f&&(mode==GuardState::Investigate||mode==GuardState::Search)){east.m_player.angle=std::atan2(delta.y,delta.x);step.forward=true;}
   east.update(step,1.f/60);
  }
  if(rusher<east.npcActors().size())report<<"east final player="<<east.player().pos.x<<','<<east.player().pos.y<<" hp="<<east.player().health<<" guard="<<int(east.guardState(rusher))<<" pos="<<east.npcActors()[rusher].pose.position.x<<','<<east.npcActors()[rusher].pose.position.y<<" yaw="<<east.npcActors()[rusher].pose.yaw<<'\n';
  report<<"east physical KO="<<rear<<" real noise memory="<<investigated<<'\n';
  return staged&&investigated&&rear&&east.player().health==100;
}
bool Game::testExclusion(){
 std::ofstream report("exclusion-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';};
 Game g(exclusionCampaign());
 check(g.world().signs().size()>=4&&g.world().lights().size()>=4,"Wayfinding signs and task lamps are present in the loaded campaign payload");
 check(g.chunkCount()==3,"Encounter stays within the existing three Exclusion chunks");
 check(g.hullFits(g.player().pos,g.player().z,Player::StandingHeight),"Entry hull is clear of authored cover");
 unsigned roles=0;
 for(size_t actor=0;actor<g.npcActors().size();++actor){const auto&npc=g.npcActors()[actor];const auto&track=npc.track();
  if(track.ai.mode!=ActorAiMode::Guard)continue;
  int role=int(track.ai.role);if(role>=1&&role<=6)roles|=1u<<(role-1);
  check(track.health==90,"Existing cleanup health remains compatible with support encounters");
  check(g.friendlyBodyFits(npc.level,npc.pose.position,npc.pose.z,track.scale),"Squad spawn has a clear standing hull");
  for(const auto&point:track.ai.patrol)check(g.friendlyBodyFits(npc.level,point.position,point.z,track.scale),"Patrol pause has a clear standing hull");
 }
 check(roles==63,"All six baked cleanup roles participate");
 {
  Game beat(exclusionCampaign());InputState brief;brief.use=true;beat.update(brief,1.f/60);beat.update({},1.f/60);beat.update(brief,1.f/60);beat.update({},1.f/60);
  beat.m_player.pos={4.3f,6.7f};beat.m_player.z=beat.groundHeight(beat.m_player.pos);beat.m_player.angle=kPi*.5f;
  check(beat.nearbyTerminal()>=0,"North distraction control is physically usable from concealment");
  InputState use;use.use=true;beat.update(use,1.f/60);
  check(beat.state("exclusion_north_lure")>0,"E starts the authored north distraction");
  bool investigated=false,quiet=true;
  for(int frame=0;frame<600;++frame){beat.update({},1.f/60);
   for(size_t actor=0;actor<beat.npcActors().size();++actor)if(beat.npcActors()[actor].track().ai.role==GuardSquadRole::Assault){investigated|=beat.guardState(actor)==GuardState::Investigate;quiet&=beat.guardState(actor)!=GuardState::Alert;}
  }
  check(investigated&&quiet,"The north machinery lure causes a real investigation without revealing the concealed player");
  report<<"north timer="<<beat.state("exclusion_north_lure")<<" done="<<beat.state("exclusion_north_lure_done")<<'\n';
 }
 for(int startDelay:{0,15,30}){
  report<<"north start delay frames="<<startDelay<<'\n';
  Game sneak(exclusionCampaign());InputState use;use.use=true;sneak.update(use,1.f/60);sneak.update({},1.f/60);sneak.update(use,1.f/60);sneak.update({},1.f/60);
  sneak.m_player.pos={4.3f,6.7f};sneak.m_player.z=sneak.groundHeight(sneak.m_player.pos);sneak.m_player.angle=kPi*.5f;sneak.update(use,1.f/60);
  sneak.update({},1.f/60);sneak.update(use,1.f/60);sneak.update({},1.f/60); // Close the machinery CRT.
  for(int frame=0;frame<startDelay;++frame)sneak.update({},1.f/60);
  size_t assault=0;while(assault<sneak.npcActors().size()&&sneak.npcActors()[assault].track().ai.role!=GuardSquadRole::Assault)++assault;
  const Vec2 lure{9.6f,10.2f};
  auto walk=[&](Vec2 target){for(int i=0;i<480&&length(sneak.player().pos-target)>.12f;++i){auto delta=target-sneak.player().pos;sneak.m_player.angle=std::atan2(delta.y,delta.x);InputState step;step.forward=true;step.crouch=true;sneak.update(step,1.f/60);}bool reached=length(sneak.player().pos-target)<.2f;report<<"route "<<target.x<<','<<target.y<<" reached="<<reached<<" at="<<sneak.player().pos.x<<','<<sneak.player().pos.y<<','<<sneak.player().z<<" guard="<<int(sneak.guardState(assault))<<" gpos="<<sneak.npcActors()[assault].pose.position.x<<','<<sneak.npcActors()[assault].pose.position.y<<'\n';return reached;};
  // Stay west of the screen until the Assault is busy at the lure. Investigate
  // often flips to Search immediately on arrival; either is fine if they face away.
  bool staged=walk({5.35f,8.4f})&&walk({5.35f,11.45f});
  bool committed=false;bool facingAway=false;
  if(assault<sneak.npcActors().size()){
   for(int i=0;i<900&&!(committed&&facingAway);++i){sneak.update({},1.f/60);
    auto pose=sneak.npcActors()[assault].pose;Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)};
    auto mode=sneak.guardState(assault);
    committed=(mode==GuardState::Investigate||mode==GuardState::Search)&&length(pose.position-lure)<1.45f;
    // West-screen approach is safe while the patrol looks roughly east/north-east.
    facingAway=committed&&facing.x>.35f&&dot(normalized(sneak.player().pos-pose.position),facing)< -.2f;
   }
  }
  report<<"lure commit="<<committed<<" facingAway="<<facingAway<<" after staging\n";
  bool routed=staged&&committed&&facingAway;
  bool rear=false;
  if(routed&&assault<sneak.npcActors().size()){
   for(int i=0;i<300&&!sneak.dead()&&sneak.player().health==100;++i){
    auto pose=sneak.npcActors()[assault].pose;Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)};
    Vec2 behind=pose.position-facing*.95f;
    auto delta=behind-sneak.player().pos;
    // Face the guard for the action; steering toward an offset rear waypoint
    // can point away from the guard precisely as melee reach becomes valid.
    sneak.m_player.angle=std::atan2(pose.position.y-sneak.player().pos.y,pose.position.x-sneak.player().pos.x);
    if(sneak.canTakedown(assault)){InputState q;q.subdue=true;q.crouch=true;sneak.update(q,1.f/60);rear=sneak.npcIncapacitated(assault);break;}
    if(length(delta)<.14f){sneak.m_player.angle=std::atan2(facing.y,facing.x);sneak.update({},1.f/60);continue;}
    sneak.m_player.angle=std::atan2(delta.y,delta.x);
    InputState step;step.forward=true;step.crouch=true;sneak.update(step,1.f/60);
   }
   report<<"physical approach player="<<sneak.player().pos.x<<','<<sneak.player().pos.y<<" health="<<sneak.player().health<<" guard="<<int(sneak.guardState(assault))<<" gpos="<<sneak.npcActors()[assault].pose.position.x<<','<<sneak.npcActors()[assault].pose.position.y<<" yaw="<<sneak.npcActors()[assault].pose.yaw<<'\n';
  }
  check(routed&&rear&&sneak.player().health==100,"Actual crouched movement around the west screen reaches a rear knockout after the authored distraction");
 }
 {Game east(exclusionCampaign());check(east.walkExclusionEast(report),"Real east winch hearing, crouched west-flank movement and Q produce a rear knockout at 100 health");}
 for(int stage=0;stage<3;++stage){try{auto scene=exclusionInspection(stage);check(!scene.dead(),"Native encounter preview remains alive");}catch(const std::exception&e){report<<e.what()<<'\n';check(false,"Native encounter preview remains alive");}}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}
