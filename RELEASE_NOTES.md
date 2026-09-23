# RawMetal 0.4.6 — Terrain and lossless compression

RawMetal 0.4.6 brings shared PSX soil/rock materials with matching normal maps, safer slope spawning, terrain-aware creature routing, twelve-chunk seam checks, and supported ruin walls. Reactor Service Gallery and Coolant Return include the latest shelf, doorway and quiet-basin repairs.

Lossless size reduction:
- The packer compares reversible sample predictors and byte-plane layouts for each WAV, keeping the smallest result. Decoded WAV files remain byte-identical.
- Textures retain their full dimensions and exact RGBA pixels. Models and animations retain their original bytes.
- Native code/data folding and size optimization of the FBX importer reduce executable overhead while preserving renderer speed optimization.
- Completed resource manifests are published atomically so interrupted packing cannot publish a truncated manifest.

Downloads: run RawMetal.exe directly, or extract RawMetal.zip and run the identical EXE inside it. All game assets are embedded.

Sizes: signed EXE **17,703,824 bytes**; ZIP **16,922,758 bytes**. The unsigned build decreased from 18,261,504 to 17,702,400 bytes, saving 559,104 bytes (3.06%) with no content removed.

Validation: all 143 embedded resources match their sources; Vulkan smoke, twelve-chunk world isolation/seam checks, and performance tests passed. The reference smoke frame is byte-identical to the pre-compression build. Ashfall's 120-frame turning benchmark averaged 16.69 ms, with a 27.79 ms maximum on the test machine (RTX 5060 Ti); these are measured results, not universal hardware guarantees.

The EXE uses the existing self-signed RawMetal Development certificate; this is not a publicly trusted publisher certificate.
