// ENERGY TEST COMPLEX (chapter 2, manifest 55-60): chunks 85, 86, 91, 92, 93.
// The Resonance Laboratory and the High-Energy Test Bay north of the Observation Ring
// (CampaignChapterObservationRing.cpp, 87-90); the Shielding Corridor, Experimental Power and
// the Conversion Lab down its west side. See docs/Design/descent-campaign-20261025.md.
// Where people first bottled resonance: older, heavier and darker than the Analysis labs,
// concrete and lead and copper, warm control rooms over cold halls, and in every room something
// that kept running after the feeds were pulled at 01:58.
#include "CampaignChapterAnalysisKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
using namespace an;
// --- The complex's own fittings (et) ------------------------------------------------------------
// A ceiling tube that works: the emitter clear below its recessed fitting.
WorldLight etTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint=0,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
// A fitting with its tubes dead: the housing alone, flush to the ceiling.
void etDeadTube(Map& m,float x,float y,float roof){anFix(m,model::clinic_ceiling_lamp_2,x,y,roof-.17f,.44f,1.04f,.17f,0,false);}
// A high-bay on its drop rod from the roof to the reflector at z; dark if intensity is 0.
void etHighBay(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});anFix(m,model::pendant_lamp,p.x,p.y,z,.5f,.5f,.34f,0,false);
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,0,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}
// A wall lamp on its face (heading: east 0, south pi/2, west pi, north -pi/2).
WorldLight etWall(Vec2 p,float z,float heading,float intensity,float range,float kelvin,float flicker=0,Point3 gel={1,1,1}){
 auto l=lamp(p,z,intensity,range,kelvin,0,WorldLightMount::Wall,heading,true,gel);l.flicker=flicker;return l;}
// A red beacon that blinks while `state` is unset and dies when it is set (amber: a cycle never finished).
WorldLight etBeacon(Vec2 p,float z,float heading,float phase,StateId state,bool amber=false){
 auto l=lamp(p,z,amber?.36f:.32f,amber?3.6f:4.f,3000,0,WorldLightMount::Wall,heading,true,amber?Point3{1.f,.5f,.06f}:Point3{1.f,.12f,.06f});
 l.period=amber?1.f:1.4f;l.onTime=.5f*l.period;l.phase=phase;l.enabledState=state;l.invert=true;return l;}
// The ore's own light: a bare, unshadowed glow (violet, or green in the cell stores).
WorldLight etGlow(Vec2 p,float z,float intensity,float range,Point3 gel,float flicker=0){
 auto l=lamp(p,z,intensity,range,6500,0,WorldLightMount::Bare,0,false,gel);l.flicker=flicker;return l;}
const Point3 etViolet{.72f,.48f,1.f},etGreen{.15f,1.f,.25f},etArcBlue{.55f,.7f,1.f};
// Live conductors: an Electricity box that arcs `on` seconds of every `period`.
void etArc(Map& m,float x1,float y1,float x2,float y2,float lo,float hi,float dps,float period,float on,float phase=0){
 Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=x1;h.y1=y1;h.x2=x2;h.y2=y2;h.bottom=lo;h.top=hi;h.damagePerSecond=dps;
 h.period=period;h.onTime=on;h.phase=phase;m.hazards.push_back(h);}
// The blue of an arc on the same clock as etArc, over the conductors.
WorldLight etArcLight(Vec2 p,float z,float intensity,float range,float period,float on,float phase=0){
 auto l=lamp(p,z,intensity,range,9000,0,WorldLightMount::Bare,0,false,etArcBlue);l.period=period;l.onTime=on;l.phase=phase;l.flicker=.6f;return l;}
// Somebody who did not get out: a posed body on the map's clock.
void etBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
// A fitted bench run along its longer side: kick plinth, steel carcass, drawer lines, worktop at f+h.
void etCounter(Map& m,float x1,float y1,float x2,float y2,float f,float h=.9f,int worktop=28){
 const float top=f+h;
 wall(m,x1+.05f,y1+.05f,x2-.05f,y2-.05f,f,f+.08f,8);wall(m,x1,y1,x2,y2,f+.08f,top-.04f,20);wall(m,x1,y1,x2,y2,top-.04f,top,worktop);
 if(x2-x1>=y2-y1)for(float y:{y1-.006f,y2-.004f})wall(m,x1+.02f,y,x2-.02f,y+.01f,top-.3f,top-.29f,8);
 else for(float x:{x1-.006f,x2-.004f})wall(m,x,y1+.02f,x+.01f,y2-.02f,top-.3f,top-.29f,8);
}
// Paper lying on furniture whose top is `surface` above the floor.
void etPaper(Map& m,Vec2 p,float surface,float yaw,Readable kind,const char* title,const char* a,const char* b){
 AuthoredTerminalData t;t.position=p;t.z=0;t.yaw=yaw;t.title=title;t.line1=a;t.line2=b;t.readable=kind;t.surface=surface;m.terminals.push_back(std::move(t));}
