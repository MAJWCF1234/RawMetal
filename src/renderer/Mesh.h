#pragma once
#include <vector>
#include <string>
#include <array>
#include "../core/Math.h"
#include "../ThirdParty/ufbx/ufbx.h"
namespace retro {
struct MeshVertex {Point3 p;float u=0,v=0,light=1;};
struct MeshTriangle {MeshVertex v[3];int part=0;};
// RML1 stores actor-space, metre-sized joints beside each baked vertex frame.
// Palm directions are vectors, rather than positions.
struct CreatureLayerPose {
 Point3 pelvis{},spine{};
 std::array<std::array<Point3,4>,2> arms{}; // shoulder, elbow, wrist, palm direction
};
static_assert(sizeof(CreatureLayerPose)==120);
class Mesh {
public:
 explicit Mesh(int resource,const char* nodeFilter="",int animationResource=0);
 ~Mesh();
 Mesh(const Mesh&)=delete;
 Mesh& operator=(const Mesh&)=delete;
 void pose(float phase,float recoil);
    bool poseAction(const char* action,float phase);
 static bool testAttachedSkinning();
 static bool testVrHands();
    void poseCreature(int clip,float phase);
 // Tank split: legs from lowerClip, torso/arms from upperClip (blend at waist),
 // then restricted mid/head aim yaw+pitch toward the look target.
 void poseCreatureTank(int lowerClip,float lowerPhase,int upperClip,float upperPhase,float aimYaw,float aimPitch);
 bool hasCreatureLayerPose()const{return !m_creatureLayerFrames.empty();}
 // RMD1 is emitted by the direct character bake. Legacy banks have no
 // authored playback duration and continue to use their existing phase clock.
 bool hasDirectCreatureMotion()const{return !m_creatureDurations.empty();}
 float creatureClipDuration(int clip)const;
 unsigned creatureClipCount()const{return m_creatureClips;}
 const CreatureLayerPose& creatureLayerPose()const{return m_creatureLayerPose;}
 CreatureLayerPose creatureLayerPoseAt(int clip,float phase)const{return sampleCreatureLayerPose(clip,phase);}
 static bool testCleanupTankAim();
 void poseAttached(Point3 right,Point3 left,float elbowSwing,float pitch,float yaw,float phase,float recoil);
 Point3 bonePosition(const char* name)const;
 std::vector<MeshTriangle> trackedHand(bool left,const std::array<float,5>& curls);
 Point3 poseAnchor(int index)const{return m_poseAnchors.at(size_t(index));}
 // Imported, normalized topology remains available after animation poses.
 const std::vector<MeshTriangle>& bindTriangles()const;
 const std::vector<uint16_t>& creatureVertexIndices()const{return m_creatureIndices;}
 std::vector<MeshTriangle> triangles;
 std::vector<std::vector<MeshVertex>> openRings;
 Point3 minimum,maximum;
 size_t bones=0;
 std::string description;
 std::vector<std::string> materialNames;
private:
 std::vector<unsigned char> m_creatureFrames;
 std::vector<unsigned char> m_anchorFrames;
 std::array<Point3,2> m_poseAnchors{};
 std::vector<CreatureLayerPose> m_creatureLayerFrames;
 std::vector<float> m_creatureDurations;
 CreatureLayerPose m_creatureLayerPose{};
 std::vector<uint16_t> m_creatureIndices;
 std::vector<Point3> m_creatureBindUnique;
 std::vector<float> m_creatureUpperMask,m_creatureHeadMask,m_creatureArmMask;
 std::vector<unsigned char> m_creatureHandSide;
 uint32_t m_creatureSamples=0,m_creatureVertices=0,m_creatureClips=0;
 std::vector<uint32_t> m_creatureSampleCounts,m_creatureSampleOffsets;
 size_t m_creatureFrameOffset=0;
 mutable std::vector<Point3> m_creaturePoseScratch;
 mutable std::vector<Point3> m_creaturePoseScratchB;
 void sampleCreatureClip(int clip,float phase,std::vector<Point3>& out)const;
 CreatureLayerPose sampleCreatureLayerPose(int clip,float phase)const;
 void sampleCreatureAnchors(int clip,float phase);
 void publishCreatureVertices();
 struct CachedTriangle {uint32_t node;std::array<uint32_t,3> corners;MeshTriangle prototype;};
 struct CachedRing {uint32_t node;std::vector<uint32_t> corners;};
 std::vector<CachedTriangle> m_cachedTriangles;
 mutable std::vector<MeshTriangle> m_bindTriangles;
 std::vector<CachedRing> m_cachedRings;
 bool m_topologyReady=false;
 bool m_materialParts=false;
 std::string m_nodeFilter;
 // The arms use the library's position skinning formula without generating
 // normals/topology that this renderer never consumes.
 bool m_positionOnlySkinning=false;
 std::vector<std::vector<ufbx_vec3>> m_skinPositionScratch;
 std::vector<unsigned char> m_skinPositionReady;
 void extract(ufbx_scene* scene,bool positionsOnly=false);
 void grip(Point3 right={-.025f,1.55f,.223f},Point3 left={-.025f,1.60f,.49f},float swing=0,float pitch=0,float yaw=0,bool skin=true);
 ufbx_scene* m_scene=nullptr;
 ufbx_scene* m_bindScene=nullptr;
 std::vector<ufbx_transform_override> m_fingerGrip;
};
}
