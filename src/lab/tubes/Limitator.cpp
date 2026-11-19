// The Limitator, the second player body (Settings::character 1): the film
// camera goes round him in each of his holds (fists, the shotgun held and
// aimed, the pistol aimed, crouched, prone), then first person looks ahead and
// down at his body under the camera. Checks as Claire's: he is the body drawn,
// his feet are on the floor and still while he stands, his hands are on the
// gun, the gun and his head point where he looks, and every face wears a real
// texture (no 4x4 swatch). lab/tune.txt `limitator.character 0` runs the same
// takes on Claire, for comparison.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
struct Hold {const char* name;WeaponId weapon;bool crouch,prone,aim,fire,firstPerson;};
constexpr Hold Holds[]={
 {"fists",WeaponId::Fists,false,false,false,false,false},
 {"shotgun",WeaponId::Shotgun,false,false,false,false,false},
 {"shotgun-aimed",WeaponId::Shotgun,false,false,true,false,false},
 {"pistol-aimed",WeaponId::Handgun,false,false,true,true,false},
 {"crouched",WeaponId::Handgun,true,false,false,false,false},
 {"prone",WeaponId::Shotgun,true,true,false,false,false},
 {"first-person",WeaponId::Shotgun,false,false,false,false,true}};
constexpr int HoldCount=int(sizeof(Holds)/sizeof(Holds[0]));
int holdAt(const Scene& s){return std::min(HoldCount-1,s.frame*HoldCount/std::max(1,s.frames));}
int perHold(const Scene& s){return std::max(1,s.frames/HoldCount);}
// How much of the frame is the body: the frame against the same moment with the body left out
// (environment inspection draws the scene alone), over the middle band (the HUD's left out).
double cover(Scene& s,const Image& with){auto& r=*s.renderer;r.setEnvironmentInspection(true);r.render(s.g());Image without=Image::of(r.pixels(),r.width(),r.height());r.setEnvironmentInspection(false);r.render(s.g());
 size_t moved=0,n=0;for(int y=with.height*15/100;y<with.height*80/100;++y)for(int x=with.width/5;x<with.width*3/5;++x){auto a=with.pixels[size_t(y)*with.width+x],b=without.pixels[size_t(y)*with.width+x];int m=0;for(int k=0;k<24;k+=8)m=std::max(m,std::abs(int((a>>k)&255)-int((b>>k)&255)));moved+=m>30;++n;}
 return n?double(moved)/double(n):0;}
