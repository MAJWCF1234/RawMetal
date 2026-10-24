#include "Game.h"
#include <fstream>
#include <queue>
#include <cmath>

namespace retro {
// Map audit: what a player can actually reach. Flood-fills walkable space from
// the player start on a 0.25 m grid with the game's own support and clearance
// queries (0.45 m step, 1.7 m body, drops allowed), then checks every spawn,
// pickup, terminal and the exit against it, and flags solid props whose
// footprint sits in a wall. Writes map-audit-<level>.txt with an ASCII view.
bool Game::auditMap(int level){
 Game game(WorldId::Campaign);game.loadLevel(level,false);
 const World& w=game.world();
 std::ofstream report("map-audit-"+std::to_string(level)+".txt");
 constexpr int N=96;constexpr float Cell=24.f/N,Step=.45f,Body=1.7f;
 // Visited feet heights per cell (stacked floors are separate states).
 std::vector<std::vector<float>> visited(N*N);
 auto seen=[&](int x,int y,float z){for(float v:visited[size_t(y*N+x)])if(std::fabs(v-z)<.3f)return true;return false;};
 struct Node{int x,y;float z;};std::queue<Node> open;
 const auto start=w.definition().playerStart;
 float startZ=w.supportBelow(start.x,start.y,w.definition().spawnHeight+1.f);
 int sx=int(start.x/Cell),sy=int(start.y/Cell);
 visited[size_t(sy*N+sx)].push_back(startZ);open.push({sx,sy,startZ});
 int nodes=0;
 while(!open.empty()){
  auto n=open.front();open.pop();++nodes;
  const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
  for(int k=0;k<4;++k){
   int nx=n.x+dx[k],ny=n.y+dy[k];if(nx<0||ny<0||nx>=N||ny>=N)continue;
   float cx=(nx+.5f)*Cell,cy=(ny+.5f)*Cell;
   float feet=w.supportBelow(cx,cy,n.z+Step);
   if(!std::isfinite(feet)||feet<n.z-6.f||feet>n.z+Step)continue;
   if(!w.fits(cx,cy,feet+.02f,Body,false))continue;
   // The edge between the two cells must be passable at the higher feet.
   float mx=(n.x+.5f)*Cell*.5f+cx*.5f,my=(n.y+.5f)*Cell*.5f+cy*.5f;
   if(!w.fits(mx,my,std::max(feet,n.z)+.02f,Body,false))continue;
   if(seen(nx,ny,feet))continue;
   visited[size_t(ny*N+nx)].push_back(feet);open.push({nx,ny,feet});
  }
 }
 auto reachable=[&](Vec2 p,float z,float radius){
  int r=int(std::ceil(radius/Cell));int cx=int(p.x/Cell),cy=int(p.y/Cell);
  for(int y=cy-r;y<=cy+r;++y)for(int x=cx-r;x<=cx+r;++x){if(x<0||y<0||x>=N||y>=N)continue;
   for(float v:visited[size_t(y*N+x)])if(z<-900||std::fabs(v-z)<1.2f)return true;}
  return false;
 };
 int problems=0;
 auto check=[&](bool ok,const std::string& what){if(!ok){++problems;report<<"UNREACHABLE "<<what<<'\n';}};
 report<<"Map "<<level<<" ("<<w.definition().playerStart.x<<","<<w.definition().playerStart.y<<" z "<<startZ<<"): "<<nodes<<" reachable cells of "<<N*N<<"\n";
 const char* kinds[]={"huntsman","wasp","brute","warden","mutant"};
 for(const auto& s:w.creatureSpawns()){float z=s.z>-900?s.z:-999;check(reachable(s.position,z,.75f),std::string("creature ")+kinds[int(s.kind)]+" at "+std::to_string(s.position.x)+","+std::to_string(s.position.y));}
 for(const auto& s:w.pickupSpawns()){float z=s.z>-900?s.z:-999;check(reachable(s.position,z,.6f),std::string(s.kind==PickupKind::Ammo?"ammo":"health")+" at "+std::to_string(s.position.x)+","+std::to_string(s.position.y));}
 // A terminal is usable only from a reachable spot within 1.5 m that sees it.
 auto usable=[&](const Terminal& t){
  int r=int(std::ceil(1.5f/Cell));int cx=int(t.position.x/Cell),cy=int(t.position.y/Cell);
  for(int y=cy-r;y<=cy+r;++y)for(int x=cx-r;x<=cx+r;++x){if(x<0||y<0||x>=N||y>=N)continue;
   Vec2 at{(x+.5f)*Cell,(y+.5f)*Cell};if(length(at-t.position)>1.5f)continue;
   for(float v:visited[size_t(y*N+x)])if(std::fabs(v-t.z)<1.2f&&w.rayClear(at,v+1.5f,t.position,t.z+1.1f,false,false))return true;}
  return false;
 };
 for(const auto& t:w.terminals())check(usable(t),std::string("terminal '")+t.title+"' at "+std::to_string(t.position.x)+","+std::to_string(t.position.y));
 check(reachable(w.exitPoint(),-999,1.f),"exit");
 // Every door must be walkable straight through: a prop in front of a doorway
 // can leave the rooms connected some other way and still block the door.
 for(const auto& d:w.doors()){
  float cx=(d.left+d.right)*.5f;bool clear=true;
  for(float dy=-1.f;dy<=1.f;dy+=.25f)if(d.y+dy>0&&d.y+dy<24)clear&=reachable({cx,d.y+dy},-999,.15f);
  if(!clear){++problems;report<<"BLOCKED door x "<<d.left<<"-"<<d.right<<" at y "<<d.y<<'\n';}
 }
 for(const auto& f:w.fixtures())if(f.solid&&w.tile(int(f.position.x),int(f.position.y))=='#'){++problems;report<<"BURIED fixture model "<<f.model<<" at "<<f.position.x<<","<<f.position.y<<'\n';}
 // ASCII: '#' wall, '.' reachable, 'o' floor nobody can reach, '^' reachable above floor level.
 report<<"\n";
 for(int y=0;y<24;++y){std::string row;
  for(int x=0;x<24;++x){
   if(w.tile(x,y)=='#'){row+='#';continue;}
   bool any=false,raised=false;
   for(int sy2=0;sy2<4;++sy2)for(int sx2=0;sx2<4;++sx2)for(float v:visited[size_t((y*4+sy2)*N+x*4+sx2)]){any=true;raised|=v>w.floorHeight(x+.5f,y+.5f)+.5f;}
   row+=raised?'^':any?'.':'o';
  }
  report<<row<<'\n';
 }
 report<<(problems?"FAIL: ":"PASS: ")<<problems<<" placement problems\n";
 return problems==0;
}
}
