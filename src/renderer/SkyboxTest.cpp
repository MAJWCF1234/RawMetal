#include "SoftwareRenderer.h"
#include "../game/EngineRegressionScene.h"
#include "../world/SkyboxCatalog.h"
#include "../lab/LabAccess.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

namespace retro {
namespace {
std::uint64_t skyHash(const SoftwareRenderer& renderer){
 std::uint64_t hash=1469598103934665603ull;
 for(int y=0;y<renderer.height()/2;++y)for(int x=0;x<renderer.width();++x){hash^=renderer.pixels()[size_t(y)*renderer.width()+x];hash*=1099511628211ull;}
 return hash;
}
void skyPicture(const SoftwareRenderer& renderer,const std::filesystem::path& path){
 std::ofstream out(path,std::ios::binary);out<<"P6\n"<<renderer.width()<<' '<<renderer.height()<<"\n255\n";
 for(int i=0;i<renderer.width()*renderer.height();++i){const auto p=renderer.pixels()[i];const char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
}
}
// The skies tools/library/make_skies.py adds (resources 2900+), each with its own air.
constexpr const char* extraSkies[]={"psx_clear_day","grey_overcast","cold_dawn","dusk_afterglow","storm_front","psx_starry_night","cloudy_night","treeline_night","ember_eclipse","resonance_aurora"};
// Every paid sky is decoded through the same loader and selected through MAP.skybox.
bool testSkyboxes(){
 std::ofstream report("skyboxes-test.txt");bool passed=true;auto check=[&](bool ok,const std::string& label){report<<(ok?"PASS ":"FAIL ")<<label<<'\n';report.flush();passed&=ok;};
 auto map=regression::room();map->definition.environment=Environment::Outdoor;map->skybox="brutal_wasteland";
 Game game(regression::campaign(map));auto camera=game.player();camera.pos={12,12};camera.z=12;camera.pitch=45;camera.angle=.3f;game.restoreEditorCamera(camera);
 auto gpu=std::make_unique<SoftwareRenderer>(320,180);gpu->setEnvironmentInspection(true);
 if(!gpu->enableHardware()){check(false,"Vulkan renderer is available for native sky switching");return false;}
 const bool post=SoftwareRenderer::postProcessing(),taa=SoftwareRenderer::temporalAA();SoftwareRenderer::setPostProcessing(false);SoftwareRenderer::setTemporalAA(false);
 struct Restore {bool post,taa;~Restore(){SoftwareRenderer::setPostProcessing(post);SoftwareRenderer::setTemporalAA(taa);}}restore{post,taa};
 // Terrain settle copies the authored map into the live World; switch skies on
 // that owned copy so the renderer sees MAP.skybox without rebuilding terrain.
 LabAccess::setSkybox(game,"unregistered_sky");gpu->render(game);const auto fallback=skyHash(*gpu);
 const char* directory=std::getenv("RM_SKYBOX_FRAMES");if(directory)std::filesystem::create_directories(directory);
 std::set<int> resources;int registered=0;std::uint64_t first=0;
 for(const auto& sky:SkyboxCatalog)if(sky.resource>=2800){
  ++registered;check(resources.insert(sky.resource).second,"unique resource for "+std::string(sky.id));
  const auto texture=LabAccess::skyTexture(sky.resource);
  check(texture.width==2048&&texture.height==512,"embedded 2048x512 neural sky decodes: "+std::string(sky.id));
  map->skybox=std::string(sky.id);World rebuilt(0,map);check(rebuilt.hasTerrain(),"changing sky preserves outdoor terrain: "+map->skybox);
  LabAccess::setSkybox(game,std::string(sky.id));gpu->render(game);const auto hash=skyHash(*gpu);check(hash!=fallback,"native game draws selected paid sky: "+std::string(sky.id));
  if(!first)first=hash;
  if(directory)skyPicture(*gpu,std::filesystem::path(directory)/(std::string(sky.id)+".ppm"));
 }
 check(registered==47+int(std::size(extraSkies)),"all 47 Brutal skies and the "+std::to_string(std::size(extraSkies))+" make_skies.py skies are registered");
 LabAccess::setSkybox(game,"skybox_emx_1");gpu->render(game);check(skyHash(*gpu)==first,"switching back restores the original selected sky");
 for(const char* id:extraSkies){const auto* sky=findSkybox(id);check(sky&&sky->resource>=2900&&sky->terrain&&sky->hazeDensity>0,std::string("make_skies.py sky in the catalog with its own air: ")+id);
  if(sky){map->skybox=id;World own(0,map);const auto haze=own.haze({12,12},own.floorHeight(12,12)+1);check(std::fabs(haze[3]-sky->hazeDensity)<1e-6f,std::string("an outdoor map with no haze of its own takes the sky's: ")+id);
   // Its light: the sun (or moon) where and as bright as the sky says, the flat fill its own, scaled by the MAP ambient.
   const auto sun=own.sun();const float scale=std::clamp(map->definition.ambient/.3f,.25f,3.f);
   check(std::fabs(sun.intensity-sky->sunIntensity)<1e-5f&&std::fabs(sun.r-sky->sunR)<1e-5f&&std::fabs(sun.b-sky->sunB)<1e-5f&&sun.direction.z>0,std::string("the sky sets the outdoor sun: ")+id);
   check(std::fabs(own.outdoorAmbient()-sky->ambient*scale)<1e-5f&&std::fabs(own.skyAmbientTone().z-sky->ambientB)<1e-5f,std::string("the sky sets the outdoor fill and its colour: ")+id);}}
 {// Night is dark and blue, day bright; the map's own ambient still turns the fill.
  auto lit=[&](const char* id,float ambient){auto copy=std::make_shared<std::remove_cvref_t<decltype(*map)>>(*map);copy->skybox=id;copy->definition.ambient=ambient;World own(0,copy);return std::pair{own.sun(),own.outdoorAmbient()};};
  const auto day=lit("psx_clear_day",.3f),night=lit("psx_starry_night",.3f),dusk=lit("dusk_afterglow",.3f),dim=lit("psx_starry_night",.15f);
  check(night.first.intensity<day.first.intensity*.3f&&night.second<day.second*.5f&&night.first.b>night.first.r,"Starry night is dark and blue under the moon, Clear day bright");
  {auto copy=std::make_shared<std::remove_cvref_t<decltype(*map)>>(*map);copy->skybox="psx_starry_night";World own(0,copy);copy->skybox="psx_clear_day";World sunny(0,copy);
   check(own.skyExposure()<=.6f&&sunny.skyExposure()==1.f,"auto exposure may not open a night back up to day");}
  check(dusk.first.direction.z<.1f&&dusk.first.r>dusk.first.b,"Dusk afterglow lights low and warm");
  check(std::fabs(dim.second-night.second*.5f)<1e-5f,"a map's own ambient (0.15 against 0.3) halves the sky's fill");
  auto plain=std::make_shared<std::remove_cvref_t<decltype(*map)>>(*map);plain->skybox="brutal_wasteland";World old(0,plain);
  check(std::fabs(old.outdoorAmbient()-.26f)<1e-6f&&std::fabs(old.sun().intensity-2.1f)<1e-6f,"a sky without its own light keeps the old daylight");}
 const auto uploaded=gpu->atlasUploadStatistics();gpu->render(game);check(gpu->atlasUploadStatistics()==uploaded,"an unchanged sky does not upload a texture every frame");
 auto cpu=std::make_unique<SoftwareRenderer>(320,180);cpu->setEnvironmentInspection(true);cpu->render(game);const auto softwareFirst=skyHash(*cpu);
 LabAccess::setSkybox(game,"skybox_plain_emx_14");cpu->render(game);check(skyHash(*cpu)!=softwareFirst,"the software renderer also switches sky textures");
 if(directory)skyPicture(*cpu,std::filesystem::path(directory)/"software-skybox_plain_emx_14.ppm");
 report<<(passed?"ALL PASSED":"FAILED")<<" skyboxes\n";return passed;
}
}
