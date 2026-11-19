// PAUSE and SETTINGS, the chat line and the voice upkeep (docs/Systems/multiplayer.md "Chat and voice"; the menus'
// layouts are PauseMenuLayout and SettingsLayout in Game.h, drawn by renderer/FrontEnd.cpp).
//  - PAUSE: a short list in the title's language. RESTART and QUIT TO TITLE ask for a second press. In a match:
//    RESUME, SETTINGS, the join code (Enter copies it) and LEAVE MATCH.
//  - SETTINGS: pages in a column (AUDIO, VIDEO, CONTROLS, GAMEPLAY, COMMUNICATION, VR) and BACK. The column has the
//    keys first: up and down turn the pages, right or Enter goes into the rows, Esc leaves. In the rows, left and
//    right change a value, Enter toggles or acts, Esc goes back to the column. The pointer (mouse or VR laser) clicks
//    a page, hovers and clicks rows and drags sliders. Every page ends with RESET TO DEFAULTS (a second press).
//    Changes apply at once; leaving SETTINGS asks main to save them (takeSettingsSave).
//  - Chat: T opens the line in a match (not in VR), Enter sends through the Match, Esc drops it. The world runs on
//    while typing; the player stands.
//  - Voice: the COMMUNICATION settings go to the session whenever they change, the push-to-talk key every frame,
//    and TEST MY MIC listens with its own MicMonitor while the page is open.
#include "Game.h"
#include "../audio/MicMonitor.h"
#include "../net/Match.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ostream>
namespace retro {
bool copyTextToClipboard(const std::string& text);   // MultiplayerMenu.cpp
namespace {
enum Row:int{RMaster=1,RMusic,REffects,RVoiceVolume,RRenderScale,RDepth,RLens,RFrameTime,RSensitivity,RInvert,RPull,RCharacter,
 RTextChat,RChatFilter,RVoiceMode,RTalkKey,ROpenMic,RMic,RSpeakers,RMicVolume,RProximity,RMuted,RMicTest,
 RVrTurn,RVrVignette,RVrSeated,RVrHeight,RVrLefty,RVrScale,RVrRate,RReset,RMutedBack,RMutedName=200};
constexpr int TabCount=int(SettingsTab::Count);   // the page column's rows; BACK is row TabCount
struct TalkKey{int code;const char* name;};
constexpr TalkKey TalkKeys[]={{0x56,"V"},{0x42,"B"},{0x58,"X"},{0x48,"H"},{0x14,"CAPS LOCK"},{0xA4,"LEFT ALT"},{0x05,"MOUSE 4"},{0x06,"MOUSE 5"}};
const char* talkKeyName(int code){for(const auto& k:TalkKeys)if(k.code==code)return k.name;return "V";}
bool slider(int id,float& low,float& high,float& step){
 switch(id){
  case RMaster:case RMusic:case REffects:low=0;high=1;step=.05f;return true;
  case RVoiceVolume:case RMicVolume:low=0;high=2;step=.1f;return true;
  case ROpenMic:low=0;high=1;step=.05f;return true;
  case RSensitivity:low=.2f;high=3;step=.1f;return true;
  case RDepth:case RLens:low=0;high=2;step=.1f;return true;
  case RVrTurn:low=.25f;high=2;step=.125f;return true;
  case RVrVignette:low=0;high=1;step=.1f;return true;
  default:return false;
 }
}
std::string percent(float v){char b[16];std::snprintf(b,sizeof b,"%d%%",int(std::lround(v*100)));return b;}
std::string hundred(float v){char b[16];std::snprintf(b,sizeof b,"%d",int(std::lround(v*100)));return b;}
}

const char* Game::settingsTabName(SettingsTab tab){
 switch(tab){case SettingsTab::Audio:return "AUDIO";case SettingsTab::Video:return "VIDEO";case SettingsTab::Controls:return "CONTROLS";
  case SettingsTab::Gameplay:return "GAMEPLAY";case SettingsTab::Communication:return "COMMUNICATION";case SettingsTab::Vr:return "VR";default:return "BACK";}
}

// The players the muted list offers: those in the match now (not this player), then any muted name not here.
static std::vector<std::string> mutedListNames(const Game& g){
 std::vector<std::string> names;
 for(const auto& r:g.matchView().scores)if(!r.self&&std::find(names.begin(),names.end(),r.name)==names.end())names.push_back(r.name);
 for(const auto& n:g.settings().mutedPlayers)if(std::find(names.begin(),names.end(),n)==names.end())names.push_back(n);
 return names;
}

std::vector<int> Game::settingsRows()const{
 if(m_settingsMuted){std::vector<int> r{RMutedBack};const auto names=mutedListNames(*this);for(size_t i=0;i<names.size()&&i<12;++i)r.push_back(RMutedName+int(i));return r;}
 switch(m_settingsTab){
  case SettingsTab::Audio:return {RMaster,RMusic,REffects,RVoiceVolume,RReset};
  case SettingsTab::Video:return {RRenderScale,RDepth,RLens,RFrameTime,RReset};
  case SettingsTab::Controls:return {RSensitivity,RInvert,RReset};
  case SettingsTab::Gameplay:return {RPull,RCharacter,RReset};
  case SettingsTab::Communication:return {RTextChat,RChatFilter,RVoiceMode,RTalkKey,ROpenMic,RMic,RSpeakers,RMicVolume,RVoiceVolume,RProximity,RMuted,RMicTest,RReset};
  case SettingsTab::Vr:return {RVrTurn,RVrVignette,RVrSeated,RVrHeight,RVrLefty,RVrScale,RVrRate,RReset};
  default:return {RReset};
 }
}

std::vector<MenuItem> Game::settingsItems()const{
 std::vector<MenuItem> items;const auto& s=m_settings;
 for(int t=0;t<=TabCount;++t){MenuItem m;m.kind=t<TabCount?MenuItem::Tab:MenuItem::Back;m.x=SettingsLayout::TabX;m.y=SettingsLayout::tabY(t);m.w=SettingsLayout::TabWidth;m.h=SettingsLayout::TabHeight;
  m.label=settingsTabName(SettingsTab(t));m.current=t==int(m_settingsTab);m.id=1000+t;
  static const char* tabHelp[]={"How loud the dark is: the world, the music, the others' voices.","What the lens does to what you see.","Your hands on the mouse, and the keys.",
   "Small aids, and who the others see.","Text chat and voice with the others in a match.","Comfort in a headset: turning, the vignette, seated play.",
   m_menuFromTitle?"Back to the title. Changes are kept.":"Back to the pause menu. Changes are kept."};
  m.help=tabHelp[t];items.push_back(m);}
 const auto rows=settingsRows();
 const bool voiceOk=m_netSession&&m_netSession->voiceStatus().available;
 const std::vector<net::AudioDevice> inputs=m_netSession?m_netSession->inputDevices():std::vector<net::AudioDevice>{},outputs=m_netSession?m_netSession->outputDevices():std::vector<net::AudioDevice>{};
 const auto names=mutedListNames(*this);
 const float meter=micMeter();
 for(size_t i=0;i<rows.size();++i){const int id=rows[i];MenuItem m;m.x=SettingsLayout::RowX;m.y=SettingsLayout::rowY(int(i));m.w=SettingsLayout::RowWidth;m.h=SettingsLayout::RowHeight;m.id=id;m.kind=MenuItem::Toggle;
  m.armed=m_menuArmed==int(i);
  float low=0,high=1,step=.1f;
  auto value=[&]()->float{switch(id){case RMaster:return s.master;case RMusic:return s.music;case REffects:return s.effects;case RVoiceVolume:return s.voiceVolume;case RMicVolume:return s.micVolume;
   case ROpenMic:return s.openMicThreshold;case RSensitivity:return s.sensitivity;case RDepth:return s.depthOfField;case RLens:return s.lensEffects;case RVrTurn:return s.vrTurnSpeed;case RVrVignette:return s.vrVignette;default:return 0;}};
  if(slider(id,low,high,step)){m.kind=MenuItem::Slider;const float v=value();m.fraction=std::clamp((v-low)/(high-low),0.f,1.f);}
  auto onOff=[](bool b){return std::string(b?"ON":"OFF");};
  switch(id){
   case RMaster:m.label="MASTER VOLUME";m.value=hundred(s.master);m.help="Everything you hear, together.";break;
   case RMusic:m.label="MUSIC VOLUME";m.value=hundred(s.music);m.help="The score under the dark.";break;
   case REffects:m.label="EFFECTS VOLUME";m.value=hundred(s.effects);m.help="Footsteps, doors, machines, gunfire and what moves in the walls.";break;
   case RVoiceVolume:m.label="VOICE CHAT VOLUME";m.value=percent(s.voiceVolume);m.help="How loud the other players' voices are. 100% is as they come.";break;
   case RRenderScale:m.kind=MenuItem::Choice;m.label="RENDER SCALE";m.value=percent(m_renderScale);m.help="The 3D view's resolution. Lower is faster; the HUD stays sharp.";break;
   case RDepth:m.label="DEPTH OF FIELD";m.value=percent(s.depthOfField);m.help="How much the lens blurs what you are not looking at. 100% is the intended look.";break;
   case RLens:m.label="LENS EFFECTS";m.value=percent(s.lensEffects);m.help="Grain, glow and the edge of the lens. 100% is the intended look.";break;
   case RFrameTime:m.label="FRAME TIME READOUT";m.value=onOff(m_showFps);m.help="Shows how long each frame takes to draw, at the bottom left.";break;
   case RSensitivity:{m.label="MOUSE SENSITIVITY";char b[16];std::snprintf(b,sizeof b,"%.1fX",s.sensitivity);m.value=b;m.help="How far a hand on the mouse turns your head.";break;}
   case RInvert:m.label="INVERT MOUSE Y";m.value=onOff(s.invertMouse);m.warn=s.invertMouse;m.help="Pushing the mouse forward looks down instead of up.";break;
   case RPull:m.kind=MenuItem::Choice;m.label="PULL HIGHLIGHT";m.value=s.pullHighlight?"SUBTLE":"NONE";m.help="The outline on what the Physgloves would pull. The reticle tightens either way.";break;
   case RCharacter:m.kind=MenuItem::Choice;m.label="CHARACTER";m.value=s.character==1?"THE LIMITATOR":"CLAIRE";m.help="Who the others see in multiplayer. It takes hold the next time you join.";break;
   case RTextChat:m.label="TEXT CHAT";m.value=onOff(s.textChat);m.help="Press T in a match to type. Off hides the others' lines and keeps the notices.";break;
   case RChatFilter:m.label="CHAT FILTER";m.value=onOff(s.chatFilter);m.help="Masks a short list of foul words with asterisks in what you read.";break;
   case RVoiceMode:m.kind=MenuItem::Choice;m.label="VOICE CHAT";m.value=net::voiceModeName(net::VoiceMode(std::clamp(s.voiceMode,0,2)));m.warn=s.voiceMode==0;
    m.help=s.voiceMode==1?"Hold the push-to-talk key to speak.":s.voiceMode==2?"You are heard whenever the mic is louder than the sensitivity mark.":"No voice at all: you neither speak nor hear the others.";break;
   case RTalkKey:m.kind=MenuItem::Choice;m.label="PUSH-TO-TALK KEY";m.value=talkKeyName(s.pushToTalkKey);m.enabled=s.voiceMode==1;m.help="The key you hold to be heard.";break;
   case ROpenMic:m.label="OPEN MIC SENSITIVITY";m.value=hundred(s.openMicThreshold);m.meter=meter;m.enabled=s.voiceMode==2;
    m.help=meter<0?"Turn on TEST MY MIC to see your level against the mark.":"Speak: the bar is your level. Above the mark, open mic sends.";break;
   case RMic:{m.kind=MenuItem::Choice;m.label="MICROPHONE";m.value=s.micDevice.empty()?"SYSTEM DEFAULT":s.micDevice;
    bool found=s.micDevice.empty();for(const auto& d:inputs)found|=d.name==s.micDevice;if(!found)m.warn=true;
    m.help=inputs.empty()?(voiceOk?"No microphone was found. Plug one in; the list refreshes.":"The list of microphones comes with the online service."):"The microphone the others hear.";break;}
   case RSpeakers:{m.kind=MenuItem::Choice;m.label="SPEAKERS";m.value=s.speakerDevice.empty()?"SYSTEM DEFAULT":s.speakerDevice;
    bool found=s.speakerDevice.empty();for(const auto& d:outputs)found|=d.name==s.speakerDevice;if(!found)m.warn=true;
    m.help=outputs.empty()?"The list of speakers comes with the online service.":"Where the others' voices come out.";break;}
   case RMicVolume:m.label="MIC VOLUME";m.value=percent(s.micVolume);m.help="How loud you are to the others. 100% is as the mic hears you.";break;
   case RProximity:m.label="PROXIMITY VOICE IN CO-OP";m.value=onOff(s.proximityVoice);m.help="In co-op the others fade with distance and walls. Deathmatch is always heard in full.";break;
   case RMuted:m.kind=MenuItem::Action;m.label="MUTED PLAYERS";m.value=s.mutedPlayers.empty()?"NONE":std::to_string(s.mutedPlayers.size());m.help="Who you will not hear or read. Enter opens the list to mute or unmute.";break;
   case RMicTest:m.label="TEST MY MIC";m.value=m_micTest?"LISTENING":"OFF";m.warn=m_micTest;m.meter=m_micTest?meter:-1.f;
    m.help=m_micTest?(m_micMonitor&&!m_micMonitor->open()?m_micMonitor->error()+".":"Speak and watch the bar. Nothing is sent."):"Listens to your microphone here, so you can see it working.";break;
   case RVrTurn:{m.label="TURN SPEED";char b[16];std::snprintf(b,sizeof b,"%d/S",int(std::lround(s.vrTurnSpeed*120)));m.value=b;m.help="How fast the stick turns you. Turning is always smooth.";break;}
   case RVrVignette:m.label="COMFORT VIGNETTE";m.value=s.vrVignette<=0?"OFF":percent(s.vrVignette);m.help="Darkens the edges of the view while you move or turn.";break;
   case RVrSeated:m.label="SEATED MODE";m.value=onOff(s.vrSeated);m.help="Play sitting down: the game stands you up to her height.";break;
   case RVrHeight:m.kind=MenuItem::Action;m.label="RECALIBRATE HEIGHT";m.value="";m.help="Stand as you play, then press: your head's height now is her eye.";break;
   case RVrLefty:m.label="LEFT-HANDED";m.value=onOff(s.vrLeftHanded);m.help="The gun hand, the hip holster and the sticks swap sides.";break;
   case RVrScale:m.kind=MenuItem::Choice;m.label="RENDER SCALE";m.value=s.vrRenderScale<=0?"AUTO":percent(s.vrRenderScale);m.help="The headset's resolution. AUTO lowers it when frames run late.";break;
   case RVrRate:m.label="FRAME RATE READOUT";m.value=onOff(s.vrFrameRate);m.help="The headset's frame rate and GPU time, under your view.";break;
   case RReset:m.kind=MenuItem::Action;m.label=m.armed?"RESET: PRESS AGAIN":"RESET TO DEFAULTS";m.warn=m.armed;
    m.help=m.armed?"Press again to put this page back as it came. Move away to keep it.":m_settingsTab==SettingsTab::Communication?"This page as it came. Muted players stay muted.":"This page as it came.";break;
   case RMutedBack:m.kind=MenuItem::Back;m.label="BACK";m.help=names.empty()?"Nobody to mute: nobody else is down here, and nobody is muted.":"Back to COMMUNICATION.";break;
   default:if(id>=RMutedName&&id-RMutedName<int(names.size())){const auto& n=names[size_t(id-RMutedName)];const bool muted=s.muted(n);
     m.label=n;m.value=muted?"MUTED":"HEARD";m.warn=muted;m.help=muted?"Enter to hear them again.":"Enter to mute them: no voice, no chat.";}
    break;
  }
  items.push_back(m);
 }
 return items;
}

void Game::openSettings(SettingsTab tab){
 m_menuPage=MenuPage::Options;m_settingsTab=tab;m_settingsOnTabs=true;m_settingsMuted=false;m_menuSelection=int(tab);m_menuArmed=-1;m_dragSlider=-1;m_menuMessage.clear();
}
void Game::leaveSettings(){
 m_settingsSaveDue=true;m_micTest=false;m_micMonitor.reset();m_settingsMuted=false;m_menuArmed=-1;m_dragSlider=-1;
 if(m_menuFromTitle){showTitleScreen();m_titleSelection=TitleSettings;return;}
 m_menuPage=MenuPage::Pause;m_menuSelection=PauseSettings;m_menuMessage.clear();
}
void Game::settingsEscape(){
 m_menuArmed=-1;m_dragSlider=-1;
 if(m_settingsMuted){m_settingsMuted=false;const auto rows=settingsRows();m_menuSelection=int(std::find(rows.begin(),rows.end(),int(RMuted))-rows.begin());return;}
 if(!m_settingsOnTabs){m_settingsOnTabs=true;m_menuSelection=int(m_settingsTab);return;}
 leaveSettings();
}

void Game::settingsReset(SettingsTab tab){
 const Settings d;auto& s=m_settings;
 switch(tab){
  case SettingsTab::Audio:s.master=d.master;s.music=d.music;s.effects=d.effects;s.voiceVolume=d.voiceVolume;m_audioMuted=false;m_musicEnabled=true;break;
  case SettingsTab::Video:s.depthOfField=d.depthOfField;s.lensEffects=d.lensEffects;m_renderScale=1;m_showFps=false;break;
  case SettingsTab::Controls:s.sensitivity=d.sensitivity;s.invertMouse=d.invertMouse;break;
  case SettingsTab::Gameplay:s.pullHighlight=d.pullHighlight;s.character=d.character;break;
  case SettingsTab::Communication:s.textChat=d.textChat;s.chatFilter=d.chatFilter;s.voiceMode=d.voiceMode;s.pushToTalkKey=d.pushToTalkKey;s.openMicThreshold=d.openMicThreshold;
   s.micDevice=d.micDevice;s.speakerDevice=d.speakerDevice;s.micVolume=d.micVolume;s.voiceVolume=d.voiceVolume;s.proximityVoice=d.proximityVoice;break;
  case SettingsTab::Vr:s.vrTurnSpeed=d.vrTurnSpeed;s.vrVignette=d.vrVignette;s.vrSeated=d.vrSeated;s.vrLeftHanded=d.vrLeftHanded;s.vrRenderScale=d.vrRenderScale;s.vrFrameRate=d.vrFrameRate;break;
  default:break;
 }
}

void Game::settingsDrag(int id,float fraction){
 float low=0,high=1,step=.1f;if(!slider(id,low,high,step))return;
 const float v=low+(high-low)*std::clamp(fraction,0.f,1.f);auto& s=m_settings;
 switch(id){case RMaster:s.master=v;m_audioMuted=false;break;case RMusic:s.music=v;m_musicEnabled=true;break;case REffects:s.effects=v;break;case RVoiceVolume:s.voiceVolume=v;break;
  case RMicVolume:s.micVolume=v;break;case ROpenMic:s.openMicThreshold=v;break;case RSensitivity:s.sensitivity=v;break;case RDepth:s.depthOfField=v;break;case RLens:s.lensEffects=v;break;
  case RVrTurn:s.vrTurnSpeed=v;break;case RVrVignette:s.vrVignette=v;break;default:break;}
}

// One row's change: direction -1/+1 from left and right (or a click), activate from Enter or a click.
void Game::settingsChange(int id,int direction,bool activate){
 auto& s=m_settings;const int step=direction?direction:activate?1:0;
 float low=0,high=1,inc=.1f;
 if(slider(id,low,high,inc)){
  if(!direction)return;
  float* v=nullptr;
  switch(id){case RMaster:v=&s.master;m_audioMuted=false;break;case RMusic:v=&s.music;m_musicEnabled=true;break;case REffects:v=&s.effects;break;case RVoiceVolume:v=&s.voiceVolume;break;
   case RMicVolume:v=&s.micVolume;break;case ROpenMic:v=&s.openMicThreshold;break;case RSensitivity:v=&s.sensitivity;break;case RDepth:v=&s.depthOfField;break;case RLens:v=&s.lensEffects;break;
   case RVrTurn:v=&s.vrTurnSpeed;break;case RVrVignette:v=&s.vrVignette;break;default:break;}
  if(v)*v=std::clamp(std::round((*v+direction*inc)/inc)*inc,low,high);
  return;
 }
 if(!step)return;
 auto cycle=[&](int at,int count){return ((at+step)%count+count)%count;};
 switch(id){
  case RRenderScale:{static constexpr float steps[]={.5f,.75f,1};int at=2;for(int i=0;i<3;++i)if(std::fabs(steps[i]-m_renderScale)<.01f)at=i;m_renderScale=steps[cycle(at,3)];break;}
  case RFrameTime:m_showFps=!m_showFps;break;
  case RInvert:s.invertMouse=!s.invertMouse;break;
  case RPull:s.pullHighlight=s.pullHighlight?0:1;break;
  case RCharacter:s.character=cycle(std::clamp(s.character,0,1),2);break;
  case RTextChat:s.textChat=!s.textChat;break;
  case RChatFilter:s.chatFilter=!s.chatFilter;break;
  case RVoiceMode:s.voiceMode=cycle(std::clamp(s.voiceMode,0,2),3);break;
  case RTalkKey:{int at=0;for(int i=0;i<int(std::size(TalkKeys));++i)if(TalkKeys[i].code==s.pushToTalkKey)at=i;s.pushToTalkKey=TalkKeys[cycle(at,int(std::size(TalkKeys)))].code;break;}
  case RMic:case RSpeakers:{
   const auto list=m_netSession?(id==RMic?m_netSession->inputDevices():m_netSession->outputDevices()):std::vector<net::AudioDevice>{};
   std::vector<std::string> names{""};for(const auto& d:list)if(std::find(names.begin(),names.end(),d.name)==names.end())names.push_back(d.name);
   std::string& chosen=id==RMic?s.micDevice:s.speakerDevice;int at=0;for(int i=0;i<int(names.size());++i)if(names[size_t(i)]==chosen)at=i;
   chosen=names[size_t(cycle(at,int(names.size())))];
   if(id==RMic&&m_micTest)m_micMonitor.reset();   // listen on the new one
   break;}
  case RProximity:s.proximityVoice=!s.proximityVoice;break;
  case RMuted:if(activate){m_settingsMuted=true;m_menuSelection=0;m_menuArmed=-1;}break;
  case RMicTest:m_micTest=!m_micTest;if(!m_micTest)m_micMonitor.reset();break;
  case RVrSeated:s.vrSeated=!s.vrSeated;break;
  case RVrHeight:if(activate){m_vrRecalibrate=true;m_menuMessage="HEIGHT SET TO YOUR HEAD NOW";}break;
  case RVrLefty:s.vrLeftHanded=!s.vrLeftHanded;break;
  case RVrScale:{static constexpr float steps[]={0,1,.9f,.8f,.7f};int at=0;for(int i=0;i<5;++i)if(std::fabs(steps[i]-s.vrRenderScale)<.01f)at=i;s.vrRenderScale=steps[cycle(at,5)];break;}
  case RVrRate:s.vrFrameRate=!s.vrFrameRate;break;
  case RReset:if(!activate)break;
   if(m_menuArmed==m_menuSelection){settingsReset(m_settingsTab);m_menuArmed=-1;m_menuMessage="PAGE RESET";}else m_menuArmed=m_menuSelection;break;
  case RMutedBack:if(activate)settingsEscape();break;
  default:
   if(id>=RMutedName){const auto names=mutedListNames(*this);const size_t i=size_t(id-RMutedName);if(i<names.size())setPlayerMuted(names[i],!s.muted(names[i]));}
   break;
 }
}

void Game::updateSettingsMenu(const InputState& input){
 auto pressed=[](bool now,bool previous){return now&&!previous;};
 const auto rows=settingsRows();const int rowCount=int(rows.size());
 const bool click=pressed(input.fire,m_menuPrevious.fire),moved=input.pointerX!=m_pointerX||input.pointerY!=m_pointerY;
 if(!input.fire)m_dragSlider=-1;
 // A held slider follows the pointer wherever it goes until the button is let go.
 if(m_dragSlider>=0&&m_dragSlider<rowCount){m_settingsOnTabs=false;m_menuSelection=m_dragSlider;settingsDrag(rows[size_t(m_dragSlider)],float(input.pointerX-SettingsLayout::SliderX)/SettingsLayout::SliderWidth);
  m_pointerX=input.pointerX;m_pointerY=input.pointerY;m_menuPrevious=input;return;}
 int tabUnder=-1,rowUnder=-1;
 for(int t=0;t<=TabCount;++t){const int y=SettingsLayout::tabY(t);if(input.pointerX>=SettingsLayout::TabX-12&&input.pointerX<SettingsLayout::TabX+SettingsLayout::TabWidth&&input.pointerY>=y&&input.pointerY<y+SettingsLayout::TabHeight)tabUnder=t;}
 if(input.pointerX>=SettingsLayout::RowX-12&&input.pointerX<SettingsLayout::RowX+SettingsLayout::RowWidth&&input.pointerY>=SettingsLayout::RowY&&input.pointerY<SettingsLayout::rowY(rowCount))
  rowUnder=(input.pointerY-SettingsLayout::RowY)/SettingsLayout::RowHeight;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 if(tabUnder>=0&&click){
  m_menuArmed=-1;
  if(tabUnder==TabCount){leaveSettings();m_menuPrevious=input;return;}
  if(SettingsTab(tabUnder)!=m_settingsTab||m_settingsMuted){m_settingsTab=SettingsTab(tabUnder);m_settingsMuted=false;}
  m_settingsOnTabs=true;m_menuSelection=tabUnder;m_menuPrevious=input;return;
 }
 if(rowUnder>=0&&(moved||click)){if(m_settingsOnTabs||m_menuSelection!=rowUnder)m_menuArmed=-1;m_settingsOnTabs=false;m_menuSelection=rowUnder;}
 if(rowUnder>=0&&click){
  const int id=rows[size_t(rowUnder)];float low,high,step;
  if(slider(id,low,high,step)){if(input.pointerX>=SettingsLayout::SliderX-6&&input.pointerX<=SettingsLayout::SliderX+SettingsLayout::SliderWidth+6){m_dragSlider=rowUnder;settingsDrag(id,float(input.pointerX-SettingsLayout::SliderX)/SettingsLayout::SliderWidth);}}
  else settingsChange(id,0,true);
  m_menuPrevious=input;return;
 }
 const int vertical=int(pressed(input.menuDown,m_menuPrevious.menuDown))-int(pressed(input.menuUp,m_menuPrevious.menuUp));
 const int across=int(pressed(input.menuRight,m_menuPrevious.menuRight))-int(pressed(input.menuLeft,m_menuPrevious.menuLeft));
 const bool accept=pressed(input.menuAccept,m_menuPrevious.menuAccept);
 if(m_settingsOnTabs){
  // The column: up and down turn the pages as they go; BACK sits under them.
  if(vertical){m_menuSelection=((std::clamp(m_menuSelection,0,TabCount)+vertical)%(TabCount+1)+TabCount+1)%(TabCount+1);if(m_menuSelection<TabCount){m_settingsTab=SettingsTab(m_menuSelection);m_settingsMuted=false;}}
  if(accept||across>0){if(m_menuSelection>=TabCount){leaveSettings();m_menuPrevious=input;return;}m_settingsOnTabs=false;m_menuSelection=0;m_menuArmed=-1;}
 }else if(rowCount>0){
  m_menuSelection=std::clamp(m_menuSelection,0,rowCount-1);
  if(vertical){m_menuSelection=((m_menuSelection+vertical)%rowCount+rowCount)%rowCount;m_menuArmed=-1;}
  const int id=rows[size_t(m_menuSelection)];
  if(across)settingsChange(id,across,false);
  if(accept)settingsChange(id,0,true);
 }
 m_menuPrevious=input;
}

void Game::updatePauseMenu(const InputState& input){
 auto pressed=[](bool now,bool previous){return now&&!previous;};
 const bool match=m_matchView.active;const int rows=PauseMenuLayout::rows(match);
 m_menuSelection=std::clamp(m_menuSelection,0,rows-1);const int before=m_menuSelection;
 if(pressed(input.menuUp,m_menuPrevious.menuUp))m_menuSelection=(m_menuSelection+rows-1)%rows;
 if(pressed(input.menuDown,m_menuPrevious.menuDown))m_menuSelection=(m_menuSelection+1)%rows;
 const bool across=input.pointerX>=PauseMenuLayout::X-14&&input.pointerX<PauseMenuLayout::X+PauseMenuLayout::Width;
 const int under=across?PauseMenuLayout::rowAt(input.pointerY,match):-1;
 const bool click=pressed(input.fire,m_menuPrevious.fire);
 if(under>=0&&(input.pointerX!=m_pointerX||input.pointerY!=m_pointerY||click))m_menuSelection=under;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 if(m_menuSelection!=before){m_menuArmed=-1;m_menuMessage.clear();}
 const bool activate=pressed(input.menuAccept,m_menuPrevious.menuAccept)||(under>=0&&click);
 m_menuPrevious=input;
 if(!activate)return;
 const int row=m_menuSelection;
 if(row==PauseResume){m_paused=false;m_suppressFire=true;m_menuArmed=-1;return;}
 if(row==PauseSettings){openSettings(m_vrInputActive?SettingsTab::Vr:SettingsTab::Audio);return;}
 if(match){
  if(row==PauseMatchCode){if(!m_matchView.joinCode.empty()&&copyTextToClipboard(m_matchView.joinCode))m_menuMessage="COPIED TO THE CLIPBOARD";return;}
  if(row==PauseMatchLeave){leaveMatch();m_titlePrevious=input;return;}
  return;
 }
 if(row==PauseSave||row==PauseLoad){m_menuPage=row==PauseSave?MenuPage::Save:MenuPage::Load;m_menuSelection=0;m_dragSlider=-1;m_menuMessage.clear();m_menuArmed=-1;refreshSaveSlots();return;}
 if(row==PauseRestart||row==PauseQuit){
  if(m_menuArmed!=row){m_menuArmed=row;return;}
  m_menuArmed=-1;
  if(row==PauseRestart){restart();m_paused=false;m_menuPage=MenuPage::Pause;m_menuSelection=0;m_suppressFire=true;}
  // Returning to the title hands it the input that chose it: a key or click still held must not land on a title row.
  else {showTitleScreen();m_titlePrevious=input;}
 }
}

// ----- Chat -----
bool Game::sendChat(const std::string& text){return m_match&&m_settings.textChat&&m_match->sendChat(text);}
void Game::updateChat(const InputState& input){
 if(!m_matchView.active||!m_settings.textChat||m_vrInputActive){m_chatOpen=false;m_chatTyped.clear();return;}
 if(input.escape&&!m_previousEscape){m_chatOpen=false;m_chatTyped.clear();return;}
 for(char c:input.textInput){
  if(c=='\r'||c=='\n'){if(!m_chatTyped.empty())sendChat(m_chatTyped);m_chatTyped.clear();m_chatOpen=false;return;}
  if(c=='\b'){if(!m_chatTyped.empty())m_chatTyped.pop_back();}
  else if(c>=32&&c<127&&m_chatTyped.size()<net::Match::ChatMax)m_chatTyped+=c;
 }
}
std::string Game::vrChatCaption()const{
 if(!m_matchView.active||m_matchView.chat.empty())return {};
 const auto& c=m_matchView.chat.back();if(c.age>6)return {};
 return c.notice?c.text:c.from+": "+c.text;
}
void Game::setPlayerMuted(const std::string& name,bool muted){
 if(name.empty())return;auto& list=m_settings.mutedPlayers;
 const auto it=std::find(list.begin(),list.end(),name);
 if(muted&&it==list.end())list.push_back(name);else if(!muted&&it!=list.end())list.erase(it);
 m_settingsSaveDue=true;
}
// Tab held: the arrows pick a row of the scoreboard, Enter mutes or unmutes that player.
void Game::updateScoreboardPick(const InputState& input){
 auto pressed=[&](bool now,bool previous){return now&&!previous;};
 const auto& rows=m_matchView.scores;const int n=int(rows.size());
 if(n==0){m_scoreboardPick=-1;return;}
 const int step=int(pressed(input.menuDown,m_boardPrevious.menuDown))-int(pressed(input.menuUp,m_boardPrevious.menuUp));
 if(step){if(m_scoreboardPick<0)m_scoreboardPick=step>0?0:n-1;else m_scoreboardPick=((m_scoreboardPick+step)%n+n)%n;}
 m_scoreboardPick=std::min(m_scoreboardPick,n-1);
 if(m_scoreboardPick>=0&&pressed(input.menuAccept,m_boardPrevious.menuAccept)){const auto& r=rows[size_t(m_scoreboardPick)];if(!r.self)setPlayerMuted(r.name,!m_settings.muted(r.name));}
}

// ----- Voice -----
float Game::micMeter()const{
 if(m_micTest&&m_micMonitor&&m_micMonitor->open())return m_micMonitor->level();
 if(m_netSession&&m_netSession->voiceStatus().inRoom)return m_netSession->micLevel();
 return -1;
}
void Game::updateVoice(const InputState& input){
 if(m_micTest&&!(m_paused&&m_menuPage==MenuPage::Options&&m_settingsTab==SettingsTab::Communication)){m_micTest=false;m_micMonitor.reset();}
 if(m_micTest){if(!m_micMonitor||m_micMonitor->deviceName()!=m_settings.micDevice)m_micMonitor=std::make_shared<MicMonitor>(m_settings.micDevice);m_micMonitor->poll();}
 if(!m_netSession)return;
 net::VoiceSettings v;v.mode=net::VoiceMode(std::clamp(m_settings.voiceMode,0,2));v.openMicThreshold=m_settings.openMicThreshold;
 v.inputDevice=m_settings.micDevice;v.outputDevice=m_settings.speakerDevice;v.inputVolume=m_settings.micVolume;v.outputVolume=m_settings.voiceVolume;
 if(!m_voicePushed||!(v==m_voiceApplied)){m_netSession->setVoice(v);m_voiceApplied=v;m_voicePushed=true;}
 m_netSession->setTalking(input.talk&&!m_chatOpen&&v.mode==net::VoiceMode::PushToTalk);
}

// ----- The menus' own test (engine.settings runs it with testSettings) -----
bool Game::testSettingsMenu(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;return pass;};
 auto key=[](Game& g,bool InputState::* k){InputState i{};i.*k=true;g.update(i,.02f);g.update({},.02f);};
 auto clickAt=[](Game& g,int x,int y){g.update({},.02f);InputState i{};i.pointerX=x;i.pointerY=y;i.fire=true;g.update(i,.02f);i.fire=false;g.update(i,.02f);};
 auto game=validationScene(Enemy::Kind::Huntsman);
 key(game,&InputState::escape);
 check(game.paused()&&game.menuPage()==MenuPage::Pause&&game.menuRows()==6,"Esc opens PAUSE with six rows");
 // The pointer finds every pause row where PauseMenuLayout puts it.
 {bool all=true;for(int r=0;r<6;++r){game.update({},.02f);InputState i{};i.pointerX=PauseMenuLayout::X+40;i.pointerY=PauseMenuLayout::rowY(r,false)+8;game.update(i,.02f);all&=game.menuSelection()==r;}
  check(all,"hovering each pause row selects it");}
 clickAt(game,PauseMenuLayout::X+40,PauseMenuLayout::rowY(PauseSettings,false)+8);
 check(game.menuPage()==MenuPage::Options&&game.settingsTab()==SettingsTab::Audio&&game.settingsOnTabs(),"SETTINGS opens on AUDIO with the page column chosen");
 // Down the column: each page opens as it is reached, BACK last.
 {bool pages=true;for(int t=1;t<TabCount;++t){key(game,&InputState::menuDown);pages&=int(game.settingsTab())==t;}
  key(game,&InputState::menuDown);check(pages&&game.menuSelection()==TabCount&&game.settingsTab()==SettingsTab::Vr,"down the column turns every page, then BACK");}
 clickAt(game,SettingsLayout::TabX+20,SettingsLayout::tabY(int(SettingsTab::Audio))+6);
 key(game,&InputState::menuRight);check(!game.settingsOnTabs()&&game.menuSelection()==0,"right goes into the rows");
 key(game,&InputState::menuLeft);check(std::fabs(game.settings().master-.95f)<.001f,"left lowers MASTER VOLUME a step");
 // Drag the music slider to its left end.
 {game.update({},.02f);InputState i{};i.fire=true;i.pointerX=SettingsLayout::SliderX+30;i.pointerY=SettingsLayout::rowY(1)+8;game.update(i,.02f);i.pointerX=SettingsLayout::SliderX-40;i.pointerY+=40;game.update(i,.02f);
  check(game.settings().music==0&&game.menuSelection()==1,"the music slider drags to 0 and holds its row while the pointer strays");i.fire=false;game.update(i,.02f);}
 // RESET TO DEFAULTS asks twice.
 {const auto rows=game.settingsRows();const int reset=int(rows.size())-1;game.m_menuSelection=reset;key(game,&InputState::menuAccept);
  check(game.menuArmed()==reset&&game.settings().music==0,"RESET TO DEFAULTS waits for a second press");key(game,&InputState::menuAccept);
  check(game.settings().music==.75f&&game.settings().master==1&&game.menuArmed()<0,"the second press puts AUDIO back as it came");}
 game.m_settings.music=.4f;
 // Every page draws its rows where SettingsLayout says and each row has its own plain line.
 {bool lines=true;for(int t=0;t<TabCount;++t){game.m_settingsTab=SettingsTab(t);for(const auto& m:game.settingsItems())lines&=!m.help.empty()&&!m.label.empty();}
  check(lines,"every tab and row on every page has a label and a line saying what it does");game.m_settingsTab=SettingsTab::Audio;}
 // Esc: rows -> column -> PAUSE, and leaving asks for a save.
 game.m_settingsOnTabs=false;key(game,&InputState::escape);check(game.menuPage()==MenuPage::Options&&game.settingsOnTabs(),"Esc in the rows returns to the column");
 key(game,&InputState::escape);check(game.menuPage()==MenuPage::Pause&&game.menuSelection()==PauseSettings&&game.takeSettingsSave(),"Esc in the column goes back to PAUSE on SETTINGS and asks main to save");
 // COMMUNICATION: voice mode cycles, the push-to-talk key, the muted list.
 game.openSettings(SettingsTab::Communication);key(game,&InputState::menuRight);
 {auto rows=game.settingsRows();auto at=[&](int id){return int(std::find(rows.begin(),rows.end(),id)-rows.begin());};
  game.m_menuSelection=at(RVoiceMode);key(game,&InputState::menuRight);check(game.settings().voiceMode==2,"VOICE CHAT: push to talk -> open mic");
  key(game,&InputState::menuRight);check(game.settings().voiceMode==0,"-> off");key(game,&InputState::menuRight);
  game.m_menuSelection=at(RTalkKey);key(game,&InputState::menuRight);check(game.settings().pushToTalkKey==0x42,"PUSH-TO-TALK KEY: V -> B");
  game.m_settings.mutedPlayers={"Gus"};game.m_menuSelection=at(RMuted);key(game,&InputState::menuAccept);
  check(game.settingsMutedList()&&game.settingsRows().size()==2,"MUTED PLAYERS opens the list (BACK and Gus)");
  game.m_menuSelection=1;key(game,&InputState::menuAccept);check(game.settings().mutedPlayers.empty(),"Enter on Gus unmutes him");
  key(game,&InputState::escape);check(!game.settingsMutedList()&&game.settingsRows()[size_t(game.menuSelection())]==RMuted,"Esc closes the list back onto MUTED PLAYERS");}
 // VR page: the comfort settings live here now.
 game.openSettings(SettingsTab::Vr);key(game,&InputState::menuRight);
 {auto rows=game.settingsRows();auto at=[&](int id){return int(std::find(rows.begin(),rows.end(),id)-rows.begin());};
  game.m_menuSelection=at(RVrTurn);key(game,&InputState::menuRight);check(std::fabs(game.settings().vrTurnSpeed-1.125f)<.001f,"VR: turn speed steps up");
  game.m_menuSelection=at(RVrHeight);key(game,&InputState::menuAccept);check(game.takeVrRecalibrate(),"VR: RECALIBRATE HEIGHT is taken");
  bool snap=false;for(const auto& m:game.settingsItems()){std::string l=m.label;for(auto& c:l)c=char(std::toupper(static_cast<unsigned char>(c)));snap|=l.find("SNAP")!=std::string::npos;}
  check(!snap,"VR: no snap turn anywhere");}
 // BACK in the column; then PAUSE's RESTART and QUIT ask twice.
 game.m_settingsOnTabs=true;game.m_menuSelection=TabCount;key(game,&InputState::menuAccept);check(game.menuPage()==MenuPage::Pause,"BACK returns to PAUSE");
 game.m_menuSelection=PauseQuit;key(game,&InputState::menuAccept);check(game.paused()&&!game.titleScreen()&&game.menuArmed()==PauseQuit,"QUIT TO TITLE waits for a second press");
 key(game,&InputState::menuUp);check(game.menuArmed()<0,"moving away disarms it");
 // A match's pause: four rows; save and load are not there. (One scene throughout: building a Game is slow.)
 key(game,&InputState::escape);game.m_matchView.active=true;game.m_matchView.joinCode="K7QM-2X";key(game,&InputState::escape);
 check(game.paused()&&game.menuRows()==4,"in a match PAUSE has RESUME, SETTINGS, JOIN CODE and LEAVE MATCH");
 game.m_matchView={};game.m_paused=false;
 // Settings from the title: Esc in the column returns to the title.
 game.showTitleScreen();game.m_titleSelection=TitleSettings;key(game,&InputState::menuAccept);
 check(!game.titleScreen()&&game.paused()&&game.menuPage()==MenuPage::Options,"the title's SETTINGS opens SETTINGS");
 key(game,&InputState::escape);check(game.titleScreen(),"Esc from its column returns to the title");
 // The settings round trip: everything new survives; an older file keeps the new defaults.
 {auto path=std::filesystem::temp_directory_path()/"rawmetal-settings-comms.txt";auto& s=game.m_settings;
  s.textChat=false;s.chatFilter=false;s.voiceMode=2;s.pushToTalkKey=0x14;s.openMicThreshold=.4f;s.micVolume=1.3f;s.voiceVolume=.6f;s.proximityVoice=false;
  s.micDevice="Headset Microphone (Arctis 7)";s.speakerDevice="Speakers (Realtek(R) Audio)";s.mutedPlayers={"Gus","Name With Spaces"};game.m_renderScale=.75f;game.m_showFps=true;
  const Settings saved=s;game.saveSettings(path.wstring());s=Settings{};game.m_renderScale=1;game.m_showFps=false;game.loadSettings(path.wstring());const auto& t=game.m_settings;
  check(!t.textChat&&!t.chatFilter&&t.voiceMode==2&&t.pushToTalkKey==0x14&&std::fabs(t.openMicThreshold-.4f)<.001f&&std::fabs(t.micVolume-1.3f)<.001f&&std::fabs(t.voiceVolume-.6f)<.001f&&!t.proximityVoice
   &&t.micDevice==saved.micDevice&&t.speakerDevice==saved.speakerDevice&&t.mutedPlayers==saved.mutedPlayers&&game.m_renderScale==.75f&&game.m_showFps,"the communication settings, render scale and frame readout survive a save and load");
  std::ofstream(path)<<"0.8 0.5 1 1.2 0 1 1 1 1 0.5 0 0 0 0 1\n";game.loadSettings(path.wstring());std::error_code e;std::filesystem::remove(path,e);const Settings d;const auto& o=game.m_settings;
  check(o.character==1&&std::fabs(o.master-.8f)<.001f&&o.textChat==d.textChat&&o.voiceMode==d.voiceMode&&o.pushToTalkKey==d.pushToTalkKey
   &&o.mutedPlayers.empty()&&o.proximityVoice&&game.m_renderScale==1,"a settings file from before chat and voice loads, with their defaults");}
 return ok;
}
}
