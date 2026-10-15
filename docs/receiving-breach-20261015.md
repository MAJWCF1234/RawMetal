# Receiving containment breach

Receiving 02's first detail is an early containment disaster. It is one of
multiple Cleanup squads in the fiction, rather than the campaign's main squad
showdown. The existing cache, chain distraction, cover and six baked roles stay
in this room. No areas, models or animation banks are added.

After two genuine player kills in this detail, three Brutes and three Huntsmen
enter ordinary creature AI. The selected living survivor, preferably Engineer,
abandons combat and physically navigates the south seam into Receiving 03. The
pressure door closes once the survivor clears it. The player can reopen that
ordinary door. The other fighters die through actual creature attacks, without
scripted death, teleportation or invulnerability. Deliberately killing the
evacuee can therefore prevent the intended lone-survivor outcome. Incapacitations
do not count as the two lethal opening picks.

The shared guard executor owns evacuation through its existing Escape task.
Saved destination fields preserve the route across reloads and streaming. The
actor runs rather than stopping to shoot, deploy drones or crouch under ordinary
gunfire; real low passages still require crouching.

## Validation

The campaign regression stages two lethal hits through the real player damage
path, saves and reloads the active breach, then simulates 60 seconds of normal
combat and navigation. It requires exactly five dead members, one living member
in Receiving 03, and a sealed retreat. Its trace records every member's actual
chunk, damage and arrival state. This is a deterministic developer replay, not
a claim that a human playtester produced these results.

Native inspection views 10 and 11 replay those same opening damage events and
observe the resulting room and survivor. The opening shots are staged; later
deaths and movement are simulated by the game. View 8 retains the opening room.

[Native aftermath](evidence/receiving-breach-20261015/receiving-aftermath.png)
shows the creatures controlling Receiving. [The survivor](evidence/receiving-breach-20261015/receiving-survivor.png)
is physically inside Receiving 03. The camera for these stills is repositioned
after the replay; it is not a continuous human playthrough. The body count is
proven by the actor-state trace, rather than by counting bodies in these views.

Current release: **25,639,424 bytes**, leaving **7,915,008 bytes** below the
unchanged 32 MiB ceiling. All **229** embedded resources verify against source.
Save, campaign, guard danger, schedule, table, role and Exclusion regressions
exit 0, as does full Vulkan smoke. The active breach is actually reloaded before
simulating its outcome. [Results and executable hash](evidence/receiving-breach-20261015/validation.json).

Save format 14 adds explicit provenance for script-spawned creatures. The older
authored-population reconciliation discarded dynamic waves on reload. New saves
retain living, injured and dead dynamic creatures while still reconciling
authored enemies against current content. Versions 1 through 13 remain accepted;
they did not record dynamic provenance and cannot recover waves already lost.

## Voice work

Actual skinned front and side renders were inspected before casting. Engineer
is bare-faced; the others have different degrees of face covering. The
[dialogue and casting sheet](cleanup-voice-direction-20261015.md) records those
differences. ElevenLabs showed available credits, but its voice library and
settings failed to load. No new voice audio has been generated or shipped in
this pass. Existing audio remains in use.
