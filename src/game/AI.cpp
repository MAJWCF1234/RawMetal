#include "Game.h"
#include <queue>
#include <cmath>
#include <fstream>
namespace retro {
static Vec2 stackedWaypoint(const World&w,Vec2 start,float startZ,Vec2 goal,float goalZ,float height){
 struct Node{Vec2 p;float z;int parent=-1;};std::vector<Node> nodes;std::array<std::vector<int>,24*24> cells;
 for(int y=1;y<23;++y)for(int x=1;x<23;++x)for(auto span:w.spansAt(x,y))if(span.ceiling-span.floor>=height&&(w.tile(x,y)!='#'||span.floor>=2.99f)){
  cells[y*24+x].push_back(int(nodes.size()));nodes.push_back({{x+.5f,y+.5f},span.floor});}
 int origin=-1,target=-1;float a=1e9f,b=1e9f;
 for(int i=0;i<int(nodes.size());++i){auto&n=nodes[i];float da=lengthSq(n.p-start)+std::fabs(n.z-startZ)*8,db=lengthSq(n.p-goal)+std::fabs(n.z-goalZ)*8;if(da<a){a=da;origin=i;}if(db<b){b=db;target=i;}}
 if(origin<0||target<0)return start;std::queue<int> queue;queue.push(origin);nodes[origin].parent=origin;
 while(!queue.empty()&&nodes[target].parent<0){int current=queue.front();queue.pop();auto n=nodes[current];int x=int(n.p.x),y=int(n.p.y);
  for(auto d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx<1||ny<1||nx>22||ny>22)continue;
   for(int next:cells[ny*24+nx]){if(nodes[next].parent>=0)continue;float feet=n.z;bool clear=true;
    for(int sample=1;sample<=8;++sample){Vec2 p=n.p+d*(sample/8.f);float ground=w.supportBelow(p.x,p.y,feet+.215f);
     if(std::fabs(ground-feet)>.24f||!w.fits(p.x,p.y,ground,height)||w.doorBlocks(p.x,p.y,ground,height)){clear=false;break;}feet=ground;}
    if(clear&&std::fabs(feet-nodes[next].z)<.03f){nodes[next].parent=current;queue.push(next);}
   }
  }
 }
 if(nodes[target].parent<0)return start;int next=target;while(nodes[next].parent!=origin&&next!=origin)next=nodes[next].parent;return nodes[next].p;
}
void Game::updateEnemies(float dt){
 if(dead()||m_won)return;
 bool footsteps=false,gunshot=false;for(auto&event:m_sounds){footsteps|=event.sound>=Sound::Metal1&&event.sound<=Sound::Concrete4;gunshot|=event.sound==Sound::Shot;}
 const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
 for(auto&e:m_enemies){
  float support=groundHeight(e.pos,e.z+.01f);
  if(e.z>support+.005f){e.verticalVelocity-=14.f*dt;e.z=std::max(support,e.z+e.verticalVelocity*dt);}else{e.z=support;e.verticalVelocity=0;}
  if(!e.alive)continue;e.repathTimer-=dt;e.moving=false;e.strike=std::max(0.f,e.strike-dt*4);e.attackCooldown=std::max(0.f,e.attackCooldown-dt);e.painFlash=std::max(0.f,e.painFlash-dt*5);
  auto to=m_player.pos-e.pos;float dist=length(to);Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
  bool visible=dist<11&&(dist<2.5f||dot(normalized(to),facing)>-.25f)&&m_world.rayClear(e.pos,e.z+.7f,m_player.pos,m_player.z+m_player.eye);
  float soundDistance=std::sqrt(dist*dist+(m_player.z-e.z)*(m_player.z-e.z));
  bool heard=(gunshot&&soundDistance<14)||(footsteps&&soundDistance<4.5f);
  if(visible){e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;e.awareness=6.f;e.state=Enemy::State::Chase;}
  else if(heard){e.lastKnown=m_player.pos;e.lastKnownZ=m_player.z;e.awareness=5.f;e.state=Enemy::State::Investigate;}
  else {e.awareness=std::max(0.f,e.awareness-dt);if(e.awareness==0)e.state=Enemy::State::Idle;else if(length(e.lastKnown-e.pos)<.6f)e.state=Enemy::State::Search;}
  e.voiceTimer-=dt;e.stepTimer-=dt;
  if(e.awareness>0&&e.voiceTimer<=0){enemySound(e,0,.65f);e.voiceTimer=6.f+float(int(e.home.x)%4);}
  bool warden=e.kind==Enemy::Kind::Warden;
  float range=warden?7.f:e.kind==Enemy::Kind::Brute?1.25f:1.05f;
  bool sameLevel=m_player.z<e.bodyTop()&&m_player.z+m_player.hullHeight()>e.bodyBottom();
  if(e.windup>0){if(!warden&&e.kind!=Enemy::Kind::Brute&&e.windup>.09f){e.heading+=wrapAngle(std::atan2(to.y,to.x)-e.heading)*std::min(1.f,dt*16.f);facing={std::cos(e.heading),std::sin(e.heading)};}e.windup-=dt;if(e.windup<=0){e.strike=1;e.attackCooldown=warden?2.4f:e.kind==Enemy::Kind::Wasp?.8f:e.kind==Enemy::Kind::Huntsman?1.f:1.6f;
    if(dist<range+.1f&&dot(normalized(to),facing)>(warden?.995f:.25f)&&sameLevel&&m_world.rayClear(e.pos,e.z+.6f,m_player.pos,m_player.z+.5f))receiveDamage(warden?16.f:e.kind==Enemy::Kind::Brute?18.f:9.f,e.pos);
   }continue;
  }
  // Warden commits its aim at the start of a long, audible charge. Cover
  // and lateral dodges remain effective through the entire attack.
  if(warden&&visible&&sameLevel&&dist<range&&e.attackCooldown<=0&&e.strike<=0){
   e.heading=std::atan2(to.y,to.x);e.windup=1.1f;enemySound(e,1,.9f);continue;
  }
  Vec2 goal=e.awareness>0?e.lastKnown:e.home;
  if(e.state==Enemy::State::Search){e.heading+=dt*1.4f;continue;}
  auto look=visible?to:goal-e.pos;
  if(lengthSq(look)>.01f)e.heading+=wrapAngle(std::atan2(look.y,look.x)-e.heading)*std::min(1.f,dt*(e.kind==Enemy::Kind::Brute?3.5f:14.f));
  if(!warden&&visible&&sameLevel&&dist<range&&e.attackCooldown<=0&&e.strike<=0){e.windup=e.kind==Enemy::Kind::Brute?.8f:.32f;enemySound(e,1,.85f);continue;}
  if(length(goal-e.pos)<.3f||(!warden&&visible&&dist<range*.82f)||e.painFlash>.65f)continue;
  Vec2 destination=goal;
  bool direct=std::fabs((visible?m_player.z:m_world.supportBelow(goal.x,goal.y,e.lastKnownZ+.02f))-e.z)<.22f&&m_world.rayClear(e.pos,e.z+.05f,goal,e.z+.05f);
  if(e.kind==Enemy::Kind::Huntsman&&visible&&m_world.tile(int(goal.x),int(goal.y))=='C')direct=m_world.rayClear(e.pos,e.z+.7f,goal,m_player.z+.3f);
  // Repeated contact switches to routed movement instead of rebuilding a
  // path every frame or continuing to push into the same corner forever.
  if(e.searchTime>.2f)direct=false;
  if(!direct&&e.repathTimer>0)destination=e.waypoint;
  else if(!direct&&m_level>=2){destination=stackedWaypoint(m_world,e.pos,e.z,goal,e.lastKnownZ,(e.kind==Enemy::Kind::Brute||warden)?1.85f:e.kind==Enemy::Kind::Wasp?1.6f:1.05f);e.waypoint=destination;e.repathTimer=.35f;}
  else if(!direct){
   int field[World::Height][World::Width];for(auto&row:field)for(auto&value:row)value=9999;
   int gx=int(goal.x),gy=int(goal.y);if(m_world.solid(gx+.5f,gy+.5f)){
    int originX=gx,originY=gy;float best=999;for(int yy=originY-1;yy<=originY+1;++yy)for(int xx=originX-1;xx<=originX+1;++xx)if(!m_world.solid(xx+.5f,yy+.5f)){float d=lengthSq(Vec2{xx+.5f,yy+.5f}-goal);if(d<best){best=d;gx=xx;gy=yy;}}
   }
   gx=std::clamp(gx,1,22);gy=std::clamp(gy,1,22);std::queue<std::pair<int,int>> queue;queue.push({gx,gy});field[gy][gx]=0;
   float hull=(e.kind==Enemy::Kind::Brute||warden)?1.85f:e.kind==Enemy::Kind::Wasp?1.6f:1.05f;
   while(!queue.empty()){auto[x,y]=queue.front();queue.pop();for(int i=0;i<4;++i){int nx=x+dx[i],ny=y+dy[i];if(nx<1||nx>22||ny<1||ny>22||field[ny][nx]!=9999||!m_world.navigable(x,y,nx,ny,hull))continue;field[ny][nx]=field[y][x]+1;queue.push({nx,ny});}}
   int x=int(e.pos.x),y=int(e.pos.y),best=field[y][x];destination=e.pos;
   for(int i=0;i<4;++i){int nx=x+dx[i],ny=y+dy[i];if(nx>0&&nx<23&&ny>0&&ny<23&&field[ny][nx]<best&&m_world.navigable(x,y,nx,ny,hull)){best=field[ny][nx];destination={nx+.5f,ny+.5f};}}
   e.waypoint=destination;e.repathTimer=.25f;
  }
  Vec2 direction=normalized(destination-e.pos);
  if(e.kind==Enemy::Kind::Wasp&&visible&&dist>1.8f&&dist<3.8f&&direct){float side=int(e.home.x)%2?1.f:-1.f;direction=normalized(direction*.5f+Vec2{-direction.y,direction.x}*side*.7f);}
  if(warden&&visible&&direct&&dist<5.f){float side=int(e.home.x)%2?1.f:-1.f;direction=normalized(direction*(dist<2.5f?-1.f:0.f)+Vec2{-direction.y,direction.x}*side);}
  Vec2 separation{};for(auto&other:m_enemies)if(&other!=&e&&other.alive&&other.bodyBottom()<e.bodyTop()&&other.bodyTop()>e.bodyBottom()){auto away=e.pos-other.pos;float d=length(away);if(d>.001f&&d<.85f)separation+=away*( (.85f-d)/d);}
  direction=normalized(direction+separation*2.f);
  float speed=e.kind==Enemy::Kind::Wasp?1.85f:e.kind==Enemy::Kind::Brute?.75f:1.4f;if(e.awareness==0)speed*=.5f;if(e.strike>0)speed*=.35f;
  auto old=e.pos;
  auto move=[&](Vec2 next){float step=e.kind==Enemy::Kind::Huntsman?.65f:.215f;float ground=groundHeight(next,e.z+step),height=(e.kind==Enemy::Kind::Brute||warden)?1.85f:e.kind==Enemy::Kind::Wasp?1.6f:1.05f;
   float feet=std::max(e.z,ground);if(ground-e.z<=step+.01f&&hullFits(next,feet,height)){e.pos=next;if(ground>e.z){e.z=ground;e.verticalVelocity=0;}}
  };
  move(e.pos+Vec2{direction.x*speed*dt,0});move(e.pos+Vec2{0,direction.y*speed*dt});
  float moved=length(e.pos-old);e.moving=moved>.0001f;e.gait+=moved*7;
  e.searchTime=e.moving?0.f:std::min(1.f,e.searchTime+dt);
  if(e.moving&&e.kind==Enemy::Kind::Brute&&e.stepTimer<=0){m_sounds.push_back({Sound::Land,e.pos,.7f,.68f,true});e.stepTimer=.8f;}
 }
}
bool Game::testAI(){
 std::ofstream debug("ai-diagnostic.txt");
 {auto alone=validationScene(Enemy::Kind::Huntsman),stacked=alone;
  Enemy upstairs=stacked.m_enemies[0];upstairs.pos.x+=.3f;upstairs.z=3;stacked.m_enemies.push_back(upstairs);
  alone.updateEnemies(.01f);stacked.updateEnemies(.01f);
  if(length(alone.m_enemies[0].pos-stacked.m_enemies[0].pos)>.00001f)return false;
 }
 // The ranged attack must hit a stationary exposed player, miss a lateral
 // dodge after aim-lock, and never damage through intervening architecture.
 for(int scenario=0;scenario<3;++scenario){auto g=validationScene(Enemy::Kind::Warden);auto& w=g.m_enemies[0];
  w.pos={7.5f,4.5f};w.home=w.pos;g.m_player.pos={5.f,4.5f};w.heading=kPi;
  if(scenario==2){w.pos={4.5f,9.5f};w.home=w.pos;g.m_player.pos={4.5f,6.5f};w.heading=-kPi*.5f;g.m_world.setDoor(0,1,true);}
  g.updateEnemies(.01f);if(w.windup<=0)return false;
  if(scenario==1)g.m_player.pos={5.f,5.5f};
  if(scenario==2)g.m_world.setDoor(0,0,false);
  for(int tick=0;tick<135;++tick)g.updateEnemies(1.f/120.f);
  debug<<"warden scenario "<<scenario<<" health "<<g.player().health<<'\n';
  if((scenario==0&&g.player().health>=100)||(scenario!=0&&g.player().health!=100))return false;
 }
 auto game=validationScene(Enemy::Kind::Huntsman);auto&e=game.m_enemies[0];e.pos={4.5f,9.5f};e.home=e.pos;e.lastKnown=e.pos;e.heading=-kPi*.5f;game.m_player.pos={4.5f,6.5f};auto original=e.pos;
 for(int i=0;i<120;++i)game.update({},1.f/120.f);debug<<"idle "<<int(e.state)<<' '<<length(e.pos-original)<<'\n';if(e.state!=Enemy::State::Idle||length(e.pos-original)>.05f)return false;
 InputState fire{};fire.fire=true;game.update(fire,.02f);debug<<"heard "<<int(e.state)<<'\n';if(e.state!=Enemy::State::Investigate)return false;
 game.m_world.openDoor(0);for(int i=0;i<600;++i)game.update({},1.f/120.f);debug<<"pursuit "<<e.pos.x<<' '<<e.pos.y<<' '<<int(e.state)<<'\n';if(e.pos.y>9.f)return false;
 auto dodge=validationScene(Enemy::Kind::Brute);auto&attacker=dodge.m_enemies[0];attacker.pos=dodge.m_player.pos+Vec2{.7f,0};attacker.heading=kPi;
 dodge.update({},.01f);if(attacker.windup<=0)return false;
 dodge.m_player.pos=attacker.pos+Vec2{.7f,0};
 for(int i=0;i<70;++i)dodge.update({},1.f/120.f);if(dodge.player().health!=100)return false;
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Wasp,Enemy::Kind::Brute}){auto stairs=validationScene(kind);auto&climber=stairs.m_enemies[0];
  climber.pos={20.5f,15.5f};climber.home=climber.pos;climber.lastKnown=climber.pos;climber.z=stairs.groundHeight(climber.pos);climber.heading=-kPi*.5f;
  stairs.m_player.pos={20.5f,11.5f};stairs.m_player.z=1.2f;
  for(int i=0;i<1200&&climber.pos.y>=13.f;++i)stairs.update({},1.f/120.f);
  debug<<"stairs "<<int(kind)<<' '<<climber.pos.y<<' '<<climber.z<<'\n';if(climber.pos.y>=13||std::fabs(climber.z-1.2f)>.01f)return false;
 }
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Wasp}){
  auto orbit=validationScene(kind);auto&bug=orbit.m_enemies[0];bug.pos={7.5f,4.5f};bug.home=bug.pos;
  for(int i=0;i<360;++i){float angle=i*4.f/120;orbit.m_player.pos=bug.pos+Vec2{std::cos(angle),std::sin(angle)}*.8f;orbit.updateEnemies(1.f/120);}
  debug<<"orbit "<<int(kind)<<" health "<<orbit.player().health<<'\n';if(orbit.player().health>=100)return false;
 }
 auto falling=validationScene(Enemy::Kind::Brute);auto&body=falling.m_enemies[0];body.pos={7.5f,4.5f};body.z=1.2f;body.alive=false;
 falling.updateEnemies(.05f);if(body.z<=0||body.z>=1.2f||body.verticalVelocity>=0)return false;
 for(int i=0;i<120;++i)falling.updateEnemies(1.f/120);if(body.z!=0)return false;
 auto crate=validationScene(Enemy::Kind::Huntsman);auto&crawler=crate.m_enemies[0];crawler.pos={5.7f,3.5f};crawler.home=crawler.pos;crawler.heading=kPi;crate.m_player.pos={3.5f,3.5f};crate.m_player.z=.6f;
 for(int i=0;i<240&&crawler.z<.59f;++i)crate.updateEnemies(1.f/120);
 debug<<"crate climb "<<crawler.z<<'\n';if(crawler.z<.59f)return false;
 std::ofstream("ai-test.txt")<<"Closed-door sight blocking, hearing/pursuit, all species climbing stairs, dodging brute windup, bugs tracking a circling target, falling bodies and huntsmen climbing crates: PASS\n";return true;
}
}
