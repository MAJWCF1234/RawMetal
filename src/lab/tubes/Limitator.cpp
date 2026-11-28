// The Limitator, the second player body (Settings::character 1), and Leon, the third (2): the film
// camera goes round him in each of his holds (fists, the shotgun held and
// aimed, the pistol aimed, crouched, prone), then first person looks 60 degrees
// down at his body under the camera standing, crouched and prone. Checks as
// Claire's: he is the body drawn, his feet are on the floor and still while he
// stands, his hands are on the gun, the gun and his head point where he looks,
// every face wears a real texture (no 4x4 swatch), and looking down nothing of
// his body covers the crosshair (within 15% of the frame's height of it).
// lab/tune.txt `limitator.character N` runs the same takes on body N (0 Claire, 1 the Limitator, 2 Leon);
// the `leon` tube is the same tube on Leon.
// Leon's materials are flat 8x8 colours, so his texture floor is 8 px; the others' is 64 (arms 16).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
struct Hold {const char* name;WeaponId weapon;bool crouch,prone,aim,fire,firstPerson;};
constexpr Hold Holds[]={
 {"fists",WeaponId::Fists,false,false,false,false,false},
 {"shotgun",WeaponId::Shotgun,false,false,false,false,false},
 {"shotgun-aimed",WeaponId::Shotgun,false,false,true,false,false},
 {"pistol",WeaponId::Handgun,false,false,false,false,false},
 {"pistol-aimed",WeaponId::Handgun,false,false,true,true,false},
 {"mac",WeaponId::MachinePistol,false,false,false,false,false},
 {"mac-aimed",WeaponId::MachinePistol,false,false,true,true,false},
 {"crouched",WeaponId::Handgun,true,false,false,false,false},
 {"prone",WeaponId::Shotgun,true,true,false,false,false},
 {"fp-stand",WeaponId::Shotgun,false,false,false,false,true},
 {"fp-crouch",WeaponId::Shotgun,true,false,false,false,true},
 {"fp-prone",WeaponId::Shotgun,true,true,false,false,true}};
constexpr int HoldCount=int(sizeof(Holds)/sizeof(Holds[0]));
// 60 degrees down, as the game counts pitch (radians times 140).
constexpr float LookDown=-1.0472f*140.f;
int holdAt(const Scene& s){return std::min(HoldCount-1,s.frame*HoldCount/std::max(1,s.frames));}
int perHold(const Scene& s){return std::max(1,s.frames/HoldCount);}
int wanted(int fallback){return std::clamp(int(tuned("limitator.character",float(fallback))+.5f),0,3);}
std::map<const Scene*,std::map<std::string,std::string>>& notes(){static std::map<const Scene*,std::map<std::string,std::string>> n;return n;}
// The body faces drawn over the screen's middle: within radius (a share of the
// frame's height) of the crosshair, sampled on a grid. Faces reaching behind
// the eye count when any corner in front lands over the middle. What covers it
// is named by the face's bone and material.
int centreCover(Scene& s,float radius,std::string& what){
 const auto& faces=LabAccess::drawnBodyFaces(*s.renderer);const int W=s.renderer->width(),H=s.renderer->height();const float cx=W*.5f,cy=H*.5f,rr=radius*H;
 const auto& body=s.renderer->playerBody(LabAccess::localCharacter(*s.renderer));const Mesh& mesh=*body.mesh;const auto& index=mesh.creatureVertexIndices();
 static const char* bones[]={"Head","Neck","Spine3","Spine2","Spine1","Hips","Shoulder_L","Shoulder_R","Arm_L","Arm_R","ForeArm_L","ForeArm_R","Hand_L","Hand_R","LegUp_L","LegUp_R","LegLow_L","LegLow_R","Foot_L","Foot_R"};
 std::vector<std::vector<float>> weights;for(const char* b:bones)weights.push_back(mesh.creatureBoneWeights({b}));
 std::map<std::string,int> named;int covering=0;
 for(const auto& face:faces){Projected q[3];int front=0;for(int i=0;i<3;++i){q[i]=project(s,face.p[size_t(i)]);front+=q[i].visible;}
  bool over=false;
  if(front==3){for(int y=-6;y<=6&&!over;++y)for(int x=-6;x<=6&&!over;++x){const float px=cx+rr*x/6.f,py=cy+rr*y/6.f;if((px-cx)*(px-cx)+(py-cy)*(py-cy)>rr*rr)continue;
    auto side=[&](const Projected& a,const Projected& b){return (b.x-a.x)*(py-a.y)-(b.y-a.y)*(px-a.x);};const float d0=side(q[0],q[1]),d1=side(q[1],q[2]),d2=side(q[2],q[0]);
    over=(d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0);}}
  else if(front>0)for(const auto& p:q)if(p.visible&&(p.x-cx)*(p.x-cx)+(p.y-cy)*(p.y-cy)<rr*rr)over=true;
  if(!over)continue;++covering;
  int best=0;float most=-1;for(size_t b=0;b<weights.size();++b){float w=0;for(int c=0;c<3;++c){const size_t corner=face.face*3+size_t(c);if(corner<index.size()&&index[corner]<weights[b].size())w+=weights[b][index[corner]];}if(w>most){most=w;best=int(b);}}
  const int part=mesh.bindTriangles()[face.face].part;const std::string material=part>=0&&size_t(part)<mesh.materialNames.size()?mesh.materialNames[size_t(part)]:"?";
  ++named[std::string(bones[best])+"/"+material];}
 what.clear();for(const auto& [n,c]:named)what+=(what.empty()?"":", ")+n+" "+std::to_string(c);
 return covering;}
