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
 void beginReuse();
 void setView(float eyeX,float eyeY,float eyeZ,float yaw,float pitch,float aspect,bool flashlight,float muzzleFlash,float elapsed=0.f);
 // The four nearest lamps for the analytic shafts (VR, or no post stage): world
 // x, y, top, bottom, their powers and their linear tones (WorldLight::tone).
 void setFogLights(const std::array<float,16>& lights,const std::array<float,4>& powers={1.f,1.f,1.f,1.f},const std::array<Point3,4>& tones={WorldLight{}.tone(),WorldLight{}.tone(),WorldLight{}.tone(),WorldLight{}.tone()});
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
 void dropStaticCache(int slot);
 // Lamp shadow maps still queued: a changed lamp set redraws a bounded
 // number per frame, so a new view settles over several frames.
 bool shadowsPending()const;
 // Static lamps (world x, y, z, range, intensity, kelvin, tint, shadows, fitting
 // half width, half depth, fitting top, clearance, yaw, gel r, g, b) lit per pixel
 // with cube shadow maps on static-cache geometry. Disabled: lamps stay in the CPU bake.
 void setLamps(const std::vector<std::array<float,16>>& lamps,bool enabled,int detail=1);
 // Per-frame power (0..1) for the lamps last passed to setLamps, in the same
 // order: switching and flicker without a new key, grid or shadow redraw.
 void setLampPowers(const std::vector<float>& powers);
 // True darkness 0..1 (ambient fill and its floor, exposure ceiling, grade lift),
 // the muzzle flash falloff and the transient lights besides it (Game::weaponFlashes: NPC guns in
 // darkness, sparks and arcs; world xyz, strength, linear tone, reach).
 void setDarkness(float darkness,float muzzleFalloff,const std::array<electric::Flash,4>& flashes);
 // Sun: direction xyz + intensity, colour rgb + pad, then three cascades of
 // four vec4 (origin + texel size, right/up/forward with inverse extents).
 void setSun(const std::array<float,56>& sun);
 // The colour of the outdoor sky fill (World::skyAmbientTone); off keeps it white.
 // exposure: the ceiling on auto exposure under this sky (1 as by day).
 void setSkyAmbient(float r,float g,float b,bool on,float exposure=1.f);
 // Surfaces fade into the haze between start and end metres of depth (post.frag); end 0: no fade.
 void setViewFade(float start,float end);
 // Would beginStaticCache(slot,key) rebuild (no cache there, or another key)? Reads nothing, changes nothing.
 bool staticCacheCurrent(int slot,std::uint64_t key)const;
 // Draw an existing cache as it is (a stand-in while its replacement waits for a frame's bake budget); false if none.
 bool drawCached(int slot);
 // 0 the default sky; 1 the open zone (broken overcast, sun shafts, ground steam).
 void setSkyStyle(int style);
 // An ash storm's strength (0-1): the open-zone sky closes over brown (lampGrid.flags.w).
 void setStorm(float storm);
 // Painterly Horizon LOD: tags the static triangles submitted next. A real
 // mesh fades out past `lod` metres; an impostor quad (centre, half size,
 // yaw, atlas grid) fades in there and, for ground plants, out again at `farFade`.
 // The impostor's centre is the midpoint of its quad's opposite (uv -1/+1) corners.
 struct LodTag {float lod=0;bool impostor=false;float z=0,half=0,yaw=0,farFade=0;int columns=0,rows=0;};
 void setLodTag(const LodTag* tag);
 // Plant physbones (Foliage.h): the static triangles submitted next are plant `slot`'s (-1: none);
 // scene.vert bends them by the plant state set each frame (wind, then per slot), read after the lamp grid.
 void setPlantTag(int slot);
 void setPlants(const float* data,size_t floats);
 // Light probes (LightProbes.h) of a map drawn this frame, at its world offset. Its irradiance
 // grid is uploaded when its key is new; its reflection probes are each captured once (six faces
 // through the scene pipeline, once the lamps' shadows are drawn), a few a frame in the window
 // and all at once headless, and again when the key changes.
 void useProbes(int level,const ProbeBake* bake,std::uint64_t key,Vec2 offset);
 bool reflectionsPending()const;
 std::uint64_t reflectionCaptures()const;
 void saveSunCascades(const char* prefix);
 std::uint64_t shadowFacesDrawn()const;
 std::uint64_t staticCacheHits()const;
 // The last finish()'s scene pass: draws, descriptor-set binds, pipeline binds, vertex-buffer binds,
 // triangles drawn from static caches, dynamic triangles drawn, actor triangles drawn, static caches
 // drawn, shadow-map draws (lamp faces + sun cascades), shadow triangles, dynamic vertex bytes and
 // actor bytes uploaded this frame.
 enum DrawStat {Draws,DescriptorBinds,PipelineBinds,BufferBinds,CachedTriangles,DynamicTriangles,ActorTriangles,CachesDrawn,ShadowDraws,ShadowTriangles,DynamicBytes,ActorBytes};
 std::array<std::uint64_t,12> drawStatistics()const;
 // The last headless finish(), in ms: CPU command recording, the wait for the GPU, the readback's
 // conversion to display pixels (the windowed path presents instead: zeros).
 std::array<double,3> finishTimes()const;
 void prepare(const SoftwareRenderer::Texture& texture);
 // Many at once, their pixels worked on every core (start-up, a map warmed).
 void prepare(const std::vector<const SoftwareRenderer::Texture*>& textures);
 // In order: a texture is made into a material, or (texture null) a place in the material
 // order is kept and written to reserveSlot, for a texture that comes later with it as sortSlot.
 struct PrepareItem {const SoftwareRenderer::Texture* texture=nullptr;std::uint32_t* reserveSlot=nullptr;};
 void prepare(const std::vector<PrepareItem>& items);
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
 // light: the face's own scalar on the side brightness (lighting.x), as the CPU path's tri(light)
 // multiplies it: 1 for people; a creature's relief, pain flash, wing glow or dark lamp part.
 struct ActorTriangle {float position[3][4],uv[3][2],settings[4],actor,light;ActorTriangle()noexcept{}};
 struct ActorStream {std::span<ActorTriangle> triangles;std::uint32_t start=0;};
 ActorStream beginActorStream(size_t maximum);
 void actorStreamRun(std::uint32_t start,std::uint32_t count,const ActorMaterial&material);
 void endActorStream(const ActorStream&stream,std::uint32_t count);
 static void writeActorTriangle(ActorTriangle&out,const MeshTriangle&face,int actor,const ActorMaterial&material,float faceLight=1.f){
  out.settings[0]=material.emission;out.settings[1]=material.normal;out.settings[2]=material.parallax;out.settings[3]=material.surface;out.actor=float(actor);out.light=faceLight;
  for(size_t i=0;i<3;++i){const auto&v=face.v[i];auto&p=out.position[i];p[0]=v.p.x;p[1]=v.p.y;p[2]=v.p.z;p[3]=v.light;out.uv[i][0]=v.u;out.uv[i][1]=v.v;}
 }
 ActorMaterial prepareActorMaterial(const SoftwareRenderer::Texture& texture,float emissionScale=1.f);
 void submitActor(const MeshVertex& a,const MeshVertex& b,const MeshVertex& c,int actor,const ActorMaterial& material);
 // Cross-frame body/gun triangle reuse. Identity is exact float bits from the
 // caller; a hit resubmits retained world-space triangles with a fresh light slot.
 bool replayActorEmit(std::uint64_t slot,std::uint64_t identity,int actor);
 std::uint32_t actorEmitPosition()const;
 void commitActorEmit(std::uint64_t slot,std::uint64_t identity,std::uint32_t startTriangle);
 void clearActorEmitCache();
 void setActorEmitCaching(bool enabled);
 std::array<std::uint64_t,3> actorStatistics()const;
 void submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture& texture,float light,const std::array<Point3,2>& directions,const std::array<float,2>& weights,float flatResponse,bool normals,float emissionScale=1.f,float dissolve=0.f,const std::array<float,3>* bounceAir=nullptr,const SoftwareRenderer::CardLook* card=nullptr);
 void finish(std::vector<std::uint32_t>& pixels);
 bool hasSurface()const;
 bool vrActive()const;
 std::uint64_t mirrorFrames()const;
 void setVrEye(int eye,const std::array<float,16>& clip);
 // VR dynamic resolution: each eye is drawn into the top-left share `scale` (0.6-1, each axis) of its image and
 // SteamVR is told to stretch that part (VrRuntime::submit bounds). Set before the first eye of a frame.
 void setVrRenderScale(float scale);
 std::pair<int,int> surfaceExtent()const;
 void present(const std::uint32_t* overlay,int overlayWidth,int overlayHeight,bool underwater,float sceneDim,float damageFlash,float shotKick,float alarm=0);
 const std::string& adapter()const;
 static bool testEmissionMips();
 static bool testDiffuseMips();
 static bool testGameDiffuseEncoding(const std::vector<const SoftwareRenderer::Texture*>& textures,std::array<uint64_t,4>& statistics,std::array<double,2>& milliseconds);
 static bool testNormalMips();
 // Lamp tones: white gels keep the blackbody, gels hold luminance under their cap,
 // and the packed half-float colour slots round-trip as normal finite floats.
 static bool testLampTones();
private:
 struct Impl;
 std::unique_ptr<Impl> m;
};
}
