#include "SoftwareRenderer.h"
#include "Projection.h"
#include "../net/NetTypes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
// The other players in a match (docs/Systems/multiplayer.md), drawn from Game::remotePlayers() as full bodies in the
// world, the way Claire is drawn from outside (Scene3D.cpp, ClaireOutside.h): the legs' clip from their velocity and
// stance, an upper-body hold from the weapon in their hands, the torso turned to their aim, the shot and reload clips
// from shotAge and reloadAge, the Death clip from deathAge and then nothing until they respawn; the gun fitted to the
// skin's hands. A VR player is posed from their AvatarPose through solveAvatar, as the local VR body is: the legs and
// lean from the solve, the IK arms on its shoulders, the gun in the hand that holds it. Each slot keeps its own
// ClaireOutside (the blend it is in, the gun remembered in the right hand). The body comes through characterBody,
// by Settings::character: Claire, the Limitator or Leon (the one before stands in while a body cannot load).
namespace retro {
namespace {
std::uint32_t rgb(int r,int g,int b){return 0xff000000u|(std::uint32_t(r)<<16)|(std::uint32_t(g)<<8)|std::uint32_t(b);}
constexpr float kPiF=3.14159265f;
constexpr Point3 TriggerSocket{.02f,.70f,1.10f},SupportSocket{-.064f,1.10f,-2.30f};  // Scene3D.cpp's shotgun sockets
// Claire's ten materials by FBX name (Scene3D.cpp claireMaterial; resources 2302-2311 in this order).
int materialOf(const Mesh& mesh,int part){
 static constexpr const char* names[]={"SOCKS","BELTS","PANTS","BOOTS","VEST","SKIRT","JACKET","MASK","EYES","HAIR"};
 if(part>=0&&size_t(part)<mesh.materialNames.size())for(int m=0;m<10;++m)if(mesh.materialNames[size_t(part)].find(names[m])!=std::string::npos)return m;
 return -1;
}
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 unit(Point3 v){const float l=std::sqrt(dot(v,v));return l>1e-6f?v*(1/l):Point3{0,0,1};}
float wrap(float a){while(a>kPiF)a-=2*kPiF;while(a<-kPiF)a+=2*kPiF;return a;}
}
// One body per character (its mesh, its faces' textures, which faces are arms and head), and per player slot the
// pose state that carries from frame to frame.
struct SoftwareRenderer::RemoteBodies {
 struct Character{std::unique_ptr<Mesh> mesh;std::vector<unsigned char> texture,arm,head;std::vector<const Texture*> faceTexture;float headFraction=.93f;bool tried=false,ready=false;};
 // The face textures of the Limitator (1) and Leon (2) (SoftwareRenderer::playerBody); set by the renderer before a body is first asked for.
 std::array<const std::vector<const Texture*>*,3> bodyTextures{nullptr,nullptr,nullptr};
 std::array<Character,3> characters;
 struct Body{ClaireOutside outside;int key=-1;float blendStart=-1,last=-1,gait=0,yaw=0,turned=0,health=100,hitStart=-1e9f;bool seen=false;const Mesh* mesh=nullptr;};
 std::array<Body,16> bodies;
 std::vector<RemoteBodyReport> reports;
 Character& body(int character){
  auto& c=characters[size_t(std::clamp(character,0,2))];
  if(!c.tried){c.tried=true;
   // 0 Claire (2300 on her clips 2301 and anchors 2312). 1 the Limitator (2290 on his clips 2291 and anchors 2292),
   // a copy of his own so remote poses never fight the local body's; while he cannot load, Claire stands in for him.
   if(&c==&characters[0]){try{c.mesh=std::make_unique<Mesh>(2300,"",2301,2312);}catch(...){c.mesh.reset();}}
   else{const size_t which=size_t(&c-characters.data());const auto* textures=bodyTextures[which];
    // 2 Leon (2270 on 2271, anchors 2272): the same.
    if(textures){try{c.mesh=which==2?std::make_unique<Mesh>(2270,"",2271,2272):std::make_unique<Mesh>(2290,"",2291,2292);if(c.mesh->triangles.size()!=textures->size())c.mesh.reset();}catch(...){c.mesh.reset();}
     if(c.mesh)c.faceTexture=*textures;}}
   if(c.mesh&&!c.mesh->triangles.empty()){
    // Classify once, on the bind pose: material, the arms (out past the shoulders) and the head (above the neck).
    const auto& bind=c.mesh->bindTriangles();Point3 low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};
    for(const auto& t:bind)for(const auto& v:t.v){low.x=std::min(low.x,v.p.x);low.y=std::min(low.y,v.p.y);high.x=std::max(high.x,v.p.x);high.y=std::max(high.y,v.p.y);}
    const float height=std::max(1e-4f,high.y-low.y),middle=(low.x+high.x)*.5f;double headSum=0;int headCount=0;
    c.texture.assign(bind.size(),0);c.arm.assign(bind.size(),0);c.head.assign(bind.size(),0);
    if(&c==&characters[2]){
     // Leon: the arms are what his arm and finger bones carry, the head what his head bone does (ears included).
     static constexpr const char* armBones[]={"Arm_L","Arm_R","ForeArm_L","ForeArm_R","Hand_L","Hand_R"};
     std::vector<std::string> names(std::begin(armBones),std::end(armBones));
     for(const char* f:{"Thumb","Index","Middle","Ring","Pinky"})for(int j=1;j<=3;++j)for(const char* s:{"_L","_R"})names.push_back(std::string(f)+std::to_string(j)+s);
     std::vector<float> armWeight;for(const auto& n:names){const auto w=c.mesh->creatureBoneWeights({n.c_str()});if(armWeight.size()<w.size())armWeight.resize(w.size(),0.f);for(size_t i=0;i<w.size();++i)armWeight[i]+=w[i];}
     const auto headWeight=c.mesh->creatureBoneWeights({"Head"});
     const auto& index=c.mesh->creatureVertexIndices();
     auto carried=[&](const std::vector<float>& w,size_t f){float s=0;for(int k=0;k<3;++k){const size_t corner=f*3+size_t(k);if(corner<index.size()&&index[corner]<w.size())s+=w[index[corner]]/3.f;}return s;};
     for(size_t f=0;f<bind.size();++f){c.arm[f]=carried(armWeight,f)>.5f;c.head[f]=carried(headWeight,f)>.5f;
      if(c.head[f])for(const auto& v:bind[f].v){headSum+=(v.p.y-low.y)/height;++headCount;}}}
    else for(size_t f=0;f<bind.size();++f){c.texture[f]=static_cast<unsigned char>(std::max(0,materialOf(*c.mesh,bind[f].part)));
     bool arm=false,head=true;for(const auto& v:bind[f].v){const float y=(v.p.y-low.y)/height,x=std::fabs(v.p.x-middle)/height;arm=arm||((x>.118f||(x>.08f&&y>.66f))&&y<.85f);head=head&&y>.88f;}
     c.arm[f]=arm;c.head[f]=head;if(head)for(const auto& v:bind[f].v){headSum+=(v.p.y-low.y)/height;++headCount;}}
    if(headCount)c.headFraction=float(headSum/headCount);
    c.ready=true;}}
  if(!c.ready&&&c!=&characters[0])return body(int(&c-characters.data())-1);   // Leon stands in for himself with the Limitator, and he with Claire
  return c;
 }
};
void SoftwareRenderer::RemoteBodiesFree::operator()(RemoteBodies* p)const{delete p;}
SoftwareRenderer::RemoteBodies& SoftwareRenderer::remoteBodies(){if(!m_remote)m_remote.reset(new RemoteBodies);return *m_remote;}
const std::vector<SoftwareRenderer::RemoteBodyReport>& SoftwareRenderer::remoteBodyReports()const{static const std::vector<RemoteBodyReport> none;return m_remote?m_remote->reports:none;}
bool SoftwareRenderer::characterBuilt(int character)const{
 if(character<=0)return true;
 return character==2?!m_leonFailed:!m_limitatorFailed;   // each is in the build; only a body that failed to load is not
}

