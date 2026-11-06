// CONTAINMENT (chapter 2, manifest 61-65): chunks 94, 95, 96, 97, 98. BLOCK-OUT: the real rooms,
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
void caFix(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,v[0]*s,v[1]*s,v[2]*s,yaw,solid});
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
WorldLight caGlow(Vec2 p,float z,float intensity,float range,float kelvin,Point3 gel={1,1,1},float flicker=0){
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
// Resonance growth: a crust on a surface, a mound on the floor, a self-lit crystal knot.
void caCrust(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){wall(m,x1,y1,x2,y2,lo,hi,34);}
void caMound(Map& m,float x,float y,float f,float r,float h){wall(m,x-r,y-r*.7f,x+r,y+r*.7f,f,f+h*.5f,34);wall(m,x-r*.6f,y-r*.45f,x+r*.6f,y+r*.45f,f+h*.5f,f+h,34);}
void caKnot(Map& m,float x,float y,float f,float h){wall(m,x-.12f,y-.1f,x+.12f,y+.1f,f,f+h,35);wall(m,x+.06f,y-.18f,x+.2f,y-.04f,f,f+h*.6f,35);wall(m,x-.22f,y+.02f,x-.08f,y+.16f,f,f+h*.45f,35);}
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
 m.lights.push_back(caBeacon({23.f,19.f},f+2.5f,clW,.3f,"cont_power"));
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
 wall(m,1.f,3.f,6.f,20.f,f,g,15);wall(m,0,8.9f,1.f,14.1f,f,g,15);                           // the gallery's mass, and its lip to the ring
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
 m.dialogue.push_back({stateId("cona_hub_hello"),"INTAKE DETAIL","Gun down, stay out of our line, and we never saw each other. That door's all we're here for.","",{
  caReply("What's behind door B?","cona_hub_b"),caReply("How do I get power to it?","cona_hub_power"),caReply("I'm going.")}});
 m.dialogue.push_back({stateId("cona_hub_b"),"INTAKE DETAIL","Kessler's detail went in at two. Feed dropped at quarter past and the door held. We hear them on the radio. Not to us.","",{caReply("Back.","cona_hub_hello")}});
 m.dialogue.push_back({stateId("cona_hub_power"),"INTAKE DETAIL","Conversion lab. Up the gallery, round the ring, west. Somebody has to turn its output to containment. Not us.","",{caReply("Back.","cona_hub_hello")}});
 caEvent(m,"cona_hub_truce",8.f,0,16.f,6.f,"",{caSet("cona_hub_stance",int(SquadStance::Truce))});
 // The Investigator, over their radio. They answer him; they do not move.
 caSequence(m,"cona_hub_radio","cona_hub_radio_done",22000,20000,6.f,11.f,20.f,16.f);
 const Vec2 radio{12.f,15.6f};
 cue(m,"cona_hub_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. A contract security officer is loose in your sector. Female, armed, no clearance. Detain her.",.9f,f+1.5f);
 cue(m,"cona_hub_radio",7000,Sound::Empty,radio,"RADIO: Sir, we're four men on a door. We can't hold it and chase a guard.",.9f,f+1.5f);
 cue(m,"cona_hub_radio",12500,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then hold the door.",.9f,f+1.5f);
 cue(m,"cona_hub_radio",17000,Sound::Empty,radio,"RADIO: ...Copy.",.9f,f+1.5f);
 // --- Light: pendants over the hall (one dead, one going), the barricade's lamp, door B's red ------
 caPendant(m,{9.f,8.f},roof,f+4.2f,1.5f,11.f,4200);caPendant(m,{16.f,8.f},roof,f+4.2f,1.4f,11.f,4200);
 caPendant(m,{9.5f,13.f},roof,f+4.2f,0,0,0);caPendant(m,{16.f,13.f},roof,f+4.2f,1.2f,10.f,4200,.3f);
 caPendant(m,{12.f,19.6f},roof,f+4.2f,.8f,8.f,4200,.15f);
 for(float y:{5.6f,21.f})m.lights.push_back(lamp({23.f,y},f+3.f,.7f,8.f,3400,0,WorldLightMount::Wall,clW));
 m.lights.push_back(caBeacon({23.f,18.f},f+2.8f,clW,.4f,"cont_power"));
 m.pickupSpawns.push_back({{19.6f,13.f},PickupKind::Health,f});
 zone(m,"",6.f,18.f,18.f,24.f,-1000,1000,{.15f,.12f,.11f,.008f});
 zone(m,"CONTAINMENT INTAKE / GALLERY",0,3.f,6.f,20.f,g-.5f,1000,{.13f,.13f,.14f,.010f});
 zone(m,"CONTAINMENT INTAKE",0,0,24,24,-1000,1000,{.14f,.13f,.13f,.012f});
 freightFinish(m,-1);
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
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceLabPanel;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"cona_a_clock",60000);const char* clock="cona_a_clock";
 caPortalY(m,1.f,1.4f,9.f,14.f,f,roof,3.4f);
 // --- The north cells --------------------------------------------------------------------------
 for(float x:{6.4f,11.9f,17.4f})wall(m,x,1.f,x+.2f,6.7f,f,roof,15);
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
 m.lights.push_back(caGlow({20.3f,4.2f},up+1.6f,1.3f,8.f,6500,{.72f,.5f,1.f},.12f));
 caCrust(m,18.4f,6.5f,21.7f,6.74f,up,up+.84f);caCrust(m,18.3f,6.74f,21.8f,7.56f,up,up+.1f);       // over the sill and onto the gantry
 for(auto [x1,x2,lo]:{std::tuple{18.5f,19.1f,0.f},{19.9f,20.6f,.9f},{21.f,21.5f,.3f}})caCrust(m,x1,7.56f,x2,7.66f,f+lo,up+.06f);   // the run down off the edge
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
 wall(m,9.4f,17.3f,9.6f,23.f,f,roof,15);wall(m,14.4f,17.3f,14.6f,23.f,f,roof,15);
 wall(m,9.6f,22.9f,14.4f,24.f,f+3.4f,roof,15);                                                  // the service ceiling starts here
 for(float y:{19.5f,21.6f})caFix(m,model::med_ward_bed,2.05f,y,f,0,true);
 m.lights.push_back(lamp({5.f,23.f},f+2.9f,.6f,6.f,6500,.02f,WorldLightMount::Wall,clN));
 // S2: they sealed one of their own in. He asked them to.
 caBody(m,clock,ActorVisual::Cleanup,{19.f,20.4f},f,.6f);
 m.lights.push_back(lamp({19.f,23.f},f+2.9f,.45f,5.f,6500,.02f,WorldLightMount::Wall,clN));m.lights.back().flicker=.25f;
 taped(m,{18.8f,17.27f},f+1.5f,caN,Readable::Note,"S2 / SEALED BY DETAIL A, 03:10","PRIVATE VANCE. READ TIER 3 ON THE DOOR METER. ASKED TO BE PUT IN.",
  "WE TOLD HIM IT WAS FOR THE COUNT. HE SAID HE KNEW WHAT IT WAS FOR.");
 m.lights.push_back(caTube(12.f,20.5f,roof,.42f,5.f,4600,.02f,.2f));
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
 caGuard(m,clock,"cona_a_post",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"cona_a",{{{19.6f,15.4f},f,60000,caW}},14,1,1.f);
 m.pickupSpawns.push_back({{16.6f,12.4f},PickupKind::Ammo,f});
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
 m.name=CampaignMapNames[97];m.definition=CampaignChunks[97];m.definition.ambient=.18f;
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
 wall(m,10.4f,0,10.6f,1.2f,f,roof,15);wall(m,10.4f,10.f,10.6f,15.2f,f,roof,15);
 wall(m,10.3f,9.8f,10.7f,10.2f,f,roof,15);                                                         // the pilaster it ended on
 wall(m,10.38f,1.2f,10.62f,9.8f,f+2.95f,roof,12);
 wall(m,10.4f,15.2f,10.6f,18.8f,f+2.6f,roof,15);wall(m,10.4f,18.8f,10.6f,24.f,f,roof,15);
 wall(m,13.4f,0,13.6f,6.6f,f,roof,15);doorwayY(m,6.6f,8.4f,13.4f,13.6f,f,roof);wall(m,13.4f,8.4f,13.6f,14.5f,f,roof,15);
 doorwayY(m,14.5f,15.9f,13.4f,13.6f,f,roof);wall(m,13.4f,15.9f,13.6f,24.f,f,roof,15);
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
 wall(m,1.f,10.f,10.4f,10.2f,f,roof,15);
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
 m.pipes.push_back({{12.9f,2.f},{12.9f,2.f},f,.2f,roof,1});flange(m,{12.9f,2.f,f+.1f},{},.2f);flange(m,{12.9f,2.f,roof-.15f},{},.2f);
 wall(m,12.65f,1.75f,13.15f,2.25f,f,roof,CollisionProxyMaterial);
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
 wall(m,13.6f,6.4f,15.f,6.6f,f,roof,15);caGlaze(m,15.f,6.4f,16.2f,6.6f,f,roof,1.35f,1.65f,true,15);
 wall(m,16.2f,6.4f,17.8f,6.6f,f,roof,15);caGlaze(m,17.8f,6.4f,19.f,6.6f,f,roof,1.35f,1.65f,true,15);
 wall(m,19.f,6.4f,20.6f,6.6f,f,roof,15);caGlaze(m,20.6f,6.4f,21.8f,6.6f,f,roof,1.35f,1.65f,true,15);
 wall(m,21.8f,6.4f,23.f,6.6f,f,roof,15);wall(m,13.6f,8.4f,23.f,8.6f,f,roof,15);
 m.lights.push_back(lamp({23.f,7.5f},f+2.2f,.22f,3.5f,2700,0,WorldLightMount::Wall,clW));
 // the cell: crust over everything, knots of crystal lit from inside, a suit hung from the lamp hook
 caCrust(m,13.6f,1.f,23.f,6.4f,f,f+.06f);caCrust(m,13.6f,1.f,23.f,1.25f,f,roof);caCrust(m,22.75f,1.f,23.f,6.4f,f,roof);
 caMound(m,15.6f,3.f,f,1.f,.9f);caMound(m,20.8f,2.4f,f,1.2f,1.3f);caMound(m,18.2f,5.3f,f,.7f,.5f);
 for(auto [x,y,h]:{std::tuple{16.4f,4.4f,.8f},{21.6f,4.f,1.1f},{14.6f,1.8f,.6f}})caKnot(m,x,y,f+.06f,h);
 hungSuit(m,{18.4f,3.4f,f+2.3f},caS);
 m.lights.push_back(caGlow({18.4f,3.8f},f+1.1f,.6f,5.f,6500,{.72f,.5f,1.f},.35f));
 m.lights.push_back(caGlow({21.6f,4.f},f+1.45f,.3f,2.6f,6500,{.3f,.6f,1.f}));
 hazard(m,Hazard::Kind::Anomaly,13.7f,1.f,22.9f,6.3f,f,f+2.6f,6);
 caHum(m,"cona_svc_hum",13.6f,6.6f,23.f,8.4f,f,f+3.f,.55f);
 // --- The plenum (east, past the bulkhead): the air handlers, and the tech who stayed with them --
 wall(m,13.6f,11.8f,23.f,12.f,f,roof,15);wall(m,13.6f,20.f,23.f,20.2f,f,roof,15);
 for(float y:{14.f,17.6f})caFix(m,model::pwr_air_handler,17.f,y,f,0,true);
 wall(m,17.4f,13.3f,23.f,14.7f,f+2.7f,f+3.2f,2);wall(m,17.4f,16.9f,23.f,18.3f,f+2.7f,f+3.2f,2);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{21.f,16.f},f,1,0});
 m.lights.push_back(lamp({23.f,18.8f},f+2.2f,.3f,5.f,1000,-.45f,WorldLightMount::Wall,clW));m.lights.back().flicker=.4f;
 // --- The west spur to B: the growth comes the other way to meet you ---------------------------
 wall(m,1.f,15.f,10.4f,15.2f,f,roof,15);wall(m,1.f,18.8f,10.4f,19.f,f,roof,15);
 crewBarrier(m,5.f,16.f,f,1.4f);
 taped(m,{5.f,16.07f},f+.75f,caS,Readable::Note,"B SPUR / CLOSED","NOTHING IN OR OUT OF B THIS WAY. ORDER OF DETAIL A 02:20.","");
 for(auto [x1,x2,y1,y2]:{std::tuple{1.f,2.6f,15.2f,18.8f},{2.6f,3.4f,16.1f,18.2f}})caCrust(m,x1,y1,x2,y2,f,f+.04f);
 caCrust(m,1.f,15.2f,3.2f,15.32f,f,f+2.f);caCrust(m,1.f,18.68f,2.6f,18.8f,f,f+1.4f);
 m.lights.push_back(caGlow({1.8f,17.f},f+.35f,.3f,3.f,6500,{.72f,.5f,1.f}));
 m.lights.push_back(lamp({8.f,18.8f},f+2.4f,.4f,5.f,3000,0,WorldLightMount::Wall,clN));m.lights.back().flicker=.5f;
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
 for(auto [x,y,w,h]:{std::tuple{3.4f,3.6f,.34f,4.8f},{2.8f,3.1f,.22f,3.3f},{4.f,4.15f,.24f,2.7f},{3.95f,3.f,.18f,3.8f},{2.75f,4.2f,.2f,2.2f}})
  wall(m,x-w,y-w*.8f,x+w,y+w*.8f,f+1.f,f+h,35);
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
 wall(m,2.6f,9.25f,9.5f,12.4f,f,f+1.1f,34);wall(m,3.2f,9.25f,9.f,11.9f,f+1.1f,f+2.3f,34);wall(m,4.4f,9.25f,7.4f,11.2f,f+2.3f,f+2.9f,34);
 wall(m,9.5f,10.f,14.5f,12.5f,f,f+1.5f,34);wall(m,9.5f,10.15f,14.5f,12.35f,f+1.5f,f+2.9f,34);
 wall(m,9.9f,10.4f,14.3f,12.1f,f+2.9f,f+4.1f,34);wall(m,10.6f,10.7f,13.7f,11.8f,f+4.1f,f+5.2f,34);
 wall(m,9.5f,10.f,14.5f,12.5f,f,roof,CollisionProxyMaterial);wall(m,2.6f,9.25f,9.5f,12.4f,f+1.1f,roof,CollisionProxyMaterial);
 for(auto [x,y,z,h]:{std::tuple{4.6f,10.4f,2.9f,1.f},{7.8f,10.6f,2.3f,.8f},{3.6f,11.6f,2.3f,.7f},{11.2f,11.3f,5.2f,1.2f},{12.9f,11.f,4.1f,.9f},{8.8f,12.1f,1.1f,.6f},
  {13.6f,9.9f,0.f,1.4f},{10.2f,9.9f,0.f,1.1f},{12.f,12.65f,0.f,.9f},{14.1f,12.2f,1.5f,.8f}})caKnot(m,x,y,f+z,h);
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
 wall(m,13.3f,4.5f,14.5f,6.45f,f,f+.42f,34);wall(m,14.42f,4.5f,14.78f,6.45f,f,f+.92f,34);wall(m,14.78f,4.5f,15.6f,6.45f,f,f+.44f,34);
 caKnot(m,13.6f,4.7f,f+.42f,.5f);
 // W2: bars bent out from inside, and the growth came out after him: the doorway is grown shut.
 wall(m,9.24f,15.15f,9.58f,17.65f,f,f+2.4f,34);wall(m,9.58f,15.4f,9.8f,17.3f,f,f+1.3f,34);wall(m,9.f,15.5f,9.24f,17.2f,f,f+1.8f,34);
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
 m.dialogue.push_back({stateId("cona_b_hello"),"B DETAIL","So the hub got its power and not us. Hands where I can see them.","",{
  caReply("What happened in here?","cona_b_what"),caReply("Come with me.","cona_b_come"),caReply("Which way out?","cona_b_way")}});
 m.dialogue.push_back({stateId("cona_b_what"),"B DETAIL","Every cell let go at two. Same second. Kessler held the inner door and the door held him.","",{caReply("Back.","cona_b_hello")}});
 m.dialogue.push_back({stateId("cona_b_come"),"B DETAIL","Doc can't walk. And you heard the radio. Out there we're exposed. In here we're just stuck.","",{caReply("Back.","cona_b_hello")}});
 m.dialogue.push_back({stateId("cona_b_way"),"B DETAIL","West is where it started. Don't. The aisle's grown shut; out our door and round is the only way south. Handling floor, then the theater. The spur east loops back to A.","",{caReply("Back.","cona_b_hello")}});
 caEvent(m,"cona_b_truce",9.5f,4.f,14.5f,9.f,"",{caSet("cona_b_stance",int(SquadStance::Truce))});
 // The Investigator writes them off. They hear it. So does the hub.
 caSequence(m,"cona_b_radio","cona_b_radio_done",24000,22000,9.5f,9.f,14.5f,15.f);
 const Vec2 radio{15.6f,11.6f};
 cue(m,"cona_b_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. Containment B detail is compromised. Treat them as exposed.",.9f,f+1.5f);
 cue(m,"cona_b_radio",6500,Sound::Empty,radio,"RADIO (B): Sir, this is B. We're not exposed. We're shut in. Kessler's dead.",.9f,f+1.5f);
 cue(m,"cona_b_radio",12500,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you know what exposed looks like.",.9f,f+1.5f);
 cue(m,"cona_b_radio",18000,Sound::Empty,radio,"B DETAIL: He's not coming. Nobody's coming. Sit down, Doc.",.9f,f+1.5f);
 // --- The aisle: the third one, the alarm, the tubes that are left --------------------------------
 m.creatureSpawns.push_back({CreatureKind::Transformed,{12.f,20.6f},f,1,0});
 for(auto [x,y,yaw,ph]:{std::tuple{9.5f,18.9f,clE,0.f},{14.5f,9.8f,clW,.8f},{9.5f,22.7f,clE,1.5f}})m.lights.push_back(caAlarm({x,y},f+2.9f,yaw,ph));
 m.lights.push_back(caTube(12.f,14.f,roof,.75f,7.5f,6000,.04f,.5f));m.lights.push_back(caTube(12.f,21.6f,roof,.7f,7.f,6000,.04f,.6f));
 caDeadTube(m,12.f,6.f,roof);caDeadTube(m,12.f,16.8f,roof);
 m.lights.push_back(caTube(12.f,2.4f,roof,.4f,5.f,5600,.03f));
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
