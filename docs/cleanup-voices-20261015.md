# Cleanup and Witness voices

The follow-up uses the user's existing ElevenLabs API access to produce **80
short takes** across Assault, Sharpshooter, Rusher, Heavy, Engineer, Medic and
the Witness. Skinned native front/side renders informed the casting. No cloned
voices or new character models are involved. The credential stays outside the
repository and executable; the game plays embedded audio entirely offline.

## Production

The [catalog](../source-assets/cleanup-voices/catalog.json) records exact text,
voice ID, role, and gameplay cue. [Provenance](../source-assets/cleanup-voices/provenance.json)
records the model, settings, master hashes, clip durations and bank hash.
The source model is `eleven_multilingual_v2`. Cached dry masters remain local in
`.build/npc-voice-masters`; generated masters are reused on subsequent bakes.

Engineer has an exposed face and retains a natural voice. Full respirators get
different band limits, saturation and restrained quantization; Heavy receives
the strongest treatment. Rusher's partial cover gets a lighter filter. Witness
keeps a hazmat filter without removing its frightened delivery. All takes get
bounded peak levels, matching RMS targets and short click-prevention fades.

The bank uses the existing supported 22.05 kHz mono IMA ADPCM WAV decoder. Its
80 clips occupy **1,231,528 bytes before resource packaging**, with no animation
changes or executable ceiling increase. The bank becomes resource 347; the
original Worker timeline voice stays separate.

Run `py -3 tools/BakeNpcVoices.py` to rebake cached masters without API calls.
Explicit `--generate` permits missing-master generation using an environment
key or a private `--key-file`. The tool does not log or retain credentials and
does not automatically retry a failed paid request. Changing text or voice
settings creates a new generation identity rather than silently reusing an old
take. NumPy, SciPy and ffmpeg are offline bake dependencies, not game dependencies.

## Runtime behavior

- Model-based voice selection keeps role voices attached to their actual skin.
- Patrol chatter is staggered and sparse. Investigation and alert use real
  awareness transitions; retreat uses actual evacuation orders.
- Medic treatment, Engineer pad work and repair require active work. Drone
  deployment is acknowledged only after a real drone is created.
- Follow, wait and danger are driven by the Witness's escort/fear states.
- Injury and dying lines replace the same speaker's previous take. Downed
  actors cannot keep making tactical calls. Third-person casualty reports are
  generic acknowledgments from a living nearby squadmate.
- The mixer allows at most two simultaneous speakers and keeps positional
  attenuation, elevation, occlusion, effects volume, pause and mute behavior.
- Saved cooldowns, variants and edge history prevent reload chatter bursts.
  Spoken lines do not create new AI hearing events or recursive alerts.

The original requested named casualty reports remain in the casting sheet as
direction; the shipped bank uses personal dying lines and living generic
casualty acknowledgments. The Medic's claim about pulling wounded was changed
to covering wounded, and Engineer's pad line describes checking it. No new
dragging or equipment action is implied by those lines.

## Validation scope

All 80 filtered/encoded clips were transcribed with ElevenLabs Scribe v2. The
word-sequence comparison differs only at “sightline” / “sight line” and
“a dressing” / “addressing.” This checks intelligibility, not acting quality,
and does not substitute for a human listening pass. Native audio validation
also decodes every embedded take and checks expected duration, non-silent
signal, speaker replacement, concurrency and mixer output. An exported native
audition plays Assault, Engineer, Sharpshooter, Heavy, Medic, Rusher, Witness.

The dedicated `--npc-voice-test --smoke-test` covers model selection, voice
priority, damage, death, saved history, hearing isolation and Witness state
transitions. The full smoke includes this regression and the audio checks.

Final release: **26,510,336 bytes (25.28 MiB)**, leaving **7,044,096 bytes
(6.72 MiB)** under 32 MiB. All **230** embedded resources verify against source.
Dedicated voice, audio, campaign, role, patient, Engineer drone, Exclusion and
stealth tests pass, as does full Vulkan smoke. Generation plus transcription
used **1,983 credits**. The credential scan of changed/new workspace files is
clean. No network credentials are needed to play.

[Listen to the native audition](evidence/cleanup-voices-20261015/voices-preview.mp3)
(Assault, Engineer, Sharpshooter, Heavy, Medic, Rusher, Witness). The compact
preview is MP3-encoded from the native mixer's exported WAV for review; it is
not the shipped bank's encoding. [Test logs, executable hash and results](evidence/cleanup-voices-20261015/validation.json).
