#pragma once
#include "../core/Math.h"
#include <limits>
#include <cstdint>
namespace retro {
enum class Sound {
 Shot, Pump, Metal1, Metal2, Metal3, Metal4, Concrete1, Concrete2, Concrete3, Concrete4,
 Jump, Land, Pickup, Hurt, SpiderCall, SpiderAttack, SpiderDeath, WaspCall, WaspAttack,
 WaspDeath, BruteCall, BruteAttack, BruteDeath, Music, Machine, Wings, Empty, Exit, Door, PunchSwing, PunchHit, JunkMetal, JunkGlass, JunkSoft, LiftCrash, LiftMotor, LiftCreak, ReactorMusic, LiftSnap, WaterReturn, FreightMusic, WorkerDying, Alarm, AlarmDistant,
 // Open-zone ambience beds and positional loops, then footsteps on painted ground.
 ZoneWind, ZoneForest, ZoneCrickets, ZoneFrogs, FireCrackle, AnomalyHum,
 StepDirt1, StepDirt2, StepDirt3, StepDirt4, StepGrass1, StepGrass2, StepGrass3, StepGrass4, StepGravel1, StepGravel2, StepGravel3, StepGravel4,
 // Area score: one loop per part of the facility (MusicCue).
 MusicUndercover, MusicSubliminal, MusicPitchBlack, MusicHeat, MusicDarkness, MusicLastChapter, MusicDeathClose,
 // The sting under an area title card.
 AreaTitle,
 // Hiding lockers: the latch, a hinge creak, the leaves shutting, a guard
 // ripping them open (and its sting), and a knock from inside.
 LockerLatch, LockerCreak, LockerShut, LockerBang, LockerSting, LockerKnock,
 // Being seen: the player's sting on a guard's '!', the tick as his '?' goes up, and a guard's radio keying.
 DetectSting, SuspicionTick, RadioSquelch,
 // The loop under ALERT and EVASION.
 MusicAlert, GlassShatter, BottleBreak, WoodBreak, WoodImpact, PlasterBreak,
 // The player's handgun: the shot, the dry click, the magazine out and in, the slide released.
 PistolShot, PistolDry, PistolMagOut, PistolMagIn, PistolRack,
 // A live lead's clip arcing as it tears off, and the crackle after (leadArc keeps its sparks in time).
 LeadArc, Count
};
// AudioEngine maps every sound from ZoneWind on to consecutive library resources
// and AreaTitle, the locker sounds, the detection sounds and the alert loop to their own: a sound appended after them
// needs its resource mapped there explicitly, or it would load an unrelated file.
// GlassShatter to PistolRack take consecutive resources from 2255 (the pistol from 2260); LeadArc is mapped on its own.
static_assert(int(Sound::Count)==int(Sound::LeadArc)+1,"map the new sound's resource in AudioEngine");
inline bool gunshotSound(Sound s){return s==Sound::Shot||s==Sound::PistolShot;}
// Footsteps: the facility's metal and concrete plus the zone's dirt, grass and gravel.
inline bool footstepSound(Sound s){return (s>=Sound::Metal1&&s<=Sound::Concrete4)||(s>=Sound::StepDirt1&&s<=Sound::StepGravel4);}
// A finite z captures the emission origin even when the player's own sound is
// mixed at the listener. NaN retains legacy events without authored elevation.
// aiRadius: how far the AI hears it (0: by its kind and gain; below 0: never,
// a sound for the player's ears alone).
struct SoundEvent { Sound sound; Vec2 position{}; float gain=1, pitch=1; bool spatial=false; float radius=17; float z=std::numeric_limits<float>::quiet_NaN(); int speech=-1; size_t speaker=0; int speechPriority=0; float aiRadius=0;int owner=-1; };
}
