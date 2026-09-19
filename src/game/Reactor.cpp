#include "Game.h"
#include <fstream>
namespace retro {
const char* World::reactorObjective()const{
 switch(m_reactorStage){
  case ReactorStage::NoDisk:return "FIND AUTH DISK / UPPER MAINTENANCE";
  case ReactorStage::DiskHeld:return "INSERT DISK / LOWER CONTROL DESK";
  case ReactorStage::DiskLoaded:return "PRIME 01 FEED / LOWER PUMP BAY";
  case ReactorStage::FeedPrimed:return "OPEN 02 RETURN / UPPER MANIFOLD";
  case ReactorStage::ReturnPrimed:return "CONFIRM RELEASE / CONTROL DESK";
  case ReactorStage::Released:return "CONTAINMENT RELEASE AUTHORIZED";
 }return "";
}
void World::refreshReactorTerminals(){
 for(auto&t:m_terminals){
  if(t.reactorAction==1){t.title="REACTOR ACCESS / FLOPPY DRIVE A:";
   switch(m_reactorStage){
    case ReactorStage::NoDisk:t.line1="NO BOOT DISK. BULKHEAD FAIL-SAFE LOCKED.";t.line2="AUTH DISK: UPPER MAINTENANCE WORKBENCH.";break;
    case ReactorStage::DiskHeld:t.line1="AUTH DISK AVAILABLE. E TO INSERT.";t.line2="COOLANT INTERLOCK REQUIRES LOCAL RESET.";break;
    case ReactorStage::DiskLoaded:t.line1=m_reactorFault?"FLOW FAULT: FEED MUST PRECEDE RETURN.":"DISK ACCEPTED. FIRST PRIME 01 / FEED.";t.line2="LOWER PUMP BAY. THEN 02 / UPPER RETURN.";break;
    case ReactorStage::FeedPrimed:t.line1="FEED PRESSURIZED. OPEN 02 / RETURN.";t.line2="UPPER MANIFOLD. THEN CONFIRM HERE.";break;
    case ReactorStage::ReturnPrimed:t.line1="FLOW STABLE. E TO AUTHORIZE BULKHEAD.";t.line2="ENSURE CONTAINMENT AREA IS CLEAR.";break;
    case ReactorStage::Released:t.line1="AUTHORIZATION WRITTEN. INTERLOCK READY.";t.line2="CLEAR HOSTILES. OPEN LOWER BULKHEAD.";break;
   }
  }else if(t.reactorAction==2){t.line1=m_reactorStage<ReactorStage::DiskLoaded?"LOCAL CONTROL LOCKED. BOOT DRIVE A:.":m_reactorStage>=ReactorStage::FeedPrimed?"FEED LINE PRESSURIZED.":"PRIME FEED BEFORE OPENING RETURN.";t.line2="RETURN VALVE IS ON THE UPPER MANIFOLD.";
  }else if(t.reactorAction==3){t.line1=m_reactorStage<ReactorStage::DiskLoaded?"LOCAL CONTROL LOCKED. BOOT DRIVE A:.":m_reactorFault?"SEQUENCE FAULT. PRIME FEED FIRST.":m_reactorStage>=ReactorStage::ReturnPrimed?"RETURN FLOW STABLE.":"FEED PRESSURE REQUIRED BEFORE RETURN.";t.line2="AFTER RESET: CONFIRM AT LOWER COMPUTER.";}
 }
}
bool World::takeReactorDisk(){
 if(m_level!=3||m_liftPhase!=LiftPhase::Crashed||m_reactorStage!=ReactorStage::NoDisk)return false;
 m_reactorStage=ReactorStage::DiskHeld;refreshReactorTerminals();return true;
}
void World::useReactorTerminal(int action){
 if(m_level!=3||m_liftPhase!=LiftPhase::Crashed)return;
 if(action==1){
  if(m_reactorStage==ReactorStage::DiskHeld)m_reactorStage=ReactorStage::DiskLoaded;
  else if(m_reactorStage==ReactorStage::ReturnPrimed){m_reactorStage=ReactorStage::Released;m_controlReleased=true;}
 }else if(action==2&&m_reactorStage>=ReactorStage::DiskLoaded&&m_reactorStage<ReactorStage::Released){m_reactorStage=ReactorStage::FeedPrimed;m_reactorFault=false;}
 else if(action==3){
  if(m_reactorStage==ReactorStage::FeedPrimed)m_reactorStage=ReactorStage::ReturnPrimed;
  else if(m_reactorStage==ReactorStage::DiskLoaded)m_reactorFault=true;
 }
 refreshReactorTerminals();
}
bool Game::nearReactorDisk()const{
 if(m_level!=3||m_world.reactorStage()!=World::ReactorStage::NoDisk||std::fabs(m_player.z+6)>.4f)return false;
 auto delta=World::reactorDiskPosition()-m_player.pos;float distance=length(delta);Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 if(distance>1.65f||dot(delta,forward)<distance*.65f)return false;
 auto face=World::reactorDiskPosition()-normalized(delta)*.38f;
 return m_world.rayClear(m_player.pos,m_player.z+m_player.eye,face,World::ReactorDiskZ+.08f);
}
bool Game::testReactor(){
 std::ofstream out("reactor-test.txt");auto check=[&](bool ok,const char*name){out<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 auto game=liftInspection(World::LiftRideComplete);game.m_enemies.clear();game.m_clutter.clear();
 if(!check(!game.world().controlReleased(),"Crash does not bypass computer puzzle"))return false;
 game.m_world.useReactorTerminal(1);game.m_world.useReactorTerminal(2);game.m_world.useReactorTerminal(3);
 if(!check(game.world().reactorStage()==World::ReactorStage::NoDisk,"Computer and valves require authorization disk"))return false;
 game.m_player.pos={6,19.9f};game.m_player.z=-6;game.m_player.angle=kPi*.5f;
 if(!check(game.nearReactorDisk(),"Workbench disk reachable from upper floor"))return false;
 InputState use{};use.use=true;game.update(use,.01f);
 if(!check(game.world().reactorStage()==World::ReactorStage::DiskHeld,"E collects a protected quest disk"))return false;
 auto operate=[&](int index,Vec2 position,float z,float angle){game.m_logTime=0;game.m_activeLog=-1;game.m_player.pos=position;game.m_player.z=z;game.m_player.angle=angle;game.m_velocity={};game.update({},.01f);if(game.nearbyTerminal()!=index)return false;game.update(use,.01f);return true;};
 if(!check(operate(3,{18.1f,17.9f},-9,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::DiskLoaded,"E inserts disk into lower computer"))return false;
 if(!check(operate(5,{17.1f,18.4f},-6,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::DiskLoaded,"Wrong valve order stays locked and can be retried"))return false;
 if(!check(operate(4,{6.3f,17.f},-9,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::FeedPrimed,"Lower feed primes first"))return false;
 if(!check(operate(5,{17.1f,18.4f},-6,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::ReturnPrimed,"Upper return restores circulation"))return false;
 if(!check(!game.world().controlReleased(),"Final computer confirmation is required"))return false;
 if(!check(operate(3,{18.1f,17.9f},-9,kPi*.5f)&&game.world().controlReleased(),"Computer authorizes next bulkhead"))return false;
 game.storeChunk();game.m_chunks[3].world.unloadGeometry();game.m_chunks[3].resident=false;game.ensureChunk(3);
 if(!check(game.m_chunks[3].world.reactorStage()==World::ReactorStage::Released,"Puzzle persists across geometry reload"))return false;
 World world(3);if(!check(world.fits(3.6f,21.8f,-9,1)&&!world.fits(3.6f,21.8f,-6,1),"Upper equipment does not block the lower floor"))return false;
 for(auto&light:world.lights())if(&light!=&world.lights().back()){bool mounted=false;for(float z:{-9.f,-6.f,-3.f,0.f,3.f,6.f,9.f})mounted|=std::fabs(light.z+.15f-world.clearanceAbove(light.position.x,light.position.y,z))<.005f;if(!check(mounted,"Fixed light has a supporting ceiling"))return false;}
 game.restart();return check(game.world().reactorStage()==World::ReactorStage::NoDisk&&!game.world().controlReleased(),"Restart resets puzzle and disk");
}
}
