#include "Game.h"
#include "../world/CampaignMaps.h"
#include <fstream>
#include <queue>
#include <cmath>
#include <cstdio>
#include <map>
#include <limits>
#include <tuple>

namespace retro {
namespace {
constexpr int N=96;constexpr float Cell=24.f/N,Step=.45f,Jump=1.35f;
// Walkable space a player can reach from the map's start: a flood fill on a
// 0.25 m grid with the game's own support query and player hull test (0.45 m
// step, 1.35 m jump, standing or crouched, drops of up to 6 m). Stacked floors are separate
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
 // Where a move from cell (x,y) at height z to the neighbouring cell lands,
 // or NaN. A step rises 45 cm; a jump clears a 1.05 m rail or climbs 1.35 m
 // when the hull fits at the apex; drops of up to 6 m are allowed.
 auto landing=[&](int x,int y,float z,int nx,int ny,float body){
  const float nan=std::numeric_limits<float>::quiet_NaN();
  float cx=(nx+.5f)*Cell,cy=(ny+.5f)*Cell,mx=(x+.5f)*Cell*.5f+cx*.5f,my=(y+.5f)*Cell*.5f+cy*.5f;
  for(float rise:{Step,Jump}){
   // Like the player, stand on the highest support under the 40 cm hull.
   float feet=-1e9f;for(float ox:{-.2f,0.f,.2f})for(float oy:{-.2f,0.f,.2f})feet=std::max(feet,w.supportBelow(cx+ox,cy+oy,z+rise));
   if(!std::isfinite(feet)||feet<z-6.f||feet>z+rise)continue;
   if(!game.hullFits({cx,cy},feet+.02f,body))continue;
   // The move between the two cells must clear the hull: at the higher feet
   // for a step, at the apex for a jump.
   float clearAt=rise>Step?std::max(feet,z+Jump-.05f):std::max(feet,z);
   if(!game.hullFits({mx,my},clearAt+.02f,body))continue;
   return feet;
  }
  return nan;
 };
 auto flood=[&](float body){
  Reach r;
  auto seen=[&](int x,int y,float z){for(float v:r.visited[size_t(y*N+x)])if(std::fabs(v-z)<.3f)return true;return false;};
  struct Node{int x,y;float z;};std::queue<Node> open;
  auto seed=[&](Vec2 p,float z){int x=int(p.x/Cell),y=int(p.y/Cell);if(seen(x,y,z))return;r.visited[size_t(y*N+x)].push_back(z);open.push({x,y,z});};
  const auto start=w.definition().playerStart;
  r.startZ=w.supportBelow(start.x,start.y,w.definition().spawnHeight+1.f);
  seed(start,r.startZ);
  // A lift map continues where the ride ends: the cab crashed in its pit.
  // A freight cage is boarded at its upper landing and ridden down.
  const auto& cage=w.cargoLift();const bool freightCage=cage.upper>cage.lower;
  if(freightCage)game.m_world.setCargoLiftHeight(cage.upper);
  for(int pass=0;pass<2;++pass){
  if(pass==1){
   if(freightCage){game.m_world.setCargoLiftHeight(cage.lower);seed({(cage.x1+cage.x2)*.5f,(cage.y1+cage.y2)*.5f},cage.lower);}
   else{if(!w.hasLift())break;
   if(w.liftPhase()==World::LiftPhase::Ready){game.m_world.startLift();game.m_world.updateLift(World::LiftRideComplete);}
   seed({12,12.5f},w.liftHeight());}}
  while(!open.empty()){
   auto n=open.front();open.pop();++r.nodes;
   const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
   for(int k=0;k<4;++k){
    int nx=n.x+dx[k],ny=n.y+dy[k];if(nx<0||ny<0||nx>=N||ny>=N)continue;
    float cx=(nx+.5f)*Cell,cy=(ny+.5f)*Cell;
    float feet=landing(n.x,n.y,n.z,nx,ny,body);
    if(!std::isfinite(feet)||seen(nx,ny,feet))continue;
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
 auto spawnFeet=[&](Vec2 p,float z){return z==-999?w.floorHeight(p.x,p.y):z;};
 // Use spawnCreature's exact sentinel and floor rule, then test the whole hull.
 for(size_t i=0;i<w.creatureSpawns().size();++i){const auto& s=w.creatureSpawns()[i];const auto& traits=creatureTraits(s.kind);float z=spawnFeet(s.position,s.z);
  std::string name="creature["+std::to_string(i)+"] "+traits.name+" at "+at(s.position)+" z "+std::to_string(z);
  check(reachableIn(standing,s.position,z,.75f),name);
  if(!w.fits(s.position.x,s.position.y,z+.03f,traits.hull,false))fail("STUCK "+name);
  const float radius=(s.kind==CreatureKind::Wasp||s.kind==CreatureKind::Huntsman)?.2f:.35f;
  name+=" radius="+std::to_string(radius)+" top="+std::to_string(z+traits.top);
  auto intersects=[&](float x,float y,float hx,float hy){float dx=std::max(std::fabs(x)-hx,0.f),dy=std::max(std::fabs(y)-hy,0.f);return dx*dx+dy*dy<radius*radius;};
  for(size_t j=0;j<w.structures().size();++j){const auto& v=w.structures()[j];if(v.top<=z+traits.step||v.bottom>=z+traits.top)continue; // a plate or kerb under its feet is a step, not a wall
   if(intersects(s.position.x-(v.x1+v.x2)*.5f,s.position.y-(v.y1+v.y2)*.5f,(v.x2-v.x1)*.5f,(v.y2-v.y1)*.5f))fail("HULL INTERSECTION "+name+" structure["+std::to_string(j)+"] bounds "+at({v.x1,v.y1})+".."+at({v.x2,v.y2})+" bottom/top "+std::to_string(v.bottom)+"/"+std::to_string(v.top));}
  for(size_t j=0;j<w.fixtures().size();++j){const auto& f=w.fixtures()[j];if(!f.solid||f.core<=0)continue;float base=w.floorHeight(f.position.x,f.position.y)+f.base;if(base+f.height<=z+.01f||base>=z+traits.top)continue;
   float dx=s.position.x-f.position.x,dy=s.position.y-f.position.y,c=std::cos(f.yaw),sn=std::sin(f.yaw);
   if(intersects(dx*c-dy*sn,dx*sn+dy*c,f.width*f.core*.5f,f.depth*f.core*.5f))fail("HULL INTERSECTION "+name+" fixture["+std::to_string(j)+"] base/height "+std::to_string(base)+"/"+std::to_string(f.height)+" position "+at(f.position)+" width/depth/core/yaw "+std::to_string(f.width)+"/"+std::to_string(f.depth)+"/"+std::to_string(f.core)+"/"+std::to_string(f.yaw));}
 }
 for(size_t i=0;i<w.structures().size();++i){const auto& v=w.structures()[i];if(v.x1<-.15f||v.y1<-.15f||v.x2>24.15f||v.y2>24.15f)fail("OUTSIDE CHUNK structure["+std::to_string(i)+"] bounds "+at({v.x1,v.y1})+".."+at({v.x2,v.y2})+" bottom/top "+std::to_string(v.bottom)+"/"+std::to_string(v.top));}
 // Tile decks occupy exactly one cell inside the fixed 24 by 24 layer; their
 // parser/load checks already reject rows of any other width.
 for(const auto& s:w.pickupSpawns()){float z=s.z>-900?s.z:-999;std::string name=std::string(s.kind==PickupKind::Ammo?"ammo":"health")+" at "+at(s.position);
  check(reachable(s.position,z,.6f),name);
  if(!w.fits(s.position.x,s.position.y,(s.z>-900?s.z:w.supportBelow(s.position.x,s.position.y,w.floorHeight(s.position.x,s.position.y)+.5f))+.03f,.3f,false))fail("STUCK "+name);}
 for(const auto& c:w.clutterSpawns())if(!w.fits(c.position.x,c.position.y,(c.z>-900?c.z:w.supportBelow(c.position.x,c.position.y,w.floorHeight(c.position.x,c.position.y)+.5f))+.03f,.2f,false))fail("STUCK clutter at "+at(c.position));
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
 // A transfer door has one authored base. Open seams may carry more than
 // one floor (e.g. the reactor's containment corridor and upper manifold).
 // Match walkable surfaces across the seam; do not mistake a scenery roof
 // for the only exit, or a pit below the adjoining floor for a valid exit.
 auto surfaces=[](const World& world,Vec2 p){
  std::vector<float> result;auto add=[&](float z){if(!world.fits(p.x,p.y,z+.03f,Player::CrouchedHeight,false))return;for(float existing:result)if(std::fabs(existing-z)<.03f)return;result.push_back(z);};
  add(world.floorHeight(p.x,p.y));
  for(const auto& v:world.structures())if(!v.rail&&p.x>=v.x1&&p.x<v.x2&&p.y>=v.y1&&p.y<v.y2)add(v.top);
  return result;
 };
 auto exitHeights=surfaces(w,w.exitPoint());bool transferExit=false;
 for(const auto& d:w.doors())if(d.transfer){exitHeights={w.floorHeight((d.left+d.right)*.5f,d.y)+d.z};transferExit=true;break;}
 if(!transferExit){Vec2 outside=w.exitPoint(),inside=outside;bool seam=true;
  // Compare at the border itself: comparing points a metre apart can mistake
  // a continuous sloped basin/terrain bank for a discontinuous step.
  if(w.openSouthBoundary()){inside.y=23.99f;outside.y=24.01f;}else if(w.openEastBoundary()){inside.x=23.99f;outside.x=24.01f;}else if(w.openNorthBoundary()){inside.y=.01f;outside.y=-.01f;}else if(w.openWestBoundary()){inside.x=.01f;outside.x=-.01f;}else seam=false;
  if(seam){exitHeights=surfaces(w,inside);Vec2 absolute=outside+w.definition().origin;
   for(int other=0;other<worldChunkCount(WorldId::Campaign);++other){if(other==level)continue;auto data=campaignMap(other);const auto& definition=data?data->definition:chunkDefinition(WorldId::Campaign,other);auto local=absolute-definition.origin;
    if(local.x<0||local.x>=24||local.y<0||local.y>=24)continue;
    World neighbor(other);auto adjoining=surfaces(neighbor,local);std::vector<float> matched;
    for(float z:exitHeights)if(std::any_of(adjoining.begin(),adjoining.end(),[&](float next){return std::fabs(next-z)<=Step;}))matched.push_back(z);
    if(matched.empty()){std::string values="EXIT STEP to map "+std::to_string(other)+" at "+at(inside)+" current surfaces";for(float z:exitHeights)values+=' '+std::to_string(z);values+=" adjoining surfaces";for(float z:adjoining)values+=' '+std::to_string(z);fail(values);}
    exitHeights=std::move(matched);break;
   }
  }
 }
 auto exitHeight=[&](float feet){return std::any_of(exitHeights.begin(),exitHeights.end(),[&](float z){return std::fabs(feet-z)<=Step;});};
 bool exitReached=false;Vec2 exit=w.exitPoint();int exitX=int(exit.x/Cell),exitY=int(exit.y/Cell),exitRadius=int(std::ceil(1.f/Cell));
 for(int y=exitY-exitRadius;y<=exitY+exitRadius;++y)for(int x=exitX-exitRadius;x<=exitX+exitRadius;++x)if(x>=0&&y>=0&&x<N&&y<N)for(float feet:visited[size_t(y*N+x)])exitReached|=exitHeight(feet);
 std::string exitValues="exit at "+at(exit)+" surfaces";for(float z:exitHeights)exitValues+=' '+std::to_string(z);
 check(exitReached,exitValues);
 // Traps: a reachable spot from which the exit can no longer be reached (a
 // basin jumped into, a pit, a ledge dropped onto). Walk the reach graph
 // backwards from the exit with the same step rule, then list what is left.
 {
  const float body=Player::CrouchedHeight+.02f;
  std::vector<std::vector<float>> escapes(N*N);
  auto marked=[&](int x,int y,float z){for(float v:escapes[size_t(y*N+x)])if(std::fabs(v-z)<.3f)return true;return false;};
  struct Node{int x,y;float z;};std::queue<Node> open;
  const Vec2 exit=w.exitPoint();
  // The lift cab is a way out too: it carries the player down to the pit.
  for(int y=0;y<N;++y)for(int x=0;x<N;++x){Vec2 c{(x+.5f)*Cell,(y+.5f)*Cell};
   bool inCage=w.cargoLift().upper>w.cargoLift().lower&&c.x>w.cargoLift().x1-.3f&&c.x<w.cargoLift().x2+.3f&&c.y>w.cargoLift().y1-.3f&&c.y<w.cargoLift().y2+.3f;
   if(length(c-exit)>1.f&&!(w.hasLift()&&w.insideLift(c.x,c.y))&&!inCage)continue;
   for(float v:visited[size_t(y*N+x)]){if(!inCage&&!w.hasLift()&&!exitHeight(v))continue;escapes[size_t(y*N+x)].push_back(v);open.push({x,y,v});}}
  while(!open.empty()){
   auto n=open.front();open.pop();
   const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
   for(int k=0;k<4;++k){int px=n.x+dx[k],py=n.y+dy[k];if(px<0||py<0||px>=N||py>=N)continue;
    for(float pz:visited[size_t(py*N+px)]){
     if(marked(px,py,pz))continue;
     // Would a forward move from (px,py,pz) land on n?
     float feet=landing(px,py,pz,n.x,n.y,body);if(!std::isfinite(feet)||std::fabs(feet-n.z)>=.3f)continue;
     escapes[size_t(py*N+px)].push_back(pz);open.push({px,py,pz});
    }
   }
  }
  int trapped=0;std::string first;
  for(int y=0;y<N;++y)for(int x=0;x<N;++x)for(float v:visited[size_t(y*N+x)])if(!marked(x,y,v)){
   if(!trapped)first=at({(x+.5f)*Cell,(y+.5f)*Cell})+" z "+std::to_string(v);++trapped;}
  if(trapped)fail("TRAP "+std::to_string(trapped)+" reachable cells cannot get back to the exit, first at "+first);
 }
 // Every door must be walkable straight through: a prop in front of a doorway
 // can leave the rooms connected some other way and still block the door.
 for(size_t i=0;i<w.doors().size();++i){const auto& d=w.doors()[i];float cx=(d.left+d.right)*.5f,base=w.floorHeight(cx,d.y)+d.z;bool clear=true;
  // Like the flood fill, assess a lift landing with the cage at that landing.
  // A called platform is a support; a temporarily lowered cage is not a pit.
  float previousCage=w.cargoLiftHeight();const auto& cage=w.cargoLift();
  if(cage.upper>cage.lower){if(std::fabs(base-cage.upper)<=Step)game.m_world.setCargoLiftHeight(cage.upper);else if(std::fabs(base-cage.lower)<=Step)game.m_world.setCargoLiftHeight(cage.lower);}
  for(float dy=-1.f;dy<=1.f;dy+=.25f)if(d.y+dy>0&&d.y+dy<24)clear&=reachable({cx,d.y+dy},base,.15f);
  if(!clear)fail("BLOCKED door["+std::to_string(i)+"] x "+std::to_string(d.left)+"-"+std::to_string(d.right)+" at y "+std::to_string(d.y)+" base "+std::to_string(base));
  for(float side:{-1.f,1.f}){float y=d.y+side*.35f;if(y<0||y>=24)continue;
   float surface=w.supportBelow(cx,y,base+.45f);
   if(!std::isfinite(surface)||std::fabs(surface-base)>.45f||!w.fits(cx,y,surface+.02f,Player::CrouchedHeight,false))fail("DOOR STEP door["+std::to_string(i)+"] at "+at({cx,d.y})+" left="+std::to_string(d.left)+" right="+std::to_string(d.right)+" relativeZ="+std::to_string(d.z)+" side="+std::to_string(side)+" base="+std::to_string(base)+" surface="+std::to_string(surface));
  }
  game.m_world.setCargoLiftHeight(previousCage);
 }
 // Water sits in its basin, never as a slab above the floor around it.
 for(const auto& b:w.waterVolumes())if(b.surface>w.layers().front().elevation-.02f)fail("WATER ABOVE FLOOR basin at "+at({b.x1,b.y1})+" surface "+std::to_string(b.surface));
 // Stairs are single flights: two flights that touch read as one broken stair.
 if(auto map=campaignMap(level))for(size_t i=0;i<map->stairs.size();++i)for(size_t j=i+1;j<map->stairs.size();++j){const auto& a=map->stairs[i];const auto& b=map->stairs[j];
  if(a.x1<b.x2+.01f&&a.x2>b.x1-.01f&&a.y1<b.y2+.01f&&a.y2>b.y1-.01f&&std::fabs(a.bottom-b.top)>.01f&&std::fabs(a.top-b.bottom)>.01f)fail("STAIRS TOUCH at "+at({a.x1,a.y1})+" and "+at({b.x1,b.y1}));}
 // Pipes: no run crosses a walkable space at body height, no horizontal run
 // passes through a deck slab, and every end lands on something: a wall,
 // slab, floor, roof, fixture or a longer pipe. Flanges and elbows (short
 // pieces) are dressing, not anchors.
 {
  const auto& pipes=w.pipes();
  // A run is vertical when its ends share an xy; otherwise it may slope.
  auto vertical=[](const PipeRun& p){return lengthSq(p.end-p.start)<1e-6f;};
  auto topZ=[](const PipeRun& p){return p.endZ>-999.f?p.endZ:p.z;};
  auto pipeLength=[&](const PipeRun& p){return vertical(p)?std::fabs(topZ(p)-p.z):length(p.end-p.start);};
  // Dressing is short and thin; a wide disc (a platform, a drum lid) is an anchor.
  auto dressing=[&](const PipeRun& p){return pipeLength(p)<.15f&&p.radius<.3f;};
  auto distanceToPipe=[&](const PipeRun& p,Vec2 xy,float z){
   if(vertical(p)){float lo=std::min(p.z,topZ(p)),hi=std::max(p.z,topZ(p));float dz=z<lo?lo-z:z>hi?z-hi:0;return std::sqrt(lengthSq(xy-p.start)+dz*dz);}
   Vec2 d=p.end-p.start;float l2=lengthSq(d);float t=l2>1e-6f?std::clamp(dot(xy-p.start,d)/l2,0.f,1.f):0.f;Vec2 c=p.start+d*t;float cz=p.z+(topZ(p)-p.z)*t;return std::sqrt(lengthSq(xy-c)+(z-cz)*(z-cz));};
  // Walkways are floors, decks and stairs: standing on top of a pump is not one.
  auto onWalkway=[&](Vec2 xy,float feet){
   for(const auto& layer:w.layers())if(std::fabs(feet-layer.elevation)<.25f)return true;
   if(auto map=campaignMap(level))for(const auto& st:map->stairs)if(xy.x>=st.x1&&xy.x<st.x2&&xy.y>=st.y1&&xy.y<st.y2&&feet>=std::min(st.bottom,st.top)-.1f&&feet<=std::max(st.bottom,st.top)+.1f)return true;
   return false;};
  // Equipment carries pipes: an end inside a fixture or prop footprint is held.
  auto onEquipment=[&](Vec2 xy,float z){
   for(const auto& f:w.fixtures()){float c=std::cos(f.yaw),sn=std::sin(f.yaw),dx=xy.x-f.position.x,dy=xy.y-f.position.y;
    if(std::fabs(dx*c-dy*sn)<f.width*.5f+.1f&&std::fabs(dx*sn+dy*c)<f.depth*.5f+.1f){float base=w.floorHeight(f.position.x,f.position.y)+f.base;if(z>=base-.1f&&z<=base+f.height+.3f)return true;}}
   for(const auto& p:w.props())if(std::fabs(xy.x-p.position.x)<p.halfSize.x+.1f&&std::fabs(xy.y-p.position.y)<p.halfSize.y+.1f){float base=w.floorHeight(p.position.x,p.position.y)+p.base;if(z>=base-.1f&&z<=base+p.height+.3f)return true;}
   return false;};
  auto solidNear=[&](Vec2 xy,float z,float reach){
   for(auto [dx,dy,dz]:{std::tuple{reach,0.f,0.f},{-reach,0.f,0.f},{0.f,reach,0.f},{0.f,-reach,0.f},{0.f,0.f,reach},{0.f,0.f,-reach}}){
    float px=xy.x+dx,py=xy.y+dy,pz=z+dz;if(px<0||py<0||px>=24||py>=24)return true;
    if(pz<=w.floorHeight(px,py)+.01f||pz>=w.ceilingHeight(px,py)-.01f||!w.fits(px,py,pz,.02f,false))return true;}
   return false;};
  for(size_t i=0;i<pipes.size();++i){const auto& p=pipes[i];
   const bool isVertical=vertical(p);const float radius=p.radius,z1=topZ(p);
   if(dressing(p))continue;
   // A stair's own stringers run inside its footprint from landing to landing.
   bool stringer=false;if(auto map=campaignMap(level))for(const auto& st:map->stairs){
    auto inside=[&](Vec2 q){return q.x>=st.x1-.05f&&q.x<=st.x2+.05f&&q.y>=st.y1-.05f&&q.y<=st.y2+.05f;};
    if(inside(p.start)&&inside(p.end)&&p.radius<.1f)stringer=true;}
   if(stringer)continue;
   float lo=std::min(p.z,z1)-radius,hi=std::max(p.z,z1)+radius;
   std::string name="pipe "+at(p.start)+(isVertical?" z "+std::to_string(p.z)+".."+std::to_string(z1):" to "+at(p.end)+" z "+std::to_string(p.z));
   // Through a walkway or a deck slab.
   Vec2 d=isVertical?Vec2{}:p.end-p.start;int samples=std::max(1,int(std::ceil(length(d)/.25f)));bool through=false,slab=false;
   for(int s=0;s<=samples&&!through;++s){Vec2 q=p.start+d*(float(s)/samples);int cx=int(q.x/Cell),cy=int(q.y/Cell);if(cx<0||cy<0||cx>=N||cy>=N)continue;
    Vec2 centre{(cx+.5f)*Cell,(cy+.5f)*Cell};if(isVertical&&length(centre-q)>radius+.2f)continue;
    // Only the head space actually open above that floor counts.
    for(float feet:visited[size_t(cy*N+cx)]){if(!onWalkway(centre,feet))continue;float head=std::min(feet+Player::StandingHeight+.1f,w.clearanceAbove(centre.x,centre.y,feet+.05f));if(lo<head&&hi>feet+.25f){through=true;break;}}
    if(!isVertical)for(const auto& layer:w.layers())if(layer.thickness>0&&layer.rows[size_t(std::clamp(int(q.y),0,23))][size_t(std::clamp(int(q.x),0,23))]=='='&&p.z-radius<layer.elevation&&p.z+radius>layer.elevation-layer.thickness)slab=true;
   }
   // A drum or vessel standing wholly on a solid prop (a pallet, a skid)
   // occupies ground the player cannot stand on anyway.
   if(through&&isVertical){float bottom=std::min(p.z,z1);
    for(const auto& f:w.fixtures()){if(!f.solid)continue;float fb=w.floorHeight(f.position.x,f.position.y)+f.base,c=std::cos(f.yaw),sn=std::sin(f.yaw),dx=p.start.x-f.position.x,dy=p.start.y-f.position.y;
     if(std::fabs(bottom-(fb+f.height))<.03f&&std::fabs(dx*c-dy*sn)+radius<=f.width*.5f+.02f&&std::fabs(dx*sn+dy*c)+radius<=f.depth*.5f+.02f){through=false;break;}}}
   if(through)fail("PIPE THROUGH WALKWAY "+name);
   if(slab)fail("PIPE THROUGH DECK "+name);
   // Anchored ends. A wide vertical cylinder is a vessel or drum: its top is free.
   auto anchored=[&](Vec2 xy,float z){
    if(solidNear(xy,z,radius+.07f)||onEquipment(xy,z))return true;
    for(size_t j=0;j<pipes.size();++j){if(j==i)continue;const auto& o=pipes[j];if(dressing(o))continue;if(distanceToPipe(o,xy,z)<=radius+o.radius+.06f)return true;}
    return false;};
   if(isVertical){float bottom=std::min(p.z,z1),top=std::max(p.z,z1);
    // A short drop hung from an anchored top (a shower, a hook shank) may end free.
    bool drop=top-bottom<1.2f&&radius<.08f&&anchored(p.start,top);
    if(!drop&&!anchored(p.start,bottom))fail("FLOATING "+name+" (bottom end)");
    if(radius<.3f&&!anchored(p.start,top))fail("FLOATING "+name+" (top end)");
   }else{
    if(!anchored(p.start,p.z))fail("FLOATING "+name+" (start)");
    if(!anchored(p.end,z1))fail("FLOATING "+name+" (end)");
   }
  }
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
 // Signage is sparse. A plant signs its hazards, a locked door and the odd
 // junction, not every wall: more than three plates in a map, or the same
 // plate twice, is clutter.
 const auto& signs=w.signs();
 if(signs.size()>3)fail("SIGNAGE "+std::to_string(signs.size())+" signs (budget 3)");
 for(size_t i=0;i<signs.size();++i)for(size_t j=i+1;j<signs.size();++j)
  if(signs[i].title==signs[j].title)fail("SIGNAGE duplicate '"+signs[i].title+"'");
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
 // One record for every reachable border tile and floor, with standing status.
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(x==0||x==23||y==0||y==23){
  std::map<int,bool> floors;
  for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx){size_t cell=size_t((y*4+sy)*N+x*4+sx);for(float v:visited[cell])floors.try_emplace(int(std::lround(v*1000)),false);for(float v:standing.visited[cell])floors[int(std::lround(v*1000))]=true;}
  for(auto [height,upright]:floors)report<<"BORDER "<<x<<' '<<y<<' '<<height/1000.f<<' '<<(upright?'.':'c')<<'\n';
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
