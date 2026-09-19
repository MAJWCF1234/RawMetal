# Cab-view shaft staging

The boarding/transfer room and both reactor floors remain occupied game spaces.
The four passing storeys are now shallow scenic rings around the shaft, not
standalone full maps. Each uses 28 deck tiles instead of 448, with simple opaque
backing and ceiling geometry. The occupied rooms retain proper ceilings and
their routes, equipment, puzzle and exit behavior.

Passing visual identities:

- Lower service: coolant riser bundles and blue accents.
- Transfer: generator/ventilation silhouettes and amber accents.
- Maintenance: grouped electrical cabinets and red accents.
- Surface: heavy sealed panels and the existing clamped exit gates.

Purchased equipment retains its original proportions. Close guide rails and
height markers provide actual parallax from the moving cab. During failure,
short-lived sparks trail upward alongside the guides and a loose cable whips
beside the window. The existing blackout, creak, snap, brake catch and second
fall provide the timing. Visual effects depend on persisted lift phase/time:
no asynchronous sequence, random particle state or player-camera takeover.
The dispatch sign is now attached to the header to keep the window clear.

Validation: full software and Vulkan smoke suites, the boarding/reactor route,
lamp supports, and seven mid-ride/puzzle save/load roundtrips. Vulkan sync
validation produced no errors. `--shaft-inspection` supplies ten cab-window
captures. `--performance-test` compares six views against uncullled geometry.

First full-resolution 640×360 Vulkan benchmark for this revision: 1,560 frames,
none above 50 ms; exact culling/reference matches. Lift/reactor scenes averaged
9.4–10.9 ms, worst 14.1 ms. These are local measurements, not a hardware-wide
minimum-FPS guarantee. The earlier occasional-hitch reports remain retained.

A second consecutive 1,560-frame run also passed with no frame above 50 ms;
lift/reactor averages were 9.5–10.9 ms, worst 14.45 ms (69.2 FPS). Culling
comparisons remained exact. Both runs used the existing full-resolution setting.
