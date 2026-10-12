#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
Game Game::killSquadInspection(int stage){
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");Game g(campaign);
 if(stage<0||stage>13||g.chunkCount()!=3||g.npcActors().size()!=8)throw std::runtime_error("Kill squad inspection requires the actual stitched Ash export");
 // Enter the real authoring trigger; no manufactured alert/target/weapon state.
 g.m_player.pos={26,20};g.crossChunkBoundary();g.updateStreaming(0);for(int i=0;i<12;++i)g.update({},1.f/60);
 if(g.level()!=1||!g.state("cleanup_enabled"))throw std::runtime_error("Cleanup entry did not enable the authored kill squad");
 g.m_player.pos={6,20};
 if(stage>=11){g.m_player.pos={2,20};g.damageFriendly(5,60,g.player().pos,{});bool reached=false;
  for(int i=0;i<1800&&!g.dead();++i){g.update({},1.f/60);float work=g.npcWeaponPose(6).treatment;if(stage==11?work>=.18f:stage==12?work>=.55f:g.state(g.friendlyKey(6,"support_used"))>0){reached=true;break;}}
  if(!reached){std::ofstream log("diagnostics/kill-squad-20261011/medic-runtime-failure.txt");for(size_t actor:{size_t(5),size_t(6)}){const auto&npc=g.npcActors()[actor];log<<"actor="<<actor<<" level="<<npc.level<<" position="<<npc.pose.position.x<<','<<npc.pose.position.y<<" health="<<npc.track().health-g.state(npc.track().damageState)<<" clip="<<npc.pose.clip<<'\n';for(const char*field:{"support_target","support_ms","support_used","support_cooldown","weapon_recovery","weapon_action","weapon_reload","grenade_windup","guard_mode","stuck","repath","way_x","way_y"})log<<field<<'='<<g.state(g.friendlyKey(actor,field))<<'\n';}throw std::runtime_error("Authored Medic did not physically open its kit and treat the injured squadmate");}
 }
 if(stage==1){bool reached=false;for(int i=0;i<600&&!g.dead();++i){g.update({},1.f/60);if(!g.tacticalGrenades().empty()){reached=true;break;}}if(!reached)throw std::runtime_error("Authored grenadier did not release a physical projectile");}
 if(stage==2){bool reached=false;for(int i=0;i<600&&!g.dead();++i){g.update({},1.f/60);for(size_t actor=2;actor<8;++actor)if(g.npcWeaponPose(actor).flash>0)reached=true;if(reached)break;}if(!reached||g.player().health>=100)throw std::runtime_error("Authored kill squad did not produce a real damaging shot");}
 if(stage==4){bool reached=false;for(int i=0;i<600&&!g.dead();++i){g.update({},1.f/60);if(g.npcWeaponPose(2).grenadeThrow>.7f&&g.tacticalGrenades().empty()){reached=true;break;}}if(!reached)throw std::runtime_error("Authored engineer did not telegraph its physical grenade throw");}
 if(stage==3){
  for(int i=0;i<60&&!g.dead();++i)g.update({},1.f/60);
  g.m_player.pos={18.5f,25.5f};g.crossChunkBoundary();g.updateStreaming(0);for(int i=0;i<60&&!g.dead();++i)g.update({},1.f/60);
  g.m_player.pos={18.5f,17.5f};bool crossed=false;for(int i=0;i<900&&!g.dead();++i){g.update({},1.f/60);for(size_t actor=2;actor<8;++actor)crossed|=g.npcActors()[actor].level==2;if(crossed)break;}
  if(!crossed)throw std::runtime_error("Kill squad did not pursue its observed threat through the south seam");
 }
 // Observer is positioned only after normal simulation reaches the milestone.
 if(stage==10){bool reached=false;for(int i=0;i<900&&!g.dead();++i){g.update({},1.f/60);if(!g.engineerDrones().empty()){reached=true;break;}}if(!reached)throw std::runtime_error("Authored Engineer did not deploy its supervised drone");}
 if(stage==10){auto p=g.engineerDrones()[0].position;g.m_player.pos={p.x+2,p.y+1};g.m_player.angle=std::atan2(p.y-g.m_player.pos.y,p.x-g.m_player.pos.x);}
 else if(stage>=4){const auto&pose=g.npcActors()[stage>=11?6:stage-2].pose;bool positioned=false;
  for(float offset:{0.f,.8f,-.8f,1.6f,-1.6f,kPi}){float turn=offset+(stage>=11?1.6f:0.f);auto forward=Vec2{std::sin(pose.yaw+turn),std::cos(pose.yaw+turn)};auto camera=pose.position+forward*2.1f+Vec2{forward.y,-forward.x}*.45f;if(g.hullFits(camera,pose.z,Player::StandingHeight)){g.m_player.pos=camera;positioned=true;break;}}
  if(!positioned)throw std::runtime_error("No clear observer position around cleanup role");g.m_player.angle=std::atan2(pose.position.y-g.m_player.pos.y,pose.position.x-g.m_player.pos.x);}
 else if(stage==3){g.m_player.pos={21.5f,5.5f};g.m_player.angle=std::atan2(1.f-5.5f,18.5f-21.5f);}
 else{g.m_player.pos={6,18};g.m_player.angle=std::atan2(12.5f-18,8.5f-6);}
 if(!g.hullFits(g.player().pos,g.player().z,Player::StandingHeight))throw std::runtime_error("Cleanup inspection observer intersects geometry");
 g.m_player.pitch=stage>=4?-20.f:-8.f;g.m_damageFlash=0;g.m_weaponEquipped=false;g.m_holster=1;return g;
}
}
