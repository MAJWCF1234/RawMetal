#pragma once
#include "../../world/World.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
namespace retro::ai {
// Route data every planner on one collision state shares: the deck graph
// stackedWaypoint searches and the cell edges the terrain floods walk. Each is
// filled by the planners' own probes, on first need, and kept per exact key:
// ai::collisionSnapshot, the door-plan marks (doors a planner may operate
// count as standing at maxOpen), the hull height, and for cell edges the step
// height and structure sweep. Equal keys answer every probe alike, so a kept
// answer is bit for bit what the planner would have probed; any door, lift,
// crate, platform or broken tile that moves changes the key. A few entries per
// thread, least recently used out; nothing is kept for an authored map no
// Owner names, so a freed map's address can never answer for a new one.
// RAWMETAL_ROUTE_CACHE_VERIFY=1 runs the live planner beside every kept answer
// and counts each difference in route_cache_mismatches.
class RouteCache {
public:
 // One probe: not asked yet, blocked, or clear and ending on `feet`.
 enum EdgeState:uint8_t {Unknown,Blocked,Clear};
 struct Edge {float feet=0;EdgeState state=Unknown;};
 // stackedWaypoint's graph: a node per walkable span over each interior cell,
 // in the planner's own order, the nodes of cell c at [cells[c].begin,
 // cells[c].end), and each node's step toward +x, -x, +y, -y probed lazily.
 struct Deck {
  struct Node {Vec2 p;float z;};struct Range {int begin=0,end=0;};
  std::vector<Node> nodes;std::array<Range,24*24> cells{};std::vector<Edge> edges;
 };
 // A terrain flood's cell grid: edge cell*4+direction (+x, -x, +y, -y) is the
 // walk from the cell centre, on its support, to that neighbour's centre.
 struct Grid {std::array<Edge,24*24*4> edges{};};
 // Names the map behind an authored World for this scope (World keeps it
 // private to Game), so an entry built on it can prove that map still lives.
 // Game opens one around each update that plans, as it does WalkQueryCache.
 class Owner {
 public:
  Owner(const World&world,const std::shared_ptr<const AuthoredMapData>&map);
  ~Owner();
  Owner(const Owner&)=delete;Owner&operator=(const Owner&)=delete;
 private:
  bool m_pushed=false;
 };
 // The deck graph for this hull on the World's present state, or null when
 // nothing may be kept (caching off, or an authored map without an Owner).
 // `fresh` is set when the caller must fill it; edges start unknown. The
 // pointer lives until the next lookup on this thread.
 static Deck* deck(const World&world,float height,bool&fresh);
 static Grid* grid(const World&world,float height,float step,bool sweep,bool&fresh);
 // Off: every planner probes live (the differential test's reference).
 inline static thread_local bool enabled=true;
 // RAWMETAL_ROUTE_CACHE_VERIFY, read once.
 static bool verify();
};
}
namespace retro {
// The planners' kept paths, for the differential test (EnemyAI.cpp): the deck
// planner a creature (human=false) or a guard's fallback (human=true) runs, and
// one terrain cell edge from cell `from` to its neighbour `to`. Each probes
// live where nothing may be kept or RouteCache::enabled is off.
Vec2 aiDeckWaypoint(const World& world,Vec2 start,float startZ,Vec2 goal,float goalZ,float height,bool human);
bool aiCellEdge(const World& world,int from,int to,float height,float stepHeight,bool sweep);
}
