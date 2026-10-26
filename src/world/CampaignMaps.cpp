#include "CampaignMaps.h"
#include "CampaignKit.h"
#include "CampaignChapters.h"
#include <algorithm>
#include <cmath>
#include <tuple>
namespace retro {
namespace {
using namespace kit;
// Early contact is a doomed containment detail, not the main Cleanup battle.
// Entities take the remaining squad while one living member evacuates.
void receivingCleanup(Map& m){
 machineryClock(m,"receiving_cleanup_clock",60000);
 // Each member walks a beat through the lanes and holds at posts that watch
 // something: the overlook floor, the lane shutters, the sealed load, the
 // pressure door. At a post a guard turns to the post's bearing and scans.
 const std::vector<ActorPatrolPoint> beats[6]={
  {{{7.8f,16.f},-25,3000,-kPi*.5f},{{6.6f,10.6f},-25,2600,kPi},{{11.f,10.8f},-25,2400,kPi*.5f}},
  {{{14.6f,16.4f},-25,4200,kPi},{{17.6f,13.2f},-25,3600,-kPi*.5f},{{11.8f,13.4f},-25,3000,kPi*.5f}},
  {{{21.f,10.5f},-25,2600,-kPi*.5f},{{20.8f,4.2f},-25,3200,-kPi*.5f},{{20.8f,16.4f},-25,2800,0}},
  {{{12.f,20.2f},-25,4500,kPi},{{9.4f,20.6f},-25,3500,-kPi*.5f}},
  {{{15.f,20.5f},-25,3000,kPi},{{19.6f,19.6f},-25,3800,0}},
  {{{7.5f,12.f},-25,1800,-kPi*.5f},{{10.f,9.6f},-25,1800,kPi},{{20.8f,17.6f},-25,2200,kPi*.5f},{{11.4f,17.6f},-25,2000,0}}};
 for(int i=0;i<6;++i){const Vec2 start=beats[i].front().position;
  ActorTrack t;t.timerState=stateId("receiving_cleanup_clock");t.visual=ActorVisual(4+i);t.scale=1.6f;t.health=90;
  t.damageState=stateId("receiving_cleanup_damage_"+std::to_string(i));t.deadState=stateId("receiving_cleanup_dead_"+std::to_string(i));
  t.keys={{0,start,-25,beats[i].front().look},{60000,start,-25,beats[i].front().look,0,1}};
  auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=GuardSquadRole(i+1);ai.faction=AiFaction::Raider;ai.squad=stateId("receiving_cleanup");ai.armed=true;
  ai.weapon=(i==1||i==4)?2:1;ai.sightRange=14;ai.sightDot=.45f;ai.searchMs=6000;
  ai.speed=i==3?1.25f:i==5?2.1f:1.55f;ai.suppress=i==3;
  ai.patrol=beats[i];m.actorTracks.push_back(std::move(t));
 }
 // No extra progression gate: the personnel aisle remains an escape route.
 terminal(m,{7,21.2f},"INSPECTION / FIELD CACHE","E: TAKE FLASHLIGHT. F: TOGGLE OWNED LIGHT.","CONTAINMENT DETAIL AHEAD. WATCH THE SEALED LOADS.",0,stateId("receiving_cache_read"));
 terminal(m,{7,19.5f},"INSPECTION / CHAIN TEST","E: RATTLE THE CHAIN NORTH OF THE AISLE.","WAIT FOR HIS BACK. CROUCH CLOSE. Q KO / E GRAB.",0,stateId("receiving_inspection_lure"));
 TimedSequence lure;lure.timerState=stateId("receiving_inspection_lure");lure.finishState=stateId("receiving_inspection_lure_done");lure.x1=-2;lure.y1=-2;lure.x2=-1;lure.y2=-1;lure.durationMs=10000;lure.finishAtMs=5000;lure.soundIntervalMs=0;m.timedSequences.push_back(lure);
 cue(m,"receiving_inspection_lure",1600,Sound::JunkMetal,{8.5f,14.5f},"CHAIN TEST / INSPECTION AISLE. WATCH THE PATROL TURN.",.6f,-24.3f);
 cue(m,"receiving_inspection_lure",3200,Sound::JunkMetal,{8.5f,14.5f},"CROUCH / WAIT FOR HIS BACK / Q WHEN CLOSE.",.5f,-24.3f);
 ScriptEvent cache;cache.id=stateId("receiving_cache_award");cache.level=14;cache.x1=0;cache.y1=0;cache.x2=24;cache.y2=24;cache.requireState=stateId("receiving_cache_read");
 ScriptAction item;item.type=ScriptAction::Type::GiveItem;item.id=stateId("flashlight");item.value=1;cache.actions.push_back(item);m.scriptEvents.push_back(cache);
 wall(m,6.1f,17.3f,9,17.5f,-25,-23.15f,15);wall(m,6.1f,17.3f,9,17.5f,-23.25f,-23.15f,13);
 m.signs.push_back({{7.5f,17.52f},-23.8f,2.7f,.5f,0,"CLEANUP / HOLD","WEST / Q KO / E GRAB"});
 wallLamp(m,7.5f,17.52f,-23.4f,kPi*.5f,1.3f,5);
 // A real, player-operable pressure door gives the fleeing member a head start.
 wall(m,1,23.4f,10.5f,23.6f,-25,-22,15);wall(m,13.5f,23.4f,23,23.6f,-25,-22,15);
 Door retreat{10.5f,13.5f,23.5f,0};retreat.open=1;retreat.opening=true;m.doors.push_back(retreat);

}
void intakeBay(Map& m){
 for(float x:{6.f,17.6f})wall(m,x,22.8f,x+.4f,23.2f,-25,-18.5f,12);
 wall(m,6,22.8f,18,23.2f,-19,-18.5f,12);
 // Panelled freight barrier flanks the damaged, functioning central shutter.
 for(float x:{6.4f,13.55f}){float right=x<10?10.45f:17.6f;
  wall(m,x,22.86f,right,22.96f,-25,-19,11);
  for(float z=-24.8f;z<-19;z+=.85f)wall(m,x,22.79f,right,22.86f,z,z+.07f,2);
  for(float xx=x+.2f;xx<right;xx+=1.f)for(float z:{-24.3f,-20.f})wall(m,xx,22.765f,xx+.06f,22.79f,z,z+.06f,8);
 }
 for(float x:{9.1f,14.8f})floorLine(m,x,8,x+.10f,22.6f,-25);
 floorLine(m,9.1f,21.8f,14.9f,22.05f,-25,13);
 for(float x:{8.5f,15.5f})wall(m,x,21.3f,x+.18f,21.5f,-25,-24.05f,12);
 for(float x:{7.f,16.8f})wall(m,x,16.8f,x+.22f,17.2f,-25,-20.6f,12);
 wall(m,7,16.8f,17.02f,17.2f,-20.85f,-20.6f,12);
 // A hoist has a drum on the gantry, paired cheeks, chain, lower block and
 // curved hook. The suspended load point is not a floating black slab.
 for(float x:{11.52f,12.36f})wall(m,x,16.77f,x+.12f,17.23f,-21.28f,-20.82f,12);
 m.pipes.push_back({{11.64f,17},{12.36f,17},-21.1f,.19f,-21.1f,2});
 for(float y:{16.93f,17.07f})m.pipes.push_back({{12,y},{12,y},-21.27f,.018f,-21.98f,2});
 wall(m,11.86f,16.86f,12.14f,17.14f,-22.20f,-21.98f,18);
 m.pipes.push_back({{12,17},{12,17},-22.2f,.035f,-22.46f,2});
 m.pipes.push_back({{12,17},{12.14f,17},-22.46f,.035f,-22.55f,2});
 m.pipes.push_back({{12.14f,17},{12.25f,17},-22.55f,.035f,-22.45f,2});
 m.fixtures.push_back({8,{18.3f,22.7f},1.f,.75f,.15f,1.f,kPi,false});
 m.lights.clear();m.lights.push_back({{12,20},-20.8f});
 wallLamp(m,6.2f,22.79f,-22.7f,-kPi*.5f,1.6f,8.f);
 wallLamp(m,17.8f,22.79f,-22.7f,-kPi*.5f,1.6f,8.f);
 m.definition.ambient=.38f;
}

// FOUNDRY. The service lift that fell from the Security Office lies wrecked in
// the Intake receiving hall. South through the gate is the Foundry: a tall
// brick hall around two converters whose pour runs in an open molten channel,
// crossed on steel bridges or flanked by the west service passage; a sodium-lit
// mezzanine holds the containment console. Containment beyond is a cold
// corridor of breached holding cells ending in decontamination and the
// transfer gate to Pressure Works.
std::shared_ptr<const Map> makeFoundry(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[0];m.definition=CampaignChunks[0];m.definition.ceiling=6.4f;m.definition.ambient=.34f;
 m.layers.push_back(floorLayer("Foundry / ground",0));
 // Always index the ground layer: deck() appends layers and would invalidate a reference.
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 const float hall=6.4f,low=3.4f;

 // --- 01 INTAKE ----------------------------------------------------------
 // Lift alcove (the wrecked cab from the Security Office drop).
 rectangle(g(),5,1,6,3,'#');rectangle(g(),5,6,6,7,'#');
 // Gate wall to the Foundry: a wide freight gate and a narrow service door.
 rectangle(g(),1,7,9,8,'#');rectangle(g(),13,7,19,8,'#');rectangle(g(),21,7,23,8,'#');
 m.doors.push_back({9,13,7.5f});m.doors.push_back({19,21,7.5f});
 // Receiving floor: crate stacks and drums give cover across the approach.
 for(auto c:{std::pair{8,2},{8,3},{9,2},{13,4},{14,4},{14,5}})g().rows[c.second][c.first]='C';
 // Drums stand clear of every doorway and the lift mouth.
 for(auto b:{std::pair{12,2},{22,5},{7,1}})g().rows[b.second][b.first]='B';
 forklift(m,17.2f,4.2f,0);
 // A 1.1 m concrete dock bumper: low enough to vault with a crouch-jump.
 wall(m,10.5f,3.f,12.f,3.35f,0,1.06f,15);wall(m,10.5f,3.f,12.f,3.35f,1.06f,1.1f,13);
 // Supervisor's booth: framed walls with a real doorway, desk, chair, shelf.
 wall(m,18,1,18.15f,3.85f,0,low,3);
 wall(m,18,3.85f,19.2f,4,0,low,3);wall(m,20.4f,3.85f,23,4,0,low,3);wall(m,19.2f,3.85f,20.4f,4,2.2f,low,3);
 for(float y:{1.8f,2.9f})wall(m,18,y,18.15f,y+.6f,.95f,2.1f,17); // view glass toward the floor
 recordsDesk(m,21.3f,2.f,0);
 terminal(m,{21.3f,2.05f},"SHIFT LOG / 03:17","COOLANT FAILED. THE NIGHT CREW","SEALED FOUNDRY WITH US INSIDE.");
 shelf(m,{19.4f,1.3f},0);
 m.lights.push_back(lamp({20.6f,2.4f},low-.15f,.9f,4.f,3200));
 serviceCeiling(m,1,1,23,7,low);
 // The cab: buckled car walls and the snapped cable drum overhead.
 wall(m,1.1f,1.1f,4.9f,1.25f,0,2.6f,11);wall(m,1.1f,1.1f,1.25f,6.9f,0,2.6f,11);wall(m,1.1f,6.75f,4.9f,6.9f,0,2.6f,11);
 wall(m,1.1f,1.1f,4.9f,6.9f,2.6f,2.75f,11);
 for(float y:{2.2f,4.1f,5.8f})wall(m,1.25f,y,1.35f,y+.08f,.3f,2.5f,2);
 m.pipes.push_back({{3,1.4f},{3,1.4f},3.2f,.025f,2.75f,2});
 floorLine(m,5.2f,3.f,9,3.12f,0,13);floorLine(m,5.2f,5.88f,9,6.f,0,13);

 // --- 02 FOUNDRY ---------------------------------------------------------
 // Brick piers carry the roof; the west wall screens the service passage.
 for(auto pier:{std::pair{6,10},{6,13},{18,14}})g().rows[pier.second][pier.first]='#';
 rectangle(g(),3,10,4,16,'#');
 // A ventilation trunk has dropped across the west passage on its hangers:
 // the way through is a crawl under it.
 wall(m,1,12.f,3,13.6f,1.f,1.62f,11);
 for(float y:{12.f,13.52f})wall(m,1,y,3,y+.08f,.92f,1.7f,12);
 // Hazard banding on both faces marks the duct as something to duck under.
 for(float y:{11.97f,13.6f})wall(m,1.f,y,3.f,y+.03f,1.1f,1.3f,13);
 for(float x:{1.25f,2.75f})m.pipes.push_back({{x,12.8f},{x,12.8f},1.62f,.02f,m.definition.ceiling,2});
 // Converter plinth: two vessels, each venting up a flue into the roof.
 rectangle(g(),10,10,14,13,'T');
 for(float x:{11.f,13.f}){m.fixtures.push_back({39,{x,11.5f},0,1.9f,1.4f,2.7f,0,false});m.fixtures.push_back({40,{x,11.5f},2.7f,.4f,.4f,.7f,0,false});
  m.pipes.push_back({{x,11.5f},{x,11.5f},3.4f,.2f,hall,2});}
 m.structures.push_back({10.04f,10.04f,13.96f,12.96f,.14f,.165f,false,25});
 // The pour: an open molten channel wall to wall, crossed on two steel bridges.
 const float c1=13.2f,c2=13.85f;
 wall(m,4,c1,23,c2,0,.012f,25);
 for(float x:{7.4f,15.4f}){wall(m,x,c1-.25f,x+1.3f,c2+.25f,0,.09f,12);
  // Handrails along both sides of the walkway, open at each end.
  for(float side:{x,x+1.22f})wall(m,side,c1-.25f,side+.08f,c2+.25f,.09f,1.0f,12);}
 hazard(m,Hazard::Kind::Fire,4,c1,7.4f,c2,-1,.7f,32);
 hazard(m,Hazard::Kind::Fire,8.7f,c1,15.4f,c2,-1,.7f,32);
 hazard(m,Hazard::Kind::Fire,16.7f,c1,23,c2,-1,.7f,32);
 // Mezzanine: a steel deck along the north-east wall with the console.
 rectangle(g(),15,8,23,10,'.');deck(m,2.6f,15,8,23,10);
 m.stairs.push_back({20,10,22,14.5f,0,2.6f,13,true,false});
 m.fixtures.push_back({41,{8,15.3f},0,2.4f,1.f,1.1f,0,true});
 for(Vec2 b:{Vec2{4.3f,8.6f},Vec2{4.3f,15.4f},Vec2{22.7f,15.4f}})m.fixtures.push_back({43,b,0,.6f,.5f,hall-.2f,kPi*.5f,true});
 terminal(m,{21.f,8.6f},"CONTAINMENT / 04:06","THE VESSELS WERE NOT EMPTY.","CLEAR THE HOSTILES. GET OUT.",2.6f);
 // Roof trusses across the hall.
 for(float y:{8.5f,12.f,15.5f})wall(m,1,y,23,y+.18f,hall-.5f,hall-.3f,2);
 wallSign(m,6.f,8.06f,2.4f,"HOT METAL","CROSS ON THE BRIDGES ONLY",2.4f,0);

 // --- 03 CONTAINMENT -----------------------------------------------------
 // Openings from the Foundry: the west service passage and the east hall.
 rectangle(g(),1,16,2,17,'#');rectangle(g(),4,16,15,17,'#');rectangle(g(),19,16,23,17,'#');
 // Holding cells off a two-metre corridor, each breached, a burst vessel inside.
 rectangle(g(),1,19,2,20,'#');rectangle(g(),3,19,7,20,'#');rectangle(g(),8,19,12,20,'#');rectangle(g(),13,19,16,20,'#');
 rectangle(g(),5,20,6,23,'#');rectangle(g(),10,20,11,23,'#');rectangle(g(),15,20,16,23,'#');
 for(float x:{3.4f,8.2f,13.2f}){float h=2.2f,scale=h/2.390135f;m.fixtures.push_back({14,{x,21.6f},0,1.58f*scale,1.62f*scale,h,.3f,true});}
 for(float x:{2.f,7.f,12.f})floorLine(m,x+.05f,19.9f,x+.95f,20.05f,0,13);
 // Decontamination: steel-lined room with the exit gate.
 transferExit(m);
 equipmentPad(m,16.3f,17.3f,18.9f,19.6f,0);
 terminal(m,{17.6f,20.6f},"DISPATCH / LAST SIGNAL","PRESSURE WORKS STILL HAS POWER.","THE TRANSFER INTERLOCK IS LIVE.");
 serviceCeiling(m,1,17,23,23,3.f);

 // --- Light ---------------------------------------------------------------
 // Intake: cold fluorescent tubes; red emergency lamp in the wrecked cab.
 m.lights.push_back(lamp({8.5f,3.5f},low-.12f,1.1f,6.5f,4300,.3f));
 m.lights.push_back(lamp({14.5f,2.5f},low-.12f,1.f,6.f,4300,.3f));
 m.lights.push_back(lamp({1.25f,4.f},2.f,1.3f,4.f,1300,-.3f,WorldLightMount::Wall,0));
 // Foundry: sodium high-bays and the molten pour's own orange glow.
 m.lights.push_back(lamp({12.f,11.5f},3.6f,2.6f,9.f,1700));
 m.lights.push_back(lamp({7.f,9.f},hall-.4f,1.6f,9.f,2100));
 m.lights.push_back(lamp({16.f,14.5f},hall-.4f,1.5f,8.f,2100));
 m.lights.push_back(lamp({8.f,14.8f},hall-.4f,1.3f,7.5f,2100));
 m.lights.push_back(lamp({19.f,8.6f},5.2f,1.1f,5.5f,4300,.25f));
 m.lights.push_back(lamp({1.f,12.5f},2.4f,.9f,4.5f,4300,.35f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({1.f,11.f},1.6f,.7f,3.f,1400,-.3f,WorldLightMount::Wall,0));
 // Containment: sick green-white tubes, cells left dark, red at the gate.
 m.lights.push_back(lamp({5.5f,18.f},2.85f,1.f,5.5f,6200,.45f));
 m.lights.push_back(lamp({12.5f,18.f},2.85f,.9f,5.5f,6200,.45f));
 // Each cell keeps one weak warm caged lamp: dark, but a shape is readable.
 for(float x:{3.f,8.f,13.f})m.lights.push_back(lamp({x,23.f},2.2f,.55f,3.8f,1900,0,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({18.5f,18.5f},2.85f,1.2f,6.f,5200,.15f));
 m.lights.push_back(lamp({23.f,21.f},2.1f,1.4f,4.5f,1250,-.35f,WorldLightMount::Wall,kPi));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{15.5f,2.5f}},{C::Wasp,{21.f,5.5f}},
                   {C::Brute,{8.f,11.5f}},{C::Huntsman,{16.5f,15.f}},{C::Wasp,{18.5f,9.f},2.6f},
                   {C::Huntsman,{6.6f,22.3f}},{C::Wasp,{12.5f,18.f}},{C::Brute,{18.f,20.f}}};
 m.pickupSpawns={{{7.f,5.8f},P::Ammo},{{22.f,8.8f},P::Health,2.6f},{{1.6f,14.6f},P::Ammo},{{14.3f,22.3f},P::Health}};
 const Vec2 junk[]={{6.5f,1.8f},{6.6f,4.2f},{2.f,9.2f},{16.5f,11.5f},{3.8f,20.2f},{17.f,22.f}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,junk[i],-999,i*.7f});

 zone(m,"01 INTAKE",0,0,24,7.5f);
 zone(m,"02 FOUNDRY",0,7.5f,24,16.5f);
 zone(m,"03 CONTAINMENT",0,16.5f,24,24);
 titleCard(m,"FOUNDRY");
 return p;
}
// PRESSURE WORKS. The transfer from Containment drops into a rail receiving
// dock of stacked freight; through the pressure wall is the Pump Hall, four
// high-pressure vessels on a plinth fed by riser bundles, lit by mercury lamps;
// south lies the Control floor with the night shift's glazed booth and the
// transfer gate to the Turbine Gantry.
std::shared_ptr<const Map> makePressureWorks(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[1];m.definition=CampaignChunks[1];m.definition.ceiling=5.5f;m.definition.ambient=.32f;
 m.layers.push_back(floorLayer("Pressure Works / ground",0));
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 const float hall=5.5f;

 // --- 04 RAIL DOCK ---------------------------------------------------------
 rectangle(g(),2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true});
 rectangle(g(),1,7,3,8,'#');rectangle(g(),6,7,18,8,'#');rectangle(g(),21,7,23,8,'#');
 m.doors.push_back({3,6,7.5f});m.doors.push_back({18,21,7.5f});
 // Rail spur along the dock with freight left where the last train stopped.
 for(float y:{3.15f,3.85f})floorLine(m,6,y,23,y+.1f,0,2);
 for(float x=6.3f;x<23;x+=.8f)floorLine(m,x,3.f,x+.18f,4.1f,0,11);
 cargo(m,9.f,2.f,0);cargo(m,10.4f,2.f,0,false);cargo(m,13.5f,5.4f,0);cargo(m,12.2f,5.4f,0);
 for(auto c:{std::pair{8,5},{8,6},{17,1},{18,1}})g().rows[c.second][c.first]='C';
 g().rows[5][21]='B';
 forklift(m,15.6f,4.9f,0);
 for(float y:{1.6f,3.6f})shelf(m,{22.65f,y+.6f},-kPi*.5f);
 terminal(m,{6.5f,1.4f},"RAIL DOCK / MANIFEST","THE LAST TRAIN ARRIVED EMPTY.","THE PRESSURE GAUGES KEPT RISING.");
 serviceCeiling(m,1,1,23,7,3.6f);

 // --- 05 PUMP HALL -------------------------------------------------------
 for(auto pier:{std::pair{6,10},{6,14},{18,10},{18,14}})g().rows[pier.second][pier.first]='#';
 // The vessel plinth: four pressure tanks fed from an overhead header.
 equipmentPad(m,8.6f,9.6f,15.4f,15.4f,0);
 for(float x:{10.4f,13.6f})for(float y:{11.f,14.f}){float h=3.f,scale=h/2.390135f;
  m.fixtures.push_back({14,{x,y},.1f,1.58f*scale,1.62f*scale,h,0,true});
  m.pipes.push_back({{x,y},{x,y},3.1f,.14f,hall,3});}
 servicePipe(m,{1.f,12.5f},{23.f,12.5f},4.6f,.22f,3);
 for(float x:{10.4f,13.6f})for(float y:{11.f,14.f})m.pipes.push_back({{x,y},{x,12.5f},4.6f,.12f,4.6f,3});
 servicePipe(m,{1,9.f},{23,9.f},4.8f,.16f,4);servicePipe(m,{1,16.f},{23,16.f},4.8f,.16f,4);
 // Riser bundles with valve wheels along both walls.
 for(float y:{9.2f,12.6f,15.4f}){m.fixtures.push_back({44,{1.06f,y},0,.4f,.1f,4.7f,kPi*.5f,false});m.fixtures.push_back({45,{1.13f,y},1.25f,.32f,.12f,.32f,kPi*.5f,false});}
 for(float y:{9.6f,13.6f}){m.fixtures.push_back({44,{22.94f,y},0,.4f,.1f,4.7f,-kPi*.5f,false});m.fixtures.push_back({45,{22.87f,y},1.25f,.32f,.12f,.32f,-kPi*.5f,false});}
 // Cover: drums and a crate line on the west flank, a breaker cage east.
 for(auto c:{std::pair{3,12},{4,12}})g().rows[c.second][c.first]='C';
 for(auto b:{std::pair{8,9},{20,12},{16,16}})g().rows[b.second][b.first]='B';
 m.fixtures.push_back({41,{21.f,15.6f},0,2.4f,1.f,1.1f,0,true});
 // A blown gland on the north-west vessel vents across the plinth.
 hazard(m,Hazard::Kind::Steam,9.f,10.2f,9.5f,11.8f,-1,2.f,14);
 for(float y:{8.5f,12.5f,16.2f})wall(m,1,y,23,y+.18f,hall-.4f,hall-.22f,2);

 // --- 06 CONTROL ---------------------------------------------------------
 rectangle(g(),1,17,2,18,'#');rectangle(g(),5,17,12,18,'#');rectangle(g(),15,17,23,18,'#');
 m.doors.push_back({2,5,17.5f});m.doors.push_back({12,15,17.5f});
 // Operator floor: a bank of desks facing the glazed supervisor's booth.
 for(float x:{4.f,7.5f,11.f})recordsDesk(m,x,20.6f,0);
 for(float x:{2.f,3.f,13.f})m.fixtures.push_back({13,{x,22.74f},0,.9f,.5f,1.9f,0,true});
 // Booth: steel sill and glass along x 16, open to the south.
 wall(m,16.f,18.f,16.15f,20.4f,0,1.f,3);wall(m,16.f,18.f,16.15f,20.4f,1.f,2.6f,17);wall(m,16.f,18.f,16.15f,20.4f,2.6f,3.f,3);
 recordsDesk(m,18.f,19.f,0);
 terminal(m,{18.f,19.05f},"CONTROL / NIGHT SHIFT","SURFACE LIFT IS ON EMERGENCY POWER.","THE PUMPS MUST STAY RUNNING.");
 transferExit(m);
 serviceCeiling(m,1,18,23,23,3.f);

 // --- Light ---------------------------------------------------------------
 m.lights.push_back(lamp({9.f,3.5f},3.45f,1.f,6.5f,4300,.3f));
 m.lights.push_back(lamp({16.f,3.5f},3.45f,1.f,6.5f,4300,.3f));
 m.lights.push_back(lamp({3.5f,3.f},3.45f,.8f,5.f,3400));
 m.lights.push_back(lamp({12.f,13.4f},hall-.6f,2.f,9.f,5600,.35f));
 m.lights.push_back(lamp({4.f,10.5f},hall-.3f,1.2f,7.f,2000));
 m.lights.push_back(lamp({20.f,14.f},hall-.3f,1.2f,7.f,2000));
 m.lights.push_back(lamp({10.5f,17.f},2.4f,1.3f,3.5f,1300,-.3f,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({7.5f,19.5f},2.85f,1.f,6.f,3200));
 m.lights.push_back(lamp({19.5f,19.5f},2.85f,1.1f,5.f,3800,.1f));
 m.lights.push_back(lamp({23.f,21.f},2.1f,1.4f,4.5f,1250,-.35f,WorldLightMount::Wall,kPi));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{17.5f,4.5f}},{C::Wasp,{11.f,4.6f}},
                   {C::Brute,{4.5f,14.5f}},{C::Huntsman,{8.f,12.5f}},{C::Wasp,{19.5f,12.5f}},{C::Brute,{21.f,10.5f}},
                   {C::Huntsman,{9.5f,19.2f}},{C::Wasp,{13.5f,20.5f}},{C::Brute,{21.5f,19.5f}}};
 m.pickupSpawns={{{4.5f,4.5f},P::Ammo},{{16.5f,2.5f},P::Health},{{2.5f,15.5f},P::Ammo},{{22.f,11.5f},P::Ammo},{{7.5f,22.f},P::Health},{{17.2f,21.5f},P::Ammo}};
 const Vec2 junk[]={{6.8f,3.2f},{7.1f,3.4f},{9.2f,12.2f},{9.5f,8.6f},{18.7f,19.8f},{19.1f,20.f}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,junk[i],-999,i*.7f});

 zone(m,"04 RAIL DOCK",0,0,24,7.5f);
 zone(m,"05 PUMP HALL",0,7.5f,24,17.5f);
 zone(m,"06 CONTROL",0,17.5f,24,24);
 return p;
}

// TURBINE GANTRY. A seven-metre turbine hall: the main set and its generator
// on the floor with two auxiliary units, ringed at three metres by a steel
// gantry. The gantry loops from the west stair along the north wall to the
// glazed control room, and back across the hall on a bridge. The control room
// releases the turbine brakes, which unseals the transfer interlock in the
// switchgear bay to the south; the east stair drops back down to it.
std::shared_ptr<const Map> makeTurbineGantry(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[2];m.definition=CampaignChunks[2];m.definition.ceiling=11.f;m.definition.ambient=.36f;
 m.layers.push_back(floorLayer("Turbine Gantry / ground",0));
 auto gantryLayer=[&](const char* name,float z){AuthoredLayerData l;l.name=name;l.elevation=z;l.thickness=.3f;for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));};
 gantryLayer("Turbine Gantry / catwalks",3);gantryLayer("Turbine Gantry / crane gallery",6);
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 auto& walk=m.layers[1];auto& high=m.layers[2];
 const float hall=11.f,deckZ=3.f,craneZ=6.f;

 // --- 07 TURBINE HALL ----------------------------------------------------
 rectangle(g(),2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true});wall(m,2,0,5,1,2.5f,hall,3);
 // The turbine-generator set on its pedestal: high- and low-pressure casings
 // on one shaft into the generator, the steam chest and main steam line on
 // the HP casing, and the exhaust trunk down into the condenser behind it.
 wall(m,7.4f,9.5f,17.6f,12.5f,0,1.f,15);floorLine(m,7.4f,9.5f,17.6f,9.62f,1.f,13);floorLine(m,7.4f,12.38f,17.6f,12.5f,1.f,13);
 const float shaft=1.95f;
 for(float x:{7.9f,11.9f,14.9f})wall(m,x-.25f,10.4f,x+.25f,11.6f,1.f,shaft-.3f,12);
 m.pipes.push_back({{7.9f,11.f},{16.4f,11.f},shaft,.16f,shaft,6});
 m.pipes.push_back({{8.3f,11.f},{11.5f,11.f},shaft,.85f,shaft,5});
 for(float x:{8.3f,9.4f,10.5f,11.5f})m.pipes.push_back({{x-.07f,11.f},{x+.07f,11.f},shaft,.92f,shaft,6});
 m.pipes.push_back({{12.f,11.f},{14.9f,11.f},shaft,1.05f,shaft,5});
 for(float x:{12.f,13.f,14.f,14.9f})m.pipes.push_back({{x-.07f,11.f},{x+.07f,11.f},shaft,1.12f,shaft,6});
 m.fixtures.push_back({6,{16.5f,11.f},1,2.04f,.6f,1.08f,0,true});
 m.pipes.push_back({{9.9f,11.f},{9.9f,11.f},shaft+.6f,.5f,shaft+1.5f,6});
 pipeRoute(m,{{9.9f,11.f,shaft+1.5f},{9.9f,11.f,hall}},.3f,4,NoRoof);
 m.fixtures.push_back({45,{10.6f,10.35f},shaft+1.1f,.32f,.1f,.32f,0,false});
 m.pipes.push_back({{13.5f,11.7f},{13.5f,14.f},1.3f,.8f,1.3f,6});m.structures.push_back({12.7f,12.4f,14.3f,13.8f,0,2.1f,false,6});
 // Condenser: a horizontal shell on saddles south of the set, cooling water
 // in and out through the west wall.
 const float condenserAxis=1.35f;
 m.pipes.push_back({{8.6f,15.f},{15.4f,15.f},condenserAxis,1.15f,condenserAxis,1});
 for(float x=9.4f;x<15.4f;x+=1.5f)m.pipes.push_back({{x-.08f,15.f},{x+.08f,15.f},condenserAxis,1.2f,condenserAxis,6});
 for(float x:{8.6f,15.4f})m.pipes.push_back({{x-.1f,15.f},{x+.1f,15.f},condenserAxis,1.26f,condenserAxis,6});
 for(float x:{9.6f,12.f,14.4f}){wall(m,x-.3f,14.f,x+.3f,16.f,0,condenserAxis-.95f,15);wall(m,x-.34f,13.95f,x+.34f,16.05f,condenserAxis-.95f,condenserAxis-.8f,12);}
 m.structures.push_back({7.4f,13.7f,16.6f,16.3f,0,2.6f,false,6});
 // Cooling water in and out: overhead across the aisle, down into the heads.
 pipeRoute(m,{{5.2f,17.f,2.3f},{5.2f,14.4f,2.3f},{8.2f,14.4f,2.3f},{8.2f,14.4f,condenserAxis}},.22f,5,NoRoof);
 pipeRoute(m,{{6.2f,17.f,2.3f},{6.2f,15.6f,2.3f},{8.2f,15.6f,2.3f},{8.2f,15.6f,condenserAxis}},.22f,5,NoRoof);
 // Boiler feed pumps on the west wall, discharging up to the feed header.
 // The header runs north, clear of the west stair, and climbs by the entry.
 P3 feedA=pumpSkid(m,{3.5f,9.5f},0,0,2.45f),feedB=pumpSkid(m,{3.5f,12.5f},0,0,2.45f);
 pipeRoute(m,{feedB,{feedA.x,feedA.y,2.45f},{1.4f,feedA.y,2.45f},{1.4f,1.05f,2.45f}},.16f,3,NoRoof);
 for(float y:{7.5f,5.f})gateValve(m,{1.4f,y,2.45f},{0,1},.16f,0,1);
 // A cracked gland on the condenser's west head vents steam across the lane.
 hazard(m,Hazard::Kind::Steam,7.3f,14.1f,8.2f,15.9f,-1,2.2f,12);
 // Cover on the hall floor: drums by the entry lane and crate stacks east.
 for(auto b:{std::pair{6,4},{14,4},{15,4}})g().rows[b.second][b.first]='B';
 for(auto c:{std::pair{18,15},{19,15},{18,16}})g().rows[c.second][c.first]='C';
 m.fixtures.push_back({41,{20.f,2.4f},0,2.4f,1.f,1.1f,0,true});
 cableTray(m,{17.5f,1.2f},{22.5f,1.2f},2.6f);
 // Roof trusses, the overhead travelling crane on its runway beams, and the
 // wall bays that give the hall its height.
 for(float y:{4.5f,9.f,13.5f})wall(m,1,y-.12f,23,y+.12f,hall-.55f,hall-.05f,12);
 for(float x:{1.f,22.6f}){wall(m,x,1,x+.4f,17,hall-1.6f,hall-1.35f,12);for(float y=3.f;y<17;y+=4.5f)wall(m,x,y-.2f,x+.4f,y+.2f,hall-2.1f,hall-1.6f,15);}
 for(float y:{10.3f,11.7f})wall(m,1.4f,y-.15f,22.6f,y+.15f,hall-1.35f,hall-1.f,18);
 wall(m,11.3f,10.f,12.7f,12.f,hall-1.7f,hall-1.35f,12);
 // The hook block hangs clear of the high bridge beneath it.
 for(float y:{10.9f,11.1f})m.pipes.push_back({{12.f,y},{12.f,y},hall-1.7f,.02f,craneZ+2.6f,6});
 wall(m,11.8f,10.8f,12.2f,11.2f,craneZ+2.3f,craneZ+2.6f,8);m.pipes.push_back({{12.f,11.f},{12.f,11.f},craneZ+2.05f,.04f,craneZ+2.3f,6});
 wallBays(m,0,hall-.6f,4.5f,true,false,true,true,3.5f);

 // --- 08 UPPER GANTRY ----------------------------------------------------
 // Catwalks at +3: the west run from the entry stair, a bridge across the
 // hall into the control room, a ring round the turbine set with spurs to
 // both, the north run over the entry lane, and an east walk down to the
 // switchgear stair. Rails are 55 cm: every walk can be dropped off.
 rectangle(walk,1,1,3,11,'=');                 // west run
 rectangle(walk,3,1,23,2,'=');rectangle(walk,6,2,23,3,'=');   // north run, joining the west run
 rectangle(walk,22,2,23,3,'=');                // down to the control room
 rectangle(walk,1,6,17,8,'=');                 // bridge
 rectangle(walk,17,3,23,9,'=');                // control room floor
 rectangle(walk,6,9,18,10,'=');rectangle(walk,6,15,18,16,'=');
 rectangle(walk,6,10,7,15,'=');rectangle(walk,17,10,18,15,'=');   // ring
 rectangle(walk,3,9,6,10,'=');                 // spur to the west run
 rectangle(walk,11,8,12,9,'=');                // spur to the bridge
 m.stairs.push_back({1,11,3,16,0,deckZ,15,true,false});
 m.stairs.push_back({20,9,22,14,0,deckZ,15,true,false});
 m.stairs.push_back({3,2,5,6,deckZ,craneZ,15,true,false});
 deckSupports(m,walk,0,6.f);
 // Crane gallery at +6: the north gallery and a high bridge over the set to
 // the crane maintenance platform.
 rectangle(high,1,1,17,2,'=');rectangle(high,1,2,2,6,'=');
 rectangle(high,11,2,13,15,'=');rectangle(high,9,15,15,17,'=');
 deckHangers(m,high,hall);
 // Control room: steel sill, glazing and frame, roofed; doorways from the
 // north run (x 20-21.5) and the bridge (y 6-8).
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,deckZ,deckZ+.9f,3);wall(m,x1,y1,x2,y2,deckZ+.9f,deckZ+2.4f,17);wall(m,x1,y1,x2,y2,deckZ+2.4f,deckZ+2.7f,3);};
 glazed(17,3,17.12f,6);glazed(17,8,17.12f,9);
 glazed(17.12f,3,20,3.12f);glazed(21.5f,3,23,3.12f);
 wall(m,20,3,21.5f,3.12f,deckZ+2.2f,deckZ+2.7f,3);
 wall(m,17,6,17.12f,8,deckZ+2.2f,deckZ+2.7f,3);
 wall(m,17,3,23,9,deckZ+2.7f,deckZ+2.85f,2);
 // The room is carried: an edge beam under its west side on two columns.
 wall(m,17,3,17.2f,9,deckZ-.6f,deckZ-.3f,12);for(float y:{3.4f,8.6f})steelColumn(m,17.4f,y,0,deckZ-.3f,false);
 {AuthoredTerminalData t;t.position={19.5f,4.f};t.title="GANTRY CONTROL / 05:42";t.line1="TURBINE BRAKES RELEASED.";t.line2="LOWER TRANSFER INTERLOCK UNSEALED.";
  t.z=deckZ;t.control=true;t.yaw=kPi;m.terminals.push_back(std::move(t));}
 recordsDesk(m,21.6f,5.2f,deckZ);
 shelf(m,{22.65f,7.6f},-kPi*.5f,deckZ);
 for(float x:{18.f,19.f})m.fixtures.push_back({13,{x,8.73f},3,.9066f,.4956f,2.2f,0,true});

 // --- 09 SWITCHGEAR ------------------------------------------------------
 rectangle(g(),1,17,8,18,'#');rectangle(g(),11,17,20,18,'#');
 m.doors.push_back({8,11,17.5f});m.doors.push_back({20,23,17.5f});
 for(float x:{2.f,3.f,4.f,5.f,6.f})m.fixtures.push_back({13,{x,18.27f},0,.9f,.5f,1.9f,kPi,true});
 m.fixtures.push_back({41,{14.f,20.6f},0,2.4f,1.f,1.1f,0,true});
 for(float x:{12.f,16.f})m.fixtures.push_back({8,{x,22.9f},.9f,.67f,.2f,.91f,0,false});
 // A burst breaker arcs across the floor at the west end of the bank.
 hazard(m,Hazard::Kind::Electricity,1.f,20.6f,2.6f,22.f,-1,2.f,18);
 transferExit(m);m.doors.back().requireControl=true;
 serviceCeiling(m,1,18,23,23,3.2f);
 wallSign(m,14.5f,18.06f,2.2f,"DANGER","LIVE SWITCHGEAR",2.f,0);

 // --- Light ---------------------------------------------------------------
 // Hall: metal-halide high bays; amber lamps along the gantry walls.
 for(Vec2 b:{Vec2{5.f,10.f},Vec2{12.5f,5.f},Vec2{19.f,12.5f},Vec2{9.f,13.f},Vec2{15.f,13.f},Vec2{20.f,15.5f},Vec2{5.f,4.f},Vec2{18.f,5.f}})m.lights.push_back(lamp(b,hall-.4f,2.6f,13.f,4200,.15f));
 for(float x:{4.f,10.f,16.f})m.lights.push_back(lamp({x,1.f},deckZ+1.9f,1.1f,6.f,2600,.1f,WorldLightMount::Wall,kPi*.5f));
 for(float y:{5.5f,9.5f,14.f})m.lights.push_back(lamp({1.f,y},deckZ+1.9f,1.1f,6.f,2600,.1f,WorldLightMount::Wall,0));
 for(float y:{5.f,10.f,15.f})m.lights.push_back(lamp({23.f,y},deckZ+1.9f,1.1f,6.f,2600,.1f,WorldLightMount::Wall,kPi));
 for(float x:{4.f,12.f,18.f})m.lights.push_back(lamp({x,17.f},2.4f,1.2f,6.f,4200,.15f,WorldLightMount::Wall,-kPi*.5f));
 // Inspection lamps slung under the bridge and the ring pick out the set.
 for(Vec2 b:{Vec2{9.f,7.f},Vec2{14.f,7.f},Vec2{6.5f,12.5f},Vec2{17.5f,12.5f}})m.lights.push_back(lamp(b,deckZ-.55f,1.3f,6.f,3300));
 for(float x:{4.f,8.f,14.f,20.f})m.lights.push_back(lamp({x,1.5f},deckZ-.55f,.8f,4.5f,3600,.2f));
 for(float y:{4.f,9.f,13.f})m.lights.push_back(lamp({12.f,y},craneZ-.42f,.9f,5.f,3600,.15f));
 m.lights.push_back(lamp({1.5f,4.f},craneZ-.42f,.8f,4.5f,3600,.15f));
 // Control room: warm desk light and a cold console glow.
 m.lights.push_back(lamp({19.5f,6.f},deckZ+2.55f,1.f,5.f,2900));
 // Switchgear: cold tubes, a blue-white arc light and red at the gate.
 m.lights.push_back(lamp({6.f,20.5f},3.05f,1.f,6.f,5200,.25f));
 m.lights.push_back(lamp({14.f,20.5f},3.05f,.9f,6.f,5200,.25f));
 m.lights.push_back(lamp({1.f,21.3f},1.5f,1.2f,3.5f,9000,-.1f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({23.f,21.f},2.1f,1.4f,4.5f,1250,-.35f,WorldLightMount::Wall,kPi));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{8.f,4.5f}},{C::Wasp,{15.f,7.5f}},{C::Brute,{5.f,16.f}},
                   {C::Huntsman,{10.f,7.f},deckZ},{C::Wasp,{19.5f,13.f}},{C::Brute,{10.5f,21.f}}};
 m.pickupSpawns={{{3.5f,3.5f},P::Ammo},{{2.f,8.5f},P::Health,deckZ},{{15.5f,7.f},P::Ammo,deckZ},{{21.5f,19.f},P::Ammo},{{12.f,16.f},P::Ammo,craneZ}};
 const Vec2 junk[]={{7.4f,3.6f},{7.6f,4.6f},{12.5f,19.2f},{13.f,19.6f},{18.4f,5.f},{18.9f,5.4f}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,junk[i],i>=4?deckZ:-999.f,i*.7f});

 zone(m,"08A CRANE GALLERY",0,0,24,24,5.5f,1000);
 zone(m,"08 UPPER GANTRY",0,0,24,24,2.5f,1000);
 zone(m,"07 TURBINE HALL",0,0,24,17.5f);
 zone(m,"09 SWITCHGEAR",0,17.5f,24,24);
 return p;
}

