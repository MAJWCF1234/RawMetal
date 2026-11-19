// VR hands on Vectra, driven by scripted hand poses (no headset): lab/fixtures/vr-hands-*.lab, `scene`.
//  carry: a crate on the floor is taken in the right hand, then the left closes on its other side: two hands. Both
//         lift it chest high (it follows their midpoint), the left rises (it tilts along the line between them),
//         both wrists tip forward (it rolls about that line); the left lets go (one hand), then the right (it falls).
//  push:  the right hand swats through a light box at 4 m/s (the box moves, no faster than the hand's 2.5 m/s cap,
//         and the drawn hand is never inside it); the left hand presses into a 35 kg drum (the drum stays, the
//         drawn hand stops at its surface while the tracked hand goes on in).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "game/physics/PropBodies.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Watch {std::map<std::string,double> v;int a=-1,b=-1;Point3 startA{},startB{};double ms=0;int steps=0;};
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
int findModel(Game& g,int model){const auto& l=LabAccess::clutter(g);for(int i=0;i<int(l.size());++i)if(l[size_t(i)].model==model)return i;return -1;}
Point3 mid(const Clutter& c){return {c.pos.x,c.pos.y,c.z+c.extent()[2]};}
float lerp(float a,float b,float t){return a+(b-a)*std::clamp(t,0.f,1.f);}
// How far inside the prop's own shape a point is (positive inside, less the hand's radius).
float inside(const Clutter& c,Point3 p,float radius){vectra::Body b;vectra::toBody(c,b);float worst=-1e9f;
 for(int k=0;k<b.shape->count;++k){vectra::V3 s,n;float d;if(vectra::nearestPoint(b,k,{p.x,p.y,p.z},s,n,d))worst=std::max(worst,radius-d);}return worst;}
constexpr Vec2 Stand{4.9f,8.5f};
// The lab has no headset to draw the tracked hands: the last shot gets each hand's place marked (a disc the size of a
// fist at that distance, the line between the two grips), so the picture shows where the hands hold the prop.
void markHands(Scene& s,Game& g){if(s.shots.empty())return;auto& shot=s.shots.back();const int W=shot.width,H=shot.height;
 const Point3 a=g.vrHandTracked(1),b=g.vrHandTracked(0);const auto pa=project(s,a),pb=project(s,b);
 auto put=[&](int x,int y,std::uint32_t c){if(x>=0&&y>=0&&x<W&&y<H)shot.pixels[size_t(y*W+x)]=c;};
 if(pa.visible&&pb.visible)for(int k=0;k<=200;++k){const float t=float(k)/200;const int x=int(pa.x+(pb.x-pa.x)*t),y=int(pa.y+(pb.y-pa.y)*t);for(int d=-1;d<=1;++d)put(x,y+d,0xffe8d0a0);}
 for(const auto& [p,w]:{std::pair{pa,a},std::pair{pb,b}}){if(!p.visible)continue;const auto q=project(s,w+Point3{0,0,.035f});const float r=std::max(4.f,std::fabs(q.y-p.y));
  for(int y=int(p.y-r)-2;y<=int(p.y+r)+2;++y)for(int x=int(p.x-r)-2;x<=int(p.x+r)+2;++x){const float d=std::sqrt((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y));if(d<=r)put(x,y,0xffd8a888);else if(d<=r+2)put(x,y,0xff3a2418);}}}
// The two-hand carry seen by someone standing off to the side (a second camera, the player put back after).
void observe(Scene& s,Game& g,const std::string& label){const Player keep=LabAccess::player(g);const Point3 m=(g.vrHandTracked(0)+g.vrHandTracked(1))*.5f;
 const Vec2 at{m.x+1.7f,m.y-1.5f};LabAccess::setCamera(g,at,0,std::atan2(m.y-at.y,m.x-at.x),140.f*std::atan2(m.z-1.42f,length(Vec2{m.x,m.y}-at)));
 s.renderer->render(g);s.shoot(label);markHands(s,g);LabAccess::player(g)=keep;LabAccess::playerCamera(g);}
