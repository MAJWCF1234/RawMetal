// The spine through a turn: a body (Settings::character 0 Claire, 1 the Limitator, 2 Leon; lab/tune.txt
// `spineturn.character`, or the claire/limitator/leon-turn tubes) turns on the spot, then turns while walking,
// the camera fixed over his shoulder. Every frame the drawn faces' centres go to .build/lab-out/spineturn-N.bin
// (float32: a header of the face count and the frame count, then per frame a scene tag and each face's x y z,
// NaN when not drawn) and the faces' weights on the spine's bones to spineturn-N.w, so tools/dev/spine_pop.py
// can measure how far the waist pops from one frame to the next. A few frames of the turn are kept as pictures.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
namespace retro::lab {
namespace {
constexpr int Settle=70,Spot1=70,Spot2=150,Walk0=240,WalkEnd=420;
// The player's look angle: still, then a 100 degree turn left over 25 frames (the body steps round on the spot),
// then back, then turning left at 90 degrees a second while walking.
float angleAt(int f){
 auto ramp=[](int f,int a,int len,float total){if(f<a)return 0.f;const float u=std::min(1.f,float(f-a)/float(len));return total*u*u*(3-2*u);};
 float a=ramp(f,Spot1,25,1.75f)+ramp(f,Spot2,25,-1.75f);
 if(f>=Walk0)a+=1.57f*float(std::min(f,WalkEnd)-Walk0)/60.f;
 return a;}
FILE*& out(){static FILE* f=nullptr;return f;}
const bool spineturn=add({
 .name="spineturn",.summary="a player body turning on the spot and while walking, its drawn faces dumped for tools/dev/spine_pop.py",
 .friends={},.fixtures={"spineturn"},
 .setup=[](Scene& s){const int who=std::clamp(int(tuned("spineturn.character",2.f)+.5f),0,2);LabAccess::settings(s.g()).character=who;LabAccess::playerCamera(s.g());
  SoftwareRenderer::setThirdPerson(true);LabAccess::setGod(s.g(),true);LabAccess::equip(s.g(),WeaponId::Fists);
  LabAccess::collectBodyFaces(*s.renderer,true);LabAccess::resetBody(*s.renderer);
  s.board["spineturn.base"]=LabAccess::player(s.g()).angle;
  auto& body=s.renderer->playerBody(who);const Mesh& mesh=*body.mesh;
  const std::string base=labRoot()+"/.build/lab-out/spineturn-"+std::to_string(who);
  static const char* bones[]={"Hips","Spine1","Spine2","Spine3","Neck","Head","Shoulder_L","Shoulder_R","Arm_L","Arm_R","LegUp_L","LegUp_R"};
  const auto& index=mesh.creatureVertexIndices();const size_t faces=mesh.bindTriangles().size();
  if(FILE* w=std::fopen((base+".w").c_str(),"wb")){const int n=int(faces),b=int(sizeof bones/sizeof bones[0]);std::fwrite(&n,4,1,w);std::fwrite(&b,4,1,w);
   std::vector<std::vector<float>> weights;for(const char* name:bones)weights.push_back(mesh.creatureBoneWeights({name}));
   for(size_t f=0;f<faces;++f)for(size_t k=0;k<weights.size();++k){float x=0;for(int c=0;c<3;++c){const size_t corner=f*3+size_t(c);if(corner<index.size()&&index[corner]<weights[k].size())x+=weights[k][index[corner]]/3.f;}std::fwrite(&x,4,1,w);}
   std::fclose(w);}
  out()=std::fopen((base+".bin").c_str(),"wb");if(out()){const int n=int(faces),fr=s.frames;std::fwrite(&n,4,1,out());std::fwrite(&fr,4,1,out());}
  s.board["spineturn.faces"]=double(faces);},
 .step=[](Scene& s,float dt){
  auto& p=LabAccess::player(s.g());p.angle=float(s.board["spineturn.base"])+angleAt(s.frame);
  s.input.forward=s.frame>=Walk0&&s.frame<WalkEnd;
  s.renderer->setBodyOrbit(tuned("spineturn.orbit",2.6f),tuned("spineturn.view",0.f));(void)dt;},
 .capture=[](Scene& s,const Frame&){
  if(!out())return;const int n=int(s.board["spineturn.faces"]);std::vector<float> c(size_t(n)*3,std::numeric_limits<float>::quiet_NaN());
  for(const auto& f:LabAccess::drawnBodyFaces(*s.renderer))if(f.face<size_t(n)){const Point3 m=(f.p[0]+f.p[1]+f.p[2])*(1.f/3.f);c[f.face*3]=m.x;c[f.face*3+1]=m.y;c[f.face*3+2]=m.z;}
  const float tag=float(s.frame);std::fwrite(&tag,4,1,out());std::fwrite(c.data(),4,c.size(),out());
  const int f=s.frame;if((f>=Spot1+6&&f<Spot1+60&&(f-Spot1)%6==0)||(f>=Walk0&&f<Walk0+60&&(f-Walk0)%8==0))s.shoot("f"+std::to_string(f));},
 .metrics=[](Scene&,Report&){},
 .teardown=[](Scene& s){if(out()){std::fclose(out());out()=nullptr;}SoftwareRenderer::setThirdPerson(false);s.renderer->setBodyOrbit(0,0);LabAccess::collectBodyFaces(*s.renderer,false);LabAccess::resetBody(*s.renderer);LabAccess::settings(s.g()).character=0;}});
}
}
