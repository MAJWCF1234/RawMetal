#pragma once
#include "SoftwareRenderer.h"
#include <memory>
#include <utility>
namespace retro {
// Hardware rasterization; CPU scene/animation code and the deterministic
// software reference share the same triangle stream and authored materials.
class GpuRenderer {
public:
 explicit GpuRenderer(void* window=nullptr);
 ~GpuRenderer();
 void begin(int width,int height);
 void setView(float eyeX,float eyeY,float eyeZ,float yaw,float pitch,float aspect,bool flashlight,float muzzleFlash);
 bool beginStaticCache(int slot,std::uint64_t key);
 void endStaticCache();
 void clearStaticCaches();
 std::uint64_t staticCacheHits()const;
 void prepare(const SoftwareRenderer::Texture& texture);
 void clearDepth();
 void submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture& texture,float light,const std::array<Point3,2>& directions,const std::array<float,2>& weights,float flatResponse,bool normals,float emissionScale=1.f);
 void finish(std::vector<std::uint32_t>& pixels);
 bool hasSurface()const;
 std::pair<int,int> surfaceExtent()const;
 void present(const std::uint32_t* overlay,int overlayWidth,int overlayHeight,bool underwater,float sceneDim,float damageFlash,float shotKick);
 const std::string& adapter()const;
private:
 struct Impl;
 std::unique_ptr<Impl> m;
};
}
