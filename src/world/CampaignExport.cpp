#include "CampaignExport.h"
#include "CampaignMaps.h"
#include "CustomCampaign.h"
#include "../core/CampaignAdmin.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#endif

namespace retro {
namespace {
std::atomic<bool> devCampaign{false};

// ---- Chapters and districts (tools/campaign/plan.py; 0-33 predate the plan) ----
struct District {int first,last,chapter;const char* title;};
constexpr District Districts[]={
 {179,179,1,"PROLOGUE"},{0,9,1,"THE PLANT"},{32,33,1,"THE PLANT"},{10,31,1,"FREIGHT DISTRICT"},
 {34,40,1,"POWER DISTRIBUTION"},{41,46,1,"MACHINE HALL AND AIR"},{47,52,1,"WATER AND COOLING"},{53,56,1,"MAINTENANCE DISTRICT"},
 {57,67,1,"THE RESTRICTED PERIMETER"},
 {68,70,2,"RESEARCH FRONT"},{71,78,2,"DEPTHWORKS MEDICAL"},{79,84,2,"ANALYSIS LABORATORIES"},{85,93,2,"ENERGY TEST COMPLEX"},
 {94,103,2,"CONTAINMENT"},{104,109,2,"DEEP RESEARCH"},
 {110,112,3,"MINEHEAD"},{113,120,3,"UPPER MINE"},{121,127,3,"LOWER MINE"},{128,133,3,"DEEP BORE"},
 {134,143,4,"THE WILD CAVERNS"},{144,148,4,"THE SCRAP FIELDS"},{149,161,4,"THE SCRAP CITY"},{162,178,4,"THE DEEP"}};

// ---- Behaviour written in C++ outside a map's records, by chunk ----
// It stays with the chunk index (so a campaign override keeps it) but does not
// travel into a custom map, and some of it leans on record order or fixed
// coordinates, which an edit must leave alone.
struct ChunkBehaviour {int first,last;const char* note;};
constexpr ChunkBehaviour Behaviours[]={
 {0,1,"C++ supplies this chunk's floor wetness while the map leaves it unset (World::wetness); a custom map gets the default 0.3."},
 {0,2,"C++ picks this chunk's floor and wall textures while the map sets no SURFACE (Scene3D)."},
 {0,0,"Door order matters: chunk 1 pairs its entry door with this chunk's last door (residency group -1 streaming)."},
 {1,2,"Door order matters: residency group -1 pairs the transfer door (last) and entry door (first) with the neighbouring chunks."},
 {3,3,"The lift and reactor are C++ (Lift.cpp, Reactor.cpp): the cab is fixed at x/y 10-14, terminal 1 is the cab console, the last light is the cab lamp, the auth disk sits at 6,21, the hazmat corpse at 8.9,8.2; states lift_crashed, reactor_bulkhead_released."},
 {3,5,"Scene3D's default walls start at z -9 for this chunk."},
 {5,5,"Door order matters: chunk 6 pairs its entry door with this chunk's last door (residency group -1 streaming)."},
 {5,9,"The HUD shows a C++ map label here when no ZONE names the spot."},
 {6,9,"Door order matters: residency group -1 pairs the transfer door (last) and entry door (first) with the neighbouring chunks."},
 {6,6,"Bench-monitor light occlusion uses a fixed yaw of -90 degrees here (World.cpp), whatever the terminal's yaw."},
 {6,9,"C++ gives this chunk the service floor texture while the map sets no SURFACE."},
 {10,10,"Door order matters: chunk 9 pairs its transfer door with this chunk's first door (residency group -1 streaming)."},
 {14,15,"The receiving breach is C++ (Script.cpp updateReceivingBreach): it picks squad receiving_cleanup by chunk, spawns six creatures at fixed spots, sends the survivor to 12,18 in chunk 15 and closes every door at y 23.5."},
 {32,33,"Part of the Reactor Complex: chunk 3's reactor C++ reads this chunk's puzzle state."},
 {57,57,"The terminal that sets cctv_radar gets its handset text from C++ (Movement.cpp, Radar.cpp)."},
 {179,179,"New Game starts here (Settings.cpp); in the campaign this map is itself the embedded prologue file (resource 398)."}};

// ---- Number and text fields ----
std::string num(float v){
 if(std::isnan(v))return "";char buffer[48];auto r=std::to_chars(buffer,buffer+sizeof(buffer),v);return std::string(buffer,r.ptr);
}
std::string encode(std::string_view s){
 static const char* hex="0123456789ABCDEF";std::string out;out.reserve(s.size());
 for(unsigned char c:s){if(c=='%'||c=='|'||c<32||c==127){out+='%';out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}
 return out;
}
// Fields the loader reads as written (ZONE names, rows, lead port names): no escapes.
std::string verbatim(std::string_view s,std::vector<std::string>& notes,const std::string& where){
 std::string out(s);bool changed=false;for(auto& c:out)if(c=='|'||c=='\r'||c=='\n'){c='/';changed=true;}
 if(changed)notes.push_back(where+": a '|' or line break in the text became '/'");
 return out;
}

// ---- State names ----
const std::unordered_map<StateId,std::string>& stateNames(){
 static const auto names=[]{
  static const char* const base[]={
#include "CampaignStateNames.inc"
  };
  std::unordered_map<StateId,std::string> m;m.reserve(std::size(base)*12);
  auto add=[&](const std::string& n){if(n.size()<=96)m.emplace(stateId(n),n);};
  for(const char* b:base)add(b);
  // What the map builders append to a literal: a step suffix, or a running number.
  static const char* const suffixes[]={"_done","_go","_cycled","_damage","_dead","_index","_kind","_level","_ms","_clock"};
  for(const char* b:base){std::string s=b;
   for(const char* x:suffixes)add(s+x);
   if(s.back()=='_'||s.back()=='/')for(int i=0;i<64;++i)add(s+std::to_string(i));}
  return m;}();
 return names;
}

// ---- Record writer ----
struct Rec {
 std::string s;
 explicit Rec(const char* tag):s(tag){}
 Rec& f(const std::string& v){s+='|';s+=v;return *this;}
 Rec& n(float v){return f(num(v));}
 Rec& i(long long v){return f(std::to_string(v));}
 Rec& b(bool v){return f(v?"1":"0");}
 Rec& t(std::string_view text){return f(encode(text));}
 // A state where an empty field means none.
 Rec& st(StateId id){return f(id?stateReference(id):"");}
 // A state the loader always hashes (an empty field is not none): zero is "#0".
 Rec& sth(StateId id){return f(id?stateReference(id):"#0");}
 // Drop trailing empty fields (the loader reads them as absent).
 // Past the first `keep` fields (the tag counts), trailing empty ones are left off: the loader reads them as absent.
 std::string trimmed(size_t keep)const{std::string r=s;
  while(!r.empty()&&r.back()=='|'&&size_t(std::count(r.begin(),r.end(),'|'))>=keep)r.pop_back();return r;}
};

bool sameFloat(float a,float b){return a==b||(std::isnan(a)&&std::isnan(b));}
}

std::filesystem::path campaignWorkspace(){
 static const std::filesystem::path root=[]()->std::filesystem::path{
#ifdef _WIN32
  std::wstring exe(32768,L'\0');exe.resize(GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size())));std::error_code ec;
  auto dir=std::filesystem::path(exe).parent_path();
  for(int up=0;up<3&&!dir.empty();++up,dir=dir.parent_path())if(std::filesystem::exists(dir/"src"/"world"/"CampaignChapters.cpp",ec))return dir;
#endif
  return {};}();
 return root;
}
bool publicRelease(){
 static const bool marked=[]{
#ifdef _WIN32
  std::wstring exe(32768,L' ');exe.resize(GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size())));std::error_code ec;
  return std::filesystem::exists(std::filesystem::path(exe).parent_path()/"public-release.txt",ec);
#else
  return false;
#endif
 }();
 return marked;
}
// A public release opens sealed levels only to a whitelisted admin who unlocked them on this
// machine (core/CampaignAdmin.h); --dev-campaign does not open them there.
bool devCampaignTools(){return !publicRelease()||!admin::unlocked().empty();}
void setDevCampaignTools(bool on){devCampaign=on;}
CampaignChunkLabel campaignChunkLabel(int chunk){
 for(const auto& d:Districts)if(chunk>=d.first&&chunk<=d.last)return {d.chapter,d.title};
 return {1,""};
}
std::vector<int> campaignChunksInPlayOrder(){
 std::vector<int> order;for(const auto& d:Districts)for(int c=d.first;c<=d.last;++c)order.push_back(c);
 return order;
}
std::string stateReference(StateId id){
 const auto& names=stateNames();auto it=names.find(id);
 return it!=names.end()?it->second:"#"+std::to_string(id);
}
std::string campaignChunkFileName(int chunk){
 char number[8];std::snprintf(number,sizeof(number),"%03d ",chunk);
 std::string name=CampaignMapNames.at(size_t(chunk));
 for(auto& c:name)if(std::strchr("\\/:*?\"<>|",c))c='-';
 // "MEDICAL - TRIAGE" reads better than "MEDICAL - TRIAGE" with doubled spaces.
 std::string out;for(char c:name){if(c==' '&&!out.empty()&&out.back()==' ')continue;out+=c;}
 return number+out+".rmc";
}

