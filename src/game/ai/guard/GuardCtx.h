#pragma once
#include <cstddef>
#include <string_view>
namespace retro {
class Game;
struct ActorTrack;
struct ActorPose;
// Narrow facade over Game's per-actor save keys. Prefer this in new guard code
// instead of repeating get/put lambdas so role and behavior modules stay small.
struct GuardCtx {
 Game& game;
 size_t actor;
 int level;
 const ActorTrack& track;
 ActorPose& pose;
 int get(std::string_view field) const;
 void put(std::string_view field,int value);
};
}
