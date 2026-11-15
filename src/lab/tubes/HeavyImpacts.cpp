// Heavy props against glass and against each other (the user on v1.4.7: "heavy objects can't
// crack windows or knock heavy objects like chairs").
//
// heavy-impacts: the numbers, each a fresh plain room stepped as play steps it (Game::update):
//  - a 9 kg crate thrown at a pane at several speeds: the pane's damage (its crack stage is
//    damage / what breaks it) and where it breaks; the same throw by the player's own hand at
//    60 and 144 Hz; a light mug tossed gently and thrown; holds glass hit hard and often;
//    a crate and a barrel shoved into a pane at a sprint (a crack, not a pane ground away);
//    a crate dropped onto a glass floor;
//  - a crate thrown and shoved into chairs, a barrel rolled into one, a mug thrown at one;
//  - a resting stack left 10 s, and the same stack woken.
// Its scenes (fixtures heavy-pane, heavy-chair) film a thrown crate cracking a pane and a
// thrown crate knocking a chair over: shots "before" and "after".
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
Clutter make(int model,Vec2 at,float z){Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;c.pos=at;c.z=z;return c;}
// As Game::updateClutter throws it: forward at 9 m/s scaled for its mass, 2 m/s up, tumbling.
void thrown(Clutter& c,Vec2 dir,float speed,float up=2){c.velocity=dir*speed;c.vz=up;c.spin=3;c.pitchSpeed=10;c.rollSpeed=4;c.projectile=true;c.sleeping=false;c.restTime=0;}
float throwSpeed(int model){return 9.f*std::clamp(std::sqrt(2.5f/Game::propTraits(model).mass),.35f,1.f);}
float tilt(const Clutter& c){return std::max(std::fabs(c.pitch),std::fabs(c.roll));}
bool toppled(const Clutter& c){return tilt(c)>.9f;}

// ---- panes ----
struct PaneRun {float damage=0,strain=0,peak=0,x=0;bool broken=false,through=false,crackSound=false,shatter=false,stimulus=false,propBroken=false;std::string note;};
struct PaneRoom {bool holds=false,floor=false;};
std::shared_ptr<CustomCampaign> paneRoom(PaneRoom r){auto m=regression::room();m->definition.playerStart={3,3};
 if(r.floor)kit::breakable(*m,7,7,10,10,1.f,1.03f,17,r.holds);else kit::breakable(*m,9.97f,5,10.03f,11,0,3,17,r.holds);return regression::campaign(m);}
void watch(Game& g,PaneRun& out){for(const auto& s:g.sounds()){if(s.sound==Sound::JunkGlass)out.crackSound=true;if(s.sound==Sound::GlassShatter){out.shatter=true;out.stimulus|=s.aiRadius>=14;}}
 for(const auto& c:LabAccess::clutter(g))if(c.model>=0&&!c.broken)out.peak=std::max(out.peak,std::sqrt(lengthSq(c.velocity)+c.vz*c.vz));}
void finish(Game& g,PaneRun& out,int index=0){const World& w=g.world();out.broken=!w.breakableIntact(0);const auto& b=w.breakables()[0];
 const float limit=b.holds?180.f:b.health>0?b.health:20.f;out.damage=out.broken?limit:w.breakableDamage(0);out.strain=out.damage/limit;
 const auto& c=LabAccess::clutter(g)[size_t(index)];out.x=c.pos.x;out.propBroken=c.broken;out.through=b.bottom>.5f?c.z<b.bottom-.1f:c.pos.x>10.2f;}
// A prop let go with a velocity at the room's upright pane (x = 10) or over its glass floor.
PaneRun paneHit(int model,Vec2 start,float z,Vec2 v,float vz,PaneRoom room={},float hz=60,float seconds=1.2f,bool tumble=true){
 Game g(paneRoom(room));LabAccess::player(g).pos={3,3};auto c=make(model,start,z);if(tumble)thrown(c,length(v)>1e-4f?v*(1/length(v)):Vec2{1,0},length(v),vz);else{c.velocity=v;c.vz=vz;c.sleeping=false;}
 LabAccess::clutter(g)={c};PaneRun out;for(int i=0;i<int(seconds*hz);++i){g.update({},1.f/hz);watch(g,out);}finish(g,out);return out;}
