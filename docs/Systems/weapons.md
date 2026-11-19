# Weapons: the MAC-11 and spent cases

The player's firearms are rows of `kWeapons` (`src/game/Combat.h`), in wheel
order: the shotgun, the handgun, the MAC-11. Fists are a `MeleeDef`.

## The MAC-11 (WeaponId::MachinePistol, 3)

A full-auto machine pistol: 32 rounds of .380 at 18 a second.

- **Numbers** (its `HitscanDef`, `GunCycleDef`): 13 / 9 / 5 damage near / mid /
  far (under 4 m, under 10 m, beyond), 11 to a person (a Cleanup soldier in 8-10
  hits, about half a second), 20 m range. The handgun hits for 20.
- **Full auto** (`GunCycleDef::fullAuto`): held, it fires every `fireCooldown`
  and carries the remainder, so the rate holds at any frame rate (18 a second at
  60 and 144 Hz). Each round climbs the view by `recoilClimb` (pitch units, 140 a
  radian) and wanders it by up to `recoilYaw`. When the trigger is let go,
  `recoilReturn` (60%) of the climb comes back. The spread blooms by
  `spreadPerShot` up to `spreadMax` and settles at `spreadSettle` once firing
  stops. It is wider on the move and in the air, and `spreadAimed` (0.45) of it
  down the sights (`Game::shotSpread`). Rounds leave anywhere on the cone's
  disc.
- **Reloads**: tactical takes 1.35 s; from empty it takes 1.9 s and ends with the
  bolt racked home (`rackAt`). The knob stays back on an empty magazine
  (`Game::slideLocked`, shared with the handgun's slide). A dry trigger clicks
  once a press.
- **Ammo**: its own pool (`Player::pools[3]`), its own box (`PickupKind::MacAmmo`,
  32 rounds) and the gun itself as a pickup (`PickupKind::MachinePistol`, which
  is drawn when new). Both are in the Campaign Builder's Health & ammo tool
  ("MAC-11 bullets", "MAC-11"; PICKUP kinds 3 and 4). It is owned as the quest
  item `machine_pistol`. A script's GIVE draws it loaded. The console has
  `give mac11` and `give 380`. Key 4 and the wheel (after the handgun) draw it.
  The inventory shows it in the secondary slot (the right half when the handgun
  is also owned) and the .380 in storage cell 4.
- **Saves**: the inventory section's version 2 holds its pool and cell. The
  pickups section's version 2 allows kinds 3 and 4. Older saves (and the
  positional formats) load with no .380, and the cell is placed clear of the
  other items.
- **First person**: Claire's arms on the RetroWeaponPack's pistol rig, baked by
  `tools/player/build_claire_fp_mac11.py` into resource 3359
  (`src/assets/library/player/claire-fp-mac11.fbx`). There are 7 clips: idle,
  aim, walk, walk_aim, run, reload, draw. The `gun_*`, `mag_*` and `knob_*`
  points carry the gun, its magazine and its cocking knob (the pack's frame,
  magazine and slide) (`fpMacPoint`).
  - The reload clip is the pack's empty-magazine change. Both reloads seat the
    magazine at 78% of their length. From empty, the knob is the clip's slide,
    which slams home at frame 29. A tactical reload keeps the knob forward.
  - Each round adds a kick: muzzle rise about the grip and a shove back. A burst
    creeps up as the spread blooms, and the mouse sway rides over the clips.
  - The aim is lowered 12 mm onto the front sight (`mac11.aim_drop`).
  - The Limitator wears the same arms through `armTextures`.
- **Model**: `tools/player/import_mac11.py` writes `src/assets/weapons/mac11.fbx`
  (metres, the grip's top at the origin, +z the muzzle, +x its left;
  `MacMagazine` part 8, `MacBolt` part 9) from the PSX-Weapon-Pack, plus the
  .380 box (PSXAmmoBoxes) and the brass (RetroWeaponPack Projectiles).
  `tools/audio/import_mac11_sounds.py` makes its sounds (Rust & Blood SMG
  takes) and the cases' clinks (Horror SFX), resources 3346-3352.
- **Third person and other players**: held as the pistol (`ClaireOutside`
  `fitPistol`, the pistol clips) with its own mesh (`held` 3), for Claire and the
  Limitator. Remote players carry it as `RemotePlayer::weapon` 3 (two bits on the
  wire). A match's shots resolve with its row's damage.
- **The hip holster**: one sidearm rides at the hip, the one not in hand. When
  both are owned and neither is drawn, it is the one drawn last
  (`Game::hipHolster`, the `AvatarPose::hip` contents, WeaponId+1). VR sets
  `AvatarPose::hip` and draws the holster.

## Spent cases (Casings.cpp)

`Game::ejectCasing(kind, position, velocity)` makes a real case in the world:
kind 0 a shotgun shell, 1 9 mm brass, 2 .380 brass.

- **Physics**: it flies with the gun's motion added and tumbles (a fast roll
  about its axis). It bounces off floors, walls (`World::shotFraction`) and props
  (`vectra::rayCastProp`) with restitution and friction. The first few bounces
  clink (`Sound::CasingBrass` / `CasingShell`, positional, never heard by the
  AI). On a floor and slow, it lies on its side for `CasingLife` (14 s), then
  goes.
- **Limits**: at most `MaxCasings` (48) live; the oldest goes first. They are
  cleared with the map, shifted across a chunk seam, and never saved.
- **Who throws one**: the handgun and the MAC-11 on every round (`Game::shoot`).
  The desktop shotgun throws one when its pump cycles after a shot
  (`updateWeaponMotion`, at `pumpSoundAt`). In VR, the hand's pump stroke throws
  the shotgun's. The port and the throw come from the view (`ejectionPort`,
  `ejectionVelocity`), or from the VR aim.
- **Drawing**: both renderers draw them each frame, never into the static
  cache. The shell is the Remington's own shell part (part 3) at 65 mm; the
  brass is the pack's 9 mm case (0.9 of it for .380). No gun draws a shell glued
  to itself any more.

`--feature=mac11` (`src/lab/tubes/Mac11.cpp`, fixture `mac11-room`) covers all of
the above.