// REACTOR COMPLEX. The surface lift's collar: an entry corridor into the lift
// lobby, with the hoist plant behind it and a casualty by the landing. The cab
// climbs three storeys of shaft, jams and falls into the containment pit.
// Below, a two-storey atrium stands round the reactor vessel, with the P-01
// feed pumps west and the R-03 control room east (the bulkhead out, and the
// Warden). A west stair climbs to the upper manifold: the maintenance bench
// with the authorization disk overlooks the vessel, and the P-02 return station
// stands on the east balcony.
std::shared_ptr<const Map> makeReactorComplex(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[3];m.definition=CampaignChunks[3];m.definition.ceiling=16.f;m.definition.ambient=.28f;
 const float lower=-9,upper=-6;
 m.layers.push_back(floorLayer("Reactor / lower containment",lower));
 auto layer=[&](const char* name,float z,float thickness){AuthoredLayerData l;l.name=name;l.elevation=z;l.thickness=thickness;
  for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));};
 layer("Reactor / upper manifold",upper,.3f);layer("Scenery / coolant risers",-3,.25f);layer("Surface lift / collar",0,.3f);
 layer("Scenery / ventilation plant",3,.25f);layer("Scenery / electrical service",6,.25f);layer("Surface / sealed gates",9,.35f);
 auto L=[&](size_t i)->AuthoredLayerData&{return m.layers[i];};
 // The shaft (x 9-15, y 9-15) passes every storey; at each passing storey a
 // ring of floor encloses it as a bay seen from the moving cab.
 auto ring=[](AuthoredLayerData& l){for(int x=8;x<16;++x){l.rows[8][x]='=';l.rows[15][x]='=';}for(int y=9;y<15;++y){l.rows[y][8]='=';l.rows[y][15]='=';}};
 for(size_t i:{2u,4u,5u,6u})ring(L(i));
 for(float z:{-3.f,3.f,6.f,9.f}){
  wall(m,7.8f,7.8f,8,16.2f,z,z+2.75f,3);wall(m,16,7.8f,16.2f,16.2f,z,z+2.75f,3);
  wall(m,8,7.8f,16,8,z,z+2.75f,3);wall(m,8,16,16,16.2f,z,z+2.75f,3);
  wall(m,7.8f,7.8f,9,16.2f,z+2.65f,z+2.7f,2);wall(m,15,7.8f,16.2f,16.2f,z+2.65f,z+2.7f,2);
  wall(m,9,7.8f,15,9,z+2.65f,z+2.7f,2);wall(m,9,15,15,16.2f,z+2.65f,z+2.7f,2);
 }
 // Roof slabs of the manifold storey (-3.3) and of the collar (2.7).
 for(float z:{-3.3f,2.7f}){wall(m,1,1,23,9,z,z+.12f,2);wall(m,1,15,23,23,z,z+.12f,2);wall(m,1,9,9,15,z,z+.12f,2);wall(m,15,9,23,15,z,z+.12f,2);}

 // --- 10 SURFACE LIFT (collar, 0 m) ----------------------------------------
 {auto& c=L(3);
  rectangle(c,2,0,5,4,'=');rectangle(c,1,4,23,9,'=');rectangle(c,14,1,23,4,'=');ring(c);
  c.rows[8][11]=c.rows[8][12]='S';}
 rectangle(L(0),2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true,9});
 wall(m,2,0,5,1,lower,-.3f,3);
 // The collar's ring round the shaft is the shaft wall, not a corridor.
 for(float x:{8.f,15.f})wall(m,x,9,x+1,9.12f,0,2.7f,3);
 // Landing plate into the cab, caged on both sides.
 wall(m,11,8,13,10,-.3f,0,7);
 m.structures.push_back({10.95f,8,11,10,0,2.1f,true});m.structures.push_back({13,8,13.05f,10,0,2.1f,true});
 terminal(m,{13.9f,8.55f},"LIFT SYSTEM / OVERRIDE","SURFACE GATES CLAMPED SHUT.","LAST CAB TO THE TOP. BOARD AND DISPATCH.",9);
 {AuthoredTerminalData t;t.position={13.35f,11.5f};t.title="CAB CONTROL / DISPATCH";t.line1="ASCENDING TO SURFACE.";t.line2="WARNING: SAME BRAKE MODEL AS SERVICE LIFT.";
  t.z=9;t.control=true;m.terminals.push_back(std::move(t));}
 // Lobby: lockers on the north wall, freight waiting for a cab that never came.
 for(float x:{6.f,7.f,8.f})m.fixtures.push_back({13,{x,4.3f},9,.9066f,.4956f,2.2f,kPi,true});
 cargo(m,3.f,7.6f,0);cargo(m,19.2f,7.6f,0);cargo(m,20.6f,7.6f,0,false);
 for(auto b:{std::pair{22,5},{1,5}})m.fixtures.push_back({15,{b.first+.5f,b.second+.5f},9,1.2f,1.15f,.16f,0,true});
 // Hoist plant: the traction motor and its cable drum, with the drive panels.
 m.fixtures.push_back({6,{18.f,2.2f},9,2.38f,.7f,1.26f,0,true});
 m.pipes.push_back({{19.6f,2.2f},{21.6f,2.2f},1.f,.45f,-999,2});m.structures.push_back({19.5f,1.75f,21.7f,2.65f,0,1.45f,false,6});
 for(float x:{19.6f,21.5f})wall(m,x,1.9f,x+.12f,2.5f,0,1.f,12);
 for(float x:{15.f,16.f})m.fixtures.push_back({13,{x,1.3f},9,.9066f,.4956f,2.2f,kPi,true});
 wallSign(m,22.945f,2.5f,1.9f,"HOIST PLANT","NO ENTRY WHILE CAB MOVES",1.6f,-kPi*.5f);

 // --- 11 CONTAINMENT (-9 m) --------------------------------------------------
 // The cab falls into the pit; the containment hall lies south of it, the
 // P-01 feed pumps west, the store north, and a two-storey passage east into
 // the reactor core (a floor corridor under a manifold bridge).
 {auto& u=L(1);rectangle(u,1,1,23,23,'=');rectangle(u,9,9,15,15,'.');rectangle(u,1,9,3,15,'.');rectangle(u,23,10,24,14,'=');}
 rectangle(L(0),23,10,24,14,'.');m.openEast=true;
 // Above the manifold roof the seam is closed: the core is a separate hall.
 wall(m,23,9,24,15,-3.3f,-3.18f,2);wall(m,23.85f,10,24,14,-3.18f,m.definition.ceiling,3);
 m.stairs.push_back({1,9,3,15,lower,upper,15,true,false});
 const float roomTop=upper-.3f;
 // P-01 feed pump room (west), opening on the containment hall.
 wall(m,1,16.85f,9,17,lower,roomTop,3);
 wall(m,8.85f,17,9,18.6f,lower,roomTop,3);wall(m,8.85f,20.2f,9,23,lower,roomTop,3);wall(m,8.85f,18.6f,9,20.2f,roomTop-.6f,roomTop,3);
 // Terminal order: the lift's two, then the log, the P-01 feed and the hall log.
 terminal(m,{7.8f,21.9f},"MAINTENANCE / SHIFT LOG","RETURN TRIPPED AGAIN. NO FEED PRESSURE.","LEFT THE SERVICE DISK WITH THE SPARES.",3);
 {AuthoredTerminalData t;t.position={3.2f,21.9f};t.title="01 / COOLANT FEED";t.reactorAction=2;m.terminals.push_back(std::move(t));}
 terminal(m,{16.5f,15.45f},"CONTAINMENT / INCIDENT LOG","CORE TEMPERATURE RISING. R-03 IS IN THE CORE HALL.","AUTH DISK UPSTAIRS. PRIME FEED, THEN THE RETURN LOOP.");
 m.terminals.back().yaw=.001f;
 // Two feed pumps on plinths draw from the suction trench along the north
 // wall; their discharge risers join one header that leaves through the
 // doorway lintel, crosses the hall overhead and enters the core wall east.
 for(float x:{1.2f,4.6f})wall(m,x,17.05f,x+3.6f,17.65f,lower,lower+.012f,8);
 P3 a=pumpSkid(m,{3.f,18.9f},lower,kPi*.5f,-6.75f),b=pumpSkid(m,{6.2f,18.9f},lower,kPi*.5f,-6.75f);
 pipeRoute(m,{a,{b.x,b.y,-6.75f},{15.f,b.y,-6.75f},{15.f,16.4f,-6.75f},{22.95f,16.4f,-6.75f}},.16f,3,roomTop);
 for(float x:{4.4f,7.4f})gateValve(m,{x,a.y,-6.75f},{1,0},.16f,lower,1);
 m.fixtures.push_back({13,{1.27f,21.2f},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
 cableTray(m,{1.6f,22.8f},{8.4f,22.8f},-6.75f);
 // Containment hall: decontamination booths, hazmat lockers, spent-fuel casks.
 for(float x:{17.5f,20.f}){wall(m,x,20.4f,x+.08f,23,lower,lower+2.2f,17);wall(m,x+2.f,20.4f,x+2.08f,23,lower,lower+2.2f,17);
  m.pipes.push_back({{x+1.04f,22.6f},{x+1.04f,22.6f},lower+2.1f,.05f,roomTop,1});m.pipes.push_back({{x+1.04f,22.6f},{x+1.04f,22.6f},lower+2.06f,.12f,lower+2.1f,6});}
 for(float x:{10.f,11.f,12.f,13.f,14.f})m.fixtures.push_back({13,{x,22.74f},0,.9066f,.4956f,2.2f,0,true});
 for(float x:{11.f,13.6f}){float h=1.7f,scale=h/2.390135f;m.fixtures.push_back({14,{x,19.4f},0,1.58f*scale,1.62f*scale,h,0,true});}
 // East corridor services, and the store north: drums, crates, a coolant spill.
 pipeRoute(m,{{15.6f,9.4f,lower},{15.6f,9.4f,-6.65f},{22.95f,9.4f,-6.65f}},.12f,4,roomTop);
 m.structures.push_back({15.4f,9.2f,15.8f,9.6f,lower,-6.6f,false,6});
 for(auto c:{std::pair{4,2},{5,2},{4,3},{18,6},{19,6}})L(0).rows[c.second][c.first]='C';
 for(auto b:{std::pair{8,2},{14,3},{21,7}})L(0).rows[b.second][b.first]='B';
 hazard(m,Hazard::Kind::Toxic,20.2f,1.2f,22.8f,4.f,lower-1,lower+1.2f,6);
 floorLine(m,20.f,4.f,23.f,4.15f,lower,13);floorLine(m,20.f,1.f,20.15f,4.15f,lower,13);
 wallSign(m,22.945f,5.2f,lower+1.8f,"DANGER","COOLANT LEAK / DO NOT ENTER",1.6f,-kPi*.5f);

 // --- 11A UPPER MANIFOLD (-6 m) ---------------------------------------------
 wall(m,1,15.85f,5,16,upper,-3.3f,3);wall(m,6.5f,15.85f,9,16,upper,-3.3f,3);wall(m,5,15.85f,6.5f,16,-3.9f,-3.3f,3);
 // Maintenance bench: the authorization disk lies on it.
 m.fixtures.push_back({19,{6.f,21.1f},3,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({18,{6.f,21.84f},3,.417f,.525f,.95f,0,true});
 m.fixtures.push_back({7,{3.f,22.5f},3,1.8f,.5f,1.8f,0,true});
 m.fixtures.push_back({41,{1.62f,18.6f},3,2.4f,1.f,1.1f,kPi*.5f,true});
 m.fixtures.push_back({13,{8.6f,22.75f},3,.9066f,.4956f,2.2f,0,true});
 cargo(m,8.f,18.3f,upper,false);
 // North pipe gallery: feed, return and service lines rise through the deck
 // from the containment below, run the length of the storey on clamps and
 // drop back through the deck at the east end.
 {const float y[]={1.9f,2.65f,3.35f},r[]={.2f,.2f,.1f};const int paint[]={3,4,5};
  for(int i=0;i<3;++i){
   pipeRoute(m,{{3.5f+i*.45f,y[i],upper},{3.5f+i*.45f,y[i],-3.75f},{22.f-i*.45f,y[i],-3.75f},{22.f-i*.45f,y[i],upper}},r[i],paint[i],-3.3f);
   for(float x:{3.5f+i*.45f,22.f-i*.45f}){flange(m,{x,y[i],upper+.02f},{},r[i]);m.structures.push_back({x-r[i]-.05f,y[i]-r[i]-.05f,x+r[i]+.05f,y[i]+r[i]+.05f,upper,-3.75f,false,6});}
  }
  for(float x:{7.f,13.f,19.f})gateValve(m,{x,1.9f,-3.75f},{1,0},.2f,lower,1);
 }
 for(float x:{6.f,12.f,18.f})steelColumn(m,x,4.85f,upper,-3.3f);
 // 11A's namesake: two distribution manifolds in the south-east bay, each a
 // header drum on saddle stands with valved branches up to the roof and down
 // through the deck to the pumps and loops below.
 for(float y:{18.2f,21.4f}){
  m.pipes.push_back({{16.2f,y},{21.8f,y},-5.15f,.42f,-5.15f,y<20?3:4});
  for(float x:{16.2f,21.8f})m.pipes.push_back({{x-.06f,y},{x+.06f,y},-5.15f,.5f,-5.15f,6});
  for(float x:{17.f,21.f}){wall(m,x-.12f,y-.35f,x+.12f,y+.35f,upper,-5.5f,12);wall(m,x-.2f,y-.45f,x+.2f,y+.45f,upper,upper+.03f,8);}
  for(float x:{17.4f,18.8f,20.2f}){
   m.pipes.push_back({{x,y},{x,y},-4.75f,.12f,-3.3f,y<20?3:4});
   flange(m,{x,y,-3.32f},{},.12f);
   m.pipes.push_back({{x,y},{x,y},-4.4f,.18f,-4.15f,6});
   m.fixtures.push_back({45,{x,y+.3f},4.6f,.32f,.1f,.32f,0,false});
   m.pipes.push_back({{x,y},{x,y},upper,.12f,-5.55f,y<20?3:4});
   flange(m,{x,y,upper+.02f},{},.12f);
  }
  m.structures.push_back({16.f,y-.55f,22.f,y+.55f,upper,-4.6f,false,6});
 }
 wallSign(m,22.945f,19.8f,upper+1.35f,"MANIFOLD","FEED (YELLOW) / RETURN (RED)",1.8f,-kPi*.5f);
 // Instrument racks south of the shaft, back to back, with their console.
 for(float x:{10.4f,11.4f,12.4f,13.4f}){m.fixtures.push_back({13,{x,19.75f},3,.9066f,.4956f,2.2f,kPi,true});m.fixtures.push_back({13,{x,20.25f},3,.9066f,.4956f,2.2f,0,true});}
 m.fixtures.push_back({22,{12.f,17.6f},3,.78f,.49f,1.3f,kPi,true});
 cableTray(m,{9.6f,20.f},{14.4f,20.f},-3.65f);
 m.fixtures.push_back({14,{21.4f,5.6f},3,1.58f,1.62f,2.3f,0,true});

 // --- Light ---------------------------------------------------------------
 // Collar: lobby tubes, the plant, an amber beacon over the landing.
 for(float x:{4.f,9.f,17.f})m.lights.push_back(lamp({x,6.5f},2.55f,1.f,6.f,4300,.25f));
 m.lights.push_back(lamp({3.5f,2.f},2.55f,.8f,4.f,4300,.25f));
 m.lights.push_back(lamp({18.5f,2.5f},2.55f,.9f,5.f,3000));
 m.lights.push_back(lamp({12.f,7.f},2.55f,1.1f,4.f,1800,-.1f));
 // Containment: sodium in the pump room, cold decon light, red emergency.
 m.lights.push_back(lamp({5.f,20.f},roomTop-.15f,1.f,6.f,2000));
 m.lights.push_back(lamp({5.f,5.f},roomTop-.15f,.7f,6.f,2800));
 m.lights.push_back(lamp({18.f,5.f},roomTop-.15f,.6f,6.f,1300,-.3f));
 m.lights.push_back(lamp({6.f,12.f},roomTop-.15f,.6f,5.f,1300,-.3f));
 m.lights.push_back(lamp({19.5f,12.f},roomTop-.15f,.9f,6.f,4300,.2f));
 m.lights.push_back(lamp({12.f,18.f},roomTop-.15f,1.f,7.f,5200,.15f));
 m.lights.push_back(lamp({20.f,21.5f},roomTop-.15f,.9f,4.f,6500,.1f));
 m.lights.push_back(lamp({12.f,15.6f},roomTop-.15f,.8f,4.f,1300,-.3f));
 // Manifold: the bench, the gallery and the east bridge.
 m.lights.push_back(lamp({5.f,19.f},-3.45f,1.1f,6.f,3500));
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,4.5f},-3.45f,.9f,7.f,2600,.1f));
 m.lights.push_back(lamp({19.5f,12.f},-3.45f,.9f,6.f,3500));
 m.lights.push_back(lamp({14.f,19.5f},-3.45f,.7f,7.f,3000));
 m.lights.push_back(lamp({19.f,19.8f},-3.45f,.9f,6.f,4300,.15f));
 m.lights.push_back(lamp({12.f,7.f},-3.45f,.8f,7.f,3000));
 // Each storey the cab passes is lit in its own colour, so the ride reads as
 // movement: blue coolant risers, the collar, amber plant, red electrical,
 // white at the sealed surface gates.
 const std::pair<float,float> storeys[]={{-3.f,9000},{0.f,4300},{3.f,2200},{6.f,1300},{9.f,5200}};
 for(auto [z,kelvin]:storeys)for(Vec2 bay:{Vec2{8.5f,9.5f},Vec2{15.5f,14.5f}})m.lights.push_back(lamp(bay,z+(z==0?2.55f:2.5f),.9f,5.f,kelvin));
 // The cab lamp rides with the cab: it must be the map's last light.
 m.lights.push_back(lamp({12.f,11.8f},2.45f,1.4f,12.f,3600));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{5.5f,5.f}},{C::Wasp,{16.f,19.f}},{C::Huntsman,{18.5f,5.5f},upper}};
 m.pickupSpawns={{{13.f,16.f},P::Health},{{2.5f,20.f},P::Ammo,upper},{{20.5f,11.5f},P::Ammo},{{16.5f,6.5f},P::Health,upper}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,{6.6f+i*.45f,4.4f},lower,i*.7f});

 zone(m,"10 SURFACE LIFT",0,0,24,24,-1.f,1000);
 zone(m,"11A UPPER MANIFOLD",0,0,24,24,-7.f,-1.f);
 zone(m,"11 CONTAINMENT",0,0,24,24);
 titleCard(m,"REACTOR COMPLEX");
 return p;
}

// REACTOR CORE. A thirteen-metre hall round the pressure vessel: its control
// rod drives crowd the head under the polar crane, hot and cold legs run out to
// two steam generators, and the pressurizer stands east. A gantry ring at -6
// circles the vessel, reached over the bridge from Containment or by the east
// stair; the glazed R-03 control room overlooks the core from the north-west.
// The bulkhead to the Service Gallery stands in its own tower, south-west.
std::shared_ptr<const Map> makeReactorCore(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[ReactorCoreChunk];m.definition=CampaignChunks[ReactorCoreChunk];m.definition.ambient=.3f;
 const float lower=-9,gantry=-6,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer("Reactor core / floor",lower));
 {AuthoredLayerData l;l.name="Reactor core / gantry";l.elevation=gantry;l.thickness=.3f;for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));}
 auto& g=m.layers[0];auto& d=m.layers[1];
 // Seams: west to Containment (y 10-14), north to the Coolant Loop (x 10-14).
 rectangle(g,0,10,1,14,'.');m.openWest=true;
 rectangle(g,10,0,14,1,'.');m.openNorth=true;
 // The Coolant Loop's roof is lower: wall the seam above it.
 wall(m,10,0,14,.15f,-2.f,roof,3);
 rectangle(d,7,7,17,17,'=');rectangle(d,9,9,15,15,'.');   // ring round the vessel
 rectangle(d,0,10,7,14,'=');                              // bridge from Containment
 rectangle(d,1,1,9,7,'=');                                // R-03 control room
 rectangle(d,17,14,21,16,'=');                            // east stair landing
 m.stairs.push_back({19,16,21,22,lower,gantry,15,true,false});

 // --- The reactor ------------------------------------------------------------
 const Vec2 core{12,12};
 vessel(m,core,2.6f,lower,1.f,5);
 m.pipes.push_back({core,core,.85f,2.75f,1.05f,6});
 m.pipes.push_back({core,core,1.f,2.1f,1.6f,5});
 // A grated work platform round the head under the drive crown.
 for(int i=0;i<16;++i){float a=i*kPi/8,b=(i+1)*kPi/8;Vec2 post=core+Vec2{std::cos(a),std::sin(a)}*2.9f,next=core+Vec2{std::cos(b),std::sin(b)}*2.9f;m.pipes.push_back({post,post,1.f,.03f,2.05f,6});
  for(float z:{2.02f,1.5f})m.pipes.push_back({post,next,z,.022f,z,6});}
 m.pipes.push_back({core,core,.95f,3.f,1.f,2});
 for(int i=0;i<12;++i){float a=i*kPi/6;Vec2 rod=core+Vec2{std::cos(a),std::sin(a)}*1.1f;m.pipes.push_back({rod,rod,1.6f,.08f,2.6f,1});}
 for(int i=0;i<4;++i){float a=i*kPi*.5f+.4f;Vec2 rod=core+Vec2{std::cos(a),std::sin(a)}*.45f;m.pipes.push_back({rod,rod,1.6f,.08f,2.6f,1});}
 m.pipes.push_back({core,core,2.6f,1.35f,2.72f,2});
 m.structures.push_back({core.x-1.4f,core.y-1.4f,core.x+1.4f,core.y+1.4f,1.f,2.72f,false,6});
 // Two coolant loops, south and east. Each hot leg runs from a vessel nozzle
 // to its steam generator; the crossover leg leaves the generator's foot for
 // a floor-standing coolant pump, and the cold leg runs back under the gantry
 // into the vessel. Main steam rises from each generator dome to the roof.
 for(Vec2 dir:{Vec2{0,1},Vec2{1,0}}){
  Vec2 sg=core+dir*9.f,pump=core+dir*6.3f,side{-dir.y,dir.x};
  vessel(m,sg,1.5f,lower,1.f,0);
  m.pipes.push_back({sg,sg,1.f,1.1f,1.8f,0});m.pipes.push_back({sg,sg,1.8f,.7f,2.15f,0});
  pipeRoute(m,{{sg.x,sg.y,2.15f},{sg.x,sg.y,roof}},.32f,5,NoRoof);
  // Hot leg with its forged nozzles at both shells.
  Vec2 a=core+dir*2.6f,b=sg-dir*1.5f;
  m.pipes.push_back({a-dir*.2f,a+dir*.45f,-3.f,.62f,-3.f,6});m.pipes.push_back({b-dir*.45f,b+dir*.2f,-3.f,.62f,-3.f,6});
  pipeRoute(m,{{a.x+dir.x*.45f,a.y+dir.y*.45f,-3.f},{b.x-dir.x*.45f,b.y-dir.y*.45f,-3.f}},.45f,4,roof);
  // Crossover leg and the coolant pump on its plinth.
  Vec2 foot=sg-dir*1.5f;
  m.pipes.push_back({foot-dir*.3f,foot+dir*.2f,-7.7f,.5f,-7.7f,6});
  pipeRoute(m,{{foot.x-dir.x*.3f,foot.y-dir.y*.3f,-7.7f},{pump.x+dir.x*.65f,pump.y+dir.y*.65f,-7.7f}},.38f,5,NoRoof,lower);
  wall(m,pump.x-.95f,pump.y-.95f,pump.x+.95f,pump.y+.95f,lower,lower+.2f,15);
  m.pipes.push_back({pump,pump,lower+.2f,.68f,-6.45f,6});
  for(float z:{-8.5f,-7.1f})m.pipes.push_back({pump,pump,z,.76f,z+.12f,6});
  m.pipes.push_back({pump,pump,-6.45f,.42f,-5.6f,6});
  m.pipes.push_back({pump,pump,-5.6f,.6f,-3.7f,0});
  for(float z=-5.4f;z<-3.8f;z+=.3f)m.pipes.push_back({pump,pump,z,.66f,z+.06f,6});
  m.pipes.push_back({pump,pump,-3.7f,.36f,-3.3f,6});
  for(float s:{-1.f,1.f})steelColumn(m,pump.x+side.x*s*1.05f,pump.y+side.y*s*1.05f,lower+.2f,-5.6f,std::fabs(dir.y)>.5f);
  m.structures.push_back({pump.x-.8f,pump.y-.8f,pump.x+.8f,pump.y+.8f,lower,-3.3f,false,6});
  // Cold leg back to the vessel, tucked under the gantry with walking
  // clearance beneath it: the floor round the vessel stays one open space.
  Vec2 inlet=core+dir*2.6f;
  m.pipes.push_back({inlet-dir*.2f,inlet+dir*.4f,-6.75f,.41f,-6.75f,6});
  pipeRoute(m,{{pump.x-dir.x*.68f,pump.y-dir.y*.68f,-6.75f},{inlet.x+dir.x*.4f,inlet.y+dir.y*.4f,-6.75f}},.32f,5,gantry-.3f);
  Vec2 c1=inlet+dir*.4f,c2=pump-dir*.68f;
  m.structures.push_back({std::min(c1.x,c2.x)-.38f,std::min(c1.y,c2.y)-.38f,std::max(c1.x,c2.x)+.38f,std::max(c1.y,c2.y)+.38f,-7.08f,-6.3f,false,6});
 }
 // The pressurizer north-east, its surge line off the east hot leg and the
 // relief line to the roof.
 const Vec2 pressurizer{19.5f,5.f};
 vessel(m,pressurizer,.8f,lower,1.2f,5);
 m.pipes.push_back({pressurizer,pressurizer,1.2f,.55f,1.5f,5});
 pipeRoute(m,{{18.f,12.f,-2.55f},{18.f,5.f,-2.55f},{18.7f,5.f,-2.55f}},.14f,4,roof);
 pipeRoute(m,{{19.5f,5.f,1.5f},{19.5f,5.f,2.4f},{21.f,5.f,2.4f},{21.f,5.f,roof}},.08f,5,roof);
 // Polar crane over the head: twin girders and end trucks on runway beams
 // along both side walls, carried on corbels; the trolley and hook block.
 for(float y:{11.2f,12.5f})wall(m,1.4f,y,22.6f,y+.3f,roof-.65f,roof-.3f,18);
 for(float x:{1.f,22.6f})wall(m,x,11.f,x+.4f,13.f,roof-.8f,roof-.2f,12);
 for(float x:{1.f,22.6f}){wall(m,x,1,x+.4f,23,roof-1.05f,roof-.8f,12);
  for(float y=3.f;y<23;y+=4.f)wall(m,x,y-.2f,x+.4f,y+.2f,roof-1.6f,roof-1.05f,15);}
 wall(m,10.8f,11.f,13.2f,13.f,roof-1.05f,roof-.65f,12);
 m.pipes.push_back({{12.f,11.9f},{12.f,11.9f},roof-1.05f,.02f,roof-2.6f,6});m.pipes.push_back({{12.f,12.1f},{12.f,12.1f},roof-1.05f,.02f,roof-2.6f,6});
 wall(m,11.75f,11.75f,12.25f,12.25f,roof-2.9f,roof-2.6f,18);
 // Roof trusses span the hall; the gantry ring and decks stand on columns.
 for(float y=3.f;y<23;y+=4.f)if(std::fabs(y-12)>1.5f)wall(m,1,y-.12f,23,y+.12f,roof-.5f,roof,12);
 deckSupports(m,d,lower);
 // The floor: a painted exclusion square round the vessel, drains, and a
 // laydown area by the bulkhead with a cask, spares and the switchgear.
 floorLine(m,8.4f,8.4f,15.6f,8.55f,lower,13);floorLine(m,8.4f,15.45f,15.6f,15.6f,lower,13);
 floorLine(m,8.4f,8.4f,8.55f,15.6f,lower,13);floorLine(m,15.45f,8.4f,15.6f,15.6f,lower,13);
 for(Vec2 g2:{Vec2{4.5f,15.f},Vec2{17.f,19.f},Vec2{4.f,4.f}})wall(m,g2.x-.35f,g2.y-.35f,g2.x+.35f,g2.y+.35f,lower,lower+.012f,8);
 {float s=1.7f/2.390135f;m.fixtures.push_back({14,{8.6f,20.6f},0,1.58f*s,1.62f*s,1.7f,0,true});}
 cargo(m,2.4f,17.f,lower);cargo(m,2.4f,15.5f,lower,false);
 m.fixtures.push_back({41,{8.9f,22.2f},0,2.4f,1.f,1.1f,0,true});
 for(float x:{14.65f,17.f,17.9f})m.fixtures.push_back({13,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 cableTray(m,{14.2f,1.2f},{18.4f,1.2f},-6.65f);
 // Under the control room: its switchgear and the transformer feeding R-03.
 for(float x:{2.f,2.95f,3.9f,4.85f})m.fixtures.push_back({13,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 m.fixtures.push_back({6,{7.6f,2.2f},0,1.87f,.55f,.99f,kPi*.5f,true});
 cableTray(m,{1.6f,1.2f},{5.4f,1.2f},-6.7f);
 m.lights.push_back(lamp({4.f,4.f},gantry-.45f,.7f,5.f,4300,.2f));
 wallBays(m,lower,roof,4.f,true,true,false,true,12.f,5.5f);
 // Blue light from the vessel well washes up under the ring.
 for(Vec2 a:{Vec2{8.f,12.f},Vec2{16.f,13.2f},Vec2{12.f,8.f},Vec2{13.2f,16.f}})m.lights.push_back(lamp(a,gantry-.45f,1.4f,6.f,12000,-.05f));

 // --- 12A R-03 CONTROL (-6 m) -------------------------------------------------
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,gantry,gantry+.9f,3);wall(m,x1,y1,x2,y2,gantry+.9f,gantry+2.4f,17);wall(m,x1,y1,x2,y2,gantry+2.4f,gantry+2.7f,3);};
 glazed(1,6.88f,7,7);glazed(8.88f,1,9,7);
 wall(m,1,1,9,7,gantry+2.7f,gantry+2.82f,2);
 {AuthoredTerminalData t;t.position={4.5f,6.4f};t.title="REACTOR ACCESS / DRIVE A:";t.z=3;t.reactorAction=1;m.terminals.push_back(std::move(t));}
 for(float x:{2.5f,3.5f,4.5f,5.5f})m.fixtures.push_back({13,{x,1.27f},3,.9066f,.4956f,2.2f,kPi,true});
 m.fixtures.push_back({19,{7.4f,3.6f},3,1.367f,.724f,.78f,kPi*.5f,true});
 m.fixtures.push_back({18,{6.6f,3.6f},3,.417f,.525f,.95f,-kPi*.5f,true});
 wallSign(m,1.055f,4.f,gantry+1.7f,"R-03 CONTROL","CONTAINMENT / INTERLOCK",1.6f,kPi*.5f);
 m.lights.push_back(lamp({5.f,4.f},gantry+2.55f,1.f,6.f,3200));

 // --- Bulkhead to the Service Gallery ----------------------------------------
 rectangle(g,4,23,7,24,'.');
 {Door exit{4,7,21.5f,0,false,true};exit.requireEnemiesClear=true;exit.requireControl=true;m.doors.push_back(exit);}
 // The bulkhead sits in a metre-deep frame: its switch housings mount on the
 // frame's faces, and the vestibule walls continue from the frame.
 wall(m,3.4f,21,4,22,lower,roof,3);wall(m,7,21,7.6f,22,lower,roof,3);
 wall(m,3.85f,22,4,24,lower,roof,3);wall(m,7,22,7.15f,24,lower,roof,3);wall(m,4,23,7,24,lower+2.5f,roof,3);
 m.lights.push_back(lamp({3.85f,22.5f},lower+2.2f,1.2f,4.f,1300,-.3f,WorldLightMount::Wall,kPi));
 {ScriptEvent checkpoint;checkpoint.id=stateId("event_reactor_exit_checkpoint");checkpoint.x1=4;checkpoint.y1=21;checkpoint.x2=7;checkpoint.y2=24;
  checkpoint.bottom=-9.5f;checkpoint.top=-7.5f;checkpoint.requireState=stateId("reactor_bulkhead_released");checkpoint.requireEnemiesClear=true;
  ScriptAction save;save.type=ScriptAction::Type::Checkpoint;checkpoint.actions.push_back(save);m.scriptEvents.push_back(checkpoint);}

 // --- Light ---------------------------------------------------------------
 for(Vec2 b:{Vec2{6.f,10.f},Vec2{18.f,9.5f},Vec2{6.f,20.f},Vec2{17.f,20.f},Vec2{12.f,4.f},Vec2{12.f,20.f},Vec2{4.f,14.2f},Vec2{20.f,16.f}})m.lights.push_back(lamp(b,roof-.15f,3.f,14.f,5000,.1f));
 // Fittings under the gantry ring's corners and work lamps along the walls
 // light the floor that the roof lamps cannot reach.
 for(Vec2 b:{Vec2{7.5f,7.5f},Vec2{15.5f,7.6f},Vec2{8.f,15.8f},Vec2{15.4f,16.2f}})m.lights.push_back(lamp(b,gantry-.45f,1.f,6.f,4300,.15f));
 for(float y:{6.f,18.f})m.lights.push_back(lamp({22.95f,y},lower+2.3f,1.1f,6.f,3800,0,WorldLightMount::Wall,kPi));
 for(float y:{16.f,20.f})m.lights.push_back(lamp({1.05f,y},lower+2.3f,1.f,6.f,3800,0,WorldLightMount::Wall,0));
 for(float x:{10.f,18.f})m.lights.push_back(lamp({x,22.95f},lower+2.3f,1.f,6.f,3800,0,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({6.5f,1.05f},lower+2.3f,.9f,5.f,3800,0,WorldLightMount::Wall,kPi*.5f));
 m.lights.push_back(lamp({3.f,12.f},gantry-.45f,.8f,5.f,4300,.2f));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Warden,{16.5f,20.5f}},{C::Huntsman,{5.f,9.f}},{C::Wasp,{15.5f,8.f},gantry},{C::Huntsman,{17.5f,19.5f}}};
 m.pickupSpawns={{{2.f,12.f},P::Health,gantry},{{15.f,20.8f},P::Ammo},{{2.5f,4.5f},P::Ammo,gantry}};
 const Vec2 litter[]={{14.5f,18.8f},{15.f,19.3f},{15.6f,18.6f}};
 for(int i=0;i<3;++i)m.clutterSpawns.push_back({i+1,litter[i],lower,i*.9f});

 zone(m,"12A R-03 CONTROL",1,1,9,7,-7.f,-1.f);
 zone(m,"12 REACTOR CORE",0,0,24,24);
 return p;
}

