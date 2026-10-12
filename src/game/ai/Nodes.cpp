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
 float best=1e9f;bool found=false;float currentExposure=length(from-threat);
 for(const auto&node:graph.nodes){
  if(node.hint!=NodeHint::Cover)continue;
  float reach=length(node.position-from);
  if(reach<minDist||reach>maxRange||std::fabs(node.feet-fromFeet)>.5f)continue;
  float end=fromFeet;if(!walk(from,fromFeet,node.position,height,&end)||std::fabs(end-node.feet)>.35f)continue;
  if(reserved(node.position,end)||!covered(node.position,end))continue;
  // Occlusion owns validity. Approaching a wall between the actor and its
  // threat is legitimate cover, so range-to-threat is only a soft preference
  // that keeps open-field retreats from walking into the enemy.
  float exposure=length(node.position-threat);float score=reach+std::max(0.f,currentExposure-exposure)*.35f;
  if(score>=best)continue;best=score;goal=node.position;goalFeet=end;found=true;
 }
 return found;
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
 if(graph.nodes.empty())return goal;
 float direct=fromFeet;if(walk(from,fromFeet,goal,height,&direct)&&std::fabs(direct-goalFeet)<.45f)return goal;
 int origin=-1,target=-1;float originDist=1e9f,targetDist=1e9f;
 for(int i=0;i<int(graph.nodes.size());++i){
  const auto&n=graph.nodes[size_t(i)];
  float da=lengthSq(n.position-from)+std::fabs(n.feet-fromFeet)*8.f;
  float db=lengthSq(n.position-goal)+std::fabs(n.feet-goalFeet)*8.f;
  float end=fromFeet;
  if(da<originDist&&walk(from,fromFeet,n.position,height,&end)&&std::fabs(end-n.feet)<.4f){originDist=da;origin=i;}
  end=n.feet;if(db<targetDist&&walk(n.position,n.feet,goal,height,&end)&&std::fabs(end-goalFeet)<.45f){targetDist=db;target=i;}
 }
 if(origin<0||target<0)return goal;
 if(origin==target)return graph.nodes[size_t(origin)].position;
 std::vector<int> parent(graph.nodes.size(),-1);std::queue<int> queue;queue.push(origin);parent[size_t(origin)]=origin;
 while(!queue.empty()&&parent[size_t(target)]<0){
  int current=queue.front();queue.pop();const auto&n=graph.nodes[size_t(current)];
  for(int i=0;i<n.linkCount;++i){
   const auto&link=graph.links[size_t(n.firstLink+i)];
   if(parent[size_t(link.dest)]>=0)continue;
   const auto&next=graph.nodes[size_t(link.dest)];float end=n.feet;
   if(!walk(n.position,n.feet,next.position,height,&end)||std::fabs(end-next.feet)>.4f)continue;
   parent[size_t(link.dest)]=current;queue.push(link.dest);
  }
 }
 if(parent[size_t(target)]<0)return goal;
 int step=target;while(parent[size_t(step)]!=origin&&step!=origin)step=parent[size_t(step)];
 return graph.nodes[size_t(step)].position;
}
}
