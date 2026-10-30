#pragma once
#include "../game/Game.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include "Mesh.h"
#include "../core/CleanupBodies.h"

namespace retro {
class GpuRenderer;
class FrameWorker;
class SoftwareRenderer {
public:
    SoftwareRenderer(int width, int height);
    ~SoftwareRenderer();
    bool enableHardware(void* window=nullptr);
    bool hardwareActive()const{return bool(m_gpu);}
    // Windowed path: the post-processed scene of the last frame as a PPM.
    bool saveSceneCapture(const char* path);
    void saveSunCascades(const char* prefix);
    // Painterly Horizon LOD: write each captured atlas; switch impostors off for A/B views.
    void saveImpostors(const std::string& prefix)const;
    static inline bool s_impostors=true;
    bool hardwarePresentsWindow()const;
    std::uint64_t vrMirrorFrames()const;
    std::array<std::uint64_t,4> atlasUploadStatistics()const;
    const std::string& hardwareName()const{return m_gpuName;}
    void render(const Game& game);
    void saveVrEye(const char* path);
    void renderVrEye(const Game& game,int eye);
    void setEnvironmentInspection(bool enabled){m_environmentInspection=enabled;}
    const std::uint32_t* pixels() const { return m_pixels.data(); }
    int width() const { return m_width; }
    int height() const { return m_height; }
    std::string modelReport()const{auto report=m_weaponMesh.description+m_armsMesh.description+m_enemyMesh.description+m_waspMesh.description+m_bruteMesh.description+m_wardenMesh.description+m_mutantMesh.description+m_snatcherMesh.description+m_chimeraMesh.description+m_transformedMesh.description+m_barrelMesh.description+m_crateMesh.description+m_medkitMesh.description+m_shellsMesh.description+m_pumpMesh.description+m_compressorMesh.description+m_pipeMesh.description+m_gateMesh.description;for(auto&mesh:m_facilityMeshes)report+=mesh.description;return report;}
    void previewModel(int model,float angle);
    bool validate3D();
    static bool testPerformance();
    static bool testVisibility();
    static bool testGpuActors();
    void setGpuActorPreparation(bool enabled){m_gpuActorPreparation=enabled;}
    void setPanelCaching(bool enabled){m_panelCaching=enabled;}
    void setActorEmitCaching(bool enabled){m_actorEmitCaching=enabled;}
    static bool testCablePerformance();
    // Every Surface value and structure finish resolves to decodable textures.
    static bool testSurfaceSets(std::string* report=nullptr);
    static bool testHardware();
    static bool testPresentationResize();
    static bool testCreatureAnimation();
    static bool testProjection();
    // Store-low / run-high: Vulkan presents bilinear-upsampled atlases (1/2/4).
    static void setTexturePresentScale(int scale);
    static int texturePresentScale();
    static void setCrispTexels(bool crisp);
    static bool crispTexels();
    static void setPostProcessing(bool enabled,bool occlusionView=false);
    static bool postProcessing();
    // Per-pixel lamps with cube shadow maps on the static world (Vulkan only).
    static void setPixelLighting(bool enabled);
    static bool pixelLighting();
    // Close-up procedural detail layer on per-pixel-lit surfaces.
    // 0 off, 1 on, 2 debug: raw field, 3 debug: detail normal.
    static void setDetailLayer(int mode);
    static int detailLayer();
    // Temporal anti-aliasing in the window (headless captures stay exact).
    static void setTemporalAA(bool enabled);
    static bool temporalAA();
    static void setTiltShift(bool enabled);
    static bool tiltShift();
    float gripError()const{return m_gripError;}
    std::array<float,4> cleanupAttachmentErrors()const{return {m_cleanupReachError,m_cleanupFingerError,m_cleanupBarrelError,m_cleanupEdgeStretch};}
    // Inspection only: 1 = donor bake, 2 = composed layers, 3 = bind mesh,
    // 0 = final attachment (ordinary gameplay).
    void setCleanupInspectionStage(int stage){m_cleanupInspectionStage=stage;}
    std::uint64_t staticGeometryBuilds()const{return m_staticGeometryBuilds;}
    // Lamp shadow faces drawn so far (Vulkan); 0 on the software path.
    std::uint64_t shadowFacesDrawn()const;
    std::array<double,3> frameStages()const{return {m_sceneMs,m_submitMs,m_presentMs};}
    void inspectRig(const Game& game,float yaw,float pitch);
private:
    friend class GpuRenderer;
    std::unique_ptr<GpuRenderer> m_gpu;
    std::unique_ptr<FrameWorker> m_animationWorker;
    std::string m_gpuName;
    bool m_gpuFrame=false;
    bool m_gpuActorPreparation=true;
    bool m_panelCaching=true;
    bool m_actorEmitCaching=true;
    bool m_actorStreamWriting=true;
    struct PanelRaster {
        std::array<int,6> key{};
        std::uint64_t textureGeneration=0;
        std::vector<std::uint32_t> pixels;
    };
    // Bounded retained decoration only; live text, meters and prompts draw over it.
    std::array<PanelRaster,16> m_panelRasters;
    size_t m_nextPanelRaster=0;
    bool m_vrRendering=false;
    // This frame's camera (Game::view): the player, or a locker's path in or out.
    Player m_view;
    bool m_vrReuseScene=false; // second eye: replay the first eye's geometry
    std::array<std::vector<MeshTriangle>,2> m_vrHandMeshes;
    std::vector<MeshTriangle> m_vrSupportGripMesh; // the left hand closed round the fore-end
    void drawTrackedHands(const Game& game);
    bool m_environmentInspection=false;
    bool m_staticGeometryBuild=false;
    // Static caches bake ambient only; Vulkan adds lamps per pixel.
    bool m_pixelLighting=false;
    std::uint64_t m_staticGeometryBuilds=0;
    float m_emissionScale=1.f;
    double m_sceneMs=0,m_submitMs=0,m_presentMs=0;
    bool m_poseReady=false;
    struct TextureGeneration {
        static std::uint64_t next(){static std::atomic<std::uint64_t> counter{1};return counter.fetch_add(1,std::memory_order_relaxed);}
        std::uint64_t value=next();
        TextureGeneration()=default;
        TextureGeneration(const TextureGeneration&):value(next()){}
        TextureGeneration& operator=(const TextureGeneration& other){if(this!=&other)value=next();return *this;}
        TextureGeneration(TextureGeneration&& other)noexcept:value(other.value){other.value=next();}
        TextureGeneration& operator=(TextureGeneration&& other)noexcept{if(this!=&other){value=other.value;other.value=next();}return *this;}
    };
    // GPU atlases are immutable after prepare/first submit; updateDynamic is
    // the explicit exception for dynamic colour pixels. Copies own a fresh
    // generation, while moves transfer it with the uploaded pixel allocation.
    struct Texture { int width=0, height=0; std::vector<std::uint32_t> pixels; bool clampEdges=false; std::vector<std::vector<std::uint32_t>> mips; bool additive=false; std::vector<std::vector<Point3>> normalLevels; std::vector<std::uint8_t> relief; float parallaxScale=0; float glossStrength=0; std::vector<std::uint32_t> emission; bool transparent=false; TextureGeneration generation;
        // A dado band: drawn once over its first `band` metres above the floor, `above` over the rest of the face.
        float band=0; const Texture* above=nullptr; };
    struct NormalLighting {std::array<Point3,2> directions{};std::array<float,2> weights{};Point3 surfaceNormal{};};
    static void attachNormal(Texture& texture,int resource,bool greenUp=true,float reliefScale=.012f);
    static void deriveSurfaceNormal(Texture& texture,float strength);
    static Point3 sampleNormal(const Texture& texture,float u,float v,float lod);
    bool testNormalMapping();
    Texture m_vrWristTexture;
    // Alyx-style VR displays: health on the back of the left hand, the load on
    // the gun, and area titles and subtitles floating ahead (additive glass).
    Texture m_vrHealthTexture,m_vrAmmoTexture,m_vrTitleTexture,m_vrCaptionTexture;
    int m_vrHealthShown=-1,m_vrAmmoShown=-1;float m_vrTitleShown=-1;std::string m_vrCaptionShown,m_vrObjective;float m_vrObjectiveTime=0;
    Texture m_muzzleFlash;
    Texture m_water,m_coastWater;
    Texture m_terrainDirt,m_terrainRock,m_coastSand,m_coastRock;
    Texture m_ashfallSky,m_coastSky;
    Mesh m_consoleMesh{240};
    Texture m_consoleTexture;
    std::array<Texture,3> m_hazmatTextures;
    std::vector<RagPoint> m_hazmatPose;
    std::array<RagPoint,Ragdoll::Count> m_hazmatPoseJoints{};
    bool m_hazmatPoseValid=false;
    Texture m_blood;
    std::array<Texture,size_t(CreatureKind::Transformed)+1> m_bloodVariants;
    Mesh m_pumpMesh{140},m_compressorMesh{142},m_pipeMesh{144},m_gateMesh{146};
    Texture m_pumpTexture,m_compressorTexture,m_pipeTexture,m_gateTexture,m_pressureWall,m_pressureFloor,m_pressureMetal;
    Texture m_transferSign,m_pumpSign,m_controlSign,m_surfaceSign,m_liftSign,m_liftDispatch;
    Texture m_feedSign,m_returnSign;
    // The two door notices (freight suspended, utilities permit): Door.sign-CampaignChunkCount.
    std::array<Texture,3> m_noticeSigns;
    std::array<Texture,3> m_paperProps; // Readable::Letter, Note, Sticky
    // Door header plates of authored maps, one per HUD zone name (lazy).
    std::array<Texture,3> m_serviceAreaSigns;
    Texture m_wall, m_floor, m_metal, m_serviceFloor, m_serviceCeiling, m_arms,m_officeCarpet,m_framePaint;
    Texture m_yellowSteel,m_redSteel,m_paleSteel,m_panelSteel;
    std::array<Mesh,6> m_clutterMeshes{Mesh{151},Mesh{153},Mesh{155},Mesh{157},Mesh{159},Mesh{161}};
    std::array<Texture,6> m_clutterTextures;
    Texture m_weaponTexture,m_enemyTexture;
    Mesh m_weaponMesh{109},m_armsMesh{111},m_enemyMesh{110};
    Mesh m_waspMesh{114},m_bruteMesh{116},m_wardenMesh{242},m_mutantMesh{272,"",274},m_snatcherMesh{2126,"",2128},m_chimeraMesh{2129,"",2131};
    // The transformed: the Worker's body (277) on its own six baked clips.
    Mesh m_transformedMesh{277,"",2185};
    Mesh m_workerMesh{277,"",278};
    std::array<Texture,3> m_workerTextures;
    // Research staff (ActorVisual::Scientist..LabTech), each baked with the
    // Worker's six clips and its own hand anchors. A lab coat is one material;
    // the light suit wears the Worker's gloves and boots (the pack ships the
    // same two textures for both suits) with its own body and respirator.
    std::array<Mesh,5> m_staffMeshes{Mesh{693,"",694,695},Mesh{697,"",698,699},Mesh{701,"",702,703},Mesh{705,"",706,707},Mesh{709,"",710,711}};
    std::array<Texture,6> m_staffTextures; // four lab coats, light suit body, respirator
    // civilianActorBody: 0 the Worker, 1-5 the staff.
    Mesh& civilianMesh(int body){return body>0?m_staffMeshes[size_t(body-1)]:m_workerMesh;}
    const Texture& civilianTexture(int body,int material)const;
    Mesh m_cleanupMesh{303,"",304};
    Mesh m_cleanupEngineer{308,"",309},m_cleanupSharpshooter{313,"",314};
    Mesh m_cleanupHeavy{318,"",319},m_cleanupMedic{323,"",324},m_cleanupRusher{328,"",329};
    Mesh m_cleanupInvestigator{cleanupBodyResources(6).mesh,"",cleanupBodyResources(6).animation};
    // Indexed by cleanupActorBody.
    std::array<Mesh*,kCleanupBodies> cleanupMeshes(){return {&m_cleanupMesh,&m_cleanupEngineer,&m_cleanupSharpshooter,&m_cleanupHeavy,&m_cleanupMedic,&m_cleanupRusher,&m_cleanupInvestigator};}
    Mesh m_engineerDrone{333,"",334};
    Texture m_droneTexture,m_handgunTexture;
    Mesh m_handgunMesh{337};
    std::array<std::array<Texture,2>,kCleanupBodies> m_cleanupTextures;
    std::array<std::array<std::array<Point3,3>,2>,kCleanupBodies> m_cleanupArms{};
 std::array<std::array<Point3,2>,kCleanupBodies> m_cleanupPalms{};
 std::array<std::vector<std::array<std::uint8_t,4>>,kCleanupBodies> m_cleanupWeights;
    Texture m_glassTexture;
    // Pink/black checker for any surface whose texture is missing; see
    // assets/materials/PROP-SURFACES-SOURCE.md for the purchased replacements.
    Texture m_officeWall,m_dropCeiling,m_securityCarpet,m_officeTile;
    // Campaign maps 0-2: Foundry brick/cast plate, Pressure Works riveted panel/concrete,
    // Turbine Gantry green steel/concrete.
    std::array<Texture,3> m_areaWalls,m_areaFloors;
    // Surface sets (World.h Surface): a map's wall, floor and ceiling finish and
    // its repeat in metres. Pressure and office plaster point at the materials
    // maps already use; the rest are the pack finishes below.
    struct SurfaceSet {const Texture* wall=nullptr;const Texture* floor=nullptr;const Texture* ceiling=nullptr;Vec2 wallRepeat{2,3};float floorRepeat=.5f,ceilingRepeat=.5f;};
    std::array<SurfaceSet,SurfaceCount> m_surfaceSets;
    // Pack finishes (tools/library/surfaces.py, resources 2100-2124 and 2150-2154):
    // structure materials 34 resonance growth and 35 crystal, then the clinic
    // paint, its three dado bands (materials 36-38) and ward curtain fabric (39).
    std::array<Texture,30> m_surfaceTextures;
    static Texture loadSurfaceFinish(int index);
    const SurfaceSet* surfaceSet(int surface)const{return surface>=0&&surface<SurfaceCount?&m_surfaceSets[size_t(surface)]:nullptr;}
    // Structure materials 26-29 and 31-39, or nullptr for every other material.
    const Texture* finishMaterial(int material)const;
    // Metres per repeat of a pack finish; {0,0} for any other texture.
    Vec2 finishRepeat(const Texture& texture)const;
    Texture m_missingTexture,m_ironTexture,m_darkMetalTexture,m_rubberTexture,m_leatherTexture,m_idFabricTexture,m_medkitCaseTexture,m_medkitLabelTexture;
    Texture m_waspTexture,m_bruteTexture,m_wingTexture,m_wardenTexture,m_mutantTexture,m_snatcherTexture,m_chimeraTexture;
    // The Worker's body, gloves and boots, scorched and drained by the zone.
    Texture m_transformedTexture,m_transformedGloves,m_transformedBoots;
    Mesh m_barrelMesh{121},m_crateMesh{123};
    std::array<Mesh,FacilityModelCount> m_facilityMeshes{Mesh{163},Mesh{164},Mesh{165},Mesh{166},Mesh{167},Mesh{168,"doorway_wide_1"},Mesh{169},Mesh{170},Mesh{171},Mesh{168,"door_wide_1_bottom"},Mesh{168,"door_wide_1_top"},Mesh{191},Mesh{142},Mesh{258},Mesh{259},Mesh{263},Mesh{265},Mesh{275},Mesh{284},Mesh{285},Mesh{286},Mesh{288},Mesh{290},Mesh{294},
        Mesh{359},Mesh{360},Mesh{361},Mesh{362},Mesh{363},Mesh{364},Mesh{365},Mesh{366},Mesh{367},Mesh{368},Mesh{369},Mesh{370},Mesh{371},Mesh{372},Mesh{373},
        Mesh{399},Mesh{400},Mesh{401},Mesh{402},Mesh{403},Mesh{404},Mesh{405},Mesh{406},
        Mesh{427},Mesh{428},Mesh{429},Mesh{430},Mesh{431},Mesh{432},Mesh{433},Mesh{434},Mesh{435},
        Mesh{443},Mesh{444},Mesh{445},Mesh{446},Mesh{447},Mesh{448},Mesh{464},
        Mesh{467},Mesh{469},Mesh{471},Mesh{474},Mesh{477},Mesh{482},Mesh{484},Mesh{486},Mesh{488},Mesh{490},Mesh{496},Mesh{498},Mesh{500},Mesh{502},Mesh{504},Mesh{506},Mesh{508},Mesh{510},Mesh{514},Mesh{516},Mesh{530},Mesh{532},Mesh{534},Mesh{536},Mesh{538},Mesh{540},Mesh{542},Mesh{544},Mesh{546},Mesh{548},Mesh{550},Mesh{552},Mesh{554},Mesh{556},Mesh{558},Mesh{560},Mesh{562},Mesh{564},Mesh{566},Mesh{568},Mesh{570},Mesh{572},Mesh{574},Mesh{576},Mesh{578},Mesh{580},Mesh{582},Mesh{584},Mesh{586},Mesh{588},Mesh{590},Mesh{593},Mesh{596},Mesh{598},Mesh{603},Mesh{605},Mesh{608},Mesh{611},Mesh{614},Mesh{617},Mesh{620},Mesh{622},Mesh{624},Mesh{626},Mesh{628},Mesh{630},Mesh{633},Mesh{635},Mesh{637},
        Mesh{647},Mesh{649},Mesh{651},Mesh{653},Mesh{655},Mesh{657},Mesh{659},Mesh{661},Mesh{663},Mesh{665},Mesh{667},Mesh{669},Mesh{671},Mesh{673},Mesh{675},Mesh{677},Mesh{679},Mesh{682},Mesh{685},Mesh{687},Mesh{689},Mesh{691},
        Mesh{745},Mesh{747},Mesh{749},Mesh{751},Mesh{753},Mesh{755},Mesh{757},Mesh{759},Mesh{761},Mesh{763},Mesh{765},Mesh{767},Mesh{769},Mesh{771},Mesh{773},Mesh{775},Mesh{777},Mesh{779},Mesh{781},Mesh{783},Mesh{785},Mesh{787},Mesh{789},Mesh{791},Mesh{793},Mesh{795},Mesh{797},Mesh{799},Mesh{801},Mesh{803},Mesh{805},Mesh{807},Mesh{809},Mesh{811},Mesh{815},Mesh{817},Mesh{820},Mesh{822},Mesh{824},Mesh{827},Mesh{829},Mesh{833},Mesh{835},Mesh{837},Mesh{839},Mesh{841},Mesh{843},Mesh{845},Mesh{848},Mesh{852},Mesh{854},Mesh{858},Mesh{860},Mesh{862},Mesh{866},Mesh{868},Mesh{870},Mesh{872},Mesh{874},Mesh{876},Mesh{878},Mesh{880},Mesh{882},Mesh{884},Mesh{886},Mesh{888},Mesh{890},Mesh{892},Mesh{895},Mesh{898},Mesh{900},Mesh{902},Mesh{904},Mesh{906},Mesh{908},Mesh{910},Mesh{912},Mesh{914},Mesh{916},Mesh{918},Mesh{920},Mesh{922},Mesh{924},Mesh{926},Mesh{928},Mesh{930},Mesh{932},Mesh{934},Mesh{936},Mesh{938},Mesh{941},Mesh{943},Mesh{946},Mesh{948},Mesh{957},Mesh{959},Mesh{961},Mesh{963},Mesh{965},Mesh{968},Mesh{971},Mesh{973},Mesh{975},Mesh{977},Mesh{979},Mesh{981},Mesh{984},Mesh{987},Mesh{990},Mesh{992},Mesh{994},Mesh{996},Mesh{998},Mesh{1000},Mesh{1002},Mesh{1004},Mesh{1007},Mesh{1010},Mesh{1012},Mesh{1014},Mesh{1016},Mesh{1018},Mesh{1021},Mesh{1026},Mesh{1028},Mesh{1031},Mesh{1033},Mesh{1035},Mesh{1037},Mesh{1039},Mesh{1041},Mesh{1043},Mesh{1045},Mesh{1047},Mesh{1049},Mesh{1051},Mesh{1056},Mesh{1058},Mesh{1060},Mesh{1062},Mesh{1064},Mesh{1066},Mesh{1068},Mesh{1070},Mesh{1072},Mesh{1074},Mesh{1076},Mesh{1078},Mesh{1080},Mesh{1082},Mesh{1086},Mesh{1088},Mesh{1090},Mesh{1092},Mesh{1094},Mesh{1096},Mesh{1098},Mesh{1100},Mesh{1102},Mesh{1104},Mesh{1107},Mesh{1109},Mesh{1111},Mesh{1113},Mesh{1115},Mesh{1117},Mesh{1119},Mesh{1121},Mesh{1124},Mesh{1126},Mesh{1129},Mesh{1131},Mesh{1134},Mesh{1137},Mesh{1139},Mesh{1141},Mesh{1150},Mesh{1153},
        Mesh{2132},Mesh{2134},Mesh{2136},
        Mesh{2204},Mesh{2209},
        Mesh{2214},Mesh{2216},Mesh{2218},Mesh{2220},Mesh{2222},Mesh{2224},Mesh{2226},Mesh{2228},Mesh{2230},Mesh{2232},Mesh{2234},Mesh{2236},Mesh{2238},Mesh{2240},Mesh{2242}};
    std::unordered_map<std::string,Texture> m_authoredSigns;
    std::unordered_map<std::string,Texture> m_facilityTextures;
    const Texture& facilityTexture(int mesh,int part)const;
    // Painterly Horizon LOD (Impostors.cpp): a shared multi-view atlas per
    // repeated cutout model, captured on first use; null for other models.
    struct Impostor {Texture atlas;Point3 center{};float radius=0;};
    static constexpr int FirstLibraryModel=63,ImpostorColumns=8,ImpostorRows=3,ImpostorCell=96;
    const Impostor* impostorFor(int model);
    std::unordered_map<int,std::unique_ptr<Impostor>> m_impostors;
    Mesh m_medkitMesh{133},m_shellsMesh{135};
    Texture m_medkitTexture,m_shellsTexture;
    Texture m_barrelTexture,m_crateTexture,m_concrete,m_bulkhead;
    Texture m_hazard,m_chemicalSign,m_signRust,m_panelMetal,m_routePaint,m_redPaint;
    Texture m_terminalTexture,m_cautionSign;
    Texture m_logo;
    Texture makeSign(const char* title,const char* subtitle,std::uint32_t accent);
    // The sheet as seen lying in the world (letter, note, sticky), and the
    // same paper read in the hand.
    Texture makePaperProp(Readable kind);
    void drawPaper(const Game& game,int index);
    Texture makePaint(std::uint32_t color);
    static Texture loadEmbeddedPng(int id);
    static void prepareDecal(Texture& texture,bool clampEdges=true);
    void drawSky(const Game& game);
    void drawScene(const Game& game,bool clearDepth=true,int level=-1);
    void drawViewModel(const Game& game);
    void prepareViewModel(const Game& game);
    void triangle3D(MeshVertex a,MeshVertex b,MeshVertex c,const Texture& texture,float light,const NormalLighting* normalLighting=nullptr);
    Point3 cameraPoint(Point3 p,const Game& game)const;
    Point3 cameraPoint(Point3 p,const Player& player)const;
    static Texture loadTexture(int id);
    static std::uint32_t sample(const Texture& texture,float u,float v,float lod=0);
    void clear(std::uint32_t color);
    void put(int x,int y,std::uint32_t c);
    void rect(int x,int y,int w,int h,std::uint32_t c);
    void drawVrStatus(const Game& game);
    void drawTerminal(const Game& game);
    void prepareVrDisplays(const Game& game);
    void drawComputer(const Game& game,int terminal);
    void drawHud(const Game& game);
    void drawTitle(const Game& game);
    void drawLoading(const Game& game);
    void drawConsole(const Game& game);
    float m_frameMs=0;
    void drawSettings(const Game& game);
    void drawInventory(const Game& game);
    void drawDialogue(const Game& game);
    void text(int x,int y,const char* s,std::uint32_t c,int scale=1);
    // Anti-aliased UI type (faces: 0 condensed semibold, 1 condensed light, 2 display, 3 mono) and alpha-blended fills.
    int uiText(int x,int y,const char* s,std::uint32_t c,int face,int size,float alpha=1,int tracking=0);
    int uiTextWidth(const char* s,int face,int size,int tracking=0)const;
    void label(int x,int y,const char* s,std::uint32_t c,int scale=1);
    void blend(int x,int y,std::uint32_t c,float alpha);
    void fill(int x,int y,int w,int h,std::uint32_t c,float alpha);
    void gradient(int x,int y,int w,int h,std::uint32_t c,float from,float to,bool horizontal);
    void frame(int x,int y,int w,int h,std::uint32_t c,float alpha);
    void uiPanel(int x,int y,int w,int h);
    std::uint32_t shade(std::uint32_t c,float s) const;
    int m_width, m_height;
    std::vector<std::uint32_t> m_pixels;
    std::vector<float> m_depth;
    std::vector<float> m_zbuffer;
    std::vector<std::uint32_t> m_scenePixels;
    std::vector<float> m_sceneZ;
    bool m_visibilityCulling=true;
    bool m_cacheFixedServices=true;
    int m_shadowBudgetLimit=2200;
    std::array<std::unordered_map<std::uint64_t,float>,Game::MaxChunks> m_chunkLighting;
    std::array<std::unordered_map<std::uint64_t,NormalLighting>,Game::MaxChunks> m_chunkNormalLighting;
    std::array<std::vector<std::vector<size_t>>,Game::MaxChunks> m_chunkLightCells;
    std::array<size_t,Game::MaxChunks> m_chunkLightCounts{};
    std::array<std::uint64_t,Game::MaxChunks> m_chunkLightSourceKeys{};
    WorldId m_lightingWorld=WorldId::Campaign;
    unsigned m_lightingSession=0;
    float m_gripError=0;
    int m_cleanupInspectionStage=0;
    float m_cleanupReachError=0,m_cleanupFingerError=0,m_cleanupBarrelError=0,m_cleanupEdgeStretch=1;
    bool m_inspectRig=false;
    float m_inspectYaw=0,m_inspectPitch=0;
    Point3 inspectionPoint(Point3 p)const;
    void wornPanel(int x,int y,int width,int height,bool recess=false,bool materialPanel=false);
    struct CreatureVisual {Mesh SoftwareRenderer::*mesh;Texture SoftwareRenderer::*texture;bool xenoblood;float bloodLight;};
    static constexpr std::array creatureVisuals{
     CreatureVisual{&SoftwareRenderer::m_enemyMesh,&SoftwareRenderer::m_enemyTexture,true,1.f},
     CreatureVisual{&SoftwareRenderer::m_waspMesh,&SoftwareRenderer::m_waspTexture,true,1.f},
     CreatureVisual{&SoftwareRenderer::m_bruteMesh,&SoftwareRenderer::m_bruteTexture,false,.9f},
     CreatureVisual{&SoftwareRenderer::m_wardenMesh,&SoftwareRenderer::m_wardenTexture,false,.9f},
     CreatureVisual{&SoftwareRenderer::m_mutantMesh,&SoftwareRenderer::m_mutantTexture,false,.9f},
     CreatureVisual{&SoftwareRenderer::m_snatcherMesh,&SoftwareRenderer::m_snatcherTexture,false,.9f},
     CreatureVisual{&SoftwareRenderer::m_chimeraMesh,&SoftwareRenderer::m_chimeraTexture,false,.9f},
     CreatureVisual{&SoftwareRenderer::m_transformedMesh,&SoftwareRenderer::m_transformedTexture,false,.9f}};
    static_assert(creatureVisuals.size()==size_t(CreatureKind::Transformed)+1);
};
}

