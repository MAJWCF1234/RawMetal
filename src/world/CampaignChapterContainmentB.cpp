// CONTAINMENT (chapter 2, manifest 65-70): chunks 99, 100, 101, 102, 103.
// See docs/Design/descent-campaign-20261025.md (Containment) for the brief.
// Route: from Containment B (98) S into Specimen Handling (99), E across its gallery into the
// Observation Theater (101), N from the theater back into the service corridors (97). From B W into
// the Breach Sector (102), ground zero, down through the crater and S to Emergency Containment (103),
// where the barricade (cont_barricade) is raised from the control room and the S door opens on Deep
// Research (104). The Isolation Wing (100) is the dead end east of the Quarantine Ward: LAB 4's
// bunker, her lamps and her handwriting.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "Primitives.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- Containment kit (cb): the same fittings as CampaignChapterContainmentA.cpp ----------------
// Fixture and sign facing: 0 faces +y (south), kPi north, cbE east, cbW west.
constexpr float cbS=0,cbN=kPi,cbE=kPi*.5f,cbW=-kPi*.5f;
// Light yaw (a wall lamp's way into the room): 0 east, kPi*.5 south, kPi west, -kPi*.5 north.
constexpr float blE=0,blS=kPi*.5f,blW=kPi,blN=-kPi*.5f;
// The cave pack's standing stones bake their texture into tiny islands and render as flat dark lumps;
// in these levels every standing stone is one of the modelled outcrops of the bedrock instead, at the
// size it was placed at (the hanging stalactites and ceiling rock keep the pack's own).
int cbBedrock(int mdl){
 if(mdl==model::cave_boulder_round||mdl==model::cave_boulder_a||mdl==model::cave_rock_block)return model::res_rock_crag;
 if(mdl==model::cave_rock_slab)return model::res_rock_low;
 if(mdl==model::cave_rock_column||mdl==model::cave_stalagmite)return model::res_rock_spire;
 return mdl;}
void cbFix(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];m.fixtures.push_back({cbBedrock(model),{x,y},z-m.layers.front().elevation,v[0]*s,v[1]*s,v[2]*s,yaw,solid});
}
WorldLight cbTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint=0,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
void cbDeadTube(Map& m,float x,float y,float roof){m.fixtures.push_back({model::clinic_ceiling_lamp_2,{x,y},roof-.17f-m.layers.front().elevation,.44f,1.04f,.17f,0,false});}
WorldLight cbBeacon(Vec2 p,float z,float yaw,float phase,const char* state){
 auto l=lamp(p,z,.36f,4.6f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.12f,.06f});
 l.period=1.4f;l.onTime=.7f;l.phase=phase;l.enabledState=stateId(state);l.invert=true;return l;}
WorldLight cbAlarm(Vec2 p,float z,float yaw,float phase){
 auto l=lamp(p,z,.42f,6.f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.1f,.05f});l.period=2.2f;l.onTime=1.3f;l.phase=phase;return l;}
// Resonance light (the violet gel) is keyed cold, as in CampaignChapterContainmentA.cpp: the growth
// sits dark under a blue-white key and the crystal's own cyan is the accent.
WorldLight cbGlow(Vec2 p,float z,float intensity,float range,float kelvin,Point3 gel={1,1,1},float flicker=0){
 if(gel.x>.55f&&gel.y<.56f&&gel.z>.9f){gel={.5f,.72f,1.f};kelvin=std::max(kelvin,8000.f);intensity*=.8f;}
 auto l=lamp(p,z,intensity,range,kelvin,0,WorldLightMount::Bare,0,false,gel);l.flicker=flicker;return l;}
void cbPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0,float tint=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});m.fixtures.push_back({model::pendant_lamp,p,z-m.layers.front().elevation,.5f,.5f,.34f,0,false});
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,tint,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}
void cbBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
void cbGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
void cbSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1,float y1,float x2,float y2){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
void cbHum(Map& m,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top,float gain=.55f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;s.bottom=bottom;s.top=top;s.durationMs=3600000;s.finishAtMs=3600000;
 s.soundIntervalMs=10000;s.soundUntilMs=3000000;s.tickSound=Sound::AnomalyHum;s.gain=gain;s.pitch=1;m.timedSequences.push_back(s);
}
ScriptAction cbSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
void cbEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions,bool once=true){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=once;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
DialogueReply cbReply(const char* text,const char* next="",const char* sets="",int stance=-1){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;if(*sets)r.sets.push_back({stateId(sets),1});r.stance=stance;return r;
}
void cbGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float roof,float sill=.95f,float head=2.4f,bool holds=true,int mat=3){
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
void cbPrim(Map& m,const char* kind,float x,float y,float z,float w,float d,float h,int facing,int mat,bool solid=true){expandPrimitive({kind,x,y,z,w,d,h,facing,mat,solid},m.structures);}
void cbModel(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid=false){m.fixtures.push_back({cbBedrock(model),{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});}
unsigned cbSeed(float x,float y){unsigned h=unsigned(x*97.f)*2654435761u^unsigned(y*131.f)*40503u;h^=h>>13;h*=1274126177u;return h^(h>>16);}
// Resonance growth and crystal, the same as CampaignChapterContainmentA.cpp builds them. Rock first: what came up through the floors and the walls is the
// cave pack's black rock, and the growth lives in it, its magenta only in the seams between the
// rocks, in the cracks it opened in the concrete and where it is wet. Nothing of the growth's finish
// is a shape stood flat on a wall or a dome stretched over a heap: it is in narrow boxes, struts and
// columns, all drawn at the finish's own repeat. The crystal is clusters of narrow hexagonal prisms,
// glass over a lit core with the bright points of the self-lit finish, leaning out of one root half
// buried in rock and rubble, the cave pack's bed of spikes at their feet.
void cbSeam(Map& m,float x1,float y1,float x2,float y2,float lo,float hi,int mat=34){wall(m,x1,y1,x2,y2,lo,hi,mat);m.structures.back().solid=false;}
// A floor (the chunk's or a deck's) at height z, under (x,y).
bool cbOnFloor(const Map& m,float x,float y,float z){
 const int tx=std::clamp(int(std::floor(x)),0,23),ty=std::clamp(int(std::floor(y)),0,23);
 for(const auto& l:m.layers){if(std::fabs(l.elevation-z)>.3f)continue;if(&l==&m.layers.front()||l.rows[size_t(ty)][size_t(tx)]=='=')return true;}
 return false;
}
// Which side of a strip laid along a wall the wall stands: -1 at its low edge (y1, or x1), +1 at its high edge.
int cbWallSide(const Map& m,float x1,float y1,float x2,float y2,float z){
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
void cbFit(Map& m,std::initializer_list<int> models,float x,float y,float z,float w,float d,float h,float yaw){
 int best=*models.begin();float bestSpread=1e9f;bool turn=false;
 for(int mdl:models)for(int q=0;q<2;++q){const auto& v=model::size[mdl];const float r0=(q?d:w)/v[0],r1=(q?w:d)/v[1],r2=h/v[2];
  const float spread=std::max({r0,r1,r2})/std::min({r0,r1,r2});if(spread<bestSpread-.02f){bestSpread=spread;best=mdl;turn=q==1;}}
 // never taller than the box: a rock too tall for its footprint is drawn narrower, not higher
 const auto& v=model::size[best];const float r0=(turn?d:w)/v[0],r1=(turn?w:d)/v[1],r2=h/v[2],s=std::min(std::cbrt(r0*r1*r2),r2/.93f);
 auto k=[&](float a){return std::clamp(a/s,.93f,1.075f)*s;};
 cbModel(m,best,x,y,z,v[0]*k(r0),v[1]*k(r1),v[2]*k(r2),yaw+(turn?kPi*.5f:0.f));
}
// One prism from its foot at (x,y,z): h long, r across, leaning (lx,ly) metres per metre of rise.
// Upright, it is one hexagonal shaft of glass round a thread of the self-lit finish (lit: a thick
// core of it); leaning, a slimmer shaft of the self-lit finish laid in short courses, each set over by
// the lean (short shards only: a long one would show its courses). Its point steps in, broad and
// bright where it is bright.
void cbPrism(Map& m,float x,float y,float z,float lx,float ly,float h,float r,bool lit=false,bool bright=false){
 const float lean=std::sqrt(lx*lx+ly*ly),tip=std::min(h*.3f,r*1.7f),body=h-tip;
 auto ring=[&](float lo,float hi,float rr,int mat){const float d=(lo+hi)*.5f-z,cx=x+lx*d,cy=y+ly*d;wall(m,cx-rr,cy-rr,cx+rr,cy+rr,lo,hi,mat);auto& s=m.structures.back();s.shape=ShapeColumn;s.solid=false;};
 if(lean<.1f){lx=ly=0;ring(z,z+body,r,17);ring(z,z+body,r*(lit?.55f:.4f),35);}
 else{const int n=std::clamp(int(body*lean/(r*.35f))+1,1,4);const float seg=body/float(n);for(int i=0;i<n;++i)ring(z+seg*float(i),z+seg*float(i+1),r*.55f,35);}
 ring(z+body-.002f,z+h,r*(lit||bright?.55f:.38f),35);
}
// A crag of the rock too big for any of the pack's stones (their black finish only reads where a
// lamp is close): the rock's own finish in faces leaning back from a ridge and from both its ends,
// so it takes the light the way the heaved bedrock of the Breach Sector does.
void cbCrag(Map& m,float x,float y,float z,float w,float d,float h,unsigned seed,bool solid=false){
 // a modelled outcrop of the bedrock (res_rock_*): broad, tall or low by the crag's proportions, laid
 // along its long side and turned end for end by its seed, sunk a little so it grows out of the floor
 const bool alongX=w>=d;const float lng=alongX?w:d,sht=alongX?d:w;
 const int mdl=h>lng*1.25f?model::res_rock_spire:h<lng*.5f?model::res_rock_low:model::res_rock_crag;
 const float yaw=(alongX?0.f:kPi*.5f)+(((seed>>3)&1u)?kPi:0.f);
 cbModel(m,mdl,x,y,z-h*.04f,lng,sht,h*1.04f,yaw,solid);
}
// A fang of the rock come up from below at scale s (1 is 3.7 m tall): a tall narrow crag in the rock's
// own finish, so it takes the light (the pack's stalagmite is too black to read but under a lamp).
void cbFang(Map& m,float x,float y,float z,float s,float yaw){cbCrag(m,x,y,z,1.5f*s,1.1f*s,3.7f*s,cbSeed(x,y+yaw),true);}
// A crack the growth opened in a wall face along x (alongX) or y at `face`, from (t,z1) up to z2:
// a zig-zag of thin struts with the magenta in it, now and then a fork.
void cbCrack(Map& m,bool alongX,float face,float t,float z1,float z2,unsigned seed){
 unsigned g=seed|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 auto leg=[&](float a,float z,float run,float rise,float w){
  if(std::fabs(run)<.03f){if(alongX)cbSeam(m,a-w*.5f,face-w*.5f,a+w*.5f,face+w*.5f,z,z+rise);else cbSeam(m,face-w*.5f,a-w*.5f,face+w*.5f,a+w*.5f,z,z+rise);return;}
  const float c=a+run*.5f;
  if(alongX)cbPrim(m,"strut",c,face,z,w,std::fabs(run),rise,run>0?1:3,34,false);else cbPrim(m,"strut",face,c,z,w,std::fabs(run),rise,run>0?0:2,34,false);};
 float a=t,z=z1,w=.035f+next()*.025f;
 while(z<z2-.1f){const float rise=std::min(.18f+next()*.4f,z2-z),run=(next()-.5f)*.36f;leg(a,z,run,rise,w);
  if(next()<.16f&&z+rise<z2-.4f){const float fr=(next()<.5f?-1.f:1.f)*(.12f+next()*.2f);leg(a+run,z+rise-.02f,fr,.2f+next()*.35f,w*.7f);}
  a+=run;z+=rise-.01f;w=std::max(.018f,w*.93f);}
}
// A crust on a surface. On a floor: a film of the rock with wet sheen on it, the growth in small wet
// patches and in the cracks across it, a stone or two. Against a wall: the rock heaped up the face
// in pieces half buried in it, the growth in the seams between them, and cracks with the growth in
// them running on up the concrete from their shoulders, wet staining down from the cracks.
void cbCrust(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){
 unsigned h=cbSeed(x1*3.f+x2,y1*5.f+y2+lo*.37f)|1u;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return float(h&0xffffu)/65535.f;};
 if(hi-lo<.3f){
  wall(m,x1,y1,x2,y2,lo,lo+.012f,26);
  const int nx=std::max(1,int((x2-x1)/.8f+.5f)),ny=std::max(1,int((y2-y1)/.8f+.5f));const float cw=(x2-x1)/nx,cd=(y2-y1)/ny;
  for(int i=0;i<nx;++i)for(int j=0;j<ny;++j){const float r=next(),cx=x1+(i+.5f)*cw,cy=y1+(j+.5f)*cd,sx=cw*(.18f+next()*.2f),sy=cd*(.18f+next()*.2f);
   if(r<.2f){cbSeam(m,cx-sx,cy-sy*.5f,cx+sx,cy+sy*.5f,lo+.012f,lo+.02f);cbSeam(m,cx-sx*.5f,cy-sy,cx+sx*.5f,cy+sy,lo+.012f,lo+.024f);}
   else if(r<.55f)cbSeam(m,cx-sx*1.4f,cy-sy*1.4f,cx+sx*1.4f,cy+sy*1.4f,lo+.013f,lo+.016f,17);
   else if(r>.9f){const float s=.05f+next()*.06f;cbModel(m,model::cave_boulder_round,cx,cy,lo-.06f,3.15f*s,2.17f*s,2.98f*s,next()*6.28f);}}
  // the cracks across it the growth came up through
  for(int k=0;k<1+int((x2-x1)*(y2-y1)>6.f);++k){float cx=x1+(x2-x1)*(.2f+next()*.6f),cy=y1+(y2-y1)*(.2f+next()*.6f);const bool ax=next()<.5f;
   for(int s=0;s<5;++s){const float run=.25f+next()*.45f,w=.03f+next()*.03f,jog=(next()-.5f)*.3f;
    if(ax){const float nx2=std::min(x2,cx+run);if(nx2-cx>.05f)cbSeam(m,cx,cy-w*.5f,nx2,cy+w*.5f,lo+.012f,lo+.02f);cx=nx2;cy=std::clamp(cy+jog,y1+.05f,y2-.05f);}
    else{const float ny2=std::min(y2,cy+run);if(ny2-cy>.05f)cbSeam(m,cx-w*.5f,cy,cx+w*.5f,ny2,lo+.012f,lo+.02f);cy=ny2;cx=std::clamp(cx+jog,x1+.05f,x2-.05f);}}}
  return;}
 const bool alongX=x2-x1>=y2-y1;const float len=alongX?x2-x1:y2-y1,span=hi-lo;
 const int side=cbWallSide(m,x1,y1,x2,y2,lo+1.f);
 const float face=alongX?(side<0?y1:y2):(side<0?x1:x2),out=-float(side);   // out: the way into the room
 auto at=[&](float t,float o){return alongX?Vec2{x1+t,face+o*out}:Vec2{face+o*out,y1+t};};
 auto across=[&](float t0,float t1,float o0,float o1,float z1,float z2,int mat=34){const Vec2 a=at(t0,o0),b=at(t1,o1);cbSeam(m,std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y),z1,z2,mat);};
 const float yawBase=alongX?0.f:kPi*.5f;
 if(len<.9f){
  // a narrow face: one buttress of the rock at its foot, the growth run down it in two veins
  const float ph=std::min(span,.8f+next()*.6f);const Vec2 c=at(len*.5f,.12f);
  cbFit(m,{model::cave_rock_column,model::cave_stalagmite},c.x,c.y,lo-.04f,len*1.1f,.5f,ph,yawBase+(next()-.5f)*.4f);
  for(int k=0;k<2;++k){const float t=len*(.25f+.5f*float(k))+(next()-.5f)*.1f,w=.03f+next()*.03f;across(t-w*.5f,t+w*.5f,-.01f,.012f,lo+ph*.5f,lo+std::min(span,ph+.4f+next()*span*.6f));}
  return;}
 float prevTop=0,prevEnd=-1;
 for(float a=0;a<len-.15f;){
  const float pw=std::min(.75f+next()*.95f,len-a+.25f),ph=std::min(span,.45f+next()*std::min(1.8f,span*.45f)),pd=.55f+next()*.55f,t=a+pw*.5f;
  const Vec2 c=at(t,pd*.12f);const float yaw=yawBase+(next()-.5f)*.5f+(next()<.5f?kPi:0.f);
  if(ph>1.1f){const int fc=alongX?(out>0?0:2):(out>0?1:3);const Vec2 b=at(t,pd*.32f);(void)fc;cbCrag(m,b.x,b.y,lo,alongX?pw:pd*.64f,alongX?pd*.64f:pw,ph,h^unsigned(t*977.f),true);
   const Vec2 f2=at(t+(next()-.5f)*pw*.4f,pd*.7f);cbFit(m,{model::cave_boulder_a,model::cave_rock_slab},f2.x,f2.y,lo-.04f,.7f,.55f,.42f,yaw);}
  else if(next()<.5f)cbFit(m,{model::cave_boulder_round,model::cave_rock_column,model::cave_boulder_a,model::cave_rock_slab},c.x,c.y,lo-.05f,pw,pd,ph,yaw);
  else cbFit(m,{model::cave_boulder_a,model::cave_rock_block,model::cave_rock_slab,model::cave_stalagmite},c.x,c.y,lo-.05f,pw,pd,ph,yaw);
  // the seam where it meets the one before: the growth in the crevice between them
  if(prevEnd>0){const float s=(prevEnd+a)*.5f,st=.07f+next()*.08f;across(s-st*.5f,s+st*.5f,0,pd*.22f,lo,lo+std::min(ph,prevTop)*.55f);}
  // a crack on up the concrete from its shoulder, and the wet coming down out of it
  if(span>ph+.5f&&next()<.6f){const float ct=t+(next()-.5f)*pw*.5f,z2=std::min(hi-.05f,lo+ph+.5f+next()*std::min(3.4f,span-ph));
   cbCrack(m,alongX,face+out*.004f,alongX?x1+ct:y1+ct,lo+ph*.75f,z2,h^unsigned(ct*977.f));
   if(next()<.5f){const float w=.05f+next()*.07f;across(ct-w*.5f,ct+w*.5f,.002f,.006f,lo+ph*.6f,lo+ph+.4f+next()*.9f,8);}}
  prevTop=ph;prevEnd=a+pw;a+=pw*.78f;}
}
// A patch of crust on a floor: the rock's film, cracked, wet, a hand high at most.
void cbLens(Map& m,float x,float y,float f,float w,float d,float h=.09f){(void)h;cbCrust(m,x-w*.45f,y-d*.45f,x+w*.45f,y+d*.45f,f,f+.04f);}
// A modelled crystal cluster (tools/library/build_resonance_crystals.py) whose prisms stand from z,
// at scale s, its rock root sunk below z so only the root's crown shows. res_crystal_l is 2.9 m tall
// at 1, res_crystal_m 1.6, res_crystal_s .7; the prisms burn from their bright tips (a *_glow finish).
void cbCrystal(Map& m,int mdl,float x,float y,float z,float s,float yaw){
 const auto& v=model::size[mdl];const float sink=mdl==model::res_crystal_l?.52f:mdl==model::res_crystal_m?.34f:.17f;
 cbModel(m,mdl,x,y,z-sink*s,v[0]*s,v[1]*s,v[2]*s,yaw);
}
// The cluster that stands about h high from z: the big knot, the middle one or the scatter.
void cbCrystalTo(Map& m,float x,float y,float z,float h,float yaw){
 const int mdl=h>=1.7f?model::res_crystal_l:h>=.85f?model::res_crystal_m:model::res_crystal_s;
 const float tall=mdl==model::res_crystal_l?2.36f:mdl==model::res_crystal_m?1.22f:.52f;   // the leader's height at scale 1
 cbCrystal(m,mdl,x,y,z,h/tall,yaw);
}
// A crystal from its foot at (x,y,z): one prism w across and h high, or (lean 0 +y, 1 +x, 2 -y, 3 -x)
// leaning out `reach` metres that way as it rises h.
void cbShard(Map& m,float x,float y,float z,float w,float h,int lean=-1,float reach=0){
 (void)w;(void)reach;constexpr float yaws[4]={kPi*.5f,0.f,-kPi*.5f,kPi};
 cbCrystalTo(m,x,y,z,h,lean<0?cbSeed(x,y)%628u*.01f:yaws[lean]);
}
// The cave pack's bed of crystal spikes at scale s.
void cbBed(Map& m,float x,float y,float z,float s,float yaw){cbModel(m,model::cave_crystal_bed_blue,x,y,z,1.22f*s,1.22f*s,.245f*s,yaw);}
// A body of growth w by d by h: the rock it came up on, in pieces of the pack's rock kept near their
// own proportions, the growth in the seams between them and wet at their foot. (back: kept for the
// callers that once used it to turn the growth from the player; the rock now faces every way.)
void cbGrowth(Map& m,float x,float y,float z,float w,float d,float h,bool solid=false,float back=0){
 (void)back;unsigned g=cbSeed(x+z,y)|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 const bool alongX=w>=d;const float lng=alongX?w:d,sht=alongX?d:w;
 const int pieces=std::clamp(int(lng/(sht*1.3f)+.5f),1,4);const float step=lng/float(pieces);
 const float yawBase=alongX?0.f:kPi*.5f;Vec2 prev{};float prevTop=0;
 for(int i=0;i<pieces;++i){
  const float t=-lng*.5f+step*(float(i)+.5f)+(next()-.5f)*step*.15f;
  const float ph=h*(i==pieces/2?1.f:.6f+next()*.3f),pw=step*1.28f,pd=sht*(.88f+next()*.18f);
  const Vec2 c=alongX?Vec2{x+t,y+(next()-.5f)*sht*.1f}:Vec2{x+(next()-.5f)*sht*.1f,y+t};const float yaw=yawBase+(next()-.5f)*.5f+(next()<.5f?kPi:0.f);
  if(ph>.9f||std::max(pw,pd)>1.4f){cbCrag(m,c.x,c.y,z,alongX?pw:pd,alongX?pd:pw,ph,g^unsigned(i*7919));
   const float q=std::min(ph*.32f,.55f);cbFit(m,{model::cave_boulder_a,model::cave_rock_slab},c.x+(next()-.5f)*pw*.6f,c.y+(next()-.5f)*pd*.6f,z-.04f,q*1.7f,q*1.3f,q,yaw);}
  else if(next()<.5f)cbFit(m,{model::cave_boulder_round,model::cave_rock_column,model::cave_boulder_a,model::cave_rock_slab},c.x,c.y,z-.06f,pw,pd,ph,yaw);
  else cbFit(m,{model::cave_rock_block,model::cave_stalagmite,model::cave_rock_slab,model::cave_boulder_a},c.x,c.y,z-.06f,pw,pd,ph,yaw);
  if(i>0){const Vec2 s=(c+prev)*.5f;const float sh=std::min(ph,prevTop)*.58f,sw=sht*.36f,st=.08f+next()*.08f;
   if(alongX)cbSeam(m,s.x-st*.5f,s.y-sw*.5f,s.x+st*.5f,s.y+sw*.5f,z,z+sh);else cbSeam(m,s.x-sw*.5f,s.y-st*.5f,s.x+sw*.5f,s.y+st*.5f,z,z+sh);}
  prev=c;prevTop=ph;}
 // wet at its foot, where it stands on a floor: sheen, and the growth out of the cracks
 if(cbOnFloor(m,x,y,z))for(int k=0;k<3;++k){const float a=next()*6.28f,r=.5f+next()*.2f,px=x+std::cos(a)*w*r,py=y+std::sin(a)*d*r,s=.12f+next()*.2f;
  cbSeam(m,px-s*1.6f,py-s*1.2f,px+s*1.6f,py+s*1.2f,z+.002f,z+.005f,17);
  cbSeam(m,px-s*.5f,py-.025f,px+s*.5f,py+.025f,z+.002f,z+.012f);cbSeam(m,px-.025f,py-s*.4f,px+.025f,py+s*.4f,z+.002f,z+.012f);}
 if(solid)wall(m,x-w*.3f,y-d*.3f,x+w*.3f,y+d*.3f,z,z+h*.75f,CollisionProxyMaterial);
}
// A mound of growth: a heap of the rock, a boulder it brought up broken off at its foot, the pods
// budding in its seams.
void cbMound(Map& m,float x,float y,float f,float r,float h){
 const unsigned s=cbSeed(x,y);
 cbGrowth(m,x,y,f,r*2.f,r*1.5f,h,true);
 cbModel(m,model::cave_boulder_round,x-r*((s&8u)?1.05f:-1.05f),y+r*.55f,f-.12f,.63f*r,.434f*r,.596f*r,float(s%628u)*.01f);
 for(int i=0;i<1+int(h>.9f);++i){const float a=float((s>>(4+i*5))%628u)*.01f,k=.62f+.1f*float(i),sc=.42f+.18f*float((s>>(i*3))&3u)/3.f;
  cbModel(m,model::cave_nest_pod,x+std::cos(a)*r*k,y+std::sin(a)*r*.75f*k,f-.04f,.7f*sc,.7f*sc,.56f*sc,a);}
}
// A cluster of crystal, h at its tallest, from a root at (x,y) on the floor f: the root a crown of
// rock with the rest of it under the floor where there is ground for it, rubble and the bed of
// spikes round it, the prisms fanned out of it, the leader nearly upright, most of them short.
// A cluster a metre or more high has its own cold light in among the prisms.
void cbKnot(Map& m,float x,float y,float f,float h){
 unsigned g=cbSeed(x,y)|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 const bool ground=std::fabs(f-m.layers.front().elevation)<.35f;
 const float rw=std::clamp(.6f+h*.3f,.7f,2.2f),show=std::clamp(.16f+h*.09f,.18f,.5f);
 const int root=(g&2u)?model::cave_rock_block:model::cave_boulder_a;const auto& v=model::size[root];const float k=rw/v[0],rh=v[2]*k;
 if(ground)cbModel(m,root,x,y,f+show-rh,rw,v[1]*k,rh,next()*6.28f);
 else{const auto& sl=model::size[model::cave_rock_slab];const float q=(show+.04f)/sl[2];cbModel(m,model::cave_rock_slab,x,y,f-.02f,sl[0]*q,sl[1]*q,sl[2]*q,next()*6.28f);}
 const float top=f+show;
 {const float q=rw*.34f;cbModel(m,model::cave_oldcity_rubble,x+(next()-.5f)*rw*.9f,y+(next()-.5f)*rw*.7f,f,1.71f*q,1.71f*q,.3f*q,next()*6.28f);}
 cbBed(m,x+(next()-.5f)*.3f,y+(next()-.5f)*.3f,f,std::clamp(rw*.95f,.7f,1.9f),next()*6.28f);
 {const float b=next()*6.28f;cbBed(m,x+std::cos(b)*rw*.6f,y+std::sin(b)*rw*.6f,f,.5f+next()*.4f,next()*6.28f);}
 // the crystal itself: the modelled cluster out of the root's crown, a smaller one beside it on a big knot
 const float a0=next()*6.28f;
 cbCrystalTo(m,x,y,top-.05f,h,a0);
 if(h>=.9f)cbCrystalTo(m,x+std::cos(a0+2.4f)*rw*.42f,y+std::sin(a0+2.4f)*rw*.42f,f,h*(.35f+next()*.15f),a0+1.3f);
 wall(m,x-rw*.28f,y-rw*.28f,x+rw*.28f,y+rw*.28f,f,f+std::min(h,1.6f)*.8f,CollisionProxyMaterial);
 // its light in among the prisms, lifted clear of anything solid it would sit in
 if(h>=1.f){const Vec2 lp{x+std::cos(a0)*rw*.42f,y+std::sin(a0)*rw*.42f};float lz=top+h*.3f;
  for(int pass=0;pass<3;++pass)for(const auto& s:m.structures)if(s.solid&&lp.x>s.x1-.05f&&lp.x<s.x2+.05f&&lp.y>s.y1-.05f&&lp.y<s.y2+.05f&&lz>s.bottom-.05f&&lz<s.top+.05f)lz=s.top+.08f;
  if(lz<f+h+.4f)m.lights.push_back(cbGlow(lp,lz,.5f+h*.16f,3.4f+h*1.3f,9000,{.55f,.85f,1.f},.06f));}
}
// A trunk of growth between two heights, where it ran down a face or reached the roof: the pack's
// rock buttress stacked in pieces near its own proportions, wandering a little, the growth running
// up it in veins and wet at its foot.
void cbTrunk(Map& m,float x,float y,float lo,float hi,float w){
 unsigned g=cbSeed(x,y+lo)|1u;auto next=[&g]{g^=g<<13;g^=g>>17;g^=g<<5;return float(g&0xffffu)/65535.f;};
 const float pw=w*1.5f;const int n=std::max(2,int((hi-lo)/1.6f+.5f));const float step=(hi-lo)/float(n);
 auto col=[&](float cx,float cy,float z1,float z2,float r,int mat){wall(m,cx-r,cy-r,cx+r,cy+r,z1,z2,mat);auto& s=m.structures.back();s.shape=ShapeColumn;s.solid=false;};
 // the rock's own finish in courses that wander a hand off the line, broad at the foot and the head
 for(int i=0;i<n;++i){const float r=w*.5f*((i==0||i==n-1)?1.2f:.85f+next()*.25f);col(x+(next()-.5f)*w*.22f,y+(next()-.5f)*w*.22f,lo+step*float(i)-(i?.12f:0.f),lo+step*float(i+1)+(i+1<n?.12f:0.f),r,26);}
 for(int k=0;k<2+int(w>.45f);++k){const float a=next()*6.28f,rr=w*.42f,z1=lo+(hi-lo)*next()*.35f,z2=std::min(hi-.05f,z1+(hi-lo)*(.2f+next()*.45f));col(x+std::cos(a)*rr,y+std::sin(a)*rr,z1,z2,.025f+next()*.03f,34);}
 if(cbOnFloor(m,x,y,lo)){const float q=std::min(w*.6f,.5f);cbFit(m,{model::cave_boulder_a,model::cave_rock_slab},x+(next()-.5f)*w,y+(next()-.5f)*w,lo-.04f,q*1.7f,q*1.3f,q,next()*6.28f);
  const float s=pw*.9f;cbSeam(m,x-s,y-s*.7f,x+s,y+s*.7f,lo+.002f,lo+.005f,17);cbSeam(m,x-s*.8f,y-.02f,x+s*.8f,y+.02f,lo+.002f,lo+.012f);}
}
void cbPortalY(Map& m,float x1,float x2,float y1,float y2,float f,float roof,float head){
 wall(m,x1,y1-.6f,x2,y1,f,roof,15);wall(m,x1,y2,x2,y2+.6f,f,roof,15);wall(m,x1,y1,x2,y2,f+head,roof,15);
 for(float y:{y1-.08f,y2})wall(m,x1-.04f,y,x2+.04f,y+.08f,f,f+head,20);
}
// Stencilled figures (0-9, A) painted on a wall face along x at y=face, facing +y (dir 1) or -y (dir -1):
// seven-stroke characters h high, the string centred on x, its foot at f+z.
void cbStencil(Map& m,const char* text,float x,float face,int dir,float z,float h,int mat=14){
 const float f=m.layers.front().elevation,w=h*.55f,t=h*.14f,gap=h*.24f,y1=dir>0?face:face-.014f,y2=dir>0?face+.014f:face;z+=f;
 const size_t n=std::char_traits<char>::length(text);const float half=(n*w+(n-1)*gap)*.5f;float cx=0;
 // Strokes are laid out in reading order; read from the -y side the text runs toward -x.
 auto stroke=[&](float a,float b,float lo,float hi){const float xa=x+dir*(a-half),xb=x+dir*(b-half);wall(m,std::min(xa,xb),y1,std::max(xa,xb),y2,lo,hi,mat);};
 for(size_t i=0;i<n;++i,cx+=w+gap){const char c=text[i];
  const int k=c=='A'?0x77:c=='0'?0x3f:c=='1'?0x06:c=='2'?0x5b:c=='3'?0x4f:c=='4'?0x66:c=='5'?0x6d:c=='6'?0x7d:c=='7'?0x07:c=='8'?0x7f:c=='9'?0x6f:0;
  if(k&1)stroke(cx,cx+w,z+h-t,z+h);if(k&8)stroke(cx,cx+w,z,z+t);if(k&64)stroke(cx,cx+w,z+(h-t)*.5f,z+(h+t)*.5f);
  if(k&2)stroke(cx+w-t,cx+w,z+h*.5f,z+h);if(k&4)stroke(cx+w-t,cx+w,z,z+h*.5f);
  if(k&32)stroke(cx,cx+t,z+h*.5f,z+h);if(k&16)stroke(cx,cx+t,z,z+h*.5f);}
}
// A blown glazed front: dado, mullions and head with the glass gone, the shards on the floor (side +1/-1:
// the side the glass fell to), the sill and head at their own heights.
void cbBlownFront(Map& m,float x1,float y1,float x2,float y2,float f,float top,int side,unsigned seed,float sill=.9f,float head=2.6f){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,15);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.03f,alongX?x1:y1,(alongX?x2:y2)-.06f);
  if(alongX)wall(m,t,y1,t+.06f,y2,f+sill,f+head,20);else wall(m,x1,t,x2,t+.06f,f+sill,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.08f,20);if(top>f+head+.08f)wall(m,x1,y1,x2,y2,f+head+.08f,top,15);
 unsigned h=seed*2654435761u+3;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
 for(int i=0;i<26;++i){float a=next()*len,o=.15f+next()*1.3f,w=.02f+next()*.05f;
  float px=alongX?x1+a:(side>0?x2+o:x1-o),py=alongX?(side>0?y2+o:y1-o):y1+a;wall(m,px-w,py-.02f,px+w,py+.02f,f+.002f,f+.006f,17);}
}
// A theater bench facing south (+y): seat, backrest on its north side, legs.
void cbBench(Map& m,float x,float y,float z){
 wall(m,x-1.3f,y-.22f,x+1.3f,y+.22f,z+.43f,z+.5f,10);wall(m,x-1.3f,y-.3f,x+1.3f,y-.22f,z+.5f,z+1.f,10);
 for(float dx:{-1.f,1.f})wall(m,x+dx-.06f,y-.2f,x+dx+.06f,y+.2f,z,z+.43f,2);
}

