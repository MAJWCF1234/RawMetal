#include "../../Game.h"
#include "../Perception.h"
#include "ScheduleTables.h"
#include "../DecisionClock.h"
#include <fstream>
#include "../../../world/CustomCampaign.h"
namespace retro {
int Game::npcMilliseconds(size_t actor,float dt,std::string_view field){auto key=friendlyKey(actor,field);int micros=state(key)+int(std::round(double(dt)*1000000));setState(key,micros%1000);return micros/1000;}
void Game::validateGuardState(size_t actor)const{
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};const auto&config=m_friendlyActors[actor].track().ai;
 for(const char*f:{"sense_ready","plan_ready","tactic_ready","room_ready","sense_due","plan_due"})if(get(f)<0||get(f)>1)throw std::runtime_error("Invalid saved AI decision flag");
 for(const char*f:{"sense_wait","plan_wait","sense_age","plan_age","tactic_age"})if(get(f)<0||get(f)>1000)throw std::runtime_error("Invalid saved AI decision clock");
 if(get("evac_level")<0||get("evac_level")>chunkCount()||get("evac_x")<0||get("evac_x")>24000||get("evac_y")<0||get("evac_y")>24000||(get("evac_z")< -100000||get("evac_z")>100000)||get("evacuated")<0||get("evacuated")>1||get("killed_by_player")<0||get("killed_by_player")>1)throw std::runtime_error("Invalid saved NPC evacuation");
 if(get("guard_fraction")<0||get("guard_fraction")>=1000||get("weapon_fraction")<0||get("weapon_fraction")>=1000)throw std::runtime_error("Invalid saved guard clock fraction");
 if(get("guard_step")<0||get("guard_step")>=1000||get("guard_bearing")< -31416||get("guard_bearing")>31416)throw std::runtime_error("Invalid saved guard motion");
 if(get("guard_cover")<0||get("guard_cover")>1||get("guard_cover_wait")<0||get("guard_cover_wait")>1000||get("guard_cover_x")<0||get("guard_cover_x")>24000||get("guard_cover_y")<0||get("guard_cover_y")>24000||get("guard_cover_z")< -100000||get("guard_cover_z")>100000)throw std::runtime_error("Invalid saved guard repositioning");
 if(get("guard_cover")&&(get("guard_cover_x")<350||get("guard_cover_x")>23650||get("guard_cover_y")<350||get("guard_cover_y")>23650))throw std::runtime_error("Invalid saved guard cover goal");
 if(get("guard_radio")<0||get("guard_radio")>2000)throw std::runtime_error("Invalid saved guard radio call");
 if(get("guard_confirmed")<0||get("guard_confirmed")>1||get("guard_seen")<0||get("guard_seen")>1||get("guard_under_fire")<0||get("guard_under_fire")>1500)throw std::runtime_error("Invalid saved guard threat memory");
 if(get("guard_heard")<0||get("guard_heard")>15||get("guard_hurt_amount")<0||get("guard_hurt_amount")>m_friendlyActors[actor].track().health||get("guard_enemy_dead")<0||get("guard_enemy_dead")>1)throw std::runtime_error("Invalid saved guard sensory conditions");
 if(get("guard_patrol_wait")<0||get("guard_patrol_wait")>5000||get("guard_patrol_progress")<0||get("guard_patrol_progress")>=100||get("guard_patrol_point_seen")<0||get("guard_patrol_point_seen")>int(config.patrol.size())||get("guard_patrol_fraction")<0||get("guard_patrol_fraction")>=1000||get("guard_patrol_movement_fraction")<0||get("guard_patrol_movement_fraction")>=1000)throw std::runtime_error("Invalid saved guard patrol recovery");
 if(get("guard_route_target")<0||get("guard_route_target")>chunkCount()||get("guard_route_next")<0||get("guard_route_next")>chunkCount()||get("guard_route_wait")<0||get("guard_route_wait")>1000||get("guard_portal_z")< -100000||get("guard_portal_z")>100000)throw std::runtime_error("Invalid saved guard pursuit route");
 if(get("guard_route_next")){float x=get("guard_portal_x")*.001f,y=get("guard_portal_y")*.001f;bool inside=false;for(int level=0;level<chunkCount();++level){auto origin=chunkOffset(level);inside|=x>=origin.x-.36f&&x<=origin.x+24.36f&&y>=origin.y-.36f&&y<=origin.y+24.36f;}if(!inside)throw std::runtime_error("Invalid saved guard pursuit portal");}
 if(get("weapon_aim")<0||get("weapon_aim")>config.aimMs||get("weapon_recovery")<0||get("weapon_recovery")>std::max(config.recoveryMs,900)||get("weapon_reload")<0||get("weapon_reload")>1800||get("weapon_rounds")<0||get("weapon_rounds")>weaponDef(guardHandgun(config)?WeaponId::Handgun:WeaponId::Shotgun).cycle.tubeCapacity||get("weapon_raise")<0||get("weapon_raise")>1000||get("weapon_flash")<0||get("weapon_flash")>80||get("weapon_recoil")<0||get("weapon_recoil")>220)throw std::runtime_error("Invalid saved guard weapon state");
 if(get("guard_mode")<0||get("guard_mode")>4||get("guard_point")<0||get("guard_point")>=int(config.patrol.size())||get("guard_pause")<0||get("guard_pause")>60000||get("guard_recognition")<0||get("guard_recognition")>config.recognitionMs||get("guard_search")<0||get("guard_search")>config.searchMs||get("guard_travel")<0||get("guard_travel")>8000||get("guard_z")< -100000||get("guard_z")>100000)throw std::runtime_error("Invalid saved guard awareness");
 validateGuardPolicyState(actor);
 validateGuardDanger(actor);
 validateGuardSchedule(actor);
}
void Game::updateGuardFlags(){
 std::vector<StateValue> flags;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();if(track.ai.mode!=ActorAiMode::Guard||!track.ai.alertState)continue;
  auto it=std::lower_bound(flags.begin(),flags.end(),track.ai.alertState,[](const auto&v,StateId id){return v.id<id;});if(it==flags.end()||it->id!=track.ai.alertState)it=flags.insert(it,{track.ai.alertState,0});
  if(!npcIncapacitated(i)&&(!track.ai.enableState||state(track.ai.enableState))&&npc.pose.clip!=4&&(!track.deadState||!state(track.deadState))&&guardState(i)==GuardState::Alert)it->value=1;
 }for(const auto&flag:flags)setState(flag.id,flag.value);
}
void Game::guardIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<AiStimulus>& stimuli,Vec2& goal,float& goalZ,bool& wantsMove){
 if(npcIncapacitated(actor)){wantsMove=false;return;}
 const auto&config=track.ai;auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*field){return ctx.get(field);};auto put=[&](const char*field,int value){ctx.put(field,value);};
 auto mode=GuardState(get("guard_mode"));int ms=npcMilliseconds(actor,dt,"guard_fraction"),recognition=get("guard_recognition");bool confirmed=get("guard_confirmed")!=0;
 auto shift=chunkOffset(level)-chunkOffset(m_level),globalOrigin=chunkOffset(level);auto observer=pose.position+shift;
 float eye=pose.z+friendlyHullHeight(actor,track)*.85f;auto previous=guardTarget(actor);
 bool freshHit=(track.damageState&&state(track.damageState)>get("guard_damage"))||get("hit_source");
 Vec2 playerLocal=m_player.pos+chunkOffset(m_level)-globalOrigin;
 bool nearThreat=(factionsHostile(actorFaction(track),AiFaction::Player)&&lengthSq(playerLocal-pose.position)<3.f*3.f&&std::fabs(m_player.z-pose.z)<track.scale)||(previous.valid()&&lengthSq(previous.position-pose.position)<3.f*3.f);
 if(!nearThreat)for(size_t i=0;i<m_friendlyActors.size();++i){const auto&other=m_friendlyActors[i];const auto&t=other.track();
  if(i==actor||!t.health||npcIncapacitated(i)||(t.deadState&&state(t.deadState))||!factionsHostile(actorFaction(track),actorFaction(t)))continue;
  if(lengthSq(other.pose.position+chunkOffset(other.level)-globalOrigin-pose.position)<9.f&&std::fabs(other.pose.z-pose.z)<track.scale){nearThreat=true;break;}
 }
 if(!nearThreat&&factionsHostile(actorFaction(track),AiFaction::Creature))for(int other=0;other<chunkCount()&&!nearThreat;++other)if(chunkResident(other)){
  const auto&enemies=other==m_level?m_enemies:m_chunks[other].enemies;auto offset=chunkOffset(other)-globalOrigin;
  for(const auto&enemy:enemies)if(enemy.alive&&lengthSq(enemy.pos+offset-pose.position)<9.f&&std::fabs(enemy.bodyBottom()-pose.z)<track.scale){nearThreat=true;break;}
 }
 bool noiseWake=false;
 for(const auto&noise:stimuli){
  if(noise.npc==int(actor)||!(noise.radius>0)||!(noise.gain>0))continue;
  if(noise.npc>=0&&size_t(noise.npc)<m_friendlyActors.size()&&(noise.categories&ai::SoundWeapon)&&actorFaction(m_friendlyActors[size_t(noise.npc)].track())==actorFaction(track))continue;
  float dz=noise.z-eye;if(lengthSq(noise.source-observer)+dz*dz<noise.radius*noise.radius){noiseWake=true;break;}
 }
 auto clock=[&](const char*wait,const char*age,const char*ready,int period,bool wake,int lane){
  ai::DecisionClock::State state{get(wait),get(age),get(ready)};
  auto step=ai::DecisionClock::advance(state,ms,period,int((actor*37+size_t(level)*11)%size_t(period)),wake,lane);
  put(wait,state.remaining);put(age,state.elapsed);put(ready,state.ready);return step.due;
 };
 bool senseDue=!m_aiDecisionScheduling||clock("sense_wait","sense_age","sense_ready",mode==GuardState::Patrol?100:50,freshHit||noiseWake||nearThreat||(!previous.valid()&&get("guard_target_kind")),0);
 put("sense_due",senseDue);
 // Selecting among all possible threats is scheduled. Tracking the current
 // reference and its live LOS remain immediate, including losing it to cover.
 auto target=senseDue?guardThreat(actor,pose):(previous.valid()&&guardCanSee(actor,pose,previous)?resolveGuardTarget(actor,{previous.kind,previous.level,previous.index}):AiTarget{});bool visible=target.valid();
 // A missing live target is not proof of death: streaming, disabling and
 // occlusion can all invalidate its reference. Confirm only a known corpse
 // which this guard can currently see in resident geometry.
 auto corpseSeen=[&](){
  if(!get("guard_memory")||!get("guard_target_kind"))return false;
  auto kind=ShotKind(get("guard_target_kind"));int targetLevel=get("guard_target_level")-1,index=get("guard_target_index")-1;
  Vec2 position{};float feet=0,corpseEye=.25f;
  if(kind==ShotKind::Player){if(!dead())return false;targetLevel=m_level;position=m_player.pos;feet=m_player.z;}
  else if(kind==ShotKind::Npc){
   if(index<0||size_t(index)>=m_friendlyActors.size()||size_t(index)==actor)return false;
   const auto&other=m_friendlyActors[size_t(index)];const auto&t=other.track();
   // Knocked-out and dead squadmates both count as bodies once they are down.
   if(!(npcIncapacitated(size_t(index))||(t.deadState&&state(t.deadState))||other.pose.clip==4))return false;
   targetLevel=other.level;position=other.pose.position;feet=other.pose.z;corpseEye=t.scale*.25f;
  }
  else if(kind==ShotKind::Enemy){
   if(targetLevel<0||targetLevel>=chunkCount()||!chunkResident(targetLevel))return false;
   const auto&enemies=targetLevel==m_level?m_enemies:m_chunks[targetLevel].enemies;
   if(index<0||size_t(index)>=enemies.size())return false;const auto&enemy=enemies[size_t(index)];
   if(enemy.alive||!enemy.visible())return false;position=enemy.pos;feet=enemy.z;
  }
  else if(kind==ShotKind::Actor){
   if(targetLevel<0||targetLevel>=chunkCount()||!chunkResident(targetLevel))return false;
   const auto&world=targetLevel==m_level?m_world:m_chunks[targetLevel].world;
   if(index<0||size_t(index)>=world.actorTracks().size())return false;
   const auto&t=world.actorTracks()[size_t(index)];const auto&p=world.actorPose(size_t(index));
   if(t.ai.mode!=ActorAiMode::Scripted||state(actorAiState(targetLevel,size_t(index),t.timerState,"restraint"))||!t.health||t.platform||!(p.clip==4||(t.deadState&&state(t.deadState))))return false;
   position=p.position;feet=p.z;corpseEye=t.scale*.25f;
  }else return false;
  if(targetLevel<0||targetLevel>=chunkCount()||!chunkResident(targetLevel))return false;
  position+=chunkOffset(targetLevel)-globalOrigin;auto delta=position-pose.position;float dz=feet+corpseEye-eye;
  return lengthSq(delta)+dz*dz<config.sightRange*config.sightRange&&dot(normalized(delta),Vec2{std::sin(pose.yaw),std::cos(pose.yaw)})>config.sightDot&&soundRayClear(observer,eye,position+shift,feet+corpseEye);
 };
 bool enemyDead=senseDue&&corpseSeen();int deadKind=get("guard_target_kind"),deadLevel=get("guard_target_level"),deadIndex=get("guard_target_index");put("guard_enemy_dead",enemyDead);
 auto schedule=GuardSchedule(get("schedule"));if(schedule==GuardSchedule::None)schedule=GuardSchedule::Patrol;
 const auto soundMask=guardScheduleDefinition(schedule).schedule.soundMask;
 const AiStimulus*heard=nullptr;float strongest=0;unsigned audible=0;
 for(const auto&noise:stimuli){
  if(!std::isfinite(noise.radius)||noise.radius<=0||!std::isfinite(noise.gain)||noise.gain<=0||!std::isfinite(noise.source.x)||!std::isfinite(noise.source.y)||!std::isfinite(noise.z))continue;
  float height=noise.z-eye;if(lengthSq(noise.source-observer)+height*height>=noise.radius*noise.radius||noise.npc==int(actor))continue;
  if(noise.npc>=0&&size_t(noise.npc)<m_friendlyActors.size()&&(noise.categories&ai::SoundWeapon)&&actorFaction(m_friendlyActors[size_t(noise.npc)].track())==actorFaction(track))continue;
  float strength=aiSoundStrength(observer,eye,noise.source,noise.z,noise.radius,noise.gain,noise.loud,soundRayClear(observer,eye,noise.source,noise.z));
  if(strength<=0)continue;audible|=noise.categories&15u;
  if((noise.categories&soundMask)&&strength>strongest){strongest=strength;heard=&noise;}
 }
 put("guard_heard",int(audible));
 auto sameRef=[](const AiTarget&a,const AiTarget&b){return a.valid()&&b.valid()&&a.kind==b.kind&&a.level==b.level&&a.index==b.index;};
 auto remember=[&](Vec2 global,float feet){put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(feet*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);};
 // Fallen colleagues are not loot — seeing one confirms a hostile was here and
 // bars silent rear takedowns until the detail clears its memory.
 bool casualty=false;
 if(senseDue&&mode!=GuardState::Alert){
  Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)};
  for(size_t i=0;i<m_friendlyActors.size();++i){
   if(i==actor)continue;const auto&buddy=m_friendlyActors[i];const auto&bt=buddy.track();
   if(bt.ai.mode!=ActorAiMode::Guard||actorFaction(bt)!=actorFaction(track)||!chunkResident(buddy.level))continue;
   if(config.squad&&bt.ai.squad&&bt.ai.squad!=config.squad)continue;
   bool down=npcIncapacitated(i)||buddy.pose.clip==4||(bt.deadState&&state(bt.deadState));if(!down)continue;
   auto bodyLocal=buddy.pose.position+chunkOffset(buddy.level)-globalOrigin;auto delta=bodyLocal-pose.position;
   float bodyEye=buddy.pose.z+bt.scale*.25f,dz=bodyEye-eye;
   if(lengthSq(delta)+dz*dz>=config.sightRange*config.sightRange||dot(normalized(delta),facing)<=config.sightDot)continue;
   auto worldBody=buddy.pose.position+chunkOffset(buddy.level)-chunkOffset(m_level);
   if(!soundRayClear(observer,eye,worldBody,bodyEye))continue;
   remember(buddy.pose.position+chunkOffset(buddy.level),buddy.pose.z);
   AiTarget blame{ShotKind::Player,m_level,0};blame.position=m_player.pos+chunkOffset(m_level)-globalOrigin;blame.feet=m_player.z;blame.height=m_player.hullHeight();blame.eye=m_player.eye;blame.faction=AiFaction::Player;
   rememberGuardTarget(actor,blame);previous=blame;confirmed=true;recognition=config.recognitionMs;casualty=true;
   mode=visible?GuardState::Alert:GuardState::Investigate;break;
  }
 }
 if(visible){
  // Recognition belongs to an entity reference, not merely a recent alert.
  // Brief occlusion preserves a confirmed foe; a new foe starts fresh.
  // A just-found body already blamed the player — keep that confirm.
  if(!sameRef(previous,target)&&!casualty){confirmed=false;recognition=0;}
  rememberGuardTarget(actor,target);remember(target.position+globalOrigin,target.feet);
  // How fast he makes the player out. Within 5 m nothing saves them; beyond
  // it distance, a crouch, keeping still, the edge of his vision and shadow
  // all buy time (at the end of his sight, three times as long before the rest).
  float rate=1.f;
  if(target.kind==ShotKind::Player&&!confirmed&&!casualty){
   auto toward=target.position-pose.position;float dz=target.feet+target.eye-eye,distance=std::sqrt(lengthSq(toward)+dz*dz);
   if(distance>5.f){rate=1.f-.7f*std::clamp((distance-5.f)/std::max(1.f,config.sightRange-5.f),0.f,1.f);
    if(m_player.crouched)rate*=.6f;
    float speed=length(m_velocity);if(speed<.3f)rate*=.7f;else if(speed>4.2f)rate*=1.4f;
    if(dot(normalized(toward),Vec2{std::sin(pose.yaw),std::cos(pose.yaw)})<(config.sightDot+1.f)*.5f)rate*=.5f;
    rate*=.35f+.65f*std::min(1.f,m_playerExposure*1.6f);
   }
  }
  int gain=ms>0?std::max(1,int(std::lround(ms*rate))):0;
  recognition=confirmed||casualty?config.recognitionMs:std::min(config.recognitionMs,recognition+gain);confirmed=recognition>=config.recognitionMs||casualty;mode=confirmed?GuardState::Alert:GuardState::Suspicious;
 }
 else{
  const int glimpsed=recognition;
  recognition=confirmed?config.recognitionMs:std::max(0,recognition-ms/2);
  // Half made out: he walks over to where he saw it, still unsure (and still
  // open to a takedown). Less than that and he shrugs, but the doubt fades
  // slowly, so a second look soon after finishes the job faster.
  if(mode==GuardState::Suspicious&&!confirmed){
   if(glimpsed*2>=config.recognitionMs){mode=GuardState::Investigate;put("guard_travel",0);}
   else{mode=GuardState::Patrol;put("guard_memory",0);rememberGuardTarget(actor,{});}
  }
  else if(mode==GuardState::Alert){mode=GuardState::Investigate;put("guard_travel",0);}
  bool supporting=schedule==GuardSchedule::Medic||schedule==GuardSchedule::Engineer||schedule==GuardSchedule::Patient;
  if(heard&&!supporting&&!enemyDead){auto local=heard->source;const auto&world=worldAt(local);remember(heard->source+chunkOffset(m_level),world.supportBelow(local.x,local.y,heard->z));mode=GuardState::Investigate;}
  auto report=guardSquadReport(actor);auto remembered=Vec2{get("guard_x")*.001f-globalOrigin.x,get("guard_y")*.001f-globalOrigin.y};
  if(report.valid()&&(mode==GuardState::Patrol||mode==GuardState::Suspicious||!sameRef(previous,report)||lengthSq(report.position-remembered)>.25f*.25f||std::fabs(report.feet-get("guard_z")*.001f)>.3f)){
   // A teammate's sighting supplies last-known knowledge, never our own sight.
   rememberGuardTarget(actor,report);remember(report.position+globalOrigin,report.feet);confirmed=true;recognition=config.recognitionMs;mode=GuardState::Investigate;
  }
 }
 int damage=track.damageState?state(track.damageState):0;int freshDamage=std::clamp(damage-get("guard_damage"),0,std::max(0,track.health));put("guard_hurt_amount",freshDamage);put("guard_under_fire",std::max(0,get("guard_under_fire")-ms));
 if(damage>get("guard_damage")){
  put("guard_under_fire",1500);auto attacker=guardAttacker(actor);
  // Fire from a squadmate or an ally of its own faction (a stray grenade, a
  // crossfire) is investigated, not answered; anyone else who hurts it is.
  if(attacker.valid()&&attacker.kind==ShotKind::Npc&&size_t(attacker.index)<m_friendlyActors.size()){const auto&other=m_friendlyActors[size_t(attacker.index)].track();auto faction=actorFaction(track);
   bool squadmate=track.ai.squad&&other.ai.squad==track.ai.squad,ally=attacker.faction==faction&&faction!=AiFaction::Neutral&&faction!=AiFaction::Unspecified;
   if(squadmate||ally)attacker={};}
  if(attacker.valid()){target=attacker;rememberGuardTarget(actor,attacker);remember(attacker.position+globalOrigin,attacker.feet);confirmed=true;recognition=config.recognitionMs;visible=guardCanSee(actor,pose,attacker);mode=visible?GuardState::Alert:GuardState::Investigate;}
  else if(get("hit_source")){Vec2 hit{get("hit_x")*.001f,get("hit_y")*.001f};remember(hit+globalOrigin,friendlyWorld(level).supportBelow(hit.x,hit.y,pose.z+track.scale));mode=GuardState::Investigate;}
 }
 if(enemyDead&&!visible&&get("guard_target_kind")==deadKind&&get("guard_target_level")==deadLevel&&get("guard_target_index")==deadIndex){
  // The corpse ends this reference's pursuit. Searching is bounded by the
  // authored awareness deadline and cannot keep chasing its tomb location.
  rememberGuardTarget(actor,{});put("guard_memory",0);put("guard_travel",0);put("guard_search",config.searchMs);
  put("guard_bearing",int(std::round(std::remainder(pose.yaw,2*kPi)*10000)));put("guard_cover",0);put("guard_cover_mode",0);put("guard_cover_wait",0);put("guard_route_next",0);put("guard_route_wait",0);
  confirmed=false;recognition=0;mode=GuardState::Search;
 }
 int oldSeen=get("guard_seen");put("guard_damage",damage);put("hit_source",0);put("guard_recognition",recognition);put("guard_confirmed",confirmed);put("guard_seen",visible);
 // Senses update independently of task ownership. Only the shared executor
 // below may select movement or advance role work for this actor/frame.
 put("guard_mode",int(mode));put("guard_radio",mode==GuardState::Alert?std::min(2000,get("guard_radio")+ms):0);
 int planFlags=(get("guard_under_fire")>0)|((get("weapon_reload")>0||get("weapon_reloading"))<<1)|((get("weapon_saved")&&!get("weapon_rounds"))<<2);
 bool changedThreat=target.valid()&&(previous.kind!=target.kind||previous.level!=target.level||previous.index!=target.index);
 bool planWake=freshHit||noiseWake||nearThreat||enemyDead||changedThreat||int(mode)!=get("plan_mode")||visible!=bool(oldSeen)||planFlags!=get("plan_flags")||get("stuck")>0;
 bool planDue=!m_aiDecisionScheduling||clock("plan_wait","plan_age","plan_ready",100,planWake,1);
 put("plan_due",planDue);put("plan_mode",int(mode));put("plan_flags",planFlags);
 runGuardSchedule(level,actor,track,pose,dt,ms,goal,goalZ,wantsMove);
}
void Game::guardBaseIntent(int level,size_t actor,const ActorTrack&track,ActorPose&pose,int ms,Vec2&goal,float&goalZ,bool&wantsMove){
 const auto&config=track.ai;auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 auto mode=GuardState(get("guard_mode"));int point=get("guard_point");auto globalOrigin=chunkOffset(level);auto target=guardTarget(actor);bool visible=get("guard_seen")!=0;float dt=ms*.001f;int damage=track.damageState?state(track.damageState):0;
 Vec2 remembered{get("guard_x")*.001f-globalOrigin.x,get("guard_y")*.001f-globalOrigin.y};float rememberedZ=get("guard_z")*.001f;
 auto selected=guardTarget(actor);bool lowHealth=track.health>0&&track.health-damage<=track.health*.35f;
 bool transientDefense=get("weapon_reload")>0||get("weapon_reloading")||(get("weapon_saved")&&get("weapon_rounds")==0)||get("guard_under_fire")>0;
 bool finishingRetreat=get("guard_cover_mode")==2&&get("guard_cover");
 bool defensiveIntent=config.armed&&selected.valid()&&(transientDefense||lowHealth||finishingRetreat);
 auto face=[&](Vec2 target){auto toward=target-pose.position;if(lengthSq(toward)>.001f)pose.yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi),-4.f*dt,4.f*dt);};
 if(mode==GuardState::Suspicious){face(remembered);wantsMove=false;}
 if(mode==GuardState::Alert||mode==GuardState::Investigate){goal=remembered;goalZ=rememberedZ;
  float distance=length(pose.position-goal);wantsMove=distance>(mode==GuardState::Alert?2.f:.35f)||std::fabs(pose.z-goalZ)>.3f;
  if(defensiveIntent){
   // Losing sight from a protected position must not make a reloading or
   // recently injured guard walk straight back into exposure. Use only its
   // remembered location; fresh visibility still belongs to guardThreat.
   face(remembered);guardReposition(level,actor,track,pose,{remembered.x,remembered.y,rememberedZ+selected.height*.6f},ms,goal,goalZ,wantsMove);
  }
  else if(mode==GuardState::Alert&&config.armed){const auto profile=guardRoleProfile(config.role);float stop=config.role==GuardSquadRole::Rusher?1.1f:config.attackRange*profile.advanceFraction;
   // Corridors pull the hold-up closer; open floors keep the authored standoff.
   Vec2 towardMem=distance>.001f?normalized(remembered-pose.position):Vec2{std::sin(pose.yaw),std::cos(pose.yaw)};
   Vec2 side{-towardMem.y,towardMem.x};float room=get("plan_room")*.0001f;
   if(!m_aiDecisionScheduling||get("plan_due")||!get("room_ready")){room=std::max(guardLateralClearance(level,actor,pose,side),guardLateralClearance(level,actor,pose,side*(-1.f)));put("plan_room",int(std::round(room*10000)));put("room_ready",1);}
   if(room<.85f)stop=std::min(stop,1.35f);else if(room>2.2f)stop=std::max(stop,config.attackRange*std::min(1.f,profile.advanceFraction+.12f));
   wantsMove=std::hypot(distance,pose.z-goalZ)>stop;face(remembered);
   if(visible&&profile.minimumDistance>0&&distance<profile.minimumDistance){auto away=normalized(pose.position-remembered);goal=pose.position+away*(profile.minimumDistance-distance+.35f);goalZ=pose.z;wantsMove=true;put("guard_cover",0);put("guard_cover_wait",0);}
   else if(visible&&!wantsMove){Point3 aim{target.position.x,target.position.y,target.feet+target.height*.6f};guardReposition(level,actor,track,pose,aim,ms,goal,goalZ,wantsMove);}
   else{put("guard_cover",0);put("guard_cover_wait",0);}
  }
  else if(mode==GuardState::Investigate&&config.armed&&wantsMove&&distance>3.5f&&m_npcNavBudget>0){
   // Soft approach: take a Cover node that still closes on the memory instead
   // of sprinting the open span of a room. Live shot matching is Alert work.
   --m_npcNavBudget;const auto&graph=navGraph(level,track.scale);std::vector<std::pair<float,size_t>> posts;posts.reserve(graph.nodes.size());
   for(size_t index=0;index<graph.nodes.size();++index){const auto&node=graph.nodes[index];
    if(node.hint!=ai::NodeHint::Cover||std::fabs(node.feet-pose.z)>.5f)continue;
    float toMem=length(node.position-remembered);if(toMem+.4f>=distance||length(node.position-pose.position)<.35f)continue;
    posts.push_back({toMem+length(node.position-pose.position)*.3f,index});
   }
   // The scoring policy is unchanged. Rank it before collision sweeps so
   // every worse Cover post does not repeat a long hull path each frame.
   std::sort(posts.begin(),posts.end());
   for(auto [score,index]:posts){const auto&node=graph.nodes[index];float end=pose.z;
    if(!friendlyWalkSegment(level,pose.position,pose.z,node.position,track.scale,&end)||std::fabs(end-node.feet)>.35f||guardPositionReserved(actor,node.position,end))continue;
    goal=node.position;goalZ=end;break;
   }
  }
  if(!wantsMove&&!defensiveIntent&&!(mode==GuardState::Alert&&config.armed))face(remembered);
  if(wantsMove&&!defensiveIntent&&friendlyChunkAt(remembered+globalOrigin,level)!=level)guardRoute(level,actor,remembered+globalOrigin,rememberedZ,ms,goal,goalZ);
  else{put("guard_route_next",0);put("guard_route_wait",0);}
  Point3 suppressPoint;bool suppressing=!defensiveIntent&&guardSuppressionPoint(actor,suppressPoint);
  if(suppressing){face({suppressPoint.x,suppressPoint.y});wantsMove=false;put("guard_route_next",0);put("guard_route_wait",0);}
  // Reloading and fresh incoming damage justify a short protected pause.
  // Permanent injury does not provide fresh knowledge of a hidden foe: let
  // an unfinished retreat complete, but bound it by the normal investigation
  // deadline so low health cannot preserve an unseen target indefinitely.
  bool pauseInvestigation=defensiveIntent&&transientDefense;
  if(mode==GuardState::Investigate&&!pauseInvestigation){int travel=std::min(8000,get("guard_travel")+ms);put("guard_travel",travel);
   if((!wantsMove&&!finishingRetreat&&!suppressing)||travel>=8000){mode=GuardState::Search;put("guard_search",config.searchMs);put("guard_bearing",int(std::round(std::remainder(pose.yaw,2*kPi)*10000)));put("guard_cover",0);put("guard_cover_mode",0);put("guard_cover_wait",0);wantsMove=false;}}
 }
 if(mode==GuardState::Search){int remaining=std::max(0,get("guard_search")-ms);put("guard_search",remaining);
  // Confirmed foes get a wide sweep. Sound-only searches stay focused on the
  // noise bearing so a rear approach after a distraction remains possible.
  float sweep=get("guard_confirmed")?1.2f:.45f;
  pose.yaw=get("guard_bearing")*.0001f+std::sin((config.searchMs-remaining)*.001f*2.4f)*sweep;wantsMove=false;
  if(remaining==0){mode=GuardState::Patrol;put("guard_arrived",0);put("guard_pause",0);put("repath",0);put("guard_memory",0);put("guard_confirmed",0);put("guard_recognition",0);put("guard_return",level!=m_friendlyActors[actor].homeLevel);rememberGuardTarget(actor,{});}}
 if(mode==GuardState::Patrol){auto home=chunkOffset(m_friendlyActors[actor].homeLevel)-globalOrigin;const auto&patrol=config.patrol.at(size_t(point));goal=patrol.position+home;goalZ=patrol.z;
  bool arrived=lengthSq(goal-pose.position)<.25f*.25f&&std::fabs(goalZ-pose.z)<.3f;
  if(arrived){put("guard_return",0);int pause=get("guard_pause");if(!get("guard_arrived")){
   put("guard_arrived",1);put("guard_post_yaw",int(std::lround(std::remainder(pose.yaw,2*kPi)*10000)));
   // Multi-point routes skip the spawn freeze when authored keys sit on patrol[0].
   // Single-point holds keep their authored pause for standing posts.
   if(config.patrol.size()>1&&!get("patrol_booted")){pause=0;put("patrol_booted",1);}else pause=patrol.pauseMs;
  }
   pause=std::max(0,pause-ms);put("guard_pause",pause);
   // A guard waiting at a post on a route looks over it: it turns to the
   // post's authored bearing and scans slowly either side, rather than
   // freezing along the line it walked in on.
   if(pause>0&&config.patrol.size()>1){float base=std::isfinite(patrol.look)?patrol.look:get("guard_post_yaw")*.0001f,t=(patrol.pauseMs-pause)*.001f;
    float want=base+std::sin(t*1.1f)*.6f*std::min(1.f,t*.8f);pose.yaw+=std::clamp(std::remainder(want-pose.yaw,2*kPi),-2.2f*dt,2.2f*dt);}
   if(pause==0){point=(point+1)%int(config.patrol.size());put("guard_point",point);put("guard_arrived",0);goal=config.patrol[size_t(point)].position+home;goalZ=config.patrol[size_t(point)].z;}
  }wantsMove=!arrived||!get("guard_arrived");if(wantsMove)guardRoute(level,actor,goal+globalOrigin,goalZ,ms,goal,goalZ);
 }
 if(mode==GuardState::Search||mode==GuardState::Suspicious){put("guard_route_next",0);put("guard_route_wait",0);}
 if(mode!=GuardState::Alert&&!defensiveIntent){put("guard_cover",0);put("guard_cover_wait",0);}
 put("guard_mode",int(mode));
}
}
