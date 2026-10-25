#pragma once
#include "SoftwareRenderer.h"
#include <memory>
#include <utility>
#include <span>
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
 // Crisp keeps source texels square when magnified; smooth is plain bilinear.
 void setCrispTexels(bool crisp);
 // Ambient occlusion, distance haze, wide bloom and grade (HDR, non-VR only).
 void setPostProcessing(bool enabled,bool occlusionView=false);
 bool postProcessing()const;
 // Temporal anti-aliasing (windowed path only; headless frames stay exact).
 void setTemporalAA(bool enabled);
 // Cinematic lens in the window composite: depth of field with eased
 // autofocus, natural vignetting, chromatic aberration, film grain, halation.
 void setTiltShift(bool enabled);
 // Player lens settings: depth-of-field and lens-effect strength (1 = default look).
 void setLens(float depthOfField,float lensEffects);
 // Tests: headless readback uses the original analytic display curve.
 static void setReferenceDisplay(bool enabled);
 void setAtmosphere(const std::array<float,4>& atmosphere);
 bool beginStaticCache(int slot,std::uint64_t key);
 void endStaticCache();
 void clearStaticCaches();
 // Lamp shadow maps still queued: a changed lamp set redraws a bounded
 // number per frame, so a new view settles over several frames.
 bool shadowsPending()const;
 // Static lamps (world x, y, z, range, intensity, kelvin, tint) lit per pixel with cube
 // shadow maps on static-cache geometry. Disabled: lamps stay in the CPU bake.
 void setLamps(const std::vector<std::array<float,7>>& lamps,bool enabled,int detail=1);
 // Sun: direction xyz + intensity, colour rgb + pad, then three cascades of
 // four vec4 (origin + texel size, right/up/forward with inverse extents).
 void setSun(const std::array<float,56>& sun);
 // 0 the default sky; 1 the open zone (broken overcast, sun shafts, ground steam).
 void setSkyStyle(int style);
 // Painterly Horizon LOD: tags the static triangles submitted next. A real
 // mesh fades out past `lod` metres; an impostor quad (centre, half size,
 // yaw, atlas grid) fades in there and, for ground plants, out again at `farFade`.
 // The impostor's centre is the midpoint of its quad's opposite (uv -1/+1) corners.
 struct LodTag {float lod=0;bool impostor=false;float z=0,half=0,yaw=0,farFade=0;int columns=0,rows=0;};
 void setLodTag(const LodTag* tag);
 void saveSunCascades(const char* prefix);
 std::uint64_t shadowFacesDrawn()const;
 std::uint64_t staticCacheHits()const;
 void prepare(const SoftwareRenderer::Texture& texture);
 // prepare/submit collect immutable atlases; finish flushes them before draw.
 void flushUploads();
 // Images, transfer submissions, staging allocations, uploaded bytes.
 std::array<std::uint64_t,4> uploadStatistics()const;
 void captureVrEye(std::vector<std::uint32_t>& pixels);
 // Windowed path: read back the post-processed scene (before the HUD
 // composite) of the last presented frame, tone mapped like headless frames.
 void captureScene(std::vector<std::uint32_t>& pixels,int& width,int& height,std::vector<float>* depth=nullptr);
 void updateDynamic(const SoftwareRenderer::Texture& texture);
 void clearDepth();
 // Vulkan 1.0 vertex-stage work. One six-sided light record per actor,
 // shared by every triangle; final attachment positions remain authoritative.
 int beginActor(const std::array<Point3,6>& normals,const std::array<float,6>& brightness,const std::array<SoftwareRenderer::NormalLighting,6>& lights,Vec2 origin,Vec2 eye,Point3 center);
 // Immutable for one draw: resolve texture ownership and surface constants
 // outside the per-triangle loop. Positions and UV arithmetic stay unchanged.
 struct ActorMaterial {std::uint64_t key=0;std::uint32_t sortKey=0;float emission=0,normal=-1,parallax=0,surface=0;};
 struct ActorTriangle {float position[3][4],uv[3][2],settings[4],actor;ActorTriangle()noexcept{}};
 struct ActorStream {std::span<ActorTriangle> triangles;std::uint32_t start=0;};
 ActorStream beginActorStream(size_t maximum);
 void actorStreamRun(std::uint32_t start,std::uint32_t count,const ActorMaterial&material);
 void endActorStream(const ActorStream&stream,std::uint32_t count);
 static void writeActorTriangle(ActorTriangle&out,const MeshTriangle&face,int actor,const ActorMaterial&material){
  out.settings[0]=material.emission;out.settings[1]=material.normal;out.settings[2]=material.parallax;out.settings[3]=material.surface;out.actor=float(actor);
  for(size_t i=0;i<3;++i){const auto&v=face.v[i];auto&p=out.position[i];p[0]=v.p.x;p[1]=v.p.y;p[2]=v.p.z;p[3]=v.light;out.uv[i][0]=v.u;out.uv[i][1]=v.v;}
 }
 ActorMaterial prepareActorMaterial(const SoftwareRenderer::Texture& texture,float emissionScale=1.f);
 void submitActor(const MeshVertex& a,const MeshVertex& b,const MeshVertex& c,int actor,const ActorMaterial& material);
 // Cross-frame body/gun triangle reuse. Identity is exact float bits from the
 // caller; a hit resubmits retained world-space triangles with a fresh light slot.
 bool replayActorEmit(std::uint64_t slot,std::uint64_t identity,int actor);
 void commitActorEmit(std::uint64_t slot,std::uint64_t identity,std::uint32_t startTriangle);
 void clearActorEmitCache();
 void setActorEmitCaching(bool enabled);
 std::array<std::uint64_t,3> actorStatistics()const;
 void submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture& texture,float light,const std::array<Point3,2>& directions,const std::array<float,2>& weights,float flatResponse,bool normals,float emissionScale=1.f);
 void finish(std::vector<std::uint32_t>& pixels);
 bool hasSurface()const;
 bool vrActive()const;
 std::uint64_t mirrorFrames()const;
 void setVrEye(int eye,const std::array<float,16>& clip);
 std::pair<int,int> surfaceExtent()const;
 void present(const std::uint32_t* overlay,int overlayWidth,int overlayHeight,bool underwater,float sceneDim,float damageFlash,float shotKick,float alarm=0);
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
