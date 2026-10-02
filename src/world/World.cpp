#include "World.h"
#include "CampaignMaps.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace retro {
namespace {
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
constexpr MapRows ReactorServiceGalleryGround = {
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
constexpr MapRows CoolantReturnGround = {
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
    "########...#############"
};
constexpr MapRows CableVaultsGround = {
    "##...###################",
    "##...###################",
    "##...###################",
    "##...###################",
    "##...###################",
    "##....................##",
    "##....................##",
    "###...................##",
    "###...................##",
    "###...................##",
    "###...................##",
    "###...................##",
    "###...................##",
    "###...................##",
    "###...................##",
    "######................##",
    "####...................#",
    "####...................#",
    "####...................#",
    "####...................#",
    "####...................#",
    "####...................#",
    "####...................#",
    "####################...#"
};
constexpr MapRows PumpAnnexLower = {
    "##...###################",
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
    "###################...##"
};
constexpr MapRows PumpAnnexMain = {
    "__===___________________",
    "_============______====_",
    "_============______====_",
    "_============______====_",
    "_=___========______====_",
    "_=___========______====_",
    "_=___========______====_",
    "_=___========______====_",
    "_=___========______====_",
    "_=___========______====_",
    "_=___========______====_",
    "_============______====_",
    "_============______====_",
    "_============______====_",
    "_============______====_",
    "_============______====_",
    "_======================_",
    "_======================_",
    "_======================_",
    "_======================_",
    "_======================_",
    "_======================_",
    "_======================_",
    "________________________"
};
constexpr MapRows PumpAnnexObservation = {
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "___________________===__",
    "_______________=======__",
    "_______________=======__",
    "________==============__",
    "________==============__",
    "________==============__",
    "___________________===__",
    "___________________===__",
    "___________________===__"
};
constexpr MapRows UtilityJunctionGround = {
    "##...###################",
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
    "####################...#"
};
constexpr MapRows UtilityJunctionUpper = {
    "__===___________________",
    "_=======___________===__",
    "_=======___________===__",
    "_=======___________===__",
    "_=======___________===__",
    "_=======___________===__",
    "_=======___________===__",
    "_=====================__",
    "_=====================__",
    "_==========_____________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________",
    "________________________"
};
constexpr MapRows WasteHandlingLower = {
    "##...###################",
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
    "####################...#"
};
constexpr MapRows WasteHandlingSortingDeck = {
    "__===___________________",
    "_======================_",
    "_======================_",
    "_======================_",
    "_======================_",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_====___________________",
    "_==========_____________",
    "_==========_____________",
    "_==========_____________",
    "________________________",
    "________________________",
    "________________________",
    "________________________"
};
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

constexpr MapRows ashfallRegion(int region){
 MapRows rows={
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................"};
 switch(region){
  case 0: rows[10]="..................CC....";rows[18]=".....##.................";break;
  case 1: rows[5]="...B....................";rows[17]="....................##..";break;
  case 2: rows[11]="................CC......";rows[20]="..##....................";break;
  case 3: rows[6]="....##..................";rows[18]="..................B.....";break;
  case 4: rows[4]="...................##...";rows[19]="....CC..................";break;
  case 5: rows[8]="..B.....................";rows[21]="..................##....";break;
  case 6: rows[6]="................CC......";rows[17]="...##...................";break;
  case 7: rows[9]="....................B...";rows[20]=".....##.................";break;
  case 8: rows[7]="...CC...................";rows[18]="...................##...";break;
  case 9: rows[5]="..................##....";rows[19]="....B...................";break;
  case 10: rows[8]=".....##.................";rows[21]=".................CC.....";break;
  case 11: rows[6]="....................##..";rows[18]="....CC..................";break;
 }
 return rows;
}
static_assert([]{for(int i=0;i<AshfallChunkCount;++i)for(auto row:ashfallRegion(i))if(row.size()!=24)return false;return true;}(),"Ashfall rows must be exactly 24 cells");

constexpr std::array<std::string_view,72> AshfallVoxelTop{{
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111222221111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111122222223222222111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111222333333333332111111111111111111111111111111111111122222111111111",
 "111111111111112222222221111112223333333333333111111111111222222222111111111111222222322222211111",
 "111111111112222223332222222212233432222222223111111222222223333322222222111112223332222222221111",
 "111111111112333333333333332222233432222222223111112222333333333333333222211122333332111111112111",
 "111111111112334444444443333222334432222222223111112233334444444444433332221122334432111111112111",
 "111111111112344455555444443322334432222222223111112333444445555544444333221122344532111111112111",
 "111111111112355555555555443322334432222222223111112334445555555555544433221223344532111111112211",
 "111111111112355666666655443332233432222222223111113333333333336665554433322223344532111111112211",
 "111111222222355667776655544332233432222222223111113222222222237666554443322223344532111111112211",
 "111111233333356677777665544333223333333333333211111211111111237766555443322122344532111111112111",
 "111111223344555667776655544332222333333333332211111211111111237666554443322122334432222222222111",
 "111111223334455666666655443332222222223222222211111211111111236665554433322122333333333333322111",
 "111111122334455555555555443322111111222221111111111211111111235555544433221112223333333332221111",
 "111111122334444455555444443322111111111111111111111211111111235544444333221111222222322222211111",
 "111111122233334444444443333222111111111111111111111211111111234444433332221111111122222111111111",
 "111111112223333333333333332221111111111111111111112211111111233333333222211111111111111111111111",
 "111111111222222223332222222211111111111111111111111211111111233322222222111111111111111111111111",
 "111111111111112222222221111111111111111111111111111111111222222222111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111222222211111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111122222222222222222111111111111111111111111111111111111111111122222111111111",
 "111111111222111111112222223333333332222221111111111111111111111111111111111111222222222222211111",
 "111111222222222111112233333333433333333221111111111112222222222211111111111122222233333222222111",
 "112222222222222222222233344444444444333222111111222222222222222222222111111122233333333333222111",
 "112222233333332222222333444455555444433322111122222111111111233332222221111222333334443333322211",
 "112233333333333332222333445555555554433322211122222111111111233333333221111223334444444443332211",
 "122233334444433332222334445566666554443322211222222111111111234444433222111223344445554444332211",
 "111111111111111132223334455566666555443332211222111111111111111144433322111223344555555544332211",
 "111111111111111111111111111111116554443322212222111111111111111111111111111111114555655544333221",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111222222221111",
 "122211111111123311111111111111111111111111111111111333333333336511111111111111111111222222221111",
 "122211111111123332222222223333331111111111111111111455666666666554443322211223341111222222221111",
 "122211111111123332221122222222222222111111112211111455555666555554433322211223344443222222222211",
 "112211111111123332211111111222222211111111111211111445555555555544433322111223334443222222222211",
 "112211111111122222211111111111111111111111111211111444444444444444433222111222333333222222222211",
 "112211111111122222211111111111111111111111111111111333344444443333333221111122233333222222222111",
 "111111222222222111111111111111111111111111111111111233333333333332222221111122222233222222222111",
 "111111111222111111111111111111111111111111111111111222222333222222222111111111222222222222211111",
 "111111111111111111111111111111111111111111111111111112222222222211111111111111111122222111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111112222222111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111222222222222222221111111111111111111",
 "111111111111111222222211111111111111111111111111111111111222222222333332222222221111111112111111",
 "111111111112222222222222221111111111111111111111111111112222233333333333333322222111112222222111",
 "111111112222222333333322222221111111111222222111111111112223333333444443333333222112222222222222",
 "111111112222233333333333332221111112222222333111112211122233334444444444444333322212222222222222",
 "111111111111233444444433333221111122233333333111112221122233344444455544444433322212111111112322",
 "111111111111234444444444433222111222222222222222223322122233344455555555544433322222111111112332",
 "111111111111234555555544433322111222111111112222223322222333444555556555554443332222111111112332",
 "111111111111235555555554433322211222111111112222223322222333444555666665554443332222111111112332",
 "111111111111235566666554443322212232111111112222223332223333333556666666555444333222111111112332",
 "111111111111235566666555443332212232111111112222223222222222223555666665554443332222111111112333",
 "111111111111235566666554443322212332111111112222223211111111123555556555554443332222111111112332",
 "111111222222235555555554433322212232111111112322222211111111123455555555544433322222222222222332",
 "111111122333334555555544433322112232111111112322222211111111123444455544444433322222333333333332",
 "111111122233444444444444433222111222222222222322222211111111123444444444444333322222333344433332",
 "111111112233333444444433333221111223333333333322222211111111123333444443333333222112233333333322",
 "111111112223333333333333332221111223333444444422222211111111123333333333333322222112222333332222",
 "111111112222222333333322222221111122233333333322222211111111122222333332222222221112222222222222",
 "111111111112222222222222221111111112222222333222222211111111122222222222222221111111112222222111",
 "111111111111111222222211111111111111111222222222111111111111111112222222111111111111111112111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
}};
static_assert([]{for(auto row:AshfallVoxelTop)if(row.size()!=96)return false;return true;}(),"Ashfall voxel rows must be 96 cells wide");
int ashfallVoxelTop(int worldX,int worldY){
 if(worldX>=96){
  // The old eastern seam remains at height one. A walkable stone shelf rises
  // inland, then falls through sea level into an actual submerged seabed.
  int x=worldX-96;
  int shelf=std::clamp((x-2)/4,0,2);
  int y=std::clamp(worldY,0,71);
  int bend=int(std::round(1.5*std::sin(y*.19)+.7*std::sin(y*.47)));
  int erosion=std::max(0,x-9+bend);
  return std::clamp(1+shelf-(erosion*3)/4,-9,4);
 }
 if(worldX<0||worldY<0||worldX>=96||worldY>=72)return 1;
 char value=AshfallVoxelTop[size_t(worldY)][size_t(worldX)];
 return value>='0'&&value<='9'?value-'0':1;
}
bool authoredAshfallVoxel(int worldX,int worldY,int worldZ){
 return worldZ<=ashfallVoxelTop(worldX,worldY);
}
std::uint8_t authoredAshfallMaterial(int worldX,int worldY,int worldZ){
 int top=ashfallVoxelTop(worldX,worldY);
 if(worldX>=96)return top<=1?TerrainSoil:TerrainRock;
 return worldZ<top-1||top>=5?TerrainRock:TerrainSoil;
}

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
bool benchTerminal(const World& world,const Terminal& terminal){return world.campaign()&&world.level()>=6&&!terminal.control&&terminal.reactorAction<2;}
bool insideTerminal(const World& world,const Terminal& terminal,float x,float y){
 if(benchTerminal(world,terminal))return x>terminal.position.x-.48f&&x<terminal.position.x+.42f&&std::fabs(y-terminal.position.y)<.36f;
 return std::fabs(x-terminal.position.x)<.27f&&std::fabs(y-terminal.position.y)<.18f;
}
float terminalHeight(const World& world,const Terminal& terminal,float x,float y){
 if(!benchTerminal(world,terminal))return .95f;
 float yaw=world.level()==6?-kPi*.5f:terminal.yaw,c=std::cos(yaw),s=std::sin(yaw);
 float dx=x-terminal.position.x,dy=y-terminal.position.y;
 // The monitor has its own narrow footprint. Treating its full height as a
 // desk-sized box made objects stand on invisible space beside the screen.
 if(std::fabs(dx*c-dy*s)<.1932545f&&std::fabs(dx*s+dy*c)<.266175f)return 1.32005f;
 float frontX=std::sin(yaw)*.22f,frontY=std::cos(yaw)*.22f;
 if(std::fabs((dx-frontX)*c-(dy-frontY)*s)<.14f&&std::fabs((dx-frontX)*s+(dy-frontY)*c)<.105f)return .85f;
 return .815f;
}
}

void World::buildPopulation(){
 using C=CreatureKind;using P=PickupKind;
 if(!campaign()){
  if(coast()){
   // Keep encounters on the dry headland; the eastern half of each chunk is
   // submerged and ordinary grounded actors cannot navigate its seabed.
   m_creatureSpawns.push_back({C::Wasp,{5.5f,17.5f},-999});
   if(m_level==13)m_pickupSpawns.push_back({{4.5f,7.5f},P::Ammo});
   return;
  }
  const Vec2 encounter[]={{18.5f,18.5f},{5.5f,18.5f},{18.5f,5.5f},{5.5f,5.5f}};
  auto p=encounter[m_level%4];
  if(m_level%5!=3)m_creatureSpawns.push_back({m_level%4==2?C::Wasp:m_level%6==5?C::Brute:C::Huntsman,p,-999});
  if(m_level==6||m_level==10)m_creatureSpawns.push_back({C::Wasp,{19.5f,18.5f},-999});
  if(m_level%3==0)m_pickupSpawns.push_back({{4.5f,19.5f},P::Ammo});
  if(m_level==5||m_level==11)m_pickupSpawns.push_back({{20.5f,4.5f},P::Health});
  int junk=2+(m_level%2);
  for(int i=0;i<junk;++i)m_clutterSpawns.push_back({(i+m_level)%6,{3.5f+i*1.4f,20.5f-float((i+m_level)%2)*1.2f},-999,(i+m_level)*.7f});
  return;
 }
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

World::World(int level,std::shared_ptr<const AuthoredMapData> map):m_level(level),m_worldId(WorldId::Custom){loadAuthoredMap(std::move(map));}
void World::loadAuthoredMap(std::shared_ptr<const AuthoredMapData> map){
 m_lightSolids.clear();m_lightNodes.clear();
 m_collisionCells={};m_collisionAll={};
 if(!map)throw std::runtime_error("Missing authored map data");
 if(map->layers.empty())throw std::runtime_error("Authored map has no floor layers");
 const auto& lift=map->cargoLift;
 if(lift.x1!=lift.x2||lift.y1!=lift.y2||lift.lower!=lift.upper){
  if(lift.x1>=lift.x2||lift.y1>=lift.y2||lift.lower>=lift.upper||lift.speed<=0||!lift.callState||!lift.releaseState||!lift.positionState||!lift.downState||!lift.arrivedState||!lift.descendedState)throw std::runtime_error("Invalid authored cargo lift or control states");
 }
 for(const auto& sequence:map->timedSequences)if(!sequence.timerState||!sequence.finishState||sequence.durationMs<1||sequence.durationMs>3600000||sequence.finishAtMs<1||sequence.finishAtMs>sequence.durationMs||sequence.soundIntervalMs<0||sequence.x1>sequence.x2||sequence.y1>sequence.y2||sequence.bottom>sequence.top||sequence.gain<0||sequence.pitch<=0)throw std::runtime_error("Invalid authored timed sequence");
 for(const auto& s:map->timedSequences)if(s.sightActor<-1||s.sightActor>=int(map->actorTracks.size())||!std::isfinite(s.sightDistance)||s.sightDistance<=0)throw std::runtime_error("Invalid sequence sight target");
 auto timerExists=[&](StateId id){return std::any_of(map->timedSequences.begin(),map->timedSequences.end(),[&](const auto& s){return s.timerState==id;});};
 for(const auto& track:map->actorTracks){
  if(!timerExists(track.timerState)||track.scale<=0||track.scale>20||!std::isfinite(track.scale)||track.keys.size()<2||track.keys.front().timeMs!=0||track.footprint.x<=0||track.footprint.y<=0||track.thickness<=0||!std::isfinite(track.footprint.x)||!std::isfinite(track.footprint.y)||!std::isfinite(track.thickness))throw std::runtime_error("Invalid authored actor track");
  int previous=-1;for(const auto& k:track.keys){if(k.timeMs<=previous||k.timeMs>3600000||!std::isfinite(k.position.x)||!std::isfinite(k.position.y)||!std::isfinite(k.z)||!std::isfinite(k.yaw)||!std::isfinite(k.phase)||k.phase<0||k.phase>64||k.lookAtActor<-1||k.lookAtActor>=int(map->actorTracks.size())||k.clip<0||k.clip>(track.visual==ActorVisual::Worker?5:4))throw std::runtime_error("Invalid actor pose key");previous=k.timeMs;}
  if(!track.idleKeys.empty()){
   if(track.idleKeys.size()<2||track.idleKeys.front().timeMs!=0||track.idleUntilMs<1||track.approachUntilMs<=track.idleUntilMs||track.approachUntilMs>track.keys.back().timeMs)throw std::runtime_error("Invalid actor prelude");
   int previous=-1;for(const auto& k:track.idleKeys){if(k.timeMs<=previous||k.timeMs>3600000||!std::isfinite(k.position.x)||!std::isfinite(k.position.y)||!std::isfinite(k.z)||!std::isfinite(k.yaw)||!std::isfinite(k.phase)||k.phase<0||k.phase>64||k.clip<0||k.clip>(track.visual==ActorVisual::Worker?5:4)||k.lookAtActor!=-1)throw std::runtime_error("Invalid actor idle key");previous=k.timeMs;}
  }
  if(track.health<0||track.health>100000||(track.health&&(!track.damageState||!track.deadState)))throw std::runtime_error("Invalid actor health states");
 }
 for(const auto& cue:map->sequenceCues)if(!timerExists(cue.timerState)||cue.timeMs<0||int(cue.sound)<0||int(cue.sound)>=int(Sound::Count)||cue.gain<0||cue.pitch<=0||!std::isfinite(cue.gain)||!std::isfinite(cue.pitch))throw std::runtime_error("Invalid actor sequence cue");
 for(const auto& layer:map->layers){
  if(!std::isfinite(layer.elevation)||!std::isfinite(layer.thickness)||layer.thickness<0)throw std::runtime_error("Invalid authored layer elevation or thickness");
  for(const auto& row:layer.rows)if(row.size()!=Width)throw std::runtime_error("Authored map rows must match the chunk width");
 }
 for(const auto& fixture:map->fixtures)if(fixture.model<0||fixture.model>=FacilityModelCount||fixture.width<=0||fixture.depth<=0||fixture.height<=0)throw std::runtime_error("Invalid authored fixture model or dimensions");
 for(const auto& sign:map->signs)if(!std::isfinite(sign.width)||!std::isfinite(sign.height)||!std::isfinite(sign.z)||!std::isfinite(sign.yaw)||!std::isfinite(sign.position.x)||!std::isfinite(sign.position.y)||sign.width<=0||sign.height<=0)throw std::runtime_error("Invalid authored sign dimensions");
 for(const auto& prop:map->props)if(prop.kind<0||prop.kind>=4)throw std::runtime_error("Invalid authored prop model");
 for(const auto& stair:map->stairs)if(stair.steps<1||stair.steps>4096||stair.x1>=stair.x2||stair.y1>=stair.y2||stair.bottom>stair.top)throw std::runtime_error("Invalid authored staircase");
 m_mapData=std::move(map);
 m_actorPoses.clear();for(const auto& track:m_mapData->actorTracks)m_actorPoses.push_back(track.idleKeys.empty()?sampleActor(track,0):sampleActorKeys(track.idleKeys,0,true));
 m_openNorthBoundary=m_mapData->openNorth;m_openSouthBoundary=m_mapData->openSouth;
 m_openWestBoundary=m_mapData->openWest;m_openEastBoundary=m_mapData->openEast;
 m_doors=m_mapData->doors;m_props=m_mapData->props;m_fixtures=m_mapData->fixtures;m_pipes=m_mapData->pipes;m_lights=m_mapData->lights;
 m_creatureSpawns=m_mapData->creatureSpawns;m_pickupSpawns=m_mapData->pickupSpawns;m_clutterSpawns=m_mapData->clutterSpawns;
 m_waterVolumes=m_mapData->waterVolumes;m_hazards=m_mapData->hazards;m_compactors=m_mapData->compactors;m_structures=m_mapData->structures;
 m_cargoLift=m_mapData->cargoLift;m_cargoLiftHeight=m_cargoLift.lower;
 m_layers.reserve(m_mapData->layers.size());
 // Boundary doors are bounded apertures. The tall hall above their frames is
 // solid structure, not a second route over a locked door into another floor.
 for(const auto& door:m_doors)if(door.entry||door.transfer){
  float base=m_mapData->layers.front().elevation+door.z,top=m_mapData->definition.ceiling;
  if(top>base+2.65f)m_structures.push_back({door.left,door.y-.14f,door.right,door.y+.14f,base+2.65f,top,false,3});
 }
 for(const auto& source:m_mapData->layers){
  MapRows rows{};for(size_t row=0;row<rows.size();++row)rows[row]=source.rows[row];
  m_layers.push_back({source.name,source.elevation,source.thickness,rows});
 }
 m_terminals.reserve(m_mapData->terminals.size());
 for(const auto& source:m_mapData->terminals)m_terminals.push_back({source.position,source.title.c_str(),source.line1.c_str(),source.line2.c_str(),source.z,source.control,source.reactorAction,source.activateState,source.toggleState,kPi,source.requireState});
 buildLayers(m_mapData->stairs);
}

World::World(int level,WorldId id):m_worldId(id) {
 if(worldChunkCount(id)<=0)throw std::invalid_argument("Custom worlds require authored map data");
 m_level=std::clamp(level,0,worldChunkCount(m_worldId)-1);
 if(campaign())if(auto map=campaignMap(m_level)){loadAuthoredMap(std::move(map));return;}
 buildPopulation();
 if(horrorMode()){
  const char* regionNames[]={
   "Ashfall / west approach","Ashfall / ridge road","Ashfall / dry interchange","Ashfall / east escarpment",
   "Ashfall / motel flats","Ashfall / relay crossroads","Ashfall / scrap basin","Ashfall / utility mesa",
   "Ashfall / south wash","Ashfall / dead subdivision","Ashfall / breaker yard","Ashfall / evacuation edge",
   "Ashfall Coast / north headland","Ashfall Coast / tidal shelf","Ashfall Coast / south bluffs"};
  auto rows=ashfallRegion(m_level);
  m_layers={{regionNames[m_level],0,0,rows}};
  int col=m_level>=12?4:m_level%4,row=m_level>=12?m_level-12:m_level/4;
  m_openWestBoundary=col>0;m_openEastBoundary=col<4;
  m_openNorthBoundary=row>0;m_openSouthBoundary=row<2;
  m_internalWallHeight=2.8f;
  buildTerrain();
  if(coast()){
   // One ocean record drives water physics and the visible sea; the terrain
   // mesh continues several metres beneath it for a proper shallow shelf.
   m_waterVolumes.push_back({0,0,24,24,-9.f,-.45f});
   buildLayers({});return;
  }

  constexpr Vec2 pads[]={{7,7},{15,8},{8,16},{16,9},{8,14},{16,16},{7,7},{16,15},{8,8},{16,10},{8,15},{16,8}};
  auto pad=pads[m_level];
  float base=floorHeight(pad.x,pad.y);
  auto junkShack=[&](float cx,float cy,float width,float depth,float height,int material,int variant){
   float x0=cx-width*.5f,x1=cx+width*.5f,y0=cy-depth*.5f,y1=cy+depth*.5f,t=.16f,door=.62f;
   m_structures.push_back({x0,y1-t,x1,y1,base,base+height,false,material});
   m_structures.push_back({x0,y0,x0+t,y1,base,base+height,false,material});
   if(variant!=2)m_structures.push_back({x1-t,y0,x1,y1,base,base+height*(variant?0.72f:1.f),false,material});
   m_structures.push_back({x0,y0,cx-door,y0+t,base,base+height,false,material});
   m_structures.push_back({cx+door,y0,x1,y0+t,base,base+height,false,material});
   if(variant==0)m_structures.push_back({x0+t,y0+t,cx-.15f,y1-t,base+height,base+height+.12f,false,2});
   if(variant==1)m_structures.push_back({cx+.25f,y0+t,x1-t,y1-t,base+height*.82f,base+height*.94f,false,2});
  };
  auto junkWall=[&](float x,float y,float length,bool alongY){
   if(alongY)m_structures.push_back({x-.10f,y,x+.10f,y+length,base,base+1.55f,false,3});
   else m_structures.push_back({x,y-.10f,x+length,y+.10f,base,base+1.55f,false,3});
  };

  switch(m_level){
   case 0:
    junkShack(pad.x,pad.y,5.6f,4.6f,2.35f,3,1);
    m_props.push_back({1,{pad.x+2.8f,pad.y+1.2f},1.1f,1.9f,kPi*.5f,{.30f,.95f},0});
    m_fixtures.push_back({8,{pad.x-2.35f,pad.y+.4f},.65f,.7f,.20f,1.f,kPi*.5f,true});
    break;
   case 1:
    junkWall(pad.x-2.5f,pad.y+1.8f,5.f,false);
    m_fixtures.push_back({6,{pad.x+1.3f,pad.y-.6f},0,1.8f,.55f,.95f,.25f,true});
    break;
   case 2:
    junkShack(pad.x,pad.y,6.2f,5.2f,2.5f,3,2);
    m_props.push_back({0,{pad.x-2.8f,pad.y+2.6f},1.35f,2.2f,.2f,{1.1f,.66f},0});
    break;
   case 3:
    junkWall(pad.x-3.f,pad.y,6.f,false);
    junkWall(pad.x+1.8f,pad.y-2.2f,4.4f,true);
    break;
   case 4:
    junkShack(pad.x,pad.y,5.2f,4.4f,2.2f,2,0);
    m_fixtures.push_back({7,{pad.x+1.9f,pad.y},0,1.7f,.5f,1.8f,kPi*.5f,true});
    break;
   case 5:
    junkShack(pad.x,pad.y,6.0f,5.2f,2.45f,3,0);
    m_props.push_back({0,{pad.x+3.4f,pad.y-1.4f},1.45f,2.35f,kPi*.5f,{1.15f,.7f},0});
    m_fixtures.push_back({8,{pad.x-2.55f,pad.y+.8f},.7f,.7f,.20f,1.f,kPi*.5f,true});
    break;
   case 6:
    junkWall(pad.x-2.6f,pad.y-2.f,5.2f,false);
    junkWall(pad.x-2.6f,pad.y-2.f,4.f,true);
    m_props.push_back({2,{pad.x+2.2f,pad.y+1.5f},.35f,3.5f,.4f,{1.7f,.18f},0});
    break;
   case 7:
    junkShack(pad.x,pad.y,5.8f,5.f,2.5f,2,1);
    m_fixtures.push_back({6,{pad.x-2.6f,pad.y+2.2f},0,1.7f,.55f,.95f,kPi*.5f,true});
    break;
   case 8:
    m_props.push_back({1,{pad.x+1.8f,pad.y},1.05f,1.8f,.7f,{.28f,.9f},0});
    break;
   case 9:
    junkShack(pad.x,pad.y,6.4f,4.8f,2.3f,3,2);
    junkWall(pad.x-3.5f,pad.y+2.9f,4.5f,false);
    break;
   case 10:
    junkWall(pad.x-3.f,pad.y-2.7f,6.f,false);
    junkWall(pad.x-3.f,pad.y+2.7f,6.f,false);
    m_fixtures.push_back({6,{pad.x,pad.y},0,2.2f,.7f,1.15f,.35f,true});
    m_props.push_back({0,{pad.x+3.1f,pad.y+.8f},1.35f,2.2f,kPi*.5f,{1.1f,.66f},0});
    break;
   case 11:
    junkShack(pad.x,pad.y,7.2f,5.8f,2.65f,3,0);
    junkWall(pad.x-4.f,pad.y+3.4f,8.f,false);
    m_fixtures.push_back({7,{pad.x+3.2f,pad.y+1.5f},0,1.8f,.5f,1.8f,kPi*.5f,true});
    break;
  }

  const auto wallCount=m_structures.size();
  for(size_t i=0;i<wallCount;++i){const auto wall=m_structures[i];if(std::fabs(wall.bottom-base)>.01f)continue;
   float bottom=base;
   for(float x=wall.x1;x<=wall.x2+.001f;x+=.2f)for(float y=wall.y1;y<=wall.y2+.001f;y+=.2f)
    bottom=std::min(bottom,floorHeight(x,y)-.12f);
   if(bottom<base-.02f)m_structures.push_back({wall.x1,wall.y1,wall.x2,wall.y2,bottom,base,false,3});
  }
  const char* relayLines[]={
   "WEST APPROACH / NO CIVIL TRAFFIC.","RIDGE ROAD / POWER LINES DOWN.","DRY INTERCHANGE / EAST ROUTE OPEN.","EAST ESCARPMENT / LONG RANGE VISIBILITY.",
   "MOTEL FLATS / STRUCTURES UNSAFE.","RELAY CROSSROADS / GRID INTERMITTENT.","SCRAP BASIN / SALVAGE SCATTERED.","UTILITY MESA / SUBSTATION DEAD.",
   "SOUTH WASH / FLASH FLOOD CHANNEL.","DEAD SUBDIVISION / NO RESPONSE.","BREAKER YARD / HIGH VOLTAGE ISOLATED.","EVACUATION EDGE / OUTER GATE LOST."};
  if(m_level==0||m_level==5||m_level==11)
   m_terminals={{{pad.x,pad.y-1.6f},"ASHFALL FIELD RELAY",relayLines[m_level],"96 X 72 M SURFACE GRID / LOCAL LINK.",0,false}};
  buildLayers({});return;
 }
 if(m_level>=6){
  using A=ScriptAction::Type;
  const float roof=definition().ceiling;
  auto wall=[&](float x1,float y1,float x2,float y2,float bottom,float top){m_structures.push_back({x1,y1,x2,y2,bottom,top,false,3});};
  auto shelf=[&](Vec2 p,float yaw=0.f,float z=0.f){m_fixtures.push_back({7,p,z,1.8f,.5f,1.8f,yaw,true});};
  auto cabinet=[&](Vec2 p,float yaw=0.f){m_fixtures.push_back({13,p,0,.9066f,.4956f,2.2f,yaw,true});};
  auto tank=[&](Vec2 p,float height){float scale=height/2.390135f;m_fixtures.push_back({14,p,0,1.58f*scale,1.62f*scale,height,0,true});};
  auto post=[&](float x,float y,float bottom,float top,float half=.12f){m_structures.push_back({x-half,y-half,x+half,y+half,bottom,top,false,2});};
  auto event=[&](const char* id,float x1,float y1,float x2,float y2,float lo,float hi,std::vector<ScriptAction> actions){
   ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=lo;e.top=hi;e.actions=std::move(actions);m_scriptEvents.push_back(std::move(e));
  };
  auto action=[](A type,StateId id=0,int value=0,float amount=0.f){ScriptAction a;a.type=type;a.id=id;a.value=value;a.amount=amount;return a;};
  std::vector<Staircase> stairs;
  // === DYNAMIC_CAMPAIGN_MAPS_START ===
  // Each standalone slot is intentionally an independent if block so the
  // injector can replace one level without rewriting adjacent campaign maps.
  // === LEVEL_6_START ===
  if(m_level==6){
   m_layers={{"Cable Vaults / service passages",-9,0,CableVaultsGround}};
   m_doors={{2,5,.5f,0,false,false,true},{20,23,23.5f,0,false,true}};
   m_doors.back().requireState=stateId("vault_disconnect");
   // Terminate the live run with short concrete cheeks rather than a solid
   // bulkhead across the player's first sightline. The open center is a dry
   // service crossing beyond the energized section of trough.
   wall(11,11,11.48f,11.35f,-9,-7.7f);
   wall(14.52f,11,15,11.35f,-9,-7.7f);
   // The surface sits below the surrounding deck and inside the recessed bed;
   // the renderer insets the water to the sloped shoreline.
   m_waterVolumes={{11,8.3f,15,10.7f,-9.45f,-9.20f}};
   wall(10.65f,8.f,10.95f,11.35f,-9.f,-8.35f);
   wall(15.05f,8.f,15.35f,11.35f,-9.f,-8.35f);
   Hazard arc{Hazard::Kind::Electricity,11,8.3f,15,10.7f,-9.4f,-7.5f,22};
   arc.enabledFlag=stateId("vault_disconnect");arc.invertFlag=true;arc.period=4;arc.onTime=1.2f;m_hazards.push_back(arc);

   // Shelves and switchgear neatly aligned to outer walls and piers (aisle kept clear)
   for(float y:{8.f,10.45f,13.35f})cabinet({21.55f,y},kPi*.5f);
   for(float y:{8.f,10.f,12.f,14.f})shelf({6.35f,y+.3f},kPi*.5f);
   for(float x:{11.12f,14.6f}){
    m_structures.push_back({x,6,x+.25f,21,-7.05f,-6.9f,false,2});
    for(float y:{6.f,9.f,12.f,15.f,18.f})m_structures.push_back({x,y,x+.25f,y+.08f,-6.9f,roof,false,2});
   }
   // Relay island breaks the rear sightline; both ends remain walkable.
   wall(9.8f,17.6f,10.15f,20.4f,-9,-6.7f);
   cabinet({10.5f,18.4f},-kPi*.5f);cabinet({10.5f,19.7f},-kPi*.5f);
   // Wall-backed cabinets along the west solid concrete perimeter wall
   cabinet({4.45f,18.5f},-kPi*.5f);cabinet({4.45f,20.0f},-kPi*.5f);cabinet({4.45f,21.5f},-kPi*.5f);

   // The live cable trough divides the chamber, but it must not divide the
   // play space into blind, parallel corridors. Heavy feeders make cover
   // islands on its flanks and leave readable walk-around lanes.
   m_fixtures.push_back({12,{8.45f,10.05f},0,2.0f,1.55f,1.75f,.10f,true});
   m_fixtures.push_back({12,{17.35f,15.85f},0,2.0f,1.55f,1.75f,-.08f,true});
   // Amber edge lights mark the two safe flanks at the live channel entrance.
   for(float x:{9.7f,16.3f})for(float y:{7.6f,11.7f})m_lights.push_back({{x,y},-4.92f});

   // Suspended high-voltage cable trays and distribution conduit headers
   m_pipes.push_back({{11.25f,5.5f},{11.25f,21.5f},-5.35f,.09f});
   m_pipes.push_back({{14.65f,5.5f},{14.65f,21.5f},-5.35f,.09f});
   for(float y:{7.5f,12.f,16.5f})m_pipes.push_back({{11.25f,y},{14.65f,y},-5.32f,.07f});
   m_pipes.push_back({{21.55f,6.f},{21.55f,21.5f},-5.2f,.10f});
   m_pipes.push_back({{4.45f,6.f},{4.45f,17.5f},-5.25f,.09f});
   m_pipes.push_back({{20.4f,11.9f},{21.55f,11.9f},-5.2f,.08f});

    // Wall-mounted breaker panels and ventilation grilles (non-solid, mounted flush)
    m_fixtures.push_back({8,{2.15f,4.f},1.1f,.67f,.20f,.91f,kPi*.5f,false});
    m_fixtures.push_back({8,{6.15f,16.8f},1.1f,.67f,.20f,.91f,kPi*.5f,false});
   m_fixtures.push_back({8,{21.85f,16.5f},1.1f,.67f,.20f,.91f,kPi*.5f,false});
   m_fixtures.push_back({8,{21.85f,11.9f},1.15f,1.15f,.22f,1.25f,kPi*.5f,false});
    m_fixtures.push_back({3,{6.15f,9.f},1.65f,1.2f,.15f,.8f,kPi*.5f,false});
    m_fixtures.push_back({3,{21.85f,9.f},1.65f,1.2f,.15f,.8f,kPi*.5f,false});
    // Make the electrified trench read as a maintained power channel: a pair
    // of flush disconnects and cable terminations frame its far bulkhead.
    for(float x:{12.15f,13.85f})
     m_fixtures.push_back({8,{x,10.91f},.20f,.72f,.18f,.91f,0,false});
    m_fixtures.push_back({3,{11.28f,10.88f},.18f,1.05f,.15f,.75f,0,false});
    m_fixtures.push_back({3,{14.72f,10.88f},.18f,1.05f,.15f,.75f,0,false});
    m_fixtures.push_back({6,{18.2f,7.f},0,1.87f,.55f,.99f,0,true});

   m_terminals={{{20.4f,11.9f},"FEEDER 7A / REMOTE TRIP","LIVE TRENCH / CROSS AT THE DRY SERVICE BRIDGE.","E / THROW LOCAL DISCONNECT.",0,false,0,stateId("vault_disconnect")}};
   m_terminals.push_back({{7.5f,6.4f},"CABLE VAULTS / MAINTENANCE","LOCAL DISCONNECT: EAST SWITCHGEAR AISLE.","SPARE LAMP AND SHELLS: SOUTH SERVICE BAY."});
   shelf({17.7f,19.8f});m_clutterSpawns={{3,{18,19}},{0,{18.5f,19.2f}}};
   event("vault_maintenance_kit",16.5f,18.5f,19,21,-9.2f,-7,{action(A::GiveItem,stateId("flashlight"),1)});
   event("vault_relay_trip",2,2,5,4,-9.2f,-7,{action(A::PlaySound,0,0,.6f)});
   m_scriptEvents.back().actions[0].sound=Sound::Door;
   m_creatureSpawns={{CreatureKind::Huntsman,{12.5f,20.5f},-9},{CreatureKind::Huntsman,{19.5f,8.5f},-9}};
   m_pickupSpawns={{{18,21.5f},PickupKind::Ammo}};
   for(Vec2 p:{Vec2{3.5f,3},Vec2{9,6.5f},Vec2{13,6.5f},Vec2{19,6.5f},Vec2{7.5f,13},Vec2{13,11.3f},Vec2{19.5f,14},Vec2{12,18},Vec2{21.5f,22},Vec2{18.5f,9.5f},Vec2{4.5f,18.5f}})m_lights.push_back({p,-5.02f});
  }
  // === LEVEL_6_END ===
  // === LEVEL_7_START ===
  if(m_level==7){
   m_layers={{"Pump Annex / lower manifold",-12,0,PumpAnnexLower},{"Pump Annex / main floor",-9,.25f,PumpAnnexMain},{"Pump Annex / observation",-4,.25f,PumpAnnexObservation}};
   m_doors={{2,5,.5f,0,false,false,true,3},{19.8f,21.2f,23.5f,0,false,true,false,8}};
   m_doors.back().swinging=true;
   m_doors.back().requireState=stateId("annex_running");

   // North entry vestibule: under floor sealed, overhead lintel sealed, passage completely clear
   wall(2,0,5,1,-12,-9);
   wall(2,0,5,0.85f,-6.5f,roof);

   // South transfer vestibule: framed cleanly around the swinging door with overhead lintel
   wall(19,23,22,24,-12,-4);
   wall(19,23.15f,19.8f,24,-4,roof);
   wall(21.2f,23.15f,22,24,-4,roof);
   wall(19.8f,23.15f,21.2f,24,-1.45f,roof);

   // Retaining bulkheads under the observation wing to eliminate open sightlines under slabs
   wall(8,18,8.25f,21,-9,-4);
   wall(8,18,15,18.25f,-9,-4);

   stairs={{2,4,5,11,-12,-9,16,true,false},{15,2,18,6,-12,-9,16,true,true},{15,6,18,16,-9,-4,26,true,true}};
    // Set the entry vessel off the stair's center sightline. Its reduced
    // diameter keeps a clear approach to the lower manifold and exit.
    for(Vec2 p:{Vec2{7.5f,4},Vec2{8,13},Vec2{12,19.5f}}){
     tank(p,4.6f);
     // Keep the supply header tight to the roof; the old -2.4 m run cut
     // through headroom over the -4 m observation deck.
     constexpr float headerZ=-1.32f;
     m_pipes.push_back({p,{p.x,22},headerZ,.28f});
     // Only the two tanks outside the observation deck get down-feed risers.
     // The former centre riser pierced the elevated walking surface.
     if(p.x<10.f)m_pipes.push_back({p,p,headerZ,.28f,-7.45f});
    }
   m_pipes.push_back({{8,22},{22,22},-1.32f,.28f});
   for(Vec2 p:{Vec2{7.5f,4},Vec2{8,13},Vec2{12,19.5f}})wall(p.x-.48f,p.y-.48f,p.x+.48f,p.y+.48f,-12,-9);
   for(Vec2 p:{Vec2{6.2f,3.1f},Vec2{11.7f,8.7f},Vec2{6.2f,15.2f},Vec2{20.4f,10.5f},Vec2{4.4f,19.3f},Vec2{18.8f,20.3f}})
    post(p.x,p.y,-12,-9.25f,.14f);
   for(Vec2 p:{Vec2{19.5f,7.2f},Vec2{20.5f,13.6f},Vec2{16.2f,17.2f},Vec2{10.2f,19.2f}})
    post(p.x,p.y,-9,-4.25f,.11f);
   for(auto& f:m_fixtures)if(f.model==14)f.base=3;

   // Low service dividers define pump bays while preserving the perimeter loop.
   wall(5.4f,11.8f,7.3f,12.05f,-12,-10.95f);
   wall(10.4f,15.5f,12.6f,15.75f,-12,-10.95f);
   m_lights.push_back({{6.5f,9.5f},-9.55f});
   m_lights.push_back({{11.5f,14},-9.55f});
   // Lower manifold heavy machinery: centrifugal pumps and compressor
   m_props.push_back({0,{6.5f,9.f},2.2f,1.4f,0,{.7f,.7f},0.f});
   m_props.push_back({0,{11.5f,14.f},2.2f,1.4f,kPi*.5f,{.7f,.7f},0.f});
   m_props.push_back({1,{6.5f,15.f},1.9f,1.3f,0,{.65f,.65f},0.f});

   // Slim reinforced columns land directly on the observation deck. The old
   // 0.8 m blocks swallowed the lower manifold sightlines.
   m_fixtures.push_back({2,{19.5f,6.5f},0.f,.30f,.30f,8.f,0,true});
   m_fixtures.push_back({2,{15.5f,16.5f},0.f,.30f,.30f,8.f,0,true});

   // Glazed operator booth: an open doorway faces the stair landing, with
   // a waist-high west sill preserving the view over the pump hall.
   wall(19,17,19.6f,17.18f,-4,roof);wall(21.4f,17,22.6f,17.18f,-4,roof);
   wall(19.6f,17,21.4f,17.18f,-1.5f,roof);
   wall(22.4f,17,22.6f,21,-4,roof);
   wall(19,20.8f,19.6f,21,-4,roof);wall(21.4f,20.8f,22.4f,21,-4,roof);
   wall(19.6f,20.8f,21.4f,21,-1.5f,roof);
   wall(19,18.2f,19.18f,20.8f,-4,-2.95f);wall(19,18.2f,19.18f,20.8f,-1.5f,roof);
   for(float y:{18.2f,19.5f,20.65f})post(19.08f,y,-2.95f,-1.5f,.045f);
   // Wall breakers remain equipment; the actual workstation is the terminal.
   m_fixtures.push_back({8,{21.85f,19.5f},8.f,.67f,.20f,.91f,kPi*.5f,false});
   m_fixtures.push_back({8,{1.15f,8.f},0.f,.67f,.20f,.91f,kPi*.5f,false});

   m_terminals={{{21.4f,19.5f},"PUMP ANNEX / OBSERVATION","DUTY PUMP START: LOWER MANIFOLD CONTROL.","JUNCTION ACCESS: SOUTH OBSERVATION WALKWAY.",8,false}};
   m_terminals.push_back({{11.5f,10.5f},"DUTY PUMPS / LOCAL START","RESTORE PRESSURE BEFORE LEAVING THE ANNEX.","E / START DUTY PUMPS. EXIT VIA EAST STAIRS.",0,false,0,stateId("annex_running"),false,-kPi*.5f});
   event("annex_pump_restart",1,1,23,23,-12.1f,-2,{action(A::Shake,0,0,.18f),action(A::PlaySound,0,0,.75f),action(A::Checkpoint)});
   m_scriptEvents.back().requireState=stateId("annex_running");
   m_scriptEvents.back().actions[1].sound=Sound::LiftMotor;
   // Service alcove behind the pump controls gives the lower loop a readable
   // destination rather than another uninterrupted rectangle of concrete.
   wall(9.2f,7.5f,12.9f,7.7f,-12,-9.25f);
   wall(12.7f,7.7f,12.9f,11.8f,-12,-9.25f);
   wall(9.2f,7.7f,9.4f,9.1f,-12,-9.25f);
   shelf({12.2f,8.8f},kPi*.5f);
   // Keep the south doorway open; the side cabinet stays outside its lane.
   cabinet({22.f,20.2f},kPi*.5f);
   m_lights.push_back({{11.3f,10},-9.4f});
   m_creatureSpawns={{CreatureKind::Wasp,{12,12},-6},{CreatureKind::Wasp,{20,13},-4},{CreatureKind::Huntsman,{9.9f,11.3f},-12},{CreatureKind::Huntsman,{8.5f,11.f},-12}};
   m_pickupSpawns={{{6,17},PickupKind::Health,-9},{{20,20},PickupKind::Ammo,-4}};
   for(Vec2 p:{Vec2{3.5f,2},Vec2{4,14},Vec2{12,9},Vec2{20,7},Vec2{20,18},Vec2{20.5f,17.5f}})m_lights.push_back({p,-1.3f});
   // This high-bay lamp belongs on the roof plane; -6.35 put it mid-room
   // with no hanger or ceiling support.
   m_lights.push_back({{10,8},-1.3f});m_lights.push_back({{18,18},-1.3f});
   for(Vec2 p:{Vec2{4,11},Vec2{10,8},Vec2{12,3},Vec2{13,17},Vec2{8.f,11.f}})m_lights.push_back({p,-9.4f});
  }
  // === LEVEL_7_END ===
  // === LEVEL_8_START ===
  if(m_level==8){
   m_layers={{"Utility Junction / concourse",-9,0,UtilityJunctionGround},{"Utility Junction / maintenance bridge",-4,.25f,UtilityJunctionUpper}};
   m_doors={{2.8f,4.2f,.5f,0,false,false,true,5},{19,22,4.5f,0,false,false,false,5},{3,5,17.5f},{20,23,23.5f,0,false,true}};
   m_doors.front().swinging=true;
   m_doors[1].requireState=stateId("freight_incident_clearance");m_doors[2].requireState=stateId("primary_utilities_permit");m_doors.back().requireState=stateId("waste_access");

   // North entrance bridge vestibule: walking opening completely clear, lintel overhead
   wall(2,0,5,1,-9,-4);
   wall(2,0,2.8f,.85f,-4,roof);
   wall(4.2f,0,5,.85f,-4,roof);
   wall(2.8f,0,4.2f,.85f,-1.45f,roof);

   // South exit to Waste Handling: side jambs and overhead lintel (walking doorway clear)
   wall(19.2f,23,20.f,24,-9,roof);
   wall(23.f,23,23.8f,24,-9,roof);
   wall(20.f,23,23.f,24,-6.5f,roof);

   stairs={{8,10,11,20,-9,-4,26,true,false}};
   wall(15,10,15.2f,15,-9,-7.85f);wall(15,10,15.2f,15,-6.05f,-5.6f);
   wall(15,10,22,10.2f,-9,-7.85f);wall(15,10,22,10.2f,-6.05f,-5.6f);
   wall(22,10,22.2f,15,-9,-5.6f);wall(15,14.8f,17,15,-9,-5.6f);wall(19,14.8f,22,15,-9,-5.6f);
   for(float x:{15.f,21.9f})for(float y:{10.f,14.8f})m_structures.push_back({x,y,x+.12f,y+.12f,-7.85f,-6.05f,false,2});

   // Elevated freight vestibule
   wall(19,1,22,4.65f,-9,-4);
   wall(18.82f,1,19.04f,4.35f,-4,roof);
   wall(21.96f,1,22.18f,4.35f,-4,roof);
   wall(18.82f,1,22.18f,1.22f,-4,roof);
   wall(19.04f,4.35f,21.96f,4.65f,-1.5f,roof);

   // The former sealed box below the arrival deck is now a walk-in service
   // store. Its south doorway offers a useful optional loop and supplies.
   wall(1,6.78f,3,7.06f,-9,-4);wall(4.6f,6.78f,7.05f,7.06f,-9,-4);
   wall(3,6.78f,4.6f,7.06f,-6.5f,-4);
   wall(6.78f,1,7.06f,6.82f,-9,-4);
   wall(18.78f,1,19.06f,6.82f,-9,-4);
   wall(18.78f,6.78f,22.2f,7.06f,-9,-4);

   // West utility partition sealed up to roof with door lintel
   wall(1,17.35f,3,17.65f,-9,roof);
   wall(5,17.35f,6,17.65f,-9,roof);
   wall(6,17.35f,6.2f,23,-9,roof);
   wall(3,17.35f,5,17.65f,-6.5f,roof);

   for(Vec2 p:{Vec2{2.3f,6.2f},Vec2{6.2f,6.2f}})
    post(p.x,p.y,-9,-4.25f,.11f);

   // The bridge needs visible structure without putting bulky posts in the
   // walking lane. Narrow steel columns sit directly under the bridge girders.
   for(float x:{7.5f,20.f}){
    m_structures.push_back({x-.16f,7.5f-.16f,x+.16f,7.5f+.16f,-9.f,-4.25f,false,2});
    m_structures.push_back({x-.11f,7.f,x+.11f,9.f,-4.50f,-4.25f,false,2});
   }

   // Route the utility trunk along the north and east walls. The previous
   // crossed mid-room rack had four-metre hangers through the bridge sightline.
   m_pipes.push_back({{2.f,2.2f},{21.5f,2.2f},-1.35f,.16f});
   m_pipes.push_back({{2.f,2.62f},{21.5f,2.62f},-1.35f,.10f});
   m_pipes.push_back({{21.5f,2.2f},{21.5f,22.f},-1.35f,.12f});
   m_pipes.push_back({{21.5f,12.5f},{21.5f,12.5f},-1.35f,.14f,-8.5f});

   // Grounded pump drives turn the otherwise empty concourse into a readable
   // service bay and give the lower route useful, waist-high combat cover.
   m_fixtures.push_back({12,{13.6f,16.42f},0.f,1.8f,1.05f,1.45f,0.f,true});
   m_fixtures.push_back({12,{18.3f,18.62f},0.f,1.8f,1.05f,1.45f,kPi*.5f,true});
   m_lights.push_back({{18,16.3f},-1.05f});
   m_lights.push_back({{19.5f,22},-1.05f});
   // Concourse substation switchgear & dispatch workstation
   cabinet({5.45f,19.5f},kPi*.5f);
   cabinet({5.45f,21.0f},kPi*.5f);
   cabinet({1.65f,19.5f},-kPi*.5f);cabinet({1.65f,21.f},-kPi*.5f);
   shelf({2,3.8f},-kPi*.5f);shelf({5.8f,3.8f},kPi*.5f);
   m_lights.push_back({{3.8f,4.8f},-4.4f});
   // Central distribution spine separates dispatch and maintenance aisles.
   // The ends stay open so fighting and exploration have two routes.
   m_structures.push_back({12.8f,10.8f,13.12f,14.8f,-9,-7.85f,false,2});
   for(float y:{11.f,14.6f})post(12.96f,y,-7.85f,-6.7f,.065f);
   m_structures.push_back({12.8f,11.2f,13.12f,14.3f,-7.35f,-6.8f,false,2});
   m_structures.push_back({12.86f,10.8f,13.06f,14.8f,-6.8f,-6.7f,false,2});
   for(float y:{11.6f,13.3f}){
    cabinet({12.35f,y},kPi*.5f);cabinet({13.57f,y},-kPi*.5f);
   }
    m_fixtures.push_back({11,{16.8f,12.5f},0.f,.8f,.8f,1.1f,kPi*.5f,true});
    m_fixtures.push_back({8,{19.2f,12.5f},0.f,.67f,.20f,.91f,0,false});
    m_fixtures.push_back({8,{1.78f,3.5f},5.f,.67f,.20f,.91f,kPi*.5f,false});
   m_terminals={{{18,12.5f},"JUNCTION / WASTE DISPATCH","DISPATCH INTERLOCK / SOUTH BULKHEAD.","E / RELEASE WASTE HANDLING ROUTE.",0,false,0,stateId("waste_access")},
               {{17.2f,8},"FREIGHT SERVICES / INCIDENT OVERRIDE","FREIGHT ROUTE / LOCAL OVERRIDE.","E / CLEAR THE INCIDENT INTERLOCK.",5,false,0,stateId("freight_incident_clearance"),false,-kPi*.5f},
               {{4,15},"PRIMARY UTILITIES / PERMIT DESK","WEST SERVICE GATE / LOCAL PERMIT.","E / RELEASE THE UTILITIES GATE.",0,false,0,stateId("primary_utilities_permit")}};
   // The two side branches had state-locked doors but no controls capable of
   // setting those states. These terminal controls are placed on the approach
   // side of each gate, so the player can operate them before entering.
   m_terminals.push_back({{20.5f,2.f},"FREIGHT / OUTGOING MANIFEST","RESEARCH CONTAINERS HELD AT THE NEXT TRANSFER.","SPARE SHELLS IN THIS BOOTH / WASTE DISPATCH BELOW.",5,false,0,0,false,0.f});
   // Shelf stock uses the actual shelf tier height, not a floating prop offset.
   m_clutterSpawns={{3,{2,3.25f},-8.028f},{1,{2,4.2f},-8.028f},
                    {2,{5.8f,3.3f},-8.694f},{3,{5.8f,4.15f},-8.028f}};
   // A back-wall service bench supplies a destination inside the stores.
   m_fixtures.push_back({6,{3.8f,1.7f},0.f,1.87f,.55f,.99f,kPi,true});
   m_clutterSpawns.push_back({3,{3.5f,1.7f},-8.01f});
   m_clutterSpawns.push_back({1,{4.1f,1.7f},-8.01f});
   shelf({13,2.2f},kPi);shelf({16,2.2f},kPi);m_fixtures.push_back({6,{13,20},0,1.87f,.55f,.99f,0,true});
   m_creatureSpawns={{CreatureKind::Huntsman,{11.8f,15.2f},-9},{CreatureKind::Wasp,{13,16},-6}};
   m_pickupSpawns={{{17,13.5f},PickupKind::Health,-9},{{3.8f,3.2f},PickupKind::Ammo,-9},{{3.1f,21.f},PickupKind::Ammo,-9},{{20.5f,2.8f},PickupKind::Ammo,-4}};
   event("junction_arrival",2,1,6,4,-4.1f,-2,{action(A::Checkpoint)});
   for(Vec2 p:{Vec2{4,4},Vec2{12,8},Vec2{20,7},Vec2{18,13},Vec2{11,22},Vec2{21,21}})m_lights.push_back({p,-1.05f});
   m_lights.push_back({{18,12},-1.05f});
   for(Vec2 p:{Vec2{8,8.6f},Vec2{13,8.6f}})m_lights.push_back({p,-4.4f});
   for(Vec2 p:{Vec2{7.5f,14},Vec2{12.5f,19.5f}})m_lights.push_back({p,-1.05f});
  }
  // === LEVEL_8_END ===
  // === LEVEL_9_START ===
  if(m_level==9){
   m_layers={{"Waste Handling / processing floor",-12,0,WasteHandlingLower},{"Waste Handling / sorting deck",-9,.25f,WasteHandlingSortingDeck}};
   m_doors={{2,5,.5f,0,false,false,true,3},{20,23,23.5f,0,false,true}};

   // North entry vestibule: under floor sealed, overhead lintel, doorway fully open
   wall(2,0,5,1,-12,-9);
   wall(2,0,5,0.85f,-6.8f,roof);

   stairs={{8,11,11,17,-12,-9,16,true,true}};
   m_waterVolumes={{14,16,18,21,-12.5f,-12.08f}};
   // The isolated platen clears a standing operator. The old 1.1 m raised
   // stroke trapped even crouching players below an opaque oversized gantry.
   m_compactors={{13,8,17,13,-12,-9.8f,9,stateId("compactor_isolated")}};
   for(float x:{13.16f,16.84f})for(float y:{8.12f,12.88f})
    m_structures.push_back({x-.105f,y-.105f,x+.105f,y+.105f,-12,-9.18f,false,2});
   for(float y:{8.12f,12.62f})m_structures.push_back({13.08f,y,16.92f,y+.26f,-9.46f,-9.26f,false,2});
   m_structures.push_back({13,5,17,14,-12,-11.97f,false,2});

   // Sorting deck retaining bulkheads: conveyor chute drop and landing perimeter fully enclosed
   wall(1,4.78f,12.2f,5.06f,-12,-9);
   wall(12.2f,4.78f,17.8f,5.06f,-12,-9);
   wall(17.8f,4.78f,23,5.06f,-12,-9);
   wall(4.78f,5,5.06f,16.9f,-12,-9);
   wall(1,17,8,17.25f,-12,-9);
   wall(1,19.72f,10.46f,20.0f,-12,-9);
   wall(10.18f,17,10.46f,20.0f,-12,-9);

   for(float x:{7.f,19.f})m_pipes.push_back({{x,2},{x,22},-6.1f,.18f});

   // Hydraulic lines to compactor press
   m_pipes.push_back({{13.f,10.5f},{17.f,10.5f},-5.8f,.14f});
   m_pipes.push_back({{15.f,10.5f},{15.f,10.5f},-5.8f,.14f,-7.5f});
   m_pipes.push_back({{19.f,10.5f},{17.f,10.5f},-5.8f,.14f});

   // Slim, evenly spaced roof supports keep the processing floor readable.
   m_fixtures.push_back({2,{11.5f,5.5f},0.f,.26f,.26f,6.7f,0,true});
   m_fixtures.push_back({2,{18.5f,5.5f},0.f,.26f,.26f,6.7f,0,true});
   m_fixtures.push_back({2,{11.5f,15.5f},0.f,.26f,.26f,6.7f,0,true});

   // Press-side guards make the bypass legible without sealing the working belt.
   wall(17.6f,9,17.85f,12,-12,-10.95f);
   wall(12.15f,9.4f,12.4f,12,-12,-10.95f);
   m_lights.push_back({{10.8f,8.8f},-9.55f});
   m_lights.push_back({{20,16},-9.55f});
   // Heavy industrial shredder machine along east lower wall (leaves wide open aisle)
    m_fixtures.push_back({12,{20.5f,8.5f},0.f,.940f,3.060f,1.751f,kPi*.5f,true});

   // Sorting deck control station and lower wall breaker boxes.
   m_fixtures.push_back({8,{10.8f,7.2f},0.f,.67f,.20f,.91f,0,false});
    m_fixtures.push_back({8,{21.2f,21.5f},0.f,.67f,.20f,.91f,-kPi*.5f,false});
    // West-wall lockers face east into the sorting-deck walkway. The mesh's
    // door face is local -Z, so -pi/2 turns it away from the backing wall.
    for(float y:{8.f,11.f,14.f})
     m_fixtures.push_back({13,{1.62f,y},3.f,.9066f,.4956f,2.2f,-kPi*.5f,true});

   m_terminals={{{10.8f,8},"HYDRAULIC PRESS / LOCAL ISOLATOR","AMBER: CYCLING / GREEN: ISOLATED.","E / TOGGLE CONVEYOR AND PRESS.",0,false,0,stateId("compactor_isolated"),true},
               {{20,21.5f},"SALVAGE DISPATCH / FREIGHT SERVICES","OUTGOING MANIFEST: RESEARCH CONTAINERS.","FREIGHT ACCESS / SOUTH DISPATCH BULKHEAD.",0,false}};
   m_terminals.push_back({{3.2f,7},"SORTING / SHIFT SAFETY","PRESS ISOLATOR: NORTHWEST PRESS APPROACH.","EAST AISLE BYPASSES PRESS / DISPATCH SOUTH.",3});
   // Two receiving bins give the west sorting approach a purpose, while the
   // recovery lane runs around the press to a separate south dispatch room.
   for(float y:{8.5f,12.8f}){
    m_structures.push_back({5.35f,y,7.8f,y+.12f,-12,-10.9f,false,2});
    m_structures.push_back({5.35f,y+2.f,7.8f,y+2.12f,-12,-10.9f,false,2});
    m_structures.push_back({5.35f,y+.12f,5.47f,y+2.f,-12,-10.9f,false,2});
   }
   // Full-sized discarded drives make the receiving bins read as salvage,
   // rather than empty architectural blocks with a few tiny floor objects.
   for(float y:{9.55f,13.9f})m_fixtures.push_back({12,{6.6f,y},0.f,.43f,1.4f,.8f,kPi*.5f,true});
   wall(17.8f,19.8f,19.1f,20,-12,-9.2f);wall(21.3f,19.8f,23,20,-12,-9.2f);
   wall(19.1f,19.8f,21.3f,20,-9.6f,-9.2f);
   wall(17.8f,20,18,23,-12,-9.2f);
   m_structures.push_back({17.8f,19.8f,23,23,-9.2f,-9.f,false,2});
   m_lights.push_back({{20.3f,21.5f},-9.36f});
   shelf({22.35f,22.f},kPi*.5f);
   const Vec2 scrap[]={{5.7f,5.3f},{6.5f,5.6f},{7.2f,5.4f},{5.9f,9.1f},{6.8f,10.2f},{7.3f,10.f},
                       {5.9f,13.4f},{6.4f,14.45f},{7.1f,13.3f},{6.0f,16.7f},{6.9f,17.1f},{7.7f,16.4f},
                       {9.4f,20.5f},{10.2f,20.9f},{11.0f,20.4f},{19.1f,4.2f},{20.0f,4.5f},{20.7f,4.0f}};
   for(int i=0;i<18;++i)m_clutterSpawns.push_back({i%6,scrap[i],-999.f,.37f*i});
   m_clutterSpawns.push_back({3,{15,7},-11.9f});
   shelf({20,3},kPi,3);cabinet({22.4f,16},kPi*.5f);
   for(Vec2 p:{Vec2{2.6f,6.2f},Vec2{4.3f,12.2f},Vec2{4.3f,16.4f},Vec2{8.4f,18.5f}})
    post(p.x,p.y,-12,-9.25f,.11f);
   m_creatureSpawns={{CreatureKind::Huntsman,{19,10},-12},{CreatureKind::Huntsman,{8,21},-12},{CreatureKind::Wasp,{18,15},-9}};
   m_pickupSpawns={{{6,19},PickupKind::Ammo,-9},{{20,18},PickupKind::Health,-12}};
   event("waste_dispatch_checkpoint",19,20,22,23,-12.1f,-10,{action(A::Checkpoint)});
   for(Vec2 p:{Vec2{3,3},Vec2{3,12},Vec2{9,18},Vec2{15,6},Vec2{20,13}})m_lights.push_back({p,-5.55f});
   for(Vec2 p:{Vec2{6.5f,7},Vec2{9,7},Vec2{7,14},Vec2{15,6.5f},Vec2{12,18},Vec2{19.5f,18.5f}})
    m_lights.push_back({p,-9.25f});
  }
  // === LEVEL_9_END ===
  // === DYNAMIC_CAMPAIGN_MAPS_END ===
  for(auto& d:m_doors){if(d.entry)d.sign=m_level-1;else if(d.transfer)d.sign=m_level+1;}
  if(m_level==8){m_doors[1].sign=32;m_doors[2].sign=33;}
  buildLayers(stairs);return;
 }
 if(m_level==4||m_level==5){
  m_layers={{m_level==4?"Reactor Service Gallery":"Coolant Return",-9,0,m_level==4?ReactorServiceGalleryGround:CoolantReturnGround}};
  m_openNorthBoundary=m_level==5;
  m_openSouthBoundary=m_level==4;
  if(m_level==4)m_doors={{5,8,.5f,0,false,false,true}};
  const float roof=m_level==4?2.9f:3.4f;
  auto wall=[&](float x1,float y1,float x2,float y2){m_structures.push_back({x1,y1,x2,y2,0,roof,false,3});};
  auto shelf=[&](float x,float y,float yaw=0.f){m_fixtures.push_back({7,{x,y},0,1.8f,.5f,1.8f,yaw,true});};
  auto machine=[&](float x,float y,float yaw=0.f){m_fixtures.push_back({12,{x,y},0,3.060f,.940f,1.751f,yaw,true});};
  auto switchgear=[&](float x,float y,float yaw=0.f){m_fixtures.push_back({13,{x,y},0,.9066f,.4956f,2.2f,yaw,true});};
  auto tank=[&](float x,float y){m_fixtures.push_back({14,{x,y},0,2.404f,2.645f,2.2f,0,true});};
  if(m_level==4){
   wall(1,4.5f,4.5f,4.75f);wall(5.9f,4.5f,9,4.75f);
   wall(8.75f,4.75f,9,8.5f);wall(8.75f,11,9,13.25f);
   wall(1,13,9,13.25f);
   Door workshop{4.5f,5.9f,4.625f};workshop.swinging=true;m_doors.push_back(workshop);
   m_structures.push_back({4.5f,4.5f,5.9f,4.75f,2.55f,roof,false,3});
   wall(16,4.5f,19,4.75f);wall(21,4.5f,23,4.75f);
   wall(16,4.75f,16.25f,8);wall(16,10.5f,16.25f,13.25f);
   wall(16,13,23,13.25f);
   for(float y:{6.f,7.5f,9.f,10.5f,12.f})switchgear(22.55f,y,kPi*.5f);
   machine(4.6f,7.3f);machine(4.6f,11.5f);
   shelf(1.65f,9.3f,kPi*.5f);shelf(7.9f,6.3f,kPi*.5f);
   wall(1,16,4,16.25f);wall(7,16,9,16.25f);
   wall(8.75f,16.25f,9,20);wall(8.75f,22,9,24);
   shelf(1.65f,18,kPi*.5f);shelf(1.65f,21,kPi*.5f);
   shelf(5.3f,23.2f);shelf(7.7f,18,kPi*.5f);
   wall(16,16,23,16.25f);wall(16,16.25f,16.25f,19);
   for(float y:{18.f,21.f})m_fixtures.push_back({6,{21.3f,y},0,1.87f,.55f,.99f,0,true});
   switchgear(17.5f,17.f);switchgear(18.9f,17.f);
   m_terminals={{{14.4f,3.4f},"SERVICE GALLERY / WORK ORDER","PUMP ASSEMBLY MOVED TO WEST WORKSHOP.","ELECTRICAL ROOM MUST REMAIN DRY.",0,false}};
   m_structures.push_back({14.7f,1,15.1f,23,2.45f,2.57f,false,2});
   m_structures.push_back({15.1f,6.2f,22.7f,6.6f,2.45f,2.57f,false,2});
   for(float y:{2.f,6.f,10.f,14.f,18.f,22.f})
    m_structures.push_back({14.87f,y,14.93f,y+.06f,2.57f,roof,false,2});
   for(Vec2 p:{Vec2{5,9},Vec2{19,9},Vec2{5,20},Vec2{19,20}})m_lights.push_back({p,-6.25f});
  }else{
   m_waterVolumes={{7.25f,7.8f,10.25f,10.7f,-9.40f,-8.85f},
                   {13.75f,7.8f,16.75f,10.7f,-9.40f,-8.85f},
                   {7.25f,12.4f,10.25f,18.f,-9.40f,-8.85f},
                   {13.75f,12.4f,16.75f,18.f,-9.40f,-8.85f}};
   tank(4.f,4.4f);tank(20.f,4.4f);
   for(float y:{9.f,15.5f}){machine(3.8f,y,kPi*.5f);machine(20.2f,y,kPi*.5f);}
   for(float x:{10.45f,13.45f})for(auto ends:{Vec2{8,10.5f},Vec2{12.6f,17.8f}})
    m_structures.push_back({x,ends.x,x+.10f,ends.y,0,.9f,true,2});
   for(const auto& water:m_waterVolumes)for(float x:{water.x1,water.x2})
    m_structures.push_back({x-.04f,water.y1+.18f,x+.04f,water.y2-.18f,0,.075f,false,2});
   for(float x:{3.8f,20.2f})m_pipes.push_back({{x,3.2f},{x,15.5f},-6.3f,.14f});
   m_pipes.push_back({{3.8f,3.2f},{20.2f,3.2f},-6.3f,.14f});
   for(float x:{3.8f,20.2f}){
    m_pipes.push_back({{x,4.4f},{x,4.4f},-6.3f,.14f,-6.85f});
    for(float y:{9.f,15.5f})m_pipes.push_back({{x,y},{x,y},-6.3f,.09f,-7.3f});
   }
   wall(17.25f,20.65f,19.05f,20.87f);wall(20.45f,20.65f,22.85f,20.87f);
   wall(17.25f,22.85f,22.85f,23.08f);wall(17.25f,20.87f,17.47f,22.85f);
   m_structures.push_back({19.05f,20.65f,20.45f,20.87f,2.55f,roof,false,3});
   Door storeDoor{19.05f,20.45f,20.76f};storeDoor.swinging=true;m_doors.push_back(storeDoor);
   m_doors.push_back({8,11,23.5f,0,false,true});m_doors.back().sign=6;
   m_fixtures.push_back({7,{21.4f,22.48f},0,1.6f,.48f,1.8f,0,true});
   m_fixtures.push_back({8,{18.15f,22.74f},.78f,.67f,.20f,.91f,0,false});
   m_clutterSpawns.push_back({3,{21.1f,21.8f}});m_clutterSpawns.push_back({2,{18.5f,21.7f}});
   shelf(2,21,kPi*.5f);switchgear(1.5f,18.5f,-kPi*.5f);
   m_terminals={{{12,4.4f},"RETURN HALL / LOCAL CONTROL","DUPLEX CIRCULATION / DUTY AND STANDBY.","DRAIN CHANNELS BEFORE MAINTENANCE.",0,false}};
   for(Vec2 p:{Vec2{12,6},Vec2{12,12},Vec2{12,18},Vec2{5.7f,12},Vec2{18.3f,12}})m_lights.push_back({p,-5.75f});
  }
  for(auto&s:m_structures){s.bottom-=9.f;s.top-=9.f;}
  buildLayers({});
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
  m_structures.push_back({11,8,13,10,-.3f,0});
  m_structures.push_back({10.95f,8,11,10,0,2.1f,true});
  m_structures.push_back({13,8,13.05f,10,0,2.1f,true});
  m_structures.push_back({2,0,5,1,-9,-.3f});
  m_structures.push_back({10.5f,18.5f,13.5f,21.5f,-9,-3.4f,false,1});
  for(float z:{-9.f,-6.f,0.f}){
   m_structures.push_back({7,2,7.2f,5,z,z+2.65f,false,3});
   m_structures.push_back({7,7,7.2f,9,z,z+2.65f,false,3});
   m_structures.push_back({2,16,5,16.2f,z,z+2.65f,false,3});
   m_structures.push_back({7,16,9,16.2f,z,z+2.65f,false,3});
  }
  for(float z:{-3.3f,2.7f}){
   m_structures.push_back({1,1,23,9,z,z+.12f,false,2});
   m_structures.push_back({1,15,23,23,z,z+.12f,false,2});
   m_structures.push_back({1,9,9,15,z,z+.12f,false,2});
   m_structures.push_back({15,9,23,15,z,z+.12f,false,2});
  }
  for(float z:{-3.f,3.f,6.f,9.f}){
   m_structures.push_back({7.8f,7.8f,8,16.2f,z,z+2.75f,false,3});
   m_structures.push_back({16,7.8f,16.2f,16.2f,z,z+2.75f,false,3});
   m_structures.push_back({8,7.8f,16,8,z,z+2.75f,false,3});
   m_structures.push_back({8,16,16,16.2f,z,z+2.75f,false,3});
   m_structures.push_back({7.8f,7.8f,9,16.2f,z+2.65f,z+2.7f,false,2});
   m_structures.push_back({15,7.8f,16.2f,16.2f,z+2.65f,z+2.7f,false,2});
   m_structures.push_back({9,7.8f,15,9,z+2.65f,z+2.7f,false,2});
   m_structures.push_back({9,15,15,16.2f,z+2.65f,z+2.7f,false,2});
  }
  for(float x:{1.3f,22.3f})for(float y:{16.7f,22.f})
   m_structures.push_back({x,y,x+.4f,y+.4f,-9,-3.3f,false,3});
  for(float y:{16.7f,22.f})m_structures.push_back({1.3f,y,22.7f,y+.4f,-3.65f,-3.3f,false,2});
  for(float x:{17.35f,18.85f})m_structures.push_back({x-.313f,19.04f,x+.313f,19.46f,-9,-7.812f,false,6});
  for(Vec2 cargo:{Vec2{4.45f,7.9f},Vec2{5.75f,9.25f}})m_structures.push_back({cargo.x-.525f,cargo.y-.525f,cargo.x+.525f,cargo.y+.525f,0,.85f,false,6});
  m_structures.push_back({6.175f,13.075f,6.825f,13.725f,0,.95f,false,6});
  m_structures.push_back({15.97f,10.74f,17.9f,11.86f,0,1.71f,false,6});
  m_structures.push_back({15.5f,14.9f,18.5f,15.05f,0,2.55f,false,2});
  m_structures.push_back({4,12.4f,8.85f,12.95f,2.15f,2.55f,false,6});
  m_structures.push_back({5.1f,20.65f,6.9f,21.4f,-5.45f,-5.4f,false,2});
  for(float x:{5.15f,6.75f})for(float y:{20.7f,21.25f})m_structures.push_back({x,y,x+.08f,y+.08f,-6,-5.45f,false,2});
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
  for(float z:{-9.f,-6.f,0.f})for(Vec2 p:{Vec2{4,6},Vec2{16,6},Vec2{21,6},Vec2{4,19},Vec2{8,22},Vec2{16,22},Vec2{21,19}}){
   float ceiling=clearanceAbove(p.x,p.y,z);if(ceiling-z>1.8f)m_lights.push_back({p,ceiling-.15f});
  }
  for(float z:{-3.f,3.f,6.f,9.f})for(Vec2 p:{Vec2{8.5f,11.5f},Vec2{15.5f,12.5f}})m_lights.push_back({p,clearanceAbove(p.x,p.y,z)-.15f});
  m_lights.push_back({{12,11.8f},2.45f});refreshReactorTerminals();
  return;
 }
 m_fixtures=MapFixtures[m_level];
 m_layers={m_level==0?FoundryGroundLayer:m_level==1?PressureWorksGroundLayer:TurbineGantryGroundLayer};
 for(int y=0;y<Height;++y)for(int x=0;x<Width;){
  if(tile(x,y)!='G'){++x;continue;}
  int cells=tile(x+1,y)=='G'?2:1;float width=cells*.94f,scale=width/3.4f;
  m_fixtures.push_back({6,{x+cells*.5f,y+.5f},0,width,scale,1.8f*scale,0,true});x+=cells;
 }
 if(m_level==2){
  m_layers.push_back(TurbineGantryUpperLayer);
  m_internalWallHeight=2.7f;
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
std::size_t World::terrainSampleIndex(int sx,int sy,int sz)const{
 return size_t((sz*TerrainSamplesY+sy)*TerrainSamplesX+sx);
}
void World::buildTerrain(){
 m_terrain.clear();
 const size_t samples=size_t(TerrainSamplesX)*TerrainSamplesY*TerrainSamplesZ;
 m_terrainDensity.assign(samples,0);m_terrainMaterial.assign(samples,0);
 const auto origin=definition().origin;
 for(int sz=0;sz<TerrainSamplesZ;++sz)for(int sy=0;sy<TerrainSamplesY;++sy)for(int sx=0;sx<TerrainSamplesX;++sx){
  int lx=sx-TerrainBorder,ly=sy-TerrainBorder,z=TerrainMinZ-1+sz;
  int wx=int(origin.x)+lx,wy=int(origin.y)+ly;
  bool solid=authoredAshfallVoxel(wx,wy,z);
  m_terrainDensity[terrainSampleIndex(sx,sy,sz)]=solid?32:-32;
  m_terrainMaterial[terrainSampleIndex(sx,sy,sz)]=authoredAshfallMaterial(wx,wy,z);
 }

 constexpr int corner[8][3]={{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};
 constexpr int edges[12][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
 constexpr int cellsX=TerrainSamplesX-1,cellsY=TerrainSamplesY-1,cellsZ=TerrainSamplesZ-1;
 auto cellSlot=[=](int cx,int cy,int cz){return size_t((cz*cellsY+cy)*cellsX+cx);};
 std::vector<int> cellVertex(size_t(cellsX)*cellsY*cellsZ,-1);
 std::vector<TerrainVertex> vertices;
 std::vector<std::uint8_t> materials;
 vertices.reserve(Width*Height*10);materials.reserve(Width*Height*10);
 auto sample=[&](int x,int y,int z){
  int sx=x+TerrainBorder,sy=y+TerrainBorder,sz=z-(TerrainMinZ-1);
  return float(m_terrainDensity[terrainSampleIndex(sx,sy,sz)])/32.f;
 };
 auto sampleMaterial=[&](int x,int y,int z){
  int sx=x+TerrainBorder,sy=y+TerrainBorder,sz=z-(TerrainMinZ-1);
  return m_terrainMaterial[terrainSampleIndex(sx,sy,sz)];
 };
 auto cellIndex=[&](int x,int y,int z)->int{
  if(x<-1||x>Width||y<-1||y>Height||z<TerrainMinZ-1||z>TerrainMaxZ)return -1;
  return cellVertex[cellSlot(x+1,y+1,z-(TerrainMinZ-1))];
 };

 for(int cz=0;cz<cellsZ;++cz)for(int cy=0;cy<cellsY;++cy)for(int cx=0;cx<cellsX;++cx){
  int x=cx-1,y=cy-1,z=TerrainMinZ-1+cz;float d[8];bool positive=false,negative=false;
  for(int i=0;i<8;++i){d[i]=sample(x+corner[i][0],y+corner[i][1],z+corner[i][2]);positive|=d[i]>0;negative|=d[i]<=0;}
  if(!positive||!negative)continue;
  float px=0,py=0,pz=0;int crossings=0;std::uint8_t material=0;float strongest=0;
  for(int i=0;i<8;++i)if(d[i]>strongest){strongest=d[i];material=sampleMaterial(x+corner[i][0],y+corner[i][1],z+corner[i][2]);}
  for(const auto&e:edges){
   int a=e[0],b=e[1];if((d[a]>0)==(d[b]>0))continue;
   float t=d[a]/(d[a]-d[b]);
   px+=x+corner[a][0]+(corner[b][0]-corner[a][0])*t;
   py+=y+corner[a][1]+(corner[b][1]-corner[a][1])*t;
   pz+=z+corner[a][2]+(corner[b][2]-corner[a][2])*t;
   ++crossings;
  }
  if(!crossings)continue;
  float inv=1.f/crossings;TerrainVertex v{px*inv,py*inv,pz*inv,(origin.x+px*inv)*.18f,(origin.y+py*inv)*.18f};
  int index=int(vertices.size());vertices.push_back(v);materials.push_back(material);cellVertex[cellSlot(cx,cy,cz)]=index;
 }
 auto emit=[&](int ia,int ib,int ic,int id,float nx,float ny,float nz,std::uint8_t material){
  if(ia<0||ib<0||ic<0||id<0)return;
  auto a=vertices[size_t(ia)],b=vertices[size_t(ib)],c=vertices[size_t(ic)],d=vertices[size_t(id)];
  float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
  float cx=uy*vz-uz*vy,cy=uz*vx-ux*vz,cz=ux*vy-uy*vx;
  if(cx*nx+cy*ny+cz*nz>=0){m_terrain.push_back({a,b,c,material});m_terrain.push_back({a,c,d,material});}
  else {m_terrain.push_back({a,d,c,material});m_terrain.push_back({a,c,b,material});}
 };
 for(int z=TerrainMinZ;z<=TerrainMaxZ;++z)for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
  float a=sample(x,y,z),b=sample(x+1,y,z);if((a>0)!=(b>0)){
   auto mat=a>0?sampleMaterial(x,y,z):sampleMaterial(x+1,y,z);float n=a>0?1.f:-1.f;
   emit(cellIndex(x,y-1,z-1),cellIndex(x,y,z-1),cellIndex(x,y,z),cellIndex(x,y-1,z),n,0,0,mat);
  }
  a=sample(x,y,z);b=sample(x,y+1,z);if((a>0)!=(b>0)){
   auto mat=a>0?sampleMaterial(x,y,z):sampleMaterial(x,y+1,z);float n=a>0?1.f:-1.f;
   emit(cellIndex(x-1,y,z-1),cellIndex(x-1,y,z),cellIndex(x,y,z),cellIndex(x,y,z-1),0,n,0,mat);
  }
  if(z<TerrainMaxZ){a=sample(x,y,z);b=sample(x,y,z+1);if((a>0)!=(b>0)){
   auto mat=a>0?sampleMaterial(x,y,z):sampleMaterial(x,y,z+1);float n=a>0?1.f:-1.f;
   emit(cellIndex(x-1,y-1,z),cellIndex(x,y-1,z),cellIndex(x,y,z),cellIndex(x-1,y,z),0,0,n,mat);
  }}
 }
}
float World::terrainDensity(float x,float y,float z)const{
 if(m_terrainDensity.empty())return -1;
 float sx=std::clamp(x+TerrainBorder,0.f,float(TerrainSamplesX-1));
 float sy=std::clamp(y+TerrainBorder,0.f,float(TerrainSamplesY-1));
 float sz=std::clamp(z-float(TerrainMinZ-1),0.f,float(TerrainSamplesZ-1));
 int x0=std::min(TerrainSamplesX-2,int(std::floor(sx))),y0=std::min(TerrainSamplesY-2,int(std::floor(sy))),z0=std::min(TerrainSamplesZ-2,int(std::floor(sz)));
 int x1=x0+1,y1=y0+1,z1=z0+1;float tx=sx-x0,ty=sy-y0,tz=sz-z0;
 auto d=[&](int ix,int iy,int iz){return float(m_terrainDensity[terrainSampleIndex(ix,iy,iz)])/32.f;};
 auto mix=[](float a,float b,float t){return a+(b-a)*t;};
 float a=mix(d(x0,y0,z0),d(x1,y0,z0),tx),b=mix(d(x0,y1,z0),d(x1,y1,z0),tx);
 float c=mix(d(x0,y0,z1),d(x1,y0,z1),tx),e=mix(d(x0,y1,z1),d(x1,y1,z1),tx);
 return mix(mix(a,b,ty),mix(c,e,ty),tz);
}
float World::terrainSurfaceBelow(float x,float y,float feet)const{
 if(m_terrainDensity.empty())return 0;
 float high=std::min(feet+.04f,float(TerrainMaxZ+1)),highD=terrainDensity(x,y,high);
 for(float low=high-.125f;low>=TerrainMinZ-1;low-=.125f){
  float lowD=terrainDensity(x,y,low);
  if(highD<=0&&lowD>0){
   float solid=low,air=high;for(int i=0;i<8;++i){float mid=(solid+air)*.5f;if(terrainDensity(x,y,mid)>0)solid=mid;else air=mid;}return (solid+air)*.5f;
  }
  high=low;highD=lowD;
 }
 return float(TerrainMinZ-1);
}
float World::terrainSurfaceAbove(float x,float y,float feet)const{
 if(m_terrainDensity.empty())return 128.f;
 float low=feet+.03f,lowD=terrainDensity(x,y,low);if(lowD>0)return feet;
 for(float high=low+.125f;high<=TerrainMaxZ+1;high+=.125f){
  float highD=terrainDensity(x,y,high);
  if(lowD<=0&&highD>0){
   float air=low,solid=high;for(int i=0;i<8;++i){float mid=(air+solid)*.5f;if(terrainDensity(x,y,mid)>0)solid=mid;else air=mid;}return (air+solid)*.5f;
  }
  low=high;lowD=highD;
 }
 return 128.f;
}
float World::floorHeight(float x,float y)const{
 if(outdoors())return hasTerrain()?terrainSurfaceBelow(x,y,float(TerrainMaxZ+1)):(m_layers.empty()?0.f:m_layers.front().elevation);
 for(const auto& water:m_waterVolumes)if(x>=water.x1&&x<water.x2&&y>=water.y1&&y<water.y2){
  float shore=std::min({x-water.x1,water.x2-x,y-water.y1,water.y2-y});
  float bank=m_layers.empty()?-9.f:m_layers.front().elevation;
  return bank+(water.bed-bank)*std::clamp(shore/.85f,0.f,1.f);
 }
 if(m_mapData)return m_layers.empty()?definition().spawnHeight:m_layers.front().elevation;
 if(m_level>=6)return m_layers.empty()?definition().spawnHeight:m_layers.front().elevation;
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
 if(x>=16&&x<23&&y>=9&&y<16){if(x<19&&y>=11&&y<13)return std::floor((x-16)*2)*.2f;if(y<13)return 1.2f;return std::min(1.2f,std::floor((16-y)*2)*.2f);}
 if(x>=1&&x<7&&y>=17&&y<21){if(x>=5&&y<19)return std::floor((7-x)*2)*.2f;if(y<19)return .8f;return std::min(.8f,std::floor((21-y)*2)*.2f);}
 return 0;
}
float World::waterSurface(float x,float y)const{
 for(const auto& water:m_waterVolumes)if(x>=water.x1&&x<water.x2&&y>=water.y1&&y<water.y2)return water.surface;
 return -1000.f;
}
float World::ceilingHeight(float x,float y)const{
 if(outdoors())return 128.f;
 if(m_mapData)return definition().ceiling;
 if(m_level>=6)return definition().ceiling;
 if(m_level>=4)return m_level==4?-6.1f:-5.6f;
 if(hasLift())return 16.f;
 if(m_level==2)return 6.f;
 if(m_level==0&&y>=24&&x>=20&&x<23)return 3.4f;
 if(m_level==1&&y<0&&x>=2&&x<5)return 3.6f;
 if(m_level==1)return y<7?3.4f:y<17?4.8f:3.8f;
 if(int(x)==10&&int(y)==14)return .66f;
 return y<8?3.1f:y<16?4.2f:3.6f;
}
void World::buildCollisionCandidates()const{
 m_collisionCells={};m_collisionAll={};for(auto&cells:m_collisionCells)cells.resize(Width*Height);
 auto add=[&](int kind,size_t index,float x1,float y1,float x2,float y2){
  m_collisionAll[kind].push_back(index);
  for(int y=std::max(0,int(std::floor(y1-.001f)));y<=std::min(Height-1,int(std::floor(y2+.001f)));++y)
   for(int x=std::max(0,int(std::floor(x1-.001f)));x<=std::min(Width-1,int(std::floor(x2+.001f)));++x)m_collisionCells[kind][y*Width+x].push_back(index);
 };
 for(size_t i=0;i<m_fixtures.size();++i){const auto&f=m_fixtures[i];float c=std::fabs(std::cos(f.yaw)),s=std::fabs(std::sin(f.yaw));float hx=(f.width*c+f.depth*s)*.5f,hy=(f.width*s+f.depth*c)*.5f;add(0,i,f.position.x-hx,f.position.y-hy,f.position.x+hx,f.position.y+hy);}
 for(size_t i=0;i<m_props.size();++i){const auto&p=m_props[i];add(1,i,p.position.x-p.halfSize.x,p.position.y-p.halfSize.y,p.position.x+p.halfSize.x,p.position.y+p.halfSize.y);}
 for(size_t i=0;i<m_terminals.size();++i){const auto&t=m_terminals[i];
  // Lift-control consoles can travel without changing the terminal count.
  // Include them in every cell; the original exact tests still decide hits.
  if(t.control){add(2,i,0,0,Width,Height);continue;}
  bool desk=benchTerminal(*this,t);add(2,i,t.position.x-(desk?.48f:.27f),t.position.y-(desk?.36f:.18f),t.position.x+(desk?.42f:.27f),t.position.y+(desk?.36f:.18f));
 }
}
std::span<const size_t> World::collisionCandidates(int kind,float x,float y)const{
 if(m_collisionCells[0].empty()||m_collisionAll[0].size()!=m_fixtures.size()||m_collisionAll[1].size()!=m_props.size()||m_collisionAll[2].size()!=m_terminals.size())buildCollisionCandidates();
 int ix=int(std::floor(x)),iy=int(std::floor(y));
 if(!m_collisionCandidatesEnabled||ix<0||iy<0||ix>=Width||iy>=Height)return m_collisionAll[kind];
 return m_collisionCells[kind][iy*Width+ix];
}
__declspec(noinline) bool World::testCollisionCandidates(){
 for(int level:{0,1,3,5,6,7,8,9,16,20}){World world(level);std::vector<Vec2> samples;
  for(int y=-1;y<=Height;++y)for(int x=-1;x<=Width;++x)samples.push_back({x+.137f,y+.713f});
  for(const auto&f:world.fixtures())for(float u:{-.51f,-.49f,0.f,.49f,.51f})for(float v:{-.51f,-.49f,0.f,.49f,.51f}){float c=std::cos(f.yaw),s=std::sin(f.yaw);samples.push_back({f.position.x+u*f.width*c+v*f.depth*s,f.position.y-u*f.width*s+v*f.depth*c});}
  for(auto p:samples)for(float offset:{-.04f,.02f,.5f,1.5f,3.f})for(bool cavities:{false,true}){
   float feet=world.floorHeight(p.x,p.y)+offset;world.m_collisionCandidatesEnabled=true;bool optimized=world.fits(p.x,p.y,feet,.015f,false,cavities);float support=world.supportHeight(p.x,p.y,false,cavities),below=world.supportBelow(p.x,p.y,feet);
   world.m_collisionCandidatesEnabled=false;bool reference=world.fits(p.x,p.y,feet,.015f,false,cavities);
   if(optimized!=reference||support!=world.supportHeight(p.x,p.y,false,cavities)||below!=world.supportBelow(p.x,p.y,feet))return false;
  }
 }
 return true;
}
float World::supportHeight(float x,float y,bool dynamic,bool shelfCavities)const{
 for(auto index:collisionCandidates(0,x,y)){const auto&fixture=m_fixtures[index];if(fixture.solid&&fixture.base<.025f&&insideFixture(fixture,x,y)){
  if(shelfCavities&&fixture.model==7)continue;
  return floorHeight(fixture.position.x,fixture.position.y)+fixture.base+fixture.height;
 }}
 for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(p.base<.025f&&std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y)return floorHeight(p.position.x,p.position.y)+p.base+p.height;}
 for(auto index:collisionCandidates(2,x,y)){const auto&terminal=m_terminals[index];if((dynamic||!hasLift()||!terminal.control)&&terminal.z==0&&insideTerminal(*this,terminal,x,y))return floorHeight(terminal.position.x,terminal.position.y)+terminalHeight(*this,terminal,x,y);}
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
 if(campaignChunk(0)&&x>20.17f&&x<22.83f&&y>22.90f&&y<23.04f)height=std::min(height,2.57f);
 return height;
}
void World::buildLightOcclusion()const{
 m_lightSolids.clear();m_lightNodes.clear();
 auto box=[&](float x1,float y1,float x2,float y2,float bottom,float top){if(x2<=x1||y2<=y1||top<=bottom)return;m_lightSolids.push_back({{(x1+x2)*.5f,(y1+y2)*.5f,(bottom+top)*.5f},{(x2-x1)*.5f,(y2-y1)*.5f,(top-bottom)*.5f}});};
 auto oriented=[&](Vec2 p,float width,float depth,float bottom,float top,float yaw){m_lightSolids.push_back({{p.x,p.y,(bottom+top)*.5f},{width*.5f,depth*.5f,(top-bottom)*.5f},std::cos(yaw),std::sin(yaw)});};
 if(m_mapData||level()>=6)box(0,0,Width,Height,-100,floorHeight(12,12)-.025f);
 else for(int y=0;y<Height*2;++y)for(int x=0;x<Width*2;++x)box(x*.5f,y*.5f,(x+1)*.5f,(y+1)*.5f,-100,floorHeight(x*.5f+.25f,y*.5f+.25f)-.025f);
 for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){float floor=floorHeight(x+.5f,y+.5f),ceiling=ceilingHeight(x+.5f,y+.5f);char t=tile(x,y);
  box(float(x),float(y),float(x+1),float(y+1),ceiling,ceiling+1);
  if(t=='#'||t=='C'||t=='B'||t=='T')box(float(x),float(y),float(x+1),float(y+1),floor,t=='#'?wallHeight(x,y):floor+(t=='C'?.60f:t=='B'?1.1f:2.62f));
 }
 for(const auto&s:m_structures){
  // Glass and painted floor markings do not cast an opaque box shadow.
  if(s.material==17||((s.material==16||s.material==14||s.material==13)&&s.top-s.bottom<.04f))continue;
  if(!s.rail){box(s.x1,s.y1,s.x2,s.y2,s.bottom,s.top);continue;}
  // Movement uses the whole guardrail envelope; light passes between its bars.
  if(s.top-s.bottom>1.5f){
   if(campaignChunk(3)){box(s.x1,s.y1,s.x2,s.y2,s.bottom,s.bottom+.16f);box(s.x1,s.y1,s.x2,s.y2,s.bottom+.92f,s.bottom+.96f);
    int bars=std::max(1,int(std::max(s.x2-s.x1,s.y2-s.y1)/.4f));for(int i=1;i<=bars;++i){float t=float(i)/(bars+1),x=s.x1+(s.x2-s.x1)*t,y=s.y1+(s.y2-s.y1)*t;box(x-.012f,y-.012f,x+.012f,y+.012f,s.bottom+.16f,s.top-.07f);}
   }else box(s.x1+.012f,s.y1+.012f,s.x2-.012f,s.y2-.012f,s.bottom+.012f,s.top-.08f);
  }
  box(s.x1,s.y1,s.x2,s.y2,s.top-.07f,s.top);box(s.x1,s.y1,s.x1+.055f,s.y1+.055f,s.bottom,s.top-.07f);box(s.x2-.055f,s.y2-.055f,s.x2,s.y2,s.bottom,s.top-.07f);
 }
 for(const auto&f:m_fixtures)if(f.solid){float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7){for(float tier:ShelfTiers){float top=base+f.height*tier;oriented(f.position,f.width,f.depth,top-.045f,top,f.yaw);}
   float c=std::cos(f.yaw),s=std::sin(f.yaw);for(float x:{-f.width*.5f+.02f,f.width*.5f-.02f})for(float y:{-f.depth*.5f+.02f,f.depth*.5f-.02f})oriented({f.position.x+x*c+y*s,f.position.y-x*s+y*c},.04f,.04f,base,base+f.height,f.yaw);
  }else oriented(f.position,f.width,f.depth,base,base+f.height,f.yaw);
 }
 for(const auto&p:m_props){float base=floorHeight(p.position.x,p.position.y)+p.base;box(p.position.x-p.halfSize.x,p.position.y-p.halfSize.y,p.position.x+p.halfSize.x,p.position.y+p.halfSize.y,base,base+p.height);}
 for(const auto&t:m_terminals){if(hasLift()&&t.control)continue;float base=floorHeight(t.position.x,t.position.y)+t.z;
  if(benchTerminal(*this,t)){box(t.position.x-.48f,t.position.y-.36f,t.position.x+.42f,t.position.y+.36f,base+.74f,base+.815f);oriented(t.position,.386509f,.53235f,base+.815f,base+1.32005f,level()==6?-kPi*.5f:t.yaw);}
  else oriented(t.position,.54f,.36f,base,base+.95f,t.yaw);
 }
 for(const auto&d:m_doors){float base=floorHeight((d.left+d.right)*.5f,d.y)+d.z;box(d.left,d.y-.10f,d.right,d.y+.10f,base+2.5f,ceilingHeight((d.left+d.right)*.5f,d.y));}
 auto bounds=[](const LightSolid&s){float hx=std::fabs(s.cosine)*s.half.x+std::fabs(s.sine)*s.half.y,hy=std::fabs(s.sine)*s.half.x+std::fabs(s.cosine)*s.half.y;return std::pair{s.center-Point3{hx,hy,s.half.z},s.center+Point3{hx,hy,s.half.z}};};
 auto build=[&](auto&&self,int begin,int end)->int{int index=int(m_lightNodes.size());LightNode node;node.begin=begin;node.end=end;node.minimum={10000,10000,10000};node.maximum={-10000,-10000,-10000};
  for(int i=begin;i<end;++i){auto [lo,hi]=bounds(m_lightSolids[i]);node.minimum={std::min(node.minimum.x,lo.x),std::min(node.minimum.y,lo.y),std::min(node.minimum.z,lo.z)};node.maximum={std::max(node.maximum.x,hi.x),std::max(node.maximum.y,hi.y),std::max(node.maximum.z,hi.z)};}
  m_lightNodes.push_back(node);if(end-begin<=4)return index;
  auto extent=node.maximum-node.minimum;int axis=extent.x>=extent.y&&extent.x>=extent.z?0:extent.y>=extent.z?1:2;auto coordinate=[&](const LightSolid&s){return axis==0?s.center.x:axis==1?s.center.y:s.center.z;};int middle=(begin+end)/2;
  std::nth_element(m_lightSolids.begin()+begin,m_lightSolids.begin()+middle,m_lightSolids.begin()+end,[&](const auto&a,const auto&b){return coordinate(a)<coordinate(b);});
  int left=self(self,begin,middle),right=self(self,middle,end);m_lightNodes[index].left=left;m_lightNodes[index].right=right;return index;
 };
 if(!m_lightSolids.empty())build(build,0,int(m_lightSolids.size()));
}
bool World::lightRayClear(Vec2 a,float az,Vec2 b,float bz)const{
 // Terrain and shaped water beds retain the volumetric reference trace.
 if(outdoors()||!m_waterVolumes.empty())return rayClear(a,az,b,bz,false,false,true);
 if(m_lightNodes.empty())buildLightOcclusion();
 Point3 origin{a.x,a.y,az},delta{b.x-a.x,b.y-a.y,bz-az};
 auto intersects=[](Point3 o,Point3 d,Point3 lo,Point3 hi){float nearT=.0001f,farT=.9999f;
  auto slab=[&](float p,float v,float lower,float upper){if(std::fabs(v)<.000001f)return p>=lower-.00001f&&p<=upper+.00001f;float t1=(lower-p)/v,t2=(upper-p)/v;if(t1>t2)std::swap(t1,t2);nearT=std::max(nearT,t1);farT=std::min(farT,t2);return farT>nearT;};
  return slab(o.x,d.x,lo.x,hi.x)&&slab(o.y,d.y,lo.y,hi.y)&&slab(o.z,d.z,lo.z,hi.z);
 };
 if(m_lightNodes.empty())return true;std::array<int,64> pending{};int count=1;pending[0]=0;
 while(count){const auto&node=m_lightNodes[pending[--count]];if(!intersects(origin,delta,node.minimum,node.maximum))continue;
  if(node.left>=0){pending[count++]=node.left;pending[count++]=node.right;continue;}
  for(int i=node.begin;i<node.end;++i){const auto&s=m_lightSolids[i];auto p=origin-s.center;Point3 local{p.x*s.cosine-p.y*s.sine,p.x*s.sine+p.y*s.cosine,p.z},direction{delta.x*s.cosine-delta.y*s.sine,delta.x*s.sine+delta.y*s.cosine,delta.z};
   if(intersects(local,direction,s.half*-1.f,s.half))return false;
  }
 }return true;
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
void World::updateDoors(float dt){for(auto&door:m_doors)door.open=std::clamp(door.open+(door.opening?1.f:-1.f)*dt*.85f,0.f,door.maxOpen);}
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
    for(auto p:m_destroyedTiles)if(p.first==x&&p.second==y)return '.';
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
 m_lightSolids.clear();m_lightNodes.clear();
 for(const auto& layer:m_layers){
  for(auto row:layer.rows)if(row.size()!=Width)throw std::runtime_error("Invalid map layer row width");
  if(layer.thickness<=0)continue;
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
    // The first/last tread can differ from the landing by one legal step.
    // A guardrail across that connection seals otherwise walkable stairs.
    if(std::fabs(top-z)<.215f)return true;
   }
   return false;
  };
  for(int y=0;y<Height;++y)for(int x=0;x<Width;){
   if(!deck(x,y)){++x;continue;}int start=x;while(x<Width&&deck(x,y))++x;
   m_structures.push_back({float(start),float(y),float(x),float(y+1),underside,z});
  }
  for(int y=1;y<Height-1;++y)for(int x=1;x<Width-1;++x)if(deck(x,y)){
   if(!hasLift()&&!custom()&&m_level<6&&(x+y)%5==0&&tile(x,y)!='#')
    m_structures.push_back({x+.06f,y+.06f,x+.14f,y+.14f,floorHeight(x+.1f,y+.1f),underside});
   if(!deck(x-1,y)&&!(hasLift()&&tile(x-1,y)=='#')&&!stairConnection(x-.001f,y+.5f)){
    m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x-1,y)=='#')m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false});
   }
   if(!deck(x+1,y)&&!(hasLift()&&tile(x+1,y)=='#')&&!stairConnection(x+1.001f,y+.5f)){
    m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x+1,y)=='#')m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false});
   }
   if(!deck(x,y-1)&&!(hasLift()&&tile(x,y-1)=='#')&&!stairConnection(x+.5f,y-.001f)){
    m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x,y-1)=='#')m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false});
   }
   if(!deck(x,y+1)&&!(hasLift()&&tile(x,y+1)=='#')&&!stairConnection(x+.5f,y+1.001f)){
    m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x,y+1)=='#')m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false});
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
 if(m_structures.size()>65535)throw std::runtime_error("Map exceeds the collision structure capacity");
 for(size_t index=0;index<m_structures.size();++index){
  const auto&s=m_structures[index];
  for(int y=int(s.y1);y<int(std::ceil(s.y2));++y)for(int x=int(s.x1);x<int(std::ceil(s.x2));++x)
   if(x>=0&&x<Width&&y>=0&&y<Height)m_structureCells[y*Width+x].push_back(static_cast<uint16_t>(index));
 }
}
float World::wallHeight(int x,int y)const{return outdoors()?floorHeight(x+.5f,y+.5f)+m_internalWallHeight:m_internalWallHeight>0&&x>0&&x<Width-1&&y>0&&y<Height-1?m_internalWallHeight:ceilingHeight(x+.5f,y+.5f);}
float World::supportBelow(float x,float y,float feet)const{
 float fixtureTop=-100;
 for(auto index:collisionCandidates(0,x,y)){const auto&f=m_fixtures[index];if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float top=base+f.height*tier;if(top<=feet+.025f)fixtureTop=std::max(fixtureTop,top);}
  else if(base+f.height<=feet+.025f)fixtureTop=std::max(fixtureTop,base+f.height);
 }}
 for(auto&p:m_props)if(p.base<.025f&&std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float top=floorHeight(p.position.x,p.position.y)+p.base+p.height;if(top<=feet+.025f)fixtureTop=std::max(fixtureTop,top);}
 float result=hasTerrain()?terrainSurfaceBelow(x,y,feet+.03f):floorHeight(x,y),base=supportHeight(x,y);if(base<=feet+.025f)result=base;
 result=std::max(result,fixtureTop);
 for(const auto& terminal:m_terminals)if(insideTerminal(*this,terminal,x,y)){
  float top=floorHeight(terminal.position.x,terminal.position.y)+terminal.z+terminalHeight(*this,terminal,x,y);
  if(top<=feet+.025f)result=std::max(result,top);
 }
 if(insideLift(x,y)&&m_liftHeight<=feet+.025f)result=std::max(result,m_liftHeight);
 if(insideCargoLift(x,y)&&m_cargoLiftHeight<=feet+.025f)result=std::max(result,m_cargoLiftHeight);
 for(size_t i=0;i<actorTracks().size();++i){const auto& t=actorTracks()[i];const auto& p=m_actorPoses[i];if(t.platform&&std::fabs(x-p.position.x)<=t.footprint.x*.5f&&std::fabs(y-p.position.y)<=t.footprint.y*.5f&&p.z<=feet+.025f)result=std::max(result,p.z);}
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.top<=feet+.025f)result=std::max(result,s.top);}
 return result;
}
float World::clearanceAbove(float x,float y,float feet)const{
 float ceiling=hasTerrain()?std::min(clearanceHeight(x,y),terrainSurfaceAbove(x,y,feet)):clearanceHeight(x,y);
 for(auto&f:m_fixtures)if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float underside=base+f.height*tier-.045f;if(underside>feet+.025f)ceiling=std::min(ceiling,underside);}
  else if(base>feet+.025f)ceiling=std::min(ceiling,base);
 }
 for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(base>feet+.025f)ceiling=std::min(ceiling,base);}
 if(insideLift(x,y)&&feet<m_liftHeight+2.6f)ceiling=std::min(ceiling,m_liftHeight+2.6f);
 if(insideCargoLift(x,y)&&feet<m_cargoLiftHeight+2.4f)ceiling=std::min(ceiling,m_cargoLiftHeight+2.4f);
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.bottom>=feet+.025f)ceiling=std::min(ceiling,s.bottom);}
 for(auto&t:m_terminals){float base=floorHeight(t.position.x,t.position.y)+t.z;if(base>feet+.025f&&insideTerminal(*this,t,x,y))ceiling=std::min(ceiling,base);}
 return ceiling;
}
bool World::fits(float x,float y,float feet,float height,bool dynamic,bool shelfCavities)const{
 if(dynamic)for(size_t i=0;i<actorTracks().size();++i){const auto& t=actorTracks()[i];const auto& p=m_actorPoses[i];if(t.platform&&std::fabs(x-p.position.x)<t.footprint.x*.5f&&std::fabs(y-p.position.y)<t.footprint.y*.5f&&feet<p.z-.025f&&feet+height>p.z-t.thickness)return false;}
 if(dynamic&&insideCargoLift(x,y)&&feet<m_cargoLiftHeight-.025f&&feet+height>m_cargoLiftHeight-.18f)return false;
 if(dynamic&&insideCargoLift(x,y)&&feet<m_cargoLiftHeight+2.52f&&feet+height>m_cargoLiftHeight+2.4f)return false;
 if(dynamic&&insideLift(x,y)&&feet<m_liftHeight+2.8f&&feet+height>m_liftHeight-.25f){
  if(feet<m_liftHeight-.025f||feet+height>m_liftHeight+2.605f)return false;
  if(x<10.12f||x>13.88f)return false;
  bool northOpen=m_liftPhase==LiftPhase::Ready&&x>11&&x<13;
  bool southOpen=m_liftPhase==LiftPhase::Crashed&&m_liftTimer>=3.65f&&x>11&&x<13;
  if((y<10.12f&&!northOpen)||(y>13.88f&&!southOpen))return false;
 }
 if(hasTerrain()){
  float support=terrainSurfaceBelow(x,y,feet+.03f),ceiling=terrainSurfaceAbove(x,y,feet);
  if(feet<support-.025f||feet+height>ceiling+.005f)return false;
  for(float z=feet+.04f;z<feet+height-.02f;z+=.16f)if(terrainDensity(x,y,z)>0)return false;
  char terrainTile=tile(int(std::floor(x)),int(std::floor(y)));
  if(terrainTile=='#'||terrainTile=='C'||terrainTile=='B'||terrainTile=='T'){
   float base=floorHeight(x,y),top=terrainTile=='#'?wallHeight(int(std::floor(x)),int(std::floor(y))):base+(terrainTile=='C'?.60f:terrainTile=='B'?1.1f:2.62f);
   if(feet<top-.025f&&feet+height>base+.005f)return false;
  }
 }else if(feet<supportHeight(x,y,dynamic,shelfCavities)-.025f||feet+height>clearanceHeight(x,y)+.005f)return false;
 for(auto index:collisionCandidates(0,x,y)){const auto&f=m_fixtures[index];if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(shelfCavities&&f.model==7){
   for(float tier:ShelfTiers){float top=base+f.height*tier;if(feet<top-.005f&&feet+height>top-.045f)return false;}
   float dx=x-f.position.x,dy=y-f.position.y,c=std::cos(f.yaw),s=std::sin(f.yaw);
   float localX=std::fabs(dx*c-dy*s),localY=std::fabs(dx*s+dy*c);
   if(localX>f.width*.5f-.04f&&localY>f.depth*.5f-.04f&&feet<base+f.height&&feet+height>base)return false;
  }else if(feet<base+f.height-.025f&&feet+height>base+.005f)return false;
 }}
 for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(feet<base+p.height-.025f&&feet+height>base+.005f)return false;}}
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&feet<s.top-.025f&&feet+height>s.bottom+.005f)return false;}
 for(auto index:collisionCandidates(2,x,y)){const auto&t=m_terminals[index];if(!dynamic&&hasLift()&&t.control)continue;float base=floorHeight(t.position.x,t.position.y)+t.z;if(insideTerminal(*this,t,x,y)&&feet<base+terminalHeight(*this,t,x,y)-.025f&&feet+height>base)return false;}
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
bool World::destroyTile(int x,int y){if(x<0||y<0||x>=Width||y>=Height||m_layers.empty()||tile(x,y)=='.')return false;if(tile(x,y)!='C'&&tile(x,y)!='B')return false;m_destroyedTiles.push_back({x,y});return true;}
}
