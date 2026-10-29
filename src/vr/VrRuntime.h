#pragma once
#include "../game/Game.h"
#include <memory>
#include <array>
#include <string>
#include <vulkan/vulkan.h>
namespace retro {
// position/basis: the controller (aim) frame. handOrigin/handBasis: where SteamVR's
// skeleton puts the hand on that controller, in the hand mesh's frame (x across
// pinky to index, y palm, z back from the knuckles), when skeletal input is live.
// gunOrigin/gunBasis: the weapon held in that hand, grip in the palm and the
// barrel along the knuckles ({side, up, back}, as the controller frame).
struct VrHand {bool tracked=false,articulated=false,handFrame=false;Point3 position{};std::array<Point3,3> basis{};Point3 handOrigin{};std::array<Point3,3> handBasis{};Point3 gunOrigin{};std::array<Point3,3> gunBasis{};std::array<float,5> curls{};};
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
 std::string poseReport()const;
 std::pair<int,int> pointer()const;
 std::string instanceExtensions()const;
 std::string deviceExtensions(VkPhysicalDevice device)const;
 VkPhysicalDevice outputDevice(VkInstance instance)const;
 void submit(int eye,VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height);
 void submitPanel(VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height);
 static VrRuntime* active();
 // A headset SteamVR can see, asked without starting SteamVR: the PC/VR choice is only offered then.
 static bool headsetPresent();
 static bool testMath();
 static constexpr float WristScale=.55f,WristCenter=.07f;
 static Point3 wristPoint(Point3 p){return {p.x*WristScale,p.y*WristScale,(p.z-.13f)*WristScale+WristCenter};}
 static std::pair<int,int> wristPointer(Point3 origin,Point3 direction);
 static bool shoulderSlot(Point3 offset,Point3 forward);
 static bool foregripSupport(Point3 right,Point3 left,Point3 rightForward,bool gripped);
private:
 void suspendTracking(Game& game,InputState& input);
 struct Impl;std::unique_ptr<Impl> m;
};
}
