#include "../core/FrameProfile.h"
#include "Game.h"
#include "../core/Ray.h"
namespace retro {
namespace {
float magnitude(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
}
ShotHit Game::traceShot(Point3 origin,Point3 direction,float range,int ignoredNpc,bool includePlayer,const HitscanDef* assist,AiTargetRef ignoredEntity)const{
 perf::count(perf::TraceShot);
 ShotHit result;result.distance=result.damageDistance=range;result.level=m_level;
 float size=magnitude(direction);if(!std::isfinite(size)||size<.0001f||!std::isfinite(range)||range<=0||range>100||!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z))return result;
 direction=direction*(1/size);auto activeOrigin=chunkOffset(m_level);
 auto exact=[&](Point3 ray){std::vector<ShotPassage> passages;ShotHit hit;hit.level=m_level;hit.distance=hit.damageDistance=range;hit.point=origin+ray*range;
  for(int level=0;level<chunkCount();++level){auto shift=activeOrigin-chunkOffset(level);Point3 local=origin+Point3{shift.x,shift.y,0},end=local+ray*range,n;
   if(rayBoxFraction(local,ray*range,{0,0,-1000},{24,24,1000})>=1)continue;
   ShotTrace trace;float fraction=chunkResident(level)?(level==m_level?m_world:m_chunks[level].world).shotFraction(local,end,n,&trace):rayBoxFraction(local,ray*range,{0,0,-1000},{24,24,1000},&n);
   if(fraction*range<hit.distance){hit={ShotKind::Solid,level,-1,fraction*range,fraction*range,origin+ray*(fraction*range),n};hit.breakable=trace.blocking;}
   for(int j=0;j<2;++j)if(trace.passed[j]>=0)passages.push_back({1,level,trace.passed[j],trace.passedAt[j]*range,origin+ray*(trace.passedAt[j]*range),trace.passedNormal[j]});
  }
  auto body=[&](ShotKind kind,int level,int index,Vec2 position,float feet,float height,float radius){auto shift=chunkOffset(level)-activeOrigin;Point3 center{position.x+shift.x,position.y+shift.y,feet};Point3 n;
   if(kind==ignoredEntity.kind&&index==ignoredEntity.index&&(kind==ShotKind::Npc||level==ignoredEntity.level))return;
   float distance=rayBoxFraction(origin,ray*range,center-Point3{radius,radius,0},center+Point3{radius,radius,height},&n)*range;
   if(distance<hit.distance-.0001f)hit={kind,level,index,distance,distance,origin+ray*distance,n};
  };
  if(includePlayer&&!dead())body(ShotKind::Player,m_level,0,m_player.pos,m_player.z,m_player.hullHeight(),.2f);
  for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();if(int(i)==ignoredNpc||!chunkResident(npc.level)||!autonomousActor(track)||(npc.pose.clip==4&&!npcIncapacitated(i))||(track.deadState&&state(track.deadState)))continue;
   if(ignoredEntity.kind==ShotKind::Npc&&ignoredEntity.index==int(i))continue;
   auto [low,high]=friendlyShotBounds(i);auto shift=chunkOffset(npc.level)-activeOrigin;Point3 offset{shift.x,shift.y,0},normal;float distance=rayBoxFraction(origin,ray*range,low+offset,high+offset,&normal)*range;
   if(distance<hit.distance-.0001f)hit={ShotKind::Npc,npc.level,int(i),distance,distance,origin+ray*distance,normal};
  }
  for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&world=level==m_level?m_world:m_chunks[level].world;const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
   for(size_t i=0;i<enemies.size();++i){const auto&e=enemies[i];if(e.alive)body(ShotKind::Enemy,level,int(i),e.pos,e.bodyBottom(),e.bodyTop()-e.bodyBottom(),e.kind==Enemy::Kind::Wasp?.28f:.4f);}
   for(size_t i=0;i<world.actorTracks().size();++i){const auto&track=world.actorTracks()[i];const auto&pose=world.actorPose(i);if(track.platform||autonomousActor(track)||pose.clip==4||(track.deadState&&state(track.deadState)))continue;body(ShotKind::Actor,level,int(i),pose.position,pose.z,track.scale,track.visual==ActorVisual::Cargo?track.scale*.5f:.2f);}
  }
  for(const auto&drone:engineerDrones())if(engineerDroneTarget(drone.slot,m_level).valid())body(ShotKind::Drone,drone.level,drone.slot,{drone.position.x,drone.position.y},drone.position.z,EngineerDroneHeight,EngineerDroneRadius);
  if(m_world.hasLift()&&m_hazmat.initialized){int joint=-1;float distance=m_hazmat.rayHit({origin.x,origin.y,origin.z},{ray.x,ray.y,ray.z},joint);if(joint>=0&&distance<hit.distance)hit={ShotKind::Ragdoll,m_level,joint,distance,distance,origin+ray*distance,{}};}
  // A hung suit is Ragdoll index (suit+1)*Count+joint; the lift's casualty keeps 0..Count-1.
  {const auto&suits=hungSuits(m_level);for(size_t s=0;s<suits.size();++s){int joint=-1;float distance=suits[s].rayHit({origin.x,origin.y,origin.z},{ray.x,ray.y,ray.z},joint);
   if(joint>=0&&distance<hit.distance)hit={ShotKind::Ragdoll,m_level,int(s+1)*Ragdoll::Count+joint,distance,distance,origin+ray*distance,{}};}}
  for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&list=chunkClutter(level);auto shift=chunkOffset(level)-activeOrigin;
   for(size_t i=0;i<list.size();++i){const auto&c=list[i];if(c.broken||c.model<0)continue;auto e=c.extent();Point3 center{c.pos.x+shift.x,c.pos.y+shift.y,c.z+e[2]},n;float distance=rayBoxFraction(origin,ray*range,center-Point3{e[0],e[1],e[2]},center+Point3{e[0],e[1],e[2]},&n)*range;if(distance>=hit.distance)continue;
    float block=propMaterialTraits(c.material()).blockMass;if(block>0&&c.mass()>=block)hit={ShotKind::Prop,level,int(i),distance,distance,origin+ray*distance,n};else passages.push_back({2,level,int(i),distance,origin+ray*distance,n});
   }
  }
  std::sort(passages.begin(),passages.end(),[](const auto&a,const auto&b){return a.distance<b.distance;});int count=0;for(const auto&p:passages)if(p.distance<hit.distance&&count<2)hit.passed[count++]=p;
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
  auto selected=exact(ray*(1/distance));if(selected.kind==ShotKind::None||selected.kind==ShotKind::Solid||selected.kind==ShotKind::Prop)return;
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
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(int(i)!=ignoredNpc&&chunkResident(npc.level)&&autonomousActor(t)&&t.health&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState)))candidate(npc.pose.position+chunkOffset(npc.level)-activeOrigin,npc.pose.z,friendlyHullHeight(i,t),.4f);}
 for(const auto&drone:engineerDrones())if(engineerDroneTarget(drone.slot,m_level).valid())candidate(Vec2{drone.position.x,drone.position.y}+chunkOffset(drone.level)-activeOrigin,drone.position.z,EngineerDroneHeight,EngineerDroneRadius);
 return result;
}
// A scripted person the player shoots at flinches (clip 3) even when the story
// keeps them out of reach: the shot lines up on them and lands no more than
// 6 m short (on protective glass, bars, a wall in front of them), or strikes
// within 1.8 m of them. Mechanisms plays the flinch over their keyed pose.
void Game::startleScriptedActors(const ShotHit& hit,Point3 origin,Point3 direction){
 if(hit.level<0||hit.level>=chunkCount()||!chunkResident(hit.level))return;
 const auto&world=friendlyWorld(hit.level);auto shift=chunkOffset(hit.level)-chunkOffset(m_level);
 float length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);if(length<.0001f)return;direction=direction*(1/length);
 for(size_t i=0;i<world.actorTracks().size();++i){const auto&track=world.actorTracks()[i];const auto&pose=world.actorPose(i);
  if(track.platform||autonomousActor(track)||!humanActorVisual(track.visual)||pose.clip==4||(track.deadState&&state(track.deadState)))continue;
  Point3 chest{pose.position.x+shift.x,pose.position.y+shift.y,pose.z+track.scale*.6f};auto toChest=chest-origin,toImpact=chest-hit.point;
  float along=toChest.x*direction.x+toChest.y*direction.y+toChest.z*direction.z;auto miss=toChest-direction*along;
  bool aimed=along>0&&along<30&&miss.x*miss.x+miss.y*miss.y+miss.z*miss.z<1.f&&hit.distance>along-6.f;
  bool close=toImpact.x*toImpact.x+toImpact.y*toImpact.y+toImpact.z*toImpact.z<1.8f*1.8f;
  if(aimed||close)setState(actorTrackState(track.timerState,i,"flinch_ms"),ScriptedFlinchMs);
 }
}
void Game::resolveShot(const ShotHit& hit,Point3 origin,Point3 direction,const HitscanDef& inputDefinition,bool playerFired,float damageOverride,AiTargetRef attacker){
 if(playerFired)startleScriptedActors(hit,origin,direction);
 // The bodies it passes through answer to the ray itself (Carcass): they block nothing.
 m_shotRays.push_back({origin,direction,hit.kind==ShotKind::None?60.f:hit.distance,m_elapsed,damageOverride>0?damageOverride:hitscanDamageAt(inputDefinition,hit.distance),inputDefinition.ragdollImpulse,m_level});if(m_shotRays.size()>24)m_shotRays.erase(m_shotRays.begin());
 HitscanDef definition=inputDefinition;float attenuation=1;
 auto prop=[&](int level,int index,Point3 point,float distance){float propDamage=hitscanDamageAt(definition,distance)*attenuation;auto shift=chunkOffset(m_level)-chunkOffset(level);point=point+Point3{shift.x,shift.y,0};knockClutter(level,size_t(index),point,direction*(definition.propImpulse*propDamage/definition.dmgNear));damageClutter(level,size_t(index),propDamage,point,direction);};
 for(const auto&p:hit.passed)if(p.kind){if(p.kind==2)prop(p.level,p.index,p.point,p.distance);else{auto shift=chunkOffset(m_level)-chunkOffset(p.level);damagePane(p.level,p.index,hitscanDamageAt(definition,p.distance)*attenuation,p.point+Point3{shift.x,shift.y,0},direction);attenuation*=.8f;}}
 if(hit.kind==ShotKind::Prop){prop(hit.level,hit.index,hit.point,hit.distance);return;}
 definition.dmgNear*=attenuation;definition.dmgMid*=attenuation;definition.dmgFar*=attenuation;definition.actorDamage*=attenuation;if(damageOverride>0)damageOverride*=attenuation;
 auto source=Vec2{origin.x,origin.y};auto shift=chunkOffset(m_level)-chunkOffset(hit.level);auto&world=friendlyWorld(hit.level);
 if(hit.kind==ShotKind::Player){receiveDamage(damageOverride>0?damageOverride:hitscanDamageAt(definition,hit.damageDistance),source,false);return;}
 if(hit.kind==ShotKind::Drone){damageEngineerDrone(hit.index,damageOverride>0?damageOverride:definition.actorDamage,attacker);if(playerFired)m_hitFlash=1;return;}
 if(hit.kind==ShotKind::Npc){if(playerFired)attacker={ShotKind::Player,m_level,0};damageFriendly(size_t(hit.index),damageOverride>0?damageOverride:definition.actorDamage,source+shift,attacker);if(playerFired)m_hitFlash=1;hitBurst(hit.level,hit.point+Point3{shift.x,shift.y,0},direction,ImpactMaterial::Flesh,-1,.8f,false);return;}
 if(hit.kind==ShotKind::Actor){const auto&actor=world.actorTracks().at(size_t(hit.index));if(!actor.health)return;const auto&pose=world.actorPose(size_t(hit.index));int previous=std::clamp(state(actor.damageState),0,actor.health);int damage=previous+std::min(actor.health-previous,int(std::ceil(std::min(damageOverride>0?damageOverride:definition.actorDamage,float(actor.health)))));setState(actor.damageState,damage);if(playerFired)m_hitFlash=1;
  hitBurst(hit.level,hit.point+Point3{shift.x,shift.y,0},direction,ImpactMaterial::Flesh,humanActorVisual(actor.visual)?-1:int(actor.visual==ActorVisual::Wasp?CreatureKind::Wasp:CreatureKind::Huntsman),damage>=actor.health?1.5f:.9f,damage>=actor.health);
  bool killed=damage>=actor.health;if(killed){setState(actor.deadState,1);setState(actorPositionState(actor.deadState,0),int(std::round(pose.position.x*1000)));setState(actorPositionState(actor.deadState,1),int(std::round(pose.position.y*1000)));setState(actorPositionState(actor.deadState,2),int(std::round(pose.z*1000)));if(!humanActorVisual(actor.visual)){if(hit.level==m_level)++m_kills;else ++m_chunks[hit.level].kills;}}
  m_sounds.push_back({humanActorVisual(actor.visual)?Sound::Hurt:killed?(actor.visual==ActorVisual::Wasp?Sound::WaspDeath:Sound::SpiderDeath):Sound::PunchHit,pose.position-shift,.8f,1,true,17,pose.z+actor.scale*.7f});updateMechanisms(0);return;
 }
 if(hit.kind==ShotKind::Enemy){auto&enemy=(hit.level==m_level?m_enemies:m_chunks[hit.level].enemies).at(size_t(hit.index));
  // A gunshot wound is a stimulus: the creature turns on the shooter even if it never saw them.
  if(playerFired&&enemy.alive){enemy.recognition=1;enemy.lastKnown=m_player.pos+chunkOffset(m_level)-chunkOffset(hit.level);enemy.lastKnownZ=m_player.z;enemy.awareness=std::max(enemy.awareness,8.f);if(enemy.state==Enemy::State::Idle||enemy.state==Enemy::State::Search)enemy.state=Enemy::State::Investigate;}
  // A hide (the cursed equipment's steel) turns most of a round; a blast is not a round.
  const float armour=m_blastShot?1.f:creatureTraits(enemy.kind).armour;
  const float dealt=(damageOverride>0?damageOverride:hitscanDamageAt(definition,hit.damageDistance))*armour;const Point3 local{shift.x,shift.y,0};const bool living=enemy.alive;
  if(living)enemy.wound(origin+local,direction);
  const float flinch=enemy.painFlash;
  bool killed=applyEnemyDamage(enemy,dealt,playerFired,playerFired&&hit.damageDistance<definition.closeHitStop,hit.level);
  // What a hide turns sparks off it as off any steel, and it does not flinch at it.
  if(armour<1&&living){enemy.painFlash=killed?enemy.painFlash:flinch;sparkBurst(hit.level,hit.point+local,direction*-1.f,.95f,14,false);}
  // It goes down the way the round was travelling.
  if(killed)enemy.deathPush={direction.x*definition.ragdollImpulse,direction.y*definition.ragdollImpulse,direction.z*definition.ragdollImpulse+.4f};
  if(living)hitBurst(hit.level,hit.point+local,direction,ImpactMaterial::Flesh,int(enemy.kind),std::clamp(dealt/(34.f*armour),.5f,2.f)*(killed?1.5f:1.f)*(armour<1&&!killed?.6f:1.f),killed);
  auto audible=enemy;audible.pos=audible.pos-shift;enemySound(audible,killed?2:0,killed?.85f:.55f,killed?.9f:1.22f);return;}
 if(hit.kind==ShotKind::Ragdoll){auto&rag=hit.index>=Ragdoll::Count&&size_t(m_level)<m_hungSuits.size()&&size_t(hit.index/Ragdoll::Count-1)<m_hungSuits[size_t(m_level)].size()?m_hungSuits[size_t(m_level)][size_t(hit.index/Ragdoll::Count-1)]:m_hazmat;
  rag.impulse(hit.index%Ragdoll::Count,{direction.x*definition.ragdollImpulse,direction.y*definition.ragdollImpulse,direction.z*definition.ragdollImpulse+.7f});m_sounds.push_back({Sound::PunchHit,{hit.point.x,hit.point.y},.55f,.8f,true,17,hit.point.z});return;}
 if(hit.kind!=ShotKind::Solid)return;
 if(hit.breakable>=0){auto offset=chunkOffset(m_level)-chunkOffset(hit.level);damagePane(hit.level,hit.breakable,hitscanDamageAt(definition,hit.damageDistance),hit.point+Point3{offset.x,offset.y,0},direction);hitBurst(hit.level,hit.point+Point3{offset.x,offset.y,0},direction*-1.f,ImpactMaterial::Glass,-1,.8f,false);return;}
 auto point=hit.point+direction*.003f+Point3{shift.x,shift.y,0};int tx=int(std::floor(point.x)),ty=int(std::floor(point.y));char tile=world.tile(tx,ty);float base=world.floorHeight(tx+.5f,ty+.5f);if(point.z<base||point.z>base+(tile=='C'?.6f:tile=='B'?1.1f:0))tile='.';
 if(tile=='C'){world.destroyTile(tx,ty);auto&clutter=hit.level==m_level?m_clutter:m_chunks[hit.level].clutter;for(int i=0;i<4;++i){Clutter c;c.kind=6;c.pos={tx+.5f,ty+.5f};c.z=world.floorHeight(c.pos.x,c.pos.y);c.yaw=i*kPi*.5f;c.pitch=(i%2?-.18f:.18f);c.roll=(i%2?.12f:-.12f);c.velocity={std::cos(c.yaw)*3,std::sin(c.yaw)*3};c.vz=2;c.spin=(i%2?1.f:-1.f)*3;c.pitchSpeed=(i%2?1.f:-1.f)*2;c.rollSpeed=(i%2?-1.f:1.f)*2;clutter.push_back(c);}m_sounds.push_back({Sound::JunkSoft,Vec2{tx+.5f,ty+.5f}-shift,.9f,1,true,17,world.floorHeight(tx+.5f,ty+.5f)+.3f});return;}
 if(tile=='B'){detonateBarrel(hit.level,tx,ty);return;}
 auto impact=hit.point+hit.normal*.004f;if(hit.normal.z>.5f)impact.z=std::max(impact.z,world.floorHeight(impact.x+shift.x,impact.y+shift.y)+.004f);if(hit.normal.z<-.5f)impact.z=std::min(impact.z,world.ceilingHeight(impact.x+shift.x,impact.y+shift.y)-.004f);
 // What it struck decides the mark it leaves and what it throws: sparks off metal, dust and chips off
 // masonry, splinters off timber, clods off earth.
 const Point3 struck{impact.x+shift.x,impact.y+shift.y,impact.z};const auto material=world.impactMaterial(struck,hit.normal);
 m_bulletImpacts.push_back({{struck.x,struck.y},struck.z,{hit.normal.x,hit.normal.y},m_elapsed,hit.level,false,0,hit.normal.z,int(material)});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
 if(material!=ImpactMaterial::Metal)hitBurst(hit.level,struck,hit.normal,material,-1,std::clamp(hitscanDamageAt(definition,hit.damageDistance)/34.f,.5f,1.6f),false);
}
void Game::hitBurst(int level,Point3 point,Point3 direction,ImpactMaterial material,int species,float power,bool kill){
 if(level<0||level>=chunkCount()||!chunkResident(level))return;const auto&world=friendlyWorld(level);
 float size=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);direction=size<.0001f?Point3{0,0,1}:direction*(1/size);
 HitBurst burst{point,direction,m_elapsed,level,material,species,power,kill};
 // Where what it throws comes down, and the wall that stops it short.
 burst.floor=world.supportBelow(point.x+direction.x*.25f,point.y+direction.y*.25f,point.z+.05f);
 const float far=2.6f;Point3 normal{},from=point+direction*.04f,to=point+direction*far;float fraction=std::clamp(world.shotFraction(from,to,normal),0.f,1.f);burst.reach=fraction*far;
 m_hitBursts.push_back(burst);if(m_hitBursts.size()>48)m_hitBursts.erase(m_hitBursts.begin());
 // A round through flesh paints the wall behind it.
 if(material==ImpactMaterial::Flesh&&fraction<1&&std::fabs(normal.z)<.5f&&power>=.7f){auto at=from+(to-from)*fraction;unsigned seed=unsigned(m_bulletImpacts.size())*2654435761u^unsigned(m_elapsed*977.f);
  m_bulletImpacts.push_back({{at.x,at.y},at.z,{normal.x,normal.y},m_elapsed,level,true,species<0?int(CreatureKind::Mutant):species,0,int(ImpactMaterial::Flesh),(.11f+.05f*float(seed>>24&255u)/255.f)*std::min(1.5f,power)*(1.15f-.25f*fraction)});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());}
}
}
