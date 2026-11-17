// Soft padding (the testers: mattresses, pads, cushions, the bedroll and the floor mattress deform).
//
// softbody: the numbers, a floor mattress (med_mattress_stained, a prop) in a plain room and a
// strapped ward bed (med_ward_bed, a fixture) in Medical Ward C (map 75):
//  - the player walking onto it, standing, kneeling, lying, walking across, landing from a jump; how
//    deep it gives under each and how far the player sinks with it; stepping off, it springs back
//    and goes to sleep (no pad left);
//  - a crate dropped on it sinks in and rests there; a creature walking over it and a dead one lying
//    on it press it;
//  - on the ward bed the player stands on the mattress (not the bed's bounding box), it gives under
//    him, and where the straps cross it does not move: the straps still lie on it (MapAudit rule K);
//  - its cost: no pad and no time with nobody on it; a pressed pad's step.
// Its scenes film the floor mattress from the player's eyes as he walks on and a crate drops beside
// him (soft-mattress), and the ward bed as he stands on it by the straps (soft-ward): "before", "after".
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
constexpr Vec2 Mattress{10,8};
std::shared_ptr<CustomCampaign> mattressRoom(Vec2 start={8.4f,8}){auto m=regression::room();m->definition.playerStart=start;kit::prop(*m,model::med_mattress_stained,Mattress.x,Mattress.y,0,1.9f,.9f,.118f,0);return regression::campaign(m);}
float peakOf(const Game& g){float d=0;for(const auto& p:g.softPads())d=std::max(d,p.peak());return d;}
void run(Game& g,float seconds,InputState in={},float* lowest=nullptr,float* highest=nullptr){for(int i=0;i<int(seconds*60);++i){g.update(in,1.f/60);if(lowest)*lowest=std::min(*lowest,g.player().z);if(highest)*highest=std::max(*highest,g.player().z);}}
// The player walked from the room onto the mattress's middle and left standing there.
void walkOn(Game& g){auto& p=LabAccess::player(g);p.angle=0;InputState walk;walk.forward=true;for(int i=0;i<120&&p.pos.x<Mattress.x-.1f;++i){p.angle=0;g.update(walk,1.f/60);}}
// A strapped ward bed in the map: the fixture, and the straps lying across it.
struct Bed {int fixture=-1;std::vector<int> straps;};
Bed strappedBed(const Game& g){const World& w=g.world();Bed best;
 for(int i=0;i<int(w.fixtures().size());++i){const auto& f=w.fixtures()[size_t(i)];if(f.model!=model::med_ward_bed||!f.visible)continue;Bed b;b.fixture=i;
  const float top=w.floorHeight(f.position.x,f.position.y)+f.base+f.height*.404f/.842f;
  for(int s=0;s<int(w.structures().size());++s){const auto& v=w.structures()[size_t(s)];if(std::min(v.x2-v.x1,v.y2-v.y1)>.15f||std::fabs(v.bottom-top)>.03f)continue;
   const Vec2 c{(v.x1+v.x2)*.5f,(v.y1+v.y2)*.5f};if(std::fabs(c.x-f.position.x)<f.width*.5f&&std::fabs(c.y-f.position.y)<f.width*.5f&&length(c-f.position)<1.f)b.straps.push_back(s);}
  if(b.straps.size()>best.straps.size())best=b;}
 return best;}
Vec2 bedAxis(const retro::Fixture& f){return {std::cos(f.yaw),-std::sin(f.yaw)};}
float bedCrown(const Game& g,const retro::Fixture& f){return g.world().floorHeight(f.position.x,f.position.y)+f.base+f.height*.404f/.842f;}
// Stand the player on the bed's mattress between its straps (as far from any as the bed allows), facing along it.
void standOnBed(Game& g,const Bed& b){const auto& f=g.world().fixtures()[size_t(b.fixture)];auto& p=LabAccess::player(g);const Vec2 along=bedAxis(f);
 float best=0,room=-1;for(float t=-.6f;t<=.601f;t+=.05f){float nearest=9;for(int si:b.straps){const auto& s=g.world().structures()[size_t(si)];nearest=std::min(nearest,std::fabs(dot(Vec2{(s.x1+s.x2)*.5f,(s.y1+s.y2)*.5f}-f.position,along)-t));}if(nearest>room){room=nearest;best=t;}}
 p.pos=f.position+along*best;p.angle=std::atan2(along.y,along.x);p.z=bedCrown(g,f);p.grounded=true;p.verticalVelocity=0;p.pitch=-112;}
