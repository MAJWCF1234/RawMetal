// RawMetalLab: the feature lab's runner (lab/README.md).
//
//   RawMetalLab --feature=NAME[,NAME|A+B+C] [--fixtures=GLOB] [--frames=N] [--step=S]
//               [--out=DIR] [--software] [--jobs=K] [--with-friends] [--reporter[=NAME,..|none]]
//               [--no-repeat] [--live [--live-seconds=S]] [--list]
//
// Each (feature group, fixture) pair is a tube run: the fixture's scene is built,
// every feature in the group plugged in (author -> setup -> step/capture ->
// metrics), N frames simulated and rendered, then the same tube run again from
// scratch to prove the frames repeat. --with-friends runs a feature alone, with
// each friend, with all of them, each friend alone and the bare fixture, and
// reports the feature x friend matrix (checks, determinism, cost against the sum
// of the parts, new flicker where they overlap). Output: DIR/report.html,
// DIR/metrics.json, DIR/<feature>-sheet.png, DIR/jobs/<tube>/*.png; exit 1 on a failure.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabAssets.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include "core/Tune.h"
#include "game/EngineRegressionScene.h"
#include "world/CustomCampaign.h"
#include "platform/Win32Window.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static double since(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
namespace retro::lab {
// ---------------------------------------------------------------- registry
static std::vector<Feature>& registry(){static std::vector<Feature> all;return all;}
bool add(Feature feature){registry().push_back(std::move(feature));return true;}
const std::vector<Feature>& features(){return registry();}
const Feature* find(const std::string& name){for(const auto& f:registry())if(f.name==name)return &f;return nullptr;}
float tuned(const char* name,float fallback){return tune(name,fallback);}
Projected project(const Scene& scene,Point3 world){
 Projected out;if(!scene.renderer)return out;const auto c=LabAccess::cameraPoint(*scene.renderer,world);
 const int W=scene.renderer->width(),H=scene.renderer->height();const float f=Projection{float(W)/H}.focal(float(W));
 if(c.z<Projection::nearPlane)return out;out.x=W*.5f+c.x*f/c.z;out.y=H*.5f-c.y*f/c.z;out.depth=c.z;out.visible=true;return out;
}
// ---------------------------------------------------------------- options
struct Options {
 std::vector<std::vector<std::string>> groups;std::string fixtures,out;int frames=0,jobs=1;float step=0;
 bool software=false,withFriends=false,repeat=true,live=false,child=false,list=false;
 float liveSeconds=0;std::string reporter="all"; // all | none | a,b
 std::vector<std::string> passThrough; // for children
 bool reporterOn(const std::string& feature)const{if(reporter=="none")return false;if(reporter=="all")return true;return (","+reporter+",").find(","+feature+",")!=std::string::npos;}
};
static std::vector<std::string> split(const std::string& s,char c){std::vector<std::string> out;std::stringstream in(s);std::string item;while(std::getline(in,item,c))if(!item.empty())out.push_back(item);return out;}
static std::string join(const std::vector<std::string>& v,const char* sep){std::string out;for(size_t i=0;i<v.size();++i)out+=(i?sep:"")+v[i];return out;}
// ---------------------------------------------------------------- results
struct JobSpec {std::vector<std::string> group;std::string fixture;std::string id()const{return (group.empty()?std::string("baseline"):join(group,"+"))+"@"+(fixture=="none"?std::string("none"):fs::path(fixture).stem().string());}};
struct JobResult {
 JobSpec spec;std::string status="error",error;double wallMs=0,renderMs=0,updateMs=0,frameMs=0,flicker=0;int frames=0;
 std::string hash,repeatHash;bool repeated=false;std::vector<Metric> metrics;std::vector<Check> checks;
 std::vector<std::pair<std::string,std::string>> images; // label, path relative to out
 bool ok()const{if(status=="skipped")return true;if(status!="ok")return false;for(const auto&c:checks)if(!c.ok)return false;return true;}
 bool featureOk(const std::string& f)const{if(status=="skipped")return true;if(status!="ok")return false;for(const auto&c:checks)if(!c.ok&&(c.owner==f||c.owner.rfind(f+"+",0)==0||c.owner=="lab"))return false;return true;}
};
static std::string clean(std::string s){for(auto& c:s)if(c=='\t'||c=='\n'||c=='\r')c=' ';return s;}
static void writeResult(const fs::path& file,const JobResult& r){
 std::ofstream o(file);o.precision(9);
 o<<"id\t"<<r.spec.id()<<"\ngroup\t"<<join(r.spec.group,"+")<<"\nfixture\t"<<r.spec.fixture<<"\nstatus\t"<<r.status<<"\nerror\t"<<clean(r.error)<<"\nwall\t"<<r.wallMs<<"\nrender\t"<<r.renderMs<<"\nupdate\t"<<r.updateMs
  <<"\nframe\t"<<r.frameMs<<"\nflicker\t"<<r.flicker<<"\nframes\t"<<r.frames<<"\nhash\t"<<r.hash<<"\nrepeat\t"<<r.repeatHash<<"\nrepeated\t"<<r.repeated<<"\n";
 for(const auto& m:r.metrics)o<<"metric\t"<<clean(m.name)<<"\t"<<m.value<<"\t"<<clean(m.unit)<<"\n";
 for(const auto& c:r.checks)o<<"check\t"<<c.ok<<"\t"<<clean(c.owner)<<"\t"<<clean(c.name)<<"\t"<<clean(c.detail)<<"\n";
 for(const auto& i:r.images)o<<"image\t"<<clean(i.first)<<"\t"<<i.second<<"\n";
}
static bool readResult(const fs::path& file,JobResult& r){
 std::ifstream in(file);if(!in)return false;std::string line;
 while(std::getline(in,line)){auto f=split(line,'\t');if(line.find('\t')==std::string::npos)continue;
  std::vector<std::string> w;{std::stringstream s(line);std::string x;while(std::getline(s,x,'\t'))w.push_back(x);}
  const auto& k=w[0];auto v=[&](size_t i){return i<w.size()?w[i]:std::string();};
  if(k=="status")r.status=v(1);else if(k=="error")r.error=v(1);else if(k=="wall")r.wallMs=std::stod(v(1));else if(k=="render")r.renderMs=std::stod(v(1));
  else if(k=="update")r.updateMs=std::stod(v(1));else if(k=="frame")r.frameMs=std::stod(v(1));else if(k=="flicker")r.flicker=std::stod(v(1));else if(k=="frames")r.frames=std::stoi(v(1));
  else if(k=="hash")r.hash=v(1);else if(k=="repeat")r.repeatHash=v(1);else if(k=="repeated")r.repeated=v(1)=="1";
  else if(k=="metric")r.metrics.push_back({v(1),std::stod(v(2)),v(3)});
  else if(k=="check")r.checks.push_back({v(3),v(1)=="1",v(4),v(2)});
  else if(k=="image")r.images.push_back({v(1),v(2)});}
 return true;
}
// ---------------------------------------------------------------- scene
static CreatureKind creatureKind(const std::string& name){
 static const char* names[]={"huntsman","wasp","brute","warden","mutant","snatcher","chimera","transformed","machine"};
 std::string n=name;for(auto& c:n)c=char(std::tolower(static_cast<unsigned char>(c)));
 for(int i=0;i<CreatureKindCount;++i)if(n==names[i])return CreatureKind(i);throw std::runtime_error("lab: no creature kind "+name);
}
static std::string readAll(const std::string& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("lab: cannot read "+path);return {std::istreambuf_iterator<char>(in),{}};}
static void buildScene(Scene& s,const Fixture& fx,const std::vector<const Feature*>& group){
 s.fixture=&fx;s.features.clear();for(auto* f:group)s.features.push_back(f->name);s.frame=0;s.time=0;s.board.clear();s.points.clear();s.map.reset();
 const bool authored=fx.world=="room"||fx.world=="door-room"||fx.world=="text";
 if(authored){
  std::shared_ptr<CustomCampaign> campaign;
  if(fx.world=="text"){auto parsed=loadCustomCampaignText(fx.campaignText,fx.name+".lab");campaign=std::make_shared<CustomCampaign>(*parsed);
   s.map=std::make_shared<AuthoredMapData>(*campaign->maps.at(size_t(fx.map)));campaign->maps[size_t(fx.map)]=s.map;}
  else{s.map=fx.world=="room"?regression::room():regression::doorRoom();campaign=regression::campaign(s.map);}
  for(const auto& sp:fx.spawns){
   if(sp.what=="creature"){CreatureSpawn c{creatureKind(sp.kind),sp.at,sp.z};s.map->creatureSpawns.push_back(c);}
   else{auto man=regression::guard(sp.at,sp.at,AiFaction::Raider);for(auto& k:man.keys)k.yaw=sp.yaw;s.map->actorTracks.push_back(man);}}
  for(const auto& l:fx.lamps){WorldLight light{};light.position={l.at.x,l.at.y};light.z=l.at.z;light.intensity=l.intensity;light.range=l.range;s.map->lights.push_back(light);}
  for(auto* f:group)if(f->author)f->author(s,*s.map);
  s.game=std::make_unique<Game>(campaign);
  if(s.game->level()!=fx.map)LabAccess::loadLevel(*s.game,fx.map);
 }else if(fx.world=="campaign"||fx.world=="ashfall"){
  const WorldId id=fx.world=="ashfall"?WorldId::Ashfall:WorldId::Campaign;
  s.game=std::make_unique<Game>(Game::mapInspection(fx.hasCamera?fx.cameraPos:Vec2{12,12},fx.cameraYaw,fx.cameraPitch,fx.map,fx.openDoors,fx.cameraZ,fx.sceneryOnly,id));
 }else if(fx.world=="custom"){
  s.game=std::make_unique<Game>(loadCustomCampaignFile(fs::path(fx.customPath).is_absolute()?fs::path(fx.customPath):fs::path(labRoot())/fx.customPath));
  if(s.game->level()!=fx.map)LabAccess::loadLevel(*s.game,fx.map);
 }else throw std::runtime_error("lab: unknown world "+fx.world);
 Game& g=*s.game;
 if(!fx.save.empty()&&!LabAccess::decodeSave(g,readAll(fx.save)))throw std::runtime_error("lab: the save "+fx.save+" would not load");
 if(fx.hasCamera)LabAccess::setCamera(g,fx.cameraPos,fx.cameraZ,fx.cameraYaw,fx.cameraPitch);
 for(const auto& st:fx.states)g.setState(st.id,st.value);
 if(!authored)for(const auto& sp:fx.spawns)if(sp.what=="creature"){CreatureSpawn c{creatureKind(sp.kind),sp.at,sp.z};LabAccess::spawnCreature(g,c);LabAccess::enemies(g).back().heading=sp.yaw;}
 LabAccess::streaming(g);
 for(auto* f:group)if(f->setup)f->setup(s);
}
// ---------------------------------------------------------------- renderers
struct Renderers {
 std::map<std::tuple<int,int,bool>,std::unique_ptr<SoftwareRenderer>> made;double loadMs=0;std::string gpuName;
 SoftwareRenderer* get(int w,int h,bool gpu){auto key=std::make_tuple(w,h,gpu);auto it=made.find(key);if(it!=made.end())return it->second.get();
  const auto t=Clock::now();auto r=std::make_unique<SoftwareRenderer>(w,h);if(gpu&&!r->enableHardware())gpu=false;gpuName=gpu?r->hardwareName():"software";loadMs+=since(t);
  auto* raw=r.get();made[key]=std::move(r);return raw;}
};
// ---------------------------------------------------------------- one tube
struct Pass {std::vector<Image> keep;std::vector<int> keepAt;std::vector<std::string> keepLabel;Image last;double renderMed=0,updateMed=0,frameMed=0;std::uint64_t hash=0;std::vector<Metric> metrics;std::vector<Check> checks;Overlay overlay;bool overlayAny=false;double flicker=0;};
static double median(std::vector<double> v){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[v.size()/2];}
static Pass runPass(const Fixture& fx,const std::vector<const Feature*>& group,SoftwareRenderer& r,int frames,float dt,bool reporter,bool keepFrames,const Options& opt){
 Pass p;Scene s;s.renderer=&r;s.frames=frames;LabAccess::resetRenderer(r);
 struct Teardown {Scene& s;const std::vector<const Feature*>& group;~Teardown(){for(auto* f:group)if(f->teardown)try{f->teardown(s);}catch(...){}}} teardown{s,group};
 r.setEnvironmentInspection(fx.value("hud","off")!="on"); // no hands or HUD unless the fixture asks (hud on)
 s.dt=dt;buildScene(s,fx,group);s.frames=frames;s.dt=dt;Game& g=*s.game;
 std::vector<double> renders,updates,totals;std::set<int> keepAt={0,frames/4,frames/2,(3*frames)/4,frames-1};
 for(int i=0;i<frames;++i){s.frame=i;const auto t0=Clock::now();
  s.input={};for(auto* f:group)if(f->step)f->step(s,dt);
  if(i>0)g.update(s.input,dt);s.time+=i>0?dt:0;const double u=since(t0);
  const auto t1=Clock::now();r.render(g);const double ms=since(t1);
  Frame frame{r.pixels(),r.width(),r.height(),i,ms,u};for(auto* f:group)if(f->capture)f->capture(s,frame);
  renders.push_back(ms);updates.push_back(u);totals.push_back(ms+u);
  if(keepFrames&&keepAt.count(i)){p.keep.push_back(Image::of(r.pixels(),r.width(),r.height()));p.keepAt.push_back(i);p.keepLabel.push_back("frame "+std::to_string(i));}}
 p.last=Image::of(r.pixels(),r.width(),r.height());p.hash=hashPixels(r.pixels(),size_t(r.width())*r.height());
 // Flicker: the same moment from 1 mm to the side. A clean scene moves only at edges; surfaces fighting for one depth swap wholesale.
 {Player& cam=LabAccess::player(g);const Vec2 was=cam.pos;cam.pos+=Vec2{-std::sin(cam.angle),std::cos(cam.angle)}*.001f;r.render(g);p.flicker=difference(p.last,Image::of(r.pixels(),r.width(),r.height()),40).changed;cam.pos=was;r.render(g);}
 p.renderMed=median(renders);p.updateMed=median(updates);p.frameMed=median(totals);
 for(auto* f:group){Report rep;rep.owner=f->name;if(f->metrics)f->metrics(s,rep);
  for(const auto& [mate,fn]:f->with)if(s.has(mate)){Report pair;pair.owner=f->name+"+"+mate;fn(s,pair);rep.metrics.insert(rep.metrics.end(),pair.metrics.begin(),pair.metrics.end());rep.checks.insert(rep.checks.end(),pair.checks.begin(),pair.checks.end());}
  p.metrics.insert(p.metrics.end(),rep.metrics.begin(),rep.metrics.end());p.checks.insert(p.checks.end(),rep.checks.begin(),rep.checks.end());
  if(reporter&&f->reporter&&opt.reporterOn(f->name)){f->reporter(s,p.overlay);p.overlayAny=true;}}
 if(keepFrames)for(const auto& shot:s.shots){Image i(shot.width,shot.height);i.pixels=shot.pixels;p.keep.push_back(std::move(i));p.keepAt.push_back(-2);p.keepLabel.push_back(shot.label);}
 if(reporter&&p.overlayAny){Image rep=p.last;drawOverlay(rep,p.overlay,[&](Point3 w){return project(s,w);});drawText(rep,4,r.height()-12,"REPORTER: "+join(s.features,"+"),0xffff60);p.keep.push_back(rep);p.keepAt.push_back(-1);p.keepLabel.push_back("reporter");}
 return p;
}
static JobResult runJob(const JobSpec& spec,const Options& opt,Renderers& renderers){
 JobResult res;res.spec=spec;const auto started=Clock::now();
 const fs::path dir=fs::path(opt.out)/"jobs"/spec.id();std::error_code ec;fs::create_directories(dir,ec);
 try{
  if(spec.fixture=="none"){ // engine tests: run in the job folder, so their report files land there
   for(const auto& n:spec.group){auto* f=find(n);if(!f||!f->standalone)throw std::runtime_error("lab: "+n+" is not an engine test");
    wchar_t was[MAX_PATH];GetCurrentDirectoryW(MAX_PATH,was);SetCurrentDirectoryW(dir.wstring().c_str());bool ok=false;std::string why;
    const auto t=Clock::now();try{ok=f->standalone();}catch(const std::exception& e){why=e.what();}SetCurrentDirectoryW(was);
    res.metrics.push_back({n+".ms",since(t),"ms"});res.checks.push_back({"the engine test passes",ok,why.empty()?"report files in "+dir.generic_string():why,n});}
   res.status="ok";res.wallMs=since(started);writeResult(dir/"result.txt",res);return res;}
  const Fixture fx=loadFixture(spec.fixture);std::vector<const Feature*> group;
  for(const auto& n:spec.group){if(n=="baseline")continue;auto* f=find(n);if(!f)throw std::runtime_error("lab: no feature "+n+" (--list)");group.push_back(f);}
  const bool gpu=!opt.software&&!fx.software;
  bool needsGpu=false;for(auto* f:group)needsGpu|=f->needsGpu;
  SoftwareRenderer& r=*renderers.get(fx.width,fx.height,gpu);
  if(needsGpu&&!r.hardwareActive()){res.status="skipped";res.error="needs the GPU (ran --software)";writeResult(dir/"result.txt",res);return res;}
  const int frames=std::max(1,opt.frames?opt.frames:fx.frames?fx.frames:60);const float dt=opt.step>0?opt.step:fx.step>0?fx.step:1.f/60;
  Pass first=runPass(fx,group,r,frames,dt,true,true,opt);
  res.frames=frames;res.renderMs=first.renderMed;res.updateMs=first.updateMed;res.frameMs=first.frameMed;res.flicker=first.flicker;
  char hex[32];std::snprintf(hex,sizeof hex,"%016llx",(unsigned long long)first.hash);res.hash=hex;
  res.metrics=first.metrics;res.checks=first.checks;
  res.metrics.push_back({"lab.render_ms",first.renderMed,"ms"});res.metrics.push_back({"lab.update_ms",first.updateMed,"ms"});res.metrics.push_back({"lab.flicker",first.flicker,"share"});
  if(opt.repeat){Pass again=runPass(fx,group,r,frames,dt,false,false,opt);std::snprintf(hex,sizeof hex,"%016llx",(unsigned long long)again.hash);res.repeatHash=hex;res.repeated=true;
   bool same=again.hash==first.hash&&again.metrics.size()==first.metrics.size();std::string moved;
   // Metrics must repeat to 1% (timings excepted): renderer-side animation blends carried from the pass
   // before (Claire's outside pose) nudge a mid-tube reading by a fraction of that. The frame must match exactly.
   for(size_t i=0;same&&i<first.metrics.size();++i){const double a=first.metrics[i].value,b=again.metrics[i].value;
    if(first.metrics[i].unit!="ms"&&std::fabs(a-b)>1e-2*std::max({1e-3,std::fabs(a),std::fabs(b)})){same=false;moved=first.metrics[i].name+" "+std::to_string(a)+" then "+std::to_string(b);}}
   if(again.hash!=first.hash){const auto d=difference(first.last,again.last,8);moved="the last frame's pixels ("+std::to_string(int(d.changed*1000)/10.0).substr(0,4)+"% moved; see repeat-diff)";
    // Where the two runs part: the repeat's last frame, and the pixels that moved in red over a grey copy.
    Image heat=first.last;for(size_t k=0;k<heat.pixels.size();++k){const auto a=first.last.pixels[k],b=again.last.pixels[k];int m=0;for(int c=0;c<24;c+=8)m=std::max(m,std::abs(int((a>>c)&255)-int((b>>c)&255)));
     const int gray=int(((a>>16)&255)+((a>>8)&255)+(a&255))/9;heat.pixels[k]=m>8?(std::uint32_t(std::min(255,64+m*4))<<16):std::uint32_t(gray*0x010101);}
    first.keep.push_back(again.last);first.keepAt.push_back(-2);first.keepLabel.push_back("repeat last frame");first.keep.push_back(heat);first.keepAt.push_back(-2);first.keepLabel.push_back("repeat diff");}
   res.checks.push_back({"the tube repeats: same last frame and metrics when run again from scratch",same,same?res.hash:"differs: "+moved,"lab"});}
  for(size_t i=0;i<first.keep.size();++i){std::string name=first.keepLabel[i]+".png";for(auto& c:name)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='.'&&c!='-')c='-';savePng((dir/name).string(),first.keep[i]);
   res.images.push_back({first.keepLabel[i],(fs::path("jobs")/spec.id()/name).generic_string()});}
  res.status="ok";
 }catch(const std::exception& e){res.status="error";res.error=e.what();}
 res.wallMs=since(started);writeResult(dir/"result.txt",res);return res;
}
// ---------------------------------------------------------------- planning
struct Plan {std::vector<JobSpec> jobs;struct Matrix {std::string feature,fixture;std::vector<std::string> columns;};std::vector<Matrix> matrices;};
static Plan plan(const Options& opt){
 Plan p;std::set<std::string> seen;auto addJob=[&](std::vector<std::string> group,const std::string& fixture){JobSpec j{group,fixture};if(seen.insert(j.id()).second)p.jobs.push_back(j);};
 auto fixturesFor=[&](const std::vector<std::string>& group){
  bool standalone=!group.empty();for(const auto& n:group){auto* f=find(n);standalone&=f&&f->standalone!=nullptr;}
  if(standalone)return std::vector<std::string>{"none"};
  if(!opt.fixtures.empty())return findFixtures(opt.fixtures);
  std::vector<std::string> names;for(const auto& n:group){if(n=="baseline")continue;auto* f=find(n);if(!f)throw std::runtime_error("lab: no feature "+n+" (--list)");for(const auto& x:f->fixtures)names.push_back(x);if(!names.empty())break;}
  if(names.empty())throw std::runtime_error("lab: "+join(group,"+")+" names no fixture; pass --fixtures");
  return findFixtures(join(names,","));};
 for(const auto& group:opt.groups){
  const auto fixtures=fixturesFor(group);
  for(const auto& fixture:fixtures){
   addJob(group,fixture);
   // The friend matrix runs in the feature's own first tube (or every --fixtures one): a friend dropped
   // into a scene made for something else mostly measures the scene.
   if(!opt.withFriends||(opt.fixtures.empty()&&fixture!=fixtures.front()))continue;
   for(const auto& name:group){auto* f=find(name);if(!f||f->friends.empty())continue;Plan::Matrix m{name,fixture,{"alone"}};
    addJob({name},fixture);addJob({},fixture);
    for(const auto& mate:f->friends){if(!find(mate))continue;addJob({name,mate},fixture);addJob({mate},fixture);m.columns.push_back(mate);}
    std::vector<std::string> all={name};for(const auto& mate:f->friends)if(find(mate))all.push_back(mate);
    if(all.size()>2){addJob(all,fixture);m.columns.push_back("all");}
    p.matrices.push_back(m);}}}
 return p;
}
// ---------------------------------------------------------------- report
static std::string html(const std::string& s){std::string o;for(char c:s){if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='&')o+="&amp;";else if(c=='"')o+="&quot;";else o+=c;}return o;}
static std::string json(const std::string& s){std::string o="\"";for(char c:s){if(c=='"'||c=='\\'){o+='\\';o+=c;}else if(unsigned(c)<32)o+=' ';else o+=c;}return o+"\"";}
static std::string num(double v,int digits=3){char b[64];std::snprintf(b,sizeof b,"%.*f",digits,v);return b;}
struct Cell {bool ok=true;std::vector<std::string> why;};
static int report(const Options& opt,const Plan& plan,std::vector<JobResult>& results,double wallMs,double loadMs,const std::string& gpu){
 const fs::path out=opt.out;std::map<std::string,const JobResult*> byId;for(const auto& r:results)byId[r.spec.id()]=&r;
 auto job=[&](std::vector<std::string> group,const std::string& fixture)->const JobResult*{auto it=byId.find(JobSpec{group,fixture}.id());return it==byId.end()?nullptr:it->second;};
 // The matrix: each feature with each friend.
 struct MatrixOut {const Plan::Matrix* m;std::vector<Cell> cells;};std::vector<MatrixOut> matrices;bool matrixFailed=false;
 for(const auto& m:plan.matrices){MatrixOut mo{&m,{}};const JobResult* base=job({},m.fixture);const JobResult* alone=job({m.feature},m.fixture);
  for(const auto& col:m.columns){Cell cell;std::vector<std::string> group={m.feature};if(col=="all"){auto* f=find(m.feature);for(const auto& x:f->friends)if(find(x))group.push_back(x);}else if(col!="alone")group.push_back(col);
   const JobResult* r=job(group,m.fixture);if(!r){cell.ok=false;cell.why.push_back("not run");mo.cells.push_back(cell);continue;}
   if(r->status!="ok"){cell.ok=r->status=="skipped";cell.why.push_back(r->status+": "+r->error);mo.cells.push_back(cell);continue;}
   for(const auto& c:r->checks)if(!c.ok&&(c.owner==m.feature||c.owner.rfind(m.feature+"+",0)==0||c.owner=="lab"||(col!="alone"&&c.owner.find('+')!=std::string::npos))){cell.ok=false;cell.why.push_back(c.owner+": "+c.name+" ("+c.detail+")");}
   if(group.size()>1&&base&&base->status=="ok"){
    double parts=0;bool all=true;for(const auto& n:group){auto* a=job({n},m.fixture);if(!a||a->status!="ok"){all=false;break;}parts+=std::max(0.0,a->frameMs-base->frameMs);}
    if(all){const double extra=r->frameMs-base->frameMs,limit=1.3*parts+1.5;if(extra>limit){cell.ok=false;cell.why.push_back("cost: together "+num(extra,2)+" ms over the bare tube, the parts "+num(parts,2)+" ms");}
     else cell.why.push_back("cost "+num(extra,2)+" ms vs parts "+num(parts,2)+" ms");}
    double worst=0;for(const auto& n:group){auto* a=job({n},m.fixture);if(a&&a->status=="ok")worst=std::max(worst,a->flicker);}
    if(r->flicker>worst+.02){cell.ok=false;cell.why.push_back("new flicker where they overlap: "+num(r->flicker*100,1)+"% of pixels vs "+num(worst*100,1)+"% alone");}}
   if(alone&&col!="alone"&&alone->ok()&&!r->featureOk(m.feature)&&cell.ok){cell.ok=false;cell.why.push_back("passes alone, fails with "+col);}
   matrixFailed|=!cell.ok;mo.cells.push_back(cell);}
  matrices.push_back(mo);}
 // Contact sheet per feature: every tube it ran in, its kept frames then its reporter view.
 std::map<std::string,std::vector<const JobResult*>> byFeature;for(const auto& r:results){if(r.spec.group.empty())byFeature["baseline"].push_back(&r);for(const auto& f:r.spec.group)byFeature[f].push_back(&r);}
 std::map<std::string,std::string> sheets;
 for(const auto& [feature,list]:byFeature){std::vector<Image> owned;std::vector<std::pair<std::string,const Image*>> tiles;owned.reserve(64);int columns=1;
  for(auto* r:list){int n=0;for(const auto& [label,path]:r->images){if(owned.size()>=64)break;Image img;
    // decode the PNG we wrote via WIC is heavier than re-reading pixels; tiles come from the files
    IWICImagingFactory* f=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f)))){IWICBitmapDecoder* d=nullptr;IWICBitmapFrameDecode* fr=nullptr;IWICFormatConverter* cv=nullptr;
     const std::wstring wide=(out/path).wstring();
     if(SUCCEEDED(f->CreateDecoderFromFilename(wide.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d))&&SUCCEEDED(d->GetFrame(0,&fr))&&SUCCEEDED(f->CreateFormatConverter(&cv))&&SUCCEEDED(cv->Initialize(fr,GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){
      UINT w=0,h=0;cv->GetSize(&w,&h);img=Image(int(w),int(h));cv->CopyPixels(nullptr,w*4,w*h*4,reinterpret_cast<BYTE*>(img.pixels.data()));}
     if(cv)cv->Release();if(fr)fr->Release();if(d)d->Release();f->Release();}
    if(img.width==0)continue;owned.push_back(std::move(img));tiles.push_back({r->spec.id()+" "+label,&owned.back()});++n;}
   columns=std::max(columns,n);}
  if(tiles.empty())continue;const std::string name=feature+"-sheet.png";if(savePng((out/name).string(),contactSheet(tiles,columns,240)))sheets[feature]=name;}
 // metrics.json
 {std::ofstream j(out/"metrics.json");j<<"{\n \"gpu\": "<<json(gpu)<<",\n \"wall_ms\": "<<num(wallMs,1)<<",\n \"renderer_load_ms\": "<<num(loadMs,1)<<",\n \"tubes\": [\n";
  for(size_t i=0;i<results.size();++i){const auto& r=results[i];
   j<<"  {\"id\": "<<json(r.spec.id())<<", \"group\": "<<json(join(r.spec.group,"+"))<<", \"fixture\": "<<json(r.spec.fixture)<<", \"status\": "<<json(r.status)<<", \"error\": "<<json(r.error)
    <<", \"passed\": "<<(r.ok()?"true":"false")<<", \"wall_ms\": "<<num(r.wallMs,1)<<", \"render_ms\": "<<num(r.renderMs)<<", \"update_ms\": "<<num(r.updateMs)<<", \"frame_ms\": "<<num(r.frameMs)<<", \"flicker\": "<<num(r.flicker,5)
    <<", \"frames\": "<<r.frames<<", \"hash\": "<<json(r.hash)<<", \"repeat_hash\": "<<json(r.repeatHash)<<",\n   \"metrics\": {";
   for(size_t k=0;k<r.metrics.size();++k)j<<(k?", ":"")<<json(r.metrics[k].name)<<": "<<num(r.metrics[k].value,6);
   j<<"},\n   \"checks\": [";for(size_t k=0;k<r.checks.size();++k)j<<(k?", ":"")<<"{\"owner\": "<<json(r.checks[k].owner)<<", \"name\": "<<json(r.checks[k].name)<<", \"ok\": "<<(r.checks[k].ok?"true":"false")<<", \"detail\": "<<json(r.checks[k].detail)<<"}";
   j<<"]}"<<(i+1<results.size()?",":"")<<"\n";}
  j<<" ],\n \"matrices\": [\n";
  for(size_t i=0;i<matrices.size();++i){const auto& m=matrices[i];j<<"  {\"feature\": "<<json(m.m->feature)<<", \"fixture\": "<<json(fs::path(m.m->fixture).stem().string())<<", \"cells\": {";
   for(size_t c=0;c<m.cells.size();++c)j<<(c?", ":"")<<json(m.m->columns[c])<<": {\"ok\": "<<(m.cells[c].ok?"true":"false")<<", \"why\": "<<json(join(m.cells[c].why,"; "))<<"}";
   j<<"}}"<<(i+1<matrices.size()?",":"")<<"\n";}
  j<<" ]\n}\n";}
 // report.html
 int failed=0,skipped=0;for(const auto& r:results){failed+=!r.ok();skipped+=r.status=="skipped";}
 std::ofstream h(out/"report.html");
 h<<"<!doctype html><meta charset=utf-8><title>Feature lab report</title><style>body{font:13px/1.4 system-ui,sans-serif;margin:16px;background:#111;color:#ddd}h2{margin-top:28px;border-bottom:1px solid #444}"
    "table{border-collapse:collapse;margin:8px 0}td,th{border:1px solid #333;padding:3px 6px;vertical-align:top}.ok{color:#6c6}.bad{color:#f66;font-weight:bold}.skip{color:#cc6}img{max-width:480px;margin:2px;border:1px solid #333}"
    ".pair{display:flex;gap:6px;flex-wrap:wrap}.small{color:#999;font-size:11px}</style>\n";
 h<<"<h1>Feature lab: "<<(failed||matrixFailed?"<span class=bad>FAIL</span>":"<span class=ok>PASS</span>")<<"</h1><p>"<<results.size()<<" tubes, "<<failed<<" failed, "<<skipped<<" skipped; wall "<<num(wallMs/1000,1)<<" s (renderer load "<<num(loadMs/1000,1)<<" s); "<<html(gpu)<<"</p>\n";
 for(const auto& m:matrices){h<<"<h2>Friends: "<<html(m.m->feature)<<" in "<<html(fs::path(m.m->fixture).stem().string())<<"</h2><table><tr>";for(const auto& c:m.m->columns)h<<"<th>"<<html(c=="alone"?"alone":c=="all"?"+ all friends":"+ "+c)<<"</th>";h<<"</tr><tr>";
  for(const auto& c:m.cells){h<<"<td><span class="<<(c.ok?"ok>PASS":"bad>FAIL")<<"</span><div class=small>";for(const auto& w:c.why)h<<html(w)<<"<br>";h<<"</div></td>";}h<<"</tr></table>\n";}
 for(const auto& [feature,list]:byFeature){auto* f=find(feature);h<<"<h2>"<<html(feature)<<"</h2>";if(f)h<<"<p>"<<html(f->summary)<<(f->friends.empty()?"":" <span class=small>friends: "+html(join(f->friends,", "))+"</span>")<<"</p>";
  if(sheets.count(feature))h<<"<p><a href=\""<<sheets[feature]<<"\">contact sheet</a></p>";
  for(auto* r:list){h<<"<h3>"<<html(r->spec.id())<<" &mdash; <span class="<<(r->status=="skipped"?"skip>SKIPPED":r->ok()?"ok>PASS":"bad>FAIL")<<"</span> <span class=small>"<<num(r->wallMs/1000,2)<<" s wall, "<<r->frames<<" frames, render "<<num(r->renderMs,2)<<" ms, update "<<num(r->updateMs,2)<<" ms, flicker "<<num(r->flicker*100,2)<<"%</span></h3>";
   if(!r->error.empty())h<<"<p class=bad>"<<html(r->error)<<"</p>";
   h<<"<table>";for(const auto& c:r->checks)h<<"<tr><td class="<<(c.ok?"ok>PASS":"bad>FAIL")<<"</td><td>"<<html(c.owner)<<"</td><td>"<<html(c.name)<<"</td><td class=small>"<<html(c.detail)<<"</td></tr>";h<<"</table>";
   h<<"<table>";for(const auto& m:r->metrics)h<<"<tr><td>"<<html(m.name)<<"</td><td>"<<num(m.value,4)<<"</td><td class=small>"<<html(m.unit)<<"</td></tr>";h<<"</table>";
   // without and with the reporter, side by side
   std::string last,rep;std::vector<std::pair<std::string,std::string>> extra;for(const auto& [label,path]:r->images){if(label=="reporter")rep=path;else if(label.rfind("frame ",0)==0)last=path;else extra.push_back({label,path});}
   h<<"<div class=pair>";if(!last.empty())h<<"<figure><img src=\""<<last<<"\"><figcaption class=small>last frame</figcaption></figure>";if(!rep.empty())h<<"<figure><img src=\""<<rep<<"\"><figcaption class=small>reporter</figcaption></figure>";
   for(const auto& [label,path]:extra)h<<"<figure><img src=\""<<path<<"\"><figcaption class=small>"<<html(label)<<"</figcaption></figure>";h<<"</div>";}}
 std::cout<<"\nFEATURE LAB "<<(failed||matrixFailed?"FAIL":"PASS")<<": "<<results.size()<<" tubes, "<<failed<<" failed, "<<skipped<<" skipped, "<<num(wallMs/1000,1)<<" s -> "<<(out/"report.html").string()<<"\n";
 for(const auto& m:matrices){std::cout<<"  matrix "<<m.m->feature<<" @ "<<fs::path(m.m->fixture).stem().string()<<":";for(size_t c=0;c<m.cells.size();++c)std::cout<<"  "<<m.m->columns[c]<<"="<<(m.cells[c].ok?"PASS":"FAIL");std::cout<<"\n";}
 for(const auto& r:results)if(!r.ok()){std::cout<<"  FAIL "<<r.spec.id()<<(r.error.empty()?"":": "+r.error)<<"\n";for(const auto& c:r.checks)if(!c.ok)std::cout<<"     "<<c.owner<<": "<<c.name<<" ("<<c.detail<<")\n";}
 return failed||matrixFailed?1:0;
}
// ---------------------------------------------------------------- live
static int live(const Options& opt,Renderers& renderers){
 if(opt.groups.empty())throw std::runtime_error("lab --live: name one --feature");
 const auto fixtures=opt.fixtures.empty()?std::vector<std::string>{}:findFixtures(opt.fixtures);
 std::vector<const Feature*> group;for(const auto& n:opt.groups[0]){auto* f=find(n);if(!f)throw std::runtime_error("lab: no feature "+n);group.push_back(f);}
 const std::string fixturePath=!fixtures.empty()?fixtures[0]:findFixtures(group[0]->fixtures.at(0))[0];
 Fixture fx=loadFixture(fixturePath);SoftwareRenderer& r=*renderers.get(fx.width,fx.height,!opt.software&&!fx.software);
 Win32Window window(fx.width*2,fx.height*2,L"RawMetalLab live");if(!window.valid())return 1;
 const int frames=std::max(1,opt.frames?opt.frames:fx.frames?fx.frames:120);const float dt=opt.step>0?opt.step:fx.step>0?fx.step:1.f/60;
 std::uint64_t tuneSeen=tuneGeneration();Scene s;s.renderer=&r;const auto began=Clock::now();int shown=0;
 auto rebuild=[&]{LabAccess::resetRenderer(r);fx=loadFixture(fixturePath);r.setEnvironmentInspection(fx.value("hud","off")!="on");s.dt=dt;buildScene(s,fx,group);s.frames=frames;s.dt=dt;};rebuild();
 std::cout<<"live: "<<join(opt.groups[0],"+")<<" in "<<fixturePath<<"; edit lab/tune.txt and watch; close the window to stop\n";
 while(window.pump()){
  if(opt.liveSeconds>0&&since(began)>opt.liveSeconds*1000)break;
  const auto tg=tuneGeneration();if(tg!=tuneSeen){tuneSeen=tg;std::cout<<"tune reloaded; tube restarted\n";rebuild();}
  if(s.frame>=frames){rebuild();}
  s.input={};for(auto* f:group)if(f->step)f->step(s,dt);if(s.frame>0)s.g().update(s.input,dt);
  const auto t=Clock::now();r.render(s.g());const double ms=since(t);Frame frame{r.pixels(),r.width(),r.height(),s.frame,ms,0};for(auto* f:group)if(f->capture)f->capture(s,frame);
  Image shot=Image::of(r.pixels(),r.width(),r.height());
  if(opt.reporter!="none"){Overlay o;for(auto* f:group)if(f->reporter&&opt.reporterOn(f->name))f->reporter(s,o);drawOverlay(shot,o,[&](Point3 w){return project(s,w);});}
  drawText(shot,4,4,"FRAME "+std::to_string(s.frame)+"  "+num(ms,2)+" MS",0xffff60);
  window.present(shot.pixels.data(),shot.width,shot.height);
  if(++shown%30==0)savePng((fs::path(opt.out)/"live.png").string(),shot);
  ++s.frame;s.time+=dt;}
 return 0;
}
}
using namespace retro::lab;
int main(int argc,char** argv){
 SetConsoleOutputCP(CP_UTF8);CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 Options opt;std::vector<std::string> args(argv+1,argv+argc);
 auto value=[](const std::string& a,const char* key)->const char*{const size_t n=std::strlen(key);return a.compare(0,n,key)==0?a.c_str()+n:nullptr;};
 for(const auto& a:args){
  if(auto v=value(a,"--feature=")){for(const auto& g:split(v,',')){auto group=split(g,'+');if(group==std::vector<std::string>{"baseline"})group.clear();opt.groups.push_back(group);}} // baseline: the bare fixture
  else if(auto v=value(a,"--fixtures="))opt.fixtures=v;else if(auto v=value(a,"--frames="))opt.frames=std::atoi(v);else if(auto v=value(a,"--step="))opt.step=float(std::atof(v));
  else if(auto v=value(a,"--out="))opt.out=v;else if(auto v=value(a,"--jobs="))opt.jobs=std::max(1,std::atoi(v));else if(auto v=value(a,"--reporter="))opt.reporter=v;else if(a=="--reporter")opt.reporter="all";
  else if(a=="--software")opt.software=true;else if(a=="--with-friends")opt.withFriends=true;else if(a=="--no-repeat")opt.repeat=false;else if(a=="--live")opt.live=true;
  else if(auto v=value(a,"--live-seconds="))opt.liveSeconds=float(std::atof(v));else if(a=="--child")opt.child=true;else if(a=="--list")opt.list=true;
  else{std::cerr<<"unknown argument "<<a<<"\n";return 2;}}
 if(opt.out.empty())opt.out=(fs::path(labRoot())/".build"/"lab-out").string();
 std::error_code ec;fs::create_directories(opt.out,ec);
 // Knobs: RM_TUNE, else lab/tune.txt.
 retro::tuneFile(std::getenv("RM_TUNE")?std::getenv("RM_TUNE"):(fs::path(labRoot())/"lab"/"tune.txt").string());
 if(opt.list){std::cout<<"features:\n";for(const auto& f:features()){std::cout<<"  "<<f.name<<" - "<<f.summary;if(!f.friends.empty())std::cout<<" [friends: "<<join(f.friends,", ")<<"]";if(!f.fixtures.empty())std::cout<<" {"<<join(f.fixtures,", ")<<"}";std::cout<<"\n";}
  std::cout<<"fixtures:\n";for(const auto& p:findFixtures("*"))std::cout<<"  "<<fs::path(p).stem().string()<<"\n";return 0;}
 if(opt.groups.empty()){std::cerr<<"RawMetalLab: --feature=NAME[,NAME|A+B] (--list shows them)\n";return 2;}
 std::string error;const auto loadStart=Clock::now();if(!useLooseAssets(error)){std::cerr<<"lab: "<<error<<"\n";return 3;}
 Renderers renderers;
 try{
  if(opt.live)return live(opt,renderers);
  if(opt.child){JobSpec spec{opt.groups.at(0),opt.fixtures=="none"?std::string("none"):findFixtures(opt.fixtures).at(0)};if(spec.group.size()==1&&spec.group[0]=="baseline")spec.group.clear();auto r=runJob(spec,opt,renderers);return r.ok()?0:1;}
  const auto started=Clock::now();const Plan p=plan(opt);std::vector<JobResult> results;
  std::cout<<"lab: "<<p.jobs.size()<<" tubes, "<<opt.jobs<<" at a time, out "<<opt.out<<"\n";
  if(opt.jobs<=1){for(const auto& j:p.jobs){auto r=runJob(j,opt,renderers);std::cout<<"  "<<(r.status=="skipped"?"SKIP":r.ok()?"PASS":"FAIL")<<" "<<j.id()<<"  "<<num(r.wallMs/1000,2)<<" s"<<(r.error.empty()?"":"  "+r.error)<<"\n";results.push_back(std::move(r));}}
  else{
   wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);struct Running {PROCESS_INFORMATION pi;size_t index;Clock::time_point t;};std::vector<Running> running;size_t next=0;results.resize(p.jobs.size());
   while(next<p.jobs.size()||!running.empty()){
    while(next<p.jobs.size()&&int(running.size())<opt.jobs){const auto& j=p.jobs[next];
     std::wstring cmd=L"\""+std::wstring(exe)+L"\" --child --feature="+fs::path(j.group.empty()?std::string("baseline"):join(j.group,"+")).wstring()+L" \"--fixtures="+fs::path(j.fixture).wstring()+L"\" \"--out="+fs::path(opt.out).wstring()+L"\"";
     if(opt.frames)cmd+=L" --frames="+std::to_wstring(opt.frames);if(opt.step>0)cmd+=L" --step="+fs::path(std::to_string(opt.step)).wstring();if(opt.software)cmd+=L" --software";if(!opt.repeat)cmd+=L" --no-repeat";cmd+=L" --reporter="+fs::path(opt.reporter).wstring();
     STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};std::vector<wchar_t> line(cmd.begin(),cmd.end());line.push_back(0);
     if(!CreateProcessW(nullptr,line.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){results[next].spec=j;results[next].error="could not start a child";++next;continue;}
     running.push_back({pi,next,Clock::now()});++next;}
    std::vector<HANDLE> handles;for(auto& r:running)handles.push_back(r.pi.hProcess);
    const DWORD w=WaitForMultipleObjects(DWORD(handles.size()),handles.data(),FALSE,600000);if(w==WAIT_TIMEOUT)throw std::runtime_error("lab: a child ran ten minutes");
    const size_t done=size_t(w-WAIT_OBJECT_0);auto rr=running[done];running.erase(running.begin()+long(done));CloseHandle(rr.pi.hThread);CloseHandle(rr.pi.hProcess);
    auto& res=results[rr.index];res.spec=p.jobs[rr.index];if(!readResult(fs::path(opt.out)/"jobs"/res.spec.id()/"result.txt",res)){res.status="error";res.error="the child left no result (crashed?)";}
    std::cout<<"  "<<(res.status=="skipped"?"SKIP":res.ok()?"PASS":"FAIL")<<" "<<res.spec.id()<<"  "<<num(res.wallMs/1000,2)<<" s (process "<<num(since(rr.t)/1000,2)<<" s)"<<(res.error.empty()?"":"  "+res.error)<<"\n";}}
  const auto assets=assetStats();std::cout<<"lab: loose assets "<<assets.loads<<" loads, "<<num(assets.bytes/1048576.0,1)<<" MB, "<<num(assets.ms/1000,2)<<" s decoding; renderer setup "<<num(renderers.loadMs/1000,2)<<" s\n";
  return report(opt,p,results,since(started),renderers.loadMs,renderers.gpuName.empty()?"(children)":renderers.gpuName);
 }catch(const std::exception& e){std::cerr<<"lab: "<<e.what()<<"\n";return 4;}
}
