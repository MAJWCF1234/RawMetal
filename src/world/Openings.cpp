// Openings: walls the played map cuts and fills (World.h Glazing). A GLAZING record is a window
// wall or a storefront: it opens the wall it stands in (the wall tiles and wall boxes there) from
// sill to head, builds the wall back below the sill and above the head where it cut whole tiles,
// and stands a frame of mullions and transoms in the middle of the wall's thickness, with a pane
// in every gap. Glass panes are ordinary breakable panes (GlassFracture.h, GlassPanes.cpp): each
// breaks on its own, a piece at a time, and saves its hits like any other. Mesh panes are
// chain-link (ChainLinkMaterial): they stop a body and nothing else. The authored map keeps only
// the record, so deleting it gives the wall back; playedMap applies it to the copy the world plays.
#include "World.h"
#include "ModelIds.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace retro {
namespace {
float numberOf(const std::string& text,const char* what){
 size_t used=0;float v=0;
 try{v=std::stof(text,&used);}catch(...){used=0;}
 if(used!=text.size()||!std::isfinite(v))throw std::runtime_error(std::string("invalid ")+what+": "+text);
 return v;}
int integerOf(const std::string& text,const char* what){
 const float v=numberOf(text,what);if(v!=std::floor(v))throw std::runtime_error(std::string("invalid ")+what+": "+text);return int(v);}
bool flagOf(const std::string& text,const char* what){
 if(text=="1"||text=="yes"||text=="true")return true;if(text=="0"||text=="no"||text=="false")return false;
 throw std::runtime_error(std::string("invalid ")+what+": "+text);}
// The frame's look: a word or a STRUCT material number.
int lookOf(const std::string& text){
 if(text=="aluminium"||text=="aluminum")return 20;
 if(text=="metal"||text=="painted"||text=="painted-metal")return 12;
 if(text=="steel")return 2;
 if(text=="dark-steel"||text=="iron")return 8;
 if(text=="wood"||text=="timber")return 51;
 const int v=integerOf(text,"glazing look");if(v<0||v>60||v==1||v==6||v==9||v==CollisionProxyMaterial)throw std::runtime_error("invalid glazing look: "+text);
 return v;}
bool paneLetter(char c){return std::string_view("cdtfwmex").find(c)!=std::string_view::npos;}

struct Box {float x1,y1,x2,y2,z1,z2;};
bool overlaps(const Structure& s,const Box& h){
 return s.x1<h.x2-1e-4f&&s.x2>h.x1+1e-4f&&s.y1<h.y2-1e-4f&&s.y2>h.y1+1e-4f&&s.bottom<h.z2-1e-4f&&s.top>h.z1+1e-4f;}
// s less h: the up to six boxes of s outside h.
void subtract(const Structure& s,const Box& h,std::vector<Structure>& out){
 if(!overlaps(s,h)){out.push_back(s);return;}
 auto keep=[&](Structure p){if(p.x2-p.x1>.004f&&p.y2-p.y1>.004f&&p.top-p.bottom>.004f)out.push_back(p);};
 Structure rest=s;
 if(rest.x1<h.x1){auto p=rest;p.x2=h.x1;keep(p);rest.x1=h.x1;}
 if(rest.x2>h.x2){auto p=rest;p.x1=h.x2;keep(p);rest.x2=h.x2;}
 if(rest.y1<h.y1){auto p=rest;p.y2=h.y1;keep(p);rest.y1=h.y1;}
 if(rest.y2>h.y2){auto p=rest;p.y1=h.y2;keep(p);rest.y2=h.y2;}
 if(rest.bottom<h.z1){auto p=rest;p.top=h.z1;keep(p);}
 if(rest.top>h.z2){auto p=rest;p.bottom=h.z2;keep(p);}}
std::vector<Structure> carved(std::vector<Structure> pieces,const std::vector<Box>& holes){
 for(const auto& h:holes){std::vector<Structure> next;for(const auto& p:pieces)subtract(p,h,next);pieces.swap(next);}
 return pieces;}
// A wall a glazing may cut: a solid box at least a metre tall that is not glass, mesh or a proxy.
bool wallBox(const Structure& s){
 return s.solid&&!s.rail&&s.shape==ShapeBox&&!seeThroughMaterial(s.material)&&s.material!=CollisionProxyMaterial&&s.material!=1&&s.material!=6&&s.top-s.bottom>=1.f;}

// Cuts `holes` out of the wall whose box is x1..x2 by y1..y2 from `bottom`: its # tiles on that floor
// open, what of each cut tile the holes do not take is built back as `wall` up to `top` (infill), and
// wall boxes through the footprint lose the holes and keep their own finish round them.
void cutWall(AuthoredMapData& m,float x1,float y1,float x2,float y2,float bottom,float top,const std::vector<Box>& holes,bool infill,int wall){
 int layer=0;for(int i=0;i<int(m.layers.size());++i)if(m.layers[size_t(i)].elevation<=bottom+.05f&&(layer==0||m.layers[size_t(i)].elevation>m.layers[size_t(layer)].elevation))layer=i;
 float highest=top;for(const auto& h:holes)highest=std::max(highest,h.z2);
 std::vector<Structure> fill;
 if(!m.layers.empty()){auto& rows=m.layers[size_t(layer)].rows;const float floor=m.layers[size_t(layer)].elevation;
  for(int y=std::max(0,int(std::floor(y1+1e-3f)));y<=std::min(23,int(std::floor(y2-1e-3f)));++y)
   for(int x=std::max(0,int(std::floor(x1+1e-3f)));x<=std::min(23,int(std::floor(x2-1e-3f)));++x){
    if(size_t(x)>=rows[size_t(y)].size()||rows[size_t(y)][size_t(x)]!='#')continue;
    rows[size_t(y)][size_t(x)]=layer==0?'.':'=';
    if(infill){Structure cell{float(x),float(y),float(x+1),float(y+1),floor,highest,false,wall,true};fill.push_back(cell);}}}
 const size_t count=m.structures.size();std::vector<Structure> kept;kept.reserve(count+8);
 for(size_t i=0;i<count;++i){const auto& s=m.structures[i];
  if(!wallBox(s)||std::none_of(holes.begin(),holes.end(),[&](const Box& h){return overlaps(s,h);})){kept.push_back(s);continue;}
  for(const auto& p:carved({s},holes))kept.push_back(p);}
 m.structures.swap(kept);
 for(const auto& p:carved(fill,holes))m.structures.push_back(p);}

void applyGlazing(AuthoredMapData& m,const Glazing& g){
 const bool alongX=g.x2-g.x1>=g.y2-g.y1;
 const float a0=alongX?g.x1:g.y1,length=alongX?g.x2-g.x1:g.y2-g.y1,thick=alongX?g.y2-g.y1:g.x2-g.x1,centre=alongX?(g.y1+g.y2)*.5f:(g.x1+g.x2)*.5f;
 const int bays=g.columns>0?g.columns:std::max(1,int(std::lround(length/std::max(.3f,g.spacing))));
 const int door=g.door<bays?g.door:-1;
 const float fr=std::min(g.frame,length/float(bays)*.3f),zb=g.bottom,zs=zb+g.sill,zh=zb+g.head,half=std::min(g.depth,thick)*.5f;
 auto at=[&](int i){return a0+length*float(i)/float(bays);};
 // A box along the wall from a to b, `across` either side of its middle, z1 to z2.
 auto box=[&](float a,float b,float across,float z1,float z2){return alongX?Box{a,centre-across,b,centre+across,z1,z2}:Box{centre-across,a,centre+across,b,z1,z2};};
 auto structure=[&](const Box& b,int material){Structure s{b.x1,b.y1,b.x2,b.y2,b.z1,b.z2,false,material,true};return s;};
 // ---- The cut ----
 if(g.cut){
  std::vector<Box> holes;
  const float wallHalf=thick*.5f+1e-3f;
  if(door<0||bays>1)holes.push_back(box(a0,a0+length,wallHalf,zs-fr,zh+fr));
  if(door>=0)holes.push_back(box(at(door),at(door+1),wallHalf,zb-.01f,zh+fr));
  cutWall(m,g.x1,g.y1,g.x2,g.y2,zb,std::max(g.top,zh+fr),holes,g.infill,g.wall);
 }
 // ---- The frame ----
 auto member=[&](float a,float b,float z1,float z2){if(b-a>.004f&&z2-z1>.004f)m.structures.push_back(structure(box(a,b,half,z1,z2),g.look));};
 auto mullion=[&](int i){const float lo=std::clamp(at(i)-fr*.5f,a0,a0+length-fr);return std::pair{lo,lo+fr};};
 const bool transom=door>=0&&g.head-g.doorHead>.3f;
 for(int i=0;i<=bays;++i){const bool byDoor=door>=0&&(i==door||i==door+1);const auto [lo,hi]=mullion(i);member(lo,hi,byDoor?zb:zs-fr,zh+fr);}
 member(a0,a0+length,zh,zh+fr);
 for(int i=0;i<bays;++i){const float lo=mullion(i).second,hi=mullion(i+1).first;
  if(i==door){if(transom)member(lo,hi,zb+g.doorHead,zb+g.doorHead+fr);continue;}
  member(lo,hi,zs-fr,zs);
  for(int r=1;r<g.rows;++r){const float z=zs+(zh-zs)*float(r)/float(g.rows);member(lo,hi,z-fr*.5f,z+fr*.5f);}}
 // ---- The panes ----
 int index=0;
 auto pane=[&](float a,float b,float z1,float z2){
  const char kind=index<int(g.panes.size())?g.panes[size_t(index)]:g.pane;++index;
  if(kind=='e'||b-a<.02f||z2-z1<.02f)return;
  if(kind=='m'){m.structures.push_back(structure(box(a,b,.008f,z1,z2),ChainLinkMaterial));return;}
  const Box p=box(a,b,.012f,z1,z2);
  Breakable v{p.x1,p.y1,p.x2,p.y2,p.z1,p.z2,17,g.health,kind=='w'};
  v.glaze=kind=='d'?Glaze::Dirty:kind=='t'?Glaze::Tinted:kind=='f'?Glaze::Frosted:kind=='w'?Glaze::Wired:Glaze::Clear;
  if(kind=='x')v.cracks=3;
  // A pane too near the map's edge for a breakable stays as unbreakable glass.
  if(v.x1>=.3f&&v.y1>=.3f&&v.x2<=23.7f&&v.y2<=23.7f)m.breakables.push_back(v);
  else m.structures.push_back(structure(p,17));};
 for(int i=0;i<bays;++i){const float lo=mullion(i).second,hi=mullion(i+1).first;
  if(i==door){if(transom)pane(lo,hi,zb+g.doorHead+fr,zh);continue;}
  for(int r=0;r<g.rows;++r){const float z1=zs+(zh-zs)*float(r)/float(g.rows),z2=zs+(zh-zs)*float(r+1)/float(g.rows);
   pane(lo,hi,r?z1+fr*.5f:z1,r+1<g.rows?z2-fr*.5f:z2);}}
}
}

