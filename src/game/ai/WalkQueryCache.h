#pragma once
#include "../../world/World.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
namespace retro::ai {
// The exact mutable collision state World::fits / supportBelow / doorBlocks read,
// behind the identity of the immutable definition it sits on: two equal
// snapshots answer every collision query alike. The walk memo below keys on it,
// and so does any route or node-graph memo; a new mutable input to those
// queries belongs here, never in one memo's private key.
inline void collisionSnapshot(const World&world,std::vector<uint32_t>&out){
 out.clear();auto word=[&](uint32_t value){out.push_back(value);};auto scalar=[&](float value){word(std::bit_cast<uint32_t>(value));};
 // Which definition: the authored map it was loaded from, its world and slot.
 const auto identity=world.definitionIdentity();const auto address=uint64_t(reinterpret_cast<uintptr_t>(identity.map));
 word(uint32_t(address));word(uint32_t(address>>32));word(uint32_t(identity.world));word(uint32_t(identity.level));
 word(uint32_t(world.layers().size()));word(uint32_t(world.structures().size()));word(uint32_t(world.fixtures().size()));word(uint32_t(world.props().size()));word(world.hasTerrain());
 // Tile destruction opens paths; comparing actual tiles also handles restore.
 for(int y=0;y<24;++y)for(int x=0;x<24;x+=4){uint32_t tiles=0;for(int i=0;i<4;++i)tiles|=uint32_t(uint8_t(world.tile(x+i,y)))<<(i*8);word(tiles);}
 // maxOpen is where a door plan stands a door it may operate; swing turns a leaf.
 word(uint32_t(world.doors().size()));for(const auto&door:world.doors()){for(float value:{door.left,door.right,door.y,door.z,door.open,door.maxOpen,door.swing})scalar(value);word(door.swinging);}
 scalar(world.cargoLiftHeight());scalar(world.liftHeight());word(uint32_t(world.liftPhase()));word(world.liftPhaseTime()>=3.65f);
 word(uint32_t(world.terminals().size()));for(const auto&terminal:world.terminals())scalar(terminal.z);
 // Ordinary NPCs do not participate in World::fits/supportBelow. Moving
 // platform tracks do, and every exact platform position must invalidate.
 word(uint32_t(world.actorTracks().size()));for(size_t i=0;i<world.actorTracks().size();++i)if(world.actorTracks()[i].platform){const auto&pose=world.actorPose(i);scalar(pose.position.x);scalar(pose.position.y);scalar(pose.z);}
 // Push crates block fits (crateBlocks) and carry supportBelow / clearanceAbove
 // wherever each one has been shoved or dropped to.
 word(uint32_t(world.pushCrates().size()));for(const auto&crate:world.pushCrates()){scalar(crate.position.x);scalar(crate.position.y);scalar(crate.z);scalar(crate.size);}
}
// Exact retained route probes, scoped to one immutable authored definition and
// an exact snapshot of its mutable collision state. This never caches movement
// execution or a seam-crossing probe. It also survives no definition lifetime.
// Activate via Scope during a read-only collision phase (friendly AI update /
// graph planning) so live locomotion and planning share one memo.
class WalkQueryCache {
public:
 struct Statistics {uint64_t queries=0,hits=0,invalidations=0;};
 inline static thread_local Statistics statistics{};
 static void resetStatistics(){statistics={};}
 static WalkQueryCache* active(const World&world);
 class Scope {
 public:
  Scope(const World&world,const std::shared_ptr<const AuthoredMapData>&definition,bool enabled=true);
  ~Scope();
  Scope(const Scope&)=delete;Scope&operator=(const Scope&)=delete;
 private:
  WalkQueryCache*m_previous=nullptr;
 };
 void begin(const World&world,const std::shared_ptr<const AuthoredMapData>&definition,bool enabled=true){
  m_enabled=enabled&&bool(definition);if(!m_enabled)return;
  collisionSnapshot(world,m_scratch);
  bool ownerChanged=m_world!=&world||m_definition.expired()||m_definition.owner_before(definition)||definition.owner_before(m_definition);
  if(ownerChanged||m_snapshot!=m_scratch){for(auto&entry:m_entries)entry.state=0;m_snapshot.swap(m_scratch);m_definition=definition;m_world=&world;++statistics.invalidations;}
 }
 template<class Probe> bool walk(Vec2 from,float feet,Vec2 to,float height,float*endFeet,const Probe&probe){
  // Entire swept footprint must remain in this world; seam permissions and
  // neighbouring collision states deliberately retain live validation.
  if(!m_enabled||!std::isfinite(feet)||!std::isfinite(height)||!std::isfinite(from.x)||!std::isfinite(from.y)||!std::isfinite(to.x)||!std::isfinite(to.y)||
    !(std::min(from.x,to.x)>=.2f&&std::min(from.y,to.y)>=.2f&&std::max(from.x,to.x)<23.8f&&std::max(from.y,to.y)<23.8f))return probe(from,feet,to,height,endFeet);
  float localFeet=feet;float*out=endFeet?endFeet:&localFeet;
  Key key{std::bit_cast<uint32_t>(from.x),std::bit_cast<uint32_t>(from.y),std::bit_cast<uint32_t>(feet),std::bit_cast<uint32_t>(to.x),std::bit_cast<uint32_t>(to.y),std::bit_cast<uint32_t>(height)};
  uint64_t hash=1469598103934665603ull;for(auto value:key){hash^=value;hash*=1099511628211ull;hash^=hash>>32;}
  auto&entry=m_entries[size_t(hash)&(m_entries.size()-1)];++statistics.queries;
  if(entry.state&&entry.key==key){++statistics.hits;if(entry.state==2)*out=entry.feet;return entry.state==2;}
  bool result=probe(from,feet,to,height,out);entry.key=key;entry.state=result?2:1;if(result)entry.feet=*out;return result;
 }
private:
 friend class Scope;
 using Key=std::array<uint32_t,6>;
 struct Entry {Key key;float feet;uint8_t state=0;};
 std::array<Entry,2048> m_entries;
 std::vector<uint32_t> m_snapshot,m_scratch;
 std::weak_ptr<const AuthoredMapData> m_definition;
 const World*m_world=nullptr;bool m_enabled=false;
 static thread_local WalkQueryCache s_cache;
 static thread_local WalkQueryCache*s_active;
};
inline thread_local WalkQueryCache WalkQueryCache::s_cache{};
inline thread_local WalkQueryCache*WalkQueryCache::s_active=nullptr;
inline WalkQueryCache* WalkQueryCache::active(const World&world){return s_active&&s_active->m_world==&world&&!world.doorPlanActive()?s_active:nullptr;}
inline WalkQueryCache::Scope::Scope(const World&world,const std::shared_ptr<const AuthoredMapData>&definition,bool enabled){
 m_previous=s_active;
 if(!enabled){s_active=nullptr;return;}
 // Nested scopes keep the outer memo so one actor update covers planning and
 // live locomotion without rebuilding the collision snapshot.
 if(s_active)return;
 s_cache.begin(world,definition,true);s_active=&s_cache;
}
inline WalkQueryCache::Scope::~Scope(){s_active=m_previous;}
}
