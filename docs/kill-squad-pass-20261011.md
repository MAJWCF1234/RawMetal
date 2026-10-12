# Cleanup squad development — October 11, 2026

The evolving Ash custom-map playground now exercises six cleanup squad members
across two additional stitched test chunks. The main campaign is unchanged.
Assault, Engineer, Sharpshooter, Heavy, Medic and Rusher use the supplied Cleanupteam models and
textures, standing 1.6 metres tall. The Limitator is not used in this experiment.

## Shared engine behavior

Guards pursue a personally observed last-known position through open chunk seams
and enabled transfer gates. They retain their original actor identity and home
patrol, pin only the needed pursuit route, and physically return after their
search expires. Locked gates, closed doors and incompatible floor heights still
block movement. Cover goals remain local; migration does not redirect a guard
to the hidden player's live position.

Firing, bash and reload phases consume explicit timed action events once. Saves
retain the phase and consumed events. Large time steps and restoration cannot
replay damage, a shell refill or an earlier sound. Optional suppression targets
an observed solid cover point for at most 1.2 seconds while a same-target
teammate is flanking. A character entering the shot line cancels the shot.

The reusable tactical projectile system supports eight active grenades with
fixed 120 Hz physics, gravity, solid/glass/door collision, bounce, a visible fuse,
occluded blast falloff and real attacker attribution. NPC policy adds a 450 ms
throw telegraph, finite inventory, a six-second squad cooldown, bounded arc
prediction and allied/neutral landing checks. Reload, bash, recovery and a
committed shot inhibit throwing. Release reserves recovery and cannot share a
frame with a shot. Active projectiles and throw phases survive saves.

## Models and authoring

Visual IDs 4–9 select Assault, Engineer, Sharpshooter, Heavy, Medic and Rusher. They use their
original UVs and separate body/head PNGs. Six newly authored baked vertex clips
provide idle, walk, attack, pain, death and look motion. The supplied Blender
archive contained meshes and a turntable action, not a combat skeleton or
movement clips; these are our prototype animations. Per-model shoulder, elbow
and wrist calibration binds the existing shotgun, reload and throw poses.

The Engineer is slower and carries two grenades. Assault aims and recovers
faster. Sharpshooter takes longer to aim and recover, with higher shot damage.
Medic uses the supplied pistol with a twelve-round magazine; other members use
the existing shotgun. This pass does not add a sniper rifle. All six share a Raider squad and bounded firing opportunities.

`ACTOR_TACTICS|map|actor|suppress|grenades` opts an existing armed guard into
suppression and 0–4 grenades. Old maps default to neither behavior. These records
and model visual IDs use the ordinary campaign parser and renderer, with no
map-specific combat logic.

Launch **Custom Maps → RawMetal // Ashfall Guard Lab**, restart, and enter the
east passage at Y18–22. The cargo room's western entry activates the squad after
100 ms. Retreat through its southern passage at X17–20 to test pursuit into the
refuge. Supported ceiling lamps, a concrete roof and physical cargo panels make
cover and chunk boundaries visible. The original Ash guard experiment remains.

## Reproduction and evidence

`tools/ImportCleanupActor.py` exports each source role through Blender.
`tools/RefreshKillSquadPlayground.py` regenerates the matching JSON/TXT using the
clean HEAD editor and current shared runtime-record helper, preserving unrelated
local editor work. The export contains 97 runtime records and zero warnings.

`--kill-squad-inspection` loads that actual TXT, enters its real activation
region, and advances normal gameplay to ready, grenade release, a damaging shot
and physical pursuit into the refuge. The camera is positioned after each
milestone. HUD/hands are hidden for observation. No alert or weapon state is
manufactured. `tools/CompareKillSquadRenders.py` converts captures losslessly and
compares the three matched cargo-room views and all six individual role closeups. The refuge view has a different
camera and is excluded from pixel comparisons. Pixel changes demonstrate
visible encounter behavior, not an overall rendering-quality score.

Validation logs and captures are retained under
`diagnostics/kill-squad-20261011/`. Physical headset testing and full GoldSrc AI
parity are not established by this pass.

## Full squad and Engineer integration

