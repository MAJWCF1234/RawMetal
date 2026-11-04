#include "Game.h"
#include "../renderer/SoftwareRenderer.h"
#include "../world/CustomCampaign.h"
#include "../world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <queue>
#include <unordered_set>
namespace retro {
bool Game::testChapterSequences(){
 const auto started=std::chrono::steady_clock::now();
 std::ofstream out("chapter-sequence-test.txt");int passed=0;
 constexpr float dt=1.f/120;
 auto place=[](Game& g,int level,Vec2 pos,float z){
  g.setDeveloperMode(true);g.loadLevel(level,false);
  g.m_player.pos=pos;g.m_player.z=z;g.m_player.grounded=true;
  g.m_player.verticalVelocity=0;g.m_player.pitch=0;g.m_player.angle=0;
 };
 auto check=[&](bool ok,int number,const char* name)->std::ostream&{
  passed+=ok;out<<(ok?"PASS":"FAIL")<<" check="<<number<<" "<<name;return out;
 };
 auto location=[&](const Game& g){out<<" level="<<g.level()<<" pos="<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z;};
 // Poll every game frame, including the intervals that must remain quiet.
 // Separate games keep earlier answers from satisfying later assertions.
 {
  Game g;place(g,39,{13,12},-28);g.setState(stateId("pwr_fuel_transfer"));
  float first=-1;for(int frame=1;frame<=12;++frame){g.update({},dt);if(first<0&&g.state("pwr_fuel_ms")>0)first=frame*dt;}
  check(first>0&&first<=.1f,1,"fuel answer")<<" start_s="<<first<<" pwr_fuel_ms="<<g.state("pwr_fuel_ms");location(g);out<<'\n';
 }
 {
  Game g;place(g,39,{13,12},-28);g.setState(stateId("pwr_set2_cranked"));
  float done=-1,running=-1;bool early=false;int at1400=0;
  for(int frame=1;frame<=216;++frame){g.update({},dt);float t=frame*dt;
   int value=g.state("pwr_set2_ms_done");if(t<1.4f&&value!=0)early=true;
   if(frame==168)at1400=value;
   if(done<0&&value==1)done=t;
   if(running<0&&g.state("pwr_set2_running")>0)running=t;
  }
  check(!early&&done>=1.5f&&done<=1.7f&&running>=done&&running<=done+.1f&&g.state("pwr_set2_ms_done")==1,2,"Set 2 catches then runs")
   <<" done_s="<<done<<" done_at_1.4s="<<at1400<<" early="<<early<<" running_s="<<running
   <<" pwr_set2_ms="<<g.state("pwr_set2_ms")<<" pwr_set2_ms_done="<<g.state("pwr_set2_ms_done")<<" pwr_set2_running="<<g.state("pwr_set2_running");location(g);out<<'\n';
 }
 {
  Game g;place(g,39,{13,12},-28);g.setState(stateId("pwr_gen_online"));
  float mains=-1,strike=-1;bool early=false;int at800=0;
  for(int frame=1;frame<=132;++frame){g.update({},dt);float t=frame*dt;
   if(mains<0&&g.state("pwr_lc_mains")==1)mains=t;
   if(frame<=96&&g.state("pwr_strike_ms_done")!=0)early=true;
   if(frame==96)at800=g.state("pwr_strike_ms_done");
   if(strike<0&&g.state("pwr_strike_ms_done")==1)strike=t;
  }
  check(mains>0&&mains<=.1f&&!early&&strike>=.85f&&strike<=1.05f&&g.state("pwr_strike_ms_done")==1,3,"generation supply and delayed strike")
   <<" mains_s="<<mains<<" strike_s="<<strike<<" done_at_0.8s="<<at800<<" early="<<early
   <<" pwr_lc_mains="<<g.state("pwr_lc_mains")<<" pwr_strike_ms="<<g.state("pwr_strike_ms")<<" pwr_strike_ms_done="<<g.state("pwr_strike_ms_done");location(g);out<<'\n';
 }
 {
  Game g;g.setState(stateId("pwr_gen_online"));place(g,40,{12,18},-19);
  float entry=-1;for(int frame=1;frame<=12;++frame){g.update({},dt);if(entry<0&&g.state("pwr_lc_mains")==1)entry=frame*dt;}
  int entered=g.state("pwr_lc_mains");g.setState(stateId("pwr_transfer_bus"));
  float dip=-1,back=-1;int at100=0;
  for(int frame=1;frame<=120;++frame){g.update({},dt);float t=frame*dt;
   if(dip<0&&g.state("pwr_lc_mains")==2)dip=t;
   if(frame==12)at100=g.state("pwr_lc_mains");
   if(dip>0&&back<0&&g.state("pwr_lc_mains")==1)back=t;
  }
  check(entry>0&&entry<=.1f&&entered==1&&dip>0&&dip<=.1f&&at100==2&&back>=.65f&&back<=.85f&&g.state("pwr_lc_mains")==1,4,"Load Control transfer dip and recovery")
   <<" entry_s="<<entry<<" entry_mains="<<entered<<" dip_s="<<dip<<" mains_at_0.1s="<<at100<<" back_s="<<back
   <<" pwr_lc_mains="<<g.state("pwr_lc_mains")<<" pwr_lc_dip_ms="<<g.state("pwr_lc_dip_ms")<<" pwr_lc_dip_ms_done="<<g.state("pwr_lc_dip_ms_done");location(g);out<<'\n';
 }
 const char* timers[]={"mh_bay_a_ms","mh_bay_b_ms","mh_bay_c_ms","mh_bay_d_ms"};
 const char* rows[]={"mh_bay_a","mh_bay_b","mh_bay_c","mh_bay_d"};
 {
  Game g;g.setState(stateId("pwr_feeder9"));place(g,42,{12,12},-28);
  float first=-1,finished[4]={-1,-1,-1,-1};bool ordered=true;int startValue=0;
  for(int frame=1;frame<=840;++frame){g.update({},dt);
   if(first<0&&g.state(timers[0])>0){first=frame*dt;startValue=g.state(timers[0]);}
   for(int i=0;i<4;++i)if(g.state(rows[i])==1){
    if(finished[i]<0)finished[i]=frame*dt;
    for(int j=0;j<i;++j)if(g.state(rows[j])!=1)ordered=false;
   }
  }
  bool complete=true;for(int i=0;i<4;++i)complete&=g.state(rows[i])==1&&finished[i]>0&&finished[i]<=7;
  for(int i=1;i<4;++i)ordered&=finished[i]>finished[i-1];
  check(first>0&&first<=.1f&&complete&&ordered&&g.level()==42&&std::fabs(g.player().z+28)<.1f,5,"exciter floor restrike in row order")
   <<" start_s="<<first<<" start_timer="<<startValue<<" ordered="<<ordered;
  for(int i=0;i<4;++i)out<<' '<<rows[i]<<'='<<g.state(rows[i])<<" at_s="<<finished[i]<<' '<<timers[i]<<'='<<g.state(timers[i]);location(g);out<<'\n';

  // Revisit the original trigger by walking east along the gallery. Observe
  // every frame so even a momentary reset cannot hide behind the final value.
  int before[4],minimum[4],previous[4];for(int i=0;i<4;++i)before[i]=minimum[i]=previous[i]=g.state(timers[i]);
  int beforeD=g.state(rows[3]),minimumD=beforeD;bool stable=true,reached=false;
  place(g,41,{19,3.2f},-25);InputState walking{};walking.forward=true;
  auto sample=[&]{
   minimumD=std::min(minimumD,g.state(rows[3]));stable&=g.state(rows[3])==1;
   for(int i=0;i<4;++i){int value=g.state(timers[i]);minimum[i]=std::min(minimum[i],value);stable&=value>=previous[i]&&value>100;previous[i]=value;}
  };
  sample();
  for(int frame=0;frame<240;++frame){
   reached|=g.level()==41&&g.player().pos.x>=19.5f&&g.player().pos.x<=25&&g.player().pos.y>=2.4f&&g.player().pos.y<=4&&std::fabs(g.player().z+25)<.1f;
   g.update(g.player().pos.x<22?walking:InputState{},dt);sample();
  }
  check(complete&&beforeD==1&&stable&&reached,6,"gallery revisit does not restart restrike")<<" reached="<<reached<<" bay_d_before="<<beforeD<<" bay_d_min="<<minimumD<<" bay_d_after="<<g.state(rows[3]);
  for(int i=0;i<4;++i)out<<' '<<timers[i]<<"_before="<<before[i]<<" min="<<minimum[i]<<" after="<<g.state(timers[i]);location(g);out<<'\n';
 }
 {
  Game g;g.setState(stateId("pwr_feeder9"));place(g,41,{12,3},-25);
  int maximum[4]={};bool quiet=true;
  for(int frame=0;frame<120;++frame){g.update({},dt);for(int i=0;i<4;++i){maximum[i]=std::max(maximum[i],g.state(timers[i]));quiet&=g.state(timers[i])==0;}}
  bool west=g.level()==41&&std::fabs(g.player().z+25)<.1f;Vec2 westPos=g.player().pos;float westZ=g.player().z;
  // Move the player to the authored east-end box without reloading or
  // clearing state; the quiet west visit must not consume the trigger.
  g.m_player.pos={22,3.2f};g.m_player.z=-25;g.m_player.verticalVelocity=0;g.m_player.grounded=true;
  float first=-1;for(int frame=1;frame<=12;++frame){g.update({},dt);if(first<0&&g.state(timers[0])>0)first=frame*dt;}
  check(quiet&&west&&first>0&&first<=.1f,7,"west gallery quiet then east end starts restrike")<<" west_pos="<<westPos.x<<','<<westPos.y<<','<<westZ;
  for(int i=0;i<4;++i)out<<' '<<timers[i]<<"_west_max="<<maximum[i]<<" east="<<g.state(timers[i]);out<<" east_start_s="<<first;location(g);out<<'\n';
 }
 out<<"SUMMARY "<<passed<<"/7 PASS "<<7-passed<<" FAIL runtime="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"s\n";
 out.flush();return passed==7&&out.good();
}
bool Game::testChapterRoute(){
 const auto started=std::chrono::steady_clock::now();
 std::ofstream out("chapter-route-test.txt");
 struct Edge{int chunk;char side;float a,b,z;bool isDoor;};
 static constexpr Edge edges[]={
#include "ChapterEdges.inc"
 };
 int passed=0;
 auto pressUse=[](Game& g){InputState key{};key.use=true;g.updateInteraction({},.01f);if(g.logTime()>0){g.updateInteraction(key,.01f);g.updateInteraction({},.01f);}g.updateInteraction(key,.01f);g.updateInteraction({},.01f);};
 for(const auto& edge:edges){
  Game g;g.setDeveloperMode(true);g.loadLevel(edge.chunk,false);
  const float middle=(edge.a+edge.b)*.5f;
  Vec2 boundary{},direction{};
  switch(edge.side){
   case 'N':boundary={middle,0};direction={0,-1};break;
   case 'S':boundary={middle,24};direction={0,1};break;
   case 'E':boundary={24,middle};direction={1,0};break;
   case 'W':boundary={0,middle};direction={-1,0};break;
  }
  const Vec2 target=g.chunkOffset(edge.chunk)+boundary+direction*2.f;
  const int neighbour=campaignChunkAt(target);
  g.m_player.pos=boundary-direction*1.5f;
  g.m_player.z=g.groundHeight(g.m_player.pos,edge.z+.215f);
  g.m_player.grounded=true;g.m_player.verticalVelocity=0;
  g.m_player.pitch=0;g.m_player.angle=std::atan2(direction.y,direction.x);
  InputState walking{};walking.forward=true;
  walking.crouch=(edge.chunk==57&&edge.side=='S')||(edge.chunk==58&&edge.side=='N');
  g.m_player.crouched=walking.crouch;
  const float startZ=g.player().z;
  const bool startFits=g.hullFits(g.player().pos,g.player().z,g.player().hullHeight());
  bool crossed=false;int frames=0,uses=0;
  // Same world-space steering and 120 Hz update pattern as moveTo below.
  // Only a real chunk ownership transition counts; approaching the seam does not.
  for(;neighbour>=0&&neighbour!=edge.chunk&&frames<720;++frames){
   Vec2 delta=target-(g.player().pos+g.chunkOffset(g.level()));
   g.m_player.angle=std::atan2(delta.y,delta.x);
   const auto before=g.player().pos+g.chunkOffset(g.level());
   g.update(walking,1.f/120);
   if(g.level()==neighbour){crossed=true;++frames;break;}
   const auto after=g.player().pos+g.chunkOffset(g.level());
   if(length(after-before)<.0001f){
    int door=g.nearbyInteractionDoor();
    if(door>=0&&!g.world().doors()[door].opening){pressUse(g);++uses;}
   }
  }
  const bool ok=crossed&&startFits;passed+=ok;
  out<<(ok?"PASS":"FAIL")<<" chunk="<<edge.chunk<<" side="<<edge.side<<" span="<<edge.a<<".."<<edge.b<<" z="<<edge.z
     <<" kind="<<(edge.isDoor?"door":"seam")<<" neighbour="<<neighbour<<" stopped="<<g.level()<<':'<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z
     <<" startZ="<<startZ<<" crouch="<<walking.crouch<<" time="<<frames/120.f<<" uses="<<uses;
  if(!ok){
   out<<" blocked=";
   if(neighbour<0)out<<"no neighbouring rectangle;";
   if(!startFits)out<<"initial hull obstructed;";
   const Vec2 probe=g.player().pos+direction*.06f;
   const float feet=g.player().z,height=g.player().hullHeight();
   bool found=false;
   for(float x:{-.20f,0.f,.20f})for(float y:{-.20f,0.f,.20f}){
    Vec2 p=probe+Vec2{x,y};const auto& w=g.worldAt(p);
    if(w.doorBlocks(p.x,p.y,feet,height)){out<<"door map="<<w.level()<<" at="<<p.x<<','<<p.y<<';';found=true;}
    if(!w.fits(p.x,p.y,feet,height)){
     const float support=w.supportBelow(p.x,p.y,feet+.215f),ceiling=w.clearanceAbove(p.x,p.y,feet);
     out<<"solid map="<<w.level()<<" sample="<<p.x<<','<<p.y<<" tile="<<w.tile(int(std::floor(p.x)),int(std::floor(p.y)))
        <<" support="<<support<<" ceiling="<<ceiling<<';';found=true;
     if(support>feet+.215f)out<<"floor rise="<<support-feet<<" exceeds walking step;";
     if(feet+height>ceiling+.005f)out<<"headroom="<<ceiling-feet<<" below hull="<<height<<';';
     for(size_t i=0;i<w.structures().size();++i){const auto& s=w.structures()[i];
      if(p.x>s.x1&&p.x<s.x2&&p.y>s.y1&&p.y<s.y2&&feet<s.top&&feet+height>s.bottom)
       out<<"structure#"<<i<<" bounds="<<s.x1<<','<<s.y1<<','<<s.bottom<<".."<<s.x2<<','<<s.y2<<','<<s.top<<" material="<<s.material<<';';
     }
    }
   }
   Vec2 local=probe;const auto& w=g.worldAt(local);
   if(w.railBlocksHull(local.x,local.y,.20f,feet,height)){out<<"rail map="<<w.level()<<';';found=true;}
   if(!found)out<<"no static contact at forward probe (actor, interaction, or transition);";
  }
  out<<'\n';out.flush();
 }
 const int total=int(std::size(edges));
 out<<"SUMMARY "<<passed<<'/'<<total<<" PASS "<<total-passed<<" FAIL runtime="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"s\n";
 return passed==total&&out.good();
}
bool Game::testCampaignMaps(){
 std::ofstream out("freight-district-test.txt");bool pass=true;
 auto check=[&](bool ok,const char* name){out<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 check(FreightDistrictEnd==32&&CampaignChunkCount>=FreightDistrictEnd+2,"Eight freight areas allocated across 22 additional stitched chunks");
 // Chapter 2 is sealed behind the great door until it ships; UNLOCK while
 // looking at the seal opens it for a playtester.
 {Game g;g.m_level=68;g.restart();const auto&doors=g.world().doors();
  bool sealed=!doors.empty()&&doors[0].requireState==stateId("chapter2_open")&&g.doorLocked(doors[0]);check(sealed,"Research Intake's inner door is sealed in an ordinary playthrough");
  if(sealed){g.m_player.pos={12.f,1.6f};g.m_player.z=g.world().floorHeight(12.f,1.6f);g.m_player.angle=-kPi*.5f;
   g.executeConsole("unlock");check(!g.doorLocked(g.world().doors()[0])&&g.state(stateId("chapter2_open"))==1,"UNLOCK while facing the seal opens Chapter 2");}}
 {Game g;g.m_level=67;g.restart();g.ensureChunk(68);int i=int(g.world().doors().size())-1;g.useDoor(i);g.updateStreaming(.1f);check(!g.state(stateId("chapter2_open"))&&g.m_chunks[68].world.doors()[0].open==0&&!g.m_chunks[68].world.doors()[0].opening,"opening chunk 67 never mirrors through the locked Chapter 2 seal");}
 // Chapter 2's first beats, played on their clocks.
 {Game g;g.m_level=68;g.restart();g.m_player.pos={12.f,2.2f};g.m_player.z=g.world().floorHeight(12.f,2.2f);g.m_god=true;
  for(int i=0;i<60*15;++i)g.update({},1.f/60);
  check(g.state(stateId("rf_intake_sweep"))==1,"Research Intake: the Investigator's radio order sends the sweep (13 s after the vestibule)");}
 {Game g;g.m_level=69;g.restart();g.m_player.pos={12.f,2.f};g.m_player.z=g.world().floorHeight(12.f,2.f);
  for(int i=0;i<30;++i)g.update({},1.f/60);
  int talker=-1;for(size_t i=0;i<g.m_friendlyActors.size();++i)if(g.m_friendlyActors[i].track().talk==stateId("proc_hello"))talker=int(i);
  check(talker>=0&&g.npcStance(g.m_friendlyActors[size_t(talker)].track())==SquadStance::Truce,"Personnel Processing: the wounded private is on a truce and has something to say");}
 {Game g;g.m_level=70;g.restart();g.m_player.pos={21.f,11.5f};g.m_player.z=g.world().floorHeight(21.f,11.5f);g.m_god=true;
  bool cast=false;for(const auto&t:g.world().actorTracks())cast|=t.visual==ActorVisual::Investigator;
  check(cast,"Research Administration: the Investigator is in the records office");
  check(g.doorLocked(g.world().doors()[1]),"Records stays shut while he is in it");
  for(int i=0;i<60*28;++i)g.update({},1.f/60);
  check(g.state(stateId("rf_admin_gone"))==1&&!g.doorLocked(g.world().doors()[1]),"He leaves by the far door, and records opens after");
  check(g.world().doors()[2].open<.05f,"The far door shuts behind him");}
 // Seen from the atrium, out of reach: at the drawers through the hatch, at the
 // table through the south bay. A shot at him cracks the protective glass and
 // stops; he flinches and the scene carries on.
 {Game g;g.m_level=70;g.restart();g.m_player.pos={21.f,11.5f};g.m_player.z=g.world().floorHeight(21.f,11.5f);g.m_god=true;
  size_t inv=0;for(size_t i=0;i<g.world().actorTracks().size();++i)if(g.world().actorTracks()[i].visual==ActorVisual::Investigator)inv=i;
  const auto timer=g.world().actorTracks()[inv].timerState;const auto flinch=actorTrackState(timer,inv,"flinch_ms");
  auto at=[&](int ms){for(int i=0;i<60*40&&g.state(timer)<ms;++i)g.update({},1.f/60);};
  auto watch=[&](Vec2 from,const char*where){const auto pose=g.world().actorPose(inv);const float floor=g.world().floorHeight(from.x,from.y);
   Point3 eye{from.x,from.y,floor+1.55f},chest{pose.position.x,pose.position.y,pose.z+1.1f};auto ray=chest-eye;
   check(g.world().lightRayClear(from,eye.z,pose.position,pose.z+1.6f)&&g.world().lightRayClear(from,eye.z,pose.position,chest.z),(std::string("Research Administration: his head and chest are in view from the atrium ")+where).c_str());
   auto hit=g.traceShot(eye,ray,30);const auto&panes=g.world().breakables();
   check(hit.kind==ShotKind::Solid&&hit.breakable>=0&&size_t(hit.breakable)<panes.size()&&panes[size_t(hit.breakable)].holds&&hit.point.x>11.8f,(std::string("Research Administration: a shot at him ")+where+" stops on the protective glass").c_str());
   g.resolveShot(hit,eye,ray,weaponDef(WeaponId::Shotgun).hitscan,true);g.update({},1.f/60);
   check(g.state(flinch)>0&&g.world().actorPose(inv).clip==3,(std::string("Research Administration: he flinches from a shot ")+where).c_str());
   for(int i=0;i<40;++i)g.update({},1.f/60);
   check(g.state(flinch)==0&&g.world().actorPose(inv).clip!=3&&!g.state(g.world().actorTracks()[inv].deadState),(std::string("Research Administration: and goes back to his business ")+where).c_str());};
  at(3000);watch({16.5f,11.85f},"through the hatch");
  at(15000);watch({14.8f,14.4f},"through the south bay");
  for(int i=0;i<60*15;++i)g.update({},1.f/60);
  check(g.state(stateId("rf_admin_gone"))==1,"Research Administration: shooting at him does not stop him leaving");}
 // Triage: the patient stays down while he is watched, and sits up once the player turns to the nurse.
 {Game g;g.m_level=72;g.restart();g.m_player.pos={6.7f,9.8f};g.m_player.z=g.world().floorHeight(6.7f,9.8f);g.m_player.angle=std::atan2(-1.75f,-.95f);g.m_player.pitch=-20;
  for(int i=0;i<180;++i)g.update({},1.f/60);
  const bool down=g.state(stateId("md_triage_rise"))==0&&g.world().actorPose(1).phase>.95f;
  g.m_player.pos={4.5f,6.f};g.m_player.z=g.world().floorHeight(4.5f,6.f);g.m_player.angle=std::atan2(1.85f,-1.75f);g.m_player.pitch=-17;
  for(int i=0;i<240;++i)g.update({},1.f/60);
  out<<"triage: down="<<down<<" rise="<<g.state(stateId("md_triage_rise"))<<" phase="<<g.world().actorPose(1).phase<<" actors="<<g.world().actorTracks().size()<<'\n';
  check(down&&g.state(stateId("md_triage_rise"))>0&&g.world().actorPose(1).clip==5,"Triage: the patient sits up behind the player who turns to the nurse");}
 // The deep wing, 74-78: each set piece played on its clock, the gates held
 // against a standing player's reach, and the 9 mm taken where it lies.
 {const StateId dark=stateId("med_blackout");
  auto fresh=[](Game& g,int level){g.m_level=level;g.restart();g.m_god=true;g.setState(stateId("chapter2_open"));g.setState(stateId("med_override"));};
  auto put=[](Game& g,Vec2 p,float z,float yaw){g.m_player.pos=p;g.m_player.z=z;g.m_player.angle=yaw;g.m_player.pitch=0;g.m_player.grounded=true;g.m_player.verticalVelocity=0;g.m_player.crouched=false;};
  auto run=[](Game& g,float seconds){for(int i=0,n=int(seconds*60+.5f);i<n;++i)g.update({},1.f/60);};
  auto lampAt=[](const Game& g,Vec2 p,int mount=-1)->const WorldLight*{for(const auto& l:g.world().lights())if(length(l.position-p)<.05f&&(mount<0||int(l.mount)==mount))return &l;return nullptr;};
  auto on=[&](const Game& g,Vec2 p,int mount=-1){const auto* l=lampAt(g,p,mount);return l&&g.lampSwitchedOn(*l);};
  auto off=[&](const Game& g,Vec2 p,int mount=-1){const auto* l=lampAt(g,p,mount);return l&&!g.lampSwitchedOn(*l);};
  auto doorAt=[](const Game& g,float left,float right,float y){const auto& d=g.world().doors();for(size_t i=0;i<d.size();++i)if(std::fabs(d[i].left-left)<.02f&&std::fabs(d[i].right-right)<.02f&&std::fabs(d[i].y-y)<.02f)return int(i);return -1;};
  auto locked=[](const Game& g,int d){return d>=0&&g.doorLocked(g.world().doors()[size_t(d)]);};
  auto opened=[](const Game& g,int d){return d>=0&&g.world().doors()[size_t(d)].opening;};
  auto terminalFor=[](const Game& g,StateId activate){const auto& t=g.world().terminals();for(size_t i=0;i<t.size();++i)if(t[i].activateState==activate)return int(i);return -1;};
  auto press=[](Game& g){InputState key{};key.use=true;g.updateInteraction({},.01f);if(g.logTime()>0){g.updateInteraction(key,.01f);g.updateInteraction({},.01f);}g.updateInteraction(key,.01f);g.updateInteraction({},.01f);};
  // Stand where E reaches terminal `index`, on its `toward` side, facing it; press E when asked.
  auto approach=[&](Game& g,int index,Vec2 toward,bool use)->bool{
   if(index<0)return false;const Vec2 at=g.world().terminals()[size_t(index)].position;const float base=std::atan2(toward.y,toward.x);
   int doorFirst=0;
   for(float r:{.7f,.9f,1.1f,1.3f,.55f,1.5f,1.7f})for(float a:{0.f,.3f,-.3f,.6f,-.6f,.9f,-.9f,1.2f,-1.2f,1.45f,-1.45f}){
    const Vec2 p=at+Vec2{std::cos(base+a),std::sin(base+a)}*r;const float z=g.groundHeight(p,g.world().floorHeight(p.x,p.y)+.215f);
    if(!g.hullFits(p,z,Player::StandingHeight))continue;
    put(g,p,z,std::atan2(at.y-p.y,at.x-p.x));if(g.nearbyTerminal()!=index)continue;
    // E takes a door in front before a terminal, as the game does.
    if(g.nearbyInteractionDoor()>=0){++doorFirst;continue;}
    if(use)press(g);if(doorFirst)out<<"    terminal "<<index<<" taken with E at "<<p.x<<','<<p.y<<" after "<<doorFirst<<" nearer spots where a door takes E first\n";return true;}
   out<<"    terminal "<<index<<" at "<<at.x<<','<<at.y<<": no standing spot reaches it with E ("<<doorFirst<<" spots where a door takes E first)\n";
   return false;};
  // Every cell a standing player walks to from `starts` (1/8 m grid, .3 m steps), as the freight route check floods.
  struct Spot{Vec2 p;float z;};
  auto flood=[](const Game& g,std::vector<Spot> starts){
   struct Node{int x,y;float z;};std::queue<Node> queue;std::unordered_set<long long> seen;std::vector<Spot> reached;
   auto key=[](Node n){return (static_cast<long long>(std::lround((n.z+100)*1000))<<16)|(n.y*192+n.x);};
   for(const auto& s:starts){Node n{int(s.p.x*8),int(s.p.y*8),s.z};if(seen.insert(key(n)).second)queue.push(n);}
   while(!queue.empty()){auto a=queue.front();queue.pop();reached.push_back({{(a.x+.5f)*.125f,(a.y+.5f)*.125f},a.z});
    for(auto d:{Vec2{-1,0},Vec2{1,0},Vec2{0,-1},Vec2{0,1}}){Node b{a.x+int(d.x),a.y+int(d.y),a.z};if(b.x<2||b.x>=190||b.y<2||b.y>=190)continue;
     const Vec2 q{(b.x+.5f)*.125f,(b.y+.5f)*.125f};b.z=g.groundHeight(q,a.z+.215f);
     if(std::fabs(b.z-a.z)>.3f||!g.hullFits(q,b.z,Player::StandingHeight)||!seen.insert(key(b)).second)continue;queue.push(b);}}
   return reached;};
  auto openDoors=[](Game& g,bool lockedToo){for(size_t d=0;d<g.world().doors().size();++d)if(lockedToo||!g.doorLocked(g.world().doors()[d]))g.m_world.openDoor(int(d));for(int i=0;i<4;++i)g.m_world.updateDoors(1);};
  auto setsState=[](const ScriptEvent& e,StateId id,int value){return std::any_of(e.actions.begin(),e.actions.end(),[&](const ScriptAction& a){return a.type==ScriptAction::Type::SetState&&a.id==id&&a.value==value;});};
  auto opensDoor=[](const ScriptEvent& e,int door){return std::any_of(e.actions.begin(),e.actions.end(),[&](const ScriptAction& a){return a.type==ScriptAction::Type::OpenDoor&&a.index==door;});};
  auto enemyNear=[](const Game& g,Vec2 p){for(size_t i=0;i<g.enemies().size();++i)if(length(g.enemies()[i].pos-p)<.3f)return int(i);return -1;};
  auto awake=[](const Game& g,int i){return i>=0&&size_t(i)<g.enemies().size()&&g.enemies()[size_t(i)].wakeState==0;};
  auto dormant=[](const Game& g,int i){return i>=0&&size_t(i)<g.enemies().size()&&g.enemies()[size_t(i)].wakeState!=0;};
  // Every 9 mm box is taken by a standing player who walked there from the map's entry, every door opened.
  for(int level=74;level<=78;++level){Game g;fresh(g,level);openDoors(g,true);
   // Heavy dressing first stands as an obstacle; a box out of reach that way is tried again with the
   // props yielding, as the engine lets them after half a second of shoving.
   const Spot entry{g.player().pos,g.player().z};const auto reached=flood(g,{entry});int boxes=0,taken=0;std::vector<Spot> shoved;
   auto take=[&](size_t i,const std::vector<Spot>& cells,Spot& from){const Vec2 box=g.m_pickups[i].pos;
    for(const auto& s:cells){if(length(s.p-box)>.85f)continue;put(g,s.p,s.z,length(box-s.p)>.01f?std::atan2(box.y-s.p.y,box.x-s.p.x):0.f);g.updatePickups();if(!g.m_pickups[i].active){from=s;return true;}}
    return false;};
   for(size_t i=0;i<g.m_pickups.size();++i){if(g.m_pickups[i].kind!=PickupKind::HandgunAmmo)continue;++boxes;const Vec2 box=g.m_pickups[i].pos;const float boxZ=g.pickupHeight(g.m_pickups[i]);Spot from{};
    bool got=take(i,reached,from),pushed=false;
    if(!got){out<<"  map "<<level<<" 9 mm at "<<box.x<<','<<box.y<<','<<boxZ<<" not taken with the props standing; reached cells within 1.2 m:";
     for(const auto& s:reached)if(length(s.p-box)<1.2f)out<<' '<<s.p.x<<','<<s.p.y;out<<'\n';     if(shoved.empty()){for(auto& c:g.m_clutter)c.blockedTime=1;shoved=flood(g,{entry});}
     pushed=got=take(i,shoved,from);}
    taken+=got;out<<"  map "<<level<<" 9 mm at "<<box.x<<','<<box.y<<','<<boxZ;
    if(got)out<<" taken standing at "<<from.p.x<<','<<from.p.y<<','<<from.z<<(pushed?" (past pushed props)":"")<<'\n';else out<<" NOT TAKEN\n";}
   check(boxes>0&&taken==boxes,(std::string("Deep wing ")+std::to_string(level)+": every 9 mm box is taken standing from the entry's reach (heavy props shoved if they must be)").c_str());
   // Its creatures stand clear where they wait, and have a step to take.
   bool clear=true;for(const auto& e:g.enemies()){const float h=e.bodyTop()-e.bodyBottom();bool step=false;
    for(int k=0;k<8;++k){const Vec2 n=e.pos+Vec2{std::cos(k*kPi/4),std::sin(k*kPi/4)}*.6f;step|=g.hullFits(n,g.groundHeight(n,e.z+.215f),h);}
    const bool fits=g.hullFits(e.pos,e.bodyBottom()+.03f,h-.03f);   // clear of floor paint and blood
    if(!fits||!step){out<<"  map "<<level<<" creature at "<<e.pos.x<<','<<e.pos.y<<','<<e.z<<" top="<<e.bodyTop()<<" fits="<<fits<<" step="<<step<<'\n';
     const auto& w=g.world();for(size_t i=0;i<w.structures().size();++i){const auto& s=w.structures()[i];if(e.pos.x+.2f>s.x1&&e.pos.x-.2f<s.x2&&e.pos.y+.2f>s.y1&&e.pos.y-.2f<s.y2&&e.bodyBottom()<s.top&&e.bodyTop()>s.bottom)
      out<<"    structure "<<s.x1<<','<<s.y1<<','<<s.bottom<<".."<<s.x2<<','<<s.y2<<','<<s.top<<" material="<<s.material<<'\n';}}
    clear&=fits&&step;}
   check(clear,(std::string("Deep wing ")+std::to_string(level)+": every creature has standing room at its spawn and beside it").c_str());}
  // --- 74 Pharmacy: the lit breather. Nothing here belongs to the blackout.
  {Game g;fresh(g,74);const auto& w=g.world();
   bool blackout=!w.darkZones().empty()||!w.creatureSpawns().empty();for(const auto& l:w.lights())blackout|=l.enabledState==dark;
   for(const auto& e:w.scriptEvents()){blackout|=e.requireState==dark||setsState(e,dark,1);}
   bool fittings=w.lights().size()<=30;for(const auto& l:w.lights())if(l.mount==WorldLightMount::Bare&&l.prop<0){bool held=false;for(const auto& f:w.fixtures())held|=length(f.position-l.position)<.25f;fittings&=held;if(!held)out<<"  74 bare lamp at "<<l.position.x<<','<<l.position.y<<" has no fitting\n";}
   check(!blackout&&fittings,"Pharmacy: no dark zone, creature, blackout lamp or blackout script; every bare lamp has its fitting");
   const int staff=doorAt(g,7.6f,8.5f,8.9f),office=doorAt(g,11.9f,12.8f,6.9f),gate=doorAt(g,3.6f,4.6f,3.5f);
   check(w.doors().size()==3&&staff>=0&&gate>=0&&office>=0&&w.doors()[size_t(staff)].requireState==stateId("medf_staff_door")&&w.doors()[size_t(gate)].requireState==stateId("medf_cage_open")&&
    locked(g,staff)&&locked(g,gate)&&!locked(g,office),"Pharmacy: three doors; the staff door and the cage gate start locked, the office does not");
   int hatches=0;bool hold=true,window=false;for(const auto& b:w.breakables()){if(b.y1>=8.75f&&b.y2<=9.05f&&b.x1>=1.55f&&b.x2<=6.65f){++hatches;hold&=b.holds;}window|=b.holds&&b.x1>11.3f&&b.x2<11.7f&&b.y1>3.5f&&b.y2<3.6f;}
   check(hatches>=2&&hold&&window,"Pharmacy: the hatch glass and the cold room window hold");
   bool sweep=true;for(const auto& c:w.clutterSpawns())if(c.position.y<3.5f&&c.position.x>3.5f&&c.position.x<4.7f&&length(c.position-Vec2{3.6f,3.5f})<1.f){sweep=false;out<<"  74 prop in the gate's swing at "<<c.position.x<<','<<c.position.y<<'\n';}
   check(sweep,"Pharmacy: nothing stands in the cage gate's swing");
   auto text=[&](const char* title)->std::string{for(const auto& t:w.terminals())if(t.title&&std::string_view(t.title)==title)return std::string(t.line1)+" "+t.line2;return "";};
   bool stale=false;for(const auto& t:w.terminals()){const std::string s=std::string(t.title)+" "+t.line1+" "+t.line2;stale|=s.find("22 BEDS")!=std::string::npos||s.find("BEDS 1-22")!=std::string::npos;}
   const int note=terminalFor(g,stateId("medf_cage_keyed"));
   check(text("COLD ROOM / HOLDING").find("BED 16")!=std::string::npos&&text("WARD C / NIGHT SEDATION / STANDING").find("BEDS 3-11")!=std::string::npos&&!stale&&
    note>=0&&std::string(w.terminals()[size_t(note)].line1).find("E /")==std::string::npos,"Pharmacy: the cold room holds bed 16, the sedation board says beds 3-11, and the time-lock note names no key");
   // The knock waits for the cold room door.
   run(g,1);const int before=g.state("medf_knock");put(g,{11.5f,5.f},-12,-kPi*.5f);g.update({},1.f/60);g.update({},1.f/60);
   check(before==0&&g.state("medf_knock")>0,"Pharmacy: the knocking starts at the cold room door, not before");
   // Under the shutter: crouched, not standing.
   check(g.hullFits({17.5f,10.9f},-12,Player::CrouchedHeight)&&!g.hullFits({17.5f,10.9f},-12,Player::StandingHeight)&&g.hullFits({17.5f,12.3f},-12,Player::StandingHeight)&&g.hullFits({17.5f,9.6f},-12,Player::StandingHeight),
    "Pharmacy: the jammed goods-in shutter clears a crouch and stops a standing player");
   InputState walk{};walk.forward=true;put(g,{17.5f,12.1f},-12,-kPi*.5f);for(int i=0;i<180;++i)g.update(walk,1.f/60);const float standingY=g.player().pos.y;
   walk.crouch=true;put(g,{17.5f,12.1f},-12,-kPi*.5f);g.m_player.crouched=true;float crouchY=99;for(int i=0;i<360;++i){g.update(walk,1.f/60);crouchY=std::min(crouchY,g.player().pos.y);}
   out<<"  74 shutter: standing stopped at y="<<standingY<<", crouched reached y="<<crouchY<<'\n';
   check(standingY>11.f&&crouchY<10.3f,"Pharmacy: walking crouched under the shutter reaches goods in; walking upright does not");}
  {Game g;fresh(g,74);const int staff=doorAt(g,7.6f,8.5f,8.9f);
   put(g,{8.05f,9.8f},-12,-kPi*.5f);run(g,.5f);press(g);run(g,.5f);
   const bool shut=locked(g,staff)&&g.world().doors()[size_t(staff)].open<.05f&&!g.state("medf_staff_door");
   put(g,{8.2f,8.f},-12,kPi*.5f);run(g,.2f);
   check(shut&&g.state("medf_staff_door")==1&&!locked(g,staff),"Pharmacy: the staff door will not open from the hall, and lets go from inside the dispensary");}
  {Game g;fresh(g,74);const int gate=doorAt(g,3.6f,4.6f,3.5f),note=terminalFor(g,stateId("medf_cage_keyed"));
   const bool keyed=approach(g,note,{0,1},true);const Vec2 spot=g.player().pos;const float spotZ=g.player().z,spotYaw=g.player().angle;
   float start=-1,open=-1,slam=-1;bool early=false;
   for(int frame=1;frame<=60*22;++frame){g.update({},1.f/60);const float t=frame/60.f;
    if(start<0&&g.state("medf_cage_timer")>0)start=t;
    if(open<0&&g.state("medf_cage_open"))open=t;else if(open<0&&!locked(g,gate))early=true;
    if(slam<0&&g.state("medf_cage_slammed"))slam=t;}
   out<<"  74 time lock: keyed="<<keyed<<" at "<<spot.x<<','<<spot.y<<" start_s="<<start<<" open_s="<<open<<" slam_s="<<slam<<" early="<<early<<'\n';
   check(keyed&&start>0&&start<=.05f&&!early&&open>=19.f&&open<=19.75f&&!locked(g,gate),"Pharmacy: E at the cage pad starts the time lock; the gate holds for 19.6 s, then releases");
   check(slam>=open+.7f&&slam<=open+1.f,"Pharmacy: the cold room door slams once, 0.8 s after the gate releases");
   const int timer=g.state("medf_cage_timer"),slamTimer=g.state("medf_cage_slam");put(g,spot,spotZ,spotYaw);const bool again=g.nearbyTerminal()==note;press(g);int low=timer,slamLow=slamTimer;
   for(int i=0;i<60;++i){g.update({},1.f/60);low=std::min(low,g.state("medf_cage_timer"));slamLow=std::min(slamLow,g.state("medf_cage_slam"));}
   out<<"  74 rekey: timer "<<timer<<" -> low "<<low<<", slam "<<slamTimer<<" -> low "<<slamLow<<'\n';
   check(again&&low>=timer&&slamLow>=slamTimer&&g.state("medf_cage_open")==1,"Pharmacy: keying the pad again replays nothing");}
  // --- 75/76 Ward C: one blackout, on the ward's own circuit.
  for(int level:{75,76}){Game g;fresh(g,level);const auto& w=g.world();const char* name=level==75?"Ward C north":"Ward C south";
   int reds=0,beacons=0,stray=0,bare=0;
   for(const auto& l:w.lights()){
    if(l.enabledState==dark&&l.invert){bare+=l.mount==WorldLightMount::Bare&&l.prop<0;continue;}
    if(l.enabledState==dark){++reds;stray+=!(l.mount==WorldLightMount::Wall&&l.kelvin<=1200);continue;}
    if(l.enabledState==stateId("medw_dead_circuit")&&!l.invert)continue;
    if(level==76&&!l.enabledState&&l.fitting==model::clinic_ceiling_lamp_1&&length(l.position-Vec2{20.6f,11.5f})<.05f){++beacons;continue;}
    ++stray;out<<"  "<<level<<" lamp off the ward circuit at "<<l.position.x<<','<<l.position.y<<'\n';}
   check(stray==0&&bare==0&&reds==(level==75?3:2)&&beacons==(level==76?1:0),(std::string(name)+": every lamp is on the ward circuit or a red emergency wall lamp; 76 keeps the one theatre beacon").c_str());
   bool zone=false;for(const auto& z:w.darkZones())zone|=z.state==dark&&z.value==1&&!z.invert&&z.jamFlashlight&&z.x1<=0&&z.y1<=0&&z.x2>=24&&z.y2>=24;
   check(zone,(std::string(name)+": a flashlight-jamming dark zone covers the whole map on med_blackout").c_str());
   struct Wake{Vec2 at;const char* state;};
   const std::vector<Wake> wakes=level==75?std::vector<Wake>{{{3.45f,9.65f},"medw_wake_n"},{{2.7f,14.85f},"med_blackout"},{{15.6f,15.47f},"med_blackout"}}:
    std::vector<Wake>{{{8.4f,9.84f},"medw_wake_c"},{{13.7f,16.95f},"medw_wake_b"},{{16.1f,20.f},"med_blackout"},{{21.4f,11.6f},"medw_wake_d"}};
   int transformed=0,matched=0;for(const auto& s:w.creatureSpawns()){transformed+=s.kind==CreatureKind::Transformed;for(const auto& k:wakes)matched+=s.kind==CreatureKind::Transformed&&length(s.position-k.at)<.1f&&s.wakeState==stateId(k.state);}
   check(transformed==int(wakes.size())&&matched==int(wakes.size())&&int(w.creatureSpawns().size())==transformed,(std::string(name)+": its patients wait on their authored wake states").c_str());
   bool setter=false;for(const auto& t:w.terminals())setter|=t.activateState==dark;
   if(level==75){for(const auto& e:w.scriptEvents())setter|=setsState(e,dark,1);
    const int sec1=doorAt(g,4.4f,5.3f,10.5f),sec2=doorAt(g,4.4f,5.3f,12.5f);
    check(!setter&&sec1>=0&&sec2>=0&&w.doors()[size_t(sec1)].requireState==dark&&w.doors()[size_t(sec2)].requireState==dark&&locked(g,sec1)&&locked(g,sec2),"Ward C north: nothing here starts the blackout; the seclusion doors hold until it");}
   else{int setters=0,starters=0;bool gated=true,inObs6=false,doors=false;
    const int obs6=doorAt(g,6.1f,7.3f,19.1f),obs5=doorAt(g,14.5f,15.4f,19.1f),recovery=doorAt(g,20.f,21.2f,8.9f),mortuary=doorAt(g,11.4f,12.6f,19.1f);
    for(const auto& e:w.scriptEvents()){
     if(setsState(e,dark,1)){++setters;gated&=e.requireState==stateId("medw_fail_done")&&e.requireValue==1;doors=opensDoor(e,obs5)&&opensDoor(e,recovery)&&opensDoor(e,mortuary);}
     if(setsState(e,stateId("medw_fail"),1)){++starters;inObs6=e.id==stateId("medw_blackout")&&e.x1>=6.09f&&e.x2<=9.71f&&e.y1>=20.49f&&e.y2<=22.91f&&e.x1<8.7f&&e.x2>8.7f&&e.y1<20.9f&&e.y2>20.9f&&
      e.requireState==stateId("medw_dark_done")&&e.requireValue==0&&!e.actions.empty()&&e.actions[0].type==ScriptAction::Type::Checkpoint;}}
    check(!setter&&setters==1&&gated&&starters==1&&inObs6&&doors,"Ward C south: the one blackout starts at OBS 6's bed with a checkpoint, and its fall opens OBS 5, recovery and the mortuary");
    check(obs6>=0&&w.doors()[size_t(obs6)].open>.5f&&locked(g,obs5)&&locked(g,recovery)&&locked(g,mortuary)&&w.doors()[size_t(recovery)].requireState==dark&&w.doors()[size_t(mortuary)].requireState==dark&&
     !opened(g,obs5)&&!opened(g,recovery)&&!opened(g,mortuary),"Ward C south: OBS 6 stands open; OBS 5, recovery and the mortuary are maglocked shut while the lights are up");
    // Before the blackout no standing player gets into the theatre transfer bay or the mortuary corridor.
    openDoors(g,false);auto reach=flood(g,{{{1.7f,17.f},-12.f},{{12.f,.6f},-12.f}});bool bay=false,morgue=false;
    for(const auto& s:reach){bay|=s.p.x>17.3f&&s.p.y>9.1f&&s.p.y<13.9f;morgue|=s.p.y>19.3f&&s.p.x>10.1f&&s.p.x<13.9f;}
    openDoors(g,true);auto all=flood(g,{{{1.7f,17.f},-12.f},{{12.f,.6f},-12.f}});bool bayOpen=false,morgueOpen=false;
    for(const auto& s:all){bayOpen|=s.p.x>17.3f&&s.p.y>9.1f&&s.p.y<13.9f;morgueOpen|=s.p.y>19.3f&&s.p.x>10.1f&&s.p.x<13.9f;}
    out<<"  76 reach lit="<<reach.size()<<" bay="<<bay<<" mortuary="<<morgue<<"; unlocked="<<all.size()<<" bay="<<bayOpen<<" mortuary="<<morgueOpen<<'\n';
    check(reach.size()>2000&&!bay&&!morgue&&bayOpen&&morgueOpen,"Ward C south: with the lights up the transfer bay and the mortuary corridor are out of reach; unlocked, both are reached");}}
  {Game g;fresh(g,75);put(g,{12.f,14.f},-12,kPi*.5f);
   const int sec1=enemyNear(g,{3.45f,9.65f}),sec2=enemyNear(g,{2.7f,14.85f}),bay=enemyNear(g,{15.6f,15.47f});
   run(g,2);const bool lit=g.darkness()<.01f&&!g.state("medw_wake_n")&&dormant(g,sec1)&&dormant(g,sec2)&&dormant(g,bay)&&on(g,{12.f,13.5f})&&off(g,{15.6f,6.02f});
   g.setState(dark);run(g,.5f);
   out<<"  75 dark: darkness="<<g.darkness()<<" wake_n="<<g.state("medw_wake_n")<<" lit_before="<<lit<<'\n';
   check(lit&&g.darkness()>.99f&&g.state("medw_wake_n")==1&&awake(g,sec1)&&awake(g,sec2)&&awake(g,bay)&&off(g,{12.f,13.5f})&&on(g,{15.6f,6.02f})&&on(g,{16.98f,11.5f})&&on(g,{7.02f,19.7f})&&
    !locked(g,doorAt(g,4.4f,5.3f,10.5f)),"Ward C north: lit, its patients sleep; dark, the tubes die, the reds burn, the ward is black and all three wake");}
  {Game g;fresh(g,76);const int obs5=doorAt(g,14.5f,15.4f,19.1f),recovery=doorAt(g,20.f,21.2f,8.9f),mortuary=doorAt(g,11.4f,12.6f,19.1f);
   const int bed4=enemyNear(g,{8.4f,9.84f}),bed12=enemyNear(g,{13.7f,16.95f}),her=enemyNear(g,{16.1f,20.f}),bay=enemyNear(g,{21.4f,11.6f});
   const Vec2 bed{8.7f,20.9f};const bool fits=g.hullFits(bed,-12,Player::StandingHeight);put(g,bed,-12,kPi);
   float black=-1,wakeB=-1;bool quiet=false,dead=false;
   for(int frame=1;frame<=60*12;++frame){g.update({},1.f/60);const float t=frame/60.f;
    if(black<0&&g.state(dark))black=t;if(wakeB<0&&g.state("medw_wake_b"))wakeB=t;
    if(frame==150)quiet=!g.state(dark)&&on(g,{12.f,16.6f})&&g.darkness()<.01f&&dormant(g,bed4)&&dormant(g,bed12)&&dormant(g,her)&&dormant(g,bay)&&g.state("medw_dark_done")==1;
    if(frame==240)dead=g.darkness()>.99f&&off(g,{12.f,16.6f})&&on(g,{9.6f,18.98f})&&on(g,{20.6f,11.5f})&&awake(g,her)&&dormant(g,bed4)&&dormant(g,bay)&&opened(g,obs5)&&opened(g,recovery)&&opened(g,mortuary);}
   out<<"  76 blackout: fits="<<fits<<" lights_out_s="<<black<<" bed12_wakes_s="<<wakeB<<" quiet_at_2.5s="<<quiet<<" dark_at_4s="<<dead<<'\n';
   check(fits&&quiet&&black>=2.5f&&black<=2.8f,"Ward C south: at OBS 6's bed the checkpoint is taken and the ward stays lit 2.6 s, its patients asleep");
   check(dead,"Ward C south: then the tubes die and the ward is black, the reds and the theatre beacon burn, OBS 5 wakes, and the maglocks let go");
   check(black>0&&wakeB>=black+6.7f&&wakeB<=black+7.2f&&awake(g,bed12),"Ward C south: bed 12 turns 7 s into the dark");
   g.setState(dark,0);run(g,.6f);const bool back=on(g,{12.f,16.6f})&&g.darkness()<.05f&&opened(g,obs5)&&opened(g,recovery)&&opened(g,mortuary);
   put(g,bed,-12,kPi);bool once=true;for(int i=0;i<240;++i){g.update({},1.f/60);once&=!g.state(dark);}
   check(back&&once&&opened(g,recovery)&&opened(g,mortuary),"Ward C south: power restored, the lamps return, the released doors stay open, and the blackout never comes again");}
  // --- 77 Surgery: the beacon. Its transfer can end the blackout and never start one.
  {Game g;fresh(g,77);const auto& w=g.world();
   const int transfer=terminalFor(g,stateId("medd_transfer_thrown")),release=terminalFor(g,stateId("med_morgue_release"));
   bool starts=terminalFor(g,dark)>=0,ends=false;for(const auto& e:w.scriptEvents()){starts|=std::any_of(e.actions.begin(),e.actions.end(),[&](const ScriptAction& a){return a.type==ScriptAction::Type::SetState&&a.id==dark&&a.value!=0;});
    ends|=e.requireState==stateId("medd_transfer_thrown")&&e.requireValue==1&&setsState(e,dark,0);}
   check(transfer>=0&&!w.terminals()[size_t(transfer)].paper()&&w.terminals()[size_t(transfer)].requireState==dark&&!w.terminals()[size_t(transfer)].toggleState&&ends&&!starts&&w.darkZones().empty(),
    "Surgery: the ward transfer is a control that needs the ward dead and can only end the blackout; no dark zone");
   check(release>=0&&w.terminals()[size_t(release)].requireState==stateId("medd_ward_transfer")&&!w.doors().empty()&&std::fabs(w.doors()[0].left-21.4f)<.02f&&std::fabs(w.doors()[0].y-7.3f)<.02f&&
    w.doors()[0].requireState==dark&&w.doors()[0].requireValue==0,"Surgery: the mortuary release needs the ward transfer, and the strongroom lock is on the ward board");
   const bool litBefore=on(g,{3.f,11.5f},int(WorldLightMount::Ceiling))&&off(g,{3.f,11.5f},int(WorldLightMount::Bare))&&on(g,{3.4f,2.7f});
   // Lit: the release console and the transfer refuse.
   const bool consoleLit=approach(g,release,{0,1},true);run(g,.3f);const bool refusedRelease=!g.state("med_morgue_release");
   const bool deskLit=approach(g,transfer,{0,1},true);run(g,.3f);const bool refusedTransfer=!g.state("medd_transfer_thrown")&&!g.state("medd_ward_transfer");
   g.setState(dark);run(g,1);
   bool panels=true;for(auto p:{Vec2{10.f,9.f},Vec2{15.3f,9.f},Vec2{10.f,14.f},Vec2{15.3f,14.f}})panels&=on(g,p);
   const bool beacon=panels&&off(g,{3.f,11.5f},int(WorldLightMount::Ceiling))&&on(g,{3.f,11.5f},int(WorldLightMount::Bare))&&off(g,{3.4f,2.7f})&&on(g,{19.45f,12.2f})&&g.darkness()<.01f;
   const bool armed=g.state("medd_ward_transfer")==1&&locked(g,0)&&g.world().doors()[0].open<.05f;
   press(g);run(g,.5f);const bool restored=!g.state(dark)&&g.state("medd_power_back")==1&&!locked(g,0);
   press(g);run(g,.3f);const bool stays=!g.state(dark);
   const bool consoleNow=approach(g,release,{0,1},true);run(g,1);
   out<<"  77 flow: lit_lamps="<<litBefore<<" console_lit="<<consoleLit<<" refused="<<refusedRelease<<" desk_lit="<<deskLit<<" refused="<<refusedTransfer<<" beacon="<<beacon<<" armed="<<armed
      <<" restored="<<restored<<" stays="<<stays<<" console="<<consoleNow<<" release="<<g.state("med_morgue_release")<<" cue="<<g.state("medd_release_t")<<'\n';
   check(litBefore&&beacon,"Surgery: in the blackout the theatre, office and generator lamps stay lit while the ward-board rooms go out");
   check(consoleLit&&refusedRelease&&deskLit&&refusedTransfer,"Surgery: with the ward lit, neither the transfer nor the mortuary release does anything");
   check(armed&&restored&&stays,"Surgery: in the dark the strongroom shuts; the transfer relights the ward, opens it, and cannot darken it again");
   check(consoleNow&&g.state("med_morgue_release")==1&&g.state("medd_release_t")>0,"Surgery: after the transfer the strongroom console releases the mortuary door");}
  // --- 78 Morgue: dark like the ward, its store darker, its way on behind the release.
  {Game g;fresh(g,78);const auto& w=g.world();const StateId store=stateId("medd_morgue_dark");
   check(!w.doors().empty()&&w.doors()[0].transfer&&std::fabs(w.doors()[0].left-10)<.02f&&std::fabs(w.doors()[0].right-14)<.02f&&w.doors()[0].requireState==stateId("med_morgue_release")&&locked(g,0)&&
    off(g,{15.1f,22.8f})&&on(g,{14.55f,22.8f}),"Morgue: the quarantine door is locked on the mortuary release, its red lamp lit");
   bool whole=false,inner=false;for(const auto& z:w.darkZones()){whole|=z.state==dark&&z.jamFlashlight&&z.x1<=0&&z.y1<=0&&z.x2>=24&&z.y2>=24;inner|=z.state==store&&z.jamFlashlight&&std::fabs(z.x1-1)<.01f&&std::fabs(z.y1-1)<.01f&&std::fabs(z.x2-9)<.01f&&std::fabs(z.y2-14.8f)<.01f;}
   int mutants=0;for(const auto& s:w.creatureSpawns())mutants+=s.kind==CreatureKind::Mutant&&length(s.position-Vec2{5.55f,9.2f})<.05f&&s.wakeState==store;
   bool pop=false,relight=false;for(const auto& e:w.scriptEvents()){pop|=setsState(e,stateId("medd_store_pop"),1)&&e.requireState==store&&e.requireValue==0;relight|=e.requireState==stateId("medd_power_back")&&e.requireValue==1&&setsState(e,store,0);}
   check(whole&&inner&&mutants==1&&w.creatureSpawns().size()==1&&pop&&relight,"Morgue: a dark zone on the blackout and one on the store; subject 40 waits on the store's dark; the pop needs the store lit, power back relights it");
   check(std::fabs(w.floorHeight(12,1.5f)+15)<.01f&&std::fabs(w.supportBelow(12,1.5f,-11.9f)+12)<.01f,"Morgue: the entry stands on the ward-level landing over the -15 floor");
   g.setState(stateId("med_morgue_release"));run(g,.1f);
   check(!locked(g,0)&&on(g,{15.1f,22.8f})&&off(g,{14.55f,22.8f}),"Morgue: the release unlocks the quarantine door and turns its lamp green");}
  {Game g;fresh(g,78);const int forty=enemyNear(g,{5.55f,9.2f});put(g,{12.f,1.5f},-12,kPi*.5f);g.setState(dark);run(g,.5f);
   int lit=0,warm=0;for(const auto& l:g.world().lights())if(g.lampSwitchedOn(l)){++lit;warm+=l.kelvin<=1200;}
   out<<"  78 blackout: store_dark="<<g.state("medd_morgue_dark")<<" lit="<<lit<<" red="<<warm<<" darkness="<<g.darkness()<<'\n';
   check(g.state("medd_morgue_dark")==1&&lit<=3&&warm==lit&&g.darkness()>.99f&&awake(g,forty),"Morgue: entered in the blackout it is black but for three reds, and subject 40 is up");}
  {Game g;fresh(g,78);const int forty=enemyNear(g,{5.55f,9.2f});put(g,{12.f,1.5f},-12,kPi*.5f);run(g,1);
   const bool calm=!g.state("medd_morgue_dark")&&dormant(g,forty)&&on(g,{6.f,4.f})&&on(g,{4.f,12.6f});
   const Vec2 aisle{4.f,12.5f};const float z=g.groundHeight(aisle,-14.8f);const bool fits=g.hullFits(aisle,z,Player::StandingHeight);put(g,aisle,z,-kPi*.5f);run(g,1.5f);
   const bool popped=g.state("medd_morgue_dark")==1&&off(g,{6.f,4.f})&&off(g,{4.f,8.4f})&&off(g,{4.f,12.6f})&&on(g,{3.f,5.8f})&&on(g,{5.f,16.9f})&&awake(g,forty)&&!g.state("medd_knock");
   float left=0;for(int i=0;i<180;++i){g.update({},1.f/60);if(forty>=0)left=std::max(left,length(g.enemies()[size_t(forty)].pos-Vec2{5.55f,9.2f}));}
   g.setState(stateId("medd_power_back"));run(g,.2f);
   out<<"  78 store: calm="<<calm<<" fits="<<fits<<" at z="<<z<<" popped="<<popped<<" subject_40_moved="<<left<<" relit="<<!g.state("medd_morgue_dark")<<'\n';
   check(calm&&fits&&popped,"Morgue: lit, the store waits; walking its west aisle blows its tubes, lights drawer 9's red and wakes subject 40");
   check(left>1.f,"Morgue: once woken, subject 40 leaves his bay");
   check(!g.state("medd_morgue_dark")&&on(g,{6.f,4.f})&&awake(g,forty),"Morgue: power back relights the store, and he stays up");}
 }
 // Surface sets: every Surface value and structure finish 26-35 resolves to
 // loaded textures, SURFACE records parse, and invalid sets are rejected.
 {std::string report;check(SoftwareRenderer::testSurfaceSets(&report),"Every surface set and structure finish 26-39 resolves to loaded textures");out<<report;
  std::string records="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Surface test|0\nMAP|0|Room|0|0|6.5|6.5|0|0|8|0.3||0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y)records+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";
  auto parse=[&](const char* surface){try{return loadCustomCampaignText(records+surface+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","surface-test.txt");}catch(const std::exception&){return std::shared_ptr<const CustomCampaign>{};}};
  auto rock=parse("SURFACE|0|5|8|5\n"),plain=parse("");
  check(rock&&rock->maps[0]->wallSurface==SurfaceRock&&rock->maps[0]->floorSurface==SurfaceMineTimber&&World(0,rock->maps[0]).ceilingSurface()==SurfaceRock,"SURFACE records set a custom map's wall, floor and ceiling finish");
  check(plain&&World(0,plain->maps[0]).wallSurface()==SurfaceDefault&&!parse(("SURFACE|0|"+std::to_string(int(SurfaceCount))+"|0|0\n").c_str())&&!parse("SURFACE|0|-2|0|0\n")&&!parse("SURFACE|0|1|2\n"),"Absent SURFACE keeps the legacy finish; out-of-range or malformed sets are rejected");
  bool allSets=true;for(int surface=SurfaceDefault;surface<SurfaceCount;++surface){auto record="SURFACE|0|"+std::to_string(surface)+"|"+std::to_string(surface)+"|"+std::to_string(surface)+"\n";auto pack=parse(record.c_str());
   allSets&=pack&&World(0,pack->maps[0]).wallSurface()==surface&&World(0,pack->maps[0]).floorSurface()==surface&&World(0,pack->maps[0]).ceilingSurface()==surface;}
  check(allSets,"Every Surface enum value round-trips through the custom campaign parser");
  bool rejected=false;if(plain){auto bad=std::make_shared<AuthoredMapData>(*plain->maps[0]);bad->ceilingSurface=SurfaceCount;try{World invalid(0,bad);}catch(const std::exception&){rejected=true;}}
  check(rejected,"World::loadAuthoredMap rejects an out-of-range surface");
 }
 // Area title cards: the first visit to a titled area raises its card with the
 // sting; the card fades in, holds and clears; staying or returning stays quiet.
 {Game area;area.loadLevel(53,false);area.m_enemies.clear();area.update({},1.f/60);
  auto sting=[&]{return std::any_of(area.m_sounds.begin(),area.m_sounds.end(),[](const SoundEvent& e){return e.sound==Sound::AreaTitle;});};
  check(area.areaTitle()=="MAINTENANCE DISTRICT"&&sting()&&area.world().zoneName(area.player().pos,area.player().z)!=nullptr,"Entering a major area raises its title card and sting; the HUD label is unchanged");
  float early=area.areaTitleAlpha();for(int i=0;i<150;++i)area.update({},1.f/60);float held=area.areaTitleAlpha();
  bool again=false;for(int i=0;i<330;++i){area.update({},1.f/60);again|=sting();}
  check(early<.05f&&held>.95f&&area.areaTitle().empty()&&!again,"The title card fades in, holds, clears and does not repeat");
  std::string records="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Title test|0\nMAP|0|Room|0|0|6.5|6.5|0|0|8|0.3||0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y)records+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";
  auto parse=[&](const char* zone){try{return loadCustomCampaignText(records+zone+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","title-test.txt");}catch(const std::exception&){return std::shared_ptr<const CustomCampaign>{};}};
  auto titled=parse("ZONE|0|0|0|24|24||||OLD WORKINGS\n");
  check(titled&&titled->maps[0]->zones.size()==1&&titled->maps[0]->zones[0].title=="OLD WORKINGS"&&titled->maps[0]->zones[0].name.empty()&&!parse("ZONE|0|0|0|24|24|||\n"),"ZONE records take an optional area title; a zone with neither name nor title is rejected");
 }
 {bool spawns=true;for(int level=34;level<CampaignChunkCount;++level){World w(level);const auto& d=w.definition();bool fit=w.fits(d.playerStart.x,d.playerStart.y,d.spawnHeight,1.f)&&std::fabs(w.supportBelow(d.playerStart.x,d.playerStart.y,d.spawnHeight+.3f)-d.spawnHeight)<.05f;if(!fit)out<<"  spawn blocked in chunk "<<level<<"\n";spawns&=fit;}check(spawns,"Every descent chunk loads and its spawn fits a standing hull");}
 {Game gallery;gallery.loadLevel(42,false);gallery.m_player.pos={21.f,10.f};gallery.m_player.z=-24.988f;
  for(int i=0;i<24;++i)gallery.tryMove({0,-.1f});
  bool crossed=gallery.m_player.pos.y<7.7f&&gallery.m_player.z>-25.1f;
  for(int i=0;i<24;++i)gallery.tryMove({0,.1f});
  check(crossed&&gallery.m_player.pos.y>9.9f&&gallery.m_player.z>-25.1f,"Unit 2 shutter gallery can be walked both ways without jumping or dropping off the table");
 }
 {bool pickups=true,creatures=true;
  for(int level:{58,64}){World w(level);
   for(const auto& s:w.pickupSpawns())pickups&=w.fits(s.position.x,s.position.y,s.z+.03f,.3f,false);
   for(const auto& s:w.creatureSpawns())creatures&=w.fits(s.position.x,s.position.y,s.z+.03f,1.2f,false);
  }
  check(pickups&&creatures,"Perimeter booth and Research Receiving spawns clear their raised floor slabs");
 }
 {auto operate=[](Game& g,int level,Vec2 pos,float feet,float yaw=kPi*.5f){
   g.loadLevel(level,false);g.m_player.pos=pos;g.m_player.z=feet;g.m_player.angle=yaw;g.m_player.pitch=0;
   InputState use{};use.use=true;g.updateInteraction(use,.01f);
  };
  auto gateAvailable=[](Game& g,int level,StateId required){
   g.loadLevel(level,false);
   for(const auto& door:g.world().doors())if(door.requireState==required)return !g.doorLocked(door);
   return false;
  };
  Game power;
  auto powerValues=[&](const char* stage,const Game& g){
   out<<"Black-start observed "<<stage;
   for(const char* name:{"pwr_fuel_transfer","pwr_set2_cranked","pwr_set2_ms","pwr_set2_ms_done","pwr_gen_online","pwr_sub_a","pwr_transfer_ready","pwr_transfer_bus","pwr_feeder9"})out<<' '<<name<<'='<<g.state(name);
   out<<'\n';
  };
  operate(power,36,{12,2.9f},-28);
  operate(power,40,{19,19.9f},-19);
  check(!power.state("pwr_sub_a")&&!power.state("pwr_feeder9"),"Power controls refuse out-of-order breaker and feeder operation");
  operate(power,39,{17.9f,6.f},-28,0.f);operate(power,39,{13.f,12.7f},-28);
  // Set 2 has to catch and run before the emergency bus will close onto it.
  operate(power,39,{13,5.9f},-28);const bool earlyClose=power.state("pwr_gen_online")!=0;
  power.loadLevel(39,false);power.m_enemies.clear();power.m_player.pos={13.f,12.7f};power.m_player.z=-28;for(int i=0;i<120;++i)power.update({},1.f/60);
  check(!earlyClose&&power.state("pwr_set2_ms_done"),"Set 2 catches after cranking, and the bus waits for it");
  operate(power,39,{13,5.9f},-28);operate(power,36,{12,2.9f},-28);
  check(power.state("pwr_gen_online")&&power.state("pwr_sub_a"),"Physical E controls restore emergency generation and Substation A");
  if(!power.state("pwr_gen_online")||!power.state("pwr_sub_a"))powerValues("after physical controls",power);
  Game restored;check(restored.decodeSave(power.encodeSave())&&restored.state("pwr_sub_a"),"Black-start progress survives save/load before load transfer");
  if(!restored.state("pwr_sub_a"))powerValues("after save/load",restored);
  operate(restored,40,{12,19.9f},-19);
  check(!restored.state("pwr_transfer_bus"),"Transfer requires loads to be shed first");
  operate(restored,40,{5,19.9f},-19);operate(restored,40,{12,19.9f},-19);operate(restored,40,{19,19.9f},-19);
  check(restored.state("pwr_feeder9"),"Physical three-step transfer restores the Machine Hall gate prerequisite");
  if(!restored.state("pwr_feeder9"))powerValues("after transfer controls",restored);
  check(gateAvailable(restored,35,stateId("pwr_feeder9")),"Restored feeder actually unlocks the hub's Machine Hall gate");
  Game maintenance;operate(maintenance,55,{15.6f,20.9f},-19);operate(maintenance,55,{6,16.9f},-19);
  check(!maintenance.state("maint_shutter_pin")&&!maintenance.state("mnt_pin_machined"),"Fabrication requires signed-out stock and a machined pin");
  operate(maintenance,54,{4.8f,12.6f},-19,kPi);
  operate(maintenance,55,{6,16.9f},-19);operate(maintenance,55,{15.6f,20.9f},-19);
  check(maintenance.state("mnt_bar_stock")&&maintenance.state("maint_shutter_pin"),"Stock issue and physical fabrication controls restore the Operations shutter prerequisite");
  check(gateAvailable(maintenance,55,stateId("maint_shutter_pin")),"Fitted pin unlocks the actual Operations shutter");
  operate(maintenance,59,{12,10.9f},-19);operate(maintenance,61,{8,6.9f},-16);
  check(maintenance.state("perim_gate")&&maintenance.state("delta_override"),"Gatehouse and chief controls restore the two perimeter gate prerequisites");
  check(gateAvailable(maintenance,58,stateId("perim_gate"))&&gateAvailable(maintenance,60,stateId("delta_override")),"Both restored perimeter controls unlock their actual route gates");
 }
 {Game office;office.loadLevel(20,false);office.updateStreaming(0);
  bool vista=true;for(int level=16;level<=19;++level)vista&=office.chunkResident(level);
  check(vista,"Manifest overlook retains all four real warehouse chunks with the door closed");
  office.loadLevel(21,false);office.updateStreaming(0);
  vista=true;for(int level=16;level<=19;++level)vista&=office.chunkResident(level);
  check(vista,"Adjacent Manifest routing wing retains the shared warehouse vista");
  World upper(19),manifest(20);
  check(upper.lightRayClear({4.5f,23.02f},-5.3f,{4.5f,23.98f},-5.3f)&&manifest.lightRayClear({4.5f,.3f},-5.3f,{4.5f,.001f},-5.3f),"Warehouse facade aperture and office glazing transmit the overlook sightline");
  check(!upper.fits(4.5f,23.95f,-7,Player::StandingHeight)&&!manifest.fits(4.5f,.055f,-7,Player::StandingHeight),"Both sides of the overlook retain physical fall protection");
 }
 {Game signal;signal.loadLevel(31,false);signal.m_enemies.clear();
  signal.m_player.pos={3.25f,11.65f};signal.m_player.z=-25;signal.m_player.angle=-kPi*.5f;
  InputState use{};use.use=true;signal.updateInteraction(use,.01f);
  check(signal.logTime()>0&&signal.world().terminals().front().yaw==0,"Signal alcove computer faces its entrance and its log opens through E");
 }
 {Game hold;hold.loadLevel(14,false);hold.updateStreaming(0);unsigned roles=0;int count=0;
  for(size_t i=0;i<hold.npcActors().size();++i){const auto& npc=hold.npcActors()[i];if(npc.homeLevel!=14||npc.track().ai.mode!=ActorAiMode::Guard)continue;++count;roles|=1u<<(int(npc.track().ai.role)-1);
   check(hold.friendlyBodyFits(14,npc.pose.position,npc.pose.z,npc.track().scale),"Receiving Cleanup spawn has physical standing clearance");
   for(const auto& p:npc.track().ai.patrol)check(hold.friendlyBodyFits(14,p.position,p.z,npc.track().scale),"Receiving Cleanup patrol pause has standing clearance");
   if(npc.track().ai.role==GuardSquadRole::Engineer||npc.track().ai.role==GuardSquadRole::Medic)check(npc.track().ai.weapon==2,"Receiving support roles retain pistols");
  }
  check(count==6&&roles==63,"Existing Receiving 02 hosts exactly the six baked Cleanup roles");
  {Vec2 portal;float feet=-25;bool found=hold.friendlyPortal(14,15,{14.6f,18.2f},-25,1.6f,portal,feet,-25);
   check(found&&portal.x>10.7f&&portal.x<13.3f&&hold.friendlyBodyFits(14,{portal.x,23.65f},feet,1.6f),"Receiving retreat selects the pressure-door opening rather than a portal footprint embedded in its side wall");}
  hold.m_player.pos={7,22.1f};hold.m_player.z=-25;hold.m_player.angle=-kPi*.5f;InputState use;use.use=true;hold.update(use,1.f/60);hold.update({},1.f/60);
  check(hold.state("receiving_cache_read")&&hold.hasFlashlight(),"Main-route physical cache grants the same owned flashlight");
  Game saved;check(saved.decodeSave(hold.encodeSave())&&saved.hasFlashlight()&&saved.state("receiving_cache_read"),"Main-route encounter and cache survive save/load");
  hold.update(use,1.f/60);hold.update({},1.f/60); // E closes the cache log before the next control.
  hold.m_player.pos={7,20.4f};hold.m_player.angle=-kPi*.5f;hold.update(use,1.f/60);
  check(hold.state("receiving_inspection_lure")>0,"Main-route chain distraction is operated through E");
  hold.update({},1.f/60);hold.m_player.pos={1.4f,20};hold.m_player.z=-25;
  std::vector<size_t> detail;for(size_t i=0;i<hold.npcActors().size();++i)if(hold.npcActors()[i].homeLevel==14&&hold.npcActors()[i].track().ai.squad==stateId("receiving_cleanup"))detail.push_back(i);
  if(detail.size()==6){
   AiTargetRef playerHit{ShotKind::Player,14,0};hold.damageFriendly(detail[0],1000,{7,22},playerHit);hold.updateScripts(0);
   check(!hold.state("receiving_breach")&&hold.enemies().empty(),"One player pick does not start the squad wipeout");
   hold.damageFriendly(detail[2],1000,{7,22},playerHit);hold.updateScripts(0);
   check(hold.state("receiving_breach")&&hold.enemies().size()==6&&hold.state("receiving_survivor")>0,"Two player picks release real entities and select one living evacuee");
   Game resumed;bool loaded=resumed.decodeSave(hold.encodeSave());check(loaded,"Active breach and authored evacuation save cleanly");
   if(loaded){resumed.updateScripts(0);check(resumed.enemies().size()==6,"Reload retains the six breach creatures without duplicating them");hold=std::move(resumed);}
   for(int frame=0;frame<3600;++frame)hold.update({},1.f/60);
   int dead=0,living=0,escaped=0;for(size_t i=0;i<hold.npcActors().size();++i){const auto& npc=hold.npcActors()[i];if(npc.homeLevel!=14||npc.track().ai.squad!=stateId("receiving_cleanup"))continue;
    bool killed=hold.state(npc.track().deadState)!=0;dead+=killed;living+=!killed;escaped+=!killed&&npc.level==15;out<<"Breach member "<<npc.homeActor<<" level="<<npc.level<<" damage="<<hold.state(npc.track().damageState)<<" evac="<<hold.state(hold.friendlyKey(i,"evacuated"))<<'\n';}
   check(dead==5&&living==1&&escaped==1&&hold.state("receiving_retreat_sealed"),"Entities kill the three remaining fighters while one survivor physically retreats into Receiving 03");
   Game after;check(after.decodeSave(hold.encodeSave())&&after.state("receiving_retreat_sealed"),"The lone survivor and sealed retreat persist across save/load");
  }

 }
 int mutants=0;for(int level=10;level<32;++level){Game area;area.loadLevel(level,false);
  check(area.world().definition().musicCue!=MusicCue::Default,"Freight district maps play their area scores, not the facility track");
  for(const auto&e:area.enemies())if(e.kind==Enemy::Kind::Mutant){++mutants;check(e.maxHp==180&&std::fabs(e.bodyTop()-e.bodyBottom()-1.85f)<.001f,"Mutated human has authored health and standing hull");check(std::fabs(area.groundHeight(e.pos,e.z+.2f)-e.z)<.02f,"Mutated human stands on authored floor");}
 }
 check(mutants==2,"Exactly two mutated humans across the new freight section");
 {Game original;original.loadLevel(17,false);auto it=std::find_if(original.m_enemies.begin(),original.m_enemies.end(),[](const Enemy&e){return e.kind==Enemy::Kind::Mutant;});
  if(it!=original.m_enemies.end()){it->hp=73;Game restored;bool saved=restored.decodeSave(original.encodeSave());auto found=std::find_if(restored.enemies().begin(),restored.enemies().end(),[](const Enemy&e){return e.kind==Enemy::Kind::Mutant;});check(saved&&found!=restored.enemies().end()&&found->hp==73,"Mutated human damage survives save/load");}
 }
 Game dispatch;dispatch.loadLevel(9,false);dispatch.m_enemies.clear();dispatch.m_player.pos={21.5f,22.f};dispatch.m_player.z=-12;dispatch.m_player.angle=kPi*.5f;dispatch.m_player.grounded=true;
 InputState through{};through.use=true;dispatch.updateInteraction(through,.01f);through={};
 for(int frame=0;frame<160;++frame)dispatch.update(through,1.f/120);
 through.forward=true;for(int frame=0;frame<100;++frame)dispatch.update(through,1.f/120);
 check(dispatch.level()==10&&std::fabs(dispatch.player().z+12)<.03f,"Waste Handling dispatch door opens through E and walking reaches Freight Access");
 dispatch.m_player.angle=-kPi*.5f;for(int frame=0;frame<100;++frame)dispatch.update(through,1.f/120);
 check(dispatch.level()==9&&std::fabs(dispatch.player().z+12)<.03f,"Freight Access permits walking back into Waste Handling");
 for(int level=10;level<32;++level){Game game;game.loadLevel(level,false);game.updateStreaming(0);
  out<<"MAP "<<level<<" / "<<CampaignMapNames[level]<<'\n';
  check(game.world().campaign()&&!game.world().custom(),"Freight geometry keeps native campaign identity");
  check(game.hullFits(game.player().pos,game.player().z,Player::StandingHeight),"Spawn has standing clearance");
  for(const auto& enemy:game.enemies())check(game.hullFits(enemy.pos,enemy.bodyBottom(),enemy.bodyTop()-enemy.bodyBottom()),"Authored enemy has clearance");
  for(const auto& light:game.world().lights())check(light.z+.13f<game.world().clearanceAbove(light.position.x,light.position.y,light.z)+.01f,"Lamp fits under its support");
  for(int d=0;d<int(game.world().doors().size());++d)game.m_world.openDoor(d);
  game.m_world.updateDoors(4);
  if(level==12)game.m_world.setCargoLiftHeight(-12);
  struct Node{int x,y;float z;};std::queue<Node> queue;std::unordered_set<long long> visited;
  auto key=[](Node n){return (static_cast<long long>(std::lround((n.z+100)*1000))<<16)|(n.y*192+n.x);};
  Node start{int(game.player().pos.x*8),int(game.player().pos.y*8),game.player().z};queue.push(start);visited.insert(key(start));
  std::vector<bool> controlsReached(game.world().terminals().size(),false);
  float low=start.z,high=start.z;std::vector<float> nearest(controlsReached.size(),999.f);
  while(!queue.empty()){
   auto a=queue.front();queue.pop();Vec2 p{(a.x+.5f)*.125f,(a.y+.5f)*.125f};
   low=std::min(low,a.z);high=std::max(high,a.z);
   for(size_t t=0;t<controlsReached.size();++t){const auto& terminal=game.world().terminals()[t];
    // A taped sheet is read standing about 1.5 m under it, as the game reads it.
    const bool taped=terminal.paper()&&terminal.taped;
    float z=game.world().floorHeight(terminal.position.x,terminal.position.y)+terminal.z-(taped?1.5f:0.f);
    nearest[t]=std::min(nearest[t],length(p-terminal.position)+std::fabs(a.z-z));
    if(length(p-terminal.position)<1.6f&&std::fabs(a.z-z)<(taped?1.f:.3f))controlsReached[t]=true;
   }
   for(auto delta:{Vec2{-1,0},Vec2{1,0},Vec2{0,-1},Vec2{0,1}}){
    Node b{a.x+int(delta.x),a.y+int(delta.y),a.z};if(b.x<2||b.x>=190||b.y<2||b.y>=190)continue;
    Vec2 q{(b.x+.5f)*.125f,(b.y+.5f)*.125f};b.z=game.groundHeight(q,a.z+.215f);
    if(std::fabs(b.z-a.z)>.3f||!game.hullFits(q,b.z,Player::StandingHeight)||!visited.insert(key(b)).second)continue;queue.push(b);
   }
  }
  out<<"Route visited "<<visited.size()<<" nodes; elevation "<<low<<" to "<<high<<'\n';
  if(level==17||level==18||level==19||level==28)check(high>=((level==17||level==28)?-19.f:level==18?-13.f:-7.f)-.02f,"Stair route reaches its upper exit landing");
  if(level==22)check(low<=-24.98f,"Transfer stair route reaches the sorting floor");
  for(size_t t=0;t<controlsReached.size();++t){out<<game.world().terminals()[t].title<<" (nearest "<<nearest[t]<<"): ";check(controlsReached[t],"Control is approachable from the chunk entry with the standing player hull");}
 }
 // Drive the real procedure through E and movement. State injection used to
 // conceal a sealed lower landing and controls with no visible confirmation.
 Game lift;lift.loadLevel(11,false);lift.m_enemies.clear();
 auto moveTo=[&](Game& g,int level,Vec2 point){Vec2 target=g.chunkOffset(level)+point;InputState walking{};walking.forward=true;
  for(int frame=0;frame<2400;++frame){Vec2 delta=target-(g.player().pos+g.chunkOffset(g.level()));if(length(delta)<.18f){for(int i=0;i<30;++i)g.update({},1.f/120);return true;}g.m_player.angle=std::atan2(delta.y,delta.x);g.update(walking,1.f/120);}
  out<<"Movement stalled: map "<<g.level()<<" at "<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<" toward "<<point.x<<','<<point.y<<'\n';return false;
 };
 auto pressUse=[&](Game& g){InputState key{};key.use=true;g.updateInteraction({},.01f);if(g.logTime()>0){g.updateInteraction(key,.01f);g.updateInteraction({},.01f);}g.updateInteraction(key,.01f);g.updateInteraction({},.01f);};
 if(!moveTo(lift,11,{2,2})||!moveTo(lift,11,{2,16.1f})||!moveTo(lift,11,{3.5f,16.1f})){check(false,"Walk west personnel route to brake console");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_brake"),"E releases brake from actual west-side console");
 if(!moveTo(lift,11,{2,16.1f})||!moveTo(lift,11,{2,21.5f})||!moveTo(lift,11,{12,21.5f})||!moveTo(lift,12,{12,2})){check(false,"Walk machinery route into auxiliary lift bay");return false;}
 if(!moveTo(lift,12,{6.5f,4.7f})){check(false,"Reach local control");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_local"),"E selects local control after brake release");
 if(!moveTo(lift,12,{12,4.7f})){check(false,"Reach call control");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_call"),"E calls platform through real control interaction");
 for(int frame=0;frame<1000;++frame)lift.update({},1.f/120);
 check(lift.state("freight_platform_arrived")&&std::fabs(lift.world().cargoLiftHeight()+12)<.02f,"Called platform reaches upper landing during gameplay");
 if(!moveTo(lift,12,{18,4.7f})){check(false,"Reach release control");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_release"),"E releases cage interlock after arrival");
 if(!moveTo(lift,12,{20.5f,4.7f})||!moveTo(lift,12,{20.5f,7.8f})||!moveTo(lift,12,{12,7.8f})){check(false,"Reach cage entry");return false;}
 for(int frame=0;frame<160;++frame)lift.update({},1.f/120);
 check(lift.world().doors().front().open>.99f,"Manual release actually opens cage gate without a second hidden interaction");
 if(!moveTo(lift,12,{12,11.3f})){check(false,"Walk through released cage door onto platform");return false;}
 for(int frame=0;frame<300;++frame)lift.update({},1.f/120);
 float mid=lift.world().cargoLiftHeight();Game restored;check(restored.decodeSave(lift.encodeSave())&&std::fabs(restored.world().cargoLiftHeight()-mid)<.01f,"Save/load preserves real passenger descent");
 for(int frame=0;frame<1000;++frame)restored.update({},1.f/120);
 check(restored.state("freight_descended")&&std::fabs(restored.player().z+19)<.02f,"Cage carries standing player to lower landing in normal gameplay");
 check(moveTo(restored,12,{12,18}),"Standing player walks off cage through lower threshold without jumping");
 // Above a tall doorway, the header must be shared visual/collision geometry.
 for(int level:{10,11,12,15}){Game frame;frame.loadLevel(level,false);for(const auto& door:frame.world().doors())if(door.entry||door.transfer){float base=frame.world().floorHeight((door.left+door.right)*.5f,door.y)+door.z;
  check(!frame.hullFits({(door.left+door.right)*.5f,door.y},base+3.2f,Player::StandingHeight),"Upper doorway header cannot be used as an unintended drop or bypass");}}
 Game warehouse;warehouse.loadLevel(16,false);warehouse.m_enemies.clear();warehouse.updateStreaming(0);
 check(warehouse.chunkResident(17)&&warehouse.chunkResident(18)&&warehouse.chunkResident(19),"All four parts of the 48m warehouse are resident together");
 warehouse.m_player.pos={24.05f,12};warehouse.crossChunkBoundary();check(warehouse.level()==17&&std::fabs(warehouse.player().pos.x-.05f)<.01f,"East seam enters the adjacent warehouse chunk using world coordinates");
 struct Crossing{int level;Vec2 start;float z,angle;int destination;bool crouch=false;};
 for(const auto& c:std::array<Crossing,5>{{{16,{23,12},-25,0,17},{17,{12,23},-19,kPi*.5f,18},{18,{1,12},-13,kPi,19},{13,{23,12},-25,0,14},{14,{12,23},-25,kPi*.5f,15}}}){
  Game walk;walk.loadLevel(c.level,false);walk.m_enemies.clear();walk.m_player.pos=c.start;walk.m_player.z=c.z;walk.m_player.angle=c.angle;walk.m_player.grounded=true;
  InputState input{};input.forward=true;for(int frame=0;frame<60;++frame)walk.update(input,1.f/120);
  out<<"Seam "<<c.level<<" -> "<<c.destination<<": ";check(walk.level()==c.destination&&std::fabs(walk.player().z-c.z)<.03f,"Actual movement crosses the hall seam without a collision or floor-height jump");
 }
 return pass;
}
bool Game::testMechanisms(){
 std::ofstream out("mechanisms-test.txt");bool pass=true;
 auto check=[&](bool ok,const char* label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Shared mechanism test";campaign->key=12345;
 for(int i=0;i<3;++i){auto map=std::make_shared<AuthoredMapData>();map->name="Test hall";map->definition={{float(i*24),0},{3,3},0,Environment::Interior,false,8,.3f,i<2?42:43};
  map->openEast=i<2;map->openWest=i>0;AuthoredLayerData layer;for(auto& row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  if(i==0){map->cargoLift={8,8,12,12,0,3,1,stateId("demo_call"),stateId("demo_release"),stateId("demo_position"),stateId("demo_down"),stateId("demo_arrived"),stateId("demo_descended")};
   TimedSequence sequence;sequence.timerState=stateId("demo_timer");sequence.finishState=stateId("demo_finished");sequence.durationMs=1000;sequence.finishAtMs=500;sequence.soundIntervalMs=0;map->timedSequences.push_back(sequence);
   TimedSequence motion;motion.timerState=stateId("demo_motion");motion.finishState=stateId("demo_cycle");motion.durationMs=4000;motion.finishAtMs=4000;motion.soundIntervalMs=0;motion.loop=true;map->timedSequences.push_back(motion);
   ActorTrack platform;platform.timerState=motion.timerState;platform.visual=ActorVisual::Cargo;platform.scale=1;platform.keys={{0,{16,16},1,0},{2000,{20,16},1,0},{4000,{16,16},1,0}};platform.loop=true;platform.platform=true;platform.footprint={2,2};map->actorTracks.push_back(platform);
   for(float x:{18.f,12.f}){ActorTrack target;target.timerState=motion.timerState;target.visual=ActorVisual::Wasp;target.scale=2;target.health=34;target.damageState=stateId(x>15?"far_damage":"near_damage");target.deadState=stateId(x>15?"far_dead":"near_dead");target.keys={{0,{x,6},0,0},{4000,{x,6},0,0}};map->actorTracks.push_back(target);}
  }campaign->maps.push_back(map);
 }
 Game game(campaign);check(game.chunkResident(1)&&!game.chunkResident(2),"Custom campaign uses authored residency groups without campaign map IDs");
 game.setState(stateId("demo_call"),1);for(int i=0;i<240;++i)game.updateMechanisms(1.f/60);
 check(game.state("demo_arrived")&&std::fabs(game.world().cargoLiftHeight()-3)<.01f,"Custom lift reaches upper landing using its own state names");
 check(game.state("demo_finished")&&game.state("demo_timer")==1000,"Custom timeline triggers and completes using its own state names");
 game.m_player.pos={10,10};game.m_player.z=3;game.setState(stateId("demo_release"),1);for(int i=0;i<60;++i)game.updateMechanisms(1.f/60);
 float mid=game.world().cargoLiftHeight();Game restored(campaign);check(restored.decodeSave(game.encodeSave())&&std::fabs(restored.world().cargoLiftHeight()-mid)<.01f,"Custom moving lift preserves position through save/load");
 for(int i=0;i<180;++i)restored.updateMechanisms(1.f/60);
 check(restored.state("demo_descended")&&std::fabs(restored.player().z)<.01f,"Shared lift carries custom-campaign passenger to the lower landing");
 check(!game.state("freight_call")&&!game.state("freight_worker_dead"),"Engine mechanism updates create no campaign-specific state");
 Game rider(campaign);rider.setState(stateId("demo_motion"),1);rider.updateMechanisms(0);rider.m_player.pos={16,16};rider.m_player.z=1;rider.m_player.grounded=true;
 for(int i=0;i<60;++i)rider.updateMechanisms(1.f/60);
 check(rider.player().pos.x>17.8f&&rider.player().pos.x<18.2f&&std::fabs(rider.player().z-1)<.01f,"Horizontal authored platform carries its passenger using shared collision");
 check(std::fabs(rider.world().supportBelow(rider.player().pos.x,16,1)-1)<.01f&&!rider.world().fits(rider.player().pos.x,16,.85f,.1f),"Platform visible surface and solid underside agree with collision");
 Game riderSave(campaign);check(riderSave.decodeSave(rider.encodeSave())&&std::fabs(riderSave.world().actorPose(0).position.x-rider.world().actorPose(0).position.x)<.001f,"Save/load restores platform and actor pose without restarting timeline");
 for(int i=0;i<240;++i)rider.updateMechanisms(1.f/60);
 check(rider.state("demo_motion")<=4000&&rider.state("demo_cycle"),"Looping machinery wraps safely and persists cycle completion");
 ActorTrack poseTrack;poseTrack.keys={{0,{2,3},0,3.1f,0,0},{1000,{4,5},2,-3.1f,0,1},{2000,{4,5},2,-3.1f,4,1}};
 auto halfway=sampleActor(poseTrack,500),end=sampleActor(poseTrack,9000);
 check(std::fabs(halfway.position.x-3)<.001f&&std::fabs(halfway.z-1)<.001f&&std::fabs(halfway.phase-.5f)<.001f&&halfway.yaw>3.1f,"Actor sampler interpolates position, skeletal phase and shortest yaw path");
 check(end.clip==4&&end.phase==1&&end.position.x==4,"Non-looping death pose remains present after sequence completion");
 Game targets(campaign);targets.m_enemies.clear();targets.m_player.pos={8,6};targets.m_player.z=0;targets.m_player.angle=0;targets.m_player.pitch=0;targets.shoot();
 check(targets.state("near_dead")&&!targets.state("far_damage"),"Actor hits select the nearest visible target rather than declaration order");
 Game targetSave(campaign);check(targetSave.decodeSave(targets.encodeSave())&&targetSave.world().actorPose(2).clip==4&&std::fabs(targetSave.world().actorPose(2).position.x-12)<.001f,"Killed authored actor preserves its death position and pose through save/load");
 auto invalid=std::make_shared<AuthoredMapData>(*campaign->maps[0]);invalid->actorTracks[0].keys[1].timeMs=0;bool rejected=false;try{World bad(0,invalid);}catch(const std::exception&){rejected=true;}check(rejected,"Malformed actor key ordering is rejected during campaign validation");
 Game receiving;receiving.loadLevel(13,false);receiving.m_enemies.clear();receiving.m_player.pos={12,2};receiving.updateMechanisms(1.f/60);check(!receiving.state("receiving_worker_encounter_ms"),"Worker encounter waits for the actual overlook rather than starting behind the entrance");receiving.m_player.pos={7.6f,9.5f};receiving.m_player.z=-19;
 receiving.m_player.angle+=kPi;auto wanderStart=receiving.world().actorPose(0).position;for(int i=0;i<100;++i)receiving.updateMechanisms(.01f);
 check(!receiving.state("receiving_worker_encounter_ms")&&length(wanderStart-receiving.world().actorPose(0).position)>.2f,"Unseen worker wanders while facing away does not trigger the encounter");
 receiving.m_player.angle=.69f;receiving.m_player.pitch=-62;receiving.updateMechanisms(.01f);
 check(receiving.state("receiving_worker_encounter_ms")>0,"Actually looking at the visible worker starts the saved encounter delay");
 for(int i=0;i<250;++i)receiving.updateMechanisms(.01f);
 check(receiving.world().actorPose(0).clip!=2&&receiving.world().actorPose(0).clip!=4&&receiving.world().actorPose(1).position.x>22,"Three-second observation delay keeps worker wandering and bugs away");
 auto workerVoiceCount=[&](){return std::count_if(receiving.m_sounds.begin(),receiving.m_sounds.end(),[](const auto& s){return s.sound==Sound::WorkerDying;});};
 check(workerVoiceCount()==0,"Worker voice stays silent during the observation delay");
 for(int i=0;i<150;++i)receiving.updateMechanisms(.01f);
 auto approach=receiving.world().actorPose(0);Game approachSave;check(approachSave.decodeSave(receiving.encodeSave())&&length(approachSave.world().actorPose(0).position-approach.position)<.002f,"Wander-to-fight approach preserves its captured origin through save/load");
 for(int i=0;i<280;++i)receiving.updateMechanisms(.01f);
 auto fighter=receiving.world().actorPose(0),bug=receiving.world().actorPose(1);Vec2 forward{std::sin(fighter.yaw),std::cos(fighter.yaw)},toward=bug.position-fighter.position;
 check(fighter.clip==2&&fighter.phase>.4f&&fighter.phase<.8f&&dot(forward,toward)/std::max(.001f,length(toward))>.999f,"Worker faces the actual attacking bug during the first fast strike");
 check(workerVoiceCount()==1,"Worker voice starts once at the first attack cue");
 for(int i=0;i<290;++i)receiving.updateMechanisms(.01f);
 fighter=receiving.world().actorPose(0);bug=receiving.world().actorPose(2);forward={std::sin(fighter.yaw),std::cos(fighter.yaw)};toward=bug.position-fighter.position;
 check(fighter.clip==2&&dot(forward,toward)/std::max(.001f,length(toward))>.98f,"Worker turns toward the second wasp for the next strike");
 check(workerVoiceCount()==1,"Worker voice does not restart during the second attack");
 Game receivingSave;check(receivingSave.decodeSave(receiving.encodeSave())&&receivingSave.state("receiving_worker_encounter_ms")==receiving.state("receiving_worker_encounter_ms"),"First worker encounter resumes from a saved skeletal timeline");
 receivingSave.loadLevel(14,false);for(int i=0;i<1200;++i)receivingSave.updateMechanisms(1.f/60);
 check(receivingSave.state("receiving_worker_encounter_complete"),"Receiving worker timeline continues across resident floor-level chunks");
 return pass;
}
}
