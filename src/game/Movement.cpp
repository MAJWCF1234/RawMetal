#include "Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>
namespace retro {
float Game::groundHeight(Vec2 p)const{
 return groundHeight(p,m_player.z+(m_player.grounded?.215f:0.f));
}
float Game::groundHeight(Vec2 p,float feet)const{
 auto center=p;const auto&world=worldAt(center);float height=world.supportBelow(center.x,center.y,feet);
 for(float x:{-.20f,.20f})for(float y:{-.20f,.20f}){auto corner=p+Vec2{x,y};const auto&w=worldAt(corner);height=std::max(height,w.supportBelow(corner.x,corner.y,feet));}return height;
}
bool Game::hullFits(Vec2 p,float feet,float height)const{
 for(float x:{-.20f,0.f,.20f})for(float y:{-.20f,0.f,.20f}){
  auto corner=p+Vec2{x,y};const auto&w=worldAt(corner);float px=corner.x,py=corner.y;
  if(!w.fits(px,py,feet,height)||w.doorBlocks(px,py,feet,height))return false;
 }
 // Rails are only 5.5 cm thick. Point-sampling the 40 cm player hull can put
 // every sample on one side or the other while the body itself overlaps the rail.
 // Test the hull footprint against rail AABBs so the player can never phase into
 // the narrow volume and become trapped inside it.
 auto center=p;const auto&w=worldAt(center);
 if(w.railBlocksHull(center.x,center.y,.20f,feet,height))return false;
 return true;
}
bool Game::tryMove(Vec2 delta){
 auto old=m_player.pos;
 for(int axis=0;axis<2;++axis){auto next=m_player.pos;if(axis==0)next.x+=delta.x;else next.y+=delta.y;
  float feet=m_player.z,ground=groundHeight(next);
  if(m_player.grounded&&ground>feet&&ground-feet<=.215f)feet=ground;
  if(hullFits(next,feet,m_player.hullHeight())){m_player.pos=next;m_player.z=feet;}
  else{
   // Resolve contact, preserving motion along the unobstructed wall axis.
   auto start=m_player.pos;float lo=0,hi=1;
   for(int i=0;i<8;++i){float t=(lo+hi)*.5f;auto p=start+(next-start)*t;
    if(hullFits(p,m_player.z,m_player.hullHeight()))lo=t;else hi=t;}
   m_player.pos=start+(next-start)*lo;
   if(axis==0)m_velocity.x=0;else m_velocity.y=0;
  }
 }return lengthSq(m_player.pos-old)>0;
}
void Game::updateMovement(const InputState& input,float dt){
 if(input.jump&&!m_previousJump)m_jumpBuffer=.12f;m_previousJump=input.jump;
 if(input.crouch&&!m_player.crouched){
  // Ducking in flight raises the feet while preserving the top of the hull.
  if(!m_player.grounded)m_player.z+=.42f;m_player.crouched=true;
 }else if(!input.crouch&&m_player.crouched){
  float feet=m_player.grounded?m_player.z:std::max(groundHeight(m_player.pos),m_player.z-.42f);
  if(hullFits(m_player.pos,feet,1.f)){m_player.crouched=false;m_player.z=feet;}
 }
 m_player.eye+=( (m_player.crouched?.48f:.78f)-m_player.eye)*std::min(1.f,dt*16.f);
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)},right{-forward.y,forward.x},wish{};
 if(input.forward)wish+=forward;if(input.back)wish+=forward*-1;if(input.right)wish+=right;if(input.left)wish+=right*-1;
 if(lengthSq(wish)>0)wish=normalized(wish);
 int count=std::max(1,int(std::ceil(dt*120)));float step=dt/count;
 for(int tick=0;tick<count;++tick){
  bool grounded=m_player.grounded;auto old=m_player.pos;
  m_jumpBuffer=std::max(0.f,m_jumpBuffer-step);m_coyote=grounded?.075f:std::max(0.f,m_coyote-step);
  if(m_jumpBuffer>0&&m_coyote>0){m_player.verticalVelocity=5.4f;m_player.grounded=false;m_jumpBuffer=0;m_coyote=0;m_verticalSpringVelocity-=.45f;sound(Sound::Jump,.38f,.97f+(m_stepVariant%3)*.025f);}
  float speed=length(m_velocity),wishSpeed=m_player.crouched?1.65f:input.sprint?5.4f:3.6f;
  if(m_player.grounded&&speed>0){float remaining=std::max(0.f,speed-std::max(1.5f,speed)*6.f*step);m_velocity=m_velocity*(remaining/speed);}
  if(lengthSq(wish)>0){
   float acceleration=m_player.grounded?12.f:3.f;
   // Air control accelerates only the requested component, retaining launch momentum.
   float target=m_player.grounded?wishSpeed:std::min(wishSpeed,1.25f);
   float add=target-dot(m_velocity,wish);
   if(add>0)m_velocity+=wish*std::min(add,acceleration*wishSpeed*step);
  }
  tryMove(m_velocity*step);
  float ground=groundHeight(m_player.pos);
  if(m_player.grounded&&m_player.z>ground+.03f){
   if(m_player.z-ground<=.215f&&hullFits(m_player.pos,ground,m_player.hullHeight()))m_player.z=ground;
   else m_player.grounded=false;
  }
  float previousZ=m_player.z;
  if(!m_player.grounded)m_player.verticalVelocity-=14.f*step;
  m_player.z+=m_player.verticalVelocity*step;
  float ceiling=100;
  for(float x:{-.20f,.20f})for(float y:{-.20f,.20f}){auto corner=m_player.pos+Vec2{x,y};const auto&w=worldAt(corner);ceiling=std::min(ceiling,w.clearanceAbove(corner.x,corner.y,previousZ));}
  if(m_player.z+m_player.hullHeight()>ceiling){m_player.z=ceiling-m_player.hullHeight();m_player.verticalVelocity=std::min(0.f,m_player.verticalVelocity);}
  float impact=-m_player.verticalVelocity;
  if(m_player.verticalVelocity<=0&&m_player.z<=ground+.005f&&previousZ>=ground-.025f){m_player.z=ground;m_player.verticalVelocity=0;m_player.grounded=true;}
  if(!grounded&&m_player.grounded){sound(Sound::Land,std::clamp(impact*.105f,.2f,.85f),.94f);m_verticalSpringVelocity-=std::min(impact,9.f)*.105f;m_stepDistance=0;}
  if(grounded&&m_player.grounded){m_stepDistance+=length(m_player.pos-old);
   if(m_stepDistance>(m_player.crouched?.85f:input.sprint?1.45f:1.22f)){
    m_stepDistance=0;unsigned variation=(m_stepVariant++*3)%4;auto base=m_world.metalFloor(int(m_player.pos.x),int(m_player.pos.y))?Sound::Metal1:Sound::Concrete1;
    sound(Sound(int(base)+variation),m_player.crouched?.20f:input.sprint?.65f:.48f,.95f+variation*.035f);
   }
  }
 }
}
int Game::nearbyTerminal()const{
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};int nearest=-1;float closest=1.8f;
 for(size_t i=0;i<m_world.terminals().size();++i){auto&t=m_world.terminals()[i];auto delta=t.position-m_player.pos;float distance=length(delta),along=dot(delta,forward),base=t.z+m_world.floorHeight(t.position.x,t.position.y);
  if(distance>=closest||along<=0||std::fabs(delta.x*forward.y-delta.y*forward.x)>.4f||std::fabs(m_player.z-base)>=.65f)continue;
  // Stop the sight ray before the computer's own collision housing.
  auto face=t.position-normalized(delta)*.4f;
  if(!m_world.rayClear(m_player.pos,m_player.z+m_player.eye,face,base+.75f))continue;
  closest=distance;nearest=int(i);
 }return nearest;
}
const char* Game::interactionHint()const{
 if(m_logTime>0)return "E / CLOSE LOG";
 if(holdingClutter())return "E / DROP     FIRE / PUNT";
 if(nearReactorDisk())return "E / TAKE REACTOR AUTH DISK";
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 int door=m_world.nearbyDoor(m_player.pos,forward,m_player.z);
 if(door>=0){auto&d=m_world.doors()[door];if(d.transfer&&(enemiesRemaining()>0||(m_level>=2&&!m_world.controlReleased())))return "TRANSFER INTERLOCK / LOCKED";return d.opening?"E / CLOSE BULKHEAD":d.transfer?"E / TRANSFER BULKHEAD":"E / OPEN BULKHEAD";}
 if(int terminal=nearbyTerminal();terminal>=0){auto&t=m_world.terminals()[terminal];if(t.reactorAction)return t.reactorAction==1?"E / USE COMPUTER":"E / OPERATE VALVE";return t.control?(m_world.hasLift()?"E / LIFT DISPATCH":"E / GANTRY CONTROL"):"E / READ SHIFT LOG";}
 return nearbyClutter()>=0?"E / LIFT":nullptr;
}
void Game::updateInteraction(const InputState& input,float dt){
 m_logTime=std::max(0.f,m_logTime-dt);
 if(input.use&&!m_previousUse&&m_logTime>0){m_logTime=0;m_activeLog=-1;m_previousUse=true;return;}
 if(input.use&&!m_previousUse&&holdingClutter()){interactClutter();m_previousUse=true;m_world.updateDoors(dt);return;}
 if(input.use&&!m_previousUse&&nearReactorDisk()&&m_world.takeReactorDisk()){giveQuestItem(ReactorAuthDisk);setObjective(stateId("restore_reactor_circulation"),ObjectiveStatus::Active);m_pickupNotice="REACTOR AUTH DISK ACQUIRED";m_pickupNoticeTime=4;sound(Sound::Pickup,.65f);m_previousUse=true;return;}
 if(input.use&&!m_previousUse){Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};int door=m_world.nearbyDoor(m_player.pos,forward,m_player.z);
  if(holdingClutter()){interactClutter();}
  else if(door>=0){if(!m_world.doors()[door].transfer||(enemiesRemaining()==0&&(m_level<2||m_world.controlReleased())))useDoor(door);}
  else if(int terminal=nearbyTerminal();terminal>=0){m_activeLog=terminal;m_logTime=9.f;if(m_world.terminals()[terminal].reactorAction)useReactorAction(m_world.terminals()[terminal].reactorAction);if(m_world.terminals()[terminal].control){
   if(m_world.hasLift()){if(m_world.insideLift(m_player.pos.x,m_player.pos.y)&&m_player.pos.y>10.35f&&m_world.startLift()){m_logTime=0;m_activeLog=-1;sound(Sound::Door,.8f,.7f);}}
   else m_world.releaseControl();
  }sound(Sound::Exit,.4f);}
  if(!holdingClutter()&&door<0&&m_logTime==0)interactClutter();
 }
 m_previousUse=input.use;m_world.updateDoors(dt);
}
bool Game::testMovement(){
 std::ofstream debug("movement-diagnostic.txt");
 auto clean=[](){auto game=validationScene(Enemy::Kind::Huntsman,3);game.m_enemies.clear();return game;};
 {auto slide=clean();slide.m_player.pos={1.3f,4.5f};slide.m_velocity={-4,2};slide.tryMove({-.6f,.1f});
  if(slide.player().pos.x<1.199f||slide.player().pos.x>1.21f||std::fabs(slide.player().pos.y-4.6f)>.001f||slide.m_velocity.x!=0||slide.m_velocity.y!=2)return false;
 }
 {auto rails=clean();rails.loadLevel(2,false);const Structure* rail=nullptr;
  for(const auto&s:rails.m_world.structures())if(s.rail&&s.bottom>2.9f){rail=&s;break;}
  if(!rail)return false;float width=rail->x2-rail->x1,depth=rail->y2-rail->y1;Vec2 a,b;
  if(width<depth){float y=(rail->y1+rail->y2)*.5f;a={rail->x1-.12f,y};b={rail->x2+.12f,y};}
  else {float x=(rail->x1+rail->x2)*.5f;a={x,rail->y1-.12f};b={x,rail->y2+.12f};}
  bool sideA=rails.hullFits(a,rail->bottom,rails.m_player.hullHeight()),sideB=rails.hullFits(b,rail->bottom,rails.m_player.hullHeight());
  debug<<"rail hull sides "<<sideA<<' '<<sideB<<'\n';if(sideA||sideB)return false;
 }
 auto game=clean();InputState input{};input.crouch=true;input.jump=true;float peak=0;
 for(int i=0;i<150;++i){game.update(input,1.f/120.f);peak=std::max(peak,game.player().z);}debug<<"jump "<<peak<<" grounded "<<game.player().grounded<<'\n';if(peak<.8f||!game.player().grounded)return false;
 auto run=[&](int rate){auto g=clean();InputState move{};move.forward=true;move.sprint=true;
  for(int i=0;i<rate;++i){move.jump=i==rate/4;move.crouch=i>rate/3&&i<rate*3/4;g.update(move,1.f/rate);}return g;};
 auto a=run(60),b=run(120),c=run(30);debug<<"rate "<<length(a.player().pos-b.player().pos)<<' '<<a.player().z<<' '<<b.player().z<<'\n';if(length(a.player().pos-b.player().pos)>.15f||std::fabs(a.player().z-b.player().z)>.15f||length(c.player().pos-b.player().pos)>.25f)return false;
 game=clean();game.m_player.pos={20.5f,15.75f};game.m_player.angle=-kPi*.5f;game.m_player.z=game.groundHeight(game.m_player.pos);input={};input.forward=true;
 for(int i=0;i<130;++i)game.update(input,1.f/120.f);debug<<"stairs "<<game.player().pos.y<<' '<<game.player().z<<'\n';if(std::fabs(game.player().z-1.2f)>.01f||game.player().pos.y>=13)return false;
 game=clean();game.m_player.pos={9.6f,14.5f};game.m_player.angle=0;input={};input.forward=true;
 for(int i=0;i<60;++i)game.update(input,1.f/120.f);debug<<"standing "<<game.player().pos.x<<'\n';if(game.player().pos.x>9.81f)return false;
 input.crouch=true;for(int i=0;i<100;++i)game.update(input,1.f/120.f);debug<<"duck "<<game.player().pos.x<<'\n';if(game.player().pos.x<10.6f)return false;
 input={};game.update(input,.01f);if(game.player().pos.x<11.2f&&!game.player().crouched)return false;
 game=clean();game.m_player.pos={8.5f,4.4f};game.m_player.angle=-kPi*.5f;game.m_velocity={0,-3.6f};bool reachedCover=false;
 for(int i=0;i<130;++i){input={};input.jump=i==0;input.crouch=i>=18;input.forward=game.player().pos.y>2.55f;game.update(input,1.f/120.f);
  if(game.player().pos.y<3.f&&game.player().pos.y>2.f&&game.player().grounded&&std::fabs(game.player().z-1.1f)<.01f){reachedCover=true;break;}
 }debug<<"duck-jump cover "<<game.player().pos.y<<' '<<game.player().z<<' '<<reachedCover<<'\n';if(!reachedCover)return false;
 std::ofstream("movement-test.txt")<<"Thin rail hull collision, crouched jump, airborne duck onto 1.1 m cover, momentum, 60/120 Hz consistency, six-step climb, crouch tunnel and blocked standing: PASS\n";return true;
}
bool Game::testProgression(){
 auto game=validationScene(Enemy::Kind::Huntsman,3);game.m_enemies.clear();game.m_player.pos={4.5f,7.9f};game.m_player.angle=kPi*.5f;
 InputState input{};input.forward=true;for(int i=0;i<60;++i)game.update(input,1.f/120.f);if(game.player().pos.y>8.18f)return false;
 input.use=true;game.update(input,.01f);input.use=false;for(int i=0;i<240;++i)game.update(input,1.f/120.f);if(game.player().pos.y<9.5f)return false;
 game=validationScene(Enemy::Kind::Huntsman,3);game.m_player.pos={3.f,5.5f};game.m_player.angle=kPi;input={};input.use=true;game.update(input,.02f);
 if(game.activeLog()!=0||game.logTime()<=0)return false;
 input.use=false;game.update(input,.01f);input.use=true;game.update(input,.01f);
 if(game.activeLog()!=-1||game.logTime()!=0)return false;
 game.m_player.angle=kPi*.75f;input.use=false;game.update(input,.01f);input.use=true;game.update(input,.01f);
 if(game.activeLog()!=-1||game.nearbyTerminal()!=-1)return false;
 // Both chunks exist before crossing; a locked transfer must not open under combat.
 game=Game{};game.m_player.pos={21.5f,20.5f};game.m_player.angle=kPi*.5f;input={};input.use=true;game.update(input,.01f);
 if(game.world().doors().back().opening||game.chunkView(1).enemies().size()!=9)return false;
 game=Game{};game.m_player.health=67;game.m_player.ammo=23;game.m_pickups.clear();
 std::ofstream diagnostic("route-diagnostic.txt");
 for(int level=0;level<2;++level){auto spawns=game.m_enemies;game.m_enemies.clear();World planning=game.m_world;
  for(int i=0;i<int(planning.doors().size());++i)planning.openDoor(i);planning.updateDoors(2);
  int parent[24*24];std::fill(std::begin(parent),std::end(parent),-1);
  int start=int(game.player().pos.y)*24+int(game.player().pos.x),goal=23*24+21;
  std::queue<int> pending;pending.push(start);parent[start]=start;
  while(!pending.empty()){int cell=pending.front();pending.pop();int x=cell%24,y=cell/24;const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
   for(int d=0;d<4;++d){int nx=x+dx[d],ny=y+dy[d];if(nx<0||ny<0||nx>=24||ny>=24)continue;int next=ny*24+nx;
    if(parent[next]<0&&planning.navigable(x,y,nx,ny)){parent[next]=cell;pending.push(next);}
   }
  }
  if(parent[goal]<0){diagnostic<<"No route in chunk "<<level;return false;}
  for(auto&e:spawns)if(parent[int(e.pos.y)*24+int(e.pos.x)]<0){diagnostic<<"Unreachable enemy "<<level;return false;}
  for(auto&p:game.pickups())if(parent[int(p.pos.y)*24+int(p.pos.x)]<0){diagnostic<<"Unreachable pickup "<<level;return false;}
  std::vector<Vec2> route;for(int cell=goal;cell!=start;cell=parent[cell])route.push_back({cell%24+.5f,cell/24+.5f});std::reverse(route.begin(),route.end());
  route.push_back({21.5f,24.25f});size_t waypoint=0;int ticks=0;
  for(;ticks<14000&&!game.won()&&game.level()==level&&waypoint<route.size();++ticks){auto delta=route[waypoint]-game.player().pos;
   if(length(delta)<.14f){++waypoint;continue;}input={};input.forward=true;
   int door=game.world().nearbyDoor(game.player().pos,normalized(delta));input.use=door>=0&&!game.world().doors()[door].opening;
   input.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game.player().angle)/(.0022f*game.settings().sensitivity);game.update(input,1.f/120.f);
  }
  diagnostic<<"Chunk "<<level<<" ticks "<<ticks<<" waypoint "<<waypoint<<"/"<<route.size()<<" position "<<game.player().pos.x<<", "<<game.player().pos.y<<" active "<<game.level()<<'\n';
  if(level==0){if(game.level()!=1||game.player().health!=67||game.player().ammo!=23||game.m_enemies.size()!=9)return false;
   // Walk back across the same seam: old doors, kills and pickups must remain changed.
   game.m_player.angle=-kPi*.5f;input={};input.forward=true;for(int i=0;i<70&&game.level()==1;++i)game.update(input,1.f/120.f);
   if(game.level()!=0||!game.m_enemies.empty()||!game.m_pickups.empty()||!game.world().doors().back().opening)return false;
   game.m_player.angle=kPi*.5f;for(int i=0;i<90&&game.level()==0;++i)game.update(input,1.f/120.f);if(game.level()!=1)return false;
  }else if(game.level()!=2)return false;
 }
 std::ofstream("progression-test.txt")<<"Three connected chunks; locked transfer; continuous boundary crossing and return; health/ammo/door/pickup persistence; first two maps traversed with player controls; arrival in Turbine Gantry: PASS\n";return true;
}
}
