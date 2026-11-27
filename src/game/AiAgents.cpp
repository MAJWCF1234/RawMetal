// The AIs placed over zones of a map (AI records, ScriptDefinition.h AiAgent) and the AI blocks (Lights, Noise,
// Later script actions). An AI reads the players and writes what it decides as states; script events (the blocks
// the Builder's AI and Entity Manager snaps together) read those states and act. Nothing here knows a map: the
// Director over Chapter 2's Engineering is a zone, its knobs and the blocks wired to it, like any other.
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>
namespace retro {
namespace {
StateId lightsMode(const AiAgent& ai){return stateId(ai.name+"_lights");}
StateId lightsLeft(const AiAgent& ai){return stateId(ai.name+"_lights_left");}
// What an AI remembers of where it heard people: its zone in patches this wide (m), so it knows an area, never a spot.
constexpr float Patch=4.f;
StateId heardIn(const AiAgent& ai,int patch){return stateId(ai.name+"_heard_"+std::to_string(patch));}
StateId lastHeard(const AiAgent& ai){return stateId(ai.name+"_lead");}       // patch + 1; 0 nothing yet
std::vector<Vec2> skinbotRoute(const World& world,Vec2 home){
 const auto* map=world.mapData();
 if(!map)return {};
 const auto& fixtures=world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){
  const auto& fixture=fixtures[i];
  if(fixture.model!=skinbot::Model||!fixture.visible||lengthSq(fixture.position-home)>.001f)continue;
  const auto fields=map->recordFields.find(recordFieldKey("FIXTURE","",int(i)));
  if(fields==map->recordFields.end())return {};
  const auto* route=findNamedField(fields->second,"skinbot.route");
  if(!route)return {};
  auto text=route->value;
  for(char& c:text)if(c==','||c==';')c=' ';
  std::istringstream input(text);std::vector<Vec2> points;
  for(;;){
   input>>std::ws;
   if(input.eof())break;
   Vec2 point;
   if(!(input>>point.x>>point.y)||!std::isfinite(point.x)||!std::isfinite(point.y)||
      point.x<1||point.x>23||point.y<1||point.y>23||points.size()==16)return {};
   points.push_back(point);
  }
  return points.size()>=2?points:std::vector<Vec2>{};
 }
 return {};
}
enum class SkinbotContact {Push, Step, Block};
struct SkinbotProp {
 size_t index;
 Vec2 at,axis;
 float halfWidth,halfDepth,bottom,top;
 SkinbotContact contact;

