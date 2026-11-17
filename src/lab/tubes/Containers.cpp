// Props inside props (the testers: "a basket of fruit, bottles in a crate, files in a box, junk in a bin").
//
// containers: the numbers, each a fresh plain room stepped as play steps it (Game::update):
//  - a bottle dropped over a crate rests on its inner floor; a pumpkin lobbed into a basket and a
//    bottle thrown into a crate land inside; a bottle let go from the hands over a crate drops in;
//  - a basket of pumpkins picked up, carried, turned and set down keeps them in it, in place;
//    a crate of bottles shoved by a sprint takes them along; a crash cart pushed carries its tray;
//  - tipped past its rim it spills; broken (shot) or burst in the hands (Game::shieldBlow) it drops them;
//  - a save keeps them in it; props authored in a container (at its floor, at -999, on its inner
//    floor) seed on its inner floor, asleep, and a filled room left 10 s stays put.
// Its scene (fixture containers-room) films the Starter Map's three: a basket of fruit, a crate of
// bottles and a bin of junk; the crate is tipped over: shots "before" and "after".
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/Containers.h"
#include "game/EngineRegressionScene.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
constexpr float BottleScale=.75f,BasketScale=.55f,PumpkinScale=.28f;
float scaleOf(int model){return model==model::glass_bottle?BottleScale:model==model::market_basket?BasketScale:model==model::market_pumpkin?PumpkinScale:
 model==model::prop_cardboard_box_open||model==model::office_file_binder||model==model::office_paper_stack?.7f:1.f;}
Clutter make(int model,Vec2 at,float z,float yaw=0){Clutter c;c.kind=7;c.model=model;const auto& s=model::propSize[model];const float k=scaleOf(model);c.dims={s[0]*k,s[1]*k,s[2]*k};c.pos=at;c.z=z;c.yaw=yaw;return c;}
void author(AuthoredMapData& m,int model,float x,float y,float z,float yaw=0){const float k=scaleOf(model);kit::prop(m,model,x,y,z,yaw,k);}
// The Starter Map's three, about (x, y): a basket of fruit, a crate of bottles, a bin of junk.
void fill(AuthoredMapData& m,Vec2 at,float sunk=0){
 const float bf=.40f*BasketScale,cf=.02f;
 author(m,model::market_basket,at.x,at.y,0,.2f);
 const Vec2 fruit[]={{-.07f,-.08f},{.07f,-.07f},{-.06f,.08f},{.07f,.08f}};for(auto o:fruit)author(m,model::market_pumpkin,at.x+o.x,at.y+o.y,bf,o.x*9);
 author(m,model::market_crate,at.x+1.1f,at.y,0,kPi*.5f);
 for(int i=0;i<6;++i){const float ox=-.24f+.1f*float(i%3)*2.4f,oy=i<3?-.09f:.09f;author(m,model::glass_bottle,at.x+1.1f+ox,at.y+oy,sunk>0?0.f:cf,float(i));}
 // Files in a box, behind them: a paper stack on its floor and two binders stood on it.
 {const Vec2 b{at.x-.05f,at.y+.95f};const float floor=.01f*.7f;author(m,model::prop_cardboard_box_open,b.x,b.y,0,0);author(m,model::office_paper_stack,b.x,b.y,floor,0);
  for(float x:{-.07f,.07f})author(m,model::office_file_binder,b.x+x,b.y,floor+.021f,0);}
 author(m,model::trash_can,at.x+2.1f,at.y,0,0);
 author(m,model::mug,at.x+2.06f,at.y-.05f,.03f,.5f);author(m,model::med_pill_bottle,at.x+2.14f,at.y+.06f,.03f,1.f);author(m,model::item_bandage,at.x+2.1f,at.y+.07f,-999,2.f);
}
int countIn(const Game& g,int box,int model){int n=0;const auto& list=g.clutter();for(const auto& v:list)if(v.model==model&&containers::holds(list[size_t(box)],v))++n;return n;}
int find(const Game& g,int model,int nth=0){const auto& list=g.clutter();for(int i=0;i<int(list.size());++i)if(list[size_t(i)].model==model&&nth--==0)return i;return -1;}
std::vector<Point3> localOf(const Game& g,int box,int model){std::vector<Point3> out;const auto& list=g.clutter();const auto& b=list[size_t(box)];const float c=std::cos(b.yaw),s=std::sin(b.yaw);
 for(const auto& v:list)if(v.model==model){const Vec2 d=v.pos-b.pos;out.push_back({d.x*c+d.y*s,-d.x*s+d.y*c,v.z-b.z});}return out;}