// SPECIMEN HANDLING (manifest 65). Where specimens were lifted, caged and moved: a handling floor
// three metres below the doors, holding cages along the west wall, isolation bays behind gates on
// the south, and an overhead crane across the middle with cage 7 still on its hook, hanging open.
// The crane log says it was lifted for the theater at 03:02 and opened at height, not by the console.
// Route: N seam from B onto the landing (-15) -> E along the gallery deck to the theater (101); the
// stair down to the floor (-18) for the cages, the bays and what came out of cage 7.
std::shared_ptr<const Map> makeSpecimenHandling(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[99];m.definition=CampaignChunks[99];m.definition.ambient=.28f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'N',10.f,14.f,up,false,0});
 openEdge(m,{'E',9.f,14.f,up,false,0});
 machineryClock(m,"conb_sh_clock",60000);const char* clock="conb_sh_clock";
 // --- The landing and the gallery (-15) -----------------------------------------------------------
 wall(m,8.f,0,17.f,6.f,fl,up,15);
 deck(m,up,17,2,23,15);deck(m,up,23,9,24,14);
 deckSupports(m,m.layers.back(),fl,6.f);
 wall(m,23.f,8.9f,24.f,14.1f,fl,up-.22f,15);                                                  // under the seam: no pit
 m.stairs.push_back({10.f,6.f,11.6f,10.48f,fl,up,16,true,false});
 guardRail(m,8.f,5.9f,10.f,6.f,up);guardRail(m,11.6f,5.9f,17.f,6.f,up);guardRail(m,7.9f,1.f,8.f,6.f,up);
 guardRail(m,17.f,6.f,17.1f,15.f,up);guardRail(m,17.f,14.9f,23.f,15.f,up);guardRail(m,17.f,2.f,23.f,2.1f,up);
 wallSign(m,15.4f,1.06f,up+2.2f,"NO ONE UNDER A SUSPENDED LOAD","CRANE 2 WORKS THE WHOLE FLOOR",2.6f,cbS);
 for(float x:{9.f,17.5f})m.lights.push_back(lamp({x,1.f},up+2.4f,.55f,6.f,3400,0,WorldLightMount::Wall,blS));
 terminal(m,{22.2f,4.2f},"CRANE 2 / LAST LIFTS","03:02 CAGE 7 LIFTED FOR TRANSFER TO THE THEATER. LOAD SWINGING. HELD AT 3.8 M.",
  "03:04 CAGE 7 DOOR OPEN AT HEIGHT. NOT FROM THIS CONSOLE. THE PADLOCK IS STILL ON IT.",up-fl);
 m.terminals.back().yaw=cbW;
 cbFix(m,model::lab_control_board,22.9f,6.2f,up+.95f,cbW,false);
 m.lights.push_back(lamp({23.f,8.f},up+2.3f,.5f,6.f,3400,0,WorldLightMount::Wall,blW));
 m.pickupSpawns.push_back({{20.6f,13.6f},PickupKind::Ammo,up});
 // --- The crane, and cage 7 on its hook --------------------------------------------------------
 wall(m,1.f,11.8f,23.f,12.2f,-9.6f,-9.f,12);
 for(float x:{1.f,22.7f})wall(m,x,1.f,x+.3f,23.f,-9.6f,-9.3f,12);
 wall(m,11.4f,11.55f,12.6f,12.45f,-10.f,-9.6f,8);
 m.fixtures.push_back({model::med_holding_cage,{12.f,12.f},3.8f,4.485f,2.026f,2.013f,0,false});
 for(float x:{11.6f,12.4f}){wall(m,x-.02f,11.98f,x+.02f,12.02f,fl+3.8f+2.01f,-10.f,8);m.structures.back().solid=false;}
 wall(m,14.24f,12.4f,14.28f,13.5f,fl+3.9f,fl+5.7f,2);m.structures.back().solid=false;           // its door, open
 m.lights.push_back(cbGlow({12.f,12.f},fl+5.4f,.4f,5.f,1000,{1.f,.12f,.06f},.2f));               // the hook's warning lamp
 // --- The floor (-18): cages west, bays south, the pool where it is wet and still --------------
 for(float y:{9.5f,12.6f,15.7f})cbFix(m,model::med_holding_cage,3.6f,y,fl,0,true);
 for(float x:{7.f,13.f,19.f})wall(m,x,19.f,x+.2f,23.f,fl,roof,15);
 // bay 1 and 3 gated, bay 2's bars hanging open, bay 4 the loading bay
 for(float cx:{4.1f,16.1f}){const float a=cx==4.1f?1.f:13.2f,b=cx==4.1f?7.f:19.f;
  wall(m,a,18.9f,cx-1.75f,19.1f,fl,roof,15);wall(m,cx+1.75f,18.9f,b,19.1f,fl,roof,15);wall(m,cx-1.75f,18.9f,cx+1.75f,19.1f,fl+2.72f,roof,15);
  cbFix(m,model::lab_cell_gate,cx,19.f,fl,0,false);wall(m,cx-1.75f,18.93f,cx+1.75f,19.07f,fl,fl+2.72f,CollisionProxyMaterial);}
 wall(m,7.2f,18.9f,8.85f,19.1f,fl,roof,15);wall(m,11.35f,18.9f,13.f,19.1f,fl,roof,15);wall(m,8.85f,18.9f,11.35f,19.1f,fl+2.4f,roof,15);
 cbFix(m,model::med_cell_bars_doorway,10.1f,19.f,fl,0,false);
 wall(m,8.85f,18.97f,9.52f,19.03f,fl,fl+2.4f,CollisionProxyMaterial);wall(m,10.68f,18.97f,11.35f,19.03f,fl,fl+2.4f,CollisionProxyMaterial);
 m.lights.push_back(lamp({21.1f,23.f},fl+2.6f,.6f,6.f,3400,0,WorldLightMount::Wall,blN));            // bay 4, the loading bay: warm, the forklift in it
 for(float cx:{4.1f,10.1f,16.1f}){m.lights.push_back(lamp({cx,23.f},fl+2.6f,.65f,6.f,4300,.25f,WorldLightMount::Wall,blN));cbFix(m,model::med_floor_drain,cx,21.f,fl,0,false);}
 forklift(m,21.1f,20.8f,fl);
 for(float x:{19.6f,20.8f})cbFix(m,model::lab_specimen_crate,x,17.2f,fl,x*.1f,true);
 handCart(m,8.2f,15.8f,fl);
 // The transfer line, the floor's reason, one operation end to end under its own sodium: the
 // holding cage the specimens were loaded from at the north end, a floor track and the hoist beam
 // over it, one sealed crate hanging off the hoist above the cart that was waiting for it, the
 // inspection table beside the line, and at the south end, behind bay 3's gate, the freight lift
 // that took them down. Everything else on the floor is under cold service light.
 const float lx=15.4f,cy=12.8f;
 cbFix(m,model::med_holding_cage,lx,8.45f,fl,kPi*.5f,true);
 wall(m,lx-1.03f,6.2f,lx+1.03f,10.7f,fl+2.01f,fl+2.6f,CollisionProxyMaterial);
 rail(m,lx,10.7f,18.8f,fl);
 floorLine(m,lx-1.25f,10.7f,lx-1.13f,18.8f,fl,13);floorLine(m,lx+1.13f,10.7f,lx+1.25f,18.8f,fl,13);
 wall(m,lx-.09f,6.4f,lx+.09f,18.9f,fl+5.1f,fl+5.5f,18);                                           // the hoist beam
 for(float y:{6.6f,10.4f,14.6f,18.6f})m.pipes.push_back({{lx,y},{lx,y},fl+5.5f,.035f,roof,6});
 wall(m,lx-.22f,cy-.3f,lx+.22f,cy+.3f,fl+4.7f,fl+5.1f,19);                                           // the hoist block
 for(float dy:{-.25f,.25f})m.pipes.push_back({{lx,cy+dy},{lx,cy+dy},fl+2.47f,.02f,fl+4.7f,8});      // its chains to the crate
 wall(m,lx-.56f,cy-.56f,lx+.56f,cy+.56f,fl+1.35f,fl+2.47f,10);                                      // the crate, sealed, in the air
 for(float dx:{-.58f,.5f})for(float dy:{-.58f,.5f})wall(m,lx+dx,cy+dy,lx+dx+.08f,cy+dy+.08f,fl+1.33f,fl+2.49f,12);
 for(float d:{-.3f,.3f})wall(m,lx-.58f,cy+d-.03f,lx+.58f,cy+d+.03f,fl+1.33f,fl+2.49f,13);              // its straps
 wall(m,lx-.6f,cy-.6f,lx+.6f,cy+.6f,fl+2.47f,fl+2.52f,13);
 wall(m,lx-.75f,cy-.9f,lx+.75f,cy+.9f,fl+.3f,fl+.42f,11);                                             // the cart, its deck
 for(float dx:{-.6f,.6f})for(float y:{cy-.7f,cy+.7f})m.pipes.push_back({{lx+dx-.08f,y},{lx+dx+.08f,y},fl+.15f,.14f,fl+.15f,2});
 for(float dx:{-.72f,.69f})wall(m,lx+dx,cy-.9f,lx+dx+.03f,cy+.9f,fl+.15f,fl+.3f,13);
 wall(m,lx-.75f,cy-.9f,lx+.75f,cy+.9f,fl,fl+.3f,CollisionProxyMaterial);
 cbFix(m,model::mine_lift,lx+.2f,21.2f,fl,kPi,true);                                                 // the lift, behind bay 3's gate
 m.lights.push_back(cbGlow({lx+.2f,19.5f},fl+3.4f,.8f,5.f,4800));
 cbPendant(m,{lx-.9f,cy},roof,fl+4.3f,2.3f,10.f,1900,0,-.25f);
 cbFix(m,model::med_autopsy_table,12.9f,14.4f,fl,cbE,true);
 m.fixtures.push_back({model::med_corpse_supine,{12.9f,14.4f},fl+.95f-m.layers.front().elevation,.744f,1.753f,.411f,0,false});
 cbFix(m,model::med_crash_cart,11.7f,15.9f,fl,cbE,true);
 kit::prop(m,model::med_microscope,11.6f,15.75f,fl+1.f,.4f);kit::prop(m,model::med_specimen_jar,11.75f,16.1f,fl+1.f,.2f);
 for(float y:{13.7f,15.1f})wall(m,12.42f,y,13.38f,y+.04f,fl+.012f,fl+.016f,16);
 cbPendant(m,{13.6f,14.4f},roof,fl+3.6f,1.9f,10.f,1900,0,-.25f);                                     // the sodium lamp over the line
 // the pool: wet steel, still, humming
 for(float x:{6.6f,8.2f})cbFix(m,model::med_floor_drain,x,13.6f,fl,0,false);
 wall(m,6.f,12.8f,9.4f,14.6f,fl+.002f,fl+.008f,17);
 cbCrust(m,6.4f,13.f,7.2f,13.6f,fl,fl+.04f);cbKnot(m,8.8f,14.f,fl,.5f);
 hazard(m,Hazard::Kind::Anomaly,6.f,12.8f,9.4f,14.6f,fl,fl+1.5f,4);
 cbHum(m,"conb_sh_hum",5.f,11.8f,10.4f,15.6f,fl,fl+3.f,.55f);
 m.lights.push_back(cbGlow({7.6f,13.7f},fl+.4f,.35f,3.f,6500,{.72f,.5f,1.f},.3f));
 // what came out of cage 7, and a handler who stayed at his post
 m.creatureSpawns.push_back({CreatureKind::Transformed,{13.9f,16.8f},fl,1,0});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{10.1f,21.4f},fl,1,0});
 cbBody(m,clock,ActorVisual::Worker,{13.3f,9.3f},fl,1.9f);
 m.pickupSpawns.push_back({{2.2f,18.2f},PickupKind::Health,fl});
 // --- Light: sodium only down the transfer line; the cages under cold service light, one out, the bays green
 cbPendant(m,{5.f,7.4f},roof,fl+5.4f,1.f,9.f,5600);cbPendant(m,{lx-1.4f,8.4f},roof,fl+4.6f,1.2f,8.f,2000,0,-.2f);
 cbPendant(m,{5.f,16.6f},roof,fl+5.4f,0,0,0);cbPendant(m,{lx+.6f,17.4f},roof,fl+4.6f,.9f,8.f,2000,.25f,-.2f);
 m.lights.push_back(lamp({1.f,12.6f},fl+2.6f,.5f,6.f,5000,0,WorldLightMount::Wall,blE));
 // cold service fill: the cage wall, and the floor under the gallery deck, so nothing off the line goes black
 for(float y:{6.6f,18.f})m.lights.push_back(lamp({1.f,y},fl+2.6f,.6f,7.f,5200,0,WorldLightMount::Wall,blE));
 for(float y:{4.5f,11.f})m.lights.push_back(lamp({23.f,y},fl+2.3f,.5f,7.f,5200,0,WorldLightMount::Wall,blW));
 zone(m,"SPECIMEN HANDLING / GALLERY",8.f,0,24.f,15.f,up-.4f,1000,{.13f,.12f,.11f,.008f});
 zone(m,"SPECIMEN HANDLING / BAYS",1.f,19.f,23.f,23.f,-1000,1000,{.12f,.13f,.12f,.008f});
 zone(m,"SPECIMEN HANDLING",0,0,24,24,-1000,1000,{.13f,.12f,.11f,.008f});
 freightFinish(m,-1);
 return p;
}

