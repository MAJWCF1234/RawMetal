#include "World.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace retro {
namespace {
// Authored maps: each array is one named layer, never a second world implementation.
constexpr MapRows FoundryGround = {
    "########################",
    "#...........#..........#",
    "#..CC...B...#...GG.....#",
    "#..CC.......#..........#",
    "#......................#",
    "#.................BB...#",
    "#......CC...#...###....#",
    "#...........#..........#",
    "###...############...###",
    "#.........#............#",
    "#..GG.....#..TT....GG..#",
    "#............TT........#",
    "#...#........TT........#",
    "#...#..................#",
    "#...#............BB....#",
    "#.........#............#",
    "##########....#####...##",
    "#..BB..........#.......#",
    "#......#..CC...#...#...#",
    "#......................#",
    "#..###.........#.......#",
    "#......G.......#####...#",
    "#..............#####.X.#",
    "####################...#"
};
constexpr MapRows PressureWorksGround = {
    "##...###################",
    "#.......#..............#",
    "#.......#..............#",
    "#...................CC.#",
    "#......................#",
    "#..CC...#..............#",
    "#......................#",
    "###...############...###",
    "#......................#",
    "#.....B...#..#.#.......#",
    "#..............#.......#",
    "#..............#.....B.#",
    "#......................#",
    "#.CC...................#",
    "#..............#.......#",
    "#.........#..#.#.......#",
    "#......................#",
    "#####...##########...###",
    "#..........#...........#",
    "#......................#",
    "#........B.............#",
    "#..........#....####...#",
    "#..........#....####.X.#",
    "####################...#"
};

// Blank two-chunk megamap experiment. The shared 22-metre interior edge has
// no wall, while the side perimeter walls continue through the seam.
constexpr MapRows CoolantReturnGround = {
    "#####...################",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#"
};
constexpr MapRows CableVaultsGround = {
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "##################...###"
};

// The gantry route includes orthogonal joins and a stair landing; the final exit is sealed.
constexpr MapRows TurbineGantryGround = {
    "##...###################",
    "#....#.................#",
    "#....#....GGGG.........#",
    "#.........GGGG.........#",
    "#......................#",
    "#..CCC.............BB..#",
    "#..CCC....######...BB..#",
    "#.........#....#.......#",
    "######....#....#...#####",
    "#.........#....#.......#",
    "#....GG...#....#..GG...#",
    "#....GG...#....#..GG...#",
    "#.........#....#.......#",
    "#.........#....#.......#",
    "#....######....#####...#",
    "#......................#",
    "#..BB..............CC..#",
    "#..B.....########..CC..#",
    "#.B......#......#......#",
    "#........#......#......#",
    "#...SS...#......#......#",
    "#...SS..........####...#",
    "#...............####.X.#",
    "####################...#"
};
constexpr MapRows TurbineGantryUpperCatwalk = {
    "________________________",
    "________________________",
    "______==========________",
    "______=________=________",
    "______=________=________",
    "______=___====_=________",
    "______=___=__===________",
    "______=___=_____________",
    "______=___=_____________",
    "___====___=______====___",
    "___=______=______=______",
    "___=______========______",
    "___=____________________",
    "___=____________________",
    "___======_______________",
    "________=_______________",
    "________=_______________",
    "________======__________",
    "_____________=__________",
    "_____________=__________",
    "____SS========__________",
    "____SS=_________________",
    "____===_________________",
    "________________________"
};

constexpr MapLayer FoundryGroundLayer{"Foundry / ground",0,0,FoundryGround};
// The lift room travels through one continuous shaft. The reactor has two
// playable floors; upper floors are separate decks over the lower map.
constexpr MapRows LiftShaftGround = {
    "##...###################",
    "#......................#",
    "#......................#",
    "#.....CC...............#",
    "#......................#",
    "#..BB..................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#....GG................#",
    "#......................#",
    "#..............GG......#",
    "#......................#",
    "#....................X.#",
    "####################...#",
};
constexpr MapRows emptyDeck(){
 MapRows rows{};for(auto& row:rows)row="________________________";return rows;
}
constexpr MapRows reactorDeck(){
 return {
"________________________",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_========______========_",
"_========______========_",
"_========______====__==_",
"_========______====__==_",
"_========______====__==_",
"_========______====__==_",
"_==================__==_",
"_=========_____====__==_",
"_=========_____====__==_",
"_=========_____========_",
"_=========_____========_",
"_=========_____========_",
"_=========_____========_",
"_=========_____========_",
"________________________",
 };
}
constexpr MapRows shaftDeck(){
 return {
"________________________",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_========______========_",
"_========______========_",
"_========______========_",
"_========______========_",
"_========______========_",
"_========______========_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"_======================_",
"________________________",
 };
}
constexpr MapRows collarDeck(){
 auto rows=shaftDeck();
 rows[0]="__===___________________";
 rows[8]="_==========SS==========_";
 rows[9]="_========__SS__========_";
 return rows;
}
// Passing storeys are shallow scenery rings, not full exploration maps.
constexpr MapRows shaftScenery(){
 MapRows rows{};for(auto&row:rows)row="________________________";
 rows[8]=rows[15]="________========________";
 for(int y=9;y<15;++y)rows[y]="________=______=________";
 return rows;
}
constexpr std::array ReactorStairs{Staircase{19,12,21,18,-9,-6,15,true,true}};
constexpr MapLayer PressureWorksGroundLayer{"Pressure Works / ground",0,0,PressureWorksGround};
constexpr MapLayer TurbineGantryGroundLayer{"Turbine Gantry / ground",0,0,TurbineGantryGround};
constexpr MapLayer TurbineGantryUpperLayer{"Turbine Gantry / upper catwalk",3,.3f,TurbineGantryUpperCatwalk};
constexpr std::array GantryStairs{Staircase{4,18,6,22,0,3,15,true,true}};
// Six connected outdoor regions. Every north/south edge has the same broad
// breach, so streaming joins terrain instead of reading as repeated rooms.
constexpr MapRows ashfallRegion(int region){
 MapRows rows={
  "##########....##########","#......................#","#......................#","#....####..............#",
  "#....#..#......####....#","#....#..#......#..#....#","#....####......####....#","#......................#",
  "#..........####........#","#..........#..#........#","#....####..#..#..####..#","#....#..#..####..#..#..#",
  "#....####........####..#","#......................#","#..####....####........#","#..#..#....#..#........#",
  "#..####....####....##..#","#......................#","#....####..............#","#....#..#....####......#",
  "#....####....#..#......#","#............####......#","#......................#","##########....##########"};
 if(region==1){rows[3]="####....####....####....";rows[8]="#.......######.........#";rows[14]="#....####....####....###";rows[19]="#....#....####....#....#";}
 if(region==2){rows[4]="#..####......####......#";rows[6]="#..#..#......#..#......#";rows[10]="#......####......####..#";rows[16]="#....####....####......#";}
 if(region==3){rows[2]="#.....####.............#";rows[7]="#..####......####......#";rows[12]="#..#..#......#..#......#";rows[18]="#......####......####..#";}
 if(region==4){rows[5]="#........####..........#";rows[9]="#....####....####......#";rows[15]="#....#..#....#..#......#";rows[20]="#....####....####......#";}
 if(region==5){rows[3]="#..####....####........#";rows[11]="#..............####....#";rows[17]="#....####....####......#";rows[21]="#.......######.........#";}
 return rows;
}
static_assert([]{for(int i=0;i<6;++i)for(auto row:ashfallRegion(i))if(row.size()!=24)return false;return true;}(),"Ashfall rows must be exactly 24 cells");
// Purchased pack fixtures: shelf=7, switch cabinet=8. Wall-mounted cabinets
// meet the wall at their backs; shelves have solid footprints on level floors.
const std::array<std::vector<Fixture>,3> MapFixtures{{
 {{7,{1.27f,2.7f},0,2,.5f,1.8f,kPi*.5f,true},{7,{22.7f,18.8f},0,1.6f,.5f,1.8f,kPi*.5f,true},
  {8,{6.5f,1.10f},.75f,.7f,.20f,1.f,0},{8,{14.5f,9.10f},.7f,.7f,.20f,1.f,0}},
 {{7,{1.27f,3.f},0,2,.5f,1.8f,kPi*.5f,true},{7,{13.f,16.72f},0,1.6f,.5f,1.8f,0,true},
  {8,{10.5f,1.10f},.8f,.7f,.20f,1.f,0},{8,{1.10f,10.5f},.75f,.7f,.20f,1.f,kPi*.5f}},
 {{7,{1.27f,10.5f},0,1.8f,.5f,1.8f,kPi*.5f,true},{7,{22.7f,4.5f},0,1.8f,.5f,1.8f,kPi*.5f,true},
  {8,{7.5f,1.10f},.7f,.7f,.20f,1.f,0},{8,{1.10f,19.5f},.8f,.7f,.20f,1.f,kPi*.5f}}
}};
bool insideFixture(const Fixture&fixture,float x,float y,float margin=0){
 float dx=x-fixture.position.x,dy=y-fixture.position.y,c=std::cos(fixture.yaw),s=std::sin(fixture.yaw);
 return std::fabs(dx*c-dy*s)<fixture.width*.5f+margin&&std::fabs(dx*s+dy*c)<fixture.depth*.5f+margin;
}
constexpr float ShelfTiers[]={.17f,.54f,.92f};
}

