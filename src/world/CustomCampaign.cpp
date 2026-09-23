#include "CustomCampaign.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace retro {
namespace {
constexpr std::string_view DataStart="--- CUSTOM_CAMPAIGN_DATA_START ---";
constexpr std::string_view DataEnd="--- CUSTOM_CAMPAIGN_DATA_END ---";

std::string readText(const std::filesystem::path& path){
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("cannot open custom campaign");
 file.seekg(0,std::ios::end);auto size=file.tellg();if(size<0||size>4*1024*1024)throw std::runtime_error("custom campaign is empty or larger than 4 MB");
 file.seekg(0);std::string text(size_t(size),'\0');if(!text.empty()&&!file.read(text.data(),std::streamsize(text.size())))throw std::runtime_error("cannot read custom campaign");
 return text;
}
std::vector<std::string> fields(std::string_view line){
 std::vector<std::string> out;size_t start=0;
 for(size_t i=0;i<=line.size();++i)if(i==line.size()||line[i]=='|'){out.emplace_back(line.substr(start,i-start));start=i+1;}
 return out;
}
std::string decode(std::string_view value){
 auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return 10+c-'a';if(c>='A'&&c<='F')return 10+c-'A';return -1;};
 std::string out;out.reserve(value.size());
 for(size_t i=0;i<value.size();++i){
  if(value[i]=='%'&&i+2<value.size()){int a=hex(value[i+1]),b=hex(value[i+2]);if(a>=0&&b>=0){out.push_back(char(a*16+b));i+=2;continue;}}
  out.push_back(value[i]);
 }return out;
}
int integer(const std::vector<std::string>& f,size_t index,const char* label){
 if(index>=f.size())throw std::runtime_error(std::string("missing ")+label);size_t used=0;int value=0;
 try{value=std::stoi(f[index],&used);}catch(...){throw std::runtime_error(std::string("invalid ")+label);}
 if(used!=f[index].size())throw std::runtime_error(std::string("invalid ")+label);return value;
}
float number(const std::vector<std::string>& f,size_t index,const char* label){
 if(index>=f.size())throw std::runtime_error(std::string("missing ")+label);size_t used=0;float value=0;
 try{value=std::stof(f[index],&used);}catch(...){throw std::runtime_error(std::string("invalid ")+label);}
 if(used!=f[index].size()||!std::isfinite(value)||std::fabs(value)>100000)throw std::runtime_error(std::string("invalid ")+label);return value;
}
bool boolean(const std::vector<std::string>& f,size_t index,const char* label){
 int v=integer(f,index,label);if(v!=0&&v!=1)throw std::runtime_error(std::string("invalid ")+label);return v!=0;
}
std::uint64_t campaignKey(std::string value){
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
 std::uint64_t h=1469598103934665603ull;for(unsigned char c:value){h^=c;h*=1099511628211ull;}return h;
}
std::shared_ptr<CustomMapData> mapAt(std::vector<std::shared_ptr<CustomMapData>>& maps,const std::vector<std::string>& f,size_t field=1){
 int index=integer(f,field,"map index");if(index<0||index>=WorldChunkCapacity)throw std::runtime_error("custom campaign map index outside engine capacity");
 if(size_t(index)>=maps.size())maps.resize(size_t(index)+1);
 if(!maps[size_t(index)])maps[size_t(index)]=std::make_shared<CustomMapData>();
 return maps[size_t(index)];
}
CustomLayerData& layerAt(std::vector<std::shared_ptr<CustomMapData>>& maps,const std::vector<std::string>& f){
 auto map=mapAt(maps,f,1);int layer=integer(f,2,"layer index");if(layer<0||layer>31)throw std::runtime_error("invalid custom layer index");
 if(size_t(layer)>=map->layers.size())map->layers.resize(size_t(layer)+1);return map->layers[size_t(layer)];
}
}

