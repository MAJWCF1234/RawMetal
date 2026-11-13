#include "Game.h"
#include <fstream>
#include "../world/ModelIds.h"
#include "../core/Ray.h"
#include "EngineRegressionScene.h"
#include "../world/CustomCampaign.h"
#include "../world/CampaignKit.h"
#include "../world/GlassFracture.h"
#include "ai/WalkQueryCache.h"
namespace retro {
PropTraits Game::propTraits(int id){
 if(id<0||id>=FacilityModelCount)return {PropMaterial::Plastic,1,0,{.1f,.1f,.1f}};
 const auto&s=model::propSize[id];float mass=model::mass[id];return {model::material[id]<0?PropMaterial::Plastic:PropMaterial(model::material[id]),mass>0?mass:std::clamp(140*s[0]*s[1]*s[2],.05f,30.f),model::health[id],{s[0],s[1],s[2]}};
}
PropMaterial Clutter::material()const{constexpr PropMaterial old[]={PropMaterial::Paper,PropMaterial::Cardboard,PropMaterial::Glass,PropMaterial::Electronic,PropMaterial::Electronic,PropMaterial::Plastic,PropMaterial::Wood};if(model>=0)return Game::propTraits(model).material;
 if(kind==6)switch(shardOf){case ShardOf::Barrel:return PropMaterial::Metal;case ShardOf::Ceiling:return PropMaterial::Cardboard;case ShardOf::Model:return shardModel>=0?Game::propTraits(shardModel).material:PropMaterial::Plastic;default:break;}
 return old[kind];}
std::vector<Clutter> Clutter::shards(ShardOf of,Point3 centre,std::array<float,3> size,float yaw,int count,float speed,float lift,int model){
 // Eighths in an order that spreads the first few round the middle, top and bottom.
 constexpr int order[8]={0,7,3,4,5,2,6,1};std::vector<Clutter> out;const float cy=std::cos(yaw),sy=std::sin(yaw);
 for(int i=0;i<count;++i){Clutter c;c.kind=6;c.shardOf=of;c.piece=order[i%8];c.shardModel=model;c.from=size;c.yaw=yaw;
  float ox,oy,oz;
  if(of==ShardOf::Ceiling){
   // A tile lets go in hand-sized slabs strewn across it.
   c.dims={.34f,.26f,std::clamp(size[2],.015f,.04f)};const float a=float(i)*2.399963f,r=.5f*std::sqrt((float(i)+.5f)/float(std::max(1,count)));
   ox=std::cos(a)*r*std::max(0.f,size[0]*.5f-.17f)*2;oy=std::sin(a)*r*std::max(0.f,size[1]*.5f-.13f)*2;oz=0;c.yaw=yaw+a;}
  else{c.dims={std::max(.02f,size[0]*.5f),std::max(.02f,size[1]*.5f),std::max(.02f,size[2]*.5f)};
   ox=(c.piece&1?.25f:-.25f)*size[0];oy=(c.piece&2?.25f:-.25f)*size[1];oz=(c.piece&4?.25f:-.25f)*size[2];}
  const Vec2 off{cy*ox-sy*oy,sy*ox+cy*oy};c.pos=Vec2{centre.x,centre.y}+off;c.z=centre.z+oz-c.dims[2]*.5f;
  const float r=std::max(.001f,length(off));const float a=float(i)*2.399963f;const Vec2 out2=r>.01f?off*(1/r):Vec2{std::cos(a),std::sin(a)};
  c.velocity=out2*(speed*(.8f+.1f*float(i%5)));c.vz=lift*(.75f+.15f*float(i%3))+(c.piece&4?.6f:0.f);
  const float sign=i%2?1.f:-1.f;c.spin=sign*(3.f+float(i%3));c.pitchSpeed=-sign*(2.5f+float(i%2));c.rollSpeed=sign*2.f;out.push_back(c);}
 return out;
}
float Clutter::mass()const{if(model<0)return 1;auto t=Game::propTraits(model);return model::mass[model]>0?t.mass:std::clamp(140*dims[0]*dims[1]*dims[2],.05f,30.f);}
// The cart handle rises above its load-bearing tray. Preserve that space for desktop items.
float Clutter::supportHeight()const{return model==model::med_crash_cart&&std::fabs(pitch)+std::fabs(roll)<.1f?dims[2]*.809f:height();}
void Game::knockClutter(int level,size_t index,Point3 at,Point3 impulse){
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;auto&c=list.at(index);if(c.broken)return;c.soundOwner=-1;
 auto wake=[&](Clutter&v){v.sleeping=false;v.restTime=0;};wake(c);
 for(auto&v:list)if(&v!=&c&&!v.broken&&std::fabs(v.z-c.z-c.supportHeight())<.03f&&length(v.pos-c.pos)<(v.footprint()+c.footprint())*.7f)wake(v);
 float mass=c.mass();Point3 dv=impulse*(1/mass);float speed=std::sqrt(dv.x*dv.x+dv.y*dv.y+dv.z*dv.z);if(speed>14)dv=dv*(14/speed);c.velocity+=Vec2{dv.x,dv.y};c.vz+=dv.z;
 auto s=c.size();auto lever=at-Point3{c.pos.x,c.pos.y,c.z+c.height()*.5f};Point3 torque{lever.y*impulse.z-lever.z*impulse.y,lever.z*impulse.x-lever.x*impulse.z,lever.x*impulse.y-lever.y*impulse.x};float inertia=mass*(s[0]*s[0]+s[1]*s[1]+s[2]*s[2])/12;
 c.spin+=std::clamp(torque.z/std::max(.001f,inertia),-14.f,14.f);c.pitchSpeed+=std::clamp(torque.y/std::max(.001f,inertia),-14.f,14.f);c.rollSpeed+=std::clamp(torque.x/std::max(.001f,inertia),-14.f,14.f);
}
void Game::breakClutter(int level,size_t index,Point3 direction){
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;auto&c=list.at(index);if(c.model<0||c.broken)return;int owner=c.soundOwner;knockClutter(level,index,{c.pos.x,c.pos.y,c.z},{});c.soundOwner=owner;c.broken=true;if(level==m_level&&int(index)==m_heldClutter)m_heldClutter=-1;c.sleeping=true;c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;
 m_breakEvents.push_back({c.pos,c.z,direction,m_elapsed,level,c.material(),c.footprint(),c.height()});m_breakEvents.back().model=c.model;if(m_breakEvents.size()>24)m_breakEvents.erase(m_breakEvents.begin());
 const auto&t=propMaterialTraits(c.material());auto shift=chunkOffset(level)-chunkOffset(m_level);SoundEvent sound{t.breakSound,c.pos+shift,1,t.breakPitch,true,17,c.z};sound.owner=owner;sound.aiRadius=c.material()==PropMaterial::Ceramic?7:8;m_sounds.push_back(sound);if(c.breakState)setState(c.breakState,1);
 if(c.material()==PropMaterial::Organic||c.material()==PropMaterial::Glass)m_bulletImpacts.push_back({c.pos,c.z+.005f,{},m_elapsed,level,true,c.material()==PropMaterial::Glass?1:int(CreatureKind::Mutant),1});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
 // A solid prop (wood, plastic, metal, electronics, cardboard) comes apart in pieces of itself, cut
 // from its own mesh with its own sheet; glass and pottery shatter to chips, food and paper to nothing.
 // Queued, not added here: callers hold references into the list (landShards adds them).
 {const auto m=c.material();const float big=std::max({c.dims[0],c.dims[1],c.dims[2]});
  if((m==PropMaterial::Wood||m==PropMaterial::Plastic||m==PropMaterial::Metal||m==PropMaterial::Electronic||m==PropMaterial::Cardboard)&&big>=.12f){
   const Point3 centre{c.pos.x,c.pos.y,c.z+c.height()*.5f};const Point3 d=direction;
   for(auto p:Clutter::shards(ShardOf::Model,centre,c.dims,c.yaw,big>=.5f?6:4,1.6f,1.8f,c.model)){p.velocity+=Vec2{d.x,d.y}*1.2f;m_pendingShards.push_back({level,p});}}}
}
void Game::landShards(){
 for(auto&[level,c]:m_pendingShards){if(level==m_level)m_clutter.push_back(c);else if(level>=0&&level<chunkCount())m_chunks[level].clutter.push_back(c);}
 m_pendingShards.clear();
}
bool Game::damageClutter(int level,size_t index,float amount,Point3,Point3 direction){auto&c=(level==m_level?m_clutter:m_chunks[level].clutter).at(index);if(c.model<0||c.broken)return false;c.damage+=std::max(0.f,amount);float health=propTraits(c.model).health;if(health>0&&c.damage>=health){breakClutter(level,index,direction);return true;}return false;}
void Game::seedClutter(){
 m_heldClutter=-1;m_clutter.clear();
 for(const auto& spawn:m_world.clutterSpawns()){
  Clutter c;c.kind=spawn.kind;c.pos=spawn.position;c.yaw=spawn.yaw;
  c.model=spawn.model;c.dims={spawn.width,spawn.depth,spawn.height};c.pitch=spawn.pitch;c.roll=spawn.roll;c.breakState=spawn.breakState;
  c.z=spawn.z==-999?m_world.supportBelow(c.pos.x,c.pos.y,m_world.floorHeight(c.pos.x,c.pos.y)+.5f):spawn.z;
  if(c.model>=0){c.authored=true;c.home=c.pos+chunkOffset(m_level);c.homeZ=spawn.z;auto e=c.extent();float support=-100;
   for(auto offset:c.footprintOffsets()){float x=offset.x,y=offset.y;float z=m_world.supportBelow(c.pos.x+x,c.pos.y+y,c.z+.05f,true);for(const auto&other:m_clutter){auto oe=other.extent();if(!other.broken&&(other.model>=0?other.footprintContains(c.pos+Vec2{x,y}):(std::fabs(c.pos.x+x-other.pos.x)<=oe[0]&&std::fabs(c.pos.y+y-other.pos.y)<=oe[1]))&&other.z+other.supportHeight()<=c.z+.03f)z=std::max(z,other.z+other.supportHeight());}support=std::max(support,z);}
   if(c.z-support>=-.025f&&c.z-support<=.03f){c.z=support;c.sleeping=true;}
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
 landShards();
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
  auto extent=c.extent();auto fit=[&](float t){auto p=m_player.pos+(target-m_player.pos)*t;float z=eye.z+(height-eye.z)*t;for(auto offset:c.footprintOffsets()){auto q=p+offset;const auto&w=worldAt(q);if(!w.fits(q.x,q.y,z,c.height(),true,true)||w.doorBlocks(q.x,q.y,z,c.height()))return false;}return true;};
  if(c.model>=0&&!fit(1)){float lo=0,hi=1;for(int i=0;i<8;++i){float mid=(lo+hi)*.5f;if(fit(mid))lo=mid;else hi=mid;}target=m_player.pos+(target-m_player.pos)*lo;height=eye.z+(height-eye.z)*lo;clear&=lo>.1f;}
  if(clear&&targetWorld.fits(local.x,local.y,height,c.height(),true,true)){c.pos=target;c.z=height;c.velocity={};c.vz=0;c.yaw=m_vrInputActive?std::atan2(forward.y,forward.x):m_player.angle;c.pitchSpeed=c.rollSpeed=c.spin=0;}
  else{m_heldClutter=-1;c.projectile=false;}
  if(holdingClutter()&&(!input.vrTracked||m_vrCarryHand==1)&&input.fire&&!m_previousFire&&!m_suppressFire){float speed=9.f*(c.model>=0?std::clamp(std::sqrt(2.5f/c.mass()),.35f,1.f):1.f);c.velocity=forward*(speed*std::cos(pitch));c.vz=2.f+speed*std::sin(pitch);c.spin=3;c.pitchSpeed=10;c.rollSpeed=4;c.sleeping=false;c.projectile=true;c.soundOwner=-1;m_heldClutter=-1;sound(Sound::PunchSwing,.55f);}
 }
 m_propHalfStep=!m_propHalfStep;simulateClutter(m_clutter,m_level,dt,m_heldClutter);
 for(int level=0;level<chunkCount();++level)if(level!=m_level&&chunkResident(level))simulateClutter(m_chunks[level].clutter,level,dt,-1);
 landShards();
 // Leads follow their props where the props have just been put.
 for(int level=0;level<chunkCount();++level)if(chunkResident(level))updateLeads(level,dt);
 updateElectricity(dt);
}
int Game::leadProp(int level,const Lead& lead,int hint)const{
 const auto&w=chunkWorld(level);if(lead.prop<0||lead.prop>=int(w.clutterSpawns().size()))return -1;
 const auto&spawn=w.clutterSpawns()[size_t(lead.prop)];const auto home=spawn.position+chunkOffset(level);const auto&list=chunkClutter(level);
 // An authored prop keeps its spawn as home wherever it goes (the lamp-on-a-prop rule).
 auto same=[&](const Clutter&c){return c.authored&&c.model==spawn.model&&lengthSq(c.home-home)<.0004f&&std::fabs(c.homeZ-spawn.z)<.05f;};
 if(hint>=0&&hint<int(list.size())&&same(list[size_t(hint)]))return hint;
 for(int i=0;i<int(list.size());++i)if(same(list[size_t(i)]))return i;
 return -1;
}
Point3 Game::leadClip(const Clutter& c,Point3 offset){auto s=c.size();auto r=c.rotate(offset.x,offset.y,offset.z-s[2]*.5f);return {c.pos.x+r[0],c.pos.y+r[1],c.z+c.height()*.5f+r[2]};}
LeadBody Game::leadBody(const Clutter& c){
 const auto s=c.size();LeadBody b{{c.pos.x,c.pos.y,c.z+c.height()*.5f}};
 for(int j=0;j<3;++j){const auto a=c.rotate(float(j==0),float(j==1),float(j==2));b.axis[j]={a[0],a[1],a[2]};b.half[j]=s[size_t(j)]*.5f;}return b;
}
float Game::leadThrough(int level,int index)const{
 const auto&w=chunkWorld(level);const auto&l=w.leads().at(size_t(index));const auto&r=w.leadRopes().at(size_t(index));const int k=l.prop>=0&&!r.torn?leadProp(level,l,r.clutter):-1;
 if(k<0||r.points.size()<2)return -1;const auto&c=chunkClutter(level)[size_t(k)];const auto body=leadBodyAtClip(leadBody(c),leadClip(c,l.offset));float deepest=-1;
 // Eight samples along each piece of the cable as it is drawn, from the anchor up to (not including) the clip.
 for(size_t j=0;j+1<r.points.size();++j)for(int t=0;t<8;++t)deepest=std::max(deepest,leadBodyDepth(body,leadCurve(r.points,j,float(t)/8)));return deepest;
}
void Game::updateLeads(int level,float dt){
 auto&w=friendlyWorld(level);if(w.leads().empty())return;const auto&list=chunkClutter(level);const auto&enemies=chunkEnemies(level);const auto&spawns=w.creatureSpawns();
 for(int i=0;i<int(w.leads().size());++i){const auto&l=w.leads()[size_t(i)];auto&r=w.m_leadRopes[size_t(i)];
  Point3 clip{};LeadBody body{};const Point3* end=l.prop<0&&!l.loose?&l.end:nullptr;const LeadBody* on=nullptr;
  // A free physics chain hung from a creature: pin its Verlet mount to the live
  // body's facing frame (mesh x right, y forward, z up from its feet).
  if(l.chain&&l.creature>=0&&l.creature<int(spawns.size())){const auto&spawn=spawns[size_t(l.creature)];const Enemy* bodyEnemy=nullptr;
   for(const auto&e:enemies){const Vec2 home=e.originLevel==level?e.originHome:e.home;if(e.kind==spawn.kind&&lengthSq(home-spawn.position)<.01f&&(e.alive||e.deathTime<.2f)){bodyEnemy=&e;break;}}
   if(bodyEnemy){const float c=std::cos(bodyEnemy->heading),s=std::sin(bodyEnemy->heading);
    const Point3 mount={bodyEnemy->pos.x+l.offset.y*c-l.offset.x*s,bodyEnemy->pos.y+l.offset.y*s+l.offset.x*c,bodyEnemy->z+l.offset.z};
    if(r.anchorOverride){const auto d=mount-r.liveAnchor;if(d.x*d.x+d.y*d.y+d.z*d.z>1e-8f){r.sleeping=false;r.rest=0;}}
    r.anchorOverride=true;r.liveAnchor=mount;}}
  // The clip reads its prop: carried into another chunk, broken, off the map or
  // pulled past the cable's length, it tears. A cable not yet hung tears quietly.
  if(!r.torn&&l.prop>=0){const int k=leadProp(level,l,r.clutter);r.clutter=k;bool away=k<0;
   if(k>=0){const auto&c=list[size_t(k)];clip=leadClip(c,l.offset);body=leadBody(c);const Point3 anchor=r.anchorOverride?r.liveAnchor:l.anchor;auto d=clip-anchor;
    away=c.broken||c.pos.x<0||c.pos.y<0||c.pos.x>=World::Width||c.pos.y>=World::Height||std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z)>l.length+Lead::TearMargin;}
   if(away)tearLead(level,i,r.points.empty());else{end=&clip;on=&body;}}
  if(r.points.empty()){w.hangLead(i,end,on);continue;}
  // Asleep until its clip moves (or a creature-hung chain's mount moves, above).
  if(r.sleeping){auto d=end&&!r.torn?*end-r.points.back():Point3{};if(d.x*d.x+d.y*d.y+d.z*d.z<1e-6f)continue;r.sleeping=false;r.rest=0;}
  w.stepLead(i,dt,end,on);
 }
}
void Game::settleLeads(int level){auto&w=friendlyWorld(level);for(auto&r:w.m_leadRopes){r.points.clear();r.previous.clear();}updateLeads(level,0);
 // Hanging only lays a cable out (a limp one over what is under it, a clipped one in a heavily
 // damped sag): each then falls and settles as it would in play (at most four seconds of it) and
 // sleeps by the play rule, so a map or a save opens on cables at rest, not frozen part-way.
 for(auto&r:w.m_leadRopes){r.sleeping=false;r.rest=0;}
 for(int t=0;t<480&&std::any_of(w.m_leadRopes.begin(),w.m_leadRopes.end(),[](const LeadRope&r){return !r.sleeping;});++t)updateLeads(level,1.f/120);}
void Game::tearLead(int level,int index,bool quiet){
 auto&w=friendlyWorld(level);auto&r=w.m_leadRopes.at(size_t(index));const auto&l=w.leads()[size_t(index)];if(r.torn)return;
 // Whether the player's hand is on the prop it was clipped to (it may already be over a seam).
 bool held=false;if(holdingClutter()&&l.prop>=0&&l.prop<int(w.clutterSpawns().size())){const auto&spawn=w.clutterSpawns()[size_t(l.prop)];const auto&c=m_clutter[size_t(m_heldClutter)];
  held=c.authored&&c.model==spawn.model&&lengthSq(c.home-(spawn.position+chunkOffset(level)))<.0004f&&std::fabs(c.homeZ-spawn.z)<.05f;}
 r.torn=true;r.sleeping=false;r.rest=0;r.clutter=-1;if(l.state)setState(l.state,1);
 // The circuit is broken: the panel's pilot goes out (it stutters while the arc lasts, lampPower).
 if(l.lamp>=0&&l.lamp<int(w.lights().size())&&w.lights()[size_t(l.lamp)].enabledState)setState(w.lights()[size_t(l.lamp)].enabledState,1);
 // Torn off under tension the cable had been drawn past its length. Lay it back at its own
 // length along where it ran, still: it falls and swings rather than buckling as it springs back.
 {float drawn=0;for(size_t k=1;k<r.points.size();++k){auto d=r.points[k]-r.points[k-1];drawn+=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
  if(r.points.size()>=2&&drawn>l.length*1.01f){const auto ran=r.points;const float rest=l.length/float(ran.size()-1);float walked=0;size_t k=1;
   for(size_t p=1;p<ran.size();++p){const float want=rest*float(p);float piece=0;
    for(;k<ran.size();++k){auto d=ran[k]-ran[k-1];piece=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(walked+piece>=want)break;walked+=piece;}
    r.points[p]=k<ran.size()?ran[k-1]+(ran[k]-ran[k-1])*(piece>1e-6f?(want-walked)/piece:0.f):ran.back();}
   r.previous=r.points;}}
 if(quiet)return;
 r.tornAt=m_elapsed;const Point3 tip=r.points.empty()?l.anchor:r.points.back();const auto shift=chunkOffset(level)-chunkOffset(m_level);const Vec2 at{tip.x+shift.x,tip.y+shift.y};
 if(!l.live){SoundEvent pop{Sound::JunkMetal,at,.3f,1.6f,true,17,tip.z};pop.aiRadius=3;m_sounds.push_back(pop);return;}
 // A live clip arcs as it comes away, loud enough to bring anything in earshot.
 SoundEvent zap{Sound::LeadArc,at,1,1,true,20,tip.z};zap.aiRadius=14;m_sounds.push_back(zap);
 if(held&&std::fabs(m_elapsed-m_leadShockAt)>.6f){m_leadShockAt=m_elapsed;if(!m_god)m_player.health=std::max(std::min(m_player.health,1.f),m_player.health-LeadShock);m_damageFlash=std::max(m_damageFlash,.8f);m_verticalSpringVelocity-=.5f;sound(Sound::Hurt,.6f,1.15f);}
}
void Game::simulateClutter(std::vector<Clutter>&list,int level,float dt,int held){
 const int initialCount=int(list.size());size_t debris=0;for(const auto&b:friendlyWorld(level).breakables())if(b.material!=17)debris+=6;list.reserve(list.size()+debris);
 auto shift=chunkOffset(level)-chunkOffset(m_level);
 auto physicalWorld=[&](Vec2& p)->const World&{auto local=p-shift;if(local.x>=0&&local.x<24&&local.y>=0&&local.y<24){p=local;return level==m_level?m_world:m_chunks[level].world;}return worldAt(p);};
 std::vector<std::array<float,3>> shapes;std::vector<Vec2> bases;shapes.reserve(list.size());bases.reserve(list.size());for(const auto&v:list){shapes.push_back(v.extent());bases.push_back({std::cos(v.yaw),std::sin(v.yaw)});}
 // One broad phase per resident list. Query neighbouring cells as movers cross a bucket.
 std::array<std::vector<int>,24*24> buckets;
 if(list.size()>32)for(int i=0;i<int(list.size());++i){const auto&v=list[i];if(v.model<0||v.broken)continue;auto e=shapes[i];for(int y=std::max(0,int(v.pos.y-e[1]));y<=std::min(23,int(v.pos.y+e[1]));++y)for(int x=std::max(0,int(v.pos.x-e[0]));x<=std::min(23,int(v.pos.x+e[0]));++x)buckets[y*24+x].push_back(i);}
 auto support=[&](int self,Vec2 p,float feet){float top=-100;auto consider=[&](int j){if(j==self||j==held)return;const auto&v=list[j];if(v.broken||v.model<0||v.z+v.supportHeight()>feet+.025f||!v.footprintContains(p))return;top=std::max(top,v.z+v.supportHeight());};
  if(list.size()>32){for(int y=std::max(0,int(p.y)-1);y<=std::min(23,int(p.y)+1);++y)for(int x=std::max(0,int(p.x)-1);x<=std::min(23,int(p.x)+1);++x)for(int j:buckets[y*24+x])consider(j);}else for(int j=0;j<int(list.size());++j)consider(j);return top;};
 struct RankedProp{float distance;int level,index;};std::vector<RankedProp> nearest;for(int l=0;l<chunkCount();++l)if(chunkResident(l)){auto offset=chunkOffset(l)-chunkOffset(m_level);const auto&items=chunkClutter(l);for(int i=0;i<int(items.size());++i){const auto&v=items[i];if(v.model>=0&&!v.broken&&!v.sleeping&&!dormantEntity(v.z)&&!(l==m_level&&i==m_heldClutter))nearest.push_back({lengthSq(v.pos+offset-m_player.pos),l,i});}}std::sort(nearest.begin(),nearest.end(),[](const RankedProp&a,const RankedProp&b){if(a.distance!=b.distance)return a.distance<b.distance;return a.level!=b.level?a.level<b.level:a.index<b.index;});std::vector<bool> slow(list.size());for(size_t rank=48;rank<nearest.size();++rank)if(nearest[rank].level==level)slow[nearest[rank].index]=true;
 for(int index=0;index<initialCount;++index){if(index==held)continue;auto&c=list[index];
  if(c.broken||dormantEntity(c.z))continue;
  if(c.model>=0){auto e=c.extent();auto delta=c.pos-(m_player.pos-shift);
   bool pushing=c.mass()>=8&&std::fabs(delta.x)<e[0]+.27f&&std::fabs(delta.y)<e[1]+.27f&&c.z<m_player.z+m_player.hullHeight()&&c.z+c.height()>m_player.z&&(m_velocity.x*m_velocity.x+m_velocity.y*m_velocity.y>.001f||m_propPushAttempt);
   if(pushing){c.blockedTime+=dt;Vec2 n=length(delta)>.001f?delta*(1/length(delta)):Vec2{1,0};c.velocity+=n*(dt*4/std::sqrt(c.mass()));c.sleeping=false;c.soundOwner=-1;}else if(std::fabs(delta.x)>e[0]+.4f||std::fabs(delta.y)>e[1]+.4f)c.blockedTime=0;
   float nx=e[0]+.2f-std::fabs(delta.x),ny=e[1]+.2f-std::fabs(delta.y);
   if(nx>0&&ny>0&&c.z<m_player.z+m_player.hullHeight()&&c.z+c.height()>m_player.z){Vec2 n=nx<ny?Vec2{delta.x<0?-1.f:1.f,0}:Vec2{0,delta.y<0?-1.f:1.f};auto next=c.pos+n*(std::min(nx,ny)+.001f);auto fit=[&](Vec2 p){for(auto offset:c.footprintOffsets()){auto q=p+offset+shift;const auto&w=physicalWorld(q);if(!w.fits(q.x,q.y,c.z,c.height(),true,true)||w.doorBlocks(q.x,q.y,c.z,c.height()))return false;}return true;};if(fit(next))c.pos=next;else{Vec2 side=n.x!=0?Vec2{0,delta.y<0?-1.f:1.f}:Vec2{delta.x<0?-1.f:1.f,0};next=c.pos+side*((n.x!=0?ny:nx)+.001f);if(fit(next)){c.pos=next;n=side;}}c.soundOwner=-1;float speed=dot(m_velocity,n)*std::min(1.f,4/c.mass())+.2f;c.velocity+=n*std::max(0.f,speed-dot(c.velocity,n));c.sleeping=false;c.restTime=0;}
  }else if(length(m_velocity)>.5f&&length(c.pos-(m_player.pos-shift))<.24f+c.footprint()*.5f&&c.z<m_player.z+1&&c.z+c.height()>m_player.z){c.velocity=m_velocity*.7f;c.pitchSpeed=5;c.rollSpeed=2;c.sleeping=false;c.restTime=0;}
  if(c.model>=0){auto probe=c.pos+shift;const auto&under=physicalWorld(probe);if(probe.x<0||probe.x>=24||probe.y<0||probe.y>=24||!chunkResident(under.level())){c.sleeping=true;c.restTime=-1;continue;}if(c.restTime<0){c.sleeping=false;c.restTime=0;}}
  if(c.model>=0){auto e=c.extent();
   auto kick=[&](Vec2 actor,float z,float height,Vec2 velocity,int owner){auto delta=c.pos-actor;float nx=e[0]+.22f-std::fabs(delta.x),ny=e[1]+.22f-std::fabs(delta.y);if(nx<=0||ny<=0||c.z>=z+height||c.z+c.height()<=z||lengthSq(velocity)<.001f)return;Vec2 n=nx<ny?Vec2{delta.x<0?-1.f:1.f,0}:Vec2{0,delta.y<0?-1.f:1.f};float speed=std::max(.25f,dot(velocity,n)*std::min(1.f,4/c.mass()));c.velocity+=n*std::max(0.f,speed-dot(c.velocity,n));c.soundOwner=owner;c.sleeping=false;c.restTime=0;};
   for(size_t a=0;a<m_friendlyActors.size();++a){const auto&npc=m_friendlyActors[a];if(npc.pose.clip!=1||!autonomousActor(npc.track())||npcHeld(a))continue;auto offset=chunkOffset(npc.level)-chunkOffset(level);Vec2 velocity{std::sin(npc.pose.yaw),std::cos(npc.pose.yaw)};if(npc.pose.clip!=1)velocity={};kick(npc.pose.position+offset,npc.pose.z,friendlyHullHeight(a,npc.track()),velocity,int(a));}
   const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;for(const auto&e:enemies)if(e.alive&&e.moving)kick(e.pos,e.bodyBottom(),e.bodyTop()-e.bodyBottom(),{std::cos(e.heading),std::sin(e.heading)},-1);
   if(m_vrInputActive&&level==m_level)for(int hand=0;hand<2;++hand)if(m_vrHandValid[hand]){auto p=m_vrHandPosition[hand],v=m_vrHandVelocity[hand];if(p.z>=c.z-.08f&&p.z<=c.z+c.height()+.08f&&std::fabs(p.x-c.pos.x)<e[0]+.08f&&std::fabs(p.y-c.pos.y)<e[1]+.08f){knockClutter(level,index,p,v*(std::min(2.f,c.mass())*.35f));c.soundOwner=-1;}}
  }
  if(c.model>=0){const auto&w=level==m_level?m_world:m_chunks[level].world;auto e=c.extent();
   for(const auto&door:w.doors()){if(!door.opening&&door.open<=0)continue;const Vec2 centre=door.centre();float base=w.floorHeight(centre.x,centre.y)+door.z;if(c.z>=base+2.5f||c.z+c.height()<=base)continue;
    Vec2 axis=door.swinging?door.leafAxis(door.open):door.tangent(),hinge=door.swinging?door.hinge():door.world(door.left,door.y),nearest=hinge+axis*std::clamp(dot(c.pos-hinge,axis),0.f,door.width());auto d=c.pos-nearest;float radius=std::max(e[0],e[1])+.16f;
    if(lengthSq(d)>=radius*radius||!w.doorBlocks(nearest.x,nearest.y,c.z,c.height()))continue;Vec2 normal=lengthSq(d)>.00001f?d*(1/length(d)):Vec2{-axis.y,axis.x};auto next=nearest+normal*(radius+.02f);
    bool clear=true;for(auto offset:c.footprintOffsets())clear&=w.fits(next.x+offset.x,next.y+offset.y,c.z,c.height(),true,true)&&!w.doorBlocks(next.x+offset.x,next.y+offset.y,c.z,c.height());
    if(clear)c.pos=next;c.velocity+=normal*.8f;c.sleeping=false;c.restTime=0;
   }
  }
  if(c.sleeping)continue;
  if(slow[index]&&m_propHalfStep)continue;float itemDt=dt*(slow[index]?2:1);int steps=c.model>=0&&lengthSq(c.velocity)+c.vz*c.vz<1?1:std::max(1,int(std::ceil(itemDt*(slow[index]?90:180))));float step=itemDt/steps;
  auto impact=[&](float speed){if(speed<.65f||c.impactCooldown>0)return;float gain=std::clamp(speed*.12f,.12f,.85f);if(c.model<0&&c.impactSound()==Sound::JunkSoft)gain*=.65f;
   const auto&t=propMaterialTraits(c.material());SoundEvent event{c.impactSound(),c.pos+shift,gain,c.model>=0?t.impactPitch:.94f+.1f*std::fabs(std::sin(c.yaw+index)),true,17,c.z+c.height()*.5f};if(c.model>=0){event.gain*=t.impactGain;event.owner=c.soundOwner;event.aiRadius=std::clamp(2.f+std::sqrt(c.mass())*speed,2.f,12.f);if(t.breakSpeed>0&&speed>t.breakSpeed)breakClutter(level,index,{c.velocity.x,c.velocity.y,c.vz});}m_sounds.push_back(event);c.impactCooldown=.075f;
  };
  for(int i=0;i<steps;++i){
   if(c.model>=0&&(lengthSq(c.velocity)>.0001f||std::fabs(c.vz)>.05f))for(auto&v:list)if(&v!=&c&&v.model>=0&&!v.broken&&v.sleeping&&std::fabs(v.z-c.z-c.supportHeight())<.03f&&length(v.pos-c.pos)<(v.footprint()+c.footprint())*.7f){v.sleeping=false;v.restTime=0;}
   c.impactCooldown=std::max(0.f,c.impactCooldown-step);
   auto oldExtent=c.extent();float center=c.z+oldExtent[2],oldPitch=c.pitch,oldRoll=c.roll,oldYaw=c.yaw;
   c.pitch=wrapAngle(c.pitch+c.pitchSpeed*step);c.roll=wrapAngle(c.roll+c.rollSpeed*step);c.yaw=wrapAngle(c.yaw+c.spin*step);
   auto extent=c.extent();c.z=center-extent[2];
   auto fits=[&](Vec2 p,float z){for(auto offset:c.footprintOffsets()){auto local=p+offset+shift;const auto&w=physicalWorld(local);if(!w.fits(local.x,local.y,z,extent[2]*2,true,true)||w.doorBlocks(local.x,local.y,z,extent[2]*2))return false;}return true;};
   auto local=c.pos+shift;const auto&supportWorld=physicalWorld(local);
   float floor=supportWorld.supportBelow(local.x,local.y,std::max(c.z,center-oldExtent[2])+.025f,c.model>=0);
   if((c.model<0||c.pitch!=oldPitch||c.roll!=oldRoll||c.yaw!=oldYaw)&&!fits(c.pos,std::max(c.z,floor))){c.pitch=oldPitch;c.roll=oldRoll;c.yaw=oldYaw;c.pitchSpeed*=-.2f;c.rollSpeed*=-.2f;c.spin*=-.2f;extent=oldExtent;c.z=center-extent[2];}
   if(c.model>=0&&c.material()==PropMaterial::Paper){float drag=std::exp(-3.f*step);c.velocity=c.velocity*drag;c.vz*=std::exp(-5.f*step);c.pitchSpeed*=drag;c.rollSpeed*=drag;}
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
   for(auto offset:c.footprintOffsets()){auto sample=c.pos+offset;auto p=sample+shift;const auto&w=physicalWorld(p);floor=std::max(floor,w.supportBelow(p.x,p.y,std::max(c.z,center-oldExtent[2])+.025f,c.model>=0));if(c.model>=0)floor=std::max(floor,support(index,sample,std::max(c.z,center-oldExtent[2])));ceiling=std::min(ceiling,w.clearanceAbove(p.x,p.y,c.z,c.model>=0));}
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
   if(c.model>=0&&c.projectile)for(size_t a=0;a<m_friendlyActors.size();++a){const auto&npc=m_friendlyActors[a];auto p=npc.pose.position+chunkOffset(npc.level)-chunkOffset(level);if(npc.pose.clip==4||lengthSq(p-c.pos)>.3f*.3f||c.z>=npc.pose.z+friendlyHullHeight(a,npc.track())||c.z+c.height()<=npc.pose.z)continue;auto velocity=c.velocity;float speed=length(velocity);Point3 dir{velocity.x/std::max(.001f,speed),velocity.y/std::max(.001f,speed),0},at{c.pos.x+shift.x,c.pos.y+shift.y,c.z+c.height()*.5f};resolveShot({ShotKind::Npc,npc.level,int(a),1,1,at,{}},at-dir,dir,kGuardHandgun.hitscan,true,5,{ShotKind::Player,m_level,0});c.projectile=false;c.velocity=c.velocity*-.18f;break;}
   if(c.projectile){auto point=c.pos+shift;const auto&contactWorld=physicalWorld(point);int targetLevel=contactWorld.level();auto&enemies=targetLevel==m_level?m_enemies:m_chunks[targetLevel].enemies;
    for(auto&e:enemies)if(e.alive&&length(e.pos-point)<.5f&&c.z+(extent[2]*2)>e.bodyBottom()&&c.z<e.bodyTop()){
     impact(length(c.velocity));e.awareness=6;e.recognition=1;e.lastKnown=point;e.lastKnownZ=contactWorld.supportBelow(point.x,point.y,c.z+extent[2]);e.state=Enemy::State::Investigate;c.projectile=false;c.velocity=c.velocity*-.18f;m_sounds.push_back({Sound::PunchHit,c.pos+shift,.6f,1,true,17,c.z+extent[2]});
     if(applyEnemyDamage(e,kUnarmedMelee.clutterDamage,false,false,targetLevel)){auto audible=e;audible.pos+=chunkOffset(targetLevel)-chunkOffset(m_level);enemySound(audible,2);}break;
    }
   }
  }
  if(c.broken){c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=true;c.restTime=0;}
  shapes[index]=c.extent();bases[index]={std::cos(c.yaw),std::sin(c.yaw)};
 }
 if(std::none_of(list.begin(),list.end(),[](const Clutter&c){return c.model>=0&&!c.broken&&!c.sleeping;}))return;
 shapes.resize(list.size());for(size_t i=0;i<list.size();++i)shapes[i]=list[i].extent();std::vector<int> seen(list.size(),-1),candidates;
 for(int i=0;i<int(list.size());++i){auto&a=list[i];if(a.model<0||a.broken||i==held)continue;
  candidates.clear();auto ae=shapes[i];if(list.size()>32){for(int y=std::max(0,int(a.pos.y-ae[1])-1);y<=std::min(23,int(a.pos.y+ae[1])+1);++y)for(int x=std::max(0,int(a.pos.x-ae[0])-1);x<=std::min(23,int(a.pos.x+ae[0])+1);++x)for(int j:buckets[y*24+x])if(j>i&&seen[j]!=i){seen[j]=i;candidates.push_back(j);}}else for(int j=i+1;j<int(list.size());++j)candidates.push_back(j);
  for(int j:candidates){auto&b=list[j];if(b.model<0||b.broken||j==held||(a.sleeping&&b.sleeping)||std::min(a.z+a.supportHeight(),b.z+b.supportHeight())-std::max(a.z,b.z)<=.02f)continue;
   auto be=shapes[j];Vec2 d=b.pos-a.pos;if(std::fabs(d.x)>ae[0]+be[0]||std::fabs(d.y)>ae[1]+be[1])continue;
   bool flat=std::fabs(a.pitch)+std::fabs(a.roll)+std::fabs(b.pitch)+std::fabs(b.roll)<.1f;float penetration=100;Vec2 normal{};bool overlap=true;
   Vec2 ax{std::cos(a.yaw),std::sin(a.yaw)},ay{-ax.y,ax.x},bx{std::cos(b.yaw),std::sin(b.yaw)},by{-bx.y,bx.x};
   for(Vec2 n:flat?std::array<Vec2,4>{ax,ay,bx,by}:std::array<Vec2,4>{Vec2{1,0},Vec2{0,1},Vec2{1,0},Vec2{0,1}}){float ra=flat?(std::fabs(dot(ax,n))*a.dims[0]+std::fabs(dot(ay,n))*a.dims[1])*.5f:std::fabs(n.x)*ae[0]+std::fabs(n.y)*ae[1];float rb=flat?(std::fabs(dot(bx,n))*b.dims[0]+std::fabs(dot(by,n))*b.dims[1])*.5f:std::fabs(n.x)*be[0]+std::fabs(n.y)*be[1];float p=ra+rb-std::fabs(dot(d,n));if(p<=0){overlap=false;break;}if(p<penetration){penetration=p;normal=n*(dot(d,n)<0?-1.f:1.f);}}
   if(!overlap)continue;float ia=a.sleeping&&a.mass()>4*b.mass()?0:1/a.mass(),ib=b.sleeping&&b.mass()>4*a.mass()?0:1/b.mass(),total=ia+ib;if(total==0)continue;
   auto move=[&](Clutter&v,Vec2 delta){auto e=v.extent();for(auto offset:v.footprintOffsets()){auto p=v.pos+delta+offset+shift;const auto&w=physicalWorld(p);if(!w.fits(p.x,p.y,v.z,e[2]*2,true,true)||w.doorBlocks(p.x,p.y,v.z,e[2]*2))return;}auto p=v.pos+delta+shift;const auto&w=physicalWorld(p);if(w.railBlocksHull(p.x,p.y,std::max(e[0],e[1]),v.z,e[2]*2))return;v.pos+=delta;if(lengthSq(delta)>.000001f){v.sleeping=false;v.restTime=0;}};
   move(a,normal*(-penetration*ia/total));move(b,normal*(penetration*ib/total));int owner=std::fabs(dot(a.velocity,normal))>=std::fabs(dot(b.velocity,normal))?a.soundOwner:b.soundOwner;float closing=dot(a.velocity-b.velocity,normal);if(closing>.65f&&a.impactCooldown<=0){const auto&t=propMaterialTraits(a.material());SoundEvent event{t.impact,a.pos+shift,std::clamp(closing*.12f,.12f,.85f)*t.impactGain,t.impactPitch,true,17,a.z+shapes[i][2]};event.owner=owner;event.aiRadius=std::clamp(2+std::sqrt(a.mass())*closing,2.f,12.f);m_sounds.push_back(event);a.impactCooldown=.075f;}
   if(closing>0){a.soundOwner=b.soundOwner=owner;float impulse=closing*(1+std::min(propMaterialTraits(a.material()).restitution,propMaterialTraits(b.material()).restitution))/total;a.velocity=a.velocity-normal*(impulse*ia);b.velocity+=normal*(impulse*ib);if(impulse*ia>.05f){a.sleeping=false;a.restTime=0;}if(impulse*ib>.05f){b.sleeping=false;b.restTime=0;}
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
 for(int variant=0;variant<4;++variant){auto m=regression::room();kit::workstation(*m,8,8,0,1,variant,true);Game workstation(regression::campaign(m));check(workstation.m_clutter.size()==size_t(3+(variant%2!=0)+(variant%3==0)+(variant%4==1))&&std::all_of(workstation.m_clutter.begin(),workstation.m_clutter.end(),[](const Clutter&c){return c.sleeping;}),"loose workstation preserves its original desktop set and supports the physical floor tower");}
 {auto next=regression::room();next->definition.origin={24,0};auto campaign=regression::campaign(regression::room());campaign->maps.push_back(next);Game resident(campaign);resident.ensureChunk(1);Enemy enemy;enemy.pos=enemy.home=enemy.lastKnown={10,10};resident.m_chunks[1].enemies={enemy};Clutter c;c.kind=7;c.model=model::keyboard;c.dims=propTraits(c.model).size;c.pos={9,10};c.z=.1f;c.velocity={12,0};c.projectile=true;resident.m_chunks[1].clutter={c};for(int i=0;i<120;++i)resident.updateClutter({},1.f/120);check(resident.m_chunks[1].enemies[0].hp==105&&!resident.m_chunks[1].clutter[0].projectile,"resident-neighbour thrown prop hits its creature once for legacy five damage");}
 {Game held(regression::campaign(regression::room()));Clutter c;c.kind=7;c.model=model::crt_monitor;c.dims=propTraits(c.model).size;c.pos={8,8};c.z=.75f;held.m_clutter={c};held.m_heldClutter=0;check(held.traceShot({6,8,.9f},{1,0,0},10).kind==ShotKind::Prop,"held monitor retains its physical bullet-blocking box");held.m_clutter[0].model=model::mug;held.m_clutter[0].dims=propTraits(model::mug).size;held.breakClutter(0,0,{1,0,0});Game loaded(held.m_customCampaign);check(!held.holdingClutter()&&loaded.decodeSave(held.encodeSave())&&loaded.m_clutter[0].broken,"breaking a held prop drops it and leaves a loadable tombstone");}
 for(bool held:{false,true})for(bool broken:{false,true}){auto first=regression::room(),second=regression::room();first->openEast=true;first->layers[0].rows[8][23]='.';second->definition.origin={24,0};second->openWest=true;second->layers[0].rows[8][0]='.';kit::prop(*first,model::mug,22.5f,8,0,0);kit::prop(*second,model::mug,22.5f,8,0,0);auto campaign=regression::campaign(first);campaign->maps.push_back(second);Game g(campaign);g.m_clutter[0].pos={24.1f,8};g.m_player.pos={24.1f,8};if(held)g.m_heldClutter=0;g.crossChunkBoundary();int moved=-1;for(int i=0;i<int(g.m_clutter.size());++i)if(std::fabs(g.m_clutter[i].home.x-22.5f)<.01f)moved=i;if(moved>=0&&broken)g.breakClutter(1,moved,{});auto bytes=g.encodeSave();Game loaded(campaign);bool valid=moved>=0&&loaded.decodeSave(bytes);check(valid&&loaded.level()==1&&loaded.m_clutter.size()==2&&loaded.chunkClutter(0).empty()&&loaded.holdingClutter()==(held&&!broken)&&loaded.encodeSave()==bytes,"cross-seam authored live/held/tombstone identity roundtrips without desk respawn");}
 check(testPropSaves(),"v16 migration, held prop save, and torn leads kept by v21 and authored from v19/v20");
 for(float dt:{1.f/60,1.f/120}){auto stacked=regression::room();kit::prop(*stacked,model::binder,10,10,0,.3f,.32f,.08f,0);kit::prop(*stacked,model::mug,10,10,.08f,0);Game stack(regression::campaign(stacked));stack.m_player.pos={3,3};stack.knockClutter(0,0,{10,10,.04f},{3,0,0});for(int i=0;i<int(5/dt);++i)stack.updateClutter({},dt);const auto&a=stack.m_clutter[0];const auto&b=stack.m_clutter[1];auto ae=a.extent(),be=b.extent();float overlap=std::min({ae[0]+be[0]-std::fabs(a.pos.x-b.pos.x),ae[1]+be[1]-std::fabs(a.pos.y-b.pos.y),std::min(a.z+a.height(),b.z+b.height())-std::max(a.z,b.z)});check(a.sleeping&&b.sleeping&&!b.broken&&a.z>=-.001f&&b.z>=-.001f&&overlap<.01f,"knocked stack wakes and settles without interpenetration at 60/120 Hz");}
 {Game carry(regression::campaign(regression::room()));carry.m_player.pos={6,6};carry.m_player.angle=0;Clutter c;c.kind=7;c.model=model::crt_monitor;c.dims={.42f,.4f,.38f};c.pos={7,6};c.z=.75f;carry.m_clutter={c};check(carry.interactClutter()&&carry.holdingClutter(),"11 kg monitor can be lifted");InputState fire;fire.fire=true;carry.updateClutter(fire,1.f/120);check(!carry.holdingClutter()&&length(carry.m_clutter[0].velocity)<5,"11 kg monitor throw is under 5 m/s");c.model=0;c.dims={1,1,1};carry.m_clutter={c};carry.m_heldClutter=-1;check(!carry.interactClutter()&&!carry.holdingClutter()&&carry.m_clutter[0].mass()==30,"30 kg uncurated box cannot be lifted");}
 {auto neighbour=regression::room();neighbour->definition.origin={24,0};neighbour->layers[0].rows[10][10]='B';auto multi=regression::campaign(regression::room());multi->maps.push_back(neighbour);Game n(multi);n.ensureChunk(1);Clutter c;c.kind=7;c.model=model::keyboard;c.dims={.45f,.16f,.03f};c.pos={10,10};c.z=2;n.m_chunks[1].clutter={c};n.detonateBarrel(1,10,10);for(int i=0;i<600;++i)n.updateClutter({},1.f/120);check(n.m_chunks[1].clutter.size()==7&&std::all_of(n.m_chunks[1].clutter.begin(),n.m_chunks[1].clutter.end(),[](const Clutter&v){return v.sleeping&&v.z<.02f;}),"resident neighbour prop and actual barrel debris land and sleep");
 check(std::all_of(n.m_chunks[1].clutter.begin()+1,n.m_chunks[1].clutter.end(),[](const Clutter&v){return v.kind==6&&v.shardOf==ShardOf::Barrel&&v.model<0;}),"a barrel is torn into pieces of its own hull, not unrelated junk");}
 report<<(ok?"ALL PASSED props":"FAILED props")<<'\n';return ok;
}
bool Game::testClutter(){
 {auto vr=validationScene(Enemy::Kind::Huntsman);vr.m_clutter={{{4.2f,4.5f},{},.56f}};vr.m_clutter[0].kind=2;
  vr.setVrHand(0,{4.2f,4.5f,.7f},{1,0,0},true);vr.setVrHand(1,{}, {},false);
  if(!vr.interactClutter()||!vr.holdingClutter())return false;vr.updateClutter({},.01f);
  if(!vr.holdingClutter()||std::fabs(vr.m_clutter[0].pos.x-4.32f)>.001f||std::fabs(vr.m_clutter[0].z+vr.m_clutter[0].height()*.5f-.7f)>.001f)return false;
  vr.setVrHand(0,{}, {},false);vr.updateClutter({},.01f);if(vr.holdingClutter())return false;
  auto pose=vr.m_player;int ammo=pose.pool(WeaponId::Shotgun).total;vr.m_enemies[0].pos={5.2f,4.5f};vr.setVrAim({3.8f,4.5f,.6f},{1,0,0},true);vr.shoot();
  if(vr.m_enemies[0].hp!=76||vr.m_player.pool(WeaponId::Shotgun).total!=ammo-1||vr.m_player.pos.x!=pose.pos.x||vr.m_player.z!=pose.z||vr.m_player.angle!=pose.angle||vr.m_player.pitch!=pose.pitch)return false;
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
Game Game::leadInspection(int view,float* tearAge){
 // A test bay of its own, so the evidence never moves with a campaign map: a bench against a
 // wall, the test set on it turned to a charged cell, red and black test leads from its sockets
 // to the cell's posts (kit::testLead). Views: 0 the panel close, 1 the cell's terminals close,
 // 2 the bench, 3 the cell lifted, turned and tipped inside the slack, 4 a plug pulling off,
 // 5 both off and swinging, 6 the leads hanging still from the set, their plugs on their ends and
 // the pilot dark, 7 the cell set back on the bench, its posts bare, 8 view 6 saved and loaded (the
 // torn leads hung again), 9 the bay opening with its black lead already off (kit::looseTestLead).
 auto m=regression::room();m->name="Lead inspection";m->definition.playerStart={8,10};
 m->structures.push_back({5,7.8f,11,8,0,3,false,8});
 m->fixtures.push_back({model::lab_clean_bench,{8,8.28f},0,1.18f,.56f,.81f,0,true});
 const float face=.55f;m->fixtures.push_back({model::lab_field_meter,{7.72f,8.3f},.81f,.25f,.328f,.148f,std::atan2(-std::cos(face),-std::sin(face)),false});
 kit::prop(*m,model::anl_cell_charged,8.3f,8.32f,.81f,-kPi*.5f);m->lights.push_back(kit::lamp({8.1f,9.f},2.9f,1.25f,7,4300));m->lights.push_back(kit::lamp({6.4f,10.6f},2.9f,.7f,6,3600));
 kit::testLead(*m,1,"+",0,"+",1.f,true,stateId("bay_lead_red"));
 if(view==9)kit::looseTestLead(*m,1,"-",{7.95f,9.3f,0},1.1f);else kit::testLead(*m,1,"-",0,"-",1.1f,true,stateId("bay_lead_black"));
 Game g(regression::campaign(m));g.m_weaponEquipped=false;g.m_holster=1;
 auto tick=[&](int n){for(int i=0;i<n;++i){g.m_elapsed+=1.f/120;g.updateClutter({},1.f/120);}};
 auto look=[&](Point3 eye,Point3 at){g.m_player.pos={eye.x,eye.y};g.m_player.z=0;g.m_player.eye=eye.z;const auto d=at-eye;g.m_player.angle=std::atan2(d.y,d.x);g.m_player.pitch=std::atan2(d.z,std::sqrt(d.x*d.x+d.y*d.y))*140;};
 const auto&leads=g.world().leads();const auto&pilot=g.world().lights()[size_t(leads[0].lamp)];
 const Point3 sockets=(leads[0].anchor+leads[1].anchor)*.5f,lens{pilot.position.x,pilot.position.y,pilot.z},lid{8.3f,8.32f,1.21f};
 // The still views settle first and only then place the camera, so its body never nudges the cell.
 if(view==0){tick(30);const auto at=sockets*.6f+lens*.4f;look(at+Point3{std::cos(face)*.3f,std::sin(face)*.3f,.24f},at);return g;}
 if(view==1){tick(30);look(lid+Point3{.17f,.2f,.21f},lid);return g;}
 if(view==2){tick(30);look({8.12f,9.42f,1.42f},{8.f,8.34f,.92f});return g;}
 // The hanging views take in the set on the bench and its face down to the floor, where a plug may come to rest.
 const Point3 hangEye{8.55f,9.7f,1.3f},hangAt{8.f,8.55f,.45f};
 if(view==9){tick(30);look(hangEye,hangAt);return g;}
 // Lifted at the bench, turned and tipped in her hands, and carried back inside the leads' slack...
 g.m_player.pos={8.3f,9.4f};g.m_player.angle=-kPi*.5f;g.m_player.pitch=-40;const auto&ropes=g.m_world.leadRopes();
 g.m_heldClutter=g.leadProp(0,leads[0]);if(g.m_heldClutter<0)return g;
 tick(60);for(int i=0;i<60;++i){const float t=float(i+1)/60;auto&c=g.m_clutter[size_t(g.m_heldClutter)];g.m_player.angle=-kPi*.5f+.25f*t;c.pitch=.3f*t;c.roll=-.2f*t;g.m_player.pos.y+=.1f/60;tick(1);}
 if(view==3){tick(30);return g;}
 // ...then back until a plug pulls off its post: the tear a few frames in, both off and swinging
 // just under half a second after the second goes, and the cell set back on the bench, its
 // posts bare, the leads hanging still from the set with their plugs four and a half seconds on.
 auto walk=[&](int n){for(int i=0;i<n;++i){g.m_player.pos.y+=.5f/120;tick(1);}};
 for(int i=0;i<720&&!ropes[0].torn&&!ropes[1].torn;++i)walk(1);
 const float tore=g.m_elapsed;walk(6);
 // The tear clip (--lead-clip): view 100+k is view 4 run on k/30 s, walking back until both go, then still.
 if(view>=100){for(int i=0;i<(view-100)*4;++i)if(!(ropes[0].torn&&ropes[1].torn))walk(1);else tick(1);if(tearAge)*tearAge=g.m_elapsed-tore;return g;}
 if(view>=5){for(int i=0;i<120&&!(ropes[0].torn&&ropes[1].torn);++i)walk(1);tick(54);}
 if(view>=6){auto&c=g.m_clutter[size_t(g.m_heldClutter)];g.m_heldClutter=-1;c.pos={8.45f,8.4f};c.z=.81f;c.pitch=c.roll=0;c.yaw=-kPi*.5f;c.velocity={};c.vz=0;c.sleeping=false;c.restTime=0;
  tick(540);if(view==8){const auto bytes=g.encodeSave();if(!g.decodeSave(bytes))return g;}
  if(view!=7)look(hangEye,hangAt);else look(Point3{8.45f,8.4f,1.21f}+Point3{.1f,.25f,.2f},{8.45f,8.4f,1.21f});}
 if(tearAge)*tearAge=g.m_elapsed-tore;
 return g;
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
 bool alongX=b.x2-b.x1>b.y2-b.y1;PropMaterial material=b.material==17?PropMaterial::Glass:b.material==22?PropMaterial::Plastic:PropMaterial::Wood;m_breakEvents.push_back({center,b.bottom,direction,m_elapsed,level,material,std::max(b.x2-b.x1,b.y2-b.y1),b.top-b.bottom,true,alongX?Vec2{1,0}:Vec2{0,1},index});if(m_breakEvents.size()>24)m_breakEvents.erase(m_breakEvents.begin());
 if(b.material==17){std::erase_if(m_glassBreaks,[&](const GlassBreak&g){return g.level==level&&g.index==index;});m_glassBreaks.push_back({level,index,at,direction});}
 SoundEvent event{b.material==17?Sound::GlassShatter:b.material==22?Sound::PlasterBreak:Sound::WoodBreak,center+shift,1,1,true,20,(b.bottom+b.top)*.5f};event.aiRadius=14;m_sounds.push_back(event);
 // A drop-ceiling tile falls in slabs of itself; boarding (10, 23) splinters as a crate does.
 if(b.material==22){auto&debris=level==m_level?m_clutter:m_chunks[level].clutter;for(auto&c:Clutter::shards(ShardOf::Ceiling,{center.x,center.y,b.bottom},{b.x2-b.x1,b.y2-b.y1,b.top-b.bottom},0,6,.6f,-.5f))debris.push_back(c);}
 else if(b.material!=17){auto&debris=level==m_level?m_clutter:m_chunks[level].clutter;for(int n=0;n<6;++n){Clutter c;c.kind=6;c.pos=center;c.z=b.material==22?b.bottom:(b.bottom+b.top)*.5f;c.velocity={std::cos(n*kPi/3)*1.5f,std::sin(n*kPi/3)*1.5f};c.vz=b.material==22?-1.f:2.f;debris.push_back(c);}}
 if(b.state)setState(b.state,1);
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;for(auto&c:list){auto e=c.extent();if(c.pos.x+e[0]>=b.x1-.1f&&c.pos.x-e[0]<=b.x2+.1f&&c.pos.y+e[1]>=b.y1-.1f&&c.pos.y-e[1]<=b.y2+.1f&&c.z+c.height()>=b.bottom-.1f&&c.z<=b.top+.1f&&!c.broken){c.sleeping=false;c.restTime=0;}}
 return true;
}
void Game::updateBreakables(float dt){
 for(int level=0;level<chunkCount();++level){auto&w=level==m_level?m_world:m_chunks[level].world;w.ageBreakables(dt);
  m_bulletImpacts.erase(std::remove_if(m_bulletImpacts.begin(),m_bulletImpacts.end(),[&](const BulletImpact&p){if(p.level!=level)return false;int i=w.breakableAt(p.pos.x,p.pos.y,p.z,.04f);return i>=0&&w.breakables()[i].holds&&w.breakableOpacity(i)==0;}),m_bulletImpacts.end());
  for(int i=0;i<int(w.breakables().size());++i){const auto b=w.breakables()[i];if(!w.breakableIntact(i)){if(b.state)setState(b.state,1);continue;}if(b.holds)continue;
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
 {const float W=2.4f,H=2.2f;const Vec2 hit{.7f,1.3f};const auto shards=glass::fracture(W,H,hit,1234);float area=0,widest=0,firstLate=1e9f,lastEarly=0;bool centred=false;
  for(const auto&s:shards){area+=s.shape.area();float lo=1e9f,hi=-1e9f,bottom=1e9f,top=-1e9f;for(int k=0;k<s.shape.n;++k){const auto p=s.shape.p[size_t(k)];lo=std::min(lo,p.x);hi=std::max(hi,p.x);bottom=std::min(bottom,p.y);top=std::max(top,p.y);
    if(std::fabs(p.x-hit.x)<1e-4f&&std::fabs(p.y-hit.y)<1e-4f)centred=true;}
   widest=std::max({widest,hi-lo,top-bottom});if(s.ring<=1)lastEarly=std::max(lastEarly,s.delay);else firstLate=std::min(firstLate,s.delay);}
  check(std::fabs(area-W*H)<.02f*W*H&&shards.size()>=40&&shards.size()<=400,"a broken pane comes apart into pieces that cover it exactly");
  check(centred&&widest<.75f&&lastEarly==0&&firstLate>0,"the pieces radiate from the hit, none much past half a metre, the centre blown out first and the rest let go after");
  const auto again=glass::fracture(W,H,hit,1234);bool same=again.size()==shards.size();for(size_t i=0;same&&i<shards.size();++i)same=again[i].shape.n==shards[i].shape.n&&again[i].shape.p[0].x==shards[i].shape.p[0].x&&again[i].delay==shards[i].delay;
  check(same,"the same pane and hit always break the same way, so the shards on the floor match the ones that fell");
  float teethTop=0;for(const auto&t:glass::teeth(W,H,1234))if(t.n==3&&t.p[0].y==0&&t.p[1].y==0)teethTop=std::max(teethTop,t.p[2].y);
  check(teethTop>.05f&&teethTop<=.24f+1e-4f,"jagged teeth stay standing in the bottom of the frame, no higher than 0.24 m");
  const auto light=glass::cracks(hit,0,77),heavy=glass::cracks(hit,1,77);float reachLight=0,reachHeavy=0;
  for(const auto&c:light)reachLight=std::max(reachLight,length(c.b-hit));for(const auto&c:heavy)reachHeavy=std::max(reachHeavy,length(c.b-hit));
  check(!light.empty()&&reachHeavy>reachLight*3&&glass::cracks(hit,0,78)[0].b.x!=light[0].b.x,"each hit cracks its own way, and the cracks spread as the pane weakens");}
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
 {Game g(room(true));g.damagePane(0,0,90,{10,8,1},{1,0,0});g.updateBreakables(19);check(g.world().breakableDamage(0)==90&&g.world().breakableOpacity(0)==1,"holds stays at stage 2 at 19 seconds");auto bytes=g.encodeSave();Game loaded(room(true));check(loaded.decodeSave(bytes)&&loaded.encodeSave()==bytes,"holds heal timer survives byte-exact save");loaded.updateBreakables(1.5f);check(loaded.world().breakableOpacity(0)>.49f&&loaded.world().breakableOpacity(0)<.51f,"holds cracks fade over one second");loaded.damagePane(0,0,1,{10,8,1},{1,0,0});loaded.updateBreakables(19);check(loaded.world().breakableOpacity(0)==1,"new hit resets healing clock");loaded.updateBreakables(2);check(loaded.world().breakableDamage(0)==0&&loaded.m_bulletImpacts.empty(),"holds intact at 21 seconds with no pane decals");Game ordinary(room());ordinary.damagePane(0,0,5,{10,8,1},{1,0,0});ordinary.updateBreakables(25);check(ordinary.world().breakableDamage(0)==5,"breakable glass never heals");}
 report<<(ok?"ALL PASSED glass":"FAILED glass")<<'\n';return ok;
}
Game Game::physicsInspection(int view,float seconds){
 // A 2.4 x 2.2 m pane three rounds have cracked (seconds < 0), or `seconds` after
 // a shotgun round breaks it: at eye level (8), or the floor beyond it (9).
 if(view==8||view==9){auto m=regression::room();m->definition.playerStart={8,view==9?8.9f:6.6f};kit::breakable(*m,6.8f,9.97f,9.2f,10.03f,0,2.2f,17);m->lights.push_back({{8,8},3});m->lights.push_back({{8,12},3});
  Game g(regression::campaign(m));g.m_weaponEquipped=false;g.m_holster=1;g.m_player.angle=kPi*.5f;g.m_player.pitch=view==9?-62.f:0.f;const Point3 into{0,1,0};
  for(Point3 at:{Point3{7.5f,10,1.5f},Point3{8.6f,10,.9f},Point3{8.1f,10,1.75f}})g.damagePane(0,0,5,at,into);
  if(seconds>=0){g.damagePane(0,0,100,{7.6f,10,1.3f},into);g.m_elapsed+=seconds;}
  return g;}
 if(view==7){auto m=regression::room();m->definition.playerStart={8,6.5f};m->fixtures.push_back({model::desk,{8,8},0,1.4f,.7f,.75f,0,true});m->fixtures.push_back({model::crt_monitor,{8,8},.75f,.42f,.4f,.38f,kPi,false});kit::fixturePart(*m,1,1,7.82f,7.78f,8.18f,7.82f,.85f,1.08f,0,stateId("fixture_inspection_break"));m->lights.push_back({{8,7},3});Game g(regression::campaign(m));g.m_weaponEquipped=false;g.m_holster=1;g.m_player.angle=kPi*.5f;g.m_player.pitch=-42;if(seconds>0){g.setState(stateId("fixture_inspection_break"),1);g.updateBreakables();g.m_elapsed=seconds;}return g;}

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

namespace retro {
void Game::blastProps(Point3 origin,bool tiles){
 struct PaneHit{int level,index;float damage;Point3 point,direction;};std::vector<PaneHit> panes;
 struct TileHit{int level,x,y;char kind;};std::vector<TileHit> targets;
 auto direction=[&](Point3 p,float&falloff){auto d=p-origin;float r=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);falloff=1-r/4;return r>.001f?d*(1/r):Point3{0,0,1};};
 for(int level=0;level<chunkCount();++level)if(chunkResident(level)){auto shift=chunkOffset(level)-chunkOffset(m_level);auto&w=friendlyWorld(level);auto&list=level==m_level?m_clutter:m_chunks[level].clutter;
  for(int i=0;i<int(list.size());++i){const auto&c=list[i];if(c.model<0||c.broken)continue;Point3 local{c.pos.x,c.pos.y,c.z+c.height()*.5f},p=local+Point3{shift.x,shift.y,0};float strength;auto dir=direction(p,strength);if(strength<=0||tacticalGrenadeTrace(origin,p-dir*std::min(.12f,c.footprint()*.5f)).kind==ShotKind::Solid)continue;knockClutter(level,i,local,dir*(40*strength));damageClutter(level,i,60*strength,local,dir);}
  for(int i=0;i<int(w.breakables().size());++i)if(w.breakableIntact(i)){const auto&b=w.breakables()[i];Point3 p{std::clamp(origin.x-shift.x,b.x1,b.x2),std::clamp(origin.y-shift.y,b.y1,b.y2),std::clamp(origin.z,b.bottom,b.top)};auto global=p+Point3{shift.x,shift.y,0};float strength;auto dir=direction(global,strength);if(strength>0&&tacticalGrenadeTrace(origin,global-dir*.04f).kind!=ShotKind::Solid)panes.push_back({level,i,100*strength,p,dir});}
  if(tiles)for(int y=0;y<24;++y)for(int x=0;x<24;++x){char t=w.tile(x,y);if(t!='B'&&t!='C')continue;Point3 p{x+.5f+shift.x,y+.5f+shift.y,w.floorHeight(x+.5f,y+.5f)+.3f};float strength;auto dir=direction(p,strength);if(strength>0&&tacticalGrenadeTrace(origin,p-dir*.75f).kind!=ShotKind::Solid)targets.push_back({level,x,y,t});}
 }
 // Snapshot the clear lines before any pane falls or a secondary barrel explodes.
 for(const auto&p:panes)damagePane(p.level,p.index,p.damage,p.point,p.direction);
 for(const auto&t:targets){auto&w=friendlyWorld(t.level);if(t.kind=='B')detonateBarrel(t.level,t.x,t.y);else if(w.tile(t.x,t.y)=='C'){w.destroyTile(t.x,t.y);auto&list=t.level==m_level?m_clutter:m_chunks[t.level].clutter;for(int i=0;i<6;++i){Clutter c;c.kind=6;c.pos={t.x+.5f,t.y+.5f};c.z=w.floorHeight(c.pos.x,c.pos.y)+.3f;c.velocity={std::cos(i*kPi/3)*3,std::sin(i*kPi/3)*3};c.vz=2;list.push_back(c);}}}
}
}

namespace retro {
bool Game::testPhysicsStages(){
 std::ofstream out("physics-stages-test.txt");bool ok=true;auto check=[&](bool pass,const char*label){out<<(pass?"PASS ":"FAIL ")<<label<<'\n';out.flush();ok&=pass;};
 auto prop=[](int model,Vec2 p,float z=0){Clutter c;c.kind=7;c.model=model;c.dims=propTraits(model).size;c.pos=p;c.z=z;c.sleeping=true;return c;};
 {auto m=regression::room();kit::breakable(*m,9.97f,6,10.03f,10,0,3);Game g(regression::campaign(m));g.m_player.pos={3,3};g.m_clutter={prop(model::crt_monitor,{8,8})};g.blastProps({7,8,1},false);check(length(g.m_clutter[0].velocity)>0&&!g.world().breakableIntact(0),"blast pushes visible props and shatters a clear pane");}
 {auto m=regression::room();kit::wall(*m,7.5f,5,7.8f,11,0,4);kit::breakable(*m,9.97f,6,10.03f,10,0,3);Game g(regression::campaign(m));g.m_clutter={prop(model::crt_monitor,{8.5f,8})};g.blastProps({7,8,1},false);check(g.m_clutter[0].sleeping&&g.world().breakableIntact(0),"opaque wall shields props and glazing from a blast");}
 {auto m=regression::room();m->layers[0].rows[8][8]='C';m->layers[0].rows[8][10]='B';Game g(regression::campaign(m));g.blastProps({7,8.5f,.5f},true);check(g.world().tile(8,8)!='C',"grenade destroys a crate on its clear line");g.blastProps({9.5f,8.5f,.5f},true);check(g.world().tile(10,8)!='B',"grenade detonates a barrel");}
 {auto m=regression::room();kit::breakable(*m,9.97f,5,10.03f,11,0,3);Game g(regression::campaign(m));check(g.soundRayClear({8,8},1,{12,8},1)&&!g.hearingRayClear({8,8},1,{12,8},1),"intact glass muffles hearing without blocking sight");g.damagePane(0,0,100,{10,8,1},{1,0,0});check(g.hearingRayClear({8,8},1,{12,8},1),"broken glass stops muffling hearing");}
 {auto m=regression::room();m->fixtures.push_back({model::desk,{8,8},0,1.4f,.7f,.75f,0,true});World w(0,m);check(w.fits(8,8,0,.3f,true,true)&&!w.fits(8,8,0,.3f,true,false)&&std::fabs(w.clearanceAbove(8,8,0,true)-.685f)<.001f,"props enter table leg space while player collision stays solid");check(std::fabs(w.supportBelow(8,8,.8f,true)-.75f)<.001f,"profile preserves tabletop support");}
 {Game g(regression::campaign(regression::room()));g.m_player.pos={7.5f,8};g.m_player.z=0;g.m_clutter={prop(model::crt_monitor,{8,8})};check(!g.hullFits({7.7f,8},0,1.6f)&&g.hullFits({7.4f,8},0,1.6f),"8 kg and heavier props block approach and always permit retreat");g.m_player.angle=0;InputState push;push.forward=true;for(int i=0;i<60;++i){g.updateMovement(push,1.f/60);g.simulateClutter(g.m_clutter,0,1.f/60,-1);}check(g.m_clutter[0].blockedTime>=.5f||g.m_clutter[0].pos.x>8.1f,"sustained pushing yields or releases a heavy prop within half a second");}
 {auto m=regression::room();auto a=regression::guard({8,8},{8,10});a.ai.squad=stateId("kick_squad");m->actorTracks.push_back(a);Game g(regression::campaign(m));g.m_player.pos={3,3};g.m_friendlyActors[0].pose.clip=1;g.m_clutter={prop(model::keyboard,{8,8.15f})};g.simulateClutter(g.m_clutter,0,1.f/60,-1);check(!g.m_clutter[0].sleeping&&g.m_clutter[0].soundOwner==0,"walking guard kicks a prop with an NPC sound owner");g.m_clutter={prop(model::keyboard,{8,8})};g.m_vrInputActive=true;g.m_vrHandValid[0]=true;g.m_vrHandPosition[0]={8,8,.015f};g.m_vrHandVelocity[0]={2,0,0};g.simulateClutter(g.m_clutter,0,1.f/60,-1);check(g.m_clutter[0].velocity.x>0,"tracked VR hand pushes a nearby prop");}
 {auto m=regression::room();auto guard=regression::guard({9,8},{9,8});guard.ai.stance=stateId("prop_truce");m->actorTracks.push_back(guard);Game g(regression::campaign(m));g.setState(stateId("prop_truce"),int(SquadStance::Truce));g.m_player.pos={3,3};auto c=prop(model::keyboard,{8.8f,8},.9f);c.sleeping=false;c.projectile=true;c.velocity={4,0};g.m_clutter={c};g.simulateClutter(g.m_clutter,0,1.f/60,-1);check(!g.m_clutter[0].projectile&&g.state(stateId("reg_guard_damage"))>0,"thrown model prop damages a guard through the normal threat path");check(g.state(stateId("prop_truce"))==int(SquadStance::Hostile),"a thrown prop hitting a man ends his squad truce");}
 {auto m=regression::room();kit::boardedGap(*m,9.97f,6,10.03f,10,0,3,stateId("board_broken"));kit::ceilingTiles(*m,6,6,7.2f,7.2f,3);kit::glassCabinet(*m,14,8,0,1,.6f,1.6f);auto campaign=regression::campaign(m);Game g(campaign);check(!g.world().fits(10,8,0,1.6f),"plywood partition initially blocks movement");g.damagePane(0,0,100,{10,8,1},{1,0,0});check(g.world().fits(10,8,0,1.6f)&&g.state(stateId("board_broken"))&&!g.m_clutter.empty(),"splintered board opens its route and emits debris");g.damagePane(0,1,100,{6.5f,6.5f,3},{0,0,1});check(!g.world().breakableIntact(1)&&std::any_of(g.m_clutter.begin(),g.m_clutter.end(),[](const auto&c){return c.vz<0;}),"ceiling tile breaks into falling debris");auto bytes=g.encodeSave();Game loaded(campaign);check(loaded.decodeSave(bytes)&&!loaded.world().breakableIntact(0)&&!loaded.world().breakableIntact(1),"stage 3 destruction survives save and load");}
 {auto m=regression::room();m->fixtures.push_back({model::crt_monitor,{8,8},0,.42f,.4f,.38f,0,false});kit::fixturePart(*m,0,2,7.8f,7.78f,8.2f,7.82f,.1f,.35f);Game g(regression::campaign(m));check(g.world().fixtureParts(0)==~0u,"fixture screen begins with its material part present");auto hit=g.traceShot({8,7,.2f},{0,1,0},4);g.resolveShot(hit,{8,7,.2f},{0,1,0},weaponDef(WeaponId::Shotgun).hitscan,true);check((g.world().fixtureParts(0)&2u)==0&&(g.world().fixtureParts(0)&1u)!=0,"broken fixture screen removes only its selected part");}
 {auto m=regression::room();m->cargoLift={6,6,10,10,0,3,1,stateId("pcall"),stateId("prelease"),stateId("ppos"),stateId("pdown"),stateId("parrived"),stateId("pdescended")};Game g(regression::campaign(m));g.m_player.pos={3,3};g.m_clutter={prop(model::keyboard,{8,8})};g.setState(stateId("pcall"),1);g.updateMechanisms(.1f);check(std::fabs(g.m_clutter[0].z-.1f)<.001f,"cargo platform carries a prop resting on its deck");}
 {auto m=regression::room();Door d{7,10,8};d.swinging=true;d.opening=true;d.open=.2f;m->doors.push_back(d);Game g(regression::campaign(m));g.m_player.pos={3,3};g.m_clutter={prop(model::keyboard,{8,8.3f})};auto before=g.m_clutter[0].pos;g.simulateClutter(g.m_clutter,0,1.f/60,-1);check(length(g.m_clutter[0].pos-before)>.05f,"a swinging door sweeps a sleeping prop aside");}
 {Game paper(regression::campaign(regression::room())),plastic(regression::campaign(regression::room()));paper.m_player.pos=plastic.m_player.pos={3,3};auto a=prop(model::office_paper_stack,{8,8},2),b=prop(model::keyboard,{8,8},2);a.velocity=b.velocity={2,0};a.sleeping=b.sleeping=false;paper.m_clutter={a};plastic.m_clutter={b};paper.simulateClutter(paper.m_clutter,0,.1f,-1);plastic.simulateClutter(plastic.m_clutter,0,.1f,-1);check(paper.m_clutter[0].velocity.x<plastic.m_clutter[0].velocity.x,"paper has additional air drag");}
 {Game g(regression::campaign(regression::room()));g.m_player.pos={3,3};auto a=prop(model::keyboard,{8,8}),b=prop(model::keyboard,{8.4f,8});a.velocity={2,0};a.sleeping=false;a.soundOwner=0;g.m_clutter={a,b};g.m_sounds.clear();g.simulateClutter(g.m_clutter,0,1.f/60,-1);out<<"contact owners "<<g.m_clutter[0].soundOwner<<','<<g.m_clutter[1].soundOwner<<" z "<<g.m_clutter[0].z<<','<<g.m_clutter[1].z<<" vx "<<g.m_clutter[0].velocity.x<<','<<g.m_clutter[1].velocity.x<<" sounds "<<g.m_sounds.size()<<'\n';check(g.m_clutter[1].soundOwner==0&&std::any_of(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.owner==0;}),"prop contacts retain the kicking guard's sound ownership");}
 {Game g(regression::campaign(regression::room()));g.m_player.pos={3,3};auto c=prop(model::mug,{8,8});c.soundOwner=0;g.m_clutter={c};g.m_sounds.clear();g.breakClutter(0,0,{1,0,0});auto stimuli=g.aiStimuli();check(g.m_clutter[0].broken&&g.m_clutter[0].soundOwner==0&&std::any_of(stimuli.begin(),stimuli.end(),[](const AiStimulus&s){return s.npc==0&&(s.categories&ai::SoundImpact);}),"guard-kicked fragile prop retains NPC ownership when it shatters");}
 {auto m=regression::room();m->definition.environment=Environment::Outdoor;m->skybox="brutal_wasteland";kit::breakable(*m,9.97f,6,10.03f,10,0,2);World w(0,m);const auto b=w.breakables()[0];const auto&s=w.structures()[w.breakableStructure(0)];check(w.hasTerrain()&&std::fabs(b.bottom-s.bottom)<.001f&&std::fabs(b.top-s.top)<.001f&&b.bottom!=0,"Ashfall terrain grounds breakable records and collision boxes together");check(w.damageBreakable(0,100)&&!w.breakableIntact(0),"terrain-grounded glazing breaks through the normal world path");}
 {std::string text="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Physics parser|0\nMAP|0|Room|0|0|4.5|6.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";for(int y=0;y<24;++y)text+="ROW|0|0|"+std::to_string(y)+"|........................\n";text+="CLUTTER|0|7|8|8|0|0|28|.45|.16|.03|0|0|prop_broken\nBREAKABLE|0|9.97|6|10.03|10|0|3|10|35|0|board_broken|board_trigger\nLEAD|0|9.958|8|1.2|0|0|0|2.6|0.006|6|1|parser_lead|0|0|0|0.03\nLEAD|0|9.958|9|1.2|9.3|9|0|1.6|0.006\nLEAD|0|9.958|9.5|1.2|9.4|9.5|0|1.1|0.006||||||||1\nCHAIN|0|10|8|2.2|1.4|0.014\nCREATURE|0|4|12|8|-999\nCHAIN|0|12|8|1.8|1.1|0.012|0|0.15|0|1.4\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";try{auto campaign=loadCustomCampaignText(text,"physics-parser.txt");Game g(campaign);check(g.m_clutter.size()==1&&g.m_clutter[0].model==28&&g.world().breakables()[0].material==10,"custom CLUTTER model and BREAKABLE records reach runtime physics");
   check(g.world().leads().size()==5&&g.world().leads()[0].prop==0&&g.world().leads()[0].live&&g.world().leads()[0].state==stateId("parser_lead")&&!g.world().leadRopes()[0].torn&&g.world().leadRopes()[0].sleeping,"a custom LEAD record clips its cable to the CLUTTER prop it names, hung at rest");
   {const auto&fixed=g.world().leads()[1];const auto&loose=g.world().leads()[2];const auto&fr=g.world().leadRopes()[1];
    check(fixed.material==7&&fixed.prop<0&&!fixed.loose&&!fixed.live&&!fixed.state&&!fr.torn&&!fr.points.empty()&&std::fabs(fr.points.back().x-9.3f)<.001f&&loose.loose&&loose.material==7&&loose.prop<0&&loose.offset.x==0&&loose.offset.y==0&&loose.offset.z==0&&g.world().leadRopes()[2].torn,"LEAD fields after the radius default when left off or empty: a fixed end, and a loose lead with no offsets");}
   {const auto&fixed=g.world().leads()[3];const auto&hung=g.world().leads()[4];const auto&fr=g.world().leadRopes()[3];const auto&hr=g.world().leadRopes()[4];
    check(fixed.chain&&fixed.loose&&fixed.creature<0&&fixed.material==5&&fr.torn&&fr.points.size()>=10&&hung.chain&&hung.creature==0&&hung.offset.z==1.4f&&hr.anchorOverride&&hr.points.size()>=10,"CHAIN records hang as free Verlet physics chains, including one pinned to a creature");}
   auto bytes=g.encodeSave();Game loaded(campaign);check(loaded.decodeSave(bytes)&&loaded.encodeSave()==bytes,"custom physics records roundtrip through a byte-exact save");}catch(const std::exception&e){out<<e.what()<<'\n';check(false,"custom physics parser");}}
{auto m=regression::room();m->fixtures.push_back({model::office_bookcase,{8,8},0,2.025f,.5f,2.5f,kPi*.5f,true});for(float top:{.03f,.656f,1.335f,1.917f})kit::prop(*m,model::office_file_binder,8.03f,8,top,.093f,.301f,.424f,kPi*.5f+.01f);Game shelves(regression::campaign(m));shelves.m_player.pos={3,3};bool stable=shelves.m_clutter.size()==4;for(auto&c:shelves.m_clutter){stable&=c.sleeping;c.sleeping=false;c.velocity={0,.02f};}for(int t=0;t<120;++t)shelves.updateClutter({},1.f/120);int n=0;for(float top:{.03f,.656f,1.335f,1.917f}){const auto& c=shelves.m_clutter[size_t(n++)];stable&=!c.broken&&c.sleeping&&std::fabs(c.z-top)<.005f&&length(c.pos-Vec2{8.03f,8})<.03f;}check(stable,"bookcase binders retain their measured shelf support after wake and motion");}
  {auto m=regression::room();m->fixtures.push_back({model::desk,{8,8},0,1.4f,.7f,.75f,kPi*.25f,true});kit::prop(*m,model::packing_crate,8,8,.75f,.6f,.3f,.2f,kPi*.25f);Game rotated(regression::campaign(m));rotated.m_player.pos={3,3};auto& c=rotated.m_clutter[0];c.sleeping=false;c.velocity={.05f,-.05f};for(int t=0;t<120;++t)rotated.updateClutter({},1.f/120);check(!c.broken&&c.sleeping&&std::fabs(c.z-.75f)<.005f&&length(c.pos-Vec2{8,8})<.08f,"rotated prop moves and settles on its oriented tabletop footprint");}
 {auto m=regression::room();kit::prop(*m,model::med_crash_cart,8,8,0,.74f,.48f,1,0);kit::prop(*m,model::med_pill_bottle,8,8,.809f,.07f,.03f,.13f,0);Game cart(regression::campaign(m));cart.m_player.pos={3,3};bool rests=cart.m_clutter[1].sleeping&&std::fabs(cart.m_clutter[1].z-.809f)<.001f;cart.knockClutter(0,0,{8,8,.5f},{18,0,0});cart.m_clutter[0].velocity={4,0};for(int t=0;t<180;++t)cart.updateClutter({},1.f/120);check(rests&&(cart.m_clutter[1].broken||cart.m_clutter[1].z<.75f),"medical cart supports its tray load and wakes it to fall when rolled away");}
 {auto m=regression::room();m->fixtures.push_back({model::packing_crate,{8,8},0,.7f,.5f,.42f,0,true});World w(0,m);bool supported=true;for(float x:{7.65f,8.35f})for(float y:{7.75f,8.25f})supported&=std::fabs(w.supportBelow(x,y,.42f,true)-.42f)<.001f;check(supported,"flush crate corners retain support at floating point boundaries");}
 // Electrical leads: a cable from a wall's face to a cell on a desk (or, east, on the floor by an open seam).
 {auto leadMap=[](bool live,bool east=false){auto m=regression::room();m->definition.playerStart={3,3};const float wall=east?21.8f:5.8f;m->structures.push_back({wall,6,wall+.2f,10,0,3});
   if(east){m->openEast=true;m->layers[0].rows[8][23]='.';}else m->fixtures.push_back({model::desk,{8,8},0,1.4f,.7f,.75f,0,true});
   kit::prop(*m,model::anl_cell_charged,east?23.2f:8.f,east?8.5f:8.f,east?0.f:.75f,0);kit::lead(*m,{wall+.212f,east?8.5f:8.f,1.6f},0,{0,0,.41f},east?2.2f:2.4f,8,live,.007f,stateId("reg_lead_torn"));return m;};
  auto lowest=[](const LeadRope&r){float z=1e9f;for(auto p:r.points)z=std::min(z,p.z);return z;};
  auto finite=[](const LeadRope&r){bool f=r.points.size()>=10;for(auto p:r.points)f&=std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);return f;};
  auto span=[](Point3 a,Point3 b){auto d=b-a;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);};
  auto heard=[](const Game&g,Sound s){return std::any_of(g.m_sounds.begin(),g.m_sounds.end(),[&](const SoundEvent&e){return e.sound==s;});};
  auto run=[](Game&g,int ticks){for(int i=0;i<ticks;++i){g.m_elapsed+=1.f/120;g.updateClutter({},1.f/120);}};
  auto campaign=regression::campaign(leadMap(true));Game g(campaign);g.m_player.pos={3,3};auto&r=g.m_world.m_leadRopes[0];const auto l=g.world().leads()[0];
  Point3 clip=leadClip(g.m_clutter[0],l.offset);
  check(finite(r)&&r.sleeping&&!r.torn&&lowest(r)<std::min(l.anchor.z,clip.z)-.05f&&lowest(r)>0&&span(r.points.back(),clip)<.002f,"a lead opens hanging in a sag below both its ends, clipped and at rest");
  // Off the prop: the point beside the clip stands up out of the cell's top, and no piece of the cable runs into the cell.
  auto rises=[&](const Game&w){const auto&p=w.world().leadRopes()[0].points;const auto up=leadBody(w.m_clutter[0]).axis[2],d=p[p.size()-2]-leadClip(w.m_clutter[0],l.offset);return d.x*up.x+d.y*up.y+d.z*up.z;};
  check(g.leadThrough(0,0)<=l.radius&&rises(g)>.03f,"a clipped lead leaves its clip up out of the prop's face and hangs clear of the prop, not through it");
  auto rested=r.points;run(g,30);check(r.sleeping&&r.points.size()==rested.size()&&std::equal(rested.begin(),rested.end(),r.points.begin(),[](Point3 a,Point3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}),"a settled lead on a prop at rest stays asleep and still");
  // Carried: she holds the cell up in front of her, inside the lead's slack.
  g.m_player.pos={8.9f,8};g.m_player.angle=kPi;g.m_player.pitch=0;g.m_heldClutter=0;const Point3 middle=r.points[r.points.size()/2];float carriedDeep=-1;for(int t=0;t<60;++t){run(g,1);carriedDeep=std::max(carriedDeep,g.leadThrough(0,0));}clip=leadClip(g.m_clutter[0],l.offset);
  check(!r.torn&&span(clip,l.anchor)<l.length&&span(r.points.back(),clip)<.002f&&span(r.points[r.points.size()/2],middle)>.05f&&lowest(r)<std::min(l.anchor.z,clip.z),"a lead follows a carried prop inside its slack, its clip on the prop and the cable sagging");
  // Turned, tipped and lowered in her hands, the cell never takes its own cable inside it.
  {Game turn(campaign);turn.m_player.pos={8.9f,8};turn.m_player.angle=kPi;turn.m_heldClutter=0;run(turn,30);turn.m_clutter[0].pitch=.6f;turn.m_clutter[0].roll=-.35f;float deep=-1,rise=1;
   for(int t=0;t<240;++t){turn.m_player.angle=kPi+std::sin(float(t)/40)*.6f;turn.m_player.pitch=-60.f*float(t)/240;run(turn,1);deep=std::max(deep,turn.leadThrough(0,0));rise=std::min(rise,rises(turn));}
   out<<"carried lead: deepest into its cell "<<std::max(carriedDeep,deep)*1000<<" mm (radius "<<l.radius*1000<<"), least rise off its face "<<rise*1000<<" mm\n";
   check(!turn.world().leadRopes()[0].torn&&carriedDeep<=l.radius&&deep<=l.radius&&rise>.03f,"a carried prop, turned and tipped, keeps its lead leaving the clip outward and never through the prop");}
  // Backing away until the clip goes.
  const float health=g.m_player.health;float tearSpan=0,tugDeep=-1;for(int i=0;i<900&&!r.torn;++i){g.m_sounds.clear();tearSpan=span(leadClip(g.m_clutter[0],l.offset),l.anchor);g.m_player.pos.x+=.004f;run(g,1);tugDeep=std::max(tugDeep,g.leadThrough(0,0));}
  check(tugDeep<=l.radius,"drawn taut toward its tear, the lead still runs clear of the prop it pulls on");
  const auto stimuli=g.aiStimuli();const Point3 tip=r.points.back();
  check(r.torn&&tearSpan>l.length+Lead::TearMargin-.02f&&tearSpan<l.length+Lead::TearMargin+.02f&&g.state(stateId("reg_lead_torn"))==1,"the clip tears pulled past the lead's length and margin, setting its state");
  check(std::any_of(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::LeadArc&&e.aiRadius>=14&&e.spatial;})&&std::any_of(stimuli.begin(),stimuli.end(),[&](const AiStimulus&s){return s.radius>=14&&(s.categories&ai::SoundImpact)&&length(s.source-Vec2{tip.x,tip.y})<.3f;}),"a live lead's tear arcs: LeadArc at the torn end, a 14 m AI stimulus");
  check(r.tornAt==g.m_elapsed&&leadArc(0)>=1&&leadArc(1.f)>0&&leadArc(2.7f)==0,"the torn end throws its spark burst, spits with the crackle, then goes dead");
  check(g.m_player.health==health-LeadShock&&g.m_damageFlash>0,"holding the prop as a live lead tears gives a small shock and a hit flash");
  int settle=0;for(;settle<600&&!r.sleeping;++settle)run(g,1);out<<"torn lead at rest after "<<settle/120.f<<" s, tip z "<<r.points.back().z<<", middle z "<<r.points[r.points.size()/2].z<<'\n';
  check(r.sleeping&&settle<=480&&finite(r)&&r.points.back().z<l.anchor.z-.5f&&r.points[r.points.size()/2].z<l.anchor.z,"the torn lead falls limp below its anchor and comes to rest inside four seconds");
  {auto bytes=g.encodeSave();Game loaded(campaign);bool decoded=loaded.decodeSave(bytes);const auto&lr=loaded.world().leadRopes()[0];
   check(decoded&&lr.torn&&lr.sleeping&&finite(lr)&&lr.points.back().z<l.anchor.z-.5f&&loaded.encodeSave()==bytes,"save and load keep the torn lead torn and hang it limp again");}
  {Game away(campaign);away.m_player.pos={3,3};const float before=away.m_player.health;auto&cell=away.m_clutter[0];cell.pos={11,8};cell.sleeping=false;away.m_sounds.clear();run(away,1);
   check(away.world().leadRopes()[0].torn&&heard(away,Sound::LeadArc)&&away.m_player.health==before,"a prop knocked away past the length tears its lead with no shock to a player not holding it");}
  {Game broken(campaign);broken.m_player.pos={3,3};const float before=broken.m_player.health;broken.breakClutter(0,0,{1,0,0});broken.m_sounds.clear();run(broken,1);
   check(broken.world().leadRopes()[0].torn&&heard(broken,Sound::LeadArc)&&broken.m_player.health==before,"a prop broken while clipped tears its lead");}
  {Game dead(regression::campaign(leadMap(false)));dead.m_player.pos={8.9f,8};dead.m_player.angle=kPi;dead.m_heldClutter=0;const float before=dead.m_player.health;run(dead,30);bool tore=false;
   for(int i=0;i<900&&!tore;++i){dead.m_sounds.clear();dead.m_player.pos.x+=.004f;run(dead,1);tore=dead.world().leadRopes()[0].torn;}
   check(tore&&!heard(dead,Sound::LeadArc)&&heard(dead,Sound::JunkMetal)&&dead.m_player.health==before,"a dead lead pops off its clip without an arc or a shock");}
  {auto first=leadMap(true,true),second=regression::room();second->definition.origin={24,0};second->openWest=true;second->layers[0].rows[8][0]='.';auto seam=regression::campaign(first);seam->maps.push_back(second);
   Game cross(seam);cross.m_heldClutter=0;cross.m_clutter[0].pos={24.1f,8.5f};cross.m_player.pos={24.1f,8.5f};const float before=cross.m_player.health;cross.crossChunkBoundary();cross.m_sounds.clear();run(cross,1);
   const auto&cr=cross.chunkWorld(0).leadRopes()[0];check(cross.level()==1&&cross.holdingClutter()&&cr.torn&&heard(cross,Sound::LeadArc)&&cross.m_player.health==before-LeadShock,"a prop carried into the next chunk tears its lead, shocking the hand that carries it");}
  // The audit passes a lead plugged into a wall and fails one hung from thin air or run through the wall.
  {Game plugged(regression::campaign(leadMap(true)));auto air=leadMap(true);air->leads[0].anchor={7,8,2.4f};Game floating(regression::campaign(air));
   auto wall=leadMap(true);wall->leads[0].anchor={5.788f,8,1.6f};wall->leads[0].length=2.8f;Game through(regression::campaign(wall));
   const bool clean=auditMapScene(plugged,0);if(!clean){std::ifstream report("map-audit-0.txt");std::string line;while(std::getline(report,line))if(line.find(" lead[")!=std::string::npos)out<<"  plugged audit: "<<line<<'\n';}
   check(clean&&!auditMapScene(floating,0)&&!auditMapScene(through,0),"the map audit passes a plugged-in lead and fails one hanging from nothing or run through a wall");}
  {auto inst=mapInspection({3.75f,2.75f},0,0,82,false,-999,true);const auto&w=inst.world();int clipped=0,loose=0;bool still=w.leads().size()==9;
   for(size_t i=0;i<w.leads().size();++i){const auto&lr=w.leadRopes()[i];still&=lr.sleeping&&finite(lr)&&inst.leadThrough(82,int(i))<=w.leads()[i].radius;++(lr.torn?loose:clipped);}
   still&=std::none_of(w.pipes().begin(),w.pipes().end(),[](const PipeRun&p){return p.material==8&&p.radius<.015f;});
   check(still&&clipped==8&&loose==1,"Instrumentation opens with eight test leads clipped to its cells clear of them, stand 3's hanging loose, all still and no wire pipes");}
  // A limp cable (torn, or loose from the start) lies as a dropped one does: asleep, each point
  // out of every solid and on or above what is under it, and woken for a second of play it stays
  // put (one held out in the air falls). `low` gets its lowest point.
  auto lies=[&](const World&w,int i,const char*what,Point3*low=nullptr){const auto&l=w.leads()[size_t(i)];const auto&r=w.leadRopes()[size_t(i)];bool rests=r.sleeping&&finite(r);Point3 least{0,0,1e9f};
   for(const auto&p:r.points){rests&=w.fits(p.x,p.y,p.z-l.radius,l.radius*2,false,true)&&p.z>=w.supportBelow(p.x,p.y,p.z+.02f)-.002f;if(p.z<least.z)least=p;}
   const float moved=w.leadUnrest(i,1.f,nullptr);if(low)*low=least;
   out<<what<<" lead "<<i<<": asleep "<<r.sleeping<<", lowest "<<least.x<<','<<least.y<<" z "<<least.z<<", end "<<r.points.back().x<<','<<r.points.back().y<<" z "<<r.points.back().z<<", woken it moves "<<moved*1000<<" mm\n";
   return rests&&moved<.03f;};
  // A torn plug (leadPlug, as drawn) never sinks into what is under its cable's end; come to rest on it, it lies on its side.
  auto plugClear=[&](const World&w,int i,int*resting){const auto&l=w.leads()[size_t(i)];const auto&r=w.leadRopes()[size_t(i)];const auto&e=r.points.back();const float under=w.supportBelow(e.x,e.y,e.z+.02f);
   const auto plug=leadPlug(l,r,under);const float low=leadPlugLowest(plug,l.radius);const bool rest=e.z-under<l.radius+.002f;if(rest)++*resting;
   out<<"  plug "<<i<<": end z "<<e.z<<" over "<<under<<", its lowest point "<<low<<", its axis z "<<plug.up.z<<'\n';return low>=under-.0005f&&(!rest||std::fabs(plug.up.z)<.2f);};
  // Test leads (kit::testLead) in the inspection's own bay: a test set's sockets to a cell's posts.
  // Seated: the cable's end on the post, the right-angle plug standing up the post's axis as the
  // cell is turned (its head PlugHead up it, the point past its boot level from there), the
  // point past the socket's boot straight out of the socket.
  {auto seated=[&](const Game&g,int i){const auto&l=g.world().leads()[size_t(i)];const auto&r=g.world().leadRopes()[size_t(i)];const int k=g.leadProp(0,l,r.clutter);if(k<0||r.torn||r.points.size()<3)return false;
     const auto&c=g.m_clutter[size_t(k)];const auto b=leadBody(c);const Point3 up=b.axis[0]*l.terminal.x+b.axis[1]*l.terminal.y+b.axis[2]*l.terminal.z;const auto&p=r.points;const auto plug=p[p.size()-2]-p.back(),boot=p[1]-l.anchor;
     return span(p.back(),leadClip(c,l.offset))<.002f&&span(r.plugUp,up)<.001f&&std::fabs(plug.x*up.x+plug.y*up.y+plug.z*up.z-Lead::PlugHead)<.001f&&boot.x*l.socket.x+boot.y*l.socket.y+boot.z*l.socket.z>.999f*span(boot,{});};
   auto pilot=[](const Game&g){const auto&l=g.world().leads()[0];return l.lamp>=0?g.lampPower(g.world().lights()[size_t(l.lamp)]):-1.f;};
   auto bay=leadInspection(2);const auto&w=bay.world();
   for(size_t i=0;i<w.leads().size();++i){const auto&l=w.leads()[i];out<<"bay lead "<<i<<": socket "<<(l.socketPort>=0?leadPorts()[size_t(l.socketPort)].name:"?")<<", terminal "<<(l.terminalPort>=0?leadPorts()[size_t(l.terminalPort)].name:"?")<<", seated "<<seated(bay,int(i))<<", asleep "<<w.leadRopes()[i].sleeping<<'\n';}
   check(w.leads().size()==2&&w.leads()[0].panel==1&&w.leads()[0].material==8&&w.leads()[1].material==7&&w.leads()[0].lamp>=0&&w.leads()[0].lamp==w.leads()[1].lamp&&leadPorts()[size_t(w.leads()[0].socketPort)].name=="+"&&leadPorts()[size_t(w.leads()[1].socketPort)].name=="-"&&leadPorts()[size_t(w.leads()[1].terminalPort)].name=="-",
    "kit::testLead seats red + to + and black - to - by name, one pilot lamp for the panel");
   check(seated(bay,0)&&seated(bay,1)&&w.leadRopes()[0].sleeping&&w.leadRopes()[1].sleeping&&pilot(bay)==1,"each plug in its socket and on its post in line with its cable, still, the pilot lit");
   {const bool clean=auditMapScene(bay,0);if(!clean){std::ifstream report("map-audit-0.txt");std::string line;while(std::getline(report,line))if(line.find(" lead[")!=std::string::npos||line.find(" lamp at ")!=std::string::npos||line.rfind("FAIL",0)==0)out<<"  bay audit: "<<line<<'\n';}
    check(clean,"the map audit passes the bay's test leads");}
   auto carried=leadInspection(3);float deep=-1;for(int i=0;i<2;++i)deep=std::max(deep,carried.leadThrough(0,i)-carried.world().leads()[size_t(i)].radius);
   check(carried.holdingClutter()&&std::fabs(carried.m_clutter[size_t(carried.m_heldClutter)].pitch)>.2f&&seated(carried,0)&&seated(carried,1)&&deep<=0&&pilot(carried)==1,"lifted, turned and tipped, the cell keeps both plugs seated on its posts and turning with it, the cables clear of it, the pilot lit");
   float age=-1;auto tearing=leadInspection(4,&age);const auto&tr=tearing.world().leadRopes();
   check(tr[0].torn!=tr[1].torn&&age>0&&age<.1f&&tearing.state(stateId("lead_circuit/Lead inspection/1"))==1&&tearing.state(stateId(tr[0].torn?"bay_lead_red":"bay_lead_black"))==1,"carried past the slack, one plug pulls off its post, breaking the panel's circuit and setting its lead's state");
   // The pilot follows the circuit: out once a lead tears, stuttering with the arc, then dark.
   int lit=0,dark=0;for(int i=0;i<360;++i){tearing.m_elapsed+=1.f/120;tearing.updateClutter({},1.f/120);const float p=pilot(tearing);if(i<240)(p>0?lit:dark)+=1;}
   out<<"pilot over the arc: lit "<<lit<<" of 240 frames, "<<pilot(tearing)<<" at 3 s\n";
   check(lit>0&&dark>lit&&pilot(tearing)==0,"the panel's pilot stutters with the arc and dies with the circuit");
   auto torn=leadInspection(5);auto dangling=leadInspection(6);bool keeps=true;
   for(const Game*g:{&torn,&dangling})for(int i=0;i<2;++i){const auto&l=g->world().leads()[size_t(i)];const auto&r=g->world().leadRopes()[size_t(i)];const auto post=leadClip(g->m_clutter[0],l.offset);
    keeps&=r.torn&&l.plugged()&&finite(r)&&span(r.points.back(),post)>.1f&&span(r.points.back(),r.points[r.points.size()-2])>.05f;}
   const auto&hang=dangling.world().leadRopes();const auto&anchor=dangling.world().leads()[0].anchor;
   check(keeps&&!(torn.world().leadRopes()[0].sleeping&&torn.world().leadRopes()[1].sleeping)&&hang[0].sleeping&&hang[1].sleeping&&hang[0].points.back().z<anchor.z-.3f&&pilot(dangling)==0&&!dangling.holdingClutter(),
    "torn, each lead keeps its plug on its end, swinging, then hanging still below its socket, the bare posts left on the cell, the pilot dark");
   {int resting=0;bool clear=true;for(const Game*g:{&torn,&dangling})for(int i=0;i<2;++i)clear&=plugClear(g->world(),i,&resting);
    check(clear&&resting>=1,"a torn plug never sinks into what is under it, and one come to rest on the floor lies on its side");}
   // Saved and loaded, the torn leads are hung again from their sockets off the bench's edge, out
   // of the wall and at rest (not laid across the bench toward the map's corner and left asleep).
   {auto reloaded=leadInspection(8);const auto&w8=reloaded.world();bool hung=w8.leadRopes().size()==2;int resting=0;
    for(int i=0;i<2&&hung;++i){const auto&r=w8.leadRopes()[size_t(i)];hung&=r.torn&&lies(w8,i,"reloaded")&&r.points.back().z<w8.leads()[size_t(i)].anchor.z-.3f&&w8.supportBelow(r.points.back().x,r.points.back().y,r.points.back().z+.02f)<.1f&&plugClear(w8,i,&resting);}
    check(hung&&pilot(reloaded)==0,"saved and loaded, the torn leads hang again from their sockets off the bench's edge, out of the wall and at rest, their plugs on their ends");}
   // A loose test lead (kit::looseTestLead) opens run off the bench's edge and hanging down its face.
   {auto loose=leadInspection(9);const auto&w9=loose.world();Point3 low{};const bool lying=w9.leads().size()==2&&w9.leads()[1].loose&&w9.leadRopes()[1].torn&&lies(w9,1,"loose",&low);int resting=0;const bool plug=plugClear(w9,1,&resting);
    check(lying&&plug&&low.z<.81f-.3f&&low.y<8.56f+.15f&&pilot(loose)==0&&auditMapScene(loose,0),"a loose test lead opens off the bench's edge and hanging at rest, its plug on its end, the pilot dark, and the audit passes it");}
   // The audit fails a plug off its port: a terminal 2 cm off its post, or a socket's panel moved after the lead was seated.
   auto bench=[](int fault){auto m=regression::room();m->definition.playerStart={8,10};m->fixtures.push_back({model::lab_clean_bench,{8,8.28f},0,1.18f,.56f,.81f,0,true});
    m->fixtures.push_back({model::lab_field_meter,{7.72f,8.3f},.81f,.25f,.328f,.148f,-2.1f,false});kit::prop(*m,model::anl_cell_charged,8.3f,8.32f,.81f,0);kit::testLead(*m,1,"+",0,"+",.8f);
    if(fault==1)m->leads[0].offset.x+=.02f;if(fault==2)m->fixtures[1].position.x+=.03f;if(fault==3)kit::looseTestLead(*m,1,"-",{7.9f,9.2f,0},.7f);return m;};
   Game good(regression::campaign(bench(0))),offPost(regression::campaign(bench(1))),offSocket(regression::campaign(bench(2)));
   check(auditMapScene(good,0)&&!auditMapScene(offPost,0)&&!auditMapScene(offSocket,0),"the map audit fails a test lead whose plug is not seated on its terminal post or in its socket");
   // ...and a cable asleep but held out in the air, as the old limp layout left a loose lead: level
   // from the bench top out past its edge, 30 cm over nothing. Above its support everywhere, it is not at rest.
   {Game hanging(regression::campaign(bench(3))),frozen(regression::campaign(bench(3)));auto&fr=frozen.m_world.m_leadRopes[1];
    for(size_t k=3;k<fr.points.size();++k)fr.points[k]=fr.points[2]+Point3{0,.06f*float(k-2),0};fr.previous=fr.points;fr.sleeping=true;
    check(auditMapScene(hanging,0)&&!auditMapScene(frozen,0),"the map audit passes a loose test lead hanging off the bench and fails one asleep but held out in the air");}
   bool refused=false;try{auto m=bench(0);kit::testLead(*m,1,"+",0,"x",.8f);}catch(const std::exception&){refused=true;}
   check(refused,"kit::testLead refuses a terminal the prop does not have");}
  // A LEAD record names its panel's socket and its prop's terminal: the same seat as kit::testLead.
  {std::string text="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Test leads|0\nMAP|0|Bay|0|0|8|10|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";for(int y=0;y<24;++y)text+="ROW|0|0|"+std::to_string(y)+"|........................\n";
   text+="FIXTURE|0|"+std::to_string(model::lab_clean_bench)+"|8|8.28|0|1.18|0.56|0.81|0|1\nFIXTURE|0|"+std::to_string(model::lab_field_meter)+"|7.72|8.3|0.81|0.25|0.328|0.148|-2.1|0\nCLUTTER|0|7|8.3|8.32|0.81|0|"+std::to_string(model::anl_cell_charged)+"|0.216|0.201|0.406|0|0|cell_broken\n"
    "LEAD|0||||0|0|0|0.8|0.007|8|1|record_red|0||||0|1|+|+\nLEAD|0||||7.9|9.2|0|0.7|0.007|7|||||||1|1|-|\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";
   try{auto campaign=loadCustomCampaignText(text,"test-leads.txt");Game g(campaign);const auto&w=g.world();
    auto m=regression::room();m->fixtures.push_back({model::lab_clean_bench,{8,8.28f},0,1.18f,.56f,.81f,0,true});m->fixtures.push_back({model::lab_field_meter,{7.72f,8.3f},.81f,.25f,.328f,.148f,-2.1f,false});kit::prop(*m,model::anl_cell_charged,8.3f,8.32f,.81f,0);kit::testLead(*m,1,"+",0,"+",.8f);
    const auto&a=w.leads()[0];const auto&b=m->leads[0];const auto&loose=w.leads()[1];const auto&lr=w.leadRopes()[0];
    check(w.leads().size()==2&&span(a.anchor,b.anchor)<1e-4f&&span(a.offset,b.offset)<1e-4f&&span(a.socket,b.socket)<1e-4f&&a.plugged()&&a.lamp>=0&&a.state==stateId("record_red")&&!lr.torn&&lr.sleeping&&span(lr.points.back(),leadClip(g.m_clutter[0],a.offset))<.002f
     &&loose.loose&&loose.socketed()&&loose.plugged()&&loose.lamp==a.lamp&&w.leadRopes()[1].torn&&g.lampPower(w.lights()[size_t(a.lamp)])==0,"LEAD panel|socket|terminal seats a record's plugs as kit::testLead does; a loose one keeps its plug and leaves the pilot dark");
    {Point3 low{};const bool rests=lies(w,1,"record loose",&low);for(int t=0;t<600;++t)g.updateClutter({},1.f/120);Point3 later{};const bool still=lies(w,1,"record loose, 5 s on",&later);
     check(rests&&still&&span(low,later)<.002f&&low.z<.6f&&low.y<8.56f+.15f,"a LEAD record's loose test lead opens off the bench's edge and hanging at rest, not held out in the air, and stays");}
    bool refused=false;try{auto bad=text;bad.replace(bad.find("|1|+|+"),6,"|1|x|+");loadCustomCampaignText(bad,"bad-leads.txt");}catch(const std::exception&){refused=true;}
    check(refused,"a LEAD record naming a socket its panel lacks is refused");}
   catch(const std::exception&e){out<<e.what()<<'\n';check(false,"LEAD panel|socket|terminal records");}}
 }
 out<<(ok?"ALL PASSED":"FAILED")<<" physics-stages\n";return ok;
}
}
