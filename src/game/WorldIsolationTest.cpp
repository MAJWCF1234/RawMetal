#include "Game.h"
#include <fstream>
#include <queue>
namespace retro {
bool Game::testWorldIsolation(){
 std::ofstream out("world-isolation-test.txt");
 auto check=[&](bool ok,const char* label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 Game campaign;Game custom(WorldId::Ashfall);Game independent;
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
  if(!check(w.outdoors()&&!w.hasLift()&&!w.insideLift(12,12)&&w.floorHeight(12,12)==0&&w.ceilingHeight(12,12)>100&&w.waterSurface(9,9)<-100&&w.particleEmitters().empty(),"No campaign floor, ceiling, lift, water or effects"))return false;
  if(!check(w.lights().empty(),"Outdoor map has no unsupported ceiling lamps"))return false;
  if(!check(custom.hullFits(custom.player().pos,0,1),"Whole player hull fits spawn"))return false;
  for(const auto&e:custom.enemies())if(!check(w.fits(e.pos.x,e.pos.y,e.z,e.bodyTop()-e.z),"Creature spawn fits geometry"))return false;
  // Flood-fill walkable half-metre cells, including a full 40 cm player hull.
  constexpr int N=48;std::array<bool,N*N> seen{};std::queue<int> pending;
  int start=int(custom.player().pos.y*2)*N+int(custom.player().pos.x*2);seen[start]=true;pending.push(start);
  while(!pending.empty()){int p=pending.front();pending.pop();int x=p%N,y=p/N;
   for(auto d:std::array<Vec2,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<0||ny<0||nx>=N||ny>=N)continue;int q=ny*N+nx;
    if(!seen[q]&&custom.hullFits({(nx+.5f)*.5f,(ny+.5f)*.5f},0,1)){seen[q]=true;pending.push(q);}}
  }
  if(level<5&&!check(seen[46*N+24],"Spawn can reach southern seam"))return false;
  if(level>0&&!check(seen[1*N+24],"Spawn can reach northern seam"))return false;
  if(level<5){
   custom.m_player.pos={12,23.5f};custom.m_player.angle=kPi*.5f;custom.m_velocity={};custom.m_enemies.clear();InputState walk{};walk.forward=true;
   for(int i=0;i<40;++i)custom.update(walk,1.f/120);
   if(!check(custom.level()==level+1&&std::fabs(custom.player().pos.x-12)<.01f&&std::fabs(custom.player().z)<.01f,"Actual forward seam crossing preserves position and height"))return false;
   custom.m_player.angle=-kPi*.5f;custom.m_velocity={};
   for(int i=0;i<50;++i)custom.update(walk,1.f/120);
   if(!check(custom.level()==level&&std::fabs(custom.player().pos.x-12)<.01f,"Actual return seam crossing"))return false;
  }
 }
 auto campaignSave=campaign.encodeSave(),customSave=custom.encodeSave();
 if(!check(campaign.decodeSave(customSave)&&campaign.worldId()==WorldId::Ashfall&&campaign.world().outdoors()&&campaign.m_scriptEvents.empty(),"Custom save restores world in campaign session"))return false;
 if(!check(custom.decodeSave(campaignSave)&&custom.worldId()==WorldId::Campaign&&custom.world().campaign()&&!custom.m_scriptEvents.empty(),"Campaign save restores world in custom session"))return false;
 // Reloading unloaded geometry must use the owning world, not the last menu choice.
 campaign.m_chunks[0].world.unloadGeometry();campaign.m_chunks[0].resident=false;campaign.ensureChunk(0);
 if(!check(campaign.m_chunks[0].world.worldId()==WorldId::Ashfall&&campaign.m_chunks[0].world.floorHeight(18,10)==0,"Unloaded custom geometry restores from correct world"))return false;
 Game menu;menu.showTitleScreen();menu.m_titleSelection=1;InputState accept{};accept.menuAccept=true;menu.update(accept,.01f);menu.update({},.01f);menu.update(accept,.01f);
 if(!check(menu.worldId()==WorldId::Ashfall&&!menu.titleScreen(),"Title browser loads custom world"))return false;
 menu.showTitleScreen();menu.update(accept,.01f);
 return check(menu.worldId()==WorldId::Campaign&&!menu.titleScreen()&&!menu.world().outdoors(),"New Game returns to campaign");
}
}
