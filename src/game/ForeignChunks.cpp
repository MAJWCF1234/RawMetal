#include "Game.h"
#include "../core/FrameProfile.h"
#include <algorithm>
#include <cmath>
// Several maps at once (docs/Systems/multiplayer.md "Several maps"). A co-op host runs every map a player stands
// on, not only its own. Everything that thinks (creatures, guards, people, scripts) was written against the active
// chunk: m_world, m_enemies, m_clutter, the players in its frame. So another map gets a frame of its own: for a
// moment it is the active chunk (ChunkFrame swaps it in and the host's own map out, keeping both whole), its minds
// run against the players on it, and the host's map comes back. Doors and props of every resident map already
// run in the host's own frame (updateStreaming, updateClutter), so they are not run twice.
namespace retro {

bool Game::stitchedTo(int a,int b)const{
 if(a==b)return true;if(a<0||b<0||a>=chunkCount()||b>=chunkCount())return false;
 const Vec2 d=chunkOffset(a)-chunkOffset(b);
 if(std::fabs(d.x)<.01f&&std::fabs(d.y)<.01f)return false;   // two maps on one origin: not side by side
 return std::max(0.f,std::fabs(d.x)-World::Width)<.5f&&std::max(0.f,std::fabs(d.y)-World::Height)<.5f;
}
bool Game::inPlayerFrame(int level)const{return level==m_level||(chunkResident(level)&&stitchedTo(level,m_level));}

Game::ChunkFrame::ChunkFrame(Game& game,int lvl):g(game),home(game.m_level),level(lvl){
 if(lvl==home||lvl<0||lvl>=g.chunkCount()||!g.chunkResident(lvl))return;
 auto& a=g.m_chunks[size_t(home)];auto& f=g.m_chunks[size_t(lvl)];
 // The live active map into its own slot, the other map into the active one (a three-way turn: nothing copied).
 std::swap(a.world,g.m_world);std::swap(g.m_world,f.world);
 std::swap(a.enemies,g.m_enemies);std::swap(g.m_enemies,f.enemies);
 std::swap(a.pickups,g.m_pickups);std::swap(g.m_pickups,f.pickups);
 std::swap(a.clutter,g.m_clutter);std::swap(g.m_clutter,f.clutter);
 std::swap(a.kills,g.m_kills);std::swap(g.m_kills,f.kills);
 std::swap(a.nav,g.m_levelNav);std::swap(g.m_levelNav,f.nav);
 std::swap(a.navReady,g.m_levelNavReady);std::swap(g.m_levelNavReady,f.navReady);
 homeResident=a.resident;a.resident=true;g.m_level=lvl;swapped=true;
 held=g.m_heldClutter;carry=g.m_carryClutter;g.m_heldClutter=g.m_carryClutter=-1;
 sounds.swap(g.m_sounds);noise=g.m_playerNoise;g.m_playerNoise={};
 // Bodies are kept in the active chunk's frame.
 delta=g.chunkOffset(home)-g.chunkOffset(lvl);for(auto& r:g.m_remoteBodies)r.body.pos+=delta;
 g.m_foreignFrame=true;g.m_foreignHome=home;
 g.m_foreignLocal=g.m_player;g.m_foreignLocal.pos+=delta;
}
Game::ChunkFrame::~ChunkFrame(){
 if(!swapped)return;
 auto& a=g.m_chunks[size_t(home)];auto& f=g.m_chunks[size_t(level)];
 // What sounded on that map is heard here only across a seam beside this one.
 std::vector<SoundEvent> there;there.swap(g.m_sounds);g.m_sounds.swap(sounds);
 if(g.stitchedTo(home,level))for(auto s:there){if(s.spatial||std::isfinite(s.z))s.position=s.position-delta;g.m_sounds.push_back(s);}
 for(auto& r:g.m_remoteBodies)r.body.pos=r.body.pos-delta;
 g.m_playerNoise=noise;g.m_heldClutter=held;g.m_carryClutter=carry;
 g.m_foreignFrame=false;g.m_foreignHome=-1;g.m_level=home;a.resident=homeResident;
 std::swap(g.m_world,f.world);std::swap(a.world,g.m_world);
 std::swap(g.m_enemies,f.enemies);std::swap(a.enemies,g.m_enemies);
 std::swap(g.m_pickups,f.pickups);std::swap(a.pickups,g.m_pickups);
 std::swap(g.m_clutter,f.clutter);std::swap(a.clutter,g.m_clutter);
 std::swap(g.m_kills,f.kills);std::swap(a.kills,g.m_kills);
 std::swap(g.m_levelNav,f.nav);std::swap(a.nav,g.m_levelNav);
 std::swap(g.m_levelNavReady,f.navReady);std::swap(a.navReady,g.m_levelNavReady);
}

// After the host's own frame: each other occupied map's minds, in that map's frame.
void Game::updateForeignChunks(float dt){
 if(m_netForeign.empty()||m_titleScreen||m_loadJob||m_netCreaturesRemote)return;
 dt=std::clamp(dt,0.f,.05f);if(dt<=0)return;
 for(int level=0;level<chunkCount()&&size_t(level)<m_netForeign.size();++level){
  if(level==m_level||!m_netForeign[size_t(level)])continue;
  if(!chunkResident(level))ensureChunk(level);
  ChunkFrame frame(*this,level);if(!frame.swapped||players().empty())continue;
  for(auto& e:m_enemies)if(!e.alive)e.deathTime+=dt;
  {perf::Scope profile(perf::Scripts);updateScripts(dt);}
  updateFriendlyActors(dt,false);updateFriendlyActors(dt,true);
  updateGuardCombat(dt);
  for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(m_friendlyActors[actor].level==level&&m_friendlyActors[actor].track().ai.mode==ActorAiMode::Guard)finishGuardTask(actor);
  updateEnemies(dt);
 }
}

// The Match's teleport to another map: the map left keeps its state (a player may still be on it).
void Game::enterChunk(int level){
 level=std::clamp(level,0,std::max(0,chunkCount()-1));if(level==m_level)return;
 dropBody();clearLockerMotion(true);if(heldNpc()>=0)releaseNpc(true);m_heldClutter=m_carryClutter=-1;
 ensureChunk(level);storeChunk();auto& chunk=m_chunks[size_t(level)];
 m_world=chunk.world;m_levelNav=std::move(chunk.nav);m_levelNavReady=chunk.navReady;chunk.nav={};chunk.navReady=false;
 m_enemies=chunk.enemies;m_pickups=chunk.pickups;m_clutter=chunk.clutter;m_kills=chunk.kills;m_level=level;
 m_casings.clear();m_sounds.clear();m_liquidDrops.clear();m_puddles.clear();m_activeLog=-1;m_logTime=0;m_capture={};
}
}
