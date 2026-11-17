// Things meeting standing water: the splash, its sound and who hears it. A round's splash is
// raised where the round is resolved (Hitscan.cpp); a prop's fall in by its buoyancy (Clutter.cpp,
// Buoyancy.cpp); the player's fall, steps and wake, and creatures' and people's steps, falls and
// bodies here, once a frame (Game::update). Each is a WaterSplash: Particles throws its spray and
// crown, WaterRipples spreads its rings over the surface.
#include "Game.h"
#include "Buoyancy.h"
#include <cmath>
namespace retro {
namespace {
// The splash sample (resource 248) by size: a big thing lower, louder and farther; a step a small
// high slap; each a shade apart (from where and when it fell). Loud ones are a stimulus: a guard
// turns to a body or a crate going into the water as to any crash.
SoundEvent splashSound(const WaterSplash& s,Vec2 at){
 const float p=s.power,shade=.94f+.12f*std::fabs(std::sin(s.point.x*12.9898f+s.point.y*78.233f+s.time*3.7f));
 SoundEvent e{Sound::WaterSplash,at,1,1,true,17,s.point.z};
 switch(s.kind){
  case WaterSplashKind::Step:e.gain=std::clamp(.12f+.42f*p,.1f,.42f);e.pitch=(1.32f-.25f*p)*shade;e.radius=10;e.aiRadius=-1;break; // the step itself is heard (stepHeard)
  case WaterSplashKind::Wake:e.gain=.08f+.06f*std::min(p,1.f);e.pitch=1.5f*shade;e.radius=7;e.aiRadius=-1;break;
  default:e.gain=std::clamp(.26f+.25f*p,.25f,1.f);e.pitch=std::clamp(1.18f-.12f*p,.8f,1.2f)*shade;e.radius=15+6*p;e.aiRadius=std::clamp(2.f+2.9f*p,2.5f,12.f);break;}
 return e;}
}
void Game::raiseWaterSplash(int level,Point3 point,Point3 direction,float power,WaterSplashKind kind,float delay){
 if(level<0||level>=chunkCount()||!chunkResident(level)||!std::isfinite(point.x+point.y+point.z))return;
 WaterSplash s{point,direction,m_elapsed+std::max(0.f,delay),level,std::clamp(power,.1f,3.f),kind,delay<=0};
 m_waterSplashes.push_back(s);if(m_waterSplashes.size()>64)m_waterSplashes.erase(m_waterSplashes.begin());
 if(s.sounded&&kind!=WaterSplashKind::Wake){const auto shift=chunkOffset(level)-chunkOffset(m_level);m_sounds.push_back(splashSound(s,Vec2{point.x,point.y}+shift));}
}
void Game::updateWater(float dt,bool bodies){
 // A splash that was due later (a body's collapse) sounds when it lands.
 for(auto&s:m_waterSplashes)if(!s.sounded&&s.time<=m_elapsed){s.sounded=true;if(s.kind!=WaterSplashKind::Wake&&s.level>=0&&s.level<chunkCount()&&chunkResident(s.level)){const auto shift=chunkOffset(s.level)-chunkOffset(m_level);m_sounds.push_back(splashSound(s,Vec2{s.point.x,s.point.y}+shift));}}
 // Any step (the player's, a creature's, a man's) with water over its foot slaps it: spray and a ring.
 const size_t end=m_sounds.size();
 for(size_t i=bodies?m_waterSoundsSeen:0;i<end;++i){const auto e=m_sounds[i];if(!footstepSound(e.sound)||!std::isfinite(e.z))continue;
  Vec2 local=e.position;const World& w=worldAt(local);const float surface=w.waterSurface(local.x,local.y),feet=e.z-.04f;
  if(surface<-999.f||surface-feet<.02f||surface-feet>1.6f)continue;
  // Shallow, a slap thrown up; deep, a push of water (the wake carries it).
  const float depth=surface-feet,power=std::clamp(.12f+e.gain*.55f,.12f,.55f)*(depth>.5f?.7f:1.f);
  raiseWaterSplash(w.level(),{local.x,local.y,surface},{0,0,-1},power,WaterSplashKind::Step);}
 m_waterSoundsSeen=m_sounds.size();
 if(!bodies){
  // The player: a jump or a fall into the water, and the wake waded through it.
  Vec2 local=m_player.pos;const World& w=worldAt(local);const float surface=w.waterSurface(local.x,local.y),feet=m_player.z,drop=m_waterFeet-feet;
  if(surface>-999.f&&!m_noclip){
   if(m_waterFeet>=surface&&feet<surface&&drop>.005f&&drop<.6f){const float speed=std::max(-m_player.verticalVelocity,drop/std::max(dt,1e-3f));
    if(speed>1.2f)raiseWaterSplash(w.level(),{local.x,local.y,surface},{m_velocity.x,m_velocity.y,-speed},buoyancy::splashPower(70.f,speed),WaterSplashKind::Entry);}
   const float depth=surface-feet,speed=std::sqrt(lengthSq(m_velocity));
   if(depth>.12f&&depth<1.6f&&speed>.45f){m_wakeWay+=speed*dt;
    if(m_wakeWay>.32f){m_wakeWay=0;const Vec2 ahead=m_velocity*(.24f/speed);raiseWaterSplash(w.level(),{local.x+ahead.x,local.y+ahead.y,surface},{m_velocity.x/speed,m_velocity.y/speed,0},std::clamp(speed/3.6f,.25f,1.4f),WaterSplashKind::Wake);}}
   else m_wakeWay=0;
  }
  m_waterFeet=feet;return;
 }
 // Creatures here: one falling in, and a body going down where it died in the water (its collapse
 // lands a moment later). Each death counts on the creature (deathSerial); a new count is a new body.
 if(m_waterDeaths.size()!=m_enemies.size()){m_waterDeaths.resize(m_enemies.size());for(size_t i=0;i<m_enemies.size();++i)m_waterDeaths[i]=m_enemies[i].deathSerial;}
 for(size_t i=0;i<m_enemies.size();++i){auto&e=m_enemies[i];const bool fell=m_waterDeaths[i]!=e.deathSerial;m_waterDeaths[i]=e.deathSerial;
  const float surface=m_world.waterSurface(e.pos.x,e.pos.y);if(surface<-999.f)continue;
  const float size=e.scale*e.scale*e.scale;
  if(fell&&!e.alive&&!e.gone()&&e.deathTime<.5f&&surface>e.z+.04f)raiseWaterSplash(m_level,{e.pos.x,e.pos.y,surface},{0,0,-1},std::clamp(1.5f*std::sqrt(size)+.4f,.8f,3.f),WaterSplashKind::Body,.35f);
  if(e.verticalVelocity<-1.5f&&e.z<surface&&e.z-e.verticalVelocity*dt>=surface)raiseWaterSplash(m_level,{e.pos.x,e.pos.y,surface},{0,0,e.verticalVelocity},buoyancy::splashPower(70.f*size,-e.verticalVelocity),WaterSplashKind::Entry);}
 // People: a man going down (his body's fall, pose clip 4) in water.
 if(m_waterNpcDown.size()!=m_friendlyActors.size()){m_waterNpcDown.assign(m_friendlyActors.size(),0);for(size_t a=0;a<m_friendlyActors.size();++a)m_waterNpcDown[a]=m_friendlyActors[a].pose.clip==4;}
 for(size_t a=0;a<m_friendlyActors.size();++a){const auto&npc=m_friendlyActors[a];const bool down=npc.pose.clip==4;
  if(down&&!m_waterNpcDown[a]&&chunkResident(npc.level)){const World& w=chunkWorld(npc.level);const float surface=w.waterSurface(npc.pose.position.x,npc.pose.position.y);
   if(surface>npc.pose.z+.04f)raiseWaterSplash(npc.level,{npc.pose.position.x,npc.pose.position.y,surface},{0,0,-1},2.f,WaterSplashKind::Body,.4f);}
  m_waterNpcDown[a]=down;}
}
}
