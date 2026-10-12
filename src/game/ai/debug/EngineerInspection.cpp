#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
Game Game::engineerSupportInspection(int stage){
 if(stage<0||stage>3)throw std::runtime_error("Unknown Engineer inspection stage");
 auto authored=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");
 auto campaign=std::make_shared<CustomCampaign>(*authored);auto cargo=std::make_shared<AuthoredMapData>(*campaign->maps.at(1));
 // Keep the actual supplied squad, cargo geometry and activation trigger. A
 // durable authored opponent permits fleet growth without resetting health.
 ActorTrack opponent=cargo->actorTracks.front();opponent.visual=ActorVisual::Worker;opponent.tool=false;opponent.ai={};opponent.ai.faction=AiFaction::Facility;
 opponent.health=5000;opponent.damageState=stateId("engineer_inspection_damage");opponent.deadState=stateId("engineer_inspection_dead");
 for(auto&key:opponent.keys){key.position={6,18};key.yaw=kPi;key.clip=0;}cargo->actorTracks.push_back(opponent);campaign->maps[1]=cargo;
 Game g(campaign);constexpr size_t engineer=2;
 g.m_player.pos={26,20};g.crossChunkBoundary();g.updateStreaming(0);for(int frame=0;frame<12;++frame)g.update({},1.f/60);
 if(g.level()!=1||!g.state("cleanup_enabled")||g.npcActors()[engineer].track().ai.role!=GuardSquadRole::Engineer)throw std::runtime_error("Engineer encounter did not enter its real activation trigger");
 g.m_player.pos={1.5f,1.5f};g.m_player.z=0;if(!g.hullFits(g.player().pos,0,Player::StandingHeight))throw std::runtime_error("Engineer observer intersects geometry");
 const auto initial=g.npcActors()[engineer].pose;int frames=0;bool reached=false;
 for(;frames<3600;++frames){g.update({},1.f/60);if(g.dead()||g.npcActors()[engineer].pose.clip==4)throw std::runtime_error("Engineer encounter killed its remote observer or Engineer");
  auto weapon=g.npcWeaponPose(engineer);size_t owned=0;for(const auto&drone:g.engineerDrones())owned+=drone.owner==int(engineer);
  if(stage==0?weapon.engineering>=.45f:owned==EngineerDroneCapacity){reached=true;break;}
 }
 if(!reached)throw std::runtime_error("Engineer did not reach terminal assembly or its five-unit fleet through normal gameplay");
 int repairedSlot=-1,damagedHealth=0;
 if(stage==2){float nearest=10000;const auto&npc=g.npcActors()[engineer];for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer)&&drone.level==npc.level){float distance=length(Vec2{drone.position.x,drone.position.y}-npc.pose.position);if(distance<nearest){nearest=distance;repairedSlot=drone.slot;}}
  if(repairedSlot<0)throw std::runtime_error("Engineer fleet has no physical repair candidate");g.damageEngineerDrone(repairedSlot,18);for(const auto&drone:g.engineerDrones())if(drone.slot==repairedSlot)damagedHealth=int(drone.health);
 }
 if(stage>=2){reached=false;for(int work=0;work<1800;++work){g.update({},1.f/60);++frames;auto weapon=g.npcWeaponPose(engineer);
   if(g.dead()||g.npcActors()[engineer].pose.clip==4)throw std::runtime_error("Engineer support work killed its remote observer or Engineer");
   bool pulse=false;for(const auto&drone:g.engineerDrones())if(drone.slot==repairedSlot)pulse=drone.health>damagedHealth&&drone.health<EngineerDroneHealth;
   if(stage==2?weapon.maintenance&&pulse:weapon.flash>0&&!weapon.engineering&&!weapon.maintenance){reached=true;break;}}
  if(!reached)throw std::runtime_error("Engineer did not physically repair its fleet or resume actual pistol fire");
 }
 const auto&npc=g.npcActors()[engineer];auto weapon=g.npcWeaponPose(engineer);int owned=0;for(const auto&drone:g.engineerDrones())owned+=drone.owner==int(engineer);
 if(!weapon.handgun||(stage>0&&owned!=EngineerDroneCapacity)||length(npc.pose.position-initial.position)<(stage?0.1f:0.f))throw std::runtime_error("Engineer encounter lost its pistol, full fleet or physical support movement");
 // Camera placement follows the completed gameplay milestone. It does not
 // change work clocks, damage, resources, enemies or the Engineer's position.
 Point3 focus{npc.pose.position.x,npc.pose.position.y,npc.pose.z+1.05f};if(stage==1){for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer))focus=focus+drone.position+Point3{0,0,EngineerDroneHeight*.5f};focus=focus*(1.f/(owned+1));}
 bool positioned=false;float cameraFeet=npc.pose.z+(stage==1?1.3f:0.f),cameraYaw=0,cameraPitch=0;
 for(float range:{stage==1?6.f:2.25f,stage==1?7.f:2.25f,stage==1?8.f:2.25f,stage==1?9.f:2.25f}){if(positioned)break;
 for(float turn:{0.f,-.45f,.45f,-.8f,.8f,-1.3f,1.3f,-2.f,2.f,kPi}){Vec2 facing{std::sin(npc.pose.yaw+turn),std::cos(npc.pose.yaw+turn)};auto camera=npc.pose.position+facing*range;
  if(!g.hullFits(camera,cameraFeet,Player::StandingHeight))continue;auto origin=g.chunkOffset(npc.level);
  if(!g.friendlyRayClear({camera.x+origin.x,camera.y+origin.y,cameraFeet+Player::StandingEye},{npc.pose.position.x+origin.x,npc.pose.position.y+origin.y,npc.pose.z+1.05f}))continue;
  cameraYaw=std::atan2(focus.y-camera.y,focus.x-camera.x);cameraPitch=std::atan2(focus.z-(cameraFeet+Player::StandingEye),length(Vec2{focus.x,focus.y}-camera));
  if(stage==1){bool allVisible=true;Point3 eye{camera.x,camera.y,cameraFeet+Player::StandingEye};for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer)){
    auto delta=drone.position+Point3{0,0,EngineerDroneHeight*.5f}-eye;float span=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);auto hit=g.traceShot(eye,delta,span+.05f,-1,false);
    allVisible&=hit.kind==ShotKind::Drone&&hit.index==drone.slot;
   }if(!allVisible)continue;
   // Center the angular bounds rather than the average position: a nearby
   // drone occupies more screen space than one behind its supervisor.
   float low=10,high=-10;for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer))for(float x:{-.6f,.6f})for(float y:{-.6f,.6f}){auto p=drone.position+Point3{x,y,0}-eye;float angle=std::remainder(std::atan2(p.y,p.x)-cameraYaw,2*kPi);low=std::min(low,angle);high=std::max(high,angle);}
   cameraYaw+=(low+high)*.5f;low=10;high=-10;
   for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer))for(float x:{-.6f,.6f})for(float y:{-.6f,.6f})for(float z:{0.f,EngineerDroneHeight}){auto p=drone.position+Point3{x,y,z}-eye;float forward=p.x*std::cos(cameraYaw)+p.y*std::sin(cameraYaw),angle=std::atan2(p.z,forward);low=std::min(low,angle);high=std::max(high,angle);}
   cameraPitch=(low+high)*.5f;
   for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer))for(float x:{-.6f,.6f})for(float y:{-.6f,.6f})for(float z:{0.f,EngineerDroneHeight}){auto p=drone.position+Point3{x,y,z}-eye;float across=-p.x*std::sin(cameraYaw)+p.y*std::cos(cameraYaw),forward=p.x*std::cos(cameraYaw)+p.y*std::sin(cameraYaw),vertical=p.z*std::cos(cameraPitch)-forward*std::sin(cameraPitch),depth=forward*std::cos(cameraPitch)+p.z*std::sin(cameraPitch);allVisible&=depth>0&&std::fabs(across)<depth*.75f&&std::fabs(vertical)<depth*.42f;}
   if(!allVisible)continue;}
  g.m_player.pos=camera;g.m_player.z=cameraFeet;positioned=true;break;}
 }
 if(!positioned)throw std::runtime_error("No hull-safe observer can see the Engineer support milestone");
 g.m_player.angle=cameraYaw;g.m_player.pitch=cameraPitch*140.f;
 g.m_weaponEquipped=false;g.m_holster=1;
 const char*names[]={"terminal-deploy","five-drone-fleet","terminal-maintenance","pistol-support"};std::filesystem::create_directories("diagnostics/engineer-support-20261012");
 std::ofstream report(std::string("diagnostics/engineer-support-20261012/")+names[stage]+"-runtime.txt");
 report<<names[stage]<<": PASS frames="<<frames<<" observerHealth="<<g.player().health<<" engineerHealth="<<npc.track().health-g.state(npc.track().damageState)
  <<" engineerPosition="<<npc.pose.position.x<<','<<npc.pose.position.y<<" supportMovement="<<length(npc.pose.position-initial.position)<<" owned="<<owned
  <<" spent="<<g.state(g.friendlyKey(engineer,"drone_used"))<<" terminal="<<weapon.engineering<<" maintenance="<<weapon.maintenance<<" handgun="<<weapon.handgun<<" pistolFlash="<<weapon.flash
  <<" repairStock="<<g.state(g.friendlyKey(engineer,"drone_repair_charge"))<<" opponentDamage="<<g.state(opponent.damageState)
  <<" schedule="<<g.state(g.friendlyKey(engineer,"schedule"))<<" step="<<g.state(g.friendlyKey(engineer,"schedule_step"))<<" task="<<g.state(g.friendlyKey(engineer,"task"))<<" status="<<g.state(g.friendlyKey(engineer,"task_status"))<<" subject="<<g.state(g.friendlyKey(engineer,"task_subject"))<<'\n';
 for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer))report<<"drone="<<drone.slot<<" position="<<drone.position.x<<','<<drone.position.y<<','<<drone.position.z<<" health="<<drone.health<<'\n';
 return g;
}
}
