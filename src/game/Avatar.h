#pragma once
// The VR player's avatar (docs/Systems/avatar.md): Claire's whole body, VRChat-style.
//  - AvatarPose is everything the body is posed from: the head, the two hands (wrist, frame and finger curls),
//    the height and stance, the locomotion, and what is in which slot. It is small and plain on purpose: a
//    later network layer can send it as it is and a remote copy poses the same body from it.
//  - solveAvatar is a pure function of a pose and a time: no input, no clock, no hidden state. The torso, the
//    arm targets, the finger curls, the legs' clip and the holsters all come out of it, so a remote avatar
//    replayed from the same poses moves the same way. (Stateful smoothing, such as the hips lagging the head,
//    is done where the pose is made, VrRuntime, and arrives in the pose as bodyYaw.)
// Space: game space, the level's own (chunk-relative) coordinates, z up, as Game keeps the player and the VR hands.
// Game space is the mirror of SteamVR's tracking space (VrRuntime rotateTracking has determinant -1): hand frames
// are built in tracking space and mapped here; the renderer maps them on into the arm rig's (right-handed) space,
// a second reflection, so the rig sees proper rotations (AvatarSolve::rig and testAvatar check it).
#include "../core/Math.h"
#include <array>
#include <cstdint>
#include <iosfwd>
namespace retro {
// One hand: the wrist (SteamVR's wrist joint), the way the fingers point (wrist to the middle knuckle) and the
// palm's normal, and each finger's curl, thumb to little finger, 0 open to 1 closed.
struct AvatarHand {bool tracked=false;Point3 wrist{},forward{1,0,0},palm{0,0,-1};std::array<float,5> curls{};};
// Slot contents are WeaponId + 1 (0 empty): 2 the shotgun, 3 the handgun.
struct AvatarPose {
 bool active=false;
 Point3 head{};                       // the eyes' middle
 float headYaw=0,headPitch=0;         // where the head looks: map angle (cos, sin), pitch in radians, up positive
 float bodyYaw=0;                     // the hips' facing; it lags the head (VrRuntime)
 Point3 root{};                       // the feet: under the body, z on the floor
 float eye=1.42f;                     // the eyes above the feet (the real head's height: ducking, crouching)
 Vec2 velocity{};float gait=0;        // locomotion (m/s) and the gait phase (radians, two footsteps a cycle)
 bool crouched=false,prone=false,grounded=true;
 std::array<AvatarHand,2> hands{};    // physical: [0] the left hand, [1] the right
 std::uint8_t gunSide=1;              // the hand a firearm is held in: 1 right, 0 left-handed
 std::uint8_t inHand=0,back=0,hip=0;  // what is drawn, what rides on the back, what on the hip
 bool support=false;                  // the other hand on the shotgun's fore-end
 float pump=0,reload=0;               // the pump's travel back (m); the reload's progress (0 none)
};
struct AvatarSolve {
 // The hips' frame and the torso's (turned toward the head by twist, leaned forward by lean).
 Point3 bodyForward{},bodyLeft{},torsoForward{},torsoLeft{},torsoUp{};
 float lean=0,twist=0;                // radians: lean forward positive, twist to the head's side
 Point3 neck{},shoulders{};           // the base of the neck and the middle between the shoulders
 std::array<Point3,2> shoulder{};     // each shoulder joint, [0] left, [1] right
 // Each arm's target: the wrist (kept within reach of its shoulder), the hand's frame, the finger curls.
 std::array<Point3,2> wrist{},forward{},palm{};std::array<std::array<float,5>,2> curls{};
 std::array<bool,2> reached{};        // the tracked wrist was within reach (else pulled in to the reach)
 int legsClip=0;float legsPhase=0;    // the locomotion clip (claire-animation.bin) and its phase
 Point3 backSlot{},hipSlot{};         // where the holsters ride
 // A game-space vector in the arm rig's frame (x her left, y up, z ahead: right-handed, as the rig and SteamVR's
 // tracking space are). Game space is a mirror image of that, so this map is a reflection too.
 Point3 rig(Point3 v)const{return {dot3(v,torsoLeft),dot3(v,torsoUp),dot3(v,torsoForward)};}
 static float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
};
// The arm from the shoulder joint to the wrist, in game metres (the body is drawn so her eyes are at
// Player::StandingEye); VrRuntime scales the player's tracking to the avatar so reaches agree.
constexpr float AvatarArmReach=.53f,AvatarShoulderHalf=.165f;
AvatarSolve solveAvatar(const AvatarPose& pose,float time);
// The holsters: grip a hand near one to draw what it holds, let go near it to put the drawn thing back.
// 1 the back (over either shoulder, behind the head), 2 the hip on the gun hand's side, 0 neither.
int avatarSlotAt(const AvatarPose& pose,Point3 hand);
// --vr-test / engine.vr: the solve's frames, reach, slots, finger curls and the mirror chain, numerically.
bool testAvatar(std::ostream& report);
}