`ACTOR_ROLE` configures all six profiles independently of model appearance.
Assault and Rusher take opposite flank assignments. Heavy anchors suppression;
Sharpshooter holds distance and retreats from close contact. Medic walks to a
living injured member of the same faction and squad and spends finite treatment
charges. Support work releases combat leases and cannot overlap attacks.

Engineer assembles the supplied military drone after personally observing a
threat. A drone follows its Engineer through permitted seams, holds a visible
firing position and fires only after a fresh first-hit check. It has finite ammo,
health and repair stock; a stationary Engineer repairs it through unobstructed
space. Deployment/repair, weapon actions and grenades exclude each other.
Player hull collision, ballistics and occluded grenade damage include drones.
All ownership, movement, clocks and resources persist and are validated on load.

The ordinary main-loop deployment path and 60/120 Hz supervision are covered by
`--engineer-drone-test`; `--guard-roles-test` checks cooperation and finite support.
The Vulkan encounter inspection also captures a normally deployed drone.

## Medic kit and handgun

Medic selects the lowest-health eligible living squadmate at half health or
lower, approaches physically and holsters its handgun during a 2.4-second kit
interaction. Completion heals 30 damage and spends one of two saved charges.
Obstruction, patient loss and incompatible combat actions interrupt treatment.
Distance hysteresis prevents millimetre-rounded saves from restarting work.

`tools/BakeMedicTreatment.py` retargets Quaternius Universal Animation Library 2
`Chest_Open` onto the supplied Medic using its calibrated limb/hand weights.
It adds a seventh grounded vertex clip and baked hand anchors. The field kit
follows these anchors; its hinged lid opens, exposes supplies and closes. The
donor and its CC0 license are retained under `source-assets/medic-motion/`.
The six baseline clips remain the existing prototype animations.

The supplied RetroWeaponPack Pistol_01 is imported with its original UVs/texture
and physical scale. Shared actor weapon records accept Shotgun (1) and Handgun
(2); Medic defaults to Handgun. Its short barrel participates in the same
first-hit safety checks, has twelve saved rounds, damage capped at ten, and
does not play the shotgun pump event after firing. This is an NPC sidearm.

`--kill-squad-inspection` now includes opening, treatment and healed captures
following an ordinary damage event on an authored Heavy. It reports the real
patient damage and treatment progress; it never sets support timers or charges.

## Final validation, October 11, 2026

Release build completed with MSVC 19.44 in `.build/squad-release`. The asset
packer verified all 222 embedded resources: exact texture pixels/dimensions and
byte-identical model, animation and audio payloads. `RawMetal.exe` is 26,991,104
bytes, below the 28,000,000-byte ceiling.

All twenty native invocations passed: guard AI, combat, tactics, policy,
defense, pursuit, action events, roles, tactical grenades, Engineer drones,
full smoke, friendly AI, physics/AI, saves, world isolation, streaming,
campaign maps, mechanisms, actual squad encounter inspection and Vulkan.
The ten focused guard/squad/projectile/drone suites contain 490 passing checks
and zero failures. Editor runtime-record tests and map-payload export also pass.

| Focused suite | Passing checks |
| --- | ---: |
| Guard AI | 63 |
| Armed combat | 61 |
| Tactics | 27 |
| Target/squad policy | 80 |
| Defense | 25 |
| Stitched pursuit | 33 |
| Attack/reload action events | 40 |
| Six-role cooperation, Medic and handgun | 36 |
| Tactical grenades | 49 |
| Engineer drones | 76 |

The fourteen Vulkan captures include all six supplied models, live deployment,
grenade release, damaging shots, south-seam pursuit and Medic kit opening,
treatment and completion. Treatment changed the Heavy's actual saved damage
from 60 to 30. Medical captures use a side observer so the patient does not
obscure the kit. Visual inspection confirmed the Medic's pistol and kit motion.

Regression work fixed a human navigation corner cut at 120 Hz, retained the
creatures' separate stair-step policy, and corrected the Medic's cargo-overlapping
spawn. Source hashes for the six transferred Downloads originals still match;
all six Downloads copies are absent. The Limitator remains unplaced in the
ordinary squad encounter. Full GoldSrc parity and physical headset testing remain
unproven; these checks do not substitute for matched Half-Life encounters.
