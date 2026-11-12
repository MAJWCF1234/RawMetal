// Doors along Y (--each-test=door-axis): a door standing in a wall that runs
// north-south, read through Door's own frame everywhere. A 24 m room split by
// a wall at x 12-13 with a two-metre gap at y 10-12; the door hangs in it.
#include "Game.h"
#include "../world/CampaignExport.h"
#include "../world/CustomCampaign.h"
#include <cmath>
#include <fstream>
#include <sstream>
namespace retro {
namespace {
// DOOR fields 0-22 (CustomCampaign.cpp); unnamed ones are left blank.
std::string doorRecord(bool swinging,const std::string& leaf,const std::string& label,const std::string& axis,const std::string& hinge,const std::string& swing="",const std::string& state=""){
 std::string f[23]={"DOOR","0","10","12","12.5","0","0","0",swinging?"1":"0","0","-1",state,state.empty()?"":"1","",swing,"","","",leaf,label,"",axis,hinge};
 std::string out=f[0];for(int i=1;i<23;++i)out+="|"+f[i];return out;
}
std::shared_ptr<const CustomCampaign> axisCampaign(const std::string& door){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Door axis|0\nMAP|0|Split|0|0|6|11|0|0|3.5|0.7|industrial_night|0|0|0|0|0|-1|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int y=0;y<24;++y){std::string row=y<4||y>19?std::string(24,'#'):"#"+std::string(22,'.')+"#";if(y>=4&&y<=19&&y!=10&&y!=11)row[12]='#';t+="ROW|0|0|"+std::to_string(y)+"|"+row+'\n';}
 return loadCustomCampaignText(t+door+"\n--- CUSTOM_CAMPAIGN_DATA_END ---\n","door-axis.rmc");
}
}
bool Game::testDoorAxis(){
 std::ofstream report("door-axis-test.txt");int failed=0;
 auto check=[&](bool ok,const std::string&text){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 try{
  // The frame itself: along Y, left/right are y and the y field is the wall's x.
  {Door d;d.left=10;d.right=12;d.y=12.5f;d.alongY=true;d.swinging=true;
   const Vec2 c=d.centre(),h=d.hinge(),shut=d.leafAxis(0),open=d.leafAxis(1);
   check(std::fabs(c.x-12.5f)<1e-4f&&std::fabs(c.y-11)<1e-4f&&std::fabs(h.x-12.5f)<1e-4f&&std::fabs(h.y-10)<1e-4f&&std::fabs(shut.y-1)<1e-4f&&std::fabs(open.x-1)<1e-4f,
    "Along Y the door centres at x 12.5, y 11, hangs from y 10, shuts along +y and swings toward +x");
   d.hingeRight=true;d.swing=-1;const Vec2 h2=d.hinge(),s2=d.leafAxis(0),o2=d.leafAxis(1);
   check(std::fabs(h2.y-12)<1e-4f&&std::fabs(s2.y+1)<1e-4f&&std::fabs(o2.x+1)<1e-4f,"Hinged at right and swung to -1 it hangs from y 12 and opens toward -x");}

  // The record: parse, write, read back; old records stay along X.
  auto parsed=axisCampaign(doorRecord(true,"","DOOR","1",""));const auto& door=parsed->maps.at(0)->doors.at(0);
  check(door.alongY&&!door.hingeRight&&door.swinging&&door.label=="DOOR","DOOR field 21 stands the door along Y");
  auto written=exportRmc("Door axis",{{parsed->maps.at(0)}}).text;
  check(written.find("|DOOR||1\n")!=std::string::npos,"The exporter writes the axis");
  auto again=loadCustomCampaignText(written,"door-axis-again.rmc");
  check(again->maps.at(0)->doors.at(0).alongY&&std::fabs(again->maps.at(0)->doors.at(0).y-12.5f)<1e-4f,"A written Y door reads back along Y");
  {auto hinged=axisCampaign(doorRecord(true,"","","1","1","-1"));const auto& h=hinged->maps.at(0)->doors.at(0);auto text=exportRmc("Door axis",{{hinged->maps.at(0)}}).text;
   auto back=loadCustomCampaignText(text,"door-axis-hinge.rmc");const auto& b=back->maps.at(0)->doors.at(0);
   check(h.hingeRight&&h.swing==-1&&b.alongY&&b.hingeRight&&b.swing==-1,"Field 22 hangs the leaf from right, and swing and hinge survive a write");}
  {auto old=axisCampaign("DOOR|0|10|12|12.5|0|0|0|1|0|-1");auto text=exportRmc("Door axis",{{old->maps.at(0)}}).text;
   const auto at=text.find("DOOR|0|");const std::string line=at==std::string::npos?"":text.substr(at,text.find('\n',at)-at);
   check(!old->maps.at(0)->doors.at(0).alongY&&line=="DOOR|0|10|12|12.5|0|0|0|1|0|-1||1","An old record stays along X and writes back with no axis ("+line+")");}
  {bool refused=false;try{axisCampaign("DOOR|0|10|12|12.5|0|0|1|0|0|-1|||||||||||1");}catch(const std::exception&){refused=true;}
   check(refused,"A seam (transfer) door along Y is refused");}

  // Collision, shut and open, swinging.
  Game g(parsed);g.m_enemies.clear();g.update({},1.f/60);
  auto& w=g.m_world;
  check(w.doorBlocks(12.5f,11,0,1.7f)&&w.doorBlocks(12.5f,10.4f,0,1.7f)&&!w.doorBlocks(11.6f,11,0,1.7f)&&!w.doorBlocks(13.4f,11,0,1.7f),"Shut, the leaf fills the gap and nothing either side of it");
  check(!w.rayClear({10,11},1.5f,{15,11},1.5f,true,true,false,true)&&!g.soundRayClear({10,11},1.5f,{15,11},1.5f,.25f,.25f),"Shut, it stops sight and sound across the wall");
  w.setDoor(0,1,true);
  check(!w.doorBlocks(12.5f,11,0,1.7f)&&w.doorBlocks(13.5f,10.02f,0,1.7f)&&!w.doorBlocks(11.5f,10.02f,0,1.7f),"Open, the gap is clear and the leaf lies against +x");
  check(w.rayClear({10,11},1.5f,{15,11},1.5f,true,true,false,true)&&g.soundRayClear({10,11},1.5f,{15,11},1.5f,.25f,.25f),"Open, sight and sound pass");
  w.setDoor(0,0,false);

  // The player's prompt, from either side, and not from along the wall.
  g.m_player.pos={11.3f,11};g.m_player.z=0;g.m_player.angle=0;
  {const char* hint=g.interactionHint();check(g.nearbyInteractionDoor()==0&&hint&&std::string(hint)=="E / OPEN DOOR","West of the door facing it, the prompt offers it ("+std::string(hint?hint:"none")+")");}
  g.m_player.pos={13.7f,11};g.m_player.angle=kPi;
  check(g.nearbyInteractionDoor()==0,"East of it facing back, the same door");
  g.m_player.pos={6,11};g.m_player.angle=0;
  check(g.nearbyInteractionDoor()<0,"Six metres off, no door");
  g.m_player.pos={11.3f,11};g.m_player.angle=0;
  {InputState use{};use.use=true;g.update(use,1.f/60);for(int i=0;i<180;++i)g.update({},1.f/60);
   check(g.world().doors()[0].open>.85f,"E opens it");}

  // Save and load: the door's state by index, its axis from the map.
  {w.setDoor(0,.4f,false);const auto data=g.encodeSave();Game b(parsed);const bool loaded=b.decodeSave(data);
   check(loaded&&std::fabs(b.world().doors()[0].open-.4f)<.01f&&!b.world().doors()[0].opening&&b.world().doors()[0].alongY,"A save keeps how far it stands open; the load keeps it along Y");}

  // Sliding doors along Y: the bulkhead rises clear; the grille lets sight and sound through.
  {Game s(axisCampaign(doorRecord(false,"","","1","")));s.m_enemies.clear();auto& sw=s.m_world;
   check(sw.doorBlocks(12.5f,11,0,1.7f)&&!sw.doorBlocks(11.6f,11,0,1.7f),"A shut sliding door along Y blocks the gap, not the floor beside it");
   sw.setDoor(0,1,true);check(!sw.doorBlocks(12.5f,11,0,1.7f),"Raised, the gap is clear");}
  {Game s(axisCampaign(doorRecord(false,"1","GATE","1","")));s.m_enemies.clear();auto& sw=s.m_world;
   check(sw.doorBlocks(12.5f,11,0,1.7f)&&!sw.doorBlocks(12.5f,11,0,1.7f,true),"A grille along Y stops a body, not the eye");
   check(sw.rayClear({10,11},1.5f,{15,11},1.5f,true,true,false,true)&&s.soundRayClear({10,11},1.5f,{15,11},1.5f,.25f,.25f),"Sight and sound pass between its bars");}

  // A guard opens it on his way through, from either side, and only in front of it.
  {Game a(parsed);a.m_enemies.clear();
   a.requestAiDoor(a.m_world,a.level(),{11.4f,11},0,{9,11});check(!a.world().doors()[0].opening,"A guard walking away from the door leaves it");
   a.requestAiDoor(a.m_world,a.level(),{11.4f,8},0,{14,8});check(!a.world().doors()[0].opening,"A guard beside the wall, off the door's span, leaves it");
   a.requestAiDoor(a.m_world,a.level(),{13.6f,11},0,{9,11});check(a.world().doors()[0].opening,"A guard east of it heading west opens it");}

  // A creature that works doors routes through it and opens it; the shut, locked one holds.
  auto hunt=[&](const std::shared_ptr<const CustomCampaign>& c,float& reached,bool& opened){Game h(c);h.m_enemies.clear();h.m_enemies.resize(1);auto& e=h.m_enemies[0];
   e.kind=Enemy::Kind::Transformed;e.hp=e.maxHp=creatureTraits(e.kind).health;e.pos={17,11};e.z=0;e.home=e.pos;e.heading=kPi;e.awareness=60;e.recognition=1;
   h.m_player.pos={6,11};h.m_player.z=0;h.m_player.health=100000;e.lastKnown=h.m_player.pos;e.lastKnownZ=0;reached=e.pos.x;opened=false;
   for(int i=0;i<2400&&e.pos.x>11;++i){h.m_player.health=100000;h.update({},1.f/120);reached=std::min(reached,e.pos.x);opened|=h.world().doors()[0].opening;}};
  {float reached;bool opened;hunt(parsed,reached,opened);
   check(opened&&reached<11.5f,"A transformed creature hunting across the wall opens the Y door and comes through (reached x "+std::to_string(reached)+")");}
  {float reached;bool opened;hunt(axisCampaign(doorRecord(false,"","","1","","","axis_lock")),reached,opened);
   check(!opened&&reached>12.8f,"A locked Y door holds it (reached x "+std::to_string(reached)+")");}
 }catch(const std::exception&e){check(false,std::string("fixture: ")+e.what());}
 report<<(failed?"FAILED\n":"PASSED\n");return failed==0;
}
// Doors in play (--each-test=doors), on the same split room's Y door: a leaf's
// weight, the hand left on it, a shove, its sounds, a door beaten down, sight
// through a door ajar, and the strength and lock fields.
bool Game::testDoors(){
 std::ofstream report("doors-test.txt");int failed=0;
 auto check=[&](bool ok,const std::string&text){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 auto fixed=[](float v){std::ostringstream s;s.precision(3);s<<std::fixed<<v;return s.str();};
 try{
  auto parsed=axisCampaign(doorRecord(true,"","DOOR","1",""));
  // Weight: it eases off the latch, knocks its stop, settles back onto it; shut, it latches.
  {Game g(parsed);g.m_enemies.clear();auto& w=g.m_world;w.setDoor(0,0,true);
   float at01=0,peak=0,dip=1;bool stop=false,rebound=false;int hits=0;
   for(int i=1;i<=240;++i){w.updateDoors(1.f/120);const auto& d=w.doors()[0];hits|=w.takeDoorHit(0);if(i==12)at01=d.open;
    if(d.open>=d.maxOpen-.001f)stop=true;else if(stop){rebound=true;dip=std::min(dip,d.open);}peak=std::max(peak,d.open);}
   const auto& d=w.doors()[0];
   check(at01>0&&at01<.06f,"It eases off the latch: "+fixed(at01)+" open after 0.1 s (a steady drive was 0.085)");
   check(stop&&rebound&&dip>d.maxOpen-.05f&&std::fabs(d.open-d.maxOpen)<.002f,"It knocks its stop, comes back off it a little (to "+fixed(dip)+") and settles there");
   check((hits&Door::HitStart)&&(hits&Door::HitStop),"Leaving the latch and striking the stop are both told");
   w.toggleDoor(0);hits=0;float t=0;for(int i=0;i<360&&w.doors()[0].open>0;++i){w.updateDoors(1.f/120);hits|=w.takeDoorHit(0);t+=1.f/120;}
   check(w.doors()[0].open==0&&(hits&Door::HitLatch)&&!(hits&Door::HitSlam)&&t>.7f&&t<1.6f,"Shut by the hand it latches, not slams, in "+fixed(t)+" s");
   w.setDoor(0,0,true);w.updateDoors(2.f);check(std::fabs(w.doors()[0].open-w.doors()[0].maxOpen)<.002f,"One long step lands where the frames would");}
  // The hand left on it: held, it eases and stays where it is let go; a tap swings it all the way.
  {Game g(parsed);g.m_enemies.clear();g.m_player.pos={11.3f,11};g.m_player.z=0;g.m_player.angle=0;
   InputState hold{};hold.use=true;for(int i=0;i<36;++i)g.update(hold,1.f/60);for(int i=0;i<180;++i)g.update({},1.f/60);
   const float ajar=g.world().doors()[0].open;
   check(ajar>.08f&&ajar<.5f&&g.world().doors()[0].rest>=0,"Held 0.6 s and let go, it stays ajar at "+fixed(ajar));
   const auto data=g.encodeSave();Game b(parsed);b.m_enemies.clear();const bool loaded=b.decodeSave(data);for(int i=0;i<120;++i)b.update({},1.f/60);
   check(loaded&&std::fabs(b.world().doors()[0].open-ajar)<.02f,"A save keeps it ajar (version "+std::to_string(SaveVersion)+")");
   // Through the crack: more of the far room is in sight the wider it stands.
   auto seen=[&](float open){Game s(parsed);s.m_enemies.clear();s.m_world.setDoor(0,open,false);s.m_world.restDoor(0,open);int clear=0;
    for(int k=0;k<40;++k){const Vec2 far{16.f,6.f+k*.25f};clear+=s.world().rayClear({11.4f,11.6f},1.5f,far,1.5f,true,true,false,true);}return clear;};
   const int crack=seen(.25f),half=seen(.5f),wide=seen(.9f);
   check(crack<half&&half<wide,"Sight through it grows with how far it stands open (a quarter, half, all the way): "+std::to_string(crack)+" < "+std::to_string(half)+" < "+std::to_string(wide)+" of 40 lines");
   Game tap(parsed);tap.m_enemies.clear();tap.m_player.pos={11.3f,11};tap.m_player.z=0;tap.m_player.angle=0;InputState use{};use.use=true;tap.update(use,1.f/60);for(int i=0;i<150;++i)tap.update({},1.f/60);
   check(tap.world().doors()[0].open>tap.world().doors()[0].maxOpen-.01f,"A tap of E swings it all the way");}
  // A shove: a shut leaf is latched; one left ajar, walked into from the side it opens away
  // from, swings open; at a run it bangs its stop.
  {InputState walk{};walk.forward=true;
   Game shut(parsed);shut.m_enemies.clear();shut.m_player.pos={11.6f,11};shut.m_player.z=0;shut.m_player.angle=0;
   for(int i=0;i<60;++i)shut.update(walk,1.f/60);check(shut.world().doors()[0].open<.01f,"Shut, it is latched: walking into it does not open it");
   Game g(parsed);g.m_enemies.clear();g.m_world.setDoor(0,.12f,false);g.m_world.restDoor(0,.12f);g.m_player.pos={11.6f,11};g.m_player.z=0;g.m_player.angle=0;
   for(int i=0;i<60;++i)g.update(walk,1.f/60);check(g.world().doors()[0].open>.3f,"Left ajar and walked into from the push side, it swings open ("+fixed(g.world().doors()[0].open)+")");
   Game pull(parsed);pull.m_enemies.clear();pull.m_world.setDoor(0,.12f,false);pull.m_world.restDoor(0,.12f);pull.m_player.pos={13.6f,11};pull.m_player.z=0;pull.m_player.angle=kPi;
   for(int i=0;i<60;++i)pull.update(walk,1.f/60);check(pull.world().doors()[0].open<.15f,"From the side it opens toward, walking into it does not swing it: it wants the handle");
   Game slam(parsed);slam.m_enemies.clear();slam.m_world.shoveDoor(0,2.6f);int hits=0;for(int i=0;i<120;++i){slam.m_world.updateDoors(1.f/120);hits|=slam.m_world.takeDoorHit(0);}
   check(hits&Door::HitSlam,"Shoved at a run it bangs off its stop");
   slam.m_sounds.clear();slam.m_world.shoveDoor(0,0);slam.m_world.setDoor(0,0,false);slam.m_world.shoveDoor(0,2.6f);for(int i=0;i<60;++i)slam.updateDoorPlay({},1.f/120),slam.m_world.updateDoors(1.f/120);
   bool loud=false,creak=false;for(const auto& s:slam.m_sounds){loud|=s.sound==Sound::LockerBang&&s.aiRadius>=16;creak|=s.sound==Sound::LockerCreak;}
   check(loud&&creak,"The bang is a sound creatures hear across the room; leaving the latch creaks");
   Game locked(axisCampaign(doorRecord(true,"","DOOR","1","","","doors_lock")));locked.m_enemies.clear();locked.m_player.pos={11.6f,11};locked.m_player.z=0;locked.m_player.angle=0;
   for(int i=0;i<60;++i)locked.update(walk,1.f/60);check(locked.world().doors()[0].open<.01f,"A locked one does not give to a shove");}
  // Battering: a door that gives is beaten down; one that does not, holds.
  auto brute=[&](const std::string& record,bool& broken,float& reached,int& blows){Game h(axisCampaign(record));h.m_enemies.clear();h.m_enemies.resize(1);auto& e=h.m_enemies[0];
   e.kind=Enemy::Kind::Brute;e.hp=e.maxHp=280;e.pos={16,11};e.z=0;e.home=e.pos;e.heading=kPi;e.awareness=60;e.recognition=1;
   h.m_player.pos={6,11};h.m_player.z=0;e.lastKnown=h.m_player.pos;e.lastKnownZ=0;reached=e.pos.x;blows=0;
   for(int i=0;i<3600&&e.pos.x>11;++i){h.m_player.health=100000;e.awareness=60;e.lastKnown=h.m_player.pos;const float before=h.world().doors()[0].damage;h.update({},1.f/120);if(h.world().doors()[0].damage>before)++blows;reached=std::min(reached,e.pos.x);}
   broken=h.world().doors()[0].broken;};
  {std::string giving=doorRecord(true,"","DOOR","1","","","doors_lock");giving+="|120";bool broken;float reached;int blows;brute(giving,broken,reached,blows);
   check(broken&&blows>=2&&reached<11.5f,"A Brute beats a locked door that gives down in "+std::to_string(blows)+" blows and comes through (x "+fixed(reached)+")");}
  {bool broken;float reached;int blows;brute(doorRecord(true,"","DOOR","1","","","doors_lock"),broken,reached,blows);
   check(!broken&&blows==0&&reached>12.8f,"A locked door that never gives holds it, untouched (x "+fixed(reached)+")");}
  {Game b(axisCampaign(doorRecord(true,"","DOOR","1","")+"|120"));b.m_enemies.clear();b.m_world.batterDoor(0,200,true);const auto data=b.encodeSave();
   Game same(axisCampaign(doorRecord(true,"","DOOR","1","")+"|120"));same.m_enemies.clear();const bool loaded=same.decodeSave(data);
   check(loaded&&same.world().doors()[0].broken&&!same.world().doorBlocks(12.5f,11,0,1.7f),"A broken door stays broken through a save, and no longer stops anyone");}
  // The record: strength and lock read, write back, and bad values are refused.
  {auto rec=axisCampaign(doorRecord(true,"","DOOR","1","")+"|150|1");const auto& d=rec->maps.at(0)->doors.at(0);auto text=exportRmc("Doors",{{rec->maps.at(0)}}).text;
   auto back=loadCustomCampaignText(text,"doors-again.rmc");const auto& b=back->maps.at(0)->doors.at(0);
   check(d.strength==150&&d.lock==1&&b.strength==150&&b.lock==1&&text.find("|DOOR||1||150|1\n")!=std::string::npos,"DOOR fields 23 and 24 (strength, lock) read and write back");
   bool refused=false;try{axisCampaign(doorRecord(true,"","DOOR","1","")+"||4");}catch(const std::exception&){refused=true;}check(refused,"An unknown lock is refused");}
 }catch(const std::exception&e){check(false,std::string("fixture: ")+e.what());}
 report<<(failed?"FAILED\n":"PASSED\n");return failed==0;
}
}
