#pragma once
#include <vector>
namespace retro {
// RMP1: eight little-endian words: magic, material, width, height, seed,
// base RGB, oxide RGB and detail RGB. Produces the ordinary RMT1 RGBA format.
std::vector<unsigned char> generateProceduralTexture(const std::vector<unsigned char>& recipe);
}
