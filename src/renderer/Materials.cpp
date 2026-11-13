#include "Materials.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace retro {
namespace {
// Roughness ranges after Lagarde & de Rousiers (2014) and the UE4/Frostbite material charts; metal
// F0 lumas from measured reflectance (iron .56, aluminium .91, chrome .55, copper/brass ~.7).
constexpr MaterialRange Ranges[size_t(Material::Count)]={
 {"none",.5f,.8f,false,.04f,.5f},
 {"painted metal",.35f,.6f,false,.04f,.5f},
 {"bare steel",.3f,.5f,true,.56f,.45f},
 {"rust",.8f,.95f,false,.04f,.8f},
 {"chrome",.05f,.15f,true,.6f,.2f},
 {"aluminium",.3f,.5f,true,.8f,.4f},
 {"brass",.25f,.45f,true,.68f,.4f},
 {"concrete",.75f,.95f,false,.04f,.9f},
 {"plaster",.6f,.85f,false,.04f,.6f},
 {"stone",.7f,.95f,false,.04f,1.f},
 {"brick",.75f,.95f,false,.04f,.9f},
 {"glazed tile",.1f,.25f,false,.04f,.5f},
 {"vinyl",.35f,.5f,false,.04f,.4f},
 {"bare wood",.6f,.85f,false,.04f,.65f},
 {"varnished wood",.25f,.4f,false,.04f,.5f},
 {"fabric",.85f,1.f,false,.04f,.6f},
 {"rubber",.7f,.9f,false,.04f,.3f},
 {"plastic",.3f,.6f,false,.04f,.35f},
 {"glass",.02f,.05f,false,.04f,0.f},
 {"water",.02f,.1f,false,.02f,0.f},
 {"skin",.45f,.6f,false,.028f,.3f},
 {"clothing",.55f,.9f,false,.04f,.35f},
 {"crystal",.08f,.25f,false,.05f,.3f},
 {"paper",.8f,.95f,false,.04f,.3f},
 {"leather",.45f,.7f,false,.04f,.3f},
 {"ground",.85f,1.f,false,.04f,1.f},
 {"ceramic",.1f,.25f,false,.04f,.3f},
 {"foliage",.5f,.85f,false,.04f,.6f},
 {"screen",.05f,.15f,false,.04f,.1f}};
float toLinear(std::uint32_t c){static const auto table=[]{std::array<float,256> t{};for(int i=0;i<256;++i)t[size_t(i)]=std::pow(i/255.f,2.2f);return t;}();return table[c&255u];}
std::uint32_t toByte(float v){static const auto table=[]{std::array<std::uint8_t,4096> t{};for(int i=0;i<4096;++i)t[size_t(i)]=std::uint8_t(std::lround(std::pow(i/4095.f,1.f/2.2f)*255.f));return t;}();
 return table[size_t(std::clamp(v,0.f,1.f)*4095.f+.5f)];}
float saturationOf(float r,float g,float b){const float hi=std::max({r,g,b}),lo=std::min({r,g,b});return hi>1e-4f?(hi-lo)/hi:0.f;}
// Rust's hue: red over green over blue, saturated (orange to brown).
float rustiness(float r,float g,float b){const float s=r+g+b+1e-4f;const float rr=r/s,gg=g/s,bb=b/s;return std::clamp((rr-.40f)*5.f,0.f,1.f)*std::clamp((gg-bb)*8.f,0.f,1.f)*std::clamp((saturationOf(r,g,b)-.25f)*4.f,0.f,1.f);}
// A tiling box blur of radius r (wraps: the textures repeat).
// A running sum along each row, then each column: the same result, linear in the texel count.
std::vector<float> boxBlur(const std::vector<float>& in,int w,int h,int r){
 std::vector<float> mid(in.size()),out(in.size());const float k=1.f/float(2*r+1);
 auto wrap=[](int i,int n){return (i%n+n)%n;};
 for(int y=0;y<h;++y){const float* row=in.data()+size_t(y)*w;float s=0;for(int i=-r;i<=r;++i)s+=row[wrap(i,w)];
  for(int x=0;x<w;++x){mid[size_t(y)*w+x]=s*k;s+=row[wrap(x+r+1,w)]-row[wrap(x-r,w)];}}
 for(int x=0;x<w;++x){float s=0;for(int i=-r;i<=r;++i)s+=mid[size_t(wrap(i,h))*w+x];
  for(int y=0;y<h;++y){out[size_t(y)*w+x]=s*k;s+=mid[size_t(wrap(y+r+1,h))*w+x]-mid[size_t(wrap(y-r,h))*w+x];}}
 return out;}
}