void SoftwareRenderer::drawRemotePlayers(const Game& game,const RemoteDrawHooks& hooks){
 auto& all=remoteBodies();all.reports.clear();
 const float now=game.elapsed();
 static const Texture shadowTexture=[]{Texture t{1,1,{0x46000000u}};t.transparent=true;return t;}();
 const Point3 eye{m_view.pos.x,m_view.pos.y,m_view.z+m_view.eye};
 for(const auto& r:game.remotePlayers()){
  RemoteBodyReport report;report.slot=r.slot;report.vr=r.vr&&r.pose.active;report.character=r.character;report.feet=r.position;
  auto& b=all.bodies[size_t(r.slot&15)];
  const float dt=b.last<0||now<b.last||now-b.last>1.f?0.f:now-b.last;
  if(dt==0&&(b.last<0||now<b.last||now-b.last>1.f)){b.key=-1;b.blendStart=-1;b.seen=false;b.gait=0;b.health=r.health;b.hitStart=-1e9f;}
  b.last=now;
  // Dead: the Death clip plays out, the body lies a beat, then it is gone until they respawn.
  if(!r.alive&&r.deathAge>4.5f){b.key=-1;b.seen=false;all.reports.push_back(report);continue;}
  // The body's face textures come from the renderer's own (playerBody); a Leon that will not load is the Limitator.
  const auto offer=[&](int which){if(!all.characters[size_t(which)].tried&&!all.bodyTextures[size_t(which)]){PlayerBody& his=playerBody(which);if(his.character==which)all.bodyTextures[size_t(which)]=&his.faceTexture;else all.characters[size_t(which)].tried=true;}};
  if(r.character==1)offer(1);
  else if(r.character==2){offer(2);if(all.characters[2].tried&&!all.bodyTextures[2])offer(1);}
  auto& character=all.body(r.character);
  if(!character.ready){all.reports.push_back(report);continue;}
  Mesh& mesh=*character.mesh;
  if(b.mesh!=&mesh){b.outside=ClaireOutside{};b.mesh=&mesh;b.key=-1;}
  if(!b.outside.prepare(mesh)||!b.outside.hasClip(ClaireOutside::Land)){all.reports.push_back(report);continue;}
  using Clip=ClaireOutside::Clip;
  const Vec2 forward{std::cos(r.angle),std::sin(r.angle)},right{-forward.y,forward.x};
  const float speed=length(r.velocity),ahead=dot(r.velocity,forward),across=dot(r.velocity,right);
  const int direction=std::fabs(ahead)>=std::fabs(across)?(ahead>=0?0:1):(across>=0?3:2);
  const bool vr=r.vr&&r.pose.active;
  // The body's facing: on the look while moving; standing, it lets the look run up to 50 degrees ahead, then follows.
  if(!b.seen){b.yaw=vr?r.pose.bodyYaw:r.angle;b.turned=b.yaw;b.seen=true;}
  if(vr)b.yaw=r.pose.bodyYaw;
  else{const float d=wrap(r.angle-b.yaw);
   if(speed>=.25f||!r.grounded||r.prone||!r.alive)b.yaw=wrap(b.yaw+d*std::min(1.f,dt*10.f));
   else if(std::fabs(d)>.87f)b.yaw=wrap(b.yaw+(d-std::copysign(.87f,d))*std::min(1.f,dt*8.f));}
  const float bodyAngle=b.yaw;
  // The legs: idle, walk, run, sprint, crouched, prone, falling, the Death clip.
  int clip=Clip::Idle;float phase=std::fmod(now/2.13f,1.f);
  float stride=2.2f;
  if(!r.alive){clip=Clip::Death;phase=std::min(1.f,r.deathAge/3.4f);}
  else if(!r.grounded){clip=Clip::Falling;phase=std::fmod(now,1.f);}
  else if(r.prone){if(speed>=.2f){clip=direction==1?Clip::CrawlBack:Clip::Crawl;stride=.9f;}else clip=Clip::ProneIdle;}
  else if(r.crouched){if(speed>=.25f){clip=Clip::CrouchWalk+direction;stride=1.1f;}else clip=Clip::CrouchIdle;}
  else if(speed>=.25f){if(direction==0&&speed>4.2f){clip=Clip::Sprint;stride=3.2f;}else if(speed<2.f){clip=direction==0?Clip::Walk:15+direction;stride=1.4f;}else clip=Clip::RunForward+direction;}
  b.gait+=dt*speed/stride;
  if(speed>=.2f&&r.alive&&r.grounded)phase=std::fmod(b.gait,1.f);
  int lower=clip;float lowerPhase=phase;
  ClaireOutside::Layer layer;float pitch=0,yaw=0,aimed=0;int held=0;
  Point3 aim{0,0,1};
  if(vr){const AvatarSolve solve=solveAvatar(r.pose,now);lower=solve.legsClip;lowerPhase=solve.legsPhase;pitch=-solve.lean;yaw=-solve.twist;}
  else{
   held=!r.alive?0:r.weapon==1?1:r.weapon==2?2:r.weapon==3?3:0;   // 3: the MAC-11, held as the pistol
   const bool upright=r.alive&&r.grounded&&!r.prone;
   const float shot=r.shotAge,look=std::clamp(r.pitch,-1.2f,1.2f);
   const bool recent=shot<3.f;    // a player who fired lately holds the gun up on their aim
   if(r.alive&&r.health<b.health-1.f)b.hitStart=now;b.health=r.health;
   if(r.alive&&!r.prone&&now-b.hitStart<.45f)layer={Clip::HitReaction,(now-b.hitStart)/.45f,1};
   else if(held==1&&upright){
    const bool reloading=r.reloadAge<1.8f;
    if(speed<.25f&&!reloading){if(shot<.3f){lower=Clip::RifleShot;lowerPhase=shot/.3f;aimed=1;}else if(recent){lower=r.crouched?Clip::CrouchAim:Clip::AimIdle;lowerPhase=std::fmod(now/(r.crouched?1.9f:2.03f),1.f);aimed=1;}}
    else if(!reloading){if(shot<.3f){layer={Clip::RifleShot,shot/.3f,1};aimed=1;}else if(recent){layer={Clip::AimIdle,std::fmod(now/2.03f,1.f),1};aimed=1;}}}
   else if(held>=2&&upright){
    if(r.reloadAge<1.5f)layer={Clip::PistolReload,r.reloadAge/1.5f,1};
    else if(shot<.45f){layer={Clip::PistolShoot,shot/.45f,1};aimed=1;}
    else if(shot<1.5f||recent){layer={Clip::PistolAim,0,1,look>0?Clip::PistolAimUp:Clip::PistolAimDown,0,std::clamp(std::fabs(look)/.9f,0.f,1.f)};aimed=1;}
    else layer={Clip::PistolIdle,std::fmod(now/1.67f,1.f),1};}
   // Their aim in the mesh's frame (+X her left, +Z ahead).
   const Point3 aimWorld{std::cos(r.angle)*std::cos(look),std::sin(r.angle)*std::cos(look),std::sin(look)};
   aim={aimWorld.x*std::sin(bodyAngle)-aimWorld.y*std::cos(bodyAngle),aimWorld.z,aimWorld.x*std::cos(bodyAngle)+aimWorld.y*std::sin(bodyAngle)};
   if(held&&upright){pitch=aimed>0?look:look*(r.reloadAge<1.8f?.2f:.35f);if(aimed>0)yaw=std::atan2(aim.x,aim.z);}
   else if(r.alive&&!r.prone&&r.grounded)pitch=look*.25f;
   pitch=std::clamp(pitch,-1.f,1.f);yaw=std::clamp(yaw,-.9f,.9f);}
  b.outside.compose(mesh,lower,lowerPhase,layer,pitch,yaw);
  // Aimed, the fitted gun is brought onto their line (twice converges).
  const float gunLength=std::max(.01f,m_weaponMesh.maximum.z-m_weaponMesh.minimum.z);
  if(aimed>0&&held){const float gunScale=b.outside.perMetre()*(held==1?1.f/gunLength:1.f);
   for(int pass=0;pass<2;++pass){const auto rh=b.outside.hand(0,false),lh=b.outside.hand(1,false);
    const auto gun=held==1?ClaireOutside::fitLongGun(rh,lh,TriggerSocket,SupportSocket,{0,0,-1},gunScale):ClaireOutside::fitPistol(rh,{},1);
    float dp=std::asin(std::clamp(aim.y,-1.f,1.f))-std::asin(std::clamp(gun.bore.y,-1.f,1.f)),dy=wrap(std::atan2(aim.x,aim.z)-std::atan2(gun.bore.x,gun.bore.z));
    pitch=std::clamp(pitch+dp,-1.2f,1.2f);yaw=std::clamp(yaw+dy,-1.f,1.f);b.outside.compose(mesh,lower,lowerPhase,layer,pitch,yaw);}}
  // A change of clip or hold blends over .2 s from where it was; a turn keeps the blend's start where it stood.
  const int key=lower*4096+(layer.clip+1)*64+(layer.mixClip+1)*2+(aimed>0)+(vr?1<<22:0);
  if(key!=b.key){if(b.key>=0){b.outside.blendFromLast();b.blendStart=now;}else b.turned=bodyAngle;b.key=key;}
  b.outside.turnBlendStart(wrap(bodyAngle-b.turned),(mesh.minimum+mesh.maximum)*.5f);b.turned=bodyAngle;
  b.outside.publish(mesh,b.blendStart<0?1.f:std::clamp((now-b.blendStart)/.2f,0.f,1.f));
  report.lower=lower;report.layer=layer.clip;report.held=held;
  // The gun in their hands (the shotgun in both, remembered in the right while the left reloads; the pistol in the right).
  ClaireOutside::Gun heldGun{};Point3 socket{};
  if(held){const bool pistol=held>=2;socket=pistol?Point3{0,-.022f,-.012f}:TriggerSocket;
   const auto rh=b.outside.hand(0);const auto lh=b.outside.hand(1);const float metre=b.outside.perMetre();
   auto lay=lh;if(r.prone){Point3 d=lh.grip-rh.grip;d.y*=.15f;lay.grip=rh.grip+d;}
   heldGun=pistol?ClaireOutside::fitPistol(rh,socket,metre):ClaireOutside::fitLongGun(rh,lay,socket,SupportSocket,{0,0,-1},metre/gunLength);
   const bool reloading=r.reloadAge<(pistol?1.5f:1.8f),bothHands=!reloading&&layer.clip!=Clip::HitReaction&&!r.prone;
   if(!pistol&&!r.prone){if(bothHands)b.outside.rememberInRightHand(rh,heldGun);else b.outside.gunFromRightHand(rh,heldGun);}
   const Point3 d=lh.grip-heldGun.origin;const float z=dot(d,heldGun.z)/heldGun.scale+socket.z;
   const bool support=bothHands&&(pistol?layer.clip==Clip::PistolAim||layer.clip==Clip::PistolShoot:true);
   if(support){const Point3 target=pistol?socket+Point3{.034f,-.004f,.004f}:Point3{SupportSocket.x,SupportSocket.y,std::clamp(z,-2.85f,-1.65f)};
    b.outside.reachLeft(mesh,ClaireOutside::place(heldGun,socket,target));}
   else if(!pistol&&reloading&&layer.clip!=Clip::HitReaction){
    const float cycle=std::fmod(r.reloadAge/1.8f*3.f,1.f),up=cycle<.35f?0.f:cycle<.75f?(cycle-.35f)/.4f:1.f,ease=up*up*(3-2*up);
    const Point3 hip=b.outside.centroid(ClaireOutside::Pelvis,true)+Point3{.17f,-.02f,.08f}*metre,port=ClaireOutside::place(heldGun,socket,{0,.55f,.35f});
    b.outside.reachLeft(mesh,hip+(port-hip)*ease);}}
  // Stood where they stand: 1.7 m from outside; a VR body as tall as the local one (its eyes at the standing eye),
  // so the IK arms on the solve's shoulders meet it.
  const Point3 center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;
  const float scale=(vr?(Player::StandingEye+.04f)/character.headFraction:1.7f)/std::max(.01f,range.y),angle=bodyAngle;
  const Vec2 at{r.position.x,r.position.y};const float feet=r.position.z;
  auto world=[&](Point3 p){p=(p-center)*scale;return Point3{at.x+p.z*std::cos(angle)+p.x*std::sin(angle),at.y+p.z*std::sin(angle)-p.x*std::cos(angle),feet+p.y+range.y*scale*.5f+.01f};};
  const Point3 receiver{at.x,at.y,feet+.85f};
  // A soft contact shadow under them, on the floor they stand on.
  {const float radius=r.prone?.6f:.38f;const Point3 c{at.x,at.y,feet+.02f};
   for(int k=0;k<12;++k){const float a0=k*kPiF/6,a1=(k+1)*kPiF/6;
    MeshVertex v0{cameraPoint(c,game),0,0},v1{cameraPoint({c.x+std::cos(a0)*radius,c.y+std::sin(a0)*radius,c.z},game),0,0},v2{cameraPoint({c.x+std::cos(a1)*radius,c.y+std::sin(a1)*radius,c.z},game),0,0};
    triangle3D(v0,v1,v2,shadowTexture,1.f);}}
  hooks.light(receiver,std::max(.2f,range.x*scale),std::max(.2f,range.z*scale),std::max(.2f,range.y*scale),kPiF*.5f-angle);
  Point3 low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f},head{};int headCount=0;
  const auto& faces=mesh.triangles;
  for(size_t f=0;f<faces.size();++f){if(vr&&f<character.arm.size()&&character.arm[f])continue;   // a VR player's arms are the IK arms below
   auto face=faces[f];for(auto& v:face.v)v.p=world(v.p);std::swap(face.v[1],face.v[2]);
   for(const auto& v:face.v){low={std::min(low.x,v.p.x),std::min(low.y,v.p.y),std::min(low.z,v.p.z)};high={std::max(high.x,v.p.x),std::max(high.y,v.p.y),std::max(high.z,v.p.z)};
    if(f<character.head.size()&&character.head[f]){head=head+v.p;++headCount;}}
   {bool near=false;for(const auto& v:face.v){const Point3 d=v.p-eye;near=near||dot(d,d)<.3f*.3f;}if(near)continue;}
   const Point3 n=cross(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p);const float len=std::sqrt(dot(n,n));
   hooks.face(n,(face.v[0].p+face.v[1].p+face.v[2].p)*(1.f/3.f)-receiver);
   hooks.tri(face.v[0],face.v[1],face.v[2],f<character.faceTexture.size()&&character.faceTexture[f]?*character.faceTexture[f]:m_claireTextures[f<character.texture.size()?character.texture[f]:0],.72f+.35f*std::fabs(n.z)/std::max(.001f,len));}
  // A VR player's arms: the first-person arm rig on the solve's shoulders, wrists on their tracked hands.
  if(vr){const AvatarSolve solve=solveAvatar(r.pose,now);const Point3 shift=r.position-r.pose.root;
   const VrRig rig=poseVrArms(m_armsMesh,solve);
   for(const auto& face:m_armsMesh.triangles){MeshVertex v[3];for(int i=0;i<3;++i){v[i]=face.v[i];v[i].p=rigToWorld(rig,solve,face.v[i].p)+shift;}
    std::swap(v[1],v[2]);const Point3 n=cross(v[1].p-v[0].p,v[2].p-v[0].p);hooks.face(n,(v[0].p+v[1].p+v[2].p)*(1.f/3.f)-receiver);
    const int m=materialOf(m_armsMesh,face.part);hooks.tri(v[0],v[1],v[2],m>=0?(&character==&all.characters[2]?m_leonArmTextures:m_claireTextures)[size_t(m)]:m_arms,.85f);}
   // The gun in the hand that holds it: the bore along the fingers, kept level about it.
   if(r.pose.inHand>=2&&r.pose.inHand<=4){const int g=r.pose.gunSide&1;const bool pistol=r.pose.inHand>=3,mac=r.pose.inHand==4;
    const Point3 wrist=rigToWorld(rig,solve,m_armsMesh.bonePosition(g?"hand.R":"hand.L"))+shift;
    const Point3 bore=unit(r.pose.hands[size_t(g)].forward),back=bore*-1.f;Point3 up=Point3{0,0,1}-bore*bore.z;up=unit(dot(up,up)<1e-4f?Point3{1,0,0}:up);const Point3 side=cross(back,up);
    const Point3 origin=wrist+bore*.07f;held=mac?3:pistol?2:1;
    auto place=[&](Point3 p){return origin+side*p.x+up*p.y+back*p.z;};
    if(pistol)for(auto face:(mac?m_macMesh:m_handgunMesh).triangles){for(auto& v:face.v)v.p=place({-v.p.x,v.p.y,-v.p.z});std::swap(face.v[1],face.v[2]);hooks.face(cross(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p),face.v[0].p-receiver);hooks.tri(face.v[0],face.v[1],face.v[2],mac?m_macTexture:m_handgunTexture,.8f);}
    else{const auto c=(m_weaponMesh.minimum+m_weaponMesh.maximum)*.5f,rr=m_weaponMesh.maximum-m_weaponMesh.minimum;const float s=.85f/std::max({rr.x,rr.y,rr.z});
     for(auto face:m_weaponMesh.triangles){if(face.part==3)continue;for(auto& v:face.v){const auto p=(v.p-c)*s;v.p=place({-p.x,p.y,p.z-.13f});}std::swap(face.v[1],face.v[2]);
      hooks.face(cross(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p),face.v[0].p-receiver);hooks.tri(face.v[0],face.v[1],face.v[2],m_weaponTexture,.8f);}}}}
  else if(held){const bool pistol=held>=2;const auto& gunMesh=held==3?m_macMesh:pistol?m_handgunMesh:m_weaponMesh;const auto& gunTexture=held==3?m_macTexture:pistol?m_handgunTexture:m_weaponTexture;
   for(auto face:gunMesh.bindTriangles()){if(!pistol&&face.part==3)continue;
    for(auto& v:face.v)v.p=world(ClaireOutside::place(heldGun,socket,v.p));std::swap(face.v[1],face.v[2]);
    {bool near=false;for(const auto& v:face.v){const Point3 d=v.p-eye;near=near||dot(d,d)<.3f*.3f;}if(near)continue;}
    const Point3 n=cross(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p);const float len=std::sqrt(dot(n,n));
    hooks.face(n,(face.v[0].p+face.v[1].p+face.v[2].p)*(1.f/3.f)-receiver);
    hooks.tri(face.v[0],face.v[1],face.v[2],gunTexture,.75f+.3f*std::fabs(n.z)/std::max(.001f,len));}}
  report.drawn=true;report.held=held;report.low=low;report.high=high;report.head=headCount?head*(1.f/float(headCount)):Point3{at.x,at.y,feet+1.6f};
  all.reports.push_back(report);
 }
}