RmcExport exportRmc(const std::string& campaignName,const std::vector<RmcMapSource>& sources,int startMap,const StormCycle& storm){
 RmcExport result;std::ostringstream data;auto& notes=result.notes;
 // keep: how many fields (tag included) the record always has; 0 writes every field.
 auto line=[&](const Rec& r,size_t keep=0){data<<(keep?r.trimmed(keep):r.s)<<'\n';};
 data<<Rec("CAMPAIGN").t(campaignName).i(startMap).s<<'\n';
 if(storm.active())line(Rec("STORM").i(storm.calmMs).i(storm.warningMs).i(storm.stormMs).n(storm.damage).n(storm.ambientAsh));
 // Terrain is campaign-wide: the first map that has it speaks for the file.
 for(const auto& source:sources)if(source.map&&source.map->terrain){const auto& t=*source.map->terrain;
  data<<"\n# ==== Campaign-wide terrain (world metres) ====\n";
  line(Rec("TERRAIN").i(t.seed).n(t.base).n(t.amplitude).n(t.scale).n(t.roughness));
  for(const auto& flat:t.flats)line(Rec("TERRAIN_FLAT").n(flat.x1).n(flat.y1).n(flat.x2).n(flat.y2).n(flat.height).n(flat.blend));
  if(t.horizonDepth>0||!t.horizonModels.empty()){std::string models;for(size_t k=0;k<t.horizonModels.size();++k)models+=(k?",":"")+std::to_string(t.horizonModels[k]);
   line(Rec("HORIZON").n(t.horizonDepth).n(t.horizonSpacing).n(t.horizonRise).f(models));}
  for(const auto& g:t.ground)line(Rec("GROUND").n(g.x1).n(g.y1).n(g.x2).n(g.y2).i(g.kind).n(g.edge));
  for(const auto& other:sources)if(other.map&&other.map->terrain&&other.map->terrain!=source.map->terrain)notes.push_back("map "+other.map->name+": its own terrain differs from the campaign's; the file carries one terrain for every map");
  break;}
 std::set<StateId> objectives,events;
 for(size_t index=0;index<sources.size();++index){
  const auto& m=*sources[index].map;const int chunk=sources[index].chunk;const auto mi=(long long)index;
  const std::string who="map "+std::to_string(index)+" ("+m.name+(chunk>=0?", campaign chunk "+std::to_string(chunk):std::string())+")";
  auto note=[&](const std::string& text){notes.push_back(who+": "+text);};
  if(chunk>=0)for(const auto& b:Behaviours)if(chunk>=b.first&&chunk<=b.last)note(b.note);
  data<<"\n# ==== Map "<<index<<": "<<m.name;if(chunk>=0)data<<" (campaign chunk "<<chunk<<", "<<campaignChunkLabel(chunk).district<<(campaignChunkSealed(chunk)?", sealed":"")<<")";data<<" ====\n"; // the builder reads this line
  const auto& d=m.definition;
  {Rec r("MAP");r.i(mi).t(m.name).n(d.origin.x).n(d.origin.y).n(d.playerStart.x).n(d.playerStart.y).n(d.spawnHeight).i(int(d.environment)).n(d.ceiling).n(d.ambient).t(m.skybox)
    .b(m.openNorth).b(m.openSouth).b(m.openWest).b(m.openEast).b(d.lift).i(d.residencyGroup).i(int(d.floorMaterial)).i(int(d.musicCue)).n(d.spawnYaw).n(d.spawnPitch).b(d.startUnarmed);
   if(d.haze[3]>=0)r.n(d.haze[0]).n(d.haze[1]).n(d.haze[2]).n(d.haze[3]);else r.f("").f("").f("").f("");
   r.f(d.wetness>=0?num(d.wetness):"");line(r,17);}
  for(int group:m.visibleResidencyGroups)line(Rec("VISIBLE_GROUP").i(mi).i(group));
  if(m.wallSurface!=SurfaceDefault||m.floorSurface!=SurfaceDefault||m.ceilingSurface!=SurfaceDefault)line(Rec("SURFACE").i(mi).i(m.wallSurface).i(m.floorSurface).i(m.ceilingSurface));
  if(m.layers.size()>32)note("only the first 32 of "+std::to_string(m.layers.size())+" floor layers fit the format");
  for(size_t l=0;l<m.layers.size()&&l<32;++l){const auto& layer=m.layers[l];
   if(layer.name.empty())note("layer "+std::to_string(l)+" has no name; the loader calls it \"Custom floor\"");
   line(Rec("LAYER").i(mi).i((long long)l).t(layer.name).n(layer.elevation).n(layer.thickness));
   for(size_t row=0;row<24;++row)line(Rec("ROW").i(mi).i((long long)l).i((long long)row).f(verbatim(layer.rows[row],notes,who+" layer "+std::to_string(l)+" row "+std::to_string(row))));}
  for(const auto& door:m.doors){Rec r("DOOR");r.i(mi).n(door.left).n(door.right).n(door.y).n(door.z).b(door.entry).b(door.transfer).b(door.swinging).b(door.requireEnemiesClear).i(door.sign).st(door.requireState).i(door.requireValue);
   r.f(door.requireControl?"1":"").f(door.swing!=1?num(door.swing):"").f(door.maxOpen!=1?num(door.maxOpen):"").f(door.open!=0?num(door.open):"").f(door.opening?"1":"");line(r,13);}
  for(const auto& s:m.structures){Rec r("STRUCT");r.i(mi).n(s.x1).n(s.y1).n(s.x2).n(s.y2).n(s.bottom).n(s.top).b(s.rail).i(s.material).f(s.solid?"":"0").f(s.shape?std::to_string(s.shape):"").f(s.facing?std::to_string(s.facing):"");line(r,10);}
  for(const auto& x:m.fixtures){Rec r("FIXTURE");r.i(mi).i(x.model).n(x.position.x).n(x.position.y).n(x.base).n(x.width).n(x.depth).n(x.height).n(x.yaw).b(x.solid);
   if(!x.visible)r.n(x.core).b(false);else if(x.core!=1)r.n(x.core);line(r);}
  for(const auto& p:m.props)line(Rec("PROP").i(mi).i(p.kind).n(p.position.x).n(p.position.y).n(p.height).n(p.footprint).n(p.yaw).n(p.halfSize.x).n(p.halfSize.y).n(p.base));
  // A test lead's pilot lamp is made again when its LEAD is seated on load.
  std::set<int> pilots;for(const auto& lead:m.leads)if(lead.panel>=0&&lead.lamp>=0)pilots.insert(lead.lamp);
  for(int li=0;li<int(m.lights.size());++li){if(pilots.count(li))continue;const auto& l=m.lights[size_t(li)];Rec r("LIGHT");
   r.i(mi).n(l.position.x).n(l.position.y).n(l.z).i(int(l.mount)).n(l.yaw).n(l.intensity).n(l.range).n(l.kelvin).n(l.tint).st(l.enabledState).f(l.enabledValue!=1?std::to_string(l.enabledValue):"").f(l.invert?"1":"")
    .f(l.period?num(l.period):"").f(l.onTime?num(l.onTime):"").f(l.phase?num(l.phase):"").f(l.flicker?num(l.flicker):"");
   if(l.fitting>=0)r.i(l.fitting).n(l.fittingWidth).n(l.fittingDepth).n(l.fittingHeight);else r.f("").f("").f("").f("");
   if(l.gel.x!=1||l.gel.y!=1||l.gel.z!=1)r.n(l.gel.x).n(l.gel.y).n(l.gel.z);else r.f("").f("").f("");
   r.f(l.shadows?"":"0").f(l.prop>=0?std::to_string(l.prop):"");line(r,5);}
  for(const auto& z:m.darkZones)line(Rec("DARK").i(mi).n(z.x1).n(z.y1).n(z.x2).n(z.y2).n(z.bottom).n(z.top).st(z.state).i(z.value).b(z.invert).b(z.jamFlashlight),6);
  for(const auto& z:m.zones){Rec r("ZONE");r.i(mi).n(z.x1).n(z.y1).n(z.x2).n(z.y2).f(verbatim(z.name,notes,who+" zone")).n(z.bottom).n(z.top).f(verbatim(z.title,notes,who+" zone title"));
   if(z.haze[3]>=0)r.n(z.haze[0]).n(z.haze[1]).n(z.haze[2]).n(z.haze[3]);
   if(z.name.size()>64||z.title.size()>64)note("zone \""+z.name+"\": names and titles over 64 characters do not load");line(r,7);}
  for(const auto& t:m.terminals){Rec r("TERMINAL");r.i(mi).n(t.position.x).n(t.position.y).n(t.z).b(t.control).t(t.title).t(t.line1).t(t.line2).st(t.activateState).b(t.toggleState).st(t.requireState).n(t.yaw)
    .f(t.readable!=Readable::Screen?std::to_string(int(t.readable)):"").f(t.taped?"1":"").f(t.surface!=-1?num(t.surface):"").f(t.reactorAction?std::to_string(t.reactorAction):"");line(r,13);}
  for(const auto& s:m.signs)line(Rec("SIGN").i(mi).n(s.position.x).n(s.position.y).n(s.z).n(s.width).n(s.height).n(s.yaw).t(s.title).t(s.subtitle).i(s.accent&0xffffffu));
  {const auto& c=m.cargoLift;if(c.x1!=c.x2||c.y1!=c.y2||c.lower!=c.upper)
   line(Rec("CARGO_LIFT").i(mi).n(c.x1).n(c.y1).n(c.x2).n(c.y2).n(c.lower).n(c.upper).n(c.speed).sth(c.callState).sth(c.releaseState).sth(c.positionState).sth(c.downState).sth(c.arrivedState).sth(c.descendedState));}
  for(const auto& s:m.timedSequences){Rec r("TIMED_SEQUENCE");r.i(mi).sth(s.timerState).sth(s.finishState).n(s.x1).n(s.y1).n(s.x2).n(s.y2).n(s.bottom).n(s.top).i(s.durationMs).i(s.finishAtMs).i(s.soundIntervalMs).i(s.soundUntilMs).i(int(s.tickSound)).n(s.gain).n(s.pitch).b(s.loop);
   if(s.soundAt.x>=0||s.soundAt.y>=0)r.n(s.soundAt.x).n(s.soundAt.y);else r.f("").f("");if(s.soundAt.x<0&&s.soundAt.y>=0)note("a timed sequence's sound spot has x < 0 and is read as unset");r.f(num(s.soundZ));line(r,18);}
  for(size_t a=0;a<m.actorTracks.size();++a){const auto& t=m.actorTracks[a];const auto& ai=t.ai;const auto ai_=(long long)a;
   Rec r("ACTOR");r.i(mi).sth(t.timerState).i(int(t.visual)).n(t.scale).b(t.loop).b(t.tool).i(ai_);if(ai.dropsSidearm)r.b(true);line(r);
   if(ai.mode!=ActorAiMode::Scripted||ai.enableState||ai.speed!=1.4f||ai.followDistance!=2.f||ai.dangerRange!=6.f)line(Rec("ACTOR_AI").i(mi).i(ai_).i(int(ai.mode)).st(ai.enableState).n(ai.speed).n(ai.followDistance).n(ai.dangerRange));
   if(ai.faction!=AiFaction::Unspecified||ai.squad)line(Rec("ACTOR_FACTION").i(mi).i(ai_).i(int(ai.faction)).st(ai.squad));
   if(ai.mode==ActorAiMode::Guard)line(Rec("ACTOR_GUARD").i(mi).i(ai_).n(ai.sightRange).n(ai.sightDot).i(ai.recognitionMs).i(ai.searchMs).st(ai.alertState));
   if(ai.armed)line(Rec("ACTOR_WEAPON").i(mi).i(ai_).i(ai.weapon).i(ai.aimMs).i(ai.recoveryMs).n(ai.attackRange).n(ai.attackDamage));
   else if(ai.weapon!=1)note("actor "+std::to_string(a)+" carries weapon "+std::to_string(ai.weapon)+" unarmed; only an armed actor's weapon is written");
   if(t.voice)line(Rec("ACTOR_VOICE").i(mi).i(ai_).i(t.voice));
   if(ai.stance)line(Rec("ACTOR_STANCE").i(mi).i(ai_).sth(ai.stance));
   if(t.talk)line(Rec("ACTOR_TALK").i(mi).i(ai_).sth(t.talk));
   if(ai.role!=GuardSquadRole::Default)line(Rec("ACTOR_ROLE").i(mi).i(ai_).i(int(ai.role)));
   if(ai.suppress||ai.grenades)line(Rec("ACTOR_TACTICS").i(mi).i(ai_).b(ai.suppress).i(ai.grenades));
   for(const auto& p:ai.patrol){Rec pr("ACTOR_PATROL");pr.i(mi).i(ai_).n(p.position.x).n(p.position.y).n(p.z).i(p.pauseMs);if(!std::isnan(p.look))pr.n(p.look);line(pr);}
   if(t.health||t.damageState||t.deadState)line(Rec("ACTOR_HEALTH").i(mi).i(ai_).i(t.health).sth(t.damageState).sth(t.deadState));
   if(t.suspensionTop!=-999)line(Rec("ACTOR_SUSPEND").i(mi).i(ai_).n(t.suspensionTop));
   if(t.platform)line(Rec("ACTOR_PLATFORM").i(mi).i(ai_).n(t.footprint.x).n(t.footprint.y).n(t.thickness));
   else if(t.footprint.x!=1||t.footprint.y!=1||t.thickness!=.2f)note("actor "+std::to_string(a)+" has a platform footprint but is no platform; it is not written");
   if(t.idleUntilMs||t.approachUntilMs)line(Rec("ACTOR_PRELUDE").i(mi).i(ai_).i(t.idleUntilMs).i(t.approachUntilMs));
   for(const auto& k:t.keys)line(Rec("ACTOR_KEY").i(mi).i(ai_).i(k.timeMs).n(k.position.x).n(k.position.y).n(k.z).n(k.yaw).i(k.clip).n(k.phase).i(k.lookAtActor));
   for(const auto& k:t.idleKeys)line(Rec("ACTOR_IDLE_KEY").i(mi).i(ai_).i(k.timeMs).n(k.position.x).n(k.position.y).n(k.z).n(k.yaw).i(k.clip).n(k.phase).i(k.lookAtActor));}
  for(size_t s=0;s<m.timedSequences.size();++s){const auto& q=m.timedSequences[s];if(q.sightActor!=-1||q.sightDistance!=30)line(Rec("SEQUENCE_SIGHT").i(mi).i((long long)s).i(q.sightActor).n(q.sightDistance));}
  for(const auto& c:m.sequenceCues){Rec r("SEQUENCE_CUE");r.i(mi).sth(c.timerState).i(c.timeMs).i(int(c.sound)).n(c.position.x).n(c.position.y).n(c.gain).n(c.pitch).t(c.caption).f(num(c.z)).t(c.voice).f(c.aiRadius?num(c.aiRadius):"");line(r,10);}
  for(const auto& node:m.dialogue){line(Rec("DIALOGUE").i(mi).sth(node.id).t(node.speaker).t(node.line).t(node.voice));
   for(const auto& reply:node.replies){std::string sets;for(const auto& [state,value]:reply.sets)sets+=(sets.empty()?"":";")+stateReference(state)+"="+std::to_string(value);
    line(Rec("DIALOGUE_REPLY").i(mi).sth(node.id).t(reply.text).st(reply.next).st(reply.require).i(reply.requireValue).st(reply.hide).t(sets).f(reply.stance>=0?std::to_string(reply.stance):""));}}
  for(const auto& h:m.hazards){Rec r("HAZARD");r.i(mi).i(int(h.kind)).n(h.x1).n(h.y1).n(h.x2).n(h.y2).n(h.bottom).n(h.top).n(h.damagePerSecond).st(h.enabledFlag).f(h.enabledValue!=1?std::to_string(h.enabledValue):"").f(h.invertFlag?"1":"")
    .f(h.period?num(h.period):"").f(h.onTime?num(h.onTime):"").f(h.phase?num(h.phase):"");line(r,10);}
  for(const auto& e:m.emitters){line(Rec("EMITTER").i(mi).n(e.position.x).n(e.position.y).n(e.z).n(e.drift.x).n(e.drift.y).n(e.rise).n(e.rate).n(e.radius).n(e.growth).i(e.count));
   if(e.rate<=0||e.rate>5||e.radius<=0||e.radius>3||e.growth<0||e.growth>5||e.count<1||e.count>32||e.rise<0||e.rise>20)note("an emitter is outside the EMITTER ranges and will not load");}
  for(const auto& s:m.stairs)line(Rec("STAIR").i(mi).n(s.x1).n(s.y1).n(s.x2).n(s.y2).n(s.bottom).n(s.top).i(s.steps).b(s.alongY).b(s.ascending).f(s.openUnderside?"1":"").f(s.treadThickness!=.12f?num(s.treadThickness):"").f(s.sideRails?"1":""),11);
  for(const auto& c:m.creatureSpawns)line(Rec("CREATURE").i(mi).i(int(c.kind)).n(c.position.x).n(c.position.y).n(c.z).f(c.scale!=1?num(c.scale):"").f(c.health?num(c.health):"").st(c.wakeState),6);
  for(const auto& p:m.pickupSpawns)line(Rec("PICKUP").i(mi).i(int(p.kind)).n(p.position.x).n(p.position.y).f(p.z!=-999?num(p.z):""),5);
  for(size_t c=0;c<m.clutterSpawns.size();++c){const auto& v=m.clutterSpawns[c];Rec r("CLUTTER");r.i(mi).i(v.kind).n(v.position.x).n(v.position.y).n(v.z).n(v.yaw);
   if(v.kind==ClutterModelKind)r.i(v.model).n(v.width).n(v.depth).n(v.height).n(v.pitch).n(v.roll).sth(v.breakState);
   else if(v.model!=-1||v.width||v.depth||v.height||v.pitch||v.roll||v.breakState)note("loose item "+std::to_string(c)+" (kind "+std::to_string(v.kind)+") carries model fields only a model prop keeps");
   line(r);}
  for(const auto& v:m.breakables){Rec r("BREAKABLE");r.i(mi).n(v.x1).n(v.y1).n(v.x2).n(v.y2).n(v.bottom).n(v.top).i(v.material).n(v.health).b(v.holds).sth(v.state).sth(v.trigger);if(v.fixture>=0)r.i(v.fixture).i((long long)v.part);line(r);}
  for(const auto& p:m.pipes)line(Rec("PIPE").i(mi).n(p.start.x).n(p.start.y).n(p.end.x).n(p.end.y).n(p.z).n(p.radius).n(p.endZ).i(p.material));
  {const auto ports=leadPorts();
   for(size_t li=0;li<m.leads.size();++li){const auto& l=m.leads[li];
    if(l.chain){Rec r("CHAIN");r.i(mi).n(l.anchor.x).n(l.anchor.y).n(l.anchor.z).n(l.length).n(l.radius);
     if(l.creature>=0)r.i(l.creature).n(l.offset.x).n(l.offset.y).n(l.offset.z);line(r,l.creature>=0?11:7);continue;}
    Rec r("LEAD");
    r.i(mi).n(l.anchor.x).n(l.anchor.y).n(l.anchor.z).n(l.end.x).n(l.end.y).n(l.end.z).n(l.length).n(l.radius).i(l.material).b(l.live).st(l.state).f(l.prop>=0?std::to_string(l.prop):"").n(l.offset.x).n(l.offset.y).n(l.offset.z).b(l.loose);
    const bool panel=l.panel>=0&&l.socketPort>=0&&l.socketPort<int(ports.size());const bool terminal=l.terminalPort>=0&&l.terminalPort<int(ports.size())&&l.prop>=0;
    r.f(panel?std::to_string(l.panel):"").f(panel?verbatim(ports[size_t(l.socketPort)].name,notes,who+" lead"):"").f(terminal?verbatim(ports[size_t(l.terminalPort)].name,notes,who+" lead"):"");
    if(!panel&&(l.panel>=0||l.socketed()))note("lead "+std::to_string(li)+" has a socket the format can only name through a panel port; it is written as a plain lead");
    if(!terminal&&l.plugged()&&!(panel&&l.loose))note("lead "+std::to_string(li)+" has a terminal plug without a named post; it is written as a plain clip");
    line(r,18);}}
  for(const auto& s:m.hungSuits)line(Rec("HUNG_SUIT").i(mi).n(s.hook.x).n(s.hook.y).n(s.hook.z).n(s.facing));
  for(const auto& c:m.pushCrates)line(Rec("CRATE").i(mi).n(c.position.x).n(c.position.y).n(c.z).n(c.size));
  for(const auto& p:m.cratePlates)line(Rec("CRATE_PLATE").i(mi).n(p.position.x).n(p.position.y).st(p.state));
  if(m.crateReset)line(Rec("CRATE_RESET").i(mi).st(m.crateReset));
  for(const auto& w:m.waterVolumes)line(Rec("WATER").i(mi).n(w.x1).n(w.y1).n(w.x2).n(w.y2).n(w.bed).n(w.surface));
  for(const auto& c:m.compactors)line(Rec("COMPACTOR").i(mi).n(c.x1).n(c.y1).n(c.x2).n(c.y2).n(c.bed).n(c.raised).n(c.period).st(c.stopState),9);
  for(const auto& h:m.navHints)line(Rec("NAV_HINT").i(mi).n(h.position.x).n(h.position.y).n(h.z).i(h.hint));
  for(const auto& s:m.shelters)line(Rec("SHELTER").i(mi).n(s.x1).n(s.y1).n(s.x2).n(s.y2).n(s.bottom).n(s.top));
  for(const auto& o:m.objectiveTexts){
   if(!objectives.insert(o.id).second){note("objective "+stateReference(o.id)+" is named again by an earlier map; the first text is kept");continue;}
   const bool printable=!o.text.empty()&&o.text.size()<=80&&std::all_of(o.text.begin(),o.text.end(),[](unsigned char c){return c>=32&&c<=126;});
   if(!printable)note("objective "+stateReference(o.id)+": its text is not 1-80 printable ASCII characters and will not load");
   line(Rec("OBJECTIVE_TEXT").i(mi).sth(o.id).t(o.text));}
  for(const auto& e:m.scriptEvents){
   if(!events.insert(e.id).second){note("script event "+stateReference(e.id)+" is declared again by an earlier map; this copy is left out");continue;}
   // e.level is not written: the game gives every event the chunk (or map) whose records hold it (Script.cpp seedScripts).
   if(e.x1<0||e.y1<0||e.x2>24||e.y2>24||e.x1>=e.x2||e.y1>=e.y2||e.bottom>=e.top)note("script event "+stateReference(e.id)+": its box is outside the map or empty and will not load");
   if(e.actions.empty())note("script event "+stateReference(e.id)+" has no actions and will not load");
   line(Rec("SCRIPT_EVENT").i(mi).sth(e.id).n(e.x1).n(e.y1).n(e.x2).n(e.y2).n(e.bottom).n(e.top).st(e.requireState).i(e.requireValue).b(e.requireEnemiesClear));
   using S=ScriptCondition::Source;using O=ScriptCondition::Op;
   for(const auto& c:e.conditions){const char* source=c.source==S::Kills?"@kills":c.source==S::Remaining?"@remaining":c.source==S::Health?"@health":c.source==S::Map?"@map":nullptr;
    const char* op=c.op==O::Equal?"=":c.op==O::NotEqual?"!=":c.op==O::Less?"<":c.op==O::LessEqual?"<=":c.op==O::Greater?">":">=";
    line(Rec("SCRIPT_REQUIRE").i(mi).sth(e.id).f(source?source:stateReference(c.id)).f(op).i(c.value));}
   if(e.scope!=ScriptEvent::Scope::Box||e.matchAny||e.rearm||e.once!=!e.rearm){Rec r("SCRIPT_TRIGGER");
    r.i(mi).sth(e.id).f(e.scope==ScriptEvent::Scope::Map?"map":e.scope==ScriptEvent::Scope::Campaign?"campaign":"box").f(e.matchAny?"any":"all").b(e.rearm);if(e.once!=!e.rearm)r.b(e.once);line(r);}
   using T=ScriptAction::Type;
   for(const auto& a:e.actions){const ScriptAction plain{};
    // The named kinds carry only some fields; anything else rides a RAW record.
    auto only=[&](bool id,bool value,bool text,bool amount){return (id||a.id==plain.id)&&(value||a.value==plain.value)&&a.index==plain.index&&a.enemyKind==plain.enemyKind&&a.position.x==plain.position.x&&a.position.y==plain.position.y&&a.z==plain.z&&(amount||a.amount==plain.amount)&&a.sound==plain.sound&&(text||a.text.empty());};
    const bool range=a.value>=-100000&&a.value<=100000;
    Rec r("SCRIPT_ACTION");r.i(mi).sth(e.id);
    if(a.type==T::SetState&&a.id&&range&&only(true,true,false,false))r.f("STATE").sth(a.id).i(a.value);
    else if(a.type==T::AddState&&a.id&&a.value&&range&&only(true,true,false,false))r.f("ADD").sth(a.id).i(a.value);
    else if(a.type==T::SetObjective&&a.id&&a.value>=0&&a.value<=3&&only(true,true,false,false))r.f("OBJECTIVE").sth(a.id).i(a.value);
    else if((a.type==T::GiveItem||a.type==T::TakeItem)&&a.id&&a.value>=1&&a.value<=99&&only(true,true,false,false))r.f(a.type==T::GiveItem?"ITEM":"TAKE").sth(a.id).i(a.value);
    else if(a.type==T::ShowMessage&&!a.text.empty()&&a.text.size()<=96&&a.amount>=.5f&&a.amount<=15&&only(false,false,true,true))r.t("MESSAGE").t(a.text).n(a.amount);
    else if((a.type==T::Checkpoint||a.type==T::CompleteCampaign||a.type==T::EnterMainCampaign)&&only(false,false,false,false))r.f(a.type==T::Checkpoint?"CHECKPOINT":a.type==T::CompleteCampaign?"COMPLETE":"ENTER_CAMPAIGN");
    else if(a.type==T::Shake&&a.amount>0&&a.amount<=3&&only(false,false,false,true))r.f("SHAKE").n(a.amount);
    else r.f("RAW").i(int(a.type)).st(a.id).i(a.value).i(a.index).i(int(a.enemyKind)).n(a.position.x).n(a.position.y).n(a.z).n(a.amount).i(int(a.sound)).t(a.text);
    line(r);}
  }
 }
 // The header: what the file is, and what it cannot carry.
 std::ostringstream text;
 text<<"META_LEVEL_ID: 0\nMETA_LEVEL_NAME: "<<campaignName<<"\nMETA_DEFAULT_TARGET: CUSTOM\nMETA_CAMPAIGN_NAME: "<<campaignName<<"\n\n";
 text<<"# Exported from the DepthWorks campaign by RawMetal.exe --export-rmc. Every record below\n"
       "# is the map's own data. States the map sources spell keep their names; \"#1234567\" is a\n"
       "# state known only by its id. Edit it in the Campaign Builder like any custom map.\n";
 if(notes.empty())text<<"#\n# Nothing was left out: this file carries every record of its maps.\n";
 else{text<<"#\n# What this file cannot carry (custom C++ behaviour or data the format has no field for):\n";for(const auto& n:notes)text<<"#  - "<<n<<'\n';}
 text<<"\n--- CUSTOM_CAMPAIGN_DATA_START ---\n"<<data.str()<<"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
 result.text=text.str();
 return result;
}

