// The AIs placed over zones of a map (AI records, ScriptDefinition.h AiAgent) and the AI blocks (Lights, Noise,
// Later script actions). An AI reads the players and writes what it decides as states; script events (the blocks
// the Builder's AI and Entity Manager snaps together) read those states and act. Nothing here knows a map: the
// Director over Chapter 2's Engineering is a zone, its knobs and the blocks wired to it, like any other.
#include "Game.h"
#include <algorithm>
#include <cmath>
namespace retro {
namespace {
StateId lightsMode(const AiAgent& ai){return stateId(ai.name+"_lights");}
StateId lightsLeft(const AiAgent& ai){return stateId(ai.name+"_lights_left");}
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
   if(!ai.contains(at.x,at.y,p.body->z))continue;++inside;if(p.local&&m_stepNoiseAge<1.f)noise=std::max(noise,m_stepNoise);}
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
