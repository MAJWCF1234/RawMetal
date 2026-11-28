#pragma once
#include <vector>
#include <string>
#include <array>
#include <initializer_list>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>
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
 // anchorResource: a baked civilian's .anchors sidecar (RightHand and
 // RightForeArm per sample). The Worker (277) finds its own 282.
 explicit Mesh(int resource,const char* nodeFilter="",int animationResource=0,int anchorResource=0);
 // What a mesh is made from: its resources, as the constructor took them.
 struct Source {int resource=0;std::string nodeFilter;int animation=0,anchor=0;};
 const Source& source()const{return m_source;}
 // While a Deferred lives on a thread, a Mesh made there only records its Source and
 // decodes nothing: a LazyMesh takes it and decodes it the first time it is asked for.
 // Braced lists run in order, so `{Mesh::Deferred{},Mesh{163},Mesh{164}}` defers both.
 struct Deferred {Deferred(){deferring()=true;}~Deferred(){deferring()=false;}Deferred(const Deferred&)=delete;Deferred& operator=(const Deferred&)=delete;};
 static bool& deferring(){thread_local bool on=false;return on;}
 ~Mesh();
 Mesh(const Mesh&)=delete;
 Mesh& operator=(const Mesh&)=delete;
 // A hold's hand frames and finger set for poseAttached: each hand's
 // wrist-to-middle-knuckle direction and palm normal in rig space ([0] the
 // trigger hand.R, [1] the support hand.L), and the curl profile (0 the
 // shotgun's, 1 the pistol's). Without one, the shotgun's fixed frames.
 struct ArmHold {std::array<Point3,2> forward{},palm{};int curls=0;std::array<Point3,2> elbow{};std::array<bool,2> swingOnly{};float forearmRoll=0;};   // swingOnly: aim the fingers and keep the forearm's own roll (no palm forced)   // elbow: the way each elbow points (rig space), zero for the default
 void pose(float phase,float recoil,int curls=0);
    bool poseAction(const char* action,float phase);
 static bool testAttachedSkinning();
 // Numbers for a posed arm (side 0 right, 1 left): reach as a share of the
 // arm's length; the wrist's twist from the bind pose (degrees); the elbow's
 // direction off the shoulder-wrist line (rig space); and palmCurl, the middle
 // finger's bend toward the computed palm normal (positive when the palm
 // frame's cross product is the right way round for that hand).
 struct GripReport {float reach=0,wristTwist=0,palmCurl=0,elbowTwist=0;Point3 elbow{};};   // elbowTwist: the forearm's turn on the upper arm about its own axis, from the bind pose
 GripReport gripReport(int side)const;
 static bool testVrHands();
    void poseCreature(int clip,float phase);
     void poseCreatureBlend(int fromClip,float fromPhase,int toClip,float toPhase,float blend);
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
 void poseAttached(Point3 right,Point3 left,float elbowSwing,float pitch,float yaw,float phase,float recoil,const ArmHold* hold=nullptr);
 // The VR avatar's arms: both wrists at right and left (rig space) turned to hold's hand frames, the elbows
 // toward hold.elbow, and each finger bent by its tracked curl (thumb to little finger, 0 open, 1 the fist):
 // curls[0] the right hand, [1] the left.
 void poseTracked(Point3 right,Point3 left,const ArmHold& hold,const std::array<std::array<float,5>,2>& curls);
 // Reuse an existing rig's immutable animation scene; retain this mesh's geometry and skin.
 void shareActionsFrom(const Mesh& source);
 int sharedActionResource()const{return m_sharedActionResource;}
 Point3 bonePosition(const char* name)const;
 std::vector<MeshTriangle> trackedHand(bool left,const std::array<float,5>& curls);
 Point3 poseAnchor(int index)const{return m_poseAnchors.at(size_t(index));}
 // Imported, normalized topology remains available after animation poses.
 const std::vector<MeshTriangle>& bindTriangles()const;
 const std::vector<uint16_t>& creatureVertexIndices()const{return m_creatureIndices;}
 // Posing a creature from outside its clips: a clip's skinned vertices (one per
 // vertex of the bank, as creatureVertexIndices numbers them), a set of them
 // published as the mesh's triangles, and per vertex the summed skin weight of
 // the named bones (Claire's body is layered and aimed this way from outside).
 void sampleCreatureVertices(int clip,float phase,std::vector<Point3>& out)const{sampleCreatureClip(clip,phase,out);}
 void publishCreatureVertices(const std::vector<Point3>& vertices);
 std::vector<float> creatureBoneWeights(std::initializer_list<const char*> bones)const;
 size_t creatureVertexCount()const{return m_creatureVertices;}
 std::vector<MeshTriangle> triangles;
 std::vector<std::vector<MeshVertex>> openRings;
 Point3 minimum,maximum;
 size_t bones=0;
 std::string description;
 std::vector<std::string> materialNames;
