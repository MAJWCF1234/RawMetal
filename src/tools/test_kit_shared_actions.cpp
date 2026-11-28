// Native regression: compact Kit rigs must preserve the accepted animated FBXs.
// Usage: test_kit_shared_actions SOURCE_DIR BASELINE_DIR
#include "renderer/Mesh.h"
#include "core/PackedResource.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace fs=std::filesystem;
namespace {
fs::path source,baseline;
bool original=false;
const std::map<int,const char*> files={{111,"claire-arms.fbx"},{2313,"claire-fp-shotgun.fbx"},{3359,"claire-fp-mac11.fbx"},{3393,"claire-fp-rifle.fbx"},{3417,"kit-arms.fbx"},{3418,"kit-shotgun.fbx"},{3419,"kit-mac11.fbx"},{3420,"kit-rifle.fbx"}};
std::vector<unsigned char> load(int id){
 auto path=original&&id>=3417?baseline/files.at(id):source/"assets/library/player"/files.at(id);
 std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing fixture "+path.string());
 return {std::istreambuf_iterator<char>(f),{}};
}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
double compare(const retro::Mesh&a,const retro::Mesh&b){
 check(a.materialNames==b.materialNames,"material names changed");check(a.triangles.size()==b.triangles.size(),"topology changed");
 double error=0;
 for(size_t i=0;i<a.triangles.size();++i){
  check(a.triangles[i].part==b.triangles[i].part,"material assignment changed");
  for(int j=0;j<3;++j){auto x=a.triangles[i].v[j],y=b.triangles[i].v[j];
   check(x.u==y.u&&x.v==y.v,"UV coordinates changed");
   auto d=x.p-y.p;double length=std::sqrt(double(d.x)*d.x+double(d.y)*d.y+double(d.z)*d.z);
   check(std::isfinite(length),"nonfinite shared pose");error=std::max(error,length);
  }
 }
 return error;
}
struct Rig {int id,donor;std::vector<const char*> actions;};
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("Usage: test_kit_shared_actions SOURCE_DIR BASELINE_DIR");
 source=argv[1];baseline=argv[2];retro::g_looseResourceLoader=&load;
 std::vector<Rig> rigs={
 {3417,111,{"finger_gun_broken","finger_gun_fire","finger_gun_fix","finger_gun_idle","grab.L","grab.R","guard_draw","guard_idle","jab.L","jab.R","knife_draw","knife_hit_01","knife_hit_02","knife_idle","push.L","push.R","relax","rest"}},
 {3418,2313,{"aim","crouch","draw","fire","fire_aim","hide","idle","reload_end","reload_start","reload_step","run","walk","walk_aim"}},
 {3419,3359,{"aim","draw","idle","reload","run","walk","walk_aim"}},
 {3420,3393,{"aim","aim_end","aim_start","bolt","draw","fire","fire_aim","idle","reload","run","walk","walk_aim"}}};
 unsigned poses=0;
 for(const auto&rig:rigs){
  original=true;retro::Mesh accepted(rig.id);original=false;
  retro::Mesh compact(rig.id),donor(rig.donor);
  check(!compact.poseAction(rig.actions.front(),.5f),"compact FBX still duplicates animations");
  compact.shareActionsFrom(donor);
  check(compact.sharedActionResource()==rig.donor,"wrong shared rig");
  bool rejected=false;try{compact.shareActionsFrom(donor);}catch(const std::runtime_error&){rejected=true;}
  check(rejected,"repeated shared-action attachment accepted");
  rejected=false;try{accepted.shareActionsFrom(donor);}catch(const std::runtime_error&){rejected=true;}
  check(rejected,"animated FBX accepted as compact input");
  double error=compare(accepted,compact);
  for(auto action:rig.actions)for(int sample=0;sample<=32;++sample){
   float phase=float(sample)/32;
   check(accepted.poseAction(action,phase),"accepted action missing");check(compact.poseAction(action,phase),"shared action missing");
   error=std::max(error,compare(accepted,compact));++poses;
  }
  check(!compact.poseAction("this-action-does-not-exist",.5f),"missing action falsely accepted");
  check(error<.00025,"shared action deviates more than 0.25 mm from accepted geometry");
  if(rig.id==3417){for(int i=0;i<6;++i){accepted.poseAttached({-.03f,1.56f,.24f},{-.025f,1.61f,.50f},.02f,.03f,.05f,i*.5f,.3f);compact.poseAttached({-.03f,1.56f,.24f},{-.025f,1.61f,.50f},.02f,.03f,.05f,i*.5f,.3f);
    check(compare(accepted,compact)<.00001,"procedural hands or sleeves changed");}}
  std::cout<<"PASS "<<rig.id<<": "<<rig.actions.size()<<" actions; maximum vertex difference "<<error*1000<<" mm; exact topology, UVs and materials\n";
 }
 // The retained animation scene must remain valid after the donor Mesh is destroyed.
 retro::Mesh compact(3417);
 {retro::Mesh donor(111);compact.shareActionsFrom(donor);}
 check(compact.poseAction("guard_idle",.5f),"shared action scene did not retain its lifetime");
 std::cout<<"PASS shared scene lifetime and validation; "<<poses<<" sampled action poses\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