// ISOLATION WING (manifest 67). Concrete bunkers off one corridor, steel doors with no handle on
// the inside. LAB 4 was put in Isolation 3 at 02:55 on her tier 3 reading and asked for a pen. At
// 03:52 her door opened without a card. Her lamps run down the corridor to it; inside, on the walls,
// her hand: she knows what is happening to her, and she tells the player to follow the lamps.
// In bunker 2 a man who was moved here stands facing the corner and does nothing. No fight.
// Route: W seam from the Quarantine Ward's east passage -> the corridor -> her bunker; back out W.
std::shared_ptr<const Map> makeIsolationWing(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[100];m.definition=CampaignChunks[100];m.definition.ambient=.26f;
 m.definition.playerStart={2.2f,11.5f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=0;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 machineryClock(m,"conb_iso_clock",60000);const char* clock="conb_iso_clock";
 // --- The corridor and the eight bunkers: doors in its two long walls -----------------------------
 const float doors[4][2]={{3.4f,4.6f},{9.4f,10.6f},{15.4f,16.6f},{20.4f,21.6f}};
 // Every bunker door is the same heavy issue and none is in the same state: a steel surround, its
 // number stencilled over it, a wired viewport beside the leaf, drop-bar brackets, a caged lamp. The
 // numbers zig-zag down the corridor: 1 north, 2 south, 3 north (hers), 4 south; 5-8 the spares.
 const char* numbers[2][4]={{"1","6","3","8"},{"5","2","7","4"}};
 // Bunker 2 is the one the corridor is about: its door under one hard red lamp, the viewport
 // widened so you see him standing at it, three restraint bars and a chained hasp across the leaf,
 // and the concrete beside the frame scored by hands, under the detail's warning.
 for(int row=0;row<2;++row){const float y1=row?13.5f:9.3f,y2=y1+.2f,swing=row?1.f:-1.f,face=row?y1:y2,dir=row?-1.f:1.f;float a=0;
  auto onFace=[&](float x1,float x2,float lo,float hi,float out,int mat,bool solid=true){
   wall(m,x1,std::min(face,face+dir*out),x2,std::max(face,face+dir*out),f+lo,f+hi,mat);m.structures.back().solid=solid;};
  for(int k=0;k<4;++k){const float d0=doors[k][0],d1=doors[k][1],cx=(d0+d1)*.5f;const int n=numbers[row][k][0]-'0';
   const float v0=n==2?d0-1.35f:d0-.9f,v1=n==2?d0-.3f:d0-.4f,sill=n==2?1.f:1.3f,head=n==2?1.9f:1.72f;
   wall(m,a,y1,v0,y2,f,roof,15);cbGlaze(m,v0,y1,v1,y2,f,roof,sill,head,true,15);wall(m,v1,y1,d0,y2,f,roof,15);
   const int door=swingDoor(m,d0,d1,(y1+y2)*.5f,f,roof,swing);a=d1;
   if(n==3){m.doors[size_t(door)].open=.9f;m.doors[size_t(door)].opening=true;}                        // 03:52: hers stands open
   if(n==2||n>=5)m.doors[size_t(door)].requireState=stateId("conb_iso_barred");                        // never: the bar is down and nobody is lifting it
   onFace(d0-.18f,d0,0,2.86f,.1f,11);onFace(d1,d1+.18f,0,2.86f,.1f,11);onFace(d0-.18f,d1+.18f,2.55f,2.86f,.1f,11);
   onFace(v0,v1,sill-.06f,sill,.08f,20);
   if(n==2){
    for(float z:{.62f,1.32f,2.02f}){onFace(d0-.3f,d1+.3f,z,z+.11f,.17f,8,false);for(float x:{d0-.42f,d1+.24f})onFace(x,x+.18f,z-.05f,z+.16f,.2f,12);}
    onFace(d1-.16f,d1+.04f,1.05f,1.3f,.24f,19,false);                                                   // the hasp and its padlock
    for(int s2=0;s2<4;++s2)for(int i=0;i<7;++i){const float x=d1+.45f+s2*.13f+i*.07f,z=1.75f-i*.16f-s2*.05f;onFace(x,x+.05f,z,z+.13f,.012f,20,false);}   // scored by hands
    for(int i=0;i<5;++i){const float x=v0+.1f+i*.18f;onFace(x,x+.04f,.45f,.95f-i*.07f,.012f,16,false);}                               // and under the glass
    m.lights.push_back(lamp({cx,face+dir*.03f},f+3.25f,1.7f,5.5f,2000,0,WorldLightMount::Wall,blN,true,{1.f,.07f,.04f}));
    m.fixtures.push_back({model::mine_cage_lamp,{cx,face+dir*.12f},f+3.05f-m.layers.front().elevation,.21f,.2f,.36f,0,false});}
   cbStencil(m,numbers[row][k],cx,face,int(dir),2.98f,.62f);
   for(float x:{d0-.16f,d1+.04f})onFace(x,x+.12f,1.02f,1.26f,.14f,8);                                 // drop-bar brackets
   if(n==2||n>=5)onFace(d0-.24f,d1+.24f,1.08f,1.2f,.12f,8,false);                                     // barred
   if(n==1)onFace(d1+.3f,d1+.38f,0,1.7f,.08f,8);                                                      // its bar lifted off and stood by
   if(n==4){for(float z:{.85f,1.55f})onFace(d0-.1f,d1+.1f,z,z+.08f,.13f,13,false);onFace(d1-.1f,d1+.02f,1.9f,2.5f,.13f,13,false);}   // the seal tape, cut
   if(n==3)for(int i=0;i<5;++i)onFace(d1+.4f,d1+.9f+i*.35f,1.15f+i*.13f,1.17f+i*.13f,.01f,8);        // her pen, on the corridor wall
   m.fixtures.push_back({model::mine_cage_lamp,{d1+.42f,face+dir*.12f},f+2.6f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
   const float amber=n==3?2600.f:n==4?1900.f:4200.f;
   auto l=lamp({d1+.42f,face+dir*.02f},f+2.75f,n==3?.75f:n==4?.45f:.38f,5.5f,amber,0,WorldLightMount::Wall,row?blN:blS,true);
   if(n==7)l.flicker=.45f;if(n!=6&&n!=8&&n!=2)m.lights.push_back(l);}                       // 6 and 8: lamps dead
  wall(m,a,y1,23.f,y2,f,roof,15);}
 for(float x:{6.9f,12.9f,18.9f}){wall(m,x,1.f,x+.2f,9.3f,f,roof,15);wall(m,x,13.7f,x+.2f,23.f,f,roof,15);}
 // The corridor's own structure: steel ribs across the soffit every three metres, a cable tray and
 // two service runs along it, so the long run reads as built, bay by bay, not one concrete tube.
 for(float x=2.5f;x<23.f;x+=3.f){wall(m,x,9.5f,x+.22f,13.5f,roof-.32f,roof,12);for(float y:{9.5f,13.36f})wall(m,x,y,x+.22f,y+.14f,roof-.75f,roof-.32f,12);}
 cableTray(m,{1.f,9.75f},{23.f,9.75f},roof-.24f);
 for(auto [y,paint]:{std::pair{13.15f,3},{12.85f,4}})servicePipe(m,{0.f,y},{23.f,y},roof-.2f,.075f,paint);
 const float bays[4][2]={{1.f,6.9f},{7.1f,12.9f},{13.1f,18.9f},{19.1f,23.f}};
 for(int i=0;i<4;++i){const float a=bays[i][0],b=bays[i][1];
  wall(m,a,1.f,a+2.1f,2.f,f,f+.45f,15);wall(m,a,22.f,a+2.1f,23.f,f,f+.45f,15);                     // the bed slabs
  cbFix(m,model::med_ward_toilet,b-.5f,1.37f,f,cbS,true);cbFix(m,model::med_ward_toilet,b-.5f,22.63f,f,cbN,true);
  // What an isolation cell is issued: a screen wall by the pan, a basin, a drain in the middle of
  // the floor, the extract grille over the bed, the speaker they talk to you through by the door.
  const float mid=(a+b)*.5f;
  wall(m,b-1.15f,1.f,b-1.05f,2.2f,f,f+1.25f,15);wall(m,b-1.15f,21.8f,b-1.05f,23.f,f,f+1.25f,15);
  if(i!=2)cbFix(m,model::med_scrub_sink,b-.3f,2.75f,f,cbW,true);
  cbFix(m,model::med_scrub_sink,b-.3f,21.25f,f,cbW,true);
  cbFix(m,model::med_floor_drain,mid,5.4f,f+.004f,0,false);cbFix(m,model::med_floor_drain,mid,18.f,f+.004f,0,false);
  cbFix(m,model::pwr_vent_grille,a+1.05f,1.03f,f+2.7f,cbS,false);cbFix(m,model::pwr_vent_grille,a+1.05f,22.97f,f+2.7f,cbN,false);
  cbFix(m,model::lab_pa_horn,doors[i][1]+1.1f,9.02f,f+2.5f,cbN,false);cbFix(m,model::lab_pa_horn,doors[i][1]+1.1f,13.98f,f+2.5f,cbS,false);
  if(i!=0&&i!=2)m.fixtures.push_back({model::med_mattress_stained,{a+1.05f,1.5f},.45f,1.9f,.9f,.118f,0,false});
  if(i!=3)m.fixtures.push_back({model::med_mattress_stained,{a+1.05f,22.5f},.45f,1.9f,.9f,.118f,0,false});}
 // What each occupant was left, and left behind when they were moved at 04:10: the stool by the door
 // they sat on to listen, the meal tray pushed back under it, a blanket, the days and hours scored
 // on the wall over the slab, and a clock high on the wall where they could see it and not reach it.
 // (5-8 spare, 1 and 4 dressed below, 3 hers.)
 for(int row=0;row<2;++row)for(int k=0;k<4;++k){const char* n=numbers[row][k];if(*n=='1'||*n=='3'||*n=='4')continue;
  const float a=bays[k][0],b=bays[k][1],door=row?14.6f:8.2f,slab=row?22.5f:1.5f,dir=row?-1.f:1.f,wallY=row?22.99f:1.01f;const unsigned s=cbSeed(a,door);
  if(*n!='2'){cbModel(m,model::office_chair,doors[k][0]-.75f,door+dir*.1f,f,.55f,.55f,.95f,row?.4f:kPi-.3f,true);
   m.clutterSpawns.push_back({1,{doors[k][0]-.6f,door+dir*.75f},f,float(s%628u)*.01f});}
  if(s&1u)cbFix(m,model::scrap_bedroll,a+1.05f,slab,f+.45f,0,false,.95f);
  for(int i=0;i<4+int(s%5u);++i)wall(m,a+.25f+i*.09f,wallY-.006f,a+.28f+i*.09f,wallY+.006f,f+1.15f,f+1.48f,8);
  cbFix(m,model::med_wall_clock,(a+b)*.5f+.6f,row?22.98f:1.02f,f+2.55f,row?cbN:cbS,false,1.4f);}
 // 1 was emptied at 04:10: the mattress pulled off the slab, the chair they took him out in left where it stopped.
 cbFix(m,model::med_wheelchair,4.3f,7.2f,f,.4f,true);cbFix(m,model::med_crutches,1.12f,6.f,f,cbE,false);
 m.fixtures.push_back({model::med_mattress_stained,{2.5f,3.4f},0,1.9f,.9f,.118f,.5f,false});
 kit::prop(m,model::office_paper_sheet,3.6f,6.6f,f,1.1f);
 // 4: he dragged the mattress to the door behind him, and it did not help.
 m.fixtures.push_back({model::med_mattress_stained,{21.9f,15.7f},0,1.9f,.9f,.118f,1.2f,false});cbFix(m,model::scrap_bedroll,22.2f,19.6f,f,.2f,false);
 kit::prop(m,model::item_walkie,21.6f,18.6f,f,.114f,.376f,.046f,2.1f);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,22.3f,17.2f,f,.18f,.4f,.52f,2.6f);
  auto l=cbGlow({22.3f,17.2f},f+.45f,.4f,3.8f,2700);l.prop=lampProp;m.lights.push_back(l);}
 // the corridor: cold tubes, two gone, her lamps on the floor, and red only on door 2
 // every bunker keeps a cage tube of its own, so a look through any open door finds a room, not black
 for(auto [x,y,i,fl]:{std::tuple{4.f,5.f,.6f,.4f},{21.f,4.6f,.45f,0.f},{10.f,5.f,.4f,.2f},{4.f,18.3f,.55f,0.f},{16.f,18.3f,.5f,.3f},{21.f,18.3f,.45f,.15f}})
  m.lights.push_back(cbTube(x,y,roof,i,6.f,4600,.08f,fl));
 for(auto [x,i,fl]:{std::tuple{1.9f,.55f,0.f},{13.f,.5f,.3f},{18.8f,.55f,0.f},{22.f,.5f,.1f}})m.lights.push_back(cbTube(x,11.5f,roof,i,6.5f,5600,.05f,fl));
 cbDeadTube(m,7.f,11.5f,roof);m.lights.push_back(cbGlow({22.6f,11.5f},f+1.3f,.35f,3.f,8000,{.6f,.8f,1.f},.08f));
 for(auto [x,y,yaw]:{std::tuple{3.2f,12.9f,.3f},{8.2f,10.1f,2.4f},{12.6f,12.9f,-.5f},{16.f,10.1f,1.8f},{16.f,7.6f,.9f},{14.4f,3.2f,2.8f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=cbGlow({x,y},f+.45f,.4f,3.8f,2700);l.prop=lampProp;m.lights.push_back(l);}
 terminal(m,{22.4f,11.5f},"ISOLATION WING / OCCUPANCY","1 EMPTY. 2 OCCUPANT DOES NOT ANSWER. 3 LAB 4 (ENERGY DIAGNOSTICS), TIER 3, IN 02:55. 4 SEE CLEANUP.",
  "03:52 DOOR 3 OPENED. NO CARD, NO OPERATOR. OCCUPANT LEFT UNASSISTED. MANUAL NOTE: THESE DOORS HAVE NO HANDLE INSIDE.",0);
 m.terminals.back().yaw=cbW;
 taped(m,{11.75f,13.49f},f+1.95f,cbN,Readable::Note,"ISOLATION / BY ORDER 04:10","OCCUPANTS RELOCATED TO THE HUB FOR TRANSFER. EXCEPT 2. 2 DOES NOT ANSWER.",
  "DO NOT OPEN 2 TO ASK.");
 // --- Isolation 3: hers ------------------------------------------------------------------------
 m.fixtures.push_back({model::med_mattress_stained,{14.15f,1.5f},.45f,1.9f,.9f,.118f,0,false});
 // Her hand, on the concrete, in the pen they left her: big where she wanted it found, on three
 // walls. What she wrote before the pad ran out is the pad's pages, taped up beside it.
 for(auto [x,y,yaw,w,title,sub]:{std::tuple{16.2f,1.012f,cbS,3.f,"I THINK THE HUM IS ME","THE LAMP HUMS WHEN I AM NOT LOOKING AT IT"},
   {18.888f,4.6f,cbW,3.4f,"FOLLOW THE LAMPS","I WILL KEEP LIGHTING THEM AS LONG AS I CAN - L4"},{13.112f,5.2f,cbE,3.4f,"IT OPENS IF I ASK IT","I CAN HEAR THE LOCK THINKING"}}){
  AuthoredSign s;s.position={x,y};s.z=f+1.2f;s.width=w;s.height=1.05f;s.yaw=yaw;s.title=title;s.subtitle=sub;s.style=3;s.accent=0xff1c2440u;s.ink=0xff1c2440u;m.signs.push_back(std::move(s));}
 taped(m,{14.2f,1.03f},f+1.1f,cbS,Readable::Note,"TORN FROM THE INTAKE PAD, IN PEN","THEY TOOK MY BADGE AT THE DOOR AND LEFT ME THE PEN. THE LAMP IN HERE HUMS WHEN I'M NOT LOOKING AT IT.",
  "I THINK THE HUM IS ME.");
 taped(m,{18.88f,7.4f},f+1.4f,cbW,Readable::Note,"TORN FROM THE INTAKE PAD, IN PEN","IF YOU'RE READING THIS YOU CAME DOWN. I SAID GO UP. I KNOW THERE IS NO UP.",
  "FOLLOW THE LAMPS. I'LL KEEP LIGHTING THEM AS LONG AS I CAN. - L4");
 taped(m,{13.12f,7.9f},f+1.4f,cbE,Readable::Note,"TORN FROM THE INTAKE PAD, IN PEN","03:50. MY HANDS ARE WARM ALL THE TIME. I CAN HEAR THE LOCK THINKING.",
  "IT OPENS IF I ASK IT. I'M GOING TO ASK IT.");
 terminal(m,{17.6f,8.6f},"ISOLATION 3 / INTERCOM","02:58 OCCUPANT: CAN SOMEONE TELL MY FRIEND IN SECURITY I'M OK. 03:20 OCCUPANT: I'M NOT OK.",
  "03:51 OCCUPANT: I'M SORRY ABOUT THE LIGHTS. 03:52 DOOR OPEN. CHANNEL CLOSED.",0);
 m.terminals.back().yaw=cbN;
 kit::prop(m,model::item_id_card,16.6f,4.f,f,.132f,.085f,.002f,1.2f);                            // somebody's; not hers
 // She kept it lit: a tube that still works and the lamps she had left, round the walls she wrote on.
 m.lights.push_back(cbTube(16.f,5.f,roof,.55f,6.f,3200,.02f,.12f));
 for(auto [x,y,yaw]:{std::tuple{15.8f,1.5f,.6f},{18.3f,2.4f,2.6f},{18.3f,6.f,-2.2f},{13.6f,7.6f,-.4f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=cbGlow({x,y},f+.45f,.45f,4.f,2700);l.prop=lampProp;m.lights.push_back(l);}
 // --- Isolation 2: he stands in the corner. He was a fitter. He keeps his habits. ----------------
 // He stands at the viewport now, against the glass, rocking; the one tube behind him makes him a shape.
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{8.65f,14.15f},f,cbN,0,0},{9000,{8.65f,14.15f},f,cbN,0,4}},true);
 m.lights.push_back(cbTube(9.6f,17.6f,roof,.5f,5.f,6500,.02f,.4f));
 // --- Isolation 4: the Cleanup man who shut himself in --------------------------------------------
 cbBody(m,clock,ActorVisual::CleanupSharpshooter,{21.f,18.f},f,2.2f);
 m.pickupSpawns.push_back({{20.2f,16.f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{2.6f,4.8f},PickupKind::Health,f});
 recessedFittings(m);
 zone(m,"ISOLATION WING / ISOLATION 3",13.1f,1.f,18.9f,9.3f,-1000,1000,{.16f,.13f,.10f,.008f});
 zone(m,"ISOLATION WING / CORRIDOR",0,9.5f,23.f,13.5f,-1000,1000,{.12f,.12f,.12f,.007f});
 zone(m,"ISOLATION WING",0,0,24,24,-1000,1000,{.12f,.12f,.12f,.008f});
 freightFinish(m,-1);
 return p;
}

// OBSERVATION THEATER (manifest 68). Where the board and Internal Investigations came to watch:
// raked benches from the doors down to a stage, and on the stage the containment cylinder. The
// subject stood up at 02:40 in the middle of demonstration 7 and has not sat down. The audience left
// in a hurry; two did not.
// Route: N seam from the service corridors onto the top landing (-15) -> the west gallery -> W seam
// to Specimen Handling; the aisle stair down the tiers to the stage (-18).
std::shared_ptr<const Map> makeObservationTheater(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[101];m.definition=CampaignChunks[101];m.definition.ambient=.21f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfacePaintedMaroon;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'N',10.f,14.f,up,false,0});
 openEdge(m,{'W',9.f,14.f,up,false,0});
 machineryClock(m,"conb_th_clock",60000);const char* clock="conb_th_clock";
 // --- The landing, the west gallery, the tiers ---------------------------------------------------
 wall(m,1.f,0,23.f,4.f,fl,up,15);
 wall(m,0,4.f,4.f,14.4f,fl,up,15);
 guardRail(m,4.f,4.f,4.1f,14.4f,up);guardRail(m,1.f,14.3f,4.f,14.4f,up);
 cbPortalY(m,1.f,1.4f,9.f,14.f,up,roof,2.8f);
 for(int i=0;i<5;++i){const float y0=4.f+.8f*i,top=up-.5f*(i+1);
  wall(m,4.1f,y0,11.2f,y0+.8f,fl,top,15);wall(m,12.8f,y0,23.f,y0+.8f,fl,top,15);
  for(float x:{5.6f,8.6f,14.4f,17.4f,20.4f})cbBench(m,x,y0+.45f,top);}
 m.stairs.push_back({11.2f,4.f,12.8f,8.f,fl,up,16,true,false});
 // The house reads as seating: a lit nosing on every tier, end standards at the aisle, a handrail
 // down it, and the house lamps on the side walls still on, so the rake is a shape from the stage.
 for(int i=0;i<5;++i){const float y0=4.f+.8f*i,top=up-.5f*(i+1);
  for(auto [x1,x2]:{std::pair{4.1f,11.2f},{12.8f,23.f}})floorLine(m,x1,y0+.72f,x2,y0+.8f,top,14);
  wall(m,12.8f,y0+.1f,12.95f,y0+.7f,top,top+.85f,10);}
 for(float x:{11.3f,12.7f})m.pipes.push_back({{x,4.1f},{x,7.9f},up+.9f,.025f,fl+.9f+.5f,2});
 for(auto [y,z]:{std::pair{4.6f,up+.2f},{6.6f,up-.8f}}){m.lights.push_back(lamp({23.f,y},z+1.6f,.6f,7.f,2800,0,WorldLightMount::Wall,blW));
  m.fixtures.push_back({model::mine_cage_lamp,{22.9f,y},z+1.45f-m.layers.front().elevation,.21f,.2f,.36f,0,false});}
 // the two who did not leave
 cbBody(m,clock,ActorVisual::SeniorScientist,{7.f,6.05f},up-1.5f,.3f);
 cbBody(m,clock,ActorVisual::Scientist,{18.6f,4.45f},up-.5f,2.8f);
 for(float x:{5.f,19.f})m.lights.push_back(lamp({x,1.f},up+2.4f,.35f,5.f,3000,0,WorldLightMount::Wall,blS));
 for(int i=0;i<4;++i){m.lights.push_back(cbGlow({11.05f,4.4f+i*.9f},up-.5f*(i+1)+.15f,.12f,1.8f,2800));}   // the aisle's step lights
 // --- The viewing glass: the audience sat behind it, the stage below. It went at 02:40 in three
 // places, outward, all over the stage floor. The aisle stair comes down through a framed gap.
 const float vg=8.f,gh=fl+4.f;
 cbGlaze(m,4.1f,vg,8.9f,vg+.12f,fl,gh+.35f,1.f,4.f);cbBlownFront(m,8.9f,vg,11.2f,vg+.12f,fl,gh+.35f,1,101,1.f,4.f);
 cbBlownFront(m,12.8f,vg,15.2f,vg+.12f,fl,gh+.35f,1,102,1.f,4.f);cbGlaze(m,15.2f,vg,20.f,vg+.12f,fl,gh+.35f,1.f,4.f);
 cbBlownFront(m,20.f,vg,23.f,vg+.12f,fl,gh+.35f,1,103,1.f,4.f);
 for(float x:{11.1f,12.8f})wall(m,x,vg-.04f,x+.1f,vg+.16f,fl,gh+.35f,20);wall(m,11.2f,vg,12.8f,vg+.12f,gh,gh+.35f,20);
 // What the glass left in the frames: teeth hanging from the head and standing from the sill.
 {unsigned h=1011u;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
  for(auto [x1,x2]:{std::pair{8.95f,11.15f},{12.85f,15.15f},{20.05f,22.95f}})for(float x=x1;x<x2-.08f;x+=.12f+next()*.14f){
   const float w=.05f+next()*.09f,top=.15f+next()*(next()<.3f?1.4f:.55f),low=next()<.6f?.05f+next()*.45f:0.f;
   wall(m,x,vg+.05f,x+w,vg+.07f,gh-top,gh,17);m.structures.back().solid=false;
   if(low>0){wall(m,x,vg+.05f,x+w*.8f,vg+.07f,fl+1.f,fl+1.f+low,17);m.structures.back().solid=false;}}}
 for(int i=0;i<3;++i){const float y0=4.f+.8f*(i+1),top=up-.5f*(i+2);paperDrift(m,4.4f,y0+.05f,10.9f,y0+.25f,top,3,1010+i);paperDrift(m,13.1f,y0+.05f,22.6f,y0+.25f,top,3,1020+i);}
 // --- The stage: raised the width of the room's middle, the cylinder at its centre, lit from the
 // rig: a truss across the volume with its cans on the subject, warm against the cylinder's violet.
 wall(m,7.5f,13.2f,16.5f,19.f,fl,fl+.3f,15);floorLine(m,7.5f,13.2f,16.5f,13.32f,fl+.3f,13);
 for(float x:{8.6f,10.4f,13.6f,15.4f})m.lights.push_back(cbGlow({x,13.05f},fl+.38f,.3f,2.6f,2600));     // the footlights
 wall(m,7.f,11.9f,17.f,12.1f,fl+6.4f,fl+6.7f,12);wall(m,7.f,12.5f,17.f,12.7f,fl+6.4f,fl+6.7f,12);
 for(float x:{7.2f,16.8f})for(float y:{12.f,12.6f})m.pipes.push_back({{x,y},{x,y},fl+6.7f,.03f,roof,6});
 for(float x:{11.f,13.f,14.8f}){wall(m,x-.16f,12.14f,x+.16f,12.46f,fl+5.85f,fl+6.4f,8);
  m.lights.push_back(cbGlow({x,12.6f},fl+5.8f,1.45f,11.f,3300));}
 paperDrift(m,6.f,9.f,10.f,12.8f,fl,6,1031);
 // The vessel: a round steel plinth with a hazard ring, the glass on it, four tie rods up to a round
 // crown that carries its extract and the feed cables to the roof.
 cbPrim(m,"column",12.f,16.f,fl,4.3f,4.3f,.3f,0,15);cbPrim(m,"column",12.f,16.f,fl+.2f,4.36f,4.36f,.07f,0,13,false);
 cbPrim(m,"column",12.f,16.f,fl+.3f,3.9f,3.9f,.012f,0,8,false);
 for(auto [x1,y1,x2,y2]:{std::tuple{10.6f,14.55f,13.4f,14.61f},{10.6f,17.39f,13.4f,17.45f},{10.55f,14.61f,10.61f,17.39f},{13.39f,14.61f,13.45f,17.39f}})
  breakable(m,x1,y1,x2,y2,fl+.3f,fl+3.8f,17,true);
 for(auto [x,y]:{std::pair{10.55f,14.55f},{13.45f,14.55f},{10.55f,17.45f},{13.45f,17.45f}}){m.pipes.push_back({{x,y},{x,y},fl+.3f,.1f,fl+3.8f,5});
  flange(m,{x,y,fl+.4f},{},.1f);flange(m,{x,y,fl+3.7f},{},.1f);}
 wall(m,10.45f,14.45f,13.55f,17.55f,fl+3.8f,fl+3.9f,12);
 cbPrim(m,"column",12.f,16.f,fl+3.9f,4.2f,4.2f,.32f,0,12,false);cbPrim(m,"column",12.f,16.f,fl+3.98f,4.26f,4.26f,.12f,0,13,false);
 cbPrim(m,"dome",12.f,16.f,fl+4.22f,3.4f,3.4f,.7f,0,2,false);
 m.pipes.push_back({{12.f,16.f},{12.f,16.f},fl+4.8f,.32f,roof,2});flange(m,{12.f,16.f,fl+4.9f},{},.32f);
 for(auto [x,y]:{std::pair{10.9f,15.f},{13.1f,17.f}})m.pipes.push_back({{x,y},{x,y},fl+3.86f,.06f,roof,6});
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{12.f,16.f},fl+.3f,cbN,0,0},{9000,{12.f,16.f},fl+.3f,cbN,0,4}},true);   // the subject: he stood up
 m.lights.push_back(cbGlow({12.f,16.f},fl+2.6f,.55f,4.5f,6500,{.72f,.5f,1.f},.25f));
 hazard(m,Hazard::Kind::Anomaly,10.62f,14.62f,13.38f,17.38f,fl+.3f,fl+3.8f,0);
 cbHum(m,"conb_th_hum",7.f,11.f,17.f,21.f,fl,fl+3.f,.6f);
 for(auto [x,y]:{std::pair{9.2f,18.4f},{14.8f,18.4f}})cbPendant(m,{x,y},roof,fl+5.f,1.1f,10.f,6500);
 wall(m,6.f,10.45f,7.1f,10.95f,fl,fl+.5f,10);wall(m,7.1f,10.38f,7.18f,11.02f,fl,fl+.62f,10);          // the lectern, knocked flat
 kit::prop(m,model::office_paper_stack,7.5f,10.4f,fl,.33f,.44f,.03f,.7f);
 kit::propPose(m,model::office_chair,8.7f,12.3f,fl,.4f,0.f,1.45f);                                    // a chair from the front row, thrown
 // One of the rig's cans came down at 02:40 and still burns, on its side on the stage, raking the boards.
 wall(m,8.4f,15.4f,8.9f,15.75f,fl+.3f,fl+.62f,8);wall(m,8.9f,15.45f,8.93f,15.7f,fl+.34f,fl+.58f,20);
 m.lights.push_back(lamp({8.9f,15.57f},fl+.48f,.9f,6.f,3000,0,WorldLightMount::Wall,0,true));
 // The demonstration's instrument bay, east of the stage: the analyser and the recorder that ran
 // the field, their leads over the floor to the vessel, a trolley of meters, the operator's stool
 // kicked back; and up by the glass, the two chairs Investigations had brought down to sit closer.
 cbFix(m,model::lab_analyzer_cabinet,21.82f,13.4f,fl,cbW,true);cbFix(m,model::lab_mainframe,21.8f,16.9f,fl,cbW,true);
 cbFix(m,model::lab_clean_bench,18.9f,15.2f,fl,cbW,true);
 kit::prop(m,model::lab_scope_monitor,18.95f,15.f,fl+.81f,cbW);kit::prop(m,model::lab_field_meter,18.85f,15.6f,fl+.81f,cbW+.3f);
 cbFix(m,model::lab_instrument_terminal,20.6f,19.4f,fl,cbN,true);kit::prop(m,model::lab_scope_monitor,20.6f,19.45f,fl+.68f,cbN);
 kit::propPose(m,model::office_chair,19.6f,17.4f,fl,2.2f,0.f,1.5f);
 for(auto [y,x1]:{std::pair{15.7f,20.6f},{16.25f,20.6f}}){wall(m,16.5f,y-.025f,x1,y+.025f,fl,fl+.03f,8);m.structures.back().solid=false;}
 for(auto [x,y]:{std::pair{13.1f,11.5f},{14.4f,11.6f}})cbFix(m,model::lab_boardroom_chair,x,y,fl,cbS+(x-13.7f)*.3f,true);
 kit::prop(m,model::office_paper_stack,13.75f,11.7f,fl,.33f,.44f,.03f,.4f);
 m.lights.push_back(cbGlow({20.3f,15.f},fl+1.3f,.45f,4.f,9000,{.6f,.8f,1.f},.05f));                       // the scopes, still tracing
 m.lights.push_back(lamp({23.f,15.f},fl+3.2f,.6f,7.f,4300,.25f,WorldLightMount::Wall,blW));
 // Backstage: the cart they wheeled him on, the crates the rig came in.
 cbFix(m,model::lab_specimen_crate,5.f,21.6f,fl,.2f,true);cbFix(m,model::lab_specimen_crate,6.2f,21.9f,fl,-.15f,true,.8f);
 handCart(m,9.2f,21.f,fl);
 m.lights.push_back(lamp({12.f,23.f},fl+2.8f,.45f,6.f,3000,0,WorldLightMount::Wall,blN));
 wall(m,16.f,10.f,19.f,10.7f,fl,fl+.88f,20);wall(m,15.95f,9.95f,19.05f,10.75f,fl+.88f,fl+.92f,28);
 terminal(m,{17.5f,10.35f},"THEATER / DEMONSTRATION 7","AUDIENCE: THE BOARD, AND INTERNAL INVESTIGATIONS. SUBJECT R-31, FORMERLY A FITTER, BAY 2. FIELD STABLE.",
  "02:40 SUBJECT STOOD UP. DEMONSTRATION HALTED. AUDIENCE LEFT BY THE WEST GALLERY. INVESTIGATIONS STAYED TO WATCH.",.92f);
 m.terminals.back().yaw=0;
 kit::prop(m,model::office_chair,17.5f,11.4f,fl,.55f,.55f,.95f,0);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{5.f,20.f},fl,1,0});
 m.pickupSpawns.push_back({{21.6f,21.8f},PickupKind::Health,fl});m.pickupSpawns.push_back({{2.4f,12.6f},PickupKind::HandgunAmmo,up});
 m.lights.push_back(lamp({1.f,20.f},fl+2.6f,.3f,5.f,1000,-.45f,WorldLightMount::Wall,blE));
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,23.f},fl+2.8f,.55f,7.f,3000,0,WorldLightMount::Wall,blN));
 zone(m,"OBSERVATION THEATER / GALLERY",0,0,24.f,14.4f,up-.4f,1000,{.12f,.11f,.12f,.01f});
 zone(m,"OBSERVATION THEATER / STAGE",4.f,8.f,23.f,23.f,-1000,fl+3.f,{.13f,.12f,.13f,.008f});
 zone(m,"OBSERVATION THEATER",0,0,24,24,-1000,1000,{.12f,.11f,.12f,.008f});
 freightFinish(m,-1);
 return p;
}

