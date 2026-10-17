#pragma once
#include "../../world/ScriptDefinition.h"
#include <algorithm>
#include <array>
#include <span>
namespace retro::ai {
// A hint is only an integer position. Validate the current sorted neighbours
// on every use, so insertion, erasure, restore and other Games need no stale
// pointers, cached values, owner identities or invalidation notifications.
class StateLookupHints {
public:
 struct Statistics {uint64_t queries=0,hits=0;};
 inline static thread_local Statistics statistics{};
 static void resetStatistics(){statistics={};}
 static size_t lowerBound(std::span<const StateValue> values,StateId id,bool enabled=true){
  ++statistics.queries;auto&hint=s_indices[id&(s_indices.size()-1)];
  if(enabled&&hint<=values.size()&&(hint==values.size()||values[hint].id>=id)&&(!hint||values[hint-1].id<id)){
   ++statistics.hits;return hint;
  }
  auto it=std::lower_bound(values.begin(),values.end(),id,[](const StateValue&value,StateId key){return value.id<key;});
  size_t index=size_t(it-values.begin());if(enabled)hint=index;return index;
 }
private:
 inline static thread_local std::array<size_t,1024> s_indices{};
};
}
