#include "Game.h"
#include "EngineRegressionScene.h"
#include "../world/CustomCampaign.h"
#include <fstream>
#include <cmath>
namespace retro {
// A downed carrier's pistol is actor state, not a Pickup: where it fell
// (loot_drop = its level+1, loot_x/y/z in mm, loot_yaw), the rounds in it
// (loot_rounds) and whether the player has it (weapon_looted). It saves with
// the other per-actor state and survives a knockout that becomes a death.
void Game::grantHandgun(int rounds){
 auto&nine=m_player.pool(WeaponId::Handgun);rounds=std::max(0,rounds);
 if(!hasHandgun()){giveQuestItem(HandgunItem);nine.loaded=std::min(rounds,playerWeaponDef(WeaponId::Handgun).cycle.tubeCapacity);}
 nine.total+=rounds;
}
int Game::sidearmRounds(size_t actor)const{
 const int magazine=weaponDef(WeaponId::Handgun).cycle.tubeCapacity;
 int left=state(friendlyKey(actor,"weapon_saved"))?state(friendlyKey(actor,"weapon_rounds")):magazine;
 return std::clamp(left,6,magazine);
}
std::vector<DroppedSidearm> Game::droppedSidearms()const{
 std::vector<DroppedSidearm> out;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){
  if(!m_friendlyActors[actor].track().ai.dropsSidearm||state(friendlyKey(actor,"weapon_looted")))continue;
  int level=state(friendlyKey(actor,"loot_drop"))-1;if(level<0||level>=chunkCount())continue;
  out.push_back({{state(friendlyKey(actor,"loot_x"))*.001f,state(friendlyKey(actor,"loot_y"))*.001f,state(friendlyKey(actor,"loot_z"))*.001f},state(friendlyKey(actor,"loot_yaw"))*.0001f,level,actor});
 }return out;
}
void Game::updateSidearms(){
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
  if(!track.ai.dropsSidearm||track.ai.mode!=ActorAiMode::Guard||state(friendlyKey(actor,"loot_drop"))||!chunkResident(npc.level))continue;
  // Down is dead, or knocked out and lying (not held, not stowed). The gun
  // stays where he fell, whoever drags him off afterwards.
  const bool killed=track.deadState&&state(track.deadState);
  const bool knockedOut=state(friendlyKey(actor,"restraint"))==1&&npc.pose.clip==4&&!state(friendlyKey(actor,"guard_stowed"));
  if(!killed&&!knockedOut)continue;
  Point3 body{npc.pose.position.x,npc.pose.position.y,npc.pose.z};
  if(killed)body={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f,state(actorPositionState(track.deadState,2))*.001f};
  // Beside his right hand: past the fingertips, away from his hips (hand is
  // the wrist, so nearer reads as still held), else to either side of it.
  auto [hand,hips]=fallenHand(actor,body,npc.pose.yaw);Vec2 out2{hand.x-hips.x,hand.y-hips.y};float reach=length(out2);
  const Vec2 wrist{hand.x,hand.y},o=reach>.01f?out2*(1.f/reach):Vec2{std::cos(npc.pose.yaw),std::sin(npc.pose.yaw)},side{-o.y,o.x};
  const auto&world=chunkWorld(npc.level);
  auto fits=[&](Vec2 p){return p.x>=.2f&&p.y>=.2f&&p.x<=23.8f&&p.y<=23.8f&&world.rayClear({body.x,body.y},body.z+.15f,p,body.z+.15f);};
  Vec2 at=Vec2{body.x,body.y}+o*.18f;
  for(Vec2 c:{wrist+o*.24f,wrist+o*.09f+side*.15f,wrist+o*.09f-side*.15f})if(fits(c)){at=c;break;}
  // A body at a chunk seam must not leave its gun outside the chunk: the
  // saved loot_x/y are validated to 0..24 m on every load.
  at.x=std::clamp(at.x,.05f,23.95f);at.y=std::clamp(at.y,.05f,23.95f);
  const float z=std::max(body.z,world.floorHeight(at.x,at.y));
  setState(friendlyKey(actor,"loot_rounds"),sidearmRounds(actor));
  setState(friendlyKey(actor,"loot_x"),int(std::round(at.x*1000)));setState(friendlyKey(actor,"loot_y"),int(std::round(at.y*1000)));setState(friendlyKey(actor,"loot_z"),int(std::round(z*1000)));
  setState(friendlyKey(actor,"loot_yaw"),int(std::round(std::remainder(npc.pose.yaw+1.1f,2*kPi)*10000)));setState(friendlyKey(actor,"loot_drop"),npc.level+1);
 }
 if(dead()||m_won)return;
 // Walking over it, or standing close and looking at it, takes it: no key,
 // so it never competes with E and works the same in VR.
 const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(const auto&gun:droppedSidearms()){if(!chunkResident(gun.level))continue;
  Vec2 local=Vec2{gun.position.x,gun.position.y}+chunkOffset(gun.level)-chunkOffset(m_level);auto delta=local-m_player.pos;float d=length(delta);
  const bool over=d<.45f&&std::fabs(m_player.z-gun.position.z)<.5f;
  const bool looked=d<.9f&&std::fabs(m_player.z-gun.position.z)<.8f&&dot(delta,forward)>d*.6f&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,local,gun.position.z+.1f);
  if(!over&&!looked)continue;
  int rounds=std::clamp(state(friendlyKey(gun.actor,"loot_rounds")),0,weaponDef(WeaponId::Handgun).cycle.tubeCapacity);setState(friendlyKey(gun.actor,"weapon_looted"),1);
  m_pickupNotice=(hasHandgun()?"TOOK 9MM x ":"TOOK HANDGUN / 9MM x ")+std::to_string(rounds);grantHandgun(rounds);
  m_pickupNoticeTime=2.5f;sound(Sound::Pickup,.6f,.92f);
 }
}
// The player handgun, game side: who drops it, taking it, its ammunition and
// HUD, semi-automatic fire, magazine reloads, switching, restores, VR, the
// console, saves and the custom format.
bool Game::testHandgun(){
 std::ofstream report("handgun-test.txt");bool passed=true;auto check=[&](bool ok,const std::string&label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;return ok;};
 auto actorNamed=[](const Game&g,const char*key){for(size_t i=0;i<g.m_friendlyActors.size();++i)if(g.m_friendlyActors[i].track().deadState==stateId(std::string(key)+"_dead"))return int(i);return -1;};
 auto walkTo=[](Game&g,Vec2 at,float z){g.m_player.pos=at;g.m_player.z=z;g.updateSidearms();};
 const auto&handgun=playerWeaponDef(WeaponId::Handgun);
 check(handgun.id==WeaponId::Handgun&&handgun.cycle.semiAuto&&handgun.cycle.tubeCapacity==12&&handgun.cycle.pickupAmount==12&&handgun.hitscan.maxRange==24&&handgun.hitscan.dmgNear==20&&handgun.hitscan.coneBase<playerWeaponDef(WeaponId::Shotgun).hitscan.coneBase&&handgun.cycle.emptyReloadTime>handgun.cycle.reloadTime,"The player handgun is its own kWeapons row: semi-automatic, 12-round magazine, 24 m, a tighter cone");
 check(weaponDef(WeaponId::Handgun).hitscan.dmgNear==10&&&weaponDef(WeaponId::Handgun)==&kGuardHandgun&&&playerWeaponDef(WeaponId::Handgun)!=&kGuardHandgun,"Guards keep kGuardHandgun; the player reads playerWeaponDef");
 check(std::ceil(90.f/handgun.hitscan.actorDamage)>=4&&std::ceil(110.f/handgun.hitscan.actorDamage)<=6,"A Cleanup soldier (90-110) takes four to six body hits");
 // --- Chapter 2: a carrier killed drops his pistol at his hand -----------------
 {Game g=mapInspection({12,12},0,0,68,false,-999,false);g.m_god=true;
  const int hall=actorNamed(g,"rf_intake_hall"),sweep=actorNamed(g,"rf_intake_sweep_b"),lead=actorNamed(g,"rf_intake_lead");
  check(hall>=0&&sweep>=0&&lead>=0&&g.m_friendlyActors[size_t(hall)].track().ai.dropsSidearm&&g.m_friendlyActors[size_t(sweep)].track().ai.dropsSidearm&&!g.m_friendlyActors[size_t(lead)].track().ai.dropsSidearm,"Map 68's medic and engineer carry a droppable sidearm; the shotgun lead does not");
  if(hall>=0&&sweep>=0){
   check(g.droppedSidearms().empty(),"Nobody has dropped anything while they stand");
   g.setState(g.friendlyKey(size_t(hall),"weapon_saved"),1);g.setState(g.friendlyKey(size_t(hall),"weapon_rounds"),9);
   g.damageFriendly(size_t(hall),500,g.m_player.pos,{ShotKind::Player,g.level(),0});g.update({},1.f/60);g.update({},1.f/60);
   auto dropped=g.droppedSidearms();const auto&body=g.m_friendlyActors[size_t(hall)].pose;
   check(dropped.size()==1&&dropped[0].actor==size_t(hall)&&dropped[0].level==68,"A killed carrier's pistol is on the floor");
   if(!dropped.empty()){auto hand=g.fallenHand(size_t(hall),{body.position.x,body.position.y,body.z},body.yaw).first;
    Vec2 gun{dropped[0].position.x,dropped[0].position.y};
    report<<"  body "<<body.position.x<<","<<body.position.y<<" hand "<<hand.x<<","<<hand.y<<" gun "<<gun.x<<","<<gun.y<<'\n';
    check(length(gun-Vec2{hand.x,hand.y})<.3f&&length(gun-body.position)<1.6f&&std::fabs(dropped[0].position.z-body.z)<.3f,"It lies beside his right hand, on his floor");
    check(!g.npcWeaponPose(size_t(hall)).armed,"The body no longer holds or holsters a gun");
    walkTo(g,gun+Vec2{2.f,0},body.z);check(!g.hasHandgun(),"Two metres away nothing is taken");
    walkTo(g,gun,body.z);
    check(g.hasHandgun()&&g.player().pool(WeaponId::Handgun)==AmmoPool{9,9}&&g.pickupNotice()=="TOOK HANDGUN / 9MM x 9"&&g.droppedSidearms().empty()&&g.state(g.friendlyKey(size_t(hall),"weapon_looted"))==1,"Walking over it takes the handgun with his remaining nine rounds");}
   g.damageFriendly(size_t(sweep),500,g.m_player.pos,{ShotKind::Player,g.level(),0});g.updateSidearms();dropped=g.droppedSidearms();
   if(check(dropped.size()==1,"A second carrier drops his")){const auto at=dropped[0].position;
    // Within .9 m and looked at: taken without stepping on it.
    g.m_player.pos={at.x-.7f,at.y};g.m_player.z=at.z;g.m_player.angle=0;g.m_player.pitch=-40;g.updateSidearms();
    check(g.hasHandgun()&&g.player().pool(WeaponId::Handgun)==AmmoPool{21,9}&&g.pickupNotice()=="TOOK 9MM x 12"&&g.droppedSidearms().empty(),"A second body gives only rounds (a full magazine), taken with a look from .7 m");}
  }}
 // --- Chapter 1: a carrier drops nothing -------------------------------------
 {Game g=mapInspection({12,12},0,0,14,false,-999,false);g.m_god=true;int carrier=-1;
  for(size_t i=0;i<g.m_friendlyActors.size();++i){const auto&npc=g.m_friendlyActors[i];if(npc.level==14&&guardHandgun(npc.track().ai)){carrier=int(i);break;}}
  if(check(carrier>=0,"Receiving Lanes fields a pistol carrier")){g.damageFriendly(size_t(carrier),500,g.m_player.pos,{ShotKind::Player,14,0});g.update({},1.f/60);
   walkTo(g,g.m_friendlyActors[size_t(carrier)].pose.position,g.m_friendlyActors[size_t(carrier)].pose.z);
   check(g.droppedSidearms().empty()&&!g.hasHandgun()&&g.state(g.friendlyKey(size_t(carrier),"loot_drop"))==0,"A Chapter 1 carrier killed drops nothing (dropsSidearm is off)");}}
 // --- A knockout drops one; looting survives the KO-to-death and save/load ---
 {Game g=mapInspection({12,12},0,0,68,false,-999,false);g.m_god=true;const int hall=actorNamed(g,"rf_intake_hall");
  if(hall>=0){const size_t a=size_t(hall);auto&npc=g.m_friendlyActors[a];
   g.setState(g.friendlyKey(a,"restraint"),1);npc.pose.clip=4;npc.pose.phase=1;g.saveRestrainedPose(a);g.updateSidearms();
   auto dropped=g.droppedSidearms();check(dropped.size()==1,"A knocked-out carrier drops his pistol");
   const int x=g.state(g.friendlyKey(a,"loot_x")),y=g.state(g.friendlyKey(a,"loot_y"));
   // Dragged off, the gun stays where he went down.
   npc.pose.position+=Vec2{1.5f,0};g.saveRestrainedPose(a);g.updateSidearms();
   check(g.state(g.friendlyKey(a,"loot_x"))==x&&g.state(g.friendlyKey(a,"loot_y"))==y&&g.droppedSidearms().size()==1,"Dragging the body leaves the gun where he fell");
   Game loaded;check(loaded.decodeSave(g.encodeSave())&&loaded.droppedSidearms().size()==1&&loaded.state(loaded.friendlyKey(a,"loot_x"))==x,"The unlooted floor gun survives save/load");
   loaded.damageFriendly(a,500,loaded.m_player.pos,{ShotKind::Player,loaded.level(),0});loaded.updateSidearms();
   check(loaded.droppedSidearms().size()==1&&loaded.state(loaded.friendlyKey(a,"loot_x"))==x,"Killing him afterwards keeps the same floor gun");
   if(!loaded.droppedSidearms().empty()){const auto at=loaded.droppedSidearms()[0].position;walkTo(loaded,{at.x,at.y},at.z);}
   Game again;check(loaded.hasHandgun()&&again.decodeSave(loaded.encodeSave())&&again.hasHandgun()&&again.droppedSidearms().empty()&&again.state(again.friendlyKey(a,"weapon_looted"))==1,"Looted after the KO-to-death, it stays looted across save/load");
  }else check(false,"Map 68 medic present");}
 // --- A carrier killed on a chunk seam leaves his gun inside the chunk ------------
 for(int turn=0;turn<4;++turn){Game g=mapInspection({12,12},0,0,68,false,-999,false);g.m_god=true;const int hall=actorNamed(g,"rf_intake_hall");
  if(hall>=0){const size_t a=size_t(hall);auto&npc=g.m_friendlyActors[a];npc.pose.position.y=.05f;npc.pose.yaw=-kPi*.5f*float(turn);
   g.damageFriendly(a,500,g.m_player.pos,{ShotKind::Player,g.level(),0});g.updateSidearms();
   const int x=g.state(g.friendlyKey(a,"loot_x")),y=g.state(g.friendlyKey(a,"loot_y"));report<<"  seam gun "<<x<<","<<y<<'\n';
   Game loaded;check(g.droppedSidearms().size()==1&&x>=0&&x<=24000&&y>=0&&y<=24000&&loaded.decodeSave(g.encodeSave())&&loaded.droppedSidearms().size()==1,"A body on the seam (y .05 m, turned "+std::to_string(turn*90)+") drops inside the chunk and the save still loads");}
  else check(false,"Map 68 medic present");}
 // --- Each box feeds its own pool, whatever is in hand --------------------------
 {auto g=validationScene(Enemy::Kind::Huntsman,3);
  g.m_pickups={{{3.5f,4.5f},Pickup::Kind::HandgunAmmo,true}};g.updatePickups();
  check(!g.hasHandgun()&&g.player().pool(WeaponId::Handgun)==AmmoPool{12,0}&&g.pickupNotice()=="COLLECTED 12 ROUNDS 9MM","A 9 mm box before the pistol waits in its pool");
  g.grantHandgun(0);g.equipWeapon(WeaponId::Handgun);const int shells=g.player().pool(WeaponId::Shotgun).total;
  g.m_pickups={{{3.5f,4.5f},Pickup::Kind::Ammo,true}};g.updatePickups();
  check(g.player().pool(WeaponId::Shotgun).total==shells+16&&g.player().pool(WeaponId::Handgun).total==12&&g.pickupNotice()=="COLLECTED 16 SHELLS","Shells feed the shotgun with the handgun drawn");
  g.equipWeapon(WeaponId::Shotgun);g.m_pickups={{{3.5f,4.5f},Pickup::Kind::HandgunAmmo,true}};g.updatePickups();
  check(g.player().pool(WeaponId::Handgun).total==24&&g.player().pool(WeaponId::Shotgun).total==shells+16,"9 mm feeds the handgun with the shotgun drawn");
  // A box on a desk is reached from beside it, looking at it.
  g.m_pickups={{{4.2f,4.5f},Pickup::Kind::HandgunAmmo,true,.78f}};g.m_player.angle=0;g.updatePickups();
  check(!g.m_pickups[0].active&&g.player().pool(WeaponId::Handgun).total==36,"A box on a desk within reach and in view is taken");
  g.m_pickups={{{4.2f,4.5f},Pickup::Kind::HandgunAmmo,true,.78f}};g.m_player.angle=kPi;g.updatePickups();check(g.m_pickups[0].active,"Not with the back to it");}
 // --- The HUD counter shows only the firearm in hand -----------------------------
 {auto g=validationScene(Enemy::Kind::Huntsman,3);g.m_player.pool(WeaponId::Shotgun)={20,4};auto hud=g.hudAmmo();
  check(hud.shown&&hud.weapon==WeaponId::Shotgun&&hud.loaded==4&&hud.reserve==16&&hud.capacity==6&&std::string(hud.caliber)=="12 GA","Shotgun counter: 4 / 16, six pips, 12 GA");
  g.grantHandgun(12);g.m_player.pool(WeaponId::Handgun)={30,7};g.equipWeapon(WeaponId::Handgun);hud=g.hudAmmo();
  check(hud.shown&&hud.weapon==WeaponId::Handgun&&hud.loaded==7&&hud.reserve==23&&hud.capacity==12&&std::string(hud.caliber)=="9MM","Handgun counter: 7 / 23, twelve pips, 9MM");
  g.equipWeapon(WeaponId::Fists);check(!g.hudAmmo().shown,"Fists show no counter");
  g.equipWeapon(WeaponId::Handgun);g.m_player.pool(WeaponId::Handgun)={};g.m_shotAge=10;for(int i=0;i<60;++i)g.update({},1.f/60);check(g.unarmed()&&!g.hudAmmo().shown,"An empty gun lowered shows no counter");}
 // --- Semi-automatic fire and its cooldown ------------------------------------------
 auto pistolScene=[&](AmmoPool rounds){auto g=validationScene(Enemy::Kind::Huntsman,3);g.m_enemies.clear();g.grantHandgun(0);g.m_player.pool(WeaponId::Handgun)=rounds;g.equipWeapon(WeaponId::Handgun);g.update({},1.f/60);return g;};
 {auto g=pistolScene({36,12});InputState fire{};fire.fire=true;int shots=0;
  for(int i=0;i<60;++i){g.update(fire,1.f/60);for(const auto&e:g.sounds())shots+=e.sound==Sound::PistolShot;}
  check(g.player().pool(WeaponId::Handgun)==AmmoPool{35,11}&&shots==1,"Holding the trigger for a second spends one round (one PistolShot)");
  g.update({},1.f/60);g.update(fire,1.f/60);check(g.player().pool(WeaponId::Handgun).loaded==10,"Released and pressed again, it fires again");
  // Tapping every other frame for a second: the .2 s cooldown caps the rate.
  int before=g.player().pool(WeaponId::Handgun).loaded;for(int i=0;i<60;++i){InputState tap{};tap.fire=i%2==0;g.update(tap,1.f/60);}
  int spent=before-g.player().pool(WeaponId::Handgun).loaded;check(spent>=4&&spent<=6,"Rapid presses fire at most one round per .2 s ("+std::to_string(spent)+" in a second)");
  auto shotgun=validationScene(Enemy::Kind::Huntsman,3);shotgun.m_enemies.clear();for(int i=0;i<90;++i)shotgun.update(fire,1.f/60);
  check(shotgun.player().pool(WeaponId::Shotgun).loaded<=4,"The shotgun still repeats while held");
  auto dry=pistolScene({12,0});dry.update(fire,1.f/60);bool click=false;for(const auto&e:dry.sounds())click|=e.sound==Sound::PistolDry;check(click,"An empty magazine clicks dry");}
 // --- Magazine reloads, tactical and empty --------------------------------------
 {auto g=pistolScene({24,5});InputState r{};r.reload=true;g.update(r,1.f/60);
  check(g.reloading()&&!g.emptyReload()&&std::fabs(g.reloadDuration()-1.5f)<.001f&&g.reloadProgress()<.05f,"A partly spent magazine reloads in 1.5 s (tactical)");
  std::vector<Sound> heard;bool midway=false;for(int i=0;i<100&&g.reloading();++i){g.update({},1.f/60);for(const auto&e:g.sounds())heard.push_back(e.sound);if(g.reloadProgress()>.45f&&g.reloadProgress()<.55f)midway|=g.player().pool(WeaponId::Handgun).loaded==5;}
  auto has=[&](Sound s){return std::find(heard.begin(),heard.end(),s)!=heard.end();};
  check(midway&&g.player().pool(WeaponId::Handgun)==AmmoPool{24,12}&&has(Sound::PistolMagOut)&&has(Sound::PistolMagIn)&&!has(Sound::PistolRack)&&!has(Sound::Pump),"It seats the magazine at the end: 12 loaded, mag out and in, no slide release");
  auto e=pistolScene({13,1});InputState fire{};fire.fire=true;e.update(fire,1.f/60);
  check(e.player().pool(WeaponId::Handgun)==AmmoPool{12,0}&&e.slideLocked(),"The last round locks the slide back");
  e.update({},1.f/60);e.update(r,1.f/60);check(e.emptyReload()&&std::fabs(e.reloadDuration()-1.9f)<.001f&&e.slideLocked(),"An empty magazine takes the longer 1.9 s reload");
  heard.clear();bool releasedAtRack=true;for(int i=0;i<130&&e.reloading();++i){e.update({},1.f/60);for(const auto&s:e.sounds()){heard.push_back(s.sound);if(s.sound==Sound::PistolRack)releasedAtRack&=!e.slideLocked();}if(e.reloading()&&e.reloadProgress()<.8f)releasedAtRack&=e.slideLocked();}
  check(e.player().pool(WeaponId::Handgun)==AmmoPool{12,12}&&has(Sound::PistolRack)&&releasedAtRack&&!e.slideLocked(),"It ends with the slide released (rack heard) and twelve loaded");
  auto full=pistolScene({24,12});full.update(r,1.f/60);check(!full.reloading(),"A full magazine does not reload");}
 // --- Switching: the wheel, the keys, lower-swap-raise ----------------------------
 {auto g=validationScene(Enemy::Kind::Huntsman,3);g.m_enemies.clear();g.grantHandgun(12);g.update({},1.f/60);
  InputState up{};up.weaponScroll=1;g.update(up,1.f/60);check(g.weaponSwitchPhase()==1&&g.weaponSwitchTarget()==WeaponId::Handgun&&g.activeFirearm()==WeaponId::Shotgun,"Wheel up with the shotgun out starts lowering it for the handgun");
  bool swappedAtBottom=true,sawRaise=false;float previous=g.holster();WeaponId was=g.activeFirearm();
  for(int i=0;i<90;++i){g.update({},1.f/60);if(g.activeFirearm()!=was){swappedAtBottom&=previous>=.999f;was=g.activeFirearm();}sawRaise|=g.weaponSwitchPhase()==2;if(g.activeFirearm()==WeaponId::Shotgun)swappedAtBottom&=g.weaponSwitchPhase()==1;previous=g.holster();}
  check(swappedAtBottom&&sawRaise&&g.activeFirearm()==WeaponId::Handgun&&g.weaponSwitchPhase()==0&&g.holster()<playerWeaponDef(WeaponId::Handgun).cycle.raiseHolster,"The shotgun lowers, the handgun swaps in only at the bottom, then rises");
  g.update(up,1.f/60);for(int i=0;i<90;++i)g.update({},1.f/60);check(g.activeFirearm()==WeaponId::Shotgun&&!g.unarmed(),"Wheel up again cycles back to the shotgun");
  InputState down{};down.weaponScroll=-1;g.update(down,1.f/60);check(!g.weaponEquipped()&&g.equippedWeapon()==WeaponId::Fists,"Wheel down holsters to fists");
  InputState key{};key.choice=3;g.update(key,1.f/60);check(g.weaponEquipped()&&g.activeFirearm()==WeaponId::Handgun&&g.weaponSwitchPhase()==2,"Key 3 draws the handgun from empty hands, raising it");
  g.update({},1.f/60);key.choice=2;g.update(key,1.f/60);check(g.weaponSwitchTarget()==WeaponId::Shotgun,"Key 2 picks the shotgun");
  for(int i=0;i<90;++i)g.update(key,1.f/60);check(g.activeFirearm()==WeaponId::Shotgun,"A held key switches once");
  key.choice=1;g.update(key,1.f/60);check(!g.weaponEquipped(),"Key 1 holsters");
  auto unowned=validationScene(Enemy::Kind::Huntsman,3);key.choice=3;unowned.update(key,1.f/60);check(unowned.activeFirearm()==WeaponId::Shotgun&&unowned.weaponSwitchPhase()==0,"Key 3 does nothing before the handgun is owned");
  InputState up2{};up2.weaponScroll=1;unowned.update({},1.f/60);unowned.update(up2,1.f/60);check(unowned.activeFirearm()==WeaponId::Shotgun&&unowned.weaponSwitchPhase()==0,"Without the handgun the wheel keeps the shotgun");
  // Keys are replies in a conversation (map 69's wounded private, at a truce).
  Game talk=mapInspection({3.f,6.75f},kPi,0,69,false,-999,false);talk.m_god=true;talk.grantHandgun(12);talk.setState(stateId("rf_proc_stance"),int(SquadStance::Truce));const int priv=actorNamed(talk,"rf_proc_private");
  if(check(priv>=0,"Map 69's private present")){talk.m_talk={};talk.m_talk.actor=priv;talk.m_talk.node=talk.m_friendlyActors[size_t(priv)].track().talk;talk.m_talk.age=1;
   InputState reply{};reply.choice=3;talk.update(reply,1.f/60);check(talk.activeFirearm()==WeaponId::Shotgun&&talk.weaponSwitchPhase()==0,"In a conversation 3 answers and never switches");}
  // A reload is abandoned by a switch: nothing reaches the magazine.
  auto cut=pistolScene({24,5});InputState r{};r.reload=true;cut.update(r,1.f/60);cut.update({},1.f/60);InputState two{};two.choice=2;cut.update(two,1.f/60);
  check(!cut.reloading()&&cut.player().pool(WeaponId::Handgun)==AmmoPool{24,5},"Switching mid-reload abandons it");}
 // --- No frame of a switch is empty-handed: no fists, no FISTS, no punch --------------
 {auto g=validationScene(Enemy::Kind::Huntsman,3);g.m_enemies.clear();g.grantHandgun(12);g.update({},1.f/60);InputState fire{};fire.fire=true;
  auto sweep=[&](WeaponId to,const std::string&label){g.selectWeapon(to);int frames=0;bool armed=true;
   for(int i=0;i<120;++i){g.update(fire,1.f/60);if(!g.weaponSwitchPhase())continue;++frames;armed&=!g.unarmed()&&g.hudAmmo().shown&&g.hudAmmo().weapon==g.activeFirearm();for(const auto&e:g.sounds())armed&=e.sound!=Sound::PunchSwing;}
   check(frames>5&&armed&&g.activeFirearm()==to&&g.weaponSwitchPhase()==0,label+": every frame of it ("+std::to_string(frames)+") keeps a gun and its counter, fire held");};
  sweep(WeaponId::Handgun,"Shotgun to handgun");sweep(WeaponId::Shotgun,"Handgun to shotgun");
  g.equipWeapon(WeaponId::Fists);g.update({},1.f/60);check(g.unarmed()&&!g.hudAmmo().shown,"Holstered, the hands are empty");
  sweep(WeaponId::Handgun,"Fists to handgun");}
 // --- Restraint, the held guard's death and the locker give back the handgun ----------
 {auto m=regression::room();m->definition.playerStart={6.5f,6.5f};auto guard=regression::guard({8.5f,6.5f},{8.5f,6.5f},AiFaction::Raider);guard.ai.armed=true;m->actorTracks.push_back(guard);
  for(int weapon:{1,2})for(bool dies:{false,true}){Game g(regression::campaign(m));g.grantHandgun(12);g.equipWeapon(WeaponId(weapon));
   g.setState(g.friendlyKey(0,"restraint"),2);g.setState(g.friendlyKey(0,"restraint_weapon"),weapon);g.saveRestrainedPose(0);g.equipWeapon(WeaponId::Fists);
   if(dies)g.damageFriendly(0,500,g.m_player.pos,{ShotKind::Player,0,0});else g.releaseNpc(false);
   check(g.weaponEquipped()&&g.activeFirearm()==WeaponId(weapon),std::string(weapon==2?"Handgun":"Shotgun")+(dies?" comes back when the held guard dies":" comes back when the held guard is released"));}
  {Game g(regression::campaign(m));g.grantHandgun(12);g.setState(g.friendlyKey(0,"restraint"),2);g.setState(g.friendlyKey(0,"restraint_weapon"),1);g.saveRestrainedPose(0);g.equipWeapon(WeaponId::Fists);
   InputState k{};k.choice=3;g.update(k,1.f/60);InputState w{};w.weaponScroll=1;g.update(w,1.f/60);
   check(g.heldNpc()==0&&!g.weaponEquipped()&&g.weaponSwitchPhase()==0,"Holding a guard, the keys and the wheel leave the hands on him");}
  Game g(regression::campaign(m));g.grantHandgun(12);g.equipWeapon(WeaponId::Handgun);g.m_lockerRearm=true;g.equipWeapon(WeaponId::Fists);g.lockerRearm();
  check(g.weaponEquipped()&&g.activeFirearm()==WeaponId::Handgun,"Leaving a locker re-arms the handgun that went in");}
 // --- VR: the pistol never waits on a pump ------------------------------------------
 {auto g=pistolScene({36,12});InputState vr{};vr.vrTracked=true;vr.vrYaw=g.player().angle;vr.fire=true;
  g.setVrAim({g.player().pos.x,g.player().pos.y,1.2f},{1,0,0},true,true);g.update(vr,1.f/60);
  vr.fire=false;for(int i=0;i<20;++i)g.update(vr,1.f/60);g.setVrAim({g.player().pos.x,g.player().pos.y,1.2f},{1,0,0},true,true);vr.fire=true;g.update(vr,1.f/60);
  check(g.player().pool(WeaponId::Handgun).loaded==10&&!g.m_vrChamberEmpty,"Two-handed in VR the pistol fires again without a pump stroke");
  auto s=validationScene(Enemy::Kind::Huntsman,3);s.m_enemies.clear();s.setVrAim({s.player().pos.x,s.player().pos.y,1.2f},{1,0,0},true,true);InputState sv{};sv.vrTracked=true;sv.vrYaw=s.player().angle;sv.fire=true;s.update(sv,1.f/60);
  for(int i=0;i<60;++i)s.update(sv,1.f/60);check(s.player().pool(WeaponId::Shotgun).loaded==5&&s.m_vrChamberEmpty,"The shotgun still waits for its pump");
  auto slot=validationScene(Enemy::Kind::Huntsman,3);slot.grantHandgun(12);InputState pick{};pick.weaponSlot=int(slot.shoulderSlotWeapon())+1;slot.update(pick,1.f/60);
  check(slot.weaponSwitchTarget()==WeaponId::Handgun,"The VR shoulder slot cycles the shotgun on to the handgun");}
 // --- Console gives ---------------------------------------------------------
 {auto g=validationScene(Enemy::Kind::Huntsman,3);g.executeConsole("give handgun");check(g.hasHandgun()&&g.player().pool(WeaponId::Handgun)==AmmoPool{12,12},"give handgun: the pistol with one magazine");
  g.executeConsole("give 9mm");check(g.player().pool(WeaponId::Handgun)==AmmoPool{36,12},"give 9mm: 24 more rounds");}
 // --- Saves -------------------------------------------------------------------
 passed&=testHandgunSaves(report);
 // --- The custom format's dropsSidearm -----------------------------------------
 {std::string records="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Handgun authoring test|0\nMAP|0|Room|0|0|18.5|18.5|0|0|8|0.3||0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y)records+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";
  records+="TIMED_SEQUENCE|0|clock|done|0|0|24|24|-0.2|8|30000|30000|0|0|0|0|1|1\n";
  const std::string rest="ACTOR_KEY|0|0|0|6.5|6.5|0|1.57|0|0\nACTOR_KEY|0|0|30000|6.5|6.5|0|1.57|0|1\nACTOR_AI|0|0|3||1.4|2|6\nACTOR_GUARD|0|0|11|0.65|600|4000|alert\nACTOR_PATROL|0|0|6.5|6.5|0|900\nACTOR_HEALTH|0|0|90|hg_damage|hg_dead\n";
  auto load=[&](const std::string&actor,int weapon){return loadCustomCampaignText(records+actor+rest+"ACTOR_WEAPON|0|0|"+std::to_string(weapon)+"|650|1000|9|18\n--- CUSTOM_CAMPAIGN_DATA_END ---\n","handgun-authoring.txt");};
  try{auto plain=load("ACTOR|0|clock|4|1.6|0|0|0\n",2);auto drops=load("ACTOR|0|clock|4|1.6|0|0|0|1\n",2);
   check(!plain->maps[0]->actorTracks[0].ai.dropsSidearm&&drops->maps[0]->actorTracks[0].ai.dropsSidearm,"ACTOR's optional trailing field sets dropsSidearm (off when absent)");
   Game g(drops);g.m_god=true;g.damageFriendly(0,500,g.m_player.pos,{ShotKind::Player,0,0});g.updateSidearms();check(g.droppedSidearms().size()==1,"A custom-map carrier with the field drops his pistol");
   Game h(plain);h.m_god=true;h.damageFriendly(0,500,h.m_player.pos,{ShotKind::Player,0,0});h.updateSidearms();check(h.droppedSidearms().empty(),"Without it he drops nothing");
   Game loaded(drops);check(loaded.decodeSave(g.encodeSave())&&loaded.droppedSidearms().size()==1,"The custom carrier's dropped pistol survives save/load");
  }catch(const std::exception&e){check(false,std::string("Custom dropsSidearm records load: ")+e.what());}
  bool refused=false;try{load("ACTOR|0|clock|4|1.6|0|0|0|1\n",1);}catch(const std::exception&){refused=true;}check(refused,"A shotgun guard cannot be authored to drop a sidearm");
  refused=false;try{load("ACTOR|0|clock|4|1.6|0|0|0|1|0\n",2);}catch(const std::exception&){refused=true;}check(refused,"ACTOR takes at most one extra field");}
 report<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
