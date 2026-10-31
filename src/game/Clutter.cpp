#include "Game.h"
#include <fstream>
#include "../world/ModelIds.h"
#include "../core/Ray.h"
#include "EngineRegressionScene.h"
#include "../world/CustomCampaign.h"
#include "../world/CampaignKit.h"
#include "ai/WalkQueryCache.h"
namespace retro {
PropTraits Game::propTraits(int id){
 if(id<0||id>=FacilityModelCount)return {PropMaterial::Plastic,1,0,{.1f,.1f,.1f}};
 const auto&s=model::propSize[id];float mass=model::mass[id];return {model::material[id]<0?PropMaterial::Plastic:PropMaterial(model::material[id]),mass>0?mass:std::clamp(140*s[0]*s[1]*s[2],.05f,30.f),model::health[id],{s[0],s[1],s[2]}};
}
PropMaterial Clutter::material()const{constexpr PropMaterial old[]={PropMaterial::Paper,PropMaterial::Cardboard,PropMaterial::Glass,PropMaterial::Electronic,PropMaterial::Electronic,PropMaterial::Plastic,PropMaterial::Wood};return model>=0?Game::propTraits(model).material:old[kind];}
float Clutter::mass()const{if(model<0)return 1;auto t=Game::propTraits(model);return model::mass[model]>0?t.mass:std::clamp(140*dims[0]*dims[1]*dims[2],.05f,30.f);}
void Game::knockClutter(int level,size_t index,Point3 at,Point3 impulse){
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;auto&c=list.at(index);if(c.broken)return;
 auto wake=[&](Clutter&v){v.sleeping=false;v.restTime=0;};wake(c);
 for(auto&v:list)if(&v!=&c&&!v.broken&&std::fabs(v.z-c.z-c.height())<.03f&&length(v.pos-c.pos)<(v.footprint()+c.footprint())*.7f)wake(v);
 float mass=c.mass();Point3 dv=impulse*(1/mass);float speed=std::sqrt(dv.x*dv.x+dv.y*dv.y+dv.z*dv.z);if(speed>14)dv=dv*(14/speed);c.velocity+=Vec2{dv.x,dv.y};c.vz+=dv.z;
 auto s=c.size();auto lever=at-Point3{c.pos.x,c.pos.y,c.z+c.height()*.5f};Point3 torque{lever.y*impulse.z-lever.z*impulse.y,lever.z*impulse.x-lever.x*impulse.z,lever.x*impulse.y-lever.y*impulse.x};float inertia=mass*(s[0]*s[0]+s[1]*s[1]+s[2]*s[2])/12;
 c.spin+=std::clamp(torque.z/std::max(.001f,inertia),-14.f,14.f);c.pitchSpeed+=std::clamp(torque.y/std::max(.001f,inertia),-14.f,14.f);c.rollSpeed+=std::clamp(torque.x/std::max(.001f,inertia),-14.f,14.f);
}
void Game::breakClutter(int level,size_t index,Point3 direction){
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;auto&c=list.at(index);if(c.model<0||c.broken)return;knockClutter(level,index,{c.pos.x,c.pos.y,c.z},{});c.broken=true;if(level==m_level&&int(index)==m_heldClutter)m_heldClutter=-1;c.sleeping=true;c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;
 m_breakEvents.push_back({c.pos,c.z,direction,m_elapsed,level,c.material(),c.footprint(),c.height()});if(m_breakEvents.size()>24)m_breakEvents.erase(m_breakEvents.begin());
 const auto&t=propMaterialTraits(c.material());auto shift=chunkOffset(level)-chunkOffset(m_level);SoundEvent sound{t.breakSound,c.pos+shift,1,t.breakPitch,true,17,c.z};sound.aiRadius=c.material()==PropMaterial::Ceramic?7:8;m_sounds.push_back(sound);if(c.breakState)setState(c.breakState,1);
 if(c.material()==PropMaterial::Organic||c.material()==PropMaterial::Glass)m_bulletImpacts.push_back({c.pos,c.z+.005f,{},m_elapsed,level,true,c.material()==PropMaterial::Glass?1:int(CreatureKind::Mutant),1});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
}
bool Game::damageClutter(int level,size_t index,float amount,Point3,Point3 direction){auto&c=(level==m_level?m_clutter:m_chunks[level].clutter).at(index);if(c.model<0||c.broken)return false;c.damage+=std::max(0.f,amount);float health=propTraits(c.model).health;if(health>0&&c.damage>=health){breakClutter(level,index,direction);return true;}return false;}
void Game::seedClutter(){
 m_heldClutter=-1;m_clutter.clear();
 for(const auto& spawn:m_world.clutterSpawns()){
  Clutter c;c.kind=spawn.kind;c.pos=spawn.position;c.yaw=spawn.yaw;
  c.model=spawn.model;c.dims={spawn.width,spawn.depth,spawn.height};c.pitch=spawn.pitch;c.roll=spawn.roll;c.breakState=spawn.breakState;
  c.z=spawn.z==-999?m_world.supportBelow(c.pos.x,c.pos.y,m_world.floorHeight(c.pos.x,c.pos.y)+.5f):spawn.z;
  if(c.model>=0){c.authored=true;c.home=c.pos+chunkOffset(m_level);c.homeZ=spawn.z;auto e=c.extent();float support=-100;
   for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]}){float z=m_world.supportBelow(c.pos.x+x,c.pos.y+y,c.z+.05f);for(const auto&other:m_clutter){auto oe=other.extent();if(!other.broken&&std::fabs(c.pos.x+x-other.pos.x)<=oe[0]&&std::fabs(c.pos.y+y-other.pos.y)<=oe[1]&&other.z+other.height()<=c.z+.03f)z=std::max(z,other.z+other.height());}support=std::max(support,z);}
   if(c.z-support>=-.01f&&c.z-support<=.03f){c.z=support;c.sleeping=true;}
  }
  m_clutter.push_back(c);
 }
}
float Game::clutterPathFraction(Point3 from,Point3 to,Point3&normal)const{
 float nearest=1;auto origin=chunkOffset(m_level);for(int level=0;level<chunkCount();++level){auto shift=origin-chunkOffset(level);Point3 offset{shift.x,shift.y,0},n;auto local=from+offset,end=to+offset;if(rayBoxFraction(local,end-local,{0,0,-1000},{24,24,1000})>=1)continue;float t=chunkResident(level)?(level==m_level?m_world:m_chunks[level].world).shotFraction(local,end,n):rayBoxFraction(local,end-local,{0,0,-1000},{24,24,1000},&n);if(t<nearest){nearest=t;normal=n;}}return nearest;
}
int Game::nearbyClutter(int grabHand)const{
 auto visible=[&](const Clutter&c){Point3 n;return clutterPathFraction({m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye},{c.pos.x,c.pos.y,c.z+c.height()*.5f},n)>=.999f;};
 if(m_vrInputActive){int best=-1;float reach=.6f*.6f;for(int i=0;i<int(m_clutter.size());++i)for(int hand=0;hand<2;++hand)if(m_vrHandValid[hand]&&(grabHand<0||grabHand==hand)){const auto&c=m_clutter[i];auto d=Point3{c.pos.x,c.pos.y,c.z+c.height()*.5f}-m_vrHandPosition[hand];float distance=d.x*d.x+d.y*d.y+d.z*d.z;if(!c.broken&&distance<reach&&visible(c)){best=i;reach=distance;}}return best;}
 int best=-1;float distance=1.5f,verticalReach=std::max(1.2f,m_player.hullHeight()+.3f);Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(int i=0;i<int(m_clutter.size());++i){auto&c=m_clutter[i];auto delta=c.pos-m_player.pos;float d=length(delta);
  if(!c.broken&&d<distance&&dot(delta,forward)>d*.5f&&std::fabs(c.z-m_player.z)<verticalReach&&visible(c)){best=i;distance=d;}}
 return best;
}
bool Game::interactClutter(int grabHand){
 if(holdingClutter()){auto&c=m_clutter[m_heldClutter];c.velocity=m_velocity*.1f;c.vz=0;if(m_vrInputActive&&m_vrHandValid[m_vrCarryHand]){auto v=m_vrHandVelocity[m_vrCarryHand];c.velocity={std::clamp(v.x,-12.f,12.f),std::clamp(v.y,-12.f,12.f)};c.vz=std::clamp(v.z,-12.f,12.f);}if(c.model>=0){float factor=std::clamp(std::sqrt(2.5f/c.mass()),.35f,1.f);c.velocity=c.velocity*factor;c.vz*=factor;}c.pitchSpeed=0;c.rollSpeed=0;c.sleeping=false;c.restTime=0;m_heldClutter=-1;return true;}
 m_heldClutter=nearbyClutter(grabHand);if(holdingClutter()&&m_clutter[m_heldClutter].mass()>25){m_heldClutter=-1;return false;}if(holdingClutter()){auto&c=m_clutter[m_heldClutter];if(m_vrInputActive){float nearest=999;for(int hand=0;hand<2;++hand)if(m_vrHandValid[hand]&&(grabHand<0||grabHand==hand)){auto d=Point3{c.pos.x,c.pos.y,c.z+c.height()*.5f}-m_vrHandPosition[hand];float distance=d.x*d.x+d.y*d.y+d.z*d.z;if(distance<nearest){nearest=distance;m_vrCarryHand=hand;}}}c.projectile=false;if(c.model>=0)knockClutter(m_level,m_heldClutter,{c.pos.x,c.pos.y,c.z},{});c.sleeping=false;c.restTime=0;sound(Sound::Pickup,.25f,.8f);return true;}return false;
}
void Game::updateClutter(const InputState&input,float dt){
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 if(holdingClutter()){
  auto&c=m_clutter[m_heldClutter];float pitch=m_player.pitch/140.f;
  // Aim at the object's centre from the current camera, including crouching.
  // The former .48 m feet-relative cap kept carried objects at knee height.
  float holdDistance=c.model>=0?std::max(.9f,.5f+c.footprint()*.5f):.9f;
  Vec2 target=m_player.pos+forward*(holdDistance*std::cos(pitch));
  float height=m_player.z+m_player.eye+holdDistance*std::sin(pitch)-c.height()*.5f;
  if(m_vrInputActive){auto hand=m_vrHandPosition[m_vrCarryHand],direction=m_vrHandDirection[m_vrCarryHand];target={hand.x+direction.x*.12f,hand.y+direction.y*.12f};height=hand.z+direction.z*.12f-c.height()*.5f;pitch=std::asin(std::clamp(direction.z,-1.f,1.f));float yaw=std::atan2(direction.y,direction.x);forward={std::cos(yaw),std::sin(yaw)};}
  auto local=target;const auto&targetWorld=worldAt(local);
  height=std::max(height,targetWorld.supportBelow(local.x,local.y,height+c.height()*.5f+.025f)+.012f);
  bool clear=!m_vrInputActive||m_vrHandValid[m_vrCarryHand];
  Point3 eye{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye},end{target.x,target.y,height+c.height()*.5f},n;
  float fraction=clutterPathFraction(eye,end,n);if(fraction<1){end=eye+(end-eye)*std::max(0.f,fraction-.02f);target={end.x,end.y};height=end.z-c.height()*.5f;}
  auto extent=c.extent();auto fit=[&](float t){auto p=m_player.pos+(target-m_player.pos)*t;float z=eye.z+(height-eye.z)*t;for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]}){auto q=p+Vec2{x,y};const auto&w=worldAt(q);if(!w.fits(q.x,q.y,z,c.height(),true,true)||w.doorBlocks(q.x,q.y,z,c.height()))return false;}return true;};
  if(c.model>=0&&!fit(1)){float lo=0,hi=1;for(int i=0;i<8;++i){float mid=(lo+hi)*.5f;if(fit(mid))lo=mid;else hi=mid;}target=m_player.pos+(target-m_player.pos)*lo;height=eye.z+(height-eye.z)*lo;clear&=lo>.1f;}
  if(clear&&targetWorld.fits(local.x,local.y,height,c.height(),true,true)){c.pos=target;c.z=height;c.velocity={};c.vz=0;c.yaw=m_vrInputActive?std::atan2(forward.y,forward.x):m_player.angle;c.pitchSpeed=c.rollSpeed=c.spin=0;}
  else{m_heldClutter=-1;c.projectile=false;}
  if(holdingClutter()&&(!input.vrTracked||m_vrCarryHand==1)&&input.fire&&!m_previousFire&&!m_suppressFire){float speed=9.f*(c.model>=0?std::clamp(std::sqrt(2.5f/c.mass()),.35f,1.f):1.f);c.velocity=forward*(speed*std::cos(pitch));c.vz=2.f+speed*std::sin(pitch);c.spin=3;c.pitchSpeed=10;c.rollSpeed=4;c.sleeping=false;c.projectile=true;m_heldClutter=-1;sound(Sound::PunchSwing,.55f);}
 }
 m_propHalfStep=!m_propHalfStep;simulateClutter(m_clutter,m_level,dt,m_heldClutter);
 for(int level=0;level<chunkCount();++level)if(level!=m_level&&chunkResident(level))simulateClutter(m_chunks[level].clutter,level,dt,-1);
}
void Game::simulateClutter(std::vector<Clutter>&list,int level,float dt,int held){
 auto shift=chunkOffset(level)-chunkOffset(m_level);
 auto physicalWorld=[&](Vec2& p)->const World&{auto local=p-shift;if(local.x>=0&&local.x<24&&local.y>=0&&local.y<24){p=local;return level==m_level?m_world:m_chunks[level].world;}return worldAt(p);};
 std::vector<std::array<float,3>> shapes;std::vector<Vec2> bases;shapes.reserve(list.size());bases.reserve(list.size());for(const auto&v:list){shapes.push_back(v.extent());bases.push_back({std::cos(v.yaw),std::sin(v.yaw)});}
 // One broad phase per resident list. Query neighbouring cells as movers cross a bucket.
 std::array<std::vector<int>,24*24> buckets;
 if(list.size()>32)for(int i=0;i<int(list.size());++i){const auto&v=list[i];if(v.model<0||v.broken)continue;auto e=shapes[i];for(int y=std::max(0,int(v.pos.y-e[1]));y<=std::min(23,int(v.pos.y+e[1]));++y)for(int x=std::max(0,int(v.pos.x-e[0]));x<=std::min(23,int(v.pos.x+e[0]));++x)buckets[y*24+x].push_back(i);}
 auto support=[&](int self,Vec2 p,float feet){float top=-100;auto consider=[&](int j){if(j==self||j==held)return;const auto&v=list[j];if(v.broken||v.model<0||v.z+shapes[j][2]*2>feet+.025f)return;auto d=p-v.pos;auto e=shapes[j];if(std::fabs(v.pitch)<.05f&&std::fabs(v.roll)<.05f){float c=bases[j].x,s=bases[j].y;auto size=v.size();if(std::fabs(d.x*c+d.y*s)>size[0]*.5f||std::fabs(-d.x*s+d.y*c)>size[1]*.5f)return;}else if(std::fabs(d.x)>e[0]||std::fabs(d.y)>e[1])return;top=std::max(top,v.z+shapes[j][2]*2);};
  if(list.size()>32){for(int y=std::max(0,int(p.y)-1);y<=std::min(23,int(p.y)+1);++y)for(int x=std::max(0,int(p.x)-1);x<=std::min(23,int(p.x)+1);++x)for(int j:buckets[y*24+x])consider(j);}else for(int j=0;j<int(list.size());++j)consider(j);return top;};
 struct RankedProp{float distance;int level,index;};std::vector<RankedProp> nearest;for(int l=0;l<chunkCount();++l)if(chunkResident(l)){auto offset=chunkOffset(l)-chunkOffset(m_level);const auto&items=chunkClutter(l);for(int i=0;i<int(items.size());++i){const auto&v=items[i];if(v.model>=0&&!v.broken&&!v.sleeping&&!dormantEntity(v.z)&&!(l==m_level&&i==m_heldClutter))nearest.push_back({lengthSq(v.pos+offset-m_player.pos),l,i});}}std::sort(nearest.begin(),nearest.end(),[](const RankedProp&a,const RankedProp&b){if(a.distance!=b.distance)return a.distance<b.distance;return a.level!=b.level?a.level<b.level:a.index<b.index;});std::vector<bool> slow(list.size());for(size_t rank=48;rank<nearest.size();++rank)if(nearest[rank].level==level)slow[nearest[rank].index]=true;
 for(int index=0;index<int(list.size());++index){if(index==held)continue;auto&c=list[index];
  if(c.broken||dormantEntity(c.z))continue;
  if(c.model>=0){auto e=c.extent();auto delta=c.pos-(m_player.pos-shift);float nx=e[0]+.2f-std::fabs(delta.x),ny=e[1]+.2f-std::fabs(delta.y);
   if(nx>0&&ny>0&&c.z<m_player.z+m_player.hullHeight()&&c.z+c.height()>m_player.z){Vec2 n=nx<ny?Vec2{delta.x<0?-1.f:1.f,0}:Vec2{0,delta.y<0?-1.f:1.f};auto next=c.pos+n*(std::min(nx,ny)+.001f);auto fit=[&](Vec2 p){for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]}){auto q=p+Vec2{x,y}+shift;const auto&w=physicalWorld(q);if(!w.fits(q.x,q.y,c.z,c.height(),true,true)||w.doorBlocks(q.x,q.y,c.z,c.height()))return false;}return true;};if(fit(next))c.pos=next;else{Vec2 side=n.x!=0?Vec2{0,delta.y<0?-1.f:1.f}:Vec2{delta.x<0?-1.f:1.f,0};next=c.pos+side*((n.x!=0?ny:nx)+.001f);if(fit(next)){c.pos=next;n=side;}}float speed=dot(m_velocity,n)*std::min(1.f,4/c.mass())+.2f;c.velocity+=n*std::max(0.f,speed-dot(c.velocity,n));c.sleeping=false;c.restTime=0;}
  }else if(length(m_velocity)>.5f&&length(c.pos-(m_player.pos-shift))<.24f+c.footprint()*.5f&&c.z<m_player.z+1&&c.z+c.height()>m_player.z){c.velocity=m_velocity*.7f;c.pitchSpeed=5;c.rollSpeed=2;c.sleeping=false;c.restTime=0;}
  if(c.model>=0){auto probe=c.pos+shift;const auto&under=physicalWorld(probe);if(probe.x<0||probe.x>=24||probe.y<0||probe.y>=24||!chunkResident(under.level())){c.sleeping=true;c.restTime=-1;continue;}if(c.restTime<0){c.sleeping=false;c.restTime=0;}}
  if(c.sleeping)continue;
  if(slow[index]&&m_propHalfStep)continue;float itemDt=dt*(slow[index]?2:1);int steps=std::max(1,int(std::ceil(itemDt*(slow[index]?90:180))));float step=itemDt/steps;
  auto impact=[&](float speed){if(speed<.65f||c.impactCooldown>0)return;float gain=std::clamp(speed*.12f,.12f,.85f);if(c.model<0&&c.impactSound()==Sound::JunkSoft)gain*=.65f;
   const auto&t=propMaterialTraits(c.material());SoundEvent event{c.impactSound(),c.pos+shift,gain,c.model>=0?t.impactPitch:.94f+.1f*std::fabs(std::sin(c.yaw+index)),true,17,c.z+c.height()*.5f};if(c.model>=0){event.gain*=t.impactGain;event.aiRadius=std::clamp(2.f+std::sqrt(c.mass())*speed,2.f,12.f);if(t.breakSpeed>0&&speed>t.breakSpeed)breakClutter(level,index,{c.velocity.x,c.velocity.y,c.vz});}m_sounds.push_back(event);c.impactCooldown=.075f;
  };
  for(int i=0;i<steps;++i){
   if(c.model>=0&&(lengthSq(c.velocity)>.0001f||std::fabs(c.vz)>.05f))for(auto&v:list)if(&v!=&c&&v.model>=0&&!v.broken&&v.sleeping&&std::fabs(v.z-c.z-c.height())<.03f&&length(v.pos-c.pos)<(v.footprint()+c.footprint())*.7f){v.sleeping=false;v.restTime=0;}
   c.impactCooldown=std::max(0.f,c.impactCooldown-step);
   auto oldExtent=c.extent();float center=c.z+oldExtent[2],oldPitch=c.pitch,oldRoll=c.roll,oldYaw=c.yaw;
   c.pitch=wrapAngle(c.pitch+c.pitchSpeed*step);c.roll=wrapAngle(c.roll+c.rollSpeed*step);c.yaw=wrapAngle(c.yaw+c.spin*step);
   auto extent=c.extent();c.z=center-extent[2];
   auto fits=[&](Vec2 p,float z){for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]}){auto local=p+Vec2{x,y}+shift;const auto&w=physicalWorld(local);if(!w.fits(local.x,local.y,z,extent[2]*2,true,true)||w.doorBlocks(local.x,local.y,z,extent[2]*2))return false;}return true;};
   auto local=c.pos+shift;const auto&supportWorld=physicalWorld(local);
   float floor=supportWorld.supportBelow(local.x,local.y,std::max(c.z,center-oldExtent[2])+.025f);
   if((c.model<0||c.pitch!=oldPitch||c.roll!=oldRoll||c.yaw!=oldYaw)&&!fits(c.pos,std::max(c.z,floor))){c.pitch=oldPitch;c.roll=oldRoll;c.yaw=oldYaw;c.pitchSpeed*=-.2f;c.rollSpeed*=-.2f;c.spin*=-.2f;extent=oldExtent;c.z=center-extent[2];}
   c.vz-=14.f*step;
   float water=supportWorld.waterSurface(local.x,local.y);
   float immersed=std::clamp((water-c.z)/std::max(.01f,(extent[2]*2)),0.f,1.f);
   const bool buoyant=c.model>=0?propMaterialTraits(c.material()).floats:c.kind!=3&&c.kind!=4;
   if(immersed>0){
    if(buoyant){float floatHeight=water-(extent[2]*2)*.5f;
     c.vz+=(14.f+std::clamp((floatHeight-c.z)*80.f,-12.f,22.f))*step;
    }else c.vz+=8.f*immersed*step;
    float drag=std::exp(-5.f*immersed*step);c.velocity=c.velocity*drag;c.vz*=drag;c.spin*=drag;c.pitchSpeed*=drag;c.rollSpeed*=drag;
   }
   if(c.model>=0){auto delta=Point3{c.velocity.x*step,c.velocity.y*step,c.vz*step};float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);if(distance>.02f){auto q=c.pos+shift;const auto&readWorld=physicalWorld(q);int targetLevel=readWorld.level();auto&w=targetLevel==m_level?m_world:m_chunks[targetLevel].world;Point3 from{q.x,q.y,c.z+extent[2]},n;ShotTrace trace;auto travel=delta*(1+std::max({extent[0],extent[1],extent[2]})/distance);float t=w.shotFraction(from,from+travel,n,&trace);
    for(int j=0;j<2;++j)if(trace.passed[j]>=0&&trace.passedAt[j]<t){float energy=.75f*c.mass()*distance*distance/(step*step);if(damagePane(targetLevel,trace.passed[j],energy,from+travel*trace.passedAt[j],delta*(1/distance))){c.velocity=c.velocity*.7f;c.vz*=.7f;}else{t=trace.passedAt[j];n=trace.passedNormal[j];break;}}
    if(t<1){if(trace.blocking>=0)damagePane(targetLevel,trace.blocking,.75f*c.mass()*distance*distance/(step*step),from+travel*t,delta*(1/distance));float closing=c.velocity.x*n.x+c.velocity.y*n.y+c.vz*n.z;impact(std::fabs(closing));if(closing<0){c.velocity=c.velocity-Vec2{n.x,n.y}*(closing*1.2f);c.vz-=n.z*closing*1.2f;}}
   }}
   if(c.broken)break;
   // Wall impulses affect the normal component, not tangential momentum.
   for(int axis=0;axis<2;++axis){auto next=c.pos;float& speed=axis==0?c.velocity.x:c.velocity.y;
    if(c.model>=0&&std::fabs(speed)<.00001f)continue;if(axis==0)next.x+=speed*step;else next.y+=speed*step;
    if(fits(next,std::max(c.z,floor)))c.pos=next;
    else{impact(std::fabs(speed));if(axis==0)c.pitchSpeed+=std::clamp(speed*1.5f,-8.f,8.f);else c.rollSpeed-=std::clamp(speed*1.5f,-8.f,8.f);speed*=-(c.model>=0?propMaterialTraits(c.material()).restitution:.25f);c.projectile=false;}
   }
   floor=-100;float ceiling=100;
   for(float x:{-extent[0],0.f,extent[0]})for(float y:{-extent[1],0.f,extent[1]}){auto sample=c.pos+Vec2{x,y};auto p=sample+shift;const auto&w=physicalWorld(p);floor=std::max(floor,w.supportBelow(p.x,p.y,std::max(c.z,center-oldExtent[2])+.025f));if(c.model>=0)floor=std::max(floor,support(index,sample,std::max(c.z,center-oldExtent[2])));ceiling=std::min(ceiling,w.clearanceAbove(p.x,p.y,c.z));}
   float z=c.z+c.vz*step;
   if(c.vz>0&&z+(extent[2]*2)>ceiling){c.z=std::max(floor,ceiling-(extent[2]*2));impact(c.vz);c.vz=-c.vz*.2f;}
   else if(z<=floor+.001f){
    float speed=-c.vz;c.z=floor;impact(speed);float bounce=c.model>=0?propMaterialTraits(c.material()).restitution:c.impactSound()==Sound::JunkSoft?.04f:.2f;c.vz=speed>1.f?speed*bounce:0;
    if(speed>1.5f&&length(c.velocity)>.1f){c.pitchSpeed+=c.velocity.x*.4f;c.rollSpeed-=c.velocity.y*.4f;}
    // Gravity lowers the centre of mass about the contact edge. Friction then
    // damps tumbling; it never snaps an object back to its original upright pose.
    auto s=c.size();float ix=(s[1]*s[1]+s[2]*s[2])/12,iy=(s[0]*s[0]+s[2]*s[2])/12;
    Clutter probe=c;probe.pitch+=.005f;float plus=probe.extent()[2];probe.pitch-=.01f;float minus=probe.extent()[2];
    c.pitchSpeed+=std::clamp(-14*(plus-minus)/(.01f*iy),-35.f,35.f)*step;
    probe=c;probe.roll+=.005f;plus=probe.extent()[2];probe.roll-=.01f;minus=probe.extent()[2];
    c.rollSpeed+=std::clamp(-14*(plus-minus)/(.01f*ix),-35.f,35.f)*step;
    c.velocity=c.velocity*std::exp(-(c.model>=0?propMaterialTraits(c.material()).friction:7.f)*step);float damping=std::exp(-8.f*step);c.pitchSpeed*=damping;c.rollSpeed*=damping;c.spin*=damping;
    if(length(c.velocity)<.5f)c.projectile=false;
    if(!(immersed>0&&buoyant)&&length(c.velocity)<.035f&&std::fabs(c.vz)<.05f&&std::fabs(c.pitchSpeed)+std::fabs(c.rollSpeed)+std::fabs(c.spin)<.45f)c.restTime+=step;else c.restTime=0;
    if(c.restTime>.35f){c.sleeping=true;c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;break;}
   }else{c.z=z;c.restTime=0;}
   if(c.projectile){auto point=c.pos+shift;const auto&contactWorld=physicalWorld(point);int targetLevel=contactWorld.level();auto&enemies=targetLevel==m_level?m_enemies:m_chunks[targetLevel].enemies;
    for(auto&e:enemies)if(e.alive&&length(e.pos-point)<.5f&&c.z+(extent[2]*2)>e.bodyBottom()&&c.z<e.bodyTop()){
     impact(length(c.velocity));e.awareness=6;e.lastKnown=point;e.lastKnownZ=contactWorld.supportBelow(point.x,point.y,c.z+extent[2]);e.state=Enemy::State::Investigate;c.projectile=false;c.velocity=c.velocity*-.18f;m_sounds.push_back({Sound::PunchHit,c.pos+shift,.6f,1,true,17,c.z+extent[2]});
     if(applyEnemyDamage(e,kUnarmedMelee.clutterDamage,false,false,targetLevel)){auto audible=e;audible.pos+=chunkOffset(targetLevel)-chunkOffset(m_level);enemySound(audible,2);}break;
    }
   }
  }
  if(c.broken){c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=true;c.restTime=0;}
  shapes[index]=c.extent();bases[index]={std::cos(c.yaw),std::sin(c.yaw)};
 }
 for(size_t i=0;i<list.size();++i)shapes[i]=list[i].extent();std::vector<int> seen(list.size(),-1),candidates;
 for(int i=0;i<int(list.size());++i){auto&a=list[i];if(a.model<0||a.broken||i==held)continue;
  candidates.clear();auto ae=shapes[i];if(list.size()>32){for(int y=std::max(0,int(a.pos.y-ae[1])-1);y<=std::min(23,int(a.pos.y+ae[1])+1);++y)for(int x=std::max(0,int(a.pos.x-ae[0])-1);x<=std::min(23,int(a.pos.x+ae[0])+1);++x)for(int j:buckets[y*24+x])if(j>i&&seen[j]!=i){seen[j]=i;candidates.push_back(j);}}else for(int j=i+1;j<int(list.size());++j)candidates.push_back(j);
  for(int j:candidates){auto&b=list[j];if(b.model<0||b.broken||j==held||(a.sleeping&&b.sleeping)||std::min(a.z+shapes[i][2]*2,b.z+shapes[j][2]*2)-std::max(a.z,b.z)<=.02f)continue;
   auto be=shapes[j];Vec2 d=b.pos-a.pos;if(std::fabs(d.x)>ae[0]+be[0]||std::fabs(d.y)>ae[1]+be[1])continue;
   bool flat=std::fabs(a.pitch)+std::fabs(a.roll)+std::fabs(b.pitch)+std::fabs(b.roll)<.1f;float penetration=100;Vec2 normal{};bool overlap=true;
   Vec2 ax{std::cos(a.yaw),std::sin(a.yaw)},ay{-ax.y,ax.x},bx{std::cos(b.yaw),std::sin(b.yaw)},by{-bx.y,bx.x};
   for(Vec2 n:flat?std::array<Vec2,4>{ax,ay,bx,by}:std::array<Vec2,4>{Vec2{1,0},Vec2{0,1},Vec2{1,0},Vec2{0,1}}){float ra=flat?(std::fabs(dot(ax,n))*a.dims[0]+std::fabs(dot(ay,n))*a.dims[1])*.5f:std::fabs(n.x)*ae[0]+std::fabs(n.y)*ae[1];float rb=flat?(std::fabs(dot(bx,n))*b.dims[0]+std::fabs(dot(by,n))*b.dims[1])*.5f:std::fabs(n.x)*be[0]+std::fabs(n.y)*be[1];float p=ra+rb-std::fabs(dot(d,n));if(p<=0){overlap=false;break;}if(p<penetration){penetration=p;normal=n*(dot(d,n)<0?-1.f:1.f);}}
   if(!overlap)continue;float ia=a.sleeping&&a.mass()>4*b.mass()?0:1/a.mass(),ib=b.sleeping&&b.mass()>4*a.mass()?0:1/b.mass(),total=ia+ib;if(total==0)continue;
   auto move=[&](Clutter&v,Vec2 delta){auto e=v.extent();for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]}){auto p=v.pos+delta+Vec2{x,y}+shift;const auto&w=physicalWorld(p);if(!w.fits(p.x,p.y,v.z,e[2]*2,true,true)||w.doorBlocks(p.x,p.y,v.z,e[2]*2))return;}auto p=v.pos+delta+shift;const auto&w=physicalWorld(p);if(w.railBlocksHull(p.x,p.y,std::max(e[0],e[1]),v.z,e[2]*2))return;v.pos+=delta;if(lengthSq(delta)>.000001f){v.sleeping=false;v.restTime=0;}};
   move(a,normal*(-penetration*ia/total));move(b,normal*(penetration*ib/total));float closing=dot(a.velocity-b.velocity,normal);if(closing>.65f&&a.impactCooldown<=0){const auto&t=propMaterialTraits(a.material());SoundEvent event{t.impact,a.pos+shift,std::clamp(closing*.12f,.12f,.85f)*t.impactGain,t.impactPitch,true,17,a.z+shapes[i][2]};event.aiRadius=std::clamp(2+std::sqrt(a.mass())*closing,2.f,12.f);m_sounds.push_back(event);a.impactCooldown=.075f;}
   if(closing>0){float impulse=closing*(1+std::min(propMaterialTraits(a.material()).restitution,propMaterialTraits(b.material()).restitution))/total;a.velocity=a.velocity-normal*(impulse*ia);b.velocity+=normal*(impulse*ib);if(impulse*ia>.05f){a.sleeping=false;a.restTime=0;}if(impulse*ib>.05f){b.sleeping=false;b.restTime=0;}
    float sa=propMaterialTraits(a.material()).breakSpeed,sb=propMaterialTraits(b.material()).breakSpeed;if(sa>0&&closing>sa)breakClutter(level,i,{normal.x,normal.y,0});if(sb>0&&closing>sb)breakClutter(level,j,{-normal.x,-normal.y,0});if(a.broken)break;
   }
  }
 }

}
bool Game::testProps(){
 std::ofstream report("props-test.txt");bool ok=true;auto check=[&](bool pass,const char*label){report<<(pass?"PASS ":"FAIL ")<<label<<'\n';ok&=pass;};
 auto map=regression::room();map->fixtures.push_back({24,{8,8},0,1.4f,.7f,.75f,0,true});kit::deskSet(*map,{8,8},.75f,0,1);auto campaign=regression::campaign(map);Game g(campaign);
 check(g.m_clutter.size()==4&&std::all_of(g.m_clutter.begin(),g.m_clutter.end(),[](const Clutter&c){return c.sleeping&&std::fabs(c.z-.75f)<.01f;}),"desk set seeds asleep on desk");
 g.m_clutter[3].breakState=stateId("mug_broken");Point3 origin{8.42f,6, .8f},direction{0,1,0};auto hit=g.traceShot(origin,direction,24);g.resolveShot(hit,origin,direction,weaponDef(WeaponId::Shotgun).hitscan,true);
 check(g.m_clutter[3].broken&&!g.m_breakEvents.empty()&&g.state(stateId("mug_broken"))==1,"shot breaks mug with tombstone, effect and state");
 check(!g.m_sounds.empty()&&std::any_of(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::BottleBreak&&e.aiRadius>0;}),"bottle break carries explicit hearing radius");
 check(!g.m_bulletImpacts.empty(),"light prop passes shot to wall");auto decalCount=g.m_bulletImpacts.size();
 origin={7.9f,6, .95f};hit=g.traceShot(origin,direction,10);check(hit.kind==ShotKind::Prop&&hit.index==0,"monitor stops shot");g.resolveShot(hit,origin,direction,weaponDef(WeaponId::Shotgun).hitscan,true);check(length(g.m_clutter[0].velocity)>0&&length(g.m_clutter[0].velocity)<1&&g.m_bulletImpacts.size()==decalCount,"monitor shot impulse respects mass and produces no wall decal");
 auto save=g.encodeSave();Game restored(campaign);check(restored.decodeSave(save)&&restored.encodeSave()==save,"v17 byte-exact save roundtrip");check(restored.m_clutter.size()==4&&restored.m_clutter[3].broken,"tombstone survives reconciliation");
 for(float dt:{1.f/60,1.f/120}){Game falling(campaign);falling.m_player.pos={3,3};auto&c=falling.m_clutter[0];c.pos={12,12};c.z=2;c.sleeping=false;for(int i=0;i<int(6/dt);++i)falling.updateClutter({},dt);check(c.sleeping&&c.z>=-.001f&&c.z<.02f,"model prop falls and sleeps above floor at 60/120 Hz");}
 Clutter heavy;heavy.kind=7;heavy.model=model::prop_cardboard_box;heavy.dims={.5f,.5f,.5f};check(propTraits(model::crt_monitor).mass==11,"curated monitor mass");
 bool supports=true,noOverlap=true;for(size_t i=0;i<g.m_clutter.size();++i){const auto&c=g.m_clutter[i];auto e=c.extent();for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]})supports&=std::fabs(g.world().supportBelow(c.home.x+x,c.home.y+y,.8f)-.75f)<.01f;for(size_t j=i+1;j<g.m_clutter.size();++j){const auto&b=g.m_clutter[j];auto be=b.extent();noOverlap&=std::fabs(c.home.x-b.home.x)>=e[0]+be[0]||std::fabs(c.home.y-b.home.y)>=e[1]+be[1];}}check(supports&&noOverlap,"all nine desk supports hit and authored desk items do not overlap");
 {auto m=regression::room();m->definition.playerStart={3,8};m->structures.push_back({4.9f,1,5.1f,23,0,4});m->actorTracks.push_back(regression::guard({11,8},{11,8}));Game hearing(regression::campaign(m));Clutter c;c.kind=7;c.model=model::mug;c.dims=propTraits(c.model).size;c.pos={8,8};c.z=1;c.vz=-3;hearing.m_clutter={c};hearing.m_sounds.clear();for(int i=0;i<120&&hearing.m_sounds.empty();++i)hearing.updateClutter({},1.f/120);for(int i=0;i<30;++i)hearing.updateFriendlyActors(.02f,true);Vec2 remembered{hearing.state(hearing.friendlyKey(0,"guard_x"))*.001f,hearing.state(hearing.friendlyKey(0,"guard_y"))*.001f};check(hearing.guardState(0)==GuardState::Investigate&&length(remembered-Vec2{8,8})<.1f&&length(remembered-hearing.m_player.pos)>3,"wall-hidden mug landing draws a guard to the impact, not the player");}
 for(bool wall:{false,true}){auto m=regression::room();m->definition.playerStart=wall?Vec2{22.1f,8}:Vec2{6,8};Game contact(regression::campaign(m));contact.m_player.angle=0;Clutter c;c.kind=7;c.model=model::med_specimen_jar;c.dims=propTraits(c.model).size;c.pos=wall?Vec2{22.7f,8}:Vec2{6.6f,8};c.sleeping=true;contact.m_clutter={c};InputState walk;walk.forward=true;for(int i=0;i<100;++i){contact.updateMovement(walk,1.f/120);contact.updateClutter({},1.f/120);}const auto&v=contact.m_clutter[0];auto e=v.extent();float dx=std::max(std::fabs(v.pos.x-contact.m_player.pos.x)-e[0],0.f),dy=std::max(std::fabs(v.pos.y-contact.m_player.pos.y)-e[1],0.f);bool clear=dx*dx+dy*dy>=.19f*.19f;for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]})clear&=contact.world().fits(v.pos.x+x,v.pos.y+y,v.z+.01f,std::max(.001f,v.height()-.02f),true,true);check(clear&&!v.broken,wall?"player pushes bottle aside at wall without embedding it":"player contact pushes floor bottle clear of the hull");}
 {Game impact(regression::campaign(regression::room()));Clutter c;c.kind=7;c.model=model::mug;c.dims=propTraits(c.model).size;c.pos={10,10};c.z=1.5f;c.vz=-8;impact.m_clutter={c};for(int i=0;i<300&&!impact.m_clutter[0].broken;++i)impact.updateClutter({},1.f/120);const auto&v=impact.m_clutter[0];check(v.broken&&v.sleeping&&lengthSq(v.velocity)==0&&v.vz==0&&v.spin==0&&v.pitchSpeed==0&&v.rollSpeed==0,"hard impact leaves a fully inert tombstone");}
 for(int variant=0;variant<4;++variant){auto m=regression::room();m->fixtures.push_back({24,{8,8},0,1.4f,.7f,.75f,0,true});kit::deskSet(*m,{8,8},.75f,0,variant);Game desk(regression::campaign(m));bool supported=desk.m_clutter.size()==4;for(const auto&c:desk.m_clutter){auto e=c.extent();supported&=c.sleeping;for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]})supported&=std::fabs(desk.world().supportBelow(c.pos.x+x,c.pos.y+y,.8f)-.75f)<.01f;}check(supported,"binder, mug, desk lamp and pill bottle desk variants seed fully supported");}
 {Game budget(regression::campaign(regression::room()));budget.m_player.pos={3,3};for(int i=0;i<100;++i){Clutter c;c.kind=7;c.model=model::keyboard;c.dims=propTraits(c.model).size;c.pos={8+(i%10)*.8f,8+(i/10)*.8f};c.z=2;budget.m_clutter.push_back(c);}budget.updateClutter({},1.f/60);int moving=0;for(const auto&c:budget.m_clutter)moving+=c.vz<0;check(moving==48,"nearest 48 awake props run at full rate");budget.updateClutter({},1.f/60);check(std::all_of(budget.m_clutter.begin(),budget.m_clutter.end(),[](const Clutter&c){return c.vz<-.2f;}),"remaining awake props advance at half rate without freezing");}
 for(int variant=0;variant<4;++variant){auto m=regression::room();kit::workstation(*m,8,8,0,1,variant,true);Game workstation(regression::campaign(m));check(workstation.m_clutter.size()==4&&std::all_of(workstation.m_clutter.begin(),workstation.m_clutter.end(),[](const Clutter&c){return c.sleeping&&c.model!=model::pc_tower;}),"loose workstation seeds desktop props without a buried floor tower");}
 {auto next=regression::room();next->definition.origin={24,0};auto campaign=regression::campaign(regression::room());campaign->maps.push_back(next);Game resident(campaign);resident.ensureChunk(1);Enemy enemy;enemy.pos=enemy.home=enemy.lastKnown={10,10};resident.m_chunks[1].enemies={enemy};Clutter c;c.kind=7;c.model=model::keyboard;c.dims=propTraits(c.model).size;c.pos={9,10};c.z=.1f;c.velocity={12,0};c.projectile=true;resident.m_chunks[1].clutter={c};for(int i=0;i<120;++i)resident.updateClutter({},1.f/120);check(resident.m_chunks[1].enemies[0].hp==105&&!resident.m_chunks[1].clutter[0].projectile,"resident-neighbour thrown prop hits its creature once for legacy five damage");}
 {Game held(regression::campaign(regression::room()));Clutter c;c.kind=7;c.model=model::crt_monitor;c.dims=propTraits(c.model).size;c.pos={8,8};c.z=.75f;held.m_clutter={c};held.m_heldClutter=0;check(held.traceShot({6,8,.9f},{1,0,0},10).kind==ShotKind::Prop,"held monitor retains its physical bullet-blocking box");held.m_clutter[0].model=model::mug;held.m_clutter[0].dims=propTraits(model::mug).size;held.breakClutter(0,0,{1,0,0});Game loaded(held.m_customCampaign);check(!held.holdingClutter()&&loaded.decodeSave(held.encodeSave())&&loaded.m_clutter[0].broken,"breaking a held prop drops it and leaves a loadable tombstone");}
 for(bool held:{false,true})for(bool broken:{false,true}){auto first=regression::room(),second=regression::room();first->openEast=true;first->layers[0].rows[8][23]='.';second->definition.origin={24,0};second->openWest=true;second->layers[0].rows[8][0]='.';kit::prop(*first,model::mug,22.5f,8,0,0);kit::prop(*second,model::mug,22.5f,8,0,0);auto campaign=regression::campaign(first);campaign->maps.push_back(second);Game g(campaign);g.m_clutter[0].pos={24.1f,8};g.m_player.pos={24.1f,8};if(held)g.m_heldClutter=0;g.crossChunkBoundary();int moved=-1;for(int i=0;i<int(g.m_clutter.size());++i)if(std::fabs(g.m_clutter[i].home.x-22.5f)<.01f)moved=i;if(moved>=0&&broken)g.breakClutter(1,moved,{});auto bytes=g.encodeSave();Game loaded(campaign);bool valid=moved>=0&&loaded.decodeSave(bytes);check(valid&&loaded.level()==1&&loaded.m_clutter.size()==2&&loaded.chunkClutter(0).empty()&&loaded.holdingClutter()==(held&&!broken)&&loaded.encodeSave()==bytes,"cross-seam authored live/held/tombstone identity roundtrips without desk respawn");}
 check(testPropSaves(),"v16 migration and held prop save");
 for(float dt:{1.f/60,1.f/120}){auto stacked=regression::room();kit::prop(*stacked,model::binder,10,10,0,.3f,.32f,.08f,0);kit::prop(*stacked,model::mug,10,10,.08f,0);Game stack(regression::campaign(stacked));stack.m_player.pos={3,3};stack.knockClutter(0,0,{10,10,.04f},{3,0,0});for(int i=0;i<int(5/dt);++i)stack.updateClutter({},dt);const auto&a=stack.m_clutter[0];const auto&b=stack.m_clutter[1];auto ae=a.extent(),be=b.extent();float overlap=std::min({ae[0]+be[0]-std::fabs(a.pos.x-b.pos.x),ae[1]+be[1]-std::fabs(a.pos.y-b.pos.y),std::min(a.z+a.height(),b.z+b.height())-std::max(a.z,b.z)});check(a.sleeping&&b.sleeping&&!b.broken&&a.z>=-.001f&&b.z>=-.001f&&overlap<.01f,"knocked stack wakes and settles without interpenetration at 60/120 Hz");}
 {Game carry(regression::campaign(regression::room()));carry.m_player.pos={6,6};carry.m_player.angle=0;Clutter c;c.kind=7;c.model=model::crt_monitor;c.dims={.42f,.4f,.38f};c.pos={7,6};c.z=.75f;carry.m_clutter={c};check(carry.interactClutter()&&carry.holdingClutter(),"11 kg monitor can be lifted");InputState fire;fire.fire=true;carry.updateClutter(fire,1.f/120);check(!carry.holdingClutter()&&length(carry.m_clutter[0].velocity)<5,"11 kg monitor throw is under 5 m/s");c.model=0;c.dims={1,1,1};carry.m_clutter={c};carry.m_heldClutter=-1;check(!carry.interactClutter()&&!carry.holdingClutter()&&carry.m_clutter[0].mass()==30,"30 kg uncurated box cannot be lifted");}
 {auto neighbour=regression::room();neighbour->definition.origin={24,0};neighbour->layers[0].rows[10][10]='B';auto multi=regression::campaign(regression::room());multi->maps.push_back(neighbour);Game n(multi);n.ensureChunk(1);Clutter c;c.kind=7;c.model=model::keyboard;c.dims={.45f,.16f,.03f};c.pos={10,10};c.z=2;n.m_chunks[1].clutter={c};n.detonateBarrel(1,10,10);for(int i=0;i<600;++i)n.updateClutter({},1.f/120);check(n.m_chunks[1].clutter.size()==6&&std::all_of(n.m_chunks[1].clutter.begin(),n.m_chunks[1].clutter.end(),[](const Clutter&v){return v.sleeping&&v.z<.02f;}),"resident neighbour prop and actual barrel debris land and sleep");}
 report<<(ok?"ALL PASSED props":"FAILED props")<<'\n';return ok;
}
bool Game::testClutter(){
 {auto vr=validationScene(Enemy::Kind::Huntsman);vr.m_clutter={{{4.2f,4.5f},{},.56f}};vr.m_clutter[0].kind=2;
  vr.setVrHand(0,{4.2f,4.5f,.7f},{1,0,0},true);vr.setVrHand(1,{}, {},false);
  if(!vr.interactClutter()||!vr.holdingClutter())return false;vr.updateClutter({},.01f);
  if(!vr.holdingClutter()||std::fabs(vr.m_clutter[0].pos.x-4.32f)>.001f||std::fabs(vr.m_clutter[0].z+vr.m_clutter[0].height()*.5f-.7f)>.001f)return false;
  vr.setVrHand(0,{}, {},false);vr.updateClutter({},.01f);if(vr.holdingClutter())return false;
  auto pose=vr.m_player;int ammo=pose.ammo;vr.m_enemies[0].pos={5.2f,4.5f};vr.setVrAim({3.8f,4.5f,.6f},{1,0,0},true);vr.shoot();
  if(vr.m_enemies[0].hp!=76||vr.m_player.ammo!=ammo-1||vr.m_player.pos.x!=pose.pos.x||vr.m_player.z!=pose.z||vr.m_player.angle!=pose.angle||vr.m_player.pitch!=pose.pitch)return false;
 }
 {auto grab=validationScene(Enemy::Kind::Huntsman);grab.m_enemies.clear();grab.m_clutter={{{4.2f,4.5f},{},.56f}};grab.m_clutter[0].kind=2;grab.setVrHand(0,{4.2f,4.5f,.7f},{1,0,0},true);grab.setVrHand(1,{2.5f,4.5f,1.f},{1,0,0},true);
  InputState grip{};grip.vrTracked=true;grip.vrGrip[1]=true;grab.updateInteraction(grip,.01f);if(grab.holdingClutter())return false;
  grip.vrGrip[0]=true;grab.updateInteraction(grip,.01f);if(!grab.holdingClutter()||grab.m_vrCarryHand!=0)return false;grab.updateInteraction(grip,.01f);if(!grab.holdingClutter())return false;
  grab.setVrHand(0,{4.2f,4.5f,.7f},{1,0,0},true,{2,1,3});grip.vrGrip[0]=false;grab.updateInteraction(grip,.01f);if(grab.holdingClutter()||grab.m_clutter[0].velocity.x!=2||grab.m_clutter[0].vz!=3)return false;
 }
 // All carryable shapes follow eye height and pitch rather than a feet-level
 // cap. Check both player stances and rotated bounds.
 for(int kind=0;kind<7;++kind)for(bool crouched:{false,true})for(float pitch:{-30.f,0.f,65.f}){
  auto held=validationScene(Enemy::Kind::Huntsman);held.m_enemies.clear();
  held.m_player.crouched=crouched;held.m_player.eye=crouched?Player::CrouchedEye:Player::StandingEye;held.m_player.pitch=pitch;
  Clutter item;item.kind=kind;item.pos=held.m_player.pos;item.pitch=.25f;item.roll=.2f;
  held.m_clutter={item};held.m_heldClutter=0;held.updateClutter({},1.f/120);
  const auto&c=held.m_clutter[0];float angle=pitch/140.f;
  if(!held.holdingClutter()||std::fabs(length(c.pos-held.m_player.pos)-.9f*std::cos(angle))>.001f||
     std::fabs(c.z+c.height()*.5f-held.m_player.z-held.m_player.eye-.9f*std::sin(angle))>.001f)return false;
 }
 {Game source;source.m_clutter.clear();Clutter shard;shard.kind=6;shard.pos={4,4};shard.z=source.world().floorHeight(4,4);source.m_clutter.push_back(shard);Game restored;
  if(shard.impactSound()!=Sound::JunkSoft||shard.size()[0]<=shard.size()[1]||!restored.decodeSave(source.encodeSave())||restored.clutter().size()!=1||restored.clutter()[0].kind!=6)return false;
 }
 {Game legacy;legacy.m_clutter.clear();for(int kind=0;kind<4;++kind){Clutter old;old.kind=kind;old.pos={4,4};old.z=legacy.world().floorHeight(4,4);legacy.m_clutter.push_back(old);}Game restored;
  if(!restored.decodeSave(legacy.encodeSave())||restored.clutter().size()!=4)return false;for(const auto&item:restored.clutter())if(item.kind!=6||item.sleeping)return false;
 }
 {auto g=mapInspection({21.55f,21.46f},kPi*.5f,0,5,true,-9,true);g.m_clutter.clear();
  Clutter bottle;bottle.kind=2;bottle.pos={21.55f,21.8f};bottle.z=-8.3f;g.m_clutter.push_back(bottle);g.m_heldClutter=0;
  g.updateClutter({},1.f/120);
  if(!g.holdingClutter()||g.m_clutter[0].pos.y<22.32f||g.m_clutter[0].z<-7.95f)return false;
  InputState use{};use.use=true;g.update(use,1.f/120);
  if(g.holdingClutter())return false;
  for(int i=0;i<300;++i)g.update({},1.f/120);
  float tier=g.world().floorHeight(21.55f,22.56f)+1.8f*.54f;
  if(std::fabs(g.m_clutter[0].z-tier)>.035f||g.m_clutter[0].pos.y<22.32f)return false;
  g.update(use,1.f/120);
  if(!g.holdingClutter())return false;
  std::ofstream("shelf-test.txt")<<"Held bottle placed onto the purchased shelf, settles on a tray and can be picked up again: PASS\n";
 }
 {auto g=mapInspection({12,23.5f},kPi*.5f,0,4,false,-9,true);g.m_clutter.clear();Clutter c;c.pos={12,23.8f};c.z=-8.5f;c.velocity={0,4};c.kind=2;g.m_clutter.push_back(c);
  for(int i=0;i<20;++i)g.updateClutter({},1.f/120);
  if(g.m_clutter.size()!=1||g.m_clutter[0].pos.y<=24)return false;
  float speed=g.m_clutter[0].velocity.y;g.m_player.pos.y=24.1f;g.crossChunkBoundary();
  // The destination map may already contain authored debris; the crossing
  // item is appended after that population and keeps its own motion state.
  size_t transferred=g.m_clutter.size()-1;
  if(g.m_clutter[transferred].pos.y<0||g.m_clutter[transferred].pos.y>2||g.m_clutter[transferred].velocity.y!=speed)return false;
  Game restored;if(!restored.decodeSave(g.encodeSave())||restored.m_clutter.size()!=g.m_clutter.size()||restored.m_clutter[transferred].velocity.y!=speed)return false;
  g.m_player.pos={12,.3f};g.m_player.angle=-kPi*.5f;g.m_heldClutter=int(transferred);g.updateClutter({},.01f);
  if(!g.holdingClutter()||g.m_clutter[transferred].pos.y>=0)return false;
  g.m_player.pos.y=-.1f;g.crossChunkBoundary();if(g.level()!=4||!g.holdingClutter()||g.m_clutter.size()!=1)return false;
 }
 {auto g=mapInspection({9,11.5f},0,0,5,false,-9.18f,true);g.m_clutter.clear();Clutter bottle;bottle.kind=2;bottle.pos={9,11.5f};bottle.z=-9.17f;bottle.velocity={2,0};g.m_clutter.push_back(bottle);
  g.updateClutter({},.02f);if(g.m_clutter[0].velocity.x>=2||!std::isfinite(g.m_clutter[0].z))return false;
  if(g.world().waterSurface(12,6)>-100||g.world().waterSurface(9,11.5f)<-10)return false;
 }
 {auto slide=validationScene(Enemy::Kind::Huntsman);slide.m_enemies.clear();slide.m_clutter={{{1.14f,9.5f},{-4,2},1.f}}; // The Foundry's bare west wall.
  slide.updateClutter({},.025f);auto& c=slide.m_clutter[0];
  if(c.velocity.x<=0||c.velocity.y<1.9f||c.pos.y<=9.5f)return false;
 }
 auto g=validationScene(Enemy::Kind::Huntsman);g.m_player.pitch=-85;g.m_enemies[0].pos={5.2f,4.5f};g.m_clutter={{{4.2f,4.5f}}};
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

namespace retro {
bool Game::damagePane(int level,int index,float amount,Point3 at,Point3 direction){
 auto&w=level==m_level?m_world:m_chunks[level].world;if(!w.breakableIntact(index))return false;const auto b=w.breakables().at(index);bool broken=w.damageBreakable(index,amount);auto shift=chunkOffset(level)-chunkOffset(m_level);Vec2 center{(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f};
 if(!broken){m_bulletImpacts.push_back({{at.x,at.y},at.z,{-direction.x,-direction.y},m_elapsed,level,false,-3});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());m_sounds.push_back({Sound::JunkGlass,center+shift,.4f,.6f,true,17,at.z});return false;}
 m_bulletImpacts.erase(std::remove_if(m_bulletImpacts.begin(),m_bulletImpacts.end(),[&](const BulletImpact&p){return p.level==level&&p.pos.x>=b.x1-.03f&&p.pos.x<=b.x2+.03f&&p.pos.y>=b.y1-.03f&&p.pos.y<=b.y2+.03f&&p.z>=b.bottom-.03f&&p.z<=b.top+.03f;}),m_bulletImpacts.end());
 bool alongX=b.x2-b.x1>b.y2-b.y1;m_breakEvents.push_back({center,b.bottom,direction,m_elapsed,level,PropMaterial::Glass,std::max(b.x2-b.x1,b.y2-b.y1),b.top-b.bottom,true,alongX?Vec2{1,0}:Vec2{0,1}});if(m_breakEvents.size()>24)m_breakEvents.erase(m_breakEvents.begin());
 SoundEvent event{Sound::GlassShatter,center+shift,1,1,true,20,(b.bottom+b.top)*.5f};event.aiRadius=14;m_sounds.push_back(event);if(b.state)setState(b.state,1);
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;for(auto&c:list){auto e=c.extent();if(c.pos.x+e[0]>=b.x1-.1f&&c.pos.x-e[0]<=b.x2+.1f&&c.pos.y+e[1]>=b.y1-.1f&&c.pos.y-e[1]<=b.y2+.1f&&c.z+c.height()>=b.bottom-.1f&&c.z<=b.top+.1f&&!c.broken){c.sleeping=false;c.restTime=0;}}
 return true;
}
void Game::updateBreakables(){
 for(int level=0;level<chunkCount();++level){auto&w=level==m_level?m_world:m_chunks[level].world;for(int i=0;i<int(w.breakables().size());++i){const auto b=w.breakables()[i];if(!w.breakableIntact(i)){if(b.state)setState(b.state,1);continue;}if(b.holds)continue;
   if(b.trigger&&state(b.trigger)){if(chunkResident(level))damagePane(level,i,1000000,{(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,b.bottom},{0,0,1});else{w.damageBreakable(i,1000000);if(b.state)setState(b.state,1);}}
   else if(b.state&&state(b.state)){bool signalled=false;for(int other=0;other<int(w.breakables().size());++other)if(w.breakables()[other].state==b.state&&!w.breakableIntact(other))signalled=true;if(!signalled)w.damageBreakable(i,1000000);}
  }
 }
 m_breakEvents.erase(std::remove_if(m_breakEvents.begin(),m_breakEvents.end(),[&](const BreakEvent&e){return m_elapsed-e.time>2.5f;}),m_breakEvents.end());
}
}

namespace retro {
bool Game::testGlass(){
 std::ofstream report("glass-test.txt");bool ok=true;auto check=[&](bool pass,const char*label){report<<(pass?"PASS ":"FAIL ")<<label<<'\n';report.flush();ok&=pass;};
 auto room=[](bool holds=false){auto m=regression::room();m->definition.playerStart={6.5f,8};kit::breakable(*m,9.97f,5,10.03f,11,0,3,17,holds,stateId("pane_broken"),stateId("pane_trigger"));return regression::campaign(m);};
 {auto m=regression::room();kit::glazing(*m,6,9.9f,12,10.1f,0,4);World w(0,m);check(w.breakables().size()==5,"glazing builds one breakable per 1.2 m bay");Game fixture(regression::campaign(m));check(auditMapScene(fixture,0),"glazing fixture audit PASS");}
 {auto m=regression::room();kit::glazing(*m,6,9.9f,12,10.1f,0,4,3,.95f,2.4f,false,stateId("shared_broken"));Game g(regression::campaign(m));g.damagePane(0,2,100,{9,10,1},{0,1,0});g.updateBreakables();check(g.world().brokenBreakables().size()==1&&g.state(stateId("shared_broken")),"a shared broken signal preserves all unbroken bays");}
 {Game g(room());std::vector<uint32_t> before,after;ai::collisionSnapshot(g.world(),before);Point3 origin{8,8,1},dir{1,0,0};auto hit=g.traceShot(origin,dir,10);check(hit.passed[0].kind==1,"shot trace reports transparent pane passage");g.m_bulletImpacts.push_back({{10,8},1,{1,0},0,0});g.resolveShot(hit,origin,dir,weaponDef(WeaponId::Shotgun).hitscan,true);ai::collisionSnapshot(g.world(),after);Point3 n;
  check(!g.world().breakableIntact(0)&&g.world().fits(10,8,0,1.6f)&&g.world().shotFraction({8,8,1},{12,8,1},n)==1&&!g.world().railBlocksHull(10,8,.2f,0,1.6f)&&before!=after,"shotgun break clears fits, ballistics and thin hull and invalidates nav snapshot");
  check(g.state(stateId("pane_broken"))&&std::none_of(g.m_bulletImpacts.begin(),g.m_bulletImpacts.end(),[](const BulletImpact&p){return p.pos.x>9.94f&&p.pos.x<10.06f;}),"break sets state and erases floating pane decals");
  auto stimuli=g.aiStimuli();check(std::any_of(stimuli.begin(),stimuli.end(),[](const AiStimulus&s){return s.radius>=14&&s.categories==ai::SoundImpact;}),"GlassShatter is a 14 m impact stimulus");
  Enemy e;e.pos=e.home=e.lastKnown={21.5f,8};g.m_enemies={e};g.m_player.pos={3,3};g.updateEnemies(.01f);check(g.m_enemies[0].state==Enemy::State::Investigate&&length(g.m_enemies[0].lastKnown-Vec2{10,8})<.1f,"shatter heard twelve metres away at pane rather than player");
  auto save=g.encodeSave();Game loaded(room());check(loaded.decodeSave(save)&&!loaded.world().breakableIntact(0),"save/load keeps broken pane");g.storeChunk();g.m_chunks[0].world.unloadGeometry();g.m_chunks[0].resident=false;g.ensureChunk(0);g.m_world=g.m_chunks[0].world;check(!g.world().breakableIntact(0)&&g.world().fits(10,8,0,1.6f),"geometry unload and ensureChunk keep pane broken");
 }
 {Game g(room());Enemy enemy;enemy.pos=enemy.home=enemy.lastKnown={12,8};g.m_enemies={enemy};Point3 origin{8,8,.8f},dir{1,0,0};auto hit=g.traceShot(origin,dir,10);g.resolveShot(hit,origin,dir,kGuardHandgun.hitscan,true);check(g.world().breakableIntact(0)&&std::fabs(g.m_enemies[0].hp-102)<.01f,"first handgun round holes pane and passes at 0.8 damage");hit=g.traceShot(origin,dir,10);g.resolveShot(hit,origin,dir,kGuardHandgun.hitscan,true);check(!g.world().breakableIntact(0),"second near handgun hit breaks pane");}
 {Game g(room());g.damagePane(0,0,10,{10,8,1},{1,0,0});auto bytes=g.encodeSave();Game loaded(g.m_customCampaign);check(loaded.decodeSave(bytes)&&loaded.world().breakableIntact(0)&&loaded.world().breakableDamage(0)==10&&loaded.encodeSave()==bytes,"intact pane damage survives byte-exact v17 save");}
 {Game g(room(true));Point3 origin{8,8,1},dir{1,0,0};for(float damage:{30.f,60.f,90.f}){auto hit=g.traceShot(origin,dir,10);check(hit.kind==ShotKind::Solid&&hit.breakable==0&&hit.passed[0].kind==0,"holds glass stops every bullet");g.damagePane(0,0,damage,{10,8,1},dir);}check(g.world().breakableIntact(0)&&g.world().breakableDamage(0)==180,"holds glass accumulates three crack stages and never breaks");}
 for(bool holds:{false,true}){auto c=room(holds);auto map=std::make_shared<AuthoredMapData>(*c->maps[0]);map->definition.playerStart={10.5f,6.5f};map->breakables[0]={8.4f,1,8.46f,23,0,3,17,0,holds};auto guard=regression::guard({6.5f,6.5f},{6.5f,6.5f},AiFaction::Raider);guard.keys[0].yaw=guard.keys[1].yaw=kPi*.5f;guard.ai.armed=true;map->actorTracks.push_back(guard);c->maps[0]=map;Game g(c);int shots=0;bool firstCleared=false;for(int i=0;i<240;++i){g.update({},1.f/60);for(const auto&s:g.sounds())if(s.sound==Sound::Shot){++shots;if(shots==1)firstCleared=!g.world().breakableIntact(0);}}report<<"guard holds="<<holds<<" shots="<<shots<<" pane intact="<<g.world().breakableIntact(0)<<" player health="<<g.player().health<<'\n';check(holds?shots==0&&g.world().breakableIntact(0):shots>0&&firstCleared&&!g.world().breakableIntact(0),holds?"guard does not fire through holds glass":"guard sees and breaks default glass with the first shotgun round");}
 for(bool holds:{false,true}){auto m=regression::room();m->definition.playerStart={10.5f,6.5f};auto guard=regression::guard({6.5f,6.5f},{6.5f,6.5f},AiFaction::Raider);guard.keys[0].yaw=guard.keys[1].yaw=kPi*.5f;guard.ai.armed=true;m->actorTracks.push_back(guard);Game reference(regression::campaign(m));reference.setState(reference.friendlyKey(0,"guard_mode"),int(GuardState::Alert));reference.setState(reference.friendlyKey(0,"weapon_raise"),1000);auto pose=reference.npcWeaponPose(0);float x=(pose.grip.x+pose.muzzle.x)*.5f;kit::breakable(*m,x-.012f,1,x+.012f,23,0,3,17,holds);Game g(regression::campaign(m));g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));g.setState(g.friendlyKey(0,"weapon_raise"),1000);auto weapon=g.npcWeaponPose(0);auto hit=g.guardShot(0,g.m_friendlyActors[0].pose,weapon);if(!holds)g.resolveShot(hit,weapon.muzzle,weapon.direction,weaponDef(WeaponId::Shotgun).hitscan,false);check(holds?hit.kind==ShotKind::Solid&&g.world().breakableIntact(0):hit.kind==ShotKind::Player&&!g.world().breakableIntact(0),holds?"holds pane stops even a muzzle crossing its box":"glass crossed by the physical barrel is damaged when guard fires");}
 for(int rate:{60,120})for(bool monitor:{false,true}){auto campaign=room();auto thin=std::make_shared<AuthoredMapData>(*campaign->maps[0]);thin->breakables[0].x1=9.988f;thin->breakables[0].x2=10.012f;campaign->maps[0]=thin;Game g(campaign);g.m_player.pos={3,3};Clutter c;c.kind=7;c.model=monitor?model::crt_monitor:model::mug;c.dims=propTraits(c.model).size;c.pos={9,8};c.z=1;c.velocity={monitor?12.f:5.f,0};g.m_clutter={c};for(int i=0;i<rate/5;++i)g.updateClutter({},1.f/rate);check(monitor?!g.world().breakableIntact(0)&&g.m_clutter[0].pos.x>10:g.world().breakableIntact(0)&&g.m_clutter[0].pos.x<10&&!g.m_clutter[0].broken&&g.m_clutter[0].velocity.x<0,monitor?"12 m/s monitor sweeps through thin pane at 60/120 Hz":"light mug stars pane and bounces at 60/120 Hz");}
 {Game g(room());g.m_player.pos={9,8};g.m_player.angle=0;InputState sprint;sprint.forward=true;sprint.sprint=true;for(int i=0;i<120;++i)g.updateMovement(sprint,1.f/120);check(g.m_player.pos.x<9.8f,"real sprint movement cannot tunnel through intact floor screen");g.damagePane(0,0,100,{10,8,1},{1,0,0});for(int i=0;i<80;++i)g.updateMovement(sprint,1.f/120);check(g.m_player.pos.x>10.2f,"real player movement crosses broken floor screen");}
 {auto m=regression::room();kit::glazing(*m,6,9.9f,12,10.1f,0,4,3,.1f,2.4f,false,stateId("run_broken"),stateId("run_trigger"));auto campaign=regression::campaign(m);Game g(campaign);g.setState(stateId("run_trigger"),1);g.updateBreakables();check(g.world().brokenBreakables().size()==5&&g.m_breakEvents.size()==5&&g.state(stateId("run_broken")),"trigger breaks whole run with effects and shared state");Game loaded(campaign);check(loaded.decodeSave(g.encodeSave())&&loaded.world().brokenBreakables().size()==5&&loaded.state(stateId("run_broken")),"trigger state stays consistent after load");}
 report<<(ok?"ALL PASSED glass":"FAILED glass")<<'\n';return ok;
}
Game Game::physicsInspection(int view,float seconds){
 auto map=regression::room();map->name="Physics inspection";map->definition.playerStart={6,5.5f};map->fixtures.push_back({24,{8,8},0,1.4f,.7f,.75f,0,true});kit::deskSet(*map,{8,8},.75f,0,1);
 kit::glazing(*map,10,6,10.12f,10,0,4,3,.1f,2.4f,view>=2&&view<=5,stateId("inspection_broken"),stateId("inspection_trigger"));
 map->lights.push_back({{8,8},3});Game g(regression::campaign(map));g.m_weaponEquipped=false;g.m_holster=1;g.m_player.angle=.78f;g.m_player.pitch=-12;
 if(view==0&&seconds>0){Point3 origin{8.42f,6,.8f},dir{0,1,0};auto hit=g.traceShot(origin,dir,10);g.resolveShot(hit,origin,dir,weaponDef(WeaponId::Shotgun).hitscan,true);}
 if(view==1){auto&c=g.m_clutter[0];c.pos={9,8};c.z=1.2f;c.velocity={12,0};c.sleeping=false;g.m_player.pos={7,5.5f};g.m_player.angle=.7f;}
 if(view>=2&&view<=5){float damage=view==2?0:view==3?30:view==4?90:180;if(damage>0)for(int i=0;i<int(g.world().breakables().size());++i){const auto&b=g.world().breakables()[i];g.damagePane(0,i,damage,{(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,(b.bottom+b.top)*.5f},{1,0,0});}}
 if(view==6){g.setState(stateId("inspection_trigger"),1);g.updateBreakables();g.m_player.pos={8.7f,8};g.m_player.angle=0;g.m_player.pitch=-65;}
 for(int i=0;i<int(seconds*120);++i){g.m_elapsed+=1.f/120;g.updateClutter({},1.f/120);}return g;
}
}
