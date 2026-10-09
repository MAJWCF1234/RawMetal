#include "Game.h"
#include "AINavigation.h"
#include "AIPerception.h"
#include <optional>
#include <fstream>
namespace retro {
void Game::damageFriendly(size_t actor,float amount,Vec2 source){
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();auto pose=npc.pose;
 if(!track.health||state(track.deadState)||amount<=0)return;
 int damage=std::min(track.health,state(track.damageState)+int(std::ceil(amount)));setState(track.damageState,damage);
 setState(friendlyKey(actor,"hit_x"),int(std::round(source.x*1000)));setState(friendlyKey(actor,"hit_y"),int(std::round(source.y*1000)));setState(friendlyKey(actor,"hit_source"),1);
 if(damage>=track.health){setState(track.deadState,1);setState(actorPositionState(track.deadState,0),int(std::round(pose.position.x*1000)));setState(actorPositionState(track.deadState,1),int(std::round(pose.position.y*1000)));setState(actorPositionState(track.deadState,2),int(std::round(pose.z*1000)));updateMechanisms(0);}
 m_sounds.push_back({damage>=track.health?Sound::WorkerDying:Sound::Hurt,pose.position+chunkOffset(npc.level)-chunkOffset(m_level),.7f,1,true});
}
bool Game::aiDoorLocked(const World&world,int level,const Door&door)const{
 int living=level==m_level?enemiesRemaining():int(std::count_if(m_chunks[level].enemies.begin(),m_chunks[level].enemies.end(),[](const Enemy&e){return e.alive;}));
 return door.transfer||door.entry||(door.requireEnemiesClear&&living)||(door.requireControl&&!world.controlReleased())||(door.requireState&&state(door.requireState)!=door.requireValue);
}
void Game::requestAiDoor(World& world,int level,Vec2 position,float feet,Vec2 goal){
 for(size_t i=0;i<world.doors().size();++i){const auto&door=world.doors()[i];
  if(aiDoorLocked(world,level,door)||door.opening||std::fabs(position.y-door.y)>1.8f||position.x<door.left+.2f||position.x>door.right-.2f||(position.y-door.y)*(goal.y-door.y)>=0)continue;
  float z=world.floorHeight((door.left+door.right)*.5f,door.y)+door.z;if(std::fabs(feet-z)>.3f)continue;
  Vec2 face{position.x,door.y+std::copysign(.2f,position.y-door.y)};
  if(world.rayClear(position,feet+.7f,face,feet+.7f))world.openDoor(int(i));
 }
}
ActorPose Game::updateFriendlyAI(World& world,int level,size_t actor,const ActorTrack& track,const ActorPose& old,ActorPose pose,float dt){
 const auto&config=track.ai;
 if(config.mode==ActorAiMode::Scripted)return pose;
 auto key=[&](std::string_view field){return friendlyKey(actor,field);};
 auto get=[&](std::string_view field){return state(key(field));};auto put=[&](std::string_view field,int value){setState(key(field),value);};
 if(track.deadState&&state(track.deadState)){
  pose.position={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f};pose.z=state(actorPositionState(track.deadState,2))*.001f;pose.clip=4;
  int age=std::clamp(get("death_ms"),0,1000);if(dt>0)age=std::min(1000,age+std::max(1,int(std::round(dt*1000))));put("death_ms",age);pose.phase=age*.001f;return pose;
 }
 if(pose.clip==4)return pose;
 if(config.enableState&&!state(config.enableState)){put("running",0);return pose;}
 if(get("saved")){
  if(dt==0||!get("running")){pose.position={get("x")*.001f,get("y")*.001f};pose.z=get("z")*.001f;pose.yaw=get("yaw")*.0001f;pose.phase=get("phase")*.0001f;pose.clip=get("walking")?1:0;
   if(pose.position.x<0||pose.position.y<0||pose.position.x>=24||pose.position.y>=24||std::fabs(pose.z)>100||std::fabs(pose.yaw)>kPi+.001f||pose.phase<0||pose.phase>1)throw std::runtime_error("Invalid saved NPC pose");
  }
  else pose=old;
 }
 pose.lookAtActor=-1;
 if(dt<=0)return pose;
 put("running",1);float height=track.scale;int ms=std::max(1,int(std::round(dt*1000)));
 int fear=std::max(0,std::clamp(get("fear"),0,4000)-ms);Vec2 threat{get("threat_x")*.001f,get("threat_y")*.001f};float nearest=config.dangerRange*config.dangerRange;bool danger=false;
 auto consider=[&](Vec2 position,float z){float distance=lengthSq(position-pose.position);
  if(distance<nearest&&aiCanSee(world,pose.position,pose.z+height*.85f,{std::sin(pose.yaw),std::cos(pose.yaw)},position,z,config.dangerRange,-.25f,2.5f)){nearest=distance;threat=position;danger=true;}};
 const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
 for(const auto&enemy:enemies)if(enemy.alive)consider(enemy.pos,(enemy.bodyBottom()+enemy.bodyTop())*.5f);
 for(size_t i=0;i<world.actorTracks().size();++i){const auto&other=world.actorTracks()[i];if(other.visual!=ActorVisual::Wasp&&other.visual!=ActorVisual::Huntsman)continue;
  const auto&target=world.actorPose(i);if(target.clip!=4&&(!other.deadState||!state(other.deadState)))consider(target.position,target.z+other.scale*.5f);}
 int damage=track.damageState?state(track.damageState):0;if(damage>get("damage")&&level==m_level){danger=true;threat=get("hit_source")?Vec2{get("hit_x")*.001f,get("hit_y")*.001f}:m_player.pos;}put("damage",damage);put("hit_source",0);
 if(danger){fear=4000;put("threat_x",int(std::round(threat.x*1000)));put("threat_y",int(std::round(threat.y*1000)));}put("fear",fear);
 bool follow=config.mode==ActorAiMode::Escort?!get("hold"):get("follow")!=0;
 Vec2 goal=pose.position;float goalZ=pose.z;bool wantsMove=false;
 if(fear>0&&lengthSq(pose.position-threat)<25.f){
  Vec2 away=normalized(pose.position-threat);if(lengthSq(away)<.001f)away={std::sin(pose.yaw),std::cos(pose.yaw)};
  float best=-1e9f;
  for(float angle:{0.f,.8f,-.8f,1.6f,-1.6f}){float c=std::cos(angle),s=std::sin(angle);auto direction=Vec2{away.x*c-away.y*s,away.x*s+away.y*c};auto candidate=pose.position+direction*3.f;
   int destination=friendlyChunkAt(candidate+chunkOffset(level),level);if(destination<0||!friendlyBoundaryOpen(level,destination,candidate+chunkOffset(level)))continue;
   ensureChunk(destination);auto local=candidate+chunkOffset(level)-chunkOffset(destination);const auto&targetWorld=friendlyWorld(destination);
   float z=targetWorld.supportBelow(local.x,local.y,pose.z+.25f);if(!targetWorld.fits(local.x,local.y,z,height))continue;
   float score=lengthSq(candidate-threat);if(friendlyWalkSegment(level,pose.position,pose.z,pose.position+direction*.7f,height))score+=10.f;
   if(score>best){best=score;goal=candidate;goalZ=z;wantsMove=true;}}
 }else if(fear==0&&follow&&get("engaged")){
  if(level==m_level){put("route_next",0);float distance=length(pose.position-m_player.pos);wantsMove=distance>config.followDistance+(get("walking")?-.3f:.3f);if(wantsMove){goal=m_player.pos;goalZ=m_player.z;}}
  else{
   int next=get("route_next")-1,wait=std::max(0,get("route_wait")-ms);Vec2 portal{get("portal_x")*.001f,get("portal_y")*.001f};float z=get("portal_z")*.001f;
   bool cached=next>=0&&next<chunkCount()&&next!=level&&get("route_player")==m_level+1&&wait>0&&friendlyBoundaryOpen(level,next,portal+chunkOffset(level));
   if(!cached&&m_npcNavBudget>0&&(wait<=0||get("route_player")!=m_level+1||next>=0)){--m_npcNavBudget;cached=friendlyPortal(level,m_level,pose.position,pose.z,height,portal,z);next=cached?friendlyChunkAt(portal+chunkOffset(level),level):-1;wait=600+int(actor%7)*30;put("portal_x",int(std::round(portal.x*1000)));put("portal_y",int(std::round(portal.y*1000)));put("portal_z",int(std::round(z*1000)));put("route_player",m_level+1);put("route_next",next+1);}
   put("route_wait",wait);if(cached){goal=portal;goalZ=z;wantsMove=true;}
  }
 }
 float timer=std::max(0.f,std::clamp(get("repath"),0,4000)*.001f-dt);Vec2 waypoint=get("saved")?Vec2{get("way_x")*.001f,get("way_y")*.001f}:pose.position;
 if(lengthSq(goal-Vec2{get("goal_x")*.001f,get("goal_y")*.001f})>1.f)timer=0;
 put("goal_x",int(std::round(goal.x*1000)));put("goal_y",int(std::round(goal.y*1000)));
 if(wantsMove){
  requestAiDoor(world,level,pose.position,pose.z,goal);
  auto toward=goal-pose.position;float distance=length(toward);auto probe=pose.position+toward*(std::min(1.f,distance)/std::max(.001f,distance));
  if((world.hasTerrain()||std::fabs(goalZ-pose.z)<.3f)&&friendlyWalkSegment(level,pose.position,pose.z,probe,height))waypoint=goal;
  else if((timer<=0||(get("walking")&&lengthSq(waypoint-pose.position)<.04f))&&m_npcNavBudget>0){
   --m_npcNavBudget;
   // Plan through doors this actor may operate; actual movement still collides
   // until a nearby request has physically opened the door.
   std::optional<World> planning;
   for(size_t i=0;i<world.doors().size();++i)if(!aiDoorLocked(world,level,world.doors()[i])&&world.doors()[i].open<world.doors()[i].maxOpen){if(!planning)planning=world;planning->setDoor(int(i),world.doors()[i].maxOpen,true);}
   waypoint=aiWalkWaypoint(planning?*planning:world,pose.position,pose.z,goal,goalZ,height);timer=.3f+float((actor*17+size_t(level)*5)%11)*.02f;
  }
  auto direction=normalized(waypoint-pose.position);Vec2 separation{};
  for(size_t i=0;i<m_friendlyActors.size();++i)if(i!=actor&&m_friendlyActors[i].level==level&&autonomousActor(m_friendlyActors[i].track())){const auto&other=m_friendlyActors[i].pose;if(other.clip==4||std::fabs(other.z-pose.z)>height)continue;
   auto away=pose.position-other.position;float neighborDistance=length(away);if(neighborDistance>.001f&&neighborDistance<.7f)separation+=away*((.7f-neighborDistance)/neighborDistance);
  }
  direction=normalized(direction+separation*2.f);float travel=std::min(length(waypoint-pose.position),config.speed*(fear>0?1.5f:1.f)*dt);
  auto next=pose.position+direction*travel;float feet=pose.z;
  bool clear=travel>0&&friendlyWalkSegment(level,pose.position,pose.z,next,height,&feet);
  auto player=m_player.pos+chunkOffset(m_level)-chunkOffset(level);
  if(std::fabs(next.x-player.x)<.45f&&std::fabs(next.y-player.y)<.45f&&lengthSq(next-player)<lengthSq(pose.position-player))clear=false;
  for(const auto&enemy:enemies)if(enemy.alive&&enemy.bodyBottom()<feet+height&&enemy.bodyTop()>feet&&lengthSq(next-enemy.pos)<.45f*.45f&&lengthSq(next-enemy.pos)<lengthSq(pose.position-enemy.pos))clear=false;
  if(clear){pose.position=next;pose.z=feet;pose.yaw=old.yaw+std::clamp(std::remainder(std::atan2(direction.x,direction.y)-old.yaw,2*kPi),-6.f*dt,6.f*dt);}
 }
 float moved=length(pose.position-old.position);pose.clip=moved>.0001f?1:0;pose.phase=pose.clip?std::fmod(old.phase+moved*1.1f,1.f):0;
 int stuck=wantsMove&&moved<.0001f?std::clamp(get("stuck"),0,500)+ms:0;if(stuck>500){timer=0;stuck=0;}put("stuck",stuck);
 put("repath",int(std::round(timer*1000)));put("way_x",int(std::round(waypoint.x*1000)));put("way_y",int(std::round(waypoint.y*1000)));
 put("x",int(std::round(pose.position.x*1000)));put("y",int(std::round(pose.position.y*1000)));put("z",int(std::round(pose.z*1000)));put("yaw",int(std::round(std::remainder(pose.yaw,2*kPi)*10000)));put("phase",int(std::round(pose.phase*10000)));put("walking",pose.clip==1);put("saved",1);
 return pose;
}
int Game::nearbyFriendly()const{
 int best=-1;float nearest=1.8f;Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();auto pose=npc.pose;pose.position+=chunkOffset(npc.level)-chunkOffset(m_level);
  if(track.ai.mode==ActorAiMode::Scripted||(track.ai.enableState&&!state(track.ai.enableState))||pose.clip==4||(track.deadState&&state(track.deadState)))continue;
  auto delta=pose.position-m_player.pos;float distance=length(delta);
  Point3 target{pose.position.x+chunkOffset(m_level).x,pose.position.y+chunkOffset(m_level).y,pose.z+track.scale*.7f};
  if(!friendlyRayClear({m_player.pos.x+chunkOffset(m_level).x,m_player.pos.y+chunkOffset(m_level).y,m_player.z+m_player.eye},target))continue;
  if(m_vrInputActive){if(distance>2.4f||!m_vrHandValid[m_vrInteractionHand])continue;auto hand=m_vrHandPosition[m_vrInteractionHand],to=Point3{pose.position.x,pose.position.y,target.z}-hand;distance=std::sqrt(to.x*to.x+to.y*to.y+to.z*to.z);auto direction=m_vrHandDirection[m_vrInteractionHand];float magnitude=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
   if(distance>.65f&&(magnitude<.001f||to.x*direction.x+to.y*direction.y+to.z*direction.z<distance*magnitude*.94f))continue;
   if(!friendlyRayClear({hand.x+chunkOffset(m_level).x,hand.y+chunkOffset(m_level).y,hand.z},target))continue;
  }else if(dot(normalized(delta),forward)<.6f||std::fabs(pose.z-m_player.z)>.6f)continue;
  if(distance<nearest){best=int(i);nearest=distance;}
 }return best;
}
bool Game::commandFriendly(){
 int actor=nearbyFriendly();if(actor<0)return false;const auto&track=m_friendlyActors[size_t(actor)].track();bool escort=track.ai.mode==ActorAiMode::Escort;
 auto key=friendlyKey(size_t(actor),escort?"hold":"follow");setState(key,!state(key));
 bool follow=escort?!state(key):state(key)!=0;m_pickupNotice=follow?"WORKER / FOLLOWING":"WORKER / HOLDING POSITION";m_pickupNoticeTime=2;return true;
}
bool Game::testFriendlyAI(){
 std::ofstream report("friendly-ai-test.txt");bool passed=true;auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;};
 auto scene=[](ActorAiMode mode,bool wall=false,int door=0){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Friendly AI validation";campaign->key=192837;
  auto map=std::make_shared<AuthoredMapData>();map->name="NPC validation room";map->definition={{0,0},{16.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');
  if(wall)for(int y=0;y<11;++y)layer.rows[y][10]='#';
  if(door){layer.rows[8]=std::string(24,'#');layer.rows[8][6]='.';Door gate{6,7,8,0,false,false,false};if(door==2)gate.requireState=stateId("npc_gate_permission");map->doors.push_back(gate);}
  map->layers.push_back(layer);TimedSequence timer;timer.timerState=stateId("npc_clock");timer.finishState=stateId("npc_clock_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  ActorTrack worker;worker.timerState=timer.timerState;worker.keys={{0,{6.5f,6.5f},0,0},{30000,{6.5f,6.5f},0,0}};worker.ai.mode=mode;worker.health=70;worker.damageState=stateId("npc_damage");worker.deadState=stateId("npc_dead");map->actorTracks.push_back(worker);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int count,float dt){for(int i=0;i<count;++i){g.updateStreaming(dt);g.updateMechanisms(dt);g.m_world.updateDoors(dt);}};
 {auto g=scene(ActorAiMode::Civilian);check(!g.hullFits({6.5f,6.5f},0,Player::StandingHeight)&&g.hullFits({6.5f,6.5f},3,Player::StandingHeight),"Living NPC body blocks player/enemy hulls without blocking another floor");}
 for(int rate:{60,120}){auto g=scene(ActorAiMode::Escort);tick(g,rate*8,1.f/rate);auto pose=g.world().actorPose(0);
  check(length(pose.position-g.player().pos)<2.4f&&length(pose.position-g.player().pos)>1.2f,"Escort follows and stops outside the player hull at both frame rates");
  Game restored(g.m_customCampaigns.back());check(restored.decodeSave(g.encodeSave())&&length(restored.world().actorPose(0).position-pose.position)<.002f,"Save restores autonomous NPC position without timeline reset");
 }
 {auto g=scene(ActorAiMode::Civilian);tick(g,60,1.f/60);check(length(g.world().actorPose(0).position-Vec2{6.5f,6.5f})<.001f,"Civilian waits until asked to follow");
  g.m_player.pos={7.5f,6.5f};g.m_player.angle=kPi;InputState use{};use.use=true;g.updateInteraction(use,.01f);g.updateInteraction({},.01f);g.m_player.pos={12.5f,6.5f};tick(g,240,1.f/60);
  check(g.world().actorPose(0).position.x>8.5f,"PC use command enables civilian following");
  auto position=g.world().actorPose(0).position;g.m_player.pos=position+Vec2{1,0};g.setVrHand(0,{position.x+.4f,position.y,1.2f},{-1,0,0},true);g.m_vrInteractionHand=0;
  check(g.commandFriendly(),"VR controller reach accepts Follow/Wait command");tick(g,120,1.f/60);check(length(g.world().actorPose(0).position-position)<.001f,"Wait command stops following");
 }
 {auto g=scene(ActorAiMode::Escort,true);tick(g,1200,1.f/60);check(g.world().actorPose(0).position.x>12&&length(g.world().actorPose(0).position-g.player().pos)<2.5f,"Escort routes around a wall instead of pushing into it");}
 for(bool unreachable:{false,true}){auto original=scene(ActorAiMode::Escort,true);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto worker=map->actorTracks[0];map->actorTracks.clear();
  if(unreachable)for(int y=11;y<24;++y)map->layers[0].rows[y][10]='#';
  for(int i=0;i<9;++i){auto npc=worker;for(auto&key:npc.keys)key.position={9.3f,6.5f+i*.25f};map->actorTracks.push_back(npc);}campaign->maps[0]=map;Game crowd(campaign);crowd.updateMechanisms(1.f/60);
  int planned=0;bool stationary=true;for(size_t i=0;i<9;++i){planned+=crowd.state(actorAiState(0,i,worker.timerState,"repath"))>0;if(i>=4)stationary&=length(crowd.world().actorPose(i).position-map->actorTracks[i].keys[0].position)<.001f;}
  check(planned==4&&crowd.m_npcNavBudget==0&&stationary,"Crowded scene limits route searches and keeps deferred actors stationary");
  crowd.updateMechanisms(1.f/60);crowd.updateMechanisms(1.f/60);planned=0;for(size_t i=0;i<9;++i)planned+=crowd.state(actorAiState(0,i,worker.timerState,"repath"))>0;
  check(planned==9,"Reachable and unreachable goals do not starve later actors");
 }
 for(int locked:{0,1}){auto g=scene(ActorAiMode::Escort,false,locked?2:1);g.m_player.pos={6.5f,11.5f};tick(g,480,1.f/60);
  check(locked?(!g.world().doors()[0].opening&&g.world().actorPose(0).position.y<8):(g.world().doors()[0].opening&&g.world().actorPose(0).position.y>8.5f),"AI opens ordinary doors and preserves locked-door requirements");
 }
 {auto g=scene(ActorAiMode::Civilian);Enemy threat{};threat.pos={4.5f,7.5f};g.m_enemies.push_back(threat);float before=length(g.world().actorPose(0).position-threat.pos);tick(g,120,1.f/60);
  auto position=g.world().actorPose(0).position;check(length(position-threat.pos)>before+1.f,"Civilian flees a visible enemy");
  g.m_enemies[0].alive=false;Game restored(g.m_customCampaigns.back());check(restored.decodeSave(g.encodeSave()),"Save restores fear memory");tick(restored,60,1.f/60);check(length(restored.world().actorPose(0).position-threat.pos)>length(position-threat.pos),"Civilian continues escape briefly after losing the threat");
 }
 {auto g=scene(ActorAiMode::Escort);tick(g,120,1.f/60);auto position=g.world().actorPose(0).position;g.m_player.pos=position+Vec2{2,0};g.m_player.angle=kPi;g.m_player.pitch=0;g.shoot();g.shoot();g.shoot();g.updateMechanisms(.4f);
  auto phase=g.world().actorPose(0).phase;Game restored(g.m_customCampaigns.back());check(phase>0&&phase<1&&restored.decodeSave(g.encodeSave())&&std::fabs(restored.world().actorPose(0).phase-phase)<.002f,"Death animation progresses and restores mid-animation");tick(g,120,1.f/60);
  check(g.world().actorPose(0).clip==4&&length(g.world().actorPose(0).position-position)<.002f,"Killed autonomous worker freezes at actual death position");
 }
 {auto g=scene(ActorAiMode::Civilian);Enemy attacker{};attacker.pos={5.8f,6.5f};attacker.heading=0;g.m_enemies.push_back(attacker);
  g.updateEnemies(.01f);check(g.m_enemies[0].windup>0,"Enemy acquires a nearby friendly NPC");
  for(int i=0;i<45;++i)g.updateEnemies(1.f/60);check(g.state("npc_damage")>0&&g.player().health==100,"Enemy contact attack damages the NPC, not a distant player");
  g.damageFriendly(0,100,g.m_enemies[0].pos);g.updateEnemies(.01f);check(g.state("npc_dead")&&g.world().actorPose(0).clip==4&&g.kills()==0,"NPC death clears targeting without awarding a player kill");
 }
 for(bool south:{false,true})for(bool blocked:{false,true}){
  auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int index=0;index<3;++index){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);int coordinate=index==0?0:index==1?2:1;map->definition.origin=south?Vec2{0,float(coordinate*24)}:Vec2{float(coordinate*24),0};map->definition.residencyGroup=100+index;
   if(index){map->actorTracks.clear();map->timedSequences.clear();}else for(auto&key:map->actorTracks[0].keys)key.position=south?Vec2{6.5f,22.3f}:Vec2{22.3f,6.5f};
   map->openEast=!south&&(index!=1)&&!(blocked&&index==0);map->openWest=!south&&index!=0;
   map->openSouth=south&&(index!=1)&&!(blocked&&index==0);map->openNorth=south&&index!=0;
   campaign->maps.push_back(map);
  }
  Game g(campaign);g.m_player.pos=south?Vec2{6.5f,52.5f}:Vec2{52.5f,6.5f};g.crossChunkBoundary();g.updateStreaming(0);tick(g,360,1.f/60);
  if(blocked){check(g.friendlyActors()[0].level==0,"Closed stitched edge stops the escort without teleporting");continue;}
  check(g.friendlyActors().size()==1&&g.friendlyActors()[0].level==2,"Escort physically enters non-sequential intermediate chunk without duplication");
  Game loaded(campaign);bool restored=loaded.decodeSave(g.encodeSave());check(restored&&loaded.friendlyActors().size()==1&&loaded.friendlyActors()[0].level==2&&length(loaded.friendlyActors()[0].pose.position-g.friendlyActors()[0].pose.position)<.002f,"Save restores escort location and identity after crossing a seam");
  if(!restored)continue;tick(loaded,1440,1.f/60);const auto&npc=loaded.friendlyActors()[0];
  check(npc.level==1&&length(npc.pose.position-loaded.player().pos)<2.4f,"Escort follows across two stitched chunks with arbitrary authoring IDs");
  loaded.m_player.pos=npc.pose.position+Vec2{1,0};loaded.m_player.angle=kPi;auto position=npc.pose.position;
  check(loaded.commandFriendly(),"Migrated escort still accepts commands in a chunk with no authored NPC slot");tick(loaded,120,1.f/60);
  check(length(loaded.friendlyActors()[0].pose.position-position)<.001f,"Wait command retains the original NPC identity after migration");
  loaded.damageFriendly(0,100,loaded.player().pos);tick(loaded,120,1.f/60);Game corpse(campaign);
  check(corpse.decodeSave(loaded.encodeSave())&&corpse.friendlyActors()[0].level==1&&corpse.friendlyActors()[0].pose.clip==4,"Migrated NPC death restores in the destination chunk");
  Game returning(campaign);bool returnSave=returning.decodeSave(g.encodeSave());if(returnSave){auto global=south?Vec2{6.5f,4.5f}:Vec2{4.5f,6.5f};returning.m_player.pos=global-returning.chunkOffset(returning.level());returning.crossChunkBoundary();tick(returning,1800,1.f/60);}
  check(returnSave&&returning.friendlyActors()[0].level==0&&length(returning.friendlyActors()[0].pose.position-returning.player().pos)<2.4f,"Escort can reverse direction and return through stitched seams");
 }
 auto seamScene=[&](bool door,bool raised=false){
  auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int level=0;level<2;++level){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);map->definition.origin={0,float(level*24)};map->definition.residencyGroup=100+level;
   if(level){map->actorTracks.clear();map->timedSequences.clear();}else for(auto&key:map->actorTracks[0].keys)key.position={6.5f,23.3f};
   if(raised&&level)map->layers[0].elevation=3;
   if(door){Door gate{};gate.left=5;gate.right=8;gate.y=level?0:24;gate.entry=level!=0;gate.transfer=level==0;map->doors.push_back(gate);}
   else{map->openNorth=level!=0;map->openSouth=level==0;}
   campaign->maps.push_back(map);
  }return Game(campaign);
 };
 {auto g=seamScene(false);g.m_player.pos={6.5f,24.1f};g.crossChunkBoundary();g.updateStreaming(0);tick(g,180,1.f/60);auto npc=g.friendlyActors()[0];auto global=npc.pose.position+g.chunkOffset(npc.level);
  check(length(global-(g.player().pos+g.chunkOffset(g.level())))>=.449f,"Escort stops outside the player hull when player occupies the seam");
  check(!g.hullFits(global-g.chunkOffset(g.level()),npc.pose.z,Player::StandingHeight),"Migrating NPC body blocks player collision across chunk ownership");
  g.m_player.angle=-kPi*.5f;check(g.commandFriendly(),"Follow/Wait works across an open seam before NPC ownership changes");
 }
 for(bool open:{false,true}){auto g=seamScene(true);g.m_player.pos={6.5f,28.5f};g.crossChunkBoundary();g.updateStreaming(0);
  if(open){g.m_world.setDoor(0,1,true);g.m_chunks[0].world.setDoor(0,1,true);}
  tick(g,480,1.f/60);
  check(open?g.friendlyActors()[0].level==1:(g.friendlyActors()[0].level==0&&!g.m_world.doors()[0].opening),"Escort uses open paired transfer doors and never forces a closed transfer");
  if(!open){g.m_player.pos={6.5f,.1f};g.m_player.angle=-kPi*.5f;check(!g.commandFriendly(),"Closed seam prevents remote NPC commands through its door");}
 }
 {auto g=seamScene(false,true);g.m_player.pos={6.5f,28.5f};g.crossChunkBoundary();g.m_player.z=3;tick(g,480,1.f/60);
  check(g.friendlyActors()[0].level==0&&std::fabs(g.friendlyActors()[0].pose.z)<.001f,"Unsafe elevation discontinuity stops NPC instead of teleporting or falling");
 }
 {auto g=seamScene(false);g.setState(g.friendlyKey(0,"chunk"),3);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Save rejects an out-of-range physical NPC chunk");}
 {auto g=seamScene(false);tick(g,1,1.f/60);g.setState(g.friendlyKey(0,"x"),24000);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Save rejects invalid NPC coordinates even for unloaded chunks");}
 {auto original=seamScene(true);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto worker=map->actorTracks[0];map->actorTracks.clear();
  for(int i=0;i<9;++i){auto npc=worker;for(auto&key:npc.keys)key.position={2.5f+float(i),22.f};map->actorTracks.push_back(npc);}campaign->maps[0]=map;Game g(campaign);
  g.m_player.pos={6.5f,42.f};g.crossChunkBoundary();tick(g,1,1.f/60);int planned=0;for(size_t i=0;i<9;++i)planned+=g.state(g.friendlyKey(i,"route_wait"))>0;
  check(!g.chunkResident(0)&&planned==4&&g.m_npcNavBudget==0,"Unloaded NPCs share the bounded portal-search budget without loading a closed route");
  tick(g,2,1.f/60);planned=0;for(size_t i=0;i<9;++i)planned+=g.state(g.friendlyKey(i,"route_wait"))>0;tick(g,1,1.f/60);
  check(planned==9&&g.m_npcNavBudget==4,"Failed unloaded portal searches back off and do not starve later actors");
 }
 {auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int level=0;level<3;++level){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);map->definition.origin=level==0?Vec2{}:level==1?Vec2{24,24}:Vec2{24,0};map->definition.residencyGroup=100+level;
   if(level){map->actorTracks.clear();map->timedSequences.clear();}else for(auto&key:map->actorTracks[0].keys)key.position={22.3f,6.5f};
   map->openEast=level==0;map->openWest=level==2;map->openSouth=level==2;map->openNorth=level==1;campaign->maps.push_back(map);
  }
  Game g(campaign);g.m_player.pos={30.5f,30.5f};g.crossChunkBoundary();tick(g,2400,1.f/60);
  check(g.friendlyActors()[0].level==1&&length(g.friendlyActors()[0].pose.position-g.player().pos)<2.4f,"Escort routes around a two-axis stitched turn with streaming enabled");
 }
 return passed;
}
}
