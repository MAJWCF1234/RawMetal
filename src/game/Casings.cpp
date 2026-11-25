#include "Game.h"
#include "physics/PropQueries.h"
#include <cmath>
// Spent cases (docs/Systems/weapons.md): a fired pistol round's brass, the MAC-11's,
// and the shell the shotgun's pump throws, each a real thing in the world from the
// ejection port: it flies with the gun's motion added, tumbles, bounces off floors,
// walls and props with a clink, settles, lies a while and goes. A cap on how many
// live at once (the oldest goes first); cleared with the map; never saved. The
// MAC-11 itself (its ownership, holster and spread) is here too.
namespace retro {
namespace {
float len3(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
// Each kind's size (the half-length of its axis, metres) and how lively it bounces.
struct CaseKind{float half,radius,restitution,friction;Sound clink;};
constexpr CaseKind kCaseKinds[]={{.0325f,.0105f,.32f,.55f,Sound::CasingShell},{.0095f,.0048f,.45f,.62f,Sound::CasingBrass},{.0085f,.0043f,.45f,.62f,Sound::CasingBrass},{.0256f,.006f,.4f,.6f,Sound::CasingBrass}};   // shotgun, 9 mm, .380, .308 (51 mm)
}
float Game::casingRandom(){m_casingSeed=m_casingSeed*1664525u+1013904223u;return float(m_casingSeed>>8)*(1.f/16777216.f);}
float Game::nextSpreadRandom(){m_spreadSeed^=m_spreadSeed<<13;m_spreadSeed^=m_spreadSeed>>17;m_spreadSeed^=m_spreadSeed<<5;return float(m_spreadSeed>>8)*(1.f/16777216.f);}

void Game::grantSniper(int rounds){
 auto&pool=m_player.pool(WeaponId::Sniper);rounds=std::max(0,rounds);
 if(!hasSniper()){giveQuestItem(SniperItem);pool.loaded=std::min(rounds,playerWeaponDef(WeaponId::Sniper).cycle.tubeCapacity);}
 pool.total+=rounds;
}
void Game::grantMachinePistol(int rounds){
 auto&pool=m_player.pool(WeaponId::MachinePistol);rounds=std::max(0,rounds);
 if(!hasMachinePistol()){giveQuestItem(MachinePistolItem);pool.loaded=std::min(rounds,playerWeaponDef(WeaponId::MachinePistol).cycle.tubeCapacity);}
 pool.total+=rounds;
}
int Game::hipHolster(WeaponId inHand)const{
 // The sidearm not in hand: with both carried and neither drawn, the one drawn last.
 const bool pistol=hasHandgun()&&inHand!=WeaponId::Handgun,mac=hasMachinePistol()&&inHand!=WeaponId::MachinePistol;
 if(pistol&&mac)return int(m_lastSidearm==WeaponId::MachinePistol?WeaponId::MachinePistol:WeaponId::Handgun)+1;
 return pistol?int(WeaponId::Handgun)+1:mac?int(WeaponId::MachinePistol)+1:0;
}
float Game::shotSpread()const{
 const auto&cycle=equippedWeaponDef().cycle;if(!cycle.fullAuto)return 0.f;
 // Wider on the move and in the air; down the sights it tightens.
 const float moving=1.f+std::min(length(m_velocity),5.f)*.09f+(m_player.grounded?0.f:.6f);
 return (cycle.spreadBase+m_spread)*moving*(1-(1-cycle.spreadAimed)*m_aimBlend);
}

// The view's frame: forward, right and up of the eye (desktop), or of the aim (VR).
Point3 Game::ejectionPort(WeaponId gun)const{
 // Right of the bore is up x forward here (x east, y the player's right at angle 0); up is forward x right.
 if(m_vrAimValid){const Point3 f=m_vrAimDirection;Point3 right=cross({0,0,1},f);const float l=len3(right);right=l>1e-4f?right*(1/l):Point3{1,0,0};const Point3 up=cross(f,right);
  const float back=gun==WeaponId::Shotgun?.10f:gun==WeaponId::Handgun?.02f:.04f;return m_vrAimPosition+right*.015f+up*.035f-f*back;}
 const float a=m_player.angle,p=m_player.pitch/140.f;
 const Point3 f{std::cos(a)*std::cos(p),std::sin(a)*std::cos(p),std::sin(p)},right{-std::sin(a),std::cos(a),0},up=cross(f,right),eye{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye};
 // The port's place in the view as each gun is held (right, up, ahead; metres),
 // drawn in toward the line of sight down the sights.
 Point3 at=gun==WeaponId::Shotgun?Point3{.15f,-.13f,.50f}:gun==WeaponId::Handgun?Point3{.13f,-.09f,.55f}:gun==WeaponId::Sniper?Point3{.07f,-.07f,.24f}:Point3{.11f,-.10f,.40f};
 const float aim=m_aimBlend;at.x+=(.03f-at.x)*aim;at.y+=(-.04f-at.y)*aim;
 return eye+right*at.x+up*at.y+f*at.z;
}
Point3 Game::ejectionVelocity(WeaponId gun){
 Point3 f,right,up;
 if(m_vrAimValid){f=m_vrAimDirection;right=cross({0,0,1},f);const float l=len3(right);right=l>1e-4f?right*(1/l):Point3{1,0,0};up=cross(f,right);}
 else{const float a=m_player.angle,p=m_player.pitch/140.f;f={std::cos(a)*std::cos(p),std::sin(a)*std::cos(p),std::sin(p)};right={-std::sin(a),std::cos(a),0};up=cross(f,right);}
 // Out to the right and up, a little back; the shotgun's shell lazier and heavier.
 const bool shell=gun==WeaponId::Shotgun;
 const float out=(shell?1.1f:1.3f)+casingRandom()*(shell?.5f:.6f),lift=(shell?.9f:1.f)+casingRandom()*.6f,back=-.3f+casingRandom()*.5f;
 Point3 v=right*out+up*lift+f*back;
 v.x+=m_velocity.x;v.y+=m_velocity.y;v.z+=m_player.verticalVelocity;
 return v;
}

void Game::ejectCasing(int kind,Point3 position,Point3 velocity){
 if(kind<0||kind>3||!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)||!std::isfinite(velocity.x)||!std::isfinite(velocity.y)||!std::isfinite(velocity.z))return;
 if(int(m_casings.size())>=MaxCasings)m_casings.erase(m_casings.begin());   // the oldest goes
 Casing c;c.kind=kind;c.position=position;c.velocity=velocity;
 // It leaves the port end over end: a fast roll about its own axis and a tumble.
 c.yaw=std::atan2(velocity.y,velocity.x);c.pitch=casingRandom()*kPi;c.roll=casingRandom()*2*kPi;
 c.spin={(casingRandom()*2-1)*8.f,(kind==0?14.f:24.f)+casingRandom()*12.f,(casingRandom()*2-1)*20.f};
 m_casings.push_back(c);
}

void Game::updateCasings(float dt){
 if(dt<=0)return;
 const float gravity=9.81f;
 for(auto&c:m_casings){
  c.age+=dt;
  if(c.resting){c.still+=dt;continue;}
  const auto&kind=kCaseKinds[size_t(std::clamp(c.kind,0,3))];
  // Short steps: a case is small and fast, and must not pass through a floor or a wall.
  const int steps=std::clamp(int(std::ceil(len3(c.velocity)*dt/.02f)),1,8);const float h=dt/float(steps);
  for(int s=0;s<steps&&!c.resting;++s){
   c.velocity.z-=gravity*h;
   // Air takes a little of it.
   c.velocity=c.velocity*(1.f-.15f*h);
   Point3 from=c.position,to=from+c.velocity*h;bool struck=false;Point3 normal{0,0,1};float speed=len3(c.velocity);
   // A wall, a shelf, a step or the floor itself across the step.
   {Point3 n{};const float t=m_world.shotFraction(from,to,n);if(t<1&&len3(n)>.5f){to=from+(to-from)*std::max(0.f,t-.02f);normal=n;struck=true;}}
   // A prop it lands on or flies into (a crate's lid, a table).
   if(!struck){const Point3 travel=to-from;const float reach=len3(travel);
    if(reach>1e-5f){const Point3 ray=travel*(1/reach);
     for(const auto&prop:m_clutter){if(prop.broken||prop.model<0)continue;const Vec2 d=prop.pos-Vec2{from.x,from.y};if(d.x*d.x+d.y*d.y>2.25f)continue;
      vectra::QueryHit q;if(vectra::rayCastProp(prop,from,ray,reach,q)&&!q.startedInside){to=from+ray*std::max(0.f,q.distance-.003f);normal=q.normal;struck=true;break;}}}}
   // The floor under it (the step can end below it on a slope or a stair's edge).
   const float floor=m_world.supportBelow(to.x,to.y,from.z+.05f);
   if(to.z<floor+kind.radius){to.z=floor+kind.radius;if(!struck){normal={0,0,1};struck=true;}}
   c.position=to;
   if(struck){
    const float n=len3(normal);normal=n>1e-4f?normal*(1/n):Point3{0,0,1};
    const float into=c.velocity.x*normal.x+c.velocity.y*normal.y+c.velocity.z*normal.z;
    if(into<0){
     // Bounce: the part against the surface comes back by the restitution, the rest is dragged by friction.
     Point3 against=normal*into,along=c.velocity-against;
     c.velocity=along*(1.f-kind.friction*.5f)-against*kind.restitution;
     c.spin=c.spin*.6f+Point3{(casingRandom()*2-1)*6.f,(casingRandom()*2-1)*10.f,(casingRandom()*2-1)*6.f}*std::min(1.f,-into*.4f);
     ++c.bounces;
     // The clink: louder the harder it lands; the first few bounces only, and not a wall of sound from a burst.
     if(-into>.6f&&c.bounces<=4&&m_elapsed-m_casingClink>.02f){m_casingClink=m_elapsed;
      const float gain=std::clamp(-into*.12f,.06f,.32f)*(c.bounces==1?1.f:.7f);
      m_sounds.push_back({kind.clink,{c.position.x,c.position.y},gain,.9f+casingRandom()*.25f,true,9,c.position.z,-1,0,0,-1});}
    }
    // On a floor (not a wall) and slow: it rolls to a stop and lies on its side.
    if(normal.z>.6f&&len3(c.velocity)<.35f){c.velocity={};c.resting=true;c.pitch=kPi*.5f;c.still=0;if(c.position.z<floor+kind.radius+.02f)c.position.z=floor+kind.radius;}
   }
   (void)speed;
  }
  if(!c.resting){c.yaw+=c.spin.x*dt;c.pitch+=c.spin.y*dt;c.roll+=c.spin.z*dt;}
  // Lost: out of the room or falling for ever.
  if(c.age>6.f&&!c.resting)c.resting=true,c.still=CasingLife;
 }
 std::erase_if(m_casings,[](const Casing&c){return c.resting&&c.still>=CasingLife;});
}
}
