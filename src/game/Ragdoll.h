#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include "../world/World.h"
namespace retro {
struct RagPoint {float x=0,y=0,z=0;RagPoint operator+(RagPoint b)const{return {x+b.x,y+b.y,z+b.z};}RagPoint operator-(RagPoint b)const{return {x-b.x,y-b.y,z-b.z};}RagPoint operator*(float s)const{return {x*s,y*s,z*s};}};
struct RagVertex {RagPoint p;float u=0,v=0,weight=1;uint8_t a=0,b=0,material=0;};
struct HazmatAsset {std::array<RagPoint,15> rest;std::vector<RagVertex> vertices;};
struct Ragdoll {
 static constexpr int Count=15;
 static const HazmatAsset& asset();
 static constexpr std::array<int,15> ends={1,2,1,4,5,4,7,8,7,10,11,10,13,14,13};
 std::array<RagPoint,Count> p{},previous{};
 bool initialized=false,sleeping=false;float quiet=0,accumulator=0;
 // Hung from a hook: joint `pin` is held at `pinAt` (a suit by its hood), and the suit is drawn
 // `slack` as thick across as when worn, so it reads empty.
 int pin=-1;RagPoint pinAt{};float slack=1;
 void hang(const World& world,RagPoint head,float facing,float empty=.7f);
 void seed(const World& world);
 void update(const World& world,float dt);
 void impulse(int joint,RagPoint velocity);
 float rayHit(RagPoint origin,RagPoint direction,int& joint)const;
 void skin(std::vector<RagPoint>& result)const;
 std::vector<RagPoint> skin()const{std::vector<RagPoint> result;skin(result);return result;}
 static bool test();
};
}
