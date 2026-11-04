# Gameplay collision and rendering fixes — 2026-11-01

The doorway pass replaces point-only contact against thin structures with full
footprint overlap for players, creatures, and autonomous NPCs. Decorative doorway
architraves retain their appearance and no longer add collision. Walls, lintels,
closed doors, rails, and breakable partitions retain collision. Player and creature
movement uses short swept steps; NPC movement can slide along an opening's edge.
Step support also uses the complete footprint, so thin track rails and low
thresholds raise the feet before the body meets their sides. Bounded same-side
recovery lets autonomous actors resume from a saved overlap near a jamb.

The Chimera uses a 0.45 m navigation half-width and a 2.35 m standing hull, scaled
with authored size. Its normal pursuit is 2.8 m/s and its rush is 5.8 m/s. Its body
stops before entering its prey, including during cooldown, and player/NPC movement
respects its body. Route probes test shoulder clearance, retain clear positions
within off-centre grid cells, and return to a safe cell position before rounding
an obstructed corner. Clip transitions blend over 120 ms; the Chimera's damage
event aligns with the authored slam's 58% contact pose. Its death uses the full
1.8 s authored duration. Huntsman, Xenowasp, Scissor Fiend, and Mutated Human
pursuit speeds are increased while the Snatcher retains its ambush pacing.
The Research Receiving freight stair leaves a full metre beside the stored
pallets, allowing the actual cold-hold Chimera to reach the receiving gate.

The HUD no longer displays `INCOMING`. Attack poses and sounds remain the tells.

Architectural draws subtract buried and coincident box faces, preserving the
original UV coordinates on exposed parts. Wall-adjacent gantries no longer add
duplicate wall backing or a rail inside the wall; exposed edges above low walls
retain rails. Rails stop at intersecting walls and columns. Automatic rails yield
to authored guardrails along the same deck edge. Dynamic object lighting no
longer depends on a shared shadow probe budget being left over from
earlier draws. A changing lamp list preserves unchanged shadow atlas slots.

The prologue cab light follows its elevator throughout the rise and fall. It sits
below the luminous ceiling panel and uses the cab's own housing. Native prologue
capture views 9–10 also avoid indexing the four office camera positions.

Validation evidence is written under `.build/gameplay-fixes-20261101/`.
The original executable and its 50 passing test groups are retained in `before/`.
The new focused checks are `--gameplay-fixes-test` and
`--gameplay-rendering-test`; the former is included in `--each-test`.
Final validation:

- MSVC Release build passed; `RawMetal.exe` is 47,477,248 bytes. All 1,147
  packaged resources passed the source verification step.
- `--each-test`: **51/51 groups passed**, ending `ALL PASSED 0`.
- Focused checks pass for both doorway directions, autonomous NPC traversal,
  thin-wall contact, narrow track-rail stepping, old-overlap recovery, joined
  gantries, duplicate rails, and the actual Research Receiving Chimera route.
  Chimera pursuit/attack separation passes at 120 Hz, 30 Hz, and 200 ms steps.
- All three formerly blocked depot controls are reachable with the standing
  player hull. The campaign map test passes.
- `--prologue-test`: **22/22 checks passed**. Navigation graph, cached-versus-live
  AI query equivalence, and creature schedule checks passed.
- `--gameplay-rendering-test`: **zero changed pixels** at shadow-probe budgets
  0, 1, 2,200, and 20,000 after warm-up.
- `--vulkan-test` passed on the NVIDIA GeForce RTX 5060 Ti, including relief,
  per-pixel lamps, shadow reuse, and 174,325,320 collision differential checks
  across campaign and Ashfall chunks with zero mismatches. The material/lamp
  comparison uses an isolated room so campaign art changes and streamed dynamic
  lamps cannot silently select a different material or force both lighting modes.
- Reviewed 60 final native-renderer captures: 11 prologue views, 40 Chimera clip
  poses from front/side, and nine doorway, gantry, depot, and cold-hold views.
  Seven architecture views also have original-executable comparisons. Contact
  sheets and raw frames are retained with the validation evidence.
- `git diff --check` passed. No manual full-campaign or VR/headset session was
  performed; the visual review uses native renderer captures.

Executable SHA-256:
`0f455791cdbe56d6e71ed12cf2156a8489876199436b60aacb35ccab4c3055b4`