// COOLANT LOOP. North of the core: two heat-exchanger shells on saddles, the
// cooling basins either side of a dry walkway, and a gantry along the north
// and east walls carrying the P-02 return valve.
std::shared_ptr<const Map> makeCoolantLoop(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[CoolantLoopChunk];m.definition=CampaignChunks[CoolantLoopChunk];m.definition.ambient=.34f;
 const float lower=-9,catwalk=-6,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer("Coolant loop / floor",lower));
 {AuthoredLayerData l;l.name="Coolant loop / gantry";l.elevation=catwalk;l.thickness=.3f;for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));}
 auto& g=m.layers[0];auto& d=m.layers[1];
 rectangle(g,10,23,14,24,'.');m.openSouth=true;
 rectangle(d,1,1,23,4,'=');rectangle(d,19,4,23,12,'=');
 m.stairs.push_back({20,12,22,18,lower,catwalk,15,true,false});
 // The circuit: hot mains rise from the core trench by the seam and drop onto
 // the two exchangers; their cooled outlets leave the west channel heads, rise
 // the west wall and join one return header along the north gantry, through
 // the P-02 valve, and back down into the floor trench to the core.
 const float axis=lower+1.35f,shellTop=axis+1.f;
 for(float y:{7.6f,12.4f}){
  m.pipes.push_back({{2.4f,y},{16.6f,y},axis,1.f,axis,5});
  for(float x=4.6f;x<16;x+=2.6f)m.pipes.push_back({{x-.09f,y},{x+.09f,y},axis,1.06f,axis,6});
  // Channel heads: bolted girth flanges and dished end caps.
  for(float x:{2.4f,16.6f})m.pipes.push_back({{x-.09f,y},{x+.09f,y},axis,1.14f,axis,6});
  m.pipes.push_back({{1.95f,y},{2.4f,y},axis,.9f,axis,5});m.pipes.push_back({{16.6f,y},{17.05f,y},axis,.9f,axis,5});
  m.pipes.push_back({{1.8f,y},{1.95f,y},axis,.6f,axis,5});m.pipes.push_back({{17.05f,y},{17.2f,y},axis,.6f,axis,5});
  // Saddles: concrete piers with steel cradles under the shell.
  for(float x:{4.f,9.5f,15.f}){wall(m,x-.3f,y-.85f,x+.3f,y+.85f,lower,axis-.85f,15);
   wall(m,x-.33f,y-.9f,x+.33f,y+.9f,axis-.85f,axis-.7f,12);
   for(float side:{-1.f,1.f})wall(m,x-.33f,y+side*.9f-.06f,x+.33f,y+side*.9f+.06f,axis-.7f,axis,12);}
  wall(m,1.8f,y-1.f,17.2f,y+1.f,lower,shellTop,6);
  // Instrument tap and a vent valve on the crown.
  m.pipes.push_back({{6.f,y},{6.f,y},shellTop-.05f,.05f,shellTop+.35f,6});
  m.pipes.push_back({{6.f,y},{6.f,y},shellTop+.35f,.11f,shellTop+.5f,4});
 }
 // Hot mains from the core trench, flanged into each shell's crown.
 for(float x:{9.4f,14.6f}){flange(m,{x,22.4f,lower+.02f},{},.24f);
  m.structures.push_back({x-.32f,22.08f,x+.32f,22.72f,lower,-4.f,false,6});
  wall(m,x-.45f,22.f,x+.45f,22.8f,lower,lower+.04f,8);}
 pipeRoute(m,{{9.4f,22.4f,lower},{9.4f,22.4f,-4.6f},{9.4f,12.4f,-4.6f},{9.4f,12.4f,shellTop}},.24f,4,roof);
 pipeRoute(m,{{14.6f,22.4f,lower},{14.6f,22.4f,-4.2f},{14.6f,7.6f,-4.2f},{14.6f,7.6f,shellTop}},.24f,4,roof);
 // Cooled return: out of the west heads, up the wall, one header north and
 // east along the gantry through the P-02 valve, then down into the trench.
 pipeRoute(m,{{1.8f,12.4f,axis},{1.5f,12.4f,axis},{1.5f,12.4f,-3.7f},{1.5f,7.6f,-3.7f}},.2f,5,roof);
 pipeRoute(m,{{1.8f,7.6f,axis},{1.5f,7.6f,axis},{1.5f,7.6f,-3.7f},{1.5f,1.7f,-3.7f},{18.2f,1.7f,-3.7f},{18.2f,1.7f,lower}},.2f,5,roof);
 flange(m,{18.2f,1.7f,catwalk+.02f},{},.2f);flange(m,{18.2f,1.7f,lower+.02f},{},.2f);
 m.structures.push_back({17.95f,1.45f,18.45f,1.95f,lower,-3.5f,false,6});
 gateValve(m,{11.f,1.7f,-3.7f},{1,0},.2f,lower,1);
 for(float x:{6.f,15.5f})gateValve(m,{x,1.7f,-3.7f},{1,0},.2f,lower,1);
 // Return booster skid and electrical gear under the north gantry.
 for(float x:{3.f,4.f,5.f})m.fixtures.push_back({13,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 cableTray(m,{2.5f,1.2f},{6.f,1.2f},catwalk-.7f);
 m.fixtures.push_back({6,{21.f,9.5f},0,1.87f,.55f,.99f,kPi*.5f,true});
 m.fixtures.push_back({14,{21.2f,6.7f},0,1.58f,1.62f,2.3f,0,true});
 // The gantry stands on steel columns.
 deckSupports(m,d,lower);
 // Cooling basins either side of the walkway, sunk in the floor with a
 // coping kerb, a guard rail on every side and an outfall from the wall.
 m.waterVolumes={{2.f,16.f,8.5f,21.f,-10.4f,-9.32f},{14.5f,16.f,18.5f,21.f,-10.4f,-9.32f}};
 for(const auto& b:m.waterVolumes){
  for(float x:{b.x1-.25f,b.x2})wall(m,x,b.y1-.25f,x+.25f,b.y2+.25f,lower,lower+.18f,15);
  for(float y:{b.y1-.25f,b.y2})wall(m,b.x1,y,b.x2,y+.25f,lower,lower+.18f,15);
  floorLine(m,b.x1-.25f,b.y1-.4f,b.x2+.25f,b.y1-.28f,lower,13);floorLine(m,b.x1-.25f,b.y2+.28f,b.x2+.25f,b.y2+.4f,lower,13);
  // A 1.55 m fence, taller than a jump: the basin is not a trap.
  const float rx1=b.x1-.12f,rx2=b.x2+.12f,ry1=b.y1-.12f,ry2=b.y2+.12f,top=lower+1.55f;
  for(float x=rx1;x<=rx2+.01f;x+=(rx2-rx1)/std::ceil((rx2-rx1)/1.6f))for(float y:{ry1,ry2})m.pipes.push_back({{x,y},{x,y},lower+.18f,.03f,top,3});
  for(float y=ry1;y<=ry2+.01f;y+=(ry2-ry1)/std::ceil((ry2-ry1)/1.6f))for(float x:{rx1,rx2})m.pipes.push_back({{x,y},{x,y},lower+.18f,.03f,top,3});
  for(float z:{top,lower+1.05f,lower+.55f}){
   for(float y:{ry1,ry2})m.pipes.push_back({{rx1,y},{rx2,y},z,.025f,z,3});
   for(float x:{rx1,rx2})m.pipes.push_back({{x,ry1},{x,ry2},z,.025f,z,3});
  }
  for(float x:{rx1-.03f,rx2-.03f})m.structures.push_back({x,ry1,x+.06f,ry2,lower,top,false,6});
  for(float y:{ry1-.03f,ry2-.03f})m.structures.push_back({rx1,y,rx2,y+.06f,lower,top,false,6});
 }
 pipeRoute(m,{{1.f,18.5f,-6.6f},{3.2f,18.5f,-6.6f},{3.2f,18.5f,-10.35f}},.12f,5,roof);
 pipeRoute(m,{{16.5f,23.f,-6.6f},{16.5f,20.f,-6.6f},{16.5f,20.f,-10.35f}},.12f,5,roof);
 // A marked walkway from the seam to the exchangers, and floor drains.
 for(float x:{9.85f,14.05f})floorLine(m,x,13.6f,x+.1f,23.f,lower,14);
 for(Vec2 g2:{Vec2{12.f,14.6f},Vec2{12.f,20.6f},Vec2{5.f,10.f},Vec2{19.f,14.f}})wall(m,g2.x-.35f,g2.y-.35f,g2.x+.35f,g2.y+.35f,lower,lower+.012f,8);
 // Wall pilasters and roof beams: the hall is built, not a box.
 wallBays(m,lower,roof,4.f,false,true,true,false,12.f);
 for(float x:{3.f,8.f,13.5f,18.5f})wall(m,x-.13f,1,x+.13f,23,roof-.45f,roof,12);
 // P-02 return valve on the gantry edge, its wheel to the walkway.
 {AuthoredTerminalData t;t.position={11.f,3.6f};t.title="02 / COOLANT RETURN";t.z=3;t.reactorAction=3;m.terminals.push_back(std::move(t));}
 terminal(m,{8.8f,22.3f},"COOLANT LOOP / LOCAL LOG","RETURN VALVE IS ON THE NORTH GANTRY.","FEED MUST BE PRIMED BEFORE THE RETURN.");
 // --- Light ---------------------------------------------------------------
 for(Vec2 b:{Vec2{6.f,10.f},Vec2{15.f,10.f},Vec2{11.5f,18.5f}})m.lights.push_back(lamp(b,roof-.15f,1.8f,10.f,5600,.2f));
 for(Vec2 b:{Vec2{5.f,18.5f},Vec2{16.5f,18.5f}})m.lights.push_back(lamp(b,roof-.15f,1.f,6.f,7000,.15f));
 // Fluorescent fittings under the whole gantry, and along the outer walls.
 for(float x:{2.5f,6.f,10.f,14.f,17.5f,21.5f})m.lights.push_back(lamp({x,2.5f},catwalk-.45f,.9f,5.5f,4300,.15f));
 for(float y:{5.5f,8.5f,11.f})m.lights.push_back(lamp({21.5f,y},catwalk-.45f,.9f,5.5f,4300,.15f));
 for(float y:{5.f,16.f,21.5f})m.lights.push_back(lamp({2.5f,y},roof-.15f,.9f,6.f,4300,.2f));
 for(float y:{15.f,20.5f})m.lights.push_back(lamp({21.5f,y},roof-.15f,1.f,6.f,4300,.2f));
 m.lights.push_back(lamp({11.f,4.6f},roof-.15f,1.f,6.f,3800));
 m.lights.push_back(lamp({9.f,23.f},lower+2.f,1.f,4.f,1300,-.3f,WorldLightMount::Wall,-kPi*.5f));
 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{6.f,10.f}},{C::Wasp,{14.f,10.f}},{C::Huntsman,{15.f,2.5f},catwalk}};
 m.pickupSpawns={{{21.f,8.f},P::Ammo,catwalk},{{11.5f,19.5f},P::Health}};
 zone(m,"12B COOLANT LOOP",0,0,24,24);
 return p;
}


// SERVICE GALLERY. The reactor bulkhead opens on a north landing; a four-metre
// service spine runs south under the headers and cable tray, straight on into
// the Coolant Return. Off the spine, each behind an open archway and a
// swinging door from its cross corridor: the pump workshop where the assembly
// was moved, the electrical room that must stay dry, the stores and the plant
// bay. Nothing hunts here: it is the breath between the reactor and the water.
std::shared_ptr<const Map> makeServiceGallery(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float floor=-9,roof=-5.8f,top=roof;
 m.name=CampaignMapNames[4];m.definition=CampaignChunks[4];m.definition.ceiling=roof;m.definition.ambient=.32f;
 m.layers.push_back(floorLayer("Reactor Service Gallery",floor,false,true));m.openSouth=true;
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 rectangle(g(),13,0,16,1,'.');m.doors.push_back({13,16,.5f,0,false,false,true});
 auto partition=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,floor,top,3);};
 auto swing=[&](float left,float right,float y){
  // A swinging leaf in a 25 cm frame, with the wall closed above the frame.
  // Leaves open into their rooms, never across the narrow cross corridors.
  Door d{left,right,y};d.swinging=true;d.swing=y<12?-1.f:1.f;m.doors.push_back(d);wall(m,left,y-.125f,right,y+.125f,floor+2.55f,top,3);};
 // Rooms: workshop (NW), electrical (NE), stores (SW), plant (SE). Each opens
 // on the spine through an archway and on its cross corridor through a door.
 partition(1,4.75f,9.85f,5);partition(14.15f,4.75f,23,5);
 for(float x:{9.85f,14.f}){partition(x,5,x+.15f,6.5f);partition(x,8.f,x+.15f,11.25f);wall(m,x,6.5f,x+.15f,8.f,floor+2.4f,top,3);
  partition(x,13,x+.15f,17.f);partition(x,18.5f,x+.15f,23);wall(m,x,17.f,x+.15f,18.5f,floor+2.4f,top,3);}
 partition(1,11,4.6f,11.25f);partition(6.f,11,10,11.25f);swing(4.6f,6.f,11.125f);
 partition(14,11,18.f,11.25f);partition(19.4f,11,23,11.25f);swing(18.f,19.4f,11.125f);
 partition(1,13,3.f,13.25f);partition(4.4f,13,10,13.25f);swing(3.f,4.4f,13.125f);
 partition(14,13,20.f,13.25f);partition(21.4f,13,23,13.25f);swing(20.f,21.4f,13.125f);
 partition(1,22.75f,10,23);partition(14,22.75f,23,23);

 // --- Landing --------------------------------------------------------------
 {AuthoredTerminalData t;t.position={9.5f,1.4f};t.title="SERVICE GALLERY / WORK ORDER";t.line1="PUMP ASSEMBLY MOVED TO WEST WORKSHOP.";t.line2="ELECTRICAL ROOM MUST REMAIN DRY.";t.yaw=.001f;m.terminals.push_back(std::move(t));}
 for(float x:{19.f,20.f,21.f})m.fixtures.push_back({13,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 // Freight waiting on the landing for the gallery stores, and the riser.
 cargo(m,20.4f,3.55f,floor);cargo(m,21.75f,3.55f,floor,false);cargo(m,17.4f,3.55f,floor,false);
 m.fixtures.push_back({44,{22.94f,2.5f},0,.4f,.1f,3.1f,-kPi*.5f,false});
 m.fixtures.push_back({45,{22.87f,2.5f},1.25f,.32f,.12f,.32f,-kPi*.5f,false});
 // Spine services: headers along the west side, a cable tray along the east.
 for(float x:{10.4f,10.85f})servicePipe(m,{x,1.05f},{x,24.f},roof-.45f,.11f,x<10.6f?3:4);
 wall(m,13.55f,1,13.95f,24,roof-.42f,roof-.34f,2);
 for(float y=2.f;y<24;y+=3.f)wall(m,13.55f,y,13.95f,y+.06f,roof-.34f,roof,2);

 // --- 13A PUMP WORKSHOP -----------------------------------------------------
 m.fixtures.push_back({12,{4.f,6.f},0,3.06f,.94f,1.751f,0,true});
 m.props.push_back({0,{3.3f,8.9f},1.25f,2.2f,0,{1.1f,.66f},.15f});wall(m,2.05f,8.15f,4.55f,9.65f,floor,floor+.15f,15);
 m.fixtures.push_back({19,{7.6f,9.3f},0,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({7,{1.27f,7.6f},0,1.8f,.5f,1.8f,kPi*.5f,true});

 // --- 13B ELECTRICAL ----------------------------------------------------------
 for(float y:{6.f,7.f,8.f,9.f})m.fixtures.push_back({13,{22.75f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});
 m.fixtures.push_back({41,{17.5f,6.f},0,2.4f,1.f,1.1f,0,true});
 // A cabinet with a cracked bus bar arcs across the floor in front of it.
 hazard(m,Hazard::Kind::Electricity,20.6f,8.6f,21.8f,9.4f,floor-1,floor+2,10);
 wallSign(m,13.945f,9.5f,floor+1.8f,"ELECTRICAL","KEEP DRY / LIVE GEAR",1.8f,-kPi*.5f);

 // --- 13C STORES --------------------------------------------------------------
 for(float y:{15.f,17.5f,20.f})m.fixtures.push_back({7,{1.27f,y},0,1.8f,.5f,1.8f,kPi*.5f,true});
 for(float y:{16.f,19.5f})m.fixtures.push_back({7,{5.5f,y},0,1.8f,.5f,1.8f,0,true});
 cargo(m,8.4f,21.5f,floor);cargo(m,8.4f,14.6f,floor,false);

 // --- 13D PLANT BAY -----------------------------------------------------------
 m.fixtures.push_back({14,{17.6f,16.f},0,2.404f,2.645f,2.2f,0,true});
 for(float y:{16.5f,21.3f})m.fixtures.push_back({6,{21.3f,y},0,1.87f,.55f,.99f,0,true});
 m.props.push_back({1,{17.6f,20.6f},1.12f,2.f,0,{1.f,.31f},.12f});wall(m,16.5f,20.2f,18.7f,21.f,floor,floor+.12f,15);
 servicePipe(m,{17.6f,13.3f},{17.6f,22.75f},roof-.5f,.12f,3);m.pipes.push_back({{17.6f,16.f},{17.6f,16.f},floor+2.2f,.1f,roof-.5f,3});

 // --- Light ---------------------------------------------------------------
 const float lampZ=roof-.15f;
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,2.6f},lampZ,.9f,6.f,4300,.2f));
 m.lights.push_back(lamp({12.3f,1.f},floor+2.f,1.1f,4.f,1300,-.3f,WorldLightMount::Wall,kPi*.5f));
 for(float y:{8.f,15.f,21.f})m.lights.push_back(lamp({12.f,y},lampZ,1.f,7.f,4300,.2f));
 for(float x:{5.f,19.f})m.lights.push_back(lamp({x,12.1f},lampZ,.6f,4.f,4300,.2f));
 m.lights.push_back(lamp({5.5f,8.f},lampZ,1.1f,6.f,3200));
 m.lights.push_back(lamp({18.5f,8.f},lampZ,1.f,6.f,6500,.1f));
 m.lights.push_back(lamp({5.5f,18.f},lampZ,.8f,6.f,4000));
 m.lights.push_back(lamp({18.5f,18.f},lampZ,1.f,6.f,2200));

 // --- Supplies ----------------------------------------------------------------
 using P=PickupKind;
 m.pickupSpawns={{{8.6f,6.6f},P::Health},{{3.f,21.6f},P::Ammo}};
 const Vec2 tools[]={{7.2f,10.3f},{7.9f,10.4f},{5.6f,11.8f},{12.6f,3.f}};
 for(int i=0;i<4;++i)m.clutterSpawns.push_back({i+2,tools[i],floor,i*.9f});

 zone(m,"13A PUMP WORKSHOP",1,5,10,11);
 zone(m,"13B ELECTRICAL",14,5,23,11);
 zone(m,"13C STORES",1,13,10,23);
 zone(m,"13D PLANT BAY",14,13,23,23);
 zone(m,"13 SERVICE GALLERY",0,0,24,24);
 return p;
}

// COOLANT RETURN. Where the plant's spent coolant settles before it is pumped
// on. The Service Gallery spine opens onto a landing above one long settling
// channel that spans the hall wall to wall; two intake culverts pour into it
// from the north. The footbridge across it has fallen in, so the way south is
// a wade: knee-deep at the sunken span, deeper either side. Beyond, the duty
// and standby return pumps lift the water to the vaults. Nothing hunts here:
// the water is the lesson. Wade it, climb out, watch what floats.
std::shared_ptr<const Map> makeCoolantReturn(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float floor=-9,roof=-4.6f;
 m.name=CampaignMapNames[5];m.definition=CampaignChunks[5];m.definition.ceiling=roof;m.definition.ambient=.3f;
 m.layers.push_back(floorLayer("Coolant Return",floor,true));m.openNorth=true;
 auto& g=m.layers[0];
 // The gallery spine arrives through the open seam; the north wall closes
 // either side of it.
 wall(m,1,0,10,.25f,floor,roof,3);wall(m,14,0,23,.25f,floor,roof,3);
 // --- The settling channel ----------------------------------------------------
 const float bed=-9.62f,surface=-9.07f;
 m.waterVolumes={{1.f,8.6f,23.f,14.4f,bed,surface}};
 for(float y:{8.35f,14.4f})wall(m,1,y,23,y+.25f,floor,floor+.12f,15);
 floorLine(m,1,8.2f,23,8.32f,floor,13);floorLine(m,1,14.68f,23,14.8f,floor,13);
 // Intake culverts from the north wall, on hangers, spilling over a weir lip.
 for(float x:{5.f,19.f}){
  pipeRoute(m,{{x,.25f,-6.f},{x,8.9f,-6.f},{x,8.9f,-9.6f}},.42f,5,roof);
  m.structures.push_back({x-.5f,8.4f,x+.5f,9.4f,bed,-6.f,false,6});
  wall(m,x-.7f,8.6f,x+.7f,9.2f,bed,floor+.05f,15);
  floorLine(m,x-.7f,8.6f,x+.7f,8.72f,floor+.05f,13);
 }
 // The condemned footbridge: both abutments stand, the span lies in the water.
 for(auto [y1,y2]:{std::pair{8.6f,9.7f},{13.3f,14.4f}}){
  wall(m,11,y1,13,y2,floor,floor+.22f,8);
  for(float x:{11.f,12.94f})m.structures.push_back({x,y1,x+.06f,y2,floor+.22f,floor+1.2f,true,0});
 }
 wall(m,11.1f,9.9f,12.9f,11.6f,bed,-9.52f,8);wall(m,11.2f,11.6f,12.8f,13.1f,bed,-9.4f,8);
 board(m,10.2f,7.4f,floor+1.1f,"FOOTBRIDGE CONDEMNED","CHANNEL WADEABLE / KEEP TO THE SPAN",2.2f);
 // --- North landing: what arrives, and where it goes ------------------------
 m.fixtures.push_back({14,{3.f,4.5f},0,2.404f,2.645f,2.2f,0,true});
 m.fixtures.push_back({14,{21.f,4.5f},0,2.404f,2.645f,2.2f,0,true});
 for(float x:{3.f,21.f})pipeRoute(m,{{x,4.5f,floor+2.2f},{x,4.5f,-6.f},{x<12?5.f:19.f,4.5f,-6.f}},.16f,4,roof);
 terminal(m,{12.f,6.6f},"RETURN HALL / LOCAL CONTROL","DUPLEX CIRCULATION / DUTY AND STANDBY.","FOOTBRIDGE DOWN SINCE THE SURGE. WADE THE CHANNEL.");
 // --- South: the return pumps ---------------------------------------------
 // Suction trench from the channel to each pump; discharge risers into one
 // header that leaves through the south wall for the vaults.
 const float header=-6.4f;
 std::array<P3,4> risers{};int k=0;
 for(float x:{3.6f,7.2f,16.8f,20.4f}){
  wall(m,x-.3f,14.65f,x+.3f,15.9f,floor,floor+.012f,8);
  risers[size_t(k++)]=pumpSkid(m,{x,17.3f},floor,kPi*.5f,header);
 }
 pipeRoute(m,{risers[0],{risers[3].x,risers[3].y,header}},.16f,5,roof);
 pipeRoute(m,{{14.f,risers[0].y,header},{14.f,22.95f,header}},.16f,5,roof);
 for(int i=0;i<4;++i)gateValve(m,{risers[size_t(i)].x+(i<2?.9f:-.9f),risers[size_t(i)].y,header},{1,0},.16f,floor,1);
 // West bay: the shift's lockers and a parts cage.
 for(float y:{18.5f,21.f,22.f})m.fixtures.push_back({13,{1.27f,y},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
 m.fixtures.push_back({41,{4.5f,22.f},0,2.4f,1.f,1.1f,0,true});
 // Rear store: a swinging door into a stock room of spares.
 wall(m,17.25f,20.65f,19.05f,20.87f,floor,roof,3);wall(m,20.45f,20.65f,22.85f,20.87f,floor,roof,3);
 wall(m,17.25f,22.85f,22.85f,23.08f,floor,roof,3);wall(m,17.25f,20.87f,17.47f,22.85f,floor,roof,3);
 wall(m,19.05f,20.65f,20.45f,20.87f,floor+2.55f,roof,3);
 {Door store{19.05f,20.45f,20.76f};store.swinging=true;m.doors.push_back(store);}
 m.fixtures.push_back({7,{21.4f,22.48f},0,1.6f,.48f,1.8f,0,true});
 m.fixtures.push_back({8,{18.15f,22.74f},.78f,.67f,.20f,.91f,0,false});
 // Transfer to the Cable Vaults.
 rectangle(g,8,23,11,24,'.');m.doors.push_back({8,11,23.5f,0,false,true});
 wall(m,8,23,11,24,floor+2.5f,roof,3);
 // What floats: a bottle bobbing in the channel, a gasket box in the store.
 m.clutterSpawns={{2,{7.5f,11.5f},surface},{0,{21.f,21.6f},floor}};
 wallBays(m,floor,roof,4.f,false,false,true,true,11.5f);
 for(float y:{5.5f,16.5f,20.5f})wall(m,1,y-.1f,23,y+.1f,roof-.3f,roof,12);
 // --- Light ---------------------------------------------------------------
 // Cold tubes along the channel draw the eye across the water; warm lamps
 // over the pumps; amber over the transfer.
 for(float x:{3.5f,8.5f,15.5f,20.5f})m.lights.push_back(lamp({x,11.5f},roof-.15f,1.2f,7.f,6500,.2f));
 for(float x:{6.f,12.f,18.f})m.lights.push_back(lamp({x,3.f},roof-.15f,1.f,6.f,4300,.15f));
 for(float x:{5.4f,18.6f})m.lights.push_back(lamp({x,18.6f},roof-.15f,1.1f,5.f,3400,.05f));
 m.lights.push_back(lamp({12.f,19.f},roof-.15f,.9f,5.f,4300,.15f));
 m.lights.push_back(lamp({20.f,22.f},roof-.15f,.9f,4.f,3000));
 m.lights.push_back(lamp({7.5f,22.95f},floor+2.2f,1.2f,4.f,1600,-.2f,WorldLightMount::Wall,-kPi*.5f));
 m.pickupSpawns={{{21.5f,7.f},PickupKind::Health}};
 zone(m,"14A RETURN STORE",17.25f,20.65f,22.85f,23.08f);
 zone(m,"14 COOLANT RETURN",0,0,24,24);
 return p;
}

// CABLE VAULTS. The plant's feeder runs underground here in three tunnels off
// a north hall. The centre tunnel is the straight way south, but its cable
// trench has flooded and is still live: the arc cycles across the water. The
// feeder's remote trip is at the end of the east tunnel. The west tunnel goes
// round, past the maintenance bay that keeps the flashlight, and then into the
// dark. Trip the feeder and the arc dies; the exit unlocks either way you go.
std::shared_ptr<const Map> makeCableVaults(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float floor=-9,roof=-4.65f;
 m.name=CampaignMapNames[6];m.definition=CampaignChunks[6];m.definition.ceiling=roof;m.definition.ambient=.24f;
 m.layers.push_back(floorLayer("Cable Vaults / service tunnels",floor));
 auto& g=m.layers[0];
 rectangle(g,2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true});
 // Tunnel walls: west | centre | east, between the north and south halls.
 wall(m,7.5f,4,8.f,19,floor,roof,15);wall(m,16.f,4,16.5f,19,floor,roof,15);
 // The east tunnel ends in the feeder room; the south hall is reached from
 // the west and centre tunnels only.
 wall(m,16.5f,18.8f,23,19,floor,roof,15);
 // --- The live trench (centre) -------------------------------------------------
 m.waterVolumes={{8.f,6.f,16.f,17.f,-9.62f,-9.22f}};
 for(float y:{5.75f,17.f})wall(m,8.f,y,16.f,y+.25f,floor,floor+.1f,15);
 floorLine(m,8.f,5.55f,16.f,5.7f,floor,13);floorLine(m,8.f,17.3f,16.f,17.45f,floor,13);
 {Hazard arc;arc.kind=Hazard::Kind::Electricity;arc.x1=8;arc.y1=6;arc.x2=16;arc.y2=17;arc.bottom=-9.62f;arc.top=-8.6f;arc.damagePerSecond=22;
  arc.enabledFlag=stateId("vault_disconnect");arc.invertFlag=true;arc.period=4;arc.onTime=1.2f;m.hazards.push_back(arc);}
 // Live cable racks three high down both trench walls, hung from the roof.
 for(float x:{8.2f,15.8f})for(float z:{-7.3f,-6.6f,-5.9f})cableTray(m,{x,4.5f},{x,18.5f},z);
 board(m,12.f,4.3f,floor+1.1f,"DANGER","LIVE CABLE TRENCH / 11 KV / ISOLATE AT FEEDER 7A",2.4f);
 // --- North hall ---------------------------------------------------------------
 for(float x:{13.f,14.f})m.fixtures.push_back({13,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 cableTray(m,{1.2f,2.f},{1.2f,18.5f},roof-.45f);cableTray(m,{22.8f,2.f},{22.8f,18.f},roof-.45f);
 // --- West tunnel: the maintenance bay, then the dark ----------------------------
 for(float y:{5.f,7.f})m.fixtures.push_back({7,{1.27f,y},0,1.8f,.5f,1.8f,kPi*.5f,true});
 bench(m,5.5f,5.2f,floor);
 {ScriptEvent kit;kit.id=stateId("vault_maintenance_kit");kit.level=6;kit.x1=2.f;kit.y1=4.5f;kit.x2=7.f;kit.y2=9.f;kit.bottom=-9.2f;kit.top=-7;
  ScriptAction give;give.type=ScriptAction::Type::GiveItem;give.id=stateId("flashlight");give.value=1;kit.actions.push_back(give);m.scriptEvents.push_back(kit);}
 // Past the bay the tunnel lights are out: cable drums and a sagging tray.
 for(Vec2 d:{Vec2{3.f,12.5f},Vec2{5.5f,15.5f}})m.pipes.push_back({{d.x-.45f,d.y},{d.x+.45f,d.y},floor+.6f,.6f,floor+.6f,8});
 for(Vec2 d:{Vec2{3.f,12.5f},Vec2{5.5f,15.5f}})m.structures.push_back({d.x-.5f,d.y-.62f,d.x+.5f,d.y+.62f,floor,floor+1.2f,false,6});
 // --- East tunnel: feeder units, transformers and the trip ------------------------
 m.fixtures.push_back({12,{18.6f,7.f},0,2.0f,1.55f,1.75f,.10f,true});
 m.fixtures.push_back({12,{21.2f,10.5f},0,2.0f,1.55f,1.75f,-.08f,true});
 for(Vec2 t:{Vec2{18.f,15.5f},Vec2{21.f,15.5f}}){wall(m,t.x-1.f,t.y-.4f,t.x+1.f,t.y+.4f,floor,floor+.12f,15);m.fixtures.push_back({6,t,.12f,1.87f,.55f,.99f,0,true});
  pipeRoute(m,{{t.x,t.y,floor+1.1f},{t.x,t.y,roof-.13f}},.06f,6,NoRoof);}
 for(float y:{12.5f,13.5f})m.fixtures.push_back({13,{22.75f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});
 terminal(m,{19.5f,13.f},"FEEDER 7A / REMOTE TRIP","LIVE TRENCH / CROSS ONLY WHEN ISOLATED.","E / THROW LOCAL DISCONNECT.",0,stateId("vault_disconnect"));
 terminal(m,{6.8f,8.6f},"CABLE VAULTS / MAINTENANCE","LOCAL DISCONNECT: FEEDER 7A, EAST TUNNEL.","WEST TUNNEL LIGHTING FAILED. TAKE A LAMP.");
 wallSign(m,20.f,18.745f,floor+1.9f,"FEEDER 7A","11 KV / REMOTE TRIP",1.6f,kPi);
 // --- South hall and the exit ------------------------------------------------------
 rectangle(g,20,23,23,24,'.');{Door exit{20,23,23.5f,0,false,true};exit.requireState=stateId("vault_disconnect");m.doors.push_back(exit);}
 wall(m,20,23,23,24,floor+2.5f,roof,3);
 m.fixtures.push_back({41,{10.f,22.2f},0,2.4f,1.f,1.1f,0,true});
 for(float y:{4.f,19.5f})wall(m,1,y-.1f,23,y+.1f,roof-.3f,roof,12);
 // --- Light ---------------------------------------------------------------
 for(Vec2 b:{Vec2{4.f,2.5f},Vec2{12.f,2.5f},Vec2{20.f,2.5f},Vec2{4.5f,6.5f}})m.lights.push_back(lamp(b,roof-.15f,1.1f,6.f,4300,.2f));
 for(Vec2 b:{Vec2{19.5f,6.f},Vec2{19.5f,10.5f},Vec2{19.5f,14.f}})m.lights.push_back(lamp(b,roof-.15f,1.f,5.5f,3600,.1f));
 // Blue-white work lamps at the trench ends; the tunnel between is lit by the arc.
 for(float y:{4.6f,18.f})m.lights.push_back(lamp({12.f,y},roof-.15f,1.1f,5.f,7500,.1f));
 for(float x:{4.f,12.f})m.lights.push_back(lamp({x,21.f},roof-.15f,.9f,5.f,4300,.15f));
 m.lights.push_back(lamp({19.5f,22.95f},floor+2.2f,1.2f,4.f,1600,-.2f,WorldLightMount::Wall,-kPi*.5f));
 // --- Encounters ------------------------------------------------------------
 m.creatureSpawns={{CreatureKind::Huntsman,{4.f,17.f}},{CreatureKind::Huntsman,{13.f,21.f}}};
 m.pickupSpawns={{{20.5f,17.5f},PickupKind::Ammo}};
 m.clutterSpawns={{3,{3.f,6.f}},{0,{2.6f,8.2f}}};
 {ScriptEvent trip;trip.id=stateId("vault_relay_trip");trip.level=6;trip.x1=2;trip.y1=1;trip.x2=5;trip.y2=3;trip.bottom=-9.2f;trip.top=-7;
  ScriptAction play;play.type=ScriptAction::Type::PlaySound;play.amount=.6f;play.sound=Sound::Door;trip.actions.push_back(play);m.scriptEvents.push_back(trip);}
 zone(m,"15A MAINTENANCE BAY",1,4,7.5f,9);
 zone(m,"15B FEEDER ROOM",16.5f,12,23,18.8f);
 zone(m,"15 CABLE VAULTS",0,0,24,24);
 titleCard(m,"CABLE VAULTS");
 return p;
}

// PUMP ANNEX. A three-storey pump house. You come in on the main floor round
// a pit where four vertical duty pumps stand in a row, their motors rising
// above the floor like landmarks. The pumps are stopped: the local start is
// at the bottom of the pit. Start them and the house shakes back to life, and
// the observation gallery's transfer to the Junction unlocks. The gallery
// runs high along the south over the pit, reached by the east stair.
std::shared_ptr<const Map> makePumpAnnex(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float pit=-12,deck=-9,gallery=-4,roof=-1.f;
 m.name=CampaignMapNames[7];m.definition=CampaignChunks[7];m.definition.ceiling=roof;m.definition.ambient=.38f;
 m.layers.push_back(floorLayer("Pump Annex / pump pit",pit));
 auto layer=[&](const char* name,float z){AuthoredLayerData l;l.name=name;l.elevation=z;l.thickness=.25f;for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));};
 layer("Pump Annex / main floor",deck);layer("Pump Annex / observation",gallery);
 auto& g=m.layers[0];auto& main=m.layers[1];auto& obs=m.layers[2];
 rectangle(main,1,1,23,23,'=');rectangle(main,6,6,18,16,'.');
 rectangle(obs,8,16,23,23,'=');rectangle(obs,20,16,22,17,'=');
 // Entry on the main floor, exit from the gallery: the walls below are sealed.
 rectangle(g,2,0,5,1,'.');rectangle(main,2,0,5,1,'=');
 m.doors.push_back({2,5,.5f,0,false,false,true,3});
 wall(m,2,0,5,1,pit,deck-.25f,15);wall(m,2,0,5,.85f,deck+2.5f,roof,3);
 // The pit: concrete walls round it, a stair down its west side.
 wall(m,5.8f,6,6,16,pit,deck-.25f,15);wall(m,18,6,18.2f,16,pit,deck-.25f,15);
 wall(m,5.8f,5.8f,18.2f,6,pit,deck-.25f,15);wall(m,5.8f,16,18.2f,16.2f,pit,deck-.25f,15);
 m.stairs.push_back({6,6,8,12,pit,deck,15,true,false});
 m.stairs.push_back({20,6,22,16,deck,gallery,26,true,true});
 // --- The duty pumps ----------------------------------------------------------
 // Pump bowl and discharge head on the pit floor, the column through the deck
 // line, the motor on a steel stool; each discharges south into the header.
 const float headerZ=-11.1f;
 for(float x:{9.6f,12.f,14.4f,16.8f}){
  Vec2 c{x,10.f};
  wall(m,x-.9f,9.1f,x+.9f,10.9f,pit,pit+.15f,15);
  m.pipes.push_back({c,c,pit+.15f,.62f,pit+1.3f,5});
  for(float z:{pit+.3f,pit+1.1f})m.pipes.push_back({c,c,z,.7f,z+.1f,6});
  m.pipes.push_back({c,c,pit+1.3f,.26f,deck+.3f,6});
  m.pipes.push_back({c,c,deck+.3f,.55f,deck+1.9f,0});
  for(float z=deck+.5f;z<deck+1.8f;z+=.32f)m.pipes.push_back({c,c,z,.6f,z+.06f,6});
  m.pipes.push_back({c,c,deck+1.9f,.3f,deck+2.2f,6});
  for(float s:{-1.f,1.f})steelColumn(m,x+s*.7f,10.f,pit+.15f,deck+.3f,false);
  wall(m,x-.85f,9.55f,x+.85f,10.45f,deck+.2f,deck+.3f,12);
  m.structures.push_back({x-.75f,9.25f,x+.75f,11.2f,pit,deck+2.2f,false,6});
  pipeRoute(m,{{x,10.62f,pit+.7f},{x,10.95f,pit+.7f},{x,10.95f,pit-.3f}},.18f,5,NoRoof);
  wall(m,x-.3f,11.2f,x+.3f,15.4f,pit-.012f,pit+.006f,8);
 }
 wall(m,9.2f,14.95f,17.9f,15.45f,pit-.012f,pit+.006f,8);
 pipeRoute(m,{{17.6f,15.2f,pit-.3f},{17.6f,15.2f,-5.2f},{17.6f,22.95f,-5.2f}},.24f,5,gallery-.25f);
 m.structures.push_back({17.2f,14.8f,18.f,16.f,pit,deck-.25f,false,6});
 wallSign(m,12.f,15.945f,pit+1.9f,"P-21 TO P-24","DUTY PUMPS / LOCAL START BELOW",2.2f,kPi);
 // The local start, on the pit floor in front of the header.
 terminal(m,{12.6f,13.2f},"DUTY PUMPS / LOCAL START","RESTORE PRESSURE BEFORE LEAVING THE ANNEX.","E / START DUTY PUMPS. GALLERY STAIR EAST.",0,stateId("annex_running"));
 for(float x:{9.f,10.f})m.fixtures.push_back({13,{x,15.73f},0,.9066f,.4956f,2.2f,kPi,true});
 {ScriptEvent restart;restart.id=stateId("annex_pump_restart");restart.x1=1;restart.y1=1;restart.x2=23;restart.y2=23;restart.bottom=-12.1f;restart.top=-2;restart.requireState=stateId("annex_running");
  ScriptAction shake;shake.type=ScriptAction::Type::Shake;shake.amount=.18f;restart.actions.push_back(shake);
  ScriptAction motor;motor.type=ScriptAction::Type::PlaySound;motor.amount=.75f;motor.sound=Sound::LiftMotor;restart.actions.push_back(motor);
  ScriptAction save;save.type=ScriptAction::Type::Checkpoint;restart.actions.push_back(save);m.scriptEvents.push_back(restart);}
 // --- Main floor ------------------------------------------------------------------
 // Motor control centre along the west wall, cable trays to the pit edge.
 for(float y:{8.f,9.f,10.f,11.f})m.fixtures.push_back({13,{1.27f,y},3,.9066f,.4956f,2.2f,-kPi*.5f,true});
 cableTray(m,{1.6f,7.5f},{1.6f,12.f},deck+2.6f);cableTray(m,{2.f,12.3f},{5.6f,12.3f},deck+2.6f);
 m.fixtures.push_back({41,{3.5f,19.5f},3,2.4f,1.f,1.1f,0,true});
 cargo(m,2.5f,15.f,deck);cargo(m,2.5f,16.4f,deck,false);
 // Pit edge columns carry the gallery; the crane runs the length of the house.
 deckSupports(m,obs,deck,6.f);
 for(float x:{4.f,9.f,15.f,20.f}){wall(m,x-.15f,1,x+.15f,23,roof-.55f,roof,12);
  for(float y:{1.f,22.75f})wall(m,x-.25f,y,x+.25f,y+.25f,gallery+2.9f,roof-.55f,15);}
 // --- Observation gallery ------------------------------------------------------------
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,gallery,gallery+.9f,3);wall(m,x1,y1,x2,y2,gallery+.9f,gallery+2.4f,17);wall(m,x1,y1,x2,y2,gallery+2.4f,gallery+2.7f,3);};
 glazed(14.f,19.f,19.f,19.12f);glazed(14.f,19.12f,14.12f,21.2f);
 wall(m,14,21.2f,14.12f,22.8f,gallery+2.2f,gallery+2.7f,3);
 wall(m,14,19,19,23,gallery+2.7f,gallery+2.85f,2);
 terminal(m,{16.5f,21.8f},"PUMP ANNEX / OBSERVATION","DUTY PUMP START: PIT FLOOR CONTROL.","JUNCTION ACCESS: SOUTH GALLERY DOOR.",8);
 m.fixtures.push_back({19,{17.6f,20.f},8,1.367f,.724f,.78f,kPi,true});
 // Transfer to the Junction, framed as a personnel door.
 rectangle(g,19,23,22,24,'.');rectangle(obs,19,23,22,24,'=');
 {Door exit{19.8f,21.2f,23.5f,0,false,true,false,8};exit.swinging=true;exit.requireState=stateId("annex_running");m.doors.push_back(exit);}
 wall(m,19,23,22,24,pit,gallery-.25f,15);
 wall(m,19,23.15f,19.8f,24,gallery,roof,3);wall(m,21.2f,23.15f,22,24,gallery,roof,3);
 wall(m,19.8f,23.15f,21.2f,24,gallery+2.55f,roof,3);
 // --- Light ---------------------------------------------------------------
 for(Vec2 b:{Vec2{9.f,8.f},Vec2{15.f,8.f},Vec2{9.f,13.5f},Vec2{15.f,13.5f}})m.lights.push_back(lamp(b,roof-.15f,2.2f,11.f,4600,.15f));
 for(Vec2 b:{Vec2{3.5f,3.5f},Vec2{20.5f,3.5f},Vec2{3.5f,13.f}})m.lights.push_back(lamp(b,roof-.15f,1.f,7.f,4300,.15f));
 // Bulkhead lamps round the main floor walls, and down inside the pit: every
 // working level of the house is lit where people stand.
 for(float x:{6.5f,12.f,17.5f})m.lights.push_back(lamp({x,1.02f},deck+2.4f,1.2f,6.5f,4000,.1f,WorldLightMount::Wall,kPi*.5f));
 for(float y:{5.f,11.f,17.f})m.lights.push_back(lamp({1.02f,y},deck+2.4f,1.2f,6.5f,4000,.1f,WorldLightMount::Wall,0));
 for(float y:{9.f,13.5f})m.lights.push_back(lamp({22.98f,y},deck+2.4f,1.2f,6.5f,4000,.1f,WorldLightMount::Wall,kPi));
 for(float y:{8.5f,13.5f}){m.lights.push_back(lamp({6.02f,y},pit+2.f,1.f,5.f,3600,.05f,WorldLightMount::Wall,0));m.lights.push_back(lamp({17.98f,y},pit+2.f,1.f,5.f,3600,.05f,WorldLightMount::Wall,kPi));}
 for(float x:{10.f,16.f,21.f})m.lights.push_back(lamp({x,19.5f},gallery-.4f,1.f,5.5f,3800,.1f));
 m.lights.push_back(lamp({16.5f,21.f},gallery+2.55f,.9f,4.f,3000));
 for(float x:{10.f,14.f})m.lights.push_back(lamp({x,6.02f},pit+2.f,1.f,5.f,3400,0,WorldLightMount::Wall,kPi*.5f));
 m.lights.push_back(lamp({12.6f,15.95f},pit+2.3f,1.1f,4.5f,1800,-.1f,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({19.4f,23.12f},gallery+2.f,1.f,4.f,1600,-.2f,WorldLightMount::Wall,-kPi*.5f));
 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Wasp,{12.f,3.5f},deck},{C::Wasp,{11.f,20.f},gallery},{C::Huntsman,{8.6f,13.6f},pit},{C::Huntsman,{15.5f,13.6f},pit}};
 m.pickupSpawns={{{3.f,10.f},P::Health,deck},{{21.f,18.5f},P::Ammo,gallery}};
 zone(m,"16A PUMP PIT",6,6,18,16,-13,-9.1f);
 zone(m,"16B OBSERVATION",8,16,23,24,-4.5f,1000);
 zone(m,"16 PUMP ANNEX",0,0,24,24);
 return p;
}

