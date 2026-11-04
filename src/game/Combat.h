#pragma once
#include "../audio/Sound.h"
#include <cstdint>
namespace retro {

// Compact combat tables for industrial-horror loadouts. Add a WeaponDef row to
// expand firearms; fists stay a MeleeDef so holster/stow does not need a gun stub.
enum class WeaponId : std::uint8_t { Fists=0, Shotgun=1, Handgun=2 };

struct HitscanDef {
 float maxRange=18.f;
 float coneBase=.38f;
 float conePerMeter=.018f;
 float dmgNear=34.f,dmgMid=28.f,dmgFar=21.f;
 float nearDist=4.f,midDist=9.f;
 float actorDamage=34.f;
 float closeHitStop=2.5f;
 float ragdollImpulse=2.5f;
 float propImpulse=6.f;
};

struct GunCycleDef {
 float fireCooldown=.55f;
 float reloadTime=.62f;
 float reloadKick=.18f;
 float pumpSoundAt=.12f;
 float boltStart=.08f,boltEnd=.42f;
 float autoHolsterAfter=.42f;
 float raiseHolster=.05f;
 int tubeCapacity=6;
 int pickupAmount=16;
 Sound fireSound=Sound::Shot;
 Sound pumpSound=Sound::Pump;
 Sound emptySound=Sound::Empty;
 // Player magazine weapons (appended, so the positional guard row keeps its
 // meaning). A semi-automatic gun fires once per trigger press. A magazine
 // reload from empty takes emptyReloadTime and ends with the slide released.
 // magOutAt/magInAt/rackAt are fractions of the reload; 0 plays no sound.
 bool semiAuto=false;
 float emptyReloadTime=0.f;
 float magOutAt=0.f,magInAt=0.f,rackAt=0.f;
 Sound magOutSound=Sound::Empty,magInSound=Sound::Empty,rackSound=Sound::Pump;
 const char* caliber="12 GA";
};

struct MeleeDef {
 float damage=28.f;
 float range=1.35f;
 float facingDot=.72f;
 float swingCooldown=.58f;
 float impactAt=.22f;
 float animLen=.48f;
 float guardFrontDot=.3f;
 float guardScale=.3f;
 float vrCooldown=.38f;
 float vrMinSpeed=1.2f;
 float clutterDamage=5.f;
 float paneDamage=15.f,propImpulse=3.f;
 float barrelDamage=150.f;
 float barrelRadius=3.5f;
};

struct WeaponDef {
 WeaponId id=WeaponId::Shotgun;
 const char* label="SHOTGUN";
 HitscanDef hitscan{};
 GunCycleDef cycle{};
};

inline constexpr MeleeDef kUnarmedMelee{};
// The player's firearms, in wheel order. The handgun is the Cleanup sidearm
// taken off a body: a 12-round magazine, a tighter cone and lighter hits than
// the shotgun (about five body hits for a Cleanup soldier).
inline constexpr WeaponDef kWeapons[]={
 {WeaponId::Shotgun,"SHOTGUN",{},{}},
 {WeaponId::Handgun,"HANDGUN",
  {24.f,.2f,.009f,20.f,16.f,12.f,6.f,14.f,20.f,0.f,1.1f,2.8f},
  {.2f,1.5f,.06f,0.f,0.f,0.f,.42f,.05f,12,12,Sound::PistolShot,Sound::Pump,Sound::PistolDry,
   true,1.9f,.14f,.58f,.84f,Sound::PistolMagOut,Sound::PistolMagIn,Sound::PistolRack,"9MM"}},
};

inline constexpr int weaponCount(){return int(sizeof(kWeapons)/sizeof(kWeapons[0]));}
// NPC sidearm uses the same first-hit ballistics, with a compact magazine and
// lower impulse. The player's authored firearm inventory remains separate.
inline constexpr WeaponDef kGuardHandgun{WeaponId::Handgun,"HANDGUN",
 {18.f,.05f,.004f,10.f,8.f,6.f,4.f,9.f,10.f,0.f,.7f,2.f},
 {.32f,1.8f,.08f,0.f,0.f,0.f,.42f,.05f,12,12,Sound::Shot,Sound::Pump,Sound::Empty}};

// NPC meaning: a guard's sidearm is kGuardHandgun, a long gun the shotgun row.
inline const WeaponDef& weaponDef(WeaponId id){
 if(id==WeaponId::Handgun)return kGuardHandgun;
 for(const auto&def:kWeapons)if(def.id==id)return def;
 return kWeapons[0];
}
// Player meaning: the kWeapons row (Fists falls back to the shotgun's cycle).
inline const WeaponDef& playerWeaponDef(WeaponId id){
 for(const auto&def:kWeapons)if(def.id==id)return def;
 return kWeapons[0];
}
constexpr bool playerFirearm(WeaponId id){return id==WeaponId::Shotgun||id==WeaponId::Handgun;}

inline float hitscanDamageAt(const HitscanDef& def,float along){
 return along<def.nearDist?def.dmgNear:(along<def.midDist?def.dmgMid:def.dmgFar);
}

inline WeaponId nextFirearm(WeaponId current,int delta){
 if(weaponCount()<=0)return WeaponId::Shotgun;
 int index=0;for(int i=0;i<weaponCount();++i)if(kWeapons[i].id==current){index=i;break;}
 index=(index+delta)%weaponCount();if(index<0)index+=weaponCount();
 return kWeapons[index].id;
}

}
