#pragma once
#include "Mesh.h"
#include <array>
#include <vector>
namespace retro {
// Claire seen from outside (the third-person camera, replays and the film
// cameras). Her clips are baked skinned vertices, so she is posed from them
// here: the legs from a locomotion clip, the torso and arms from a hold (the
// pistol's, the rifle aim, a shot, a reload) laid over it from the waist up,
// and the upper body turned about her chest so the gun in her hands points
// where the player aims. Her hands are read back from the posed skin, and the
// world gun is fitted to them.
//
// Mesh space is her FBX's: Y up, +Z ahead of her, +X her left.
class ClaireOutside {
public:
 // claire-animation.bin, in order (docs/Systems/player-body.md).
 enum Clip:int{Idle=0,RunForward=1,Sprint=5,CrouchIdle=6,CrouchWalk=7,Falling=11,Death=12,Walk=13,ProneIdle=14,Crawl=15,CrawlBack=19,
  AimIdle=24,CrouchAim=25,RifleShot=26,RifleReload=27,Turn=28,PistolIdle=32,PistolAim=33,PistolAimUp=34,PistolAimDown=35,PistolShoot=36,PistolReload=37,HitReaction=38,Land=39,ClipCount=40};
 // An upper-body hold: clip at phase, mixed toward mixClip by mix (the
 // pistol's aim up and down), laid over the legs by weight.
 struct Layer{int clip=-1;float phase=0,weight=0;int mixClip=-1;float mixPhase=0,mix=0;};
 // One hand as the skin has it: palm centre, the point a held grip runs
 // through, the way the fingers point, index-to-little-finger, the palm's
 // normal (toward what the fingers close on), and the forearm's middle.
 struct Hand{Point3 palm,grip,forward,across,normal,forearm;};
 enum Group:int{Pelvis,Chest,Head,FootLeft,FootRight,GroupCount};
 // A held gun fitted to her hands: a point p of the gun's model lands at
 // origin+x*q.x+y*q.y+z*q.z with q=(p-socket)*scale; bore is the way it shoots.
 struct Gun{Point3 origin,x,y,z,bore;float scale=1;};
 // A long gun: the trigger socket in the right hand, turned about it so the
 // support socket's line runs through the left, its top toward the right hand's
 // knuckles. forward is the model's muzzle direction.
 static Gun fitLongGun(const Hand& right,const Hand& left,Point3 trigger,Point3 support,Point3 forward,float scale);
 // A pistol: its grip in the right fist, the bore along the forearm, the top
 // toward the index finger. The model is Y up, +Z out of the muzzle.
 static Gun fitPistol(const Hand& right,Point3 grip,float scale);
 // While both hands hold it, the long gun is remembered in the right hand's
 // frame; when the left lets go (a reload) it stays as it was in the right.
 void rememberInRightHand(const Hand& right,const Gun& gun);
 bool gunFromRightHand(const Hand& right,Gun& gun)const;
 static Point3 place(const Gun& gun,Point3 socket,Point3 p){const Point3 q=(p-socket)*gun.scale;return gun.origin+gun.x*q.x+gun.y*q.y+gun.z*q.z;}
 // What one frame from outside measured (the third-person test reads it).
 struct Report{bool drawn=false;int weapon=0,lower=-1,layer=-1,headFaces=0;float supportGap=-1,supportAlong=0,aimError=-1,floorGap=0,handGap=-1,scale=0;bool blending=false;Point3 feet[2]{},gripRight{},gripLeft{};};
 bool prepare(const Mesh& mesh);
 bool ready()const{return m_ready;}
 // Her mesh is not in metres: this many of its units make a metre when she
 // stands 1.7 m tall from outside.
 float perMetre()const{return m_perMetre;}
 bool hasClip(int clip)const{return clip>=0&&clip<m_clips;}
 // Poses legs, layer and aim into the working pose (not yet published).
 // pitch raises the upper body (radians, up positive) and yaw turns it to her
 // left, both about the chest, each vertex as far as it belongs to the torso.
 void compose(const Mesh& mesh,int lowerClip,float lowerPhase,const Layer& layer,float pitch,float yaw);
 // Starts a blend from the pose last published (a change of clip or hold).
 void blendFromLast(){m_from=m_out;m_hasFrom=!m_from.empty();}
 // The body turned by yaw (a turn on the spot ending) while a blend runs: the
 // pose it blends from is turned back by as much about centre, so it stays put.
 void turnBlendStart(float yaw,Point3 centre);
 bool blending()const{return m_hasFrom;}
 // Publishes the working pose, mixed from the blend start by amount (1 done).
 void publish(Mesh& mesh,float amount);
 // Bends the published left arm (shoulder, elbow, wrist, as the skin weights
 // share them) so its grip point reaches target: the support hand onto the
 // gun when the clip's hands are spaced for a longer one. Republishes; returns
 // how far the grip was left from the target (0 when it reached).
 float reachLeft(Mesh& mesh,Point3 target);
 // Read from the working pose (posed) or the published one.
 Hand hand(int side,bool published=true)const;
 Point3 centroid(Group group,bool published=true)const;
 const std::vector<Point3>& published()const{return m_out;}
 float lowestPublished()const;
private:
 struct Weighted{std::vector<std::pair<uint32_t,float>> members;};
 Point3 mean(const Weighted& set,const std::vector<Point3>& from)const;
 bool m_ready=false,m_failed=false;int m_clips=0;float m_perMetre=1;
 std::array<Point3,5> m_gunInHand{};bool m_gunRemembered=false;float m_gunScale=1;   // origin offset, x, y, z, bore in the right hand's frame
 std::vector<float> m_upper;                      // how far each vertex belongs to the torso and arms
 std::array<Weighted,GroupCount> m_groups;
 std::array<std::array<Weighted,6>,2> m_hand;     // per side: palm, index, middle, little knuckles, fingertips, forearm
 std::array<Weighted,3> m_leftJoints;            // the left shoulder, elbow and wrist, as rings of skin shared by two bones
 std::vector<float> m_leftUpper,m_leftFore,m_leftHand;   // each vertex's weight on the left upper arm, forearm and hand (fingers in)
 std::vector<Point3> m_lower,m_layer,m_mix,m_pose,m_out,m_from;bool m_hasFrom=false;
};
}
