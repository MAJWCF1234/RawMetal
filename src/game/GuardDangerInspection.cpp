#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>

namespace retro {
Game Game::guardDangerInspection(int stage){
 if(stage<0||stage>2)throw std::runtime_error("Unknown guard danger inspection stage");
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");Game g(campaign);
 constexpr size_t engineer=2;
 if(g.chunkCount()!=3||g.npcActors().size()!=8||g.npcActors()[engineer].track().ai.role!=GuardSquadRole::Engineer)
  throw std::runtime_error("Guard danger inspection requires the actual stitched cleanup squad export");

 // Cross the ordinary western entrance and run its real activation trigger.
 // Health, targets, action clocks and finite resources remain gameplay-owned.
 g.m_player.pos={26,20};g.crossChunkBoundary();g.updateStreaming(0);
 for(int frame=0;frame<12;++frame)g.update({},1.f/60);
 if(g.level()!=1||!g.state("cleanup_enabled")||g.dead())throw std::runtime_error("Cleanup entry did not safely activate its authored squad");
 Vec2 observer{1.5f,1.5f};
 if(!g.hullFits(observer,0,Player::StandingHeight))throw std::runtime_error("Danger encounter observer intersects geometry");
 g.m_player.pos=observer;g.m_player.z=0;

 const auto start=g.npcActors()[engineer].pose;
 if(g.npcActors()[engineer].level!=g.level())throw std::runtime_error("Authored Engineer is outside the inspected cargo chunk");
 // A real stationary player-attributed projectile just west of the Engineer
 // permits an exposed reaction without guessing a concealed enemy position.
 Point3 hazard{start.position.x-1.25f,start.position.y,start.z+.15f};
 if(!g.hullFits({hazard.x,hazard.y},start.z,Player::StandingHeight))throw std::runtime_error("Authored Engineer grenade placement intersects geometry");
 if(!g.spawnTacticalGrenade(hazard,{0,0,0},{ShotKind::Player,g.level(),0},0,AiFaction::Player,2400))
  throw std::runtime_error("Danger encounter could not create its physical warning grenade");
 float maximumEscape=0;
 const int frames=stage==0?0:stage==1?36:240;
 for(int frame=0;frame<frames;++frame){
  g.update({},1.f/60);
  if(g.dead())throw std::runtime_error("Danger encounter killed its remote observer");
  const auto&npc=g.npcActors()[engineer];
  if(npc.level!=g.level()||npc.pose.clip==4||g.state(npc.track().deadState))throw std::runtime_error("Authored Engineer did not survive the same-chunk danger encounter");
  maximumEscape=std::max(maximumEscape,length(npc.pose.position-start.position));
 }
 const auto&npc=g.npcActors()[engineer];float displacement=length(npc.pose.position-start.position);
 int danger=g.state(g.friendlyKey(engineer,"guard_danger_ms"));
 if(stage==0&&g.tacticalGrenades().size()!=1)throw std::runtime_error("Initial danger capture lacks its real grenade");
 if(stage==1&&(g.tacticalGrenades().size()!=1||danger<=0||displacement<.25f||npc.pose.clip!=1))
  throw std::runtime_error("Stationary authored Engineer did not physically escape the exposed enemy grenade");
 if(stage==1&&(g.state(g.friendlyKey(engineer,"weapon_aim"))||g.state(g.friendlyKey(engineer,"grenade_windup"))||g.state(g.friendlyKey(engineer,"drone_busy"))))
  throw std::runtime_error("Danger escape retained an incompatible attack or engineering task");
 // The genuine explosion may leave an ordinary sound investigation. Recovery
 // is normal movement toward that heard site or the original patrol, rather
 // than forcing a particular awareness state into the inspected encounter.
 if(stage==2&&(!g.tacticalGrenades().empty()||danger||maximumEscape<.4f||npc.pose.clip!=1||!(displacement+.1f<maximumEscape)))
  throw std::runtime_error("Authored Engineer did not clear danger and resume ordinary movement");

 // The observer moves only after the gameplay milestone. Require an actual
 // hull fit and clear view of the Engineer and the hazard site for each frame.
 Vec2 site{hazard.x,hazard.y},focus=(npc.pose.position+site)*.5f;bool positioned=false;
 auto origin=g.chunkOffset(g.level());
 for(float turn:{0.f,.65f,-.65f,1.3f,-1.3f,kPi}){
  Vec2 camera=focus+Vec2{std::sin(turn),std::cos(turn)}*4.3f;
  if(!g.hullFits(camera,start.z,Player::StandingHeight))continue;
  Point3 eye{camera.x+origin.x,camera.y+origin.y,start.z+Player::StandingEye};
  Point3 body{npc.pose.position.x+origin.x,npc.pose.position.y+origin.y,npc.pose.z+npc.track().scale*.65f};
  Point3 floorSite{site.x+origin.x,site.y+origin.y,start.z+.12f};
  if(!g.friendlyRayClear(eye,body)||!g.friendlyRayClear(eye,floorSite))continue;
  g.m_player.pos=camera;g.m_player.z=start.z;positioned=true;break;
 }
 if(!positioned)throw std::runtime_error("No hull-safe observer can see the real Engineer danger encounter");
 g.m_player.angle=std::atan2(focus.y-g.m_player.pos.y,focus.x-g.m_player.pos.x);
 g.m_player.pitch=std::atan2(start.z+.65f-(g.m_player.z+Player::StandingEye),length(focus-g.m_player.pos))*180.f/kPi;
 g.m_weaponEquipped=false;g.m_holster=1;

 const char*names[]={"danger-ready","danger-escape","danger-clear"};
 std::filesystem::create_directories("diagnostics/guard-danger-20261011");
 std::ofstream report(std::string("diagnostics/guard-danger-20261011/")+names[stage]+"-runtime.txt");
 report<<names[stage]<<": PASS frames="<<frames<<" observerHealth="<<g.player().health
       <<" grenades="<<g.tacticalGrenades().size()<<" engineerHealth="<<npc.track().health-g.state(npc.track().damageState)
       <<" engineer="<<npc.level<<'/'<<npc.pose.position.x<<','<<npc.pose.position.y
       <<" displacement="<<displacement<<" maximumEscape="<<maximumEscape<<" dangerMs="<<danger<<" guardMode="<<int(g.guardState(engineer))
       <<" action="<<g.state(g.friendlyKey(engineer,"weapon_action"))<<" droneBusy="<<g.state(g.friendlyKey(engineer,"drone_busy"))<<'\n';
 return g;
}
}
