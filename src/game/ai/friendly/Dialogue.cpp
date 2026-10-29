// Squads that change sides, and conversations with choices.
//
// A squad's stance lives in a state its actors share (ActorAiConfig::stance):
// hostile, a truce, or allied with the player. Every hostility test in the AI
// asks npcFaction(), so a stance changes who a squad shoots, who it covers and
// whom it reports, the same way for every chunk and campaign.
//
// A truce holds while the player keeps the peace: a shot at one of them ends
// it at once, and a gun held on a man warns him and then ends it. An ally
// forgives one stray hit with a warning; the second turns the squad.
//
// Actors with ActorTrack::talk open a conversation on E: the speaker's line,
// then up to four replies that may need or set states and change the
// speaker's squad stance.
#include "../../Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace retro {
namespace {
constexpr float TalkRange=2.4f;
// A gun on a truce: the warning after this long, the truce gone after the longer.
constexpr int AimWarnMs=1200,AimBreakMs=4500;
// An ally's second stray hit within this window turns the squad.
constexpr int AllyForgiveMs=20000;
}

SquadStance Game::npcStance(const ActorTrack& track)const{
 if(!track.ai.stance)return SquadStance::Declared;
 int value=state(track.ai.stance);return value>=0&&value<=3?SquadStance(value):SquadStance::Declared;
}
AiFaction Game::npcFaction(const ActorTrack& track)const{
 AiFaction declared=actorFaction(track);
 switch(npcStance(track)){
  case SquadStance::Hostile:return declared==AiFaction::Creature?declared:AiFaction::Raider;
  case SquadStance::Truce:return declared==AiFaction::Creature?declared:AiFaction::Wary;
  case SquadStance::Ally:return declared==AiFaction::Creature?declared:AiFaction::Facility;
  default:return declared;
 }
}
void Game::setSquadStance(size_t actor,SquadStance stance){
 if(actor>=m_friendlyActors.size())return;const auto&track=m_friendlyActors[actor].track();if(!track.ai.stance)return;
 SquadStance old=npcStance(track);setState(track.ai.stance,int(stance));
 for(size_t i=0;i<m_friendlyActors.size();++i)if(m_friendlyActors[i].track().ai.stance==track.ai.stance)
  for(const char*field:{"stance_aim_ms","stance_warned","stance_strike_ms"})setState(friendlyKey(i,field),0);
 if(old!=SquadStance::Hostile&&stance==SquadStance::Hostile)npcVoice(actor,NpcVoiceCue::Betrayed);
 if(talking()&&m_friendlyActors[size_t(m_talk.actor)].track().ai.stance==track.ai.stance&&stance==SquadStance::Hostile)closeDialogue();
}
int Game::squadMood(size_t actor)const{
 if(actor>=m_friendlyActors.size())return 1;const auto&track=m_friendlyActors[actor].track();
 int members=0,down=0;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&t=m_friendlyActors[i].track();
  if(i!=actor&&(!track.ai.squad||t.ai.squad!=track.ai.squad))continue;++members;
  if((t.deadState&&state(t.deadState))||npcIncapacitated(i))++down;}
 bool hurt=track.health&&state(track.damageState)*2>=track.health;
 if(members>1&&down*2>=members)return 3;
 return down||hurt?2:1;
}
void Game::stanceStruck(size_t actor,AiTargetRef attacker){
 if(attacker.kind!=ShotKind::Player||actor>=m_friendlyActors.size())return;
 const auto&track=m_friendlyActors[actor].track();
 switch(npcStance(track)){
  case SquadStance::Truce:setSquadStance(actor,SquadStance::Hostile);break;
  case SquadStance::Ally:
   if(state(friendlyKey(actor,"stance_strike_ms"))>0)setSquadStance(actor,SquadStance::Hostile);
   else{for(size_t i=0;i<m_friendlyActors.size();++i)if(m_friendlyActors[i].track().ai.stance==track.ai.stance)setState(friendlyKey(i,"stance_strike_ms"),AllyForgiveMs);npcVoice(actor,NpcVoiceCue::Warn);}
   break;
  default:break;
 }
}
void Game::updateStances(float dt){
 int ms=std::max(0,int(std::round(dt*1000)));
 const bool armed=equippedWeapon()!=WeaponId::Fists;
 const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();if(!track.ai.stance)continue;
  if(int strike=state(friendlyKey(i,"stance_strike_ms")))setState(friendlyKey(i,"stance_strike_ms"),std::max(0,strike-ms));
  if(npcStance(track)!=SquadStance::Truce||!autonomousActor(track)||!chunkResident(npc.level)||(track.deadState&&state(track.deadState))||npcIncapacitated(i))continue;
  // A gun held on him: inside 10 m, within a few degrees of his chest, nothing in between.
  Vec2 at=npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level),delta=at-m_player.pos;float distance=length(delta);
  bool covered=false;
  if(armed&&!talking()&&distance>.3f&&distance<10.f){
   float across=std::fabs(delta.x*forward.y-delta.y*forward.x),along=delta.x*forward.x+delta.y*forward.y;
   Point3 eye{m_player.pos.x+chunkOffset(m_level).x,m_player.pos.y+chunkOffset(m_level).y,m_player.z+m_player.eye};
   Point3 chest{at.x+chunkOffset(m_level).x,at.y+chunkOffset(m_level).y,npc.pose.z+track.scale*.7f};
   covered=along>0&&across<std::max(.35f,along*.07f)&&friendlyRayClear(eye,chest);
  }
  int aimed=state(friendlyKey(i,"stance_aim_ms"));aimed=covered?aimed+ms:std::max(0,aimed-ms*2);setState(friendlyKey(i,"stance_aim_ms"),aimed);
  if(aimed>=AimWarnMs&&!state(friendlyKey(i,"stance_warned"))){setState(friendlyKey(i,"stance_warned"),1);npcVoice(i,NpcVoiceCue::Warn);}
  if(!aimed)setState(friendlyKey(i,"stance_warned"),0);
  if(aimed>=AimBreakMs)setSquadStance(i,SquadStance::Hostile);
 }
}

