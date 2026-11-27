// The AIs placed over zones of a map (AI records, ScriptDefinition.h AiAgent) and the AI blocks (Lights, Noise,
// Later script actions). An AI reads the players and writes what it decides as states; script events (the blocks
// the Builder's AI and Entity Manager snaps together) read those states and act. Nothing here knows a map: the
// Director over Chapter 2's Engineering is a zone, its knobs and the blocks wired to it, like any other.
#include "Game.h"
#include <algorithm>
#include <cmath>
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
 const skinbot::Ground ground=[this](float x,float y,float from){return m_world.supportBelow(x,y,from);};
 if(m_skinbotLevel!=m_level){
  m_skinbotLevel=-1;
  for(const auto& f:m_world.fixtures())if(f.model==skinbot::Model&&f.visible){
   m_skinbotHome=f.position;const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
   m_skinbot.place(f.position.x,f.position.y,f.yaw,ground,floor+.3f);m_skinbotLevel=m_level;m_skinbotStop=0;m_skinbotWait=2.f;m_skinbotPointed=false;break;}
  if(m_skinbotLevel<0)return;
 }
 // Its beat, until pointed elsewhere: four places round where it was put, standing dead still at each.
 if(!m_skinbotPointed&&m_skinbot.arrived()){
  m_skinbotWait-=dt;
  if(m_skinbotWait<=0){
   static const Vec2 beat[4]={{1.2f,3.f},{-3.f,3.f},{-3.f,-2.5f},{1.2f,-2.5f}};
   const auto route=skinbotRoute(m_world,m_skinbotHome);
   const size_t stop=size_t(m_skinbotStop++);
   const Vec2 at=route.empty()?m_skinbotHome+beat[stop%4]:route[stop%route.size()];
   m_skinbot.pointAt(at.x,at.y);m_skinbotWait=2.5f;
  }
 }
 // It watches the player when it can see them: near, and nothing solid between (never through walls).
 {const float dx=m_player.pos.x-m_skinbot.x,dy=m_player.pos.y-m_skinbot.y,away=std::hypot(dx,dy);
  bool seen=away<9.f&&!dead();
  for(int k=1;seen&&k<10;++k){const float t=float(k)/10;if(m_world.supportBelow(m_skinbot.x+dx*t,m_skinbot.y+dy*t,m_skinbot.z+3.f)>m_skinbot.z+1.5f)seen=false;}
  if(seen)m_skinbot.watch({m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye});else m_skinbot.lookAway();}
 m_skinbot.update(dt,ground);
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