private:
 Source m_source;
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
 void grip(Point3 right={-.025f,1.55f,.223f},Point3 left={-.025f,1.60f,.49f},float swing=0,float pitch=0,float yaw=0,bool skin=true,const ArmHold* hold=nullptr);
 ufbx_scene* m_scene=nullptr;
 ufbx_scene* m_bindScene=nullptr;
 ufbx_scene* m_actionScene=nullptr;
 int m_sharedActionResource=0;
 std::vector<std::pair<uint32_t,uint32_t>> m_actionNodes;
 void cacheFingerGrips(ufbx_scene* actions);
 std::vector<ufbx_transform_override> m_fingerGrip;
 std::array<std::array<float,5>,2> m_trackedCurls{};   // poseTracked's curls (pose's curl set 3)
};
// A model decoded the first time something draws, measures or warms it, not at start-up:
// the library grows by the model and a map uses a few dozen of them. get() is safe from
// any thread (the first caller decodes, the others wait for it); the Mesh it returns is
// the one the eager constructor made, so what it draws is the same.
class LazyMesh {
public:
 LazyMesh(Mesh&& deferred):m_source(deferred.source()){}
 LazyMesh(const LazyMesh&)=delete;LazyMesh& operator=(const LazyMesh&)=delete;
 Mesh& get()const{if(!m_ready.load(std::memory_order_acquire))std::call_once(m_once,[this]{m_mesh=std::make_unique<Mesh>(m_source.resource,m_source.nodeFilter.c_str(),m_source.animation,m_source.anchor);m_ready.store(true,std::memory_order_release);});return *m_mesh;}
 bool loaded()const{return m_ready.load(std::memory_order_acquire);}
 const Mesh::Source& source()const{return m_source;}
private:
 Mesh::Source m_source;mutable std::once_flag m_once;mutable std::unique_ptr<Mesh> m_mesh;mutable std::atomic<bool> m_ready{false};
};
// The library's models: written as an array of Mesh{id} (tools/library/register.py adds
// to it), indexed as one, each decoded on first use. Starts with Mesh::Deferred{}.
template<size_t N> class LazyMeshes {
public:
 template<class... M> LazyMeshes(const Mesh::Deferred&,M&&... meshes):m_meshes{LazyMesh(std::forward<M>(meshes))...}{static_assert(sizeof...(M)==N,"one Mesh per facility model");Mesh::deferring()=false;}
 Mesh& operator[](size_t i)const{return m_meshes[i].get();}
 Mesh& at(size_t i)const{return m_meshes.at(i).get();}
 const LazyMesh& slot(size_t i)const{return m_meshes.at(i);}
 static constexpr size_t size(){return N;}
 size_t loaded()const{size_t n=0;for(const auto& m:m_meshes)n+=m.loaded();return n;}
 // Walking every model decodes every model (a full report or check asks for that).
 struct iterator {const LazyMesh* at;Mesh& operator*()const{return at->get();}iterator& operator++(){++at;return *this;}bool operator!=(const iterator& o)const{return at!=o.at;}};
 iterator begin()const{return {m_meshes.data()};}
 iterator end()const{return {m_meshes.data()+N};}
private:
 std::array<LazyMesh,N> m_meshes;
};
}
