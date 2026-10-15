#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
#include <filesystem>

namespace retro {
namespace {
auto exclusionCampaign(){return loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt");}
}
Game Game::exclusionInspection(int stage){
 if(stage<0||stage>6)throw std::runtime_error("Unknown Exclusion encounter view");
 Game g(exclusionCampaign());
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
 for(int i=0;i<12;++i)g.update({},1.f/60);
 return g;
}
bool Game::testExclusion(){
 std::ofstream report("exclusion-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';};
 Game g(exclusionCampaign());
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
 {
  Game sneak(exclusionCampaign());InputState use;use.use=true;sneak.update(use,1.f/60);sneak.update({},1.f/60);sneak.update(use,1.f/60);sneak.update({},1.f/60);
  sneak.m_player.pos={4.3f,6.7f};sneak.m_player.z=sneak.groundHeight(sneak.m_player.pos);sneak.m_player.angle=kPi*.5f;sneak.update(use,1.f/60);
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
    auto delta=behind-sneak.player().pos;sneak.m_player.angle=std::atan2(delta.y,delta.x);
    if(sneak.canTakedown(assault)){InputState q;q.subdue=true;q.crouch=true;sneak.update(q,1.f/60);rear=sneak.npcIncapacitated(assault);break;}
    if(length(delta)<.14f){sneak.m_player.angle=std::atan2(facing.y,facing.x);sneak.update({},1.f/60);continue;}
    InputState step;step.forward=true;step.crouch=true;sneak.update(step,1.f/60);
   }
   report<<"physical approach player="<<sneak.player().pos.x<<','<<sneak.player().pos.y<<" health="<<sneak.player().health<<" guard="<<int(sneak.guardState(assault))<<" gpos="<<sneak.npcActors()[assault].pose.position.x<<','<<sneak.npcActors()[assault].pose.position.y<<" yaw="<<sneak.npcActors()[assault].pose.yaw<<'\n';
  }
  check(routed&&rear&&sneak.player().health==100,"Actual crouched movement around the west screen reaches a rear knockout after the authored distraction");
 }
 for(int stage=0;stage<3;++stage){try{auto scene=exclusionInspection(stage);check(!scene.dead(),"Native encounter preview remains alive");}catch(const std::exception&e){report<<e.what()<<'\n';check(false,"Native encounter preview remains alive");}}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}
