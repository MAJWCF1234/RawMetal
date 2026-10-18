# OpenKTG in RawMetal

OpenKTG by Fabian "ryg" Giesen (2007), explicitly placed in the public domain
by its author. See LICENSE and the retained source-file notices.

Vendored from the local kkrieger-werkkzeug3 checkout:
`E:\No Platform\kkrieger-werkkzeug3-master\kkrieger-werkkzeug3-master\ktg`
Upstream archive: https://github.com/farbrausch/fr_public/tree/master/ktg

The `ktg` tree is copied here in full (library sources, demo, upstream
CMake/VS project files). RawMetal builds only `gentexture.cpp` via
`RawMetalTextureGenerator` in `src/CMakeLists.txt`; it does not build the
standalone demo or upstream `CMakeLists.txt`.

- gentexture.cpp SHA-256: 7654ADFE27A9306BAA9F2C807374976EB31035EEEDBCAFE2B3783DA1F1EE76FC
- gentexture.hpp SHA-256: 4454C5A2EE1F34F2B9EABCDEED717257A40425B0853DF5BC1E927C88C8885641
- types.hpp SHA-256: 1CEB05843E671C463C5C2B94CEB1C63F948078AE72C01424446092D5F5D9FE14

RawMetal's own recipe renderer in core/ProceduralTexture.cpp uses OpenKTG's
tileable multiscale noise plus per-texel white-noise grit (freq = log2 size).
RawMetal authors the colors, pits, oxide coverage, roll/drip grain, scratches
and grout. Material kinds: 1 concrete, 2 sign rust, 3 panel metal, 4 service
floor (3×3 tiles matching the replaced atlas seams), 5 weathered metal.
Textures generate during resource loading, then use the existing renderer and
mip/normal preparation. The static library adds no runtime DLL and no editor
or sound synthesizer.

Quality notes (vs the five replaced PNGs): bandlimited cloud alone reads soft.
Photographic grit needs the white-noise layer; rust wants hard wear masks more
than sand static; service floor needs thin grout and speck pits; weathered
metal needs organic oxide blooms without scrape lattices. Side-by-side evidence
lives under `.build/procedural-textures-20261018/`.
