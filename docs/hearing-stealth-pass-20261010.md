# 3D hearing and stealth slice — October 10, 2026

Sound events now retain their emission position and height. Previously spatial
sounds used a guessed floor elevation, quieter footsteps still alerted enemies
out to the same radius, and directly hitting an enemy with a thrown object exposed
the hidden player's position. These behaviors are repaired in the shared engine.

Footsteps capture foot-contact height, shots capture view/VR weapon height, and
object impacts capture corrected contact positions. NPC voices, sequence ticks,
compactor impacts and crate destruction use their own sources. Existing receiving
worker voice/footstep and overhead-rack cues now author their elevation. No new
maps, assets or renderer settings were added.

Gain scales effective hearing radius; standing movement can be detected where
equivalent crouching stays quiet. Visible targets retain priority. Distractions
lead to the actual impact and support elevation, and losing sight retains the
last visible position rather than tracking a hidden player's movement.

Hearing and stereo playback share `Game::soundRayClear`. It splits traces at
actual chunk bounds, uses cached exact static intersections for thin floors and
structures, and performs bounded door/platform checks. Static rooms with no
dynamic geometry avoid those samples. Out-of-range AI stimuli and inaudible audio
voices skip tracing. This removes repeated per-sample chunk searches; it is not
an overall FPS claim.

`SoundEvent.z` is appended without disturbing existing aggregate fields. Legacy
unspecified heights retain fallback behavior. New `SEQUENCE_CUE` records optionally
append absolute Z after the caption; existing ten-field records still load and
nonfinite authored heights are rejected. Save format remains 13: sound events are
transient, and the existing last-known-position fields already store elevation.

Machine emitters still provide X/Y only and retain planar playback. The mixer
provides stereo positional attenuation and occlusion, not HRTF binaural audio.
Enemy simulation remains scoped to the active player chunk; this slice does not
implement human guard patrols or chapter-wide background AI. External source
assets are temporarily unavailable, and this work uses the project's existing set.

All 31 hearing assertions and offline audio regressions passed. Physics/AI,
world isolation, mechanisms, campaign maps, saves, controls, offline VR,
lift audio and Vulkan smoke checks returned zero. The executable is
23,472,640 bytes, below the 28,000,000-byte limit, and all 185 embedded
resources were verified lossless by the Release packer. Raw results are in
[the evidence directory](hearing-stealth-pass-20261010/).
The hearing fixtures exercise actual movement/physics and script execution,
including separate floors, a four-centimetre deck, hidden throws, tracked gun
origins, neighboring chunk geometry, source handoff and old/new cue parsing.
