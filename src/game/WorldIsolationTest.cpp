#include "Game.h"
#include "../world/CustomCampaign.h"
#include <chrono>
#include <fstream>
#include <queue>
namespace retro {
bool Game::testWorldIsolation(){
 std::ofstream out("world-isolation-test.txt");
 auto check=[&](bool ok,const char* label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 Game campaign;Game custom(WorldId::Ashfall);Game independent;
 // Keep authored encounter composition stable while moving ownership out of Game.
 constexpr int creatureCounts[]={8,9,6,3,0,0,2,4,2,3},pickupCounts[]={4,6,4,3,0,0,1,2,4,2};
 constexpr int clutterCounts[]={6,6,6,6,0,2,2,0,6,19};
 for(int level=0;level<int(std::size(creatureCounts));++level){
  const auto& chunk=campaign.m_chunks[level];
  if(!check(int(chunk.enemies.size())==creatureCounts[level]&&int(chunk.pickups.size())==pickupCounts[level]&&int(chunk.clutter.size())==clutterCounts[level],"Campaign population preserved"))return false;
 }
 auto invalidWorldRejected=[](){try{World invalid(0,WorldId::Custom);return false;}catch(const std::invalid_argument&){return true;}};
 if(!check(invalidWorldRejected(),"Custom world construction requires explicit authored data"))return false;
 auto authored=std::make_shared<AuthoredMapData>();AuthoredLayerData floor;for(auto& row:floor.rows)row=std::string(World::Width,'.');authored->layers.push_back(floor);
 {World external(0,authored);World native(10);if(!check(external.custom()&&!external.campaign()&&native.campaign()&&!native.custom(),"Shared authored geometry preserves custom and native identities"))return false;}
 {
  World flat(0,authored);flat.buildLightOcclusion();
  if(!check(flat.m_lightSolids.size()==2&&flat.lightRayClear({2,2},3.2f,{20,20},3.2f)&&!flat.lightRayClear({2,2},3.6f,{20,20},3.6f),"Constant ceiling merges to one optical solid without changing its shadow"))return false;
  // Isolate the Foundry's authored 3.1/4.2/3.6-metre height bands and its
  // single low ceiling tile, so fixture shadows cannot mask a merge error.
  World varied(0);std::array<std::string,24> rows;MapRows openRows{};for(size_t i=0;i<rows.size();++i){rows[i]=std::string(24,'.');openRows[i]=rows[i];}
  varied.m_layers={{"Ceiling regression",0,0,openRows}};varied.m_structures.clear();varied.m_fixtures.clear();varied.m_props.clear();varied.m_terminals.clear();varied.m_doors.clear();varied.m_lightSolids.clear();varied.m_lightNodes.clear();
  bool ceiling=varied.lightRayClear({1.5f,4.5f},2.8f,{22.5f,4.5f},2.8f)&&!varied.lightRayClear({1.5f,4.5f},3.2f,{22.5f,4.5f},3.2f)&&varied.lightRayClear({1.5f,4.5f},4.25f,{22.5f,4.5f},4.25f);
  ceiling&=varied.lightRayClear({1.5f,11.5f},3.25f,{22.5f,11.5f},3.25f)&&!varied.lightRayClear({1.5f,11.5f},4.5f,{22.5f,11.5f},4.5f);
  ceiling&=!varied.lightRayClear({4.5f,7.5f},3.5f,{4.5f,8.5f},3.5f)&&!varied.lightRayClear({4.5f,8.5f},3.5f,{4.5f,7.5f},3.5f)&&varied.lightRayClear({4.5f,7.5f},4.15f,{4.5f,8.5f},4.15f);
  ceiling&=!varied.lightRayClear({9.5f,14.5f},1.1f,{11.5f,14.5f},1.1f)&&!varied.lightRayClear({11.5f,14.5f},1.1f,{9.5f,14.5f},1.1f)&&varied.lightRayClear({9.5f,13.5f},1.1f,{11.5f,13.5f},1.1f);
  if(!check(ceiling,"Merged ceilings preserve height steps, thickness and isolated low tiles in both ray directions"))return false;
 }
 auto rejected=[&](){try{World invalid(0,authored);return false;}catch(const std::runtime_error&){return true;}};
 {
  auto openMap=std::make_shared<AuthoredMapData>(*authored);
  openMap->stairs.push_back({3,3,5,11,0,2,12,true,true,true,.12f,true});
  World openStairs(0,openMap);auto solidMap=std::make_shared<AuthoredMapData>(*openMap);solidMap->stairs[0].openUnderside=false;World solidStairs(0,solidMap);
  if(!check(openStairs.fits(4,10,0,1.6f)&&!solidStairs.fits(4,10,0,1.6f)&&openStairs.lightRayClear({4,9},1,{4,10},1)&&!solidStairs.lightRayClear({4,9},1,{4,10},1),"Open steel stairs preserve usable, lit space beneath treads; legacy stairs remain solid"))return false;
  if(!check(openStairs.railBlocksHull(3.1f,10,.2f,1.833f,1.6f)&&!openStairs.railBlocksHull(4,10,.2f,1.833f,1.6f)&&!openStairs.railBlocksHull(3.1f,10,.2f,0,1.6f),"Stair side rails block falls while leaving the route and underside clear"))return false;
 }
 authored->stairs.push_back({3,3,5,11,0,2,12,true,true,true,-.1f});if(!check(rejected(),"Invalid stair tread thickness rejected"))return false;authored->stairs.clear();
 authored->fixtures.push_back({99,{3,3},0,1,1,1,0});if(!check(rejected(),"Invalid fixture model rejected before renderer access"))return false;authored->fixtures.clear();
 authored->lights.push_back({{3,3},2,WorldLightMount::Wall,0,1,-1});if(!check(rejected(),"Invalid authored light range rejected before renderer binning"))return false;authored->lights.clear();
 authored->props.push_back({99,{3,3},1,1,0,{.5f,.5f}});if(!check(rejected(),"Invalid prop model rejected before renderer access"))return false;authored->props.clear();
 authored->layers[0].rows[0]=".";if(!check(rejected(),"Malformed map row rejected before collision generation"))return false;authored->layers[0].rows[0]=std::string(World::Width,'.');
 authored->structures.resize(65536);if(!check(rejected(),"Excess structures rejected without a wrapping collision index"))return false;authored->structures.clear();
 {
  auto path=std::filesystem::temp_directory_path()/("rawmetal-map-validation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".txt");
  auto records=[](int id,int x){std::string text="MAP|"+std::to_string(id)+"|Validation|"+std::to_string(x)+"|0|3|3|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|"+std::to_string(id)+"|0|Floor|0|0\n";for(int row=0;row<24;++row)text+="ROW|"+std::to_string(id)+"|0|"+std::to_string(row)+"|........................\n";return text;};
  auto base=std::string("CAMPAIGN|Validation|0\n")+records(0,0);
  auto load=[&](const std::string& text){std::ofstream file(path);file<<"--- CUSTOM_CAMPAIGN_DATA_START ---\n"<<text<<"--- CUSTOM_CAMPAIGN_DATA_END ---\n";file.close();return loadCustomCampaignFile(path);};
  bool valid=false,lightContract=false;try{valid=load(base)->maps.size()==1;
   auto visibility=load(base+"VISIBLE_GROUP|0|2\nVISIBLE_GROUP|0|2\nVISIBLE_GROUP|0|3\n");
   World visibleMap(0,visibility->maps[0]);auto groups=visibleMap.visibleResidencyGroups();
   valid&=groups.size()==2&&groups[0]==2&&groups[1]==3;
   auto lamps=load(base+"LIGHT|0|5|5|3\nLIGHT|0|1|6|2|1|1.5707963|1.2|6\n");
   const auto& ceiling=lamps->maps[0]->lights[0];const auto& wall=lamps->maps[0]->lights[1];
   auto ceilingPoint=ceiling.emitter(),ceilingTarget=ceiling.shadowTarget(),wallPoint=wall.emitter(),wallTarget=wall.shadowTarget();
   lightContract=ceiling.mount==WorldLightMount::Ceiling&&ceiling.intensity==1&&ceiling.range==12&&ceilingPoint.x==5&&ceilingPoint.y==5&&ceilingPoint.z==3&&std::fabs(ceilingTarget.z-2.96f)<.0001f;
   lightContract&=wall.mount==WorldLightMount::Wall&&std::fabs(wallPoint.x-1)<.0001f&&std::fabs(wallPoint.y-6.16f)<.0001f&&wallTarget.z==2&&wall.range==6&&wall.intensity==1.2f;
   lightContract&=std::fabs(ceiling.falloff(25)-3.2f*std::pow(1.f-25.f/144.f,2.f)/(1.f+25.f*.12f))<.000001f&&wall.falloff(36)==0&&wall.falloff(35)>0;
   auto mounted=load(base+"STRUCT|0|0|1|1|20|0|3.4|0|2\nLIGHT|0|1|6|2|1|0|1.2|6\n");World mountedWall(0,mounted->maps[0]);auto target=mountedWall.lights()[0].shadowTarget();
   lightContract&=mountedWall.lightRayClear({3,6},1.5f,{target.x,target.y},target.z)&&!mountedWall.lightRayClear({3,6},1.5f,{.98f,6},2);
   valid&=lightContract;
   auto stairs=load(base+"STAIR|0|3|3|5|11|0|2|12|1|1\nSTAIR|0|6|3|8|11|0|2|12|1|1|1|0.12|1\n");
   valid&=!stairs->maps[0]->stairs[0].openUnderside&&!stairs->maps[0]->stairs[0].sideRails&&stairs->maps[0]->stairs[1].openUnderside&&stairs->maps[0]->stairs[1].sideRails&&stairs->maps[0]->stairs[1].treadThickness==.12f;
   auto mechanisms=load(base+"CARGO_LIFT|0|8|8|12|12|0|3|1|call|release|position|down|arrived|descended\nTIMED_SEQUENCE|0|timer|finished|0|0|24|24|-1|4|1000|500|0|0|2|0.5|1\nTERMINAL|0|5|5|0|0|Test|Call|Local|call|0|brake\nSIGN|0|5|8|2|3|0.9|3.14159|PLATFORM 3|ARRIVED|13277517\n");
   valid&=mechanisms->maps[0]->cargoLift.upper==3&&mechanisms->maps[0]->timedSequences.size()==1&&mechanisms->maps[0]->terminals[0].requireState==stateId("brake")&&mechanisms->maps[0]->signs.size()==1&&mechanisms->maps[0]->signs[0].title=="PLATFORM 3";
   World legacyTerminal(0,mechanisms->maps[0]);
   auto oriented=load(base+"TERMINAL|0|5|5|0|0|Test|Call|Local||0||0\n");World orientedTerminal(0,oriented->maps[0]);
   valid&=std::fabs(legacyTerminal.terminals()[0].yaw-kPi)<.001f&&oriented->maps[0]->terminals[0].yaw==0&&orientedTerminal.terminals()[0].yaw==0;
   auto actors=load(base+"TIMED_SEQUENCE|0|scene|finished|0|0|24|24|-1|4|10000|9000|0|0|2|0.5|1\nACTOR|0|scene|0|1.7|0|1|0\nACTOR_KEY|0|0|0|5|5|0|0|1|0|1\nACTOR_KEY|0|0|10000|6|5|0|0|1|4|1\nACTOR_IDLE_KEY|0|0|0|4|5|0|0|1|0\nACTOR_IDLE_KEY|0|0|2000|5|5|0|0|1|3\nACTOR_PRELUDE|0|0|1000|3000\nACTOR|0|scene|1|1|0|0|1\nACTOR_KEY|0|1|0|6|6|1|0|0|0\nACTOR_KEY|0|1|10000|6|6|1|0|0|1\nSEQUENCE_SIGHT|0|0|0|25\n");
   valid&=actors->maps[0]->actorTracks[0].idleKeys.size()==2&&actors->maps[0]->actorTracks[0].keys[0].lookAtActor==1&&actors->maps[0]->timedSequences[0].sightActor==0;
  }catch(...){}
  auto invalid=[&](const std::string& text){try{load(text);return false;}catch(const std::runtime_error&){return true;}};
  bool guarded=invalid(base+records(0,0))&&invalid(base+records(1,12))&&invalid(base+"FIXTURE|0|99|3|3|0|1|1|1|0|1\n")&&invalid(base+"PROP|0|99|3|3|1|1|0|0.5|0.5|0\n");
  guarded&=invalid(base+"VISIBLE_GROUP|0|-1\n")&&invalid(base+"VISIBLE_GROUP|0\n")&&invalid(base+"VISIBLE_GROUP|0|2|extra\n")&&invalid(base+"VISIBLE_GROUP|0|hall\n");
  guarded&=invalid(base+"TERMINAL|0|5|5|0|0|Test|Call|Local||0||nan\n");
  guarded&=invalid(base+"STAIR|0|3|3|5|11|0|2|12|1|1|1|-0.1\n")&&invalid(base+"STAIR|0|3|3|5|11|0|2|12|1|1|2|0.12\n");
  guarded&=invalid(base+"LIGHT|0|1|6|2|2\n")&&invalid(base+"LIGHT|0|1|6|2|1|nan\n")&&invalid(base+"LIGHT|0|1|6|2|1|0|-1|6\n")&&invalid(base+"LIGHT|0|1|6|2|1|0|1|0\n")&&invalid(base+"LIGHT|0|1|6|2|1|0|1|25\n")&&invalid(base+"LIGHT|0|1|6|2|1|0|1|6|extra\n");
  std::error_code cleanup;std::filesystem::remove(path,cleanup);
  if(!check(lightContract,"Ceiling lights retain compatibility and wall emitters stay outside mounting solids"))return false;
  if(!check(valid&&guarded,"Custom loader accepts valid records and rejects duplicate/overlapping maps, invalid models and malformed visible groups"))return false;
 }
 const auto& warden=campaign.m_chunks[3].enemies.back();
 if(!check(warden.kind==CreatureKind::Warden&&warden.hp==320&&warden.pos.x==21.5f&&warden.pos.y==18.5f,"Reactor encounter keeps its authored creature and health"))return false;
 if(!check(campaign.m_chunks[2].clutter[4].z==3&&campaign.m_chunks[2].clutter[5].z==3,"Upper-deck clutter retains explicit elevation"))return false;
 // Any map can author an ordinary locked door or an unrestricted transfer.
 Game gate(WorldId::Ashfall);Door door{2,5,6};door.requireState=stateId("gate_power");door.requireValue=2;
 gate.m_world.m_doors={door};gate.m_player.pos={3.5f,5.2f};gate.m_player.angle=kPi*.5f;
 gate.m_player.z=gate.m_world.floorHeight(3.5f,6);
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
 for(int level=0;level<custom.chunkCount();++level){
  out<<"Chunk "<<level<<'\n';custom.loadLevel(level,false);custom.updateStreaming(0);const auto&w=custom.world();
  if(!check(w.outdoors()&&!w.hasLift()&&!w.insideLift(12,12)&&w.hasTerrain()&&w.terrain().size()>200&&w.ceilingHeight(12,12)>100&&(w.coast()?w.waterSurface(9,9)>-1.f:w.waterSurface(9,9)<-100.f)&&w.particleEmitters().empty(),"Outdoor world owns terrain, sky clearance and no campaign-only systems"))return false;
  if(w.coast()&&!check(w.floorHeight(20,12)<w.waterSurface(20,12)-2.f&&w.floorHeight(2,12)>w.waterSurface(2,12)+.5f,"Coast has a dry bank and land beneath the sea"))return false;
  if(!check(w.lights().empty(),"Outdoor map has no unsupported ceiling lamps"))return false;
  if(!check(std::fabs(custom.player().z-custom.groundHeight(custom.player().pos,float(World::TerrainMaxZ+1)))<.001f&&custom.hullFits(custom.player().pos,custom.player().z,1),"Whole player hull starts on generated terrain"))return false;
  for(const auto&e:custom.enemies())if(!check(w.fits(e.pos.x,e.pos.y,e.z,e.bodyTop()-e.z),"Creature spawn fits geometry"))return false;
  // Flood-fill walkable half-metre cells using the terrain surface as feet.
  constexpr int N=48;std::array<bool,N*N> seen{};std::queue<int> pending;
  int start=int(custom.player().pos.y*2)*N+int(custom.player().pos.x*2);seen[start]=true;pending.push(start);
  while(!pending.empty()){int p=pending.front();pending.pop();int x=p%N,y=p/N;
   for(auto d:std::array<Vec2,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<0||ny<0||nx>=N||ny>=N)continue;int q=ny*N+nx;
    if(seen[q])continue;
    Vec2 from{(x+.5f)*.5f,(y+.5f)*.5f};float feet=custom.groundHeight(from,float(World::TerrainMaxZ+1));bool clear=true;
    // Match the full player footprint and sample the connecting slope, rather
    // than embedding the uphill corners at the centre-point floor height.
    for(int step=1;step<=8;++step){Vec2 probe=from+d*(step*.0625f);float ground=custom.groundHeight(probe,feet+.215f);
     if(std::fabs(ground-feet)>.215f||!custom.hullFits(probe,ground,1)){clear=false;break;}feet=ground;}
    if(clear){seen[q]=true;pending.push(q);}}
  }
  int col=level>=12?4:level%4,row=level>=12?level-12:level/4;
  if(col<4){World neighbour(col==3?12+row:level+1,WorldId::Ashfall);
   for(float y=.5f;y<24;y+=.5f)
    if(!check(std::fabs(w.floorHeight(24,y)-neighbour.floorHeight(0,y))<.001f,"East/west terrain support agrees at chunk boundary"))return false;
  }
  if(row<2){World neighbour(col==4?level+1:level+4,WorldId::Ashfall);
   for(float x=.5f;x<24;x+=.5f)
    if(!check(std::fabs(w.floorHeight(x,24)-neighbour.floorHeight(x,0))<.001f,"North/south terrain support agrees at chunk boundary"))return false;
  }
  if(col<4&&!check(seen[24*N+46],"Spawn can reach eastern seam"))return false;
  if(col>0&&!check(seen[24*N+1],"Spawn can reach western seam"))return false;
  if(row<2&&!check(seen[46*N+24],"Spawn can reach southern seam"))return false;
  if(row>0&&!check(seen[1*N+24],"Spawn can reach northern seam"))return false;
 }
 auto campaignSave=campaign.encodeSave(),customSave=custom.encodeSave();
 if(!check(campaign.decodeSave(customSave)&&campaign.worldId()==WorldId::Ashfall&&campaign.world().outdoors()&&campaign.m_scriptEvents.empty(),"Custom save restores world in campaign session"))return false;
 if(!check(custom.decodeSave(campaignSave)&&custom.worldId()==WorldId::Campaign&&custom.world().campaign()&&!custom.m_scriptEvents.empty(),"Campaign save restores world in custom session"))return false;
 // Reloading unloaded geometry must use the owning world, not the last menu choice.
 campaign.m_chunks[0].world.unloadGeometry();campaign.m_chunks[0].resident=false;campaign.ensureChunk(0);
 if(!check(campaign.m_chunks[0].world.worldId()==WorldId::Ashfall&&campaign.m_chunks[0].world.hasTerrain()&&campaign.m_chunks[0].world.terrain().size()>200,"Unloaded custom terrain restores from correct world"))return false;
 Game menu;menu.showTitleScreen();menu.m_titleSelection=1;InputState accept{};accept.menuAccept=true;menu.update(accept,.01f);menu.update({},.01f);menu.update(accept,.01f);
 if(!check(menu.worldId()==WorldId::Ashfall&&!menu.titleScreen(),"Title browser loads custom world"))return false;
 menu.showTitleScreen();menu.update(accept,.01f);
 return check(menu.worldId()==WorldId::Campaign&&!menu.titleScreen()&&!menu.world().outdoors(),"New Game returns to campaign");
}
}
