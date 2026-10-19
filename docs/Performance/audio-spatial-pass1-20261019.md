# Spatial audio first pass - 2026-10-19

Shipping build: `.build/audio-upgrade/shipping/RawMetal.exe`, promoted to root `RawMetal.exe` as v0.7.2-preview.13.
The approved first audio pass is integrated with all sixteen reduced texture resources.

## Changes

- Geometry-driven spectral occlusion: blocked emitters lose treble as well as volume. Fully blocked cutoff is approximately 900 Hz. Clear paths bypass filtering. The existing three-ray occlusion and gain envelope remain; coefficients also smooth at sample rate.
- Positional stereo recordings resolve to one mono emitter before distance/pan processing. Nonspatial music and listener-centered effects retain stereo.
- No new audio assets, dependencies, geometry queries, animation banks, changed voice triggers, or changed cue ordering. Scalar native arithmetic uses the existing Windows waveOut backend and baseline ISA.
- Three floats of filter state per voice (48-voice ceiling). Coefficient exponentials run in spatial update, never per audio sample. No new mixer allocations.

## Validation

Full Vulkan smoke (exit 0), audio regression, lift music/creak/snap/crash transitions, live Windows device playback and 231 embedded resource verification passed. All 80 NPC takes retain expected decoded durations. Closed/open doors, thin floors, remote chunk occlusion and chunk handoffs pass.

Synthetic mixer checks preserve nonspatial stereo while cancelling deliberately opposite channels in a positional point source. At the blocked pole, 120 Hz energy retains 98.27%; 8 kHz retains 1.40%, relative to the clear path. This validates filter behavior, not perceived realism.

48 simultaneous spatial machine loops, 16 warmup blocks then 256 measured 512-frame blocks: **0.251-0.284 ms/block**, versus 11.61 ms of audio output per block (roughly 2.17-2.44% of a core). The final PGO shipping build measured **0.53705 ms/block** in its first validation run (4.63% of a core); these short stress results vary and do not show a speed improvement. CPU mixer wall timing only; excludes game-thread geometry, lock contention and driver scheduling. No matched gameplay FPS claim.

Initial candidate: **24,558,592 bytes**, ordinary Release with PGO OFF. Final shipping build uses the existing trained PGO profile: **24,171,008 bytes / 23.0513 MiB**, with **9,383,424 bytes / 8.9487 MiB** free below 32 MiB. It is 2,560 bytes smaller than preview.12 after link optimization. No matched shipping FPS comparison has been performed.

Listening sample: `.build/audio-upgrade/candidate/spatial-wall-preview.wav`, eight seconds: clear left, blocked left, clear right, blocked right. Each section is two seconds, with the same machine source. Gameplay render: `.build/audio-upgrade/candidate/audio-preview.wav`. The user approved the listening sample before root promotion.

## Remaining engine work

This slice does not add HRTF, interaural delays, reflections, room reverb or portal diffraction. The mixer and game-thread update still share a mutex; independent audio control snapshots and underrun instrumentation should precede more expensive acoustics. Game-wide FPS and deliberate scene-stall playback testing remain follow-up work; no gameplay FPS gain is claimed for this audio patch.
