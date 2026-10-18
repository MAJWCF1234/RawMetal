---
tags: [performance, exclusion, validation]
date: 2026-10-18
---

# Competitive round 8: profile-guided native build

**The PGO candidate wins the paired Exclusion competition.** It is a candidate,
not a root replacement or published release. Existing source edits are preserved.

## Identity

| | Value |
| --- | --- |
| Candidate | `.build/competitive-round8-20261018/pgo-candidate.exe` |
| SHA256 | `4E62707E07A1991763365B5920DE9358EFEE08001F6B9C8B5251F82BBD3F9E11` |
| Candidate size | 26,919,936 bytes (25.67 MiB) |
| Headroom below 32 MiB | 6,634,496 bytes (6.33 MiB) |
| Baseline | Saved root, SHA256 `EDAF720D6021D99D5AA351D6CDB81AE23B7B0EB0597AC0B6B7516DE8EF0F34C9` |
| Baseline size | 27,795,456 bytes |

## Final competition

RTX 5060 Ti; native hidden 640x360 Vulkan window, linear HDR and 4x MSAA.
Fixed 60 Hz simulation, 30 warmup + 3,600 measured frames per run, live muted
device mixer, deep instrumentation disabled. Order: B1, C1, C2, B2, B3, C3.
No builds or regression tests ran alongside the timed competition.

| Run | Baseline FPS | Candidate FPS |
| --- | ---: | ---: |
| 1 | 244.56 | 271.10 |
| 2 | 242.93 | 278.61 |
| 3 | 243.03 | 271.74 |

| Three-run median | Baseline | Candidate |
| --- | ---: | ---: |
| Mean frame time | 4.115 ms | 3.680 ms |
| Average FPS | 243.0 | **271.7** |
| 1% low FPS | 153.6 | **178.3** |
| P99 frame time | 6.154 ms | 5.332 ms |
| Simulation mean | 0.850 ms | 0.737 ms |
| Scene mean | 2.377 ms | 2.121 ms |
| Audio mean | 0.219 ms | 0.164 ms |

Average FPS improves **11.8%**; mean frame time falls **10.6%**. A run's 1% low
is 1000 divided by the mean of its slowest 36 frame times, then medians are
taken across the three runs. These are CPU wall timings, not GPU timestamps.
The single worst candidate frame was 12.093 ms versus 10.669 ms for baseline;
this is not a claim that every hitch improved or every map runs at 272 FPS.

All 3,600 per-frame health values and aggregate AI/route/sensing/planning
fingerprints match across all six runs. Final health is 99,158 throughout.
Raw reports/CSVs: `.build/competitive-round8-20261018/PB*.{txt,csv}` and
`PC*.{txt,csv}`. Computed metrics: `pgo-results.json` in that directory.

## Change and reproduction

Only `src/CMakeLists.txt` adds an optional MSVC Release PGO phase. Compiler
feedback guides native code layout, inlining and hot/cold optimization;
there are no new animation banks, gameplay rules, shader changes, ISA flags,
floating-point shortcuts or reduced render settings. Hardware verification
was on the RTX 5060 Ti; older hardware was not available for a fresh test.

Use the same sources for GENERATE and USE:

1. Configure `RAWMETAL_PGO_PHASE=GENERATE`, an isolated
   `RAWMETAL_RUNTIME_DIRECTORY`, and an absolute `RAWMETAL_PGO_DATABASE` path.
2. Build Release. The linker uses `/FASTGENPROFILE`. Put the matching MSVC x64
   `pgort140.dll` beside this training executable only.
3. Run `--lab-performance --smoke-test` and
   `--ashfall-performance --smoke-test` there, with the current `custom maps`
   fixture. Copy generated matching `fast!*.pgc` files beside `fast.pgd`.
4. Configure `RAWMETAL_PGO_PHASE=USE` with the same database and a separate
   output directory; build Release. The linker uses `/USEPROFILE` and consumes
   those counters. The resulting candidate needs no PGO runtime DLL.
5. Run the paired competition, regressions and image comparisons again.
   Retrain after source/content changes rather than assuming old results hold.

This run retains `fast.pgd` and the training counters locally. Ordinary build
configuration has been reset to `OFF`, with isolated output, to avoid accidentally
reusing training data for later edits. PGO is opt-in and missing databases are
rejected at configuration time. See Microsoft's
[PGO linker documentation](https://learn.microsoft.com/en-us/cpp/build/reference/useprofile?view=msvc-170).

## Validation

- All **230 packaged resources** verify unchanged against source.
- **11 successful regression invocations:** AI queries, decision scheduling,
  navigation graph, squad roles, Medic patients, saves, terrain queries,
  stealth, Exclusion, Vulkan materials, and full `--smoke-test --vulkan`.
- **48 paired six-role captures are byte-identical**, including walk, fire,
  support and death, front/side views, phase 0.37. See `pixel-results.json`.
- Root remains byte-identical to the saved baseline. No release was published.

Existing limitations remain: root's idle-emission cache can omit animation
phase. A fresh candidate `--gpu-actor-test` passes its first 54 render comparisons,
then exits 8 with `Receiving retreat did not complete`, matching the root failure
confirmed in the preceding inspection. The whole GPU actor suite therefore
remains failed despite full smoke passing.
These performance results do not certify those inherited systems as fixed.

## Rejected experiment

A bulk actor-triangle submission experiment was slower: median 4.258 ms
versus 4.106 ms (about 3.7% more frame time). Its extra staging copy outweighed
the batching savings. All of its renderer/test edits were removed; preserved
renderer files match the pre-experiment copies byte for byte. The precise
`/GENPROFILE` training attempt was stopped because its instrumentation startup
cost was excessive; its timings were never used as performance evidence.
