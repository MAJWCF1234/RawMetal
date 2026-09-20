#pragma once

#include <array>
#include <string_view>
#include <span>
#include <vector>
#include <cstdint>
#include "../core/Math.h"

namespace retro {
struct Door {float left=0,right=0,y=0,open=0;bool opening=false,transfer=false,entry=false;float z=0;};
struct WorldProp {int kind;Vec2 position;float height,footprint,yaw;Vec2 halfSize;float base=0;};
struct Fixture {int model;Vec2 position;float base,width,depth,height,yaw;bool solid=false;};
struct WorldLight {Vec2 position;float z;};
struct Hazard {
 enum class Kind {Electricity,Steam,Crusher,Toxic,Fire,FallingDebris,Pressure,Anomaly};
 Kind kind=Kind::Electricity;float x1=0,y1=0,x2=0,y2=0,bottom=-100,top=100,damagePerSecond=0;
 std::uint32_t enabledFlag=0;int enabledValue=1;bool invertFlag=false;
};
struct Terminal {Vec2 position;const char* title;const char* line1;const char* line2;float z=0;bool control=false;int reactorAction=0;};
struct Structure {float x1,y1,x2,y2,bottom,top;bool rail=false;int material=0;};
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

class World {
public:
    static constexpr int Width = 24;
    static constexpr int Height = 24;

    explicit World(int level=0);
    const std::vector<MapLayer>& layers()const{return m_layers;}
    int level()const{return m_level;}
    bool openNorthBoundary()const{return m_openNorthBoundary;}
    bool openSouthBoundary()const{return m_openSouthBoundary;}
    Vec2 exitPoint()const{return m_level==5?Vec2{19.5f,22.5f}:m_level==4?Vec2{12.f,22.5f}:Vec2{21.5f,22.5f};}
    const std::vector<WorldProp>& props()const{return m_props;}
    const std::vector<Fixture>& fixtures()const{return m_fixtures;}
    bool wallSpaceFree(Vec2 center,Vec2 along,float width,float bottom,float top)const;
    const std::vector<WorldLight>& lights()const{return m_lights;}
    const std::vector<Hazard>& hazards()const{return m_hazards;}
    const std::vector<Structure>& structures()const{return m_structures;}
    std::vector<Span> spansAt(int x,int y)const;
    float supportBelow(float x,float y,float feet)const;
    float clearanceAbove(float x,float y,float feet)const;
    bool fits(float x,float y,float feet,float height,bool dynamic=true)const;
    bool railBlocksHull(float x,float y,float radius,float feet,float height)const;
    float wallHeight(int x,int y)const;
    bool controlReleased()const{return m_controlReleased;}
    void releaseControl(){m_controlReleased=true;}
    enum class LiftPhase { Ready, Ascending, Jammed, Falling, Caught, Crashed };
    static constexpr float LiftRideComplete=48.f;
    LiftPhase liftPhase()const{return m_liftPhase;}
    float liftHeight()const{return m_liftHeight;}
    float liftPhaseTime()const{return m_liftTimer;}
    bool insideLift(float x,float y)const{return m_level==3&&x>=10&&x<14&&y>=10&&y<14;}
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
    bool solid(float x, float y) const;
    bool isExit(float x, float y) const;
    bool metalFloor(int x,int y)const{return m_level>=4||y>=8||x>=12;}
    std::vector<Vec2> machines()const;
    float floorHeight(float x,float y)const;
    float supportHeight(float x,float y,bool dynamic=true)const;
    float ceilingHeight(float x,float y)const;
    float clearanceHeight(float x,float y)const;
    bool rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors=true,bool dynamic=true)const;
    bool doorBlocks(float x,float y,float feet,float height)const;
    bool navigable(int x,int y,int nx,int ny,float height=1.f)const;
    void updateDoors(float dt);
    bool openDoor(int index);
    bool toggleDoor(int index);
    void restoreDoors(const std::vector<Door>& doors){m_doors=doors;}
    void setDoor(int index,float open,bool opening){m_doors.at(index).open=open;m_doors.at(index).opening=opening;}
    void unloadGeometry(){std::vector<MapLayer>{}.swap(m_layers);std::vector<Structure>{}.swap(m_structures);std::vector<std::vector<uint16_t>>{}.swap(m_structureCells);std::vector<WorldProp>{}.swap(m_props);std::vector<Fixture>{}.swap(m_fixtures);std::vector<WorldLight>{}.swap(m_lights);std::vector<Hazard>{}.swap(m_hazards);std::vector<Terminal>{}.swap(m_terminals);}
    int nearbyDoor(Vec2 position,Vec2 forward,float feet=0)const;
    const std::vector<Door>& doors()const{return m_doors;}
    const std::vector<Terminal>& terminals()const{return m_terminals;}

private:
    friend class Game; // Save codec persists dynamic state, never geometry or pointers.
    std::vector<MapLayer> m_layers;
    float m_internalWallHeight=0;
    int m_level=0;
    bool m_openNorthBoundary=false,m_openSouthBoundary=false;
    std::vector<WorldProp> m_props;
    std::vector<Fixture> m_fixtures;
    std::vector<WorldLight> m_lights;
    std::vector<Hazard> m_hazards;
    void buildLights();
    std::vector<Door> m_doors;
    std::vector<Terminal> m_terminals;
    std::vector<Structure> m_structures;
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


