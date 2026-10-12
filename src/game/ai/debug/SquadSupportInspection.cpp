#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
#include <array>
#include <fstream>
#include <limits>
namespace retro {
Game Game::squadSupportInspection(int stage){
 if(stage<0||stage>2)throw std::runtime_error("Unknown squad support inspection stage");
 auto authored=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");
 auto campaign=std::make_shared<CustomCampaign>(*authored);auto cargo=std::make_shared<AuthoredMapData>(*campaign->maps.at(1));
 // Keep the supplied six roles, their starting poses and the real cargo
 // geometry/activation trigger. A durable scripted Facility opponent gives
 // the ordinary Rusher a continuing threat without resetting its health.
 const Vec2 threatPosition{20.5f,21.5f};const size_t opponentIndex=cargo->actorTracks.size();
 ActorTrack opponent=cargo->actorTracks.front();opponent.visual=ActorVisual::Worker;opponent.tool=false;opponent.ai={};opponent.ai.faction=AiFaction::Facility;
 opponent.health=5000;opponent.damageState=stateId("squad_support_opponent_damage");opponent.deadState=stateId("squad_support_opponent_dead");
 for(auto&key:opponent.keys){key.position=threatPosition;key.yaw=kPi;key.clip=0;key.lookAtActor=-1;}
 cargo->actorTracks.push_back(opponent);campaign->maps[1]=cargo;Game g(campaign);
 size_t patient=std::numeric_limits<size_t>::max(),medic=patient;
 for(size_t actor=0;actor<g.npcActors().size();++actor){const auto&npc=g.npcActors()[actor];if(npc.homeLevel!=1)continue;
  if(npc.track().ai.role==GuardSquadRole::Rusher)patient=actor;if(npc.track().ai.role==GuardSquadRole::Medic)medic=actor;}
 if(patient==std::numeric_limits<size_t>::max()||medic==std::numeric_limits<size_t>::max()||g.npcActors()[patient].track().health!=90)
  throw std::runtime_error("Squad support inspection requires the actual ninety-health cargo Rusher and Medic");
 g.m_player.pos={26,20};g.crossChunkBoundary();g.updateStreaming(0);
 for(int frame=0;frame<12;++frame)g.update({},1.f/60);
 if(g.level()!=1||!g.state("cleanup_enabled"))throw std::runtime_error("Squad support did not enter its real cleanup activation trigger");
 g.m_player.pos={1.5f,1.5f};g.m_player.z=0;
 if(!g.hullFits(g.player().pos,0,Player::StandingHeight))throw std::runtime_error("Squad support remote observer intersects geometry");
 const char*names[]={"patient-withdraw","medic-kit","patient-resume"};
 std::filesystem::create_directories("diagnostics/squad-support-20261012");
 std::ofstream timeline(std::string("diagnostics/squad-support-20261012/")+names[stage]+"-timeline.csv");
 timeline<<"frame,actor,x,y,damage,schedule,step,task,status,subject,underFire,patientMove,treatment,charges,failures,reason\n";
 auto get=[&](size_t actor,const char*field){return g.state(g.friendlyKey(actor,field));};
 int frames=12;bool budgetSafe=true;
 auto record=[&](){for(size_t actor:{patient,medic}){const auto&npc=g.npcActors()[actor];timeline<<frames<<','<<actor<<','<<npc.pose.position.x<<','<<npc.pose.position.y<<','<<g.state(npc.track().damageState);
   for(const char*field:{"schedule","schedule_step","task","task_status","task_subject","guard_under_fire","guard_patient_move","support_ms","support_used","task_failures","task_reason"})timeline<<','<<get(actor,field);timeline<<'\n';}timeline.flush();};
 auto advance=[&](){g.update({},1.f/60);++frames;budgetSafe&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;
  if(g.dead()||g.npcActors()[patient].pose.clip==4||g.npcActors()[medic].pose.clip==4)throw std::runtime_error("Squad support killed its remote observer, patient or Medic");
  if(g.npcActors()[patient].level!=1||g.npcActors()[medic].level!=1)throw std::runtime_error("Squad support pair left the actual cargo encounter");
  if(frames%10==0)record();};
 auto pairDistance=[&](){return length(g.npcActors()[patient].pose.position-g.npcActors()[medic].pose.position);};
 const auto originalPatient=g.npcActors()[patient].pose.position;bool rushing=false;
 for(int attempt=0;attempt<600;++attempt){advance();
  if(g.npcActors()[patient].pose.clip==1&&get(patient,"guard_seen")&&get(patient,"guard_confirmed")&&length(g.npcActors()[patient].pose.position-originalPatient)>.15f){rushing=true;break;}}
 if(!rushing){record();throw std::runtime_error("Authored Rusher did not begin genuine confirmed-threat movement before injury");}
 const auto woundedAt=g.npcActors()[patient].pose.position;
 g.damageFriendly(patient,45,threatPosition,{ShotKind::Actor,1,int(opponentIndex)});record();
 if(g.state(g.npcActors()[patient].track().damageState)!=45)throw std::runtime_error("Actual injury did not place the Rusher at forty-five of ninety health");
 int woundFrame=frames,withdrawFrame=-1,kitFrame=-1,healFrame=-1,resumeFrame=-1;float withdrawal=0;Vec2 healedAt{};bool reached=false;
 for(int attempt=0;attempt<2400;++attempt){advance();const auto&receiver=g.npcActors()[patient];
  bool paired=get(patient,"schedule")==int(GuardSchedule::Patient)&&get(patient,"task_subject")==int(medic)+1&&get(medic,"support_target")==int(patient)+1;
  auto away=normalized(woundedAt-threatPosition);float rearMovement=dot(receiver.pose.position-woundedAt,away);withdrawal=std::max(withdrawal,rearMovement);
  bool withdrawing=paired&&!get(patient,"guard_under_fire")&&get(patient,"guard_patient_move")&&receiver.pose.clip==1&&rearMovement>.25f;
  if(withdrawing&&withdrawFrame<0){withdrawFrame=frames;record();}
  bool kit=paired&&receiver.pose.clip==0&&pairDistance()<=1.31f&&get(medic,"support_ms")>=2100&&get(medic,"support_used")==0&&g.guardTaskAllows(medic,GuardTask::Treat);
  if(kit&&kitFrame<0){kitFrame=frames;record();}
  bool healed=g.state(receiver.track().damageState)==15&&get(medic,"support_used")==1;
  if(healed&&healFrame<0){healFrame=frames;healedAt=receiver.pose.position;record();}
  bool resumed=healFrame>=0&&receiver.pose.clip==1&&get(patient,"schedule")!=int(GuardSchedule::Patient)&&!get(patient,"guard_patient_move")&&
   receiver.track().ai.role==GuardSquadRole::Rusher&&dot(receiver.pose.position-healedAt,normalized(threatPosition-healedAt))>.25f;
  if(resumed&&resumeFrame<0){resumeFrame=frames;record();}
  reached=stage==0?withdrawFrame>=0:stage==1?withdrawFrame>=0&&kit:withdrawFrame>=0&&kitFrame>=0&&healed&&resumed;
  if(reached)break;
 }
 record();
 if(!reached||!budgetSafe){std::ofstream failure(std::string("diagnostics/squad-support-20261012/")+names[stage]+"-failure.txt");
  failure<<"frames="<<frames<<" wound="<<woundFrame<<" withdraw="<<withdrawFrame<<" kit="<<kitFrame<<" heal="<<healFrame<<" resume="<<resumeFrame<<" maximumRearMovement="<<withdrawal<<" pairDistance="<<pairDistance()<<" navigationBudgetSafe="<<budgetSafe<<'\n';
  throw std::runtime_error("Actual cargo squad did not reach reciprocal withdrawal, physical kit treatment or resumed Rusher advance; inspect squad support timeline");}
 // Capture placement happens after the observed gameplay milestone. Nothing
 // alters either actor's job, movement, resources or damage for the image.
 const auto&receiver=g.npcActors()[patient];const auto&provider=g.npcActors()[medic];
 Vec2 focus=(receiver.pose.position+provider.pose.position)*.5f;float focusZ=(receiver.pose.z+provider.pose.z)*.5f+.85f;
 auto pairAxis=provider.pose.position-receiver.pose.position;float side=std::atan2(pairAxis.y,pairAxis.x)+kPi*.5f;bool positioned=false;
 float cameraFeet=std::min(receiver.pose.z,provider.pose.z);
 const auto ranges=stage==0?std::array<float,4>{7.f,9.f,11.f,13.f}:std::array<float,4>{3.25f,4.f,5.f,6.f};
 for(float range:ranges){if(positioned)break;
  for(float turn:{0.f,kPi,.45f,-.45f,kPi+.45f,kPi-.45f,1.3f,-1.3f}){auto camera=focus+Vec2{std::cos(side+turn),std::sin(side+turn)}*range;
   if(!g.hullFits(camera,cameraFeet,Player::StandingHeight))continue;
   float yaw=std::atan2(focus.y-camera.y,focus.x-camera.x),pitch=std::atan2(focusZ-(cameraFeet+Player::StandingEye),length(focus-camera));auto origin=g.chunkOffset(1);
   Point3 eye{camera.x,camera.y,cameraFeet+Player::StandingEye};bool visible=true;
   for(size_t actor:{patient,medic}){const auto&npc=g.npcActors()[actor];
    visible&=g.friendlyRayClear(eye+Point3{origin.x,origin.y,0},{npc.pose.position.x+origin.x,npc.pose.position.y+origin.y,npc.pose.z+npc.track().scale*.65f});
    for(float x:{-.45f,.45f})for(float y:{-.45f,.45f})for(float z:{0.f,npc.track().scale}){auto p=Point3{npc.pose.position.x+x,npc.pose.position.y+y,npc.pose.z+z}-eye;
     float across=-p.x*std::sin(yaw)+p.y*std::cos(yaw),forward=p.x*std::cos(yaw)+p.y*std::sin(yaw),vertical=p.z*std::cos(pitch)-forward*std::sin(pitch),depth=forward*std::cos(pitch)+p.z*std::sin(pitch);
     visible&=depth>0&&std::fabs(across)<depth*.70f&&std::fabs(vertical)<depth*.38f;}}
   if(!visible)continue;g.m_player.pos=camera;g.m_player.z=cameraFeet;g.m_player.angle=yaw;g.m_player.pitch=pitch*140.f;positioned=true;break;}
 }
 if(!positioned)throw std::runtime_error("No hull-safe camera can see both actual squad support actors");
 g.m_weaponEquipped=false;g.m_holster=1;g.m_damageFlash=0;
 std::ofstream report(std::string("diagnostics/squad-support-20261012/")+names[stage]+"-runtime.txt");
 report<<names[stage]<<": PASS frames="<<frames<<" woundFrame="<<woundFrame<<" withdrawFrame="<<withdrawFrame<<" kitFrame="<<kitFrame<<" healFrame="<<healFrame<<" resumeFrame="<<resumeFrame
  <<" maximumRearMovement="<<withdrawal<<" pairDistance="<<pairDistance()<<" observerHealth="<<g.player().health<<" opponentDamage="<<g.state(opponent.damageState)<<" navigationBudgetSafe="<<budgetSafe<<'\n';
 for(size_t actor:{patient,medic}){const auto&npc=g.npcActors()[actor];auto weapon=g.npcWeaponPose(actor);report<<"actor="<<actor<<" role="<<int(npc.track().ai.role)<<" position="<<npc.pose.position.x<<','<<npc.pose.position.y<<','<<npc.pose.z
   <<" health="<<npc.track().health-g.state(npc.track().damageState)<<" walking="<<(npc.pose.clip==1)<<" handgun="<<weapon.handgun<<" treatment="<<weapon.treatment;
  for(const char*field:{"schedule","schedule_step","task","task_status","task_subject","guard_under_fire","guard_patient_move","support_target","support_ms","support_used","task_failures","task_reason"})report<<' '<<field<<'='<<get(actor,field);report<<'\n';}
 return g;
}
}
