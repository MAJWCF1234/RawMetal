# Mixamo sources (local)

Dropped Mixamo rifle/pistol packs live here as **source donors**. FBX and zip
files are gitignored.

## Existing runtime donors

These original donor downloads use FBX Binary, No Character, 30 fps, no key reduction:

- Basic Shooter Pack: 16 clips; archive SHA256 `f6d87cf307fecda5b61cadda87789e6cb10e806202a9358bbcb9f07d33e1c6c5`.
- Pistol/Handgun Locomotion Pack: 20 clips, including kneel transitions; archive SHA256 `e857a7d3b45a3e0e0e7c0bdd214337c9681cfafa23b95c1b93d55d0d3a651acf`.
- Pro Rifle Pack: 49 clips, including crouch locomotion, turns and deaths; archive SHA256 `eaa67336b0bf7bf755b0136d4d0b1a3550c50f724d49ca832bf57ccb3d38a91a`.

Cleanup clips bake through `tools/BakeCleanupMixamo.py` at 32 samples per clip.
The repaired OBJ and
RMC2 arm calibration remain the geometry authority. Donor arm/finger meshes
are never substituted. Medic treatment retains UAL2 Chest_Open.

The bake first runs `tools/BuildCleanupSkin.py` to derive connected arm
membership into separate `*-skin.bin` resources. It preserves the original
calibration and repaired geometry, rejects isolated false hand assignments,
and gives sleeves coherent ownership. Pelvis/spine deformation blends across
the waist rather than using a horizontal hard cutoff. See the
[visual repair report](../../docs/cleanup-animation-visual-fix-20261013.md).

RMA2 carries compact baked vertices. Its optional RML1 trailer stores pelvis,
spine and calibrated shoulder/elbow/wrist/palm-direction frames. Runtime joins
the layers at the posed pelvis, aims about the posed spine, and attaches the
composed arms to the same grip and muzzle used by gameplay. Whole repaired
hands receive rigid transforms. Donor planar travel is removed before baking.

The 32-sample choice was compared with the original 16-sample bake against
exact donor poses between keys. Walking and foot motion track the donor more
closely while remaining below the executable's 32 MiB ceiling. See the
[quality report](../../docs/cleanup-animation-quality-20261013.md).

## Character-specific intake: pending game bake

All six main squad members now have their own repaired 65-bone source rig
and audited direct Mixamo library. These new collections requested 24 fps
and uniform key reduction; some packs retain 30 fps, recorded in their catalogs.
The raw animation FBXs and archives remain local and ignored. Repaired source
rigs, manifests, catalogs and verification reports are tracked.

| Character | Animation FBX files | Library |
| --- | ---: | --- |
| Assault | 311 | [assault-direct](assault-direct/README.md) |
| Engineer | 118 | [engineer-direct](engineer-direct/README.md) |
| Medic | 121 | [medic-direct](medic-direct/README.md) |
| Sharpshooter | 196 | [sharpshooter-direct](sharpshooter-direct/README.md) |
| Heavy | 191 | [heavy-direct](heavy-direct/README.md) |
| Rusher | 156 | [rusher-direct](rusher-direct/README.md) |

Counts include pack overlaps, not distinct moves; each library also contains
one static character FBX. The game still uses the existing donor bake above.
Publish matching mesh/bindings/animation data together in the later bake,
retain evaluated bone poses and animated fingers, then verify weapon/prop
alignment, waist aiming, transitions and timing in native gameplay.

## Additional actors: pending game bake

[Investigator](investigator-direct/README.md) now has nine packs and 28 special
clips: 241 animation FBXs plus one static character, including overlaps.
Its original existing 65-bone rig and both authored hands are preserved.
Selected crouch/prone poses expose sharp coat folds requiring integration work.
Limitator remains a separate sparse-use actor; its intake is not completed here.
