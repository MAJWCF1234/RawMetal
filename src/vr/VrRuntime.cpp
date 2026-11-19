#include "VrRuntime.h"
#include "../ThirdParty/openvr/openvr.h"
#include "../renderer/SoftwareRenderer.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <set>
#include <chrono>
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
 std::array<vr::VRActionHandle_t,BindingActions> actions{};bool crouchToggled=false,previousCrouch=false;int framesSeen=0;
 // SteamVR's grip pose: the same hand frame on every controller (the legacy
 // device pose is pitched differently on Touch, Index and Vive controllers).
 vr::VRActionHandle_t grip[2]{};std::array<vr::HmdMatrix34_t,2> raw{},gripPose{};std::array<bool,2> gripValid{};
 // Roles: hands[] and track[] are by role (1 the gun hand, 0 the other); side[role] is the physical hand
 // (0 left, 1 right): swapped when left-handed. SteamVR's devices, skeletons and controller states stay physical.
 std::array<int,2> side{0,1};bool leftHanded=false,seated=false,support=false;float pump=0;
 // supportKind: 0 none, 1 the shotgun's fore-end, 2 round the pistol's grip. vel: each role's hand velocity (game
 // space). rawGrip: each role's grip as read, before a holster, the pouch or a held magazine takes it.
 // haptic: the vibration actions (physical hands); hapticBound: SteamVR bound them (else the legacy pulse).
 int supportKind=0;std::array<Point3,2> vel{};std::array<bool,2> rawGrip{};vr::VRActionHandle_t haptic[2]{};std::array<bool,2> hapticBound{true,true};
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
  return fromJoints(role,h,game,joint(1),joint(7),joint(12),joint(22));
 }
 // A hand (and the gun it would hold) from its wrist and knuckles in tracking space: sampleHand's joints, or a
 // scripted pose's.
 bool fromJoints(int role,VrHand& h,const Game& game,Point3 wrist,Point3 index,Point3 middle,Point3 pinky){
  const int physical=side[size_t(role)];
  std::array<Point3,3> frame{};if(!handFrame(middle-wrist,index-pinky,physical,frame))return false;
  auto across=frame[0],palm=frame[1],forward=frame[2];
  h.handOrigin=world(wrist,game);h.handBasis={rotateTracking(across,yaw),rotateTracking(palm,yaw),rotateTracking(forward*-1.f,yaw)};h.handFrame=true;
  // The weapon: its grip through the fist, up the fist toward the index finger, the barrel along the knuckles
  // raised by the grip's rake (gunFrame). Built in tracking space like the hand, so the aim and the model agree
  // with the hand the player sees. The mesh frame's +y is the palm side (the closed fist spans y -0.03 to
  // +0.06): the grip goes through the fist, 7.5 cm along and 3 cm into the palm.
  Point3 right,up,back;gunFrame(across,palm,forward,physical==0,right,up,back,gunPitch(game.equippedWeapon()));
  const Point3 grip=wrist+forward*GripAlong+palm*GripIn;
  h.gunOrigin=world(grip,game);h.gunBasis={rotateTracking(right,yaw),rotateTracking(up,yaw),rotateTracking(back,yaw)};
  track[size_t(role)]={true,wrist,across,palm,forward,grip,right,up,back};
  return true;
 }
 // The support arm's elbow in tracking space, roughly (half a metre under the head and out to the support hand's
 // side, as an elbow hangs under a gun held up), for the fore-end snap's roll.
 Point3 elbow()const{
  const auto&hp=poses[0].mDeviceToAbsoluteTracking;const Point3 head=translation(hp);Point3 f{-hp.m[0][2],0,-hp.m[2][2]};f=unit3(f);if(dot3(f,f)<.5f)f={0,0,-1};
  const Point3 right{-f.z,0,f.x};return head+Point3{0,-.52f,0}+right*(side[0]==0?-.22f:.22f)-f*.05f;}
 // The support hand on the gun (in tracking space), then mapped. On the fore-end (supportPose) the gun frame is
 // aimed through the fist and the hand drawn round the wood; round the pistol's grip (pistolSupportPose) only the
 // hand moves.
 void applySupport(const Game& game){
  const auto&g=track[1],&o=track[0];if(!support||!g.valid||!o.valid)return;
  SupportPose s;
  if(supportKind==1){s=supportPose(g.grip,g.right,g.up,o.wrist,o.across,o.palm,o.forward,g_foreEnd,g_foreEndHalf,pump,side[0]==0,elbow());
   auto&gun=hands[1];gun.gunBasis={rotateTracking(s.gunRight,yaw),rotateTracking(s.gunUp,yaw),rotateTracking(s.gunBack,yaw)};}
  else if(supportKind==2)s=pistolSupportPose(g.grip,g.right,g.up,g.back,side[0]==0,gunPitch(game.equippedWeapon()));
  else return;
  auto&h=hands[0];h.handOrigin=world(s.wrist,game);h.handBasis={rotateTracking(s.across,yaw),rotateTracking(s.palm,yaw),rotateTracking(s.forward*-1.f,yaw)};h.snapped=true;
 }
 // Recoil (Game::vrRecoil) on the gun in the hand: the frame turned about its right through the grip so the muzzle
 // rises, and pushed back along the bore; a support hand on the gun goes with it. The view never moves.
 void applyRecoil(const Game& game){
  const auto r=game.vrRecoil();if(r.back==0&&r.rise==0)return;auto&gun=hands[1];if(!gun.tracked)return;
  auto&o=gun.handFrame?gun.gunOrigin:gun.position;auto&b=gun.handFrame?gun.gunBasis:gun.basis;
  const float c=std::cos(r.rise),sn=std::sin(r.rise);const Point3 right=b[0],up=b[1],back=b[2];
  const Point3 back2=back*c-up*sn,up2=up*c+back*sn;
  auto turn=[&](Point3 v){return right*dot3(v,right)+up2*dot3(v,up)+back2*dot3(v,back);};
  const Point3 pivot=o;b={right,up2,back2};o=o+back2*r.back;
  auto&h=hands[0];if(h.snapped){h.handOrigin=pivot+turn(h.handOrigin-pivot)+back2*r.back;for(auto&v:h.handBasis)v=turn(v);}
 }
 vr::VRActionSetHandle_t set=0;vr::VRActionHandle_t skeleton[2]{};vr::IVROverlay* overlay=nullptr;vr::VROverlayHandle_t panel=0;bool titlePanel=false;
 std::array<vr::TrackedDevicePose_t,vr::k_unMaxTrackedDeviceCount> poses{};
 std::array<VrHand,2> hands{};uint32_t width=0,height=0;float yaw=0,headStartY=0;Point3 previousHead{};bool calibrated=false,trackingInterrupted=false,menuWasOpen=false;int pointerX=-1,pointerY=-1;
 // The gun hand's grip, held from the last frame; a draw from a holster, and whether the hand has left it since
 // (a release back at the holster puts the gun away only then).
 std::array<bool,2> previousGrip{},consumed{},gripDown{};int drawnFrom=0;bool leftHolster=false;
 // Tracking metres to game metres (avatarScale), the hips' facing (lagging the head), the comfort vignette.
 float scale=1,bodyYaw=0,turnRate=0,vignette=0;bool bodyReady=false;
 VrTiming timing;int timingFrames=0;float displayHz=0;
 Point3 world(Point3 point,const Game&game)const{auto head=translation(poses[0].mDeviceToAbsoluteTracking);auto offset=rotateTracking(point-head,yaw)*scale;auto origin=game.chunkOffset(game.level());return offset+Point3{game.player().pos.x+origin.x,game.player().pos.y+origin.y,game.player().z+game.player().eye};}
 ~Impl(){if(overlay&&panel)overlay->DestroyOverlay(panel);if(shutdown&&system)shutdown();if(dll)FreeLibrary(dll);}
};
VrRuntime::VrRuntime():m(std::make_unique<Impl>()){}
VrRuntime::~VrRuntime(){if(current==this)current=nullptr;}
VrRuntime* VrRuntime::active(){return current;}
// The registry key Steam records its folder under. Written with escaped backslashes:
// 1.0.4 to 1.5.5 had L"Software\Valve\Steam" here, which C++ reads as
// "SoftwareValveSteam", so the lookup always failed and the PC/VR choice never appeared.
const wchar_t* const VrRuntime::SteamKey=L"Software\\Valve\\Steam";
namespace {
std::string narrow(const std::filesystem::path& p){auto u=p.u8string();return std::string(u.begin(),u.end());}
std::filesystem::path widen(const std::string& s){return std::filesystem::path(std::u8string(s.begin(),s.end()));}
std::wstring steamFolder(std::string& note){
 wchar_t steam[32768]{};DWORD bytes=sizeof(steam);
 LSTATUS status=RegGetValueW(HKEY_CURRENT_USER,VrRuntime::SteamKey,L"SteamPath",RRF_RT_REG_SZ,nullptr,steam,&bytes);
 if(status==ERROR_SUCCESS)return steam;
 note+="HKCU\\Software\\Valve\\Steam SteamPath: not found (error "+std::to_string(status)+")\n";
 bytes=sizeof(steam);status=RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\Valve\\Steam",L"InstallPath",RRF_RT_REG_SZ,nullptr,steam,&bytes);
 if(status==ERROR_SUCCESS)return steam;
 note+="HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam InstallPath: not found (error "+std::to_string(status)+")\n";
 return {};
}
std::string readVrpath(std::string& note){
 wchar_t local[32768]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768)){note+="LOCALAPPDATA is not set\n";return {};}
 auto file=std::filesystem::path(local)/L"openvr/openvrpaths.vrpath";std::ifstream in(file,std::ios::binary);
 if(!in){note+="openvrpaths.vrpath: not found at "+narrow(file)+"\n";return {};}
 std::stringstream text;text<<in.rdbuf();return text.str();
}
// Load the runtime's openvr_api.dll and ask it; never starts SteamVR. 1 present, 0 absent, -1 failed.
int queryRuntime(const std::filesystem::path& dllPath,std::string& note){
 HMODULE dll=LoadLibraryExW(dllPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!dll){note+="LoadLibraryExW failed (error "+std::to_string(GetLastError())+")\n";return -1;}
 auto installed=reinterpret_cast<bool(__cdecl*)()>(GetProcAddress(dll,"VR_IsRuntimeInstalled"));
 auto present=reinterpret_cast<bool(__cdecl*)()>(GetProcAddress(dll,"VR_IsHmdPresent"));
 if(!present){FreeLibrary(dll);note+="VR_IsHmdPresent export missing\n";return -1;}
 if(installed)note+=std::string("VR_IsRuntimeInstalled: ")+(installed()?"yes":"no")+"\n";
 bool found=present();FreeLibrary(dll);note+=std::string("VR_IsHmdPresent: ")+(found?"yes":"no")+"\n";return found?1:0;
}
}
std::string VrRuntime::vrpathRuntime(const std::string& json){
 auto key=json.find("\"runtime\"");if(key==std::string::npos)return {};
 auto open=json.find('[',key);if(open==std::string::npos)return {};
 auto start=json.find('"',open);auto close=json.find(']',open);if(start==std::string::npos||(close!=std::string::npos&&start>close))return {};
 std::string out;for(size_t i=start+1;i<json.size()&&json[i]!='"';++i){if(json[i]=='\\'&&i+1<json.size())++i;out+=json[i];}
 return out;
}
VrDetection VrRuntime::detect(const VrProbe& probe){
 VrDetection d;d.log=probe.note;
 std::vector<std::filesystem::path> candidates;
 // openvrpaths.vrpath is what SteamVR itself reads; it also finds SteamVR in a second Steam library.
 auto runtime=vrpathRuntime(probe.vrpathJson);
 if(!runtime.empty())candidates.push_back(widen(runtime)/L"bin/win64/openvr_api.dll");else if(!probe.vrpathJson.empty())d.log+="openvrpaths.vrpath: no runtime entry\n";
 if(!probe.steamPath.empty())candidates.push_back(std::filesystem::path(probe.steamPath)/L"steamapps/common/SteamVR/bin/win64/openvr_api.dll");
 if(candidates.empty()){d.reason="Steam and SteamVR were not found";d.log+="result: "+d.reason+"\n";return d;}
 for(const auto& c:candidates){
  bool exists=probe.exists(c);d.log+="candidate "+narrow(c)+": "+(exists?"exists":"missing")+"\n";
  if(!exists)continue;
  d.runtime=narrow(c);int answer=probe.query(c,d.log);
  if(answer<0){d.reason="openvr_api.dll could not be queried: "+d.runtime;continue;}
  d.offered=answer>0;d.reason=d.offered?"SteamVR sees a headset":"SteamVR is installed but sees no headset (connect it and start Quest Link or Air Link)";
  d.log+="result: "+d.reason+"\n";return d;
 }
 if(d.reason.empty())d.reason="SteamVR's openvr_api.dll was not found";
 d.log+="result: "+d.reason+"\n";return d;
}
VrDetection VrRuntime::detectHeadset(){
 VrProbe probe;probe.steamPath=steamFolder(probe.note);probe.vrpathJson=readVrpath(probe.note);
 probe.exists=[](const std::filesystem::path& p){std::error_code e;return std::filesystem::exists(p,e);};
 probe.query=queryRuntime;
 return detect(probe);
}
bool VrRuntime::headsetPresent(){return detectHeadset().offered;}
void VrRuntime::shutdownGraphics(){if(m->overlay&&m->panel){m->overlay->DestroyOverlay(m->panel);m->panel=0;}if(m->shutdown&&m->system)m->shutdown();m->system=nullptr;m->compositor=nullptr;m->input=nullptr;m->overlay=nullptr;if(current==this)current=nullptr;}
bool VrRuntime::initialize(std::string& error){
 try{
  // Load only the Steam installation's runtime, never an arbitrary current-directory DLL.
  VrProbe probe;probe.steamPath=steamFolder(probe.note);probe.vrpathJson=readVrpath(probe.note);
  probe.exists=[](const std::filesystem::path& p){std::error_code e;return std::filesystem::exists(p,e);};
  probe.query=[](const std::filesystem::path&,std::string&){return 1;}; // only locate it here; VR_Init reports the headset
  auto found=detect(probe);if(found.runtime.empty())throw std::runtime_error("SteamVR was not found ("+found.reason+"). Install SteamVR, then retry VR mode.\n"+found.log);
  auto path=widen(found.runtime);
  m->dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!m->dll)throw std::runtime_error("Could not load "+found.runtime+" (error "+std::to_string(GetLastError())+"). Reinstall SteamVR, then retry VR mode.");
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
   const char* const* names=bindingActionNames();
   {std::ofstream manifestFile(folder/L"actions.json");manifestFile<<actionManifest();}
   for(const char*controller:{"knuckles","oculus_touch","vive_controller"}){std::ofstream file(folder/(std::string(controller)+"-v"+std::to_string(BindingsVersion)+".json"));file<<controllerBindings(controller);}
   auto manifest=(folder/L"actions.json").string();if(m->input->SetActionManifestPath(manifest.c_str())==vr::VRInputError_None){m->input->GetActionSetHandle("/actions/game",&m->set);for(int i=0;i<BindingActions;++i)m->input->GetActionHandle((std::string("/actions/game/in/")+names[i]).c_str(),&m->actions[size_t(i)]);m->input->GetActionHandle("/actions/game/in/left",&m->skeleton[0]);m->input->GetActionHandle("/actions/game/in/right",&m->skeleton[1]);m->input->GetActionHandle("/actions/game/out/hapticleft",&m->haptic[0]);m->input->GetActionHandle("/actions/game/out/hapticright",&m->haptic[1]);}
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
  input.vrMoveForward=input.vrMoveRight=0;input.vrGrip={};input.vrTrigger={};input.fire=input.use=input.jump=input.reload=false;
  input.pointerX=input.pointerY=m->pointerX=m->pointerY=-1;
  m->previousGrip={};m->consumed={};m->gripDown={};m->previousCrouch=false;m->drawnFrom=0;m->turnRate=0;m->vignette=0;m->support=false;
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
  for(int side=0;side<2;++side){states[side].rAxis[1].x=digital(side,states[side].rAxis[1].x>.65f)?1.f:0.f;vr::InputDigitalActionData_t click{};vr::InputAnalogActionData_t squeeze{};
   const bool clickActive=m->input->GetDigitalActionData(m->actions[size_t(side+2)],&click,sizeof(click),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&click.bActive;
   const bool squeezeActive=m->input->GetAnalogActionData(m->actions[size_t(4+side)],&squeeze,sizeof(squeeze),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&squeeze.bActive;
   const bool grip=gripHeld(m->gripDown[size_t(side)],clickActive,click.bState,squeezeActive,squeeze.x,(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))!=0);m->gripDown[size_t(side)]=grip;if(grip)states[side].ulButtonPressed|=vr::ButtonMaskFromId(vr::k_EButton_Grip);else states[side].ulButtonPressed&=~vr::ButtonMaskFromId(vr::k_EButton_Grip);vr::InputAnalogActionData_t move{};if(m->input->GetAnalogActionData(m->actions[6+side],&move,sizeof(move),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&move.bActive)sticks[side]={move.x,move.y};}
  // Once SteamVR has loaded the bindings, record what drives each button action.
  if(++m->framesSeen==180){wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);std::ofstream log(std::filesystem::path(local)/L"RawMetal/vr/bindings-active.txt");
   const char* labels[]={"jump","reload","menu","inventory","crouch","talk"};const int ids[]={8,9,10,11,12,13};
   for(int k=0;k<6;++k){vr::VRInputValueHandle_t origins[vr::k_unMaxActionOriginCount]{};log<<labels[k]<<":";if(m->input->GetActionOrigins(m->set,m->actions[size_t(ids[k])],origins,vr::k_unMaxActionOriginCount)==vr::VRInputError_None)for(auto origin:origins){if(!origin)break;char name[128]{};m->input->GetOriginLocalizedName(origin,name,sizeof name,vr::VRInputString_All);log<<" ["<<name<<"]";}log<<'\n';}
   // The vibration outputs: bound, they are played through SteamVR Input; not, through the legacy pulse.
   for(int h=0;h<2;++h){vr::VRInputValueHandle_t origins[vr::k_unMaxActionOriginCount]{};bool bound=false;log<<(h?"haptic right:":"haptic left:");
    if(m->haptic[h]&&m->input->GetActionOrigins(m->set,m->haptic[h],origins,vr::k_unMaxActionOriginCount)==vr::VRInputError_None)for(auto origin:origins){if(!origin)break;bound=true;char name[128]{};m->input->GetOriginLocalizedName(origin,name,sizeof name,vr::VRInputString_All);log<<" ["<<name<<"]";}
    m->hapticBound[size_t(h)]=bound;log<<(bound?"":" (unbound: legacy pulse)")<<'\n';}}
  jumpButton=digital(8,jumpButton);reloadButton=digital(9,reloadButton);crouchButton=digital(12,crouchButton);menuButton=digital(10,menuButton);input.inventory|=digital(11,false);
  // Push-to-talk: held (the left thumbstick pressed in by default), the voice chat sends (InputState::talk).
  input.talk|=m->system->IsInputAvailable()&&digital(13,false);
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
 input.fire=focused&&m->hands[1].tracked&&gunTrigger;input.vrTrigger={focused&&m->hands[0].tracked&&offTrigger,input.fire};input.use=focused&&((states[0].ulButtonPressed|states[1].ulButtonPressed)&vr::ButtonMaskFromId(vr::k_EButton_Grip));input.jump=focused&&jumpButton;input.reload=focused&&reloadButton;input.menuAccept|=input.fire||(focused&&offTrigger);input.menuUp|=focused&&moveStick.y>.6f;input.menuDown|=focused&&moveStick.y<-.6f;input.escape|=focused&&menuButton;
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
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];m->rawGrip[size_t(role)]=(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))!=0;}
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
 // Each hand's velocity (game space), then the hands' play: holsters, pouch, support grip, recoil, aim.
 for(int role=0;role<2;++role){const int side=m->side[size_t(role)];m->vel[size_t(role)]={};if(devices[side]<m->poses.size()){const auto&v=m->poses[devices[side]].vVelocity;m->vel[size_t(role)]=rotateTracking({v.v[0],v.v[1],v.v[2]},m->yaw)*m->scale;}}
 playHands(game,input,focused,menuOpen);
 // The haptic pulses the game asked for this frame: SteamVR Input's vibration action, or the legacy pulse.
 for(const auto&pulse:game.takeVrHaptics()){const int side=m->side[size_t(pulse.hand&1)];
  bool played=false;if(m->input&&m->haptic[side]&&m->hapticBound[size_t(side)])played=m->input->TriggerHapticVibrationAction(m->haptic[side],0,pulse.seconds,pulse.frequency,std::clamp(pulse.amplitude,0.f,1.f),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None;
  if(!played&&devices[side]<m->poses.size())m->system->TriggerHapticPulse(devices[side],0,static_cast<unsigned short>(std::clamp(pulse.amplitude*3999.f,300.f,3999.f)));}
 // The comfort vignette: the setting's strength, as far as the view is being moved or turned by the sticks.
 m->vignette=menuOpen?0.f:cfg.vrVignette*std::clamp(std::max(std::hypot(input.vrMoveForward,input.vrMoveRight),std::fabs(m->turnRate)/2.1f),0.f,1.f);
 // The compositor's timing of the last frame, and the render scale for the next (dynamic unless fixed).
 {vr::Compositor_FrameTiming t{};t.m_nSize=sizeof(t);if(m->compositor->GetFrameTiming(&t,0)){m->timing.gpuMs=t.m_flTotalRenderGpuMs;m->timing.frameMs=t.m_flClientFrameIntervalMs;m->timing.dropped=t.m_nNumDroppedFrames;m->timing.reprojected=t.m_nReprojectionFlags;}
  m->timing.hz=m->displayHz;
  // (--vr-inspection-test captures whole eye images: its inspection camera keeps the full size.)
  if(game.inspectionCamera())m->timing.scale=1;
  else if(cfg.vrRenderScale>0)m->timing.scale=cfg.vrRenderScale;
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
 // The hip: the sidearm not in hand (the handgun or the MAC-11; with both carried and neither out, the one drawn last).
 p.hip=std::uint8_t(game.hipHolster(armed?game.equippedWeapon():WeaponId::Fists));
 p.support=m->support&&game.vrSupportGrip();p.pump=game.vrPumpOffset();p.reload=game.reloadProgress();
 return p;
}
bool VrRuntime::head(const Game& game,Point3& position,Point3& forward)const{
 if(!m->poses[0].bPoseIsValid)return false;const auto&hp=m->poses[0].mDeviceToAbsoluteTracking;position=m->world(translation(hp),game);const auto origin=game.chunkOffset(game.level());position.x-=origin.x;position.y-=origin.y;
 forward=rotateTracking({-hp.m[0][2],-hp.m[1][2],-hp.m[2][2]},m->yaw);return true;}
VrEye VrRuntime::eye(int index,const Game&game)const{
 // The hands again, as late as the frame allows (the eye images are drawn after the game's update).
 for(int role=0;role<2;++role)if(m->hands[size_t(role)].tracked){const int side=m->side[size_t(role)];auto device=m->system->GetTrackedDeviceIndexForControllerRole(side?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand);if(device>=m->poses.size())continue;vr::HmdMatrix34_t grip{};const auto&pose=m->sampleGrip(side,grip,nullptr)?grip:m->poses[device].mDeviceToAbsoluteTracking;m->hands[size_t(role)].position=m->world(translation(pose),game);for(int i=0;i<3;++i)m->hands[size_t(role)].basis[i]=rotateTracking({pose.m[0][i],pose.m[1][i],pose.m[2][i]},m->yaw);m->sampleHand(role,m->hands[size_t(role)],game);}
 m->applySupport(game);m->applyRecoil(game);
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
// The actions in handle order: 0-1 triggers, 2-3 grip clicks, 4-5 grip squeezes (vector1), 6 move and 7 turn
// (vector2), then the buttons. v3 (2026-11-19) added the vibration outputs; v4 (2026-11-19) push-to-talk ("talk",
// held: the left thumbstick pressed in on Touch and Index, the left trackpad's west click on a Vive wand).
const char* const* VrRuntime::bindingActionNames(){
 static const char* const names[BindingActions]={"triggerleft","triggerright","gripleft","gripright","squeezeleft","squeezeright","move","turn","jump","reload","menu","inventory","crouch","talk"};
 return names;}
std::string VrRuntime::actionManifest(){
 const char* const* names=bindingActionNames();std::ostringstream out;const std::string v="-v"+std::to_string(BindingsVersion)+".json";
 out<<R"({"action_sets":[{"name":"/actions/game","usage":"leftright"}],"actions":[{"name":"/actions/game/in/left","type":"skeleton","skeleton":"/skeleton/hand/left"},{"name":"/actions/game/in/right","type":"skeleton","skeleton":"/skeleton/hand/right"},{"name":"/actions/game/out/hapticleft","type":"vibration"},{"name":"/actions/game/out/hapticright","type":"vibration"})";
 for(int i=0;i<BindingActions;++i)out<<",{\"name\":\"/actions/game/in/"<<names[i]<<"\",\"type\":\""<<(i==6||i==7?"vector2":i==4||i==5?"vector1":"boolean")<<"\"}";
 out<<R"(],"default_bindings":[{"controller_type":"knuckles","binding_url":"knuckles)"<<v<<R"("},{"controller_type":"oculus_touch","binding_url":"oculus_touch)"<<v<<R"("},{"controller_type":"vive_controller","binding_url":"vive_controller)"<<v<<R"("}]})";
 return out.str();}
std::string VrRuntime::controllerBindings(const std::string& controller){
 const char* const* names=bindingActionNames();std::ostringstream file;const bool vive=controller=="vive_controller",index=controller=="knuckles";
 file<<"{\"controller_type\":\""<<controller<<R"(","name":"RawMetal gameplay","bindings":{"/actions/game":{"haptics":[{"output":"/actions/game/out/hapticleft","path":"/user/hand/left/output/haptic"},{"output":"/actions/game/out/hapticright","path":"/user/hand/right/output/haptic"}],"skeleton":[{"output":"/actions/game/in/left","path":"/user/hand/left/input/skeleton/left"},{"output":"/actions/game/in/right","path":"/user/hand/right/input/skeleton/right"}],"sources":[)";
 bool first=true;auto source=[&](const char*hand,const char*control,const char*mode,const char*slot,const char*action){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/"<<hand<<"/input/"<<control<<"\",\"mode\":\""<<mode<<"\",\"inputs\":{\""<<slot<<"\":{\"output\":\"/actions/game/in/"<<action<<"\"}}}";};
 // A stick: its position and its click (pressed in).
 auto stick=[&](const char*hand,const char*position,const char*click){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/"<<hand<<"/input/"<<(index?"thumbstick":"joystick")<<"\",\"mode\":\"joystick\",\"inputs\":{\"position\":{\"output\":\"/actions/game/in/"<<position<<"\"},\"click\":{\"output\":\"/actions/game/in/"<<click<<"\"}}}";};
 for(int side=0;side<2;++side){const char*hand=side?"right":"left";source(hand,"trigger","button","click",names[side]);source(hand,"grip",vive?"button":index?"force_sensor":"trigger",vive?"click":index?"force":"pull",names[(vive?2:4)+side]);
  if(vive)source(hand,"trackpad","trackpad","position",side?"turn":"move");
  else if(side)stick("right","turn","inventory");
  else stick("left","move","talk");}   // the left stick pressed in: push-to-talk
 if(vive){source("left","application_menu","button","click","menu");source("right","application_menu","button","click","reload");
  // Touch supplies locomotion; clicks select discrete cardinal actions.
  auto dpad=[&](const char*hand,const char*north,const char*south,const char*west){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/"<<hand<<"/input/trackpad\",\"mode\":\"dpad\",\"parameters\":{\"sub_mode\":\"click\"},\"inputs\":{\"north\":{\"output\":\"/actions/game/in/"<<north<<"\"},\"south\":{\"output\":\"/actions/game/in/"<<south<<"\"}";
   if(west)file<<",\"west\":{\"output\":\"/actions/game/in/"<<west<<"\"}";file<<"}}";};
  dpad("left","crouch","inventory","talk");dpad("right","jump","inventory",nullptr);}
 else{source("right","a","button","click","jump");source("right","b","button","click","reload");source("left",index?"a":"x","button","click","crouch");source("left",index?"b":"y","button","click","menu");}
 file<<"]}}}";
 return file.str();}
// The bindings as written: v4 file names, push-to-talk declared and held on the left stick's click, nothing else on it.
bool VrRuntime::testBindings(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto count=[](const std::string& s,const std::string& what){int n=0;for(size_t at=s.find(what);at!=std::string::npos;at=s.find(what,at+1))++n;return n;};
 const std::string manifest=actionManifest();
 check(BindingsVersion==4&&count(manifest,"-v4.json")==3&&count(manifest,"-v3.json")==0&&count(manifest,R"({"name":"/actions/game/in/talk","type":"boolean"})")==1&&std::string(bindingActionNames()[13])=="talk",
  "the manifest declares talk (boolean) and points at the v4 default bindings");
 for(const char* controller:{"oculus_touch","knuckles"}){const std::string b=controllerBindings(controller),stickName=std::string(controller)=="knuckles"?"thumbstick":"joystick";
  const std::string left=R"({"path":"/user/hand/left/input/)"+stickName+R"(","mode":"joystick","inputs":{"position":{"output":"/actions/game/in/move"},"click":{"output":"/actions/game/in/talk"}}})";
  check(count(b,left)==1&&count(b,"/actions/game/in/talk")==1&&count(b,"/actions/game/in/reload")==1&&count(b,"/actions/game/in/jump")==1&&count(b,"/actions/game/in/crouch")==1&&count(b,"/actions/game/in/menu")==1&&count(b,"/actions/game/in/inventory")==1,
   std::string(controller)+": the left stick moves and, pressed in, is push-to-talk; A jump, B reload, X crouch, Y menu, right stick click inventory unchanged");}
 {const std::string b=controllerBindings("vive_controller");check(count(b,R"("west":{"output":"/actions/game/in/talk"})")==1&&count(b,"/actions/game/in/talk")==1,"vive_controller: push-to-talk on the left trackpad's west click");}
 return ok;}
void VrRuntime::gunFrame(Point3 across,Point3 palm,Point3 forward,bool leftHand,Point3& right,Point3& up,Point3& back,float pitch){
 (void)palm;
 // Square to the knuckles: back from them, up the fist toward the index finger (across runs the other way on a left
 // hand), right = up x back (right-handed, as tracking space is). Then the muzzle is raised by the grip's rake.
 const Point3 b0=unit3(forward*-1.f);Point3 u0=across*(leftHand?-1.f:1.f);u0=unit3(u0-b0*dot3(u0,b0));
 const float c=std::cos(pitch),sn=std::sin(pitch);
 right=unit3(cross3(u0,b0));back=unit3(b0*c-u0*sn);up=unit3(u0*c+b0*sn);
}
VrRuntime::SupportPose VrRuntime::supportPose(Point3 grip,Point3 gunRight,Point3 gunUp,Point3 wrist,Point3 across,Point3 palm,Point3 forward,Point3 foreEnd,float half,float pump,bool leftHand,Point3 elbow){
 SupportPose s;const Point3 hold=wrist+forward*GripAlong+palm*GripIn;   // where the support fist closes
 // The gun: turned about the trigger hand so the fore-end's line (offset from the grip's axis by foreEnd x, y)
 // runs through the support fist. The trigger hand's up keeps the gun's roll; a few passes settle the offset.
 Point3 right=gunRight,up=gunUp,back=unit3(cross3(gunRight,gunUp));
 for(int pass=0;pass<4;++pass){const Point3 d=hold-grip-(right*foreEnd.x+up*foreEnd.y);if(len3(d)<1e-4f)break;back=unit3(d*-1.f);
  Point3 r=unit3(cross3(up,back));if(dot3(r,r)<.5f)r=unit3(right-back*dot3(right,back));if(dot3(r,r)<.5f)break;right=r;up=unit3(cross3(back,right));}
 s.gunRight=right;s.gunUp=up;s.gunBack=back;
 const Point3 muzzle=back*-1.f;
 // On the fore-end's line, where along it the hand is, held within the wood (slid back with the pump).
 const Point3 centre=grip+right*foreEnd.x+up*foreEnd.y+back*(foreEnd.z+pump);
 const float t=std::clamp(dot3(hold-centre,muzzle),-half*.85f,half*.85f);const Point3 onWood=centre+muzzle*t;
 // The knuckles: square to the wood and pointing away from the elbow, so the forearm comes up to the wrist from
 // where the arm is and the hand wraps the wood from that side (an elbow under it: the palm under the wood, the
 // fingers up its far side, the thumb along the near side).
 Point3 f0=onWood-elbow;f0=f0-muzzle*dot3(f0,muzzle);
 if(len3(f0)<1e-3f){f0=right*(leftHand?1.f:-1.f)+up;f0=f0-muzzle*dot3(f0,muzzle);}
 f0=unit3(f0);
 // The real hand's roll about the wood turns that by at most SupportRoll either way: the more the real knuckles
 // lie square to the wood the more it counts (a controller held upright with the knuckles along the wood counts
 // for nothing), and it fades out as the real hand turns past a quarter turn from the arm's way, so it never jumps.
 Point3 fr=forward-muzzle*dot3(forward,muzzle);const float frLength=len3(fr);float phi=0;
 if(frLength>1e-4f){fr=fr*(1/frLength);const float raw=std::atan2(dot3(cross3(f0,fr),muzzle),dot3(f0,fr));
  const float weight=std::clamp((frLength-.2f)/.3f,0.f,1.f),fade=std::clamp((kPi-std::fabs(raw))/(kPi*.5f),0.f,1.f);
  phi=std::clamp(raw,-SupportRoll,SupportRoll)*weight*weight*(3-2*weight)*fade;}
 const Point3 f=unit3(f0*std::cos(phi)+cross3(muzzle,f0)*std::sin(phi));
 // The fist's channel along the wood, index to the muzzle (across: toward the index on a right hand, the little
 // finger on a left); the palm from the hand frame's handedness (across = forward x palm, so palm = across x forward).
 const Point3 a=muzzle*(leftHand?-1.f:1.f),p=unit3(cross3(a,f));
 s.across=a;s.palm=p;s.forward=f;s.wrist=onWood-(f*GripAlong+p*GripIn);
 return s;
}
VrRuntime::SupportPose VrRuntime::pistolSupportPose(Point3 grip,Point3 gunRight,Point3 gunUp,Point3 gunBack,bool leftHand,float pitch){
 SupportPose s;s.gunRight=gunRight;s.gunUp=gunUp;s.gunBack=gunBack;
 const Point3 muzzle=gunBack*-1.f,toGun=gunRight*(leftHand?1.f:-1.f);   // from the support side toward the gun hand
 // Up the grip as the gun hand's fist runs (the rake back from the gun's up), knuckles forward and round the front
 // of the gun hand's fingers.
 const Point3 c=unit3(gunUp*std::cos(pitch)-gunBack*std::sin(pitch));
 Point3 f=muzzle*std::cos(.6f)+toGun*std::sin(.6f);f=unit3(f-c*dot3(f,c));
 const Point3 a=c*(leftHand?-1.f:1.f),p=unit3(cross3(a,f));
 const Point3 hold=grip-toGun*.035f-c*.03f+muzzle*.005f;
 s.across=a;s.palm=p;s.forward=f;s.wrist=hold-(f*GripAlong+p*GripIn);
 return s;
}
std::array<Point3,4> VrRuntime::scriptHand(Point3 hold,Point3 forward,Point3 channel,bool leftHand){
 const Point3 f=unit3(forward);Point3 c=unit3(channel-f*dot3(channel,f));
 std::array<Point3,3> frame{};handFrame(f,c,leftHand?0:1,frame);const Point3 palm=frame[1];
 const Point3 wrist=hold-f*GripAlong-palm*GripIn,middle=wrist+f*.09f;
 return {wrist,middle+c*.022f,middle,middle-c*.04f-f*.012f};
}
void VrRuntime::playHands(Game& game,InputState& input,bool focused,bool menuOpen){
 auto&M=*m;const int gunSide=M.side[1];
 const auto origin=game.chunkOffset(game.level());auto local=[&](Point3 w){w.x-=origin.x;w.y-=origin.y;return w;};
 for(int role=0;role<2;++role)input.vrGrip[size_t(role)]=focused&&M.hands[size_t(role)].tracked&&M.rawGrip[size_t(role)];
 const std::array<bool,2> grips=input.vrGrip;
 const bool armed=game.weaponEquipped()&&!game.unarmed();
 const bool firearm=armed&&playerFirearm(game.activeFirearm());
 // Holsters on her body (avatarSlotAt): the gun hand gripping over a shoulder draws the shotgun from her back,
 // at the hip the sidearm riding there (the handgun or the MAC-11: Game::hipHolster). A drawn gun stays in the hand when the grip is let go; let go back at its own holster
 // (after leaving it) and it is put away there. A grip spent on a holster does not also grab or use.
 // The support hand at the belt pouch with a gun drawn takes a magazine or shell (VrGuns.cpp); that grip, a
 // magazine or shell in the hand and a hand on the slide are the gun's, not a grab either.
 {const AvatarPose body=avatar(game);const Point3 at=body.hands[size_t(gunSide)].wrist;const bool grip=input.vrGrip[1],was=M.previousGrip[1];
  const int slot=M.hands[1].tracked?avatarSlotAt(body,at):0;
  const int heldSlot=!armed?0:game.equippedWeapon()==WeaponId::Shotgun?1:sidearm(game.equippedWeapon())?2:0;
  const int hipGun=game.hipHolster(armed?game.equippedWeapon():WeaponId::Fists);
  if(M.drawnFrom&&slot!=M.drawnFrom)M.leftHolster=true;
  if(grip&&!was&&!menuOpen&&!game.holdingClutter()&&slot){
   const WeaponId want=slot==1?WeaponId::Shotgun:hipGun?WeaponId(hipGun-1):WeaponId::Fists;
   if(heldSlot==slot){M.drawnFrom=slot;M.leftHolster=true;M.consumed[1]=true;}   // gripping it at its own holster: let go to put it away
   else if(want!=WeaponId::Fists&&game.ownsWeapon(want)){input.weaponSlot=int(want)+1;M.drawnFrom=slot;M.leftHolster=false;M.consumed[1]=true;game.vrHaptic(1,.03f,120,.35f,"draw");}}
  if(!grip&&was&&M.drawnFrom&&heldSlot==M.drawnFrom&&slot==M.drawnFrom&&M.leftHolster&&!menuOpen){input.weaponSlot=int(WeaponId::Fists)+1;M.drawnFrom=0;game.vrHaptic(1,.03f,100,.3f,"holster");}
  if(!armed&&!grip)M.drawnFrom=0;
  const Point3 fist0=local(M.hands[0].handFrame?M.hands[0].gunOrigin:M.hands[0].position);
  const bool pouch=M.hands[0].tracked&&firearm&&avatarPouchAt(body,body.hands[size_t(M.side[0])].wrist);
  if(grips[0]&&!M.previousGrip[0]&&(pouch||game.vrRackZone(fist0)))M.consumed[0]=true;
  if(grips[0]&&(game.vrSupportBusy()||game.vrRacking()))M.consumed[0]=true;
  for(int role=0;role<2;++role){if(!input.vrGrip[size_t(role)])M.consumed[size_t(role)]=false;if(M.consumed[size_t(role)]){input.vrGrip[size_t(role)]=false;input.use=input.vrGrip[0]||input.vrGrip[1];}M.previousGrip[size_t(role)]=grips[size_t(role)];}}
 // The support grip on the gun. The shotgun: the support hand along the barrel takes the fore-end and the gun is
 // turned (in tracking space, supportPose) so its fore-end runs through that hand. The pistol: the support hand
 // closing near the gun hand (and not on the slide) wraps the grip; the pistol still aims from the gun hand.
 auto&gunHand=M.hands[1];bool support=false,legacy=false;Point3 legacyAim{};
 M.support=false;M.supportKind=0;M.pump=game.vrPumpOffset();
 const bool canSupport=focused&&M.hands[0].tracked&&gunHand.tracked&&armed&&!game.holdingClutter()&&!menuOpen&&!game.vrSupportBusy();
 if(canSupport&&game.activeFirearm()==WeaponId::Shotgun){
  const Point3 right=gunHand.handFrame?gunHand.gunOrigin:gunHand.position,left=M.hands[0].handFrame?M.hands[0].gunOrigin:M.hands[0].position,handForward=(gunHand.handFrame?gunHand.gunBasis[2]:gunHand.basis[2])*-1;
  if(foregripSupport(right,left,handForward,grips[0])){support=true;if(M.track[0].valid&&M.track[1].valid)M.supportKind=1;else{legacy=true;legacyAim=unit3(left-right);}}}
 else if(canSupport&&sidearm(game.activeFirearm())&&grips[0]&&M.track[0].valid&&M.track[1].valid){
  const Point3 f0=M.hands[0].gunOrigin,f1=gunHand.gunOrigin;
  if(len3(f0-f1)<.13f&&!game.vrRackZone(local(f0))&&!game.vrRacking()){support=true;M.supportKind=2;}}
 M.support=M.supportKind!=0;
 M.applySupport(game);M.applyRecoil(game);
 // The hands the game uses are the avatar's: where her arms can reach (solveAvatar pulls a wrist past her
 // reach in to it), so what a hand grabs, pushes or shoots from is where her hand is drawn.
 const AvatarPose body=avatar(game);const AvatarSolve solved=solveAvatar(body,game.elapsed());
 auto reachShift=[&](int role){const int side=M.side[size_t(role)];return solved.wrist[size_t(side)]-body.hands[size_t(side)].wrist;};
 // The shot: from the drawn gun's muzzle toward its sights' zero (Game::vrShotLine); without skeletal input, from
 // the controller along its pointing (the line between the hands with the support hand on).
 const auto&basis=gunHand.handFrame?gunHand.gunBasis:gunHand.basis;
 const Point3 grip=(gunHand.handFrame?gunHand.gunOrigin:gunHand.position)+(gunHand.tracked?reachShift(1):Point3{});
 Point3 aim=grip,aimDir=basis[2]*-1.f;
 if(gunHand.handFrame&&firearm)Game::vrShotLine(Game::vrGunGeometry(game.activeFirearm()),grip,basis[0],basis[1],basis[2],aim,aimDir);
 else if(legacy)aimDir=legacyAim;
 game.setVrAim(local(aim),aimDir,gunHand.tracked&&focused,support);
 for(int role=0;role<2;++role){auto hand=M.hands[size_t(role)].position+(M.hands[size_t(role)].tracked?reachShift(role):Point3{});game.setVrHand(role,local(hand),M.hands[size_t(role)].basis[2]*-1,M.hands[size_t(role)].tracked&&focused,M.vel[size_t(role)],M.hands[size_t(role)].basis[1]);}
 // The gun and the hands for the hand reloads and the pump (VrGuns.cpp): as tracked, not pulled in to her reach,
 // so a magazine, a shell or a pump stroke is where the player's own hands put it against the gun even at full
 // stretch (the drawn arm stops at her reach; the stroke still counts).
 Game::VrGunInput in;in.valid=focused&&gunHand.tracked&&gunHand.handFrame&&firearm;in.grip=local(gunHand.handFrame?gunHand.gunOrigin:gunHand.position);in.right=basis[0];in.up=basis[1];in.back=basis[2];
 for(int role=0;role<2;++role){const auto&h=M.hands[size_t(role)];in.fist[size_t(role)]=local(h.handFrame?h.gunOrigin:h.position);in.velocity[size_t(role)]=M.vel[size_t(role)];}
 in.grips=grips;in.pouch=M.hands[0].tracked&&!M.hands[0].snapped&&avatarPouchAt(body,body.hands[size_t(M.side[0])].wrist);
 game.setVrGunInput(in);
}
bool VrRuntime::script(Game& game,InputState& input,const VrScript& s,float dt){
 (void)dt;current=this;auto&M=*m;
 const auto&cfg=game.settings();M.leftHanded=cfg.vrLeftHanded;M.side=M.leftHanded?std::array<int,2>{1,0}:std::array<int,2>{0,1};M.seated=cfg.vrSeated;
 // The head: -z where it looks, level (no roll).
 const Point3 z=unit3(s.headForward*-1.f);Point3 x=unit3(cross3({0,1,0},z));if(dot3(x,x)<.5f)x={1,0,0};const Point3 y=cross3(z,x);
 auto&hp=M.poses[0].mDeviceToAbsoluteTracking;hp={};const Point3 axes[3]={x,y,z};
 for(int c=0;c<3;++c){hp.m[0][c]=axes[c].x;hp.m[1][c]=axes[c].y;hp.m[2][c]=axes[c].z;}hp.m[0][3]=s.head.x;hp.m[1][3]=s.head.y;hp.m[2][3]=s.head.z;
 M.poses[0].bPoseIsValid=true;
 if(!M.calibrated){M.yaw=game.player().angle;M.calibrated=true;M.bodyReady=false;M.headStartY=s.head.y;}
 M.previousHead=s.head;M.scale=avatarScale(M.headStartY,M.seated);
 const Point3 forward=rotateTracking(z*-1.f,M.yaw);
 input.vrTracked=true;input.vrYaw=std::atan2(forward.y,forward.x);input.vrPitch=140.f*std::asin(std::clamp(forward.z,-1.f,1.f));input.vrEye=std::clamp(Player::StandingEye+(s.head.y-M.headStartY)*M.scale,.15f,2.5f);
 input.mouseDx=input.mouseDy=0;
 if(!M.bodyReady){M.bodyYaw=input.vrYaw;M.bodyReady=true;}
 for(int role=0;role<2;++role){const int side=M.side[size_t(role)];auto&h=M.hands[size_t(role)];h={};h.left=side==0;M.track[size_t(role)].valid=false;M.rawGrip[size_t(role)]=false;M.vel[size_t(role)]={};
  h.tracked=s.tracked[size_t(side)];if(!h.tracked)continue;const auto&j=s.joints[size_t(side)];
  M.fromJoints(role,h,game,j[0],j[1],j[2],j[3]);h.position=h.gunOrigin;h.basis=h.gunBasis;
  h.curls.fill(s.grip[size_t(side)]?.85f:.1f);h.curls[1]=s.trigger[size_t(side)]?1.f:.1f;
  M.rawGrip[size_t(role)]=s.grip[size_t(side)];M.vel[size_t(role)]=rotateTracking(s.velocity[size_t(side)],M.yaw)*M.scale;}
 const int gunSide=M.side[1],offSide=M.side[0];
 input.fire=M.hands[1].tracked&&s.trigger[size_t(gunSide)];input.vrTrigger={M.hands[0].tracked&&s.trigger[size_t(offSide)],input.fire};input.reload=s.reload;input.use=s.grip[0]||s.grip[1];
 const bool menuOpen=game.titleScreen()||game.paused()||game.inventoryOpen()||game.consoleOpen();
 playHands(game,input,true,menuOpen);
 return true;
}
Point3 VrRuntime::lazyFollow(Point3 anchor,Point3 forward,float dt,float slack){
 const Point3 f=unit3(forward);if(dot3(f,f)<.5f)return anchor;const Point3 a=unit3(anchor);if(dot3(a,a)<.5f)return f;
 const float c=std::clamp(dot3(a,f),-1.f,1.f),angle=std::acos(c);
 if(angle>1.745f)return f;if(angle<=slack)return a;
 const float step=std::min(angle-slack,dt*(.3f+4.f*(angle-slack)));
 const Point3 across=unit3(f-a*c);if(dot3(across,across)<.5f)return f;
 return unit3(a*std::cos(step)+across*std::sin(step));
}
// A hand's grip from SteamVR Input: the bound click and squeeze actions alone while either is bound (the legacy
// controller state is not asked then: on some controllers its grip bit stays set while the hand only rests on the
// handle, which kept a prop held after the grip was let go); the squeeze closes past 0.6 and opens below 0.45.
bool VrRuntime::gripHeld(bool was,bool clickActive,bool click,bool squeezeActive,float squeeze,bool legacy){
 if(clickActive||squeezeActive)return (clickActive&&click)||(squeezeActive&&squeeze>(was?.45f:.6f));
 return legacy;}
bool VrRuntime::testGrip(){
 // Bound squeeze: closes past 0.6, holds to 0.45, opens below; a stuck legacy bit is ignored.
 if(gripHeld(false,false,false,true,.55f,true)||!gripHeld(false,false,false,true,.7f,false)||!gripHeld(true,false,false,true,.5f,false)||gripHeld(true,false,false,true,.4f,true))return false;
 // Bound click (Vive): its state alone; nothing bound: the legacy bit.
 if(!gripHeld(false,true,true,false,0,false)||gripHeld(true,true,false,false,0,true)||!gripHeld(false,false,false,false,0,true)||gripHeld(true,false,false,false,0,false))return false;
 return true;}
bool VrRuntime::testDetection(){
 // The registry key must carry real backslashes (the 1.0.4-1.5.5 bug).
 if(std::wstring(SteamKey)!=std::wstring(L"Software")+L'\\'+L"Valve"+L'\\'+L"Steam")return false;
 std::set<std::wstring> disk;int queried=0;int answer=1;
 auto probe=[&](std::wstring steam,std::string vrpath){VrProbe p;p.steamPath=steam;p.vrpathJson=vrpath;
  p.exists=[&](const std::filesystem::path& f){return disk.count(f.lexically_normal().wstring())>0;};
  p.query=[&](const std::filesystem::path&,std::string&){++queried;return answer;};return detect(p);};
 auto add=[&](std::filesystem::path f){disk.insert(f.lexically_normal().wstring());};
 // Nothing installed: no VR row, and a reason.
 auto none=probe(L"",std::string());if(none.offered||none.reason.empty()||queried)return false;
 // Steam present, SteamVR present, headset seen: the choice is offered.
 add(std::filesystem::path(L"C:/Steam")/L"steamapps/common/SteamVR/bin/win64/openvr_api.dll");
 auto present=probe(L"C:/Steam",std::string());if(!present.offered||present.runtime.empty()||queried!=1)return false;
 // Headset off: hidden, with a reason that names the cause.
 answer=0;auto off=probe(L"C:/Steam",std::string());if(off.offered||off.reason.find("no headset")==std::string::npos)return false;
 // The DLL will not load: hidden, the path is in the reason.
 answer=-1;auto broken=probe(L"C:/Steam",std::string());if(broken.offered||broken.reason.find("openvr_api.dll")==std::string::npos)return false;
 // SteamVR in a second library, found through openvrpaths.vrpath even with Steam's own folder empty.
 answer=1;add(std::filesystem::path(L"D:/Games/SteamLibrary/steamapps/common/SteamVR")/L"bin/win64/openvr_api.dll");
 const std::string vrpath="{\n\t\"config\" : [ \"C:\\\\Steam\\\\config\" ],\n\t\"runtime\" : [\n\t\t\"D:\\\\Games\\\\SteamLibrary\\\\steamapps\\\\common\\\\SteamVR\"\n\t],\n\t\"version\" : 1\n}";
 if(vrpathRuntime(vrpath)!="D:\\Games\\SteamLibrary\\steamapps\\common\\SteamVR")return false;
 auto library=probe(L"",vrpath);if(!library.offered||library.runtime.find("SteamLibrary")==std::string::npos)return false;
 if(!vrpathRuntime("{\"runtime\":[]}").empty())return false;
 return true;
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
// The support hand on the gun (supportPose, pistolSupportPose), numerically, in tracking space and through the mirror.
bool VrRuntime::testSupport(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto det=[](Point3 a,Point3 b,Point3 c){return dot3(a,cross3(b,c));};
 auto angle=[](Point3 a,Point3 b){return std::acos(std::clamp(dot3(unit3(a),unit3(b)),-1.f,1.f));};
 // About an axis through the origin by t (Rodrigues).
 auto spin=[](Point3 v,Point3 k,float t){k=unit3(k);return v*std::cos(t)+cross3(k,v)*std::sin(t)+k*(dot3(k,v)*(1-std::cos(t)));};
 // The gun in the right hand, level, pointing ahead (-z): right x, up y, back +z. The support (left) elbow below,
 // left of and behind the wood, as when the gun is up at the eye.
 const Point3 grip{0,1.2f,-.3f},right{1,0,0},up{0,1,0},foreEnd{0,-.012f,-.36f};const float half=.09f;
 const Point3 muzzle{0,0,-1},line=grip+right*foreEnd.x+up*foreEnd.y+Point3{0,0,1}*foreEnd.z,elbow{-.22f,.88f,-.12f};
 auto place=[&](Point3 f,Point3 p,Point3 holdAt){return holdAt-(f*GripAlong+p*GripIn);};
 auto handAt=[&](Point3 forward,Point3 channel,bool leftHand,Point3 hold,Point3& w,Point3& a,Point3& p,Point3& f){const auto j=scriptHand(hold,forward,channel,leftHand);
  std::array<Point3,3> fr{};handFrame(j[2]-j[0],j[1]-j[3],leftHand?0:1,fr);a=fr[0];p=fr[1];f=fr[2];w=j[0];};
 auto armWay=[&](Point3 at){Point3 d=at-elbow;d=d-muzzle*dot3(d,muzzle);return unit3(d);};
 auto sane=[&](const SupportPose& s,bool leftHand,Point3 gunMuzzle,float& bend){const Point3 hold=s.wrist+s.forward*GripAlong+s.palm*GripIn;
  Point3 d=hold-elbow;d=d-gunMuzzle*dot3(d,gunMuzzle);bend=std::max(bend,angle(s.forward,d));
  const Point3 palmAt=hold-s.palm*GripIn;   // the palm's middle: the wood's axis lies ahead of it
  return std::fabs(dot3(s.across,gunMuzzle)*(leftHand?-1.f:1.f)-1)<1e-3f&&dot3(s.palm,unit3(hold-palmAt))>.999f&&std::fabs(dot3(s.palm,gunMuzzle))<1e-3f&&det(s.across,s.palm,s.forward)<-.999f;};
 // A Touch controller held as it is held under a fore-end: the handle upright, the knuckles along the wood.
 {Point3 w,a,p,f;handAt(muzzle,up,true,line,w,a,p,f);const auto s=supportPose(grip,right,up,w,a,p,f,foreEnd,half,0,true,elbow);float bend=0;
  const bool ok1=sane(s,true,muzzle,bend);
  check(ok1&&bend<1e-3f&&dot3(s.wrist-line,up)<-.03f&&dot3(s.forward,armWay(line))>.999f,"the controller upright under the wood: the fist round it, index to the muzzle, palm to the wood, knuckles away from the elbow, the wrist under the wood (not over it)");
  check(len3(s.gunBack-Point3{0,0,1})<1e-4f&&det(s.gunRight,s.gunUp,s.gunBack)>.999f,"the gun keeps its aim and its right-handed frame");}
 // The controller rolled a full turn about the wood: the drawn hand turns with it no further than SupportRoll from
 // the arm's way, smoothly (no flip), always round the wood.
 {float bend=0,jump=0;bool all=true,first=true;Point3 previous{};int followed=0;
  const Point3 square=armWay(line);   // knuckles already square to the wood, the arm's way
  for(int k=0;k<=120;++k){const float t=float(k)*kPi/60;const Point3 f=spin(square,muzzle,t);Point3 w,a,p,ff;handAt(f,muzzle,true,line,w,a,p,ff);
   const auto s=supportPose(grip,right,up,w,a,p,ff,foreEnd,half,0,true,elbow);all&=sane(s,true,muzzle,bend);
   const float wanted=std::remainder(t,2*kPi);if(std::fabs(wanted)<SupportRoll-.01f)followed+=angle(s.forward,ff)<.02f;
   if(!first)jump=std::max(jump,angle(s.palm,previous));previous=s.palm;first=false;}
  check(all&&bend<=SupportRoll+1e-3f&&jump<.07f&&followed>=20,"rolled a full turn round the wood: the hand follows within 35 degrees of the arm's way (worst "+std::to_string(int(bend*57.3f+.5f))+"), never more than 4 degrees a 3-degree step");}
 // Swung off the wood's line by up to 50 degrees (and rolled): laid along the barrel, smooth, the hand's handedness
 // kept in tracking space and mapped to game space.
 {float jump=0,offAxis=0,bend=0;bool frames=true,mirrored=true;Point3 previous{};bool first=true;
  for(int k=-50;k<=50;++k){const float swing=float(k)*kPi/180,roll=float(k)*kPi/360;
   const Point3 f=spin(spin(muzzle,up,swing),muzzle,roll),c=spin(spin(up,up,swing),muzzle,roll);Point3 w,a,p,ff;handAt(f,c,true,line+Point3{.012f,.008f,0},w,a,p,ff);
   const auto s=supportPose(grip,right,up,w,a,p,ff,foreEnd,half,0,true,elbow);
   offAxis=std::max(offAxis,len3(s.across-s.gunBack));   // the left fist's channel (-across) along the re-aimed muzzle
   frames&=sane(s,true,s.gunBack*-1.f,bend);
   for(float yaw:{0.f,1.3f}){const Point3 ga=rotateTracking(s.across,yaw),gp=rotateTracking(s.palm,yaw),gb=rotateTracking(s.forward*-1.f,yaw);
    const Point3 ua=rotateTracking(a,yaw),up2=rotateTracking(p,yaw),ub=rotateTracking(ff*-1.f,yaw);mirrored&=(det(ga,gp,gb)<0)==(det(ua,up2,ub)<0);}
   if(!first)jump=std::max(jump,angle(s.palm,previous));previous=s.palm;first=false;}
  check(offAxis<1e-3f,"swung off the line, the fist is laid along the barrel, index to the muzzle");
  check(frames&&mirrored,"the snapped frame keeps the hand's handedness, in tracking space and mapped to game space");
  check(jump<.07f,"the palm moves on smoothly through the swing, no flip (worst "+std::to_string(int(jump*57.3f+.5f))+" degrees for a 1-degree step of the swing)");}
 // The gun turns about the trigger hand so the fore-end runs through the support fist.
 {const Point3 hold=grip+Point3{.08f,.05f,-.38f};Point3 w,a,p,f;handAt(spin(muzzle,up,.3f),up,true,hold,w,a,p,f);
  const auto s=supportPose(grip,right,up,w,a,p,f,foreEnd,half,0,true,elbow);
  const Point3 centre=grip+s.gunRight*foreEnd.x+s.gunUp*foreEnd.y;const Point3 d=hold-centre;const float off=len3(d-s.gunBack*dot3(d,s.gunBack));
  check(off<2e-3f&&dot3(d,s.gunBack)<0&&det(s.gunRight,s.gunUp,s.gunBack)>.999f&&dot3(s.gunUp,up)>.95f,"the gun is aimed so its fore-end runs through the support fist, keeping its roll");}
 // Along the fore-end: a hand off its ends is held on the wood; the pump slides the wood (and its ends) back.
 {const Point3 lf=muzzle,lp=up;Point3 w,a,p,f;handAt(lf,lp,true,line+muzzle*.4f,w,a,p,f);const auto s=supportPose(grip,right,up,w,a,p,f,foreEnd,half,0,true,elbow);
  const Point3 hold=s.wrist+s.forward*GripAlong+s.palm*GripIn;check(std::fabs(dot3(hold-line,muzzle)-half*.85f)<1e-3f,"a hand past the fore-end's front is held at its front");
  handAt(lf,lp,true,line-muzzle*.3f,w,a,p,f);const auto q=supportPose(grip,right,up,w,a,p,f,foreEnd,half,.07f,true,elbow);const Point3 hq=q.wrist+q.forward*GripAlong+q.palm*GripIn;
  check(std::fabs(dot3(hq-line,muzzle)-(-half*.85f-.07f))<1e-3f,"pumped back 7 cm, the wood's rear end is 7 cm further back");}
 // A left-handed player: the right hand on the fore-end, its elbow on the right.
 {const Point3 rightElbow{.22f,.88f,-.12f};Point3 w,a,p,f;handAt(muzzle,up,false,line,w,a,p,f);const auto s=supportPose(grip,right,up,w,a,p,f,foreEnd,half,0,false,rightElbow);
  Point3 d=line-rightElbow;d=unit3(d-muzzle*dot3(d,muzzle));
  check(std::fabs(dot3(s.across,muzzle)-1)<1e-3f&&dot3(s.forward,d)>.999f&&dot3(s.wrist-line,up)<-.03f&&det(s.across,s.palm,s.forward)<-.999f,"left-handed: the right hand round the wood, index to the muzzle, from its own elbow's side");}
 // Two hands on the pistol: the support fist on the support side of the gun hand, up the grip, palm to the gun.
 for(bool leftHand:{true,false}){const Point3 g{0,1.3f,-.35f};Point3 r,u,b;
  {Point3 w,a,p,f;handAt(muzzle,up,!leftHand,g,w,a,p,f);gunFrame(a,p,f,!leftHand,r,u,b);}
  const auto s=pistolSupportPose(g,r,u,b,leftHand);const Point3 hold=s.wrist+s.forward*GripAlong+s.palm*GripIn,toGun=r*(leftHand?1.f:-1.f);
  check(len3(hold-g)<.06f&&dot3(hold-g,toGun)<-.02f&&dot3(s.palm,toGun)>.6f&&angle(s.across*(leftHand?-1.f:1.f),u)<.35f&&dot3(s.forward,b)<-.6f&&det(s.across,s.palm,s.forward)<-.999f,
   std::string(leftHand?"right-handed":"left-handed")+" two-handed pistol: the support fist beside the gun hand, up the grip, palm to the gun, knuckles forward");}
 return ok;
}
// The guns in scripted hands (VrRuntime::script, no headset): the frame from the skeleton, the sights and the shot.
bool VrRuntime::testGuns(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto angle=[](Point3 a,Point3 b){return std::acos(std::clamp(dot3(unit3(a),unit3(b)),-1.f,1.f));};
 auto deg=[](float r){return std::to_string(r*57.2958f).substr(0,5);};
 auto spin=[](Point3 v,Point3 k,float t){k=unit3(k);return v*std::cos(t)+cross3(k,v)*std::sin(t)+k*(dot3(k,v)*(1-std::cos(t)));};
 // The hand that holds the gun with its sights on the line from the eye to a target: the gun frame wanted, back to
 // the hand that makes it (gunFrame undone), its joints.
 auto aimedHand=[&](Point3 eye,Point3 target,const Game::VrGunGeometry& g,bool leftHand,float eyeRelief,float pitch){
  const Point3 sight=unit3(target-eye);const Point3 dg=unit3(g.frontSight-g.rearSight);   // the sight line in the gun frame
  // The gun frame: level (right horizontal), its sight line along the eye's line.
  Point3 back0=sight*-1.f,right0=unit3(cross3({0,1,0},back0));Point3 up0=cross3(back0,right0);
  // Turn so the frame's sight direction (dg in it) lands on the eye's line: dg is a small tilt of -z about x.
  const float tilt=std::atan2(dg.y,-dg.z);Point3 back=spin(back0,right0,-tilt),up=spin(up0,right0,-tilt);
  const Point3 grip=eye+sight*eyeRelief-(right0*g.rearSight.x+up*g.rearSight.y+back*g.rearSight.z);
  // Undo gunFrame's raise: the knuckles' way and the fist's channel.
  const float c=std::cos(pitch),sn=std::sin(pitch);const Point3 b0=back*c+up*sn,u0=up*c-back*sn;
  return scriptHand(grip,b0*-1.f,u0,leftHand);};
 const Point3 eye{0,Player::StandingEye,0};
 for(WeaponId gun:{WeaponId::Handgun,WeaponId::Shotgun,WeaponId::MachinePistol})for(bool lefty:{false,true}){
  if(gun==WeaponId::Shotgun&&lefty)continue;
  const bool magGun=Game::vrMagazineGun(gun);const float pitch=gunPitch(gun);
  const std::string name=std::string(gun==WeaponId::Handgun?"handgun":gun==WeaponId::MachinePistol?"MAC-11":"shotgun")+(lefty?", left-handed":"");
  Game game=Game::vrGunScene(gun,{20,6},lefty);VrRuntime runtime;InputState input{};
  const auto g=Game::vrGunGeometry(gun);const Point3 target=eye+Point3{.35f,-.25f,-10.f};
  VrScript pose;pose.head=eye;pose.headForward=unit3(target-eye);const int gunHand=lefty?0:1,other=1-gunHand;
  pose.joints[size_t(gunHand)]=aimedHand(eye,target,g,lefty,gun==WeaponId::Handgun?.32f:gun==WeaponId::MachinePistol?.30f:.18f,pitch);
  pose.joints[size_t(other)]=scriptHand(eye+Point3{lefty?.3f:-.3f,-.55f,-.1f},{0,0,-1},{0,1,0},!lefty);pose.tracked={true,true};
  for(int k=0;k<4;++k){runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);}
  const auto&h=runtime.hands()[1];const auto origin=game.chunkOffset(game.level());auto local=[&](Point3 w){w.x-=origin.x;w.y-=origin.y;return w;};
  auto at=[&](Point3 p){return local(h.gunOrigin)+h.gunBasis[0]*p.x+h.gunBasis[1]*p.y+h.gunBasis[2]*p.z;};
  const Point3 eyeAt=local(runtime.m->world(eye,game)),targetAt=local(runtime.m->world(target,game));
  const Point3 rear=at(g.rearSight),front=at(g.frontSight),bore=h.gunBasis[2]*-1.f;
  const Point3 sightLine=unit3(front-rear);
  // The eye on the sight line, the sight line on the target.
  auto offLine=[](Point3 p,Point3 a,Point3 d){const Point3 v=p-a;return len3(v-d*dot3(v,d));};
  check(offLine(eyeAt,rear,sightLine)<.003f&&offLine(targetAt,rear,sightLine)<.02f*runtime.m->scale,name+": raised to the eye, the eye looks through both sights onto the target");
  // The bore follows the hand: the grip angle off the knuckles, the model's grip along the fist.
  {const auto&t=runtime.m->track[1];const Point3 knuckles=rotateTracking(t.forward,runtime.m->yaw),channel=rotateTracking(t.across*(lefty?-1.f:1.f),runtime.m->yaw);
   const Point3 magAxis=unit3(h.gunBasis[0]*g.magAxis.x+h.gunBasis[1]*g.magAxis.y+h.gunBasis[2]*g.magAxis.z);
   check(std::fabs(angle(bore,knuckles)-pitch)<.01f&&dot3(h.gunBasis[1],channel)>.95f&&(!magGun||angle(magAxis,channel)<.09f),name+": the bore runs "+deg(angle(bore,knuckles))+" degrees above the knuckles and the gun's up along the fist"+(magGun?" (the magazine "+deg(angle(magAxis,channel))+" degrees off the fist's channel)":""));
   check(h.gunBasis[1].z>.9f,name+": the gun is upright in the hand");}
  // The sight line and the bore agree; the trace leaves from the muzzle along the bore and meets the sight line at the zero.
  Point3 o,d;Game::vrShotLine(g,local(h.gunOrigin),h.gunBasis[0],h.gunBasis[1],h.gunBasis[2],o,d);
  // The MAC-11's sights are cut 1.1 degrees down onto the bore on the model (rear 4 mm higher than front): 1.5 for it.
  const float sightTolerance=gun==WeaponId::MachinePistol?.026f:.017f;
  check(angle(sightLine,bore)<sightTolerance&&angle(d,bore)<sightTolerance&&angle(d,sightLine)<.01f,name+": sight line, bore and shot within "+(gun==WeaponId::MachinePistol?"1.5 degrees":"a degree")+" ("+deg(angle(sightLine,bore))+", "+deg(angle(d,bore))+", "+deg(angle(d,sightLine))+")");
  // Fire: the trace the game records.
  // A full-auto gun throws each round somewhere on its spread's cone (Game::shotSpread): that much more is allowed.
  const float cone=playerWeaponDef(gun).cycle.fullAuto?game.shotSpread():0.f;
  pose.trigger[size_t(gunHand)]=true;runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);pose.trigger[size_t(gunHand)]=false;
  const bool fired=!game.shotRays().empty();const ShotRay ray=fired?game.shotRays().back():ShotRay{};
  const float atTarget=fired?offLine(targetAt,ray.origin,ray.direction):1.f;
  check(fired&&len3(ray.origin-at(g.muzzle))<.002f&&angle(ray.direction,bore)<sightTolerance+cone&&atTarget<.025f+10.f*cone,name+": the shot leaves from the muzzle along the bore and passes "+std::to_string(int(atTarget*1000+.5f))+" mm from the point the sights were on at 10 m");
  // The recoil turns the gun in the hand, not the view; the view's yaw and pitch are the head's.
  for(int k=0;k<3;++k){runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);}
  const auto&k2=runtime.hands()[1];check(game.vrRecoil().rise>.02f&&angle(k2.gunBasis[2]*-1.f,bore)>.02f&&std::fabs(game.player().pitch-input.vrPitch)<1e-4f,name+": the kick lifts the gun in the hand while the view stays on the head");}
 // The holsters: a grip at the hip draws the handgun and asks for a pulse; let go back there, it goes back.
 {Game game=Game::vrGunScene(WeaponId::Shotgun,{20,6});VrRuntime runtime;InputState input{};VrScript pose;pose.head=eye;
  pose.joints[1]=scriptHand(eye+Point3{.2f,-.45f,-.35f},{0,0,-1},{0,1,0},false);pose.joints[0]=scriptHand(eye+Point3{-.3f,-.6f,-.1f},{0,0,-1},{0,1,0},true);
  runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);
  const AvatarSolve s=solveAvatar(runtime.avatar(game),0);const float c=std::cos(runtime.m->yaw),sn=std::sin(runtime.m->yaw);
  auto untrack=[&](Point3 w){const auto origin=game.chunkOffset(game.level());w.x+=origin.x;w.y+=origin.y;const Point3 e=runtime.m->world(eye,game);const Point3 g=(w-e)*(1/runtime.m->scale);return eye+Point3{-g.x*sn+g.y*c,g.z,-(g.x*c+g.y*sn)};};
  const Point3 hipWrist=untrack(s.hipSlot);auto j=scriptHand(hipWrist,{0,-1,0},{0,0,-1},false);const Point3 shift=hipWrist-j[0];for(auto&p:j)p=p+shift;pose.joints[1]=j;
  runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);
  pose.grip[1]=true;runtime.script(game,input,pose,1.f/60);const int slot=input.weaponSlot;bool pulse=false;for(const auto&p:game.vrHaptics())pulse|=std::string(p.cause)=="draw"&&p.hand==1;
  game.update(input,1.f/60);
  check(slot==int(WeaponId::Handgun)+1&&pulse,"gripping at the hip draws the handgun, with a pulse on the gun hand");
  // The MAC-11 drawn last rides the hip instead (Game::hipHolster): the avatar wears it there and the grip draws it.
  {Game mac=Game::vrGunScene(WeaponId::MachinePistol,{32,32});mac.equipWeapon(WeaponId::Shotgun);VrRuntime r2;InputState in2{};pose.grip[1]=false;
   r2.script(mac,in2,pose,1.f/60);mac.update(in2,1.f/60);const int worn=r2.avatar(mac).hip;pose.grip[1]=true;in2={};r2.script(mac,in2,pose,1.f/60);
   check(worn==int(WeaponId::MachinePistol)+1&&in2.weaponSlot==int(WeaponId::MachinePistol)+1,"with the MAC-11 drawn last it rides the hip, and gripping there draws it");
   mac.update(in2,1.f/60);for(int k=0;k<60;++k){InputState i3{};pose.grip[1]=true;r2.script(mac,i3,pose,1.f/60);mac.update(i3,1.f/60);}
   check(mac.equippedWeapon()==WeaponId::MachinePistol&&r2.avatar(mac).hip==int(WeaponId::Handgun)+1,"in the hand, the handgun takes the hip ("+std::to_string(int(r2.avatar(mac).hip))+")");}}
 // Two hands on the pistol: the support hand closing beside the gun hand wraps it; the aim stays the gun hand's.
 {Game game=Game::vrGunScene(WeaponId::Handgun,{20,6});VrRuntime runtime;InputState input{};VrScript pose;pose.head=eye;
  const Point3 hold=eye+Point3{0,-.15f,-.4f};pose.joints[1]=scriptHand(hold,{0,0,-1},{0,1,0},false);
  pose.joints[0]=scriptHand(hold+Point3{-.05f,-.03f,0},{.4f,0,-1},{0,1,0},true);
  runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);const Point3 aim1=runtime.hands()[1].gunBasis[2];const bool before=runtime.hands()[0].snapped;
  pose.grip[0]=true;runtime.script(game,input,pose,1.f/60);game.update(input,1.f/60);
  const auto&s=runtime.hands()[0];
  check(!before&&s.snapped&&game.vrSupportGrip()&&angle(runtime.hands()[1].gunBasis[2],aim1)<1e-4f&&len3(s.handOrigin-runtime.hands()[1].gunOrigin)<.15f,"the support hand closing beside the gun hand wraps the pistol's grip; the pistol aims from the gun hand");}
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
 // How long each group takes (a slow one is a performance bug).
 auto clock=std::chrono::steady_clock::now();auto lap=[&](const char* what){const auto now=std::chrono::steady_clock::now();report<<"TIME: "<<what<<' '<<std::chrono::duration<double,std::milli>(now-clock).count()<<" ms\n";clock=now;};
 run("SteamVR detection (registry key, libraries, headset off, broken runtime)",testDetection());
 run("tracking recovery, foregrip support, coordinate axes, rigid-view inversion, shoulder box",testMath());
 ok&=testSupport(report);ok&=testComfortMath(report);ok&=testAvatar(report);ok&=SoftwareRenderer::testVrAvatar(report);
 lap("solve, arms, support");ok&=Game::testComfortMenu(report);ok&=Game::testVrHardening(report);lap("comfort menu, hardening");
 run("grip from the bound actions, with hysteresis, no stuck legacy bit",testGrip());
 ok&=Game::testVrHeldProps(report);ok&=Game::testVrHeldProps(report,[](Point3 p){return rotateTracking(p,.7f);});lap("held props");
 ok&=testBindings(report);ok&=SoftwareRenderer::testVrGunGeometry(report);lap("bindings, gun geometry");ok&=testGuns(report);lap("guns in scripted hands");ok&=Game::testVrGuns(report);lap("hand reloads, pump, recoil, haptics");
 run("articulated hand meshes (Mesh::testVrHands)",Mesh::testVrHands());run("physical melee",Game::testVrMelee());run("controller interaction",Game::testVrInteraction());lap("hand meshes, melee, interaction");
 report<<(ok?"ALL PASSED":"FAILED")<<'\n';return ok;
}
}
