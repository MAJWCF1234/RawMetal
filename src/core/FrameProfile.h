#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
namespace retro::perf {
// Scoped frame costs, in milliseconds, summed per thread while `enabled`.
// steady_clock is QueryPerformanceCounter on MSVC. Costs nest: a scope inside
// another is counted in both (guard_threat sits inside guard_mind, which sits
// inside friendly). --perf-profile and the Ashfall frame profile read them.
enum Cost {Friendly,Intent,Route,GraphBuild,GraphPath,Walk,Combat,Drones,Enemies,ChunkCopy,Scene,Actor,Prop,Skin,Support,Fits,Structure,Room,Post,Sky,ViewModel,PoseWait,RenderSetup,FenceWait,Overlay,GpuAllocation,
 // Game::update's other phases, at their call sites in Game.cpp.
 Mechanisms,Movement,Stealth,Scripts,Streaming,Interaction,Clutter,Hazards,Takedown,GuardTasks,Grenades,Voices,Pickups,LockerDoors,
 // Inside updateEnemies: senses (sight, friendlies, hearing), the stalker's
 // choices, the door overlay, the deck graph, the cell flood, the step itself.
 EnemySense,EnemyStalk,EnemyDoorPlan,EnemyStacked,EnemyRepath,EnemyMove,
 // stackedWaypoint wherever it runs (creatures on decks, guards and civilians
 // on every non-terrain map through aiWalkWaypoint).
 StackedWaypoint,
 // Inside updateFriendlyAI for guards: guardIntent whole, its threat pick,
 // the locker witness, and the schedule/executor it hands on to.
 GuardMind,GuardThreat,LockerWitness,GuardSchedule,Awareness,
 // updateFriendlyActors for civilians and escorts (inside mechanisms and friendly).
 Civilians,
 // requestAiDoor: an aware door-opening creature each frame, a moving guard each frame.
 DoorRequests,
 // friendlyChunkNeeded: updateStreaming asks it of every other chunk each frame (inside streaming).
 ChunkNeeded,
 Count};
inline constexpr const char* names[]={"friendly","intent","route","graph_build","graph_path","walk","combat","drones","enemies","chunk_copy","draw_scene","actor","prop","skin","support","fits","structure","room","post","sky","viewmodel","pose_wait","render_setup","fence_wait","overlay","gpu_allocation",
 "mechanisms","movement","stealth","scripts","streaming","interaction","clutter","hazards","takedown","guard_tasks","grenades","voices","pickups","locker_doors",
 "enemy_sense","enemy_stalk","enemy_door_plan","enemy_stacked","enemy_repath","enemy_move",
 "stacked_waypoint",
 "guard_mind","guard_threat","locker_witness","guard_schedule","awareness",
 "civilians","door_requests","chunk_needed"};
static_assert(std::size(names)==Count,"every cost has a name");
inline thread_local uint64_t cpuGrowthFrames=0,gpuBufferGrowths=0;
inline thread_local bool enabled=false;
// Leaf timers sit in the hottest collision queries (fits, supportBelow, the
// structure sweep), called up to hundreds of thousands of times a frame; two
// clock reads each can cost more than the query. --perf-profile turns them off
// unless asked (--perf-leaf) and counts those calls instead.
inline thread_local bool leafTimers=true;
inline thread_local std::array<double,Count> costs{};
struct Scope {
 bool active;Cost cost;std::chrono::steady_clock::time_point start;
 explicit Scope(Cost value,bool leaf=false):active(enabled&&(!leaf||leafTimers)),cost(value){if(active)start=std::chrono::steady_clock::now();}
 // Close a section early (a block whose locals outlive it).
 void stop(){if(active){costs[cost]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();active=false;}}
 ~Scope(){stop();}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
struct LeafScope:Scope {explicit LeafScope(Cost value):Scope(value,true){}};
// Calls per frame of the queries the AI leans on. Always counted (one
// thread-local add each, no clock); a frame profiler resets and reads them.
enum Counter {Navigable,RoughSegment,NavFits,FitsCalls,FitsComputed,RayClear,RayClearSteps,SupportCalls,DoorBlocks,Solid,SpansAt,
 StackedCalls,StackedNodes,StackedEdges,StackedNoRoute,FloodFills,FloodEdges,FloodNoRoute,DoorPlans,AiCanSee,SoundRay,LightRay,
 GuardIntents,GuardThreats,GuardTargets,GuardSight,FriendlyUpdates,FriendlyWalks,FriendlyWalksComputed,FriendlyRay,TraceShot,HostileCensus,DoorLockChecks,StateWrites,StateInserts,StateErases,
 CreatureUpdates,CreaturesHunting,
 // Retained AI data, keyed on ai::collisionSnapshot: route-graph lookups that
 // found or built their entry, guard node graphs built whole, kept across
 // residency or advanced a slice ahead of need. The mismatch counters are the
 // verify modes' own: a retained answer that differs from the live one, or a
 // per-frame chunk-residency answer that differs from asking chunk by chunk.
 // Each must read zero.
 RouteCacheHits,RouteCacheMisses,RouteCacheMismatches,NavGraphBuilt,NavGraphReused,NavGraphSlices,StreamingMismatches,CounterCount};
inline constexpr const char* counterNames[]={"navigable","rough_segment","nav_fits","fits_calls","fits_computed","ray_clear","ray_clear_steps","support_below","door_blocks","solid","spans_at",
 "stacked_calls","stacked_nodes","stacked_edges","stacked_no_route","flood_fills","flood_edges","flood_no_route","door_plans","ai_can_see","sound_ray","light_ray",
 "guard_intents","guard_threats","guard_targets","guard_sight","friendly_updates","friendly_walks","friendly_walks_computed","friendly_ray","trace_shot","hostiles_remaining","door_lock_checks","state_writes","state_inserts","state_erases",
 "creature_updates","creatures_hunting",
 "route_cache_hits","route_cache_misses","route_cache_mismatches","navgraph_built","navgraph_reused","navgraph_slices","streaming_mismatches"};
static_assert(std::size(counterNames)==CounterCount,"every counter has a name");
inline thread_local std::array<uint64_t,CounterCount> counts{};
inline void count(Counter counter,uint64_t amount=1){counts[counter]+=amount;}
// One creature's whole updateEnemies body, by CreatureKind.
inline constexpr int CreatureKinds=8;
inline thread_local std::array<double,CreatureKinds> creatureCosts{};
inline thread_local std::array<uint32_t,CreatureKinds> creatureRuns{};
struct CreatureScope {
 bool active;int kind;std::chrono::steady_clock::time_point start;
 explicit CreatureScope(int value):active(enabled&&value>=0&&value<CreatureKinds),kind(value){if(active)start=std::chrono::steady_clock::now();}
 ~CreatureScope(){if(active){creatureCosts[size_t(kind)]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();++creatureRuns[size_t(kind)];}}
 CreatureScope(const CreatureScope&)=delete;CreatureScope& operator=(const CreatureScope&)=delete;
};
inline void resetFrame(){costs.fill(0);counts.fill(0);creatureCosts.fill(0);creatureRuns.fill(0);}
}
