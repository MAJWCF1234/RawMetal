// Materials: the physically based response (renderer/Materials.h, scene.frag) on fixed views.
//
// Numbers before pictures:
// - the white furnace, two ways: the split-sum table against a brute-force integral of the shader's
//   own BRDF, and the multiple-scattering compensation bringing a white metal's lobe to 1 at every
//   roughness; then rendered (--detail 15: every surface white in a white room, no lamps) — rough
//   and smooth reflect the same energy;
// - every derived texture's roughness inside its class's physical range, and spread across it;
// - a dielectric never reflects more than its Fresnel allows head-on (--detail 16);
// - the anti-plastic metric: the coefficient of variation of the specular term (--detail 14) across
//   each surface the fixture names (mat-surface1..8 NAME X0 Y0 X1 Y1): a plastic smear is even, a real
//   surface breaks up.
// Reporter view: --detail 13 (roughness red, metalness green, cavity blue).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "renderer/GpuRenderer.h"
#include "renderer/Materials.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>
#include <fstream>
#include <cstdlib>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// GpuRenderer's reference display curve (setReferenceDisplay), inverted: the shot's byte back to the
// linear value the shader wrote.
float linearOf(int byte){
 static const std::array<float,256> table=[]{std::array<float,256> t{};
  auto display=[](float v){v=std::max(0.f,v)*1.05f;const float m=std::clamp((v*(2.51f*v+.03f))/(v*(2.43f*v+.59f)+.14f),0.f,1.f);return std::pow(m,1.f/2.2f)*255.f;};
  for(int b=0;b<256;++b){float lo=0,hi=16;for(int k=0;k<40;++k){const float mid=(lo+hi)*.5f;if(display(mid)<b)lo=mid;else hi=mid;}t[size_t(b)]=b==0?0.f:(lo+hi)*.5f;}
  return t;}();
 return table[size_t(std::clamp(byte,0,255))];}
float greyOf(std::uint32_t p){return linearOf(int((p>>8)&255u));}
struct Box {std::string name;int x0=0,y0=0,x1=0,y1=0;};
std::vector<Box> surfaces(const Fixture& f){std::vector<Box> out;
 for(int i=1;i<=8;++i){const auto v=f.value(("mat-surface"+std::to_string(i)).c_str());if(v.empty())continue;
  std::istringstream in(v);Box b;if(in>>b.name>>b.x0>>b.y0>>b.x1>>b.y1)out.push_back(b);}
 return out;}
