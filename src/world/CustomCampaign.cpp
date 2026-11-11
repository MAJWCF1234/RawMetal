#include "CustomCampaign.h"
#include "Primitives.h"
#include "../audio/SoundBank.h"
#include "../core/PackedResource.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <map>
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
// SIGN_ART pixels: base64 of RGBA bytes, exactly width x height of them.
bool signPicture(std::string_view text,int width,int height,SignPicture& out){
 if(width<1||height<1||width>256||height>256||width*height>65536)return false;
 auto value=[](char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
 std::vector<unsigned char> bytes;bytes.reserve(text.size()*3/4);unsigned acc=0;int bits=0;
 for(char c:text){if(c=='=')break;int v=value(c);if(v<0)return false;acc=(acc<<6)|unsigned(v);bits+=6;if(bits>=8){bits-=8;bytes.push_back((acc>>bits)&255);}}
 if(bytes.size()!=size_t(width)*height*4)return false;
 out.width=width;out.height=height;out.pixels.resize(size_t(width)*height);
 for(size_t i=0;i<out.pixels.size();++i)out.pixels[i]=(std::uint32_t(bytes[i*4+3])<<24)|(std::uint32_t(bytes[i*4])<<16)|(std::uint32_t(bytes[i*4+1])<<8)|bytes[i*4+2];
 return true;
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
// "#1234567" names a state by its id (stateId's hash) instead of its spelling:
// what the campaign exporter writes for a state whose name it does not know.
// It loads back to the identical state. Any other text is hashed as a name.
bool rawState(std::string_view value,StateId& id){
 if(value.size()<2||value[0]!='#')return false;std::uint64_t n=0;
 for(char c:value.substr(1)){if(c<'0'||c>'9')return false;n=n*10+unsigned(c-'0');if(n>0xffffffffull)return false;}
 id=StateId(n);return true;
}
StateId stateField(std::string_view raw){auto value=decode(raw);StateId id=0;return rawState(value,id)?id:stateId(value);}
StateId scriptName(const std::vector<std::string>& f,size_t index,const char* label){
 if(index>=f.size())throw std::runtime_error(std::string("missing ")+label);auto value=decode(f[index]);
 if(StateId id=0;rawState(value,id)){if(!id)throw std::runtime_error(std::string("invalid ")+label);return id;}
 if(value.empty()||value.size()>96||!std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/';}))throw std::runtime_error(std::string("invalid ")+label);
 return stateId(value);
}
std::uint64_t campaignKey(std::string value){
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
 // Saves made while a pack was still "Name.txt" must keep loading after it is renamed "Name.rmc".
 constexpr std::string_view packExtension=".rmc";
 if(value.size()>packExtension.size()&&value.ends_with(packExtension))value.replace(value.size()-packExtension.size(),packExtension.size(),".txt");
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
 std::vector<std::pair<int,int>> guardTargets;std::shared_ptr<TerrainShape> terrain;
 // Test leads named by panel socket and prop terminal, seated once every fixture and prop is read.
 struct Seat{std::shared_ptr<AuthoredMapData> map;size_t lead;int panel;std::string socket,terminal;};std::vector<Seat> seats;
 // Sign pictures by name, and the signs that wear one (resolved after the last record).
 std::map<std::string,std::shared_ptr<const SignPicture>> art;std::vector<std::pair<std::shared_ptr<AuthoredMapData>,size_t>> artUsers;
 // Errors name the file line, counted from the top, so an author can find the record.
 // The stream opens on the rest of the start marker's line, so that first read brings this to the marker's line.
 int lineNumber=int(std::count(text.begin(),text.begin()+std::ptrdiff_t(start),'\n'));
 while(std::getline(stream,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#'){++lineNumber;continue;}
  auto f=fields(line);if(f.empty()){++lineNumber;continue;}const auto& tag=f[0];
  ++lineNumber;try{
  // A field the exporter appends to an older record (empty or absent keeps the default).
  auto given=[&](size_t i){return f.size()>i&&!f[i].empty();};
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
   if(f.size()>=20){int cue=integer(f,19,"music cue");if(cue<0||cue>=int(MusicCue::Count))throw std::runtime_error("invalid music cue");map->definition.musicCue=MusicCue(cue);}
   if(f.size()>=21)map->definition.spawnYaw=number(f,20,"spawn yaw");
   if(f.size()>=22)map->definition.spawnPitch=number(f,21,"spawn pitch");
   if(f.size()>=23)map->definition.startUnarmed=boolean(f,22,"start unarmed");
   // |hazeR|hazeG|hazeB|hazeDensity: the map's own air (all four, or none for the environment's).
   if(f.size()>=24&&!f[23].empty()){if(f.size()<27)throw std::runtime_error("a map haze needs its red, green, blue and density");
    map->definition.haze={number(f,23,"haze red"),number(f,24,"haze green"),number(f,25,"haze blue"),number(f,26,"haze density")};
    if(map->definition.haze[3]<0||!validHaze(map->definition.haze))throw std::runtime_error("map haze channels must be 0 through 1 and density 0 through 0.1");}
   // |wetness: the floors' sheen and puddles, 0 dry through 1 flooded (empty for the default).
   if(f.size()>=28&&!f[27].empty()){map->definition.wetness=number(f,27,"wetness");if(!(map->definition.wetness>=0&&map->definition.wetness<=1))throw std::runtime_error("map wetness must be 0 through 1");}
  }else if(tag=="TERRAIN"){
   // TERRAIN|seed|base|amplitude|scale|roughness: the campaign's outdoor heightfield.
   if(f.size()!=6)throw std::runtime_error("malformed TERRAIN record");if(!terrain)terrain=std::make_shared<TerrainShape>();
   terrain->seed=std::uint32_t(integer(f,1,"terrain seed"));terrain->base=number(f,2,"terrain base");terrain->amplitude=number(f,3,"terrain amplitude");
   terrain->scale=number(f,4,"terrain scale");terrain->roughness=number(f,5,"terrain roughness");
   if(terrain->base<World::TerrainMinZ+2||terrain->base>World::TerrainMaxZ-4||terrain->amplitude<0||terrain->amplitude>12||terrain->scale<6||terrain->scale>400||terrain->roughness<0||terrain->roughness>.9f)throw std::runtime_error("terrain parameters out of range");
   if(terrain->base-terrain->amplitude<World::TerrainMinZ+1||terrain->base+terrain->amplitude>World::TerrainMaxZ-2)throw std::runtime_error("TERRAIN[0] base="+std::to_string(terrain->base)+" amplitude="+std::to_string(terrain->amplitude)+" height range leaves [-11,18] (campaign-wide, all maps)");
  }else if(tag=="TERRAIN_FLAT"){
   // TERRAIN_FLAT|x1|y1|x2|y2|height|blend in world metres: a level pad or road.
   if(f.size()!=7)throw std::runtime_error("malformed TERRAIN_FLAT record");if(!terrain)terrain=std::make_shared<TerrainShape>();
   TerrainFlat flat{number(f,1,"flat x1"),number(f,2,"flat y1"),number(f,3,"flat x2"),number(f,4,"flat y2"),number(f,5,"flat height"),number(f,6,"flat blend")};
   if(flat.x2<flat.x1||flat.y2<flat.y1||flat.blend<0||flat.blend>60||flat.height<World::TerrainMinZ+2||flat.height>World::TerrainMaxZ-4)throw std::runtime_error("invalid TERRAIN_FLAT");
   if(terrain->flats.size()>=512)throw std::runtime_error("too many TERRAIN_FLAT records");terrain->flats.push_back(flat);
  }else if(tag=="HORIZON"){
   // HORIZON|depth|spacing|rise|model,model,...: wooded land beyond the zone edge.
   if(f.size()!=5)throw std::runtime_error("malformed HORIZON record");if(!terrain)terrain=std::make_shared<TerrainShape>();
   terrain->horizonDepth=number(f,1,"horizon depth");terrain->horizonSpacing=number(f,2,"horizon spacing");terrain->horizonRise=number(f,3,"horizon rise");
   if(terrain->horizonDepth<0||terrain->horizonDepth>400||terrain->horizonSpacing<2||terrain->horizonSpacing>40||terrain->horizonRise<0||terrain->horizonRise>60)throw std::runtime_error("invalid HORIZON");
   terrain->horizonModels.clear();std::stringstream list(f[4]);std::string item;
   while(std::getline(list,item,',')){int model=std::stoi(item);if(model<0||model>=FacilityModelCount)throw std::runtime_error("invalid HORIZON model");terrain->horizonModels.push_back(model);}
  }else if(tag=="GROUND"){
   // GROUND|x1|y1|x2|y2|kind|edge in world metres: kind 0 short grass, 1 leafy
   // grass, 2 clover, 3 wild, 4 dry dirt, 5 forest litter, 6 gravel, 7 mud, 8 meadow.
   if(f.size()!=7)throw std::runtime_error("malformed GROUND record");if(!terrain)terrain=std::make_shared<TerrainShape>();
   GroundPaint paint{number(f,1,"ground x1"),number(f,2,"ground y1"),number(f,3,"ground x2"),number(f,4,"ground y2"),integer(f,5,"ground kind"),number(f,6,"ground edge")};
   if(paint.x2<paint.x1||paint.y2<paint.y1||paint.kind<0||paint.kind>GroundMeadow||paint.edge<0||paint.edge>8)throw std::runtime_error("invalid GROUND");
   if(terrain->ground.size()>=1024)throw std::runtime_error("too many GROUND records");terrain->ground.push_back(paint);
  }else if(tag=="VISIBLE_GROUP"){
   if(f.size()!=3)throw std::runtime_error("malformed VISIBLE_GROUP record");auto map=mapAt(maps,f);
   int group=integer(f,2,"visible residency group");if(group<0)throw std::runtime_error("visible residency group must be nonnegative");
   auto& groups=map->visibleResidencyGroups;if(std::find(groups.begin(),groups.end(),group)==groups.end())groups.push_back(group);
  }else if(tag=="SURFACE"){
   // SURFACE|map|wall|floor|ceiling: World.h surface sets, -1 keeps the default;
   // 0 concrete, 1 pressure, 2 brick, 3 clinic tile, 4 lab panel, 5 rock,
   // 6 corrugated, 7 office plaster, 8 mine timber, 9 ancient stone, 10-12 clinic
   // paint over a teal, green or maroon dado band.
   if(f.size()!=5)throw std::runtime_error("malformed SURFACE record");auto map=mapAt(maps,f);
   int wall=integer(f,2,"wall surface"),floor=integer(f,3,"floor surface"),ceiling=integer(f,4,"ceiling surface");
   for(int surface:{wall,floor,ceiling})if(surface<SurfaceDefault||surface>=SurfaceCount)throw std::runtime_error("invalid surface; use -1 through 12");
   map->wallSurface=wall;map->floorSurface=floor;map->ceilingSurface=ceiling;
  }else if(tag=="OBJECTIVE_TEXT"){
   if(f.size()!=4)throw std::runtime_error("malformed OBJECTIVE_TEXT record");auto map=mapAt(maps,f);auto id=scriptName(f,2,"objective name");auto label=decode(f[3]);
   if(label.empty()||label.size()>80||!std::all_of(label.begin(),label.end(),[](unsigned char c){return c>=32&&c<=126;})||std::all_of(label.begin(),label.end(),[](char c){return c==' ';}))throw std::runtime_error("invalid objective text; use 1 to 80 printable ASCII characters");
   for(const auto&other:maps)if(other)for(const auto&text:other->objectiveTexts)if(text.id==id)throw std::runtime_error("duplicate objective name");
   if(map->objectiveTexts.size()>=128)throw std::runtime_error("too many objective texts");map->objectiveTexts.push_back({id,std::move(label)});
  }else if(tag=="SCRIPT_EVENT"){
   if(f.size()!=12)throw std::runtime_error("malformed SCRIPT_EVENT record");auto map=mapAt(maps,f);ScriptEvent event;event.id=scriptName(f,2,"script event name");event.level=integer(f,1,"map index");
   for(const auto&other:maps)if(other)for(const auto&previous:other->scriptEvents)if(previous.id==event.id)throw std::runtime_error("duplicate script event name");
   event.x1=number(f,3,"event x1");event.y1=number(f,4,"event y1");event.x2=number(f,5,"event x2");event.y2=number(f,6,"event y2");event.bottom=number(f,7,"event bottom");event.top=number(f,8,"event top");
   if(event.x1<0||event.y1<0||event.x2>World::Width||event.y2>World::Height||event.x1>=event.x2||event.y1>=event.y2||event.bottom>=event.top)throw std::runtime_error("map "+f[1]+" scriptEvent["+std::to_string(map->scriptEvents.size())+"] id="+std::to_string(event.id)+" bounds="+std::to_string(event.x1)+","+std::to_string(event.y1)+","+std::to_string(event.x2)+","+std::to_string(event.y2)+" bottom="+std::to_string(event.bottom)+" top="+std::to_string(event.top)+" invalid region");
   if(!f[9].empty())event.requireState=scriptName(f,9,"event gate state");event.requireValue=integer(f,10,"event gate value");
   if(event.requireValue< -100000||event.requireValue>100000||(!event.requireState&&event.requireValue!=1))throw std::runtime_error("invalid script event gate");event.requireEnemiesClear=boolean(f,11,"event enemy-clear gate");event.once=true;
   if(map->scriptEvents.size()>=128)throw std::runtime_error("too many script events");map->scriptEvents.push_back(std::move(event));
  }else if(tag=="SCRIPT_REQUIRE"){
   // SCRIPT_REQUIRE|map|event|name|op|value: one more gate on an event, ANDed with the
   // others unless SCRIPT_TRIGGER says any. op is = != < <= > >=. name is a state, or
   // @kills / @remaining (the event's map), @health or @map.
   if(f.size()!=6)throw std::runtime_error("malformed SCRIPT_REQUIRE record");auto map=mapAt(maps,f);auto eventId=scriptName(f,2,"require event name");
   auto event=std::find_if(map->scriptEvents.begin(),map->scriptEvents.end(),[&](const ScriptEvent&candidate){return candidate.id==eventId;});if(event==map->scriptEvents.end())throw std::runtime_error("SCRIPT_REQUIRE references an undeclared event");
   ScriptCondition c;using S=ScriptCondition::Source;using O=ScriptCondition::Op;const auto name=decode(f[3]);
   if(name=="@kills")c.source=S::Kills;else if(name=="@remaining")c.source=S::Remaining;else if(name=="@health")c.source=S::Health;else if(name=="@map")c.source=S::Map;
   else if(!name.empty()&&name[0]=='@')throw std::runtime_error("unknown SCRIPT_REQUIRE value; use a state name, @kills, @remaining, @health or @map");
   else c.id=scriptName(f,3,"require state");
   const auto op=decode(f[4]);
   if(op=="="||op=="==")c.op=O::Equal;else if(op=="!=")c.op=O::NotEqual;else if(op=="<")c.op=O::Less;else if(op=="<=")c.op=O::LessEqual;else if(op==">")c.op=O::Greater;else if(op==">=")c.op=O::GreaterEqual;
   else throw std::runtime_error("invalid SCRIPT_REQUIRE comparison; use = != < <= > >=");
   c.value=integer(f,5,"require value");if(c.value< -100000||c.value>100000)throw std::runtime_error("invalid SCRIPT_REQUIRE value");
   if(event->conditions.size()>=8)throw std::runtime_error("too many SCRIPT_REQUIRE records for one event (8 at most)");event->conditions.push_back(c);
  }else if(tag=="SCRIPT_TRIGGER"){
   // SCRIPT_TRIGGER|map|event|where|match|repeat: where is box (default), map or campaign;
   // match is all (default) or any; repeat 1 fires each time the gates turn true.
   // Optional |once: 0 lets an event that does not repeat fire on every pass its gates hold.
   if(f.size()!=6&&f.size()!=7)throw std::runtime_error("malformed SCRIPT_TRIGGER record");auto map=mapAt(maps,f);auto eventId=scriptName(f,2,"trigger event name");
   auto event=std::find_if(map->scriptEvents.begin(),map->scriptEvents.end(),[&](const ScriptEvent&candidate){return candidate.id==eventId;});if(event==map->scriptEvents.end())throw std::runtime_error("SCRIPT_TRIGGER references an undeclared event");
   const auto where=decode(f[3]),match=decode(f[4]);
   if(where=="box"||where.empty())event->scope=ScriptEvent::Scope::Box;else if(where=="map")event->scope=ScriptEvent::Scope::Map;else if(where=="campaign")event->scope=ScriptEvent::Scope::Campaign;
   else throw std::runtime_error("invalid SCRIPT_TRIGGER scope; use box, map or campaign");
   if(match=="all"||match.empty())event->matchAny=false;else if(match=="any")event->matchAny=true;else throw std::runtime_error("invalid SCRIPT_TRIGGER match; use all or any");
   event->rearm=boolean(f,5,"trigger repeat");event->once=!event->rearm;if(given(6))event->once=boolean(f,6,"trigger once");
  }else if(tag=="SCRIPT_ACTION"){
   if(f.size()<4)throw std::runtime_error("malformed SCRIPT_ACTION record");auto map=mapAt(maps,f);auto eventId=scriptName(f,2,"action event name");
   auto event=std::find_if(map->scriptEvents.begin(),map->scriptEvents.end(),[&](const ScriptEvent&candidate){return candidate.id==eventId;});if(event==map->scriptEvents.end())throw std::runtime_error("script action references an undeclared event");
   ScriptAction action;const auto&kind=f[3];
   if(kind=="STATE"||kind=="OBJECTIVE"||kind=="ITEM"||kind=="ADD"||kind=="TAKE"){
    if(f.size()!=6)throw std::runtime_error("malformed named SCRIPT_ACTION record");action.id=scriptName(f,4,"action name");action.value=integer(f,5,"action value");
    if(kind=="STATE"){if(action.value< -100000||action.value>100000)throw std::runtime_error("invalid script state value");action.type=ScriptAction::Type::SetState;}
    else if(kind=="ADD"){if(action.value==0||action.value< -100000||action.value>100000)throw std::runtime_error("invalid ADD amount; use a non-zero whole number");action.type=ScriptAction::Type::AddState;}
    else if(kind=="OBJECTIVE"){if(action.value<0||action.value>3)throw std::runtime_error("invalid script objective status");action.type=ScriptAction::Type::SetObjective;}
    else if(kind=="TAKE"){if(action.value<1||action.value>99)throw std::runtime_error("invalid script item count");action.type=ScriptAction::Type::TakeItem;}
    else{if(action.value<1||action.value>99)throw std::runtime_error("invalid script item count");action.type=ScriptAction::Type::GiveItem;}
   }else if(kind=="MESSAGE"){
    // MESSAGE|text|seconds: a caption; {state_name} shows that state's value.
    if(f.size()!=6)throw std::runtime_error("malformed MESSAGE SCRIPT_ACTION record");action.type=ScriptAction::Type::ShowMessage;action.text=decode(f[4]);action.amount=number(f,5,"message seconds");
    if(action.text.empty()||action.text.size()>96||action.amount<.5f||action.amount>15)throw std::runtime_error("invalid MESSAGE; use 1-96 characters for 0.5-15 seconds");
   }else if(kind=="CHECKPOINT"||kind=="COMPLETE"||kind=="ENTER_CAMPAIGN"){
    if(f.size()!=4)throw std::runtime_error("malformed terminal SCRIPT_ACTION record");action.type=kind=="CHECKPOINT"?ScriptAction::Type::Checkpoint:kind=="COMPLETE"?ScriptAction::Type::CompleteCampaign:ScriptAction::Type::EnterMainCampaign;
   }else if(kind=="SHAKE"){
    if(f.size()!=5)throw std::runtime_error("malformed SHAKE SCRIPT_ACTION record");action.type=ScriptAction::Type::Shake;action.amount=number(f,4,"shake amount");
    if(action.amount<=0||action.amount>3)throw std::runtime_error("invalid shake amount; use more than 0 up to 3");
   }else if(kind=="RAW"){
    // RAW|type|state|value|index|creature|x|y|z|amount|sound|text: any action exactly as the
    // engine holds it (ScriptAction), for what the kinds above can not say (doors, sounds, spawns).
    if(f.size()!=15)throw std::runtime_error("malformed RAW SCRIPT_ACTION record");int type=integer(f,4,"action type");
    if(type<0||type>int(ScriptAction::Type::ShowMessage))throw std::runtime_error("invalid RAW action type");action.type=ScriptAction::Type(type);
    if(given(5))action.id=stateField(f[5]);action.value=integer(f,6,"action value");action.index=integer(f,7,"action index");
    int creature=integer(f,8,"action creature");if(creature<0||creature>int(CreatureKind::Transformed))throw std::runtime_error("invalid RAW action creature");action.enemyKind=CreatureKind(creature);
    action.position={number(f,9,"action x"),number(f,10,"action y")};action.z=number(f,11,"action z");action.amount=number(f,12,"action amount");
    int sound=integer(f,13,"action sound");if(sound<0||sound>=int(Sound::Count))throw std::runtime_error("invalid RAW action sound");action.sound=Sound(sound);action.text=decode(f[14]);
   }else throw std::runtime_error("unknown script action type");
   if(event->actions.size()>=16)throw std::runtime_error("too many actions for script event");event->actions.push_back(action);
  }else if(tag=="SIGN"){
   // SIGN|map|x|y|z|width|height|yaw|title|subtitle|colour[|style|icon|ink|art]: the
   // designer's plate style, symbol, lettering colour (0 = the style's) and SIGN_ART picture.
   if(f.size()<11||f.size()>15)throw std::runtime_error("malformed SIGN record");auto map=mapAt(maps,f);
   AuthoredSign sign;sign.position={number(f,2,"sign x"),number(f,3,"sign y")};sign.z=number(f,4,"sign z");
   sign.width=number(f,5,"sign width");sign.height=number(f,6,"sign height");sign.yaw=number(f,7,"sign yaw");
   sign.title=decode(f[8]);sign.subtitle=decode(f[9]);sign.accent=std::uint32_t(integer(f,10,"sign accent"))|0xff000000u;
   if(given(11))sign.style=integer(f,11,"sign style");if(given(12))sign.icon=integer(f,12,"sign symbol");
   if(given(13))sign.ink=std::uint32_t(integer(f,13,"sign ink"))&0xffffffu;if(sign.ink)sign.ink|=0xff000000u;
   if(given(14))sign.art=decode(f[14]);
   if(sign.style<0||sign.style>6||sign.icon<0||sign.icon>10)throw std::runtime_error("invalid sign style or symbol");
   if(sign.width<=0||sign.height<=0)throw std::runtime_error("invalid sign dimensions");
   if(!sign.art.empty())artUsers.push_back({map,map->signs.size()});
   map->signs.push_back(std::move(sign));
  }else if(tag=="AMBIENT"){
   // AMBIENT|map|sound|x|y|z|gain|radius[|mode|every|spread|state|value]: a bank sound (audio/SoundBank.h) placed
   // in the map. mode 0 a looping bed, 1 now and then (every..every+spread seconds), 2 once each time state = value.
   if(f.size()<8||f.size()>13)throw std::runtime_error("malformed AMBIENT record");auto map=mapAt(maps,f);
   const auto* row=bankSound(decode(f[2]));if(!row)throw std::runtime_error("unknown AMBIENT sound "+decode(f[2]));
   AmbientEmitter a;a.sound=row->sound;a.position={number(f,3,"ambient x"),number(f,4,"ambient y")};a.z=number(f,5,"ambient z");a.gain=number(f,6,"ambient gain");a.radius=number(f,7,"ambient radius");
   a.mode=given(8)?integer(f,8,"ambient mode"):row->loop?AmbientEmitter::Loop:AmbientEmitter::Random;
   if(given(9))a.every=number(f,9,"ambient every");if(given(10))a.spread=number(f,10,"ambient spread");if(given(11))a.state=stateField(f[11]);if(given(12))a.value=integer(f,12,"ambient value");
   if(a.gain<=0||a.gain>4||a.radius<=0||a.radius>60||a.mode<AmbientEmitter::Loop||a.mode>AmbientEmitter::OnState||(a.mode==AmbientEmitter::Random&&(a.every<.5f||a.every>3600||a.spread<0||a.spread>3600)))throw std::runtime_error("invalid AMBIENT gain, radius, mode or timing");
   if(a.mode==AmbientEmitter::Loop&&!row->loop)throw std::runtime_error("AMBIENT sound "+std::string(row->name)+" is a single sound, not a loop (use mode 1 or 2)");
   if(a.mode==AmbientEmitter::OnState&&!a.state)throw std::runtime_error("AMBIENT mode 2 needs a state");
   map->ambients.push_back(a);
  }else if(tag=="REVERB"){
   // REVERB|map|wet|seconds: this map's reverb, instead of the measure the game takes of the space.
   if(f.size()!=4)throw std::runtime_error("malformed REVERB record");auto map=mapAt(maps,f);map->reverbWet=number(f,2,"reverb wet");map->reverbSeconds=number(f,3,"reverb seconds");
   if(map->reverbWet<0||map->reverbWet>1||map->reverbSeconds<.1f||map->reverbSeconds>6)throw std::runtime_error("invalid REVERB (wet 0-1, seconds 0.1-6)");
  }else if(tag=="SIGN_ART"){
   // SIGN_ART|name|width|height|pixels: a picture for signs, RGBA bytes in base64 (at most 256 a side, 65536 pixels).
   if(f.size()!=5)throw std::runtime_error("malformed SIGN_ART record");auto name=decode(f[1]);if(name.empty())throw std::runtime_error("SIGN_ART needs a name");
   int w=integer(f,2,"art width"),h=integer(f,3,"art height");auto picture=std::make_shared<SignPicture>();
   if(!signPicture(f[4],w,h,*picture))throw std::runtime_error("invalid SIGN_ART picture (size or base64)");
   if(!art.emplace(name,std::move(picture)).second)throw std::runtime_error("SIGN_ART "+name+" is defined twice");
  }else if(tag=="LAYER"){
   if(f.size()<6)throw std::runtime_error("malformed LAYER record");auto& layer=layerAt(maps,f);layer.name=decode(f[3]);layer.elevation=number(f,4,"layer elevation");layer.thickness=number(f,5,"layer thickness");
  }else if(tag=="ROW"){
   if(f.size()<5)throw std::runtime_error("malformed ROW record");auto& layer=layerAt(maps,f);int row=integer(f,3,"row index");if(row<0||row>=24)throw std::runtime_error("invalid row index");layer.rows[size_t(row)]=f[4];
  }else if(tag=="DOOR"){
   if(f.size()<11)throw std::runtime_error("malformed DOOR record");auto map=mapAt(maps,f);
   Door d;d.left=number(f,2,"door left");d.right=number(f,3,"door right");d.y=number(f,4,"door y");d.z=number(f,5,"door z");
   d.entry=boolean(f,6,"door entry");d.transfer=boolean(f,7,"door transfer");d.swinging=boolean(f,8,"door swinging");d.requireEnemiesClear=boolean(f,9,"door clear flag");d.sign=integer(f,10,"door sign");
   if(f.size()>=12&&!f[11].empty())d.requireState=stateField(f[11]);
   if(given(12))d.requireValue=integer(f,12,"door require value");
   // |control|swing|maxOpen|open|opening: the plant's control gate, the side a swinging
   // leaf opens to (1 or -1), how far it opens (0-1), and how open it starts and whether it is opening.
   if(given(13))d.requireControl=boolean(f,13,"door control gate");
   if(given(14))d.swing=number(f,14,"door swing");
   if(given(15))d.maxOpen=number(f,15,"door max open");
   if(given(16))d.open=number(f,16,"door open");
   if(given(17))d.opening=boolean(f,17,"door opening");
   // |leaf|label|locked: 0 standard, 1 steel grille, 2 painted panel; the prompt's
   // name for it; what the prompt says while it is locked.
   if(given(18))d.leaf=integer(f,18,"door leaf");
   if(f.size()>19)d.label=decode(f[19]);
   if(f.size()>20)d.locked=decode(f[20]);
   if(f.size()>21||d.maxOpen<=0||d.maxOpen>1||d.open<0||d.open>1||d.leaf<0||d.leaf>2||d.label.size()>24||d.locked.size()>40)throw std::runtime_error("malformed DOOR record");
   map->doors.push_back(d);
  }else if(tag=="STRUCT"){
   if(f.size()<10)throw std::runtime_error("malformed STRUCT record");auto map=mapAt(maps,f);map->structures.push_back({
    number(f,2,"structure x1"),number(f,3,"structure y1"),number(f,4,"structure x2"),number(f,5,"structure y2"),
     number(f,6,"structure bottom"),number(f,7,"structure top"),boolean(f,8,"structure rail"),integer(f,9,"structure material"),f.size()>10&&!f[10].empty()?boolean(f,10,"structure solid"):true});
   // |shape|facing: a drawn shape (World.h StructureShape, 0 box .. 7 roundel) and the quarter turn it faces.
   if(given(11)){int shape=integer(f,11,"structure shape");if(shape<0||shape>ShapeRoundel)throw std::runtime_error("invalid structure shape");map->structures.back().shape=std::uint8_t(shape);}
   if(given(12)){int facing=integer(f,12,"structure facing");if(facing<0||facing>3)throw std::runtime_error("invalid structure facing");map->structures.back().facing=std::uint8_t(facing);}
  }else if(tag=="PRIM"){
   if(f.size()<11)throw std::runtime_error("malformed PRIM record");auto map=mapAt(maps,f);
   const Primitive primitive{f[2],number(f,3,"primitive x"),number(f,4,"primitive y"),number(f,5,"primitive z"),number(f,6,"primitive width"),number(f,7,"primitive depth"),
    number(f,8,"primitive height"),integer(f,9,"primitive facing"),integer(f,10,"primitive material"),f.size()>11&&!f[11].empty()?boolean(f,11,"primitive solid"):true};
   try{expandPrimitive(primitive,map->structures);}catch(const std::runtime_error& e){throw std::runtime_error("map "+f[1]+" PRIM: "+e.what());}
  }else if(tag=="FIXTURE"){
   if(f.size()<11)throw std::runtime_error("malformed FIXTURE record");auto map=mapAt(maps,f);map->fixtures.push_back({
    integer(f,2,"fixture model"),{number(f,3,"fixture x"),number(f,4,"fixture y")},number(f,5,"fixture base"),
    number(f,6,"fixture width"),number(f,7,"fixture depth"),number(f,8,"fixture height"),number(f,9,"fixture yaw"),boolean(f,10,"fixture solid")});
   if(f.size()>=12){float core=number(f,11,"fixture collision core");if(core<0||core>1)throw std::runtime_error("map "+f[1]+" fixture["+std::to_string(map->fixtures.size()-1)+"] core="+std::to_string(core)+" must be in [0,1]");map->fixtures.back().core=core;}
   // 13th field 0: collision only (a neighbouring map draws the model).
   if(f.size()>=13)map->fixtures.back().visible=boolean(f,12,"fixture visible");
   if(f.size()>13)throw std::runtime_error("malformed FIXTURE record");
   const auto& fixture=map->fixtures.back();if(fixture.model<0||fixture.model>=FacilityModelCount||fixture.width<=0||fixture.depth<=0||fixture.height<=0)throw std::runtime_error("invalid fixture model or dimensions");
  }else if(tag=="PROP"){
   if(f.size()<11)throw std::runtime_error("malformed PROP record");auto map=mapAt(maps,f);map->props.push_back({
    integer(f,2,"prop kind"),{number(f,3,"prop x"),number(f,4,"prop y")},number(f,5,"prop height"),number(f,6,"prop footprint"),number(f,7,"prop yaw"),
    {number(f,8,"prop half x"),number(f,9,"prop half y")},number(f,10,"prop base")});
   if(map->props.back().kind<0||map->props.back().kind>=4)throw std::runtime_error("invalid prop model");
  }else if(tag=="LIGHT"){
   // LIGHT|map|x|y|z[|mount|yaw|intensity|range|kelvin|tint|state|value|invert|period|onTime|phase|flicker|fitting|width|depth|height|gelR|gelG|gelB]:
   // a lamp; the optional tail switches it like a HAZARD (state gate, timed window) and adds flicker 0-1.
   // A ceiling lamp's fitting is a fixture model drawn flush under its ceiling at width x depth x height.
   // gelR|gelG|gelB (each 0-1, all three) filters its colour: 1|0.1|0.06 is a red emergency lamp.
   // |shadows|prop: 0 casts no shadow; prop rides that CLUTTER record (from 0) when it is carried.
   if(f.size()<5||f.size()>27)throw std::runtime_error("malformed LIGHT record");auto map=mapAt(maps,f);
   WorldLight light{{number(f,2,"light x"),number(f,3,"light y")},number(f,4,"light z")};
   if(f.size()>=6&&!f[5].empty()){int mount=integer(f,5,"light mount");if(mount<0||mount>2)throw std::runtime_error("invalid light mount");light.mount=WorldLightMount(mount);}
   if(f.size()>=7&&!f[6].empty())light.yaw=number(f,6,"light yaw");
   if(f.size()>=8&&!f[7].empty())light.intensity=number(f,7,"light intensity");
   if(f.size()>=9&&!f[8].empty())light.range=number(f,8,"light range");
   if(f.size()>=10&&!f[9].empty()){light.kelvin=number(f,9,"light kelvin");if(light.kelvin<1000||light.kelvin>12000)throw std::runtime_error("light kelvin must be 1000 through 12000");}
   if(f.size()>=11&&!f[10].empty()){light.tint=number(f,10,"light tint");if(light.tint<-1||light.tint>1)throw std::runtime_error("light tint must be -1 through 1");}
   if(f.size()>=12&&!f[11].empty())light.enabledState=scriptName(f,11,"light state");
   if(f.size()>=13&&!f[12].empty())light.enabledValue=integer(f,12,"light state value");
   if(f.size()>=14&&!f[13].empty())light.invert=boolean(f,13,"light invert");
   if(f.size()>=15&&!f[14].empty())light.period=number(f,14,"light period");
   if(f.size()>=16&&!f[15].empty())light.onTime=number(f,15,"light on time");
   if(f.size()>=17&&!f[16].empty())light.phase=number(f,16,"light phase");
   if(f.size()>=18&&!f[17].empty())light.flicker=number(f,17,"light flicker");
   if(f.size()>=19&&!f[18].empty()){
    if(f.size()<22)throw std::runtime_error("a light fitting needs its width, depth and height");
    light.fitting=integer(f,18,"light fitting");if(light.fitting<0||light.fitting>=FacilityModelCount)throw std::runtime_error("invalid light fitting model");
    light.fittingWidth=number(f,19,"fitting width");light.fittingDepth=number(f,20,"fitting depth");light.fittingHeight=number(f,21,"fitting height");
    if(light.fittingWidth<=0||light.fittingDepth<=0||light.fittingHeight<=0)throw std::runtime_error("light fitting size must be positive");
   }
   if(f.size()>=23&&!f[22].empty()){
    if(f.size()<25)throw std::runtime_error("a light gel needs its red, green and blue");
    light.gel={number(f,22,"light gel red"),number(f,23,"light gel green"),number(f,24,"light gel blue")};
    if(!(light.gel.x>=0&&light.gel.x<=1&&light.gel.y>=0&&light.gel.y<=1&&light.gel.z>=0&&light.gel.z<=1)||light.gel.x+light.gel.y+light.gel.z<=0)throw std::runtime_error("light gel channels must be 0 through 1 and not all 0");
   }
   if(given(25))light.shadows=boolean(f,25,"light shadows");
   if(given(26)){light.prop=integer(f,26,"light prop");if(light.prop<-1)throw std::runtime_error("invalid light prop");}
   if(light.period<0||light.onTime<0||light.onTime>light.period||light.flicker<0||light.flicker>1)throw std::runtime_error("light switching needs period >= 0, on time within the period and flicker 0 through 1");
   map->lights.push_back(light);
  }else if(tag=="DARK"){
   // DARK|map|x1|y1|x2|y2[|bottom|top|state|value|invert|jam]: true darkness while the
   // player's eye is inside and the zone is active; jam (default 1) kills the flashlight.
   if(f.size()<6||f.size()>12)throw std::runtime_error("malformed DARK record");auto map=mapAt(maps,f);DarkZone zone;
   zone.x1=number(f,2,"dark x1");zone.y1=number(f,3,"dark y1");zone.x2=number(f,4,"dark x2");zone.y2=number(f,5,"dark y2");
   if(f.size()>=7&&!f[6].empty())zone.bottom=number(f,6,"dark bottom");
   if(f.size()>=8&&!f[7].empty())zone.top=number(f,7,"dark top");
   if(f.size()>=9&&!f[8].empty())zone.state=scriptName(f,8,"dark state");
   if(f.size()>=10&&!f[9].empty())zone.value=integer(f,9,"dark state value");
   if(f.size()>=11&&!f[10].empty())zone.invert=boolean(f,10,"dark invert");
   if(f.size()>=12&&!f[11].empty())zone.jamFlashlight=boolean(f,11,"dark flashlight jam");
   if(zone.x1>=zone.x2||zone.y1>=zone.y2||zone.bottom>=zone.top)throw std::runtime_error("invalid DARK bounds");
   if(map->darkZones.size()>=64)throw std::runtime_error("too many DARK records in one map");map->darkZones.push_back(zone);
  }else if(tag=="ZONE"){
   // ZONE|map|x1|y1|x2|y2|name[|bottom|top[|title[|hazeR|hazeG|hazeB|hazeDensity]]]: a named (up to 64 characters)
   // HUD location; a title makes it a major area announced by a title card (name
   // may be empty); a haze gives the box its own air (name and title may then both be empty).
   if(f.size()<7||f.size()>14)throw std::runtime_error("malformed ZONE record");auto map=mapAt(maps,f);AuthoredZone zone;
   zone.x1=number(f,2,"zone x1");zone.y1=number(f,3,"zone y1");zone.x2=number(f,4,"zone x2");zone.y2=number(f,5,"zone y2");zone.name=f[6];
   if(f.size()>=8&&!f[7].empty())zone.bottom=number(f,7,"zone bottom");
   if(f.size()>=9&&!f[8].empty())zone.top=number(f,8,"zone top");
   if(f.size()>=10)zone.title=f[9];
   if(f.size()>=11&&!f[10].empty()){if(f.size()<14)throw std::runtime_error("a zone haze needs its red, green, blue and density");
    zone.haze={number(f,10,"zone haze red"),number(f,11,"zone haze green"),number(f,12,"zone haze blue"),number(f,13,"zone haze density")};
    if(zone.haze[3]<0||!validHaze(zone.haze))throw std::runtime_error("zone haze channels must be 0 through 1 and density 0 through 0.1");}
   if(zone.x1>=zone.x2||zone.y1>=zone.y2||zone.bottom>=zone.top||(zone.name.empty()&&zone.title.empty()&&zone.haze[3]<0)||zone.name.size()>64||zone.title.size()>64)throw std::runtime_error("invalid ZONE bounds, name or title");
   map->zones.push_back(std::move(zone));
  }else if(tag=="TERMINAL"){
   if(f.size()<9)throw std::runtime_error("malformed TERMINAL record");auto map=mapAt(maps,f);AuthoredTerminalData t;
   t.position={number(f,2,"terminal x"),number(f,3,"terminal y")};t.z=number(f,4,"terminal z");t.control=boolean(f,5,"terminal control");
   t.title=decode(f[6]);t.line1=decode(f[7]);t.line2=decode(f[8]);
   if(f.size()>=10&&!f[9].empty())t.activateState=stateField(f[9]);
   if(f.size()>=11)t.toggleState=boolean(f,10,"terminal toggle");
   if(f.size()>=12&&!f[11].empty())t.requireState=stateField(f[11]);
   if(f.size()>=13&&!f[12].empty())t.yaw=number(f,12,"terminal yaw");
   // |readable|taped|surface|reactor: 0 screen, 1 letter, 2 note, 3 sticky; paper taped to its
   // wall; the desk top paper lies on (-1 its own bench); a reactor console action.
   if(given(13)){int readable=integer(f,13,"terminal readable");if(readable<0||readable>3)throw std::runtime_error("invalid terminal readable");t.readable=Readable(readable);}
   if(given(14))t.taped=boolean(f,14,"terminal taped");
   if(given(15))t.surface=number(f,15,"terminal surface");
   if(given(16))t.reactorAction=integer(f,16,"terminal reactor action");
   map->terminals.push_back(std::move(t));
  }else if(tag=="CARGO_LIFT"){
   if(f.size()<15)throw std::runtime_error("malformed CARGO_LIFT record");auto map=mapAt(maps,f);
   for(size_t key=9;key<15;++key)if(f[key].empty())throw std::runtime_error("cargo lift requires six named states");
   map->cargoLift={number(f,2,"lift x1"),number(f,3,"lift y1"),number(f,4,"lift x2"),number(f,5,"lift y2"),number(f,6,"lift lower"),number(f,7,"lift upper"),number(f,8,"lift speed"),stateField(f[9]),stateField(f[10]),stateField(f[11]),stateField(f[12]),stateField(f[13]),stateField(f[14])};
  }else if(tag=="TIMED_SEQUENCE"){
   if(f.size()<17||f[2].empty()||f[3].empty())throw std::runtime_error("malformed TIMED_SEQUENCE record");auto map=mapAt(maps,f);
   int tick=integer(f,14,"sequence sound");if(tick<0||tick>=int(Sound::Count))throw std::runtime_error("invalid sequence sound");
   map->timedSequences.push_back({stateField(f[2]),stateField(f[3]),number(f,4,"sequence x1"),number(f,5,"sequence y1"),number(f,6,"sequence x2"),number(f,7,"sequence y2"),number(f,8,"sequence bottom"),number(f,9,"sequence top"),integer(f,10,"sequence duration"),integer(f,11,"sequence finish time"),integer(f,12,"sequence sound interval"),integer(f,13,"sequence sound cutoff"),Sound(tick),number(f,15,"sequence gain"),number(f,16,"sequence pitch")});
   if(f.size()>17)map->timedSequences.back().loop=boolean(f,17,"sequence loop");
   // |soundX|soundY|soundZ: where the tick plays when it is not the box's centre.
   if(given(18)&&given(19))map->timedSequences.back().soundAt={number(f,18,"sequence sound x"),number(f,19,"sequence sound y")};
   if(given(20))map->timedSequences.back().soundZ=number(f,20,"sequence sound z");
  }else if(tag=="ACTOR"){
   // ACTOR|map|timer|visual|scale|loop|tool|index[|dropsSidearm]: the optional last field drops a carrier's pistol when he goes down.
   if(f.size()!=8&&f.size()!=9)throw std::runtime_error("malformed ACTOR record");auto map=mapAt(maps,f);int kind=integer(f,3,"actor visual");if(kind<0||kind>int(LastActorVisual)||f[2].empty())throw std::runtime_error("invalid actor visual or timer");
   if(integer(f,7,"actor index")!=int(map->actorTracks.size()))throw std::runtime_error("actor indices must be sequential");
   map->actorTracks.push_back({stateField(f[2]),ActorVisual(kind),number(f,4,"actor scale"),boolean(f,5,"actor loop"),boolean(f,6,"actor tool"),{}});
   if(f.size()==9)map->actorTracks.back().ai.dropsSidearm=boolean(f,8,"actor drops sidearm");
  }else if(tag=="ACTOR_AI"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR_AI record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),mode=integer(f,3,"actor AI mode");
   if(index<0||index>=int(map->actorTracks.size())||mode<0||mode>3)throw std::runtime_error("invalid actor AI target or mode");
   auto& ai=map->actorTracks[size_t(index)].ai;ai.mode=ActorAiMode(mode);ai.enableState=f[4].empty()?0:stateField(f[4]);ai.speed=number(f,5,"actor speed");ai.followDistance=number(f,6,"follow distance");ai.dangerRange=number(f,7,"danger range");
  }else if(tag=="ACTOR_FACTION"){
   if(f.size()!=5)throw std::runtime_error("malformed ACTOR_FACTION record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),faction=integer(f,3,"actor faction");
   if(index<0||index>=int(map->actorTracks.size())||faction<0||faction>5)throw std::runtime_error("invalid actor faction target or value");
   auto&ai=map->actorTracks[size_t(index)].ai;ai.faction=AiFaction(faction);ai.squad=f[4].empty()?0:stateField(f[4]);
  }else if(tag=="ACTOR_GUARD"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR_GUARD record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("guard configuration has no actor");
   auto& ai=map->actorTracks[size_t(index)].ai;ai.sightRange=number(f,3,"guard sight range");ai.sightDot=number(f,4,"guard sight dot");ai.recognitionMs=integer(f,5,"guard recognition duration");ai.searchMs=integer(f,6,"guard search duration");ai.alertState=f[7].empty()?0:stateField(f[7]);
   guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_WEAPON"){
   if(f.size()!=8)throw std::runtime_error("malformed ACTOR_WEAPON record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("weapon configuration has no actor");
   int weapon=integer(f,3,"actor weapon");if(weapon<1||weapon>2)throw std::runtime_error("invalid actor weapon; use Shotgun (1) or Handgun (2)");map->actorTracks[size_t(index)].ai.weapon=weapon;
   auto& ai=map->actorTracks[size_t(index)].ai;ai.armed=true;ai.aimMs=integer(f,4,"weapon aim duration");ai.recoveryMs=integer(f,5,"weapon recovery duration");ai.attackRange=number(f,6,"weapon attack range");ai.attackDamage=number(f,7,"weapon attack damage");
   guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_VOICE"){
   // ACTOR_VOICE|map|actor|role: a voice bank role (1-10) instead of the visual's default.
   if(f.size()!=4)throw std::runtime_error("malformed ACTOR_VOICE record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),voice=integer(f,3,"voice role");
   if(index<0||index>=int(map->actorTracks.size())||voice<1||voice>12)throw std::runtime_error("invalid actor voice");map->actorTracks[size_t(index)].voice=voice;
  }else if(tag=="STORM"){
   // STORM|calm ms|warning ms|storm ms|damage per second|ambient ash (0-1): one cycle for the campaign.
   if(f.size()!=6)throw std::runtime_error("malformed STORM record");
   campaign->storm={integer(f,1,"storm calm"),integer(f,2,"storm warning"),integer(f,3,"storm duration"),number(f,4,"storm damage"),number(f,5,"ambient ash")};
   const auto&s=campaign->storm;if(s.calmMs<1000||s.warningMs<0||s.stormMs<1000||s.damage<0||s.damage>100||s.ambientAsh<0||s.ambientAsh>1)throw std::runtime_error("invalid STORM timing or strength");
  }else if(tag=="SHELTER"){
   // SHELTER|map|x1|y1|x2|y2|bottom|top: under this roof the storm does no harm.
   if(f.size()!=8)throw std::runtime_error("malformed SHELTER record");auto map=mapAt(maps,f);
   Shelter s{number(f,2,"shelter x1"),number(f,3,"shelter y1"),number(f,4,"shelter x2"),number(f,5,"shelter y2"),number(f,6,"shelter bottom"),number(f,7,"shelter top")};
   if(s.x2<=s.x1||s.y2<=s.y1||s.top<=s.bottom)throw std::runtime_error("empty SHELTER box");map->shelters.push_back(s);
  }else if(tag=="ACTOR_STANCE"){
   // ACTOR_STANCE|map|actor|state: the state holding this squad's stance (0 declared, 1 hostile, 2 truce, 3 ally).
   if(f.size()!=4||f[3].empty())throw std::runtime_error("malformed ACTOR_STANCE record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("stance has no actor");map->actorTracks[size_t(index)].ai.stance=stateField(f[3]);
  }else if(tag=="ACTOR_TALK"){
   // ACTOR_TALK|map|actor|node: the dialogue node E opens on this actor.
   if(f.size()!=4||f[3].empty())throw std::runtime_error("malformed ACTOR_TALK record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("dialogue has no actor");map->actorTracks[size_t(index)].talk=stateField(f[3]);
  }else if(tag=="DIALOGUE"){
   // DIALOGUE|map|node|speaker|line|voice
   if(f.size()!=6||f[2].empty())throw std::runtime_error("malformed DIALOGUE record");auto map=mapAt(maps,f);
   map->dialogue.push_back({stateField(f[2]),decode(f[3]),decode(f[4]),decode(f[5]),{}});
  }else if(tag=="DIALOGUE_REPLY"){
   // DIALOGUE_REPLY|map|node|text|next|require|requireValue|hide|sets|stance
   // sets: state=value pairs joined by ';'. next and stance may be empty (end; no change).
   if(f.size()!=10||f[2].empty()||f[3].empty())throw std::runtime_error("malformed DIALOGUE_REPLY record");auto map=mapAt(maps,f);StateId node=stateField(f[2]);
   auto owner=std::find_if(map->dialogue.begin(),map->dialogue.end(),[&](const DialogueNode&n){return n.id==node;});
   if(owner==map->dialogue.end())throw std::runtime_error("dialogue reply before its node");
   DialogueReply reply;reply.text=decode(f[3]);reply.next=f[4].empty()?0:stateField(f[4]);reply.require=f[5].empty()?0:stateField(f[5]);
   reply.requireValue=integer(f,6,"reply require value");reply.hide=f[7].empty()?0:stateField(f[7]);
   std::string sets=decode(f[8]);for(size_t at=0;at<sets.size();){size_t end=sets.find(';',at);if(end==std::string::npos)end=sets.size();auto pair=sets.substr(at,end-at);at=end+1;if(pair.empty())continue;
    auto eq=pair.find('=');if(eq==std::string::npos||eq==0)throw std::runtime_error("dialogue reply sets need state=value");reply.sets.push_back({stateField(pair.substr(0,eq)),std::stoi(pair.substr(eq+1))});}
   reply.stance=f[9].empty()?-1:integer(f,9,"reply stance");if(reply.stance<-1||reply.stance>3)throw std::runtime_error("invalid reply stance");
   owner->replies.push_back(std::move(reply));
  }else if(tag=="ACTOR_ROLE"){
   if(f.size()!=4)throw std::runtime_error("malformed ACTOR_ROLE record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),role=integer(f,3,"squad role");if(index<0||index>=int(map->actorTracks.size())||role<0||role>6)throw std::runtime_error("invalid guard role");map->actorTracks[size_t(index)].ai.role=GuardSquadRole(role);guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_TACTICS"){
   if(f.size()!=5)throw std::runtime_error("malformed ACTOR_TACTICS record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index"),suppress=integer(f,3,"suppress fire");
   if(index<0||index>=int(map->actorTracks.size())||suppress<0||suppress>1)throw std::runtime_error("invalid actor tactics target or suppression");
   auto&ai=map->actorTracks[size_t(index)].ai;ai.suppress=suppress!=0;ai.grenades=integer(f,4,"grenade inventory");guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_PATROL"){
   // Optional |look: the bearing held at this post.
   if(f.size()!=7&&f.size()!=8)throw std::runtime_error("malformed ACTOR_PATROL record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");
   if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("patrol point has no actor");
   map->actorTracks[size_t(index)].ai.patrol.push_back({{number(f,3,"patrol x"),number(f,4,"patrol y")},number(f,5,"patrol height"),integer(f,6,"patrol pause")});
   if(given(7))map->actorTracks[size_t(index)].ai.patrol.back().look=number(f,7,"patrol look");
   guardTargets.emplace_back(integer(f,1,"map index"),index);
  }else if(tag=="ACTOR_HEALTH"){
   if(f.size()!=6||f[4].empty()||f[5].empty())throw std::runtime_error("malformed ACTOR_HEALTH record");auto map=mapAt(maps,f);int index=integer(f,2,"actor index");if(index<0||index>=int(map->actorTracks.size()))throw std::runtime_error("health has no actor");auto& t=map->actorTracks[size_t(index)];t.health=integer(f,3,"actor health");t.damageState=stateField(f[4]);t.deadState=stateField(f[5]);
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
   // Optional |aiRadius: how far the AI hears it (0 as its sound allows).
   if(f.size()<10||f.size()>13)throw std::runtime_error("malformed SEQUENCE_CUE record");auto map=mapAt(maps,f);int sound=integer(f,4,"cue sound");if(sound<0||sound>=int(Sound::Count))throw std::runtime_error("invalid cue sound");
   map->sequenceCues.push_back({stateField(f[2]),integer(f,3,"cue time"),Sound(sound),{number(f,5,"cue x"),number(f,6,"cue y")},number(f,7,"cue gain"),number(f,8,"cue pitch"),decode(f[9]),f.size()>=11&&!f[10].empty()?number(f,10,"cue height"):std::numeric_limits<float>::quiet_NaN()});
   if(f.size()>=12)map->sequenceCues.back().voice=decode(f[11]); // a voiced take (NpcVoiceClip::line)
   if(given(12))map->sequenceCues.back().aiRadius=number(f,12,"cue AI radius");
  }else if(tag=="HAZARD"){
   if(f.size()<10)throw std::runtime_error("malformed HAZARD record");auto map=mapAt(maps,f);int kind=integer(f,2,"hazard kind");if(kind<0||kind>int(Hazard::Kind::Anomaly))throw std::runtime_error("invalid hazard kind");
   map->hazards.push_back({Hazard::Kind(kind),number(f,3,"hazard x1"),number(f,4,"hazard y1"),number(f,5,"hazard x2"),number(f,6,"hazard y2"),number(f,7,"hazard bottom"),number(f,8,"hazard top"),number(f,9,"hazard damage")});
   // |state|value|invert|period|onTime|phase: switched like a LIGHT (state gate, timed window).
   {auto& h=map->hazards.back();if(given(10))h.enabledFlag=stateField(f[10]);if(given(11))h.enabledValue=integer(f,11,"hazard state value");if(given(12))h.invertFlag=boolean(f,12,"hazard invert");
    if(given(13))h.period=number(f,13,"hazard period");if(given(14))h.onTime=number(f,14,"hazard on time");if(given(15))h.phase=number(f,15,"hazard phase");}
  }else if(tag=="EMITTER"){
   // EMITTER|map|x|y|z|driftX|driftY|rise|rate|radius|growth|count: a smoke/vapour plume.
   if(f.size()!=12)throw std::runtime_error("malformed EMITTER record");auto map=mapAt(maps,f);
   ParticleEmitter e{{number(f,2,"emitter x"),number(f,3,"emitter y")},number(f,4,"emitter z"),{number(f,5,"emitter drift x"),number(f,6,"emitter drift y")},number(f,7,"emitter rise"),number(f,8,"emitter rate"),number(f,9,"emitter radius"),number(f,10,"emitter growth"),integer(f,11,"emitter count")};
   if(e.rate<=0||e.rate>5||e.radius<=0||e.radius>3||e.growth<0||e.growth>5||e.count<1||e.count>32||e.rise<0||e.rise>20)throw std::runtime_error("emitter parameters out of range");
   if(map->emitters.size()>=64)throw std::runtime_error("too many EMITTER records in one map");map->emitters.push_back(e);
  }else if(tag=="STAIR"){
   if(f.size()<11||f.size()>14)throw std::runtime_error("malformed STAIR record");auto map=mapAt(maps,f);map->stairs.push_back({
    number(f,2,"stair x1"),number(f,3,"stair y1"),number(f,4,"stair x2"),number(f,5,"stair y2"),number(f,6,"stair bottom"),number(f,7,"stair top"),
    integer(f,8,"stair steps"),boolean(f,9,"stair along y"),boolean(f,10,"stair ascending"),f.size()>11&&!f[11].empty()?boolean(f,11,"stair open underside"):false,f.size()>12&&!f[12].empty()?number(f,12,"stair tread thickness"):.12f,f.size()>13&&!f[13].empty()?boolean(f,13,"stair side rails"):false});
  }else if(tag=="CREATURE"){
   if(f.size()<6)throw std::runtime_error("malformed CREATURE record");auto map=mapAt(maps,f);int kind=integer(f,2,"creature kind");if(kind<0||kind>int(CreatureKind::Transformed))throw std::runtime_error("invalid creature kind");
   CreatureSpawn spawn{CreatureKind(kind),{number(f,3,"creature x"),number(f,4,"creature y")},number(f,5,"creature z")};
   // Optional: size (.5-2), health (0 the kind's own) and a wake state that
   // keeps it dormant, blind and deaf until set or until it is hurt.
   if(f.size()>6&&!f[6].empty())spawn.scale=number(f,6,"creature scale");
   if(f.size()>7&&!f[7].empty())spawn.health=number(f,7,"creature health");
   if(f.size()>8&&!f[8].empty())spawn.wakeState=stateField(f[8]);
   // A hunter name makes it a recurring animal (it cannot die; it runs, is
   // gone, and spawns sharing the name are it coming back); prey names an
   // actor on this map (track index) it comes out for first.
   if(f.size()>9&&!f[9].empty())spawn.hunter=stateField(f[9]);
   if(f.size()>10&&!f[10].empty())spawn.prey=integer(f,10,"creature prey");
   if(f.size()>11||spawn.scale<.5f||spawn.scale>2||spawn.health<0||spawn.health>5000||spawn.prey<-1||spawn.prey>4096)throw std::runtime_error("creature parameters out of range");
   map->creatureSpawns.push_back(spawn);
  }else if(tag=="PICKUP"){
   if(f.size()<5)throw std::runtime_error("malformed PICKUP record");auto map=mapAt(maps,f);int kind=integer(f,2,"pickup kind");if(kind<0||kind>int(PickupKind::HandgunAmmo))throw std::runtime_error("invalid pickup kind");
   map->pickupSpawns.push_back({{number(f,3,"pickup x"),number(f,4,"pickup y")},PickupKind(kind)});
   if(given(5))map->pickupSpawns.back().z=number(f,5,"pickup z"); // absolute height (empty: on the floor)
  }else if(tag=="CLUTTER"){
   if(f.size()<7)throw std::runtime_error("malformed CLUTTER record");auto map=mapAt(maps,f);int kind=integer(f,2,"clutter kind");if(kind<0||kind>7)throw std::runtime_error("invalid clutter kind");
   map->clutterSpawns.push_back({kind,{number(f,3,"clutter x"),number(f,4,"clutter y")},number(f,5,"clutter z"),number(f,6,"clutter yaw")});
   if(kind==7){if(f.size()!=14)throw std::runtime_error("model CLUTTER needs model, dimensions, pose and break state");auto&v=map->clutterSpawns.back();v.model=integer(f,7,"prop model");v.width=number(f,8,"prop width");v.depth=number(f,9,"prop depth");v.height=number(f,10,"prop height");v.pitch=number(f,11,"prop pitch");v.roll=number(f,12,"prop roll");v.breakState=stateField(f[13]);}
  }else if(tag=="BREAKABLE"){
   if(f.size()!=13&&f.size()!=15)throw std::runtime_error("malformed BREAKABLE record");auto map=mapAt(maps,f);
   Breakable v{number(f,2,"breakable x1"),number(f,3,"breakable y1"),number(f,4,"breakable x2"),number(f,5,"breakable y2"),number(f,6,"breakable bottom"),number(f,7,"breakable top"),integer(f,8,"breakable material"),number(f,9,"breakable health"),boolean(f,10,"breakable holds"),stateField(f[11]),stateField(f[12])};
   if(f.size()==15){v.fixture=integer(f,13,"breakable fixture");v.part=unsigned(integer(f,14,"breakable part"));}map->breakables.push_back(v);
  }else if(tag=="PIPE"){
   if(f.size()<9)throw std::runtime_error("malformed PIPE record");auto map=mapAt(maps,f);map->pipes.push_back({
    {number(f,2,"pipe x1"),number(f,3,"pipe y1")},{number(f,4,"pipe x2"),number(f,5,"pipe y2")},number(f,6,"pipe z"),number(f,7,"pipe radius"),number(f,8,"pipe end z"),f.size()>9?integer(f,9,"pipe material"):0});
   // 0-6 the PipeRun finishes; any other number (the campaign has 7, 8 and 20) is drawn as service pipe.
   if(map->pipes.back().material<0||map->pipes.back().material>99)throw std::runtime_error("invalid pipe material");
  }else if(tag=="LEAD"){
   // LEAD|map|ax|ay|az|bx|by|bz|length|radius[|material|live|state|prop|ox|oy|oz|loose|panel|socket|terminal]: prop is the map's
   // CLUTTER record (from 0, file order) it clips to; without one the far end is fixed at b, or loose.
   // Fields after radius may be empty or left off: material 7, not live, no state or prop, offset 0.
   // A test lead names its FIXTURE `panel` (from 0) and that model's `socket`, which places its anchor
   // (ax|ay|az may be empty), and its prop's `terminal`, which places its offset (ox|oy|oz empty).
   if(f.size()<10||f.size()>21)throw std::runtime_error("malformed LEAD record");auto map=mapAt(maps,f);Lead v;
   const bool panel=f.size()>18&&!f[18].empty();auto anchor=[&](size_t i,const char* label){return panel&&f[i].empty()?0.f:number(f,i,label);};
   v.anchor={anchor(2,"lead ax"),anchor(3,"lead ay"),anchor(4,"lead az")};v.end={number(f,5,"lead bx"),number(f,6,"lead by"),number(f,7,"lead bz")};
   v.length=number(f,8,"lead length");v.radius=number(f,9,"lead radius");if(f.size()>10&&!f[10].empty())v.material=integer(f,10,"lead material");
   if(f.size()>11&&!f[11].empty())v.live=boolean(f,11,"lead live");if(f.size()>12&&!f[12].empty())v.state=stateField(f[12]);
   if(f.size()>13&&!f[13].empty())v.prop=integer(f,13,"lead prop");
   auto offset=[&](size_t i,const char* label){return f[i].empty()?0.f:number(f,i,label);};
   if(f.size()>16)v.offset={offset(14,"lead ox"),offset(15,"lead oy"),offset(16,"lead oz")};else if(f.size()>14)throw std::runtime_error("lead offset needs all three of ox, oy and oz");
   if(f.size()>17&&!f[17].empty())v.loose=boolean(f,17,"lead loose");
   const std::string socket=f.size()>19?f[19]:"",terminal=f.size()>20?f[20]:"";
   if(panel!=!socket.empty()||(!terminal.empty()&&v.prop<0))throw std::runtime_error("a LEAD panel needs its socket, and a terminal its prop");
   if(panel||!terminal.empty()){if(panel&&v.loose)v.terminal={0,0,1};seats.push_back({map,map->leads.size(),panel?integer(f,18,"lead panel"):-1,socket,terminal});}
   map->leads.push_back(v);
  }else if(tag=="CHAIN"){
   // CHAIN|map|ax|ay|az|length|radius[|creature|ox|oy|oz]: a free hanging physics chain.
   // Without creature it hangs from ax|ay|az. With a CREATURE spawn index it hangs from that
   // creature at local offset (x right, y forward, z up from its feet).
   if(f.size()!=7&&f.size()!=11)throw std::runtime_error("malformed CHAIN record");auto map=mapAt(maps,f);Lead v;
   v.chain=true;v.loose=true;v.material=5;v.live=false;v.prop=-1;
   v.anchor={number(f,2,"chain ax"),number(f,3,"chain ay"),number(f,4,"chain az")};
   v.length=number(f,5,"chain length");v.radius=number(f,6,"chain radius");
   if(v.length<=0||v.radius<=0)throw std::runtime_error("invalid chain length or radius");
   if(f.size()==11){v.creature=integer(f,7,"chain creature");v.offset={number(f,8,"chain ox"),number(f,9,"chain oy"),number(f,10,"chain oz")};
    if(v.creature<0)throw std::runtime_error("chain creature out of range");}
   v.end={v.anchor.x,v.anchor.y+.05f,v.anchor.z-v.length};map->leads.push_back(v);
  }else if(tag=="WATER"){
   // WATER|map|x1|y1|x2|y2|bed|surface[|murk]: murk 0 clear to 1 black, standing water.
   if(f.size()<8||f.size()>9)throw std::runtime_error("malformed WATER record");auto map=mapAt(maps,f);map->waterVolumes.push_back({
    number(f,2,"water x1"),number(f,3,"water y1"),number(f,4,"water x2"),number(f,5,"water y2"),number(f,6,"water bed"),number(f,7,"water surface")});
   if(given(8)){const float murk=number(f,8,"water murk");if(!(murk>=0&&murk<=1))throw std::runtime_error("water murk is 0 to 1");map->waterVolumes.back().murk=murk;}
  }else if(tag=="COMPACTOR"){
   if(f.size()<9)throw std::runtime_error("malformed COMPACTOR record");auto map=mapAt(maps,f);map->compactors.push_back({
    number(f,2,"compactor x1"),number(f,3,"compactor y1"),number(f,4,"compactor x2"),number(f,5,"compactor y2"),number(f,6,"compactor bed"),number(f,7,"compactor raised"),number(f,8,"compactor period"),0});
   if(given(9))map->compactors.back().stopState=stateField(f[9]); // |stop: a state that halts it
  }else if(tag=="HUNG_SUIT"){
   // HUNG_SUIT|map|x|y|z|facing: an empty suit hung by its hood from a hook (absolute height).
   if(f.size()!=6)throw std::runtime_error("malformed HUNG_SUIT record");auto map=mapAt(maps,f);
   map->hungSuits.push_back({{number(f,2,"suit hook x"),number(f,3,"suit hook y"),number(f,4,"suit hook z")},number(f,5,"suit facing")});
  }else if(tag=="CRATE"){
   // CRATE|map|x|y|z|size: a heavy crate to shove; z above the map's first floor.
   if(f.size()!=6)throw std::runtime_error("malformed CRATE record");auto map=mapAt(maps,f);PushCrate crate;
   crate.position={number(f,2,"crate x"),number(f,3,"crate y")};crate.z=number(f,4,"crate z");crate.size=number(f,5,"crate size");
   if(crate.size<=0||crate.size>3)throw std::runtime_error("invalid crate size");map->pushCrates.push_back(crate);
  }else if(tag=="CRATE_PLATE"){
   // CRATE_PLATE|map|x|y|state: a floor plate; its state is 1 while a crate rests on it.
   if(f.size()!=5)throw std::runtime_error("malformed CRATE_PLATE record");auto map=mapAt(maps,f);
   map->cratePlates.push_back({{number(f,2,"plate x"),number(f,3,"plate y")},given(4)?stateField(f[4]):0});
  }else if(tag=="CRATE_RESET"){
   // CRATE_RESET|map|state: setting it sends every crate on the map home.
   if(f.size()!=3)throw std::runtime_error("malformed CRATE_RESET record");auto map=mapAt(maps,f);map->crateReset=given(2)?stateField(f[2]):0;
  }else if(tag=="NAV_HINT"){
   if(f.size()!=6)throw std::runtime_error("malformed NAV_HINT record");auto map=mapAt(maps,f);int hint=integer(f,5,"nav hint kind");
   if(hint<0||hint>1)throw std::runtime_error("invalid nav hint kind; use Path (0) or Cover (1)");
   map->navHints.push_back({{number(f,2,"nav hint x"),number(f,3,"nav hint y")},number(f,4,"nav hint z"),hint});
  }else throw std::runtime_error("unknown custom campaign record: "+tag);
  }catch(const std::exception& error){throw std::runtime_error("line "+std::to_string(lineNumber)+" ("+tag+"): "+error.what());}
 }
 for(auto& [map,index]:artUsers){auto& sign=map->signs[index];auto found=art.find(sign.art);
  if(found==art.end())throw std::runtime_error("SIGN record names SIGN_ART "+sign.art+", which is not in the file");sign.picture=found->second;}
 for(auto& seat:seats)if(const auto error=seatTestLead(*seat.map,seat.map->leads[seat.lead],seat.panel,seat.socket,seat.terminal);!error.empty())throw std::runtime_error("LEAD record: "+error);
 for(const auto& map:maps)for(size_t i=0;i<map->leads.size();++i){const auto&l=map->leads[i];if(l.chain&&l.creature>=0&&l.creature>=int(map->creatureSpawns.size()))throw std::runtime_error("CHAIN record "+std::to_string(i)+" creature out of range");}
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
  maps[i]->terrain=terrain;
  // Reject malformed geometry during discovery, while the caller can report
  // a per-file error, rather than crashing when a player selects the pack.
  World validated(int(i),maps[i]);
 }
 if(campaign->startMap<0||campaign->startMap>=int(maps.size()))throw std::runtime_error("custom campaign start map is invalid");
 campaign->maps.reserve(maps.size());for(auto& map:maps)campaign->maps.push_back(std::move(map));
 return campaign;
}
static std::wstring lowerExtension(const std::filesystem::path& path){
 auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});return ext;
}
static bool isRawMetalCampaignName(const std::string& file){return lowerExtension(std::filesystem::path(file))==L".rmc";}
bool isCustomCampaignFile(const std::filesystem::path& path){
 auto ext=lowerExtension(path);return ext==L".rmc"||ext==L".txt";
}
std::shared_ptr<const CustomCampaign> loadCustomCampaignFile(const std::filesystem::path& path){
 return loadCustomCampaignText(readText(path),path.filename().string());
}
std::shared_ptr<const CustomCampaign> loadEmbeddedExclusionZoneCampaign(){
 // Packed resource 346 is the Ashfall Exclusion Zone wasteland custom campaign TXT.
 auto bytes=loadResource(346);return loadCustomCampaignText(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()),"RawMetal-Ashfall-Exclusion-Zone.txt");
}
std::shared_ptr<const CustomCampaign> loadEmbeddedPrologueCampaign(){
 // Parsed once: New Game, checkpoints and tests share the same payload.
 static const auto prologue=[]{auto bytes=loadResource(398);return loadCustomCampaignText(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()),"RawMetal-Security-Office-Prologue.txt");}();
 return prologue;
}
std::shared_ptr<const CustomCampaign> builtInCampaignForKey(std::uint64_t key){
 auto prologue=loadEmbeddedPrologueCampaign();if(prologue->key==key)return prologue;
 auto exclusion=loadEmbeddedExclusionZoneCampaign();if(exclusion->key==key)return exclusion;
 return nullptr;
}
static bool isOfficialCampaignExport(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);if(!in)return false;
 std::string head(4096,'\0');in.read(head.data(),std::streamsize(head.size()));
 head.resize(size_t(std::max<std::streamsize>(0,in.gcount())));
 return head.find("CAMPAIGN_BASE|chunk=")!=std::string::npos;
}
std::vector<std::shared_ptr<const CustomCampaign>> loadCustomCampaignDirectory(const std::filesystem::path& directory,std::vector<std::string>* errors){
 std::vector<std::shared_ptr<const CustomCampaign>> result;std::error_code ec;
 if(std::filesystem::exists(directory,ec))for(auto& entry:std::filesystem::directory_iterator(directory,ec)){
  if(ec)break;if(!entry.is_regular_file()||!isCustomCampaignFile(entry.path()))continue;
  // An exported official level stays in Open from campaign. It is not a project.
  if(isOfficialCampaignExport(entry.path()))continue;
  try{result.push_back(loadCustomCampaignFile(entry.path()));}
  catch(const std::exception& error){if(errors)errors->push_back(entry.path().filename().string()+": "+error.what());}
 }
 // "Name.txt" left beside its renamed "Name.rmc" is the same pack; the renamed file wins.
 auto isPack=[](const auto& c){return isRawMetalCampaignName(c->sourceFile);};
 std::stable_sort(result.begin(),result.end(),[&](auto&a,auto&b){return isPack(a)&&!isPack(b);});
 std::vector<std::uint64_t> seen;
 std::erase_if(result,[&](const auto& c){if(std::find(seen.begin(),seen.end(),c->key)!=seen.end())return true;seen.push_back(c->key);return false;});
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
