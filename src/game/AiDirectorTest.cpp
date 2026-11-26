// The Director and the AI blocks (--each-test=ai-director; the Builder's AI and Entity Manager writes these records): an AI record over half a room, a lamp inside its zone
// and one outside, and a block wired to the Director's beat: the lamps go dark for two seconds, a noise behind the
// player that monsters within 10 m hear, and a door state set a second and a half later. The player walks in, the
// menace climbs, a beat comes after the calm, the blocks act on it, and it all comes back from a save.
#include "Game.h"
#include "../world/CampaignExport.h"
#include "../world/CustomCampaign.h"
#include <cmath>
#include <fstream>
namespace retro {
bool Game::testAiDirector(){
 std::ofstream report("ai-director-test.txt");int failed=0;
 auto check=[&](bool ok,const std::string& text){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 try{
  std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\nCAMPAIGN|Director|0\nMAP|0|Engineering|0|0|3|20|0|0|6|0.8|industrial_night|0|0|0|0|0|-1|0|0|0\nLAYER|0|0|Floor|0|0\n";
  // Row 6 a wall with a doorway at x 8-10 (the lockdown's door, gated on engineering_open).
  for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+(y==0||y==23?std::string(24,'#'):y==6?std::string(8,'#')+".."+std::string(14,'#'):"#"+std::string(22,'.')+"#")+'\n';
  t+="DOOR|0|8|10|6.5|0|0|0|1|0|-1|engineering_open|1|0|1|1|1|0\n";
  // The Director over the east half (x 12-23); calm 5 s, a beat at the latest 10 s after the last.
  t+="AI|0|director|director|12|1|23|23|-5|10|pressure=0.8|calm=5|peak=10\n";
  t+="LIGHT|0|16|12|3\nLIGHT|0|6|12|3\n";
  t+="SCRIPT_EVENT|0|on_beat|0|0|24|24|-100|100||1|0\nSCRIPT_REQUIRE|0|on_beat|director_beat|>=|1\nSCRIPT_TRIGGER|0|on_beat|map|all|0\n"
     "SCRIPT_ACTION|0|on_beat|LIGHTS|director|dark|2\nSCRIPT_ACTION|0|on_beat|NOISE|metal_drag|1|behind|10\nSCRIPT_ACTION|0|on_beat|LATER|engineering_locked|1|1.5\n";
  // The rhythm (a block on every beat, one only on the fights) and a lockdown that seals the door as the player comes in.
  t+="SCRIPT_EVENT|0|on_any_beat|0|0|24|24|-100|100||1|0\nSCRIPT_REQUIRE|0|on_any_beat|director_pulse|=|1\nSCRIPT_TRIGGER|0|on_any_beat|map|all|1\nSCRIPT_ACTION|0|on_any_beat|ADD|beats_seen|1\n"
     "SCRIPT_EVENT|0|on_fight|0|0|24|24|-100|100||1|0\nSCRIPT_REQUIRE|0|on_fight|director_pulse|=|1\nSCRIPT_REQUIRE|0|on_fight|director_mood|=|1\nSCRIPT_TRIGGER|0|on_fight|map|all|1\nSCRIPT_ACTION|0|on_fight|ADD|fights|1\n"
     "SCRIPT_EVENT|0|lockdown|12|1|23|23|-5|10||1|0\nSCRIPT_TRIGGER|0|lockdown|box|all|0\nSCRIPT_ACTION|0|lockdown|SEAL|engineering_open|0\n";
  t+="--- CUSTOM_CAMPAIGN_DATA_END ---\n";
  auto campaign=loadCustomCampaignText(t,"director.rmc");
  check(campaign!=nullptr,"the AI record and the LIGHTS, NOISE and LATER blocks load");
  Game g(campaign);g.m_enemies.clear();g.update({},1.f/60);
  const auto& lights=g.m_world.lights();
  check(lights.size()>=2&&lights[0].directed==0&&lights[1].directed<0,"the lamp in the Director's zone answers to it, the one outside does not");
  auto step=[&](float seconds){for(float s=0;s<seconds;s+=1.f/60)g.update({},1.f/60);};
  // Outside the zone: nothing climbs.
  g.m_player.pos={5,12};g.m_player.z=0;step(3);
  check(g.state(stateId("director_inside"))==0&&g.state(stateId("director_tension"))==0,"outside its zone the menace stays at 0");
  // In the zone: the menace climbs, and after the calm a beat comes.
  g.m_player.pos={18,12};g.m_player.angle=0;size_t soundsBefore=0;
  float beatAt=-1;for(float s=0;s<20&&beatAt<0;s+=1.f/60){soundsBefore=g.m_sounds.size();g.update({},1.f/60);if(g.state(stateId("director_beat"))>=1)beatAt=s;}
  check(g.state(stateId("director_inside"))==1,"in its zone the Director counts the player in");
  check(beatAt>=5.f&&beatAt<=10.5f,"a beat comes after the calm and by the peak ("+std::to_string(beatAt)+" s)");
  bool noise=false;for(const auto& e:g.m_sounds)if(e.aiRadius>=9.f&&e.aiRadius<=11.f&&e.position.x<18.f)noise=true;(void)soundsBefore;
  check(noise,"the NOISE block plays behind the player, heard by monsters within 10 m");
  check(g.lampPower(g.m_world.lights()[0])==0&&g.lampPower(g.m_world.lights()[1])>0,"the LIGHTS block darkens the zone's lamp, not the one outside");
  check(g.state(stateId("engineering_locked"))==0,"the LATER block waits");
  step(1.7f);
  check(g.state(stateId("engineering_locked"))==1,"the LATER block sets its state after 1.5 s");
  step(.6f);
  check(g.lampPower(g.m_world.lights()[0])>0,"the zone's lamp comes back when the LIGHTS block's two seconds are up");
  check(g.state(stateId("director_tension"))>0&&g.state(stateId("director_tension"))<100,"the menace is a 0-100 gauge ("+std::to_string(g.state(stateId("director_tension")))+")");
  // The rhythm: quiet when the player came in, a fight on the first beat, quiet on the next, a fight again.
  check(g.state(stateId("beats_seen"))==1&&g.state(stateId("fights"))==1&&g.state(stateId("director_mood"))==1,"the first beat's pulse fires a repeating block once and turns the rhythm to a fight");
  check(g.state(stateId("director_pulse"))==0,"the beat's pulse lasts one frame");
  // The lockdown: the open door gated on engineering_open swung shut when the player came into the zone.
  {const auto& d=g.m_world.doors().at(0);check(!d.opening&&d.open<.9f&&g.doorLocked(d),"the SEAL block swings the open, gated door shut and it stays locked ("+std::to_string(d.open)+")");}
  {bool second=false;for(float s=0;s<24&&g.state(stateId("beats_seen"))<3;s+=1.f/60){g.update({},1.f/60);
    if(!second&&g.state(stateId("beats_seen"))==2){second=true;check(g.state(stateId("director_mood"))==2&&g.state(stateId("fights"))==1,"the second beat turns it quiet and the fight block waits");}}
   check(second&&g.state(stateId("beats_seen"))==3&&g.state(stateId("fights"))==2&&g.state(stateId("director_mood"))==1,"the third beat is a fight again (beats "+std::to_string(g.state(stateId("beats_seen")))+", fights "+std::to_string(g.state(stateId("fights")))+")");}
  // A SEAL record writes back as SEAL.
  {const auto text=exportRmc("Director",{{campaign->maps.at(0)}}).text;check((text.find("|SEAL|engineering_open|0")!=std::string::npos||text.find("|SEAL|#"+std::to_string(stateId("engineering_open"))+"|0")!=std::string::npos),"the exporter writes the SEAL block by name");}
 }catch(const std::exception& e){check(false,std::string("exception: ")+e.what());}
 report<<(failed?"FAIL":"PASS")<<'\n';return failed==0;
}
}
