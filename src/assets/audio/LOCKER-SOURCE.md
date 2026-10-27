# Locker sounds

The six locker one-shots (resources 2140-2145, `Sound::LockerLatch` to
`Sound::LockerKnock`) are cut from the licensed packs under
`O:/retro official/Audio` (Rust & Blood v1.1d / ROT v1.9 sound effects, Echoes
v2.1 Pure Horror; licences beside this note). Each is decoded with ffmpeg to
44.1 kHz mono, trimmed, given a 1 ms fade-in and a squared fade-out, peak
normalised and written as PCM16 WAV:

| file | source | start | length | fade | peak |
| --- | --- | --- | --- | --- | --- |
| `locker-latch.wav` | `Sound Effects/Buttons & Levers/lever_mx_1.ogg` | 0.00 s | 0.36 s | 0.08 s | 0.55 |
| `locker-creak.wav` | `Sound Effects/Doors & Gates/rusty_gate_mx_1.ogg` | 0.02 s | 0.62 s | 0.18 s | 0.50 |
| `locker-shut.wav` | `Sound Effects/Impact & Break/Metal/impact_metal_ufx_1.ogg` | 0.00 s | 0.62 s | 0.20 s | 0.70 |
| `locker-bang.wav` | `Sound Effects/Impact & Break/Metal/impact_metal_ufx_4.ogg` | 0.00 s | 0.95 s | 0.35 s | 0.90 |
| `locker-sting.wav` | `OGG/Pure Horror/One-shots/stinger_ph_1.ogg` (Echoes) | 0.00 s | 1.00 s | 0.35 s | 0.70 |
| `locker-knock.wav` | `Sound Effects/Melee/Hit/blunt_hit_metal_ufx_1.ogg` | 0.03 s | 0.42 s | 0.12 s | 0.75 |

Nothing here comes from `Complex` or `Audio/folley`.
