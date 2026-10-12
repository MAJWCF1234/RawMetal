#include "Game.h"
#include "../core/Ray.h"
namespace retro {
namespace {
constexpr int GrenadeSlots=8,StepUs=8333;
constexpr float Gravity=9.81f,BlastRadius=3.5f;
StateId grenadeKey(int slot,std::string_view field){auto value="grenade/slot/"+std::to_string(slot)+"/"+std::string(field);StateId hash=2166136261u;for(unsigned char c:value)hash=(hash^c)*16777619u;return hash|0x80000000u;}
float size3(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
Point3 quant(Point3 p){return {std::round(p.x*1000)*.001f,std::round(p.y*1000)*.001f,std::round(p.z*1000)*.001f};}
struct Flight {Point3 p{},v{};};
template<class Trace>void flightStep(Flight&flight,float dt,Trace trace){
 flight.v.z-=Gravity*dt;auto end=flight.p+flight.v*dt;auto hit=trace(flight.p,end);
 if(hit.kind!=ShotKind::Solid){flight.p=end;}
 else{auto normal=hit.normal;float norm=size3(normal);if(norm<.1f){flight.v={};return;}normal=normal*(1/norm);flight.p=hit.point+normal*.05f;
  float along=flight.v.x*normal.x+flight.v.y*normal.y+flight.v.z*normal.z;if(along<0)flight.v=flight.v-normal*(1.35f*along);
  if(normal.z>.5f){flight.v.x*=.15f;flight.v.y*=.15f;if(std::fabs(flight.v.z)<.55f)flight.v.z=0;}else flight.v=flight.v*.6f;
 }
 flight.p=quant(flight.p);flight.v=quant(flight.v);
}
}
ShotHit Game::tacticalGrenadeTrace(Point3 from,Point3 to)const{
 auto delta=to-from;float range=size3(delta);ShotHit hit;hit.kind=ShotKind::None;hit.distance=range;hit.point=to;auto active=chunkOffset(m_level);
 if(!std::isfinite(range)||range<=.000001f)return hit;
 for(int level=0;level<chunkCount();++level){auto shift=active-chunkOffset(level);Point3 a=from+Point3{shift.x,shift.y,0},b=to+Point3{shift.x,shift.y,0},normal;
  float edge=rayBoxFraction(a,delta,{0,0,-1000},{24,24,1000},&normal);if(edge>=1)continue;
  float t=chunkResident(level)?(level==m_level?m_world:m_chunks[level].world).shotFraction(a,b,normal):edge;
  if(t*range<hit.distance)hit={ShotKind::Solid,level,-1,t*range,t*range,from+delta*t,normal};
 }return hit;
}
bool Game::spawnTacticalGrenade(Point3 origin,Point3 velocity,AiTargetRef attacker,StateId squad,AiFaction faction,int fuseMs){
 if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z)||!std::isfinite(size3(velocity))||size3(velocity)>24||fuseMs<1800||fuseMs>6000||int(faction)<0||int(faction)>5)return false;
 if(attacker.kind!=ShotKind::Player&&attacker.kind!=ShotKind::Npc&&attacker.kind!=ShotKind::Enemy&&attacker.kind!=ShotKind::Actor)return false;
 if(attacker.level<0||attacker.level>=chunkCount()||attacker.index<0||attacker.index>65535)return false;
 auto global=chunkOffset(m_level);Point3 p=origin+Point3{global.x,global.y,0};int level=friendlyChunkAt({p.x,p.y},m_level);if(level<0||!chunkResident(level)||std::fabs(p.z)>1000)return false;
 int slot=-1;for(int i=0;i<GrenadeSlots;++i)if(!state(grenadeKey(i,"active"))){slot=i;break;}if(slot<0)return false;
 auto put=[&](std::string_view f,int v){setState(grenadeKey(slot,f),v);};put("active",1);put("fuse_us",fuseMs*1000);put("fraction",0);put("x",int(std::round(p.x*1000)));put("y",int(std::round(p.y*1000)));put("z",int(std::round(p.z*1000)));
 put("vx",int(std::round(velocity.x*1000)));put("vy",int(std::round(velocity.y*1000)));put("vz",int(std::round(velocity.z*1000)));put("kind",int(attacker.kind));put("level",attacker.level+1);put("index",attacker.index+1);put("squad",int(squad));put("faction",int(faction));
 m_sounds.push_back({Sound::JunkMetal,{origin.x,origin.y},.45f,.9f,true,17,origin.z});return true;
}
std::vector<TacticalGrenadePose> Game::tacticalGrenades(bool dangerOnly)const{
 std::vector<TacticalGrenadePose> poses;for(int i=0;i<GrenadeSlots;++i)if(state(grenadeKey(i,"active"))){
  // A fast outgoing throw is not yet a local explosion warning. Slow
  // projectiles and the final fuse second produce the bounded danger cue.
  if(dangerOnly&&state(grenadeKey(i,"fuse_us"))>1000000&&size3({state(grenadeKey(i,"vx"))*.001f,state(grenadeKey(i,"vy"))*.001f,state(grenadeKey(i,"vz"))*.001f})>2.f)continue;
  Point3 global{state(grenadeKey(i,"x"))*.001f,state(grenadeKey(i,"y"))*.001f,state(grenadeKey(i,"z"))*.001f};int level=friendlyChunkAt({global.x,global.y},m_level);if(level<0)continue;auto offset=chunkOffset(level);poses.push_back({global-Point3{offset.x,offset.y,0},state(grenadeKey(i,"fuse_us"))*.000001f,level});}return poses;
}
void Game::explodeTacticalGrenade(int slot,Point3 globalPosition,AiTargetRef attacker){
 setState(grenadeKey(slot,"active"),0);auto active=chunkOffset(m_level);Point3 origin=globalPosition-Point3{active.x,active.y,0};int level=friendlyChunkAt({globalPosition.x,globalPosition.y},m_level);if(level<0)return;
 auto blast=[&](ShotKind kind,int targetLevel,int index,Vec2 position,float feet,float height){auto shift=chunkOffset(targetLevel)-active;Point3 point{position.x+shift.x,position.y+shift.y,feet+height*.5f},delta=point-origin;float range=size3(delta);if(range>=BlastRadius)return;
  if(range>.05f&&tacticalGrenadeTrace(origin,point).kind==ShotKind::Solid)return;float damage=64.f*(1-range/BlastRadius);auto direction=range>.001f?delta*(1/range):Point3{0,0,1};resolveShot({kind,targetLevel,index,range,range,point,{}},origin,direction,weaponDef(WeaponId::Shotgun).hitscan,attacker.kind==ShotKind::Player,damage,attacker);
 };
 if(!dead())blast(ShotKind::Player,m_level,0,m_player.pos,m_player.z,m_player.hullHeight());
 for(const auto&drone:engineerDrones())if(chunkResident(drone.level))blast(ShotKind::Drone,drone.level,drone.slot,{drone.position.x,drone.position.y},drone.position.z,EngineerDroneHeight);
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(chunkResident(npc.level)&&autonomousActor(t)&&t.health&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState)))blast(ShotKind::Npc,npc.level,int(i),npc.pose.position,npc.pose.z,t.scale);}
 for(int targetLevel=0;targetLevel<chunkCount();++targetLevel)if(chunkResident(targetLevel)){auto&enemies=targetLevel==m_level?m_enemies:m_chunks[targetLevel].enemies;for(size_t i=0;i<enemies.size();++i){const auto&e=enemies[i];if(e.alive)blast(ShotKind::Enemy,targetLevel,int(i),e.pos,e.bodyBottom(),e.bodyTop()-e.bodyBottom());}
  const auto&world=friendlyWorld(targetLevel);for(size_t i=0;i<world.actorTracks().size();++i){const auto&t=world.actorTracks()[i];const auto&p=world.actorPose(i);if(t.ai.mode==ActorAiMode::Scripted&&!t.platform&&t.health&&p.clip!=4&&(!t.deadState||!state(t.deadState))&&(!t.ai.enableState||state(t.ai.enableState)))blast(ShotKind::Actor,targetLevel,int(i),p.position,p.z,t.scale);}
 }
 auto offset=chunkOffset(level);m_barrelExplosions.push_back({{globalPosition.x-offset.x,globalPosition.y-offset.y},globalPosition.z,m_elapsed,level});if(m_barrelExplosions.size()>16)m_barrelExplosions.erase(m_barrelExplosions.begin());m_sounds.push_back({Sound::LiftCrash,{origin.x,origin.y},1,.9f,true,20,origin.z});
 m_guardShots.push_back({{globalPosition.x,globalPosition.y},globalPosition.z,20,1,true,attacker.kind==ShotKind::Npc?attacker.index:-1});
}
void Game::updateTacticalGrenades(float dt){
 if(!std::isfinite(dt)||dt<=0)return;dt=std::min(dt,.05f);
 auto active=chunkOffset(m_level);for(int slot=0;slot<GrenadeSlots;++slot){auto get=[&](std::string_view f){return state(grenadeKey(slot,f));};auto put=[&](std::string_view f,int v){setState(grenadeKey(slot,f),v);};if(!get("active"))continue;
  Flight flight{{get("x")*.001f-active.x,get("y")*.001f-active.y,get("z")*.001f},{get("vx")*.001f,get("vy")*.001f,get("vz")*.001f}};int pending=get("fraction")+int(std::round(dt*1000000)),fuse=get("fuse_us");
  while(pending>=StepUs&&fuse>0){flightStep(flight,StepUs*.000001f,[&](Point3 a,Point3 b){return tacticalGrenadeTrace(a,b);});pending-=StepUs;fuse=std::max(0,fuse-StepUs);}
  auto global=flight.p+Point3{active.x,active.y,0};put("x",int(std::round(global.x*1000)));put("y",int(std::round(global.y*1000)));put("z",int(std::round(global.z*1000)));put("vx",int(std::round(flight.v.x*1000)));put("vy",int(std::round(flight.v.y*1000)));put("vz",int(std::round(flight.v.z*1000)));put("fraction",pending);put("fuse_us",fuse);
  if(fuse==0)explodeTacticalGrenade(slot,global,{ShotKind(get("kind")),get("level")-1,get("index")-1});
 }
}
void Game::tryGuardGrenade(size_t actor,float dt){
 auto&npc=m_friendlyActors[actor];const auto&t=npc.track();const auto&c=t.ai;if(c.mode!=ActorAiMode::Guard||c.grenades==0)return;
 auto get=[&](std::string_view f){return state(friendlyKey(actor,f));};auto put=[&](std::string_view f,int v){setState(friendlyKey(actor,f),v);};int ms=npcMilliseconds(actor,dt,"grenade_fraction");put("grenade_cooldown",std::max(0,get("grenade_cooldown")-ms));
 auto target=guardTarget(actor);bool same=target.valid()&&get("grenade_target_kind")==int(target.kind)&&get("grenade_target_level")==target.level+1&&get("grenade_target_index")==target.index+1;
 if(!same){put("grenade_seen",0);put("grenade_memory_ms",2000);put("grenade_windup",0);put("grenade_target_kind",target.valid()?int(target.kind):0);put("grenade_target_level",target.valid()?target.level+1:0);put("grenade_target_index",target.valid()?target.index+1:0);}
 bool seen=target.valid()&&get("guard_seen")&&guardCanSee(actor,npc.pose,target);if(seen){put("grenade_seen",1);put("grenade_memory_ms",0);}else put("grenade_memory_ms",std::min(2000,get("grenade_memory_ms")+ms));
 bool ready=!get("guard_danger_ms")&&!get("drone_busy")&&!get("support_target")&&chunkResident(npc.level)&&autonomousActor(t)&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState))&&get("guard_confirmed")&&get("guard_memory")&&target.valid()&&get("grenade_seen")&&get("grenade_memory_ms")<=1500&&get("grenade_used")<c.grenades&&!get("grenade_cooldown")&&npc.pose.clip!=1&&!get("weapon_reload")&&!get("weapon_reloading")&&!get("weapon_recovery")&&!get("weapon_bash")&&!get("weapon_bash_pose")&&get("weapon_action")!=int(GuardAction::Bash)&&get("weapon_action")!=int(GuardAction::Fire);
 for(int slot=0;slot<GrenadeSlots;++slot)if(state(grenadeKey(slot,"active"))&&((c.squad&&state(grenadeKey(slot,"squad"))==int(c.squad)&&state(grenadeKey(slot,"faction"))==int(actorFaction(t)))||(state(grenadeKey(slot,"kind"))==int(ShotKind::Npc)&&state(grenadeKey(slot,"index"))==int(actor)+1)))ready=false;
 auto shift=chunkOffset(npc.level)-chunkOffset(m_level);Point3 origin{npc.pose.position.x+shift.x,npc.pose.position.y+shift.y,npc.pose.z+t.scale*.7f};Vec2 remembered{get("guard_x")*.001f-chunkOffset(m_level).x,get("guard_y")*.001f-chunkOffset(m_level).y};Point3 landing{remembered.x,remembered.y,get("guard_z")*.001f+.075f};float distance=std::hypot(landing.x-origin.x,landing.y-origin.y);ready&=distance>=4.75f&&distance<=11.f&&std::fabs(landing.z-origin.z)<3;
 Point3 velocity=(landing-origin)*(1/1.05f);velocity.z+=Gravity*1.05f*.5f;auto active=chunkOffset(m_level);Flight predicted{{get("grenade_land_x")*.001f-active.x,get("grenade_land_y")*.001f-active.y,get("grenade_land_z")*.001f},velocity};
 // Predict once before the telegraph and recheck at release. The expensive
 // bounded arc query shares the existing four-search frame budget.
 if(ready&&(get("grenade_windup")==0||get("grenade_windup")+ms>=450)){if(m_npcNavBudget<=0)ready=false;else{--m_npcNavBudget;predicted.p=origin;for(int step=0;step<289;++step)flightStep(predicted,StepUs*.000001f,[&](Point3 a,Point3 b){return tacticalGrenadeTrace(a,b);});auto global=predicted.p+Point3{active.x,active.y,0};put("grenade_land_x",int(std::round(global.x*1000)));put("grenade_land_y",int(std::round(global.y*1000)));put("grenade_land_z",int(std::round(global.z*1000)));}}
 auto protectedBody=[&](Vec2 position,float feet,float height,AiFaction faction){if(faction!=AiFaction::Neutral&&factionsHostile(actorFaction(t),faction))return;Point3 center{position.x,position.y,feet+height*.5f};if(size3(center-predicted.p)<4.5f)ready=false;};
 if(ready){protectedBody(m_player.pos,m_player.z,m_player.hullHeight(),AiFaction::Player);for(size_t i=0;i<m_friendlyActors.size();++i){const auto&other=m_friendlyActors[i];const auto&track=other.track();if(!chunkResident(other.level)||!autonomousActor(track)||other.pose.clip==4||(track.deadState&&state(track.deadState)))continue;auto offset=chunkOffset(other.level)-chunkOffset(m_level);protectedBody(other.pose.position+offset,other.pose.z,track.scale,actorFaction(track));}
  for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&world=friendlyWorld(level);auto offset=chunkOffset(level)-chunkOffset(m_level);for(size_t i=0;i<world.actorTracks().size();++i){const auto&track=world.actorTracks()[i];const auto&pose=world.actorPose(i);if(track.ai.mode==ActorAiMode::Scripted&&pose.clip!=4&&(!track.deadState||!state(track.deadState))&&(!track.ai.enableState||state(track.ai.enableState)))protectedBody(pose.position+offset,pose.z,track.scale,actorFaction(track));}}
 }
 if(ready)for(const auto&drone:engineerDrones()){auto offset=chunkOffset(drone.level)-chunkOffset(m_level);auto target=engineerDroneTarget(drone.slot,m_level);if(target.valid())protectedBody(Vec2{drone.position.x,drone.position.y}+offset,drone.position.z,EngineerDroneHeight,target.faction);}
 if(!ready){put("grenade_windup",0);return;}int windup=get("grenade_windup");if(!windup&&ms>0)m_sounds.push_back({Sound::Pump,{origin.x,origin.y},.4f,.65f,true,17,origin.z});windup=std::min(450,windup+ms);put("grenade_windup",windup);if(windup<450)return;
 if(!spawnTacticalGrenade(origin,velocity,{ShotKind::Npc,npc.level,int(actor)},c.squad,actorFaction(t)))return;
 put("grenade_used",get("grenade_used")+1);put("grenade_windup",0);put("grenade_cooldown",6000);put("weapon_aim",0);put("weapon_recovery",std::max(450,get("weapon_recovery")));
 if(c.squad)for(size_t i=0;i<m_friendlyActors.size();++i)if(m_friendlyActors[i].track().ai.squad==c.squad&&actorFaction(m_friendlyActors[i].track())==actorFaction(t))setState(friendlyKey(i,"grenade_cooldown"),6000);
}
void Game::validateTacticalGrenades()const{
 for(int slot=0;slot<GrenadeSlots;++slot){auto get=[&](std::string_view f){return state(grenadeKey(slot,f));};if(get("active")<0||get("active")>1)throw std::runtime_error("Invalid saved grenade active flag");if(!get("active"))continue;
  if(get("fuse_us")<=0||get("fuse_us")>6000000||get("fraction")<0||get("fraction")>=StepUs||get("faction")<0||get("faction")>5||get("squad")<0||get("squad")>0x7fffff||get("level")<=0||get("level")>chunkCount()||get("index")<=0||get("index")>65536)throw std::runtime_error("Invalid saved grenade clock or owner");
  int kind=get("kind");if(kind!=int(ShotKind::Player)&&kind!=int(ShotKind::Npc)&&kind!=int(ShotKind::Enemy)&&kind!=int(ShotKind::Actor))throw std::runtime_error("Invalid saved grenade attacker");
  int owner=get("index")-1,level=get("level")-1;if((kind==int(ShotKind::Player)&&owner!=0)||(kind==int(ShotKind::Npc)&&size_t(owner)>=m_friendlyActors.size())||(kind==int(ShotKind::Actor)&&size_t(owner)>=(level==m_level?m_world:m_chunks[level].world).actorTracks().size())||(kind==int(ShotKind::Enemy)&&size_t(owner)>=(level==m_level?m_enemies:m_chunks[level].enemies).size()))throw std::runtime_error("Saved grenade attacker is outside its entity collection");
  for(auto f:{"x","y","z"})if(get(f)< -10000000||get(f)>10000000)throw std::runtime_error("Invalid saved grenade position");for(auto f:{"vx","vy","vz"})if(get(f)< -100000||get(f)>100000)throw std::runtime_error("Invalid saved grenade velocity");
  if(friendlyChunkAt({get("x")*.001f,get("y")*.001f},m_level)<0)throw std::runtime_error("Saved grenade outside authored world");
 }
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){auto get=[&](std::string_view f){return state(friendlyKey(actor,f));};const auto&c=m_friendlyActors[actor].track().ai;
  if(get("grenade_used")<0||get("grenade_used")>c.grenades||get("grenade_cooldown")<0||get("grenade_cooldown")>6000||get("grenade_windup")<0||get("grenade_windup")>450||get("grenade_fraction")<0||get("grenade_fraction")>=1000||get("grenade_memory_ms")<0||get("grenade_memory_ms")>2000||get("grenade_seen")<0||get("grenade_seen")>1)throw std::runtime_error("Invalid saved guard grenade policy");
  for(auto f:{"grenade_land_x","grenade_land_y","grenade_land_z"})if(get(f)< -10000000||get(f)>10000000)throw std::runtime_error("Invalid saved grenade landing");
 }
}
void Game::guardAvoidGrenade(size_t actor,const ActorPose&pose,Vec2&goal,float&goalZ,bool&wantsMove){
 if(!wantsMove)return;const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();auto offset=chunkOffset(npc.level);
 for(int slot=0;slot<GrenadeSlots;++slot){if(!state(grenadeKey(slot,"active"))||state(grenadeKey(slot,"faction"))!=int(actorFaction(track)))continue;
  Point3 landing{state(grenadeKey(slot,"x"))*.001f-offset.x,state(grenadeKey(slot,"y"))*.001f-offset.y,state(grenadeKey(slot,"z"))*.001f};
  if(state(grenadeKey(slot,"kind"))==int(ShotKind::Npc)){int owner=state(grenadeKey(slot,"index"))-1;if(owner>=0&&size_t(owner)<m_friendlyActors.size()){landing.x=state(friendlyKey(size_t(owner),"grenade_land_x"))*.001f-offset.x;landing.y=state(friendlyKey(size_t(owner),"grenade_land_y"))*.001f-offset.y;landing.z=state(friendlyKey(size_t(owner),"grenade_land_z"))*.001f;}}
  Vec2 hazard{landing.x,landing.y};if(std::fabs(pose.z-landing.z)>3.5f||lengthSq(goal-hazard)>=4.5f*4.5f)continue;
  auto away=normalized(pose.position-hazard);if(lengthSq(away)<.001f)away={std::sin(pose.yaw),std::cos(pose.yaw)};
  if(lengthSq(pose.position-hazard)>=4.5f*4.5f){wantsMove=false;return;}
  auto safe=hazard+away*4.75f;safe.x=std::clamp(safe.x,.35f,23.65f);safe.y=std::clamp(safe.y,.35f,23.65f);float feet=pose.z;
  if(m_npcNavBudget>0){--m_npcNavBudget;if(lengthSq(safe-hazard)>=4.5f*4.5f&&friendlyWalkSegment(npc.level,pose.position,pose.z,safe,track.scale,&feet)){goal=safe;goalZ=feet;return;}}
  wantsMove=false;return;
 }
}
}