// A hole broken through a wall (BREACH): its outline as boxes (one for a square hole, a rectangle and
// slices of a half circle for an arch, ragged strips with a lip and teeth for a broken one), the wall cut
// to it, chips of its core standing proud of the hole's edge on one face or the other, rebar where it
// tore, and rubble from it on the floor either side. The core shows in the reveal: concrete or brick.
void applyBreach(AuthoredMapData& m,const Breach& b){
 const bool alongX=b.x2-b.x1>=b.y2-b.y1;
 const float a0=alongX?b.x1:b.y1,a1=alongX?b.x2:b.y2,width=a1-a0,thick=alongX?b.y2-b.y1:b.x2-b.x1,centre=alongX?(b.y1+b.y2)*.5f:(b.x1+b.x2)*.5f;
 const float zs=b.bottom+b.sill,zh=b.bottom+b.head;
 const std::uint32_t seed=b.seed*2654435761u+std::uint32_t(std::lround(a0*100))*97u+std::uint32_t(std::lround(centre*100));
 auto rnd=[&](std::uint32_t k,std::uint32_t j=0){return glass::hashUnit(seed,k,j);};
 auto box=[&](float a,float c,float across0,float across1,float z1,float z2){return alongX?Box{a,centre+across0,c,centre+across1,z1,z2}:Box{centre+across0,a,centre+across1,c,z1,z2};};
 const float half=thick*.5f+1e-3f;
 std::vector<Box> holes;
 // Strips of the outline: along a, from bottom z1 to top z2.
 struct Strip {float a,c,z1,z2;};std::vector<Strip> strips;
 if(b.shape==0)strips.push_back({a0,a1,zs,zh});
 else if(b.shape==1){const float r=width*.5f,spring=std::max(zs+.3f,zh-r);const int n=12;
  for(int i=0;i<n;++i){const float a=a0+width*float(i)/float(n),c=a0+width*float(i+1)/float(n),x=(a+c)*.5f-(a0+r);
   strips.push_back({a,c,zs,std::min(zh,spring+std::sqrt(std::max(0.f,r*r-x*x))*(zh-spring)/std::max(.01f,r))});}}
 else{
  // Strips of uneven width, 4 to 10 cm. The top is a dome (tall in the middle, falling away to the
  // sides as a hole knocked through falls) roughened by a wandering line and the odd notch or tooth;
  // the foot is clear in the middle and lumpy at the sides, where the wall's base was left standing.
  std::vector<float> cuts{a0};for(int k=0;cuts.back()<a1-.05f&&k<400;++k)cuts.push_back(std::min(a1,cuts.back()+.04f+.06f*rnd(1,std::uint32_t(k))));cuts.back()=a1;
  const int n=int(cuts.size())-1;float wander=0,lump=0;
  for(int i=0;i<n;++i){const float a=cuts[size_t(i)],c=cuts[size_t(i+1)],t=((a+c)*.5f-a0)/width,e=1-std::fabs(2*t-1);
   wander=.7f*wander+.3f*(rnd(2,std::uint32_t(i))-.5f)*.7f;lump=.6f*lump+.4f*rnd(3,std::uint32_t(i));
   const float dome=(zh-zs)*(1-std::sqrt(std::max(0.f,1-std::pow(1-e,1.8f))))*.85f;
   const float notch=rnd(4,std::uint32_t(i))<.12f?.06f+.12f*rnd(5,std::uint32_t(i)):rnd(4,std::uint32_t(i))>.92f?-.08f:0.f;
   const float drop=std::max(0.f,dome+wander*(.25f+.2f*(1-e))+notch);
   const float foot=b.sill>0?(.1f*lump):e>.5f?0.f:(.5f-e)*1.1f*lump;
   strips.push_back({a,c,zs+foot,std::max(zs+foot+.25f,zh-drop)});}}
 for(const auto& st:strips)holes.push_back(box(st.a,st.c,-half,half,st.z1,st.z2));
 cutWall(m,b.x1,b.y1,b.x2,b.y2,b.bottom,b.top,holes,b.infill,b.wall);
 if(b.shape==0&&b.edge==2)return;
 // ---- The broken edge: chips of core on the lip and the jambs, proud of the hole on one face ----
 const int core=b.edge==1?31:15;
 auto solidBox=[&](const Box& x,int material,bool solid){Structure s{x.x1,x.y1,x.x2,x.y2,x.z1,x.z2,false,material,solid};if(x.x2-x.x1>.003f&&x.y2-x.y1>.003f&&x.z2-x.z1>.003f)m.structures.push_back(s);};
 if(b.shape!=0)for(size_t i=0;i<strips.size();++i){const auto& st=strips[i];
  // Under the lip: a chip hanging from one face or the other, a jagged tooth.
  const float face=rnd(10,std::uint32_t(i))<.5f?-1.f:1.f,deep=thick*(.2f+.45f*rnd(11,std::uint32_t(i))),hang=.025f+.11f*rnd(12,std::uint32_t(i))*rnd(13,std::uint32_t(i));
  const float lo=face<0?-half+.002f:half-deep,hi=face<0?-half+deep:half-.002f;
  solidBox(box(st.a,st.c,lo,hi,st.z2-hang,st.z2),core,false);
  if(st.z1>zs+.01f||rnd(14,std::uint32_t(i))<.3f){const float lift=.02f+.06f*rnd(15,std::uint32_t(i));solidBox(box(st.a,st.c,-half*.6f,half*.6f,st.z1,st.z1+lift),core,false);}}
 // The jambs: chips standing out of each side.
 for(int side=0;side<2;++side)for(int k=0;k<6;++k){const float z=zs+(zh-zs)*(float(k)+rnd(20,std::uint32_t(side*8+k)))/7.f,h=.06f+.16f*rnd(21,std::uint32_t(side*8+k)),out=.02f+.07f*rnd(22,std::uint32_t(side*8+k));
  const float face=rnd(23,std::uint32_t(side*8+k))<.5f?-1.f:1.f,deep=thick*(.25f+.4f*rnd(24,std::uint32_t(side*8+k)));
  const float a=side?a1-out:a0,c=side?a1:a0+out,lo=face<0?-half+.002f:half-deep,hi=face<0?-half+deep:half-.002f;
  // Only where the hole is at that height (the arch's and the ragged hole's sides close in).
  bool open=false;for(const auto& st:strips)open|=(side?st.c>=a1-.13f:st.a<=a0+.13f)&&z>st.z1&&z+h<st.z2;
  if(open)solidBox(box(a,c,lo,hi,z,std::min(z+h,zh)),core,false);}
 // ---- Rebar where the wall tore: bars hanging from the lip and standing out of the jambs ----
 if(b.rebar&&b.edge!=1){const float bar=.012f;int n=std::max(2,int(width/.22f));
  for(int k=0;k<n;++k){const float t=(float(k)+.3f+.4f*rnd(30,std::uint32_t(k)))/float(n),a=a0+width*t;float top=zh;for(const auto& st:strips)if(a>=st.a&&a<st.c)top=st.z2;
   const float len=.1f+.35f*rnd(31,std::uint32_t(k)),across=(rnd(32,std::uint32_t(k))-.5f)*thick*.5f,lean=(rnd(33,std::uint32_t(k))-.5f)*.12f;
   solidBox(box(a,a+bar,across,across+bar,top-len,top+.02f),8,false);
   if(rnd(34,std::uint32_t(k))<.5f)solidBox(box(std::min(a,a+lean),std::max(a,a+lean)+bar,across,across+bar,top-len-bar,top-len),8,false);}
  for(int side=0;side<2;++side)for(int k=0;k<3;++k){const float z=zs+.4f+(zh-zs-.6f)*(float(k)+rnd(35,std::uint32_t(side*4+k)))/3.f,len=.08f+.22f*rnd(36,std::uint32_t(side*4+k)),across=(rnd(37,std::uint32_t(side*4+k))-.5f)*thick*.5f;
   bool open=false;for(const auto& st:strips)open|=(side?st.c>=a1-.13f:st.a<=a0+.13f)&&z>st.z1&&z<st.z2;
   if(open)solidBox(side?box(a1-len,a1+.02f,across,across+bar,z,z+bar):box(a0-.02f,a0+len,across,across+bar,z,z+bar),8,false);}}
 // ---- Rubble on the floor either side, most on the side the blow went to ----
 for(int k=0;k<b.rubble;++k){const float u=rnd(40,std::uint32_t(k)),v=rnd(41,std::uint32_t(k)),w=rnd(42,std::uint32_t(k));
  const float a=a0-.3f+(width+.6f)*u,side=(v<.65f?1.f:-1.f)*(b.direction<0?-1.f:1.f),out=half+.05f+1.1f*w*w;
  const float x=alongX?a:centre+side*out,y=alongX?centre+side*out:a;if(x<.2f||y<.2f||x>23.8f||y>23.8f)continue;
  const int pick=b.edge==1&&rnd(43,std::uint32_t(k))<.6f?model::brick_pile:rnd(43,std::uint32_t(k))<.55f?model::cave_oldcity_rubble:model::rock_small;
  const float s=pick==model::brick_pile?.12f+.12f*rnd(44,std::uint32_t(k)):pick==model::rock_small?.08f+.12f*rnd(44,std::uint32_t(k)):.22f+.3f*rnd(44,std::uint32_t(k));
  const auto& size=model::size[size_t(pick)];
  Fixture f{pick,{x,y},-.02f,size[0]*s,size[1]*s,size[2]*s,rnd(45,std::uint32_t(k))*kTwoPi,false};m.fixtures.push_back(f);}
}

