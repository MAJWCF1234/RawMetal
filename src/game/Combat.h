#pragma once
#include "../audio/Sound.h"
#include <cstdint>
namespace retro {

// Compact combat tables for industrial-horror loadouts. Add a WeaponDef row to
// expand firearms; fists stay a MeleeDef so holster/stow does not need a gun stub.
enum class WeaponId : std::uint8_t { Fists=0, Shotgun=1 };

struct HitscanDef {
 float maxRange=18.f;
 float coneBase=.38f;
 float conePerMeter=.018f;
 float dmgNear=34.f,dmgMid=28.f,dmgFar=21.f;
 float nearDist=4.f,midDist=9.f;
 float actorDamage=34.f;
 float closeHitStop=2.5f;
 float ragdollImpulse=2.5f;
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
inline constexpr WeaponDef kWeapons[]={
 {WeaponId::Shotgun,"SHOTGUN",{},{}},
};

inline constexpr int weaponCount(){return int(sizeof(kWeapons)/sizeof(kWeapons[0]));}

inline const WeaponDef& weaponDef(WeaponId id){
 for(const auto&def:kWeapons)if(def.id==id)return def;
 return kWeapons[0];
}

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
