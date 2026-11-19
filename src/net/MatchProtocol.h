#pragma once
// The Match's messages: the first byte of every packet (Match.cpp, MatchPlay.cpp). Bump ProtocolVersion
// (Match.h) whenever one changes shape.
#include <cstdint>
namespace retro::net::protocol {
enum Message:std::uint8_t{
 // Control (reliable)
 Hello=1,        // guest -> host: u16 version, u8 character, str name, u8 slot hint (255 none), u8 already playing
 Welcome=2,      // host -> guest: u8 slot, u8 mode, u16 frag limit, u16 time limit (s), u8 friendly fire, u8 max players,
                 //   str map file, str map hash, u8 map index, u8 round, u32 host clock (ms)
 MapRequest=3,   // guest -> host: send me the map
 Ready=4,        // guest -> host: u8 already spawned (a new host's guest playing on)
 Roster=5,       // host -> all: u8 count, {u8 slot, str peer, str name, u8 character, i16 frags, i16 deaths, i16 ping, u8 host|alive|ready}
 Refuse=6,       // host -> guest: str why
 // State (unreliable, newest wins)
 GuestState=10,  // guest -> host: u16 seq, u32 echoed host ms, u16 held ms, player state
 Snapshot=11,    // host -> all: u32 host ms, u8 round, i16 time left (ds, -1 none), u8 over, u8 count, {u8 slot, player state + health}
 Creatures=12,   // host -> all (co-op): u32 host ms, u8 level, u16 count, {creature}
 Doors=13,       // host -> all (co-op): u8 level, u8 count, {u8 open, u8 opening}
 Actors=14,      // host -> all (co-op): u32 host ms, u8 count, {u16 actor, u8 level, i16 x y z (1/256 m), u8 yaw, u8 clip,
                 //   u8 phase, u8 field count, {u8 field, zigzag varint value}} (MatchCoop.cpp ActorFields)
 // Events (reliable)
 Shot=20,        // guest -> host: u8 weapon, u8 level, f32 x y z, i16 direction x y z, u32 seed, u32 view time (host ms)
 ShotFx=21,      // host -> all: u8 shooter, u8 weapon, u8 level, f32 origin, f32 end, u8 kind (0 world, 1 flesh, 2 nothing), i8 normal x y z
 Hit=22,         // host -> all: u8 victim, u8 attacker (255 the world), u8 damage, u8 health after
 Death=23,       // host -> all: u8 victim, u8 killer (255 the world), u8 weapon, u8 respawn (ds)
 Respawn=24,     // host -> all: u8 slot, u8 life, u8 level, f32 x y z, u16 yaw
 Correct=25,     // host -> guest: u8 life, u8 level, f32 x y z (a move the host would not take)
 PickupTaken=26, // either way: u8 level, u16 index
 DoorUse=27,     // guest -> host: u8 level, u16 index, u8 opening
 Travel=28,      // host -> all (co-op): u8 level, f32 x y z, u16 angle (the host went somewhere new)
 RoundOver=29,   // host -> all: str winner
 RoundStart=31,  // host -> all: u8 round
 PickupsInit=32, // host -> joining guest (co-op): u16 count, {u8 level, u16 index}
 Use=33,         // guest -> host (co-op): u8 level, u16 terminal (a switch, lever or lift call the host works for them)
 ScriptFx=34,    // host -> all (co-op): u8 action type, u32 id, i32 value, u16 sound, f32 amount, str text (a script's
                 //   sound, shake, caption, release, ending; or an item for the guest it is sent to)
 StateSync=35,   // host -> all (co-op): u8 full, u16 count {u32 id, i32 value}, u8 count {u32 objective, u8 status},
                 //   u16 count {u32 property, i32 value}; full replaces, otherwise only what changed
 // MapData (reliable)
 MapChunk=30,    // host -> guest: u32 total, u32 offset, bytes (<= MapChunkBytes)
};
}
