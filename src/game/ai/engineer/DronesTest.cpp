#include "../../Game.h"
#include "Drones.h"
#include "../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
namespace {StateId droneTestKey(int slot,std::string_view field){auto text="drone/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;for(unsigned char c:text)key=(key^c)*16777619u;return key|0x80000000u;}float norm3(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}}
bool Game::testEngineerDrones(){
 std::ofstream report("engineer-drone-test.txt");bool passed=true;int checks=0;auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](auto edit){auto campaign=std::make_shared<CustomCampaign>();campaign->name="Engineer drone validation";campaign->key=882746;auto map=std::make_shared<AuthoredMapData>();map->name="Drone test room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("drone_clock");timer.finishState=stateId("drone_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{30000,{6.5f,6.5f},0,kPi*.5f}};guard.scale=1.6f;guard.health=70;guard.damageState=stateId("drone_guard_damage");guard.deadState=stateId("drone_guard_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.role=GuardSquadRole::Engineer;guard.ai.armed=true;guard.ai.squad=stateId("drone_team");guard.ai.faction=AiFaction::Raider;guard.ai.enableState=stateId("drone_enabled");guard.ai.patrol={{{6.5f,6.5f},0,1000}};map->actorTracks.push_back(guard);edit(*map);campaign->maps.push_back(map);Game g(campaign);g.setState(stateId("drone_enabled"),1);return g;
 };
 auto tick=[](Game&g,int frames,int rate=60,bool game=false){for(int frame=0;frame<frames;++frame){if(game)g.update({},1.f/rate);else{g.m_npcNavBudget=4;g.updateEngineerDrones(1.f/rate);}}};
 auto known=[](Game&g){AiTarget target;target.kind=ShotKind::Player;target.level=g.level();target.index=0;target.position=g.player().pos;target.feet=g.player().z;target.height=g.player().hullHeight();target.eye=g.player().eye;target.faction=AiFaction::Player;g.rememberGuardTarget(0,target);g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));g.setState(g.friendlyKey(0,"guard_confirmed"),1);g.setState(g.friendlyKey(0,"guard_seen"),1);g.setState(g.friendlyKey(0,"guard_memory"),1);g.setState(g.friendlyKey(0,"guard_x"),int(std::round(target.position.x*1000)));g.setState(g.friendlyKey(0,"guard_y"),int(std::round(target.position.y*1000)));g.setState(g.friendlyKey(0,"guard_z"),int(std::round(target.feet*1000)));};
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.definition.playerStart={10.5f,6.5f};});tick(g,rate,rate,true);check(g.engineerDrones().empty(),"Engineer deployment requires both personal recognition and a visible 1.2-second assembly interval");tick(g,rate*2,rate,true);
  auto poses=g.engineerDrones();check(poses.size()==1&&poses[0].owner==0&&poses[0].health==EngineerDroneHealth&&std::fabs(poses[0].position.z)<.001f,"Confirmed Engineer assembles one actual grounded drone at 60/120 Hz");
  check(g.state(g.friendlyKey(0,"drone_used"))==1&&g.state(g.friendlyKey(0,"drone_repair_charge"))==80,"Deployment spends finite stock and initializes a finite maintenance reservoir");
 }
 {auto g=scene([](auto&){});tick(g,240,60,true);check(g.engineerDrones().empty()&&!g.state(g.friendlyKey(0,"drone_used")),"Engineer without any personally observed threat never fabricates a deployed drone");}
 {auto g=scene([](auto&map){map.actorTracks[0].ai.role=GuardSquadRole::Assault;});check(!g.deployEngineerDrone(0)&&!g.deployEngineerDrone(99),"Drone deployment rejects non-Engineer and nonexistent owners");}
 {auto g=scene([](auto&map){map.actorTracks[0].ai.faction=AiFaction::Facility;});g.deployEngineerDrone(0);auto drones=g.engineerDrones();auto target=g.engineerDroneTarget(0,g.level());
  check(drones.size()==1&&target.valid()&&target.kind==ShotKind::Drone&&target.index==drones[0].slot&&target.level==drones[0].level&&target.faction==AiFaction::Facility&&length(target.position-Vec2{drones[0].position.x,drones[0].position.y})<.001f&&target.height==EngineerDroneHeight,"Generic AI target exposes the physical drone's stable slot, owner faction and correct body geometry");
  check(!g.engineerDroneTarget(-1,0).valid()&&!g.engineerDroneTarget(0,-1).valid()&&!g.engineerDroneTarget(EngineerDroneSlots,0).valid(),"Drone target lookup rejects invalid slots and observer chunks");
  g.setState(stateId("drone_enabled"),0);check(!g.engineerDroneTarget(0,0).valid(),"Disabled owner's drone immediately stops being a valid AI target before cleanup");
 }
 {auto g=scene([](auto&map){map.definition.playerStart={10.5f,6.5f};});known(g);tick(g,1);check(g.state(g.friendlyKey(0,"drone_busy"))==1&&g.state(g.friendlyKey(0,"drone_windup"))>0,"Committed assembly publishes a persisted busy flag before weapon combat");g.setState(g.friendlyKey(0,"weapon_recovery"),200);tick(g,1);check(g.state(g.friendlyKey(0,"drone_busy"))==0&&g.state(g.friendlyKey(0,"drone_windup"))==0,"Weapon recovery interrupts assembly and releases the Engineer's busy state");}
 for(const char*field:{"weapon_reload","weapon_reloading","weapon_bash","weapon_bash_pose","grenade_windup"}){auto g=scene([](auto&map){map.definition.playerStart={10.5f,6.5f};});known(g);g.setState(g.friendlyKey(0,field),1);tick(g,1);check(!g.state(g.friendlyKey(0,"drone_busy"))&&!g.state(g.friendlyKey(0,"drone_windup")),"Incompatible reload, bash or grenade stage prevents simultaneous drone assembly");}
 for(auto action:{GuardAction::Fire,GuardAction::Bash,GuardAction::Reload}){auto g=scene([](auto&map){map.definition.playerStart={10.5f,6.5f};});known(g);g.setState(g.friendlyKey(0,"weapon_action"),int(action));tick(g,1);check(!g.state(g.friendlyKey(0,"drone_busy"))&&!g.state(g.friendlyKey(0,"drone_windup")),"A committed attack/action cannot be replaced by drone assembly in the same frame");}
 for(int rate:{60,120}){
  auto g=scene([](auto&){});check(g.deployEngineerDrone(0),"An Engineer spends finite stock to deploy an actual owned physical drone");if(g.engineerDrones().empty())continue;
  int slot=g.engineerDrones()[0].slot;g.damageEngineerDrone(slot,20);tick(g,rate/2,rate);check(g.engineerDrones()[0].health>40&&g.engineerDrones()[0].health<EngineerDroneHealth&&g.engineerDrones()[0].repair>0,"Nearby Engineer visibly repairs damaged drone health over time");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded&&restored.engineerDrones().size()==1,"Save restores a live drone's body, ownership, repair phase and finite resources");tick(g,rate*2,rate);if(decoded)tick(restored,rate*2,rate);
  check(g.engineerDrones()[0].health==EngineerDroneHealth&&g.state(g.friendlyKey(0,"drone_repair_charge"))==60,"Repair heals only the missing health and spends one saved charge per health point");
  check(decoded&&restored.engineerDrones()[0].health==g.engineerDrones()[0].health&&restored.state(restored.friendlyKey(0,"drone_repair_charge"))==g.state(g.friendlyKey(0,"drone_repair_charge")),"Saved and uninterrupted repair have the same health and resource outcome");
  g.damageEngineerDrone(slot,50);g.setState(g.friendlyKey(0,"drone_repair_charge"),3);tick(g,rate,rate);check(g.engineerDrones()[0].health==13&&g.state(g.friendlyKey(0,"drone_repair_charge"))==0,"An exhausted maintenance reservoir cannot regenerate more health");
  auto pose=g.engineerDrones()[0];Point3 from=pose.position+Point3{3,0,.6f};auto hit=g.traceShot(from,{-1,0,0},4,-1,true);check(hit.kind==ShotKind::Drone&&hit.index==slot,"Shared ballistic first-hit tracing sees the deployed drone's real body");
  g.resolveShot(hit,from,{-1,0,0},weaponDef(WeaponId::Shotgun).hitscan,true,30,{ShotKind::Player,g.level(),0});check(g.engineerDrones().empty(),"A traced player hit damages and destroys the physical drone through shared combat");
  bool replacements=true;for(int used=1;used<EngineerDroneCapacity;++used){replacements&=g.deployEngineerDrone(0);for(const auto&drone:g.engineerDrones())g.damageEngineerDrone(drone.slot,100);}
  check(replacements&&!g.deployEngineerDrone(0)&&g.state(g.friendlyKey(0,"drone_used"))==EngineerDroneCapacity,"Destroyed drones never replenish the Engineer's finite five-unit deployment stock");
 }
 {auto g=scene([](auto&){});g.deployEngineerDrone(0);g.damageEngineerDrone(0,10);g.m_friendlyActors[0].pose.clip=1;tick(g,30);check(g.engineerDrones()[0].health==50&&!g.state(g.friendlyKey(0,"drone_busy")),"A walking Engineer cannot perform physical maintenance while moving");g.m_friendlyActors[0].pose.clip=0;tick(g,1);check(g.state(g.friendlyKey(0,"drone_busy"))==1,"Stationary maintenance claims busy before its first repair pulse");g.setState(g.friendlyKey(0,"weapon_recovery"),200);tick(g,1);check(!g.state(g.friendlyKey(0,"drone_busy")),"Incompatible weapon recovery interrupts active maintenance immediately");}
 {auto g=scene([](auto&map){Door door{};door.left=0;door.right=24;door.y=6;door.open=1;door.opening=true;map.doors.push_back(door);});check(g.deployEngineerDrone(0),"Repair obstruction fixture starts with an open physical door");if(!g.engineerDrones().empty()){int slot=g.engineerDrones()[0].slot;g.damageEngineerDrone(slot,20);g.m_world.setDoor(0,0,false);tick(g,120);check(g.engineerDrones()[0].health==40&&g.state(g.friendlyKey(0,"drone_repair_charge"))==80,"A closed door blocks remote repair without consuming maintenance resources");}}
 for(bool bystander:{false,true}){auto g=scene([&](auto&map){map.definition.playerStart={10.5f,6.5f};if(bystander){auto other=map.actorTracks[0];other.ai={};other.ai.faction=AiFaction::Neutral;other.health=0;other.damageState=stateId("drone_bystander_damage");other.deadState=stateId("drone_bystander_dead");for(auto&key:other.keys)key.position={8.5f,5.925f};map.actorTracks.push_back(other);}else map.structures.push_back({8,0,8.02f,24,0,3});});g.deployEngineerDrone(0);g.setState(g.friendlyKey(0,"drone_used"),EngineerDroneCapacity);known(g);tick(g,240);
  check(g.engineerDrones().size()==1&&g.player().health==100&&g.state(droneTestKey(0,"ammo"))==24&&g.state("drone_bystander_damage")==0,"Drone cannon refuses every blocked first hit through solid cover or a neutral living body");}
 {auto g=scene([](auto&map){map.definition.playerStart={10.5f,6.5f};});g.deployEngineerDrone(0);known(g);tick(g,90);check(g.player().health<100&&g.state(droneTestKey(0,"ammo"))<24,"Personally visible hostile receives delayed drone cannon fire which spends saved ammunition");float health=g.player().health;g.m_player.pos={18.5f,18.5f};tick(g,240);check(g.player().health==health&&g.state(droneTestKey(0,"aim"))==0,"Hidden target movement cancels cannon aim and cannot be followed through last-known knowledge");}
 for(bool death:{false,true}){auto g=scene([](auto&){});g.deployEngineerDrone(0);if(death)g.damageFriendly(0,100,g.player().pos,{ShotKind::Player,0,0});else g.setState(stateId("drone_enabled"),0);tick(g,1);check(g.engineerDrones().empty(),"Dead or disabled Engineer safely releases its deployed drone and residency");}
 {auto g=scene([](auto&map){auto prototype=map.actorTracks[0];map.actorTracks.clear();const Vec2 places[]={{5.5f,5.5f},{17.5f,5.5f},{5.5f,17.5f},{17.5f,17.5f},{11.5f,11.5f}};
   for(int owner=0;owner<5;++owner){auto engineer=prototype;Vec2 p=places[owner];for(auto&key:engineer.keys)key.position=p;engineer.ai.patrol={{p,0,1000}};engineer.damageState=stateId("drone_fleet_damage/"+std::to_string(owner));engineer.deadState=stateId("drone_fleet_dead/"+std::to_string(owner));map.actorTracks.push_back(engineer);}});
  bool bounded=true;for(size_t owner=0;owner<4;++owner)for(int drone=0;drone<EngineerDroneCapacity;++drone)bounded&=g.deployEngineerDrone(owner);
  check(bounded&&!g.deployEngineerDrone(4)&&g.engineerDrones().size()==EngineerDroneSlots&&EngineerDroneSlots==20,"Four five-unit Engineer fleets fill the shared twenty-body pool and prevent overflow by a fifth owner");
  Game restored(g.m_customCampaign);check(restored.decodeSave(g.encodeSave())&&restored.engineerDrones().size()==EngineerDroneSlots,"Save accepts a full bounded pool containing several valid drones per Engineer");
 }
 std::vector<Point3> motion;
 for(int rate:{60,120}){auto base=scene([](auto&map){map.openEast=true;for(auto&key:map.actorTracks[0].keys)key.position={22.5f,6.5f};map.actorTracks[0].ai.patrol={{{22.5f,6.5f},0,1000}};});
  auto campaign=std::make_shared<CustomCampaign>(*base.m_customCampaign);auto next=std::make_shared<AuthoredMapData>(*campaign->maps[0]);next->definition.origin={24,0};next->definition.residencyGroup=43;next->openEast=false;next->openWest=true;next->actorTracks.clear();next->timedSequences.clear();campaign->maps.push_back(next);Game g(campaign);g.setState(stateId("drone_enabled"),1);g.ensureChunk(1);bool deployed=g.deployEngineerDrone(0);
  if(deployed){g.m_friendlyActors[0].level=1;g.m_friendlyActors[0].pose.position={6.5f,6.5f};tick(g,rate*8,rate);}
  check(deployed&&!g.engineerDrones().empty()&&g.engineerDrones()[0].level==1&&g.engineerDrones()[0].position.x>3,"Supervised drone physically follows its Engineer through a stitched seam at 60/120 Hz");
 }
 {auto g=scene([](auto&map){map.definition.playerStart={10.5f,6.5f};map.actorTracks[0].ai.attackDamage=1;});bool deployed=false;
  for(int frame=0;frame<360&&!g.dead();++frame){tick(g,1,60,true);if(!g.engineerDrones().empty()){deployed=true;break;}}
  check(deployed&&g.state(g.friendlyKey(0,"drone_used"))==1,"Ordinary gameplay perception deploys a finite supervised drone without diagnostic state injection");
  if(deployed){auto d=g.engineerDrones()[0];auto off=g.chunkOffset(d.level)-g.chunkOffset(g.level());Vec2 body{d.position.x+off.x,d.position.y+off.y};float hp=d.health;g.explodeTacticalGrenade(g.level(),{body.x,body.y,d.position.z+.5f},{ShotKind::Player,g.level(),0});check(g.engineerDrones().empty()||g.engineerDrones()[0].health<hp,"Shared occluded tactical blast damages the physical drone body");}
 }
 for(int rate:{60,120}){auto g=scene([](auto&){});g.deployEngineerDrone(0);auto&npc=g.m_friendlyActors[0];npc.pose.position={12.5f,6.5f};Vec2 start{g.engineerDrones()[0].position.x,g.engineerDrones()[0].position.y};bool safe=true;for(int frame=0;frame<rate*2;++frame){auto before=g.engineerDrones()[0].position;tick(g,1,rate);auto after=g.engineerDrones()[0].position;safe&=norm3(after-before)<=1.6f/rate+.003f&&std::fabs(after.z)<.001f&&g.world().fits(after.x,after.y,after.z,EngineerDroneHeight);}auto pose=g.engineerDrones()[0];motion.push_back(pose.position);check(safe&&length(Vec2{pose.position.x,pose.position.y}-start)>2.8f&&pose.walking,"Supervised drone walks physically toward its Engineer with bounded speed and floor support");}
 if(motion.size()==2)check(norm3(motion[0]-motion[1])<.025f,"Saved movement fractions keep grounded drone travel consistent at 60/120 Hz");
 for(const char*field:{"active","owner","level","hp","ammo","x","z","aim","fraction","x_fraction","route_next"}){auto g=scene([](auto&){});g.deployEngineerDrone(0);g.setState(droneTestKey(0,field),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Corrupt saved drone body, identity, movement or ammunition is rejected safely");}
 for(const char*field:{"drone_used","drone_repair_charge","drone_windup","drone_fraction","drone_busy"}){auto g=scene([](auto&){});g.setState(g.friendlyKey(0,field),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Corrupt saved Engineer stock or maintenance cannot bypass finite resource limits");}
 auto fleetScene=[](auto&map){
  auto&engineer=map.actorTracks[0];engineer.ai.attackRange=12;engineer.ai.attackDamage=1;engineer.ai.patrol={{{6.5f,6.5f},0,60000}};
  // A durable real opponent lets ordinary perception, assembly and cannon
  // events operate for a full fleet without resetting health or AI clocks.
  auto target=engineer;target.ai={};target.ai.faction=AiFaction::Facility;target.health=5000;target.damageState=stateId("drone_durable_target_damage");target.deadState=stateId("drone_durable_target_dead");
  for(auto&key:target.keys)key.position={12.5f,6.5f};map.actorTracks.push_back(target);
 };
 auto spaced=[](Game&g){auto drones=g.engineerDrones();bool safe=true;for(size_t i=0;i<drones.size();++i){const auto&a=drones[i];safe&=a.owner==0&&std::fabs(a.position.z)<.001f&&
   g.friendlyWorld(a.level).fits(a.position.x,a.position.y,a.position.z,EngineerDroneHeight);
   for(size_t j=i+1;j<drones.size();++j){const auto&b=drones[j];auto off=g.chunkOffset(b.level)-g.chunkOffset(a.level);
    safe&=std::hypot(a.position.x-b.position.x-off.x,a.position.y-b.position.y-off.y)>=EngineerDroneRadius*2-.015f;}}
  return safe;
 };
 for(int rate:{60,120}){
  auto g=scene(fleetScene);float start=g.npcActors()[0].pose.position.x;bool bounded=true,geometry=true,partial=false;int frames=0;
  while(frames++<rate*16){tick(g,1,rate,true);bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;geometry&=spaced(g);
   if(g.state(g.friendlyKey(0,"drone_used"))>=2&&g.state(g.friendlyKey(0,"drone_windup"))>=300){partial=true;break;}}
  check(partial&&g.engineerDrones().size()>=2&&g.state(g.friendlyKey(0,"drone_used"))<EngineerDroneCapacity,"Ordinary gameplay creates several simultaneous drones and exposes the next terminal assembly interval at 60/120 Hz");
  check(g.npcWeaponPose(0).handgun&&g.npcActors()[0].pose.position.x<start-.5f,"Engineer uses its handgun and physically retreats toward its support stand-off after deploying the first drone");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded&&restored.engineerDrones().size()==g.engineerDrones().size()&&
   restored.state(restored.friendlyKey(0,"drone_used"))==g.state(g.friendlyKey(0,"drone_used"))&&restored.state(restored.friendlyKey(0,"drone_windup"))==g.state(g.friendlyKey(0,"drone_windup")),
   "Save restores a multi-drone owner and its finite stock during a later assembly instead of rejecting duplicate ownership");
  for(int frame=0;frame<rate*20;++frame){tick(g,1,rate,true);if(decoded)tick(restored,1,rate,true);bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;geometry&=spaced(g);}
  report<<"fleet Hz="<<rate<<" size="<<g.engineerDrones().size()<<" stock="<<g.state(g.friendlyKey(0,"drone_used"))<<" position="<<g.npcActors()[0].pose.position.x<<','<<g.npcActors()[0].pose.position.y<<'\n';
  check(g.engineerDrones().size()==EngineerDroneCapacity&&g.state(g.friendlyKey(0,"drone_used"))==EngineerDroneCapacity&&!g.deployEngineerDrone(0),"Gameplay fills one five-unit fleet and refuses a sixth deployment without replenishing spent stock");
  check(bounded&&geometry,"Multi-drone gameplay keeps distinct supported bodies and respects the shared navigation search budget");
  bool continuation=decoded&&restored.engineerDrones().size()==g.engineerDrones().size()&&restored.state(restored.friendlyKey(0,"drone_used"))==EngineerDroneCapacity;
  for(const auto&drone:g.engineerDrones()){int ammo=g.state(droneTestKey(drone.slot,"ammo")),savedAmmo=restored.state(droneTestKey(drone.slot,"ammo"));
   continuation&=restored.state(droneTestKey(drone.slot,"owner"))==1&&std::abs(ammo-savedAmmo)<=1&&
    g.state(droneTestKey(drone.slot,"hp"))==restored.state(droneTestKey(drone.slot,"hp"));}
  check(continuation,"Saved and uninterrupted fleet growth reach the same finite capacity and preserve body health and cannon ammunition within one attack event");
  for(const auto&drone:g.engineerDrones())g.damageEngineerDrone(drone.slot,100);tick(g,rate*8,rate,true);
  check(g.engineerDrones().empty()&&g.state(g.friendlyKey(0,"drone_used"))==EngineerDroneCapacity&&!g.deployEngineerDrone(0),"Destroying an exhausted five-unit fleet never fabricates a replacement or resets total stock");
 }
 {auto g=scene([](auto&){});bool deployed=true;for(int i=0;i<EngineerDroneCapacity;++i)deployed&=g.deployEngineerDrone(0);check(deployed&&g.engineerDrones().size()==5,"Direct deployment admits all five simultaneous units for one owner");
  for(int stock:{0,4,6}){g.setState(g.friendlyKey(0,"drone_used"),stock);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Saved active fleet count cannot exceed spent stock and spent stock cannot exceed five");}
  g.setState(g.friendlyKey(0,"drone_used"),EngineerDroneCapacity);
  for(const char*f:{"active","owner","level","hp","ammo","x","y","z","yaw","aim","cooldown","flash","repair","phase","walking","fraction","x_fraction","y_fraction","route_next","route_wait","way_x","way_y","repath","target_kind","target_level","target_index"})
   g.setState(droneTestKey(5,f),g.state(droneTestKey(0,f)));
  Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"A malformed sixth active drone attributed to one Engineer is rejected even when the shared twenty-unit pool has room");
 }
 for(int rate:{60,120}){auto g=scene([](auto&){});bool deployed=g.deployEngineerDrone(0)&&g.deployEngineerDrone(0);check(deployed,"Shared maintenance fixture creates two physical drones owned by the same Engineer");if(!deployed)continue;
  auto drones=g.engineerDrones();for(const auto&drone:drones)g.damageEngineerDrone(drone.slot,20);g.setState(g.friendlyKey(0,"drone_repair_ms"),99);
  int before=0;for(const auto&drone:drones)before+=g.state(droneTestKey(drone.slot,"hp"));tick(g,1,rate);int after=0,changed=0;
  for(const auto&drone:drones){int hp=g.state(droneTestKey(drone.slot,"hp"));after+=hp;changed+=hp>40;}
  check(after-before==1&&changed==1&&g.state(g.friendlyKey(0,"drone_repair_charge"))==79&&g.state(g.friendlyKey(0,"drone_repair_target"))>0,
   "One physical maintenance pulse selects one recipient and spends one point from the shared reservoir instead of multiplying work across the fleet");
  g.setState(g.friendlyKey(0,"guard_danger_ms"),600);g.setState(g.friendlyKey(0,"drone_repair_ms"),99);int stock=g.state(g.friendlyKey(0,"drone_repair_charge"));tick(g,1,rate);
  int interrupted=0;for(const auto&drone:drones)interrupted+=g.state(droneTestKey(drone.slot,"hp"));check(interrupted==after&&g.state(g.friendlyKey(0,"drone_repair_charge"))==stock&&!g.state(g.friendlyKey(0,"drone_busy")),
   "Active danger inhibits fleet repair without spending or duplicating the shared maintenance resource");
 }
 {auto g=scene(fleetScene);bool deployed=g.deployEngineerDrone(0);check(deployed,"Support maintenance fixture starts with one actual damaged drone");if(deployed){int slot=g.engineerDrones()[0].slot;
   g.setState(droneTestKey(slot,"x"),4000);g.setState(droneTestKey(slot,"y"),10000);g.damageEngineerDrone(slot,20);auto start=g.npcActors()[0].pose.position;float moved=0;bool supportSafe=true;
   for(int frame=0;frame<600;++frame){tick(g,1,60,true);auto p=g.npcActors()[0].pose.position;moved=std::max(moved,length(p-start));supportSafe&=g.world().fits(p.x,p.y,g.npcActors()[0].pose.z,g.npcActors()[0].track().scale);}
   check(moved>.4f&&supportSafe&&g.state(droneTestKey(slot,"hp"))>40&&g.state(g.friendlyKey(0,"drone_repair_charge"))<80,"Engineer physically approaches a safely placed wounded fleet member and performs finite maintenance while retaining a supported body");
  }
 }
 for(const char*f:{"drone_repair_target","guard_engineer_goal","guard_engineer_wait","guard_engineer_x","guard_engineer_y","guard_engineer_z"}){
  auto g=scene([](auto&){});g.setState(g.friendlyKey(0,f),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);
  check(!restored.decodeSave(g.encodeSave()),"Malformed saved fleet repair recipient or Engineer support goal is rejected safely");
 }
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
