// Glass that breaks a piece at a time, as Half-Life 2's and Alyx's panes do (the testers:
// "glass physics like Half-Life 2 and Half-Life: Alyx"). Game::damagePane hands each blow on
// glass to GlassPanes.cpp; the pane's state is its hits (GlassFracture.h).
//
// glass: the numbers, each from a fresh plain room with one pane, stepped as play steps it:
//  - rounds: each handgun round's own hole, how many close rounds before pieces fall out as
//    shard props, that a round through a hole meets no glass and one beside it does;
//  - size: shotgun rounds to bring down a 0.56 m2 lite, a 1.74 m2 glazing bay and a 6 x 3 m pane;
//  - shards: what falls, that it lands, breaks again landing hard and lies still;
//  - props: a thrown crate punches a hole its size and goes through, a mug thrown through that
//    hole passes untouched, a mug at whole glass bounces;
//  - fists, bodies (a dive through, a sprinting creature; walking into it does nothing);
//  - holds glass: blows to open it at one spot, rounds through only there, never down;
//  - feet on glass: the crunch and how far the AI hears it walking, sprinting, crouched;
//  - saves: how many bytes a holed pane adds, and that it comes back exactly.
// Its scenes (fixtures glass-pane, glass-holds) film rounds holing a pane until pieces fall and a
// thrown crate going through it, and holds glass spiderwebbing and sagging: shots "before",
// "holes" and "after".
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
struct Pane {float x1,y1,x2,y2,lo,hi;bool holds=false;};
const Pane Big{9.97f,5,10.03f,11,0,3},Bay{9.97f,7.4f,10.03f,8.6f,.95f,2.4f},Lite{9.97f,7.6f,10.03f,8.4f,.9f,1.6f};
std::shared_ptr<CustomCampaign> room(Pane p){auto m=regression::room();m->definition.playerStart={3,3};kit::breakable(*m,p.x1,p.y1,p.x2,p.y2,p.lo,p.hi,17,p.holds);return regression::campaign(m);}
Clutter make(int model,Vec2 at,float z){Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;c.pos=at;c.z=z;return c;}
void thrown(Clutter& c,Vec2 dir,float speed,float up=1){c.velocity=dir*speed;c.vz=up;c.spin=3;c.pitchSpeed=6;c.rollSpeed=3;c.projectile=true;c.sleeping=false;c.restTime=0;}
const World::PaneGlass* glassOf(Game& g){return g.world().paneGlass(0);}
int missing(Game& g){const auto* p=glassOf(g);return p?p->missing:0;}
int hits(Game& g){const auto* p=glassOf(g);return p?int(p->hits.size()):0;}
struct Shards {int props=0,broken=0,awake=0;float highest=-9;};
Shards shards(Game& g){Shards s;for(const auto& c:LabAccess::clutter(g))if(c.kind==6&&c.shardOf==ShardOf::Glass){if(c.broken)++s.broken;else ++s.props;s.awake+=!c.sleeping;s.highest=std::max(s.highest,c.z);}return s;}
void run(Game& g,float seconds){for(int i=0;i<int(seconds*60);++i)g.update({},1.f/60);}
const HitscanDef& handgun(){return playerWeaponDef(WeaponId::Handgun).hitscan;}
const HitscanDef& shotgun(){return playerWeaponDef(WeaponId::Shotgun).hitscan;}
// Rounds from (8, y, z) straight at the pane until it comes down, walking a grid over it.
int roundsToBring(Pane p,const HitscanDef& gun,float step){Game g(room(p));int rounds=0;
 for(float z=p.lo+step*.5f;z<p.hi&&g.world().breakableIntact(0);z+=step)for(float y=p.y1+step*.5f;y<p.y2&&g.world().breakableIntact(0);y+=step){LabAccess::fireFrom(g,{8,y,z},{10,y,z},gun);++rounds;}
 if(g.world().breakableIntact(0))for(int k=0;k<200&&g.world().breakableIntact(0);++k){const float y=p.y1+(p.y2-p.y1)*glass::hashUnit(k,1),z=p.lo+(p.hi-p.lo)*glass::hashUnit(k,2);LabAccess::fireFrom(g,{8,y,z},{10,y,z},gun);++rounds;}
 return g.world().breakableIntact(0)?-1:rounds;}

