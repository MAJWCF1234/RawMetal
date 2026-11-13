#include "EditorPreview.h"
#ifdef _WIN32
#include "Win32Window.h"
#include "../world/CustomCampaign.h"
#include "../world/CampaignExport.h"
#include "EditorChannel.h"
#include "../core/FrameProfile.h"
#include "../renderer/SoftwareRenderer.h"
#include "../renderer/Projection.h"
#include "../renderer/Particles.h"
#include "../game/FilmPuppet.h"
#include "../audio/AudioEngine.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <string_view>
#include <unordered_set>

namespace retro {
std::vector<std::array<float,6>> resolveEditorLeadEndpoints(const std::string& text,int map){
 auto loaded=loadCustomCampaignText(text,"editor-cable-copy.rmc");
 if(map<0||map>=int(loaded->maps.size()))throw std::runtime_error("Cable copy map is outside this campaign");
 auto campaign=std::make_shared<CustomCampaign>(*loaded);campaign->startMap=map;Game game(campaign);
 const auto& world=game.world();const auto& leads=world.leads();const auto& ropes=world.leadRopes();
 std::vector<std::array<float,6>> result;result.reserve(leads.size());
 for(size_t i=0;i<leads.size();++i){const auto& lead=leads[i];auto end=lead.end;
  if(!lead.loose&&lead.prop>=0&&i<ropes.size()&&!ropes[i].points.empty())end=ropes[i].points.back();
  result.push_back({lead.anchor.x,lead.anchor.y,lead.anchor.z,end.x,end.y,end.z});}
 return result;
}
int validateEditorCampaign(const std::filesystem::path& path){
 auto campaign=loadCustomCampaignFile(path);
 for(size_t i=0;i<campaign->maps.size();++i){World validated(int(i),campaign->maps[i]);}
 std::cout<<"OK "<<campaign->maps.size()<<'\n';return 0;
}
int exportEditorCampaign(const std::filesystem::path& path){
 auto campaign=loadCustomCampaignFile(path);std::vector<RmcMapSource> maps;
 for(const auto& m:campaign->maps)maps.push_back({m,-1});
 auto exported=exportRmc(campaign->name,maps,campaign->startMap);
 std::ofstream out(path.wstring()+L".exported.rmc",std::ios::binary);out<<exported.text;
 if(!out)throw std::runtime_error("Cannot write the exported campaign");
 std::cout<<"OK "<<campaign->maps.size()<<' '<<exported.notes.size()<<'\n';return 0;
}
int validateEditorCampaignText(const std::string& text){
 // The parse builds every map's World to check it (the file check above builds them twice).
 auto campaign=loadCustomCampaignText(text,"working.rmc");
 std::cout<<"OK "<<campaign->maps.size()<<'\n';return 0;
}
namespace{
// Records the parser files under one map: field 1 is the map's index
// (CustomCampaign.cpp: mapAt, layerAt). A tag that is not listed counts as
// campaign-wide and is reparsed with every change, which is only slower.
bool mapScopedTag(std::string_view tag){
 static const std::unordered_set<std::string_view> tags{"AMBIENT","REVERB","ACTOR","ACTOR_AI","ACTOR_FACTION","ACTOR_GUARD","ACTOR_HEALTH","ACTOR_IDLE_KEY","ACTOR_KEY","ACTOR_PATROL","ACTOR_PLATFORM","ACTOR_PRELUDE","ACTOR_ROLE","ACTOR_STANCE","ACTOR_SUSPEND","ACTOR_TACTICS","ACTOR_TALK","ACTOR_VOICE","ACTOR_WEAPON","BREAKABLE","CARGO_LIFT","CLUTTER","COMPACTOR","CRATE","CRATE_PLATE","CRATE_RESET","CREATURE","DARK","DIALOGUE","DIALOGUE_REPLY","DOOR","EMITTER","FIXTURE","HAZARD","HUNG_SUIT","LAYER","LEAD","LIGHT","MAP","NAV_HINT","OBJECTIVE_TEXT","PICKUP","PIPE","PRIM","PROP","ROW","SCRIPT_ACTION","SCRIPT_EVENT","SCRIPT_REQUIRE","SCRIPT_TRIGGER","SEQUENCE_CUE","SEQUENCE_SIGHT","SHELTER","SIGN","STAIR","STRUCT","SURFACE","TERMINAL","TIMED_SEQUENCE","VISIBLE_GROUP","WATER","ZONE"};
 return tags.count(tag)!=0;
}
// Read for every map whichever map changed: the maps and their floors (with
// their 24 rows) must all exist, and objective and trigger names are checked
// across the whole campaign.
bool parsedForEveryMap(std::string_view tag){return tag=="MAP"||tag=="LAYER"||tag=="ROW"||tag=="OBJECTIVE_TEXT"||tag=="SCRIPT_EVENT"||tag=="SCRIPT_ACTION"||tag=="SCRIPT_REQUIRE"||tag=="SCRIPT_TRIGGER";}
std::string_view tagOf(std::string_view line){if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);return line.substr(0,line.find('|'));}
template<class F> void eachLine(const std::string& text,F&& f){size_t at=0;while(at<text.size()){size_t end=text.find('\n',at);if(end==std::string::npos)end=text.size();f(std::string_view(text).substr(at,end-at));at=end+1;}}
}
LiveCampaign::Split LiveCampaign::split(const std::string& text){
 Split s;s.global=1469598103934665603ull;
 auto mix=[](std::uint64_t& h,std::string_view line){for(unsigned char c:line){h^=c;h*=1099511628211ull;}h^=0x0a;h*=1099511628211ull;};
 eachLine(text,[&](std::string_view line){
  int owner=-1;auto tag=tagOf(line);
  // Editor ownership is a comment, not a campaign-wide world change. Claiming
  // an auto item during a drag must retain the single-map live-update path.
  if(!tag.empty()&&tag.front()=='#'){s.owner.push_back(-1);return;}
  if(mapScopedTag(tag)){
   auto rest=line.substr(std::min(line.size(),tag.size()+1));if(!rest.empty()&&rest.back()=='\r')rest.remove_suffix(1);
   auto index=rest.substr(0,rest.find('|'));
   if(!index.empty()&&index.size()<4&&std::all_of(index.begin(),index.end(),[](char c){return c>='0'&&c<='9';}))owner=std::stoi(std::string(index));
  }
  s.owner.push_back(owner);
  if(owner<0)mix(s.global,line);
  else{if(size_t(owner)>=s.maps.size())s.maps.resize(size_t(owner)+1,1469598103934665603ull);mix(s.maps[size_t(owner)],line);}
 });
 return s;
}
std::shared_ptr<const CustomCampaign> LiveCampaign::mapOnly(const std::string& text,const Split& s,int map,const std::string& source,const std::shared_ptr<const CustomCampaign>& good){
 // The campaign-wide lines, the lines every map needs and this map's own, in file order.
 std::string part;part.reserve(text.size()/8);size_t i=0;
 eachLine(text,[&](std::string_view line){int owner=s.owner[i++];if(owner<0||owner==map||parsedForEveryMap(tagOf(line))){part.append(line);part.push_back('\n');}});
 auto parsed=loadCustomCampaignText(std::move(part),source,std::vector<char>{});
 if(parsed->maps.size()!=good->maps.size())throw std::runtime_error("map count changed");
 auto next=std::make_shared<CustomCampaign>(*parsed);
 for(size_t j=0;j<next->maps.size();++j)if(int(j)!=map)next->maps[j]=good->maps[j];
 // One campaign-wide terrain, as a whole parse shares it.
 auto own=std::make_shared<AuthoredMapData>(*parsed->maps[size_t(map)]);own->terrain=good->maps[size_t(map)]->terrain;next->maps[size_t(map)]=std::move(own);
 return next;
}
std::shared_ptr<const CustomCampaign> LiveCampaign::load(const std::string& text,int shown,const std::string& source,std::string& how){
 auto s=split(text);
 std::vector<int> changed;bool comparable=m_good&&s.global==m_split.global&&s.maps.size()==m_split.maps.size()&&s.maps.size()==m_good->maps.size();
 if(comparable)for(size_t j=0;j<s.maps.size();++j)if(s.maps[j]!=m_split.maps[j])changed.push_back(int(j));
 if(comparable&&changed.empty()){how="unchanged";return m_good;}
 if(comparable&&changed.size()==1&&changed[0]==shown){
  // A refusal here may only be the slice's own line numbers or a check that
  // needs another map: the whole parse below gives the exact answer.
  try{auto next=mapOnly(text,s,shown,source,m_good);m_good=next;m_split=std::move(s);how="map only";return next;}catch(const std::exception&){}
 }
 // The whole campaign. The map on show is built by the scene right after;
 // other maps whose records changed are built here to check them. A brand
 // new scene builds every map itself.
 std::vector<char> validate(s.maps.size(),0);
 if(comparable&&shown>=0)for(int j:changed)if(j!=shown)validate[size_t(j)]=1;
 auto next=loadCustomCampaignText(text,source,validate);
 m_good=next;m_split=std::move(s);how="whole campaign";return next;
}
bool LiveCampaign::test(const std::filesystem::path& file,std::string& report){
 std::ifstream in(file,std::ios::binary);std::string text(std::istreambuf_iterator<char>(in),{});
 auto full=loadCustomCampaignText(text,"working.rmc");auto s=split(text);std::ostringstream out;int failures=0;
 // Every map read on its own matches the whole parse, and the others are shared untouched.
 for(int k=0;k<int(full->maps.size());++k){
  auto part=mapOnly(text,s,k,"working.rmc",full);
  auto differences=compareAuthoredMaps(*full->maps[size_t(k)],*part->maps[size_t(k)],true,3);
  bool shared=true;for(size_t j=0;j<full->maps.size();++j)if(int(j)!=k&&part->maps[j]!=full->maps[j])shared=false;
  if(!differences.empty()||!shared||part->name!=full->name||part->startMap!=full->startMap){++failures;out<<"map "<<k<<": "<<(differences.empty()?"maps not shared":differences.front())<<'\n';}
 }
 // An edit on one map (a piece of furniture moved a quarter metre) takes the
 // map-only path and equals a whole parse of the edited text; so does moving it back.
 using Clock=std::chrono::steady_clock;auto ms=[](Clock::time_point a){return std::chrono::duration<double,std::milli>(Clock::now()-a).count();};
 LiveCampaign live;std::string how;auto t0=Clock::now();live.load(text,-1,"working.rmc",how);const double wholeMs=ms(t0);
 auto first=text.find("\nFIXTURE|");int edits=0;double sliceMs=0;
 if(first!=std::string::npos){
  auto edited=text;auto end=edited.find('\n',first+1);auto line=edited.substr(first+1,end-first-1);
  std::vector<std::string> f;{std::string_view rest=line;for(;;){auto bar=rest.find('|');f.emplace_back(rest.substr(0,bar));if(bar==std::string_view::npos)break;rest.remove_prefix(bar+1);}}
  const int map=std::stoi(f[1]);f[3]=std::to_string(std::stof(f[3])+.25f);
  std::string moved;for(size_t i=0;i<f.size();++i)moved+=(i?"|":"")+f[i];edited.replace(first+1,line.size(),moved);
  t0=Clock::now();auto result=live.load(edited,map,"working.rmc",how);sliceMs=ms(t0);auto whole=loadCustomCampaignText(edited,"working.rmc");
  auto differences=compareAuthoredMaps(*whole->maps[size_t(map)],*result->maps[size_t(map)],true,3);++edits;
  if(how!="map only"||!differences.empty()){++failures;out<<"edited map "<<map<<": "<<how<<(differences.empty()?"":": "+differences.front())<<'\n';}
  result=live.load(text,map,"working.rmc",how);differences=compareAuthoredMaps(*full->maps[size_t(map)],*result->maps[size_t(map)],true,3);++edits;
  if(how!="map only"||!differences.empty()){++failures;out<<"restored map "<<map<<": "<<how<<'\n';}
  // The same text again changes nothing.
  live.load(text,map,"working.rmc",how);++edits;if(how!="unchanged"){++failures;out<<"same text: "<<how<<'\n';}
 }
 std::ostringstream timing;timing.setf(std::ios::fixed);timing.precision(1);
 timing<<"; first read (no map built) "<<wholeMs<<" ms, one map's edit "<<sliceMs<<" ms";
 report=out.str()+std::to_string(full->maps.size())+" maps read one at a time, "+std::to_string(edits)+" live edits, "+std::to_string(failures)+" failures"+timing.str();
 return failures==0;
}
Win32Window* createEditorViewport(HWND parent,bool visible){
 return new Win32Window(DisplayWidth,DisplayHeight,L"Raw Metal engine viewport",parent,visible);
}
void destroyEditorViewport(Win32Window* window){
 if(!window)return;
 window->abandonIfClosed();
 delete window;
}
int runEditorPreview(const EditorPreviewOptions& options){
 if(options.pack.empty()||options.control.empty()||options.status.empty())throw std::runtime_error("Editor preview requires pack, control and status files");
 constexpr int width=DisplayWidth,height=DisplayHeight;
 // The builder passes a window it created on its own thread. Making the child
 // here would put it on the picture thread, and Windows can then destroy the
 // builder window (the editor vanishes and the menu comes back).
 std::unique_ptr<Win32Window> owned;
 Win32Window* view=options.sharedWindow;
 if(!view){owned=std::make_unique<Win32Window>(width,height,L"Raw Metal engine viewport",options.parent,!options.hidden);view=owned.get();}
 if(!view||!view->valid())throw std::runtime_error("Cannot create engine preview child window");
 Win32Window& window=*view;
 auto renderer=std::make_unique<SoftwareRenderer>(width,height);
 renderer->setEnvironmentInspection(true);
 // Test captures use the same offscreen Vulkan path as native map shots.
 if(!renderer->enableHardware(options.capture.empty()&&!options.hidden?window.handle():nullptr))throw std::runtime_error("Cannot initialize engine preview renderer");
 // Frozen authoring and hidden checks do not need a sound device or decode
 // the full soundtrack. Initialize native audio only when explicitly requested.
 std::unique_ptr<AudioEngine> audio;
 std::unique_ptr<Game> scene;
 long long revision=-1;int selected=-1,attemptedMap=-1;bool playing=false,valid=false,haveEditCamera=false;
 // Play keeps the builder's view: the camera at the moment play began comes back on Escape.
 Player editCamera{};std::shared_ptr<CustomCampaign> current;std::string loadError;unsigned long long frame=0;
 // The control and status files: memory when the builder is this process,
 // else small files (renamed into place so a reader never sees half of one).
 auto put=[&](const std::filesystem::path& path,const std::string& text){
  if(options.inProcess){editorchannel::write(path,text);return;}
  auto temporary=path;temporary+=L".tmp";
  {std::ofstream out(temporary,std::ios::binary);out<<text;}
  MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);
 };
 auto get=[&](const std::filesystem::path& path){return editorchannel::text(path,options.inProcess);};
 // The builder hears at once when a change is in the picture (its 50 ms poll stays as a fallback).
 const HWND host=options.parent?GetAncestor(options.parent,GA_ROOT):nullptr;
 auto report=[&](const std::string& state,const std::string& detail=""){
  std::ostringstream out;out<<state<<' '<<revision<<' '<<selected<<' '<<int(playing)<<' '<<frame<<'\n'<<detail;
  put(options.status,out.str());
 };
 auto last=std::chrono::steady_clock::now(),lastReport=last,lastHover=last;
 // --perf-trace: what a reload costs (read and parse, world, first picture)
 // and the frame times, summed every two seconds. Off unless asked for.
 using Clock=std::chrono::steady_clock;
 auto ms=[](Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();};
 struct FrameStats{int n=0;double work=0,workMax=0,period=0,periodMax=0,render=0,renderMax=0,control=0,controlMax=0;}stats;
 auto statsAt=last,previousFrame=last;std::string pendingPicture;Clock::time_point reloadStarted{};
 auto perfLine=[&](const std::string& line){if(options.perf.empty())return;std::ofstream out(options.perf,std::ios::app);out<<GetTickCount64()<<" preview "<<line<<'\n';};
 // Simulate (the builder's F6): the map runs in the picture around the free
 // camera without entering play. The scene as it stood is set aside and comes
 // back on stop, so nothing that ran survives. simulate.txt beside the control
 // file asks for it ("on fireSerial x1 y1 x2 y2": a new serial fires the
 // trigger with that box); simulate-status.txt answers with where people and
 // creatures are and which triggers just fired, for the plan.
 const auto simControl=options.control.parent_path()/L"simulate.txt",simStatus=options.control.parent_path()/L"simulate-status.txt";
 std::unique_ptr<Game> authored;bool simulating=false,simKeyHeld=false,simEscapeHeld=false;int fireSerial=0;float simTime=0;
 struct Pulse{std::array<float,4> box;float at;};std::vector<Pulse> pulses;auto lastSimReport=last;
 auto reportSim=[&]{
  {std::ostringstream out;out.setf(std::ios::fixed);out.precision(3);
   out<<"SIM "<<int(simulating)<<' '<<simTime;
   if(scene)out<<' '<<scene->player().pos.x<<' '<<scene->player().pos.y<<' '<<scene->player().z;
   out<<'\n';
   if(scene){
    const auto& people=scene->npcActors();
    for(size_t i=0;i<people.size();++i)if(people[i].level==scene->level()){
     const bool alert=people[i].track().ai.mode==ActorAiMode::Guard&&scene->guardState(i)!=GuardState::Patrol;
     out<<"P "<<people[i].pose.position.x<<' '<<people[i].pose.position.y<<' '<<int(alert)<<'\n';
    }
    for(const auto& e:scene->enemies())if(e.alive)out<<"C "<<e.pos.x<<' '<<e.pos.y<<'\n';
   }
   for(const auto& p:pulses)if(simTime-p.at<1.5f)out<<"F "<<p.box[0]<<' '<<p.box[1]<<' '<<p.box[2]<<' '<<p.box[3]<<' '<<simTime-p.at<<'\n';
   put(simStatus,out.str());
  }
 };
 // The picture's own F6 or Escape answers in the same file the builder writes.
 auto askSim=[&](bool on){
  put(simControl,std::to_string(int(on))+' '+std::to_string(fireSerial)+" 0 0 0 0\n");
 };
 // Back to the authored scene, the camera where it is now.
 auto stopSim=[&]{
  if(!simulating)return;
  auto camera=scene->player();scene=std::move(authored);scene->restoreEditorCamera(camera);
  simulating=false;simTime=0;pulses.clear();reportSim();
 };
 // The builder's lighting view and play options: look.txt beside the control file,
 // "LOOK mode third rebake animate". mode 0 lit, 1 the probes' fill alone (--detail-probes),
 // 2 the finish (--detail-finish: crystal red, metal blue, polish grey), 3 no probes (--no-probes);
 // third 1 plays over Claire's shoulder; a new rebake number bakes every map's probes again;
 // animate 1 runs the clock between edits, so the air moves in the picture. look-status.txt
 // answers with what the picture is doing.
 const auto lookControl=options.control.parent_path()/L"look.txt",lookStatus=options.control.parent_path()/L"look-status.txt";
 int lookMode=-1,lookThird=0,lookRebake=0,lookAnimate=1;bool thirdShown=false;auto lastLook=last;
 auto reportLook=[&]{
  std::ostringstream out;out.setf(std::ios::fixed);out.precision(3);
  const auto& air=renderer->particleMemory().last;
  out<<"LOOK "<<std::max(0,lookMode)<<' '<<lookThird<<' '<<int(playing)<<' '<<SoftwareRenderer::lightProbeBakes()<<' '<<SoftwareRenderer::lastLightProbeBakeMs()
     <<' '<<air.sources<<' '<<air.particles<<' '<<(scene?scene->elapsed():0.f)<<' '<<int(thirdShown&&renderer->claireOutsideReport().drawn)<<' '<<lookRebake<<'\n';
  put(lookStatus,out.str());
 };
 // The Filmmaker's preview (film.txt beside the control file, editor/NativeFilmmaker.inc):
 // "FILM 1 X Y EYE YAW PITCH FOV" (metres, degrees; FOV across, 0 the game's) and then
 // "pose aK|eK X Y FEET YAW CLIP PHASE" lines (game/FilmPuppet.h) put the picture at the
 // shot's camera at the playhead with its people and creatures posed; "FILM 0" gives the
 // free camera and the authored poses back. film-status.txt answers "VIEW X Y EYE YAW PITCH"
 // (the free camera, degrees) and whether the preview is on, so the Filmmaker can key the view.
 const auto filmControl=options.control.parent_path()/L"film.txt",filmStatus=options.control.parent_path()/L"film-status.txt";
 bool filmOn=false;long long filmRevision=-1;Player filmFree{};float filmX=0,filmY=0,filmEye=0,filmYaw=0,filmPitch=0,filmFov=0;
 std::vector<FilmPuppet> filmHeld;std::vector<ActorPose> filmWorldPoses,filmNpcPoses;std::vector<Enemy> filmEnemies;int filmLevel=-1;
 auto filmSnapshot=[&]{
  filmWorldPoses.clear();filmNpcPoses.clear();filmLevel=scene->level();filmRevision=revision;
  for(size_t i=0;i<scene->world().actorTracks().size();++i)filmWorldPoses.push_back(scene->world().actorPose(i));
  for(const auto& npc:scene->npcActors())filmNpcPoses.push_back(npc.pose);
  filmEnemies=scene->chunkEnemies(filmLevel);
 };
 auto filmRestore=[&]{
  if(!scene||filmRevision!=revision||filmLevel!=scene->level())return;
  auto& world=const_cast<World&>(scene->world());
  for(size_t i=0;i<filmWorldPoses.size()&&i<world.actorTracks().size();++i)world.setActorPose(i,filmWorldPoses[i]);
  auto& people=const_cast<std::vector<FriendlyActor>&>(scene->npcActors());
  for(size_t i=0;i<filmNpcPoses.size()&&i<people.size();++i)people[i].pose=filmNpcPoses[i];
  auto& foes=const_cast<std::vector<Enemy>&>(scene->chunkEnemies(filmLevel));if(foes.size()==filmEnemies.size())foes=filmEnemies;
 };
 auto reportFilm=[&]{
  if(!scene)return;const auto& v=filmOn?filmFree:scene->player();
  std::ostringstream out;out.setf(std::ios::fixed);out.precision(3);
  out<<"VIEW "<<v.pos.x<<' '<<v.pos.y<<' '<<v.z+v.eye<<' '<<v.angle*180.f/kPi<<' '<<v.pitch/140.f*180.f/kPi<<' '<<int(filmOn)<<'\n';
  put(filmStatus,out.str());
 };
 int lastWidth=-1,lastHeight=-1;LiveCampaign live;
 // Every map stored in the scene matches the document (false after a
 // campaign-wide change was applied to the map on show only).
 bool sceneCurrent=false;
 int pickSerial=0,orbitX=-1,orbitY=-1;bool pickHeld=false;std::string pickDetail;
 float pickOx=0,pickOy=0,pickOz=0,pickDx=0,pickDy=0,pickDz=0;
 auto formatPick=[&](bool held){std::ostringstream ray;ray.setf(std::ios::fixed);ray.precision(4);ray<<"PICK "<<pickSerial<<' '<<int(held)<<' '<<pickOx<<' '<<pickOy<<' '<<pickOz<<' '<<pickDx<<' '<<pickDy<<' '<<pickDz<<' '<<int((GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);return ray.str();};
 auto cameraRay=[&](int px,int py,float& ox,float& oy,float& oz,float& dx,float& dy,float& dz){
  const auto& v=scene->player();
  const float ca=std::cos(v.angle),sa=std::sin(v.angle),cp=std::cos(v.pitch/140.f),sp=std::sin(v.pitch/140.f);
  const float focal=Projection{float(DisplayHeight)/float(DisplayWidth)}.focal(float(DisplayWidth));
  RECT client{};GetClientRect(window.handle(),&client);
  const float aspect=float(std::max(1L,client.bottom))/float(std::max(1L,client.right));
  const float nx=(px-DisplayWidth*.5f)/focal,ny=(DisplayHeight*.5f-py)/focal*aspect/(float(DisplayHeight)/DisplayWidth);
  ox=v.pos.x;oy=v.pos.y;oz=v.z+v.eye;
  dx=ca*cp-sa*nx-ca*sp*ny;dy=sa*cp+ca*nx-sa*sp*ny;dz=sp+cp*ny;
 };
 // A window the builder created is pumped on the builder's thread. Peeking
 // every message here would take that thread's input, and can close it.
 while(options.sharedWindow?IsWindow(window.handle()):window.pump()){
  const auto loopStart=Clock::now();
  if(options.parent){if(!IsWindow(options.parent))break;RECT rc{};GetClientRect(options.parent,&rc);if(rc.right!=lastWidth||rc.bottom!=lastHeight){lastWidth=rc.right;lastHeight=rc.bottom;SetWindowPos(window.handle(),nullptr,0,0,std::max(1L,rc.right),std::max(1L,rc.bottom),SWP_NOZORDER|SWP_NOACTIVATE);}}
  long long requested=revision;int map=std::max(0,selected),play=playing?1:0,stop=0,audition=0,spot=0;float spotX=0,spotY=0,spotZ=0;
  std::vector<EditorFloorView> editorFloors;std::vector<EditorGuide> editorGuides;
  {std::istringstream control(get(options.control));if(control>>requested>>map>>play>>stop)control>>audition>>spot>>spotX>>spotY>>spotZ;
   int count=0;if(control>>count&&count>=0&&count<=32){for(int i=0;i<count;++i){EditorFloorView f;
    if(!(control>>f.bottom>>f.top>>f.opacity))break;for(auto& row:f.mask)control>>row;editorFloors.push_back(f);}}
   count=0;if(control>>count&&count>=0&&count<=512)for(int i=0;i<count;++i){EditorGuide g;if(!(control>>g.ax>>g.ay>>g.az>>g.bx>>g.by>>g.bz>>g.kind))break;editorGuides.push_back(g);}
  }
  if(stop)break;
  int simWanted=0,simFire=fireSerial;float fireX1=0,fireY1=0,fireX2=0,fireY2=0;
  {std::istringstream ask(get(simControl));ask>>simWanted>>simFire>>fireX1>>fireY1>>fireX2>>fireY2;}
  {int mode=0,third=0,rebake=lookRebake,animate=1;std::string tag;std::istringstream ask(get(lookControl));
   if(ask>>tag>>mode>>third>>rebake>>animate&&tag=="LOOK"){
    if(mode!=lookMode){lookMode=std::clamp(mode,0,3);SoftwareRenderer::setLightProbes(lookMode!=3);SoftwareRenderer::setDetailLayer(lookMode==1?10:lookMode==2?11:1);}
    if(rebake!=lookRebake){lookRebake=rebake;SoftwareRenderer::rebakeLightProbes();}
    lookThird=third!=0;lookAnimate=animate!=0;}}
  bool filmWanted=false;std::vector<FilmPuppet> filmAsked;
  {std::istringstream ask(get(filmControl));std::string tag;int on=0;
   if(ask>>tag>>on&&tag=="FILM"&&on&&ask>>filmX>>filmY>>filmEye>>filmYaw>>filmPitch>>filmFov){filmWanted=true;
    for(std::string line;std::getline(ask,line);){std::istringstream l(line);std::string op;FilmPuppet p;if(l>>op&&op=="pose"&&readFilmPuppet(l,p))holdFilmPuppet(filmAsked,p);}}}
  const auto controlRead=Clock::now();
  // An edit, another map or play ends Simulate first: the change lands on the authored scene.
  if(simulating&&(requested!=revision||map!=attemptedMap||play!=0||!simWanted))stopSim();
  if(requested!=revision||map!=attemptedMap){
   attemptedMap=map;
   // Keep the last valid scene on an invalid edit; expose the actual parser error.
   reloadStarted=Clock::now();
   try{
    // The document: shared with the builder in memory, or its file. Only the
    // map that changed is reparsed (LiveCampaign above).
    std::shared_ptr<const std::string> document=options.inProcess?editorchannel::read(options.pack):nullptr;
    if(!options.inProcess){std::ifstream in(options.pack,std::ios::binary);if(!in)throw std::runtime_error("Cannot open campaign");document=std::make_shared<const std::string>(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());}
    if(!document)throw std::runtime_error("Cannot open campaign");
    // A new scene (first picture, another map) builds every map itself.
    const bool sameScene=scene&&selected==map&&scene->worldId()==WorldId::Custom;
    std::string how;auto loaded=options.legacyReload?(how="whole campaign, every map built",loadCustomCampaignText(*document,options.pack.filename().string())):live.load(*document,sameScene?map:-1,options.pack.filename().string(),how);
    const auto parsed=Clock::now();bool adopted=false;
    if(map<0||map>=int(loaded->maps.size()))throw std::runtime_error("Preview map is outside this campaign");
    auto campaign=std::make_shared<CustomCampaign>(*loaded);campaign->startMap=map;current=campaign;
    // Same map: rebuild that room in the session already running. A new game
    // constructs every unused chunk, which is the hitch while placing a wall.
    if(scene&&selected==map&&scene->worldId()==WorldId::Custom){if(how!="unchanged"){scene->adoptEditorCampaign(campaign);adopted=true;}if(how=="whole campaign")sceneCurrent=false;}
    // Another map of the same, unchanged campaign: every map is built in this
    // scene already, so step across to it (Game::switchEditorMap) instead of
    // building a new game, which builds and lights all of them again. Only when
    // every stored map is known to match the document (no campaign-wide change
    // since the scene was built) and nobody is playing.
    else if(!options.legacyReload&&scene&&sceneCurrent&&!playing&&how=="unchanged"&&scene->worldId()==WorldId::Custom&&int(campaign->maps.size())==scene->chunkCount()){
     scene->switchEditorMap(campaign,map);scene->update({},1.f/60);how="unchanged, other map";
    }
    else{
     sceneCurrent=true;
     auto next=std::make_unique<Game>(campaign);
     renderer->forgetScene(); // a new game: nothing baked for the last one is this one's
     if(scene&&selected==map)next->restoreEditorCamera(scene->player());
     wchar_t local[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
     if(n>0&&n<32768)next->loadSettings(std::wstring(local)+L"\\RawMetal\\settings.ini");
     next->update({},1.f/60);
     scene=std::move(next);
    }
    selected=map;revision=requested;valid=true;loadError.clear();report("READY");reportSim();
    if(host&&!options.legacyReload)PostMessageW(host,WM_APP+41,0,0);
    if(!options.perf.empty()){std::ostringstream line;line.setf(std::ios::fixed);line.precision(2);const auto built=Clock::now();
     line<<"rev "<<requested<<" map "<<map<<": control seen after "<<ms(loopStart,controlRead)<<" ms, read+parse ("<<how<<") "<<ms(reloadStarted,parsed)<<" ms, world "<<(adopted?"rebuild ":how=="unchanged, other map"?"switch ":"new game ")<<ms(parsed,built)<<" ms";
     pendingPicture=line.str();}
   }catch(const std::exception& error){revision=requested;valid=false;loadError=error.what();report("ERROR",loadError);if(host)PostMessageW(host,WM_APP+41,0,0);}
  }
  auto now=std::chrono::steady_clock::now();float dt=std::min(.05f,std::chrono::duration<float>(now-last).count());last=now;
  // spot 1 starts a fresh run at the map's own start; spot 2 drops in at the plan pointer.
  if(play!=0&&valid&&!playing&&scene){
   editCamera=scene->player();haveEditCamera=true;
   if(spot==1&&current){auto next=std::make_unique<Game>(current);wchar_t local[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(n>0&&n<32768)next->loadSettings(std::wstring(local)+L"\\RawMetal\\settings.ini");next->update({},1.f/60);scene=std::move(next);}
   else if(spot==2&&std::isfinite(spotX)&&std::isfinite(spotY)){auto at=scene->player();at.pos.x=spotX;at.pos.y=spotY;at.z=spotZ;at.pitch=0;scene->restoreEditorCamera(at);}
  }
  if(playing&&!(play!=0&&valid)&&scene&&haveEditCamera){scene->restoreEditorCamera(editCamera);haveEditCamera=false;}
  playing=play!=0&&valid;
  // Over the shoulder in play when asked: the renderer draws her from outside only out of inspection.
  {const bool third=playing&&lookThird;if(third!=thirdShown){thirdShown=third;renderer->setEnvironmentInspection(!third);SoftwareRenderer::setThirdPerson(third);}}
  // The Filmmaker's preview: never in play or Simulate, which own the picture.
  if(scene&&filmWanted&&!playing&&!simulating){
   if(!filmOn){filmOn=true;filmFree=scene->player();filmSnapshot();}
   else if(filmRevision!=revision||filmLevel!=scene->level())filmSnapshot();
   filmHeld=std::move(filmAsked);
  }else if(filmOn){filmRestore();filmOn=false;filmHeld.clear();if(scene&&!playing)scene->restoreEditorCamera(filmFree);}
  if(simWanted&&!simulating&&!playing&&valid&&scene){
   authored=std::make_unique<Game>(*scene);scene->setEditorObserver(true);
   simulating=true;simTime=0;fireSerial=simFire;pulses.clear();reportSim();lastSimReport=now;
  }
  if(simulating&&simFire!=fireSerial){fireSerial=simFire;scene->fireEditorTrigger(fireX1,fireY1,fireX2,fireY2);}
  if(scene){
   if(playing){bool menu=scene->paused()||scene->inventoryOpen()||scene->talking()||scene->consoleOpen();auto input=window.input(menu);if(input.escape){playing=false;input={};window.setMenu(true);if(haveEditCamera){scene->restoreEditorCamera(editCamera);haveEditCamera=false;}put(options.control,std::to_string(revision)+' '+std::to_string(selected)+" 0 0 "+std::to_string(audition)+'\n');if(options.parent)PostMessageW(GetAncestor(options.parent,GA_ROOT),WM_APP+31,0,0);}else scene->update(input,dt);}
   else{
    auto input=window.input(true);
    const bool inside=input.pointerX>=0&&input.pointerY>=0&&input.pointerX<DisplayWidth&&input.pointerY<DisplayHeight;
    auto camera=scene->player();
    bool moved=false;
    if(input.guard&&inside){
     if(orbitX>=0){camera.angle=std::remainder(camera.angle+(input.pointerX-orbitX)*0.0022f,6.28318530718f);camera.pitch=std::clamp(camera.pitch-(input.pointerY-orbitY)*0.308f,-210.f,210.f);moved=true;}
     orbitX=input.pointerX;orbitY=input.pointerY;
    }else orbitX=orbitY=-1;
    // Same idea as Creating Constructs: look around and walk the picture
    // without entering play, so a click still builds where you are looking.
    {
     const float speed=(input.sprint?10.f:4.5f)*dt;
     const float ca=std::cos(camera.angle),sa=std::sin(camera.angle);
     auto step=[&](float x,float y,float z){camera.pos.x+=x;camera.pos.y+=y;camera.z+=z;moved=true;};
     if(input.forward)step(ca*speed,sa*speed,0);
     if(input.back)step(-ca*speed,-sa*speed,0);
     if(input.left)step(sa*speed,-ca*speed,0);
     if(input.right)step(-sa*speed,ca*speed,0);
     if(input.jump)step(0,0,speed);
     if(input.crouch)step(0,0,-speed);
     // Official levels sit well below 0 (Medical is on a floor at -12). A fixed
     // -2 m limit lifted the camera through the ceiling, so the picture was the
     // empty shell above the rooms.
     const float ground=scene->world().floorHeight(camera.pos.x,camera.pos.y);
     camera.z=std::clamp(camera.z,ground-1.5f,ground+48.f);
    }
    if(moved&&!filmOn)scene->restoreEditorCamera(camera);
    // F6 in the picture starts or stops Simulate; Escape stops it.
    const bool simKey=window.focused()&&(GetAsyncKeyState(VK_F6)&0x8000)!=0;
    if(simKey&&!simKeyHeld&&valid){if(simulating){stopSim();askSim(false);}else askSim(true);}
    if(input.escape&&!simEscapeHeld&&simulating){stopSim();askSim(false);}
    simKeyHeld=simKey;simEscapeHeld=input.escape;
    // Between edits the clock alone runs: smoke, steam, sparks and water move; nothing else does.
    if(!simulating&&lookAnimate)scene->advanceEditorClock(dt);
    if(simulating){
     // The world runs with no hands on it; the camera stays wherever it is flown.
     auto eye=scene->player();scene->update({},dt);scene->restoreEditorCamera(eye);simTime+=dt;
     for(const auto& box:scene->takeEditorFires())pulses.push_back({box,simTime});
     if(now-lastSimReport>std::chrono::milliseconds(100)){reportSim();lastSimReport=now;}
    }
    if(now-lastHover>std::chrono::milliseconds(35)){
     lastHover=now;if(inside&&!input.fire){
      float ox,oy,oz,dx,dy,dz;cameraRay(input.pointerX,input.pointerY,ox,oy,oz,dx,dy,dz);
      std::ostringstream hover;hover<<"HOVER "<<ox<<' '<<oy<<' '<<oz<<' '<<dx<<' '<<dy<<' '<<dz;put(options.control.parent_path()/L"hover.txt",hover.str());
     }else put(options.control.parent_path()/L"hover.txt","CLEAR");
    }
    if(inside&&input.fire&&!pickHeld)++pickSerial;
    pickHeld=inside&&input.fire;
    if(pickSerial>0&&pickHeld){
     cameraRay(input.pointerX,input.pointerY,pickOx,pickOy,pickOz,pickDx,pickDy,pickDz);
     auto next=formatPick(true);if(next!=pickDetail){pickDetail=std::move(next);report(valid?"READY":"ERROR",valid?pickDetail:loadError);lastReport=now;}
    }else if(pickSerial>0&&!pickDetail.empty()&&pickDetail!=formatPick(false)){
     pickDetail=formatPick(false);report(valid?"READY":"ERROR",valid?pickDetail:loadError);lastReport=now;
    }
   }
   const bool hostFocused=options.parent&&GetForegroundWindow()==GetAncestor(options.parent,GA_ROOT);
   const bool audible=!options.hidden&&((playing&&window.focused())||((audition||simulating)&&hostFocused));
   if(audible&&!audio)audio=std::make_unique<AudioEngine>();
   if(audio)audio->update(*scene,audible);
   const auto renderStart=Clock::now();
   // --perf-trace: the engine's own cost scopes for the first picture after a change.
   const bool profileFrame=!options.perf.empty()&&!pendingPicture.empty();
   if(profileFrame){perf::enabled=true;perf::costs={};}
   const auto staticBuilds=renderer->staticGeometryBuilds();
   renderer->setEditorView(playing||simulating?std::vector<EditorFloorView>{}:editorFloors,playing||simulating||filmOn?std::vector<EditorGuide>{}:editorGuides);
   if(filmOn){auto cam=scene->player();cam.pos={filmX,filmY};cam.eye=Player::StandingEye;cam.z=filmEye-cam.eye;cam.crouched=cam.prone=false;
    cam.angle=filmYaw*kPi/180.f;cam.pitch=std::clamp(filmPitch*kPi/180.f*140.f,-210.f,210.f);scene->restoreEditorCamera(cam);applyFilmPuppets(*scene,filmHeld);Projection::setFieldOfView(filmFov);}
   renderer->render(*scene);
   if(filmOn)Projection::setFieldOfView(0);
   if(profileFrame){
    perf::enabled=false;std::vector<std::pair<double,int>> top;for(int c=0;c<perf::Count;++c)if(perf::costs[size_t(c)]>.5)top.push_back({perf::costs[size_t(c)],c});
    std::sort(top.rbegin(),top.rend());std::ostringstream line;line.setf(std::ios::fixed);line.precision(1);
    line<<"costs of rev "<<revision<<" picture (static rebuilds "<<renderer->staticGeometryBuilds()-staticBuilds<<"):";
    for(size_t i=0;i<std::min<size_t>(8,top.size());++i)line<<' '<<perf::names[top[i].second]<<' '<<top[i].first;
    perfLine(line.str());
   }if(!renderer->hardwarePresentsWindow())window.present(renderer->pixels(),width,height);
   ++frame;
   if(!options.perf.empty()){
    const auto done=Clock::now();
    if(!pendingPicture.empty()){std::ostringstream line;line.setf(std::ios::fixed);line.precision(2);line<<pendingPicture<<", first picture "<<ms(renderStart,done)<<" ms, edit seen to picture "<<ms(reloadStarted,done)<<" ms, picture at "<<std::chrono::duration<double,std::milli>(done.time_since_epoch()).count();perfLine(line.str());pendingPicture.clear();}
    const double work=ms(loopStart,done),render=ms(renderStart,done),control=ms(loopStart,controlRead),period=ms(previousFrame,done);previousFrame=done;
    ++stats.n;stats.work+=work;stats.workMax=std::max(stats.workMax,work);stats.render+=render;stats.renderMax=std::max(stats.renderMax,render);
    stats.control+=control;stats.controlMax=std::max(stats.controlMax,control);stats.period+=period;stats.periodMax=std::max(stats.periodMax,period);
    if(done-statsAt>std::chrono::seconds(2)){
     std::ostringstream line;line.setf(std::ios::fixed);line.precision(2);const double span=ms(statsAt,done);
     line<<"frames "<<stats.n<<" ("<<stats.n*1000.0/std::max(1.0,span)<<" fps"<<(simulating?", simulating":playing?", playing":"")<<"): work avg "<<stats.work/stats.n<<" max "<<stats.workMax<<" ms; render avg "<<stats.render/stats.n<<" max "<<stats.renderMax<<" ms; control files avg "<<stats.control/stats.n<<" max "<<stats.controlMax<<" ms; frame-to-frame avg "<<stats.period/stats.n<<" max "<<stats.periodMax<<" ms";
     perfLine(line.str());stats={};statsAt=done;
    }
   }
   if(now-lastReport>std::chrono::milliseconds(50)){report(valid?"READY":"ERROR",valid&&!pickDetail.empty()?pickDetail:loadError);lastReport=now;}
   if(now-lastLook>std::chrono::milliseconds(100)){reportLook();reportFilm();lastLook=now;}
   if(options.frames>0&&frame>=unsigned(options.frames))break;
  }
  Sleep(scene?1:16);
 }
 window.setMenu(true);
 if(scene)reportLook();
 if(thirdShown){renderer->setEnvironmentInspection(true);SoftwareRenderer::setThirdPerson(false);}
 if(!options.capture.empty()&&scene){std::ofstream out(options.capture,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
 report("STOPPED");return scene?0:8;
}
}
#endif

namespace retro {
int testLayerEditingExport(const std::filesystem::path& file){
 std::ofstream report(file.wstring()+L".layer-test.txt");
 try{
  auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
  auto loaded=loadCustomCampaignFile(file);require(loaded->maps.size()==1,"Layer regression export must contain one map");
  const auto& authored=*loaded->maps[0];World world(0,loaded->maps[0]);
  require(authored.layers.size()>=2,"The exported upper floor is missing");
  float deck=authored.layers[1].elevation;
  require(std::abs(world.supportBelow(12.5f,8.5f,deck+.05f)-deck)<.03f,"The saved catwalk has no native collision support");
  require(world.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The exported catwalk still lacks standing clearance");
  require(!world.fits(9.5f,8.5f,deck,Player::StandingHeight,false),"The exported upper wall has no collision");
  auto low=std::make_shared<AuthoredMapData>(authored);low->definition.ceiling=deck+1.f;World cramped(0,low);
  require(!cramped.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The regression did not reproduce the original low roof");
  require(std::any_of(world.structures().begin(),world.structures().end(),[](const auto& s){return s.layerSurface==1;}),"The game did not construct native upper-floor walls");
  report<<"PASS: editor export loads directly; catwalk support, standing clearance, low-roof reproduction and upper-wall collision\n";
  auto walker=std::make_unique<Game>(loaded);auto start=walker->player();start.angle=0;walker->restoreEditorCamera(start);
  require(std::abs(start.z-deck)<.03f,"The saved player start is not on the catwalk");
  InputState walk{};walk.forward=true;for(int frame=0;frame<60;++frame)walker->update(walk,1.f/60.f);
  require(walker->player().pos.x>start.pos.x+1.f&&std::abs(walker->player().z-deck)<.08f&&!walker->player().crouched,"The player could not walk upright along the exported catwalk");
  report<<"PASS: actual player simulation walks upright on the exported catwalk without falling or crouching\n";
  auto roofPack=loadCustomCampaignFile(file.parent_path()/L"roof-edit.rmc");World roofWorld(0,roofPack->maps[0]);
  require(roofWorld.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The editor-raised explicit roof still blocks the standing player");
  auto lowRoof=std::make_shared<AuthoredMapData>(*roofPack->maps[0]);require(!lowRoof->structures.empty(),"The exported explicit roof is missing");
  auto& slab=lowRoof->structures.back();require(std::abs(slab.top-slab.bottom-.25f)<.001f,"Export changed explicit roof thickness");slab.bottom=4.5f;slab.top=4.75f;World lowRoofWorld(0,lowRoof);
  require(!lowRoofWorld.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The explicit-roof check did not reproduce blocked headroom");
  report<<"PASS: separately exported thin-roof repair preserves slab thickness and restores real standing clearance\n";
  auto original=std::make_shared<CustomCampaign>(*loaded),taller=std::make_shared<CustomCampaign>(*loaded);
  std::ifstream input(file);std::string line,edited;while(std::getline(input,line)){if(line.starts_with("MAP|0|")){std::vector<std::string> fields;std::istringstream split(line);std::string field;while(std::getline(split,field,'|'))fields.push_back(field);fields[9]=std::to_string(authored.definition.ceiling+2.f);line.clear();for(const auto& value:fields){if(!line.empty())line+='|';line+=value;}}edited+=line+'\n';}
  taller=std::make_shared<CustomCampaign>(*loadCustomCampaignText(edited,"taller-export.rmc"));
  auto before=std::make_unique<Game>(original),after=std::make_unique<Game>(taller);
  before->update({},1.f/60);after->update({},1.f/60);
  auto camera=before->player();camera.pos={3.5f,8.5f};camera.z=deck;camera.angle=0;camera.pitch=0;camera.eye=Player::StandingEye;
  before->restoreEditorCamera(camera);after->restoreEditorCamera(camera);
  report<<"Native roof heights: "<<before->world().definition().ceiling<<" / "<<after->world().definition().ceiling<<"; camera z "<<camera.z<<" eye "<<camera.eye<<"\n";
  auto renderer=std::make_unique<SoftwareRenderer>(DisplayWidth,DisplayHeight);renderer->setEnvironmentInspection(true);
  require(renderer->enableHardware(nullptr),"Native UV verification requires the offscreen Vulkan renderer");
  int previousDetail=SoftwareRenderer::detailLayer();SoftwareRenderer::setDetailLayer(4);
  auto draw=[&](const Game& scene){for(int i=0;i<6;++i)renderer->render(scene);return std::vector<std::uint32_t>(renderer->pixels(),renderer->pixels()+DisplayWidth*DisplayHeight);};
  auto a=draw(*before),b=draw(*after);SoftwareRenderer::setDetailLayer(previousDetail);
  int total=0,matching=0,changed=0;
  auto difference=[](std::uint32_t p,std::uint32_t q){return std::abs(int((p>>16)&255)-int((q>>16)&255))+std::abs(int((p>>8)&255)-int((q>>8)&255))+std::abs(int(p&255)-int(q&255));};
  for(size_t i=0;i<a.size();++i)changed+=difference(a[i],b[i])>18;
  for(int y=DisplayHeight/2+10;y<DisplayHeight-35;++y)for(int x=DisplayWidth/2-24;x<DisplayWidth/2+24;++x){++total;matching+=difference(a[size_t(y)*DisplayWidth+x],b[size_t(y)*DisplayWidth+x])<=12;}
  report<<"UV comparison: "<<matching<<'/'<<total<<" lower-wall pixels match, "<<changed<<" geometry pixels differ\n";
  // UV debugging may give the roof and wall the same periodic values. Check
  // the normal material render independently to prove the height changed.
  SoftwareRenderer::setDetailLayer(0);auto materialA=draw(*before),materialB=draw(*after);SoftwareRenderer::setDetailLayer(previousDetail);
  int materialChanged=0;for(size_t i=0;i<materialA.size();++i)materialChanged+=difference(materialA[i],materialB[i])>18;
  report<<"Material geometry pixels changed: "<<materialChanged<<"\n";
  auto savePicture=[&](const auto& pixels,const wchar_t* suffix){std::ofstream out(file.wstring()+suffix,std::ios::binary);out<<"P6\n"<<DisplayWidth<<' '<<DisplayHeight<<"\n255\n";for(auto pixel:pixels){char rgb[]={char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}};savePicture(materialA,L".before.ppm");savePicture(materialB,L".after.ppm");
  require(materialChanged>100,"Height edit left the native rendered geometry unchanged");
  require(total>100&&matching>total*.94f,"Raising a wall stretched its lower material UV scale");
  report<<"PASS: native rendered UV density stays stable under wall-height edits ("<<matching<<'/'<<total<<" lower-wall pixels match; "<<changed<<" geometry pixels change)\n";
  return 0;
 }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<'\n';return 1;}
}
}
namespace retro {
int testCampaignEditingExport(const std::filesystem::path& file){
 std::ofstream report(file.wstring()+L".campaign-test.txt");
 try{auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(message);};auto loaded=loadCustomCampaignFile(file);
  require(loaded->maps.size()==6,"Campaign regression needs the four joined edges and one distant room");
  for(int edge=0;edge<4;++edge){auto game=std::make_unique<Game>(loaded);auto at=game->player();
   at.pos={edge==2?.8f:edge==3?23.2f:12.5f,edge==0?.8f:edge==1?23.2f:12.5f};at.angle=edge==0?-1.5707963f:edge==1?1.5707963f:edge==2?3.1415926f:0;at.z=0;game->restoreEditorCamera(at);
   InputState walk{};walk.forward=true;for(int i=0;i<60;++i)game->update(walk,1.f/60.f);
   require(game->level()==edge+1,"The player could not cross a joined map edge in the game");}
  report<<"PASS: actual player simulation crosses all four saved campaign joins\n";
  auto game=std::make_unique<Game>(loaded);auto renderer=std::make_unique<SoftwareRenderer>(DisplayWidth,DisplayHeight);renderer->setEnvironmentInspection(true);require(renderer->enableHardware(nullptr),"Offscreen native renderer unavailable");
  for(int i=0;i<3;++i)renderer->render(*game);auto chunks=renderer->renderedChunks();
  require(std::find(chunks.begin(),chunks.end(),0)!=chunks.end(),"Active map was culled");
  for(int edge=1;edge<=4;++edge)require(std::find(chunks.begin(),chunks.end(),edge)!=chunks.end(),"A nearby joined map was culled");
  require(std::find(chunks.begin(),chunks.end(),5)==chunks.end(),"A distant map was still submitted to the renderer");
  report<<"PASS: native rendering draws active and adjoining maps while excluding the distant map\n";
  auto garage=loadCustomCampaignFile(file.parent_path()/L"parking-garage.rmc");World garageWorld(0,garage->maps[0]);
  require(garageWorld.supportBelow(6.5f,12.f,3.51f)<3.f,"The painted garage ramp opening still contains a hidden deck");
  bool concrete=false,yellow=false;for(const auto& p:garage->maps[0]->structures){concrete|=p.material==15;yellow|=p.material==18;}require(concrete&&yellow,"Concrete or the local yellow stripe disappeared from the exported garage");
  for(float startZ:{0.f,3.5f}){auto driver=std::make_unique<Game>(garage);auto at=driver->player();at.pos={startZ<1?6.5f:10.5f,4.4f};at.z=startZ;at.angle=1.5707963f;driver->restoreEditorCamera(at);InputState walk{};walk.forward=true;
   for(int i=0;i<420;++i)driver->update(walk,1.f/60.f);report<<"Garage ramp from "<<startZ<<": player "<<driver->player().pos.x<<','<<driver->player().pos.y<<','<<driver->player().z<<'\n';require(driver->player().pos.y>19.3f&&driver->player().z>startZ+3.3f,"The player cannot climb the garage ramp and cross its deck landing");}
  report<<"PASS: concrete and yellow stripe remain native; erased ramp gaps stay open; player walks both garage ramps onto their landings\n";return 0;
 }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<'\n';return 1;}
}
}

#ifdef _WIN32
namespace retro {
int testCorridorEditingExport(const std::filesystem::path& list){
 std::ofstream report(list.wstring()+L".corridor-test.txt");
 if(!report){std::cerr<<"Cannot create corridor export report\n";return 1;}
 auto require=[](bool value,const std::string& message){if(!value)throw std::runtime_error(message);};
 int fixtures=0,failures=0;
 try{
  std::ifstream paths(list);require(bool(paths),"Cannot open corridor fixture list");
  std::string entry;
  while(std::getline(paths,entry)){
   if(!entry.empty()&&entry.back()=='\r')entry.pop_back();
   if(fixtures==0&&entry.starts_with("\xef\xbb\xbf"))entry.erase(0,3);
   if(entry.empty())continue;
   ++fixtures;
   try{
    const auto file=std::filesystem::u8path(entry);require(file.is_absolute(),"Fixture path must be absolute");
    auto loaded=loadCustomCampaignFile(file);require(!loaded->maps.empty(),"Fixture has no map zero");
    World world(0,loaded->maps[0]);
    auto integer=[&](const std::string& value){size_t used=0;int result=std::stoi(value,&used);require(used==value.size(),"Invalid integer in corridor test comment");return result;};
    auto decimal=[&](const std::string& value){size_t used=0;float result=std::stof(value,&used);require(used==value.size()&&std::isfinite(result),"Invalid height in corridor test comment");return result;};
    int routeCount=0,routeLayer=-1;float expectedZ=0;std::vector<int> anchors;std::vector<Point3> belowWalls;
    std::ifstream input(file);require(bool(input),"Cannot read corridor fixture");
    std::string line;
    while(std::getline(input,line)){
     if(!line.empty()&&line.back()=='\r')line.pop_back();
     if(!line.starts_with("# EDITOR_CORRIDOR_ROUTE|")&&!line.starts_with("# EDITOR_CORRIDOR_WALL_BELOW|"))continue;
     std::vector<std::string> fields;std::istringstream split(line);std::string field;while(std::getline(split,field,'|'))fields.push_back(field);
     if(fields[0]=="# EDITOR_CORRIDOR_ROUTE"){
      require(fields.size()==5,"Malformed corridor route test comment");require(++routeCount==1,"Fixture has multiple route test comments");
      require(integer(fields[1])==0,"Corridor regression must test map zero");routeLayer=integer(fields[2]);expectedZ=decimal(fields[3]);
      std::istringstream cells(fields[4]);while(std::getline(cells,field,',')){int anchor=integer(field);int x=anchor%24,y=anchor/24;
       require(anchor>=0&&anchor<576&&x>=1&&x<=21&&y>=1&&y<=21,"Route anchor clips the passage or wall buffer");
       if(!anchors.empty()){int previous=anchors.back();require(std::abs(x-previous%24)+std::abs(y-previous/24)==1,"Route anchors are not cardinal neighbors");}
       anchors.push_back(anchor);
      }
     }else{
      require(fields.size()==4,"Malformed wall-below test comment");belowWalls.push_back({decimal(fields[1]),decimal(fields[2]),decimal(fields[3])});
     }
    }
    require(routeCount==1&&anchors.size()>=2,"Fixture needs a corridor route with at least two anchors");
    require(routeLayer>=0&&routeLayer<int(loaded->maps[0]->layers.size()),"Route references an absent native layer");
    require(std::abs(loaded->maps[0]->layers[size_t(routeLayer)].elevation-expectedZ)<.08f,"Route height differs from the native layer elevation");
    auto center=[](int anchor){return Vec2{float(anchor%24)+1.f,float(anchor/24)+1.f};};
    for(size_t i=0;i<anchors.size();++i){const auto p=center(anchors[i]);float support=world.supportBelow(p.x,p.y,expectedZ+.05f);
     require(std::isfinite(support)&&std::abs(support-expectedZ)<.08f,"Anchor "+std::to_string(i)+" has no native floor support");
     require(world.fits(p.x,p.y,expectedZ,Player::StandingHeight,false),"Anchor "+std::to_string(i)+" lacks standing clearance");
    }
    for(const auto& p:belowWalls)require(!world.fits(p.x,p.y,p.z,Player::StandingHeight,false),"Corridor opening removed a wall below the active floor");
    auto game=std::make_unique<Game>(loaded);require(game->level()==0,"Fixture starts outside map zero");
    auto camera=game->player();camera.pos=center(anchors.front());camera.z=expectedZ;camera.angle=0;camera.pitch=0;camera.crouched=false;camera.prone=false;camera.eye=Player::StandingEye;
    // This is the only position assignment. Later camera restores change yaw
    // at the current simulated position and never place the player at a waypoint.
    game->restoreEditorCamera(camera);
    float traveled=0;int frames=0;constexpr float tolerance=.13f,step=1.f/60.f;
    auto standing=[&](){
     const auto& player=game->player();
     require(game->level()==0,"Player left the fixture map");
     require(std::isfinite(player.pos.x)&&std::isfinite(player.pos.y)&&std::isfinite(player.z),"Player simulation produced a nonfinite position");
     require(!game->dead()&&!player.crouched&&!player.prone,"Player could not remain standing");
     require(std::abs(player.z-expectedZ)<.08f&&player.grounded,"Player fell or left the corridor floor");
     require(game->world().fits(player.pos.x,player.pos.y,player.z,Player::StandingHeight,false),"Simulated player lost standing clearance");
    };
    for(size_t i=1;i<anchors.size();++i){
     const auto target=center(anchors[i]);camera=game->player();auto delta=target-camera.pos;camera.angle=std::atan2(delta.y,delta.x);game->restoreEditorCamera(camera);
     int segmentFrames=0;
     while(length(target-game->player().pos)>tolerance&&segmentFrames<180){
      const auto before=game->player().pos;delta=target-before;
      InputState walk{};walk.forward=true;
      // Ordinary mouse input corrects retained sideways momentum at elbows.
      walk.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game->player().angle)/(.0022f*std::max(.01f,game->settings().sensitivity));
      game->update(walk,step);++segmentFrames;++frames;traveled+=length(game->player().pos-before);standing();
     }
     const auto& player=game->player();
     require(length(target-player.pos)<=tolerance,"Blocked before waypoint "+std::to_string(i)+" at "+std::to_string(player.pos.x)+","+std::to_string(player.pos.y)+","+std::to_string(player.z));
    }
    standing();require(std::abs(game->player().z-expectedZ)<.08f,"Final player height differs from the corridor floor");
    report<<"PASS: "<<entry<<" | waypoints="<<anchors.size()<<" | distance="<<traveled<<" | frames="<<frames<<" | final-z="<<game->player().z<<" | below-walls="<<belowWalls.size()<<'\n';
   }catch(const std::exception& error){++failures;report<<"FAIL: "<<entry<<" | "<<error.what()<<'\n';}
  }
  require(fixtures>0,"Corridor fixture list is empty");
  report<<"Fixtures: "<<fixtures<<"; passed: "<<fixtures-failures<<"; failed: "<<failures<<'\n';
  return failures?1:0;
 }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<'\n';return 1;}
}
}
#endif
