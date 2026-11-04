#include "EngineRegressionScene.h"
#include "CreatureAnimation.h"
#include "../world/CampaignKit.h"
#include "ai/Navigation.h"
#include "../renderer/BoxSurfaces.h"
#include "../renderer/Mesh.h"
#include <fstream>
#include <sstream>
#include <random>
#include <cstdlib>
#include <cerrno>

namespace retro {
bool Game::testGameplayFixes(){
 std::ofstream report("gameplay-fixes-test.txt");bool passed=true;
 auto check=[&](bool ok,const char*name,const std::string&value=""){passed&=ok;report<<(ok?"PASS ":"FAIL ")<<name<<' '<<value<<'\n';report.flush();};
 // Differential test: compare the spatial index against a full scan of the
 // actual structures, including thin, non-solid, and cell-boundary fixtures.
 {unsigned seed=0x4257a21u;if(const char*text=std::getenv("RAWMETAL_ISSUE_SEED")){char*end=nullptr;errno=0;auto value=std::strtoul(text,&end,10);
   if(*text<'0'||*text>'9'||*end||errno==ERANGE)check(false,"Random collision seed must be an unsigned decimal integer",text);else seed=unsigned(value);}
  std::mt19937 rng(seed);auto random=[&](float a,float b){return a+(b-a)*float(rng()%100000)/99999.f;};
  auto map=regression::room();for(int i=0;i<96;++i){float x=random(.1f,22.8f),y=random(.1f,22.8f),z=random(-1.f,3.f);
   float width=i%3==0?.018f:random(.08f,1.4f),depth=i%4==0?.023f:random(.08f,1.4f);
   map->structures.push_back({x,y,x+width,y+depth,z,z+random(.04f,2.5f),false,0,i%5!=0});}
  World w(0,map);int errors=0;std::ostringstream first;
  for(int i=0;i<12000;++i){const auto&s=map->structures[size_t(i)%map->structures.size()];float radius=random(.05f,.6f);
   float x=i%2?s.x1-radius+random(-.02f,.02f):random(0,24),y=i%3?s.y1+random(-.1f,.1f):random(0,24);
   float feet=random(-1.5f,4.f),height=random(.3f,2.5f),support=-100.f;bool blocked=false;
   for(const auto&box:w.structures())if(box.solid&&box.x2>box.x1&&box.y2>box.y1){
    float overlapX=std::min(x+radius,box.x2)-std::max(x-radius,box.x1);
    float overlapY=std::min(y+radius,box.y2)-std::max(y-radius,box.y1);
    if(overlapX<=.0001f||overlapY<=.0001f)continue;
    if(box.top<=feet+.025f)support=std::max(support,box.top);
    if(feet<box.top-.025f&&feet+height>box.bottom+.005f)blocked=true;
   }
   bool actual=w.structureBlocksHull(x,y,radius,feet,height);float actualSupport=w.structureSupportBelow(x,y,radius,feet);
   if(actual!=blocked||std::fabs(actualSupport-support)>.0001f){if(!errors)first<<"seed="<<seed<<" sample="<<i<<" position="<<x<<','<<y<<" radius="<<radius<<" feet="<<feet<<" height="<<height<<" blocked="<<actual<<'/'<<blocked<<" support="<<actualSupport<<'/'<<support;++errors;}
  }
  check(errors==0,"Spatial hull and support queries match a full scan across 12000 reproducible boundary cases",errors?first.str():"seed="+std::to_string(seed));
 }
 // A 2 cm wall can lie entirely between the old 3x3 hull samples.
 {std::vector<SurfaceBox> boxes{{{0,0,0},{2,2,1},1},{{.5f,.5f,0},{1.5f,1.5f,1},2}};
  auto exposed=exposedBoxFace({0,0,1},{2,0,1},{0,2,1},1,boxes);float area=0;for(auto p:exposed)area+=(p.u1-p.u0)*(p.v1-p.v0)*4;
  auto skin=exposedBoxFace({.5f,.5f,1},{1.5f,.5f,1},{.5f,1.5f,1},2,boxes);
  check(std::fabs(area-3.f)<.0001f&&skin.size()==1,"Coincident skins draw once while preserving the surrounding surface");
  check(exposedBoxFace({0,0,.5f},{2,0,.5f},{0,2,.5f},1,boxes).empty(),"Material bands do not add coincident caps inside a wall");
  boxes={{{0,0,0},{1,1,2},1},{{1,0,0},{2,1,2},2}};
  check(exposedBoxFace({1,0,0},{1,1,0},{1,0,2},1,boxes).empty(),"Touching solid boxes have no internal face");
   check(exposedBoxFace({1,1,0},{1,0,0},{1,1,2},1,boxes).empty(),"Grid wall winding also removes the shared face");
   check(exposedBoxFace({0,0,0},{0,1,0},{0,0,2},1,boxes).size()==1,"Grid wall exterior survives clipping");
 }
 {auto m=regression::room();m->structures.push_back({6.08f,5,6.10f,8,0,3});Game g(regression::campaign(m));g.m_player.pos={5.5f,6};
  check(!g.hullFits({6,6},0,Player::StandingHeight),"Thin wall rejects an overlapping player hull");
  check(!g.friendlyBodyFits(0,{6,6},0,1.6f),"Thin wall rejects an overlapping NPC hull");
  for(int i=0;i<80;++i)g.tryMove({.015f,.01f});
  check(g.m_player.pos.x<=5.881f&&g.m_player.pos.y>6.7f&&g.hullFits(g.m_player.pos,0,Player::StandingHeight),"Player slides along a thin wall and remains outside it");
 }
  // Narrow low rails must raise the feet before the complete hull meets them.
  // Checking only corners can miss the 9 cm cap between those support sites.
  {auto m=regression::room();m->structures.push_back({6.255f,1,6.345f,23,0,.07f});
   Game g(regression::campaign(m));g.m_player.pos={5.5f,6};g.m_player.z=0;bool clear=true;
   for(int i=0;i<120;++i){g.tryMove({.015f,0});clear&=g.hullFits(g.m_player.pos,g.m_player.z,Player::StandingHeight);}
   check(clear&&g.m_player.pos.x>7.2f,"Player steps across a narrow track rail without catching its side");
   check(g.friendlyWalkSegment(0,{5.5f,6},0,{7.3f,6},1.6f),"NPC steps across the same narrow track rail");
   g.m_player.pos={4,12};g.m_player.z=0;g.spawnCreature({Enemy::Kind::Chimera,{8,12},0},30,false);auto&e=g.m_enemies[0];e.heading=kPi;e.lastKnown=g.m_player.pos;e.state=Enemy::State::Chase;
   for(int i=0;i<300&&!g.dead();++i){g.updateEnemies(1.f/60);g.m_sounds.clear();}
   check(e.pos.x<6,"Chimera navigation steps across the same track rail");
  }
 // An off-centre opening with protruding visual architraves, in both directions.
 {auto m=regression::room();m->structures.push_back({6.08f,5,6.1f,8,0,4});Game g(regression::campaign(m));g.m_player.pos={3.4f,6};g.m_player.angle=kPi;
  g.spawnCreature({Enemy::Kind::Chimera,{5.8f,6},0},30,false);auto&e=g.m_enemies[0];e.heading=kPi;e.lastKnown=g.m_player.pos;e.state=Enemy::State::Chase;
  for(int i=0;i<120;++i){g.updateEnemies(1.f/60);g.m_sounds.clear();}
  check(e.pos.x<5.4f&&g.hullFits(e.pos,e.z,e.hull(),e.radius(),false),"An obstructed creature pose recovers on the same side of its wall");
  m->actorTracks.push_back(regression::guard({5.95f,6},{4,6}));Game npc(regression::campaign(m));npc.m_player.pos={20,20};
  for(int i=0;i<240;++i){npc.update({},1.f/60);npc.m_sounds.clear();}
  check(npc.m_friendlyActors[0].pose.position.x<5.4f,"An NPC recovers an old jamb overlap and resumes walking");
 }
 for(float sign:{-1.f,1.f}){
  auto m=regression::room();kit::officeWall(*m,1,10,10.2f,10.16f,0,3);kit::officeWall(*m,11.4f,10,23,10.16f,0,3);
  kit::officeDoorway(*m,10.2f,11.4f,10,10.16f,0,3);Game g(regression::campaign(m));g.m_player.pos={10.8f,10.08f-sign*1.2f};g.m_player.z=0;
  bool fits=true;for(int i=0;i<160;++i){g.tryMove({0,sign*.015f});fits&=g.hullFits(g.m_player.pos,0,Player::StandingHeight);}
  check(fits&&(g.m_player.pos.y-10.08f)*sign>1.f,"Player traverses framed opening in either direction",std::to_string(sign));
  check(g.friendlyWalkSegment(0,{10.8f,8.9f},0,{10.8f,11.3f},1.6f),"NPC sweep traverses the same framed opening");
  check(!g.hullFits({10,10.08f},0,Player::StandingHeight),"Wall beside the opening still blocks the player");
  auto guardMap=std::make_shared<AuthoredMapData>(*m);guardMap->actorTracks.push_back(regression::guard({10.8f,10.08f-sign*1.2f},{10.8f,10.08f+sign*1.2f}));Game npc(regression::campaign(guardMap));npc.m_player.pos={20,20};
  for(int i=0;i<400;++i){npc.update({},1.f/60);npc.m_sounds.clear();}
  check((npc.m_friendlyActors[0].pose.position.y-10.08f)*sign>.7f,"Autonomous NPC walks through the framed opening",std::to_string(sign));
 }
 {auto m=regression::room();m->structures={{1,9.9f,10.1f,10.1f,0,8},{11.5f,9.9f,23,10.1f,0,8},{10.1f,9.9f,11.5f,10.1f,2.5f,8}};
  m->doors.push_back({10.1f,11.5f,10});Game g(regression::campaign(m));g.m_world.setDoor(0,1,true);g.m_player.pos={10.8f,13};g.m_player.angle=kPi*.5f;
  g.spawnCreature({Enemy::Kind::Chimera,{10.3f,7.5f},0},30,false);auto&e=g.m_enemies[0];e.heading=kPi*.5f;e.lastKnown=g.m_player.pos;e.lastKnownZ=0;e.state=Enemy::State::Chase;
  bool clear=true;for(int i=0;i<700&&!g.dead();++i){g.updateEnemies(1.f/60);g.m_sounds.clear();clear&=!g.m_world.structureBlocksHull(e.pos.x,e.pos.y,e.radius(),e.z,e.hull());}
  check(clear&&e.pos.y>10.5f,"Chimera aligns with an off-centre doorway and crosses without clipping",std::to_string(e.pos.x)+","+std::to_string(e.pos.y));
 }
 // Chase, attack recovery, and a slow frame cannot drive a large body into its prey.
 for(float dt:{1.f/120,1.f/30,.2f}){
  Game g(regression::campaign(regression::room()));g.m_player.pos={6,12};g.m_player.angle=kPi;
  g.spawnCreature({Enemy::Kind::Chimera,{14,12},0},30,false);auto&e=g.m_enemies[0];e.heading=kPi;e.lastKnown=g.m_player.pos;e.lastKnownZ=0;e.state=Enemy::State::Chase;
  float closest=8,peak=0;bool attacked=false;
  for(int i=0;i<int(5/dt)&&!g.dead();++i){auto before=e.pos;g.updateEnemies(dt);closest=std::min(closest,length(e.pos-g.m_player.pos));peak=std::max(peak,length(e.pos-before)/dt);attacked|=e.windup>0||e.strike>0;g.m_sounds.clear();}
  std::ostringstream value;value<<"dt="<<dt<<" closest="<<closest<<" peak="<<peak<<" attacked="<<attacked;
  check(closest>=1.199f&&peak>3.6f&&peak<=5.85f&&attacked,"Chimera closes quickly, attacks, and preserves body clearance",value.str());
  g.m_player.pos=e.pos+Vec2{-.2f,0};g.m_velocity={};bool escaped=g.tryMove({-.1f,0});check(escaped,"A player overlapping an old saved creature can retreat");
 }
 // A narrow pillar misses the centre ray but blocks the creature's shoulders.
 {auto m=regression::room();m->actorTracks.push_back(regression::guard({6,12},{6,12}));Game g(regression::campaign(m));g.m_player.pos={21,21};
  g.spawnCreature({Enemy::Kind::Chimera,{14,12},0},30,false);auto&e=g.m_enemies[0];e.heading=kPi;e.lastKnown={6,12};e.state=Enemy::State::Chase;float closest=8;
  for(int i=0;i<300;++i){g.updateEnemies(1.f/60);closest=std::min(closest,length(e.pos-g.m_friendlyActors[0].pose.position));g.m_sounds.clear();}
  check(closest>=1.199f&&g.state(m->actorTracks[0].damageState)>0,"Chimera attacks NPC prey without entering its body during cooldown",std::to_string(closest));
 }
 {auto m=regression::room();m->structures.push_back({9.8f,11.5f,10.2f,11.85f,0,4});Game g(regression::campaign(m));g.m_player.pos={6,12};g.m_player.angle=kPi;
  g.spawnCreature({Enemy::Kind::Chimera,{14,12},0},30,false);auto&e=g.m_enemies[0];e.heading=kPi;e.lastKnown=g.m_player.pos;e.lastKnownZ=0;e.state=Enemy::State::Chase;
  bool clear=true;float detour=0;
  for(int i=0;i<900&&!g.dead();++i){g.updateEnemies(1.f/60);clear&=!g.m_world.structureBlocksHull(e.pos.x,e.pos.y,e.radius(),e.z,e.hull());detour=std::max(detour,std::fabs(e.pos.y-12));g.m_sounds.clear();}
  check(clear&&e.pos.x<9&&detour>.35f,"Chimera routes its body around an obstacle instead of pushing into it",std::to_string(e.pos.x)+","+std::to_string(e.pos.y));
 }
 {World w(PrologueChunk);int moving=0;float offset=0;
  for(const auto&l:w.lights())if(l.cargoLift){++moving;offset=l.z-w.cargoLiftHeight();check(l.mount==WorldLightMount::Bare,"Cab uses its own light housing");}
  bool follows=true;for(float z:{9.f,4.5f,0.f}){w.setCargoLiftHeight(z);for(const auto&l:w.lights())if(l.cargoLift)follows&=std::fabs(l.z-z-offset)<.001f;}
  check(moving==1&&follows,"Prologue light follows the cab at both landings and during its fall");
 }
 {Game g;g.loadLevel(37,false);g.m_enemies.clear();g.m_player.pos={14,16.6f};g.m_player.z=-28;g.m_player.grounded=true;
  for(int i=0;i<500;++i)g.tryMove({0,-.01f});
  check(g.m_player.pos.y<12.f&&g.m_player.z>-25.05f,"A player can climb out of the Switch Hall pit",std::to_string(g.m_player.pos.y)+","+std::to_string(g.m_player.z));
 }
 {Game g;g.loadLevel(28,false);g.m_enemies.clear();g.m_player.pos={19,14.3f};g.m_player.z=-28;g.m_player.grounded=true;
  for(int i=0;i<550;++i)g.tryMove({0,.01f});
  check(g.m_player.pos.y>19.f&&g.m_player.z>-25.05f,"A player can climb out of the inspection pit",std::to_string(g.m_player.pos.y)+","+std::to_string(g.m_player.z));
 }
 {Game g;g.loadLevel(64,false);g.m_enemies.clear();
  check(g.m_world.structureBlocksHull(8.375f,13.625f,.2f,-19,Player::StandingHeight),"The receiving fence blocks a hull overlapping its narrow panel");
 }
  {Game g;g.loadLevel(64,false);auto found=std::find_if(g.m_enemies.begin(),g.m_enemies.end(),[](const Enemy&e){return e.kind==Enemy::Kind::Chimera;});
   bool authored=found!=g.m_enemies.end();check(authored,"Research Receiving retains its authored Chimera");
   if(authored){auto enemy=*found;g.m_enemies={enemy};auto&e=g.m_enemies[0];g.setState(e.wakeState,1);e.wakeState=0;
    g.m_player.pos={10.7f,11.55f};g.m_player.z=-19;g.m_player.angle=0;e.heading=-kPi*.5f;e.awareness=60;e.lastKnown=g.m_player.pos;e.lastKnownZ=-19;e.state=Enemy::State::Chase;
    bool clear=true;for(int i=0;i<3600&&!g.dead()&&length(e.pos-g.m_player.pos)>1.9f;++i){g.updateEnemies(1.f/120);g.m_sounds.clear();clear&=!g.m_world.structureBlocksHull(e.pos.x,e.pos.y,e.radius(),e.z,e.hull());}
    std::ostringstream value;value<<e.pos.x<<','<<e.pos.y<<','<<e.z<<" distance "<<length(e.pos-g.m_player.pos);
    check(clear&&e.pos.x>8.5f&&length(e.pos-g.m_player.pos)<1.95f,"Authored Chimera leaves the cold hold and passes the receiving gate",value.str());
   }
  }
 // No generated guardrail sits against a solid wall tile at the same elevation.
 {auto m=regression::room();
  for(int side=0;side<2;++side){AuthoredLayerData deck;deck.elevation=3;deck.thickness=.2f;for(auto&row:deck.rows)row=std::string(24,'.');
   for(int y=5;y<10;++y)for(int x=4+side*4;x<8+side*4;++x)deck.rows[y][x]='=';m->layers.push_back(deck);}
   kit::guardRail(*m,4,9.94f,12,10,3);
   Game g(regression::campaign(m));g.m_player.pos={7,7.5f};g.m_player.z=3;
  for(int i=0;i<80;++i)g.tryMove({.025f,0});
  check(g.m_player.pos.x>8.9f&&g.friendlyWalkSegment(0,{7,7.5f},3,{9,7.5f},1.6f),"Players and NPCs cross joined gantry sections without an internal guardrail");
   int duplicates=0;for(const auto&s:g.world().structures())if(s.rail&&s.top<3.6f&&s.y1>9.9f&&s.y2<=10.01f&&s.x2-s.x1>.5f)++duplicates;
   check(duplicates==0,"Authored guardrails suppress duplicate automatic rails on the same edge");
 }
 {World w(2);int rails=0,overlaps=0;
  for(const auto&s:w.structures())if(s.rail){++rails;for(const auto&o:w.structures())if(!o.rail&&o.solid&&o.material!=CollisionProxyMaterial&&o.top>=s.top&&o.bottom<=s.bottom&&
   s.x1<o.x2-.001f&&s.x2>o.x1+.001f&&s.y1<o.y2-.001f&&s.y2>o.y1+.001f)++overlaps;}
  check(rails>0&&overlaps==0,"Gantry rails are trimmed out of walls",std::to_string(overlaps));
 }
 {Enemy e;e.kind=Enemy::Kind::Chimera;e.windup=.0001f;auto before=creatureAnimationSample(e,0);e.windup=0;e.strike=1;auto impact=creatureAnimationSample(e,0);
  check(before.clip==2&&impact.clip==2&&std::fabs(before.phase-impact.phase)<.001f&&std::fabs(impact.phase-.58f)<.001f,"Chimera damage coincides with its authored slam contact");
  advanceCreatureAnimation(e,0,.016f);e.strike=0;e.moving=true;advanceCreatureAnimation(e,.016f,.016f);
  check(e.previousVisualClip==2&&e.visualClip==1&&e.visualTransition>0&&e.visualTransition<1,"Attack recovers into locomotion with a crossfade");
 }
 {Mesh mesh(2129,"",2131);mesh.poseCreature(0,.3f);auto idle=mesh.triangles;mesh.poseCreature(1,.6f);auto walk=mesh.triangles;mesh.poseCreatureBlend(0,.3f,1,.6f,.5f);bool midpoint=true;float motion=0;
  for(size_t i=0;i<idle.size();++i)for(int v=0;v<3;++v){auto expected=(idle[i].v[v].p+walk[i].v[v].p)*.5f,actual=mesh.triangles[i].v[v].p;
   auto error=actual-expected,delta=walk[i].v[v].p-idle[i].v[v].p;midpoint&=std::fabs(error.x)+std::fabs(error.y)+std::fabs(error.z)<.00001f;motion=std::max(motion,std::fabs(delta.y));}
  check(midpoint&&motion>.01f,"Chimera transitions interpolate the actual animated body");
 }
 return passed;
}
}
