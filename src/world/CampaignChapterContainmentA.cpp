// CONTAINMENT (chapter 2, manifest 61-65): chunks 94, 95, 96, 97, 98: the real rooms,
// route, heights, doors and light at final scale, the story staged coarsely with library models.
// Polish (dressing density, pipework, writing passes, voice) comes later.
// See docs/Design/descent-campaign-20261025.md (Containment) for the brief.
// Route: the Morgue's door into the Quarantine Ward (94) -> S to the Containment Intake hub (95),
// whose south door to Containment B is held until containment power (cont_power, from the
// Conversion Lab through the Observation Ring, the hub's west gallery). E from the hub: Containment
// A (96), intact, and behind its walls the Service corridors (97), whose bulkhead also waits on the
// feed; through the slots the player sees what is in the walls. With power: B (98), breached, the
// way on W (Breach Sector) and S (Specimen Handling); the service loop back E.
// The Cleanup Force is losing here: the hub detail holds a truce at its barricade, A's two men still
// have their orders, B's detail is what is left of Kessler's team, and the Investigator writes it off.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "Primitives.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- Containment kit (ca) ---------------------------------------------------------------------
// Fixture and sign facing: 0 faces +y (south), kPi north, caE east, caW west.
constexpr float caS=0,caN=kPi,caE=kPi*.5f,caW=-kPi*.5f;
// Light yaw (a wall lamp's way into the room): 0 east, kPi*.5 south, kPi west, -kPi*.5 north.
constexpr float clE=0,clS=kPi*.5f,clW=kPi,clN=-kPi*.5f;
// The cave pack's standing stones bake their texture into tiny islands and render as flat dark lumps;
// in these levels every standing stone is one of the modelled outcrops of the bedrock instead, at the
// size it was placed at (the hanging stalactites and ceiling rock keep the pack's own).
int caBedrock(int mdl){
 if(mdl==model::cave_boulder_round||mdl==model::cave_boulder_a||mdl==model::cave_rock_block)return model::res_rock_crag;
 if(mdl==model::cave_rock_slab)return model::res_rock_low;
 if(mdl==model::cave_rock_column||mdl==model::cave_stalagmite)return model::res_rock_spire;
 return mdl;}
void caFix(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];m.fixtures.push_back({caBedrock(model),{x,y},z-m.layers.front().elevation,v[0]*s,v[1]*s,v[2]*s,yaw,solid});
}
// A ceiling tube that works, under its recessed fitting (recessedFittings at the end).
WorldLight caTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint=0,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
// A fitting with its tubes dead.
void caDeadTube(Map& m,float x,float y,float roof){m.fixtures.push_back({model::clinic_ceiling_lamp_2,{x,y},roof-.17f-m.layers.front().elevation,.44f,1.04f,.17f,0,false});}
// Containment lockdown: a red wall lamp blinking while `state` is unset (the feed is down).
WorldLight caBeacon(Vec2 p,float z,float yaw,float phase,const char* state){
 auto l=lamp(p,z,.36f,4.6f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.12f,.06f});
 l.period=1.4f;l.onTime=.7f;l.phase=phase;l.enabledState=stateId(state);l.invert=true;return l;}
// A breached sector's alarm: red, slow, never cleared.
WorldLight caAlarm(Vec2 p,float z,float yaw,float phase){
 auto l=lamp(p,z,.42f,6.f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.1f,.05f});l.period=2.2f;l.onTime=1.3f;l.phase=phase;return l;}
// A bare-bulb glow: monitor blue, resonance violet, a work lamp.
// Resonance light (the violet gel) is keyed cold: the growth sits dark magenta under a blue-white
// key and the crystal's own cyan is the one bright accent, rather than the whole space lit violet.
WorldLight caGlow(Vec2 p,float z,float intensity,float range,float kelvin,Point3 gel={1,1,1},float flicker=0){
 if(gel.x>.55f&&gel.y<.56f&&gel.z>.9f){gel={.5f,.72f,1.f};kelvin=std::max(kelvin,8000.f);intensity*=.8f;}
 auto l=lamp(p,z,intensity,range,kelvin,0,WorldLightMount::Bare,0,false,gel);l.flicker=flicker;return l;}