// A Cleanup man on a beat: the squad's guard record, as the Medical chapter writes it.
void etGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
void etSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1,float y1,float x2,float y2){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
ScriptAction etSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
void etEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions,bool once=true){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=once;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
ScriptAction etSound(Sound sound,float gain){ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=sound;a.amount=gain;return a;}
ScriptAction etShake(float amount){ScriptAction a;a.type=ScriptAction::Type::Shake;a.amount=amount;return a;}
ScriptAction etSave(){ScriptAction a;a.type=ScriptAction::Type::Checkpoint;return a;}
ScriptAction etSpawn(CreatureKind kind,Vec2 p,float z,float awareness){ScriptAction a;a.type=ScriptAction::Type::SpawnEnemy;a.enemyKind=kind;a.position=p;a.z=z;a.amount=awareness;return a;}
ScriptAction etObjective(const char* id,ObjectiveStatus status){ScriptAction a;a.type=ScriptAction::Type::SetObjective;a.id=stateId(id);a.value=int(status);return a;}
// A script event in its box, once, when every state test holds (a value of 0 asks for a state still unset).
void etWhen(Map& m,const char* id,float x1,float y1,float x2,float y2,std::initializer_list<std::pair<const char*,int>> when,std::vector<ScriptAction> actions){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;
 for(auto [s,v]:when){ScriptCondition c;c.id=stateId(s);c.value=v;e.conditions.push_back(c);}
 e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
// A reply: where it goes, the state it sets (to 1), and the state that hides it once set.
DialogueReply etReply(const char* text,const char* next="",const char* sets="",const char* hide=""){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;if(*sets)r.sets.push_back({stateId(sets),1});r.hide=*hide?stateId(hide):0;return r;}
// A walk between two points on the actor's clock: yaw toward where it goes, the walk cycle's phase by distance.
float etHeading(Vec2 from,Vec2 to){return std::atan2(to.x-from.x,to.y-from.y);}
// A suit cabinet on a wall: an open steel bay with a canopy and a valance, the suits on their
// hangers from a rail under it with their feet clear of the boot tray, a lamp under the canopy.
// a..b is the bay's span along the wall, `face` the wall face (y for a wall along x, x for one
// along y), `facing` the heading out of the wall (0 east, pi/2 south). Every hook but `bare` is hung.
void etSuitRail(Map& m,float a,float b,float face,float f,float facing,int bare=-1){
 const Vec2 o{std::round(std::cos(facing)),std::round(std::sin(facing))};const bool alongX=std::fabs(o.y)>.5f;const float sg=alongX?o.y:o.x;
 auto box=[&](float s1,float s2,float d1,float d2,float lo,float hi,int mat){const float p1=face+sg*d1,p2=face+sg*d2;
  if(alongX)wall(m,s1,std::min(p1,p2),s2,std::max(p1,p2),lo,hi,mat);else wall(m,std::min(p1,p2),s1,std::max(p1,p2),s2,lo,hi,mat);};
 auto at=[&](float s,float d){const float p=face+sg*d;return alongX?Vec2{s,p}:Vec2{p,s};};
 box(a,b,.004f,.02f,f,f+2.3f,2);box(a,b,.02f,.62f,f+2.24f,f+2.3f,2);                                // back skin, canopy
 box(a,b,.58f,.62f,f+1.62f,f+2.24f,2);box(a,b,.62f,.626f,f+1.62f,f+1.7f,13);                          // valance, its striped lip
 box(a-.04f,a,.004f,.62f,f,f+2.3f,2);box(b,b+.04f,.004f,.62f,f,f+2.3f,2);                              // the bay's sides
 box(a,b,.02f,.56f,f,f+.05f,8);box(a,b,.54f,.56f,f+.05f,f+.12f,8);                                     // the boot tray and its lip
 m.pipes.push_back({at(a-.03f,.3f),at(b+.03f,.3f),f+2.f,.016f,f+2.f,5});                               // the rail, into both sides
 const float len=b-a;const int hooks=std::max(2,int(len/.72f));
 for(int i=0;i<hooks;++i){const Vec2 h=at(a+len*(i+.5f)/hooks,.3f);
  m.pipes.push_back({h,h,f+1.86f,.008f,f+2.f,5});
  if(i!=bare)hungSuit(m,{h.x,h.y,f+1.8f},facing+(i%3-1)*.1f);}
 m.lights.push_back(lamp(at((a+b)*.5f,.32f),f+2.16f,.4f,3.2f,4300,0,WorldLightMount::Bare,0,false));   // the strip under the canopy
}
// An open steel flight guarded the way stairs are built, not as stepped blocks: a sloped handrail
// and knee rail on posts on the open sides (bit 1 the low-x/low-y side, bit 2 the other), the
// guard's collision kept as invisible per-step proxies. Call after freightFinish (it sets sideRails).
void etStairRails(Map& m,size_t index,int sides,int paint=3){
 auto& s=m.stairs[index];s.sideRails=false;
 const float run=s.alongY?s.y2-s.y1:s.x2-s.x1,rise=s.top-s.bottom;
 auto line=[&](float t){return s.ascending?s.bottom+rise*t:s.top-rise*t;};
 for(int side=0;side<2;++side){if(!(sides&(1<<side)))continue;
  const float c=side?(s.alongY?s.x2-.05f:s.y2-.05f):(s.alongY?s.x1+.05f:s.y1+.05f);
  auto P=[&](float t){return s.alongY?Vec2{c,s.y1+run*t}:Vec2{s.x1+run*t,c};};
  for(int k=0;k<s.steps;++k){const float lo=float(k)/s.steps,hi=float(k+1)/s.steps,top=s.bottom+rise*(s.ascending?hi:1-lo);
   const Vec2 p=P(lo),q=P(hi);wall(m,std::min(p.x,q.x)-.035f,std::min(p.y,q.y)-.035f,std::max(p.x,q.x)+.035f,std::max(p.y,q.y)+.035f,top,top+1.05f,CollisionProxyMaterial);}
  m.pipes.push_back({P(0),P(1),line(0)+1.08f,.032f,line(1)+1.08f,paint});
  m.pipes.push_back({P(0),P(1),line(0)+.58f,.02f,line(1)+.58f,paint});
  const int posts=std::max(2,int(run/1.3f)+1);
  for(int i=0;i<posts;++i){const float t=std::clamp(float(i)/(posts-1),.02f,.98f);const Vec2 p=P(t);m.pipes.push_back({p,p,line(t)-.12f,.024f,line(t)+1.08f,paint});}}
}
}

// RESONANCE LABORATORY (manifest 55). The oldest room in the complex: where R-01, the first piece
// brought up from the face, was sat across from people to see what it did. It did things. The
// sittings went on for twenty years in the lead-glass booth; the volunteers came from the face
// crews, paid double. At 01:58 the booth alarm went during sitting 212 and the staff sealed the
// passage to Energy Diagnostics behind them, and then the booth, with the volunteer inside it.
// Route: in from the Observation Ring (S) -> the lab -> the passage north, where the seal is
// released from this side (res_lab_unsealed): Lab 4's back door opens and the loop to Analysis
// closes. W to the High-Energy Test Bay.
std::shared_ptr<const Map> makeResonanceLaboratory(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[85];m.definition=CampaignChunks[85];m.definition.ambient=.23f;
 m.definition.playerStart={12.f,21.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=-kPi*.5f;
 m.wallSurface=SurfacePaintedGreen;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});openEdge(m,{'W',9.f,14.f,f,false,0});openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"et_res_clock",60000);
 const StateId unsealed=stateId("res_lab_unsealed");
 auto crash=[&](float x1,float x2,float y1,float y2){wall(m,x1,y1,x2,y2,f+.86f,f+.96f,20);};
 // --- The passage north: the seal bulkhead and its release ---------------------------------------
 wall(m,9.8f,1.f,10.f,6.9f,f,roof,3);wall(m,14.f,1.f,14.2f,6.9f,f,roof,3);
 wall(m,10.f,2.7f,10.5f,3.3f,f,roof,15);wall(m,13.5f,2.7f,14.f,3.3f,f,roof,15);wall(m,10.5f,2.7f,13.5f,3.3f,f+2.6f,roof,15);
 {Door d{10.5f,13.5f,3.f,0};d.requireState=unsealed;m.doors.push_back(d);}                       // door 0: the seal, this side
 floorLine(m,10.f,3.4f,14.f,3.55f,f,13);floorLine(m,10.f,2.45f,14.f,2.6f,f,13);
 terminal(m,{13.6f,4.9f},"PASSAGE SEAL / ENERGY DIAGNOSTICS","SEALED FROM THIS SIDE 01:58:30 ON THE BOOTH ALARM. THE LAB 4 DOOR HELD SHUT TO MATCH.",
  "E / RELEASE THE SEAL. BOTH DOORS OPEN.",0,unsealed);
 m.terminals.back().yaw=anW;
 taped(m,{10.01f,4.6f},f+1.5f,anE,Readable::Note,"SEAL / 01:59","WE SEALED OURSELVES IN WITH IT. THAT IS THE PROTOCOL. THE PROTOCOL WAS WRITTEN IN 1981.",
  "IF THE BOOTH LAMP GOES RED AGAIN, DO NOT RELEASE. GO OUT THROUGH THE RING. -K.");
 wallSign(m,12.f,3.3f,f+2.68f,"PRESSURE SEAL","SEALS WITH THE BOOTH / RELEASES FROM THIS SIDE",2.2f,anS);
 m.lights.push_back(etBeacon({11.f,3.32f},f+2.95f,kPi*.5f,0,unsealed));
 {auto l=etWall({13.f,3.32f},f+2.95f,kPi*.5f,.3f,4.f,4000,0,etGreen);l.enabledState=unsealed;m.lights.push_back(l);}
 // the release lands: the seal's dogs let go with a bang through the passage, and the booth answers
 etEvent(m,"et_res_released",0,0,24,24,"res_lab_unsealed",{etSound(Sound::OneMetalBang,.8f),etShake(.2f),etSave()});
 m.ambients.push_back({Sound::OneGlassBang,{12.f,12.f},f+1.4f,.55f,12.f,AmbientEmitter::OnState,8,8,unsealed,1});   // something in the booth, at the glass
 m.lights.push_back(etTube(12.f,5.4f,roof,.45f,5.f,4000,.02f));
 m.lights.push_back(etTube(12.f,1.6f,roof,.25f,4.f,4000,.02f,.4f));
 // the passage's own lamps at the seal, either side, so its walls read past the bulkhead
 for(auto [x,h,yaw]:{std::tuple{10.02f,0.f,anE},{13.98f,kPi,anW}}){anFix(m,model::mine_cage_lamp,x+(h>1?-.1f:.1f),1.9f,f+2.2f,.21f,.2f,.36f,yaw,false);
  m.lights.push_back(etWall({x,1.9f},f+2.35f,h,.55f,5.f,4200));}
 // the passage's mouth on the lab: steel posts and a head
 for(float x:{9.92f,14.f})doorTrim(m,x,6.88f,x+.08f,7.12f,f,f+2.8f);
 wall(m,10.f,6.9f,14.f,7.1f,f+2.8f,roof,3);
 // --- The archive (northwest): twenty years of sittings on paper ---------------------------------
 wallDoor(m,1.f,9.8f,6.9f,7.1f,4.f,5.2f,f,roof,-1);
 for(float x:{2.2f,4.5f}){anFix(m,model::office_bookcase,x,1.27f,f,2.02f,.5f,2.5f,anS,true);wall(m,x-1.01f,1.02f,x+1.01f,1.52f,f+2.5f,roof,30);}
 anModel(m,model::lab_mainframe,8.6f,2.3f,f,anE,true);wall(m,7.46f,1.2f,9.74f,3.4f,f+3.18f,roof,30);   // the sittings' own computer, reels and all
 for(float y:{4.2f,5.4f})anModel(m,model::med_records_cabinet,1.27f,y,f,anE,true);
 recordsDesk(m,5.2f,4.7f,f);
 {const int dl=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,5.56f,4.7f,f+.825f,.18f,.4f,.52f,.1f);
  m.lights.push_back(lamp({5.65f,4.95f},f+1.2f,.3f,2.6f,2700,0,WorldLightMount::Bare));m.lights.back().prop=dl;}
 etPaper(m,{4.9f,4.75f},.78f,0,Readable::Letter,"SITTINGS PROGRAMME / 1981","VOLUNTEERS FROM THE FACE CREWS, PAID DOUBLE. ONE HOUR IN THE CHAIR AT ARM'S LENGTH FROM R-01. LAMPS STEADY, NO TORCHES.",
  "THE THREE WHO SAT LONGEST ASKED TO COME BACK. ALL THREE CAME BACK WITHOUT BEING ASKED. ONE CAME BACK AFTER HIS FUNERAL WAS BOOKED.");
 paperDrift(m,1.6f,3.6f,9.f,6.6f,f,6,8511);
 for(int i=0;i<3;++i)kit::prop(m,model::office_file_binder,1.18f,3.85f+i*.12f,f+1.f,.093f,.301f,.424f,kPi*.5f);
 m.lights.push_back(etTube(4.5f,3.2f,roof,.35f,5.f,2900,0,.2f));
 // --- The sample vault (northeast): R-01's first cell behind the bars ------------------------------
 wallDoor(m,14.2f,23.f,6.9f,7.1f,18.8f,20.f,f,roof,-1);
 anFix(m,model::med_cell_bars,15.5f,3.6f,f,2.5f,.04f,2.4f,0,false);anFix(m,model::lab_cell_gate,18.6f,3.6f,f,3.5f,.19f,2.72f,0,false);
 anFix(m,model::med_cell_bars,21.65f,3.6f,f,2.2f,.04f,2.4f,0,false);
 wall(m,14.2f,3.5f,23.f,3.7f,f,f+2.75f,30);wall(m,14.2f,3.52f,23.f,3.68f,f+2.75f,roof,20);       // the cage: shut, its header to the slab
 for(float x:{15.8f,21.3f}){anModel(m,model::lab_specimen_cabinet,x,1.4f,f,anS,true);wall(m,x-.67f,1.03f,x+.67f,1.77f,f+2.19f,roof,30);}
 wall(m,18.3f,1.9f,18.9f,2.5f,f,f+.9f,28);                                                    // the plinth
 {const int cell=int(m.clutterSpawns.size());kit::prop(m,model::anl_cell_charged,18.6f,2.2f,f+.9f,.2f);
  m.lights.push_back(etGlow({18.6f,2.2f},f+1.45f,.22f,2.4f,etGreen));m.lights.back().prop=cell;}
 for(auto [x,y,yaw]:{std::tuple{16.9f,2.6f,.4f},{20.2f,2.9f,2.2f}})kit::prop(m,model::anl_cell_spent,x,y,f,yaw);
 propPose(m,model::anl_cell_spent,19.9f,1.9f,f,1.1f,kPi*.5f,0);
 etCounter(m,22.3f,4.1f,23.f,6.6f,f);
 kit::prop(m,model::anl_cell_cap,22.6f,4.5f,f+.9f,1.2f);kit::prop(m,model::anl_cell_core,22.62f,5.2f,f+.9f,.3f);
 kit::prop(m,model::lab_field_meter,22.6f,6.1f,f+.9f,.25f,.328f,.148f,anW);
 taped(m,{14.21f,5.4f},f+1.5f,anE,Readable::Note,"CELL 0001","THE FIRST CELL EVER FILLED. 40 G OF R-01, REFINED. SEALED 1984 AND NEVER PUT ON A LOAD.",
  "IT HAS READ FULL EVERY MORNING FOR TWENTY YEARS. AT 02:10 IT READ FULLER.");
 m.lights.push_back(etTube(18.6f,5.3f,roof,.35f,4.5f,4200,0));
 // --- The lab: the booth in the middle, the recording desk facing it ------------------------------
 ceilingGrid(m,1.f,7.1f,23.f,16.9f,roof);
 // the booth: lead glass three sides, the door south, the copper coil overhead
 const float lid=f+3.2f;
 glazing(m,9.f,9.5f,15.f,9.7f,f,lid,3,.95f,2.4f,true);
 glazing(m,9.f,9.7f,9.2f,14.3f,f,lid,3,.95f,2.4f,true);glazing(m,14.8f,9.7f,15.f,14.3f,f,lid,3,.95f,2.4f,true);
 glazing(m,9.f,14.3f,11.4f,14.5f,f,lid,3,.95f,2.4f,true);swingDoor(m,11.4f,12.6f,14.4f,f,lid,-1);glazing(m,12.6f,14.3f,15.f,14.5f,f,lid,3,.95f,2.4f,true);
 // the booth's lid is gone to an open steel frame: the machine hangs down into the glass through it
 for(auto [x1,y1,x2,y2]:{std::tuple{8.9f,9.4f,15.1f,9.6f},{8.9f,14.4f,15.1f,14.6f},{8.9f,9.4f,9.1f,14.6f},{14.9f,9.4f,15.1f,14.6f},{8.9f,10.45f,15.1f,10.75f},{12.9f,9.4f,13.1f,14.6f}})
  wall(m,x1,y1,x2,y2,lid,lid+.25f,12);
 // THE RESONATOR (1981): what the booth was built under. One pylon on the west, an arm out over the
 // lid, the primary coil wound along it, the emitter dropped through the lid at R-01; on the east the
 // reservoir tank, and the copper bus that climbs from it to the coil. Lopsided, older than the room.
 wall(m,6.5f,10.6f,8.5f,13.f,f,f+.35f,15);floorLine(m,6.5f,10.6f,8.5f,10.72f,f+.35f,13);
 wall(m,7.f,11.f,8.f,12.6f,f+.35f,roof-.05f,12);wall(m,6.86f,11.f,7.f,12.6f,f+1.f,f+2.6f,8);   // the pylon, its counterweight
 wall(m,7.f,11.5f,13.4f,12.1f,f+4.4f,f+5.f,12);wall(m,7.f,11.44f,13.4f,11.5f,f+4.55f,f+4.85f,18);   // the arm and its stripe
 m.pipes.push_back({{8.2f,11.8f},{13.2f,11.8f},f+3.95f,.48f,f+3.95f,4});                         // the primary coil
 for(float x=8.45f;x<13.f;x+=.3f)m.pipes.push_back({{x,11.8f},{x+.07f,11.8f},f+3.95f,.53f,f+3.95f,6});
 for(float x:{8.3f,13.1f})m.pipes.push_back({{x,11.8f},{x,11.8f},f+4.3f,.05f,f+4.4f,6});
 m.pipes.push_back({{12.f,10.6f},{12.f,11.8f},f+3.95f,.22f,f+3.95f,6});                         // the throat down to the emitter
 vessel(m,{12.f,10.6f},.34f,lid+.25f,f+4.15f,6);
 for(auto [z,r]:{std::pair{lid+.25f,.75f},{lid+.4f,.6f},{lid+.55f,.45f}})m.pipes.push_back({{12.f,10.6f},{12.f,10.6f},z,r,z+.08f,4});
 m.pipes.push_back({{12.f,10.6f},{12.f,10.6f},f+2.1f,.07f,lid,4});m.pipes.push_back({{12.f,10.6f},{12.f,10.6f},f+1.95f,.16f,f+2.15f,6});   // the electrode, down at R-01
 vessel(m,{17.6f,8.6f},.62f,f,f+4.6f,4);                                                        // the reservoir tank
 m.pipes.push_back({{17.6f,8.6f},{13.2f,11.8f},f+4.6f,.12f,f+3.95f,4});m.pipes.push_back({{17.6f,8.6f},{13.4f,11.9f},f+4.3f,.06f,f+4.95f,6});
 wall(m,16.8f,7.8f,18.4f,9.4f,f+4.6f,roof,30);wall(m,6.86f,10.6f,8.5f,13.f,roof-.4f,roof,30);
 m.lights.push_back(etGlow({12.f,9.7f},lid+.55f,.9f,8.f,etViolet,.25f));                        // the emitter, lit from what it holds
 m.lights.push_back(etWall({1.02f,11.8f},f+4.6f,0,.8f,12.f,4200));                                // the old floods on the pylon side
 m.lights.push_back(lamp({10.f,11.f},f+5.6f,1.f,7.5f,4400,0,WorldLightMount::Bare,0,true));      // the work lamp over the coil, still on
 m.lights.push_back(etWall({22.98f,11.8f},f+4.6f,kPi,.7f,12.f,4200,.1f));
 // What it became: the resonator was never finished being added to. Coils hung off it on chains wherever
 // a sitting asked for more, down through the open lid into the booth; the second arm, the 1990 arm,
 // sheared at the pylon on the night of sitting 212 and hangs across the booth's door to the floor,
 // trailing its feed. And under it all one point, the electrode's tip over R-01, burns white.
 for(auto [x,y,lo,hi,r]:{std::tuple{10.2f,12.9f,f+2.45f,f+3.35f,.34f},{13.7f,12.4f,f+2.7f,f+3.6f,.3f},{10.3f,10.2f,f+2.9f,f+3.7f,.26f},{14.2f,10.1f,f+3.3f,f+4.3f,.38f}}){
  m.pipes.push_back({{x,y},{x,y},lo,r,hi,4});
  for(float z=lo+.1f;z<hi-.05f;z+=.14f)m.pipes.push_back({{x,y},{x,y},z,r+.04f,z+.06f,6});        // the windings
  m.pipes.push_back({{x,y},{x,y},hi,r*.5f,hi+.12f,6});
  for(float d:{-.12f,.12f})m.pipes.push_back({{x+d,y},{x+d,y},hi+.12f,.012f,roof-.05f,6});       // its chains to the slab
  m.pipes.push_back({{x,y-r},{12.f,10.6f},lo+.2f,.03f,f+2.2f,7});}                                // each one's lead to the electrode
 m.pipes.push_back({{7.85f,12.45f},{9.9f,15.35f},f+5.15f,.15f,f+2.25f,2});                         // the sheared arm
 m.pipes.push_back({{7.85f,12.5f},{10.05f,15.5f},f+5.3f,.06f,f+2.5f,18});
 wall(m,9.75f,15.2f,10.15f,15.65f,f+2.05f,f+2.4f,12);                                               // the torn end
 anFix(m,model::pwr_cable_bundle,11.6f,16.1f,f,4.f,.14f,.22f,.15f,false);
 m.pipes.push_back({{17.3f,9.f},{15.4f,11.4f},f+4.2f,.07f,f+3.3f,7});m.pipes.push_back({{15.4f,11.4f},{14.2f,10.1f},f+3.3f,.07f,f+4.3f,7});   // the reservoir's feed, sagging to the big coil
 wall(m,11.92f,10.52f,12.08f,10.68f,f+1.82f,f+1.98f,35);                                             // the tip: white-hot from inside
 {auto l=lamp({12.f,10.6f},f+1.75f,1.7f,7.f,9500,-.2f,WorldLightMount::Bare,0,false,{.92f,.82f,1.f});l.flicker=.18f;m.lights.push_back(l);}
 m.lights.push_back(etArcLight({12.f,10.9f},f+2.4f,.9f,6.f,2.6f,.3f,.4f));
 // the lab's frame: three beams across the slab, so the old room reads in bays round the machine
 for(float x:{3.2f,15.7f,19.6f})wall(m,x-.15f,7.1f,x+.15f,16.9f,roof-.42f,roof,12);
 equipmentPad(m,11.3f,10.f,12.7f,11.2f,f);wall(m,11.75f,10.35f,12.25f,10.85f,f+.1f,f+1.f,28);
 anFix(m,model::lab_anomaly_relic,12.f,10.6f,f+1.f,.5f,.4f,.4f,.3f,false);                      // R-01
 kit::prop(m,model::wooden_chair,12.f,12.75f,f,.417f,.525f,.95f,0);                            // the chair, turned to it
 for(float y:{10.1f,12.4f})m.pipes.push_back({{9.15f,y},{14.85f,y},f+2.7f,.045f,f+2.7f,4});
 m.lights.push_back(etGlow({12.f,10.6f},f+1.45f,.7f,5.f,etViolet,.3f));
 hazard(m,Hazard::Kind::Anomaly,11.f,9.8f,13.f,11.6f,f,f+2.f,0);
 anHum(m,"et_res_booth_hum",9.2f,9.7f,14.8f,14.3f,f,f+2.5f,.5f);
 kit::prop(m,model::item_id_card,13.4f,13.6f,f,.132f,.085f,.002f,1.1f);
 taped(m,{10.3f,14.29f},f+.62f,anN,Readable::Note,"SITTING 212 / 01:40","VOLUNTEER IN THE CHAIR, R-01 ON THE PLINTH, LAMPS STEADY. 01:52 HE SAID IT WAS WARM. 01:57 HE SAID IT HAD TURNED ROUND.",
  "01:58 BOOTH ALARM. WE SEALED THE PASSAGE, THEN THE BOOTH. NOBODY ASKED WHICH SIDE OF THE GLASS HE WAS ON.");
 wallSign(m,13.8f,14.5f,f+2.6f,"NO MOVING LIGHT","LAMPS STEADY WHILE A SAMPLE IS ON THE PLINTH",1.8f,anS);
 // the recording desk east of the booth, its operators facing the glass
 etCounter(m,16.f,10.f,16.7f,14.f,f);
 kit::prop(m,model::crt_monitor,16.3f,10.9f,f+.9f,.42f,.4f,.38f,anE);kit::prop(m,model::keyboard,16.58f,10.9f,f+.9f,.45f,.16f,.03f,anE);
 anModel(m,model::lab_scope_monitor,16.3f,12.3f,f+.9f,anE,false);
 kit::prop(m,model::office_paper_stack,16.4f,13.3f,f+.9f,.33f,.44f,.03f,.2f);kit::prop(m,model::lab_field_meter,16.35f,13.75f,f+.9f,.25f,.328f,.148f,anE+.3f);
 kit::prop(m,model::office_chair,17.3f,11.f,f,.55f,.55f,.95f,kPi*.5f);
 propPose(m,model::office_chair,18.4f,13.4f,f,.55f,.55f,.95f,2.1f,kPi*.5f,0);                   // the one who ran
 // the east wall: the analysers that wrote the sittings down, the board between them
 for(float y:{9.1f,14.2f}){anModel(m,model::lab_analyzer_cabinet,21.85f,y,f,anW,true);wall(m,20.74f,y-.92f,22.96f,y+.92f,f+3.11f,roof,30);}
 anFix(m,model::med_notice_board,22.965f,11.65f,f+1.25f,1.2f,.07f,.9f,anW,false);
 anFix(m,model::lab_pa_horn,22.7f,11.65f,f+2.7f,.495f,.564f,.433f,anW,false);
 // the north wall: instrument benches either side of the passage
 etCounter(m,1.2f,7.1f,3.8f,7.75f,f);etCounter(m,5.6f,7.1f,9.6f,7.75f,f);etCounter(m,14.6f,7.1f,18.4f,7.75f,f);
 kit::prop(m,model::lab_microscope,2.4f,7.45f,f+.9f,.1f);kit::prop(m,model::med_specimen_jar,3.3f,7.35f,f+.9f,.4f);
 anModel(m,model::lab_scope_monitor,6.4f,7.4f,f+.9f,anS,false);kit::prop(m,model::lab_field_meter,8.3f,7.42f,f+.9f,.25f,.328f,.148f,.2f);
 kit::prop(m,model::office_file_binder,9.1f,7.35f,f+.9f,.093f,.301f,.424f,0);
 anModel(m,model::lab_instrument_terminal,15.5f,7.42f,f+.9f,anS,false,.8f);kit::prop(m,model::office_paper_stack,17.2f,7.4f,f+.9f,.33f,.44f,.03f,1.3f);
 anModel(m,model::lab_wall_clock,12.f,7.12f,f+3.2f,anS,false);
 // the southwest: the old instrument cabinets, a chart table
 for(float x:{2.f,3.1f}){anModel(m,model::pwr_control_cabinet,x,16.68f,f,anN,true);wall(m,x-.4f,16.48f,x+.4f,16.88f,f+1.6f,roof,30);}
 anFix(m,model::wooden_table,6.4f,15.6f,f,1.367f,.724f,.78f,0,true);
 kit::prop(m,model::office_paper_stack,6.2f,15.6f,f+.78f,.33f,.44f,.03f,.5f);kit::prop(m,model::mug,6.85f,15.5f,f+.78f,.09f,.09f,.1f,.3f);
 crash(1.f,1.04f,14.4f,16.5f);crash(1.f,1.04f,7.4f,8.6f);
 // the west floor: where the tapes were marked up after a sitting, the cabinet they went into, the
 // instrument cart wheeled back from the glass
 anFix(m,model::wooden_table,3.4f,11.3f,f,1.367f,.724f,.78f,0,true);
 for(auto [x,y,yaw]:{std::tuple{3.f,10.6f,.2f+kPi},{3.9f,12.05f,-.3f}})kit::prop(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw);
 kit::prop(m,model::office_paper_stack,3.f,11.35f,f+.78f,.33f,.44f,.03f,.4f);kit::prop(m,model::office_file_binder,3.75f,11.2f,f+.78f,.093f,.301f,.424f,1.3f);
 kit::prop(m,model::lab_microscope,3.55f,11.45f,f+.78f,.18f,.22f,.29f,.6f);
 anModel(m,model::office_file_cabinet_open,1.42f,12.6f,f,anE,true);
 handCart(m,5.2f,9.4f,f);kit::prop(m,model::lab_field_meter,5.15f,9.2f,f+.29f,.25f,.328f,.148f,.4f);anModel(m,model::lab_scope_monitor,5.25f,9.75f,f+.29f,anE,false);
 paperDrift(m,2.f,9.6f,5.6f,13.4f,f,4,8531);
 extinguisher(m,1.f,15.f,f,anE);
 // the west seam: a framed mouth onto the passage to the test bay
 wall(m,0.f,9.f,1.f,14.f,f+2.7f,roof,28);
 paperDrift(m,1.5f,8.2f,20.4f,16.5f,f,12,8501);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{19.2f,15.7f},-999,1,0});               // the volunteer, out of the booth after all
 m.pickupSpawns.push_back({{7.6f,8.4f},PickupKind::HandgunAmmo,f});
 for(auto [x,y,fl]:{std::tuple{4.f,10.f,0.f},{4.f,14.2f,.35f},{19.f,8.6f,0.f},{19.f,15.6f,0.f}})m.lights.push_back(etTube(x,y,roof,.75f,7.f,5600,.04f,fl));
 etDeadTube(m,12.f,8.3f,roof);etDeadTube(m,12.f,15.7f,roof);
 // --- The south: the corridor in from the ring, the waiting room and the suit room ---------------
 wallDoor(m,1.f,9.8f,16.9f,17.1f,5.f,6.2f,f,roof,1);wallDoor(m,14.2f,23.f,16.9f,17.1f,18.f,19.2f,f,roof,1);
 wall(m,9.8f,16.9f,10.f,23.f,f,roof,3);wall(m,14.f,16.9f,14.2f,23.f,f,roof,3);
 wall(m,10.f,16.9f,14.f,17.1f,f+2.6f,roof,3);for(float x:{9.92f,14.f})doorTrim(m,x,16.88f,x+.08f,17.12f,f,f+2.6f);
 floorLine(m,11.94f,17.2f,12.06f,23.f,f,14);
 anFix(m,model::lab_id_scanner,13.97f,18.2f,f+1.1f,.3f,.06f,.41f,anW,false);
 m.lights.push_back(etTube(12.f,20.f,roof,.5f,5.5f,4400,.02f));
 // the waiting room (southwest): where the volunteers sat before they sat
 bench(m,3.f,22.5f,f);bench(m,6.8f,22.5f,f);
 anFix(m,model::water_cooler,9.5f,18.f,f,.35f,.35f,1.1f,anW,true);
 anModel(m,model::lab_potted_plant,1.4f,17.6f,f,0,true);
 anFix(m,model::wooden_table,4.4f,19.6f,f,1.367f,.724f,.78f,0,true);
 kit::prop(m,model::wooden_chair,4.f,18.9f,f,.417f,.525f,.95f,kPi+.15f);kit::prop(m,model::wooden_chair,4.9f,20.35f,f,.417f,.525f,.95f,-.2f);
 kit::prop(m,model::mug,4.2f,19.5f,f+.78f,.09f,.09f,.1f,.4f);kit::prop(m,model::mug,4.75f,19.7f,f+.78f,.09f,.09f,.1f,1.4f);
 anFix(m,model::med_notice_board,1.035f,20.f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anFix(m,model::med_wall_clock,9.79f,20.6f,f+2.05f,.24f,.02f,.23f,anW,false);
 taped(m,{1.01f,21.5f},f+1.45f,anE,Readable::Letter,"VOLUNTEERS / ROTA","FACE CREW C: SITTINGS 209-214 THIS WEEK. TIER 2 STAFF NOT TO SIT. TIER 3 STAFF TO SIT TWICE.",
  "SITTERS WAIT HERE AND ARE CALLED BY NUMBER. SITTERS DO NOT TALK ABOUT THE CHAIR TO THE NEXT SITTER.");
 m.lights.push_back(etTube(5.f,19.4f,roof,.8f,7.f,3000,0));
 anModel(m,model::lab_desk_lamp,4.9f,19.45f,f+.78f,anS+.3f,false);
 m.lights.push_back(lamp({4.85f,19.5f},f+1.15f,.35f,3.f,2600,0,WorldLightMount::Bare,0,false));          // left on, on the sitters' table
 // the couch along the west wall, the clerk's desk facing the benches, the board that showed the number
 anModel(m,model::office_couch_green,1.72f,19.05f,f,anE,true);
 recordsDesk(m,7.8f,20.2f,f);
 kit::prop(m,model::mug,7.8f,19.95f,f+.78f,.09f,.09f,.1f,.5f);
 anModel(m,model::lab_status_display,9.78f,21.3f,f+2.1f,anW,false);
 anModel(m,model::art_painting_5,2.6f,17.15f,f+1.3f,anS,false);anModel(m,model::radiator_panel,7.6f,17.24f,f+.1f,anS,false);
 kit::prop(m,model::item_id_card,2.9f,22.45f,f+.51f,.132f,.085f,.002f,.7f);
 paperDrift(m,2.4f,17.6f,9.f,22.f,f,5,8521);
 // the suit room (southeast): suits hung on the east wall, one hook bare
 etSuitRail(m,18.f,21.6f,23.f,f,kPi,2);
 bench(m,18.2f,20.4f,f);
 anModel(m,model::med_scrub_sink,14.5f,21.6f,f,anE,true);
 for(int i=0;i<3;++i)anFix(m,model::psx_locker,15.2f+i*.88f,22.665f,f,.64f,.85f,2.42f,kPi*.5f,true);
 wall(m,14.75f,22.1f,17.6f,23.f,f+2.42f,roof,30);
 // the sitters' dose chart on the board, the mirror over the sink, their hamper, boots off by the bench
 anModel(m,model::lab_notice_board,20.6f,17.14f,f+1.2f,anS,false);anModel(m,model::med_wall_clock,16.4f,17.12f,f+2.1f,anS,false);
 anModel(m,model::med_washroom_mirror,14.22f,21.6f,f+1.3f,anE,false,.5f);anModel(m,model::med_floor_drain,15.2f,21.6f,f+.002f,0,false);
 anModel(m,model::ind_barrel,22.45f,22.4f,f,.3f,true);anModel(m,model::radiator_panel,14.32f,19.2f,f+.1f,anE,false);
 kit::prop(m,model::mine_hard_hat,17.1f,20.38f,f+.51f,.279f,.351f,.153f,1.6f);kit::prop(m,model::item_walkie,18.9f,20.4f,f+.51f,.114f,.376f,.046f,1.2f);
 m.lights.push_back(etWall({20.6f,22.98f},f+2.4f,-kPi*.5f,.45f,5.f,3400));
 kit::prop(m,model::item_dosimeter,17.6f,20.42f,f+.51f,.31f,.146f,.066f,.4f);
 m.pickupSpawns.push_back({{20.4f,18.2f},PickupKind::Health,f});
 m.lights.push_back(etTube(18.6f,19.6f,roof,.8f,7.f,4000,.02f,.12f));
 recessedFittings(m);
 // Names, and the air: violet in the booth, stale everywhere else.
 zone(m,"RESONANCE LABORATORY / THE BOOTH",9.2f,9.7f,14.8f,14.3f,-1000,1000,{.15f,.13f,.18f,.007f});
 zone(m,"RESONANCE LABORATORY / PASSAGE",10.f,0,14.f,7.f,-1000,1000,{.13f,.13f,.13f,.005f});
 zone(m,"RESONANCE LABORATORY / ARCHIVE",1.f,1.f,9.8f,6.9f,-1000,1000,{.17f,.14f,.10f,.005f});
 zone(m,"RESONANCE LABORATORY / SAMPLE VAULT",14.2f,1.f,23.f,6.9f,-1000,1000,{.10f,.18f,.12f,.005f});
 zone(m,"RESONANCE LABORATORY / WAITING ROOM",1.f,17.1f,9.8f,23.f);
 zone(m,"RESONANCE LABORATORY / SUIT ROOM",14.2f,17.1f,23.f,23.f);
 zone(m,"RESONANCE LABORATORY",0,0,24,24,-1000,1000,{.14f,.15f,.15f,.005f});
 titleCard(m,"ENERGY TEST COMPLEX",9.8f,17.f,14.2f,24.f);
 freightFinish(m,-1);
 return p;
}

