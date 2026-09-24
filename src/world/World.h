#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <cstdint>
#include "../core/Math.h"
#include "WorldDefinition.h"
#include "ScriptDefinition.h"

namespace retro {
struct Door {
 float left=0,right=0,y=0,open=0;bool opening=false,transfer=false,entry=false;float z=0;
 // Authored requirements are independent of whether a door connects chunks.
 bool requireEnemiesClear=false,requireControl=false;
 StateId requireState=0;int requireValue=1;
 bool swinging=false;
 int sign=-1;
};
struct WorldProp {int kind;Vec2 position;float height,footprint,yaw;Vec2 halfSize;float base=0;};
struct Fixture {int model;Vec2 position;float base,width,depth,height,yaw;bool solid=false;};
struct WorldLight {Vec2 position;float z;};
// Authored overhead services, with absolute elevations. Kept above standing clearance.
struct PipeRun {Vec2 start,end;float z,radius,endZ=-999;};
struct CreatureSpawn {CreatureKind kind;Vec2 position;float z=-999;};
struct PickupSpawn {Vec2 position;PickupKind kind;};
struct ClutterSpawn {int kind;Vec2 position;float z=-999,yaw=0;};
// One authored liquid basin controls the bed, physics surface and drawn mesh.
struct WaterVolume {float x1,y1,x2,y2,bed,surface;};
// Content specifies emitters; rendering does not decide their positions from chunk IDs.
struct ParticleEmitter {Vec2 position;float z;Vec2 drift;float rise=1.05f,rate=.55f,radius=.07f,growth=.2f;int count=7;};
struct Hazard {
 enum class Kind {Electricity,Steam,Crusher,Toxic,Fire,FallingDebris,Pressure,Anomaly};
 Kind kind=Kind::Electricity;float x1=0,y1=0,x2=0,y2=0,bottom=-100,top=100,damagePerSecond=0;
 std::uint32_t enabledFlag=0;int enabledValue=1;bool invertFlag=false;
 float period=0,onTime=0,phase=0;
};
struct Terminal {Vec2 position;const char* title;const char* line1;const char* line2;float z=0;bool control=false;int reactorAction=0;StateId activateState=0;bool toggleState=false;};
struct Compactor {float x1,y1,x2,y2,bed,raised,period=9;StateId stopState=0;};
struct Structure {float x1,y1,x2,y2,bottom,top;bool rail=false;int material=0;};
// Terrain source points are one-metre solid/air voxels, Minecraft-style.
// Surface Nets removes the cube faces and emits the engine-native low-poly skin;
// collision queries the same voxel density field.
struct TerrainVertex {float x=0,y=0,z=0,u=0,v=0;};
// Shared terrain material IDs, independent of campaign/custom world identity.
enum TerrainMaterial : std::uint8_t { TerrainSoil, TerrainRock };
struct TerrainTriangle {TerrainVertex a,b,c;std::uint8_t material=TerrainSoil;};
struct Span {float floor,ceiling;uint16_t flags=0;};
using MapRows = std::array<std::string_view,24>;
struct MapLayer {
    std::string_view name;
    float elevation,thickness;
    MapRows rows;
};
struct Staircase {
    float x1,y1,x2,y2,bottom,top;
    int steps;
    bool alongY,ascending;
};

// Runtime-authored campaign data. The browser editor writes these records into
// the same .txt payload used by InstallMap.cmd, so a player can drop a complete
// campaign into /custom maps without compiling or editing WorldDefinition.h.
struct CustomLayerData {
    std::string name;
    float elevation=0,thickness=0;
    std::array<std::string,24> rows{};
};
struct CustomTerminalData {
    Vec2 position{};
    std::string title,line1,line2;
    float z=0;
    bool control=false;
    int reactorAction=0;
    StateId activateState=0;
    bool toggleState=false;
};
struct CustomMapData {
    std::string name;
    std::string skybox="industrial_night";
    ChunkDefinition definition{};
    bool openNorth=false,openSouth=false,openWest=false,openEast=false;
    std::vector<CustomLayerData> layers;
    std::vector<Door> doors;
    std::vector<WorldProp> props;
    std::vector<Fixture> fixtures;
    std::vector<PipeRun> pipes;
    std::vector<WorldLight> lights;
    std::vector<CreatureSpawn> creatureSpawns;
    std::vector<PickupSpawn> pickupSpawns;
    std::vector<ClutterSpawn> clutterSpawns;
    std::vector<WaterVolume> waterVolumes;
    std::vector<Hazard> hazards;
    std::vector<Compactor> compactors;
    std::vector<Structure> structures;
    std::vector<Staircase> stairs;
    std::vector<CustomTerminalData> terminals;
};
struct CustomCampaign {
    std::string name;
    std::string sourceFile;
    std::uint64_t key=0;
    int startMap=0;
    std::vector<std::shared_ptr<const CustomMapData>> maps;
};

class World {
public:
    static constexpr int Width = 24;
    static constexpr int Height = 24;
    // The toy Ashfall world proves the same volumetric terrain representation
    // intended for the later Dark Below cavern chapter.
    static constexpr int TerrainMinZ = -12;
    static constexpr int TerrainMaxZ = 20;

