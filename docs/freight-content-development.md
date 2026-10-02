# Freight content development — 2026-10-01

This development build continues the existing eight-area freight district.
The campaign still contains 32 chunks; no maps were added. The published
v0.6.1 release remains separate from this 0.6.2-dev iteration.

Receiving has an inspection island with a raised pallet load and a research
hold sign. Warehouse pallets and upper rack loads now use the original
PSX Bunkers wooden_crate_8 mesh and texture instead of plain boxes. Tall block
labels, a break corner and radio-check log, chemical holding equipment and a
restricted manifest give the four warehouse chunks different contents.

Manifest Control has framed office partitions, visible workstations, records
bays, seating and a missed-call log. The apertures are open; translucent glass
rendering is not implemented by this pass. The rail yard gains a weighing pad
and depot routing sign. The empty platform gains seated-height benches with
legs and backs, trackside barriers and a wall panel. The inspection pit gains
maintenance equipment and a jack warning. A collapsed section in Tunnel 4A
forces movement around the west side instead of through a straight open bore.

Fixture 17 exposes the new crate through the shared native/custom authored
world data. The original 256-pixel texture and complete source model are
retained, and both embedded resources pass exact source verification.

Validation:

- Release build: all 159 resources verified exactly, executable below 22 MB.
- Campaign test: all freight spawns, enemies, lights and terminals have
  clearance/access; upper stair routes and the transfer descent remain usable.
- Actual lift procedure with movement and E presses, passenger descent,
  save/load and lower landing; stitched receiving and warehouse seams pass.
- Shared mechanism test and Vulkan smoke test pass.
- Vulkan captures of all 22 entries, nine existing detail views and five
  new inspection views; repeated fixed-camera captures have zero static-cache
  rebuilds. Visual inspection led to relocation of an inspection island away
  from an enemy, raising a log to its accessible floor, and moving a rack sign
  out of a floor slab.
- Freight presentation benchmark uses eight scenes at 1920x1080 with 4x MSAA
  on the RTX 5060 Ti: 120.1–146.5 FPS and zero static-cache rebuilds. This is
  measured throughput on this machine, not a general performance guarantee.
  The report is under `diagnostics/freight-development/after/`.

The executable is 20,781,568 bytes. The local package is
`artifacts/RawMetal-0.6.2-dev-Freight-Contents.zip`, also copied to the workspace's
`RawMetal.zip`; its single executable entry matches the final build exactly.

Matched-camera before/after images and pixel measurements are under
`diagnostics/freight-development/`. Differences measure screen coverage,
not artistic quality. The small changes in some entry views are occluded by
existing cargo; dedicated detail views show the added contents. Generated
PPM captures are converted to PNG and removed.

The full brief is not complete: this pass does not add a forklift, the staged
worker-versus-bugs fight, dynamic crane rides or a climb-through shuttle car.