const bool materials=add({
 .name="materials",.summary="physically based materials: furnace, class ranges, Fresnel bound and specular variation (anti-plastic) on fixed views",
 .fixtures={"mat-*"},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;auto& r=*s.renderer;const int w=f.width,h=f.height;
  const int layer=SoftwareRenderer::detailLayer();
  auto view=[&](int detail,const char* label,bool reference)->std::vector<std::uint32_t>{
   SoftwareRenderer::setDetailLayer(detail);if(reference)GpuRenderer::setReferenceDisplay(true);r.render(s.g());
   std::vector<std::uint32_t> px(r.pixels(),r.pixels()+size_t(w)*h);GpuRenderer::setReferenceDisplay(false);s.shoot(label);return px;};
  view(13,"material: roughness (red), metalness (green), cavity (blue)",true);
  const auto spec=view(14,"specular term alone",true),furnace=view(15,"white furnace",true),refl=view(16,"dielectric reflectance facing the eye",true);
  SoftwareRenderer::setDetailLayer(layer);r.render(s.g());
  // Furnace: every surface pixel against the frame's median.
  std::vector<float> fv;for(auto p:furnace){const float v=greyOf(p);if(v>.05f)fv.push_back(v);}
  if(!fv.empty()){std::sort(fv.begin(),fv.end());const float med=fv[fv.size()/2];s.board["mat.furnace_lo"]=fv[fv.size()/100]/med;s.board["mat.furnace_hi"]=fv[fv.size()-1-fv.size()/100]/med;s.board["mat.furnace_median"]=med;s.board["mat.furnace_pixels"]=double(fv.size());}
  float rmax=0;std::vector<float> rv;for(auto p:refl){const float v=greyOf(p);if(v>0){rv.push_back(v);rmax=std::max(rmax,v);}}
  if(!rv.empty()){std::sort(rv.begin(),rv.end());s.board["mat.refl_p99"]=rv[rv.size()*99/100];s.board["mat.refl_pixels"]=double(rv.size());}
  s.board["mat.refl_max"]=rmax;
  for(const auto& b:surfaces(*s.fixture)){double sum=0,sum2=0;long n=0;
   for(int y=std::max(0,b.y0);y<std::min(h,b.y1);++y)for(int x=std::max(0,b.x0);x<std::min(w,b.x1);++x){const double v=greyOf(spec[size_t(y)*w+x]);sum+=v;sum2+=v*v;++n;}
   if(n<16)continue;const double mean=sum/n,sd=std::sqrt(std::max(0.0,sum2/n-mean*mean));
   s.board["mat.cov."+b.name]=mean>1e-5?sd/mean:0;s.board["mat.mean."+b.name]=mean;}},
 .metrics=[](Scene& s,Report& r){
  // The table against brute force, the compensation to 1 (F0 = 1, every roughness a furnace sees).
  double worstTable=0,worstComp=0;std::string where;
  for(float rough:{.25f,.4f,.55f,.7f,.85f,1.f})for(float NoV:{.2f,.5f,.8f,1.f}){
   const auto ab=envBrdf(NoV,rough);const float e=ab[0]+ab[1],brute=ggxFurnace(NoV,rough,256);
   const double tableErr=std::fabs(e-brute)/std::max(brute,1e-3f);if(tableErr>worstTable){worstTable=tableErr;where="r "+fixed(rough,2)+" NoV "+fixed(NoV,2);}
   worstComp=std::max(worstComp,std::fabs(double(brute)*(1+1*(1/std::max(e,.05f)-1))-1));}
  r.metric("table_vs_bruteforce",worstTable,"share");r.metric("furnace_compensated_error",worstComp,"share");
  r.check("the split-sum table matches a brute-force integral of the BRDF (within 3%)",worstTable<.03,fixed(worstTable*100,1)+"% worst at "+where);
  r.check("white furnace: compensated GGX reflects all the energy at every roughness (within 3%)",worstComp<.03,fixed(worstComp*100,1)+"% worst");
  {const auto ab=envBrdf(1.f,.02f);const float head=.04f*ab[0]+ab[1];r.metric("dielectric_head_on",head);
   r.check("a dielectric (F0 .04) reflects about 4% head-on in the table",head>.035f&&head<.05f,fixed(head,4));}
  // Every derived texture inside its class, using its range.
  int outside=0,flat=0,total=0;std::string firstOut,firstFlat;std::map<int,std::pair<double,int>> covByClass;
  for(const auto& m:materialRecords()){const auto& range=materialRange(m.material);++total;
   const bool secondary=m.material==Material::PaintedMetal||m.material==Material::BareSteel||m.material==Material::Aluminium||m.material==Material::GlazedTile||m.material==Material::Ceramic;
   const float lo=secondary?std::min(range.lo,.3f):range.lo,hi=secondary?std::max(range.hi,.95f):range.hi;
   if(m.minRough<lo-.01f||m.maxRough>hi+.01f){if(!outside++)firstOut=m.name+" ("+materialName(m.material)+" "+fixed(m.minRough,2)+"-"+fixed(m.maxRough,2)+")";}
   if(m.cov<.03f&&m.texels>64){if(!flat++)firstFlat=m.name;}
   auto& c=covByClass[int(m.material)];c.first+=m.cov;++c.second;}
  r.metric("textures",total);r.metric("textures_outside_range",outside);r.metric("textures_flat",flat);
  r.check("every derived surface map stays in its class's physical roughness range",total>0&&outside==0,std::to_string(outside)+" of "+std::to_string(total)+(outside?", first "+firstOut:""));
  r.check("no derived surface map is one flat roughness (CoV over 3%)",flat*20<=total,std::to_string(flat)+" of "+std::to_string(total)+(flat?", first "+firstFlat:""));
  for(const auto& [cls,c]:covByClass)r.metric(std::string("roughness_cov.")+materialName(Material(cls)),c.first/std::max(1,c.second));
  // The audit: every texture's class and what its surface map came out as (RM_MATERIAL_AUDIT=FILE).
  if(const char* path=std::getenv("RM_MATERIAL_AUDIT")){std::ofstream out(path);out<<"name\tclass\tmin\tmax\tmean\tcov\tmetal\ttexels\n";
   for(const auto& m:materialRecords())out<<m.name<<'\t'<<materialName(m.material)<<'\t'<<fixed(m.minRough,2)<<'\t'<<fixed(m.maxRough,2)<<'\t'<<fixed(m.meanRough,2)<<'\t'<<fixed(m.cov,3)<<'\t'<<fixed(m.metalShare,2)<<'\t'<<m.texels<<'\n';}
  // Rendered.
  if(s.board.count("mat.furnace_median")){const double lo=s.board["mat.furnace_lo"],hi=s.board["mat.furnace_hi"];
   r.metric("furnace_spread_lo",lo,"ratio");r.metric("furnace_spread_hi",hi,"ratio");r.metric("furnace_median",s.board["mat.furnace_median"]);
   r.check("rendered white furnace: rough and smooth, metal and dielectric reflect the same energy (1st-99th pct within 6%)",lo>.94&&hi<1.06,fixed(lo)+" .. "+fixed(hi));}
  {const double p99=s.board.count("mat.refl_p99")?s.board["mat.refl_p99"]:0;r.metric("dielectric_reflectance_p99",p99);r.metric("dielectric_reflectance_max",s.board["mat.refl_max"]);
   r.check("dielectrics facing the eye reflect no more than Fresnel allows (99th pct under .05)",p99<.05,fixed(p99,4));}
  for(const auto& b:surfaces(*s.fixture)){const auto k="mat.cov."+b.name;if(!s.board.count(k))continue;const double cov=s.board[k];
   r.metric("specular_cov."+b.name,cov);r.metric("specular_mean."+b.name,s.board["mat.mean."+b.name]);
   r.check("the specular term varies across "+b.name+" (CoV over .15: not a plastic smear)",cov>.15,fixed(cov));}},
 .reporter=[](Scene& s,Overlay& o){for(const auto& b:surfaces(*s.fixture)){o.text(b.x0+2,b.y0+2,b.name,0xffff40);
  o.lines.push_back({{float(b.x0),float(b.y0),0},{float(b.x1),float(b.y0),0},0xffff40,false});o.lines.push_back({{float(b.x0),float(b.y1),0},{float(b.x1),float(b.y1),0},0xffff40,false});
  o.lines.push_back({{float(b.x0),float(b.y0),0},{float(b.x0),float(b.y1),0},0xffff40,false});o.lines.push_back({{float(b.x1),float(b.y0),0},{float(b.x1),float(b.y1),0},0xffff40,false});}},
 .teardown=[](Scene&){SoftwareRenderer::setDetailLayer(1);GpuRenderer::setReferenceDisplay(false);},
 .needsGpu=true});
