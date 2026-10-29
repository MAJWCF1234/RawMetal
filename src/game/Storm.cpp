// Ash storms: a campaign's STORM cycle runs on a saved clock. The warning
// sounds the siren and tells the player to find a roof; during the storm the
// sky closes in, the ash thickens and the open ground hurts. SHELTER boxes
// are the roofs. The phase is a state (storm_phase) that scripts, dialogue
// and other systems can read: 0 calm, 1 warning, 2 storm.
#include "Game.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace retro {
namespace {
// The storm closes in and lifts over this long at either end.
constexpr float StormRampMs=8000.f;
}

const StormCycle* Game::stormCycle()const{
 return m_customCampaign&&m_customCampaign->storm.active()?&m_customCampaign->storm:nullptr;
}
int Game::stormPhase()const{return state(stateId("storm_phase"));}
float Game::stormIntensity()const{
 const auto*cycle=stormCycle();if(!cycle)return 0;
 const float clock=float(state(stateId("storm_clock"))),calm=float(cycle->calmMs),warning=float(cycle->warningMs),storm=float(cycle->stormMs);
 if(clock<calm)return 0;
 // The warning darkens the sky a little; the storm comes in over the ramp.
 if(clock<calm+warning)return .3f*(clock-calm)/std::max(1.f,warning);
 const float in=clock-calm-warning;
 return std::clamp(std::min(.3f+.7f*in/StormRampMs,(storm-in)/StormRampMs),0.f,1.f);
}
float Game::ashFall()const{
 const auto*cycle=stormCycle();if(!cycle)return 0;
 return std::clamp(cycle->ambientAsh+(1-cycle->ambientAsh)*stormIntensity(),0.f,1.f);
}
bool Game::sheltered()const{
 for(const auto&s:m_world.shelters())if(m_player.pos.x>=s.x1&&m_player.pos.x<=s.x2&&m_player.pos.y>=s.y1&&m_player.pos.y<=s.y2&&m_player.z>=s.bottom&&m_player.z<=s.top)return true;
 return false;
}
void Game::updateStorm(float dt){
 const auto*cycle=stormCycle();if(!cycle)return;
 const int length=cycle->calmMs+cycle->warningMs+cycle->stormMs,ms=std::max(0,int(std::round(dt*1000)));
 const int clock=(state(stateId("storm_clock"))+ms)%length;setState(stateId("storm_clock"),clock);
 const int phase=clock<cycle->calmMs?0:clock<cycle->calmMs+cycle->warningMs?1:2,previous=stormPhase();
 if(phase!=previous){setState(stateId("storm_phase"),phase);
  if(phase==1){sound(Sound::AlarmDistant,.9f);m_pickupNotice="ASH STORM COMING / GET UNDER A ROOF";m_pickupNoticeTime=5;}
  if(phase==2){sound(Sound::Alarm,.6f,.8f);m_pickupNotice="ASH STORM";m_pickupNoticeTime=3;}
  if(phase==0&&previous==2){m_pickupNotice="THE STORM HAS PASSED";m_pickupNoticeTime=3;}}
 // The siren keeps calling through the warning.
 if(phase==1&&(clock-cycle->calmMs)%6000<ms)sound(Sound::AlarmDistant,.7f);
 m_stormHurtTimer=std::max(0.f,m_stormHurtTimer-dt);
 if(phase!=2||sheltered()||dead())return;
 const float strength=stormIntensity();if(strength<.5f)return;
 if(!m_god)m_player.health=std::max(0.f,m_player.health-cycle->damage*strength*dt);
 m_damageFlash=std::max(m_damageFlash,.25f*strength);
 if(m_stormHurtTimer<=0){sound(Sound::Hurt,.45f,.9f);m_stormHurtTimer=1.6f;m_pickupNotice="FIND SHELTER";m_pickupNoticeTime=1.5f;}
}

bool Game::testStorm(){
 std::ofstream report("storm-test.txt");bool passed=true;int checks=0,good=0;
 auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;good+=ok;return ok;};
 std::string text="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Storm fixture|0\nSTORM|20000|6000|30000|10|0.2\nMAP|0|Yard|0|0|6.5|12|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row)text+="ROW|0|0|"+std::to_string(row)+"|"+(row==0||row==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+"\n";
 text+="SHELTER|0|2|2|6|6|-1|3\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";
 std::shared_ptr<const CustomCampaign> campaign;try{campaign=loadCustomCampaignText(text,"storm-fixture.txt");}catch(const std::exception&e){report<<e.what()<<'\n';}
 if(!check(campaign&&campaign->storm.active()&&campaign->maps[0]->shelters.size()==1,"STORM and SHELTER records parse"))return false;
 auto rejects=[&](const char*record){try{loadCustomCampaignText(std::string(text).replace(text.find("STORM|"),text.find('\n',text.find("STORM|"))-text.find("STORM|"),record),"bad.txt");return false;}catch(const std::exception&){return true;}};
 check(rejects("STORM|20000|6000|0|10|0.2")&&rejects("STORM|20000|6000|30000|10|2"),"A storm with no length or ash past 1 is refused");
 auto run=[](Game&g,float seconds){for(int i=0;i<int(seconds*20);++i)g.updateStorm(.05f);};
 {Game g(campaign);g.m_player.pos={12,12};g.m_player.z=0;
  run(g,10);check(g.stormPhase()==0&&g.stormIntensity()==0&&std::fabs(g.ashFall()-.2f)<1e-4f&&g.player().health==100,"Calm: light ash, nothing hurts");
  run(g,12);check(g.stormPhase()==1&&g.stormIntensity()>0&&g.stormIntensity()<=.3f&&g.player().health==100,"The warning darkens the sky and does no harm");
  run(g,10);check(g.stormPhase()==2&&g.stormIntensity()>.5f&&g.player().health<100,"Caught in the open, the storm hurts");
  float hurt=g.player().health;g.m_player.pos={4,4};run(g,5);check(g.sheltered()&&g.player().health==hurt,"Under a roof it does not");
  g.m_player.z=4;check(!g.sheltered(),"Above the roof is not under it");g.m_player.z=0;
  run(g,20);check(g.stormPhase()==0&&g.stormIntensity()==0,"The storm passes and the cycle starts again");
  Game loaded(campaign);g.updateStorm(0);bool decoded=loaded.decodeSave(g.encodeSave());check(decoded&&loaded.state(stateId("storm_clock"))==g.state(stateId("storm_clock")),"The storm clock saves with the game");}
 {Game plain;plain.updateStorm(10);check(plain.stormPhase()==0&&plain.ashFall()==0,"A campaign with no STORM has none");}
 report<<good<<"/"<<checks<<" checks passed\n";return passed;
}
}
