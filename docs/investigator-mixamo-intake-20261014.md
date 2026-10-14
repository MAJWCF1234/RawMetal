# Investigator Mixamo intake — 2026-10-14

Nine packs and 28 individual clips are saved locally, catalogued and audited:
242 FBX files, including 241 animations and one static character. Pack overlaps
remain; this is not a count of unique actions. The original source model and
texture are preserved unchanged with verified SHA-256 hashes.

## Validation

- All 242 FBXs passed the full 65-bone/finger-joint audit, source timeline checks
  and finite bone poses sampled at beginning, midpoint and end.
- All ten independent source finger-bend checks passed.
- Requested 24 fps and uniform reduction: 192 animations use 24 fps; Pro Rifle
  retains 49 animations plus its static character at 30 fps. Preserve elapsed
  source timing during the later 24 fps bake.
- Native source renders cover three walk phases, pistol/rifle aim, crouch,
  awareness and prone; close-up shows both hands and bent fingers. Evaluated
  pose matrix transfer is checked within 0.001 for exports without skin.
- The shared source renderer also passed the Medic regression after adding
  Investigator support. This checks rendering/pose transfer, not game behavior.
- Existing executable asset verification remains unchanged: 228 assets match.

![Selected native source poses](images/investigator-mixamo-source-poses-20261014.png)

## Observed limitation and integration boundary

The long coat follows the existing leg skinning and folds sharply in crouch
and prone. These poses need coat deformation review before acceptance in game.
Weapons are absent from the source renders; grip/contact alignment is pending.
Generic axe equip reaches require adaptation for gun draws. No claim is made
that every downloaded animation has been visually reviewed.

This is source intake, not a new runtime animation bank or GoldSrc parity pass.
Full-body source deformation, animated fingers, waist aiming, prop contacts,
transitions and events must survive the later bake and be checked in gameplay.
Raw animation FBXs/ZIPs are local and ignored; Git tracks the original rig,
texture, catalog, manifests, renderer changes and this visual report.
