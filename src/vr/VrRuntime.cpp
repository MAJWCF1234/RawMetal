#include "VrRuntime.h"
#include "../ThirdParty/openvr/openvr.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace retro {
namespace {
VrRuntime* current=nullptr;
using Matrix=std::array<float,16>;
Matrix multiply(const Matrix&a,const Matrix&b){Matrix c{};for(int r=0;r<4;++r)for(int col=0;col<4;++col)for(int k=0;k<4;++k)c[col*4+r]+=a[k*4+r]*b[col*4+k];return c;}
Point3 rotateTracking(Point3 p,float yaw){float c=std::cos(yaw),s=std::sin(yaw);return {-p.z*c-p.x*s,-p.z*s+p.x*c,p.y};}
Matrix rigidInverse(const Matrix&a){Matrix b{};b[15]=1;for(int r=0;r<3;++r)for(int c=0;c<3;++c)b[c*4+r]=a[r*4+c];for(int r=0;r<3;++r)for(int k=0;k<3;++k)b[12+r]-=b[k*4+r]*a[12+k];return b;}
Point3 translation(const vr::HmdMatrix34_t&p){return {p.m[0][3],p.m[1][3],p.m[2][3]};}
float axis(float value){float a=std::fabs(value);return a<.18f?0:std::copysign(std::min(1.f,(a-.18f)/.82f),value);}
}
struct VrRuntime::Impl {
 HMODULE dll=nullptr;void(__cdecl* shutdown)()=nullptr;vr::IVRSystem* system=nullptr;vr::IVRCompositor* compositor=nullptr;vr::IVRInput* input=nullptr;
 std::array<vr::VRActionHandle_t,12> actions{};
 vr::VRActionSetHandle_t set=0;vr::VRActionHandle_t skeleton[2]{};vr::IVROverlay* overlay=nullptr;vr::VROverlayHandle_t panel=0;bool titlePanel=false,previousWristClick=false;
 std::array<vr::TrackedDevicePose_t,vr::k_unMaxTrackedDeviceCount> poses{};
 std::array<VrHand,2> hands{};uint32_t width=0,height=0;float yaw=0,headStartY=0;Point3 previousHead{};std::array<bool,2> previousGrip{},shoulderConsumed{};bool calibrated=false,trackingInterrupted=false,menuWasOpen=false;int pointerX=-1,pointerY=-1;
 Point3 world(Point3 point,const Game&game)const{auto head=translation(poses[0].mDeviceToAbsoluteTracking);auto offset=rotateTracking(point-head,yaw);auto origin=game.chunkOffset(game.level());return offset+Point3{game.player().pos.x+origin.x,game.player().pos.y+origin.y,game.player().z+game.player().eye};}
 ~Impl(){if(overlay&&panel)overlay->DestroyOverlay(panel);if(shutdown&&system)shutdown();if(dll)FreeLibrary(dll);}
};
VrRuntime::VrRuntime():m(std::make_unique<Impl>()){}
VrRuntime::~VrRuntime(){if(current==this)current=nullptr;}
VrRuntime* VrRuntime::active(){return current;}
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
  if(m->input){wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);auto folder=std::filesystem::path(local)/L"RawMetal/vr";std::filesystem::create_directories(folder);
   const char* names[]={"triggerleft","triggerright","gripleft","gripright","squeezeleft","squeezeright","move","turn","jump","reload","menu","inventory"};
   std::ofstream manifestFile(folder/L"actions.json");manifestFile<<R"({"action_sets":[{"name":"/actions/game","usage":"leftright"}],"actions":[{"name":"/actions/game/in/left","type":"skeleton","skeleton":"/skeleton/hand/left"},{"name":"/actions/game/in/right","type":"skeleton","skeleton":"/skeleton/hand/right"})";
   for(int i=0;i<12;++i)manifestFile<<",{\"name\":\"/actions/game/in/"<<names[i]<<"\",\"type\":\""<<(i==6||i==7?"vector2":i==4||i==5?"vector1":"boolean")<<"\"}";
   manifestFile<<R"(],"default_bindings":[{"controller_type":"knuckles","binding_url":"knuckles.json"},{"controller_type":"oculus_touch","binding_url":"oculus_touch.json"},{"controller_type":"vive_controller","binding_url":"vive_controller.json"}]})";manifestFile.close();
   for(const char*controller:{"knuckles","oculus_touch","vive_controller"}){
    bool vive=std::string(controller)=="vive_controller",index=std::string(controller)=="knuckles";std::ofstream file(folder/(std::string(controller)+".json"));
    file<<"{\"controller_type\":\""<<controller<<R"(","name":"RawMetal gameplay","bindings":{"/actions/game":{"skeleton":[{"output":"/actions/game/in/left","path":"/user/hand/left/input/skeleton/left"},{"output":"/actions/game/in/right","path":"/user/hand/right/input/skeleton/right"}],"sources":[)";
    bool first=true;auto source=[&](const char*hand,const char*control,const char*mode,const char*slot,const char*action){if(!first)file<<',';first=false;file<<"{\"path\":\"/user/hand/"<<hand<<"/input/"<<control<<"\",\"mode\":\""<<mode<<"\",\"inputs\":{\""<<slot<<"\":{\"output\":\"/actions/game/in/"<<action<<"\"}}}";};
    for(int side=0;side<2;++side){const char*hand=side?"right":"left";source(hand,"trigger","button","click",names[side]);source(hand,"grip",vive?"button":index?"force_sensor":"trigger",vive?"click":index?"force":"pull",names[(vive?2:4)+side]);source(hand,vive?"trackpad":index?"thumbstick":"joystick",vive?"trackpad":"joystick","position",side?"turn":"move");}
    if(vive){source("left","application_menu","button","click","menu");source("right","application_menu","button","click","reload");}else{source("left",index?"a":"x","button","click","jump");source("right","a","button","click","reload");source("left",index?"b":"y","button","click","menu");source("right","b","button","click","inventory");}
    file<<"]}}}";
   }
   auto manifest=(folder/L"actions.json").string();if(m->input->SetActionManifestPath(manifest.c_str())==vr::VRInputError_None){m->input->GetActionSetHandle("/actions/game",&m->set);for(int i=0;i<12;++i)m->input->GetActionHandle((std::string("/actions/game/in/")+names[i]).c_str(),&m->actions[i]);m->input->GetActionHandle("/actions/game/in/left",&m->skeleton[0]);m->input->GetActionHandle("/actions/game/in/right",&m->skeleton[1]);}
  }
  current=this;return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
std::pair<int,int> VrRuntime::extent()const{return {int(m->width),int(m->height)};}
bool VrRuntime::titlePanel()const{return m->titlePanel;}
bool VrRuntime::trackingValid()const{return m->poses[0].bPoseIsValid;}
std::pair<int,int> VrRuntime::pointer()const{return {m->pointerX,m->pointerY};}
const std::array<VrHand,2>& VrRuntime::hands()const{return m->hands;}
std::string VrRuntime::instanceExtensions()const{std::string s(m->compositor->GetVulkanInstanceExtensionsRequired(nullptr,0),'\0');m->compositor->GetVulkanInstanceExtensionsRequired(s.data(),uint32_t(s.size()));return s;}
std::string VrRuntime::deviceExtensions(VkPhysicalDevice device)const{std::string s(m->compositor->GetVulkanDeviceExtensionsRequired(device,nullptr,0),'\0');m->compositor->GetVulkanDeviceExtensionsRequired(device,s.data(),uint32_t(s.size()));return s;}
VkPhysicalDevice VrRuntime::outputDevice(VkInstance instance)const{uint64_t device=0;m->system->GetOutputDevice(&device,vr::TextureType_Vulkan,instance);return reinterpret_cast<VkPhysicalDevice>(device);}
void VrRuntime::suspendTracking(Game&game,InputState&input){
  // Keep tracked movement enabled but neutral: PC keys and stale controller
  // actions must not move the player while the headset pose is unavailable.
  input.vrTracked=true;input.vrYaw=game.player().angle;input.vrPitch=game.player().pitch;input.vrEye=game.player().eye;
  input.vrMoveForward=input.vrMoveRight=0;input.vrGrip={};input.fire=input.use=input.jump=input.reload=false;
  input.pointerX=input.pointerY=m->pointerX=m->pointerY=-1;
  m->previousGrip={};m->shoulderConsumed={};m->previousWristClick=false;
  for(auto&h:m->hands)h={};game.setVrAim({}, {},false);game.setVrHand(0,{}, {},false);game.setVrHand(1,{}, {},false);
  // Resume at the recovered tracking origin rather than applying the whole
  // unobserved head displacement as a room-scale collision move.
  m->trackingInterrupted=true;
}
bool VrRuntime::update(Game&game,InputState&input,float dt){
 vr::VREvent_t event{};while(m->system->PollNextEvent(&event,sizeof(event)))if(event.eventType==vr::VREvent_Quit)return false;
 if(m->compositor->WaitGetPoses(m->poses.data(),uint32_t(m->poses.size()),nullptr,0)!=vr::VRCompositorError_None)throw std::runtime_error("SteamVR tracking/compositor disconnected.");
 if(!m->poses[0].bPoseIsValid){suspendTracking(game,input);return true;}
 auto head=translation(m->poses[0].mDeviceToAbsoluteTracking);
 if(!m->calibrated){m->yaw=game.player().angle;m->headStartY=head.y;m->previousHead=head;m->calibrated=true;}
 if(m->trackingInterrupted){m->previousHead=head;m->trackingInterrupted=false;}
 if(!game.paused()&&!game.titleScreen()){auto delta=rotateTracking(head-m->previousHead,m->yaw);game.moveVrRoom({delta.x,delta.y});}m->previousHead=head;
 vr::VRControllerState_t states[2]{};uint32_t devices[2]={m->system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),m->system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand)};
 vr::VRControllerAxis_t sticks[2]{};
 for(int side=0;side<2;++side){auto device=devices[side];auto&h=m->hands[side];h={};
  if(device>=m->poses.size()||!m->poses[device].bPoseIsValid)continue;
  m->system->GetControllerState(device,&states[side],sizeof(states[side]));h.tracked=true;auto pose=m->poses[device].mDeviceToAbsoluteTracking;h.position=m->world(translation(pose),game);
  for(int i=0;i<3;++i)h.basis[i]=rotateTracking({pose.m[0][i],pose.m[1][i],pose.m[2][i]},m->yaw);
  for(int i=0;i<5;++i){int type=m->system->GetInt32TrackedDeviceProperty(device,vr::ETrackedDeviceProperty(vr::Prop_Axis0Type_Int32+i));if(type==vr::k_eControllerAxis_Joystick||(type==vr::k_eControllerAxis_TrackPad&&(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad)))){sticks[side]=states[side].rAxis[i];break;}}
  float trigger=states[side].rAxis[1].x;h.curls.fill((states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))?.85f:.1f);h.curls[1]=std::clamp(trigger,0.f,1.f);
 }
 if(m->input&&m->set){vr::VRActiveActionSet_t set{};set.ulActionSet=m->set;m->input->UpdateActionState(&set,sizeof(set),1);for(int side=0;side<2;++side){vr::InputSkeletalActionData_t data{};vr::VRSkeletalSummaryData_t summary{};if(m->input->GetSkeletalActionData(m->skeleton[side],&data,sizeof(data))==vr::VRInputError_None&&data.bActive&&m->input->GetSkeletalSummaryData(m->skeleton[side],vr::VRSummaryType_FromDevice,&summary)==vr::VRInputError_None){std::copy(summary.flFingerCurl,summary.flFingerCurl+5,m->hands[side].curls.begin());m->hands[side].articulated=true;}}}
 if(m->input&&m->set){
  auto digital=[&](int action,bool fallback){vr::InputDigitalActionData_t data{};return m->input->GetDigitalActionData(m->actions[action],&data,sizeof(data),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&data.bActive?data.bState:fallback;};
  for(int side=0;side<2;++side){states[side].rAxis[1].x=digital(side,states[side].rAxis[1].x>.65f)?1.f:0.f;vr::InputAnalogActionData_t squeeze{};bool grip=digital(side+2,(states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))!=0);if(m->input->GetAnalogActionData(m->actions[4+side],&squeeze,sizeof(squeeze),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&squeeze.bActive)grip|=squeeze.x>.65f;if(grip)states[side].ulButtonPressed|=vr::ButtonMaskFromId(vr::k_EButton_Grip);else states[side].ulButtonPressed&=~vr::ButtonMaskFromId(vr::k_EButton_Grip);vr::InputAnalogActionData_t move{};if(m->input->GetAnalogActionData(m->actions[6+side],&move,sizeof(move),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&move.bActive)sticks[side]={move.x,move.y};}
  if(digital(8,false))states[0].ulButtonPressed|=vr::ButtonMaskFromId(vr::k_EButton_A);if(digital(9,false))states[1].ulButtonPressed|=vr::ButtonMaskFromId(vr::k_EButton_A);input.inventory|=digital(11,false);if(digital(10,false))states[0].ulButtonPressed|=vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu);
 }
 // SteamVR action state can outlive its tracked device. Discard it before
 // locomotion/menu/use processing, including the legacy input fallback.
 for(int side=0;side<2;++side){if(!m->hands[side].tracked){states[side]={};sticks[side]={};}else if(!m->hands[side].articulated){m->hands[side].curls.fill((states[side].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip))?.85f:.1f);m->hands[side].curls[1]=std::clamp(states[side].rAxis[1].x,0.f,1.f);}}
 bool focused=m->system->IsInputAvailable();if(focused&&!game.paused()&&!game.titleScreen())m->yaw=wrapAngle(m->yaw+axis(sticks[1].x)*1.5707963f*std::min(dt,.05f));
 auto&pose=m->poses[0].mDeviceToAbsoluteTracking;auto forward=rotateTracking({-pose.m[0][2],-pose.m[1][2],-pose.m[2][2]},m->yaw);
 input.vrTracked=true;input.vrYaw=std::atan2(forward.y,forward.x);input.vrPitch=140.f*std::asin(std::clamp(forward.z,-1.f,1.f));input.vrEye=std::clamp(Player::StandingEye+head.y-m->headStartY,.15f,2.5f);
 input.mouseDx=input.mouseDy=0;input.vrMoveForward=focused?axis(sticks[0].y):0;input.vrMoveRight=focused?axis(sticks[0].x):0;input.crouch=input.vrEye<1.f;
 bool desktopClick=input.fire,desktopAccept=input.menuAccept;
 input.fire=focused&&m->hands[1].tracked&&states[1].rAxis[1].x>.65f;input.use=focused&&((states[0].ulButtonPressed|states[1].ulButtonPressed)&vr::ButtonMaskFromId(vr::k_EButton_Grip));input.reload=focused&&(states[1].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_A));input.jump=focused&&(states[0].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_A));input.menuAccept|=input.fire||(focused&&states[0].rAxis[1].x>.65f);input.menuUp|=focused&&sticks[0].y>.6f;input.menuDown|=focused&&sticks[0].y<-.6f;input.escape|=focused&&(states[0].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu));
 bool menuOpen=game.titleScreen()||game.paused()||game.inventoryOpen()||game.consoleOpen();
 for(int hand=0;hand<2;++hand){bool grip=focused&&m->hands[hand].tracked&&(states[hand].ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip));input.vrGrip[hand]=grip;
  if(grip&&!m->previousGrip[hand]&&!menuOpen&&!game.holdingClutter()){auto offset=rotateTracking(translation(m->poses[devices[hand]].mDeviceToAbsoluteTracking)-head,m->yaw);if(shoulderSlot(offset,forward)){input.weaponScroll=game.weaponEquipped()?-1:1;m->shoulderConsumed[hand]=true;}}
  if(!grip)m->shoulderConsumed[hand]=false;if(m->shoulderConsumed[hand]){input.use=false;input.vrGrip[hand]=false;}m->previousGrip[hand]=grip;
 }

 if(menuOpen&&!m->menuWasOpen){
  const auto&headPose=m->poses[0].mDeviceToAbsoluteTracking;float x=-headPose.m[0][2],z=-headPose.m[2][2];float length=std::hypot(x,z);if(length<.001f){x=0;z=-1;length=1;}x/=length;z/=length;
  vr::HmdMatrix34_t panelPose{};panelPose.m[0][0]=-z;panelPose.m[2][0]=x;panelPose.m[1][1]=1;panelPose.m[0][2]=-x;panelPose.m[2][2]=-z;panelPose.m[0][3]=head.x+x*2.f;panelPose.m[1][3]=head.y;panelPose.m[2][3]=head.z+z*2.f;
  m->overlay->SetOverlayTransformAbsolute(m->panel,vr::TrackingUniverseStanding,&panelPose);
 }else if(!menuOpen&&m->menuWasOpen){vr::HmdMatrix34_t panelPose{};panelPose.m[0][0]=panelPose.m[1][1]=panelPose.m[2][2]=1;panelPose.m[2][3]=-2.f;m->overlay->SetOverlayTransformTrackedDeviceRelative(m->panel,vr::k_unTrackedDeviceIndex_Hmd,&panelPose);}
 m->menuWasOpen=menuOpen;m->titlePanel=game.titleScreen()||(menuOpen&&!m->hands[0].tracked);
 if(m->titlePanel)m->overlay->ShowOverlay(m->panel);else m->overlay->HideOverlay(m->panel);
 if(!m->titlePanel)input.pointerX=input.pointerY=-1;
 if(menuOpen||!m->titlePanel){
  // Only menus replace shooting with pointer clicks. Preserve the controller
  // trigger during gameplay unless the ray actually hits the wrist screen.
  if(menuOpen)input.fire=desktopClick;input.menuLeft|=focused&&sticks[0].x<-.6f;input.menuRight|=focused&&sticks[0].x>.6f;
  int first=states[0].rAxis[1].x>.65f&&states[1].rAxis[1].x<=.65f?0:1;
  for(int n=0;n<2&&focused;++n){int side=n?1-first:first;if(!m->titlePanel&&side!=1)continue;auto device=devices[side];if(device>=m->poses.size()||!m->hands[side].tracked)continue;const auto&controllerPose=m->poses[device].mDeviceToAbsoluteTracking;vr::VROverlayIntersectionParams_t ray{};ray.eOrigin=vr::TrackingUniverseStanding;ray.vSource={{controllerPose.m[0][3],controllerPose.m[1][3],controllerPose.m[2][3]}};ray.vDirection={{-controllerPose.m[0][2],-controllerPose.m[1][2],-controllerPose.m[2][2]}};vr::VROverlayIntersectionResults_t hit{};
   bool intersects=false;if(m->titlePanel)intersects=m->overlay->ComputeOverlayIntersection(m->panel,&ray,&hit);else if(m->hands[0].tracked){const auto&left=m->poses[devices[0]].mDeviceToAbsoluteTracking;Point3 source{},direction{};for(int axis=0;axis<3;++axis){float o=0,d=0;for(int k=0;k<3;++k){o+=left.m[k][axis]*(ray.vSource.v[k]-left.m[k][3]);d+=left.m[k][axis]*ray.vDirection.v[k];}if(axis==0){source.x=o;direction.x=d;}else if(axis==1){source.y=o;direction.y=d;}else{source.z=o;direction.z=d;}}auto pixel=wristPointer(source,direction);intersects=pixel.first>=0;hit.vUVs.v[0]=(pixel.first+.5f)/DisplayWidth;hit.vUVs.v[1]=1.f-(pixel.second+.5f)/DisplayHeight;}
   if(intersects){input.pointerX=std::clamp(int(hit.vUVs.v[0]*DisplayWidth),0,DisplayWidth-1);input.pointerY=std::clamp(int((1.f-hit.vUVs.v[1])*DisplayHeight),0,DisplayHeight-1);bool click=states[side].rAxis[1].x>.65f;if(menuOpen){input.fire|=click;input.menuAccept=desktopAccept;}else {input.fire=false;input.use=false;if(click&&!m->previousWristClick){if(input.pointerY>=292&&input.pointerY<=342){if(input.pointerX>=24&&input.pointerX<290){if(game.logTime()>0)input.use=true;else input.inventory=true;}else if(input.pointerX>=334&&input.pointerX<616)input.escape=true;}}}m->previousWristClick=click;break;}
  }
 }
 if(states[1].rAxis[1].x<=.65f)m->previousWristClick=false;m->pointerX=input.pointerX;m->pointerY=input.pointerY;
 auto origin=game.chunkOffset(game.level());auto aim=m->hands[1].position;aim.x-=origin.x;aim.y-=origin.y;Point3 aimDir=m->hands[1].basis[2]*-1;bool support=false;
 if(focused&&m->hands[0].tracked&&m->hands[1].tracked&&game.weaponEquipped()&&!game.unarmed()&&!game.holdingClutter()&&!menuOpen){
  auto right=m->hands[1].position,left=m->hands[0].position,handForward=m->hands[1].basis[2]*-1;
  if(foregripSupport(right,left,handForward,input.vrGrip[0])){auto delta=left-right;float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);if(length>.0001f){aimDir={delta.x/length,delta.y/length,delta.z/length};support=true;}}
 }
 game.setVrAim(aim,aimDir,m->hands[1].tracked&&focused,support);for(int side=0;side<2;++side){auto hand=m->hands[side].position;hand.x-=origin.x;hand.y-=origin.y;Point3 velocity{};if(devices[side]<m->poses.size()){const auto&v=m->poses[devices[side]].vVelocity;velocity=rotateTracking({v.v[0],v.v[1],v.v[2]},m->yaw);}game.setVrHand(side,hand,m->hands[side].basis[2]*-1,m->hands[side].tracked&&focused,velocity);}return true;
}
VrEye VrRuntime::eye(int index,const Game&game)const{
 for(int side=0;side<2;++side)if(m->hands[side].tracked){auto device=m->system->GetTrackedDeviceIndexForControllerRole(side?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand);if(device>=m->poses.size())continue;const auto&pose=m->poses[device].mDeviceToAbsoluteTracking;m->hands[side].position=m->world(translation(pose),game);for(int i=0;i<3;++i)m->hands[side].basis[i]=rotateTracking({pose.m[0][i],pose.m[1][i],pose.m[2][i]},m->yaw);}
 auto h=m->poses[0].mDeviceToAbsoluteTracking;auto e=m->system->GetEyeToHeadTransform(vr::EVREye(index));vr::HmdMatrix34_t pose{};
 for(int r=0;r<3;++r){for(int col=0;col<3;++col)for(int k=0;k<3;++k)pose.m[r][col]+=h.m[r][k]*e.m[k][col];pose.m[r][3]=h.m[r][3];for(int k=0;k<3;++k)pose.m[r][3]+=h.m[r][k]*e.m[k][3];}
 Matrix world{};world[15]=1;for(int col=0;col<3;++col){auto p=rotateTracking({pose.m[0][col],pose.m[1][col],pose.m[2][col]},m->yaw);world[col*4]=p.x;world[col*4+1]=p.y;world[col*4+2]=p.z;}auto p=m->world(translation(pose),game);world[12]=p.x;world[13]=p.y;world[14]=p.z;
 auto projection=m->system->GetProjectionMatrix(vr::EVREye(index),.06f,250.f);Matrix clip{};for(int col=0;col<4;++col){clip[col*4]=projection.m[0][col];clip[col*4+1]=-projection.m[1][col];clip[col*4+2]=(projection.m[2][col]+projection.m[3][col])*.5f;clip[col*4+3]=projection.m[3][col];}
 return {p,multiply(clip,rigidInverse(world))};
}
void VrRuntime::submit(int eye,VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height){
 vr::VRVulkanTextureData_t data{};data.m_nImage=reinterpret_cast<uint64_t>(image);data.m_pDevice=device;data.m_pPhysicalDevice=physical;data.m_pInstance=instance;data.m_pQueue=queue;data.m_nQueueFamilyIndex=family;data.m_nWidth=width;data.m_nHeight=height;data.m_nFormat=VK_FORMAT_R8G8B8A8_UNORM;data.m_nSampleCount=1;
 vr::Texture_t texture{&data,vr::TextureType_Vulkan,vr::ColorSpace_Gamma};auto result=m->compositor->Submit(vr::EVREye(eye),&texture);if(result!=vr::VRCompositorError_None)throw std::runtime_error("SteamVR Vulkan eye submission failed ("+std::to_string(result)+").");if(eye==1)m->compositor->PostPresentHandoff();
}
void VrRuntime::submitPanel(VkInstance instance,VkPhysicalDevice physical,VkDevice device,VkQueue queue,uint32_t family,VkImage image,int width,int height){
 if(!m->titlePanel)return;vr::VRVulkanTextureData_t data{};data.m_nImage=reinterpret_cast<uint64_t>(image);data.m_pDevice=device;data.m_pPhysicalDevice=physical;data.m_pInstance=instance;data.m_pQueue=queue;data.m_nQueueFamilyIndex=family;data.m_nWidth=width;data.m_nHeight=height;data.m_nFormat=VK_FORMAT_B8G8R8A8_UNORM;data.m_nSampleCount=1;vr::Texture_t texture{&data,vr::TextureType_Vulkan,vr::ColorSpace_Gamma};auto result=m->overlay->SetOverlayTexture(m->panel,&texture);if(result!=vr::VROverlayError_None)throw std::runtime_error("SteamVR menu texture failed ("+std::to_string(result)+").");
}
std::pair<int,int> VrRuntime::wristPointer(Point3 origin,Point3 direction){origin={origin.x/WristScale,origin.y/WristScale,(origin.z-WristCenter)/WristScale+.13f};direction=direction*(1.f/WristScale);if(direction.y>=-.0001f)return {-1,-1};float distance=(.037f-origin.y)/direction.y;if(distance<0||distance>3.f)return {-1,-1};auto p=origin+direction*distance;if(p.x<-.11f||p.x>.11f||p.z<.068125f||p.z>.191875f)return {-1,-1};return {std::clamp(int((p.x+.11f)/.22f*DisplayWidth),0,DisplayWidth-1),std::clamp(int((p.z-.068125f)/.12375f*DisplayHeight),0,DisplayHeight-1)};}
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
bool VrRuntime::testMath(){
 {auto game=std::make_unique<Game>();VrRuntime runtime;InputState input{};
  input.fire=input.use=input.jump=input.reload=true;input.vrGrip={true,true};
  input.vrMoveForward=1;input.vrMoveRight=-1;input.pointerX=input.pointerY=42;
  runtime.m->hands[0].tracked=runtime.m->hands[1].tracked=true;
  runtime.m->previousGrip={true,true};runtime.m->shoulderConsumed={true,true};
  runtime.m->previousWristClick=true;runtime.m->calibrated=true;runtime.m->headStartY=1.45f;
  runtime.suspendTracking(*game,input);
  if(!input.vrTracked||input.fire||input.use||input.jump||input.reload||input.vrGrip[0]||input.vrGrip[1]||input.vrMoveForward||input.vrMoveRight||input.pointerX!=-1||input.pointerY!=-1||runtime.m->hands[0].tracked||runtime.m->hands[1].tracked||runtime.m->previousGrip[0]||runtime.m->shoulderConsumed[1]||runtime.m->previousWristClick||!runtime.m->trackingInterrupted||!runtime.m->calibrated||runtime.m->headStartY!=1.45f)return false;
 }
 if(!foregripSupport({0,0,0},{.35f,0,0},{1,0,0},true)||foregripSupport({0,0,0},{.35f,0,0},{1,0,0},false)||foregripSupport({0,0,0},{.08f,0,0},{1,0,0},true)||foregripSupport({0,0,0},{-.35f,0,0},{1,0,0},true)||foregripSupport({0,0,0},{0,.35f,0},{1,0,0},true)||foregripSupport({0,0,0},{.35f,.25f,0},{1,0,0},true)||!foregripSupport({1,2,3},{1,2.35f,3.05f},{0,2,0},true))return false;
 auto pixel=wristPointer(wristPoint({0,.4f,.13f}),{0,-1,0});if(pixel.first!=320||std::abs(pixel.second-180)>1||wristPointer(wristPoint({.2f,.4f,.13f}),{0,-1,0}).first!=-1||wristPointer(wristPoint({0,.4f,.13f}),{0,1,0}).first!=-1)return false;if(!shoulderSlot({-.3f,.2f,-.1f},{1,0,0})||!shoulderSlot({-.2f,-.3f,-.1f},{0,1,0})||shoulderSlot({.3f,.2f,-.1f},{1,0,0})||shoulderSlot({-.3f,.2f,-.8f},{1,0,0}))return false;Matrix a{};a[0]=a[5]=a[10]=a[15]=1;a[12]=4;a[13]=-2;a[14]=9;auto b=multiply(a,rigidInverse(a));for(int i=0;i<16;++i)if(std::fabs(b[i]-(i%5==0?1.f:0.f))>.00001f)return false;auto f=rotateTracking({0,0,-1},0);auto r=rotateTracking({1,0,0},0);auto up=rotateTracking({0,1,0},0);return f.x==1&&r.y==1&&up.z==1;}
}
