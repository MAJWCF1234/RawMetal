#pragma once
// The PRIM kinds as one table: their names, the plain words and palette group the
// Campaign Builder shows, the default sizes (scaled to the 1.6 m player) and the
// footprint each covers on the plan. Header-only and free of the World, so the
// engine (Primitives.cpp) and the Builder's native editor read the same numbers.
//
// The table's order is the Builder's palette order: its pictures are cached as
// prim-N, so new kinds go on the end and none is ever moved.
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

namespace retro {
// The player the defaults are sized for (game/Game.h Player, Movement.cpp):
// 1.6 m standing, 0.2 m radius, a 0.215 m automatic step, a 1.04 m jump
// (a crouch-jump clears 1.1 m). Openings meant to be walked through clear
// her with 0.15 m to spare; screens leave gaps narrower than she is.
inline constexpr float PlayerHeight=1.6f,PlayerRadius=.2f,PlayerStep=.215f,PlayerHeadroom=PlayerHeight+.15f,GuardHeight=1.1f;
// Stairs: risers no taller than 0.17 m on 0.28 m treads. Ramps and gentle slopes
// collide as 0.1 m steps; a slope steeper than 1:2 collides as 0.5 m steps, too
// tall to walk up.
inline constexpr float PrimMaxRiser=.17f,PrimTread=.28f,PrimRampStep=.1f,PrimSteepStep=.5f;

namespace prim {
enum Section {WallsAndOpenings,Supports,RoofsAndSlabs,StairsAndSteps,Details,NewShapes,SectionCount};
inline constexpr const char* sectionNames[SectionCount]={"Walls & openings","Supports","Roofs & slabs","Stairs & steps","Details","New shapes"};
// How a kind's footprint reads on the plan, for picking and outlines.
enum class Outline {Box,Round,Eight,Arc,Frame,Sill,Hood,Capital};
struct KindInfo {std::string_view name;const char* label;Section section;float w,d,h;Outline outline;const char* blurb;};
inline constexpr KindInfo kinds[]={
 // The Builder's first ten, in the order its pictures were cached.
 {"box","Box",WallsAndOpenings,1,1,1,Outline::Box,"a plain box"},
 {"pillar","Pillar",Supports,.5f,.5f,3,Outline::Box,"a square pier"},
 {"stairs","Stairs",StairsAndSteps,1.2f,0,1,Outline::Box,"a flight of 0.17 m risers on 0.28 m treads, rising toward facing"},
 {"ramp","Ramp",StairsAndSteps,1.2f,0,.6f,Outline::Box,"a 1:4 slope, walked without steps"},
 {"column","Column",Supports,.6f,.6f,3,Outline::Round,"a round column"},
 {"arch","Arch",WallsAndOpenings,2.4f,.6f,3.4f,Outline::Box,"a wall with a round-headed opening; refused if a standing player could not walk through"},
 {"vault","Vault roof",RoofsAndSlabs,4,4,3,Outline::Box,"a barrel-vaulted roof, open at both ends, 2 m+ walkable under"},
 {"dome","Dome",RoofsAndSlabs,4,4,2,Outline::Round,"a half ellipsoid"},
 {"doorframe","Door frame",WallsAndOpenings,1.2f,.3f,2.2f,Outline::Frame,"jambs and lintel round a game-size door opening"},
 {"window","Window",WallsAndOpenings,3,.3f,1,Outline::Sill,"a ribbon window with sill, head, mullions and glass; set z to about 0.9 so it frames the eye line"},
 // The rest of the engine's kinds.
 {"fins","Fins",WallsAndOpenings,3,.4f,3,Outline::Box,"vertical louvres with 0.38 m gaps: see through, not walk through"},
 {"canopy","Canopy",RoofsAndSlabs,3,1.5f,.25f,Outline::Box,"a cantilevered slab with a lip; set z to 2.6 or more"},
 {"railing","Railing",Details,2,.06f,GuardHeight,Outline::Box,"a guard rail"},
 {"corbel","Corbel",Supports,.6f,.6f,.6f,Outline::Box,"stepped brackets"},
 {"plinth","Plinth",StairsAndSteps,2,2,.45f,Outline::Box,"a podium at seat height, jumped onto"},
 {"slit","Arrow slit",WallsAndOpenings,1.2f,.4f,3,Outline::Box,"a wall with a 0.25 m arrow slit"},
 {"clerestory","High slot",WallsAndOpenings,3,.4f,3,Outline::Box,"a wall with a slot above head height"},
 {"embrasure","Deep window",WallsAndOpenings,1.6f,.8f,3,Outline::Box,"a deep wall with an 0.8 m opening at eye height"},
 {"roundel","Round window",WallsAndOpenings,2,.4f,3,Outline::Box,"a wall with a round opening centred near eye height"},
 {"niche","Niche",WallsAndOpenings,1,.5f,1.6f,Outline::Box,"an alcove in a wall face"},
 {"hood","Hooded niche",WallsAndOpenings,1,.5f,1.6f,Outline::Hood,"an alcove with a projecting hood"},
 {"batter","Leaning wall",WallsAndOpenings,3,1,2.5f,Outline::Box,"a wall leaning back, too steep to climb"},
 {"waffle","Ribbed slab",RoofsAndSlabs,4,4,.6f,Outline::Box,"a coffered slab with ribs under it; z is its underside"},
 {"pilotis","Flared column",Supports,.6f,.6f,3.5f,Outline::Capital,"a column with a flared capital"},
 {"megastep","Big steps",StairsAndSteps,3,0,1.35f,Outline::Box,"0.45 m risers you jump, or sit on"},
 {"lightwell","Shaft rim",RoofsAndSlabs,4,4,GuardHeight,Outline::Box,"a parapet round a shaft"},
 {"bridge","Covered bridge",RoofsAndSlabs,3,8,3,Outline::Box,"an enclosed span you walk through, with an eye-level slot"},
 {"core","Tower",Supports,4,4,8,Outline::Box,"a stair or lift tower with a 1.2 x 2.2 m door in its face"},
 {"strut","Brace",Supports,.35f,3,3,Outline::Box,"a diagonal brace"},
 {"girder","Steel beam",Supports,.5f,4,.6f,Outline::Box,"a steel I-beam"},
 {"quadpier","Four piers",Supports,1,1,3,Outline::Box,"four slender piers you can walk between"},
 {"band","Ledge band",Details,3,.08f,.25f,Outline::Box,"a string course on a facade"},
 {"spout","Rain spout",Details,.25f,.8f,.2f,Outline::Box,"a rain scupper"},
 {"chase","Service recess",Details,.8f,.2f,3,Outline::Box,"a recessed service channel"},
 {"lattice","Screen",WallsAndOpenings,2,.2f,2.4f,Outline::Box,"a cobogo screen with 0.24 m holes"},
 // New shapes.
 {"wedge","Roof wedge",NewShapes,3,4,1.2f,Outline::Box,"a gable roof: a triangular prism, its ridge running along facing"},
 {"cone","Spire",NewShapes,2,2,4,Outline::Round,"a cone: a spire, a chimney cap or a hopper upturned"},
 {"pyramid","Pyramid roof",NewShapes,4,4,2,Outline::Box,"a four-sided hip roof meeting at a point"},
 {"spiral","Spiral stair",NewShapes,2.4f,2.4f,3,Outline::Round,"a stair winding round a post, 0.17 m risers, 2 m headroom a turn; hand=left winds the other way"},
 {"culvert","Culvert",NewShapes,2.6f,4,2.2f,Outline::Box,"a round concrete pipe sunk into the floor, open at both ends, walked through"},
 {"buttress","Buttress",NewShapes,.6f,1.5f,3,Outline::Box,"a pier propping a wall, its top sloping down away from the wall toward facing"},
 {"chamfer","Bevelled block",NewShapes,1,1,1,Outline::Eight,"a block with its edges bevelled: a ruin stone, a bollard, a plinth"},
 {"octagon","Eight-sided pillar",NewShapes,.6f,.6f,3,Outline::Eight,"an octagonal pillar"},
 {"arcwall","Curved wall",NewShapes,4,1,3,Outline::Arc,"a wall on a circular arc: w the chord, d how far it bows toward facing; thick= its thickness"},
 {"catwalk","Catwalk",NewShapes,1.2f,4,GuardHeight,Outline::Box,"a grating walkway along facing on steel stringers, guard rails both sides; z is the deck you walk on; rails=left, right or none"}};
inline constexpr int kindCount=int(std::size(kinds));

inline const KindInfo* find(std::string_view name){for(const auto& k:kinds)if(k.name==name)return &k;return nullptr;}
inline int indexOf(std::string_view name){for(int i=0;i<kindCount;++i)if(kinds[i].name==name)return i;return -1;}

// A spiral stair's risers and treads: n risers of `riser` metres; a full turn
// takes `perTurn` treads, enough that a turn rises over the player's headroom.
struct Spiral {int risers;float riser;int perTurn;float post,radius;};
inline Spiral spiralOf(float w,float d,float h){
 Spiral s{};s.risers=std::max(1,int(std::ceil(h/PrimMaxRiser-1e-4f)));s.riser=h/s.risers;
 s.perTurn=std::max(12,int(std::ceil((PlayerHeadroom+.35f)/s.riser)));
 s.radius=std::min(w,d)*.5f;s.post=std::clamp(s.radius*.09f,.06f,.12f);return s;}
// Tread i of a spiral stair spans angles [a0, a1] in its own frame, (u, v) = r (cos a, sin a):
// the first is centred on its facing side, and they wind anticlockwise on the plan's (u, v)
// (hand 1) or the other way (hand -1). Its top is riser x (i + 1) over the base.
inline std::pair<float,float> spiralTread(const Spiral& s,int i,int hand){
 const float step=6.2831853f/s.perTurn*(hand<0?-1.f:1.f),a0=1.5707963f-step*.5f+step*i;return {a0,a0+step};}
inline constexpr float SpiralTreadThickness=.06f;
// A culvert's round pipe: outer radius R (half its width), the shell `shell`
// thick, its centre `centre` above the floor it is sunk into.
struct Culvert {float radius,shell,centre;};
inline Culvert culvertOf(float w,float h,float thick){
 const float r=w*.5f,shell=std::clamp(thick>0?thick:.2f,.05f,r*.5f);return {r,shell,std::clamp(h,r*.5f,2*r)-r};}
// A curved wall: the outer face's circle (radius, centre at v = -centre from the
// footprint's middle) through the chord ends and the crown at v = +d/2.
struct ArcWall {float radius,centre,thick,half;};
inline ArcWall arcWallOf(float w,float d,float thick){
 const float c=w*.5f,s=std::clamp(d,.05f,c);const float r=(c*c+s*s)/(2*s);
 return {r,r-d*.5f,std::clamp(thick>0?thick:.3f,.05f,std::min(r*.5f,1.5f)),std::asin(std::min(1.f,c/r))};}
// The bevel on a bevelled block's edges.
inline float chamferOf(float w,float d,float h){return std::min(.1f,.18f*std::min({w,d,h}));}

// A size of 0 takes the kind's default. Stairs, big steps and ramps run as deep
// as their rise needs; a spiral stair is as deep as it is wide.
struct Size {float w=0,d=0,h=0;};
inline Size resolve(std::string_view kind,float w,float d,float h){
 const KindInfo* k=find(kind);if(!k)return {w,d,h};
 Size s{w>0?w:k->w,d,h>0?h:k->h};
 if(s.d<=0){const int risers=int(std::ceil(s.h/PrimMaxRiser-1e-4f)),megasteps=int(std::ceil(s.h/.45f-1e-4f));
  s.d=kind=="stairs"?risers*PrimTread:kind=="megastep"?megasteps*.9f:kind=="ramp"?std::max(1.f,s.h*4):kind=="spiral"?s.w:k->d;}
 return s;}

// The quarter turn: u across, v along facing (0 +y south, 1 +x east, 2 -y north, 3 -x west).
inline std::pair<float,float> forward(int facing){constexpr float f[4][2]={{0,1},{1,0},{0,-1},{-1,0}};const auto& v=f[facing&3];return {v[0],v[1]};}
inline std::pair<float,float> toPlan(float x,float y,int facing,float u,float v){
 const auto [fx,fy]=forward(facing);const float ax=fy,ay=-fx;return {x+ax*u+fx*v,y+ay*u+fy*v};}
inline const char* facingWord(int facing){constexpr const char* w[4]={"south","east","north","west"};return w[facing&3];}

// The footprint the kind covers, in its own frame (u, v), wound anticlockwise.
inline std::vector<std::pair<float,float>> outline(std::string_view kind,float w,float d,float thick=0){
 const KindInfo* k=find(kind);const Outline o=k?k->outline:Outline::Box;const float hw=w*.5f,hd=d*.5f;
 auto rect=[](float u0,float u1,float v0,float v1){return std::vector<std::pair<float,float>>{{u0,v0},{u1,v0},{u1,v1},{u0,v1}};};
 switch(o){
 case Outline::Frame:return rect(-hw-.12f,hw+.12f,-hd-.04f,hd+.04f);
 case Outline::Sill:return rect(-hw-.1f,hw+.1f,-hd,hd+.12f);
 case Outline::Hood:return rect(-hw-.1f,hw+.1f,-hd,hd+.35f);
 case Outline::Capital:{const float r=std::min(hw,hd)*1.7f;return rect(-r,r,-r,r);}
 case Outline::Round:case Outline::Eight:{
  const int n=o==Outline::Eight?8:16;std::vector<std::pair<float,float>> out;
  const float cut=o==Outline::Eight&&kind=="chamfer"?chamferOf(w,d,1):0;
  if(cut>0){for(auto [u,v]:std::initializer_list<std::pair<float,float>>{{-hw+cut,-hd},{hw-cut,-hd},{hw,-hd+cut},{hw,hd-cut},{hw-cut,hd},{-hw+cut,hd},{-hw,hd-cut},{-hw,-hd+cut}})out.emplace_back(u,v);return out;}
  for(int i=0;i<n;++i){const float a=(i+(o==Outline::Eight?.5f:0.f))*6.2831853f/n;out.emplace_back(hw*std::cos(a),hd*std::sin(a));}
  return out;}
 case Outline::Arc:{
  const auto a=arcWallOf(w,d,thick);std::vector<std::pair<float,float>> out;constexpr int n=12;
  for(int i=0;i<=n;++i){const float t=-a.half+2*a.half*i/n;out.emplace_back(a.radius*std::sin(t),-a.centre+a.radius*std::cos(t));}
  for(int i=n;i>=0;--i){const float t=-a.half+2*a.half*i/n;out.emplace_back((a.radius-a.thick)*std::sin(t),-a.centre+(a.radius-a.thick)*std::cos(t));}
  return out;}
 default:return rect(-hw,hw,-hd,hd);}
}
// Is (px, py), in the kind's own frame, inside its outline or within `slack` of it?
inline bool insideOutline(const std::vector<std::pair<float,float>>& poly,float px,float py,float slack){
 bool in=false;float nearest=1e9f;
 for(size_t i=0,j=poly.size()-1;i<poly.size();j=i++){
  const auto [ax,ay]=poly[j];const auto [bx,by]=poly[i];
  if((by>py)!=(ay>py)&&px<(ax-bx)*(py-by)/(ay-by)+bx)in=!in;
  const float ex=bx-ax,ey=by-ay,len=ex*ex+ey*ey;const float t=len>0?std::clamp(((px-ax)*ex+(py-ay)*ey)/len,0.f,1.f):0.f;
  nearest=std::min(nearest,std::hypot(ax+ex*t-px,ay+ey*t-py));}
 return in||nearest<=slack;}
}
}
