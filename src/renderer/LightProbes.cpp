#include "renderer/LightProbes.h"
#include "world/ModelIds.h"
#include "world/CustomCampaign.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <thread>
#include <unordered_map>
namespace retro {
namespace {
constexpr Point3 Axes[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 times(Point3 a,Point3 b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
// Directions spread evenly over the sphere (a Fibonacci spiral), the same every bake.
std::vector<Point3> sphere(int count){
 std::vector<Point3> out;out.reserve(size_t(count));const float golden=2.3999632f;
 for(int i=0;i<count;++i){float z=1.f-2.f*(i+.5f)/count,r=std::sqrt(std::max(0.f,1.f-z*z)),a=golden*i;out.push_back({r*std::cos(a),r*std::sin(a),z});}
 return out;
}
// Linear, unit-luminance colour of the crystal's glow: the cyan of its burning tips, keyed cold but
// pale (a saturated cyan summed over a wall of growth turned the whole room teal).
constexpr Point3 CrystalTone{.42f,1.1f,1.6f};
// How much of a crystal's glow reaches `d2` (squared metres) away: bright beside it, gone within a
// few metres (it lights its own floor and the stone it grows from, not the room).
float crystalGlow(const ProbeEmitter& e,float d2){const float t=1.f-d2/(e.range*e.range);return e.power*2.4f*t*t/(1.f+d2*.9f);}
}

std::array<float,4> IrradianceVolume::sample(Point3 p,Point3 n)const{
 if(empty())return {0,0,0,1};
 float length=std::sqrt(dot3(n,n));n=length>1e-5f?n*(1.f/length):Point3{0,0,1};
 const Point3 q{p.x+n.x*cell*.5f,p.y+n.y*cell*.5f,p.z+n.z*cellZ*.5f};
 const float f[3]={std::clamp((q.x-origin.x)/cell-.5f,0.f,float(nx-1)),std::clamp((q.y-origin.y)/cell-.5f,0.f,float(ny-1)),std::clamp((q.z-origin.z)/cellZ-.5f,0.f,float(nz-1))};
 const int size[3]={nx,ny,nz};int c0[3];float t[3];
 for(int k=0;k<3;++k){c0[k]=std::min(int(std::floor(f[k])),std::max(0,size[k]-2));t[k]=size[k]>1?f[k]-c0[k]:0.f;}
 const int faces[3]={n.x>=0?0:1,n.y>=0?2:3,n.z>=0?4:5};const float weights[3]={n.x*n.x,n.y*n.y,n.z*n.z};
 std::array<float,4> out{};
 for(int corner=0;corner<8;++corner){
  int c[3];float w=1;
  for(int k=0;k<3;++k){const int bit=(corner>>k)&1;c[k]=std::min(c0[k]+bit,size[k]-1);w*=bit?t[k]:1.f-t[k];}
  if(w<=0)continue;const float* probe=data.data()+index(c[0],c[1],c[2])*Stride;
  for(int a=0;a<3;++a){const float* face=probe+faces[a]*4;for(int ch=0;ch<4;++ch)out[size_t(ch)]+=face[ch]*w*weights[a];}
 }
 return out;
}
std::array<float,4> IrradianceVolume::ambient(Point3 p)const{
 std::array<float,4> sum{};
 for(const auto&axis:Axes){auto s=sample(p-axis*(cell*.5f),axis);for(size_t c=0;c<4;++c)sum[c]+=s[c]/6.f;}
 return sum;
}

std::vector<ProbeEmitter> crystalEmitters(const World& w){
 std::vector<ProbeEmitter> out;
 for(const auto&f:w.fixtures()){
  if(!f.visible||f.model<model::res_crystal_l||f.model>model::res_crystal_wall)continue;
  // Brightness by the cluster's size against its model's own (the big knot, the middle one, the
  // scatter, the wall growth): a cluster scaled down glows less and reaches less far.
  const float nominal=model::size[f.model][2],scale=std::clamp(f.height/std::max(.1f,nominal),.3f,2.f);
  const float power=f.model==model::res_crystal_l?4.f:f.model==model::res_crystal_m?2.f:f.model==model::res_crystal_s?.8f:1.6f;
  const float range=f.model==model::res_crystal_l?6.f:f.model==model::res_crystal_m?4.5f:f.model==model::res_crystal_s?2.8f:3.6f;
  const float base=w.floorHeight(f.position.x,f.position.y)+f.base;
  // The glow sits in the cores, a little above the root's crown.
  out.push_back({{f.position.x,f.position.y,base+f.height*.5f},CrystalTone,power*scale,range*std::sqrt(scale),std::max(.15f,std::min(f.width,f.depth)*.4f)});
 }
 return out;
}

ProbeBake bakeLightProbes(const World& w,const ProbeBakeInput& input){
 const auto start=std::chrono::steady_clock::now();
 ProbeBake bake;auto& v=bake.volume;
 // Build the optical solids once, before any worker reads them.
 {Point3 n;w.lightRayHit({.5f,.5f,0},{.6f,.5f,0},n);}
 const bool outdoors=w.outdoors();
 // The grid's height: the lowest floor to the highest ceiling or deck, at most 48 cells.
 float low=1e9f,high=-1e9f;
 std::array<float,World::Width*World::Height> floors{},ceilings{};
 for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;++x){
  const float f=w.floorHeight(x+.5f,y+.5f),c=w.ceilingHeight(x+.5f,y+.5f);
  floors[size_t(y*World::Width+x)]=f;ceilings[size_t(y*World::Width+x)]=std::min(c,f+(outdoors?9.f:40.f));
  if(w.tile(x,y)=='#')continue;low=std::min(low,f);high=std::max(high,ceilings[size_t(y*World::Width+x)]);
 }
 for(const auto&s:w.structures())if(s.top>low&&s.top<low+48.f)high=std::max(high,std::min(s.top+3.f,low+48.f));
 if(low>high){low=0;high=3;}
 v.nx=v.ny=IrradianceVolume::Width;v.cell=1.f;
 v.nz=std::clamp(int(std::ceil(high-low)),1,IrradianceVolume::Depth);v.cellZ=std::max(1.f,(high-low)/float(v.nz));
 v.origin={0,0,low};
 const size_t count=size_t(v.nx)*v.ny*v.nz;
 v.data.assign(count*IrradianceVolume::Stride,0.f);v.valid.assign(count,0);
 // Open air: above its column's floor, under its ceiling and in no solid. Decided here, on one
 // thread, because the collision queries keep caches of their own.
 for(int z=0;z<v.nz;++z)for(int y=0;y<v.ny;++y)for(int x=0;x<v.nx;++x){
  const Point3 p=v.position(x,y,z);const size_t column=size_t(y*World::Width+x);
  bool open=w.tile(x,y)!='#'&&p.z>floors[column]+.04f&&p.z<ceilings[column]-.04f&&!w.insideLightSolid(p);
  if(open&&outdoors)open=w.fits(p.x,p.y,p.z,.01f,false,false);
  v.valid[v.index(x,y,z)]=open?1:0;v.openProbes+=open;
 }

 // The lamps as the bake sees them, at the power they are baked at.
 struct Lamp {const WorldLight* light;Point3 emitter,target,tone;float power;};
 std::vector<Lamp> lamps;
 for(size_t i=0;i<w.lights().size();++i){const auto&l=w.lights()[i];
  if(l.cargoLift||(w.hasLift()&&i+1==w.lights().size()))continue;
  const float power=i<input.lampPower.size()?input.lampPower[i]:1.f;if(power<=0||l.intensity<=0)continue;
  lamps.push_back({&l,l.emitter(),l.shadowTarget(),l.tone(),power});}
 const SunLight sun=w.sun();
 const Point3 sunTone{sun.r,sun.g,sun.b};
 const ProbeSurfaces& surfaces=input.surfaces;
 auto albedo=[&](Point3 n){return n.z>.6f?surfaces.floor:n.z<-.6f?surfaces.ceiling:surfaces.wall;};
 // Light falling on a surface at q facing n: every lamp in reach through its beam (shadowed as the
 // vertex bake shadows it, but never through a wall), the crystals, and the sun outdoors.
 auto irradiance=[&](Point3 q,Point3 n,std::uint64_t& rays){
  Point3 sum{};const Point3 from=q+n*.03f;
  for(const auto&l:lamps){const Point3 d=l.emitter-q;const float d2=dot3(d,d),range=l.light->range;if(d2>=range*range||d2<1e-4f)continue;
   const float distance=std::sqrt(d2),facing=dot3(n,d)/distance;if(facing<=0)continue;
   const float f=l.light->falloff(d2)*l.power*l.light->beam(d*(-1.f/distance));if(f<=.002f)continue;
   // A wall between them stops it outright: bounce light never comes through a wall.
   if(l.light->shadows){Point3 hit;++rays;if(w.lightRayHit(from,l.target,hit)<.999f)continue;}
   sum=sum+l.tone*(f*(.04f+.96f*facing));}
  for(const auto&e:input.emitters){const Point3 d=e.position-q;const float d2=dot3(d,d);if(d2>=e.range*e.range||d2<1e-4f)continue;
   const float distance=std::sqrt(d2),facing=dot3(n,d)/distance;if(facing<=0)continue;
   const float f=crystalGlow(e,d2);
   // The crystal's own body stops a ray only at its heart, not at its skin.
   Point3 hit;++rays;const float reach=w.lightRayHit(from,e.position,hit);if(reach<.999f&&(1.f-reach)*distance>e.radius*1.6f)continue;
   sum=sum+e.tone*(f*facing);}
  if(sun.intensity>0){const float c=dot3(n,sun.direction);if(c>0){Point3 hit;++rays;if(w.lightRayHit(from,from+sun.direction*80.f,hit)>=.999f)sum=sum+sunTone*(sun.intensity*c);}}
  return sum;
 };
 // A surface patch's returned light is computed once per half-metre square of its plane and shared
 // by every probe that sees it. The patch is keyed (and lit) by the square, never by the ray that
 // found it, so the result does not depend on which probe or thread got there first.
 auto patchKey=[](Point3 q,Point3 n,Point3& at,Point3& facing){
  const float a[3]={std::fabs(n.x),std::fabs(n.y),std::fabs(n.z)};const int axis=a[0]>=a[1]&&a[0]>=a[2]?0:a[1]>=a[2]?1:2;
  const float component=axis==0?n.x:axis==1?n.y:n.z,side=component>=0?1.f:-1.f;
  float c[3]={q.x,q.y,q.z};const int plane=int(std::lround(c[axis]*100.f));int cells[2];int k=0;
  for(int i=0;i<3;++i)if(i!=axis){cells[k]=int(std::floor(c[i]*2.f));c[i]=(cells[k]+.5f)*.5f;++k;}
  c[axis]=plane*.01f;at={c[0],c[1],c[2]};facing={axis==0?side:0.f,axis==1?side:0.f,axis==2?side:0.f};
  return (std::uint64_t(axis)<<62)|(std::uint64_t(side>0)<<61)|(std::uint64_t(std::uint32_t(plane+1000000)&0x1fffffu)<<40)|(std::uint64_t(std::uint32_t(cells[0]+100000)&0xfffffu)<<20)|std::uint64_t(std::uint32_t(cells[1]+100000)&0xfffffu);
 };
 const auto directions=sphere(48);constexpr float Reach=14.f;
 int threads=input.threads>0?input.threads:int(std::clamp(std::thread::hardware_concurrency(),2u,9u))-1;
 threads=std::max(1,threads);v.threads=threads;
 std::atomic<size_t> next{0};std::atomic<std::uint64_t> rayTotal{0};
 // A probe whose rays mostly meet the inside of solids is shut in a pocket of rock or machinery:
 // it sees nothing of the room and is treated as inside a solid (filled from its neighbours).
 std::vector<std::uint8_t> trapped(count,0);
 auto work=[&]{
  std::unordered_map<std::uint64_t,Point3> patches;patches.reserve(8192);std::uint64_t rays=0;
  for(;;){const size_t first=next.fetch_add(64);if(first>=count)break;
   for(size_t i=first;i<std::min(count,first+64);++i){if(v.valid[i]!=1)continue;
    const int x=int(i%size_t(v.nx)),y=int((i/size_t(v.nx))%size_t(v.ny)),z=int(i/(size_t(v.nx)*v.ny));const Point3 p=v.position(x,y,z);
    float light[6][3]{},weight[6]{},sky[6]{},reach=0;int inside=0;
    for(const auto&d:directions){Point3 hitNormal;++rays;const float t=w.lightRayHit(p,p+d*Reach,hitNormal);
     Point3 radiance{};bool open=false;reach+=std::min(t,1.f)*Reach;
     if(t<1.f){if(dot3(hitNormal,d)>=0||t<1e-4f)++inside;else{Point3 at,facing;const auto key=patchKey(p+d*(Reach*t),hitNormal,at,facing);
       auto found=patches.find(key);if(found==patches.end())found=patches.emplace(key,times(albedo(facing),irradiance(at,facing,rays))).first;radiance=found->second;}}
     else open=d.z>-.05f;
     for(int f=0;f<6;++f){const float c=std::max(0.f,dot3(Axes[f],d));if(c<=0)continue;weight[f]+=c;light[f][0]+=radiance.x*c;light[f][1]+=radiance.y*c;light[f][2]+=radiance.z*c;if(open)sky[f]+=c;}
    }
    // ...or wedged in a gap a hand wide (between a rock and the wall): its dark would bleed onto
    // the faces round it, so it is filled from the open probes beside it instead.
    if(inside*4>int(directions.size())||reach<.5f*float(directions.size())){trapped[i]=1;continue;}
    float* out=v.data.data()+i*IrradianceVolume::Stride;
    for(int f=0;f<6;++f){const float s=weight[f]>0?1.f/weight[f]:0.f;for(int c=0;c<3;++c)out[f*4+c]=light[f][c]*s;out[f*4+3]=outdoors?sky[f]*s:1.f;}
    // The crystals light the air round them directly too: their glow on a surface that faces them.
    for(const auto&e:input.emitters){const Point3 d=e.position-p;const float d2=dot3(d,d);if(d2>=e.range*e.range)continue;
     const float distance=std::max(std::sqrt(d2),.05f),f=crystalGlow(e,d2)/3.14159265f;
     Point3 hit;++rays;const float reach=w.lightRayHit(p,e.position,hit);if(reach<.999f&&(1.f-reach)*distance>e.radius*1.6f)continue;
     const Point3 toward=d*(1.f/distance);
     for(int k=0;k<6;++k){const float c=std::max(0.f,dot3(Axes[k],toward));out[k*4]+=e.tone.x*f*c;out[k*4+1]+=e.tone.y*f*c;out[k*4+2]+=e.tone.z*f*c;}}
   }
  }
  rayTotal+=rays;
 };
 {std::vector<std::thread> pool;for(int t=1;t<threads;++t)pool.emplace_back(work);work();for(auto&t:pool)t.join();}
 v.rays=rayTotal.load();
 for(size_t i=0;i<count;++i)if(trapped[i]){v.valid[i]=0;--v.openProbes;}
 // Probes inside solids take the mean of their open neighbours (outward until every one is filled
 // or eight cells deep), so trilinear reads at a wall or into a rock never pull black out of it.
 for(int pass=0;pass<8;++pass){
  std::vector<std::uint8_t> filled=v.valid;
  for(int z=0;z<v.nz;++z)for(int y=0;y<v.ny;++y)for(int x=0;x<v.nx;++x){const size_t i=v.index(x,y,z);if(v.valid[i])continue;
   float sum[IrradianceVolume::Stride]{};int n=0;
   for(int k=0;k<6;++k){const int ax=x+int(Axes[k].x),ay=y+int(Axes[k].y),az=z+int(Axes[k].z);
    if(ax<0||ay<0||az<0||ax>=v.nx||ay>=v.ny||az>=v.nz)continue;const size_t j=v.index(ax,ay,az);if(!v.valid[j])continue;
    const float* s=v.data.data()+j*IrradianceVolume::Stride;for(int c=0;c<IrradianceVolume::Stride;++c)sum[c]+=s[c];++n;}
   if(!n)continue;float* out=v.data.data()+i*IrradianceVolume::Stride;for(int c=0;c<IrradianceVolume::Stride;++c)out[c]=sum[c]/float(n);filled[i]=2;}
  v.valid.swap(filled);
 }
 for(size_t i=0;i<count;++i)if(!v.valid[i])for(int f=0;f<6;++f)v.data[i*IrradianceVolume::Stride+size_t(f*4+3)]=outdoors?0.f:1.f;
 // Reflection probes: one per 8 m block of open floor, at eye height where the block has the most
 // room round it, its box the walls a ray from it meets (clipped to 12 m).
 for(int by=0;by<3;++by)for(int bx=0;bx<3;++bx){
  int best=-1;float bestScore=-1;
  for(int y=by*8;y<by*8+8;++y)for(int x=bx*8;x<bx*8+8;++x){const size_t column=size_t(y*World::Width+x);if(w.tile(x,y)=='#')continue;
   const float eye=floors[column]+1.6f;const int z=std::clamp(int(std::floor((eye-v.origin.z)/v.cellZ)),0,v.nz-1);if(v.valid[v.index(x,y,z)]!=1)continue;
   int room=0;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){const int ax=x+dx,ay=y+dy;if(ax>=0&&ay>=0&&ax<v.nx&&ay<v.ny&&v.valid[v.index(ax,ay,z)]==1)++room;}
   const float centre=std::hypot(x+.5f-(bx*8+4.f),y+.5f-(by*8+4.f));const float score=room*10.f-centre;
   if(score>bestScore){bestScore=score;best=y*World::Width+x;}}
  if(best<0)continue;const int x=best%World::Width,y=best/World::Width;
  const Point3 p{x+.5f,y+.5f,floors[size_t(best)]+1.6f};ReflectionProbe probe{p,p,p};
  for(const auto&axis:Axes){Point3 n;const float t=w.lightRayHit(p,p+axis*12.f,n)*12.f;const Point3 end=p+axis*t;
   probe.lo={std::min(probe.lo.x,end.x),std::min(probe.lo.y,end.y),std::min(probe.lo.z,end.z)};probe.hi={std::max(probe.hi.x,end.x),std::max(probe.hi.y,end.y),std::max(probe.hi.z,end.z)};}
  bool duplicate=false;for(const auto&other:bake.reflections)if(p.x>other.lo.x&&p.x<other.hi.x&&p.y>other.lo.y&&p.y<other.hi.y&&std::fabs(p.z-other.position.z)<1.f&&std::hypot(p.x-other.position.x,p.y-other.position.y)<4.5f)duplicate=true;
  if(!duplicate)bake.reflections.push_back(probe);
 }
 bake.wet=bakeWetness(w,v.origin.z);
 v.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 return bake;
}

// --each-test=light-probes: every campaign map bakes in budget, no open probe sits in a solid, a
// lit room fills and an unlit one does not, a crystal tints the floor round it cyan, and two bakes
// of one map (on different thread counts) agree to the bit.
bool testLightProbes(){
 std::ofstream report("light-probes-test.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 try{
  // Two rooms side by side, a lamp in one, a crystal in a third.
  std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Probe test|0\nMAP|0|Probes|0|0|2|12|0|0|4.5|0.7|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y){std::string row(24,'#');if(y>=2&&y<=10)for(int x=1;x<23;++x)row[size_t(x)]=(x==8||x==16)?'#':'.';t+="ROW|0|0|"+std::to_string(y)+"|"+row+'\n';}
  t+="LIGHT|0|4|6|3.2|0|0|2.4|10|4200\n";
  t+="FIXTURE|0|"+std::to_string(model::res_crystal_l)+"|20|6|0|1.6|1.5|2.6|0|0\n";
  t+="--- CUSTOM_CAMPAIGN_DATA_END ---\n";
  auto campaign=loadCustomCampaignText(t,"probe-test.rmc");World room(0,campaign->maps[0]);
  ProbeBakeInput in;in.emitters=crystalEmitters(room);
  check(in.emitters.size()==1,"the crystal fixture is a probe emitter ("+std::to_string(in.emitters.size())+")");
  auto bake=bakeLightProbes(room,in);const auto& v=bake.volume;
  auto fill=[&](float x0,float x1){double sum=0;int n=0;for(float y=3.f;y<10.f;y+=1.f)for(float x=x0;x<x1;x+=1.f){auto s=v.sample({x,y,.02f},{0,0,1});sum+=s[0]*.2126+s[1]*.7152+s[2]*.0722;++n;}return n?sum/n:0.;};
  const double lit=fill(2,7.5f),dark=fill(10,15.5f);
  check(lit>.02&&dark<lit*.02,"the lamp's room fills ("+std::to_string(lit)+") and the dark room does not ("+std::to_string(dark)+")");
  {auto s=v.sample({19.f,6.f,.02f},{0,0,1});check(s[2]>s[0]*3.f&&s[1]>.01f,"the floor beside the crystal is lit cold cyan ("+std::to_string(s[0])+", "+std::to_string(s[1])+", "+std::to_string(s[2])+")");}
  ProbeBakeInput single=in;single.threads=1;auto again=bakeLightProbes(room,single);
  check(again.volume.data==v.data&&again.volume.valid==v.valid,"two bakes on "+std::to_string(v.threads)+" and 1 threads agree exactly");
  // Every campaign map.
  double worst=0,total=0;int maps=0,inside=0,reflections=0;std::string slowest;
  for(int level=0;level<CampaignChunkCount;++level){
   World w(level,WorldId::Campaign);ProbeBakeInput input;input.emitters=crystalEmitters(w);
   auto b=bakeLightProbes(w,input);++maps;total+=b.volume.milliseconds;reflections+=int(b.reflections.size());
   if(b.volume.milliseconds>worst){worst=b.volume.milliseconds;slowest=std::to_string(level);}
   for(int z=0;z<b.volume.nz;++z)for(int y=0;y<b.volume.ny;++y)for(int x=0;x<b.volume.nx;++x)if(b.volume.valid[b.volume.index(x,y,z)]==1){
    const Point3 p=b.volume.position(x,y,z);if(w.insideLightSolid(p)||w.tile(x,y)=='#'){++inside;if(inside<5)report<<"  probe in a solid: map "<<level<<" at "<<p.x<<','<<p.y<<','<<p.z<<'\n';}}
   report<<"  map "<<level<<": "<<b.volume.milliseconds<<" ms, "<<b.volume.openProbes<<" open probes of "<<b.volume.valid.size()<<", "<<b.volume.rays<<" rays, "<<b.reflections.size()<<" reflection probes\n";
  }
  check(maps==CampaignChunkCount&&worst<500.,"all "+std::to_string(maps)+" campaign maps bake, slowest "+std::to_string(int(worst))+" ms (map "+slowest+"), mean "+std::to_string(int(total/std::max(1,maps)))+" ms");
  check(inside==0,"no open probe stands inside a wall or solid ("+std::to_string(inside)+")");
  check(reflections>=maps,"reflection probes placed ("+std::to_string(reflections)+")");
 }catch(const std::exception& e){check(false,std::string("error: ")+e.what());}
 return ok;
}
}