void World::buildPopulation(){
 using C=CreatureKind;using P=PickupKind;
 if(!campaign()){
  m_creatureSpawns={{C::Huntsman,{20.5f,7.5f}},{C::Wasp,{3.5f,13.5f}},{C::Brute,{20.5f,17.5f}}};
  m_pickupSpawns={{{3.5f,7.5f},P::Ammo},{{20.5f,17.5f},P::Health}};
  for(int i=0;i<6;++i)m_clutterSpawns.push_back({i,{2.5f+i*.45f,7.5f},-999,i*.7f});
  return;
 }
 // Population belongs to the map, just like its layers and fixtures.
 switch(m_level){
 case 0:
  m_creatureSpawns={{C::Huntsman,{9.5f,4.9f}},{C::Wasp,{15.5f,2.5f}},{C::Brute,{19.5f,7.5f}},{C::Huntsman,{8.5f,11.5f}},
                   {C::Wasp,{18.5f,12.5f}},{C::Brute,{5.5f,15.5f}},{C::Huntsman,{12.5f,18.5f}},{C::Wasp,{19.5f,20.5f}}};
  m_pickupSpawns={{{4.5f,7.5f},P::Ammo},{{13.5f,5.5f},P::Health},{{22.5f,10.5f},P::Ammo},{{7.5f,20.5f},P::Health}};
  break;
 case 1:
  m_creatureSpawns={{C::Huntsman,{17.5f,4.5f}},{C::Wasp,{6.5f,6.2f}},{C::Brute,{4.5f,14.5f}},{C::Huntsman,{9.5f,12.5f}},
                   {C::Wasp,{19.5f,12.5f}},{C::Brute,{21.5f,15.5f}},{C::Huntsman,{7.5f,19.5f}},{C::Wasp,{15.5f,20.5f}},{C::Brute,{21.5f,19.5f}}};
  m_pickupSpawns={{{4.5f,4.5f},P::Ammo},{{16.5f,3.5f},P::Health},{{2.5f,15.5f},P::Ammo},{{21.5f,12.5f},P::Ammo},{{7.5f,20.5f},P::Health},{{17.5f,19.5f},P::Ammo}};
  break;
 case 2:
  m_creatureSpawns={{C::Huntsman,{7.5f,4.5f}},{C::Wasp,{16.5f,4.5f}},{C::Brute,{7.5f,12.5f}},
                   {C::Huntsman,{3.5f,15.5f}},{C::Wasp,{19.5f,15.5f}},{C::Brute,{21.5f,20.5f}}};
  m_pickupSpawns={{{3.5f,3.5f},P::Ammo},{{7.5f,15.5f},P::Health},{{7.5f,19.5f},P::Ammo},{{21.5f,18.5f},P::Ammo}};
  break;
 case 3:
  m_creatureSpawns={{C::Huntsman,{5.5f,13.5f}},{C::Wasp,{17.5f,19.5f}},{C::Warden,{21.5f,18.5f}}};
  m_pickupSpawns={{{12.5f,15.5f},P::Ammo},{{15.5f,17.5f},P::Health},{{21.5f,19.5f},P::Ammo}};
  break;
 default:return;
 }
 const Vec2 positions[3][6]={{{2.8f,5.9f},{3.1f,6.1f},{11.3f,4.3f},{11.7f,4.6f},{18.1f,19.9f},{18.8f,20.2f}},{{6.8f,3.2f},{7.1f,3.4f},{9.2f,12.2f},{9.5f,12.6f},{18.7f,19.8f},{19.1f,20.f}},{{7.3f,4.6f},{7.7f,4.8f},{7.2f,18.7f},{7.6f,19.f},{18.6f,9.7f},{19.5f,9.5f}}};
 for(int i=0;i<6;++i)m_clutterSpawns.push_back({i,m_level==3?Vec2{3.5f+i*.45f,19.5f}:positions[m_level][i],m_level==2&&i>=4?3.f:-999.f,i*.7f});
}

