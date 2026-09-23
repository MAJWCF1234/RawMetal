#include "Game.h"
#include <fstream>
#include <queue>
namespace retro {
bool Game::testWorldIsolation(){
 std::ofstream out("world-isolation-test.txt");
 auto check=[&](bool ok,const char* label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 Game campaign;Game custom(WorldId::Ashfall);Game independent;
 // Keep authored encounter composition stable while moving ownership out of Game.
 constexpr int creatureCounts[]={8,9,6,3,0,0},pickupCounts[]={4,6,4,3,0,0};
 for(int level=0;level<ChunkCount;++level){
  const auto& chunk=campaign.m_chunks[level];
  if(!check(int(chunk.enemies.size())==creatureCounts[level]&&int(chunk.pickups.size())==pickupCounts[level]&&chunk.clutter.size()==(level<4?6u:0u),"Campaign population preserved"))return false;
 }
 const auto& warden=campaign.m_chunks[3].enemies.back();
 if(!check(warden.kind==CreatureKind::Warden&&warden.hp==320&&warden.pos.x==21.5f&&warden.pos.y==18.5f,"Reactor encounter keeps its authored creature and health"))return false;
 if(!check(campaign.m_chunks[2].clutter[4].z==3&&campaign.m_chunks[2].clutter[5].z==3,"Upper-deck clutter retains explicit elevation"))return false;
 // Any map can author an ordinary locked door or an unrestricted transfer.
 Game gate(WorldId::Ashfall);Door door{2,5,6};door.requireState=stateId("gate_power");door.requireValue=2;
 gate.m_world.m_doors={door};gate.m_player.pos={3.5f,5.2f};gate.m_player.angle=kPi*.5f;
 InputState use{};use.use=true;gate.updateInteraction(use,.01f);
 if(!check(gate.doorLocked(door)&&!gate.world().doors()[0].opening,"Script-locked ordinary door rejects interaction"))return false;
 gate.setState(door.requireState,1);
 if(!check(gate.doorLocked(door),"Door requires the authored state value"))return false;
 gate.setState(door.requireState,2);gate.updateInteraction({},.01f);gate.updateInteraction(use,.01f);
 if(!check(!gate.doorLocked(door)&&gate.world().doors()[0].opening,"Script state unlocks ordinary door through player interaction"))return false;
 Door transfer;transfer.transfer=true;
 if(!check(gate.enemiesRemaining()>0&&!gate.doorLocked(transfer),"Transfer connection alone does not impose combat lock"))return false;
 transfer.requireEnemiesClear=true;
 if(!check(gate.doorLocked(transfer),"Map-authored combat lock is enforced"))return false;
 gate.m_enemies.clear();transfer.requireControl=true;
 if(!check(gate.doorLocked(transfer),"Map-authored control lock is enforced"))return false;
 gate.m_world.releaseControl();
 if(!check(!gate.doorLocked(transfer),"Cleared encounter and control release unlock door"))return false;
 if(!check(campaign.world().campaign()&&independent.world().campaign()&&!custom.world().campaign(),"Worlds coexist without global state"))return false;
 if(!check(custom.m_scriptEvents.empty()&&!campaign.m_scriptEvents.empty(),"Campaign script content is isolated"))return false;
 // The same trigger/action executor works in a custom world without level-ID branches.
 ScriptEvent trigger;trigger.id=stateId("isolation_trigger");trigger.x1=0;trigger.y1=0;trigger.x2=24;trigger.y2=24;
 ScriptAction action;action.type=ScriptAction::Type::SetState;action.id=stateId("custom_trigger_fired");action.value=7;
 trigger.actions.push_back(action);custom.m_chunks[0].world.m_scriptEvents.push_back(trigger);custom.seedScripts();custom.updateScripts(.01f);
 if(!check(custom.state(action.id)==7,"Reusable script executes custom-authored trigger"))return false;
 custom.m_chunks[0].world.m_scriptEvents.clear();custom.seedScripts();
 for(int level=0;level<ChunkCount;++level){
  out<<"Chunk "<<level<<'\n';custom.loadLevel(level,false);custom.updateStreaming(0);const auto&w=custom.world();
  if(!check(w.outdoors()&&!w.hasLift()&&!w.insideLift(12,12)&&w.hasTerrain()&&w.terrain().size()==size_t(World::Width*World::Height*2)&&w.ceilingHeight(12,12)>100&&w.waterSurface(9,9)<-100&&w.particleEmitters().empty(),"Outdoor world owns terrain, sky clearance and no campaign-only systems"))return false;
  if(!check(w.lights().empty(),"Outdoor map has no unsupported ceiling lamps"))return false;
  if(!check(std::fabs(custom.player().z-w.floorHeight(custom.player().pos.x,custom.player().pos.y))<.001f&&custom.hullFits(custom.player().pos,custom.player().z,1),"Whole player hull starts on generated terrain"))return false;
  for(const auto&e:custom.enemies())if(!check(w.fits(e.pos.x,e.pos.y,e.z,e.bodyTop()-e.z),"Creature spawn fits geometry"))return false;
  // Flood-fill walkable half-metre cells using the terrain surface as feet.
  constexpr int N=48;std::array<bool,N*N> seen{};std::queue<int> pending;
  int start=int(custom.player().pos.y*2)*N+int(custom.player().pos.x*2);seen[start]=true;pending.push(start);
  while(!pending.empty()){int p=pending.front();pending.pop();int x=p%N,y=p/N;
   for(auto d:std::array<Vec2,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<0||ny<0||nx>=N||ny>=N)continue;int q=ny*N+nx;Vec2 probe{(nx+.5f)*.5f,(ny+.5f)*.5f};float feet=w.floorHeight(probe.x,probe.y);
    if(!seen[q]&&custom.hullFits(probe,feet,1)){seen[q]=true;pending.push(q);}}
  }
  if(level%3<2&&!check(seen[24*N+46],"Spawn can reach eastern seam"))return false;
  if(level%3>0&&!check(seen[24*N+1],"Spawn can reach western seam"))return false;
  if(level<3&&!check(seen[46*N+24],"Spawn can reach southern seam"))return false;
  if(level>=3&&!check(seen[1*N+24],"Spawn can reach northern seam"))return false;
 }
 auto campaignSave=campaign.encodeSave(),customSave=custom.encodeSave();
 if(!check(campaign.decodeSave(customSave)&&campaign.worldId()==WorldId::Ashfall&&campaign.world().outdoors()&&campaign.m_scriptEvents.empty(),"Custom save restores world in campaign session"))return false;
 if(!check(custom.decodeSave(campaignSave)&&custom.worldId()==WorldId::Campaign&&custom.world().campaign()&&!custom.m_scriptEvents.empty(),"Campaign save restores world in custom session"))return false;
 // Reloading unloaded geometry must use the owning world, not the last menu choice.
 campaign.m_chunks[0].world.unloadGeometry();campaign.m_chunks[0].resident=false;campaign.ensureChunk(0);
 if(!check(campaign.m_chunks[0].world.worldId()==WorldId::Ashfall&&campaign.m_chunks[0].world.hasTerrain()&&campaign.m_chunks[0].world.terrain().size()==size_t(World::Width*World::Height*2),"Unloaded custom terrain restores from correct world"))return false;
 Game menu;menu.showTitleScreen();menu.m_titleSelection=1;InputState accept{};accept.menuAccept=true;menu.update(accept,.01f);menu.update({},.01f);menu.update(accept,.01f);
 if(!check(menu.worldId()==WorldId::Ashfall&&!menu.titleScreen(),"Title browser loads custom world"))return false;
 menu.showTitleScreen();menu.update(accept,.01f);
 return check(menu.worldId()==WorldId::Campaign&&!menu.titleScreen()&&!menu.world().outdoors(),"New Game returns to campaign");
}
}
