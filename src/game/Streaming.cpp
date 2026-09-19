#include "Game.h"
#include <fstream>
namespace retro {
void Game::ensureChunk(int level){
 auto&chunk=m_chunks[level];if(chunk.resident)return;auto doors=chunk.world.doors();bool control=chunk.world.controlReleased();
 auto saved=chunk.world;chunk.world=World(level);chunk.world.restoreLift(saved);chunk.world.restoreDoors(doors);if(control)chunk.world.releaseControl();chunk.resident=true;
}
void Game::useDoor(int index){
 auto door=m_world.doors()[index];m_world.toggleDoor(index);bool opening=!door.opening;
 if(door.transfer&&m_level+1<ChunkCount){ensureChunk(m_level+1);auto&next=m_chunks[m_level+1].world;next.setDoor(0,door.open,opening);}
 if(door.entry&&m_level>0){ensureChunk(m_level-1);auto&previous=m_chunks[m_level-1].world;previous.setDoor(int(previous.doors().size())-1,door.open,opening);}
 sound(Sound::Door,.65f);
}
void Game::updateStreaming(float dt){
 // Doors form a short transfer vestibule. Keep both geometries until the
 // opaque leaves fully close, then release static geometry, retaining state.
 for(int index=0;index<int(m_world.doors().size());++index){auto door=m_world.doors()[index];
  if(!door.opening&&door.open>0&&m_player.pos.x>door.left-.25f&&m_player.pos.x<door.right+.25f&&std::fabs(m_player.pos.y-door.y)<.55f){m_world.openDoor(index);door=m_world.doors()[index];}
  if(door.transfer&&m_level+1<ChunkCount){auto&next=m_chunks[m_level+1].world;next.setDoor(0,door.open,door.opening);}
  if(door.entry&&m_level>0){auto&previous=m_chunks[m_level-1].world;previous.setDoor(int(previous.doors().size())-1,door.open,door.opening);}
 }
 for(int level=0;level<ChunkCount;++level)if(level!=m_level){bool needed=false;
  if(level==m_level+1){auto&d=m_world.doors().back();needed=d.opening||d.open>0;}
  if(level==m_level-1){auto&d=m_world.doors().front();needed=d.opening||d.open>0;}
  if(needed)ensureChunk(level);
  else if(m_chunks[level].resident){m_chunks[level].world.unloadGeometry();m_chunks[level].resident=false;}
 }
 (void)dt;
}
bool Game::testStreaming(){
 Game game;if(!game.chunkResident(0)||game.chunkResident(1)||game.chunkResident(2))return false;
 game.m_enemies.clear();game.m_pickups.clear();game.m_player.pos={21.5f,20.5f};game.useDoor(int(game.world().doors().size())-1);game.updateStreaming(0);
 if(!game.chunkResident(1))return false;
 for(int i=0;i<180;++i)game.update({},1.f/120);
 game.m_player.pos={21.5f,24.1f};game.crossChunkBoundary();if(game.level()!=1||!game.chunkResident(0))return false;
 game.m_player.pos={3.5f,2.f};game.useDoor(0);game.updateStreaming(0);if(!game.chunkResident(0))return false;
 for(int i=0;i<160;++i)game.update({},1.f/120);
 if(game.chunkResident(0)||!game.m_chunks[0].world.structures().empty()||game.m_chunks[0].world.tile(3,4)!='#')return false;
 game.useDoor(0);game.updateStreaming(0);if(!game.chunkResident(0)||game.m_chunks[0].world.tile(3,4)=='#')return false;
 for(int i=0;i<160;++i)game.update({},1.f/120);
 game.m_player.pos={3.5f,-.1f};game.crossChunkBoundary();if(game.level()!=0||!game.enemies().empty()||!game.pickups().empty())return false;
 std::ofstream("streaming-test.txt")<<"Closed chunk released, opening rebuilds before visibility, closing retains until sealed, return preserves cleared enemies/pickups: PASS\n";return true;
}
}