// UTILITY JUNCTION. Where the plant's services split. You arrive high on the
// bridge from the Annex and see the whole hub from its overlook: three trunk
// mains, colour coded, cross the hall into one distribution header, then part
// for Waste Handling (yellow, south), Freight Services (red, north-east) and
// Primary Utilities (white, south-west). Dispatch in the glazed office releases
// the south route; each side branch has its own local control at its gate.
std::shared_ptr<const Map> makeUtilityJunction(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float floor=-9,bridge=-4,roof=-.8f;
 m.name=CampaignMapNames[8];m.definition=CampaignChunks[8];m.definition.ceiling=roof;m.definition.ambient=.36f;
 m.layers.push_back(floorLayer("Utility Junction / concourse",floor));
 {AuthoredLayerData l;l.name="Utility Junction / arrival bridge";l.elevation=bridge;l.thickness=.25f;for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));}
 auto& g=m.layers[0];auto& b=m.layers[1];
 rectangle(b,1,1,17,3,'=');rectangle(b,10,3,14,8,'=');rectangle(b,2,0,5,1,'=');
 m.stairs.push_back({1,4,3,14,floor,bridge,26,true,false});
 rectangle(b,1,3,3,4,'=');
 // Arrival: through the Annex door onto the bridge.
 rectangle(g,2,0,5,1,'.');
 {Door entry{2.8f,4.2f,.5f,0,false,false,true,5};entry.swinging=true;m.doors.push_back(entry);}
 wall(m,2,0,5,1,floor,bridge-.25f,15);
 wall(m,2,0,2.8f,.85f,bridge,roof,3);wall(m,4.2f,0,5,.85f,bridge,roof,3);wall(m,2.8f,0,4.2f,.85f,bridge+2.55f,roof,3);
 deckSupports(m,b,floor,6.f);
 for(float x:{10.25f,13.75f})steelColumn(m,x,7.75f,floor,bridge-.25f);
 {ScriptEvent arrive;arrive.id=stateId("junction_arrival");arrive.x1=2;arrive.y1=1;arrive.x2=6;arrive.y2=4;arrive.bottom=-4.1f;arrive.top=-2;
  ScriptAction save;save.type=ScriptAction::Type::Checkpoint;arrive.actions.push_back(save);m.scriptEvents.push_back(arrive);}
 // The overlook's wayfinding board.
 board(m,12.f,7.3f,bridge+1.f,"JUNCTION 17","WASTE SOUTH / FREIGHT NORTH-EAST / UTILITIES SOUTH-WEST",3.2f);
 // --- Branch gates ---------------------------------------------------------------
 // Freight Services, north-east: a vestibule under its own slab.
 wall(m,17,1,17.2f,5.65f,floor,bridge,3);wall(m,17,5.35f,18.5f,5.65f,floor,bridge,3);wall(m,21.5f,5.35f,23,5.65f,floor,bridge,3);
 wall(m,18.5f,5.35f,21.5f,5.65f,floor+2.5f,bridge,3);wall(m,17,1,23,5.65f,bridge,bridge+.2f,2);
 {Door freight{18.5f,21.5f,5.5f};freight.requireState=stateId("freight_incident_clearance");freight.sign=CampaignChunkCount;m.doors.push_back(freight);}
 // Primary Utilities, south-west.
 wall(m,1,17.35f,3,17.65f,floor,roof,3);wall(m,5,17.35f,6.2f,17.65f,floor,roof,3);wall(m,6,17.65f,6.2f,23,floor,roof,3);
 wall(m,3,17.35f,5,17.65f,floor+2.5f,roof,3);
 {Door utilities{3,5,17.5f};utilities.requireState=stateId("primary_utilities_permit");utilities.sign=CampaignChunkCount+1;m.doors.push_back(utilities);}
 // --- Dispatch office ------------------------------------------------------------
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,floor,floor+.9f,3);wall(m,x1,y1,x2,y2,floor+.9f,floor+2.4f,17);wall(m,x1,y1,x2,y2,floor+2.4f,floor+2.7f,3);};
 glazed(9.f,15.f,11.2f,15.12f);glazed(12.8f,15.f,15.f,15.12f);wall(m,11.2f,15.f,12.8f,15.12f,floor+2.2f,floor+2.7f,3);
 glazed(9.f,15.12f,9.12f,20.f);glazed(14.88f,15.12f,15.f,20.f);
 wall(m,9.f,19.88f,15.f,20.f,floor,floor+2.7f,3);wall(m,9,15,15,20,floor+2.7f,floor+2.85f,2);
 terminal(m,{12.f,18.6f},"JUNCTION / WASTE DISPATCH","DISPATCH INTERLOCK / SOUTH BULKHEAD.","E / RELEASE WASTE HANDLING ROUTE.",0,stateId("waste_access"));
 m.fixtures.push_back({19,{10.4f,18.8f},0,1.367f,.724f,.78f,kPi*.5f,true});
 for(float x:{13.4f,14.2f})m.fixtures.push_back({13,{x,19.6f},0,.7f,.45f,1.9f,0,true});
 // --- The junction: three mains into one header, three branches out ----------------
 const float overhead=-6.4f,drum=-7.6f;
 m.pipes.push_back({{7.f,8.4f},{7.f,14.6f},drum,.58f,drum,5});
 for(float y:{8.4f,14.6f})m.pipes.push_back({{7.f,y-.08f},{7.f,y+.08f},drum,.66f,drum,6});
 for(float y:{9.5f,13.5f}){wall(m,6.6f,y-.3f,7.4f,y+.3f,floor,drum-.5f,15);wall(m,6.5f,y-.34f,7.5f,y+.34f,drum-.5f,drum-.38f,12);}
 m.structures.push_back({6.35f,8.3f,7.65f,14.7f,floor,drum+.6f,false,6});
 const struct {float y;int paint;} mains[]={{9.5f,4},{11.5f,3},{13.5f,5}};
 for(auto main:mains)pipeRoute(m,{{22.95f,main.y,overhead},{7.f,main.y,overhead},{7.f,main.y,drum+.58f}},.2f,main.paint,roof);
 pipeRoute(m,{{7.f,8.75f,drum+.58f},{7.f,8.75f,overhead},{7.f,8.2f,overhead},{16.f,8.2f,overhead},{16.f,4.5f,overhead},{16.95f,4.5f,overhead}},.16f,4,roof);
 pipeRoute(m,{{7.f,14.3f,drum+.58f},{7.f,14.3f,overhead},{7.f,21.2f,overhead},{19.f,21.2f,overhead},{19.f,22.95f,overhead}},.16f,3,roof);
 pipeRoute(m,{{7.f,12.5f,drum+.58f},{7.f,12.5f,-5.6f},{4.5f,12.5f,-5.6f},{4.5f,17.3f,-5.6f}},.14f,5,roof);
 wallSign(m,22.945f,14.f,floor+1.6f,"TRUNK MAINS","FREIGHT RED / WASTE YELLOW / UTILITIES WHITE",2.4f,-kPi*.5f);
 // --- Local controls for the branch gates ----------------------------------------
 terminal(m,{16.2f,7.5f},"FREIGHT SERVICES / INCIDENT OVERRIDE","FREIGHT ROUTE / LOCAL OVERRIDE.","E / CLEAR THE INCIDENT INTERLOCK.",0,stateId("freight_incident_clearance"));
 terminal(m,{6.6f,16.4f},"PRIMARY UTILITIES / PERMIT DESK","WEST SERVICE GATE / LOCAL PERMIT.","E / RELEASE THE UTILITIES GATE.",0,stateId("primary_utilities_permit"));
 // Inside the branches: the freight manifest, the utilities substation.
 terminal(m,{20.f,2.f},"FREIGHT / OUTGOING MANIFEST","RESEARCH CONTAINERS HELD AT THE NEXT TRANSFER.","FREIGHT LINE SUSPENDED BEYOND THIS DOOR.");
 m.terminals.back().yaw=.001f; // its back to the north wall, screen to the room
 wall(m,18.f,1.f,22.f,1.2f,floor,floor+2.8f,7);
 for(float y:{19.5f,21.f})m.fixtures.push_back({13,{1.27f,y},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
 m.fixtures.push_back({7,{5.6f,21.f},0,1.8f,.5f,1.8f,kPi*.5f,true});
 // --- Concourse ---------------------------------------------------------------------
 m.fixtures.push_back({12,{16.5f,11.5f},0,1.8f,1.05f,1.45f,0,true});
 m.fixtures.push_back({12,{19.5f,16.5f},0,1.8f,1.05f,1.45f,kPi*.5f,true});
 cargo(m,20.5f,9.f,floor);cargo(m,21.8f,9.f,floor,false);
 // The south exit to Waste Handling.
 rectangle(g,20,23,23,24,'.');{Door exit{20,23,23.5f,0,false,true};exit.requireState=stateId("waste_access");m.doors.push_back(exit);}
 wall(m,20,23,23,24,floor+2.5f,roof,3);
 wallBays(m,floor,roof,4.f,false,true,false,true,21.5f,5.f);
 // --- Light ---------------------------------------------------------------
 for(Vec2 l:{Vec2{12.f,11.5f},Vec2{18.f,11.5f},Vec2{4.f,19.f},Vec2{20.f,19.5f},Vec2{18.f,14.f}})m.lights.push_back(lamp(l,roof-.15f,1.8f,10.f,4800,.15f));
 for(Vec2 l:{Vec2{12.f,10.f},Vec2{20.f,4.f}})m.lights.push_back(lamp(l,roof-.15f,1.f,6.f,4300,.15f));
 for(Vec2 l:{Vec2{5.f,2.f},Vec2{9.f,2.f},Vec2{15.f,2.f},Vec2{12.f,5.f}})m.lights.push_back(lamp(l,bridge-.4f,1.1f,6.f,3800,.1f));
 // The arrival bridge itself, lit from the roof so the overlook reads first.
 for(Vec2 l:{Vec2{3.5f,2.f},Vec2{8.5f,2.f},Vec2{14.f,2.f},Vec2{12.f,6.f}})m.lights.push_back(lamp(l,roof-.15f,1.2f,6.5f,4300,.15f));
 m.lights.push_back(lamp({1.02f,16.2f},floor+2.4f,1.f,5.5f,4000,.1f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({12.f,17.5f},floor+2.55f,1.f,5.f,3600));
 m.lights.push_back(lamp({20.f,3.5f},bridge-.15f,.9f,4.f,3000));
 m.lights.push_back(lamp({3.f,20.5f},roof-.15f,.9f,4.5f,4300,.15f));
 for(float y:{9.f,14.f})m.lights.push_back(lamp({22.98f,y},floor+2.4f,1.1f,6.5f,4000,.1f,WorldLightMount::Wall,kPi));
 for(float x:{9.f,15.f})m.lights.push_back(lamp({x,22.98f},floor+2.4f,1.f,6.f,4000,.1f,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({1.02f,15.5f},floor+2.4f,1.f,6.f,4000,.1f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({19.5f,22.95f},floor+2.2f,1.2f,4.f,1600,-.2f,WorldLightMount::Wall,-kPi*.5f));
 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{14.f,12.5f}},{C::Wasp,{16.f,17.f}}};
 m.pickupSpawns={{{17.f,13.5f},P::Health},{{21.f,3.5f},P::Ammo},{{3.1f,21.f},P::Ammo},{{8.f,2.f},P::Ammo,bridge}};
 m.clutterSpawns={{3,{20.f,10.2f}},{1,{21.3f,10.3f}},{2,{13.4f,18.f}},{3,{4.6f,20.6f}},{0,{2.f,22.f}},{1,{15.f,13.f}}};
 zone(m,"17A DISPATCH",9,15,15,20);
 zone(m,"17B FREIGHT SERVICES",17,1,23,6);
 zone(m,"17C PRIMARY UTILITIES",1,17.5f,6.2f,23);
 zone(m,"17 UTILITY JUNCTION",0,0,24,24);
 return p;
}

// WASTE HANDLING. Where the plant's scrap is sorted and crushed before it is
// shipped. You come in on the sorting gallery: conveyors run along it from
// the wall chutes to the hopper over the press. Below, a bunker wall divides
// the processing floor, and the only way south is through the press bay
// under the cycling platen. Time the stroke, or isolate the press from the
// glazed control pulpit on the gallery. Beyond the bunker, the sump, the
// balers and the dispatch room to the freight district.
std::shared_ptr<const Map> makeWasteHandling(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float floor=-12,gallery=-9,roof=-5.3f;
 m.name=CampaignMapNames[9];m.definition=CampaignChunks[9];m.definition.ceiling=roof;m.definition.ambient=.4f;
 m.layers.push_back(floorLayer("Waste Handling / processing floor",floor));
 {AuthoredLayerData l;l.name="Waste Handling / sorting gallery";l.elevation=gallery;l.thickness=.25f;for(auto& row:l.rows)row=std::string(24,'.');
  rectangle(l,1,1,23,7,'=');rectangle(l,2,0,5,1,'=');m.layers.push_back(std::move(l));}
 auto& g=m.layers[0];auto& deck=m.layers[1];
 // Arrival from the Junction onto the gallery; the wall below is sealed.
 rectangle(g,2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true,3});
 wall(m,2,0,5,1,floor,gallery-.25f,15);wall(m,2,0,5,.85f,gallery+2.5f,roof,3);
 m.stairs.push_back({1,7,3,13,floor,gallery,16,true,false});
 deckSupports(m,deck,floor,6.f);
 // --- Sorting gallery: conveyors to the hopper ------------------------------------
 // Two wall chutes drop onto a belt that runs east along the gallery to a
 // hopper over the press. Belts stand on frames; the hopper on its own legs.
 auto belt=[&](float x1,float x2,float y){wall(m,x1,y-.45f,x2,y+.45f,gallery+.7f,gallery+.82f,8);
  for(float yy:{y-.5f,y+.42f})wall(m,x1,yy,x2,yy+.08f,gallery+.6f,gallery+.95f,18);
  for(float x=x1+.3f;x<x2;x+=1.6f)for(float yy:{y-.42f,y+.36f})wall(m,x,yy,x+.06f,yy+.06f,gallery,gallery+.7f,12);
  for(float x=x1+.2f;x<x2;x+=.5f)wall(m,x,y-.4f,x+.07f,y+.4f,gallery+.62f,gallery+.7f,12);};
 belt(4.f,14.4f,5.8f);
 // Two round drop chutes come down through the roof onto the belt.
 for(float x:{5.f,9.f}){
  m.pipes.push_back({{x,5.8f},{x,5.8f},gallery+1.15f,.34f,roof-.4f,1});m.pipes.push_back({{x,5.8f},{x,5.8f},roof-1.f,.4f,roof-.92f,6});
  for(float z:{gallery+1.15f,gallery+2.6f})m.pipes.push_back({{x,5.8f},{x,5.8f},z,.4f,z+.1f,6});
  }
 // The hopper: a steel funnel on legs at the gallery edge, its throat over the press.
 wall(m,14.1f,5.1f,15.9f,7.f,gallery+.4f,gallery+1.3f,12);
 for(float x:{14.2f,15.7f})for(float y:{5.2f,6.8f})m.pipes.push_back({{x,y},{x,y},gallery+.05f,.04f,gallery+.4f,6});
 // --- Control pulpit over the press ---------------------------------------------
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,gallery,gallery+.95f,3);wall(m,x1,y1,x2,y2,gallery+.95f,gallery+2.3f,17);wall(m,x1,y1,x2,y2,gallery+2.3f,gallery+2.6f,3);};
 glazed(16.5f,6.88f,21.5f,7.f);glazed(21.38f,3.5f,21.5f,6.88f);
 wall(m,16.5f,3.5f,17.6f,3.62f,gallery,gallery+2.6f,3);wall(m,19.f,3.5f,21.5f,3.62f,gallery,gallery+2.6f,3);wall(m,17.6f,3.5f,19.f,3.62f,gallery+2.2f,gallery+2.6f,3);
 wall(m,16.5f,3.62f,16.62f,6.88f,gallery,gallery+2.6f,3);
 wall(m,16.5f,3.5f,21.5f,7.f,gallery+2.6f,gallery+2.75f,2);
 terminal(m,{18.5f,6.3f},"HYDRAULIC PRESS / LOCAL ISOLATOR","AMBER: CYCLING / GREEN: ISOLATED.","E / ISOLATE THE PRESS. WAY SOUTH IS THROUGH THE BAY.",3,stateId("compactor_isolated"));
 m.terminals.back().toggleState=true;
 for(float x:{20.f,20.9f})m.fixtures.push_back({13,{x,3.9f},3,.9066f,.4956f,2.2f,kPi,true});
 // --- The press bay -----------------------------------------------------------------
 m.compactors={{13,8,17,13,floor,-9.8f,9,stateId("compactor_isolated")}};
 // The press frame's four columns are solid (the renderer draws the frame).
 for(float x:{13.16f,16.84f})for(float y:{8.12f,12.88f})m.structures.push_back({x-.105f,y-.105f,x+.105f,y+.105f,floor,-9.18f,false,6});
 wall(m,13,5,17,14,floor,floor+.03f,8);
 for(float y:{7.85f,13.05f})floorLine(m,13,y,17,y+.1f,floor,13);
 // Bay walls either side, and the bunker wall dividing the floor: the bay is the way through.
 wall(m,12.7f,7.6f,13.f,14.3f,floor,-9.6f,15);wall(m,17.f,7.6f,17.3f,14.3f,floor,-9.6f,15);
 wall(m,1,14.f,12.7f,14.3f,floor,-9.6f,15);wall(m,17.3f,14.f,23,14.3f,floor,-9.6f,15);
 for(float x:{12.7f,17.f})floorLine(m,x-.15f,7.6f,x+.45f,7.75f,floor,13);
 wallSign(m,12.f,13.995f,floor+1.8f,"DANGER","PRESS BAY / CYCLING / ISOLATE AT PULPIT",2.2f,kPi);
 // Hydraulic power unit east of the bay: pump, tank, lines up to the crosshead.
 m.fixtures.push_back({14,{20.3f,10.5f},0,1.58f,1.62f,1.9f,0,true});
 P3 hp=pumpSkid(m,{20.3f,8.4f},floor,kPi*.5f,-8.4f);
 pipeRoute(m,{hp,{17.6f,hp.y,-8.4f},{17.6f,8.12f,-8.4f},{17.6f,8.12f,-9.4f},{16.96f,8.12f,-9.4f}},.09f,4,roof);
 pipeRoute(m,{{20.3f,10.5f,floor+1.9f},{20.3f,10.5f,-8.1f},{17.8f,10.5f,-8.1f},{17.8f,12.88f,-8.1f},{17.8f,12.88f,-9.4f},{16.96f,12.88f,-9.4f}},.07f,3,roof);
 // North floor: receiving bins under the chutes, the shredder, scrap.
 for(float x:{4.5f,8.5f}){
  wall(m,x-1.3f,9.f,x+1.3f,9.12f,floor,floor+1.1f,11);wall(m,x-1.3f,11.88f,x+1.3f,12.f,floor,floor+1.1f,11);
  wall(m,x-1.3f,9.12f,x-1.18f,11.88f,floor,floor+1.1f,11);wall(m,x+1.18f,9.12f,x+1.3f,11.88f,floor,floor+1.1f,11);
  floorLine(m,x-1.3f,8.8f,x+1.3f,8.92f,floor,13);
 }
 m.fixtures.push_back({12,{8.5f,10.5f},0,2.0f,1.55f,.8f,.3f,true});
 m.fixtures.push_back({12,{10.5f,12.6f},0,.940f,3.060f,1.751f,kPi*.5f,true});
 // --- South floor: sump, balers, dispatch ---------------------------------------
 // A drainage sump with a curb and rail, and its pump.
 m.waterVolumes={{2.f,16.f,7.f,21.f,-12.55f,-12.07f}};
 for(float x:{1.75f,7.f})wall(m,x,15.75f,x+.25f,21.25f,floor,floor+.15f,15);
 for(float y:{15.75f,21.f})wall(m,1.75f,y,7.25f,y+.25f,floor,floor+.15f,15);
 for(float y:{15.85f,21.15f})m.pipes.push_back({{1.85f,y},{7.15f,y},floor+1.05f,.025f,floor+1.05f,3});
 for(float x:{1.85f,4.5f,7.15f})for(float y:{15.85f,21.15f})m.pipes.push_back({{x,y},{x,y},floor+.15f,.03f,floor+1.05f,3});
 for(float x:{1.85f,7.15f})m.pipes.push_back({{x,15.85f},{x,21.15f},floor+1.05f,.025f,floor+1.05f,3});
 for(float y:{15.82f,21.12f})m.structures.push_back({1.82f,y,7.18f,y+.06f,floor,floor+1.08f,false,6});
 for(float x:{1.82f,7.12f})m.structures.push_back({x,15.82f,x+.06f,21.18f,floor,floor+1.08f,false,6});
 P3 sump=pumpSkid(m,{9.5f,18.5f},floor,0,-8.f);
 pipeRoute(m,{sump,{9.5f,22.95f,-8.f}},.1f,5,roof);
 // Balers: two compact bales presses on plinths with finished bales stacked.
 for(float x:{14.5f,18.5f}){m.fixtures.push_back({12,{x,17.2f},.12f,2.45f,1.30f,1.40f,0,true});wall(m,x-1.4f,16.4f,x+1.4f,18.f,floor,floor+.12f,15);}
 for(Vec2 b:{Vec2{13.5f,20.f},Vec2{14.7f,20.f},Vec2{13.5f,21.2f}})m.fixtures.push_back({21,b,0,1.f,1.f,.9f,0,true});
 // Dispatch room and the transfer to Freight Access.
 wall(m,17.8f,19.8f,19.1f,20,floor,-9.2f,3);wall(m,21.3f,19.8f,23,20,floor,-9.2f,3);
 wall(m,19.1f,19.8f,21.3f,20,-9.6f,-9.2f,3);wall(m,17.8f,20,18,23,floor,-9.2f,3);
 wall(m,17.8f,19.8f,23,23,-9.2f,-9.f,2);
 terminal(m,{20,21.5f},"SALVAGE DISPATCH / FREIGHT SERVICES","OUTGOING MANIFEST: RESEARCH CONTAINERS.","FREIGHT ACCESS / SOUTH DISPATCH BULKHEAD.");
 m.fixtures.push_back({7,{22.35f,22.f},0,1.8f,.5f,1.8f,kPi*.5f,true});
 rectangle(g,20,23,23,24,'.');m.doors.push_back({20,23,23.5f,0,false,true});
 wall(m,20,23,23,24,floor+2.5f,roof,3);
 {ScriptEvent dispatch;dispatch.id=stateId("waste_dispatch_checkpoint");dispatch.x1=19;dispatch.y1=20;dispatch.x2=22;dispatch.y2=23;dispatch.bottom=-12.1f;dispatch.top=-10;
  ScriptAction save;save.type=ScriptAction::Type::Checkpoint;dispatch.actions.push_back(save);m.scriptEvents.push_back(dispatch);}
 terminal(m,{3.2f,4.f},"SORTING / SHIFT SAFETY","PRESS ISOLATOR: PULPIT ON THIS GALLERY, EAST.","BAY UNDER THE PRESS IS THE ONLY WAY SOUTH.",3);
 wallBays(m,floor,roof,4.f,false,true,true,true,20.5f,12.f);
 for(float x:{6.f,12.f,18.f})wall(m,x-.12f,1,x+.12f,23,roof-.45f,roof,12);
 // --- Light ---------------------------------------------------------------
 for(Vec2 b:{Vec2{4.f,3.f},Vec2{10.f,3.f},Vec2{14.f,3.f}})m.lights.push_back(lamp(b,roof-.15f,1.f,6.f,4300,.15f));
 m.lights.push_back(lamp({19.f,5.2f},gallery+2.45f,1.f,4.5f,3200));
 for(Vec2 b:{Vec2{15.f,10.5f}})m.lights.push_back(lamp(b,roof-.15f,2.f,9.f,5200,.2f));
 for(Vec2 b:{Vec2{6.5f,10.5f},Vec2{20.f,10.f},Vec2{4.5f,18.5f},Vec2{10.f,18.f},Vec2{16.5f,19.f},Vec2{20.3f,21.5f}})m.lights.push_back(lamp(b,roof-.15f,1.3f,7.f,4000,.1f));
 for(float x:{4.f,12.f,20.f})m.lights.push_back(lamp({x,7.f},gallery-.4f,1.f,5.f,3800,.1f));
 for(float x:{11.f,19.f})m.lights.push_back(lamp({x,14.f},floor+2.f,1.2f,4.5f,1800,-.15f,WorldLightMount::Wall,-kPi*.5f));
 // Work lamps on the bay walls, and along the bunker's south face.
 for(float y:{9.6f,12.4f}){m.lights.push_back(lamp({13.f,y},floor+2.f,1.3f,4.5f,4200,.1f,WorldLightMount::Wall,0));m.lights.push_back(lamp({17.f,y},floor+2.f,1.3f,4.5f,4200,.1f,WorldLightMount::Wall,kPi));}
 for(float x:{4.f,9.f,21.f})m.lights.push_back(lamp({x,14.3f},floor+2.2f,1.2f,6.f,4000,.1f,WorldLightMount::Wall,kPi*.5f));
 m.lights.push_back(lamp({19.f,23.12f},floor+2.1f,1.1f,4.f,1600,-.2f,WorldLightMount::Wall,-kPi*.5f));
 // --- Encounters ------------------------------------------------------------
 m.creatureSpawns={{CreatureKind::Huntsman,{19,10},floor},{CreatureKind::Huntsman,{8,21},floor},{CreatureKind::Wasp,{12,4},gallery}};
 m.pickupSpawns={{{6.f,3.f},PickupKind::Ammo,gallery},{{21.f,18.f},PickupKind::Health,floor}};
 const Vec2 scrap[]={{3.8f,10.f},{4.6f,10.9f},{5.2f,10.1f},{8.f,9.6f},{9.f,11.3f},{3.9f,11.5f},
                     {6.5f,4.6f},{7.2f,4.8f},{11.f,4.7f},{12.2f,4.5f},{2.5f,12.6f},{3.4f,13.f},
                     {9.f,16.f},{10.5f,15.2f},{12.f,16.f},{20.5f,15.3f},{21.4f,15.8f},{15.4f,21.6f},{15.f,7.f}};
 for(int i=0;i<19;++i)m.clutterSpawns.push_back({i%6,scrap[i],(scrap[i].y<7.f&&scrap[i].x<14.f)?gallery:-999.f,.37f*i});
 zone(m,"18A SORTING GALLERY",0,0,24,7.1f,-9.5f,1000);
 zone(m,"18B PRESS BAY",12.7f,7.6f,17.3f,14.3f);
 zone(m,"18 WASTE HANDLING",0,0,24,24);
 return p;
}

