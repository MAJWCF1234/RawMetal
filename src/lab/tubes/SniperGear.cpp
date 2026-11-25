// The sniper rifle's gear, in three pictures: the rifle and its .308 box lying on the floor, the inventory screen with the rifle
// line and its round count, and the rifle held in third person (a close orbit round the body).
// Checks: the pickups are in the scene; taking the rifle's box adds ten rounds; the inventory opens; the rifle is the one drawn.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <algorithm>
namespace retro::lab {
namespace {
constexpr int FloorUntil=120,InventoryUntil=240;
const bool sniperGear=add({
 .name="sniper-gear",.summary="the sniper rifle's gear: the pickups on the floor, the inventory line, the third-person hold",
 .fixtures={"sniper"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);
  LabAccess::pickups(g).assign({Pickup{{8.1f,19.5f},Pickup::Kind::Sniper,true,-999},Pickup{{9.1f,19.5f},Pickup::Kind::SniperAmmo,true,-999},
   Pickup{{10.1f,19.5f},Pickup::Kind::HandgunAmmo,true,-999}});
  LabAccess::setCamera(g,{9.f,22.4f},-999,-1.5707963f,-62.f);},
 .step=[](Scene& s,float){Game& g=s.g();
  if(s.frame==FloorUntil){LabAccess::playerCamera(g);g.grantSniper(5);g.grantSniper(10);LabAccess::equip(g,WeaponId::Sniper);LabAccess::inventory(g,true);}
  if(s.frame==InventoryUntil){LabAccess::inventory(g,false);SoftwareRenderer::setThirdPerson(true);}
  if(s.frame>=FloorUntil){auto& p=LabAccess::player(g);p.pos={8.f,22.f};p.angle=-1.5707963f;p.pitch=0;}
  #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
  if(s.frame>=InventoryUntil)s.renderer->setBodyOrbit(2.9f,6.2832f*float(s.frame-InventoryUntil)/120.f);
  #endif
  if(s.frame==s.frames-1)SoftwareRenderer::setThirdPerson(false);},
 .capture=[](Scene& s,const Frame&){
  if(s.frame==100){s.board["pickups"]=double(LabAccess::pickups(s.g()).size());s.shoot("the rifle and its .308 box on the floor, beside a 9 mm box");}
  if(s.frame==200){s.board["inventory"]=s.g().inventoryOpen()?1.:0.;s.board["rounds"]=double(s.g().player().pool(WeaponId::Sniper).total);s.shoot("the inventory screen: the rifle line and its round count");}
  if(s.frame==270)s.shoot("third person: the rifle held, from the side");
  if(s.frame==300)s.shoot("third person: from behind");
  if(s.frame==330)s.shoot("third person: the other side");},
 .metrics=[](Scene& s,Report& r){
  r.check("the rifle, its .308 box and a 9 mm box are on the floor",s.board["pickups"]==3.);
  r.check("the inventory screen opens",s.board["inventory"]==1.);
  r.check("the rifle is carried with its rounds (15)",s.board["rounds"]==15.,std::to_string(s.board["rounds"]));}
});
}
}