// HIGH-ENERGY TEST BAY (manifest 56). Where they hit the ore with everything the plant had: four
// capacitor towers round a shot chamber, the bank fed from a transformer the size of a lorry,
// the control room behind glass on the east gallery. Shot 31 fired at 01:57. The banks have been
// charging themselves ever since with their feeds out, and they let go every few seconds.
// Route: the E seam from the Resonance Laboratory onto the east gallery -> the control room, or
// down the long stair to the bay floor (what the shot crew left) -> the south gallery -> S to the
// Observation Ring.
std::shared_ptr<const Map> makeHighEnergyTestBay(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[86];m.definition=CampaignChunks[86];m.definition.ambient=.22f;
 m.definition.playerStart={21.8f,11.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-20,g=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'E',9.f,14.f,g,false,0});openEdge(m,{'S',10.f,14.f,g,false,0});
 machineryClock(m,"et_bay_clock",60000);
 // --- The galleries: east and south, solid to the gallery floor ---------------------------------
 wall(m,17.f,1.f,23.f,23.f,f,g,15);wall(m,23.f,9.f,24.f,14.f,f,g,15);
 wall(m,1.f,17.f,17.f,23.f,f,g,15);wall(m,10.f,23.f,14.f,24.f,f,g,15);
 for(float z=f+2.f;z<g-.5f;z+=2.f){wall(m,16.96f,1.f,17.f,17.f,z,z+.08f,12);wall(m,1.f,16.96f,17.f,17.f,z,z+.08f,12);}   // the walls' steel courses
 wall(m,16.9f,1.f,17.f,17.f,g-.32f,g,12);wall(m,1.f,16.9f,15.f,17.f,g-.32f,g,12);                // the gallery fascia
 guardRail(m,16.9f,8.f,17.f,16.9f,g);guardRail(m,1.f,16.9f,8.f,17.f,g);guardRail(m,10.f,16.9f,15.f,17.f,g);   // broken for the cross-catwalk
 // the long stair down to the bay floor, against the east gallery's face
 m.stairs.push_back({15.f,4.96f,17.f,17.f,f,g,43,true,true});
 // lit from the side as it goes down: caged lamps on the gallery's face over the flight, and one at its foot
 for(auto [y,z]:{std::pair{7.2f,f+4.25f},{12.4f,f+7.2f}}){anFix(m,model::mine_cage_lamp,16.88f,y,z,.21f,.2f,.36f,anW,false);
  m.lights.push_back(etWall({16.96f,y},z+.15f,kPi,.75f,7.f,4200));}
 anFix(m,model::mine_cage_lamp,16.88f,4.3f,f+2.3f,.21f,.2f,.36f,anW,false);m.lights.push_back(etWall({16.96f,4.3f},f+2.45f,kPi,.6f,6.f,4200));
 // --- The control room (east gallery, north): glass onto the bay --------------------------------
 const float c=g+3.2f;
 glazing(m,17.f,1.2f,17.2f,7.8f,g,c,3,.95f,2.4f,true);wall(m,17.f,1.f,17.2f,1.2f,g,c,3);wall(m,17.f,7.8f,17.2f,8.f,g,c,3);
 wallDoor(m,17.2f,23.f,7.9f,8.1f,19.4f,20.6f,g,c,-1);
 wall(m,17.f,1.f,23.f,8.1f,c,c+.2f,3);                                                         // its slab
 etCounter(m,17.25f,1.6f,17.95f,7.4f,g);
 for(float y:{2.4f,4.6f}){kit::prop(m,model::crt_monitor,17.55f,y,g+.9f,.42f,.4f,.38f,anE);kit::prop(m,model::keyboard,17.82f,y,g+.9f,.45f,.16f,.03f,anE);}
 anModel(m,model::lab_scope_monitor,17.55f,6.5f,g+.9f,anE,false);
 for(auto [y,yaw]:{std::pair{2.5f,kPi*.5f+.2f},{4.7f,kPi*.5f-.25f}})kit::prop(m,model::office_chair,18.7f,y,g,.55f,.55f,.95f,yaw);
 wall(m,19.6f,1.f,21.4f,1.015f,g+.92f,g+1.68f,20);anModel(m,model::lab_control_board,20.5f,1.11f,g+1.f,anS,false);
 for(float x:{21.85f,22.25f})anModel(m,model::lab_knife_switch,x,1.1f,g+1.3f,anS,false);
 anModel(m,model::lab_wall_clock,20.5f,1.02f,g+2.3f,anS,false);
 terminal(m,{22.4f,4.2f},"SHOT LOG / BAY 2","SHOT 31 01:57: R-12 IN THE CHAMBER, BANKS TO FULL, FIRED. THE CHAMBER READ MORE OUT THAN THE BANKS PUT IN.",
  "02:05 FEEDS OUT, BANKS DUMPED TO GROUND. 02:06 BANKS READING FULL. 02:09 FULL. NOBODY ON THE FLOOR.",g-f);
 m.terminals.back().yaw=anW;
 taped(m,{22.99f,6.4f},g+1.5f,anW,Readable::Note,"BANKS / DO NOT GO DOWN","THEY LET GO EVERY FEW SECONDS. COUNT THREE AFTER A FLASH AND YOU CAN CROSS. SAUL COUNTED TWO.",
  "THE FEEDS ARE OUT. I PULLED THEM MYSELF. THE TOWERS ARE FILLING FROM SOMETHING AND IT IS NOT US.");
 m.lights.push_back(etTube(20.f,3.f,c,.42f,4.5f,3200,0));m.lights.push_back(etTube(20.5f,6.2f,c,.35f,4.f,3200,0,.25f));
 m.lights.push_back(lamp({17.6f,3.5f},g+1.25f,.25f,2.4f,9000,-.15f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.1f;   // the screens on the glass
 m.lights.push_back(etBeacon({22.98f,2.4f},g+2.5f,kPi,.3f,0));
 // --- The east gallery: staging, cable, the way in ----------------------------------------------
 anFix(m,model::cable_reel,21.9f,16.6f,g,1.f,2.f,2.f,0,true);
 anFix(m,model::pallet,21.6f,19.6f,g,1.2f,1.15f,.16f,0,true);anModel(m,model::lab_specimen_crate,21.6f,19.6f,g+.16f,.2f,true,.8f);
 anModel(m,model::mine_portable_generator,19.6f,21.9f,g,.3f,true);
 anFix(m,model::pwr_cable_bundle,20.f,15.f,g,4.f,.14f,.22f,kPi*.5f,false);
 extinguisher(m,23.f,10.f,g,anW);
 wallSign(m,22.94f,15.6f,g+2.2f,"HIGH VOLTAGE","STAY OFF THE BAY FLOOR WHILE THE TOWERS ARE UP",2.4f,anW);
 for(float y:{16.6f,19.f})m.lights.push_back(etWall({22.98f,y},g+2.8f,kPi,.6f,7.f,3400));
 // --- The south gallery: the shot crew's lockers and suits --------------------------------------
 etSuitRail(m,1.6f,6.f,23.f,g,-kPi*.5f,4);
 for(int i=0;i<3;++i)anFix(m,model::psx_locker,7.f+i*.86f,22.55f,g,.64f,.9f,2.3f,kPi*.5f,true);
 wall(m,6.65f,22.1f,9.5f,23.f,g+2.3f,roof,30);
 bench(m,4.f,19.2f,g);
 kit::prop(m,model::mine_hard_hat,5.6f,20.4f,g,.279f,.351f,.153f,.6f);
 m.pickupSpawns.push_back({{2.2f,18.4f},PickupKind::Ammo,g});
 for(float x:{4.f,15.f})m.lights.push_back(etWall({x,22.98f},g+2.8f,-kPi*.5f,.55f,7.f,3400,x>8?.3f:0.f));
 // --- The bay floor: four towers round the shot chamber, the transformer, the crane --------------
 equipmentPad(m,6.f,6.f,11.f,11.f,f);
 anFix(m,model::lab_test_chamber,8.5f,8.5f,f+.1f,3.42f,2.04f,1.92f,0,true);                     // the shot chamber
 wall(m,8.2f,8.2f,8.8f,8.8f,f+2.02f,f+3.2f,8);                                               // its insulator stack, the bars' terminal
 m.lights.push_back(etGlow({8.5f,9.6f},f+1.1f,.3f,2.6f,etViolet,.4f));                         // R-12, still in there
 const Vec2 towers[4]={{3.5f,3.5f},{13.5f,3.5f},{3.5f,14.f},{13.5f,14.f}};
 for(int i=0;i<4;++i){const Vec2 t=towers[i];
  vessel(m,t,.7f,f,f+9.f,5);
  m.pipes.push_back({t,{8.5f,8.5f},f+8.6f,.09f,f+3.f,4});                                       // the busbar down to the chamber
  for(float z:{f+3.f,f+5.f,f+7.f})m.pipes.push_back({t,t,z,.95f,z+.12f,20});                    // the grading rings
  m.lights.push_back(etArcLight(t,f+9.3f,.6f,7.f,3.f+i*.4f,.35f,i*.7f));}
 etArc(m,6.f,6.f,11.f,11.f,f,f+2.4f,35,3.2f,.8f);
 m.lights.push_back(etArcLight({8.5f,10.1f},f+2.6f,.8f,8.f,3.2f,.8f));
 etArc(m,2.6f,13.1f,4.4f,14.9f,f,f+2.6f,30,2.1f,.5f,1.f);                                        // tower 3 lets go to the floor
 // the bay's step-up set on the west wall: three pack transformers on their pad, their bushings bussed up the
 // wall to the gantry spoke, an isolator beside them that was opened at 01:58 and made no difference
 equipmentPad(m,1.2f,6.9f,3.4f,11.1f,f);
 for(float y:{7.7f,9.f,10.3f}){anModel(m,model::pwr_step_transformer,2.2f,y,f+.1f,anE,true,1.3f);
  m.pipes.push_back({{2.2f,y},{1.2f,y},f+2.35f,.05f,f+2.35f,4});m.pipes.push_back({{1.15f,y},{1.15f,y},f+2.35f,.05f,roof,4});}
 anModel(m,model::pwr_disconnect_switch,1.3f,11.55f,f+1.f,anE,false);wall(m,1.f,11.3f,1.08f,11.8f,f+.95f,f+1.9f,8);
 wall(m,1.2f,6.9f,3.4f,11.1f,f+2.28f,f+2.8f,30);                                                     // nobody on their tops
 for(float y:{8.2f,9.8f})anFix(m,model::pwr_cable_bundle,5.4f,y,f,4.f,.14f,.22f,0,false);
 // the crane: its girder east-west over the chamber, the hook block down on its rope
 wall(m,1.f,9.3f,23.f,9.9f,roof-3.6f,roof-2.8f,13);
 m.pipes.push_back({{4.6f,9.6f},{4.6f,9.6f},f+5.f,.02f,roof-3.6f,6});wall(m,4.4f,9.4f,4.8f,9.8f,f+4.6f,f+5.f,12);
 // The bay read in three tiers. FLOOR: each tower's foot lit cold from its plinth. MID: the
 // charging gantry, a ring of grating hung on rods round the tower heads with a spoke to each
 // wall, its lamps looking down the towers. TOP: the emitter, hung from the roof over the chamber.
 for(int i=0;i<4;++i){const Vec2 t=towers[i];
  wall(m,t.x-1.f,t.y-1.f,t.x+1.f,t.y+1.f,f,f+.3f,15);floorLine(m,t.x-1.f,t.y-1.f,t.x+1.f,t.y-.88f,f+.3f,13);
  // the foot lamp on the tower's south-east face: the face the galleries and the stair foot look at
  m.lights.push_back(etGlow({t.x+1.25f,t.y+1.25f},f+.6f,1.f,7.5f,{.75f,.85f,1.f}));
  // the tower heads at the galleries' eye: a stepped pale cap, a grading ring, the copper terminal the busbar leaves from
  m.pipes.push_back({t,t,f+9.f,.62f,f+9.14f,5});m.pipes.push_back({t,t,f+9.14f,.46f,f+9.4f,5});
  m.pipes.push_back({t,t,f+9.5f,1.f,f+9.58f,5});m.pipes.push_back({t,t,f+9.4f,.2f,f+9.9f,4});m.pipes.push_back({t,t,f+9.9f,.28f,f+9.98f,4});}
 const float gz=f+10.4f;                                                                       // the gantry deck
 for(auto [x1,y1,x2,y2]:{std::tuple{4.6f,4.6f,12.4f,5.4f},{4.6f,12.6f,12.4f,13.4f},{4.6f,4.6f,5.4f,13.4f},{11.6f,4.6f,12.4f,13.4f},   // the ring
   {8.1f,1.f,8.9f,4.6f},{1.f,8.6f,4.6f,9.4f},{12.4f,8.6f,14.6f,9.4f}}){                           // spokes: north wall, west wall, the stair head
  wall(m,x1,y1,x2,y2,gz-.12f,gz,8);
  const bool alongX=x2-x1>y2-y1;
  for(float s:{0.f,1.f}){const float a=alongX?(s?y2-.04f:y1):(s?x2-.04f:x1);
   if(alongX)wall(m,x1,a,x2,a+.04f,gz+1.f,gz+1.05f,18);else wall(m,a,y1,a+.04f,y2,gz+1.f,gz+1.05f,18);}
  for(float t:{.1f,.35f,.65f,.9f}){const Vec2 c=alongX?Vec2{x1+(x2-x1)*t,(y1+y2)*.5f}:Vec2{(x1+x2)*.5f,y1+(y2-y1)*t};
   m.pipes.push_back({c,c,gz,.025f,roof-.05f,6});}}
 for(auto c:{Vec2{5.f,5.f},Vec2{12.f,5.f},Vec2{5.f,13.f},Vec2{12.f,13.f}}){
  anFix(m,model::mine_cage_lamp,c.x,c.y,gz-.48f,.21f,.2f,.36f,0,false);
  m.lights.push_back(lamp(c,gz-.55f,.85f,10.f,2300,0,WorldLightMount::Bare));}
 m.lights.push_back(etArcLight({8.5f,5.f},gz+.6f,.6f,9.f,2.4f,.3f,.4f));
 // THE STACK: the bank's own capacitor column, from the shot chamber's insulator to the roof, can on can,
 // copper and steel, a grading ring at every joint and a lamp on every can: the one thing in the bay you
 // can see from anywhere in it, and what the towers have been filling since 01:57.
 for(int k=0;;++k){const float z0=f+3.2f+k*2.6f;if(z0>roof-1.2f)break;const float z1=std::min(z0+2.2f,roof-.5f);
  vessel(m,{8.5f,8.5f},.5f,z0,z1,k%2?5:4);
  m.pipes.push_back({{8.5f,8.5f},{8.5f,8.5f},z1,.24f,z0+2.6f,20});                                   // the insulator neck
  for(float dz:{.35f,1.85f})m.pipes.push_back({{8.5f,8.5f},{8.5f,8.5f},std::min(z0+dz,z1-.1f),.86f,std::min(z0+dz,z1-.1f)+.08f,20});
  for(float a:{.8f,2.4f,3.9f,5.5f}){const Vec2 c{8.5f+std::cos(a)*.62f,8.5f+std::sin(a)*.62f};m.pipes.push_back({c,c,z0+.2f,.05f,z1-.2f,6});}   // the tie rods
  // one lamp a can, turning round the stack can by can so every face is lit somewhere up its height
  const float a=kPi*.25f+k*kPi*.5f;
  auto l=lamp({8.5f+std::cos(a)*1.3f,8.5f+std::sin(a)*1.3f},z0+1.1f,1.15f,8.5f,8000,-.1f,WorldLightMount::Bare,0,false);l.flicker=.05f;m.lights.push_back(l);}
 m.pipes.push_back({{8.5f,8.5f},{8.5f,8.5f},roof-.5f,.7f,roof,12});                                     // its head plate in the roof
 // THE CROSS-CATWALK: off the south gallery, straight across the bay at gallery height, round the stack
 // on a collar and on to the north wall, hung from the roof on rods: the crossing they read the cans from.
 deck(m,g,8,1,10,17);deck(m,g,7,7,8,10);
 for(auto& L:m.layers)if(L.thickness>0&&std::fabs(L.elevation-g)<.01f)deckHangers(m,L,roof);
 for(auto [x1,y1,x2,y2]:{std::tuple{7.92f,1.f,8.f,7.f},{7.92f,10.f,8.f,16.9f},{6.92f,7.f,7.f,10.f},{7.f,6.92f,8.f,7.f},{7.f,10.f,8.f,10.08f},{10.f,1.f,10.08f,16.9f}})guardRail(m,x1,y1,x2,y2,g);
 for(float y:{3.f,13.f}){anFix(m,model::mine_cage_lamp,10.03f,y,g+1.1f,.21f,.2f,.36f,0,false);m.lights.push_back(lamp({10.03f,y},g+1.3f,.4f,5.f,3400,0,WorldLightMount::Bare,0,false));}
 // the bay's walls in bays: concrete piers floor to roof, a steel course every four metres
 for(float x:{6.f,11.f,15.f})wall(m,x-.25f,1.f,x+.25f,1.38f,f,roof,15);
 for(float y:{5.5f,13.7f})wall(m,1.f,y-.25f,1.38f,y+.25f,f,roof,15);
 for(float z=f+4.f;z<roof-1.f;z+=4.f){wall(m,1.f,1.f,16.9f,1.08f,z,z+.18f,12);wall(m,1.f,1.f,1.08f,16.9f,z,z+.18f,12);}
 // the floor's walls painted to head height, grey-green with a striped edge, so the bay floor reads
 // as a working floor under the bare concrete above
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,16.97f,1.03f},{1.f,1.03f,1.03f,16.97f},{16.97f,1.f,17.f,16.97f},{1.03f,16.97f,17.f,17.f}}){
  wall(m,x1,y1,x2,y2,f,f+2.4f,2);wall(m,x1,y1,x2,y2,f+2.4f,f+2.48f,13);}
 // what the crew left: the one who counted two, his meter, his ammunition
 etBody(m,"et_bay_clock",ActorVisual::LabTech,{11.6f,12.3f},f,kPi*.25f);
 kit::prop(m,model::lab_field_meter,12.4f,11.6f,f,.25f,.328f,.148f,1.2f);
 m.pickupSpawns.push_back({{12.6f,12.9f},PickupKind::Ammo,f});
 kit::prop(m,model::item_dosimeter,10.4f,14.6f,f,.31f,.146f,.066f,2.4f);
 propPose(m,model::anl_cell_spent,5.2f,12.4f,f,1.7f,kPi*.5f,0);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{6.2f,15.f},f,1,0});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{9.6f,2.6f},f,1,0});
 // A charging unit stood dead among the step-up set: a fight on the floor wakes it, and between it and the
 // stair foot is the shot chamber's pad, live every 3.2 s. Brought across it, it does not get off it.
 m.creatureSpawns.push_back({CreatureKind::Machine,{4.4f,9.f},f});
 // light: sodium high-bays far overhead, two of them out; the towers' blue; the chamber's violet
 etHighBay(m,{5.f,6.f},roof,-6.f,2.2f,22.f,2000);etHighBay(m,{12.f,6.f},roof,-4.f,0,0,0);
 etHighBay(m,{5.f,13.f},roof,-6.f,2.f,22.f,4300,.25f);etHighBay(m,{12.f,13.f},roof,-4.f,1.4f,18.f,2100);   // one metal-halide refit among the sodium
 etHighBay(m,{20.f,14.f},roof,-4.f,1.2f,16.f,4000);
 m.lights.push_back(etWall({1.02f,15.f},f+3.f,0,.35f,5.f,3000,0,{1.f,.15f,.08f}));              // the floor's emergency lamp
 hazard(m,Hazard::Kind::Anomaly,7.f,7.f,10.f,10.f,f,f+3.f,0);anHum(m,"et_bay_hum",5.5f,5.5f,11.5f,11.5f,f,f+4.f,.55f);
 zone(m,"HIGH-ENERGY TEST BAY / CONTROL ROOM",17.f,1.f,23.f,8.f);
 zone(m,"HIGH-ENERGY TEST BAY / BAY FLOOR",1.f,1.f,17.f,17.f,f-1.f,g-.5f,{.1f,.1f,.12f,.005f});
 zone(m,"HIGH-ENERGY TEST BAY / GALLERY",0,0,24,24,g-.5f,1000,{.11f,.11f,.12f,.004f});
 zone(m,"HIGH-ENERGY TEST BAY",0,0,24,24);
 freightFinish(m,-1);etStairRails(m,0,1);
 return p;
}

