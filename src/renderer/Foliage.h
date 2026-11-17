#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../world/World.h"
namespace retro {
class Game;
// Plants that answer (physbones): every cutout plant drawn in the static world (grass, ferns, bushes,
// trees, their Painterly Horizon impostors) is a two-bone chain rooted at its base. scene.vert bends
// each vertex by its height over the root: the wind for every plant, everywhere, on the GPU, and on top
// of it the chain's own state, which only the plants near the eye carry: a spring per plant (CPU) that
// bends away from whoever pushes through it (the player, creatures, men, props, bodies), flinches and
// shakes when a round, a hit, a splash or a blast reaches it, and springs back. A plant left alone
// sleeps and costs nothing; one out of reach is never looked at.
//
// The GPU reads, after the lamp grid (GpuRenderer::setPlants): [0] wind x, y (tip metres a unit of
// flex), the time, the gust; [1] unused; then per slot two: the chain (lower bone tip x, y; upper x, y)
// and the plant (root z, height, flex, 0).
class PlantBones {
public:
 static constexpr int MaxPlants=16384;
 // A body or thing that pushes through plants, in the frame of `level`.
 struct Pusher {int level=0;Point3 feet{};float radius=.3f,height=1.7f;Point3 velocity{};};
 // The slot for a plant (the same plant, the same slot), or -1 when there is no room.
 int add(int level,int fixture,Point3 root,float height,float radius);
 void clear();
 // One frame: the wind, the pushers, the rounds and splashes since the last, the springs.
 void update(const Game& game,const std::vector<Pusher>& extra,float now);
 const std::vector<float>& gpu()const{return m_gpu;}
 int slotOf(int level,int fixture)const{auto it=m_slots.find(key(level,fixture));return it==m_slots.end()?-1:it->second;}
 // Where the chain's tip is off its rest (both bones), and whether it is awake.
 Point3 tip(int slot)const;
 bool awake(int slot)const{return slot>=0&&size_t(slot)<m_state.size()&&m_state[size_t(slot)].awake;}
 Point3 wind()const{return {m_gpu.size()>1?m_gpu[0]:0.f,m_gpu.size()>1?m_gpu[1]:0.f,0};}
 // RM_PLANTS=0 stills every plant (wind too), for comparison.
 static bool enabled();
 // Held still (the lab's A/B): no wind and every chain at rest on the GPU; the springs keep their state.
 bool still=false;
 struct Stats {int plants=0,looked=0,awake=0,pushed=0,struck=0;double ms=0;};
 const Stats& stats()const{return m_stats;}
private:
 static std::uint64_t key(int level,int fixture){return std::uint64_t(std::uint32_t(level))<<32|std::uint32_t(fixture);}
 struct Info {int level=0;Point3 root{};float height=1,radius=.5f,flex=1,omega=10;};
 struct State {float x[2]{},v[2]{},x2[2]{},v2[2]{},target[2]{},quiet=0;bool awake=false,pushed=false;};
 void wake(int slot);
 template<class F> void around(int level,float x,float y,float reach,F&& f)const;
 std::vector<Info> m_info;std::vector<State> m_state;std::unordered_map<std::uint64_t,int> m_slots;
 // Plants by 2 m cell of their level.
 std::unordered_map<std::uint64_t,std::vector<int>> m_grid;
 mutable std::vector<unsigned> m_seen;mutable unsigned m_stamp=0;
 std::vector<int> m_awake;std::vector<float> m_gpu;float m_time=-1,m_rays=-1e30f,m_impacts=-1e30f,m_splashes=-1e30f,m_blasts=-1e30f,m_carry=0;
 Stats m_stats;
};
}
