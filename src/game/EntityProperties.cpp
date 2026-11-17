// Map format 2 entity properties at runtime (EntityTraits.h, docs/Systems/map-format.md).
//
// A named record's custom properties (p.key=value) and a named prop's `frozen` flag are values a
// script reads and writes (@entity.key in SCRIPT_REQUIRE and SCRIPT_ACTION STATE/ADD). The parser
// resolves each to a StateId (entityPropertyState) and files it with its map, entity and key; this
// file is the only place their values are read and written.
//
// The authored value is the map's (AuthoredMapData::propertySeeds). What scripts change is the
// save's: the entity store (Game::setEntityProperty, the "entities" section, save format 28),
// keyed by map (the chunk), the entity's name (Game::entityIdKey for a record without one) and
// the property: "p.<key>" for a custom property, "frozen" for a prop's frozen flag. A read takes
// the store's value when it has one and the authored value otherwise, so a new game starts from
// the map and a load resumes what scripts made of it.
#include "Game.h"
#include <algorithm>
#include <cmath>

namespace retro {
namespace {
std::string storeProperty(const std::string& key){return key=="frozen"?key:"p."+key;}
int storeInt(const SaveValue& v){
 if(const auto* b=std::get_if<bool>(&v))return *b?1:0;
 if(const auto* i=std::get_if<std::int64_t>(&v))return int(std::clamp<std::int64_t>(*i,-100000,100000));
 if(const auto* d=std::get_if<double>(&v))return int(std::clamp(std::lround(*d),-100000L,100000L));
 return entityPropertyValue(std::get<std::string>(v));
}
}
void Game::seedEntityProperties(){
 m_entityProperties.clear();
 for(int chunk=0;chunk<chunkCount();++chunk){
  const auto& data=m_chunks[size_t(chunk)].world.m_mapData;if(!data)continue;
  for(const auto& p:data->propertySeeds){
   auto& ref=m_entityProperties[p.id];
   // An authored value wins over a script-only mention of the same property.
   if(ref.level>=0&&!p.seeded)continue;
   ref={chunk,p.entity,storeProperty(p.key),p.seeded?p.value:0};
  }
 }
}
int Game::entityPropertyInt(StateId id)const{
 auto it=m_entityProperties.find(id);if(it==m_entityProperties.end())return 0;const auto& ref=it->second;
 if(const auto* v=entityProperty(ref.level,ref.entity,ref.key))return storeInt(*v);
 return ref.authored;
}
void Game::setEntityPropertyInt(StateId id,int value){
 auto it=m_entityProperties.find(id);if(it==m_entityProperties.end())return;const auto& ref=it->second;
 setEntityProperty(ref.level,ref.entity,ref.key,std::int64_t(std::clamp(value,-100000,100000)));
}
int Game::entityPropertyInt(int level,std::string_view entity,std::string_view key)const{
 const auto property=storeProperty(std::string(key));
 for(const auto& [id,ref]:m_entityProperties)if(ref.level==level&&ref.entity==entity&&ref.key==property)return entityPropertyInt(id);
 return 0;
}
}
