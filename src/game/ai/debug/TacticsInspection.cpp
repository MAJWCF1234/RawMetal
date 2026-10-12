#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
namespace retro {
Game Game::guardTacticsInspection(int stage){
 if(stage<0||stage>2)throw std::runtime_error("Unknown guard tactics inspection stage");
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");
 if(campaign->maps.empty())throw std::runtime_error("Ash custom playground has no map");
 const auto&map=*campaign->maps[0];
 bool cover=false;for(const auto&box:map.structures)if(std::fabs(box.x1-20.6f)<.001f&&std::fabs(box.x2-20.8f)<.001f&&std::fabs(box.y1-7.1f)<.001f&&std::fabs(box.y2-7.9f)<.001f&&std::fabs(box.bottom)<.001f&&std::fabs(box.top-1.3f)<.001f)cover=true;
 if(!cover)throw std::runtime_error("Actual exported custom playground lacks the low cover geometry");
 Game g(campaign);if(g.npcActors().empty()||!g.npcActors()[0].track().ai.armed)throw std::runtime_error("Custom playground has no authored armed guard");
 const auto start=g.npcActors()[0].pose;
 g.m_player.pos=start.position+Vec2{2.3f,0};g.m_player.z=start.z;
 if(!g.hullFits(g.m_player.pos,g.m_player.z,Player::StandingHeight))throw std::runtime_error("Tactics encounter player intersects map geometry");
 Point3 target{g.m_player.pos.x,g.m_player.pos.y,g.m_player.z+g.m_player.hullHeight()*.6f};
 auto raised=g.npcWeaponPoseAt(0,start,target,1);
 if(g.guardShot(0,start,raised).kind!=ShotKind::Solid)throw std::runtime_error("Initial fully raised guard shot is not blocked by cover");
 if(!g.soundRayClear(start.position,start.z+g.npcActors()[0].track().scale*.85f,g.m_player.pos,g.m_player.z+g.m_player.eye))throw std::runtime_error("Low cover incorrectly hides the player's head");
 if(stage>0){
  bool reached=false;
  for(int frame=0;frame<600;++frame){
   g.update({},1.f/60);
   float moved=length(g.npcActors()[0].pose.position-start.position);
   if(stage==1&&moved>=.4f&&g.m_player.health==100){reached=true;break;}
   if(stage==2&&g.m_player.health==82&&g.state(g.friendlyKey(0,"weapon_flash"))>0){if(moved<.4f)throw std::runtime_error("Guard fired without first leaving the blocked firing position");reached=true;break;}
   if(g.m_player.health<100&&(stage==1||g.m_player.health!=82))throw std::runtime_error("Tactics encounter damaged the player before the expected capture stage");
  }
  if(!reached)throw std::runtime_error(stage==1?"Guard did not sidestep cover during normal gameplay updates":"Guard did not acquire a clear shot after repositioning");
 }
 // The camera is an observation step after actual gameplay, so it cannot
 // alter recognition, candidate selection, aim timing or shot damage.
 Vec2 camera{22,10.5f};if(!g.hullFits(camera,start.z,Player::StandingHeight))throw std::runtime_error("Tactics inspection camera intersects geometry");
 g.m_player.pos=camera;g.m_player.z=start.z;g.m_player.angle=std::atan2(start.position.y-camera.y,start.position.x-camera.x);g.m_player.pitch=-18;
 g.m_weaponEquipped=false;g.m_holster=1;g.m_damageFlash=0;return g;
}
}
