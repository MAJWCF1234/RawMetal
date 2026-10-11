#include "Game.h"
namespace retro {
namespace {
StateId squadKey(AiFaction faction,StateId squad){std::string value="npc/squad/"+std::to_string(int(faction))+"/"+std::to_string(squad)+"/cursor";StateId hash=2166136261u;for(unsigned char c:value)hash=(hash^c)*16777619u;return hash|0x80000000u;}
}
AiTarget Game::guardSquadReport(size_t actor)const{
 const auto&npc=m_friendlyActors[actor];const auto&c=npc.track().ai;if(!c.squad)return {};auto faction=actorFaction(npc.track());AiTarget report;float best=1e9f;auto origin=chunkOffset(npc.level);
 for(size_t i=0;i<m_friendlyActors.size();++i){if(i==actor)continue;const auto&source=m_friendlyActors[i];const auto&t=source.track();
  if(t.ai.mode!=ActorAiMode::Guard||t.ai.squad!=c.squad||actorFaction(t)!=faction||!chunkResident(source.level)||!autonomousActor(t)||source.pose.clip==4||(t.deadState&&state(t.deadState))||guardState(i)!=GuardState::Alert||!state(friendlyKey(i,"guard_seen")))continue;
  auto delta=source.pose.position+chunkOffset(source.level)-origin-npc.pose.position;float height=source.pose.z-npc.pose.z,range=lengthSq(delta)+height*height;if(range>18*18||range>=best)continue;auto target=guardTarget(i);if(!guardCanSee(i,source.pose,target))continue;
  if(!factionsHostile(faction,target.faction))continue;target.position+=chunkOffset(source.level)-origin;report=target;best=range;
 }return report;
}
bool Game::guardFireSlot(size_t actor)const{return !m_friendlyActors[actor].track().ai.squad||state(friendlyKey(actor,"guard_slot"))!=0;}
void Game::updateGuardSquadSlots(float dt){
 struct Group{StateId key;std::vector<size_t> actors;};std::vector<Group> groups;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();if(t.ai.mode!=ActorAiMode::Guard||!t.ai.squad)continue;
  int lease=std::max(0,state(friendlyKey(i,"guard_slot_ms"))-npcMilliseconds(i,dt,"squad_fraction"));auto target=guardTarget(i);
  bool eligible=t.ai.armed&&chunkResident(npc.level)&&autonomousActor(t)&&npc.pose.clip!=4&&(!t.deadState||!state(t.deadState))&&guardState(i)==GuardState::Alert&&npc.pose.clip!=1&&!state(friendlyKey(i,"weapon_reload"))&&!state(friendlyKey(i,"weapon_recovery"))&&!guardMeleeReady(i,npc.pose,target)&&guardCanSee(i,npc.pose,target)&&guardShotMatches(i,guardShot(i,npc.pose,npcWeaponPoseAt(i,npc.pose,{target.position.x,target.position.y,target.feet+target.height*.6f},1)));
  eligible=eligible&&(!guardFlankSide(i)||state(friendlyKey(i,"guard_flank_done")));if(!eligible||!lease){lease=0;setState(friendlyKey(i,"guard_slot"),0);}setState(friendlyKey(i,"guard_slot_ms"),lease);if(!eligible)continue;
  auto key=squadKey(actorFaction(t),t.ai.squad);auto group=std::find_if(groups.begin(),groups.end(),[&](const Group&g){return g.key==key;});if(group==groups.end()){groups.push_back({key,{}});group=std::prev(groups.end());}group->actors.push_back(i);
 }
 for(const auto&group:groups){int occupied=0;for(auto i:group.actors)occupied+=state(friendlyKey(i,"guard_slot"))!=0;
  int cursor=std::clamp(state(group.key),0,int(m_friendlyActors.size()));for(size_t pass=0;pass<group.actors.size()&&occupied<2;++pass){auto found=std::lower_bound(group.actors.begin(),group.actors.end(),size_t(cursor));if(found==group.actors.end())found=group.actors.begin();auto i=*found;cursor=int(i)+1;
   if(state(friendlyKey(i,"guard_slot")))continue;setState(friendlyKey(i,"guard_slot"),1);setState(friendlyKey(i,"guard_slot_ms"),std::max(1600,m_friendlyActors[i].track().ai.aimMs+500));++occupied;
  }setState(group.key,cursor);
 }
}
int Game::guardFlankSide(size_t actor)const{
 const auto&npc=m_friendlyActors[actor];if(!npc.track().ai.squad)return 0;auto target=guardTarget(actor);if(!target.valid())return 0;int rank=0,count=0;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&other=m_friendlyActors[i];const auto&t=other.track();if(t.ai.mode!=ActorAiMode::Guard||!t.ai.armed||t.ai.squad!=npc.track().ai.squad||actorFaction(t)!=actorFaction(npc.track())||!autonomousActor(t)||other.pose.clip==4||(t.deadState&&state(t.deadState))||!chunkResident(other.level))continue;
  auto selected=guardTarget(i);if(selected.kind!=target.kind||selected.index!=target.index||selected.level!=target.level)continue;auto delta=other.pose.position+chunkOffset(other.level)-chunkOffset(npc.level)-npc.pose.position;if(lengthSq(delta)>18*18)continue;if(i<actor)++rank;++count;
 }return count>1?(rank%3==1?-1:rank%3==2?1:0):0;
}
bool Game::guardPositionReserved(size_t actor,Vec2 position,float feet)const{
 const auto&npc=m_friendlyActors[actor];if(!npc.track().ai.squad)return false;auto origin=chunkOffset(npc.level);
 for(size_t i=0;i<m_friendlyActors.size();++i){if(i==actor)continue;const auto&other=m_friendlyActors[i];if(!chunkResident(other.level)||!autonomousActor(other.track())||other.pose.clip==4||(other.track().deadState&&state(other.track().deadState))||other.track().ai.squad!=npc.track().ai.squad||actorFaction(other.track())!=actorFaction(npc.track()))continue;
  auto current=other.pose.position+chunkOffset(other.level)-origin;if(std::fabs(feet-other.pose.z)<npc.track().scale&&lengthSq(current-position)<.7f*.7f)return true;
  if(!state(friendlyKey(i,"guard_cover")))continue;
  Vec2 goal{state(friendlyKey(i,"guard_cover_x"))*.001f,state(friendlyKey(i,"guard_cover_y"))*.001f};goal+=chunkOffset(other.level)-origin;if(std::fabs(feet-state(friendlyKey(i,"guard_cover_z"))*.001f)<npc.track().scale&&lengthSq(goal-position)<.7f*.7f)return true;
 }return false;
}
}
