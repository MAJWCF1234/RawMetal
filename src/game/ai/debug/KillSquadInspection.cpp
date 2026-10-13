#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <initializer_list>
namespace retro {
// Isolated poses exercise the actual drawActor path; this fixture deliberately
// does not claim AI encounter progress from manufactured animation states.
Game Game::cleanupLayerInspection(int role,int sample,int view,float phase){
 auto authored=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");
 if(role<0||role>5||sample<0||sample>15||authored->maps[0]->actorTracks.empty())throw std::runtime_error("Invalid cleanup layer inspection");
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Cleanup animation inspection";campaign->key=883013;
 auto map=std::make_shared<AuthoredMapData>();map->name="Unobstructed pose inspection";map->definition={{0,0},{10.5f,9.5f},0,Environment::Interior,false,8,.7f,42};
 AuthoredLayerData floor;for(auto&row:floor.rows)row=std::string(24,'.');map->layers.push_back(floor);
 auto track=authored->maps[0]->actorTracks.front();track.visual=ActorVisual(int(ActorVisual::Cleanup)+role);track.scale=1.6f;
 track.timerState=stateId("cleanup_inspection_clock");track.keys={{0,{8.5f,8.5f},0,0},{30000,{8.5f,8.5f},0,0}};track.idleKeys.clear();track.ai.patrol={{{8.5f,8.5f},0,1000}};
 TimedSequence clock;clock.timerState=track.timerState;clock.finishState=stateId("cleanup_inspection_done");clock.loop=true;clock.soundIntervalMs=0;map->timedSequences.push_back(clock);
 constexpr GuardSquadRole roles[]={GuardSquadRole::Assault,GuardSquadRole::Engineer,GuardSquadRole::Sharpshooter,GuardSquadRole::Heavy,GuardSquadRole::Medic,GuardSquadRole::Rusher};
 track.ai.role=roles[role];track.ai.mode=ActorAiMode::Guard;track.ai.armed=true;track.ai.enableState=0;track.ai.weapon=1;
 map->actorTracks={track};campaign->maps.push_back(map);Game g(campaign);auto&npc=g.m_friendlyActors.at(0);auto&pose=npc.pose;
 pose.yaw=0;pose.phase=phase<0?(sample==2?.99f:.5f):std::clamp(phase,0.f,1.f);pose.clip=sample==0?0:(sample==7||sample==12||sample==13)?5:sample==11?4:1;
 auto put=[&](const char*key,int value){g.setState(g.friendlyKey(0,key),value);};
 float yaw=sample==5?-.55f:sample==6?.55f:sample==12?.70f:sample==13?-.70f:0,pitch=sample==3?.50f:sample==4?-.50f:sample==12?.55f:sample==13?-.45f:0;
 Point3 direction{std::sin(yaw)*std::cos(pitch),std::cos(yaw)*std::cos(pitch),std::sin(pitch)};
 auto target=Point3{pose.position.x,pose.position.y,pose.z+1.24f}+direction*5.f;
 put("weapon_raise",1000);put("weapon_target",1);put("weapon_x",int(std::lround(target.x*1000)));put("weapon_y",int(std::lround(target.y*1000)));put("weapon_z",int(std::lround(target.z*1000)));
 put("crouched",pose.clip==5||sample==15);if(sample==14){put("weapon_raise",0);put("weapon_target",0);}if(sample==8){put("weapon_action",int(GuardAction::Fire));put("weapon_action_ms",100);put("weapon_recoil",110);put("weapon_flash",50);}
 if(sample==9){put("weapon_action",int(GuardAction::Reload));put("weapon_action_ms",900);put("weapon_reloading",1);put("weapon_reload",900);}
 if(sample==10&&role==1){put("drone_busy",1);put("drone_windup",600);pose.clip=0;}
 if(sample==10&&role==4){put("support_ms",phase<0?1200:std::max(1,int(pose.phase*2400)));pose.clip=0;}
 Vec2 forward{std::sin(pose.yaw),std::cos(pose.yaw)},right{forward.y,-forward.x};
 g.m_player.pos=pose.position+(view?right*2.4f+forward*.35f:forward*2.5f+right*.6f);g.m_player.z=pose.z;
 g.m_player.angle=std::atan2(pose.position.y-g.m_player.pos.y,pose.position.x-g.m_player.pos.x);g.m_player.pitch=-30;
 g.m_weaponEquipped=false;g.m_holster=1;g.m_damageFlash=0;return g;
}
Game Game::killSquadInspection(int stage){
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");Game g(campaign);
 if(stage<0||stage>13||g.chunkCount()!=3)throw std::runtime_error("Kill squad inspection requires the actual stitched Ash export");
 constexpr std::array<GuardSquadRole,6> roles={GuardSquadRole::Engineer,GuardSquadRole::Assault,GuardSquadRole::Sharpshooter,GuardSquadRole::Heavy,GuardSquadRole::Medic,GuardSquadRole::Rusher};
 std::array<size_t,6> members;members.fill(g.npcActors().size());
 for(size_t actor=0;actor<g.npcActors().size();++actor){const auto&npc=g.npcActors()[actor];if(npc.homeLevel!=1||npc.track().ai.mode!=ActorAiMode::Guard)continue;
  for(size_t role=0;role<roles.size();++role)if(npc.track().ai.role==roles[role]){if(members[role]!=g.npcActors().size())throw std::runtime_error("Cleanup inspection has duplicate cargo squad roles");members[role]=actor;}}
 for(size_t actor:members)if(actor==g.npcActors().size())throw std::runtime_error("Cleanup inspection requires all six authored cargo squad roles");
 const size_t engineer=members[0],heavy=members[3],medic=members[4];
 // Enter the real authoring trigger; no manufactured alert/target/weapon state.
 g.m_player.pos={26,20};g.crossChunkBoundary();g.updateStreaming(0);for(int i=0;i<12;++i)g.update({},1.f/60);
 if(g.level()!=1||!g.state("cleanup_enabled"))throw std::runtime_error("Cleanup entry did not enable the authored kill squad");
 g.m_player.pos={6,20};
 auto dumpFailure=[&](const char*name,bool milestone){
  std::filesystem::create_directories("diagnostics/kill-squad-20261011");
  std::ofstream report(std::string("diagnostics/kill-squad-20261011/")+name+"-runtime-failure.txt");
  auto grenades=g.tacticalGrenades();
  report<<"stage="<<stage<<" milestone="<<milestone<<" elapsed="<<g.elapsed()<<" observerHealth="<<g.player().health<<" observerDead="<<g.dead()<<" level="<<g.level()
   <<" observer="<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<" cleanupEnabled="<<g.state("cleanup_enabled")<<" activeGrenades="<<grenades.size()<<" activeDrones="<<g.engineerDrones().size()<<'\n';
  for(const auto&grenade:grenades)report<<"grenade level="<<grenade.level<<" position="<<grenade.position.x<<','<<grenade.position.y<<','<<grenade.position.z<<" fuse="<<grenade.fuse<<'\n';
  for(const auto&explosion:g.barrelExplosions())report<<"explosion level="<<explosion.level<<" position="<<explosion.pos.x<<','<<explosion.pos.y<<','<<explosion.z<<" age="<<g.elapsed()-explosion.time<<'\n';
  for(const auto&sound:g.sounds())report<<"sound="<<int(sound.sound)<<" position="<<sound.position.x<<','<<sound.position.y<<','<<sound.z<<" gain="<<sound.gain<<" radius="<<sound.radius<<'\n';
  for(size_t actor:members){const auto&npc=g.npcActors()[actor];const auto&track=npc.track();auto weapon=g.npcWeaponPose(actor);
   report<<"actor="<<actor<<" role="<<guardRoleProfile(track.ai.role).name<<" level="<<npc.level<<" homeLevel="<<npc.homeLevel<<" position="<<npc.pose.position.x<<','<<npc.pose.position.y<<','<<npc.pose.z
    <<" yaw="<<npc.pose.yaw<<" clip="<<npc.pose.clip<<" health="<<track.health-g.state(track.damageState)<<" dead="<<(npc.pose.clip==4||g.state(track.deadState))<<" flash="<<weapon.flash<<'\n';
   auto fields=[&](const char*group,std::initializer_list<const char*>names){report<<group;for(const char*field:names)report<<' '<<field<<'='<<g.state(g.friendlyKey(actor,field));report<<'\n';};
   fields("perception",{"guard_mode","guard_seen","guard_confirmed","guard_memory","guard_target_kind","guard_target_level","guard_target_index","guard_x","guard_y","guard_z","guard_under_fire","guard_hurt_amount","guard_danger_ms"});
   fields("ownership",{"schedule","schedule_step","task","task_status","task_cursor","task_cursor_saved","task_ms","task_subject","task_reason","task_failures","task_stall","task_conditions"});
   fields("combat",{"weapon_target_kind","weapon_target_level","weapon_target_index","weapon_rounds","weapon_raise","weapon_aim","weapon_reload","weapon_reloading","weapon_reload_events","weapon_recovery","weapon_action","weapon_action_ms","weapon_action_events","weapon_bash","guard_slot","guard_slot_ms","grenade_windup","grenade_used"});
   fields("support",{"support_target","support_ms","support_used","drone_windup","drone_busy","drone_used","drone_repair_target","guard_engineer_active","guard_engineer_goal","guard_patient_move"});
   fields("movement",{"guard_route_target","guard_route_next","guard_route_wait","guard_cover","guard_cover_mode","repath","stuck","goal_x","goal_y","way_x","way_y","crouched","grounded","vv"});
  }
 };
 if(stage>=11){
  // Wound the Heavy in view, then leave the cargo bay so the Medic can own
  // treatment without continuous Player fire interleaved with the Creature.
  g.m_player.pos={6,20};g.damageFriendly(heavy,60,g.player().pos,{});
  g.m_player.pos={1.5f,1.5f};g.m_player.z=0;bool reached=false;
  for(int i=0;i<2400&&!g.dead();++i){g.update({},1.f/60);float work=g.npcWeaponPose(medic).treatment;if(stage==11?work>=.18f:stage==12?work>=.55f:g.state(g.friendlyKey(medic,"support_used"))>0){reached=true;break;}}
  if(!reached){std::ofstream log("diagnostics/kill-squad-20261011/medic-runtime-failure.txt");for(size_t actor:{heavy,medic}){const auto&npc=g.npcActors()[actor];log<<"actor="<<actor<<" level="<<npc.level<<" position="<<npc.pose.position.x<<','<<npc.pose.position.y<<" health="<<npc.track().health-g.state(npc.track().damageState)<<" clip="<<npc.pose.clip<<'\n';for(const char*field:{"support_target","support_ms","support_used","support_cooldown","weapon_recovery","weapon_action","weapon_reload","grenade_windup","guard_mode","stuck","repath","way_x","way_y","schedule","task"})log<<field<<'='<<g.state(g.friendlyKey(actor,field))<<'\n';}throw std::runtime_error("Authored Medic did not physically open its kit and treat the injured squadmate");}
 }
 if(stage==1){bool reached=false;for(int i=0;i<600&&!g.dead();++i){g.update({},1.f/60);if(!g.tacticalGrenades().empty()){reached=true;break;}}if(!reached)throw std::runtime_error("Authored grenadier did not release a physical projectile");}
 if(stage==2){
  // The supplied encounter now includes a Creature. A physical squad shot
  // may correctly damage that foe instead of the distant player observer.
  auto targetHealth=[&](AiTargetRef target)->float{
   if(target.kind==ShotKind::Player)return g.player().health;
   if(target.kind==ShotKind::Npc){if(target.index<0||size_t(target.index)>=g.npcActors().size())return -1;const auto&npc=g.npcActors()[size_t(target.index)];return float(std::max(0,npc.track().health-g.state(npc.track().damageState)));}
   if(target.kind==ShotKind::Drone){for(const auto&drone:g.engineerDrones())if(drone.slot==target.index)return drone.health;return 0;}
   if(target.level<0||target.level>=g.chunkCount()||!g.chunkResident(target.level)||target.index<0)return -1;
   if(target.kind==ShotKind::Enemy){const auto&enemies=target.level==g.m_level?g.m_enemies:g.m_chunks[size_t(target.level)].enemies;return size_t(target.index)<enemies.size()?enemies[size_t(target.index)].hp:-1;}
   if(target.kind==ShotKind::Actor){const auto&world=g.friendlyWorld(target.level);if(size_t(target.index)>=world.actorTracks().size())return -1;const auto&track=world.actorTracks()[size_t(target.index)];return float(std::max(0,track.health-g.state(track.damageState)));}
   return -1;
  };
  struct ShotBefore{AiTarget target;float health=-1;int rounds=0;bool clear=false;};
  bool reached=false,emitted=false;size_t shooter=g.npcActors().size();AiTarget hitTarget;float healthBefore=-1,healthAfter=-1;int spent=0;
  for(int frame=0;frame<600&&!g.dead();++frame){std::array<ShotBefore,6> before{};float elapsedBefore=g.elapsed();
   for(size_t role=0;role<members.size();++role){size_t actor=members[role];auto&shot=before[role];const auto&npc=g.npcActors()[actor];shot.target=g.guardTarget(actor);shot.health=targetHealth(shot.target);shot.rounds=g.state(g.friendlyKey(actor,"weapon_rounds"));
    if(shot.target.valid()&&factionsHostile(actorFaction(npc.track()),shot.target.faction)&&g.state(g.friendlyKey(actor,"guard_seen"))&&g.state(g.friendlyKey(actor,"guard_confirmed"))){
     Point3 aim{shot.target.position.x,shot.target.position.y,shot.target.feet+shot.target.height*.6f};shot.clear=g.guardShotMatches(actor,g.guardShot(actor,npc.pose,g.npcWeaponPoseAt(actor,npc.pose,aim,1)));}
   }
   g.update({},1.f/60);
   bool explosion=std::any_of(g.barrelExplosions().begin(),g.barrelExplosions().end(),[&](const auto&event){return event.time>elapsedBefore;});
   for(size_t role=0;role<members.size();++role){size_t actor=members[role];const auto&shot=before[role];auto get=[&](const char*field){return g.state(g.friendlyKey(actor,field));};
    bool fired=get("weapon_action")==int(GuardAction::Fire)&&get("weapon_action_ms")==0&&(get("weapon_action_events")&GuardActionStart)&&get("weapon_rounds")<shot.rounds;
    emitted|=fired;bool same=get("weapon_target_kind")==int(shot.target.kind)&&get("weapon_target_index")==shot.target.index+1&&(shot.target.kind==ShotKind::Npc||get("weapon_target_level")==shot.target.level+1);
    float remaining=targetHealth(shot.target);
    if(!explosion&&fired&&same&&shot.clear&&shot.health>0&&remaining>=0&&remaining<shot.health){reached=true;shooter=actor;hitTarget=shot.target;healthBefore=shot.health;healthAfter=remaining;spent=shot.rounds-get("weapon_rounds");break;}
   }
   if(reached)break;
  }
  if(!reached){dumpFailure("firearm-shot",emitted);throw std::runtime_error("Authored squad firearm event did not produce measured physical hostile damage outside a detonation frame");}
  std::filesystem::create_directories("diagnostics/kill-squad-20261011");std::ofstream report("diagnostics/kill-squad-20261011/firearm-shot-runtime-result.txt");
  report<<"PASS elapsed="<<g.elapsed()<<" shooter="<<shooter<<" role="<<guardRoleProfile(g.npcActors()[shooter].track().ai.role).name<<" targetKind="<<int(hitTarget.kind)<<" targetLevel="<<hitTarget.level<<" targetIndex="<<hitTarget.index
   <<" hostileHealthBefore="<<healthBefore<<" hostileHealthAfter="<<healthAfter<<" firearmAmmoSpent="<<spent<<" observerHealth="<<g.player().health<<" physicalShotClear=1 detonationFrame=0\n";
 }
 if(stage==4){bool reached=false;for(int i=0;i<600&&!g.dead();++i){g.update({},1.f/60);if(g.npcWeaponPose(engineer).grenadeThrow>.7f&&g.tacticalGrenades().empty()){reached=true;break;}}if(!reached)throw std::runtime_error("Authored engineer did not telegraph its physical grenade throw");}
 if(stage==3){
  // Establish the prerequisite on a mobile role using genuine perception.
  // Remaining at the old western observer point lets these members correctly
  // prefer the nearer authored Creature, so it does not test Player pursuit.
  bool positioned=false;Vec2 cargoContact{};
  for(Vec2 point:{Vec2{20.5f,19.f},Vec2{19.5f,19.f},Vec2{18.5f,19.5f},Vec2{20.5f,20.f}}){float feet=g.friendlyWorld(1).supportBelow(point.x,point.y,g.player().z+.265f);
   if(!g.hullFits(point,feet,Player::StandingHeight))continue;g.m_player.pos=point;g.m_player.z=feet;cargoContact=point;positioned=true;break;}
  if(!positioned){dumpFailure("player-contact-position",false);throw std::runtime_error("No hull-safe cargo observer position could establish the Player-contact prerequisite");}
  auto personallySeesPlayer=[&](size_t actor){const auto&npc=g.npcActors()[actor];return g.state(g.friendlyKey(actor,"guard_seen"))&&g.state(g.friendlyKey(actor,"guard_confirmed"))&&
   g.guardTarget(actor).kind==ShotKind::Player&&g.guardCanSee(actor,npc.pose,g.guardTarget(actor));};
  size_t pursuer=g.npcActors().size();int cargoContactFrames=0;
  for(;cargoContactFrames<600&&!g.dead();++cargoContactFrames){g.update({},1.f/60);for(size_t actor:{members[5],members[1]})if(personallySeesPlayer(actor)){pursuer=actor;break;}if(pursuer!=g.npcActors().size())break;}
  if(pursuer==g.npcActors().size()){dumpFailure("player-contact-before-pursuit",false);throw std::runtime_error("A mobile cargo squad member did not personally confirm the Player; south pursuit was not tested");}
  const Vec2 acrossSeam{20.f,25.5f};float seamFeet=g.player().z;
  if(!g.friendlyWalkSegment(1,g.player().pos,g.player().z,acrossSeam,Player::StandingHeight,&seamFeet)){
   dumpFailure("south-opening-observer-path",true);throw std::runtime_error("The observer's south-seam contact path was not physically open");}
  g.m_player.pos=acrossSeam;g.m_player.z=seamFeet;g.crossChunkBoundary();g.updateStreaming(0);
  bool seamContact=false,seamApproach=false;int seamContactFrames=-1,seamApproachFrames=0;
  for(;seamApproachFrames<300&&!g.dead();++seamApproachFrames){g.update({},1.f/60);const auto&npc=g.npcActors()[pursuer];
   bool sees=g.level()==2&&personallySeesPlayer(pursuer)&&g.state(g.friendlyKey(pursuer,"guard_y"))>=24000;
   if(sees&&!seamContact){seamContact=true;seamContactFrames=seamApproachFrames;}
   // Keep the observer in the south lane while the role actually approaches.
   // Withdrawing immediately on the first far-side sighting leaves the old
   // Creature closer and visible, which is legitimate threat retargeting.
   if(sees&&(npc.level==2||(npc.level==1&&npc.pose.position.y>=21.5f))){seamApproach=true;break;}}
  if(!seamContact){dumpFailure("player-contact-across-south-seam",false);throw std::runtime_error("The selected pursuer did not personally observe the Player beyond the south seam; far pursuit was not tested");}
  if(!seamApproach){dumpFailure("confirmed-player-seam-approach",false);throw std::runtime_error("The selected pursuer did not approach its personally confirmed Player in the south lane; far pursuit was not tested");}
  Vec2 observedGlobal{g.state(g.friendlyKey(pursuer,"guard_x"))*.001f,g.state(g.friendlyKey(pursuer,"guard_y"))*.001f};
  // Pull the observer deeper into the south chunk so the pursuer has to cross.
  // Hull-safe candidates only — navigation of the observer is not under test.
  bool sheltered=false;Vec2 refuge{};float refugeFeet=g.player().z;
  for(Vec2 point:{Vec2{18.5f,17.5f},Vec2{21.5f,5.5f},Vec2{12.5f,6.5f},Vec2{20.5f,4.5f},Vec2{16.5f,8.5f}}){
   float feet=g.friendlyWorld(2).supportBelow(point.x,point.y,g.player().z+.265f);if(!g.hullFits(point,feet,Player::StandingHeight))continue;
   refuge=point;refugeFeet=feet;sheltered=true;break;}
  if(!sheltered){dumpFailure("refuge-observer-path",true);throw std::runtime_error("No hull-safe south-chunk refuge observer position is available");}
  g.m_player.pos=refuge;g.m_player.z=refugeFeet;bool crossed=false;int pursuitFrames=0;
  for(;pursuitFrames<900&&!g.dead();++pursuitFrames){g.update({},1.f/60);crossed=g.npcActors()[pursuer].level==2;if(crossed)break;}
  if(!crossed){dumpFailure("south-pursuit",false);throw std::runtime_error("The personally confirmed Player pursuer did not physically cross the south seam");}
  std::filesystem::create_directories("diagnostics/kill-squad-20261011");std::ofstream report("diagnostics/kill-squad-20261011/south-pursuit-runtime-result.txt");const auto&npc=g.npcActors()[pursuer];
  report<<"PASS pursuer="<<pursuer<<" role="<<guardRoleProfile(npc.track().ai.role).name<<" cargoObserver="<<cargoContact.x<<','<<cargoContact.y<<" cargoContactFrames="<<cargoContactFrames
   <<" seamContactFrames="<<seamContactFrames<<" seamApproachFrames="<<seamApproachFrames<<" personallyObservedGlobal="<<observedGlobal.x<<','<<observedGlobal.y<<" pursuitFrames="<<pursuitFrames<<" crossedLevel="<<npc.level
   <<" pursuer="<<npc.pose.position.x<<','<<npc.pose.position.y<<','<<npc.pose.z<<" observerHealth="<<g.player().health<<" observerDead="<<g.dead()<<'\n';
 }
 // Observer is positioned only after normal simulation reaches the milestone.
 if(stage==10){bool reached=false;for(int i=0;i<900&&!g.dead();++i){g.update({},1.f/60);for(const auto&drone:g.engineerDrones())if(drone.owner==int(engineer)){reached=true;break;}if(reached)break;}if(!reached)throw std::runtime_error("Authored Engineer did not deploy its supervised drone");}
 if(stage==10){auto drones=g.engineerDrones();auto found=std::find_if(drones.begin(),drones.end(),[&](const auto&drone){return drone.owner==int(engineer);});auto p=found->position;g.m_player.pos={p.x+2,p.y+1};g.m_player.angle=std::atan2(p.y-g.m_player.pos.y,p.x-g.m_player.pos.x);}
 else if(stage>=4){const auto&pose=g.npcActors()[stage>=11?medic:members[size_t(stage-4)]].pose;bool positioned=false;
  for(float offset:{0.f,.8f,-.8f,1.6f,-1.6f,kPi}){float turn=offset+(stage>=11?1.6f:0.f);auto forward=Vec2{std::sin(pose.yaw+turn),std::cos(pose.yaw+turn)};auto camera=pose.position+forward*2.1f+Vec2{forward.y,-forward.x}*.45f;if(g.hullFits(camera,pose.z,Player::StandingHeight)){g.m_player.pos=camera;positioned=true;break;}}
  if(!positioned)throw std::runtime_error("No clear observer position around cleanup role");g.m_player.angle=std::atan2(pose.position.y-g.m_player.pos.y,pose.position.x-g.m_player.pos.x);}
 else if(stage==3){g.m_player.pos={21.5f,5.5f};g.m_player.angle=std::atan2(1.f-5.5f,18.5f-21.5f);}
 else{g.m_player.pos={6,18};g.m_player.angle=std::atan2(12.5f-18,8.5f-6);}
 if(!g.hullFits(g.player().pos,g.player().z,Player::StandingHeight))throw std::runtime_error("Cleanup inspection observer intersects geometry");
 g.m_player.pitch=stage>=4?-20.f:-8.f;g.m_damageFlash=0;g.m_weaponEquipped=false;g.m_holster=1;
 std::filesystem::create_directories("diagnostics/guard-schedule-20261012");std::ofstream tasks("diagnostics/guard-schedule-20261012/ash-task-ownership.csv",stage==0?std::ios::trunc:std::ios::app);
 if(stage==0)tasks<<"stage,actor,schedule,step,task,status,subject,reason,failures,transitions\n";
 for(size_t actor:members){auto get=[&](const char*f){return g.state(g.friendlyKey(actor,f));};
  if(get("support_ms")&&!g.guardTaskAllows(actor,GuardTask::Treat))throw std::runtime_error("Authored healing has no running treatment owner");
  if(get("drone_busy")&&!g.guardTaskAllows(actor,get("drone_windup")?GuardTask::Deploy:GuardTask::Repair))throw std::runtime_error("Authored terminal has no running engineering owner");
  tasks<<stage<<','<<actor;for(const char*f:{"schedule","schedule_step","task","task_status","task_subject","task_reason","task_failures","task_transitions"})tasks<<','<<get(f);tasks<<'\n';
 }
 return g;
}
}
