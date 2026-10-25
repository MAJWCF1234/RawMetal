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
 // A swinging leaf opens toward +y (1) or -y (-1) from its hinge at `left`.
 float swing=1.f;
 int sign=-1;
 float maxOpen=1.f;
};
struct WorldProp {int kind;Vec2 position;float height,footprint,yaw;Vec2 halfSize;float base=0;};
// core: the fraction of the footprint that collides when solid (a tree blocks
// at its trunk, not across its canopy); 1 for ordinary furniture and plant.
struct Fixture {int model;Vec2 position;float base,width,depth,height,yaw;bool solid=false;float core=1;};
// Bare: a light with no housing (a fire, a lamp head that already has its model).
enum class WorldLightMount { Ceiling, Wall, Bare };
struct WorldLight {
 Vec2 position;float z;
 // Ceiling records retain the original source and fixture placement. For a
 // wall lamp, position/z is the backing centre and yaw points into the room.
 WorldLightMount mount=WorldLightMount::Ceiling;
 float yaw=0,intensity=1,range=12;
 // Colour temperature (K, Planck blackbody) and a green(+)/magenta(-) tint
 // for discharge tubes. 3400 K, 0 is a neutral warm incandescent.
 float kelvin=3400,tint=0;
 Point3 emitter()const{
  if(mount==WorldLightMount::Wall)return {position.x+std::cos(yaw)*.16f,position.y+std::sin(yaw)*.16f,z};
  return {position.x,position.y,z};
 }
 Point3 shadowTarget()const{auto p=emitter();if(mount==WorldLightMount::Ceiling)p.z-=.04f;return p;}
 float falloff(float squaredDistance)const{
  float radiusSquared=range*range;if(squaredDistance>=radiusSquared)return 0.f;
  float t=1.f-squaredDistance/radiusSquared;
  return intensity*3.2f*t*t/(1.f+squaredDistance*.12f);
 }
};
// Outdoor worlds carry one directional light. Direction points toward the
// sun (z up); intensity 0 means no sun. Colour is linear.
struct SunLight {Point3 direction{};float intensity=0,r=1,g=1,b=1;};
// Authored overhead services, with absolute elevations. Kept above standing clearance.
// Materials: 0 service pipe, 1 rusted metal, 2 panel steel, 3 yellow steel,
// 4 red steel, 5 pale steel, 6 dark finish. An explicit endZ
// supplies equipment/bracing geometry without automatic ceiling hangers.
struct PipeRun {Vec2 start,end;float z,radius,endZ=-999;int material=0;};
struct CreatureSpawn {CreatureKind kind;Vec2 position;float z=-999;};
struct PickupSpawn {Vec2 position;PickupKind kind;float z=-999;};
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
struct Terminal {Vec2 position;const char* title;const char* line1;const char* line2;float z=0;bool control=false;int reactorAction=0;StateId activateState=0;bool toggleState=false;float yaw=kPi;StateId requireState=0;};
struct CargoLift {
 float x1=0,y1=0,x2=0,y2=0,lower=0,upper=0,speed=1.f;
 StateId callState=0,releaseState=0,positionState=0,downState=0,arrivedState=0,descendedState=0;
};
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
    bool openUnderside=false;
    float treadThickness=.12f;
    bool sideRails=false;
};

