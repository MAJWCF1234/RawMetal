#pragma once
#include <array>
#include <chrono>
#include <cstdint>
namespace retro::perf {
enum Cost {Friendly,Intent,Route,GraphBuild,GraphPath,Walk,Combat,Drones,Enemies,ChunkCopy,Scene,Actor,Prop,Skin,Support,Fits,Structure,Room,Post,Sky,ViewModel,PoseWait,RenderSetup,FenceWait,Overlay,GpuAllocation,Count};
inline constexpr const char* names[]={"friendly","intent","route","graph_build","graph_path","walk","combat","drones","enemies","chunk_copy","draw_scene","actor","prop","skin","support","fits","structure","room","post","sky","viewmodel","pose_wait","render_setup","fence_wait","overlay","gpu_allocation"};
inline thread_local uint64_t cpuGrowthFrames=0,gpuBufferGrowths=0;
inline thread_local bool enabled=false;
inline thread_local std::array<double,Count> costs{};
struct Scope {
 bool active;Cost cost;std::chrono::steady_clock::time_point start;
 explicit Scope(Cost value):active(enabled),cost(value){if(active)start=std::chrono::steady_clock::now();}
 ~Scope(){if(active)costs[cost]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
};
}
