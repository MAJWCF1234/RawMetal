#include "../../Game.h"
#include "../engineer/Drones.h"
namespace retro {
namespace {
StateId squadKey(AiFaction faction,StateId squad,std::string_view field="cursor"){std::string value="npc/squad/"+std::to_string(int(faction))+"/"+std::to_string(squad)+"/"+std::string(field);StateId hash=2166136261u;for(unsigned char c:value)hash=(hash^c)*16777619u;return hash|0x80000000u;}
// Caution's key is asked for every frame for every guard: remember it per squad.
StateId squadCautionKey(AiFaction faction,StateId squad){struct Slot{AiFaction faction=AiFaction::Unspecified;StateId squad=0,key=0;};static thread_local std::array<Slot,64> slots{};auto&slot=slots[(squad^uint32_t(faction)*0x9e3779b9u)&63u];if(!slot.key||slot.squad!=squad||slot.faction!=faction)slot={faction,squad,squadKey(faction,squad,"caution")};return slot.key;}
}
AiTarget Game::guardSquadReport(size_t actor)const{
 if(npcIncapacitated(actor))return {};
 const auto&npc=m_friendlyActors[actor];const auto&c=npc.track().ai;if(!c.squad)return {};auto faction=actorFaction(npc.track());AiTarget report;float best=1e9f;auto origin=chunkOffset(npc.level);
 for(size_t i=0;i<m_friendlyActors.size();++i){if(i==actor)continue;const auto&source=m_friendlyActors[i];const auto&t=source.track();
  if(npcIncapacitated(i)||t.ai.mode!=ActorAiMode::Guard||t.ai.squad!=c.squad||actorFaction(t)!=faction||!chunkResident(source.level)||!autonomousActor(t)||source.pose.clip==4||(t.deadState&&state(t.deadState))||guardState(i)!=GuardState::Alert||!state(friendlyKey(i,"guard_seen")))continue;
  // A sighting reaches the squad over the radio, which takes a moment.
  if(state(friendlyKey(i,"guard_radio"))<RadioCallMs)continue;
  auto delta=source.pose.position+chunkOffset(source.level)-origin-npc.pose.position;float height=source.pose.z-npc.pose.z,range=lengthSq(delta)+height*height;if(range>18*18||range>=best)continue;auto target=guardTarget(i);if(!guardCanSee(i,source.pose,target))continue;
  if(!factionsHostile(faction,target.faction))continue;target.position+=chunkOffset(source.level)-origin;report=target;best=range;
 }return report;
}
bool Game::guardFireSlot(size_t actor)const{return guardTaskAllows(actor,GuardTask::Fire)&&!state(friendlyKey(actor,"guard_danger_ms"))&&!state(friendlyKey(actor,"engineer_pad_stage"))&&(!m_friendlyActors[actor].track().ai.squad||state(friendlyKey(actor,"guard_slot"))!=0);}
int Game::guardCoveringPeer(size_t actor)const{
 if(actor>=m_friendlyActors.size()||npcIncapacitated(actor))return 0;const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();auto target=guardTarget(actor);
 bool anchor=t.ai.role==GuardSquadRole::Heavy||t.ai.role==GuardSquadRole::Sharpshooter||(t.ai.role==GuardSquadRole::Default&&guardSuppressionEnabled(t.ai)&&!guardFlankSide(actor));
 if(!anchor||!t.ai.squad||!t.ai.armed||!autonomousActor(t)||!chunkResident(npc.level)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||!target.valid()||!factionsHostile(actorFaction(t),target.faction)||!state(friendlyKey(actor,"guard_confirmed"))||!state(friendlyKey(actor,"guard_memory")))return 0;
 auto faction=actorFaction(t);auto origin=chunkOffset(npc.level);int chosen=0;float nearest=18.f*18.f;
 auto livingPeer=[&](size_t peer){const auto&other=m_friendlyActors[peer];const auto&c=other.track();return !npcIncapacitated(peer)&&c.ai.mode==ActorAiMode::Guard&&c.ai.squad==t.ai.squad&&actorFaction(c)==faction&&chunkResident(other.level)&&autonomousActor(c)&&other.pose.clip!=4&&(!c.deadState||!state(c.deadState));};
 for(size_t i=0;i<m_friendlyActors.size();++i){if(i==actor||!livingPeer(i))continue;const auto&other=m_friendlyActors[i];const auto&c=other.track();auto get=[&](const char*f){return state(friendlyKey(i,f));};
  auto delta=other.pose.position+chunkOffset(other.level)-origin-npc.pose.position;float height=other.pose.z-npc.pose.z,distance=lengthSq(delta)+height*height;if(distance>nearest||get("guard_danger_ms"))continue;
  auto selected=guardTarget(i);if(selected.valid()&&(selected.kind!=target.kind||selected.index!=target.index||(selected.kind!=ShotKind::Npc&&selected.level!=target.level)))continue;
  auto schedule=GuardSchedule(get("schedule"));auto task=GuardTask(get("task"));bool running=get("task_status")==int(GuardTaskStatus::Running),moving=running&&get("schedule_step")==1&&task==GuardTask::Move;
  bool reload=c.ai.armed&&guardTaskAllows(i,GuardTask::Reload)&&(get("weapon_reload")||get("weapon_reloading")||(running&&get("schedule_step")==2&&get("weapon_saved")&&!get("weapon_rounds")));
  bool retreat=moving&&schedule==GuardSchedule::Cover&&get("guard_cover_mode")==2&&get("guard_cover");
  bool treating=false;int patient=get("support_target")-1;
  if(c.ai.role==GuardSquadRole::Medic&&schedule==GuardSchedule::Medic&&running&&(moving||guardTaskAllows(i,GuardTask::Treat))&&patient>=0&&size_t(patient)<m_friendlyActors.size()&&size_t(patient)!=i&&get("task_subject")==patient+1&&!get("support_cooldown")&&get("support_used")<guardRoleProfile(c.ai.role).treatments&&livingPeer(size_t(patient))){const auto&p=m_friendlyActors[size_t(patient)];treating=p.track().health>0&&state(p.track().damageState)>=p.track().health*.5f;}
  bool deploying=c.ai.role==GuardSquadRole::Engineer&&guardTaskAllows(i,GuardTask::Deploy)&&get("drone_windup")>0&&get("drone_used")<EngineerDroneCapacity;
  bool repairing=c.ai.role==GuardSquadRole::Engineer&&guardTaskAllows(i,GuardTask::Repair)&&get("drone_busy")&&get("drone_repair_charge")>0&&get("drone_repair_target")>0&&get("drone_repair_target")==get("task_subject");
  bool padBusy=c.visual==ActorVisual::CleanupEngineer&&c.ai.role==GuardSquadRole::Engineer&&get("engineer_pad_stage")>0;
  bool withdrawing=false;int medic=get("task_subject")-1;
  if(moving&&schedule==GuardSchedule::Patient&&get("guard_patient_move")&&medic>=0&&size_t(medic)<m_friendlyActors.size()&&size_t(medic)!=i&&livingPeer(size_t(medic))){const auto&m=m_friendlyActors[size_t(medic)];auto mg=[&](const char*f){return state(friendlyKey(size_t(medic),f));};withdrawing=m.track().ai.role==GuardSquadRole::Medic&&mg("schedule")==int(GuardSchedule::Medic)&&mg("task_status")==int(GuardTaskStatus::Running)&&mg("support_target")==int(i)+1;}
  if(!reload&&!retreat&&!treating&&!deploying&&!repairing&&!padBusy&&!withdrawing)continue;chosen=int(i)+1;nearest=distance;
 }return chosen;
}
bool Game::guardSuppressionPoint(size_t actor,Point3& point)const{
 if(npcIncapacitated(actor))return false;
 const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto target=guardTarget(actor);
 if(!guardSuppressionEnabled(t.ai)||!t.ai.squad||!t.ai.armed||!autonomousActor(t)||!chunkResident(npc.level)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||!target.valid()||!factionsHostile(actorFaction(t),target.faction)||!get("guard_confirmed")||!get("guard_memory")||!get("weapon_target")||get("guard_seen")||get("guard_suppress_ms")<=0||guardFlankSide(actor)!=0||guardCanSee(actor,npc.pose,target))return false;
 bool flank=guardCoveringPeer(actor)!=0;auto faction=actorFaction(t);
 for(size_t i=0;i<m_friendlyActors.size();++i){if(i==actor)continue;const auto&other=m_friendlyActors[i];const auto&c=other.track();
  if(npcIncapacitated(i)||c.ai.mode!=ActorAiMode::Guard||!c.ai.armed||c.ai.squad!=t.ai.squad||actorFaction(c)!=faction||!chunkResident(other.level)||!autonomousActor(c)||other.pose.clip==4||(c.deadState&&state(c.deadState))||!guardFlankSide(i)||state(friendlyKey(i,"guard_flank_done")))continue;
  auto selected=guardTarget(i);if(selected.kind==target.kind&&selected.index==target.index&&selected.level==target.level){flank=true;break;}
 }
 if(!flank)return false;
 // Probe only remembered cover. Never consult the concealed body's current
 // coordinates for aim, nor shoot a character which enters the probe line.
 auto origin=chunkOffset(npc.level);Point3 remembered{get("weapon_x")*.001f-origin.x,get("weapon_y")*.001f-origin.y,get("weapon_z")*.001f};
 Point3 direction=remembered-Point3{npc.pose.position.x,npc.pose.position.y,npc.pose.z+t.scale*.7f};float horizontal=std::hypot(direction.x,direction.y);if(horizontal<.001f||horizontal>t.ai.attackRange)return false;
 Point3 side{-direction.y/horizontal,direction.x/horizontal,0};auto shift=chunkOffset(npc.level)-chunkOffset(m_level);
 // A small side probe supplies a reachable exposed edge of the obstruction.
 // All three are tightly bounded around the last observed ray; open space,
 // nearer barrels/shoulders and any intervening body fail closed.
 for(float offset:{.25f,-.25f,0.f}){auto aim=remembered+side*offset;auto probe=npcWeaponPoseAt(actor,npc.pose,aim,1);auto delta=aim-probe.muzzle;float range=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);auto hit=guardShot(actor,npc.pose,probe);if(hit.kind!=ShotKind::Solid||hit.distance<.35f||hit.distance>range+.1f)continue;
  auto local=hit.point-Point3{shift.x,shift.y,0}-hit.normal*.015f;auto weapon=npcWeaponPoseAt(actor,npc.pose,local,1);auto check=guardShot(actor,npc.pose,weapon);
  if(check.kind!=ShotKind::Solid||check.distance<.35f)continue;point=local;return true;
 }return false;
}
void Game::updateGuardSquadSlots(float dt){
 struct Group{StateId key;std::vector<size_t> actors;};std::vector<Group> groups;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(t.ai.mode!=ActorAiMode::Guard||!t.ai.squad)continue;
  if(npcIncapacitated(i)){setState(friendlyKey(i,"guard_slot"),0);setState(friendlyKey(i,"guard_slot_ms"),0);continue;}
  int lease=std::max(0,state(friendlyKey(i,"guard_slot_ms"))-npcMilliseconds(i,dt,"squad_fraction"));auto target=guardTarget(i);
  Point3 cover;bool suppression=guardSuppressionPoint(i,cover);
  bool ordinary=guardState(i)==GuardState::Alert&&guardCanSee(i,npc.pose,target)&&guardShotMatches(i,guardShot(i,npc.pose,npcWeaponPoseAt(i,npc.pose,{target.position.x,target.position.y,target.feet+target.height*.6f},1)));
  bool eligible=t.ai.armed&&chunkResident(npc.level)&&autonomousActor(t)&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState))&&npc.pose.clip!=1&&(!state(friendlyKey(i,"weapon_saved"))||state(friendlyKey(i,"weapon_rounds"))>0)&&!state(friendlyKey(i,"weapon_reload"))&&!state(friendlyKey(i,"weapon_reloading"))&&!state(friendlyKey(i,"weapon_recovery"))&&!state(friendlyKey(i,"grenade_windup"))&&!guardMeleeReady(i,npc.pose,target)&&(ordinary||suppression);
  eligible=eligible&&guardTaskAllows(i,GuardTask::Fire)&&!state(friendlyKey(i,"guard_danger_ms"))&&!state(friendlyKey(i,"drone_busy"))&&!state(friendlyKey(i,"engineer_pad_stage"))&&!state(friendlyKey(i,"support_target"))&&(!guardFlankSide(i)||state(friendlyKey(i,"guard_flank_done")));if(!eligible||!lease){lease=0;setState(friendlyKey(i,"guard_slot"),0);}setState(friendlyKey(i,"guard_slot_ms"),lease);if(!eligible)continue;
  auto key=squadKey(actorFaction(t),t.ai.squad);auto group=std::find_if(groups.begin(),groups.end(),[&](const Group&g){return g.key==key;});if(group==groups.end()){groups.push_back({key,{}});group=std::prev(groups.end());}group->actors.push_back(i);
 }
 for(const auto&group:groups){int occupied=0;for(auto i:group.actors)occupied+=state(friendlyKey(i,"guard_slot"))!=0;
  int cursor=std::clamp(state(group.key),0,int(m_friendlyActors.size()));bool covered=false;for(auto i:group.actors)covered|=state(friendlyKey(i,"guard_slot"))&&guardCoveringPeer(i)!=0;
  // Fill one free lease with a loaded covering anchor. Surviving leases keep
  // their finite aim interval, and the other opportunity retains fair rotation.
  if(occupied<2&&!covered){auto start=std::lower_bound(group.actors.begin(),group.actors.end(),size_t(cursor));size_t first=start==group.actors.end()?0:size_t(start-group.actors.begin());for(size_t pass=0;pass<group.actors.size();++pass){auto i=group.actors[(first+pass)%group.actors.size()];if(state(friendlyKey(i,"guard_slot"))||!guardCoveringPeer(i))continue;setState(friendlyKey(i,"guard_slot"),1);setState(friendlyKey(i,"guard_slot_ms"),std::max(1600,m_friendlyActors[i].track().ai.aimMs+500));++occupied;break;}}
  for(size_t pass=0;pass<group.actors.size()&&occupied<2;++pass){auto found=std::lower_bound(group.actors.begin(),group.actors.end(),size_t(cursor));if(found==group.actors.end())found=group.actors.begin();auto i=*found;cursor=int(i)+1;
   if(state(friendlyKey(i,"guard_slot")))continue;setState(friendlyKey(i,"guard_slot"),1);setState(friendlyKey(i,"guard_slot_ms"),std::max(1600,m_friendlyActors[i].track().ai.aimMs+500));++occupied;
  }setState(group.key,cursor);
 }
}
int Game::guardFlankSide(size_t actor)const{
 if(npcIncapacitated(actor))return 0;
 if(state(friendlyKey(actor,"schedule"))==int(GuardSchedule::Patient))return 0;
 const auto&npc=m_friendlyActors[actor];if(!npc.track().ai.squad)return 0;auto target=guardTarget(actor);if(!target.valid())return 0;
 if(npc.track().ai.role!=GuardSquadRole::Default){if(!guardRoleProfile(npc.track().ai.role).flanks)return 0;return npc.track().ai.role==GuardSquadRole::Rusher?1:-1;}
 int rank=0,count=0;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&other=m_friendlyActors[i];const auto&t=other.track();if(npcIncapacitated(i)||t.ai.mode!=ActorAiMode::Guard||!t.ai.armed||t.ai.squad!=npc.track().ai.squad||actorFaction(t)!=actorFaction(npc.track())||!autonomousActor(t)||other.pose.clip==4||(t.deadState&&state(t.deadState))||!chunkResident(other.level))continue;
  auto selected=guardTarget(i);if(selected.kind!=target.kind||selected.index!=target.index||selected.level!=target.level)continue;auto delta=other.pose.position+chunkOffset(other.level)-chunkOffset(npc.level)-npc.pose.position;if(lengthSq(delta)>18*18)continue;if(i<actor)++rank;++count;
 }return count>1?(rank%3==1?-1:rank%3==2?1:0):0;
}
bool Game::guardPositionReserved(size_t actor,Vec2 position,float feet)const{
 if(npcIncapacitated(actor))return false;
 const auto&npc=m_friendlyActors[actor];if(!npc.track().ai.squad)return false;auto origin=chunkOffset(npc.level);
 for(size_t i=0;i<m_friendlyActors.size();++i){if(i==actor)continue;const auto&other=m_friendlyActors[i];if(npcIncapacitated(i)||!chunkResident(other.level)||!autonomousActor(other.track())||other.pose.clip==4||(other.track().deadState&&state(other.track().deadState))||other.track().ai.squad!=npc.track().ai.squad||actorFaction(other.track())!=actorFaction(npc.track()))continue;
  auto current=other.pose.position+chunkOffset(other.level)-origin;if(std::fabs(feet-other.pose.z)<npc.track().scale&&lengthSq(current-position)<.7f*.7f)return true;
  if(state(friendlyKey(i,"guard_danger_ms"))&&state(friendlyKey(i,"guard_danger_goal"))){Vec2 escape{state(friendlyKey(i,"guard_danger_goal_x"))*.001f-origin.x,state(friendlyKey(i,"guard_danger_goal_y"))*.001f-origin.y};if(std::fabs(feet-state(friendlyKey(i,"guard_danger_goal_z"))*.001f)<npc.track().scale&&lengthSq(escape-position)<.7f*.7f)return true;}
  if(!state(friendlyKey(i,"guard_cover")))continue;
  Vec2 goal{state(friendlyKey(i,"guard_cover_x"))*.001f,state(friendlyKey(i,"guard_cover_y"))*.001f};goal+=chunkOffset(other.level)-origin;if(std::fabs(feet-state(friendlyKey(i,"guard_cover_z"))*.001f)<npc.track().scale&&lengthSq(goal-position)<.7f*.7f)return true;
 }return false;
}
// Caution keeps one clock per squad (a man with no squad keeps his own), so a
// squad spread over several chunks counts down together wherever the player is.
StateId Game::guardCautionKey(size_t actor)const{const auto&t=m_friendlyActors.at(actor).track();return t.ai.squad?squadCautionKey(actorFaction(t),t.ai.squad):friendlyKey(actor,"guard_caution");}
int Game::guardCaution(size_t actor)const{return actor<m_friendlyActors.size()&&m_friendlyActors[actor].track().ai.mode==ActorAiMode::Guard?state(guardCautionKey(actor)):0;}
bool Game::guardWary(size_t actor)const{return guardCaution(actor)||(actor<m_friendlyActors.size()&&m_friendlyActors[actor].track().ai.mode==ActorAiMode::Guard&&guardState(actor)==GuardState::Alert);}
void Game::startGuardCaution(size_t actor){if(actor<m_friendlyActors.size()&&m_friendlyActors[actor].track().ai.mode==ActorAiMode::Guard)setState(guardCautionKey(actor),CautionMs);}
void Game::updateGuardCaution(float dt){
 m_cautionMicros+=int(std::lround(double(dt)*1000000));int ms=m_cautionMicros/1000;m_cautionMicros%=1000;
 std::vector<std::pair<StateId,bool>> clocks;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(t.ai.mode!=ActorAiMode::Guard)continue;auto key=guardCautionKey(i);if(!state(key))continue;
  bool alert=!npcIncapacitated(i)&&(!t.ai.enableState||state(t.ai.enableState))&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState))&&guardState(i)==GuardState::Alert&&guardTarget(i).kind==ShotKind::Player;
  auto clock=std::find_if(clocks.begin(),clocks.end(),[&](const auto&c){return c.first==key;});if(clock==clocks.end())clocks.push_back({key,alert});else clock->second=clock->second||alert;
 }
 // Any man of the squad in Alert against the player ends it (a fight with a creature or another
 // faction lets it run on); the next search that gives up on the player starts it again.
 for(const auto&[key,alert]:clocks)setState(key,alert?0:std::max(0,state(key)-ms));
}
int Game::guardAlarm()const{
 int caution=0;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();
  if(t.ai.mode!=ActorAiMode::Guard||!chunkResident(npc.level)||npcIncapacitated(i)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||(t.ai.enableState&&!state(t.ai.enableState)))continue;
  // ALERT is a man in Alert with the player as his target, here or over a seam; a fight with a wasp is not.
  if(guardState(i)==GuardState::Alert&&guardTarget(i).kind==ShotKind::Player)return -1;
  // CAUTION is a squad hostile to the player with a man in this chunk.
  if(npc.level==m_level&&factionsHostile(actorFaction(t),AiFaction::Player))caution=std::max(caution,state(guardCautionKey(i)));
 }return caution;
}
}