// Shared geometry records for built-in and runtime-authored maps. Loading these
// records never changes the world's campaign identity. CustomCampaign parses
// the editor's .txt payload into the same representation.
struct AuthoredLayerData {
    std::string name;
    float elevation=0,thickness=0;
    std::array<std::string,24> rows{};
};
struct AuthoredTerminalData {
    Vec2 position{};
    std::string title,line1,line2;
    float z=0;
    bool control=false;
    int reactorAction=0;
    StateId activateState=0;
    bool toggleState=false;
    StateId requireState=0;
    float yaw=kPi;
};
struct AuthoredSign {
    Vec2 position{};
    float z=0,width=3,height=.9f,yaw=kPi;
    std::string title,subtitle;
    std::uint32_t accent=0xffca994du;
};
// A named region of a map (HUD location label). The first zone containing
// the player wins; maps without zones show the map name.
struct AuthoredZone {
    std::string name;
    float x1=0,y1=0,x2=24,y2=24,bottom=-1000,top=1000;
};
struct AuthoredNavHint {
    Vec2 position{};
    float z=0;
    int hint=0; // 0 Path, 1 Cover — matches ai::NodeHint
};
// Campaign-authored outdoor terrain (TERRAIN / TERRAIN_FLAT records). A seeded
// fractal heightfield in world space, so every stitched chunk samples one
// continuous surface. Flats level pads, roads and building sites, blending
// back into the hills over `blend` metres. Heights are 1 m voxel tops.
struct TerrainFlat {float x1=0,y1=0,x2=0,y2=0,height=2,blend=4;};
// Ground cover painted over authored terrain: one texture kind per rectangle
// (world metres), with a noisy border `edge` metres wide.
enum GroundKind:int {GroundShortGrass,GroundLeafyGrass,GroundClover,GroundWild,GroundDry,GroundLitter,GroundGravel,GroundMud,GroundKinds,GroundMeadow=GroundKinds};
struct GroundPaint {float x1=0,y1=0,x2=0,y2=0;int kind=GroundMeadow;float edge=1.5f;};
struct TerrainShape {
    std::uint32_t seed=1;
    float base=2,amplitude=3,scale=36,roughness=.45f;
    std::vector<TerrainFlat> flats;
    // Any GROUND record turns on painted cover: a noise meadow of grass, clover,
    // wild and dry ground everywhere, then the rectangles in order.
    std::vector<GroundPaint> ground;
    // HORIZON: unreachable land around the playable maps, rising `rise` metres
    // over `depth` metres and wooded every `spacing` metres with these models
    // (drawn only as Painterly Horizon LOD impostors).
    float horizonDepth=0,horizonSpacing=6,horizonRise=0;std::vector<int> horizonModels;
};
struct AuthoredMapData {
    std::string name;
    std::string skybox="industrial_night";
    std::shared_ptr<const TerrainShape> terrain;
    std::vector<ParticleEmitter> emitters;
    ChunkDefinition definition{};
    // Adjacent views can retain another hall without joining its traversal group.
    std::vector<int> visibleResidencyGroups;
    bool openNorth=false,openSouth=false,openWest=false,openEast=false;
    std::vector<AuthoredLayerData> layers;
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
    std::vector<AuthoredTerminalData> terminals;
    std::vector<AuthoredSign> signs;
    CargoLift cargoLift;
    std::vector<TimedSequence> timedSequences;
    std::vector<ActorTrack> actorTracks;
    std::vector<SequenceCue> sequenceCues;
    std::vector<ScriptEvent> scriptEvents;
    std::vector<ObjectiveText> objectiveTexts;
    std::vector<AuthoredNavHint> navHints;
    std::vector<AuthoredZone> zones;
};
struct CustomCampaign {
    std::string name;
    std::string sourceFile;
    std::uint64_t key=0;
    int startMap=0;
    std::vector<std::shared_ptr<const AuthoredMapData>> maps;
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
    World(int level,std::shared_ptr<const AuthoredMapData> customMap);
    const std::vector<MapLayer>& layers()const{return m_layers;}
    int level()const{return m_level;}
    WorldId worldId()const{return m_worldId;}
    bool campaign()const{return m_worldId==WorldId::Campaign;}
    bool custom()const{return m_worldId==WorldId::Custom;}
    // A legacy hard-coded campaign chunk. Authored maps (including rebuilt
    // campaign slots) carry their own content, so legacy special cases skip them.
    bool campaignChunk(int index)const{return campaign()&&m_level==index&&!m_mapData;}
    bool authored()const{return m_mapData!=nullptr;}
    const ChunkDefinition& definition()const{return m_mapData?m_mapData->definition:chunkDefinition(m_worldId,m_level);}
    std::span<const int> visibleResidencyGroups()const{return m_mapData?std::span<const int>(m_mapData->visibleResidencyGroups):std::span<const int>{};}
    bool outdoors()const{return definition().environment==Environment::Outdoor;}
    bool hasLift()const{return definition().lift;}
    // Authored location name at a point (HUD), or nullptr when no zone covers it.
    const char* zoneName(Vec2 p,float z)const{
     if(!m_mapData)return nullptr;
     for(const auto& zone:m_mapData->zones)if(p.x>=zone.x1&&p.x<zone.x2&&p.y>=zone.y1&&p.y<zone.y2&&z>=zone.bottom&&z<zone.top)return zone.name.c_str();
     return nullptr;
    }
    bool horrorMode()const{return m_worldId==WorldId::Ashfall;}
    bool coast()const{return horrorMode()&&m_level>=12;}
    // The open-zone sky: procedural broken overcast, a warm sun with shafts
    // and low steam over the ground, on the same authored terrain.
    bool zoneSky()const{return m_mapData&&m_mapData->skybox=="zone_overcast";}
    // Painted ground cover at a world position, or -1 for the default terrain look.
    int groundKind(float worldX,float worldY)const;
    // The authored terrain shape, or null; and its continuation past the zone
    // edge (`outside` metres beyond it), rising toward the horizon hills.
    const TerrainShape* terrainShape()const{return m_exclusionTerrain&&m_mapData?m_mapData->terrain.get():nullptr;}
    float outskirtHeight(float worldX,float worldY,float outside)const;
    const char* skyboxId()const{return m_mapData?m_mapData->skybox.c_str():horrorMode()?"brutal_wasteland":"industrial_night";}
    const char* customMapName()const{return m_mapData?m_mapData->name.c_str():"";}
    bool openNorthBoundary()const{return m_openNorthBoundary;}
    bool openSouthBoundary()const{return m_openSouthBoundary;}
    bool openWestBoundary()const{return m_openWestBoundary;}
    bool openEastBoundary()const{return m_openEastBoundary;}
    // Where the way on lies: the transfer door, else the open seam onward.
    Vec2 exitPoint()const{
     for(const auto& door:m_doors)if(door.transfer)return {(door.left+door.right)*.5f,std::min(door.y+1.f,23.5f)};
     if(m_openSouthBoundary)return {12.f,23.f};if(m_openEastBoundary)return {23.f,12.f};
     if(m_openNorthBoundary)return {12.f,1.f};if(m_openWestBoundary)return {1.f,12.f};
     return {21.5f,22.5f};}
    const std::vector<WorldProp>& props()const{return m_props;}
    const std::vector<Fixture>& fixtures()const{return m_fixtures;}
    const std::vector<AuthoredSign>& signs()const{static const std::vector<AuthoredSign> empty;return m_mapData?m_mapData->signs:empty;}
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
    // How wet floors are (0 dry .. 1 flooded): damp sheen everywhere, puddles
    // on level ground. Carpet never puddles; the Foundry runs hot and dry.
    float wetness()const{
     if(definition().floorMaterial==FloorMaterial::OfficeCarpet)return 0.f;
     if(zoneSky())return .38f;
     if(outdoors())return .55f;
     if(campaign()&&level()==0)return .12f;
     if(campaign()&&level()==1)return .65f;
     return .3f;
    }
    // Low overcast sun for the Ashfall exterior; indoor worlds have none.
    SunLight sun()const{if(!outdoors())return {};SunLight s;Point3 d{.50f,.30f,.55f};
     if(zoneSky()){d={-.62f,.48f,.42f};float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);s.direction={d.x/l,d.y/l,d.z/l};s.intensity=2.6f;s.r=1.f;s.g=.86f;s.b=.66f;return s;}float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);s.direction={d.x/l,d.y/l,d.z/l};s.intensity=2.1f;s.r=1.f;s.g=.92f;s.b=.80f;return s;}
    std::vector<Span> spansAt(int x,int y)const;
    float supportBelow(float x,float y,float feet)const;
    float clearanceAbove(float x,float y,float feet)const;
    bool fits(float x,float y,float feet,float height,bool dynamic=true,bool shelfCavities=false)const;
    const CargoLift& cargoLift()const{return m_cargoLift;}
    const std::vector<TimedSequence>& timedSequences()const{static const std::vector<TimedSequence> empty;return m_mapData?m_mapData->timedSequences:empty;}
    const std::vector<ActorTrack>& actorTracks()const{static const std::vector<ActorTrack> empty;return m_mapData?m_mapData->actorTracks:empty;}
    const ActorPose& actorPose(size_t index)const{return m_actorPoses.at(index);}
    void setActorPose(size_t index,ActorPose pose){m_actorPoses.at(index)=pose;}
    const std::vector<SequenceCue>& sequenceCues()const{static const std::vector<SequenceCue> empty;return m_mapData?m_mapData->sequenceCues:empty;}
    float cargoLiftHeight()const{return m_cargoLiftHeight;}
    void setCargoLiftHeight(float z){m_cargoLiftHeight=z;}
    bool insideCargoLift(float x,float y)const{return m_cargoLift.x2>m_cargoLift.x1&&x>=m_cargoLift.x1&&x<m_cargoLift.x2&&y>=m_cargoLift.y1&&y<m_cargoLift.y2;}
    bool railBlocksHull(float x,float y,float radius,float feet,float height)const;
    bool structureSegmentClear(Vec2 start,float feet,Vec2 goal,float height,bool indexed=true)const;
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
    static constexpr float ReactorDiskZ=-5.215f; // On the upper workshop bench.
    bool takeReactorDisk();
    void useReactorTerminal(int action);
    // A stitched reactor chunk mirrors the lift chunk's puzzle state, so its
    // terminals read and its interlocks open from one source of truth.
    void syncReactor(const World& source);

    char tile(int x, int y) const;
    bool destroyTile(int x,int y);
    bool solid(float x, float y) const;
    bool isExit(float x, float y) const;
    // Legacy grating rule for the hard-coded chunks; authored maps lay their own floors.
    bool metalFloor(int x,int)const{return !outdoors()&&!m_mapData&&x>=7&&x<=16;}
    std::vector<Vec2> machines()const;
    float floorHeight(float x,float y)const;
    float supportHeight(float x,float y,bool dynamic=true,bool shelfCavities=false)const;
    float ceilingHeight(float x,float y)const;
    float clearanceHeight(float x,float y)const;
    bool rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors=true,bool dynamic=true,bool shelfCavities=false)const;
    // Static optical visibility: exact solid intersections, separate from player hull clearance.
    bool lightRayClear(Vec2 a,float az,Vec2 b,float bz)const;
    // Nearest physical shot obstruction on a finite 3D segment. Actor hulls
    // are resolved by Game; this includes static and moving world geometry.
    float shotFraction(Point3 from,Point3 to,Point3&normal)const;
    static bool testCollisionCandidates();
    bool doorBlocks(float x,float y,float feet,float height)const;
    bool navigable(int x,int y,int nx,int ny,float height=1.f)const;
    void updateDoors(float dt,bool includeTransfers=true);
    bool openDoor(int index);
    bool toggleDoor(int index);
    void restoreDoors(const std::vector<Door>& doors){m_doors=doors;}
    void setDoor(int index,float open,bool opening){m_doors.at(index).open=open;m_doors.at(index).opening=opening;}
    void unloadGeometry(){m_collisionCells={};m_collisionAll={};std::vector<LightSolid>{}.swap(m_lightSolids);std::vector<LightNode>{}.swap(m_lightNodes);std::vector<MapLayer>{}.swap(m_layers);std::vector<Structure>{}.swap(m_structures);std::vector<TerrainTriangle>{}.swap(m_terrain);std::vector<std::int8_t>{}.swap(m_terrainDensity);std::vector<std::uint8_t>{}.swap(m_terrainTops);std::vector<std::uint8_t>{}.swap(m_terrainMaterial);std::vector<std::vector<uint16_t>>{}.swap(m_structureCells);std::vector<WorldProp>{}.swap(m_props);std::vector<Fixture>{}.swap(m_fixtures);std::vector<PipeRun>{}.swap(m_pipes);std::vector<WorldLight>{}.swap(m_lights);std::vector<Hazard>{}.swap(m_hazards);std::vector<Compactor>{}.swap(m_compactors);std::vector<Terminal>{}.swap(m_terminals);}
    int nearbyDoor(Vec2 position,Vec2 forward,float feet=0)const;
    const std::vector<Door>& doors()const{return m_doors;}
    const std::vector<Terminal>& terminals()const{return m_terminals;}

