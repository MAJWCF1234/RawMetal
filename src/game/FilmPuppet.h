#pragma once
// Film puppets (capture only): a take holds a map's people and creatures where the
// Campaign Builder's Filmmaker keyed them, whatever their scripts or AI would do.
// --film's "pose" lines (src/main.cpp) and the builder's live preview
// (tools/Level Editor/src/platform/EditorPreview.cpp) both pose through here.
//
//   pose aK X Y FEET YAW CLIP PHASE   person K of the map in view (its ACTOR index)
//   pose eK X Y FEET YAW CLIP PHASE   creature K of the map in view (its CREATURE order)
//   pose aK off                       let it go again
//
// YAW is degrees (0 east, 90 south), FEET metres. A person's CLIP is its ACTOR_KEY clip
// (0 idle, 1 walk, 2 punch, 3 hit, 4 death, 5 crouch; the Investigator 6-9) at PHASE
// 0..1. A creature plays 0 idle, 1 walk (PHASE its stride), 2 its attack, 3 a flinch.
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <istream>
#include <string>
#include <vector>
namespace retro {
struct FilmPuppet {
 char kind='a';int index=0;ActorPose pose{};bool release=false;
 bool same(const FilmPuppet& o)const{return kind==o.kind&&index==o.index;}
};
// The rest of a "pose" line after the op.
inline bool readFilmPuppet(std::istream& in,FilmPuppet& p){
 std::string who;if(!(in>>who)||who.size()<2||(who[0]!='a'&&who[0]!='e'))return false;
 p.kind=who[0];try{p.index=std::stoi(who.substr(1));}catch(...){return false;}
 if(p.index<0)return false;
 std::string first;if(!(in>>first))return false;
 if(first=="off"){p.release=true;return true;}
 float x=0,y=0,feet=0,yaw=0,phase=0;int clip=0;
 try{x=std::stof(first);}catch(...){return false;}
 if(!(in>>y>>feet>>yaw>>clip>>phase))return false;
 p.pose.position={x,y};p.pose.z=feet;p.pose.yaw=yaw*kPi/180.f;p.pose.clip=std::max(0,clip);p.pose.phase=std::clamp(phase,0.f,1.f);
 return true;
}
// Keep one entry per puppet: a later line replaces it, "off" drops it.
inline void holdFilmPuppet(std::vector<FilmPuppet>& held,const FilmPuppet& p){
 std::erase_if(held,[&](const FilmPuppet& q){return q.same(p);});
 if(!p.release)held.push_back(p);
}
inline void applyFilmPuppet(Game& scene,const FilmPuppet& p){
 const int level=scene.level();
 if(p.kind=='a'){
  auto& world=const_cast<World&>(scene.world());
  if(size_t(p.index)<world.actorTracks().size()){
   ActorPose pose=p.pose;pose.clip=std::min(pose.clip,lastActorClip(world.actorTracks()[size_t(p.index)].visual));
   world.setActorPose(size_t(p.index),pose);
   for(auto& npc:const_cast<std::vector<FriendlyActor>&>(scene.npcActors()))
    if(npc.homeLevel==level&&npc.homeActor==size_t(p.index)){npc.level=level;npc.pose=pose;}
  }
  return;
 }
 auto& foes=const_cast<std::vector<Enemy>&>(scene.chunkEnemies(level));
 if(size_t(p.index)>=foes.size())return;
 auto& e=foes[size_t(p.index)];
 if(!e.alive)return;
 e.pos=p.pose.position;e.z=p.pose.z;e.heading=p.pose.yaw;e.wakeState=0;e.feeding=0;
 e.moving=p.pose.clip==1;e.gait=p.pose.phase*2*kPi;
 e.windup=0;e.strike=p.pose.clip==2?std::max(.001f,1-p.pose.phase):0;e.painFlash=p.pose.clip==3?std::max(.001f,1-p.pose.phase):0;
}
inline void applyFilmPuppets(Game& scene,const std::vector<FilmPuppet>& held){for(const auto& p:held)applyFilmPuppet(scene,p);}
}
