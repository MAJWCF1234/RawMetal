#include "EntityTraits.h"
#include "World.h"
#include "../audio/SoundBank.h"
#include <algorithm>
#include <charconv>
#include <limits>
#include <cmath>
#include <stdexcept>

namespace retro {
namespace {
bool keyChar(unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-';}
std::string decodeValue(std::string_view value){
 auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return 10+c-'a';if(c>='A'&&c<='F')return 10+c-'A';return -1;};
 std::string out;out.reserve(value.size());
 for(size_t i=0;i<value.size();++i){if(value[i]=='%'&&i+2<value.size()){int a=hex(value[i+1]),b=hex(value[i+2]);if(a>=0&&b>=0){out.push_back(char(a*16+b));i+=2;continue;}}out.push_back(value[i]);}
 return out;
}
std::string lower(std::string_view s){std::string out(s);for(auto&c:out)c=char(std::tolower(static_cast<unsigned char>(c)));return out;}
[[noreturn]] void bad(const NamedField& f,const std::string& why){throw std::runtime_error("named field "+f.key+"="+f.value+": "+why);}
float number(const NamedField& f,float lo,float hi){
 const auto text=decodeValue(f.value);size_t used=0;float v=0;
 try{v=std::stof(text,&used);}catch(...){bad(f,"not a number");}
 if(used!=text.size()||!std::isfinite(v))bad(f,"not a number");
 if(v<lo||v>hi)bad(f,"must be "+std::to_string(lo).substr(0,std::to_string(lo).find('.')+3)+" to "+std::to_string(hi).substr(0,std::to_string(hi).find('.')+3));
 return v;
}
signed char flag(const NamedField& f){
 const auto v=lower(decodeValue(f.value));
 if(v=="1"||v=="true"||v=="yes"||v=="on")return 1;if(v=="0"||v=="false"||v=="no"||v=="off")return 0;bad(f,"use 1 or 0");
}
// r,g,b (each 0-4) or #rrggbb.
Point3 colour(const NamedField& f){
 const auto v=decodeValue(f.value);
 if(v.size()==7&&v[0]=='#'){unsigned rgb=0;for(size_t i=1;i<7;++i){char c=v[i];int d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?10+c-'a':c>='A'&&c<='F'?10+c-'A':-1;if(d<0)bad(f,"use r,g,b or #rrggbb");rgb=rgb*16+unsigned(d);}
  return {float((rgb>>16)&255)/255.f,float((rgb>>8)&255)/255.f,float(rgb&255)/255.f};}
 float c[3]{};size_t at=0;
 for(int i=0;i<3;++i){const size_t end=i<2?v.find(',',at):v.size();if(end==std::string::npos)bad(f,"use r,g,b (each 0-4) or #rrggbb");
  const auto part=v.substr(at,end-at);size_t used=0;try{c[i]=std::stof(part,&used);}catch(...){bad(f,"use r,g,b (each 0-4) or #rrggbb");}
  if(used!=part.size()||!(c[i]>=0&&c[i]<=4))bad(f,"each channel is 0 to 4");at=end+1;}
 return {c[0],c[1],c[2]};
}
}

bool namedFieldKey(std::string_view key){
 if(key.empty()||key.size()>48)return false;const unsigned char first=static_cast<unsigned char>(key[0]);
 // Lower case first: tile rows, titles and labels (upper case by habit) are never read as keys.
 if(!(first>='a'&&first<='z'))return false;
 return std::all_of(key.begin(),key.end(),[](char c){return keyChar(static_cast<unsigned char>(c));});
}
bool splitNamedField(std::string_view field,NamedField& out){
 const auto eq=field.find('=');if(eq==std::string_view::npos||!namedFieldKey(field.substr(0,eq)))return false;
 out.key=std::string(field.substr(0,eq));out.value=std::string(field.substr(eq+1));return true;
}
NamedFields peelNamedFields(std::vector<std::string>& f){
 size_t first=f.size();NamedField n;
 // A ROW is its 24 tiles and nothing more (a tile row may hold "=").
 const size_t floor=!f.empty()&&f[0]=="ROW"?5:1;
 while(first>floor&&splitNamedField(f[first-1],n))--first;
 NamedFields out;out.reserve(f.size()-first);
 for(size_t i=first;i<f.size();++i){splitNamedField(f[i],n);out.push_back(n);}
 f.resize(first);return out;
}
const NamedField* findNamedField(const NamedFields& fields,std::string_view key){
 for(const auto& f:fields)if(f.key==key)return &f;return nullptr;
}
bool validEntityName(std::string_view name){
 if(name.empty()||name.size()>48)return false;const unsigned char first=static_cast<unsigned char>(name[0]);
 if(!((first>='a'&&first<='z')||(first>='A'&&first<='Z')||first=='_'))return false;
 return std::all_of(name.begin(),name.end(),[](char ch){const unsigned char c=static_cast<unsigned char>(ch);return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});
}
bool validPropertyKey(std::string_view key){
 return !key.empty()&&key.size()<=32&&std::all_of(key.begin(),key.end(),[](char ch){const unsigned char c=static_cast<unsigned char>(ch);return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';});
}

bool parseFactionWord(std::string_view word,bool creature,AiFaction& out){
 const auto v=lower(word);
 if(v=="ally"||v=="facility"||v=="friend"){out=AiFaction::Facility;return true;}
 if(v=="enemy"||v=="hostile"){out=creature?AiFaction::Creature:AiFaction::Raider;return true;}
 if(v=="neutral"){out=AiFaction::Neutral;return true;}if(v=="player"){out=AiFaction::Player;return true;}
 if(v=="creature"){out=AiFaction::Creature;return true;}if(v=="raider"){out=AiFaction::Raider;return true;}
 if(v=="wary"||v=="truce"){out=AiFaction::Wary;return true;}return false;
}
const char* factionWord(AiFaction f){
 switch(f){case AiFaction::Player:return "PLAYER";case AiFaction::Facility:return "ALLY";case AiFaction::Creature:return "CREATURE";
  case AiFaction::Raider:return "ENEMY";case AiFaction::Neutral:return "NEUTRAL";case AiFaction::Wary:return "WARY";default:return "UNSET";}
}
namespace {constexpr const char* OrderWords[]={"idle","patrol","wander","guard","hunt","follow"};}
bool parseOrderWord(std::string_view word,int& out){
 const auto v=lower(word);for(int i=0;i<6;++i)if(v==OrderWords[i]){out=i;return true;}return false;
}
const char* orderWord(int order){return order>=0&&order<6?OrderWords[order]:"";}
bool parseWeaponWord(std::string_view word,int& out){
 const auto v=lower(word);
 if(v=="rifle"||v=="shotgun"||v=="1"){out=1;return true;}if(v=="handgun"||v=="pistol"||v=="2"){out=2;return true;}
 if(v=="none"||v=="unarmed"||v=="0"){out=0;return true;}return false;
}
bool parseRoleWord(std::string_view word,GuardSquadRole& out){
 constexpr const char* roles[]={"default","assault","engineer","sharpshooter","heavy","medic","rusher"};
 const auto v=lower(word);for(int i=0;i<7;++i)if(v==roles[i]){out=GuardSquadRole(i);return true;}
 if(v=="soldier"){out=GuardSquadRole::Assault;return true;}return false;
}
bool parseActorAiWord(std::string_view word,ActorAiMode& out){
 const auto v=lower(word);
 if(v=="scripted"){out=ActorAiMode::Scripted;return true;}if(v=="civilian"||v=="idle"){out=ActorAiMode::Civilian;return true;}
 if(v=="escort"||v=="follow"){out=ActorAiMode::Escort;return true;}
 if(v=="guard"||v=="patrol"||v=="hunt"||v=="wander"){out=ActorAiMode::Guard;return true;}return false;
}

const std::vector<std::string_view>& knownNamedKeys(){
 static const std::vector<std::string_view> keys{"name","mass","health","breakable","density","floats","friction","restitution","frozen","volume","soft","anchored","solid","material",
  "tint","scale","texture","emissive","impact","faction","ai","hp","weapon","role","display",
  // Read by the records they belong to: BREAKABLE glass (CustomCampaign.cpp) and GLAZING (Openings.cpp).
  "glaze","cracked","cols","spacing","rows","sill","head","frame","depth","look","door","door_head","pane","panes","infill","cut","wall",
  "shape","rebar","rubble","seed","toward",
  // Finishes, read by the STAIR and LAYER parsers (CustomCampaign.cpp finishField).
  "tread","riser","stringer","deck","edge","railing","trim","trimwidth",
  // Material turns (degrees), read by the STRUCT, PRIM, STAIR, LAYER and SURFACE parsers (CustomCampaign.cpp turnField).
  "turn","treadturn","deckturn","wallturn","floorturn","ceilingturn",
  // Footings (Footing.h), read by the STRUCT parser.
  "footing","snap","raise","base","cap","capwidth","steps","terrace",
  "drape",
  // PRIM options, read by the PRIM parser (CustomCampaign.cpp).
  "hand","rails","thick",
  // Underground (CaveField.h): stands on the carved floor under it, read by World::settleExclusionFacilityOnTerrain.
  "cave"};
 return keys;
}
std::string recordFieldKey(std::string_view tag,std::string_view parent,int ordinal){
 std::string k(tag);k+='/';k+=parent;k+='/';k+=std::to_string(ordinal);return k;
}

EntityTraits parseEntityTraits(std::string_view tag,const NamedFields& fields){
 EntityTraits t;t.tag=std::string(tag);const bool creature=tag=="CREATURE";
 for(const auto& f:fields){
  const auto& k=f.key;
  if(k=="name"){t.name=decodeValue(f.value);if(!validEntityName(t.name))bad(f,"a name is a letter or _ then letters, digits, _ or - (at most 48)");}
  else if(k.rfind("p.",0)==0){const auto key=k.substr(2);if(!validPropertyKey(key))bad(f,"a property key is letters, digits and _ (at most 32)");
   for(const auto& [have,v]:t.properties)if(have==key)bad(f,"property given twice");t.properties.emplace_back(key,decodeValue(f.value));}
  else if(k=="mass")t.mass=number(f,.01f,1000);
  else if(k=="health")t.health=number(f,0,100000);
  else if(k=="breakable")t.breakable=flag(f);
  else if(k=="density")t.density=number(f,10,20000);
  else if(k=="floats")t.floats=flag(f);
  else if(k=="friction")t.friction=tag=="JOINT"?number(f,0,5000):number(f,0,4);   // a joint's is N m (N on a slider), the JOINT parser's own 0 to 5000; a prop's surface is 0 to 4
  else if(k=="restitution")t.restitution=number(f,0,1);
  else if(k=="frozen")t.frozen=flag(f);
  else if(k=="volume")t.volume=number(f,0,5000);
  else if(k=="soft")t.soft=flag(f);
  else if(k=="anchored")t.anchored=flag(f);
  else if(k=="solid")t.solid=flag(f);
  else if(k=="cave"){if(f.value!="0"&&f.value!="1")bad(f,"use 1 (on the cave floor under it) or 0");}
  else if(k=="material"){constexpr const char* names[]={"paper","cardboard","plastic","wood","metal","glass","ceramic","electronic","cloth","organic"};
   const auto v=lower(decodeValue(f.value));t.material=-1;for(int i=0;i<int(PropMaterial::Count);++i)if(v==names[i])t.material=i;
   if(t.material<0)bad(f,"use paper, cardboard, plastic, wood, metal, glass, ceramic, electronic, cloth or organic");}
  else if(k=="tint"){t.tint=colour(f);t.tinted=true;}
  else if(k=="scale")t.scale=number(f,.05f,20);
  else if(k=="texture")t.texture=int(number(f,0,float(FacilityModelCount-1)));
  else if(k=="emissive")t.emissive=number(f,0,8);
  else if(k=="impact"){const auto* row=bankSound(decodeValue(f.value));if(!row)bad(f,"not a sound bank name (the AMBIENT names)");t.impact=int(row->sound);}
  else if(k=="faction"){AiFaction a{};if(!parseFactionWord(decodeValue(f.value),creature,a))bad(f,"use ally, enemy, neutral, player, facility, creature, raider or wary");t.faction=int(a);}
  else if(k=="ai"){
   if(tag=="ACTOR"){ActorAiMode m{};if(!parseActorAiWord(decodeValue(f.value),m))bad(f,"use scripted, civilian, escort or guard (or idle, follow, patrol, hunt)");t.aiMode=int(m);}
   else if(!parseOrderWord(decodeValue(f.value),t.order))bad(f,"use idle, patrol, wander, guard, hunt or follow");}
  else if(k=="hp")t.hp=number(f,1,100000);
  else if(k=="weapon"){if(!parseWeaponWord(decodeValue(f.value),t.weapon))bad(f,"use rifle, handgun or none");if(creature)bad(f,"creatures carry no weapon");}
  else if(k=="role"){GuardSquadRole r{};if(!parseRoleWord(decodeValue(f.value),r))bad(f,"use default, assault, engineer, sharpshooter, heavy, medic or rusher");t.role=int(r);}
  else if(k=="display"){t.display=decodeValue(f.value);if(t.display.empty()||t.display.size()>40)bad(f,"a display name is 1 to 40 characters");}
 }
 return t;
}

StateId entityPropertyState(int map,std::string_view mapName,std::string_view entity,std::string_view key){
 std::string s="entity/";s+=std::to_string(map);s+='/';s+=mapName;s+='/';s+=entity;s+='/';s+=key;return stateId(s);
}
StateId entityPropertySeedState(StateId property){return stateId("entity_seeded/"+std::to_string(property));}
int entityPropertyValue(std::string_view text){
 const auto v=lower(text);if(v=="true"||v=="yes"||v=="on")return 1;
 int out=0;auto r=std::from_chars(text.data(),text.data()+text.size(),out);
 if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size())return 0;return std::clamp(out,-100000,100000);
}

// The map as it plays: the authored map with each record's overrides applied to
// copies of the things they change. Authored data stays as written (the exporter
// and the Builder write it back), so a size factor is never applied twice.
std::shared_ptr<const AuthoredMapData> playedMap(std::shared_ptr<const AuthoredMapData> map){
 if(!map||(!map->entityTraits&&map->glazings.empty()&&map->breaches.empty()))return map;
 auto out=std::make_shared<AuthoredMapData>(*map);
 // Window walls cut and fill the played copy's walls (Openings.cpp); the authored map keeps the records.
 applyOpenings(*out);
 for(auto& f:out->fixtures)if(f.traits){const auto& t=*f.traits;
  if(t.scale!=1){f.width*=t.scale;f.depth*=t.scale;f.height*=t.scale;}
  if(t.anchored>=0)f.anchored=t.anchored>0;if(t.solid>=0)f.solid=t.solid>0;}
 for(auto& c:out->clutterSpawns)if(c.traits&&c.traits->scale!=1&&c.kind==ClutterModelKind){const float s=c.traits->scale;c.width*=s;c.depth*=s;c.height*=s;}
 for(auto& c:out->creatureSpawns)if(c.traits){const auto& t=*c.traits;
  if(t.scale!=1)c.scale=std::clamp(c.scale*t.scale,.25f,4.f);if(t.hp>0)c.health=t.hp;}
 for(size_t i=0;i<out->actorTracks.size();++i){auto& a=out->actorTracks[i];if(!a.traits)continue;const auto& t=*a.traits;
  if(t.scale!=1)a.scale*=t.scale;
  if(t.aiMode>=0){a.ai.mode=ActorAiMode(t.aiMode);
   // A man turned guard holds the spot he was authored at (as a summoned one does).
   if(a.ai.mode==ActorAiMode::Guard&&a.ai.patrol.empty()){const auto& k=!a.keys.empty()?a.keys.front():!a.idleKeys.empty()?a.idleKeys.front():ActorKey{};
    a.ai.patrol.push_back({k.position,k.z,60000,k.yaw});}}
  if(t.faction>=0)a.ai.faction=AiFaction(t.faction);
  if(t.role>=0)a.ai.role=GuardSquadRole(t.role);
  if(t.weapon>=0){a.ai.armed=t.weapon>0;if(t.weapon>0)a.ai.weapon=t.weapon;}
  if(t.hp>0){a.health=int(std::lround(t.hp));
   if(!a.damageState)a.damageState=actorTrackState(a.timerState,i,"damage");if(!a.deadState)a.deadState=actorTrackState(a.timerState,i,"dead");}}
 return out;
}
}
