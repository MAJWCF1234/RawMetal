# Creature behaviour test results

Base: f7c4b08. Branch: codex/creature-test. Worktree: N:/rawmetal/.build/codex-wt.

Only the test, its declaration and CLI registration changed. No game behaviour changes.

Commands and results:

- Build.cmd via the requested PowerShell redirection: exit 0. No fallback repack required.
- RawMetal.exe --creature-behaviour-test: exit 21; wall runtime 0.649155 s; test runtime 0.431387 s.
- RawMetal.exe --each-test: exit 1; wall runtime 242.031328 s. All 30 existing tests passed; creature-behaviour alone failed (0.236603 s).
- RawMetal.exe --smoke-test: exit 0; wall runtime 221.941234 s.

Standalone observed results:

```text
PASS 1 traits: HUNTSMAN health=110 reach=1.05 gaitPerMetre=7 painDecay=5 deathSeconds=1.15 idleSeconds=2.5 rushSpeed=0
PASS 1 traits: XENOWASP health=85 reach=1.05 gaitPerMetre=7 painDecay=5 deathSeconds=1.15 idleSeconds=2.5 rushSpeed=0
PASS 1 traits: SCISSOR FIEND health=280 reach=1.25 gaitPerMetre=7 painDecay=5 deathSeconds=1.15 idleSeconds=2.5 rushSpeed=0
PASS 1 traits: REACTOR STALKER health=320 reach=1.1 gaitPerMetre=7 painDecay=5 deathSeconds=1.15 idleSeconds=2.5 rushSpeed=3.8
PASS 1 traits: MUTATED HUMAN health=180 reach=1.05 gaitPerMetre=7 painDecay=5 deathSeconds=1.15 idleSeconds=2.5 rushSpeed=0
PASS 2 dormancy: player visible=1 3s max displacement=0 turn=0 awareness=0 wakeState=1330713
PASS 2 state wakes and approaches: awareness at=0.01s distance=3 -> 2.92535 in 2s
PASS 3 damage wakes: hp=139 maxHp=140 awareness=12 awake at=0.01s
FAIL 4 Snatcher stalks under continuous gaze: player visible=1 Watch for first 1s=1 first non-Watch=-1s distance at 6s=6 timeline=0.01s=Watch 
PASS 5 Snatcher Rush pace: Rush throughout=1 displacement=1.14999m / 0.25s speed=4.59995m/s (3.8..5.2)
PASS 6 Chimera windup hit and cooldown: windup start=0.01s duration=0.91s first strike=0.92s damage=28 second strike=3.84s interval=2.92s hits over 4s=2 distance=1.384
PASS 7 save/load dormancy and state wake: decode=1 Snatcher found=1 saved wakeState=1330713 max displacement/turn/awareness over 3s=0/0/0 displacement after state +2s=0.074655
runtime_seconds=0.431387
```

Check 4 passes the initial Watch requirement but fails progression: continuous gaze holds Watch for all 6 seconds, with distance unchanged at 6 m. The timeline contains only 0.01s=Watch; first non-Watch=-1 means no transition was observed. This is reported without changing AI.

Dormancy, wake, Rush speed and Chimera measurements use updateEnemies at 100 Hz in the existing validationScene intake aisle. The dormant player faces away, leaving the player visible to the creature while allowing stalking movement after wake. The stalking check keeps the player looking straight at the Snatcher for all 6 seconds. Rush is seeded as an already active mode to isolate speed from entry rules. Save/load identifies the script-owned Snatcher and isolates it from the restored authored population before the update probe.

Independent suite:

```text
PASS hazmat 0.371083s
PASS creature-animation 0.064385s
PASS cleanup-tank-aim 0.307074s
PASS combat 0.586417s
PASS weapon-motion 1.38982s
PASS audio-events 0.544276s
PASS settings 0.264153s
PASS inventory 0.0307522s
PASS flashlight 0.118085s
PASS pickups 0.0297009s
PASS movement 1.40515s
PASS progression 4.45142s
PASS ai 18.7936s
FAIL creature-behaviour 0.236603s
PASS gantry 8.091s
PASS lift 36.2994s
PASS reactor 8.21683s
PASS saves 70.2278s
PASS systems 0.323832s
PASS console 0.887611s
PASS lift-mix 6.29413s
PASS clutter 2.04427s
PASS service-maps 1.09702s
PASS streaming 1.7338s
PASS unarmed 0.320441s
PASS npc-voices 4.7231s
PASS mechanisms 0.499285s
PASS campaign-maps 47.9939s
PASS chapter-route 11.4619s
PASS chapter-sequence 3.08336s
PASS darkness 9.98485s
FAILED 1
```
