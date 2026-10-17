#pragma once
#include "CollisionQueryBatch.h"
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
namespace retro {
// Terrain revisions identify immutable density fields, including their World
// copies. Exact queries may be shared across simulation and rendering without
// retaining dynamic doors, actors, platforms or any approximate surface height.
class TerrainSurfaceCache {
public:
 struct Statistics {uint64_t queries=0,hits=0;};
 inline static std::atomic<bool> enabled{true};
 static Statistics statistics(){return s_statistics;}
 static void resetStatistics(){s_statistics={};}
 template<class Query> static float below(uint64_t revision,float x,float y,float feet,const Query&query){
  if(!revision||!enabled.load(std::memory_order_relaxed)||!CollisionQueryBatch::immutableQueriesAllowed()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(feet))return query();
  std::array<uint32_t,3> key{std::bit_cast<uint32_t>(x),std::bit_cast<uint32_t>(y),std::bit_cast<uint32_t>(feet)};
  uint64_t hash=revision*0x9e3779b97f4a7c15ull^((uint64_t(key[0])<<32)|key[1])^std::rotl(uint64_t(key[2])*0x9e3779b97f4a7c15ull,19);
  hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;
  auto&entry=s_entries[size_t(hash)&(s_entries.size()-1)];++s_statistics.queries;
  if(entry.revision==revision&&entry.key==key){++s_statistics.hits;return entry.value;}
  float value=query();entry={revision,key,value};return value;
 }
private:
 struct Entry {uint64_t revision;std::array<uint32_t,3> key;float value;};
 inline static thread_local std::array<Entry,4096> s_entries{};
 inline static thread_local Statistics s_statistics{};
};
}
