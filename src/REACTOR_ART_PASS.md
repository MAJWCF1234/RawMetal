# Reactor readability pass

- Removed the computer instruction board and maintenance-disk billboard.
- Removed the persistent post-crash objective walkthrough. Computer and valve interactions report machinery state; the maintenance log provides an environmental clue.
- Replaced the automatic post grid with perimeter piers and supported ceiling beams. Opened the upper deck north of the core without changing the stair or puzzle interaction positions.
- Concrete lower floor and plate-metal upper deck replace room-wide grating. The stairs retain concrete bodies and inset anti-slip treads.
- Added original PSX Tech instrument cabinets with proportional scaling and collision, and elevated coolant headers with flange joints. Reduced the core's continuous cyan strips to shorter inspection windows.
- Follow-up: replaced switch-mounted valve wheels with complete pipe-mounted mechanical assemblies. Opened the boarding safety cage visually without changing collision. Freight, traction equipment and ventilation now establish the purpose of the lift room and remain visible through the cab windows.

`--plant-inspection` writes six reproducible player-height captures for valve and boarding-area review. `--shaft-inspection` covers the moving sequence.

The v0.3.1 follow-up passed functional tests and Vulkan synchronization validation. Its 1,560-frame benchmark averaged 53–58 FPS in lift/reactor scenes, with three ride frames above 50 ms (worst 184 ms). All six culling comparisons were pixel-identical. The strict minimum-frame-rate test remains failing; see release notes rather than treating the earlier measurements below as current release results.

Asset provenance is in `assets/reactor/SOURCES.md`. Original source archives are unchanged. Existing save state and puzzle order remain compatible.

## Verification

Player-height Vulkan captures reviewed for the computer, core balcony and stair landing. Lift traversal, reactor puzzle, save/load, audio and console tests pass; software and Vulkan smoke tests pass. New regression checks cover cabinet collision, computer access and the opened containment sightline.

Two 1,560-frame Intel Graphics Vulkan runs at 640x360: reactor averages 58–64 FPS, worst reactor frame 22.76 ms (43.9 FPS), and all six culling/reference comparisons identical. The overall strict 50 ms benchmark **fails**: run one had a 91 ms fall hitch; run two had ascent hitches up to 98 ms. These remain unresolved; no universal minimum FPS guarantee is claimed. Reports are in local `diagnostics/reactor-art-performance-first.txt` and `diagnostics/performance-test.txt`.