bool parseGlazeWord(std::string_view word,Glaze& out){
 static constexpr const char* words[]={"clear","dirty","tinted","frosted","wired"};
 for(int i=0;i<int(Glaze::Count);++i)if(word==words[i]){out=Glaze(i);return true;}return false;}
const char* glazeWord(Glaze glaze){static constexpr const char* words[]={"clear","dirty","tinted","frosted","wired"};return int(glaze)<int(Glaze::Count)?words[int(glaze)]:"clear";}

Glazing parseGlazing(const std::vector<std::string>& f,const NamedFields& named){
 if(f.size()!=8)throw std::runtime_error("malformed GLAZING record (GLAZING|map|x1|y1|x2|y2|bottom|top)");
 Glazing g;g.x1=numberOf(f[2],"glazing x1");g.y1=numberOf(f[3],"glazing y1");g.x2=numberOf(f[4],"glazing x2");g.y2=numberOf(f[5],"glazing y2");
 g.bottom=numberOf(f[6],"glazing bottom");g.top=numberOf(f[7],"glazing top");
 if(!(g.x2>g.x1&&g.y2>g.y1&&g.x1>=0&&g.y1>=0&&g.x2<=24&&g.y2<=24))throw std::runtime_error("invalid GLAZING box");
 for(const auto& n:named){const std::string& k=n.key;const std::string& v=n.value;
  if(k=="cols")g.columns=integerOf(v,"glazing cols");
  else if(k=="spacing")g.spacing=numberOf(v,"glazing spacing");
  else if(k=="rows")g.rows=integerOf(v,"glazing rows");
  else if(k=="sill")g.sill=numberOf(v,"glazing sill");
  else if(k=="head")g.head=numberOf(v,"glazing head");
  else if(k=="frame")g.frame=numberOf(v,"glazing frame");
  else if(k=="depth")g.depth=numberOf(v,"glazing depth");
  else if(k=="look")g.look=lookOf(v);
  else if(k=="door")g.door=integerOf(v,"glazing door");
  else if(k=="door_head")g.doorHead=numberOf(v,"glazing door_head");
  else if(k=="pane"){if(v.size()!=1||!paneLetter(v[0]))throw std::runtime_error("glazing pane is one of c d t f w m e x: "+v);g.pane=v[0];}
  else if(k=="panes"){if(!std::all_of(v.begin(),v.end(),paneLetter))throw std::runtime_error("glazing panes are letters c d t f w m e x: "+v);g.panes=v;}
  else if(k=="infill")g.infill=flagOf(v,"glazing infill");
  else if(k=="cut")g.cut=flagOf(v,"glazing cut");
  else if(k=="wall"){g.wall=integerOf(v,"glazing wall");if(g.wall<0||g.wall>60)throw std::runtime_error("invalid glazing wall: "+v);}
  else if(k=="health")g.health=numberOf(v,"glazing health");}
 const float height=g.top-g.bottom;
 if(!(height>.5f))throw std::runtime_error("GLAZING top must be above its bottom");
 if(g.columns<0||g.columns>48||g.rows<1||g.rows>8||!(g.spacing>=.3f&&g.spacing<=8))throw std::runtime_error("GLAZING cols 0-48, rows 1-8, spacing 0.3-8");
 if(!(g.sill>=0&&g.head>g.sill+.2f&&g.head<=height+1e-3f))throw std::runtime_error("GLAZING sill and head: 0 <= sill < head <= top - bottom");
 if(!(g.frame>=.02f&&g.frame<=.3f&&g.depth>=.02f&&g.depth<=1.f))throw std::runtime_error("GLAZING frame 0.02-0.3, depth 0.02-1");
 if(g.door<-1||g.door>=48||!(g.doorHead>=1.5f&&g.doorHead<=g.head+1e-3f))throw std::runtime_error("GLAZING door -1 or a bay; door_head 1.5 up to head");
 if(!(g.health>=0&&g.health<=10000))throw std::runtime_error("GLAZING health 0-10000");
 return g;}

