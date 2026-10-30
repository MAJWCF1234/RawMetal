#pragma once
#include <array>
namespace retro {
// The direct-Mixamo Cleanup bodies (cleanupActorBody): 0-5 the squad roles,
// 6 the company Investigator. Each is a bind OBJ, an RMA3 bank with layer
// joints and durations, body and head textures, and RMC2 arm calibration.
constexpr int kCleanupBodies=7;
struct CleanupBodyResources {int mesh,animation;std::array<int,2> textures;int skin;};
constexpr CleanupBodyResources cleanupBodyResources(int body){
 // The Investigator's single atlas covers the coat, hat and mask alike.
 return body<6?CleanupBodyResources{303+body*5,304+body*5,{305+body*5,306+body*5},340+body}:CleanupBodyResources{2200,2201,{2202,2202},2203};
}
}