// The foot of the bed: the end of its axis with open floor past it.
Vec2 bedFoot(const Game& g,const retro::Fixture& f){const Vec2 along=bedAxis(f);for(float e:{1.f,-1.f}){const Vec2 q=f.position+along*(e*1.3f);if(g.world().tile(int(q.x),int(q.y))!='#'&&g.world().fits(q.x,q.y,g.world().floorHeight(q.x,q.y)+.02f,.5f))return along*e;}return along;}
// Inspection views: the mattress from its long side, the ward bed from its foot.
void sideView(Game& g){LabAccess::setCamera(g,{Mattress.x+.25f,Mattress.y-1.55f},-999,kPi*.5f,-104);}
void footView(Game& g,const Bed& b){const auto& f=g.world().fixtures()[size_t(b.fixture)];const Vec2 e=bedFoot(g,f),at=f.position+e*1.35f;LabAccess::setCamera(g,at,-999,std::atan2(-e.y,-e.x),-100);}
std::shared_ptr<Game> wardGame(){auto g=std::make_shared<Game>(WorldId::Campaign);LabAccess::loadLevel(*g,75);LabAccess::enemies(*g).clear();LabAccess::setGod(*g,true);return g;}

const bool tube=add({
 .name="softbody",.summary="mattresses, the bedroll, ward beds and cushions give under the player, props, creatures and the dead, recover and sleep; straps stay on the mattress",
 .fixtures={"soft-mattress","soft-ward"},
 .author=[](Scene& s,AuthoredMapData& m){if(s.fixture->value("soft","mattress")=="mattress")kit::prop(m,model::med_mattress_stained,Mattress.x,Mattress.y,0,1.9f,.9f,.118f,0);},
 // Film: the view (frames 0-3, "before" at 3); the player walks on / is stood on the bed and stays;
 // a crate drops on the mattress's far end; on the last frame the view again ("after": the dents he
 // and the crate leave, a frame after he is lifted off to hold the camera).
 .setup=[](Scene& s){Game& g=s.g();LabAccess::enemies(g).clear();LabAccess::setGod(g,true);
  if(s.fixture->value("soft","mattress")=="mattress")sideView(g);else{const auto b=strappedBed(g);if(b.fixture>=0)footView(g,b);}},
 .step=[](Scene& s,float){Game& g=s.g();auto& p=LabAccess::player(g);const bool mattress=s.fixture->value("soft","mattress")=="mattress";const bool last=s.frame==s.frames-1;
  if(mattress){
   if(s.frame==5){LabAccess::playerCamera(g);p.pos={8.3f,Mattress.y-.15f};p.z=0;p.angle=0;p.pitch=-60;p.grounded=true;}
   if(s.frame>5&&!last&&p.pos.x<Mattress.x-.45f){s.input.forward=true;p.angle=0;}
   if(s.frame==60){Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.45f,.45f,.45f};c.pos={Mattress.x+.6f,Mattress.y+.1f};c.z=.9f;c.sleeping=false;LabAccess::clutter(g).push_back(c);}
   if(last)sideView(g);}
  else{const auto b=strappedBed(g);if(b.fixture<0)return;if(s.frame==5){LabAccess::playerCamera(g);standOnBed(g,b);const auto& f=g.world().fixtures()[size_t(b.fixture)];p.pos=f.position+bedFoot(g,f)*.38f;}if(last)footView(g,b);}},
 .capture=[](Scene& s,const Frame&){if(s.frame==3)s.shoot("before");if(s.last()){s.shoot("after");s.board["soft.shot_peak"]=peakOf(s.g());}},
 .metrics=[](Scene& s,Report& r){
  r.metric("scene_dent_at_after_shot",s.board["soft.shot_peak"],"m");
  if(s.fixture->value("soft","mattress")!="mattress"){
   // ---- the ward bed (map 75) ----
   auto gp=wardGame();Game& g=*gp;const auto bed=strappedBed(g);r.check("map 75 has a strapped ward bed",bed.fixture>=0&&!bed.straps.empty(),std::to_string(bed.straps.size())+" straps");if(bed.fixture<0)return;
   const auto& f=g.world().fixtures()[size_t(bed.fixture)];const float crown=g.world().floorHeight(f.position.x,f.position.y)+f.base+f.height*.404f/.842f;
   standOnBed(g,bed);run(g,1.5f);auto& p=LabAccess::player(g);const auto* pad=g.softPad(g.level(),bed.fixture,-1);
   const float sink=crown-p.z;r.metric("ward_stand_sink",sink,"m");r.metric("ward_peak",pad?pad->peak():0,"m");
   r.check("the player stands on the ward bed's mattress (not its 84 cm box) and it gives under him",pad&&p.grounded&&sink>.015f&&sink<.06f&&p.z<crown+.01f,"z "+fixed(p.z)+" crown "+fixed(crown)+" sink "+fixed(sink));
   float under=0;std::string where;for(int si:bed.straps){const auto& v=g.world().structures()[size_t(si)];for(float a:{.2f,.5f,.8f})for(float b:{.2f,.5f,.8f}){const Vec2 q{v.x1+(v.x2-v.x1)*a,v.y1+(v.y2-v.y1)*b};if(pad&&pad->covers(q))under=std::max(under,pad->depthAt(q));}}
   r.metric("ward_depth_under_straps",under,"m");
   r.check("where the straps cross the mattress it does not go down: they still lie on it (rule K)",under<.004f,"deepest under a strap "+fixed(under,4));
   // The cost: a ward of beds and nobody on them.
   {auto g2=wardGame();auto& q=LabAccess::player(*g2);q.pos={12,12};const auto t0=std::chrono::steady_clock::now();for(int i=0;i<600;++i)LabAccess::stepSoft(*g2,1.f/60);
    const double idle=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/600;r.metric("ward_idle_step_us",idle,"us");
    r.check("with nobody on them the ward's beds keep no pad",g2->softPads().empty());
    standOnBed(*g2,bed);for(int i=0;i<30;++i)g2->update({},1.f/60);const auto t1=std::chrono::steady_clock::now();for(int i=0;i<600;++i)LabAccess::stepSoft(*g2,1.f/60);
    const double pressed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t1).count()/600;r.metric("ward_pressed_step_us",pressed,"us");
    r.check("a pressed bed's step stays cheap (under 0.25 ms)",pressed<250,fixed(pressed,1)+" us");}
   return;}
  // ---- the floor mattress ----
  const float top=.118f;
  {Game g(mattressRoom());walkOn(g);float lowest=9,highest=-9;run(g,1.5f,{},&lowest,&highest);const float stand=top-g.player().z,peak=peakOf(g);
   r.metric("stand_sink",stand,"m");r.metric("stand_peak_depth",peak,"m");
   r.check("the player walks onto the floor mattress and sinks a few cm standing on it",g.player().pos.x>Mattress.x-.3f&&stand>.02f&&stand<.06f&&peak>=stand-.005f,"sink "+fixed(stand)+" peak "+fixed(peak));
   r.check("the mattress is not shoved away by standing on it",length(g.clutter()[0].pos-Mattress)<.01f,fixed(length(g.clutter()[0].pos-Mattress)));
   InputState kneel;kneel.crouch=true;run(g,1.f,kneel);const float crouch=top-g.player().z;r.metric("crouch_sink",crouch,"m");
   r.check("kneeling presses it a little deeper",crouch>stand&&crouch<.07f,fixed(crouch));
   InputState prone;prone.prone=true;run(g,.05f,prone);run(g,1.f);const float lying=top-g.player().z;r.metric("prone_sink",lying,"m");
   r.check("lying on it spreads the weight: shallower",g.player().prone&&lying<stand&&lying>.005f,fixed(lying));
   // Off it: it springs back and sleeps.
   InputState up;up.prone=true;run(g,.05f,up);run(g,.5f);LabAccess::player(g).angle=kPi;InputState away;away.forward=true;
   // From the moment he walks off: back within a millimetre, and the pad asleep (gone).
   float off=-1,flat=-1,slept=-1;for(int i=0;i<300&&slept<0;++i){g.update(i<60?away:InputState{},1.f/60);const float t=float(i+1)/60;
    if(off<0&&g.player().z<.001f)off=t;if(off>=0&&flat<0&&peakOf(g)<.001f)flat=t-off;if(off>=0&&g.softPads().empty())slept=t-off;}
   r.metric("recover_to_1mm_s",flat,"s");r.metric("recover_to_sleep_s",slept,"s");
   r.check("stepped off, it springs back within a second and the pad goes to sleep",off>0&&flat>=0&&flat<1.f&&slept>=flat&&slept<1.5f,"off at "+fixed(off,2)+" s, flat "+fixed(flat,2)+" s, asleep "+fixed(slept,2)+" s");}
  // Walking across it: the step's load moves with each foot.
  {Game g(mattressRoom({8.4f,8}));auto& p=LabAccess::player(g);float lowest=9,highest=-9,deepest=0;InputState walk;walk.forward=true;
   for(int i=0;i<150&&p.pos.x<11.6f;++i){p.angle=0;g.update(walk,1.f/60);if(std::fabs(p.pos.x-Mattress.x)<.6f){lowest=std::min(lowest,p.z);highest=std::max(highest,p.z);deepest=std::max(deepest,peakOf(g));}}
   r.metric("walk_depth_max",deepest,"m");r.metric("walk_bob",highest-lowest,"m");
   r.check("walking across it, it gives under each step and the player stays on it",deepest>.015f&&p.pos.x>11.2f&&highest<=top+.002f,"deepest "+fixed(deepest)+" z "+fixed(lowest)+".."+fixed(highest));}
  // Landing on it from a jump: deeper than standing, then back.
  {Game g(mattressRoom());walkOn(g);run(g,1.f);const float stand=peakOf(g);InputState jump;jump.jump=true;g.update(jump,1.f/60);
   float peak=0;for(int i=0;i<120;++i){g.update({},1.f/60);peak=std::max(peak,peakOf(g));}run(g,1.5f);const float after=peakOf(g);
   r.metric("land_peak_depth",peak,"m");r.check("landing on it from a jump dents it deeper than standing, and it settles back",peak>stand*1.2f&&std::fabs(after-stand)<.006f,"stand "+fixed(stand)+" land "+fixed(peak)+" after "+fixed(after));}
  // A crate dropped on it sinks in and rests there; it holds the dent asleep (no stepping).
  {Game g(mattressRoom({3,3}));Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.45f,.45f,.45f};c.pos=Mattress;c.z=.9f;LabAccess::clutter(g).push_back(c);run(g,3);
   const auto& v=g.clutter()[1];const float in=top-v.z;r.metric("crate_rest_sink",in,"m");
   r.check("a 9 kg crate dropped on it rests sunk in it, asleep, on the mattress",v.sleeping&&in>.003f&&in<.03f&&!v.broken,"sunk "+fixed(in,4));
   const auto* pad=g.softPad(g.level(),-1,0);r.check("under a load that does not change the pad holds its dent without stepping",pad&&pad->still>.3f,pad?fixed(pad->still,2):"no pad");}
  // A creature walking over it and a dead one lying on it.
  {Game g(mattressRoom({3,3}));Enemy e;e.pos={Mattress.x-.3f,Mattress.y};e.z=0;e.moving=true;LabAccess::enemies(g)={e};for(int i=0;i<30;++i)LabAccess::stepSoft(g,1.f/60);const float walking=peakOf(g);
   LabAccess::enemies(g)[0].alive=false;LabAccess::enemies(g)[0].z=top;for(int i=0;i<60;++i)LabAccess::stepSoft(g,1.f/60);const float dead=peakOf(g);
   r.metric("creature_depth",walking,"m");r.metric("corpse_depth",dead,"m");
   r.check("a creature walking over it and a body lying on it press it in",walking>.01f&&dead>.01f,fixed(walking)+" / "+fixed(dead));}
  // Asleep when untouched: nothing near it, nothing kept.
  {Game g(mattressRoom({3,3}));for(int i=0;i<60;++i)g.update({},1.f/60);r.check("untouched, it keeps no pad",g.softPads().empty());}
 }});
}
}
