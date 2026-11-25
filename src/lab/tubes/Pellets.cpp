// The shotgun is eight pellets (Game::pelletDirection): fired at a wall five metres off, it leaves eight marks inside
// a cone, not one, and the same seed always lays out the same eight. Picture: the wall after one shot.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
namespace retro::lab {
namespace {
const bool pellets=add({
 .name="pellets",.summary="the shotgun fires eight seeded pellets in a cone (Game::pelletDirection)",
 .fixtures={"pellets"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Shotgun);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.pos={8.f,6.f};p.angle=-1.5707963f;p.pitch=0;},
 .step=[](Scene& s,float){auto& p=LabAccess::player(s.g());p.pos={8.f,6.f};p.angle=-1.5707963f;p.pitch=0;s.input.fire=s.frame==3;},
 .capture=[](Scene& s,const Frame&){
  if(s.frame==200){int marks=0;for(const auto& m:s.g().bulletImpacts())if(!m.blood&&m.tint==0)++marks;s.board["pellets.marks"]=marks;s.shoot("one shotgun shot, five metres off: the pellets' marks");}},
 .metrics=[](Scene& s,Report& r){
  r.metric("marks",s.board["pellets.marks"]);
  r.check("one shot leaves several marks, not one",s.board["pellets.marks"]>=6,std::to_string(int(s.board["pellets.marks"]))+" marks");
  const Point3 aim{0,-1,0};bool same=true,spread=true;float widest=0;
  for(int p=0;p<Game::ShotgunPellets;++p){const Point3 a=Game::pelletDirection(aim,12345u,p),b=Game::pelletDirection(aim,12345u,p),c=Game::pelletDirection(aim,54321u,p);
   same&=a.x==b.x&&a.y==b.y&&a.z==b.z;widest=std::max(widest,std::fabs(a.x));spread&=std::fabs(a.x)<.06f&&std::fabs(a.z)<.06f;(void)c;}
  r.check("the same seed lays out the same eight",same);
  r.check("every pellet is inside the cone, and they are not all one ray",spread&&widest>.01f,"widest "+std::to_string(widest));}
});
}
}
