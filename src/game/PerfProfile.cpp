#include "Game.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <ostream>
#include <sstream>
// The game side of --perf-profile (main.cpp runs the frames and renders): a
// campaign scene, creatures added to it, and the wake that keeps every hunter
// on the player for a whole measured run. None of it changes how the AI or the
// world behaves; it only arranges who is where and who knows what.
namespace retro {
Game Game::perfScene(int level,Vec2 position,float feet,float yawDegrees,float pitch,bool openDoors){
 // A NaN position stands at the map's own player start (its spawn height unless feet is given).
 const bool start=std::isnan(position.x)||std::isnan(position.y);
 auto game=mapInspection(start?Vec2{12,12}:position,yawDegrees*kPi/180.f,pitch,level,openDoors,feet,false);
 if(start){game.m_player.pos=game.m_world.definition().playerStart;game.m_player.z=feet!=-999?feet:game.m_world.hasTerrain()?game.groundHeight(game.m_player.pos,float(World::TerrainMaxZ+1)):game.m_world.definition().spawnHeight;game.updateStreaming(0);}
 // The console's god mode: hits land and are counted, health does not fall,
 // so a run never ends early on a dead player (death stops every AI update).
 game.m_god=true;game.m_player.health=std::max(game.m_player.health,100.f);
 return game;
}
void Game::perfClearCreatures(){m_enemies.clear();}
int Game::perfSpawn(CreatureKind kind,int count,float minimum){
 // Free spots on the highest support under the player's feet, within a storey
 // of them (any ground on a terrain map), spread
 // across the chunk in a fixed scrambled order, at least `minimum` metres off.
 const float hull=creatureTraits(kind).hull;
 auto clear=[&](Vec2 p,float z){for(float x:{-.25f,0.f,.25f})for(float y:{-.25f,0.f,.25f})if(!m_world.fits(p.x+x,p.y+y,z,hull)||m_world.doorBlocks(p.x+x,p.y+y,z,hull))return false;return !m_world.solid(p.x,p.y);};
 struct Spot{unsigned order;Vec2 p;float z;};std::vector<Spot> spots;
 for(int y=1;y<23;++y)for(int x=1;x<23;++x){Vec2 p{x+.5f,y+.5f};if(length(p-m_player.pos)<minimum)continue;
  float z=m_world.hasTerrain()?groundHeight(p,float(World::TerrainMaxZ+1)):m_world.supportBelow(p.x,p.y,m_player.z+.3f);
  // The floor he stands on or the one under his ledge, never a pit below that.
  if(!m_world.hasTerrain()&&z<m_player.z-3.f)continue;
  if(!clear(p,z))continue;
  unsigned order=unsigned(x)*73856093u^unsigned(y)*19349663u;order^=order>>13;order*=0x5bd1e995u;order^=order>>15;
  spots.push_back({order,p,z});}
 std::sort(spots.begin(),spots.end(),[](const Spot&a,const Spot&b){return a.order<b.order;});
 int placed=0;std::vector<Vec2> taken;for(const auto&e:m_enemies)taken.push_back(e.pos);
 for(float spacing:{2.f,1.f})for(const auto&spot:spots){if(placed>=count)break;
  if(std::any_of(taken.begin(),taken.end(),[&](Vec2 other){return length(other-spot.p)<spacing;}))continue;
  CreatureSpawn spawn{kind,spot.p,spot.z};spawnCreature(spawn,0,false);taken.push_back(spot.p);++placed;}
 return placed;
}
void Game::perfWake(bool guards){
 // Creatures: what a heard gunshot does (updateEnemies' own wake), held on the
 // player's position so each one keeps routing to him for the whole run.
 for(auto&e:m_enemies)if(e.alive&&!dormantEntity(e.z)){const auto&traits=creatureTraits(e.kind);
  e.wakeState=0;e.awareness=std::max(e.awareness,traits.rushSpeed>0?12.f:6.f);e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;
  if(e.state==Enemy::State::Idle)e.state=Enemy::State::Investigate;}
 if(!guards)return;
 // Guards hostile to the player in any resident chunk: told where he is and
 // that it is him (the engineer drone tests' "known"), as a radio call would.
 AiTarget target;target.kind=ShotKind::Player;target.level=m_level;target.index=0;target.position=m_player.pos;target.feet=m_player.z;target.height=m_player.hullHeight();target.eye=m_player.eye;target.faction=AiFaction::Player;
 Vec2 global=m_player.pos+chunkOffset(m_level);
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();
  if(t.ai.mode!=ActorAiMode::Guard||!chunkResident(npc.level)||!autonomousActor(t)||npcIncapacitated(i)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||!factionsHostile(npcFaction(t),AiFaction::Player))continue;
  rememberGuardTarget(i,target);
  if(guardState(i)!=GuardState::Alert)setState(friendlyKey(i,"guard_mode"),int(GuardState::Investigate));
  setState(friendlyKey(i,"guard_confirmed"),1);setState(friendlyKey(i,"guard_memory"),1);
  setState(friendlyKey(i,"guard_x"),int(std::round(global.x*1000)));setState(friendlyKey(i,"guard_y"),int(std::round(global.y*1000)));setState(friendlyKey(i,"guard_z"),int(std::round(m_player.z*1000)));
 }
}
void Game::perfHold(){m_player.health=std::max(m_player.health,100.f);}
std::string Game::perfPopulation()const{
 std::ostringstream out;std::map<std::string,std::pair<int,int>> kinds;int alive=0,hunting=0;
 for(const auto&e:m_enemies)if(e.alive){++alive;auto&k=kinds[creatureTraits(e.kind).name];++k.first;if(e.awareness>0){++hunting;++k.second;}}
 int resident=0;for(int level=0;level<chunkCount();++level)resident+=chunkResident(level);
 int guards=0,autonomous=0,hostile=0,alert=0,civilians=0;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(!chunkResident(npc.level))continue;
  if(t.ai.mode!=ActorAiMode::Guard){++civilians;continue;}
  ++guards;if(!autonomousActor(t)||npcIncapacitated(i)||npc.pose.clip==4)continue;++autonomous;
  if(factionsHostile(npcFaction(t),AiFaction::Player))++hostile;if(guardState(i)==GuardState::Alert)++alert;}
 out<<"creatures="<<alive<<" hunting="<<hunting;for(const auto&[name,k]:kinds)out<<" ["<<name<<' '<<k.first<<'/'<<k.second<<']';
 out<<" resident_chunks="<<resident<<" npcs_total="<<m_friendlyActors.size()<<" resident_guards="<<guards<<" autonomous="<<autonomous<<" hostile="<<hostile<<" alert="<<alert<<" resident_civilians="<<civilians
    <<" layers="<<m_world.layers().size()<<" terrain="<<m_world.hasTerrain()<<" doors="<<m_world.doors().size()<<" fixtures="<<m_world.fixtures().size()<<" props="<<m_world.props().size()<<" structures="<<m_world.structures().size()<<" states="<<m_states.size();
 return out.str();
}
void Game::perfCensus(std::ostream& out){
 // Every campaign map: who lives there and what its collision queries walk.
 // A chunk away from the player is a shell until streamed; load each in full.
 Game game;
 out<<"level\tname\tgroup\tlayers\tterrain\tdoors\tfixtures\tprops\tstructures\tterminals\tspawned_creatures\tscript_spawns\tguards\tother_npcs\tby_kind\n";
 for(int level=0;level<game.chunkCount();++level){if(level!=game.m_level)game.ensureChunk(level);const auto&w=game.chunkWorld(level);std::map<std::string,int> kinds;int scripted=0;
  for(const auto&spawn:w.creatureSpawns())++kinds[creatureTraits(spawn.kind).name];
  for(const auto&event:w.scriptEvents())for(const auto&action:event.actions)if(action.type==ScriptAction::Type::SpawnEnemy){++scripted;++kinds[std::string(creatureTraits(action.enemyKind).name)+"(script)"];}
  int guards=0,others=0;for(const auto&t:w.actorTracks())(t.ai.mode==ActorAiMode::Guard?guards:others)++;
  out<<level<<'\t'<<(w.m_mapData?w.m_mapData->name:std::string("-"))<<'\t'<<w.definition().residencyGroup<<'\t'<<w.layers().size()<<'\t'<<w.hasTerrain()<<'\t'<<w.doors().size()<<'\t'<<w.fixtures().size()<<'\t'<<w.props().size()<<'\t'<<w.structures().size()<<'\t'<<w.terminals().size()<<'\t'<<w.creatureSpawns().size()<<'\t'<<scripted<<'\t'<<guards<<'\t'<<others<<'\t';
  bool first=true;for(const auto&[name,n]:kinds){out<<(first?"":", ")<<name<<' '<<n;first=false;}out<<'\n';
 }
}
}
