#include "Game.h"
#include "../core/Ray.h"
namespace retro {
namespace {
float magnitude(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
}
ShotHit Game::traceShot(Point3 origin,Point3 direction,float range,int ignoredNpc,bool includePlayer,const HitscanDef* assist,AiTargetRef ignoredEntity)const{
 ShotHit result;result.distance=result.damageDistance=range;result.level=m_level;
 float size=magnitude(direction);if(!std::isfinite(size)||size<.0001f||!std::isfinite(range)||range<=0||range>100||!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z))return result;
 direction=direction*(1/size);auto activeOrigin=chunkOffset(m_level);
 auto exact=[&](Point3 ray){ShotHit hit;hit.level=m_level;hit.distance=hit.damageDistance=range;hit.point=origin+ray*range;
  for(int level=0;level<chunkCount();++level){auto shift=activeOrigin-chunkOffset(level);Point3 local=origin+Point3{shift.x,shift.y,0},end=local+ray*range,n;
   if(rayBoxFraction(local,ray*range,{0,0,-1000},{24,24,1000})>=1)continue;
   float fraction=chunkResident(level)?(level==m_level?m_world:m_chunks[level].world).shotFraction(local,end,n):rayBoxFraction(local,ray*range,{0,0,-1000},{24,24,1000},&n);
   if(fraction*range<hit.distance){hit={ShotKind::Solid,level,-1,fraction*range,fraction*range,origin+ray*(fraction*range),n};}
  }
  auto body=[&](ShotKind kind,int level,int index,Vec2 position,float feet,float height,float radius){auto shift=chunkOffset(level)-activeOrigin;Point3 center{position.x+shift.x,position.y+shift.y,feet};Point3 n;
   if(kind==ignoredEntity.kind&&index==ignoredEntity.index&&(kind==ShotKind::Npc||level==ignoredEntity.level))return;
   float distance=rayBoxFraction(origin,ray*range,center-Point3{radius,radius,0},center+Point3{radius,radius,height},&n)*range;
   if(distance<hit.distance-.0001f)hit={kind,level,index,distance,distance,origin+ray*distance,n};
  };
  if(includePlayer&&!dead())body(ShotKind::Player,m_level,0,m_player.pos,m_player.z,m_player.hullHeight(),.2f);
  for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();if(int(i)==ignoredNpc||!chunkResident(npc.level)||!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState)))continue;body(ShotKind::Npc,npc.level,int(i),npc.pose.position,npc.pose.z,track.scale,.2f);}
  for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&world=level==m_level?m_world:m_chunks[level].world;const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
   for(size_t i=0;i<enemies.size();++i){const auto&e=enemies[i];if(e.alive)body(ShotKind::Enemy,level,int(i),e.pos,e.bodyBottom(),e.bodyTop()-e.bodyBottom(),e.kind==Enemy::Kind::Wasp?.28f:.4f);}
   for(size_t i=0;i<world.actorTracks().size();++i){const auto&track=world.actorTracks()[i];const auto&pose=world.actorPose(i);if(track.platform||autonomousActor(track)||pose.clip==4||(track.deadState&&state(track.deadState)))continue;body(ShotKind::Actor,level,int(i),pose.position,pose.z,track.scale,track.visual==ActorVisual::Cargo?track.scale*.5f:.2f);}
  }
  if(m_world.hasLift()&&m_hazmat.initialized){int joint=-1;float distance=m_hazmat.rayHit({origin.x,origin.y,origin.z},{ray.x,ray.y,ray.z},joint);if(joint>=0&&distance<hit.distance)hit={ShotKind::Ragdoll,m_level,joint,distance,distance,origin+ray*distance,{}};}
  return hit;
 };
 result=exact(direction);
 if(!assist)return result;
 // Preserve the desktop shotgun's forgiving cone, while every selected ray
 // still resolves its first solid/body. VR supplies its own explicit origin.
 float horizontal=std::hypot(direction.x,direction.y);if(horizontal<.0001f)return result;Vec2 forward{direction.x/horizontal,direction.y/horizontal};float nearest=range;
 auto candidate=[&](Vec2 p,float feet,float height,float cone){auto delta=p-Vec2{origin.x,origin.y};float along=dot(delta,forward),lateral=std::fabs(delta.x*forward.y-delta.y*forward.x),z=origin.z+along*direction.z/horizontal;
  if(along<=0||along>=nearest||lateral>cone+along*assist->conePerMeter||z<feet||z>feet+height)return;
  Point3 target{p.x,p.y,z};auto ray=target-origin;float distance=magnitude(ray);if(distance<.001f||distance>range)return;
  auto selected=exact(ray*(1/distance));if(selected.kind==ShotKind::None||selected.kind==ShotKind::Solid)return;
  nearest=along;Vec2 victim{selected.point.x,selected.point.y};auto shift=chunkOffset(selected.level)-activeOrigin;
  if(selected.kind==ShotKind::Enemy)victim=(selected.level==m_level?m_enemies:m_chunks[selected.level].enemies)[size_t(selected.index)].pos+shift;
  if(selected.kind==ShotKind::Npc)victim=m_friendlyActors[size_t(selected.index)].pose.position+shift;
  if(selected.kind==ShotKind::Actor)victim=(selected.level==m_level?m_world:m_chunks[selected.level].world).actorPose(size_t(selected.index)).position+shift;
  selected.damageDistance=std::max(0.f,dot(victim-Vec2{origin.x,origin.y},forward));result=selected;
 };
 for(int level=0;level<chunkCount();++level)if(chunkResident(level)){auto shift=chunkOffset(level)-activeOrigin;const auto&world=level==m_level?m_world:m_chunks[level].world;const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
  for(const auto&e:enemies)if(e.alive)candidate(e.pos+shift,e.bodyBottom(),e.bodyTop()-e.bodyBottom(),assist->coneBase);
  for(size_t i=0;i<world.actorTracks().size();++i){const auto&t=world.actorTracks()[i];const auto&p=world.actorPose(i);if(!t.platform&&!autonomousActor(t)&&t.health&&p.clip!=4&&(!t.deadState||!state(t.deadState)))candidate(p.position+shift,p.z,t.scale,.4f);}
 }
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(int(i)!=ignoredNpc&&chunkResident(npc.level)&&autonomousActor(t)&&t.health&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState)))candidate(npc.pose.position+chunkOffset(npc.level)-activeOrigin,npc.pose.z,t.scale,.4f);}
 return result;
}
void Game::resolveShot(const ShotHit& hit,Point3 origin,Point3 direction,const HitscanDef& definition,bool playerFired,float damageOverride,AiTargetRef attacker){
 auto source=Vec2{origin.x,origin.y};auto shift=chunkOffset(m_level)-chunkOffset(hit.level);auto&world=friendlyWorld(hit.level);
 if(hit.kind==ShotKind::Player){receiveDamage(damageOverride>0?damageOverride:hitscanDamageAt(definition,hit.damageDistance),source,false);return;}
 if(hit.kind==ShotKind::Npc){if(playerFired)attacker={ShotKind::Player,m_level,0};damageFriendly(size_t(hit.index),damageOverride>0?damageOverride:definition.actorDamage,source+shift,attacker);if(playerFired)m_hitFlash=1;return;}
 if(hit.kind==ShotKind::Actor){const auto&actor=world.actorTracks().at(size_t(hit.index));if(!actor.health)return;const auto&pose=world.actorPose(size_t(hit.index));int previous=std::clamp(state(actor.damageState),0,actor.health);int damage=previous+std::min(actor.health-previous,int(std::ceil(std::min(damageOverride>0?damageOverride:definition.actorDamage,float(actor.health)))));setState(actor.damageState,damage);if(playerFired)m_hitFlash=1;
  bool killed=damage>=actor.health;if(killed){setState(actor.deadState,1);setState(actorPositionState(actor.deadState,0),int(std::round(pose.position.x*1000)));setState(actorPositionState(actor.deadState,1),int(std::round(pose.position.y*1000)));setState(actorPositionState(actor.deadState,2),int(std::round(pose.z*1000)));if(actor.visual!=ActorVisual::Worker){if(hit.level==m_level)++m_kills;else ++m_chunks[hit.level].kills;}}
  m_sounds.push_back({actor.visual==ActorVisual::Worker?Sound::Hurt:killed?(actor.visual==ActorVisual::Wasp?Sound::WaspDeath:Sound::SpiderDeath):Sound::PunchHit,pose.position-shift,.8f,1,true,17,pose.z+actor.scale*.7f});updateMechanisms(0);return;
 }
 if(hit.kind==ShotKind::Enemy){auto&enemy=(hit.level==m_level?m_enemies:m_chunks[hit.level].enemies).at(size_t(hit.index));bool killed=applyEnemyDamage(enemy,damageOverride>0?damageOverride:hitscanDamageAt(definition,hit.damageDistance),playerFired,playerFired&&hit.damageDistance<definition.closeHitStop,hit.level);auto audible=enemy;audible.pos=audible.pos-shift;enemySound(audible,killed?2:0,killed?.85f:.55f,killed?.9f:1.22f);return;}
 if(hit.kind==ShotKind::Ragdoll){m_hazmat.impulse(hit.index,{direction.x*definition.ragdollImpulse,direction.y*definition.ragdollImpulse,direction.z*definition.ragdollImpulse+.7f});m_sounds.push_back({Sound::PunchHit,{hit.point.x,hit.point.y},.55f,.8f,true,17,hit.point.z});return;}
 if(hit.kind!=ShotKind::Solid)return;
 auto point=hit.point+direction*.003f+Point3{shift.x,shift.y,0};int tx=int(std::floor(point.x)),ty=int(std::floor(point.y));char tile=world.tile(tx,ty);float base=world.floorHeight(tx+.5f,ty+.5f);if(point.z<base||point.z>base+(tile=='C'?.6f:tile=='B'?1.1f:0))tile='.';
 if(tile=='C'){world.destroyTile(tx,ty);auto&clutter=hit.level==m_level?m_clutter:m_chunks[hit.level].clutter;for(int i=0;i<4;++i){Clutter c;c.kind=6;c.pos={tx+.5f,ty+.5f};c.z=world.floorHeight(c.pos.x,c.pos.y);c.yaw=i*kPi*.5f;c.pitch=(i%2?-.18f:.18f);c.roll=(i%2?.12f:-.12f);c.velocity={std::cos(c.yaw)*3,std::sin(c.yaw)*3};c.vz=2;c.spin=(i%2?1.f:-1.f)*3;c.pitchSpeed=(i%2?1.f:-1.f)*2;c.rollSpeed=(i%2?-1.f:1.f)*2;clutter.push_back(c);}m_sounds.push_back({Sound::JunkSoft,Vec2{tx+.5f,ty+.5f}-shift,.9f,1,true,17,world.floorHeight(tx+.5f,ty+.5f)+.3f});return;}
 if(tile=='B'){float floor=world.floorHeight(tx+.5f,ty+.5f);world.destroyTile(tx,ty);m_barrelExplosions.push_back({{tx+.5f,ty+.5f},floor+.55f,m_elapsed,hit.level});if(m_barrelExplosions.size()>16)m_barrelExplosions.erase(m_barrelExplosions.begin());m_verticalSpringVelocity-=1.5f;m_damageFlash=.8f;sound(Sound::LiftCrash,1,.8f);auto&enemies=hit.level==m_level?m_enemies:m_chunks[hit.level].enemies;for(auto&e:enemies)if(e.alive&&length(e.pos-Vec2{tx+.5f,ty+.5f})<kUnarmedMelee.barrelRadius)applyEnemyDamage(e,kUnarmedMelee.barrelDamage,false,false,hit.level);return;}
 auto impact=hit.point+hit.normal*.004f;if(hit.normal.z>.5f)impact.z=std::max(impact.z,world.floorHeight(impact.x+shift.x,impact.y+shift.y)+.004f);if(hit.normal.z<-.5f)impact.z=std::min(impact.z,world.ceilingHeight(impact.x+shift.x,impact.y+shift.y)-.004f);
 m_bulletImpacts.push_back({{impact.x+shift.x,impact.y+shift.y},impact.z,{hit.normal.x,hit.normal.y},m_elapsed,hit.level,false,0,hit.normal.z});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
}
}