const MaterialRange& materialRange(Material m){return Ranges[std::min(size_t(m),size_t(Material::Count)-1)];}

Material classifyMaterial(std::string_view raw){
 std::string name(raw);for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
 auto has=[&](const char* part){return name.find(part)!=std::string::npos;};
 auto any=[&](std::initializer_list<const char*> parts){for(auto* p:parts)if(has(p))return true;return false;};
 if(any({"crystal","gemstone"}))return Material::Crystal;
 if(any({"mirror"}))return Material::Chrome;
 if(any({"_screen","screen_","x_ray","glass"}))return has("glass")?Material::Glass:Material::Screen;
 // Water is the liquid surfaces' own (SoftwareRenderer marks them): a watercooler, a dewatering pump or
 // a water main is not water.
 if(any({"rust","corrod"}))return Material::Rust;
 if(any({"chrome","wheelchair"}))return Material::Chrome;
 // Brushed stainless (autopsy tables, scrub sinks, trays) and aluminium tube (crutches): broad, dim sheen.
 if(any({"stainless","autopsy","scrub_sink","tray"}))return Material::BareSteel;
 if(any({"crutches"}))return Material::Aluminium;
 if(any({"brass","copper","bronze","candle"}))return Material::Brass;
 if(any({"toilet","sink","ceramic","porcelain"}))return Material::Ceramic;
 if(any({"tire","rubber","hose","cable_reel"}))return Material::Rubber;
 if(any({"leather","office_chair","boardroom_chair"}))return Material::Leather;
 if(any({"fabric","cloth","curtain","couch","seat","mattress","bedroll","sleeping","carpet","sandbag","bags_","_bag","trash_heap_bag","blood_pack"}))return Material::Fabric;
 if(any({"bark","branch","leaf","leaves","tree","fern","grass","bush","ivy","moss","mushroom","fungus","clover","aster","bracken","sumac","dogbane","creeper","goldenrod","wheat","sapling","aspen","pine","oak","log_ends","plant","barnacle","nest_pod"}))return Material::Foliage;
 if(any({"desk","bookcase","cabinet_wood","display_cabinet_wood","table","chair","painting","wood_1"}))return has("metal")?Material::PaintedMetal:Material::VarnishedWood;
 if(any({"wood","plank","crate_8","crate_hx","pallet","palet","timber","board_barricade","prop_post","shoring","log","box_wood","barricade_hm"}))return Material::BareWood;
 if(any({"paper","cardboard","binder","book","notice","information_board","poster","sign","billboard","totem","plackard","labels","x_ray"}))return Material::Paper;
 if(any({"tile","marble","polished"}))return Material::GlazedTile;
 if(any({"brick","rooftiles","roof_tiles"}))return Material::Brick;
 if(any({"concrete","cement","jersey","barrier_road","roadblock","road_blocks","manhole","bollard"}))return has("metal")?Material::PaintedMetal:Material::Concrete;
 if(any({"rock","stone","cave_","n64","ossuary","crag","rubble","muck","ruin","artifact","relic","skeleton","res_rock"}))return Material::Stone;
 if(any({"soil","dirt","gravel","asphalt","mud","ground","litter","trash"}))return Material::Ground;
 if(any({"plaster","ceiling","wall_5","wall_8","stall","wall_wall","bareconcrete"}))return Material::Plaster;
 if(any({"plastic","cone","phone","mug","keyboard","keypad","computer","pc_","printer","electronics","etx","scanner","walkie","dosimeter","identification","medkit","medical_box","bottle","jar","cooler","coffee","fridge","refrigerador","vending","ticket","binders","lamp","helmet","flu_","monitor","dial","pot","misc_mp","medicine","extinguisher","fire_extenguisher","bandage","mp_1_on","radiator","cones"}))return Material::Plastic;
 if(any({"aluminium","aluminum","sheetmetal","roofing","metalplates","canopy","sat_dish","antenna","aerial"}))return Material::Aluminium;
 if(any({"painted","paint","locker","cabinet","container","shipping","barrel","drum","generator","genset","transformer","_box","panel","electrical","switch","door","gate_","machinery","pump","turbine","housing","fan","air_handler","fl_","duct","forklift","van","car_","car2","car3","bus","train","trains","metro_","tank","reservoir","silo","cooling","tower","building","hall","shed","hut","warehouse","annex","works","stack","fuel","yellow","hazard","control","cart","wagon","chest","tool","generator","cylinder","firepot","brazier","burn_barrel","still","stovepipe","lantern","shade_lamp","shack","leanto","foundry","server","scifi","scope","material","texture","wall_6","wall_7","wall_box","detonator","tnt"}))
  return any({"metal_hr","metalbare","metal_mp","metal_1","metal_2","metal_3","metal_4","metal_6","beams","steel_beams"})&&!any({"painted","locker","cabinet"})?Material::BareSteel:Material::PaintedMetal;
 if(any({"metal","steel","iron","pipe","beam","grate","catwalk","ladder","rail","mesh","fence","bars","cage","chain","shovel","beak","pick","bare","floor","stairs","vent","grid","shield","valve","grille","handrail"}))return Material::BareSteel;
 return Material::Plaster;
}

