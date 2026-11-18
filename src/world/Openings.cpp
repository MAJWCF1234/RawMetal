// Openings: walls the played map cuts and fills (World.h Glazing). A GLAZING record is a window
// wall or a storefront: it opens the wall it stands in (the wall tiles and wall boxes there) from
// sill to head, builds the wall back below the sill and above the head where it cut whole tiles,
// and stands a frame of mullions and transoms in the middle of the wall's thickness, with a pane
// in every gap. Glass panes are ordinary breakable panes (GlassFracture.h, GlassPanes.cpp): each
// breaks on its own, a piece at a time, and saves its hits like any other. Mesh panes are
// chain-link (ChainLinkMaterial): they stop a body and nothing else. The authored map keeps only
// the record, so deleting it gives the wall back; playedMap applies it to the copy the world plays.
#include "World.h"
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
  // Wall tiles under the footprint, on the floor the glazing stands on, open; what of each the hole
  // does not take is built back as a box of the wall's finish.
  int layer=0;for(int i=0;i<int(m.layers.size());++i)if(m.layers[size_t(i)].elevation<=zb+.05f&&(layer==0||m.layers[size_t(i)].elevation>m.layers[size_t(layer)].elevation))layer=i;
  std::vector<Structure> infill;
  if(!m.layers.empty()){auto& rows=m.layers[size_t(layer)].rows;const float floor=m.layers[size_t(layer)].elevation;
   for(int y=std::max(0,int(std::floor(g.y1+1e-3f)));y<=std::min(23,int(std::floor(g.y2-1e-3f)));++y)
    for(int x=std::max(0,int(std::floor(g.x1+1e-3f)));x<=std::min(23,int(std::floor(g.x2-1e-3f)));++x){
     if(size_t(x)>=rows[size_t(y)].size()||rows[size_t(y)][size_t(x)]!='#')continue;
     rows[size_t(y)][size_t(x)]=layer==0?'.':'=';
     if(g.infill){Structure cell{float(x),float(y),float(x+1),float(y+1),floor,std::max(g.top,zh+fr),false,g.wall,true};infill.push_back(cell);}}}
  // Wall boxes through the footprint lose the hole and keep their own finish above and below it.
  const size_t count=m.structures.size();std::vector<Structure> kept;kept.reserve(count+8);
  for(size_t i=0;i<count;++i){const auto& s=m.structures[i];
   if(!wallBox(s)||std::none_of(holes.begin(),holes.end(),[&](const Box& h){return overlaps(s,h);})){kept.push_back(s);continue;}
   for(const auto& p:carved({s},holes))kept.push_back(p);}
  m.structures.swap(kept);
  for(const auto& p:carved(infill,holes))m.structures.push_back(p);
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

void applyOpenings(AuthoredMapData& m){
 const auto glazings=std::move(m.glazings);m.glazings.clear();
 for(const auto& g:glazings)applyGlazing(m,g);}
}
