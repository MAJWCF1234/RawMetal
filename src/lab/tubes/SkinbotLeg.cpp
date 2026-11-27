// The skinbot walking in the plain room: over a low slab, past a crate it steps onto, round a turn. Nothing about the
// walk is authored (world/SkinbotWalker.h); this films it drawn. The rules have their own test (--each-test=skinbot-walk).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/SkinbotWalker.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
constexpr int Frames=2400;   // 40 s at 60 a second
const bool skinbotWalk=add({
 .name="skinbot-walk",.summary="the skinbot walks over a slab and a crate and turns",
 .fixtures={"skinbot-walk"},
 .author=[](Scene&,AuthoredMapData& m){const float k=2.4f/1.474f;
  m.fixtures.push_back({skinbot::Model,{6.f,12.f},0,1.451f*k,1.903f*k,2.4f,1.5707963f,false});   // facing +x, along its way
  // The climbing course, west to east: a low wall, a slab longer than it is, two thin walls, a stair of blocks, crates.
  m.structures.push_back({7.9f,9.8f,8.2f,14.4f,0,.35f,false,3});
  m.structures.push_back({9.3f,9.8f,12.2f,14.4f,0,.28f,false,3});
  m.structures.push_back({13.1f,9.8f,13.35f,14.4f,0,.5f,false,3});
  m.structures.push_back({14.3f,9.8f,14.55f,14.4f,0,.3f,false,3});
  m.structures.push_back({15.4f,10.2f,16.2f,14.f,0,.22f,false,3});
  m.structures.push_back({16.2f,10.2f,17.f,14.f,0,.44f,false,3});
  m.structures.push_back({17.f,10.2f,17.8f,14.f,0,.22f,false,3});
  m.structures.push_back({11.f,12.5f,11.8f,13.2f,.28f,.62f,false,3});
  m.structures.push_back({18.6f,11.f,19.3f,11.7f,0,.5f,false,3});
  m.structures.push_back({11.8f,21.45f,13.8f,22.8f,0,1.3f,false,3});   // what the camera stands on, behind its own view
 },
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){Game& g=s.g();
  LabAccess::setCamera(g,{12.8f,21.55f},1.3f,-1.5707963f,-15.f);   // wide: the whole course in the frame
  if(s.frame==30)g.pointSkinbot({19.8f,12.f});
  if(const auto* w=g.skinbotWalker()){s.board["high"]=std::max(s.board["high"],double(w->z));s.board["x"]=std::max(s.board["x"],double(w->x));
   if(s.frame>200&&w->arrived()&&s.board["turned"]==0){s.board["turned"]=1;g.pointSkinbot({6.f,12.4f});}
   s.board["slips"]=w->curse.slips;s.board["hesitations"]=w->curse.hesitations;s.board["freezes"]=w->curse.freezes;}},
 .capture=[](Scene& s,const Frame&){if(s.frame>=30&&s.frame<Frames&&s.frame%3==0){char l[16];std::snprintf(l,sizeof l,"v%04d",s.frame);s.shoot(l);}},
 .metrics=[](Scene& s,Report& r){
  r.check("it walks across the course",s.board["x"]>18.5,std::to_string(s.board["x"]));
  r.check("it hesitates, slips and freezes on the way",s.board["hesitations"]>0&&s.board["slips"]>0&&s.board["freezes"]>0,
          std::to_string(int(s.board["hesitations"]))+" / "+std::to_string(int(s.board["slips"]))+" / "+std::to_string(int(s.board["freezes"])));
  r.check("its body rides up over the slab",s.board["high"]>.2,std::to_string(s.board["high"]));}
});
}
}