// BREACH SECTOR (manifest 69). Ground zero: the primary containment hall. At 02:00 the middle of its
// floor went down three metres into the rock under it and the rock came back up through the hole:
// bedrock heaved against the walls, black stone from further down standing in it like teeth, the
// growth over all of it, and at the centre the spire, lit from inside. What is left of the floor is
// two ledges, east and south, with the slab's broken edge and its steel hanging off them; the plant
// that stood on the rest is in the hole. The Cleanup Force held the east ledge until 03:05 and left
// a note on how they lost it. The tall one stands under that ledge with its back to the door.
// The beat: it does not move while the Force's floodlight is on it. Step off the slab onto the
// crater floor and the flood blows [conb_br_dark]; it wakes, and the crossing is made in the spire's
// pulse, between the knots, looking at it.
// Route: E seam from B onto the east ledge (-15) -> down the slid slab into the crater (-18) ->
// across between the knots -> up the scree to the south ledge -> S seam to Emergency Containment.
std::shared_ptr<const Map> makeBreachSector(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[102];m.definition=CampaignChunks[102];m.definition.ambient=.15f;
 m.definition.playerStart={22.f,11.5f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceRock;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'E',9.f,14.f,up,false,0});
 openEdge(m,{'S',10.f,14.f,up,false,0});
 machineryClock(m,"conb_br_clock",60000);const char* clock="conb_br_clock";
 const StateId dark=stateId("conb_br_dark");
 // The crater's rock. The heaved bedrock against the sides (batters) and the scree (ramps) keep their
 // own faces; a dome is the black rock come up from below, the collision of the boulders of the cave
 // pack laid over it in pieces near their own proportions.
 auto heap=[&](float x,float y,float z,float w,float d,float h,bool round){
  // the rock's own finish in faces leaning back from a ridge, so it takes the crater's light
  (void)round;cbCrag(m,x,y,z-.1f,w*1.04f,d*1.04f,h+.1f,cbSeed(x*1.7f,y+z*.3f));
 };
 auto rock=[&](const char* kind,float x,float y,float z,float w,float d,float h,int facing,bool solid=true){
  if(std::string(kind)!="dome"){cbPrim(m,kind,x,y,z,w,d,h,facing,26,solid);return;}
  if(solid)cbPrim(m,kind,x,y,z,w,d,h,facing,CollisionProxyMaterial,true);
  heap(x,y,z,w,d,h,std::string(kind)=="dome");};
 // A stone of the black rock that came up: the pack model, and a core to walk into.
 auto stone=[&](int mdl,float x,float y,float z,float s,float yaw,bool block=true){const auto& v=model::size[mdl];cbModel(m,mdl,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw);
  if(block){const float r=std::min(v[0],v[1])*s*.34f;wall(m,x-r,y-r,x+r,y+r,z,z+v[2]*s*.8f,CollisionProxyMaterial);}};
 // Plant that went down with the floor: the model sunk into the crater, a core to walk into.
 auto sunk=[&](int mdl,float x,float y,float sink,float yaw,float hx,float hy){const auto& v=model::size[mdl];cbModel(m,mdl,x,y,fl-sink,v[0],v[1],v[2],yaw);
  wall(m,x-hx,y-hy,x+hx,y+hy,fl,fl-sink+v[2],CollisionProxyMaterial);};
 // Crust on a floor: the rock's film over the crater floor, cracked and wet, the growth in its cracks.
 auto crust=[&](float x,float y,float w,float d,float z){cbCrust(m,x-w*.5f,y-d*.5f,x+w*.5f,y+d*.5f,z,z+.03f);};
 // --- What is left of the sector floor: the east ledge, the south ledge, the lift core between ---
 wall(m,18.f,1.f,24.f,17.f,fl,up,15);wall(m,1.f,19.f,18.f,24.f,fl,up,15);
 wall(m,18.f,17.f,23.f,23.f,fl,roof,15);
 // --- The crater's sides: bedrock heaved back against the walls and under the ledges, never one
 // straight face; the slab's broken edge shows over it, east and south. (a..b along the side, how
 // far it stands out, how high.)
 for(auto [a,b,d,h]:{std::tuple{1.f,4.2f,2.2f,3.6f},{4.2f,7.f,1.5f,3.f},{7.f,10.5f,2.f,3.3f},{10.5f,13.2f,1.3f,2.9f},{13.2f,16.f,1.9f,3.4f},{16.f,18.f,1.4f,3.1f}})rock("batter",(a+b)*.5f,1.f+d*.5f,fl,b-a,d,h,0);
 for(auto [a,b,d,h]:{std::tuple{3.f,5.5f,1.8f,3.4f},{5.5f,8.4f,1.3f,3.f},{8.4f,11.6f,2.1f,3.7f},{11.6f,14.6f,1.4f,3.1f},{14.6f,19.f,1.9f,3.3f}})rock("batter",1.f+d*.5f,(a+b)*.5f,fl,b-a,d,h,1);
 for(auto [a,b,d,h]:{std::tuple{1.f,4.f,1.6f,2.6f},{4.f,7.f,1.1f,2.4f},{7.f,9.7f,1.5f,2.7f},{11.9f,14.4f,1.2f,2.5f},{14.4f,17.f,1.7f,2.7f},{17.f,19.f,1.3f,3.f}})rock("batter",18.f-d*.5f,(a+b)*.5f,fl,b-a,d,h,3);
 for(auto [a,b,d,h]:{std::tuple{1.f,4.9f,1.5f,2.7f},{6.7f,9.5f,1.3f,2.5f},{9.5f,12.6f,1.8f,2.7f},{12.6f,15.4f,1.2f,2.4f},{15.4f,18.f,1.6f,2.7f}})rock("batter",(a+b)*.5f,19.f-d*.5f,fl,b-a,d,h,2);
 for(auto [x,y,w,d,h]:{std::tuple{3.4f,3.4f,3.4f,2.8f,2.2f},{16.4f,3.f,3.f,2.4f,1.7f},{3.f,17.f,2.6f,2.4f,1.6f},{16.6f,16.4f,2.2f,2.f,1.5f},{12.2f,2.9f,2.2f,1.8f,1.1f},
  {2.9f,9.9f,2.4f,3.f,1.f},{8.4f,17.9f,2.6f,1.9f,.9f},{3.9f,15.3f,2.2f,1.9f,.8f}})rock("dome",x,y,fl,w,d,h,0);
 // What the floor became: slabs of it, tipped in against the sides where they slid.
 for(auto [x,y,w,d,h,fc]:{std::tuple{16.3f,7.9f,2.6f,2.9f,2.2f,1},{16.2f,13.4f,2.2f,2.5f,1.7f,1},{14.9f,17.5f,2.2f,2.5f,1.9f,0},{10.f,17.8f,1.8f,2.1f,1.4f,0},{3.1f,7.3f,2.4f,2.6f,1.6f,3}})
  cbPrim(m,"ramp",x,y,fl-.12f,w,d,h,fc,42);
 // the slab edge: lips hanging off the ledges, the bar out of the concrete
 for(auto [x,y,w,d,h,fc]:{std::tuple{17.3f,5.7f,1.7f,1.4f,1.1f,1},{17.4f,15.4f,2.f,1.2f,.9f,1},{8.6f,18.4f,1.8f,1.2f,1.f,0},{16.2f,18.5f,1.9f,1.f,.8f,0}})cbPrim(m,"ramp",x,y,up-h,w,d,h,fc,42,false);
 for(auto [y,l,drop]:{std::tuple{2.6f,.7f,.3f},{3.1f,.5f,.15f},{8.3f,.5f,.1f},{8.7f,.8f,.4f},{12.6f,.9f,.5f},{13.f,.6f,.2f},{16.1f,.6f,.2f}})cbPrim(m,"strut",18.f-l*.5f,y,up-.14f-drop,.03f,l,drop+.03f,1,8,false);
 for(auto [x,l,drop]:{std::tuple{7.6f,.6f,.3f},{8.f,.8f,.45f},{11.3f,.8f,.5f},{13.8f,.5f,.1f},{14.2f,.7f,.3f},{17.1f,.7f,.4f}})cbPrim(m,"strut",x,19.f-l*.5f,up-.14f-drop,.03f,l,drop+.03f,0,8,false);
 // --- The way down: the slab that slid, one end still on the ledge; its rail on one side only -----
 cbPrim(m,"ramp",15.2f,10.8f,fl,1.6f,5.6f,3.f,1,42);
 cbPrim(m,"strut",15.2f,10.03f,fl+.92f,.05f,5.6f,3.05f,1,12,false);
 for(float x:{13.f,14.8f,16.6f}){const float z=fl+(x-12.4f)/5.6f*3.f;wall(m,x-.02f,10.01f,x+.02f,10.05f,z-.1f,z+.96f,12);m.structures.back().solid=false;}
 floorLine(m,18.05f,10.f,18.2f,11.6f,up,13);
 // --- The way out: the scree up to the south ledge, the Force's hand line along it ----------------
 rock("ramp",5.8f,16.6f,fl,1.6f,4.8f,3.f,0);
 rock("ramp",4.3f,17.3f,fl,1.4f,3.4f,1.9f,0);rock("ramp",7.3f,17.6f,fl,1.4f,2.8f,1.5f,0);rock("dome",5.9f,14.3f,fl,3.6f,1.6f,.45f,0,false);
 m.pipes.push_back({{6.64f,14.3f},{6.64f,19.1f},fl+1.f,.018f,up+1.f,3});
 for(float y:{14.3f,16.7f,19.1f}){const float z=fl+std::clamp((y-14.2f)/4.8f,0.f,1.f)*3.f;m.pipes.push_back({{6.64f,y},{6.64f,y},z,.022f,z+1.f,2});}
 // --- At the bottom, the source: the heave, the black rock standing out of it, the spire ----------
 const float sx=8.6f,sy=7.2f;
 rock("dome",sx,sy,fl,5.4f,4.6f,1.5f,0);
 for(auto [dx,dy,w,d,h]:{std::tuple{-.9f,.5f,2.4f,2.2f,1.9f},{1.f,.2f,2.6f,2.f,2.3f},{.1f,-1.f,2.2f,1.8f,1.7f},{.2f,1.3f,1.8f,1.4f,1.5f}})cbGrowth(m,sx+dx,sy+dy,fl,w,d,h*.8f);
 // The spire: the tallest prisms, lit through, out of the heave's crown; round them the rest of it,
 // shorter and leaning further out the lower they stand, and the clusters at its foot.
 // (modelled clusters: the hero knot scaled up out of the crown, two more leaning off it, smaller round its foot)
 cbCrystal(m,model::res_crystal_l,sx,sy,fl+1.f,2.6f,.3f);
 cbCrystal(m,model::res_crystal_l,sx-.75f,sy+.55f,fl+.85f,1.55f,2.4f);
 cbCrystal(m,model::res_crystal_m,sx+.8f,sy-.5f,fl+.9f,2.1f,4.1f);
 for(auto [dx,dy,h,yaw]:{std::tuple{1.15f,.6f,1.1f,1.f},{-1.2f,-.4f,.95f,3.3f},{.3f,-1.25f,.8f,5.f},{-.2f,1.3f,.75f,.6f}})cbCrystalTo(m,sx+dx,sy+dy,fl+.6f,h,yaw);
 for(auto [dx,dy,h]:{std::tuple{-2.3f,.7f,.9f},{3.1f,-.7f,1.1f},{.7f,-2.1f,.8f},{-1.f,2.1f,1.f},{1.9f,1.7f,.6f}})cbKnot(m,sx+dx,sy+dy,fl,h);
 for(auto [dx,dy,sc,z,yaw]:{std::tuple{-1.5f,.9f,1.6f,1.f,.4f},{1.6f,.2f,1.4f,1.1f,1.3f},{.9f,-1.5f,1.3f,1.f,2.2f},{-1.2f,-1.3f,1.5f,.9f,2.9f},{.1f,1.8f,1.2f,.9f,3.6f},{2.3f,1.3f,1.1f,.2f,.9f},{-2.4f,-.2f,1.2f,.25f,2.f}})
  cbBed(m,sx+dx,sy+dy,fl+z,sc,yaw);
 wall(m,sx-1.f,sy-1.f,sx+1.f,sy+1.f,fl,fl+8.f,CollisionProxyMaterial);wall(m,9.6f,7.05f,11.5f,7.55f,fl,fl+3.f,CollisionProxyMaterial);
 stone(model::cave_stalagmite_cluster,11.7f,5.6f,fl,.85f,.6f,false);stone(model::cave_stalagmite_cluster,5.9f,9.f,fl,.75f,2.3f,false);
 m.lights.push_back(cbGlow({sx,sy+1.7f},fl+2.4f,2.5f,18.f,6500,{.72f,.5f,1.f},.12f));
 m.lights.push_back(cbGlow({sx,sy-.8f},fl+9.f,1.3f,12.f,6500,{.6f,.45f,1.f},.08f));
 m.lights.push_back(cbGlow({sx+2.7f,sy-.9f},fl+1.f,1.5f,9.f,7000,{.45f,.6f,1.f},.1f));               // its base, cold
 {auto l=cbGlow({sx-.4f,sy+1.9f},fl+3.4f,3.4f,17.f,6500,{.8f,.6f,1.f});l.enabledState=dark;l.period=3.2f;l.onTime=.45f;m.lights.push_back(l);}   // its pulse, once the flood is gone
 hazard(m,Hazard::Kind::Anomaly,6.2f,4.8f,11.f,9.6f,fl,fl+3.f,0);hazard(m,Hazard::Kind::Anomaly,7.f,5.6f,10.2f,8.8f,fl,fl+3.f,14);
 // --- The crater floor: rock, and the crust over it in blisters; mounds, knots, the black stones ---
 for(auto [x,y,w,d]:{std::tuple{8.6f,7.4f,9.5f,8.f},{5.f,11.6f,5.4f,4.4f},{10.4f,13.4f,5.6f,3.8f},{13.2f,4.8f,5.f,3.4f},{4.4f,15.8f,3.6f,3.6f},{13.8f,15.8f,3.6f,3.2f},{12.9f,9.4f,2.8f,2.4f},{2.6f,4.4f,2.4f,3.f},{15.6f,10.6f,1.8f,2.6f}})crust(x,y,w,d,fl);
 for(auto [x,y,r,h]:{std::tuple{5.6f,7.7f,1.1f,1.f},{10.9f,3.9f,.9f,.7f},{3.7f,13.5f,1.f,1.2f},{12.9f,17.2f,.9f,.7f},{10.f,16.4f,.7f,.5f}})cbMound(m,x,y,fl,r,h);
 stone(model::cave_boulder_a,13.f,7.9f,fl-.15f,.55f,1.1f);stone(model::cave_boulder_round,14.4f,3.6f,fl-.2f,.6f,.4f);
 stone(model::cave_rock_block,8.2f,15.3f,fl-.2f,.5f,.8f);stone(model::cave_boulder_round,2.3f,12.6f,fl-.3f,.7f,1.4f);
 stone(model::cave_rock_block,5.4f,2.3f,fl+.3f,1.1f,.2f,false);wall(m,4.f,1.3f,6.8f,3.3f,fl,fl+2.8f,CollisionProxyMaterial);
 // the growth up the walls it reached: the rock heaved up the faces over the heave, and over that the
 // concrete cracked toward the roof, the growth in the cracks and the wet run down out of them
 for(auto [x,z2]:{std::pair{2.1f,5.2f},{3.6f,6.8f},{5.3f,4.6f},{6.4f,7.4f},{12.4f,6.2f},{13.6f,4.4f},{15.1f,5.6f},{16.6f,4.2f}})cbCrack(m,true,1.004f,x,fl+2.9f,fl+z2,cbSeed(x,z2+.5f));
 for(auto [y,z2]:{std::pair{3.6f,5.8f},{5.2f,4.4f},{6.6f,7.f},{7.9f,5.4f},{12.6f,6.4f},{14.f,4.8f},{15.6f,6.f},{17.2f,4.2f}})cbCrack(m,false,1.004f,y,fl+3.f,fl+z2,cbSeed(z2+.5f,y));
 // behind the spire, where it came up hardest, the cracks run nearly to the roof
 for(auto [x,z2]:{std::pair{8.4f,9.6f},{9.1f,12.f},{10.3f,11.2f},{11.4f,8.8f}})cbCrack(m,true,1.004f,x,fl+3.f,std::min(roof-.3f,fl+z2),cbSeed(x,z2));
 for(auto [y,z2]:{std::pair{9.2f,9.4f},{10.f,12.2f},{11.2f,10.6f}})cbCrack(m,false,1.004f,y,fl+3.1f,std::min(roof-.3f,fl+z2),cbSeed(z2,y));
 const Vec2 knots[4]={{10.6f,13.9f},{8.2f,11.4f},{11.5f,8.f},{3.3f,15.9f}};
 for(int i=0;i<4;++i){const Vec2 k=knots[i];cbKnot(m,k.x,k.y,fl,1.2f+.3f*(i%2));
  hazard(m,Hazard::Kind::Anomaly,k.x-1.4f,k.y-1.4f,k.x+1.4f,k.y+1.4f,fl,fl+2.4f,0);                // the shimmer first
  hazard(m,Hazard::Kind::Anomaly,k.x-.7f,k.y-.7f,k.x+.7f,k.y+.7f,fl,fl+2.4f,10);}                 // then it bites
 cbHum(m,"conb_br_hum",1.f,1.f,18.f,19.f,fl,fl+4.f,.7f);
 // --- The plant that stood on the floor: in the hole, sunk to its waist ---------------------------
 sunk(model::lab_mainframe,4.5f,5.1f,1.2f,.45f,1.f,1.f);
 sunk(model::pwr_switchgear_cabinet,6.3f,3.7f,.9f,-.3f,.7f,.45f);
 sunk(model::pwr_turbine_housing,15.3f,5.4f,.7f,.6f,1.1f,1.1f);
 sunk(model::med_holding_cage,4.8f,11.f,.5f,.3f,1.9f,.95f);                                           // a holding cage, its bars sprung
 sunk(model::lab_cell_gate,12.1f,16.9f,.9f,.5f,1.4f,.25f);
 sunk(model::pwr_catwalk_section,12.6f,4.5f,.3f,.25f,1.6f,.8f);sunk(model::pwr_catwalk_section,14.7f,7.7f,.45f,1.9f,.8f,1.6f);   // the north gallery, where it landed
 cbModel(m,model::pwr_coolant_main,14.f,3.5f,fl+.05f,.5f,7.26f,.5f,kPi*.5f);
 // The north gallery ran along the wall four metres over the floor. Its brackets are still in the
 // concrete; the corner bay still hangs, the rest is in the hole under the cable tray that came down burning.
 for(float x:{3.f,6.f,9.f,12.f})cbPrim(m,"girder",x,1.6f,-11.95f,.3f,1.2f,.35f,0,12,false);
 wall(m,14.2f,1.f,23.f,2.3f,-11.6f,-11.5f,46);wall(m,21.7f,2.3f,23.f,8.4f,-11.6f,-11.5f,46);
 guardRail(m,14.2f,2.22f,21.7f,2.3f,-11.5f);guardRail(m,21.7f,2.3f,21.78f,8.4f,-11.5f);
 for(float x:{15.f,18.f,21.f})cbPrim(m,"girder",x,1.6f,-11.95f,.3f,1.2f,.35f,0,12,false);
 for(float y:{4.f,7.f})cbPrim(m,"girder",22.4f,y,-11.95f,.3f,1.2f,.35f,3,12,false);
 cbPrim(m,"strut",13.4f,1.65f,-13.7f,.14f,1.6f,2.2f,1,12,false);cbPrim(m,"strut",22.3f,9.1f,-12.7f,.14f,1.4f,1.2f,2,12,false);   // its torn ends hanging
 cbModel(m,model::pwr_cable_bundle,8.9f,3.3f,fl+.03f,4.f,.14f,.22f,.05f);cbModel(m,model::pwr_cable_bundle,9.5f,2.7f,fl+.55f,4.f,.14f,.22f,-.08f);
 hazard(m,Hazard::Kind::Fire,7.2f,3.f,10.6f,3.6f,fl,fl+1.4f,10);
 m.lights.push_back(cbGlow({8.9f,3.9f},fl+.9f,.8f,7.f,1800,{1,1,1},.3f));
 // one high-bay still burns on its drop over the plant; two hang dead
 cbPendant(m,{5.6f,6.4f},roof,fl+3.f,2.4f,9.f,1900,.12f,-.2f);cbPendant(m,{14.f,6.2f},roof,fl+3.6f,0,0,0);cbPendant(m,{10.6f,15.6f},roof,fl+4.2f,0,0,0);
 m.lights.push_back(cbGlow({10.f,10.5f},-9.5f,4.6f,19.f,7500));                                       // the hole in the roof the light comes down
 // the crystal's own light thrown about the crater floor, cold, so the black rock it came up with
 // shows its shape against the beige of the heave: low fills off the clusters, no shadows of their own
 for(auto [x,y]:{std::pair{5.2f,12.4f},{12.6f,14.6f}})m.lights.push_back(cbGlow({x,y},fl+1.3f,1.15f,9.5f,9000,{.55f,.85f,1.f},.04f));
 m.lights.push_back(cbGlow({4.4f,10.6f},fl+1.75f,.45f,4.f,6500,{.72f,.5f,1.f},.3f));                  // in the cage
 // --- The tall one. It stands under the east ledge, its back to you, where the flood throws its
 // shadow across the floor; it does not move until the flood is gone.
 m.creatureSpawns.push_back({CreatureKind::Warden,{14.2f,15.f},fl,1,0,dark});
 {ScriptAction shake;shake.type=ScriptAction::Type::Shake;shake.amount=.35f;
  ScriptAction sting;sting.type=ScriptAction::Type::PlaySound;sting.sound=Sound::Sting;sting.amount=.7f;
  ScriptEvent e;e.id=stateId("conb_br_descent");e.x1=1.f;e.y1=1.f;e.x2=18.f;e.y2=19.f;e.bottom=-100;e.top=fl+1.2f;       // however she comes down
  e.actions={cbSet("conb_br_dark",1),shake,sting};m.scriptEvents.push_back(std::move(e));}
 m.ambients.push_back({Sound::OneMetalBang,{18.7f,13.2f},up+2.4f,.6f,22,AmbientEmitter::OnState,8,8,dark,1});
 m.ambients.push_back({Sound::OneMonsterGroan,{14.2f,15.f},fl+1.6f,.6f,22,AmbientEmitter::OnState,8,8,dark,1});
 cbBody(m,clock,ActorVisual::Cleanup,{13.3f,12.5f},fl,1.2f);cbBody(m,clock,ActorVisual::CleanupHeavy,{6.4f,12.5f},fl,2.9f);
 cbBody(m,clock,ActorVisual::Scientist,{12.9f,15.3f},fl,.2f);
 m.pickupSpawns.push_back({{7.f,9.9f},PickupKind::Health,fl});
 // --- The east ledge: where the Force held, and what it left ---------------------------------------
 cbPortalY(m,22.6f,23.f,9.f,14.f,up,roof,3.2f);
 for(float y:{3.f,6.f,16.f})wall(m,22.78f,y-.25f,23.f,y+.25f,up,roof,15);
 guardRail(m,17.92f,1.2f,18.f,4.9f,up);guardRail(m,17.92f,6.6f,18.f,10.f,up);guardRail(m,17.92f,11.6f,18.f,12.8f,up);
 // the floodlight on its stand at the edge, thrown down across the crater
 for(float y:{13.f,13.3f})wall(m,19.f,y,19.08f,y+.08f,up,up+2.3f,12);wall(m,18.72f,12.9f,19.04f,13.5f,up,up+.06f,8);
 wall(m,18.7f,12.95f,19.05f,13.45f,up+2.3f,up+2.7f,8);wall(m,18.66f,13.f,18.7f,13.4f,up+2.34f,up+2.66f,20);
 {auto l=lamp({18.75f,13.2f},up+2.5f,1.7f,20.f,4600,0,WorldLightMount::Wall,blW,true);l.enabledState=dark;l.invert=true;m.lights.push_back(l);}
 looseLead(m,{19.12f,13.2f,up+2.4f},{20.6f,14.6f,up},3.8f,7,false,.012f);
 cbFix(m,model::mine_portable_generator,20.9f,14.9f,up,.4f,true);
 crewBarrier(m,20.3f,12.7f,up,1.4f);
 taped(m,{20.3f,12.7f},up+.75f,cbN,Readable::Note,"SECTOR LOST / 03:05","THE TALL ONE DOES NOT MOVE WHILE YOU LOOK AT IT. SO KEEP LOOKING AT IT.",
  "IT COMES WHEN YOU RELOAD. IT COMES AT THE LIGHT. DON'T DO EITHER. - REYES");
 // the north end: four hours of holding a rail
 for(auto [x,y,yaw]:{std::tuple{19.1f,2.f,.1f},{20.3f,1.9f,-.15f},{18.9f,5.6f,1.5f}})cbFix(m,model::lab_specimen_crate,x,y,up,yaw,true);
 cbFix(m,model::scrap_bedroll,22.1f,3.6f,up,.1f,false);cbFix(m,model::scrap_bedroll,22.f,6.3f,up,-.2f,false);
 kit::prop(m,model::item_walkie,20.3f,1.9f,up+1.12f,.114f,.376f,.046f,.6f);kit::prop(m,model::mine_hard_hat,21.2f,5.f,up,1.1f);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,19.1f,2.f,up+1.12f,.18f,.4f,.52f,2.4f);
  auto l=cbGlow({19.1f,2.2f},up+1.5f,.8f,7.f,3000);l.prop=lampProp;m.lights.push_back(l);}
 cbBody(m,clock,ActorVisual::CleanupSharpshooter,{21.2f,2.9f},up,2.2f);
 cbFix(m,model::med_first_aid_box,22.94f,7.6f,up+1.2f,cbW,false);
 m.pickupSpawns.push_back({{21.6f,15.6f},PickupKind::Ammo,up});m.pickupSpawns.push_back({{19.6f,3.4f},PickupKind::HandgunAmmo,up});
 crust(19.3f,7.6f,2.2f,3.f,up);
 // the lift core: its shutter came down at 02:00 and has not moved
 trackPortal(m,20.5f,16.8f,up,1.f);
 cbFix(m,model::lab_status_display,20.5f,16.96f,up+5.05f,cbN,false);
 // --- The south ledge: the top of the scree, the way on --------------------------------------------
 wall(m,9.4f,22.4f,10.f,23.f,up,roof,15);wall(m,14.f,22.4f,14.6f,23.f,up,roof,15);wall(m,10.f,22.4f,14.f,23.f,up+3.2f,roof,15);
 floorLine(m,10.f,22.1f,14.f,22.3f,up,13);
 cbFix(m,model::urb_jersey_barrier,3.76f,19.7f,up,0,true);cbFix(m,model::urb_jersey_broken,7.84f,19.7f,up,0,true);
 guardRail(m,9.4f,19.f,12.2f,19.08f,up);guardRail(m,14.6f,19.f,18.f,19.08f,up);
 cbMound(m,2.3f,21.6f,up,1.f,.9f);crust(3.2f,21.2f,3.8f,3.f,up);
 cbFix(m,model::pwr_generator_unit,16.2f,21.6f,up,.2f,true);cbFix(m,model::pwr_step_transformer,15.f,22.3f,up,0,true);
 cbFix(m,model::lab_status_display,18.f-.04f,20.1f,up+2.f,cbW,false);
 m.lights.push_back(lamp({18.f,21.3f},up+2.4f,.55f,6.f,4200,0,WorldLightMount::Wall,blW));             // the ledge's own lamp on the lift core
 m.fixtures.push_back({model::mine_cage_lamp,{17.9f,21.3f},up+2.2f-m.layers.front().elevation,.21f,.2f,.36f,cbW,false});
 m.pickupSpawns.push_back({{2.8f,20.2f},PickupKind::Health,up});
 {ScriptAction save;save.type=ScriptAction::Type::Checkpoint;cbEvent(m,"conb_br_out",9.5f,20.6f,14.5f,23.6f,"conb_br_dark",{save});}
 // --- Light: the Force's work lamps mark the line; red on the walls; the way on is the one warm door
 for(auto [x,y,z]:{std::tuple{18.5f,12.2f,up},{12.f,12.f,fl},{9.3f,12.9f,fl},{6.95f,13.9f,fl},{7.1f,20.6f,up}}){
  m.fixtures.push_back({model::mine_cage_lamp,{x,y},z-m.layers.front().elevation,.21f,.2f,.36f,0,false});
  m.lights.push_back(cbGlow({x,y},z+.3f,.65f,6.f,3000));}
 for(auto [x,y,yaw,ph]:{std::tuple{23.f,4.6f,blW,0.f},{23.f,15.6f,blW,.9f},{16.6f,23.f,blN,.4f},{18.f,21.5f,blW,1.3f}})m.lights.push_back(cbAlarm({x,y},up+2.4f,yaw,ph));
 m.lights.push_back(lamp({12.f,22.4f},up+3.5f,.8f,7.f,3200,0,WorldLightMount::Wall,blN));
 zone(m,"BREACH SECTOR / CRATER",1.f,1.f,18.f,19.f,-1000,up-.5f,{.11f,.10f,.12f,.006f});
 zone(m,"BREACH SECTOR",0,0,24,24,-1000,1000,{.11f,.10f,.11f,.006f});
 freightFinish(m,-1);
 return p;
}

