#pragma once
#include "../../core/Math.h"
#include "../../world/World.h"
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
namespace retro::ai {
// Retained navigation/hint graph for GoldSrc-style cover and path queries.
// Nodes are local to one chunk. Links are the walkability authority at build
// time; live hull moves recheck doors/rails/dynamics at hull-scale pitch, not
// an 8 cm density lattice.
enum class NodeHint : std::uint8_t {Path=0,Cover=1};
struct NavNode {
 Vec2 position{};
 float feet=0;
 NodeHint hint=NodeHint::Path;
 int firstLink=-1,linkCount=0;
};
struct NavLink {
 int dest=-1;
 float cost=0;
};
struct NodeGraph {
 std::vector<NavNode> nodes;
 std::vector<NavLink> links;
 // Exact collision key and unfinished pair buckets move with chunk ownership.
 std::vector<uint32_t> snapshot;
 float height=0;
 bool building=false,complete=false;
 size_t pairI=0,pairJ=1;
 std::vector<std::vector<NavLink>> buckets;
 bool empty()const{return nodes.empty();}
};
using WalkProbe=std::function<bool(Vec2 from,float fromFeet,Vec2 to,float height,float* endFeet)>;
using CoverProbe=std::function<bool(Vec2 position,float feet)>;
using ReservedProbe=std::function<bool(Vec2 position,float feet)>;
struct AuthoredHint {Vec2 position{};float feet=0;NodeHint hint=NodeHint::Path;};
// Build cover hints beside solid structures and a coarse walkable lattice for
// connectivity. Height is the actor hull used for fit and stroll tests.
// Authored hints are merged first so designer cover/path markers survive.
NodeGraph buildNodeGraph(const World& world,float height,const WalkProbe& walk,std::span<const AuthoredHint> authored={});
// Seed nodes once, then consume pairs in the original i/j order. Only a
// completed graph may be queried. The caller owns a per-slice DoorPlan.
void beginNodeGraph(NodeGraph&,const World&,float,std::span<const AuthoredHint> authored={});
bool advanceNodeGraph(NodeGraph&,const WalkProbe&,size_t pairBudget);
bool sameNodeGraph(const NodeGraph&,const NodeGraph&);
bool streamingVerify();
// Prefer Cover hints that block the threat probe, stay walkable from the actor
// and do not move closer to the remembered threat than the current pose.
// minDist rejects node cover closer than that range (FIND_FAR_NODE_COVER).
bool findCoverNode(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float maxRange=8.f,float minDist=0.f);
// Side-step probes without requiring a Cover hint (FIND_LATERAL_COVER).
// sideSign <0 / >0 biases the preferred flank; 0 alternates both sides.
bool findLateralCover(Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,
 Vec2& goal,float& goalFeet,float sideSign=0.f);
// Catalogue wrappers — capability peers of GoldSrc FIND_*_COVER tasks.
bool findCoverFromEnemy(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,
 Vec2& goal,float& goalFeet,float sideSign=0.f,float maxRange=8.f);
bool findNodeCoverFromEnemy(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float maxRange=8.f);
bool findNearNodeCover(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float nearRange=4.f);
bool findFarNodeCover(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 threat,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float minDist=3.f,float maxRange=8.f);
bool findCoverFromOrigin(const NodeGraph& graph,Vec2 from,float fromFeet,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float maxRange=8.f);
bool findCoverFromSound(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 sound,float height,
 const WalkProbe& walk,const CoverProbe& covered,const ReservedProbe& reserved,Vec2& goal,float& goalFeet,float minDist=0.f,float maxRange=8.f);
// Next hop toward a goal using live walk validation on retained links.
Vec2 graphWaypoint(const NodeGraph& graph,Vec2 from,float fromFeet,Vec2 goal,float goalFeet,float height,const WalkProbe& walk);
}