Feature tube(const char* name,const char* summary,int def){return Feature{
 .name=name,.summary=summary,
 .friends={"claire"},.fixtures={"limitator-turn"},
 .setup=[def](Scene& s){LabAccess::settings(s.g()).character=wanted(def);LabAccess::playerCamera(s.g());SoftwareRenderer::setThirdPerson(true);LabAccess::setGod(s.g(),true);LabAccess::equip(s.g(),Holds[0].weapon);
  LabAccess::collectBodyFaces(*s.renderer,true);LabAccess::resetBody(*s.renderer);
  // Every texture his faces and arms wear, measured once: the smallest side.
  auto& body=s.renderer->playerBody(wanted(def));int smallest=1<<30,arms=1<<30;size_t missing=0;
  for(const auto* t:body.faceTexture){if(!t){++missing;continue;}smallest=std::min({smallest,t->width,t->height});}
  if(body.armTextures)for(const auto& t:*body.armTextures)arms=std::min({arms,t.width,t.height});
  s.board["limitator.body"]=body.character;s.board["limitator.faces"]=double(body.faceTexture.size());s.board["limitator.texture_min"]=smallest;s.board["limitator.arm_texture_min"]=arms;s.board["limitator.texture_missing"]=double(missing);
  s.board["limitator.clips"]=body.mesh?double(body.mesh->creatureClipCount()):0;},
 .step=[](Scene& s,float dt){
  Game& g=s.g();const int hold=holdAt(s);const Hold& h=Holds[hold];const int per=perHold(s),at=s.frame-hold*per;
  if(hold!=int(s.board["limitator.hold"])||s.frame==0){s.board["limitator.hold"]=hold;LabAccess::equip(g,h.weapon);SoftwareRenderer::setThirdPerson(!h.firstPerson);}
  // Stances as the player takes them: C held, Z pressed once to go prone, a jump to get up.
  auto& p=LabAccess::player(g);const bool upFromProne=!h.prone&&at<3;
  s.input.crouch=h.crouch&&!h.prone&&at>2;s.input.prone=h.prone&&at==0;s.input.sprint=upFromProne;s.input.guard=h.aim;s.input.fire=h.fire&&at==2;
  // First person looks 60 degrees down.
  p.pitch=h.firstPerson?tuned("limitator.down",LookDown):0.f;
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
  // Looking down, over the last third of the hold (the stance settled): nothing of the body over the crosshair.
  if(h.firstPerson&&at>=per*2/3){std::string what;const int n=centreCover(s,.15f,what);if(n>=s.board[k+".centre"]){s.board[k+".centre"]=n;notes()[&s][k]=what;}}
  // The stance is judged where it has settled: the transitions into a kneel or prone take up to 0.95 s.
  if(at==per-2)s.board[k+".lower"]=rep.lower;
  const bool shot=h.firstPerson?(at==per-2):(at==per/4||at==per/2||at==per*3/4);if(!shot)return;
  s.shoot(std::string(h.name)+(h.firstPerson?" down":at==per/4?" a":at==per/2?" b":" c"));(void)f;
  if(at<per-2)s.board[k+".lower"]=rep.lower;
  s.board[k+".drawn"]=h.firstPerson?double(rep.faces>0):double(rep.drawn);s.board[k+".character"]=rep.character;s.board[k+".layer"]=rep.layer;s.board[k+".faces"]=rep.faces;
  s.board[k+".floor_gap"]=h.firstPerson?double(rep.lowest-p.z):double(rep.floorGap);
  if(rep.handGap>=0)s.board[k+".hand_gap"]=rep.handGap;
  if(rep.supportGap>=0)s.board[k+".support_gap"]=std::max(s.board.count(k+".support_gap")?s.board[k+".support_gap"]:0.,double(rep.supportGap));
  if(rep.aimError>=0&&at>=per/2)s.board[k+".aim_error"]=std::max(s.board.count(k+".aim_error")?s.board[k+".aim_error"]:0.,double(rep.aimError));},
 .metrics=[def](Scene& s,Report& r){
  const int body=wanted(def);const bool him=body>0;const std::string who=him?"he":"she";const double floor=body==2?8:64,armFloor=body==2?8:16;
  r.check(body==2?"Leon loads (40 clips, his flat materials)":him?"the Limitator loads (40 clips, his textures)":"Claire is the body",s.board["limitator.body"]==body&&s.board["limitator.clips"]>=40,
   "body "+std::to_string(int(s.board["limitator.body"]))+", "+std::to_string(int(s.board["limitator.clips"]))+" clips");
  r.check("the local player is drawn as that body",s.board["limitator.drawn_as"]==body);
  r.check("every face wears a real texture (no 4x4 swatch)",s.board["limitator.texture_missing"]==0&&s.board["limitator.texture_min"]>=floor,
   "smallest "+std::to_string(int(s.board["limitator.texture_min"]))+" px over "+std::to_string(int(s.board["limitator.faces"]))+" faces");
  r.check("the viewmodel's arms wear real textures",s.board["limitator.arm_texture_min"]>=armFloor,"smallest "+std::to_string(int(s.board["limitator.arm_texture_min"]))+" px");
  for(const auto& h:Holds){const std::string k=std::string("limitator.")+h.name;auto get=[&](const char* n,double fallback=-1){return s.board.count(k+n)?s.board[k+n]:fallback;};
   r.check(who+" is drawn: "+h.name,get(".drawn",0)>0&&get(".character")==body,std::to_string(int(get(".faces",0)))+" faces");
   if(h.crouch&&!h.prone){const int c=int(get(".lower"));r.check(who+" kneels: "+std::string(h.name),c==6||(c>=7&&c<=10)||c==25,"legs clip "+std::to_string(c));}
   if(h.prone){const int c=int(get(".lower"));r.check(who+" lies prone: "+std::string(h.name),c==14||c==15||c==19,"legs clip "+std::to_string(c));}
   if(h.firstPerson&&!h.crouch){const int c=int(get(".lower"));r.check(who+" stands: "+std::string(h.name),c==0||c==24||c==26||c==39,"legs clip "+std::to_string(c));}
   if(h.firstPerson){const int n=int(get(".centre",0));r.metric(std::string("centre_faces.")+h.name,n,"faces");
    r.check(std::string("looking 60 degrees down nothing of the body covers the crosshair: ")+h.name,n==0,std::to_string(n)+" faces within 15% of it"+(n?" ("+notes()[&s][k]+")":""));}
   r.metric(std::string("legs_clip.")+h.name,get(".lower"),"clip");
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
  }
  notes().erase(&s);},
 .reporter=[](Scene& s,Overlay& o){
  const Player& p=s.g().player();const Point3 feet{p.pos.x,p.pos.y,p.z},eye{p.pos.x,p.pos.y,p.z+p.stanceEye()};
  o.cross(feet,.15f,0x40ff60);o.line(eye,{eye.x+std::cos(p.angle)*3.f,eye.y+std::sin(p.angle)*3.f,eye.z},0xff4040);o.label(eye,std::string("HOLD ")+Holds[holdAt(s)].name,0xffff60);
  const auto& rep=LabAccess::claireReport(*s.renderer);if(rep.drawn){o.point(rep.gripRight,0xff40ff,3);o.point(rep.gripLeft,0x40c0ff,3);o.point(rep.feet[0],0x40ff60,3);o.point(rep.feet[1],0x40ff60,3);}},
 .teardown=[](Scene& s){SoftwareRenderer::setThirdPerson(false);s.renderer->setBodyOrbit(0,0);LabAccess::collectBodyFaces(*s.renderer,false);LabAccess::resetBody(*s.renderer);LabAccess::settings(s.g()).character=0;}};}
const bool limitator=add(tube("limitator","the Limitator (Settings::character 1) turned through his holds from outside, then first person 60 degrees down standing, crouched and prone",1));
const bool kit=add(tube("kit","Kit: her own body and first-person sleeves and hands through the character holds",3));
const bool leon=add(tube("leon","Leon (Settings::character 2) through the same holds from outside, then first person 60 degrees down standing, crouched and prone",2));
}
}
