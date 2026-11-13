// The look: reflections, brightness and finish on fixed views of the campaign and the Starter Map,
// against the numbers v1.4.1 drew of the same views (lab/fixtures/look-*.lab, look-v141). v1.4.2's
// probe fill and screen reflections brightened lit rooms, cut floors and water into horizontal bands
// and made paint, wood and worn metal read as glass; this tube holds each of those to the release
// before it.
//
// Per view: the frame's mean luma and 90th percentile (0..255, as displayed), the band energy of a
// reflective region (the RMS of its row means against their 9-row running mean: horizontal bands
// across a reflection show as rows brighter or darker than their neighbours; texture averages out
// along the row) and row-over-column gradient energy (streak), then the same frame with reflections
// off (lookReflections), so what they add is measured where the fixture says the surface is matte
// (look-matte / look-matte2 boxes) and where it is a mirror (look-mirror).
//
// Fixture keys: look-v141 MEAN BAND P90 (the baseline), look-v142 the same for v1.4.2 (reported),
// look-dark 1 (a dark-by-design view: may not brighten, need not match), look-band-box X0 Y0 X1 Y1
// (default the lower half), look-mirror / look-matte / look-matte2 X0 Y0 X1 Y1, look-lift M.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include "renderer/LookTuning.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Box {int x0=0,y0=0,x1=0,y1=0;bool set=false;};
Box boxOf(const Fixture& f,const char* key){Box b;const auto t=f.value(key);if(!t.empty()&&std::sscanf(t.c_str(),"%d %d %d %d",&b.x0,&b.y0,&b.x1,&b.y1)==4)b.set=true;return b;}
double lumaOf(std::uint32_t c){return .2126*((c>>16)&255)+.7152*((c>>8)&255)+.0722*(c&255);}
std::vector<double> lumas(const std::vector<std::uint32_t>& p){std::vector<double> out(p.size());for(size_t i=0;i<p.size();++i)out[i]=lumaOf(p[i]);return out;}
// Row means over the box against their 9-row running mean (edges repeated), RMS: horizontal bands.
double band(const std::vector<double>& L,int w,const Box& b){
 const int rows=b.y1-b.y0;if(rows<10)return 0;std::vector<double> m(size_t(rows),0);
 for(int y=0;y<rows;++y){double s=0;for(int x=b.x0;x<b.x1;++x)s+=L[size_t(b.y0+y)*w+x];m[size_t(y)]=s/std::max(1,b.x1-b.x0);}
 double sum=0;for(int y=0;y<rows;++y){double a=0;for(int k=-4;k<=4;++k)a+=m[size_t(std::clamp(y+k,0,rows-1))];sum+=std::pow(m[size_t(y)]-a/9.0,2);}
 return std::sqrt(sum/rows);}
// Row-to-row over column-to-column gradient energy in the box.
double streak(const std::vector<double>& L,int w,const Box& b){
 double dy=0,dx=0;long ny=0,nx=0;
 for(int y=b.y0;y<b.y1;++y)for(int x=b.x0;x<b.x1;++x){const double v=L[size_t(y)*w+x];
  if(y+1<b.y1){dy+=std::pow(L[size_t(y+1)*w+x]-v,2);++ny;}if(x+1<b.x1){dx+=std::pow(L[size_t(y)*w+x+1]-v,2);++nx;}}
 return ny&&nx?(dy/ny)/std::max(1e-6,dx/nx):0;}
double meanIn(const std::vector<double>& L,int w,const Box& b){double s=0;long n=0;for(int y=b.y0;y<b.y1;++y)for(int x=b.x0;x<b.x1;++x){s+=L[size_t(y)*w+x];++n;}return n?s/n:0;}
double percentile(std::vector<double> L,double q){if(L.empty())return 0;const size_t k=size_t(q*(L.size()-1));std::nth_element(L.begin(),L.begin()+long(k),L.end());return L[k];}