// Name tags: small, quiet type over each head, only where the head can be seen from here (a co-op partner shows
// faintly through walls), fading out with distance. Drawn over the frame after the scene.
void SoftwareRenderer::drawNameTags(const Game& game){
 auto& all=remoteBodies();
 const auto bone=rgb(236,228,214),black=rgb(0,0,0),amber=rgb(232,160,60),red=rgb(214,32,26);
 const auto& v=m_view;const float cy=std::cos(v.angle),sy=std::sin(v.angle),cp=std::cos(v.pitch/140.f),sp=std::sin(v.pitch/140.f),focal=Projection{float(m_width)/m_height}.focal(float(m_width));
 const Point3 eye{v.pos.x,v.pos.y,v.z+v.eye},forward{cy*cp,sy*cp,sp},right{-sy,cy,0},up{-cy*sp,-sy*sp,cp};
 const bool coop=game.matchView().mode==net::Mode::Coop;
 for(const auto& r:game.remotePlayers()){
  RemoteBodyReport* report=nullptr;for(auto& rep:all.reports)if(rep.slot==r.slot)report=&rep;
  if(!r.alive&&r.deathAge>2.f)continue;
  const float top=report&&report->drawn?report->high.z:r.position.z+(r.prone?.45f:r.crouched?1.2f:1.75f);
  const Point3 head{r.position.x,r.position.y,top+.22f},d=head-eye;
  const float depth=d.x*forward.x+d.y*forward.y+d.z*forward.z;if(depth<.3f)continue;
  const float distance=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
  const bool seen=game.world().rayClear({eye.x,eye.y},eye.z,{head.x,head.y},head.z-.15f);
  const bool partner=coop&&r.team!=0;if(!seen&&!partner)continue;
  float alpha=std::clamp(1.f-(distance-14.f)/10.f,0.f,1.f)*(seen?.9f:.35f)*(r.alive?1.f:.5f);if(alpha<=.02f)continue;
  const float sx=m_width*.5f+(d.x*right.x+d.y*right.y+d.z*right.z)/depth*focal,syp=m_height*.5f-(d.x*up.x+d.y*up.y+d.z*up.z)/depth*focal;
  if(sx<-60||sx>m_width+60||syp<-20||syp>m_height+20)continue;
  std::string name=r.name.empty()?"PLAYER":r.name;for(auto& c:name)c=char(std::toupper(static_cast<unsigned char>(c)));
  while(name.size()>3&&uiTextWidth(name.c_str(),0,10,1)>120)name=name.substr(0,name.size()-4)+"...";
  const int w=uiTextWidth(name.c_str(),0,10,1),x=int(sx)-w/2,y=int(syp)-12;
  // Under the kill feed (top right, Game::matchView().feed) a tag steps back so the feed reads.
  if(x+w>m_width-230&&y<70+16*int(std::min<size_t>(5,game.matchView().feed.size())))alpha*=.25f;
  gradient(x-14,y-1,w/2+14,13,black,0.f,.5f*alpha,true);gradient(x+w/2,y-1,w/2+14,13,black,.5f*alpha,0.f,true);
  if(partner)fill(x-7,y+4,3,3,amber,alpha);else if(r.health<35&&r.alive)fill(x-7,y+4,3,3,red,alpha);
  uiText(x,y,name.c_str(),bone,0,10,alpha,1);
  // Speaking: three small bars after the name, the middle one tallest.
  if(r.speaking)for(int k=0;k<3;++k){const int h=k==1?8:5;fill(x+w+6+k*3,y+9-h,2,h,amber,alpha);}
  if(report){report->tag=true;report->tagX=sx;report->tagY=syp;}
 }
}
}