std::string rmcDataHash(const std::string& text){
 const auto start=text.find("--- CUSTOM_CAMPAIGN_DATA_START ---"),end=text.find("--- CUSTOM_CAMPAIGN_DATA_END ---");
 if(start==std::string::npos||end==std::string::npos||end<start)return {};
 std::string canonical;std::istringstream lines(text.substr(start,end-start));std::string line;std::getline(lines,line); // the marker's own line
 while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;canonical+=line;canonical+='\n';}
 const auto d=assetHash(std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(canonical.data()),canonical.size()));
 static const char* hex="0123456789abcdef";std::string out;for(auto b:d){out+=hex[b>>4];out+=hex[b&15];}return out;
}
std::string gameVersion(){
#ifdef _WIN32
 static const std::string version=[]{
  HRSRC resource=FindResourceW(nullptr,MAKEINTRESOURCEW(1),RT_VERSION);if(!resource)return std::string("unknown");
  const auto* bytes=static_cast<const BYTE*>(LockResource(LoadResource(nullptr,resource)));const DWORD size=SizeofResource(nullptr,resource);if(!bytes||size<64)return std::string("unknown");
  // VS_FIXEDFILEINFO follows its signature, 0xFEEF04BD.
  for(DWORD i=0;i+52<=size;i+=4){DWORD signature;std::memcpy(&signature,bytes+i,4);if(signature!=0xFEEF04BD)continue;
   DWORD ms,ls;std::memcpy(&ms,bytes+i+8,4);std::memcpy(&ls,bytes+i+12,4);
   return std::to_string(HIWORD(ms))+"."+std::to_string(LOWORD(ms))+"."+std::to_string(HIWORD(ls));}
  return std::string("unknown");}();
 return version;
#else
 return "unknown";
#endif
}

