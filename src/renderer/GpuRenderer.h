#pragma once
#include "SoftwareRenderer.h"
#include <memory>
namespace retro {
// Hardware rasterization; CPU scene/animation code and the deterministic
// software reference share the same triangle stream and authored materials.
class GpuRenderer {
public:
 explicit GpuRenderer(void* window=nullptr);
 ~GpuRenderer();
 void begin(int width,int height);
 void prepare(const SoftwareRenderer::Texture& texture);
 void clearDepth();
 void submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture& texture,float light,const std::array<Point3,2>& directions,const std::array<float,2>& weights,float flatResponse,bool normals,float emissionScale=1.f);
 void finish(std::vector<std::uint32_t>& pixels);
 bool hasSurface()const;
 void present(const std::uint32_t* overlay,int width,int height,bool underwater,float sceneDim);
 const std::string& adapter()const;
private:
 struct Impl;
 std::unique_ptr<Impl> m;
};
}
