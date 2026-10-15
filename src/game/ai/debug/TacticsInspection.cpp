#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
namespace retro {
Game Game::guardTacticsInspection(int stage){
 if(stage<0||stage>2)throw std::runtime_error("Unknown guard tactics inspection stage");
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt");
 if(campaign->maps.empty())throw std::runtime_error("Ash custom playground has no map");
 const auto&map=*campaign->maps[0];
 bool cover=false;for(const auto&box:map.structures)if(std::fabs(box.x1-3.2f)<.001f&&std::fabs(box.x2-3.4f)<.001f&&std::fabs(box.y1-8.5f)<.001f&&std::fabs(box.y2-11.5f)<.001f&&std::fabs(box.bottom)<.001f&&std::fabs(box.top-1.55f)<.001f)cover=true;
 if(!cover)throw std::runtime_error("Exclusion Zone wasteland export lacks yard cover geometry");
 Game g(campaign);size_t assault=g.npcActors().size();
 for(size_t actor=0;actor<g.npcActors().size();++actor)if(g.npcActors()[actor].homeLevel==0&&g.npcActors()[actor].track().ai.role==GuardSquadRole::Assault){assault=actor;break;}
 if(assault==g.npcActors().size()||!g.npcActors()[assault].track().ai.armed)throw std::runtime_error("Exclusion Zone has no authored armed Assault");
 const auto start=g.npcActors()[assault].pose;
 // Stand west of the yard scrap wall so the Assault's raised shot is blocked.
 g.m_player.pos={2.0f,10.0f};g.m_player.z=start.z;
 if(!g.hullFits(g.m_player.pos,g.m_player.z,Player::StandingHeight))throw std::runtime_error("Tactics encounter player intersects map geometry");
 Point3 target{g.m_player.pos.x,g.m_player.pos.y,g.m_player.z+g.m_player.hullHeight()*.6f};
 auto raised=g.npcWeaponPoseAt(assault,start,target,1);
 if(g.guardShot(assault,start,raised).kind!=ShotKind::Solid)throw std::runtime_error("Initial fully raised guard shot is not blocked by cover");
 if(!g.soundRayClear(start.position,start.z+g.npcActors()[assault].track().scale*.85f,g.m_player.pos,g.m_player.z+g.m_player.eye))throw std::runtime_error("Low cover incorrectly hides the player's head");
 if(stage>0){
  bool reached=false;
  for(int frame=0;frame<600;++frame){
   g.update({},1.f/60);
   float moved=length(g.npcActors()[assault].pose.position-start.position);
   if(stage==1&&moved>=.4f&&g.m_player.health==100){reached=true;break;}
   if(stage==2&&g.m_player.health==82&&g.state(g.friendlyKey(assault,"weapon_flash"))>0){if(moved<.4f)throw std::runtime_error("Guard fired without first leaving the blocked firing position");reached=true;break;}
   if(g.m_player.health<100&&(stage==1||g.m_player.health!=82))throw std::runtime_error("Tactics encounter damaged the player before the expected capture stage");
  }
  if(!reached)throw std::runtime_error(stage==1?"Guard did not sidestep cover during normal gameplay updates":"Guard did not acquire a clear shot after repositioning");
 }
 Vec2 camera{2.5f,14.5f};if(!g.hullFits(camera,start.z,Player::StandingHeight))throw std::runtime_error("Tactics inspection camera intersects geometry");
 g.m_player.pos=camera;g.m_player.z=start.z;g.m_player.angle=std::atan2(start.position.y-camera.y,start.position.x-camera.x);g.m_player.pitch=-18;
 g.m_weaponEquipped=false;g.m_holster=1;g.m_damageFlash=0;return g;
}
}
