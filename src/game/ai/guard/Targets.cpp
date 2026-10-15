#include "../../Game.h"
namespace retro {
AiTarget Game::resolveGuardTarget(size_t actor,AiTargetRef ref)const{
 AiTarget target;if(actor>=m_friendlyActors.size())return target;auto origin=chunkOffset(m_friendlyActors[actor].level);
 target.kind=ref.kind;target.level=ref.level;target.index=ref.index;
 if(ref.kind==ShotKind::Player){if(dead())return {};target.level=m_level;target.index=0;target.position=m_player.pos;target.feet=m_player.z;target.height=m_player.hullHeight();target.eye=m_player.eye;target.faction=AiFaction::Player;}
 else if(ref.kind==ShotKind::Npc){if(ref.index<0||size_t(ref.index)>=m_friendlyActors.size()||size_t(ref.index)==actor)return {};const auto&npc=m_friendlyActors[size_t(ref.index)];const auto&t=npc.track();if(!autonomousActor(t)||npc.pose.clip==4||!t.health||(t.deadState&&state(t.deadState)))return {};float body=friendlyHullHeight(size_t(ref.index),t);target.level=npc.level;target.position=npc.pose.position;target.feet=npc.pose.z;target.height=body;target.eye=body*.85f;target.faction=actorFaction(t);}
 else if(ref.kind==ShotKind::Enemy){if(ref.level<0||ref.level>=chunkCount()||!chunkResident(ref.level))return {};const auto&enemies=ref.level==m_level?m_enemies:m_chunks[ref.level].enemies;if(ref.index<0||size_t(ref.index)>=enemies.size()||!enemies[size_t(ref.index)].alive)return {};const auto&e=enemies[size_t(ref.index)];target.position=e.pos;target.feet=e.bodyBottom();target.height=e.bodyTop()-e.bodyBottom();target.eye=target.height*.65f;target.faction=AiFaction::Creature;}
 else if(ref.kind==ShotKind::Actor){if(ref.level<0||ref.level>=chunkCount()||!chunkResident(ref.level))return {};const auto&w=ref.level==m_level?m_world:m_chunks[ref.level].world;if(ref.index<0||size_t(ref.index)>=w.actorTracks().size())return {};const auto&t=w.actorTracks()[size_t(ref.index)];const auto&p=w.actorPose(size_t(ref.index));if(t.ai.mode!=ActorAiMode::Scripted||t.platform||!t.health||p.clip==4||(t.deadState&&state(t.deadState))||(t.ai.enableState&&!state(t.ai.enableState)))return {};target.position=p.position;target.feet=p.z;target.height=t.scale;target.eye=t.scale*.85f;target.faction=actorFaction(t);}
 else if(ref.kind==ShotKind::Drone)return engineerDroneTarget(ref.index,m_friendlyActors[actor].level);
 else return {};
 if(!chunkResident(target.level))return {};target.position+=chunkOffset(target.level)-origin;return target;
}
AiTarget Game::guardTarget(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};AiTargetRef ref{ShotKind(get("guard_target_kind")),get("guard_target_level")-1,get("guard_target_index")-1};
 if(ref.kind==ShotKind::None&&guardState(actor)==GuardState::Alert&&factionsHostile(actorFaction(m_friendlyActors[actor].track()),AiFaction::Player))ref={ShotKind::Player,m_level,0};
 auto target=resolveGuardTarget(actor,ref);if(target.valid()&&get("guard_memory")&&!get("guard_seen")){auto origin=chunkOffset(m_friendlyActors[actor].level);target.position={get("guard_x")*.001f-origin.x,get("guard_y")*.001f-origin.y};target.feet=get("guard_z")*.001f;}return target;
}
AiTarget Game::guardAttacker(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};return resolveGuardTarget(actor,{ShotKind(get("hit_kind")),get("hit_level")-1,get("hit_index")-1});
}
void Game::rememberGuardTarget(size_t actor,const AiTarget&target){
 if(state(friendlyKey(actor,"guard_target_kind"))!=int(target.kind)||state(friendlyKey(actor,"guard_target_level"))!=(target.valid()?target.level+1:0)||state(friendlyKey(actor,"guard_target_index"))!=(target.valid()?target.index+1:0)){setState(friendlyKey(actor,"guard_flank_done"),0);setState(friendlyKey(actor,"guard_flank_wait"),0);}
 setState(friendlyKey(actor,"guard_target_kind"),target.valid()?int(target.kind):0);setState(friendlyKey(actor,"guard_target_level"),target.valid()?target.level+1:0);setState(friendlyKey(actor,"guard_target_index"),target.valid()?target.index+1:0);
}
bool Game::guardCanSee(size_t actor,const ActorPose&pose,const AiTarget&target)const{
 if(!target.valid())return false;auto live=resolveGuardTarget(actor,{target.kind,target.level,target.index});if(!live.valid())return false;const auto&npc=m_friendlyActors[actor];const auto&c=npc.track().ai;float eye=friendlyHullHeight(actor,npc.track())*.85f;auto delta=live.position-pose.position;float z=live.feet+live.eye-pose.z-eye;
 // Crouched targets present a smaller silhouette; shrink recognition range so
 // sneaking closer is required before the cone can lock on.
 float range=c.sightRange;if(live.kind==ShotKind::Player&&m_player.crouched)range*=.55f;else if(live.height<Player::StandingHeight*.75f)range*=.55f;
 auto shift=chunkOffset(npc.level)-chunkOffset(m_level);return lengthSq(delta)+z*z<range*range&&dot(normalized(delta),Vec2{std::sin(pose.yaw),std::cos(pose.yaw)})>c.sightDot&&soundRayClear(pose.position+shift,pose.z+eye,live.position+shift,live.feet+live.eye);
}
AiTarget Game::guardThreat(size_t actor,const ActorPose&pose)const{
 AiTarget best;float score=1e9f;const auto&npc=m_friendlyActors[actor];auto faction=actorFaction(npc.track());auto previous=guardTarget(actor);
 auto consider=[&](AiTargetRef ref){auto t=resolveGuardTarget(actor,ref);if(!t.valid())return;
  bool personal=t.kind==previous.kind&&t.index==previous.index&&(t.kind==ShotKind::Npc||t.level==previous.level)&&state(friendlyKey(actor,"guard_confirmed"));
  if(!factionsHostile(faction,t.faction)&&!personal)return;if(!guardCanSee(actor,pose,t))return;
  float height=t.feet-pose.z,value=lengthSq(t.position-pose.position)+height*height;if(personal)value*=.8f;if(value<score){best=t;score=value;}
 };
 consider({ShotKind::Player,m_level,0});
 for(size_t i=0;i<m_friendlyActors.size();++i)if(i!=actor)consider({ShotKind::Npc,m_friendlyActors[i].level,int(i)});
 for(const auto&drone:engineerDrones())consider({ShotKind::Drone,drone.level,drone.slot});
 for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&w=level==m_level?m_world:m_chunks[level].world;const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
  auto center=chunkOffset(level)+Vec2{12,12}-chunkOffset(npc.level);if(length(center-pose.position)>npc.track().ai.sightRange+17)continue;
  for(size_t i=0;i<enemies.size();++i)consider({ShotKind::Enemy,level,int(i)});for(size_t i=0;i<w.actorTracks().size();++i)if(w.actorTracks()[i].ai.mode==ActorAiMode::Scripted)consider({ShotKind::Actor,level,int(i)});
 }return best;
}
bool Game::guardShotMatches(size_t actor,const ShotHit&hit)const{
 auto t=guardTarget(actor);return t.valid()&&hit.kind==t.kind&&hit.index==t.index&&(t.kind==ShotKind::Npc||hit.level==t.level);
}
bool Game::guardMeleeReady(size_t actor,const ActorPose&pose,const AiTarget&t)const{
 if(!t.valid()||!guardCanSee(actor,pose,t)||std::hypot(length(t.position-pose.position),t.feet-pose.z)>1.25f||t.feet>=pose.z+m_friendlyActors[actor].track().scale||t.feet+t.height<=pose.z)return false;
 auto shift=chunkOffset(m_friendlyActors[actor].level)-chunkOffset(m_level);Point3 from{pose.position.x+shift.x,pose.position.y+shift.y,pose.z+m_friendlyActors[actor].track().scale*.65f},to{t.position.x+shift.x,t.position.y+shift.y,t.feet+t.height*.6f},direction=to-from;float range=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
 return range>.001f&&guardShotMatches(actor,traceShot(from,direction,range+.1f,int(actor),true));
}
void Game::damageGuardTarget(size_t actor,float amount){
 auto t=guardTarget(actor);if(!t.valid())return;const auto&npc=m_friendlyActors[actor];auto shift=chunkOffset(npc.level)-chunkOffset(m_level);Point3 from{npc.pose.position.x+shift.x,npc.pose.position.y+shift.y,npc.pose.z+npc.track().scale*.65f},to{t.position.x+shift.x,t.position.y+shift.y,t.feet+t.height*.6f};auto direction=to-from;float distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);if(distance>.001f)direction=direction*(1/distance);
 if(t.kind==ShotKind::Player){receiveDamage(amount,{from.x,from.y},true);return;}
 resolveShot({t.kind,t.level,t.index,distance,distance,to,{}},from,direction,weaponDef(WeaponId::Shotgun).hitscan,false,amount,{ShotKind::Npc,npc.level,int(actor)});
}
bool Game::guardCovered(size_t actor,const ActorPose&pose,const AiTarget&t)const{
 if(!t.valid())return false;auto shift=chunkOffset(m_friendlyActors[actor].level)-chunkOffset(m_level);Point3 from{t.position.x+shift.x,t.position.y+shift.y,t.feet+t.eye};
 for(float portion:{.6f,.85f}){Point3 to{pose.position.x+shift.x,pose.position.y+shift.y,pose.z+m_friendlyActors[actor].track().scale*portion},delta=to-from;float range=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);if(range<.001f||range>100)return false;
  auto hit=traceShot(from,delta,range,int(actor),false,nullptr,{t.kind,t.level,t.index});if(hit.kind!=ShotKind::Solid)return false;
 }return true;
}
bool Game::guardHostileToPlayer(size_t actor)const{
 auto target=guardTarget(actor);return factionsHostile(actorFaction(m_friendlyActors[actor].track()),AiFaction::Player)||(target.kind==ShotKind::Player&&state(friendlyKey(actor,"guard_confirmed")));
}
void Game::validateGuardPolicyState(size_t actor)const{
 validateGuardActions(actor);
 validateGuardSupport(actor);
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};
 for(const char*f:{"guard_return","guard_seen","guard_confirmed","guard_slot","guard_flank_done","weapon_saved","weapon_reloading"})if(get(f)<0||get(f)>1)throw std::runtime_error("Invalid saved guard policy flag");
 int saved=get("weapon_saved"),reloading=get("weapon_reloading"),rounds=get("weapon_rounds"),reload=get("weapon_reload");
 if((reloading&&(!saved||rounds!=0||reload<=0))||(!reloading&&reload!=0)||(!saved&&rounds!=0))throw std::runtime_error("Inconsistent saved guard reload state");
 if(get("guard_slot_ms")<0||get("guard_slot_ms")>3500||get("guard_flank_wait")<0||get("guard_flank_wait")>5000||get("squad_fraction")<0||get("squad_fraction")>=1000||get("weapon_bash")<0||get("weapon_bash")>300||get("weapon_bash_pose")<0||get("weapon_bash_pose")>220||get("guard_cover_mode")<0||get("guard_cover_mode")>2)throw std::runtime_error("Invalid saved guard policy clock");
 for(const char*prefix:{"guard_target","weapon_target","hit"}){auto kind=get((std::string(prefix)+"_kind").c_str()),level=get((std::string(prefix)+"_level").c_str()),index=get((std::string(prefix)+"_index").c_str());
  if(kind!=0&&kind!=int(ShotKind::Enemy)&&kind!=int(ShotKind::Npc)&&kind!=int(ShotKind::Actor)&&kind!=int(ShotKind::Player)&&kind!=int(ShotKind::Drone))throw std::runtime_error("Invalid saved guard target kind");
  if(level<0||level>chunkCount()||index<0||index>65536||(kind&&(level==0||index==0)))throw std::runtime_error("Invalid saved guard target reference");
 }
}
}