// SHIELDING CORRIDOR (manifest 57). The only way west from the ring: lead-faced walls and three
// bulkheads built to stop a burst. At 01:58 one came down it and the shielding held; the four
// men walking it are on the north wall now, in soot, where they stood. A Shielding detail holds
// the dosimetry post with orders that nothing passes either way. They are breaking: two men,
// one magazine each, and a radio they have stopped answering.
// Route: the E seam from the ring -> the corridor west, past the post -> the bend -> the blast
// door -> S to Experimental Power. The shielding store off the bend.
std::shared_ptr<const Map> makeShieldingCorridor(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[91];m.definition=CampaignChunks[91];m.definition.ambient=.23f;
 m.definition.playerStart={21.8f,11.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 auto& L=m.layers[0];
 rectangle(L,1,1,23,23,'#');
 rectangle(L,9,9,23,14,'.');      // the corridor west from the ring
 rectangle(L,9,14,15,23,'.');     // the leg south to Experimental Power
 rectangle(L,14,4,21,9,'.');      // the dosimetry post
 rectangle(L,2,15,9,22,'.');      // the shielding store
 openEdge(m,{'E',9.f,14.f,f,false,0});openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"et_shield_clock",60000);
 // --- The lead facing and the shadows on it ------------------------------------------------------
 // the north face is lead wherever it is not the post's door (16.4-17.6) or its window (19.4-20.6)
 // (lead sheet: dull grey, the pale steel finish; it takes the work lights and shows what burnt onto it)
 for(float x=9.2f;x<22.9f;x+=1.2f){if(x+1.14f<16.36f||x>20.64f)wall(m,x,9.f,x+1.14f,9.06f,f,f+2.6f,20);if(x>15.f)wall(m,x,13.94f,x+1.14f,14.f,f,f+2.6f,20);}
 wall(m,17.66f,9.f,18.58f,9.06f,f,f+2.6f,20);wall(m,20.66f,9.f,21.14f,9.06f,f,f+2.6f,20);
 for(float y=14.2f;y<22.9f;y+=1.2f){if(y+1.14f<16.9f||y>18.5f)wall(m,9.f,y,9.06f,y+1.14f,f,f+2.6f,20);wall(m,14.94f,y,15.f,y+1.14f,f,f+2.6f,20);}
 wall(m,9.f,9.f,23.f,9.04f,f+2.6f,f+2.75f,12);wall(m,15.f,13.96f,23.f,14.f,f+2.6f,f+2.75f,12);
 // four men where they stood at 01:58, burnt onto the lead: a body and a head each. Soot is a stain,
 // not a thing: none of it collides, and none of it hangs across the post's doorway.
 // Lead burns to its red oxide: the men are finish 47, the floor under them dark iron.
 auto burnt=[&](float x1,float y1,float x2,float y2,float lo,float hi,int mat){wall(m,x1,y1,x2,y2,lo,hi,mat);m.structures.back().solid=false;};
 const std::tuple<float,float,bool> men[4]={{14.2f,1.72f,true},{15.75f,1.8f,true},{18.05f,1.65f,false},{20.9f,1.78f,false}};
 float soot=.002f;
 for(auto [x,h,thrown]:men){soot+=.0015f;
  burnt(x-.24f,9.06f,x+.24f,9.07f,f+.85f,f+h-.28f,47);burnt(x-.12f,9.06f,x+.12f,9.07f,f+h-.26f,f+h,47);
  burnt(x-.2f,9.06f,x-.08f,9.07f,f,f+.85f,47);burnt(x+.08f,9.06f,x+.2f,9.07f,f,f+.85f,47);
  burnt(x-.36f,9.06f,x-.24f,9.07f,f+.9f,f+h-.5f,47);burnt(x+.24f,9.06f,x+.34f,9.07f,f+.95f,f+h-.55f,47);   // the arms, thrown up
  // the burst came down the corridor from the ring: each man's soot is thrown long to the west of
  // him, on the lead where there is lead and across the floor, and what he carried lies at his feet
  if(thrown){burnt(x-1.3f,9.07f,x-.24f,9.075f,f+.95f,f+h-.45f,47);burnt(x-.9f,9.07f,x-.2f,9.075f,f+.15f,f+.8f,47);}
  burnt(x-1.6f,9.07f,x+.3f,10.6f,f+soot,f+soot+.001f,8);burnt(x-2.4f,9.07f,x-1.6f,9.9f,f+soot,f+soot+.001f,8);}
 {
  for(auto [x,y,yaw]:{std::tuple{14.f,9.5f,.7f},{15.8f,9.8f,2.2f},{20.9f,9.85f,-.5f}})kit::prop(m,model::mine_hard_hat,x,y,f,.279f,.351f,.153f,yaw);
  kit::prop(m,model::item_dosimeter,17.9f,9.7f,f,.31f,.146f,.066f,1.3f);kit::prop(m,model::lab_field_meter,15.2f,10.1f,f,.25f,.33f,.15f,2.6f);
  for(auto [x,y,yaw]:{std::tuple{14.6f,10.4f,.3f},{16.2f,10.f,1.9f},{21.2f,10.2f,.8f}})kit::prop(m,model::office_paper_sheet,x,y,f+.001f,.4f,.293f,.004f,yaw);
  // lead facing the heat brought down: panels slumped off their fixings onto the floor
  wall(m,17.68f,9.08f,18.56f,9.5f,f,f+.05f,20);wall(m,21.3f,9.1f,21.9f,9.75f,f,f+.04f,20);}
 // the light on them: a caged work lamp on the south wall, a low fill across at them, and the flood the
 // detail stood at the bulkhead to look at them by, raking along the lead from the west so every edge of
 // every man throws its own shadow; the fourth man has his own, past bulkhead 2
 anFix(m,model::mine_cage_lamp,16.3f,13.8f,f+2.25f,.21f,.2f,.36f,kPi,false);wall(m,16.2f,13.92f,16.4f,13.94f,f+2.2f,f+2.7f,12);
 {auto l=lamp({16.3f,13.9f},f+2.4f,.4f,6.f,3600,.25f,WorldLightMount::Wall,-kPi*.5f,true,{.85f,1.f,.5f});l.flicker=.06f;m.lights.push_back(l);}
 for(auto [x,y,heading,power,reach]:{std::tuple{13.35f,9.42f,0.f,2.4f,8.f},{22.62f,9.42f,kPi,1.3f,4.5f}}){
  wall(m,x-.025f,y-.025f,x+.025f,y+.025f,f,f+1.42f,12);wall(m,x-.2f,y-.2f,x+.2f,y+.2f,f,f+.03f,8);                 // the stand, its foot
  for(float d:{-.16f,.16f})wall(m,x+d-.012f,y-.012f,x+d+.012f,y+.012f,f,f+.9f,12);
  anFix(m,model::mine_cage_lamp,x,y,f+1.4f,.21f,.2f,.36f,heading>1?kPi*.5f:-kPi*.5f,false);
  {auto l=lamp({x+(heading>1?-.14f:.14f),y},f+1.6f,power,reach,3900,.2f,WorldLightMount::Bare,0,true,{.88f,1.f,.62f});l.flicker=.04f;m.lights.push_back(l);}}
 // what was on them, under them: run and fused to the floor where they stood
 for(auto [x,seed]:{std::pair{14.2f,1},{15.75f,2},{18.05f,3},{20.9f,4}}){if(seed==3)continue;                   // the third's under the slumped lead
  wall(m,x-.32f,9.08f,x+.18f,9.4f,f+.001f,f+.003f,8);wall(m,x-.12f,9.12f,x+.3f,9.44f,f+.001f,f+.0032f,8);wall(m,x-.05f,9.2f,x+.12f,9.36f,f+.0032f,f+.004f,20);   // the run of it
  if(seed%2)propPose(m,model::item_walkie,x+.2f,9.22f,f+.004f,.11f,.38f,.05f,seed*.9f,.25f,1.3f);
  else propPose(m,model::mug,x-.05f,9.25f,f+.004f,.12f,.12f,.1f,seed*.7f,.6f,.4f);
  propPose(m,model::item_id_card,x+.32f,9.55f,f+.004f,.132f,.085f,.002f,seed*1.3f,.06f,.1f);
  if(seed==2)propPose(m,model::item_dosimeter,x-.35f,9.25f,f+.004f,.31f,.146f,.066f,seed*2.1f,.2f,.35f);}
 // the corridor's ribs: lead-lined steel frames across the ceiling every bay, the cable run on the south wall
 for(float x:{10.f,14.4f,16.8f,21.f})wall(m,x-.12f,9.f,x+.12f,14.f,roof-.3f,roof,12);
 cableTray(m,{15.2f,13.7f},{22.8f,13.7f},f+3.2f);
 // --- The bulkheads: concrete frames a metre deep, the leaves swung back --------------------------
 for(float x:{18.6f,12.2f}){
  wall(m,x,9.f,x+.8f,10.f,f,roof,15);wall(m,x,13.f,x+.8f,14.f,f,roof,15);wall(m,x,10.f,x+.8f,13.f,f+2.6f,roof,15);
  for(float y:{10.f,12.96f})wall(m,x-.04f,y,x+.84f,y+.04f,f,f+2.6f,13);
  if(x>15)wall(m,x-1.6f,13.7f,x-.1f,13.94f,f,f+2.5f,8);else wall(m,x-1.6f,9.06f,x-.1f,9.3f,f,f+2.5f,8);}                                                 // the leaf, back against the lead
 m.lights.push_back(etBeacon({18.58f,11.5f},f+2.8f,kPi,0,0));m.lights.push_back(etBeacon({12.18f,11.5f},f+2.8f,kPi,.6f,0));
 taped(m,{19.41f,9.6f},f+1.5f,anE,Readable::Note,"BULKHEAD 2 / 02:15","THE SHIELD HELD. EVERY METER WEST OF IT READ CLEAN. LOOK AT THE WALL BEHIND YOU.",
  "WE TRIED TO SCRUB THEM OFF AT 02:30. BY 02:40 THEY WERE BACK. THEY ARE A LITTLE FURTHER WEST EACH TIME.");
 // the bend's blast door, a real leaf, and the line nobody crossed without a badge
 wall(m,9.f,17.6f,10.5f,18.4f,f,roof,15);wall(m,13.5f,17.6f,15.f,18.4f,f,roof,15);wall(m,10.5f,17.6f,13.5f,18.4f,f+2.6f,roof,15);
 // Held shut from the post since 02:25 (et_shield_blast): its board is in the dosimetry post, among the detail, and
 // its repeater is in the shielding store, with the one man of the detail who has stopped.
 {Door d{10.5f,13.5f,18.f,0};d.requireState=stateId("et_shield_blast");d.label="BLAST DOOR";d.locked="BLAST DOOR / HELD FROM THE POST";m.doors.push_back(d);}
 floorLine(m,9.f,17.3f,15.f,17.45f,f,13);floorLine(m,9.f,18.55f,15.f,18.7f,f,13);
 taped(m,{14.25f,17.6f},f+1.5f,anN,Readable::Note,"BLAST DOOR / 02:25","SHUT FROM THE POST BOARD. NOTHING FROM THE RING GOES WEST. NOTHING FROM THE WEST GOES EAST.",
  "THE STORE HAS THE REPEATER AND THE STORE IS HARRIS'S. NOBODY GOES IN THE STORE.");
 m.lights.push_back(etBeacon({12.f,17.58f},f+2.75f,-kPi*.5f,.3f,stateId("et_shield_blast")));
 {auto l=etWall({12.f,17.58f},f+2.75f,-kPi*.5f,.3f,4.f,4000,0,etGreen);l.enabledState=stateId("et_shield_blast");m.lights.push_back(l);}
 m.ambients.push_back({Sound::OneMetalBang,{12.f,18.f},f+1.6f,.6f,18.f,AmbientEmitter::OnState,8,8,stateId("et_shield_blast"),1});
 m.ambients.push_back({Sound::OneMetalFall,{12.f,18.f},f+2.f,.55f,14.f,AmbientEmitter::OnState,8,8,stateId("et_shield_blast"),1});
 m.objectiveTexts.push_back({stateId("et_obj_blast"),"THE BLAST DOOR IS HELD FROM THE POST. ITS BOARD IS IN THE DOSIMETRY POST, WITH THE DETAIL."});
 etWhen(m,"et_shield_obj_door",9.f,14.f,15.f,17.6f,{{"et_shield_blast",0}},{etObjective("et_obj_blast",ObjectiveStatus::Active)});
 etWhen(m,"et_shield_obj_open",0,0,24,24,{{"et_shield_blast",1}},{etObjective("et_obj_blast",ObjectiveStatus::Complete)});
 // --- The dosimetry post: the Shielding detail's room ------------------------------------------
 wall(m,14.f,8.9f,16.4f,9.f,f,roof,3);swingDoor(m,16.4f,17.6f,8.95f,f,roof,-1);wall(m,17.6f,8.9f,19.4f,9.f,f,roof,3);
 glazing(m,19.4f,8.9f,20.6f,9.f,f,roof,3,.95f,2.1f);wall(m,20.6f,8.9f,21.f,9.f,f,roof,3);                    // the post's window on the corridor, east of bulkhead 2
 etCounter(m,18.f,4.f,21.f,4.6f,f);
 kit::prop(m,model::crt_monitor,19.f,4.3f,f+.9f,.42f,.4f,.38f,anS);anFix(m,model::cctv_tv,20.3f,4.3f,f+.9f,.5f,.4f,.42f,anS,false);
 kit::prop(m,model::item_walkie,20.72f,6.1f,f+1.f,.114f,.376f,.046f,0.f);
 kit::prop(m,model::office_chair,19.2f,5.25f,f,.55f,.55f,.95f,0.1f);
 for(int i=0;i<3;++i)anFix(m,model::psx_locker,14.335f,5.f+i*.88f,f,.64f,.85f,2.42f,0,true);
 wall(m,14.f,4.56f,14.9f,7.2f,f+2.42f,roof,30);
 // the dose board: the detail's readings pinned over the counter, the dosimeters back in their tray,
 // the records of who walked the corridor and when, and their gear by the door
 anModel(m,model::lab_notice_board,19.5f,4.04f,f+1.45f,anS,false);
 for(float y:{5.7f,6.62f})anModel(m,model::med_records_cabinet,20.75f,y,f,anW,true);
 for(auto [y,yaw]:{std::pair{5.5f,.2f},{5.75f,1.6f},{6.5f,.9f}})kit::prop(m,model::item_dosimeter,20.7f,y,f+1.f,.31f,.146f,.066f,yaw);
 kit::prop(m,model::office_file_binder,20.8f,6.85f,f+1.f,.093f,.301f,.424f,kPi*.5f);
 kit::prop(m,model::mug,20.75f,4.25f,f+.9f,.09f,.09f,.1f,.7f);
 kit::prop(m,model::mine_hard_hat,15.9f,7.6f,f,.279f,.351f,.153f,1.2f);
 anModel(m,model::radiator_panel,16.f,4.13f,f+.1f,anS,false);
 anModel(m,model::lab_pa_horn,20.7f,4.3f,f+2.6f,anW,false);
 extinguisher(m,21.f,8.2f,f,anW);
 anModel(m,model::ind_barrel,20.4f,8.45f,f,.4f,true);
 paperDrift(m,15.2f,5.f,19.6f,8.4f,f,4,9101);
 {const int dl=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,18.27f,4.32f,f+.9f,.18f,.4f,.52f,.4f);
  m.lights.push_back(lamp({18.45f,4.6f},f+1.3f,.35f,2.8f,2700,0,WorldLightMount::Bare));m.lights.back().prop=dl;}
 anFix(m,model::lab_id_scanner,16.2f,8.88f,f+1.1f,.3f,.06f,.41f,anN,false);
 etPaper(m,{20.2f,4.35f},.9f,0,Readable::Letter,"I.I. / SHIELDING DETAIL / 02:20","HOLD THE CORRIDOR. NOTHING FROM THE RING GOES WEST. NOTHING FROM THE WEST GOES EAST.",
  "ANYONE WITHOUT A DEPTHWORKS BADGE ISSUED TONIGHT IS EXPOSED. CONTRACT SECURITY INCLUDED. YOU KNOW WHAT TO DO.");
 terminal(m,{17.2f,4.75f},"POST / BLAST DOOR BOARD","BLAST DOOR AT THE BEND: SHUT 02:25 ON I.I. ORDER. REPEATER IN THE SHIELDING STORE, LOCKED TO THIS BOARD.",
  "E / RELEASE THE BLAST DOOR.",0,stateId("et_shield_blast"));
 m.terminals.back().yaw=0;
 m.lights.push_back(etTube(17.5f,6.5f,roof,.75f,6.f,4600,0));m.lights.push_back(etTube(19.9f,7.1f,roof,.5f,5.f,4600,0,.1f));
 m.lights.push_back(lamp({20.3f,4.6f},f+1.3f,.22f,2.2f,8000,-.15f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.12f;
 m.pickupSpawns.push_back({{15.4f,8.f},PickupKind::HandgunAmmo,f});
 // --- The shielding store: lead in stacks, the panels nobody fitted -------------------------------
 // West wall: brick on pallets, one stack half drawn down. North wall: the fitters' aprons and suits on
 // their rail. The middle: sheet on its bearers, the cart they were loading. East, south of the door: the
 // portable shield blocks. South wall: the store's shelving, meters and hats. One tube and a caged
 // work lamp over the sheet; the south-west corner is left to the fitter.
 wall(m,8.8f,15.f,9.f,17.f,f,roof,3);doorwayY(m,17.f,18.4f,8.8f,9.f,f,roof);wall(m,8.8f,18.4f,9.f,22.f,f,roof,3);
 for(auto [y,courses]:{std::pair{16.2f,5},{17.9f,3},{19.6f,6}}){anFix(m,model::pallet,2.8f,y,f,1.2f,1.15f,.16f,0,true);
  for(int c=0;c<courses;++c){const float in=(c==courses-1&&courses!=6)?.22f:0.f,z=f+.16f+c*.1f;                 // lead brick, course on course, bonded
   wall(m,2.3f+(c%2)*.03f,y-.45f+in,3.3f-(c%2)*.03f,y+.45f,z,z+.095f,8);}}
 kit::prop(m,model::item_dosimeter,2.75f,18.f,f+.455f,.31f,.146f,.066f,.7f);
 for(int i=0;i<3;++i)anFix(m,model::pwr_sheet_steel_stack,5.6f,19.5f,f+.1f+i*.13f,1.79f,3.4f,.13f,i*.04f,true);
 for(float y:{18.3f,19.5f,20.7f})wall(m,4.6f,y-.06f,6.6f,y+.06f,f,f+.1f,10);                      // its bearers
 handCart(m,7.3f,16.3f,f);kit::prop(m,model::lab_field_meter,7.3f,16.2f,f+.29f,.25f,.328f,.148f,.6f);
 etSuitRail(m,2.6f,5.6f,15.f,f,kPi*.5f,2);
 for(float x:{8.4f,8.52f,8.64f})wall(m,x,15.1f,x+.07f,16.8f,f,f+1.9f+(x-8.4f)*1.4f,8);          // panels stood on edge in their rack, never fitted
 for(float y:{15.3f,16.6f})wall(m,8.3f,y,8.8f,y+.05f,f,f+1.f,12);
 anFix(m,model::urb_jersey_barrier,8.25f,19.8f,f,2.479f,.814f,1.136f,kPi*.5f,true);
 anFix(m,model::urb_jersey_barrier,7.36f,20.25f,f,2.479f,.814f,1.136f,kPi*.5f,true);
 for(float x:{3.2f,5.1f}){shelf(m,{x,21.72f},0,0);wall(m,x-.9f,21.47f,x+.9f,21.97f,f+1.95f,roof,30);}
 kit::prop(m,model::mine_hard_hat,2.8f,21.7f,f+.972f,.279f,.351f,.153f,.4f);kit::prop(m,model::lab_field_meter,3.5f,21.7f,f+.972f,.25f,.328f,.148f,2.9f);
 kit::prop(m,model::mine_hard_hat,3.3f,21.7f,f+1.656f,.279f,.351f,.153f,1.9f);kit::prop(m,model::prop_cardboard_box,2.75f,21.72f,f+.306f,.436f,.432f,.22f,.05f);
 kit::prop(m,model::item_dosimeter,4.6f,21.7f,f+.972f,.31f,.146f,.066f,.2f);kit::prop(m,model::item_dosimeter,5.f,21.72f,f+.972f,.31f,.146f,.066f,1.4f);
 kit::prop(m,model::office_file_binder,5.6f,21.72f,f+.972f,.093f,.301f,.424f,kPi*.5f);kit::prop(m,model::prop_cardboard_box,5.2f,21.72f,f+.306f,.436f,.432f,.22f,0);
 kit::prop(m,model::mine_hard_hat,4.4f,17.1f,f,.279f,.351f,.153f,2.f);
 taped(m,{7.1f,15.01f},f+1.5f,anS,Readable::Note,"SHIELDING STORE / ISSUE","PANELS 14-19 FOR THE POST WALL: DRAWN 01:40, NOT FITTED. BRICK: 3 PALLETS, ONE OPEN.",
  "APRONS BACK ON THE RAIL AFTER USE. HARRIS HAS NOT BROUGHT HIS BACK. HARRIS IS STILL IN HERE.");
 m.creatureSpawns.push_back({CreatureKind::Transformed,{4.05f,20.7f},-999,1,0,stateId("et_store_harris")});   // the fitter, still in his apron: quiet while Kemp talks to him
 m.lights.push_back(etTube(5.4f,18.4f,roof,.7f,6.f,4000,.03f,.12f));
 anFix(m,model::mine_cage_lamp,2.06f,18.8f,f+2.2f,.21f,.2f,.36f,kPi*.5f,false);
 m.lights.push_back(etWall({2.04f,18.8f},f+2.35f,0,.45f,5.f,3000,.05f));
 anFix(m,model::mine_cage_lamp,8.7f,20.4f,f+2.2f,.21f,.2f,.36f,anW,false);m.lights.push_back(etWall({8.78f,20.4f},f+2.35f,kPi,.5f,6.f,4500));
 // --- The corridor's lamps: a caged lamp at each bulkhead, dark between ------------------------
 m.lights.push_back(etWall({21.5f,13.92f},f+2.4f,-kPi*.5f,.42f,5.5f,5200));
 m.lights.push_back(etWall({9.02f,11.5f},f+2.4f,0,.6f,6.5f,5200,.2f));
 m.lights.push_back(etWall({14.92f,21.f},f+2.4f,kPi,.6f,6.f,5200));
 // The Shielding detail's work lights: caged lamps hung from the ribs down the corridor, cold, and one on
 // the west face of each bulkhead for the men coming from the ring to see what they are walking into
 for(float x:{10.f,14.4f,21.f}){m.pipes.push_back({{x,11.5f},{x,11.5f},f+2.95f,.012f,roof-.3f,6});anFix(m,model::mine_cage_lamp,x,11.5f,f+2.6f,.21f,.2f,.36f,0,false);
  m.lights.push_back(lamp({x,11.5f},f+2.55f,.75f,7.f,4800,0,WorldLightMount::Bare,0,true));}
 for(float x:{18.6f,12.2f}){anFix(m,model::mine_cage_lamp,x-.1f,9.55f,f+2.15f,.21f,.2f,.36f,anW,false);m.lights.push_back(etWall({x-.02f,9.55f},f+2.3f,kPi,.5f,5.5f,4500));}
 for(float y:{15.8f,21.f}){anFix(m,model::mine_cage_lamp,9.16f,y,f+2.2f,.21f,.2f,.36f,anE,false);m.lights.push_back(etWall({9.08f,y},f+2.35f,0,.75f,7.5f,4500));}
 // what the detail brought down to hold it: lead brick on a cart, a spare stand flood, the monitor they
 // read the wall by; a pallet of brick at the blast door nobody got round to stacking
 handCart(m,20.4f,13.05f,f);for(int c=0;c<3;++c)wall(m,20.06f,12.6f+c*.02f,20.74f,13.5f-c*.02f,f+.29f+c*.09f,f+.375f+c*.09f,20);
 kit::prop(m,model::lab_field_meter,20.4f,12.9f,f+.56f,.25f,.328f,.148f,2.8f);
 anFix(m,model::pallet,10.1f,21.6f,f,1.2f,1.15f,.16f,0,true);
 for(int c=0;c<4;++c){const float z=f+.16f+c*.1f;wall(m,9.6f+(c%2)*.03f,21.15f,10.6f-(c%2)*.03f,22.05f-(c==3?.3f:0.f),z,z+.095f,20);}
 kit::prop(m,model::item_dosimeter,10.25f,21.5f,f+.555f,.31f,.146f,.066f,.4f);
 anModel(m,model::urb_cone,13.9f,18.9f,f,.3f,false);anModel(m,model::urb_cone,10.1f,18.95f,f,1.1f,false);
 // --- The Shielding detail: two men, hostile and breaking ----------------------------------------
 const StateId stance=stateId("et_shield_stance");
 etGuard(m,"et_shield_clock","et_shield_a",ActorVisual::Cleanup,GuardSquadRole::Assault,"et_shield",{{{17.f,6.8f},f,9000,kPi*.5f},{{20.f,6.f},f,5000,kPi*.5f}},9,1,1.f,85);
 m.actorTracks.back().ai.stance=stance;
 etGuard(m,"et_shield_clock","et_shield_b",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"et_shield",
  {{{16.f,11.5f},f,6000,0},{{11.f,11.5f},f,4000,kPi},{{12.f,16.2f},f,5000,kPi*.5f}},12,2,1.05f,80);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.ai.dropsSidearm=true;}
 etSequence(m,"et_shield_radio","et_shield_radio_done",15000,13000,21.f,9.f,24.f,14.f);
 cue(m,"et_shield_radio",800,Sound::Empty,{19.f,4.4f},"INVESTIGATOR (RADIO): Shielding detail. Anything that comes west from the ring, you burn.",.9f,f+1.f,"et_shield_1");
 cue(m,"et_shield_radio",6000,Sound::Empty,{19.f,4.4f},"RADIO: Sir. We've got one mag each. The wall's moving.",.9f,f+1.f,"et_shield_2");
 cue(m,"et_shield_radio",10500,Sound::Empty,{19.f,4.4f},"INVESTIGATOR (RADIO): Then be accurate.",.9f,f+1.f,"et_shield_3");
 etEvent(m,"et_shield_order",0,0,24,24,"et_shield_radio_done",{etSet("et_shield_stance",int(SquadStance::Hostile))});
 // KEMP STOPS. The detail's third man, on his knees at the second of the four, scrubbing him off the lead with a wire
 // brush, seen down the corridor from the ring as the player comes in. The order comes over the post's radio; he hears
 // it out, says the one word back to the wall, drops the brush, and walks: through bulkhead 1, down the leg, into the
 // store, where Harris is. He crouches by Harris and stays (et_desert_free: from then on he can be talked to).
 {const char* scene="et_desert_scene";
  etSequence(m,scene,"et_desert_free",40000,28000,21.f,9.f,24.f,14.f);
  const Vec2 wallAt{15.75f,9.65f},p1{13.4f,11.5f},p2{10.4f,12.6f},p3{10.2f,16.f},p4{9.6f,17.25f},p5{8.f,17.4f},p6{4.f,17.5f},by{4.f,19.f};
  ActorTrack t;t.timerState=stateId(scene);t.visual=ActorVisual::CleanupEngineer;t.scale=1.6f;t.health=70;
  t.damageState=stateId("et_desert_damage");t.deadState=stateId("et_desert_dead");
  t.keys={{0,wallAt,f,kPi,5,0},{11800,wallAt,f,kPi,5,7.f},{12000,wallAt,f,kPi,0,0},{12800,wallAt,f,etHeading(wallAt,p1),0,.5f},
   {12900,wallAt,f,etHeading(wallAt,p1),1,0},{15300,p1,f,etHeading(p1,p2),1,1.6f},{17900,p2,f,etHeading(p2,p3),1,3.3f},{20600,p3,f,etHeading(p3,p4),1,5.2f},
   {21700,p4,f,etHeading(p4,p5),1,5.9f},{23000,p5,f,etHeading(p5,p6),1,6.8f},{26200,p6,f,etHeading(p6,by),1,9.f},{27400,by,f,0,1,9.8f},
   {27600,by,f,0,5,0},{40000,by,f,0,5,9.f}};
  t.ai.mode=ActorAiMode::Civilian;t.ai.faction=AiFaction::Neutral;t.ai.enableState=stateId("et_desert_free");t.talk=stateId("et_desert_hello");
  m.actorTracks.push_back(std::move(t));
  for(int at=300;at<11400;at+=1650)cue(m,scene,at,Sound::OneMetalDrag,{15.75f,9.2f},"",.22f,f+.6f);                            // the brush on the lead
  cue(m,scene,11300,Sound::Empty,{15.75f,9.5f},"SHIELDING (TO THE WALL): ...Accurate.",.8f,f+1.f,"et_desert_scene_11300");
  cue(m,scene,12100,Sound::JunkMetal,{15.6f,9.7f},"",.6f,f+.2f);                                                               // the brush, dropped
  // walked into the store before he got there, the player finds him already sat with Harris
  etWhen(m,"et_desert_found",5.8f,15.2f,9.6f,19.2f,{{scene,0}},{etSet(scene,27700)});
  anFix(m,model::mine_hard_hat,15.3f,9.75f,f,.279f,.351f,.153f,.8f,false);                                                   // his helmet, left where he knelt
  wall(m,15.55f,9.072f,16.05f,9.078f,f+.4f,f+1.5f,20);m.structures.back().solid=false;}                                         // the patch he had scrubbed back to the lead
 // the repeater: the blast door's second board, on the store's north wall, locked to the post
 anModel(m,model::pwr_breaker_panel,6.3f,15.06f,f+1.35f,anS,false);m.pipes.push_back({{6.3f,15.06f},{6.3f,15.06f},f+1.81f,.02f,roof,8});
 // What Kemp says, crouched by Harris. Nothing he says is about the post; he has not been on its radio since 02:47.
 m.dialogue.push_back({stateId("et_desert_hello"),"KEMP","Don't. My magazine's in the brick. I'm off his radio. Keep your voice down; he settles while somebody talks.","et_desert_hello",{
  etReply("Who is he?","et_desert_harris"),etReply("The blast door's shut.","et_desert_door","","et_shield_blast"),etReply("Come west with me.","et_desert_west"),etReply("Stay out of sight.")}});
 m.dialogue.push_back({stateId("et_desert_harris"),"KEMP","Harris. Fitter. Taught me to lay brick, course on course. He still goes to the shelves. He still knows where everything is.","et_desert_harris",{
  etReply("Back.","et_desert_hello"),etReply("That isn't Harris any more.","et_desert_wake","et_store_harris")}});
 m.dialogue.push_back({stateId("et_desert_wake"),"KEMP","...Harris? No. No, he's listening. Harris, it's me. It's-","et_desert_wake",{etReply("...")}});
 m.dialogue.push_back({stateId("et_desert_door"),"KEMP","Their board shut it. The repeater's on that wall, locked to the board, and the locks on this detail are a screwdriver. I'll throw it. You go west and you never saw me.","et_desert_door",{
  etReply("Throw it.","et_desert_thrown","et_shield_blast"),etReply("Back.","et_desert_hello")}});
 m.dialogue.push_back({stateId("et_desert_thrown"),"KEMP","Done. Go on. Don't come back through here with that light on.","et_desert_thrown",{etReply("...")}});
 m.dialogue.push_back({stateId("et_desert_west"),"KEMP","And leave him? He'd come looking. He always comes looking for whoever had the radio on.","et_desert_west",{etReply("Back.","et_desert_hello")}});
 etWhen(m,"et_store_wake_dead",0,0,24,24,{{"et_desert_dead",1}},{etSet("et_store_harris",1)});
 m.pickupSpawns.push_back({{13.6f,22.f},PickupKind::Health,f});
 zone(m,"SHIELDING CORRIDOR / DOSIMETRY POST",14.f,4.f,21.f,9.f,-1000,1000,{.15f,.13f,.10f,.005f});
 zone(m,"SHIELDING CORRIDOR / SHIELDING STORE",2.f,15.f,9.f,22.f);
 zone(m,"SHIELDING CORRIDOR",0,0,24,24,-1000,1000,{.11f,.12f,.13f,.005f});
 recessedFittings(m);
 freightFinish(m,-1);
 return p;
}

