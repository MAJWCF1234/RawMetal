#include "SoftwareRenderer.h"
#include "GpuRenderer.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace retro {
bool SoftwareRenderer::testSurfaceSets(std::string* report){
 std::ostringstream out;bool ok=true;
 auto check=[&](bool pass,const std::string& label){out<<(pass?"PASS ":"FAIL ")<<label<<'\n';ok&=pass;};
 auto storage=std::make_unique<SoftwareRenderer>(320,180);auto& renderer=*storage;
 auto loaded=[](const Texture* t){return t&&t->width>0&&t->height>0&&t->pixels.size()==size_t(t->width)*size_t(t->height)&&(t->normalLevels.empty()||t->normalLevels.size()==t->mips.size()+1);};
 for(size_t i=0;i<renderer.m_surfaceTextures.size();++i){const auto& t=renderer.m_surfaceTextures[i];
  check(loaded(&t)&&t.width==256&&t.height==256&&t.mips.size()==8&&t.normalLevels.size()==9,"finish "+std::to_string(i<25?2100+i:2150+i-25));}
 for(int s=0;s<SurfaceCount;++s){const auto* set=renderer.surfaceSet(s);
  check(set&&loaded(set->wall)&&loaded(set->floor)&&loaded(set->ceiling)&&set->wallRepeat.x>0&&set->wallRepeat.y>0&&set->floorRepeat>0&&set->ceilingRepeat>0,"surface "+std::to_string(s));}
 check(!renderer.surfaceSet(SurfaceDefault)&&!renderer.surfaceSet(SurfaceCount),"default and invalid surfaces retain legacy selection");
 const int materials[]={26,27,28,29,31,32,33,34,35,36,37,38,39};
 check(renderer.finishMaterial(36)->band>0&&renderer.finishMaterial(36)->above&&renderer.finishMaterial(36)->above->band==0,"dado bands carry their height and the paint above");
 for(int material:materials){const auto* t=renderer.finishMaterial(material);check(loaded(t)&&renderer.finishRepeat(*t).x>0,"structure material "+std::to_string(material));}
 check(!renderer.finishMaterial(30)&&!renderer.finishMaterial(25),"collision proxy and molten metal retain their behavior");
 auto glowing=[](const Texture& t){return t.emission.size()==t.pixels.size()&&std::any_of(t.emission.begin(),t.emission.end(),[](auto p){return (p&0xffffffu)!=0;});};
 check(glowing(*renderer.finishMaterial(35))&&renderer.finishMaterial(34)->emission.empty()&&glowing(*renderer.surfaceSet(SurfaceLabPanel)->ceiling),"crystal and lab diffuser have nonzero emission; growth is unlit");

 // Test-only room: identical camera, geometry and lighting for every finish.
 // No campaign builder or purchased source image is inspected or modified.
 std::string records="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Surface evidence|0\nMAP|0|Room|0|0|5|6|0|0|3.4|0.3||0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int y=0;y<24;++y){std::string row(24,'#');if(y>=2&&y<=9)for(int x=2;x<=10;++x)row[x]='.';records+="ROW|0|0|"+std::to_string(y)+"|"+row+"\n";}
 records+="LIGHT|0|6|6|3.1|2|0|1.2|10\n";
 auto scene=[&](int surface,int material){
  auto text=records+"SURFACE|0|"+std::to_string(surface)+"|"+std::to_string(surface)+"|"+std::to_string(surface)+"\n";
  if(material>=0)text+="STRUCT|0|7|4.8|8|7.2|0|2.7|0|"+std::to_string(material)+"\n";
  return Game(loadCustomCampaignText(text+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","surface-evidence.txt"));
 };
 renderer.setEnvironmentInspection(true);
 std::filesystem::create_directories(".build/surface-evidence");
 auto measure=[&](const std::string& name){
  std::ofstream image(".build/surface-evidence/"+name+".ppm",std::ios::binary);image<<"P6\n320 180\n255\n";
  double sum=0,squares=0;size_t black=0,white=0;uint64_t hash=1469598103934665603ull;
  for(auto p:renderer.m_pixels){double y=.2126*((p>>16)&255)+.7152*((p>>8)&255)+.0722*(p&255);sum+=y;squares+=y*y;black+=y<1;white+=y>254;hash^=p;hash*=1099511628211ull;char rgb[]={char(p>>16),char(p>>8),char(p)};image.write(rgb,3);}
  double mean=sum/renderer.m_pixels.size(),sd=std::sqrt(std::max(0.,squares/renderer.m_pixels.size()-mean*mean));
  out<<name<<" mean="<<mean<<" sd="<<sd<<" black="<<black<<" white="<<white<<" hash="<<hash<<'\n';
  check(image.good()&&mean>1&&mean<254&&sd>1&&black<renderer.m_pixels.size()/2&&white<renderer.m_pixels.size()/2,name+" has finite, varied, non-clipped frame output");
  return hash;
 };
 for(bool gpu:{false,true}){
  if(gpu){bool active=renderer.enableHardware();check(active,"Vulkan headless initialization");if(!active)break;}
  const std::string path=gpu?"gpu-":"cpu-";
  std::set<uint64_t> surfaces,structures;
  // Reassign the same Game address to exercise cache invalidation on a finish change.
  auto room=scene(SurfaceDefault,-1);
  for(int s=SurfaceDefault;s<SurfaceCount;++s){room=scene(s,-1);renderer.render(room);
   if(gpu){for(int frame=0;frame<16&&renderer.m_gpu->shadowsPending();++frame)renderer.render(room);auto builds=renderer.staticGeometryBuilds();renderer.render(room);check(renderer.hardwareActive()&&builds==renderer.staticGeometryBuilds(),"surface static geometry reused "+std::to_string(s));}
   surfaces.insert(measure(path+"surface-"+std::to_string(s)));
  }
  check(surfaces.size()==SurfaceCount+1,path+"all surface sets and legacy produce distinct frames");
  for(int material:materials){room=scene(SurfaceConcrete,material);renderer.render(room);
   if(gpu){for(int frame=0;frame<16&&renderer.m_gpu->shadowsPending();++frame)renderer.render(room);renderer.render(room);check(renderer.hardwareActive(),"Vulkan structure frame "+std::to_string(material));}
   structures.insert(measure(path+"material-"+std::to_string(material)));
  }
  check(structures.size()==std::size(materials),path+"every new structure material produces a distinct frame");
  if(!gpu){
   // Same crystal, same dark-room lighting: only emission changes. This proves
   // it contributes to rasterized output instead of merely existing in memory.
   room=scene(SurfaceConcrete,35);renderer.render(room);auto lit=renderer.m_pixels;
   auto* crystal=const_cast<Texture*>(renderer.finishMaterial(35));auto emission=std::move(crystal->emission);
   renderer.render(room);double gain=0;size_t brighter=0;
   for(size_t i=0;i<lit.size();++i)for(int shift:{0,8,16}){int delta=int((lit[i]>>shift)&255)-int((renderer.m_pixels[i]>>shift)&255);gain+=delta;brighter+=delta>0;}
   crystal->emission=std::move(emission);
   out<<"crystal emission channel gain="<<gain<<" brighter channels="<<brighter<<'\n';
   check(gain>100&&brighter>100,"crystal emission measurably illuminates its rendered facets");
  }
 }
 if(report)*report=out.str();
 std::ofstream("surface-sets-test.txt")<<out.str();
 return ok;
}
}