RmcExport exportCampaignChunk(int chunk,bool asPlayed){
 if(chunk<0||chunk>=CampaignChunkCount)throw std::out_of_range("no campaign chunk "+std::to_string(chunk));
 auto map=asPlayed?campaignMapResolved(chunk):campaignMap(chunk);if(!map)throw std::runtime_error("campaign chunk "+std::to_string(chunk)+" has no authored map");
 auto exported=exportRmc(CampaignMapNames[size_t(chunk)],{{map,chunk}});
 // What this level was when exported: an edit sent back is merged against it (tools/campaign/merge_edits.py).
 const auto marker=exported.text.find("\n--- CUSTOM_CAMPAIGN_DATA_START ---");
 if(marker!=std::string::npos)exported.text.insert(marker,"\n# CAMPAIGN_BASE|chunk="+std::to_string(chunk)+"|version="+gameVersion()+"|hash="+rmcDataHash(exported.text)+"\n");
 return exported;
}

// ---- Comparison ----
namespace {
struct Diff {
 std::vector<std::string>& out;size_t limit;size_t total=0;
 static std::string show(float v){return std::isnan(v)?"nan":num(v);}
 static std::string show(double v){return show(float(v));}
 static std::string show(bool v){return v?"1":"0";}
 static std::string show(const std::string& v){return "\""+v+"\"";}
 static std::string show(Vec2 v){return show(v.x)+","+show(v.y);}
 static std::string show(Point3 v){return show(v.x)+","+show(v.y)+","+show(v.z);}
 template<class T> static std::string show(const T& v){
  if constexpr(std::is_enum_v<T>)return std::to_string(static_cast<long long>(v));
  else return std::to_string(v);}
 static bool same(float a,float b){return sameFloat(a,b);}
 static bool same(Vec2 a,Vec2 b){return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y);}
 static bool same(Point3 a,Point3 b){return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y)&&sameFloat(a.z,b.z);}
 template<class T> static bool same(const T& a,const T& b){return a==b;}
 template<class T> void eq(const std::string& what,const T& a,const T& b){if(same(a,b))return;++total;if(out.size()<limit)out.push_back(what+": "+show(a)+" vs "+show(b));}
 void count(const std::string& what,size_t a,size_t b){if(a!=b){++total;if(out.size()<limit)out.push_back(what+" count: "+std::to_string(a)+" vs "+std::to_string(b));}}
};
std::string at(const char* kind,size_t i){return std::string(kind)+"["+std::to_string(i)+"]";}
}

