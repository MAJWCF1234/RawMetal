// The MAC-11 machine pistol and the spent cases every gun throws (Casings.cpp).
// The scene, first person in the regression room: idle, a held burst (the climb,
// the flash, the brass), the cases lying on the floor, aimed, the tactical reload
// (the magazine leaves and comes back), the knob held back on an empty magazine
// and the slower empty reload, sprinting, the shotgun's pumped shell and the
// handgun's brass, then third person for Claire and the Limitator holding it, and
// its .380 box and the gun itself on the floor. Its checks also run the game on
// its own: the rate of fire at two frame rates, the climb and its return, the
// spread's bloom and settle, both reloads' lengths and sounds, the pickups and
// the HUD, key 4 and the wheel, the hip holster, saves old and new, and every
// gun's cases coming to rest on the floor away from the gun.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
float dist(Point3 a,Point3 b){const Point3 d=a-b;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
const auto& macDef(){return playerWeaponDef(WeaponId::MachinePistol);}
// A Game with nobody about, the MAC-11 drawn and `rounds` in it.
Game macScene(AmmoPool rounds){auto g=Game::validationScene(Enemy::Kind::Huntsman,3);LabAccess::enemies(g).clear();g.grantMachinePistol(0);LabAccess::player(g).pool(WeaponId::MachinePistol)=rounds;g.equipWeapon(WeaponId::MachinePistol);g.update({},1.f/60);return g;}
int heard(const Game& g,Sound s){int n=0;for(const auto& e:g.sounds())n+=e.sound==s;return n;}
// Phases of the scene (frames at 60 Hz).
enum Phase{Idle,Burst,Floor,Aimed,Tactical,Dry,EmptyReload,Sprint,Shotgun,Pistol,OutsideClaire,OutsideLimitator,Pickups,PhaseCount};
constexpr int PhaseStart[PhaseCount+1]={0,30,90,150,210,300,330,450,510,570,630,720,810,840};
constexpr const char* PhaseName[PhaseCount]={"idle","burst","floor","aimed","tactical reload","dry","empty reload","sprint","shotgun","handgun","claire outside","limitator outside","pickups"};
int phaseAt(int frame){for(int p=0;p<PhaseCount;++p)if(frame<PhaseStart[p+1])return p;return PhaseCount-1;}

// ---- the game alone ----------------------------------------------------------
void logic(Report& r){
 const auto& mac=macDef();
 r.check("the MAC-11 is its own kWeapons row: full auto, 32 rounds of .380, after the handgun on the wheel",
  mac.id==WeaponId::MachinePistol&&mac.cycle.fullAuto&&!mac.cycle.semiAuto&&mac.cycle.tubeCapacity==32&&std::string(mac.cycle.caliber)==".380"&&weaponCount()==3&&nextFirearm(WeaponId::Handgun,1)==WeaponId::MachinePistol);
 const auto& pistol=playerWeaponDef(WeaponId::Handgun);
 r.check("each round hits lighter than the handgun's, hard close up and falling away with range",
  mac.hitscan.dmgNear<pistol.hitscan.dmgNear&&mac.hitscan.actorDamage<pistol.hitscan.actorDamage&&mac.hitscan.dmgFar<mac.hitscan.dmgMid&&mac.hitscan.dmgMid<mac.hitscan.dmgNear&&mac.hitscan.dmgFar*2<mac.hitscan.dmgNear,
  "near "+fixed(mac.hitscan.dmgNear,0)+" mid "+fixed(mac.hitscan.dmgMid,0)+" far "+fixed(mac.hitscan.dmgFar,0)+" (handgun "+fixed(pistol.hitscan.dmgNear,0)+")");
 r.check("a Cleanup soldier (90-110) goes down in 8-10 body hits, about half a second of fire",std::ceil(90.f/mac.hitscan.actorDamage)>=8&&std::ceil(110.f/mac.hitscan.actorDamage)<=10);
 // Rate of fire at two frame rates.
 for(int hz:{60,144}){auto g=macScene({200,32});InputState fire{};fire.fire=true;int shots=0;
  for(int i=0;i<hz;++i){g.update(fire,1.f/float(hz));shots+=heard(g,Sound::MacShot);}
  r.metric("rounds_per_second_"+std::to_string(hz)+"hz",shots,"rps");
  r.check("held, it fires 16-20 rounds a second at "+std::to_string(hz)+" Hz",shots>=16&&shots<=20,std::to_string(shots)+" in one second");
  if(hz==60)r.check("a held second spends those rounds from the magazine",g.player().pool(WeaponId::MachinePistol).loaded==32-shots,std::to_string(g.player().pool(WeaponId::MachinePistol).loaded)+" left");}
 // The climb, its return, the bloom and its settle.
 {auto g=macScene({200,32});const float pitch0=g.player().pitch;const float base=g.shotSpread();InputState fire{};fire.fire=true;std::vector<float> spreads;float peak=0;
  for(int i=0;i<36;++i){g.update(fire,1.f/60);spreads.push_back(g.shotSpread());peak=std::max(peak,g.shotSpread());}
  const float climbed=g.player().pitch-pitch0;
  r.metric("climb_after_burst",climbed,"pitch units");r.metric("spread_base",base,"rad");r.metric("spread_peak",peak,"rad");
  r.check("a held burst climbs the view",climbed>8.f,fixed(climbed,1)+" pitch units ("+fixed(climbed/140.f*57.3f,1)+" degrees) in 0.6 s");
  r.check("the spread blooms while held",peak>base*3&&spreads.back()>spreads[2],fixed(base,4)+" to "+fixed(peak,4)+" rad");
  for(int i=0;i<60;++i)g.update({},1.f/60);
  const float back=g.player().pitch-pitch0;
  r.check("let go, the view comes back by most of the climb",back<climbed*.55f&&back>climbed*.2f,fixed(back,1)+" of "+fixed(climbed,1)+" left a second after");
  r.check("and the spread settles back to its base",std::fabs(g.shotSpread()-base)<.002f,fixed(g.shotSpread(),4)+" rad");
  // The rounds really scatter: the shot rays' directions over a long burst.
  auto h=macScene({200,32});float widest=0;for(int i=0;i<40;++i){h.update(fire,1.f/60);}
  const auto& rays=h.shotRays();for(size_t a=0;a+1<rays.size();++a)for(size_t b=a+1;b<rays.size();++b){const Point3 da=rays[a].direction,db=rays[b].direction;widest=std::max(widest,dist(da,db));}
  r.check("the rounds scatter within the cone",widest>.01f,fixed(widest,4)+" widest apart (unit directions)");
  auto still=macScene({200,32});LabAccess::aimBlend(still)=1;const float aimed=still.shotSpread();r.check("down the sights the cone is tighter",aimed<base,fixed(aimed,4)+" against "+fixed(base,4));}
 // Dry: one click a press, not on and on.
 {auto g=macScene({40,0});InputState fire{};fire.fire=true;int clicks=0;for(int i=0;i<60;++i){g.update(fire,1.f/60);clicks+=heard(g,Sound::MacDry);}
  g.update({},1.f/60);g.update(fire,1.f/60);clicks+=heard(g,Sound::MacDry);
  r.check("an empty magazine clicks once a press",clicks==2,std::to_string(clicks)+" clicks for two presses");
  r.check("the knob stays back on an empty magazine",g.slideLocked());}
 // Reloads.
 {auto g=macScene({64,10});InputState rl{};rl.reload=true;g.update(rl,1.f/60);
  r.check("a partly spent magazine reloads in 1.35 s (tactical)",g.reloading()&&!g.emptyReload()&&std::fabs(g.reloadDuration()-1.35f)<.001f);
  std::vector<Sound> s;bool midway=false;for(int i=0;i<100&&g.reloading();++i){g.update({},1.f/60);for(const auto& e:g.sounds())s.push_back(e.sound);if(g.reloadProgress()>.45f&&g.reloadProgress()<.55f)midway|=g.player().pool(WeaponId::MachinePistol).loaded==10;}
  auto has=[&](Sound x){return std::find(s.begin(),s.end(),x)!=s.end();};
  r.check("it seats the magazine at the end: 32 in, mag out and in, no rack",midway&&g.player().pool(WeaponId::MachinePistol)==AmmoPool{64,32}&&has(Sound::MacMagOut)&&has(Sound::MacMagIn)&&!has(Sound::MacRack));
  auto e=macScene({40,0});e.update(rl,1.f/60);
  r.check("an empty magazine takes the longer 1.9 s reload",e.emptyReload()&&std::fabs(e.reloadDuration()-1.9f)<.001f&&e.slideLocked());
  s.clear();bool knobHeld=true;for(int i=0;i<130&&e.reloading();++i){e.update({},1.f/60);for(const auto& x:e.sounds()){s.push_back(x.sound);if(x.sound==Sound::MacRack)knobHeld&=!e.slideLocked();}if(e.reloading()&&e.reloadProgress()<.8f)knobHeld&=e.slideLocked();}
  r.check("it ends with the bolt racked home (rack heard) and 32 in",e.player().pool(WeaponId::MachinePistol)==AmmoPool{40,32}&&has(Sound::MacRack)&&knobHeld&&!e.slideLocked());}
 // Pickups and the HUD.
 {auto g=Game::validationScene(Enemy::Kind::Huntsman,3);LabAccess::enemies(g).clear();
  LabAccess::player(g).pos={3.5f,4.5f};
  auto& pickups=LabAccess::pickups(g);pickups={{{3.5f,4.5f},Pickup::Kind::MacAmmo,true}};LabAccess::updatePickups(g);
  r.check("a .380 box before the gun waits in its pool",!g.hasMachinePistol()&&g.player().pool(WeaponId::MachinePistol)==AmmoPool{32,0}&&g.pickupNotice()=="COLLECTED 32 ROUNDS .380",g.pickupNotice());
  pickups={{{3.5f,4.5f},Pickup::Kind::MachinePistol,true}};LabAccess::updatePickups(g);for(int i=0;i<90;++i)g.update({},1.f/60);
  r.check("the MAC-11 on the floor is taken loaded and drawn",g.hasMachinePistol()&&g.activeFirearm()==WeaponId::MachinePistol&&g.player().pool(WeaponId::MachinePistol)==AmmoPool{64,32}&&g.pickupNotice().rfind("TOOK MAC-11",0)==0,g.pickupNotice());
  auto hud=g.hudAmmo();r.check("the HUD counts it: 32 / 32, thirty-two pips, .380",hud.shown&&hud.weapon==WeaponId::MachinePistol&&hud.loaded==32&&hud.reserve==32&&hud.capacity==32&&std::string(hud.caliber)==".380");
  g.equipWeapon(WeaponId::Shotgun);const int shells=g.player().pool(WeaponId::Shotgun).total;pickups={{{3.5f,4.5f},Pickup::Kind::MacAmmo,true}};LabAccess::updatePickups(g);
  r.check(".380 feeds the MAC-11 with the shotgun drawn",g.player().pool(WeaponId::MachinePistol).total==96&&g.player().pool(WeaponId::Shotgun).total==shells);}
 // Keys, the wheel, the console.
 {auto g=Game::validationScene(Enemy::Kind::Huntsman,3);LabAccess::enemies(g).clear();g.update({},1.f/60);InputState k{};k.choice=4;g.update(k,1.f/60);
  r.check("key 4 does nothing before the MAC-11 is owned",g.activeFirearm()==WeaponId::Shotgun&&g.weaponSwitchPhase()==0);
  LabAccess::console(g,"give mac11");g.update({},1.f/60);g.update(k,1.f/60);for(int i=0;i<90;++i)g.update({},1.f/60);
  r.check("give mac11, then key 4 draws it",g.hasMachinePistol()&&g.activeFirearm()==WeaponId::MachinePistol&&g.player().pool(WeaponId::MachinePistol)==AmmoPool{32,32});
  LabAccess::console(g,"give 380");r.check("give 380: a box more",g.player().pool(WeaponId::MachinePistol).total==64);
  g.grantHandgun(12);InputState up{};up.weaponScroll=1;g.equipWeapon(WeaponId::Handgun);g.update({},1.f/60);g.update(up,1.f/60);for(int i=0;i<90;++i)g.update({},1.f/60);
  r.check("the wheel goes on from the handgun to the MAC-11",g.activeFirearm()==WeaponId::MachinePistol);
  g.update(up,1.f/60);for(int i=0;i<90;++i)g.update({},1.f/60);r.check("and round from it to the shotgun",g.activeFirearm()==WeaponId::Shotgun);
  // The hip holster: the sidearm not in hand; with neither out the one drawn last.
  auto hip=[&](WeaponId inHand){return g.hipHolster(inHand);};
  const bool rides=hip(WeaponId::MachinePistol)==int(WeaponId::Handgun)+1&&hip(WeaponId::Handgun)==int(WeaponId::MachinePistol)+1&&hip(WeaponId::Shotgun)==int(WeaponId::MachinePistol)+1;
  g.equipWeapon(WeaponId::Handgun);const bool after=hip(WeaponId::Shotgun)==int(WeaponId::Handgun)+1;
  r.check("the hip holds the sidearm not in hand (the MAC-11 when it was drawn last)",rides&&after);}
 // Saves: the new pool, the gun, the pickups; an older save.
 {auto m=regression::room();m->pickupSpawns.push_back({{6,6},PickupKind::MacAmmo});m->pickupSpawns.push_back({{7,6},PickupKind::MachinePistol});
  Game g(regression::campaign(m));g.grantMachinePistol(32);LabAccess::player(g).pool(WeaponId::MachinePistol)={70,21};g.equipWeapon(WeaponId::MachinePistol);
  Game loaded(regression::campaign(m));const bool ok=LabAccess::decodeSave(loaded,LabAccess::encodeSave(g));int kinds=0;for(const auto& p:loaded.pickups())kinds+=p.kind==PickupKind::MacAmmo||p.kind==PickupKind::MachinePistol;
  r.check("a save keeps the MAC-11, its .380 (70 / 21), in hand, and its pickups",ok&&loaded.hasMachinePistol()&&loaded.player().pool(WeaponId::MachinePistol)==AmmoPool{70,21}&&loaded.activeFirearm()==WeaponId::MachinePistol&&kinds==2);
  // The positional archive (the last format before sections) of a room with no MAC-11 in it.
  auto bare=regression::room();Game legacy(regression::campaign(bare));Game plain(regression::campaign(bare));plain.grantHandgun(12);
  const bool olderOk=LabAccess::decodeSave(legacy,LabAccess::legacySaveText(plain,Game::LegacySaveVersion));
  bool clear=true;for(int i=0;i<4;++i){const int at=legacy.itemCell(i),w=i==0?4:i==2?2:1,c=legacy.itemCell(4);for(int rr=c/6;rr<c/6+2;++rr)if(rr>=at/6&&rr<at/6+2&&c%6>=at%6&&c%6<at%6+w)clear=false;}
  r.check("a save from before the MAC-11 loads: no .380, its cell clear of the others",olderOk&&legacy.player().pool(WeaponId::MachinePistol)==AmmoPool{}&&!legacy.hasMachinePistol()&&clear);
  }
 // Every gun's cases leave the gun and come to rest on the floor, away from it.
 struct Gun{WeaponId id;int kind;const char* name;};
 for(const Gun gun:{Gun{WeaponId::MachinePistol,2,"MAC-11"},Gun{WeaponId::Handgun,1,"handgun"},Gun{WeaponId::Shotgun,0,"shotgun"}}){
  auto g=Game::validationScene(Enemy::Kind::Huntsman,3);LabAccess::enemies(g).clear();LabAccess::player(g).pos={4.5f,4.5f};
  if(gun.id==WeaponId::Handgun)g.grantHandgun(24);if(gun.id==WeaponId::MachinePistol)g.grantMachinePistol(32);g.equipWeapon(gun.id);g.update({},1.f/60);
  const Point3 port=g.ejectionPort(gun.id);InputState fire{};fire.fire=true;g.update(fire,1.f/60);
  for(int i=0;i<20;++i)g.update({},1.f/60);   // the pump works the shell out at 0.12 s
  int count=0;for(const auto& c:g.casings())count+=c.kind==gun.kind;
  r.check(std::string("a round from the ")+gun.name+" throws its case",count>=1,std::to_string(count)+" cases");
  for(int i=0;i<150;++i)g.update({},1.f/60);
  bool rest=true,floor=true,away=true;float worstFloor=0,nearest=99;
  for(const auto& c:g.casings()){if(c.kind!=gun.kind)continue;rest&=c.resting;const float f=LabAccess::world(g).floorHeight(c.position.x,c.position.y);worstFloor=std::max(worstFloor,std::fabs(c.position.z-f));
   floor&=c.position.z-f<.02f&&c.position.z>=f;const float d=dist(c.position,port);nearest=std::min(nearest,d);away&=d>.3f&&std::hypot(c.position.x-4.5f,c.position.y-4.5f)>.2f;}
  r.check(std::string("its case comes to rest on the floor (")+gun.name+")",count>=1&&rest&&floor,"furthest off the floor "+fixed(worstFloor,4)+" m");
  r.check(std::string("well away from the gun, not hanging by it (")+gun.name+")",count>=1&&away,"nearest "+fixed(nearest,2)+" m from the port");}
 {auto g=macScene({200,32});InputState fire{};fire.fire=true;for(int i=0;i<200;++i)g.update(fire,1.f/60);
  r.check("live cases are capped",int(g.casings().size())<=Game::MaxCasings,std::to_string(g.casings().size())+" of "+std::to_string(Game::MaxCasings));
  LabAccess::loadLevel(g,0);r.check("a new map clears them",g.casings().empty());}
}

const bool mac11=add({
 .name="mac11",.summary="the MAC-11: full auto, climb and bloom, both reloads seen in first person, the cases every gun throws, third person and the floor pickups",
 .friends={},.fixtures={"mac11-room"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::playerCamera(g);LabAccess::setGod(g,true);SoftwareRenderer::setThirdPerson(false);LabAccess::resetBody(*s.renderer);LabAccess::settings(g).character=0;
  g.grantMachinePistol(0);LabAccess::player(g).pool(WeaponId::MachinePistol)={160,32};g.grantHandgun(24);g.equipWeapon(WeaponId::MachinePistol);},
 .step=[](Scene& s,float){
  Game& g=s.g();auto& p=LabAccess::player(g);const int phase=phaseAt(s.frame),at=s.frame-PhaseStart[phase];
  if(at==0){s.board["mac11.phase"]=phase;
   switch(phase){
    case Floor:break;   // turned onto the cases each frame (below)
    case Aimed:p.pitch=0;break;
    case Dry:{auto& pool=p.pool(WeaponId::MachinePistol);pool.loaded=std::min(pool.loaded,3);}break;
    case Sprint:p.angle=0;p.pitch=0;p.pos={8,12};break;
    case Shotgun:LabAccess::equip(g,WeaponId::Shotgun);p.pos={12,12};p.angle=0;LabAccess::velocity(g)={};p.pitch=-8;break;
    case Pistol:LabAccess::equip(g,WeaponId::Handgun);p.pitch=-20;break;
    case OutsideClaire:LabAccess::equip(g,WeaponId::MachinePistol);p.pitch=0;SoftwareRenderer::setThirdPerson(true);LabAccess::resetBody(*s.renderer);break;
    case OutsideLimitator:LabAccess::settings(g).character=1;LabAccess::resetBody(*s.renderer);break;
    case Pickups:{SoftwareRenderer::setThirdPerson(false);s.renderer->setBodyOrbit(0,0);LabAccess::settings(g).character=0;LabAccess::resetBody(*s.renderer);
     p.pos={12,12};p.angle=0;LabAccess::velocity(g)={};
     auto& pickups=LabAccess::pickups(g);pickups={{{13.15f,11.8f},Pickup::Kind::MacAmmo,true},{{13.2f,12.2f},Pickup::Kind::MachinePistol,true}};p.pitch=-75;}break;
    default:break;}}
  // Looking at where the burst's cases came down: the eye turned onto their middle.
  if(phase==Floor&&!g.casings().empty()){Point3 c{};for(const auto& k:g.casings())c=c+k.position;c=c*(1.f/float(g.casings().size()));
   const float dx=c.x-p.pos.x,dy=c.y-p.pos.y,dz=c.z-(p.z+p.eye);p.angle=std::atan2(dy,dx);p.pitch=std::atan2(dz,std::hypot(dx,dy))*140.f;}
  s.input.fire=phase==Burst||(phase==Dry&&at<20)||(phase==Shotgun&&at==4)||(phase==Pistol&&at==4)||(phase==OutsideClaire&&at>=20&&at<34);
  s.input.guard=phase==Aimed||phase==OutsideClaire||phase==OutsideLimitator;
  s.input.reload=(phase==Tactical||phase==EmptyReload)&&at==1;
  s.input.forward=phase==Sprint;s.input.sprint=phase==Sprint;
  if(phase==OutsideClaire||phase==OutsideLimitator)s.renderer->setBodyOrbit(2.6f,-.9f+1.8f*float(at)/90.f);},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();const int phase=phaseAt(s.frame),at=s.frame-PhaseStart[phase];const auto& m=s.renderer->macReport();
  auto shoot=[&](const std::string& label){s.shoot(label);};
  // The magazine against the grip, as the frame drew it (view space).
  if(m.drawn){const float off=dist(m.magazine,m.grip);
   if(phase==Idle&&at==25){s.board["mac11.mag_seated"]=off;s.board["mac11.right_grip_idle"]=m.rightGrip;}
   if(phase==Tactical){s.board["mac11.mag_out_max"]=std::max(s.board["mac11.mag_out_max"],double(off-s.board["mac11.mag_seated"]));
    if(g.reloading()&&g.reloadProgress()>.45f&&g.reloadProgress()<.6f)s.board["mac11.left_mag_held"]=std::max(s.board["mac11.left_mag_held"],double(m.leftMagazine));
    s.board["mac11.right_grip_reload"]=std::max(s.board["mac11.right_grip_reload"],double(m.rightGrip));
    if(at==89)s.board["mac11.mag_back"]=off-s.board["mac11.mag_seated"];}
   if(phase==Dry&&at==25)s.board["mac11.knob_dry"]=m.knobBack;
   if(phase==EmptyReload){s.board["mac11.empty_mag_out_max"]=std::max(s.board["mac11.empty_mag_out_max"],double(off-s.board["mac11.mag_seated"]));if(at==119)s.board["mac11.knob_after_empty"]=m.knobBack;if(at==10)s.board["mac11.knob_empty_start"]=m.knobBack;}
   if(phase==Burst)s.board["mac11.right_grip_burst"]=std::max(s.board["mac11.right_grip_burst"],double(m.rightGrip));
   s.board["mac11.drawn_frames"]+=1;}
  if(phase==Aimed&&at==55){int resting=0,onFloor=0;float worst=0;for(const auto& c:g.casings()){resting+=c.resting;const float off=std::fabs(c.position.z-LabAccess::world(g).floorHeight(c.position.x,c.position.y));worst=std::max(worst,off);onFloor+=c.resting&&off<.02f;}
   s.board["mac11.scene_cases"]=double(g.casings().size());s.board["mac11.scene_resting"]=resting;s.board["mac11.scene_on_floor"]=onFloor;s.board["mac11.scene_worst_off_floor"]=worst;}
  if(phase==Burst&&at==10)s.board["mac11.burst_cases_flying"]=double(g.casings().size());
  if(phase==OutsideClaire||phase==OutsideLimitator){const auto& rep=LabAccess::claireReport(*s.renderer);const std::string k=phase==OutsideClaire?"mac11.claire":"mac11.limitator";
   if(at>45&&rep.drawn){s.board[k+".weapon"]=rep.weapon;s.board[k+".character"]=rep.character;if(rep.handGap>=0)s.board[k+".hand_gap"]=std::max(s.board[k+".hand_gap"],double(rep.handGap));if(rep.aimError>=0)s.board[k+".aim_error"]=std::max(s.board[k+".aim_error"],double(rep.aimError));s.board[k+".seen"]=1;}}
  const std::map<int,std::string> pictures={{25,"idle hip"},{44,"burst"},{80,"burst climbing"},{88,"brass in the air"},{148,"cases on the floor"},{205,"aimed"},
   {226,"tactical: magazine out"},{248,"tactical: fresh magazine"},{268,"tactical: seating"},{325,"empty: knob held back"},{372,"empty reload: magazine out"},{418,"empty reload: knob home"},
   {500,"sprint low ready"},{512,"shotgun drawn"},{523,"shotgun: the pumped shell"},{565,"shotgun after"},{580,"handgun: brass"},{675,"Claire third person"},{708,"Claire third person (b)"},{770,"Limitator third person"},{800,"Limitator third person (b)"},{835,"pickups"}};
  if(auto it=pictures.find(s.frame);it!=pictures.end())shoot(it->second);},
 .metrics=[](Scene& s,Report& r){
  auto b=[&](const char* k,double fallback=-1){return s.board.count(k)?s.board[k]:fallback;};
  r.check("first person draws the MAC-11 on its rig",b("mac11.drawn_frames",0)>300,std::to_string(int(b("mac11.drawn_frames",0)))+" frames");
  r.metric("magazine_out",b("mac11.mag_out_max",0),"m");r.metric("magazine_back",b("mac11.mag_back"),"m");
  r.check("tactical reload: the magazine visibly leaves the gun",b("mac11.mag_out_max",0)>.08,fixed(float(b("mac11.mag_out_max",0)))+" m from seated at most");
  r.check("and comes back seated",std::fabs(b("mac11.mag_back",1))<.01,fixed(float(b("mac11.mag_back",1)),4)+" m from seated at the end");
  r.check("empty reload: the magazine leaves too",b("mac11.empty_mag_out_max",0)>.08,fixed(float(b("mac11.empty_mag_out_max",0)))+" m");
  r.check("the left hand holds the fresh magazine",b("mac11.left_mag_held",1)<.07,fixed(float(b("mac11.left_mag_held",1)))+" m from its axis");
  r.metric("right_grip_idle",b("mac11.right_grip_idle"),"m");
  r.check("the right hand is on the grip (idle, firing, reloading)",b("mac11.right_grip_idle",1)<.05&&b("mac11.right_grip_burst",1)<.05&&b("mac11.right_grip_reload",1)<.05,
   fixed(float(b("mac11.right_grip_idle",1)))+" / "+fixed(float(b("mac11.right_grip_burst",1)))+" / "+fixed(float(b("mac11.right_grip_reload",1)))+" m from its axis");
  r.check("on an empty magazine the knob is held back",b("mac11.knob_dry",0)>.03,fixed(float(b("mac11.knob_dry",0)))+" m");
  r.check("the empty reload starts with the knob back and racks it home",b("mac11.knob_empty_start",0)>.03&&b("mac11.knob_after_empty",1)<.005,fixed(float(b("mac11.knob_empty_start",0)))+" then "+fixed(float(b("mac11.knob_after_empty",1)))+" m");
  r.check("brass is in the air during the burst",b("mac11.burst_cases_flying",0)>=2,std::to_string(int(b("mac11.burst_cases_flying",0))));
  r.check("the burst's cases lie on the floor two seconds later",b("mac11.scene_cases",0)>=10&&b("mac11.scene_on_floor",0)>=b("mac11.scene_cases",0)*.9,
   std::to_string(int(b("mac11.scene_on_floor",0)))+" of "+std::to_string(int(b("mac11.scene_cases",0)))+" on the floor ("+std::to_string(int(b("mac11.scene_resting",0)))+" at rest, the furthest "+fixed(float(b("mac11.scene_worst_off_floor",0)))+" m off it)");
  for(const char* who:{"claire","limitator"}){const std::string k=std::string("mac11.")+who;auto v=[&](const char* n,double f=-1){return s.board.count(k+n)?s.board[k+n]:f;};
   r.check(std::string("third person: ")+who+" holds the MAC-11 as the pistol",v(".seen",0)>0&&v(".weapon")==3&&v(".character")==(who[0]=='c'?0:1),"weapon "+std::to_string(int(v(".weapon")))+", body "+std::to_string(int(v(".character"))));
   r.check(std::string("third person: ")+who+"'s support hand on the grip",v(".hand_gap",1)<.08,fixed(float(v(".hand_gap",1)))+" m (the engine's test allows 0.08)");
   r.check(std::string("third person: the gun points where ")+who+" aims",v(".aim_error",99)<5,fixed(float(v(".aim_error",99)),2)+" degrees");}
  logic(r);},
 .reporter=[](Scene& s,Overlay& o){for(const auto& c:s.g().casings())o.cross(c.position,.03f,c.resting?0x40ff60:0xffc040);o.text(8,8,std::string("PHASE ")+PhaseName[phaseAt(s.frame)],0xffff60);},
 .teardown=[](Scene& s){SoftwareRenderer::setThirdPerson(false);s.renderer->setBodyOrbit(0,0);LabAccess::resetBody(*s.renderer);LabAccess::settings(s.g()).character=0;}});
}
}
