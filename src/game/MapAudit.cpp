#include "Game.h"
#include <fstream>
#include <queue>
#include <cmath>
#include <cstdio>
#include <map>

namespace retro {
namespace {
constexpr int N=96;constexpr float Cell=24.f/N,Step=.45f;
// Walkable space a player can reach from the map's start: a flood fill on a
// 0.25 m grid with the game's own support query and player hull test (0.45 m
// step, standing or crouched, drops of up to 6 m). Stacked floors are separate
// states per cell.
struct Reach {
 std::vector<std::vector<float>> visited=std::vector<std::vector<float>>(N*N);
 int nodes=0;float startZ=0;
};
std::string at(Vec2 p){char text[48];std::snprintf(text,sizeof text,"%.2f,%.2f",p.x,p.y);return text;}
}

// Map audit: what a player can actually reach and what visibly floats.
// Checks every spawn, pickup, terminal, door and the exit against the walkable
// flood fill; then that lamps, signs, terminals and props are mounted on
// something, that solid props are not sunk into walls or each other, and that
// nothing spawns inside geometry. Writes map-audit-<level>.txt with an ASCII
// view, and map-views-<level>.txt: reachable viewpoints for --map-sweep.
bool Game::auditMap(int level){
 Game game(WorldId::Campaign);game.loadLevel(level,false);
 // Doors stand open: the audit asks what is reachable, not what is unlocked.
 for(int i=0;i<int(game.m_world.doors().size());++i)game.m_world.openDoor(i);
 game.m_world.updateDoors(2,true);
 const World& w=game.world();
 auto flood=[&](float body){
  Reach r;
  auto seen=[&](int x,int y,float z){for(float v:r.visited[size_t(y*N+x)])if(std::fabs(v-z)<.3f)return true;return false;};
  struct Node{int x,y;float z;};std::queue<Node> open;
  auto seed=[&](Vec2 p,float z){int x=int(p.x/Cell),y=int(p.y/Cell);if(seen(x,y,z))return;r.visited[size_t(y*N+x)].push_back(z);open.push({x,y,z});};
  const auto start=w.definition().playerStart;
  r.startZ=w.supportBelow(start.x,start.y,w.definition().spawnHeight+1.f);
  seed(start,r.startZ);
  // A lift map continues where the ride ends: the cab crashed in its pit.
  for(int pass=0;pass<2;++pass){
  if(pass==1){if(!w.hasLift())break;
   if(w.liftPhase()==World::LiftPhase::Ready){game.m_world.startLift();game.m_world.updateLift(World::LiftRideComplete);}
   seed({12,12.5f},w.liftHeight());}
  while(!open.empty()){
   auto n=open.front();open.pop();++r.nodes;
   const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
   for(int k=0;k<4;++k){
    int nx=n.x+dx[k],ny=n.y+dy[k];if(nx<0||ny<0||nx>=N||ny>=N)continue;
    float cx=(nx+.5f)*Cell,cy=(ny+.5f)*Cell;
    // Like the player, stand on the highest support under the 40 cm hull.
    float feet=-1e9f;for(float ox:{-.2f,0.f,.2f})for(float oy:{-.2f,0.f,.2f})feet=std::max(feet,w.supportBelow(cx+ox,cy+oy,n.z+Step));
    if(!std::isfinite(feet)||feet<n.z-6.f||feet>n.z+Step)continue;
    if(!game.hullFits({cx,cy},feet+.02f,body))continue;
    // The step between the two cells must clear the hull at the higher feet.
    float mx=(n.x+.5f)*Cell*.5f+cx*.5f,my=(n.y+.5f)*Cell*.5f+cy*.5f;
    if(!game.hullFits({mx,my},std::max(feet,n.z)+.02f,body))continue;
    if(seen(nx,ny,feet))continue;
    r.visited[size_t(ny*N+nx)].push_back(feet);open.push({nx,ny,feet});
   }
  }
  }
  return r;
 };
 std::ofstream report("map-audit-"+std::to_string(level)+".txt");
 // The player may crouch anywhere; creatures need standing room.
 const auto reach=flood(Player::CrouchedHeight+.02f);const auto& visited=reach.visited;
 const auto standing=flood(Player::StandingHeight+.02f);
 auto reachableIn=[&](const Reach& area,Vec2 p,float z,float radius){
  int r=int(std::ceil(radius/Cell));int cx=int(p.x/Cell),cy=int(p.y/Cell);
  for(int y=cy-r;y<=cy+r;++y)for(int x=cx-r;x<=cx+r;++x){if(x<0||y<0||x>=N||y>=N)continue;
   for(float v:area.visited[size_t(y*N+x)])if(z<-900||std::fabs(v-z)<1.2f)return true;}
  return false;
 };
 auto reachable=[&](Vec2 p,float z,float radius){return reachableIn(reach,p,z,radius);};
 int problems=0;
 auto fail=[&](const std::string& what){++problems;report<<what<<'\n';};
 {std::map<int,std::pair<int,int>> heights;for(size_t i=0;i<visited.size();++i)for(float v:visited[i]){auto& h=heights[int(std::floor(v*2))];if(!h.first++)h.second=int(i);}
  report<<"Reached floor heights (m: cells, first at):";for(auto [h,entry]:heights)report<<' '<<h*.5f<<':'<<entry.first<<'@'<<(entry.second%N+.5f)*Cell<<','<<(entry.second/N+.5f)*Cell;report<<'\n';}
 auto check=[&](bool ok,const std::string& what){if(!ok)fail("UNREACHABLE "+what);};
 report<<"Map "<<level<<" ("<<w.definition().playerStart.x<<","<<w.definition().playerStart.y<<" z "<<reach.startZ<<"): "<<reach.nodes<<" reachable cells ("<<standing.nodes<<" standing) of "<<N*N<<"\n";
 const char* kinds[]={"huntsman","wasp","brute","warden","mutant"};
 auto spawnFeet=[&](Vec2 p,float z){return z>-900?z:w.supportBelow(p.x,p.y,w.floorHeight(p.x,p.y)+.5f);};
 for(const auto& s:w.creatureSpawns()){float z=s.z>-900?s.z:-999;std::string name=std::string("creature ")+kinds[int(s.kind)]+" at "+at(s.position);
  check(reachableIn(standing,s.position,z,.75f),name);
  if(!w.fits(s.position.x,s.position.y,spawnFeet(s.position,s.z)+.03f,1.2f,false))fail("STUCK "+name);}
 for(const auto& s:w.pickupSpawns()){float z=s.z>-900?s.z:-999;std::string name=std::string(s.kind==PickupKind::Ammo?"ammo":"health")+" at "+at(s.position);
  check(reachable(s.position,z,.6f),name);
  if(!w.fits(s.position.x,s.position.y,spawnFeet(s.position,s.z)+.03f,.3f,false))fail("STUCK "+name);}
 for(const auto& c:w.clutterSpawns())if(!w.fits(c.position.x,c.position.y,spawnFeet(c.position,c.z)+.03f,.2f,false))fail("STUCK clutter at "+at(c.position));
 // A terminal is usable only from a reachable spot within 1.5 m that sees it.
 auto usable=[&](const Terminal& t){
  float base=w.floorHeight(t.position.x,t.position.y)+t.z;
  int r=int(std::ceil(1.5f/Cell));int cx=int(t.position.x/Cell),cy=int(t.position.y/Cell);
  for(int y=cy-r;y<=cy+r;++y)for(int x=cx-r;x<=cx+r;++x){if(x<0||y<0||x>=N||y>=N)continue;
   Vec2 spot{(x+.5f)*Cell,(y+.5f)*Cell};if(length(spot-t.position)>1.5f)continue;
   // Sight line to just above the console (a bench monitor stands 1.32 m).
   for(float v:visited[size_t(y*N+x)])if(std::fabs(v-base)<1.2f&&w.rayClear(spot,v+1.5f,t.position,base+1.4f,false,false))return true;}
  return false;
 };
 for(const auto& t:w.terminals()){
  if(w.hasLift()&&t.control)continue; // The cab console rides the lift.
  std::string name=std::string("terminal '")+t.title+"' at "+at(t.position);
  check(usable(t),name);
  float base=w.floorHeight(t.position.x,t.position.y)+t.z;
  if(std::fabs(w.supportBelow(t.position.x,t.position.y,base+.05f)-base)>.06f)fail("FLOATING "+name);
 }
 check(reachable(w.exitPoint(),-999,1.f),"exit");
 // Every door must be walkable straight through: a prop in front of a doorway
 // can leave the rooms connected some other way and still block the door.
 for(const auto& d:w.doors()){
  float cx=(d.left+d.right)*.5f;bool clear=true;
  for(float dy=-1.f;dy<=1.f;dy+=.25f)if(d.y+dy>0&&d.y+dy<24)clear&=reachable({cx,d.y+dy},-999,.15f);
  if(!clear)fail("BLOCKED door x "+std::to_string(d.left)+"-"+std::to_string(d.right)+" at y "+std::to_string(d.y));
 }
 // Lamps: wall lamps sit on a wall face, ceiling lamps under a roof or slab
 // (the renderer hangs them on rods from it) and never inside geometry.
 for(size_t i=0;i<w.lights().size();++i){const auto& l=w.lights()[i];
  if(w.hasLift()&&i+1==w.lights().size())continue; // The cab lamp rides the lift.
  std::string name="lamp at "+at(l.position)+" z "+std::to_string(l.z);
  if(l.mount==WorldLightMount::Wall){
   Vec2 n{std::cos(l.yaw),std::sin(l.yaw)};
   // Solid 5 cm behind the backing plate, open air 15 cm in front of it.
   Vec2 back=l.position-n*.05f,front=l.position+n*.15f;
   if(w.fits(back.x,back.y,l.z,.015f,false))fail("FLOATING wall "+name+" (no wall behind it)");
   if(!w.fits(front.x,front.y,l.z,.015f,false))fail("BURIED wall "+name+" (blocked in front)");
  }else{
   float roof=w.clearanceAbove(l.position.x,l.position.y,l.z);
   if(!(roof<l.z+40.f))fail("FLOATING ceiling "+name+" (nothing above)");
   if(!w.fits(l.position.x,l.position.y,l.z+.01f,.1f,false)&&roof>l.z+.2f)fail("BURIED ceiling "+name);
  }
 }
 // Signs face out of a backing surface.
 for(const auto& s:w.signs()){
  Vec2 n{std::sin(s.yaw),std::cos(s.yaw)};float z=s.z+s.height*.5f;
  // The plate sits 6 cm proud of an 11 cm backing; a wall (even a 15 cm
  // partition) must be solid 14 cm behind the face, across the whole plate.
  bool backed=true;Vec2 along{n.y,-n.x};
  for(float u:{-.45f,0.f,.45f}){Vec2 back=s.position-n*.14f+along*(u*s.width);backed&=!w.fits(back.x,back.y,z,.015f,false);}
  if(!backed)fail("FLOATING sign '"+s.title+"' at "+at(s.position));
 }
 // Props: a solid prop stands on a floor, slab or another prop, is not sunk
 // into walls or into structure at eye level (a column may meet a roof truss),
 // and does not interpenetrate another solid prop.
 auto corners=[](const Fixture& f,float inset){std::array<Vec2,9> p;int k=0;
  float c=std::cos(f.yaw),s=std::sin(f.yaw);
  for(float u:{-.5f,0.f,.5f})for(float v:{-.5f,0.f,.5f}){float x=u*(f.width-inset*2),y=v*(f.depth-inset*2);p[size_t(k++)]={f.position.x+x*c+y*s,f.position.y-x*s+y*c};}
  return p;};
 const auto& fixtures=w.fixtures();
 for(size_t i=0;i<fixtures.size();++i){const auto& f=fixtures[i];if(!f.solid)continue;
  float base=w.floorHeight(f.position.x,f.position.y)+f.base,top=base+f.height;
  std::string name="prop model "+std::to_string(f.model)+" at "+at(f.position);
  float support=w.supportBelow(f.position.x,f.position.y,base+.05f);
  if(std::fabs(support-base)>.06f)fail("FLOATING "+name+" base "+std::to_string(base)+" support "+std::to_string(support));
  for(auto p:corners(f,.06f)){
   if(w.tile(int(std::floor(p.x)),int(std::floor(p.y)))=='#'&&base<w.floorHeight(p.x,p.y)+3.f){fail("BURIED "+name+" in wall at "+at(p));break;}
   bool hit=false;
   for(const auto& s:w.structures())if(!s.rail&&p.x>s.x1&&p.x<s.x2&&p.y>s.y1&&p.y<s.y2&&s.top>base+.08f&&s.bottom<std::min(top-.08f,base+2.2f)){hit=true;break;}
   if(hit){fail("BURIED "+name+" in structure at "+at(p));break;}
  }
  for(size_t j=i+1;j<fixtures.size();++j){const auto& g=fixtures[j];if(!g.solid)continue;
   float gb=w.floorHeight(g.position.x,g.position.y)+g.base;if(gb>=top-.02f||base>=gb+g.height-.02f)continue;
   int inside=0;for(auto p:corners(g,.08f)){float c=std::cos(f.yaw),s=std::sin(f.yaw),dx=p.x-f.position.x,dy=p.y-f.position.y;
    if(std::fabs(dx*c-dy*s)<f.width*.5f-.02f&&std::fabs(dx*s+dy*c)<f.depth*.5f-.02f)++inside;}
   if(inside)fail("OVERLAP "+name+" with model "+std::to_string(g.model)+" at "+at(g.position));
  }
 }
 // ASCII: '#' wall, '.' reachable, 'c' only crouched, 'o' floor nobody can
 // reach, '^' reachable above floor level.
 report<<"\n";
 for(int y=0;y<24;++y){std::string row;
  for(int x=0;x<24;++x){
   if(w.tile(x,y)=='#'){row+='#';continue;}
   bool any=false,raised=false,upright=false;
   for(int sy2=0;sy2<4;++sy2)for(int sx2=0;sx2<4;++sx2){size_t cell=size_t((y*4+sy2)*N+x*4+sx2);
    for(float v:visited[cell]){any=true;raised|=v>w.floorHeight(x+.5f,y+.5f)+.5f;}
    upright|=!standing.visited[cell].empty();}
   row+=raised?'^':upright?'.':any?'c':'o';
  }
  report<<row<<'\n';
 }
 // Viewpoints for the visual sweep: one per reachable 3 m block and floor.
 std::ofstream views("map-views-"+std::to_string(level)+".txt");
 for(int gy=0;gy<8;++gy)for(int gx=0;gx<8;++gx){std::vector<float> floors;
  for(int y=gy*12;y<gy*12+12;++y)for(int x=gx*12;x<gx*12+12;++x)for(float v:visited[size_t(y*N+x)]){
   bool known=false;for(float f:floors)known|=std::fabs(f-v)<1.5f;if(known)continue;floors.push_back(v);
   // The reachable cell nearest the block centre at this floor.
   float best=1e9f;Vec2 spot{};
   for(int yy=gy*12;yy<gy*12+12;++yy)for(int xx=gx*12;xx<gx*12+12;++xx)for(float u:visited[size_t(yy*N+xx)])if(std::fabs(u-v)<.3f){
    Vec2 c{(xx+.5f)*Cell,(yy+.5f)*Cell};float d=length(c-Vec2{gx*3+1.5f,gy*3+1.5f});if(d<best){best=d;spot=c;}}
   views<<spot.x<<' '<<spot.y<<' '<<v<<'\n';
  }
 }
 report<<(problems?"FAIL: ":"PASS: ")<<problems<<" placement problems\n";
 return problems==0;
}
}