// FREIGHT ACCESS. The goods entrance to the freight district, low and
// workaday. Waste Handling's dispatch door opens into the personnel wing:
// lockers, the clock-in station and the tool crib, behind a glazed screen.
// East of it the freight hall: a trailer dock in the east wall, the dock
// office, forklift charging and staged loads, and painted lanes running to
// the freight gate south, which opens onto the Lift Machinery hall.
std::shared_ptr<const Map> makeFreightAccess(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[10];m.definition=CampaignChunks[10];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.4f;
 const float floor=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),floor,false,true));m.openSouth=true;
 auto& g=m.layers[0];
 rectangle(g,2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true});
 // --- Personnel wing (west) -------------------------------------------------------
 // A glazed screen with two doorways divides it from the freight hall.
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,floor,floor+.95f,3);wall(m,x1,y1,x2,y2,floor+.95f,floor+2.25f,17);wall(m,x1,y1,x2,y2,floor+2.25f,roof,3);};
 glazed(6.6f,1,6.8f,4.5f);glazed(6.6f,6.f,6.8f,13.f);glazed(6.6f,14.5f,6.8f,18.f);
 for(float y:{4.5f,13.f})wall(m,6.6f,y,6.8f,y+1.5f,floor+2.25f,roof,3);
 wall(m,1,18.f,6.8f,18.2f,floor,roof,3);
 for(float y:{4.5f,6.f,13.f,14.5f})steelColumn(m,6.7f,y,floor,roof,false);
 // Clock-in: the time clock, the card rack and the shift board.
 terminal(m,{4.5f,2.6f},"FREIGHT / CLOCK-IN","EMPLOYEE TRANSIT / USE PERSONNEL ROUTE.","CARGO LIFTS: FOLLOW CLEARANCE MARKINGS.");
 m.terminals.back().yaw=kPi*.5f;
 m.fixtures.push_back({8,{6.45f,2.7f},1.1f,.67f,.2f,.91f,-kPi*.5f,false});
 // Lockers along the west wall and a changing bench.
 for(float y:{6.5f,7.5f,8.5f,9.5f,10.5f,11.5f})m.fixtures.push_back({13,{1.27f,y},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
 bench(m,3.6f,9.f,floor);
 // Tool crib behind a mesh cage: shelving and the issue hatch.
 for(float y:{14.6f,16.6f})m.fixtures.push_back({7,{1.27f,y},0,1.6f,.5f,1.8f,kPi*.5f,true});
 m.structures.push_back({1.f,14.f,2.8f,14.06f,floor,floor+2.2f,true,2});m.structures.push_back({4.2f,14.f,6.6f,14.06f,floor,floor+2.2f,true,2});
 wall(m,2.8f,14.f,4.2f,14.06f,floor+2.f,floor+2.2f,2);
 // --- Freight hall ------------------------------------------------------------------
 // East trailer dock: shutter in its frame, leveller plate, bumpers.
 wall(m,22.8f,5.6f,23.f,10.4f,floor,roof,7);
 for(float z=floor+.2f;z<roof-.3f;z+=.3f)wall(m,22.74f,5.8f,22.8f,10.2f,z,z+.05f,2);
 for(float y:{5.45f,10.4f})wall(m,22.6f,y,23.f,y+.15f,floor,roof,12);
 wall(m,20.6f,6.f,22.6f,10.f,floor,floor+.03f,8);
 floorLine(m,20.6f,5.85f,22.6f,6.f,floor+.03f,13);floorLine(m,20.6f,10.f,22.6f,10.15f,floor+.03f,13);
 for(float y:{6.3f,9.7f})wall(m,22.45f,y-.2f,22.6f,y+.2f,floor+.4f,floor+.9f,8);
 wallSign(m,22.795f,8.f,roof-.55f,"DOCK 01","TRAILER BAY / SHUTTER LOCKED OUT",1.8f,-kPi*.5f);
 // Staged loads on the apron, a hand cart and a pallet jack lane.
 cargo(m,18.5f,6.5f,floor);cargo(m,18.5f,8.f,floor,false);cargo(m,17.f,6.5f,floor);
 handCart(m,15.5f,9.5f,floor);
 // Dock office: glazed on two sides, the loading schedule on its desk.
 glazed(16.5f,13.f,23.f,13.2f);glazed(16.5f,13.2f,16.7f,15.5f);
 wall(m,16.5f,16.9f,16.7f,18.2f,floor,roof,3);wall(m,16.5f,15.5f,16.7f,16.9f,floor+2.25f,roof,3);
 wall(m,16.5f,18.f,23.f,18.2f,floor,roof,3);
 recordsDesk(m,19.5f,15.8f,floor);
 terminal(m,{21.2f,14.2f},"LOADING SCHEDULE","SHIFT 02 / RECEIVING 01-03 ACTIVE.","RESTRICTED CARGO: MANIFEST AUTHORIZATION.");
 m.fixtures.push_back({7,{22.6f,16.6f},0,1.4f,.45f,1.8f,-kPi*.5f,true});
 // Forklift charging bay against the north wall.
 forklift(m,10.f,3.f,floor);
 wall(m,11.6f,1.f,12.6f,1.3f,floor+.8f,floor+1.8f,12);
 // Lanes and the freight gate south: a portal frame with hazard posts.
 for(float x:{8.2f,15.8f})floorLine(m,x,4.f,x+.12f,24.f,floor,14);
 for(float y=5.f;y<23.f;y+=2.f)floorLine(m,11.9f,y,12.1f,y+1.f,floor,14);
 for(float x:{7.4f,16.6f}){steelColumn(m,x,21.5f,floor,roof,true);wall(m,x-.2f,21.3f,x+.2f,21.7f,floor,floor+1.2f,13);}
 wall(m,7.2f,21.35f,16.8f,21.65f,roof-.5f,roof,12);
 wallSign(m,12.f,21.34f,roof-.48f,"LIFT MACHINERY","FREIGHT GATE / HARD HATS",2.4f,kPi);
 // The south corners either side of the gate: outbound staging west, the
 // empties and the spill kit east.
 for(float x:{2.2f,4.4f})m.fixtures.push_back({7,{x,18.45f},0,1.6f,.5f,1.8f,0,true});
 cargo(m,5.8f,20.4f,floor);cargo(m,4.3f,21.2f,floor,false,.3f);cargo(m,1.9f,22.6f,floor);
 floorLine(m,1.2f,19.2f,6.6f,19.32f,floor,13);
 cargo(m,18.f,20.f,floor,false);cargo(m,19.5f,20.f,floor,false,.15f);cargo(m,21.6f,21.5f,floor);
 m.fixtures.push_back({7,{21.8f,18.45f},0,1.4f,.45f,1.8f,0,true});
 m.lights.push_back(lamp({3.8f,21.f},roof-.15f,.9f,5.f,3800,.1f));m.lights.push_back(lamp({20.f,21.f},roof-.15f,.9f,5.f,3800,.1f));
// Services: the low roof carries ducts and trays on hangers.
 servicePipe(m,{1.2f,19.5f},{22.8f,19.5f},roof-.35f,.12f,4);
 servicePipe(m,{8.6f,1.2f},{8.6f,22.8f},roof-.3f,.08f,3);
 cableTray(m,{15.2f,1.2f},{15.2f,12.5f},roof-.4f);
 // --- Light ---------------------------------------------------------------
 for(Vec2 l:{Vec2{3.5f,5.f},Vec2{3.5f,11.f},Vec2{3.5f,16.f}})m.lights.push_back(lamp(l,roof-.15f,1.f,5.f,4300,.15f));
 for(Vec2 l:{Vec2{10.f,6.f},Vec2{14.5f,6.f},Vec2{10.f,12.f},Vec2{14.5f,12.f},Vec2{12.f,18.f},Vec2{19.f,8.f}})m.lights.push_back(lamp(l,roof-.15f,1.2f,6.f,4600,.15f));
 m.lights.push_back(lamp({19.5f,15.8f},roof-.15f,1.f,4.f,3200));
 m.lights.push_back(lamp({12.f,21.2f},roof-.6f,1.1f,5.f,2200,0));
 // --- Supplies ----------------------------------------------------------------
 m.pickupSpawns={{{11.f,5.7f},PickupKind::Ammo},{{3.f,15.5f},PickupKind::Health}};
 m.clutterSpawns={{3,{2.f,16.f}},{1,{18.f,15.f}},{2,{4.f,11.f}}};
 zone(m,"FREIGHT ACCESS / PERSONNEL",1,1,6.6f,18);
 zone(m,"FREIGHT ACCESS / DOCK OFFICE",16.7f,13.2f,23,18);
 freightFinish(m,10);
 titleCard(m,"FREIGHT DISTRICT");
 return p;
}

// LIFT MACHINERY. The winding house of the main freight lifts. From the
// north landing a fenced walkway rings a thirteen-metre pit: both main
// shafts stand dead in it, car A stalled halfway on its ropes, car B fallen
// to the bottom. The winding engines and their drums sit on the headframe
// overhead; the motor-generator sets and control cubicles line the east
// walk. The auxiliary brake console is on the west walk, and the route goes
// on south to the auxiliary lift.
std::shared_ptr<const Map> makeLiftMachinery(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[11];m.definition=CampaignChunks[11];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.36f;
 const float base=-25,walk=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,true));m.openNorth=true;m.openSouth=true;
 deck(m,walk,1,0,23,4);deck(m,walk,1,4,6,24);deck(m,walk,18,4,23,24);deck(m,walk,6,20,18,24);
 auto& w=m.layers[1];
 // Guard rails round the pit.
 guardRail(m,6.f,3.95f,18.f,4.f,walk);guardRail(m,6.f,20.f,18.f,20.05f,walk);
 guardRail(m,5.95f,4.f,6.f,20.f,walk);guardRail(m,18.f,4.f,18.05f,20.f,walk);
 deckSupports(m,w,base,6.f);
 // --- The shafts ------------------------------------------------------------------
 // Each shaft: four corner guides from the pit floor to the headframe, a
 // crosshead at the top, and the winding drum on it.
 for(Vec2 c:{Vec2{9.f,12.f},Vec2{15.f,12.f}}){
 for(float dx:{-1.6f,1.6f})for(float dy:{-3.2f,3.2f})steelColumn(m,c.x+dx,c.y+dy,base,roof-1.6f,true);
 for(float dy:{-3.2f,3.2f})wall(m,c.x-1.8f,c.y+dy-.15f,c.x+1.8f,c.y+dy+.15f,roof-1.6f,roof-1.25f,12);
 wall(m,c.x-1.8f,c.y-3.35f,c.x-1.5f,c.y+3.35f,roof-1.6f,roof-1.25f,12);wall(m,c.x+1.5f,c.y-3.35f,c.x+1.8f,c.y+3.35f,roof-1.6f,roof-1.25f,12);
 m.pipes.push_back({{c.x-1.3f,c.y},{c.x+1.3f,c.y},roof-.7f,.55f,roof-.7f,8});
 for(float x:{c.x-1.25f,c.x+1.25f})m.pipes.push_back({{x,c.y},{x+.08f,c.y},roof-.7f,.62f,roof-.7f,6});
 m.structures.push_back({c.x-1.4f,c.y-.7f,c.x+1.4f,c.y+.7f,roof-1.6f,roof-.1f,false,6});
 }
 // Car A hangs stalled halfway down shaft A on its ropes.
 const float carA=-18.5f;
 wall(m,7.6f,9.f,10.4f,15.f,carA,carA+.2f,11);
 for(float x:{7.6f,10.3f})wall(m,x,9.f,x+.1f,15.f,carA+.2f,carA+2.4f,11);
 wall(m,7.6f,9.f,10.4f,9.1f,carA+.2f,carA+2.4f,11);wall(m,7.6f,9.f,10.4f,15.f,carA+2.4f,carA+2.55f,12);
 for(float y:{11.5f,12.5f})m.pipes.push_back({{9.f,y},{9.f,y},carA+2.55f,.03f,roof-1.6f,6});
 // Car B lies wrecked at the pit bottom, its ropes coiled on top.
 wall(m,13.5f,9.5f,16.6f,14.5f,base,base+1.4f,11);wall(m,13.4f,9.4f,16.7f,14.6f,base+1.4f,base+1.6f,12);
 for(float y:{11.f,13.f})m.pipes.push_back({{14.f,y},{16.f,y},base+1.75f,.12f,base+1.75f,6});
 board(m,12.f,6.5f,base+1.2f,"SHAFTS A / B","OUT OF SERVICE / NO ENTRY TO PIT",2.6f);
 // Counterweights on their own guides along the pit's north and south walls.
 for(float x:{9.f,15.f})for(float y:{5.2f,18.8f}){wall(m,x-.5f,y-.3f,x+.5f,y+.3f,base,base+2.f,8);m.pipes.push_back({{x,y},{x,y},base+2.f,.03f,roof-.05f,6});}
 // --- East walk: the winding plant ---------------------------------------------------
 for(float y:{6.5f,11.5f}){m.fixtures.push_back({6,{20.5f,y},13,3.4f*.85f,.85f,1.8f*.85f,kPi*.5f,true});}
 for(float y:{13.f,14.f,17.5f,18.5f})m.fixtures.push_back({13,{22.75f,y},13,.9066f,.4956f,2.2f,kPi*.5f,true});
 cableTray(m,{22.6f,5.f},{22.6f,19.f},walk+2.7f);
 // --- West walk: the auxiliary brake ------------------------------------------------
 terminal(m,{3.5f,17.5f},"AUXILIARY / BRAKE RELEASE","MAIN SHAFTS OUT OF SERVICE.","01: RELEASE AUXILIARY BRAKE.",13,stateId("freight_brake"));
 m.props.push_back({1,{3.5f,12},1.4f,1.f,0,{.45f,.65f},13});
 board(m,3.5f,15.f,walk+1.5f,"AUX BRAKE","RELEASE / THEN LOCAL CONTROL NEXT BAY",2.f);
 // Mains out of the side walls along the walks.
 pipeRoute(m,{{1.05f,1.5f,walk+3.3f},{2.f,1.5f,walk+3.3f},{2.f,22.5f,walk+3.3f},{1.05f,22.5f,walk+3.3f}},.13f,1,roof);
 pipeRoute(m,{{1.05f,2.f,walk+2.5f},{2.4f,2.f,walk+2.5f},{2.4f,22.f,walk+2.5f},{1.05f,22.f,walk+2.5f}},.13f,3,roof);
 wallBays(m,base,roof,4.f,false,false,true,true,12.f);
 // --- Light ---------------------------------------------------------------
 for(float y:{2.f,6.f,10.f,14.f,18.f,22.f}){m.lights.push_back(lamp({1.02f,y},walk+2.3f,1.2f,6.f,4200,.1f,WorldLightMount::Wall,0));m.lights.push_back(lamp({22.98f,y},walk+2.3f,1.2f,6.f,4200,.1f,WorldLightMount::Wall,kPi));}
 for(float x:{6.f,12.f,18.f})m.lights.push_back(lamp({x,2.f},walk-.4f,1.f,5.f,3800,.1f));
 // The pit is lit from below in amber, the headframe from above.
 for(Vec2 l:{Vec2{12.f,7.f},Vec2{12.f,17.f}})m.lights.push_back(lamp(l,roof-.15f,1.6f,14.f,4600,.15f));
 for(float y:{8.f,16.f})for(float x:{5.5f,18.5f})m.lights.push_back(lamp({x,y},walk-.45f,1.1f,9.f,2000,0));
 m.pickupSpawns={{{20.5f,2.5f},PickupKind::Ammo,walk}};
 zone(m,"LIFT MACHINERY / WINDING PLANT",18,4,23,20,-12.5f,1000);
 freightFinish(m,11);
 return p;
}

// AUXILIARY FREIGHT LIFT. The one working cage. The upper landing runs
// along the north: three consoles, worked in order (local, call, release),
// face the shaft across a hazard-edged apron. The cage rides an open steel
// shaft between guide columns under its hoist; the counterweight runs on
// its own rails beside it. Seven metres down, the lower landing leads to
// the receiving hall.
std::shared_ptr<const Map> makeAuxiliaryLift(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[12];m.definition=CampaignChunks[12];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.38f;
 const float base=-25,upper=-12,lower=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));m.openNorth=true;
 // Exit to Receiving Overlook from the lower landing.
 rectangle(m.layers[0],10,23,14,24,'.');m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,lower-base});
 wall(m,10,23,14,24,lower+2.5f,roof,3);wall(m,10,23,14,24,base,lower-.22f,15);
 // Upper landing and its threshold onto the cage; the lower landing.
 deck(m,upper,1,0,23,9);m.stairs.push_back({10,8,14,10,-12.12f,-12,1,true,true});
 deck(m,lower,8,15,16,24);m.stairs.push_back({10,14,14,16,-19,-19,1,true,true});
 deckSupports(m,m.layers[1],base,6.f);
 // The control line: three consoles in order, each under its own sign.
 terminal(m,{6.5f,6},"AUXILIARY / LOCAL CONTROL","02: SELECT LOCAL AFTER BRAKE RELEASE.","INTERLOCK: BRAKE RELEASE REQUIRED.",13,stateId("freight_local"),stateId("freight_brake"));
 terminal(m,{12,6},"AUXILIARY / CALL PLATFORM","03: CALL PLATFORM TO BOARDING LEVEL.","INTERLOCK: LOCAL CONTROL REQUIRED.",13,stateId("freight_call"),stateId("freight_local"));
 terminal(m,{18,6},"AUXILIARY / MANUAL DOOR RELEASE","04: RELEASE THE CAGE DOOR AFTER ARRIVAL.","BOARD THE PLATFORM TO DESCEND.",13,stateId("freight_release"),stateId("freight_platform_arrived"));
 for(auto [x,title]:{std::pair{6.5f,"02 LOCAL"},{12.f,"03 CALL"},{18.f,"04 RELEASE"}})board(m,x,7.4f,upper+1.6f,title,"",1.6f);
 floorLine(m,8.5f,8.6f,15.5f,8.75f,upper,13);
 // The cage, its guides, hoist and counterweight.
 m.cargoLift={9.5f,10,14.5f,15,-19,-12,1.f,stateId("freight_call"),stateId("freight_release"),stateId("freight_lift_mm"),stateId("freight_lift_down"),stateId("freight_platform_arrived"),stateId("freight_descended")};
 {Door gate{9.5f,14.5f,10.f,0,false,false,false,13.f};gate.requireState=stateId("freight_release");m.doors.insert(m.doors.begin(),gate);}
 m.doors.back().requireState=stateId("freight_descended");
 for(float x:{9.25f,14.7f})wall(m,x,9,x+.12f,16,base,0,2);
 for(float x:{9.f,15.f})for(float y:{9.6f,15.4f})steelColumn(m,x,y,base,roof-.6f,false);
 wall(m,8.8f,9.4f,15.2f,15.6f,roof-.9f,roof-.6f,12);
 m.pipes.push_back({{10.4f,12.5f},{13.6f,12.5f},roof-.2f,.55f,roof-.2f,8});
 for(float x:{10.3f,13.6f})m.pipes.push_back({{x,12.5f},{x+.1f,12.5f},roof-.2f,.62f,roof-.2f,6});
 for(float x:{16.f,16.8f})wall(m,x,11.6f,x+.1f,13.4f,base,roof-.9f,8);
 wall(m,15.9f,11.9f,16.9f,13.1f,-16.f,-14.6f,8);
 // Beacons either side of the gate.
 m.lights.push_back(lamp({9.25f,10.8f},-10.f,1.f,4.f,1800,-.1f,WorldLightMount::Wall,kPi));m.lights.push_back(lamp({14.82f,10.8f},-10.f,1.f,4.f,1800,-.1f,WorldLightMount::Wall,0));
 // Something moves on the far gantry while the player works the controls.
 machineryClock(m,"lift_distant_motion",12000);
 actor(m,"lift_distant_motion",ActorVisual::Huntsman,1.5f,{{0,{6,18},-16,0},{2000,{6,18},-16,0},{3600,{17,18},-16,0},{8000,{17,18},-16,kPi},{10400,{6,18},-16,kPi},{12000,{6,18},-16,0}});
 deck(m,-16,6,17,18,19);cargo(m,17,18,-16);
 // Upper landing dressing: the hoist motor and the control cubicle.
 m.fixtures.push_back({6,{20.2f,2.2f},13,2.38f,.7f,1.26f,0,true});
 // The lockers are wall-mounted cabinets: their backs must meet the west
 // wall (standing free at the open north edge showed their hollow sides).
 for(float y:{2.6f,3.6f})m.fixtures.push_back({13,{y>3.f?1.48f:1.27f,y},13,.9066f,.4956f,2.2f,-kPi*.5f,true});
 cableTray(m,{1.6f,1.2f},{22.f,1.2f},upper+2.6f);
 // Lower landing: buffers, a hazard apron and the transfer to receiving.
 for(float x:{10.f,14.f})wall(m,x-.2f,15.f,x+.2f,15.4f,lower,lower+.5f,8);
 floorLine(m,9.5f,16.f,14.5f,16.15f,lower,13);
 wallBays(m,base,roof,4.f,false,true,true,true,12.f);
 // --- Light ---------------------------------------------------------------
 for(float y:{2.f,6.f}){m.lights.push_back(lamp({1.02f,y},upper+2.2f,1.f,6.f,4200,.1f,WorldLightMount::Wall,0));m.lights.push_back(lamp({22.98f,y},upper+2.2f,1.f,6.f,4200,.1f,WorldLightMount::Wall,kPi));}
 m.lights.push_back(lamp({12.f,12.5f},roof-1.15f,1.4f,12.f,4800,.1f));
 for(float x:{9.f,15.f})m.lights.push_back(lamp({x,22.98f},lower+2.2f,1.1f,6.f,3800,.1f,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({12.f,18.f},-16.37f,.9f,5.f,3800,.1f));
 freightFinish(m,12);
 return p;
}

// RECEIVING OVERLOOK. The first sight of the receiving floor. From the
// lift's lower landing a mezzanine runs along the north wall, with the
// receiving foreman's glazed booth on it and a balcony that juts out over
// the floor. Below: numbered unloading bays, two shipping containers, the
// gantry crane on its runway, trailer docks in the south wall and the
// receiving office. The floor opens east into the receiving lanes. The
// worker on the floor is the last of the shift.
std::shared_ptr<const Map> makeReceivingOverlook(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[13];m.definition=CampaignChunks[13];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.38f;
 const float base=-25,mezz=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));m.openEast=true;rectangle(m.layers[0],23,1,24,23,'.');
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,mezz-base});
 wall(m,10,0,14,1,base,mezz-.22f,15);wall(m,10,0,14,.85f,mezz+2.5f,roof,3);
 const float sightLine=0;(void)sightLine;
 // --- The worker encounter (scripted; staged on the open floor) -----------------------
 m.definition.playerStart={7.6f,9.5f};m.definition.spawnYaw=.69f;m.definition.spawnPitch=-62.f;
 m.timedSequences.push_back({stateId("receiving_worker_encounter_ms"),stateId("receiving_worker_encounter_complete"),6.4f,8,8.5f,10,-19.5f,-18.6f,18000,12500,0,0});
 m.timedSequences.back().sightActor=0;
 actor(m,"receiving_worker_encounter_ms",ActorVisual::Worker,1.7f,{
 {0,{16.2f,16},-25,0,1,0},{5200,{16.2f,16},-25,0,0,0},
 {6000,{16.2f,16},-25,0,0,0,1},
 {6200,{16.2f,16},-25,0,5,0,1},{6500,{16.2f,16},-25,0,5,1,1},
 {6501,{16.2f,16},-25,0,2,0,1},{7040,{16.2f,16},-25,0,2,1,1},
 {7100,{16.2f,16},-25,0,1,0,1},{8200,{16.2f,17.2f},-25,0,1,2,1},
 {8201,{16.2f,17.2f},-25,0,0,0,1},{9000,{16.2f,17.2f},-25,0,3,0,2},
 {9450,{16.4f,17.5f},-25,0,3,1,2},{9451,{16.4f,17.5f},-25,0,2,0,2},
 {9900,{16.4f,17.5f},-25,0,2,1,2},{9901,{16.4f,17.5f},-25,0,3,0,2},
 {10400,{17,18.1f},-25,0,3,1,2},{10401,{17,18.1f},-25,0,2,0,1},
 {10850,{17,18.1f},-25,0,2,1,1},{10851,{17,18.1f},-25,0,3,0,2},
 {11300,{17.3f,18.3f},-25,0,3,1,2},{11301,{17.3f,18.3f},-25,0,4,0},
 {12500,{17.8f,19},-25,0,4,1},{18000,{17.8f,19},-25,0,4,1}},false,true);
 auto& worker=m.actorTracks.back();worker.idleUntilMs=3000;worker.approachUntilMs=5200;
 worker.idleKeys={
 {0,{14.6f,15},-25,.785f,1,0},{2600,{15.6f,16},-25,.785f,1,4},
 {2601,{15.6f,16},-25,.785f,0,0},{3300,{15.6f,16},-25,-1.418f,1,0},
 {5900,{14.3f,16.2f},-25,-1.418f,1,4},{5901,{14.3f,16.2f},-25,-1.418f,0,0},
 {7000,{14.3f,16.2f},-25,2.897f,1,0},{9600,{14.6f,15},-25,2.897f,1,4},
 {9601,{14.6f,15},-25,2.897f,0,0},{10000,{14.6f,15},-25,.785f,0,0}};
 actor(m,"receiving_worker_encounter_ms",ActorVisual::Wasp,1.15f,{
 {0,{23,13},-23.4f,0},{5600,{23,13},-23.4f,0},{6200,{16,15},-23.7f,0},
 {6900,{16.2f,15.4f},-23.8f,0},{7040,{14.6f,16.4f},-24.9f,0,4},
 {9300,{14.6f,16.4f},-24.9f,0,4},{10100,{16.8f,17.5f},-23.7f,0},
 {10700,{16.8f,17.6f},-23.8f,0},{10900,{15.6f,19},-24.96f,0,4},{18000,{15.6f,19},-24.96f,0,4}});
 actor(m,"receiving_worker_encounter_ms",ActorVisual::Wasp,1.15f,{
 {0,{23,23},-23,0},{8800,{23,23},-23,0},{9300,{17.2f,17.5f},-23.7f,0},
 {9800,{17.3f,17.7f},-23.8f,0},{10500,{17.8f,18.3f},-23.9f,0},
 {11300,{17.7f,18.6f},-24.2f,0},{13000,{17.8f,19},-24.5f,0},
 {15500,{22,22},-22.7f,0},{18000,{25,23},-22.7f,0}});
 cue(m,"receiving_worker_encounter_ms",6100,Sound::WaspAttack,{16,15},"WORKER: GET BACK!",1.2f);
 cue(m,"receiving_worker_encounter_ms",6200,Sound::WorkerDying,{16.2f,16},"",1.5f,-23.81f);
 cue(m,"receiving_worker_encounter_ms",6900,Sound::PunchHit,{16.2f,16});
 cue(m,"receiving_worker_encounter_ms",7040,Sound::Metal1,{14.6f,16.4f},"",1,-24.96f);
 cue(m,"receiving_worker_encounter_ms",9150,Sound::Hurt,{16.2f,17.5f},"WORKER: NO! GET OFF ME!",0);
 cue(m,"receiving_worker_encounter_ms",9750,Sound::PunchHit,{17.2f,17.5f});
 cue(m,"receiving_worker_encounter_ms",10100,Sound::WaspAttack,{16.8f,17.5f});
 cue(m,"receiving_worker_encounter_ms",10700,Sound::PunchHit,{16.8f,17.6f});
 cue(m,"receiving_worker_encounter_ms",10900,Sound::WaspDeath,{15.6f,19});
 cue(m,"receiving_worker_encounter_ms",11400,Sound::Hurt,{17.8f,19},"WORKER: SOMEBODY...",0);
 // --- Mezzanine, booth and balcony --------------------------------------------------
 deck(m,mezz,1,0,23,4);deck(m,mezz,1,4,8,10);
 m.stairs.push_back({3,9,6,18,-25,-19,32,true,false});
 deckSupports(m,m.layers[1],base,6.f);
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,mezz,mezz+.95f,3);wall(m,x1,y1,x2,y2,mezz+.95f,mezz+2.3f,17);wall(m,x1,y1,x2,y2,mezz+2.3f,mezz+2.6f,3);};
 // The foreman's booth at the east end of the mezzanine, glazed to the floor.
 glazed(17.f,3.88f,23.f,4.f);glazed(17.f,1.f,17.12f,2.f);wall(m,17.f,3.2f,17.12f,3.88f,mezz,mezz+2.6f,3);wall(m,17.f,2.f,17.12f,3.2f,mezz+2.2f,mezz+2.6f,3);
 wall(m,17.f,1.f,23.f,4.f,mezz+2.6f,mezz+2.75f,2);
 recordsDesk(m,20.f,2.2f,mezz);
 terminal(m,{21.8f,3.2f},"RECEIVING / FOREMAN","UNLOADING 01 / SHIFT 02 / NO HANDOVER.","OVERLOOK BALCONY: KEEP RADIO ON.",6);
 for(float x:{18.f,19.f})m.fixtures.push_back({13,{x,1.27f},6,.9066f,.4956f,2.2f,kPi,true});
 // --- The floor -----------------------------------------------------------------------
 // Three numbered unloading bays painted on the floor.
 for(int i=0;i<3;++i){float x=8.f+i*5.f;
 for(float xx:{x,x+4.f})floorLine(m,xx,6.f,xx+.12f,13.f,base,14);
 floorLine(m,x,13.f,x+4.12f,13.12f,base,14);
 }
 // Two containers in the bays, one standing open and half unloaded.
 container(m,8.5f,6.5f,base,true);container(m,18.5f,6.5f,base,false);
 cargo(m,14.f,9.f,base);cargo(m,14.f,10.5f,base,false);
 // Gantry crane: runway beams on the side walls, the bridge and its hoist.
 for(float x:{1.f,22.6f})wall(m,x,4.f,x+.4f,23.f,-6.f,-5.6f,12);
 for(float x:{1.f,22.6f})for(float y=6.f;y<23;y+=5.f)wall(m,x,y-.2f,x+.4f,y+.2f,-7.f,-6.f,15);
 for(float y:{11.f,12.f})wall(m,1.4f,y,22.6f,y+.3f,-5.6f,-5.f,18);
 wall(m,11.f,10.8f,13.f,12.5f,-6.f,-5.6f,12);
 for(float y:{11.5f,11.8f})m.pipes.push_back({{12.f,y},{12.f,y},-6.f,.02f,-12.f,6});
 wall(m,11.8f,11.45f,12.2f,11.85f,-12.4f,-12.f,8);
 // Receiving office, south-west.
 office(m,4.5f,20.f,base,"RECEIVING / WORKER NOTE","THEY ARE STILL DOWN HERE. KEEP THE RADIO ON.","WAREHOUSE INTAKE: FOLLOW THE EAST LANES.");
 terminal(m,{2.5f,21.5f},"WORKER / HANDWRITTEN ROUTE","I HAVE BEEN HERE SINCE SHIFT CHANGE. TWO DAYS.","WEST SERVICE STAIRS / RADIO CALLS: STILL NO ANSWER.");
 // Trailer docks in the south wall, a forklift between runs.
 for(float x:{8.5f,12.5f}){
 wall(m,x,22.8f,x+3.f,23.f,base,base+3.4f,7);
 for(float z=base+.2f;z<base+3.2f;z+=.3f)wall(m,x+.15f,22.74f,x+2.85f,22.8f,z,z+.05f,2);
 for(float px:{x-.15f,x+3.f})wall(m,px,22.6f,px+.15f,23.f,base,base+3.6f,12);
 wall(m,x-.15f,22.55f,x+3.15f,23.f,base+3.3f,base+3.6f,12);
 wall(m,x+.2f,20.8f,x+2.8f,22.6f,base,base+.03f,8);
 floorLine(m,x+.2f,20.65f,x+2.8f,20.8f,base+.03f,13);
 for(float bx:{x+.35f,x+2.45f})wall(m,bx,22.45f,bx+.2f,22.6f,base+.4f,base+.9f,8);
 wallLamp(m,x+1.5f,22.99f,base+4.f,-kPi*.5f,1.3f,7.f);
 }
 wallSign(m,12.f,22.945f,base+4.6f,"DOCKS 03 / 04","RECEIVING / TRAILERS HELD",2.f,kPi);
 forklift(m,10.f,17.f,base);
 m.clutterSpawns.push_back({1,{5.5f,15.f},base});m.clutterSpawns.push_back({3,{9.f,20.f},base});
 m.pickupSpawns.push_back({{6.f,21.8f},PickupKind::Health,base});
 wallBays(m,base,roof,4.f,false,false,true,false,12.f);
 // --- Light ---------------------------------------------------------------
 for(Vec2 l:{Vec2{6.f,8.f},Vec2{12.f,8.f},Vec2{18.f,8.f},Vec2{6.f,16.f},Vec2{12.f,16.f},Vec2{18.f,16.f}})m.lights.push_back(lamp(l,roof-.15f,3.2f,24.f,4800,.15f));
 for(float x:{4.f,9.f,14.f})m.lights.push_back(lamp({x,1.02f},mezz+2.2f,1.f,6.f,4000,.1f,WorldLightMount::Wall,kPi*.5f));
 m.lights.push_back(lamp({20.f,2.5f},mezz+2.45f,1.f,4.f,3200));
 for(float y:{4.5f,8.5f})m.lights.push_back(lamp({3.f,y},mezz-.4f,1.f,5.f,3800,.1f));
 m.lights.push_back(lamp({16.5f,11.6f},-5.75f,1.8f,14.f,4200,.1f));
 zone(m,"RECEIVING OVERLOOK / FOREMAN",17.1f,1,23,4,-19.5f,1000);
 freightFinish(m,13);
 return p;
}