std::vector<std::string> compareAuthoredMaps(const AuthoredMapData& a,const AuthoredMapData& b,bool compareEventLevels,size_t limit){
 std::vector<std::string> out;Diff d{out,limit};
 d.eq("name",a.name,b.name);d.eq("skybox",a.skybox,b.skybox);
 d.eq("terrain present",bool(a.terrain),bool(b.terrain));
 if(a.terrain&&b.terrain){const auto& x=*a.terrain;const auto& y=*b.terrain;d.eq("terrain.seed",x.seed,y.seed);d.eq("terrain.base",x.base,y.base);d.eq("terrain.amplitude",x.amplitude,y.amplitude);d.eq("terrain.scale",x.scale,y.scale);d.eq("terrain.roughness",x.roughness,y.roughness);
  d.count("terrain.flats",x.flats.size(),y.flats.size());for(size_t i=0;i<std::min(x.flats.size(),y.flats.size());++i){const auto&p=x.flats[i];const auto&q=y.flats[i];auto w=at("flat",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".height",p.height,q.height);d.eq(w+".blend",p.blend,q.blend);}
  d.count("terrain.ground",x.ground.size(),y.ground.size());for(size_t i=0;i<std::min(x.ground.size(),y.ground.size());++i){const auto&p=x.ground[i];const auto&q=y.ground[i];auto w=at("ground",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".kind",p.kind,q.kind);d.eq(w+".edge",p.edge,q.edge);}
  d.eq("horizon.depth",x.horizonDepth,y.horizonDepth);d.eq("horizon.spacing",x.horizonSpacing,y.horizonSpacing);d.eq("horizon.rise",x.horizonRise,y.horizonRise);d.eq("horizon.models",x.horizonModels==y.horizonModels,true);}
 {const auto& p=a.definition;const auto& q=b.definition;d.eq("origin",p.origin,q.origin);d.eq("playerStart",p.playerStart,q.playerStart);d.eq("spawnHeight",p.spawnHeight,q.spawnHeight);d.eq("environment",p.environment,q.environment);
  d.eq("lift",p.lift,q.lift);d.eq("ceiling",p.ceiling,q.ceiling);d.eq("ambient",p.ambient,q.ambient);d.eq("residencyGroup",p.residencyGroup,q.residencyGroup);d.eq("floorMaterial",p.floorMaterial,q.floorMaterial);
  d.eq("musicCue",p.musicCue,q.musicCue);d.eq("spawnYaw",p.spawnYaw,q.spawnYaw);d.eq("spawnPitch",p.spawnPitch,q.spawnPitch);d.eq("startUnarmed",p.startUnarmed,q.startUnarmed);
  for(int k=0;k<4;++k)d.eq("haze"+std::to_string(k),p.haze[size_t(k)],q.haze[size_t(k)]);d.eq("wetness",p.wetness,q.wetness);}
 d.eq("visibleResidencyGroups",a.visibleResidencyGroups==b.visibleResidencyGroups,true);
 d.eq("openNorth",a.openNorth,b.openNorth);d.eq("openSouth",a.openSouth,b.openSouth);d.eq("openWest",a.openWest,b.openWest);d.eq("openEast",a.openEast,b.openEast);
 d.eq("wallSurface",a.wallSurface,b.wallSurface);d.eq("floorSurface",a.floorSurface,b.floorSurface);d.eq("ceilingSurface",a.ceilingSurface,b.ceilingSurface);
 d.count("layers",a.layers.size(),b.layers.size());
 for(size_t i=0;i<std::min(a.layers.size(),b.layers.size());++i){const auto&p=a.layers[i];const auto&q=b.layers[i];auto w=at("layer",i);d.eq(w+".name",p.name,q.name);d.eq(w+".elevation",p.elevation,q.elevation);d.eq(w+".thickness",p.thickness,q.thickness);for(size_t r=0;r<24;++r)d.eq(w+".row"+std::to_string(r),p.rows[r],q.rows[r]);}
 d.count("doors",a.doors.size(),b.doors.size());
 for(size_t i=0;i<std::min(a.doors.size(),b.doors.size());++i){const auto&p=a.doors[i];const auto&q=b.doors[i];auto w=at("door",i);
  d.eq(w+".left",p.left,q.left);d.eq(w+".right",p.right,q.right);d.eq(w+".y",p.y,q.y);d.eq(w+".z",p.z,q.z);d.eq(w+".open",p.open,q.open);d.eq(w+".opening",p.opening,q.opening);d.eq(w+".transfer",p.transfer,q.transfer);d.eq(w+".entry",p.entry,q.entry);
  d.eq(w+".requireEnemiesClear",p.requireEnemiesClear,q.requireEnemiesClear);d.eq(w+".requireControl",p.requireControl,q.requireControl);d.eq(w+".requireState",p.requireState,q.requireState);d.eq(w+".requireValue",p.requireValue,q.requireValue);
  d.eq(w+".swinging",p.swinging,q.swinging);d.eq(w+".swing",p.swing,q.swing);d.eq(w+".sign",p.sign,q.sign);d.eq(w+".maxOpen",p.maxOpen,q.maxOpen);}
 d.count("props",a.props.size(),b.props.size());
 for(size_t i=0;i<std::min(a.props.size(),b.props.size());++i){const auto&p=a.props[i];const auto&q=b.props[i];auto w=at("prop",i);d.eq(w+".kind",p.kind,q.kind);d.eq(w+".position",p.position,q.position);d.eq(w+".height",p.height,q.height);d.eq(w+".footprint",p.footprint,q.footprint);d.eq(w+".yaw",p.yaw,q.yaw);d.eq(w+".halfSize",p.halfSize,q.halfSize);d.eq(w+".base",p.base,q.base);}
 d.count("fixtures",a.fixtures.size(),b.fixtures.size());
 for(size_t i=0;i<std::min(a.fixtures.size(),b.fixtures.size());++i){const auto&p=a.fixtures[i];const auto&q=b.fixtures[i];auto w=at("fixture",i);d.eq(w+".model",p.model,q.model);d.eq(w+".position",p.position,q.position);d.eq(w+".base",p.base,q.base);
  d.eq(w+".size",Point3{p.width,p.depth,p.height},Point3{q.width,q.depth,q.height});d.eq(w+".yaw",p.yaw,q.yaw);d.eq(w+".solid",p.solid,q.solid);d.eq(w+".core",p.core,q.core);d.eq(w+".visible",p.visible,q.visible);}
 d.count("pipes",a.pipes.size(),b.pipes.size());
 for(size_t i=0;i<std::min(a.pipes.size(),b.pipes.size());++i){const auto&p=a.pipes[i];const auto&q=b.pipes[i];auto w=at("pipe",i);d.eq(w+".start",p.start,q.start);d.eq(w+".end",p.end,q.end);d.eq(w+".z",p.z,q.z);d.eq(w+".radius",p.radius,q.radius);d.eq(w+".endZ",p.endZ,q.endZ);d.eq(w+".material",p.material,q.material);}
 // Lights: pilot lamps of test leads are matched by what they are; the rest in order.
 auto pilotsOf=[](const AuthoredMapData& m){std::set<int> s;for(const auto& l:m.leads)if(l.panel>=0&&l.lamp>=0)s.insert(l.lamp);return s;};
 const auto pa=pilotsOf(a),pb=pilotsOf(b);
 auto lightKey=[](const WorldLight& l){std::ostringstream s;s<<num(l.position.x)<<' '<<num(l.position.y)<<' '<<num(l.z)<<' '<<int(l.mount)<<' '<<num(l.yaw)<<' '<<num(l.intensity)<<' '<<num(l.range)<<' '<<num(l.kelvin)<<' '<<num(l.tint)<<' '<<num(l.gel.x)<<' '<<num(l.gel.y)<<' '<<num(l.gel.z)
  <<' '<<l.enabledState<<' '<<l.enabledValue<<' '<<l.invert<<' '<<num(l.period)<<' '<<num(l.onTime)<<' '<<num(l.phase)<<' '<<num(l.flicker)<<' '<<l.fitting<<' '<<num(l.fittingWidth)<<' '<<num(l.fittingDepth)<<' '<<num(l.fittingHeight)<<' '<<l.shadows<<' '<<l.prop<<' '<<l.propLevel<<' '<<l.cargoLift;return s.str();};
 {std::vector<const WorldLight*> la,lb;std::vector<std::string> ka,kb;
  for(int i=0;i<int(a.lights.size());++i)if(pa.count(i))ka.push_back(lightKey(a.lights[size_t(i)]));else la.push_back(&a.lights[size_t(i)]);
  for(int i=0;i<int(b.lights.size());++i)if(pb.count(i))kb.push_back(lightKey(b.lights[size_t(i)]));else lb.push_back(&b.lights[size_t(i)]);
  d.count("lights (without test-lead pilots)",la.size(),lb.size());
  for(size_t i=0;i<std::min(la.size(),lb.size());++i){auto x=lightKey(*la[i]),y=lightKey(*lb[i]);if(x!=y)d.eq(at("light",i),x,y);}
  std::sort(ka.begin(),ka.end());std::sort(kb.begin(),kb.end());d.eq("test-lead pilot lamps",ka==kb,true);}
 d.count("leads",a.leads.size(),b.leads.size());
 for(size_t i=0;i<std::min(a.leads.size(),b.leads.size());++i){const auto&p=a.leads[i];const auto&q=b.leads[i];auto w=at("lead",i);d.eq(w+".anchor",p.anchor,q.anchor);d.eq(w+".end",p.end,q.end);d.eq(w+".offset",p.offset,q.offset);d.eq(w+".prop",p.prop,q.prop);d.eq(w+".loose",p.loose,q.loose);
  d.eq(w+".length",p.length,q.length);d.eq(w+".radius",p.radius,q.radius);d.eq(w+".material",p.material,q.material);d.eq(w+".live",p.live,q.live);d.eq(w+".state",p.state,q.state);d.eq(w+".socket",p.socket,q.socket);d.eq(w+".terminal",p.terminal,q.terminal);
  d.eq(w+".panel",p.panel,q.panel);d.eq(w+".socketPort",p.socketPort,q.socketPort);d.eq(w+".terminalPort",p.terminalPort,q.terminalPort);
  d.eq(w+".lamp",p.lamp>=0?lightKey(a.lights[size_t(p.lamp)]):std::string("none"),q.lamp>=0?lightKey(b.lights[size_t(q.lamp)]):std::string("none"));}
 d.count("hungSuits",a.hungSuits.size(),b.hungSuits.size());
 for(size_t i=0;i<std::min(a.hungSuits.size(),b.hungSuits.size());++i){d.eq(at("hungSuit",i)+".hook",a.hungSuits[i].hook,b.hungSuits[i].hook);d.eq(at("hungSuit",i)+".facing",a.hungSuits[i].facing,b.hungSuits[i].facing);}
 d.count("creatures",a.creatureSpawns.size(),b.creatureSpawns.size());
 for(size_t i=0;i<std::min(a.creatureSpawns.size(),b.creatureSpawns.size());++i){const auto&p=a.creatureSpawns[i];const auto&q=b.creatureSpawns[i];auto w=at("creature",i);d.eq(w+".kind",p.kind,q.kind);d.eq(w+".position",p.position,q.position);d.eq(w+".z",p.z,q.z);d.eq(w+".scale",p.scale,q.scale);d.eq(w+".health",p.health,q.health);d.eq(w+".wakeState",p.wakeState,q.wakeState);}
 d.count("pickups",a.pickupSpawns.size(),b.pickupSpawns.size());
 for(size_t i=0;i<std::min(a.pickupSpawns.size(),b.pickupSpawns.size());++i){const auto&p=a.pickupSpawns[i];const auto&q=b.pickupSpawns[i];auto w=at("pickup",i);d.eq(w+".kind",p.kind,q.kind);d.eq(w+".position",p.position,q.position);d.eq(w+".z",p.z,q.z);}
 d.count("clutter",a.clutterSpawns.size(),b.clutterSpawns.size());
 for(size_t i=0;i<std::min(a.clutterSpawns.size(),b.clutterSpawns.size());++i){const auto&p=a.clutterSpawns[i];const auto&q=b.clutterSpawns[i];auto w=at("clutter",i);d.eq(w+".kind",p.kind,q.kind);d.eq(w+".position",p.position,q.position);d.eq(w+".z",p.z,q.z);d.eq(w+".yaw",p.yaw,q.yaw);
  d.eq(w+".model",p.model,q.model);d.eq(w+".size",Point3{p.width,p.depth,p.height},Point3{q.width,q.depth,q.height});d.eq(w+".pitch",p.pitch,q.pitch);d.eq(w+".roll",p.roll,q.roll);d.eq(w+".breakState",p.breakState,q.breakState);}
 d.count("breakables",a.breakables.size(),b.breakables.size());
 for(size_t i=0;i<std::min(a.breakables.size(),b.breakables.size());++i){const auto&p=a.breakables[i];const auto&q=b.breakables[i];auto w=at("breakable",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);
  d.eq(w+".material",p.material,q.material);d.eq(w+".health",p.health,q.health);d.eq(w+".holds",p.holds,q.holds);d.eq(w+".state",p.state,q.state);d.eq(w+".trigger",p.trigger,q.trigger);d.eq(w+".fixture",p.fixture,q.fixture);d.eq(w+".part",p.part,q.part);}
 d.count("water",a.waterVolumes.size(),b.waterVolumes.size());
 for(size_t i=0;i<std::min(a.waterVolumes.size(),b.waterVolumes.size());++i){const auto&p=a.waterVolumes[i];const auto&q=b.waterVolumes[i];auto w=at("water",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bed",p.bed,q.bed);d.eq(w+".surface",p.surface,q.surface);}
 d.count("emitters",a.emitters.size(),b.emitters.size());
 for(size_t i=0;i<std::min(a.emitters.size(),b.emitters.size());++i){const auto&p=a.emitters[i];const auto&q=b.emitters[i];auto w=at("emitter",i);d.eq(w+".position",p.position,q.position);d.eq(w+".z",p.z,q.z);d.eq(w+".drift",p.drift,q.drift);d.eq(w+".rise",p.rise,q.rise);d.eq(w+".rate",p.rate,q.rate);d.eq(w+".radius",p.radius,q.radius);d.eq(w+".growth",p.growth,q.growth);d.eq(w+".count",p.count,q.count);}
 d.count("hazards",a.hazards.size(),b.hazards.size());
 for(size_t i=0;i<std::min(a.hazards.size(),b.hazards.size());++i){const auto&p=a.hazards[i];const auto&q=b.hazards[i];auto w=at("hazard",i);d.eq(w+".kind",p.kind,q.kind);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);
  d.eq(w+".damage",p.damagePerSecond,q.damagePerSecond);d.eq(w+".state",p.enabledFlag,q.enabledFlag);d.eq(w+".value",p.enabledValue,q.enabledValue);d.eq(w+".invert",p.invertFlag,q.invertFlag);d.eq(w+".period",p.period,q.period);d.eq(w+".onTime",p.onTime,q.onTime);d.eq(w+".phase",p.phase,q.phase);}
 d.count("compactors",a.compactors.size(),b.compactors.size());
 for(size_t i=0;i<std::min(a.compactors.size(),b.compactors.size());++i){const auto&p=a.compactors[i];const auto&q=b.compactors[i];auto w=at("compactor",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bed",p.bed,q.bed);d.eq(w+".raised",p.raised,q.raised);d.eq(w+".period",p.period,q.period);d.eq(w+".stopState",p.stopState,q.stopState);}
 d.count("structures",a.structures.size(),b.structures.size());
 for(size_t i=0;i<std::min(a.structures.size(),b.structures.size());++i){const auto&p=a.structures[i];const auto&q=b.structures[i];auto w=at("structure",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);
  d.eq(w+".rail",p.rail,q.rail);d.eq(w+".material",p.material,q.material);d.eq(w+".solid",p.solid,q.solid);d.eq(w+".shape",int(p.shape),int(q.shape));d.eq(w+".facing",int(p.facing),int(q.facing));}
 d.count("stairs",a.stairs.size(),b.stairs.size());
 for(size_t i=0;i<std::min(a.stairs.size(),b.stairs.size());++i){const auto&p=a.stairs[i];const auto&q=b.stairs[i];auto w=at("stair",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);d.eq(w+".steps",p.steps,q.steps);
  d.eq(w+".alongY",p.alongY,q.alongY);d.eq(w+".ascending",p.ascending,q.ascending);d.eq(w+".openUnderside",p.openUnderside,q.openUnderside);d.eq(w+".tread",p.treadThickness,q.treadThickness);d.eq(w+".sideRails",p.sideRails,q.sideRails);}
 d.count("terminals",a.terminals.size(),b.terminals.size());
 for(size_t i=0;i<std::min(a.terminals.size(),b.terminals.size());++i){const auto&p=a.terminals[i];const auto&q=b.terminals[i];auto w=at("terminal",i);d.eq(w+".position",p.position,q.position);d.eq(w+".z",p.z,q.z);d.eq(w+".title",p.title,q.title);d.eq(w+".line1",p.line1,q.line1);d.eq(w+".line2",p.line2,q.line2);
  d.eq(w+".control",p.control,q.control);d.eq(w+".reactorAction",p.reactorAction,q.reactorAction);d.eq(w+".activateState",p.activateState,q.activateState);d.eq(w+".toggleState",p.toggleState,q.toggleState);d.eq(w+".requireState",p.requireState,q.requireState);
  d.eq(w+".yaw",p.yaw,q.yaw);d.eq(w+".readable",p.readable,q.readable);d.eq(w+".taped",p.taped,q.taped);d.eq(w+".surface",p.surface,q.surface);}
 d.count("signs",a.signs.size(),b.signs.size());
 for(size_t i=0;i<std::min(a.signs.size(),b.signs.size());++i){const auto&p=a.signs[i];const auto&q=b.signs[i];auto w=at("sign",i);d.eq(w+".position",p.position,q.position);d.eq(w+".z",p.z,q.z);d.eq(w+".size",Vec2{p.width,p.height},Vec2{q.width,q.height});d.eq(w+".yaw",p.yaw,q.yaw);d.eq(w+".title",p.title,q.title);d.eq(w+".subtitle",p.subtitle,q.subtitle);d.eq(w+".accent",p.accent,q.accent);}
 {const auto&p=a.cargoLift;const auto&q=b.cargoLift;d.eq("cargoLift.box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq("cargoLift.y2",p.y2,q.y2);d.eq("cargoLift.lower",p.lower,q.lower);d.eq("cargoLift.upper",p.upper,q.upper);d.eq("cargoLift.speed",p.speed,q.speed);
  d.eq("cargoLift.states",std::array<StateId,6>{p.callState,p.releaseState,p.positionState,p.downState,p.arrivedState,p.descendedState}==std::array<StateId,6>{q.callState,q.releaseState,q.positionState,q.downState,q.arrivedState,q.descendedState},true);}
 d.count("timedSequences",a.timedSequences.size(),b.timedSequences.size());
 for(size_t i=0;i<std::min(a.timedSequences.size(),b.timedSequences.size());++i){const auto&p=a.timedSequences[i];const auto&q=b.timedSequences[i];auto w=at("sequence",i);d.eq(w+".timer",p.timerState,q.timerState);d.eq(w+".finish",p.finishState,q.finishState);
  d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);d.eq(w+".duration",p.durationMs,q.durationMs);d.eq(w+".finishAt",p.finishAtMs,q.finishAtMs);
  d.eq(w+".interval",p.soundIntervalMs,q.soundIntervalMs);d.eq(w+".soundUntil",p.soundUntilMs,q.soundUntilMs);d.eq(w+".tick",p.tickSound,q.tickSound);d.eq(w+".gain",p.gain,q.gain);d.eq(w+".pitch",p.pitch,q.pitch);d.eq(w+".loop",p.loop,q.loop);
  d.eq(w+".sightActor",p.sightActor,q.sightActor);d.eq(w+".sightDistance",p.sightDistance,q.sightDistance);d.eq(w+".soundAt",p.soundAt,q.soundAt);d.eq(w+".soundZ",p.soundZ,q.soundZ);}
 d.count("actors",a.actorTracks.size(),b.actorTracks.size());
 auto keys=[&](const std::string& w,const std::vector<ActorKey>& x,const std::vector<ActorKey>& y){d.count(w,x.size(),y.size());for(size_t k=0;k<std::min(x.size(),y.size());++k){auto v=w+"["+std::to_string(k)+"]";d.eq(v+".time",x[k].timeMs,y[k].timeMs);d.eq(v+".position",x[k].position,y[k].position);d.eq(v+".z",x[k].z,y[k].z);d.eq(v+".yaw",x[k].yaw,y[k].yaw);d.eq(v+".clip",x[k].clip,y[k].clip);d.eq(v+".phase",x[k].phase,y[k].phase);d.eq(v+".look",x[k].lookAtActor,y[k].lookAtActor);}};
 for(size_t i=0;i<std::min(a.actorTracks.size(),b.actorTracks.size());++i){const auto&p=a.actorTracks[i];const auto&q=b.actorTracks[i];auto w=at("actor",i);const auto&x=p.ai;const auto&y=q.ai;
  d.eq(w+".timer",p.timerState,q.timerState);d.eq(w+".visual",p.visual,q.visual);d.eq(w+".scale",p.scale,q.scale);d.eq(w+".loop",p.loop,q.loop);d.eq(w+".tool",p.tool,q.tool);keys(w+".keys",p.keys,q.keys);keys(w+".idleKeys",p.idleKeys,q.idleKeys);
  d.eq(w+".platform",p.platform,q.platform);d.eq(w+".footprint",p.footprint,q.footprint);d.eq(w+".thickness",p.thickness,q.thickness);d.eq(w+".suspensionTop",p.suspensionTop,q.suspensionTop);
  d.eq(w+".damageState",p.damageState,q.damageState);d.eq(w+".deadState",p.deadState,q.deadState);d.eq(w+".health",p.health,q.health);d.eq(w+".idleUntil",p.idleUntilMs,q.idleUntilMs);d.eq(w+".approachUntil",p.approachUntilMs,q.approachUntilMs);d.eq(w+".voice",p.voice,q.voice);d.eq(w+".talk",p.talk,q.talk);
  d.eq(w+".ai.mode",x.mode,y.mode);d.eq(w+".ai.enable",x.enableState,y.enableState);d.eq(w+".ai.speed",x.speed,y.speed);d.eq(w+".ai.follow",x.followDistance,y.followDistance);d.eq(w+".ai.danger",x.dangerRange,y.dangerRange);
  d.eq(w+".ai.sightRange",x.sightRange,y.sightRange);d.eq(w+".ai.sightDot",x.sightDot,y.sightDot);d.eq(w+".ai.recognition",x.recognitionMs,y.recognitionMs);d.eq(w+".ai.search",x.searchMs,y.searchMs);d.eq(w+".ai.alert",x.alertState,y.alertState);
  d.count(w+".ai.patrol",x.patrol.size(),y.patrol.size());for(size_t k=0;k<std::min(x.patrol.size(),y.patrol.size());++k){auto v=w+".patrol["+std::to_string(k)+"]";d.eq(v+".position",x.patrol[k].position,y.patrol[k].position);d.eq(v+".z",x.patrol[k].z,y.patrol[k].z);d.eq(v+".pause",x.patrol[k].pauseMs,y.patrol[k].pauseMs);d.eq(v+".look",x.patrol[k].look,y.patrol[k].look);}
  d.eq(w+".ai.armed",x.armed,y.armed);d.eq(w+".ai.aim",x.aimMs,y.aimMs);d.eq(w+".ai.recovery",x.recoveryMs,y.recoveryMs);d.eq(w+".ai.range",x.attackRange,y.attackRange);d.eq(w+".ai.damage",x.attackDamage,y.attackDamage);
  d.eq(w+".ai.faction",x.faction,y.faction);d.eq(w+".ai.squad",x.squad,y.squad);d.eq(w+".ai.stance",x.stance,y.stance);d.eq(w+".ai.suppress",x.suppress,y.suppress);d.eq(w+".ai.grenades",x.grenades,y.grenades);d.eq(w+".ai.role",x.role,y.role);d.eq(w+".ai.weapon",x.weapon,y.weapon);d.eq(w+".ai.dropsSidearm",x.dropsSidearm,y.dropsSidearm);}
 d.count("sequenceCues",a.sequenceCues.size(),b.sequenceCues.size());
 for(size_t i=0;i<std::min(a.sequenceCues.size(),b.sequenceCues.size());++i){const auto&p=a.sequenceCues[i];const auto&q=b.sequenceCues[i];auto w=at("cue",i);d.eq(w+".timer",p.timerState,q.timerState);d.eq(w+".time",p.timeMs,q.timeMs);d.eq(w+".sound",p.sound,q.sound);d.eq(w+".position",p.position,q.position);
  d.eq(w+".gain",p.gain,q.gain);d.eq(w+".pitch",p.pitch,q.pitch);d.eq(w+".caption",p.caption,q.caption);d.eq(w+".z",p.z,q.z);d.eq(w+".aiRadius",p.aiRadius,q.aiRadius);d.eq(w+".voice",p.voice,q.voice);}
 d.count("dialogue",a.dialogue.size(),b.dialogue.size());
 for(size_t i=0;i<std::min(a.dialogue.size(),b.dialogue.size());++i){const auto&p=a.dialogue[i];const auto&q=b.dialogue[i];auto w=at("dialogue",i);d.eq(w+".id",p.id,q.id);d.eq(w+".speaker",p.speaker,q.speaker);d.eq(w+".line",p.line,q.line);d.eq(w+".voice",p.voice,q.voice);
  d.count(w+".replies",p.replies.size(),q.replies.size());for(size_t k=0;k<std::min(p.replies.size(),q.replies.size());++k){const auto&r=p.replies[k];const auto&s=q.replies[k];auto v=w+".reply["+std::to_string(k)+"]";
   d.eq(v+".text",r.text,s.text);d.eq(v+".next",r.next,s.next);d.eq(v+".require",r.require,s.require);d.eq(v+".requireValue",r.requireValue,s.requireValue);d.eq(v+".hide",r.hide,s.hide);d.eq(v+".sets",r.sets==s.sets,true);d.eq(v+".stance",r.stance,s.stance);}}
 d.count("scriptEvents",a.scriptEvents.size(),b.scriptEvents.size());
 for(size_t i=0;i<std::min(a.scriptEvents.size(),b.scriptEvents.size());++i){const auto&p=a.scriptEvents[i];const auto&q=b.scriptEvents[i];auto w=at("event",i);d.eq(w+".id",p.id,q.id);if(compareEventLevels)d.eq(w+".level",p.level,q.level);
  d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);d.eq(w+".requireState",p.requireState,q.requireState);d.eq(w+".requireValue",p.requireValue,q.requireValue);
  d.eq(w+".requireEnemiesClear",p.requireEnemiesClear,q.requireEnemiesClear);d.eq(w+".once",p.once,q.once);d.eq(w+".scope",p.scope,q.scope);d.eq(w+".matchAny",p.matchAny,q.matchAny);d.eq(w+".rearm",p.rearm,q.rearm);
  d.count(w+".conditions",p.conditions.size(),q.conditions.size());for(size_t k=0;k<std::min(p.conditions.size(),q.conditions.size());++k){const auto&r=p.conditions[k];const auto&s=q.conditions[k];auto v=w+".condition["+std::to_string(k)+"]";d.eq(v+".source",r.source,s.source);d.eq(v+".id",r.id,s.id);d.eq(v+".op",r.op,s.op);d.eq(v+".value",r.value,s.value);}
  d.count(w+".actions",p.actions.size(),q.actions.size());for(size_t k=0;k<std::min(p.actions.size(),q.actions.size());++k){const auto&r=p.actions[k];const auto&s=q.actions[k];auto v=w+".action["+std::to_string(k)+"]";
   d.eq(v+".type",r.type,s.type);d.eq(v+".id",r.id,s.id);d.eq(v+".value",r.value,s.value);d.eq(v+".index",r.index,s.index);d.eq(v+".creature",r.enemyKind,s.enemyKind);d.eq(v+".position",r.position,s.position);d.eq(v+".z",r.z,s.z);d.eq(v+".amount",r.amount,s.amount);d.eq(v+".sound",r.sound,s.sound);d.eq(v+".text",r.text,s.text);}}
 d.count("objectiveTexts",a.objectiveTexts.size(),b.objectiveTexts.size());
 for(size_t i=0;i<std::min(a.objectiveTexts.size(),b.objectiveTexts.size());++i){d.eq(at("objective",i)+".id",a.objectiveTexts[i].id,b.objectiveTexts[i].id);d.eq(at("objective",i)+".text",a.objectiveTexts[i].text,b.objectiveTexts[i].text);}
 d.count("navHints",a.navHints.size(),b.navHints.size());
 for(size_t i=0;i<std::min(a.navHints.size(),b.navHints.size());++i){d.eq(at("navHint",i)+".position",a.navHints[i].position,b.navHints[i].position);d.eq(at("navHint",i)+".z",a.navHints[i].z,b.navHints[i].z);d.eq(at("navHint",i)+".hint",a.navHints[i].hint,b.navHints[i].hint);}
 d.count("zones",a.zones.size(),b.zones.size());
 for(size_t i=0;i<std::min(a.zones.size(),b.zones.size());++i){const auto&p=a.zones[i];const auto&q=b.zones[i];auto w=at("zone",i);d.eq(w+".name",p.name,q.name);d.eq(w+".title",p.title,q.title);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);for(int k=0;k<4;++k)d.eq(w+".haze"+std::to_string(k),p.haze[size_t(k)],q.haze[size_t(k)]);}
 d.count("pushCrates",a.pushCrates.size(),b.pushCrates.size());
 for(size_t i=0;i<std::min(a.pushCrates.size(),b.pushCrates.size());++i){d.eq(at("crate",i)+".position",a.pushCrates[i].position,b.pushCrates[i].position);d.eq(at("crate",i)+".z",a.pushCrates[i].z,b.pushCrates[i].z);d.eq(at("crate",i)+".size",a.pushCrates[i].size,b.pushCrates[i].size);}
 d.count("cratePlates",a.cratePlates.size(),b.cratePlates.size());
 for(size_t i=0;i<std::min(a.cratePlates.size(),b.cratePlates.size());++i){d.eq(at("plate",i)+".position",a.cratePlates[i].position,b.cratePlates[i].position);d.eq(at("plate",i)+".state",a.cratePlates[i].state,b.cratePlates[i].state);}
 d.eq("crateReset",a.crateReset,b.crateReset);
 d.count("darkZones",a.darkZones.size(),b.darkZones.size());
 for(size_t i=0;i<std::min(a.darkZones.size(),b.darkZones.size());++i){const auto&p=a.darkZones[i];const auto&q=b.darkZones[i];auto w=at("dark",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);d.eq(w+".state",p.state,q.state);d.eq(w+".value",p.value,q.value);d.eq(w+".invert",p.invert,q.invert);d.eq(w+".jam",p.jamFlashlight,q.jamFlashlight);}
 d.count("shelters",a.shelters.size(),b.shelters.size());
 for(size_t i=0;i<std::min(a.shelters.size(),b.shelters.size());++i){const auto&p=a.shelters[i];const auto&q=b.shelters[i];auto w=at("shelter",i);d.eq(w+".box",Point3{p.x1,p.y1,p.x2},Point3{q.x1,q.y1,q.x2});d.eq(w+".y2",p.y2,q.y2);d.eq(w+".bottom",p.bottom,q.bottom);d.eq(w+".top",p.top,q.top);}
 if(d.total>out.size())out.push_back("... "+std::to_string(d.total-out.size())+" more differences");
 return out;
}

// ---- Overrides ----
namespace {
struct OverrideRegistry {
 std::mutex lock;
 std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount> installed{},embedded{};
 std::vector<std::string> errors;
 std::once_flag scanned;
};
OverrideRegistry& registry(){static OverrideRegistry r;return r;}
void scanEmbedded(){
#ifdef _WIN32
 auto& r=registry();
 const auto workspace=campaignWorkspace();
 // A released copy plays the levels built into it (campaign-overrides at build time)...
 if(workspace.empty())for(int chunk=0;chunk<CampaignChunkCount;++chunk){
  HRSRC resource=FindResourceW(nullptr,MAKEINTRESOURCEW(CampaignOverrideResourceBase+chunk),MAKEINTRESOURCEW(10));if(!resource)continue;
  const auto* bytes=static_cast<const char*>(LockResource(LoadResource(nullptr,resource)));const DWORD size=SizeofResource(nullptr,resource);
  if(!bytes||!size){r.errors.push_back("chunk "+std::to_string(chunk)+": empty embedded override");continue;}
  try{r.embedded[size_t(chunk)]=campaignOverrideFromText(chunk,std::string(bytes,size),"campaign-overrides/"+std::to_string(chunk)+".rmc");}
  catch(const std::exception& e){r.errors.push_back("chunk "+std::to_string(chunk)+": "+e.what()+" (its C++ map is used)");}
 }
 // ...and then a campaign-overrides folder beside it: levels a player saved into their own
 // copy from the Campaign Builder, which win. In the source workspace that folder (in the
 // repository) is the whole truth, so a level saved into the game, or put back to its
 // original, plays that way at once without a rebuild.
 std::filesystem::path folder;
 if(!workspace.empty())folder=workspace/"campaign-overrides";
 else{std::wstring exe(32768,L'\0');exe.resize(GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size())));folder=std::filesystem::path(exe).parent_path()/"campaign-overrides";}
 std::error_code ec;
 if(std::filesystem::is_directory(folder,ec))for(const auto& entry:std::filesystem::directory_iterator(folder,ec)){
  if(!entry.is_regular_file()||entry.path().extension()!=".rmc")continue;const int chunk=campaignOverrideChunk(entry.path());if(chunk<0)continue;
  try{std::ifstream in(entry.path(),std::ios::binary);std::string text((std::istreambuf_iterator<char>(in)),{});r.embedded[size_t(chunk)]=campaignOverrideFromText(chunk,std::move(text),entry.path().filename().string());}
  catch(const std::exception& e){r.errors.push_back(entry.path().filename().string()+": "+e.what()+" (its C++ map is used)");}
 }
 if(!r.errors.empty()){std::ofstream log("RawMetal-campaign-overrides.txt");for(const auto& e:r.errors)log<<e<<'\n';}
#endif
}
}

