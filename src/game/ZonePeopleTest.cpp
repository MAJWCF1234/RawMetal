// The Ashfall zone's people, played through (--zone-people-test): stances on
// arrival, the elder's work, the mine's truce kept and broken, the scrapyard
// toll paid and jumped, and the storm's roofs.
#include "Game.h"
#include <cmath>
#include <fstream>

namespace retro {
bool Game::testZonePeople(){
 std::ofstream report("zone-people-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string& text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 InputState use{};use.use=true;
 auto run=[](Game&g,float seconds,const InputState&input={}){for(int i=0;i<int(seconds*60);++i)g.update(input,1.f/60);};
 auto press=[&](Game&g,const InputState&key){g.update(key,1.f/60);g.update({},1.f/60);};
 auto choose=[&](Game&g,int reply){run(g,.5f);InputState key{};key.choice=reply;press(g,key);};
 // Who stands where (zone metres, from tools/ashfall/build_zone.py).
 auto actorNear=[](const Game&g,Vec2 local){int best=-1;float nearest=1.5f;
  for(size_t i=0;i<g.m_friendlyActors.size();++i){const auto&npc=g.m_friendlyActors[i];if(npc.level!=g.level())continue;float d=length(npc.pose.position-local);if(d<nearest){nearest=d;best=int(i);}}
  return best;};
 // Face a speaker from a metre and a half and open the conversation.
 auto talkTo=[&](Game&g,int actor){const auto at=g.m_friendlyActors[size_t(actor)].pose.position;g.m_player.pos=at+Vec2{0,-1.5f};g.m_player.z=g.groundHeight(g.m_player.pos,float(World::TerrainMaxZ+1));g.m_player.angle=kPi*.5f;g.equipWeapon(WeaponId::Fists);run(g,.2f);press(g,use);return g.talking();};
 // 1. Arrival at the camp sets every squad's stance.
 {auto g=zoneInspection(15,{13,14},0,0);
  check(g.state(stateId("zone_camp_stance"))==3&&g.state(stateId("zone_cleanup_stance"))==2&&g.state(stateId("zone_bandit_stance"))==2&&g.state(stateId("zone_toll_unpaid"))==1,
        "Arriving at the camp: the sentries allied, the mine and the yard on a truce, the toll unpaid");
  // 2. The elder gives the work.
  int elder=actorNear(g,{19.5f,12.8f});check(elder>=0,"The camp elder stands by the fire");
  if(elder>=0){check(talkTo(g,elder)&&g.dialogueView()->line.rfind("Another one through the wire",0)==0,"E on the elder opens his conversation");
   check(g.dialogueView()->replies.size()==3,"He offers work, the story and leave; the mine and the PDA wait their turn");
   choose(g,1);check(g.dialogueNode()&&g.dialogueNode()->id==stateId("elder_work"),"Asking for work, he tells of the courier");
   choose(g,1);check(!g.talking()&&g.state(stateId("zone_briefed"))==1,"Taking the job sets the briefing");
   run(g,.5f);press(g,use);check(g.talking()&&g.dialogueView()->replies.size()==3&&g.dialogueView()->replies[1]=="Who are the men holed up at the mine?","Briefed, the work is gone and the mine can be asked about");}}
 // 3. The mine: a truce that holds, then one that breaks.
 {auto g=zoneInspection(2,{9.5f,8.5f},90,0);g.setState(stateId("zone_cleanup_stance"),2);
  int cpl=actorNear(g,{9.5f,11.2f});check(cpl>=0,"The corporal holds the mine yard");
  bool alert=false;for(int i=0;i<240;++i){g.update({},1.f/60);for(size_t a=0;a<g.m_friendlyActors.size();++a)alert|=g.m_friendlyActors[a].level==g.level()&&g.m_friendlyActors[a].track().ai.mode==ActorAiMode::Guard&&g.guardState(a)==GuardState::Alert;}
  check(!alert&&g.player().health>99,"On the truce the squad lets the player stand among them (4 s)");
  if(cpl>=0){check(talkTo(g,cpl)&&g.dialogueView()->line.rfind("That's close enough",0)==0,"The corporal talks");
   check(g.dialogueView()->replies.size()==3,"The way out is not offered before the courier's PDA");
   choose(g,2);choose(g,2);check(!g.talking()&&g.state(stateId("zone_cleanup_stance"))==1,"Goading him ends the truce for the whole squad");
   bool fight=false;for(int i=0;i<240&&!fight;++i){g.update({},1.f/60);fight=g.guardState(size_t(cpl))==GuardState::Alert||g.player().health<99;}
   check(fight,"And they come for the player");}}
 {auto g=zoneInspection(2,{9.5f,8.5f},90,0);g.setState(stateId("zone_cleanup_stance"),2);g.setState(stateId("zone_pda"),1);
  int cpl=actorNear(g,{9.5f,11.2f});
  if(cpl>=0&&talkTo(g,cpl)){check(g.dialogueView()->replies.size()==4,"With the PDA the way out is on the table");
   choose(g,3);choose(g,1);check(g.state(stateId("zone_cleanup_stance"))==3&&g.state(stateId("zone_cleanup_escort"))==1,"Offering them a way out makes them allies");}
  else check(false,"The corporal talks with the PDA in hand");}
 // 4. The scrapyard toll: jumped, then paid.
 {auto g=zoneInspection(13,{3,8},0,0);g.setState(stateId("zone_bandit_stance"),2);g.setState(stateId("zone_toll_unpaid"),1);
  g.m_player.pos={14,14};g.m_player.z=g.groundHeight(g.m_player.pos,float(World::TerrainMaxZ+1));run(g,.3f);
  check(g.state(stateId("zone_bandit_stance"))==1,"Walking into the yard unpaid turns the bandits hostile");}
 {auto g=zoneInspection(13,{3,8},0,0);g.setState(stateId("zone_bandit_stance"),2);g.setState(stateId("zone_toll_unpaid"),1);g.setState(stateId("zone_checkpoint_crate"),1);
  int boss=actorNear(g,{6.5f,9.5f});check(boss>=0,"The toll boss stands at the yard gate");
  if(boss>=0&&talkTo(g,boss)){check(g.dialogueView()->replies.size()==4,"With the soldiers' crate the toll can be paid");
   choose(g,3);choose(g,1);check(g.state(stateId("zone_bandit_stance"))==3&&g.state(stateId("zone_toll_paid"))==1&&g.state(stateId("zone_toll_unpaid"))==0,"Paying the toll wins the yard over");
   g.m_player.pos={14,14};g.m_player.z=g.groundHeight(g.m_player.pos,float(World::TerrainMaxZ+1));run(g,.3f);check(g.state(stateId("zone_bandit_stance"))==3,"Paid, the yard is open");}}
 // 5. The storm and its roofs.
 {auto g=zoneInspection(11,{6,20},0,0);
  check(g.stormCycle()&&g.stormCycle()->stormMs>=30000&&g.ashFall()>0,"The zone has an ash storm and light ash between storms");
  int roofs=0;for(int level=0;level<g.chunkCount();++level)roofs+=int(g.chunkWorld(level).shelters().size());
  check(roofs>=12,"Shelters stand across the zone ("+std::to_string(roofs)+")");
  check(g.sheltered(),"Under the fuel station canopy is out of the storm");}
 // 6. A player with nothing but the exe finds the zone under Custom Maps.
 {Game title;title.loadCustomCampaignDirectory(L"no-such-custom-maps-folder");
  check(title.m_customCampaigns.size()==1&&title.m_customCampaigns[0]->name=="ASHFALL EXCLUSION ZONE","Custom Maps lists the zone shipped inside the exe");}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}