const bool registered=add({
 .name="vr-hands",.summary="VR hands on Vectra from scripted poses: a crate carried in two hands (follows, tilts, rolls, back to one hand, dropped); hands as kinematic bodies that push a light box no faster than 2.5 m/s and stop at a 35 kg drum's surface",
 .fixtures={"vr-hands-carry","vr-hands-push"},
 .author=[](Scene& s,AuthoredMapData& m){const std::string scene=s.fixture->value("scene");
  if(scene=="carry")kit::prop(m,model::wooden_crate,6.f,8.5f,0,0,.6f);
  else{kit::prop(m,model::prop_cardboard_box,6.2f,7.6f,0,0,.8f);kit::prop(m,model::drum,6.3f,9.4f,0,.58f,.58f,.88f,0);}},
 .setup=[](Scene& s){watches().erase(&s);auto& w=watches()[&s];Game& g=s.g();LabAccess::setGod(g,true);LabAccess::playerCamera(g);auto& p=LabAccess::player(g);p.pos=Stand;p.z=0;const bool carry=s.fixture->value("scene")=="carry";p.angle=0;p.pitch=carry?-40.f:-70.f;
  const std::string scene=s.fixture->value("scene");
  if(scene=="carry")w.a=findModel(g,model::wooden_crate);else{w.a=findModel(g,model::prop_cardboard_box);w.b=findModel(g,model::drum);}
  if(w.a>=0)w.startA=mid(LabAccess::clutter(g)[size_t(w.a)]);if(w.b>=0)w.startB=mid(LabAccess::clutter(g)[size_t(w.b)]);},
 .step=[](Scene& s,float dt){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");const int f=s.frame;auto& p=LabAccess::player(g);
  s.input.vrTracked=true;s.input.vrYaw=p.angle;s.input.vrPitch=p.pitch;s.input.vrEye=1.42f;
  auto hand=[&](int side,Point3 at,Point3 dir,Point3 vel){g.setVrHand(side,at,dir,true,vel);};
  if(scene=="carry"&&w.a>=0){const Point3 c0=w.startA;const float half=.33f+.03f;
   // Height, forward shift, the left hand's extra rise and both wrists' tip, by frame.
   const float lift=lerp(0,.75f,(f-22)/48.f),toward=lerp(0,-.4f,(f-22)/48.f),rise=lerp(0,.25f,(f-72)/26.f),tip=lerp(0,.5f,(f-100)/18.f);
   Point3 right{c0.x+toward,c0.y-half,c0.z+.02f+lift},left{c0.x+toward,c0.y+half,c0.z+.02f+lift+rise};
   // Once the right hand has it (the one-hand carry holds it before the hand), the left closes on its left face, and
   // moves with the right from then on.
   static std::map<const Scene*,Point3> reach;if(f==18){const Point3 m=mid(LabAccess::clutter(g)[size_t(w.a)]);reach[&s]=m+Point3{0,.17f,0}-right;}
   if(f>=18)left=right+reach[&s]+Point3{0,0,rise};
   const Point3 dir{std::cos(tip),0,-std::sin(tip)};
   static std::map<const Scene*,std::array<Point3,2>> last;auto& l=last[&s];if(f==0)l={right,left};
   hand(1,right,dir,(right-l[0])*(1/dt));hand(0,left,dir,(left-l[1])*(1/dt));l={right,left};
   s.input.vrGrip[1]=f>=10&&f<140;s.input.vrGrip[0]=f>=20&&f<124;
   const auto& c=LabAccess::clutter(g)[size_t(w.a)];const Point3 m=mid(c);
   if(f==12)w.v["oneHand"]=LabAccess::held(g)==w.a&&g.vrSecondHand()<0;
   if(f==22){w.v["twoHands"]=LabAccess::held(g)==w.a&&g.vrSecondHand()==0;
    // The line between the hands in the crate's own frame as they took it.
    const auto q=vectra::propOrientation(c);const auto d=q.unrotate(vectra::norm(vectra::V3{left.x-right.x,left.y-right.y,left.z-right.z}));w.v["ax"]=d.x;w.v["ay"]=d.y;w.v["az"]=d.z;}
   // From the two-hand grab to here the hands only moved (no turn): the crate moved with their midpoint.
   if(f==22){const Point3 hm=(right+left)*.5f;w.v["m22x"]=m.x-hm.x;w.v["m22y"]=m.y-hm.y;w.v["m22z"]=m.z-hm.z;}
   if(f==70){const Point3 hm=(right+left)*.5f;const double dx=m.x-hm.x-w.v["m22x"],dy=m.y-hm.y-w.v["m22y"],dz=m.z-hm.z-w.v["m22z"];w.v["followErr"]=std::sqrt(dx*dx+dy*dy+dz*dz);w.v["lifted"]=m.z-c0.z;observe(s,g,"two-hands-lifted");}
   if(f==99){const auto q=vectra::propOrientation(c);const auto a=q.rotate({float(w.v["ax"]),float(w.v["ay"]),float(w.v["az"])});const auto h=vectra::norm(vectra::V3{left.x-right.x,left.y-right.y,left.z-right.z});
    // The crate's own grip line tilts as the line between the hands does; its up, kept to measure the roll from.
    w.v["tilt"]=std::asin(std::clamp(a.z,-1.f,1.f))*57.2958f;w.v["tiltWant"]=std::asin(std::clamp(h.z,-1.f,1.f))*57.2958f;const auto u=q.rotate({0,0,1});w.v["ux"]=u.x;w.v["uy"]=u.y;w.v["uz"]=u.z;observe(s,g,"two-hands-tilted");}
   if(f==118){const auto u=vectra::propOrientation(c).rotate({0,0,1});
    // Both wrists tipped 29 degrees: the crate turns about the line between the hands by their roll across it.
    w.v["roll"]=std::acos(std::clamp(u.x*float(w.v["ux"])+u.y*float(w.v["uy"])+u.z*float(w.v["uz"]),-1.f,1.f))*57.2958f;observe(s,g,"two-hands-rolled");}
   // (One hand of two letting go is acted on after Game::TwoHandGrace: by frame 139 the right holds it alone.)
   if(f==139)w.v["backToOne"]=LabAccess::held(g)==w.a&&g.vrSecondHand()<0;
   if(f==141)w.v["dropped"]=LabAccess::held(g)<0;}
  if(scene=="push"&&w.a>=0&&w.b>=0){
   // Right: a swat through the box at 4 m/s, 5.6 -> 7.0 along x at the box's middle height. Left: into the drum at 1 m/s.
   const float t=(f-20)*dt;Point3 right{lerp(5.6f,7.0f,t*4/1.4f),w.startA.y,w.startA.z},left{lerp(5.75f,6.4f,(f-20)*dt),w.startB.y,w.startB.z+.1f};
   if(f<20){right.x=5.6f;left.x=5.75f;}
   hand(1,right,{1,0,0},f>=20&&right.x<7.f?Point3{4,0,0}:Point3{});hand(0,left,{1,0,0},f>=20&&left.x<6.4f?Point3{1,0,0}:Point3{});
   const auto& box=LabAccess::clutter(g)[size_t(w.a)];const auto& drum=LabAccess::clutter(g)[size_t(w.b)];
   const float speed=std::sqrt(box.velocity.x*box.velocity.x+box.velocity.y*box.velocity.y+box.vz*box.vz);w.v["boxSpeed"]=std::max(w.v["boxSpeed"],double(speed));
   if(f>1){w.v["rightInside"]=std::max(w.v["rightInside"],double(inside(box,g.vrHandProxy(1),.045f)));w.v["leftInside"]=std::max(w.v["leftInside"],double(inside(drum,g.vrHandProxy(0),.045f)));}
   if(f==26)s.shoot("hand-swat");
   if(f==s.frames-1){const Point3 bm=mid(box),dm=mid(drum);w.v["boxMoved"]=length(Vec2{bm.x,bm.y}-Vec2{w.startA.x,w.startA.y});w.v["drumMoved"]=length(Vec2{dm.x,dm.y}-Vec2{w.startB.x,w.startB.y});
    const Point3 lp=g.vrHandProxy(0),lt=left;w.v["leftHeldOut"]=std::sqrt((lp.x-lt.x)*(lp.x-lt.x)+(lp.y-lt.y)*(lp.y-lt.y)+(lp.z-lt.z)*(lp.z-lt.z));w.v["leftBlocked"]=g.vrHandBlocked(0);}}
  const auto& st=LabAccess::rigidStats(g);w.ms+=st.ms;++w.steps;},
 .metrics=[](Scene& s,Report& r){auto& w=watches()[&s];const std::string scene=s.fixture->value("scene");
  r.metric("rigid_ms_per_frame",w.steps?w.ms/w.steps:0,"ms");
  if(scene=="carry"){
   r.check("one hand takes the crate",w.v["oneHand"]>0);r.check("the other hand closing on it makes it two-handed",w.v["twoHands"]>0);
   r.check("lifted with both hands, it follows their midpoint (within 4 cm)",w.v["lifted"]>.6&&w.v["followErr"]<.04,"lifted "+f2(w.v["lifted"])+" m, off "+f2(w.v["followErr"],3)+" m");
   r.check("the left hand rising tilts it as the line between the hands (within 3 degrees)",std::fabs(w.v["tilt"]-w.v["tiltWant"])<3,f2(w.v["tilt"],1)+" deg, want "+f2(w.v["tiltWant"],1));
   r.check("both wrists tipping forward roll it about that line (12 to 35 degrees for a 29 degree tip, part of it across the line)",w.v["roll"]>12&&w.v["roll"]<35,f2(w.v["roll"],1)+" deg");
   r.check("letting go with the left leaves it in the right",w.v["backToOne"]>0);r.check("letting go with the right drops it",w.v["dropped"]>0);
   r.metric("tilt",w.v["tilt"],"deg");r.metric("roll",w.v["roll"],"deg");}
  else{
   r.check("the swat moved the light box",w.v["boxMoved"]>.1,f2(w.v["boxMoved"])+" m");
   r.check("no faster than the hand's push cap (2.5 m/s, 2.9 with a bounce)",w.v["boxSpeed"]<2.9,f2(w.v["boxSpeed"])+" m/s");
   r.check("the drawn hand never went into the box (5 mm)",w.v["rightInside"]<.005,f2(w.v["rightInside"]*1000,1)+" mm");
   r.check("the 35 kg drum did not move under the hand",w.v["drumMoved"]<.02,f2(w.v["drumMoved"],3)+" m");
   r.check("the drawn hand stopped at the drum's surface (blocked, held out 5 cm and more, never inside)",w.v["leftBlocked"]>0&&w.v["leftHeldOut"]>.05&&w.v["leftInside"]<.005,"held out "+f2(w.v["leftHeldOut"],3)+" m, inside "+f2(w.v["leftInside"]*1000,1)+" mm");
   r.metric("box_speed",w.v["boxSpeed"],"m/s");}}});
}
}
