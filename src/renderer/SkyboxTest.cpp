#include "SoftwareRenderer.h"
#include "../game/EngineRegressionScene.h"
#include "../world/SkyboxCatalog.h"
#include "../lab/LabAccess.h"
#include <filesystem>
#include <fstream>
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
  if(sky){map->skybox=id;World own(0,map);const auto haze=own.haze({12,12},own.floorHeight(12,12)+1);check(std::fabs(haze[3]-sky->hazeDensity)<1e-6f,std::string("an outdoor map with no haze of its own takes the sky's: ")+id);}}
 const auto uploaded=gpu->atlasUploadStatistics();gpu->render(game);check(gpu->atlasUploadStatistics()==uploaded,"an unchanged sky does not upload a texture every frame");
 auto cpu=std::make_unique<SoftwareRenderer>(320,180);cpu->setEnvironmentInspection(true);cpu->render(game);const auto softwareFirst=skyHash(*cpu);
 LabAccess::setSkybox(game,"skybox_plain_emx_14");cpu->render(game);check(skyHash(*cpu)!=softwareFirst,"the software renderer also switches sky textures");
 if(directory)skyPicture(*cpu,std::filesystem::path(directory)/"software-skybox_plain_emx_14.ppm");
 report<<(passed?"ALL PASSED":"FAILED")<<" skyboxes\n";return passed;
}
}