float drift(const std::vector<Point3>& a,const std::vector<Point3>& b){float w=0;for(size_t i=0;i<a.size()&&i<b.size();++i){const Point3 d=a[i]-b[i];w=std::max(w,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));}return w;}
void run(Game& g,float seconds,InputState in={},float hz=60){for(int i=0;i<int(seconds*hz);++i)g.update(in,1.f/hz);}
std::shared_ptr<CustomCampaign> filledRoom(Vec2 at,float sunk=0,Vec2 start={3,3}){auto m=regression::room();m->definition.playerStart=start;fill(*m,at,sunk);return regression::campaign(m);}
bool allAsleep(const Game& g){for(const auto& v:g.clutter())if(v.model>=0&&!v.broken&&!v.sleeping)return false;return true;}

const bool tube=add({
 .name="containers",.summary="props in open containers rest on the inner floor, ride along when it is carried, shoved or thrown, spill when it tips or breaks, and keep through saves and authoring",
 .fixtures={"containers-room","containers-starter"},
 .author=[](Scene&,AuthoredMapData& m){fill(m,{9.f,8.f});},
 .step=[](Scene& s,float){if(s.frame!=40||s.fixture->value("containers","room")!="room")return;auto& list=LabAccess::clutter(s.g());const int crate=find(s.g(),model::market_crate);if(crate<0)return;auto& c=list[size_t(crate)];c.rollSpeed=-7;c.velocity={0,.6f};c.vz=1.2f;c.sleeping=false;c.restTime=0;c.tipTime=1.5f;},
 .capture=[](Scene& s,const Frame&){if(s.frame==39)s.shoot("before");if(s.last())s.shoot("after");},
 .metrics=[](Scene& s,Report& r){Game& scene=s.g();
  if(s.fixture->value("containers","room")!="room"){
   // The Starter Map's exhibit (PROPS, map 5): seeded in place, asleep, nothing spilled.
   int crate=-1,basket=-1,bin=-1;const auto& list=scene.clutter();
   for(int i=0;i<int(list.size());++i){const auto& v=list[size_t(i)];const bool little=v.dims[0]<.5f;if(v.model==model::market_crate&&countIn(scene,i,model::glass_bottle)>0)crate=i;if(v.model==model::market_basket&&little)basket=i;if(v.model==model::trash_can&&countIn(scene,i,model::mug)>0)bin=i;}
   r.check("starter map: the basket holds 4 fruit, the crate 6 bottles, the bin its junk, all asleep",basket>=0&&crate>=0&&bin>=0&&countIn(scene,basket,model::market_pumpkin)==4&&countIn(scene,crate,model::glass_bottle)==6&&countIn(scene,bin,model::med_pill_bottle)==1,
    "basket "+std::to_string(basket)+" crate "+std::to_string(crate)+" bin "+std::to_string(bin));
   return;}
  {const int crate=find(scene,model::market_crate);int out=0;for(const auto& v:scene.clutter())if(v.model==model::glass_bottle&&!containers::holds(scene.clutter()[size_t(crate)],v))++out;
   r.metric("scene_bottles_spilled",out);r.check("scene: the tipped crate spills its bottles",out>=3,std::to_string(out)+" of 6 out");
   const int basket=find(scene,model::market_basket),bin=find(scene,model::trash_can);
   const int box=find(scene,model::prop_cardboard_box_open);
   r.check("scene: the basket keeps its fruit, the bin its junk and the box its files",countIn(scene,basket,model::market_pumpkin)==4&&countIn(scene,bin,model::mug)+countIn(scene,bin,model::med_pill_bottle)+countIn(scene,bin,model::item_bandage)==3&&
    countIn(scene,box,model::office_paper_stack)==1&&countIn(scene,box,model::office_file_binder)==2);}
  // 1. Dropped in: rests on the inner floor.
  for(float hz:{60.f,144.f}){Game g(regression::campaign(regression::room()));LabAccess::player(g).pos={3,3};auto crate=make(model::market_crate,{10,8},0);crate.sleeping=true;auto b=make(model::glass_bottle,{10.05f,8.1f},.7f);
   LabAccess::clutter(g)={crate,b};run(g,3,{},hz);const auto& v=g.clutter()[1];const float above=v.z-containers::inner(g.clutter()[0]).floor;
   r.metric("drop_rest_above_floor_"+fixed(hz,0)+"hz",above,"m");
   r.check("a bottle dropped over a crate rests on its inner floor, held, asleep at "+fixed(hz,0)+" Hz",containers::holds(g.clutter()[0],v)&&std::fabs(above)<.006f&&v.sleeping&&!v.broken&&std::fabs(g.clutter()[0].pos.x-10)<.01f,"z above floor "+fixed(above));}
  // 2. Thrown in: a pumpkin lobbed into a basket and a bottle into a crate.
  for(int boxModel:{model::market_basket,model::market_crate}){Game g(regression::campaign(regression::room()));LabAccess::player(g).pos={3,3};auto box=make(boxModel,{10,8},0);box.sleeping=true;
   const int itemModel=boxModel==model::market_basket?model::market_pumpkin:model::glass_bottle;auto item=make(itemModel,{8.8f,8},.5f);
   // An underhand lob that clears the near rim: up at 2.2 m/s, across to come down on the middle.
   const float floor=containers::inner(box).floor+.02f,vz=2.2f,t=(vz+std::sqrt(vz*vz+4*7*(.5f-floor)))/14.f;item.velocity={1.2f/t,0};item.vz=vz;item.projectile=true;item.spin=2;item.sleeping=false;
   {Game bare(regression::campaign(regression::room()));LabAccess::player(bare).pos={3,3};auto away=box;away.pos={20,20};LabAccess::clutter(bare)={away,item};run(bare,3);const auto& u=bare.clutter()[1];
    r.check("the same throw on the open floor settles and sleeps (no endless rocking)",u.sleeping,"z "+fixed(u.z)+" turn "+fixed(std::fabs(u.spin)+std::fabs(u.pitchSpeed)+std::fabs(u.rollSpeed))+" pitch "+fixed(u.pitch)+" roll "+fixed(u.roll)+" at "+fixed(u.pos.x));}
   LabAccess::clutter(g)={box,item};run(g,3);const auto& v=g.clutter()[1];const bool in=containers::holds(g.clutter()[0],v);
   r.metric(std::string(boxModel==model::market_basket?"lob_basket":"lob_crate")+"_rest_z",v.z,"m");
   r.check(boxModel==model::market_basket?"a pumpkin lobbed at the basket lands in it":"a bottle thrown at the crate lands in it",in&&v.sleeping,"at "+fixed(v.pos.x)+","+fixed(v.pos.y)+" z "+fixed(v.z)+" box moved "+fixed(length(g.clutter()[0].pos-Vec2{10,8}))+" in "+std::to_string(in)+" asleep "+std::to_string(v.sleeping)+" v "+fixed(length(v.velocity))+" vz "+fixed(v.vz)+" turn "+fixed(std::fabs(v.spin)+std::fabs(v.pitchSpeed)+std::fabs(v.rollSpeed))+" pitch "+fixed(v.pitch)+" roll "+fixed(v.roll)+" rest "+fixed(v.restTime));}
  // 3. Let go from the hands over a crate.
  {Game g(regression::campaign(regression::room()));auto& p=LabAccess::player(g);p.pos={8.6f,8};p.angle=0;p.pitch=-121;auto crate=make(model::market_crate,{9.19f,8},0,kPi*.5f);crate.sleeping=true;auto b=make(model::glass_bottle,{9.f,8},.95f);
   LabAccess::clutter(g)={crate,b};const bool grabbed=LabAccess::grab(g)&&LabAccess::held(g)==1;run(g,.4f);InputState let;let.use=true;g.update(let,1.f/60);run(g,3);const auto& v=g.clutter()[1];
   r.check("a bottle let go from the hands over a crate drops into it",grabbed&&containers::holds(g.clutter()[0],v)&&v.sleeping&&!v.broken,std::string("grabbed ")+(grabbed?"yes":"no")+" z "+fixed(v.z)+" at "+fixed(v.pos.x));}
  // 4. Carried: the basket picked up (the look is not into it, so the hand takes the basket), walked, turned, set down.
  {Game g(filledRoom({10,8},0,{8.95f,8}));auto& p=LabAccess::player(g);p.angle=0;p.pitch=0;const int basket=find(g,model::market_basket);run(g,.2f);const auto before=localOf(g,basket,model::market_pumpkin);
   const bool picked=LabAccess::grab(g)&&LabAccess::held(g)==basket;float worst=0,lowest=99;int insideAll=4;
   InputState walk;walk.forward=true;for(int i=0;i<90;++i){g.update(walk,1.f/60);worst=std::max(worst,drift(before,localOf(g,basket,model::market_pumpkin)));insideAll=std::min(insideAll,countIn(g,basket,model::market_pumpkin));lowest=std::min(lowest,g.clutter()[size_t(basket)].z);}
   InputState turn;turn.mouseDx=6;for(int i=0;i<60;++i){g.update(turn,1.f/60);insideAll=std::min(insideAll,countIn(g,basket,model::market_pumpkin));}
   const Vec2 carried=g.clutter()[size_t(basket)].pos;InputState put;put.use=true;g.update(put,1.f/60);run(g,3);
   const int after=countIn(g,basket,model::market_pumpkin);const float moved=length(g.clutter()[size_t(basket)].pos-Vec2{10,8});
   r.metric("carry_drift_m",worst,"m");r.metric("carry_basket_moved_m",moved,"m");
   r.check("a basket of fruit picked up, walked 1.5 s, turned and set down keeps all four in it, in place",picked&&insideAll==4&&after==4&&worst<.02f&&moved>.8f,
    std::string("picked ")+(picked?"basket":"other/none")+" in "+std::to_string(insideAll)+"/"+std::to_string(after)+" drift "+fixed(worst)+" moved "+fixed(moved)+" carried at "+fixed(carried.x)+","+fixed(carried.y));}
  // 4b. Looking into the opening takes a pumpkin out instead.
  {Game g(filledRoom({10,8},0,{9.35f,8}));auto& p=LabAccess::player(g);p.angle=0;p.pitch=-144;run(g,.2f);const bool picked=LabAccess::grab(g);const int h=LabAccess::held(g);
   r.check("looking down into the basket the hand takes a pumpkin out of it",picked&&h>=0&&g.clutter()[size_t(h)].model==model::market_pumpkin);}
  // 5. Shoved: a sprint into the crate of bottles takes them along.
  {Game g(filledRoom({10,8},0,{11.1f,6.9f}));auto& p=LabAccess::player(g);p.angle=kPi*.5f;p.pitch=0;const int crate=find(g,model::market_crate);run(g,.2f);const Vec2 from=g.clutter()[size_t(crate)].pos;
   InputState shove;shove.forward=true;shove.sprint=true;int least=6;for(int i=0;i<90;++i){p.angle=kPi*.5f;g.update(shove,1.f/60);least=std::min(least,countIn(g,crate,model::glass_bottle));}run(g,2);
   const float moved=length(g.clutter()[size_t(crate)].pos-from);const int in=countIn(g,crate,model::glass_bottle);r.metric("shove_crate_moved_m",moved,"m");
   r.check("a crate of bottles shoved at a sprint takes all six along",moved>.3f&&least==6&&in==6,"moved "+fixed(moved)+" least in "+std::to_string(least)+" end "+std::to_string(in));}
  // 5b. The crash cart's tray: pushed, what is on it goes with it.
  {auto m=regression::room();m->definition.playerStart={10,6.9f};kit::prop(*m,model::med_crash_cart,10,8,0,0);const float tray=.809f;kit::prop(*m,model::med_pill_bottle,10.1f,8,tray,0);kit::prop(*m,model::med_specimen_jar,9.9f,8.05f,tray,0);
   Game g(regression::campaign(m));auto& p=LabAccess::player(g);p.angle=kPi*.5f;InputState push;push.forward=true;const Vec2 from=g.clutter()[0].pos;for(int i=0;i<120;++i){p.angle=kPi*.5f;g.update(push,1.f/60);}run(g,2);
   const float moved=length(g.clutter()[0].pos-from);r.metric("cart_moved_m",moved,"m");
   r.check("a crash cart pushed carries what is on its tray",moved>.2f&&containers::holds(g.clutter()[0],g.clutter()[1])&&containers::holds(g.clutter()[0],g.clutter()[2])&&!g.clutter()[2].broken,"moved "+fixed(moved));}
  // 5c. The cart yanked away (engine.physics-stages: a blow and 4 m/s at once): the tray's lip holds nothing, the load falls.
  {auto m=regression::room();kit::prop(*m,model::med_crash_cart,8,8,0,.74f,.48f,1,0);kit::prop(*m,model::med_pill_bottle,8,8,.809f,.07f,.03f,.13f,0);Game cart(regression::campaign(m));LabAccess::player(cart).pos={3,3};
   const bool rests=cart.clutter()[1].sleeping&&std::fabs(cart.clutter()[1].z-.809f)<.001f;LabAccess::clutter(cart)[0].velocity={4,0};LabAccess::clutter(cart)[0].sleeping=false;LabAccess::clutter(cart)[1].sleeping=false;
   std::string trace;for(int t=0;t<180;++t){LabAccess::stepProps(cart,1.f/120);if(t%30==0){const auto& a=cart.clutter()[0];const auto& b=cart.clutter()[1];trace+=" t"+std::to_string(t)+" cart "+fixed(a.pos.x)+" v "+fixed(a.velocity.x)+" up "+fixed(a.rotate(0,0,1)[2])+" pill "+fixed(b.pos.x)+","+fixed(b.z)+" v "+fixed(b.velocity.x)+(b.riding?" R":"")+(b.sleeping?" S":"");}}
   const auto& v=cart.clutter()[1];r.check("a cart yanked from under its tray load leaves it to fall",rests&&(v.broken||v.z<.75f),trace);}
  // 6. Tipped past its rim: they pour out.
  {Game g(filledRoom({10,8}));const int crate=find(g,model::market_crate);run(g,.2f);auto& c=LabAccess::clutter(g)[size_t(crate)];c.rollSpeed=-7;c.velocity={0,.6f};c.vz=1.2f;c.sleeping=false;c.tipTime=1.5f;
   run(g,4);int out=0;for(const auto& v:g.clutter())if(v.model==model::glass_bottle&&!containers::holds(g.clutter()[size_t(crate)],v))++out;
   r.metric("tip_bottles_out",out);r.check("a crate tipped over spills its bottles",out>=4,std::to_string(out)+" of 6 out, crate up "+fixed(g.clutter()[size_t(crate)].rotate(0,0,1)[2]));}
  // 7. Broken (a shot, a blow): what it held falls to the floor.
  {Game g(filledRoom({10,8}));run(g,.2f);const int crate=find(g,model::market_crate);LabAccess::breakClutter(g,size_t(crate),{1,0,0});run(g,3);bool down=true;float highest=0;
   for(const auto& v:g.clutter())if(v.model==model::glass_bottle&&!v.broken){down&=v.z<.03f&&v.sleeping;highest=std::max(highest,v.z);}
   r.check("a crate broken drops its bottles to the floor, none left hanging",g.clutter()[size_t(crate)].broken&&down,"highest "+fixed(highest));}
  // 8. Burst in the hands by a blow (the shield): the bottles fall out of the air.
  {Game g(regression::campaign(regression::room()));auto& p=LabAccess::player(g);p.pos={10,6.95f};p.angle=kPi*.5f;p.pitch=0;const int crate=0;
   // The shield rule wants a prop at least 25 cm deep: this crate is the market one at 1.15 times its size.
   auto box=make(model::market_crate,{10,8},0);for(auto& d:box.dims)d*=1.15f;box.sleeping=true;std::vector<Clutter> list{box};
   for(int i=0;i<6;++i){auto b=make(model::glass_bottle,{10-.12f+.12f*float(i%3),8+(i<3?-.18f:.18f)},containers::inner(box).floor);b.sleeping=true;list.push_back(b);}
   LabAccess::clutter(g)=list;run(g,.2f);
   const bool picked=LabAccess::grab(g)&&LabAccess::held(g)==crate;run(g,.5f);const int carried=countIn(g,crate,model::glass_bottle);const float heldZ=g.clutter()[size_t(crate)].z;
   const bool blocked=LabAccess::shieldBlow(g,40,p.pos+Vec2{0,2});run(g,3);bool fell=true;for(const auto& v:g.clutter())if(v.model==model::glass_bottle&&!v.broken)fell&=v.z<.05f;
   r.check("a crate of bottles held up as a shield bursts under a blow and its bottles fall out of the air",picked&&carried==6&&heldZ>.5f&&blocked&&g.clutter()[size_t(crate)].broken&&fell,
    std::string("picked ")+(picked?"crate":"other")+" carried "+std::to_string(carried)+" at z "+fixed(heldZ));}
  // 9. Saved and loaded: still in it.
  {auto campaign=filledRoom({10,8},0,{11.1f,6.9f});Game g(campaign);auto& p=LabAccess::player(g);p.angle=kPi*.5f;InputState shove;shove.forward=true;for(int i=0;i<40;++i){p.angle=kPi*.5f;g.update(shove,1.f/60);}run(g,3);
   const auto bytes=LabAccess::encodeSave(g);Game loaded(campaign);const bool decoded=LabAccess::decodeSave(loaded,bytes);const int crate=find(loaded,model::market_crate),basket=find(loaded,model::market_basket);
   const bool same=decoded&&LabAccess::encodeSave(loaded)==bytes&&countIn(loaded,crate,model::glass_bottle)==6&&countIn(loaded,basket,model::market_pumpkin)==4;run(loaded,2);
   r.check("a save keeps the bottles in the moved crate and the fruit in the basket, byte for byte",same&&countIn(loaded,crate,model::glass_bottle)==6&&allAsleep(loaded),"moved crate at "+fixed(loaded.clutter()[size_t(crate)].pos.x)+","+fixed(loaded.clutter()[size_t(crate)].pos.y));}
  // 10. Authored: in the container at its floor (sunk), at -999 and on its inner floor, all seed inside, asleep.
  for(float sunk:{0.f,1.f}){Game g(filledRoom({10,8},sunk));const int crate=find(g,model::market_crate),basket=find(g,model::market_basket),bin=find(g,model::trash_can);
   const float floor=g.clutter()[size_t(crate)].z+containers::inner(g.clutter()[size_t(crate)]).floor;bool level=true;for(const auto& v:g.clutter())if(v.model==model::glass_bottle)level&=std::fabs(v.z-floor)<.002f;
   const int junk=countIn(g,bin,model::mug)+countIn(g,bin,model::med_pill_bottle)+countIn(g,bin,model::item_bandage);
   r.check(std::string("authored ")+(sunk>0?"at the floor under it":"on the inner floor")+": bottles in the crate, fruit in the basket, junk in the bin, seeded asleep",
    countIn(g,crate,model::glass_bottle)==6&&countIn(g,basket,model::market_pumpkin)==4&&junk==3&&level&&allAsleep(g),"junk "+std::to_string(junk));}
  // 11. Left alone: nothing creeps, nothing wakes.
  {Game g(filledRoom({10,8}));const auto start=g.clutter();run(g,10);float worst=0;for(size_t i=0;i<start.size();++i)worst=std::max(worst,length(g.clutter()[i].pos-start[i].pos)+std::fabs(g.clutter()[i].z-start[i].z));
   r.metric("rest_drift_10s",worst,"m");r.check("a room of filled containers left 10 s does not creep or wake",worst<.001f&&allAsleep(g),fixed(worst,4));}
 }});
}
}