// An industrial pendant: rod from the roof to the reflector head at z.
void caPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});m.fixtures.push_back({model::pendant_lamp,p,z-m.layers.front().elevation,.5f,.5f,.34f,0,false});
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,0,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}
// Somebody left where they fell.
void caBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
// An armed Cleanup man on an authored beat (the Medical chapter's guard, the same way).
void caGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
void caSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1,float y1,float x2,float y2){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
// The hum of a live field: AnomalyHum every 10 s from the box once the player first stands in it.
void caHum(Map& m,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top,float gain=.55f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;s.bottom=bottom;s.top=top;s.durationMs=3600000;s.finishAtMs=3600000;
 s.soundIntervalMs=10000;s.soundUntilMs=3000000;s.tickSound=Sound::AnomalyHum;s.gain=gain;s.pitch=1;m.timedSequences.push_back(s);
}
ScriptAction caSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
void caEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions,bool once=true){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=once;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
DialogueReply caReply(const char* text,const char* next="",const char* sets="",int stance=-1){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;if(*sets)r.sets.push_back({stateId(sets),1});r.stance=stance;return r;
}
// Wired glass in a straight wall: dado, sill, panes (holding or not), mullions, head, wall above;
// an invisible proxy from sill to head so nobody perches on the sill.
void caGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float roof,float sill=.95f,float head=2.4f,bool holds=true,int mat=3){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)kit::glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill,f+head,17,holds);else kit::glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.03f,alongX?x1:y1,(alongX?x2:y2)-.06f);
  if(alongX)wall(m,t,y1,t+.06f,y2,f+sill,f+head,20);else wall(m,x1,t,x2,t+.06f,f+sill,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.08f,20);if(roof>f+head+.08f)wall(m,x1,y1,x2,y2,f+head+.08f,roof,mat);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill,f+head,CollisionProxyMaterial);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill,f+head,CollisionProxyMaterial);
}
// A blown cell front: dado, mullions and head with the glass gone, the shards on the floor (side +1/-1:
// the side the glass fell to).
void caBlownFront(Map& m,float x1,float y1,float x2,float y2,float f,float roof,int side,unsigned seed){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+.9f,15);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){if(i%2&&i!=bays&&(seed+i)%3==0)continue;float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.03f,alongX?x1:y1,(alongX?x2:y2)-.06f);
  if(alongX)wall(m,t,y1,t+.06f,y2,f+.9f,f+2.6f,20);else wall(m,x1,t,x2,t+.06f,f+.9f,f+2.6f,20);}
 wall(m,x1,y1,x2,y2,f+2.6f,f+2.68f,20);if(roof>f+2.68f)wall(m,x1,y1,x2,y2,f+2.68f,roof,15);
 unsigned h=seed*2654435761u+3;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
 for(int i=0;i<22;++i){float a=next()*len,o=.15f+next()*1.1f,w=.02f+next()*.05f;
  float px=alongX?x1+a:(side>0?x2+o:x1-o),py=alongX?(side>0?y2+o:y1-o):y1+a;wall(m,px-w,py-.02f,px+w,py+.02f,f+.002f,f+.006f,17);}
}
// Stencilled characters (0-9, A) painted on a wall face along x at y=face, facing +y (dir 1) or -y
// (dir -1): seven-stroke figures h high, the string centred on x, its foot at z.
void caStencil(Map& m,const char* text,float x,float face,int dir,float z,float h,int mat=14){
 const float w=h*.55f,t=h*.14f,gap=h*.24f,y1=dir>0?face:face-.014f,y2=dir>0?face+.014f:face;
 const size_t n=std::char_traits<char>::length(text);const float half=(n*w+(n-1)*gap)*.5f;float cx=0;
 // Strokes are laid out in reading order; read from the -y side the text runs toward -x.
 auto stroke=[&](float a,float b,float lo,float hi){const float xa=x+dir*(a-half),xb=x+dir*(b-half);wall(m,std::min(xa,xb),y1,std::max(xa,xb),y2,lo,hi,mat);};
 for(size_t i=0;i<n;++i,cx+=w+gap){const char c=text[i];
  const int k=c=='A'?0x77:c=='0'?0x3f:c=='1'?0x06:c=='2'?0x5b:c=='3'?0x4f:c=='4'?0x66:c=='5'?0x6d:c=='6'?0x7d:c=='7'?0x07:c=='8'?0x7f:c=='9'?0x6f:0;
  if(k&1)stroke(cx,cx+w,z+h-t,z+h);if(k&8)stroke(cx,cx+w,z,z+t);if(k&64)stroke(cx,cx+w,z+(h-t)*.5f,z+(h+t)*.5f);
  if(k&2)stroke(cx+w-t,cx+w,z+h*.5f,z+h);if(k&4)stroke(cx+w-t,cx+w,z,z+h*.5f);
  if(k&32)stroke(cx,cx+t,z+h*.5f,z+h);if(k&16)stroke(cx,cx+t,z,z+h*.5f);}
}
// A drag trail across a floor: smears from point to point, every ~0.7 m, alternately narrow and broad.
void caTrail(Map& m,std::initializer_list<Vec2> points,float f,unsigned seed){
 const Vec2* prev=nullptr;unsigned h=seed*2654435761u+7;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
 for(const auto& p:points){if(prev){Vec2 d=p-*prev;float len=std::sqrt(d.x*d.x+d.y*d.y);int n=std::max(1,int(len/.7f));
  for(int i=0;i<n;++i){Vec2 c=*prev+d*((i+.5f)/n);float r=.12f+next()*.2f,s=.1f+next()*.12f;
   if(std::fabs(d.x)>std::fabs(d.y))wall(m,c.x-r,c.y-s,c.x+r,c.y+s,f+.004f,f+.008f,16);else wall(m,c.x-s,c.y-r,c.x+s,c.y+r,f+.004f,f+.008f,16);}}
  prev=&p;}
}
void caPrim(Map& m,const char* kind,float x,float y,float z,float w,float d,float h,int facing,int mat,bool solid=true){expandPrimitive({kind,x,y,z,w,d,h,facing,mat,solid},m.structures);}
void caModel(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid=false){m.fixtures.push_back({caBedrock(model),{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});}
unsigned caSeed(float x,float y){unsigned h=unsigned(x*97.f)*2654435761u^unsigned(y*131.f)*40503u;h^=h>>13;h*=1274126177u;return h^(h>>16);}
// Resonance growth and crystal. Rock first: what came up through the floors and the walls is the
// cave pack's black rock, and the growth lives in it, its magenta only in the seams between the
// rocks, in the cracks it opened in the concrete and where it is wet. Nothing of the growth's finish
// is a shape stood flat on a wall or a dome stretched over a heap: it is in narrow boxes, struts and
// columns, all drawn at the finish's own repeat. The crystal is clusters of narrow hexagonal prisms,
// glass over a lit core with the bright points of the self-lit finish, leaning out of one root half
// buried in rock and rubble, the cave pack's bed of spikes at their feet.
void caSeam(Map& m,float x1,float y1,float x2,float y2,float lo,float hi,int mat=34){wall(m,x1,y1,x2,y2,lo,hi,mat);m.structures.back().solid=false;}
// A floor (the chunk's or a deck's) at height z, under (x,y).
bool caOnFloor(const Map& m,float x,float y,float z){
 const int tx=std::clamp(int(std::floor(x)),0,23),ty=std::clamp(int(std::floor(y)),0,23);
 for(const auto& l:m.layers){if(std::fabs(l.elevation-z)>.3f)continue;if(&l==&m.layers.front()||l.rows[size_t(ty)][size_t(tx)]=='=')return true;}
 return false;
}
// Which side of a strip laid along a wall the wall stands: -1 at its low edge (y1, or x1), +1 at its high edge.
int caWallSide(const Map& m,float x1,float y1,float x2,float y2,float z){
 const bool alongX=x2-x1>=y2-y1;
 auto blocked=[&](float px,float py){const int tx=int(std::floor(px)),ty=int(std::floor(py));
  if(tx<0||ty<0||tx>23||ty>23||m.layers.front().rows[size_t(ty)][size_t(tx)]=='#')return true;
  for(const auto& s:m.structures)if(s.solid&&px>s.x1&&px<s.x2&&py>s.y1&&py<s.y2&&z>s.bottom&&z<s.top)return true;return false;};
 int low=0,high=0;
 for(float k:{.2f,.5f,.8f}){if(alongX){const float px=x1+(x2-x1)*k;low+=blocked(px,y1-.07f);high+=blocked(px,y2+.07f);}
  else{const float py=y1+(y2-y1)*k;low+=blocked(x1-.07f,py);high+=blocked(x2+.07f,py);}}
 return high>low?1:-1;
}
// The pack's rock fitted to a w by d by h box standing at (x,y) from z: of `models`, the one whose own
// shape is nearest the box (turned a quarter where that fits better), scaled alike on every axis to
// within a few percent, so no rock is ever drawn stretched, and never standing taller than the box.
void caFit(Map& m,std::initializer_list<int> models,float x,float y,float z,float w,float d,float h,float yaw){
 int best=*models.begin();float bestSpread=1e9f;bool turn=false;
 for(int mdl:models)for(int q=0;q<2;++q){const auto& v=model::size[mdl];const float r0=(q?d:w)/v[0],r1=(q?w:d)/v[1],r2=h/v[2];
  const float spread=std::max({r0,r1,r2})/std::min({r0,r1,r2});if(spread<bestSpread-.02f){bestSpread=spread;best=mdl;turn=q==1;}}
 // never taller than the box: a rock too tall for its footprint is drawn narrower, not higher
 const auto& v=model::size[best];const float r0=(turn?d:w)/v[0],r1=(turn?w:d)/v[1],r2=h/v[2],s=std::min(std::cbrt(r0*r1*r2),r2/.93f);
 auto k=[&](float a){return std::clamp(a/s,.93f,1.075f)*s;};
 caModel(m,best,x,y,z,v[0]*k(r0),v[1]*k(r1),v[2]*k(r2),yaw+(turn?kPi*.5f:0.f));
}
// One prism from its foot at (x,y,z): h long, r across, leaning (lx,ly) metres per metre of rise.
// Upright, it is one hexagonal shaft of glass round a thread of the self-lit finish (lit: a thick
// core of it); leaning, a slimmer shaft of the self-lit finish laid in short courses, each set over by
// the lean (short shards only: a long one would show its courses). Its point steps in, broad and
// bright where it is bright.
void caPrism(Map& m,float x,float y,float z,float lx,float ly,float h,float r,bool lit=false,bool bright=false){
 const float lean=std::sqrt(lx*lx+ly*ly),tip=std::min(h*.3f,r*1.7f),body=h-tip;
 auto ring=[&](float lo,float hi,float rr,int mat){const float d=(lo+hi)*.5f-z,cx=x+lx*d,cy=y+ly*d;wall(m,cx-rr,cy-rr,cx+rr,cy+rr,lo,hi,mat);auto& s=m.structures.back();s.shape=ShapeColumn;s.solid=false;};
 if(lean<.1f){lx=ly=0;ring(z,z+body,r,17);ring(z,z+body,r*(lit?.55f:.4f),35);}
 else{const int n=std::clamp(int(body*lean/(r*.35f))+1,1,4);const float seg=body/float(n);for(int i=0;i<n;++i)ring(z+seg*float(i),z+seg*float(i+1),r*.55f,35);}
 ring(z+body-.002f,z+h,r*(lit||bright?.55f:.38f),35);
}
// A crag of the rock too big for any of the pack's stones (their black finish only reads where a
// lamp is close): the rock's own finish in faces leaning back from a ridge and from both its ends,
// so it takes the light the way the heaved bedrock of the Breach Sector does.
void caCrag(Map& m,float x,float y,float z,float w,float d,float h,unsigned seed,bool solid=false){
 // a modelled outcrop of the bedrock (res_rock_*): broad, tall or low by the crag's proportions, laid
 // along its long side and turned end for end by its seed, sunk a little so it grows out of the floor
 const bool alongX=w>=d;const float lng=alongX?w:d,sht=alongX?d:w;
 const int mdl=h>lng*1.25f?model::res_rock_spire:h<lng*.5f?model::res_rock_low:model::res_rock_crag;
 const float yaw=(alongX?0.f:kPi*.5f)+(((seed>>3)&1u)?kPi:0.f);
 caModel(m,mdl,x,y,z-h*.04f,lng,sht,h*1.04f,yaw,solid);
}
// A fang of the rock come up from below at scale s (1 is 3.7 m tall): a tall narrow crag in the rock's
// own finish, so it takes the light (the pack's stalagmite is too black to read but under a lamp).
void caFang(Map& m,float x,float y,float z,float s,float yaw){caCrag(m,x,y,z,1.5f*s,1.1f*s,3.7f*s,caSeed(x,y+yaw),true);}
// A crack the growth opened in a wall face along x (alongX) or y at `face`, from (t,z1) up to z2:
// a zig-zag of thin struts with the magenta in it, now and then a fork.
void caCrack(Map& m,bool alongX,float face,float t,float z1,float z2,unsigned seed){
 unsigned g=seed|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 auto leg=[&](float a,float z,float run,float rise,float w){
  if(std::fabs(run)<.03f){if(alongX)caSeam(m,a-w*.5f,face-w*.5f,a+w*.5f,face+w*.5f,z,z+rise);else caSeam(m,face-w*.5f,a-w*.5f,face+w*.5f,a+w*.5f,z,z+rise);return;}
  const float c=a+run*.5f;
  if(alongX)caPrim(m,"strut",c,face,z,w,std::fabs(run),rise,run>0?1:3,34,false);else caPrim(m,"strut",face,c,z,w,std::fabs(run),rise,run>0?0:2,34,false);};
 float a=t,z=z1,w=.035f+next()*.025f;
 while(z<z2-.1f){const float rise=std::min(.18f+next()*.4f,z2-z),run=(next()-.5f)*.36f;leg(a,z,run,rise,w);
  if(next()<.16f&&z+rise<z2-.4f){const float fr=(next()<.5f?-1.f:1.f)*(.12f+next()*.2f);leg(a+run,z+rise-.02f,fr,.2f+next()*.35f,w*.7f);}
  a+=run;z+=rise-.01f;w=std::max(.018f,w*.93f);}
}
// A crust on a surface. On a floor: a film of the rock with wet sheen on it, the growth in small wet
// patches and in the cracks across it, a stone or two. Against a wall: the rock heaped up the face
// in pieces half buried in it, the growth in the seams between them, and cracks with the growth in
// them running on up the concrete from their shoulders, wet staining down from the cracks.
void caCrust(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){
 unsigned h=caSeed(x1*3.f+x2,y1*5.f+y2+lo*.37f)|1u;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return float(h&0xffffu)/65535.f;};
 if(hi-lo<.3f){
  wall(m,x1,y1,x2,y2,lo,lo+.012f,26);
  const int nx=std::max(1,int((x2-x1)/.8f+.5f)),ny=std::max(1,int((y2-y1)/.8f+.5f));const float cw=(x2-x1)/nx,cd=(y2-y1)/ny;
  for(int i=0;i<nx;++i)for(int j=0;j<ny;++j){const float r=next(),cx=x1+(i+.5f)*cw,cy=y1+(j+.5f)*cd,sx=cw*(.18f+next()*.2f),sy=cd*(.18f+next()*.2f);
   if(r<.2f){caSeam(m,cx-sx,cy-sy*.5f,cx+sx,cy+sy*.5f,lo+.012f,lo+.02f);caSeam(m,cx-sx*.5f,cy-sy,cx+sx*.5f,cy+sy,lo+.012f,lo+.024f);}
   else if(r<.55f)caSeam(m,cx-sx*1.4f,cy-sy*1.4f,cx+sx*1.4f,cy+sy*1.4f,lo+.013f,lo+.016f,17);
   else if(r>.9f){const float s=.05f+next()*.06f;caModel(m,model::cave_boulder_round,cx,cy,lo-.06f,3.15f*s,2.17f*s,2.98f*s,next()*6.28f);}}
  // the cracks across it the growth came up through
  for(int k=0;k<1+int((x2-x1)*(y2-y1)>6.f);++k){float cx=x1+(x2-x1)*(.2f+next()*.6f),cy=y1+(y2-y1)*(.2f+next()*.6f);const bool ax=next()<.5f;
   for(int s=0;s<5;++s){const float run=.25f+next()*.45f,w=.03f+next()*.03f,jog=(next()-.5f)*.3f;
    if(ax){const float nx2=std::min(x2,cx+run);if(nx2-cx>.05f)caSeam(m,cx,cy-w*.5f,nx2,cy+w*.5f,lo+.012f,lo+.02f);cx=nx2;cy=std::clamp(cy+jog,y1+.05f,y2-.05f);}
    else{const float ny2=std::min(y2,cy+run);if(ny2-cy>.05f)caSeam(m,cx-w*.5f,cy,cx+w*.5f,ny2,lo+.012f,lo+.02f);cy=ny2;cx=std::clamp(cx+jog,x1+.05f,x2-.05f);}}}
  return;}
 const bool alongX=x2-x1>=y2-y1;const float len=alongX?x2-x1:y2-y1,span=hi-lo;
 const int side=caWallSide(m,x1,y1,x2,y2,lo+1.f);
 const float face=alongX?(side<0?y1:y2):(side<0?x1:x2),out=-float(side);   // out: the way into the room
 auto at=[&](float t,float o){return alongX?Vec2{x1+t,face+o*out}:Vec2{face+o*out,y1+t};};
 auto across=[&](float t0,float t1,float o0,float o1,float z1,float z2,int mat=34){const Vec2 a=at(t0,o0),b=at(t1,o1);caSeam(m,std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y),z1,z2,mat);};
 const float yawBase=alongX?0.f:kPi*.5f;
 if(len<.9f){
  // a narrow face: one buttress of the rock at its foot, the growth run down it in two veins
  const float ph=std::min(span,.8f+next()*.6f);const Vec2 c=at(len*.5f,.12f);
  caFit(m,{model::cave_rock_column,model::cave_stalagmite},c.x,c.y,lo-.04f,len*1.1f,.5f,ph,yawBase+(next()-.5f)*.4f);
  for(int k=0;k<2;++k){const float t=len*(.25f+.5f*float(k))+(next()-.5f)*.1f,w=.03f+next()*.03f;across(t-w*.5f,t+w*.5f,-.01f,.012f,lo+ph*.5f,lo+std::min(span,ph+.4f+next()*span*.6f));}
  return;}
 float prevTop=0,prevEnd=-1;
 for(float a=0;a<len-.15f;){
  const float pw=std::min(.75f+next()*.95f,len-a+.25f),ph=std::min(span,.45f+next()*std::min(1.8f,span*.45f)),pd=.55f+next()*.55f,t=a+pw*.5f;
  const Vec2 c=at(t,pd*.12f);const float yaw=yawBase+(next()-.5f)*.5f+(next()<.5f?kPi:0.f);
  if(ph>1.1f){const int fc=alongX?(out>0?0:2):(out>0?1:3);const Vec2 b=at(t,pd*.32f);(void)fc;caCrag(m,b.x,b.y,lo,alongX?pw:pd*.64f,alongX?pd*.64f:pw,ph,h^unsigned(t*977.f),true);
   const Vec2 f2=at(t+(next()-.5f)*pw*.4f,pd*.7f);caFit(m,{model::cave_boulder_a,model::cave_rock_slab},f2.x,f2.y,lo-.04f,.7f,.55f,.42f,yaw);}
  else if(next()<.5f)caFit(m,{model::cave_boulder_round,model::cave_rock_column,model::cave_boulder_a,model::cave_rock_slab},c.x,c.y,lo-.05f,pw,pd,ph,yaw);
  else caFit(m,{model::cave_boulder_a,model::cave_rock_block,model::cave_rock_slab,model::cave_stalagmite},c.x,c.y,lo-.05f,pw,pd,ph,yaw);
  // the seam where it meets the one before: the growth in the crevice between them
  if(prevEnd>0){const float s=(prevEnd+a)*.5f,st=.07f+next()*.08f;across(s-st*.5f,s+st*.5f,0,pd*.22f,lo,lo+std::min(ph,prevTop)*.55f);}
  // a crack on up the concrete from its shoulder, and the wet coming down out of it
  if(span>ph+.5f&&next()<.6f){const float ct=t+(next()-.5f)*pw*.5f,z2=std::min(hi-.05f,lo+ph+.5f+next()*std::min(3.4f,span-ph));
   caCrack(m,alongX,face+out*.004f,alongX?x1+ct:y1+ct,lo+ph*.75f,z2,h^unsigned(ct*977.f));
   if(next()<.5f){const float w=.05f+next()*.07f;across(ct-w*.5f,ct+w*.5f,.002f,.006f,lo+ph*.6f,lo+ph+.4f+next()*.9f,8);}}
  prevTop=ph;prevEnd=a+pw;a+=pw*.78f;}
}
// A patch of crust on a floor: the rock's film, cracked, wet, a hand high at most.
void caLens(Map& m,float x,float y,float f,float w,float d,float h=.09f){(void)h;caCrust(m,x-w*.45f,y-d*.45f,x+w*.45f,y+d*.45f,f,f+.04f);}
// A modelled crystal cluster (tools/library/build_resonance_crystals.py) whose prisms stand from z,
// at scale s, its rock root sunk below z so only the root's crown shows. res_crystal_l is 2.9 m tall
// at 1, res_crystal_m 1.6, res_crystal_s .7; the prisms burn from their bright tips (a *_glow finish).
void caCrystal(Map& m,int mdl,float x,float y,float z,float s,float yaw){
 const auto& v=model::size[mdl];const float sink=mdl==model::res_crystal_l?.52f:mdl==model::res_crystal_m?.34f:.17f;
 caModel(m,mdl,x,y,z-sink*s,v[0]*s,v[1]*s,v[2]*s,yaw);
}
// The cluster that stands about h high from z: the big knot, the middle one or the scatter.
void caCrystalTo(Map& m,float x,float y,float z,float h,float yaw){
 const int mdl=h>=1.7f?model::res_crystal_l:h>=.85f?model::res_crystal_m:model::res_crystal_s;
 const float tall=mdl==model::res_crystal_l?2.36f:mdl==model::res_crystal_m?1.22f:.52f;   // the leader's height at scale 1
 caCrystal(m,mdl,x,y,z,h/tall,yaw);
}
// A crystal from its foot at (x,y,z) about h high: a modelled cluster, turned (lean 0 +y, 1 +x, 2 -y,
// 3 -x) the way it was once leant. (w and reach are kept for the callers.)
void caShard(Map& m,float x,float y,float z,float w,float h,int lean=-1,float reach=0){
 (void)w;(void)reach;constexpr float yaws[4]={kPi*.5f,0.f,-kPi*.5f,kPi};
 caCrystalTo(m,x,y,z,h,lean<0?caSeed(x,y)%628u*.01f:yaws[lean]);
}
// The cave pack's bed of crystal spikes at scale s.
void caBed(Map& m,float x,float y,float z,float s,float yaw){caModel(m,model::cave_crystal_bed_blue,x,y,z,1.22f*s,1.22f*s,.245f*s,yaw);}
// A body of growth w by d by h: the rock it came up on, in pieces of the pack's rock kept near their
// own proportions, the growth in the seams between them and wet at their foot. (back: kept for the
// callers that once used it to turn the growth from the player; the rock now faces every way.)
void caGrowth(Map& m,float x,float y,float z,float w,float d,float h,bool solid=false,float back=0){
 (void)back;unsigned g=caSeed(x+z,y)|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 const bool alongX=w>=d;const float lng=alongX?w:d,sht=alongX?d:w;
 const int pieces=std::clamp(int(lng/(sht*1.3f)+.5f),1,4);const float step=lng/float(pieces);
 const float yawBase=alongX?0.f:kPi*.5f;Vec2 prev{};float prevTop=0;
 for(int i=0;i<pieces;++i){
  const float t=-lng*.5f+step*(float(i)+.5f)+(next()-.5f)*step*.15f;
  const float ph=h*(i==pieces/2?1.f:.6f+next()*.3f),pw=step*1.28f,pd=sht*(.88f+next()*.18f);
  const Vec2 c=alongX?Vec2{x+t,y+(next()-.5f)*sht*.1f}:Vec2{x+(next()-.5f)*sht*.1f,y+t};const float yaw=yawBase+(next()-.5f)*.5f+(next()<.5f?kPi:0.f);
  if(ph>.9f||std::max(pw,pd)>1.4f){caCrag(m,c.x,c.y,z,alongX?pw:pd,alongX?pd:pw,ph,g^unsigned(i*7919));
   const float q=std::min(ph*.32f,.55f);caFit(m,{model::cave_boulder_a,model::cave_rock_slab},c.x+(next()-.5f)*pw*.6f,c.y+(next()-.5f)*pd*.6f,z-.04f,q*1.7f,q*1.3f,q,yaw);}
  else if(next()<.5f)caFit(m,{model::cave_boulder_round,model::cave_rock_column,model::cave_boulder_a,model::cave_rock_slab},c.x,c.y,z-.06f,pw,pd,ph,yaw);
  else caFit(m,{model::cave_rock_block,model::cave_stalagmite,model::cave_rock_slab,model::cave_boulder_a},c.x,c.y,z-.06f,pw,pd,ph,yaw);
  if(i>0){const Vec2 s=(c+prev)*.5f;const float sh=std::min(ph,prevTop)*.58f,sw=sht*.36f,st=.08f+next()*.08f;
   if(alongX)caSeam(m,s.x-st*.5f,s.y-sw*.5f,s.x+st*.5f,s.y+sw*.5f,z,z+sh);else caSeam(m,s.x-sw*.5f,s.y-st*.5f,s.x+sw*.5f,s.y+st*.5f,z,z+sh);}
  prev=c;prevTop=ph;}
 // wet at its foot, where it stands on a floor: sheen, and the growth out of the cracks
 if(caOnFloor(m,x,y,z))for(int k=0;k<3;++k){const float a=next()*6.28f,r=.5f+next()*.2f,px=x+std::cos(a)*w*r,py=y+std::sin(a)*d*r,s=.12f+next()*.2f;
  caSeam(m,px-s*1.6f,py-s*1.2f,px+s*1.6f,py+s*1.2f,z+.002f,z+.005f,17);
  caSeam(m,px-s*.5f,py-.025f,px+s*.5f,py+.025f,z+.002f,z+.012f);caSeam(m,px-.025f,py-s*.4f,px+.025f,py+s*.4f,z+.002f,z+.012f);}
 if(solid)wall(m,x-w*.3f,y-d*.3f,x+w*.3f,y+d*.3f,z,z+h*.75f,CollisionProxyMaterial);
}
// A mound of growth: a heap of the rock, a boulder it brought up broken off at its foot, the pods
// budding in its seams.
void caMound(Map& m,float x,float y,float f,float r,float h){
 const unsigned s=caSeed(x,y);
 caGrowth(m,x,y,f,r*2.f,r*1.5f,h,true);
 caModel(m,model::cave_boulder_round,x-r*((s&8u)?1.05f:-1.05f),y+r*.55f,f-.12f,.63f*r,.434f*r,.596f*r,float(s%628u)*.01f);
 for(int i=0;i<1+int(h>.9f);++i){const float a=float((s>>(4+i*5))%628u)*.01f,k=.62f+.1f*float(i),sc=.42f+.18f*float((s>>(i*3))&3u)/3.f;
  caModel(m,model::cave_nest_pod,x+std::cos(a)*r*k,y+std::sin(a)*r*.75f*k,f-.04f,.7f*sc,.7f*sc,.56f*sc,a);}
}
// A cluster of crystal, h at its tallest, from a root at (x,y) on the floor f: the root a crown of
// rock with the rest of it under the floor where there is ground for it, rubble and the bed of
// spikes round it, the prisms fanned out of it, the leader nearly upright, most of them short.
// A cluster a metre or more high has its own cold light in among the prisms.
void caKnot(Map& m,float x,float y,float f,float h){
 unsigned g=caSeed(x,y)|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 const bool ground=std::fabs(f-m.layers.front().elevation)<.35f;
 const float rw=std::clamp(.6f+h*.3f,.7f,2.2f),show=std::clamp(.16f+h*.09f,.18f,.5f);
 const int root=(g&2u)?model::cave_rock_block:model::cave_boulder_a;const auto& v=model::size[root];const float k=rw/v[0],rh=v[2]*k;
 if(ground)caModel(m,root,x,y,f+show-rh,rw,v[1]*k,rh,next()*6.28f);
 else{const auto& sl=model::size[model::cave_rock_slab];const float q=(show+.04f)/sl[2];caModel(m,model::cave_rock_slab,x,y,f-.02f,sl[0]*q,sl[1]*q,sl[2]*q,next()*6.28f);}
 const float top=f+show;
 {const float q=rw*.34f;caModel(m,model::cave_oldcity_rubble,x+(next()-.5f)*rw*.9f,y+(next()-.5f)*rw*.7f,f,1.71f*q,1.71f*q,.3f*q,next()*6.28f);}
 caBed(m,x+(next()-.5f)*.3f,y+(next()-.5f)*.3f,f,std::clamp(rw*.95f,.7f,1.9f),next()*6.28f);
 {const float b=next()*6.28f;caBed(m,x+std::cos(b)*rw*.6f,y+std::sin(b)*rw*.6f,f,.5f+next()*.4f,next()*6.28f);}
 // the crystal itself: the modelled cluster out of the root's crown, a smaller one beside it on a big knot
 const float a0=next()*6.28f;
 caCrystalTo(m,x,y,top-.05f,h,a0);
 if(h>=.9f)caCrystalTo(m,x+std::cos(a0+2.4f)*rw*.42f,y+std::sin(a0+2.4f)*rw*.42f,f,h*(.35f+next()*.15f),a0+1.3f);
 wall(m,x-rw*.28f,y-rw*.28f,x+rw*.28f,y+rw*.28f,f,f+std::min(h,1.6f)*.8f,CollisionProxyMaterial);
 // its light in among the prisms, lifted clear of anything solid it would sit in
 if(h>=1.f){const Vec2 lp{x+std::cos(a0)*rw*.42f,y+std::sin(a0)*rw*.42f};float lz=top+h*.3f;
  for(int pass=0;pass<3;++pass)for(const auto& s:m.structures)if(s.solid&&lp.x>s.x1-.05f&&lp.x<s.x2+.05f&&lp.y>s.y1-.05f&&lp.y<s.y2+.05f&&lz>s.bottom-.05f&&lz<s.top+.05f)lz=s.top+.08f;
  if(lz<f+h+.4f)m.lights.push_back(caGlow(lp,lz,.5f+h*.16f,3.4f+h*1.3f,9000,{.55f,.85f,1.f},.06f));}
}
// A trunk of growth between two heights, where it ran down a face or reached the roof: the pack's
// rock buttress stacked in pieces near its own proportions, wandering a little, the growth running
// up it in veins and wet at its foot.
void caTrunk(Map& m,float x,float y,float lo,float hi,float w){
 unsigned g=caSeed(x,y+lo)|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 const float pw=w*1.5f;const int n=std::max(2,int((hi-lo)/1.6f+.5f));const float step=(hi-lo)/float(n);
 auto col=[&](float cx,float cy,float z1,float z2,float r,int mat){wall(m,cx-r,cy-r,cx+r,cy+r,z1,z2,mat);auto& s=m.structures.back();s.shape=ShapeColumn;s.solid=false;};
 // the rock's own finish in courses that wander a hand off the line, broad at the foot and the head
 for(int i=0;i<n;++i){const float r=w*.5f*((i==0||i==n-1)?1.2f:.85f+next()*.25f);col(x+(next()-.5f)*w*.22f,y+(next()-.5f)*w*.22f,lo+step*float(i)-(i?.12f:0.f),lo+step*float(i+1)+(i+1<n?.12f:0.f),r,26);}
 for(int k=0;k<2+int(w>.45f);++k){const float a=next()*6.28f,rr=w*.42f,z1=lo+(hi-lo)*next()*.35f,z2=std::min(hi-.05f,z1+(hi-lo)*(.2f+next()*.45f));col(x+std::cos(a)*rr,y+std::sin(a)*rr,z1,z2,.025f+next()*.03f,34);}
 if(caOnFloor(m,x,y,lo)){const float q=std::min(w*.6f,.5f);caFit(m,{model::cave_boulder_a,model::cave_rock_slab},x+(next()-.5f)*w,y+(next()-.5f)*w,lo-.04f,q*1.7f,q*1.3f,q,next()*6.28f);
  const float s=pw*.9f;caSeam(m,x-s,y-s*.7f,x+s,y+s*.7f,lo+.002f,lo+.005f,17);caSeam(m,x-s*.8f,y-.02f,x+s*.8f,y+.02f,lo+.002f,lo+.012f);}
}
// A seam portal: pilasters either side of a wall-to-wall opening along y at x (the chunk side),
// with a lintel at head height up to the roof.
void caPortalY(Map& m,float x1,float x2,float y1,float y2,float f,float roof,float head){
 wall(m,x1,y1-.6f,x2,y1,f,roof,15);wall(m,x1,y2,x2,y2+.6f,f,roof,15);wall(m,x1,y1,x2,y2,f+head,roof,15);
 for(float y:{y1-.08f,y2})wall(m,x1-.04f,y,x2+.04f,y+.08f,f,f+head,20);
 floorLine(m,x1,y1,x2,y2,f,13);
}

