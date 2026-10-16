# Executable space optimization — 14 October 2026

The finished squad animation, glue-gun aiming, Ashfall terrain and stealth AI
remain in place. This change modifies packaging and decoding, with no source
asset edits, animation resampling, texture resizing or audio transcoding.

| Measure | Before | After |
| --- | ---: | ---: |
| Executable bytes | 30,843,392 | 25,499,648 |
| Executable MiB | 29.41 | 24.32 |
| Headroom below 32 MiB | 2,711,040 bytes | 8,054,784 bytes |

The executable saves **5,343,744 bytes**. Growth reserve rises from **8.1%**
to **24.0%** of the cartridge budget, almost three times the previous room.
The strict ceiling remains 33,554,432 bytes; release builds now also warn
when less than 6 MiB of reserve remains.

## Measured contributors

The initial packed resources totalled 27,177,900 bytes. Textures accounted for
10,794,968 bytes, animation/skin binaries 9,604,314, WAV audio 2,583,942,
FBX models 1,912,072, OBJ geometry 1,152,242 and MP3 audio 1,125,764.

A new lossless animation encoding predicts each quantized coordinate from
that same vertex in the preceding frame, resets at every clip boundary,
and groups the resulting axis/byte lanes before LZMS compression. It supports
both RMA2 and RMA3. Topology, source samples, joint frames, clip durations and
all trailer bytes are restored exactly. The packer retains the new encoding
only if it is smaller than the existing candidates.

The six squad banks alone fall from **8,470,322 to 3,901,938 packed bytes**.
The same encoding also benefits the existing Worker, mutant, stalker and
drone banks. Seven byte-identical packed payloads share canonical resources:
the electronics/control-console texture pair and the six arm/skin pairs.
Resource IDs remain unchanged. Equality is checked by SHA-256 and bytes;
references cannot form chains or cycles.

Packed resources now total **21,831,384 bytes**. New decoder code and PE
alignment account for the difference between packed-resource and EXE savings.

## Verification

- Release build succeeded and passed the unchanged hard size limit.
- Every one of **229 embedded resources** matches its source: exact RGBA
  pixels/dimensions and byte-identical models, audio and animations.
- **199 codec checks** passed, including independent known planes and scalar
  reference comparisons, wraparound, unequal clip lengths, boundary resets,
  RMA2 compatibility, malformed layouts and all **10 actual animation banks**.
- Native split/aim checks: **103 pass**. Engineer behavior checks: **134 pass**.
- No source assets, gameplay rules or authored levels changed in this task.

Small reports are retained in [the evidence folder](evidence/space-optimization-20261014/results.json).
No new diagnostic image sequences or comparison executables were retained.

```powershell
cmake --build .build/vs2022 --config Release
cmake --build .build/vs2022 --config Release --target RawMetalAnimationCodecTest
.build/vs2022/tools/RawMetalAnimationCodecTest.exe
.build/vs2022/tools/RawMetalAssetPacker.exe --verify-exe N:/rawmetal/src N:/rawmetal/RawMetal.exe
```