// EXPERIMENTAL POWER (manifest 59). Where the test cells were wired straight to the plant: the
// feed comes in over the landing in an insulated duct, drops to the converter, and goes out to
// the plant bus through the switchgear rows. At 02:00 the test cells were isolated. The bus is
// still fed. Coolant from a cracked main pools in the northeast corner across a dropped cable,
// and it hums before it bites.
// Route: the N seam from the Shielding Corridor onto the landing -> down the stair -> the bus hall
// -> S to the Conversion Lab.
std::shared_ptr<const Map> makeExperimentalPower(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[92];m.definition=CampaignChunks[92];m.definition.ambient=.21f;
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-16,g=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,g,false,0});openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"et_power_clock",60000);
 // --- The landing: solid to the landing floor, rails round it, the stair down -------------------
 wall(m,6.f,1.f,18.f,6.f,f,g,15);wall(m,10.f,0.f,14.f,1.f,f,g,15);
 wall(m,5.96f,1.f,6.f,6.f,g-.3f,g,12);wall(m,18.f,1.f,18.04f,6.f,g-.3f,g,12);wall(m,6.f,5.96f,18.f,6.f,g-.3f,g,12);
 guardRail(m,6.f,1.f,6.1f,6.f,g);guardRail(m,17.9f,1.f,18.f,6.f,g);guardRail(m,6.f,5.9f,10.f,6.f,g);guardRail(m,14.f,5.9f,18.f,6.f,g);
 m.stairs.push_back({10.f,6.f,14.f,12.16f,f,g,22,true,false});
 // the flight lit from both sides off the landing's face, cold, so its treads and stringers read from the hall
 for(float x:{9.3f,14.7f}){anFix(m,model::mine_cage_lamp,x,6.12f,f+2.3f,.21f,.2f,.36f,anS,false);m.lights.push_back(etWall({x,6.04f},f+2.45f,kPi*.5f,.7f,7.f,4300));}
 etHighBay(m,{12.f,8.4f},roof,f+6.6f,.8f,8.f,4300);                                                       // and one hung over the flight
 // a stand flood parked under it against the landing, aimed up, so the flight's underside is steel and not a hole
 wall(m,11.975f,6.575f,12.025f,6.625f,f,f+1.12f,12);wall(m,11.8f,6.4f,12.2f,6.8f,f,f+.03f,8);
 for(float d:{-.16f,.16f})wall(m,12.f+d-.012f,6.588f,12.f+d+.012f,6.612f,f,f+.7f,12);
 anFix(m,model::mine_cage_lamp,12.f,6.6f,f+1.1f,.21f,.2f,.36f,anS,false);
 m.lights.push_back(lamp({12.f,6.8f},f+1.3f,.6f,6.f,4300,0,WorldLightMount::Bare,0,true));
 // the routing desk on the landing: the board, the switches, the log
 wall(m,6.6f,1.f,8.8f,1.015f,g+.92f,g+1.68f,20);anModel(m,model::lab_control_board,7.7f,1.11f,g+1.f,anS,false);
 for(float x:{9.f,9.4f})anModel(m,model::lab_knife_switch,x,1.1f,g+1.3f,anS,false);
 etCounter(m,6.4f,2.4f,9.f,3.05f,g);
 kit::prop(m,model::crt_monitor,7.f,2.62f,g+.9f,.42f,.4f,.38f,anS);kit::prop(m,model::keyboard,7.05f,2.9f,g+.9f,.45f,.16f,.03f,anS);
 kit::prop(m,model::office_chair,7.1f,3.75f,g,.55f,.55f,.95f,.2f);
 terminal(m,{8.4f,2.72f},"PLANT BUS / DIRECT FEED","TEST CELLS TO THE PLANT BUS, CONVERSION BYPASSED. 01:30 RING TEST: FULL DRAW ORDERED FROM THE PIT.",
  "02:00 TEST CELLS ISOLATED AT THE DUCT. 02:00 TO NOW: BUS STILL FED. SOURCE: NONE ON THE BOARD.",g-f);
 m.terminals.back().yaw=0;
 m.lights.push_back(etTube(8.f,3.2f,roof,.45f,5.f,3200,0));
 // --- The feed: the insulated duct over the landing and the hall, down to the converter ----------
 wall(m,16.2f,1.f,17.4f,15.6f,roof-2.6f,roof-2.f,8);
 for(float y=2.5f;y<15.f;y+=3.f)for(float x:{16.3f,17.3f})m.pipes.push_back({{x,y},{x,y},roof-2.f,.02f,roof,6});
 wall(m,16.4f,14.8f,17.2f,15.6f,f+2.7f,roof-2.6f,8);
 anFix(m,model::anl_switch_cabinet_2,16.8f,15.4f,f,1.5f,.82f,2.7f,anW,true);                     // the converter cabinet the duct drops into
 m.lights.push_back(etGlow({16.f,15.4f},f+1.6f,.25f,2.6f,etViolet,.3f));
 // --- The switchgear: rows down both walls, the open bay arcing ---------------------------------
 for(float y:{8.2f,9.8f,15.f,16.6f}){anFix(m,model::pwr_switchgear_cabinet,1.42f,y,f,1.5f,.82f,2.7f,anE,true);}
 wall(m,1.f,7.3f,1.84f,17.5f,f+2.7f,f+4.7f,30);
 // THE MAIN BOARD: the west wall above the gear rows is the plant bus's board, eight panels wide and
 // six metres tall: three rows of breakers with their label plates, the meters, the dead mimic screens,
 // the busbars up through the hood to the roof and the cable trays out across the hall to the duct.
 wall(m,1.f,7.4f,1.45f,17.4f,f+2.75f,f+8.6f,2);wall(m,1.f,7.3f,1.75f,17.5f,f+8.6f,f+8.85f,12);wall(m,1.f,7.3f,1.6f,17.5f,f+2.7f,f+2.86f,12);
 for(int b=0;b<=8;++b){const float y=7.4f+b*1.25f;wall(m,1.45f,y-.04f,1.56f,y+.04f,f+2.86f,f+8.6f,12);}
 for(int b=0;b<8;++b){const float y=8.025f+b*1.25f;
  for(int r=0;r<3;++r){const float z=f+3.25f+r*1.15f;
   if(!(b==3&&r==0))anModel(m,model::lab_knife_switch,1.55f,y,z,anE,false);
   wall(m,1.45f,y-.22f,1.47f,y+.22f,z+.42f,z+.52f,b==3?13:20);}
  anModel(m,model::pwr_ammeter_box,1.5f,y-.25f,f+6.85f,anE,false);anModel(m,model::lab_knife_switch,1.55f,y+.25f,f+6.85f,anE,false);
  if(b%3==1)anModel(m,model::lab_status_display,1.48f,y,f+7.75f,anE,false);
  else wall(m,1.45f,y-.4f,1.47f,y+.4f,f+7.65f,f+8.25f,7);}
 for(float y:{8.6f,12.4f,16.2f}){m.pipes.push_back({{1.3f,y},{1.3f,y},f+8.85f,.08f,roof,4});
  for(float z:{f+9.1f,f+9.5f})m.pipes.push_back({{1.3f,y},{1.3f,y},z,.13f,z+.12f,6});}
 for(float y:{9.2f,15.6f}){cableTray(m,{1.8f,y},{15.6f,y},roof-.55f);
  m.pipes.push_back({{1.8f,y+.08f},{4.4f,y+.08f},f+8.7f,.05f,roof-1.5f,7});m.pipes.push_back({{4.4f,y+.08f},{7.8f,y+.08f},roof-1.5f,.05f,roof-.6f,7});}
 m.lights.push_back(etArcLight({2.f,11.15f},f+3.6f,.55f,6.f,1.9f,.25f,.5f));                    // the fault on panel four, arcing behind its missing breaker
 for(auto [y,ph]:{std::pair{9.2f,0.f},{15.6f,.5f}})m.lights.push_back(etBeacon({1.48f,y},f+8.35f,0,ph,0,true));
 m.lights.push_back(lamp({6.f,12.4f},f+6.8f,.95f,11.f,2300,0,WorldLightMount::Bare));            // the board's flood, the one sodium left working on it
 anFix(m,model::pwr_breaker_panel_open,1.12f,12.5f,f+1.3f,.318f,.225f,.457f,anE,false);
 for(auto [a,b,lo,hi]:{std::tuple{11.6f,13.4f,1.f,2.6f},{11.9f,13.f,2.6f,3.2f}})wall(m,1.f,a,1.006f,b,f+lo,f+hi,16);   // soot
 etArc(m,1.f,11.2f,3.f,13.8f,f,f+2.4f,30,2.8f,.7f);
 m.lights.push_back(etArcLight({1.6f,12.5f},f+1.6f,.6f,6.f,2.8f,.7f));
 taped(m,{1.01f,10.9f},f+1.5f,anE,Readable::Note,"BAY 3 / LEAVE IT","BREAKER 3 OPENED 02:01 TO ISOLATE THE FEED. IT ARCED ACROSS THE GAP AND KEPT FEEDING.",
  "DO NOT CLOSE IT. DO NOT STAND IN FRONT OF IT. IT IS NOT THE BREAKER ANY MORE THAT DECIDES.");
 for(float y:{8.6f,11.f,13.4f})anFix(m,model::pwr_tall_switch_cabinet,22.45f,y,f,.95f,1.54f,3.02f,kPi,true);
 wall(m,21.4f,8.1f,23.f,13.9f,f+3.02f,roof,30);
 // --- The transformers, their pads, the plant bus out --------------------------------------------
 // each pad a bank of three step transformers, their bushings bussed back to the breaker wall behind
 for(float x:{5.5f,18.5f}){equipmentPad(m,x-2.5f,18.2f,x+2.5f,21.4f,f);
  for(float dx:{-1.4f,0.f,1.4f}){anModel(m,model::pwr_step_transformer,x+dx,19.6f,f+.1f,anN,true,1.4f);
   m.pipes.push_back({{x+dx,19.6f},{x+dx,22.3f},f+2.5f,.06f,f+2.5f,4});}
  anModel(m,model::pwr_disconnect_switch,x+2.1f,20.9f,f+.1f,anN,true);
  wall(m,x-2.2f,18.8f,x+1.9f,20.4f,f+2.45f,f+3.f,30);}
 // THE BREAKER WALL: the whole south wall, the first thing seen from the landing and the frame of the way
 // out. Switchgear cubicles along the floor either side of the exit, the plant bus's breaker banks over them
 // three tiers high on a steel frame to nine metres, the cable trunks out of the top of every bay to the
 // roof, a striped beam over the exit carrying the bus across it, and bay 5 east of the exit in fault.
 for(auto [a,b]:{std::pair{1.2f,10.f},{14.f,22.8f}}){
  const int bays=int((b-a)/1.45f);const float w=(b-a)/bays;
  for(int i=0;i<bays;++i){const float x=a+w*(i+.5f);const bool fault=a>12&&i==1;
   anFix(m,i%3==2?model::pwr_tall_switch_cabinet:model::pwr_switchgear_cabinet,x,i%3==2?22.47f:22.59f,f,i%3==2?.95f:1.4f,i%3==2?1.4f:.82f,i%3==2?3.02f:2.7f,i%3==2?kPi*.5f:anN,true);
   wall(m,x-w*.5f+.03f,22.5f,x+w*.5f-.03f,22.98f,f+3.05f,f+9.f,2);                                   // the bank's back panel
   for(int r=0;r<3;++r){const float z=f+3.45f+r*1.6f;
    wall(m,x-w*.5f+.06f,22.44f,x+w*.5f-.06f,22.5f,z-.22f,z+1.1f,r==1?7:2);                            // the tier's face
    for(float d:{-.33f,0.f,.33f})if(!(fault&&r==0))anModel(m,model::lab_knife_switch,x+d,22.42f,z+.2f,anN,false);
    wall(m,x-.4f,22.4f,x+.4f,22.44f,z+.78f,z+.9f,(i+r)%4==0?13:20);}                                 // its label plate; every fourth striped
   anModel(m,model::pwr_ammeter_box,x-.3f,22.4f,f+8.25f,anN,false);anModel(m,model::pwr_ammeter_box,x+.3f,22.4f,f+8.25f,anN,false);
   wall(m,x-w*.5f,22.36f,x-w*.5f+.06f,22.98f,f+2.7f,f+9.f,12);                                        // the frame's posts
   m.pipes.push_back({{x,22.72f},{x,22.72f},f+9.f,.2f,roof,7});m.pipes.push_back({{x,22.72f},{x,22.72f},f+9.1f,.26f,f+9.3f,6});   // its trunk to the roof
   if(fault){for(auto [lo,hi,dx]:{std::tuple{2.8f,6.4f,0.f},{6.4f,8.2f,.25f}})wall(m,x-.55f+dx,22.39f,x+.55f-dx,22.4f,f+lo,f+hi,16);   // soot up the bay
    anFix(m,model::pwr_breaker_panel_open,x,22.36f,f+3.6f,.318f,.225f,.457f,anN,false);}}
  wall(m,b-.06f,22.36f,b,22.98f,f+2.7f,f+9.f,12);
  wall(m,a,22.36f,b,22.98f,f+9.f,f+9.25f,12);wall(m,a,22.3f,b,22.4f,f+2.7f,f+2.9f,13);}               // the frame's head and its kick rail, striped
 wall(m,10.f,22.3f,14.f,23.f,f+3.2f,f+3.9f,13);wall(m,10.f,22.3f,14.f,23.f,f+3.9f,f+9.25f,2);          // the beam over the exit, the bank carried over it
 m.pipes.push_back({{1.2f,22.6f},{22.8f,22.6f},f+9.45f,.22f,f+9.45f,4});                               // the plant bus along the top of the wall
 wall(m,1.2f,22.f,10.f,23.f,f+2.7f,roof,30);wall(m,14.f,22.f,22.8f,23.f,f+2.7f,roof,30);wall(m,10.f,22.f,14.f,23.f,f+3.2f,roof,30);   // nobody up the banks
 // bay 5's fault: the breaker blown out of the bank, the tier arcing to the frame every few seconds, the
 // floor in front of it live while it does
 {const float x=14.f+(22.8f-14.f)/int((22.8f-14.f)/1.45f)*1.5f;
  m.lights.push_back(etArcLight({x,21.9f},f+3.7f,1.4f,9.f,2.3f,.35f,.2f));
  m.lights.push_back(etArcLight({x,21.6f},f+1.f,.6f,5.f,2.3f,.35f,.3f));
  etArc(m,x-.9f,21.25f,x+.9f,22.2f,f,f+2.6f,30,2.3f,.35f,.2f);
  m.lights.push_back(lamp({x,21.7f},f+3.9f,.5f,4.f,2600,0,WorldLightMount::Bare,0,false,{1.f,.6f,.25f}));m.lights.back().flicker=.6f;}   // what is still burning in it
 wallSign(m,12.f,22.26f,f+4.6f,"11 KV PLANT BUS","BANKS LIVE. FEED NOT ISOLATED",2.2f,anN);
 // the wall's own light: floods on the frame head looking down the banks
 for(float x:{4.f,8.6f,15.6f,20.2f}){anFix(m,model::mine_cage_lamp,x,22.1f,f+9.4f,.21f,.2f,.36f,0,false);
  m.lights.push_back(lamp({x,21.8f},f+9.f,.9f,10.f,3600,0,WorldLightMount::Bare));}
 // --- The wet corner (northeast): the cracked main, the pool, the cable in it -----------------
 m.pipes.push_back({{17.9f,1.12f},{22.95f,1.12f},f+3.2f,.12f,f+3.2f,3});
 for(float x:{19.4f,21.6f})m.pipes.push_back({{x,1.12f},{x,1.12f},f+3.3f,.02f,roof,6});
 m.waterVolumes.push_back({18.2f,1.2f,22.8f,6.f,f-.45f,f-.1f});
 anFix(m,model::pwr_cable_bundle,20.5f,3.6f,f,4.f,.14f,.22f,.4f,false);
 hazard(m,Hazard::Kind::Anomaly,18.4f,1.4f,22.6f,5.8f,f,f+2.f,2);
 etArc(m,18.4f,1.4f,22.6f,5.8f,f-.2f,f+1.2f,25,4.6f,.9f,.5f);
 anHum(m,"et_power_pool_hum",18.f,1.f,23.f,6.4f,f,f+2.f,.6f);
 m.lights.push_back(etArcLight({20.5f,3.6f},f+.6f,.5f,5.f,4.6f,.9f,.5f));
 wallSign(m,22.94f,4.f,f+2.2f,"LIVE CONDUCTORS IN WATER","KEEP OUT OF THE POOL",2.f,anW);
 // --- Stock under the landing (northwest): reels, pallets -----------------------------------------
 anFix(m,model::cable_reel,2.f,2.6f,f,1.f,2.f,2.f,0,true);anFix(m,model::cable_reel,4.4f,2.6f,f,1.f,2.f,2.f,0,true);
 anFix(m,model::pallet,3.2f,5.f,f,1.2f,1.15f,.16f,0,true);anModel(m,model::lab_specimen_crate,3.2f,5.f,f+.16f,.3f,true,.8f);
 extinguisher(m,7.6f,6.f,f,anS);
 // work lamps hung low over the gear rows, so the cubicle fronts read across the hall
 etHighBay(m,{19.6f,11.f},roof,f+3.4f,.75f,7.5f,4300);etHighBay(m,{14.2f,14.5f},roof,f+3.3f,.55f,6.f,4300,.1f);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{19.8f,10.6f},-999,1,0});
 m.pickupSpawns.push_back({{4.6f,6.4f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{13.6f,21.2f},PickupKind::Health,f});
 // --- The run to the exit: the temporary feed they laid across the floor between the pads at 02:00 to
 // take the bus round the open breaker, its sheath split where the forklift crossed it. It arcs to the pads
 // on the bus's own beat. Cross it on the dark, or go round the west pad and along the gear.
 // It runs in a steel cable protector, split open where the forklift crossed it: the arcs crawl the
 // protector's plate and jump to the glands and the pads, never across bare floor.
 for(auto [x1,x2]:{std::pair{8.2f,11.7f},{12.2f,15.8f}}){wall(m,x1,19.66f,x2,20.14f,f,f+.07f,46);
  for(float y:{19.66f,20.08f})wall(m,x1,y,x2,y+.06f,f+.07f,f+.075f,13);}
 for(float x:{10.f,14.f})anFix(m,model::pwr_cable_bundle,x,19.9f,f+.075f,3.4f,.14f,.2f,0,false);
 for(float x:{8.1f,15.9f})wall(m,x-.12f,19.75f,x+.12f,20.05f,f+.1f,f+.42f,8);                            // its glands at the pads
 wall(m,11.7f,19.7f,12.2f,20.1f,f,f+.004f,8);m.structures.back().solid=false;                          // where it is split, the floor burnt
 etArc(m,8.2f,19.55f,15.8f,20.25f,f,f+2.2f,35,2.6f,.75f,.3f);
 m.lights.push_back(etArcLight({12.f,19.9f},f+.5f,1.4f,8.f,2.6f,.75f,.3f));
 m.lights.push_back(etArcLight({9.4f,19.9f},f+.5f,.7f,5.f,2.6f,.75f,.3f));m.lights.push_back(etArcLight({14.6f,19.9f},f+.5f,.7f,5.f,2.6f,.75f,.3f));
 floorLine(m,8.f,18.55f,16.f,18.7f,f,13);floorLine(m,8.f,21.1f,16.f,21.25f,f,13);
 // THE PARKED UNIT: a bus receiver they wheeled to the exit at 02:20 and isolated, stood dead in the mouth of
 // the way out. It wakes to a shot, a torch in its face or a wound, and nothing the gun does stops it; the
 // split feed between it and the stair does, on its beat, if it can be brought north across it.
 m.creatureSpawns.push_back({CreatureKind::Machine,{12.6f,21.45f},f});
 taped(m,{1.01f,18.6f},f+1.5f,anE,Readable::Note,"EXIT / THE RECEIVER UNIT","02:20 PARKED IN THE EXIT AND ISOLATED. 02:31 RUNNING AGAIN, NOTHING CONNECTED. IT WOULD NOT STAY OFF.",
  "IT CAME AT THE TORCH, NOT AT US, AND WALKED ONTO THE SPLIT FEED. IT STOOD THERE BURNING UNTIL THE FEED LET GO OF IT.");
 // --- Light: sodium over the hall, red on the walls, the arcs and the converter --------------------
 etHighBay(m,{4.5f,9.f},roof,-8.6f,1.f,10.f,2000);etHighBay(m,{12.f,16.f},roof,-8.6f,1.1f,11.f,2000,.2f);
 etHighBay(m,{20.f,9.f},roof,-8.6f,0,0,0);etHighBay(m,{8.f,21.f},roof,-8.6f,.6f,8.f,2000);
 for(auto [x,y,h]:{std::tuple{22.98f,17.f,kPi},{1.02f,20.f,0.f}})m.lights.push_back(etWall({x,y},f+3.2f,h,.45f,6.f,3000,0,{1.f,.15f,.08f}));
 zone(m,"EXPERIMENTAL POWER / LANDING",6.f,0,18.f,6.f,g-.5f,1000);
 zone(m,"EXPERIMENTAL POWER / WET CORNER",18.f,1.f,23.f,6.4f,-1000,1000,{.12f,.16f,.22f,.008f});
 zone(m,"EXPERIMENTAL POWER",0,0,24,24,-1000,1000,{.13f,.12f,.12f,.005f});
 freightFinish(m,-1);etStairRails(m,0,3);
 return p;
}

