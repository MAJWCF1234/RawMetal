#include "Game.h"
#include <fstream>
namespace retro {
void Game::seedClutter(){
 m_heldClutter=-1;m_clutter.clear();
 for(const auto& spawn:m_world.clutterSpawns()){
  Clutter c;c.kind=spawn.kind;c.pos=spawn.position;c.yaw=spawn.yaw;
  c.z=spawn.z==-999?m_world.floorHeight(c.pos.x,c.pos.y):spawn.z;
  m_clutter.push_back(c);
 }
}
int Game::nearbyClutter()const{
 int best=-1;float distance=1.5f;Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(int i=0;i<int(m_clutter.size());++i){auto&c=m_clutter[i];auto delta=c.pos-m_player.pos;float d=length(delta);
  if(d<distance&&dot(delta,forward)>d*.5f&&std::fabs(c.z-m_player.z)<1.2f&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,c.pos,c.z+.15f)){best=i;distance=d;}}
 return best;
}
bool Game::interactClutter(){
 if(holdingClutter()){auto&c=m_clutter[m_heldClutter];c.velocity=m_velocity*.3f;c.pitchSpeed=1.2f;c.rollSpeed=.7f;c.sleeping=false;c.restTime=0;m_heldClutter=-1;return true;}
 m_heldClutter=nearbyClutter();if(holdingClutter()){auto&c=m_clutter[m_heldClutter];c.projectile=false;c.sleeping=false;c.restTime=0;sound(Sound::Pickup,.25f,.8f);return true;}return false;
}
void Game::updateClutter(const InputState&input,float dt){
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 if(holdingClutter()){
  auto&c=m_clutter[m_heldClutter];float pitch=m_player.pitch/140.f;Vec2 target=m_player.pos+forward*.9f;float height=m_player.z+m_player.eye-.3f+std::sin(pitch)*.7f;
  bool clear=true;for(int i=1;i<=12;++i){float t=float(i)/12;auto p=m_player.pos+(target-m_player.pos)*t;const auto&w=worldAt(p);float z=m_player.z+m_player.eye+(height+c.height()*.5f-m_player.z-m_player.eye)*t;clear&=w.fits(p.x,p.y,z,.02f)&&!w.doorBlocks(p.x,p.y,z,.02f);}
  auto local=target;const auto&targetWorld=worldAt(local);
  if(clear&&targetWorld.fits(local.x,local.y,height,c.height())){c.pos=target;c.z=height;c.velocity={};c.vz=0;c.yaw=m_player.angle;c.pitchSpeed=c.rollSpeed=c.spin=0;}
  else{m_heldClutter=-1;c.projectile=false;}
  if(holdingClutter()&&input.fire&&!m_previousFire&&!m_suppressFire){c.velocity=forward*(9.f*std::cos(pitch));c.vz=2.f+9.f*std::sin(pitch);c.spin=3;c.pitchSpeed=10;c.rollSpeed=4;c.sleeping=false;c.projectile=true;m_heldClutter=-1;sound(Sound::PunchSwing,.55f);}
 }
 int steps=std::max(1,int(std::ceil(dt*180)));float step=dt/steps;
 for(int index=0;index<int(m_clutter.size());++index){if(index==m_heldClutter)continue;auto&c=m_clutter[index];
  if(dormantEntity(c.z))continue;
  if(length(m_velocity)>.5f&&length(c.pos-m_player.pos)<.24f+c.footprint()*.5f&&c.z<m_player.z+1&&c.z+c.height()>m_player.z){c.velocity=m_velocity*.7f;c.pitchSpeed=5;c.rollSpeed=2;c.sleeping=false;c.restTime=0;}
  if(c.sleeping)continue;
  auto impact=[&](float speed){if(speed<.65f||c.impactCooldown>0)return;float gain=std::clamp(speed*.12f,.12f,.85f);if(c.impactSound()==Sound::JunkSoft)gain*=.65f;
   m_sounds.push_back({c.impactSound(),c.pos,gain,.94f+.1f*std::fabs(std::sin(c.yaw+index)),true});c.impactCooldown=.075f;
  };
  for(int i=0;i<steps;++i){
   c.impactCooldown=std::max(0.f,c.impactCooldown-step);
   auto oldExtent=c.extent();float center=c.z+oldExtent[2],oldPitch=c.pitch,oldRoll=c.roll,oldYaw=c.yaw;
   c.pitch=wrapAngle(c.pitch+c.pitchSpeed*step);c.roll=wrapAngle(c.roll+c.rollSpeed*step);c.yaw=wrapAngle(c.yaw+c.spin*step);
   auto extent=c.extent();c.z=center-extent[2];
   auto fits=[&](Vec2 p,float z){for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]}){auto local=p+Vec2{x,y};const auto&w=worldAt(local);if(!w.fits(local.x,local.y,z,extent[2]*2)||w.doorBlocks(local.x,local.y,z,extent[2]*2))return false;}return true;};
   auto local=c.pos;const auto&supportWorld=worldAt(local);
   float floor=supportWorld.supportBelow(local.x,local.y,std::max(c.z,center-oldExtent[2])+.025f);
   if(!fits(c.pos,std::max(c.z,floor))){c.pitch=oldPitch;c.roll=oldRoll;c.yaw=oldYaw;c.pitchSpeed*=-.2f;c.rollSpeed*=-.2f;c.spin*=-.2f;extent=oldExtent;c.z=center-extent[2];}
   c.vz-=14.f*step;
   float water=supportWorld.waterSurface(local.x,local.y);
   float immersed=std::clamp((water-c.z)/std::max(.01f,c.height()),0.f,1.f);
   if(immersed>0){float buoyancy=c.kind==3||c.kind==4?8.f:24.f;c.vz+=buoyancy*immersed*step;
    float drag=std::exp(-5.f*immersed*step);c.velocity=c.velocity*drag;c.vz*=drag;c.spin*=drag;c.pitchSpeed*=drag;c.rollSpeed*=drag;
   }
   // Wall impulses affect the normal component, not tangential momentum.
   for(int axis=0;axis<2;++axis){auto next=c.pos;float& speed=axis==0?c.velocity.x:c.velocity.y;
    if(axis==0)next.x+=speed*step;else next.y+=speed*step;
    if(fits(next,std::max(c.z,floor)))c.pos=next;
    else{impact(std::fabs(speed));if(axis==0)c.pitchSpeed+=std::clamp(speed*1.5f,-8.f,8.f);else c.rollSpeed-=std::clamp(speed*1.5f,-8.f,8.f);speed*=-.25f;c.projectile=false;}
   }
   floor=-100;float ceiling=100;
   for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]}){auto p=c.pos+Vec2{x,y};const auto&w=worldAt(p);floor=std::max(floor,w.supportBelow(p.x,p.y,std::max(c.z,center-oldExtent[2])+.025f));ceiling=std::min(ceiling,w.clearanceAbove(p.x,p.y,c.z));}
   float z=c.z+c.vz*step;
   if(c.vz>0&&z+c.height()>ceiling){impact(c.vz);c.z=std::max(floor,ceiling-c.height());c.vz=-c.vz*.2f;}
   else if(z<=floor+.001f){
    float speed=-c.vz;impact(speed);c.z=floor;float bounce=c.impactSound()==Sound::JunkSoft?.04f:.2f;c.vz=speed>1.f?speed*bounce:0;
    if(speed>1.5f&&length(c.velocity)>.1f){c.pitchSpeed+=c.velocity.x*.4f;c.rollSpeed-=c.velocity.y*.4f;}
    // Gravity lowers the centre of mass about the contact edge. Friction then
    // damps tumbling; it never snaps an object back to its original upright pose.
    auto s=c.size();float ix=(s[1]*s[1]+s[2]*s[2])/12,iy=(s[0]*s[0]+s[2]*s[2])/12;
    Clutter probe=c;probe.pitch+=.005f;float plus=probe.extent()[2];probe.pitch-=.01f;float minus=probe.extent()[2];
    c.pitchSpeed+=std::clamp(-14*(plus-minus)/(.01f*iy),-35.f,35.f)*step;
    probe=c;probe.roll+=.005f;plus=probe.extent()[2];probe.roll-=.01f;minus=probe.extent()[2];
    c.rollSpeed+=std::clamp(-14*(plus-minus)/(.01f*ix),-35.f,35.f)*step;
    c.velocity=c.velocity*std::exp(-7.f*step);float damping=std::exp(-8.f*step);c.pitchSpeed*=damping;c.rollSpeed*=damping;c.spin*=damping;
    if(length(c.velocity)<.5f)c.projectile=false;
    if(length(c.velocity)<.035f&&std::fabs(c.vz)<.05f&&std::fabs(c.pitchSpeed)+std::fabs(c.rollSpeed)+std::fabs(c.spin)<.45f)c.restTime+=step;else c.restTime=0;
    if(c.restTime>.35f){c.sleeping=true;c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;break;}
   }else{c.z=z;c.restTime=0;}
   if(c.projectile)for(auto&e:m_enemies)if(e.alive&&length(e.pos-c.pos)<.5f&&c.z+c.height()>e.bodyBottom()&&c.z<e.bodyTop()){
    impact(length(c.velocity));e.hp-=5;e.painFlash=1;e.awareness=6;e.lastKnown=m_player.pos;c.projectile=false;c.velocity=c.velocity*-.18f;m_sounds.push_back({Sound::PunchHit,c.pos,.6f,1,true});
    if(e.hp<=0){e.alive=false;e.deathTime=0;e.windup=0;++m_kills;enemySound(e,2);}break;
   }
  }
 }
}
bool Game::testClutter(){
 {auto g=mapInspection({12,23.5f},kPi*.5f,0,4,false,-9,true);g.m_clutter.clear();Clutter c;c.pos={12,23.8f};c.z=-8.5f;c.velocity={0,4};c.kind=2;g.m_clutter.push_back(c);
  for(int i=0;i<20;++i)g.updateClutter({},1.f/120);
  if(g.m_clutter.size()!=1||g.m_clutter[0].pos.y<=24)return false;
  float speed=g.m_clutter[0].velocity.y;g.m_player.pos.y=24.1f;g.crossChunkBoundary();
  if(g.m_clutter.size()!=1||g.m_clutter[0].pos.y<0||g.m_clutter[0].pos.y>2||g.m_clutter[0].velocity.y!=speed)return false;
  Game restored;if(!restored.decodeSave(g.encodeSave())||restored.m_clutter.size()!=1||restored.m_clutter[0].velocity.y!=speed)return false;
  g.m_player.pos={12,.3f};g.m_player.angle=-kPi*.5f;g.m_heldClutter=0;g.updateClutter({},.01f);
  if(!g.holdingClutter()||g.m_clutter[0].pos.y>=0)return false;
  g.m_player.pos.y=-.1f;g.crossChunkBoundary();if(g.level()!=4||!g.holdingClutter()||g.m_clutter.size()!=1)return false;
 }
 {auto g=mapInspection({9,9},0,0,5,false,-9.18f,true);g.m_clutter.clear();Clutter bottle;bottle.kind=2;bottle.pos={9,9};bottle.z=-9.17f;bottle.velocity={2,0};g.m_clutter.push_back(bottle);
  g.updateClutter({},.02f);if(g.m_clutter[0].velocity.x>=2||!std::isfinite(g.m_clutter[0].z))return false;
  if(g.world().waterSurface(12,9)>-100||g.world().waterSurface(9,9)<-10)return false;
 }
 {auto slide=validationScene(Enemy::Kind::Huntsman);slide.m_enemies.clear();slide.m_clutter={{{1.14f,4.5f},{-4,2},1.f}};
  slide.updateClutter({},.025f);auto& c=slide.m_clutter[0];
  if(c.velocity.x<=0||c.velocity.y<1.9f||c.pos.y<=4.5f)return false;
 }
 auto g=validationScene(Enemy::Kind::Huntsman);g.m_enemies[0].pos={5.2f,4.5f};g.m_clutter={{{4.2f,4.5f}}};
 if(!g.interactClutter()||!g.holdingClutter())return false;g.updateClutter({},.01f);InputState fire{};fire.fire=true;g.updateClutter(fire,.01f);
 for(int i=0;i<90;++i)g.updateClutter({},1.f/120);if(g.m_enemies[0].hp!=105||g.holdingClutter())return false;
 g.m_clutter={{{6.5f,2.5f},{},4.f}};g.m_world=World(2);g.m_level=2;
 for(int i=0;i<240;++i)g.updateClutter({},1.f/120);if(std::fabs(g.m_clutter[0].z-3)>.01f)return false;
 for(int kind=0;kind<6;++kind){g.m_clutter={{{6.5f,2.5f},{},4.f}};g.m_clutter[0].kind=kind;g.m_sounds.clear();
  for(int i=0;i<240;++i)g.updateClutter({},1.f/120);
  int impacts=0;for(auto&event:g.m_sounds)if(event.sound==g.m_clutter[0].impactSound()){if(!event.spatial||event.gain<=0||event.gain>.85f)return false;++impacts;}
  if(impacts<1||impacts>4)return false;g.m_sounds.clear();for(int i=0;i<120;++i)g.updateClutter({},1.f/120);if(!g.m_sounds.empty())return false;
 }
 for(float dt:{1.f/60,1.f/120}){auto bottle=clutterInspection(2,0);float high=bottle.m_clutter[0].height();
  for(int tick=0;tick<int(5/dt);++tick)bottle.updateClutter({},dt);auto&c=bottle.m_clutter[0];
  std::ofstream diagnostic("clutter-tumble-"+std::to_string(int(std::round(1/dt)))+".txt");diagnostic<<"pitch "<<c.pitch<<" roll "<<c.roll<<" height "<<c.height()<<" z "<<c.z<<" sleeping "<<c.sleeping;
  if(c.height()>.13f||c.height()>=high||std::fabs(c.z)>.005f||!c.sleeping)return false;
  auto extent=c.extent();auto size=c.size();for(float x:{-.5f,.5f})for(float y:{-.5f,.5f})for(float z:{-.5f,.5f})if(c.z+extent[2]+c.rotate(x*size[0],y*size[1],z*size[2])[2]<-.001f)return false;
 }
 std::ofstream("clutter-test.txt")<<"Bunker debris, rations, electronics, disks and glass bottles: lift/punt, exactly five damage once, gravity, upper-span landing, spatial impacts, silence at rest. Tilted bottles fall onto their sides at 60/120 Hz, sleep and stay above the floor: PASS\n";return true;
}
Game Game::clutterInspection(int kind,float seconds){
 auto game=mapInspection({5.7f,3.5f},.62f,-65,0,false,0,true);game.m_clutter={{{7.1f,4.5f},{},.65f}};auto&c=game.m_clutter[0];c.kind=kind;c.pitch=.65f;c.roll=.1f;
 for(int i=0;i<int(seconds*120);++i)game.updateClutter({},1.f/120);return game;
}
}