World::World(int level,WorldId id):m_worldId(id) {
 m_level=std::clamp(level,0,5);
 buildPopulation();
 if(horrorMode()){
  const char* regionNames[]={"Ashfall / perimeter ruins","Ashfall / collapsed highway","Ashfall / rusted yard","Ashfall / sunken district","Ashfall / radio spire","Ashfall / evacuation gate"};
  auto rows=ashfallRegion(m_level);
  if(m_level==0)rows[0]="########################";
  if(m_level==5)rows[23]="########################";
  m_layers={{regionNames[m_level],0,0,rows}}; m_openNorthBoundary=m_level>0; m_openSouthBoundary=m_level<5;
  m_internalWallHeight=2.8f;
  float shift=float(m_level%3)*2.f;
  // Keep the deployment pad completely clear: the player enters at 3.5, 4.5.
  m_props={{0,{14.5f+shift*.25f,4.5f},1.4f,2.2f,0,{1.1f,.66f},0},{1,{17.5f-shift,13.5f},1.2f,2.f,.4f,{1.f,.5f},0},{2,{9.5f,19.5f-shift},.35f,3.5f,.2f,{1.7f,.18f},0}};
  m_fixtures={{7,{2.3f,8.5f},0,2,.5f,1.8f,0,true},{8,{21.8f,9.5f},.8f,.7f,.2f,1.f,3.14f,true}};
  // Outdoor ambient light needs no unsupported indoor ceiling fixtures.
  m_terminals={{{10,3},"ASHFALL FIELD RELAY","OUTER PERIMETER COMPROMISED.","FOLLOW THE SOUTHERN BREACH.",0,false}};
  buildLayers({});return;
 }
 if(m_level>=4){
  if(m_level==5)m_particleEmitters.push_back({{20.35f,18.2f},-7.8f,{-.28f,0}});
  m_layers={{m_level==4?"Reactor Service Gallery":"Coolant Return",-9,0,m_level==4?CoolantReturnGround:CableVaultsGround}};
  m_openNorthBoundary=m_level==5;
  m_openSouthBoundary=m_level==4;
  if(m_level==4)m_doors={{5,8,.5f,0,false,false,true}};
  // Concrete bay walls, real overhead transfer beams and pipe supports frame
  // the service route. All structural solids share collision and rendering.
  const float roof=m_level==4?2.9f:3.4f;
  auto wall=[&](float x1,float y1,float x2,float y2){m_structures.push_back({x1,y1,x2,y2,0,roof,false,3});};
  for(float y:{3.8f,12.3f,20.2f}){
   m_structures.push_back({1.2f,y,22.8f,y+.22f,roof-.54f,roof-.32f,false,2});
   for(float x:{1.2f,22.4f})m_structures.push_back({x,y-.15f,x+.38f,y+.37f,0,roof-.32f,false,3});
  }
  if(m_level==4){
   for(float x:{6.8f,16.8f}){wall(x,1,x+.25f,8.5f);wall(x,11.5f,x+.25f,17.5f);wall(x,20.5f,x+.25f,24);}
   // Bolted cable trays follow the actual bay dividers rather than floating
   // in space. Shallow steel service crossings break up the long floor run.
   for(float y:{2.f,12.f,21.f}){
    float end=y==2.f?8.f:y==12.f?17.f:23.f;
    m_structures.push_back({7.04f,y,7.14f,end,1.92f,2.07f,false,2});
    m_structures.push_back({16.70f,y,16.82f,end,1.92f,2.07f,false,2});
    for(float bracket=y+.8f;bracket<end;bracket+=2.1f){
     m_structures.push_back({7.02f,bracket,7.25f,bracket+.14f,1.62f,2.07f,false,2});
     m_structures.push_back({16.59f,bracket,16.84f,bracket+.14f,1.62f,2.07f,false,2});
    }
   }
   for(float y:{7.55f,15.85f})m_structures.push_back({7.3f,y,16.65f,y+.3f,0,.055f,false,2});
   // Cross aisles link all three galleries; bay dividers meet the outer walls.
   for(float y:{8.25f,17.25f}){wall(1,y,4.5f,y+.25f);wall(19.5f,y,23,y+.25f);}
   wall(7.05f,14,10.5f,14.25f);wall(13.5f,14,16.8f,14.25f);
   m_props={{0,{3.1f,5.3f},1.35f,2.2f,0,{1.1f,.66f},0},
            {1,{14.7f,17.f},1.12f,2.f,kPi*.5f,{.31f,1.f},0},
            {0,{20.5f,14.f},1.35f,2.2f,kPi,{1.1f,.66f},0},
            {1,{3.2f,21.2f},1.12f,2.f,kPi*.5f,{.31f,1.f},0},
            {0,{20.5f,5.9f},1.35f,2.2f,0,{1.1f,.66f},0}};
   m_fixtures={{7,{1.28f,12.5f},0,2,.5f,1.8f,kPi*.5f,true},
               {7,{1.28f,15.3f},0,2,.5f,1.8f,kPi*.5f,true},
               {8,{22.89f,18.5f},.8f,.7f,.2f,1.f,kPi*.5f,true}};
   m_terminals={{{8.f,4.2f},"REACTOR SERVICE / GALLERY 05","MAINTENANCE ROUTE BELOW REACTOR.","RETURN LINE ACCESS / KEEP CLEAR.",0,false},
                {{20.2f,12.f},"CABLE GALLERY / HIGH VOLTAGE","BUS BARS LIVE ALONG THE TRAYS.","KEEP CLEAR OF THE DIVIDER RAILS.",0,false}};
   // Authored maintenance machinery gives each bay a purpose and cover. Skids,
   // control cabinets and coolant manifolds are floor-anchored solids (shifted
   // to reactor elevation below); they sit inside bays, clear of the aisles.
   for(auto skid:{Structure{4.55f,2.15f,5.65f,3.15f,0,1.0f,false,6},   // west-upper pump skid
                  Structure{18.35f,2.15f,19.45f,3.05f,0,1.15f,false,6}, // east-upper control cabinet
                  Structure{8.35f,3.15f,9.45f,4.45f,0,1.25f,false,6},   // middle-upper coolant manifold (west of spine)
                  Structure{14.55f,3.15f,15.65f,4.45f,0,1.25f,false,6}, // middle-upper coolant manifold (east of spine)
                  Structure{4.35f,13.35f,5.55f,14.45f,0,1.05f,false,6}, // mid-west junction box
                  Structure{18.45f,13.35f,19.65f,14.45f,0,1.05f,false,6}, // mid-east junction box
                  Structure{4.35f,21.55f,5.35f,22.45f,0,.85f,false,3},  // lower-west crate cover
                  Structure{18.65f,21.55f,19.65f,22.45f,0,.85f,false,3}}) // lower-east crate cover
    m_structures.push_back(skid);
   // Extra authored props and wall equipment dress the galleries.
   m_props.push_back({1,{20.4f,15.6f},1.12f,2.f,kPi*.5f,{.31f,1.f},0}); // east-mid standpipe
   m_props.push_back({0,{5.0f,15.5f},1.35f,2.2f,0,{1.1f,.66f},0});      // west-mid generator
   m_props.push_back({1,{9.3f,16.4f},1.12f,2.f,0,{.31f,.31f},0});       // middle-south standpipe (west of spine)
   m_fixtures.push_back({7,{1.28f,3.4f},0,1.8f,.5f,1.8f,kPi*.5f,true}); // west-wall shelf
   m_fixtures.push_back({8,{22.89f,3.2f},.8f,.7f,.2f,1.f,kPi*.5f,true}); // east-wall cabinet
   m_fixtures.push_back({6,{20.3f,21.2f},0,1.87f,.55f,.99f,kPi*.5f,true}); // east-lower machine
   // Live bus bars run beside the two cable-tray dividers. The zones are off
   // the walkable galleries; brushing the trays hurts but never blocks a route.
   m_hazards.push_back({Hazard::Kind::Electricity,6.55f,3.f,7.35f,7.f,-9.f,-7.6f,12.f});
   m_hazards.push_back({Hazard::Kind::Electricity,16.55f,12.5f,17.35f,16.5f,-9.f,-7.6f,12.f});
   // Spark and dust wisps read as live electrical service; purely visual.
   m_particleEmitters.push_back({{7.1f,5.0f},-7.0f,{0.f,0.f},.6f,.5f,.05f,.15f,5});
   m_particleEmitters.push_back({{16.75f,14.f},-7.0f,{0.f,0.f},.6f,.5f,.05f,.15f,5});
   m_particleEmitters.push_back({{20.f,9.f},-7.5f,{-.15f,0.f}});
   // Accent lights over the added machinery.
   for(Vec2 p:{Vec2{5.f,2.6f},Vec2{19.f,2.6f},Vec2{5.f,14.f},Vec2{19.f,14.f}})m_lights.push_back({p,-6.3f});
  }else{
   // Shallow ramps on all sides let the player walk out of either flooded
   // trench. The middle bridge and both cross aisles remain dry.
   m_waterVolumes={{7.25f,7.8f,10.25f,10.7f,-9.40f,-9.045f},
                   {13.75f,7.8f,16.75f,10.7f,-9.40f,-9.045f},
                   {7.25f,14.2f,10.25f,17.2f,-9.40f,-9.045f},
                   {13.75f,14.2f,16.75f,17.2f,-9.40f,-9.045f}};
   // A dry central bridge runs between wet return trenches, flanked by pump bays.
   for(float x:{6.8f,17.f}){wall(x,0,x+.25f,5.f);wall(x,8.f,x+.25f,11.f);wall(x,14.f,x+.25f,18.f);wall(x,21.f,x+.25f,23);}
   for(float y:{10.75f,17.75f}){wall(1,y,4.5f,y+.25f);wall(19.5f,y,23,y+.25f);}
   // One continuous raised grated spine reads as a bridge above four basins.
   m_structures.push_back({10.5f,6.f,13.5f,19.f,0,.045f,false,2});
   for(float y:{7.95f,14.35f})for(float x:{10.42f,13.50f})
    m_structures.push_back({x,y,x+.08f,y+2.55f,.045f,.76f,true,2});
   m_props={{0,{3.f,5.3f},1.5f,2.5f,0,{1.25f,.75f},0},
            {0,{20.5f,11.8f},1.5f,2.5f,kPi,{1.25f,.75f},0},
            {1,{3.f,16.f},1.1f,1.9f,kPi*.5f,{.30f,.95f},0},
            {0,{3.4f,21.2f},1.5f,2.5f,0,{1.25f,.75f},0}};
   // Low curbs and supported safety rails outline flooded returns without
   // covering their four sloped entry/exit ends.
   for(const auto& water:m_waterVolumes)for(float x:{water.x1,water.x2}){
    m_structures.push_back({x-.04f,water.y1+.18f,x+.04f,water.y2-.18f,0,.075f,false,2});
   }
   m_fixtures={{7,{1.28f,20.5f},0,2,.5f,1.8f,kPi*.5f,true},
               {8,{22.89f,5.5f},.8f,.7f,.2f,1.f,kPi*.5f,true}};
   // Rear service store: the old sealed panel led nowhere. A personnel door
   // now opens into an actual enclosed bay within this chunk.
   wall(17.25f,20.65f,19.05f,20.87f);
   wall(20.45f,20.65f,22.85f,20.87f);
   wall(17.25f,22.85f,22.85f,23.08f);
   Door storeDoor{19.05f,20.45f,20.76f};storeDoor.swinging=true;
   m_doors.push_back(storeDoor);
   m_fixtures.push_back({7,{21.55f,22.56f},0,1.6f,.48f,1.8f,0,true});
   m_fixtures.push_back({8,{18.15f,22.74f},.78f,.67f,.20f,.91f,0,false});
   m_clutterSpawns.push_back({3,{21.1f,21.8f}});
   m_clutterSpawns.push_back({2,{18.5f,21.7f}});
   m_terminals={{{18.f,4.2f},"COOLANT RETURN / SECTOR 06","RETURN PRESSURE: UNSTABLE.","STEAM LEAKS AHEAD. USE THE HIGH WALKWAY.",0,false},
                {{5.5f,7.5f},"01 / COOLANT FEED","FEED PRESSURE NOMINAL.","VALVE LOCKED - SEE SECTOR CONTROL.",0,false},
                {{18.5f,17.5f},"02 / COOLANT RETURN","RETURN LINE VENTING STEAM.","DO NOT WADE THE TRENCHES.",0,false}};
   // Scalding steam vents from each flooded return. The zones sit below the dry
   // floor/bridge line (top < -9.0), so they punish wading the trenches while
   // leaving the raised walkway and cross aisles safe - hence "use the high
   // walkway". Hazards never affect collision or routing.
   for(const auto& water:m_waterVolumes)
    m_hazards.push_back({Hazard::Kind::Steam,water.x1,water.y1,water.x2,water.y2,-9.5f,-9.06f,9.f});
   // Visible steam rising off each basin makes the leaking returns readable.
   for(const auto& water:m_waterVolumes)
    m_particleEmitters.push_back({{(water.x1+water.x2)*.5f,(water.y1+water.y2)*.5f},-9.02f,{0.f,0.f},1.4f,.7f,.12f,.3f,12});
   // Pump-bay machinery flanks the returns without narrowing the dry margins.
   m_props.push_back({1,{5.4f,9.2f},1.1f,1.9f,kPi*.5f,{.30f,.95f},0});   // west-upper feed standpipe
   m_props.push_back({1,{18.6f,9.2f},1.1f,1.9f,kPi*.5f,{.30f,.95f},0});  // east-upper return standpipe
   m_props.push_back({0,{18.6f,15.8f},1.5f,2.5f,kPi,{1.25f,.75f},0});    // east-lower return pump
   m_fixtures.push_back({6,{4.75f,12.5f},0,1.6f,.5f,.9f,kPi*.5f,true});  // west-mid coolant pump
   m_fixtures.push_back({8,{22.89f,15.5f},.8f,.7f,.2f,1.f,kPi*.5f,true}); // east-wall control cabinet
   // Walkway and basin lighting.
   for(Vec2 p:{Vec2{12.f,6.f},Vec2{12.f,12.f},Vec2{12.f,18.f},Vec2{5.f,12.5f},Vec2{19.f,12.5f}})m_lights.push_back({p,-5.75f});
  }
  // Structures use absolute elevations; props/fixtures have floor-relative bases.
  for(auto&s:m_structures){s.bottom-=9.f;s.top-=9.f;}
  buildLayers({}); // Build the collision/occlusion index for these structures too.
  buildLights();
  return;
 }
 if(hasLift()){
  ScriptEvent reactorExit;reactorExit.id=stateId("event_reactor_exit_checkpoint");
  reactorExit.x1=20;reactorExit.y1=21;reactorExit.x2=23;reactorExit.y2=24;reactorExit.bottom=-9.5f;reactorExit.top=-7.5f;
  reactorExit.requireState=stateId("reactor_bulkhead_released");reactorExit.requireEnemiesClear=true;
  ScriptAction checkpoint;checkpoint.type=ScriptAction::Type::Checkpoint;reactorExit.actions.push_back(checkpoint);m_scriptEvents.push_back(reactorExit);
  m_layers={{"Reactor / lower containment",-9,0,LiftShaftGround},
            {"Reactor / upper manifold",-6,.3f,reactorDeck()},
            {"Scenery / coolant risers",-3,.25f,shaftScenery()},
            {"Surface lift / collar",0,.3f,collarDeck()},
            {"Scenery / ventilation plant",3,.25f,shaftScenery()},
            {"Scenery / electrical service",6,.25f,shaftScenery()},
            {"Surface / sealed gates",9,.35f,shaftScenery()}};
  // Entry bridge ends exactly at the lift's north door.
  m_structures.push_back({11,8,13,10,-.3f,0});
  // Cage the boarding bridge so a missed jump cannot strand the player below
  // the uncalled cab with no way to complete the mandatory descent sequence.
  m_structures.push_back({10.95f,8,11,10,0,2.1f,true});
  m_structures.push_back({13,8,13.05f,10,0,2.1f,true});
  m_structures.push_back({2,0,5,1,-9,-.3f});
  m_structures.push_back({10.5f,18.5f,13.5f,21.5f,-9,-3.4f,false,1});
  // Only occupied spaces retain full room layouts.
  for(float z:{-9.f,-6.f,0.f}){
   m_structures.push_back({7,2,7.2f,5,z,z+2.65f,false,3});
   m_structures.push_back({7,7,7.2f,9,z,z+2.65f,false,3});
   m_structures.push_back({2,16,5,16.2f,z,z+2.65f,false,3});
   m_structures.push_back({7,16,9,16.2f,z,z+2.65f,false,3});
  }
  // Cheap opaque ceilings preserve the boarding and reactor rooms below the
  // scenery floors. The central shaft stays open; no distant upper rooms exist.
  for(float z:{-3.3f,2.7f}){
   m_structures.push_back({1,1,23,9,z,z+.12f,false,2});
   m_structures.push_back({1,15,23,23,z,z+.12f,false,2});
   m_structures.push_back({1,9,9,15,z,z+.12f,false,2});
   m_structures.push_back({15,9,23,15,z,z+.12f,false,2});
  }
  for(float z:{-3.f,3.f,6.f,9.f}){
   // Backing walls stop sightlines behind each one-metre-deep scenic bay.
   m_structures.push_back({7.8f,7.8f,8,16.2f,z,z+2.75f,false,3});
   m_structures.push_back({16,7.8f,16.2f,16.2f,z,z+2.75f,false,3});
   m_structures.push_back({8,7.8f,16,8,z,z+2.75f,false,3});
   m_structures.push_back({8,16,16,16.2f,z,z+2.75f,false,3});
   m_structures.push_back({7.8f,7.8f,9,16.2f,z+2.65f,z+2.7f,false,2});
   m_structures.push_back({15,7.8f,16.2f,16.2f,z+2.65f,z+2.7f,false,2});
   m_structures.push_back({9,7.8f,15,9,z+2.65f,z+2.7f,false,2});
   m_structures.push_back({9,15,15,16.2f,z+2.65f,z+2.7f,false,2});
  }
  // Deliberate perimeter piers replace the automatic forest of thin posts.
  for(float x:{1.3f,22.3f})for(float y:{16.7f,22.f})
   m_structures.push_back({x,y,x+.4f,y+.4f,-9,-3.3f,false,3});
  for(float y:{16.7f,22.f})m_structures.push_back({1.3f,y,22.7f,y+.4f,-3.65f,-3.3f,false,2});
  // Console cabinets use the source model's proportions, with matching collision.
  for(float x:{17.35f,18.85f})m_structures.push_back({x-.313f,19.04f,x+.313f,19.46f,-9,-7.812f,false,6});
  // Maintenance workbench, with open leg space and a correctly supported disk.
  // Boarding-level freight and traction equipment. Bounds match visible props;
  // the north approach and 11..13 metre boarding bridge remain unobstructed.
  for(Vec2 cargo:{Vec2{4.45f,7.9f},Vec2{5.75f,9.25f}})m_structures.push_back({cargo.x-.525f,cargo.y-.525f,cargo.x+.525f,cargo.y+.525f,0,.85f,false,6});
  m_structures.push_back({6.175f,13.075f,6.825f,13.725f,0,.95f,false,6});
  m_structures.push_back({15.97f,10.74f,17.9f,11.86f,0,1.71f,false,6});
  m_structures.push_back({15.5f,14.9f,18.5f,15.05f,0,2.55f,false,2});
  m_structures.push_back({4,12.4f,8.85f,12.95f,2.15f,2.55f,false,6});
  m_structures.push_back({5.1f,20.65f,6.9f,21.4f,-5.45f,-5.4f,false,2});
  for(float x:{5.15f,6.75f})for(float y:{20.7f,21.25f})m_structures.push_back({x,y,x+.08f,y+.08f,-6,-5.45f,false,2});
  // Pump plinths, cable trunks and service-bay partitions leave the stair route clear.
  m_structures.push_back({2.8f,17.2f,5.5f,19.1f,-9,-8.85f,false,3});
  m_structures.push_back({15.2f,20.55f,18.2f,22.f,-9,-8.88f,false,3});
  for(float z:{-9.f,-6.f}){
   m_structures.push_back({2.2f,17.1f,2.38f,22.5f,z+2.1f,z+2.28f,false,2});
   m_structures.push_back({15.1f,22.3f,22.5f,22.48f,z+2.1f,z+2.28f,false,2});
  }
  buildLayers(ReactorStairs);
  m_doors={{2,5,.5f,0,false,false,true,9},{20,23,21.5f,0,false,true}};
  m_doors.back().requireEnemiesClear=true;m_doors.back().requireControl=true;
  m_terminals={{{4.5f,2.5f},"LIFT SYSTEM / OVERRIDE","SURFACE GATES CLAMPED SHUT.","BOARD CAB. USE DISPATCH CONTROL.",9},
               {{13.35f,11.5f},"CAB CONTROL / DISPATCH","ASCENDING TO SURFACE.","WARNING: CABLE TENSION CRITICAL.",9,true},
               {{8.3f,21.2f},"MAINTENANCE / SHIFT LOG","RETURN TRIPPED AGAIN. NO FEED PRESSURE.","LEFT THE SERVICE DISK WITH THE SPARES.",3},
               {{18.1f,19.1f},"REACTOR ACCESS / DRIVE A:","","",0,false,1},
               {{6.3f,18.2f},"01 / COOLANT FEED","","",0,false,2},
               {{17.1f,19.6f},"02 / COOLANT RETURN","","",3,false,3}};
  m_props={{0,{4.15f,18.15f},1.25f,2.2f,0,{1.1f,.66f},.15f},{1,{16.7f,21.2f},1.12f,2.f,0,{1.f,.31f},.12f},
           {2,{5.1f,22.15f},.3f,4.2f,kPi*.5f,{2.1f,.15f}}};
  m_fixtures={{6,{4.2f,20.2f},0,1.87f,.55f,.99f,0,true},
              {7,{3.6f,21.8f},3,2.052f,.61217f,1.44f,0,true},
              {7,{16.2f,21.6f},3,2.052f,.61217f,1.44f,0,true}};
  for(float y:{16.7f,18.4f,20.1f})m_fixtures.push_back({8,{22.9f,y},.5f,.66776f,.1156f,.90576f,kPi*.5f,true});
  for(float x:{15.9f,17.1f})m_fixtures.push_back({8,{x,22.87f},3.55f,.66776f,.1156f,.90576f,0,true});
  m_fixtures.push_back({7,{6.2f,11.f},9,2.052f,.61217f,1.44f,kPi*.5f,true});
  m_fixtures.push_back({6,{16.9f,13.2f},9,2.38f,.70f,1.26f,0,true});
  for(float x:{16.f,17.f,18.f})m_fixtures.push_back({8,{x,14.83f},9.7f,.66776f,.1156f,.90576f,0,true});
  // Explicit ceiling-mounted lights: no fixtures generated from narrow rail spans.
  for(float z:{-9.f,-6.f,0.f})for(Vec2 p:{Vec2{4,6},Vec2{16,6},Vec2{21,6},Vec2{4,19},Vec2{8,22},Vec2{16,22},Vec2{21,19}}){
   float ceiling=clearanceAbove(p.x,p.y,z);if(ceiling-z>1.8f)m_lights.push_back({p,ceiling-.15f});
  }
  for(float z:{-3.f,3.f,6.f,9.f})for(Vec2 p:{Vec2{8.5f,11.5f},Vec2{15.5f,12.5f}})m_lights.push_back({p,clearanceAbove(p.x,p.y,z)-.15f});
  m_lights.push_back({{12,11.8f},2.45f});refreshReactorTerminals();
  return;
 }
 m_fixtures=MapFixtures[m_level];
 m_layers={m_level==0?FoundryGroundLayer:m_level==1?PressureWorksGroundLayer:TurbineGantryGroundLayer};
 // Each pair (or remaining single tile) owns one complete generator. Preserve
 // the supplied 3.4 x 1.0 x 1.8 metre proportions for rendering and collision.
 for(int y=0;y<Height;++y)for(int x=0;x<Width;){
  if(tile(x,y)!='G'){++x;continue;}
  int cells=tile(x+1,y)=='G'?2:1;float width=cells*.94f,scale=width/3.4f;
  m_fixtures.push_back({6,{x+cells*.5f,y+.5f},0,width,scale,1.8f*scale,0,true});x+=cells;
 }
 if(m_level==2){
  m_layers.push_back(TurbineGantryUpperLayer);
  m_internalWallHeight=2.7f;
  // The ground-level transfer tunnels intentionally punch through the outer
  // shell, but those openings must not continue above the door headers.
  // Catwalk rails stay jumpable for parkour; these are visible upper walls at
  // the two actual map exits so a successful jump cannot leave the level.
  m_structures.push_back({2.f,0.f,5.f,.18f,2.5f,6.f,false,3});
  m_structures.push_back({20.f,23.82f,23.f,24.f,2.5f,6.f,false,3});
  buildLayers(GantryStairs);
  m_doors={{2,5,.5f,0,false,false,true},{20,23,21.5f,0,false,true}};
  m_doors.back().requireEnemiesClear=true;m_doors.back().requireControl=true;
  m_terminals={{{18.5f,9.5f},"GANTRY CONTROL / 05:42","TURBINE BRAKES RELEASED.","LOWER TRANSFER INTERLOCK UNSEALED.",3,true}};
  buildLights();m_lights.push_back({{5,20},5.85f});
  return;
 }
 if(m_level==1){
  m_props={{0,{11.5f,11.f},1.5f,2.8f,0,{1.4f,.8f}},
           {0,{11.5f,14.f},1.5f,2.8f,kPi,{1.4f,.8f}},
           {1,{20.f,10.6f},1.545f,2.7f,kPi*.5f,{.415f,1.35f}},
           {1,{4.f,10.5f},1.374f,2.4f,0,{1.2f,.369f}},
           {2,{7.f,12.5f},.1928f,2.8f,0,{.0964f,1.4f}},
           {3,{13.f,3.5f},.96f,3.f,0,{1.5f,.03f}}};
 }
 for(int y:m_level==0?std::initializer_list<int>{8,16}:std::initializer_list<int>{7,17})for(int x=1;x<Width-1;){if(solid(x+.5f,y+.5f)){++x;continue;}int start=x;while(x<Width-1&&!solid(x+.5f,y+.5f))++x;m_doors.push_back({float(start),float(x),float(y)+.5f});}
 m_doors.push_back({20,23,21.5f,0,false,true});
 m_doors.back().requireEnemiesClear=true;
 if(m_level==1)m_doors.insert(m_doors.begin(),{2,5,.5f,0,false,false,true});
 m_terminals={{{2.2f,5.5f},"SHIFT LOG / 03:17","COOLANT FAILED. THE NIGHT CREW","SEALED FOUNDRY WITH US INSIDE."},
 {{16.5f,10.5f},"CONTAINMENT / 04:06","THE VESSELS WERE NOT EMPTY.","CLEAR THE HOSTILES. GET OUT."},
 {{18.5f,19.5f},"DISPATCH / LAST SIGNAL","PRESSURE WORKS STILL HAS POWER.","THE TRANSFER INTERLOCK IS LIVE."}};
 if(m_level==1)m_terminals={{{6.5f,2.5f},"RECEIVING / MANIFEST","THE LAST TRAIN ARRIVED EMPTY.","THE PRESSURE GAUGES KEPT RISING."},{{19.5f,20.5f},"CONTROL / NIGHT SHIFT","SURFACE LIFT IS ON EMERGENCY POWER.","THE PUMPS MUST STAY RUNNING."}};
 buildLights();
}
void World::buildLights(){
 for(int y=1;y<Height-1;y+=4)for(int x=2;x<Width-1;x+=5)if(tile(x,y)!='#')
  for(const auto&span:spansAt(x,y))if(span.ceiling-span.floor>1.8f)m_lights.push_back({{x+.5f,y+.35f},span.ceiling-.15f});
}
float World::floorHeight(float x,float y)const{
 if(outdoors())return m_layers.empty()?0.f:m_layers.front().elevation;
 for(const auto& water:m_waterVolumes)if(x>=water.x1&&x<water.x2&&y>=water.y1&&y<water.y2){
  float shore=std::min({x-water.x1,water.x2-x,y-water.y1,water.y2-y});
  return -9.f+(water.bed+9.f)*std::clamp(shore/.85f,0.f,1.f);
 }
 if(m_level>=4)return -9.f;
 if(hasLift())return -9.f;
 if(m_level==2)return 0;
 if(m_level==1){
  if(x>=20&&x<23&&y>=21&&y<23)return std::max(0.f,.8f-std::floor((y-21)*2)*.2f);
  if(x>=17&&x<23&&y>=9&&y<16)return y<13?1.2f:std::min(1.2f,std::floor((16-y)*2)*.2f);
  if(x>=12&&x<23&&y>=18&&y<23)return std::min(.8f,std::floor((y-18)*2)*.2f);
  if(x>=6&&x<9&&y>=10&&y<15)return y<11||y>=14?-.2f:-.4f;
  return 0;
 }
 // Foundry service deck: six 20 cm treads lead to a 1.2 m raised work area.
 if(x>=16&&x<23&&y>=9&&y<16){if(x<19&&y>=11&&y<13)return std::floor((x-16)*2)*.2f;if(y<13)return 1.2f;return std::min(1.2f,std::floor((16-y)*2)*.2f);}
 // Lower storage landing with a separate four-step approach from the south.
 if(x>=1&&x<7&&y>=17&&y<21){if(x>=5&&y<19)return std::floor((7-x)*2)*.2f;if(y<19)return .8f;return std::min(.8f,std::floor((21-y)*2)*.2f);}
 return 0;
}
float World::waterSurface(float x,float y)const{
 for(const auto& water:m_waterVolumes)if(x>=water.x1&&x<water.x2&&y>=water.y1&&y<water.y2)return water.surface;
 return -1000.f;
}
float World::ceilingHeight(float x,float y)const{
 if(outdoors())return 128.f; // Traversable sky; no ceiling geometry.
 if(m_level>=4)return m_level==4?-6.1f:-5.6f;
 if(hasLift())return 16.f;
 if(m_level==2)return 6.f;
 if(m_level==0&&y>=24&&x>=20&&x<23)return 3.4f;
 if(m_level==1&&y<0&&x>=2&&x<5)return 3.6f;
 if(m_level==1)return y<7?3.4f:y<17?4.8f:3.8f;
 if(int(x)==10&&int(y)==14)return .66f; // Crouch-only service bypass.
 return y<8?3.1f:y<16?4.2f:3.6f;
}
float World::supportHeight(float x,float y,bool dynamic,bool shelfCavities)const{
 for(const auto&fixture:m_fixtures)if(fixture.solid&&fixture.base<.025f&&insideFixture(fixture,x,y)){
  if(shelfCavities&&fixture.model==7)continue;
  return floorHeight(fixture.position.x,fixture.position.y)+fixture.base+fixture.height;
 }
 for(auto&p:m_props)if(p.base<.025f&&std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y)return floorHeight(p.position.x,p.position.y)+p.base+p.height;
 for(auto&terminal:m_terminals)if((dynamic||!hasLift()||!terminal.control)&&terminal.z==0&&std::fabs(x-terminal.position.x)<.27f&&std::fabs(y-terminal.position.y)<(terminal.control?.18f:.27f))return floorHeight(x,y)+.95f;
 float floor=floorHeight(x,y);switch(tile(int(std::floor(x)),int(std::floor(y)))){
 case '#':return wallHeight(int(std::floor(x)),int(std::floor(y)));case 'C':return floor+.60f;case 'B':return floor+1.1f;
 case 'T':return floor+2.62f;default:return floor;
 }
}
bool World::doorBlocks(float x,float y,float feet,float height)const{
 for(auto&door:m_doors){float base=floorHeight((door.left+door.right)*.5f,door.y)+door.z;
  if(door.swinging){
   float angle=door.open*kPi*.5f,width=door.right-door.left;
   Vec2 hinge{door.left,door.y},axis{std::cos(angle),std::sin(angle)};
   Vec2 delta=Vec2{x,y}-hinge;
   float along=std::clamp(dot(delta,axis),0.f,width);
   Vec2 nearest=hinge+axis*along;
   if(lengthSq(Vec2{x,y}-nearest)<.15f*.15f&&feet+height>base+.015f&&feet<base+2.35f)return true;
  }else if(x>door.left&&x<door.right&&std::fabs(y-door.y)<.13f&&feet+height>base+door.open*2.65f+.015f&&feet<base+door.open*2.65f+2.48f)return true;
 }
 return false;
}
float World::clearanceHeight(float x,float y)const{
 float height=ceilingHeight(x,y);for(auto&door:m_doors)if(x>door.left&&x<door.right&&std::fabs(y-door.y)<.62f)height=std::min(height,floorHeight((door.left+door.right)*.5f,door.y)+door.z+2.5f);
 // The dispatch board hangs from the vestibule ceiling, above the walking route.
 if(campaignChunk(0)&&x>20.17f&&x<22.83f&&y>22.90f&&y<23.04f)height=std::min(height,2.57f);
 return height;
}
bool World::rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors,bool dynamic,bool shelfCavities)const{
 auto delta=b-a;float dz=bz-az;int steps=std::max(1,int(std::ceil(std::sqrt(lengthSq(delta)+dz*dz)/.12f)));
 for(int i=1;i<steps;++i){float t=float(i)/steps;auto p=a+delta*t;float z=az+(bz-az)*t;
  if(!fits(p.x,p.y,z,.015f,dynamic,shelfCavities)|| (doors&&doorBlocks(p.x,p.y,z,.015f)))return false;
 }return true;
}
bool World::navigable(int x,int y,int nx,int ny,float height)const{
 if(solid(nx+.5f,ny+.5f))return false;
 float floor=floorHeight(nx+.5f,ny+.5f);
 return std::fabs(floor-floorHeight(x+.5f,y+.5f))<=.45f&&clearanceHeight(nx+.5f,ny+.5f)-floor>=height&&!doorBlocks(nx+.5f,ny+.5f,floor,height);
}
void World::updateDoors(float dt){for(auto&door:m_doors)door.open=std::clamp(door.open+(door.opening?1.f:-1.f)*dt*.85f,0.f,1.f);}
bool World::openDoor(int index){if(index<0||size_t(index)>=m_doors.size()||m_doors[index].opening)return false;m_doors[index].opening=true;return true;}
bool World::toggleDoor(int index){if(index<0||size_t(index)>=m_doors.size())return false;m_doors[index].opening=!m_doors[index].opening;return true;}
int World::nearbyDoor(Vec2 position,Vec2 forward,float feet)const{
 int best=-1;float distance=2.f;
 for(size_t i=0;i<m_doors.size();++i){auto&d=m_doors[i];Vec2 closest{std::clamp(position.x,d.left+.2f,d.right-.2f),d.y};auto delta=closest-position;float lengthTo=length(delta);
  if(std::fabs(feet-floorHeight((d.left+d.right)*.5f,d.y)-d.z)>1.25f)continue;
  if(lengthTo<distance&&dot(delta,forward)>-.15f){best=int(i);distance=lengthTo;}
 }return best;
}