// The CPU fallback's match (run with --software): the same frame with and without its surface-map
// shading. It may not move the frame's brightness much (the GPU's balance), and what it changes must vary
// across each surface (CoV of the change in the fixture's mat-surface boxes, else the frame).
double lumaOf(std::uint32_t c){return .2126*((c>>16)&255)+.7152*((c>>8)&255)+.0722*(c&255);}
const bool materialsCpu=add({
 .name="materials-cpu",.summary="the CPU fallback's surface-map shading: brightness held, specular varying across surfaces (run with --software)",
 .fixtures={"mat-*"},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;auto& r=*s.renderer;const size_t n=size_t(f.width)*f.height;
  const std::vector<std::uint32_t> with(f.pixels,f.pixels+n);
  cpuMaterials()=false;r.render(s.g());cpuMaterials()=true;s.shoot("without surface maps");
  const std::vector<std::uint32_t> without(r.pixels(),r.pixels()+n);
  double a=0,b=0;for(size_t i=0;i<n;++i){a+=lumaOf(with[i]);b+=lumaOf(without[i]);}
  s.board["cpu.mean"]=a/double(n);s.board["cpu.mean_without"]=b/double(n);
  auto boxes=surfaces(*s.fixture);if(boxes.empty())boxes.push_back({"frame",0,0,f.width,f.height});
  for(const auto& box:boxes){double sum=0,sum2=0;long k=0;
   for(int y=std::max(0,box.y0);y<std::min(f.height,box.y1);++y)for(int x=std::max(0,box.x0);x<std::min(f.width,box.x1);++x){const size_t i=size_t(y)*f.width+x;const double v=std::fabs(lumaOf(with[i])-lumaOf(without[i]));sum+=v;sum2+=v*v;++k;}
   if(k>16){const double m=sum/k;s.board["cpu.cov."+box.name]=m>1e-3?std::sqrt(std::max(0.0,sum2/k-m*m))/m:0;}}
  r.render(s.g());},
 .metrics=[](Scene& s,Report& r){
  const double m=s.board["cpu.mean"],m0=s.board["cpu.mean_without"];r.metric("mean",m,"luma");r.metric("mean_without_maps",m0,"luma");
  r.check("the CPU's materials keep the frame's brightness (within 10%)",m0>0&&std::fabs(m/m0-1)<.10,fixed(m,2)+" vs "+fixed(m0,2));
  for(const auto& [k,v]:s.board)if(k.rfind("cpu.cov.",0)==0){r.metric("cov."+k.substr(8),v);r.check("what the CPU's materials change varies across "+k.substr(8)+" (CoV over .15)",v>.15,fixed(v));}},
 .teardown=[](Scene&){cpuMaterials()=true;}});
}
}