// RECEIVING LANES. Receiving 02: vans backed in through the two north
// shutters and were unloaded on the apron. Between the lanes stands the
// inspection hold, a caged bay under the overhead crane where sealed loads
// for Energy Research were opened and checked. Staged containers line the
// west side, open to the overlook floor; a painted personnel aisle runs along
// the south bulkhead to the pressure door into Warehouse Intake. The Cleanup
// detail is working the floor.
std::shared_ptr<const Map> makeReceivingLanes(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[14];m.definition=CampaignChunks[14];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.36f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openWest=true;m.openSouth=true;
 rectangle(m.layers[0],0,1,1,23,'.');
 // The Cleanup detail, its field cache, the chain test and the pressure door.
 receivingCleanup(m);
 // The bulkhead closes the hall above the pressure door; the door has a frame.
 wall(m,1,23.4f,23,23.6f,-22.f,roof,15);
 for(float x:{10.3f,13.5f})wall(m,x,23.25f,x+.2f,23.75f,base,-21.6f,12);
 wall(m,10.3f,23.25f,13.7f,23.75f,-22.f,-21.6f,12);
 // --- North shutters and the two vehicle lanes -------------------------------------------
 for(float x:{8.4f,16.4f}){
  wall(m,x,1.f,x+3.2f,1.12f,base,base+4.6f,7);
  for(float z=base+.25f;z<base+4.5f;z+=.3f)wall(m,x+.1f,1.12f,x+3.1f,1.16f,z,z+.05f,2);
  for(float px:{x-.3f,x+3.2f})wall(m,px,1.f,px+.3f,1.4f,base,base+5.f,12);
  wall(m,x-.3f,1.f,x+3.5f,1.4f,base+4.6f,base+5.f,12);
  for(float lx:{x,x+3.2f})floorLine(m,lx-.06f,1.4f,lx+.06f,10.3f,base,14);
  floorLine(m,x,10.3f,x+3.2f,10.45f,base,13);
  wallLamp(m,x+1.6f,1.02f,base+5.5f,kPi*.5f,1.4f,9.f);
 }
 // Lane 2: the last van in, half unloaded, its pallets on the apron behind it.
 m.fixtures.push_back({16,{18.f,5.2f},0,2.05f,4.9f,2.15f,0,true});
 cargo(m,17.3f,9.3f,base);cargo(m,18.8f,9.5f,base,false);handCart(m,15.f,8.6f,base);
 // Lane 1: empty, a load staged where the van would stop.
 cargo(m,9.4f,4.f,base);cargo(m,10.7f,4.2f,base);cargo(m,9.6f,5.4f,base,false);
 // --- Staged containers, west, open to the overlook floor ---------------------------------
 container(m,1.6f,2.4f,base,true);container(m,4.9f,2.4f,base,false);
 // --- The inspection hold -------------------------------------------------------------
 equipmentPad(m,12.6f,11.f,16.6f,15.2f,base);
 // Waist-high mesh on a post-and-header frame: the load stays in view.
 guardRail(m,12.6f,11.f,16.6f,11.06f,base+.1f);guardRail(m,16.54f,11.f,16.6f,15.2f,base+.1f);guardRail(m,12.6f,11.f,12.66f,15.2f,base+.1f);
 guardRail(m,12.6f,15.14f,13.8f,15.2f,base+.1f);guardRail(m,15.4f,15.14f,16.6f,15.2f,base+.1f);
 for(float x:{12.6f,13.7f,15.4f,16.48f})for(float y:{11.f,15.08f})wall(m,x,y,x+.12f,y+.12f,base+.1f,base+2.7f,12);
 for(float y:{11.f,15.08f})wall(m,12.6f,y,16.6f,y+.12f,base+2.58f,base+2.7f,12);
 for(float x:{12.6f,16.48f})wall(m,x,11.f,x+.12f,15.2f,base+2.58f,base+2.7f,12);
 // The sealed load on its pallet, the console that logged it and a sample case.
 m.fixtures.push_back({15,{14.f,12.6f},.1f,1.2f,1.15f,.16f,0,true});
 m.fixtures.push_back({21,{14.f,12.6f},.26f,1.1f,1.05f,1.f,0,true});
 m.fixtures.push_back({22,{15.7f,14.2f},.1f,.78f,.49f,1.3f,kPi,true});
 m.fixtures.push_back({21,{13.4f,14.4f},.1f,.6f,.45f,.4f,.3f,true});
 board(m,14.6f,10.4f,base+2.7f,"INSPECTION HOLD","SEALED LOAD / ENERGY RESEARCH / NO ENTRY",3.2f);
 for(float y=11.3f;y<15.f;y+=.6f)floorLine(m,16.7f,y,17.f,y+.3f,base,13);
 // --- Overhead crane: runways on the north wall and bulkhead, bridge over the hold ---------
 wall(m,1.f,1.f,23.f,1.6f,-13.2f,-12.6f,12);wall(m,1.f,22.8f,23.f,23.4f,-13.2f,-12.6f,12);
 for(float x=3.f;x<23;x+=4.f){wall(m,x-.12f,1.f,x+.12f,1.6f,-14.2f,-13.2f,12);wall(m,x-.12f,22.8f,x+.12f,23.4f,-14.2f,-13.2f,12);}
 wall(m,14.2f,1.6f,15.f,22.8f,-12.6f,-11.8f,18);
 for(float y:{1.6f,21.8f})wall(m,13.9f,y,15.3f,y+1.f,-12.9f,-11.6f,12);
 wall(m,13.9f,12.4f,15.3f,13.6f,-13.4f,-12.6f,12);
 m.pipes.push_back({{14.2f,13.f},{14.95f,13.f},-13.15f,.18f,-13.15f,6});
 m.pipes.push_back({{14.6f,13.f},{14.6f,13.f},-21.f,.025f,-13.4f,6});
 wall(m,14.45f,12.85f,14.75f,13.15f,-21.4f,-21.f,8);
 m.pipes.push_back({{14.6f,13.f},{14.6f,13.f},-21.75f,.05f,-21.4f,6});
 // --- Personnel aisle along the bulkhead --------------------------------------------------
 floorLine(m,1.f,17.9f,6.f,18.02f,base,14);floorLine(m,9.1f,17.9f,23.f,18.02f,base,14);
 floorLine(m,1.f,22.95f,10.3f,23.07f,base,14);floorLine(m,13.7f,22.95f,23.f,23.07f,base,14);
 for(float x=2.f;x<22;x+=3.f)floorLine(m,x,20.4f,x+1.2f,20.52f,base,14);
 floorLine(m,10.5f,22.4f,13.5f,22.95f,base,13);
 wallSign(m,4.5f,23.345f,base+2.f,"PERSONNEL ROUTE","PRESSURE DOOR EAST / WAREHOUSE INTAKE",2.6f,kPi);
 // The Cleanup field station against the bulkhead: table, sample cases, floodlight.
 m.fixtures.push_back({19,{19.4f,22.3f},0,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({21,{21.6f,22.2f},0,.8f,.7f,.6f,0,true});
 m.fixtures.push_back({21,{21.6f,22.2f},.6f,.7f,.6f,.45f,.2f,true});
 wall(m,17.57f,21.87f,17.63f,21.93f,base,base+2.2f,8);
 for(float a:{0.f,2.1f,4.2f})wall(m,17.6f+std::sin(a)*.32f-.02f,21.9f+std::cos(a)*.32f-.02f,17.6f+std::sin(a)*.32f+.02f,21.9f+std::cos(a)*.32f+.02f,base,base+.04f,6);
 wall(m,17.42f,21.72f,17.78f,22.08f,base+2.2f,base+2.48f,8);
 m.lights.push_back(lamp({17.6f,21.9f},base+2.05f,1.8f,10.f,5600,0));
 // --- East wall: switchgear and its riser --------------------------------------------------
 for(float y:{13.6f,14.6f})m.fixtures.push_back({13,{22.75f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});
 m.pipes.push_back({{22.92f,14.1f},{22.92f,14.1f},base+2.2f,.04f,base+6.f,1});
 cableTray(m,{22.6f,1.6f},{22.6f,22.6f},base+6.f);
 wallBays(m,base,roof,4.f,false,false,false,true);
 // --- Roof: girders on a column line at the seam, ducts, lamps ------------------------------
 for(float y:{4.f,10.f,16.f,22.f}){wall(m,.5f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);steelColumn(m,.65f,y,base,roof-.8f,false);}
 servicePipe(m,{1.2f,19.6f},{22.8f,19.6f},-9.f,.38f,5);
 servicePipe(m,{1.2f,7.f},{22.8f,7.f},-8.f,.12f,3);
 for(Vec2 l:{Vec2{6.f,7.f},Vec2{12.f,7.f},Vec2{19.f,7.f},Vec2{6.f,15.f},Vec2{10.5f,15.f},Vec2{19.f,15.f}})m.lights.push_back(lamp(l,-9.f,3.f,22.f,4200,.1f));
 m.lights.push_back(lamp({14.6f,13.9f},-13.6f,2.2f,14.f,4600,.1f));
 for(float y:{6.f,10.f,18.f})wallLamp(m,22.98f,y,base+4.5f,kPi,1.3f,8.f);
 for(float x:{5.f,18.5f})wallLamp(m,x,23.38f,base+3.6f,-kPi*.5f,1.2f,7.f);
 // --- Supplies ----------------------------------------------------------------
 m.pickupSpawns.push_back({{15.6f,6.5f},PickupKind::Ammo,base});
 m.clutterSpawns.push_back({1,{20.5f,8.f},base});m.clutterSpawns.push_back({3,{3.f,19.5f},base});
 zone(m,"RECEIVING LANES / INSPECTION HOLD",12.6f,11.f,16.6f,15.2f);
 freightFinish(m,14);
 return p;
}

// WAREHOUSE INTAKE. Receiving 03, where checked freight was booked into the
// warehouse. Loads came through the pressure door from the lanes, were
// weighed on the scale plate and labelled at the intake office, staged in
// the numbered bays, then run down the roller bed to the intake hoist and
// taken through the intake shutter. The shutter's track is damaged: it lifts
// only far enough to crouch through.
std::shared_ptr<const Map> makeWarehouseIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[15];m.definition=CampaignChunks[15];m.definition.musicCue=MusicCue::Freight;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));m.openNorth=true;
 // The damaged intake shutter into the warehouse.
 rectangle(m.layers[0],10,23,14,24,'.');
 m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,0});m.doors.back().maxOpen=.4f;
 wall(m,10,23,14,24,base+2.5f,roof,3);
 intakeBay(m);m.definition.ambient=.37f;
 wallSign(m,8.2f,22.805f,base+2.25f,"SHUTTER JAMMED","40% TRAVEL / DUCK UNDER",1.8f,kPi);
 for(float x=10.6f;x<13.4f;x+=.5f)floorLine(m,x,22.1f,x+.25f,22.6f,base,13);
 // --- Intake office, west: glazed onto the floor ------------------------------------------
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,base,base+.95f,3);wall(m,x1,y1,x2,y2,base+.95f,base+2.3f,17);wall(m,x1,y1,x2,y2,base+2.3f,base+2.7f,3);};
 wall(m,1,7.5f,6,7.7f,base,base+2.7f,3);wall(m,1,13.3f,6,13.5f,base,base+2.7f,3);
 glazed(5.8f,7.7f,6.f,11.8f);wall(m,5.8f,13.f,6.f,13.3f,base,base+2.7f,3);wall(m,5.8f,11.8f,6.f,13.f,base+2.3f,base+2.7f,3);
 wall(m,1,7.5f,6,13.5f,base+2.7f,base+2.85f,2);
 m.fixtures.push_back({24,{3.f,8.15f},0,1.4f,.7f,.75f,0,true});
 m.fixtures.push_back({26,{3.f,9.05f},0,.55f,.55f,.95f,0,true});
 m.fixtures.push_back({27,{2.8f,8.f},.75f,.42f,.4f,.38f,0,false});
 m.fixtures.push_back({28,{3.f,8.4f},.75f,.45f,.16f,.03f,0,false});
 m.fixtures.push_back({33,{3.55f,8.2f},.75f,.2f,.2f,.1f,0,false});
 m.fixtures.push_back({34,{2.3f,8.35f},.75f,.09f,.09f,.1f,0,false});
 for(float y:{10.2f,11.f})m.fixtures.push_back({32,{1.4f,y},0,.5f,.6f,1.3f,kPi*.5f,true});
 m.fixtures.push_back({31,{3.4f,12.95f},0,.9f,.5f,.75f,kPi,true});
 m.fixtures.push_back({35,{5.4f,12.9f},0,.35f,.35f,1.1f,0,true});
 m.lights.push_back(lamp({3.5f,10.5f},base+2.5f,1.f,5.f,3400));
 terminal(m,{5.f,19.f},"WAREHOUSE / TRACK DAMAGE","INTAKE SHUTTER CANNOT FULLY RETRACT.","CROUCH THROUGH THE CLEARANCE OPENING.");
 // --- Scale plate inside the pressure door, and its readout --------------------------------
 wall(m,11.2f,4.2f,12.8f,5.8f,base,base+.04f,8);
 for(float y:{4.2f,5.68f})floorLine(m,11.2f,y,12.8f,y+.12f,base+.04f,13);
 wall(m,13.3f,4.9f,13.42f,5.02f,base,base+1.2f,2);
 m.fixtures.push_back({8,{13.36f,4.96f},1.2f,.5f,.15f,.45f,0,false});
 floorLine(m,9.1f,1.f,9.2f,8.f,base,14);floorLine(m,14.8f,1.f,14.9f,8.f,base,14);
 // --- Roller bed down to the hoist -------------------------------------------------------
 for(float x:{7.3f,8.82f})wall(m,x,6.f,x+.08f,16.f,base+.62f,base+.76f,12);
 for(float y=6.15f;y<16;y+=.25f)wall(m,7.38f,y-.035f,8.82f,y+.035f,base+.65f,base+.72f,20);
 for(float y:{6.2f,9.4f,12.6f,15.8f})for(float x:{7.3f,8.82f})wall(m,x,y-.04f,x+.08f,y+.04f,base,base+.62f,12);
 // --- Staging bays, east, and the quarantine cages ----------------------------------------
 for(int i=0;i<3;++i){float y=2.f+i*4.f;
  floorLine(m,16.f,y,21.6f,y+.1f,base,14);
  if(i==2)floorLine(m,16.f,y+3.9f,21.6f,y+4.f,base,14);
  floorLine(m,16.f,y,16.1f,y+4.f,base,14);
 }
 cargo(m,17.6f,3.2f,base);cargo(m,19.2f,3.3f,base);cargo(m,17.6f,4.8f,base,false);
 cargo(m,17.7f,7.4f,base);cargo(m,19.3f,8.6f,base);
 m.fixtures.push_back({21,{18.5f,12.f},0,1.4f,1.4f,1.1f,0,true});
 for(float y:{5.6f,6.9f,9.6f,10.9f})m.fixtures.push_back({41,{22.45f,y},0,.8f,1.2f,1.8f,0,true});
 // --- Local control for the shutter --------------------------------------------------------
 equipmentPad(m,18.4f,17.8f,22.5f,21,base);
 m.fixtures.push_back({21,{21.3f,19.5f},.1f,1.68f,1.68f,1.2f,0,true});
 m.fixtures.push_back({22,{19.2f,19.2f},.1f,.75f,.47f,1.25f,kPi,true});
 serviceCeiling(m,18.4f,17.8f,23,21,-21.45f);
 wall(m,22.75f,17.8f,23,21,base,-21.3f,15);
 wallLamp(m,22.74f,19,-22.7f,kPi,1.4f,7.f);
 wallBays(m,base,roof,4.f,false,false,true,true);
 // --- Roof ---------------------------------------------------------------------
 for(float y:{4.f,10.f,16.f,22.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 servicePipe(m,{1.2f,2.4f},{22.8f,2.4f},-9.f,.38f,5);
 cableTray(m,{1.4f,14.5f},{1.4f,22.6f},base+5.f);
 for(Vec2 l:{Vec2{5.f,5.f},Vec2{12.f,7.f},Vec2{19.f,5.f},Vec2{5.f,16.f},Vec2{19.f,13.f}})m.lights.push_back(lamp(l,-9.f,3.f,22.f,4200,.1f));
 for(float y:{6.f,10.f})wallLamp(m,22.98f,y,base+4.5f,kPi,1.3f,8.f);
 // --- Supplies ----------------------------------------------------------------
 m.pickupSpawns.push_back({{4,15},PickupKind::Ammo,base});
 m.clutterSpawns.push_back({2,{16.8f,15.f},base});
 zone(m,"WAREHOUSE INTAKE / OFFICE",1,7.7f,5.8f,13.3f);
 freightFinish(m,15);
 return p;
}

// WAREHOUSE / A-B. The first half of the high-bay warehouse: A and B
// blocks, three rows of pallet racking twelve metres high under a roof
// twenty-seven metres up, either side of the centre lane from the intake
// shutter. Industrial parts and maintenance spares. The operations office in
// the north-east corner ran the floor; dispatch and packing are beside it,
// the break corner by the west wall. The hall goes on east into C block and
// south into E.
std::shared_ptr<const Map> makeWarehouseAB(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[16];m.definition=CampaignChunks[16];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.34f;m.definition.spawnYaw=.85f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openEast=true;m.openSouth=true;
 rectangle(m.layers[0],23,1,24,23,'.');
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,0});
 wall(m,10,0,14,1,base+2.5f,roof,3);
 // --- Racking: rows A1, A2, B1; a cross aisle at mid-length ----------------------------------
 for(float x:{1.3f,7.2f,14.f})for(float y:{3.f,7.02f,13.2f,17.22f})rack(m,x,y,base,4,y==7.02f||y==17.22f);
 rackSign(m,2.68f,3.f,base+3.4f,"A1","INDUSTRIAL PARTS / BAYS 01-08");
 rackSign(m,8.58f,3.f,base+3.4f,"A2","INDUSTRIAL PARTS / BAYS 09-16");
 rackSign(m,15.38f,3.f,base+3.4f,"B1","MAINTENANCE SPARES / BAYS 01-08");
 for(float x:{4.12f,7.08f,10.08f,13.88f})floorLine(m,x-.05f,2.6f,x+.05f,21.6f,base,14);
 for(float x=1.4f;x<16.7f;x+=1.2f)floorLine(m,x,11.5f,x+.6f,12.7f,base,13);
 // --- Operations office, north-east --------------------------------------------------------
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,base,base+.95f,3);wall(m,x1,y1,x2,y2,base+.95f,base+2.3f,17);wall(m,x1,y1,x2,y2,base+2.3f,base+2.6f,3);};
 glazed(17.6f,1.f,17.8f,5.8f);wall(m,17.6f,5.8f,17.8f,7.f,base+2.3f,base+2.6f,3);wall(m,17.6f,7.f,17.8f,7.8f,base,base+2.6f,3);
 glazed(17.8f,7.6f,22.7f,7.8f);wall(m,22.7f,1.f,22.9f,7.8f,base,base+2.6f,3);
 wall(m,17.6f,1.f,22.9f,7.8f,base+2.6f,base+2.75f,2);
 m.fixtures.push_back({24,{20.2f,1.62f},0,1.4f,.7f,.75f,0,true});
 m.fixtures.push_back({26,{20.2f,2.55f},0,.55f,.55f,.95f,0,true});
 m.fixtures.push_back({27,{20.f,1.5f},.75f,.42f,.4f,.38f,0,false});
 m.fixtures.push_back({28,{20.2f,1.85f},.75f,.45f,.16f,.03f,0,false});
 m.fixtures.push_back({29,{20.75f,1.55f},.75f,.2f,.45f,.42f,0,false});
 m.fixtures.push_back({33,{19.65f,1.7f},.75f,.2f,.2f,.1f,0,false});
 m.fixtures.push_back({34,{20.55f,1.85f},.75f,.09f,.09f,.1f,0,false});
 m.fixtures.push_back({31,{18.6f,1.35f},0,.9f,.5f,.75f,0,true});
 m.fixtures.push_back({30,{18.6f,1.35f},.75f,.5f,.4f,.42f,0,false});
 for(float y:{3.2f,4.f})m.fixtures.push_back({32,{22.35f,y},0,.5f,.6f,1.3f,-kPi*.5f,true});
 m.fixtures.push_back({37,{22.5f,5.6f},0,.9f,.3f,1.8f,-kPi*.5f,true});
 m.fixtures.push_back({35,{18.15f,7.25f},0,.35f,.35f,1.1f,0,true});
 terminal(m,{18.6f,3.6f},"WAREHOUSE / OPERATIONS","DO NOT SEND ANYTHING TO RECEIVING.","THEY ARE NOT ANSWERING.");
 m.lights.push_back(lamp({20.2f,4.4f},base+2.45f,1.f,5.f,3200));
 // --- Dispatch and packing, south of the office -------------------------------------------
 for(float x:{18.6f,21.6f})m.fixtures.push_back({19,{x,11.f},0,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({21,{18.4f,10.95f},.78f,.5f,.4f,.35f,.2f,false});
 m.fixtures.push_back({21,{21.9f,10.9f},.78f,.6f,.45f,.3f,-.15f,false});
 cargo(m,18.4f,14.6f,base);cargo(m,19.8f,14.6f,base);cargo(m,21.2f,14.6f,base,false);
 forklift(m,20.2f,19.3f,base);cargo(m,20.2f,16.9f,base);
 terminal(m,{22.4f,19.3f},"FORKLIFT / SHIFT REPORT","DRIVE MOTOR LEFT RUNNING. BATTERY EMPTY.","OPERATOR DID NOT CLOCK OUT.");
 // --- Break corner, south-west ----------------------------------------------------------------
 bench(m,3.2f,23.3f,base);
 m.fixtures.push_back({19,{5.9f,22.5f},0,1.367f,.724f,.78f,0,true});
 for(float x:{5.4f,6.4f})m.fixtures.push_back({18,{x,23.25f},0,.417f,.525f,.95f,kPi,true});
 m.fixtures.push_back({36,{6.3f,22.45f},.78f,.3f,.3f,.4f,0,false});
 m.fixtures.push_back({34,{5.6f,22.5f},.78f,.09f,.09f,.1f,0,false});
 terminal(m,{1.8f,22.2f},"WAREHOUSE / BREAK CORNER","RADIO CHECK 02:14 / NO REPLY FROM RECEIVING.","DO NOT USE THE FREIGHT AISLES FOR EVACUATION.");
 wallLamp(m,1.02f,21.9f,base+2.7f,0,1.1f,6.f);
 // --- Walls, roof and services ---------------------------------------------------------
 wallBays(m,base,roof,4.f,true,false,false,false,12.f,20.f);
 for(float x:{9.4f,14.6f})wallLamp(m,x,1.02f,base+2.8f,kPi*.5f,1.2f,7.f);
 for(float y:{4.f,10.f,16.f,22.f}){wall(m,1.f,y-.18f,24.f,y+.18f,roof-1.2f,roof,12);steelColumn(m,23.85f,y,base,roof-1.2f,false);}
 servicePipe(m,{1.2f,12.1f},{23.8f,12.1f},-9.f,.4f,5);
 for(float x:{5.63f,11.98f})servicePipe(m,{x,1.2f},{x,24.f},-12.4f,.05f,4);
 cableTray(m,{1.6f,1.6f},{22.4f,1.6f},base+5.f);
 for(float x:{5.63f,11.98f,20.2f})for(float y:{5.5f,12.1f,18.6f})m.lights.push_back(lamp({x,y},-11.5f,3.2f,22.f,4000,.1f));
 // --- Supplies ----------------------------------------------------------------
 m.pickupSpawns.push_back({{12.f,3.2f},PickupKind::Ammo,base});
 m.clutterSpawns.push_back({2,{5.6f,10.f},base});m.clutterSpawns.push_back({1,{12.6f,20.f},base});
 zone(m,"WAREHOUSE / OPERATIONS",17.8f,1.f,22.7f,7.6f);
 freightFinish(m,16);
 return p;
}

// WAREHOUSE / C BLOCK. Chemical inventory. Drum and carton racking runs
// along the west; bunded bays of intermediate bulk containers sit under the
// south walk; the chemical hold in the north-east corner is caged, and its
// gate has been forced from inside. The inventory lift between the racks and
// the hold carries stock, and anyone standing on it, up to the south walk,
// which leads on into D block. A stair climbs the east wall to the same walk.
std::shared_ptr<const Map> makeWarehouseC(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[17];m.definition=CampaignChunks[17];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.32f;
 const float base=-25,walk=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openWest=true;m.openSouth=true;
 rectangle(m.layers[0],0,1,1,23,'.');
 // --- The south walk at -19, which continues into D block -------------------------------------
 deck(m,walk,1,19,23,24);
 m.stairs.push_back({20.4f,10.f,22.7f,19.f,base,walk,32,true,true});
 guardRail(m,1.f,19.f,10.5f,19.05f,walk);guardRail(m,13.5f,19.f,20.4f,19.05f,walk);guardRail(m,1.f,19.f,1.05f,24.f,walk);
 floorLine(m,10.5f,19.f,13.5f,19.15f,walk,13);
 wallLamp(m,22.98f,22.6f,walk+2.5f,kPi,1.3f,8.f);
 // --- The inventory lift --------------------------------------------------------------------
 machineryClock(m,"warehouse_car",26000);
 actor(m,"warehouse_car",ActorVisual::Cargo,1.f,{{0,{12,17.5f},base,0},{6000,{12,17.5f},base,0},{11000,{12,17.5f},walk,0},{19000,{12,17.5f},walk,0},{24000,{12,17.5f},base,0},{26000,{12,17.5f},base,0}},true);
 m.actorTracks.back().platform=true;m.actorTracks.back().footprint={3,3};m.actorTracks.back().suspensionTop=-15.6f;
 for(float x:{10.05f,13.65f})for(float y:{16.2f,18.5f})wall(m,x,y,x+.3f,y+.3f,base,-15.3f,12);
 for(float x:{10.05f,13.65f})wall(m,x,16.2f,x+.3f,18.8f,-15.6f,-15.3f,12);
 wall(m,10.05f,17.35f,13.95f,17.65f,-15.6f,-15.3f,12);
 m.pipes.push_back({{12.f,17.5f},{13.f,17.5f},-15.15f,.15f,-15.15f,6});
 for(float x:{10.35f,13.65f})floorLine(m,x-.1f,15.9f,x,19.f,base,13);
 floorLine(m,10.25f,15.8f,13.75f,15.9f,base,13);
 // --- Drum and carton racking, west ----------------------------------------------------------
 for(float x:{2.2f,6.4f})for(float y:{2.6f,6.62f})rack(m,x,y,base,2);
 rackSign(m,3.58f,2.6f,base+3.4f,"C1","CHEMICAL INVENTORY / CARTONS");
 rackSign(m,7.78f,2.6f,base+3.4f,"C2","CHEMICAL INVENTORY / DRUMS");
 auto drums=[&](float x,float y,int paint){m.fixtures.push_back({15,{x,y},0,1.2f,1.15f,.16f,0,true});
  for(float dx:{-.3f,.3f})for(float dy:{-.29f,.29f}){m.pipes.push_back({{x+dx,y+dy},{x+dx,y+dy},base+.16f,.3f,base+1.04f,paint});m.pipes.push_back({{x+dx,y+dy},{x+dx,y+dy},base+1.04f,.12f,base+1.07f,6});}};
 drums(11.f,4.f,3);drums(12.6f,4.f,3);drums(11.f,6.2f,4);
 m.fixtures.push_back({15,{12.7f,6.3f},0,1.2f,1.15f,.16f,0,true});
 m.fixtures.push_back({21,{12.7f,6.3f},.16f,1.1f,1.f,1.f,0,true});
 // --- Safety shower and eyewash --------------------------------------------------------------
 wall(m,9.4f,11.6f,10.2f,11.7f,base,base+2.6f,20);
 m.pipes.push_back({{10.f,11.745f},{10.f,11.745f},base,.045f,base+2.3f,5});
 m.pipes.push_back({{10.f,11.745f},{10.f,12.5f},base+2.3f,.045f,base+2.3f,5});
 wall(m,9.85f,12.42f,10.15f,12.75f,base+2.22f,base+2.36f,2);
 wall(m,9.45f,11.7f,9.9f,12.05f,base+.95f,base+1.05f,20);
 wallSign(m,9.65f,11.755f,base+1.4f,"SAFETY SHOWER","EYEWASH / 15 MIN",.5f,0);
 wallLamp(m,9.8f,11.72f,base+2.45f,kPi*.5f,1.f,5.f);
 // --- The chemical hold, north-east: caged, its gate forced -------------------------------------
 guardRail(m,15.f,1.f,15.06f,8.6f,base);guardRail(m,15.f,8.54f,17.5f,8.6f,base);guardRail(m,19.f,8.54f,22.8f,8.6f,base);
 guardRail(m,19.f,8.6f,19.06f,10.f,base);
 for(float x:{15.f,17.4f,19.f,22.7f})wall(m,x,8.5f,x+.1f,8.64f,base,base+2.7f,12);
 for(float y:{1.f,4.8f})wall(m,15.f,y,15.1f,y+.1f,base,base+2.7f,12);
 wall(m,15.f,8.5f,22.8f,8.64f,base+2.58f,base+2.7f,12);wall(m,15.f,1.f,15.1f,8.6f,base+2.58f,base+2.7f,12);
 for(Vec2 t:{Vec2{21.8f,2.4f},Vec2{21.8f,4.6f}})m.fixtures.push_back({14,t,0,1.3f,1.35f,2.f,0,true});
 drums(16.6f,2.4f,3);drums(16.6f,4.f,4);drums(18.2f,2.4f,5);
 wall(m,17.4f,5.2f,20.2f,7.2f,base,base+.004f,8);
 for(float x:{15.4f,21.2f})wallLamp(m,x,1.02f,base+3.f,kPi*.5f,1.1f,6.f);
 // --- Bunded bays under the south walk -------------------------------------------------------
 for(auto [x1,x2]:{std::pair{1.4f,9.4f},std::pair{14.6f,20.2f}}){
  wall(m,x1,19.6f,x2,19.75f,base,base+.25f,12);wall(m,x1,19.6f,x1+.15f,23.6f,base,base+.25f,12);wall(m,x2-.15f,19.6f,x2,23.6f,base,base+.25f,12);
  floorLine(m,x1,19.6f,x2,19.75f,base+.25f,13);
  for(float x=x1+1.f;x<x2-.6f;x+=1.4f)for(float y:{21.f,22.5f})m.fixtures.push_back({21,{x,y},0,1.2f,1.f,1.15f,0,true});
 }
 m.lights.push_back(lamp({5.4f,21.6f},walk-.5f,1.2f,7.f,4000));m.lights.push_back(lamp({17.4f,21.6f},walk-.5f,1.2f,7.f,4000));
 // --- Walls, roof and services ---------------------------------------------------------
 wallBays(m,base,roof,4.f,true,false,false,true);
 for(float y:{4.f,10.f,16.f,22.f})wall(m,0,y-.18f,23.f,y+.18f,roof-1.2f,roof,12);
 servicePipe(m,{1.2f,12.1f},{22.8f,12.1f},-9.f,.4f,5);
 servicePipe(m,{1.2f,1.7f},{22.8f,1.7f},-14.f,.45f,5);
 for(float y:{6.f,14.f})wallLamp(m,22.98f,y,base+4.5f,kPi,1.3f,8.f);
 for(float x:{4.5f,12.f,19.f})for(float y:{5.5f,12.f})m.lights.push_back(lamp({x,y},-11.5f,3.f,22.f,4200,.1f));
 for(float x:{4.5f,17.f})m.lights.push_back(lamp({x,21.5f},-11.5f,2.6f,20.f,4200,.1f));
 // --- Creatures and supplies --------------------------------------------------------------
 m.creatureSpawns={{CreatureKind::Huntsman,{18.f,12.5f},base},{CreatureKind::Wasp,{6.5f,15.f},base},{CreatureKind::Mutant,{19.6f,6.4f},base}};
 m.pickupSpawns.push_back({{12.f,2.6f},PickupKind::Ammo,base});
 m.pickupSpawns.push_back({{16.f,22.f},PickupKind::Health,walk});
 zone(m,"C BLOCK / CHEMICAL HOLD",15.f,1.f,23.f,8.6f);
 deckSupports(m,m.layers[1],base,6.f);
 freightFinish(m,17);
 return p;
}

// WAREHOUSE / D BLOCK. Restricted freight. The south walk from C block
// arrives on the north walk six metres up, looking down into the restricted
// compound: a fenced enclosure round an open containment container, sealed
// crates, the sample-handling plant and a bay of tall long-hold racking that
// rises nearly to the roof. Something moves in the top of it. A stair climbs
// from the north walk to the west mezzanine over the parts racks, and the
// mezzanine runs on west into the upper racks.
std::shared_ptr<const Map> makeWarehouseD(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[18];m.definition=CampaignChunks[18];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.3f;
 const float base=-25,walk=-19,mezz=-13,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));m.openNorth=true;m.openWest=true;
 rectangle(m.layers[0],0,1,1,23,'.');
 // --- North walk (continues C block's south walk) and the stair to the mezzanine ------------
 deck(m,walk,1,0,23,4);
 m.stairs.push_back({6.f,4.f,9.f,13.f,walk,mezz,32,true,true});
 guardRail(m,1.f,4.f,6.f,4.06f,walk);guardRail(m,9.f,4.f,23.f,4.06f,walk);guardRail(m,1.f,0,1.06f,4.f,walk);
 terminal(m,{16.f,2.4f},"RESTRICTED / SEALED MANIFEST","CONTAINMENT HARDWARE / SAMPLE HANDLING.","CONTENTS NOT FOR LOCAL INSPECTION.",walk-base);
 wallSign(m,22.945f,2.f,walk+1.5f,"RACK ACCESS CLOSED","OVERHEAD STOCK DISPLACED / D3",1.8f,-kPi*.5f);
 // --- West mezzanine over the parts racks, open west to the upper racks ---------------------
 deck(m,mezz,0,9,10,24);
 guardRail(m,10.f,9.f,10.06f,24.f,mezz);guardRail(m,0,9.f,6.f,9.06f,mezz);guardRail(m,9.f,9.f,10.f,9.06f,mezz);
 guardRail(m,5.94f,9.f,6.f,13.f,mezz);guardRail(m,9.f,9.f,9.06f,13.f,mezz);
 for(float y:{9.6f,13.62f,17.64f})for(float x:{1.3f,6.3f})rack(m,x,y,base,3,y>17.f);
 // --- The restricted compound ------------------------------------------------------------
 guardRail(m,11.f,8.f,15.f,8.06f,base);guardRail(m,16.4f,8.f,22.8f,8.06f,base);
 guardRail(m,11.f,8.f,11.06f,12.f,base);guardRail(m,11.f,13.4f,11.06f,22.8f,base);
 for(float x:{11.f,13.f,15.f,16.4f,18.6f,20.7f,22.7f})wall(m,x,7.97f,x+.1f,8.09f,base,base+3.f,12);
 for(float y:{10.f,12.f,13.4f,15.6f,17.8f,20.f,22.6f})wall(m,10.97f,y,11.09f,y+.1f,base,base+3.f,12);
 for(float z:{base+1.8f,base+2.9f}){wall(m,11.f,7.98f,22.8f,8.08f,z,z+.08f,12);wall(m,10.98f,8.f,11.08f,22.8f,z,z+.08f,12);}
 guardRail(m,11.06f,12.f,12.3f,12.06f,base);
 container(m,12.4f,10.f,base,true,7);
 for(float y:{13.5f,14.9f})m.fixtures.push_back({21,{13.9f,y},0,1.1f,1.f,1.f,0,true});
 for(float y:{18.8f,20.6f})m.fixtures.push_back({14,{17.4f,y},0,1.3f,1.35f,2.f,0,true});
 m.fixtures.push_back({22,{15.6f,21.6f},0,.78f,.49f,1.3f,kPi,true});
 for(float y:{19.8f,21.4f})m.fixtures.push_back({21,{13.4f,y},0,1.2f,1.f,1.1f,.1f,true});
 for(float x=12.6f;x<16.f;x+=.6f)floorLine(m,x,17.6f,x+.3f,17.9f,base,13);
 for(float y:{9.f,13.02f,17.04f})rack(m,19.9f,y,base,6,y>16.f);
 rackSign(m,21.28f,9.f,base+3.4f,"D3","RESTRICTED / LONG HOLD");
 machineryClock(m,"upper_rack_noise",23000);
 cue(m,"upper_rack_noise",7000,Sound::JunkMetal,{21.f,14.f},"",.9f,-8.f);
 cue(m,"upper_rack_noise",16000,Sound::SpiderCall,{21.f,14.f},"",.5f,-8.f);
 // --- Walls, roof and services ---------------------------------------------------------
 wallBays(m,base,roof,4.f,false,true,false,true);
 for(float y:{4.f,10.f,16.f,22.f})wall(m,0,y-.18f,23.f,y+.18f,roof-1.2f,roof,12);
 servicePipe(m,{1.2f,12.1f},{22.8f,12.1f},-9.f,.4f,5);
 for(float x:{4.f,12.f,17.5f})for(float y:{6.f,14.f,20.f})m.lights.push_back(lamp({x,y},-10.f,3.f,22.f,4200,.1f));
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,2.f},walk-.5f,1.2f,8.f,4000));
 for(float y:{12.f,20.f})m.lights.push_back(lamp({4.f,y},mezz-.5f,1.3f,9.f,4000));
 for(float y:{6.f,14.f})wallLamp(m,22.98f,y,base+4.5f,kPi,1.3f,8.f);
 // --- Creatures and supplies --------------------------------------------------------------
 m.creatureSpawns={{CreatureKind::Huntsman,{3.f,18.f},mezz},{CreatureKind::Wasp,{14.f,2.4f},walk}};
 m.pickupSpawns.push_back({{12.f,2.6f},PickupKind::Ammo,walk});
 zone(m,"D BLOCK / RESTRICTED COMPOUND",11.f,8.f,23.f,23.f,-26.f,-20.f);
 deckSupports(m,m.layers[1],base,6.f);deckSupports(m,m.layers[2],base,6.f);
 freightFinish(m,18);
 return p;
}

// WAREHOUSE / UPPER RACKS. E block, long-term storage: racking eighteen
// metres high, the tallest in the warehouse. D block's mezzanine arrives on
// the east picking deck; a stair climbs from it to the top landing along the
// south wall, where the manifest offices' window looks back over the hall and
// the door leads into them. Under the deck a run of upper racking has come
// down: the breach.
std::shared_ptr<const Map> makeWarehouseUpper(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[19];m.definition=CampaignChunks[19];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.3f;
 const float base=-25,deckZ=-13,top=-7,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));m.openNorth=true;m.openEast=true;
 rectangle(m.layers[0],23,1,24,23,'.');
 // The door into the manifest offices, and their window onto the hall.
 rectangle(m.layers[0],10,23,14,24,'.');m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,top-base});
 wall(m,10,23,14,24,top+2.5f,roof,3);wall(m,10,23,14,24,base,top-.22f,15);
 rectangle(m.layers[0],2,23,10,24,'.');
 wall(m,2,23.85f,10,24,base,-6.15f,3);wall(m,2,23.85f,10,24,-4.35f,roof,3);
 for(float x:{2.f,5.9f,9.85f})wall(m,x,23.85f,x+.15f,24,-6.15f,-4.35f,2);
 // --- East picking deck, the stair and the top landing ------------------------------------
 deck(m,deckZ,14,9,24,16);
 m.stairs.push_back({16.f,13.f,19.f,22.f,deckZ,top,32,true,true});
 guardRail(m,14.f,9.f,14.06f,16.f,deckZ);guardRail(m,14.f,9.f,23.f,9.06f,deckZ);
 guardRail(m,14.f,15.94f,16.f,16.f,deckZ);guardRail(m,19.f,15.94f,23.f,16.f,deckZ);
 deck(m,top,2,21,20,24);
 guardRail(m,2.f,21.f,16.f,21.06f,top);guardRail(m,19.f,21.f,20.f,21.06f,top);
 guardRail(m,19.94f,21.f,20.f,23.f,top);guardRail(m,2.f,21.f,2.06f,23.f,top);
 terminal(m,{5.f,22.4f},"UPPER STORAGE / BREACH","RESTRICTED FREIGHT: MANIFEST CONTROL.","OFFICES ABOVE THE INTAKE END.",top-base);
 m.clutterSpawns.push_back({3,{13.f,22.f},top});
 // --- E block racking ---------------------------------------------------------------------
 for(float x:{1.3f,7.2f,9.96f})for(float y:{2.5f,6.52f,10.54f,14.56f})rack(m,x,y,base,6,y>14.f);
 for(float x:{14.5f,19.f})rack(m,x,2.5f,base,6);
 rackSign(m,2.68f,2.5f,base+3.4f,"E1","LONG TERM / BAYS 01-08");
 rackSign(m,8.58f,2.5f,base+3.4f,"E2","LONG TERM / BAYS 09-16");
 rackSign(m,15.88f,2.5f,base+3.4f,"E3","LONG TERM / OVERSIZE");
 for(float x:{4.12f,13.f})floorLine(m,x-.05f,2.f,x+.05f,19.f,base,14);
 // The breach: a run of upper stock down under the deck, a beam with it.
 cargo(m,15.4f,18.6f,base,true,.5f);cargo(m,17.2f,19.8f,base,false,-.3f);cargo(m,20.6f,18.2f,base,true,1.1f);
 m.fixtures.push_back({17,{18.4f,17.6f},0,1.f,1.2f,.9f,.7f,true});
 m.fixtures.push_back({17,{21.4f,20.6f},0,1.f,1.2f,.8f,-.4f,true});
 wall(m,14.8f,21.f,19.8f,21.14f,base,base+.14f,18);
 for(float y=17.f;y<22.f;y+=.7f)floorLine(m,14.4f,y,14.7f,y+.35f,base,13);
 // --- Walls, roof and services ---------------------------------------------------------
 wallBays(m,base,roof,4.f,false,true,true,false,6.f,12.f);
 for(float y:{4.f,10.f,16.f,22.f}){wall(m,1.f,y-.18f,24.f,y+.18f,roof-1.2f,roof,12);steelColumn(m,23.85f,y,base,roof-1.2f,false);}
 servicePipe(m,{1.2f,20.4f},{23.8f,20.4f},-2.f,.4f,5);
 for(Vec2 l:{Vec2{5.63f,6.f},Vec2{5.63f,13.f},Vec2{13.5f,6.f},Vec2{19.f,12.f},Vec2{8.f,19.5f}})m.lights.push_back(lamp(l,-3.f,3.f,22.f,4200,.1f));
 for(float y:{5.f,12.f,18.f})m.lights.push_back(lamp({5.63f,y},-8.5f,1.4f,12.f,4000));
 m.lights.push_back(lamp({13.6f,12.f},-8.5f,1.4f,12.f,4000));
 for(Vec2 l:{Vec2{21.f,12.f},Vec2{20.f,19.5f}})m.lights.push_back(lamp(l,deckZ-.6f,1.4f,12.f,4000));
 m.lights.push_back(lamp({6.f,22.f},top+2.5f,1.1f,8.f,3800));
 for(float x:{15.f,19.5f})wallLamp(m,x,22.98f,top+2.3f,-kPi*.5f,1.1f,7.f);
 // --- Supplies ----------------------------------------------------------------
 m.pickupSpawns.push_back({{21.f,12.f},PickupKind::Ammo,deckZ});
 deckSupports(m,m.layers[1],base,6.f);deckSupports(m,m.layers[2],base,6.f);
 freightFinish(m,19);
 return p;
}

