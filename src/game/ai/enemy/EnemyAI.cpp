#include "../../../core/FrameProfile.h"
#include "../../Game.h"
#include "../../CreatureAnimation.h"
#include "../Perception.h"
#include "../Navigation.h"
#include "../RouteCache.h"
#include "ScheduleTables.h"
#include <bit>
#include <queue>
#include <cmath>
#include <fstream>
#include <optional>
namespace retro {
static float navSupport(const World&w,Vec2 p,float feet,float radius=.20f){float z=std::max(w.supportBelow(p.x,p.y,feet),w.structureSupportBelow(p.x,p.y,radius,feet));for(float x:{-radius,radius})for(float y:{-radius,radius})z=std::max(z,w.supportBelow(p.x+x,p.y+y,feet));return z;}
static bool navFits(const World&w,Vec2 p,float z,float height,float radius=.20f){perf::count(perf::NavFits);if(w.structureBlocksHull(p.x,p.y,radius,z,height))return false;for(float x:{-radius,0.f,radius})for(float y:{-radius,0.f,radius})if(!w.fits(p.x+x,p.y+y,z,height)||w.doorBlocks(p.x+x,p.y+y,z,height))return false;return true;}
// Openings need not be centred on the metre grid. Retain a clear point
// inside each span's cell, nudging away from a jamb when necessary.
// rise: the step the walker takes (the deck's .215 m; a creature's own interior
// step), so a node may stand on a raised floor it climbs: a culvert's or duct's.
static bool navCellPoint(const World&w,int x,int y,float floor,float height,float radius,Vec2&p,float&z,float rise=.215f){
 // Prefer the aperture centre on both sides of a door. Merely finding a safe
 // point farther from its wall can leave adjacent grid nodes on the jamb side
 // of a narrow opening, with no edge that actually passes through it.
 for(const auto&door:w.doors()){
  // In the door's frame: the cell its centre's span falls in, across the wall.
  const Vec2 cell{x+.5f,y+.5f},mid=door.centre();const float center=(door.left+door.right)*.5f,cellAlong=door.along(cell),cellAcross=door.across(cell),base=w.floorHeight(mid.x,mid.y)+door.z;
  if(center<cellAlong-.5f||center>=cellAlong+.5f||std::fabs(cellAcross-door.y)>1.25f||std::fabs(floor-base)>.24f)continue;
  p=door.world(center,cellAcross);z=navSupport(w,p,floor+rise,radius);
  if(std::fabs(z-floor)<rise+.025f&&navFits(w,p,z,height,radius))return true;
 }
 for(float dx:{0.f,-.25f,.25f})for(float dy:{0.f,-.25f,.25f}){
  p={x+.5f+dx,y+.5f+dy};z=navSupport(w,p,floor+rise,radius);
  if(std::fabs(z-floor)<rise+.025f&&navFits(w,p,z,height,radius))return true;
 }return false;
}
static float roughStep(Enemy::Kind kind){
 return creatureTraits(kind).step;
}
// Surface Nets navigation follows the actual terrain support instead of comparing
// one-metre cell centres. A one-metre voxel rise becomes a traversable slope when
// its short movement samples stay within the creature's step capability, while a
// real cliff still fails the same test.
static bool roughSegment(const World&w,Vec2 from,float fromZ,Vec2 to,float height,float stepHeight,float* endZ=nullptr,bool sweep=false,float radius=.20f){
 perf::count(perf::RoughSegment);
 auto delta=to-from;float distance=length(delta);if(distance<.001f){if(endZ)*endZ=fromZ;return navFits(w,from,fromZ,height,radius);}
 float pitch=aiWalkSamplePitch(w.hasTerrain());int samples=std::max(1,int(std::ceil(distance/pitch)));float z=fromZ;
 for(int i=1;i<=samples;++i){Vec2 p=from+delta*(float(i)/samples);float ground=navSupport(w,p,z+stepHeight+.03f,radius);
  if(sweep&&!aiStructureSegment(w,from+delta*(float(i-1)/samples),z,p,height,true,radius))return false;
  float rise=ground-z,drop=z-ground;if(rise>stepHeight+.025f||drop>std::max(.48f,stepHeight*1.45f)||!navFits(w,p,ground,height,radius))return false;z=ground;
 }
 if(endZ)*endZ=z;return true;
}
// One deck edge: four samples from a node toward a neighbour node, each a step
// of at most `rise` up or down that the hull fits; `feet` ends where it lands.
// Two nodes off each other's line (one in a duct, nudged off its jamb, one in
// the room before its mouth) may also join by the square corner between them:
// along the room, then straight in, as a walker lines up on an opening.
static bool deckLeg(const World&w,Vec2 from,Vec2 to,float height,float radius,float rise,float&feet){
 for(int sample=1;sample<=4;++sample){Vec2 p=from+(to-from)*(sample/4.f);float ground=navSupport(w,p,feet+rise,radius);
  if(std::fabs(ground-feet)>rise+.025f||!navFits(w,p,ground,height,radius))return false;feet=ground;}
 return true;
}
// A point under a roof close over the hull's head (within .6 m): inside a duct,
// culvert or crawlway rather than out in a room.
static bool inPassage(const World&w,Vec2 p,float feet,float height,float radius,float rise){
 const float floor=navSupport(w,p,feet+rise,radius);return w.structureBlocksHull(p.x,p.y,radius,floor,height+.6f);
}
static bool deckStep(const World&w,Vec2 from,float fromZ,Vec2 to,float height,float radius,float rise,float&feet){
 feet=fromZ;if(deckLeg(w,from,to,height,radius,rise,feet))return true;
 // The bend is for passages only: out in a room the straight edge is the walk.
 if(std::fabs(from.x-to.x)<.01f||std::fabs(from.y-to.y)<.01f||!(inPassage(w,to,fromZ,height,radius,rise)||inPassage(w,from,fromZ,height,radius,rise)))return false;
 for(Vec2 corner:{Vec2{to.x,from.y},Vec2{from.x,to.y}}){feet=fromZ;if(deckLeg(w,from,corner,height,radius,rise,feet)&&deckLeg(w,corner,to,height,radius,rise,feet))return true;}
 return false;
}
// A waypoint the walker can head straight for: the next node, or when the line
// from its feet clips a jamb or a duct's rim, the square corner that lines it up
// on the node first (the bend deckStep allows).
// A walker stepping up into a raised passage (a duct's or culvert's floor) also
// lines up first, entering square-on: a slanting entry scrapes the rim where the
// planner's coarser samples slip past it.
static Vec2 lineUp(const World&w,Vec2 start,float startZ,Vec2 next,float height,float radius,float rise){
 const bool entering=std::fabs(navSupport(w,next,startZ+rise,radius)-startZ)>.1f&&inPassage(w,next,startZ,height,radius,rise);
 if(!entering&&roughSegment(w,start,startZ,next,height,rise+.025f,nullptr,true,radius))return next;
 // The corner that leaves the longer last leg first: the square entry.
 Vec2 corners[2]={Vec2{next.x,start.y},Vec2{start.x,next.y}};
 if(std::fabs(next.y-start.y)<std::fabs(next.x-start.x))std::swap(corners[0],corners[1]);
 for(Vec2 corner:corners){float feet=startZ;
  if(lengthSq(corner-start)>.0025f&&roughSegment(w,start,startZ,corner,height,rise+.025f,&feet,true,radius)&&roughSegment(w,corner,feet,next,height,rise+.025f,nullptr,true,radius))return corner;}
 return next;
}
// The deck planner as it probes with nothing kept: the reference the route
// cache must match, and the planner wherever nothing may be kept.
static Vec2 liveStackedWaypoint(const World&w,Vec2 start,float startZ,Vec2 goal,float goalZ,float height,bool human=false,float radius=.20f,float rise=.215f){
 perf::Scope stackedProfile(perf::StackedWaypoint);perf::count(perf::StackedCalls);
 struct Node{Vec2 p;float z;int parent=-1;};std::vector<Node> nodes;std::array<std::vector<int>,24*24> cells;
 for(int y=1;y<23;++y)for(int x=1;x<23;++x)for(auto span:w.spansAt(x,y))if(span.ceiling-span.floor>=height&&(w.tile(x,y)!='#'||span.floor>=2.99f)){
  Vec2 p;float floor;if(!navCellPoint(w,x,y,span.floor,height,radius,p,floor,rise))continue;
  cells[y*24+x].push_back(int(nodes.size()));nodes.push_back({p,floor});}
 perf::count(perf::StackedNodes,nodes.size());
 int origin=-1,target=-1;float a=1e9f,b=1e9f;
 for(int i=0;i<int(nodes.size());++i){auto&n=nodes[i];float da=lengthSq(n.p-start)+std::fabs(n.z-startZ)*8,db=lengthSq(n.p-goal)+std::fabs(n.z-goalZ)*8;if(da<a){a=da;origin=i;}if(db<b){b=db;target=i;}}
 if(origin<0||target<0){perf::count(perf::StackedNoRoute);return start;}std::queue<int> queue;queue.push(origin);nodes[origin].parent=origin;
 while(!queue.empty()&&nodes[target].parent<0){int current=queue.front();queue.pop();auto n=nodes[current];int x=int(n.p.x),y=int(n.p.y);
  for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<1||ny<1||nx>22||ny>22)continue;
   for(int next:cells[ny*24+nx]){if(nodes[next].parent>=0)continue;perf::count(perf::StackedEdges);float feet=n.z;const bool clear=deckStep(w,n.p,n.z,nodes[next].p,height,radius,rise,feet);
    if(clear&&std::fabs(feet-nodes[next].z)<.03f){nodes[next].parent=current;queue.push(next);}
   }
  }
 }
 if(nodes[target].parent<0){perf::count(perf::StackedNoRoute);return start;}int next=target;while(nodes[next].parent!=origin&&next!=origin)next=nodes[next].parent;
 // The actor can enter a navigation cell near an obstacle corner. Its next
 // cell-to-cell edge is safe from the cell centre, but the diagonal from the
 // actor's actual feet may clip that corner. Reach the origin centre first.
 if(human&&lengthSq(nodes[origin].p-start)>.0025f&&!roughSegment(w,start,startZ,nodes[next].p,height,rise+.025f,nullptr,true,radius)&&roughSegment(w,start,startZ,nodes[origin].p,height,rise+.025f,nullptr,true,radius))return nodes[origin].p;
 return human?lineUp(w,start,startZ,nodes[next].p,height,radius,rise):nodes[next].p;
}
static bool sameBits(Vec2 a,Vec2 b){return std::bit_cast<uint32_t>(a.x)==std::bit_cast<uint32_t>(b.x)&&std::bit_cast<uint32_t>(a.y)==std::bit_cast<uint32_t>(b.y);}
// A verify-mode shadow probe leaves the frame's query and decision counters as
// the kept answer left them (only its time shows).
template<class Probe> static auto uncounted(const Probe&probe){auto counts=perf::counts;auto result=probe();perf::counts=counts;return result;}
// The deck graph's own pieces, which the kept planner and a creature's partial
// route share. A fresh deck takes its nodes and cell lists exactly as
// liveStackedWaypoint builds them; its edges start unknown.
static void fillDeck(const World&w,ai::RouteCache::Deck&deck,float height,float radius,float rise=.215f){
 auto&nodes=deck.nodes;auto&cells=deck.cells;
 for(int y=1;y<23;++y)for(int x=1;x<23;++x){cells[y*24+x].begin=int(nodes.size());for(auto span:w.spansAt(x,y))if(span.ceiling-span.floor>=height&&(w.tile(x,y)!='#'||span.floor>=2.99f)){
  Vec2 p;float floor;if(!navCellPoint(w,x,y,span.floor,height,radius,p,floor,rise))continue;
  nodes.push_back({p,floor});}cells[y*24+x].end=int(nodes.size());}
 deck.edges.assign(nodes.size()*4,{});
}
// The step from node `current` toward its neighbour cell in `direction` (+x, -x,
// +y, -y), ending on node `next` there: probed on first need exactly as
// liveStackedWaypoint probes it (the walk does not depend on which node of the
// next cell it reaches), then kept.
static bool deckEdge(const World&w,ai::RouteCache::Deck&deck,int current,int direction,int next,float height,float radius,float rise=.215f){
 const auto&nodes=deck.nodes;const auto n=nodes[size_t(current)];auto&edge=deck.edges[size_t(current*4+direction)];
 if(edge.state==ai::RouteCache::Unknown||lengthSq(edge.goal-nodes[size_t(next)].p)>.000001f){float feet=n.z;const bool clear=deckStep(w,n.p,n.z,nodes[size_t(next)].p,height,radius,rise,feet);
  edge={feet,clear?ai::RouteCache::Clear:ai::RouteCache::Blocked,nodes[size_t(next)].p};}
 return edge.state==ai::RouteCache::Clear&&std::fabs(edge.feet-nodes[size_t(next)].z)<.03f;
}
// The node nearest a point, a metre of height weighed as eight square metres of floor; -1 on an empty deck.
static int nearestDeckNode(const ai::RouteCache::Deck&deck,Vec2 p,float z){
 int best=-1;float score=1e9f;for(int i=0;i<int(deck.nodes.size());++i){const auto&n=deck.nodes[size_t(i)];float d=lengthSq(n.p-p)+std::fabs(n.z-z)*8;if(d<score){score=d;best=i;}}return best;
}
// The same planner over the deck graph kept for this collision state and hull.
// Origin and target choice, search order, early stop and the corner rule are
// unchanged, so the waypoint is the live planner's to the bit.
static Vec2 keptStackedWaypoint(const World&w,ai::RouteCache::Deck&deck,bool fresh,Vec2 start,float startZ,Vec2 goal,float goalZ,float height,bool human,float radius,float rise){
 auto&nodes=deck.nodes;auto&cells=deck.cells;
 if(fresh)fillDeck(w,deck,height,radius,rise);
 perf::count(perf::StackedNodes,nodes.size());
 const int origin=nearestDeckNode(deck,start,startZ),target=nearestDeckNode(deck,goal,goalZ);
 if(origin<0||target<0){perf::count(perf::StackedNoRoute);return start;}
 thread_local std::vector<int> parent;parent.assign(nodes.size(),-1);std::queue<int> queue;queue.push(origin);parent[size_t(origin)]=origin;
 while(!queue.empty()&&parent[size_t(target)]<0){int current=queue.front();queue.pop();auto n=nodes[current];int x=int(n.p.x),y=int(n.p.y),direction=-1;
  for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){++direction;int nx=x+int(d.x),ny=y+int(d.y);if(nx<1||ny<1||nx>22||ny>22)continue;
   for(int next=cells[ny*24+nx].begin;next<cells[ny*24+nx].end;++next){if(parent[size_t(next)]>=0)continue;perf::count(perf::StackedEdges);
    if(deckEdge(w,deck,current,direction,next,height,radius,rise)){parent[size_t(next)]=current;queue.push(next);}
   }
  }
 }
 if(parent[size_t(target)]<0){perf::count(perf::StackedNoRoute);return start;}int next=target;while(parent[size_t(next)]!=origin&&next!=origin)next=parent[size_t(next)];
 if(human&&lengthSq(nodes[origin].p-start)>.0025f&&!roughSegment(w,start,startZ,nodes[next].p,height,rise+.025f,nullptr,true,radius)&&roughSegment(w,start,startZ,nodes[origin].p,height,rise+.025f,nullptr,true,radius))return nodes[origin].p;
 return human?lineUp(w,start,startZ,nodes[next].p,height,radius,rise):nodes[next].p;
}
static Vec2 stackedWaypoint(const World&w,Vec2 start,float startZ,Vec2 goal,float goalZ,float height,bool human=false,float radius=.20f,float rise=.215f){
 Vec2 result;
 {perf::Scope stackedProfile(perf::StackedWaypoint);bool fresh=false;auto*deck=ai::RouteCache::deck(w,height,fresh,radius,rise);
  if(!deck){stackedProfile.stop();return liveStackedWaypoint(w,start,startZ,goal,goalZ,height,human,radius,rise);}
  perf::count(perf::StackedCalls);result=keptStackedWaypoint(w,*deck,fresh,start,startZ,goal,goalZ,height,human,radius,rise);}
 if(ai::RouteCache::verify()&&!sameBits(result,uncounted([&]{return liveStackedWaypoint(w,start,startZ,goal,goalZ,height,human,radius,rise);})))perf::count(perf::RouteCacheMismatches);
 return result;
}
// Where a creature goes when no route on the decks fits its body to the goal
// (the player crawled into a duct it is too big for), or when it already stands
// at the node nearest the goal: never shoving at the opening or ambling off.
// It floods every node it can reach. Then it takes the prey's own way to the
// goal, walked by the smallest body that moves (a prone player), and finds
// where that way leaves the places the creature fits: the duct's mouth. It
// goes to the last of its own nodes there and holds, facing in, until the goal
// moves somewhere it can reach. Sets `next` (its next hop, or its own position
// when holding) and `holding`. False when the prey's way leaves no mouth to
// wait at (no way for the prey either, or a deck too coarse for the body to
// find one): the caller's amble then works it round what holds it.
static bool partialDeckRoute(const World&w,Vec2 start,float startZ,Vec2 goal,float goalZ,float height,float radius,float rise,Vec2&next,bool&holding){
 // The prey clambers: a lip or duct floor half a metre up is in its way, not a wall.
 constexpr float PreyHeight=Player::ProneHeight,PreyRadius=.20f,PreyRise=.5f;
 holding=false;next=start;
 // The deck pointer lives until the next lookup: copy what the flood found before the prey's deck is asked for.
 std::vector<ai::RouteCache::Deck::Node> nodes;std::array<ai::RouteCache::Deck::Range,24*24> cells{};std::vector<int> parent;int origin=-1,target=-1;
 {ai::RouteCache::Deck local;bool fresh=false;auto*deck=ai::RouteCache::deck(w,height,fresh,radius,rise);if(!deck){deck=&local;fresh=true;}
  if(fresh)fillDeck(w,*deck,height,radius,rise);
  origin=nearestDeckNode(*deck,start,startZ);target=nearestDeckNode(*deck,goal,goalZ);if(origin<0||target<0)return false;
  parent.assign(deck->nodes.size(),-1);std::queue<int> queue;queue.push(origin);parent[size_t(origin)]=origin;
  while(!queue.empty()){int current=queue.front();queue.pop();const auto n=deck->nodes[size_t(current)];int x=int(n.p.x),y=int(n.p.y),direction=-1;
   for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){++direction;int nx=x+int(d.x),ny=y+int(d.y);if(nx<1||ny<1||nx>22||ny>22)continue;
    for(int k=deck->cells[size_t(ny*24+nx)].begin;k<deck->cells[size_t(ny*24+nx)].end;++k)if(parent[size_t(k)]<0&&deckEdge(w,*deck,current,direction,k,height,radius,rise)){parent[size_t(k)]=current;queue.push(k);}}}
  nodes=deck->nodes;cells=deck->cells;}
 // Its own reachable node in a cell, on the floor near z.
 auto reachableIn=[&](Vec2 p,float z){const int c=std::clamp(int(p.y),0,23)*24+std::clamp(int(p.x),0,23);
  for(int k=cells[size_t(c)].begin;k<cells[size_t(c)].end;++k)if(parent[size_t(k)]>=0&&std::fabs(nodes[size_t(k)].z-z)<.3f)return k;return -1;};
 int hold=-1;
 if(parent[size_t(target)]>=0)hold=target;   // as near as the decks come: it stands there
 else{
  ai::RouteCache::Deck local;bool fresh=false;auto*prey=ai::RouteCache::deck(w,PreyHeight,fresh,PreyRadius,PreyRise);if(!prey){prey=&local;fresh=true;}
  if(fresh)fillDeck(w,*prey,PreyHeight,PreyRadius,PreyRise);
  const int from=nearestDeckNode(*prey,start,startZ),to=nearestDeckNode(*prey,goal,goalZ);
  if(from>=0&&to>=0){std::vector<int> way(prey->nodes.size(),-1);std::queue<int> queue;queue.push(from);way[size_t(from)]=from;
   while(!queue.empty()&&way[size_t(to)]<0){int current=queue.front();queue.pop();const auto n=prey->nodes[size_t(current)];int x=int(n.p.x),y=int(n.p.y),direction=-1;
    for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){++direction;int nx=x+int(d.x),ny=y+int(d.y);if(nx<1||ny<1||nx>22||ny>22)continue;
     for(int k=prey->cells[size_t(ny*24+nx)].begin;k<prey->cells[size_t(ny*24+nx)].end;++k)if(way[size_t(k)]<0&&deckEdge(w,*prey,current,direction,k,PreyHeight,PreyRadius,PreyRise)){way[size_t(k)]=current;queue.push(k);}}}
   if(way[size_t(to)]>=0){std::vector<int> path;for(int k=to;;k=way[size_t(k)]){path.push_back(k);if(k==from)break;}
    // From the creature's end: the last of the prey's steps it can still stand on.
    bool mouth=false;
    for(auto it=path.rbegin();it!=path.rend();++it){const auto&n=prey->nodes[size_t(*it)];const int own=reachableIn(n.p,n.z);if(own<0){mouth=hold>=0;break;}hold=own;}
    // An ambusher waits beside the mouth, not in it, so smaller things that fit
    // still get by. The mouth: its own nodes within 1.25 m of a place on the
    // prey's deck it cannot reach. It holds at the reachable node off the mouth
    // (within 3 m of it) nearest the mouth's middle; that choice depends only on
    // the decks, so it does not shift as the creature walks.
    if(mouth){const auto at=nodes[size_t(hold)];std::vector<Vec2> beyond;
     for(const auto&q:prey->nodes)if(lengthSq(q.p-at.p)<16.f&&reachableIn(q.p,q.z)<0)beyond.push_back(q.p);
     auto nearBeyond=[&](Vec2 p){for(const auto&u:beyond)if(lengthSq(u-p)<1.5625f)return true;return false;};
     Vec2 middle{};int count=0;std::vector<int> near;
     for(int k=0;k<int(nodes.size());++k){const auto&q=nodes[size_t(k)];if(parent[size_t(k)]<0||std::fabs(q.z-at.z)>.3f||lengthSq(q.p-at.p)>9.f)continue;
      if(nearBeyond(q.p)){middle+=q.p;++count;}else near.push_back(k);}
     if(count){middle=middle*(1.f/float(count));int beside=-1;float best=1e9f;
      for(int k:near){const float d=lengthSq(nodes[size_t(k)].p-middle);if(d<best-1e-4f){best=d;beside=k;}}
      if(beside>=0)hold=beside;}}
    if(!mouth)hold=-1;}}
 }
 if(hold<0)return false;
 if(hold==origin&&lengthSq(nodes[size_t(hold)].p-start)<.1225f){holding=true;return true;}
 int step=hold;while(parent[size_t(step)]!=origin&&step!=origin)step=parent[size_t(step)];
 // The corner rule the planner keeps: reach the origin node first when the diagonal from its feet would clip a jamb.
 next=lengthSq(nodes[size_t(origin)].p-start)>.0025f&&!roughSegment(w,start,startZ,nodes[size_t(step)].p,height,rise+.025f,nullptr,true,radius)&&roughSegment(w,start,startZ,nodes[size_t(origin)].p,height,rise+.025f,nullptr,true,radius)?nodes[size_t(origin)].p:lineUp(w,start,startZ,nodes[size_t(step)].p,height,radius,rise);
 return true;
}
// One terrain cell-grid edge: the walk from cell a's centre, standing on its
// support, to neighbour b's centre. With a kept grid each edge is probed once
// per collision state; verify mode re-probes every kept answer.
static bool cellEdge(const World&w,ai::RouteCache::Grid*grid,int a,int b,float height,float stepHeight,bool sweep,float radius=.20f){
 auto probe=[&]{Vec2 p{a%24+.5f,a/24+.5f},q{b%24+.5f,b/24+.5f};return roughSegment(w,p,navSupport(w,p,float(World::TerrainMaxZ+1),radius),q,height,stepHeight,nullptr,sweep,radius);};
 if(!grid)return probe();
 auto&edge=grid->edges[size_t(a*4+(b==a+1?0:b==a-1?1:b==a+24?2:3))];
 if(edge.state==ai::RouteCache::Unknown)edge.state=probe()?ai::RouteCache::Clear:ai::RouteCache::Blocked;
 else if(ai::RouteCache::verify()&&uncounted(probe)!=(edge.state==ai::RouteCache::Clear))perf::count(perf::RouteCacheMismatches);
 return edge.state==ai::RouteCache::Clear;
}
bool aiWalkSegment(const World&w,Vec2 start,float feet,Vec2 goal,float height,float*endFeet){return roughSegment(w,start,feet,goal,height,w.hasTerrain()?.34f:.24f,endFeet,true);}
// The terrain fallback: flood from the goal cell over the whole grid, then
// take the start's best neighbour. Its edges are aiWalkSegment's walk.
static Vec2 terrainWaypoint(const World&w,ai::RouteCache::Grid*grid,Vec2 start,Vec2 goal,float height){
 std::array<int,24*24> field;field.fill(9999);std::queue<int> queue;
 int gx=std::clamp(int(goal.x),0,23),gy=std::clamp(int(goal.y),0,23);field[gy*24+gx]=0;queue.push(gy*24+gx);
 auto edge=[&](int a,int b){return cellEdge(w,grid,a,b,height,.34f,true);};
 while(!queue.empty()){int cell=queue.front();queue.pop();int x=cell%24,y=cell/24;
  for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<0||ny<0||nx>23||ny>23)continue;int next=ny*24+nx;
   if(field[next]==9999&&edge(next,cell)){field[next]=field[cell]+1;queue.push(next);}}
 }
 int x=std::clamp(int(start.x),0,23),y=std::clamp(int(start.y),0,23),best=field[y*24+x];Vec2 result=start;
 for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<0||ny<0||nx>23||ny>23)continue;int next=ny*24+nx;
  if(field[next]<best&&edge(y*24+x,next)){best=field[next];result={nx+.5f,ny+.5f};}}
 return result;
}
Vec2 aiWalkWaypoint(const World&w,Vec2 start,float feet,Vec2 goal,float goalFeet,float height){
 if(aiWalkSegment(w,start,feet,goal,height))return goal;
 if(!w.hasTerrain())return stackedWaypoint(w,start,feet,goal,goalFeet,height,true);
 bool fresh=false;auto*grid=ai::RouteCache::grid(w,height,.34f,true,fresh);Vec2 result=terrainWaypoint(w,grid,start,goal,height);
 if(grid&&ai::RouteCache::verify()&&!sameBits(result,uncounted([&]{return terrainWaypoint(w,nullptr,start,goal,height);})))perf::count(perf::RouteCacheMismatches);
 return result;
}
Vec2 aiDeckWaypoint(const World&w,Vec2 start,float startZ,Vec2 goal,float goalZ,float height,bool human){return stackedWaypoint(w,start,startZ,goal,goalZ,height,human);}
bool aiCellEdge(const World&w,int from,int to,float height,float stepHeight,bool sweep){
 bool fresh=false;return cellEdge(w,ai::RouteCache::grid(w,height,stepHeight,sweep,fresh),from,to,height,stepHeight,sweep);
}
// Of every player (players()), the one this creature goes for. One player: that one, nothing weighed. Several:
// each living one is scored by what the creature makes of them through its own senses: in sight now (aiCanSee,
// a locker hides) +40, the one who wounded it in the last 6 s +25, near where it last knew someone +12, a metre
// nearer +1, and -8 for each other aware creature close by already on them (a pack spreads). Its current prey
// is kept unless another scores 6 more, and for a second after any change.
const Game::PlayerView& Game::creatureTarget(Enemy& e,bool huntsPlayer){
 const auto& list=players();
 if(list.size()==1){e.target=list.front().id;return list.front();}
 const auto& traits=creatureTraits(e.kind);const Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
 const PlayerView* current=nullptr;if(e.targetSince>-98)for(const auto& p:list)if(p.id==e.target&&p.alive())current=&p;   // (none before its first choice)
 const PlayerView* best=nullptr;float bestScore=-1e9f,currentScore=-1e9f;
 for(const auto& p:list){if(!p.alive())continue;const Player& b=*p.body;
  const float dz=b.z-e.z,d=std::sqrt(lengthSq(b.pos-e.pos)+dz*dz);
  float score=-d;
  if(huntsPlayer&&!playerHidden(p)&&aiCanSee(m_world,e.pos,e.z+.7f,facing,b.pos,b.z+b.eye,traits.sightRange,traits.sightDot,2.5f))score+=40;
  if(p.id==e.aggro&&m_elapsed-e.aggroTime<6)score+=25;
  if(e.awareness>0&&lengthSq(e.lastKnown-b.pos)<9)score+=12;
  for(const auto& o:m_enemies)if(&o!=&e&&o.alive&&o.targetSince>-98&&o.target==p.id&&lengthSq(o.pos-e.pos)<144)score-=8;
  if(&p==current)currentScore=score;
  if(score>bestScore){bestScore=score;best=&p;}
 }
 if(!best)return current?*current:list.front();
 if(current&&best!=current&&(bestScore<currentScore+6||m_elapsed-e.targetSince<1))best=current;
 if(best->id!=e.target||e.targetSince<-98){e.target=best->id;e.targetSince=m_elapsed;}
 return *best;
}
const Game::PlayerView& Game::creaturePrey(const Enemy& e)const{const auto* p=playerById(e.target);return p?*p:players().front();}
void Game::updateEnemies(float dt){
 perf::Scope frameProfile(perf::Enemies);
 // A co-op guest: the host's creatures think; here they are only posed from its snapshots (Match).
 if(m_netCreaturesRemote){for(auto&e:m_enemies)advanceCreatureAnimation(e,m_elapsed,dt);return;}
 // Nobody left alive to hunt (single player: the player is dead): the creatures stand.
 if(m_won)return;
 {bool anyone=false;for(const auto& p:players())anyone|=p.alive();if(!anyone)return;}
 ai::RouteCache::Owner routeOwner(m_world,m_world.m_mapData);
 auto stimuli=aiStimuli();hearPlayerNoise(stimuli);
 const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
 for(auto&e:m_enemies){
  perf::CreatureScope creatureProfile(int(e.kind));
  if(dormantEntity(e.z))continue;
  struct Animate {Enemy&e;float time,dt;~Animate(){advanceCreatureAnimation(e,time,dt);}} animate{e,m_elapsed,dt};
  // An authored spawn/saved foot starts at the centre's terrain height. Seat
  // its complete footprint on a legal step before testing movement, otherwise
  // a wider body can begin with one corner below a sloping floor and never fit.
  float settle=e.alive&&e.surfaceMode==0&&e.verticalVelocity<=0?(m_world.hasTerrain()?roughStep(e.kind):.215f):.01f;
  float support=groundHeight(e.pos,e.z+settle,e.radius());
  if(e.surfaceMode==0){if(e.z>support+.005f){e.verticalVelocity-=14.f*dt;e.z=std::max(support,e.z+e.verticalVelocity*dt);}else{e.z=support;e.verticalVelocity=0;}}
  if(e.alive&&e.surfaceMode==0&&e.searchTime>.25f&&!hullFits(e.pos,e.z,e.hull(),e.radius(),false,false)){
   // Recover an old/authored pose whose centre is clear but whose body overlaps
   // a jamb or terrain obstacle. The centre ray prevents crossing a real wall.
   bool recovered=false;float step=m_world.hasTerrain()?roughStep(e.kind):.215f;
   for(float radius=.05f;radius<=.6f&&!recovered;radius+=.05f)for(int side=0;side<16;++side){
    float angle=side*kTwoPi/16;Vec2 p=e.pos+Vec2{std::cos(angle),std::sin(angle)}*radius;float z=groundHeight(p,e.z+step,e.radius());
    if(std::fabs(z-e.z)>step+.025f||!hullFits(p,z,e.hull(),e.radius(),false,false)||!m_world.rayClear(e.pos,e.z+.6f,p,z+.6f))continue;
    e.pos=p;e.z=z;e.verticalVelocity=0;e.repathTimer=0;recovered=true;break;
   }
   e.searchTime=0;
  }
  if(!e.alive){clearCreatureSchedule(e);continue;}
  // The console's freeze and `ai off` (Summon.cpp): it stands where it is.
  if(m_aiOff||e.frozen){e.moving=false;e.windup=0;continue;}
  // A hunter still coming through the seam after the player.
  if(e.arrive>0){hunterArrive(e,dt);continue;}
  perf::count(perf::CreatureUpdates);
  const auto& traits=creatureTraits(e.kind);
  // A footfall each half gait cycle (two to a move-clip cycle), wherever this
  // frame's move ends: the floor it treads at its kind's weight, a limp's
  // dragged foot lighter. The player's ears only: never an AI stimulus.
  struct Footfall {Game&g;Enemy&e;float stride;~Footfall(){const auto&t=creatureTraits(e.kind);const float now=std::floor(e.gait/kPi);if(now==stride||t.stepGain<=0||!e.alive)return;
   const unsigned n=unsigned(now)&3u;const bool drag=t.stepLimp>0&&(n&1u);
   SoundEvent step{Sound(int(stepSoundAt(g.m_world,e.pos))+int(n)),e.pos,t.stepGain*(drag?t.stepLimp:1.f),t.stepPitch*(drag?.86f:.97f+.02f*float(n)),true,17,e.z+.04f};step.aiRadius=-1;g.m_sounds.push_back(step);}} footfall{*this,e,std::floor(e.gait/kPi)};
  // The cursed equipment: asleep, starting up or seized by live current, it does nothing else (Machine.cpp).
  if(traits.dormant&&machineUpdate(e,stimuli,dt))continue;
  // A dormant creature does nothing until its state is set or it is hurt; then
  // it knows where the player is (has them made out) and calls out.
  if(e.wakeState){if(!state(e.wakeState)&&e.hp>=e.maxHp){e.moving=false;continue;}
   // A stalker woken by distant noise keeps coming as long as one that saw the player.
   e.wakeState=0;e.awareness=std::max(e.awareness,traits.rushSpeed>0?12.f:6.f);e.recognition=1;{const Player& near=*nearestPlayer(e.pos,e.z).body;e.lastKnown=near.pos;e.lastKnownZ=near.z;}e.state=Enemy::State::Investigate;enemySound(e,0,.9f);if(e.hunter||e.prey>=0)wakeHunter(e);}
  // Down on a kill it sees and hears little: Hunter.cpp ends the meal.
  if(e.feeding>0&&updateFeeding(e,dt))continue;
  bool hadAwareness=e.awareness>0;e.repathTimer-=dt;e.moving=false;e.strike=std::max(0.f,e.strike-dt*(1.f/traits.strikeSeconds));e.attackCooldown=std::max(0.f,e.attackCooldown-dt);e.painFlash=std::max(0.f,e.painFlash-dt*traits.painDecay);
  perf::Scope senseProfile(perf::EnemySense);
  // Who it fights is its faction's business (factionsHostile): an authored
  // creature (a Creature) hunts the player and every squad; a summoned ally
  // leaves the player be. One on a guard order (Summon.cpp) keeps to its post.
  const bool huntsPlayer=factionsHostile(e.faction,AiFaction::Player);
  // Which player, of every one there is (creatureTarget): single player, the player.
  const PlayerView& prey=creatureTarget(e,huntsPlayer);const Player& P=*prey.body;
  Vec2 target=P.pos;float targetFeet=P.z,targetEye=P.eye,targetHull=P.hullHeight();int friendly=-1,rival=-1;
  const bool posted=e.summon&&e.order==3;
  auto leashed=[&](Vec2 p){return !posted||lengthSq(p-e.anchor)<64.f;};
  auto to=target-e.pos;float dist=length(to);Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
  // Shut in a locker the player is out of sight of anything that hunts by eye.
  const bool seen=huntsPlayer&&leashed(P.pos)&&!playerHidden(prey)&&aiCanSee(m_world,e.pos,e.z+.7f,facing,target,targetFeet+targetEye,traits.sightRange,traits.sightDot,2.5f);
  // Making the player out, as a guard does (aiRecognitionRate) at its own
  // notice time and need of light. Inside 2.5 m it knows them at once, and a
  // wound the player dealt fills it where it was dealt (Hitscan, the punch, a
  // thrown prop); a wound from anyone else does not.
  // Below a full meter it has only seen something: it turns and creeps toward
  // it (Investigate), and a stalker's stare-down waits for the meter to fill.
  // Out of sight the meter drains at half speed, but not while it still hunts
  // what it made out. The meter is the player's alone (a chase after an NPC
  // does not fill it), and a save keeps it.
  const bool noticing=e.recognition>0&&e.recognition<1;
  if(seen){float dz=targetFeet+targetEye-e.z-.7f;
   e.recognition=dist<2.5f||traits.noticeMs<=0?1.f:std::min(1.f,e.recognition+dt*1000.f/float(traits.noticeMs)*aiRecognitionRate(std::sqrt(dist*dist+dz*dz),traits.sightRange,dist>.001f?dot(to*(1.f/dist),facing):1.f,traits.sightDot,P.crouched,length(playerVelocity(prey)),playerLit(prey),traits.lightWeight));}
  else if(e.recognition<1||e.awareness<=0)e.recognition=traits.noticeMs>0?std::max(0.f,e.recognition-dt*500.f/float(traits.noticeMs)):0.f;
  bool visible=seen&&e.recognition>=1,glimpsed=seen&&!visible;
  float nearest=visible?dist:traits.sightRange;
  // The man it came out for comes first, wherever he is on its floor plan.
  if(const int quarry=hunterPrey(e);quarry>=0){const auto&npc=m_friendlyActors[size_t(quarry)];target=npc.pose.position;targetFeet=npc.pose.z;targetHull=npc.track().scale;targetEye=npc.track().scale*.65f;friendly=quarry;nearest=dist=length(target-e.pos);visible=true;glimpsed=false;to=target-e.pos;}
  else for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];if(npc.level!=m_level)continue;const auto&track=npc.track();const auto&pose=npc.pose;
   if(npcIncapacitated(i)||track.ai.mode==ActorAiMode::Scripted||!track.health||state(track.deadState)||pose.clip==4||(track.ai.enableState&&!state(track.ai.enableState))||!factionsHostile(e.faction,npcFaction(track))||!leashed(pose.position))continue;
   float distance=length(pose.position-e.pos);if(distance<nearest&&aiCanSee(m_world,e.pos,e.z+.7f,facing,pose.position,pose.z+track.scale*.65f,traits.sightRange,traits.sightDot,2.5f)){
    target=pose.position;targetFeet=pose.z;targetHull=track.scale;targetEye=track.scale*.65f;friendly=int(i);nearest=dist=distance;visible=true;glimpsed=false;to=target-e.pos;
   }
  }
  // Another creature on the other side (a summoned ally against the zone's own,
  // or the other way round): nearer than anything else it sees, it is the prey.
  if(hunterPrey(e)<0)for(size_t i=0;i<m_enemies.size();++i){const auto&o=m_enemies[i];
   if(&o==&e||!o.alive||o.sleeping()||!factionsHostile(e.faction,o.faction)||!leashed(o.pos))continue;
   const float distance=length(o.pos-e.pos),mid=(o.bodyBottom()+o.bodyTop())*.5f;
   if(distance<nearest&&aiCanSee(m_world,e.pos,e.z+.7f,facing,o.pos,mid,traits.sightRange,traits.sightDot,2.5f)){
    target=o.pos;targetFeet=o.bodyBottom();targetHull=o.bodyTop()-o.bodyBottom();targetEye=targetHull*.65f;friendly=-1;rival=int(i);nearest=dist=distance;visible=true;glimpsed=false;to=target-e.pos;}
  }
  // Whatever it is striking now: the player, a man (friendly) or a creature (rival).
  const bool atPlayer=friendly<0&&rival<0;
  auto strikeRival=[&](float amount){auto&o=m_enemies[size_t(rival)];const float flash=m_hitFlash;
   const Point3 at{o.pos.x,o.pos.y,(o.bodyBottom()+o.bodyTop())*.5f};const Vec2 push=normalized(o.pos-e.pos);
   applyEnemyDamage(o,amount,true,false);m_hitFlash=flash; // the player's hit marker is the player's
   hitBurst(m_level,at,{push.x*.6f,push.y*.6f,.5f},ImpactMaterial::Flesh,int(o.kind),1.f,!o.alive);
   // It knows who did it, and goes for them.
   if(o.alive&&factionsHostile(o.faction,e.faction)){o.lastKnown=e.pos;o.lastKnownZ=e.z;o.awareness=std::max(o.awareness,6.f);if(o.state==Enemy::State::Idle||o.state==Enemy::State::Search)o.state=Enemy::State::Investigate;}};
  const AiStimulus* heard=nullptr;float strongest=0;
  // Keep footsteps (a sprint on steel carries 12 m)/impacts/weapons, but ignore
  // long-range world ambience that would steal lastKnown on Ashfall. Near-field sources are ignored so a
  // Brute cannot hear its own live footfall; soft Land impacts are ignored
  // entirely so updateEnemies-only callers cannot accumulate a trail of
  // historical Brute thumps into Investigate magnets.
  // Its kind's hearing scales how far each sound carries to it.
  if(!visible)for(const auto&stimulus:stimuli){
   if(stimulus.npc==-2-int(&e-m_enemies.data()))continue;   // its own blows on a door (DoorPlay.cpp)
   // Nor the door it stands beating: the leaf's rattle and slam come from where the quarry is not.
   if(battersDoors(e.kind)){bool atDoor=false;for(const auto&d:m_world.doors()){if(!d.breakable()||d.broken||!d.spans(e.pos,-.1f)||std::fabs(d.across(e.pos)-d.y)>1.6f)continue;
     const Vec2 c=d.centre();if(lengthSq(stimulus.source-c)<2.25f){atDoor=true;break;}}if(atDoor)continue;}
   bool combat=stimulus.categories&(ai::SoundWeapon|ai::SoundDanger|ai::SoundImpact);
   if(!combat&&!stimulus.loud&&!stimulus.step&&stimulus.radius>5.f)continue;
   if((stimulus.categories&ai::SoundImpact)&&!stimulus.loud&&stimulus.gain<=.75f)continue;
   const float radius=stimulus.radius*traits.hearingScale;
   float dz=stimulus.z-e.z-.7f;float planar=lengthSq(stimulus.source-e.pos);if(planar+dz*dz>=radius*radius||planar<1.f)continue;
   float strength=aiSoundStrength(e.pos,e.z+.7f,stimulus.source,stimulus.z,radius,stimulus.gain,stimulus.loud,hearingRayClear(e.pos,e.z+.7f,stimulus.source,stimulus.z));
   if(strength>strongest&&leashed(stimulus.source)){strongest=strength;heard=&stimulus;}}
  if(visible){e.lastKnown=target;e.lastKnownZ=targetFeet;e.awareness=std::max(e.awareness,traits.rushSpeed>0?12.f:6.f);e.state=Enemy::State::Chase;}
  // It has seen something there: it goes to look, as it would at a noise.
  else if(glimpsed){e.lastKnown=target;e.lastKnownZ=targetFeet;e.awareness=std::max(e.awareness,5.f);e.state=Enemy::State::Investigate;}
  else if(heard){e.lastKnown=heard->source;auto local=heard->source;const auto&sourceWorld=worldAt(local);e.lastKnownZ=sourceWorld.supportBelow(local.x,local.y,heard->z);e.awareness=std::max(e.awareness,5.f);e.state=Enemy::State::Investigate;}
  else {e.awareness=std::max(0.f,e.awareness-dt);if(e.awareness==0)e.state=Enemy::State::Idle;else if(length(e.lastKnown-e.pos)<.6f)e.state=Enemy::State::Search;}
  // A recurring hunter goes by ear between sightings: to the player's last
  // loud noise, however far, each time there is a new one.
  if(e.hunter&&huntsPlayer&&!visible&&!glimpsed&&!heard&&e.stalkMode!=Enemy::StalkMode::Retreat&&m_playerNoise.time>e.earTime){e.earTime=m_playerNoise.time;e.lastKnown=m_playerNoise.pos;e.lastKnownZ=m_playerNoise.z;e.awareness=std::max(e.awareness,6.f);e.state=Enemy::State::Investigate;e.repathTimer=0;}
  // A hunter with hands that knows which locker its prey is shut in goes to the
  // doors, and at them takes hold (LockerCapture.cpp).
  if(huntsPlayer)creatureLockerSense(e);
  senseProfile.stop();if(e.awareness>0)perf::count(perf::CreaturesHunting);
  if(creatureLockerHold(e,dt))continue;
  if(e.kind==Enemy::Kind::Huntsman&&e.surfaceMode){
   // The real surface overhead: a service soffit or deck, not just the map roof.
   float floor=m_world.floorHeight(e.pos.x,e.pos.y),ceiling=std::min(m_world.clearanceHeight(e.pos.x,e.pos.y),m_world.clearanceAbove(e.pos.x,e.pos.y,std::max(e.z,floor)+.05f));
   if(e.awareness<=0||m_world.outdoors()||ceiling-floor<2.15f||dist<2.2f){e.surfaceMode=0;e.verticalVelocity=0;}
   else if(e.surfaceMode==1){
    e.z=std::min(ceiling-1.05f,e.z+dt*2.1f);e.moving=true;e.gait+=dt*9.f;
    if(e.z>=ceiling-1.06f)e.surfaceMode=2;
    continue;
   }else{
    Vec2 toward=normalized(e.lastKnown-e.pos),next=e.pos+toward*(dt*1.6f);
    float roof=std::min(m_world.clearanceHeight(next.x,next.y),m_world.clearanceAbove(next.x,next.y,e.z+.05f)),nextZ=roof-1.05f;
    if(roof-m_world.floorHeight(next.x,next.y)>2.15f&&std::fabs(nextZ-e.z)<.4f&&navFits(m_world,next,nextZ,1.05f)){
     e.pos=next;e.z=nextZ;e.heading=std::atan2(toward.y,toward.x);e.moving=true;e.gait+=dt*9.f;
    }else e.surfaceMode=0;
    continue;
   }
  }
  e.voiceTimer-=dt;e.idleTimer-=dt;
  if(e.awareness>0&&e.voiceTimer<=0){enemySound(e,0,e.kind==Enemy::Kind::Warden?.28f:.65f);e.voiceTimer=(e.kind==Enemy::Kind::Warden?11.f:6.f)+float(int(e.home.x)%4);}
  // Unaware, it is still heard: its idle sound at an uneven spacing (.75-1.25
  // of its period), to the player's ears only, on its own clock so the call
  // when it notices the player is never held back by it.
  if(e.awareness<=0&&traits.idlePeriod>0&&e.idleTimer<=0){SoundEvent idle{traits.idleSound,e.pos,traits.idleGain,traits.idlePitch,true,14,(e.bodyBottom()+e.bodyTop())*.5f};idle.aiRadius=-1;m_sounds.push_back(idle);
   e.idleTimer=traits.idlePeriod*(.75f+.125f*float(int(e.home.x*7.f+e.home.y*13.f+m_elapsed*5.f)%5));}
  // Stalkers (the reactor's thing, the Snatcher) hunt by sight: stare, flank, rush, hide.
  const bool stalker=traits.rushSpeed>0;
  // Hurt past its threshold a recurring hunter only runs (Hunter.cpp).
  const bool huntedFlight=e.hunter&&e.stalkMode==Enemy::StalkMode::Retreat;bool fleeing=huntedFlight;
  // A summoned neutral runs from what fights all comers and from whoever hurt it (Summon.cpp).
  Vec2 fleeGoal{};float fleeZ=0;if(e.summon&&!fleeing)fleeing=summonFlight(e,fleeGoal,fleeZ,dt);
  float range=e.reach();
  bool sameLevel=targetFeet<e.bodyTop()&&targetFeet+targetHull>e.bodyBottom();
  // The Snatcher crouches to lunge from further out than its claws reach.
  bool canMelee=!fleeing&&visible&&sameLevel&&dist<(e.kind==Enemy::Kind::Snatcher?2.8f*e.scale:e.kind==Enemy::Kind::Transformed?3.2f*e.scale:range)&&e.attackCooldown<=0&&e.strike<=0;
  bool heardCue=heard!=nullptr;
  bool hurt=e.painFlash>(stalker?.85f:.65f);
  Vec2 goal=e.awareness>0?e.lastKnown:e.home;
  // Going home means its own floor, not the one it last saw the player on.
  float goalZ=e.awareness>0||e.homeZ<=-999?e.lastKnownZ:e.homeZ;
  // Nothing to go on, the cursed equipment goes to the nearest live lamp or live circuit.
  if(traits.dormant&&e.awareness<=0)machineAttraction(e,goal,goalZ);
  if(huntedFlight&&hunterFlight(e,goal,goalZ,dt))continue; // out of sight and away: gone
  if(fleeing&&!huntedFlight){goal=fleeGoal;goalZ=fleeZ;}
  bool wantsMove=fleeing||e.state!=Enemy::State::Search&&!(e.windup>0||e.strike>0)&&!canMelee&&length(goal-e.pos)>=.3f&&!(!stalker&&visible&&dist<range*.82f);
  int ms=std::max(1,int(std::round(dt*1000)));
  runCreatureSchedule(e,ms,visible,heardCue,canMelee||e.windup>0||e.strike>0,hurt,wantsMove);
  bool scheduleMove=fleeing||e.scheduleStep==1&&e.scheduleStatus==int(CreatureTaskStatus::Running);
  bool scheduleStrike=!fleeing&&(creatureTaskAllows(e,CreatureTask::Strike)||e.schedule==int(CreatureSchedule::Attack));
  const bool snatcher=e.kind==Enemy::Kind::Snatcher;
  if(e.kind==Enemy::Kind::Transformed){
   // Two moves, both readable. Out of arm's reach it leaps (Rush), and one
   // that lands pins its prey (Grab) and bites until the hold ends or a hit
   // knocks it off; a miss stumbles it (Recover). In arm's reach it raises
   // both fists for the overhead blow: the generic wind-up below, which a hit
   // during the raise breaks.
   constexpr float Leap=TransformedLeap,LeapSpeed=6.f,Hold=TransformedHold,Stumble=TransformedStumble,Bite=9.f;
   Vec2 aim{std::cos(e.heading),std::sin(e.heading)};
   const bool struck=e.painFlash>.85f;
   auto release=[&](float seconds){e.windup=0;e.strike=0;e.stalkMode=Enemy::StalkMode::Recover;e.stalkTimer=seconds;e.attackCooldown=std::max(e.attackCooldown,traits.cooldown);if(atPlayer&&prey.local)m_grabbed=0;};
   if(e.stalkMode==Enemy::StalkMode::Recover){
    e.stalkTimer=std::max(0.f,e.stalkTimer-dt);if(e.stalkTimer<=0)e.stalkMode=Enemy::StalkMode::Watch;
    continue;
   }
   if(e.stalkMode==Enemy::StalkMode::Rush){
    float travel=std::min(dt,e.stalkTimer);e.stalkTimer=std::max(0.f,e.stalkTimer-dt);
    int samples=std::max(1,int(std::ceil(LeapSpeed*travel/.08f)));bool blocked=false,caught=false;
    for(int i=0;i<samples&&!blocked&&!caught;++i){
     Vec2 next=e.pos+aim*(LeapSpeed*travel/samples);float feet=e.z;
     if(length(target-next)<.65f)break;
     if(aiWalkSegment(m_world,e.pos,e.z,next,e.hull(),&feet)&&std::fabs(feet-e.z)<=traits.step+.025f){float moved=length(next-e.pos);e.pos=next;e.z=feet;e.moving=moved>.0001f;}
     else blocked=true;
     Vec2 reachTo=target-e.pos;
     caught=sameLevel&&length(reachTo)<range&&dot(normalized(reachTo),aim)>.7f&&m_world.rayClear(e.pos,e.z+.6f,target,targetFeet+.5f);
    }
    if(caught){e.stalkMode=Enemy::StalkMode::Grab;e.stalkTimer=Hold;e.attackCooldown=traits.cooldown;enemySound(e,1,.95f,1.05f);}
    else if(blocked||e.stalkTimer<=0)release(Stumble);
    continue;
   }
   if(e.stalkMode==Enemy::StalkMode::Grab){
    if(struck){release(Stumble);continue;} // shoot or punch it off
    float before=e.stalkTimer;e.stalkTimer=std::max(0.f,e.stalkTimer-dt);
    // A bite as the hold opens and two more while it lasts, on whoever is
    // still in its hands.
    bool bite=false;for(float at:{Hold-.05f,Hold*.55f,Hold*.15f})bite=bite||(before>at&&e.stalkTimer<=at);
    const bool holding=sameLevel&&dist<range+.35f;
    if(bite&&holding){if(friendly>=0)damageFriendly(size_t(friendly),Bite,e.pos,{ShotKind::Enemy,m_level,int(&e-m_enemies.data())});else if(rival>=0)strikeRival(Bite);else hurtPlayer(prey,Bite,e.pos);}
    if(atPlayer&&prey.local&&holding)m_grabbed=std::max(m_grabbed,std::min(e.stalkTimer,.25f));
    if(lengthSq(to)>.01f)e.heading=std::atan2(to.y,to.x);
    if(e.stalkTimer<=0||!holding){e.stalkMode=Enemy::StalkMode::Watch;e.attackCooldown=std::max(e.attackCooldown,traits.cooldown*.6f);}
    continue;
   }
   if(e.windup>0&&struck){e.windup=0;e.attackCooldown=std::max(e.attackCooldown,.8f);continue;}
   // Out of arm's reach and facing its prey: leap.
   if(e.windup<=0&&e.strike<=0&&scheduleStrike&&visible&&sameLevel&&dist>=range&&dist<3.2f*e.scale&&e.attackCooldown<=0&&dot(normalized(to),aim)>.8f){
    e.heading=std::atan2(to.y,to.x);e.stalkMode=Enemy::StalkMode::Rush;e.stalkTimer=Leap;enemySound(e,1,.9f,1.15f);continue;
   }
  }
  if(e.windup>0&&!snatcher){if(!stalker&&e.kind!=Enemy::Kind::Brute&&e.windup>.09f){e.heading+=wrapAngle(std::atan2(to.y,to.x)-e.heading)*std::min(1.f,dt*16.f);facing={std::cos(e.heading),std::sin(e.heading)};}e.windup-=dt;if(e.windup<=0){e.strike=1;e.attackCooldown=traits.cooldown;
    if(dist<range+.1f&&dot(normalized(to),facing)>(stalker?.5f:.25f)&&sameLevel&&m_world.rayClear(e.pos,e.z+.6f,target,targetFeet+.5f)){
     float damage=creatureTraits(e.kind).damage;
     // The man it came out for does not get up from it.
     if(friendly>=0&&friendly==hunterPrey(e))damage=1e4f;
     if(friendly>=0){damageFriendly(size_t(friendly),damage,e.pos,{ShotKind::Enemy,m_level,int(&e-m_enemies.data())});
      const auto&track=m_friendlyActors[size_t(friendly)].track();if(traits.feedSeconds>0&&track.deadState&&state(track.deadState))biteKill(e,size_t(friendly));}
     else if(rival>=0)strikeRival(damage);
     else hurtPlayer(prey,damage,e.pos);
    }
   }continue;
  }
  // Close-range committed swing: the player can backstep or circle behind it.
   if(stalker&&!snatcher&&scheduleStrike&&visible&&sameLevel&&dist<range&&e.attackCooldown<=0&&e.strike<=0&&
      (e.kind!=Enemy::Kind::Chimera||dot(normalized(to),facing)>.55f)){
   e.heading=std::atan2(to.y,to.x);e.windup=traits.windup;enemySound(e,1,.9f);continue;
  }
  if(e.awareness>0&&creatureTraits(e.kind).opensDoors)requestAiDoor(m_world,m_level,e.pos,e.z,goal);
  // A Brute, a Chimera or the Machine at a door that gives stands and beats it (DoorPlay.cpp).
  if(e.awareness>0&&battersDoors(e.kind)&&batterDoor(e,goal)){enemySound(e,1,.85f);continue;}
  // Its pace scales every stalking speed and shortens its stare-downs.
  const float pace=traits.rushSpeed/3.8f;
  float stalkSpeed=traits.speed;bool watching=false;
  // What a stalker knows about the player's eyes. "Observed" is a full 3D gaze
  // test: looking at the floor beneath it, or over a railing above it, does
  // not count as having it in view.
  perf::Scope stalkProfile(perf::EnemyStalk);
  Vec2 playerForward{std::cos(P.angle),std::sin(P.angle)};
  Vec2 playerToWarden=e.pos-P.pos;float playerToWardenDistance=length(playerToWarden);
  bool observed=false;
  if(stalker&&atPlayer){
   bool playerLineOfSight=playerToWardenDistance<12.f&&m_world.rayClear(P.pos,P.z+P.eye,e.pos,e.z+.85f);
   float gazePitch=P.pitch/140.f,gazeDz=(e.z+.9f)-(P.z+P.eye),gazeDistance=std::sqrt(playerToWardenDistance*playerToWardenDistance+gazeDz*gazeDz);
   float gazeDot=gazeDistance>.001f?(playerForward.x*std::cos(gazePitch)*playerToWarden.x+playerForward.y*std::cos(gazePitch)*playerToWarden.y+std::sin(gazePitch)*gazeDz)/gazeDistance:1.f;
   observed=playerLineOfSight&&gazeDot>.72f;
  }
  auto standable=[&](Vec2 c,float& z){z=navSupport(m_world,c,m_world.hasTerrain()?float(World::TerrainMaxZ+1):e.z+.215f);
   bool reachable=m_world.hasTerrain()?roughSegment(m_world,e.pos,e.z,c,e.hull(),roughStep(e.kind),nullptr,true,e.radius()):std::fabs(z-e.z)<.23f;
   return reachable&&navFits(m_world,c,z,e.hull(),e.radius());};
  // The nearest spot round about that the player cannot see.
  auto hidingSpot=[&](Vec2 fallback){Vec2 best=fallback;float bestScore=1e9f;
   for(int i=0;i<8;++i){float a=float(i)*kPi*.25f;Vec2 c=e.pos+Vec2{std::cos(a),std::sin(a)}*3.2f;float z=0;if(!standable(c,z))continue;
    bool seen=m_world.rayClear(P.pos,P.z+P.eye,c,z+.9f);
    float score=(seen?10.f:0.f)-lengthSq(c-P.pos)*.05f;
    if(score<bestScore){bestScore=score;best=c;}}
   return best;};
  // A point beside the player on the side it is least likely to be looking at.
  auto flankPoint=[&](float out,float across,Vec2 fallback){
   auto radial=normalized(e.pos-P.pos);auto side=Vec2{-radial.y,radial.x};
   Vec2 best=fallback;float bestScore=9999.f;
   for(float sign:{e.stalkSide,-e.stalkSide}){auto candidate=P.pos+radial*out+side*(sign*across);float z=0;if(!standable(candidate,z))continue;
    auto fromPlayer=candidate-P.pos;float candidateDistance=length(fromPlayer);
    float gaze=candidateDistance>.001f?dot(playerForward,fromPlayer*(1.f/candidateDistance)):1.f;
    float score=gaze+lengthSq(candidate-e.pos)*.02f;
    if(score<bestScore){bestScore=score;best=candidate;}
   }
   return best;};
  if(snatcher){
   // An ambusher, not a runner. It limps up out of sight, slips away from a
   // player who watches it, and hunts with one readable move: a hissing crouch,
   // a straight lunge it cannot steer, a grab that hauls its prey a step, then
   // a fade back into cover. A miss, or a hit during the tell, sprawls it.
   constexpr float Tell=.65f,Lunge=.45f,Hold=.55f,Sprawl=1.25f,Fade=2.5f,Strike=2.8f;
   Vec2 aim{std::cos(e.heading),std::sin(e.heading)};
   bool struck=e.painFlash>.85f;
   auto sprawl=[&](float seconds){e.fades=0;e.windup=0;e.strike=0;e.stalkMode=Enemy::StalkMode::Recover;e.stalkTimer=seconds;e.attackCooldown=std::max(e.attackCooldown,traits.cooldown);if(atPlayer&&prey.local)m_grabbed=0;};
   if(e.stalkMode==Enemy::StalkMode::Recover){
    e.stalkTimer=std::max(0.f,e.stalkTimer-dt);e.windup=0;e.strike=0;
    e.painFlash=.84f*e.stalkTimer/Sprawl; // plays its stagger through
    if(e.stalkTimer<=0){e.stalkMode=Enemy::StalkMode::Retreat;e.stalkTimer=Fade;}
    continue;
   }
   if(e.windup>0){ // the tell
    if(struck){sprawl(1.f);continue;}
    // It tracks its prey through the crouch, then commits to a line.
    if(e.windup>.25f)e.heading+=wrapAngle(std::atan2(to.y,to.x)-e.heading)*std::min(1.f,dt*10.f);
    e.windup=std::max(0.f,e.windup-dt);
    if(e.windup==0){e.stalkMode=Enemy::StalkMode::Rush;e.stalkTimer=Lunge;e.strike=1;}
    continue;
   }
   if(e.stalkMode==Enemy::StalkMode::Rush){ // the lunge
    float travel=std::min(dt,e.stalkTimer);e.stalkTimer=std::max(0.f,e.stalkTimer-dt);
    e.strike=1.f-.25f*(1.f-e.stalkTimer/Lunge);
    int samples=std::max(1,int(std::ceil(traits.rushSpeed*travel/.08f)));bool blocked=false,caught=false;
    for(int i=0;i<samples&&!blocked&&!caught;++i){
     Vec2 next=e.pos+aim*(traits.rushSpeed*travel/samples);float feet=e.z;
     if(length(target-next)<.7f)break; // it does not run through its prey
     // Swept with the same walk authority as everyone else: no phasing through
     // closed doors, railings, low ceilings, cliffs or stair corners.
     if(aiWalkSegment(m_world,e.pos,e.z,next,e.hull(),&feet)&&std::fabs(feet-e.z)<=traits.step+.025f){
      float moved=length(next-e.pos);e.pos=next;e.z=feet;e.moving=moved>.0001f;e.gait+=moved*traits.gaitPerMetre;
     }else blocked=true;
     Vec2 reachTo=target-e.pos;float reachDistance=length(reachTo);
     bool overlap=targetFeet<e.bodyTop()&&targetFeet+targetHull>e.bodyBottom();
     caught=overlap&&reachDistance<range&&dot(normalized(reachTo),aim)>.72f&&m_world.rayClear(e.pos,e.z+.6f,target,targetFeet+.5f);
    }
    if(caught){
     e.stalkMode=Enemy::StalkMode::Grab;e.stalkTimer=Hold;e.attackCooldown=traits.cooldown;enemySound(e,1,.95f,.82f);
     if(friendly>=0)damageFriendly(size_t(friendly),traits.damage,e.pos,{ShotKind::Enemy,m_level,int(&e-m_enemies.data())});
     else if(rival>=0)strikeRival(traits.damage);
     else if(hurtPlayer(prey,traits.damage,e.pos)){if(prey.local)m_grabbed=Hold;}else sprawl(Sprawl);   // a held crate takes the lunge: nothing to hold on to
    }else if(blocked||e.stalkTimer<=0)sprawl(Sprawl);
    continue;
   }
   if(e.stalkMode==Enemy::StalkMode::Grab){
    if(struck){sprawl(1.f);continue;} // shoot or punch it off
    e.stalkTimer=std::max(0.f,e.stalkTimer-dt);e.strike=.75f*e.stalkTimer/Hold;
    // It backs off with its prey in its hands. The haul stops at a wall and
    // never drags the player off a ledge or up a step.
    Vec2 back=e.pos-aim*(1.6f*dt);float feet=e.z;
    if(aiWalkSegment(m_world,e.pos,e.z,back,e.hull(),&feet)&&std::fabs(feet-e.z)<=traits.step+.025f){e.pos=back;e.z=feet;}
    // Only the local player's body is hauled (a guest's movement is its own machine's).
    if(atPlayer&&prey.local){m_grabbed=std::max(m_grabbed,e.stalkTimer);
     Vec2 haul=e.pos+aim*.95f-P.pos;float pull=std::min(length(haul),2.5f*dt);
     if(pull>.001f){Vec2 step=normalized(haul)*pull;if(std::fabs(groundHeight(P.pos+step,P.z+.215f)-P.z)<=.215f)tryMove(step);}}
    if(e.stalkTimer<=0){e.strike=0;e.stalkMode=Enemy::StalkMode::Retreat;e.stalkTimer=Fade;}
    continue;
   }
   e.stalkTimer=std::max(0.f,e.stalkTimer-dt);e.strike=0;
   // Close enough to snatch: hiss and crouch. Fading, it only turns on a
   // player who follows it right into its corner.
   bool fading=e.stalkMode==Enemy::StalkMode::Retreat;
   if(scheduleStrike&&visible&&sameLevel&&dist<(fading?1.8f:Strike)*e.scale&&e.attackCooldown<=0){
    e.heading=std::atan2(to.y,to.x);e.windup=traits.windup;e.stalkMode=Enemy::StalkMode::Watch;enemySound(e,1,.9f,1.2f);continue;
   }
   if(atPlayer&&e.awareness>0){
    // Seen from a distance, it does not come on: it slips out of sight and
    // comes again from somewhere the player is not looking. Twice; a player
    // who keeps staring it down the third time gets it coming anyway. It picks
    // its cover once, when it is seen, and stays hidden a while before it moves.
    const bool spotted=observed&&dist>4.5f&&e.fades<2;
    auto fade=[&]{e.stalkMode=Enemy::StalkMode::Retreat;e.stalkTimer=2.f;fading=true;++e.fades;e.hideAt=hidingSpot(e.pos);e.hideSet=true;};
    if(fading&&e.stalkTimer<=0){e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=2.6f;fading=false;e.hideSet=false;}
    if(!fading&&spotted&&e.stalkMode!=Enemy::StalkMode::Watch)fade();
    if(e.stalkMode==Enemy::StalkMode::Watch){
     if(spotted&&e.stalkTimer<1.8f)fade();
     else if(e.stalkTimer<=0){e.stalkMode=Enemy::StalkMode::Flank;e.stalkSide=-e.stalkSide;}
    }
    if(fading&&!e.hideSet){e.hideAt=hidingSpot(e.pos);e.hideSet=true;} // a load or a hurt flight
    // Nowhere to hide: it does not stand frozen in the open, it comes on.
    if(fading&&lengthSq(e.hideAt-e.pos)<.01f){e.stalkMode=Enemy::StalkMode::Flank;fading=false;e.hideSet=false;e.fades=2;}
    if(fading)goal=e.hideAt;
    else if(e.stalkMode==Enemy::StalkMode::Flank&&visible&&dist>2.4f&&sameLevel)goal=flankPoint(2.2f,1.6f,goal);
    watching=watching||e.stalkMode==Enemy::StalkMode::Watch;
    stalkSpeed=fading?2.2f:traits.speed;
   }else stalkSpeed=traits.speed;
  }
  if(stalker&&!snatcher&&atPlayer&&!fleeing){
   e.stalkTimer=std::max(0.f,e.stalkTimer-dt);
   bool flat=std::fabs(e.z-P.z)<.23f;
   if(m_world.hasTerrain()&&playerToWardenDistance>.001f){Vec2 probe=e.pos+(P.pos-e.pos)*std::min(1.f,1.6f/playerToWardenDistance);flat=roughSegment(m_world,e.pos,e.z,probe,e.hull(),roughStep(e.kind),nullptr,true,e.radius());}
   // Fresh contact is the moment it makes the player out: unaware before, or
   // aware only of what it was still making out.
   bool freshContact=visible&&(!hadAwareness||noticing);

   // A rush is a short burst, never the Warden's permanent navigation speed.
   // Losing sight cannot leave it sprinting forever.
   if(e.stalkMode==Enemy::StalkMode::Rush&&e.stalkTimer<=0){
    e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=.85f;
   }

   // First contact is a stare-down. Even if the player's back is turned, the
   // Warden gets a stalking beat before it is allowed to charge.
   if(freshContact){
    e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=(observed?1.6f:.75f)/pace;
   }

   // Catch it trying to circle you and it stops rather than cartoonishly
   // continuing the flank in full view.
   if(observed&&e.stalkMode==Enemy::StalkMode::Flank&&dist>2.6f){
    // One that does not freeze under a gaze charges instead.
    if(traits.freezesWatched){e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=std::max(e.stalkTimer,.65f);}
    else{e.stalkMode=Enemy::StalkMode::Rush;e.stalkTimer=1.65f;enemySound(e,1,.65f,.9f);}
   }

   // Hurt badly, it breaks contact: slips to the nearest spot the player
   // cannot see, waits there, and comes back as a watcher.
   if(e.stalkMode==Enemy::StalkMode::Retreat){
    if(e.stalkTimer<=0){e.stalkMode=Enemy::StalkMode::Watch;e.stalkTimer=.75f;}
    else goal=hidingSpot(goal);
   }
   // Height no longer switches the stalk off: it watches from a gantry and
   // rushes down a stair; only the flank needs level ground.
   if(visible&&e.stalkMode!=Enemy::StalkMode::Retreat){
    bool emergencyRush=!freshContact&&(m_reloadTimer>0||e.painFlash>.8f||dist<1.8f);
    if(e.stalkMode!=Enemy::StalkMode::Rush&&emergencyRush){
     e.stalkMode=Enemy::StalkMode::Rush;e.stalkTimer=1.65f;enemySound(e,1,.65f,.9f);
    }else if(e.stalkMode==Enemy::StalkMode::Watch){
     if(traits.freezesWatched&&observed&&dist>2.2f)e.stalkTimer=std::max(e.stalkTimer,.25f);
     else if(e.stalkTimer<=0){e.stalkMode=Enemy::StalkMode::Flank;e.stalkTimer=2.25f;e.stalkSide=-e.stalkSide;}
    }else if(e.stalkMode==Enemy::StalkMode::Flank&&e.stalkTimer<=0){
     e.stalkMode=Enemy::StalkMode::Rush;e.stalkTimer=1.65f;enemySound(e,1,.65f,.9f);
    }

    // Flanking targets the least visible valid side of the player.
    if(e.stalkMode==Enemy::StalkMode::Flank&&dist>2.4f&&flat)goal=flankPoint(2.9f,2.3f,goal);
   }

   // Watching is deliberately unnerving: if the player has it in clear view,
   // it holds position. Looking away gives it permission to creep/flank.
   watching=traits.freezesWatched&&observed&&e.awareness>0&&e.stalkMode==Enemy::StalkMode::Watch&&dist>2.f&&dist<9.5f;
   stalkSpeed=e.kind==Enemy::Kind::Chimera?(e.stalkMode==Enemy::StalkMode::Rush?traits.rushSpeed:e.stalkMode==Enemy::StalkMode::Retreat?3.2f:e.stalkMode==Enemy::StalkMode::Flank?traits.speed:traits.speed*.7f):pace*(e.stalkMode==Enemy::StalkMode::Rush?3.8f:e.stalkMode==Enemy::StalkMode::Retreat?2.6f:e.stalkMode==Enemy::StalkMode::Flank?1.55f:.75f);

   // Search the last witnessed area; never sample the hidden player's new position.
   if(e.state==Enemy::State::Search&&e.stalkTimer<=0){auto offset=Vec2{std::cos(e.heading+e.stalkSide),std::sin(e.heading+e.stalkSide)}*1.3f;auto candidate=e.lastKnown+offset;float z=navSupport(m_world,candidate,m_world.hasTerrain()?float(World::TerrainMaxZ+1):e.lastKnownZ+.215f);
    bool reachable=m_world.hasTerrain()?roughSegment(m_world,e.pos,e.z,candidate,e.hull(),roughStep(e.kind),nullptr,true,e.radius()):std::fabs(z-e.lastKnownZ)<.23f;
    if(reachable&&navFits(m_world,candidate,z,e.hull(),e.radius())){e.lastKnown=candidate;e.lastKnownZ=z;goal=candidate;e.state=Enemy::State::Investigate;e.repathTimer=0;}e.stalkTimer=.6f;e.stalkSide=-e.stalkSide;
   }
  }
  stalkProfile.stop();
  if(e.state==Enemy::State::Search){e.heading+=dt*1.4f;continue;}
  auto look=visible&&!fleeing?to:goal-e.pos;
   if(lengthSq(look)>.01f&&(!scheduleMove||watching||canMelee))e.heading+=wrapAngle(std::atan2(look.y,look.x)-e.heading)*std::min(1.f,dt*(e.kind==Enemy::Kind::Brute?3.5f:14.f));
  if(scheduleStrike&&!stalker&&visible&&sameLevel&&dist<range&&e.attackCooldown<=0&&e.strike<=0){e.windup=creatureTraits(e.kind).windup;enemySound(e,1,.85f);continue;}
  if(watching||!scheduleMove||length(goal-e.pos)<.3f||(!fleeing&&!stalker&&visible&&dist<range*.82f)||e.painFlash>(stalker?.85f:.65f))continue;
  Vec2 destination=goal;
  float hull=e.hull(),radius=e.radius();
  float stepHeight=m_world.hasTerrain()?roughStep(e.kind):(e.kind==Enemy::Kind::Huntsman?.65f:.215f);
  bool direct=false;
  if(m_world.hasTerrain()){
   // Terrain-aware AI must not reject a hill or cavern slope merely because the target is several metres above us.
   // Test only the terrain immediately ahead; if a cliff blocks progress the
   // stuck timer falls back to routed navigation around it.
   auto delta=goal-e.pos;float distance=length(delta);Vec2 probe=distance>1.6f?e.pos+delta*(1.6f/distance):goal;
   direct=roughSegment(m_world,e.pos,e.z,probe,hull,stepHeight,nullptr,true,radius);
  }else{
   direct=roughSegment(m_world,e.pos,e.z,goal,hull,stepHeight,nullptr,true,radius);
  }
  if(e.kind==Enemy::Kind::Huntsman&&visible&&m_world.tile(int(goal.x),int(goal.y))=='C')direct=m_world.rayClear(e.pos,e.z+.7f,goal,targetFeet+.3f);
  // Repeated contact switches to routed movement instead of rebuilding a
  // path every frame or continuing to push into the same corner forever.
  if(e.searchTime>.2f)direct=false;
  // A creature that opens doors plans through the ones it may operate; it
  // still collides with them until requestAiDoor has swung them open.
  std::vector<char> operable;std::optional<World::DoorPlan> doorPlan;
  const bool keepWaypoint=!direct&&e.repathTimer>0&&(e.lostPath||lengthSq(e.waypoint-e.pos)>.04f);
  if(direct)e.lostPath=false;
  // One that batters doors plans through those that give (Door::strength), locked or not.
  const bool batters=battersDoors(e.kind);
  if(!direct&&!keepWaypoint&&(traits.opensDoors||batters)&&e.awareness>0){perf::Scope doorProfile(perf::EnemyDoorPlan);perf::count(perf::DoorPlans);bool any=false;operable.assign(m_world.doors().size(),0);
   for(size_t i=0;i<m_world.doors().size();++i){const auto&d=m_world.doors()[i];if(d.open<d.maxOpen&&((traits.opensDoors&&!aiDoorLocked(m_world,m_level,d))||(batters&&d.breakable()&&!d.broken))){operable[i]=1;any=true;}}
   if(any)doorPlan.emplace(m_world,operable);}
  // A fast mover reaches a one-metre waypoint before the repath clock runs out.
  if(keepWaypoint)destination=e.waypoint;
   else if(!direct&&!m_world.hasTerrain()){perf::Scope stackedProfile(perf::EnemyStacked);destination=stackedWaypoint(m_world,e.pos,e.z,goal,goalZ,hull,true,radius,stepHeight);e.waypoint=destination;e.repathTimer=.35f;e.lostPath=false;
   // No route through the decks that fits its body: it goes to where the
   // prey's way leaves the places it fits (a duct's mouth) and waits there
   // (partialDeckRoute). With no deck at all it ambles a step the way it
   // faces (or turns) and asks again later; rebuilding the deck graph every
   // frame stalls the game.
   if(lengthSq(destination-e.pos)<.0001f){bool holding=false;Vec2 next;
    if(partialDeckRoute(m_world,e.pos,e.z,goal,goalZ,hull,radius,stepHeight,next,holding)){destination=holding?e.pos:next;e.lostPath=holding;e.repathTimer=holding?.5f:.35f;}
    else{e.lostPath=true;e.repathTimer=1.6f;
     for(int turn=0;turn<4;++turn){float a=e.heading+float(turn)*kPi*.5f*((int(e.gait*3)&1)?1.f:-1.f);Vec2 step=e.pos+Vec2{std::cos(a),std::sin(a)}*.9f;
      if(roughSegment(m_world,e.pos,e.z,step,hull,stepHeight,nullptr,true,radius)){destination=step;break;}}}
    e.waypoint=destination;}}
  else if(!direct){
   perf::Scope repathProfile(perf::EnemyRepath);perf::count(perf::FloodFills);
   int field[World::Height][World::Width];for(auto&row:field)for(auto&value:row)value=9999;
   int gx=int(goal.x),gy=int(goal.y);if(m_world.solid(gx+.5f,gy+.5f)){
    int originX=gx,originY=gy;float best=999;for(int yy=originY-1;yy<=originY+1;++yy)for(int xx=originX-1;xx<=originX+1;++xx)if(!m_world.solid(xx+.5f,yy+.5f)){float d=lengthSq(Vec2{xx+.5f,yy+.5f}-goal);if(d<best){best=d;gx=xx;gy=yy;}}
   }
   int minCell=m_world.hasTerrain()?0:1,maxCell=m_world.hasTerrain()?23:22;
   gx=std::clamp(gx,minCell,maxCell);gy=std::clamp(gy,minCell,maxCell);std::queue<std::pair<int,int>> queue;queue.push({gx,gy});field[gy][gx]=0;
   // Terrain edges are kept per collision state, hull and step (the route cache).
   bool freshGrid=false;auto*grid=m_world.hasTerrain()?ai::RouteCache::grid(m_world,hull,stepHeight,true,freshGrid,radius):nullptr;
   auto edgeWalkable=[&](int x,int y,int nx,int ny){
    perf::count(perf::FloodEdges);
    if(!m_world.hasTerrain()){Vec2 a{x+.5f,y+.5f},b{nx+.5f,ny+.5f};float z=navSupport(m_world,a,e.z+stepHeight,radius);return roughSegment(m_world,a,z,b,hull,stepHeight,nullptr,true,radius);}
    return cellEdge(m_world,grid,y*24+x,ny*24+nx,hull,stepHeight,true,radius);
   };
   // Flooding outward from the goal must test travel TOWARD the goal. A
   // traversable descent is not necessarily a climbable reverse edge.
   while(!queue.empty()){auto[x,y]=queue.front();queue.pop();for(int i=0;i<4;++i){int nx=x+dx[i],ny=y+dy[i];if(nx<minCell||nx>maxCell||ny<minCell||ny>maxCell||field[ny][nx]!=9999||!edgeWalkable(nx,ny,x,y))continue;field[ny][nx]=field[y][x]+1;queue.push({nx,ny});}}
   int x=std::clamp(int(e.pos.x),minCell,maxCell),y=std::clamp(int(e.pos.y),minCell,maxCell),best=field[y][x];destination=e.pos;
   for(int i=0;i<4;++i){int nx=x+dx[i],ny=y+dy[i];if(nx>=minCell&&nx<=maxCell&&ny>=minCell&&ny<=maxCell&&field[ny][nx]<best&&edgeWalkable(x,y,nx,ny)){best=field[ny][nx];destination={nx+.5f,ny+.5f};}}
   e.waypoint=destination;e.repathTimer=.25f;e.lostPath=false;
   // Nothing it can walk leads to the goal (the player on a ledge, behind a
   // locked door): it wanders a cell at a time, mostly the way it faces, and
   // asks again in a while instead of flooding the map every frame.
   if(field[y][x]==9999){perf::count(perf::FloodNoRoute);int pick=-1;float bestTurn=9.f;
    for(int i=0;i<4;++i){int nx=x+dx[i],ny=y+dy[i];if(nx<minCell||nx>maxCell||ny<minCell||ny>maxCell||!edgeWalkable(x,y,nx,ny))continue;
     float turn=std::fabs(wrapAngle(std::atan2(float(dy[i]),float(dx[i]))-e.heading))+float((x*7+y*13+i*5+int(e.gait*3))%5)*.35f;
     if(turn<bestTurn){bestTurn=turn;pick=i;}}
    destination=pick<0?e.pos:Vec2{x+dx[pick]+.5f,y+dy[pick]+.5f};e.waypoint=destination;e.repathTimer=1.6f;e.lostPath=true;}
  }
  doorPlan.reset();
  // Nowhere nearer to go (it holds at a mouth it does not fit): it stands and faces its goal.
  if(lengthSq(destination-e.pos)<.0001f){const Vec2 toward=goal-e.pos;if(lengthSq(toward)>.01f)e.heading+=wrapAngle(std::atan2(toward.y,toward.x)-e.heading)*std::min(1.f,dt*4.f);continue;}
  perf::Scope moveProfile(perf::EnemyMove);
  Vec2 direction=normalized(destination-e.pos);
   if(e.kind==Enemy::Kind::Wasp&&visible&&dist>1.8f&&dist<3.8f&&std::fabs(targetFeet-e.z)<.22f&&direct){float side=int(e.home.x)%2?1.f:-1.f;direction=normalized(direction*.5f+Vec2{-direction.y,direction.x}*side*.7f);}
  Vec2 separation{};for(auto&other:m_enemies)if(&other!=&e&&other.alive&&other.bodyBottom()<e.bodyTop()&&other.bodyTop()>e.bodyBottom()){auto away=e.pos-other.pos;float d=length(away);if(d>.001f&&d<.85f)separation+=away*( (.85f-d)/d);}
  direction=normalized(direction+separation*2.f);
  // Unaware, or creeping up on something it has only glimpsed, at half pace.
  float speed=fleeing?std::max(traits.speed,traits.rushSpeed*.75f):stalker?stalkSpeed:traits.speed;if(e.awareness==0||glimpsed)speed*=.5f;
   // A heavy thing's short lurch at prey close in front of it.
   if(traits.lurchSpeed>0&&visible&&sameLevel&&dist<3.2f*e.scale)speed=traits.lurchSpeed;if(e.strike>0)speed*=.35f;
   auto old=e.pos;
   auto move=[&](Vec2 next){float ground=groundHeight(next,e.z+stepHeight,radius),height=hull;
    bool followStep=e.z-ground<=std::max(.24f,stepHeight)&&e.verticalVelocity<=0;float feet=followStep?ground:std::max(e.z,ground);
    if(ground-e.z>stepHeight+.01f||!aiStructureSegment(m_world,e.pos,e.z,next,height,true,radius)||!hullFits(next,feet,height,radius,false,false))return false;
    auto approaches=[&](Vec2 body,float bodyFeet,float bodyTop,float clearance){return feet<bodyTop&&feet+height>bodyFeet&&lengthSq(next-body)<clearance*clearance&&lengthSq(next-body)+.000001f<lengthSq(e.pos-body);};
    for(const auto& p:players())if(approaches(p.body->pos,p.body->z,p.body->z+p.body->hullHeight(),e.contactRadius()+.2f))return false;
    if(!atPlayer&&approaches(target,targetFeet,targetFeet+targetHull,e.contactRadius()+.2f))return false;
    for(const auto&other:m_enemies)if(&other!=&e&&other.alive&&approaches(other.pos,other.bodyBottom(),other.bodyTop(),radius+other.radius()))return false;
    e.pos=next;if(followStep||ground>e.z){e.z=ground;e.verticalVelocity=0;}return true;
   };
   // A full swept step first preserves diagonal speed. Short substeps cannot
   // tunnel through a jamb, a closed leaf or the player's body on a slow frame.
   float travel=std::min(speed*dt,length(destination-e.pos));int steps=std::max(1,int(std::ceil(travel/.08f)));Vec2 delta=direction*(travel/steps);
   for(int i=0;i<steps;++i)if(!move(e.pos+delta)){move(e.pos+Vec2{delta.x,0});move(e.pos+Vec2{0,delta.y});}
  float moved=length(e.pos-old);e.moving=moved>.0001f;e.gait+=moved*creatureTraits(e.kind).gaitPerMetre;
   if(e.moving){auto actual=e.pos-old;e.heading+=std::clamp(wrapAngle(std::atan2(actual.y,actual.x)-e.heading),-dt*6.f,dt*6.f);}
   e.searchTime=dot(e.pos-old,normalized(destination-old))>travel*.2f?0.f:std::min(1.f,e.searchTime+dt);
  if(e.kind==Enemy::Kind::Huntsman&&!m_world.outdoors()&&e.awareness>0&&e.searchTime>.22f){
   float roof=m_world.clearanceHeight(e.pos.x,e.pos.y),floor=m_world.floorHeight(e.pos.x,e.pos.y);
   if(roof-floor>2.15f)for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){
    auto probe=e.pos+d*.42f;
    if(m_world.solid(probe.x,probe.y)&&m_world.wallSpaceFree(e.pos,Vec2{-d.y,d.x},.8f,e.z,e.z+1.05f)){
     e.surfaceNormal=d*-1.f;e.surfaceMode=1;e.verticalVelocity=0;break;
    }
   }
  }
 }
 migrateEnemiesAcrossChunks();
}
ActorPose Game::friendlyAwareness(const World& world,int level,const ActorTrack& track,int homeLevel,size_t homeActor,const ActorPose& old,ActorPose pose,float dt)const{
 perf::Scope awarenessProfile(perf::Awareness);
 if(dt<=0||state(actorAiState(homeLevel,homeActor,track.timerState,"restraint"))||!humanActorVisual(track.visual)||pose.clip!=0||pose.lookAtActor>=0||(track.deadState&&state(track.deadState)))return pose;
 Vec2 facing{std::sin(old.yaw),std::cos(old.yaw)},target{};float nearest=64.f;bool found=false;
 auto consider=[&](Vec2 position,float z){float distance=lengthSq(position-pose.position);
  if(distance<nearest&&aiCanSee(world,pose.position,pose.z+track.scale*.85f,facing,position,z,8.f,-.25f,2.f)){nearest=distance;target=position;found=true;}};
 const std::vector<Enemy>* enemies=nullptr;
 if(level==m_level)enemies=&m_enemies;
 else if(level>=0&&level<chunkCount()&&chunkResident(level))enemies=&m_chunks[level].enemies;
 if(enemies)for(const auto&enemy:*enemies)if(enemy.alive)consider(enemy.pos,(enemy.bodyBottom()+enemy.bodyTop())*.5f);
 if(found){auto toward=target-pose.position;float yaw=std::atan2(toward.x,toward.y);pose.yaw=old.yaw+std::clamp(std::remainder(yaw-old.yaw,2*kPi),-4.f*dt,4.f*dt);}
 return pose;
}
void Game::migrateEnemiesAcrossChunks(){
 // The open zone hands every creature on; elsewhere only a recurring hunter
 // follows its quarry through a seam (one still walking in is not handed back).
 const bool open=m_worldId==WorldId::Ashfall;
 if(!open&&std::none_of(m_enemies.begin(),m_enemies.end(),[](const Enemy&e){return e.hunter&&e.alive;}))return;
 auto sourceOrigin=chunkOffset(m_level);
 for(auto it=m_enemies.begin();it!=m_enemies.end();){
  if(it->pos.x>=0&&it->pos.x<World::Width&&it->pos.y>=0&&it->pos.y<World::Height){++it;continue;}
  if(it->arrive>0||(!open&&!(it->hunter&&it->alive))){++it;continue;}
  Vec2 global=it->pos+sourceOrigin;int target=m_level;
  for(int level=0;level<chunkCount();++level){auto origin=chunkOffset(level);
   if(global.x>=origin.x&&global.x<origin.x+World::Width&&global.y>=origin.y&&global.y<origin.y+World::Height){target=level;break;}
  }
  if(target==m_level){++it;continue;}
  ensureChunk(target);auto origin=chunkOffset(target);auto enemy=*it;Vec2 shift=sourceOrigin-origin;
  if(enemy.originLevel<0&&!enemy.scriptSpawned){enemy.originLevel=m_level;enemy.originHome=enemy.home;}
  enemy.pos+=shift;enemy.lastKnown+=shift;enemy.waypoint+=shift;enemy.home=enemy.pos;
  enemy.z=m_chunks[target].world.supportBelow(enemy.pos.x,enemy.pos.y,enemy.z+.25f);enemy.lastKnownZ=enemy.homeZ=enemy.z;
  m_chunks[target].enemies.push_back(enemy);int leaving=int(it-m_enemies.begin());it=m_enemies.erase(it);
  // Guards, drones and grenades hold enemies by list index: follow this one to
  // its new list and close the gap it left behind.
  std::vector<AiTargetRef> moved(m_enemies.size()+1);for(int i=0;i<int(moved.size());++i)moved[size_t(i)]={ShotKind::Enemy,m_level,i<leaving?i:i-1};
  moved[size_t(leaving)]={ShotKind::Enemy,target,int(m_chunks[target].enemies.size())-1};remapEnemyRefs(m_level,moved);
 }
}
void Game::followEnemyRef(const std::function<StateId(std::string_view)>& key,std::string_view prefix,int level,const std::vector<AiTargetRef>& moved){
 auto field=[&](const char* name){return key(std::string(prefix)+name);};
 if(state(field("kind"))!=int(ShotKind::Enemy)||state(field("level"))!=level+1)return;
 int index=state(field("index"))-1;if(index<0||size_t(index)>=moved.size())return;
 const auto& to=moved[size_t(index)];setState(field("level"),to.level+1);setState(field("index"),to.index+1);
}
void Game::remapEnemyRefs(int level,const std::vector<AiTargetRef>& moved){
 for(size_t actor=0;actor<m_friendlyActors.size();++actor)
  for(std::string_view prefix:{"guard_target_","weapon_target_","hit_","task_enemy_","tactic_","grenade_target_"})
   followEnemyRef([&](std::string_view f){return friendlyKey(actor,f);},prefix,level,moved);
 remapDroneEnemyRefs(level,moved);remapGrenadeEnemyRefs(level,moved);
}
bool Game::testAI(){
 if(!testGuardAI())return false;
 if(!testHearing())return false;
 if(!testFriendlyAI())return false;
 std::ofstream debug("ai-diagnostic.txt");
 // Spatial distractions lead to the noise, never the hidden player's pose.
 for(int scenario=0;scenario<3;++scenario){auto g=validationScene(Enemy::Kind::Huntsman);auto&e=g.m_enemies[0];
  e.pos={4.5f,9.5f};e.home=e.lastKnown=e.pos;e.heading=-kPi*.5f;g.m_player.pos={4.5f,6.5f};g.m_sounds.clear();
  Vec2 source=scenario==2?g.m_player.pos:Vec2{6.5f,9.5f};g.m_sounds.push_back({Sound::Concrete1,source,scenario==1?0.f:1.f,1,true});g.updateEnemies(.01f);
  if(scenario==0){if(e.state!=Enemy::State::Investigate||lengthSq(e.lastKnown-source)>.00001f)return false;}
  else if(e.state!=Enemy::State::Idle||e.awareness!=0)return false;
 }
 // Friendly reactions run through the machinery update, respect walls/death,
 // and never replace a scripted animation or explicit look-at target.
 for(int scenario=0;scenario<4;++scenario){auto g=validationScene(Enemy::Kind::Huntsman);auto map=std::make_shared<AuthoredMapData>();
  map->name="Friendly perception test";map->definition={{0,0},{3,3},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');if(scenario==1)layer.rows[6][7]='#';map->layers.push_back(layer);
  ActorTrack worker;worker.timerState=stateId("friendly_test");worker.keys={{0,{6.5f,6.5f},0,0,scenario==3?1:0},{1000,{6.5f,6.5f},0,0,scenario==3?1:0}};map->actorTracks.push_back(worker);
  TimedSequence timeline;timeline.timerState=worker.timerState;timeline.finishState=stateId("friendly_test_complete");timeline.soundIntervalMs=0;map->timedSequences.push_back(timeline);
  g.m_world=World(0,map);g.m_enemies[0].pos={9.5f,6.5f};g.m_enemies[0].alive=scenario!=2;g.updateMechanisms(.1f);
  float yaw=g.m_world.actorPose(0).yaw;if(scenario==0){if(yaw<=0||yaw>.401f)return false;}else if(yaw!=0)return false;
 }
 // Real reactor stairwell (west, rising north to the manifold), in both
 // directions, not the shorter foundry steps.
 for(bool descend:{true,false}){auto g=mapInspection({2,descend?16.2f:8.f},0,0,3,false,descend?-9.f:-6.f,false);g.m_enemies.resize(1);auto&e=g.m_enemies[0];e={};e.kind=Enemy::Kind::Warden;e.pos={2,descend?8.f:16.2f};e.z=descend?-6.f:-9.f;e.home=e.pos;e.lastKnown=g.m_player.pos;e.lastKnownZ=g.m_player.z;e.awareness=60;e.heading=descend?kPi*.5f:-kPi*.5f;
  g.sound(Sound::Shot);for(int i=0;i<2400&&length(e.pos-g.m_player.pos)>1.2f;++i)g.updateEnemies(1.f/120);
  debug<<"reactor stairs "<<descend<<" position "<<e.pos.x<<' '<<e.pos.y<<" height "<<e.z<<" waypoint "<<e.waypoint.x<<' '<<e.waypoint.y<<'\n';debug.flush();
  if(length(e.pos-g.m_player.pos)>1.3f||std::fabs(e.z-g.m_player.z)>.23f){auto next=e.pos+normalized(e.waypoint-e.pos)*.014f;float ground=g.groundHeight(next,e.z+.215f);debug<<"next "<<next.x<<' '<<next.y<<" ground "<<ground<<" fits "<<g.hullFits(next,ground,1.85f)<<" state "<<int(e.state)<<" distance "<<length(e.pos-g.m_player.pos)<<'\n';return false;}
 }
 {auto alone=validationScene(Enemy::Kind::Huntsman),stacked=alone;
  Enemy upstairs=stacked.m_enemies[0];upstairs.pos.x+=.3f;upstairs.z=3;stacked.m_enemies.push_back(upstairs);
  alone.updateEnemies(.01f);stacked.updateEnemies(.01f);
  if(length(alone.m_enemies[0].pos-stacked.m_enemies[0].pos)>.00001f)return false;
 }
 // Contact-only damage, backstep dodge, and closing door canceling a swing.
 for(int scenario=0;scenario<3;++scenario){auto g=validationScene(Enemy::Kind::Warden);auto& w=g.m_enemies[0];
  w.pos={7.5f,4.5f};w.home=w.pos;g.m_player.pos={6.6f,4.5f};w.heading=kPi;
  if(scenario==2){w.pos={11.f,7.95f};w.home=w.pos;g.m_player.pos={11.f,6.95f};w.heading=-kPi*.5f;g.m_world.setDoor(0,1,true);}
  g.updateEnemies(.01f);if(w.windup<=0)return false;
  if(scenario==1)g.m_player.pos={5.f,5.5f};
  if(scenario==2)g.m_world.setDoor(0,0,false);
  for(int tick=0;tick<70;++tick)g.updateEnemies(1.f/120.f);
  debug<<"warden scenario "<<scenario<<" health "<<g.player().health<<'\n';
  if((scenario==0&&g.player().health>=100)||(scenario!=0&&g.player().health!=100))return false;
 }
 {auto g=validationScene(Enemy::Kind::Warden);auto& e=g.m_enemies[0];e.pos={7.5f,4.5f};g.m_player.pos={4.5f,4.5f};e.heading=kPi;
  for(int i=0;i<120;++i){e.pos={7.5f,4.5f};g.updateEnemies(1.f/120.f);if(e.windup>0||g.player().health!=100)return false;}
 }
 // Stalker behavior: hold while watched, then commit when the player's gaze breaks.
 // It has already made the player out (its notice time is creature-behaviour's 12).
 {auto g=validationScene(Enemy::Kind::Warden);auto& s=g.m_enemies[0];s.pos={8.f,4.5f};s.home=s.pos;s.heading=kPi;s.awareness=0;s.recognition=1;s.stalkMode=Enemy::StalkMode::Watch;s.stalkTimer=0;
  g.m_player.pos={4.5f,4.5f};g.m_player.angle=0;auto start=s.pos;
  for(int i=0;i<60;++i)g.updateEnemies(1.f/120.f);
  debug<<"stalker watch mode "<<int(s.stalkMode)<<" moved "<<length(s.pos-start)<<" timer "<<s.stalkTimer<<'\n';
  if(s.stalkMode!=Enemy::StalkMode::Watch||length(s.pos-start)>.05f)return false;
  g.m_player.angle=kPi;s.stalkTimer=0;float before=length(s.pos-g.m_player.pos);g.updateEnemies(.02f);
  if(s.stalkMode!=Enemy::StalkMode::Flank||s.stalkTimer<2.f)return false;
  s.stalkTimer=0;g.updateEnemies(.02f);
  if(s.stalkMode!=Enemy::StalkMode::Rush||s.stalkTimer<1.f)return false;
  for(int i=0;i<45;++i)g.updateEnemies(1.f/120.f);
  float after=length(s.pos-g.m_player.pos);debug<<"stalker flank then rush distance "<<before<<" -> "<<after<<'\n';
  if(after>=before-.25f)return false;
 }
 // Across the closed Intake freight gate (door 0): unseen, heard, pursued once open.
 auto game=validationScene(Enemy::Kind::Huntsman);auto&e=game.m_enemies[0];e.pos={11.f,9.5f};e.home=e.pos;e.lastKnown=e.pos;e.heading=-kPi*.5f;game.m_player.pos={11.f,6.f};auto original=e.pos;
 for(int i=0;i<120;++i)game.update({},1.f/120.f);debug<<"idle "<<int(e.state)<<' '<<length(e.pos-original)<<'\n';if(e.state!=Enemy::State::Idle||length(e.pos-original)>.05f)return false;
 InputState fire{};fire.fire=true;game.update(fire,.02f);debug<<"heard "<<int(e.state)<<'\n';if(e.state!=Enemy::State::Investigate)return false;
 game.m_world.openDoor(0);for(int i=0;i<600;++i)game.update({},1.f/120.f);debug<<"pursuit "<<e.pos.x<<' '<<e.pos.y<<' '<<int(e.state)<<'\n';if(e.pos.y>9.f)return false;
 {auto mutant=validationScene(Enemy::Kind::Mutant);auto&body=mutant.m_enemies[0];body.pos=mutant.m_player.pos+Vec2{.7f,0};body.heading=kPi;
  mutant.updateEnemies(.01f);if(body.windup<=0)return false;
  for(int i=0;i<72;++i)mutant.updateEnemies(1.f/120.f);
  if(mutant.player().health>=100||mutant.player().health<70)return false;
 }
 auto dodge=validationScene(Enemy::Kind::Brute);auto&attacker=dodge.m_enemies[0];attacker.pos=dodge.m_player.pos+Vec2{.7f,0};attacker.heading=kPi;
 dodge.update({},.01f);if(attacker.windup<=0)return false;
 dodge.m_player.pos=attacker.pos+Vec2{.7f,0};
 for(int i=0;i<70;++i)dodge.update({},1.f/120.f);if(dodge.player().health!=100)return false;
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Wasp,Enemy::Kind::Brute,Enemy::Kind::Mutant,Enemy::Kind::Snatcher,Enemy::Kind::Transformed}){auto stairs=validationScene(kind);auto&climber=stairs.m_enemies[0];
  climber.pos={21.f,15.5f};climber.home=climber.pos;climber.lastKnown=climber.pos;climber.z=stairs.groundHeight(climber.pos);climber.heading=-kPi*.5f;
  stairs.m_player.pos={21.f,11.5f};stairs.m_player.z=stairs.groundHeight(stairs.m_player.pos,5.f);
  climber.lastKnown=stairs.m_player.pos;climber.lastKnownZ=stairs.m_player.z;climber.awareness=60;
  // The flight is open-sided: reaching the player means climbing to their tread.
  auto reached=[&]{return length(climber.pos-stairs.m_player.pos)<1.5f&&climber.z>=1.2f-.01f;};
  for(int i=0;i<1200&&!reached();++i)stairs.update({},1.f/120.f);
  debug<<"stairs "<<int(kind)<<' '<<climber.pos.x<<' '<<climber.pos.y<<' '<<climber.z<<" player z "<<stairs.m_player.z<<'\n';if(!reached())return false;
 }
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Wasp}){
  auto orbit=validationScene(kind);auto&bug=orbit.m_enemies[0];bug.pos={7.5f,4.5f};bug.home=bug.pos;
  for(int i=0;i<360;++i){float angle=i*4.f/120;orbit.m_player.pos=bug.pos+Vec2{std::cos(angle),std::sin(angle)}*.8f;orbit.updateEnemies(1.f/120);}
  debug<<"orbit "<<int(kind)<<" health "<<orbit.player().health<<'\n';if(orbit.player().health>=100)return false;
 }
 auto falling=validationScene(Enemy::Kind::Brute);auto&body=falling.m_enemies[0];body.pos={7.5f,4.5f};body.z=1.2f;body.alive=false;
 falling.updateEnemies(.05f);if(body.z<=0||body.z>=1.2f||body.verticalVelocity>=0)return false;
 for(int i=0;i<120;++i)falling.updateEnemies(1.f/120);if(body.z!=0)return false;
 // Surface Nets terrain traversal: the old grid compared one-metre cell
 // centre heights and treated this authored slope as a wall. Both a nimble
 // Huntsman and the heavier Brute must now follow the sampled ground profile.
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Brute}){
  Game rough(WorldId::Ashfall);rough.m_enemies.clear();rough.m_sounds.clear();
  rough.m_player.pos={20.5f,12.5f};rough.m_player.z=navSupport(rough.m_world,rough.m_player.pos,float(World::TerrainMaxZ+1));rough.m_player.angle=0;
  Enemy climber{};climber.kind=kind;climber.pos={6.5f,12.5f};climber.z=navSupport(rough.m_world,climber.pos,float(World::TerrainMaxZ+1));climber.home=climber.pos;climber.lastKnown=rough.m_player.pos;climber.lastKnownZ=rough.m_player.z;climber.awareness=40.f;climber.state=Enemy::State::Investigate;climber.heading=0;
  float rise=rough.m_player.z-climber.z;if(rise<3.f)return false;rough.m_enemies.push_back(climber);
  if(!roughSegment(rough.m_world,climber.pos,climber.z,{12.5f,12.5f},kind==Enemy::Kind::Brute?1.85f:1.05f,roughStep(kind)))return false;
  for(int i=0;i<3600&&length(rough.m_enemies[0].pos-rough.m_player.pos)>1.5f;++i)rough.updateEnemies(1.f/120.f);
  auto&done=rough.m_enemies[0];debug<<"rough terrain "<<int(kind)<<" distance "<<length(done.pos-rough.m_player.pos)<<" z "<<done.z<<" target z "<<rough.m_player.z<<'\n';
  float support=navSupport(rough.m_world,done.pos,float(World::TerrainMaxZ+1));
  if(length(done.pos-rough.m_player.pos)>1.8f||std::fabs(done.z-support)>.05f||!navFits(rough.m_world,done.pos,done.z,kind==Enemy::Kind::Brute?1.85f:1.05f))return false;
 }
 // Outdoor pursuit ownership crosses the same seam as the player.
 {Game seam(WorldId::Ashfall);seam.m_enemies.clear();Enemy pursuer{};pursuer.kind=Enemy::Kind::Huntsman;pursuer.pos={23.4f,12.f};pursuer.home=pursuer.pos;pursuer.lastKnown={24.1f,12.f};pursuer.awareness=5;pursuer.state=Enemy::State::Chase;pursuer.z=seam.world().floorHeight(pursuer.pos.x,pursuer.pos.y);pursuer.lastKnownZ=pursuer.z;seam.m_enemies.push_back(pursuer);
  seam.m_player.pos={24.1f,12.f};seam.crossChunkBoundary();if(seam.level()!=1||seam.m_enemies.empty()||seam.m_enemies.back().pos.x<.2f||seam.m_enemies.back().pos.x>1.f)return false;
 }
 auto crate=validationScene(Enemy::Kind::Huntsman);auto&crawler=crate.m_enemies[0];crawler.pos={8.5f,5.3f};crawler.home=crawler.pos;crawler.heading=-kPi*.5f;crate.m_player.pos={8.5f,2.5f};crate.m_player.z=.6f;
 crawler.lastKnown=crate.m_player.pos;crawler.lastKnownZ=crate.m_player.z;crawler.awareness=60;
 for(int i=0;i<240&&crawler.z<.59f;++i)crate.updateEnemies(1.f/120);
 debug<<"crate climb "<<crawler.z<<'\n';if(crawler.z<.59f)return false;
 {auto walls=validationScene(Enemy::Kind::Huntsman);auto&spider=walls.m_enemies[0];
  spider.pos={10.5f,1.5f};spider.z=walls.m_world.floorHeight(spider.pos.x,spider.pos.y);spider.surfaceMode=1;
  spider.surfaceNormal={0,1};spider.awareness=8;spider.lastKnown={10,10};walls.m_player.pos={10,10};
  for(int i=0;i<180&&spider.surfaceMode!=2;++i)walls.updateEnemies(1.f/120.f);
  debug<<"wall climb mode "<<spider.surfaceMode<<" at "<<spider.pos.x<<' '<<spider.pos.y<<" z "<<spider.z<<'\n';
  if(spider.surfaceMode!=2||spider.z<=walls.m_world.floorHeight(spider.pos.x,spider.pos.y)+.5f)return false;
  walls.m_player.pos=spider.pos+Vec2{1,0};walls.updateEnemies(1.f/120.f);
  debug<<"wall drop mode "<<spider.surfaceMode<<'\n';
  if(spider.surfaceMode!=0)return false;
 }
 std::ofstream("ai-test.txt")<<"Closed-door sight blocking, hearing/pursuit, reactor stalker watch/flank/rush behavior, committed melee dodging, all species climbing stairs, bugs tracking a circling target, falling bodies, huntsmen climbing crates, rough Surface Nets hill traversal and outdoor cross-chunk pursuit: PASS\n";return true;
}
}
