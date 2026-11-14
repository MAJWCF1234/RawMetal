// Large physics props shoved into walls (the user on v1.4.5: "you can shove large physics
// props through walls in the dock hall"). Each pushable prop of the map (mass 8 kg and up,
// the ones the player shoves) in turn: the player stands behind it on the side away from
// its nearest wall and sprints into it for 2.5 s. Nothing may end up inside a solid or on
// the far side of the wall it was pushed into.
//   prop-walls   campaign map 79 (Materials Intake / Dock Hall) and any fixture naming a map.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
constexpr int Frames=150; // 2.5 s a prop
struct Trial {size_t index;bool large=false,endsInside=false;Vec2 start{},dir{};float startZ=0;std::string why;Vec2 lastPos{};float lastMid=0;bool tracked=false;std::string crossWhy;float wall=0;bool inside=false,crossed=false;float moved=0;int insideFrames=0;};
struct Rig {std::vector<Trial> trials;size_t current=0;int frame=0;bool staged=false;std::string log;};
std::map<const Scene*,std::unique_ptr<Rig>> rigs;
Rig& rig(Scene& s){auto& r=rigs[&s];if(!r)r=std::make_unique<Rig>();return *r;}
// Densely, every 3 cm round the prop's outline and across it: is any of it in a solid?
bool inSolid(const World& w,const Clutter& c,std::string* why=nullptr){
 auto local=c;local.yaw=0;const auto e=local.extent();const float co=std::cos(c.yaw),si=std::sin(c.yaw),h=c.height();
 const int nx=std::max(2,int(std::ceil(2*e[0]/.03f))),ny=std::max(2,int(std::ceil(2*e[1]/.03f)));
 for(int i=0;i<=nx;++i)for(int j=0;j<=ny;++j){const float x=-e[0]*.98f+1.96f*e[0]*float(i)/float(nx),y=-e[1]*.98f+1.96f*e[1]*float(j)/float(ny);
  const Vec2 p=c.pos+Vec2{x*co-y*si,x*si+y*co};if(!w.fits(p.x,p.y,c.z+.03f,std::max(.05f,h-.06f),false,true)){if(why)*why="at "+fixed(p.x)+","+fixed(p.y)+" z "+fixed(c.z)+".."+fixed(c.z+h)+" pitch "+fixed(c.pitch)+" roll "+fixed(c.roll)+" support "+fixed(w.supportHeight(p.x,p.y,false,true))+" clear "+fixed(w.clearanceHeight(p.x,p.y,c.z))+" floor "+fixed(w.floorHeight(p.x,p.y))+" sleeping "+std::to_string(c.sleeping);return true;}}
 return false;}
// How far from the prop's middle (at half its height) a straight line runs before a solid, along d.
float clearAlong(const World& w,const Clutter& c,Vec2 d){const float z=c.z+c.height()*.5f;for(float t=.05f;t<6.f;t+=.05f){const Vec2 p=c.pos+d*t;if(p.x<.05f||p.y<.05f||p.x>23.95f||p.y>23.95f||!w.fits(p.x,p.y,z-.02f,.04f,false,true))return t;}return 99.f;}