// The player picks the prop up in front of him and throws it (use, then fire), at `hz`.
PaneRun playerThrow(int model,float hz){Game g(paneRoom({}));auto& p=LabAccess::player(g);p.pos={7.9f,8};p.angle=0;p.pitch=0;
 LabAccess::clutter(g)={make(model,{8.6f,8},0)};const bool grabbed=LabAccess::grab(g);PaneRun out;out.note=std::string("grabbed ")+(grabbed?"yes":"no");
 for(int i=0;i<int(1.6f*hz);++i){InputState in;in.fire=i==int(.4f*hz);if(in.fire){const auto& c=LabAccess::clutter(g)[0];out.note+=" held at "+fixed(c.pos.x)+","+fixed(c.z)+" held "+std::to_string(LabAccess::held(g));}g.update(in,1.f/hz);watch(g,out);}
 finish(g,out);out.note+=" ends at "+fixed(out.x)+" held "+std::to_string(LabAccess::held(g));return out;}
// The player sprints into a prop standing before the pane for `seconds`, and keeps pushing.
struct Shove {PaneRun first,last;float firstAt=-1;};
Shove shovePane(int model,float seconds){Game g(paneRoom({}));auto& p=LabAccess::player(g);auto c=make(model,{9.2f,8},0);const float half=Game::propTraits(model).size[0]*.5f;
 c.pos.x=9.95f-half-.45f;LabAccess::clutter(g)={c};p.pos={c.pos.x-half-.5f,8};p.angle=0;p.pitch=0;Shove out;
 for(int i=0;i<int(seconds*60);++i){InputState in;in.forward=true;in.sprint=true;p.angle=0;g.update(in,1.f/60);watch(g,out.last);
  if(out.firstAt<0&&g.world().breakableDamage(0)>0){out.firstAt=float(i)/60;finish(g,out.first);}}
 finish(g,out.last);return out;}

// ---- props against props ----
struct Knock {float moved=0,tiltMax=0,tilt=0,peak=0,strikerPeak=0;bool toppled=false,sound=false;};
// `striker` sent at `speed` along +x into `target` standing still; or (shove) pushed by the player at a sprint.
Knock knock(int striker,int target,float speed,bool shove=false,float up=0,float targetYaw=0,bool tumble=false){
 auto m=regression::room();m->definition.playerStart={3,3};Game g(regression::campaign(m));auto& p=LabAccess::player(g);p.pos={3,3};
 auto t=make(target,{10.f,8},0);t.yaw=targetYaw;t.sleeping=true;const float half=Game::propTraits(striker).size[0]*.5f+t.extent()[0];
 auto s=make(striker,{10.f-half-(shove?.25f:.55f),8},up>0?.35f:0.f);if(!shove){s.velocity={speed,0};s.vz=up;s.sleeping=false;if(tumble)thrown(s,{1,0},speed,up);}
 LabAccess::clutter(g)={s,t};if(shove){p.pos={s.pos.x-Game::propTraits(striker).size[0]*.5f-.5f,8};p.angle=0;p.pitch=0;}
 Knock out;const Vec2 from=t.pos;const float before=g.world().breakables().size();(void)before;
 for(int i=0;i<180;++i){InputState in;if(shove&&i<90){in.forward=true;in.sprint=true;p.angle=0;}g.update(in,1.f/60);const auto& list=LabAccess::clutter(g);const auto& v=list[1];
  out.tiltMax=std::max(out.tiltMax,tilt(v));out.peak=std::max(out.peak,std::sqrt(lengthSq(v.velocity)+v.vz*v.vz));out.strikerPeak=std::max(out.strikerPeak,std::sqrt(lengthSq(list[0].velocity)+list[0].vz*list[0].vz));
  for(const auto& e:g.sounds())if(length(e.position-v.pos)<1.5f&&e.gain>0)out.sound=true;}
 const auto& v=LabAccess::clutter(g)[1];out.moved=length(v.pos-from);out.tilt=tilt(v);out.toppled=toppled(v);return out;}

// ---- a resting stack ----
struct Stack {float drift=0,turn=0;int awake=0;};
Stack stack(bool woken){auto m=regression::room();m->definition.playerStart={3,3};
 kit::prop(*m,model::wooden_crate,10,8,0,0);kit::prop(*m,model::wooden_crate,10,8,.6f,.2f);kit::prop(*m,model::packing_crate,10.05f,8,1.2f,0);kit::prop(*m,model::mug,10.1f,8.05f,1.6f,0);
 kit::prop(*m,model::office_chair,10.75f,8,0,0);kit::prop(*m,model::wooden_chair,9.3f,8,0,1.2f);kit::prop(*m,model::ind_barrel,10,9.1f,0,0);
 Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};auto& list=LabAccess::clutter(g);if(woken)for(auto& c:list){c.sleeping=false;c.restTime=0;}
 const auto start=list;for(int i=0;i<600;++i)g.update({},1.f/60);Stack out;
 for(size_t i=0;i<list.size()&&i<start.size();++i){out.drift=std::max(out.drift,length(list[i].pos-start[i].pos)+std::fabs(list[i].z-start[i].z));out.turn=std::max({out.turn,std::fabs(list[i].pitch-start[i].pitch),std::fabs(list[i].roll-start[i].roll),std::fabs(list[i].yaw-start[i].yaw)});out.awake+=!list[i].sleeping;}
 return out;}