char World::tile(int x, int y) const {
    if (x < 0 || y < 0 || x >= Width || y >= Height || m_layers.empty() || size_t(x)>=m_layers.front().rows[y].size()) return '#';
    return m_layers.front().rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

bool World::solid(float x, float y) const {
    for(const auto&fixture:m_fixtures)if(fixture.solid&&insideFixture(fixture,x,y,.2f))return true;
    for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x+.2f&&std::fabs(y-p.position.y)<p.halfSize.y+.2f)return true;
    for(auto&terminal:m_terminals)if(terminal.z==0&&int(x)==int(terminal.position.x)&&int(y)==int(terminal.position.y))return true;
    const char t = tile(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
    return t=='#'||t=='C'||t=='B'||t=='T';
}
bool World::wallSpaceFree(Vec2 center,Vec2 along,float width,float bottom,float top)const{
 Vec2 normal{-along.y,along.x};
 for(auto&f:m_fixtures){float base=floorHeight(f.position.x,f.position.y)+f.base;if(base>=top||base+f.height<=bottom)continue;
  Vec2 a{std::cos(f.yaw),-std::sin(f.yaw)},b{-a.y,a.x},delta=f.position-center;
  float tangentExtent=std::fabs(dot(along,a))*f.width*.5f+std::fabs(dot(along,b))*f.depth*.5f;
  float normalExtent=std::fabs(dot(normal,a))*f.width*.5f+std::fabs(dot(normal,b))*f.depth*.5f;
  if(std::fabs(dot(delta,along))<tangentExtent+width*.5f+.02f&&std::fabs(dot(delta,normal))<normalExtent+.06f)return false;
 }return true;
}
std::vector<Vec2> World::machines()const{
 std::vector<Vec2> result;for(auto&fixture:m_fixtures)if(fixture.model==6)result.push_back(fixture.position);for(auto&p:m_props)if(p.kind<2)result.push_back(p.position);return result;
}

bool World::isExit(float x, float y) const {
    return tile(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))) == 'X';
}


