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
// gunOrigin/gunBasis: the weapon held in that hand, grip in the fist and the
// barrel along the knuckles raised by GunPitch ({right, up, back}, as the controller frame; up toward the index
// finger on either hand), recoil applied.
// hands() is indexed by role: 1 the gun hand, 0 the other (the support hand). left:
// this role's hand is the physical left one. snapped: the support hand is on the
// gun: the shotgun's fore-end (supportPose; the gun frame is then aimed through it) or round the pistol's grip
// (pistolSupportPose); handOrigin and handBasis are moved there.
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
 static bool gripHeld(bool was,bool clickActive,bool click,bool squeezeActive,float squeeze,bool legacy);
 static bool testGrip();
 static bool testMath();
 // --vr-test and the lab's engine.vr: every VR check that needs no headset, one PASS/FAIL line each.
 static bool testAll(std::ostream& report);
 static bool shoulderSlot(Point3 offset,Point3 forward);
 static bool foregripSupport(Point3 right,Point3 left,Point3 rightForward,bool gripped);
 // The gun in a hand, in tracking space, from the skeletal hand frame (across, palm, forward as handFrame builds
 // it): up along the fist's channel toward the index finger (either hand), back from the knuckles, then the whole
 // frame turned about its right so the muzzle rises GunPitch above the knuckles' line. A pistol grip is raked: with
 // the hand closed round it the knuckles run that far below the bore (the medic pistol's grip is 10-15 degrees off
 // square to its slide, a Glock's 22, the Touch controller's handle more), so the bore follows the hand the way the
 // real gun would and the model's grip lies along the fist instead of poking out of its heel.
 static constexpr float GunPitch=.2443f;   // 14 degrees
 // The MAC-11's grip is square to its bore (its magazine runs straight down through it, measured from mac11.fbx):
 // raised only the 4 degrees the medic pistol's 10-degree grip sits under GunPitch, so its magazine lies along the
 // fist's channel as the pistol's does, and the same hand points it that much lower than the pistol.
 static constexpr float MacPitch=.0698f;   // 4 degrees
 static float gunPitch(WeaponId gun){return gun==WeaponId::MachinePistol?MacPitch:GunPitch;}
 static void gunFrame(Point3 across,Point3 palm,Point3 forward,bool leftHand,Point3& right,Point3& up,Point3& back,float pitch=GunPitch);
 // The support hand on the fore-end, worked in tracking space (right-handed; game space is its mirror, so
 // frames are built here and only then mapped). In: the gun hand's grip point and its gun frame's right and
 // up, the support hand's skeletal frame (wrist, across pinky to index, palm, forward along the knuckles), the
 // fore-end's middle in the gun frame (x right, y up, z back, metres), its half length and the pump's travel.
 // Out: the gun frame turned (about the trigger hand, keeping its roll) so the fore-end runs through the
 // support hand's fist, and the support hand laid round the fore-end: its fist's channel along the wood (index to
 // the muzzle), its palm toward the wood, and its knuckles pointing away from the support arm's elbow (elbow: an
 // estimate in tracking space), so the forearm comes up to the wrist from where the arm is whatever way the
 // controller is held. The real hand's roll about the wood turns that by at most SupportRoll either way.
 // (The first snap, reverted in October, and the one after it took the shortest turn from the controller's hand
 // onto the wood. A Touch controller is held with its handle upright under a fore-end, so that turn tipped the
 // knuckles down and put the drawn wrist on top of the wood with the forearm bent back over it: the twist.)
 struct SupportPose {Point3 gunRight{},gunUp{},gunBack{},wrist{},across{},palm{},forward{};};
 // across is the hand frame's x (forward x palm: toward the thumb on a right hand, the little finger on a left),
 // so leftHand says which way the fist's channel (little finger to index) runs.
 static constexpr float SupportRoll=.61f;   // 35 degrees
 static SupportPose supportPose(Point3 grip,Point3 gunRight,Point3 gunUp,Point3 wrist,Point3 across,Point3 palm,Point3 forward,Point3 foreEnd,float foreEndHalf,float pump,bool leftHand,Point3 elbow);
 // The support hand round the pistol's grip (tracking space): its fist on the support side of the gun hand and a
 // little lower, the channel up the grip, knuckles forward and round the front of the gun hand's fingers, the palm
 // toward the gun. Only the hand moves: the pistol aims from the gun hand.
 static SupportPose pistolSupportPose(Point3 grip,Point3 gunRight,Point3 gunUp,Point3 gunBack,bool leftHand,float pitch=GunPitch);
 // SteamVR Input: the action manifest and each controller's default bindings, written to %LOCALAPPDATA%/RawMetal/vr.
 // SteamVR caches default bindings by file name, so a binding change takes a new BindingsVersion (the files are
 // <controller>-v<BindingsVersion>.json). BindingActions: the boolean, vector1 and vector2 inputs in handle order.
 static constexpr int BindingsVersion=4,BindingActions=14;
 static const char* const* bindingActionNames();
 static std::string actionManifest();
 static std::string controllerBindings(const std::string& controller);
 static bool testBindings(std::ostream& report);
 static bool testSupport(std::ostream& report);
 // A pose without SteamVR, for tests and the lab's VR captures: the head (tracking space, where it looks), each
 // physical hand's skeletal joints (wrist, index, middle and little-finger knuckles, tracking space), its velocity,
 // grip and trigger, and the reload button. script() makes this runtime the active one and runs the hands, the
 // holsters, the support grip, the recoil and the aim exactly as update() does after it has read SteamVR.
 struct VrScript {Point3 head{0,1.42f,0},headForward{0,0,-1};std::array<std::array<Point3,4>,2> joints{};std::array<Point3,2> velocity{};std::array<bool,2> tracked{true,true},grip{},trigger{};bool reload=false;};
 bool script(Game& game,InputState& input,const VrScript& pose,float dt);
 // A hand's joints for a scripted pose (tracking space): the wrist, the knuckles' way (forward) and the fist's
 // channel toward the index finger. hold: where the fist closes (GripAlong/GripIn), which places the wrist.
 static std::array<Point3,4> scriptHand(Point3 hold,Point3 forward,Point3 channel,bool leftHand);
 // The scripted checks of the guns in the hands (aim, sights and trace, left-handed, holsters, two hands).
 static bool testGuns(std::ostream& report);
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
 // After the hands are read (update() from SteamVR, script() from a scripted pose): the grips, the holsters, the
 // belt pouch, the support grip on the gun, the recoil, the aim and what the game is told of the hands and gun.
 void playHands(Game& game,InputState& input,bool focused,bool menuOpen);
 struct Impl;std::unique_ptr<Impl> m;
};
}
