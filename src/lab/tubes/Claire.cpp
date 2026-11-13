// Claire from outside: the third-person camera over her shoulder while she
// turns through her holds (fists, the shotgun, the pistol, crouched). With the
// main tree's ClaireOutside (renderer/ClaireOutside.h) the tube also reads her
// pose report: hands on the gun, feet on the floor, aim.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
struct Hold {const char* name;WeaponId weapon;bool crouch,aim;};
constexpr Hold Holds[]={{"fists",WeaponId::Fists,false,false},{"shotgun",WeaponId::Shotgun,false,false},{"pistol-crouched",WeaponId::Handgun,true,false},{"pistol-aimed",WeaponId::Handgun,false,true}};
// How much of the frame is her: the frame against the same moment with her body hidden (environment
// inspection), over the middle of the frame (the HUD's bands at the top and bottom left out).
double cover(Scene& s,const Image& with){auto& r=*s.renderer;r.setEnvironmentInspection(true);r.render(s.g());Image without=Image::of(r.pixels(),r.width(),r.height());r.setEnvironmentInspection(false);r.render(s.g());
 size_t moved=0,n=0;for(int y=with.height*15/100;y<with.height*75/100;++y)for(int x=0;x<with.width;++x){auto a=with.pixels[size_t(y)*with.width+x],b=without.pixels[size_t(y)*with.width+x];int m=0;for(int k=0;k<24;k+=8)m=std::max(m,std::abs(int((a>>k)&255)-int((b>>k)&255)));moved+=m>30;++n;}
 return n?double(moved)/double(n):0;}
int holdAt(const Scene& s){return std::min(3,s.frame*4/std::max(1,s.frames));}
const bool claire=add({
 .name="claire",.summary="third-person Claire turning through her holds (fists, shotgun, pistol, crouched) over her shoulder",
 .friends={},.fixtures={"claire-turn"},
 .setup=[](Scene& s){LabAccess::playerCamera(s.g());SoftwareRenderer::setThirdPerson(true);LabAccess::setGod(s.g(),true);LabAccess::equip(s.g(),Holds[0].weapon);},
 .step=[](Scene& s,float dt){
  Game& g=s.g();const int hold=holdAt(s);if(hold!=int(s.board["claire.hold"])||s.frame==0){s.board["claire.hold"]=hold;LabAccess::equip(g,Holds[hold].weapon);}
  // The pistol comes up to the eye when it fires (and stays up 1.5 s): one shot early in the aimed hold.
  LabAccess::player(g).crouched=Holds[hold].crouch;s.input.guard=Holds[hold].aim;s.input.fire=Holds[hold].aim&&s.frame%std::max(1,s.frames/4)==2;
  #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
  // A turnaround: the camera goes once round her in each hold, 2.6 m off (SoftwareRenderer::setBodyOrbit).
  const int per=std::max(1,s.frames/4);s.renderer->setBodyOrbit(tuned("claire.orbit",2.6f),6.2832f*float(s.frame%per)/float(per));(void)dt;
  #else
  // Without the film camera she turns instead: the room wheels behind her and the light comes round.
  LabAccess::player(g).angle+=dt*6.2832f*4.f/std::max(1.f,float(s.frames)*dt);
  #endif
  },
 .capture=[](Scene& s,const Frame& f){
  // At the middle of each hold: how much of the frame is her (the same moment drawn first person).
  const int per=std::max(1,s.frames/4),hold=holdAt(s);if(s.frame%per!=per/2)return;
  s.shoot(std::string("hold ")+Holds[hold].name);(void)f;
  #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
  const auto& rep=LabAccess::claireReport(*s.renderer);const std::string k=std::string("claire.")+Holds[hold].name;
  s.board[k+".drawn"]=rep.drawn;s.board[k+".layer"]=rep.layer;
  // In frame: the box from her feet to a head's height over them, projected, as a share of the frame.
  {float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;int seen=0;for(const auto& foot:rep.feet)for(float up:{0.f,1.65f}){const auto p=project(s,{foot.x,foot.y,foot.z+up});if(!p.visible)continue;++seen;x0=std::min(x0,p.x);y0=std::min(y0,p.y);x1=std::max(x1,p.x);y1=std::max(y1,p.y);}
   x0=std::max(x0,0.f);y0=std::max(y0,0.f);x1=std::min(x1,float(f.width));y1=std::min(y1,float(f.height));
   s.board[std::string("claire.cover.")+Holds[hold].name]=rep.drawn&&seen==4&&x1>x0&&y1>y0?double((x1-x0+12)*(y1-y0))/double(f.width*f.height):0;}s.board[k+".hand_gap"]=rep.handGap;s.board[k+".support_gap"]=rep.supportGap;s.board[k+".floor_gap"]=rep.floorGap;s.board[k+".aim_error"]=rep.aimError;
  #endif
  },
 .metrics=[](Scene& s,Report& r){
  for(const auto& h:Holds){const std::string k=std::string("claire.cover.")+h.name;const double c=s.board.count(k)?s.board[k]:0;r.metric(std::string("cover.")+h.name,c,"share");
   #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
   r.check(std::string("she is in the frame holding ")+h.name,c>.01,fixed(float(c*100),1)+"% of the frame is her box");
   #endif
   #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
   const std::string p=std::string("claire.")+h.name;r.metric(std::string("hand_gap.")+h.name,s.board[p+".hand_gap"],"m");r.metric(std::string("floor_gap.")+h.name,s.board[p+".floor_gap"],"m");
   r.check(std::string("her pose is drawn: ")+h.name,s.board[p+".drawn"]>0);
   // The support hand closes on the pistol's grip when she aims (the engine's third-person test allows 8 cm).
   if(h.aim){const int layer=int(s.board[p+".layer"]);const bool aiming=layer==ClaireOutside::PistolAim||layer==ClaireOutside::PistolShoot;
    r.check(std::string("she brings the pistol up to aim: ")+h.name,aiming,"upper-body layer clip "+std::to_string(layer));
    if(aiming)r.check(std::string("her support hand is on the grip: ")+h.name,s.board[p+".hand_gap"]>=0&&s.board[p+".hand_gap"]<.08,fixed(float(s.board[p+".hand_gap"]))+" m (the engine's test allows 0.08)");}
   r.check(std::string("her feet are on the floor: ")+h.name,std::fabs(s.board[p+".floor_gap"])<.06,fixed(float(s.board[p+".floor_gap"]))+" m");
   #endif
  }},
 .reporter=[](Scene& s,Overlay& o){
  const Player& p=s.g().player();const Point3 feet{p.pos.x,p.pos.y,p.z},eye{p.pos.x,p.pos.y,p.z+p.stanceEye()};
  o.cross(feet,.15f,0x40ff60);o.line(eye,{eye.x+std::cos(p.angle)*3.f,eye.y+std::sin(p.angle)*3.f,eye.z},0xff4040);o.label(eye,std::string("HOLD ")+Holds[holdAt(s)].name,0xffff60);
  #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
  const auto& rep=LabAccess::claireReport(*s.renderer);if(rep.drawn){o.point(rep.gripRight,0xff40ff,3);o.point(rep.gripLeft,0x40c0ff,3);o.point(rep.feet[0],0x40ff60,3);o.point(rep.feet[1],0x40ff60,3);
   o.label(rep.gripRight,"GRIP "+fixed(rep.handGap,3),0xff80ff);}
  #endif
  },
 .teardown=[](Scene& s){SoftwareRenderer::setThirdPerson(false);
  #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
  s.renderer->setBodyOrbit(0,0);
  #endif
  (void)s;}});
}
}
