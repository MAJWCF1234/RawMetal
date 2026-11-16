// Actual shipped Starter Map pools: parsing, collision, traced shots, effects and player firing.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "world/CustomCampaign.h"
#include <cmath>
#include <fstream>

namespace retro::lab {
namespace {
float norm(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
const bool waterLifetime=add({.name="water-lifetime",.summary="restarts discard old water impacts and splash-only inspection input draws",.standalone=+[]{
 std::ofstream report("water-lifetime-test.txt");auto room=regression::room();room->waterVolumes.push_back({8,8,16,16,-2,1});Game game(regression::campaign(room));
 LabAccess::setCamera(game,{6,10},0,0,0);LabAccess::fireAt(game,{10,10,1});const bool struck=game.waterSplashes().size()==1;
 game.restart();const bool reset=struck&&game.waterSplashes().empty();report<<(reset?"PASS":"FAIL")<<" a fresh game discards the previous timeline's splash\n";
 auto memory=makeParticleMemory();memory->extra.splashes.push_back({{10,10,1},{0,0,-1},0,0,1});
 Particles::View view;view.eye={6,10,2};view.forward={1,0,0};view.level=0;view.now=.08f;
 Game clean(regression::campaign(room));const auto stats=Particles::draw(*memory,clean,view,[](const MeshVertex*,const Particles::Texture&,float,const Particles::CardLook&){},[](Point3,Point3){return 1.f;},[](Point3,Point3){return true;});
 const bool inspection=stats.blended>=12;report<<(inspection?"PASS":"FAIL")<<" splash-only particle memory emits its spray and drops; cards "<<stats.blended<<'\n';return reset&&inspection;
}});
struct Cards {int all=0,nearby=0,mist=0,drops=0;ParticleStats stats{};};
Cards waterCards(Game& game,Point3 target){Cards out;const auto&p=game.player();const Point3 eye{p.pos.x,p.pos.y,p.z+p.stanceEye()},toward=target-eye;
 Particles::View view;view.eye=eye;view.forward=toward*(1.f/norm(toward));view.pixel=1.f/500;view.level=game.level();view.now=game.elapsed()+.08f;auto memory=makeParticleMemory();
 out.stats=Particles::draw(*memory,game,view,[&](const MeshVertex*q,const Particles::Texture&t,float,const Particles::CardLook&){++out.all;const auto center=(q[0].p+q[2].p)*.5f;
  const auto d=center-target;if(d.x*d.x+d.y*d.y>1.25f*1.25f||center.z<target.z+.005f)return;++out.nearby;
  if(&t==&Particles::blendedAtlas()){const float v=q[0].v;out.mist+=v>=5.f/9.f&&v<6.f/9.f;out.drops+=v>=8.f/9.f;}
 },[](Point3,Point3){return .7f;},[](Point3,Point3){return true;});return out;}
const bool starterWater=add({.name="starter-water",.summary="all real Starter Map water volumes react to bank/vertical shots and player InputState firing",.standalone=+[]{
 std::ofstream report("starter-water-test.txt");bool passed=true;auto check=[&](bool ok,const std::string& name){report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();passed&=ok;};
 const auto campaign=loadCustomCampaignFile(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");if(!campaign){check(false,"loads actual Starter Map campaign");return false;}int pools=0;
 for(size_t level=0;level<campaign->maps.size();++level)for(const auto& water:campaign->maps[level]->waterVolumes){++pools;
  const Point3 target{(water.x1+water.x2)*.5f,(water.y1+water.y2)*.5f,water.surface};
  for(bool vertical:{false,true}){Game game(campaign);LabAccess::loadLevel(game,int(level));LabAccess::setGod(game,true);
   const Vec2 at=vertical?Vec2{target.x,target.y}:level==38?Vec2{target.x,water.y1-.8f}:Vec2{water.x1-.8f,target.y};const float feet=vertical?water.surface+.8f:game.world().floorHeight(at.x,at.y);
   LabAccess::setCamera(game,at,feet,0,0);const auto before=waterCards(game,target);ShotHit hit;LabAccess::fireAt(game,target,&hit);const auto after=waterCards(game,target);
   int sounds=0;for(const auto&s:game.sounds())sounds+=s.sound==Sound::WaterSplash;
   report<<"map "<<level<<" pool "<<pools<<' '<<(vertical?"vertical":"bank")<<" target "<<target.x<<','<<target.y<<','<<target.z<<" bed "<<water.bed
    <<" floor "<<game.world().floorHeight(target.x,target.y)<<" support "<<game.world().supportBelow(target.x,target.y,target.z+.1f)
    <<" hit "<<int(hit.kind)<<" at "<<hit.point.x<<','<<hit.point.y<<','<<hit.point.z<<" distance "<<hit.distance
    <<" passage "<<hit.waterLevel<<'/'<<hit.waterDistance<<" events "<<game.waterSplashes().size()<<" sounds "<<sounds
    <<" cards "<<before.nearby<<" -> "<<after.nearby<<" mist "<<before.mist<<" -> "<<after.mist<<" drops "<<before.drops<<" -> "<<after.drops
    <<" particle sources "<<after.stats.sources<<" inside "<<after.stats.inside<<" culled "<<after.stats.culled<<'\n';
   const auto label="map "+std::to_string(level)+(vertical?" vertical":" bank");
   check(hit.waterLevel==int(level)&&game.waterSplashes().size()==1,label+" traces a surface passage and queues its splash");
   check(sounds==1,label+" raises one splash sound");check(after.nearby>=before.nearby+8&&after.drops>before.drops,label+" draws new raised water drops");
  }
 }
 check(pools==4,"loads all four actual Starter Map WATER records");
 // This is the real weapon update path, including stance eye, pitch, ammo, holster and cooldown.
 for(int level:{0,38})for(const auto& water:campaign->maps[size_t(level)]->waterVolumes){Game game(campaign);LabAccess::loadLevel(game,level);LabAccess::setGod(game,true);LabAccess::equip(game,WeaponId::Shotgun);
  const Point3 target{(water.x1+water.x2)*.5f,(water.y1+water.y2)*.5f,water.surface};const Vec2 bank=level==38?Vec2{target.x,water.y1-.8f}:Vec2{water.x1-.8f,target.y};
  LabAccess::setCamera(game,bank,0,0,0);LabAccess::playerCamera(game);for(int i=0;i<60;++i)game.update({},1.f/60);
  auto&p=LabAccess::player(game);const Point3 eye{p.pos.x,p.pos.y,p.z+p.stanceEye()};const auto d=target-eye;
  p.angle=std::atan2(d.y,d.x);p.pitch=std::atan2(d.z,std::hypot(d.x,d.y))*140.f;const auto before=waterCards(game,target);const int loaded=p.pool(WeaponId::Shotgun).loaded;
  InputState fire{};fire.fire=true;game.update(fire,1.f/60);const auto after=waterCards(game,target);int sounds=0;for(const auto&s:game.sounds())sounds+=s.sound==Sound::WaterSplash;
  report<<"player fire map "<<level<<" target "<<target.x<<','<<target.y<<','<<target.z<<" eye "<<eye.x<<','<<eye.y<<','<<eye.z<<" yaw "<<p.angle<<" pitch "<<p.pitch<<" loaded "<<loaded<<" -> "<<p.pool(WeaponId::Shotgun).loaded
   <<" rays "<<game.shotRays().size()<<" events "<<game.waterSplashes().size()<<" sounds "<<sounds<<" cards "<<before.nearby<<" -> "<<after.nearby<<'\n';
  const auto label="map "+std::to_string(level)+" pool x "+std::to_string(target.x)+" InputState fire";
  check(p.pool(WeaponId::Shotgun).loaded==loaded-1,label+" spends a real shell");
  check(game.waterSplashes().size()==1&&sounds==1,label+" queues the splash and its sound");
  check(after.nearby>=before.nearby+8&&after.drops>before.drops,label+" draws raised water drops");
  AudioEngine audio(false);audio.update(game,true);float mixedLevel=0;bool spatialVoice=false;
  for(const auto& voice:LabAccess::held(audio))if(voice.sound==Sound::WaterSplash){mixedLevel=std::max(mixedLevel,voice.level);spatialVoice|=voice.spatial&&!voice.loop;}
  LabAccess::onlySound(audio,Sound::WaterSplash);std::vector<int16_t> pcm;LabAccess::mix(audio,pcm,8820);double energy=0;int peak=0;
  for(auto sample:pcm){energy+=double(sample)*sample;peak=std::max(peak,std::abs(int(sample)));}const double rms=std::sqrt(energy/std::max(size_t(1),pcm.size()));
  report<<"actual splash mixer voice "<<spatialVoice<<" level "<<mixedLevel<<" first .2s energy "<<energy<<" rms "<<rms<<" peak "<<peak<<'\n';
  check(spatialVoice&&mixedLevel>0,label+" starts an actual spatial splash voice");check(rms>10&&peak>100,label+" mixes audible early splash PCM isolated from the gunshot");
 }
 return passed;
}});
}
}
