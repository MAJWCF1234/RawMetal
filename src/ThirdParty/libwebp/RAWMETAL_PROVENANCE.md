# Vendored libwebp

Upstream: https://chromium.googlesource.com/webm/libwebp/

Version: v1.6.0, commit `4fa21912338357f89e4fd51cf2368325b59e9bd9`.

Unmodified source subset: src, sharpyuv, cmake, CMakeLists.txt, configure.ac,
COPYING, PATENTS, AUTHORS, README.md. Examples, external image I/O, test data
and standalone tools are not vendored or enabled.

The build-time packer uses lossless mode, near_lossless=100 and exact=1.
It verifies every RGBA byte after decoding, including invisible RGB beneath
transparent pixels. The game statically links only webpdecoder, not the encoder.
No texture resizing, palette reduction or lossy colour conversion is performed.
