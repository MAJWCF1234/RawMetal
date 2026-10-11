#pragma once
#include "../core/Math.h"
#include <limits>
namespace retro {
enum class Sound {
 Shot, Pump, Metal1, Metal2, Metal3, Metal4, Concrete1, Concrete2, Concrete3, Concrete4,
 Jump, Land, Pickup, Hurt, SpiderCall, SpiderAttack, SpiderDeath, WaspCall, WaspAttack,
 WaspDeath, BruteCall, BruteAttack, BruteDeath, Music, Machine, Wings, Empty, Exit, Door, PunchSwing, PunchHit, JunkMetal, JunkGlass, JunkSoft, LiftCrash, LiftMotor, LiftCreak, ReactorMusic, LiftSnap, WaterReturn, FreightMusic, WorkerDying, Count
};
// A finite z captures the emission origin even when the player's own sound is
// mixed at the listener. NaN retains legacy events without authored elevation.
struct SoundEvent { Sound sound; Vec2 position{}; float gain=1, pitch=1; bool spatial=false; float radius=17; float z=std::numeric_limits<float>::quiet_NaN(); };
}
