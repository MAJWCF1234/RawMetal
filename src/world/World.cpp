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
    "########################"
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
constexpr MapLayer PressureWorksGroundLayer{"Pressure Works / ground",0,0,PressureWorksGround};
constexpr MapLayer TurbineGantryGroundLayer{"Turbine Gantry / ground",0,0,TurbineGantryGround};
constexpr MapLayer TurbineGantryUpperLayer{"Turbine Gantry / upper catwalk",3,.3f,TurbineGantryUpperCatwalk};
constexpr std::array GantryStairs{Staircase{4,18,6,22,0,3,15,true,true}};
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
}

World::World(int level) {
 m_level=std::clamp(level,0,2);
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
  buildLayers(GantryStairs);
  m_doors={{2,5,.5f,0,false,false,true},{20,23,21.5f,0,false,true}};
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
float World::ceilingHeight(float x,float y)const{
 if(m_level==2)return 6.f;
 if(m_level==0&&y>=24&&x>=20&&x<23)return 3.4f;
 if(m_level==1&&y<0&&x>=2&&x<5)return 3.6f;
 if(m_level==1)return y<7?3.4f:y<17?4.8f:3.8f;
 if(int(x)==10&&int(y)==14)return .66f; // Crouch-only service bypass.
 return y<8?3.1f:y<16?4.2f:3.6f;
}
float World::supportHeight(float x,float y)const{
 for(const auto&fixture:m_fixtures)if(fixture.solid&&insideFixture(fixture,x,y))return floorHeight(fixture.position.x,fixture.position.y)+fixture.base+fixture.height;
 for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y)return floorHeight(p.position.x,p.position.y)+p.height;
 for(auto&terminal:m_terminals)if(terminal.z==0&&std::fabs(x-terminal.position.x)<.27f&&std::fabs(y-terminal.position.y)<(terminal.control?.18f:.27f))return floorHeight(x,y)+.95f;
 float floor=floorHeight(x,y);switch(tile(int(std::floor(x)),int(std::floor(y)))){
 case '#':return wallHeight(int(std::floor(x)),int(std::floor(y)));case 'C':return floor+.60f;case 'B':return floor+1.1f;
 case 'T':return floor+2.62f;default:return floor;
 }
}
bool World::doorBlocks(float x,float y,float feet,float height)const{
 for(auto&door:m_doors)if(x>door.left&&x<door.right&&std::fabs(y-door.y)<.13f&&feet+height>door.open*2.65f+.015f)return true;
 return false;
}
float World::clearanceHeight(float x,float y)const{
 float height=ceilingHeight(x,y);for(auto&door:m_doors)if(x>door.left&&x<door.right&&std::fabs(y-door.y)<.62f)height=std::min(height,2.5f);
 // The dispatch board hangs from the vestibule ceiling, above the walking route.
 if(m_level==0&&x>20.17f&&x<22.83f&&y>22.90f&&y<23.04f)height=std::min(height,2.57f);
 return height;
}
bool World::rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors)const{
 auto delta=b-a;int steps=std::max(1,int(std::ceil(length(delta)/.12f)));
 for(int i=1;i<steps;++i){float t=float(i)/steps;auto p=a+delta*t;float z=az+(bz-az)*t;
  if(!fits(p.x,p.y,z,.015f)|| (doors&&doorBlocks(p.x,p.y,z,.015f)))return false;
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
int World::nearbyDoor(Vec2 position,Vec2 forward)const{
 int best=-1;float distance=2.f;
 for(size_t i=0;i<m_doors.size();++i){auto&d=m_doors[i];Vec2 closest{std::clamp(position.x,d.left+.2f,d.right-.2f),d.y};auto delta=closest-position;float lengthTo=length(delta);
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
  auto deck=[&](int x,int y){return x>=0&&y>=0&&x<Width&&y<Height&&layer.rows[y][x]=='=';};
  auto stairConnection=[&](float x,float y){
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
   if((x+y)%5==0&&tile(x,y)!='#')
    m_structures.push_back({x+.06f,y+.06f,x+.14f,y+.14f,floorHeight(x+.1f,y+.1f),underside});
   if(!deck(x-1,y)&&!stairConnection(x-.001f,y+.5f)){
    m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,z+.55f,true});
    // Seal the upper catwalk against a lower-layer wall.  Without this
    // backing panel the rail leaves a one-cell sightline into the void.
    if(tile(x-1,y)=='#')m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,6.f,false});
   }
   if(!deck(x+1,y)&&!stairConnection(x+1.001f,y+.5f)){
    m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,z+.55f,true});
    if(tile(x+1,y)=='#')m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,6.f,false});
   }
   if(!deck(x,y-1)&&!stairConnection(x+.5f,y-.001f)){
    m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,z+.55f,true});
    if(tile(x,y-1)=='#')m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,6.f,false});
   }
   if(!deck(x,y+1)&&!stairConnection(x+.5f,y+1.001f)){
    m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,z+.55f,true});
    if(tile(x,y+1)=='#')m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,6.f,false});
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
   stair.alongY?stair.y1+(stair.y2-stair.y1)*hi:stair.y2,stair.bottom,top});
 }
 m_structureCells.resize(Width*Height);
 for(uint16_t index=0;index<m_structures.size();++index){
  const auto&s=m_structures[index];
  for(int y=int(s.y1);y<int(std::ceil(s.y2));++y)for(int x=int(s.x1);x<int(std::ceil(s.x2));++x)
   if(x>=0&&x<Width&&y>=0&&y<Height)m_structureCells[y*Width+x].push_back(index);
 }
}
float World::wallHeight(int x,int y)const{return m_internalWallHeight>0&&x>0&&x<Width-1&&y>0&&y<Height-1?m_internalWallHeight:ceilingHeight(x+.5f,y+.5f);}
float World::supportBelow(float x,float y,float feet)const{
 float result=floorHeight(x,y),base=supportHeight(x,y);if(base<=feet+.025f)result=base;
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.top<=feet+.025f)result=std::max(result,s.top);}
 return result;
}
float World::clearanceAbove(float x,float y,float feet)const{
 float ceiling=clearanceHeight(x,y);
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.bottom>=feet+.025f)ceiling=std::min(ceiling,s.bottom);}
 for(auto&t:m_terminals)if(t.z>feet+.025f&&std::fabs(x-t.position.x)<.27f&&std::fabs(y-t.position.y)<.18f)ceiling=std::min(ceiling,t.z);
 return ceiling;
}
bool World::fits(float x,float y,float feet,float height)const{
 if(feet<supportHeight(x,y)-.025f||feet+height>clearanceHeight(x,y)+.005f)return false;
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&feet<s.top-.025f&&feet+height>s.bottom+.005f)return false;}
 for(auto&t:m_terminals)if(t.z>0&&std::fabs(x-t.position.x)<.27f&&std::fabs(y-t.position.y)<.18f&&feet<t.z+.95f&&feet+height>t.z)return false;
 return true;
}
std::vector<Span> World::spansAt(int x,int y)const{
 float px=x+.5f,py=y+.5f,roof=ceilingHeight(px,py);std::vector<std::pair<float,float>> solids={{-100.f,supportHeight(px,py)}};
 for(auto&s:m_structures)if(px>=s.x1&&px<s.x2&&py>=s.y1&&py<s.y2)solids.push_back({s.bottom,s.top});
 std::sort(solids.begin(),solids.end());std::vector<Span> result;float bottom=0;
 for(auto solid:solids){if(solid.first>bottom)result.push_back({bottom,solid.first,0});bottom=std::max(bottom,solid.second);}
 if(bottom<roof)result.push_back({bottom,roof,0});return result;
}
}