std::shared_ptr<const AuthoredMapData> campaignOverrideFromText(int chunk,std::string text,const std::string& source){
 if(chunk<0||chunk>=CampaignChunkCount)throw std::out_of_range("no campaign chunk "+std::to_string(chunk));
 auto campaign=loadCustomCampaignText(std::move(text),source);
 if(campaign->maps.empty())throw std::runtime_error("the file has no map");
 auto map=std::make_shared<AuthoredMapData>(*campaign->maps.front());
 // Where it stands is the campaign's: streaming, seams and saves place chunks by the table.
 map->definition.origin=CampaignChunks[size_t(chunk)].origin;
 for(auto& event:map->scriptEvents)event.level=chunk;
 return map;
}
std::shared_ptr<const AuthoredMapData> campaignOverride(int chunk){
 if(chunk<0||chunk>=CampaignChunkCount)return nullptr;
 auto& r=registry();std::call_once(r.scanned,scanEmbedded);
 std::lock_guard guard(r.lock);
 if(r.installed[size_t(chunk)])return r.installed[size_t(chunk)];
 return r.embedded[size_t(chunk)];
}
std::shared_ptr<const AuthoredMapData> campaignMapResolved(int chunk){
 if(auto map=campaignOverride(chunk))return map;
 return campaignMap(chunk);
}
void setCampaignOverride(int chunk,std::shared_ptr<const AuthoredMapData> map){
 if(chunk<0||chunk>=CampaignChunkCount)throw std::out_of_range("no campaign chunk "+std::to_string(chunk));
 auto& r=registry();std::call_once(r.scanned,scanEmbedded);std::lock_guard guard(r.lock);r.installed[size_t(chunk)]=std::move(map);
}
int campaignOverrideChunk(const std::filesystem::path& file){
 const auto name=file.filename().string();size_t digits=0;while(digits<name.size()&&digits<3&&std::isdigit(static_cast<unsigned char>(name[digits])))++digits;
 if(!digits||(digits<name.size()&&std::isdigit(static_cast<unsigned char>(name[digits]))))return -1;
 const int chunk=std::stoi(name.substr(0,digits));return chunk<CampaignChunkCount?chunk:-1;
}
std::vector<std::string> loadCampaignOverrideFolder(const std::filesystem::path& folder){
 std::vector<std::string> report;std::error_code ec;
 for(const auto& entry:std::filesystem::directory_iterator(folder,ec)){
  if(!entry.is_regular_file()||entry.path().extension()!=".rmc")continue;const int chunk=campaignOverrideChunk(entry.path());
  if(chunk<0){report.push_back(entry.path().filename().string()+": skipped (the name does not start with a chunk number)");continue;}
  try{std::ifstream in(entry.path(),std::ios::binary);std::string text((std::istreambuf_iterator<char>(in)),{});
   setCampaignOverride(chunk,campaignOverrideFromText(chunk,std::move(text),entry.path().filename().string()));
   report.push_back(entry.path().filename().string()+": chunk "+std::to_string(chunk)+" overridden");}
  catch(const std::exception& e){report.push_back(entry.path().filename().string()+": "+e.what());}
 }
 if(ec)report.push_back(folder.string()+": "+ec.message());
 return report;
}
std::vector<std::string> campaignOverrideErrors(){auto& r=registry();std::call_once(r.scanned,scanEmbedded);std::lock_guard guard(r.lock);return r.errors;}

