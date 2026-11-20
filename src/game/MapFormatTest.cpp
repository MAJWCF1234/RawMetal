// Map format 2 (--each-test=map-format; the lab's engine.map-format): named fields, names,
// overrides, prefabs, custom properties and the FORMAT record, from the parser through the
// exporter to the game. docs/Systems/map-format.md is the reference.
#include "Game.h"
#include "../audio/SoundBank.h"
#include "../world/CampaignExport.h"
#include "../world/CampaignFieldRanges.h"
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
 t+="LIGHT|0|12|12|3.6|0|0|1.4|18|4000\nLIGHT|0|8|8|3.6|0|0|1.2|10|4000\nLIGHT|0|8.5|10|3.4|0|0|2.6|12|4500\nLIGHT|0|6|6|3.4|0|0|1.8|10|4500\n"
  // A pair of desks, placed twice: turned a quarter, and with their names prefixed.
  "PREFAB|desk_pair\nFIXTURE|@|24|1|0|0|1.4|0.7|0.75|0|1|name=desk\nFIXTURE|@|24|-1|0|0|1.4|0.7|0.75|0|1|name=desk2|tint=0.5,0.5,1\nPREFAB_END\n"
  "PREFAB_PLACE|0|desk_pair|18|6|0|0|a_\nPREFAB_PLACE|0|desk_pair|18|18|0|1.5707964|b_\n"
  // A red crate beside a plain one, and a box frozen in the air until a script lets it go.
  "CLUTTER|0|7|8|8|0|0|17|0.6|0.6|0.6|0|0|#0|name=red_crate|tint=1,0.25,0.25|mass=3|impact=metal_bang\n"
  "CLUTTER|0|7|9.6|9.4|0|0|17|0.6|0.6|0.6|0|0|#0\n"
  "CLUTTER|0|7|10|8|1.2|0|17|0.5|0.5|0.5|0|0|#0|name=held_box|frozen=1\n"
  // A record without a name may carry properties too: they are kept under its id key, #prop:3.
  "CLUTTER|0|0|14|14|-999|0|p.weight=7\n"
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
  // ---- The Campaign Builder's limits (CampaignFieldRanges.h): one table, the loader's own bounds ----
  {
   // "": the loader took it; otherwise its refusal.
   // A record on the room's map 0 (a MAP record is the room's own: its sample stands in for it).
   auto load=[&](const std::string& body){try{
     if(body.rfind("MAP|",0)==0){std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\nCAMPAIGN|R|0\n"+body+"LAYER|0|0|Floor|0|0\n";for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";loadCustomCampaignText(t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","limits.rmc");}
     else loadCustomCampaignText(room("FORMAT|2\n",body),"limits.rmc");
     return std::string();}catch(const std::exception& e){return std::string(e.what());}};
   // An event needs an action to be loaded.
   auto eventTail=[](const std::string& line){return line.rfind("SCRIPT_EVENT|0|",0)==0?"\nSCRIPT_ACTION|0|"+line.substr(15,line.find('|',15)-15)+"|MESSAGE|hi|2":std::string();};
   auto join=[](const std::vector<std::string>& v){std::string s;for(size_t i=0;i<v.size();++i)s+=(i?"|":"")+v[i];return s;};
   auto split=[](const std::string& s){std::vector<std::string> v;size_t a=0;for(size_t i=0;i<=s.size();++i)if(i==s.size()||s[i]=='|'){v.push_back(s.substr(a,i-a));a=i+1;}return v;};
   auto text=[](double v){char b[40];std::snprintf(b,sizeof b,"%.6g",v);return std::string(b);};
   int rows=0;std::string wrong;
   for(const auto& r:fieldRanges){
    ++rows;
    auto at=[&](double v){auto f=split(r.base);const auto t=text(v);
     if(r.key)f.push_back(std::string(r.key)+"="+t);else{if(f.size()<=size_t(r.index))f.resize(size_t(r.index)+1);f[size_t(r.index)]=t;}
     const auto line=join(f);return load(line+eventTail(line)+"\n");};
    const double step=r.whole?1:(r.hi-r.lo)*.02;
    auto note=[&](const char* what,double v,const std::string& why){wrong+=std::string(r.kind)+(r.key?std::string(".")+r.key:"["+std::to_string(r.index)+"]")+" "+what+" "+text(v)+(why.empty()?std::string(" was refused? no: it loaded"):": "+why)+"; ";};
    for(double v:{r.lo,r.hi})if(const auto why=at(v);!why.empty())note("takes",v,why);
    if(r.exact)for(double v:{r.lo-step,r.hi+step})if(at(v).empty())note("should refuse",v,"");}
   check(wrong.empty(),"the Builder's field limits (CampaignFieldRanges.h, "+std::to_string(rows)+" rows) are the loader's: the ends load, a step beyond is refused"+(wrong.empty()?std::string():": "+wrong));
   std::string words;
   for(const auto& t:textRules){
    auto with=[&](const std::string& v){auto f=split(t.base);if(f.size()<=size_t(t.index))f.resize(size_t(t.index)+1);f[size_t(t.index)]=v;const auto line=join(f);return load(line+eventTail(line)+"\n");};
    const std::string where=std::string(t.kind)+"["+std::to_string(t.index)+"]";
    if(const auto why=with(std::string(size_t(t.maxLen),'a'));!why.empty())words+=where+" "+std::to_string(t.maxLen)+" letters should load: "+why+"; ";
    if(with(std::string(size_t(t.maxLen+1),'a')).empty())words+=where+" "+std::to_string(t.maxLen+1)+" letters should be refused; ";
    if(t.mode==1&&with("bad name").empty())words+=where+" a name with a space should be refused; ";
    if(t.mode==2&&with("caf\xC3\xA9").empty())words+=where+" non-ASCII text should be refused; ";
    const auto fitted=fitText(t,t.mode==1?"two words":t.mode==2?"caf\xC3\xA9 au lait":(t.mode==3?"a|b%c ":"a b c ")+std::string(size_t(t.maxLen),'z'));
    if(const auto why=with(fitted);!why.empty())words+=where+" the text the Builder writes ("+fitted+") should load: "+why+"; ";}
   check(words.empty(),"the Builder's name rules (script names, printable text, lengths) are the loader's"+(words.empty()?std::string():": "+words));
   std::string over;
   for(const auto& c:recordCaps){
    auto many=[&](int n){std::string body;for(int i=1;i<=n;++i){std::string line=c.sample;for(auto q=line.find("{n}");q!=std::string::npos;q=line.find("{n}"))line.replace(q,3,std::to_string(i));body+=line+eventTail(line)+"\n";}return load(body);};
    if(const auto why=many(c.limit);!why.empty())over+=std::string(c.kind)+" "+std::to_string(c.limit)+" should load: "+why+"; ";
    if(many(c.limit+1).empty())over+=std::string(c.kind)+" "+std::to_string(c.limit+1)+" should be refused; ";}
   check(over.empty(),"the Builder's record caps (SPAWN 64, EMITTER 64, DARK 64, SCRIPT_EVENT 128, TERRAIN_FLAT 512, GROUND 1024) are the loader's"+(over.empty()?std::string():": "+over));
  }
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

  // ---- The values scripts change live in the save's entity store (save format 28) ----
  {const auto* opened=g.entityProperty(0,"vault","p.opened");const auto* frozen=g.entityProperty(0,"held_box","frozen");
   check(opened&&std::holds_alternative<std::int64_t>(*opened)&&std::get<std::int64_t>(*opened)==1&&frozen&&std::holds_alternative<std::int64_t>(*frozen)&&std::get<std::int64_t>(*frozen)==0,
    "the script's writes are in the entity store: vault p.opened 1, held_box frozen 0");
   check(g.entityProperty(0,"vault","p.code")==nullptr&&g.state(entityPropertyState(0,"Format room","vault","opened"))==0,"the store holds only what scripts changed (not the authored code), and no script state is written");
   check(g.entityPropertyInt(0,"#prop:3","weight")==7,"a record without a name keeps its property under its id key (#prop:3)");}

  // ---- A save keeps the scripts' values; the load takes the overrides from the map ----
  const auto saved=g.encodeSave();Game loaded(campaign);
  check(loaded.decodeSave(saved),"the game saves and loads");
  check(!loaded.saveStore().entities.empty()&&loaded.saveStore().entities==g.saveStore().entities,"the entity store comes back whole through the save's entities section");
  check(loaded.entityPropertyInt(0,"vault","opened")==1&&loaded.entityPropertyInt(0,"vault","code")==1234,"after the load the script's opened=1 stands and the authored code is not given again");
  const Enemy* rex2=nullptr;for(const auto& e:loaded.m_enemies)if(e.traits&&e.traits->name=="rex")rex2=&e;
  bool tinted=false;for(const auto& c:loaded.m_clutter)tinted|=c.traits&&c.traits->name=="red_crate"&&c.traits->tinted;
  check(rex2&&rex2->faction==AiFaction::Facility&&tinted,"after the load the creature is still an ally and the crate still red");
  {Game fresh(campaign);check(fresh.saveStore().entities.empty()&&fresh.entityPropertyInt(0,"vault","opened")==0&&fresh.entityPropertyInt(0,"vault","code")==1234,"a new game starts from the map's values, with an empty store");}

  // ---- A format 27 save (the last before the section container) still loads ----
  {const auto old=g.legacySaveText(Game::LegacySaveVersion);Game older(campaign);const bool ok=older.decodeSave(old);
   const Enemy* rex3=nullptr;for(const auto& e:older.m_enemies)if(e.traits&&e.traits->name=="rex")rex3=&e;
   bool red=false,held=false;for(const auto& c:older.m_clutter){red|=c.traits&&c.traits->name=="red_crate"&&c.traits->tinted;held|=c.traits&&c.traits->name=="held_box";}
   check(ok&&old.rfind("RAWMETAL_SAVE 27 ",0)==0&&rex3&&rex3->faction==AiFaction::Facility&&red&&held&&older.entityPropertyInt(0,"vault","code")==1234,
    "a format 27 save of the room loads: the overrides come from the map again ("+(ok?std::string("loaded"):Game::lastLoadFailure())+")");}

  // ---- Stair and balcony finishes: STAIR tread= riser= stringer=, LAYER deck= edge= railing= ----
  {auto finished=[&](bool named){
    std::string body="LAYER|0|1|Gallery|3|0.3"+std::string(named?"|deck=24|edge=12|railing=19":"")+"\n";
    for(int y=0;y<24;++y)body+="ROW|0|1|"+std::to_string(y)+"|"+(y>=10&&y<=12?std::string(14,'.')+std::string(6,'=')+std::string(4,'.'):std::string(24,'.'))+"\n";
    body+="STAIR|0|6|10|12|12|0|3|12|0|1|0|0.12|1"+std::string(named?"|tread=46|riser=15|stringer=18":"")+"\n";
    return loadCustomCampaignText(room("FORMAT|2\n",body),named?"finished.rmc":"plain.rmc");};
   auto plain=finished(false),dressed=finished(true);
   Game a(plain),b(dressed);const auto& p=a.world().structures();const auto& q=b.world().structures();
   auto count=[](const std::vector<Structure>& list,auto&& keep){size_t n=0;for(const auto& s:list)n+=keep(s);return n;};
   const size_t plainSteps=count(p,[](const Structure& s){return !s.rail&&s.x1>=6&&s.x2<=12.001f&&s.y1>=10&&s.y2<=12&&s.material==0;});
   check(plainSteps==12&&count(p,[](const Structure& s){return s.finishedRail;})==0,"a stair and a balcony without finishes build as before (12 plain steps, no finished rails; "+std::to_string(plainSteps)+")");
   const size_t treads=count(q,[](const Structure& s){return !s.rail&&s.material==46&&s.top-s.bottom<.05f&&s.x1>=6&&s.x2<=12.001f;});
   const size_t risers=count(q,[](const Structure& s){return !s.rail&&s.material==15&&s.x1>=6&&s.x2<=12.001f;});
   const size_t stringers=count(q,[](const Structure& s){return s.rail&&s.material==18&&s.finishedRail;});
   check(treads==12&&risers==12&&stringers==24,"the stair has 12 treads of 46 over 12 risers of 15 and 24 stringer pieces of 18 ("+std::to_string(treads)+", "+std::to_string(risers)+", "+std::to_string(stringers)+")");
   const size_t decks=count(q,[](const Structure& s){return !s.rail&&s.material==24&&std::fabs(s.top-3)<.001f&&std::fabs(s.bottom-2.96f)<.001f;});
   const size_t edges=count(q,[](const Structure& s){return !s.rail&&s.material==12&&std::fabs(s.top-2.96f)<.001f&&std::fabs(s.bottom-2.7f)<.001f;});
   const size_t rails=count(q,[](const Structure& s){return s.rail&&s.material==19&&s.finishedRail;});
   check(decks>0&&decks==edges&&rails>0,"the balcony deck is 24 over a 12 edge with railing 19 ("+std::to_string(decks)+", "+std::to_string(edges)+", "+std::to_string(rails)+")");
   // Walking support is the same: the tread split leaves the step tops where they were.
   check(std::fabs(a.world().floorHeight(9.5f,11)-b.world().floorHeight(9.5f,11))<1e-4f&&std::fabs(a.world().floorHeight(17,11)-b.world().floorHeight(17,11))<1e-4f,"stairs and deck stand at the same heights with finishes");
   const auto exported=exportRmc("F",{{dressed->maps[0],-1}}).text;
   check(exported.find("|tread=46|riser=15|stringer=18")!=std::string::npos&&exported.find("|deck=24|edge=12|railing=19")!=std::string::npos,"the exporter writes the finishes back");
   {const auto why=refused(room("FORMAT|2\n","STAIR|0|6|10|12|12|0|3|12|0|1|tread=6\n"),"tread=6");check(why.empty(),"a finish that cannot be drawn as a box is refused, naming the key ("+why+")");}}

  // ---- Flooring: STRUCT trim= trimwidth= (an edge band split off its sides) ----
  {auto c=loadCustomCampaignText(room("FORMAT|2\n","STRUCT|0|4|4|8|7|0|0.03|0|51|trim=32|trimwidth=0.15\n"),"trim.rmc");Game g(c);const auto& s=g.world().structures();
   size_t bands=0,inner=0;for(const auto& st:s){bands+=st.material==32&&std::fabs(st.top-.03f)<1e-4f;inner+=st.material==51&&std::fabs(st.x1-4.15f)<1e-4f&&std::fabs(st.y2-6.85f)<1e-4f;}
   check(bands==4&&inner==1&&std::fabs(g.world().supportBelow(6,5.5f,2)-.03f)<1e-3f&&std::fabs(g.world().supportBelow(4.05f,5.5f,2)-.03f)<1e-3f,
    "a trimmed flooring is a 51 field inside four 32 bands, walked at 0.03 everywhere ("+std::to_string(bands)+" bands)");
   check(exportRmc("T",{{c->maps[0],-1}}).text.find("|trim=32|trimwidth=0.15")!=std::string::npos,"the exporter writes the trim back");
   check(refused(room("FORMAT|2\n","STRUCT|0|4|4|8|7|0|0.03|0|51|trimwidth=0.1\n"),"trimwidth").empty(),"trimwidth= without trim= is refused");}

  // ---- Material turns: STRUCT/PRIM turn=, STAIR treadturn=, LAYER deckturn=, SURFACE wallturn= floorturn= ceilingturn= ----
  {const std::string body="STRUCT|0|4|4|8|7|0|0.03|0|51|turn=90\nSTRUCT|0|9|4|12|7|0|0.03|0|51\nSTRUCT|0|13|4|16|7|0|0.03|0|51|turn=-90|trim=32\n"
    "PRIM|0|box|18|6|0|2|2|0.5|0|46|turn=180\nSTAIR|0|6|10|12|12|0|3|12|0|1|tread=51|riser=15|treadturn=90\nSURFACE|0|0|8|-1|floorturn=90|wallturn=270\n";
   auto c=loadCustomCampaignText(room("FORMAT|2\n",body),"turns.rmc");Game g(c);const auto& s=g.world().structures();
   int turned=0,plain=0,back=0,trimTurned=0,prim=0,treads=0,risers=0;
   for(const auto& st:s){
    if(st.material==51&&st.x1==4)turned+=st.turn==90;
    if(st.material==51&&st.x1==9)plain+=st.turn==0;
    if(st.material==51&&st.x1>13&&st.x2<16)back+=st.turn==270;
    if(st.material==32)trimTurned+=st.turn!=0;
    if(st.material==46)prim+=st.turn==180;
    if(st.material==51&&st.x1>=6&&st.y1>=10&&st.top-st.bottom<.05f)treads+=st.turn==90;
    if(st.material==15&&st.y1>=10)risers+=st.turn==0;}
   check(c&&turned==1&&plain==1&&back==1&&trimTurned==0,"turn= turns a block's material (absent, 0; -90 is 270); its edge trim keeps the band's own way");
   check(prim>0&&treads==12&&risers==12,"a shape's boxes take its turn; treadturn= turns the step tops, not the fronts ("+std::to_string(prim)+" shape pieces, "+std::to_string(treads)+" tops)");
   check(std::fabs(g.world().floorTurn()-90)<1e-4f&&std::fabs(g.world().wallTurn()-270)<1e-4f&&g.world().ceilingTurn()==0,"SURFACE floorturn= and wallturn= reach the map");
   const auto exported=exportRmc("Turns",{{c->maps[0],-1}}).text;
   check(exported.find("|51|turn=90")!=std::string::npos&&exported.find("|turn=180")!=std::string::npos&&exported.find("treadturn=90")!=std::string::npos&&exported.find("floorturn=90|wallturn=270")!=std::string::npos,
    "the exporter writes every turn back");
   {auto again=loadCustomCampaignText(exported,"turns-again.rmc");const auto differences=again?compareAuthoredMaps(*c->maps[0],*again->maps[0],false):std::vector<std::string>{"did not load"};
    check(differences.empty(),"a turned map exports and loads back the same"+(differences.empty()?std::string():": "+differences.front()));}
   {const auto why=refused(room("FORMAT|2\n","STRUCT|0|4|4|8|7|0|0.03|0|51|turn=sideways\n"),"turn=sideways");check(why.empty(),"a turn that is not degrees is refused, naming it ("+why+")");}
   {const auto why=refused(room("FORMAT|2\n","STRUCT|0|4|4|8|7|0|0.03|0|51|turn=450\n"),"turn=450");check(why.empty(),"a turn past 360 is refused ("+why+")");}
   {auto none=loadCustomCampaignText(room("FORMAT|2\n","STRUCT|0|4|4|8|7|0|0.03|0|51\n"),"plain.rmc"),zero=loadCustomCampaignText(room("FORMAT|2\n","STRUCT|0|4|4|8|7|0|0.03|0|51|turn=360\n"),"zero.rmc");
    check(none&&zero&&compareAuthoredMaps(*none->maps[0],*zero->maps[0],false).empty(),"turn=360 is no turn: the same map as one without it");}}

  // ---- Footings indoors: a room raised 0.6 m on a brick base with a stone edge; its door gets steps ----
  {std::string body="STRUCT|0|8|8|14|13|0|0.1|0|50|footing=foundation|raise=0.6|base=31|cap=33|capwidth=0.12\n"
    "DOOR|0|10|12|8.5|0|0|0|1|0|-1||||1|||||||0\n";
   auto c=loadCustomCampaignText(room("FORMAT|2\n",body),"raised.rmc");Game g(c);const World& w=g.world();
   size_t plinth=0,cap=0,steps=0;for(const auto& st:w.structures()){
    plinth+=st.material==31&&st.x1<=8.001f&&st.x2>=13.999f&&std::fabs(st.bottom)<1e-3f&&std::fabs(st.top-.6f)<1e-3f;
    cap+=st.material==33&&std::fabs(st.top-.7f)<1e-3f;
    steps+=st.material==31&&st.y2<=8.001f&&st.top<.7f&&st.top>0;}
   check(std::fabs(w.floorHeight(11,10.5f)-.7f)<1e-4f&&std::fabs(w.supportBelow(11,10.5f,3)-.7f)<1e-3f,"the raised room is walked at 0.7 (floorHeight "+std::to_string(w.floorHeight(11,10.5f))+")");
   check(plinth==1&&cap==4,"a brick plinth fills under it and a stone edge rings its top ("+std::to_string(plinth)+", "+std::to_string(cap)+")");
   check(steps>=3&&std::fabs(w.supportBelow(11,7.85f,3)-.7f+.7f/4)<.2f,"its door on the north edge gets steps down to the hall floor ("+std::to_string(steps)+")");
   check(exportRmc("R",{{c->maps[0],-1}}).text.find("|footing=foundation|raise=0.6|base=31|cap=33|capwidth=0.12")!=std::string::npos,"the exporter writes the footing back");
   check(refused(room("FORMAT|2\n","STRUCT|0|8|8|14|13|0|0.1|0|50|footing=tower\n"),"footing=tower").empty()&&refused(room("FORMAT|2\n","STRUCT|0|8|8|14|13|0|0.1|0|50|footing=pad|raise=9\n"),"raise=9").empty(),
    "a footing that is not one, or raised past 6 m, is refused");}

  // ---- Footings on the land: pad, foundation, stilts, steps; a draped path; finishes on a hill ----
  {auto land=[](const std::string& body){std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\nCAMPAIGN|L|0\nTERRAIN|7|1|5|16|0.45\n"
     "MAP|0|Hill|0|0|2|2|0|1|2.8|0.2|brutal_wasteland|0|0|0|0|0|-1|0|0|0|0|0\nLAYER|0|0|Ground|0|0\n";
    for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";return t+body+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";};
   // The steepest 8 x 6 spot of that land, measured as the parser will (no footings yet).
   auto bare=loadCustomCampaignText(land(""),"bare.rmc");const auto& shape=*bare->maps[0]->terrain;
   int fx=6,fy=6;float worst=-1;for(int y=2;y<=15;++y)for(int x=2;x<=13;++x){float lo=1e9f,hi=-1e9f;for(float yy=y;yy<=y+6;yy+=.5f)for(float xx=x;xx<=x+8;xx+=.5f){float z=authoredTerrainSurface(shape,0,0,xx,yy);lo=std::min(lo,z);hi=std::max(hi,z);}
    if(hi-lo>worst){worst=hi-lo;fx=x;fy=y;}}
   auto box=[&](const std::string& tail){return "STRUCT|0|"+std::to_string(fx)+"|"+std::to_string(fy)+"|"+std::to_string(fx+8)+"|"+std::to_string(fy+6)+"|0|0.1|0|50|"+tail+"\n";};
   check(worst>1.5f,"the test land has a slope to build on ("+std::to_string(worst)+" m)");
   for(const char* mode:{"pad","foundation","stilts","stepped"}){
    auto c=loadCustomCampaignText(land(box(std::string("footing=")+mode)),std::string(mode)+".rmc");Game g(c);const World& w=g.world();const auto& sh=*c->maps[0]->terrain;
    const auto& parts=c->maps[0]->footings.front().parts;float rise=0,spread=0;int through=0;
    const bool stepped=std::string(mode)=="stepped",terracesAlongX=parts.size()>1&&parts[1].x1>parts[0].x1+.01f;
    for(const auto& p:parts){float lo=1e9f,hi=-1e9f;
     for(float y=p.y1+.1f;y<p.y2;y+=.25f)for(float x=p.x1+.1f;x<p.x2;x+=.25f){through+=authoredTerrainSurface(sh,0,0,x,y)>p.slab+.04f;
      // A terrace's steps up to the next one stand on its far end: measure its floor short of them.
      if(stepped&&(terracesAlongX?x>p.x2-.9f||x<p.x1+.9f:y>p.y2-.9f||y<p.y1+.9f))continue;
      const float s=w.supportBelow(x,y,40);lo=std::min(lo,s);hi=std::max(hi,s);}
     spread=std::max(spread,hi-lo);rise=std::max(rise,std::fabs(hi-p.surface));}
    if(stepped){
     // Walked end to end along its middle, no rise is taller than a step.
     const auto& a=parts.front();const auto& b=parts.back();const bool alongX=b.x2-a.x1>b.y2-a.y1;float worstStep=0,last=-1000;
     for(float u=.05f;u<(alongX?b.x2-a.x1:b.y2-a.y1)-.05f;u+=.05f){const float x=alongX?a.x1+u:(a.x1+a.x2)*.5f,y=alongX?(a.y1+a.y2)*.5f:a.y1+u;
      const float s=w.supportBelow(x,y,40);if(last>-999)worstStep=std::max(worstStep,std::fabs(s-last));last=s;}
     check(worstStep<.21f,"stepped: walked end to end, no rise is taller than a step ("+std::to_string(worstStep)+" m)");}
    check(spread<.002f&&rise<.002f&&through==0,std::string(mode)+": every piece is one level floor where it was planned, no land through it (spread "+std::to_string(spread)+", "+std::to_string(through)+" through)");
    if(std::string(mode)=="pad"){const auto& p=parts.front();float off=0;for(float y=p.y1+.1f;y<p.y2;y+=.5f)for(float x=p.x1+.1f;x<p.x2;x+=.5f)off=std::max(off,std::fabs(authoredTerrainSurface(sh,0,0,x,y)-p.level));
     check(off<.04f,"pad: the land under it is cut and filled to its level ("+std::to_string(off)+" m off)");}
    if(std::string(mode)=="stepped")check(parts.size()>=2,"stepped: an 8 m footing on the slope is terraces ("+std::to_string(parts.size())+")");
    if(std::string(mode)=="stilts"){size_t posts=0;for(const auto& st:w.structures())posts+=st.x2-st.x1<.21f&&st.y2-st.y1<.21f&&st.top>st.bottom+.1f;
     // Under the deck at its lowest ground there is room to stand.
     const auto& p=parts.front();float lowest=1e9f,lx=0,ly=0;for(float y=p.y1+.5f;y<p.y2-.4f;y+=.5f)for(float x=p.x1+.5f;x<p.x2-.4f;x+=.5f){float z=authoredTerrainSurface(sh,0,0,x,y);if(z<lowest){lowest=z;lx=x;ly=y;}}
     check(posts>=6&&std::fabs(w.supportBelow(lx,ly,lowest+.5f)-lowest)<.05f,"stilts: "+std::to_string(posts)+" posts hold the deck and the ground under it is walked ("+std::to_string(w.supportBelow(lx,ly,lowest+.5f))+" vs "+std::to_string(lowest)+")");}
   }
   // A path that follows the slope: drawn over the ground, walked as the ground.
   {auto c=loadCustomCampaignText(land(box("drape=1|trim=33|trimwidth=0.2")),"path.rmc");Game g(c);const World& w=g.world();const auto& sh=*c->maps[0]->terrain;
    bool flat=true;for(const auto& st:w.structures())if(st.drape)flat&=!st.solid;
    const float x=fx+4.f,y=fy+3.f;check(flat&&std::fabs(w.supportBelow(x,y,40)-authoredTerrainSurface(sh,0,0,x,y))<.02f,"a draped path is not solid and is walked on the ground itself");}
   // Finishes on a hill: a stair's step tops keep its own height (split after grounding).
   {auto c=loadCustomCampaignText(land("STAIR|0|"+std::to_string(fx)+"|"+std::to_string(fy)+"|"+std::to_string(fx+6)+"|"+std::to_string(fy+2)+"|0|2|8|0|1|0|0.12|0|tread=51|riser=31\n"),"hillstair.rmc");Game g(c);
    size_t faces=0,met=0;const auto& s=g.world().structures();for(const auto& face:s)if(face.material==51){++faces;for(const auto& body:s)met+=body.material==31&&std::fabs(body.x1-face.x1)<1e-4f&&std::fabs(body.y1-face.y1)<1e-4f&&std::fabs(body.top-face.bottom)<1e-4f;}
    check(faces==8&&met==8,"on a hill every step top sits exactly on its step ("+std::to_string(met)+" of "+std::to_string(faces)+")");}
  }

  // ---- Joint hinge/drawer options: friction, detent, catch, latch round-trip through export ----
  {const std::string body="CLUTTER|0|7|8|8|0|0|17|0.6|0.6|0.6|0|0|#0\nCLUTTER|0|7|9.2|8|0|0|17|0.6|0.6|0.6|0|0|#0\n"
    "JOINT|0|hinge|0|1|8.6|8|0.3|axis=z|min=-10|max=95|friction=12|detent=0,90|catch=6|latch=40\n"
    "SCRIPT_EVENT|0|gather|2|2|6|6|-10|10||1|0\nSCRIPT_TRIGGER|0|gather|box|all|0|players=all\nSCRIPT_ACTION|0|gather|STATE|gathered|1\n";
   auto c=loadCustomCampaignText(room("FORMAT|2\n",body),"joints.rmc");const auto& j=c->maps[0]->joints.at(0);const auto& ev=c->maps[0]->scriptEvents.at(0);
   check(j.kind==PropJointKind::Hinge&&j.friction==12&&j.detentCount==2&&j.latch==40&&ev.allPlayers,"JOINT friction/detent/latch and SCRIPT_TRIGGER players=all parse");
   const auto text=exportRmc("Joints",{{c->maps[0],-1}}).text;
   check(text.find("friction=12")!=std::string::npos&&text.find("detent=")!=std::string::npos&&text.find("catch=")!=std::string::npos&&text.find("latch=40")!=std::string::npos&&text.find("players=all")!=std::string::npos,
    "the exporter writes JOINT friction/detent/catch/latch and SCRIPT_TRIGGER players=all back");
   auto again=loadCustomCampaignText(text,"joints-again.rmc");const auto diffs=compareAuthoredMaps(*c->maps[0],*again->maps[0],false);
   check(diffs.empty()&&again->maps[0]->joints.at(0).friction==12&&again->maps[0]->joints.at(0).detentCount==2&&again->maps[0]->scriptEvents.at(0).allPlayers,
    "a jointed map exports and loads back the same"+(diffs.empty()?std::string():": "+diffs.front()));}

  // ---- Weapon drops: the handgun and the shotgun themselves, and a pickup's height on an upper floor ----
  {const std::string body="PICKUP|0|5|8|8\nPICKUP|0|6|9|8|3.5\nPICKUP|0|0|10|8|3.5\n";
   auto c=loadCustomCampaignText(room("FORMAT|2\n",body),"drops.rmc");const auto& p=c->maps[0]->pickupSpawns;
   check(p.size()==3&&p[0].kind==PickupKind::Handgun&&p[1].kind==PickupKind::Shotgun&&p[1].z==3.5f&&p[0].z<-900&&p[2].z==3.5f,"PICKUP kinds 5 (the handgun) and 6 (the shotgun) load, and an optional z puts one on an upper floor");
   bool refused=false;try{loadCustomCampaignText(room("FORMAT|2\n","PICKUP|0|7|8|8\n"),"drops-bad.rmc");}catch(const std::exception&){refused=true;}
   check(refused,"a pickup kind past the shotgun is refused");
   const auto text=exportRmc("Drops",{{c->maps[0],-1}}).text;auto again=loadCustomCampaignText(text,"drops-again.rmc");const auto& q=again->maps[0]->pickupSpawns;
   check(q.size()==3&&q[0].kind==PickupKind::Handgun&&q[1].kind==PickupKind::Shotgun&&q[1].z==3.5f,"weapon drops and their heights export and load back the same");}

  // ---- Multiplayer: MULTIPLAYER and SPAWN (docs/Systems/multiplayer.md), named fields only ----
  {for(const char* head:{"","FORMAT|2\n"}){auto plain=loadCustomCampaignText(room(head,""),"plain.rmc");const auto& mp=plain->multiplayer;const auto text=exportRmc(plain->name,{{plain->maps[0],-1}},0).text;
    check(mp.deathmatch&&mp.coop&&mp.deathmatchMap==-1&&mp.maxPlayers==8&&mp.fragLimit==20&&mp.timeLimitMinutes==10&&!mp.friendlyFire&&mp.spawns.empty()&&
     text.find("MULTIPLAYER")==std::string::npos&&text.find("SPAWN|")==std::string::npos,
     std::string("without the records (")+(*head?"format 2":"format 1")+") every mode is on at the defaults, no spawns, and the export writes neither");}
   const std::string records="MULTIPLAYER|deathmatch=1|coop=0|dmmap=0|maxplayers=12|fraglimit=30|timelimit=7.5|friendlyfire=1\n"
    "SPAWN|map=0|x=12.5|y=4|z=0|yaw=90|team=0\nSPAWN|map=0|x=3|y=20.25|z=1.5|yaw=-45|team=2\nSPAWN|map=0|x=6|y=6\n";
   auto c=loadCustomCampaignText(room("FORMAT|2\n",records),"multiplayer.rmc");const auto& mp=c->multiplayer;
   check(mp.deathmatch&&!mp.coop&&mp.deathmatchMap==0&&mp.maxPlayers==12&&mp.fragLimit==30&&std::fabs(mp.timeLimitMinutes-7.5f)<1e-6f&&mp.friendlyFire,
    "MULTIPLAYER is read: deathmatch on, co-op off, deathmatch on map 0, 12 players, 30 frags, 7.5 minutes, friendly fire");
   const bool spawnsRead=mp.spawns.size()==3&&mp.spawns[0].map==0&&mp.spawns[0].x==12.5f&&mp.spawns[0].y==4&&mp.spawns[0].yaw==90&&mp.spawns[0].team==0&&
    mp.spawns[1].x==3&&mp.spawns[1].y==20.25f&&mp.spawns[1].z==1.5f&&mp.spawns[1].yaw==-45&&mp.spawns[1].team==2&&
    mp.spawns[2].x==6&&mp.spawns[2].y==6&&mp.spawns[2].z==0&&mp.spawns[2].yaw==0&&mp.spawns[2].team==0;
   check(spawnsRead,"SPAWN records are read in order with their map, spot, height, turn and team (z, yaw and team default to 0; "+std::to_string(mp.spawns.size())+" spawns)");
   {auto partial=loadCustomCampaignText(room("FORMAT|2\n","MULTIPLAYER|coop=0\n"),"partial.rmc");const auto& p=partial->multiplayer;
    check(p.deathmatch&&!p.coop&&p.deathmatchMap==-1&&p.maxPlayers==8&&p.fragLimit==20&&p.timeLimitMinutes==10&&!p.friendlyFire,"a MULTIPLAYER field left out keeps its default");}
   const auto first=exportRmc(c->name,{{c->maps[0],-1}},0).text;
   check(first.find("\nMULTIPLAYER|deathmatch=1|coop=0|dmmap=0|maxplayers=12|fraglimit=30|timelimit=7.5|friendlyfire=1\nSPAWN|map=0|x=12.5|y=4|z=0|yaw=90|team=0\n"
    "SPAWN|map=0|x=3|y=20.25|z=1.5|yaw=-45|team=2\nSPAWN|map=0|x=6|y=6\n")!=std::string::npos&&first.find("\nFORMAT|2\n")!=std::string::npos,
    "the exporter writes MULTIPLAYER and every SPAWN back byte for byte, in order, under FORMAT|2");
   auto again=loadCustomCampaignText(first,"multiplayer-again.rmc");const auto second=exportRmc(again->name,{{again->maps[0],-1}},0).text;
   const auto& b=again->multiplayer;bool same=b.spawns.size()==mp.spawns.size()&&b.deathmatch==mp.deathmatch&&b.coop==mp.coop&&b.deathmatchMap==mp.deathmatchMap&&b.maxPlayers==mp.maxPlayers&&
    b.fragLimit==mp.fragLimit&&b.timeLimitMinutes==mp.timeLimitMinutes&&b.friendlyFire==mp.friendlyFire;
   for(size_t i=0;same&&i<b.spawns.size();++i){const auto&x=b.spawns[i];const auto&y=mp.spawns[i];same=x.map==y.map&&x.x==y.x&&x.y==y.y&&x.z==y.z&&x.yaw==y.yaw&&x.team==y.team;}
   check(first==second&&same,"export, parse, export writes the same file and reads the same settings and spawns");
   {Game g(c);for(int i=0;i<10;++i)g.update({},1.f/60);check(g.m_level==0,"a campaign with MULTIPLAYER and SPAWN records still plays alone");}
   auto refusal=[&](const std::string& head,const std::string& body,const std::string& expect,const std::string& what){const auto why=refused(room(head,body),expect);check(why.empty(),what+" ("+why+")");};
   refusal("","MULTIPLAYER|deathmatch=1\n","FORMAT|2","MULTIPLAYER in a version 1 file is refused, saying it needs FORMAT|2");
   refusal("FORMAT|2\n","MULTIPLAYER|1|coop=1\n","named key=value fields only","a positional MULTIPLAYER field is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|deathmatch=2\n","deathmatch=2","a mode that is not 1 or 0 is refused, naming it");
   refusal("FORMAT|2\n","MULTIPLAYER|maxplayers=40\n","maxplayers=40","more than 16 players is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|maxplayers=1\n","maxplayers=1","fewer than 2 players is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|fraglimit=2.5\n","whole number","a fraction of a frag is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|timelimit=-1\n","timelimit=-1","a negative time limit is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|lives=3\n","no field lives","an unknown MULTIPLAYER field is refused, naming it");
   refusal("FORMAT|2\n","MULTIPLAYER|coop=1\nMULTIPLAYER|coop=0\n","one MULTIPLAYER","a second MULTIPLAYER record is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|coop=1|coop=0\n","twice","a field given twice is refused");
   refusal("FORMAT|2\n","MULTIPLAYER|dmmap=5\n","dmmap=5","a deathmatch map the campaign does not have is refused");
   refusal("FORMAT|2\n","SPAWN|map=0|x=4\n","needs y","a SPAWN without y is refused");
   refusal("FORMAT|2\n","SPAWN|map=3|x=4|y=4\n","map=3","a SPAWN on a map the campaign does not have is refused");
   refusal("FORMAT|2\n","SPAWN|map=0|x=30|y=4\n","x=30","a SPAWN outside its map is refused");
   refusal("FORMAT|2\n","SPAWN|map=0|x=4|y=4|team=9\n","team=9","a team past 4 is refused");
   refusal("FORMAT|2\n","SPAWN|map=0|x=4|y=4|yaw=north\n","yaw=north","a turn that is not degrees is refused");
   {std::string many;for(int i=0;i<65;++i)many+="SPAWN|map=0|x=4|y=4\n";refusal("FORMAT|2\n",many,"too many SPAWN","more than 64 spawns are refused");}
   // The Starter Map shows it off: a MULTIPLAYER record and 8 spawns in the Ground paint yard.
   const auto starter=campaignWorkspace()/"custom maps"/"Starter Map.rmc";std::error_code ec;
   if(!campaignWorkspace().empty()&&std::filesystem::exists(starter,ec)){auto s=loadCustomCampaignFile(starter);const auto& sm=s->multiplayer;
    const bool yard=sm.deathmatchMap>=0&&sm.deathmatchMap<int(s->maps.size())&&s->maps[size_t(sm.deathmatchMap)]->name=="Starter Map / Ground paint";
    int there=0;for(const auto& sp:sm.spawns)there+=sp.map==sm.deathmatchMap;
    check(yard&&sm.spawns.size()==8&&there==8,"the Starter Map plays deathmatch in its Ground paint yard with 8 spawns there ("+std::to_string(there)+")");
    // Where a player is put there: the walked support under the spot (Game puts the start on it).
    if(yard){World w(sm.deathmatchMap,s->maps[size_t(sm.deathmatchMap)]);float worst=0;std::string under;
     for(const auto& sp:sm.spawns){const float floor=w.supportBelow(sp.x,sp.y,sp.z+2);worst=std::max(worst,std::fabs(floor-sp.z));under+=(under.empty()?"":", ")+std::to_string(floor).substr(0,5);}
     check(worst<.05f,"each Starter spawn stands at its floor's height ("+std::to_string(worst)+" m off at worst; floors "+under+")");}}}
 }catch(const std::exception& e){check(false,std::string("threw: ")+e.what());}
 report<<(failed?"FAILED ":"PASSED ")<<failed<<" failures\n";
 return failed==0;
}
}
