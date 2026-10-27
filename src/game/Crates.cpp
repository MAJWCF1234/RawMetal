#include "Game.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
namespace retro {
// Heavy crates. Lean into one along an axis for a moment and it goes, and it
// keeps going on its own weight until something stops it: a wall, a door,
// another crate, a body, a step up or down. It always comes to rest on the
// centre of a floor cell, so a room of them is a grid puzzle. Where a crate
// rests on a plate the plate's state is 1. Positions live in script state,
// so saves carry them without a format change; a map's reset state puts every
// crate back where it started.
namespace {
constexpr float ShoveSeconds=.35f,SlideSpeed=3.4f;
StateId crateKey(int level,size_t crate,char axis){return stateId("crate_"+std::to_string(level)+"_"+std::to_string(crate)+"_"+axis);}
}
bool Game::crateFree(const World& w,size_t index,Vec2 at)const{
 const auto& crate=w.pushCrates()[index];const float half=crate.size*.5f-.04f;
 for(float x:{-half,0.f,half})for(float y:{-half,0.f,half}){Vec2 p=at+Vec2{x,y};
  // The same floor all the way: a crate does not climb a step or drop off one.
  if(std::fabs(w.floorHeight(p.x,p.y)-crate.z)>.05f)return false;
  if(!w.fits(p.x,p.y,crate.z+.02f,crate.size-.04f,false)||w.crateBlocks(p.x,p.y,crate.z+.02f,crate.size-.04f,int(index))||w.doorBlocks(p.x,p.y,crate.z+.02f,crate.size-.04f))return false;
 }
 auto overlaps=[&](Vec2 p,float feet,float top,float radius){return std::fabs(p.x-at.x)<crate.size*.5f+radius&&std::fabs(p.y-at.y)<crate.size*.5f+radius&&feet<crate.z+crate.size&&top>crate.z;};
 if(overlaps(m_player.pos,m_player.z,m_player.z+m_player.hullHeight(),.2f))return false;
 if(&w==&m_world){for(const auto& e:m_enemies)if(e.alive&&overlaps(e.pos,e.bodyBottom(),e.bodyTop(),.2f))return false;
  for(const auto& npc:m_friendlyActors)if(npc.level==m_level&&overlaps(npc.pose.position,npc.pose.z,npc.pose.z+1.7f,.2f))return false;}
 return true;
}
void Game::restoreCrates(World& w,int level)const{
 if(w.pushCrates().empty())return;
 for(size_t i=0;i<w.pushCrates().size();++i){auto& crate=w.m_pushCrates[i];
  int x=state(crateKey(level,i,'x')),y=state(crateKey(level,i,'y'));
  crate.slide={};crate.position=x||y?Vec2{x*.001f,y*.001f}:crate.home;
 }
}
void Game::updatePushCrates(const InputState& input,float dt){
 auto& crates=m_world.m_pushCrates;if(crates.empty()){m_crateShove=0;return;}
 // A reset lever (terminal) puts every crate on this map back.
 if(m_world.crateReset()&&state(m_world.crateReset())){
  for(size_t i=0;i<crates.size();++i){crates[i].position=crates[i].home;crates[i].slide={};setState(crateKey(m_level,i,'x'),0);setState(crateKey(m_level,i,'y'),0);}
  setState(m_world.crateReset(),0);sound(Sound::LiftCreak,.55f,.8f);
 }
 int pushing=-1;Vec2 axis{};
 // Where the player is trying to walk, not where they are moving: a crate
 // stops them dead while they lean on it.
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)},right{-forward.y,forward.x},wish{};
 if(input.forward)wish+=forward;if(input.back)wish+=forward*-1;if(input.right)wish+=right;if(input.left)wish+=right*-1;
 if(input.vrTracked)wish=forward*input.vrMoveForward+right*input.vrMoveRight;
 if(heldNpc()>=0||holdingClutter())wish={};
 for(size_t i=0;i<crates.size();++i){auto& crate=crates[i];
  if(lengthSq(crate.slide)>0){
   // Slide toward the cell it was bound for when it was shoved. Something
   // stepping into the way stops it short, on the last cell centre it cleared.
   float travel=SlideSpeed*dt;bool stopped=false;
   while(travel>0&&!stopped){float left=dot(crate.target-crate.position,crate.slide),step=std::min({travel,.05f,left});travel-=step;
    if(left<=.0001f){crate.position=crate.target;stopped=true;break;}
    Vec2 next=crate.position+crate.slide*step;
    if(crateFree(m_world,i,next))crate.position=next;
    else{Vec2 back=crate.position-crate.slide*.49f;Vec2 snapped{std::floor(back.x)+.5f,std::floor(back.y)+.5f};if(crateFree(m_world,i,snapped))crate.position=snapped;stopped=true;}}
   if(stopped){
    crate.slide={};
    m_sounds.push_back({Sound::Land,crate.position,1.f,.45f,true,18,crate.z+.2f});
    m_sounds.push_back({Sound::JunkMetal,crate.position,.5f,.5f,true,14,crate.z+.5f});
    setState(crateKey(m_level,i,'x'),int(std::lround(crate.position.x*1000)));setState(crateKey(m_level,i,'y'),int(std::lround(crate.position.y*1000)));
   }
   continue;
  }
  // The player leans into one face: grounded on the crate's floor, against
  // the face, square to it and walking into it.
  if(!m_player.grounded||std::fabs(m_player.z-crate.z)>.1f||lengthSq(wish)<.25f)continue;
  Vec2 to=crate.position-m_player.pos;Vec2 along=std::fabs(to.x)>std::fabs(to.y)?Vec2{to.x>0?1.f:-1.f,0}:Vec2{0,to.y>0?1.f:-1.f};
  float gap=std::fabs(dot(to,along))-crate.size*.5f,offset=std::fabs(to.x*along.y-to.y*along.x);
  if(gap<.42f&&offset<crate.size*.4f&&dot(normalized(wish),along)>.75f){pushing=int(i);axis=along;}
 }
 if(pushing<0){m_crateShove=0;}
 else if((m_crateShove+=dt)>=ShoveSeconds){
  auto& crate=crates[size_t(pushing)];m_crateShove=0;
  // Where it will come to rest: the last whole cell it can reach along this
  // axis (checked every 10 cm, so a post or a table leg stops it).
  Vec2 target=crate.position;
  for(int cell=0;cell<24;++cell){bool clear=true;for(int k=1;k<=10&&clear;++k)clear=crateFree(m_world,size_t(pushing),target+axis*(k*.1f));if(!clear)break;target+=axis;}
  if(lengthSq(target-crate.position)>.25f){crate.slide=axis;crate.target=target;m_sounds.push_back({Sound::LiftCreak,crate.position,.7f,1.35f,true,16,crate.z+.3f});}
  else m_sounds.push_back({Sound::JunkMetal,crate.position,.35f,.55f,true,10,crate.z+.5f});
 }
 // Plates read the crates at rest.
 for(const auto& plate:m_world.cratePlates()){bool held=false;
  for(const auto& crate:crates)held|=lengthSq(crate.slide)==0&&lengthSq(crate.position-plate.position)<.3f*.3f&&std::fabs(crate.z-m_world.floorHeight(plate.position.x,plate.position.y))<.1f;
  if((state(plate.state)!=0)!=held){setState(plate.state,held?1:0);if(held)sound(Sound::Door,.5f,.7f);}
 }
}
}
namespace retro {
// The Central Stores test bay, played with real input: the wrong first shove
// wedges the load and the winch brings it back; the right two (south, then
// west) land it on the scale and release the cage; it survives a save.
bool Game::testCrates(){
 std::ofstream report("crates-test.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& text){report<<(pass?"PASS ":"FAIL ")<<text<<'\n';report.flush();ok&=pass;};
 Game g;g.setDeveloperMode(true);g.loadLevel(54,false);g.m_enemies.clear();g.m_friendlyActors.clear();
 if(g.world().pushCrates().empty()){check(false,"Central Stores has a test load");return false;}
 auto crate=[&]{return g.world().pushCrates()[0];};
 auto where=[&]{std::ostringstream s;s<<" crate at "<<crate().position.x<<','<<crate().position.y;return s.str();};
 auto shove=[&](Vec2 from,Vec2 dir){g.m_player.pos=from;g.m_player.z=g.world().floorHeight(from.x,from.y);g.m_player.grounded=true;g.m_velocity={};g.m_player.angle=std::atan2(dir.y,dir.x);
  InputState lean;lean.forward=true;for(int i=0;i<45;++i)g.update(lean,1.f/60);
  report<<"  leaned from "<<from.x<<','<<from.y<<" to "<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<" grounded "<<g.player().grounded<<" crate "<<g.world().pushCrates()[0].position.x<<','<<g.world().pushCrates()[0].position.y<<'\n';
  for(int i=0;i<150;++i)g.update({},1.f/60);};
 const float floor=crate().z;
 check(!g.world().fits(16.5f,18.5f,floor+.1f,1.f)&&std::fabs(g.world().supportBelow(16.5f,18.5f,floor+1.2f)-(floor+1.f))<.01f,"The crate is solid and can be stood on");
 shove({17.6f,18.5f},{-1,0});
 check(lengthSq(crate().position-Vec2{14.5f,18.5f})<.01f,"Shoved west first it slides until the staged load stops it,"+where());
 g.setState(stateId("mnt_test_reset"),1);g.update({},1.f/60);g.update({},1.f/60);
 check(lengthSq(crate().position-Vec2{16.5f,18.5f})<.01f&&!g.state("mnt_test_reset"),"The winch drags it home,"+where());
 shove({16.5f,17.6f},{0,1});
 check(lengthSq(crate().position-Vec2{16.5f,19.5f})<.01f,"Shoved south it stops against the bay's bollard,"+where());
 check(!g.state("mnt_test_load"),"The scale is empty until the load is on it");
 shove({17.6f,19.5f},{-1,0});
 check(lengthSq(crate().position-Vec2{9.5f,19.5f})<.01f&&g.state("mnt_test_load")==1,"Shoved west again it rests against the bollard on the scale and the cage releases,"+where());
 {Game loaded;bool decoded=loaded.decodeSave(g.encodeSave());check(decoded&&lengthSq(loaded.world().pushCrates()[0].position-Vec2{9.5f,19.5f})<.01f&&loaded.state("mnt_test_load")==1,"A save keeps the crate where it was left");}
 g.setState(stateId("mnt_test_reset"),1);g.update({},1.f/60);g.update({},1.f/60);
 check(lengthSq(crate().position-Vec2{16.5f,18.5f})<.01f&&!g.state("mnt_test_load"),"Winched home from the scale, the scale reads empty again,"+where());
 return ok;
}
}