// ── The renderer's per-map probe cache ─────────────────────────────────────
#include "renderer/SoftwareRenderer.h"
#include "renderer/GpuRenderer.h"
namespace retro {
namespace {bool g_lightProbes=true;
// Rebake: a salt in every map's key, so each bakes again; and what the bakes that landed took.
unsigned g_probeSalt=0,g_probeBakes=0;double g_probeBakeMs=0;}
void SoftwareRenderer::setLightProbes(bool enabled){g_lightProbes=enabled;}
bool SoftwareRenderer::lightProbes(){return g_lightProbes;}
void SoftwareRenderer::rebakeLightProbes(){++g_probeSalt;}
unsigned SoftwareRenderer::lightProbeBakes(){return g_probeBakes;}
double SoftwareRenderer::lastLightProbeBakeMs(){return g_probeBakeMs;}
const ProbeBake* SoftwareRenderer::chunkProbes(const Game& game,int level,const World& w,std::uint64_t lightKey,bool wait){
 if(!g_lightProbes||level<0||level>=Game::MaxChunks)return nullptr;
 auto& entry=m_chunkProbes[size_t(level)];
 ProbeBakeInput input;input.lampPower.assign(w.lights().size(),1.f);
 std::uint64_t key=1469598103934665603ull;auto mix=[&](std::uint64_t value){key^=value;key*=1099511628211ull;};
 mix(lightKey);mix(game.staticRevision(level));mix(std::uint64_t(game.worldId()));mix(std::uint64_t(level));
 mix(std::uint64_t(reinterpret_cast<std::uintptr_t>(w.definitionIdentity().map)));mix(w.fixtures().size());mix(g_probeSalt);
 // Switchable lamps (a breaker, a timer, a failing tube) stay out of the bake: their power changes
 // in play, so each keeps its own flat room bounce at this frame's power (scene.frag, Scene3D).
 for(size_t i=0;i<w.lights().size();++i)if(w.lights()[i].switchable())input.lampPower[i]=0.f;
 auto collect=[&]{
  try{entry.bake=entry.pending.get();if(entry.bake){++g_probeBakes;g_probeBakeMs=entry.bake->volume.milliseconds;}}catch(const std::exception& e){std::ofstream("RawMetal-renderer-events.txt",std::ios::app)<<"Light probe bake failed on map "<<level<<": "<<e.what()<<'\n';}
  entry.pending={};entry.key=entry.pendingKey;++m_probeBakesTaken;
  // The CPU rasteriser's vertex light carries the fill: its cache for this map goes.
  m_chunkLighting[size_t(level)].clear();
 };
 if(entry.pending.valid()&&entry.pending.wait_for(std::chrono::seconds(0))==std::future_status::ready)collect();
 if(entry.key!=key&&!entry.pending.valid()){
  // The bake reads a copy of the map, so play (doors, crates, a chunk streamed out) never races it.
  auto copy=std::make_shared<const World>(w);input.emitters=crystalEmitters(w);
  // What the map's own finishes return of the light on them: their mean colour, kept in a sane range.
  auto mean=[](const Texture* t,Point3 fallback){if(!t||t->pixels.empty())return fallback;double c[3]{};
   for(auto p:t->pixels)for(int k=0;k<3;++k)c[k]+=std::pow(((p>>(16-8*k))&255)/255.,2.2);
   Point3 m{float(c[0]/t->pixels.size()),float(c[1]/t->pixels.size()),float(c[2]/t->pixels.size())};
   // Half its colour only: a room's mean finish (a teal-banded clinic, rust) bounced and then lit on
   // that same finish again would wash every face in it; the lamps' own tones carry the colour.
   const float luma=m.x*.2126f+m.y*.7152f+m.z*.0722f;m=Point3{luma,luma,luma}+(m-Point3{luma,luma,luma})*.5f;return luma>1e-4f?m*(std::clamp(luma,.16f,.42f)/luma):fallback;};
  ProbeSurfaces defaults;
  if(auto*set=surfaceSet(w.floorSurface()))input.surfaces.floor=mean(set->floor,defaults.floor);
  if(auto*set=surfaceSet(w.wallSurface()))input.surfaces.wall=mean(set->wall,defaults.wall);
  if(auto*set=surfaceSet(w.ceilingSurface()))input.surfaces.ceiling=mean(set->ceiling,defaults.ceiling);
  entry.pendingKey=key;
  entry.pending=std::async(std::launch::async,[copy,input]{return std::make_shared<const ProbeBake>(bakeLightProbes(*copy,input));}).share();
 }
 // In play only a map's first bake is waited for (a re-bake comes in when it is done, never as a
 // hitch); an inspection frame (a test, a shot) waits for the bake of what it is shown, so the
 // same moment asked twice draws with the same light.
 if(entry.pending.valid()&&((wait&&!entry.bake)||m_environmentInspection)){entry.pending.wait();collect();}
 return entry.bake.get();
}
}