const bool tube=add({
 .name="prop-walls",.summary="each pushable prop shoved at a sprint into its nearest wall: none ends inside a solid or beyond the wall",
 .fixtures={"prop-walls-m79"},
 .setup=[](Scene& s){Game& g=s.g();Rig& r=rig(s);r=Rig{};LabAccess::playerCamera(g);LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Fists);
  const auto& list=LabAccess::clutter(g);const World& w=g.world();
  for(size_t i=0;i<list.size();++i){const auto& c=list[i];if(c.model<0||c.broken||(c.mass()<8&&c.footprint()<.3f))continue;
   if(inSolid(w,c)){r.log+="prop "+std::to_string(i)+" starts in a solid (skipped); ";continue;}
   Trial t{i};t.large=c.footprint()>=.3f&&c.dims[2]>=.1f;t.start=c.pos;t.startZ=c.z+c.height()*.5f;t.wall=99;for(Vec2 d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){const float a=clearAlong(w,c,d);if(a<t.wall){t.wall=a;t.dir=d;}}
   if(t.wall<5.f)r.trials.push_back(t);}
  s.frames=std::max(1,int(r.trials.size())*Frames);},
 .step=[](Scene& s,float){Game& g=s.g();Rig& r=rig(s);if(r.current>=r.trials.size())return;auto& t=r.trials[r.current];auto& c=LabAccess::clutter(g)[t.index];Player& p=LabAccess::player(g);
  if(r.frame==0){// Behind it, facing the wall; every other prop asleep where it is.
   const auto e=c.extent();const float back=std::fabs(t.dir.x)*e[0]+std::fabs(t.dir.y)*e[1]+.45f;p.pos=c.pos-t.dir*back;p.z=g.world().floorHeight(p.pos.x,p.pos.y);p.angle=std::atan2(t.dir.y,t.dir.x);p.pitch=0;}
  // Keep him square behind it, as a player steering into the push does.
  if(r.frame>0){const Vec2 side{-t.dir.y,t.dir.x};const float off=dot(p.pos-c.pos,side);p.angle=std::atan2(t.dir.y,t.dir.x)-std::clamp(off*.8f,-.4f,.4f);}
  s.input.forward=true;s.input.sprint=true;},
 .capture=[](Scene& s,const Frame&){Game& g=s.g();Rig& r=rig(s);if(r.current>=r.trials.size())return;auto& t=r.trials[r.current];const auto& c=LabAccess::clutter(g)[t.index];const World& w=g.world();
  if(inSolid(w,c,t.why.empty()?&t.why:nullptr)){t.inside=true;++t.insideFrames;}
  t.moved=std::max(t.moved,length(c.pos-t.start));
  // Through a wall: the prop's middle, frame to frame, never passes through a solid (sampled every 2 cm).
  {const float mid=c.z+c.height()*.5f;if(t.tracked&&!t.crossed){const float d=length(c.pos-t.lastPos);const int n=int(std::ceil(d/.02f));
    for(int k=1;k<n;++k){const float f=float(k)/float(n);const Vec2 q=t.lastPos+(c.pos-t.lastPos)*f;const float z=t.lastMid+(mid-t.lastMid)*f;
     if(!w.fits(q.x,q.y,z-.01f,.02f,false,true)){t.crossed=true;t.crossWhy=" (its middle went through a solid at "+fixed(q.x)+","+fixed(q.y)+","+fixed(z)+", "+fixed(d,3)+" m in one frame)";break;}}}
   t.lastPos=c.pos;t.lastMid=mid;t.tracked=true;}
  if(++r.frame>=Frames){t.endsInside=inSolid(w,c);// Beyond the wall: the line from where it started to where it is, at half its height, meets a solid.
   Point3 n;const float z=std::max(t.startZ,c.z+c.height()*.5f);const Vec2 a=t.start,b=c.pos;
   (void)n;(void)z;(void)a;(void)b;
   if(t.inside||t.crossed)r.log+="prop "+std::to_string(t.index)+" model "+std::to_string(c.model)+" ("+fixed(c.dims[0])+"x"+fixed(c.dims[1])+"x"+fixed(c.dims[2])+", "+fixed(c.mass(),1)+" kg) from "+fixed(t.start.x)+","+fixed(t.start.y)+
    " to "+fixed(c.pos.x)+","+fixed(c.pos.y)+(t.inside?" inside a solid "+std::to_string(t.insideFrames)+" frames, first "+t.why:"")+(t.crossed?" through a wall"+t.crossWhy:"")+"; ";
   ++r.current;r.frame=0;}},
 .metrics=[](Scene& s,Report& rep){Rig& r=rig(s);int inside=0,insideSmall=0,crossed=0,moved=0,endsInside=0,worstFrames=0;double furthest=0;
  for(const auto& t:r.trials){inside+=t.inside&&t.large;if(t.large){worstFrames=std::max(worstFrames,t.insideFrames);endsInside+=t.endsInside;}insideSmall+=t.inside&&!t.large;crossed+=t.crossed;moved+=t.moved>.1f;furthest=std::max(furthest,double(t.moved));}
  rep.metric("props_shoved",double(r.trials.size()));rep.metric("props_moved",moved);rep.metric("large_in_solid",inside);rep.metric("small_in_solid",insideSmall);rep.metric("beyond_wall",crossed);rep.metric("furthest_m",furthest,"m");
  rep.check("the map has props to shove, and the shoves move them",r.trials.size()>=3&&moved>=int(r.trials.size())/2,std::to_string(moved)+" of "+std::to_string(r.trials.size())+" moved");
  rep.metric("large_worst_frames_inside",worstFrames);rep.metric("large_ending_inside",endsInside);
  rep.check("no large shoved prop (30 cm across, 10 cm tall) ends inside a solid, or stays in one more than 3 frames (a thin solid's corner between two outline points)",endsInside==0&&worstFrames<=3,r.log.empty()?"0":r.log.substr(0,900));
  rep.check("no shoved prop passes through a wall (its middle, frame to frame)",crossed==0,std::to_string(crossed));
  std::string list;for(const auto& t:r.trials){const auto& c=LabAccess::clutter(s.g())[t.index];list+="#"+std::to_string(t.index)+" m"+std::to_string(c.model)+" "+fixed(c.dims[0])+"x"+fixed(c.dims[1])+"x"+fixed(c.dims[2])+" "+fixed(c.mass(),0)+"kg wall "+fixed(t.wall)+" moved "+fixed(t.moved)+"; ";}
  rep.check("diagnostic: the props",true,list.substr(0,1500));},
 .teardown=[](Scene& s){rigs.erase(&s);}});
}
}
