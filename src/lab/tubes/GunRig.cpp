// The gun rig: the player is a mount on the firing line (y 2.5) that slides to stand square to each target, which stands 4 m off in the row at y 6.5. For every
// target the rig aims at its centre line, fires a burst, and an inspection camera then looks at the marks. A weapon is a phase: the
// shotgun first (its eight pellets spread marks wide), then the sniper rifle. Checks: a mark never hangs over a target's edge (the ring
// the hole could draw, scaled by BulletImpact::size, stays inside the width the target gives it), every shot leaves a mark or is refused
// for want of surface, and the burst's numbers are reported.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
struct Target{const char* name;float x,x0,x1;};   // where it is aimed at (x), and the left and right edges of the face it is shot on (m)
constexpr Target Targets[]={{"a thin pole",6.f,5.85f,6.15f},{"the end of a wall",9.f,7.f,9.1f},{"a plinth",12.f,11.7f,12.3f},{"a ledge's lip",15.f,14.75f,15.25f}};
constexpr float FireY=2.5f,TargetY=6.5f,MarkRadius=.14f;
constexpr int FramesPerTarget=40,Targets_n=4;
void author(AuthoredMapData& m){
 m.definition.ceiling=3.4f;m.definition.playerStart={12,FireY};
 // A thin pole, the end of a wall (a slab 2 m long, its end face is the target), a plinth, a low ledge.
 m.structures.push_back({5.85f,TargetY-.15f,6.15f,TargetY+.15f,0,3.f,false,3});
 m.structures.push_back({7.f,TargetY-.1f,9.1f,TargetY+.1f,0,2.5f,false,3});
 m.structures.push_back({11.7f,TargetY-.3f,12.3f,TargetY+.3f,0,1.2f,false,3});
 m.structures.push_back({14.75f,TargetY-.25f,15.25f,TargetY+.25f,0,1.f,false,3});
}
struct Run{int shots=0;float worst=0;int unmarked=0,marks=0,shrunk=0,refused=0;};
std::map<const Scene*,Run>& runs(){static std::map<const Scene*,Run> r;return r;}
const bool gunRig=add({
 .name="gun-rig",.summary="a fixed mount fires the shotgun and the sniper rifle at a pole, a wall's end, a plinth and a ledge: no mark may hang over an edge",
 .fixtures={"gun-rig"},
 .author=[](Scene&,AuthoredMapData& m){author(m);},
 .setup=[](Scene& s){runs()[&s]=Run{};Game& g=s.g();LabAccess::setGod(g,true);g.grantSniper(5);g.grantSniper(30);LabAccess::equip(g,WeaponId::Shotgun);
  LabAccess::player(g).pools[1]={200,6};},
 .step=[](Scene& s,float){Game& g=s.g();auto& r=runs()[&s];
  const int target=std::min(Targets_n-1,s.frame/FramesPerTarget%Targets_n);const int phase=s.frame/(FramesPerTarget*Targets_n);   // 0 shotgun, 1 sniper
  const int at=s.frame%FramesPerTarget;
  if(s.frame%FramesPerTarget==0){LabAccess::impacts(g).clear();LabAccess::equip(g,phase==0?WeaponId::Shotgun:WeaponId::Sniper);}
  const Target& t=Targets[target];
  auto& p=LabAccess::player(g);
  // The mount: fixed place, aimed at the target's middle, 1.1 m up.
  // The mount slides along the firing line to stand square to each target, 4 m off, and aims at a height of 0.8 m (under the low targets' tops).
  if(at<20){LabAccess::playerCamera(g);p.pos={t.x,FireY};p.angle=kPi*.5f;p.pitch=std::atan2(.8f-(p.z+p.eye),TargetY-FireY)*140.f;}
  if(at==2||at==6||at==10){LabAccess::own(g,phase==0?WeaponId::Shotgun:WeaponId::Sniper,6);LabAccess::fire(g);++r.shots;}
  if(at==22)LabAccess::setCamera(g,{t.x,TargetY-1.6f},-999,kPi*.5f,-8.f);},
 .capture=[](Scene& s,const Frame&){Game& g=s.g();auto& r=runs()[&s];
  const int target=std::min(Targets_n-1,s.frame/FramesPerTarget%Targets_n),phase=s.frame/(FramesPerTarget*Targets_n),at=s.frame%FramesPerTarget;
  if(at==36){const Target& t=Targets[target];
   for(const auto& i:LabAccess::impacts(g)){if(i.blood||i.tint<0)continue;++r.marks;
    const float fit=i.size>0?std::min(1.f,i.size):1.f,ring=MarkRadius*fit,overhang=std::max({0.f,t.x0-(i.pos.x-ring),(i.pos.x+ring)-t.x1});
    // a mark on the target's front face (its normal points back at the mount, its y at the face) is judged by its across-distance; one on a side, the floor or the wall behind is not
    if(i.normal.y<-.7f&&std::fabs(i.pos.y-(TargetY-(target==0?.15f:target==1?.1f:target==2?.3f:.25f)))<.05f){r.worst=std::max(r.worst,overhang);
     if(fit<1.f)++r.shrunk;
     {char k[40];std::snprintf(k,sizeof k,"off.%d.%d",phase,target);s.board[k]=std::max(s.board[k],double(overhang));std::snprintf(k,sizeof k,"dx.%d.%d",phase,target);s.board[k]=std::max(s.board[k],double(std::fabs(i.pos.x-t.x)));
      std::snprintf(k,sizeof k,"front.%d.%d",phase,target);s.board[k]+=1;}}}
   {int onFace=0;for(const auto& i:LabAccess::impacts(g))if(!i.blood&&i.tint>=0)++onFace;const int fired=3*(phase==0?Game::ShotgunPellets:1);if(onFace<fired)r.refused+=fired-onFace;}
   // Where the marks are, for the report: the first mark's place, and how many were left.
   {const auto& im=LabAccess::impacts(g);char k[40];std::snprintf(k,sizeof k,"imp.%d.%d",phase,target);s.board[k]=double(im.size());
    if(!im.empty()){std::snprintf(k,sizeof k,"imp.%d.%d.x",phase,target);s.board[k]=im.front().pos.x;std::snprintf(k,sizeof k,"imp.%d.%d.y",phase,target);s.board[k]=im.front().pos.y;}}
   // The picture is the scene alone (no gun in the way), looked at from 1.6 m.
   if(s.renderer){s.renderer->setEnvironmentInspection(true);s.renderer->render(g);}
   char l[96];std::snprintf(l,sizeof l,"%s: %s, marks %d",phase==0?"shotgun":"sniper",t.name,int(LabAccess::impacts(g).size()));s.shoot(l);
   if(s.renderer)s.renderer->setEnvironmentInspection(false);}},
 .metrics=[](Scene& s,Report& r){const Run& u=runs()[&s];
  for(int ph=0;ph<2;++ph)for(int t=0;t<Targets_n;++t){char k[40];std::snprintf(k,sizeof k,"imp.%d.%d",ph,t);const double n=s.board[k];std::snprintf(k,sizeof k,"imp.%d.%d.x",ph,t);const double x=s.board[k];std::snprintf(k,sizeof k,"imp.%d.%d.y",ph,t);const double y=s.board[k];
   r.check(std::string("info: ")+(ph?"sniper":"shotgun")+" at "+Targets[t].name,true,std::to_string(int(n))+" marks, the first at ("+std::to_string(x)+", "+std::to_string(y)+"); front-face marks "+std::to_string(int(s.board[std::string("front.")+std::to_string(ph)+"."+std::to_string(t)]))+", widest from centre "+std::to_string(s.board[std::string("dx.")+std::to_string(ph)+"."+std::to_string(t)])+" m, worst overhang "+std::to_string(s.board[std::string("off.")+std::to_string(ph)+"."+std::to_string(t)])+" m");}
  r.check("the rig fired its bursts",u.shots>=24,std::to_string(u.shots)+" rounds");
  r.check("marks were left on the targets",u.marks>0,std::to_string(u.marks));
  r.check("no mark hangs over a target's edge",u.worst<=.02f,"worst overhang "+std::to_string(u.worst)+" m");
  r.check("the rig reached the edges: some marks were made smaller or not laid",u.shrunk+u.refused>0,std::to_string(u.shrunk)+" shrunk, "+std::to_string(u.refused)+" fewer marks than rounds");}
});
}
}
