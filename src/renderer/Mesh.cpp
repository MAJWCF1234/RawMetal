#include "../core/FrameProfile.h"
#include "Mesh.h"
#include "../core/PackedResource.h"
#include "../core/ActorBodyAim.h"
#include <windows.h>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>
#include <limits>
#include <map>
#include <emmintrin.h>
namespace retro {
namespace {
float layerLength(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
bool finiteLayerPoint(Point3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::fabs(p.x)<64&&std::fabs(p.y)<64&&std::fabs(p.z)<64;}
float layerSmooth(float a,float b,float value){float t=std::clamp((value-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
struct LayerRotation {
 float cy,sy,cp,sp;
 LayerRotation(float yaw,float pitch):cy(std::cos(yaw)),sy(std::sin(yaw)),cp(std::cos(pitch)),sp(std::sin(pitch)){}
 Point3 operator()(Point3 p)const{
  // Engine meshes face +Z with +Y up. Pitch upward around local right,
  // then yaw that right axis with the body: Ry(yaw) * Rx(-pitch).
  Point3 raised{p.x,p.y*cp+p.z*sp,-p.y*sp+p.z*cp};
  return {raised.x*cy+raised.z*sy,raised.y,-raised.x*sy+raised.z*cy};
 }
};
bool validLayerPose(const CreatureLayerPose&pose){
 if(!finiteLayerPoint(pose.pelvis)||!finiteLayerPoint(pose.spine))return false;
 for(const auto&arm:pose.arms){
  for(auto point:arm)if(!finiteLayerPoint(point))return false;
  float upper=layerLength(arm[1]-arm[0]),lower=layerLength(arm[2]-arm[1]),palm=layerLength(arm[3]);
  if(upper<.001f||upper>4||lower<.001f||lower>4||palm<.00001f)return false;
 }
 return true;
}
}
Mesh::Mesh(int id,const char* nodeFilter,int animationResource):m_materialParts((id>=163&&id<200)||(id>=257&&id<=265)||(id>=284&&id<=294)||id==142||id==277||id==303||id==308||id==313||id==318||id==323||id==328),m_nodeFilter(nodeFilter){
 m_positionOnlySkinning=id==111;
 auto resource=loadResource(id);
 ufbx_load_opts opts{};opts.evaluate_skinning=true;opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;
 if(!resource.empty()&&resource[0]=='#')opts.file_format=UFBX_FILE_FORMAT_OBJ;
 ufbx_error error{};m_scene=ufbx_load_memory(resource.data(),resource.size(),&opts,&error);
 if(!m_scene)throw std::runtime_error("FBX mesh decode failed");
 for(auto material:m_scene->materials)materialNames.emplace_back(material->name.data);
 extract(m_scene);
 // This supplied vent lies in the XZ ceiling plane, with its grille toward -Y.
 // Rotate it into a wall panel: grille toward +Z, horizontal texture louvers.
 if(id==166)for(auto&face:triangles)for(auto&v:face.v){auto p=v.p;v.p={-p.z,p.x,-p.y};}
 // These electrical cabinets and the forklift were authored facing opposite RawMetal's
 // fixture-forward convention. Normalize them once at import so every cabinet
 // in every map, editor preview, and future placement uses the visible/control
 // face as forward instead of showing the back/interior side.
 if(id==171||id==258||id==294)for(auto&face:triangles)for(auto&v:face.v){v.p.x=-v.p.x;v.p.z=-v.p.z;}
 // Keep the importer normalization in the immutable prototypes too. The
 // lazy bind snapshot does not depend on which animation was rendered first.
 for(size_t i=0;i<triangles.size();++i)m_cachedTriangles[i].prototype=triangles[i];
 minimum={1e9f,1e9f,1e9f};maximum={-1e9f,-1e9f,-1e9f};
 for(auto&t:triangles)for(auto&v:t.v){minimum.x=std::min(minimum.x,v.p.x);minimum.y=std::min(minimum.y,v.p.y);minimum.z=std::min(minimum.z,v.p.z);maximum.x=std::max(maximum.x,v.p.x);maximum.y=std::max(maximum.y,v.p.y);maximum.z=std::max(maximum.z,v.p.z);}
 std::ostringstream info;info<<id<<": "<<triangles.size()<<" triangles; bounds "<<minimum.x<<","<<minimum.y<<","<<minimum.z<<" to "<<maximum.x<<","<<maximum.y<<","<<maximum.z<<"\n";
 for(auto node:m_scene->nodes)if(node->bone){++bones;info<<" bone "<<node->name.data<<" at "<<node->node_to_world.m03<<","<<node->node_to_world.m13<<","<<node->node_to_world.m23<<"\n";}
 description=info.str();if(triangles.empty())throw std::runtime_error("Mesh has no triangles");
 if(id==242||animationResource){m_creatureFrames=loadResource(animationResource?animationResource:244);uint32_t vertices=0,clips=0,headerSamples=0;
  bool compact=m_creatureFrames.size()>=20&&!std::memcmp(m_creatureFrames.data(),"RMA3",4);
  if(m_creatureFrames.size()<20||(!compact&&std::memcmp(m_creatureFrames.data(),"RMA2",4)))throw std::runtime_error("Invalid creature animation");
  std::memcpy(&vertices,m_creatureFrames.data()+4,4);std::memcpy(&headerSamples,m_creatureFrames.data()+8,4);std::memcpy(&clips,m_creatureFrames.data()+12,4);
  std::memcpy(&m_creatureVertices,m_creatureFrames.data()+16,4);
  if(vertices!=triangles.size()*3||clips<1||clips>32||m_creatureVertices==0||m_creatureVertices>65535)throw std::runtime_error("Creature animation topology mismatch");
  m_creatureFrameOffset=20+size_t(vertices)*2+(compact?size_t(clips)*4:0);
  if(m_creatureFrames.size()<m_creatureFrameOffset)throw std::runtime_error("Missing creature sample table");
  m_creatureSampleCounts.resize(clips);m_creatureSampleOffsets.resize(clips);
  uint32_t totalSamples=0;
  for(uint32_t clip=0;clip<clips;++clip){uint32_t count=headerSamples;
   if(compact)std::memcpy(&count,m_creatureFrames.data()+20+size_t(vertices)*2+size_t(clip)*4,4);
   if(count<2||count>512)throw std::runtime_error("Invalid creature clip sample count");
   m_creatureSampleCounts[clip]=count;m_creatureSampleOffsets[clip]=totalSamples;totalSamples+=count;
   m_creatureSamples=std::max(m_creatureSamples,count);
  }
  if(compact&&totalSamples!=headerSamples)throw std::runtime_error("Creature sample table total mismatch");
  size_t payload=m_creatureFrameOffset+size_t(m_creatureVertices)*6*totalSamples;
  if(m_creatureFrames.size()<payload)throw std::runtime_error("Creature animation topology mismatch");
  m_creatureClips=clips;
  if(m_creatureFrames.size()!=payload){
   if(m_creatureFrames.size()<payload+20||std::memcmp(m_creatureFrames.data()+payload,"RML1",4))throw std::runtime_error("Invalid creature layer trailer");
   uint32_t version=0,samples=0,layerClips=0,stride=0;
   std::memcpy(&version,m_creatureFrames.data()+payload+4,4);std::memcpy(&samples,m_creatureFrames.data()+payload+8,4);
   std::memcpy(&layerClips,m_creatureFrames.data()+payload+12,4);std::memcpy(&stride,m_creatureFrames.data()+payload+16,4);
   size_t layerEnd=payload+20+size_t(totalSamples)*stride;
   if(version!=(compact?2u:1u)||samples!=headerSamples||layerClips!=clips||stride!=sizeof(CreatureLayerPose)||m_creatureFrames.size()<layerEnd)throw std::runtime_error("Creature layer topology mismatch");
   m_creatureLayerFrames.resize(totalSamples);
   std::memcpy(m_creatureLayerFrames.data(),m_creatureFrames.data()+payload+20,m_creatureLayerFrames.size()*sizeof(CreatureLayerPose));
   for(const auto&pose:m_creatureLayerFrames)if(!validLayerPose(pose))throw std::runtime_error("Invalid creature layer joints");
   m_creatureLayerPose=m_creatureLayerFrames.front();
   if(m_creatureFrames.size()!=layerEnd){
    uint32_t durationClips=0;
    if(m_creatureFrames.size()<layerEnd+8||std::memcmp(m_creatureFrames.data()+layerEnd,"RMD1",4))throw std::runtime_error("Invalid creature duration trailer");
    std::memcpy(&durationClips,m_creatureFrames.data()+layerEnd+4,4);
    if(durationClips!=clips||m_creatureFrames.size()!=layerEnd+8+size_t(clips)*sizeof(float))throw std::runtime_error("Creature duration topology mismatch");
    m_creatureDurations.resize(clips);
    std::memcpy(m_creatureDurations.data(),m_creatureFrames.data()+layerEnd+8,size_t(clips)*sizeof(float));
    for(float duration:m_creatureDurations)if(!std::isfinite(duration)||duration<=0||duration>120)throw std::runtime_error("Invalid creature clip duration");
   }
  }
  if(id==277){m_anchorFrames=loadResource(282);if(m_anchorFrames.size()!=size_t(totalSamples)*24)throw std::runtime_error("Actor attachment topology mismatch");}
    if(id==323){m_anchorFrames=loadResource(339);if(m_anchorFrames.size()!=size_t(totalSamples)*24)throw std::runtime_error("Medic treatment attachment topology mismatch");}
  m_creatureIndices.resize(vertices);std::memcpy(m_creatureIndices.data(),m_creatureFrames.data()+20,size_t(vertices)*2);
  for(auto index:m_creatureIndices)if(index>=m_creatureVertices)throw std::runtime_error("Invalid creature vertex index");
  if(id==303||id==308||id==313||id==318||id==323||id==328){
   auto calibration=loadResource(340+(id-303)/5);uint32_t corners=0;
   if(calibration.size()<104||std::memcmp(calibration.data(),"RMC2",4))throw std::runtime_error("Invalid cleanup layer calibration");
   std::memcpy(&corners,calibration.data()+100,4);
   if(corners!=vertices||calibration.size()!=104+size_t(corners)*4)throw std::runtime_error("Cleanup layer calibration topology mismatch");
   // Cache immutable bind positions and semantic ownership once. Hand vertices
   // below the belt still belong to the upper layer, including whole fingers.
   sampleCreatureClip(0,0,m_creatureBindUnique);
   m_creatureArmMask.assign(m_creatureVertices,0);m_creatureHandSide.assign(m_creatureVertices,0);
   for(size_t corner=0;corner<vertices;++corner){
    auto index=m_creatureIndices[corner];m_creatureBindUnique[index]=m_cachedTriangles[corner/3].prototype.v[corner%3].p;
    const auto*weight=calibration.data()+104+corner*4;
    if(weight[3]>1)throw std::runtime_error("Invalid cleanup layer arm side");
    float influence=weight[0]/255.f,palm=weight[2]/255.f;
    m_creatureArmMask[index]=std::max(m_creatureArmMask[index],influence);
    if(influence*palm>.15f)m_creatureHandSide[index]=weight[3]+1;
   }
   m_creatureUpperMask.resize(m_creatureVertices);m_creatureHeadMask.resize(m_creatureVertices);
   for(size_t i=0;i<m_creatureVertices;++i){auto p=m_creatureBindUnique[i];
    // An arm is a member of the upper clip, not a fractional blend with a
    // second arm from the walking clip. Fractional ownership below the waist
    // produced long sheets between the upper wrist and lower-clip sleeve.
    m_creatureUpperMask[i]=m_creatureArmMask[i]>.001f?1.f:layerSmooth(.75f,.97f,p.y);
    m_creatureHeadMask[i]=std::fabs(p.x)<.16f&&m_creatureArmMask[i]<.01f?layerSmooth(1.20f,1.34f,p.y):0;
    if(m_creatureHandSide[i]){m_creatureUpperMask[i]=1;m_creatureHeadMask[i]=0;}
   }
   for(size_t face=0;face<triangles.size();++face){int part=triangles[face].part;
    if(part<0||size_t(part)>=materialNames.size()||materialNames[size_t(part)].find("head")==std::string::npos)continue;
    for(int vertex=0;vertex<3;++vertex){auto index=m_creatureIndices[face*3+vertex];m_creatureHeadMask[index]=1;m_creatureUpperMask[index]=1;}
   }
  }
 }
 for(auto n:m_scene->nodes)if(n->mesh){description+=" mesh "+std::string(n->name.data)+" triangles "+std::to_string(n->mesh->num_triangles)+"\n";for(auto mat:n->materials)description+=" material "+std::string(mat->name.data)+"\n";}
 description+="Animation stacks: "+std::to_string(m_scene->anim_stacks.count)+"\n";
 for(auto stack:m_scene->anim_stacks)description+=std::string(stack->name.data)+"\n";
 if(id==111){
  // Retain the authored finger poses from the supplied left/right grab actions.
  for(auto stack:m_scene->anim_stacks){std::string name=stack->name.data;bool left=name.find("grab.L")!=std::string::npos,right=name.find("grab.R")!=std::string::npos;if(!left&&!right)continue;
   ufbx_evaluate_opts eo{};auto frame=ufbx_evaluate_scene(m_scene,stack->anim,stack->time_end,&eo,nullptr);
   if(frame){for(auto n:frame->nodes){std::string bone=n->name.data;
    if((bone.find("f_")==0||bone.find("thumb.")==0)&&bone.find(left?".L":".R")!=std::string::npos&&bone.find("_end")==std::string::npos)m_fingerGrip.push_back({n->typed_id,n->local_transform});
   }ufbx_free_scene(frame);}
  }
  description+="Authored grab finger transforms: "+std::to_string(m_fingerGrip.size())+"\n";
  m_bindScene=m_scene;ufbx_retain_scene(m_bindScene);grip();extract(m_scene);
  description+="Arm opening rings: "+std::to_string(openRings.size())+"\n";
 }
}
Mesh::~Mesh(){ufbx_free_scene(m_scene);ufbx_free_scene(m_bindScene);}
const std::vector<MeshTriangle>& Mesh::bindTriangles()const{
 if(m_bindTriangles.empty()){m_bindTriangles.reserve(m_cachedTriangles.size());for(const auto&face:m_cachedTriangles)m_bindTriangles.push_back(face.prototype);}
 return m_bindTriangles;
}
float Mesh::creatureClipDuration(int clip)const{
 if(clip<0||clip>=int(m_creatureClips))throw std::runtime_error("Missing creature clip duration");
 return hasDirectCreatureMotion()?m_creatureDurations[size_t(clip)]:0.f;
}
void Mesh::sampleCreatureClip(int clip,float phase,std::vector<Point3>& out)const{
 if(m_creatureFrames.empty()||clip<0||clip>=int(m_creatureClips))throw std::runtime_error("Missing creature clip");
 if(!std::isfinite(phase))throw std::runtime_error("Invalid creature phase");
 unsigned samples=m_creatureSampleCounts[size_t(clip)];
 float frame=std::clamp(phase,0.f,1.f)*(samples-1);unsigned a=unsigned(frame),b=std::min(a+1,samples-1);float blend=frame-a;
 size_t stride=size_t(m_creatureVertices)*6,base=m_creatureFrameOffset+size_t(m_creatureSampleOffsets[size_t(clip)])*stride;
 if(out.size()<m_creatureVertices)out.resize(m_creatureVertices);
 // Eight signed coordinates at a time, using only the x64 baseline SSE2.
 // Subtract as int32 before conversion, retaining the scalar operation order.
 static_assert(sizeof(Point3)==3*sizeof(float));
 const auto*first=m_creatureFrames.data()+base+a*stride;
 const auto*second=m_creatureFrames.data()+base+b*stride;
 auto*destination=reinterpret_cast<unsigned char*>(out.data());
 const __m128 factor=_mm_set1_ps(blend),metres=_mm_set1_ps(.001f);
 auto interpolate=[&](__m128i x,__m128i y){
  return _mm_mul_ps(_mm_add_ps(_mm_cvtepi32_ps(x),_mm_mul_ps(_mm_cvtepi32_ps(_mm_sub_epi32(y,x)),factor)),metres);
 };
 size_t coordinate=0,count=size_t(m_creatureVertices)*3;
 for(;coordinate+8<=count;coordinate+=8){
  auto x=_mm_loadu_si128(reinterpret_cast<const __m128i*>(first+coordinate*2));
  auto y=_mm_loadu_si128(reinterpret_cast<const __m128i*>(second+coordinate*2));
  auto signX=_mm_srai_epi16(x,15),signY=_mm_srai_epi16(y,15);
  auto low=interpolate(_mm_unpacklo_epi16(x,signX),_mm_unpacklo_epi16(y,signY));
  auto high=interpolate(_mm_unpackhi_epi16(x,signX),_mm_unpackhi_epi16(y,signY));
  std::memcpy(destination+coordinate*4,&low,16);std::memcpy(destination+(coordinate+4)*4,&high,16);
 }
 for(;coordinate<count;++coordinate){int16_t x,y;std::memcpy(&x,first+coordinate*2,2);std::memcpy(&y,second+coordinate*2,2);float value=(x+(y-x)*blend)*.001f;std::memcpy(destination+coordinate*4,&value,4);}
}
CreatureLayerPose Mesh::sampleCreatureLayerPose(int clip,float phase)const{
 if(!hasCreatureLayerPose()||clip<0||clip>=int(m_creatureClips)||!std::isfinite(phase))throw std::runtime_error("Missing creature layer pose");
 unsigned samples=m_creatureSampleCounts[size_t(clip)];
 float frame=std::clamp(phase,0.f,1.f)*(samples-1);unsigned a=unsigned(frame),b=std::min(a+1,samples-1);float blend=frame-a;
 size_t first=m_creatureSampleOffsets[size_t(clip)];
 const auto&A=m_creatureLayerFrames[first+a];const auto&B=m_creatureLayerFrames[first+b];
 CreatureLayerPose pose;pose.pelvis=A.pelvis+(B.pelvis-A.pelvis)*blend;pose.spine=A.spine+(B.spine-A.spine)*blend;
 for(int side=0;side<2;++side)for(int joint=0;joint<4;++joint)pose.arms[side][joint]=A.arms[side][joint]+(B.arms[side][joint]-A.arms[side][joint])*blend;
 for(int side=0;side<2;++side){auto&direction=pose.arms[side][3];float size=layerLength(direction);
  if(size<.00001f){direction=A.arms[side][3];size=layerLength(direction);}direction=direction*(1.f/size);
 }
 return pose;
}
void Mesh::sampleCreatureAnchors(int clip,float phase){
 unsigned samples=m_creatureSampleCounts[size_t(clip)];
 float frame=std::clamp(phase,0.f,1.f)*(samples-1);unsigned a=unsigned(frame),b=std::min(a+1,samples-1);float blend=frame-a;
 if(!m_anchorFrames.empty())for(int joint=0;joint<2;++joint){Point3 A,B;size_t offset=size_t(m_creatureSampleOffsets[size_t(clip)])*24+size_t(joint)*12;std::memcpy(&A,m_anchorFrames.data()+offset+a*24,12);std::memcpy(&B,m_anchorFrames.data()+offset+b*24,12);m_poseAnchors[joint]=A+(B-A)*blend;}
}
void Mesh::publishCreatureVertices(){
 for(size_t t=0;t<triangles.size();++t)for(int v=0;v<3;++v)triangles[t].v[v].p=m_creaturePoseScratch[m_creatureIndices[t*3+v]];
}
void Mesh::poseCreature(int clip,float phase){
 sampleCreatureClip(clip,phase,m_creaturePoseScratch);
 sampleCreatureAnchors(clip,phase);
 if(hasCreatureLayerPose())m_creatureLayerPose=sampleCreatureLayerPose(clip,phase);
 publishCreatureVertices();
}
void Mesh::poseCreatureTank(int lowerClip,float lowerPhase,int upperClip,float upperPhase,float aimYaw,float aimPitch){
 perf::Scope frameProfile(perf::Skin);
 // Old animation resources have no joint frames. Preserve their ordinary
 // full-body motion instead of guessing a split pivot or hand ownership.
 if(!hasCreatureLayerPose()||m_creatureUpperMask.empty()){poseCreature(lowerClip,lowerPhase);return;}
 if(!std::isfinite(aimYaw)||!std::isfinite(aimPitch))throw std::runtime_error("Invalid creature layer aim");
 aimYaw=std::clamp(aimYaw,-.70f,.70f);aimPitch=std::clamp(aimPitch,-.45f,.55f);
 sampleCreatureClip(lowerClip,lowerPhase,m_creaturePoseScratch);
 const bool sameSample=lowerClip==upperClip&&lowerPhase==upperPhase;
 if(!sameSample)sampleCreatureClip(upperClip,upperPhase,m_creaturePoseScratchB);
 const auto&upperVertices=sameSample?m_creaturePoseScratch:m_creaturePoseScratchB;
 auto lower=sampleCreatureLayerPose(lowerClip,lowerPhase),upper=sameSample?lower:sampleCreatureLayerPose(upperClip,upperPhase);
 Point3 rebase=lower.pelvis-upper.pelvis,pivot=upper.spine+rebase;
 ActorBodyAimRotation bodyAim(aimYaw,aimPitch);LayerRotation headAim(aimYaw,aimPitch*.55f);
 Point3 neckLocal=(upper.arms[0][0]+upper.arms[1][0])*.5f-upper.spine+Point3{0,.02f,0};
 Point3 neck=pivot+bodyAim(neckLocal);
 auto aimPoint=[&](Point3 p){return pivot+bodyAim(p+rebase-pivot);};
 for(size_t i=0;i<m_creatureVertices;++i){
  if(m_creatureUpperMask[i]==0.f)continue;
  Point3 local=upperVertices[i]+rebase-pivot;
  Point3 body=pivot+bodyAim(local),posed=body;
  if(m_creatureHeadMask[i]!=0.f){Point3 head=neck+headAim(local-neckLocal);posed=body+(head-body)*m_creatureHeadMask[i];}
  m_creaturePoseScratch[i]=m_creaturePoseScratch[i]+(posed-m_creaturePoseScratch[i])*m_creatureUpperMask[i];
 }
 m_creatureLayerPose=upper;m_creatureLayerPose.pelvis=lower.pelvis;m_creatureLayerPose.spine=pivot;
 for(int side=0;side<2;++side){for(int joint=0;joint<3;++joint)m_creatureLayerPose.arms[side][joint]=aimPoint(upper.arms[side][joint]);m_creatureLayerPose.arms[side][3]=bodyAim(upper.arms[side][3]);}
 sampleCreatureAnchors(upperClip,upperPhase);
 if(!m_anchorFrames.empty())for(auto&anchor:m_poseAnchors)anchor=aimPoint(anchor);
 publishCreatureVertices();
}
bool Mesh::testCleanupTankAim(){
 std::ofstream report("cleanup-tank-aim-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool condition,const char*name,int role){++checks;report<<(condition?"PASS: ":"FAIL: ")<<role<<' '<<name<<'\n';passed&=condition;};
 const float phases[]={0.f,.125f,.25f,.5f,.75f,.875f,1.f};
 const Point3 angles[]={{0,0,0},{.7f,.55f,0},{-.7f,-.45f,0},{.55f,-.35f,0},{-.5f,.45f,0}};
 const char*roles[]={"Assault","Engineer","Sharpshooter","Heavy","Medic","Rusher"};
 struct Edge {uint16_t a,b;float bind;};
 for(int role=0;role<6;++role){
  Mesh mesh(303+role*5,"",304+role*5);report<<"ROLE "<<roles[role]<<'\n';
  bool decodedExact=true;std::vector<Point3> decoded;
  for(unsigned clip=0;clip<mesh.m_creatureClips;++clip)for(float phase:{-.1f,0.f,.013f,.125f,.333f,.5f,.999f,1.f,1.1f}){
   mesh.sampleCreatureClip(int(clip),phase,decoded);
   unsigned samples=mesh.m_creatureSampleCounts[clip];float frame=std::clamp(phase,0.f,1.f)*(samples-1);unsigned a=unsigned(frame),b=std::min(a+1,samples-1);float blend=frame-a;
   size_t stride=size_t(mesh.m_creatureVertices)*6,base=mesh.m_creatureFrameOffset+size_t(mesh.m_creatureSampleOffsets[clip])*stride;
   for(size_t i=0;i<mesh.m_creatureVertices;++i)for(int axis=0;axis<3;++axis){int16_t x,y;size_t offset=i*6+axis*2;std::memcpy(&x,mesh.m_creatureFrames.data()+base+a*stride+offset,2);std::memcpy(&y,mesh.m_creatureFrames.data()+base+b*stride+offset,2);float expected=(x+(y-x)*blend)*.001f,actual;std::memcpy(&actual,reinterpret_cast<const unsigned char*>(decoded.data())+(i*3+axis)*4,4);decodedExact&=actual==expected;}
  }
  check(decodedExact,"SSE2 decoding matches scalar coordinates for every clip, including clamped endpoints and tails",role);
  check(mesh.hasCreatureLayerPose(),"real RML1 joints are present",role);if(!mesh.hasCreatureLayerPose())continue;
  if(mesh.hasDirectCreatureMotion()){
   bool timed=true;
   for(unsigned clip=0;clip<mesh.creatureClipCount();++clip){float seconds=mesh.creatureClipDuration(int(clip));timed&=std::isfinite(seconds)&&seconds>0&&float(mesh.m_creatureSampleCounts[clip]-1)/seconds>=23.9f;}
   check(timed,"direct character clips retain at least 24 source samples per second",role);
  }else check(mesh.creatureClipDuration(0)==0,"legacy banks retain their existing playback clock",role);
  const auto&bind=mesh.bindTriangles();auto calibration=loadResource(307+role*5);
  std::vector<Edge>handEdges,fingerEdges,headEdges,waistEdges;int hands[2]{};
  for(auto side:mesh.m_creatureHandSide)if(side)++hands[side-1];
  bool handOwned=hands[0]>20&&hands[1]>20;
  for(size_t i=0;i<mesh.m_creatureVertices;++i)if(mesh.m_creatureHandSide[i])handOwned&=mesh.m_creatureUpperMask[i]==1&&mesh.m_creatureHeadMask[i]==0;
  check(handOwned,"whole hands belong to the upper layer",role);
  for(size_t face=0;face<bind.size();++face)for(int vertex=0;vertex<3;++vertex){
   int next=(vertex+1)%3;auto a=mesh.m_creatureIndices[face*3+vertex],b=mesh.m_creatureIndices[face*3+next];
   Edge edge{a,b,layerLength(bind[face].v[vertex].p-bind[face].v[next].p)};
   if(mesh.m_creatureHandSide[a]&&mesh.m_creatureHandSide[a]==mesh.m_creatureHandSide[b])handEdges.push_back(edge);
   const auto*AWeight=calibration.data()+104+(face*3+vertex)*4;const auto*BWeight=calibration.data()+104+(face*3+next)*4;
   if(AWeight[0]>250&&AWeight[2]>250&&BWeight[0]>250&&BWeight[2]>250&&AWeight[3]==BWeight[3])fingerEdges.push_back(edge);
   if(mesh.m_creatureHeadMask[a]==1&&mesh.m_creatureHeadMask[b]==1)headEdges.push_back(edge);
   auto A=mesh.m_creatureBindUnique[a],B=mesh.m_creatureBindUnique[b];
   if(edge.bind>.001f&&A.y>.75f&&A.y<.97f&&B.y>.75f&&B.y<.97f&&mesh.m_creatureArmMask[a]<.01f&&mesh.m_creatureArmMask[b]<.01f)waistEdges.push_back(edge);
  }
  float footError=0,handChange=0,handBindError=0,headChange=0,waistStretch=1,jointLengthError=0,crouchTop=0;
  bool finite=true,grounded=true,compositionExact=true;
  std::vector<Point3>lowerVertices,upperVertices;
  for(int lowerClip:{0,1,5})for(int upperClip:{0,2,5})for(float phase:phases){
   if((lowerClip==5)!=(upperClip==5))continue;
   float upperPhase=phase;
   mesh.sampleCreatureClip(lowerClip,phase,lowerVertices);mesh.sampleCreatureClip(upperClip,upperPhase,upperVertices);
   auto upperPose=mesh.sampleCreatureLayerPose(upperClip,upperPhase);
   for(auto angle:angles){
    auto lowerPose=mesh.sampleCreatureLayerPose(lowerClip,phase);
    Point3 rebase=lowerPose.pelvis-upperPose.pelvis,pivot=upperPose.spine+rebase;
    ActorBodyAimRotation bodyAim(angle.x,angle.y);LayerRotation headAim(angle.x,angle.y*.55f);
    Point3 neckLocal=(upperPose.arms[0][0]+upperPose.arms[1][0])*.5f-upperPose.spine+Point3{0,.02f,0},neck=pivot+bodyAim(neckLocal);
    mesh.poseCreatureTank(lowerClip,phase,upperClip,upperPhase,angle.x,angle.y);
    const auto&posed=mesh.m_creaturePoseScratch;
    finite&=validLayerPose(mesh.creatureLayerPose());
    for(size_t i=0;i<mesh.m_creatureVertices;++i){
     auto local=upperVertices[i]+rebase-pivot;auto body=pivot+bodyAim(local),head=neck+headAim(local-neckLocal);
     auto reference=lowerVertices[i]+(body+(head-body)*mesh.m_creatureHeadMask[i]-lowerVertices[i])*mesh.m_creatureUpperMask[i];
     compositionExact&=posed[i].x==reference.x&&posed[i].y==reference.y&&posed[i].z==reference.z;
     finite&=finiteLayerPoint(posed[i]);grounded&=posed[i].y>=-.003f&&posed[i].y<=2.1f;
     if(lowerClip==5)crouchTop=std::max(crouchTop,posed[i].y);
     if(mesh.m_creatureBindUnique[i].y<.13f&&mesh.m_creatureArmMask[i]<.01f)footError=std::max(footError,layerLength(posed[i]-lowerVertices[i]));
    }
    for(auto edge:handEdges){float length=layerLength(posed[edge.a]-posed[edge.b]);
     handChange=std::max(handChange,std::fabs(length-layerLength(upperVertices[edge.a]-upperVertices[edge.b])));
    }
    for(auto edge:fingerEdges)handBindError=std::max(handBindError,std::fabs(layerLength(posed[edge.a]-posed[edge.b])-edge.bind));
    for(auto edge:headEdges)headChange=std::max(headChange,std::fabs(layerLength(posed[edge.a]-posed[edge.b])-layerLength(upperVertices[edge.a]-upperVertices[edge.b])));
    for(auto edge:waistEdges){float baseline=std::max({edge.bind,layerLength(lowerVertices[edge.a]-lowerVertices[edge.b]),layerLength(upperVertices[edge.a]-upperVertices[edge.b])});waistStretch=std::max(waistStretch,layerLength(posed[edge.a]-posed[edge.b])/baseline);}
    for(int side=0;side<2;++side)for(int joint=0;joint<2;++joint){const auto&arm=mesh.creatureLayerPose().arms[side];jointLengthError=std::max(jointLengthError,std::fabs(layerLength(arm[joint+1]-arm[joint])-layerLength(upperPose.arms[side][joint+1]-upperPose.arms[side][joint])));}
   }
  }
  report<<"hand split error mm "<<handChange*1000<<" bind error mm "<<handBindError*1000<<" waist stretch "<<waistStretch<<" crouch top "<<crouchTop<<'\n';
  check(footError<.000001f,"feet retain exact lower-layer vertices",role);
  check(compositionExact,"Zero-weight layer fast paths match the full float composition for every vertex",role);
  check(handChange<.00001f,"split and aim preserve every hand edge",role);
  // Direct skinning bends fingers across their joints. Compare against the
  // selected source pose above, rather than demanding T-pose edge lengths.
  if(!mesh.hasDirectCreatureMotion())check(handBindError<.006f,"legacy looping hands preserve authored rigid finger geometry",role);
  check(headChange<.00001f,"aim preserves rigid head geometry",role);
  check(waistStretch<3.f,"waist edges remain bounded across gait and aim",role);
  check(jointLengthError<.00001f,"composed arm metadata preserves limb lengths",role);
  check(finite&&grounded,"all combined poses and joints remain finite and grounded",role);
  check(crouchTop<=1.363f,"aimed crouch stays inside the 1.36 metre NPC hull",role);
  float identicalError=0,loopError=0,rootDrift=0,clampError=0;
  for(int clip:{0,1,5}){
   auto start=mesh.sampleCreatureLayerPose(clip,0),end=mesh.sampleCreatureLayerPose(clip,1);auto rootDelta=end.pelvis-start.pelvis;
   rootDrift=std::max(rootDrift,std::hypot(rootDelta.x,rootDelta.z));
   mesh.sampleCreatureClip(clip,0,lowerVertices);mesh.sampleCreatureClip(clip,1,upperVertices);
   for(size_t i=0;i<mesh.m_creatureVertices;++i)loopError=std::max(loopError,layerLength(lowerVertices[i]-upperVertices[i]));
   for(float phase:phases){mesh.sampleCreatureClip(clip,phase,upperVertices);mesh.poseCreatureTank(clip,phase,clip,phase,0,0);
    for(size_t i=0;i<mesh.m_creatureVertices;++i)identicalError=std::max(identicalError,layerLength(mesh.m_creaturePoseScratch[i]-upperVertices[i]));
   }
  }
  check(rootDrift<.004f&&loopError<.006f,"idle walk and crouch loops have no planar root reset",role);
  check(identicalError<.000001f,"identical layers reproduce the original baked pose",role);
  mesh.poseCreatureTank(1,.5f,0,.5f,.7f,.55f);upperVertices=mesh.m_creaturePoseScratch;
  mesh.poseCreatureTank(1,.5f,0,.5f,2.f,2.f);
  for(size_t i=0;i<mesh.m_creatureVertices;++i)clampError=std::max(clampError,layerLength(mesh.m_creaturePoseScratch[i]-upperVertices[i]));
  mesh.poseCreatureTank(1,.5f,0,.5f,-.7f,-.45f);upperVertices=mesh.m_creaturePoseScratch;
  mesh.poseCreatureTank(1,.5f,0,.5f,-2.f,-2.f);
  for(size_t i=0;i<mesh.m_creatureVertices;++i)clampError=std::max(clampError,layerLength(mesh.m_creaturePoseScratch[i]-upperVertices[i]));
  check(clampError<.000001f,"yaw and pitch limits apply to the complete composed pose",role);
  auto headCenter=[&](){Point3 sum{};int count=0;for(size_t i=0;i<mesh.m_creatureVertices;++i)if(mesh.m_creatureHeadMask[i]==1){sum=sum+mesh.m_creaturePoseScratch[i];++count;}return sum*(1.f/std::max(1,count));};
  mesh.poseCreatureTank(1,.5f,0,.5f,0,0);auto neutral=headCenter();
  mesh.poseCreatureTank(1,.5f,0,.5f,0,.45f);auto raised=headCenter();
  mesh.poseCreatureTank(1,.5f,0,.5f,0,-.35f);auto lowered=headCenter();
  check(raised.z<neutral.z-.02f&&lowered.z>neutral.z+.02f,"positive elevation raises the upper look direction",role);
  auto direction=LayerRotation(.55f,.3f)({0,0,1});
  check(std::fabs(direction.x-std::sin(.55f)*std::cos(.3f))<.000001f&&std::fabs(direction.y-std::sin(.3f))<.000001f&&std::fabs(direction.z-std::cos(.55f)*std::cos(.3f))<.000001f,"combined yaw and pitch use the yawed local right axis",role);
  auto savedFrames=std::move(mesh.m_creatureLayerFrames);mesh.sampleCreatureClip(1,.5f,upperVertices);mesh.poseCreatureTank(1,.5f,0,.8f,.7f,.55f);float legacyError=0;
  for(size_t i=0;i<mesh.m_creatureVertices;++i)legacyError=std::max(legacyError,layerLength(mesh.m_creaturePoseScratch[i]-upperVertices[i]));
  mesh.m_creatureLayerFrames=std::move(savedFrames);check(legacyError<.000001f,"legacy RMA2 safely falls back to whole-body motion",role);
  bool rejected=false;try{mesh.poseCreatureTank(1,.5f,0,.5f,std::numeric_limits<float>::quiet_NaN(),0);}catch(const std::runtime_error&){rejected=true;}
  auto invalid=mesh.m_creatureLayerFrames.front();invalid.pelvis.x=std::numeric_limits<float>::quiet_NaN();
  check(rejected&&!validLayerPose(invalid)&&!validLayerPose({}),"nonfinite aim and malformed zero joints are rejected",role);
  if(role==4){mesh.poseCreatureTank(1,.5f,6,.5f,.4f,.2f);float anchorError=0;
   for(int side=0;side<2;++side)anchorError=std::max(anchorError,layerLength(mesh.poseAnchor(side)-mesh.creatureLayerPose().arms[side][2]));
   check(anchorError<.02f,"Medic kit anchors share composed wrist transforms",role);
  }
 }
 report<<"CHECKS "<<checks<<'\n';return passed;
}
Point3 Mesh::bonePosition(const char* name)const{auto n=ufbx_find_node(m_scene,name);if(!n)return {};return {float(n->node_to_world.m03),float(n->node_to_world.m13),float(n->node_to_world.m23)};}
std::vector<MeshTriangle> Mesh::trackedHand(bool left,const std::array<float,5>& curls){
 if(!m_bindScene)return {};
 std::string suffix=left?".L":".R";std::vector<ufbx_transform_override> changes;
 const char* fingers[]={"thumb","f_index","f_middle","f_ring","f_pinky"};
 for(int finger=0;finger<5;++finger)for(int joint=1;joint<=3;++joint){
  auto name=std::string(fingers[finger])+".0"+std::to_string(joint)+suffix;auto node=ufbx_find_node(m_bindScene,name.c_str());if(!node)continue;
  auto transform=node->local_transform;float angle=std::clamp(curls[finger],0.f,1.f)*1.15f;
  ufbx_quat bend{std::sin(angle*.5f),0,0,std::cos(angle*.5f)};transform.rotation=ufbx_quat_mul(transform.rotation,bend);changes.push_back({node->typed_id,transform});
 }
 ufbx_anim_opts options{};options.transform_overrides={changes.data(),changes.size()};auto anim=ufbx_create_anim(m_bindScene,&options,nullptr);ufbx_evaluate_opts evaluate{};evaluate.evaluate_skinning=true;auto scene=ufbx_evaluate_scene(m_bindScene,anim,0,&evaluate,nullptr);ufbx_free_anim(anim);if(!scene)return {};
 auto hand=ufbx_find_node(scene,("hand"+suffix).c_str()),middle=ufbx_find_node(scene,("f_middle.01"+suffix).c_str()),index=ufbx_find_node(scene,("f_index.01"+suffix).c_str()),pinky=ufbx_find_node(scene,("f_pinky.01"+suffix).c_str());
 auto position=[](ufbx_node*n){return Point3{float(n->node_to_world.m03),float(n->node_to_world.m13),float(n->node_to_world.m23)};};
 auto dot=[](Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};auto unit=[&](Point3 p){return p*(1/std::sqrt(std::max(.000001f,dot(p,p))));};
 std::vector<MeshTriangle> result;if(hand&&middle&&index&&pinky){
  auto cross3=[](Point3 a,Point3 b){return Point3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
  auto wrist=position(hand),forward=unit(position(middle)-wrist),across=unit(position(index)-position(pinky));auto palm=unit(cross3(across,forward)*(left?-1.f:1.f));across=unit(cross3(forward,palm));
  // Clip exactly at the wrist plane. Triangles crossing it are split, so no
  // forearm remains and the hand retains the authored texture coordinates.
  for(const auto&cached:m_cachedTriangles){auto node=scene->nodes[cached.node];MeshVertex polygon[6]{};int count=3;float nearest=999;
   for(int i=0;i<3;++i){auto p=ufbx_get_vertex_vec3(&node->mesh->skinned_position,cached.corners[i]);if(node->mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);polygon[i]=cached.prototype.v[i];polygon[i].p={float(p.x),float(p.y),float(p.z)};auto delta=polygon[i].p-wrist;nearest=std::min(nearest,dot(delta,delta));}
   if(nearest>.25f*.25f)continue;
   MeshVertex clipped[6]{};int output=0;for(int i=0;i<count;++i){auto a=polygon[i],b=polygon[(i+1)%count];float da=dot(a.p-wrist,forward),db=dot(b.p-wrist,forward);if(da>=0)clipped[output++]=a;if((da>=0)!=(db>=0)){float t=da/(da-db);clipped[output++]={a.p+(b.p-a.p)*t,a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,1};}}
   for(int i=0;i<output;++i){auto p=clipped[i].p-wrist;clipped[i].p={dot(p,across),dot(p,palm),-dot(p,forward)};}
   for(int i=1;i+1<output;++i)result.push_back({{clipped[0],clipped[i],clipped[i+1]},cached.prototype.part});
  }
 }
 ufbx_free_scene(scene);return result;
}
__declspec(noinline) bool Mesh::testVrHands(){
 Mesh mesh(111);float changed=0;
 for(bool left:{false,true}){auto open=mesh.trackedHand(left,{0,0,0,0,0}),closed=mesh.trackedHand(left,{1,1,1,1,1});if(open.size()<20||closed.size()<20)return false;
  for(const auto&faces:{open,closed})for(const auto&face:faces)for(const auto&v:face.v)if(!std::isfinite(v.p.x)||!std::isfinite(v.p.y)||!std::isfinite(v.p.z)||std::fabs(v.p.x)>.3f||std::fabs(v.p.y)>.3f||v.p.z>.0001f||v.p.z<-.4f)return false;
  for(size_t i=0;i<std::min(open.size(),closed.size());++i)for(int j=0;j<3;++j){auto d=open[i].v[j].p-closed[i].v[j].p;changed+=d.x*d.x+d.y*d.y+d.z*d.z;}
 }
 return changed>.0001f;
}
void Mesh::grip(Point3 right,Point3 left,float swing,float pitch,float yaw,bool skin){
 auto add=[](ufbx_vec3 a,ufbx_vec3 b){return ufbx_vec3{a.x+b.x,a.y+b.y,a.z+b.z};};
 auto sub=[](ufbx_vec3 a,ufbx_vec3 b){return ufbx_vec3{a.x-b.x,a.y-b.y,a.z-b.z};};
 auto mul=[](ufbx_vec3 a,double s){return ufbx_vec3{a.x*s,a.y*s,a.z*s};};
 auto dot=[](ufbx_vec3 a,ufbx_vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 auto unit=[&](ufbx_vec3 a){return mul(a,1.0/std::sqrt(std::max(1e-12,dot(a,a))));};
 auto position=[](ufbx_node*n){return ufbx_vec3{n->node_to_world.m03,n->node_to_world.m13,n->node_to_world.m23};};
 auto arc=[&](ufbx_vec3 a,ufbx_vec3 b){a=unit(a);b=unit(b);ufbx_quat q{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x,1+dot(a,b)};double s=1/std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return ufbx_quat{q.x*s,q.y*s,q.z*s,q.w*s};};
 auto rotated=[&](ufbx_node*n,ufbx_quat delta){auto global=ufbx_matrix_to_transform(&n->node_to_world).rotation;auto parent=ufbx_matrix_to_transform(&n->parent->node_to_world).rotation;parent={-parent.x,-parent.y,-parent.z,parent.w};auto tr=n->local_transform;tr.rotation=ufbx_quat_mul(parent,ufbx_quat_mul(delta,global));return tr;};
 // Two-bone IK establishes the grip using the actual upper-arm and forearm bones.
 auto cross=[](ufbx_vec3 a,ufbx_vec3 b){return ufbx_vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
 ufbx_quat handRot[2];for(int s=0;s<2;++s){
  std::string suffix=s?".L":".R";auto hand=ufbx_find_node(m_scene,("hand"+suffix).c_str());
  auto forward=unit(sub(position(ufbx_find_node(m_scene,("f_middle.01"+suffix).c_str())),position(hand)));
  auto across=sub(position(ufbx_find_node(m_scene,("f_index.01"+suffix).c_str())),position(ufbx_find_node(m_scene,("f_pinky.01"+suffix).c_str())));
  auto palm=unit(s?cross(forward,across):cross(across,forward));
  // Support palm faces up across the fore-end; trigger palm faces the stock.
  ufbx_vec3 desiredForward=unit(s?ufbx_vec3{-.95,0,.312}:ufbx_vec3{0,.65,.76});
  ufbx_vec3 desiredPalm=s?ufbx_vec3{0,1,0}:ufbx_vec3{1,0,0};
  ufbx_vec3 source[]={forward,palm,cross(forward,palm)},target[]={desiredForward,desiredPalm,cross(desiredForward,desiredPalm)};
  ufbx_matrix rotation{};
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)for(int axis=0;axis<3;++axis)rotation.cols[col].v[row]+=target[axis].v[row]*source[axis].v[col];
  handRot[s]=ufbx_quat_mul(ufbx_matrix_to_transform(&rotation).rotation,ufbx_matrix_to_transform(&hand->node_to_world).rotation);
 }
 for(int step=0;step<3;++step){std::vector<ufbx_transform_override> changes;
  for(int side=0;side<2;++side){std::string suffix=side?".L":".R";
   auto upper=ufbx_find_node(m_scene,("upper_arm"+suffix).c_str()),elbow=ufbx_find_node(m_scene,("forearm"+suffix).c_str()),hand=ufbx_find_node(m_scene,("hand"+suffix).c_str());
   if(!upper||!elbow||!hand)continue;
   auto target=side?left:right;
   ufbx_vec3 S=position(upper),E=position(elbow),H=position(hand),T{target.x,target.y,target.z};
   if(step==0){auto axis=unit(sub(T,S));double l1=std::sqrt(dot(sub(E,S),sub(E,S))),l2=std::sqrt(dot(sub(H,E),sub(H,E))),d=std::sqrt(dot(sub(T,S),sub(T,S)));d=std::clamp(d,std::fabs(l1-l2)+.001,l1+l2-.001);
    double along=(l1*l1-l2*l2+d*d)/(2*d),height=std::sqrt(std::max(0.,l1*l1-along*along));
    ufbx_vec3 bend={side?1.:-1.,-.8+double(swing)*.6,-.2+double(swing)};bend=unit(sub(bend,mul(axis,dot(bend,axis))));auto targetElbow=add(S,add(mul(axis,along),mul(bend,height)));
    changes.push_back({upper->typed_id,rotated(upper,arc(sub(E,S),sub(targetElbow,S)))});
   }else if(step==1)changes.push_back({elbow->typed_id,rotated(elbow,arc(sub(H,E),sub(T,E)))});
   else {auto parent=ufbx_matrix_to_transform(&hand->parent->node_to_world).rotation;parent={-parent.x,-parent.y,-parent.z,parent.w};auto tr=hand->local_transform;double angle=yaw;ufbx_quat turn{0,std::sin(angle*.5),0,std::cos(angle*.5)},tilt{std::sin(pitch*.5),0,0,std::cos(pitch*.5)};tr.rotation=ufbx_quat_mul(parent,ufbx_quat_mul(turn,ufbx_quat_mul(tilt,handRot[side])));changes.push_back({hand->typed_id,tr});}
  }
  for(auto n:m_scene->nodes)if(n->bone&&std::none_of(changes.begin(),changes.end(),[&](auto&c){return c.node_id==n->typed_id;}))changes.push_back({n->typed_id,n->local_transform});
  ufbx_anim_opts ao{};ao.transform_overrides={changes.data(),changes.size()};auto anim=ufbx_create_anim(m_scene,&ao,nullptr);ufbx_evaluate_opts eo{};eo.evaluate_skinning=skin&&step==2;auto posed=ufbx_evaluate_scene(m_scene,anim,0,&eo,nullptr);ufbx_free_anim(anim);
  if(!posed)throw std::runtime_error("Arm IK evaluation failed");ufbx_free_scene(m_scene);m_scene=posed;
 }
}
void Mesh::poseAttached(Point3 right,Point3 left,float elbowSwing,float pitch,float yaw,float phase,float recoil){
 if(!m_bindScene)return;
 ufbx_free_scene(m_scene);m_scene=m_bindScene;ufbx_retain_scene(m_scene);
 // IK needs bone transforms only. The final finger/recoil pose skins all
 // vertices once, avoiding the redundant skin pass at the end of IK.
 grip(right,left,elbowSwing,pitch,yaw,false);pose(phase,recoil);
}
bool Mesh::poseAction(const char* action,float phase){
 if(!m_bindScene)return false;
 for(auto stack:m_bindScene->anim_stacks){std::string name=stack->name.data;
  if(name.ends_with(action)){ufbx_evaluate_opts opts{};opts.evaluate_skinning=!m_positionOnlySkinning;
   auto frame=ufbx_evaluate_scene(m_bindScene,stack->anim,stack->time_begin+(stack->time_end-stack->time_begin)*std::clamp(phase,0.f,1.f),&opts,nullptr);
   if(!frame)return false;ufbx_free_scene(m_scene);m_scene=frame;extract(frame,m_positionOnlySkinning);return true;
  }
 }return false;
}
__declspec(noinline) bool Mesh::testAttachedSkinning(){
 Mesh optimized(111),reference(111);
 reference.m_positionOnlySkinning=false;
 for(int frame=0;frame<6;++frame){
  Point3 right{-.025f+frame*.006f,1.55f,.223f},left{-.025f,1.60f,.49f-frame*.004f};
  float elbow=frame*.02f,pitch=frame*.015f,yaw=-frame*.025f,phase=frame*.31f,recoil=frame*.04f;
  optimized.poseAttached(right,left,elbow,pitch,yaw,phase,recoil);
  ufbx_free_scene(reference.m_scene);reference.m_scene=reference.m_bindScene;ufbx_retain_scene(reference.m_scene);
  reference.grip(right,left,elbow,pitch,yaw,true);reference.pose(phase,recoil);
  if(optimized.triangles.size()!=reference.triangles.size())return false;
  for(size_t i=0;i<optimized.triangles.size();++i)for(int v=0;v<3;++v){
   const auto&a=optimized.triangles[i].v[v];const auto&b=reference.triangles[i].v[v];
   if(a.p.x!=b.p.x||a.p.y!=b.p.y||a.p.z!=b.p.z||a.u!=b.u||a.v!=b.v)return false;
  }
  for(const char*name:{"hand.R","hand.L"}){auto a=optimized.bonePosition(name),b=reference.bonePosition(name);if(a.x!=b.x||a.y!=b.y||a.z!=b.z)return false;}
 }
 for(const char*action:{"guard_idle","jab.L","jab.R"})for(float phase:{0.f,.17f,.37f,.79f,1.f}){
  if(!optimized.poseAction(action,phase)||!reference.poseAction(action,phase)||optimized.triangles.size()!=reference.triangles.size())return false;
  for(size_t i=0;i<optimized.triangles.size();++i)for(int v=0;v<3;++v){const auto&a=optimized.triangles[i].v[v];const auto&b=reference.triangles[i].v[v];
   if(a.p.x!=b.p.x||a.p.y!=b.p.y||a.p.z!=b.p.z||a.u!=b.u||a.v!=b.v)return false;}
  for(const char*name:{"hand.R","hand.L"}){auto a=optimized.bonePosition(name),b=reference.bonePosition(name);if(a.x!=b.x||a.y!=b.y||a.z!=b.z)return false;}
 }
 return true;
}
void Mesh::extract(ufbx_scene* scene,bool positionsOnly){
 // Animation changes positions, not topology, UVs, materials or boundary
 // connectivity. Triangulate and discover shoulder rings only once per asset.
 if(m_topologyReady){
  if(positionsOnly){m_skinPositionScratch.resize(scene->meshes.count);m_skinPositionReady.assign(scene->meshes.count,0);}
  auto position=[&](uint32_t nodeId,uint32_t corner){auto node=scene->nodes[nodeId];auto*mesh=node->mesh;
   ufbx_vec3 p;bool local=mesh->skinned_is_local;
   if(positionsOnly&&(mesh->skin_deformers.count||mesh->blend_deformers.count)){
    auto&vertices=m_skinPositionScratch[mesh->typed_id];
    if(!m_skinPositionReady[mesh->typed_id]){
     vertices.resize(mesh->num_vertices);std::memcpy(vertices.data(),mesh->vertices.data,vertices.size()*sizeof(ufbx_vec3));
     for(auto*blend:mesh->blend_deformers)ufbx_add_blend_vertex_offsets(blend,vertices.data(),vertices.size(),1.0f);
     if(mesh->skin_deformers.count){const auto*fallback=mesh->instances.count?&mesh->instances.data[0]->geometry_to_world:nullptr;auto*skin=mesh->skin_deformers.data[0];
      for(size_t i=0;i<vertices.size();++i){auto matrix=ufbx_get_skin_vertex_matrix(skin,i,fallback);vertices[i]=ufbx_transform_position(&matrix,vertices[i]);}}
     m_skinPositionReady[mesh->typed_id]=1;
    }
    p=vertices[mesh->skinned_position.indices.data[corner]];local=mesh->skin_deformers.count==0;
   }else p=ufbx_get_vertex_vec3(&mesh->skinned_position,corner);
   if(local)p=ufbx_transform_position(&node->geometry_to_world,p);return Point3{float(p.x),float(p.y),float(p.z)};
  };
  triangles.resize(m_cachedTriangles.size());for(size_t i=0;i<triangles.size();++i){auto&source=m_cachedTriangles[i];triangles[i]=source.prototype;for(int c=0;c<3;++c)triangles[i].v[c].p=position(source.node,source.corners[c]);}
  openRings.resize(m_cachedRings.size());for(size_t i=0;i<openRings.size();++i){auto&source=m_cachedRings[i];auto&ring=openRings[i];ring.resize(source.corners.size());for(size_t j=0;j<ring.size();++j)ring[j]={position(source.node,source.corners[j]),0,0};}
  return;
 }
 triangles.clear();
 openRings.clear();
 for(auto node:scene->nodes){auto mesh=node->mesh;if(!mesh||(!m_nodeFilter.empty()&&m_nodeFilter!=node->name.data))continue;
  // Detect actual open mesh boundaries, independent of UV seam duplication.
  if(std::string(node->name.data)=="ArmsMesh"){
   struct Edge{uint32_t a,b,faceA,faceB;int count;};std::map<std::pair<uint32_t,uint32_t>,Edge> edges;
   for(auto face:mesh->faces)for(uint32_t i=0;i<face.num_indices;++i){uint32_t ia=face.index_begin+i,ib=face.index_begin+(i+1)%face.num_indices;
    uint32_t a=mesh->vertex_indices.data[ia],b=mesh->vertex_indices.data[ib];auto key=std::minmax(a,b);auto&edge=edges[{key.first,key.second}];edge={a,b,ia,ib,edge.count+1};
   }
   std::map<uint32_t,uint32_t> next,corner;
   for(auto&entry:edges){auto&e=entry.second;if(e.count==1){next[e.a]=e.b;corner[e.a]=e.faceA;corner[e.b]=e.faceB;}}
   while(!next.empty()){std::vector<MeshVertex> ring;CachedRing cached{node->typed_id,{}};auto start=next.begin()->first,current=start;bool closed=false;
    for(size_t i=0;i<mesh->num_vertices;++i){auto it=next.find(current);if(it==next.end())break;
     auto ix=corner[current];auto p=ufbx_get_vertex_vec3(&mesh->skinned_position,ix);if(mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);
     ring.push_back({{float(p.x),float(p.y),float(p.z)},0,0});cached.corners.push_back(ix);current=it->second;next.erase(it);if(current==start){closed=true;break;}
    }
    if(closed&&ring.size()>=3){openRings.push_back(ring);m_cachedRings.push_back(std::move(cached));}
   }
  }
  std::vector<uint32_t> indices(mesh->max_face_triangles*3);
  for(size_t faceIndex=0;faceIndex<mesh->faces.count;++faceIndex){auto face=mesh->faces[faceIndex];auto count=ufbx_triangulate_face(indices.data(),indices.size(),mesh,face);
   for(uint32_t t=0;t<count;++t){MeshTriangle triangle;std::string partName=node->name.data;triangle.part=partName.find("Wing")!=std::string::npos?1:partName=="Bolt"?2:partName=="ShotgunBullet"?3:0;
    if(m_materialParts&&mesh->face_material.count>faceIndex){auto materialIndex=mesh->face_material[faceIndex];if(materialIndex<node->materials.count){std::string materialName=node->materials[materialIndex]->name.data;auto found=std::find(materialNames.begin(),materialNames.end(),materialName);triangle.part=int(found-materialNames.begin());}}
    for(int c=0;c<3;++c){uint32_t ix=indices[t*3+c];auto p=ufbx_get_vertex_vec3(&mesh->skinned_position,ix);
     if(mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);
     ufbx_vec2 uv{};if(mesh->vertex_uv.exists)uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,ix);
     triangle.v[c]={{float(p.x),float(p.y),float(p.z)},float(uv.x),1.f-float(uv.y)};
    }triangles.push_back(triangle);m_cachedTriangles.push_back({node->typed_id,{indices[t*3],indices[t*3+1],indices[t*3+2]},triangle});
   }
  }
 }
 m_topologyReady=true;
}
void Mesh::pose(float phase,float recoil){
 if(!bones)return;
 std::vector<ufbx_transform_override> overrides;
 for(auto node:m_scene->nodes){if(!node->bone)continue;
  std::string name=node->name.data;
  bool finger=name.find("f_")==0&&name.find("_end")==std::string::npos;
  auto tr=node->local_transform;
  auto authored=std::find_if(m_fingerGrip.begin(),m_fingerGrip.end(),[&](auto&t){return t.node_id==node->typed_id;});
  if(authored!=m_fingerGrip.end()){
   // Grab endpoints are closed fists. Blend each joint toward a firearm grasp,
   // preserving a relaxed support palm and a distinct trigger finger.
   bool support=name.find(".L")!=std::string::npos;
   float curl=support?.55f:.60f;
   if(support&&name.find(".02.")!=std::string::npos)curl=.85f;
   if(support&&name.find(".03.")!=std::string::npos)curl=.90f;
   if(name.find("f_index")==0&&!support)curl=.20f;
   if(name.find("thumb.")==0)curl=support?.60f:.56f;
   if(!support&&name.find(".03.")!=std::string::npos)curl*=.7f;
   tr.rotation=ufbx_quat_slerp(tr.rotation,authored->transform.rotation,curl);
  }
  const double angle=finger?((authored==m_fingerGrip.end()?.8:0)+std::sin(phase)*.008+recoil*.015):0;
  ufbx_quat q{std::sin(angle*.5),0,0,std::cos(angle*.5)};
  auto r=tr.rotation;
  tr.rotation={q.w*r.x+q.x*r.w,q.w*r.y-q.x*r.z,q.w*r.z+q.x*r.y,q.w*r.w-q.x*r.x};
  overrides.push_back({node->typed_id,tr});
 }
 ufbx_anim_opts animOpts{};animOpts.transform_overrides={overrides.data(),overrides.size()};
 auto anim=ufbx_create_anim(m_scene,&animOpts,nullptr);if(!anim)throw std::runtime_error("Rig animation creation failed");
 ufbx_evaluate_opts opts{};opts.evaluate_skinning=!m_positionOnlySkinning;
 auto evaluated=ufbx_evaluate_scene(m_scene,anim,0,&opts,nullptr);
 if(evaluated){extract(evaluated,m_positionOnlySkinning);ufbx_free_scene(evaluated);}ufbx_free_anim(anim);
}
}
