# Guard memory, factions and squad decisions — October 11, 2026

This pass addresses the main gaps identified after the armed-guard work:
forgotten threats after brief occlusion, permanently failed patrol points,
player-only targeting, independent squad members and missing close-range and
defensive decisions. These are shared engine systems with supported authoring.

## Resulting behavior

- Confirmed entity identity survives brief occlusion. Reacquisition still requires
  a fresh weapon raise and aim; new or forgotten entities require recognition.
  Investigation/search expires, including for permanently injured guards. Memory
  uses last-observed coordinates instead of hidden target movement.
- `ACTOR_FACTION` configures Player, Facility, Creature, Raider and Neutral
  relationships. Guards select living players, NPCs, enemies or scripted actors;
  explicit personal attackers can provoke retaliation. Friendly guards do not
  block player enemy-clear gates. Civilian/escort fear respects factions; actual
  injury still provokes escape.
- Named squads share fresh sightings within 18 m without fabricating personal
  visibility. At most two eligible guards hold firing leases, rotated fairly
  with time for the maximum authored three-second aim. Reserved flank positions
  are attempted once per encounter; blocked or five-second failed flanks return
  to normal firing tactics instead of stalling or orbiting.
- Armed guards can bash through a 300 ms interruptible windup for 12 damage and
  900 ms recovery. Bash spends no ammunition, cannot cross walls and supports
  frontal fist guarding. Shared weapon/hand attachment follows its short lunge.
- Empty tubes, recent damage and low health request reachable cover shielding
  head and body. Losing sight during reload preserves protection. Permanent
  injury does not freeze memory: after transient defense ends, the existing
  eight-second investigation deadline and authored search bound forgetting.
- Patrols skip a waypoint after five seconds without meaningful navigation
  progress; legitimate detours and pauses retain their behavior. Save format 13
  preserves clocks, references, ammunition, leases and destinations. Invalid
  flags/references and contradictory reload states are rejected.

Cover/flank searches retain the shared four-search frame budget, eight candidates
per attempt and 600–780 ms retry backoff. This bounds work; it is not a measured
FPS increase. Renderer pipelines, imported assets and campaign-map counts are unchanged.

## Actual custom-map and visual checks

The evolving **Custom Maps → RawMetal // Ashfall Guard Lab** retains its original
solo encounter and adds an opt-in southern squad/creature experiment. Enter
`X17–17.8, Y12–13` at ground level, then retreat behind the original hiding cover.
The JSON/TXT pair matches the clean HEAD editor with the updated runtime-record
helper: **29 records, seven editor tests passed, zero export warnings**.
See [playground instructions](ashfall-playground.md).

`--guard-policy-inspection` loads that actual TXT and activates the authored
region through normal updates. It verifies shared recognition, two real damaging
shots and protected reload movement. The final stage deliberately empties the
actual shooter's tube to demonstrate retreat before the stationary target dies.
The hidden player remains at 100 health; target damage reaches 36.

Four matched Vulkan captures were inspected after widening the initial close
camera. The final observer at `(22.5, 10)` fits the map hull. Original solo patrol,
distraction, cover movement and combat inspections also passed. Existing Worker
appearance is prototype art; the captures establish behavior and poses.

`tools/CompareGuardPolicyRenders.py diagnostics/guard-policy-20261011` creates
PNGs, a contact sheet and difference images. At RGB threshold 12, activation to
firing changes **158,112 pixels**, and firing to reload cover **160,210**, within
the same 739,200-pixel region. These are visible state differences, not rendering
quality percentages. Original cover and muzzle-flash comparison scripts also pass.

## Validation and delivery

All **251 guard checks** passed: 58 patrol/authoring, 61 combat/ballistics, 27 cover
tactics, 80 policy/faction and 25 defense/save checks. Coverage includes 60/120 Hz,
interrupted saves, four-member firing fairness, maximum aiming delay, bystander
safety, actual creature retaliation, protected reloads, faction-aware fear,
unreachable navigation and malformed saves.

Physics/AI, controls, saves, offline VR, campaign maps, mechanisms, streaming,
audio, world isolation and Vulkan smoke passed. Captures use Vulkan on the host's
NVIDIA GeForce RTX 5060 Ti with linear HDR and 4x MSAA. Offline VR tests do not
validate a connected headset. Full GoldSrc parity and long-session playtester
satisfaction remain unproven.

The local rebuilt `N:\rawmetal\RawMetal.exe` is **23,799,296 bytes**, below
28,000,000. All **185 embedded resources** match source losslessly.
SHA-256: `10435D3B6F27D505798E47AA24C08C984C7512A3AA4CF9C784B92BCFF76D886F`.
Build logs retain existing asset-packer, legacy test conversion and vendored
image-library warnings. Evidence is under `diagnostics/guard-policy-20261011/`.
Delivery is a local EXE and GitHub source push; no new binary release is published.
Unrelated editor, asset and diagnostic changes are preserved.
