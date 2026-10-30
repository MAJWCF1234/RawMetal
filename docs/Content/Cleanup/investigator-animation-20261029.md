# The company Investigator — 29 October 2026

The Depthworks Internal Investigations officer joins the direct-Mixamo
Cleanup bodies as **`ActorVisual::Investigator` (15)**, body **6** in
`cleanupActorBody`. It is human, drives with the guard and civilian AI, and
stands **1.8 m** at `scale` 1.8. Armed, it carries the handgun (`weapon` 2).

Source: the purchased long-coat model (`source-assets/extra-actors/investigator/`)
on its own verified 65-bone rig, and its own Mixamo downloads
(`source-assets/mixamo-motion/investigator-direct/`, raw FBXs local).

## Clips

| ID | Use | Source | Seconds |
| ---: | --- | --- | ---: |
| 0 | idle (loop) | `locomotion-pack/idle` | 8.29 |
| 1 | walk (loop) | `male-locomotion-pack/walking` | 1.04 |
| 2 | pistol fire | `special/shooting-pistol`, centre 0.25 s | 0.25 |
| 3 | hit | `male-injured-pack/injured hurting idle`, 0-0.5 s | 0.50 |
| 4 | death | `special/dying-backwards` | 3.38 |
| 5 | pistol ready (loop) | `pistol-handgun-locomotion-pack/pistol idle` | 1.33 |
| 6 | radio (loop) | `special/picking-up-object…one-hand`, 1.7-2.2 s, out and back | 1.00 |
| 7 | reading a card (loop) | `special/picking-up-an-object`, 5.0-6.1 s, out and back | 2.20 |
| 8 | pointing | `special/picking-up-object…one-hand`, 0-0.55 s; hold phase 1 | 0.55 |
| 9 | directing | `gestures-pack-basic/dismissing gesture` | 3.13 |

Standing poses only: the coat folds sharply in a kneel or prone. Slot 5,
the squad's crouch, is therefore a standing two-hand pistol ready; a
crouched Investigator stays at full height. The packs hold no radio,
document or pointing take, so 6-8 are held moments cut from the pick-up
takes (a fist held up at the chest under the mask, as with a handheld set; head bowed over the hand at the chest; arm
straight ahead). Loops cut from the middle of a take play out and back so
the seam is one sample.

Scripted actors name clips 0-9 in `ACTOR_KEY`; other humans stay at 0-5.
The guard path layers 0/1/5 legs under the aimed upper body and uses 2 to
fire, like the squad.

## Resources

2200 `investigator.obj`, 2201 `investigator-animation.bin`, 2202
`investigator.png` (one atlas for both material slots), 2203
`investigator-skin.bin`. Maximum quantization error 0.86 mm; loop seams 0.4 mm.

## Reproduce

```powershell
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --python-exit-code 1 --python tools/BakeCleanupDirect.py -- --roles investigator --source-root N:/rawmetal
RawMetal.exe --investigator-inspection
RawMetal.exe --cleanup-layer-inspection --cleanup-layer-role=6
```
