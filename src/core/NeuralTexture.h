#pragma once
#include <cstdint>
#include <vector>
namespace retro {
// A neural texture packet stores a reduced RGB image and names one shared,
// learned model resource. Reconstruction runs once, when the texture loads.
//  NTX1 + NTM1: lossless low-res, a 1,084-parameter x2 luma model (x4 runs it twice).
//  NTX2 + NTM2: lossless or lossy (VP8) low-res, an optional exact full-size
//   alpha plane, and a per-factor residual head (x2 or x4 in one pass).
uint32_t neuralTextureModelResource(const std::vector<unsigned char>& packet);
uint64_t neuralTextureReconstructionCount();
std::vector<unsigned char> reconstructNeuralTexture(const std::vector<unsigned char>& packet,const std::vector<unsigned char>& model);
}
