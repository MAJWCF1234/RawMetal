# Engine AI and state-storage pass — October 9, 2026

Civilian and escort behavior is implemented in the shared actor system, with
explicit opt-in for authored campaigns. Existing cinematic workers remain
scripted. No campaign maps or assets were added.

The behavior pass includes PC/VR Follow/Wait commands, follow-distance
hysteresis, collision-aware movement, wall detours, ordinary door requests,
locked-door restrictions, visible-threat fleeing and short fear memory. Enemy
creatures can perceive and attack enabled living friendly actors. NPC bodies
block player/enemy hulls on their own floor. Saved position, commands, fear and
animated death restore without restarting the actor timeline.

Navigation work is limited to four full waypoint searches per mechanisms tick,
with staggered retries and short local steering probes. Crowded tests cover
both reachable and unreachable goals, ensuring later actors receive a route
and deferred actors remain stationary.

The next engine improvement replaces linear script-state lookup with binary
search and raises the bounded saved-state capacity from 1,024 to 8,192 entries.
Older insertion-ordered saves load correctly; duplicate state IDs are rejected.
In the final isolated benchmark, 250,000 lookups across 4,095 flags took
12.63 ms versus 198.25 ms for the linear reference, with identical results.
This is about 15.7 times faster for these lookups, not an overall FPS claim.

The release build passed lossless verification for all 185 embedded resources.
The local executable is 23,408,128 bytes, below the 28,000,000-byte limit.
AI/physics, world isolation, mechanisms, campaign maps, saves, combat controls,
offline VR and Vulkan smoke checks passed. Raw results are in
[the evidence directory](ai-engine-pass-20261009/).

See [AI authoring and behavior](ai-perception.md) for the `ACTOR_AI` record.
Following currently operates within the NPC's authored chunk; automatic escort
migration between stitched chunks is not implemented. Live controller testing
and a broader playtest remain necessary to assess overall AI quality against
GoldSrc. This pass is a local build, not a newly published release.
