#pragma once
#include "../game/Game.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Mesh.h"

namespace retro {
class GpuRenderer;
class FrameWorker;
class SoftwareRenderer {
public:
    SoftwareRenderer(int width, int height);
    ~SoftwareRenderer();
    bool enableHardware();
    bool hardwareActive()const{return bool(m_gpu);}
    const std::string& hardwareName()const{return m_gpuName;}
    void render(const Game& game);
    const std::uint32_t* pixels() const { return m_pixels.data(); }
    int width() const { return m_width; }
    int height() const { return m_height; }
    std::string modelReport()const{auto report=m_weaponMesh.description+m_armsMesh.description+m_enemyMesh.description+m_waspMesh.description+m_bruteMesh.description+m_barrelMesh.description+m_crateMesh.description+m_medkitMesh.description+m_shellsMesh.description+m_pumpMesh.description+m_compressorMesh.description+m_pipeMesh.description+m_gateMesh.description;for(auto&mesh:m_facilityMeshes)report+=mesh.description;return report;}
    void previewModel(int model,float angle);
    bool validate3D();
    static bool testPerformance();
    static bool testHardware();
    static bool testCreatureAnimation();
    float gripError()const{return m_gripError;}
    void inspectRig(const Game& game,float yaw,float pitch);
private:
    friend class GpuRenderer;
    std::unique_ptr<GpuRenderer> m_gpu;
    std::unique_ptr<FrameWorker> m_animationWorker;
    std::string m_gpuName;
    bool m_gpuFrame=false;
    float m_emissionScale=1.f;
    double m_sceneMs=0,m_submitMs=0;
    bool m_poseReady=false;
    struct Texture { int width=0, height=0; std::vector<std::uint32_t> pixels; bool clampEdges=false; std::vector<std::vector<std::uint32_t>> mips; bool additive=false; std::vector<std::vector<Point3>> normalLevels; std::vector<std::uint32_t> emission; bool transparent=false; };
    struct NormalLighting {std::array<Point3,2> directions{};std::array<float,2> weights{};};
    static void attachNormal(Texture& texture,int resource,bool greenUp=true);
    static Point3 sampleNormal(const Texture& texture,float u,float v,float lod);
    bool testNormalMapping();
    Texture m_muzzleFlash;
    Texture m_water;
    Mesh m_consoleMesh{240};
    Texture m_consoleTexture;
    std::array<Texture,3> m_hazmatTextures;
    std::vector<RagPoint> m_hazmatPose;
    std::array<RagPoint,Ragdoll::Count> m_hazmatPoseJoints{};
    bool m_hazmatPoseValid=false;
    Texture m_blood;
    Mesh m_pumpMesh{140},m_compressorMesh{142},m_pipeMesh{144},m_gateMesh{146};
    Texture m_pumpTexture,m_compressorTexture,m_pipeTexture,m_gateTexture,m_pressureWall,m_pressureFloor,m_pressureMetal;
    Texture m_transferSign,m_pumpSign,m_controlSign,m_surfaceSign,m_gantrySign,m_reactorSign,m_liftSign,m_liftDispatch;
    Texture m_feedSign,m_returnSign,m_diskSign,m_authSign;
    Texture m_wall, m_floor, m_metal, m_arms;
    std::array<Mesh,6> m_clutterMeshes{Mesh{151},Mesh{153},Mesh{155},Mesh{157},Mesh{159},Mesh{161}};
    std::array<Texture,6> m_clutterTextures;
    Texture m_weaponTexture,m_enemyTexture;
    Mesh m_weaponMesh{109},m_armsMesh{111},m_enemyMesh{110};
    Mesh m_waspMesh{114},m_bruteMesh{116},m_wardenMesh{242};
    Texture m_waspTexture,m_bruteTexture,m_wingTexture,m_wardenTexture;
    Mesh m_barrelMesh{121},m_crateMesh{123};
    std::array<Mesh,12> m_facilityMeshes{Mesh{163},Mesh{164},Mesh{165},Mesh{166},Mesh{167},Mesh{168,"doorway_wide_1"},Mesh{169},Mesh{170},Mesh{171},Mesh{168,"door_wide_1_bottom"},Mesh{168,"door_wide_1_top"},Mesh{191}};
    std::unordered_map<std::string,Texture> m_facilityTextures;
    const Texture& facilityTexture(int mesh,int part)const{return m_facilityTextures.at(m_facilityMeshes[mesh].materialNames.at(part));}
    Mesh m_medkitMesh{133},m_shellsMesh{135};
    Texture m_medkitTexture,m_shellsTexture;
    Texture m_barrelTexture,m_crateTexture,m_concrete,m_bulkhead;
    Texture m_intakeSign,m_processingSign,m_containmentSign,m_exitSign;
    Texture m_hazard,m_chemicalSign,m_machineSign,m_confinedSign,m_signRust,m_panelMetal,m_routePaint,m_redPaint;
    Texture m_terminalTexture,m_cautionSign,m_serviceSign;
    Texture makeSign(const char* title,const char* subtitle,std::uint32_t accent);
    Texture makePaint(std::uint32_t color);
    static void prepareDecal(Texture& texture,bool clampEdges=true);
    void drawScene(const Game& game,bool clearDepth=true);
    void drawViewModel(const Game& game);
    void prepareViewModel(const Game& game);
    void triangle3D(MeshVertex a,MeshVertex b,MeshVertex c,const Texture& texture,float light,const NormalLighting* normalLighting=nullptr);
    Point3 cameraPoint(Point3 p,const Game& game)const;
    static Texture loadTexture(int id);
    static std::uint32_t sample(const Texture& texture,float u,float v,float lod=0);
    void clear(std::uint32_t color);
    void put(int x,int y,std::uint32_t c);
    void rect(int x,int y,int w,int h,std::uint32_t c);
    void drawHud(const Game& game);
    void drawTitle(const Game& game);
    void drawConsole(const Game& game);
    float m_frameMs=0;
    void drawSettings(const Game& game);
    void drawInventory(const Game& game);
    void text(int x,int y,const char* s,std::uint32_t c,int scale=1);
    std::uint32_t shade(std::uint32_t c,float s) const;
    int m_width, m_height;
    std::vector<std::uint32_t> m_pixels;
    std::vector<float> m_depth;
    std::vector<float> m_zbuffer;
    std::vector<std::uint32_t> m_scenePixels;
    std::vector<float> m_sceneZ;
    bool m_visibilityCulling=true;
    int m_shadowBudgetLimit=2200;
    std::array<std::unordered_map<std::uint64_t,float>,Game::ChunkCount> m_chunkLighting;
    std::array<std::unordered_map<std::uint64_t,NormalLighting>,Game::ChunkCount> m_chunkNormalLighting;
    std::array<std::vector<float>,Game::ChunkCount> m_chunkLightingDoors;
    std::array<std::vector<std::vector<size_t>>,Game::ChunkCount> m_chunkLightCells;
    std::array<size_t,Game::ChunkCount> m_chunkLightCounts{};
    float m_gripError=0;
    bool m_inspectRig=false;
    float m_inspectYaw=0,m_inspectPitch=0;
    Point3 inspectionPoint(Point3 p)const;
    void wornPanel(int x,int y,int width,int height,bool recess=false,bool materialPanel=false);
};
}

