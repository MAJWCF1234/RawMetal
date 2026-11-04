# Chapter 1 playtest fixes - 2026-10-25

Focused follow-up to v0.12.0. Existing chapter layouts, states, encounter timing,
and content scope are preserved. This pass addresses physical placement and
one trapped pocket in the authored route.

The route inspection also found missing setters for the feeder and three
downstream gate prerequisites. Supported, labeled E controls now restore the
intended sequences instead of deleting the locks:

- Emergency Generation (39) -> Substation A (36) -> Load Control (40): shed
  loads, transfer the bus, energize Feeder 9. The direct hub gate now works.
  The existing placeholder emergency-generation seam also offers a bypass;
  the feeder was not the only possible geometric route into Machine Hall.
- Central Stores (54): sign out bar stock; Fabrication Shop (55): machine the
  pin, then fit it to release the Operations shutter.
- Gatehouse (59): release the Security Perimeter yard gate.
- Administrative Link (61): obtain the Checkpoint Delta override.

These utility rooms are still explicitly unfinished. Operational consoles do
not constitute a finished generator room or a fully modeled lathe. Chunk 67's
great door remains the playtest endpoint, protecting unfinished Chapter 2.

## Changes

- Power Distribution (34): a solid field case supports the portable floodlight,
  keeping its legs out of walkable floor space and retaining the lamp height.
- Machine Hall north gallery (41): the red bulkhead at x=6 is mounted on the
  front of its pilaster rather than buried inside it.
- Machine Hall exciter bay (42): Unit 2's north railing ends where the shutter
  gallery starts. Previously, the overlapping rails enclosed a walkable pocket.
  A movement regression walks this gallery north and back without a jump.
- Security Perimeter (58): the booth health pack sits on its 12 cm slab;
  STOP has a steel backing, the vehicle-lock sign is attached to its shutter,
  and the portable floodlight has a solid crate base at the same lamp height.
- Research Receiving (64): R-114's Huntsman and ammo sit on its 8 cm floor;
  the north pallet stacks clear their corner stack; the CL-2 overhead pipe
  has roof hangers at both ends.
- Maintenance District (53): moved loose clutter off the canteen chair onto
  the actual tabletop, avoiding an initially intersecting physics object.

## Validation

The native shipping-PGO build verifies all **1,011 resources**, including
**61 neural textures**. No source assets, animation banks or voices were added.
All **27 independent suites** (`--each-test`) and the full **Vulkan smoke**
(`--smoke-test --vulkan`) returned exit 0 on the final executable.
The new interaction regressions exercise E from physical approach positions,
refuse incorrect operation order, resume the power chain after save/load,
and confirm the four actual route doors accept their restored prerequisites.
The shutter gallery is walked both ways with the real player hull, without
jumping or dropping from the table.

All **34 chunks (34-67)** were audited: **26 pass**, **8 still fail** with
**121 placement reports**. Baseline had 139 reports: **18 removed**. No TRAP,
UNREACHABLE, STUCK creature, STUCK ammo or STUCK health reports remain in this
audit range. A stuck clutter report remains in chunk 44; the other reports
include pipe routing, mount/support, embedded props and overlapping details.
They remain review work, not certified harmless or automatically waived.

Audits open the doors, so they cannot alone establish normal progression.
The interaction/door regressions complement them; a continuous human playtest
of the complete route remains necessary. Existing room/actor quality and FPS
are not certified by the audit.

Original targeted reports and all final reports are under
`docs/evidence/chapter1-release-fixes-20261025/`. The contact sheet documents
the inspected corrections and the clearly unfinished control rooms.

The baseline audit of chunks 34-67 also reports other placement concerns.
This document does not claim that every chapter is finished or every audit
warning is resolved. Water/Cooling and the later campaign remain at their
current playtest status; no parked chapter drafts are restored by this pass.