const bool look=add({
 .name="look",.summary="the look against v1.4.1 on fixed views: mean brightness, horizontal reflection bands, and no reflection on matte finishes (wood, concrete, paint)",
 .fixtures={"look-*"},
 .setup=[](Scene& s){lookReflections()=true;const float lift=s.fixture->number("look-lift",0);if(lift!=0)LabAccess::player(s.g()).z+=lift;},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;auto& r=*s.renderer;const int w=f.width,h=f.height;
  const std::vector<std::uint32_t> with(f.pixels,f.pixels+size_t(w)*h);const auto L=lumas(with);
  Box bandBox=boxOf(*s.fixture,"look-band-box");if(!bandBox.set)bandBox={0,h/2,w,h,true};
  s.board["look.mean"]=std::accumulate(L.begin(),L.end(),0.0)/double(L.size());s.board["look.p90"]=percentile(L,.9);
  s.board["look.band"]=band(L,w,bandBox);s.board["look.streak"]=streak(L,w,bandBox);
  // The same frame without reflections: what they add, and where.
  lookReflections()=false;r.render(s.g());lookReflections()=true;s.shoot("reflections off");
  const std::vector<std::uint32_t> without(r.pixels(),r.pixels()+size_t(w)*h);const auto L0=lumas(without);
  std::vector<double> added(L.size());for(size_t i=0;i<L.size();++i)added[i]=std::fabs(L[i]-L0[i]);
  Box all{0,0,w,h,true};s.board["look.refl"]=meanIn(added,w,all);
  // What the reflections add, four times over, on a dark copy of the frame: where they are and whether they band.
  {Scene::Shot shot{"reflections only (x4)",w,h,std::vector<std::uint32_t>(with.size())};
   for(size_t i=0;i<with.size();++i){const int g=int(L0[i]*.25);const int a=std::clamp(int(added[i]*4),0,255);shot.pixels[i]=0xff000000u|(std::uint32_t(std::min(255,g+a))<<16)|(std::uint32_t(std::min(255,g+a))<<8)|std::uint32_t(g);}
   s.shots.push_back(std::move(shot));}
  std::vector<double> signedAdded(L.size());for(size_t i=0;i<L.size();++i)signedAdded[i]=L[i]-L0[i];
  for(const char* key:{"look-mirror","look-matte","look-matte2"}){const Box b=boxOf(*s.fixture,key);if(!b.set)continue;const std::string k=std::string("look.")+(key+5);
   s.board[k]=meanIn(added,w,b);s.board[k+"_band"]=band(signedAdded,w,b);s.board[k+".set"]=1;}
  // The finish as the post pass reads it (--detail 12): the face's axis, smoothness, the water film.
  {const int layer=SoftwareRenderer::detailLayer();SoftwareRenderer::setDetailLayer(12);r.render(s.g());s.shoot("finish as post reads it (detail 12)");SoftwareRenderer::setDetailLayer(layer);}
  r.render(s.g());},
 .metrics=[](Scene& s,Report& r){
  const auto& fx=*s.fixture;const double mean=s.board["look.mean"],p90=s.board["look.p90"],bnd=s.board["look.band"];
  r.metric("mean",mean,"luma");r.metric("p90",p90,"luma");r.metric("band",bnd,"luma");r.metric("streak",s.board["look.streak"]);r.metric("reflection",s.board["look.refl"],"luma");
  double b141[3]{},b142[3]{};const bool has141=std::sscanf(fx.value("look-v141").c_str(),"%lf %lf %lf",&b141[0],&b141[1],&b141[2])==3,has142=std::sscanf(fx.value("look-v142").c_str(),"%lf %lf %lf",&b142[0],&b142[1],&b142[2])==3;
  const bool dark=fx.number("look-dark",0)>0;
  if(has141){r.metric("mean_vs_v141",mean/std::max(1e-3,b141[0]),"ratio");r.metric("band_v141",b141[1],"luma");
   if(dark)r.check("a dark-by-design view stays dark (mean at most 10% over v1.4.1)",mean<=b141[0]*1.10,fixed(mean)+" vs "+fixed(b141[0]));
   else{r.check("mean brightness within 5% of v1.4.1",std::fabs(mean/b141[0]-1)<=.05,fixed(mean)+" vs "+fixed(b141[0])+(has142?" (v1.4.2 "+fixed(b142[0])+")":""));
    r.check("the lit areas are no brighter than v1.4.1 (90th percentile, 5%)",p90<=b141[2]*1.05,fixed(p90)+" vs "+fixed(b141[2]));}
   r.check("no horizontal reflection bands beyond v1.4.1's",bnd<=std::max(b141[1]*1.1,b141[1]+.15),fixed(bnd,3)+" vs "+fixed(b141[1],3)+(has142?" (v1.4.2 "+fixed(b142[1],3)+")":""));}
  if(has142)r.metric("mean_vs_v142",mean/std::max(1e-3,b142[0]),"ratio");
  for(const char* key:{"look-matte","look-matte2"}){const std::string k=std::string("look.")+(key+5);if(!s.board.count(k+".set"))continue;const double v=s.board[k];
   r.metric(std::string(key+5)+"_reflection",v,"luma");r.check(std::string("the matte finish in ")+key+" shows no reflection (under 1.0 luma)",v<1.0,fixed(v,3));}
  if(s.board.count("look.mirror.set")){const double v=s.board["look.mirror"],b=s.board["look.mirror_band"];
   r.metric("mirror_reflection",v,"luma");r.metric("mirror_band",b,"luma");
   r.check("the mirror in look-mirror still reflects (over 0.5 luma)",v>.5,fixed(v,3));}},
 .teardown=[](Scene&){lookReflections()=true;},
 .needsGpu=true});
}
}