std::vector<MaterialRecord>& materialRecords(){static std::vector<MaterialRecord> records;return records;}

std::vector<std::uint32_t> deriveSurfaceMap(std::vector<std::uint32_t>& pixels,int w,int h,Material m,std::string_view name){
 std::vector<std::uint32_t> out;if(w<1||h<1||pixels.size()!=size_t(w)*size_t(h))return out;
 const auto& range=materialRange(m);const size_t n=pixels.size();
 std::vector<float> r(n),g(n),b(n),L(n);std::vector<char> solid(n);
 double meanR=0,meanG=0,meanB=0,meanL=0;size_t count=0;
 for(size_t i=0;i<n;++i){const auto p=pixels[i];r[i]=toLinear(p>>16);g[i]=toLinear(p>>8);b[i]=toLinear(p);L[i]=.2126f*r[i]+.7152f*g[i]+.0722f*b[i];solid[i]=(p>>24)>=128;
  if(solid[i]){meanR+=r[i];meanG+=g[i];meanB+=b[i];meanL+=L[i];++count;}}
 if(!count){count=1;}
 meanR/=double(count);meanG/=double(count);meanB/=double(count);meanL/=double(count);
 // Detail at three scales: a 3x3 smoothing of the texel (no single-texel noise), the 9x9 neighbourhood
 // it stands out of (pits, scratches, seams) and the texture's own mean (grime, stains, wear).
 const int small=std::min({1,w/4,h/4}),large=std::max(1,std::min({4,w/4,h/4}));
 const auto L3=small>0?boxBlur(L,w,h,small):L,L9=boxBlur(L,w,h,large);
 double hfEnergy=0;for(size_t i=0;i<n;++i){const float d=L3[i]-L9[i];hfEnergy+=double(d)*d;}
 const float hfScale=2.5f*float(std::sqrt(hfEnergy/double(n)))+1e-4f;
 const float meanSat=saturationOf(float(meanR),float(meanG),float(meanB)),meanRust=rustiness(float(meanR),float(meanG),float(meanB));
 const float mR=float(meanR),mG=float(meanG),mB=float(meanB),mL=float(meanL),mSum=mR+mG+mB+1e-4f;
 std::vector<float> t(n),metal(n,range.metal?1.f:0.f),cavity(n,1.f),second(n,0.f);
 // The secondary material inside the class: chips through paint (bare steel), rust on steel (rough
 // dielectric), grout between glazed tiles (rough). Where a secondary feature covers more than a third
 // of the texture it is the texture's colour, not a feature, and fades out.
 for(size_t i=0;i<n;++i){
  const float s=r[i]+g[i]+b[i]+1e-4f;
  const float chroma=std::fabs(r[i]/s-mR/mSum)+std::fabs(g[i]/s-mG/mSum)+std::fabs(b[i]/s-mB/mSum);
  const float dark=std::clamp((mL-L3[i])/(mL+.02f),-1.f,1.f);
  const float pit=std::clamp((L9[i]-L3[i])/hfScale,-1.f,1.f);
  // Darker than the texture (grime, stains) and darker than the neighbourhood (pits, seams) is rougher;
  // brighter (worn raised edges, polish) smoother; a colour off the texture's own (stains) rougher.
  t[i]=.5f+.22f*dark+.25f*pit+.25f*std::min(chroma*3.f,1.f);
  cavity[i]=1.f-.45f*std::clamp((L9[i]-L3[i])/(L9[i]+.03f)*3.f,0.f,1.f);
  // Rust on bare metal is rust whatever the rest of the sheet is (a rust-red tank plate is rust, not a
  // brown metal); on paint it is what stands out from the paint's own colour (an orange paint is paint).
  const float rust=m==Material::PaintedMetal?std::max(0.f,rustiness(r[i],g[i],b[i])-meanRust):rustiness(r[i],g[i],b[i]);
  if(m==Material::BareSteel||m==Material::Aluminium||m==Material::PaintedMetal)second[i]=std::clamp(rust*2.f,0.f,1.f);
  if(m==Material::PaintedMetal&&meanSat>.18f){const float sat=saturationOf(r[i],g[i],b[i]);
   const float chip=std::clamp((meanSat*.55f-sat)/(meanSat*.35f),0.f,1.f)*std::clamp((std::fabs(L3[i]-mL)/(mL+.02f)-.15f)*4.f,0.f,1.f);
   second[i]=std::max(second[i],0.f);metal[i]=std::max(metal[i],chip*(1.f-second[i]));}
  if(m==Material::GlazedTile||m==Material::Ceramic)second[i]=std::clamp((pit-.35f)*2.5f,0.f,1.f);
  // A metal atlas's labels, stencils and painted parts (far brighter or more saturated than its metal)
  // are paint: a dielectric at the paint's roughness, never a metal brought up to F0.
  // Bare steel, aluminium and chrome are grey: a beige, tan or brown texel on their sheet is a coating,
  // grime or a painted plate, a dielectric (brass keeps its own yellow).
  if(range.metal&&m!=Material::Brass){const float sat=saturationOf(r[i],g[i],b[i]);metal[i]*=1.f-std::clamp((sat-.2f)/.15f,0.f,1.f);}
  if(range.metal){const float sat=saturationOf(r[i],g[i],b[i]);
   const float paint=std::max(std::clamp((L3[i]/(mL+.01f)-2.2f)*1.5f,0.f,1.f),std::clamp((sat-meanSat-.3f)*4.f,0.f,1.f));
   metal[i]=std::min(metal[i],1.f-paint);if(paint>0)t[i]=std::min(t[i],.5f);}
 }
 auto share=[&](const std::vector<float>& v){double s=0;for(size_t i=0;i<n;++i)s+=v[i];return float(s/double(n));};
 // Rust and chips come in patches, not single texels: a speckled atlas would print them as dirt.
 {const auto soft=boxBlur(second,w,h,std::max(1,std::min({2,w/8,h/8})));for(size_t i=0;i<n;++i)second[i]=std::clamp((soft[i]-.25f)*2.5f,0.f,1.f);
  if(!range.metal){const auto chips=boxBlur(metal,w,h,1);for(size_t i=0;i<n;++i)metal[i]=std::clamp((chips[i]-.2f)*2.f,0.f,1.f);}}
 // (Rust over most of a bare metal sheet is a rusted sheet: kept. Grout or rust on paint that covers
 // most of the texture is its colour, not a feature: faded.)
 // A bare metal sheet's metal comes in regions too: a speckled sheet is a coated or dirty metal, not
 // single bright and black texels.
 if(range.metal){const auto soft=boxBlur(metal,w,h,std::max(1,std::min({3,w/8,h/8})));metal=soft;}
 if(const float c=share(second);c>.33f&&!(range.metal&&m!=Material::GlazedTile)){const float k=std::clamp((.5f-c)/.17f,0.f,1.f);for(auto& v:second)v*=k;}
 if(!range.metal){const float c=share(metal);if(c>.33f){const float k=std::clamp((.5f-c)/.17f,0.f,1.f);for(auto& v:metal)v*=k;}}
 // Stretch the detail over the class's range (its 2nd to 98th percentile to 10%-90% of it): every
 // texture uses the spread its class allows instead of one value.
 {std::vector<float> sorted(t);const size_t lo=n/50,hi=n-1-n/50;std::nth_element(sorted.begin(),sorted.begin()+long(lo),sorted.end());const float p2=sorted[lo];
  std::nth_element(sorted.begin(),sorted.begin()+long(hi),sorted.end());const float p98=sorted[hi];const float span=std::max(p98-p2,.08f);
  for(auto& v:t)v=std::clamp(.1f+.8f*(v-p2)/span,0.f,1.f);}
 // A metal's albedo is its F0: bring its texels to the class's measured reflectance, keeping their
 // variation and hue (the packs paint steel as dark grey diffuse).
 double metalL=0,metalW=0;for(size_t i=0;i<n;++i)if(solid[i]){const float k=metal[i]*(1.f-(range.metal?second[i]:0.f));metalL+=double(L[i])*k;metalW+=k;}
 const float metalF0=m==Material::PaintedMetal?materialRange(Material::BareSteel).f0:range.f0;
 const float f0Scale=metalW>0?std::clamp(metalF0/float(std::max(metalL/metalW,1e-3)),1.f,5.f):1.f;
 out.resize(n);double sum=0,sum2=0,metalSum=0;float minR=1,maxR=0;
 for(size_t i=0;i<n;++i){
  float rough=range.lo+t[i]*(range.hi-range.lo),k=metal[i];
  if(second[i]>0){
   if(m==Material::GlazedTile||m==Material::Ceramic)rough+=(.7f+.2f*t[i]-rough)*second[i];
   else{const auto& rust=materialRange(Material::Rust);rough+=(rust.lo+t[i]*(rust.hi-rust.lo)-rough)*second[i];k*=1.f-second[i];}}
  if(m==Material::PaintedMetal&&k>0){const auto& steel=materialRange(Material::BareSteel);rough+=(steel.lo+t[i]*(steel.hi-steel.lo)-rough)*k;}
  // Albedo: a metal texel to its F0; a dielectric out of the black no real surface has (linear .006).
  const bool keepAlbedo=m==Material::Glass||m==Material::Water||m==Material::Crystal||m==Material::Screen;
  float cr=r[i],cg=g[i],cb=b[i];const float scale=keepAlbedo?1.f:1.f+(f0Scale-1.f)*k;cr*=scale;cg*=scale;cb*=scale;
  const float l=.2126f*cr+.7152f*cg+.0722f*cb;
  if(!keepAlbedo&&k<.5f&&l<.006f){if(l>1e-5f){const float lift=.006f/l;cr*=lift;cg*=lift;cb*=lift;}else cr=cg=cb=.006f;}
  if(scale!=1.f||(!keepAlbedo&&l<.006f))pixels[i]=(pixels[i]&0xff000000u)|(toByte(std::min(cr,.96f))<<16)|(toByte(std::min(cg,.96f))<<8)|toByte(std::min(cb,.96f));
  const auto R=std::uint32_t(std::clamp(int(std::lround(rough*255.f)),0,255)),M=std::uint32_t(std::clamp(int(std::lround(k*255.f)),0,255)),C=std::uint32_t(std::clamp(int(std::lround(cavity[i]*255.f)),0,255));
  out[i]=0xff000000u|(C<<16)|(M<<8)|R;
  const float q=float(R)/255.f;if(solid[i]){sum+=q;sum2+=double(q)*q;metalSum+=float(M)/255.f;minR=std::min(minR,q);maxR=std::max(maxR,q);}
 }
 const double mean=sum/double(count),var=std::max(0.0,sum2/double(count)-mean*mean);
 static std::mutex lock;std::lock_guard guard(lock);
 materialRecords().push_back({std::string(name),m,minR,maxR,float(mean),float(std::sqrt(var)/std::max(mean,1e-3)),float(metalSum/double(count)),int(count)});
 return out;
}