// The rounds' dust off the far wall hangs about three seconds: the "holes" shot waits it out.
constexpr int Volley=12,VolleyEvery=5,Throw=230,HoldsMore=260;
const bool tube=add({
 .name="glass",.summary="glass breaks a piece at a time: rounds hole it, close holes let pieces fall as shard props, thrown props punch holes their size, bodies and fists break it, holds glass spiderwebs and sags, glass crunches underfoot, saves keep the holes",
 .friends={"heavy-impacts","debris"},.fixtures={"glass-pane","glass-holds"},
 .author=[](Scene& s,AuthoredMapData& m){const bool holds=s.fixture->value("glass","pane")=="holds";kit::breakable(m,9.97f,6.4f,10.03f,9.6f,0,2.4f,17,holds);if(!holds)kit::prop(m,model::wooden_crate,8.4f,8.9f,0,0);},
 .step=[](Scene& s,float){Game& g=s.g();const bool holds=s.fixture->value("glass","pane")=="holds";const int k=(s.frame-Volley)/VolleyEvery;
  if(s.frame>=Volley&&(s.frame-Volley)%VolleyEvery==0){
   if(holds&&k<12)LabAccess::fireFrom(g,{7.6f,8,1.3f},{10,7.7f,1.3f},shotgun());
   if(!holds&&k<9){const float a=float(k)*2.1f,r=.05f+.045f*float(k);LabAccess::fireFrom(g,{7.6f,7.6f,1.4f},{10,7.4f+std::cos(a)*r,1.45f+std::sin(a)*r},handgun());}}
  // Holds glass: four more rounds round the hole once the first twelve's smoke has cleared.
  if(holds&&s.frame>=HoldsMore&&(s.frame-HoldsMore)%VolleyEvery==0&&(s.frame-HoldsMore)/VolleyEvery<4){const float a=float((s.frame-HoldsMore)/VolleyEvery)*1.7f;LabAccess::fireFrom(g,{7.6f,8,1.3f},{10,7.7f+std::cos(a)*.6f,1.3f+std::sin(a)*.45f},shotgun());}
  if(!holds&&s.frame==Throw)for(auto& c:LabAccess::clutter(g))if(c.model==model::wooden_crate){c.z=.55f;thrown(c,{1,0},9,1.2f);}},
 .capture=[](Scene& s,const Frame&){const bool holds=s.fixture->value("glass","pane")=="holds";if(s.frame==Volley-1)s.shoot("before");if(s.frame==(holds?HoldsMore-1:Throw-1))s.shoot("holes");if(s.last())s.shoot("after");},
 .metrics=[](Scene& s,Report& r){Game& g=s.g();const bool holds=s.fixture->value("glass","pane")=="holds";
  if(holds){r.metric("scene_holds_hits",hits(g));r.metric("scene_holds_missing",missing(g));r.metric("scene_holds_strain",g.world().breakableDamage(0)/180.f);
   r.check("scene: holds glass spiderwebs, sags and opens only a small hole where it was hammered, and stands",g.world().breakableIntact(0)&&hits(g)>=10&&missing(g)>=1&&missing(g)<40,std::to_string(hits(g))+" hits, "+std::to_string(missing(g))+" pieces gone");return;}
  {const auto sh=shards(g);float crate=0;for(const auto& c:LabAccess::clutter(g))if(c.model==model::wooden_crate)crate=c.pos.x;
   r.metric("scene_missing",missing(g));r.metric("scene_shard_props",sh.props);r.metric("scene_shards_broken_landing",sh.broken);r.metric("scene_crate_x",crate,"m");
   r.check("scene: rounds hole the pane until pieces fall, and the thrown crate goes through it; the pane stands",g.world().breakableIntact(0)&&missing(g)>10&&sh.props+sh.broken>0&&crate>10.2f,std::to_string(missing(g))+" pieces gone, "+std::to_string(sh.props)+" shards, crate at x "+fixed(crate));}
  // ---- rounds ----
  {Game h(room(Big));std::string ladder;int firstPiece=-1;int one=0;
   for(int k=0;k<12;++k){const float a=float(k)*2.1f,rad=.05f+.045f*float(k);LabAccess::fireFrom(h,{8,8,1.5f},{10,8+std::cos(a)*rad,1.5f+std::sin(a)*rad},handgun());LabAccess::landShards(h);
    if(k==0)one=missing(h);const auto sh=shards(h);if(firstPiece<0&&sh.props>0)firstPiece=k+1;ladder+=std::to_string(missing(h))+(k<11?" ":"");}
   r.metric("handgun_round_pieces",one);r.metric("handgun_rounds_to_first_shard",firstPiece);r.metric("handgun_12_rounds_missing",missing(h));r.metric("handgun_12_rounds_shards",shards(h).props);
   r.check("a handgun round makes its own small hole (1 to 6 pieces of about 9 cm)",one>=1&&one<=6,std::to_string(one));
   r.check("close rounds join their holes until pieces fall out as shard props, the pane standing",firstPiece>=2&&firstPiece<=6&&h.world().breakableIntact(0),"pieces gone per round: "+ladder+"; first shard at round "+std::to_string(firstPiece));
   const auto through=LabAccess::trace(h,{8,8,1.5f},{1,0,0},10),beside=LabAccess::trace(h,{8,6,.6f},{1,0,0},10);
   r.check("a round through a hole meets no glass; one beside it meets glass",through.passed[0].kind==0&&beside.passed[0].kind==1);
   // ---- saves ----
   const auto bytes=LabAccess::encodeSave(h);Game fresh(room(Big));const auto plain=LabAccess::encodeSave(fresh);Game back(room(Big));const bool loaded=LabAccess::decodeSave(back,bytes);
   // The pane block alone: its index, its count and the hits, one number each, as the save writes them.
   const auto packed=h.world().packedPaneHits();std::size_t pane=8;for(const auto& hit:packed.front().second)pane+=std::to_string(std::uint64_t(hit.u)<<32|std::uint64_t(hit.v)<<16|std::uint64_t(hit.radius)<<8|hit.strain).size()+1;
   r.metric("save_pane_hits",hits(h));r.metric("save_pane_block_bytes",double(pane),"B");r.metric("save_bytes_over_fresh_room",double(bytes.size())-double(plain.size()),"B");
   r.check("a save keeps the holes compactly and brings them back exactly",loaded&&back.world().paneGlass(0)&&back.world().paneGlass(0)->gone==h.world().paneGlass(0)->gone&&LabAccess::encodeSave(back)==bytes&&pane<20*size_t(hits(h))+16,std::to_string(hits(h))+" hits in "+std::to_string(pane)+" bytes");}
  // ---- size ----
  {const int lite=roundsToBring(Lite,shotgun(),.3f),bay=roundsToBring(Bay,shotgun(),.3f),big=roundsToBring(Big,shotgun(),.4f);
   r.metric("shotgun_rounds_lite_0.56m2",lite);r.metric("shotgun_rounds_bay_1.74m2",bay);r.metric("shotgun_rounds_6x3m",big);
   r.check("a small lite goes at a round; a bay takes a few; a large pane breaks in parts first and takes many",lite==1&&bay>=2&&bay<=6&&big>=12,"lite "+std::to_string(lite)+", bay "+std::to_string(bay)+", 6 x 3 m "+std::to_string(big));}
  // ---- shards ----
  {Game h(room(Bay));LabAccess::player(h).pos={3,3};int rounds=0;for(float z=1.1f;z<2.3f&&h.world().breakableIntact(0);z+=.3f)for(float y=7.55f;y<8.5f&&h.world().breakableIntact(0);y+=.3f){LabAccess::fireFrom(h,{8,y,z},{10,y,z},shotgun());++rounds;}
   LabAccess::landShards(h);const auto fell=shards(h);run(h,3.f);const auto lay=shards(h);
   r.metric("bay_shard_props_spawned",fell.props+fell.broken);r.metric("bay_shards_whole_after_3s",lay.props);r.metric("bay_shards_broke_landing",lay.broken);r.metric("bay_shards_highest_after_3s",lay.highest,"m");r.metric("bay_shards_awake_after_3s",lay.awake);
   r.check("a pane coming down drops shard props that land, some breaking again, and lie still",fell.props>=6&&lay.broken>0&&lay.highest<.05f&&lay.awake==0,std::to_string(fell.props)+" fell, "+std::to_string(lay.broken)+" broke landing, highest "+fixed(lay.highest,3)+" m, "+std::to_string(lay.awake)+" awake");}
  // ---- props ----
  {Game h(room(Big));LabAccess::player(h).pos={3,3};auto crate=make(model::wooden_crate,{8.9f,8},.5f);thrown(crate,{1,0},9,.5f);LabAccess::clutter(h)={crate};run(h,1.2f);LabAccess::landShards(h);
   const float crateX=LabAccess::clutter(h)[0].pos.x,hole=glassOf(h)?glassOf(h)->hits.back().radius:0;const int before=hits(h);
   auto mug=make(model::mug,{9.2f,8},LabAccess::clutter(h)[0].z>0?.9f:.9f);
   // The mug goes at the crate's hole (where the crate struck: its last hit), then at whole glass.
   const auto f=h.world().paneFrame(0);const auto at=f.at({glassOf(h)?glassOf(h)->hits.back().u:3,glassOf(h)?glassOf(h)->hits.back().v:1});
   mug.pos={9.3f,at.y};mug.z=at.z-.05f;thrown(mug,{1,0},9,0);mug.pitchSpeed=mug.rollSpeed=mug.spin=0;auto& list=LabAccess::clutter(h);list.push_back(mug);run(h,.6f);const float mugX=list.back().pos.x;const int after=hits(h);
   Game k(room(Big));auto other=make(model::mug,{9.3f,6},1.2f);thrown(other,{1,0},9,0);LabAccess::clutter(k)={other};run(k,.6f);const float bounce=LabAccess::clutter(k)[0].pos.x;
   r.metric("crate_hole_radius",hole,"m");r.metric("crate_x_after",crateX,"m");r.metric("mug_through_hole_x",mugX,"m");r.metric("mug_at_glass_x",bounce,"m");
   r.check("a thrown crate punches a hole its size and carries on through; the pane stands",h.world().breakableIntact(0)&&hole>=.3f&&crateX>10.2f,"hole "+fixed(hole)+" m, crate at "+fixed(crateX));
   r.check("a mug thrown through that hole passes untouched; one thrown at whole glass bounces",mugX>10.2f&&after==before&&bounce<10,"mug through at "+fixed(mugX)+" (hits "+std::to_string(before)+" -> "+std::to_string(after)+"), at glass "+fixed(bounce));}
  // ---- fists and bodies ----
  {Game h(room(Bay));LabAccess::damagePane(h,0,kUnarmedMelee.paneDamage,{10,8,1.6f},{1,0,0},{PaneBlow::Fist});const int fist=missing(h);
   Game dive(room(Bay));auto& p=LabAccess::player(dive);p.pos={9.75f,8};p.grounded=false;LabAccess::velocity(dive)={6,0};LabAccess::updateGlass(dive,1.f/60);
   Game walk(room(Bay));auto& q=LabAccess::player(walk);q.pos={9.2f,8};q.angle=0;for(int i=0;i<90;++i){InputState in;in.forward=true;in.sprint=true;walk.update(in,1.f/60);}
   Game charge(room(Big));Enemy e;e.kind=CreatureKind::Brute;e.pos={9.95f-e.radius()-.03f,8};e.heading=0;e.moving=true;e.state=Enemy::State::Chase;e.alive=true;LabAccess::enemies(charge)={e};LabAccess::updateGlass(charge,1.f/60);
   Game amble(room(Big));e.state=Enemy::State::Idle;LabAccess::enemies(amble)={e};LabAccess::updateGlass(amble,1.f/60);
   Game laminated(room({9.97f,5,10.03f,11,0,3,true}));auto& l=LabAccess::player(laminated);l.pos={9.75f,8};l.grounded=false;LabAccess::velocity(laminated)={6,0};LabAccess::updateGlass(laminated,1.f/60);
   r.metric("fist_pieces",fist);
   r.check("a fist punches a hole",fist>=2&&h.world().breakableIntact(0),std::to_string(fist)+" pieces");
   r.check("a dive through takes the pane; sprinting into it on foot does not",!dive.world().breakableIntact(0)&&walk.world().breakableIntact(0),"player at "+fixed(q.pos.x));
   r.check("a charging creature crashes through; an ambling one does not",!charge.world().breakableIntact(0)&&amble.world().breakableIntact(0));
   r.check("a body against holds glass spiderwebs it and it stands",laminated.world().breakableIntact(0)&&laminated.world().breakableDamage(0)>0);}
  // ---- a glass floor ----
  {Game h(room({7,7,10,10,1,1.03f}));LabAccess::player(h).pos={3,3};auto crate=make(model::wooden_crate,{8.5f,8.5f},2.35f);crate.sleeping=false;LabAccess::clutter(h)={crate};
   float lowest=9;std::string trail;for(int i=0;i<90;++i){h.update({},1.f/60);const auto& v=LabAccess::clutter(h)[0];lowest=std::min(lowest,v.z);if(i%6==0)trail+=" "+fixed(v.z)+(v.sleeping?"s":"")+"/"+fixed(v.vz,1);}const auto& c=LabAccess::clutter(h)[0];
   if(glassOf(h))for(const auto& hit:glassOf(h)->hits)trail+=" [hit r "+fixed(hit.radius)+" s "+fixed(hit.strain)+" at "+fixed(hit.u)+","+fixed(hit.v)+"]";
   r.metric("floor_crate_z_after",c.z,"m");r.metric("floor_pane_missing",missing(h));
   r.check("a crate dropped on a glass floor punches through it and lands on the floor below",c.z<.1f&&h.world().breakableIntact(0)&&missing(h)>0,"crate z "+fixed(c.z)+" (lowest "+fixed(lowest)+"), "+std::to_string(missing(h))+" pieces gone, "+std::to_string(hits(h))+" hits, intact "+std::to_string(h.world().breakableIntact(0))+trail);}
  // ---- holds ----
  {Game h(room({9.97f,5,10.03f,11,0,3,true}));int blows=0;while(blows<30&&missing(h)==0){LabAccess::fireFrom(h,{8,8,1.2f},{10,8,1.2f},shotgun());++blows;}
   const bool there=LabAccess::trace(h,{8,8,1.2f},{1,0,0},10).kind!=ShotKind::Solid,beside=LabAccess::trace(h,{8,7.7f,1.2f},{1,0,0},10).kind==ShotKind::Solid;
   for(int k=0;k<40;++k)LabAccess::damagePane(h,0,34,{10,5.5f+k*.12f,.3f+k*.06f},{1,0,0},{});
   r.metric("holds_blows_to_open",blows);r.metric("holds_hole_pieces",missing(h));
   r.check("holds glass opens at one spot only once punched far enough there; rounds pass there only; it never comes down",blows>=6&&there&&beside&&h.world().breakableIntact(0),std::to_string(blows)+" blows");}
  // ---- feet ----
  {auto crunch=[](float speed,bool crouched){Game h(room(Big));Clutter c;c.kind=6;c.shardOf=ShardOf::Glass;c.dims={.12f,.1f,.006f};c.pos={6,8};c.sleeping=true;LabAccess::clutter(h)={c};
    auto& p=LabAccess::player(h);p.pos={5.4f,8};p.crouched=crouched;LabAccess::velocity(h)={speed,0};LabAccess::crunchFrom(h)=p.pos;LabAccess::sounds(h).clear();
    for(int i=0;i<30;++i){p.pos.x+=speed/60;LabAccess::updateGlass(h,1.f/60);}float radius=-1;for(const auto& e:LabAccess::sounds(h))if(e.sound==Sound::JunkGlass)radius=std::max(radius,e.aiRadius);return radius;};
   const float walk=crunch(3.6f,false),sprint=crunch(5.4f,false),crouch=crunch(1.6f,true),bare=[]{Game h(room(Big));auto& p=LabAccess::player(h);p.pos={5.4f,8};LabAccess::velocity(h)={3.6f,0};LabAccess::crunchFrom(h)=p.pos;LabAccess::sounds(h).clear();
    for(int i=0;i<30;++i){p.pos.x+=.06f;LabAccess::updateGlass(h,1.f/60);}for(const auto& e:LabAccess::sounds(h))if(e.sound==Sound::JunkGlass)return 1.f;return 0.f;}();
   r.metric("crunch_ai_radius_walk",walk,"m");r.metric("crunch_ai_radius_sprint",sprint,"m");r.metric("crunch_ai_radius_crouch",crouch,"m");
   r.check("glass underfoot crunches: heard further sprinting, hardly crouched; a bare floor is silent",sprint>walk&&walk>crouch&&crouch>0&&bare==0,"walk "+fixed(walk)+" sprint "+fixed(sprint)+" crouch "+fixed(crouch));}
 },
 .reporter=[](Scene& s,Overlay& o){const World& w=s.g().world();for(int i=0;i<int(w.breakables().size());++i){const auto& b=w.breakables()[size_t(i)];
  o.box({b.x1,b.y1,b.bottom},{b.x2,b.y2,b.top},w.breakableIntact(i)?0x40c0ff:0xff4040);if(const auto* p=w.paneGlass(i))o.label({(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,b.top+.1f},std::to_string(p->hits.size())+" hits, "+std::to_string(p->missing)+" gone");}}});
}
}
