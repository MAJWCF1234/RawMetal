#include "Game.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <queue>
#include <tuple>
#include <unordered_set>

namespace retro {
bool Game::testServiceMaps(){
 std::ofstream out("service-map-test.txt");
 auto check=[&](bool ok,const char* what){out<<what<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 constexpr int N=96;
 for(int level:{4,5}){
  Game game;game.loadLevel(level,false);game.updateStreaming(0);
  const auto& world=game.world();
  for(const auto& fixture:world.fixtures())if(fixture.solid){
   bool clear=true;
   for(int u=0;u<=16;++u)for(int v=0;v<=8;++v){
    float x=(u/16.f-.5f)*fixture.width,y=(v/8.f-.5f)*fixture.depth;
    float px=fixture.position.x+x*std::cos(fixture.yaw)+y*std::sin(fixture.yaw);
    float py=fixture.position.y-x*std::sin(fixture.yaw)+y*std::cos(fixture.yaw);
    if(world.tile(int(std::floor(px)),int(std::floor(py)))=='#')clear=false;
    for(const auto& s:world.structures())if(px>s.x1&&px<s.x2&&py>s.y1&&py<s.y2&&s.top>-9+fixture.base+.02f&&s.bottom<-9+fixture.base+fixture.height)clear=false;
   }
   if(!check(clear,"Entire equipment footprint clears walls and structural columns"))return false;
  }
  World doorsOpen=world;
  for(size_t d=0;d<world.doors().size();++d)doorsOpen.openDoor(int(d));
  doorsOpen.updateDoors(2);
  for(const auto& door:doorsOpen.doors()){
   float x=(door.left+door.right)*.5f;
   for(float y:{door.y-.45f,door.y,door.y+.45f})
    if(!check(doorsOpen.fits(x,y,-9,1.7f)&&!doorsOpen.doorBlocks(x,y,-9,1.7f),"Opened door has standing clearance on both sides"))return false;
  }
  if(level==4&&!check(world.floorHeight(12,12)==-9&&world.ceilingHeight(12,12)>-6.2f,"Dry service spine has headroom"))return false;
  // A quarter-metre walk grid uses the same full hull and step allowance as
  // movement. It catches sealed bays and one-way drops around water ramps.
  std::array<bool,N*N> reached{};std::queue<int> pending;
  auto spawn=world.definition().playerStart;
  int start=int(spawn.y*4)*N+int(spawn.x*4);reached[start]=true;pending.push(start);
  auto point=[](int index){return Vec2{(index%N+.5f)*.25f,(index/N+.5f)*.25f};};
  auto floorAt=[&](Vec2 p){return game.groundHeight(p,-8.f);};
  while(!pending.empty()){
   int from=pending.front();pending.pop();auto a=point(from);float az=floorAt(a);
   for(int offset:{-N,N,-1,1}){
    int to=from+offset;if(to<0||to>=N*N||(offset==-1&&from%N==0)||(offset==1&&from%N==N-1)||reached[to])continue;
    auto b=point(to);float bz=floorAt(b);
    if(std::fabs(bz-az)>.215f||!game.hullFits(b,bz,1.7f))continue;
    reached[to]=true;pending.push(to);
   }
  }
  for(Vec2 target:{Vec2{12,22},Vec2{3.5f,19.5f},Vec2{20.5f,19.5f}}){
   int at=int(target.y*4)*N+int(target.x*4);
   if(!reached[at])out<<"Level "<<level<<" unreached "<<target.x<<","<<target.y<<'\n';
   if(!check(reached[at],"Service route reaches exit and both maintenance bays"))return false;
  }
  if(level==4){
   for(Vec2 target:{Vec2{5.5f,9.5f},Vec2{19.5f,9},Vec2{5.5f,20},Vec2{19,20}})
    if(!check(reached[int(target.y*4)*N+int(target.x*4)],"Workshop, electrical room, stores and plant bay are accessible"))return false;
  }
  for(const auto& pipe:world.pipes()){
   bool clear=true;
   for(int i=0;i<=32;++i){auto p=pipe.start+(pipe.end-pipe.start)*(i/32.f);
    // Low runs are allowed inside equipment collision (pump suctions).
    bool inEquipment=false;for(const auto& s:world.structures())if((s.material==6||s.material==15)&&p.x>s.x1-.05f&&p.x<s.x2+.05f&&p.y>s.y1-.05f&&p.y<s.y2+.05f)inEquipment=true;
    for(const auto& f:world.fixtures())if(f.solid&&std::fabs(p.x-f.position.x)<f.width*.5f+.05f&&std::fabs(p.y-f.position.y)<f.depth*.5f+.05f)inEquipment=true;
    clear&=(inEquipment||pipe.z-pipe.radius>=-9+2.2f)&&pipe.z+pipe.radius<world.ceilingHeight(p.x,p.y);
   }
   if(!check(clear,"Authored headers fit below ceiling and above standing clearance"))return false;
  }
  if(level==5){
   if(!check(world.particleEmitters().empty(),"Settling channel does not emit fountain jets"))return false;
   if(!check(world.waterVolumes().size()==1,"One settling channel spans the hall"))return false;
   const auto& channel=world.waterVolumes().front();
   if(!check(channel.x1<=1.01f&&channel.x2>=22.99f,"The channel runs wall to wall: the way south is a wade"))return false;
   float x=(channel.x1+channel.x2)*.5f,y=(channel.y1+channel.y2)*.5f;
   if(!check(world.waterSurface(x,y)==channel.surface&&std::fabs(world.floorHeight(x,y)-channel.bed)<.001f,"Render, bed and buoyancy share the channel definition"))return false;
   if(!check(world.waterSurface(12,6)<-100&&world.floorHeight(12,6)==-9&&world.waterSurface(12,18)<-100,"Both landings stay dry"))return false;
   if(!check(world.doors().size()==2&&world.doors()[1].transfer&&world.doors()[0].swinging&&world.doorBlocks(19.7f,20.76f,-9,1.7f),"Rear store has a closed swinging door"))return false;
   World opened=world;opened.openDoor(0);opened.updateDoors(2);
   if(!check(!opened.doorBlocks(19.7f,20.76f,-9,1.7f)&&opened.fits(19.7f,21.3f,-9,1.7f),"Swinging leaf clears a walkable store entrance"))return false;
  }
 }
 {World channelGeometry(5);const auto& channel=channelGeometry.waterVolumes().front();
  for(float direction:{1.f,-1.f}){
   Game wade;wade.loadLevel(5,false);wade.m_enemies.clear();
   wade.m_player.pos={6.f,direction>0?7.4f:15.6f};wade.m_player.z=-9;wade.m_player.grounded=true;wade.m_player.angle=direction*kPi*.5f;
   InputState forward{};forward.forward=true;float lowest=0;float end=direction>0?channel.y2+.6f:channel.y1-.6f;
   for(int frame=0;frame<900&&direction*(wade.player().pos.y-end)<0;++frame){wade.update(forward,1.f/120);lowest=std::min(lowest,wade.player().z);}
   if(!check(lowest<channel.surface&&direction*(wade.player().pos.y-end)>=0&&std::fabs(wade.player().z+9)<.08f,"The channel can be waded and climbed out of in both directions"))return false;
  }
 }
 Game debris;debris.loadLevel(5,false);debris.m_enemies.clear();debris.m_clutter.clear();
 Clutter bottle;bottle.kind=2;bottle.pos={9,11.5f};bottle.z=-9.10f;debris.m_clutter.push_back(bottle);
 Clutter metal;metal.kind=3;metal.pos={9.5f,9.f};metal.z=-9.10f;debris.m_clutter.push_back(metal);
 for(int i=0;i<360;++i)debris.updateClutter({},1.f/120);
 float bed=debris.world().floorHeight(9,11.5f);
 if(!check(debris.m_clutter[0].z>bed+.1f&&!debris.m_clutter[0].sleeping,"Buoyant debris stays at the water surface"))return false;
 auto& heavy=debris.m_clutter[1];auto extent=heavy.extent();float contact=-100;
 for(float dx:{-extent[0],0.f,extent[0]})for(float dy:{-extent[1],0.f,extent[1]})
  contact=std::max(contact,debris.world().floorHeight(heavy.pos.x+dx,heavy.pos.y+dy));
 if(!check(std::fabs(heavy.z-contact)<.035f,"Heavy debris settles against the sloped basin floor"))return false;
 Game restored;
 if(!check(restored.decodeSave(debris.encodeSave())&&restored.level()==5&&restored.world().waterVolumes().size()==1&&restored.clutter().size()==2,
           "Saving and restoring retains channel geometry and debris state"))return false;
 return true;
}
bool Game::testCampaignExtension(){
 std::ofstream out("campaign-extension-test.txt");
 auto check=[&](bool ok,const char* name){out<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 bool result=true;
 for(int level=6;level<10;++level){
  Game game;game.loadLevel(level,false);game.updateStreaming(0);
  out<<"CHUNK "<<level<<'\n';
  if(level>=7)for(const auto& enemy:game.m_enemies)
   result&=check(game.hullFits(enemy.pos,enemy.bodyBottom(),enemy.bodyTop()-enemy.bodyBottom()),"Creature spawns clear stairs, machinery and workstation collision");
  game.m_enemies.clear();
  // Test geometry with access gates open; the control and locked-door sequence
  // is exercised separately below using a fresh game with closed gates.
  for(int i=0;i<int(game.world().doors().size());++i)game.m_world.setDoor(i,1.f,true);
  result&=check(game.hullFits(game.player().pos,game.player().z,1.7f),"Spawn has standing clearance");
  // Search all reachable elevations. Each edge uses the game's standing hull
  // and 21.5 cm step allowance, including stair-to-deck joins.
  struct Node{int x,y;float z;};constexpr int N=96;
  auto key=[](Node n){return (uint64_t(int(std::round((n.z+32)*100)))<<16)|uint64_t(n.y*96+n.x);};
  auto point=[](Node n){return Vec2{(n.x+.5f)*.25f,(n.y+.5f)*.25f};};
  Node start{int(game.player().pos.x*4),int(game.player().pos.y*4),game.player().z};
  std::queue<Node> pending;std::unordered_set<uint64_t> seen;std::vector<Node> reachable;
  pending.push(start);seen.insert(key(start));
  while(!pending.empty()){
   auto a=pending.front();pending.pop();reachable.push_back(a);
   for(Vec2 direction:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){
    Node b{a.x+int(direction.x),a.y+int(direction.y),a.z};if(b.x<0||b.x>=N||b.y<0||b.y>=N)continue;
    auto pos=point(b);b.z=game.groundHeight(pos,a.z+.215f);
    if(std::fabs(b.z-a.z)>.215f||!game.hullFits(pos,b.z,1.7f)||!seen.insert(key(b)).second)continue;
    pending.push(b);
   }
  }
  struct Target{Vec2 p;float z;};std::vector<Target> targets;
  if(level==6)targets={{{19.5f,14},-9},{{4.5f,6.5f},-9},{{4,20},-9},{{21.5f,22.5f},-9}};
  if(level==7)targets={{{12.6f,12.4f},-12},{{3,10},-9},{{16.f,20.5f},-4},{{20.5f,22.5f},-4}};
  if(level==8)targets={{{12,17.5f},-9},{{20,3},-9},{{3,20},-9},{{21.5f,22.5f},-9},{{8,2},-4}};
  if(level==9)targets={{{15,10.5f},-12},{{21.5f,22.f},-12},{{18.5f,5.5f},-9},{{11.5f,18},-12}};
  for(auto target:targets){bool found=false;for(auto n:reachable)if(length(point(n)-target.p)<.45f&&std::fabs(n.z-target.z)<.03f){found=true;break;}
   out<<"target "<<target.p.x<<','<<target.p.y<<','<<target.z<<' ';result&=check(found,"Authored destination reachable without jumping");
  }
  for(const auto& pickup:game.pickups()){
   bool accessible=false;float z=game.pickupHeight(pickup);
   for(auto n:reachable)if(length(point(n)-pickup.pos)<.4f&&std::fabs(n.z-z)<.03f){accessible=true;break;}
   result&=check(accessible,"Supply can be reached on the same floor where it is rendered and collected");
  }
  // Authored equipment offsets must keep meshes and collision above the base floor.
  bool equipmentAboveFloor=true;
  for(const auto& f:game.world().fixtures())equipmentAboveFloor&=f.base>=0;
  for(const auto& p:game.world().props())equipmentAboveFloor&=p.base>=0;
  result&=check(equipmentAboveFloor,"Service equipment is not buried below its owning floor");
   bool servicesClear=true;
   for(const auto& pipe:game.world().pipes()){
    const bool vertical=pipe.endZ> -999.f;
    float high=vertical?std::max(pipe.z,pipe.endZ):pipe.z;
    float low=vertical?std::min(pipe.z,pipe.endZ):pipe.z;
    if(!(high+pipe.radius<game.world().ceilingHeight(pipe.start.x,pipe.start.y))){servicesClear=false;out<<"pipe at roof "<<pipe.start.x<<','<<pipe.start.y<<" top "<<high+pipe.radius<<'\n';}
    float length=retro::length(pipe.end-pipe.start);int samples=std::max(1,int(std::ceil(length/.25f)));
    for(int i=0;i<=samples;++i){auto p=pipe.start+(pipe.end-pipe.start)*(float(i)/samples);
     for(const auto& layer:game.world().layers())if(layer.thickness>0){int x=int(std::floor(p.x)),y=int(std::floor(p.y));
      if(x<0||y<0||y>=int(layer.rows.size())||x>=int(layer.rows[size_t(y)].size())||layer.rows[size_t(y)][size_t(x)]!='=')continue;
      const float slabTop=layer.elevation,slabBottom=layer.elevation-layer.thickness;
      bool hit=vertical?(high+pipe.radius>slabBottom&&low-pipe.radius<slabTop):(pipe.z-pipe.radius<slabTop+1.7f&&pipe.z+pipe.radius>slabBottom);
      if(hit){servicesClear=false;out<<"pipe through deck "<<pipe.start.x<<','<<pipe.start.y<<" z "<<pipe.z<<".."<<pipe.endZ<<'\n';}
     }
    }
   }
  result&=check(servicesClear,"Overhead pipe runs clear every playable deck and roof");
  if(level>=7){
   bool lampsFit=true;for(const auto& light:game.world().lights())
    lampsFit&=light.z+.13f<=game.world().clearanceAbove(light.position.x,light.position.y,light.z)+.005f;
   result&=check(lampsFit,"Light housings fit below their actual mounting slab or roof");
  }
  // Every control/help station must be approachable with a standing player.
  for(size_t terminalIndex=0;terminalIndex<game.world().terminals().size();++terminalIndex){const auto& terminal=game.world().terminals()[terminalIndex];
   float z=game.world().floorHeight(terminal.position.x,terminal.position.y)+terminal.z;
   bool accessible=false;
   for(auto n:reachable){auto p=point(n),delta=terminal.position-p;if(length(delta)>=1.8f||std::fabs(n.z-z)>=.03f)continue;
    game.m_player.pos=p;game.m_player.z=n.z;game.m_player.angle=std::atan2(delta.y,delta.x);
    if(game.nearbyTerminal()==int(terminalIndex)){accessible=true;break;}
   }
  if(level==8)out<<"junction terminal "<<terminal.position.x<<','<<terminal.position.y<<" z "<<terminal.z<<" interaction "<<(accessible?"PASS":"FAIL")<<'\n';
  result&=check(accessible,"Service terminal has a reachable standing interaction position");
  }
  // Regression checks for the cleanup pass: imported furniture must face the
  // aisle, upper decks use authored structure instead of procedural post spam,
  // and the suspended freight branch ends in a real vestibule.
  if(level==6){
   auto routeReaches=[&](Vec2 p,float z){for(const auto& n:reachable)if(length(point(n)-p)<.3f&&std::fabs(n.z-z)<.05f)return true;return false;};
   const auto& trench=game.world().waterVolumes().front();const auto& arc=game.world().hazards().front();
   bool recessed=trench.surface<-9.1f&&trench.surface-trench.bed>.2f;
   bool arcOverWater=arc.x1<=trench.x1+.01f&&arc.x2>=trench.x2-.01f&&arc.bottom<=trench.bed+.01f;
   result&=check(recessed&&arcOverWater,"The centre tunnel is a recessed, flooded, live trench");
   result&=check(routeReaches({4.f,20.f},-9)&&routeReaches({19.5f,14.f},-9),"West tunnel and east feeder room are reachable on dry ground");
   bool tripInFeederRoom=false;for(const auto& t:game.world().terminals())tripInFeederRoom|=t.activateState==stateId("vault_disconnect")&&t.position.x>16.5f&&t.position.y>12.f;
   result&=check(tripInFeederRoom,"The feeder trip stands in the east feeder room");
   int feederAssemblies=0;for(const auto& f:game.world().fixtures())if(f.model==12&&f.solid)++feederAssemblies;
   result&=check(feederAssemblies>=2,"The feeder room holds grounded feeder machinery as cover");
  }
  if(level==7){
   int pumps=0;for(const auto& st:game.world().structures())if(st.material==6&&std::fabs(st.bottom+12.f)<.01f&&st.top>-7.f&&st.x2-st.x1<1.6f)++pumps;
   result&=check(pumps==4,"Four vertical duty pumps stand in the pit, motors above the main floor");
   bool galleryLit=false;for(const auto& light:game.world().lights())galleryLit|=std::fabs(light.z+4.4f)<.01f;
   result&=check(galleryLit,"Lamps hang under the observation gallery");
   Game annex;annex.loadLevel(7,false);annex.m_enemies.clear();
   result&=check(annex.doorLocked(annex.world().doors().back()),"Pump Annex transfer stays locked until duty pumps restart");
   annex.m_player.pos={16.f,20.5f};annex.m_player.z=-4.f;annex.m_player.grounded=true;annex.update({},1.f/60.f);
   result&=check(annex.state("annex_running")==0&&annex.doorLocked(annex.world().doors().back()),"Walking onto the observation gallery cannot skip the pump restart");
   annex.m_player.pos={12.6f,12.4f};annex.m_player.z=-12.f;annex.m_player.angle=kPi*.5f;
   InputState startPump{};startPump.use=true;annex.updateInteraction(startPump,.01f);
   result&=check(annex.state("annex_running")==1&&!annex.doorLocked(annex.world().doors().back()),"The pit-floor start releases the Annex transfer through player interaction");
  }
  if(level==8){
   result&=check(!game.world().fits(17.1f,3.f,-9,1.7f)&&game.world().fits(20.f,3.f,-9,1.7f),"Freight branch has enclosing walls and a usable interior");
   bool lightsMounted=true;
   for(const auto& light:game.world().lights())if(light.mount==WorldLightMount::Ceiling)lightsMounted&=std::fabs(light.z+.15f-game.world().clearanceAbove(light.position.x,light.position.y,light.z-1.f))<.02f;
   result&=check(lightsMounted,"Utility Junction lamps hang under a deck, office roof or the hall roof");
   int columns=0;for(const auto& st:game.world().structures())if(st.material==12&&std::fabs(st.bottom+9.f)<.01f&&std::fabs(st.top+4.25f)<.01f)++columns;
   result&=check(columns>=4,"The arrival bridge stands on columns down to the concourse");
   int mains=0;for(const auto& pipe:game.world().pipes())if(pipe.radius>.19f&&pipe.radius<.21f&&std::fabs(pipe.start.x-22.95f)<.01f)++mains;
   result&=check(mains==3,"Three trunk mains enter the hall from the east wall");
   bool freightControl=false,permitControl=false;
   for(const auto& terminal:game.world().terminals()){
    freightControl|=terminal.activateState==stateId("freight_incident_clearance");
    permitControl|=terminal.activateState==stateId("primary_utilities_permit");
   }
   result&=check(freightControl&&permitControl,"Utility Junction has a local operator for each locked service branch");
   for(auto [name,flag,doorIndex,position]:std::array<std::tuple<const char*,StateId,int,Vec2>,2>{{
       {"freight override",stateId("freight_incident_clearance"),1,{16.2f,7.5f}},
       {"utilities permit",stateId("primary_utilities_permit"),2,{6.6f,16.4f}}}}){
    Game branch;branch.loadLevel(8,false);branch.updateStreaming(0);branch.m_enemies.clear();
    auto terminal=std::find_if(branch.world().terminals().begin(),branch.world().terminals().end(),[&](const auto& t){return t.activateState==flag;});
    bool station=terminal!=branch.world().terminals().end()&&std::fabs(terminal->position.x-position.x)<.01f&&std::fabs(terminal->position.y-position.y)<.01f;
    for(const auto& fixture:branch.world().fixtures())if(fixture.model==11&&length(fixture.position-position)<.05f)station=true;
    branch.m_player.pos=position+Vec2{-.8f,0};branch.m_player.z=branch.world().floorHeight(position.x,position.y)+(terminal!=branch.world().terminals().end()?terminal->z:0.f);branch.m_player.angle=0;branch.m_player.grounded=true;
    InputState use{};use.use=true;branch.updateInteraction(use,.01f);
    result&=check(station&&branch.state(flag)==1&&!branch.doorLocked(branch.world().doors()[size_t(doorIndex)]),name);
   }
  }
  if(level==9){
   result&=check(game.world().clutterSpawns().size()==19,"Waste Handling keeps its scrap population");
   // The bunker wall divides the floor: the press bay is the only way south.
   bool bunker=!game.world().fits(6.f,14.15f,-12,1.7f)&&!game.world().fits(20.f,14.15f,-12,1.7f)&&game.world().fits(15.f,14.15f,-12,1.7f);
   result&=check(bunker,"The bunker wall leaves the press bay as the way south");
   bool pulpit=false;for(const auto& t:game.world().terminals())pulpit|=t.activateState==stateId("compactor_isolated")&&t.z>2.9f;
   result&=check(pulpit,"The press isolator is in the pulpit on the sorting gallery");
  }
  // Emit reached positions for diagnosing a failed stair or rail join.
  std::ofstream positions("campaign-reach-"+std::to_string(level)+".txt");for(auto n:reachable)positions<<point(n).x<<' '<<point(n).y<<' '<<n.z<<'\n';
 }
 for(int level=5;level<9;++level){
  Game seam;seam.loadLevel(level,false);seam.updateStreaming(0);const auto d=seam.world().doors().back();
  result&=check(d.transfer,"Last door transfers to the next chapter");
  if(level==7){
   result&=check(d.swinging&&std::fabs((d.right-d.left)-1.4f)<.01f,
                 "Pump Annex to Utility Junction uses a normal framed personnel door");
   World framed=seam.world();framed.openDoor(int(framed.doors().size())-1);framed.updateDoors(2);
   result&=check(framed.fits(20.5f,23.45f,-4,1.7f)&&!framed.fits(19.35f,23.45f,-4,1.7f)&&!framed.fits(21.65f,23.45f,-4,1.7f),
                 "Annex transfer vestibule leaves only the normal doorway open");
  }
  seam.useDoor(int(seam.world().doors().size())-1);seam.m_world.updateDoors(2);seam.updateStreaming(0);
  float x=(d.left+d.right)*.5f,z=seam.world().floorHeight(x,d.y)+d.z;
  seam.m_player.pos={x,23.6f};seam.m_player.z=z;seam.m_player.grounded=true;seam.m_player.angle=kPi*.5f;seam.m_enemies.clear();
  InputState walk{};walk.forward=true;
  for(int i=0;i<70;++i)seam.update(walk,1.f/120);
  result&=check(seam.level()==level+1&&std::fabs(seam.player().z-z)<.03f,"Walk through opened transfer at matching elevation");
  seam.m_velocity={};seam.m_player.angle=-kPi*.5f;
  for(int i=0;i<90;++i)seam.update(walk,1.f/120);
  result&=check(seam.level()==level,"Return through the same seamless doorway");
 }
 Game cable;cable.loadLevel(6,false);auto h=cable.world().hazards().front();
 for(int level:{6,8,9}){
  Game control;control.loadLevel(level,false);auto terminal=control.world().terminals().front();
  control.m_player.pos=terminal.position+Vec2{0,-.8f};control.m_player.z=control.world().floorHeight(terminal.position.x,terminal.position.y)+terminal.z;control.m_player.angle=kPi*.5f;
  InputState use{};use.use=true;control.updateInteraction(use,.01f);
  result&=check(control.state(terminal.activateState)==1,"Player E interaction operates the authored local control");
 }
 cable.m_elapsed=.5f;bool on=cable.hazardActive(h);cable.m_elapsed=2;bool off=!cable.hazardActive(h);cable.setState(stateId("vault_disconnect"),1);cable.m_elapsed=.5f;
 result&=check(on&&off&&!cable.hazardActive(h),"Electrical arc pulses and local isolation disables it");
 Game waste;waste.loadLevel(9,false);auto press=waste.world().compactors().front();
 waste.m_enemies.clear();waste.spawnCreature({CreatureKind::Huntsman,{15,10},-12});waste.m_clutter.clear();Clutter junk;junk.pos={15,10};junk.z=-12;junk.kind=3;waste.m_clutter.push_back(junk);
 waste.m_elapsed=press.period*.55f;waste.updateHazards(.5f);
 result&=check(!waste.m_enemies[0].alive&&waste.m_clutter.empty(),"Closed press crushes creatures and loose scrap");
 waste.setState(press.stopState,1);
 waste.m_player.pos={15,10};waste.m_player.z=waste.groundHeight({15,10},press.bed+.1f);waste.m_player.health=100;waste.updateHazards(.25f);
 result&=check(waste.compactorHeight(press)==press.raised&&press.raised-waste.m_player.z>=waste.m_player.hullHeight()+.15f&&waste.m_player.health==100&&waste.hullFits({15,10},waste.m_player.z,waste.m_player.hullHeight()),
                "Isolated press has real standing clearance and does not injure the operator");
 Game restored;result&=check(restored.decodeSave(waste.encodeSave())&&restored.level()==9&&restored.state(press.stopState)==1,"New chapters and machinery state survive save/load");
 for(int level:{7,8,9}){
  Game supplies;supplies.loadLevel(level,false);supplies.m_enemies.clear();
  for(size_t i=0;i<supplies.m_pickups.size();++i){auto& pickup=supplies.m_pickups[i];float z=supplies.pickupHeight(pickup),bottom=supplies.world().floorHeight(pickup.pos.x,pickup.pos.y);
   if(std::fabs(z-bottom)<.5f)continue;
   supplies.m_player.pos=pickup.pos;supplies.m_player.z=bottom;supplies.updatePickups();bool wrongFloorProtected=pickup.active;
   supplies.m_player.z=z;supplies.updatePickups();
   Game loaded;bool saved=loaded.decodeSave(supplies.encodeSave());
   result&=check(wrongFloorProtected&&!pickup.active&&saved&&!loaded.m_pickups[i].active&&std::fabs(loaded.pickupHeight(loaded.m_pickups[i])-z)<.01f,
                 "Elevated supply rejects collection from below and preserves its elevation and collected state after load");
  }
 }
 return result;
}

}
