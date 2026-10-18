#pragma once
#include <cstdint>
#include <vector>
namespace retro {
// NTX1 stores a lossless reduced RGB image and references one shared, learned
// NTM1 x2 subpixel model. Reconstruction runs once during resource loading.
uint32_t neuralTextureModelResource(const std::vector<unsigned char>& packet);
uint64_t neuralTextureReconstructionCount();
std::vector<unsigned char> reconstructNeuralTexture(const std::vector<unsigned char>& packet,const std::vector<unsigned char>& model);
}
