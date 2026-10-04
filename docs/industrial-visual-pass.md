# Industrial visual pass — preview 4

The rendering defect was an absolute UV Jacobian threshold in `scene.frag`. Screen derivatives shrink as a valid surface fills more pixels. Consequently, a large close surface could lose its tangent frame and skip GGX specular and parallax while a smaller copy remained shaded. The replacement checks degeneracy relative to the two derivative axes. No textures, render targets or draw calls are added by the shader change.

This follows the screen-coordinate meaning of derivatives and explicit texture gradients in the [Khronos GLSL built-in specification](https://github.khronos.org/Vulkan-Site/glsl/latest/chapters/builtinfunctions.html#derivative-functions). The specific defect and its correction are established by RawMetal's GPU regression, rather than inferred from the specification.

`--vulkan-test` renders a black dielectric using compact and magnified geometry, a small UV span, mirrored UVs and collapsed UVs. Compact and valid magnified samples must agree within one display channel; the collapsed sample must remain finite and black. The old shader is built into the same executable/test harness as an explicit failure control, then the corrected source is restored and rebuilt.

Map review uses `--freight-art-inspection` and `--campaign-inspection`, with all temporary captures stored under `diagnostics/industrial-pass`. Initial captures exposed a generator incorrectly used for a desk and stepped waste chutes that looked like floating staircases. Both were revised before packaging. Additional views check the office's actual warehouse vista, chair direction, grounded pump equipment, tunnel fasteners and signal alcove.

The warehouse view uses the shared authored `visibleResidencyGroups` system, also exposed to custom campaign packs as `VISIBLE_GROUP|mapIndex|group`. It adds no campaign-index condition to the engine. Solid glass guards the high opening; light queries pass through the pane. Regression checks cover warehouse residency, optical apertures and fall protection, as well as generic distant-view retention, save/load, eviction and malformed records.

The signal-room computer exposed a second engine limitation: authored terminal population forced pi even though the runtime terminal supported yaw. Authored yaw now flows through map population and both editor export paths. Older records retain pi. The signal console faces its entrance, has supported local lighting, and its log opens through the normal E interaction in a gameplay regression.

Pixel-change percentages are image measurements, not quality scores. They are calculated at identical cameras against preview 3. New camera angles do not have an equivalent old capture. Several unchanged depot/intake views are expected to match exactly; this pass does not pretend every map changed.

Performance uses the native windowed Vulkan path at 1920x1080, linear HDR and 4x MSAA on the development RTX 5060 Ti. The freight benchmark includes Manifest offices, which now retains six chunks. Warmed rendering and first geometry construction are reported separately. This is not a GTX 1660 or headset measurement.

## Recorded validation

The old shader produced compact / magnified / small-UV / mirrored highlight values of **58 / 0 / 0 / 0** and failed the new regression. The corrected shader produced **58 / 58 / 58 / 58**. Collapsed UVs remained black in both. Vulkan material, cache reuse, camera-independent shading and optical visibility checks passed.

Campaign map traversal, campaign extension, service-map clearance, visible-group streaming, custom parsing, saves, the full smoke suite and offline VR tests passed. The x64 Windows 10/11 manifest and runtime-dependency binary audit passed; this does not replace a Windows 10 machine test.

| Matched view | Resolution | Changed pixels | Mean absolute RGB error / 255 |
| --- | --- | ---: | ---: |
| Foundry close material | 1920x1080 | 91.329% | 8.8870 |
| Foundry corridor | 1920x1080 | 42.924% | 3.1795 |
| Coolant basin | 1920x1080 | 7.735% | 0.2540 |
| Warehouse D block | 1920x1080 | 7.699% | 0.2358 |
| Pump Annex lower bay | 640x360 | 32.491% | 14.0821 |
| Waste receiving bin | 640x360 | 31.478% | 14.9652 |
| Warehouse aisle | 640x360 | 6.809% | 1.3331 |

The full comparison has 46 matched views. Close material effects were much more visible at native resolution than in the smaller inspection frames; native captures are part of this validation specifically because the defect depended on projected pixel size.

| Native freight scene | Preview 3 average FPS | Preview 4 average FPS | Preview 4 first render |
| --- | ---: | ---: | ---: |
| Receiving overlook | 127.9 | 127.6 | 299 ms |
| Receiving lanes | 151.9 | 157.3 | 38 ms |
| Warehouse A-B | 139.3 | 138.2 | 1681 ms |
| Manifest offices | 168.0 | 169.4 | 319 ms |
| Empty platform | 172.3 | 181.3 | 131 ms |
| Depot trackside | 155.6 | 152.6 | 640 ms |
| Inspection pits | 169.0 | 172.6 | 193 ms |
| Depot gantries | 178.9 | 179.7 | 440 ms |

These short samples show warmed performance remaining comparable while the office renders its real vista. They do not establish an overall speed improvement. First geometry construction remains a noticeable cost, particularly in the warehouse; it is excluded from warmed FPS and must still be improved. Every freight scene and all six closed/open/closed door benchmarks reported **zero static-map rebuilds during motion**. Door-cycle averages were 151.9–174.2 FPS; this does not prove the absence of all possible stalls.

The release executable is **22,124,032 bytes**, below 25,000,000. All **168** embedded resources passed lossless verification: exact decoded image pixels/dimensions and byte-identical models, audio and animation.