// ---- the filmed scenes ----
constexpr int Launch=20;
const bool tube=add({
 .name="heavy-impacts",.summary="heavy props crack and break glass by the energy of the blow (holds glass only cracks), and knock loose props aside and over; resting stacks stay put",
 .fixtures={"heavy-pane","heavy-chair"},
 .author=[](Scene& s,AuthoredMapData& m){const std::string what=s.fixture->value("heavy","pane");
  if(what=="pane")kit::breakable(m,9.97f,6.4f,10.03f,9.6f,0,2.4f,17);
  else{kit::prop(m,model::wooden_chair,10.2f,8,0,kPi*.5f);kit::prop(m,model::office_chair,11.6f,8.9f,0,.4f);}
  kit::prop(m,model::wooden_crate,what=="pane"?8.7f:9.05f,8,0,0);},
 .step=[](Scene& s,float){Game& g=s.g();if(s.frame!=Launch)return;const std::string what=s.fixture->value("heavy","pane");for(auto& c:LabAccess::clutter(g))if(c.model==model::wooden_crate){c.z=what=="pane"?.35f:.6f;thrown(c,{1,0},throwSpeed(c.model),what=="pane"?1.2f:1.f);}},
 .capture=[](Scene& s,const Frame&){if(s.frame==Launch-1)s.shoot("before");if(s.last())s.shoot("after");},
 .metrics=[](Scene& s,Report& r){const std::string what=s.fixture->value("heavy","pane");Game& g=s.g();
  if(what=="pane"){
   // The scene: the crate it filmed.
   const bool cracked=g.world().breakableIntact(0)&&g.world().breakableDamage(0)>0;r.metric("scene_pane_damage",g.world().breakableIntact(0)?g.world().breakableDamage(0):-1);
   r.check("scene: the thrown crate cracks the pane",cracked);
   // A 9 kg crate at speed into the pane, flat (no lift, no tumble): damage by speed.
   std::string ladder;float firstCrack=-1,firstBreak=-1;bool rising=true;float previous=-1;
   for(float v:{1.f,2.f,3.f,4.f,4.74f,6.f,7.f,8.f,10.f}){auto h=paneHit(model::wooden_crate,{8.9f,8},.4f,{v,0},0,{},60,1.2f,false);
    ladder+=fixed(v,1)+" m/s: "+(h.broken?"broken"+std::string(h.through?" and through":" (stopped)"):"strain "+fixed(h.strain))+"; ";
    r.metric("crate_"+fixed(v,1)+"ms_strain",h.strain);if(h.strain>0&&firstCrack<0)firstCrack=v;if(h.broken&&firstBreak<0)firstBreak=v;
    rising&=h.strain>=previous;previous=h.strain;}
   r.metric("crate_first_crack_ms",firstCrack,"m/s");r.metric("crate_first_break_ms",firstBreak,"m/s");
   r.check("a crate cracks the pane from a shove's speed and breaks it from a hard throw's, more the harder it hits",firstCrack>0&&firstCrack<=3&&firstBreak>=5&&firstBreak<=7.5f&&rising,ladder);
   // The player's own throw: the same at 60 and 144 Hz.
   for(float hz:{60.f,144.f}){auto t=playerThrow(model::wooden_crate,hz);r.metric("player_throw_"+fixed(hz,0)+"hz_strain",t.strain);
    r.check("the player's crate throw at "+fixed(hz,0)+" Hz cracks the pane (no break), with the glass sound",!t.broken&&t.strain>.3f&&t.strain<1&&t.crackSound,"strain "+fixed(t.strain)+" peak "+fixed(t.peak)+" m/s; "+t.note);}
   {auto a=playerThrow(model::wooden_crate,60),b=playerThrow(model::wooden_crate,144);r.check("the throw's crack does not hang on the frame rate",std::fabs(a.strain-b.strain)<.15f,fixed(a.strain)+" vs "+fixed(b.strain));}
   // A hard throw (VR, 9 m/s) and the thrown monitor break it; the prop carries on through.
   {auto h=paneHit(model::wooden_crate,{8.9f,8},.5f,{9,0},1);r.check("a hard-thrown crate breaks the pane with the shatter (a 14 m stimulus) and carries on through",h.broken&&h.through&&h.shatter&&h.stimulus,"x "+fixed(h.x)+" peak "+fixed(h.peak));r.metric("hard_throw_peak_ms",h.peak,"m/s");
    r.check("no explosive speed: nothing ever goes faster than it was thrown",h.peak<=std::sqrt(81.f+1)+.5f,fixed(h.peak));}
   // Light things.
   {auto gentle=paneHit(model::mug,{9.5f,8},.9f,{3,0},.5f),full=paneHit(model::mug,{9.5f,8},.9f,{9,0},.5f);r.metric("mug_3ms_strain",gentle.strain);r.metric("mug_9ms_strain",full.strain);
    r.check("a light mug tossed gently does nothing to the pane",gentle.damage==0&&!gentle.broken,fixed(gentle.damage));
    r.check("a mug thrown hard stars it, no more",!full.broken&&full.strain<.25f,fixed(full.strain));}
   // Holds glass: hit hard and often, it cracks to its last stage and never breaks.
   {Game g2(paneRoom({true}));LabAccess::player(g2).pos={3,3};PaneRun out;float worst=0;
    for(int k=0;k<6;++k){auto c=make(model::wooden_crate,{8.9f,8},.5f);thrown(c,{1,0},9,1);LabAccess::clutter(g2)={c};for(int i=0;i<60;++i){g2.update({},1.f/60);watch(g2,out);}worst=std::max(worst,g2.world().breakableDamage(0));}
    finish(g2,out);auto one=paneHit(model::wooden_crate,{8.9f,8},.4f,{4.74f,0},0,{true},60,1.2f,false);r.metric("holds_one_throw_strain",one.strain);r.metric("holds_six_hard_hits_strain",out.strain);
    r.check("holds glass cracks under a thrown crate and never breaks, however hard and often",!out.broken&&one.strain>0&&one.strain<.5f&&out.strain>one.strain&&LabAccess::clutter(g2)[0].pos.x<10,"one "+fixed(one.strain)+" six hard "+fixed(out.strain));}
   // Shoved at a sprint: a crack, not a pane worn through by a steady push.
   for(int model:{model::wooden_crate,model::ind_barrel}){auto sh=shovePane(model,3);const std::string name=model==model::wooden_crate?"crate":"barrel";
    r.metric(name+"_shove_first_strain",sh.first.strain);r.metric(name+"_shove_3s_strain",sh.last.strain);
    r.check("a "+name+" shoved into the pane at a sprint cracks it, and three seconds' push does not wear it through",sh.firstAt>=0&&!sh.last.broken&&sh.last.strain<sh.first.strain*1.6f+.05f,"first "+fixed(sh.first.strain)+" at "+fixed(sh.firstAt)+" s, after 3 s "+fixed(sh.last.strain));}
   // Dropped onto a glass floor (1 m up): a crate from the ceiling breaks it and falls through; a mug does not.
   {Game probe(paneRoom({false,true}));const float top=probe.world().clearanceAbove(8.5f,8.5f,1.1f,true);(void)top;
    auto crate=paneHit(model::wooden_crate,{8.5f,8.5f},2.35f,{0,0},0,{false,true},60,1.5f,false),mug=paneHit(model::mug,{8.5f,8.5f},2.8f,{0,0},0,{false,true},60,1.5f,false);
    r.metric("dropped_crate_floor_strain",crate.strain);r.metric("dropped_mug_floor_strain",mug.strain);
    r.check("a crate dropped onto a glass floor breaks it and falls through; a mug dropped does not break it",crate.broken&&crate.through&&!mug.broken,"crate "+fixed(crate.strain)+" mug "+fixed(mug.strain));}
  }else{
   const auto& list=LabAccess::clutter(g);int chair=-1;for(size_t i=0;i<list.size();++i)if(list[i].model==model::wooden_chair)chair=int(i);
   r.check("scene: the thrown crate knocks the chair over",chair>=0&&toppled(list[size_t(chair)]),chair>=0?"tilt "+fixed(tilt(list[size_t(chair)]))+" at "+fixed(list[size_t(chair)].pos.x)+","+fixed(list[size_t(chair)].pos.y):"no chair");
   struct Case {const char* name;int striker,target;float speed;bool shove;float up;bool wantTopple,wantMove;float yaw=0;bool tumble=false;};
   for(const auto& k:{Case{"crate_thrown_wooden_chair",model::wooden_crate,model::wooden_chair,throwSpeed(model::wooden_crate),false,1.f,true,true},
                      Case{"crate_thrown_office_chair",model::wooden_crate,model::office_chair,throwSpeed(model::wooden_crate),false,1.f,true,true},
                      Case{"crate_shoved_wooden_chair",model::wooden_crate,model::wooden_chair,0,true,0,false,true},
                      Case{"crate_shoved_office_chair",model::wooden_crate,model::office_chair,0,true,0,false,true},
                      Case{"barrel_sliding_wooden_chair",model::ind_barrel,model::wooden_chair,4.5f,false,0,false,true},
                      Case{"mug_thrown_office_chair",model::mug,model::office_chair,9.f,false,.5f,false,false},
                      Case{"crate_thrown_tumbling_side_wooden_chair",model::wooden_crate,model::wooden_chair,throwSpeed(model::wooden_crate),false,1.2f,true,true,-kPi*.5f,true},
                      Case{"crate_thrown_side_wooden_chair",model::wooden_crate,model::wooden_chair,throwSpeed(model::wooden_crate),false,1.f,true,true,-kPi*.5f,false},
                      Case{"crate_thrown_tumbling_wooden_chair",model::wooden_crate,model::wooden_chair,throwSpeed(model::wooden_crate),false,1.2f,true,true,0,true}}){
    auto o=knock(k.striker,k.target,k.speed,k.shove,k.up,k.yaw,k.tumble);const std::string n=k.name;
    r.metric(n+"_moved",o.moved,"m");r.metric(n+"_tilt_max",o.tiltMax,"rad");r.metric(n+"_tilt_end",o.tilt,"rad");r.metric(n+"_peak",o.peak,"m/s");
    const std::string d="moved "+fixed(o.moved)+" m, tilt max "+fixed(o.tiltMax)+" end "+fixed(o.tilt)+", peak "+fixed(o.peak)+" m/s (striker "+fixed(o.strikerPeak)+")";
    const bool sane=o.peak<=std::max(o.strikerPeak,3.f)*1.6f+.5f;
    if(k.wantTopple)r.check(n+": knocked over, with an impact sound, and not flung end over end",o.toppled&&sane&&o.sound&&o.tiltMax<2.4f,d);
    else if(k.wantMove)r.check(n+": pushed aside",o.moved>.15f&&sane,d);
    else r.check(n+": hardly stirred",o.moved<.08f&&!o.toppled,d);
    if(k.shove&&k.target==model::office_chair)r.check(n+": a shoved crate does not bowl the heavy chair over",!o.toppled,d);}
   auto rest=stack(false),woken=stack(true);r.metric("stack_drift_mm",rest.drift*1000,"mm");r.metric("stack_turn",rest.turn,"rad");r.metric("woken_stack_drift_mm",woken.drift*1000,"mm");r.metric("woken_stack_awake",woken.awake);
   r.check("a resting stack stays put over 10 s",rest.drift<.001f&&rest.turn<.001f&&rest.awake==0,"drift "+fixed(rest.drift*1000)+" mm, turn "+fixed(rest.turn,4)+", awake "+std::to_string(rest.awake));
   r.check("the same stack woken settles where it stands and sleeps",woken.drift<.03f&&woken.turn<.05f&&woken.awake==0,"drift "+fixed(woken.drift*1000)+" mm, turn "+fixed(woken.turn,4)+", awake "+std::to_string(woken.awake));
  }},
 .reporter=[](Scene& s,Overlay& o){const World& w=s.g().world();for(int i=0;i<int(w.breakables().size());++i){const auto& b=w.breakables()[size_t(i)];
  o.box({b.x1,b.y1,b.bottom},{b.x2,b.y2,b.top},w.breakableIntact(i)?0x40c0ff:0xff4040);if(w.breakableIntact(i))o.label({(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,b.top+.1f},"damage "+fixed(w.breakableDamage(i)));}
  for(const auto& c:LabAccess::clutter(s.g()))if(c.model>=0&&!c.broken)o.label({c.pos.x,c.pos.y,c.z+c.height()+.1f},"tilt "+fixed(tilt(c)));}});
}
}