// GGX with alpha = r^2, the height-correlated Smith visibility (Heitz 2014) the shader uses.
namespace {
float visibility(float NoV,float NoL,float a2){const float gv=NoL*std::sqrt(NoV*NoV*(1-a2)+a2),gl=NoV*std::sqrt(NoL*NoL*(1-a2)+a2);return .5f/std::max(gv+gl,1e-7f);}
float distribution(float NoH,float a2){const float d=NoH*NoH*(a2-1)+1;return a2/(3.14159265f*d*d);}
std::vector<float> computeEnvBrdf(){
 std::vector<float> table(size_t(EnvBrdfSize*EnvBrdfSize*2));constexpr int Samples=1024;
 for(int row=0;row<EnvBrdfSize;++row)for(int column=0;column<EnvBrdfSize;++column){
  // Texel centres on the ends of both ranges (NoV and roughness 0 and 1 exactly): the shader reads
  // u = (v (N-1) + .5) / N.
  const float NoV=std::max(column/float(EnvBrdfSize-1),1e-3f),rough=row/float(EnvBrdfSize-1),a=rough*rough,a2=std::max(a*a,1e-7f);
  const float vx=std::sqrt(1-NoV*NoV),vz=NoV;double A=0,B=0;
  for(int i=0;i<Samples;++i){
   // Hammersley point, GGX importance sampling of the half vector.
   unsigned bits=unsigned(i);bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);
   const float u1=(i+.5f)/Samples,u2=float(bits)*2.3283064365386963e-10f;
   const float phi=6.2831853f*u2,cosT=std::sqrt((1-u1)/(1+(a2-1)*u1)),sinT=std::sqrt(1-cosT*cosT);
   const float hx=sinT*std::cos(phi),hy=sinT*std::sin(phi),hz=cosT;
   const float VoH=vx*hx+vz*hz,lx=2*VoH*hx-vx,lz=2*VoH*hz-vz;(void)lx;(void)hy;
   const float NoL=lz,NoH=hz;if(NoL<=0||VoH<=0)continue;
   // pdf(L) = D NoH / (4 VoH): f NoL / pdf = 4 Vis NoL VoH / NoH.
   const float g=4*visibility(NoV,NoL,a2)*NoL*VoH/NoH,fc=std::pow(1-VoH,5.f);
   A+=(1-fc)*g;B+=fc*g;}
  table[size_t((row*EnvBrdfSize+column)*2)]=float(A/Samples);table[size_t((row*EnvBrdfSize+column)*2+1)]=float(B/Samples);}
 return table;}
}
const std::vector<float>& envBrdfTable(){static const std::vector<float> table=computeEnvBrdf();return table;}
std::array<float,2> envBrdf(float NoV,float roughness){
 const auto& t=envBrdfTable();const float x=std::clamp(NoV,0.f,1.f)*(EnvBrdfSize-1),y=std::clamp(roughness,0.f,1.f)*(EnvBrdfSize-1);
 const int x0=int(x),y0=int(y),x1=std::min(x0+1,EnvBrdfSize-1),y1=std::min(y0+1,EnvBrdfSize-1);const float fx=x-x0,fy=y-y0;
 std::array<float,2> out{};for(int c=0;c<2;++c){auto at=[&](int xx,int yy){return t[size_t((yy*EnvBrdfSize+xx)*2+c)];};
  out[size_t(c)]=(at(x0,y0)*(1-fx)+at(x1,y0)*fx)*(1-fy)+(at(x0,y1)*(1-fx)+at(x1,y1)*fx)*fy;}
 return out;}
float ggxFurnace(float NoV,float roughness,int steps){
 // Uniform over the hemisphere in (cos theta, phi), midpoint rule: E = int f NoL dw, F = 1.
 const float a=roughness*roughness,a2=a*a,vx=std::sqrt(std::max(0.f,1-NoV*NoV)),vz=NoV;double sum=0;
 // Sharp lobes need fine steps near the mirror direction: sample cos theta finer where it matters.
 const int thetaSteps=steps*4,phiSteps=steps*2;
 for(int i=0;i<thetaSteps;++i){const float cz=(i+.5f)/thetaSteps,sz=std::sqrt(1-cz*cz);
  for(int j=0;j<phiSteps;++j){const float phi=6.2831853f*(j+.5f)/phiSteps;const float lx=sz*std::cos(phi),ly=sz*std::sin(phi),lz=cz;
   float hx=lx+vx,hy=ly,hz=lz+vz;const float hl=std::sqrt(hx*hx+hy*hy+hz*hz);if(hl<1e-6f)continue;hx/=hl;hy/=hl;hz/=hl;
   sum+=double(distribution(hz,a2)*visibility(NoV,lz,a2)*lz);}}
 return float(sum*(6.2831853/phiSteps)*(1.0/thetaSteps));}
}