// ---- Tests ----
namespace {
// Built-world facts that the game reads: what World makes of the records.
std::vector<std::string> compareWorlds(const World& a,const World& b,size_t limit=20){
 std::vector<std::string> out;Diff d{out,limit};
 d.count("world structures",a.structures().size(),b.structures().size());
 for(size_t i=0;i<std::min(a.structures().size(),b.structures().size());++i){const auto&p=a.structures()[i];const auto&q=b.structures()[i];d.eq(at("world structure",i),Point3{p.x1,p.y1,p.bottom},Point3{q.x1,q.y1,q.bottom});d.eq(at("world structure",i)+" far",Point3{p.x2,p.y2,p.top},Point3{q.x2,q.y2,q.top});d.eq(at("world structure",i)+".material",p.material,q.material);}
 d.count("world fixtures",a.fixtures().size(),b.fixtures().size());
 for(size_t i=0;i<std::min(a.fixtures().size(),b.fixtures().size());++i){d.eq(at("world fixture",i),a.fixtures()[i].position,b.fixtures()[i].position);d.eq(at("world fixture",i)+".model",a.fixtures()[i].model,b.fixtures()[i].model);}
 d.count("world lights",a.lights().size(),b.lights().size());
 {std::vector<std::string> x,y;for(const auto& l:a.lights())x.push_back(num(l.position.x)+","+num(l.position.y)+","+num(l.z)+" "+std::to_string(int(l.mount))+" "+num(l.intensity));
  for(const auto& l:b.lights())y.push_back(num(l.position.x)+","+num(l.position.y)+","+num(l.z)+" "+std::to_string(int(l.mount))+" "+num(l.intensity));
  std::sort(x.begin(),x.end());std::sort(y.begin(),y.end());d.eq("world lights (positions, mounts, intensities)",x==y,true);}
 d.count("world doors",a.doors().size(),b.doors().size());
 for(size_t i=0;i<std::min(a.doors().size(),b.doors().size());++i){d.eq(at("world door",i),Point3{a.doors()[i].left,a.doors()[i].right,a.doors()[i].y},Point3{b.doors()[i].left,b.doors()[i].right,b.doors()[i].y});}
 d.count("world terminals",a.terminals().size(),b.terminals().size());
 for(size_t i=0;i<std::min(a.terminals().size(),b.terminals().size());++i)d.eq(at("world terminal",i),a.terminals()[i].position,b.terminals()[i].position);
 d.count("world layers",a.layers().size(),b.layers().size());
 for(size_t i=0;i<std::min(a.layers().size(),b.layers().size());++i)for(size_t r=0;r<24;++r)d.eq(at("world layer",i)+".row"+std::to_string(r),std::string(a.layers()[i].rows[r]),std::string(b.layers()[i].rows[r]));
 d.count("world props",a.props().size(),b.props().size());d.count("world pipes",a.pipes().size(),b.pipes().size());d.count("world clutter",a.clutterSpawns().size(),b.clutterSpawns().size());
 d.count("world creatures",a.creatureSpawns().size(),b.creatureSpawns().size());d.count("world pickups",a.pickupSpawns().size(),b.pickupSpawns().size());d.count("world hazards",a.hazards().size(),b.hazards().size());
 d.count("world water",a.waterVolumes().size(),b.waterVolumes().size());d.count("world emitters",a.particleEmitters().size(),b.particleEmitters().size());d.count("world script events",a.scriptEvents().size(),b.scriptEvents().size());
 d.count("world leads",a.leads().size(),b.leads().size());d.count("world crates",a.pushCrates().size(),b.pushCrates().size());
 d.eq("world seams",std::array<bool,4>{a.openNorthBoundary(),a.openSouthBoundary(),a.openWestBoundary(),a.openEastBoundary()}==std::array<bool,4>{b.openNorthBoundary(),b.openSouthBoundary(),b.openWestBoundary(),b.openEastBoundary()},true);
 if(d.total>out.size())out.push_back("... "+std::to_string(d.total-out.size())+" more differences");
 return out;
}
std::string countsOf(const AuthoredMapData& m){
 std::ostringstream s;s<<m.structures.size()<<" structures, "<<m.fixtures.size()<<" fixtures, "<<m.lights.size()<<" lights, "<<m.doors.size()<<" doors, "<<m.clutterSpawns.size()<<" props, "<<m.actorTracks.size()<<" actors, "<<m.scriptEvents.size()<<" scripts";return s.str();
}
}

bool testRmcExportRoundTrip(const std::filesystem::path& reportPath){
 std::ofstream report(reportPath);bool ok=true;int passed=0,failed=0;
 report<<"RMC export round trip: export a map, load it with the custom-campaign loader, build the World, compare record by record.\n\n";
 auto check=[&](const std::string& label,const std::vector<RmcMapSource>& maps,const StormCycle& storm,bool chunkWorlds){
  try{
   auto exported=exportRmc(label,maps,0,storm);
   auto loaded=loadCustomCampaignText(exported.text,"roundtrip.rmc");
   std::vector<std::string> diffs;
   if(loaded->maps.size()!=maps.size())diffs.push_back("map count "+std::to_string(maps.size())+" vs "+std::to_string(loaded->maps.size()));
   if(storm.active()&&(loaded->storm.calmMs!=storm.calmMs||loaded->storm.stormMs!=storm.stormMs||loaded->storm.warningMs!=storm.warningMs||loaded->storm.damage!=storm.damage||loaded->storm.ambientAsh!=storm.ambientAsh))diffs.push_back("storm cycle differs");
   for(size_t i=0;i<std::min(maps.size(),loaded->maps.size());++i){
    for(auto& line:compareAuthoredMaps(*maps[i].map,*loaded->maps[i],false))diffs.push_back("map "+std::to_string(i)+": "+line);
    // The World each builds: the campaign chunk (C++ map, then the file as its override) or the custom map.
    if(chunkWorlds&&maps[i].chunk>=0){const int c=maps[i].chunk;setCampaignOverride(c,campaignMap(c));World original(c,WorldId::Campaign);
     setCampaignOverride(c,campaignOverrideFromText(c,exported.text,"roundtrip.rmc"));World replaced(c,WorldId::Campaign);setCampaignOverride(c,nullptr);
     for(auto& line:compareWorlds(original,replaced))diffs.push_back("chunk "+std::to_string(c)+" world: "+line);}
    else{World original(int(i),maps[i].map),replaced(int(i),loaded->maps[i]);for(auto& line:compareWorlds(original,replaced))diffs.push_back("map "+std::to_string(i)+" world: "+line);}
   }
   const bool pass=diffs.empty();ok&=pass;(pass?passed:failed)++;
   report<<(pass?"PASS ":"FAIL ")<<label<<": "<<maps.size()<<" map(s), "<<exported.text.size()/1024<<" KB";
   if(maps.size()==1)report<<", "<<countsOf(*maps[0].map);
   report<<'\n';
   for(const auto& d:diffs)report<<"   differs: "<<d<<'\n';
   for(const auto& n:exported.notes)report<<"   cannot carry: "<<n<<'\n';
  }catch(const std::exception& e){ok=false;++failed;report<<"FAIL "<<label<<": "<<e.what()<<'\n';}
  report.flush();
 };
 // Chapter 1 and Chapter 2 maps (the latter only with --dev-campaign), alone.
 const int chapter1[]={179,0,3,10,14,32,34,56,57,67};
 const int chapter2[]={68,74,78,81,82,90,102,107};
 for(int c:chapter1)check(std::to_string(c)+" "+CampaignMapNames[size_t(c)],{{campaignMap(c),c}},{},true);
 if(devCampaignTools())for(int c:chapter2)check(std::to_string(c)+" "+CampaignMapNames[size_t(c)],{{campaignMap(c),c}},{},true);
 else report<<"SKIP Chapter 2 maps 68, 74, 78, 81, 82, 90, 102, 107: sealed (run with --dev-campaign)\n";
 // Several stitched maps as one campaign (a district).
 {std::vector<RmcMapSource> district;for(int c=34;c<=40;++c)district.push_back({campaignMap(c),c});check("POWER DISTRIBUTION (34-40) as one campaign",district,{},false);}
 // Custom campaigns: the Ashfall outdoor zone (terrain, ground, horizon, storm, shelters) and the Starter-like maps.
 auto custom=[&](const std::string& label,std::shared_ptr<const CustomCampaign> campaign){std::vector<RmcMapSource> maps;for(const auto& m:campaign->maps)maps.push_back({m,-1});check(label,maps,campaign->storm,false);};
 try{custom("Ashfall Exclusion Zone (embedded custom campaign)",loadEmbeddedExclusionZoneCampaign());}catch(const std::exception& e){ok=false;report<<"FAIL Ashfall Exclusion Zone: "<<e.what()<<'\n';}
 for(const char* file:{"custom maps/Starter Map.rmc","custom maps/Editor Starter Reconstruction.rmc","custom maps/Editor Starter Reconstruction Props.rmc","custom maps/Editor Authored Exhibit Gallery.rmc"}){
  if(!std::filesystem::exists(file)){report<<"SKIP "<<file<<": not found beside the exe\n";continue;}
  try{custom(file,loadCustomCampaignFile(file));}catch(const std::exception& e){ok=false;++failed;report<<"FAIL "<<file<<": does not load: "<<e.what()<<'\n';}
 }
 report<<'\n'<<(ok?"PASS":"FAIL")<<": "<<passed<<" passed, "<<failed<<" failed\n";
 return ok;
}

