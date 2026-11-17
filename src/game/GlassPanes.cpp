// Glass that breaks a piece at a time, as Half-Life 2's and Alyx's panes do. Game::damagePane
// (Clutter.cpp) hands every blow on a glass pane here first: a round holes the pane where it
// hits, with its own radial and ring cracks; holes and cracks close together let the glass
// between them go; a piece cut loose from the frame falls out; a fist or a thrown prop punches
// a hole its size; a body coming through, a blast, or a pane half gone brings it down whole,
// teeth left in the frame. Holds (laminated) glass spiderwebs and sags, and only when it has
// been punched far enough at one place opens a small hole there; it never comes down unless the
// design gives it a health. What falls is glass shard props (Clutter kind 6, ShardOf::Glass):
// they fall, tumble, slide, tinkle and break again when they land hard, and crunch underfoot.
// The pane's state is World::PaneGlass (GlassFracture.h): its hits, all a save keeps.
#include "Game.h"
#include "../world/GlassFracture.h"
#include <cmath>

namespace retro {
namespace {
constexpr float HoldsStrain=120.f,HoldsPunch=2.4f,HoldsHoleMost=.12f,ShardBreakSpeed=3.2f;
constexpr int GlassShardsMost=160,PiecesPerBlow=14,HitsMost=40;
float glassBase(const Breakable& b){return b.health>0?b.health:20.f;}
// A spray of glints from a hole or a crack; when the list is full it drops the oldest spray, never a pane coming down.
void spray(std::vector<BreakEvent>& events,const BreakEvent& e){events.push_back(e);if(events.size()<=24)return;
 auto oldest=std::find_if(events.begin(),events.end(),[](const BreakEvent& v){return !v.pane;});events.erase(oldest==events.end()?events.begin():oldest);}
}

float Game::paneResistance(int level,int index,Point3 at)const{
 const auto&w=level==m_level?m_world:m_chunks[level].world;if(!w.breakableIntact(index))return 0;const auto&b=w.breakables()[size_t(index)];
 if(b.material!=17||b.holds)return std::max(0.f,(b.holds?180.f:b.health>0?b.health:b.material==22?8.f:35.f)-w.breakableDamage(index));
 const auto f=w.paneFrame(index);const auto*p=w.paneGlass(index);
 const float weak=p?std::clamp(glass::strainAt(p->hits,f.clamp(f.local(at))),0.f,1.f):0.f;return glassBase(b)*(1-weak);}

int Game::glassBlow(int level,int index,float amount,Point3 at,Point3 direction,PaneBlow blow){
 auto&w=level==m_level?m_world:m_chunks[level].world;const auto b=w.breakables().at(size_t(index));
 const auto f=w.paneFrame(index);const Vec2 q=f.clamp(f.local(at));const auto shift=chunkOffset(level)-chunkOffset(m_level);
 const Vec2 heard=Vec2{at.x,at.y}+shift;
 auto crackSound=[&](float gain,float pitch){m_sounds.push_back({Sound::JunkGlass,heard,gain,pitch,true,17,at.z});};
 if(b.holds){
  // Laminated: every blow spiderwebs it; where it has been punched far enough a small hole opens.
  const float s=std::clamp(amount/HoldsStrain,.05f,1.f);const auto*p=w.paneGlass(index);const float punched=p?glass::strainAt(p->hits,q):0.f;
  float r=0;if(punched>=HoldsPunch)r=blow.kind==PaneBlow::Round?.045f:blow.kind==PaneBlow::Fist?.07f:blow.kind==PaneBlow::Prop?std::clamp(blow.radius,.03f,HoldsHoleMost):HoldsHoleMost;
  const auto before=w.addPaneHit(index,{q.x,q.y,r,s});
  if(w.damageBreakable(index,amount))return 2;
  if(r>0){const auto*now=w.paneGlass(index);dropGlassPieces(level,index,glass::pieces(w.paneLattice(index),before,now->gone,now->hits.back()),direction,r,blow.kind!=PaneBlow::Round);}
  // A few glints of the fines laminated glass sheds where it is struck.
  spray(m_breakEvents,{{at.x,at.y},at.z-.05f,direction*-.5f,m_elapsed,level,PropMaterial::Glass,.04f,.04f});
  crackSound(r>0?.6f:.4f,r>0?.8f:.6f);
  return r>0&&(blow.kind!=PaneBlow::Prop||blow.radius<=HoldsHoleMost)?1:0;}
 const float base=glassBase(b);
 // A body coming through, a blast that beats the glass or a scripted break takes it whole.
 if(blow.kind==PaneBlow::Body||(blow.kind==PaneBlow::Blast&&amount>=base)||amount>=1e5f)return w.damageBreakable(index,1e9f)?2:0;
 const auto*p=w.paneGlass(index);const float weak=p?std::clamp(glass::strainAt(p->hits,q),0.f,1.f):0.f;
 float r=0,s=std::clamp(amount/base,.05f,.85f);
 switch(blow.kind){
  case PaneBlow::Round:r=std::clamp(.0032f*amount,.02f,.12f);s=std::max(s,.3f);break;
  case PaneBlow::Fist:r=amount>=.5f*base*(1-weak)?.085f:0.f;break;
  case PaneBlow::Prop:r=amount>=base*(1-weak)-1e-4f?std::clamp(blow.radius,.03f,1.f):0.f;break;
  default:break;}
 const auto before=w.addPaneHit(index,{q.x,q.y,r,s});
 if(w.damageBreakable(index,amount))return 2;
 const auto*now=w.paneGlass(index);const auto l=w.paneLattice(index);
 // Half of it gone, or riddled past keeping count: the rest comes down.
 if(now->missing*2>=l.cells()||int(now->hits.size())>=HitsMost){w.damageBreakable(index,1e9f);return 2;}
 const auto pieces=glass::pieces(l,before,now->gone,now->hits.back());dropGlassPieces(level,index,pieces,direction,r,blow.kind!=PaneBlow::Round);
 if(r>0||!pieces.empty()){
  // The hole's own glass sprays from it; a big piece letting go is heard further.
  spray(m_breakEvents,{{at.x,at.y},at.z-.05f,direction,m_elapsed,level,PropMaterial::Glass,std::max(.05f,r*2),std::max(.05f,r*2)});
  // A hole a body could nearly go through is heard as a pane going (14 m); a smaller one as a bottle, a round's as a crack.
  float area=0;for(const auto&piece:pieces)area+=piece.area;area=std::max(area,kPi*r*r);const bool big=area>.15f;
  SoundEvent event{big?Sound::GlassShatter:area>.02f?Sound::BottleBreak:Sound::JunkGlass,heard,big?.85f:std::clamp(.45f+area*6,.45f,.9f),big?1.1f:area>.02f?1.15f:1.3f,true,big?20.f:17.f,at.z};event.aiRadius=big?14.f:std::clamp(5.f+area*40,5.f,11.f);m_sounds.push_back(event);
  // Whatever rests on or leans against the glass feels it go.
  auto&list=level==m_level?m_clutter:m_chunks[level].clutter;for(auto&c:list){const auto e=c.extent();if(!c.broken&&c.pos.x+e[0]>=b.x1-.1f&&c.pos.x-e[0]<=b.x2+.1f&&c.pos.y+e[1]>=b.y1-.1f&&c.pos.y-e[1]<=b.y2+.1f&&c.z+c.height()>=b.bottom-.1f&&c.z<=b.top+.1f){c.sleeping=false;c.restTime=0;}}}
 else crackSound(.4f,.6f);
 return r>0?1:0;}

void Game::dropGlassPieces(int level,int index,const std::vector<glass::Piece>& pieces,Point3 direction,float blowRadius,bool blownProps){
 if(pieces.empty())return;const auto&w=level==m_level?m_world:m_chunks[level].world;const auto&b=w.breakables()[size_t(index)];const auto f=w.paneFrame(index);
 const auto&list=level==m_level?m_clutter:m_chunks[level].clutter;int lying=0;for(const auto&c:list)lying+=c.kind==6&&c.shardOf==ShardOf::Glass&&!c.broken;
 for(const auto&[at,c]:m_pendingShards)lying+=at==level&&c.shardOf==ShardOf::Glass;
 // Which side of the pane they leave by: the way the blow was going (a flat pane: down).
 const float push=direction.x*f.normal.x+direction.y*f.normal.y+direction.z*f.normal.z,side=f.flat?-1.f:push>=0?1.f:-1.f;
 const float half=f.flat?(b.top-b.bottom)*.5f:(f.normal.x!=0?b.x2-b.x1:b.y2-b.y1)*.5f;const Vec2 drive=Vec2{f.normal.x,f.normal.y}*side;
 int made=0;
 for(size_t i=0;i<pieces.size();++i){const auto&piece=pieces[i];// A round's own hole is crushed to the spray of chips (its break event); what it loosens round it falls as pieces.
  if(piece.area<.0012f||(piece.blown&&!blownProps)||made>=PiecesPerBlow||lying+made>=GlassShardsMost)continue;
  const std::uint32_t key=std::uint32_t(std::lround(piece.centre.x*997)+std::lround(piece.centre.y*6007));const float h0=glass::hashUnit(key,11,std::uint32_t(index)),h1=glass::hashUnit(key,12),h2=glass::hashUnit(key,13),h3=glass::hashUnit(key,14);
  Clutter c;c.kind=6;c.shardOf=ShardOf::Glass;c.piece=int(h0*8)&7;c.dims={std::clamp(piece.high.x-piece.low.x,.03f,.4f),std::clamp(piece.high.y-piece.low.y,.03f,.4f),.006f};
  const Point3 centre=f.at(piece.centre,(f.flat?-1.f:side)*(half+.025f));
  if(f.flat){c.yaw=h1*kTwoPi;}else{c.yaw=std::atan2(f.along.y,f.along.x);c.roll=kPi*.5f+(h1-.5f)*.3f;}
  c.pos={centre.x,centre.y};c.z=centre.z-c.extent()[2];
  const Vec2 along{f.along.x,f.along.y};
  if(piece.blown){c.velocity=drive*(1.4f+1.6f*h2+blowRadius*2)+along*((h3-.5f)*1.2f);c.vz=.4f+.7f*h3;}
  else{c.velocity=drive*(.2f+.45f*h2)+along*((h3-.5f)*.3f);c.vz=f.flat?-.5f:0.f;}
  c.spin=(h1-.5f)*7;c.pitchSpeed=(h2-.5f)*9;c.rollSpeed=(h3-.5f)*7+(f.flat?0.f:side*1.5f);c.sleeping=false;c.restTime=0;
  m_pendingShards.push_back({level,c});++made;}
}

void Game::dropCollapsedPane(int level,int index,Point3 at,Point3 direction){
 const auto&w=level==m_level?m_world:m_chunks[level].world;const auto&b=w.breakables()[size_t(index)];if(b.material!=17)return;const auto f=w.paneFrame(index);
 const auto shards=glass::fracture(f.width,f.height,f.clamp(f.local(at)),glass::paneSeed(level,index));const auto*p=w.paneGlass(index);const auto l=w.paneLattice(index);
 const auto loose=glass::loosePieces(shards,p?&l:nullptr,p?&p->gone:nullptr);std::vector<glass::Piece> pieces;
 for(size_t i=0;i<shards.size();++i)if(loose[i]==1){const auto&s=shards[i].shape;glass::Piece piece;piece.centre=s.centroid();piece.low={1e9f,1e9f};piece.high={-1e9f,-1e9f};
  for(int k=0;k<s.n;++k){const auto v=s.p[size_t(k)];piece.low={std::min(piece.low.x,v.x),std::min(piece.low.y,v.y)};piece.high={std::max(piece.high.x,v.x),std::max(piece.high.y,v.y)};}
  piece.area=s.area();piece.blown=shards[i].ring<=2;piece.cells=1;pieces.push_back(piece);}
 dropGlassPieces(level,index,pieces,direction,.2f,true);}

void Game::crackGlassShard(int level,size_t index,float speed){
 auto&list=level==m_level?m_clutter:m_chunks[level].clutter;auto&c=list.at(index);if(c.broken||c.kind!=6||c.shardOf!=ShardOf::Glass||speed<ShardBreakSpeed)return;
 c.broken=true;const auto shift=chunkOffset(level)-chunkOffset(m_level);SoundEvent event{Sound::JunkGlass,c.pos+shift,std::clamp(.3f+speed*.06f,.3f,.7f),1.45f,true,14,c.z};event.aiRadius=5;m_sounds.push_back(event);
 // A big piece breaks in two smaller ones that skitter on; the rest is the crumbs it leaves (drawn from the broken shard).
 if(std::max(c.dims[0],c.dims[1])<.11f)return;
 for(int k=0;k<2;++k){Clutter half=c;half.broken=false;half.dims={std::max(.03f,c.dims[0]*.58f),std::max(.03f,c.dims[1]*.58f),c.dims[2]};half.piece=(c.piece+3+k*2)&7;
  const float a=c.yaw+(k?kPi*.5f:-kPi*.5f)+.3f*float(c.piece);half.pos=c.pos+Vec2{std::cos(a),std::sin(a)}*(.05f+.15f*std::max(c.dims[0],c.dims[1]));
  half.velocity=c.velocity*.4f+Vec2{std::cos(a),std::sin(a)}*.6f;half.vz=.6f;half.pitch=0;half.roll=(k?.25f:-.25f);half.yaw=c.yaw+float(k)*1.3f;half.spin=(k?4.f:-4.f);half.pitchSpeed=0;half.rollSpeed=(k?2.f:-2.f);half.sleeping=false;half.restTime=0;
  m_pendingShards.push_back({level,half});}
}

void Game::updateGlass(float dt){
 if(dt<=0)return;m_glassBodyRearm=std::max(0.f,m_glassBodyRearm-dt);
 // A body coming through: the player diving through, or faster than a sprint; a creature
 // charging. Holds glass only spiderwebs (and only from the player).
 for(int level=0;level<chunkCount();++level)if(chunkResident(level)){auto&w=friendlyWorld(level);const auto shift=chunkOffset(level)-chunkOffset(m_level);
  for(int i=0;i<int(w.breakables().size());++i){if(!w.breakableIntact(i))continue;const auto b=w.breakables()[size_t(i)];if(b.material!=17)continue;const auto f=w.paneFrame(i);
   auto crash=[&](Point3 at,Point3 dir){damagePane(level,i,1000,at,dir,{PaneBlow::Body,.5f});};
   {const Point3 feet{m_player.pos.x-shift.x,m_player.pos.y-shift.y,m_player.z};
    if(f.flat){if(feet.x>b.x1&&feet.x<b.x2&&feet.y>b.y1&&feet.y<b.y2&&feet.z>=b.top-.05f&&feet.z<=b.top+.25f&&m_player.verticalVelocity<=-5.5f&&!b.holds){crash(feet,{0,0,-1});continue;}}
    else{const float across=f.across(feet),u=f.local(feet).x;const Vec2 n{f.normal.x,f.normal.y};const float closing=-(across>=0?1.f:-1.f)*dot(m_velocity,n);
     if(std::fabs(across)<.3f&&u>-.15f&&u<f.width+.15f&&feet.z<b.top-.2f&&feet.z+m_player.hullHeight()>b.bottom+.3f&&(closing>=6.f||(!m_player.grounded&&closing>=3.6f))){
      const Point3 at{feet.x,feet.y,std::clamp(feet.z+1.f,b.bottom,b.top)},dir{n.x*(across>=0?-1.f:1.f),n.y*(across>=0?-1.f:1.f),0};
      if(!b.holds){crash(at,dir);continue;}
      // Holds glass takes the body's blow (a spiderweb and a sag) once a half second, and stands.
      if(m_glassBodyRearm<=0){damagePane(level,i,60,at,dir,{PaneBlow::Body,.5f});m_glassBodyRearm=.5f;}}}}
   if(b.holds||f.flat)continue;
   auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
   for(const auto&e:enemies){if(!e.alive||!e.moving||!(e.state==Enemy::State::Chase||e.stalkMode==Enemy::StalkMode::Rush))continue;
    const Point3 body{e.pos.x,e.pos.y,e.z};const float across=f.across(body),u=f.local(body).x;const Vec2 heading{std::cos(e.heading),std::sin(e.heading)},n{f.normal.x,f.normal.y};
    if(std::fabs(across)<e.radius()+.12f&&u>-.1f&&u<f.width+.1f&&e.bodyBottom()<b.top-.2f&&e.bodyTop()>b.bottom+.3f&&-(across>=0?1.f:-1.f)*dot(heading,n)>.5f){
     crash({e.pos.x,e.pos.y,std::clamp((e.bodyBottom()+e.bodyTop())*.5f,b.bottom,b.top)},{heading.x,heading.y,0});break;}}}}
 // A sheet of glass come to rest near flat lies flat (the tumble solver rocks a sheet that thin
 // on its face for ever): slow, low and within 0.2 rad of a face, it settles and sleeps.
 for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&w=friendlyWorld(level);auto&list=level==m_level?m_clutter:m_chunks[level].clutter;
  for(auto&c:list){if(c.kind!=6||c.shardOf!=ShardOf::Glass||c.broken||c.sleeping||lengthSq(c.velocity)>.0025f||std::fabs(c.vz)>.1f||std::fabs(c.pitchSpeed)+std::fabs(c.rollSpeed)+std::fabs(c.spin)>1.5f)continue;
   const float pitch=std::round(c.pitch/kPi)*kPi,roll=std::round(c.roll/kPi)*kPi;if(std::fabs(c.pitch-pitch)>.2f||std::fabs(c.roll-roll)>.2f)continue;
   const float floor=w.supportBelow(c.pos.x,c.pos.y,c.z+.05f,true);if(c.z-floor>.02f)continue;
   c.pitch=wrapAngle(pitch);c.roll=wrapAngle(roll);c.z=floor;c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=true;c.restTime=0;}}
 // Glass underfoot crunches: a shard, the crumbs of one, or the settled glass under a fallen pane.
 const float speed=length(m_velocity);if(!m_player.grounded||speed<.3f){m_glassCrunchLast=m_player.pos;return;}
 m_glassCrunchWalked+=length(m_player.pos-m_glassCrunchLast);m_glassCrunchLast=m_player.pos;const bool sprint=speed>4.2f;if(m_glassCrunchWalked<(sprint?.5f:.42f))return;
 bool glassy=false;const Vec2 at=m_player.pos;
 for(const auto&c:m_clutter)if(c.kind==6&&c.shardOf==ShardOf::Glass&&lengthSq(c.pos-at)<.45f*.45f&&std::fabs(c.z-m_player.z)<.3f){glassy=true;break;}
 if(!glassy)for(int i=0;i<int(m_world.breakables().size())&&!glassy;++i){const auto&b=m_world.breakables()[size_t(i)];if(m_world.breakableIntact(i)||b.material!=17)continue;
  const Vec2 centre{(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f};const float reach=std::max(b.x2-b.x1,b.y2-b.y1)*.5f+.6f;const float floor=m_world.supportBelow(at.x,at.y,m_player.z+.1f);
  glassy=lengthSq(at-centre)<reach*reach&&std::fabs(m_player.z-floor)<.1f&&std::fabs(floor-m_world.supportBelow(centre.x,centre.y,b.bottom+.1f))<.35f;}
 m_glassCrunchWalked=0;if(!glassy)return;
 const float pitch=1.25f+.3f*glass::hashUnit(std::uint32_t(m_elapsed*1000),5);
 SoundEvent event{Sound::JunkGlass,at,m_player.crouched?.16f:sprint?.7f:.45f,pitch,true,14,m_player.z};event.aiRadius=m_player.crouched?1.5f:sprint?8.f:4.5f;m_sounds.push_back(event);
}
}