    explicit World(int level=0,WorldId id=WorldId::Campaign);
    World(int level,std::shared_ptr<const CustomMapData> customMap);
    const std::vector<MapLayer>& layers()const{return m_layers;}
    int level()const{return m_level;}
    WorldId worldId()const{return m_worldId;}
    bool campaign()const{return m_worldId==WorldId::Campaign;}
    bool custom()const{return m_worldId==WorldId::Custom;}
    bool campaignChunk(int index)const{return campaign()&&m_level==index;}
    const ChunkDefinition& definition()const{return m_customMap?m_customMap->definition:chunkDefinition(m_worldId,m_level);}
    bool outdoors()const{return definition().environment==Environment::Outdoor;}
    bool hasLift()const{return definition().lift;}
    bool horrorMode()const{return m_worldId==WorldId::Ashfall;}
    const char* skyboxId()const{return m_customMap?m_customMap->skybox.c_str():horrorMode()?"brutal_wasteland":"industrial_night";}
    const char* customMapName()const{return m_customMap?m_customMap->name.c_str():"";}
    bool openNorthBoundary()const{return m_openNorthBoundary;}
    bool openSouthBoundary()const{return m_openSouthBoundary;}
    bool openWestBoundary()const{return m_openWestBoundary;}
    bool openEastBoundary()const{return m_openEastBoundary;}
    Vec2 exitPoint()const{return m_level==5?Vec2{19.5f,22.5f}:m_level==4?Vec2{12.f,22.5f}:Vec2{21.5f,22.5f};}
    const std::vector<WorldProp>& props()const{return m_props;}
    const std::vector<Fixture>& fixtures()const{return m_fixtures;}
    const std::vector<PipeRun>& pipes()const{return m_pipes;}
    bool wallSpaceFree(Vec2 center,Vec2 along,float width,float bottom,float top)const;
    const std::vector<WorldLight>& lights()const{return m_lights;}
    const std::vector<CreatureSpawn>& creatureSpawns()const{return m_creatureSpawns;}
    const std::vector<PickupSpawn>& pickupSpawns()const{return m_pickupSpawns;}
    const std::vector<ClutterSpawn>& clutterSpawns()const{return m_clutterSpawns;}
    const std::vector<WaterVolume>& waterVolumes()const{return m_waterVolumes;}
    const std::vector<ParticleEmitter>& particleEmitters()const{return m_particleEmitters;}
    const std::vector<ScriptEvent>& scriptEvents()const{return m_scriptEvents;}
    const std::vector<Hazard>& hazards()const{return m_hazards;}
    const std::vector<Compactor>& compactors()const{return m_compactors;}
    const std::vector<Structure>& structures()const{return m_structures;}
    const std::vector<TerrainTriangle>& terrain()const{return m_terrain;}
    bool hasTerrain()const{return !m_terrain.empty();}
    std::vector<Span> spansAt(int x,int y)const;
    float supportBelow(float x,float y,float feet)const;
    float clearanceAbove(float x,float y,float feet)const;
    bool fits(float x,float y,float feet,float height,bool dynamic=true,bool shelfCavities=false)const;
    bool railBlocksHull(float x,float y,float radius,float feet,float height)const;
    float wallHeight(int x,int y)const;
    bool controlReleased()const{return m_controlReleased;}
    void releaseControl(){m_controlReleased=true;}
    enum class LiftPhase { Ready, Ascending, Jammed, Falling, Caught, Crashed };
    static constexpr float LiftRideComplete=48.f;
    LiftPhase liftPhase()const{return m_liftPhase;}
    float liftHeight()const{return m_liftHeight;}
    float waterSurface(float x,float y)const;
    float liftPhaseTime()const{return m_liftTimer;}
    bool insideLift(float x,float y)const{return hasLift()&&x>=10&&x<14&&y>=10&&y<14;}
    bool liftMoving()const{return m_liftPhase!=LiftPhase::Ready&&m_liftPhase!=LiftPhase::Crashed;}
    bool startLift();
    void updateLift(float dt);
    void restoreLift(const World& saved);
    const char* liftStatus()const;
    float liftLampPower()const;
    float liftMotorGain()const;
    enum class ReactorStage { NoDisk, DiskHeld, DiskLoaded, FeedPrimed, ReturnPrimed, Released };
    ReactorStage reactorStage()const{return m_reactorStage;}
    static Vec2 reactorDiskPosition(){return {6.f,21.f};}
    static constexpr float ReactorDiskZ=-5.39f;
    bool takeReactorDisk();
    void useReactorTerminal(int action);