void World::buildLayers(std::span<const Staircase> stairs){
 for(const auto& layer:m_layers){
  for(auto row:layer.rows)if(row.size()!=Width)throw std::runtime_error("Invalid map layer row width");
  if(layer.thickness<=0)continue; // Ground tiles are read directly by tile().
  const float z=layer.elevation, underside=z-layer.thickness;
  const bool collarWall=hasLift()&&z==0;
  const float railHeight=collarWall?2.7f:.55f;
  auto deck=[&](int x,int y){return x>=0&&y>=0&&x<Width&&y<Height&&layer.rows[y][x]=='=';};
  auto stairConnection=[&](float x,float y){
   if(hasLift()&&z==0&&x>=11&&x<=13&&y>=7.9f&&y<=10)return true;
   for(const auto& stair:stairs)if(x>=stair.x1&&x<stair.x2&&y>=stair.y1&&y<stair.y2){
    float t=stair.alongY?(y-stair.y1)/(stair.y2-stair.y1):(x-stair.x1)/(stair.x2-stair.x1);
    if(!stair.ascending)t=1-t;
    float top=stair.bottom+(stair.top-stair.bottom)*(std::min(stair.steps-1,int(t*stair.steps))+1)/stair.steps;
    if(std::fabs(top-z)<.025f)return true;
   }
   return false;
  };
  for(int y=0;y<Height;++y)for(int x=0;x<Width;){
   if(!deck(x,y)){++x;continue;}int start=x;while(x<Width&&deck(x,y))++x;
   m_structures.push_back({float(start),float(y),float(x),float(y+1),underside,z});
  }
  for(int y=1;y<Height-1;++y)for(int x=1;x<Width-1;++x)if(deck(x,y)){
   if(!hasLift()&&(x+y)%5==0&&tile(x,y)!='#')
    m_structures.push_back({x+.06f,y+.06f,x+.14f,y+.14f,hasLift()?std::max(-9.f,z-3.f):floorHeight(x+.1f,y+.1f),underside});
   if(!deck(x-1,y)&&!(hasLift()&&tile(x-1,y)=='#')&&!stairConnection(x-.001f,y+.5f)){
    m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    // Seal the upper catwalk against a lower-layer wall.  Without this
    // backing panel the rail leaves a one-cell sightline into the void.
    if(tile(x-1,y)=='#')m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,hasLift()?z+2.7f:6.f,false});
   }
   if(!deck(x+1,y)&&!(hasLift()&&tile(x+1,y)=='#')&&!stairConnection(x+1.001f,y+.5f)){
    m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x+1,y)=='#')m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,hasLift()?z+2.7f:6.f,false});
   }
   if(!deck(x,y-1)&&!(hasLift()&&tile(x,y-1)=='#')&&!stairConnection(x+.5f,y-.001f)){
    m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x,y-1)=='#')m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,hasLift()?z+2.7f:6.f,false});
   }
   if(!deck(x,y+1)&&!(hasLift()&&tile(x,y+1)=='#')&&!stairConnection(x+.5f,y+1.001f)){
    m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x,y+1)=='#')m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,hasLift()?z+2.7f:6.f,false});
   }
  }
 }
 for(const auto& stair:stairs)for(int step=0;step<stair.steps;++step){
  float lo=float(step)/stair.steps,hi=float(step+1)/stair.steps;
  float top=stair.bottom+(stair.top-stair.bottom)*(stair.ascending?hi:1-lo);
  m_structures.push_back({
   stair.alongY?stair.x1:stair.x1+(stair.x2-stair.x1)*lo,
   stair.alongY?stair.y1+(stair.y2-stair.y1)*lo:stair.y1,
   stair.alongY?stair.x2:stair.x1+(stair.x2-stair.x1)*hi,
   stair.alongY?stair.y1+(stair.y2-stair.y1)*hi:stair.y2,stair.bottom,top,false,hasLift()?4:0});
 }
 m_structureCells.resize(Width*Height);
 for(uint16_t index=0;index<m_structures.size();++index){
  const auto&s=m_structures[index];
  for(int y=int(s.y1);y<int(std::ceil(s.y2));++y)for(int x=int(s.x1);x<int(std::ceil(s.x2));++x)
   if(x>=0&&x<Width&&y>=0&&y<Height)m_structureCells[y*Width+x].push_back(index);
 }
}
float World::wallHeight(int x,int y)const{return outdoors()?m_internalWallHeight:m_internalWallHeight>0&&x>0&&x<Width-1&&y>0&&y<Height-1?m_internalWallHeight:ceilingHeight(x+.5f,y+.5f);}
float World::supportBelow(float x,float y,float feet)const{
 float fixtureTop=-100;
 for(auto&f:m_fixtures)if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float top=base+f.height*tier;if(top<=feet+.025f)fixtureTop=std::max(fixtureTop,top);}
  else if(base+f.height<=feet+.025f)fixtureTop=std::max(fixtureTop,base+f.height);
 }
 for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float top=floorHeight(p.position.x,p.position.y)+p.base+p.height;if(top<=feet+.025f)fixtureTop=std::max(fixtureTop,top);}
 float result=floorHeight(x,y),base=supportHeight(x,y);if(base<=feet+.025f)result=base;
 result=std::max(result,fixtureTop);
 if(insideLift(x,y)&&m_liftHeight<=feet+.025f)result=std::max(result,m_liftHeight);
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.top<=feet+.025f)result=std::max(result,s.top);}
 return result;
}
float World::clearanceAbove(float x,float y,float feet)const{
 float ceiling=clearanceHeight(x,y);
 for(auto&f:m_fixtures)if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float underside=base+f.height*tier-.045f;if(underside>feet+.025f)ceiling=std::min(ceiling,underside);}
  else if(base>feet+.025f)ceiling=std::min(ceiling,base);
 }
 for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(base>feet+.025f)ceiling=std::min(ceiling,base);}
 if(insideLift(x,y)&&feet<m_liftHeight+2.6f)ceiling=std::min(ceiling,m_liftHeight+2.6f);
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.bottom>=feet+.025f)ceiling=std::min(ceiling,s.bottom);}
 for(auto&t:m_terminals){float base=floorHeight(t.position.x,t.position.y)+t.z;if(base>feet+.025f&&std::fabs(x-t.position.x)<.27f&&std::fabs(y-t.position.y)<.18f)ceiling=std::min(ceiling,base);}
 return ceiling;
}
bool World::fits(float x,float y,float feet,float height,bool dynamic,bool shelfCavities)const{
 if(dynamic&&insideLift(x,y)&&feet<m_liftHeight+2.8f&&feet+height>m_liftHeight-.25f){
  if(feet<m_liftHeight-.025f||feet+height>m_liftHeight+2.605f)return false;
  if(x<10.12f||x>13.88f)return false;
  bool northOpen=m_liftPhase==LiftPhase::Ready&&x>11&&x<13;
  bool southOpen=m_liftPhase==LiftPhase::Crashed&&m_liftTimer>=3.65f&&x>11&&x<13;
  if((y<10.12f&&!northOpen)||(y>13.88f&&!southOpen))return false;
 }
 if(feet<supportHeight(x,y,dynamic,shelfCavities)-.025f||feet+height>clearanceHeight(x,y)+.005f)return false;
 for(auto&f:m_fixtures)if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(shelfCavities&&f.model==7){
   for(float tier:ShelfTiers){float top=base+f.height*tier;if(feet<top-.005f&&feet+height>top-.045f)return false;}
   float dx=x-f.position.x,dy=y-f.position.y,c=std::cos(f.yaw),s=std::sin(f.yaw);
   float localX=std::fabs(dx*c-dy*s),localY=std::fabs(dx*s+dy*c);
   if(localX>f.width*.5f-.04f&&localY>f.depth*.5f-.04f&&feet<base+f.height&&feet+height>base)return false;
  }else if(feet<base+f.height-.025f&&feet+height>base+.005f)return false;
 }
 for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(feet<base+p.height-.025f&&feet+height>base+.005f)return false;}
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&feet<s.top-.025f&&feet+height>s.bottom+.005f)return false;}
 for(auto&t:m_terminals){if(!dynamic&&hasLift()&&t.control)continue;float base=floorHeight(t.position.x,t.position.y)+t.z;if(t.z!=0&&std::fabs(x-t.position.x)<.27f&&std::fabs(y-t.position.y)<.18f&&feet<base+.95f&&feet+height>base)return false;}
 return true;
}
bool World::railBlocksHull(float x,float y,float radius,float feet,float height)const{
 if(m_structureCells.empty())return false;
 int x0=std::max(0,int(std::floor(x-radius))),x1=std::min(Width-1,int(std::floor(x+radius)));
 int y0=std::max(0,int(std::floor(y-radius))),y1=std::min(Height-1,int(std::floor(y+radius)));
 for(int cy=y0;cy<=y1;++cy)for(int cx=x0;cx<=x1;++cx)for(auto index:m_structureCells[cy*Width+cx]){
  const auto&s=m_structures[index];if(!s.rail)continue;
  if(x+radius<=s.x1||x-radius>=s.x2||y+radius<=s.y1||y-radius>=s.y2)continue;
  if(feet<s.top-.025f&&feet+height>s.bottom+.005f)return true;
 }
 return false;
}
std::vector<Span> World::spansAt(int x,int y)const{
 float px=x+.5f,py=y+.5f,roof=ceilingHeight(px,py);std::vector<std::pair<float,float>> solids={{-100.f,supportHeight(px,py)}};
 for(auto&f:m_fixtures)if(f.solid&&f.base>.025f&&insideFixture(f,px,py)){float base=floorHeight(px,py)+f.base;solids.push_back({base,base+f.height});}
 for(auto&p:m_props)if(p.base>.025f&&std::fabs(px-p.position.x)<p.halfSize.x&&std::fabs(py-p.position.y)<p.halfSize.y){float base=floorHeight(px,py)+p.base;solids.push_back({base,base+p.height});}
 for(auto index:structureIndices(px,py)){auto&s=m_structures[index];if(px>=s.x1&&px<s.x2&&py>=s.y1&&py<s.y2)solids.push_back({s.bottom,s.top});}
 std::sort(solids.begin(),solids.end());std::vector<Span> result;float bottom=-100.f;
 for(auto solid:solids){if(solid.first>bottom)result.push_back({bottom,solid.first,0});bottom=std::max(bottom,solid.second);}
 if(bottom<roof)result.push_back({bottom,roof,0});return result;
}
}

