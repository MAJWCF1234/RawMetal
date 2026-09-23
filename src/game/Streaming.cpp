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
 if(m_worldId==WorldId::Ashfall){
  // Keep the 3x3 neighbourhood around the active outdoor chunk resident.
  // With the current 3x2 Ashfall layout this is at most six tiny regions, but
  // the rule scales without hard-coding level-number adjacency.
  auto current=chunkOffset(m_level);
  for(int level=0;level<ChunkCount;++level)if(level!=m_level){auto origin=chunkOffset(level);
   bool needed=std::fabs(origin.x-current.x)<=World::Width+.01f&&std::fabs(origin.y-current.y)<=World::Height+.01f;
   if(needed)ensureChunk(level);
   else if(m_chunks[level].resident){m_chunks[level].world.unloadGeometry();m_chunks[level].resident=false;}
  }
  (void)dt;return;
 }
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
 auto fail=[](int stage){std::ofstream("streaming-debug.txt")<<"Failed at stage "<<stage<<'\n';return false;};
 Game game;if(!game.chunkResident(0)||game.chunkResident(1)||game.chunkResident(2))return fail(1);
 game.m_enemies.clear();game.m_pickups.clear();game.m_player.pos={21.5f,20.5f};game.useDoor(int(game.world().doors().size())-1);game.updateStreaming(0);
 if(!game.chunkResident(1))return fail(2);
 for(int i=0;i<180;++i)game.update({},1.f/120);
 game.m_player.pos={21.5f,24.1f};game.crossChunkBoundary();if(game.level()!=1||!game.chunkResident(0))return fail(3);
 game.m_player.pos={3.5f,2.f};game.useDoor(0);game.updateStreaming(0);if(!game.chunkResident(0))return fail(4);
 for(int i=0;i<160;++i)game.update({},1.f/120);
 if(game.chunkResident(0)||!game.m_chunks[0].world.structures().empty()||game.m_chunks[0].world.tile(3,4)!='#')return fail(5);
 game.useDoor(0);game.updateStreaming(0);if(!game.chunkResident(0)||game.m_chunks[0].world.tile(3,4)=='#')return fail(6);
 for(int i=0;i<160;++i)game.update({},1.f/120);
 game.m_player.pos={3.5f,-.1f};game.crossChunkBoundary();if(game.level()!=0||!game.enemies().empty()||!game.pickups().empty())return fail(7);
 Game joined;joined.m_level=4;joined.restart();
 if(!joined.chunkResident(5)||joined.world().tile(0,23)!='#'||joined.world().tile(1,23)=='#'||joined.m_chunks[5].world.tile(0,0)!='#'||joined.m_chunks[5].world.tile(1,0)=='#')return fail(8);
 Vec2 probe{12.f,24.1f};if(joined.worldAt(probe).level()!=5||std::fabs(probe.x-12.f)>.001f||std::fabs(probe.y-.1f)>.001f)return fail(9);
 joined.m_player.pos={12.f,24.1f};joined.crossChunkBoundary();
 if(joined.level()!=5||std::fabs(joined.player().pos.x-12.f)>.001f||std::fabs(joined.player().pos.y-.1f)>.001f||!joined.chunkResident(4))return fail(10);
 // Exercise real movement in both directions, not just a boundary teleport.
 joined.m_player.pos={12,1};joined.m_player.z=-9;joined.m_player.grounded=true;joined.m_player.angle=-kPi*.5f;joined.m_velocity={};
 InputState walking{};walking.forward=true;
 for(int i=0;i<90;++i)joined.update(walking,1.f/120);
 if(joined.level()!=4||!joined.chunkResident(5)||std::fabs(joined.player().z+9)>.001f)return fail(11);
 joined.m_player.angle=kPi*.5f;joined.m_velocity={};
 for(int i=0;i<110;++i)joined.update(walking,1.f/120);
 if(joined.level()!=5||!joined.chunkResident(4)||std::fabs(joined.player().z+9)>.001f)return fail(12);

 // Ashfall uses the same resident-chunk renderer/collision path in two axes.
 Game ash(WorldId::Ashfall);
 if(ash.world().terrain().size()<200)return fail(16);
 if(!ash.chunkResident(1)||!ash.chunkResident(3)||!ash.chunkResident(4)||ash.chunkResident(2))return fail(22);
 float eastA=ash.world().floorHeight(23.999f,12),eastB=ash.m_chunks[1].world.floorHeight(.001f,12);
 if(std::fabs(eastA-eastB)>.01f)return fail(17);
 ash.m_player.pos={24.10f,12.f};ash.crossChunkBoundary();
 if(ash.level()!=1||std::fabs(ash.player().pos.x-.10f)>.01f||std::fabs(ash.player().pos.y-12.f)>.01f)return fail(18);
 ash.updateStreaming(0);
 if(!ash.chunkResident(0)||!ash.chunkResident(2)||!ash.chunkResident(3)||!ash.chunkResident(4)||!ash.chunkResident(5))return fail(19);
 float southA=ash.world().floorHeight(12,23.999f),southB=ash.m_chunks[3].world.floorHeight(12,.001f);
 if(std::fabs(southA-southB)>.02f)return fail(20);
 // Central Ashfall ridge contains a real subtractive cavern. At local 12,8
 // in chunk 4 there must be a walkable floor with a solid ceiling above it.
 const auto& cave=ash.m_chunks[4].world;float caveFloor=cave.supportBelow(12,8,1.5f),caveRoof=cave.clearanceAbove(12,8,caveFloor+.02f);
 if(caveRoof-caveFloor<2.2f||caveRoof>8.f||!cave.fits(12,8,caveFloor,1.f)||cave.fits(12,8,caveFloor,5.f))return fail(23);
 ash.m_player.pos={12.f,24.10f};ash.crossChunkBoundary();
 if(ash.level()!=4||std::fabs(ash.player().pos.x-12.f)>.01f||std::fabs(ash.player().pos.y-.10f)>.01f)return fail(21);
 // The rear service store has a real door. Its closed leaf blocks a sprint;
 // once opened, the back wall still stops the player inside the chunk.
 joined.m_player.pos={19.75f,19.2f};joined.m_player.z=-9;joined.m_velocity={};walking.sprint=true;
 for(int i=0;i<120;++i)joined.update(walking,1.f/120);
 if(joined.player().pos.y>20.7f||joined.player().pos.y<20.2f||!joined.hullFits(joined.player().pos,-9,1))return fail(13);
 joined.useDoor(0);joined.m_world.updateDoors(2);joined.m_velocity={};
 for(int i=0;i<120;++i)joined.update(walking,1.f/120);
 if(joined.player().pos.y<21.1f||joined.player().pos.y>22.8f||!joined.hullFits(joined.player().pos,-9,1))return fail(15);
 // A player can walk up the authored basin ramp without jumping.
 joined.m_player.pos={9,9};joined.m_player.z=joined.world().floorHeight(9,9);joined.m_player.angle=-kPi*.5f;joined.m_velocity={};walking.sprint=false;
 for(int i=0;i<100;++i)joined.update(walking,1.f/120);
 if(joined.player().pos.y>=7.7f||std::fabs(joined.player().z+9)>.08f)return fail(14);
 std::ofstream("streaming-test.txt")<<"Door streaming and state retention: PASS\nAligned 24x48 seam stays resident, side walls remain continuous, and crossing preserves X: PASS\nWalking across the seam in both directions at reactor elevation: PASS\nAshfall 3x2 neighbour residency, east/south crossing, Surface Nets seams and volumetric cave collision: PASS\nRear store door and wall collision, then walking out of flooded returns: PASS\n";return true;
}
}
