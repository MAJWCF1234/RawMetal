#pragma once
// The sound bank: library sounds a map can place by name (AMBIENT records, the
// campaign builder's Sound tool). One row per sound of Sound.h's bank block, in
// the same order as that block and as tools/audio/import_horror_sfx.py's BANK
// (which makes resources 2370 on). A loop is a bed that plays for as long as its
// emitter is in earshot; the rest are single sounds an emitter plays now and
// then or when its state is set. gain and radius are the defaults the builder
// starts a new emitter with. The builder reads this table from the source file,
// so keep each row on one line in this exact shape.
#include "Sound.h"
#include <string_view>
namespace retro {
struct BankSound {Sound sound;const char* name;bool loop;float gain,radius;const char* label;};
inline constexpr BankSound BankSounds[]={
 {Sound::AmbVent,"vent_airflow",true,0.5f,9,"Vent airflow"},
 {Sound::AmbAirConditioner,"air_conditioner",true,0.4f,8,"Air conditioner"},
 {Sound::AmbFan,"fan_large",true,0.5f,10,"Large fan"},
 {Sound::AmbLightBuzz,"light_buzz",true,0.3f,6,"Light buzz"},
 {Sound::AmbFluorescent,"fluorescent",true,0.35f,7,"Fluorescent tube"},
 {Sound::AmbStatic,"static_electrical",true,0.3f,7,"Electrical static"},
 {Sound::AmbRadioStatic,"radio_static",true,0.3f,7,"Radio static"},
 {Sound::AmbWaterDrip,"water_drip",true,0.6f,8,"Water dripping"},
 {Sound::AmbGasLeak,"gas_leak",true,0.45f,8,"Gas leak"},
 {Sound::AmbDrone,"drone",true,0.4f,18,"Low drone"},
 {Sound::AmbHaunting,"haunting",true,0.38f,16,"Haunting air"},
 {Sound::AmbCrying,"crying",true,0.3f,14,"Crying, far off"},
 {Sound::AmbMechanical,"mechanical",true,0.4f,12,"Machinery, uneven"},
 {Sound::AmbMetalTapping,"metal_tapping",true,0.35f,10,"Metal tapping"},
 {Sound::OneMetalCreak,"metal_creak",false,0.6f,16,"Metal creak"},
 {Sound::OneMetalMoan,"metal_moan",false,0.6f,16,"Metal moan"},
 {Sound::OneMetalBang,"metal_bang",false,0.7f,18,"Metal bang"},
 {Sound::OneMetalDrag,"metal_drag",false,0.6f,16,"Metal dragged"},
 {Sound::OneMetalFall,"metal_fall",false,0.7f,16,"Metal falling"},
 {Sound::OneGlassBang,"glass_bang",false,0.7f,14,"Banging on thick glass"},
 {Sound::OneDistantYell,"distant_yell",false,0.5f,22,"Distant yell"},
 {Sound::OneDistantYell2,"distant_yell_2",false,0.5f,22,"Distant yell, long"},
 {Sound::OneHouseCreak,"house_creak",false,0.6f,12,"Structure creak"},
 {Sound::OneDebris,"debris",false,0.6f,12,"Debris falling"},
 {Sound::OneMonsterGroan,"monster_groan",false,0.6f,18,"Groan"},
 {Sound::OneMonsterBreath,"monster_breath",false,0.55f,10,"Breath"},
 {Sound::OneMonsterGrowl,"monster_growl",false,0.6f,18,"Growl"},
 {Sound::Sting,"sting",false,0.8f,30,"Stinger"},
 {Sound::StingPiano,"sting_piano",false,0.8f,30,"Stinger, piano"},
 {Sound::StingScratch,"sting_scratch",false,0.7f,30,"Stinger, scratch"},
 {Sound::PaAccessDenied,"pa_access_denied",false,0.7f,16,"Voice: access denied"},
 {Sound::PaAccessGranted,"pa_access_granted",false,0.7f,16,"Voice: access granted"},
 {Sound::PaDoorsLocked,"pa_doors_locked",false,0.7f,16,"Voice: doors locked"},
 {Sound::PaDoorsUnlocked,"pa_doors_unlocked",false,0.7f,16,"Voice: doors unlocked"},
 {Sound::PaContainmentBreach,"pa_containment_breach",false,0.7f,20,"Voice: containment breach"},
 {Sound::PaEvacuate,"pa_evacuate",false,0.7f,20,"Voice: evacuate immediately"},
 {Sound::PaCautionHazard,"pa_caution_hazard",false,0.7f,18,"Voice: caution, hazardous area"},
 {Sound::PaVentilationOffline,"pa_ventilation_offline",false,0.7f,18,"Voice: ventilation offline"},
 {Sound::PaUnauthorized,"pa_unauthorized",false,0.7f,18,"Voice: unauthorized personnel"},
 {Sound::PaSystemLockdown,"pa_system_lockdown",false,0.7f,18,"Voice: system lockdown"},
 {Sound::PaBackupGenerators,"pa_backup_generators",false,0.7f,18,"Voice: backup generators online"},
 {Sound::PaHostileDetected,"pa_hostile_detected",false,0.7f,20,"Voice: hostile creature detected"},
};
static_assert(sizeof(BankSounds)/sizeof(BankSounds[0])==size_t(Sound::Count)-size_t(Sound::AmbVent),"one SoundBank row per bank sound, in Sound.h's order");
inline const BankSound* bankSound(Sound sound){
 const int index=int(sound)-int(Sound::AmbVent);
 return index>=0&&index<int(sizeof(BankSounds)/sizeof(BankSounds[0]))?&BankSounds[index]:nullptr;
}
inline const BankSound* bankSound(std::string_view name){
 for(const auto& row:BankSounds)if(name==row.name)return &row;
 return nullptr;
}
}