private:
    CargoLift m_cargoLift{};
    float m_cargoLiftHeight=0;
    friend class Game; // Save codec persists dynamic state, never geometry or pointers.
    std::vector<MapLayer> m_layers;
    std::vector<std::pair<int,int>> m_destroyedTiles;
    float m_internalWallHeight=0;
    int m_level=0;
    WorldId m_worldId=WorldId::Campaign;
    std::shared_ptr<const AuthoredMapData> m_mapData;
    std::vector<ActorPose> m_actorPoses;
    void loadAuthoredMap(std::shared_ptr<const AuthoredMapData> map);
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
    void buildLights(int startX=2,int startY=1);
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
    float supportBelowUncached(float x,float y,float feet)const;
    bool fitsUncached(float x,float y,float feet,float height,bool dynamic,bool shelfCavities)const;
    // Highest positive sample per XY column; conservative empty-air bounds.
    std::vector<std::uint8_t> m_terrainTops;
    uint64_t m_terrainRevision=0;
    std::vector<std::uint8_t> m_terrainMaterial;
    // Custom outdoor wasteland packs (Exclusion Zone) sample a local heightfield
    // instead of the stitched Ashfall world voxels.
    bool m_exclusionTerrain=false;
    void buildTerrain();
    void buildTerrainBounds();
    void settleExclusionFacilityOnTerrain();
    std::size_t terrainSampleIndex(int sx,int sy,int sz)const;
    float terrainDensity(float x,float y,float z)const;
    float terrainSurfaceBelow(float x,float y,float feet)const;
    float terrainSurfaceAbove(float x,float y,float feet)const;
    struct LightSolid {Point3 center,half;float cosine=1,sine=0;};
    struct LightNode {Point3 minimum,maximum;int begin=0,end=0,left=-1,right=-1;};
    mutable std::vector<LightSolid> m_lightSolids;
    mutable std::vector<LightNode> m_lightNodes;
    void buildLightOcclusion()const;
    mutable std::array<std::vector<std::vector<size_t>>,3> m_collisionCells;
    mutable std::array<std::vector<size_t>,3> m_collisionAll;
    bool m_collisionCandidatesEnabled=true;
    void buildCollisionCandidates()const;
    std::span<const size_t> collisionCandidates(int kind,float x,float y)const;
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