int wanted(){return tuned("limitator.character",1.f)>.5f?1:0;}
const bool limitator=add({
 .name="limitator",.summary="the Limitator (Settings::character 1) turned through his holds from outside, then first person over his body",
 .friends={"claire"},.fixtures={"limitator-turn"},
 .setup=[](Scene& s){LabAccess::settings(s.g()).character=wanted();LabAccess::playerCamera(s.g());SoftwareRenderer::setThirdPerson(true);LabAccess::setGod(s.g(),true);LabAccess::equip(s.g(),Holds[0].weapon);
  // Every texture his faces and arms wear, measured once: the smallest side.
  auto& body=s.renderer->playerBody(wanted());int smallest=1<<30,arms=1<<30;size_t missing=0;
  for(const auto* t:body.faceTexture){if(!t){++missing;continue;}smallest=std::min({smallest,t->width,t->height});}
  if(body.armTextures)for(const auto& t:*body.armTextures)arms=std::min({arms,t.width,t.height});
  s.board["limitator.body"]=body.character;s.board["limitator.faces"]=double(body.faceTexture.size());s.board["limitator.texture_min"]=smallest;s.board["limitator.arm_texture_min"]=arms;s.board["limitator.texture_missing"]=double(missing);
  s.board["limitator.clips"]=body.mesh?double(body.mesh->creatureClipCount()):0;},
 .step=[](Scene& s,float dt){
  Game& g=s.g();const int hold=holdAt(s);const Hold& h=Holds[hold];const int per=perHold(s),at=s.frame-hold*per;
  if(hold!=int(s.board["limitator.hold"])||s.frame==0){s.board["limitator.hold"]=hold;LabAccess::equip(g,h.weapon);SoftwareRenderer::setThirdPerson(!h.firstPerson);}
  // Stances as the player takes them: C held, Z pressed once to go prone, a jump to get up.
  auto& p=LabAccess::player(g);s.input.crouch=h.crouch&&!h.prone;s.input.prone=h.prone&&at==0;s.input.jump=h.firstPerson&&at==0;s.input.guard=h.aim;s.input.fire=h.fire&&at==2;
  // First person looks ahead for the first half, then down at his body.
  p.pitch=h.firstPerson&&at>=per/2?-tuned("limitator.down",195.f):0.f;
  if(h.firstPerson)s.renderer->setBodyOrbit(0,0);
  else s.renderer->setBodyOrbit(tuned("limitator.orbit",2.8f),6.2832f*float(at)/float(per));
  (void)dt;},
 .capture=[](Scene& s,const Frame& f){
  const int hold=holdAt(s),per=perHold(s),at=s.frame-hold*per;const Hold& h=Holds[hold];const std::string k=std::string("limitator.")+h.name;
  const auto& rep=LabAccess::claireReport(*s.renderer);const Player& p=s.g().player();
  s.board["limitator.drawn_as"]=LabAccess::localCharacter(*s.renderer);
  // Standing still, his feet must not slide (the engine's test allows 1 cm a frame).
  if(!h.firstPerson&&rep.drawn){
   const std::string fk=k+".feet";
   if(at>14&&s.points.count(fk)){const auto& last=s.points[fk];float worst=0;for(int i=0;i<2;++i){const Point3 d=rep.feet[i]-last[size_t(i)];worst=std::max(worst,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));}
    s.board[k+".foot_slide"]=std::max(s.board[k+".foot_slide"],double(worst));}
   s.points[fk]={rep.feet[0],rep.feet[1]};}
  // The head's facing against the look, over the hold's second half (blends done).
  if(at>per/2&&(rep.headAhead.x!=0||rep.headAhead.y!=0)){const float look=p.angle,a=std::atan2(rep.headAhead.y,rep.headAhead.x);float d=a-look;while(d>3.14159f)d-=6.28319f;while(d<-3.14159f)d+=6.28319f;
   s.board[k+".head_off"]=std::max(s.board[k+".head_off"],double(std::fabs(d)*57.2958f));s.board[k+".head_seen"]=1;}
  const bool shot=h.firstPerson?(at==per/4||at==per-2):(at==per/4||at==per/2||at==per*3/4);if(!shot)return;
  s.shoot(std::string(h.name)+(at==per/4?" a":at==per/2?" b":at==per*3/4?" c":" d"));(void)f;
  s.board[k+".lower"]=rep.lower;
  if(h.firstPerson&&at==per-2)s.board[k+".cover"]=cover(s,Image::of(f.pixels,f.width,f.height));
  s.board[k+".drawn"]=h.firstPerson?double(rep.faces>0):double(rep.drawn);s.board[k+".character"]=rep.character;s.board[k+".layer"]=rep.layer;s.board[k+".faces"]=rep.faces;
  s.board[k+".floor_gap"]=h.firstPerson?double(rep.lowest-p.z):double(rep.floorGap);
  if(rep.handGap>=0)s.board[k+".hand_gap"]=rep.handGap;
  if(rep.supportGap>=0)s.board[k+".support_gap"]=std::max(s.board.count(k+".support_gap")?s.board[k+".support_gap"]:0.,double(rep.supportGap));
  if(rep.aimError>=0&&at>=per/2)s.board[k+".aim_error"]=std::max(s.board.count(k+".aim_error")?s.board[k+".aim_error"]:0.,double(rep.aimError));},
 .metrics=[](Scene& s,Report& r){
  const bool him=wanted()==1;const std::string who=him?"he":"she";
  r.check(him?"the Limitator loads (40 clips, his textures)":"Claire is the body",s.board["limitator.body"]==wanted()&&s.board["limitator.clips"]>=40,
   "body "+std::to_string(int(s.board["limitator.body"]))+", "+std::to_string(int(s.board["limitator.clips"]))+" clips");
  r.check("the local player is drawn as that body",s.board["limitator.drawn_as"]==wanted());
  r.check("every face wears a real texture (no 4x4 swatch)",s.board["limitator.texture_missing"]==0&&s.board["limitator.texture_min"]>=64,
   "smallest "+std::to_string(int(s.board["limitator.texture_min"]))+" px over "+std::to_string(int(s.board["limitator.faces"]))+" faces");
  r.check("the viewmodel's arms wear real textures",s.board["limitator.arm_texture_min"]>=16,"smallest "+std::to_string(int(s.board["limitator.arm_texture_min"]))+" px");
  for(const auto& h:Holds){const std::string k=std::string("limitator.")+h.name;auto get=[&](const char* n,double fallback=-1){return s.board.count(k+n)?s.board[k+n]:fallback;};
   r.check(who+" is drawn: "+h.name,get(".drawn",0)>0&&get(".character")==wanted(),std::to_string(int(get(".faces",0)))+" faces");
   if(h.crouch&&!h.prone){const int c=int(get(".lower"));r.check(who+" kneels: crouched",c==6||(c>=7&&c<=10)||c==25,"legs clip "+std::to_string(c));}
   if(h.prone){const int c=int(get(".lower"));r.check(who+" lies prone",c==14||c==15||c==19,"legs clip "+std::to_string(c));}
   if(h.firstPerson){r.metric("cover.first-person-down",get(".cover",0),"share");r.check("looking down in first person shows the body",get(".cover",0)>.03,fixed(float(get(".cover",0)*100),1)+"% of the middle of the frame");}
   r.metric(std::string("floor_gap.")+h.name,get(".floor_gap",0),"m");
   r.check(std::string("feet on the floor: ")+h.name,std::fabs(get(".floor_gap",1))<.06,fixed(float(get(".floor_gap",1)))+" m");
   if(!h.firstPerson&&!h.prone&&!h.aim&&!h.crouch){r.metric(std::string("foot_slide.")+h.name,get(".foot_slide",0),"m");
    r.check(std::string("feet planted standing: ")+h.name,get(".foot_slide",0)<.01,fixed(float(get(".foot_slide",0)))+" m a frame at most");}
   if(!h.firstPerson&&!h.prone){r.metric(std::string("head_off.")+h.name,get(".head_off",0),"deg");
    r.check(std::string("the head faces where ")+who+" looks: "+h.name,get(".head_seen",0)>0&&get(".head_off",180)<40,fixed(float(get(".head_off",180)),1)+" degrees at worst");}
   if(h.weapon==WeaponId::Shotgun&&!h.firstPerson&&!h.prone){r.metric(std::string("support_gap.")+h.name,get(".support_gap"),"m");
    r.check(std::string("left hand on the pump: ")+h.name,get(".support_gap")>=0&&get(".support_gap")<.05,fixed(float(get(".support_gap")))+" m (the engine's test allows 0.05)");}
   if(h.aim){const int layer=int(get(".layer"));
    if(h.weapon==WeaponId::Handgun){const bool aiming=layer==ClaireOutside::PistolAim||layer==ClaireOutside::PistolShoot;r.check(who+" brings the pistol up",aiming,"layer "+std::to_string(layer));
     r.check("support hand on the grip: "+std::string(h.name),get(".hand_gap")>=0&&get(".hand_gap")<.08,fixed(float(get(".hand_gap")))+" m (the engine's test allows 0.08)");}
    r.metric(std::string("aim_error.")+h.name,get(".aim_error"),"deg");
    r.check(std::string("the gun points where ")+who+" aims: "+h.name,get(".aim_error")>=0&&get(".aim_error")<5,fixed(float(get(".aim_error")),2)+" degrees (the engine's test allows 5)");}
  }},
 .reporter=[](Scene& s,Overlay& o){
  const Player& p=s.g().player();const Point3 feet{p.pos.x,p.pos.y,p.z},eye{p.pos.x,p.pos.y,p.z+p.stanceEye()};
  o.cross(feet,.15f,0x40ff60);o.line(eye,{eye.x+std::cos(p.angle)*3.f,eye.y+std::sin(p.angle)*3.f,eye.z},0xff4040);o.label(eye,std::string("HOLD ")+Holds[holdAt(s)].name,0xffff60);
  const auto& rep=LabAccess::claireReport(*s.renderer);if(rep.drawn){o.point(rep.gripRight,0xff40ff,3);o.point(rep.gripLeft,0x40c0ff,3);o.point(rep.feet[0],0x40ff60,3);o.point(rep.feet[1],0x40ff60,3);}},
 .teardown=[](Scene& s){SoftwareRenderer::setThirdPerson(false);s.renderer->setBodyOrbit(0,0);LabAccess::settings(s.g()).character=0;}});
}
}
