// The sniper rifle (--each-test=sniper): owning and drawing it, a body hit and a head hit, the slow bolt, the
// five-round magazine and the scope at the eye. Run on a flat validation scene with one creature set 8 m ahead.
#include "Game.h"
#include <cmath>
#include <fstream>
namespace retro {
bool Game::testSniper(){
 std::ofstream report("sniper-test.txt");int failed=0;
 auto check=[&](bool ok,const std::string&text){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 try{
  auto g=validationScene(Enemy::Kind::Huntsman);g.update({},1.f/60);
  check(!g.hasSniper()&&!g.ownsWeapon(WeaponId::Sniper),"the rifle is not owned until it is given");
  g.grantSniper(5);
  check(g.hasSniper()&&g.ownsWeapon(WeaponId::Sniper)&&g.player().pool(WeaponId::Sniper).loaded==5&&g.player().pool(WeaponId::Sniper).total==5,"given, it is owned with five rounds in the magazine");
  g.selectWeapon(WeaponId::Sniper);for(int i=0;i<120;++i)g.update({},1.f/60);
  check(g.equippedWeapon()==WeaponId::Sniper&&g.holster()<.05f,"key 5 draws it");
  // Damage: a body hit and a head hit on the same creature, 12 m out.
  auto& e=g.m_enemies.front();e.pos={g.m_player.pos.x+8.f*std::cos(g.m_player.angle),g.m_player.pos.y+8.f*std::sin(g.m_player.angle)};e.z=g.m_player.z;e.hp=e.maxHp=5000;
  const auto& def=playerWeaponDef(WeaponId::Sniper);
  const Point3 origin{g.m_player.pos.x,g.m_player.pos.y,g.m_player.z+1.4f};
  auto fireAt=[&](float z){e.hp=5000;const Point3 target{e.pos.x,e.pos.y,z};Point3 d=target-origin;const float n=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);d=d*(1.f/n);
   const auto hit=g.traceShot(origin,d,def.hitscan.maxRange,-1,false,&def.hitscan);g.resolveShot(hit,origin,d,def.hitscan,true);return 5000.f-e.hp;};
  const float low=e.bodyBottom(),high=e.bodyTop(),height=high-low;
  const float body=fireAt(low+height*.45f),head=fireAt(low+height*.93f);
  check(body>60.f&&body<=100.f,"a body hit deals about 95 ("+std::to_string(body)+")");
  check(head>body*2.8f,"a head hit deals three times as much ("+std::to_string(head)+" against "+std::to_string(body)+")");
  check(g.critFlash()>0,"a head hit marks the cross HEAD");
  // The bolt: one shot per press, a long wait before the next, one round fewer in the magazine.
  g.m_enemies.clear();
  {InputState fire{};fire.fire=true;g.update(fire,1.f/60);g.update({},1.f/60);}
  check(g.player().pool(WeaponId::Sniper).loaded==5&&g.m_shotCooldown<=0,"pulled at the hip the trigger does nothing: the rifle fires only shouldered");
  {InputState up{};up.guard=true;for(int i=0;i<24;++i)g.update(up,1.f/60);InputState fire{};fire.guard=true;fire.fire=true;g.update(fire,1.f/60);InputState hold{};hold.guard=true;g.update(hold,1.f/60);}
  check(g.player().pool(WeaponId::Sniper).loaded==4,"shouldered, a shot uses one round of five");
  check(g.m_shotCooldown>1.f,"the bolt takes more than a second to cycle ("+std::to_string(g.m_shotCooldown)+" s left)");
  {InputState fire{};fire.fire=true;g.update(fire,1.f/60);g.update({},1.f/60);}
  check(g.player().pool(WeaponId::Sniper).loaded==4,"pulled again before the bolt is home, it does not fire");
  // The reload takes about three seconds and fills the magazine from the reserve.
  g.grantSniper(10);for(int i=0;i<150;++i)g.update({},1.f/60);
  {InputState r{};r.reload=true;g.update(r,1.f/60);}
  check(g.reloading(),"R starts a reload");
  for(int i=0;i<60*5;++i)g.update({},1.f/60);
  check(!g.reloading()&&g.player().pool(WeaponId::Sniper).loaded==5,"and the magazine is full again afterwards");
  // The scope: right mouse brings the rifle to the eye.
  {InputState aim{};aim.guard=true;for(int i=0;i<4;++i)g.update(aim,1.f/60);
   check(g.aimBlend()<.5f&&g.scopeBlend()<.01f,"the rifle is brought up first: the scope view does not close before it is at the shoulder ("+std::to_string(g.scopeBlend())+")");
   for(int i=0;i<60;++i)g.update(aim,1.f/60);}
  check(g.scopeBlend()>.95f,"aimed, the rifle is at the eye and the scope is up ("+std::to_string(g.scopeBlend())+")");
  for(int i=0;i<60;++i)g.update({},1.f/60);
  check(g.scopeBlend()<.05f,"let go, the scope comes down");
 // A rifle on the floor: walked over, it is taken loaded and drawn.
 {auto p=validationScene(Enemy::Kind::Huntsman);p.m_enemies.clear();p.update({},1.f/60);
  p.m_pickups.assign(1,Pickup{p.m_player.pos,Pickup::Kind::Sniper,true,-999});
  for(int i=0;i<30;++i)p.update({},1.f/60);
  check(p.hasSniper()&&p.player().pool(WeaponId::Sniper).loaded==5,"a rifle on the floor is taken with five rounds in it");
  for(int i=0;i<120;++i)p.update({},1.f/60);
  check(p.equippedWeapon()==WeaponId::Sniper,"and drawn at once, being new");}
 }catch(const std::exception& ex){check(false,std::string("exception: ")+ex.what());}
 report<<(failed?"FAIL":"PASS")<<'\n';return failed==0;
}
}
