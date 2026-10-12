#include "../../Game.h"
#include "../Perception.h"
#include <fstream>
#include "../../../world/CustomCampaign.h"
namespace retro {
int Game::npcMilliseconds(size_t actor,float dt,std::string_view field){auto key=friendlyKey(actor,field);int micros=state(key)+int(std::round(double(dt)*1000000));setState(key,micros%1000);return micros/1000;}
void Game::validateGuardState(size_t actor)const{
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};const auto&config=m_friendlyActors[actor].track().ai;
 if(get("guard_fraction")<0||get("guard_fraction")>=1000||get("weapon_fraction")<0||get("weapon_fraction")>=1000)throw std::runtime_error("Invalid saved guard clock fraction");
 if(get("guard_step")<0||get("guard_step")>=1000||get("guard_bearing")< -31416||get("guard_bearing")>31416)throw std::runtime_error("Invalid saved guard motion");
 if(get("guard_cover")<0||get("guard_cover")>1||get("guard_cover_wait")<0||get("guard_cover_wait")>1000||get("guard_cover_x")<0||get("guard_cover_x")>24000||get("guard_cover_y")<0||get("guard_cover_y")>24000||get("guard_cover_z")< -100000||get("guard_cover_z")>100000)throw std::runtime_error("Invalid saved guard repositioning");
 if(get("guard_cover")&&(get("guard_cover_x")<350||get("guard_cover_x")>23650||get("guard_cover_y")<350||get("guard_cover_y")>23650))throw std::runtime_error("Invalid saved guard cover goal");
 if(get("guard_confirmed")<0||get("guard_confirmed")>1||get("guard_seen")<0||get("guard_seen")>1||get("guard_under_fire")<0||get("guard_under_fire")>1500)throw std::runtime_error("Invalid saved guard threat memory");
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
  if((!track.ai.enableState||state(track.ai.enableState))&&npc.pose.clip!=4&&(!track.deadState||!state(track.deadState))&&guardState(i)==GuardState::Alert)it->value=1;
 }for(const auto&flag:flags)setState(flag.id,flag.value);
}
void Game::guardIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<AiStimulus>& stimuli,Vec2& goal,float& goalZ,bool& wantsMove){
 const auto&config=track.ai;auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*field){return ctx.get(field);};auto put=[&](const char*field,int value){ctx.put(field,value);};
 auto mode=GuardState(get("guard_mode"));int ms=npcMilliseconds(actor,dt,"guard_fraction"),recognition=get("guard_recognition");bool confirmed=get("guard_confirmed")!=0;
 auto shift=chunkOffset(level)-chunkOffset(m_level),globalOrigin=chunkOffset(level);auto observer=pose.position+shift;
 float eye=pose.z+track.scale*.85f;auto previous=guardTarget(actor),target=guardThreat(actor,pose);bool visible=target.valid();
 auto sameRef=[](const AiTarget&a,const AiTarget&b){return a.valid()&&b.valid()&&a.kind==b.kind&&a.level==b.level&&a.index==b.index;};
 auto remember=[&](Vec2 global,float feet){put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(feet*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);};
 if(visible){
  // Recognition belongs to an entity reference, not merely a recent alert.
  // Brief occlusion preserves a confirmed foe; a new foe starts fresh.
  if(!sameRef(previous,target)){confirmed=false;recognition=0;}
  rememberGuardTarget(actor,target);remember(target.position+globalOrigin,target.feet);
  recognition=confirmed?config.recognitionMs:std::min(config.recognitionMs,recognition+ms);confirmed=recognition>=config.recognitionMs;mode=confirmed?GuardState::Alert:GuardState::Suspicious;
 }
 else{
  recognition=confirmed?config.recognitionMs:0;
  if(mode==GuardState::Suspicious&&!confirmed){mode=GuardState::Patrol;put("guard_memory",0);rememberGuardTarget(actor,{});}
  else if(mode==GuardState::Alert){mode=GuardState::Investigate;put("guard_travel",0);}
  const AiStimulus*heard=nullptr;float strongest=0;
  for(const auto&noise:stimuli){float height=noise.z-eye;if(lengthSq(noise.source-observer)+height*height>=noise.radius*noise.radius)continue;
   if(noise.npc==int(actor))continue;
   float strength=aiSoundStrength(observer,eye,noise.source,noise.z,noise.radius,noise.gain,noise.loud,soundRayClear(observer,eye,noise.source,noise.z));if(strength>strongest){strongest=strength;heard=&noise;}}
  if(heard){auto local=heard->source;const auto&world=worldAt(local);remember(heard->source+chunkOffset(m_level),world.supportBelow(local.x,local.y,heard->z));mode=GuardState::Investigate;}
  auto report=guardSquadReport(actor);auto remembered=Vec2{get("guard_x")*.001f-globalOrigin.x,get("guard_y")*.001f-globalOrigin.y};
  if(report.valid()&&(mode==GuardState::Patrol||mode==GuardState::Suspicious||!sameRef(previous,report)||lengthSq(report.position-remembered)>.25f*.25f||std::fabs(report.feet-get("guard_z")*.001f)>.3f)){
   // A teammate's sighting supplies last-known knowledge, never our own sight.
   rememberGuardTarget(actor,report);remember(report.position+globalOrigin,report.feet);confirmed=true;recognition=config.recognitionMs;mode=GuardState::Investigate;
  }
 }
 int damage=track.damageState?state(track.damageState):0;put("guard_under_fire",std::max(0,get("guard_under_fire")-ms));
 if(damage>get("guard_damage")){
  put("guard_under_fire",1500);auto attacker=guardAttacker(actor);
  if(attacker.valid()){target=attacker;rememberGuardTarget(actor,attacker);remember(attacker.position+globalOrigin,attacker.feet);confirmed=true;recognition=config.recognitionMs;visible=guardCanSee(actor,pose,attacker);mode=visible?GuardState::Alert:GuardState::Investigate;}
  else if(get("hit_source")){Vec2 hit{get("hit_x")*.001f,get("hit_y")*.001f};remember(hit+globalOrigin,friendlyWorld(level).supportBelow(hit.x,hit.y,pose.z+track.scale));mode=GuardState::Investigate;}
 }
 put("guard_damage",damage);put("hit_source",0);put("guard_recognition",recognition);put("guard_confirmed",confirmed);put("guard_seen",visible);
 // Senses update independently of task ownership. Only the shared executor
 // below may select movement or advance role work for this actor/frame.
 put("guard_mode",int(mode));runGuardSchedule(level,actor,track,pose,dt,ms,goal,goalZ,wantsMove);
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
  else if(mode==GuardState::Alert&&config.armed){const auto profile=guardRoleProfile(config.role);float stop=config.role==GuardSquadRole::Rusher?1.1f:config.attackRange*profile.advanceFraction;wantsMove=std::hypot(distance,pose.z-goalZ)>stop;face(remembered);
   if(visible&&profile.minimumDistance>0&&distance<profile.minimumDistance){auto away=normalized(pose.position-remembered);goal=pose.position+away*(profile.minimumDistance-distance+.35f);goalZ=pose.z;wantsMove=true;put("guard_cover",0);put("guard_cover_wait",0);}
   else if(visible&&!wantsMove){Point3 aim{target.position.x,target.position.y,target.feet+target.height*.6f};guardReposition(level,actor,track,pose,aim,ms,goal,goalZ,wantsMove);}
   else{put("guard_cover",0);put("guard_cover_wait",0);}
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
 if(mode==GuardState::Search){int remaining=std::max(0,get("guard_search")-ms);put("guard_search",remaining);pose.yaw=get("guard_bearing")*.0001f+std::sin((config.searchMs-remaining)*.001f*2.4f)*1.2f;wantsMove=false;
  if(remaining==0){mode=GuardState::Patrol;put("guard_arrived",0);put("guard_pause",0);put("repath",0);put("guard_memory",0);put("guard_confirmed",0);put("guard_recognition",0);put("guard_return",level!=m_friendlyActors[actor].homeLevel);rememberGuardTarget(actor,{});}}
 if(mode==GuardState::Patrol){auto home=chunkOffset(m_friendlyActors[actor].homeLevel)-globalOrigin;const auto&patrol=config.patrol.at(size_t(point));goal=patrol.position+home;goalZ=patrol.z;
  bool arrived=lengthSq(goal-pose.position)<.25f*.25f&&std::fabs(goalZ-pose.z)<.3f;
  if(arrived){put("guard_return",0);int pause=get("guard_pause");if(!get("guard_arrived")){put("guard_arrived",1);pause=patrol.pauseMs;}
   pause=std::max(0,pause-ms);put("guard_pause",pause);if(pause==0){point=(point+1)%int(config.patrol.size());put("guard_point",point);put("guard_arrived",0);goal=config.patrol[size_t(point)].position+home;goalZ=config.patrol[size_t(point)].z;}
  }wantsMove=!arrived||!get("guard_arrived");if(wantsMove)guardRoute(level,actor,goal+globalOrigin,goalZ,ms,goal,goalZ);
 }
 if(mode==GuardState::Search||mode==GuardState::Suspicious){put("guard_route_next",0);put("guard_route_wait",0);}
 if(mode!=GuardState::Alert&&!defensiveIntent){put("guard_cover",0);put("guard_cover_wait",0);}
 put("guard_mode",int(mode));
}
}
