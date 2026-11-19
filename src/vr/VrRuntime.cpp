#include "VrRuntime.h"
#include "../ThirdParty/openvr/openvr.h"
#include "../renderer/SoftwareRenderer.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <set>
namespace retro {
namespace {
VrRuntime* current=nullptr;
using Matrix=std::array<float,16>;
Matrix multiply(const Matrix&a,const Matrix&b){Matrix c{};for(int r=0;r<4;++r)for(int col=0;col<4;++col)for(int k=0;k<4;++k)c[col*4+r]+=a[k*4+r]*b[col*4+k];return c;}
// Tracking space (x right, y up, -z ahead; right-handed) into game space (x, y the map, z up). The map is a
// mirror: its determinant is -1, so a frame is built in tracking space and only then mapped.
Point3 rotateTracking(Point3 p,float yaw){float c=std::cos(yaw),s=std::sin(yaw);return {-p.z*c-p.x*s,-p.z*s+p.x*c,p.y};}
Matrix rigidInverse(const Matrix&a){Matrix b{};b[15]=1;for(int r=0;r<3;++r)for(int c=0;c<3;++c)b[c*4+r]=a[r*4+c];for(int r=0;r<3;++r)for(int k=0;k<3;++k)b[12+r]-=b[k*4+r]*a[12+k];return b;}
Point3 translation(const vr::HmdMatrix34_t&p){return {p.m[0][3],p.m[1][3],p.m[2][3]};}
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross3(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float len3(Point3 v){return std::sqrt(dot3(v,v));}
Point3 unit3(Point3 v){const float l=len3(v);return l>1e-6f?v*(1/l):Point3{};}
float wrapYaw(float a){while(a>kPi)a-=2*kPi;while(a<-kPi)a+=2*kPi;return a;}
// The shortest turn taking unit a onto unit b, applied to v (Rodrigues). Antiparallel: a half turn about an axis
// square to a.
Point3 turnVector(Point3 v,Point3 a,Point3 b){
 const float c=dot3(a,b);Point3 k=cross3(a,b);const float s=len3(k);
 if(s<1e-6f){if(c>0)return v;const Point3 t=unit3(std::fabs(a.x)<.8f?cross3(a,{1,0,0}):cross3(a,{0,1,0}));return t*(2*dot3(t,v))-v;}
 k=k*(1/s);return v*c+cross3(k,v)*s+k*(dot3(k,v)*(1-c));}
// The fore-end in the gun frame until the renderer measures the drawn shotgun's pump (setForeEnd).
Point3 g_foreEnd{0,-.012f,-.36f};float g_foreEndHalf=.09f;
bool transientCompositor(vr::EVRCompositorError error){
 return error==vr::VRCompositorError_DoNotHaveFocus||error==vr::VRCompositorError_IsNotSceneApplication||error==vr::VRCompositorError_AlreadySubmitted;
}
void logCompositor(vr::EVRCompositorError error){
 static std::set<int> logged;if(logged.insert(int(error)).second){auto message="RawMetal: SteamVR compositor frame suspended ("+std::to_string(error)+").\n";OutputDebugStringA(message.c_str());std::ofstream("vr-compositor-errors.txt",std::ios::app)<<message;}
}
bool handFrame(Point3 forward,Point3 across,int side,std::array<Point3,3>& frame){
 auto dot=[](Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 auto unit=[&](Point3 v){float l=std::sqrt(dot(v,v));return l>1e-6f?v*(1/l):Point3{};};
 auto cross=[](Point3 a,Point3 b){return Point3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
 forward=unit(forward);if(dot(forward,forward)<.5f)return false;
 // Missing or collinear knuckles need an axis independent of the finger ray.
 across=unit(across-forward*dot(across,forward));
 if(dot(across,across)<.5f){Point3 fallback=std::fabs(forward.x)<.8f?Point3{1,0,0}:Point3{0,1,0};across=unit(fallback-forward*dot(fallback,forward));}
 auto palm=unit(cross(across,forward)*(side==0?-1.f:1.f));across=unit(cross(forward,palm));frame={across,palm,forward};return true;
}
float axis(float value){float a=std::fabs(value);return a<.18f?0:std::copysign(std::min(1.f,(a-.18f)/.82f),value);}
}
struct VrRuntime::Impl {
 HMODULE dll=nullptr;void(__cdecl* shutdown)()=nullptr;vr::IVRSystem* system=nullptr;vr::IVRCompositor* compositor=nullptr;vr::IVRInput* input=nullptr;
 std::array<vr::VRActionHandle_t,13> actions{};bool crouchToggled=false,previousCrouch=false;int framesSeen=0;
 // SteamVR's grip pose: the same hand frame on every controller (the legacy
 // device pose is pitched differently on Touch, Index and Vive controllers).
 vr::VRActionHandle_t grip[2]{};std::array<vr::HmdMatrix34_t,2> raw{},gripPose{};std::array<bool,2> gripValid{};
 // Roles: hands[] and track[] are by role (1 the gun hand, 0 the other); side[role] is the physical hand
 // (0 left, 1 right): swapped when left-handed. SteamVR's devices, skeletons and controller states stay physical.
 std::array<int,2> side{0,1};bool leftHanded=false,seated=false,support=false;float pump=0;
 int roleOf(int physical)const{return side[0]==physical?0:1;}
 // Each role's hand in tracking space (right-handed), as sampleHand built it: the frames are made here and only
 // mapped into the mirrored game space afterwards.
 struct Track{bool valid=false;Point3 wrist{},across{},palm{},forward{},grip{},right{},up{},back{};};
 std::array<Track,2> track{};
 bool sampleGrip(int physical,vr::HmdMatrix34_t& pose,vr::HmdVector3_t* velocity){
  if(!input||!grip[physical])return false;vr::InputPoseActionData_t data{};
  if(input->GetPoseActionDataForNextFrame(grip[physical],vr::TrackingUniverseStanding,&data,sizeof(data),vr::k_ulInvalidInputValueHandle)!=vr::VRInputError_None||!data.bActive||!data.pose.bPoseIsValid)return false;
  pose=data.pose.mDeviceToAbsoluteTracking;if(velocity)*velocity=data.pose.vVelocity;return true;
 }
 // The hand's frame from SteamVR's skeleton on whatever controller is held:
 // wrist, index, middle and pinky knuckles, built exactly as Mesh::trackedHand
 // builds the mesh frame, so the drawn hand sits where the real one is.
 bool sampleHand(int role,VrHand& h,const Game& game){
  const int physical=side[size_t(role)];h.snapped=false;track[size_t(role)].valid=false;
  if(!input||!skeleton[physical])return false;
  vr::InputPoseActionData_t root{};
  if(input->GetPoseActionDataForNextFrame(skeleton[physical],vr::TrackingUniverseStanding,&root,sizeof(root),vr::k_ulInvalidInputValueHandle)!=vr::VRInputError_None||!root.bActive||!root.pose.bPoseIsValid)return false;
  vr::VRBoneTransform_t bones[31]{};
  if(input->GetSkeletalBoneData(skeleton[physical],vr::VRSkeletalTransformSpace_Model,vr::VRSkeletalMotionRange_WithController,bones,31)!=vr::VRInputError_None)return false;
  const auto& p=root.pose.mDeviceToAbsoluteTracking;
  // Joints in tracking space, which is right-handed like the mesh's own space:
  // the frame is built there, then carried into the (mirrored) game space.
  auto joint=[&](int bone){const auto& b=bones[bone].position.v;return Point3{p.m[0][0]*b[0]+p.m[0][1]*b[1]+p.m[0][2]*b[2]+p.m[0][3],p.m[1][0]*b[0]+p.m[1][1]*b[1]+p.m[1][2]*b[2]+p.m[1][3],p.m[2][0]*b[0]+p.m[2][1]*b[1]+p.m[2][2]*b[2]+p.m[2][3]};};
  const Point3 wrist=joint(1),index=joint(7),middle=joint(12),pinky=joint(22);
  std::array<Point3,3> frame{};if(!handFrame(middle-wrist,index-pinky,physical,frame))return false;
  auto across=frame[0],palm=frame[1],forward=frame[2];
  h.handOrigin=world(wrist,game);h.handBasis={rotateTracking(across,yaw),rotateTracking(palm,yaw),rotateTracking(forward*-1.f,yaw)};h.handFrame=true;
  // The weapon: its grip through the fist (index side up), the barrel along the
  // knuckles, anchored in the palm. Built in tracking space like the hand, so the
  // aim and the model agree with the hand the player sees.
  const Point3 back=forward*-1.f,right=unit3(cross3(across,back)),up=unit3(cross3(back,right));
  // The mesh frame's +y is the palm side (the closed fist spans y -0.03 to
  // +0.06): the grip goes through the fist, 7.5 cm along and 3 cm into the palm.
  const Point3 grip=wrist+forward*GripAlong+palm*GripIn;
  h.gunOrigin=world(grip,game);h.gunBasis={rotateTracking(right,yaw),rotateTracking(up,yaw),rotateTracking(back,yaw)};
  track[size_t(role)]={true,wrist,across,palm,forward,grip,right,up,back};
  return true;
 }
 // The support hand on the fore-end (supportPose, in tracking space), then mapped: the gun frame aimed through
 // the fist, and the hand drawn round the fore-end.
 void applySupport(const Game& game){
  const auto&g=track[1],&o=track[0];if(!g.valid||!o.valid)return;
  const auto s=supportPose(g.grip,g.right,g.up,o.wrist,o.across,o.palm,o.forward,g_foreEnd,g_foreEndHalf,pump,side[0]==0);
  auto&gun=hands[1];gun.gunBasis={rotateTracking(s.gunRight,yaw),rotateTracking(s.gunUp,yaw),rotateTracking(s.gunBack,yaw)};
  auto&h=hands[0];h.handOrigin=world(s.wrist,game);h.handBasis={rotateTracking(s.across,yaw),rotateTracking(s.palm,yaw),rotateTracking(s.forward*-1.f,yaw)};h.snapped=true;
 }
 vr::VRActionSetHandle_t set=0;vr::VRActionHandle_t skeleton[2]{};vr::IVROverlay* overlay=nullptr;vr::VROverlayHandle_t panel=0;bool titlePanel=false;
 std::array<vr::TrackedDevicePose_t,vr::k_unMaxTrackedDeviceCount> poses{};
 std::array<VrHand,2> hands{};uint32_t width=0,height=0;float yaw=0,headStartY=0;Point3 previousHead{};bool calibrated=false,trackingInterrupted=false,menuWasOpen=false;int pointerX=-1,pointerY=-1;
 // The gun hand's grip, held from the last frame; a draw from a holster, and whether the hand has left it since
 // (a release back at the holster puts the gun away only then).
 std::array<bool,2> previousGrip{},consumed{};int drawnFrom=0;bool leftHolster=false;
 // Tracking metres to game metres (avatarScale), the hips' facing (lagging the head), the comfort vignette.
 float scale=1,bodyYaw=0,turnRate=0,vignette=0;bool bodyReady=false;
 VrTiming timing;int timingFrames=0;float displayHz=0;
 Point3 world(Point3 point,const Game&game)const{auto head=translation(poses[0].mDeviceToAbsoluteTracking);auto offset=rotateTracking(point-head,yaw)*scale;auto origin=game.chunkOffset(game.level());return offset+Point3{game.player().pos.x+origin.x,game.player().pos.y+origin.y,game.player().z+game.player().eye};}
 ~Impl(){if(overlay&&panel)overlay->DestroyOverlay(panel);if(shutdown&&system)shutdown();if(dll)FreeLibrary(dll);}
};
VrRuntime::VrRuntime():m(std::make_unique<Impl>()){}
VrRuntime::~VrRuntime(){if(current==this)current=nullptr;}
VrRuntime* VrRuntime::active(){return current;}
bool VrRuntime::headsetPresent(){
 wchar_t steam[32768]{};DWORD bytes=sizeof(steam);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,steam,&bytes)!=ERROR_SUCCESS)return false;
 auto path=std::filesystem::path(steam)/L"steamapps/common/SteamVR/bin/win64/openvr_api.dll";if(!std::filesystem::exists(path))return false;
 HMODULE dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll)return false;
 auto present=reinterpret_cast<bool(__cdecl*)()>(GetProcAddress(dll,"VR_IsHmdPresent"));bool found=present&&present();FreeLibrary(dll);return found;
}
void VrRuntime::shutdownGraphics(){if(m->overlay&&m->panel){m->overlay->DestroyOverlay(m->panel);m->panel=0;}if(m->shutdown&&m->system)m->shutdown();m->system=nullptr;m->compositor=nullptr;m->input=nullptr;m->overlay=nullptr;if(current==this)current=nullptr;}
bool VrRuntime::initialize(std::string& error){
 try{
  // Load only the Steam installation's runtime, never an arbitrary current-directory DLL.
  wchar_t steam[32768]{};DWORD bytes=sizeof(steam);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,steam,&bytes)!=ERROR_SUCCESS)throw std::runtime_error("Steam installation was not found.");
  auto path=std::filesystem::path(steam)/L"steamapps/common/SteamVR/bin/win64/openvr_api.dll";
  m->dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!m->dll)throw std::runtime_error("Install SteamVR in the Steam library, then retry VR mode.");
  auto init=reinterpret_cast<uint32_t(__cdecl*)(vr::EVRInitError*,vr::EVRApplicationType,const char*)>(GetProcAddress(m->dll,"VR_InitInternal2"));
  auto get=reinterpret_cast<void*(__cdecl*)(const char*,vr::EVRInitError*)>(GetProcAddress(m->dll,"VR_GetGenericInterface"));
  m->shutdown=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(m->dll,"VR_ShutdownInternal"));
  if(!init||!get||!m->shutdown)throw std::runtime_error("SteamVR runtime exports are unavailable.");
  vr::EVRInitError result=vr::VRInitError_None;init(&result,vr::VRApplication_Scene,nullptr);if(result!=vr::VRInitError_None)throw std::runtime_error("SteamVR could not start the headset (error "+std::to_string(result)+"). Start SteamVR and check the headset connection.");
  m->system=static_cast<vr::IVRSystem*>(get(vr::IVRSystem_Version,&result));m->compositor=static_cast<vr::IVRCompositor*>(get(vr::IVRCompositor_Version,&result));m->input=static_cast<vr::IVRInput*>(get(vr::IVRInput_Version,&result));
  if(!m->system||!m->compositor)throw std::runtime_error("SteamVR system/compositor interface is unavailable. Update SteamVR.");
  m->overlay=static_cast<vr::IVROverlay*>(get(vr::IVROverlay_Version,&result));
  if(!m->overlay||m->overlay->CreateOverlay("rawmetal.menu","Depthworks",&m->panel)!=vr::VROverlayError_None)throw std::runtime_error("SteamVR menu overlay is unavailable.");
  vr::HmdMatrix34_t panelPose{};panelPose.m[0][0]=panelPose.m[1][1]=panelPose.m[2][2]=1;panelPose.m[2][3]=-2.f;
  m->overlay->SetOverlayTransformTrackedDeviceRelative(m->panel,vr::k_unTrackedDeviceIndex_Hmd,&panelPose);m->overlay->SetOverlayWidthInMeters(m->panel,1.4f);m->overlay->ShowOverlay(m->panel);
  m->compositor->SetTrackingSpace(vr::TrackingUniverseStanding);m->system->GetRecommendedRenderTargetSize(&m->width,&m->height);
  if(!m->width||!m->height||m->width>8192||m->height>8192)throw std::runtime_error("SteamVR returned an invalid eye render size.");
  m->displayHz=m->system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,vr::Prop_DisplayFrequency_Float);if(!(m->displayHz>30&&m->displayHz<200))m->displayHz=90;m->timing.hz=m->displayHz;
  if(m->input){wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);auto folder=std::filesystem::path(local)/L"RawMetal/vr";std::filesystem::create_directories(folder);
   const char* names[]={"triggerleft","triggerright","gripleft","gripright","squeezeleft","squeezeright","move","turn","jump","reload","menu","inventory","crouch"};
   std::ofstream manifestFile(folder/L"actions.json");manifestFile<<R"({"action_sets":[{"name":"/actions/game","usage":"leftright"}],"actions":[{"name":"/actions/game/in/left","type":"skeleton","skeleton":"/skeleton/hand/left"},{"name":"/actions/game/in/right","type":"skeleton","skeleton":"/skeleton/hand/right"})";
   for(int i=0;i<13;++i)manifestFile<<",{\"name\":\"/actions/game/in/"<<names[i]<<"\",\"type\":\""<<(i==6||i==7?"vector2":i==4||i==5?"vector1":"boolean")<<"\"}";
   manifestFile<<R"(],"default_bindings":[{"controller_type":"knuckles","binding_url":"knuckles-v2.json"},{"controller_type":"oculus_touch","binding_url":"oculus_touch-v2.json"},{"controller_type":"vive_controller","binding_url":"vive_controller-v2.json"}]})";manifestFile.close();
   for(const char*controller:{"knuckles","oculus_touch","vive_controller"}){
    bool vive=std::string(controller)=="vive_controller",index=std::string(controller)=="knuckles";std::ofstream file(folder/(std::string(controller)+"-v2.json"));
    file<<"{\"controller_type\":\""<<controller<<R"(","name":"RawMetal gameplay","bindings":{"/actions/game":{"skeleton":[{"output":"/actions/game/in/left","path":"/user/hand/left/input/skeleton/left"},{"output":"/actions/game/in/right","path":"/user/hand/right/input/skeleton/right"}],"sources":[)";
    bool first=true;auto source=[&](const char*hand,const char*control,const char*mode,const char*slot,const char*action){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/"<<hand<<"/input/"<<control<<"\",\"mode\":\""<<mode<<"\",\"inputs\":{\""<<slot<<"\":{\"output\":\"/actions/game/in/"<<action<<"\"}}}";};
    for(int side=0;side<2;++side){const char*hand=side?"right":"left";source(hand,"trigger","button","click",names[side]);source(hand,"grip",vive?"button":index?"force_sensor":"trigger",vive?"click":index?"force":"pull",names[(vive?2:4)+side]);if(side&&!vive){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/right/input/"<<(index?"thumbstick":"joystick")<<"\",\"mode\":\"joystick\",\"inputs\":{\"position\":{\"output\":\"/actions/game/in/turn\"},\"click\":{\"output\":\"/actions/game/in/inventory\"}}}";}
     else source(hand,vive?"trackpad":index?"thumbstick":"joystick",vive?"trackpad":"joystick","position",side?"turn":"move");}
    if(vive){source("left","application_menu","button","click","menu");source("right","application_menu","button","click","reload");
     // Touch supplies locomotion; clicks select discrete cardinal actions.
     auto dpad=[&](const char*hand,const char*north,const char*south){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/"<<hand<<"/input/trackpad\",\"mode\":\"dpad\",\"parameters\":{\"sub_mode\":\"click\"},\"inputs\":{\"north\":{\"output\":\"/actions/game/in/"<<north<<"\"},\"south\":{\"output\":\"/actions/game/in/"<<south<<"\"}}}";};
     dpad("left","crouch","inventory");dpad("right","jump","inventory");}else{source("right","a","button","click","jump");source("right","b","button","click","reload");source("left",index?"a":"x","button","click","crouch");source("left",index?"b":"y","button","click","menu");}
    file<<"]}}}";
   }
   auto manifest=(folder/L"actions.json").string();if(m->input->SetActionManifestPath(manifest.c_str())==vr::VRInputError_None){m->input->GetActionSetHandle("/actions/game",&m->set);for(int i=0;i<13;++i)m->input->GetActionHandle((std::string("/actions/game/in/")+names[i]).c_str(),&m->actions[i]);m->input->GetActionHandle("/actions/game/in/left",&m->skeleton[0]);m->input->GetActionHandle("/actions/game/in/right",&m->skeleton[1]);}
  }
  current=this;return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
std::pair<int,int> VrRuntime::extent()const{return {int(m->width),int(m->height)};}
bool VrRuntime::titlePanel()const{return m->titlePanel;}
std::string VrRuntime::poseReport()const{
 std::ostringstream out;out.precision(2);out<<std::fixed;auto basis=[&](const char* name,const vr::HmdMatrix34_t& p){out<<name<<" x("<<p.m[0][0]<<','<<p.m[1][0]<<','<<p.m[2][0]<<") y("<<p.m[0][1]<<','<<p.m[1][1]<<','<<p.m[2][1]<<") z("<<p.m[0][2]<<','<<p.m[1][2]<<','<<p.m[2][2]<<")";};
 basis("head",m->poses[0].mDeviceToAbsoluteTracking);out<<'\n';
 for(int side=0;side<2;++side){out<<(side?"right ":"left ");basis("raw",m->raw[side]);out<<"  ";if(m->gripValid[side])basis("grip",m->gripPose[side]);else out<<"grip unavailable";out<<'\n';}
 out<<"avatar scale "<<m->scale<<", calibrated eye "<<m->headStartY<<" m, body yaw "<<m->bodyYaw<<", "<<(m->leftHanded?"left-handed":"right-handed")<<(m->seated?", seated":"")<<(m->support?", support hand on the fore-end":"")<<'\n';
 return out.str();
}
bool VrRuntime::trackingValid()const{return m->poses[0].bPoseIsValid;}
std::pair<int,int> VrRuntime::pointer()const{return {m->pointerX,m->pointerY};}
const std::array<VrHand,2>& VrRuntime::hands()const{return m->hands;}
const VrTiming& VrRuntime::timing()const{return m->timing;}
float VrRuntime::vignette()const{return m->vignette;}
std::string VrRuntime::instanceExtensions()const{std::string s(m->compositor->GetVulkanInstanceExtensionsRequired(nullptr,0),'\0');m->compositor->GetVulkanInstanceExtensionsRequired(s.data(),uint32_t(s.size()));return s;}
std::string VrRuntime::deviceExtensions(VkPhysicalDevice device)const{std::string s(m->compositor->GetVulkanDeviceExtensionsRequired(device,nullptr,0),'\0');m->compositor->GetVulkanDeviceExtensionsRequired(device,s.data(),uint32_t(s.size()));return s;}
VkPhysicalDevice VrRuntime::outputDevice(VkInstance instance)const{uint64_t device=0;m->system->GetOutputDevice(&device,vr::TextureType_Vulkan,instance);return reinterpret_cast<VkPhysicalDevice>(device);}
void VrRuntime::suspendTracking(Game&game,InputState&input){
  // Keep tracked movement enabled but neutral: PC keys and stale controller
  // actions must not move the player while the headset pose is unavailable.
  input.vrTracked=true;input.vrYaw=game.player().angle;input.vrPitch=game.player().pitch;input.vrEye=game.player().eye;
  input.vrMoveForward=input.vrMoveRight=0;input.vrGrip={};input.fire=input.use=input.jump=input.reload=false;
  input.pointerX=input.pointerY=m->pointerX=m->pointerY=-1;
  m->previousGrip={};m->consumed={};m->previousCrouch=false;m->drawnFrom=0;m->turnRate=0;m->vignette=0;m->support=false;
  for(auto&h:m->hands)h={};for(auto&t:m->track)t.valid=false;game.setVrAim({}, {},false);game.setVrHand(0,{}, {},false);game.setVrHand(1,{}, {},false);
  // Resume at the recovered tracking origin rather than applying the whole
  // unobserved head displacement as a room-scale collision move.
  m->trackingInterrupted=true;
}
float VrRuntime::avatarScale(float calibratedEye,bool seated){return std::clamp(Player::StandingEye/(seated?1.62f:std::clamp(calibratedEye,1.f,2.1f)),.7f,1.25f);}
float VrRuntime::nextRenderScale(float scale,float gpuMs,float budgetMs){
 if(!(gpuMs>0)||!(budgetMs>0))return std::clamp(scale,.6f,1.f);
 if(gpuMs>budgetMs*.9f)scale*=.95f;else if(gpuMs<budgetMs*.7f)scale*=1.025f;
 return std::clamp(scale,.6f,1.f);}
bool VrRuntime::update(Game&game,InputState&input,float dt){
 vr::VREvent_t event{};while(m->system->PollNextEvent(&event,sizeof(event)))if(event.eventType==vr::VREvent_Quit)return false;
 auto result=m->compositor->WaitGetPoses(m->poses.data(),uint32_t(m->poses.size()),nullptr,0);
 if(result!=vr::VRCompositorError_None){logCompositor(result);m->poses[0].bPoseIsValid=false;suspendTracking(game,input);if(transientCompositor(result)||m->system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd))return true;throw std::runtime_error("SteamVR tracking/compositor failed ("+std::to_string(result)+").");}
 if(!m->compositor->CanRenderScene()||!m->poses[0].bPoseIsValid){m->poses[0].bPoseIsValid=false;suspendTracking(game,input);return true;}
 const auto&cfg=game.settings();
 m->leftHanded=cfg.vrLeftHanded;m->side=m->leftHanded?std::array<int,2>{1,0}:std::array<int,2>{0,1};m->seated=cfg.vrSeated;
 auto head=translation(m->poses[0].mDeviceToAbsoluteTracking);
 // The standing eye is the head's height when play starts or Recalibrate height is chosen; the player's
 // tracking is scaled to Claire (avatarScale) so their reach is her arm's.
 const bool recalibrate=game.takeVrRecalibrate();
 if(!m->calibrated){m->yaw=game.player().angle;m->previousHead=head;m->calibrated=true;m->bodyReady=false;}
 if(!m->calibrated||recalibrate||m->headStartY==0)m->headStartY=head.y;
 m->scale=avatarScale(m->headStartY,m->seated);
 if(m->trackingInterrupted){m->previousHead=head;m->trackingInterrupted=false;}
 if(!game.paused()&&!game.titleScreen()){auto delta=rotateTracking(head-m->previousHead,m->yaw)*m->scale;game.moveVrRoom({delta.x,delta.y});}m->previousHead=head;
 // Physical controllers (0 left, 1 right): devices, states and sticks stay physical; hands[] is by role.
 vr::VRControllerState_t states[2]{};uint32_t devices[2]={m->system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),m->system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand)};
 vr::VRControllerAxis_t sticks[2]{};
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];auto device=devices[side];auto&h=m->hands[size_t(role)];h={};h.left=side==0;
  if(device>=m->poses.size()||!m->poses[device].bPoseIsValid)continue;
  m->system->GetControllerState(device,&states[side],sizeof(states[side]));h.tracked=true;auto pose=m->poses[device].mDeviceToAbsoluteTracking;h.position=m->world(translation(pose),game);
  for(int i=0;i<3;++i)h.basis[i]=rotateTracking({pose.m[0][i],pose.m[1][i],pose.m[2][i]},m->yaw);
  for(int i=0;i<5;++i){int type=m->system->GetInt32TrackedDeviceProperty(device,vr::ETrackedDeviceProperty(vr::Prop_Axis0Type_Int32+i));if(type==vr::k_eControllerAxis_Joystick||(type==vr::k_eControllerAxis_TrackPad&&(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)))){sticks[side]=states[side].rAxis[i];break;}}
  float trigger=states[side].rAxis[1].x;h.curls.fill((states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))?.85f:.1f);h.curls[1]=std::clamp(trigger,0.f,1.f);
 }
 if(m->input&&m->set){vr::VRActiveActionSet_t set{};set.ulActionSet=m->set;m->input->UpdateActionState(&set,sizeof(set),1);for(int side=0;side<2;++side){auto&hand=m->hands[size_t(m->roleOf(side))];vr::InputSkeletalActionData_t data{};vr::VRSkeletalSummaryData_t summary{};if(m->input->GetSkeletalActionData(m->skeleton[side],&data,sizeof(data))==vr::VRInputError_None&&data.bActive&&m->input->GetSkeletalSummaryData(m->skeleton[side],vr::VRSummaryType_FromDevice,&summary)==vr::VRInputError_None){std::copy(summary.flFingerCurl,summary.flFingerCurl+5,hand.curls.begin());hand.articulated=true;}}}
 // Hand frames come from the grip pose wherever SteamVR provides one.
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];auto&h=m->hands[size_t(role)];if(!h.tracked)continue;m->raw[side]=m->poses[devices[side]].mDeviceToAbsoluteTracking;vr::HmdMatrix34_t pose{};m->gripValid[side]=m->sampleGrip(side,pose,nullptr);if(!m->gripValid[side])continue;
  m->gripPose[side]=pose;h.position=m->world(translation(pose),game);for(int i=0;i<3;++i)h.basis[i]=rotateTracking({pose.m[0][i],pose.m[1][i],pose.m[2][i]},m->yaw);}
 for(int role=0;role<2;++role)if(m->hands[size_t(role)].tracked)m->sampleHand(role,m->hands[size_t(role)],game);
 auto pressed=[&](int side,vr::EVRButtonId id){return (states[side].ulButtonPressed&vr::ButtonMaskFromId(id))!=0;};
 bool jumpButton=pressed(1,vr::k_EButton_A),reloadButton=pressed(1,vr::k_EButton_ApplicationMenu),crouchButton=pressed(0,vr::k_EButton_A),menuButton=pressed(0,vr::k_EButton_ApplicationMenu);
 if(m->input&&m->set){
  auto digital=[&](int action,bool fallback){vr::InputDigitalActionData_t data{};return m->input->GetDigitalActionData(m->actions[action],&data,sizeof(data),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&data.bActive?data.bState:fallback;};
  for(int side=0;side<2;++side){states[side].rAxis[1].x=digital(side,states[side].rAxis[1].x>.65f)?1.f:0.f;vr::InputAnalogActionData_t squeeze{};bool grip=digital(side+2,(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))!=0);if(m->input->GetAnalogActionData(m->actions[4+side],&squeeze,sizeof(squeeze),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&squeeze.bActive)grip|=squeeze.x>.65f;if(grip)states[side].ulButtonPressed|=vr::ButtonMaskFromId(vr::k_EButton_Grip);else states[side].ulButtonPressed&=~vr::ButtonMaskFromId(vr::k_EButton_Grip);vr::InputAnalogActionData_t move{};if(m->input->GetAnalogActionData(m->actions[6+side],&move,sizeof(move),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&move.bActive)sticks[side]={move.x,move.y};}
  // Once SteamVR has loaded the bindings, record what drives each button action.
  if(++m->framesSeen==180){wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);std::ofstream log(std::filesystem::path(local)/L"RawMetal/vr/bindings-active.txt");
   const char* labels[]={"jump","reload","menu","inventory","crouch"};const int ids[]={8,9,10,11,12};
   for(int k=0;k<5;++k){vr::VRInputValueHandle_t origins[vr::k_unMaxActionOriginCount]{};log<<labels[k]<<":";if(m->input->GetActionOrigins(m->set,m->actions[size_t(ids[k])],origins,vr::k_unMaxActionOriginCount)==vr::VRInputError_None)for(auto origin:origins){if(!origin)break;char name[128]{};m->input->GetOriginLocalizedName(origin,name,sizeof name,vr::VRInputString_All);log<<" ["<<name<<"]";}log<<'\n';}}
  jumpButton=digital(8,jumpButton);reloadButton=digital(9,reloadButton);crouchButton=digital(12,crouchButton);menuButton=digital(10,menuButton);input.inventory|=digital(11,false);
 }
 // SteamVR action state can outlive its tracked device. Discard it before
 // locomotion/menu/use processing, including the legacy input fallback.
 for(int side=0;side<2;++side){auto&hand=m->hands[size_t(m->roleOf(side))];if(!hand.tracked){states[side]={};sticks[side]={};}else if(!hand.articulated){hand.curls.fill((states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))?.85f:.1f);hand.curls[1]=std::clamp(states[side].rAxis[1].x,0.f,1.f);}}
 // The off hand's stick moves, the gun hand's stick turns (swapped when left-handed). Turning is smooth only,
 // at the chosen speed (1 = 120 degrees a second at full stick).
 const auto&moveStick=sticks[m->side[0]],&turnStick=sticks[m->side[1]];const int gunSide=m->side[1],offSide=m->side[0];
 bool focused=m->system->IsInputAvailable();const bool playing=focused&&!game.paused()&&!game.titleScreen();
 const float step=std::min(dt,.05f);m->turnRate=playing?std::copysign(std::pow(std::fabs(axis(turnStick.x)),1.4f),turnStick.x)*2.1f*cfg.vrTurnSpeed:0.f;
 m->yaw=wrapYaw(m->yaw+m->turnRate*step);
 auto&pose=m->poses[0].mDeviceToAbsoluteTracking;auto forward=rotateTracking({-pose.m[0][2],-pose.m[1][2],-pose.m[2][2]},m->yaw);
 input.vrTracked=true;input.vrYaw=std::atan2(forward.y,forward.x);input.vrPitch=140.f*std::asin(std::clamp(forward.z,-1.f,1.f));input.vrEye=std::clamp(Player::StandingEye+(head.y-m->headStartY)*m->scale,.15f,2.5f);
 input.mouseDx=input.mouseDy=0;input.vrMoveForward=focused?axis(moveStick.y):0;input.vrMoveRight=focused?axis(moveStick.x):0;
 // Ducking for real crouches her (the legs kneel); seated, only the crouch button does.
 input.crouch=!m->seated&&input.vrEye<1.f;
 bool desktopClick=input.fire,desktopAccept=input.menuAccept;
 const bool gunTrigger=states[gunSide].rAxis[1].x>.65f,offTrigger=states[offSide].rAxis[1].x>.65f;
 input.fire=focused&&m->hands[1].tracked&&gunTrigger;input.use=focused&&((states[0].ulButtonPressed|states[1].ulButtonPressed)&vr::ButtonMaskFromId(vr::k_EButton_Grip));input.jump=focused&&jumpButton;input.reload=focused&&reloadButton;input.menuAccept|=input.fire||(focused&&offTrigger);input.menuUp|=focused&&moveStick.y>.6f;input.menuDown|=focused&&moveStick.y<-.6f;input.escape|=focused&&menuButton;
 // The crouch button toggles a crouch that also lowers the view.
 if(focused&&crouchButton&&!m->previousCrouch)m->crouchToggled=!m->crouchToggled;m->previousCrouch=crouchButton;
 if(m->crouchToggled&&!game.paused()&&!game.titleScreen()){input.crouch=true;input.vrEye=std::max(.15f,input.vrEye-(Player::StandingEye-Player::CrouchedEye));}
 bool menuOpen=game.titleScreen()||game.paused()||game.inventoryOpen()||game.consoleOpen();
 // The hips' facing: it trails the head (VRChat's upper-body turn), following at once while walking and when the
 // head is more than 43 degrees off it, and the stick's turn carries it with the view.
 {const float headYaw=std::atan2(forward.y,forward.x);if(!m->bodyReady){m->bodyYaw=headYaw;m->bodyReady=true;}
  m->bodyYaw=wrapYaw(m->bodyYaw+m->turnRate*step);float d=wrapYaw(headYaw-m->bodyYaw);
  const bool walking=std::fabs(input.vrMoveForward)+std::fabs(input.vrMoveRight)>.2f;
  if(walking)m->bodyYaw=wrapYaw(m->bodyYaw+d*std::min(1.f,dt*6.f));else{if(std::fabs(d)>.75f)m->bodyYaw=wrapYaw(m->bodyYaw+d-std::copysign(.75f,d));d=wrapYaw(headYaw-m->bodyYaw);m->bodyYaw=wrapYaw(m->bodyYaw+d*std::min(1.f,dt*.4f));}}
 // Menus, inventory and terminals open on the floating panel.
 const bool panelOpen=menuOpen||game.logTime()>0;
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];bool grip=focused&&m->hands[size_t(role)].tracked&&(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip));input.vrGrip[size_t(role)]=grip;}
 // Holsters on her body (avatarSlotAt): the gun hand gripping over a shoulder draws the shotgun from her back,
 // at the hip the handgun. A drawn gun stays in the hand when the grip is let go; let go back at its own holster
 // (after leaving it) and it is put away there. A grip spent on a holster does not also grab or use.
 {const AvatarPose body=avatar(game);const Point3 at=body.hands[size_t(gunSide)].wrist;const bool grip=input.vrGrip[1],was=m->previousGrip[1];
  const int slot=m->hands[1].tracked?avatarSlotAt(body,at):0;const bool armed=game.weaponEquipped()&&!game.unarmed();
  const int heldSlot=!armed?0:game.equippedWeapon()==WeaponId::Shotgun?1:game.equippedWeapon()==WeaponId::Handgun?2:0;
  if(m->drawnFrom&&slot!=m->drawnFrom)m->leftHolster=true;
  if(grip&&!was&&!menuOpen&&!game.holdingClutter()&&slot){
   const WeaponId want=slot==1?WeaponId::Shotgun:WeaponId::Handgun;
   if(heldSlot==slot){m->drawnFrom=slot;m->leftHolster=true;m->consumed[1]=true;}   // gripping it at its own holster: let go to put it away
   else if(game.ownsWeapon(want)){input.weaponSlot=int(want)+1;m->drawnFrom=slot;m->leftHolster=false;m->consumed[1]=true;}}
  if(!grip&&was&&m->drawnFrom&&heldSlot==m->drawnFrom&&slot==m->drawnFrom&&m->leftHolster&&!menuOpen){input.weaponSlot=int(WeaponId::Fists)+1;m->drawnFrom=0;}
  if(!armed&&!grip)m->drawnFrom=0;
  for(int role=0;role<2;++role){if(!input.vrGrip[size_t(role)])m->consumed[size_t(role)]=false;if(m->consumed[size_t(role)]){input.vrGrip[size_t(role)]=false;input.use=input.vrGrip[0]||input.vrGrip[1];}m->previousGrip[size_t(role)]=focused&&m->hands[size_t(role)].tracked&&(states[m->side[size_t(role)]].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip));}}
 if(panelOpen&&!m->menuWasOpen){
  const auto&headPose=m->poses[0].mDeviceToAbsoluteTracking;float x=-headPose.m[0][2],z=-headPose.m[2][2];float length=std::hypot(x,z);if(length<.001f){x=0;z=-1;length=1;}x/=length;z/=length;
  vr::HmdMatrix34_t panelPose{};panelPose.m[0][0]=-z;panelPose.m[2][0]=x;panelPose.m[1][1]=1;panelPose.m[0][2]=-x;panelPose.m[2][2]=-z;panelPose.m[0][3]=head.x+x*2.f;panelPose.m[1][3]=head.y;panelPose.m[2][3]=head.z+z*2.f;
  m->overlay->SetOverlayTransformAbsolute(m->panel,vr::TrackingUniverseStanding,&panelPose);
 }else if(!panelOpen&&m->menuWasOpen){vr::HmdMatrix34_t panelPose{};panelPose.m[0][0]=panelPose.m[1][1]=panelPose.m[2][2]=1;panelPose.m[2][3]=-2.f;m->overlay->SetOverlayTransformTrackedDeviceRelative(m->panel,vr::k_unTrackedDeviceIndex_Hmd,&panelPose);}
 m->menuWasOpen=panelOpen;m->titlePanel=panelOpen;
 if(m->titlePanel)m->overlay->ShowOverlay(m->panel);else m->overlay->HideOverlay(m->panel);
 input.pointerX=input.pointerY=-1;
 // The panel takes a pointer from either hand (the gun hand's first); there is nothing on her hands or wrists.
 if(m->titlePanel){
  if(menuOpen)input.fire=desktopClick;input.menuLeft|=focused&&moveStick.x<-.6f;input.menuRight|=focused&&moveStick.x>.6f;
  int first=offTrigger&&!gunTrigger?0:1;
  for(int n=0;n<2&&focused;++n){const int role=n?1-first:first,side=m->side[size_t(role)];auto device=devices[side];if(device>=m->poses.size()||!m->hands[size_t(role)].tracked)continue;const auto&controllerPose=m->poses[device].mDeviceToAbsoluteTracking;vr::VROverlayIntersectionParams_t ray{};ray.eOrigin=vr::TrackingUniverseStanding;ray.vSource={{controllerPose.m[0][3],controllerPose.m[1][3],controllerPose.m[2][3]}};ray.vDirection={{-controllerPose.m[0][2],-controllerPose.m[1][2],-controllerPose.m[2][2]}};vr::VROverlayIntersectionResults_t hit{};
   if(m->overlay->ComputeOverlayIntersection(m->panel,&ray,&hit)){input.pointerX=std::clamp(int(hit.vUVs.v[0]*DisplayWidth),0,DisplayWidth-1);input.pointerY=std::clamp(int((1.f-hit.vUVs.v[1])*DisplayHeight),0,DisplayHeight-1);if(menuOpen){input.fire|=states[side].rAxis[1].x>.65f;input.menuAccept=desktopAccept;}break;}
  }
 }
 m->pointerX=input.pointerX;m->pointerY=input.pointerY;
 auto origin=game.chunkOffset(game.level());const auto&gunHand=m->hands[1];auto aim=gunHand.handFrame?gunHand.gunOrigin:gunHand.position;Point3 aimDir=(gunHand.handFrame?gunHand.gunBasis[2]:gunHand.basis[2])*-1;bool support=false;
 // Only the shotgun takes a fore-end: the pistol is aimed from the gun hand alone. With the support hand on it,
 // the gun is turned (in tracking space, supportPose) so its fore-end runs through that hand.
 m->support=false;m->pump=game.vrPumpOffset();
 if(focused&&m->hands[0].tracked&&m->hands[1].tracked&&game.weaponEquipped()&&!game.unarmed()&&!game.holdingClutter()&&!menuOpen&&game.activeFirearm()==WeaponId::Shotgun){
  auto right=gunHand.handFrame?gunHand.gunOrigin:gunHand.position,left=m->hands[0].handFrame?m->hands[0].gunOrigin:m->hands[0].position,handForward=aimDir;
  if(foregripSupport(right,left,handForward,input.vrGrip[0])){support=true;
   if(m->track[0].valid&&m->track[1].valid){m->support=true;m->applySupport(game);aimDir=m->hands[1].gunBasis[2]*-1;}
   else{auto delta=left-right;float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);if(length>.0001f)aimDir={delta.x/length,delta.y/length,delta.z/length};}}
 }
 // The hands the game uses are the avatar's: where her arms can reach (solveAvatar pulls a wrist past her
 // reach in to it), so what a hand grabs, pushes or shoots from is where her hand is drawn.
 const AvatarPose body=avatar(game);const AvatarSolve solved=solveAvatar(body,game.elapsed());
 auto reachShift=[&](int role){const int side=m->side[size_t(role)];return solved.wrist[size_t(side)]-body.hands[size_t(side)].wrist;};
 aim=aim+reachShift(1);aim.x-=origin.x;aim.y-=origin.y;
 game.setVrAim(aim,aimDir,m->hands[1].tracked&&focused,support);
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];auto hand=m->hands[size_t(role)].position+(m->hands[size_t(role)].tracked?reachShift(role):Point3{});hand.x-=origin.x;hand.y-=origin.y;Point3 velocity{};if(devices[side]<m->poses.size()){const auto&v=m->poses[devices[side]].vVelocity;velocity=rotateTracking({v.v[0],v.v[1],v.v[2]},m->yaw)*m->scale;}game.setVrHand(role,hand,m->hands[size_t(role)].basis[2]*-1,m->hands[size_t(role)].tracked&&focused,velocity);}
 // The comfort vignette: the setting's strength, as far as the view is being moved or turned by the sticks.
 m->vignette=menuOpen?0.f:cfg.vrVignette*std::clamp(std::max(std::hypot(input.vrMoveForward,input.vrMoveRight),std::fabs(m->turnRate)/2.1f),0.f,1.f);
 // The compositor's timing of the last frame, and the render scale for the next (dynamic unless fixed).
 {vr::Compositor_FrameTiming t{};t.m_nSize=sizeof(t);if(m->compositor->GetFrameTiming(&t,0)){m->timing.gpuMs=t.m_flTotalRenderGpuMs;m->timing.frameMs=t.m_flClientFrameIntervalMs;m->timing.dropped=t.m_nNumDroppedFrames;m->timing.reprojected=t.m_nReprojectionFlags;}
  m->timing.hz=m->displayHz;
  if(cfg.vrRenderScale>0)m->timing.scale=cfg.vrRenderScale;
  else if(++m->timingFrames>=6){m->timingFrames=0;m->timing.scale=nextRenderScale(m->timing.scale,m->timing.gpuMs,1000.f/m->displayHz);}}
 return true;
}
AvatarPose VrRuntime::avatar(const Game& game)const{
 AvatarPose p;if(!m->calibrated)return p;p.active=true;
 const auto origin=game.chunkOffset(game.level());auto local=[&](Point3 w){w.x-=origin.x;w.y-=origin.y;return w;};
 const auto&hp=m->poses[0].mDeviceToAbsoluteTracking;p.head=local(m->world(translation(hp),game));
 const Point3 f=rotateTracking({-hp.m[0][2],-hp.m[1][2],-hp.m[2][2]},m->yaw);p.headYaw=std::atan2(f.y,f.x);p.headPitch=std::asin(std::clamp(f.z,-1.f,1.f));
 p.bodyYaw=m->bodyReady?m->bodyYaw:p.headYaw;
 const auto&player=game.player();p.root={player.pos.x,player.pos.y,player.z};p.eye=player.eye;p.velocity=game.playerVelocity();p.gait=game.bodyGait();
 p.crouched=player.crouched;p.prone=player.prone;p.grounded=player.grounded;
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];const auto&h=m->hands[size_t(role)];AvatarHand a;a.tracked=h.tracked;a.curls=h.curls;
  if(h.handFrame){a.wrist=local(h.handOrigin);a.forward=h.handBasis[2]*-1.f;a.palm=h.handBasis[1];}
  else{a.wrist=local(h.position);a.forward=h.basis[2]*-1.f;a.palm=side==1?h.basis[0]*-1.f:h.basis[0];}
  if(!h.snapped)a.wrist=a.wrist+game.vrHandOffset(role);   // held out of the props it pushes (VrHands.cpp)
  p.hands[size_t(side)]=a;}
 p.gunSide=std::uint8_t(m->side[1]);
 const bool armed=game.weaponEquipped()&&!game.unarmed();p.inHand=armed?std::uint8_t(int(game.equippedWeapon())+1):0;
 p.back=p.inHand==int(WeaponId::Shotgun)+1?0:std::uint8_t(int(WeaponId::Shotgun)+1);
 p.hip=game.hasHandgun()&&p.inHand!=int(WeaponId::Handgun)+1?std::uint8_t(int(WeaponId::Handgun)+1):0;
 p.support=m->support&&game.vrSupportGrip();p.pump=game.vrPumpOffset();p.reload=game.reloadProgress();
 return p;
}
bool VrRuntime::head(const Game& game,Point3& position,Point3& forward)const{
 if(!m->poses[0].bPoseIsValid)return false;const auto&hp=m->poses[0].mDeviceToAbsoluteTracking;position=m->world(translation(hp),game);const auto origin=game.chunkOffset(game.level());position.x-=origin.x;position.y-=origin.y;
 forward=rotateTracking({-hp.m[0][2],-hp.m[1][2],-hp.m[2][2]},m->yaw);return true;}
