# v0.6.1 — Freight Repairs

The service van now has all four wheels at their correct axles. The supplied
OBJ had lost the wheel nodes' placement transforms. The assembly was rebuilt
from the original FBX with every triangle and material retained; baked OBJ
and corrected FBX renders are pixel-identical. The original source is retained.

Freight Access and Lift Machinery (displayed maps 11 and 12) have clearer
personnel-route signs, additional cargo dressing and guards around the shaft.
The auxiliary lift's release control now opens its gate directly. Its lower
landing connects to the deck, so the player can walk off without jumping over
an unintended barrier. Control logs remain visible after activation and show
whether a command was accepted or blocked by an interlock.

Shared boundary-door headers close the large openings above entry and transfer
doors, including Warehouse Intake. They render and block movement. The cargo
lift's roof also blocks movement. These fixes apply through the shared authored
world system rather than special campaign cases.

Computer lore throughout native and custom campaigns now appears as green
terminal text on a recessed black screen, inside the existing metal housing.
Long body lines wrap, and the close prompt stays inside the terminal window.

No maps were added. The freight soundtrack and two mutated-human encounters
remain. Save format 13 is unchanged. The self-contained executable is
**20,710,912 bytes**, below the **22,000,000-byte** limit.

Validation passed:

- Release build and exact verification of all 157 embedded resources.
- Vulkan smoke test and shared mechanisms, isolation, streaming, saves and
  console regression tests.
- Campaign traversal and a lift procedure using actual movement and E presses:
  release the machinery brake, select local control, call the platform, release
  the cage, board, save/load during descent, and walk onto the lower landing.
  The test does not inject puzzle states or teleport onto the platform.
- Collision checks above entry and transfer doors in the affected freight maps.
- Door animation benchmark across six campaign maps: zero static-map rebuilds
  during motion.
- Visual inspection of both van sides, the doorway repairs and legacy/freight
  terminal logs. Matched-camera before/after pixel comparisons are recorded in
  `diagnostics/freight-repairs/`; pixel coverage is not a quality score.
