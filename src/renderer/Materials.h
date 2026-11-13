#pragma once
// Physically based materials (Karis, "Real Shading in Unreal Engine 4", 2013; Lagarde & de Rousiers,
// "Moving Frostbite to PBR", 2014). Every texture belongs to a material class with a physical range of
// roughness and a metalness. The packs ship albedo only, so the per-texel roughness, metalness and
// cavity are derived from the albedo itself when it loads (the "surface map"): grime, pits and stains
// rougher, worn raised areas smoother, rust on steel and chips through paint their own material,
// grout rough between glazed tiles. Nothing is packed into the exe; it is all generated here.
//
// The split-sum environment BRDF (scale and bias of F0 by NoV and roughness, height-correlated Smith
// GGX) is computed here once and sampled by scene.frag; its F0 = 1 sum is also the single-scattering
// directional albedo the multiple-scattering compensation divides by (Turquin, "Practical multiple
// scattering compensation for microfacet models", 2019).
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace retro {
enum class Material : std::uint8_t {
 None,PaintedMetal,BareSteel,Rust,Chrome,Aluminium,Brass,Concrete,Plaster,Stone,Brick,GlazedTile,Vinyl,
 BareWood,VarnishedWood,Fabric,Rubber,Plastic,Glass,Water,Skin,Clothing,Crystal,Paper,Leather,Ground,
 Ceramic,Foliage,Screen,SealedConcrete,Count};
// lo..hi: the class's roughness range (perceptual roughness, alpha = r^2). metal: the class is a metal
// (its albedo is its F0, it has no diffuse). f0: a metal's F0 luma (linear) its albedo is brought to;
// a dielectric's specular reflectance at normal incidence. normal: strength of the micro-normal
// derived from the albedo's luminance (deriveSurfaceNormal) when the texture has no normal map.
struct MaterialRange {const char* name;float lo,hi;bool metal;float f0,normal;};
const MaterialRange& materialRange(Material m);
inline const char* materialName(Material m){return materialRange(m).name;}
// A material from a texture's pack or engine name (lower case words: "metalrusted", "wood", "tile"...).
Material classifyMaterial(std::string_view name);

// The surface map, one texel per albedo texel: 0xAABBGGRR with R roughness, G metalness, B cavity
// (1 open, darker in pits and seams), A 255. `pixels` (0xAARRGGBB, sRGB) is adjusted in place: a
// metal's texels to its physical F0 (its albedo is its F0), dielectric albedo out of the near-black
// a real surface never has. Returns the map; its statistics go to materialRecords().
std::vector<std::uint32_t> deriveSurfaceMap(std::vector<std::uint32_t>& pixels,int width,int height,Material m,std::string_view name);

// What each derived texture came out as, for the lab: its class, the range of roughness it uses,
// the spread of it (coefficient of variation) and the share of it that is metal.
struct MaterialRecord {std::string name;Material material;float minRough,maxRough,meanRough,cov,metalShare;int texels;};
std::vector<MaterialRecord>& materialRecords();

// Split-sum environment BRDF, EnvBrdfSize x EnvBrdfSize, (scale, bias) pairs: column NoV (texel
// centres from 0 to 1), row roughness (texel centres from 0 to 1).
constexpr int EnvBrdfSize=32;
const std::vector<float>& envBrdfTable();
// Bilinear lookup in that table, as the shader samples it.
std::array<float,2> envBrdf(float NoV,float roughness);
// The specular lobe's directional albedo E(NoV) for F0 = 1, integrated by brute force over the
// hemisphere (uniform stratified directions, not importance sampling): the white-furnace reference
// the table and the compensation are checked against.
float ggxFurnace(float NoV,float roughness,int steps=192);
// The CPU rasteriser shades surface maps (Scene3D triangle3D); the lab turns it off for an A/B.
inline bool& cpuMaterials(){static bool on=true;return on;}
}
