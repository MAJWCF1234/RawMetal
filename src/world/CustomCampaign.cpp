#include "CustomCampaign.h"
#include "../core/PackedResource.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace retro {
namespace {
constexpr std::string_view DataStart="--- CUSTOM_CAMPAIGN_DATA_START ---";
constexpr std::string_view DataEnd="--- CUSTOM_CAMPAIGN_DATA_END ---";

std::string readText(const std::filesystem::path& path){
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("cannot open custom campaign");
 file.seekg(0,std::ios::end);auto end=file.tellg();if(end<0)throw std::runtime_error("cannot size custom campaign");auto size=std::uint64_t(static_cast<std::streamoff>(end));
 if(size>4ull*1024ull*1024ull)throw std::runtime_error("custom campaign is larger than 4 MB");
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
StateId scriptName(const std::vector<std::string>& f,size_t index,const char* label){
 if(index>=f.size())throw std::runtime_error(std::string("missing ")+label);auto value=decode(f[index]);
 if(value.empty()||value.size()>96||!std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/';}))throw std::runtime_error(std::string("invalid ")+label);
 return stateId(value);
}
std::uint64_t campaignKey(std::string value){
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
 std::uint64_t h=1469598103934665603ull;for(unsigned char c:value){h^=c;h*=1099511628211ull;}return h;
}
std::shared_ptr<AuthoredMapData> mapAt(std::vector<std::shared_ptr<AuthoredMapData>>& maps,const std::vector<std::string>& f,size_t field=1){
 int index=integer(f,field,"map index");if(index<0||index>=WorldChunkCapacity)throw std::runtime_error("custom campaign map index outside engine capacity");
 if(size_t(index)>=maps.size())maps.resize(size_t(index)+1);
 if(!maps[size_t(index)])maps[size_t(index)]=std::make_shared<AuthoredMapData>();
 return maps[size_t(index)];
}
AuthoredLayerData& layerAt(std::vector<std::shared_ptr<AuthoredMapData>>& maps,const std::vector<std::string>& f){
 auto map=mapAt(maps,f,1);int layer=integer(f,2,"layer index");if(layer<0||layer>31)throw std::runtime_error("invalid custom layer index");
 if(size_t(layer)>=map->layers.size())map->layers.resize(size_t(layer)+1);return map->layers[size_t(layer)];
}
}

std::shared_ptr<const CustomCampaign> loadCustomCampaignText(std::string text,std::string sourceFile){
 auto start=text.find(DataStart),end=text.find(DataEnd);
 if(start==std::string::npos||end==std::string::npos||end<=start)throw std::runtime_error("missing CUSTOM_CAMPAIGN_DATA markers");
 start+=DataStart.size();std::istringstream stream(text.substr(start,end-start));std::string line;
 auto campaign=std::make_shared<CustomCampaign>();campaign->sourceFile=std::move(sourceFile);campaign->key=campaignKey(campaign->sourceFile);
 std::vector<std::shared_ptr<AuthoredMapData>> maps;bool sawCampaign=false;std::array<bool,WorldChunkCapacity> declared{};
 std::vector<std::pair<int,int>> guardTargets;
 while(std::getline(stream,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
  auto f=fields(line);if(f.empty())continue;const auto& tag=f[0];
  if(tag=="CAMPAIGN"){
   if(f.size()<3)throw std::runtime_error("malformed CAMPAIGN record");campaign->name=decode(f[1]);campaign->startMap=integer(f,2,"campaign start map");sawCampaign=true;
  }else if(tag=="MAP"){
   if(f.size()<17)throw std::runtime_error("malformed MAP record");auto map=mapAt(maps,f);map->name=decode(f[2]);
   int index=integer(f,1,"map index");if(declared[size_t(index)])throw std::runtime_error("duplicate MAP record");declared[size_t(index)]=true;
   map->definition.origin={number(f,3,"origin x"),number(f,4,"origin y")};
   map->definition.playerStart={number(f,5,"spawn x"),number(f,6,"spawn y")};map->definition.spawnHeight=number(f,7,"spawn z");
   int environment=integer(f,8,"environment");if(environment<0||environment>1)throw std::runtime_error("invalid custom environment");map->definition.environment=Environment(environment);
   map->definition.ceiling=number(f,9,"ceiling");map->definition.ambient=number(f,10,"ambient");map->skybox=decode(f[11]);
   map->openNorth=boolean(f,12,"north boundary");map->openSouth=boolean(f,13,"south boundary");map->openWest=boolean(f,14,"west boundary");map->openEast=boolean(f,15,"east boundary");
   map->definition.lift=boolean(f,16,"lift flag");
   if(f.size()>=18)map->definition.residencyGroup=integer(f,17,"residency group");
   if(f.size()>=19){int style=integer(f,18,"floor material");if(style<0||style>1)throw std::runtime_error("invalid floor material");map->definition.floorMaterial=FloorMaterial(style);}
   if(f.size()>=20){int cue=integer(f,19,"music cue");if(cue<0||cue>1)throw std::runtime_error("invalid music cue");map->definition.musicCue=MusicCue(cue);}
   if(f.size()>=21)map->definition.spawnYaw=number(f,20,"spawn yaw");
   if(f.size()>=22)map->definition.spawnPitch=number(f,21,"spawn pitch");
  }else if(tag=="VISIBLE_GROUP"){
   if(f.size()!=3)throw std::runtime_error("malformed VISIBLE_GROUP record");auto map=mapAt(maps,f);
   int group=integer(f,2,"visible residency group");if(group<0)throw std::runtime_error("visible residency group must be nonnegative");
   auto& groups=map->visibleResidencyGroups;if(std::find(groups.begin(),groups.end(),group)==groups.end())groups.push_back(group);
  }else if(tag=="OBJECTIVE_TEXT"){
   if(f.size()!=4)throw std::runtime_error("malformed OBJECTIVE_TEXT record");auto map=mapAt(maps,f);auto id=scriptName(f,2,"objective name");auto label=decode(f[3]);
   if(label.empty()||label.size()>80||!std::all_of(label.begin(),label.end(),[](unsigned char c){return c>=32&&c<=126;})||std::all_of(label.begin(),label.end(),[](char c){return c==' ';}))throw std::runtime_error("invalid objective text; use 1 to 80 printable ASCII characters");
   for(const auto&other:maps)if(other)for(const auto&text:other->objectiveTexts)if(text.id==id)throw std::runtime_error("duplicate objective name");
   if(map->objectiveTexts.size()>=128)throw std::runtime_error("too many objective texts");map->objectiveTexts.push_back({id,std::move(label)});
  }else if(tag=="SCRIPT_EVENT"){
   if(f.size()!=12)throw std::runtime_error("malformed SCRIPT_EVENT record");auto map=mapAt(maps,f);ScriptEvent event;event.id=scriptName(f,2,"script event name");event.level=integer(f,1,"map index");
   for(const auto&other:maps)if(other)for(const auto&previous:other->scriptEvents)if(previous.id==event.id)throw std::runtime_error("duplicate script event name");
   event.x1=number(f,3,"event x1");event.y1=number(f,4,"event y1");event.x2=number(f,5,"event x2");event.y2=number(f,6,"event y2");event.bottom=number(f,7,"event bottom");event.top=number(f,8,"event top");
   if(event.x1<0||event.y1<0||event.x2>World::Width||event.y2>World::Height||event.x1>=event.x2||event.y1>=event.y2||event.bottom>=event.top)throw std::runtime_error("invalid script event region");
   if(!f[9].empty())event.requireState=scriptName(f,9,"event gate state");event.requireValue=integer(f,10,"event gate value");
   if(event.requireValue< -100000||event.requireValue>100000||(!event.requireState&&event.requireValue!=1))throw std::runtime_error("invalid script event gate");event.requireEnemiesClear=boolean(f,11,"event enemy-clear gate");event.once=true;
   if(map->scriptEvents.size()>=128)throw std::runtime_error("too many script events");map->scriptEvents.push_back(std::move(event));
  }else if(tag=="SCRIPT_ACTION"){
   if(f.size()<4)throw std::runtime_error("malformed SCRIPT_ACTION record");auto map=mapAt(maps,f);auto eventId=scriptName(f,2,"action event name");
   auto event=std::find_if(map->scriptEvents.begin(),map->scriptEvents.end(),[&](const ScriptEvent&candidate){return candidate.id==eventId;});if(event==map->scriptEvents.end())throw std::runtime_error("script action references an undeclared event");
   ScriptAction action;const auto&kind=f[3];
   if(kind=="STATE"||kind=="OBJECTIVE"||kind=="ITEM"){
    if(f.size()!=6)throw std::runtime_error("malformed named SCRIPT_ACTION record");action.id=scriptName(f,4,"action name");action.value=integer(f,5,"action value");
    if(kind=="STATE"){if(action.value< -100000||action.value>100000)throw std::runtime_error("invalid script state value");action.type=ScriptAction::Type::SetState;}
    else if(kind=="OBJECTIVE"){if(action.value<0||action.value>3)throw std::runtime_error("invalid script objective status");action.type=ScriptAction::Type::SetObjective;}
    else{if(action.value<1||action.value>99)throw std::runtime_error("invalid script item count");action.type=ScriptAction::Type::GiveItem;}
   }else if(kind=="CHECKPOINT"||kind=="COMPLETE"||kind=="ENTER_CAMPAIGN"){
    if(f.size()!=4)throw std::runtime_error("malformed terminal SCRIPT_ACTION record");action.type=kind=="CHECKPOINT"?ScriptAction::Type::Checkpoint:kind=="COMPLETE"?ScriptAction::Type::CompleteCampaign:ScriptAction::Type::EnterMainCampaign;
   }else if(kind=="SHAKE"){
    if(f.size()!=5)throw std::runtime_error("malformed SHAKE SCRIPT_ACTION record");action.type=ScriptAction::Type::Shake;action.amount=number(f,4,"shake amount");
    if(action.amount<=0||action.amount>3)throw std::runtime_error("invalid shake amount; use more than 0 up to 3");
   }else throw std::runtime_error("unknown script action type");
   if(event->actions.size()>=16)throw std::runtime_error("too many actions for script event");event->actions.push_back(action);
  }else if(tag=="SIGN"){
   if(f.size()!=11)throw std::runtime_error("malformed SIGN record");auto map=mapAt(maps,f);
   AuthoredSign sign;sign.position={number(f,2,"sign x"),number(f,3,"sign y")};sign.z=number(f,4,"sign z");
   sign.width=number(f,5,"sign width");sign.height=number(f,6,"sign height");sign.yaw=number(f,7,"sign yaw");
   sign.title=decode(f[8]);sign.subtitle=decode(f[9]);sign.accent=std::uint32_t(integer(f,10,"sign accent"))|0xff000000u;
   if(sign.width<=0||sign.height<=0)throw std::runtime_error("invalid sign dimensions");map->signs.push_back(std::move(sign));
  }else if(tag=="LAYER"){
   if(f.size()<6)throw std::runtime_error("malformed LAYER record");auto& layer=layerAt(maps,f);layer.name=decode(f[3]);layer.elevation=number(f,4,"layer elevation");layer.thickness=number(f,5,"layer thickness");
  }else if(tag=="ROW"){
   if(f.size()<5)throw std::runtime_error("malformed ROW record");auto& layer=layerAt(maps,f);int row=integer(f,3,"row index");if(row<0||row>=24)throw std::runtime_error("invalid row index");layer.rows[size_t(row)]=f[4];
  }else if(tag=="DOOR"){
   if(f.size()<11)throw std::runtime_error("malformed DOOR record");auto map=mapAt(maps,f);
   Door d;d.left=number(f,2,"door left");d.right=number(f,3,"door right");d.y=number(f,4,"door y");d.z=number(f,5,"door z");
   d.entry=boolean(f,6,"door entry");d.transfer=boolean(f,7,"door transfer");d.swinging=boolean(f,8,"door swinging");d.requireEnemiesClear=boolean(f,9,"door clear flag");d.sign=integer(f,10,"door sign");
   if(f.size()>=12&&!f[11].empty())d.requireState=stateId(decode(f[11]));
   if(f.size()>=13)d.requireValue=integer(f,12,"door require value");
   map->doors.push_back(d);
  }else if(tag=="STRUCT"){
   if(f.size()<10)throw std::runtime_error("malformed STRUCT record");auto map=mapAt(maps,f);map->structures.push_back({
    number(f,2,"structure x1"),number(f,3,"structure y1"),number(f,4,"structure x2"),number(f,5,"structure y2"),
    number(f,6,"structure bottom"),number(f,7,"structure top"),boolean(f,8,"structure rail"),integer(f,9,"structure material")});
  }else if(tag=="FIXTURE"){
   if(f.size()<11)throw std::runtime_error("malformed FIXTURE record");auto map=mapAt(maps,f);map->fixtures.push_back({
    integer(f,2,"fixture model"),{number(f,3,"fixture x"),number(f,4,"fixture y")},number(f,5,"fixture base"),
    number(f,6,"fixture width"),number(f,7,"fixture depth"),number(f,8,"fixture height"),number(f,9,"fixture yaw"),boolean(f,10,"fixture solid")});
   const auto& fixture=map->fixtures.back();if(fixture.model<0||fixture.model>=FacilityModelCount||fixture.width<=0||fixture.depth<=0||fixture.height<=0)throw std::runtime_error("invalid fixture model or dimensions");
  }else if(tag=="PROP"){
   if(f.size()<11)throw std::runtime_error("malformed PROP record");auto map=mapAt(maps,f);map->props.push_back({
    integer(f,2,"prop kind"),{number(f,3,"prop x"),number(f,4,"prop y")},number(f,5,"prop height"),number(f,6,"prop footprint"),number(f,7,"prop yaw"),
    {number(f,8,"prop half x"),number(f,9,"prop half y")},number(f,10,"prop base")});
   if(map->props.back().kind<0||map->props.back().kind>=4)throw std::runtime_error("invalid prop model");
  }else if(tag=="LIGHT"){
   if(f.size()<5||f.size()>9)throw std::runtime_error("malformed LIGHT record");auto map=mapAt(maps,f);
   WorldLight light{{number(f,2,"light x"),number(f,3,"light y")},number(f,4,"light z")};
   if(f.size()>=6&&!f[5].empty()){int mount=integer(f,5,"light mount");if(mount<0||mount>1)throw std::runtime_error("invalid light mount");light.mount=WorldLightMount(mount);}
   if(f.size()>=7&&!f[6].empty())light.yaw=number(f,6,"light yaw");
   if(f.size()>=8&&!f[7].empty())light.intensity=number(f,7,"light intensity");
   if(f.size()>=9&&!f[8].empty())light.range=number(f,8,"light range");
   map->lights.push_back(light);
  }else if(tag=="TERMINAL"){
   if(f.size()<9)throw std::runtime_error("malformed TERMINAL record");auto map=mapAt(maps,f);AuthoredTerminalData t;
   t.position={number(f,2,"terminal x"),number(f,3,"terminal y")};t.z=number(f,4,"terminal z");t.control=boolean(f,5,"terminal control");
   t.title=decode(f[6]);t.line1=decode(f[7]);t.line2=decode(f[8]);
   if(f.size()>=10&&!f[9].empty())t.activateState=stateId(decode(f[9]));
   if(f.size()>=11)t.toggleState=boolean(f,10,"terminal toggle");
   if(f.size()>=12&&!f[11].empty())t.requireState=stateId(decode(f[11]));
   if(f.size()>=13&&!f[12].empty())t.yaw=number(f,12,"terminal yaw");
   map->terminals.push_back(std::move(t));
  }else if(tag=="CARGO_LIFT"){
   if(f.size()<15)throw std::runtime_error("malformed CARGO_LIFT record");auto map=mapAt(maps,f);
   for(size_t key=9;key<15;++key)if(f[key].empty())throw std::runtime_error("cargo lift requires six named states");
   map->cargoLift={number(f,2,"lift x1"),number(f,3,"lift y1"),number(f,4,"lift x2"),number(f,5,"lift y2"),number(f,6,"lift lower"),number(f,7,"lift upper"),number(f,8,"lift speed"),stateId(decode(f[9])),stateId(decode(f[10])),stateId(decode(f[11])),stateId(decode(f[12])),stateId(decode(f[13])),stateId(decode(f[14]))};
  }else if(tag=="TIMED_SEQUENCE"){
   if(f.size()<17||f[2].empty()||f[3].empty())throw std::runtime_error("malformed TIMED_SEQUENCE record");auto map=mapAt(maps,f);
   int tick=integer(f,14,"sequence sound");if(tick<0||tick>=int(Sound::Count))throw std::runtime_error("invalid sequence sound");
   map->timedSequences.push_back({stateId(decode(f[2])),stateId(decode(f[3])),number(f,4,"sequence x1"),number(f,5,"sequence y1"),number(f,6,"sequence x2"),number(f,7,"sequence y2"),number(f,8,"sequence bottom"),number(f,9,"sequence top"),integer(f,10,"sequence duration"),integer(f,11,"sequence finish time"),integer(f,12,"sequence sound interval"),integer(f,13,"sequence sound cutoff"),Sound(tick),number(f,15,"sequence gain"),number(f,16,"sequence pitch")});
   if(f.size()>17)map->timedSequences.back().loop=boolean(f,17,"sequence loop");
  }else if(tag=="ACTOR"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR record");auto map=mapAt(maps,f);int kind=integer(f,3,"actor visual");if(kind<0||kind>9||f[2].empty())throw std::runtime_error("invalid actor visual or timer");
   if(integer(f,7,"actor index")!=int(map->actorTracks.size()))throw std::runtime_error("actor indices must be sequential");
   map->actorTracks.push_back({stateId(decode(f[2])),ActorVisual(kind),number(f,4,"actor scale"),boolean(f,5,"actor loop"),boolean(f,6,"actor tool"),{}});
  }else if(tag=="ACTOR_AI"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR_AI record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),mode=integer(f,3,"actor AI mode");
   if(index<0||index>=int(map->actorTracks.size())||mode<0||mode>3)throw std::runtime_error("invalid actor AI target or mode");
   auto& ai=map->actorTracks[size_t(index)].ai;ai.mode=ActorAiMode(mode);ai.enableState=f[4].empty()?0:stateId(decode(f[4]));ai.speed=number(f,5,"actor speed");ai.followDistance=number(f,6,"follow distance");ai.dangerRange=number(f,7,"danger range");
  }else if(tag=="ACTOR_FACTION"){
   if(f.size()!=5)throw std::runtime_error("malformed ACTOR_FACTION record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),faction=integer(f,3,"actor faction");
   if(index<0||index>=int(map->actorTracks.size())||faction<0||faction>5)throw std::runtime_error("invalid actor faction target or value");
   auto&ai=map->actorTracks[size_t(index)].ai;ai.faction=AiFaction(faction);ai.squad=f[4].empty()?0:stateId(decode(f[4]));
  }else if(tag=="ACTOR_GUARD"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR_GUARD record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("guard configuration has no actor");
   auto& ai=map->actorTracks[size_t(index)].ai;ai.sightRange=number(f,3,"guard sight range");ai.sightDot=number(f,4,"guard sight dot");ai.recognitionMs=integer(f,5,"guard recognition duration");ai.searchMs=integer(f,6,"guard search duration");ai.alertState=f[7].empty()?0:stateId(decode(f[7]));
   guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_WEAPON"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR_WEAPON record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("weapon configuration has no actor");
   int weapon=integer(f,3,"actor weapon");if(weapon<1||weapon>2)throw std::runtime_error("invalid actor weapon; use Shotgun (1) or Handgun (2)");map->actorTracks[size_t(index)].ai.weapon=weapon;
   auto& ai=map->actorTracks[size_t(index)].ai;ai.armed=true;ai.aimMs=integer(f,4,"weapon aim duration");ai.recoveryMs=integer(f,5,"weapon recovery duration");ai.attackRange=number(f,6,"weapon attack range");ai.attackDamage=number(f,7,"weapon attack damage");
   guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_ROLE"){
   if(f.size()!=4)throw std::runtime_error("malformed ACTOR_ROLE record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),role=integer(f,3,"squad role");if(index<0||index>=int(map->actorTracks.size())||role<0||role>6)throw std::runtime_error("invalid guard role");map->actorTracks[size_t(index)].ai.role=GuardSquadRole(role);guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_TACTICS"){
   if(f.size()!=5)throw std::runtime_error("malformed ACTOR_TACTICS record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),suppress=integer(f,3,"suppress fire");
   if(index<0||index>=int(map->actorTracks.size())||suppress<0||suppress>1)throw std::runtime_error("invalid actor tactics target or suppression");
   auto&ai=map->actorTracks[size_t(index)].ai;ai.suppress=suppress!=0;ai.grenades=integer(f,4,"grenade inventory");guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_PATROL"){
   if(f.size()!=7)throw std::runtime_error("malformed ACTOR_PATROL record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("patrol point has no actor");
   map->actorTracks[size_t(index)].ai.patrol.push_back({{number(f,3,"patrol x"),number(f,4,"patrol y")},number(f,5,"patrol height"),integer(f,6,"patrol pause")});
   guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_HEALTH"){
   if(f.size()!=6||f[4].empty()||f[5].empty())throw std::runtime_error("malformed ACTOR_HEALTH record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("health has no actor");auto& t=map->actorTracks[size_t(index)];t.health=integer(f,3,"actor health");t.damageState=stateId(decode(f[4]));t.deadState=stateId(decode(f[5]));
  }else if(tag=="ACTOR_SUSPEND"){
   if(f.size()!=4)throw std::runtime_error("malformed ACTOR_SUSPEND record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("suspension has no actor");map->actorTracks[size_t(index)].suspensionTop=number(f,3,"suspension top");
  }else if(tag=="ACTOR_PLATFORM"){
   if(f.size()!=6)throw std::runtime_error("malformed ACTOR_PLATFORM record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("platform has no actor");auto& t=map->actorTracks[size_t(index)];t.platform=true;t.footprint={number(f,3,"platform width"),number(f,4,"platform depth")};t.thickness=number(f,5,"platform thickness");
  }else if(tag=="ACTOR_PRELUDE"){
   if(f.size()!=5)throw std::runtime_error("malformed ACTOR_PRELUDE record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("prelude has no actor");auto& actor=map->actorTracks[size_t(index)];actor.idleUntilMs=integer(f,3,"idle duration");actor.approachUntilMs=integer(f,4,"approach end");
  }else if(tag=="SEQUENCE_SIGHT"){
   if(f.size()!=5)throw std::runtime_error("malformed SEQUENCE_SIGHT record");auto map=mapAt(maps,f);int index=integer(f,2,"sequence index");if(index<0||index>=int(map->timedSequences.size()))throw std::runtime_error("sight has no sequence");map->timedSequences[size_t(index)].sightActor=integer(f,3,"sight actor");map->timedSequences[size_t(index)].sightDistance=number(f,4,"sight range");
  }else if(tag=="ACTOR_KEY"||tag=="ACTOR_IDLE_KEY"){
   if(f.size()!=10&&f.size()!=11)throw std::runtime_error("malformed actor key record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("actor key has no actor");
   ActorKey key{integer(f,3,"actor time"),{number(f,4,"actor x"),number(f,5,"actor y")},number(f,6,"actor z"),number(f,7,"actor yaw"),integer(f,8,"actor clip"),number(f,9,"actor phase")};if(f.size()>10)key.lookAtActor=integer(f,10,"look target");
   auto& actor=map->actorTracks[size_t(index)];(tag=="ACTOR_KEY"?actor.keys:actor.idleKeys).push_back(key);
  }else if(tag=="SEQUENCE_CUE"){
   if(f.size()!=10&&f.size()!=11)throw std::runtime_error("malformed SEQUENCE_CUE record");auto map=mapAt(maps,f);int sound=integer(f,4,"cue sound");if(sound<0||sound>=int(Sound::Count))throw std::runtime_error("invalid cue sound");
   map->sequenceCues.push_back({stateId(decode(f[2])),integer(f,3,"cue time"),Sound(sound),{number(f,5,"cue x"),number(f,6,"cue y")},number(f,7,"cue gain"),number(f,8,"cue pitch"),decode(f[9]),f.size()==11?number(f,10,"cue height"):std::numeric_limits<float>::quiet_NaN()});
  }else if(tag=="HAZARD"){
   if(f.size()<10)throw std::runtime_error("malformed HAZARD record");auto map=mapAt(maps,f);int kind=integer(f,2,"hazard kind");if(kind<0||kind>int(Hazard::Kind::Anomaly))throw std::runtime_error("invalid hazard kind");
   map->hazards.push_back({Hazard::Kind(kind),number(f,3,"hazard x1"),number(f,4,"hazard y1"),number(f,5,"hazard x2"),number(f,6,"hazard y2"),number(f,7,"hazard bottom"),number(f,8,"hazard top"),number(f,9,"hazard damage")});
  }else if(tag=="STAIR"){
   if(f.size()<11||f.size()>14)throw std::runtime_error("malformed STAIR record");auto map=mapAt(maps,f);map->stairs.push_back({
    number(f,2,"stair x1"),number(f,3,"stair y1"),number(f,4,"stair x2"),number(f,5,"stair y2"),number(f,6,"stair bottom"),number(f,7,"stair top"),
    integer(f,8,"stair steps"),boolean(f,9,"stair along y"),boolean(f,10,"stair ascending"),f.size()>11&&!f[11].empty()?boolean(f,11,"stair open underside"):false,f.size()>12&&!f[12].empty()?number(f,12,"stair tread thickness"):.12f,f.size()>13&&!f[13].empty()?boolean(f,13,"stair side rails"):false});
  }else if(tag=="CREATURE"){
   if(f.size()<6)throw std::runtime_error("malformed CREATURE record");auto map=mapAt(maps,f);int kind=integer(f,2,"creature kind");if(kind<0||kind>int(CreatureKind::Mutant))throw std::runtime_error("invalid creature kind");
   map->creatureSpawns.push_back({CreatureKind(kind),{number(f,3,"creature x"),number(f,4,"creature y")},number(f,5,"creature z")});
  }else if(tag=="PICKUP"){
   if(f.size()<5)throw std::runtime_error("malformed PICKUP record");auto map=mapAt(maps,f);int kind=integer(f,2,"pickup kind");if(kind<0||kind>int(PickupKind::Ammo))throw std::runtime_error("invalid pickup kind");
   map->pickupSpawns.push_back({{number(f,3,"pickup x"),number(f,4,"pickup y")},PickupKind(kind)});
  }else if(tag=="CLUTTER"){
   if(f.size()<7)throw std::runtime_error("malformed CLUTTER record");auto map=mapAt(maps,f);int kind=integer(f,2,"clutter kind");if(kind<0||kind>5)throw std::runtime_error("invalid clutter kind");
   map->clutterSpawns.push_back({kind,{number(f,3,"clutter x"),number(f,4,"clutter y")},number(f,5,"clutter z"),number(f,6,"clutter yaw")});
  }else if(tag=="PIPE"){
   if(f.size()<9)throw std::runtime_error("malformed PIPE record");auto map=mapAt(maps,f);map->pipes.push_back({
    {number(f,2,"pipe x1"),number(f,3,"pipe y1")},{number(f,4,"pipe x2"),number(f,5,"pipe y2")},number(f,6,"pipe z"),number(f,7,"pipe radius"),number(f,8,"pipe end z"),f.size()>9?integer(f,9,"pipe material"):0});
   if(map->pipes.back().material<0||map->pipes.back().material>6)throw std::runtime_error("invalid pipe material");
  }else if(tag=="WATER"){
   if(f.size()<8)throw std::runtime_error("malformed WATER record");auto map=mapAt(maps,f);map->waterVolumes.push_back({
    number(f,2,"water x1"),number(f,3,"water y1"),number(f,4,"water x2"),number(f,5,"water y2"),number(f,6,"water bed"),number(f,7,"water surface")});
  }else if(tag=="COMPACTOR"){
   if(f.size()<9)throw std::runtime_error("malformed COMPACTOR record");auto map=mapAt(maps,f);map->compactors.push_back({
    number(f,2,"compactor x1"),number(f,3,"compactor y1"),number(f,4,"compactor x2"),number(f,5,"compactor y2"),number(f,6,"compactor bed"),number(f,7,"compactor raised"),number(f,8,"compactor period"),0});
  }else if(tag=="NAV_HINT"){
   if(f.size()!=6)throw std::runtime_error("malformed NAV_HINT record");auto map=mapAt(maps,f);int hint=integer(f,5,"nav hint kind");
   if(hint<0||hint>1)throw std::runtime_error("invalid nav hint kind; use Path (0) or Cover (1)");
   map->navHints.push_back({{number(f,2,"nav hint x"),number(f,3,"nav hint y")},number(f,4,"nav hint z"),hint});
  }else throw std::runtime_error("unknown custom campaign record: "+tag);
 }
 for(const auto& target:guardTargets)if(maps[size_t(target.first)]->actorTracks[size_t(target.second)].ai.mode!=ActorAiMode::Guard)throw std::runtime_error("guard configuration, weapon or patrol requires Guard actor AI mode");
 if(!sawCampaign||campaign->name.empty())throw std::runtime_error("missing CAMPAIGN record");
 if(maps.empty()||maps.size()>WorldChunkCapacity)throw std::runtime_error("custom campaign has no maps or exceeds engine capacity");
 for(size_t i=0;i<maps.size();++i){
  if(!maps[i]||!declared[i])throw std::runtime_error("custom campaign maps require contiguous MAP records");
  if(maps[i]->layers.empty())throw std::runtime_error("custom campaign map has no layers");
  for(const auto&event:maps[i]->scriptEvents){
   if(event.actions.empty())throw std::runtime_error("script event requires at least one action");
   for(const auto&action:event.actions)if(action.type==ScriptAction::Type::SetObjective){bool named=false;for(const auto&other:maps)if(other)for(const auto&label:other->objectiveTexts)named|=label.id==action.id;if(!named)throw std::runtime_error("script objective has no OBJECTIVE_TEXT");}
  }
  for(auto& layer:maps[i]->layers){
   if(layer.name.empty())layer.name="Custom floor";
   for(auto& row:layer.rows)if(row.size()!=24)throw std::runtime_error("custom map rows must be exactly 24 characters");
  }
  auto spawn=maps[i]->definition.playerStart;if(spawn.x<0||spawn.x>=World::Width||spawn.y<0||spawn.y>=World::Height)throw std::runtime_error("custom map spawn is outside its chunk");
  for(size_t j=0;j<i;++j){auto a=maps[i]->definition.origin,b=maps[j]->definition.origin;
   if(a.x<b.x+World::Width&&a.x+World::Width>b.x&&a.y<b.y+World::Height&&a.y+World::Height>b.y)throw std::runtime_error("custom map chunk footprints overlap");
  }
  // Reject malformed geometry during discovery, while the caller can report
  // a per-file error, rather than crashing when a player selects the pack.
  World validated(int(i),maps[i]);
 }
 if(campaign->startMap<0||campaign->startMap>=int(maps.size()))throw std::runtime_error("custom campaign start map is invalid");
 campaign->maps.reserve(maps.size());for(auto& map:maps)campaign->maps.push_back(std::move(map));
 return campaign;
}
std::shared_ptr<const CustomCampaign> loadCustomCampaignFile(const std::filesystem::path& path){
 return loadCustomCampaignText(readText(path),path.filename().string());
}
std::shared_ptr<const CustomCampaign> loadEmbeddedExclusionZoneCampaign(){
 // Packed resource 346 is the Ashfall Exclusion Zone wasteland custom campaign TXT.
 auto bytes=loadResource(346);return loadCustomCampaignText(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()),"RawMetal-Ashfall-Exclusion-Zone.txt");
}
std::vector<std::shared_ptr<const CustomCampaign>> loadCustomCampaignDirectory(const std::filesystem::path& directory,std::vector<std::string>* errors){
 std::vector<std::shared_ptr<const CustomCampaign>> result;std::error_code ec;
 if(std::filesystem::exists(directory,ec))for(auto& entry:std::filesystem::directory_iterator(directory,ec)){
  if(ec)break;if(!entry.is_regular_file()||entry.path().extension()!=L".txt")continue;
  try{result.push_back(loadCustomCampaignFile(entry.path()));}
  catch(const std::exception& error){if(errors)errors->push_back(entry.path().filename().string()+": "+error.what());}
 }
 // One-file friend builds have no sidecar custom-maps folder. Keep Exclusion Zone
 // available from the embedded payload unless a disk copy already wins.
 bool hasExclusionZone=false;for(const auto& campaign:result)hasExclusionZone|=campaign->sourceFile=="RawMetal-Ashfall-Exclusion-Zone.txt";
 if(!hasExclusionZone){
  try{result.push_back(loadEmbeddedExclusionZoneCampaign());}
  catch(const std::exception& error){if(errors)errors->push_back(std::string("embedded Exclusion Zone: ")+error.what());}
 }
 std::sort(result.begin(),result.end(),[](auto&a,auto&b){return a->name<b->name;});return result;
}
}
