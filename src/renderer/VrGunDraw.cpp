// The VR guns' working points measured from the meshes the VR hands draw (Game::VrGunGeometry): the muzzle, the
// sights, the ejection port, the pistol's magazine well and slide, the shotgun's loading port. drawTrackedHands sets
// them once its meshes are loaded; --vr-test checks the game's defaults against a fresh measurement.
#include "SoftwareRenderer.h"
#include <algorithm>
#include <cmath>
#include <ostream>
#include <sstream>
#include <fstream>
#include <vector>
namespace retro {
Point3 SoftwareRenderer::vrShotgunPoint(const Mesh& mesh,Point3 v){
 // The Remington scaled to 85 cm, its pistol grip in the fist (13 cm behind the model's centre), the muzzle at -z.
 const Point3 center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;const float scale=.85f/std::max({range.x,range.y,range.z});
 const Point3 p=(v-center)*scale;return {-p.x,p.y,p.z-.13f};
}
Game::VrGunGeometry SoftwareRenderer::measureVrGun(const Mesh& mesh,bool pistol){
 Game::VrGunGeometry g;struct V{Point3 q;int part;};std::vector<V> all;
 for(const auto&face:mesh.triangles)for(const auto&v:face.v)all.push_back({pistol?vrPistolPoint(v.p,true):vrShotgunPoint(mesh,v.p),face.part});
 if(all.empty())return Game::vrGunGeometry(pistol?WeaponId::Handgun:WeaponId::Shotgun);
 auto each=[&](auto keep,auto use){for(const auto&v:all)if(keep(v))use(v.q);};
 auto centroid=[&](auto keep){Point3 c{};int n=0;each(keep,[&](Point3 q){c=c+q;++n;});return n?c*(1.f/float(n)):Point3{};};
 auto highest=[&](auto keep){Point3 best{0,-1e9f,0};each(keep,[&](Point3 q){if(q.y>best.y)best=q;});return Point3{0,best.y,best.z};};
 if(pistol){
  // The slide (part 4): front face, sights on top, rear end. The magazine (5): base and top. The trigger (7)
  // marks the chamber over it, where the port is.
  float front=1e9f,rear=-1e9f,right=-1e9f,low=1e9f,high=-1e9f;
  each([](const V& v){return v.part==4;},[&](Point3 q){front=std::min(front,q.z);rear=std::max(rear,q.z);right=std::max(right,q.x);low=std::min(low,q.y);high=std::max(high,q.y);});
  const float length=rear-front;
  const Point3 face=centroid([&](const V& v){return v.part==4&&v.q.z<front+.004f;});g.muzzle={0,face.y,front};
  g.frontSight=highest([&](const V& v){return v.part==4&&v.q.z<front+length*.15f;});
  g.rearSight=highest([&](const V& v){return v.part==4&&v.q.z>rear-length*.3f;});
  const Point3 trigger=centroid([](const V& v){return v.part==7;});g.port={right+.001f,(low+high)*.5f+.01f,trigger.z-.005f};
  float magLow=1e9f,magHigh=-1e9f;each([](const V& v){return v.part==5;},[&](Point3 q){magLow=std::min(magLow,q.y);magHigh=std::max(magHigh,q.y);});
  const Point3 base=centroid([&](const V& v){return v.part==5&&v.q.y<magLow+.01f;}),top=centroid([&](const V& v){return v.part==5&&v.q.y>magHigh-.01f;});
  const Point3 d=top-base;const float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);g.magwell={0,base.y,base.z};g.magAxis=l>1e-4f?Point3{0,d.y/l,d.z/l}:Point3{0,1,0};g.magLength=l;
  const Point3 back=centroid([&](const V& v){return v.part==4&&v.q.z>rear-.01f;});g.slideRear={0,back.y,rear};g.loadingPort=g.magwell;
  return g;}
 // The Remington: the barrel's front ring (the higher of the two tubes that end at the muzzle), the bead on top of
 // it, the receiver's top behind the chamber, the ejection port on the receiver's right at the bore's height, the
 // loading port under the receiver ahead of the trigger guard. The model's shell (part 3) sits under the receiver,
 // at the loading port: its middle gives the receiver's place along the gun.
 float front=1e9f;each([](const V& v){return v.part!=3;},[&](Point3 q){front=std::min(front,q.z);});
 float top=-1e9f,bottom=1e9f;each([&](const V& v){return v.part!=3&&v.q.z<front+.012f;},[&](Point3 q){top=std::max(top,q.y);bottom=std::min(bottom,q.y);});
 // The barrel's ring: the front points in the top tube (within 2.2 cm of the top: a 12-bore barrel is ~2 cm across).
 float ringLow=1e9f,ringHigh=-1e9f;each([&](const V& v){return v.part!=3&&v.q.z<front+.012f&&v.q.y>top-.022f;},[&](Point3 q){ringLow=std::min(ringLow,q.y);ringHigh=std::max(ringHigh,q.y);});
 g.muzzle={0,(ringLow+ringHigh)*.5f,front};
 g.frontSight=highest([&](const V& v){return v.part!=3&&v.q.z<front+.04f;});
 const Point3 shell=centroid([](const V& v){return v.part==3;});
 g.rearSight=highest([&](const V& v){return v.part!=3&&v.part!=2&&v.q.z>shell.z-.05f&&v.q.z<shell.z+.08f;});
 float side=-1e9f;each([&](const V& v){return v.part!=3&&v.part!=2&&std::fabs(v.q.z-shell.z)<.03f;},[&](Point3 q){side=std::max(side,q.x);});
 g.port={side>-1e8f?side:shell.x,g.muzzle.y,shell.z};
 float under=1e9f;each([&](const V& v){return v.part!=3&&v.part!=2&&std::fabs(v.q.z-(shell.z-.02f))<.025f&&std::fabs(v.q.x)<.03f;},[&](Point3 q){under=std::min(under,q.y);});
 g.loadingPort={0,under<1e8f?under:shell.y-.04f,shell.z-.02f};g.magwell=g.loadingPort;g.slideRear=g.loadingPort;g.magAxis={0,1,0};g.magLength=.07f;
 (void)bottom;return g;
}
bool SoftwareRenderer::testVrGunGeometry(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto len=[](Point3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);};
 auto show=[](Point3 p){std::ostringstream o;o.precision(4);o<<std::fixed<<'{'<<p.x<<','<<p.y<<','<<p.z<<'}';return o.str();};
 const Mesh pistol(337),shotgun(109);
 if(pistol.triangles.empty()||shotgun.triangles.empty()){check(false,"the handgun (337) and shotgun (109) meshes load");return false;}
 for(bool isPistol:{true,false}){const auto m=measureVrGun(isPistol?pistol:shotgun,isPistol);const auto d=Game::vrGunGeometry(isPistol?WeaponId::Handgun:WeaponId::Shotgun);
  const char* name=isPistol?"handgun":"shotgun";
  report<<"INFO: "<<name<<" measured: muzzle "<<show(m.muzzle)<<" rear sight "<<show(m.rearSight)<<" front sight "<<show(m.frontSight)<<" port "<<show(m.port)<<" magwell "<<show(m.magwell)<<" mag axis "<<show(m.magAxis)<<" length "<<m.magLength<<" slide rear "<<show(m.slideRear)<<" loading port "<<show(m.loadingPort)<<'\n';
  float worst=std::max({len(m.muzzle-d.muzzle),len(m.rearSight-d.rearSight),len(m.frontSight-d.frontSight),len(m.port-d.port),len(m.loadingPort-d.loadingPort)});
  if(isPistol)worst=std::max({worst,len(m.magwell-d.magwell),len(m.slideRear-d.slideRear),len(m.magAxis-d.magAxis)*.1f,std::fabs(m.magLength-d.magLength)});
  // A side view (z across, y up) of the mesh in the gun frame, its parts in colours and the points as crosses:
  // vr-gun-<name>-side.ppm beside the report, for a look.
  {const float z0=isPistol?-.22f:-.62f,z1=isPistol?.08f:.28f,y0=isPistol?-.12f:-.2f,y1=isPistol?.1f:.16f;const int W=1000,H=int(float(W)*(y1-y0)/(z1-z0));
   std::vector<std::uint32_t> px(size_t(W*H),0xff202020u);
   auto sx=[&](Point3 q){return (q.z-z0)/(z1-z0)*float(W);};auto sy=[&](Point3 q){return (y1-q.y)/(y1-y0)*float(H);};
   auto put=[&](Point3 q,std::uint32_t c){const int x=int(sx(q)),y=int(sy(q));if(x>=0&&y>=0&&x<W&&y<H)px[size_t(y*W+x)]=c;};
   const std::uint32_t colours[8]={0xff707070u,0xff5050a0u,0xff408040u,0xffa08020u,0xffa05020u,0xff2080a0u,0xffa020a0u,0xffa0a0a0u};
   // Filled triangles, nearest-x on top (the right side faces the viewer: x toward the eye).
   std::vector<float> depth(size_t(W*H),-1e9f);
   for(const auto&face:(isPistol?pistol:shotgun).triangles){Point3 q[3];for(int i=0;i<3;++i)q[i]=isPistol?vrPistolPoint(face.v[i].p,true):vrShotgunPoint(shotgun,face.v[i].p);
    const float ax=sx(q[0]),ay=sy(q[0]),bx=sx(q[1]),by=sy(q[1]),cx=sx(q[2]),cy=sy(q[2]);const float area=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax);if(std::fabs(area)<1e-6f)continue;
    for(int y=std::max(0,int(std::min({ay,by,cy})));y<=std::min(H-1,int(std::max({ay,by,cy})));++y)for(int x=std::max(0,int(std::min({ax,bx,cx})));x<=std::min(W-1,int(std::max({ax,bx,cx})));++x){
     const float px0=float(x)+.5f,py0=float(y)+.5f,w0=((bx-px0)*(cy-py0)-(by-py0)*(cx-px0))/area,w1=((cx-px0)*(ay-py0)-(cy-py0)*(ax-px0))/area,w2=1-w0-w1;
     if(w0<0||w1<0||w2<0)continue;const float d=q[0].x*w0+q[1].x*w1+q[2].x*w2;if(d<depth[size_t(y*W+x)])continue;depth[size_t(y*W+x)]=d;
     const std::uint32_t c=colours[size_t(std::clamp(face.part,0,7))];const float shade=std::clamp(.75f+d*4.f,.4f,1.3f);
     auto ch=[&](int s){return std::uint32_t(std::clamp(float((c>>s)&255u)*shade,0.f,255.f))<<s;};px[size_t(y*W+x)]=0xff000000u|ch(16)|ch(8)|ch(0);}}
   auto mark=[&](Point3 q,std::uint32_t c){for(int k=-6;k<=6;++k){put(q+Point3{0,float(k)*.0015f,0},c);put(q+Point3{0,0,float(k)*.0015f},c);}};
   mark(m.muzzle,0xffff0000u);mark(m.rearSight,0xff00ff00u);mark(m.frontSight,0xff00ff00u);mark(m.port,0xffff00ffu);mark(m.loadingPort,0xff00ffffu);if(isPistol){mark(m.magwell,0xff00ffffu);mark(m.slideRear,0xffffff00u);}
   mark({0,0,0},0xffffffffu);
   std::ofstream file(std::string("vr-gun-")+name+"-side.ppm",std::ios::binary);file<<"P6\n"<<W<<' '<<H<<"\n255\n";for(auto p:px){const char rgb[3]={char(p>>16),char(p>>8),char(p)};file.write(rgb,3);}}
  check(worst<.004f,std::string(name)+": the game's gun points match the drawn mesh within 4 mm (worst "+std::to_string(int(worst*1000+.5f))+" mm)");
  // The sights sit over the bore, the line through them nearly parallel to it (a zero, not a skew).
  const Point3 sight=m.frontSight-m.rearSight;const float tilt=std::atan2(sight.y,-sight.z);
  check(m.rearSight.y>m.muzzle.y&&m.frontSight.y>m.muzzle.y&&std::fabs(tilt)<.03f&&m.muzzle.z<m.frontSight.z+.03f,std::string(name)+": the sights stand over the bore and their line is within 2 degrees of it");}
 return ok;
}
}