// EMERGENCY CONTAINMENT (manifest 70). The last line between the sector and Deep Research: welded
// shutters, two lines of barricade, a room welded shut with something still in it, and the
// barricade itself lowered over the south door at 03:20. The control room raises it. The Emergency
// detail has been on that door since three; the Investigator tells them nothing goes down. They
// will go with the player, if asked.
// Route: N seam from the Breach Sector -> through the shutters' gap -> round the second line (west)
// -> the control room (east): raise the barricade [cont_barricade] -> the S door to 104.
std::shared_ptr<const Map> makeEmergencyContainment(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[103];m.definition=CampaignChunks[103];m.definition.ambient=.27f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'S',10.f,14.f,f,true,stateId("cont_barricade")});                                  // door 0: the barricade
 openEdge(m,{'N',10.f,14.f,f,false,0});
 machineryClock(m,"conb_ec_clock",60000);const char* clock="conb_ec_clock";
 const StateId stance=stateId("conb_ec_stance");
 // --- Line A: welded shutters across the hall, a gap cut at the middle --------------------------
 // Checkpoint A, failed: a wall across the hall, shutters to head height and block above, and the
 // lockdown door at its middle sealed to the floor, its beacon the only red you see from the north.
 // Its console still runs and still refuses. East, what did not wait for the door tore its own way
 // through panel 6 and the block over it: that hole is the way on.
 wall(m,1.f,6.2f,10.5f,6.5f,f,f+2.6f,2);wall(m,13.5f,6.2f,19.6f,6.5f,f,f+2.6f,2);wall(m,21.1f,6.2f,23.f,6.5f,f,f+2.6f,2);
 for(auto [x1,x2,lo]:{std::tuple{1.f,10.5f,2.6f},{10.5f,13.5f,3.4f},{13.5f,19.6f,2.6f},{21.1f,23.f,2.6f},{19.6f,21.1f,2.55f}})wall(m,x1,6.2f,x2,6.5f,f+lo,roof,15);
 wall(m,19.6f,6.2f,20.05f,6.5f,f+2.2f,f+2.55f,15);wall(m,20.7f,6.2f,21.1f,6.5f,f+2.35f,f+2.55f,15);wall(m,20.05f,6.22f,20.35f,6.48f,f+2.42f,f+2.55f,15);   // its torn lip
 for(float x:{4.f,8.f,16.f})wall(m,x-.25f,5.95f,x+.25f,6.2f,f,roof,15);                                  // the wall's piers
 for(float x:{10.5f,13.f})wall(m,x,6.f,x+.5f,6.7f,f,f+3.4f,7);wall(m,10.5f,6.f,13.5f,6.7f,f+2.9f,f+3.4f,7);
 for(float x:{10.5f,13.f})wall(m,x,5.98f,x+.5f,6.f,f+.2f,f+2.6f,13);
 for(float z=f+.03f;z<f+2.86f;z+=.17f)wall(m,11.f,6.3f,13.f,6.4f,z,z+.13f,2);                            // the shutter, sealed
 wall(m,11.f,6.29f,13.f,6.41f,f,f+2.9f,CollisionProxyMaterial);
 wall(m,11.f,5.96f,13.f,6.f,f+.02f,f+.12f,13);                                                            // its striped foot
 for(auto [x1,x2,z1,z2]:{std::tuple{11.4f,11.9f,1.1f,1.7f},{12.3f,12.65f,.4f,.9f},{11.9f,12.2f,1.9f,2.3f}})wall(m,x1,6.26f,x2,6.3f,f+z1,f+z2,16);   // hands on it, from this side
 // its console, on the north side where the Force stood to work it
 wall(m,13.85f,5.25f,15.35f,5.9f,f,f+.92f,2);wall(m,13.8f,5.2f,15.4f,5.95f,f+.92f,f+.96f,8);
 cbFix(m,model::lab_control_board,14.6f,5.88f,f+.96f,cbN,false);
 cbFix(m,model::lab_status_display,13.25f,5.97f,f+2.25f,cbN,false);
 terminal(m,{14.6f,5.5f},"CHECKPOINT A / LOCKDOWN","03:18 SECTOR DOOR LOWERED ON LOSS. 03:19 DOOR SEALED. OPEN REQUESTED. REFUSED. REFUSED. REFUSED.",
  "03:22 SHUTTER BREACH EAST, PANEL 6. 03:24 POST ABANDONED. LOCKDOWN STILL IN FORCE.",.96f);
 m.terminals.back().yaw=kPi;
 m.lights.push_back(cbGlow({14.6f,4.85f},f+1.4f,.5f,3.5f,9000,{.5f,.9f,1.f},.12f));
 m.fixtures.push_back({model::mine_cage_lamp,{12.f,5.9f},f+3.f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 {auto l=cbAlarm({12.f,5.97f},f+3.1f,blN,0);l.period=1.f;l.onTime=.45f;l.intensity=1.5f;l.range=6.f;m.lights.push_back(l);}
 {auto l=cbAlarm({12.f,6.73f},f+3.15f,blS,.55f);l.period=1.f;l.onTime=.45f;l.intensity=.3f;l.range=4.f;m.lights.push_back(l);}
 // the tear through panel 6: slats bent up out of the way, scorched, the drag of something through
 for(float z:{f+2.1f,f+2.36f})wall(m,19.6f,6.25f,21.1f,6.45f,z,z+.1f,2);
 for(float x:{19.6f,21.f})wall(m,x,6.18f,x+.1f,6.52f,f,f+2.2f,8);
 for(float x:{19.5f,21.05f})wall(m,x,6.15f,x+.15f,6.55f,f+.6f,f+2.1f,16);                               // scorched lips
 m.lights.push_back(cbGlow({20.35f,5.4f},f+.6f,.6f,3.5f,2400,{1.f,.5f,.2f},.4f));                         // something still smouldering on the floor
 for(auto [x1,y1,x2,y2]:{std::tuple{19.9f,5.6f,20.4f,5.75f},{20.6f,6.9f,21.2f,7.05f},{19.2f,7.3f,19.5f,7.6f}})wall(m,x1,y1,x2,y2,f+.002f,f+.04f,2);
 for(auto [x1,y1,x2,y2]:{std::tuple{19.7f,4.6f,20.5f,5.4f},{20.1f,6.6f,20.9f,7.6f},{19.6f,8.f,20.4f,9.f}})wall(m,x1,y1,x2,y2,f+.004f,f+.008f,16);
 m.lights.push_back(cbGlow({20.3f,7.4f},f+1.7f,1.3f,7.f,4600));                                           // the work light left by the tear
 cbFix(m,model::mine_board_barricade,5.f,6.6f,f,0,true);
 m.lights.push_back(lamp({2.4f,6.5f},f+2.35f,.45f,5.f,3400,0,WorldLightMount::Wall,blS));m.lights.back().flicker=.25f;
 // their board barricade, knocked flat off its feet by what came through, the barriers thrown
 for(float y:{6.85f,7.95f,9.05f})wall(m,13.55f,y,16.65f,y+.85f,f,f+.07f,10);
 for(float y:{7.75f,8.85f})wall(m,13.55f,y,16.65f,y+.1f,f+.07f,f+.1f,13);
 for(float x:{13.7f,16.4f})wall(m,x,9.95f,x+.12f,10.05f,f,f+.75f,2);
 cbFix(m,model::barrier,9.f,7.8f,f,.5f,true);
 wall(m,6.2f,8.5f,8.2f,9.05f,f,f+.5f,15);floorLine(m,6.2f,8.5f,8.2f,9.05f,f+.5f,13);                       // a jersey barrier, on its side
 for(auto [x,y,v,yaw]:{std::tuple{8.f,7.6f,ActorVisual::Cleanup,.2f},{11.6f,8.4f,ActorVisual::CleanupRusher,2.7f},{15.6f,7.6f,ActorVisual::Cleanup,1.4f}})cbBody(m,clock,v,{x,y},f,yaw);
 m.fixtures.push_back({model::med_corpse_supine,{20.f,9.6f},0,.744f,1.753f,.411f,cbE,false});
 // --- The welded room (west): it knocks -----------------------------------------------------------
 wall(m,1.f,7.8f,6.2f,8.f,f,roof,38);wall(m,6.f,8.f,6.2f,13.f,f,roof,38);
 // its outside: the store it was, the main along it, the hose point, a lamp over the gap behind line A
 pipeRoute(m,{{1.f,7.62f,f+2.75f},{5.6f,7.62f,f+2.75f},{5.6f,7.97f,f+2.75f}},.07f,4,roof);pipeRoute(m,{{1.f,7.4f,f+3.05f},{4.4f,7.4f,f+3.05f},{4.4f,7.97f,f+3.05f}},.045f,1,roof);
 extinguisher(m,2.6f,7.8f,f+.3f,cbN);cbFix(m,model::pwr_fire_cabinet,5.3f,7.78f,f+1.1f,cbN,false);
 cbFix(m,model::lab_notice_board,3.9f,7.77f,f+1.3f,cbN,false);
 m.lights.push_back(lamp({1.8f,7.8f},f+2.35f,.55f,5.5f,4600,.04f,WorldLightMount::Wall,blN));
 m.fixtures.push_back({model::mine_cage_lamp,{1.8f,7.7f},f+2.15f-m.layers.front().elevation,.21f,.2f,.36f,kPi,false});
 cbFix(m,model::lab_specimen_crate,4.7f,7.18f,f,.1f,true,.55f);kit::prop(m,model::prop_cardboard_box,4.7f,7.18f,f+.62f,.3f,.7f);
 {const int d=wallDoor(m,1.f,6.2f,13.f,13.2f,3.f,4.2f,f,roof,1);m.doors[size_t(d)].requireState=stateId("conb_ec_welded");}   // never: welded
 for(float x:{3.f,4.15f})wall(m,x,13.2f,x+.05f,13.26f,f,f+2.4f,13);
 taped(m,{2.f,13.2f},f+1.5f,cbS,Readable::Note,"WELDED 03:30","DO NOT OPEN. WHATEVER IT SAYS. IT KNOWS OUR NAMES NOW.","");
 // It knocks: on the door, now and then, never the same gap twice; and something taps along the wall.
 m.ambients.push_back({Sound::OneMetalBang,{3.6f,12.7f},f+1.3f,.55f,13,AmbientEmitter::Random,16,12});
 m.ambients.push_back({Sound::AmbMetalTapping,{2.f,10.4f},f+1.f,.3f,7,AmbientEmitter::Loop});
 cbBody(m,clock,ActorVisual::CleanupMedic,{3.6f,10.4f},f,.8f);
 // --- Line B: crates and barriers across the hall, open at the west wall --------------------------
 for(float x:{7.f,8.7f,15.5f,17.2f})cbFix(m,model::barrier,x,15.5f,f,0,true);
 for(float x:{10.2f,11.4f,12.6f,13.8f,18.6f,19.8f,21.f,22.2f})cbFix(m,model::lab_specimen_crate,x,15.5f,f,(x-16.f)*.006f,true);
 // --- The control room (east): the release -------------------------------------------------------
 wall(m,17.f,7.8f,23.f,8.f,f,roof,36);
 wall(m,16.8f,8.f,17.f,8.6f,f,roof,36);cbGlaze(m,16.8f,8.6f,17.f,12.4f,f,roof,.95f,2.4f,true,36);wall(m,16.8f,12.4f,17.f,13.2f,f,roof,36);
 // its back, on the gap behind line A: the room's feeds coming in, the breakers for the hall, a lamp
 cableTray(m,{17.f,7.52f},{23.f,7.52f},f+2.6f);
 for(float x:{18.2f,18.6f,19.f})cbFix(m,model::pwr_breaker_panel,x,7.75f,f+1.4f,cbN,false);
 cbFix(m,model::pwr_disconnect_switch,21.4f,7.62f,f+1.f,cbN,false);cbFix(m,model::pwr_ammeter_box,22.1f,7.74f,f+1.5f,cbN,false);
 m.lights.push_back(lamp({20.f,7.8f},f+2.35f,.55f,5.5f,4600,.04f,WorldLightMount::Wall,blN));
 m.fixtures.push_back({model::mine_cage_lamp,{20.f,7.7f},f+2.15f-m.layers.front().elevation,.21f,.2f,.36f,kPi,false});
 wallDoor(m,17.f,23.f,13.f,13.2f,19.6f,20.8f,f,roof,-1);
 cbFix(m,model::lab_control_board,22.9f,10.6f,f+.95f,cbW,false);
 terminal(m,{22.2f,10.6f},"EMERGENCY BARRICADE / SOUTH","LOWERED 03:20 ON SECTOR LOSS. ACCUMULATOR BLED DOWN 03:41 BY ORDER. RAISE NEEDS 180 BAR.",
  "PUMP SET: SOUTH WALL, WEST OF LINE B. RAISE ON ORDER OF THE INVESTIGATOR ONLY. HE HAS NOT ORDERED IT.",0,stateId("cont_barricade"),stateId("conb_ec_pressure"));
 m.terminals.back().yaw=cbW;
 cbFix(m,model::lab_status_display,22.95f,12.f,f+1.8f,cbW,false);
 for(float x:{18.2f,19.2f})cbFix(m,model::med_records_cabinet,x,8.27f,f,cbS,true);
 m.lights.push_back(cbTube(20.f,10.6f,roof,.8f,6.5f,4200,.03f));
 m.lights.push_back(cbGlow({22.4f,10.6f},f+1.4f,.25f,2.4f,9000,{.6f,.8f,1.f},.08f));
 // raising it brings the sector's things to the noise
 {ScriptAction a;a.type=ScriptAction::Type::SpawnEnemy;a.enemyKind=CreatureKind::Transformed;a.position={11.f,3.f};a.z=f;a.amount=8;
  ScriptAction b=a;b.position={14.f,4.2f};ScriptAction s;s.type=ScriptAction::Type::Shake;s.amount=.5f;
  cbEvent(m,"conb_ec_raised",0,0,24,24,"cont_barricade",{a,b,s});}
 // --- The south door and the Emergency detail -----------------------------------------------------
 wall(m,8.6f,22.4f,10.f,23.f,f,roof,15);wall(m,14.f,22.4f,15.4f,23.f,f,roof,15);wall(m,10.f,22.4f,14.f,23.f,f+3.2f,roof,15);
 floorLine(m,9.f,21.5f,15.f,21.7f,f,13);
 wallSign(m,12.f,22.38f,f+3.35f,"BARRICADE DOWN","RAISED FROM THE CONTROL ROOM / EAST",3.f,cbN);
 for(auto [x,ph]:{std::pair{9.3f,0.f},{14.7f,.7f}})m.lights.push_back(cbBeacon({x,22.4f},f+2.9f,blN,ph,"cont_barricade"));
 {auto l=cbGlow({12.f,21.5f},f+3.5f,.6f,5.f,5000,{.2f,1.f,.3f});l.enabledState=stateId("cont_barricade");m.lights.push_back(l);}
 cbFix(m,model::lab_specimen_crate,20.6f,20.4f,f,.3f,true);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,20.6f,20.4f,f+1.12f,.18f,.4f,.52f,-2.f);
  auto l=cbGlow({20.6f,20.2f},f+1.5f,.85f,7.f,3200);l.prop=lampProp;m.lights.push_back(l);}
 cbGuard(m,clock,"conb_ec_heavy",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"conb_ec",{{{12.f,19.6f},f,60000,cbN}},13,1,.9f,130);
 m.actorTracks.back().ai.stance=stance;
 cbGuard(m,clock,"conb_ec_lead",ActorVisual::Cleanup,GuardSquadRole::Assault,"conb_ec",{{{16.4f,19.2f},f,60000,cbN}},12,1,1.f);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("conb_ec_hello");}
 m.dialogue.push_back({stateId("conb_ec_hello"),"EMERGENCY DETAIL","We've been on this door since three. Raise it and we go down with you, or ahead of you. Your choice.","conb_ec_hello",{
  cbReply("Cover the north. I'll raise it.","","conb_ec_allied",int(SquadStance::Ally)),cbReply("What's down there?","conb_ec_down"),cbReply("Stay here.")}});
 m.dialogue.push_back({stateId("conb_ec_down"),"EMERGENCY DETAIL","Deep research. Bore samples. Whatever they were digging toward. He says nothing goes down. He didn't say why.","conb_ec_down",{cbReply("Back.","conb_ec_hello")}});
 cbEvent(m,"conb_ec_truce",8.f,0,16.f,5.f,"",{cbSet("conb_ec_stance",int(SquadStance::Truce))});
 cbSequence(m,"conb_ec_radio","conb_ec_radio_done",22000,20000,8.f,16.f,17.f,23.f);
 const Vec2 radio{12.f,19.6f};
 cue(m,"conb_ec_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Emergency detail. Hold the barricade. Nothing goes down to Deep Research.",.9f,f+1.5f,"conb_ec_radio_1");
 cue(m,"conb_ec_radio",7000,Sound::Empty,radio,"RADIO: Sir, we've held it four hours. There's nobody left out there to keep out.",.9f,f+1.5f,"conb_ec_radio_2");
 cue(m,"conb_ec_radio",13000,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you are keeping something in.",.9f,f+1.5f,"conb_ec_radio_3");
 cue(m,"conb_ec_radio",18000,Sound::Empty,radio,"EMERGENCY DETAIL: Oh. He means us.",.9f,f+1.5f,"conb_ec_radio_4");
 // Written off ("He means us"), they are hers, if she has kept her side of the truce.
 cbEvent(m,"conb_ec_written_off",0,0,24,24,"conb_ec_radio_done",{cbSet("conb_ec_stance",int(SquadStance::Ally))});
 m.scriptEvents.back().conditions.push_back({ScriptCondition::Source::State,stance,ScriptCondition::Op::Equal,int(SquadStance::Truce)});
 // The pump set that charges the barricade's accumulator: bled down by order at 03:41 so nobody
 // could raise it from the console. Its line runs high along the south wall to the rams at the door.
 cbFix(m,model::mine_dewatering_pump,1.52f,20.9f,f,cbE,true);
 vessel(m,{1.62f,17.2f},.42f,f,f+2.3f,4);
 // the accumulator's line: up off the vessel's head, along the wall, into the high run to the rams
 m.pipes.push_back({{1.62f,17.2f},{1.62f,17.2f},f+2.3f,.05f,f+3.05f,4});m.pipes.push_back({{1.62f,17.2f},{1.16f,17.2f},f+3.05f,.05f,f+3.05f,4});
 m.pipes.push_back({{1.16f,17.2f},{1.16f,22.6f},f+3.05f,.05f,f+3.05f,4});m.pipes.push_back({{1.16f,22.6f},{1.2f,22.6f},f+3.05f,.05f,f+3.05f,4});
 terminal(m,{1.75f,18.55f},"BARRICADE HYDRAULICS / PUMP SET","ACCUMULATOR 0 BAR. BLED 03:41, ORDER OF THE INVESTIGATOR. PUMP SET ON LOCAL. CHARGE TO 180 BAR?",
  "03:41 MANUAL ANNOTATION: IF YOU ARE READING THIS AND YOU ARE NOT HIM, CHARGE IT.",0,stateId("conb_ec_pressure"));
 m.terminals.back().yaw=cbE;
 m.lights.push_back(lamp({1.f,19.15f},f+2.45f,.6f,6.f,3400,0,WorldLightMount::Wall,blE));
 m.fixtures.push_back({model::mine_cage_lamp,{1.1f,19.15f},f+2.25f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 {auto l=cbGlow({1.9f,18.55f},f+1.3f,.3f,2.6f,9000,{.5f,1.f,.6f});l.enabledState=stateId("conb_ec_pressure");m.lights.push_back(l);}   // CHARGED
 {ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=Sound::OneMetalCreak;a.amount=.6f;cbEvent(m,"conb_ec_pumped",0,0,24,24,"conb_ec_pressure",{a});}
 servicePipe(m,{1.2f,22.6f},{9.6f,22.6f},f+3.05f,.06f,4);
 // the rams either side of the door, the barricade's crash beam riding on them
 for(float x:{9.72f,14.28f}){m.pipes.push_back({{x,22.22f},{x,22.22f},f+.15f,.11f,f+2.9f,4});
  flange(m,{x,22.22f,f+.2f},{},.11f);flange(m,{x,22.22f,f+2.85f},{},.11f);
  for(float z:{.1f,2.86f})wall(m,x-.16f,22.06f,x+.16f,22.4f,f+z,f+z+.12f,8);                          // its strapped feet and head
  wall(m,x-.14f,22.08f,x+.14f,22.4f,f,f+3.f,CollisionProxyMaterial);}
 // The two crash beams the rams carry: down across the opening at knee and chest while the
 // accumulator is empty; when the barricade is raised the rams take them up under the head one after
 // the other (conb_ec_ram, started by the raise), the pump set's line banging as each one seats.
 // Two clocks, so nothing stands in the doorway until somebody is here to see it: conb_ec_ram_held
 // starts as the player walks in and sets the lowered beams across the door (it is pushed past its
 // last key, where they are gone, the moment the barricade is raised); conb_ec_ram carries the
 // raised pair up the rams from where the lowered pair was.
 {TimedSequence s;s.timerState=stateId("conb_ec_ram");s.finishState=stateId("conb_ec_ram_up");s.x1=-9.f;s.y1=-9.f;s.x2=-8.f;s.y2=-8.f;
  s.durationMs=9000;s.finishAtMs=8000;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
  s.timerState=stateId("conb_ec_ram_held");s.finishState=stateId("conb_ec_ram_held_end");s.x1=0;s.y1=0;s.x2=24.f;s.y2=24.f;s.durationMs=3600000;s.finishAtMs=3600000;m.timedSequences.push_back(s);}
 const float gone=f-25.f;
 for(auto [y,lo,hi,thick,t0,t1]:{std::tuple{22.12f,f+.98f,f+2.98f,.34f,1400,6200},{22.16f,f+1.82f,f+3.3f,.3f,500,4600}}){
  ActorTrack b;b.timerState=stateId("conb_ec_ram_held");b.visual=ActorVisual::Cargo;b.scale=1.f;b.platform=true;b.footprint={4.9f,.3f};b.thickness=thick;
  b.keys={{0,{12.f,y},gone,0},{1,{12.f,y},lo,0},{3000000,{12.f,y},lo,0},{3000001,{12.f,y},gone,0}};m.actorTracks.push_back(b);
  b.timerState=stateId("conb_ec_ram");b.keys={{0,{12.f,y},gone,0},{1,{12.f,y},lo,0},{t0,{12.f,y},lo,0},{t1,{12.f,y},hi,0},{9000,{12.f,y},hi,0}};m.actorTracks.push_back(std::move(b));}
 for(auto [t,snd,gain]:{std::tuple{200,Sound::LiftMotor,.8f},{700,Sound::OneMetalCreak,.7f},{4600,Sound::OneMetalBang,.9f},{6200,Sound::OneMetalBang,1.f}}){
  SequenceCue c;c.timerState=stateId("conb_ec_ram");c.timeMs=t;c.sound=snd;c.position={12.f,21.6f};c.gain=gain;c.z=f+2.f;m.sequenceCues.push_back(c);}
 {ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=Sound::PaDoorsUnlocked;a.amount=.7f;
  cbEvent(m,"conb_ec_ram_go",0,0,24,24,"cont_barricade",{cbSet("conb_ec_ram",1),cbSet("conb_ec_ram_held",3100000),a});}
 m.pickupSpawns.push_back({{21.6f,22.2f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{3.4f,21.8f},PickupKind::Health,f});
 // --- Light: tubes mostly out, the flood by the detail, red on the lines and the door -------------
 m.lights.push_back(cbTube(6.f,3.f,roof,.6f,6.5f,5200,.03f,.4f));m.lights.push_back(cbTube(12.f,11.f,roof,.65f,7.f,5200,.03f));m.lights.push_back(cbTube(12.f,3.f,roof,.6f,6.5f,5200,.03f));m.lights.push_back(cbTube(4.f,11.f,roof,.4f,5.f,5200,.03f,.3f));
 // the detail's half kept its own tubes on, so the second line and the way to the door read
 for(auto [x,y,i]:{std::tuple{8.6f,17.4f,.55f},{16.f,17.4f,.55f},{8.5f,11.6f,.4f},{19.f,22.f,.4f}})m.lights.push_back(cbTube(x,y,roof,i,6.5f,5200,.03f,0));
 m.lights.push_back(cbTube(18.f,3.f,roof,.45f,6.f,5200,.03f,.35f));cbDeadTube(m,12.f,18.f,roof);cbDeadTube(m,7.f,11.f,roof);
 // The detail's floodlight: on a mast behind the second line, thrown hard north through the gap in
 // it at the tear, so whatever comes through panel 6 comes into the light. From the north the line
 // reads as a lit firing position; behind it the detail's own lamps, warm and low.
 {const float mx=12.f,my=16.6f;
  wall(m,mx-.05f,my-.05f,mx+.05f,my+.05f,f,f+3.1f,12);wall(m,mx-.35f,my-.35f,mx+.35f,my+.35f,f,f+.05f,8);
  for(auto [dx,dy]:{std::pair{-.33f,0.f},{.33f,0.f}})m.pipes.push_back({{mx+dx,my+dy},{mx,my},f+.05f,.018f,f+1.4f,6});
  wall(m,mx-.34f,my-.36f,mx+.34f,my-.02f,f+3.1f,f+3.5f,8);wall(m,mx-.3f,my-.37f,mx+.3f,my-.36f,f+3.14f,f+3.46f,20);
  m.lights.push_back(lamp({mx,my-.38f},f+3.3f,2.2f,14.f,5600,0,WorldLightMount::Wall,blN,true));
  wall(m,mx-.02f,my+.05f,mx+.02f,my+2.2f,f,f+.03f,8);m.structures.back().solid=false;}
 cbPendant(m,{12.f,20.4f},roof,f+3.4f,1.1f,7.f,2600);                                                 // over the door they held
 cbPendant(m,{20.f,10.6f},roof,f+2.9f,.9f,6.f,3800);                                                  // the control room's desk light
 m.lights.push_back(cbGlow({3.6f,13.4f},f+.08f,.35f,3.f,2000,{1.f,.3f,.15f},.5f));                     // under the welded door: something lit in there
 m.lights.push_back(lamp({22.98f,2.4f},f+2.6f,.5f,6.f,6500,.02f,WorldLightMount::Wall,blW));m.lights.back().flicker=.3f;   // the north bay's emergency fitting
 recessedFittings(m);
 zone(m,"EMERGENCY CONTAINMENT / CONTROL ROOM",17.f,8.f,23.f,13.f);
 // The hall's frame: downstand beams across the roof, two columns in the detail's half, pilasters.
 for(float y:{4.3f,9.3f,14.3f,20.6f})wall(m,1.f,y,23.f,y+.3f,roof-.45f,roof,15);
 for(float x:{6.5f,17.8f}){wall(m,x-.3f,18.3f,x+.3f,18.9f,f,roof,15);wall(m,x-.38f,18.22f,x+.38f,18.98f,f,f+.3f,15);floorLine(m,x-.38f,18.22f,x+.38f,18.34f,f+.3f,13);}
 wallBays(m,f,roof,4,false,true,true,true,12);
 zone(m,"",8.f,20.f,16.f,24.f,-1000,1000,{.13f,.12f,.12f,.006f});
 zone(m,"EMERGENCY CONTAINMENT",0,0,24,24,-1000,1000,{.12f,.12f,.12f,.008f});
 freightFinish(m,-1);
 return p;
}
}
void buildChapterContainmentB(CampaignMapTable& maps){
 maps[99]=makeSpecimenHandling();
 maps[100]=makeIsolationWing();
 maps[101]=makeObservationTheater();
 maps[102]=makeBreachSector();
 maps[103]=makeEmergencyContainment();
}
}