// MANIFEST OFFICES. Freight records at the top of the warehouse. The door
// from the upper racks opens on reception; the overlook gallery watches the
// warehouse through the window wall; the bullpen of cubicles fills the
// floor, with the records room west and the supervisors' glass offices east.
// The kitchen is in the south-west corner; the floor runs on south into
// the routing wing.
std::shared_ptr<const Map> makeManifestOffices(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[20];m.definition=CampaignChunks[20];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.46f;
 m.definition.floorMaterial=FloorMaterial::OfficeCarpet;m.visibleResidencyGroups={2};
 const float f=-7,ceil=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f,false,true));m.openSouth=true;
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,0});
 // --- The overlook gallery and its window wall --------------------------------------------
 rectangle(m.layers[0],2,0,10,1,'.');
 wall(m,2,0,10,.16f,f,-6.15f,3);wall(m,2,0,10,.16f,-4.35f,ceil,3);
 for(float x:{2.f,4.6f,7.25f,9.85f})wall(m,x,0,x+.15f,.16f,-6.15f,-4.35f,20);
 wall(m,2.15f,.045f,9.85f,.065f,-6.15f,-4.35f,17);
 wall(m,2.f,.16f,10.f,.36f,-6.2f,-6.12f,20);
 for(float x:{3.2f,6.f,8.8f}){
  m.fixtures.push_back({24,{x,1.45f},0,1.4f,.7f,.75f,0,true});
  m.fixtures.push_back({26,{x,2.3f},0,.55f,.55f,.95f,0,true});
  m.fixtures.push_back({27,{x-.15f,1.4f},.75f,.42f,.4f,.38f,0,false});
  m.fixtures.push_back({28,{x,1.72f},.75f,.45f,.16f,.03f,0,false});
 }
 m.fixtures.push_back({38,{6.5f,1.3f},.75f,.3f,.08f,.32f,0,false});
 // --- Reception ---------------------------------------------------------------------------
 m.fixtures.push_back({24,{15.2f,2.8f},0,1.4f,.7f,.75f,0,true});
 m.fixtures.push_back({25,{16.7f,2.6f},0,1.2f,1.2f,.75f,kPi*.5f,true});
 m.fixtures.push_back({26,{15.4f,1.9f},0,.55f,.55f,.95f,kPi,true});
 m.fixtures.push_back({27,{15.f,2.85f},.75f,.42f,.4f,.38f,kPi,false});
 m.fixtures.push_back({28,{15.3f,2.55f},.75f,.45f,.16f,.03f,kPi,false});
 m.fixtures.push_back({33,{16.4f,2.9f},.75f,.2f,.2f,.1f,kPi,false});
 wall(m,14.4f,3.2f,17.4f,3.4f,f,f+1.08f,21);wall(m,14.35f,3.15f,17.45f,3.55f,f+1.08f,f+1.12f,20);
 for(float x:{19.4f,20.4f,21.4f})m.fixtures.push_back({18,{x,1.5f},0,.417f,.525f,.95f,0,true});
 m.fixtures.push_back({35,{22.5f,1.5f},0,.35f,.35f,1.1f,0,true});
 // --- Records room, north-west ---------------------------------------------------------------
 officeWall(m,1.f,5.8f,2.2f,6.f,f,ceil);officeDoorway(m,2.2f,3.4f,5.8f,6.f,f,ceil);officeWall(m,3.4f,5.8f,7.f,6.f,f,ceil);
 officeWall(m,6.8f,6.f,7.f,13.8f,f,ceil);officeWall(m,1.f,13.6f,7.f,13.8f,f,ceil);
 for(float y:{8.f,10.2f,12.4f})for(float x:{3.4f,5.2f})m.fixtures.push_back({7,{x,y},0,1.5f,.45f,1.8f,0,true});
 for(float y:{7.4f,8.f,8.6f})m.fixtures.push_back({32,{1.4f,y+3.2f},0,.5f,.6f,1.3f,kPi*.5f,true});
 for(float z:{-5.4f,-4.95f,-4.5f}){wall(m,1.02f,6.6f,1.47f,9.8f,z,z+.045f,2);
  for(int i=0;i<15;++i){float y=6.7f+i*.2f;wall(m,1.14f,y,1.38f,y+.15f,z+.045f,z+.37f,i%3==0?19:i%3==1?20:18);}}
 m.lights.push_back(lamp({4.f,10.f},ceil-.16f,.9f,6.f,4200));
 // --- The bullpen -----------------------------------------------------------------------------
 cubicleBank(m,8.8f,4,9.2f,f,0);
 cubicleBank(m,8.8f,4,13.8f,f,2);
 terminal(m,{6,16},"MANIFEST / MISSED CALLS","RECEIVING: 02:14 / 02:17 / 02:21 / NO ANSWER.","EMPLOYEE TRANSIT LOG: PLATFORM 3.");
 // --- Supervisors' offices, east ----------------------------------------------------------------
 officeGlass(m,17.2f,5.8f,17.4f,17.8f,f,ceil);
 officeGlass(m,17.4f,5.8f,18.4f,6.f,f,ceil);officeDoorway(m,18.4f,19.6f,5.8f,6.f,f,ceil);officeGlass(m,19.6f,5.8f,23.f,6.f,f,ceil);
 officeWall(m,17.4f,11.7f,23.f,11.9f,f,ceil);
 officeGlass(m,17.4f,17.6f,18.4f,17.8f,f,ceil);officeDoorway(m,18.4f,19.6f,17.6f,17.8f,f,ceil);officeGlass(m,19.6f,17.6f,23.f,17.8f,f,ceil);
 // North office: the arrival record for Train 27.
 m.fixtures.push_back({25,{21.6f,7.4f},0,1.6f,1.6f,.75f,0,true});
 m.fixtures.push_back({26,{20.9f,8.6f},0,.55f,.55f,.95f,0,true});
 m.fixtures.push_back({27,{21.3f,7.2f},.75f,.42f,.4f,.38f,0,false});
 m.fixtures.push_back({31,{22.5f,10.6f},0,.9f,.5f,.75f,-kPi*.5f,true});
 m.fixtures.push_back({30,{22.5f,10.6f},.75f,.5f,.4f,.42f,-kPi*.5f,false});
 m.fixtures.push_back({37,{18.f,11.45f},0,.9f,.3f,1.8f,kPi,true});
 terminal(m,{19.4f,8.6f},"TRAIN 27 / ARRIVAL RECORD","PASSENGERS: 0 / CARGO: NONE / PLATFORM 3.","MANUAL ANNOTATION: THIS IS WRONG.");
 // South office: ordinary freight.
 m.fixtures.push_back({24,{21.4f,15.6f},0,1.4f,.7f,.75f,kPi,true});
 m.fixtures.push_back({26,{21.4f,14.8f},0,.55f,.55f,.95f,kPi,true});
 m.fixtures.push_back({27,{21.2f,15.7f},.75f,.42f,.4f,.38f,kPi,false});
 for(float y:{12.6f,13.4f})m.fixtures.push_back({32,{22.55f,y},0,.5f,.6f,1.3f,-kPi*.5f,true});
 m.fixtures.push_back({37,{18.f,12.25f},0,.9f,.3f,1.8f,0,true});
 terminal(m,{19.4f,14.6f},"MANIFEST / ORDINARY FREIGHT","BEARINGS / FILTERS / FOOD / MEDICAL SUPPLIES.","SPECIALIZED CONTAINMENT HARDWARE: RESEARCH RECEIVING.");
 for(Vec2 l:{Vec2{20.2f,8.8f},Vec2{20.2f,14.8f}})m.lights.push_back(lamp(l,ceil-.16f,.9f,6.f,3800));
 // --- Kitchen, south-west ---------------------------------------------------------------------
 officeWall(m,1.f,16.6f,6.8f,16.8f,f,ceil);
 wall(m,1.f,17.f,1.65f,22.6f,f,f+.9f,21);wall(m,1.f,16.95f,1.7f,22.65f,f+.9f,f+.94f,20);
 m.fixtures.push_back({36,{1.35f,17.8f},.94f,.35f,.35f,.45f,kPi*.5f,false});
 m.fixtures.push_back({34,{1.35f,18.4f},.94f,.09f,.09f,.1f,0,false});
 m.fixtures.push_back({35,{1.4f,23.2f},0,.35f,.35f,1.1f,0,true});
 m.fixtures.push_back({19,{4.2f,20.f},0,1.367f,.724f,.78f,0,true});
 for(float x:{3.7f,4.7f}){m.fixtures.push_back({18,{x,19.25f},0,.417f,.525f,.95f,0,true});m.fixtures.push_back({18,{x,20.75f},0,.417f,.525f,.95f,kPi,true});}
 wallSign(m,1.055f,20.f,f+1.6f,"STAFF NOTICES","SHIFT 02 / RADIO CHECK EVERY 30 MIN",1.4f,kPi*.5f);
 // --- Copier corner, south-east ---------------------------------------------------------------
 for(float y:{19.6f,20.6f,21.6f})m.fixtures.push_back({31,{22.5f,y},0,.9f,.5f,.75f,-kPi*.5f,true});
 m.fixtures.push_back({37,{22.6f,23.f},0,.9f,.3f,1.8f,-kPi*.5f,true});
 // --- Ceiling and light ------------------------------------------------------------------------
 ceilingGrid(m,1.f,1.f,23.f,24.f,ceil);
 for(float x:{4.f,9.6f,14.4f,19.2f})for(float y:{3.f,7.4f,11.6f,15.8f,20.4f}){if(x>17&&y>5&&y<18)continue;if(x<7&&y>5&&y<14)continue;m.lights.push_back(lamp({x,y},ceil-.16f,.8f,7.f,4000));}
 m.pickupSpawns.push_back({{6.f,19.6f},PickupKind::Health,f});
 zone(m,"MANIFEST OFFICES / RECORDS",1,6,6.8f,13.6f);
 freightFinish(m,20);
 return p;
}

// MANIFEST ROUTING. The routing wing. The corridor from the offices passes
// the manifest archive, the briefing room and the switch room; beyond them
// the routing control room faces the transfer yard mimic board, and the
// routing supervisor's booth holds the control that releases the
// downstream shutters. The door south leads to the transfer conveyors.
std::shared_ptr<const Map> makeManifestRouting(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[21];m.definition=CampaignChunks[21];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.44f;
 m.definition.floorMaterial=FloorMaterial::OfficeCarpet;m.visibleResidencyGroups={2};
 const float f=-7,ceil=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f,true,false));m.openNorth=true;
 rectangle(m.layers[0],10,23,14,24,'.');m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,0});
 m.doors.back().requireState=stateId("freight_routing");
 wall(m,10,23,14,24,f+2.5f,ceil,21);
 // --- Archive, north-west --------------------------------------------------------------------
 officeWall(m,1.f,3.4f,2.6f,3.6f,f,ceil);officeDoorway(m,2.6f,3.8f,3.4f,3.6f,f,ceil);officeWall(m,3.8f,3.4f,8.f,3.6f,f,ceil);
 officeWall(m,7.8f,3.6f,8.f,11.6f,f,ceil);officeWall(m,1.f,11.4f,8.f,11.6f,f,ceil);
 for(float y:{5.4f,7.6f,9.8f})for(float x:{3.4f,5.2f})m.fixtures.push_back({7,{x,y},0,1.5f,.45f,1.8f,0,true});
 for(float y:{5.4f,7.6f,9.8f})m.fixtures.push_back({38,{4.3f,y-.1f},1.2f,.3f,.08f,.32f,0,false});
 for(float y:{5.f,6.f,7.f})m.fixtures.push_back({32,{1.4f,y+2.f},0,.5f,.6f,1.3f,kPi*.5f,true});
 m.lights.push_back(lamp({4.8f,7.6f},ceil-.16f,.8f,6.f,4200));
 // --- Briefing room, centre ---------------------------------------------------------------------
 officeGlass(m,9.6f,3.4f,10.4f,3.6f,f,ceil);officeDoorway(m,10.4f,11.6f,3.4f,3.6f,f,ceil);officeGlass(m,11.6f,3.4f,15.2f,3.6f,f,ceil);
 officeGlass(m,9.6f,11.4f,15.2f,11.6f,f,ceil);officeGlass(m,9.6f,3.6f,9.8f,11.4f,f,ceil);officeWall(m,15.f,3.6f,15.2f,11.4f,f,ceil);
 m.fixtures.push_back({19,{11.8f,7.5f},0,1.2f,3.2f,.78f,0,true});
 for(float y:{6.4f,7.5f,8.6f}){m.fixtures.push_back({26,{10.75f,y},0,.55f,.55f,.95f,-kPi*.5f,true});m.fixtures.push_back({26,{12.85f,y},0,.55f,.55f,.95f,kPi*.5f,true});}
 m.fixtures.push_back({26,{11.8f,9.65f},0,.55f,.55f,.95f,0,true});
 for(Vec2 q:{Vec2{11.6f,6.6f},Vec2{12.f,8.4f}})m.fixtures.push_back({38,q,.78f,.3f,.08f,.32f,.4f,false});
 wallSign(m,14.945f,7.5f,f+1.2f,"SHIFT 02 BRIEFING","DEPOT / PLATFORM 3 / DEEP FREIGHT / NO CHANGES",2.4f,-kPi*.5f);
 m.lights.push_back(lamp({11.8f,7.5f},ceil-.16f,.9f,6.f,4000));
 // --- Switch room, north-east -------------------------------------------------------------------
 officeWall(m,15.2f,3.4f,16.6f,3.6f,f,ceil);officeDoorway(m,16.6f,17.8f,3.4f,3.6f,f,ceil);officeWall(m,17.8f,3.4f,23.f,3.6f,f,ceil);
 officeWall(m,15.2f,11.4f,23.f,11.6f,f,ceil);
 for(float y:{4.6f,5.6f,6.6f,7.6f,8.6f,9.6f})m.fixtures.push_back({13,{22.75f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});
 cableTray(m,{22.6f,4.f},{22.6f,11.f},ceil-.12f);
 for(float y:{4.6f,6.6f,8.6f})m.pipes.push_back({{22.85f,y+.5f},{22.85f,y+.5f},f+2.2f,.04f,ceil-.1f,1});
 // Two back-to-back rows of switchgear down the room, fed from trays at the
 // ceiling; distribution boards on the west wall; the duty desk and UPS.
 for(float y=5.f;y<10.2f;y+=.92f){m.fixtures.push_back({13,{19.f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});m.fixtures.push_back({13,{19.52f,y},0,.9066f,.4956f,2.2f,-kPi*.5f,true});}
 wall(m,18.7f,4.5f,19.82f,10.2f,f+2.2f,f+2.26f,20);
 cableTray(m,{19.26f,4.f},{19.26f,11.f},ceil-.12f);
 for(float y:{5.5f,7.3f,9.2f})m.pipes.push_back({{19.26f,y},{19.26f,y},f+2.26f,.05f,ceil-.12f,1});
 for(float y:{5.f,6.6f})m.fixtures.push_back({8,{15.27f,y},1.1f,.7f,.15f,1.f,kPi*.5f,false});
 for(float y:{5.f,6.6f})m.pipes.push_back({{15.3f,y},{15.3f,y},f+2.1f,.04f,ceil-.1f,1});
 m.fixtures.push_back({24,{15.6f,9.f},0,1.4f,.7f,.75f,kPi*.5f,true});
 m.fixtures.push_back({26,{16.45f,9.f},0,.55f,.55f,.95f,kPi*.5f,true});
 m.fixtures.push_back({27,{15.5f,9.2f},.75f,.42f,.4f,.38f,kPi*.5f,false});
 m.fixtures.push_back({28,{15.8f,8.95f},.75f,.45f,.16f,.03f,kPi*.5f,false});
 m.fixtures.push_back({6,{16.2f,10.8f},0,1.4f,.7f,1.1f,0,true});
 for(float x=15.6f;x<22.6f;x+=.6f)wall(m,x,3.8f,x+.02f,11.2f,f,f+.006f,2);
 for(float y=3.8f;y<11.2f;y+=.6f)wall(m,15.4f,y,22.6f,y+.02f,f,f+.006f,2);
 wallSign(m,21.f,11.345f,f+1.6f,"DANGER / 415 V","ISOLATE BEFORE OPENING / FREIGHT CONTROL",1.8f,kPi);
 for(float x:{17.f,21.4f})for(float y:{5.2f,9.4f})m.lights.push_back(lamp({x,y},ceil-.16f,1.4f,7.f,5200));
 // --- Routing control room --------------------------------------------------------------------
 // The transfer yard mimic board fills the south wall either side of the door.
 for(float x1:{1.6f,14.f}){float x2=x1+8.4f;
  wall(m,x1,22.75f,x2,22.95f,f+.9f,f+2.75f,2);
  for(float z:{f+1.4f,f+2.f,f+2.6f})wall(m,x1+.3f,22.72f,x2-.3f,22.75f,z,z+.05f,x1<10?18:19);
  for(float x=x1+1.f;x<x2-.5f;x+=1.6f)wall(m,x,22.72f,x+.05f,22.75f,f+1.4f,f+2.65f,20);
 }
 wallSign(m,5.8f,22.945f,f+2.85f,"TRANSFER YARD / ROUTING","CONVEYORS 22 / SORTING 23 / RAIL YARD 24",4.f,kPi);
 // Two rows of routing desks face the board.
 for(float y:{16.f,19.2f})for(float x:{3.f,5.f,7.f,15.f,17.f})workstation(m,x,y,f,1,int(x+y));
 for(float y:{16.f,19.2f})wall(m,2.2f,y+.4f,7.8f,y+.46f,f+.75f,f+1.15f,20);
 // CCTV wall.
 wall(m,1.f,12.6f,1.5f,15.f,f+1.2f,f+1.25f,20);
 for(float y:{13.f,14.f})for(float z:{1.25f,1.7f})m.fixtures.push_back({30,{1.25f,y},z,.5f,.4f,.42f,kPi*.5f,false});
 // The routing supervisor's booth, east.
 officeGlass(m,19.f,12.6f,19.2f,18.4f,f,ceil);officeGlass(m,19.2f,12.6f,23.f,12.8f,f,ceil);
 officeGlass(m,19.2f,18.2f,19.8f,18.4f,f,ceil);officeDoorway(m,19.8f,21.f,18.2f,18.4f,f,ceil);officeGlass(m,21.f,18.2f,23.f,18.4f,f,ceil);
 terminal(m,{20.4f,16.4f},"MANIFEST / ROUTING SHUTTERS","TRANSFER YARD: ROUTING AUTHORIZATION.","E / RELEASE THE DOWNSTREAM SHUTTERS.",0,stateId("freight_routing"));
 m.fixtures.push_back({25,{21.8f,13.9f},0,1.6f,1.6f,.75f,kPi*.5f,true});
 m.fixtures.push_back({27,{22.2f,13.8f},.75f,.42f,.4f,.38f,-kPi*.5f,false});
 m.fixtures.push_back({26,{20.4f,14.f},0,.55f,.55f,.95f,-kPi*.5f,true});
 m.lights.push_back(lamp({21.f,15.5f},ceil-.16f,.9f,6.f,4000));
 // --- Ceiling and light ------------------------------------------------------------------------
 ceilingGrid(m,1.f,0.f,23.f,23.f,ceil);
 for(float x:{4.f,9.6f,14.4f,19.2f})m.lights.push_back(lamp({x,1.8f},ceil-.16f,.8f,7.f,4000));
 for(float x:{4.f,9.6f,14.4f})for(float y:{14.f,20.4f})m.lights.push_back(lamp({x,y},ceil-.16f,.8f,7.f,4000));
 m.pickupSpawns.push_back({{12.f,13.f},PickupKind::Ammo,f});
 zone(m,"MANIFEST ROUTING / SWITCH ROOM",15.6f,3.6f,23,11.4f);
 freightFinish(m,21);
 return p;
}

// TRANSFER / CONVEYORS. The transfer hall between the manifest offices and
// the sorting floor. Line 1 runs east beside the top landing, line 2 west
// beside the middle landing and drops down a chute to line 3, which carries
// freight south along the floor to sorting. The personnel route follows the
// belts down three flights.
std::shared_ptr<const Map> makeTransferConveyors(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[22];m.definition=CampaignChunks[22];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.32f;
 const float base=-25,top=-7,mid=-13,low=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openSouth=true;
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,top-base});
 wall(m,10,0,14,1,top+2.5f,roof,3);wall(m,10,0,14,1,base,top-.22f,15);
 // --- Landings and flights ----------------------------------------------------------------
 deck(m,top,1,0,23,4);
 deck(m,mid,1,13,23,16);deck(m,mid,5,16,8,19);
 deck(m,low,17,16,23,24);
 m.stairs.push_back({1.5f,4.f,4.5f,13.f,mid,top,32,true,false});
 m.stairs.push_back({8.f,16.5f,17.f,19.f,low,mid,32,false,false});
 m.stairs.push_back({8.f,20.5f,17.f,23.f,base,low,32,false,true});
 guardRail(m,4.5f,4.f,23.f,4.06f,top);
 guardRail(m,4.5f,12.94f,23.f,13.f,mid);guardRail(m,1.f,15.94f,5.f,16.f,mid);guardRail(m,8.f,15.94f,23.f,16.f,mid);
 guardRail(m,5.f,16.f,5.06f,19.f,mid);guardRail(m,5.f,18.94f,8.f,19.f,mid);guardRail(m,7.94f,16.f,8.f,16.5f,mid);
 guardRail(m,17.f,16.f,23.f,16.06f,low);guardRail(m,17.f,16.f,17.06f,16.5f,low);guardRail(m,17.f,19.f,17.06f,20.5f,low);
 guardRail(m,17.f,23.f,17.06f,24.f,low);guardRail(m,17.f,23.94f,23.f,24.f,low);
 // --- The three lines -----------------------------------------------------------------------
 beltConveyor(m,5.f,22.8f,4.8f,top+.65f,roof);
 beltConveyor(m,5.6f,22.8f,12.2f,mid+.65f,roof);
 wall(m,4.6f,11.8f,5.6f,12.6f,base,mid+.6f,11);
 for(float z=base+1.f;z<mid;z+=2.f)wall(m,4.55f,11.75f,5.65f,12.65f,z,z+.1f,12);
 floorConveyor(m,5.1f,12.6f,24.f,base,base+.85f);
 machineryClock(m,"transfer_belts",14000);
 actor(m,"transfer_belts",ActorVisual::Cargo,.7f,{{0,{6.2f,4.8f},top+.65f,kPi*.5f},{9000,{21.6f,4.8f},top+.65f,kPi*.5f},{9001,{21.6f,4.8f},-40.f,0},{13999,{6.2f,4.8f},-40.f,0},{14000,{6.2f,4.8f},top+.65f,kPi*.5f}},true);
 actor(m,"transfer_belts",ActorVisual::Cargo,.7f,{{0,{21.6f,12.2f},-40.f,0},{3999,{21.6f,12.2f},-40.f,0},{4000,{21.6f,12.2f},mid+.65f,-kPi*.5f},{11000,{6.f,12.2f},mid+.65f,-kPi*.5f},{11001,{6.f,12.2f},-40.f,0},{14000,{21.6f,12.2f},-40.f,0}},true);
 actor(m,"transfer_belts",ActorVisual::Cargo,.7f,{{0,{5.1f,13.2f},base+.85f,0},{7000,{5.1f,23.6f},base+.85f,0},{7001,{5.1f,23.6f},-40.f,0},{13999,{5.1f,13.2f},-40.f,0},{14000,{5.1f,13.2f},base+.85f,0}},true);
 cue(m,"transfer_belts",100,Sound::Machine,{12.f,4.8f},"",.35f,top+.6f);
 cue(m,"transfer_belts",11000,Sound::JunkMetal,{5.1f,12.2f},"",.4f,mid);
 for(Vec2 d:{Vec2{21.4f,3.6f},Vec2{21.4f,14.4f}})m.fixtures.push_back({6,d,(d.y<5?top:mid)-base,1.2f,.6f,.7f,0,true});
 terminal(m,{21.f,22.f},"TRANSFER / CONVEYOR ROUTE","SORTING FLOOR BELOW / FOLLOW AMBER CLEARANCE.","DEPOT AND PLATFORMS: SOUTH RAIL SPUR.",low-base);
 // --- The floor: belt plant and stores -------------------------------------------------------
 m.fixtures.push_back({53,{16.f,8.f},0,4.53f,2.04f,2.74f,0,true});
 for(Vec2 d:{Vec2{19.4f,10.2f},Vec2{20.3f,10.2f},Vec2{19.85f,11.f},Vec2{12.4f,7.6f}})m.fixtures.push_back({49,d,0,.75f,.75f,1.08f,0,true});
 for(float y:{17.4f,18.4f,21.4f,22.4f})m.fixtures.push_back({13,{22.75f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});
 m.fixtures.push_back({6,{7.6f,13.6f},0,1.4f,.8f,.9f,kPi*.5f,true});
 cargo(m,9.f,6.f,base);cargo(m,10.4f,6.2f,base,false);cargo(m,9.2f,7.5f,base);
 for(float y:{14.f,17.f,20.f})m.fixtures.push_back({52,{3.8f,y},0,.5f,1.6f,.7f,0,true});
 wallBays(m,base,roof,4.f,false,false,true,true,12.f);
 // --- Roof and light -----------------------------------------------------------------------
 for(float y:{2.f,8.f,14.f,20.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 for(float x:{3.5f,17.5f})wallLamp(m,x,1.02f,top+2.3f,kPi*.5f,1.1f,7.f);
 for(float y:{14.5f})wallLamp(m,22.98f,y,mid+2.3f,kPi,1.2f,8.f);
 wallLamp(m,1.02f,14.5f,mid+2.3f,0,1.2f,8.f);
 for(float y:{18.f,22.f})wallLamp(m,22.98f,y,low+2.3f,kPi,1.1f,7.f);
 for(Vec2 l:{Vec2{8.f,9.f},Vec2{14.f,9.f},Vec2{20.f,7.f}})m.lights.push_back(lamp(l,-16.f,2.2f,14.f,4200,.1f));
 m.lights.push_back(lamp({20.f,20.f},low-.5f,1.3f,9.f,4000));
 for(Vec2 l:{Vec2{8.f,2.f},Vec2{16.f,2.f},Vec2{12.f,14.5f}})m.lights.push_back(lamp(l,roof-.95f,2.4f,14.f,4200,.1f));
 m.pickupSpawns.push_back({{12.f,14.5f},PickupKind::Ammo,mid});
 m.clutterSpawns.push_back({1,{14.f,12.f},base});
 deckSupports(m,m.layers[1],base,6.f);deckSupports(m,m.layers[2],base,6.f);deckSupports(m,m.layers[3],base,6.f);
 freightFinish(m,22);
 titleCard(m,"TRANSFER YARD");
 return p;
}

// TRANSFER / SORTING. Line 3 arrives from the conveyors and feeds the
// pusher lane, where a hydraulic ram drove each load onto its destination
// roller lane: depot, platform 3, deep freight. The ram still cycles; its
// isolation console is at the head of the lane. The sorting crane works over
// the lanes, and shipping containers wait along the east wall.
std::shared_ptr<const Map> makeTransferSorting(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[23];m.definition=CampaignChunks[23];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.32f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,true));m.openNorth=true;m.openSouth=true;
 // --- Line 3 into the pusher lane -----------------------------------------------------------
 floorConveyor(m,5.1f,0,6.2f,base,base+.85f);
 for(float s:{-.4f,.32f})wall(m,4.7f,6.2f+s+.4f,7.f,6.2f+s+.48f,base+.55f,base+.91f,12);
 wall(m,4.7f,6.2f,7.f,6.92f,base+.75f,base+.85f,8);m.structures.push_back({4.7f,6.2f,7.f,6.92f,base,base+.85f,false,8});
 // --- The pusher lane and its ram ---------------------------------------------------------
 m.compactors.push_back({7,7,11,15,base,-22.4f,8,stateId("sorting_gate_stop")});
 for(float x:{6.75f,11.05f}){
  wall(m,x,6.75f,x+.20f,15.3f,base,base+.25f,18);
  for(float y:{6.75f,15.1f})wall(m,x,y,x+.20f,y+.20f,base,-21.7f,12);
 }
 for(float y:{6.75f,15.1f})wall(m,6.75f,y,11.25f,y+.2f,-21.9f,-21.7f,18);
 wall(m,8.6f,6.75f,9.4f,15.3f,-21.9f,-21.6f,18);
 m.pipes.push_back({{9.f,11.f},{9.f,11.f},-21.8f,.18f,-20.6f,5});
 wall(m,8.7f,10.7f,9.3f,11.3f,-20.6f,-20.4f,12);
 for(float y=7.2f;y<15;y+=.8f)floorLine(m,6.2f,y,6.55f,y+.4f,base,13);
 terminal(m,{12.4f,5.6f},"SORTING / CARGO GATE","HYDRAULIC PUSHER: ACTIVE.","E / TOGGLE GATE ISOLATION.",0,stateId("sorting_gate_stop"));m.terminals.back().toggleState=true;
 wallLamp(m,6.74f,6.85f,-22.8f,kPi,1.1f,6.f);wallLamp(m,11.26f,6.85f,-22.8f,0,1.1f,6.f);
 // --- Destination roller lanes and pallet stations ---------------------------------------------
 const char* lanes[]={"DEPOT","PLATFORM 3","DEEP FREIGHT"};
 for(int i=0;i<3;++i){float y=8.f+i*3.f;
  for(float s:{-.55f,.47f})wall(m,11.3f,y+s,16.f,y+s+.08f,base+.6f,base+.78f,12);
  for(float x=11.45f;x<16;x+=.3f)wall(m,x,y-.47f,x+.07f,y+.47f,base+.66f,base+.72f,20);
  for(float x:{11.5f,15.6f})for(float s:{-.55f,.47f})wall(m,x,y+s,x+.08f,y+s+.08f,base,base+.6f,12);
  m.fixtures.push_back({15,{17.3f,y},0,1.2f,1.15f,.16f,0,true});
  if(i!=1)m.fixtures.push_back({17,{17.3f,y},.16f,1.1f,1.f,.9f,0,true});
  board(m,17.3f,y-1.25f,base+2.f,lanes[i],"DESTINATION LANE",1.6f);
 }
 // --- The sorting crane ----------------------------------------------------------------------
 machineryClock(m,"sorting_crane",18000);
 actor(m,"sorting_crane",ActorVisual::Cargo,1.2f,{{0,{13,8},-19,0},{4000,{13,8},-19,0},{10000,{13,17},-19,.35f},{14000,{13,17},-19,.35f},{18000,{13,8},-19,0}},true);
 m.actorTracks.back().suspensionTop=-17.5f;
 wall(m,12.8f,5,13.2f,21,-17.5f,-17.2f,12);
 for(float y:{5.f,21.f})wall(m,12.8f,y,13.2f,y+.3f,base,-17.2f,12);
 cue(m,"sorting_crane",5000,Sound::LiftCreak,{13,12},"",.5f);
 // --- Containers waiting along the east wall ---------------------------------------------------
 m.fixtures.push_back({48,{21.4f,7.2f},0,2.44f,10.8f,2.5f,0,true});
 m.fixtures.push_back({48,{21.4f,7.2f},2.5f,2.44f,10.8f,2.5f,0,true});
 m.fixtures.push_back({48,{21.4f,18.4f},0,2.44f,10.8f,2.5f,0,true});
 for(Vec2 d:{Vec2{18.6f,16.f},Vec2{18.6f,16.8f},Vec2{19.4f,16.4f}})m.fixtures.push_back({49,d,0,.75f,.75f,1.08f,0,true});
 // --- Sorting office, south-west -------------------------------------------------------------
 office(m,3.5f,19.f,base,"SORTING / LANE LOG","DEPOT: 41 / PLATFORM 3: 0 / DEEP FREIGHT: 3.","DEEP FREIGHT LOADS: CREW TO ACCOMPANY.");
 m.fixtures.push_back({18,{3.5f,18.15f},0,.417f,.525f,.95f,kPi,true});
 wallBays(m,base,roof,4.f,false,false,true,true);
 // --- Roof and light -----------------------------------------------------------------------
 for(float y:{2.f,8.f,14.f,20.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 servicePipe(m,{1.2f,4.f},{22.8f,4.f},-9.f,.35f,5);
 for(Vec2 l:{Vec2{6.f,4.f},Vec2{9.f,11.f},Vec2{16.f,11.f},Vec2{18.f,4.f},Vec2{8.f,19.f},Vec2{17.f,20.f}})m.lights.push_back(lamp(l,-10.f,2.8f,20.f,4200,.1f));
 wallLamp(m,22.98f,13.f,base+3.f,kPi,1.1f,7.f);
 m.creatureSpawns={{CreatureKind::Huntsman,{9.f,18.5f},base},{CreatureKind::Wasp,{18.6f,13.6f},base}};
 m.pickupSpawns.push_back({{18.6f,12.6f},PickupKind::Ammo,base});
 freightFinish(m,23);
 return p;
}

// TRANSFER / RAIL YARD. The underground freight yard: two running lines
// with wagons standing, a container on each; the traverser cradle that
// moved bogies between them; the weighbridge on line 2 and the signal box
// beside it. The door south leads to platform 3.
std::shared_ptr<const Map> makeTransferRailYard(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[24];m.definition=CampaignChunks[24];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.32f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,false));
 // Running lines end at buffer stops at both ends of the yard.
 for(float x:{7.f,17.f})trackRun(m,x,base);
 m.openNorth=true;rectangle(m.layers[0],1,0,23,1,'.');
 rectangle(m.layers[0],10,23,14,24,'.');m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,0});
 wall(m,10,23,14,24,base+2.5f,roof,3);
 wallSign(m,12.f,22.945f,base+4.2f,"PLATFORM 3","EMPLOYEE TRANSIT / NO FREIGHT",2.4f,kPi);
 // --- Wagons standing ------------------------------------------------------------------------
 flatWagon(m,7.f,2.1f,base);
 flatWagon(m,17.f,3.f,base);
 // --- The weighbridge on line 2 and the signal box ----------------------------------------------
 wall(m,15.6f,16.f,18.4f,20.f,base,base+.02f,8);
 for(float y:{16.f,19.88f})floorLine(m,15.6f,y,18.4f,y+.12f,base+.02f,13);
 office(m,20.2f,18.f,base,"TRANSFER / SIGNAL ROUTING","DEPOT / PLATFORMS / DEEP FREIGHT.","PLATFORM 3: SERVICE ACCESS ONLY.",0,true);
 m.fixtures.push_back({18,{20.2f,17.15f},0,.417f,.525f,.95f,kPi,true});
 for(float x:{4.5f,19.5f}){
  wall(m,x-.08f,21.4f,x+.08f,21.56f,base,base+3.2f,12);
  wall(m,x-.2f,21.3f,x+.2f,21.6f,base+3.2f,base+3.9f,8);
  wallLamp(m,x,21.28f,base+3.65f,-kPi*.5f,.6f,4.f);
 }
 // --- The traverser cradle ---------------------------------------------------------------------
 machineryClock(m,"yard_cradle",20000);
 actor(m,"yard_cradle",ActorVisual::Cargo,1.f,{{0,{4,16},-24.7f,0},{3000,{4,16},-24.7f,0},{10000,{11,16},-24.7f,0},{13000,{11,16},-24.7f,0},{20000,{4,16},-24.7f,0}},true);
 m.actorTracks.back().platform=true;m.actorTracks.back().footprint={2.2f,2.2f};m.actorTracks.back().suspensionTop=-17.7f;
 wall(m,2.8f,15.9f,12.2f,16.1f,-18,-17.7f,12);
 for(float x:{2.8f,12.f})wall(m,x,15.7f,x+.2f,16.3f,base,-17.7f,12);
 board(m,5.f,18.6f,base+2.2f,"TRAVERSER CRADLE","AUTOMATIC TRANSFER / STAND CLEAR",2.4f);
 for(float x=3.2f;x<12;x+=.6f)floorLine(m,x,14.6f,x+.3f,14.75f,base,13);
 // --- Yard stores along the west wall --------------------------------------------------------
 m.fixtures.push_back({48,{2.45f,8.6f},0,2.44f,10.8f,2.5f,0,true});
 m.fixtures.push_back({48,{2.45f,8.6f},2.5f,2.44f,10.8f,2.5f,0,true});
 for(float y:{19.4f,20.6f})m.fixtures.push_back({52,{2.2f,y},0,1.6f,.5f,.7f,0,true});
 for(Vec2 d:{Vec2{11.f,20.4f},Vec2{11.8f,20.8f},Vec2{11.f,21.4f}})m.fixtures.push_back({49,d,0,.75f,.75f,1.08f,0,true});
 for(float x:{14.4f,14.8f,15.2f})m.fixtures.push_back({54,{x,22.6f},0,.33f,.33f,1.58f,0,true});
 wall(m,14.1f,22.85f,15.5f,22.95f,base+1.f,base+1.08f,12);
 wallBays(m,base,roof,4.f,false,true,true,true,12.f);
 // --- Roof and light -----------------------------------------------------------------------
 for(float y:{2.f,8.f,14.f,20.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 for(Vec2 l:{Vec2{7.f,6.f},Vec2{17.f,6.f},Vec2{7.f,13.f},Vec2{17.f,13.f},Vec2{12.f,19.f}})m.lights.push_back(lamp(l,-10.f,2.8f,20.f,4200,.1f));
 for(float y:{6.f,14.f})wallLamp(m,22.98f,y,base+3.5f,kPi,1.2f,8.f);
 m.lights.push_back(lamp({8.f,22.f},base+3.f,1.f,6.f,3800));
 m.pickupSpawns.push_back({{12.f,10.f},PickupKind::Health,base});
 freightFinish(m,24);
 return p;
}

// EMPTY PLATFORM. Platform 3, the employee transit station. An island
// platform between two lines, reached by steps from the door. Train 27
// arrived at 02:13; nothing has departed. Benches, a vending machine, the
// departure board and the emergency phone; the lines run on south.
std::shared_ptr<const Map> makeEmptyPlatform(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[25];m.definition=CampaignChunks[25];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.36f;
 const float base=-25,deckZ=-23.9f,ceil=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openSouth=true;
 for(float x:{5.f,19.f})trackRun(m,x,base);
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,0});
 wall(m,10,0,14,1,base+2.5f,ceil,3);
 // --- Island platform: concrete body, steps, coping and tactile edges ---------------------------
 wall(m,9.f,5.f,15.f,24.f,base,deckZ-.22f,15);
 deck(m,deckZ,9,5,15,24);
 m.stairs.push_back({9.f,3.f,15.f,5.f,base,deckZ,6,true,true});
 for(float x:{9.f,14.55f})wall(m,x,5.f,x+.45f,24.f,deckZ,deckZ+.01f,18);
 for(float x:{8.85f,15.f})wall(m,x,5.f,x+.15f,24.f,base,deckZ+.01f,20);
 for(float x:{9.f,14.8f})wall(m,x,3.f,x+.2f,5.f,base,base+1.05f,12);
 // --- Platform columns, benches, the vending machine ------------------------------------------
 for(float y:{8.f,14.f,20.f})for(float x:{10.6f,13.4f})wall(m,x-.2f,y-.2f,x+.2f,y+.2f,deckZ,ceil,15);
 for(float y:{11.f,17.f,22.f})m.fixtures.push_back({51,{12.f,y},deckZ-base,1.86f,.44f,.63f,kPi*.5f,true});
 m.fixtures.push_back({62,{14.1f,6.6f},deckZ-base,1.5f,.68f,2.38f,kPi*.5f,true});
 m.fixtures.push_back({49,{10.1f,6.4f},deckZ-base,.55f,.55f,.8f,0,true});
 terminal(m,{12,3},"PLATFORM 3 / DEPARTURES","TRAIN 27 / ARRIVED 02:13.","NO DEPARTURE RECORDED / TRACK CLEAR.");
 m.fixtures.push_back({8,{13.62f,14.f},deckZ-base+1.1f,.55f,.12f,.8f,kPi*.5f,false});
 terminal(m,{14.f,15.2f},"PLATFORM / EMERGENCY PHONE","DISPATCH LINE OPEN / NO OPERATOR RESPONSE.","HOLD THE PLATFORM. DO NOT ENTER THE TRACK.",deckZ-base);
 board(m,12.f,7.6f,-21.f,"PLATFORM 3","TRAIN 27 / ARRIVED 02:13",3.6f);
 // --- Walls across the lines --------------------------------------------------------------------
 wallSign(m,12.f,1.055f,-20.f,"WAY OUT","FREIGHT YARD / MANIFEST OFFICES",2.4f,0);
 for(float y:{4.f,20.f}){wallLamp(m,1.02f,y,-21.5f,0,1.1f,8.f);wallLamp(m,22.98f,y,-21.5f,kPi,1.1f,8.f);}
 // --- Ceiling and light ----------------------------------------------------------------------
 for(float y:{2.f,8.f,14.f,20.f})wall(m,1.f,y-.15f,23.f,y+.15f,ceil-.5f,ceil,12);
 for(float y:{5.f,11.f,17.f,23.f})m.lights.push_back(lamp({12.f,y},ceil-.65f,1.6f,10.f,4600,.05f));
 m.lights.push_back(lamp({12.f,1.6f},ceil-.2f,1.f,6.f,3800));
 freightFinish(m,25);
 titleCard(m,"EMPTY PLATFORM");
 return p;
}