bool testCampaignOverrides(const std::vector<int>& chunks,const std::filesystem::path& reportPath,const std::filesystem::path& keep){
 std::ofstream report(reportPath);bool ok=true;int identical=0,differ=0;
 report<<"Campaign map overrides: export each chunk, load the file as that chunk's override, build the World, compare with the C++ map.\n\n";
 if(!keep.empty())std::filesystem::create_directories(keep);
 for(int c:chunks){
  try{
   if(campaignChunkSealed(c)&&!devCampaignTools()){report<<"SKIP "<<c<<": sealed (run with --dev-campaign)\n";continue;}
   auto original=campaignMap(c);auto exported=exportCampaignChunk(c,false);
   if(!keep.empty()){std::ofstream out(keep/campaignChunkFileName(c),std::ios::binary);out<<exported.text;}
   auto override=campaignOverrideFromText(c,exported.text,campaignChunkFileName(c));
   auto diffs=compareAuthoredMaps(*original,*override,false); // seedScripts sets each event's level from its chunk
   // The C++ map as the chunk (whatever override is embedded), then the file.
   setCampaignOverride(c,original);World built(c,WorldId::Campaign);setCampaignOverride(c,override);World replaced(c,WorldId::Campaign);setCampaignOverride(c,nullptr);
   for(auto& line:compareWorlds(built,replaced))diffs.push_back("world: "+line);
   const bool same=diffs.empty();ok&=same;(same?identical:differ)++;
   report<<(same?"IDENTICAL ":"DIFFERS ")<<c<<' '<<CampaignMapNames[size_t(c)]<<" ("<<campaignChunkLabel(c).district<<"): "<<countsOf(*original)<<'\n';
   for(const auto& d:diffs)report<<"   differs: "<<d<<'\n';
   bool behaviour=false;for(const auto& n:exported.notes){report<<"   note: "<<n<<'\n';behaviour=true;}
   report<<"   hand-over: "<<(same&&!behaviour?"SAFE (every record survives; no C++ outside the map depends on it)":same?"CARE (records survive; see the notes)":"NOT SAFE (records differ)")<<'\n';
  }catch(const std::exception& e){ok=false;++differ;report<<"FAIL "<<c<<' '<<CampaignMapNames[size_t(c)]<<": "<<e.what()<<'\n';}
  report.flush();
 }
 report<<'\n'<<(ok?"PASS":"FAIL")<<": "<<identical<<" identical, "<<differ<<" differ\n";
 return ok;
}

std::vector<int> parseCampaignChunks(const std::string& spec){
 std::vector<int> chunks;std::stringstream list(spec);std::string item;
 auto add=[&](int c){if(c<0||c>=CampaignChunkCount)throw std::runtime_error("no campaign chunk "+std::to_string(c));if(std::find(chunks.begin(),chunks.end(),c)==chunks.end())chunks.push_back(c);};
 while(std::getline(list,item,',')){
  if(item.empty())continue;
  if(item=="all"){for(int c:campaignChunksInPlayOrder())add(c);continue;}
  if(item.rfind("chapter",0)==0){const int chapter=std::atoi(item.c_str()+7);bool any=false;for(int c:campaignChunksInPlayOrder())if(campaignChunkLabel(c).chapter==chapter){add(c);any=true;}if(!any)throw std::runtime_error("no "+item);continue;}
  const auto dash=item.find('-');
  try{if(dash==std::string::npos)add(std::stoi(item));else{const int a=std::stoi(item.substr(0,dash)),b=std::stoi(item.substr(dash+1));if(b<a)throw std::runtime_error("empty range");for(int c=a;c<=b;++c)add(c);}}
  catch(const std::invalid_argument&){throw std::runtime_error("cannot read chunks \""+item+"\"; use 68-109, 0,10,34, chapter1 or all");}
 }
 if(chunks.empty())throw std::runtime_error("no chunks named");
 return chunks;
}

int campaignRmcCommands(const std::vector<std::wstring>& arguments){
 auto find=[&](std::wstring_view flag)->const std::wstring*{for(const auto& a:arguments)if(a==flag||(flag.back()==L'='&&a.starts_with(flag))||(flag.back()!=L'='&&a.starts_with(flag)&&a.size()>flag.size()&&a[flag.size()]==L'='))return &a;return nullptr;};
 auto value=[&](std::wstring_view flag)->std::filesystem::path{auto a=find(flag);return a?std::filesystem::path(a->substr(flag.size())):std::filesystem::path{};};
 auto narrow=[](const std::wstring& w){std::string s;for(wchar_t c:w)s+=c<128?char(c):'?';return s;};
 setDevCampaignTools(find(L"--dev-campaign")!=nullptr);
 // Commands report to a text file (the exe has no console) and never open a window.
 try{
  if(find(L"--campaign-overrides=")){
   std::ofstream log("campaign-overrides.txt");
   if(!devCampaignTools()){log<<"--campaign-overrides needs --dev-campaign\n";return 2;}
   for(const auto& line:loadCampaignOverrideFolder(value(L"--campaign-overrides=")))log<<line<<'\n';
  }
  if(auto spec=find(L"--export-rmc=")){
   std::ofstream report("export-rmc.txt");const auto chunks=parseCampaignChunks(narrow(spec->substr(13)));
   for(int c:chunks)if(campaignChunkSealed(c)&&!devCampaignTools()){report<<"REFUSED: chunk "<<c<<" ("<<CampaignMapNames[size_t(c)]<<") is in a sealed chapter; add --dev-campaign to export it\n";return 3;}
   auto out=value(L"--out=");if(out.empty())out=chunks.size()==1?std::filesystem::path(campaignChunkFileName(chunks[0])):std::filesystem::path("campaign-export");
   // Sealed maps never go where a release zips from.
   bool sealed=false;for(int c:chunks)sealed|=campaignChunkSealed(c);
   if(sealed)for(const auto& part:std::filesystem::absolute(out))if(part=="custom maps"){report<<"REFUSED: sealed maps are never written into a \"custom maps\" folder (it ships); choose a private folder\n";return 4;}
   auto ext=out.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
   if(ext==".rmc"||ext==".txt"){
    // One campaign holding every map named.
    std::vector<RmcMapSource> maps;for(int c:chunks)maps.push_back({campaignMapResolved(c),c});
    std::string name=chunks.size()==1?std::string(CampaignMapNames[size_t(chunks[0])]):std::string(CampaignMapNames[size_t(chunks.front())])+" - "+CampaignMapNames[size_t(chunks.back())];
    auto exported=chunks.size()==1?exportCampaignChunk(chunks[0]):exportRmc(name,maps);if(out.has_parent_path())std::filesystem::create_directories(out.parent_path());
    std::ofstream file(out,std::ios::binary);file<<exported.text;if(!file){report<<"ERROR: cannot write "<<out.string()<<'\n';return 5;}
    report<<"WROTE "<<out.string()<<": "<<chunks.size()<<" map(s)\n";for(const auto& n:exported.notes)report<<"  cannot carry: "<<n<<'\n';
   }else{
    // One file per map, and an index of them.
    std::filesystem::create_directories(out);std::ofstream index(out/"campaign-maps.txt");
    index<<"# chunk|chapter|district|name|sealed|file (RawMetal.exe --export-rmc)\n";
    for(int c:chunks){auto exported=exportCampaignChunk(c);const auto file=campaignChunkFileName(c);std::ofstream f(out/file,std::ios::binary);f<<exported.text;
     if(!f){report<<"ERROR: cannot write "<<(out/file).string()<<'\n';return 5;}
     const auto label=campaignChunkLabel(c);index<<c<<'|'<<label.chapter<<'|'<<label.district<<'|'<<CampaignMapNames[size_t(c)]<<'|'<<campaignChunkSealed(c)<<'|'<<file<<'\n';
     report<<"WROTE "<<(out/file).string()<<'\n';for(const auto& n:exported.notes)report<<"  cannot carry: "<<n<<'\n';}
   }
   return 0;
  }
  if(find(L"--campaign-map-list")){
   auto out=value(L"--out=");if(out.empty())out="campaign-maps.txt";if(out.has_parent_path())std::filesystem::create_directories(out.parent_path());
   std::ofstream list(out);list<<"# chunk|chapter|district|name|sealed|override (RawMetal.exe --campaign-map-list)\n";
   for(int c:campaignChunksInPlayOrder()){if(campaignChunkSealed(c)&&!devCampaignTools())continue;const auto label=campaignChunkLabel(c);
    list<<c<<'|'<<label.chapter<<'|'<<label.district<<'|'<<CampaignMapNames[size_t(c)]<<'|'<<campaignChunkSealed(c)<<'|'<<(campaignOverride(c)?1:0)<<'\n';}
   return list?0:5;
  }
  if(find(L"--canonical-rmc=")){
   // A custom campaign file written again the way the exporter writes it, so two files
   // can be compared record for record whatever editor saved them (merge_edits.py).
   const auto in=value(L"--canonical-rmc=");auto out=value(L"--out=");if(out.empty())out="canonical.rmc";
   auto campaign=loadCustomCampaignFile(in);std::vector<RmcMapSource> maps;for(const auto& m:campaign->maps)maps.push_back({m,-1});
   std::ofstream file(out,std::ios::binary);file<<exportRmc(campaign->name,maps,campaign->startMap,campaign->storm).text;return file?0:5;
  }
  if(find(L"--export-roundtrip-test"))return testRmcExportRoundTrip("export-roundtrip-test.txt")?0:21;
  if(find(L"--campaign-admin-test")){
   // The admin gate without its PIN: the hash agrees with Python's SHA-256 on a known
   // answer, wrong names and PINs fail, and a remembered unlock opens sealed levels in a
   // public copy and Lock shuts them. The user's own setting is put back afterwards.
   std::ofstream report("campaign-admin-test.txt");bool ok=true;auto check=[&](bool pass,const char* what){report<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;};
   check(admin::hash("Test","1234")=="888f80ff6514d6ad2ecbd5cf6a6d7d4a2564974384f4ee95d49f5d035ff3b65e","salted SHA-256 matches a known answer");
   const auto savedName=admin::read(L"Admin"),savedKey=admin::read(L"AdminKey");
   check(!admin::unlock("Nobody","1234"),"a name off the whitelist does not unlock");
   check(!admin::unlock(admin::Whitelist[0].name,"0000"),"a wrong PIN does not unlock");
   auto put=[&](const std::wstring& n,const std::wstring& k){RegSetKeyValueW(HKEY_CURRENT_USER,admin::Key,L"Admin",REG_SZ,n.c_str(),DWORD((n.size()+1)*2));RegSetKeyValueW(HKEY_CURRENT_USER,admin::Key,L"AdminKey",REG_SZ,k.c_str(),DWORD((k.size()+1)*2));};
   const std::string name=admin::Whitelist[0].name,key=admin::Whitelist[0].hash;put(std::wstring(name.begin(),name.end()),std::wstring(key.begin(),key.end()));
   check(admin::unlocked()==name,"a remembered unlock names its admin");
   check(devCampaignTools(),"unlocked: sealed levels are open");
   admin::lock();check(admin::unlocked().empty(),"Lock forgets the unlock");
   check(!publicRelease()||!devCampaignTools(),"locked in a public copy: sealed levels are hidden again");
   if(!savedName.empty()||!savedKey.empty())put(savedName,savedKey);
   report<<(publicRelease()?"(public copy)\n":"(not a public copy: sealed levels stay open to it anyway)\n")<<(ok?"PASS\n":"FAIL\n");return ok?0:21;
  }
  if(auto spec=find(L"--campaign-override-test")){
   std::string chunks=spec->size()>25?narrow(spec->substr(25)):devCampaignTools()?"chapter2":"chapter1";
   auto keep=value(L"--keep=");if(keep.empty())keep=".build/campaign-override-test";
   return testCampaignOverrides(parseCampaignChunks(chunks),"campaign-override-test.txt",keep)?0:21;
  }
 }catch(const std::exception& e){std::ofstream("campaign-rmc-error.txt")<<"ERROR: "<<e.what()<<'\n';return 6;}
 return -1;
}
}
