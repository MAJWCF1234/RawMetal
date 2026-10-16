#include "../../core/FrameProfile.h"
#include "Nodes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
namespace retro::ai {
namespace {
bool nearFloor(const World& world,Vec2 p,float& feet,float height){
 feet=world.supportBelow(p.x,p.y,float(World::TerrainMaxZ+1));
 return p.x>=.35f&&p.y>=.35f&&p.x<=23.65f&&p.y<=23.65f&&world.fits(p.x,p.y,feet,height);
}
int addNode(NodeGraph& graph,Vec2 p,float feet,NodeHint hint){
 for(int i=0;i<int(graph.nodes.size());++i){
  auto&n=graph.nodes[size_t(i)];
  if(lengthSq(n.position-p)<.45f*.45f&&std::fabs(n.feet-feet)<.35f){
   if(hint==NodeHint::Cover)n.hint=NodeHint::Cover;
   return i;
  }
 }
 graph.nodes.push_back({p,feet,hint,-1,0});
 return int(graph.nodes.size())-1;
}
}
NodeGraph buildNodeGraph(const World& world,float height,const WalkProbe& walk,std::span<const AuthoredHint> authored){
 perf::Scope frameProfile(perf::GraphBuild);
 NodeGraph graph;
 for(const auto&hint:authored){
  float feet=hint.feet;Vec2 p=hint.position;
  if(!nearFloor(world,p,feet,height)&&!world.fits(p.x,p.y,hint.feet,height))continue;
  if(world.fits(p.x,p.y,hint.feet,height))feet=hint.feet;
  addNode(graph,p,feet,hint.hint);
 }
 // Cover candidates sit beside solid waist+ mass. Rails and thin shelves are
 // ignored so the lattice does not treat climbable geometry as fighting cover.
 for(const auto&s:world.structures()){
  if(s.rail||s.top-s.bottom<.85f||s.top<.7f)continue;
  float cx=(s.x1+s.x2)*.5f,cy=(s.y1+s.y2)*.5f;
  const std::array<Vec2,4> sides={{
   {s.x1-.75f,cy},{s.x2+.75f,cy},{cx,s.y1-.75f},{cx,s.y2+.75f}
  }};
  for(auto side:sides){float feet=0;if(!nearFloor(world,side,feet,height))continue;
   if(std::fabs(feet-s.bottom)>.45f&&std::fabs(feet)>.45f)continue;
   addNode(graph,side,feet,NodeHint::Cover);
  }
 }
 // Coarse lattice keeps long pursuits connected when structures are sparse.
 for(int y=1;y<=22;y+=2)for(int x=1;x<=22;x+=2){
  Vec2 p{x+.5f,y+.5f};float feet=0;if(!nearFloor(world,p,feet,height))continue;
  addNode(graph,p,feet,NodeHint::Path);
 }
 if(graph.nodes.empty())return graph;
 std::vector<std::vector<NavLink>> buckets(graph.nodes.size());
 for(int i=0;i<int(graph.nodes.size());++i){
  const auto&a=graph.nodes[size_t(i)];
  for(int j=i+1;j<int(graph.nodes.size());++j){
   const auto&b=graph.nodes[size_t(j)];
   float distance=length(a.position-b.position);
   if(distance<.35f||distance>3.1f||std::fabs(a.feet-b.feet)>.45f)continue;
   float ab=a.feet,ba=b.feet;
   if(!walk(a.position,a.feet,b.position,height,&ab)||std::fabs(ab-b.feet)>.35f)continue;
   if(!walk(b.position,b.feet,a.position,height,&ba)||std::fabs(ba-a.feet)>.35f)continue;
   buckets[size_t(i)].push_back({j,distance});
   buckets[size_t(j)].push_back({i,distance});
  }
 }
 for(int i=0;i<int(graph.nodes.size());++i){
  auto&n=graph.nodes[size_t(i)];
  n.firstLink=int(graph.links.size());
  n.linkCount=int(buckets[size_t(i)].size());
  for(const auto&link:buckets[size_t(i)])graph.links.push_back(link);
 }
 return graph;
}
bool findCoverNode(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float maxRange,float minDist){
 float currentExposure=length(from-threat);std::vector<std::pair<float,size_t>> ranked;ranked.reserve(graph.nodes.size());
 for(size_t index=0;index<graph.nodes.size();++index){const auto&node=graph.nodes[index];
  if(node.hint!=NodeHint::Cover)continue;
  float reach=length(node.position-from);
  if(reach<minDist||reach>maxRange||std::fabs(node.feet-fromFeet)>.5f)continue;
  // Occlusion owns validity. Approaching a wall between the actor and its
  // threat is legitimate cover, so range-to-threat is only a soft preference
  // that keeps open-field retreats from walking into the enemy.
  float exposure=length(node.position-threat);float score=reach+std::max(0.f,currentExposure-exposure)*.35f;
  ranked.push_back({score,index});
 }
 // Scoring has no collision side effects. Preserve its exact minimum and
 // original tie order while sweeping only until the best valid post is found.
 std::sort(ranked.begin(),ranked.end());
 for(auto [score,index]:ranked){const auto&node=graph.nodes[index];float end=fromFeet;
  if(!walk(from,fromFeet,node.position,height,&end)||std::fabs(end-node.feet)>.35f)continue;
  if(reserved(node.position,end)||!covered(node.position,end))continue;
  goal=node.position;goalFeet=end;return true;
 }
 return false;
}
bool findLateralCover(Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,
 Vec2& goal,float& goalFeet,float sideSign){
 Vec2 toward=normalized(threat-from);if(lengthSq(toward)<.001f)toward={0,1};
 Vec2 side{-toward.y,toward.x};if(sideSign<0)side=side*(-1.f);
 const std::array<Vec2,8> offsets={{toward*(-.75f),toward*(-1.5f),side*.75f,side*(-.75f),toward*(-.65f)+side*1.2f,toward*(-.65f)-side*1.2f,side*2.f,side*(-2.f)}};
 float best=1e9f;bool found=false;
 for(auto offset:offsets){auto candidate=from+offset;if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
  float feet=fromFeet;if(!walk(from,fromFeet,candidate,height,&feet)||reserved(candidate,feet)||!covered(candidate,feet))continue;
  float score=lengthSq(offset);if(score>=best)continue;best=score;goal=candidate;goalFeet=feet;found=true;
 }
 return found;
}
bool findCoverFromEnemy(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,
 Vec2& goal,float& goalFeet,float sideSign,float maxRange){
 if(findLateralCover(from,fromFeet,threat,height,walk,covered,reserved,goal,goalFeet,sideSign))return true;
 return findCoverNode(graph,from,fromFeet,threat,height,walk,covered,reserved,goal,goalFeet,maxRange);
}
bool findNodeCoverFromEnemy(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float maxRange){
 return findCoverNode(graph,from,fromFeet,threat,height,walk,covered,reserved,goal,goalFeet,maxRange);
}
bool findNearNodeCover(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float nearRange){
 return findCoverNode(graph,from,fromFeet,threat,height,walk,covered,reserved,goal,goalFeet,nearRange);
}
bool findFarNodeCover(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float minDist,float maxRange){
 return findCoverNode(graph,from,fromFeet,threat,height,walk,covered,reserved,goal,goalFeet,maxRange,minDist);
}
bool findCoverFromOrigin(const NodeGraph& graph,Vec2 from,float fromFeet,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float maxRange){
 return findCoverNode(graph,from,fromFeet,from,height,walk,covered,reserved,goal,goalFeet,maxRange);
}
bool findCoverFromSound(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 sound,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float minDist,float maxRange){
 return findCoverNode(graph,from,fromFeet,sound,height,walk,covered,reserved,goal,goalFeet,maxRange,minDist);
}
Vec2 graphWaypoint(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 goal,float goalFeet,float height,const WalkProbe& walk){
 perf::Scope frameProfile(perf::GraphPath);
 if(graph.nodes.empty())return goal;
 float direct=fromFeet;if(walk(from,fromFeet,goal,height,&direct)&&std::fabs(direct-goalFeet)<.45f)return goal;
 // Select the nearest reachable connectors first. The former insertion-order
 // scan swept many successively closer nodes before reaching the same answer.
 auto connector=[&](bool source){std::vector<std::pair<float,int>> order;order.reserve(graph.nodes.size());
  for(int i=0;i<int(graph.nodes.size());++i){const auto&n=graph.nodes[size_t(i)];order.push_back({lengthSq(n.position-(source?from:goal))+std::fabs(n.feet-(source?fromFeet:goalFeet))*8.f,i});}
  std::sort(order.begin(),order.end());
  for(auto [distance,index]:order){const auto&n=graph.nodes[size_t(index)];float end=source?fromFeet:n.feet;
   if(source?walk(from,fromFeet,n.position,height,&end)&&std::fabs(end-n.feet)<.4f:walk(n.position,n.feet,goal,height,&end)&&std::fabs(end-goalFeet)<.45f)return index;}
  return -1;
 };
 int origin=connector(true),target=connector(false);if(origin<0||target<0)return goal;
 if(origin==target)return graph.nodes[size_t(origin)].position;
 // Search retained topology cheaply, then certify only its candidate route.
 // A blocked live edge is excluded and the same BFS is retried. Validation is
 // local to this query, so doors, lifts and changed hulls cannot go stale.
 std::vector<signed char> valid(graph.links.size(),-1);
 std::vector<int> parent(graph.nodes.size()),via(graph.nodes.size()),path;
 for(;;){
  std::fill(parent.begin(),parent.end(),-1);std::queue<int> queue;queue.push(origin);parent[size_t(origin)]=origin;
  while(!queue.empty()&&parent[size_t(target)]<0){int current=queue.front();queue.pop();const auto&n=graph.nodes[size_t(current)];
   for(int i=0;i<n.linkCount;++i){int edge=n.firstLink+i;int next=graph.links[size_t(edge)].dest;
    if(valid[size_t(edge)]==0||parent[size_t(next)]>=0)continue;
    parent[size_t(next)]=current;via[size_t(next)]=edge;queue.push(next);
   }
  }
  if(parent[size_t(target)]<0)return goal;
  path.clear();for(int node=target;node!=origin;node=parent[size_t(node)])path.push_back(node);
  bool blocked=false;
  // Baked links already paid the lattice at build. Live certify is the same
  // walk probe (doors/rails/dynamics), now at hull-scale pitch — do not rebuild
  // an 8 cm density check along every retained edge each hop.
  for(auto it=path.rbegin();it!=path.rend();++it){int node=*it,edge=via[size_t(node)];if(valid[size_t(edge)]>=0)continue;
   const auto&a=graph.nodes[size_t(parent[size_t(node)])];const auto&b=graph.nodes[size_t(node)];float end=a.feet;
   valid[size_t(edge)]=walk(a.position,a.feet,b.position,height,&end)&&std::fabs(end-b.feet)<.4f?1:0;
   if(!valid[size_t(edge)]){blocked=true;break;}
  }
  if(blocked)continue;
  const auto&next=graph.nodes[size_t(path.back())];float end=fromFeet;
  // The actor may stand off-centre beside a corner. A valid node-to-node edge
  // does not prove the diagonal from its actual feet; reach the origin first.
  if(!walk(from,fromFeet,next.position,height,&end)||std::fabs(end-next.feet)>.4f)return graph.nodes[size_t(origin)].position;
  return next.position;
 }
}
}