// --- Conversations ---------------------------------------------------------------------
int Game::nearbyTalker()const{
 int best=-1;float nearest=TalkRange;Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();
  if(!track.talk||!autonomousActor(track)||!chunkResident(npc.level)||npcIncapacitated(i)||npc.pose.clip==4||(track.deadState&&state(track.deadState)))continue;
  if(factionsHostile(npcFaction(track),AiFaction::Player))continue;
  if(track.ai.mode==ActorAiMode::Guard&&guardState(i)==GuardState::Alert)continue;
  Vec2 at=npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level),delta=at-m_player.pos;float distance=length(delta);
  if(distance>=nearest||std::fabs(npc.pose.z-m_player.z)>1.2f)continue;
  if(distance>.4f&&(delta.x*forward.x+delta.y*forward.y)<distance*.6f)continue;
  Point3 eye{m_player.pos.x+chunkOffset(m_level).x,m_player.pos.y+chunkOffset(m_level).y,m_player.z+m_player.eye};
  if(!friendlyRayClear(eye,{at.x+chunkOffset(m_level).x,at.y+chunkOffset(m_level).y,npc.pose.z+track.scale*.85f}))continue;
  nearest=distance;best=int(i);
 }
 return best;
}
const DialogueNode* Game::dialogueNode()const{
 if(!talking())return nullptr;const auto&npc=m_friendlyActors[size_t(m_talk.actor)];
 for(const auto&node:npc.definition->dialogue)if(node.id==m_talk.node)return &node;
 return nullptr;
}
std::vector<int> Game::dialogueReplies()const{
 std::vector<int> shown;const auto*node=dialogueNode();if(!node)return shown;
 for(size_t i=0;i<node->replies.size();++i){const auto&reply=node->replies[i];
  if(reply.require&&state(reply.require)<reply.requireValue)continue;
  if(reply.hide&&state(reply.hide))continue;
  if(shown.size()<4)shown.push_back(int(i));}
 return shown;
}
std::optional<Game::DialogueView> Game::dialogueView()const{
 const auto*node=dialogueNode();if(!node)return std::nullopt;
 DialogueView view{node->speaker,node->line,{},m_talk.selected,m_talk.age};
 for(int i:dialogueReplies())view.replies.push_back(node->replies[size_t(i)].text);
 // A line with nothing to answer is closed with E: show that as the reply.
 if(view.replies.empty())view.replies.push_back("[Leave]");
 return view;
}
bool Game::openDialogue(){
 int actor=nearbyTalker();if(actor<0)return false;
 m_talk={};m_talk.actor=actor;m_talk.node=m_friendlyActors[size_t(actor)].track().talk;m_talk.previous.use=true;
 if(!dialogueNode()){m_talk={};return false;}
 setState(friendlyKey(size_t(actor),"talking"),1);m_suppressFire=true;sound(Sound::Exit,.35f);
 // The speaker's voiced take, if one was recorded for this line.
 if(const auto*node=dialogueNode();node&&!node->voice.empty())for(size_t clip=0;clip<NpcVoiceClips.size();++clip)if(node->voice==NpcVoiceClips[clip].line){
  const auto&npc=m_friendlyActors[size_t(actor)];
  std::erase_if(m_sounds,[&](const auto&event){return event.speech>=0&&event.speaker==size_t(actor)+1;});
  m_sounds.push_back({Sound::WorkerDying,npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level),.95f,1,true,22,npc.pose.z+npc.track().scale*.85f,int(clip),size_t(actor)+1,3});break;}
 return true;
}
void Game::closeDialogue(){
 if(m_talk.actor>=0)setState(friendlyKey(size_t(m_talk.actor),"talking"),0);
 m_talk={};m_suppressFire=true;m_previousUse=true;
}
void Game::updateDialogue(const InputState& input,float dt){
 if(!talking())return;m_talk.age+=dt;
 const auto&npc=m_friendlyActors[size_t(m_talk.actor)];const auto&track=npc.track();
 Vec2 at=npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level);
 // Walked off, shot, or turned on: the conversation is over.
 if(length(at-m_player.pos)>TalkRange+1.f||!chunkResident(npc.level)||npcIncapacitated(size_t(m_talk.actor))||(track.deadState&&state(track.deadState))||factionsHostile(npcFaction(track),AiFaction::Player)){closeDialogue();return;}
 auto pressed=[&](bool now,bool before){return now&&!before;};
 const auto shown=dialogueReplies();const int count=std::max(1,int(shown.size()));
 m_talk.selected=std::clamp(m_talk.selected,0,count-1);
 if(pressed(input.menuUp,m_talk.previous.menuUp))m_talk.selected=(m_talk.selected+count-1)%count;
 if(pressed(input.menuDown,m_talk.previous.menuDown))m_talk.selected=(m_talk.selected+1)%count;
 int picked=-1;
 if(input.choice>=1&&input.choice<=count&&input.choice!=m_talk.previous.choice)picked=input.choice-1;
 if(pressed(input.menuAccept,m_talk.previous.menuAccept)||pressed(input.use,m_talk.previous.use))picked=m_talk.selected;
 // The mouse: the reply rows sit in the lower panel (see the HUD), 18 px apart.
 if(input.pointerY>=0){int row=(input.pointerY-DialogueFirstReplyY)/DialogueReplyRowHeight;if(input.pointerY>=DialogueFirstReplyY&&row>=0&&row<count){m_talk.selected=row;if(pressed(input.fire,m_talk.previous.fire))picked=row;}}
 bool leave=pressed(input.escape,m_talk.previous.escape);
 m_talk.previous=input;
 // The first half second ignores input: the E that opened it must not answer too.
 if(m_talk.age<.35f)return;
 if(leave){closeDialogue();return;}
 if(picked<0)return;
 if(shown.empty()){closeDialogue();return;}
 const auto&reply=dialogueNode()->replies[size_t(shown[size_t(picked)])];
 for(auto [id,value]:reply.sets)setState(id,value);
 size_t speaker=size_t(m_talk.actor);StateId next=reply.next;
 if(reply.stance>=0)setSquadStance(speaker,SquadStance(reply.stance));
 if(!talking())return; // the stance ended it
 if(!next){closeDialogue();return;}
 m_talk.node=next;m_talk.selected=0;m_talk.age=0;
 if(const auto*node=dialogueNode();node&&!node->voice.empty())for(size_t clip=0;clip<NpcVoiceClips.size();++clip)if(node->voice==NpcVoiceClips[clip].line){
  std::erase_if(m_sounds,[&](const auto&event){return event.speech>=0&&event.speaker==speaker+1;});
  m_sounds.push_back({Sound::WorkerDying,at,.95f,1,true,22,npc.pose.z+track.scale*.85f,int(clip),speaker+1,3});break;}
 sound(Sound::Exit,.25f,1.2f);
}
}
