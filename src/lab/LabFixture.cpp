#include "lab/LabFixture.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace fs=std::filesystem;
namespace retro::lab {
std::string labRoot(){
 if(const char* env=std::getenv("RM_LAB_ROOT"))return env;
 return RAWMETAL_LAB_ROOT;
}
static std::vector<std::string> words(const std::string& line){
 std::vector<std::string> out;std::string cur;bool quoted=false,any=false;
 for(char c:line){if(c=='"'){quoted=!quoted;any=true;continue;}
  if(!quoted&&(c==' '||c=='\t'||c=='\r')){if(any||!cur.empty())out.push_back(cur);cur.clear();any=false;continue;}
  if(!quoted&&c=='#')break;cur+=c;}
 if(any||!cur.empty())out.push_back(cur);return out;
}
Fixture loadFixture(const std::string& path){
 std::ifstream in(path);if(!in)throw std::runtime_error("lab: no fixture "+path);
 Fixture f;f.path=path;f.name=fs::path(path).stem().string();
 std::string line;int number=0;bool inCampaign=false;
 auto fail=[&](const std::string& why){throw std::runtime_error(path+":"+std::to_string(number)+": "+why);};
 auto num=[&](const std::vector<std::string>& w,size_t i,float fallback){if(i>=w.size())return fallback;try{return std::stof(w[i]);}catch(...){fail("not a number: "+w[i]);}return fallback;};
 while(std::getline(in,line)){++number;
  if(inCampaign){if(line.rfind("end-campaign",0)==0){inCampaign=false;continue;}f.campaignText+=line+"\n";continue;}
  const auto w=words(line);if(w.empty())continue;const auto& d=w[0];
  if(d=="world"){if(w.size()<2)fail("world what?");f.world=w[1];if(f.world=="custom"){if(w.size()<3)fail("world custom \"path\"");f.customPath=w[2];}}
  else if(d=="map")f.map=int(num(w,1,0));
  else if(d=="camera"){if(w.size()<6)fail("camera X Y Z YAW PITCH");f.hasCamera=true;f.cameraPos={num(w,1,0),num(w,2,0)};f.cameraZ=num(w,3,-999);f.cameraYaw=num(w,4,0)*0.01745329f;f.cameraPitch=num(w,5,0);}
  else if(d=="open-doors")f.openDoors=true;
  else if(d=="scenery-only")f.sceneryOnly=true;
  else if(d=="software")f.software=true;
  else if(d=="save"){if(w.size()<2)fail("save FILE");f.save=(fs::path(path).parent_path()/w[1]).string();}
  else if(d=="state"){if(w.size()<3)fail("state NAME VALUE");f.states.push_back({stateId(w[1]),int(num(w,2,1))});}
  else if(d=="state-id"){if(w.size()<3)fail("state-id 0xHEX VALUE");f.states.push_back({StateId(std::stoul(w[1],nullptr,0)),int(num(w,2,1))});}
  else if(d=="spawn"){if(w.size()<4)fail("spawn creature KIND X Y | spawn guard X Y");Fixture::Spawn s;s.what=w[1];
   if(s.what=="creature"){if(w.size()<5)fail("spawn creature KIND X Y [Z] [YAW]");s.kind=w[2];s.at={num(w,3,0),num(w,4,0)};s.z=num(w,5,-999);s.yaw=num(w,6,0)*0.01745329f;}
   else if(s.what=="guard"){s.at={num(w,2,0),num(w,3,0)};s.yaw=num(w,4,0)*0.01745329f;}
   else fail("spawn creature|guard");f.spawns.push_back(s);}
  else if(d=="light"){if(w.size()<4)fail("light X Y Z [INTENSITY] [RANGE]");f.lamps.push_back({{num(w,1,0),num(w,2,0),num(w,3,3)},num(w,4,1),num(w,5,8)});}
  else if(d=="size"){f.width=int(num(w,1,480));f.height=int(num(w,2,270));}
  else if(d=="frames")f.frames=int(num(w,1,0));
  else if(d=="step")f.step=num(w,1,0);
  else if(d=="begin-campaign"){inCampaign=true;f.world="text";}
  else{std::string rest;for(size_t i=1;i<w.size();++i)rest+=(i>1?" ":"")+w[i];f.values[d]=rest;}
 }
 if(inCampaign)fail("begin-campaign without end-campaign");
 return f;
}
std::vector<std::string> findFixtures(const std::string& pattern){
 std::vector<std::string> out;const fs::path dir=fs::path(labRoot())/"lab"/"fixtures";
 auto glob=[&](const fs::path& folder,const std::string& mask){
  std::string rx;for(char c:mask){if(c=='*')rx+=".*";else if(c=='?')rx+='.';else if(std::string(".()[]{}+^$|\\").find(c)!=std::string::npos){rx+='\\';rx+=c;}else rx+=c;}
  const std::regex re(rx,std::regex::icase);std::error_code ec;
  for(const auto& e:fs::directory_iterator(folder,ec))if(e.path().extension()==".lab"&&std::regex_match(e.path().filename().string(),re))out.push_back(e.path().string());};
 std::stringstream all(pattern);std::string item; // a comma-separated list of names, paths and globs
 while(std::getline(all,item,',')){if(item.empty())continue;
  if(item.find_first_of("*?")!=std::string::npos){const fs::path p(item);glob(p.has_parent_path()?p.parent_path():dir,p.filename().string());if(!p.has_parent_path()&&p.extension()!=".lab")glob(dir,p.filename().string()+".lab");continue;}
  std::error_code ec;if(fs::exists(item,ec)){out.push_back(item);continue;}
  const fs::path named=dir/(item+".lab");if(fs::exists(named,ec))out.push_back(named.string());else throw std::runtime_error("lab: no fixture named "+item+" ("+named.string()+")");}
 std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
}