// PLATFORM / SERVICE END. The south end of platform 3. The island platform
// ends at a staff stair down to track level, where a boarded crossing spans
// both lines; the security office watches the end of the platform; both
// lines run on through shuttered portals into the depot, and the personnel
// door between them leads there too.
std::shared_ptr<const Map> makePlatformServiceEnd(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[26];m.definition=CampaignChunks[26];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.34f;
 const float base=-25,deckZ=-23.9f,ceil=m.definition.ceiling;m.definition.spawnHeight=deckZ;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));
 m.openNorth=true;m.openSouth=true;for(float x:{5.f,19.f})trackRun(m,x,base);m.openSouth=false;
 for(float x:{5.f,19.f})trackPortal(m,x,22.8f,base,1);
 rectangle(m.layers[0],10,23,14,24,'.');m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,0});
 wall(m,10,23,14,24,base+2.5f,ceil,3);
 // --- The end of the island platform and its staff stair ----------------------------------
 wall(m,9.f,0,15.f,13.f,base,deckZ-.22f,15);
 deck(m,deckZ,9,0,15,13);
 for(float x:{9.f,14.55f})wall(m,x,0,x+.45f,13.f,deckZ,deckZ+.01f,18);
 for(float x:{8.85f,15.f})wall(m,x,0,x+.15f,13.f,base,deckZ+.01f,20);
 m.stairs.push_back({10.5f,13.f,13.5f,15.f,base,deckZ,6,true,false});
 guardRail(m,9.f,12.94f,10.5f,13.f,deckZ);guardRail(m,13.5f,12.94f,15.f,13.f,deckZ);
 for(float x:{10.3f,13.5f})wall(m,x,13.f,x+.2f,15.f,base,base+1.05f,12);
 for(float y:{2.f,8.f})for(float x:{10.6f,13.4f})wall(m,x-.2f,y-.2f,x+.2f,y+.2f,deckZ,ceil,15);
 m.fixtures.push_back({61,{12.f,5.f},deckZ-base,2.27f,.54f,.95f,kPi*.5f,true});
 m.fixtures.push_back({59,{12.f,10.6f},deckZ-base,.6f,.6f,.76f,0,true});
 m.fixtures.push_back({60,{12.f,8.24f},deckZ-base+1.f,1.29f,.08f,1.58f,0,false});
 wall(m,11.3f,8.2f,12.7f,8.22f,deckZ+.95f,deckZ+2.62f,2);
 board(m,12.f,11.6f,-21.f,"END OF PLATFORM","NO PUBLIC ACCESS BEYOND THIS POINT",3.f);
 // --- Track level: crossing, security office ----------------------------------------------------
 for(float x=2.f;x<22.f;x+=.5f)if(x<9.f||x>15.f)wall(m,x,16.f,x+.4f,17.6f,base+.07f,base+.11f,10);
 for(float y:{15.9f,17.6f})floorLine(m,1.f,y,9.f,y+.1f,base,13),floorLine(m,15.f,y,23.f,y+.1f,base,13);
 office(m,10.5f,19.5f,base,"PLATFORM / SECURITY RECORD","02:12:51 ... 02:19:04 / BLOCK MISSING.","EMPLOYEE TRANSIT: ROLLING STOCK DEPOT.",0,true);
 m.fixtures.push_back({18,{10.5f,18.65f},0,.417f,.525f,.95f,kPi,true});
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i%4,{15.6f+float(i%3)*.8f,19.4f+float(i/3)},base});
 m.fixtures.push_back({58,{16.2f,21.9f},0,.97f,.41f,2.24f,kPi,true});
 // --- Walls across the lines, ceiling and light -------------------------------------------------
 wallSign(m,22.945f,8.f,-22.2f,"MIND THE GAP","STAND BEHIND THE YELLOW LINE",4.f,-kPi*.5f);
 for(float y:{4.f,14.f}){wallLamp(m,1.02f,y,-21.5f,0,1.1f,8.f);wallLamp(m,22.98f,y,-21.5f,kPi,1.1f,8.f);}
 for(float y:{2.f,8.f,14.f,20.f})wall(m,1.f,y-.15f,23.f,y+.15f,ceil-.5f,ceil,12);
 for(float y:{5.f,11.f})m.lights.push_back(lamp({12.f,y},ceil-.65f,1.6f,10.f,4600,.05f));
 for(Vec2 l:{Vec2{5.f,18.f},Vec2{19.f,18.f},Vec2{12.f,18.f}})m.lights.push_back(lamp(l,ceil-.65f,1.4f,9.f,4200,.05f));
 freightFinish(m,26);
 return p;
}

// DEPOT / TRACKSIDE. The arrival road of the rolling-stock depot. Both lines
// come in through the shuttered portals on raised track beds; Train 27
// stands on line 1, its doors shut, where its driver tried to take it out.
// A loaded flat stands on line 2. Between the beds a sunken service road
// runs south past the maintenance stores to the inspection pits.
std::shared_ptr<const Map> makeDepotTrackside(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[27];m.definition=CampaignChunks[27];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.3f;
 const float base=-28,bed=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openSouth=true;
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,bed-base});
 wall(m,10,0,14,1,bed+2.5f,roof,3);wall(m,10,0,14,1,base,bed-.22f,15);
 // --- Track beds, the north landing and the stair to the service road -------------------------
 deck(m,bed,1,0,9,24);deck(m,bed,15,0,23,24);deck(m,bed,9,0,15,4);
 m.openNorth=true;for(float x:{5.f,19.f})trackRun(m,x,bed);m.openNorth=false;
 for(float x:{5.f,19.f})trackPortal(m,x,1.f,bed,0);
 m.stairs.push_back({10.5f,4.f,13.5f,9.f,base,bed,16,true,false});
 guardRail(m,9.f,4.f,10.5f,4.06f,bed);guardRail(m,13.5f,4.f,15.f,4.06f,bed);
 guardRail(m,8.94f,4.f,9.f,24.f,bed);guardRail(m,15.f,4.f,15.06f,24.f,bed);
 for(float x:{8.85f,15.f})wall(m,x,4.f,x+.15f,24.f,base,bed,15);
 // --- Train 27 on line 1 ---------------------------------------------------------------------
 m.fixtures.push_back({56,{5.f,12.6f},bed-base+.05f,3.83f,22.4f,5.33f,0,true});
 for(float y:{2.f,6.f,10.f,14.f,18.f,22.f})wallLamp(m,1.02f,y,bed+3.2f,0,1.5f,9.f);
 for(float y:{6.f,18.f})m.lights.push_back(lamp({7.6f,y},bed+6.5f,2.f,10.f,4600,.05f));
 // --- A loaded flat on line 2 and the maintenance log ----------------------------------------------
 flatWagon(m,19.f,6.f,bed);
 terminal(m,{22.f,20.f},"DEPOT / MAINTENANCE LOG","ENGINE 04: DRIVER ATTEMPTED EVACUATION.","VEHICLE STOPPED AT THE BARRICADE.",bed-base);
 m.fixtures.push_back({13,{22.75f,22.4f},bed-base,.9066f,.4956f,2.2f,kPi*.5f,true});
 // --- The service road: stores, carts and the way south -------------------------------------------
 for(float y:{11.f,12.3f})m.fixtures.push_back({7,{9.3f,y},0,1.2f,.45f,1.8f,kPi*.5f,true});
 for(Vec2 d:{Vec2{14.4f,10.f},Vec2{14.4f,10.8f},Vec2{13.7f,10.4f}})m.fixtures.push_back({49,d,0,.75f,.75f,1.08f,0,true});
 for(float x:{13.9f,14.3f})m.fixtures.push_back({54,{x,16.2f},0,.33f,.33f,1.58f,0,true});
 handCart(m,10.2f,18.f,base);cargo(m,13.8f,21.f,base);
 for(float y=5.f;y<24;y+=2.f)floorLine(m,11.95f,y,12.05f,y+1.f,base,14);
 for(float y:{9.f,17.f}){wallLamp(m,9.02f,y,base+2.4f,0,1.f,6.f);wallLamp(m,14.98f,y,base+2.4f,kPi,1.f,6.f);}
 // --- Roof and light -----------------------------------------------------------------------
 wallBays(m,base,roof,4.f,false,false,true,true);
 for(float y:{4.f,10.f,16.f,22.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 servicePipe(m,{1.2f,7.f},{22.8f,7.f},-10.f,.35f,5);
 for(Vec2 l:{Vec2{5.f,6.f},Vec2{5.f,18.f},Vec2{19.f,6.f},Vec2{19.f,18.f},Vec2{12.f,12.f}})m.lights.push_back(lamp(l,-12.f,2.6f,18.f,4000,.1f));
 m.creatureSpawns={{CreatureKind::Mutant,{20.5f,19.5f},bed}};
 m.pickupSpawns.push_back({{12.f,14.f},PickupKind::Ammo,base});
 freightFinish(m,27);
 titleCard(m,"ROLLING STOCK DEPOT");
 return p;
}

// DEPOT / INSPECTION PITS. Line 1 ends at the barricade the depot staff
// threw across it; Train 27 stopped short of it. Line 2 runs over the
// inspection pit where Engine 02 stands stripped on its jacks, beside the
// tool bay. The service road ends at a stair to the track beds and a second
// flight climbs to the gantry walk south.
std::shared_ptr<const Map> makeDepotInspectionPits(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[28];m.definition=CampaignChunks[28];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.38f;
 const float base=-28,bed=-25,walk=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,true));m.openNorth=true;m.openSouth=true;
 deck(m,bed,1,0,9,24);deck(m,bed,15,0,23,24);deck(m,bed,9,10,15,24);
 for(float x:{5.f,19.f})trackRun(m,x,bed);
 for(float x:{8.85f,15.f})wall(m,x,0,x+.15f,10.f,base,bed,15);wall(m,9.f,9.85f,15.f,10.f,base,bed,15);
 // --- Stairs: service road to the beds, then up to the gantry walk ---------------------------------
 m.stairs.push_back({12.f,5.f,14.5f,10.f,base,bed,16,true,true});
 m.stairs.push_back({9.f,12.f,12.f,21.f,bed,walk,32,true,true});
 deck(m,walk,8,21,16,24);
 guardRail(m,8.94f,0,9.f,10.f,bed);guardRail(m,15.f,0,15.06f,10.f,bed);guardRail(m,9.f,10.f,12.f,10.06f,bed);guardRail(m,14.5f,10.f,15.f,10.06f,bed);
 guardRail(m,8.f,21.f,9.f,21.06f,walk);guardRail(m,12.f,21.f,16.f,21.06f,walk);guardRail(m,8.f,21.f,8.06f,24.f,walk);guardRail(m,15.94f,21.f,16.f,24.f,walk);
 // --- The barricade across line 1 ------------------------------------------------------------
 for(float x:{3.6f,5.f,6.4f})m.fixtures.push_back({52,{x,1.6f},bed-base,1.4f,.55f,.8f,0,true});
 for(Vec2 c:{Vec2{3.2f,3.f},Vec2{7.f,3.2f}})cargo(m,c.x,c.y,bed);
 m.fixtures.push_back({17,{5.f,3.f},bed-base,1.f,1.1f,1.f,.4f,true});
 forklift(m,2.3f,7.f,bed);
 board(m,5.f,4.6f,bed+2.f,"LINE 1 CLOSED","DEPOT LOCKDOWN / NO MOVEMENTS",2.6f);
 // --- Line 2 over the inspection pit: Engine 02 on its jacks ------------------------------------
 for(float x:{18.f,19.6f})wall(m,x,3.f,x+.4f,19.f,base,bed,15);
 for(float x:{18.f,19.f})for(int y=3;y<19;++y)rectangle(m.layers[1],int(x),y,int(x)+1,y+1,'.');
 locomotive(m,19.f,6.f,bed,true);
 for(float x:{17.4f,20.6f})for(float y:{6.6f,12.4f}){wall(m,x-.2f,y-.2f,x+.2f,y+.2f,bed,bed+.1f,8);wall(m,x-.07f,y-.07f,x+.07f,y+.07f,bed+.1f,bed+.72f,18);}
 for(float y:{5.f,9.f,13.f,17.f})wallLamp(m,18.42f,y,base+1.6f,0,1.f,4.f);
 board(m,21.6f,2.4f,bed+2.f,"INSPECTION PIT","JACKS ENGAGED / DO NOT LOWER",2.f);
 // --- The tool bay ---------------------------------------------------------------------------
 for(float y:{6.f,10.f})m.fixtures.push_back({19,{22.2f,y},bed-base,1.367f,.724f,.78f,kPi*.5f,true});
 m.fixtures.push_back({22,{22.4f,14.f},bed-base,.718f,.453f,1.2f,-kPi*.5f,true});
 m.fixtures.push_back({12,{21.8f,17.4f},bed-base,1.8f,1.4f,1.6f,0,true});
 m.fixtures.push_back({55,{21.5f,21.4f},bed-base,2.2f,2.8f,2.9f,0,true});
 for(float y:{5.5f,9.f,12.5f})m.fixtures.push_back({7,{1.6f,y+8.f},bed-base,1.4f,.5f,1.8f,-kPi*.5f,true});
 // --- Roof and light -----------------------------------------------------------------------
 wallBays(m,base,roof,4.f,false,false,true,true);
 for(float y:{4.f,10.f,16.f,22.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 for(Vec2 l:{Vec2{5.f,6.f},Vec2{19.f,4.f},Vec2{12.f,6.f},Vec2{5.f,17.f},Vec2{19.f,20.f},Vec2{12.f,17.f}})m.lights.push_back(lamp(l,-12.f,2.6f,18.f,4000,.1f));
 for(float y:{6.f,18.f})wallLamp(m,22.98f,y,bed+3.f,kPi,1.2f,8.f);
 m.pickupSpawns.push_back({{12,3},PickupKind::Ammo,base});
 deckSupports(m,m.layers[2],base,6.f);
 freightFinish(m,28);
 return p;
}

// DEPOT / GANTRIES. The loading end of the depot. Gantry walks run the
// length of the hall either side, six metres up, under the crane runway
// where something moves. Freight flats stand on line 2 with their loads
// still chained. A stair drops from the west walk to the floor and the door
// south leads to the maintenance bore.
std::shared_ptr<const Map> makeDepotGantries(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[29];m.definition=CampaignChunks[29];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.3f;
 const float base=-25,walk=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));
 m.openNorth=true;m.openSouth=true;for(float x:{5.f,19.f})trackRun(m,x,base);m.openSouth=false;
 for(float x:{5.f,19.f})trackPortal(m,x,22.8f,base,1);
 rectangle(m.layers[0],10,23,14,24,'.');m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,0});
 wall(m,10,23,14,24,base+2.5f,roof,3);
 wallSign(m,12.f,22.945f,base+4.2f,"DEEP CARGO","MAINTENANCE BORE / RESTRICTED",2.4f,kPi);
 terminal(m,{12,21.6f},"DEEP CARGO ROUTE","POWER DISTRIBUTION / RESTRICTED INDUSTRIAL ACCESS.","BEYOND THIS POINT: NO EMPLOYEE TRANSIT.");
 // --- Gantry walks ---------------------------------------------------------------------------
 deck(m,walk,1,0,23,4);deck(m,walk,1,4,8,22);deck(m,walk,16,4,23,22);
 m.stairs.push_back({1.2f,4.f,4.f,13.f,base,walk,32,true,true});
 guardRail(m,8.f,4.f,16.f,4.06f,walk);guardRail(m,7.94f,4.f,8.f,22.f,walk);guardRail(m,16.f,4.f,16.06f,22.f,walk);
 guardRail(m,1.f,21.94f,8.f,22.f,walk);guardRail(m,16.f,21.94f,23.f,22.f,walk);guardRail(m,4.f,4.f,4.06f,13.f,walk);guardRail(m,1.f,3.94f,4.f,4.f,walk);
 // --- Crane runway, the bridge and what moves along it --------------------------------------------
 for(float x:{1.f,22.6f})wall(m,x,4.f,x+.4f,22.f,-12.6f,-12.2f,12);
 for(float x:{1.f,22.6f})for(float y=5.f;y<22;y+=4.f)wall(m,x,y-.15f,x+.4f,y+.15f,-13.4f,-12.6f,12);
 wall(m,1.4f,9.6f,22.6f,10.4f,-12.2f,-11.4f,18);
 wall(m,11.4f,9.4f,12.6f,10.6f,-12.8f,-12.2f,12);
 m.pipes.push_back({{12.f,10.f},{12.f,10.f},-17.f,.025f,-12.8f,6});wall(m,11.85f,9.85f,12.15f,10.15f,-17.4f,-17.f,8);
 machineryClock(m,"depot_roof_movement",24000);
 actor(m,"depot_roof_movement",ActorVisual::Huntsman,1.5f,{{0,{19,6},-16,0},{6000,{19,6},-16,0},{8500,{19,14},-16,kPi*.5f},{11000,{19,17},walk,kPi*.5f},{17000,{19,17},walk,-kPi*.5f},{19500,{19,14},-16,-kPi*.5f},{22000,{19,6},-16,-kPi*.5f},{24000,{19,6},-16,0}});
 wall(m,18.6f,5.7f,19.4f,14.5f,-16.2f,-16,12);for(float yy:{6.f,14.f})wall(m,19.3f,yy,19.5f,yy+.15f,walk,-16,12);
 cue(m,"depot_roof_movement",7000,Sound::JunkMetal,{19,12},"",.8f);
 // --- Freight flats on line 2, the line 1 run-off -----------------------------------------------
 flatWagon(m,19.f,.4f,base);
 for(Vec2 c:{Vec2{21.6f,15.f},Vec2{21.6f,16.4f},Vec2{16.4f,15.4f}})cargo(m,c.x,c.y,base);
 cargo(m,16.4f,17.f,base,false);
 m.fixtures.push_back({48,{8.6f,16.f},0,2.44f,10.8f,2.5f,0,true});
 for(Vec2 d:{Vec2{14.4f,6.f},Vec2{14.4f,6.8f},Vec2{13.6f,6.4f}})m.fixtures.push_back({49,d,0,.75f,.75f,1.08f,0,true});
 // --- Roof and light -----------------------------------------------------------------------
 wallBays(m,base,roof,4.f,false,true,true,true,12.f);
 for(float y:{4.f,10.f,16.f,22.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.8f,roof,12);
 for(Vec2 l:{Vec2{12.f,6.f},Vec2{12.f,16.f},Vec2{5.f,10.f},Vec2{19.f,10.f}})m.lights.push_back(lamp(l,-10.f,2.6f,18.f,4000,.1f));
 for(float y:{10.f,18.f}){wallLamp(m,1.02f,y,walk+2.4f,0,1.1f,7.f);wallLamp(m,22.98f,y,walk+2.4f,kPi,1.1f,7.f);}
 for(Vec2 l:{Vec2{4.f,8.f},Vec2{20.f,8.f},Vec2{20.f,18.f}})m.lights.push_back(lamp(l,walk-.5f,1.2f,8.f,4000));
 m.creatureSpawns={{CreatureKind::Huntsman,{20,15},walk}};
 deckSupports(m,m.layers[1],base,6.f);
 freightFinish(m,29);
 return p;
}

// CARGO / MAINTENANCE BORE. Tunnel 4A, twin bores split by a centre wall.
// The east bore carries the freight main line; its drain has backed up
// across the north end and, further south, the lining has come down over
// the line. The west bore holds the closed service line, the signal room and
// the bypass: the cross passage between the bores is the way round.
std::shared_ptr<const Map> makeMaintenanceBore(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[30];m.definition=CampaignChunks[30];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.24f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,false,true));m.openSouth=true;
 rectangle(m.layers[0],10,0,14,1,'.');m.doors.push_back({10.5f,13.5f,.5f,0,false,false,true,0});
 wall(m,10,0,14,1,base+2.5f,roof,3);
 // --- The bores and the cross passage ----------------------------------------------------
 wall(m,9.f,1.f,9.2f,10.f,base,roof,15);wall(m,9.f,14.f,9.2f,24.f,base,roof,15);wall(m,9.f,10.f,9.2f,14.f,base+2.6f,roof,15);
 for(float x:{8.9f,9.2f})wall(m,x,9.8f,x+.1f,10.f,base,base+2.6f,12),wall(m,x,14.f,x+.1f,14.2f,base,base+2.6f,12);
 wallSign(m,8.945f,12.f,base+2.7f,"CROSS PASSAGE 4A-3","BYPASS / SIGNAL ROOM",1.8f,-kPi*.5f);
 tunnelRibs(m,1.f,9.f,base,roof,3.f,24.f,4.f);tunnelRibs(m,9.2f,23.f,base,roof,3.f,24.f,4.f);
 m.openNorth=true;for(float x:{5.f,19.f})trackRun(m,x,base);m.openNorth=false;
 for(float x:{5.f,19.f})trackPortal(m,x,1.f,base,0);
 // --- The flooded drain, east bore ------------------------------------------------------------
 m.waterVolumes.push_back({9.4f,5.f,15.6f,11.f,-25.5f,-25.15f});
 for(float y:{4.75f,11.f})wall(m,9.2f,y,15.6f,y+.25f,base,base+.1f,15);
 for(float y:{4.6f,11.3f})floorLine(m,9.2f,y,15.6f,y+.15f,base,13);
 for(float x=10.f;x<15.4f;x+=1.f)wall(m,x,4.82f,x+.5f,4.94f,base+.1f,base+.12f,2);
 // --- The collapse over the main line ---------------------------------------------------------
 wall(m,13.6f,15.2f,17.2f,17.6f,base,base+.9f,15);
 wall(m,15.4f,15.6f,19.6f,17.2f,base+.9f,base+1.6f,15);
 wall(m,17.6f,14.8f,22.8f,18.2f,base,base+1.4f,15);
 wall(m,19.2f,15.4f,22.4f,17.4f,base+1.4f,base+2.3f,15);
 wall(m,13.2f,16.55f,22.8f,16.8f,base+.9f,base+1.15f,12);
 for(Vec2 b:{Vec2{12.4f,14.6f},Vec2{18.2f,19.f},Vec2{21.f,13.8f}})wall(m,b.x-.35f,b.y-.3f,b.x+.35f,b.y+.3f,base,base+.4f,15);
 wall(m,13.f,14.6f,23.f,19.f,roof-.08f,roof,8);
 board(m,15.f,12.6f,base+2.f,"LINE BLOCKED","ROOF FAILURE / USE CROSS PASSAGE",2.6f);
 for(Vec2 r:{Vec2{12.8f,13.6f},Vec2{16.f,13.6f}})m.fixtures.push_back({52,r,0,1.6f,.5f,.7f,0,true});
 // --- West bore: signal room, cable runs, stores ----------------------------------------------------
 office(m,11.f,19.f,base,"SIGNAL ROOM / TUNNEL 4A","MAIN LINE CLOSED / PARALLEL BORE AVAILABLE.","POWER DISTRIBUTION: DEEP JUNCTION.",0,true);
 m.fixtures.push_back({18,{11.f,18.15f},0,.417f,.525f,.95f,kPi,true});
 m.fixtures.push_back({13,{8.5f,6.f},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
 for(Vec2 d:{Vec2{6.8f,3.f},Vec2{7.6f,3.f},Vec2{7.2f,3.7f}})m.fixtures.push_back({49,d,0,.75f,.75f,1.08f,0,true});
 for(float z:{-22.8f,-22.5f,-22.2f})servicePipe(m,{22.75f,1.2f},{22.75f,24.f},z,.045f,2);
 for(float y=2.f;y<24;y+=4.f)wall(m,22.62f,y,23.f,y+.07f,-23.f,-22.f,2);
 cableTray(m,{1.4f,1.4f},{1.4f,24.f},base+3.4f);
 servicePipe(m,{1.2f,6.f},{8.95f,6.f},roof-1.f,.08f,3);
 // --- Light ------------------------------------------------------------------------------------
 for(float y:{5.f,13.f,21.f}){wallLamp(m,22.98f,y,base+3.8f,kPi,1.1f,8.f);wallLamp(m,1.02f,y,base+3.8f,0,.9f,7.f);}
 for(float y:{6.f,20.f})m.lights.push_back(lamp({13.f,y},roof-.7f,1.2f,9.f,4000,.05f));
 m.lights.push_back(lamp({19.f,12.f},roof-.7f,1.f,8.f,2600,-.2f));
 m.pickupSpawns.push_back({{6.f,12.f},PickupKind::Health,base});
 freightFinish(m,30);
 titleCard(m,"CARGO TUNNELS");
 return p;
}

// CARGO / DEEP JUNCTION. Where Tunnel 4A meets the main line east. The
// service line ends at a buffer; past it the signal alcove for block 4A,
// its door forced. In the east bore the main line runs into the sealed blast
// door of the eastern main, and between the bores stands the junction
// substation that powers the deep freight route.
std::shared_ptr<const Map> makeDeepJunction(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[31];m.definition=CampaignChunks[31];m.definition.musicCue=MusicCue::Freight;m.definition.ambient=.24f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base,true,false));m.openNorth=true;
 // --- The signal alcove (first control in this map) -----------------------------------------
 wall(m,2.1f,9.3f,2.25f,12.1f,base,-22.2f,3);
 wall(m,6.1f,9.3f,6.25f,12.1f,base,-22.2f,3);
 wall(m,2.1f,9.3f,6.25f,9.45f,base,-22.2f,3);
 wall(m,2.1f,11.9f,4,12.1f,base,-22.2f,3);wall(m,6,11.9f,6.25f,12.1f,base,-22.2f,3);
 wall(m,4,11.9f,6,12.1f,-22.6f,-22.2f,2);
 wall(m,2.1f,9.3f,6.25f,12.1f,-22.2f,-22.05f,2);
 terminal(m,{3.25f,10.55f},"SIGNAL / BLOCK 4A","BOTH TRACK CIRCUITS OCCUPIED / NO VEHICLE LOGGED.","SERVICE OVERRIDE REMAINS ISOLATED.");
 m.terminals.back().yaw=0;
 {Door forced{4,6,12,0,true,false,false};forced.swinging=true;forced.open=.8f;forced.opening=true;m.doors.push_back(forced);}
 m.lights.push_back({{3.4f,10.55f},-22.65f});
 // --- The bores ----------------------------------------------------------------------------------
 wall(m,9.f,0,9.2f,14.f,base,roof,15);wall(m,9.f,18.f,9.2f,23.f,base,roof,15);wall(m,9.f,14.f,9.2f,18.f,base+2.6f,roof,15);
 tunnelRibs(m,1.f,9.f,base,roof,3.f,23.f,4.f);tunnelRibs(m,9.2f,23.f,base,roof,3.f,23.f,4.f);
 // The service line ends at a buffer short of the alcove.
 rail(m,5.f,0,7.4f,base);
 for(float dx:{-.7f,.7f})wall(m,5.f+dx-.06f,7.3f,5.f+dx+.06f,7.4f,base,base+1.15f,12);
 wall(m,4.f,7.4f,6.f,7.65f,base+.7f,base+1.f,10);wall(m,4.f,7.38f,6.f,7.4f,base+.72f,base+.98f,13);
 m.structures.push_back({4.f,6.5f,6.f,7.65f,base,base+1.15f,false,6});
 // The main line runs on through the blast door into Power Distribution. The
 // door stays shut until the junction substation releases it.
 m.openNorth=true;m.openSouth=true;trackRun(m,19.f,base);m.openSouth=false;
 rectangle(m.layers[0],16,23,22,24,'.');
 {Door blast{16.5f,21.5f,23.5f,0,false,true,false,0};blast.requireState=stateId("pwr_blast_release");m.doors.push_back(blast);}
 for(float px:{16.f,21.6f})wall(m,px,22.4f,px+.4f,23.f,base,base+5.6f,18);
 wall(m,16.f,22.4f,22.f,23.f,base+2.65f,base+5.6f,18);
 for(float z=base+2.9f;z<base+5.f;z+=.8f)wall(m,16.4f,22.55f,21.6f,22.6f,z,z+.08f,13);
 wallSign(m,12.f,22.945f,base+3.2f,"MAIN LINE EAST","BLAST DOOR / POWER DISTRIBUTION",2.4f,kPi);
 // --- Junction substation, between the bores at the south ---------------------------------------
 for(float x:{10.f,11.8f})m.fixtures.push_back({13,{x,22.5f},0,.9066f,.4956f,2.2f,kPi,true});
 m.fixtures.push_back({53,{12.4f,19.6f},0,3.f,1.4f,1.8f,kPi*.5f,true});
 guardRail(m,9.2f,17.6f,10.6f,17.66f,base);guardRail(m,12.2f,17.6f,14.f,17.66f,base);guardRail(m,13.94f,17.6f,14.f,23.f,base);
 terminal(m,{10.4f,19.4f},"DEEP JUNCTION / SUBSTATION","MAIN LINE BLAST DOOR: LOCAL RELEASE.","E / OPEN THE BLAST DOOR TO POWER DISTRIBUTION.",0,stateId("pwr_blast_release"));
 for(float xx:{11.3f,12.3f,13.3f})wall(m,xx,17.7f,xx+.22f,18.f,base,base+.02f,12);
 // --- Services and light ---------------------------------------------------------------------
 for(float z:{-22.8f,-22.5f,-22.2f})servicePipe(m,{22.75f,0.f},{22.75f,22.8f},z,.045f,2);
 for(float y=2.f;y<22;y+=4.f)wall(m,22.62f,y,23.f,y+.07f,-23.f,-22.f,2);
 cableTray(m,{1.4f,0.f},{1.4f,9.f},base+3.4f);
 for(float y:{5.f,13.f,21.f}){wallLamp(m,22.98f,y,base+3.8f,kPi,1.1f,8.f);}
 for(float y:{5.f,20.f})wallLamp(m,1.02f,y,base+3.8f,0,.9f,7.f);
 for(Vec2 l:{Vec2{13.f,8.f},Vec2{5.f,16.f},Vec2{17.f,18.f}})m.lights.push_back(lamp(l,roof-.7f,1.3f,10.f,4000,.05f));
 m.lights.push_back(lamp({12.f,20.f},roof-.7f,1.f,6.f,5200,.1f));
 m.creatureSpawns={{CreatureKind::Huntsman,{16.5f,19.f},base}};
 freightFinish(m,31);
 return p;
}

}
std::shared_ptr<const AuthoredMapData> campaignMap(int level){
 static const auto maps=[](){std::array<std::shared_ptr<const Map>,CampaignChunkCount> result;
  result[0]=makeFoundry();result[1]=makePressureWorks();result[2]=makeTurbineGantry();result[3]=makeReactorComplex();result[4]=makeServiceGallery();result[5]=makeCoolantReturn();result[6]=makeCableVaults();result[7]=makePumpAnnex();result[8]=makeUtilityJunction();result[9]=makeWasteHandling();
  result[10]=makeFreightAccess();result[11]=makeLiftMachinery();result[12]=makeAuxiliaryLift();result[13]=makeReceivingOverlook();result[14]=makeReceivingLanes();result[15]=makeWarehouseIntake();result[16]=makeWarehouseAB();result[17]=makeWarehouseC();result[18]=makeWarehouseD();result[19]=makeWarehouseUpper();result[20]=makeManifestOffices();result[21]=makeManifestRouting();result[22]=makeTransferConveyors();result[23]=makeTransferSorting();result[24]=makeTransferRailYard();result[25]=makeEmptyPlatform();result[26]=makePlatformServiceEnd();result[27]=makeDepotTrackside();result[28]=makeDepotInspectionPits();result[29]=makeDepotGantries();result[30]=makeMaintenanceBore();result[31]=makeDeepJunction();
  result[ReactorCoreChunk]=makeReactorCore();result[CoolantLoopChunk]=makeCoolantLoop();
  // Chunks 34 on: the descent chapters, one file per district.
  buildCampaignChapters(result);
  // Area score: each part of the facility has its own loop; stitched maps
  // share one so it carries on across the seam.
  using M=MusicCue;
  // Chunks 34 on carry their cue in the chunk table (tools/campaign/plan.py).
  constexpr std::array<MusicCue,34> score{{
   M::Default,M::Default,M::Undercover,M::Subliminal,M::Undercover,M::Undercover,M::PitchBlack,M::PitchBlack,M::PitchBlack,M::PitchBlack,
   M::Freight,M::Freight,M::Freight,M::Freight,M::Freight,M::Freight,
   M::Heat,M::Heat,M::Heat,M::Heat,M::Darkness,M::Darkness,M::Darkness,M::Darkness,M::Darkness,
   M::LastChapter,M::LastChapter,M::LastChapter,M::LastChapter,M::LastChapter,M::DeathClose,M::DeathClose,
   M::Subliminal,M::Subliminal}};
  for(size_t i=0;i<score.size();++i){auto scored=std::make_shared<Map>(*result[i]);scored->definition.musicCue=score[i];result[i]=std::move(scored);}
  return result;}();
 return maps.at(size_t(level));
}
}
