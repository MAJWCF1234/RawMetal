#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
namespace retro {
Game Game::guardPolicyInspection(int stage){
 // Exclusion Zone wasteland facility: live cleanup squad on the open yard.
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt");Game g(campaign);
 size_t assault=g.npcActors().size(),engineer=assault;
 for(size_t actor=0;actor<g.npcActors().size();++actor){const auto&npc=g.npcActors()[actor];
  if(npc.homeLevel!=0||npc.track().ai.mode!=ActorAiMode::Guard)continue;
  if(npc.track().ai.role==GuardSquadRole::Assault)assault=actor;
  if(npc.track().ai.role==GuardSquadRole::Engineer)engineer=actor;}
 if(stage<0||stage>3||assault==g.npcActors().size()||engineer==g.npcActors().size())
  throw std::runtime_error("Exclusion Zone policy inspection requires the authored facility Assault and Engineer");
 auto advance=[&](){g.update({},1.f/60);};
 // Observe from the west approach; stay clear of the eastern scrap dens.
 g.m_player.pos={2.5f,4.5f};g.m_player.z=0;
 if(!g.hullFits(g.player().pos,0,Player::StandingHeight))throw std::runtime_error("Exclusion Zone policy entry intersects geometry");
 for(int i=0;i<12;++i)advance();
 if(g.level()!=0)throw std::runtime_error("Policy inspection left the wasteland facility chunk");
 if(stage>=1){bool reached=false;g.m_player.pos={6.5f,10.5f};g.m_player.z=0;
  for(int i=0;i<900;++i){advance();auto learned=g.guardTarget(assault);
   if(learned.kind==ShotKind::Player&&g.guardState(assault)==GuardState::Alert){reached=true;break;}}
  if(!reached)throw std::runtime_error("Facility Assault failed to recognise the player in the wasteland yard");
 }
 if(stage>=2){bool reached=false;for(int i=0;i<1200;++i){advance();
  if((g.npcWeaponPose(assault).flash>0||g.npcWeaponPose(engineer).flash>0)&&g.player().health<100){reached=true;break;}}
  if(!reached)throw std::runtime_error("Facility squad failed to produce damaging fire in the wasteland yard");
 }
 if(stage==3){
  g.setState(g.friendlyKey(assault,"weapon_rounds"),0);g.setState(g.friendlyKey(assault,"weapon_saved"),1);bool protectedReload=false;
  for(int i=0;i<300;++i){advance();const auto&npc=g.npcActors()[assault];auto target=g.guardTarget(assault);
   if(target.kind==ShotKind::Player&&g.state(g.friendlyKey(assault,"weapon_reload"))>0&&g.guardCovered(assault,npc.pose,target)){protectedReload=true;break;}}
  if(!protectedReload)throw std::runtime_error("Facility Assault failed to use cover while reloading");
 }
 Vec2 camera{2.5f,8.5f};if(!g.hullFits(camera,0,Player::StandingHeight))throw std::runtime_error("Policy inspection camera intersects map geometry");
 g.m_player.pos=camera;g.m_player.z=0;g.m_player.angle=std::atan2(14.f-camera.y,11.f-camera.x);g.m_player.pitch=-8;g.m_weaponEquipped=false;g.m_holster=1;g.m_damageFlash=0;
 return g;
}
}
