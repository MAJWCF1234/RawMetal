#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
namespace retro {
Game Game::guardPolicyInspection(int stage){
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");Game g(campaign);
 if(stage<0||stage>3||g.npcActors().size()<2||g.world().actorTracks().size()!=3)throw std::runtime_error("Ash policy inspection requires the actual opt-in squad/creature experiment");
 if(g.state("ash_team_enabled")||g.guardTarget(1).valid())throw std::runtime_error("Ash squad experiment unexpectedly begins enabled");
 auto advance=[&](){g.update({},1.f/60);if(g.player().health!=100||g.state("ash_target_dead"))throw std::runtime_error("Ash squad inspection damaged the hidden player or killed its creature target");};
 // Use the same authored entry trigger as ordinary play. Do not manufacture
 // alerted states, target references, squad leases or initial shots.
 g.m_player.pos={17.2f,12.4f};g.m_player.z=0;
 if(!g.hullFits(g.player().pos,0,Player::StandingHeight))throw std::runtime_error("Ash squad activation entry intersects geometry");
 for(int i=0;i<12;++i)advance();
 if(!g.state("ash_team_enabled"))throw std::runtime_error("Ash authored entry did not enable the squad experiment");
 g.m_player.pos={16.7f,10.1f};g.m_player.z=0;
 if(stage>=1){bool reached=false;for(int i=0;i<600;++i){advance();auto learned=g.guardTarget(0);if(g.guardState(1)==GuardState::Alert&&learned.kind==ShotKind::Actor&&learned.index==2){reached=true;break;}}
  if(!reached)throw std::runtime_error("Ash guard failed to recognise its creature target and share the sighting with its squadmate");
 }
 if(stage>=2){bool reached=false;for(int i=0;i<1200;++i){advance();if(g.state("ash_target_damage")>=36&&(g.npcWeaponPose(0).flash>0||g.npcWeaponPose(1).flash>0)){reached=true;break;}}
  if(!reached)throw std::runtime_error("Ash authored squad failed to produce two real damaging shots with a visible muzzle flash");
 }
 if(stage==3){
  // Interrupt the actual shooter with an empty tube to inspect defensive
  // movement without waiting for enough natural shots to kill the target.
  g.setState(g.friendlyKey(1,"weapon_rounds"),0);g.setState(g.friendlyKey(1,"weapon_saved"),1);bool protectedReload=false;
  for(int i=0;i<300;++i){advance();const auto&npc=g.npcActors()[1];auto target=g.guardTarget(1);if(target.kind==ShotKind::Actor&&target.index==2&&g.state(g.friendlyKey(1,"weapon_reload"))>0&&g.guardCovered(1,npc.pose,target)){protectedReload=true;break;}}
  if(!protectedReload)throw std::runtime_error("Ash guard failed to retreat into physical head/body cover while reloading");
 }
 // Position the observer only after the real simulation reaches its milestone.
 // All stages share this camera so captures can be compared directly.
 Vec2 camera{22.5f,10};if(!g.hullFits(camera,0,Player::StandingHeight))throw std::runtime_error("Ash policy inspection camera intersects map geometry");
 g.m_player.pos=camera;g.m_player.z=0;g.m_player.angle=std::atan2(15.f-camera.y,21.5f-camera.x);g.m_player.pitch=-5;g.m_weaponEquipped=false;g.m_holster=1;g.m_damageFlash=0;
 return g;
}
}
