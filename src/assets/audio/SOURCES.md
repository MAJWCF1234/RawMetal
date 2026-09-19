# Imported RawMetal audio

Copied and converted to 44.1 kHz PCM16 for the embedded stereo mixer. Source files are untouched.

- shot.wav (resource 200): O:\retro official\Audio\Sound Effects\Firearms\Fire\shotgun_fire_ufx_1.ogg
- pump.wav (resource 201): O:\retro official\Audio\Sound Effects\Firearms\Racking\shotgun_racking_ufx_1.ogg
- metal1.wav (resource 202): O:\retro official\Audio\Sound Effects\Player\Footsteps\Metal\footstep_metal_ufx_1.ogg
- metal2.wav (resource 203): O:\retro official\Audio\Sound Effects\Player\Footsteps\Metal\footstep_metal_ufx_2.ogg
- metal3.wav (resource 204): O:\retro official\Audio\Sound Effects\Player\Footsteps\Metal\footstep_metal_ufx_3.ogg
- metal4.wav (resource 205): O:\retro official\Audio\Sound Effects\Player\Footsteps\Metal\footstep_metal_ufx_4.ogg
- concrete1.wav (resource 206): O:\retro official\Audio\Sound Effects\Player\Footsteps\Concrete\footstep_concrete_ufx_1.ogg
- concrete2.wav (resource 207): O:\retro official\Audio\Sound Effects\Player\Footsteps\Concrete\footstep_concrete_ufx_2.ogg
- concrete3.wav (resource 208): O:\retro official\Audio\Sound Effects\Player\Footsteps\Concrete\footstep_concrete_ufx_3.ogg
- concrete4.wav (resource 209): O:\retro official\Audio\Sound Effects\Player\Footsteps\Concrete\footstep_concrete_ufx_4.ogg
- jump-takeoff.wav (resource 210): O:/retro official/Audio/OGG/Player/Footsteps/footstep_concrete_a_3.ogg layered with a quieter, delayed OGG/Player/Misc/equip_1.ogg.
- landing-body.wav (resource 211): O:/retro official/Audio/OGG/Player/Footsteps/footstep_concrete_a_7.ogg with low-pass filtering and a quieter, delayed OGG/Player/Misc/equip_1.ogg.
- pickup.wav (resource 212): O:\retro official\Audio\Sound Effects\Misc\item_pickup_ufx_1.ogg
- hurt.wav (resource 213): O:\retro official\Audio\OGG\Player\Misc\hit_1.ogg
- spider_call.wav (resource 214): O:\retro official\Audio\OGG\Creatures\Mindleech\mindleech_1.ogg
- spider_attack.wav (resource 215): O:\retro official\Audio\OGG\Creatures\Barnacle\barnacle_2.ogg
- spider_death.wav (resource 216): O:\retro official\Audio\OGG\Creatures\Barnacle\barnacle_4.ogg
- wasp_call.wav (resource 217): O:\retro official\Audio\OGG\Creatures\Alien Drone\alien_drone_talking_1.ogg
- wasp_attack.wav (resource 218): O:\retro official\Audio\OGG\Creatures\Alien Drone\alien_drone_roaring_1.ogg
- wasp_death.wav (resource 219): O:\retro official\Audio\OGG\Creatures\Alien Drone\alien_drone_roaring_3.ogg
- brute_call.wav (resource 220): O:\retro official\Audio\OGG\Creatures\Daemon\daemon_roar_1.ogg
- brute_attack.wav (resource 221): O:\retro official\Audio\OGG\Creatures\Daemon\daemon_roar_3.ogg
- brute_death.wav (resource 222): O:\retro official\Audio\OGG\Creatures\Daemon\daemon_roar_7.ogg
- music.wav (resource 223): O:\retro official\Audio\Tracks\ost_melting_spine_mx_1_short_loop.ogg
- machine.wav (resource 224): O:\retro official\Audio\Sound Effects\Machines\old_machine_mx_1_loop.wav
- wings.wav (resource 225): O:\retro official\Audio\OGG\Creatures\Alien Drone\alien_drone_flying_1.ogg
- empty.wav (resource 226): O:\retro official\Audio\Sound Effects\Firearms\Misc\rifle_universal_empty_barrel_ufx_1.ogg
- exit.wav (resource 227): O:\retro official\Audio\Sound Effects\Machines\retro_computer_beep_mx_1.ogg
- door.wav (resource 228): O:/retro official/Audio/OGG/heavy_door_open-close_1.ogg; opening section, trimmed to 1.5 seconds with a 0.2-second fade.
- punch-swing.wav (resource 229): O:/retro official/Audio/Sound Effects/Melee/Swing/universal_swing_miss_light_ufx_1.ogg.
- punch-hit.wav (resource 230): O:/retro official/Audio/Sound Effects/Melee/Hit/blunt_hit_body_ufx_1.ogg.

- junk-metal.wav (231): Sound Effects/Impact & Break/Metal/impact_metal_ufx_2.ogg, first 0.8 seconds after leading silence, with a 0.2-second tail fade.
- junk-glass.wav (232): Sound Effects/Impact & Break/Glass/impact_glass_ufx_3.ogg, first 0.6 seconds after leading silence, with a 0.15-second tail fade.
- junk-soft.wav (233): Sound Effects/Impact & Break/Body/impact_body_ufx_2.ogg at 0.65 gain mixed with Sound Effects/Misc/plastic_container_open_small_ufx_1.ogg at 0.22 gain; first 0.55 seconds after leading silence, with a 0.15-second tail fade.

The junk cues use the same O:/retro official/Audio library. All three are mono PCM16 at 44100 Hz; positional panning and attenuation happen in the mixer. Physics scales impact gain by speed, applies a short per-object retrigger interval, and suppresses stationary contact noise.
