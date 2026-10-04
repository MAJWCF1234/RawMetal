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
 void setView(float eyeX,float eyeY,float eyeZ,float yaw,float pitch,float aspect,bool flashlight,float muzzleFlash,float elapsed=0.f);
 void setFogLights(const std::array<float,16>& lights,const std::array<float,4>& powers={1.f,1.f,1.f,1.f});
 void setAtmosphere(const std::array<float,4>& atmosphere);
 bool beginStaticCache(int slot,std::uint64_t key);
 void endStaticCache();
 void clearStaticCaches();
 std::uint64_t staticCacheHits()const;
 void prepare(const SoftwareRenderer::Texture& texture);
 // prepare/submit collect immutable atlases; finish flushes them before draw.
 void flushUploads();
 // Images, transfer submissions, staging allocations, uploaded bytes.
 std::array<std::uint64_t,4> uploadStatistics()const;
 void captureVrEye(std::vector<std::uint32_t>& pixels);
 void updateDynamic(const SoftwareRenderer::Texture& texture);
 void clearDepth();
 void submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture& texture,float light,const std::array<Point3,2>& directions,const std::array<float,2>& weights,float flatResponse,bool normals,float emissionScale=1.f);
 void finish(std::vector<std::uint32_t>& pixels);
 bool hasSurface()const;
 bool vrActive()const;
 std::uint64_t mirrorFrames()const;
 void setVrEye(int eye,const std::array<float,16>& clip);
 std::pair<int,int> surfaceExtent()const;
 void present(const std::uint32_t* overlay,int overlayWidth,int overlayHeight,bool underwater,float sceneDim,float damageFlash,float shotKick);
 const std::string& adapter()const;
 static bool testEmissionMips();
 static bool testDiffuseMips();
 static bool testGameDiffuseEncoding(const std::vector<const SoftwareRenderer::Texture*>& textures,std::array<uint64_t,4>& statistics,std::array<double,2>& milliseconds);
 static bool testNormalMips();
private:
 struct Impl;
 std::unique_ptr<Impl> m;
};
}
