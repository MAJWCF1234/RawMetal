#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::soundRayClear(Vec2 from,float fromZ,Vec2 to,float toZ,float nearTrim,float farTrim)const{
 auto delta=to-from;float dz=toZ-fromZ,distance=std::sqrt(lengthSq(delta)+dz*dz);if(distance<.001f)return true;
 float begin=std::max(0.f,nearTrim)/distance,finish=1-std::max(0.f,farTrim)/distance;
 while(begin<finish){
  float probe=begin+std::min(.00001f,(finish-begin)*.5f);auto point=from+delta*probe,local=point;const auto&world=worldAt(local);auto shift=local-point;
  if(!chunkResident(world.level())||local.x<0||local.x>=World::Width||local.y<0||local.y>=World::Height)return false;
  float next=finish;
  if(delta.x>.000001f)next=std::min(next,probe+(World::Width-local.x)/delta.x);else if(delta.x<-.000001f)next=std::min(next,probe-local.x/delta.x);
  if(delta.y>.000001f)next=std::min(next,probe+(World::Height-local.y)/delta.y);else if(delta.y<-.000001f)next=std::min(next,probe-local.y/delta.y);
  if(next<=begin)return false;
  if(!world.lightRayClear(from+delta*begin+shift,fromZ+dz*begin,from+delta*next+shift,fromZ+dz*next))return false;
  bool dynamic=world.hasLift()||world.cargoLift().x2>world.cargoLift().x1||std::any_of(world.actorTracks().begin(),world.actorTracks().end(),[](const auto&t){return t.platform;});
  if(dynamic||!world.doors().empty()){
   int steps=std::max(1,int(std::ceil((next-begin)*distance/.2f)));
   for(int i=0;i<=steps;++i){float t=begin+(next-begin)*float(i)/steps;auto p=from+delta*t+shift;float z=fromZ+dz*t;
    if(world.doorBlocks(p.x,p.y,z,.015f)||(dynamic&&!world.fits(p.x,p.y,z,.015f,true,true)))return false;
   }
  }begin=next;
 }return true;
}
bool Game::testHearing(){
 std::ofstream report("hearing-test.txt");bool passed=true;auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;};
 auto scene=[](bool stacked=false,bool wall=false){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="3D hearing validation";campaign->key=891237;
  auto map=std::make_shared<AuthoredMapData>();map->name="Hearing fixture";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData floor;for(auto&row:floor.rows)row=std::string(24,'.');if(wall)for(auto&row:floor.rows)row[10]='#';map->layers.push_back(floor);
  if(stacked){floor.elevation=3;floor.thickness=.2f;for(auto&row:floor.rows)row=std::string(24,'=');map->layers.push_back(floor);}
  campaign->maps.push_back(map);Game g(campaign);Enemy e{};e.pos=e.home=e.lastKnown={6.5f,6.5f};e.heading=0;g.m_enemies={e};g.m_sounds.clear();return g;
 };
 for(bool upper:{false,true}){auto g=scene(true);g.m_enemies[0].z=upper?3.f:0.f;Vec2 source{8.5f,6.5f};
  g.m_sounds.push_back({Sound::JunkMetal,source,1,1,true,17,3.04f});g.updateEnemies(.01f);const auto&e=g.m_enemies[0];
  report<<"floor observer="<<upper<<" state="<<int(e.state)<<" awareness="<<e.awareness<<" feet="<<e.z<<" goal="<<e.lastKnown.x<<','<<e.lastKnown.y<<','<<e.lastKnownZ<<'\n';
  check(upper?(e.state==Enemy::State::Investigate&&lengthSq(e.lastKnown-source)<.001f&&std::fabs(e.lastKnownZ-3)<.001f):e.awareness==0,"Identical XY sound selects its actual floor and is muffled by the separating slab");
 }
 {auto g=scene(true);Vec2 source{8.5f,6.5f};g.m_sounds.push_back({Sound::Shot,source,1,1,true,17,3.04f});g.updateEnemies(.01f);
  report<<"loud floor state="<<int(g.m_enemies[0].state)<<" awareness="<<g.m_enemies[0].awareness<<" goalZ="<<g.m_enemies[0].lastKnownZ<<'\n';
  check(g.m_enemies[0].state==Enemy::State::Investigate&&std::fabs(g.m_enemies[0].lastKnownZ-3)<.001f,"Loud sound heard through a floor retains upper-floor investigation height");
 }
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);map->structures.push_back({0,0,24,24,3,3.04f});campaign->maps[0]=map;Game g(campaign);g.m_enemies=original.m_enemies;
  g.m_sounds.push_back({Sound::JunkMetal,{8.5f,6.5f},1,1,true,17,4.2f});g.updateEnemies(.01f);check(g.m_enemies[0].awareness==0,"Exact acoustic trace blocks a four-centimetre slab that point samples can miss");
 }
 for(bool blocked:{false,true}){auto g=scene();auto campaign=std::make_shared<CustomCampaign>(*g.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
  if(blocked)map->structures.push_back({7.4f,0,7.6f,24,0,8,false,3});campaign->maps[0]=map;Game test(campaign);test.m_enemies=g.m_enemies;
  test.m_sounds.push_back({Sound::JunkMetal,{9.5f,6.5f},1,1,true,17,.1f});test.updateEnemies(.01f);
  check(blocked?test.m_enemies[0].awareness==0:test.m_enemies[0].state==Enemy::State::Investigate,"Wall occlusion limits quiet-impact detection");
 }
 for(int rate:{60,120})for(bool crouch:{false,true}){auto g=scene();g.m_player.pos={3.7f,6.5f};g.m_player.angle=0;g.m_stepDistance=2;
  InputState input{};input.forward=true;input.crouch=crouch;g.updateMovement(input,1.f/rate);
  auto step=std::find_if(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound>=Sound::Metal1&&e.sound<=Sound::Concrete4;});
  check(step!=g.m_sounds.end()&&std::isfinite(step->z)&&std::fabs(step->z-g.player().z-.04f)<.001f,"Real standing/crouched movement emits captured foot-contact height at 60/120 Hz");
  if(step==g.m_sounds.end())continue;auto source=step->position;g.m_player.pos={18.5f,18.5f};g.updateEnemies(.01f);
  check(crouch?g.m_enemies[0].awareness==0:(g.m_enemies[0].state==Enemy::State::Investigate&&lengthSq(g.m_enemies[0].lastKnown-source)<.001f),"Walking is heard where emitted while equivalent crouching remains undetected");
 }
 {auto g=scene(false,true);Clutter object;object.kind=3;object.pos={8.5f,6.5f};object.z=1.2f;object.vz=-4;g.m_clutter={object};
  for(int i=0;i<120&&g.m_sounds.empty();++i)g.updateClutter({},1.f/120);
  auto impact=std::find_if(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::JunkMetal;});
  check(impact!=g.m_sounds.end()&&std::isfinite(impact->z)&&std::fabs(impact->z-g.m_clutter[0].height()*.5f)<.01f,"Falling-object sound is emitted at corrected floor contact");
  g.updateEnemies(.01f);check(g.m_enemies[0].state==Enemy::State::Investigate&&lengthSq(g.m_enemies[0].lastKnown-object.pos)<.001f&&lengthSq(g.m_enemies[0].lastKnown-g.player().pos)>1,"Wall-hidden object impact attracts the enemy without revealing the thrower");
 }
 {auto g=scene(false,true);Clutter object;object.kind=3;object.pos={6.7f,6.5f};object.z=.3f;object.velocity={-3,0};object.projectile=true;g.m_clutter={object};g.updateClutter({},.01f);
  auto hit=std::find_if(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::PunchHit;});
  check(hit!=g.m_sounds.end()&&g.m_enemies[0].hp==105&&lengthSq(g.m_enemies[0].lastKnown-hit->position)<.001f&&lengthSq(g.m_enemies[0].lastKnown-g.player().pos)>1,"Direct thrown-object hit records the impact instead of the hidden player's position");
 }
 {auto g=scene();auto before=g.player();Point3 origin{8.5f,6.5f,4.2f};g.setVrAim(origin,{1,0,0},true);g.shoot();
  auto shot=std::find_if(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::Shot;});
  check(shot!=g.m_sounds.end()&&lengthSq(shot->position-Vec2{origin.x,origin.y})<.001f&&std::fabs(shot->z-origin.z)<.001f&&g.player().pos.x==before.pos.x&&g.player().z==before.z,"VR gunshot retains tracked weapon origin after view pose restoration");
 }
 {auto original=scene(true);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
  TimedSequence timer;timer.timerState=stateId("hearing_clock");timer.finishState=stateId("hearing_done");timer.soundIntervalMs=1;map->timedSequences.push_back(timer);
  ActorTrack actor;actor.timerState=timer.timerState;actor.keys={{0,{8.5f,6.5f},3,0},{30000,{8.5f,6.5f},3,0}};map->actorTracks.push_back(actor);
  map->sequenceCues.push_back({timer.timerState,1,Sound::Shot,{10.5f,6.5f},1,1,"",3.04f});campaign->maps[0]=map;Game g(campaign);g.m_enemies=original.m_enemies;g.m_enemies[0].z=3;g.setState(timer.timerState,1);g.updateMechanisms(.01f);
  auto tick=std::find_if(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::Metal1;});
  check(tick!=g.m_sounds.end()&&lengthSq(tick->position-Vec2{8.5f,6.5f})<.001f&&std::fabs(tick->z-3.04f)<.001f,"Sequence footsteps originate at their actor instead of the player");
  g.updateEnemies(.01f);check(g.m_enemies[0].state==Enemy::State::Investigate&&lengthSq(g.m_enemies[0].lastKnown-Vec2{10.5f,6.5f})<.001f&&std::fabs(g.m_enemies[0].lastKnownZ-3)<.001f,"Authored elevated cue uses exact source height and strongest audible stimulus");
  report<<"cue state="<<int(g.m_enemies[0].state)<<" goal="<<g.m_enemies[0].lastKnown.x<<','<<g.m_enemies[0].lastKnown.y<<','<<g.m_enemies[0].lastKnownZ<<'\n';
 }
 {auto g=scene();g.m_player.pos={8.5f,6.5f};g.updateEnemies(.01f);auto remembered=g.m_enemies[0].lastKnown;g.m_player.pos={18.5f,18.5f};g.m_sounds.clear();g.updateEnemies(.01f);
  check(lengthSq(remembered-Vec2{8.5f,6.5f})<.001f&&lengthSq(g.m_enemies[0].lastKnown-remembered)<.001f,"Losing sight preserves the last visible position instead of following the hidden player");
 }
 for(bool blocked:{false,true}){auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int level=0;level<2;++level){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);map->definition.origin={float(level*24),0};map->openEast=level==0;map->openWest=level==1;
   if(level){map->layers[0].elevation=2;if(blocked)map->structures.push_back({1,0,1.1f,24,2,8});}else map->structures.push_back({20,6,24,7,1.8f,2});campaign->maps.push_back(map);
  }
  Game g(campaign);Enemy e{};e.pos=e.home=e.lastKnown={22.5f,6.5f};e.z=2;e.heading=0;g.m_enemies={e};g.m_sounds={{Sound::JunkMetal,{27.5f,6.5f},1,1,true,17,2.04f}};g.updateEnemies(.01f);
  check(blocked?g.m_enemies[0].awareness==0:(g.m_enemies[0].state==Enemy::State::Investigate&&std::fabs(g.m_enemies[0].lastKnownZ-2)<.001f),"Cross-chunk hearing uses the neighbor's geometry and source-floor elevation");
  if(!blocked){g.m_enemies.clear();g.m_player.pos={24.1f,6.5f};g.crossChunkBoundary();check(g.level()==1&&std::fabs(g.m_sounds[0].position.x-3.5f)<.001f&&std::fabs(g.m_sounds[0].z-2.04f)<.001f,"Sound chunk handoff changes XY and preserves absolute height");}
 }
 for(int invalid=0;invalid<4;++invalid){auto g=scene();SoundEvent noise{Sound::JunkMetal,{8.5f,6.5f},1,1,true,17,.04f};
  if(invalid==0)noise.gain=std::numeric_limits<float>::quiet_NaN();if(invalid==1)noise.radius=-1;if(invalid==2)noise.z=std::numeric_limits<float>::infinity();if(invalid==3)noise.position.x=std::numeric_limits<float>::quiet_NaN();
  g.m_sounds.push_back(noise);g.updateEnemies(.01f);check(g.m_enemies[0].awareness==0,"Invalid acoustic inputs cannot alert or relocate an enemy");
 }
 for(const char*height:{"","|3.04","|nan"}){std::ofstream file("hearing-cue-fixture.txt");
  file<<"--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Hearing|0\nMAP|0|Room|0|0|18.5|18.5|0|0|8|0.3||0|0|0|0|0\nLAYER|0|0|Ground|0|0.2\n";
  for(int row=0;row<24;++row)file<<"ROW|0|0|"<<row<<"|"<<std::string(24,'.')<<'\n';
  file<<"TIMED_SEQUENCE|0|clock|done|0|0|24|24|-100|100|30000|16000|0|16000|2|1|1\nSEQUENCE_CUE|0|clock|1|0|8.5|6.5|1|1|Caption"<<height<<"\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";file.close();bool accepted=false;float z=0;
  try{auto loaded=loadCustomCampaignFile("hearing-cue-fixture.txt");z=loaded->maps[0]->sequenceCues[0].z;accepted=true;}catch(const std::exception&){}
  check(*height==0?(accepted&&std::isnan(z)):std::string_view(height)=="|nan"?!accepted:(accepted&&std::fabs(z-3.04f)<.001f),"Custom cue parser preserves legacy records, accepts elevation and rejects nonfinite height");
 }
 return passed;
}
}