 float penetration(Vec2 point,float radius)const{
  const Vec2 d=point-at;
  const float x=std::fabs(dot(d,axis))-halfWidth;
  const float y=std::fabs(dot(d,Vec2{-axis.y,axis.x}))-halfDepth;
  const float outside=std::hypot(std::max(x,0.f),std::max(y,0.f));
  return radius-outside-std::min(std::max(x,y),0.f);
 }
};

SkinbotProp skinbotProp(const Clutter& prop,size_t index,bool held){
 auto local=prop;local.yaw=0;const auto half=local.extent();
 const float mass=prop.mass(),height=2*half[2];
 SkinbotContact contact=SkinbotContact::Block;
 if(!held&&!prop.pinned&&mass<=4&&height<=.35f&&std::max(half[0],half[1])<=.25f)contact=SkinbotContact::Push;
 else if(!held&&mass<25&&height<=skinbot::Walker::Climb-.04f)contact=SkinbotContact::Step;
 return {index,prop.pos,{std::cos(prop.yaw),std::sin(prop.yaw)},half[0],half[1],prop.z,prop.z+height,contact};
}

constexpr float SkinbotBodyRadius=.82f,SkinbotRouteRadius=1.46f,SkinbotBodyHeight=2.3f;
using SkinbotClearance=std::function<bool(Vec2)>;
bool skinbotSegmentClear(Vec2 from,Vec2 to,const SkinbotClearance& clear){
 const int samples=std::max(1,int(std::ceil(length(to-from)/.24f)));
 for(int i=1;i<=samples;++i)if(!clear(from+(to-from)*(float(i)/samples)))return false;
 return true;
}

std::vector<Vec2> skinbotBypass(Vec2 from,Vec2 goal,const SkinbotClearance& clear){
 // Search within the loaded room. Smooth the result to a few turns, then recheck it against live props while walking.
 constexpr float spacing=.5f;
 constexpr int width=World::Width*2+1,height=World::Height*2+1;
 const int start=std::clamp(int(std::lround(from.y/spacing)),0,height-1)*width
                 +std::clamp(int(std::lround(from.x/spacing)),0,width-1);
 const auto position=[&](int cell){return cell==start?from:Vec2{(cell%width)*spacing,(cell/width)*spacing};};
 std::array<float,width*height> cost;
 std::array<int,width*height> previous;
 std::array<signed char,width*height> walkable{};
 cost.fill(std::numeric_limits<float>::infinity());previous.fill(-1);cost[start]=0;
 using Candidate=std::pair<float,int>;
 std::priority_queue<Candidate,std::vector<Candidate>,std::greater<Candidate>> open;
 open.push({length(goal-from),start});
 while(!open.empty()){
  const auto [priority,cell]=open.top();open.pop();
  const Vec2 here=position(cell);
  if(priority>cost[cell]+length(goal-here)+.001f)continue;
  if(lengthSq(goal-here)<=2.25f&&skinbotSegmentClear(here,goal,clear)){
   std::vector<Vec2> route{goal};
   for(int i=cell;i!=start;i=previous[i])route.push_back(position(i));
   std::vector<Vec2> waypoints;
   Vec2 anchor=from;
   for(size_t i=route.size();i>0;){
    size_t next=0;
    while(next+1<i&&!skinbotSegmentClear(anchor,route[next],clear))++next;
    anchor=route[next];waypoints.push_back(anchor);i=next;
   }
   waypoints.pop_back(); // the patrol goal is already held by the walker
   return waypoints;
  }
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   if(dx==0&&dy==0)continue;
   const int x=cell%width+dx,y=cell/width+dy;
   if(x<0||x>=width||y<0||y>=height)continue;
   const int next=y*width+x;
   const Vec2 there=position(next);
   const float candidate=cost[cell]+length(there-here);
   if(candidate>=cost[next])continue;
   if(walkable[next]==0)walkable[next]=clear(there)?1:-1;
   if(walkable[next]<0||!clear((here+there)*.5f))continue;
   cost[next]=candidate;previous[next]=cell;
   open.push({candidate+length(goal-there),next});
  }
 }
 return {};
}
StateId mostHeard(const AiAgent& ai){return stateId(ai.name+"_haunt");}
int patchOf(Vec2 p){return std::clamp(int(p.y/Patch),0,5)*6+std::clamp(int(p.x/Patch),0,5);}
}
void Game::seedAiAgents(){
 m_aiAgents.clear();
 for(int chunk=0;chunk<chunkCount();++chunk)for(auto ai:m_chunks[chunk].world.aiAgents()){ai.level=chunk;m_aiAgents.push_back(std::move(ai));}
 m_aiMinds.assign(m_aiAgents.size(),{});m_aiTick=0;
}
void Game::updateAiAgents(float dt){
 if(m_aiMinds.size()!=m_aiAgents.size())m_aiMinds.assign(m_aiAgents.size(),{});
 for(size_t i=0;i<m_aiAgents.size();++i){const AiAgent& ai=m_aiAgents[i];AiMind& mind=m_aiMinds[i];
  // A beat's pulse lasts the one frame the blocks saw it.
  if(state(ai.pulse))setState(ai.pulse,0);
  // After a load (or the first frame) the fractions come back from the saved states.
  if(!mind.seeded){mind.tension=float(state(ai.tension));mind.quiet=float(state(ai.quiet));mind.seeded=true;}
  // Who is in the zone, in the zone's own map frame (a body is in the active chunk's).
  int inside=0;float noise=0;
  for(const auto& p:players()){if(!p.alive()||p.level!=ai.level)continue;Vec2 at=p.body->pos;if(p.level!=m_level)at=at+chunkOffset(m_level)-chunkOffset(p.level);
   if(!ai.contains(at.x,at.y,p.body->z))continue;++inside;if(p.local&&m_stepNoiseAge<1.f)noise=std::max(noise,m_stepNoise);
   // Heard: a footstep that carried, or a shot. A quiet walker leaves it nothing to remember.
   if(p.local&&((m_stepNoiseAge<dt+.001f&&m_stepNoise>=3.f)||m_shotAge<dt+.001f)){
    const int patch=patchOf(at);const StateId count=heardIn(ai,patch);setState(count,std::min(state(count)+1,100000));
    setState(lastHeard(ai),patch+1);const int best=state(mostHeard(ai));
    if(!best||state(count)>state(heardIn(ai,best-1)))setState(mostHeard(ai),patch+1);}}
  // The menace gauge: it climbs while someone is in the zone (by pressure, faster for a noisy one) and ebbs
  // when the zone is empty. Never a twitch: a footstep moves it a fraction of a point.
  if(inside>0)mind.tension+=dt*(.6f+2.4f*ai.pressure)*(1.f+std::min(noise,12.f)/12.f);
  else mind.tension-=dt*3.f;
  mind.tension=std::clamp(mind.tension,0.f,100.f);
  if(inside>0)mind.quiet+=dt;
  // Its rhythm starts quiet the first time someone comes in.
  if(inside>0&&!state(ai.mood))setState(ai.mood,2);
  // A beat: after the calm, once the menace is high enough for the pressure (or the peak wait is up). The
  // blocks decide what a beat is (a flicker, a noise behind, a monster sent); the beat lets the menace down a third.
  const float due=70.f-40.f*ai.pressure;
  if(inside>0&&mind.quiet>=ai.calm&&(mind.tension>=due||mind.quiet>=ai.peak)){setState(ai.beat,state(ai.beat)+1);mind.quiet=0;mind.tension*=.66f;
   setState(ai.pulse,1);setState(ai.mood,state(ai.mood)==1?2:1);}
  setState(ai.tension,int(mind.tension));setState(ai.inside,inside);setState(ai.quiet,int(mind.quiet));
 }
 // Tenths of a second: the Lights block's hold and each Later block's count, both in saved states.
 m_aiTick+=dt;
 while(m_aiTick>=.1f){m_aiTick-=.1f;
  for(const auto& ai:m_aiAgents){const int left=state(lightsLeft(ai));if(left>1)setState(lightsLeft(ai),left-1);else if(left==1){setState(lightsLeft(ai),0);setState(lightsMode(ai),0);}}
  for(const auto& event:m_scriptEvents)for(size_t a=0;a<event.actions.size();++a){const auto& action=event.actions[a];if(action.type!=ScriptAction::Type::Later)continue;
   const StateId key=laterKey(event.id,a);const int left=state(key);if(left>1)setState(key,left-1);else if(left==1){setState(key,0);setState(action.id,action.value);if(m_scriptTrace)m_scriptTrace(event.id,int(a));}}
 }
}
void Game::updateSkinbot(float dt){
 const Fixture* spawn=nullptr;
 if(m_skinbotLevel!=m_level){
  m_skinbotLevel=-1;
  for(const auto& fixture:m_world.fixtures())if(fixture.model==skinbot::Model&&fixture.visible){spawn=&fixture;break;}
  if(!spawn)return;
 }
 std::vector<SkinbotProp> props;
 const Vec2 at=spawn?spawn->position:Vec2{m_skinbot.x,m_skinbot.y};
 for(size_t i=0;i<m_clutter.size();++i){
  const auto& prop=m_clutter[i];
  const float range=12.f+prop.footprint();
  if(prop.broken||lengthSq(prop.pos-at)>range*range)continue;
  props.push_back(skinbotProp(prop,i,int(i)==m_heldClutter||prop.netHolder>=0));
 }
 const skinbot::Ground ground=[&](float x,float y,float from){
  float floor=m_world.supportBelow(x,y,from);
  for(const auto& prop:props)if(prop.contact==SkinbotContact::Step&&prop.top<=from+.001f&&prop.top>floor
     &&prop.penetration({x,y},0)>=0)floor=prop.top;
  return floor;
 };
 if(spawn){
  m_skinbotHome=spawn->position;
  const float floor=m_world.floorHeight(at.x,at.y)+spawn->base;
  m_skinbot.place(at.x,at.y,spawn->yaw,ground,floor+.3f);
  m_skinbotLevel=m_level;m_skinbotStop=0;m_skinbotWait=2.f;m_skinbotPointed=false;
  m_skinbotDetour.clear();m_skinbotRetry=0;
 }
 // Its beat, until pointed elsewhere: four places round where it was put, standing dead still at each.
 if(!m_skinbotPointed&&m_skinbot.arrived()){
  m_skinbotWait-=dt;
  if(m_skinbotWait<=0){
   static const Vec2 beat[4]={{1.2f,3.f},{-3.f,3.f},{-3.f,-2.5f},{1.2f,-2.5f}};
   const auto route=skinbotRoute(m_world,m_skinbotHome);
   const size_t stop=size_t(m_skinbotStop++);
   const Vec2 at=route.empty()?m_skinbotHome+beat[stop%4]:route[stop%route.size()];
   m_skinbot.pointAt(at.x,at.y);m_skinbotWait=2.5f;m_skinbotDetour.clear();m_skinbotRetry=0;
  }
 }
 // It watches the player when it can see them: near, and nothing solid between (never through walls).
 {const float dx=m_player.pos.x-m_skinbot.x,dy=m_player.pos.y-m_skinbot.y,away=std::hypot(dx,dy);
  bool seen=away<9.f&&!dead();
  for(int k=1;seen&&k<10;++k){const float t=float(k)/10;if(m_world.supportBelow(m_skinbot.x+dx*t,m_skinbot.y+dy*t,m_skinbot.z+3.f)>m_skinbot.z+1.5f)seen=false;}
  if(seen)m_skinbot.watch({m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye});else m_skinbot.lookAway();}
 const Vec2 origin{m_skinbot.x,m_skinbot.y};
 const Vec2 goal{m_skinbot.goal.x,m_skinbot.goal.y};
 const float feet=m_skinbot.z;
 auto worldClear=[&](Vec2 point,float radius){
  for(const Vec2 offset:{Vec2{},Vec2{radius,0},Vec2{-radius,0},Vec2{0,radius},Vec2{0,-radius}}){
   const Vec2 p=point+offset;
   const float floor=m_world.supportBelow(p.x,p.y,feet+skinbot::Walker::Climb-.04f);
   if(floor<feet-1.f||!m_world.fits(p.x,p.y,floor+.04f,SkinbotBodyHeight,false))return false;
  }
  return true;
 };
 auto blocks=[&](const SkinbotProp& prop){
  if(prop.top<=feet+.06f||prop.bottom>=feet+SkinbotBodyHeight||prop.contact==SkinbotContact::Push)return false;
  return prop.contact==SkinbotContact::Block||prop.top>feet+skinbot::Walker::Climb-.035f;
 };
 auto routeClear=[&](Vec2 point){
  for(const auto& prop:props)if(blocks(prop)){
   const float depth=prop.penetration(point,SkinbotRouteRadius);
   if(depth>0&&depth>=prop.penetration(origin,SkinbotRouteRadius))return false;
  }
  return worldClear(point,SkinbotBodyRadius);
 };
 skinbot::W steering;
 const skinbot::W* waypoint=nullptr;
 if(m_skinbot.going){
  if(!m_skinbotDetour.empty()&&lengthSq(m_skinbotDetour.front()-origin)<.12f)m_skinbotDetour.erase(m_skinbotDetour.begin());
  m_skinbotRetry-=dt;
  if(m_skinbotRetry<=0){
   m_skinbotRetry=.25f;
   if(!m_skinbotDetour.empty()&&!skinbotSegmentClear(origin,m_skinbotDetour.front(),routeClear))m_skinbotDetour.clear();
   if(m_skinbotDetour.empty()){
    const Vec2 ahead=origin+normalized(goal-origin)*std::min(3.5f,length(goal-origin));
    if(!skinbotSegmentClear(origin,ahead,routeClear)){
     m_skinbotDetour=skinbotBypass(origin,goal,routeClear);
     if(m_skinbotDetour.empty())m_skinbotRetry=.65f;
    }
   }
  }
  if(!m_skinbotDetour.empty()){const Vec2 next=m_skinbotDetour.front();steering={next.x,next.y,0};waypoint=&steering;}
  const Vec2 forward{std::sin(m_skinbot.yaw),std::cos(m_skinbot.yaw)};
  const Vec2 intent=normalized((waypoint?m_skinbotDetour.front():goal)-origin);
  if(dot(intent,forward)>.6f&&m_skinbot.curse.freeze<=0){
   const Vec2 side{-forward.y,forward.x};
   for(const auto& prop:props){
    const Vec2 delta=prop.at-origin;
    if(prop.contact!=SkinbotContact::Push||prop.top<feet-.12f||prop.bottom>feet+.28f||dot(delta,forward)<0)continue;
    bool touches=prop.penetration(origin+forward*.65f,.58f)>0;
    for(const auto& foot:m_skinbot.feet)if(foot.at.z<prop.top+.06f&&foot.at.z>prop.bottom-.06f
        &&prop.penetration({foot.at.x,foot.at.y},.18f)>0)touches=true;
    if(!touches)continue;
    const auto& item=m_clutter[prop.index];
    const float flank=dot(delta,side)<0?-1.f:1.f;
    const Vec2 push=forward*.6f+side*(flank*.8f);
    const float speed=std::clamp(1.55f-dot(item.velocity,push),0.f,1.55f);
    if(speed<=.08f)continue;
    const float mass=item.mass();
    knockClutter(m_level,prop.index,{prop.at.x,prop.at.y,(prop.top+prop.bottom)*.5f},
                 {push.x*mass*speed,push.y*mass*speed,0});
   }
  }
 }
 const skinbot::BodyClear clear=[&](skinbot::W next){
  const Vec2 point{next.x,next.y};
  if(lengthSq(point-origin)<1e-9f)return true;
  for(const auto& prop:props)if(blocks(prop)){
   const float depth=prop.penetration(point,SkinbotBodyRadius);
   if(depth>0&&depth>=prop.penetration(origin,SkinbotBodyRadius))return false;
  }
  return worldClear(point,SkinbotBodyRadius);
 };
 const skinbot::FootClear footClear=[&](skinbot::W from,skinbot::W to){
  const Vec2 start{from.x,from.y},end{to.x,to.y};
  const int samples=std::max(1,int(std::ceil(length(end-start)/.12f)));
  for(const auto& prop:props)if(prop.contact==SkinbotContact::Block||blocks(prop)){
   constexpr float blade=.06f;
   const float depth=from.z>=prop.bottom-blade&&from.z<=prop.top+blade?prop.penetration(start,blade):0.f;
   for(int i=1;i<=samples;++i){
    const float t=float(i)/samples,z=from.z+(to.z-from.z)*t;
    if(z<prop.bottom-blade||z>prop.top+blade)continue;
    const float inside=prop.penetration(start+(end-start)*t,blade);
    if(inside>0&&inside>=depth)return false;
   }
  }
  return true;
 };
 // A hard hit queues a longer charge than the ordinary wind-up.
 const float queuedRush=m_skinbot.curse.freeze>0?m_skinbot.curse.rush:0.f;
 m_skinbot.update(dt,ground,clear,waypoint,footClear);
 if(m_skinbot.curse.freeze<=0)m_skinbot.curse.rush=std::max(queuedRush,m_skinbot.curse.rush);
}
bool Game::aiRemembered(bool haunt,Vec2& at,float& z)const{
 // The AI over the map being played; its memory is in that map's frame.
 for(const auto& ai:m_aiAgents){if(ai.level!=m_level)continue;const int patch=state(haunt?mostHeard(ai):lastHeard(ai))-1;if(patch<0)continue;
  at={(float(patch%6)+.5f)*Patch,(float(patch/6)+.5f)*Patch};z=m_world.floorHeight(at.x,at.y);return true;}
 return false;
}
// A lamp in a zone its AI drives: 1 as authored, 0 dark, a stutter when flickering; tint, when asked, is the gel
// the mode lays on it (emergency red).
float Game::aiLampScale(const WorldLight& lamp,int level,Point3* tint)const{
 if(lamp.directed<0)return 1;
 for(const auto& ai:m_aiAgents){if(ai.level!=level)continue;
  // The lamp's directed index is its AI's place in its own map's list.
  int index=0;for(const auto& other:m_aiAgents){if(&other==&ai)break;if(other.level==level)++index;}
  if(index!=lamp.directed)continue;
  const int mode=state(lightsMode(ai));
  if(mode==2)return 0;
  if(mode==3){if(tint)*tint={1.f,.12f,.08f};return .75f;}
  if(mode==1){
   // A failing circuit: irregular, a lamp its own rhythm (by where it hangs), mostly on with hard drops.
   const float t=float(m_elapsed)*9.f+lamp.position.x*3.7f+lamp.position.y*5.3f;
   const float n=std::sin(t)*std::sin(t*2.13f+1.7f)*std::sin(t*.61f+.4f);
   return n>.18f?.08f:n>.05f?.55f:1.f;}
  return 1;}
 return 1;
}
}
