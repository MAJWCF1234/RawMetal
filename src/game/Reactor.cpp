#include "Game.h"
#include <fstream>
namespace retro {
void World::refreshReactorTerminals(){
 for(auto&t:m_terminals){
  if(t.reactorAction==1){t.title="R-03 / CONTAINMENT CONTROL";
   switch(m_reactorStage){
    case ReactorStage::NoDisk:t.line1="DRIVE A: AUTH FLOPPY REQUIRED.";t.line2="TAKE THE AUTH FLOPPY FROM THE UPPER WORKBENCH.";break;
    case ReactorStage::DiskHeld:t.line1="AUTH FLOPPY RECOVERED. DRIVE A: EMPTY.";t.line2="USE THIS COMPUTER TO INSERT THE FLOPPY.";break;
    case ReactorStage::DiskLoaded:t.line1=m_reactorFault?"RETURN TRIPPED / FEED PRESSURE TOO LOW.":"AUTH FLOPPY ACCEPTED / SERVICE SESSION ACTIVE.";t.line2="NEXT: PRIME P-01 COOLANT FEED ON THE LOWER FLOOR.";break;
    case ReactorStage::FeedPrimed:t.line1="P-01 FEED PRIMED: 4.2 BAR. RETURN: ISOLATED.";t.line2="NEXT: PRIME P-02 COOLANT RETURN ON THE UPPER FLOOR.";break;
    case ReactorStage::ReturnPrimed:t.line1="COOLANT CIRCULATION: NOMINAL.";t.line2="USE THIS COMPUTER TO RESET THE INTERLOCK.";break;
    case ReactorStage::Released:t.line1="INTERLOCK RESET ACCEPTED.";t.line2="BULKHEAD: LOCAL CONTROL ENABLED.";break;
   }
  }else if(t.reactorAction==2){
   t.line1=m_reactorStage<ReactorStage::DiskLoaded?"REMOTE SERVICE LOCK.":m_reactorStage>=ReactorStage::FeedPrimed?"INLET PRESSURE: 4.2 BAR.":"INLET PRESSURE: 0 BAR.";
   t.line2=m_reactorStage<ReactorStage::DiskLoaded?"INSERT THE AUTH FLOPPY AT THE R-03 COMPUTER FIRST.":m_reactorStage==ReactorStage::ReturnPrimed?"REPRIMING THE FEED RESETS RETURN. RESET AT R-03.":m_reactorStage==ReactorStage::FeedPrimed?"NEXT: PRIME P-02 RETURN ON THE UPPER FLOOR.":m_reactorStage==ReactorStage::Released?"P-01 / CIRCULATION AUTHORIZED.":"P-01 / PRIME FEED BEFORE THE P-02 RETURN.";
  }else if(t.reactorAction==3){
   t.line1=m_reactorStage<ReactorStage::DiskLoaded?"REMOTE SERVICE LOCK.":m_reactorFault?"TRIPPED: INLET PRESSURE LOW.":m_reactorStage>=ReactorStage::ReturnPrimed?"RETURN FLOW: NOMINAL.":"RETURN FLOW: ISOLATED.";
   t.line2=m_reactorStage<ReactorStage::DiskLoaded?"INSERT THE AUTH FLOPPY AT THE R-03 COMPUTER FIRST.":m_reactorStage==ReactorStage::DiskLoaded?"PRIME P-01 FEED ON THE LOWER FLOOR FIRST.":m_reactorStage==ReactorStage::Released?"P-02 / CIRCULATION AUTHORIZED.":m_reactorStage==ReactorStage::ReturnPrimed?"NEXT: CONFIRM THE INTERLOCK RESET AT R-03.":"P-02 / PRIME RETURN, THEN RESET THE R-03 COMPUTER.";
  }
 }
}
bool World::takeReactorDisk(){
 if(!hasLift()||m_liftPhase!=LiftPhase::Crashed||m_reactorStage!=ReactorStage::NoDisk)return false;
 m_reactorStage=ReactorStage::DiskHeld;refreshReactorTerminals();return true;
}
void World::syncReactor(const World& source){
 if(m_reactorStage!=source.m_reactorStage||m_reactorFault!=source.m_reactorFault){m_reactorStage=source.m_reactorStage;m_reactorFault=source.m_reactorFault;refreshReactorTerminals();}
 if(source.m_controlReleased)m_controlReleased=true;
}
void World::useReactorTerminal(int action){
 if(!hasLift()||m_liftPhase!=LiftPhase::Crashed)return;
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
World* Game::reactorWorld(){
 if(m_world.hasLift())return &m_world;
 for(int level=0;level<chunkCount();++level)if(level!=m_level&&m_chunks[level].world.hasLift())return &m_chunks[level].world;
 return nullptr;
}
void Game::syncReactorComplex(){
 if(m_world.hasLift())return;
 bool reactor=false;for(const auto& t:m_world.terminals())reactor|=t.reactorAction!=0;
 if(reactor)if(auto* source=reactorWorld())m_world.syncReactor(*source);
}
void Game::useReactorAction(int action){
 // The puzzle lives with the lift; its terminals may stand in stitched chunks.
 World* reactor=reactorWorld();if(!reactor)reactor=&m_world;
 if(action==1&&reactor->reactorStage()==World::ReactorStage::DiskHeld){
  if(!takeQuestItem(ReactorAuthDisk)){m_pickupNotice="REACTOR AUTH DISK REQUIRED";m_pickupNoticeTime=2;return;}
 }
 reactor->useReactorTerminal(action);
 if(reactor!=&m_world)m_world.syncReactor(*reactor);
 if(reactor->reactorStage()==World::ReactorStage::Released){setState(stateId("reactor_bulkhead_released"),1);setObjective(stateId("restore_reactor_circulation"),ObjectiveStatus::Complete);}
}
bool Game::nearReactorDisk()const{
 if(!m_world.hasLift()||m_world.reactorStage()!=World::ReactorStage::NoDisk||std::fabs(m_player.z+6)>.4f)return false;
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
 if(!check(game.world().reactorStage()==World::ReactorStage::DiskHeld&&game.hasQuestItem(ReactorAuthDisk),"E collects a protected reusable quest item"))return false;
 // The puzzle spans the stitched complex: R-03 in the core, P-01 feed in
 // Containment, P-02 return in the coolant loop. Each step is taken in its
 // own chunk, with the player moved there across the world.
 auto goTo=[&](int level,Vec2 local,float z,float angle){Vec2 global=game.chunkOffset(level)+local;game.m_player.pos=global-game.chunkOffset(game.level());game.crossChunkBoundary();
  game.m_player.z=z;game.m_player.angle=angle;game.m_velocity={};};
 auto operate=[&](int level,int index,Vec2 position,float z,float angle){game.m_logTime=0;game.m_activeLog=-1;goTo(level,position,z,angle);game.update({},.01f);
  if(game.level()!=level||game.nearbyTerminal()!=index)return false;game.update(use,.01f);return true;};
 if(!check(operate(ReactorCoreChunk,0,{4.5f,5.4f},-6,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::DiskLoaded&&!game.hasQuestItem(ReactorAuthDisk),"E inserts and consumes the authorization disk at R-03"))return false;
 if(!check(operate(CoolantLoopChunk,0,{11.f,2.6f},-6,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::DiskLoaded,"Wrong valve order stays locked and can be retried"))return false;
 if(!check(operate(3,3,{3.2f,20.9f},-9,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::FeedPrimed,"Containment feed primes first"))return false;
 if(!check(operate(CoolantLoopChunk,0,{11.f,2.6f},-6,kPi*.5f)&&game.world().reactorStage()==World::ReactorStage::ReturnPrimed,"Coolant loop return restores circulation"))return false;
 if(!check(!game.world().controlReleased()&&!game.reactorWorld()->controlReleased(),"Final computer confirmation is required"))return false;
 if(!check(operate(ReactorCoreChunk,0,{4.5f,5.4f},-6,kPi*.5f)&&game.world().controlReleased()&&game.reactorWorld()->controlReleased(),"Computer authorizes the core bulkhead"))return false;
 game.storeChunk();game.m_chunks[3].world.unloadGeometry();game.m_chunks[3].resident=false;game.ensureChunk(3);
 if(!check(game.m_chunks[3].world.reactorStage()==World::ReactorStage::Released,"Puzzle persists across geometry reload"))return false;
 World world(3);if(!check(world.fits(2.4f,22.5f,-9,1)&&!world.fits(2.4f,22.5f,-6,1),"Upper equipment does not block the lower floor"))return false;
 World core(ReactorCoreChunk),loop(CoolantLoopChunk);
 if(!check(!core.fits(3.5f,1.27f,-6,1)&&core.fits(4.5f,5.4f,-6,1),"Instrument cabinets collide but computer approach stays clear"))return false;
 if(!check(core.rayClear({7.2f,8.5f},-4.8f,{5.5f,8.5f},-8.5f),"The gantry overlooks the core floor"))return false;
 if(!check(!core.fits(12,12,-9,1)&&!core.fits(12,21,-9,1)&&!core.fits(21,12,-9,1)&&!core.fits(12,18.3f,-9,1)&&core.fits(6,16,-9,1)&&core.fits(5,20,-9,1),"Vessel and steam generators are solid; the floor round them is clear"))return false;
 // Wall lamps are checked against their wall by the map audit; every fixed
 // ceiling lamp hangs 15 cm under a real ceiling, roof, deck or slab.
 for(const World* map:{&world,&core,&loop})for(auto&light:map->lights())if(!(map->hasLift()&&&light==&map->lights().back())&&light.mount==WorldLightMount::Ceiling){bool mounted=false;
  for(float z:{-9.f,-6.f,-3.f,0.f,3.f,6.f,9.f})mounted|=std::fabs(light.z+.15f-map->clearanceAbove(light.position.x,light.position.y,z))<.005f;
  if(!mounted)out<<"Unmounted light in "<<map->level()<<" at "<<light.position.x<<','<<light.position.y<<','<<light.z<<'\n';
  if(!check(mounted,"Fixed light has a supporting ceiling"))return false;}
 game.restart();return check(game.world().reactorStage()==World::ReactorStage::NoDisk&&!game.world().controlReleased(),"Restart resets puzzle and disk");
}
}
