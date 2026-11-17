// Map format 2 (--each-test=map-format; the lab's engine.map-format): named fields, names,
// overrides, prefabs, custom properties and the FORMAT record, from the parser through the
// exporter to the game. docs/Systems/map-format.md is the reference.
#include "Game.h"
#include "../audio/SoundBank.h"
#include "../world/CampaignExport.h"
#include "../world/CustomCampaign.h"
#include "../world/ModelIds.h"
#include <cmath>
#include <fstream>
namespace retro {
// One room that uses every part of format 2 (the lab's map-format tube plays it too).
std::string mapFormatTestText(){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\nCAMPAIGN|Format two|0|future.campaign=yes\n"
  "MAP|0|Format room|0|0|4|12|0|0|4|0.6|industrial_night|0|0|0|0|0|-1|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+(y==0||y==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+"\n";
 t+="LIGHT|0|12|12|3.6|0|0|1.4|18|4000\nLIGHT|0|8|8|3.6|0|0|1.2|10|4000\n"
  // A pair of desks, placed twice: turned a quarter, and with their names prefixed.
  "PREFAB|desk_pair\nFIXTURE|@|24|1|0|0|1.4|0.7|0.75|0|1|name=desk\nFIXTURE|@|24|-1|0|0|1.4|0.7|0.75|0|1|name=desk2|tint=0.5,0.5,1\nPREFAB_END\n"
  "PREFAB_PLACE|0|desk_pair|18|6|0|0|a_\nPREFAB_PLACE|0|desk_pair|18|18|0|1.5707964|b_\n"
  // A red crate beside a plain one, and a box frozen in the air until a script lets it go.
  "CLUTTER|0|7|8|8|0|0|17|0.6|0.6|0.6|0|0|#0|name=red_crate|tint=1,0.25,0.25|mass=3|impact=metal_bang\n"
  "CLUTTER|0|7|9.6|9.4|0|0|17|0.6|0.6|0.6|0|0|#0\n"
  "CLUTTER|0|7|10|8|1.2|0|17|0.5|0.5|0.5|0|0|#0|name=held_box|frozen=1\n"
  // A monitor with properties a script reads and writes, and a key no build knows yet.
  "FIXTURE|0|27|6|16|0|0.42|0.4|0.38|0|1|name=vault|p.code=1234|p.owner=Jenkins|future.key=7\n"
  // A facility guard (ACTOR_FACTION 2, ally) turned hostile by faction=enemy.
  "TIMED_SEQUENCE|0|fmt_clock|fmt_clock_done|0|0|1|1|-1|-0.5|60000|60000|0|0|0|0|1\n"
  "ACTOR|0|fmt_clock|4|1.6|0|0|0|name=turncoat|faction=enemy|display=Turncoat\nACTOR_AI|0|0|3||1.5|2|6\nACTOR_FACTION|0|0|2|\nACTOR_GUARD|0|0|11|0.65|600|4000|\nACTOR_PATROL|0|0|16|8|0|60000\n"
  "ACTOR_KEY|0|0|0|16|8|0|0|0|0\nACTOR_KEY|0|0|60000|16|8|0|0|0|1\n"
  // A sleeping Huntsman on the player's side, with a guard order, a name and 500 health.
  "CREATURE|0|0|4|20|0|1|0|fmt_sleep|||name=rex|faction=ally|ai=guard|display=Rex|hp=500\n"
  // Standing in the corner box: if the vault's code is 1234, open it, free the box, count two.
  "SCRIPT_EVENT|0|fmt_open|2|2|6|6|-10|10||1|0|name=open_box\nSCRIPT_REQUIRE|0|fmt_open|@vault.code|=|1234\n"
  "SCRIPT_ACTION|0|fmt_open|STATE|@vault.opened|1\nSCRIPT_ACTION|0|fmt_open|STATE|@held_box.frozen|0\nSCRIPT_ACTION|0|fmt_open|ADD|fmt_count|2\n";
 return t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
}
bool Game::testMapFormat(){
 std::ofstream report("map-format-test.txt");int failed=0;
 auto check=[&](bool ok,const std::string& text){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 auto refused=[&](const std::string& text,const std::string& expect){try{loadCustomCampaignText(text,"refused.rmc");return std::string("loaded");}catch(const std::exception& e){std::string w=e.what();return w.find(expect)!=std::string::npos?std::string():w;}};
 auto room=[](const std::string& head,const std::string& body){std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\n"+head+"CAMPAIGN|R|0\nMAP|0|R|0|0|4|12|0|0|4|0.6|industrial_night|0|0|0|0|0|-1|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";return t+body+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";};
 try{
  // ---- Version 1 stays version 1 ----
  {auto old=loadCustomCampaignText(room("","FIXTURE|0|24|5|5|0|1.4|0.7|0.75|0|1\n"),"old.rmc");const auto text=exportRmc("Old",{{old->maps[0],-1}}).text;
   check(old->format==1&&old->maps[0]->recordFields.empty()&&text.find("FORMAT|")==std::string::npos,"a file without FORMAT is version 1, and its export writes no FORMAT record");}
  {const auto why=refused(room("","FIXTURE|0|24|5|5|0|1.4|0.7|0.75|0|1|name=desk\n"),"FORMAT|2");check(why.empty(),"a version 1 file with a key=value field is refused, saying it needs FORMAT|2 ("+why+")");}
  {const auto why=refused(room("FORMAT|3\n",""),"formats up to 2");check(why.empty(),"FORMAT|3 is refused as made for a newer build ("+why+")");}
  {const auto why=refused(room("","FORMAT|2\n"),"first record");check(why.empty(),"FORMAT anywhere but first is refused ("+why+")");}
  {const auto why=refused(room("FORMAT|2\n","FIXTURE|0|24|5|5|0|1.4|0.7|0.75|0|1|name=a\nCLUTTER|0|0|6|6|-999|0|name=a\n"),"already taken");check(why.empty(),"two records named a on one map are refused ("+why+")");}
  {const auto why=refused(room("FORMAT|2\n","FIXTURE|0|24|5|5|0|1.4|0.7|0.75|0|1|tint=red\n"),"tint=red");check(why.empty(),"a bad value for a known key names the key ("+why+")");}
  {const auto why=refused(room("FORMAT|2\n","PREFAB|wall\nSTRUCT|@|0|0|2|0.2|0|3|0|0\nPREFAB_END\nPREFAB_PLACE|0|wall|5|5|0|0.5\n"),"quarter turns");check(why.empty(),"a box record placed at a yaw that is not a quarter turn is refused ("+why+")");}
  {const auto why=refused(room("FORMAT|2\n","PREFAB|bad\nLEAD|@|1|1|1|2|2|1|2|0.01\nPREFAB_END\n"),"by number");check(why.empty(),"a prefab cannot hold a record that names others by number ("+why+")");}
  {const auto why=refused(room("FORMAT|2\n","SCRIPT_EVENT|0|e|0|0|24|24|-10|10||1|0\nSCRIPT_REQUIRE|0|e|@ghost.key|=|1\nSCRIPT_ACTION|0|e|STATE|x|1\n"),"no record named ghost");check(why.empty(),"a script naming a record that is not there is refused ("+why+")");}

  // ---- The full room: parse ----
  const auto text=mapFormatTestText();{std::ofstream room("map-format.rmc",std::ios::binary);room<<"META_LEVEL_NAME: Format two\n# Written by --each-test=map-format (MapFormatTest.cpp); lab/fixtures/map-format.rmc is a copy.\n"<<text;}auto campaign=loadCustomCampaignText(text,"format-two.rmc");const auto& m=*campaign->maps[0];
  check(campaign->format==2,"FORMAT|2 is read");
  int names=0;for(const char* n:{"a_desk","a_desk2","b_desk","b_desk2","red_crate","held_box","vault","turncoat","rex","open_box"})names+=m.entity(n)!=nullptr;
  check(names==10,"every name is filed, prefab names with their prefix ("+std::to_string(names)+" of 10)");
  auto fixtureAt=[&](const char* name)->const Fixture*{auto* e=m.entity(name);return e&&e->tag=="FIXTURE"?&m.fixtures[size_t(e->index)]:nullptr;};
  const auto *a=fixtureAt("a_desk"),*b=fixtureAt("b_desk"),*b2=fixtureAt("b_desk2");
  check(a&&b&&b2&&std::fabs(a->position.x-19)<1e-4f&&std::fabs(a->position.y-6)<1e-4f&&std::fabs(b->position.x-18)<1e-4f&&std::fabs(b->position.y-19)<1e-4f&&std::fabs(b2->position.y-17)<1e-4f&&std::fabs(b->yaw-1.5707964f)<1e-4f,
   "PREFAB_PLACE sets the group down at its spot and turns it with its yaw");
  check(b2&&b2->traits&&b2->traits->tinted&&std::fabs(b2->traits->tint.z-1)<1e-6f,"a prefab record keeps its overrides");
  const auto* vault=fixtureAt("vault");const auto* future=vault?findNamedField(m.recordFields.at(recordFieldKey("FIXTURE","",int(m.entity("vault")->index))),"future.key"):nullptr;
  check(future&&future->value=="7","an unknown key is kept with its record");
  check(m.actorTracks.size()==1&&m.actorTracks[0].traits&&m.actorTracks[0].traits->faction==int(AiFaction::Raider)&&m.actorTracks[0].ai.faction==AiFaction::Facility,"the guard's faction=enemy is kept as an override; the authored ACTOR_FACTION stays as written");

  // ---- Export: every named field goes back, and the trip is a fixed point ----
  const auto first=exportRmc(campaign->name,{{campaign->maps[0],-1}},0).text;
  check(first.find("\nFORMAT|2\nCAMPAIGN|Format two|0|future.campaign=yes\n")!=std::string::npos,"the export opens FORMAT|2 and keeps the campaign record's own key");
  check(first.find("|name=vault|p.code=1234|p.owner=Jenkins|future.key=7\n")!=std::string::npos,"the export writes a record's named fields after its own, in order, the unknown one too");
  check(first.find("STATE|@vault.opened|1")!=std::string::npos&&first.find("@vault.code|=|1234")!=std::string::npos,"scripts keep their @entity.key spellings");
  check(first.find("|name=b_desk2|tint=0.5,0.5,1\n")!=std::string::npos,"a placed prefab is written as its records, names prefixed");
  auto again=loadCustomCampaignText(first,"format-two-again.rmc");const auto second=exportRmc(again->name,{{again->maps[0],-1}},0).text;
  check(first==second,"export, parse, export writes the same file");
  const auto differences=compareAuthoredMaps(*campaign->maps[0],*again->maps[0],false);
  check(differences.empty(),"the exported map parses back to the same map"+(differences.empty()?std::string():": "+differences.front()));
  size_t carried=0,expected=0;for(const auto& [key,value]:m.recordFields){if(key.rfind("PREFAB_PLACE",0)==0)continue;++expected;auto it=again->maps[0]->recordFields.find(key);carried+=it!=again->maps[0]->recordFields.end()&&it->second==value;}
  check(carried==expected,"every record's named fields come back under the same key ("+std::to_string(carried)+" of "+std::to_string(expected)+")");

  // ---- The game: overrides take effect ----
  Game g(campaign);
  auto prop=[&](const char* name)->Clutter*{for(auto& c:g.m_clutter)if(c.traits&&c.traits->name==name)return &c;return nullptr;};
  auto* crate=prop("red_crate");
  check(crate&&crate->traits->tinted&&std::fabs(crate->mass()-3)<1e-5f&&bankSound(crate->impactSound())&&std::string(bankSound(crate->impactSound())->name)=="metal_bang",
   "the red crate carries its tint, its 3 kg and its metal_bang impact");
  auto* held=prop("held_box");const float heldZ=held?held->z:0;
  check(held&&held->pinned,"the frozen box is pinned where it was put");
  if(held){g.knockClutter(g.m_level,size_t(held-g.m_clutter.data()),{held->pos.x,held->pos.y,held->z},{0,6,2});for(int i=0;i<60;++i)g.update({},1.f/60);held=prop("held_box");}
  check(held&&std::fabs(held->z-heldZ)<1e-4f&&std::fabs(held->pos.x-10)<1e-4f,"knocked and left a second, the frozen box has not moved (z "+std::to_string(held?held->z:-1)+")");
  const Enemy* rex=nullptr;for(const auto& e:g.m_enemies)if(e.traits&&e.traits->name=="rex")rex=&e;
  check(rex&&rex->faction==AiFaction::Facility&&std::string(rex->name())=="Rex"&&rex->maxHp==500&&rex->ordered&&rex->order==std::uint8_t(SummonOrder::Guard),"the creature is an ally named Rex with 500 health on a guard order");
  const auto& turncoat=g.world().actorTracks().at(0);
  check(turncoat.ai.faction==AiFaction::Raider&&factionsHostile(g.npcFaction(turncoat),AiFaction::Player),"the facility guard turned by faction=enemy is hostile to the player");
  check(g.entityPropertyInt(0,"vault","code")==1234&&g.entityPropertyInt(0,"vault","opened")==0,"the vault's code reads 1234 and its opened property 0 before the script");
  g.m_player.pos={4,4};for(int i=0;i<30;++i)g.update({},1.f/60);
  check(g.entityPropertyInt(0,"vault","opened")==1&&g.state(stateId("fmt_count"))==2,"standing in the box, the script reads @vault.code and sets @vault.opened (count "+std::to_string(g.state(stateId("fmt_count")))+")");
  for(int i=0;i<90;++i)g.update({},1.f/60);held=prop("held_box");
  check(held&&!held->pinned&&held->z<heldZ-.5f,"the script's @held_box.frozen 0 lets the box fall (z "+std::to_string(held?held->z:-1)+")");

  // ---- A save keeps the scripts' values; the load takes the overrides from the map ----
  const auto saved=g.encodeSave();Game loaded(campaign);
  check(loaded.decodeSave(saved),"the game saves and loads");
  check(loaded.entityPropertyInt(0,"vault","opened")==1&&loaded.entityPropertyInt(0,"vault","code")==1234,"after the load the script's opened=1 stands and the authored code is not given again");
  const Enemy* rex2=nullptr;for(const auto& e:loaded.m_enemies)if(e.traits&&e.traits->name=="rex")rex2=&e;
  bool tinted=false;for(const auto& c:loaded.m_clutter)tinted|=c.traits&&c.traits->name=="red_crate"&&c.traits->tinted;
  check(rex2&&rex2->faction==AiFaction::Facility&&tinted,"after the load the creature is still an ally and the crate still red");
 }catch(const std::exception& e){check(false,std::string("threw: ")+e.what());}
 report<<(failed?"FAILED ":"PASSED ")<<failed<<" failures\n";
 return failed==0;
}
}
