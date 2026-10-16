#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace retro {
// Exact, bounded memoization during a read-only collision phase. The owner
// discards the batch before any physics/geometry updates. Only its specified
// World is eligible; temporary planning worlds and other chunks bypass it.
class CollisionQueryBatch {
public:
 enum Kind : uint32_t {Fits=1,SupportBelow=2};
 struct Statistics {uint64_t queries=0,hits=0;};
 explicit CollisionQueryBatch(const void* world,bool enabled=true):m_world(world),m_previous(s_active){
  if(enabled){for(auto&entry:m_entries)entry.tag=0;s_active=this;}
  else s_active=nullptr;
 }
 ~CollisionQueryBatch(){s_totals.queries+=m_statistics.queries;s_totals.hits+=m_statistics.hits;s_active=m_previous;}
 CollisionQueryBatch(const CollisionQueryBatch&)=delete;
 CollisionQueryBatch&operator=(const CollisionQueryBatch&)=delete;
 static Statistics statistics(){return s_totals;}
 static void resetStatistics(){s_totals={};}
 static CollisionQueryBatch* active(const void*world){return s_active&&s_active->m_world==world?s_active:nullptr;}
 bool find(Kind kind,float x,float y,float feet,float height,uint32_t flags,float&value){
  auto key=makeKey(x,y,feet,height);uint32_t tag=uint32_t(kind)|(flags<<3);
  auto&entry=m_entries[slot(key,tag)];++m_statistics.queries;
  if(entry.tag!=tag||entry.key!=key)return false;
  value=entry.value;++m_statistics.hits;return true;
 }
 void store(Kind kind,float x,float y,float feet,float height,uint32_t flags,float value){
  auto key=makeKey(x,y,feet,height);uint32_t tag=uint32_t(kind)|(flags<<3);
  m_entries[slot(key,tag)]={key,tag,value};
 }
private:
 using Key=std::array<uint32_t,4>;
 struct Entry {Key key;uint32_t tag;float value;};
 static Key makeKey(float x,float y,float feet,float height){return {std::bit_cast<uint32_t>(x),std::bit_cast<uint32_t>(y),std::bit_cast<uint32_t>(feet),std::bit_cast<uint32_t>(height)};}
 static std::size_t slot(const Key&key,uint32_t tag){
  uint64_t a=(uint64_t(key[0])<<32)|key[1],b=(uint64_t(key[2])<<32)|key[3];
  uint64_t hash=a^std::rotl(b,23)^(uint64_t(tag)*0x9e3779b97f4a7c15ull);
  hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;
  return std::size_t(hash)&1023;
 }
 const void*m_world;
 CollisionQueryBatch*m_previous;
 std::array<Entry,1024> m_entries;
 Statistics m_statistics{};
 inline static thread_local CollisionQueryBatch*s_active=nullptr;
 inline static thread_local Statistics s_totals{};
};
}