    char tile(int x, int y) const;
    bool destroyTile(int x,int y);
    bool solid(float x, float y) const;
    bool isExit(float x, float y) const;
    bool metalFloor(int x,int y)const{return !outdoors()&&(m_level>=4?(x>=7&&x<=16):y>=8||x>=12);}
    std::vector<Vec2> machines()const;
    float floorHeight(float x,float y)const;
    float supportHeight(float x,float y,bool dynamic=true,bool shelfCavities=false)const;
    float ceilingHeight(float x,float y)const;
    float clearanceHeight(float x,float y)const;
    bool rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors=true,bool dynamic=true,bool shelfCavities=false)const;
    bool doorBlocks(float x,float y,float feet,float height)const;
    bool navigable(int x,int y,int nx,int ny,float height=1.f)const;
    void updateDoors(float dt);
    bool openDoor(int index);
    bool toggleDoor(int index);
    void restoreDoors(const std::vector<Door>& doors){m_doors=doors;}
    void setDoor(int index,float open,bool opening){m_doors.at(index).open=open;m_doors.at(index).opening=opening;}
    void unloadGeometry(){std::vector<MapLayer>{}.swap(m_layers);std::vector<Structure>{}.swap(m_structures);std::vector<TerrainTriangle>{}.swap(m_terrain);std::vector<std::int8_t>{}.swap(m_terrainDensity);std::vector<std::uint8_t>{}.swap(m_terrainMaterial);std::vector<std::vector<uint16_t>>{}.swap(m_structureCells);std::vector<WorldProp>{}.swap(m_props);std::vector<Fixture>{}.swap(m_fixtures);std::vector<PipeRun>{}.swap(m_pipes);std::vector<WorldLight>{}.swap(m_lights);std::vector<Hazard>{}.swap(m_hazards);std::vector<Compactor>{}.swap(m_compactors);std::vector<Terminal>{}.swap(m_terminals);}
    int nearbyDoor(Vec2 position,Vec2 forward,float feet=0)const;
    const std::vector<Door>& doors()const{return m_doors;}
    const std::vector<Terminal>& terminals()const{return m_terminals;}

private:
    friend class Game; // Save codec persists dynamic state, never geometry or pointers.
    std::vector<MapLayer> m_layers;
    std::vector<std::pair<int,int>> m_destroyedTiles;
    float m_internalWallHeight=0;
    int m_level=0;
    WorldId m_worldId=WorldId::Campaign;
    std::shared_ptr<const CustomMapData> m_customMap;
    bool m_openNorthBoundary=false,m_openSouthBoundary=false,m_openWestBoundary=false,m_openEastBoundary=false;
    std::vector<WorldProp> m_props;
    std::vector<Fixture> m_fixtures;
    std::vector<Compactor> m_compactors;
    std::vector<PipeRun> m_pipes;
    std::vector<WorldLight> m_lights;
    std::vector<CreatureSpawn> m_creatureSpawns;
    std::vector<PickupSpawn> m_pickupSpawns;
    std::vector<ClutterSpawn> m_clutterSpawns;
    std::vector<WaterVolume> m_waterVolumes;
    void buildPopulation();
    std::vector<ParticleEmitter> m_particleEmitters;
    std::vector<ScriptEvent> m_scriptEvents;
    std::vector<Hazard> m_hazards;
    void buildLights();
    std::vector<Door> m_doors;
    std::vector<Terminal> m_terminals;
    std::vector<Structure> m_structures;
    std::vector<TerrainTriangle> m_terrain;
    // One ghost sample on each horizontal side lets Surface Nets build seam
    // faces from neighbouring voxel cells without storing duplicate world maps.
    static constexpr int TerrainBorder=1;
    static constexpr int TerrainSamplesX=Width+3;
    static constexpr int TerrainSamplesY=Height+3;
    static constexpr int TerrainSamplesZ=TerrainMaxZ-TerrainMinZ+3;
    std::vector<std::int8_t> m_terrainDensity;
    std::vector<std::uint8_t> m_terrainMaterial;
    void buildTerrain();
    std::size_t terrainSampleIndex(int sx,int sy,int sz)const;
    float terrainDensity(float x,float y,float z)const;
    float terrainSurfaceBelow(float x,float y,float feet)const;
    float terrainSurfaceAbove(float x,float y,float feet)const;
    std::vector<std::vector<uint16_t>> m_structureCells;
    const std::vector<uint16_t>& structureIndices(float x,float y)const{static const std::vector<uint16_t> empty;int ix=int(std::floor(x)),iy=int(std::floor(y));return m_structureCells.empty()||ix<0||iy<0||ix>=Width||iy>=Height?empty:m_structureCells[iy*Width+ix];}
    bool m_controlReleased=false;
    LiftPhase m_liftPhase=LiftPhase::Ready;
    float m_liftHeight=0,m_liftTimer=0,m_liftVelocity=0;
    bool m_liftCaught=false,m_reactorFault=false;
    ReactorStage m_reactorStage=ReactorStage::NoDisk;
    void refreshReactorTerminals();
    void buildLayers(std::span<const Staircase> stairs);
};

}


