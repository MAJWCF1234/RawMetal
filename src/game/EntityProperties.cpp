// Map format 2 entity properties at runtime (EntityTraits.h, docs/Systems/map-format.md).
//
// A named record's custom properties (p.key=value) and a named prop's `frozen` flag are values a
// script reads and writes (@entity.key in SCRIPT_REQUIRE and SCRIPT_ACTION STATE/ADD). This file
// is the only place their values are read, written and seeded: the parser resolves each to a
// StateId (entityPropertyState) and files it here with its map, entity and key, and every caller
// (Script.cpp, Clutter.cpp's pinned props) comes through these functions.
//
// Today the values are script states, so they are saved with every other state and a load needs
// nothing new. A store keyed by map, entity and property (the save format's entity properties)
// can replace the two accessors below without changing any caller: m_entityProperties already
// knows each id's level, entity and key.
#include "Game.h"
#include <algorithm>

namespace retro {
void Game::seedEntityProperties(){
 m_entityProperties.clear();
 for(int chunk=0;chunk<chunkCount();++chunk){
  const auto& data=m_chunks[size_t(chunk)].world.m_mapData;if(!data)continue;
  for(const auto& p:data->propertySeeds){
   m_entityProperties[p.id]={chunk,p.entity,p.key};
   // An authored value is given once a game: a load keeps whatever scripts made of it since.
   if(!p.seeded)continue;const StateId seeded=entityPropertySeedState(p.id);
   if(!state(seeded)){setEntityPropertyInt(p.id,p.value);setState(seeded,1);}
  }
 }
}
int Game::entityPropertyInt(StateId id)const{return state(id);}
void Game::setEntityPropertyInt(StateId id,int value){setState(id,std::clamp(value,-100000,100000));}
int Game::entityPropertyInt(int level,std::string_view entity,std::string_view key)const{
 for(const auto& [id,ref]:m_entityProperties)if(ref.level==level&&ref.entity==entity&&ref.key==key)return entityPropertyInt(id);
 return 0;
}
}