std::shared_ptr<const CustomCampaign> loadCustomCampaignFile(const std::filesystem::path& path){
 auto text=readText(path);auto start=text.find(DataStart),end=text.find(DataEnd);
 if(start==std::string::npos||end==std::string::npos||end<=start)throw std::runtime_error("missing CUSTOM_CAMPAIGN_DATA markers");
 start+=DataStart.size();std::istringstream stream(text.substr(start,end-start));std::string line;
 auto campaign=std::make_shared<CustomCampaign>();campaign->sourceFile=path.filename().string();campaign->key=campaignKey(campaign->sourceFile);
 std::vector<std::shared_ptr<CustomMapData>> maps;bool sawCampaign=false;
 while(std::getline(stream,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
  auto f=fields(line);if(f.empty())continue;const auto& tag=f[0];
  if(tag=="CAMPAIGN"){
   if(f.size()<3)throw std::runtime_error("malformed CAMPAIGN record");campaign->name=decode(f[1]);campaign->startMap=integer(f,2,"campaign start map");sawCampaign=true;
  }else if(tag=="MAP"){
   if(f.size()<17)throw std::runtime_error("malformed MAP record");auto map=mapAt(maps,f);map->name=decode(f[2]);
   map->definition.origin={number(f,3,"origin x"),number(f,4,"origin y")};
   map->definition.playerStart={number(f,5,"spawn x"),number(f,6,"spawn y")};map->definition.spawnHeight=number(f,7,"spawn z");
   int environment=integer(f,8,"environment");if(environment<0||environment>1)throw std::runtime_error("invalid custom environment");map->definition.environment=Environment(environment);
   map->definition.ceiling=number(f,9,"ceiling");map->definition.ambient=number(f,10,"ambient");map->skybox=decode(f[11]);
   map->openNorth=boolean(f,12,"north boundary");map->openSouth=boolean(f,13,"south boundary");map->openWest=boolean(f,14,"west boundary");map->openEast=boolean(f,15,"east boundary");
   map->definition.lift=boolean(f,16,"lift flag");
  }else if(tag=="LAYER"){
   if(f.size()<6)throw std::runtime_error("malformed LAYER record");auto& layer=layerAt(maps,f);layer.name=decode(f[3]);layer.elevation=number(f,4,"layer elevation");layer.thickness=number(f,5,"layer thickness");
  }else if(tag=="ROW"){
   if(f.size()<5)throw std::runtime_error("malformed ROW record");auto& layer=layerAt(maps,f);int row=integer(f,3,"row index");if(row<0||row>=24)throw std::runtime_error("invalid row index");layer.rows[size_t(row)]=f[4];
  }else if(tag=="DOOR"){
   if(f.size()<11)throw std::runtime_error("malformed DOOR record");auto map=mapAt(maps,f);
   Door d;d.left=number(f,2,"door left");d.right=number(f,3,"door right");d.y=number(f,4,"door y");d.z=number(f,5,"door z");
   d.entry=boolean(f,6,"door entry");d.transfer=boolean(f,7,"door transfer");d.swinging=boolean(f,8,"door swinging");d.requireEnemiesClear=boolean(f,9,"door clear flag");d.sign=integer(f,10,"door sign");map->doors.push_back(d);
  }else if(tag=="STRUCT"){
   if(f.size()<10)throw std::runtime_error("malformed STRUCT record");auto map=mapAt(maps,f);map->structures.push_back({
    number(f,2,"structure x1"),number(f,3,"structure y1"),number(f,4,"structure x2"),number(f,5,"structure y2"),
    number(f,6,"structure bottom"),number(f,7,"structure top"),boolean(f,8,"structure rail"),integer(f,9,"structure material")});
  }else if(tag=="FIXTURE"){
   if(f.size()<11)throw std::runtime_error("malformed FIXTURE record");auto map=mapAt(maps,f);map->fixtures.push_back({
    integer(f,2,"fixture model"),{number(f,3,"fixture x"),number(f,4,"fixture y")},number(f,5,"fixture base"),
    number(f,6,"fixture width"),number(f,7,"fixture depth"),number(f,8,"fixture height"),number(f,9,"fixture yaw"),boolean(f,10,"fixture solid")});
  }else if(tag=="PROP"){
   if(f.size()<11)throw std::runtime_error("malformed PROP record");auto map=mapAt(maps,f);map->props.push_back({
    integer(f,2,"prop kind"),{number(f,3,"prop x"),number(f,4,"prop y")},number(f,5,"prop height"),number(f,6,"prop footprint"),number(f,7,"prop yaw"),
    {number(f,8,"prop half x"),number(f,9,"prop half y")},number(f,10,"prop base")});
  }else if(tag=="LIGHT"){
   if(f.size()<5)throw std::runtime_error("malformed LIGHT record");auto map=mapAt(maps,f);map->lights.push_back({{number(f,2,"light x"),number(f,3,"light y")},number(f,4,"light z")});
  }else if(tag=="TERMINAL"){
   if(f.size()<9)throw std::runtime_error("malformed TERMINAL record");auto map=mapAt(maps,f);CustomTerminalData t;
   t.position={number(f,2,"terminal x"),number(f,3,"terminal y")};t.z=number(f,4,"terminal z");t.control=boolean(f,5,"terminal control");
   t.title=decode(f[6]);t.line1=decode(f[7]);t.line2=decode(f[8]);map->terminals.push_back(std::move(t));
  }else if(tag=="HAZARD"){
   if(f.size()<10)throw std::runtime_error("malformed HAZARD record");auto map=mapAt(maps,f);int kind=integer(f,2,"hazard kind");if(kind<0||kind>int(Hazard::Kind::Anomaly))throw std::runtime_error("invalid hazard kind");
   map->hazards.push_back({Hazard::Kind(kind),number(f,3,"hazard x1"),number(f,4,"hazard y1"),number(f,5,"hazard x2"),number(f,6,"hazard y2"),number(f,7,"hazard bottom"),number(f,8,"hazard top"),number(f,9,"hazard damage")});
  }else if(tag=="STAIR"){
   if(f.size()<11)throw std::runtime_error("malformed STAIR record");auto map=mapAt(maps,f);map->stairs.push_back({
    number(f,2,"stair x1"),number(f,3,"stair y1"),number(f,4,"stair x2"),number(f,5,"stair y2"),number(f,6,"stair bottom"),number(f,7,"stair top"),
    integer(f,8,"stair steps"),boolean(f,9,"stair along y"),boolean(f,10,"stair ascending")});
  }else if(tag=="CREATURE"){
   if(f.size()<6)throw std::runtime_error("malformed CREATURE record");auto map=mapAt(maps,f);int kind=integer(f,2,"creature kind");if(kind<0||kind>int(CreatureKind::Warden))throw std::runtime_error("invalid creature kind");
   map->creatureSpawns.push_back({CreatureKind(kind),{number(f,3,"creature x"),number(f,4,"creature y")},number(f,5,"creature z")});
  }else if(tag=="PICKUP"){
   if(f.size()<5)throw std::runtime_error("malformed PICKUP record");auto map=mapAt(maps,f);int kind=integer(f,2,"pickup kind");if(kind<0||kind>int(PickupKind::Ammo))throw std::runtime_error("invalid pickup kind");
   map->pickupSpawns.push_back({{number(f,3,"pickup x"),number(f,4,"pickup y")},PickupKind(kind)});
  }else if(tag=="CLUTTER"){
   if(f.size()<7)throw std::runtime_error("malformed CLUTTER record");auto map=mapAt(maps,f);int kind=integer(f,2,"clutter kind");if(kind<0||kind>5)throw std::runtime_error("invalid clutter kind");
   map->clutterSpawns.push_back({kind,{number(f,3,"clutter x"),number(f,4,"clutter y")},number(f,5,"clutter z"),number(f,6,"clutter yaw")});
  }else if(tag=="PIPE"){
   if(f.size()<9)throw std::runtime_error("malformed PIPE record");auto map=mapAt(maps,f);map->pipes.push_back({
    {number(f,2,"pipe x1"),number(f,3,"pipe y1")},{number(f,4,"pipe x2"),number(f,5,"pipe y2")},number(f,6,"pipe z"),number(f,7,"pipe radius"),number(f,8,"pipe end z")});
  }else if(tag=="WATER"){
   if(f.size()<8)throw std::runtime_error("malformed WATER record");auto map=mapAt(maps,f);map->waterVolumes.push_back({
    number(f,2,"water x1"),number(f,3,"water y1"),number(f,4,"water x2"),number(f,5,"water y2"),number(f,6,"water bed"),number(f,7,"water surface")});
  }else if(tag=="COMPACTOR"){
   if(f.size()<9)throw std::runtime_error("malformed COMPACTOR record");auto map=mapAt(maps,f);map->compactors.push_back({
    number(f,2,"compactor x1"),number(f,3,"compactor y1"),number(f,4,"compactor x2"),number(f,5,"compactor y2"),number(f,6,"compactor bed"),number(f,7,"compactor raised"),number(f,8,"compactor period"),0});
  }else throw std::runtime_error("unknown custom campaign record: "+tag);
 }
 if(!sawCampaign||campaign->name.empty())throw std::runtime_error("missing CAMPAIGN record");
 if(maps.empty()||maps.size()>WorldChunkCapacity)throw std::runtime_error("custom campaign has no maps or exceeds engine capacity");
 for(size_t i=0;i<maps.size();++i){
  if(!maps[i])throw std::runtime_error("custom campaign map indices must be contiguous");
  if(maps[i]->layers.empty())throw std::runtime_error("custom campaign map has no layers");
  for(auto& layer:maps[i]->layers){
   if(layer.name.empty())layer.name="Custom floor";
   for(auto& row:layer.rows)if(row.size()!=24)throw std::runtime_error("custom map rows must be exactly 24 characters");
  }
 }
 if(campaign->startMap<0||campaign->startMap>=int(maps.size()))throw std::runtime_error("custom campaign start map is invalid");
 campaign->maps.reserve(maps.size());for(auto& map:maps)campaign->maps.push_back(std::move(map));
 return campaign;
}
std::vector<std::shared_ptr<const CustomCampaign>> loadCustomCampaignDirectory(const std::filesystem::path& directory,std::vector<std::string>* errors){
 std::vector<std::shared_ptr<const CustomCampaign>> result;std::error_code ec;if(!std::filesystem::exists(directory,ec))return result;
 for(auto& entry:std::filesystem::directory_iterator(directory,ec)){
  if(ec)break;if(!entry.is_regular_file()||entry.path().extension()!=L".txt")continue;
  try{result.push_back(loadCustomCampaignFile(entry.path()));}
  catch(const std::exception& error){if(errors)errors->push_back(entry.path().filename().string()+": "+error.what());}
 }
 std::sort(result.begin(),result.end(),[](auto&a,auto&b){return a->name<b->name;});return result;
}
}
