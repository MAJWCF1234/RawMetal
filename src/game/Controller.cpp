// The controller's share of the game (docs/Systems/accessibility.md): the motors, the prompts in the pad's own
// button names, and aim assist on the look stick. Win32Window reads the pad and sends the motors; this file says
// what they do. Every strength here is provisional: a player with cerebral palsy is trying the controller and their
// word sets the defaults.
#include "Game.h"
#include <cmath>
#include <cstring>
#include <ostream>
namespace retro {
namespace {
float wrapPi(float a){while(a>kPi)a-=2*kPi;while(a<-kPi)a+=2*kPi;return a;}
}

// The motors: a shot kicks the light motor (and the heavy one for the big guns) for a beat, a blow taken rumbles the
// heavy one as the red flash fades, a Snatcher's hands shake it in pulses. Silent in a menu, on the title, in VR.
Vec2 Game::padRumble()const{
 const int level=std::clamp(m_settings.pad.rumble,0,2);
 if(!level||m_vrInputActive||m_titleScreen||m_paused||m_consoleOpen||m_inventoryOpen)return {};
 float low=0,high=0;
 if(m_weaponEquipped&&!unarmed()){
  struct Kick{float low,high,seconds;};
  const Kick k=m_equippedWeapon==WeaponId::Shotgun?Kick{.85f,.9f,.16f}:m_equippedWeapon==WeaponId::Sniper?Kick{.9f,1.f,.2f}:
               m_equippedWeapon==WeaponId::MachinePistol?Kick{.2f,.5f,.06f}:m_equippedWeapon==WeaponId::Handgun?Kick{.3f,.65f,.09f}:Kick{0,0,0};
  if(k.seconds>0&&m_shotAge>=0&&m_shotAge<k.seconds){const float fade=1.f-m_shotAge/k.seconds;low=std::max(low,k.low*fade);high=std::max(high,k.high*fade);}
 }
 low=std::max(low,std::clamp(m_damageFlash,0.f,1.f)*.8f);
 if(m_grabbed>0){const float pulse=std::sin(m_elapsed*18.f)>0?.55f:.2f;low=std::max(low,pulse);}
 const float scale=level==1?.45f:1.f;
 return {std::clamp(low*scale,0.f,1.f),std::clamp(high*scale,0.f,1.f)};
}

// A prompt with the keyboard's keys named as the pad's buttons, as the pad is laid out (Win32Window::input and the
// FIRE AND AIM ON setting): E use, LMB/FIRE/CLICK fire, RIGHT CLICK aim, Q subdue, R reload, G the Physgloves,
// F the torch, I the inventory, ESC back (START on the death screen), ENTER choose, ARROWS the D-pad. Only whole
// words change, so "E / OPEN BULKHEAD" becomes "RB / OPEN BULKHEAD" and nothing inside a word is touched.
std::string Game::padPrompt(const char* text)const{
 if(!text)return {};
 std::string s=text;if(!m_padPrompts)return s;
 const bool bumpers=m_settings.pad.bumpers;
 const char* fire=bumpers?"RB":"RT";const char* aim=bumpers?"LB":"LT";const char* use=bumpers?"RT":"RB";const char* glove=bumpers?"LT":"LB";
 auto phrase=[&](const char* from,const char* to){for(size_t at=s.find(from);at!=std::string::npos;at=s.find(from,at+std::strlen(to)))s.replace(at,std::strlen(from),to);};
 phrase("PRESS ESC","PRESS START");phrase("RIGHT CLICK",aim);phrase("CLICK / ARROWS / ENTER","D-PAD / A");
 struct Word{const char* key;const char* button;};
 const Word words[]={{"E",use},{"LMB",fire},{"FIRE",fire},{"CLICK",fire},{"Q","Y"},{"R","X"},{"G",glove},{"F","R3"},{"I","D-PAD DOWN"},{"ESC","B"},{"ENTER","A"},{"ARROWS","D-PAD"}};
 std::string out;out.reserve(s.size()+8);
 for(size_t i=0;i<s.size();){
  auto wordChar=[&](char c){return (c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='\'';};
  if(!wordChar(s[i])){out+=s[i++];continue;}
  size_t j=i;while(j<s.size()&&wordChar(s[j]))++j;
  const std::string w=s.substr(i,j-i);const char* swap=nullptr;
  for(const auto& k:words)if(w==k.key){swap=k.button;break;}
  out+=swap?std::string(swap):w;i=j;
 }
 return out;
}

// Aim assist on the look stick (pad only, never the mouse, never VR): the nearest hostile the crosshair is near and
// can see, within 30 m. Over it the stick turns the view slower (friction: easier to stop on it), and while the
// player steers, the view drifts toward its middle (the pull follows a target the stick is already tracking; with
// the stick at rest nothing moves, so it never aims for the player). Light: friction .35, pull 2.4/s; strong: .55, 4/s.
Vec2 Game::aimAssist(Vec2 stickLook,float dt)const{
 (void)dt;const int level=std::clamp(m_settings.pad.assist,0,2);
 if(!level||m_vrInputActive||dead())return stickLook;
 const Point3 eye{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye};const float pitch=m_player.pitch/140.f;
 float best=1;Vec2 error{};
 auto consider=[&](Vec2 at,float z,float radius){
  const Vec2 d=at-m_player.pos;const float flat=length(d);if(flat<.5f||flat>30.f)return;
  const float yawError=wrapPi(std::atan2(d.y,d.x)-m_player.angle),pitchError=std::atan2(z-eye.z,flat)-pitch;
  const float zone=std::clamp(2.2f*std::atan2(radius,flat),.026f,.14f);   // 1.5 to 8 degrees
  const float e=std::sqrt(yawError*yawError+pitchError*pitchError)/zone;if(e>=best)return;
  if(!m_world.rayClear(m_player.pos,eye.z,at,z))return;
  best=e;error={yawError,pitchError};};
 for(const auto& e:m_enemies)if(e.alive&&factionsHostile(e.faction,AiFaction::Player))consider(e.pos,(e.bodyBottom()+e.bodyTop())*.5f,.45f);
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto& npc=m_friendlyActors[i];if(npc.level!=m_level)continue;const auto& track=npc.track();
  if(track.ai.mode!=ActorAiMode::Guard||!track.ai.armed||!autonomousActor(track)||npcIncapacitated(i)||(track.deadState&&state(track.deadState))||!guardHostileToPlayer(i))continue;
  consider(npc.pose.position,npc.pose.z+track.scale*.62f,.35f);}
 if(m_matchView.active&&m_matchView.mode==net::Mode::Deathmatch)for(const auto& r:m_remotePlayers)if(r.alive)
  consider({r.position.x,r.position.y},r.position.z+(r.prone?.25f:r.crouched?.7f:1.1f),.35f);
 if(best>=1)return stickLook;
 const float close=1-best,friction=level==2?.55f:.35f,pull=level==2?4.f:2.4f;
 Vec2 out=stickLook*(1-friction*close);
 const float steering=std::clamp(length(stickLook)*4.f,0.f,1.f);
 out.x+=error.x*pull*close*steering;out.y+=error.y*pull*close*steering;
 return out;
}

// --each-test=controller: the motors, the prompts and aim assist, numerically.
bool Game::testController(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 // Prompts.
 {Game g;g.m_padPrompts=false;check(g.padPrompt("E / OPEN BULKHEAD")=="E / OPEN BULKHEAD","with the keyboard in use the prompt keeps its keys");
  g.m_padPrompts=true;
  check(g.padPrompt("E / OPEN BULKHEAD")=="RB / OPEN BULKHEAD","with the pad in use E reads RB");
  check(g.padPrompt("E / STEP OUT   LMB / KNOCK")=="RB / STEP OUT   RT / KNOCK","LMB reads RT");
  check(g.padPrompt("E / RELEASE    Q / KNOCK OUT    FIRE / KILL")=="RB / RELEASE    Y / KNOCK OUT    RT / KILL","Q reads Y, FIRE reads RT");
  check(g.padPrompt("PRESS ESC TO RESTART")=="PRESS START TO RESTART","the death screen asks for START");
  check(g.padPrompt("RIGHT CLICK IN WORLD TO GUARD")=="LT IN WORLD TO GUARD","right click reads LT");
  check(g.padPrompt("E / READ SHIFT LOG")=="RB / READ SHIFT LOG"&&g.padPrompt("ELEVATOR")=="ELEVATOR","only whole words change");
  g.m_settings.pad.bumpers=true;check(g.padPrompt("E / DROP     FIRE / PUNT")=="RT / DROP     RB / PUNT","FIRE AND AIM ON BUMPERS swaps the names too");}
 // Rumble.
 {Game g=weaponInspection(2,.02f);g.m_titleScreen=false;g.m_paused=false;g.m_settings.pad.rumble=2;const Vec2 shot=g.padRumble();
  check(shot.x>.5f&&shot.y>.5f,"a shotgun shot kicks both motors ("+std::to_string(shot.x)+", "+std::to_string(shot.y)+")");
  g.m_shotAge=1;check(g.padRumble().x==0&&g.padRumble().y==0,"a beat later they are still");
  g.m_damageFlash=1;check(g.padRumble().x>.7f,"a blow taken rumbles the heavy motor");
  g.m_settings.pad.rumble=1;check(g.padRumble().x<.4f&&g.padRumble().x>0,"LIGHT is softer");
  g.m_settings.pad.rumble=0;check(g.padRumble().x==0,"OFF is silent");
  g.m_settings.pad.rumble=2;g.m_paused=true;check(g.padRumble().x==0,"silent in the pause menu");}
 // Aim assist: a creature 8 m ahead and a little to the side.
 {Game g=validationScene(Enemy::Kind::Brute,-1);g.m_god=true;g.m_settings.pad.assist=1;g.m_titleScreen=false;
  if(g.m_enemies.empty()){check(false,"aim assist: the scene has a creature");return ok;}
  auto& e=g.m_enemies[0];e.alive=true;e.hp=std::max(e.hp,200.f);const Vec2 to=e.pos-g.m_player.pos;const float mid=(e.bodyBottom()+e.bodyTop())*.5f;
  g.m_player.angle=std::atan2(to.y,to.x)+.02f;g.m_player.pitch=std::atan2(mid-(g.m_player.z+g.m_player.eye),length(to))*140.f;
  const Vec2 stick{-.8f,0};const Vec2 over=g.aimAssist(stick,1.f/60);
  report<<"  creature at "<<e.pos.x<<','<<e.pos.y<<" z "<<mid<<" alive "<<e.alive<<" hostile "<<factionsHostile(e.faction,AiFaction::Player)<<" player "<<g.m_player.pos.x<<','<<g.m_player.pos.y<<" clear "<<g.m_world.rayClear(g.m_player.pos,g.m_player.z+g.m_player.eye,e.pos,mid)<<'\n';
  report<<"  stick "<<stick.x<<" over the target "<<over.x<<','<<over.y<<'\n';
  check(std::fabs(over.x)<std::fabs(stick.x)||over.x<stick.x,"over a target the stick turns slower or is pulled onto it");
  const Vec2 rest=g.aimAssist({0,0},1.f/60);check(rest.x==0&&rest.y==0,"the stick at rest: nothing moves (it never aims for you)");
  g.m_player.angle+=1.2f;const Vec2 away=g.aimAssist(stick,1.f/60);check(away.x==stick.x&&away.y==stick.y,"looking well away: the stick is untouched");
  g.m_player.angle-=1.2f;g.m_settings.pad.assist=0;const Vec2 off=g.aimAssist(stick,1.f/60);check(off.x==stick.x&&off.y==stick.y,"ASSIST OFF: untouched");
  g.m_settings.pad.assist=2;const Vec2 strong=g.aimAssist(stick,1.f/60);g.m_settings.pad.assist=1;const Vec2 light=g.aimAssist(stick,1.f/60);
  check(std::fabs(strong.x-stick.x)>std::fabs(light.x-stick.x),"STRONG helps more than LIGHT");
  // Driving the view with the stick past the target: with assist it ends nearer the target's middle.
  auto sweep=[&](int assist){Game h=g;h.m_settings.pad.assist=assist;h.m_player.angle=std::atan2(to.y,to.x)+.12f;float nearest=9;
   for(int i=0;i<40;++i){InputState in{};in.padActive=true;in.controllerLook={-.5f,0};h.update(in,1.f/60);nearest=std::min(nearest,std::fabs(wrapPi(std::atan2(to.y,to.x)-h.m_player.angle)));}
   return std::pair{nearest,std::fabs(wrapPi(std::atan2(to.y,to.x)-h.m_player.angle))};};
  const auto plain=sweep(0),helped=sweep(1);
  report<<"  sweep past: without assist ends "<<plain.second<<" rad off, with assist "<<helped.second<<" rad off\n";
  check(helped.second<plain.second,"swept past at the same stick, the assisted view lingers nearer the target");}
 return ok;
}
}
