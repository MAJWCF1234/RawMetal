// The shadows creatures (and Claire, from outside) throw on the floor under the nearest lamp:
// each face turned to the lamp projected onto the floor as a 24% black film (Scene3D). The
// user on v1.4.5: entity shadows are broken. The tube draws the room with the creatures,
// then again with them carried out of it, and counts the floor pixels the first frame has
// darker by the film's own factor (every channel x ~0.76, hue kept): the shadow, apart from
// the bodies themselves (which change hue, not only level).
//   actor-shadows   a Brute and a Huntsman beside a lamp, seen from above.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
#include <map>
namespace retro::lab {
namespace {
struct Rig {int shadow=0,body=0,frames=0;};
std::map<const Scene*,Rig> rigs;
// Pixels the film darkened: each channel of `with` is ~0.62-0.88 of `without` (a black film of
// alpha .24 over the floor, read after the display curve), on a floor bright enough to tell.
void count(const std::uint32_t* with,const std::vector<std::uint32_t>& without,int n,int& shadow,int& body){
 for(int i=0;i<n;++i){const std::uint32_t a=with[i],b=without[size_t(i)];if(a==b)continue;
  float r[3];bool lit=true;for(int c=0;c<3;++c){const float x=float((a>>(c*8))&255u),y=float((b>>(c*8))&255u);if(y<14){lit=false;break;}r[c]=x/y;}
  if(!lit){++body;continue;}
  const float lo=std::min({r[0],r[1],r[2]}),hi=std::max({r[0],r[1],r[2]});
  if(hi<.93f&&lo>.45f&&hi-lo<.12f)++shadow;else ++body;}}

const bool tube=add({
 .name="actor-shadows",.summary="creatures and people beside a lamp throw their shadow on the floor: pixels darkened by the shadow film, against the same room without them",
 .fixtures={"shadow-room","shadow-m80","shadow-person"},
 .setup=[](Scene& s){rigs[&s]=Rig{};LabAccess::setGod(s.g(),true);for(auto& e:LabAccess::enemies(s.g())){e.awareness=0;}},
 .capture=[](Scene& s,const Frame& f){if(s.frame<20||!s.renderer)return;Game& g=s.g();auto& r=rigs[&s];
  const std::vector<std::uint32_t> with(f.pixels,f.pixels+size_t(f.width)*f.height);
  auto& foes=LabAccess::enemies(g);std::vector<Vec2> was;for(auto& e:foes){was.push_back(e.pos);e.pos={1.2f,1.2f};}
  auto& people=LabAccess::friendlies(g);std::vector<Vec2> stood;for(auto& n:people){stood.push_back(n.pose.position);n.pose.position={1.2f,1.6f};}
  s.renderer->render(g);const std::vector<std::uint32_t> without(s.renderer->pixels(),s.renderer->pixels()+with.size());if(s.last())s.shoot("without");
  for(size_t i=0;i<foes.size();++i)foes[i].pos=was[i];for(size_t i=0;i<people.size();++i)people[i].pose.position=stood[i];
  int shadow=0,body=0;count(with.data(),without,int(with.size()),shadow,body);r.shadow+=shadow;r.body+=body;++r.frames;
  s.renderer->render(g);},
 .metrics=[](Scene& s,Report& rep){auto& r=rigs[&s];const double frames=std::max(1,r.frames),px=double(s.renderer?s.renderer->width()*s.renderer->height():1);
  rep.metric("shadow_px_per_frame",r.shadow/frames);rep.metric("body_px_per_frame",r.body/frames);rep.metric("shadow_share",r.shadow/frames/px);
  rep.check("the bodies throw shadows on the floor (at least a fifth as many film pixels as body pixels)",r.shadow>0&&r.shadow*5>=r.body,
   std::to_string(int(r.shadow/frames))+" shadow px, "+std::to_string(int(r.body/frames))+" body px a frame");},
 .teardown=[](Scene& s){rigs.erase(&s);}});
}
}