VrEye VrRuntime::eye(int index,const Game&game)const{
 // The hands again, as late as the frame allows (the eye images are drawn after the game's update).
 for(int role=0;role<2;++role)if(m->hands[size_t(role)].tracked){const int side=m->side[size_t(role)];auto device=m->system->GetTrackedDeviceIndexForControllerRole(side?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand);if(device>=m->poses.size())continue;vr::HmdMatrix34_t grip{};const auto&pose=m->sampleGrip(side,grip,nullptr)?grip:m->poses[device].mDeviceToAbsoluteTracking;m->hands[size_t(role)].position=m->world(translation(pose),game);for(int i=0;i<3;++i)m->hands[size_t(role)].basis[i]=rotateTracking({pose.m[0][i],pose.m[1][i],pose.m[2][i]},m->yaw);m->sampleHand(role,m->hands[size_t(role)],game);}
 if(m->support)m->applySupport(game);
 auto h=m->poses[0].mDeviceToAbsoluteTracking;auto e=m->system->GetEyeToHeadTransform(vr::EVREye(index));vr::HmdMatrix34_t pose{};
 for(int r=0;r<3;++r){for(int col=0;col<3;++col)for(int k=0;k<3;++k)pose.m[r][col]+=h.m[r][k]*e.m[k][col];pose.m[r][3]=h.m[r][3];for(int k=0;k<3;++k)pose.m[r][3]+=h.m[r][k]*e.m[k][3];}
 Matrix world{};world[15]=1;for(int col=0;col<3;++col){auto p=rotateTracking({pose.m[0][col],pose.m[1][col],pose.m[2][col]},m->yaw);world[col*4]=p.x;world[col*4+1]=p.y;world[col*4+2]=p.z;}auto p=m->world(translation(pose),game);world[12]=p.x;world[13]=p.y;world[14]=p.z;
 auto projection=m->system->GetProjectionMatrix(vr::EVREye(index),.06f,250.f);Matrix clip{};for(int col=0;col<4;++col){clip[col*4]=projection.m[0][col];clip[col*4+1]=-projection.m[1][col];clip[col*4+2]=(projection.m[2][col]+projection.m[3][col])*.5f;clip[col*4+3]=projection.m[3][col];}
 return {p,multiply(clip,rigidInverse(world))};
}
void VrRuntime::submit(int eye,VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height,float bounds){
 if(!trackingValid())return;
 vr::VRVulkanTextureData_t data{};data.m_nImage=reinterpret_cast<uint64_t>(image);data.m_pDevice=device;data.m_pPhysicalDevice=physical;data.m_pInstance=instance;data.m_pQueue=queue;data.m_nQueueFamilyIndex=family;data.m_nWidth=width;data.m_nHeight=height;data.m_nFormat=VK_FORMAT_R8G8B8A8_UNORM;data.m_nSampleCount=1;
 // A render scale under 1 draws the eye into the image's top-left corner; SteamVR is told to stretch that part.
 vr::VRTextureBounds_t area{0,0,std::clamp(bounds,.1f,1.f),std::clamp(bounds,.1f,1.f)};
 vr::Texture_t texture{&data,vr::TextureType_Vulkan,vr::ColorSpace_Gamma};auto result=m->compositor->Submit(vr::EVREye(eye),&texture,bounds<1.f?&area:nullptr);if(result!=vr::VRCompositorError_None){logCompositor(result);m->trackingInterrupted=true;m->poses[0].bPoseIsValid=false;if(transientCompositor(result)||m->system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd))return;throw std::runtime_error("SteamVR Vulkan eye submission failed ("+std::to_string(result)+").");}if(eye==1)m->compositor->PostPresentHandoff();
}
void VrRuntime::submitPanel(VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height){
 if(!m->titlePanel)return;vr::VRVulkanTextureData_t data{};data.m_nImage=reinterpret_cast<uint64_t>(image);data.m_pDevice=device;data.m_pPhysicalDevice=physical;data.m_pInstance=instance;data.m_pQueue=queue;data.m_nQueueFamilyIndex=family;data.m_nWidth=width;data.m_nHeight=height;data.m_nFormat=VK_FORMAT_B8G8R8A8_UNORM;data.m_nSampleCount=1;vr::Texture_t texture{&data,vr::TextureType_Vulkan,vr::ColorSpace_Gamma};auto result=m->overlay->SetOverlayTexture(m->panel,&texture);if(result!=vr::VROverlayError_None)throw std::runtime_error("SteamVR menu texture failed ("+std::to_string(result)+").");
}
bool VrRuntime::shoulderSlot(Point3 offset,Point3 forward){float horizontal=std::hypot(forward.x,forward.y);if(horizontal<.1f)return false;forward.x/=horizontal;forward.y/=horizontal;float behind=offset.x*forward.x+offset.y*forward.y,side=-offset.x*forward.y+offset.y*forward.x;return behind<-.12f&&behind>-.65f&&std::fabs(side)<.55f&&offset.z>-.45f&&offset.z<.25f;}
bool VrRuntime::foregripSupport(Point3 right,Point3 left,Point3 rightForward,bool gripped){
 if(!gripped)return false;
 auto delta=left-right;float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
 if(distance<.16f||distance>.58f)return false;
 float forwardLength=std::sqrt(rightForward.x*rightForward.x+rightForward.y*rightForward.y+rightForward.z*rightForward.z);
 if(forwardLength<.0001f)return false;rightForward=rightForward*(1.f/forwardLength);
 float along=delta.x*rightForward.x+delta.y*rightForward.y+delta.z*rightForward.z;
 // A hand beside the barrel is not a support grip. Limit lateral/vertical
 // reach to 12 cm instead of accepting the former 69-degree cone.
 return along/distance>.8f&&distance*distance-along*along<=.12f*.12f;
}
void VrRuntime::setForeEnd(Point3 centre,float half){if(half>.01f&&half<.4f&&std::isfinite(centre.x)&&std::isfinite(centre.y)&&std::isfinite(centre.z)){g_foreEnd=centre;g_foreEndHalf=half;}}
VrRuntime::SupportPose VrRuntime::supportPose(Point3 grip,Point3 gunRight,Point3 gunUp,Point3 wrist,Point3 across,Point3 palm,Point3 forward,Point3 foreEnd,float half,float pump,bool leftHand){
 SupportPose s;const Point3 hold=wrist+forward*GripAlong+palm*GripIn;   // where the support fist closes
 // The gun: turned about the trigger hand so the fore-end's line (offset from the grip's axis by foreEnd x, y)
 // runs through the support fist. The trigger hand's up keeps the gun's roll; a few passes settle the offset.
 Point3 right=gunRight,up=gunUp,back=unit3(cross3(gunRight,gunUp));
 for(int pass=0;pass<4;++pass){const Point3 d=hold-grip-(right*foreEnd.x+up*foreEnd.y);if(len3(d)<1e-4f)break;back=unit3(d*-1.f);
  Point3 r=unit3(cross3(up,back));if(dot3(r,r)<.5f)r=unit3(right-back*dot3(right,back));if(dot3(r,r)<.5f)break;right=r;up=unit3(cross3(back,right));}
 s.gunRight=right;s.gunUp=up;s.gunBack=back;
 // The hand: the shortest turn laying its fist's channel (pinky to index) along the barrel, index to the
 // muzzle. It keeps the roll the real wrist has about the fore-end, so turning the hand round it turns the
 // drawn hand as far, and never flips it.
 const Point3 muzzle=back*-1.f;const float handed=dot3(cross3(across,palm),forward)<0?-1.f:1.f;
 const Point3 channel=unit3(across*(leftHand?-1.f:1.f));
 Point3 a=unit3(turnVector(across,channel,muzzle)),p=turnVector(palm,channel,muzzle);p=unit3(p-a*dot3(p,a));
 const Point3 f=unit3(cross3(a,p)*handed);
 s.across=a;s.palm=p;s.forward=f;
 // On the fore-end's line, where along it the hand is, held within the wood (slid back with the pump).
 const Point3 centre=grip+right*foreEnd.x+up*foreEnd.y+back*(foreEnd.z+pump);
 const float t=std::clamp(dot3(hold-centre,muzzle),-half*.85f,half*.85f);
 s.wrist=centre+muzzle*t-(f*GripAlong+p*GripIn);
 return s;
}
Point3 VrRuntime::lazyFollow(Point3 anchor,Point3 forward,float dt,float slack){
 const Point3 f=unit3(forward);if(dot3(f,f)<.5f)return anchor;const Point3 a=unit3(anchor);if(dot3(a,a)<.5f)return f;
 const float c=std::clamp(dot3(a,f),-1.f,1.f),angle=std::acos(c);
 if(angle>1.745f)return f;if(angle<=slack)return a;
 const float step=std::min(angle-slack,dt*(.3f+4.f*(angle-slack)));
 const Point3 across=unit3(f-a*c);if(dot3(across,across)<.5f)return f;
 return unit3(a*std::cos(step)+across*std::sin(step));
}
bool VrRuntime::testMath(){
 if(!transientCompositor(vr::VRCompositorError_DoNotHaveFocus)||!transientCompositor(vr::VRCompositorError_IsNotSceneApplication)||transientCompositor(vr::VRCompositorError_InvalidTexture))return false;
 for(int side:{0,1})for(Point3 forward:{Point3{0,0,1},Point3{1,0,0}})for(Point3 across:{Point3{},forward}){
  std::array<Point3,3> frame{};if(!handFrame(forward,across,side,frame))return false;
  auto dot=[](Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
  for(int i=0;i<3;++i){if(std::fabs(dot(frame[i],frame[i])-1)>.0001f)return false;for(int j=0;j<i;++j)if(std::fabs(dot(frame[i],frame[j]))>.0001f)return false;}
 }
 {auto game=std::make_unique<Game>();VrRuntime runtime;InputState input{};
  input.fire=input.use=input.jump=input.reload=true;input.vrGrip={true,true};
  input.vrMoveForward=1;input.vrMoveRight=-1;input.pointerX=input.pointerY=42;
  runtime.m->hands[0].tracked=runtime.m->hands[1].tracked=true;
  runtime.m->previousGrip={true,true};runtime.m->consumed={true,true};runtime.m->drawnFrom=1;runtime.m->support=true;
  runtime.m->calibrated=true;runtime.m->headStartY=1.45f;
  runtime.suspendTracking(*game,input);
  if(!input.vrTracked||input.fire||input.use||input.jump||input.reload||input.vrGrip[0]||input.vrGrip[1]||input.vrMoveForward||input.vrMoveRight||input.pointerX!=-1||input.pointerY!=-1||runtime.m->hands[0].tracked||runtime.m->hands[1].tracked||runtime.m->previousGrip[0]||runtime.m->consumed[1]||runtime.m->drawnFrom||runtime.m->support||!runtime.m->trackingInterrupted||!runtime.m->calibrated||runtime.m->headStartY!=1.45f)return false;
 }
 if(!foregripSupport({0,0,0},{.35f,0,0},{1,0,0},true)||foregripSupport({0,0,0},{.35f,0,0},{1,0,0},false)||foregripSupport({0,0,0},{.08f,0,0},{1,0,0},true)||foregripSupport({0,0,0},{-.35f,0,0},{1,0,0},true)||foregripSupport({0,0,0},{0,.35f,0},{1,0,0},true)||foregripSupport({0,0,0},{.35f,.25f,0},{1,0,0},true)||!foregripSupport({1,2,3},{1,2.35f,3.05f},{0,2,0},true))return false;
 if(!shoulderSlot({-.3f,.2f,-.1f},{1,0,0})||!shoulderSlot({-.2f,-.3f,-.1f},{0,1,0})||shoulderSlot({.3f,.2f,-.1f},{1,0,0})||shoulderSlot({-.3f,.2f,-.8f},{1,0,0}))return false;
 {Matrix a{};a[0]=a[5]=a[10]=a[15]=1;a[12]=4;a[13]=-2;a[14]=9;auto b=multiply(a,rigidInverse(a));for(int i=0;i<16;++i)if(std::fabs(b[i]-(i%5==0?1.f:0.f))>.00001f)return false;}
 {auto f=rotateTracking({0,0,-1},0);auto r=rotateTracking({1,0,0},0);auto up=rotateTracking({0,1,0},0);if(!(f.x==1&&r.y==1&&up.z==1))return false;}
 std::ostringstream sink;return testSupport(sink)&&testComfortMath(sink);
}
// The support hand on the fore-end (supportPose), numerically, in tracking space and through the mirror.
bool VrRuntime::testSupport(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto det=[](Point3 a,Point3 b,Point3 c){return dot3(a,cross3(b,c));};
 auto angle=[](Point3 a,Point3 b){return std::acos(std::clamp(dot3(unit3(a),unit3(b)),-1.f,1.f));};
 // About an axis through the origin by t (Rodrigues).
 auto spin=[](Point3 v,Point3 k,float t){k=unit3(k);return v*std::cos(t)+cross3(k,v)*std::sin(t)+k*(dot3(k,v)*(1-std::cos(t)));};
 // The gun in the right hand, level, pointing ahead (-z): right x, up y, back +z.
 const Point3 grip{0,1.2f,-.3f},right{1,0,0},up{0,1,0},foreEnd{0,-.012f,-.36f};const float half=.09f;
 const Point3 muzzle{0,0,-1},line=grip+right*foreEnd.x+up*foreEnd.y+Point3{0,0,1}*foreEnd.z;
 // A left hand under the fore-end, palm up, index toward the muzzle, fingers across to the right; its frame as
 // handFrame builds it (across = forward x palm: toward the little finger on a left hand).
 const Point3 lf{1,0,0},lp{0,1,0},la=cross3(lf,lp);
 auto place=[&](Point3 f,Point3 p,Point3 holdAt){return holdAt-(f*GripAlong+p*GripIn);};
 {const Point3 w=place(lf,lp,line);const auto s=supportPose(grip,right,up,w,la,lp,lf,foreEnd,half,0,true);
  check(len3(s.wrist-w)<1e-4f&&len3(s.across-la)<1e-4f&&len3(s.palm-lp)<1e-4f&&len3(s.forward-lf)<1e-4f,"a hand already round the fore-end is left where it is");
  check(len3(s.gunBack-Point3{0,0,1})<1e-4f&&det(s.gunRight,s.gunUp,s.gunBack)>.999f,"the gun keeps its aim and its right-handed frame");}
 // The hand rolled round the fore-end: the drawn hand rolls as far, every step (no twist, no flip).
 {float worst=0,lastPalmJump=0;Point3 previous{};bool first=true,follows=true;
  for(int k=0;k<=120;++k){const float t=float(k)*kPi/60;const Point3 f=spin(lf,muzzle,t),p=spin(lp,muzzle,t),a=cross3(f,p);const Point3 w=place(f,p,line);
   const auto s=supportPose(grip,right,up,w,a,p,f,foreEnd,half,0,true);follows&=angle(s.palm,p)<1e-3f;
   if(!first)lastPalmJump=std::max(lastPalmJump,angle(s.palm,previous));previous=s.palm;first=false;worst=std::max(worst,len3(s.wrist-w));}
  check(follows&&lastPalmJump<.06f&&worst<1e-3f,"rolled a full turn round the fore-end, the drawn hand follows it 3 degrees a step");}
 // Swung off the fore-end's line by up to 50 degrees (and rolled): the fist is laid along the barrel, the
 // palm moves on smoothly, and the frame stays the hand's own handedness through the mirror.
 {float jump=0,offAxis=0;bool frames=true,mirrored=true;Point3 previous{};bool first=true;
  for(int k=-50;k<=50;++k){const float swing=float(k)*kPi/180,roll=float(k)*kPi/360;
   Point3 f=spin(spin(lf,muzzle,roll),up,swing),p=spin(spin(lp,muzzle,roll),up,swing);const Point3 a=cross3(f,p),w=place(f,p,line+Point3{.01f*float(k%3),0,0});
   const auto s=supportPose(grip,right,up,w,a,p,f,foreEnd,half,0,true);
   offAxis=std::max(offAxis,len3(s.across-s.gunBack));   // the left fist's channel (-across) along the re-aimed muzzle
   frames&=std::fabs(dot3(s.across,s.palm))<1e-4f&&std::fabs(dot3(s.palm,s.forward))<1e-4f&&det(s.across,s.palm,s.forward)<-.999f;
   // Mapped into game space as the hand mesh's basis (across, palm, back): the same sign as an unsnapped hand's.
   for(float yaw:{0.f,1.3f}){const Point3 ga=rotateTracking(s.across,yaw),gp=rotateTracking(s.palm,yaw),gb=rotateTracking(s.forward*-1.f,yaw);
    const Point3 ua=rotateTracking(a,yaw),up2=rotateTracking(p,yaw),ub=rotateTracking(f*-1.f,yaw);mirrored&=(det(ga,gp,gb)<0)==(det(ua,up2,ub)<0);}
   if(!first)jump=std::max(jump,angle(s.palm,previous));previous=s.palm;first=false;}
  check(offAxis<1e-3f,"swung off the line, the fist is laid along the barrel, index to the muzzle");
  check(frames&&mirrored,"the snapped frame keeps the hand's handedness, in tracking space and mapped to game space");
  check(jump<.05f,"the palm moves on smoothly through the swing (no flip)");}
 // The gun turns about the trigger hand so the fore-end runs through the support fist.
 {const Point3 f=spin(lf,up,.3f),p=spin(lp,up,.3f),a=cross3(f,p);const Point3 hold=grip+Point3{.08f,.05f,-.38f};
  const auto s=supportPose(grip,right,up,place(f,p,hold),a,p,f,foreEnd,half,0,true);
  const Point3 centre=grip+s.gunRight*foreEnd.x+s.gunUp*foreEnd.y;const Point3 d=hold-centre;const float off=len3(d-s.gunBack*dot3(d,s.gunBack));
  check(off<2e-3f&&dot3(d,s.gunBack)<0&&det(s.gunRight,s.gunUp,s.gunBack)>.999f&&dot3(s.gunUp,up)>.95f,"the gun is aimed so its fore-end runs through the support fist, keeping its roll");}
 // Along the fore-end: a hand off its ends is held on the wood; the pump slides the wood (and its ends) back.
 {const Point3 beyond=line+muzzle*.4f;const auto s=supportPose(grip,right,up,place(lf,lp,beyond),la,lp,lf,foreEnd,half,0,true);
  const Point3 hold=s.wrist+s.forward*GripAlong+s.palm*GripIn;check(std::fabs(dot3(hold-line,muzzle)-half*.85f)<1e-3f,"a hand past the fore-end's front is held at its front");
  const Point3 rear=line-muzzle*.3f;const auto q=supportPose(grip,right,up,place(lf,lp,rear),la,lp,lf,foreEnd,half,.07f,true);const Point3 hq=q.wrist+q.forward*GripAlong+q.palm*GripIn;
  check(std::fabs(dot3(hq-line,muzzle)-(-half*.85f-.07f))<1e-3f,"pumped back 7 cm, the wood's rear end is 7 cm further back");}
 // A left-handed player: the right hand on the fore-end (its channel runs the other way along its frame's x).
 {const Point3 rf{-1,0,0},rp{0,1,0},ra=cross3(rf,rp);const auto s=supportPose(grip,right,up,place(rf,rp,line),ra,rp,rf,foreEnd,half,0,false);
  check(len3(s.across-ra)<1e-4f&&len3(s.forward-rf)<1e-4f,"left-handed: a right support hand round the fore-end is left as it is");}
 return ok;
}
// The comfort maths: the cards' lazy follow, the dynamic resolution step, the avatar's scale.
bool VrRuntime::testComfortMath(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto angle=[](Point3 a,Point3 b){return std::acos(std::clamp(dot3(unit3(a),unit3(b)),-1.f,1.f));};
 const Point3 ahead{1,0,0};
 {const Point3 slight{std::cos(.1f),std::sin(.1f),0};check(len3(lazyFollow(ahead,slight,1.f/90)-ahead)<1e-6f,"a glance inside the slack leaves the cards still");
  Point3 a=ahead;const Point3 f{std::cos(1.05f),std::sin(1.05f),0};float last=angle(a,f);bool monotone=true;
  for(int i=0;i<180;++i){a=lazyFollow(a,f,1.f/90);const float now=angle(a,f);monotone&=now<=last+1e-6f;last=now;}
  check(monotone&&last<=.15f+.005f&&std::fabs(len3(a)-1)<1e-4f,"turned 60 degrees, the cards ease round to the slack within 2 s");
  const Point3 behind{std::cos(2.f),std::sin(2.f),0};check(len3(lazyFollow(ahead,behind,1.f/90)-behind)<1e-5f,"turned past 100 degrees, they jump to the look");}
 {check(nextRenderScale(1,12,11.1f)<1&&nextRenderScale(.8f,5,11.1f)>.8f&&nextRenderScale(.8f,9,11.1f)==.8f,"over budget the scale drops, well under it rises, in between it holds");
  check(nextRenderScale(.6f,30,11.1f)==.6f&&nextRenderScale(1,1,11.1f)==1&&nextRenderScale(.9f,0,11.1f)==.9f,"it stays within 0.6-1 and holds without a measurement");
  // A GPU that needs 15 ms at full size (time as the pixel count, the scale squared) on a 90 Hz budget.
  float s=1;float lowest=1,highest=0;for(int i=0;i<400;++i){s=nextRenderScale(s,15*s*s,11.11f);if(i>200){lowest=std::min(lowest,s);highest=std::max(highest,s);}}
  check(15*lowest*lowest>=.69f*11.11f&&15*highest*highest<=.91f*11.11f&&highest-lowest<.06f,"a 15 ms GPU at 90 Hz settles between 70 and 90 % of the budget");}
 {check(std::fabs(avatarScale(1.62f,false)-Player::StandingEye/1.62f)<1e-5f&&std::fabs(avatarScale(1.1f,true)-Player::StandingEye/1.62f)<1e-5f,"the player's tracking is scaled to her height (seated: an average standing eye)");
  check(avatarScale(0,false)==1.25f&&avatarScale(3,false)>=.7f,"a floor that is not set up cannot scale her past 0.7-1.25");}
 return ok;
}
bool VrRuntime::testAll(std::ostream& report){
 bool ok=true;auto run=[&](const char* name,bool pass){report<<(pass?"PASS: ":"FAIL: ")<<name<<'\n';ok&=pass;};
 run("tracking recovery, foregrip support, coordinate axes, rigid-view inversion, shoulder box",testMath());
 ok&=testSupport(report);ok&=testComfortMath(report);ok&=testAvatar(report);ok&=SoftwareRenderer::testVrAvatar(report);
 ok&=Game::testComfortMenu(report);ok&=Game::testVrHardening(report);
 run("articulated hand meshes (Mesh::testVrHands)",Mesh::testVrHands());run("physical melee",Game::testVrMelee());run("controller interaction",Game::testVrInteraction());
 report<<(ok?"ALL PASSED":"FAILED")<<'\n';return ok;
}
}
