// Physgloves (src/game/Physgloves.cpp): point, flick, catch. Each fixture (lab/fixtures/physgloves-*.lab, `scene`):
//  pull:      a box on a high shelf 5 m off is pulled with G: it flies an arc and is caught in the hand (seen from the side too).
//  container: a bottle in an open crate is pulled out of it and caught; the crate stays where it was.
//  miss:      the mug is pulled and the player turns away: it is not caught and lands on the floor.
//  heavy:     a 35 kg drum and a 9 kg crate are pulled: the drum only lurches, the crate hops part way; neither is caught.
//  hung:      the Starter Map's crate on a rope (SHOWCASE: PHYSICS) is pulled: it swings out toward the player on its rope.
//  vr:        a tracked hand points at the mug, closes (it locks on), flicks back: the mug lands in the closed hand.
//  mouse:     the middle button held empty-handed locks on the mug, a flick of the mouse back pulls it; caught.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "world/GibSets.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Watch {std::map<std::string,double> v;int a=-1,b=-1;Point3 start{},startB{};int pulledAt=-1;std::vector<Point3> path;};
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
Point3 mid(const Clutter& c){return {c.pos.x,c.pos.y,c.z+c.height()*.5f};}
float dist(Point3 a,Point3 b){const Point3 d=a-b;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
int findModel(Game& g,int model,int skip=-1){const auto& l=LabAccess::clutter(g);for(int i=0;i<int(l.size());++i)if(l[size_t(i)].model==model&&i!=skip)return i;return -1;}
constexpr Vec2 Stand{4.f,8.5f};
const bool registered=add({
 .name="physgloves",.summary="Physgloves: point at a loose prop and flick; it pops to the hand on an arc and is caught, falls if missed, lurches if heavy, swings on its rope if hung; desktop G, the mouse flick and a VR hand",
 .fixtures={"physgloves-pull","physgloves-container","physgloves-miss","physgloves-heavy","physgloves-hung","physgloves-vr","physgloves-mouse","physgloves-starter"},
 .author=[](Scene& s,AuthoredMapData& m){const std::string scene=s.fixture->value("scene");
  // A high shelf (a board on two posts, its top 1.6 m up) 5 m ahead of where the player stands.
  kit::wall(m,8.8f,8.1f,9.4f,8.9f,1.55f,1.6f,40);kit::wall(m,8.85f,8.15f,8.95f,8.25f,0,1.55f,40);kit::wall(m,8.85f,8.75f,8.95f,8.85f,0,1.55f,40);
  if(scene=="pull")kit::prop(m,model::prop_cardboard_box,9.1f,8.5f,1.6f,.3f,.6f);   // (a box, for the picture)
  if(scene=="miss"||scene=="vr"||scene=="mouse")kit::prop(m,model::mug,9.1f,8.5f,1.6f,0);
  if(scene=="container"){kit::prop(m,model::market_crate,8.6f,8.5f,0,0);kit::prop(m,model::glass_bottle,8.6f,8.5f,-999,0);}
  if(scene=="heavy"){kit::prop(m,model::drum,8.4f,7.6f,0,.58f,.58f,.88f,0);kit::prop(m,model::wooden_crate,8.4f,9.6f,0,0,.5f);}},
 .setup=[](Scene& s){watches().erase(&s);auto& w=watches()[&s];Game& g=s.g();LabAccess::setGod(g,true);const std::string scene=s.fixture->value("scene");
  if(scene=="hung"){w.a=-1;float best=1e9f;const auto& l=LabAccess::clutter(g);for(int i=0;i<int(l.size());++i)if(l[size_t(i)].model==model::wooden_crate&&l[size_t(i)].z>2.f){const float d=length(l[size_t(i)].pos-Vec2{12,11.2f});if(d<best){best=d;w.a=i;}}
   LabAccess::playerCamera(g);auto& p=LabAccess::player(g);p.pos={12.f,15.6f};p.z=0;p.angle=-kPi/2;p.pitch=140.f*std::atan2(3.1f-1.42f,4.4f);}
  else{LabAccess::playerCamera(g);auto& p=LabAccess::player(g);p.pos=scene=="starter"?Vec2{2.6f,19.4f}:scene=="pull"?Vec2{5.4f,8.5f}:Stand;p.z=0;p.angle=0;
   if(scene=="starter"){const auto& l=LabAccess::clutter(g);float best=1e9f;for(int i=0;i<int(l.size());++i)if(l[size_t(i)].model==model::mug&&l[size_t(i)].z>1.5f){const float d=length(l[size_t(i)].pos-Vec2{1.7f,22.7f});if(d<best){best=d;w.a=i;}}}
   else if(scene=="container"){w.a=findModel(g,model::glass_bottle);w.b=findModel(g,model::market_crate);}
   else if(scene=="heavy"){w.a=findModel(g,model::drum);w.b=findModel(g,model::wooden_crate);}
   else w.a=findModel(g,scene=="pull"?model::prop_cardboard_box:model::mug);
   // Looking at the target's middle.
   if(w.a>=0){auto m=mid(LabAccess::clutter(g)[size_t(w.a)]);if(scene=="container")m.z+=.13f;   // (at its neck, over the crate's rim)
   p.angle=std::atan2(m.y-p.pos.y,m.x-p.pos.x);p.pitch=140.f*std::atan2(m.z-1.42f,length(Vec2{m.x,m.y}-p.pos));}}
  if(w.a>=0)w.start=mid(LabAccess::clutter(g)[size_t(w.a)]);if(w.b>=0)w.startB=mid(LabAccess::clutter(g)[size_t(w.b)]);},
 .step=[](Scene& s,float){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");const int f=s.frame;auto& p=LabAccess::player(g);
  const int pull=20;
  if(scene=="vr"){
   // The free hand (not the gun hand: a hand holding a drawn gun does not lock on, Game::physgloveHandFree) at the
   // chest, pointing at the mug: closed at the pull frame (it locks), snapped back two frames on.
   const Point3 head{p.pos.x,p.pos.y,p.z+1.42f},hand{p.pos.x+.35f,p.pos.y-.18f,p.z+1.15f};const Point3 m=w.start;Point3 d=m-hand;const float n=dist(m,hand);d=d*(1/n);
   Point3 v{};if(f==pull+2||f==pull+3)v=Point3{(head.x-hand.x)*3,(head.y-hand.y)*3,(head.z-hand.z)*3+1.5f};
   g.setVrHand(0,hand,d,true,v);g.setVrHand(1,{p.pos.x+.3f,p.pos.y+.25f,p.z+1.f},{1,0,0},true);
   s.input.vrTracked=true;s.input.vrYaw=p.angle;s.input.vrPitch=p.pitch;s.input.vrEye=1.42f;s.input.vrGrip[0]=f>=pull;}
  else if(scene=="mouse"){if(f>=pull)s.input.rotateProp=true;if(f==pull+3)s.input.mouseDy=-40;}
  else if(f==pull)s.input.physglove=true;
  if(f==pull)w.v["targeted"]=g.physgloves().target==w.a;
  if(scene=="heavy"&&f==pull+40){s.input.physglove=false;}
  if(scene=="heavy"&&f==pull+60){const auto m=mid(LabAccess::clutter(g)[size_t(w.b)]);p.angle=std::atan2(m.y-p.pos.y,m.x-p.pos.x);p.pitch=140.f*std::atan2(m.z-1.42f,length(Vec2{m.x,m.y}-p.pos));}
  if(scene=="heavy"&&f==pull+62)s.input.physglove=true;
  if(scene=="miss"&&f>=pull+4)p.angle=kPi*.5f;   // turned aside as it comes
  if(g.physgloves().flying>=0&&w.pulledAt<0)w.pulledAt=f;
  if(w.pulledAt>=0&&w.a>=0&&LabAccess::held(g)!=w.a&&w.path.size()<120)w.path.push_back(mid(LabAccess::clutter(g)[size_t(w.a)]));
  if(w.a>=0){const auto& c=LabAccess::clutter(g)[size_t(w.a)];const auto m=mid(c);
   w.v["maxRise"]=std::max(w.v["maxRise"],double(m.z-w.start.z));
   w.v["moved"]=std::max(w.v["moved"],double(length(Vec2{m.x,m.y}-Vec2{w.start.x,w.start.y})));
   w.v["toward"]=std::max(w.v["toward"],double(length(Vec2{w.start.x,w.start.y}-p.pos)-length(Vec2{m.x,m.y}-p.pos)));
   if(LabAccess::held(g)==w.a&&w.v["caughtAt"]==0)w.v["caughtAt"]=f;}
  if(w.b>=0){const auto m=mid(LabAccess::clutter(g)[size_t(w.b)]);w.v["movedB"]=std::max(w.v["movedB"],double(length(Vec2{m.x,m.y}-Vec2{w.startB.x,w.startB.y})));w.v["towardB"]=std::max(w.v["towardB"],double(length(Vec2{w.startB.x,w.startB.y}-p.pos)-length(Vec2{m.x,m.y}-p.pos)));}},
 .capture=[](Scene& s,const Frame& f){auto& w=watches()[&s];const bool pull=s.fixture->value("scene")=="pull";
  // The pull seen from beside the arc (a second camera; the player put back after): its path so far as a dotted trail
  // and the hand it flies to marked (the lab draws no hands).
  auto side=[&](const std::string& label){if(!pull||w.a<0)return;Game& g=s.g();const Player keep=LabAccess::player(g);const Point3 hand=g.physgloveCatchPoint(-1,LabAccess::clutter(g)[size_t(w.a)]);
   LabAccess::setCamera(g,{7.45f,4.75f},0,kPi*.5f,26.f);s.renderer->render(g);s.shoot(label);auto& shot=s.shots.back();const int W=shot.width,H=shot.height;
   auto put=[&](int x,int y,std::uint32_t c){if(x>=0&&y>=0&&x<W&&y<H)shot.pixels[size_t(y*W+x)]=c;};
   for(size_t k=0;k+1<w.path.size();k+=2){const auto q=project(s,w.path[k]);if(!q.visible)continue;for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx)if(dx*dx+dy*dy<=9)put(int(q.x)+dx,int(q.y)+dy,0xfff0d890);}
   if(const auto q=project(s,hand);q.visible){const auto e=project(s,hand+Point3{0,0,.05f});const float r=std::max(6.f,std::fabs(e.y-q.y));
    for(int y=int(q.y-r)-2;y<=int(q.y+r)+2;++y)for(int x=int(q.x-r)-2;x<=int(q.x+r)+2;++x){const float d=std::sqrt((x-q.x)*(x-q.x)+(y-q.y)*(y-q.y));if(d<=r)put(x,y,0xffd8a888);else if(d<=r+2)put(x,y,0xff3a2418);}}
   LabAccess::player(g)=keep;LabAccess::playerCamera(g);};
  if(w.pulledAt<0){if(f.index==19){s.shoot("highlighted");side("side-0-target");}return;}const int since=f.index-w.pulledAt;
  for(int k:{1,9,17,25,33})if(since==k)s.shoot("flight-"+std::to_string(k));
  for(int k:{4,10,16,22})if(since==k)side("side-"+std::to_string(k));
  if(w.v["caughtAt"]>0&&f.index==int(w.v["caughtAt"])+2)side("side-caught");},
 .metrics=[](Scene& s,Report& r){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");const auto& gl=g.physgloves();
  r.metric("pulls",gl.pulls);r.metric("catches",gl.catches);r.metric("misses",gl.misses);r.metric("max_rise",w.v["maxRise"],"m");r.metric("moved",w.v["moved"],"m");
  if(w.v["caughtAt"]>0&&w.pulledAt>=0)r.metric("flight",(w.v["caughtAt"]-w.pulledAt)/60.,"s");
  r.check("the target was found and highlighted before the pull",w.v["targeted"]>0||scene=="vr");
  if(w.a<0){r.check("the scene has its prop",false);return;}
  const auto& c=LabAccess::clutter(g)[size_t(w.a)];
  if(scene=="starter"){
   // The Starter Map's phase-3 corner (SHOWCASE: PHYSICS): its bottles hold water, oil and chemical, its breakables
   // have authored gibs, its crate hangs on a rope.
   int kinds=0;bool gibbed=true;const auto& l=LabAccess::clutter(g);for(int k=0;k<3;++k){bool found=false;for(const auto& b:l)found|=b.liquid.kind==liquid::Kind(k)&&b.liquid.level>.5f&&b.pos.y>21&&b.pos.y<22.5f;kinds+=found;}
   for(int m:{model::wooden_chair,model::scrap_chair,model::crt_monitor,model::packing_crate}){bool found=false;for(const auto& b:l)found|=b.model==m&&b.pos.y>19&&b.pos.x<9.6f;gibbed&=found&&gibs::setOf(m)!=nullptr;}
   bool hung=false;for(const auto& j:g.world().joints())hung|=j.propA>=0&&j.propB<0&&j.propA<int(g.world().clutterSpawns().size())&&g.world().clutterSpawns()[size_t(j.propA)].model==model::wooden_crate&&g.world().clutterSpawns()[size_t(j.propA)].position.y>21;
   r.check("the corner's bottles hold water, oil and chemical",kinds==3,std::to_string(kinds)+" kinds");
   r.check("its breakables have authored gibs",gibbed);r.check("its crate hangs on a rope",hung);}
  if(scene=="pull"||scene=="container"||scene=="vr"||scene=="mouse"||scene=="starter"){
   r.check("pulled once and caught in the hand",gl.pulls==1&&gl.catches==1&&LabAccess::held(g)==w.a,"pulls "+std::to_string(gl.pulls)+" catches "+std::to_string(gl.catches)+" held "+std::to_string(LabAccess::held(g)));
   r.check("it flew an arc (rose above its start on the way)",w.v["maxRise"]>.15,f2(w.v["maxRise"])+" m");
   if(w.v["caughtAt"]>0)r.check("caught within a second of the flick",w.v["caughtAt"]-w.pulledAt<=60,f2((w.v["caughtAt"]-w.pulledAt)/60.)+" s");
   if(scene=="container")r.check("the crate it lay in stayed put",w.v["movedB"]<.05,f2(w.v["movedB"],3)+" m");}
  if(scene=="miss"){r.check("pulled, not caught: a miss",gl.pulls==1&&gl.catches==0&&gl.misses==1&&LabAccess::held(g)!=w.a);
   r.check("it fell on and lies on the floor",c.z<.05f&&c.sleeping,"z "+f2(c.z,3)+(c.sleeping?" asleep":" awake"));
   r.check("it came most of the way before falling",w.v["toward"]>2.,f2(w.v["toward"])+" m");}
  if(scene=="heavy"){r.check("neither was caught",gl.catches==0&&LabAccess::held(g)<0);
   r.check("the 35 kg drum only lurched (moved under 0.6 m)",w.v["moved"]>.01&&w.v["moved"]<.6,f2(w.v["moved"],3)+" m");
   r.check("the 9 kg crate hopped toward the player, part way (0.3 to 3.5 m)",w.v["towardB"]>.3&&w.v["towardB"]<3.5,f2(w.v["towardB"])+" m");}
  if(scene=="hung"){bool snapped=false;for(const auto& j:g.world().jointStates())snapped|=j.broken;
   r.check("the hung crate swung toward the player on its rope",w.v["toward"]>.15,f2(w.v["toward"])+" m");
   r.check("its rope held and it was not caught",!snapped&&gl.catches==0&&c.z>1.5f,"z "+f2(c.z));}}});
}
}
