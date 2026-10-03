#pragma once
#include "../game/Game.h"
#include <memory>
#include <array>
#include <string>
#include <vulkan/vulkan.h>
namespace retro {
struct VrHand {bool tracked=false,articulated=false;Point3 position{};std::array<Point3,3> basis{};std::array<float,5> curls{};};
struct VrEye {Point3 position{};std::array<float,16> clip{};};
class VrRuntime {
public:
 VrRuntime();~VrRuntime();
 bool initialize(std::string& error);
 void shutdownGraphics();
 bool update(Game& game,InputState& input,float dt);
 VrEye eye(int index,const Game& game)const;
 const std::array<VrHand,2>& hands()const;
 std::pair<int,int> extent()const;
 bool trackingValid()const;
 bool titlePanel()const;
 std::pair<int,int> pointer()const;
 std::string instanceExtensions()const;
 std::string deviceExtensions(VkPhysicalDevice device)const;
 VkPhysicalDevice outputDevice(VkInstance instance)const;
 void submit(int eye,VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height);
 void submitPanel(VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height);
 static VrRuntime* active();
 static bool testMath();
 static constexpr float WristScale=.55f,WristCenter=.07f;
 static Point3 wristPoint(Point3 p){return {p.x*WristScale,p.y*WristScale,(p.z-.13f)*WristScale+WristCenter};}
 static std::pair<int,int> wristPointer(Point3 origin,Point3 direction);
 static bool shoulderSlot(Point3 offset,Point3 forward);
private:
 struct Impl;std::unique_ptr<Impl> m;
};
}
