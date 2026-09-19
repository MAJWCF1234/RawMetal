#include "Game.h"
#include <fstream>
namespace retro {
void Game::seedClutter(){
 m_heldClutter=-1;m_clutter.clear();
 const Vec2 positions[3][6]={{{2.8f,5.9f},{3.1f,6.1f},{11.3f,4.3f},{11.7f,4.6f},{18.1f,19.9f},{18.8f,20.2f}},{{6.8f,3.2f},{7.1f,3.4f},{9.2f,12.2f},{9.5f,12.6f},{18.7f,19.8f},{19.1f,20.f}},{{7.3f,4.6f},{7.7f,4.8f},{7.2f,18.7f},{7.6f,19.f},{18.6f,9.7f},{19.5f,9.5f}}};
 for(int i=0;i<6;++i){auto p=positions[m_level][i];Clutter c;c.pos=p;c.z=m_level==2&&i>=4?3:m_world.floorHeight(p.x,p.y);c.kind=i;c.yaw=i*.7f;m_clutter.push_back(c);}
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
  if(m_world.rayClear(m_player.pos,m_player.z+m_player.eye,target,height+c.height()*.5f)&&m_world.fits(target.x,target.y,height,c.height())){c.pos=target;c.z=height;c.velocity={};c.vz=0;c.yaw=m_player.angle;c.pitchSpeed=c.rollSpeed=c.spin=0;}
  else{m_heldClutter=-1;c.projectile=false;}
  if(holdingClutter()&&input.fire&&!m_previousFire&&!m_suppressFire){c.velocity=forward*(9.f*std::cos(pitch));c.vz=2.f+9.f*std::sin(pitch);c.spin=3;c.pitchSpeed=10;c.rollSpeed=4;c.sleeping=false;c.projectile=true;m_heldClutter=-1;sound(Sound::PunchSwing,.55f);}
 }
 int steps=std::max(1,int(std::ceil(dt*180)));float step=dt/steps;
 for(int index=0;index<int(m_clutter.size());++index){if(index==m_heldClutter)continue;auto&c=m_clutter[index];
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
   auto fits=[&](Vec2 p,float z){for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]})if(!m_world.fits(p.x+x,p.y+y,z,extent[2]*2)||m_world.doorBlocks(p.x+x,p.y+y,z,extent[2]*2))return false;return true;};
   float floor=m_world.supportBelow(c.pos.x,c.pos.y,std::max(c.z,center-oldExtent[2])+.025f);
   if(!fits(c.pos,std::max(c.z,floor))){c.pitch=oldPitch;c.roll=oldRoll;c.yaw=oldYaw;c.pitchSpeed*=-.2f;c.rollSpeed*=-.2f;c.spin*=-.2f;extent=oldExtent;c.z=center-extent[2];}
   c.vz-=14.f*step;auto next=c.pos+c.velocity*step;
   if(fits(next,std::max(c.z,floor))){c.pos=next;}
   else{impact(length(c.velocity));c.pitchSpeed+=std::clamp(c.velocity.x*1.5f,-8.f,8.f);c.rollSpeed-=std::clamp(c.velocity.y*1.5f,-8.f,8.f);c.velocity=c.velocity*-.25f;c.projectile=false;}
   floor=-100;float ceiling=100;
   for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]}){floor=std::max(floor,m_world.supportBelow(c.pos.x+x,c.pos.y+y,std::max(c.z,center-oldExtent[2])+.025f));ceiling=std::min(ceiling,m_world.clearanceAbove(c.pos.x+x,c.pos.y+y,c.z));}
   float z=c.z+c.vz*step;
   if(c.vz>0&&z+c.height()>ceiling){impact(c.vz);c.vz=-c.vz*.2f;}
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
