#pragma once
#include "../game/Game.h"
#include "../game/Avatar.h"
#include <memory>
#include <array>
#include <string>
#include <functional>
#include <filesystem>
#include <vulkan/vulkan.h>
namespace retro {
// position/basis: the controller (aim) frame. handOrigin/handBasis: where SteamVR's
// skeleton puts the hand on that controller, in the hand mesh's frame (x across
// pinky to index, y palm, z back from the knuckles), when skeletal input is live.
// gunOrigin/gunBasis: the weapon held in that hand, grip in the palm and the
// barrel along the knuckles ({side, up, back}, as the controller frame).
// hands() is indexed by role: 1 the gun hand, 0 the other (the support hand). left:
// this role's hand is the physical left one. snapped: the support hand is on the
// shotgun's fore-end (handOrigin and handBasis moved there by supportPose); the gun
// frame is then aimed through it.
struct VrHand {bool tracked=false,articulated=false,handFrame=false,left=false,snapped=false;Point3 position{};std::array<Point3,3> basis{};Point3 handOrigin{};std::array<Point3,3> handBasis{};Point3 gunOrigin{};std::array<Point3,3> gunBasis{};std::array<float,5> curls{};};
struct VrEye {Point3 position{};std::array<float,16> clip{};};
// What the compositor measured of the last frame (IVRCompositor::GetFrameTiming): the display's rate, the game's
// GPU time for both eyes, the interval between its frames, and frames the compositor had to drop or reproject.
struct VrTiming {float hz=90,gpuMs=0,frameMs=0;unsigned dropped=0,reprojected=0;float scale=1;};
// What the launch chooser is told: offer VR or not, why, which openvr_api.dll answered,
// and every step taken (written to RawMetal-vr.txt).
struct VrDetection {bool offered=false;std::string reason,runtime,log;};
// The inputs to detection, so a test can stand in for the registry, disk and runtime.
// query: 1 headset present, 0 none, -1 the DLL could not be loaded or asked.
struct VrProbe {std::wstring steamPath;std::string vrpathJson,note;std::function<bool(const std::filesystem::path&)> exists;std::function<int(const std::filesystem::path&,std::string&)> query;};
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
 // bounds: the share of the eye image drawn (the render scale); SteamVR stretches it to the eye.
 void submit(int eye,VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height,float bounds=1.f);
 void submitPanel(VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height);
 static VrRuntime* active();
 // A headset SteamVR can see, asked without starting SteamVR: the PC/VR choice is only offered then.
 static bool headsetPresent();
 static VrDetection detectHeadset();
 static VrDetection detect(const VrProbe& probe);
 static std::string vrpathRuntime(const std::string& json);
 static const wchar_t* const SteamKey;
 static bool testDetection();
 static bool testMath();
 // --vr-test and the lab's engine.vr: every VR check that needs no headset, one PASS/FAIL line each.
 static bool testAll(std::ostream& report);
 static bool shoulderSlot(Point3 offset,Point3 forward);
 static bool foregripSupport(Point3 right,Point3 left,Point3 rightForward,bool gripped);
 // The support hand on the fore-end, worked in tracking space (right-handed; game space is its mirror, so
 // frames are built here and only then mapped). In: the gun hand's grip point and its gun frame's right and
 // up, the support hand's skeletal frame (wrist, across pinky to index, palm, forward along the knuckles), the
 // fore-end's middle in the gun frame (x right, y up, z back, metres), its half length and the pump's travel.
 // Out: the gun frame turned (about the trigger hand, keeping its roll) so the fore-end runs through the
 // support hand's fist, and the support hand turned by the shortest turn that lays its fist along the
 // fore-end (keeping the roll the real wrist has about it: no twist) and slid onto the fore-end.
 struct SupportPose {Point3 gunRight{},gunUp{},gunBack{},wrist{},across{},palm{},forward{};};
 // across is the hand frame's x (forward x palm: toward the thumb on a right hand, the little finger on a left),
 // so leftHand says which way the fist's channel (little finger to index) runs.
 static SupportPose supportPose(Point3 grip,Point3 gunRight,Point3 gunUp,Point3 wrist,Point3 across,Point3 palm,Point3 forward,Point3 foreEnd,float foreEndHalf,float pump,bool leftHand);
 static bool testSupport(std::ostream& report);
 static bool testComfortMath(std::ostream& report);
 // Where the fist closes: wrist + forward * GripAlong + palm * GripIn (sampleHand's gun grip).
 static constexpr float GripAlong=.075f,GripIn=.03f;
 // The fore-end in the gun frame, measured from the drawn shotgun's pump by the renderer.
 static void setForeEnd(Point3 centre,float half);
 // A direction that lags the head (subtitle and title cards): it eases after the look once the look is more
 // than `slack` radians away, and jumps when the look is more than 100 degrees off.
 static Point3 lazyFollow(Point3 anchor,Point3 forward,float dt,float slack=.15f);
 // The head in game space (chunk-relative, as cards and hands are drawn) and where it looks.
 bool head(const Game& game,Point3& position,Point3& forward)const;
 // The avatar's pose (Avatar.h) from the latest tracking and the game: what the renderer poses her body from.
 AvatarPose avatar(const Game& game)const;
 // The comfort vignette's strength now (the setting times how fast the view moves and turns), 0-1.
 float vignette()const;
 const VrTiming& timing()const;
 // Dynamic resolution: the next render scale from this one, the GPU time of the last frame and the frame's
 // budget (both ms). Down 5 % past 90 % of the budget, up 2.5 % under 70 %, within 0.6-1. A pure step, tested.
 static float nextRenderScale(float scale,float gpuMs,float budgetMs);
 // Tracking space scaled to the avatar: her standing eye over the player's calibrated eye height (0.7-1.25).
 static float avatarScale(float calibratedEye,bool seated);
private:
 void suspendTracking(Game& game,InputState& input);
 struct Impl;std::unique_ptr<Impl> m;
};
}
