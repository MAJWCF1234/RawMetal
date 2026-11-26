#pragma once

namespace retro::debuglayer {
enum Layer : unsigned {
 Colliders=1u, Lights=2u, Chunks=4u, Hazards=8u, Stats=16u,
 Navigation=32u, Issues=64u,
 MapDiagnostics=Colliders|Navigation|Issues,
 All=MapDiagnostics|Lights|Chunks|Hazards|Stats
};

// Renderer diagnostics are local to this process and outside game/save state.
inline unsigned visible=0;
inline constexpr unsigned Stroke=0xff63e4ffu;
inline constexpr unsigned NavigationStroke=0xff65dacf;
inline constexpr unsigned IssueStroke=0xffff4545;
}