Breach parseBreach(const std::vector<std::string>& f,const NamedFields& named){
 if(f.size()!=8)throw std::runtime_error("malformed BREACH record (BREACH|map|x1|y1|x2|y2|bottom|top)");
 Breach b;b.x1=numberOf(f[2],"breach x1");b.y1=numberOf(f[3],"breach y1");b.x2=numberOf(f[4],"breach x2");b.y2=numberOf(f[5],"breach y2");
 b.bottom=numberOf(f[6],"breach bottom");b.top=numberOf(f[7],"breach top");
 if(!(b.x2>b.x1&&b.y2>b.y1&&b.x1>=0&&b.y1>=0&&b.x2<=24&&b.y2<=24))throw std::runtime_error("invalid BREACH box");
 for(const auto& n:named){const std::string& k=n.key;const std::string& v=n.value;
  if(k=="shape"){if(v=="rect"||v=="square")b.shape=0;else if(v=="arch")b.shape=1;else if(v=="ragged"||v=="broken")b.shape=2;else throw std::runtime_error("breach shape is rect, arch or ragged: "+v);}
  else if(k=="edge"){if(v=="concrete")b.edge=0;else if(v=="brick")b.edge=1;else if(v=="clean"||v=="plain")b.edge=2;else throw std::runtime_error("breach edge is concrete, brick or clean: "+v);}
  else if(k=="sill")b.sill=numberOf(v,"breach sill");
  else if(k=="head")b.head=numberOf(v,"breach head");
  else if(k=="rebar")b.rebar=flagOf(v,"breach rebar");
  else if(k=="rubble")b.rubble=integerOf(v,"breach rubble");
  else if(k=="seed")b.seed=std::uint32_t(integerOf(v,"breach seed"));
  else if(k=="toward")b.direction=integerOf(v,"breach toward")<0?-1:1;
  else if(k=="infill")b.infill=flagOf(v,"breach infill");
  else if(k=="wall"){b.wall=integerOf(v,"breach wall");if(b.wall<0||b.wall>60)throw std::runtime_error("invalid breach wall: "+v);}}
 const float height=b.top-b.bottom;
 if(!(height>.5f))throw std::runtime_error("BREACH top must be above its bottom");
 if(!(b.sill>=0&&b.head>=b.sill+.4f&&b.head<=height+1e-3f))throw std::runtime_error("BREACH sill and head: 0 <= sill, sill + 0.4 <= head <= top - bottom");
 if(b.rubble<0||b.rubble>60)throw std::runtime_error("BREACH rubble 0-60");
 return b;}

void applyOpenings(AuthoredMapData& m){
 const auto glazings=std::move(m.glazings);m.glazings.clear();
 for(const auto& g:glazings)applyGlazing(m,g);
 const auto breaches=std::move(m.breaches);m.breaches.clear();
 for(const auto& b:breaches)applyBreach(m,b);}
}
