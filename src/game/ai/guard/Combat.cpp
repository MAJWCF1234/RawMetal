#include "../../../core/FrameProfile.h"
#include "../../Game.h"
#include "../../../core/PackedResource.h"
#include "../../../core/ActorBodyAim.h"
#include "../../../core/CleanupBodies.h"
#include "Schedule.h"
namespace retro {
namespace {
Point3 unit(Point3 p){float length=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);return length>.0001f?p*(1/length):Point3{};}
struct CleanupBodyFrame {Point3 pelvis{},spine{},rightShoulder{},leftShoulder{};};
struct CleanupBodyFrames {unsigned clips=0;std::vector<unsigned> counts,offsets;std::vector<CleanupBodyFrame> poses;std::vector<float> durations;std::vector<std::pair<Point3,Point3>> fallBounds;};
const std::array<CleanupBodyFrames,kCleanupBodies>& cleanupBodyFrames(){
 // Share the baked body frame with physical weapon placement. A fixed root
 // offset cannot seat a stock on a leaning or deeply kneeling shoulder.
 static const auto frames=[](){std::array<CleanupBodyFrames,kCleanupBodies> result;
  for(int role=0;role<kCleanupBodies;++role){auto data=loadResource(cleanupBodyResources(role).animation);uint32_t header[4]{};
   bool compact=data.size()>=20&&!std::memcmp(data.data(),"RMA3",4);
   if(data.size()<20||(!compact&&std::memcmp(data.data(),"RMA2",4)))throw std::runtime_error("Missing cleanup stance animation");std::memcpy(header,data.data()+4,16);
   auto [corners,samples,clips,vertices]=std::array<uint32_t,4>{header[0],header[1],header[2],header[3]};size_t frameStart=20+size_t(corners)*2+(compact?size_t(clips)*4:0);
   if((!compact&&(samples<2||samples>512))||(compact&&(!samples||samples>32*512))||clips<6||clips>32||!vertices||vertices>65536||corners>1000000||frameStart>data.size())throw std::runtime_error("Invalid cleanup stance animation");
   auto&table=result[role];table.clips=clips;table.counts.assign(clips,samples);table.offsets.resize(clips);
   if(compact)std::memcpy(table.counts.data(),data.data()+20+size_t(corners)*2,size_t(clips)*4);
   size_t total=0;for(unsigned clip=0;clip<clips;++clip){unsigned count=table.counts[clip];if(count<2||count>512)throw std::runtime_error("Invalid cleanup stance clip count");table.offsets[clip]=unsigned(total);total+=count;}
   if(compact&&total!=samples)throw std::runtime_error("Invalid cleanup stance total count");
   size_t payload=frameStart+total*vertices*6;uint32_t trailer[4]{};
   if(payload+20>data.size()||std::memcmp(data.data()+payload,"RML1",4))throw std::runtime_error("Invalid cleanup stance layers");std::memcpy(trailer,data.data()+payload+4,16);
   size_t layerEnd=payload+20+total*120;
   if(trailer[0]!=(compact?2u:1u)||trailer[1]!=samples||trailer[2]!=clips||trailer[3]!=120||data.size()<layerEnd)throw std::runtime_error("Invalid cleanup stance layer frames");
   table.poses.resize(total);
   // The existing falling clip moves the torso away from the standing root.
   // Retain its small per-sample bounds so shots hit the visible unconscious
   // body, including during the fall, rather than an empty ankle-sized box.
   table.fallBounds.resize(table.counts[4]);
   for(unsigned frame=0;frame<table.counts[4];++frame){Point3 low{1000,1000,1000},high{-1000,-1000,-1000};size_t base=frameStart+size_t(table.offsets[4]+frame)*vertices*6;
    for(unsigned vertex=0;vertex<vertices;++vertex){int16_t xyz[3];std::memcpy(xyz,data.data()+base+size_t(vertex)*6,6);Point3 p{xyz[0]*.001f,xyz[1]*.001f,xyz[2]*.001f};low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
    table.fallBounds[frame]={low,high};
   }
   if(data.size()!=layerEnd){uint32_t count=0;if(data.size()!=layerEnd+8+size_t(clips)*4||std::memcmp(data.data()+layerEnd,"RMD1",4))throw std::runtime_error("Invalid cleanup animation durations");std::memcpy(&count,data.data()+layerEnd+4,4);if(count!=clips)throw std::runtime_error("Invalid cleanup animation duration count");table.durations.resize(clips);std::memcpy(table.durations.data(),data.data()+layerEnd+8,size_t(clips)*4);for(float duration:table.durations)if(!std::isfinite(duration)||duration<=0||duration>120)throw std::runtime_error("Invalid cleanup animation duration");}
   for(size_t frame=0;frame<table.poses.size();++frame){size_t base=payload+20+frame*120;auto&pose=table.poses[frame];std::memcpy(&pose.pelvis,data.data()+base,12);std::memcpy(&pose.spine,data.data()+base+12,12);std::memcpy(&pose.rightShoulder,data.data()+base+24,12);std::memcpy(&pose.leftShoulder,data.data()+base+72,12);for(auto point:{pose.pelvis,pose.spine,pose.rightShoulder,pose.leftShoulder})if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))throw std::runtime_error("Invalid cleanup body frame");}
  }return result;
 }();return frames;
}
CleanupBodyFrame cleanupBodyFrame(int role,int clip,float phase){
 if(!std::isfinite(phase))throw std::runtime_error("Invalid cleanup stance phase");const auto&table=cleanupBodyFrames().at(size_t(role));if(clip<0||unsigned(clip)>=table.clips)throw std::runtime_error("Invalid cleanup stance clip");unsigned count=table.counts[size_t(clip)],offset=table.offsets[size_t(clip)];float frame=std::clamp(phase,0.f,1.f)*float(count-1);unsigned a=unsigned(frame),b=std::min(a+1,count-1);const auto&A=table.poses[offset+a];const auto&B=table.poses[offset+b];float blend=frame-a;return {A.pelvis+(B.pelvis-A.pelvis)*blend,A.spine+(B.spine-A.spine)*blend,A.rightShoulder+(B.rightShoulder-A.rightShoulder)*blend,A.leftShoulder+(B.leftShoulder-A.leftShoulder)*blend};
}
}
float cleanupAnimationDuration(ActorVisual visual,int clip){
 int role=cleanupActorBody(visual);if(role<0)return 0;const auto&table=cleanupBodyFrames()[size_t(role)];return clip>=0&&size_t(clip)<table.durations.size()?table.durations[size_t(clip)]:0;
}
// The fall clip read by the height of its shoulders: the first phase at which
// they have come down to `lift` of the way from the floor pose to standing.
// Other visuals (no baked frames) take the phase as the lift.
float Game::fallPhaseAtLift(ActorVisual visual,float lift){
 lift=std::clamp(lift,0.f,1.f);const int role=cleanupActorBody(visual);if(role<0)return 1-lift;
 const auto&table=cleanupBodyFrames()[size_t(role)];const unsigned count=table.counts[4],offset=table.offsets[4];
 auto height=[&](unsigned frame){const auto&p=table.poses[offset+frame];return (p.rightShoulder.y+p.leftShoulder.y)*.5f;};
 float low=height(0),high=height(0);for(unsigned frame=1;frame<count;++frame)low=std::min(low,height(frame));
 const float want=low+(high-low)*lift;
 for(unsigned frame=1;frame<count;++frame){const float a=height(frame-1),b=height(frame);
  if(b<=want){const float k=a>b?std::clamp((a-want)/(a-b),0.f,1.f):1.f;return (float(frame-1)+k)/float(count-1);}}
 return 1;
}
// Where a hand takes a man by the collar: midway between his shoulders, at
// their top and a little behind them, in his present pose.
Point3 Game::npcCollar(size_t actor)const{
 const auto&npc=m_friendlyActors.at(actor);const int role=cleanupActorBody(npc.track().visual);const float c=std::cos(npc.pose.yaw),s=std::sin(npc.pose.yaw),scale=npc.track().scale/1.6f;
 Point3 p{0,npc.track().scale*.85f,-.06f};
 if(role>=0){const auto frame=cleanupBodyFrame(role,std::clamp(npc.pose.clip,0,int(cleanupBodyFrames()[size_t(role)].clips)-1),npc.pose.phase);p=(frame.rightShoulder+frame.leftShoulder)*.5f;p.y+=.05f;p.z-=.06f;}
 return {npc.pose.position.x+(p.x*c+p.z*s)*scale,npc.pose.position.y+(-p.x*s+p.z*c)*scale,npc.pose.z+p.y*scale};
}
std::pair<Point3,Point3> Game::friendlyShotBounds(size_t actor)const{
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();Point3 center{npc.pose.position.x,npc.pose.position.y,npc.pose.z};
 int role=cleanupActorBody(track.visual);
 if(!npcIncapacitated(actor)||npcHeld(actor)||role<0)return {center-Point3{.2f,.2f,0},center+Point3{.2f,.2f,friendlyHullHeight(actor,track)}};
 const auto&bounds=cleanupBodyFrames()[size_t(role)].fallBounds;float frame=std::clamp(npc.pose.phase,0.f,1.f)*(bounds.size()-1);size_t a=size_t(frame),b=std::min(a+1,bounds.size()-1);
 auto [lo,hi]=bounds[a];const auto&next=bounds[b];lo={std::min(lo.x,next.first.x),std::min(lo.y,next.first.y),std::min(lo.z,next.first.z)};hi={std::max(hi.x,next.second.x),std::max(hi.y,next.second.y),std::max(hi.z,next.second.z)};
 float c=std::cos(npc.pose.yaw),s=std::sin(npc.pose.yaw),scale=track.scale/1.6f;Point3 low{1000,1000,1000},high{-1000,-1000,-1000};
 for(float x:{lo.x,hi.x})for(float y:{lo.y,hi.y})for(float z:{lo.z,hi.z}){auto p=center+Point3{(x*c+z*s)*scale,(-x*s+z*c)*scale,y*scale};low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
 return {low-Point3{.02f,.02f,.02f},high+Point3{.02f,.02f,.02f}};
}
std::array<Point3,2> Game::heldNpcGrips()const{
 int actor=heldNpc();if(actor<0)return {};const auto&npc=m_friendlyActors[size_t(actor)];int role=cleanupActorBody(npc.track().visual);
 Point3 right{-.18f,1.24f,0},left{.18f,1.24f,0};if(role>=0){auto frame=cleanupBodyFrame(role,0,0);right=frame.rightShoulder;left=frame.leftShoulder;}
 float c=std::cos(npc.pose.yaw),s=std::sin(npc.pose.yaw),scale=npc.track().scale/1.6f;
 auto grip=[&](Point3 p){p.y+=.05f;p.z-=.07f;return Point3{npc.pose.position.x+(p.x*c+p.z*s)*scale,npc.pose.position.y+(-p.x*s+p.z*c)*scale,npc.pose.z+p.y*scale};};
 return {grip(right),grip(left)};
}
std::array<Point3,2> Game::takedownGrips()const{
 if(m_takedownActor<0||size_t(m_takedownActor)>=m_friendlyActors.size())return {};
 const auto&npc=m_friendlyActors[size_t(m_takedownActor)];int role=cleanupActorBody(npc.track().visual);
 // Wrap the neck/upper shoulders from behind — not a jab aimed at the ankles.
 Point3 right{-.12f,1.48f,-.04f},left{.12f,1.48f,-.04f};
 if(role>=0){auto frame=cleanupBodyFrame(role,0,0);right=frame.rightShoulder;left=frame.leftShoulder;right.y=left.y=std::max(right.y,left.y)+.22f;right.z=left.z=-.05f;right.x=-.11f;left.x=.11f;}
 float c=std::cos(npc.pose.yaw),s=std::sin(npc.pose.yaw),scale=npc.track().scale/1.6f;
 auto grip=[&](Point3 p){return Point3{npc.pose.position.x+(p.x*c+p.z*s)*scale,npc.pose.position.y+(-p.x*s+p.z*c)*scale,npc.pose.z+p.y*scale};};
 return {grip(right),grip(left)};
}
NpcWeaponPose Game::npcWeaponPoseAt(size_t actor,const ActorPose& pose,Point3 target,float raise)const{
 const auto&track=m_friendlyActors.at(actor).track();NpcWeaponPose weapon;
 if(npcIncapacitated(actor)||track.ai.mode!=ActorAiMode::Guard||!track.ai.armed)return weapon;
 weapon.armed=true;weapon.handgun=guardHandgun(track.ai);weapon.raise=std::clamp(raise,0.f,1.f);
 float scale=track.scale/1.6f;Point3 forward{std::sin(pose.yaw),std::cos(pose.yaw),0},right{forward.y,-forward.x,0};
 // The cleanup shotgun sits close enough for the retained short forearms to
 // support its receiver. Rendering and shot clearance share this same grip.
 float gripForward=civilianActorVisual(track.visual)?.40f:weapon.handgun?.31f:.23f;
 float bodyHeight=pose.clip==5?npcCrouchedHeight(track):friendlyHullHeight(actor,track),crouchDrop=std::max(0.f,track.scale-bodyHeight);
 weapon.crouched=bodyHeight<track.scale-.01f;
 const bool cleanup=cleanupActorVisual(track.visual);
 CleanupBodyFrame upperBody{},lowerBody{};bool layerAim=pose.clip<=1||pose.clip==5;
 if(cleanup){int upperClip=pose.clip,lowerClip=pose.clip;float upperPhase=pose.phase;
  if(layerAim){upperClip=weapon.crouched?5:0;lowerClip=weapon.crouched?5:pose.clip;
   if(!weapon.crouched&&(!weapon.handgun||cleanupAnimationDuration(track.visual,2)>0)&&state(friendlyKey(actor,"weapon_action"))==int(GuardAction::Fire)){upperClip=2;upperPhase=float(state(friendlyKey(actor,"weapon_action_ms")))/guardActionDuration(GuardAction::Fire);}}
  upperBody=cleanupBodyFrame(cleanupActorBody(track.visual),upperClip,upperPhase);lowerBody=cleanupBodyFrame(cleanupActorBody(track.visual),lowerClip,pose.phase);
  if(weapon.crouched){auto standing=cleanupBodyFrame(cleanupActorBody(track.visual),0,pose.phase);crouchDrop=std::max(0.f,((standing.rightShoulder.y+standing.leftShoulder.y)-(upperBody.rightShoulder.y+upperBody.leftShoulder.y))*.5f)*scale;}
 }
 weapon.grip=Point3{pose.position.x,pose.position.y,pose.z}+right*(-.12f*scale)+forward*(gripForward*scale)+Point3{0,0,(1.024f+.216f*weapon.raise)*scale-crouchDrop};
 const auto carryGrip=weapon.grip;
 // The imported Remington butt centre is .249 m behind the grip, with
 // its centre .0065 m below it. Rotate around that shoulder contact when
 // aiming, rather than lifting the butt above the shoulder on downward aim.
 const bool shoulderWeapon=!weapon.handgun&&cleanup;
 auto seatStock=[&](Point3 direction){if(!shoulderWeapon)return;float h=std::hypot(direction.x,direction.y);auto r=h>.001f?Point3{direction.y/h,-direction.x/h,0}:right;Point3 u{r.y*direction.z,-r.x*direction.z,r.x*direction.y-r.y*direction.x};
  Point3 rebase=lowerBody.pelvis-upperBody.pelvis,pivot=upperBody.spine+rebase,local=upperBody.rightShoulder+rebase-pivot;
  float yaw=layerAim?std::remainder(std::atan2(direction.x,direction.y)-pose.yaw,2*kPi)*weapon.raise:0,pitch=layerAim?std::asin(std::clamp(direction.z,-1.f,1.f))*weapon.raise:0;
  auto shoulder=pivot+actorBodyAim(local,yaw,pitch);
  auto contact=Point3{pose.position.x,pose.position.y,pose.z}+(right*(shoulder.x+.025f)+forward*(shoulder.z+.025f)+Point3{0,0,shoulder.y-.04f})*scale;
  auto seated=contact+direction*(.249f*scale)+u*(.0065f*scale)+r*(.0073f*scale);weapon.grip=carryGrip+(seated-carryGrip)*weapon.raise;
 };
 weapon.direction=unit(forward+Point3{0,0,-.32f});
 if(weapon.raise>0){auto aimed=unit(target-weapon.grip);
  for(int i=0;i<6;++i){seatStock(aimed);float horizontal=std::hypot(aimed.x,aimed.y);auto r=horizontal>.001f?Point3{aimed.y/horizontal,-aimed.x/horizontal,0}:right;Point3 up{r.y*aimed.z,-r.x*aimed.z,r.x*aimed.y-r.y*aimed.x};aimed=unit(target-weapon.grip-up*((weapon.handgun?.035f:.058961f)*scale)-r*((weapon.handgun?0.f:-.007496f)*scale));}
  weapon.direction=unit(weapon.direction*(1-weapon.raise)+aimed*weapon.raise);
 }
 seatStock(weapon.direction);
 float horizontal=std::hypot(weapon.direction.x,weapon.direction.y);auto r=horizontal>.001f?Point3{weapon.direction.y/horizontal,-weapon.direction.x/horizontal,0}:right;Point3 up{r.y*weapon.direction.z,-r.x*weapon.direction.z,r.x*weapon.direction.y-r.y*weapon.direction.x};
 weapon.muzzle=weapon.grip+weapon.direction*((weapon.handgun?.172f:.500522f)*scale)+up*((weapon.handgun?.035f:.058961f)*scale)+r*((weapon.handgun?0.f:-.007496f)*scale);return weapon;
}
NpcWeaponPose Game::npcWeaponPose(size_t actor)const{
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();NpcWeaponPose weapon;
 if(npcIncapacitated(actor)||track.ai.mode!=ActorAiMode::Guard||!track.ai.armed||!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState)))return weapon;
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};float raise=get("weapon_raise")*.001f;
 bool engineering=track.ai.role==GuardSquadRole::Engineer&&(get("drone_windup")||get("drone_busy"));if(engineering)raise=0;
 // Hauled out at gunpoint (the hold): the pistol comes up into the player's
 // view, lifted a hand's breadth over the carry, and is aimed from where it is
 // lifted to: its line, pitch and yaw, runs from the muzzle to the player's
 // eye (aimed at the eye less the lift, then lifted, the line is the same).
 if(get("guard_hold_ms")>0&&get("guard_target_kind")==int(ShotKind::Player)){const Vec2 at=m_player.pos+chunkOffset(m_level)-chunkOffset(npc.level);
  const float lift=guardHandgun(track.ai)?.16f*raise*track.scale/1.6f:0.f;
  weapon=npcWeaponPoseAt(actor,npc.pose,{at.x,at.y,m_player.z+m_player.eye-lift},raise);weapon.grip.z+=lift;weapon.muzzle.z+=lift;}
 else if(get("weapon_target")){auto shift=chunkOffset(npc.level);Point3 target{get("weapon_x")*.001f-shift.x,get("weapon_y")*.001f-shift.y,get("weapon_z")*.001f},cover;if(guardSuppressionPoint(actor,cover))target=cover;weapon=npcWeaponPoseAt(actor,npc.pose,target,raise);}
 else{
  // A raised saved pose without an aim target retains its low-ready direction.
  weapon=npcWeaponPoseAt(actor,npc.pose,{},0);float lift=.216f*raise*track.scale/1.6f;weapon.grip.z+=lift;weapon.muzzle.z+=lift;weapon.raise=raise;
 }
 weapon.flash=get("weapon_flash")/80.f;weapon.recoil=get("weapon_recoil")/220.f;
 weapon.treatment=float(get("support_ms"))/guardTreatmentDuration(track);
 weapon.engineering=track.ai.role==GuardSquadRole::Engineer?std::clamp(get("drone_windup")/1200.f,0.f,1.f):0;
 weapon.maintenance=track.ai.role==GuardSquadRole::Engineer&&get("drone_busy")&&get("drone_repair_target")>0&&!get("drone_windup");
 int padStage=get("engineer_pad_stage");weapon.supportAnimation=track.visual==ActorVisual::CleanupEngineer&&track.ai.role==GuardSquadRole::Engineer&&padStage>0&&npc.pose.clip==0&&!get("guard_danger_ms");
 if(weapon.supportAnimation){weapon.supportClip=padStage==1?6:padStage==2?7:8;float duration=cleanupAnimationDuration(track.visual,weapon.supportClip);weapon.supportPhase=duration>0?std::clamp(get("engineer_pad_ms")/(duration*1000),0.f,1.f):0;weapon.padVisible=padStage==2||(padStage==1&&weapon.supportPhase>=.6f)||(padStage==3&&weapon.supportPhase<.5f);weapon.raise=0;weapon.flash=0;weapon.recoil=0;}
 weapon.pistolVisible=weapon.armed&&weapon.handgun&&!weapon.supportAnimation&&weapon.treatment<=0;
 if(engineering){weapon.flash=0;weapon.recoil=0;}
 weapon.action=GuardAction(get("weapon_action"));weapon.actionProgress=guardActionDuration(weapon.action)?float(get("weapon_action_ms"))/guardActionDuration(weapon.action):0;
 weapon.reload=get("weapon_reloading")?1.f-get("weapon_reload")/1800.f:0;
 int grenade=get("grenade_windup");weapon.grenadeThrow=grenade>0?grenade/450.f+.0001f:0;
 // At a locker's doors his support hand goes to the handle (Lockers.cpp).
 if(get("guard_haul"))weapon.handle=guardLockerHandle(actor,weapon.haul);
 // Both hands follow this cosmetic assembly offset in the shared renderer.
 // Recovery prevents firing from the transient bash pose.
 float bash=get("weapon_bash_pose")/220.f,windup=get("weapon_bash")/300.f;
 auto lunge=weapon.direction*((bash>.0f?.12f*bash:.045f*windup)*track.scale/1.6f);weapon.grip=weapon.grip+lunge;weapon.muzzle=weapon.muzzle+lunge;return weapon;
}
ShotHit Game::guardShot(size_t actor,const ActorPose& pose,const NpcWeaponPose& weapon)const{
 const auto&npc=m_friendlyActors.at(actor);if(!weapon.armed)return {};
 auto shift=chunkOffset(npc.level)-chunkOffset(m_level);auto grip=weapon.grip+Point3{shift.x,shift.y,0},muzzle=weapon.muzzle+Point3{shift.x,shift.y,0};
 auto segment=[&](Point3 from,Point3 to){auto direction=to-from;float distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);if(distance<=.0001f){ShotHit blocked;blocked.kind=ShotKind::Solid;blocked.level=npc.level;blocked.point=from;return blocked;}return traceShot(from,direction,distance,int(actor),false);};
 // Both candidate evaluation and actual firing use the same physical barrel
 // clearance, with this NPC's own hull ignored and all points in active space.
 auto shoulder=Point3{pose.position.x+shift.x,pose.position.y+shift.y,grip.z};auto blocked=segment(shoulder,grip);if(blocked.kind!=ShotKind::None)return blocked;
 blocked=segment(grip,muzzle);if(blocked.kind!=ShotKind::None)return blocked;
 return traceShot(muzzle,weapon.direction,npc.track().ai.attackRange,int(actor),true);
}
void Game::updateGuardCombat(float dt){
 perf::Scope frameProfile(perf::Combat);
 if(dt<=0)return;updateGuardSquadSlots(dt);
 // A man a locker burst into, from the leaves reaching him to the floor (Lockers.cpp).
 updateLockerBurst(dt);
 for(size_t i=0;i<m_friendlyActors.size();++i){auto&npc=m_friendlyActors[i];const auto&track=npc.track();const auto&config=track.ai;if(config.mode!=ActorAiMode::Guard||!config.armed||!chunkResident(npc.level))continue;
  const auto&gun=weaponDef(guardHandgun(config)?WeaponId::Handgun:WeaponId::Shotgun);
  auto get=[&](const char*field){return state(friendlyKey(i,field));};auto put=[&](const char*field,int value){setState(friendlyKey(i,field),value);};
  auto cancelAction=[&](){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);put("weapon_bash",0);put("weapon_bash_pose",0);};
  if(get("evac_level")||npcIncapacitated(i)||!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState))){put("weapon_aim",0);put("weapon_raise",0);put("weapon_flash",0);put("weapon_recoil",0);put("guard_suppress_ms",0);put("weapon_reload",0);put("weapon_reloading",0);put("weapon_reload_events",0);cancelAction();continue;}
  int ms=npcMilliseconds(i,dt,"weapon_fraction");
  auto shift=chunkOffset(npc.level)-chunkOffset(m_level);
  auto pump=[&](float gain,float pitch){m_sounds.push_back({Sound::Pump,npc.pose.position+shift,gain,pitch,true,17,npc.pose.z+track.scale*.7f});};
  auto phase=[&](GuardAction action,int elapsed,unsigned consumed){put("weapon_action",int(action));put("weapon_action_ms",elapsed);put("weapon_action_events",int(consumed));};
  auto action=GuardAction(get("weapon_action"));int elapsed=get("weapon_action_ms");unsigned consumed=unsigned(get("weapon_action_events"));
  // Older saves lack the timeline fields. Adopt their already consumed phase,
  // never retrospectively replaying a strike, flash, shell or refill.
  if(action==GuardAction::None&&get("weapon_bash")){action=GuardAction::Bash;elapsed=get("weapon_bash");consumed=guardActionMask(action,elapsed);phase(action,elapsed,consumed);}
  else if(action==GuardAction::None&&get("weapon_bash_pose")){action=GuardAction::Bash;elapsed=520-get("weapon_bash_pose");consumed=guardActionMask(action,elapsed);phase(action,elapsed,consumed);}
  if(action==GuardAction::Fire||(action==GuardAction::Bash&&elapsed>=300)){
   elapsed=std::min(guardActionDuration(action),elapsed+std::min(ms,1800));auto events=consumeGuardAction(action,elapsed,consumed);
   if(action==GuardAction::Fire&&!guardHandgun(config)&&(events&GuardActionWork))pump(.3f,.9f);
   if(action==GuardAction::Bash)put("weapon_bash_pose",std::max(0,520-elapsed));
   if(events&GuardActionComplete){cancelAction();action=GuardAction::None;}else phase(action,elapsed,consumed);
  }
  if(!get("weapon_saved")){put("weapon_rounds",gun.cycle.tubeCapacity);put("weapon_saved",1);put("weapon_reload",0);put("weapon_reloading",0);put("weapon_reload_events",0);}
  // A loaded tube cannot also be in its empty-tube reload interval. Normalize
  // stale in-memory flags as well as rejecting inconsistent saves on decode.
  if(get("weapon_rounds")>0&&(get("weapon_reload")||get("weapon_reloading"))){put("weapon_reload",0);put("weapon_reloading",0);put("weapon_reload_events",0);}
  // Already committed animations and mechanical recovery describe past work,
  // so they advance even when another task currently owns this guard.
  int recovery=std::max(0,get("weapon_recovery")-ms),oldReload=get("weapon_reload");put("weapon_recovery",recovery);put("weapon_flash",std::max(0,get("weapon_flash")-ms));put("weapon_recoil",std::max(0,get("weapon_recoil")-ms));
  auto target=guardTarget(i);int kind=target.valid()?int(target.kind):0,targetLevel=target.valid()?target.level+1:0,targetIndex=target.valid()?target.index+1:0;
  if(kind!=get("weapon_target_kind")||targetLevel!=get("weapon_target_level")||targetIndex!=get("weapon_target_index")){put("weapon_aim",0);put("weapon_bash",0);put("guard_suppress_ms",0);if(action==GuardAction::Bash&&elapsed<300){cancelAction();action=GuardAction::None;}put("weapon_target_kind",kind);put("weapon_target_level",targetLevel);put("weapon_target_index",targetIndex);}
  Point3 aimPoint{target.position.x,target.position.y,target.feet+target.height*.6f};auto delta=aimPoint-Point3{npc.pose.position.x,npc.pose.position.y,npc.pose.z+track.scale*.85f};
  bool visible=target.valid()&&guardState(i)==GuardState::Alert&&delta.x*delta.x+delta.y*delta.y+delta.z*delta.z<config.attackRange*config.attackRange&&guardCanSee(i,npc.pose,target);
  bool workBlocked=get("guard_danger_ms")||get("drone_busy")||get("engineer_pad_stage")||get("support_ms")||get("support_target");
  bool close=visible&&guardMeleeReady(i,npc.pose,target),bashAllowed=!workBlocked&&guardTaskAllows(i,GuardTask::Bash);
  // Contact defense can own Bash with an empty tube. Pause the reload before
  // its event/refill boundary, preserving its remaining clock and event mask.
  bool meleePreemptsReload=close&&bashAllowed&&!recovery&&npc.pose.clip!=1;
  // A committed reload (events already consumed) must keep ticking after Danger
  // even while Recovery/Hold owns the schedule; only a fresh empty-tube start
  // still requires live Reload task ownership.
  bool reloadCommitted=get("weapon_reloading")&&get("weapon_reload_events")!=0;
  bool reloadAllowed=!workBlocked&&!meleePreemptsReload&&(guardTaskAllows(i,GuardTask::Reload)||reloadCommitted);
  int reload=reloadAllowed?std::max(0,oldReload-ms):oldReload;put("weapon_reload",reload);
  if(reloadAllowed&&get("weapon_reloading")&&!get("weapon_reload_events"))put("weapon_reload_events",int(guardActionMask(GuardAction::Reload,1800-oldReload)));
  if(get("weapon_rounds")==0&&reloadAllowed){
   if(get("weapon_reloading")){
    unsigned reloadEvents=unsigned(get("weapon_reload_events"));if(!reloadEvents)reloadEvents=guardActionMask(GuardAction::Reload,1800-oldReload);
    auto events=consumeGuardAction(GuardAction::Reload,1800-reload,reloadEvents);put("weapon_reload_events",int(reloadEvents));
    if(events&GuardActionWork)pump(.25f,.8f);if(events&GuardActionChamber)pump(.3f,.85f);
    if(events&GuardActionComplete){put("weapon_rounds",gun.cycle.tubeCapacity);put("weapon_reloading",0);put("weapon_reload_events",0);if(action==GuardAction::Reload){cancelAction();action=GuardAction::None;}}
   }else{
    put("weapon_reloading",1);put("weapon_reload",1800);put("weapon_reload_events",GuardActionStart);reload=1800;pump(.4f,.75f);
   }
  }
  if(get("weapon_reloading")&&(reloadAllowed||get("weapon_reload_events"))&&(action==GuardAction::None||action==GuardAction::Reload)){action=GuardAction::Reload;phase(action,1800-reload,unsigned(get("weapon_reload_events")));}
  else if(!get("weapon_reloading")&&action==GuardAction::Reload){cancelAction();action=GuardAction::None;}
  if(visible){auto global=target.position+chunkOffset(npc.level);put("weapon_target",1);put("weapon_x",int(std::round(global.x*1000)));put("weapon_y",int(std::round(global.y*1000)));put("weapon_z",int(std::round(aimPoint.z*1000)));}
  else if(!target.valid())put("weapon_target",0);
  put("guard_suppress_ms",guardSuppressionEnabled(config)&&visible?1200:std::max(0,get("guard_suppress_ms")-ms));
  // Grenade cooldown and personal sighting clocks tick exactly once even when
  // support, reload or movement owns the actor; the helper gates execution.
  int grenadesUsed=get("grenade_used");tryGuardGrenade(i,dt);
  if(workBlocked){put("weapon_aim",0);put("weapon_raise",0);put("weapon_bash",0);if(action==GuardAction::Bash&&elapsed<300)cancelAction();continue;}
  Point3 coverPoint;bool suppress=!visible&&guardSuppressionPoint(i,coverPoint);
  if(get("grenade_windup")>0||get("grenade_used")!=grenadesUsed){put("weapon_aim",0);put("weapon_raise",0);if(action==GuardAction::Bash&&elapsed<300){cancelAction();action=GuardAction::None;}continue;}
  // A move in a fight (an advance, a flank, a sidestep: the Combat schedule's
  // Move) is no ceasefire: with the target in his sight he has the gun up and
  // fires on the way whenever the shot line is clear (ready, below). Before,
  // a man walking in place against a yard fence on ALERT never raised it.
  const bool repositioning=visible&&get("schedule")==int(GuardSchedule::Combat)&&get("task")==int(GuardTask::Move)&&get("task_status")==int(GuardTaskStatus::Running);
  bool fireAllowed=guardTaskAllows(i,GuardTask::Fire)||repositioning;
  // Hauled out of a locker, the player gets the muzzle in their face and a
  // moment to answer it (run, hit him, or be shot) before he fires or swings:
  // the gun comes up at twice the pace and stays up through the hold.
  const int hold=get("guard_hold_ms"),pace=hold>0?ms*6:ms*3;
  int raised=hold>0||((fireAllowed||bashAllowed)&&(visible||suppress)&&(!reload||close)&&(npc.pose.clip!=1||repositioning))?1000:0;int current=get("weapon_raise");put("weapon_raise",current+std::clamp(raised-current,-pace,pace));
  if(hold>0){put("guard_hold_ms",std::max(0,hold-ms));put("weapon_aim",0);put("weapon_bash",0);if(action==GuardAction::Bash&&elapsed<300){cancelAction();action=GuardAction::None;}continue;}
  // At contact range a finite, interruptible weapon bash takes precedence.
  // It is usable with an empty tube, and never spends ammunition or shares a
  // frame with a firearm attack. The live target/cover test runs every tick.
  if(bashAllowed&&close&&!recovery&&npc.pose.clip!=1){
   put("weapon_aim",0);if(get("weapon_raise")<1000){put("weapon_bash",0);continue;}
   if(action!=GuardAction::Bash){action=GuardAction::Bash;elapsed=0;consumed=0;}
   elapsed=std::min(300,elapsed+std::min(ms,1800));auto events=consumeGuardAction(action,elapsed,consumed);phase(action,elapsed,consumed);put("weapon_bash",elapsed);
   auto weapon=npcWeaponPose(i);auto origin=weapon.grip+Point3{shift.x,shift.y,0};
   if(events&GuardActionStart)m_sounds.push_back({Sound::PunchSwing,{origin.x,origin.y},.45f,.9f,true,17,origin.z});
   if(!(events&GuardActionWork))continue;
   put("weapon_bash",0);put("weapon_bash_pose",220);put("weapon_recovery",900);put("weapon_flash",0);
   damageGuardTarget(i,12.f);m_sounds.push_back({Sound::PunchHit,{origin.x,origin.y},.7f,.85f,true,17,origin.z});auto global=chunkOffset(m_level);m_guardShots.push_back({{origin.x+global.x,origin.y+global.y},origin.z,4.f,.7f,true,int(i)});continue;
  }
  put("weapon_bash",0);if(action==GuardAction::Bash&&elapsed<300){cancelAction();action=GuardAction::None;if(get("weapon_reloading"))phase(GuardAction::Reload,1800-reload,unsigned(get("weapon_reload_events")));}
  // A flank still to reach holds no fire either: whenever he has a clear line he takes it.
  if(!fireAllowed||(!visible&&!suppress)||reload||recovery||(npc.pose.clip==1&&!repositioning)||get("weapon_raise")<1000||!guardFireSlot(i)){put("weapon_aim",0);continue;}
  auto weapon=suppress?npcWeaponPoseAt(i,npc.pose,coverPoint,1):npcWeaponPose(i);auto origin=weapon.muzzle+Point3{shift.x,shift.y,0};auto hit=guardShot(i,npc.pose,weapon);
  // Seeing the head is insufficient: the barrel and its full shot line must
  // clear cover and intervening bodies. Cancel an interrupted aim completely.
  bool ready=(visible||suppress)&&!reload&&!recovery&&(npc.pose.clip!=1||repositioning)&&weapon.raise>=.999f&&(suppress?hit.kind==ShotKind::Solid&&hit.distance>.15f:guardShotMatches(i,hit));
  int aim=ready?std::min(config.aimMs,get("weapon_aim")+ms):0;put("weapon_aim",aim);
  if(aim<config.aimMs)continue;
  unsigned fireEvents=0;auto events=consumeGuardAction(GuardAction::Fire,0,fireEvents);phase(GuardAction::Fire,0,fireEvents);
  if(!(events&GuardActionStart))continue;
  put("weapon_aim",0);put("weapon_recovery",config.recoveryMs);put("weapon_flash",80);put("weapon_recoil",220);put("weapon_rounds",get("weapon_rounds")-1);
  m_sounds.push_back({gun.cycle.fireSound,{origin.x,origin.y},.7f,1,true,17,origin.z});auto global=chunkOffset(m_level);m_guardShots.push_back({{origin.x+global.x,origin.y+global.y},origin.z,14.f*std::sqrt(.7f),.7f,true,int(i)});
  resolveShot(hit,origin,weapon.direction,gun.hitscan,false,guardHandgun(config)?std::min(config.attackDamage,10.f):config.attackDamage,{ShotKind::Npc,npc.level,int(i)});
 }
}
void Game::validateGuardActions(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto action=GuardAction(get("weapon_action"));int elapsed=get("weapon_action_ms"),events=get("weapon_action_events"),reloadEvents=get("weapon_reload_events");
 if(events<0||events>15||!validGuardAction(action,elapsed,unsigned(events))||reloadEvents<0||reloadEvents>15||get("guard_suppress_ms")<0||get("guard_suppress_ms")>1200)throw std::runtime_error("Invalid saved guard action phase");
 // Zero event masks are accepted for pre-timeline saves and adopted without
 // replay. A new timeline must agree with the existing mechanical reload.
 if(reloadEvents&&(!get("weapon_reloading")||reloadEvents!=int(guardActionMask(GuardAction::Reload,1800-get("weapon_reload")))))throw std::runtime_error("Inconsistent saved guard reload events");
 if(action==GuardAction::Reload&&(!get("weapon_reloading")||elapsed!=1800-get("weapon_reload")||events!=reloadEvents))throw std::runtime_error("Inconsistent saved guard reload animation");
}
}
