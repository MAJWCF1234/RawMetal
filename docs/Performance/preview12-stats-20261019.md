# Preview.12 patch statistics

2026-10-19. Release: **v0.7.2-preview.12**, second neural texture reduction patch.

## Executable and storage

| Metric | Result |
|---|---:|
| Root/release executable | **24,173,568 bytes / 23.0537 MiB** |
| Saved against preview.11 | **1,294,848 bytes / 1.2349 MiB / 5.08%** |
| Saved against procedural preview.10 | **2,260,480 bytes / 2.1558 MiB / 8.55%** |
| Ceiling | 33,554,432 bytes / 32 MiB |
| Free space | **9,380,864 bytes / 8.9463 MiB** |
| Budget used | **72.04%** |
| Reduced textures | **16**, nine more than preview.11 |
| Selected/rejected this pass | **9 / 24** of 33 screened images |
| Stored samples for those 16 images | **969,246 pixels**, versus 3,874,074 original pixels |
| Pixel reduction | **74.98%**; half each axis, with edge padding for odd dimensions |
| Packed reduced images plus shared model | **1,028,082 bytes / 0.9805 MiB** |
| Shared model | **2,204 raw bytes**, 1,084 parameters |

The nine additions are Scissors, generator, three facility metals, PC casing,
transformer, service van and van metal. Scissors retains its authored 1024-square
runtime size, reconstructed from 512-square samples. No maps, geometry, audio,
animations or AI features are removed. Fine texture grain is softer; this is
lossy reconstruction, not exact recovery. Original source images remain intact.

Runtime images reconstruct to their original dimensions and remain cached.
Storage savings do **not** imply smaller steady-state texture buffers or a VRAM
reduction. This pass changes resource packets and the encoding manifest, not
runtime/AI/renderer code or model weights.

## FPS and frame times

**NVIDIA GeForce RTX 5060 Ti**, Windows x64 trained-PGO Release, native-window
Vulkan at **640 × 360, linear HDR, 4× MSAA**. Exclusion lab, fixed input replay,
30 warmup frames and 3,600 measured frames per run; live muted audio mixer.
Four runs per build in two opposite-order batches: **14,400 measured frames
per build, 28,800 total**. Timings are CPU wall frame times including native
presentation, not GPU timestamp measurements or a game-wide FPS guarantee.

| Metric | Preview.11 baseline | Preview.12 patch |
|---|---:|---:|
| Aggregate FPS | 300.77 | **309.57** |
| Average frame time | 3.3248 ms | **3.2303 ms** |
| Median frame time | 3.2845 ms | **3.2740 ms** |
| p95 frame time | 4.3641 ms | **4.2392 ms** |
| p99 frame time | 5.3019 ms | **4.7726 ms** |
| 1% low FPS | 89.16 | **196.74** |
| Run-average FPS range | 273.05–324.51 | **292.03–320.72** |
| Slowest recorded frame | 80.2727 ms | **6.8180 ms** |

Aggregate FPS is 1000 divided by mean frame time. The 1% low is 1000 divided by
the mean of the slowest 1% of frames; it is not the reciprocal of p99. The
baseline includes an 80 ms stall, and both builds show substantial between-run
variation. Opposite-order batches reverse the apparent result. **These recorded
numbers do not establish an FPS gain, a stable 1% low improvement, or a consistent
performance change.** No runs were discarded. The verified improvement is file
size and the absence of per-frame reconstruction work.

Patch CPU wall stage means (overlap/boundaries mean these are not an exact sum):

| Stage | Average |
|---|---:|
| Simulation | 0.7344 ms |
| Audio | 0.1536 ms |
| Scene preparation/render work | 1.7780 ms |
| Submission | 0.3426 ms |
| Presentation | 0.1345 ms |

Scene work is the largest measured CPU wall stage. This is not a GPU bottleneck
diagnosis; no GPU timestamp pass was performed for this patch.

## Validation and identity

- **750** native decoder checks passed.
- **231** embedded resources verified; sixteen neural outputs match native decoded expectations.
- All nine new native outputs match Python within one 8-bit channel level, with deterministic repeated decodes.
- Full **Vulkan smoke**, **28 visibility views**, twelve Exclusion and 23 freight-detail captures passed/inspected.
- Culling/reference comparisons report **zero changed pixels**.
- **17 startup reconstructions**, **zero** during 240 gameplay frames or atlas scale changes. Water loads twice.
- Human character textures, hands, weapons, normal/emission maps and UI assets are unchanged.

Root and release SHA-256:

`54E771DF0DBE6F3F08584B01B6BC4CC5D9E1A6BE01E341D51245BE9E7DC7509B`

[Screening, decisions and visual comparisons](../Assets/neural-textures-round2-20261019.md).
[Machine-readable statistics](../evidence/neural-textures-round2-20261019/release-stats.json).
[Compressed per-frame evidence](../evidence/neural-textures-round2-20261019/frame-times.csv.gz).
[Release download](https://github.com/MAJWCF1234/RawMetal/releases/tag/v0.7.2-preview.12).
