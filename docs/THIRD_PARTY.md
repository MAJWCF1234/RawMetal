# Third-party notes

This file centralizes small version/provenance notes that were previously scattered across several ThirdParty README files. License text that must travel with vendored code remains beside that dependency.

## meshoptimizer
Vendored decoder subset of meshoptimizer v0.25, pinned to commit:
`6daea4695c48338363b08022d2fb15deaef6ac09`

Upstream: https://github.com/zeux/meshoptimizer

The MIT license remains at:
`src/ThirdParty/meshoptimizer/LICENSE.md`

## stb_image
Vendored stb_image v2.30 by Sean Barrett and contributors.

Recorded SHA-256:
`1F8C1B6B408F26E3B20CBFBBD4758AFB3DC9B837FF1E17C258928F406148A87C`

Upstream: https://github.com/nothings/stb

The MIT/public-domain license alternatives are included in the vendored `stb_image.h`.

## Archive support
RawIron/RawMetal package tooling uses a bounded ZIP-compatible reader for `.ripak` packages and reuses vendored stb code for raw DEFLATE decoding. The reader rejects unsupported or unsafe archive features such as traversal paths, links/reparse entries, encryption, ZIP64 and split archives.

## Epic Online Services
The repository has an optional EOS SDK slot under `src/ThirdParty/EOS`. Proprietary EOS SDK binaries and live credentials must not be committed. Any local EOS configuration containing client secrets remains outside source control.