// QUARANTINE WARD (manifest 66). Where exposed staff were sealed in after the surge: the
// airlock off the Morgue, a corridor of wired-glass wards, the nurses' station, and the Cleanup
// detail's own dead laid out in the holding room. Ward Q2 was boarded up from the corridor at
// 03:40 with its patients inside. East, a passage nobody lit but her: LAB 4's lamps run toward the
// Isolation Wing, and on the wall in her hand, "follow the lamps".
// Route: the N door from the Morgue -> airlock -> corridor S to the hub (95); E passage to 100.
std::shared_ptr<const Map> makeQuarantineWard(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[94];m.definition=CampaignChunks[94];m.definition.ambient=.23f;
 m.definition.playerStart={12.f,1.8f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});                                                   // door 0: in from the Morgue
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"cona_qw_clock",60000);const char* clock="cona_qw_clock";
 titleCard(m,"CONTAINMENT",8.5f,1.f,15.5f,6.f);
 // --- The airlock: suits on the rail, the showers, the inner door ------------------------------
 wall(m,8.3f,1.f,8.5f,6.2f,f,roof,3);wall(m,15.5f,1.f,15.7f,6.2f,f,roof,3);
 wallDoor(m,8.3f,15.7f,6.f,6.2f,11.4f,12.6f,f,roof,1);                                        // door 1
 wall(m,14.95f,1.7f,15.5f,3.9f,f+1.9f,f+1.94f,20);
 for(float y:{2.f,2.75f,3.5f})hungSuit(m,{15.2f,y,f+1.5f},caW+(y-2.75f)*.2f);              // three hung back; the rest went in
 for(float x:{9.4f,10.6f}){wall(m,8.5f,x-7.f,8.62f,x-6.8f,f+2.1f,f+2.16f,20);caFix(m,model::med_floor_drain,9.3f,x-6.4f,f,0,false);}
 caFix(m,model::med_scrub_sink,8.78f,4.6f,f,caE,true);
 caFix(m,model::med_first_aid_box,15.48f,4.9f,f+1.3f,caW,false);
 floorLine(m,8.5f,5.5f,15.5f,5.65f,f,13);
 wallSign(m,12.f,5.98f,f+2.65f,"HOODS ON PAST THIS DOOR","EXPOSURE TIER 3 AND OVER / WARD STAFF ONLY",2.4f,caN);
 m.lights.push_back(caTube(12.f,3.f,roof,.55f,5.5f,5600,.04f));
 { auto l=lamp({10.2f,5.98f},f+2.4f,.3f,3.6f,3000,0,WorldLightMount::Wall,clN,true,{1.f,.5f,.06f});l.period=1.f;l.onTime=.5f;m.lights.push_back(l);}   // the cycle lamp: amber, never cleared
 kit::prop(m,model::item_dosimeter,11.3f,4.2f,f,.31f,.146f,.066f,2.1f);
 // --- The corridor: wards west, station and holding east --------------------------------------
 // West wall (x 9.8-10): Q1's glass and doorway, Q2's glass and its boarded doorway.
 wall(m,9.8f,6.2f,10.f,7.f,f,roof,3);caGlaze(m,9.8f,7.f,10.f,10.6f,f,roof);wall(m,9.8f,10.6f,10.f,11.2f,f,roof,3);
 doorwayY(m,11.2f,12.6f,9.8f,10.f,f,roof);wall(m,9.8f,12.6f,10.f,15.6f,f,roof,3);
 caGlaze(m,9.8f,15.6f,10.f,19.f,f,roof);wall(m,9.8f,19.f,10.f,19.8f,f,roof,3);
 doorwayY(m,19.8f,21.2f,9.8f,10.f,f,roof);wall(m,9.8f,21.2f,10.f,23.f,f,roof,3);
 boardedGap(m,9.84f,19.8f,9.96f,21.2f,f,f+2.3f);                                              // Q2, boarded from the corridor at 03:40
 for(float z:{.5f,1.2f,1.9f})wall(m,10.f,19.6f,10.06f,21.4f,f+z,f+z+.16f,10);
 taped(m,{10.f,19.4f},f+1.55f,caE,Readable::Note,"Q2 / SEALED 03:40","NINE IN Q2. ALL TIER 3. DOORWAY BOARDED BY ORDER. DO NOT FEED. DO NOT ANSWER.",
  "THEY STOPPED KNOCKING AT FOUR. ONE OF THEM STILL SAYS HIS NAME IF YOU WALK PAST.");
 // East wall (x 14-14.2): the station's counter glass, the passage mouth, the holding room's glass.
 wall(m,14.f,6.2f,14.2f,6.6f,f,roof,3);caGlaze(m,14.f,6.6f,14.2f,8.8f,f,roof,1.05f,2.3f);wall(m,14.f,8.8f,14.2f,9.6f,f,roof,3);
 doorwayY(m,9.6f,13.4f,14.f,14.2f,f,roof,2.6f);wall(m,14.f,13.4f,14.2f,16.f,f,roof,3);
 caGlaze(m,14.f,16.f,14.2f,20.f,f,roof);wall(m,14.f,20.f,14.2f,23.f,f,roof,3);
 // the corridor's light: cold tubes, two gone, the lockdown red down both walls
 for(auto [y,fl]:{std::pair{8.f,0.f},{16.f,.25f}})m.lights.push_back(caTube(12.f,y,roof,.62f,6.f,6000,.04f,fl));
 caDeadTube(m,12.f,12.f,roof);caDeadTube(m,12.f,20.f,roof);
 m.lights.push_back(caBeacon({10.f,15.4f},f+2.55f,clE,0,"cont_power"));
 m.lights.push_back(caBeacon({14.f,21.8f},f+2.55f,clW,.7f,"cont_power"));
 m.lights.push_back(caTube(12.f,22.4f,roof,.58f,6.f,5600,.03f,.25f));                         // the way on to the hub, still lit
 // The corridor's soffit: downstand beams across it on a 2.4 m rhythm, the ward's two service runs
 // along its edges, so the long run reads as bays rather than one plaster tube.
 {const float westSolid[][2]={{6.2f,7.f},{10.6f,11.2f},{12.6f,15.6f},{19.f,19.8f},{21.2f,23.f}},eastSolid[][2]={{6.2f,6.6f},{8.8f,9.6f},{13.4f,16.f},{20.f,23.f}};
  auto solid=[](const float (*r)[2],int n,float y){for(int i=0;i<n;++i)if(y-.05f>r[i][0]&&y+.3f<r[i][1])return true;return false;};
  for(float y=7.4f;y<22.5f;y+=2.4f){wall(m,10.f,y,14.f,y+.24f,roof-.34f,roof,15);                  // pilasters only where the wall is plain
   if(solid(westSolid,5,y))wall(m,10.f,y-.04f,10.14f,y+.28f,f,roof-.34f,15);if(solid(eastSolid,4,y))wall(m,13.86f,y-.04f,14.f,y+.28f,f,roof-.34f,15);}}
 for(auto [x,paint]:{std::pair{10.4f,3},{13.6f,4}})servicePipe(m,{x,6.12f},{x,23.f},roof-.2f,.07f,paint);
 caFix(m,model::med_wheelchair,13.4f,14.8f,f,caW+.4f,true);
 kit::prop(m,model::med_crutches,10.4f,17.6f,f,.4f);
 paperDrift(m,10.2f,6.6f,13.8f,22.8f,f,8,9401);
 // The one they did bag: a gurney left against the holding glass, the bag taped shut twice over.
 caFix(m,model::med_ward_bed,13.3f,17.9f,f,caE,true);
 wall(m,13.02f,17.f,13.58f,18.8f,f+.6f,f+.84f,8);m.structures.back().solid=false;
 for(float y:{17.45f,18.3f}){wall(m,12.99f,y,13.61f,y+.07f,f+.6f,f+.86f,13);m.structures.back().solid=false;}
 // The ward alarm: the panel by Q1 still calling a breach nobody answered, under its own amber lamp.
 caFix(m,model::lab_status_display,10.03f,13.45f,f+1.85f,caE,false);
 caFix(m,model::lab_control_board,10.1f,13.45f,f+1.05f,caE,false);
 terminal(m,{10.5f,14.15f},"WARD ALARM / Q1","BAY 2 TENT: SEAL BREACH 04:02. NEGATIVE PRESSURE LOST. BAG 2 OPEN FROM INSIDE.",
  "ALARM NOT ACKNOWLEDGED. WARD DOORS NOT RESPONDING. CALLING CLEANUP. CALLING CLEANUP.",0);
 m.terminals.back().yaw=caE;
 { auto l=lamp({10.f,13.45f},f+2.35f,.55f,5.f,3000,0,WorldLightMount::Wall,clE,true,{1.f,.45f,.05f});l.period=.9f;l.onTime=.45f;m.lights.push_back(l);}
 // --- Ward Q1: four beds, curtains between, the monitors dark ----------------------------------
 wall(m,1.f,6.2f,9.8f,6.4f,f,roof,3);
 // Q2 boarded from the corridor, so they went through the plaster into Q1: a ragged hole at the east end.
 wall(m,1.f,14.4f,6.9f,14.6f,f,roof,3);wall(m,8.3f,14.4f,9.8f,14.6f,f,roof,3);wall(m,6.9f,14.4f,8.3f,14.6f,f+2.05f,roof,3);
 for(auto [x1,x2,lo,hi]:{std::tuple{6.9f,7.12f,0.f,1.4f},{8.1f,8.3f,.3f,2.05f},{7.1f,7.6f,1.97f,2.05f}})wall(m,x1,14.38f,x2,14.62f,f+lo,f+hi,3);
 kit::prop(m,model::prop_cardboard_box_open,7.4f,13.7f,f,.36f,.36f,.36f,.6f);
 for(int i=0;i<4;++i){const float y=7.6f+i*1.9f;
  caFix(m,i==2?model::med_ward_bed_tossed:model::med_ward_bed,2.05f,y,f,0,true);
  if(i<3){wall(m,1.f,y+.92f,3.7f,y+.98f,f+.25f,f+2.1f,39);wall(m,1.f,y+.92f,3.7f,y+.98f,f+2.1f,roof,CollisionProxyMaterial);}
  kit::prop(m,model::med_vitals_monitor_dead,3.55f,y-.62f,f,caE);}
 // Bay 2, framed by the ward doorway from the corridor: the isolation tent the deceased of 03:15 was
 // bagged in. Its corridor side is torn open from inside, the bag lies split on the floor, and the
 // drag of him runs out through the doorway, down the corridor, to Q2's boards.
 for(float y:{10.5f,12.26f})wall(m,3.72f,y,3.8f,y+.08f,f,f+2.26f,20);                          // tent posts
 wall(m,3.72f,10.5f,3.8f,12.34f,f+2.18f,f+2.26f,20);wall(m,1.f,10.48f,3.8f,12.34f,f+2.26f,f+2.28f,39);   // head rail, the plastic roof
 for(auto [y1,y2,lo]:{std::tuple{10.58f,11.12f,.55f},{11.78f,12.26f,.3f},{11.3f,11.5f,1.7f}})wall(m,3.75f,y1,3.77f,y2,f+lo,f+2.18f,39);   // the flap, torn out from inside
 wall(m,1.3f,10.9f,3.5f,12.05f,f+.004f,f+.008f,16);wall(m,3.4f,11.25f,4.3f,11.7f,f+.004f,f+.008f,16);                // what ran out of the bed
 // The tent's exam lamp, the one hard white light in Q1: framed by the ward doorway from the corridor,
 // it puts the torn flap, the split bag and the start of the drag in a pool you see from the airlock.
 m.pipes.push_back({{2.7f,11.4f},{2.7f,11.4f},f+2.2f,.016f,f+2.27f,6});
 m.fixtures.push_back({model::pendant_lamp,{2.7f,11.4f},f+1.86f-m.layers.front().elevation,.5f,.5f,.34f,0,false});
 {auto l=lamp({2.7f,11.4f},f+1.8f,1.9f,6.5f,6200,.02f,WorldLightMount::Bare);l.flicker=.08f;m.lights.push_back(l);}
 {auto l=lamp({3.95f,12.2f},f+2.1f,.7f,5.f,3000,0,WorldLightMount::Bare,0,false,{1.f,.1f,.05f});l.period=1.2f;l.onTime=.6f;m.lights.push_back(l);}   // the bay's alarm, never answered
 caFix(m,model::med_vitals_monitor,3.9f,12.18f,f+1.45f,caE,false);
 wall(m,3.8f,12.08f,3.86f,12.3f,f+1.2f,f+1.45f,2);
 for(auto [x1,y1,x2,y2]:{std::tuple{4.1f,10.7f,4.9f,10.95f},{4.5f,12.1f,5.6f,12.32f},{5.2f,11.0f,5.5f,11.6f}})wall(m,x1,y1,x2,y2,f+.002f,f+.012f,39);
 wall(m,4.2f,11.18f,6.f,11.72f,f,f+.07f,8);wall(m,4.2f,11.43f,6.f,11.47f,f+.07f,f+.075f,20);          // the bag, split along its zip
 wall(m,4.6f,11.16f,4.68f,11.74f,f+.07f,f+.078f,13);
 caTrail(m,{{6.f,11.5f},{8.4f,11.8f},{9.9f,11.9f},{11.4f,12.6f},{11.8f,15.f},{11.2f,18.2f},{10.2f,20.5f}},f,94);
 for(float y:{20.1f,20.6f,21.f})wall(m,10.06f,y,10.07f,y+.18f,f+.7f+(y-20.f)*.6f,f+1.6f,16);       // hands on the boards
 m.lights.push_back(caGlow({3.1f,11.9f},f+.35f,.6f,4.5f,4600,{1,1,1},.15f));                        // the exam lamp, on its side in the tent
 caFix(m,model::med_crash_cart,7.6f,7.f,f,0,true);
 caFix(m,model::med_supply_cabinet,6.f,6.62f,f,caS,true);wall(m,5.4f,6.4f,6.6f,6.9f,f+1.72f,roof,CollisionProxyMaterial);
 for(auto [x,y,fl]:{std::tuple{3.6f,8.4f,0.f},{7.4f,10.4f,.15f}})m.lights.push_back(caTube(x,y,roof,.3f,5.f,4300,.25f,fl));
 caDeadTube(m,3.6f,13.2f,roof);
 m.pickupSpawns.push_back({{7.8f,13.2f},PickupKind::Health,f});
 // --- Ward Q2 (boarded): what nine people did to a room in an hour ------------------------------
 for(auto [x,y,mdl,yaw]:{std::tuple{2.05f,16.f,model::med_bed_frame_wrecked,0.f},{3.f,18.6f,model::med_ward_bed_tossed,.5f},{2.05f,21.7f,model::med_ward_bed,0.f},{7.2f,21.8f,model::med_bed_frame_wrecked,1.3f}})
  caFix(m,mdl,x,y,f,yaw,true);
 m.fixtures.push_back({model::med_mattress_stained,{6.2f,17.2f},.0f,1.9f,.9f,.118f,.7f,false});
 for(auto [x1,y1,x2,y2]:{std::tuple{4.6f,15.4f,6.f,16.2f},{5.2f,19.f,7.4f,19.7f},{8.4f,20.2f,9.8f,21.f}})wall(m,x1,y1,x2,y2,f+.004f,f+.01f,16);
 for(float y:{15.2f,17.8f,20.6f})wall(m,1.f,y,1.03f,y+.9f,f+.6f,f+2.f,16);                  // on the wall, hand height
 m.lights.push_back(caGlow({5.f,18.f},f+2.6f,.22f,4.f,1000,{1.f,.15f,.08f},.5f));
 m.creatureSpawns.push_back({CreatureKind::Transformed,{5.6f,18.8f},f,1,0});                 // the one who still says his name
 // --- Nurses' station: the counter onto the corridor, the board, the kettle ---------------------
 wall(m,14.2f,6.2f,23.f,6.4f,f,roof,3);
 wallDoor(m,14.2f,23.f,9.f,9.2f,16.f,17.2f,f,roof,-1);                                       // door 2: station from the passage
 workstation(m,19.f,7.9f,f,-1,1);workstation(m,21.2f,7.9f,f,-1,2);
 caFix(m,model::med_records_cabinet,22.4f,6.66f,f,caS,true);
 caFix(m,model::med_notice_board,15.6f,6.44f,f+1.25f,caS,false);
 terminal(m,{14.5f,7.7f},"QUARANTINE / WARD STATE","Q1: 4 BEDS / 4 OCCUPIED / 1 DECEASED 03:15. Q2: 9 OCCUPIED / SEALED 03:40 BY CLEANUP ORDER.",
  "LAB 4 (ENERGY DIAG.) MOVED TO ISOLATION 3, 02:55, ON THE TIER 3 READING. SHE ASKED FOR A PEN.",0);
 m.terminals.back().yaw=caE;
 m.lights.push_back(caTube(19.f,7.7f,roof,.45f,5.f,3800,0));
 // --- The holding room: the detail's dead laid out under sheets --------------------------------
 wallDoor(m,14.2f,23.f,13.8f,14.f,19.4f,20.6f,f,roof,1);                                    // door 3: the passage's south wall
 for(auto y:{16.2f,18.4f,20.6f})m.fixtures.push_back({model::med_corpse_supine,{21.7f,y},0,.744f,1.753f,.411f,caN,false});
 caBody(m,clock,ActorVisual::Cleanup,{17.8f,20.2f},f,1.2f);
 for(int i=0;i<3;++i)caFix(m,model::psx_locker,15.2f+i*.92f,22.66f,f,kPi*.5f,true);
 wall(m,14.7f,22.2f,17.6f,23.f,f+2.3f,roof,CollisionProxyMaterial);
 caFix(m,model::lab_specimen_crate,17.f,15.2f,f,.2f,true,.7f);
 kit::prop(m,model::item_walkie,17.f,15.2f,f+.79f,.114f,.376f,.046f,.4f);
 taped(m,{22.98f,15.f},f+1.5f,caW,Readable::Note,"QUARANTINE DETAIL / 03:50","FOUR DOWN. THREE ON SHEETS. HE SAYS NO TRANSFERS, NO BAGS LEAVE THE SECTOR. WE WRITE THEIR NAMES ON THE SHEETS.",
  "REYES SAYS THE ONE IN Q1 SAT UP AT FOUR. WE DIDN'T GO IN. WE'RE GOING SOUTH TO THE HUB.");
 m.lights.push_back(caTube(18.6f,17.4f,roof,.45f,5.5f,5000,.03f));
 // It was the ward's overflow before it was a morgue: a bed they could not lift him off, the crash
 // cart that did nothing, the screens wheeled back; one lamp pulled down low over the sheets to write by.
 caFix(m,model::med_ward_bed,20.6f,14.95f,f,0,true);caFix(m,model::med_crash_cart,15.3f,16.6f,f,caE,true);
 kit::prop(m,model::med_vitals_monitor_dead,15.3f,16.6f,f+1.f,caE);
 m.fixtures.push_back({model::med_corpse_supine,{20.6f,14.95f},.66f,.744f,1.753f,.411f,caE,false});
 caPendant(m,{20.9f,18.4f},roof,f+2.2f,.85f,5.5f,2900);
 paperDrift(m,19.6f,17.f,20.6f,19.8f,f,5,941);
 m.pickupSpawns.push_back({{16.2f,22.f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{17.6f,15.9f},PickupKind::HandgunAmmo,f});
 // --- The east passage: her lamps ----------------------------------------------------------------
 // Nobody lights a corridor with desk lamps on the floor. She did, one every few metres, toward Isolation.
 for(auto [x,y,yaw]:{std::tuple{15.4f,12.9f,.4f},{18.6f,10.1f,2.f},{21.6f,12.8f,-.6f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=caGlow({x,y},f+.45f,.38f,3.6f,2700);l.prop=lampProp;m.lights.push_back(l);}
 taped(m,{22.4f,9.21f},f+1.45f,caS,Readable::Note,"WRITTEN ON THE WALL, IN PEN","FOLLOW THE LAMPS.",
  "I'LL LEAVE THEM LIT WHERE I'VE BEEN. DON'T LET THEM SEE YOU READ THIS. - L4");
 // --- Names, the air ----------------------------------------------------------------------------
 recessedFittings(m);
 zone(m,"QUARANTINE WARD / AIRLOCK",8.5f,0,15.5f,6.f,-1000,1000,{.13f,.15f,.16f,.009f});
 zone(m,"QUARANTINE WARD / Q1",1.f,6.4f,9.8f,14.4f,-1000,1000,{.12f,.14f,.13f,.008f});
 zone(m,"QUARANTINE WARD / Q2",1.f,14.6f,9.8f,23.f,-1000,1000,{.14f,.10f,.09f,.011f});
 zone(m,"QUARANTINE WARD / NURSES' STATION",14.2f,6.4f,23.f,9.f);
 zone(m,"QUARANTINE WARD / HOLDING",14.2f,14.f,23.f,23.f,-1000,1000,{.13f,.14f,.16f,.012f});
 zone(m,"QUARANTINE WARD / EAST PASSAGE",14.2f,9.2f,24.f,13.8f,-1000,1000,{.16f,.12f,.08f,.012f});
 zone(m,"QUARANTINE WARD",0,0,24,24,-1000,1000,{.13f,.15f,.17f,.010f});
 freightFinish(m,-1);
 return p;
}

// CONTAINMENT INTAKE (manifest 61). The hub: a tall intake hall where specimens came in on pallets
// and went east to A or south to B. The south door to B held shut when the containment feed dropped
// at 02:14 and it is still shut: the hub says so on the door and on the gallery console, and the
// feed is restored from the Conversion Lab, out through the west gallery onto the Observation Ring.
// The Intake detail holds a barricade facing that door. Four men with orders about the player they
// do not mean to keep: a truce while the gun stays down, and the Investigator on the radio.
// Route: N seam from Quarantine -> the hall -> E to Containment A, or up the gallery stair -> W
// (at -12) to the Observation Ring; with cont_power, the S door to Containment B.
std::shared_ptr<const Map> makeContainmentIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[95];m.definition=CampaignChunks[95];m.definition.ambient=.26f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,g=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'S',10.f,14.f,f,true,stateId("cont_power")});                                     // door 0: Containment B, on the feed
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'W',9.f,14.f,g,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 machineryClock(m,"cona_hub_clock",60000);const char* clock="cona_hub_clock";
 const StateId stance=stateId("cona_hub_stance");
 // --- The west gallery (-12): the intake console over the hall, the way out to the ring --------
 wall(m,1.f,3.f,6.f,20.f,f,g,37);wall(m,0,8.9f,1.f,14.1f,f,g,15);                           // the gallery's mass (painted plant green to the band), and its lip to the ring
 wall(m,5.98f,3.f,6.f,20.f,f+1.08f,f+1.16f,13);                                                     // the hazard edge along its foot
 // its east face is the hall's west wall: lamps on it, the intake's supply and records cabinets and the board at its foot
 for(float y:{9.f,15.f}){m.lights.push_back(lamp({6.f,y},f+2.35f,.6f,6.5f,4600,.05f,WorldLightMount::Wall,clE));
  m.fixtures.push_back({model::mine_cage_lamp,{6.1f,y},f+2.15f-m.layers.front().elevation,.21f,.2f,.36f,caE,false});}
 caFix(m,model::med_supply_cabinet,6.24f,12.6f,f,caE,true);caFix(m,model::med_records_cabinet,6.27f,13.8f,f,caE,true);
 caFix(m,model::lab_notice_board,6.03f,17.6f,f+1.25f,caE,false);caFix(m,model::pwr_fire_cabinet,6.1f,6.2f,f+1.1f,caE,false);
 m.stairs.push_back({6.f,3.3f,10.48f,4.8f,f,g,16,false,false});
 guardRail(m,5.9f,4.8f,6.f,20.f,g);guardRail(m,1.f,19.9f,6.f,20.f,g);guardRail(m,1.f,3.f,6.f,3.1f,g);
 floorLine(m,5.7f,4.8f,5.82f,19.9f,g,13);
 wall(m,1.f,8.4f,1.4f,9.f,g,roof,15);wall(m,1.f,14.f,1.4f,14.6f,g,roof,15);wall(m,0,9.f,1.4f,14.f,g+2.8f,roof,15);   // the ring portal
 caFix(m,model::lab_control_board,1.1f,6.4f,g+.95f,caE,false);
 terminal(m,{1.75f,7.5f},"CONTAINMENT FEED / STATUS","A: LOCAL BATTERY 31%. B: NO FEED SINCE 02:14. DOOR B HELD SHUT ON LOSS OF FEED. B DETAIL INSIDE.",
  "TO RESTORE: CONVERSION LAB OUTPUT TO CONTAINMENT. CONVERSION IS WEST, ROUND THE OBSERVATION RING. THIS DECK.",g-f);
 m.terminals.back().yaw=caE;
 workstation(m,3.4f,17.4f,g,-1,2);
 caFix(m,model::lab_pa_horn,1.3f,12.f,g+2.7f,caE,false);
 for(float y:{5.f,17.f})m.lights.push_back(lamp({1.f,y},g+2.3f,.55f,6.f,3200,0,WorldLightMount::Wall,clE));
 m.lights.push_back(caGlow({1.5f,6.4f},g+1.3f,.25f,2.4f,8000,{.7f,.85f,1.f},.08f));             // the console's screens
 m.pickupSpawns.push_back({{2.2f,15.2f},PickupKind::Ammo,g});
 // --- The hall: intake pallets NE, the way east to A, the barricade, door B ---------------------
 for(float x:{17.6f,18.9f,20.2f})caFix(m,model::lab_specimen_crate,x,2.2f,f,(x-19.f)*.03f,true);
 handCart(m,16.2f,5.4f,f);
 caFix(m,model::lab_specimen_crate,21.4f,5.f,f,.3f,true,.75f);
 // Receiving, as it ran until two: the weigh pad under its sodium lamp with the last cage still on it,
 // the booking desk by the stair where the manifest was signed, the suits hung by the door for the
 // handlers, and the intake lane painted from the door to door B.
 equipmentPad(m,15.f,6.8f,20.6f,9.4f,f);caFix(m,model::med_holding_cage,17.8f,8.1f,f+.1f,0,true);
 caPendant(m,{17.8f,8.1f},roof,f+4.4f,1.5f,9.f,2000);
 caFix(m,model::lab_status_display,20.4f,6.75f,f+2.1f,caN,false);
 caFix(m,model::med_nurse_desk,8.3f,8.3f,f,caS,true);caModel(m,model::office_chair,8.4f,7.5f,f,.55f,.55f,.95f,caN+.4f,true);
 paperDrift(m,7.f,8.8f,9.6f,9.8f,f,5,951);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,7.86f,8.3f,f+.81f,.18f,.4f,.52f,3.f);
  auto l=caGlow({7.8f,8.4f},f+1.3f,.5f,4.f,2900);l.prop=lampProp;m.lights.push_back(l);}
 wall(m,6.6f,1.f,8.6f,1.08f,f+2.1f,f+2.16f,8);for(float x:{6.9f,7.7f,8.3f})hungSuit(m,{x,1.33f,f+2.05f},caS);
 for(float x:{10.6f,13.3f})floorLine(m,x,1.f,x+.12f,9.9f,f,14);
 caPortalY(m,22.6f,23.f,9.f,14.f,f,roof,3.4f);
 caFix(m,model::lab_status_display,22.56f,11.5f,f+3.6f,caW,false);
 m.lights.push_back(caGlow({21.7f,11.5f},f+3.f,.6f,6.f,3400));                                 // A still has its own light
 // Door B: pilasters, lintel, the stripes, the reason on it, red until the feed, green after.
 wall(m,8.6f,22.4f,10.f,23.f,f,roof,15);wall(m,14.f,22.4f,15.4f,23.f,f,roof,15);wall(m,10.f,22.4f,14.f,23.f,f+3.2f,roof,15);
 floorLine(m,9.f,21.5f,15.f,21.7f,f,13);
 wallSign(m,12.f,22.38f,f+3.35f,"CONTAINMENT POWER: OFFLINE","DOOR B HELD ON LOSS OF FEED / ROUTE POWER FROM THE CONVERSION LAB",3.4f,caN);
 for(auto [x,ph]:{std::pair{9.3f,0.f},{14.7f,.7f}})m.lights.push_back(caBeacon({x,22.4f},f+2.9f,clN,ph,"cont_power"));
 {auto l=caGlow({12.f,22.1f},f+3.f,.6f,5.f,5000,{.2f,1.f,.3f});l.enabledState=stateId("cont_power");m.lights.push_back(l);}
 terminal(m,{16.1f,22.2f},"DOOR B","HELD: NO CONTAINMENT FEED. LAST OPENED 02:01 (B DETAIL, 6 MEN, KESSLER).","OPENS ON FEED. MANUAL OVERRIDE REMOVED BY ORDER 02:30.",0);
 m.terminals.back().yaw=caN;
 // The barricade: a wall of intake crates stacked two high from the gallery's foot across the hall,
 // a steel shield at the middle with a firing slit onto door B, and the east end where the line was
 // breached at 02:20 and never closed again: the way round it, striped and lit, past what got through.
 const float by=16.9f;
 for(float x:{6.65f,7.8f,8.95f,10.1f,13.9f,15.05f,16.2f,17.35f})caFix(m,model::lab_specimen_crate,x,by,f,(x-12.f)*.006f,true);
 for(float x:{6.65f,7.8f,10.1f,13.9f,16.2f}){wall(m,x-.53f,by-.53f,x+.53f,by+.53f,f+1.12f,f+2.2f,10);   // the second tier, lashed down
  wall(m,x-.55f,by-.06f,x+.55f,by+.06f,f+2.2f,f+2.22f,13);wall(m,x-.56f,by-.56f,x+.56f,by+.56f,f+2.22f,roof,CollisionProxyMaterial);}
 wall(m,10.66f,by-.1f,13.34f,by+.1f,f,f+1.25f,11);wall(m,10.66f,by-.1f,13.34f,by+.1f,f+1.43f,f+2.3f,11);   // the shield, the slit
 for(float x:{10.66f,13.24f})wall(m,x,by-.16f,x+.1f,by+.16f,f,f+2.36f,12);
 wall(m,10.66f,by-.1f,13.34f,by+.1f,f+2.3f,roof,CollisionProxyMaterial);
 for(float x:{11.1f,12.f,12.9f})wall(m,x-.12f,by-.1f,x+.12f,by-.12f,f+1.43f,f+1.47f,8);               // the slit's scars
 // the breach at the east end: a crate shoved out of the line, the barrier thrown, the drag marks
 caFix(m,model::lab_specimen_crate,18.55f,16.3f,f,.62f,true);
 caFix(m,model::barrier,18.7f,18.4f,f,1.15f,true);
 caTrail(m,{{18.2f,17.6f},{19.6f,17.1f},{20.6f,15.8f},{21.f,14.4f}},f,95);
 for(float x:{19.45f,22.6f})floorLine(m,x,15.f,x+.12f,19.2f,f,14);                                 // the way round, painted by the detail
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,8.95f,by,f+1.12f,.18f,.4f,.52f,kPi);
  auto l=caGlow({8.95f,17.1f},f+1.5f,.9f,7.f,3200);l.prop=lampProp;m.lights.push_back(l);}
 kit::prop(m,model::item_walkie,15.05f,16.8f,f+1.12f,.114f,.376f,.046f,.3f);
 caPendant(m,{21.f,18.3f},roof,f+3.6f,1.25f,8.f,3600);                                              // the work light over the way round
 // The checkpoint: a floodlight on a mast over the shield, thrown hard on door B, so from the north
 // door the line reads as a lit firing position with its back to you; and at the east end the
 // checkpoint gate it was built round, its frame still standing, the leaf wrenched off its hinge.
 wall(m,11.95f,by-.05f,12.05f,by+.05f,f+2.3f,f+3.15f,12);wall(m,11.7f,by-.2f,12.3f,by+.2f,f+2.3f,f+2.36f,8);
 wall(m,11.68f,by+.02f,12.32f,by+.34f,f+3.1f,f+3.48f,8);wall(m,11.72f,by+.34f,12.28f,by+.36f,f+3.14f,f+3.44f,20);
 m.lights.push_back(lamp({12.f,by+.3f},f+3.28f,2.3f,13.f,5200,0,WorldLightMount::Wall,clS,true));
 for(float x:{19.35f,22.72f})steelColumn(m,x,by,f,f+2.95f,false);
 wall(m,19.2f,by-.09f,22.87f,by+.09f,f+2.62f,f+2.95f,13);                                        // the gate head, striped
 caFix(m,model::mine_cage_gate,20.35f,15.55f,f,.95f,true);                                           // the leaf, forced in and left
 for(auto [x,z]:{std::pair{19.47f,.5f},{19.47f,1.9f}})wall(m,x,by-.06f,x+.1f,by+.06f,f+z,f+z+.18f,8);  // its hinge straps, torn
 // The hall's frame: a girder line across the roof on two concrete columns down the middle, pilasters
 // on the north and east walls, so the 7 m box has a structure the eye can measure it by.
 for(float y:{4.6f,10.35f,15.4f}){wall(m,6.f,y,22.6f,y+.3f,roof-.7f,roof,12);wall(m,6.f,y-.06f,22.6f,y+.36f,roof-.74f,roof-.7f,12);}
 for(float x:{9.6f,16.4f}){wall(m,x-.35f,10.15f,x+.35f,10.85f,f,roof-.7f,15);wall(m,x-.45f,10.05f,x+.45f,10.95f,f,f+.35f,15);
  wall(m,x-.42f,10.08f,x+.42f,10.92f,roof-1.05f,roof-.7f,15);floorLine(m,x-.45f,10.05f,x+.45f,10.17f,f+.35f,13);}
 wallBays(m,f,roof,4,true,false,false,true,12,11.5);
 // the two they brought back out of B's mouth before it shut, and the one who ran for it after
 for(float y:{17.6f,19.8f})m.fixtures.push_back({model::med_corpse_supine,{22.3f,y},0,.744f,1.753f,.411f,0,false});
 caBody(m,clock,ActorVisual::Cleanup,{6.8f,20.6f},f,2.2f);
 actor(m,clock,ActorVisual::CleanupEngineer,1.6f,{{0,{18.6f,14.3f},f,caW,3,.3f},{1000,{18.6f,14.3f},f,caW,3,.3f}});   // hit, sitting it out
 // --- The Intake detail: a truce while the gun stays down ----------------------------------------
 caGuard(m,clock,"cona_hub_heavy",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"cona_hub",{{{12.f,15.6f},f,60000,0}},12,1,.9f,130);
 m.actorTracks.back().ai.stance=stance;
 caGuard(m,clock,"cona_hub_medic",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"cona_hub",{{{17.8f,13.4f},f,60000,caE}},10,2,1.f);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("cona_hub_hello");t.ai.dropsSidearm=true;}
 caGuard(m,clock,"cona_hub_marks",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"cona_hub",{{{4.6f,14.6f},g,9000,.25f},{{4.6f,10.4f},g,6000,.6f}},15,1,1.1f);
 m.actorTracks.back().ai.stance=stance;
 m.dialogue.push_back({stateId("cona_hub_hello"),"INTAKE DETAIL","Gun down, stay out of our line, and we never saw each other. That door's all we're here for.","cona_hub_hello",{
  caReply("What's behind door B?","cona_hub_b"),caReply("How do I get power to it?","cona_hub_power"),caReply("I'm going.")}});
 m.dialogue.push_back({stateId("cona_hub_b"),"INTAKE DETAIL","Kessler's detail went in at two. Feed dropped at quarter past and the door held. We hear them on the radio. Not to us.","cona_hub_b",{caReply("Back.","cona_hub_hello")}});
 m.dialogue.push_back({stateId("cona_hub_power"),"INTAKE DETAIL","Conversion lab. Up the gallery, round the ring, west. Somebody has to turn its output to containment. Not us.","cona_hub_power",{caReply("Back.","cona_hub_hello")}});
 caEvent(m,"cona_hub_truce",8.f,0,16.f,6.f,"",{caSet("cona_hub_stance",int(SquadStance::Truce))});
 // The Investigator, over their radio. They answer him; they do not move.
 caSequence(m,"cona_hub_radio","cona_hub_radio_done",22000,20000,6.f,11.f,20.f,16.f);
 const Vec2 radio{12.f,15.6f};
 cue(m,"cona_hub_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. A contract security officer is loose in your sector. Female, armed, no clearance. Detain her.",.9f,f+1.5f,"cona_hub_radio_1");
 cue(m,"cona_hub_radio",10800,Sound::Empty,radio,"RADIO: Sir, we're four men on a door. We can't hold it and chase a guard.",.9f,f+1.5f,"cona_hub_radio_2");
 cue(m,"cona_hub_radio",16000,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then hold the door.",.9f,f+1.5f,"cona_hub_radio_3");
 cue(m,"cona_hub_radio",17600,Sound::Empty,radio,"RADIO: ...Copy.",.9f,f+1.5f,"cona_hub_radio_4");
 // The order was to hold the door. While door B had no feed there was nothing to hold it against;
 // the first time she comes back through with the feed on, he repeats it, and this time they obey.
 caSequence(m,"cona_hub_order","cona_hub_order_done",12000,11600,-9.f,-9.f,-8.f,-8.f);
 caEvent(m,"cona_hub_order_go",0,0,24,24,"cona_hub_radio_done",{caSet("cona_hub_order",1)});
 for(auto [id,value]:{std::pair{"cont_power",1},{"cona_hub_stance",int(SquadStance::Truce)},{"cona_hub_order",0}})
  m.scriptEvents.back().conditions.push_back({ScriptCondition::Source::State,stateId(id),ScriptCondition::Op::Equal,value});
 cue(m,"cona_hub_order",600,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. Door B has its feed. Nobody goes through it. Her least of all.",.9f,f+1.5f,"cona_hub_order_1");
 cue(m,"cona_hub_order",7900,Sound::Empty,radio,"RADIO: ...Copy. Sorry, miss. Hands where we can see them.",.9f,f+1.5f,"cona_hub_order_2");
 {ScriptAction pa;pa.type=ScriptAction::Type::PlaySound;pa.sound=Sound::PaUnauthorized;pa.amount=.7f;
  caEvent(m,"cona_hub_order_turn",0,0,24,24,"cona_hub_order_done",{caSet("cona_hub_stance",int(SquadStance::Hostile)),pa});
  m.scriptEvents.back().conditions.push_back({ScriptCondition::Source::State,stance,ScriptCondition::Op::Equal,int(SquadStance::Truce)});}
 // --- Light: pendants over the hall (one dead, one going), the barricade's lamp, door B's red ------
 caPendant(m,{9.f,8.f},roof,f+4.2f,1.5f,11.f,4200);caPendant(m,{16.f,8.f},roof,f+4.2f,1.4f,11.f,4200);
 caPendant(m,{9.5f,13.f},roof,f+4.2f,0,0,0);caPendant(m,{16.f,13.f},roof,f+4.2f,1.2f,10.f,4200,.3f);
 caPendant(m,{12.f,19.6f},roof,f+4.2f,.8f,8.f,4200,.15f);
 for(float y:{5.6f,21.f})m.lights.push_back(lamp({23.f,y},f+3.f,.7f,8.f,3400,0,WorldLightMount::Wall,clW));
 m.lights.push_back(caBeacon({23.f,18.f},f+2.8f,clW,.4f,"cont_power"));
 m.lights.push_back(lamp({3.5f,20.f},f+2.3f,.55f,6.f,3200,0,WorldLightMount::Wall,clS));               // the gallery's foot, and the pocket under it
 m.lights.push_back(lamp({3.5f,23.f},f+2.6f,.6f,6.f,5200,.04f,WorldLightMount::Wall,clN));               // and its face, from the south wall
 m.fixtures.push_back({model::mine_cage_lamp,{3.5f,22.9f},f+2.4f-m.layers.front().elevation,.21f,.2f,.36f,kPi,false});
 caFix(m,model::med_notice_board,4.7f,20.04f,f+1.3f,caS,false);caFix(m,model::ind_crate,5.3f,22.4f,f,.3f,true);
 m.lights.push_back(lamp({9.8f,1.f},f+2.3f,.6f,6.5f,4600,.05f,WorldLightMount::Wall,clS));                // the stair's flank, from the north wall
 m.fixtures.push_back({model::mine_cage_lamp,{9.8f,1.1f},f+2.1f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 m.fixtures.push_back({model::mine_cage_lamp,{3.5f,20.1f},f+2.1f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 m.pickupSpawns.push_back({{19.6f,13.f},PickupKind::Health,f});
 zone(m,"",6.f,18.f,18.f,24.f,-1000,1000,{.15f,.12f,.11f,.008f});
 zone(m,"CONTAINMENT INTAKE / GALLERY",0,3.f,6.f,20.f,g-.5f,1000,{.13f,.13f,.14f,.010f});
 zone(m,"CONTAINMENT INTAKE",0,0,24,24,-1000,1000,{.14f,.13f,.13f,.012f});
 // The stair: its stringers in safety yellow, and in place of a guard on every tread (a black comb
 // along both sides) a yellow handrail on posts, open below so the light gets through.
 {const size_t first=m.pipes.size();freightFinish(m,-1);for(size_t i=first;i<m.pipes.size();++i)m.pipes[i].material=3;
  for(auto& s:m.stairs)s.sideRails=false;
  auto tread=[&](float x){return f+(g-f)*(10.48f-x)/4.48f;};
  for(float y:{3.36f,4.74f}){m.pipes.push_back({{10.48f,y},{6.f,y},f+1.f,.025f,g+1.f,3});
   for(float x:{10.3f,8.9f,7.5f,6.15f})m.pipes.push_back({{x,y},{x,y},tread(x),.02f,tread(x)+1.f,3});}}
 return p;
}

// CONTAINMENT A (manifest 62). Intact: four cells along the north wall and two on the south, wired
// glass, each lit on A's own battery. A1 is clean, A2 has someone in it who stands at the glass, A3
// holds a piece of the ore on a plinth that hums, A4 was emptied at 02:40 by the Cleanup detail and
// nobody wrote down where to. Detail A still has its orders: two men, short of rounds, who shoot.
// Route: W seam from the hub -> the observation floor -> S through the service door to 97.
std::shared_ptr<const Map> makeContainmentA(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[96];m.definition=CampaignChunks[96];m.definition.ambient=.25f;
 m.definition.playerStart={2.2f,11.5f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=0;
 m.wallSurface=SurfaceClinicTile;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceLabPanel;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"cona_a_clock",60000);const char* clock="cona_a_clock";
 caPortalY(m,1.f,1.4f,9.f,14.f,f,roof,3.4f);
 // --- The north cells --------------------------------------------------------------------------
 for(float x:{6.4f,11.9f,17.4f})wall(m,x,1.f,x+.2f,6.7f,f,roof,27);
 const float cells[4][2]={{1.f,6.4f},{6.6f,11.9f},{12.1f,17.4f},{17.6f,23.f}};
 // The block reads from across the floor as one secured wall: four fronts under a hazard band, A1-A4
 // stencilled a metre high over each, the upper service hatches on their gantry, a reader at every
 // pier, each front washed from below the gantry. A4 is the one that went: its front blown out.
 // The block is two storeys: A1-A4 on the floor, A5-A8 over them off the gantry, each upper cell lit
 // on its own lamp. The A4/A8 stack is the one that went: A8's front is blown out from inside and
 // the growth has come out of it, over the gantry and down A4's face to the floor.
 const char* cellNames[4]={"A1","A2","A3","A4"};const char* upperNames[4]={"A5","A6","A7","A8"};
 const float up=f+3.04f;
 for(int i=0;i<4;++i){const float a=cells[i][0],b=cells[i][1],cx=(a+b)*.5f;
  wall(m,a,6.5f,a+.5f,6.7f,f,roof,15);wall(m,b-.5f,6.5f,b,6.7f,f,roof,15);
  wall(m,a,1.f,b,6.5f,f+2.9f,up,15);                                                                // the slab between the storeys
  if(i==3)caBlownFront(m,a+.5f,6.5f,b-.5f,6.7f,f,up,1,96);else caGlaze(m,a+.5f,6.5f,b-.5f,6.7f,f,up,.6f,2.2f,true,15);
  if(i==3)caBlownFront(m,a+.5f,6.5f,b-.5f,6.7f,up,roof,1,97);else caGlaze(m,a+.5f,6.5f,b-.5f,6.7f,up,roof,.75f,2.15f,true,15);
  floorLine(m,a+.5f,6.95f,b-.5f,7.1f,f,13);
  if(i<2)m.lights.push_back(lamp({cx,1.f},f+2.45f,i==1?.55f:.75f,6.f,6500,.02f,WorldLightMount::Wall,clS));
  if(i<3)m.lights.push_back(lamp({cx,1.f},up+2.f,.5f,6.f,6800,.02f,WorldLightMount::Wall,clS));      // the upper cells' own lamps
  caFix(m,model::med_floor_drain,cx,4.2f,f,0,false);
  caFix(m,model::lab_id_scanner,b-.25f,6.73f,f+1.15f,caS,false);
  caStencil(m,cellNames[i],cx,6.7f,1,f+2.34f,.46f);
  caStencil(m,upperNames[i],cx,6.7f,1,up+.12f,.5f);
  if(i==0||i==2)caFix(m,model::med_ward_bed,a+1.6f,2.f,up,0,false);
  m.fixtures.push_back({model::mine_cage_lamp,{cx,7.25f},f+2.5f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
  m.lights.push_back(caGlow({cx,7.25f},f+2.58f,i==3?.35f:.7f,6.f,6000,{1,1,1},i==3?.5f:0.f));
  m.lights.push_back(lamp({cx,7.3f},up+.35f,i==3?.25f:.5f,5.f,6000,.02f,WorldLightMount::Bare,0,false));}   // gantry floods up the upper fronts
 wall(m,1.f,6.7f,23.f,6.74f,f+2.86f,f+2.98f,13);                                                    // the band
 wall(m,1.f,6.74f,23.f,7.6f,f+2.98f,up,2);guardRail(m,1.f,7.56f,23.f,7.6f,up);                       // the gantry to the upper cells
 for(float x=2.f;x<23.f;x+=3.f)m.pipes.push_back({{x,7.5f},{x,7.5f},up,.025f,roof,6});
 // A8: the source of what got out. Crust lines the cell, a knot of crystal stands lit in it, and the
 // growth spills out over the sill, runs across the gantry and down A4's piers to a heave on the floor.
 caCrust(m,18.1f,1.f,22.5f,1.2f,up,roof);caCrust(m,22.75f,1.f,23.f,6.5f,up,roof);caCrust(m,18.1f,1.f,22.5f,6.4f,up,up+.08f);
 caMound(m,20.6f,3.f,up+.08f,1.2f,1.1f);caKnot(m,20.f,4.4f,up+.08f,1.6f);caKnot(m,21.4f,2.4f,up+.08f,1.2f);
 caGrowth(m,20.05f,6.62f,up,3.3f,1.1f,1.f);caLens(m,20.05f,7.1f,up,3.5f,.9f,.12f);      // over the sill and onto the gantry
 for(auto [x,lo,w]:{std::tuple{18.8f,0.f,.36f},{20.2f,.5f,.3f},{21.2f,.3f,.26f},{19.5f,1.3f,.2f}})caTrunk(m,x,7.62f,f+lo,up+.06f,w);   // the run down off the edge
 for(auto [x1,x2]:{std::pair{17.6f,18.1f},{22.5f,22.75f}})caCrust(m,x1,6.7f,x2,6.78f,f,up);
 caMound(m,19.9f,7.45f,f,1.f,.55f);caKnot(m,19.4f,7.4f,f+.2f,.9f);caKnot(m,20.7f,7.9f,f+.15f,.7f);
 m.lights.push_back(caGlow({20.f,7.9f},f+.9f,.7f,5.f,6500,{.72f,.5f,1.f},.2f));
 caKnot(m,19.f,7.1f,up+.1f,.8f);
 // A4: what went through the front went through it hard. The leaf of the inner door is on the floor.
 wall(m,18.7f,7.7f,20.9f,8.8f,f,f+.07f,11);
 for(auto [x,lo]:{std::pair{17.62f,.6f},{22.55f,.7f},{22.8f,1.3f}})wall(m,x,6.7f,x+.05f,6.712f,f+lo,f+lo+1.1f,16);
 caTrail(m,{{20.2f,8.9f},{19.4f,11.6f},{17.4f,14.f},{14.2f,16.4f},{12.4f,18.4f}},f,96);
 m.lights.push_back(caAlarm({20.3f,6.78f},f+2.87f,clS,0));
 // A1: clean. A bed bolted down, a toilet, nothing else.
 caFix(m,model::med_ward_bed,2.05f,2.5f,f,0,true);caFix(m,model::med_ward_toilet,5.6f,1.4f,f,caS,true);
 // A2: he stands at the glass. Hands flat on it. He keeps his habits.
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{9.2f,6.05f},f,caS,0,0},{8000,{9.2f,6.05f},f,caS,0,4}},true);
 m.fixtures.push_back({model::med_mattress_stained,{8.6f,2.4f},0,1.9f,.9f,.118f,.15f,false});
 for(float x:{8.5f,9.f,9.8f})wall(m,x,6.46f,x+.18f,6.5f,f+1.f,f+2.2f,16);                    // where his hands go
 // A3: the ore on a plinth. It hums; the glass between is the only reason it is quiet out here.
 wall(m,14.25f,3.3f,15.25f,4.3f,f,f+.9f,15);
 caFix(m,model::lab_anomaly_relic,14.75f,3.8f,f+.9f,.6f,false);
 m.lights.push_back(caGlow({14.75f,3.8f},f+1.4f,.5f,4.5f,6500,{.72f,.5f,1.f},.3f));
 hazard(m,Hazard::Kind::Anomaly,12.3f,1.f,17.2f,6.4f,f,f+3.f,0);
 caHum(m,"cona_a3_hum",12.1f,6.7f,17.4f,9.5f,f,f+3.f,.5f);
 // A4: emptied. The bed broken, the floor scrubbed in a hurry, one smear missed.
 caFix(m,model::med_bed_frame_wrecked,20.2f,2.6f,f,.2f,true);
 wall(m,19.f,4.4f,21.6f,5.2f,f+.004f,f+.01f,16);
 m.lights.push_back(lamp({20.3f,1.f},f+2.45f,.7f,6.f,6500,.02f,WorldLightMount::Wall,clS));m.lights.back().flicker=.4f;
 m.lights.push_back(caAlarm({22.98f,3.8f},f+2.4f,clW,.6f));
 for(auto [x1,x2,z]:{std::tuple{18.4f,19.6f,1.2f},{19.f,20.4f,1.6f},{20.8f,21.8f,.9f}})wall(m,x1,1.f,x2,1.012f,f+z,f+z+.05f,16);   // the marks on the back wall
 taped(m,{17.85f,6.71f},f+1.5f,caS,Readable::Note,"A4 / VACATED 02:40","OCCUPANT REMOVED BY CLEANUP DETAIL A ON ORDER. NO TRANSFER RECORD TO BE KEPT.",
  "WHOEVER READS THIS: HE WALKED OUT. WE DID NOT CARRY HIM. HE WALKED.");
 // --- The south cells and the service door ------------------------------------------------------
 wall(m,1.f,17.3f,1.5f,17.5f,f,roof,15);wall(m,8.9f,17.3f,9.4f,17.5f,f,roof,15);caGlaze(m,1.5f,17.3f,8.9f,17.5f,f,roof,.6f,2.7f,true,15);
 wall(m,14.6f,17.3f,15.1f,17.5f,f,roof,15);wall(m,22.5f,17.3f,23.f,17.5f,f,roof,15);caGlaze(m,15.1f,17.3f,22.5f,17.5f,f,roof,.6f,2.7f,true,15);
 wall(m,9.4f,17.3f,9.6f,23.f,f,roof,37);wall(m,14.4f,17.3f,14.6f,23.f,f,roof,37);
 wall(m,9.6f,22.9f,14.4f,24.f,f+3.4f,roof,15);                                                  // the service ceiling starts here
 m.lights.push_back(lamp({14.4f,20.2f},f+2.4f,.5f,5.5f,4200,0,WorldLightMount::Wall,clW));
 m.fixtures.push_back({model::mine_cage_lamp,{14.3f,20.2f},f+2.2f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 for(float y:{19.5f,21.6f})caFix(m,model::med_ward_bed,2.05f,y,f,0,true);
 m.lights.push_back(lamp({5.f,23.f},f+2.9f,.6f,6.f,6500,.02f,WorldLightMount::Wall,clN));
 // S2: they sealed one of their own in. He asked them to.
 caBody(m,clock,ActorVisual::Cleanup,{19.f,20.4f},f,.6f);
 m.lights.push_back(lamp({19.f,23.f},f+2.9f,.45f,5.f,6500,.02f,WorldLightMount::Wall,clN));m.lights.back().flicker=.25f;
 taped(m,{18.8f,17.27f},f+1.5f,caN,Readable::Note,"S2 / SEALED BY DETAIL A, 03:10","PRIVATE VANCE. READ TIER 3 ON THE DOOR METER. ASKED TO BE PUT IN.",
  "WE TOLD HIM IT WAS FOR THE COUNT. HE SAID HE KNEW WHAT IT WAS FOR.");
 m.lights.push_back(caTube(12.f,20.5f,roof,.7f,6.5f,4600,.02f,.2f));
 // --- The observation floor: the monitoring island, the cabinets ---------------------------------
 wall(m,8.f,10.4f,16.f,11.1f,f,f+.88f,20);wall(m,7.95f,10.35f,16.05f,11.15f,f+.88f,f+.92f,28);
 for(float x:{9.f,11.2f,13.4f,15.2f})kit::prop(m,model::crt_monitor,x,10.75f,f+.92f,.42f,.4f,.38f,0);
 for(auto [x,yaw]:{std::pair{9.f,.15f},{13.4f,-.3f}})kit::prop(m,model::office_chair,x,11.8f,f,.55f,.55f,.95f,yaw);
 terminal(m,{12.3f,10.7f},"CONTAINMENT A / CELLS","A1 CLEAR. A2 OCCUPIED, ACTIVE. NOT FED SINCE 00:00. STANDS AT THE GLASS. A3 SAMPLE R-12, FIELD STABLE.",
  "A4 VACATED 02:40. S2 SEALED 03:10. BATTERY 31%: AT ZERO THE GLASS LOCKS STAY SHUT. THEY DO NOT FAIL OPEN.",.92f);
 m.terminals.back().yaw=0;
 m.lights.push_back(caGlow({12.f,11.7f},f+1.5f,.3f,3.f,9000,{.6f,.8f,1.f},.06f));
 caFix(m,model::lab_specimen_cabinet,1.4f,16.f,f,caE,true);wall(m,1.f,15.3f,1.8f,16.7f,f+2.19f,roof,CollisionProxyMaterial);
 for(float y:{8.9f,10.f})caFix(m,model::med_records_cabinet,22.73f,y,f,caW,true);
 caFix(m,model::lab_sample_fridge,22.55f,15.6f,f,caW,true);
 // --- Detail A: two men, short of rounds, still on their orders ----------------------------------
 caGuard(m,clock,"cona_a_walk",ActorVisual::Cleanup,GuardSquadRole::Assault,"cona_a",
  {{{3.f,13.f},f,3000,caE},{{20.5f,13.f},f,4000,caN},{{20.5f,8.4f},f,3000,caW},{{3.f,8.4f},f,3000,caS}},12,1,1.1f);
 const int post=int(m.actorTracks.size());
 caGuard(m,clock,"cona_a_post",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"cona_a",{{{19.6f,15.4f},f,60000,caW}},14,1,1.f);
 // his post is under the one tube on this side that still works, unsteady: what happens to him is seen
 m.lights.push_back(caTube(19.2f,15.4f,roof,.85f,6.5f,4300,.12f,.2f));
 m.pickupSpawns.push_back({{16.6f,12.4f},PickupKind::Ammo,f});
 // --- R-117: what came out of A8. It went into the walls, down the service run behind the cells,
 // and it is heard before it is seen: a bang in the run as the player steps onto the floor, its
 // breath, a growl at the service door. Then it comes out of the door across the floor for the man at the post,
 // takes his head and stays down on him: the window to cross the floor to the service door, or to
 // watch. It is Chapter 2's one recurring animal (hunter r117): it cannot be killed here, it runs
 // when badly hurt, and it comes back (Specimen Handling, the theater, the barricade, the core).
 {CreatureSpawn r{CreatureKind::Chimera,{12.f,21.6f},f};r.wakeState=stateId("cona_r117_out");r.hunter=stateId("r117");r.prey=post;m.creatureSpawns.push_back(r);}
 caSequence(m,"cona_r117","cona_r117_out",8000,3800,2.6f,8.f,7.f,16.6f);
 cue(m,"cona_r117",0,Sound::OneMetalBang,{12.f,22.6f},"",1.f,f+2.6f);
 cue(m,"cona_r117",1500,Sound::OneMonsterBreath,{12.f,21.f},"",.85f,f+.9f);
 cue(m,"cona_r117",3000,Sound::OneMonsterGrowl,{12.f,18.6f},"",1.f,f+.8f);
 taped(m,{22.99f,12.3f},f+1.5f,caW,Readable::Letter,"A8 / R-117 / HOLDING LOG","R-117. FOUR LEGS. TAKEN AT THE 900 LEVEL ON THE 2ND. HELD IN A8. FED TWICE DAILY, LIVE. WILL NOT TAKE IT DEAD.",
  "02:00 A8 FRONT OUT FROM INSIDE. 02:35 HEARD IN THE SERVICE RUN BEHIND A4. A4 CLEARED. DETAIL A TO HOLD THE FLOOR. NOT TO FIRE FIRST.");
 taped(m,{22.99f,14.3f},f+1.45f,caW,Readable::Note,"(PENCIL, ON THE BACK OF A DUTY ROSTER)","IT WAITS TILL ONE OF US IS ON HIS OWN. WHEN IT HAS ONE IT STAYS DOWN ON HIM A WHILE.",
  "THAT IS YOUR TIME. YOU WALK. YOU DON'T LOOK AT IT AND YOU DON'T SHOOT IT. IT GETS UP.");
 // --- Light: the cells bright, the floor dimmer, one fitting dead ---------------------------------
 for(auto [x,y,fl]:{std::tuple{5.f,8.8f,0.f},{19.f,8.8f,0.f},{5.f,14.6f,.2f},{12.f,14.6f,0.f},{19.f,14.6f,0.f}})m.lights.push_back(caTube(x,y,roof,.5f,6.f,5000,.03f,fl));
 caDeadTube(m,12.f,8.8f,roof);
 recessedFittings(m);
 zone(m,"",12.1f,1.f,17.4f,6.5f,-1000,1000,{.15f,.14f,.18f,.01f});
 zone(m,"CONTAINMENT A / CELLS",1.f,1.f,23.f,6.5f,-1000,1000,{.12f,.14f,.17f,.008f});
 zone(m,"CONTAINMENT A / SOUTH CELLS",1.f,17.5f,23.f,23.f,-1000,1000,{.12f,.14f,.17f,.008f});
 zone(m,"CONTAINMENT A",0,0,24,24,-1000,1000,{.12f,.14f,.17f,.010f});
 freightFinish(m,-1);
 return p;
}

// CONTAINMENT SERVICE (manifest 63). The corridors behind the cell walls: 3.4 m, pipes, valves,
// cages over the lamps. A bulkhead across the main run is held on the containment feed, the same
// as door B, so before power this is a dead end with a view: the slots on the east spur look into a
// cell the growth took. With power the bulkhead opens, the west spur reaches B, the south run the
// Observation Theater. A Cleanup man came back here to be on his own.
// Route: N seam from A -> the bulkhead [cont_power] -> W to B or S to the theater.
std::shared_ptr<const Map> makeContainmentService(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[97];m.definition=CampaignChunks[97];m.definition.ambient=.23f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'W',15.f,19.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"cona_svc_clock",60000);const char* clock="cona_svc_clock";
 // --- The main run: 2.8 m between block walls, end to end ----------------------------------------
 // North of the bulkhead the run's west wall is gone: the run and the valve room are one junction
 // round the manifold, its old wall line kept only as the beam overhead.
 // The wall stub at the door is gone too: you come in from A straight onto the manifold.
 wall(m,10.4f,0,10.6f,1.2f,f,roof,15);wall(m,10.4f,10.f,10.6f,15.2f,f,roof,37);
 wall(m,10.3f,9.8f,10.7f,10.2f,f,roof,15);                                                         // the pilaster it ended on
 wall(m,10.38f,1.2f,10.62f,9.8f,f+2.95f,roof,12);
 wall(m,10.4f,15.2f,10.6f,18.8f,f+2.6f,roof,15);wall(m,10.4f,18.8f,10.6f,24.f,f,roof,37);
 wall(m,13.4f,0,13.6f,6.6f,f,roof,37);doorwayY(m,6.6f,8.4f,13.4f,13.6f,f,roof);wall(m,13.4f,8.4f,13.6f,14.5f,f,roof,37);
 doorwayY(m,14.5f,15.9f,13.4f,13.6f,f,roof);wall(m,13.4f,15.9f,13.6f,24.f,f,roof,37);
 // the bulkhead: held on the feed
 {const int d=wallDoor(m,10.6f,13.4f,10.9f,11.1f,11.4f,12.6f,f,roof,1);m.doors[size_t(d)].requireState=stateId("cont_power");}
 wallSign(m,12.f,10.88f,f+2.62f,"BULKHEAD HELD","OPENS ON CONTAINMENT FEED",1.5f,caN);
 m.lights.push_back(caBeacon({10.9f,10.9f},f+2.2f,clN,0,"cont_power"));
 m.lights.push_back(caBeacon({13.1f,11.1f},f+2.2f,clS,.6f,"cont_power"));
 floorLine(m,10.6f,10.4f,13.4f,10.55f,f,13);
 m.lights.push_back(lamp({13.4f,2.f},f+2.4f,.5f,5.5f,3000,0,WorldLightMount::Wall,clW));
 for(float y:{13.f,20.f})m.lights.push_back(lamp({10.6f,y},f+2.4f,.5f,5.5f,3000,0,WorldLightMount::Wall,clE));
 m.lights.back().flicker=.35f;
 // --- The valve room (west, before the bulkhead): where he came to be on his own ----------------
 wall(m,1.f,10.f,10.4f,10.2f,f,roof,37);
 caFix(m,model::pwr_tall_switch_cabinet,1.5f,6.f,f,0,true);wall(m,1.f,5.2f,2.f,6.8f,f+3.02f,roof,CollisionProxyMaterial);
 for(auto [x,y]:{std::pair{1.6f,9.35f},{2.25f,9.45f}})caFix(m,model::ind_barrel,x,y,f,x,true);
 // The manifold: two headers across the junction on floor shoes, three risers into the roof, a gate
 // valve on every branch, and the east gasket weeping steam onto the floor since the feed dropped.
 const float hy=5.2f;
 caFix(m,model::pwr_circulation_pump,3.7f,hy,f,0,true);
 pipeRoute(m,{{4.12f,hy,f+.25f},{4.4f,hy,f+.25f},{4.4f,hy,f+.9f}},.1f,3,roof,f);
 for(float z:{f+.9f,f+1.7f})pipeRoute(m,{{4.4f,hy,z},{8.9f,hy,z}},.13f,3,roof,f);
 for(float x:{5.2f,6.8f,8.4f}){m.pipes.push_back({{x,hy},{x,hy},f+.9f,.09f,f+1.7f,3});
  m.pipes.push_back({{x,hy},{x,hy},f+1.7f,.1f,roof,3});flange(m,{x,hy,f+1.9f},{},.1f);
  gateValve(m,{x+.4f,hy,f+.9f},{1,0},.13f,f,1);
  caFix(m,model::lab_pressure_gauge,x,hy+.16f,f+1.95f,caS,false);}
 // The junction is the plant: a second, older header bank on the north wall feeding a receiver,
 // a rack of four mains overhead from wall to wall, and the lagged return dropping into the floor,
 // so every way you look from the door is pipe, valve and gauge, not block wall.
 vessel(m,{1.95f,3.1f},.62f,f,roof-.12f,4);
 caFix(m,model::lab_pressure_gauge,1.95f,3.78f,f+1.6f,caS,false);
 for(auto [z,r,paint]:{std::tuple{f+.55f,.16f,4},{f+1.35f,.11f,1}})pipeRoute(m,{{2.6f,2.1f,z},{9.6f,2.1f,z},{9.6f,2.1f,roof-.05f}},r,paint,roof,f);
 for(float x:{4.2f,7.1f}){gateValve(m,{x,2.1f,f+.55f},{1,0},.16f,f,1);caFix(m,model::lab_pressure_gauge,x+.5f,2.36f,f+1.62f,caS,false);}
 wall(m,2.5f,1.6f,10.f,2.6f,f,f+1.6f,CollisionProxyMaterial);
 for(auto [y,r,paint]:{std::tuple{3.45f,.12f,3},{3.85f,.09f,2},{7.15f,.13f,4},{7.55f,.08f,1}})servicePipe(m,{1.f,y},{13.4f,y},roof-.32f,r,paint);
 m.pipes.push_back({{13.1f,5.2f},{13.1f,5.2f},f,.2f,roof,1});flange(m,{13.1f,5.2f,f+.1f},{},.2f);flange(m,{13.1f,5.2f,roof-.15f},{},.2f);
 wall(m,12.85f,4.95f,13.35f,5.45f,f,roof,CollisionProxyMaterial);
 m.emitters.push_back({{6.5f,2.35f},f+.6f,{.1f,.7f},.45f,.7f,.05f,.4f,7});                           // the old bank weeps too
 wall(m,4.3f,hy-.32f,9.05f,hy+.72f,f,roof,CollisionProxyMaterial);
 m.emitters.push_back({{8.96f,hy+.1f},f+1.7f,{.25f,.9f},.55f,.85f,.06f,.45f,9});                      // the gasket
 m.emitters.push_back({{8.96f,hy+.1f},f+1.75f,{.4f,.5f},.9f,.6f,.05f,.35f,6});
 {Hazard h;h.kind=Hazard::Kind::Steam;h.x1=8.7f;h.x2=9.7f;h.y1=hy+.7f;h.y2=hy+1.9f;h.bottom=f-.5f;h.top=f+2.4f;
  h.damagePerSecond=12;h.period=6;h.onTime=1.8f;m.hazards.push_back(h);}
 wall(m,7.9f,hy+.75f,10.2f,hy+2.4f,f+.002f,f+.006f,17);caFix(m,model::med_floor_drain,9.4f,hy+1.9f,f,0,false);
 caPendant(m,{6.8f,hy+1.2f},roof,f+2.75f,1.7f,9.f,2700);caPendant(m,{5.2f,3.f},roof,f+2.8f,.9f,7.f,3000,.1f);
 caBody(m,clock,ActorVisual::Cleanup,{6.5f,8.f},f,2.f);
 kit::prop(m,model::item_walkie,7.2f,8.4f,f,.114f,.376f,.046f,1.1f);
 taped(m,{1.01f,8.2f},f+1.5f,caE,Readable::Note,"(PENCIL, ON A RATION CARD)","TOLD THE SARGE I WAS CHECKING THE VALVES. I'M NOT GOING BACK OUT THERE TO DO WHAT HE SAYS.",
  "IF SOMEONE FINDS THIS, THE ROUNDS ARE IN THE BOX. I DIDN'T USE THEM ON ANYONE.");
 m.lights.push_back(lamp({5.f,10.f},f+2.5f,.5f,5.5f,3000,0,WorldLightMount::Wall,clN));
 m.pickupSpawns.push_back({{8.8f,8.6f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{2.6f,8.8f},PickupKind::Health,f});
 // --- The slot spur (east): three slots into the cell the growth took ----------------------------
 // The slots are the reason anybody came down this spur: three wired panes two metres wide at eye
 // height, each in a steel surround, and the spur kept dim so the cell behind them is the light.
 wall(m,13.6f,6.4f,14.6f,6.6f,f,roof,15);
 for(float x:{14.6f,17.4f,20.2f}){caGlaze(m,x,6.4f,x+2.f,6.6f,f,roof,1.f,2.f,true,15);
  wall(m,x-.08f,6.6f,x+2.08f,6.66f,f+.92f,f+1.f,11);wall(m,x-.08f,6.6f,x+2.08f,6.66f,f+2.f,f+2.08f,11);
  for(float e:{x-.08f,x+2.f})wall(m,e,6.6f,e+.08f,6.66f,f+1.f,f+2.f,11);
  if(x<20.f)wall(m,x+2.f,6.4f,x+2.8f,6.6f,f,roof,15);}
 wall(m,22.2f,6.4f,23.f,6.6f,f,roof,15);wall(m,13.6f,8.4f,23.f,8.6f,f,roof,15);
 m.lights.push_back(lamp({23.f,7.5f},f+2.2f,.32f,4.5f,2700,0,WorldLightMount::Wall,clW));
 m.lights.push_back(lamp({14.3f,8.4f},f+2.3f,.3f,4.f,2700,0,WorldLightMount::Wall,clN));
 for(auto [x,y]:{std::pair{22.9f,7.5f},{14.3f,8.3f}})m.fixtures.push_back({model::mine_cage_lamp,{x,y},f+2.05f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 for(auto [y,r,paint]:{std::tuple{8.1f,.09f,3},{7.85f,.06f,1}})servicePipe(m,{13.6f,y},{23.f,y},roof-.3f,r,paint);
 // The cell: crust over everything, trunks of it up to the roof, knots of crystal lit from inside,
 // what is left of the bed, and a suit hung from the lamp hook with the light behind it.
 caCrust(m,13.6f,1.f,23.f,6.4f,f,f+.06f);caCrust(m,13.6f,1.f,23.f,1.25f,f,roof);caCrust(m,22.75f,1.f,23.f,6.4f,f,roof);
 caMound(m,15.6f,3.f,f,1.f,.9f);caMound(m,20.8f,2.4f,f,1.2f,1.3f);caMound(m,18.2f,5.3f,f,.7f,.5f);
 for(auto [x,y,h]:{std::tuple{16.4f,4.4f,.8f},{21.6f,4.f,1.1f},{14.6f,1.8f,.6f},{19.3f,2.f,1.5f}})caKnot(m,x,y,f+.06f,h);
 for(auto [x,y,w]:{std::tuple{14.3f,4.6f,.42f},{17.5f,1.6f,.5f},{22.3f,5.5f,.36f}})caTrunk(m,x,y,f,roof,w);
 caKnot(m,20.1f,4.9f,f+.06f,.9f);                                                                       // (no black rock here: through the glass it read as a hole)
 caFix(m,model::med_bed_frame_wrecked,15.3f,5.3f,f,.5f,false);
 hungSuit(m,{18.4f,3.9f,f+2.3f},caS);
 m.lights.push_back(caGlow({18.4f,2.5f},f+1.7f,1.7f,9.f,6500,{.72f,.5f,1.f},.2f));
 m.lights.push_back(caGlow({21.6f,4.2f},f+1.45f,.8f,5.f,6500,{.3f,.6f,1.f}));
 m.lights.push_back(caGlow({15.2f,3.9f},f+1.3f,.8f,5.f,6500,{.6f,.45f,1.f},.3f));
 hazard(m,Hazard::Kind::Anomaly,13.7f,1.f,22.9f,6.3f,f,f+2.6f,6);
 caHum(m,"cona_svc_hum",13.6f,6.6f,23.f,8.4f,f,f+3.f,.55f);
 // --- The plenum (east, past the bulkhead): the air handlers, and the tech who stayed with them --
 wall(m,13.6f,11.8f,23.f,12.f,f,roof,37);wall(m,13.6f,20.f,23.f,20.2f,f,roof,37);
 for(float y:{14.f,17.6f})caFix(m,model::pwr_air_handler,17.f,y,f,0,true);
 wall(m,17.4f,13.3f,23.f,14.7f,f+2.7f,f+3.2f,2);wall(m,17.4f,16.9f,23.f,18.3f,f+2.7f,f+3.2f,2);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{21.f,16.f},f,1,0});
 m.lights.push_back(lamp({23.f,18.8f},f+2.2f,.3f,5.f,1000,-.45f,WorldLightMount::Wall,clW));m.lights.back().flicker=.4f;
 // The plenum is plant like the junction is: the supply and return along its walls, the handlers'
 // starter cabinets by the door, a caged lamp each end, so the tech's room is a room and not a void.
 for(auto [y,r,paint]:{std::tuple{12.35f,.1f,3},{19.65f,.08f,4}})servicePipe(m,{13.6f,y},{23.f,y},roof-.32f,r,paint);
 for(float y:{14.f,17.6f}){m.pipes.push_back({{17.f,y},{17.f,y},f+2.62f,.12f,roof,2});flange(m,{17.f,y,roof-.15f},{},.12f);}
 for(float y:{18.3f,19.3f})caFix(m,model::pwr_control_cabinet,13.82f,y,f,caE,true);
 caFix(m,model::pwr_disconnect_switch,13.78f,17.3f,f+1.f,caE,false);
 caFix(m,model::pwr_vent_grille,20.f,12.03f,f+1.9f,caS,false);caFix(m,model::pwr_vent_grille,20.f,19.97f,f+1.9f,caN,false);
 for(auto [x,y]:{std::pair{22.5f,12.6f},{22.4f,19.4f}})caFix(m,model::ind_barrel,x,y,f,x,true);
 m.lights.push_back(lamp({19.f,12.f},f+2.4f,.5f,6.f,3000,0,WorldLightMount::Wall,clS));
 m.lights.push_back(lamp({15.2f,20.f},f+2.4f,.42f,5.5f,3000,0,WorldLightMount::Wall,clN));m.lights.back().flicker=.3f;
 for(auto [x,y]:{std::pair{19.f,12.1f},{15.2f,19.9f}})m.fixtures.push_back({model::mine_cage_lamp,{x,y},f+2.15f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 // --- The south run to the theater: the junction's mains carried on down it ----------------------
 for(auto [x,r,paint]:{std::tuple{10.95f,.11f,3},{11.22f,.07f,1},{13.05f,.09f,4}})servicePipe(m,{x,11.05f},{x,24.f},roof-.3f,r,paint);
 m.pipes.push_back({{10.82f,22.2f},{10.82f,22.2f},f,.07f,roof-.3f,3});flange(m,{10.82f,22.2f,f+.06f},{},.07f);
 wall(m,10.6f,22.f,11.05f,22.4f,f,roof,CollisionProxyMaterial);
 caFix(m,model::pwr_gate_valve,10.86f,22.2f,f+1.05f,caE,false);caFix(m,model::lab_pressure_gauge,10.66f,21.6f,f+1.5f,caE,false);
 cableTray(m,{13.18f,16.1f},{13.18f,23.9f},roof-.62f);
 // the run as the fitters left it: a ladder against the wall, a drum, a box of fittings, the line of cold tubes down the middle
 caFix(m,model::pwr_wall_ladder,13.36f,17.4f,f,caW,false);caFix(m,model::scrap_drum,11.f,16.9f,f,.4f,true);
 kit::prop(m,model::prop_cardboard_box_open,12.9f,13.4f,f,.5f);kit::prop(m,model::mine_shovel,13.16f,14.f,f,caW);
 for(float y:{13.6f,17.6f,21.6f})m.lights.push_back(caTube(12.f,y,roof,.42f,5.5f,5600,.04f,y>20?.25f:0.f));
 for(auto [x,y]:{std::pair{5.f,17.f},{18.f,16.f}})m.lights.push_back(caTube(x,y,roof,.4f,5.5f,5600,.04f,.1f));
 m.lights.push_back(lamp({13.4f,22.f},f+2.4f,.45f,5.f,3000,0,WorldLightMount::Wall,clW));
 for(auto [x,y]:{std::pair{10.72f,13.f},{10.72f,20.f},{13.28f,22.f}})m.fixtures.push_back({model::mine_cage_lamp,{x,y},f+2.15f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 for(auto [x,y]:{std::pair{11.f,19.5f},{11.05f,20.6f}})caFix(m,model::ind_barrel,x,y,f,x+y,true);
 caFix(m,model::pwr_fire_cabinet,13.3f,12.6f,f+1.1f,caW,false);
 // --- The west spur to B: the growth comes the other way to meet you ---------------------------
 wall(m,1.f,15.f,10.4f,15.2f,f,roof,37);wall(m,1.f,18.8f,10.4f,19.f,f,roof,37);
 // what the spur was used for until two: the fitters' stores against its walls
 for(auto [x,y,yaw]:{std::tuple{9.7f,18.3f,.1f},{8.9f,18.35f,-.2f},{9.75f,17.45f,.5f}})caFix(m,model::ind_crate,x,y,f,yaw,true);
 caFix(m,model::cable_reel,6.9f,18.1f,f,caE,true,.55f);caFix(m,model::pwr_sheet_steel_stack,4.2f,18.45f,f,caE,false,.5f);
 kit::prop(m,model::prop_cardboard_box,7.7f,17.5f,f,.4f);kit::prop(m,model::mine_hard_hat,9.7f,17.45f,f+.81f,1.2f);
 crewBarrier(m,5.f,16.f,f,1.4f);
 taped(m,{5.f,16.07f},f+.75f,caS,Readable::Note,"B SPUR / CLOSED","NOTHING IN OR OUT OF B THIS WAY. ORDER OF DETAIL A 02:20.","");
 for(auto [x1,x2,y1,y2]:{std::tuple{1.f,2.6f,15.2f,18.8f},{2.6f,3.4f,16.1f,18.2f}})caCrust(m,x1,y1,x2,y2,f,f+.04f);
 caCrust(m,1.f,15.2f,3.2f,15.32f,f,f+2.f);caCrust(m,1.f,18.68f,2.6f,18.8f,f,f+1.4f);
 m.lights.push_back(caGlow({1.8f,17.f},f+.35f,.3f,3.f,6500,{.72f,.5f,1.f}));
 m.lights.push_back(lamp({8.f,18.8f},f+2.4f,.4f,5.f,3000,0,WorldLightMount::Wall,clN));m.lights.back().flicker=.5f;
 for(auto [y,r,paint]:{std::tuple{15.52f,.1f,3},{15.8f,.06f,1}})servicePipe(m,{0.f,y},{10.4f,y},roof-.3f,r,paint);
 m.pipes.push_back({{7.2f,15.4f},{7.2f,15.4f},f,.07f,roof-.3f,3});flange(m,{7.2f,15.4f,f+.06f},{},.07f);
 wall(m,7.f,15.2f,7.4f,15.65f,f,roof,CollisionProxyMaterial);
 caFix(m,model::pwr_gate_valve,7.2f,15.45f,f+1.05f,caS,false);
 m.lights.push_back(lamp({4.6f,15.2f},f+2.4f,.42f,5.f,3000,0,WorldLightMount::Wall,clS));
 for(auto [x,y]:{std::pair{4.6f,15.3f},{8.f,18.7f}})m.fixtures.push_back({model::mine_cage_lamp,{x,y},f+2.15f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 caMound(m,2.1f,17.7f,f,.8f,.7f);caTrunk(m,1.25f,15.45f,f,roof,.4f);caLens(m,2.6f,16.8f,f,3.4f,2.8f,.08f);
 recessedFittings(m);
 zone(m,"CONTAINMENT SERVICE / VALVE JUNCTION",1.f,1.f,13.4f,10.9f,-1000,1000,{.16f,.15f,.13f,.01f});
 zone(m,"CONTAINMENT SERVICE / SLOTS",13.6f,6.6f,23.f,8.4f,-1000,1000,{.14f,.13f,.15f,.008f});
 zone(m,"",13.6f,1.f,23.f,6.4f,-1000,1000,{.17f,.13f,.2f,.012f});
 zone(m,"CONTAINMENT SERVICE / PLENUM",13.6f,12.f,23.f,20.f,-1000,1000,{.16f,.1f,.09f,.016f});
 zone(m,"CONTAINMENT SERVICE / B SPUR",0,15.2f,10.4f,18.8f,-1000,1000,{.15f,.12f,.16f,.018f});
 zone(m,"CONTAINMENT SERVICE",0,0,24,24,-1000,1000,{.12f,.12f,.11f,.014f});
 freightFinish(m,-1);
 return p;
}

// CONTAINMENT B (manifest 64). Breached. Every cell let go at 02:00, the same second; the glass is
// on the aisle floor, the bars of W2 are bent out, and the growth comes up the west passage from
// the Breach Sector across the aisle. Kessler's detail held the inner door and lost. Two are left
// in the cell control booth. Over their radio the Investigator writes them off to the Intake detail.
// Route: N door from the hub (on cont_power) -> the aisle -> W to the Breach Sector, S to Specimen
// Handling, E back into the service spur.
std::shared_ptr<const Map> makeContainmentB(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[98];m.definition=CampaignChunks[98];m.definition.ambient=.2f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceLabPanel;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});                                                         // door 0: in from the hub
 openEdge(m,{'E',15.f,19.f,f,false,0});
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"cona_b_clock",60000);const char* clock="cona_b_clock";
 const StateId stance=stateId("cona_b_stance");
 // --- The vestibule: the suits they did not get to, the inner door blown in ---------------------
 wall(m,9.5f,3.9f,10.4f,4.1f,f,roof,15);wall(m,13.6f,3.9f,14.5f,4.1f,f,roof,15);wall(m,10.4f,3.9f,13.6f,4.1f,f+3.f,roof,15);
 wall(m,10.7f,5.2f,13.3f,6.5f,f,f+.1f,2);                                                         // the leaf, on the floor
 wall(m,9.5f,1.5f,9.95f,3.1f,f+1.9f,f+1.94f,20);
 for(float y:{1.8f,2.6f})hungSuit(m,{9.8f,y,f+1.5f},caE);
 caBody(m,clock,ActorVisual::Cleanup,{11.f,7.f},f,.4f);caBody(m,clock,ActorVisual::CleanupRusher,{13.f,8.4f},f,2.6f);
 kit::prop(m,model::item_walkie,12.2f,7.4f,f,.114f,.376f,.046f,2.2f);
 // --- West of the aisle: W1 blown, the passage the growth came up, W2's bars bent out, W3 sealed ---
 wall(m,9.3f,1.f,9.5f,4.2f,f,roof,15);caBlownFront(m,9.3f,4.2f,9.5f,8.4f,f,roof,1,11);wall(m,9.3f,8.4f,9.5f,9.f,f,roof,15);
 wall(m,9.3f,9.f,9.5f,14.f,f+3.4f,roof,15);
 wall(m,9.3f,14.f,9.5f,15.15f,f,roof,15);wall(m,9.3f,17.65f,9.5f,19.4f,f,roof,15);wall(m,9.3f,15.15f,9.5f,17.65f,f+2.4f,roof,15);
 caFix(m,model::med_cell_bars_doorway,9.4f,16.4f,f,caE,false);
 wall(m,9.37f,15.15f,9.43f,15.82f,f,f+2.4f,CollisionProxyMaterial);wall(m,9.37f,16.98f,9.43f,17.65f,f,f+2.4f,CollisionProxyMaterial);
 caGlaze(m,9.3f,19.4f,9.5f,22.4f,f,roof,.6f,2.7f,true,15);wall(m,9.3f,22.4f,9.5f,23.f,f,roof,15);
 wall(m,1.f,8.8f,9.3f,9.f,f,roof,15);wall(m,1.f,14.f,9.3f,14.2f,f,roof,15);wall(m,1.f,18.6f,9.3f,18.8f,f,roof,15);
 // W1 is where it opened in this block: a spire of the crystal out of a mound of growth, lit from
 // inside, the one thing in B you see from the vestibule through the blown front.
 caCrust(m,1.f,1.f,1.25f,8.8f,f,f+3.5f);caMound(m,3.f,6.6f,f,.9f,.7f);caFix(m,model::med_bed_frame_wrecked,6.7f,2.f,f,.4f,true);
 caMound(m,3.4f,3.6f,f,1.6f,1.2f);
 // the spire: the leaders lit through, the rest fanned out of the same root, shorter the further they lean
 // (modelled clusters: the big knot scaled up out of the mound, a middle one leaning off it)
 caCrystal(m,model::res_crystal_l,3.4f,3.6f,f+.75f,1.6f,.8f);
 caCrystal(m,model::res_crystal_m,3.f,3.9f,f+.6f,1.25f,3.4f);
 for(auto [x,y,sc,yaw]:{std::tuple{2.5f,3.9f,1.2f,.4f},{4.4f,3.3f,1.1f,1.7f},{3.5f,4.7f,1.f,2.9f}})caBed(m,x,y,f+.7f,sc,yaw);
 caFang(m,2.1f,2.5f,f,.6f,.9f);caFang(m,5.f,4.6f,f,.48f,3.1f);
 wall(m,3.f,3.2f,3.8f,4.f,f,f+4.6f,CollisionProxyMaterial);
 m.lights.push_back(caGlow({4.2f,4.6f},f+2.2f,1.2f,10.f,6500,{.72f,.5f,1.f},.18f));
 m.lights.push_back(caGlow({3.4f,3.6f},f+5.2f,.7f,8.f,6500,{.6f,.45f,1.f},.1f));
 hazard(m,Hazard::Kind::Anomaly,1.6f,1.8f,5.2f,5.4f,f,f+3.f,10);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{5.6f,6.2f},f,1,0});
 // The west passage to the Breach Sector is grown shut but for one lane along its south wall, lit
 // by the lamps Kessler's men set on the floor; its rim shimmers and bites. The growth comes on out of
 // the passage and closes the aisle wall to wall, to the roof: the normal way south is gone. The way
 // on is the detour Kessler's men lamped: over the heave into E1, through the hole the growth
 // punched in E1's back wall, through the detail's booth and out by its door to the south passage.
 caCrust(m,1.f,9.f,9.3f,14.f,f,f+.04f);caCrust(m,1.f,9.f,9.3f,9.25f,f,roof);caCrust(m,1.f,13.75f,6.f,14.f,f,f+3.f);
 // The mass is one body, grown not built: lobes swollen against each other from the passage mouth
 // across the aisle, trunks of it into the roof, pods on its shoulders, the black rock it carried up
 // standing through it and the crystal seated in its folds. It collides as the wall it is.
 for(auto [x,y,w,d,h]:{std::tuple{6.f,10.8f,7.f,3.2f,2.9f},{3.9f,10.6f,3.f,2.7f,2.1f},{8.3f,10.9f,3.f,2.9f,3.4f},{5.6f,11.6f,3.6f,1.6f,1.5f}})caGrowth(m,x,y,f,w,d,h);
 for(auto [x,y,w,d,h]:{std::tuple{12.1f,11.25f,3.6f,2.6f,5.7f},{10.5f,11.2f,3.f,2.8f,4.3f},{13.5f,11.3f,2.9f,2.6f,3.8f},{11.5f,10.3f,2.4f,1.3f,1.7f},{12.9f,12.2f,2.6f,1.2f,1.5f},{9.7f,11.9f,1.6f,1.4f,2.4f}})caGrowth(m,x,y,f,w,d,h,false,1.f);
 for(auto [x,y,lo,w]:{std::tuple{11.1f,11.2f,3.6f,.6f},{13.f,11.4f,3.f,.5f},{12.1f,10.9f,5.2f,.75f},{7.9f,10.6f,2.9f,.5f},{4.8f,10.2f,1.8f,.4f}})caTrunk(m,x,y,f+lo,roof,w);
 caFang(m,10.9f,10.2f,f+.4f,.8f,.6f);caFang(m,13.4f,12.2f,f+.3f,.62f,2.2f);caFang(m,6.4f,12.f,f+.2f,.55f,1.3f);
 wall(m,9.5f,10.f,14.5f,12.5f,f,roof,CollisionProxyMaterial);wall(m,2.6f,9.25f,9.5f,12.4f,f,roof,CollisionProxyMaterial);
 for(auto [x,y,z,h]:{std::tuple{4.6f,10.4f,2.f,1.f},{7.8f,10.6f,2.9f,.8f},{3.6f,11.6f,1.3f,.7f},{11.2f,11.3f,5.1f,1.2f},{12.9f,11.f,4.5f,.9f},{8.8f,12.1f,1.3f,.6f},
  {13.6f,9.9f,0.f,1.4f},{10.2f,9.9f,0.f,1.1f},{12.f,12.65f,0.f,.9f},{14.1f,12.2f,1.2f,.8f}})caKnot(m,x,y,f+z,h);
 caKnot(m,1.6f,10.2f,f+.04f,.9f);
 hazard(m,Hazard::Kind::Anomaly,2.6f,12.25f,9.5f,12.6f,f,f+2.4f,6);hazard(m,Hazard::Kind::Anomaly,9.5f,9.75f,14.5f,12.75f,f,f+2.4f,5);
 caHum(m,"cona_b_hum",1.f,9.f,9.5f,14.f,f,f+3.f,.6f);
 m.lights.push_back(caGlow({1.8f,11.f},f+2.2f,.45f,4.5f,6500,{.72f,.5f,1.f},.3f));
 m.lights.push_back(caGlow({12.f,9.6f},f+1.2f,.8f,6.f,6500,{.72f,.5f,1.f},.15f));                       // the wall's face, lit from its own knots
 m.lights.push_back(caGlow({12.f,9.3f},f+4.6f,.6f,6.f,6500,{.6f,.45f,1.f},.1f));
 for(auto [x,y,yaw]:{std::tuple{8.6f,13.45f,.3f},{5.6f,13.1f,2.1f},{3.f,13.4f,-.7f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f+.04f,.18f,.4f,.52f,yaw);
  auto l=caGlow({x,y},f+.5f,.42f,3.8f,3000);l.prop=lampProp;m.lights.push_back(l);}
 // E1's blown front is half grown over: a heave of growth you can climb, over the sill, into E1.
 // (A tongue of it lies over the sill: up one side, down the other, no jump. The detail's lamp is at its foot.)
 caPrim(m,"ramp",13.66f,5.5f,f,1.9f,1.68f,.92f,1,26);wall(m,14.5f,4.55f,14.7f,6.45f,f+.9f,f+.92f,26);caPrim(m,"ramp",15.4f,5.5f,f,1.9f,1.4f,.92f,3,26);
 for(auto [y,w]:{std::pair{5.1f,.05f},{5.9f,.035f}}){caPrim(m,"strut",13.66f,y,f+.004f,w,1.68f,.92f,1,34,false);caPrim(m,"strut",15.4f,y,f+.004f,w,1.4f,.92f,3,34,false);}
 caGrowth(m,13.5f,4.3f,f,1.5f,1.f,.8f);caGrowth(m,15.5f,6.7f,f,1.4f,1.f,.7f);
 caKnot(m,13.f,4.2f,f,.5f);
 // W2: bars bent out from inside, and the growth came out after him: the doorway is grown shut.
 caGrowth(m,9.42f,16.4f,f,1.1f,2.5f,2.5f);caGrowth(m,9.7f,16.f,f,.9f,1.3f,1.2f);
 caModel(m,model::cave_nest_pod,9.75f,16.9f,f+.9f,.5f,.5f,.4f,.7f);
 wall(m,9.24f,15.15f,9.58f,17.65f,f,f+2.4f,CollisionProxyMaterial);
 caMound(m,4.8f,16.4f,f,1.2f,1.f);caKnot(m,6.6f,15.4f,f,.9f);
 hazard(m,Hazard::Kind::Anomaly,3.4f,15.f,6.2f,17.8f,f,f+2.f,8);
 m.lights.push_back(caGlow({6.6f,15.4f},f+1.2f,.3f,3.f,6500,{.72f,.5f,1.f}));
 // W3: intact glass, and the man they sealed in it
 caBody(m,clock,ActorVisual::CleanupEngineer,{5.4f,21.f},f,1.1f);
 hungSuit(m,{3.f,19.4f,f+2.1f},caS);
 taped(m,{9.52f,20.9f},f+1.5f,caE,Readable::Note,"W3 / SEALED 02:50","MARSH. HE WAS FINE. HE KEPT SAYING HE WAS FINE, THE SAME WAY, EVERY TIME WE ASKED.",
  "KESSLER SAID LEAVE THE LIGHT ON FOR HIM.");
 m.lights.push_back(lamp({5.f,23.f},f+2.8f,.35f,5.f,6500,.02f,WorldLightMount::Wall,clN));
 // --- East of the aisle: E1 blown, the booth, the passage to service, E3 behind its gate ---------
 wall(m,14.5f,1.f,14.7f,4.2f,f,roof,15);caBlownFront(m,14.5f,4.2f,14.7f,9.f,f,roof,-1,23);wall(m,14.5f,9.f,14.7f,10.2f,f,roof,15);
 caGlaze(m,14.5f,10.2f,14.7f,14.4f,f,roof);wall(m,14.5f,14.4f,14.7f,15.f,f,roof,15);
 wall(m,14.5f,15.f,14.7f,19.f,f+3.4f,roof,15);
 wall(m,14.5f,19.f,14.7f,19.35f,f,roof,15);wall(m,14.5f,22.85f,14.7f,23.f,f,roof,15);wall(m,14.5f,19.35f,14.7f,22.85f,f+2.72f,roof,15);
 caFix(m,model::lab_cell_gate,14.6f,21.1f,f,caE,false);wall(m,14.53f,19.35f,14.67f,22.85f,f,f+2.72f,CollisionProxyMaterial);
 // E1's back wall, holed through into the booth where the growth pushed: ragged concrete, crust round
 // the lip, the rubble blown out on the booth floor. The detail's lamps mark it from the aisle.
 wall(m,14.7f,9.5f,15.7f,9.7f,f,roof,15);wall(m,17.1f,9.5f,23.f,9.7f,f,roof,15);wall(m,15.7f,9.5f,17.1f,9.7f,f+2.3f,roof,15);
 wall(m,15.7f,9.5f,16.05f,9.7f,f+1.85f,f+2.3f,15);wall(m,16.7f,9.5f,17.1f,9.7f,f+2.05f,f+2.3f,15);wall(m,16.85f,9.5f,17.1f,9.7f,f,f+.4f,15);
 for(float x:{15.58f,17.1f})caCrust(m,x,9.35f,x+.12f,9.85f,f,f+2.3f);caCrust(m,15.7f,9.3f,17.1f,9.5f,f,f+.05f);caCrust(m,15.4f,9.7f,17.4f,10.5f,f,f+.04f);
 caKnot(m,17.35f,9.25f,f,1.2f);caKnot(m,15.45f,9.95f,f+.04f,.8f);
 for(auto [x1,y1,x2,y2,h]:{std::tuple{15.9f,10.f,16.3f,10.35f,.22f},{16.6f,10.5f,17.f,10.8f,.16f},{17.3f,9.9f,17.6f,10.2f,.2f}})wall(m,x1,y1,x2,y2,f,f+h,15);
 for(auto [x,y,yaw]:{std::tuple{13.1f,7.1f,.4f},{15.9f,8.7f,2.2f},{17.6f,10.9f,-.8f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=caGlow({x,y},f+.5f,.55f,4.2f,3000);l.prop=lampProp;m.lights.push_back(l);}
 wall(m,14.7f,19.f,23.f,19.2f,f,roof,15);
 wallDoor(m,14.7f,23.f,14.8f,15.f,18.f,19.2f,f,roof,-1);                                       // door 1: the booth, from the passage
 // E1
 caMound(m,21.f,7.f,f,1.f,.8f);caFix(m,model::med_ward_bed_tossed,18.4f,2.4f,f,.1f,true);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{19.2f,5.4f},f,1,0});
 // E3: dark behind the gate; something hung up in there
 hungSuit(m,{19.f,21.f,f+2.2f},caW);
 m.darkZones.push_back({14.7f,19.2f,23.f,23.f,-1000,1000,0,1,false,false});
 // the booth: what is left of Kessler's detail
 caFix(m,model::lab_control_board,22.86f,12.2f,f+.95f,caW,false);
 terminal(m,{22.2f,11.f},"CONTAINMENT B / CELL STATE","ALL CELLS: LOCKS RELEASED 02:00:00. NO OPERATOR INPUT. NO FAULT LOGGED. FEED LOST 02:14.",
  "INNER DOOR HELD 02:01-02:09 (KESSLER). BOOTH SEALED 02:10. TWO IN BOOTH.",0);
 m.terminals.back().yaw=caW;
 caFix(m,model::lab_specimen_crate,21.6f,14.f,f,.2f,true,.7f);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,21.6f,14.f,f+.79f,.18f,.4f,.52f,2.2f);
  auto l=caGlow({21.6f,13.8f},f+1.2f,.6f,5.f,3000);l.prop=lampProp;m.lights.push_back(l);}
 caGuard(m,clock,"cona_b_lead",ActorVisual::Cleanup,GuardSquadRole::Assault,"cona_b",{{{15.6f,11.6f},f,60000,caW}},10,1,1.f,70);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("cona_b_hello");}
 caGuard(m,clock,"cona_b_doc",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"cona_b",{{{19.6f,10.6f},f,60000,caS}},8,2,.7f,45);
 m.actorTracks.back().ai.stance=stance;
 m.dialogue.push_back({stateId("cona_b_hello"),"B DETAIL","So the hub got its power and not us. Hands where I can see them.","cona_b_hello",{
  caReply("What happened in here?","cona_b_what"),caReply("Come with me.","cona_b_come"),caReply("Which way out?","cona_b_way")}});
 m.dialogue.push_back({stateId("cona_b_what"),"B DETAIL","Every cell let go at two. Same second. Kessler held the inner door and the door held him.","cona_b_what",{caReply("Back.","cona_b_hello")}});
 m.dialogue.push_back({stateId("cona_b_come"),"B DETAIL","Doc can't walk. And you heard the radio. Out there we're exposed. In here we're just stuck.","cona_b_come",{caReply("Back.","cona_b_hello")}});
 m.dialogue.push_back({stateId("cona_b_way"),"B DETAIL","West is where it started. Don't. The aisle's grown shut; out our door and round is the only way south. Handling floor, then the theater. The spur east loops back to A.","cona_b_way",{caReply("Back.","cona_b_hello")}});
 caEvent(m,"cona_b_truce",9.5f,4.f,14.5f,9.f,"",{caSet("cona_b_stance",int(SquadStance::Truce))});
 // The Investigator writes them off. They hear it. So does the hub.
 caSequence(m,"cona_b_radio","cona_b_radio_done",26500,25800,9.5f,9.f,14.5f,15.f);
 const Vec2 radio{15.6f,11.6f};
 cue(m,"cona_b_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. Containment B detail is compromised. Treat them as exposed.",.9f,f+1.5f,"cona_b_radio_1");
 cue(m,"cona_b_radio",8100,Sound::Empty,radio,"RADIO (B): Sir, this is B. We're not exposed. We're shut in. Kessler's dead.",.9f,f+1.5f,"cona_b_radio_2");
 cue(m,"cona_b_radio",13900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you know what exposed looks like.",.9f,f+1.5f,"cona_b_radio_3");
 cue(m,"cona_b_radio",16800,Sound::Empty,radio,"B DETAIL: He's not coming. Nobody's coming. Sit down, Doc.",.9f,f+1.5f,"cona_b_radio_4");
 cue(m,"cona_b_radio",21200,Sound::Empty,radio,"B DETAIL: You. Anything comes through that hole while you're here, we've got it.",.9f,f+1.5f,"cona_b_radio_5");
 // Written off, they are hers: anything that comes at the booth while she is in it, they shoot.
 caEvent(m,"cona_b_written_off",0,0,24,24,"cona_b_radio_done",{caSet("cona_b_stance",int(SquadStance::Ally))});
 m.scriptEvents.back().conditions.push_back({ScriptCondition::Source::State,stance,ScriptCondition::Op::Equal,int(SquadStance::Truce)});
 // --- The aisle: the third one, the alarm, the tubes that are left --------------------------------
 m.creatureSpawns.push_back({CreatureKind::Transformed,{12.f,20.6f},f,1,0});
 for(auto [x,y,yaw,ph]:{std::tuple{9.5f,18.9f,clE,0.f},{14.5f,9.8f,clW,.8f},{9.5f,22.7f,clE,1.5f}})m.lights.push_back(caAlarm({x,y},f+2.9f,yaw,ph));
 m.lights.push_back(caTube(12.f,14.f,roof,.75f,7.5f,6000,.04f,.5f));m.lights.push_back(caTube(12.f,21.6f,roof,.7f,7.f,6000,.04f,.6f));
 caDeadTube(m,12.f,6.f,roof);caDeadTube(m,12.f,16.8f,roof);
 m.lights.push_back(caTube(12.f,2.4f,roof,.4f,5.f,5600,.03f));
 // the one tube still on over the aisle: a cold key down onto the mass, so it reads as a shape in
 // the dark, the rock black and the growth a dull wet magenta, the crystal bright in its folds
 m.lights.push_back(caTube(12.f,8.4f,roof,1.15f,8.5f,7400,.05f,.08f));
 // W1, what it was before it opened: a cell, its pan and basin on the south wall, the lamp over
 // them still working; the crust has come along that wall from the spire and the rubble of the
 // front is round the bed.
 caFix(m,model::med_ward_toilet,8.5f,8.43f,f,caN,true);caFix(m,model::med_scrub_sink,7.2f,8.5f,f,caN,true);
 caFix(m,model::med_floor_drain,5.6f,5.f,f,0,false);
 caCrust(m,1.f,8.6f,5.2f,8.8f,f,f+3.6f);
 m.fixtures.push_back({model::mine_cage_lamp,{6.1f,8.68f},f+2.45f-m.layers.front().elevation,.21f,.2f,.36f,kPi,false});
 m.lights.push_back(lamp({6.1f,8.8f},f+2.6f,.6f,6.5f,6800,.02f,WorldLightMount::Wall,clN));m.lights.back().flicker=.25f;
 for(auto [x1,x2,z]:{std::tuple{6.6f,7.9f,1.1f},{7.f,8.4f,1.45f}})wall(m,x1,8.788f,x2,8.8f,f+z,f+z+.05f,16);
 caModel(m,model::cave_oldcity_rubble,6.3f,6.f,f,1.03f,1.03f,.18f,.7f);caModel(m,model::cave_oldcity_rubble,8.1f,3.4f,f,.8f,.8f,.14f,2.1f);
 m.lights.push_back(lamp({9.5f,3.5f},f+2.5f,.45f,4.5f,4200,0,WorldLightMount::Wall,clE));                // the suit rail's own lamp: the vestibule's west wall is a wall, not a void
 m.lights.push_back(lamp({20.f,19.f},f+2.4f,.35f,5.f,4200,.2f,WorldLightMount::Wall,clN));
 m.pickupSpawns.push_back({{20.2f,12.8f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{16.f,17.6f},PickupKind::Health,f});
 recessedFittings(m);
 zone(m,"CONTAINMENT B / WEST PASSAGE",0,9.f,9.3f,14.f,-1000,1000,{.16f,.12f,.18f,.012f});
 zone(m,"CONTAINMENT B / BOOTH",14.7f,9.7f,23.f,14.8f,-1000,1000,{.15f,.12f,.1f,.008f});
 zone(m,"CONTAINMENT B / WEST CELLS",1.f,1.f,9.3f,23.f,-1000,1000,{.15f,.12f,.15f,.009f});
 zone(m,"CONTAINMENT B / EAST CELLS",14.7f,1.f,23.f,23.f,-1000,1000,{.15f,.12f,.15f,.009f});
 zone(m,"CONTAINMENT B",0,0,24,24,-1000,1000,{.14f,.12f,.14f,.009f});
 freightFinish(m,-1);
 return p;
}
}
void buildChapterContainmentA(CampaignMapTable& maps){
 maps[94]=makeQuarantineWard();
 maps[95]=makeContainmentIntake();
 maps[96]=makeContainmentA();
 maps[97]=makeContainmentService();
 maps[98]=makeContainmentB();
}
}