// CONVERSION LAB (manifest 60). Where the full cells were drawn on: three conversion columns
// take the refined ore's output, a filling line on the floor, and the bank on the south wall
// sends it where the board says. At 02:31 Internal Investigations had it taken off the
// containment feed and put on the plant bus alone. Containment asked twice. B's fields went at
// 02:44. The routing board is in the control room up the stair; it goes back with one switch.
// Route: the N seam from Experimental Power -> the floor past the columns -> the stair -> the
// control room: OUTPUT ROUTING (cont_power) -> back north, to the ring and Containment.
std::shared_ptr<const Map> makeConversionLab(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[93];m.definition=CampaignChunks[93];m.definition.ambient=.21f;
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=-16.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-16,g=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 machineryClock(m,"et_conv_clock",60000);
 const StateId routed=stateId("cont_power");
 // --- The mezzanine: solid to the control room floor, the stair down ------------------------------
 wall(m,16.f,1.f,23.f,9.f,f,g,15);
 for(float z=f+1.5f;z<g-.3f;z+=1.5f)wall(m,15.96f,1.f,16.f,9.f,z,z+.06f,12);
 m.stairs.push_back({16.f,9.f,18.f,15.16f,f,g,22,true,false});
 // the flight lit from the side: a caged lamp on the mezzanine's face, a work lamp hung over its west side
 anFix(m,model::mine_cage_lamp,18.35f,9.12f,f+2.4f,.21f,.2f,.36f,anS,false);m.lights.push_back(etWall({18.35f,9.04f},f+2.55f,kPi*.5f,.7f,7.f,4300));
 etHighBay(m,{15.2f,12.4f},roof,f+3.4f,.6f,6.5f,4300);etHighBay(m,{17.f,12.6f},roof,f+6.6f,.7f,7.f,4300);
 const float c=g+3.2f;
 glazing(m,16.f,1.2f,16.2f,8.8f,g,c,3,.95f,2.4f,true);wall(m,16.f,1.f,16.2f,1.2f,g,c,3);
 wallDoor(m,16.f,23.f,8.8f,9.f,16.4f,17.6f,g,c,-1);
 wall(m,16.f,1.f,23.f,9.f,c,c+.2f,3);
 // the control room: the console on the glass, the routing board on the north wall
 etCounter(m,16.25f,1.8f,16.95f,7.6f,g);
 for(float y:{2.6f,5.2f}){kit::prop(m,model::crt_monitor,16.55f,y,g+.9f,.42f,.4f,.38f,anE);kit::prop(m,model::keyboard,16.82f,y,g+.9f,.45f,.16f,.03f,anE);}
 kit::prop(m,model::office_chair,17.7f,2.7f,g,.55f,.55f,.95f,kPi*.5f+.3f);
 wall(m,18.6f,1.f,21.4f,1.015f,g+.92f,g+1.68f,20);anModel(m,model::lab_control_board,20.f,1.11f,g+1.f,anS,false);
 for(float x:{18.9f,21.1f})anModel(m,model::lab_knife_switch,x,1.1f,g+1.85f,anS,false);
 // The board routes nothing while the containment feeder at the bank stands open: its handle went off the bank at
 // 02:31 (et_conv_handle: Holt's spare, by column four, or the night engineer's own, up at the ring); the feeder is
 // closed at the bank face between its arcs (et_conv_feed); then this switch puts the output back (cont_power).
 terminal(m,{20.f,1.75f},"OUTPUT ROUTING / CONVERSION","OUTPUT: PLANT BUS ONLY, BY I.I. ORDER 02:31. CONTAINMENT FEEDER: OPEN AT THE BANK, HANDLE PULLED. FIELDS ON STANDBY.",
  "E / ROUTE CONVERSION OUTPUT TO CONTAINMENT.",g-f,routed,stateId("et_conv_feed"));
 m.terminals.back().yaw=0;
 taped(m,{19.5f,8.79f},g+1.5f,anN,Readable::Letter,"I.I. / CONVERSION / 02:31","CONVERSION OUTPUT OFF THE CONTAINMENT FEED. FEEDER HANDLE OFF THE BANK, HELD BY THE NIGHT ENGINEER. PLANT BUS ONLY UNTIL THE INVESTIGATOR ADVISES.",
  "CONTAINMENT ASKED AT 02:35 AND AT 02:40. B'S FIELDS WENT AT 02:44. NOBODY ADVISED. -NIGHT ENGINEER");
 anModel(m,model::lab_mainframe,21.8f,6.6f,g,anW,true);wall(m,20.66f,5.5f,22.94f,7.7f,g+3.18f,c,30);
 m.lights.push_back(etTube(19.f,4.f,c,.45f,5.f,3200,0));
 m.lights.push_back(lamp({16.6f,4.f},g+1.25f,.25f,2.4f,9000,-.15f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.1f;
 {auto l=etWall({20.f,1.02f},g+2.1f,kPi*.5f,.3f,3.5f,4000,0,etGreen);l.enabledState=routed;m.lights.push_back(l);}
 m.lights.push_back(etBeacon({21.f,1.02f},g+2.1f,kPi*.5f,0,routed));
 // --- The conversion lane: four columns down the hall, alternating sides, each tied to its wall by a waist-
 // high manifold, so the only way south is a zig-zag between them. One and three are run down and dark:
 // steel to hide behind. Two and four never stopped: their lower shells are off for the fill, open cages of
 // glass round the charge, the lane side of each cage blown out, and every couple of seconds each fires out
 // through the gap across the lane as far as the stripes, lighting the bend it stands on end to end. Cross
 // on the dark, or go round behind the dead steel. Routing the output (cont_power) gives the charge
 // somewhere to go: the columns hold steady and the lane is safe to walk back.
 struct Column{float x,y;bool live;float phase;};
 const Column columns[4]={{4.5f,5.f,false,0},{11.5f,8.6f,true,0},{4.5f,12.6f,false,0},{11.5f,16.8f,true,1.1f}};
 for(const auto& c:columns){
  equipmentPad(m,c.x-1.7f,c.y-1.7f,c.x+1.7f,c.y+1.7f,f);vessel(m,{c.x,c.y},1.3f,c.live?f+4.2f:f+.1f,f+9.f,c.live?6:2);
  m.pipes.push_back({{c.x+(c.x<8?1.3f:-1.3f),c.y},{8.f,c.y},f+7.5f,.3f,f+7.5f,6});                       // its draw-off to the header
  if(!c.live){const float sx=c.x+1.3f;                                                                   // the sight glasses on the lane side, nearly out
   for(float z:{f+2.4f,f+5.6f})m.lights.push_back(etGlow({sx+.15f,c.y},z,.14f,2.f,etViolet,.25f));
   wall(m,sx-.05f,c.y-.25f,sx+.05f,c.y+.25f,f+2.f,f+2.8f,17);wall(m,sx-.05f,c.y-.25f,sx+.05f,c.y+.25f,f+5.2f,f+6.f,17);
   anFix(m,model::mine_cage_lamp,sx+.1f,c.y+.75f,f+3.4f,.21f,.2f,.36f,-kPi*.5f,false);                     // its inspection lamp, left on
   {auto l=lamp({sx+.3f,c.y+.75f},f+3.5f,.5f,5.5f,2300,0,WorldLightMount::Bare,0,true);l.flicker=c.y>8?.25f:0.f;m.lights.push_back(l);}
   continue;}
  const float y=c.y,x1=c.x-1.4f,x2=c.x+1.4f,y1=y-1.4f,y2=y+1.4f,top=f+4.2f;
  for(float x:{x1,x2-.16f})for(float yy:{y1,y2-.16f})wall(m,x,yy,x+.16f,yy+.16f,f+.1f,top,12);         // the cage posts
  wall(m,x1,y1,x2,y2,top-.2f,top,12);wall(m,x1,y1,x2,y2,f+.1f,f+.45f,8);
  for(float z:{f+1.6f,f+2.9f}){wall(m,x1,y1,x2,y1+.06f,z,z+.06f,12);wall(m,x1,y2-.06f,x2,y2,z,z+.06f,12);wall(m,x2-.06f,y1,x2,y2,z,z+.06f,12);}
  wall(m,x1+.16f,y1+.02f,x2-.16f,y1+.06f,f+.45f,top-.2f,17);wall(m,x1+.16f,y2-.06f,x2-.16f,y2-.02f,f+.45f,top-.2f,17);
  wall(m,x2-.06f,y1+.16f,x2-.02f,y2-.16f,f+.45f,top-.2f,17);
  // the lane side: no pane and no rails, the floor scorched out to the stripes
  wall(m,6.4f,y1,x1,y2,f+.002f,f+.005f,8);m.structures.back().solid=false;
  m.pipes.push_back({{c.x,y},{c.x,y},f+.4f,.3f,f+1.2f,6});
  anFix(m,model::lab_anomaly_relic,c.x,y,f+1.6f,1.1f,.9f,.9f,.5f+c.phase,false);                       // the charge, held up in its field
  m.pipes.push_back({{c.x,y},{c.x,y},f+2.6f,.12f,top-.2f,4});
  m.lights.push_back(etGlow({c.x,y},f+2.f,1.5f,4.5f,etViolet,.35f));
  {auto l=etGlow({c.x-1.6f,y},f+2.4f,1.9f,11.f,{.82f,.62f,1.f});l.period=2.2f;l.onTime=.45f;l.phase=c.phase;l.enabledState=routed;l.invert=true;m.lights.push_back(l);}   // the fire: the whole bend lit
  {auto l=etArcLight({c.x-2.f,y},f+1.6f,1.f,7.f,2.2f,.45f,c.phase);l.enabledState=routed;l.invert=true;m.lights.push_back(l);}
  etArc(m,6.4f,y1,x2,y2,f,top,40,2.2f,.45f,c.phase);m.hazards.back().enabledFlag=routed;m.hazards.back().invertFlag=true;   // out of the cage and across the lane
  wall(m,x1-.1f,y1-.1f,x2+.1f,y2+.1f,top,f+4.25f,30);}
 // the manifolds: each column tied to its own wall at waist height, a crouch's cover and no more
 for(auto [x1,x2,y1,y2]:{std::tuple{12.9f,16.f,8.3f,8.9f},{1.f,3.2f,12.3f,12.9f},{12.9f,16.f,16.5f,17.1f},{1.f,3.2f,4.7f,5.3f}}){
  wall(m,x1,y1,x2,y2,f,f+1.15f,8);wall(m,x1,y1,x2,y1+.06f,f+1.f,f+1.15f,13);
  for(float x=x1+.5f;x<x2-.2f;x+=1.f)wall(m,x-.1f,(y1+y2)*.5f-.1f,x+.1f,(y1+y2)*.5f+.1f,f+1.15f,f+1.32f,12);}
 // the lane's painted edge at each bend, where the fire reaches
 for(auto [x1,y1,x2,y2]:{std::tuple{6.1f,7.f,10.f,7.12f},{6.1f,10.2f,10.f,10.32f},{6.f,14.9f,10.f,15.02f},{6.f,18.5f,10.f,18.62f}})floorLine(m,x1,y1,x2,y2,f,13);
 wall(m,7.6f,1.f,8.4f,23.f,f+7.1f,f+7.9f,8);                                                  // the header duct
 for(float y=2.5f;y<23.f;y+=3.5f)for(float x:{7.7f,8.3f})m.pipes.push_back({{x,y},{x,y},f+7.9f,.02f,roof,6});
 // --- The filling line, along the stair side between the bends: a belt of cells to the fill head ------
 floorConveyor(m,14.6f,10.2f,15.6f,f,f+.9f);
 anFix(m,model::lab_test_chamber,14.6f,12.6f,f+1.6f,1.14f,.68f,.64f,anE,false);                 // the fill head over the belt
 for(float x:{14.25f,14.95f})m.pipes.push_back({{x,12.6f},{x,12.6f},f+.96f,.04f,f+1.6f,6});
 for(auto [y,yaw]:{std::pair{10.9f,.3f},{11.7f,1.4f},{14.f,2.f}})kit::prop(m,model::anl_cell_charged,14.6f,y,f+.9f,yaw);
 kit::prop(m,model::anl_cell_spent,14.72f,15.f,f+.9f,.8f);
 m.lights.push_back(etGlow({14.6f,12.6f},f+1.4f,.4f,3.f,etGreen,.2f));
 // --- The bank on the south wall: where the output goes, live -----------------------------------
 for(float x:{4.f,6.f,8.f}){anFix(m,model::anl_switch_cabinet_2,x,22.59f,f,1.5f,.82f,2.7f,anN,true);}
 anFix(m,model::pwr_tall_switch_cabinet,11.6f,22.47f,f,.95f,1.54f,3.02f,kPi*.5f,true);
 wall(m,3.2f,22.1f,12.1f,23.f,f+2.7f,f+3.05f,30);wall(m,10.8f,21.9f,12.4f,23.f,f+3.05f,roof,30);wall(m,3.2f,22.1f,11.1f,23.f,f+3.05f,roof,30);
 wall(m,14.4f,22.f,15.f,23.f,f+3.f,f+4.f,8);
 m.pipes.push_back({{1.2f,22.5f},{14.4f,22.5f},f+3.5f,.16f,f+3.5f,4});
 etArc(m,3.2f,20.6f,12.2f,21.8f,f,f+2.2f,30,3.6f,.8f);m.hazards.back().enabledFlag=routed;m.hazards.back().invertFlag=true;
 {auto l=etArcLight({7.f,21.4f},f+2.4f,.7f,7.f,3.6f,.8f);l.enabledState=routed;l.invert=true;m.lights.push_back(l);}
 {auto l=etWall({2.7f,22.98f},f+3.2f,-kPi*.5f,.5f,7.f,4000,0,etGreen);l.enabledState=routed;m.lights.push_back(l);}        // the bank's healthy lamp, once it has somewhere to send it
 wallSign(m,13.4f,22.94f,f+2.4f,"OUTPUT BUS LIVE","KEEP BACK FROM THE BANK FACE",2.f,anN);
 taped(m,{14.4f,22.99f},f+1.5f,anN,Readable::Note,"BANK / 02:50","WE RAN THE COLUMNS DOWN AT 02:46. THE BANK KEPT SENDING. THE COLUMNS KEPT FILLING.",
  "WHATEVER IS IN THE CELLS DOES NOT WANT TO BE SWITCHED OFF. IT WILL GO WHERE IT IS ROUTED. ROUTE IT SOMEWHERE THAT HELPS.");
 for(auto [x,ph]:{std::pair{2.f,0.f},{16.f,.7f}})m.lights.push_back(etBeacon({x,22.98f},f+3.2f,-kPi*.5f,ph,routed));
 // THE CONTAINMENT FEEDER: its desk at the bank face between cubicle 3 and the tall cabinet, inside the bank's reach.
 // The bank throws to the floor every 3.6 s for most of a second and the feeder's own bay throws further out, to the
 // floor plate; walk in on the dark, fit the handle, close it, walk out. Closed, the bay's amber comes on with a bang.
 {const StateId feed=stateId("et_conv_feed");
  terminal(m,{9.6f,21.72f},"BANK / CONTAINMENT FEEDER","FEEDER OPEN 02:31 BY I.I. ORDER. HANDLE REMOVED AND TAGGED. CUBICLE LIVE: THE BAY THROWS TO THE PLATE.",
   "E / FIT THE HANDLE AND CLOSE THE FEEDER.",0,feed,stateId("et_conv_handle"));
  etArc(m,8.4f,19.5f,10.8f,20.7f,f,f+2.2f,30,3.6f,.8f);m.hazards.back().enabledFlag=routed;m.hazards.back().invertFlag=true;
  {auto l=etArcLight({9.6f,20.2f},f+.4f,.8f,5.f,3.6f,.8f);l.enabledState=routed;l.invert=true;m.lights.push_back(l);}
  wall(m,8.4f,19.5f,10.8f,20.7f,f,f+.006f,8);m.structures.back().solid=false;                                            // the plate it throws to, burnt
  floorLine(m,8.3f,19.32f,10.9f,19.44f,f,13);
  {auto l=lamp({9.6f,21.95f},f+1.55f,.7f,4.5f,2600,0,WorldLightMount::Bare,0,false,{1.f,.55f,.08f});l.enabledState=feed;m.lights.push_back(l);}   // FEEDER CLOSED
  m.ambients.push_back({Sound::OneMetalBang,{9.6f,22.2f},f+1.4f,.6f,16.f,AmbientEmitter::OnState,8,8,feed,1});}
 // Holt, who went for the feeder with the spare handle on the light, at the mouth of column four's cage; the handle's tag
 // by his hand, inside the column's throw: the fire reaches the stripes and the stripes are where he lies.
 etBody(m,"et_conv_clock",ActorVisual::Worker,{9.25f,16.95f},f,1.75f);
 wall(m,8.7f,16.5f,9.9f,17.5f,f+.004f,f+.008f,16);m.structures.back().solid=false;
 {AuthoredTerminalData t;t.position={9.95f,16.75f};t.z=0;t.yaw=.4f;t.title="FEEDER HANDLE / BANK 4 / SPARE";
  t.line1="IN HOLT'S GLOVE. A T-HANDLE OFF THE CONTAINMENT FEEDER, TAGGED BY I.I. 02:31: NOT TO BE REFITTED.";t.line2="E / TAKE THE HANDLE.";
  t.readable=Readable::Letter;t.surface=0;t.activateState=stateId("et_conv_handle");m.terminals.push_back(std::move(t));}
 etArc(m,8.f,14.9f,10.1f,18.6f,f,f+2.2f,40,2.2f,.45f,1.1f);m.hazards.back().enabledFlag=routed;m.hazards.back().invertFlag=true;   // column four's throw, to its stripes
 // The steps, in the plant's words: what the board wants, then the handle, then the feeder, then the switch.
 m.objectiveTexts.push_back({stateId("et_obj_feeder"),"THE CONTAINMENT FEEDER IS OPEN AT THE BANK. ITS HANDLE WENT WITH HOLT, ON THE FLOOR BY COLUMN FOUR."});
 m.objectiveTexts.push_back({stateId("et_obj_bank"),"FIT THE HANDLE AT THE BANK FACE. THE BANK THROWS EVERY FOUR SECONDS: GO IN ON THE DARK."});
 m.objectiveTexts.push_back({stateId("et_obj_route"),"FEEDER CLOSED. ROUTE THE OUTPUT TO CONTAINMENT FROM THE CONTROL ROOM."});
 etWhen(m,"et_conv_obj_look",16.f,1.f,23.f,9.f,{{"et_conv_handle",0},{"cont_power",0}},{etObjective("et_obj_feeder",ObjectiveStatus::Active)});
 etWhen(m,"et_conv_obj_look2",3.f,18.6f,13.f,23.f,{{"et_conv_handle",0},{"cont_power",0}},{etObjective("et_obj_feeder",ObjectiveStatus::Active)});
 etWhen(m,"et_conv_obj_handle",0,0,24,24,{{"et_conv_handle",1},{"et_conv_feed",0}},{etObjective("et_obj_feeder",ObjectiveStatus::Complete),etObjective("et_obj_bank",ObjectiveStatus::Active)});
 etWhen(m,"et_conv_obj_feed",0,0,24,24,{{"et_conv_feed",1}},{etObjective("et_obj_feeder",ObjectiveStatus::Complete),etObjective("et_obj_bank",ObjectiveStatus::Complete),etObjective("et_obj_route",ObjectiveStatus::Active),etShake(.15f)});
 etWhen(m,"et_conv_obj_done",0,0,24,24,{{"cont_power",1}},{etObjective("et_obj_feeder",ObjectiveStatus::Complete),etObjective("et_obj_bank",ObjectiveStatus::Complete),etObjective("et_obj_route",ObjectiveStatus::Complete)});
 // --- The switch lands: the board answers, the bank drops off the plant bus with a bang that comes up
 // through the mezzanine, the columns stop firing, the PA says what it opened, and the game saves. On the
 // way back down the stair something that was quiet while the hall was live is not quiet any more.
 anModel(m,model::lab_pa_horn,22.7f,1.3f,g+2.6f,anW,false);
 etSequence(m,"et_conv_route","et_conv_route_done",9000,8000,-9,-9,-8,-8);
 cue(m,"et_conv_route",300,Sound::Machine,{20.f,1.2f},"",1.f,g+1.4f);
 cue(m,"et_conv_route",1500,Sound::OneMetalBang,{7.f,22.f},"",1.f,f+2.f);
 cue(m,"et_conv_route",3600,Sound::PaDoorsUnlocked,{22.7f,1.3f},"PA: CONVERSION OUTPUT ON THE CONTAINMENT FEED. INTAKE FIELDS ON MAIN POWER. DOORS RELEASED.",.9f,g+2.6f,"et_conv_pa");
 etEvent(m,"et_conv_routed",0,0,24,24,"cont_power",{etSet("et_conv_route",1),etSound(Sound::Machine,.9f),etShake(.3f),etSave()});
 etEvent(m,"et_conv_wake",15.8f,9.f,18.2f,15.4f,"cont_power",{etSpawn(CreatureKind::Transformed,{8.f,4.6f},f,6),etSound(Sound::OneMonsterGroan,.6f)});
 taped(m,{15.f,1.01f},f+1.5f,anS,Readable::Note,"COLUMNS 2 AND 4 / 02:47","RUN DOWN AT 02:46 AND STILL FIRING. EACH THROWS ACROSS THE LANE AS FAR AS THE STRIPES, EVERY TWO SECONDS.",
  "CROSS ON THE DARK OR GO ROUND BEHIND 1 AND 3. HOLT WENT ON THE LIGHT.");
 // --- Dispatch (southeast, and under the mezzanine's south face): cells racked for the fill, the truck
 // that took the full ones out, the last two crates it never came back for ---------------------------
 for(float x:{19.6f,21.6f}){shelf(m,{x,9.32f},0,0);wall(m,x-.9f,9.07f,x+.9f,9.57f,f+1.95f,g,30);
  for(int t=0;t<2;++t)for(int i=0;i<3;++i){if((i+t+int(x))%3==0)continue;
   kit::prop(m,(t+i)%2?model::anl_cell_spent:model::anl_cell_charged,x-.55f+i*.55f,9.32f,f+(t==0?.306f:.972f),.3f*i+t);}
  kit::prop(m,model::prop_cardboard_box,x+(x<20?-.4f:.3f),9.32f,f+1.656f,.436f,.432f,.22f,.05f);}
 m.lights.push_back(etGlow({20.6f,9.9f},f+1.3f,.3f,3.2f,etGreen,.15f));
 anModel(m,model::mine_depot_forklift,21.f,18.4f,f,.25f,true);wall(m,20.3f,16.5f,21.7f,20.3f,f+2.27f,roof,30);
 for(auto [x,y,yaw]:{std::tuple{19.2f,21.9f,.1f},{21.7f,22.f,-.2f}}){anFix(m,model::pallet,x,y,f,1.2f,1.15f,.16f,yaw,true);anModel(m,model::lab_specimen_crate,x,y,f+.16f,yaw+.1f,true,.8f);}
 anFix(m,model::pallet,22.1f,15.6f,f,1.2f,1.15f,.16f,.3f,true);
 kit::prop(m,model::anl_cell_spent,22.f,15.5f,f+.16f,.7f);propPose(m,model::anl_cell_spent,18.9f,19.8f,f,2.1f,kPi*.5f,0);
 anFix(m,model::mine_cage_lamp,22.94f,19.f,f+2.5f,.21f,.2f,.36f,kPi*.5f,false);
 m.lights.push_back(etWall({22.96f,19.f},f+2.65f,kPi,.5f,6.5f,2300));
 // --- The suit rail by the door, the crew's things -------------------------------------------
 etSuitRail(m,1.8f,6.4f,1.f,f,kPi*.5f,1);
 handCart(m,13.f,4.f,f);for(auto [x,y]:{std::pair{12.9f,3.8f},{13.15f,4.25f}})kit::prop(m,model::anl_cell_charged,x,y,f+.29f,x+y);
 anModel(m,model::lab_specimen_crate,14.9f,5.f,f,.2f,true,.8f);
 extinguisher(m,1.f,8.6f,f,anE);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{14.2f,20.2f},-999,1,0});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{2.2f,14.8f},-999,1,0});
 m.pickupSpawns.push_back({{14.f,6.2f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{22.f,3.f},PickupKind::Health,g});
 // --- Light: the hall dark under sodium; the columns' violet; the bank's beacons until it is routed
 etHighBay(m,{9.6f,3.f},roof,-8.f,1.f,11.f,2000);etHighBay(m,{9.6f,12.6f},roof,-8.f,.9f,11.f,4300,.3f);
 etHighBay(m,{20.f,16.f},roof,-8.f,.9f,10.f,2000);etHighBay(m,{20.f,21.f},roof,-8.f,0,0,0);etHighBay(m,{9.6f,20.5f},roof,-8.f,.8f,10.f,2000);
 zone(m,"CONVERSION LAB / CONTROL ROOM",16.f,1.f,23.f,9.f,g-.5f,1000);
 zone(m,"CONVERSION LAB",0,0,24,24,-1000,1000,{.12f,.12f,.13f,.005f});
 freightFinish(m,-1);etStairRails(m,0,3);
 return p;
}
void buildChapterEnergyTestLabs(CampaignMapTable& maps){
 maps[85]=makeResonanceLaboratory();
 maps[86]=makeHighEnergyTestBay();
 maps[91]=makeShieldingCorridor();
 maps[92]=makeExperimentalPower();
 maps[93]=makeConversionLab();
}
}
