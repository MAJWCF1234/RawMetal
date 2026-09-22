#include "Game.h"
#include <fstream>
namespace retro {
void Game::ensureChunk(int level){
 auto&chunk=m_chunks[level];if(chunk.resident)return;auto doors=chunk.world.doors();bool control=chunk.world.controlReleased();
 auto saved=chunk.world;chunk.world=World(level,m_worldId);chunk.world.restoreLift(saved);chunk.world.restoreDoors(doors);if(control)chunk.world.releaseControl();chunk.resident=true;
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
  if(level==m_level+1){
   needed=m_world.openSouthBoundary();
   if(!needed&&!m_world.doors().empty()){auto&d=m_world.doors().back();needed=d.transfer&&(d.opening||d.open>0);}
  }
  if(level==m_level-1){
   needed=m_world.openNorthBoundary();
   if(!needed&&!m_world.doors().empty()){auto&d=m_world.doors().front();needed=d.entry&&(d.opening||d.open>0);}
  }
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
 Game joined;joined.m_level=4;joined.restart();
 if(!joined.chunkResident(5)||joined.world().tile(0,23)!='#'||joined.world().tile(1,23)=='#'||joined.m_chunks[5].world.tile(0,0)!='#'||joined.m_chunks[5].world.tile(1,0)=='#')return false;
 Vec2 probe{12.f,24.1f};if(joined.worldAt(probe).level()!=5||std::fabs(probe.x-12.f)>.001f||std::fabs(probe.y-.1f)>.001f)return false;
 joined.m_player.pos={12.f,24.1f};joined.crossChunkBoundary();
 if(joined.level()!=5||std::fabs(joined.player().pos.x-12.f)>.001f||std::fabs(joined.player().pos.y-.1f)>.001f||!joined.chunkResident(4))return false;
 // Exercise real movement in both directions, not just a boundary teleport.
 joined.m_player.pos={12,1};joined.m_player.z=-9;joined.m_player.grounded=true;joined.m_player.angle=-kPi*.5f;joined.m_velocity={};
 InputState walking{};walking.forward=true;
 for(int i=0;i<90;++i)joined.update(walking,1.f/120);
 if(joined.level()!=4||!joined.chunkResident(5)||std::fabs(joined.player().z+9)>.001f)return false;
 joined.m_player.angle=kPi*.5f;joined.m_velocity={};
 for(int i=0;i<110;++i)joined.update(walking,1.f/120);
 if(joined.level()!=5||!joined.chunkResident(4)||std::fabs(joined.player().z+9)>.001f)return false;
 // Closed endpoint must stop a running player before the world boundary.
 joined.m_player.pos={19.5f,22};joined.m_velocity={};walking.sprint=true;
 for(int i=0;i<120;++i)joined.update(walking,1.f/120);
 if(joined.player().pos.y>23.451f||joined.player().pos.y<23.4f||!joined.hullFits(joined.player().pos,-9,1))return false;
 // A player can walk out of the 18 cm flooded return without jumping.
 joined.m_player.pos={9,9};joined.m_player.z=-9.18f;joined.m_player.angle=-kPi*.5f;joined.m_velocity={};walking.sprint=false;
 for(int i=0;i<60;++i)joined.update(walking,1.f/120);
 if(joined.player().pos.y>=8.3f||std::fabs(joined.player().z+9)>.001f)return false;
 std::ofstream("streaming-test.txt")<<"Door streaming and state retention: PASS\nAligned 24x48 seam stays resident, side walls remain continuous, and crossing preserves X: PASS\nWalking across the seam in both directions at reactor elevation: PASS\nEnd bulkhead collision and walking out of flooded returns: PASS\n";return true;
}
}
