#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include <algorithm>
namespace retro::lab { namespace {
constexpr WeaponId weapons[]={WeaponId::Fists,WeaponId::Handgun,WeaponId::Shotgun,WeaponId::MachinePistol,WeaponId::Sniper};
constexpr const char* names[]={"fists","pistol","shotgun","mac","rifle"};
const bool kitViews=add({
 .name="kit-viewmodels",.summary="Kit's own hands and sleeves across all four first-person rigs",
 .fixtures={"limitator-turn"},
 .setup=[](Scene& s){LabAccess::settings(s.g()).character=3;LabAccess::playerCamera(s.g());LabAccess::setGod(s.g(),true);SoftwareRenderer::setThirdPerson(false);},
 .step=[](Scene& s,float){const int per=std::max(1,s.frames/5),v=std::min(4,s.frame/per),at=s.frame-v*per;
  if(at==0)LabAccess::equipKitTest(s.g(),weapons[v]);
  LabAccess::player(s.g()).pitch=0;s.input.guard=at>per/2;},
 .capture=[](Scene& s,const Frame&){const int per=std::max(1,s.frames/5),v=std::min(4,s.frame/per),at=s.frame-v*per;
  if(at!=per/2-1&&at!=per-2)return;
  auto state=LabAccess::kitViewState(*s.renderer);
  bool own=state[0]&&state[1]==3417&&state[2]==3418&&state[3]==3419&&state[4]==3420;
  s.board[std::string(names[v])+".own"]=own;
  s.board[std::string(names[v])+".shared"]=LabAccess::kitSharedActions(*s.renderer)==std::array<int,4>{111,2313,3359,3393};
  if(v>=2)s.board[std::string(names[v])+".clip"]=state[5];
  s.shoot(std::string(names[v])+(at<per/2?" idle":" aim"));},
 .metrics=[](Scene& s,Report& r){for(int i=0;i<5;++i){r.check(std::string(names[i])+" uses Kit meshes",s.board[std::string(names[i])+".own"]==1);
  r.check(std::string(names[i])+" shares existing rig animations",s.board[std::string(names[i])+".shared"]==1);
  if(i>=2)r.check(std::string(names[i])+" plays its authored clip",s.board[std::string(names[i])+".clip"]==1);}},
 .teardown=[](Scene& s){LabAccess::settings(s.g()).character=0;SoftwareRenderer::setThirdPerson(false);}
});
}}
