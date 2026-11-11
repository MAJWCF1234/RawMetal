// RESEARCH FRONT (chapter 2, manifest 45-47): chunks 68, 69, 70. Research
// Intake, Personnel Processing and Research Administration: the clean,
// corporate front of Energy Research behind the great door, where the player
// first hears the Investigator who runs the cleanup, then sees him. See
// docs/Design/descent-campaign-20261025.md (The Investigator) for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "CampaignExport.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- ResearchFront kit (rf) --------------------------------------------------------------
constexpr float rfS=0,rfN=kPi,rfE=kPi*.5f,rfW=-kPi*.5f;
constexpr std::uint32_t rfBlue=0xff3f86c8u,rfRed=0xffc8392du;
// A fixture at an absolute base height (see PerimeterB: lab and mine models face -y, add kPi).
void rfFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
void rfSign(Map& m,float x,float y,float z,const char* title,const char* sub,float width,float yaw,std::uint32_t accent){
 wallSign(m,x,y,z,title,sub,width,yaw);m.signs.back().accent=accent;
}
// Clean-side glazing: panel dado, sill, glass with mullions every 1.2 m, transom, bulkhead to `top`.
void rfGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=3,float sill=.95f,float head=2.4f,bool holds=false){
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 wall(m,x1-.015f,y1-.015f,x2+.015f,y2+.015f,f+sill-.03f,f+sill+.03f,20);
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)kit::glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill+.03f,f+head,17,holds);else kit::glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill+.03f,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,std::min(t,x2-.05f),y1,std::min(t+.05f,x2),y2,f+sill+.03f,f+head,20);
  else wall(m,x1,std::min(t,y2-.05f),x2,std::min(t+.05f,y2),f+sill+.03f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.07f,20);
 if(top>f+head+.07f)wall(m,x1,y1,x2,y2,f+head+.07f,top,mat);
}
// A door frame in a wall line at `at` (along x when alongX) from a to b, with a lintel at 2.65.
void rfFrame(Map& m,float a,float b,float at,bool alongX,float f,float roof,int mat){
 if(alongX){wall(m,a,at-.1f,b,at+.1f,f+2.65f,roof,mat);for(float x:{a-.08f,b})doorTrim(m,x,at-.14f,x+.08f,at+.14f,f,f+2.7f,20);}
 else{wall(m,at-.1f,a,at+.1f,b,f+2.65f,roof,mat);for(float y:{a-.08f,b})doorTrim(m,at-.14f,y,at+.14f,y+.08f,f,f+2.7f,20);}
}
// A clean painted steel locker unit (the pack's break-room bank, doors +x) turned to face
// `front`, its back on the same wall line the old rusted locker kept: the front offices are
// clean rooms, and the rust-streaked locker belongs below, where rust does.
void rfLockers(Map& m,float x,float y,float z,float front,float scale=1){
 // Every campaign locker is the double-door psx_locker the player can hide in (the locker test
 // holds to it); a clean-finish variant for these offices is still to come.
 const float yaw=front-kPi*.5f,back=.115f*scale;
 rfFix(m,model::psx_locker,x-std::cos(yaw)*back,y+std::sin(yaw)*back,z,.64f*scale,.85f*scale,2.42f*scale,yaw,true);
}
// Pale steel casings over a sliding door's jambs: the renderer posts every lifting leaf
// in dark panel metal 1.16 m deep, which stood in the opening as a black slab.
void rfJambCovers(Map& m,int door,float f,int mat=20){
 const Door d=m.doors[size_t(door)];const float base=m.layers.front().elevation+d.z;
 const float y1=std::max(0.f,d.y-.62f),y2=std::min(24.f,d.y+.62f);
 wall(m,d.left-.07f,y1,d.left+.075f,y2,base,base+2.8f,mat);m.structures.back().solid=false;
 wall(m,d.right-.075f,y1,d.right+.07f,y2,base,base+2.8f,mat);m.structures.back().solid=false;
 (void)f;
}
// A lamp switched by a state: lit while the state is set (or, inverted, until it is).
WorldLight rfSwitched(WorldLight l,StateId state,bool invert=false){l.enabledState=state;l.invert=invert;return l;}
// A seated workstation whose worker looks along `face`.
void rfDesk(Map& m,float x,float y,float f,float face,int variant){
 Vec2 p{x,y},d{std::sin(face),std::cos(face)},s{std::cos(face),-std::sin(face)};
 auto at=[&](float u,float v){return p+s*u+d*v;};
 const float b=f-m.layers.front().elevation;
 m.fixtures.push_back({model::desk,p,b,1.4f,.7f,.75f,face+kPi,true});
 {auto p=at(variant%2?.15f:-.1f,-.78f);kit::prop(m,model::office_chair,p.x,p.y,f,.55f,.55f,.95f,face+kPi+(variant%3-1)*.25f);}
 kit::prop(m,model::crt_monitor,(at(-.1f,.12f)).x,(at(-.1f,.12f)).y,m.layers.front().elevation+(b+.75f),.42f,.4f,.38f,face+kPi);
 kit::prop(m,model::keyboard,(at(-.1f,-.2f)).x,(at(-.1f,-.2f)).y,m.layers.front().elevation+(b+.75f),.45f,.16f,.03f,face+kPi);
 kit::prop(m,model::desk_phone,(at(.48f,.05f)).x,(at(.48f,.05f)).y,m.layers.front().elevation+(b+.75f),.2f,.2f,.1f,face+kPi);
 if(variant%2)kit::prop(m,model::mug,(at(.42f,-.2f)).x,(at(.42f,-.2f)).y,m.layers.front().elevation+(b+.75f),.09f,.09f,.1f,0);
 if(variant%3==0)kit::prop(m,model::binder,(at(-.52f,.18f)).x,(at(-.52f,.18f)).y,m.layers.front().elevation+(b+.75f),.3f,.08f,.32f,face+kPi);
}
// An armed Cleanup man on an authored beat; enable holds him on his mark until that state is set.
void rfGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,
             float sight,float dot,int weapon,float speed,StateId enable=0,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=dot;ai.searchMs=7000;ai.speed=speed;ai.enableState=enable;
 ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
// A sequence started only by entering its box (or by setting its timer).
void rfSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1=-2,float y1=-2,float x2=-1,float y2=-1){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
// A seam header: closes the step between this chunk's roof and a passage of `height` across an edge.
void rfSeamHeader(Map& m,char side,float a,float b,float f,float height,int mat=3){
 const float roof=m.definition.ceiling;if(roof<=f+height+.01f)return;
 if(side=='N')wall(m,a,0,b,.2f,f+height,roof,mat);
 if(side=='S')wall(m,a,23.8f,b,24,f+height,roof,mat);
}
// An air-shower booth along y from y1 to y2 against the west wall (x 1 to x2):
// nozzle columns on its side walls, a drain, the cycle display over the open doorway.
void rfBooth(Map& m,float x2,float y1,float y2,float f,float roof,float doorA,float doorB){
 wall(m,1.f,y1,x2,y1+.2f,f,roof,27);
 rfGlass(m,x2,y1,x2+.2f,doorA,f,roof,27);rfGlass(m,x2,doorB,x2+.2f,y2,f,roof,27);rfFrame(m,doorA,doorB,x2+.1f,false,f,roof,27);
 for(float y:{y1+.9f,y2-.9f}){wall(m,1.2f,y-.15f,1.35f,y+.15f,f+.25f,f+2.5f,20);
  for(float z=f+.5f;z<f+2.4f;z+=.35f)m.pipes.push_back({{1.35f,y},{1.43f,y},z,.04f,z,6});}
 for(float x=1.6f;x<x2-.3f;x+=.3f)wall(m,x,y1+.6f,x+.06f,y2-.6f,roof-.06f,roof,8);
 rfFix(m,model::med_floor_drain,(1.f+x2)*.5f,(y1+y2)*.5f,f,.5f,.5f,.01f,0,false);
 rfFix(m,model::lab_status_display,x2+.32f,(doorA+doorB)*.5f,f+2.72f,.69f,.06f,.2f,rfE,false);
 m.lights.push_back(lamp({(1.f+x2)*.5f,(y1+y2)*.5f},roof-.18f,.9f,5.f,6800,.04f));
}

// --- Research Intake kit (rfi): chunk 68's own helpers ----------------------------------------------
// Clean-side glazing in anodised steel: panel dado, a steel sill, glass between
// steel mullions every 1.2 m, a steel head and the panel bulkhead up to `top`.
void rfiGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=28,float sill=.95f,float head=2.4f,bool holds=false){
 const int steel=2;
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 wall(m,x1-.015f,y1-.015f,x2+.015f,y2+.015f,f+sill,f+sill+.04f,steel);
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)kit::glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill+.04f,f+head,17,holds);else kit::glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill+.04f,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,std::min(t,x2-.05f),y1,std::min(t+.05f,x2),y2,f+sill+.04f,f+head,steel);
  else wall(m,x1,std::min(t,y2-.05f),x2,std::min(t+.05f,y2),f+sill+.04f,f+head,steel);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.07f,steel);
 if(top>f+head+.07f)wall(m,x1,y1,x2,y2,f+head+.07f,top,mat);
}
// A swinging service door in a wall along x: the renderer draws the leaf, its
// posts and head to 2.55 m; the wall closes above it.
void rfiSwing(Map& m,float x1,float x2,float y,float f,float top,float swing,int mat){
 wall(m,x1,y-.1f,x2,y+.1f,f+2.55f,top,mat);
 Door d{x1,x2,y,0};d.swinging=true;d.swing=swing;m.doors.push_back(d);
}
// An open doorway from y1 to y2 through a wall along y (x1..x2): steel posts and head, the wall above.
void rfiDoorY(Map& m,float y1,float y2,float x1,float x2,float f,float top,int mat){
 // Pale anodised architraves: the dark panel steel read as black slots in the paint.
 for(float y:{y1-.08f,y2})doorTrim(m,x1-.03f,y,x2+.03f,y+.08f,f,f+2.3f,20);
 doorTrim(m,x1-.03f,y1-.08f,x2+.03f,y2+.08f,f+2.22f,f+2.3f,20);
 wall(m,x1,y1,x2,y2,f+2.3f,top,mat);
}
// A full-height wall along y with open doorways (y pairs) cut through it.
void rfiWallY(Map& m,float x1,float x2,float y1,float y2,std::initializer_list<std::pair<float,float>> gaps,float f,float top,int mat){
 float y=y1;for(auto [a,b]:gaps){if(a>y)wall(m,x1,y,x2,a,f,top,mat);rfiDoorY(m,a,b,x1,x2,f,top,mat);y=b;}
 if(y2>y)wall(m,x1,y,x2,y2,f,top,mat);
}
// A ceiling lamp in the clinic two-tube fitting: back of house and the booth canopy.
WorldLight rfiTube(Vec2 p,float z,float intensity,float range,float kelvin,float tint,float yaw=0){
 WorldLight l=lamp(p,z,intensity,range,kelvin,tint);l.yaw=yaw;
 l.fitting=model::clinic_ceiling_lamp_2;l.fittingWidth=.44f;l.fittingDepth=1.04f;l.fittingHeight=.17f;return l;
}

// A framed picture on a wall face at (x, y), facing `face`, its bottom edge at z,
// under its own brass picture light: the pack's canvases are dark, so each gets a
// short hot lamp close over it instead of the room's light.
void rfiPicture(Map& m,int picture,float x,float y,float z,float w,float d,float h,float face){
 const Vec2 out{std::sin(face),std::cos(face)},along{std::cos(face),-std::sin(face)};
 rfFix(m,picture,x+out.x*d*.5f,y+out.y*d*.5f,z,w,d,h,face,false);
 const Vec2 arm{x+out.x*.25f,y+out.y*.25f},head{x+out.x*.5f,y+out.y*.5f};
 doorTrim(m,arm.x-std::fabs(out.x)*.25f-.012f,arm.y-std::fabs(out.y)*.25f-.012f,arm.x+std::fabs(out.x)*.25f+.012f,arm.y+std::fabs(out.y)*.25f+.012f,z+h+.2f,z+h+.224f,18);
 doorTrim(m,head.x-std::fabs(along.x)*.2f-.03f,head.y-std::fabs(along.y)*.2f-.03f,head.x+std::fabs(along.x)*.2f+.03f,head.y+std::fabs(along.y)*.2f+.03f,z+h+.14f,z+h+.2f,18);
 m.lights.push_back(lamp({head.x,head.y},z+h+.1f,5.f,1.7f,3000,0.f,WorldLightMount::Bare,0,false));
}

// RESEARCH INTAKE (manifest 45). The checkpoint the player was never cleared
// for. The great door opens into an interlock; past its collar a screening
// line of three glass booths splits the floor into the unsecured approach and
// the secured concourse. A security post watches the line from the west, the
// badge office from the east. The Cleanup detail holds the reception island,
// and over its radio the Investigator is heard for the first time: at his
// order two of the men walk back through the booths toward the door. Three
// ways through: the lit centre booth; the staff side (post, kit room, the
// corridor behind the holding pen); the visitor side (badge office, the dark
// welcome theatre, the lounge). Both sides come out behind the island.
std::shared_ptr<const Map> makeResearchIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[68];m.definition=CampaignChunks[68];m.definition.ambient=.30f;
 // Each kind of room its own finish: the public concourse in maroon-banded paint, the
 // interlock and the screening line in white tile, the back of house (the post, the
 // kit room, holding, the staff corridor) in painted brick, the visitor side (badge office, theatre, lounge) in green-banded paint.
 m.wallSurface=SurfacePaintedMaroon;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 const int plaster=21,tile=27,brick=31,paint=38,office=37;
 const StateId sweep=stateId("rf_intake_sweep"),alert=stateId("rf_intake_alert");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 // Door 0, the great door's inner leaf, opens on chapter2_open, which chunk 67's
 // great door sets once Chapter 2 is released (CampaignExport.h). Until then it is
 // sealed and says so; playtesters open it with UNLOCK while looking at it, MAP 68
 // or developer mode.
 openEdge(m,{'N',9.f,15.f,f,true,stateId("chapter2_open")});if(!Chapter2Released)m.doors.back().sign=CampaignChunkCount+2;
 openEdge(m,{'S',10.f,14.f,f,false,0});
 rfSeamHeader(m,'S',10.f,14.f,f,3.2f);
 // Solid either side of the way south: nothing is built behind the lobby.
 rectangle(m.layers[0],1,20,10,23,'#');rectangle(m.layers[0],14,20,23,23,'#');
 machineryClock(m,"rf_intake_clock",60000);
 // The back of house lines the chunk's own walls in painted brick, the badge office in its green paint.
 wall(m,1.f,1.f,1.02f,20.f,f,roof,brick);wall(m,1.02f,1.f,8.6f,1.008f,f,roof,brick);wall(m,1.02f,19.98f,6.6f,20.f,f,roof,brick);
 wall(m,15.4f,1.f,22.98f,1.02f,f,roof,office);wall(m,22.98f,1.f,23.f,6.4f,f,roof,office);

 // --- R1 The interlock vestibule (x 8.8-15.2, y 1-3.15) -------------------------------------------
 rfiGlass(m,8.6f,1.f,8.8f,3.15f,f,roof,tile,.95f,2.4f,true);                                   // the post looks in
 // The badge hatch to the clerk: glass over a pass slot and a steel ledge.
 wall(m,15.2f,1.f,15.4f,1.6f,f,roof,tile);wall(m,15.2f,2.6f,15.4f,3.15f,f,roof,tile);
 wall(m,15.2f,1.6f,15.4f,2.6f,f,f+.95f,tile);wall(m,15.2f,1.6f,15.4f,2.6f,f+2.f,roof,tile);
 kit::breakable(m,15.29f,1.64f,15.31f,2.56f,f+1.08f,f+1.96f,17,false);
 for(float y:{1.6f,2.56f})wall(m,15.2f,y,15.4f,y+.04f,f+.99f,f+2.f,2);
 wall(m,15.2f,1.6f,15.4f,2.6f,f+1.96f,f+2.f,2);
 wall(m,14.92f,1.56f,15.4f,2.64f,f+.95f,f+.99f,2);
 // The collar: 1.3 m of cased wall that holds the inner leaf, a vision slot each side.
 for(auto [x1,x2,s1,s2]:{std::tuple{8.8f,10.5f,9.2f,10.f},{13.5f,15.2f,14.f,14.8f}}){
  wall(m,x1,3.15f,x2,4.45f,f,f+1.3f,tile);wall(m,x1,3.15f,x2,4.45f,f+1.8f,roof,tile);
  wall(m,x1,3.15f,s1,4.45f,f+1.3f,f+1.8f,tile);wall(m,s2,3.15f,x2,4.45f,f+1.3f,f+1.8f,tile);
  kit::breakable(m,s1,3.79f,s2,3.81f,f+1.3f,f+1.8f,17,true);
 }
 wall(m,10.5f,3.15f,13.5f,4.45f,f+2.65f,roof,tile);
 m.doors.push_back({10.5f,13.5f,3.8f,0});                                 // 1: the inner interlock leaf
 rfJambCovers(m,0,f);rfJambCovers(m,1,f,tile);   // the collar's reveals in its own tile
 floorLine(m,9.f,1.2f,15.f,1.32f,f,13);floorLine(m,9.f,2.95f,15.f,3.07f,f,13);
 rfFix(m,model::lab_id_scanner,9.65f,3.12f,f+1.f,.3f,.06f,.41f,rfS,false);   // lab models face -y: yaw = facing + kPi
 rfFix(m,model::lab_status_display,12.f,3.121f,f+2.72f,.693f,.058f,.2f,rfS,false);
 rfFix(m,model::lab_pa_horn,14.918f,2.1f,f+2.45f,.495f,.564f,.433f,rfW,false);
 extinguisher(m,15.2f,2.88f,f,rfW);
 m.lights.push_back(lamp({12.f,2.6f},roof-.16f,.9f,5.f,6400,.03f));
 {auto l=lamp({12.f,3.03f},f+2.82f,.18f,1.5f,1100,-.45f,WorldLightMount::Bare);l.period=2;l.onTime=1;m.lights.push_back(l);}   // the cycle pilot

 // --- R4/R5 The approach and the screening line -------------------------------------------------
 // Three booths between panel piers under one steel canopy: an air wash and a band scan in one pass.
 for(auto [a,b]:{std::pair{8.8f,9.f},{10.6f,11.2f},{12.8f,13.4f},{15.f,15.2f}})wall(m,a,5.8f,b,7.4f,f,f+2.4f,tile);
 wall(m,8.8f,5.8f,15.2f,7.4f,f+2.4f,f+2.55f,12);wall(m,8.8f,5.8f,15.2f,7.4f,f+2.55f,roof,tile);
 for(auto [a,b]:{std::pair{9.f,10.6f},{11.2f,12.8f},{13.4f,15.f}}){
  wall(m,a,5.8f,b,7.4f,f+.003f,f+.012f,12);
  rfFix(m,model::med_floor_drain,(a+b)*.5f,6.6f,f+.012f,.5f,.5f,.01f,0,false);
  rfFix(m,model::pwr_vent_grille,a+.02f,6.8f,f+.6f,.819f,.04f,.8f,rfE,false);
  rfFix(m,model::pwr_vent_grille,b-.02f,6.8f,f+.6f,.819f,.04f,.8f,rfW,false);
 }
 rfFix(m,model::lab_id_scanner,10.57f,6.05f,f+1.f,.3f,.06f,.41f,rfE,false);
 rfFix(m,model::lab_id_scanner,11.23f,6.05f,f+1.f,.3f,.06f,.41f,rfW,false);
 rfFix(m,model::lab_id_scanner,13.43f,6.05f,f+1.f,.3f,.06f,.41f,rfW,false);
 for(float x:{9.8f,14.2f})rfFix(m,model::lab_status_display,x,5.771f,f+2.62f,.693f,.058f,.2f,rfS,false);
 rfSign(m,12.f,5.8f,f+2.6f,"SCREENING SUSPENDED","BY ORDER OF INTERNAL INVESTIGATIONS",3.2f,kPi,rfRed);
 rfFix(m,model::lab_wall_clock,9.9f,7.416f,f+2.75f,.402f,.032f,.4f,rfS,false);
 // Lane 1 keeps the staff gate; lane 3 the Investigator closed; lane 2 is the way, painted and lit.
 rfFix(m,model::ticket_gate,9.3f,6.6f,f,.4f,1.42f,1.09f,0,true);
 crewBarrier(m,14.2f,5.72f,f,1.4f);
 taped(m,{14.2f,5.72f},f+.75f,kPi,Readable::Note,"LANE CLOSED","BY ORDER OF INTERNAL INVESTIGATIONS 03:45.","USE LANE 2 ONLY.");
 floorLine(m,11.88f,4.5f,12.12f,5.8f,f,14);wall(m,11.88f,5.8f,12.12f,7.4f,f+.012f,f+.016f,14);floorLine(m,11.88f,7.4f,12.12f,12.6f,f,14);
 m.lights.push_back(rfiTube({12.f,6.6f},f+2.24f,1.3f,4.5f,6800,.05f));
 m.lights.push_back(rfiTube({14.2f,6.6f},f+2.24f,.35f,2.5f,1100,-.45f));
 m.lights.push_back(lamp({9.8f,6.6f},f+2.2f,.35f,2.5f,5600,.05f,WorldLightMount::Bare));   // lane 1, the staff gate's own light

 // --- R2 The security post (x 3.6-8.6, y 1-6.4) and R3 the kit room (x 1-3.4) -------------------
 rfiWallY(m,3.4f,3.6f,1.f,6.4f,{{2.2f,3.2f}},f,roof,brick);
 rfiWallY(m,8.6f,8.8f,3.15f,6.6f,{{4.6f,5.6f}},f,roof,brick);
 wall(m,1.f,6.4f,1.4f,6.6f,f,roof,brick);rfiSwing(m,1.4f,2.4f,6.5f,f,roof,1,brick);   // 2: D3, kit room to the corridor
 wall(m,2.4f,6.4f,4.f,6.6f,f,roof,brick);
 rfiGlass(m,4.f,6.4f,8.6f,6.6f,f,roof,brick);                                    // over the holding pen
 // The CCTV wall: two monitors on cabinets, three on a steel shelf over them.
 for(float x:{5.f,5.92f}){rfFix(m,model::file_cabinet_wide,x,1.25f,f,.9f,.5f,.75f,rfS,true);kit::prop(m,model::cctv_tv,x,1.22f,f+.75f,.5f,.4f,.42f,rfS);}
 wall(m,4.6f,1.f,6.4f,1.42f,f+1.5f,f+1.54f,12);
 for(float x:{4.72f,6.28f})wall(m,x-.02f,1.f,x+.02f,1.38f,f+1.3f,f+1.5f,12);
 for(float x:{4.95f,5.5f,6.05f})kit::prop(m,model::cctv_tv,x,1.21f,f+1.54f,.5f,.4f,.42f,rfS);
 rfFix(m,model::lab_control_board,7.6f,1.094f,f+1.f,1.32f,.187f,.6f,rfS,false);
 rfFix(m,model::lab_wall_clock,7.6f,1.016f,f+2.1f,.402f,.032f,.4f,rfS,false);
 taped(m,{6.67f,1.f},f+1.25f,rfS,Readable::Note,"II / CCTV","ALL TAPES 01:50-03:40 TO THE INVESTIGATOR.","TAKEN 03:52. NO COPIES.");
 // The PA desk: one call to the holding room speaker.
 terminal(m,{4.1f,1.4f},"INTAKE PA / PAGE HOLDING","ONE CALL TO THE HOLDING ROOM SPEAKER.","LAST PAGE 02:30: TIER 2 TO SCREENING.",0,stateId("rf_intake_page"));m.terminals.back().yaw=rfS;
 // The operator counter under the vestibule glass.
 wall(m,7.9f,1.3f,8.6f,4.4f,f,f+.74f,plaster);wall(m,7.85f,1.25f,8.6f,4.45f,f+.74f,f+.78f,12);
 const float ctr=f+.78f;
 for(float y:{2.f,3.5f}){kit::prop(m,model::crt_monitor,8.3f,y,ctr,.42f,.4f,.38f,rfW);kit::prop(m,model::keyboard,7.98f,y,ctr,.45f,.16f,.03f,rfW);}
 kit::prop(m,model::desk_phone,8.35f,2.98f,ctr,.2f,.2f,.1f,rfW);
 kit::prop(m,model::office_paper_stack,8.14f,2.65f,ctr,.329f,.443f,.03f,rfW);
 kit::prop(m,model::item_walkie,8.22f,1.5f,ctr,.114f,.376f,.046f,.1f);
 kit::prop(m,model::item_walkie,8.42f,1.48f,ctr,.114f,.376f,.046f,-.12f);
 terminal(m,{8.15f,4.08f},"INTAKE SECURITY / SHIFT LOG","01:58 SURGE ALARM. INTAKE LOCKED BY ORDER 7. 41 BANDS INSIDE.",
  "03:40 INTERNAL INVESTIGATIONS IN BY SERVICE LIFT 2. ALL DETAILS NOW REPORT TO THE INVESTIGATOR.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfW;m.terminals.back().surface=.78f;
 kit::prop(m,model::office_chair,7.3f,2.f,f,.55f,.55f,.95f,rfW);
 kit::prop(m,model::office_chair,7.35f,3.55f,f,.55f,.55f,.95f,rfW+.35f);
 // The officer who wrote it: down by his chair, his band cut off.
 kit::propPose(m,model::office_chair,6.5f,4.8f,f,.55f,.55f,.95f,1.9f,kPi*.5f,0);
 floorLine(m,5.f,5.f,6.3f,5.85f,f,16);
 kit::prop(m,model::item_id_card,6.25f,4.45f,f,.132f,.085f,.002f,.7f);
 paperDrift(m,4.2f,2.4f,7.f,4.2f,f,4,683);
 // The kit room: the detail took the shotguns and left the lockers.
 for(float y:{3.7f,4.6f,5.5f})rfLockers(m,1.45f,y,f,rfE);
 wall(m,1.2f,1.f,3.2f,1.08f,f+.95f,f+1.f,8);wall(m,1.2f,1.f,3.2f,1.08f,f+1.75f,f+1.8f,8);
 for(float x=1.3f;x<3.15f;x+=.36f)wall(m,x,1.f,x+.03f,1.14f,f+.9f,f+1.9f,8);
 rfFix(m,model::med_first_aid_box,3.34f,4.6f,f+1.3f,.367f,.12f,.349f,rfW,false);
 kit::prop(m,model::prop_cardboard_box_open,2.7f,5.7f,f,.73f,.725f,.726f,.25f);
 m.lights.push_back([&]{auto l=lamp({6.1f,3.7f},roof-.16f,.7f,5.f,4300,.25f);l.flicker=.35f;return l;}());
 m.lights.push_back(lamp({5.45f,1.55f},f+1.75f,.45f,2.5f,7000,.1f,WorldLightMount::Bare));   // the monitors' glow
 m.lights.push_back(lamp({7.6f,1.6f},f+1.9f,.75f,2.2f,6500,.1f,WorldLightMount::Bare));     // the control board's bays
 m.lights.push_back(rfiTube({2.2f,3.7f},roof-.16f,.5f,4.f,4300,.3f));

 // --- R6 Holding, R7 the effects store, R8 the staff corridor and back passage ----------------
 rfiWallY(m,2.8f,3.f,6.6f,17.8f,{{8.4f,9.4f},{14.4f,15.4f}},f,roof,brick);
 wall(m,3.f,11.8f,6.8f,12.f,f,roof,brick);
 wall(m,6.6f,12.f,6.8f,17.6f,f,roof,brick);
 wall(m,2.8f,17.6f,6.8f,17.8f,f,roof,brick);
 wall(m,6.6f,6.6f,6.8f,6.8f,f,f+2.4f,brick);wall(m,6.6f,6.6f,6.8f,11.8f,f+2.4f,roof,brick);
 rfFix(m,model::med_cell_bars,6.7f,8.05f,f,2.5f,.04f,2.4f,rfE,false);
 rfFix(m,model::med_cell_bars_doorway,6.7f,10.55f,f,2.5f,.04f,2.4f,rfE,false);
 wall(m,6.66f,6.8f,6.74f,9.97f,f,f+2.4f,CollisionProxyMaterial);wall(m,6.66f,11.13f,6.74f,11.8f,f,f+2.4f,CollisionProxyMaterial);
 bench(m,4.8f,7.5f,f);bench(m,4.4f,11.3f,f);
 // 41 bands taken at the door, tipped out of the box they were counted into.
 kit::prop(m,model::prop_cardboard_box_open,3.6f,10.15f,f,.73f,.725f,.726f,-.3f);
 for(auto [x,y,yaw]:{std::tuple{4.2f,9.7f,.4f},{4.5f,10.1f,2.f},{4.05f,10.55f,1.1f},{4.8f,9.85f,2.7f},{4.35f,10.75f,.2f},{5.f,10.4f,1.6f}})
  rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{4.6f,9.55f,.9f},{4.15f,10.3f,2.4f},{5.2f,10.f,.3f},{4.7f,10.65f,1.9f},{3.75f,9.6f,1.2f},{5.35f,9.65f,2.8f}})
  kit::prop(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw);
 taped(m,{3.5f,6.6f},f+1.5f,rfS,Readable::Note,"HOLDING / 01:58","HELD: 41. RELEASED: 0. BANDS TAKEN AT THE DOOR.","02:30 TIER 2 WALKED TO SCREENING. REST TO FOLLOW.");
 rfFix(m,model::lab_pa_horn,3.282f,7.3f,f+2.5f,.495f,.564f,.433f,rfE,false);
 rfFix(m,model::med_first_aid_box,3.06f,11.f,f+1.3f,.367f,.12f,.349f,rfE,false);
 m.lights.push_back(lamp({3.f,9.9f},f+2.3f,.5f,4.f,2700,0,WorldLightMount::Wall,0));
 // The store: what was taken from the 41, held for the Investigator.
 rfFix(m,model::stock_cage,4.8f,12.6f,f,2.4f,1.f,1.1f,0,true);
 for(auto [x,yaw]:{std::pair{4.f,.1f},{4.8f,-.15f},{5.6f,.3f}})kit::prop(m,model::prop_cardboard_box,x,12.6f,f+1.1f,.727f,.72f,.367f,0);
 taped(m,{4.8f,13.1f},f+.75f,rfS,Readable::Note,"EFFECTS / HELD FOR II","41 BANDS. 41 BAGS. NOT TO BE RETURNED TO STAFF.","SIGNED FOR 03:47.");
 for(float y:{14.f,16.3f}){shelf(m,{6.35f,y},rfW);kit::prop(m,model::prop_cardboard_box,5.7f,y+.2f,f,.727f,.72f,.367f,rfW);
  rfFix(m,model::prop_cardboard_box,6.35f,y-.45f,f+.972f,.45f,.45f,.23f,rfW,false);rfFix(m,model::prop_cardboard_box_open,6.35f,y+.4f,f+.972f,.45f,.45f,.45f,rfW,false);
  for(int i=0;i<5;++i)rfFix(m,model::office_file_binder,6.42f,y-.6f+i*.12f,f+.306f,.093f,.301f,.424f,rfW+(i==3?.25f:0),false);}
 rfFix(m,model::office_file_cabinet_open,4.2f,17.2f,f,1.451f,.699f,1.054f,rfN,true);
 rfFix(m,model::wooden_table,4.6f,15.6f,f,1.367f,.724f,.78f,0,true);
 const float table=f+.78f;
 kit::prop(m,model::item_walkie,4.05f,15.6f,table,.114f,.376f,.046f,0);
 for(auto [x,y,yaw]:{std::tuple{4.75f,15.4f,0.f},{4.75f,15.65f,0.f},{5.07f,15.7f,kPi*.5f}})kit::prop(m,model::item_dosimeter,x,y,table,.31f,.146f,.066f,yaw);
 kit::prop(m,model::office_paper_stack,4.32f,15.6f,table,.329f,.443f,.03f,0);
 m.lights.push_back(lamp({4.8f,14.8f},roof-.16f,.5f,4.f,5000,.05f));
 // The corridor behind it all.
 cableTray(m,{1.2f,6.8f},{1.2f,17.6f},f+3.f);
 for(float y:{12.f,12.4f})rfFix(m,model::pwr_breaker_panel,1.051f,y,f+1.3f,.183f,.102f,.457f,rfE,false);
 extinguisher(m,1.f,16.5f,f,rfE);
 handCart(m,5.5f,18.9f,f);
 rfFix(m,model::lab_notice_board,2.765f,16.1f,f+1.2f,1.2f,.07f,.9f,rfW,false);   // the staff side's own board, at the corridor's end
 // Where the 41 were walked through: a drain, a box set down, bands that came off on the way.
 floorLine(m,2.62f,6.8f,2.7f,17.6f,f,13);floorLine(m,1.f,17.92f,6.6f,18.f,f,13);
 rfFix(m,model::med_floor_drain,1.9f,11.2f,f,.5f,.5f,.01f,0,false);
 kit::prop(m,model::prop_cardboard_box_open,1.42f,13.9f,f,.45f,.45f,.45f,.3f);
 kit::prop(m,model::prop_cardboard_box,1.4f,14.45f,f,.45f,.45f,.23f,-.2f);
 for(auto [x,y,yaw]:{std::tuple{2.f,10.4f,1.3f},{1.7f,13.1f,2.6f},{3.2f,18.6f,.5f}})rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{2.2f,12.2f,.8f},{4.2f,19.35f,2.1f}})kit::prop(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw);
 paperDrift(m,1.3f,7.f,2.5f,17.f,f,4,684);
 m.lights.push_back(rfiTube({1.9f,9.5f},roof-.16f,.6f,5.f,4300,.3f));
 m.lights.push_back([&]{auto l=rfiTube({1.9f,15.5f},roof-.16f,.6f,5.f,4300,.3f);l.flicker=.5f;return l;}());
 m.lights.push_back(rfiTube({3.9f,18.9f},roof-.16f,.5f,5.f,4300,.3f,kPi*.5f));

 // --- R9 The intake concourse (x 6.8-17.2, y 7.4-17.6) and R10 the exit lobby ------------------
 for(float x:{8.6f,15.4f}){wall(m,x-.3f,11.7f,x+.3f,12.3f,f,roof,paint);wall(m,x-.36f,11.64f,x+.36f,12.36f,f,f+.12f,12);}
 for(float x:{9.25f,14.75f})kit::prop(m,model::trash_can,x,12.f,f,.5f,.5f,.62f,0);
 // The reception island the detail holds: a front and returns, the transaction
 // ledge, two clerks' desks behind it.
 wall(m,10.f,13.f,14.f,13.5f,f,f+1.05f,plaster);
 wall(m,10.f,13.5f,10.3f,14.6f,f,f+1.05f,plaster);wall(m,13.7f,13.5f,14.f,14.6f,f,f+1.05f,plaster);
 wall(m,9.95f,12.9f,14.05f,13.55f,f+1.05f,f+1.1f,12);
 wall(m,10.f,12.98f,14.f,13.f,f,f+.1f,8);for(float x:{11.f,12.f,13.f})wall(m,x-.015f,12.98f,x+.015f,13.f,f+.1f,f+1.05f,12);   // kick plate, panel joints
 rfDesk(m,11.f,14.15f,f,rfN,1);rfDesk(m,13.f,14.15f,f,rfN,2);
 for(float x:{11.55f,13.45f})kit::prop(m,model::office_pc_tower,x,14.25f,f,.262f,.609f,.609f,rfS);
 const float ledge=f+1.1f;
 rfFix(m,model::item_walkie,12.4f,13.2f,ledge,.114f,.376f,.046f,.4f,false);   // the detail's radio
 for(auto [x,y,yaw]:{std::tuple{10.6f,13.2f,.3f},{10.95f,13.28f,-.4f},{13.3f,13.2f,1.3f},{13.62f,13.27f,.2f}})rfFix(m,model::item_dosimeter,x,y,ledge,.31f,.146f,.066f,yaw,false);
 for(auto [x,yaw]:{std::pair{11.62f,.1f},{12.9f,-.2f}})rfFix(m,model::office_paper_stack,x,13.22f,ledge,.329f,.443f,.03f,yaw,false);
 // The queue rails that funnel the centre booth to the ledge.
 for(float x:{10.4f,13.6f}){
  for(float y:{8.8f,10.2f,11.6f}){wall(m,x-.03f,y-.03f,x+.03f,y+.03f,f,f+.95f,12);wall(m,x-.12f,y-.12f,x+.12f,y+.12f,f,f+.03f,12);}
  wall(m,x-.012f,8.83f,x+.012f,11.57f,f+.86f,f+.92f,19);
 }
 bench(m,10.5f,15.9f,f);bench(m,13.5f,15.9f,f);
 // The exhibit: the product, the first cell, lit in its case. No plaque.
 rfFix(m,model::office_display_cabinet,16.93f,8.6f,f,.968f,.54f,2.1f,rfW,true);
 kit::fixturePart(m,int(m.fixtures.size())-1,4,16.63f,8.17f,16.69f,9.03f,f+.23f,f+1.95f,stateId("rf_intake_alert"));
 rfFix(m,model::lab_test_chamber,16.93f,8.6f,f+1.05f,.627f,.374f,.352f,rfW,false);
 // A soft edge to the exit lobby: planters, and a few things on the walls.
 for(auto [x1,x2]:{std::pair{7.2f,9.6f},{14.4f,16.8f}}){
  wall(m,x1,17.3f,x2,17.8f,f,f+.5f,plaster);wall(m,x1-.02f,17.28f,x2+.02f,17.82f,f+.5f,f+.54f,12);
  for(float x:{x1+.45f,(x1+x2)*.5f,x2-.45f})rfFix(m,model::lab_potted_plant,x,17.55f,f+.54f,.44f,.51f,.82f,x,false);
 }
 kit::prop(m,model::lab_potted_plant,7.2f,7.f,f,.44f,.51f,.82f,.6f);
 kit::prop(m,model::lab_potted_plant,7.15f,16.75f,f,.44f,.51f,.82f,2.f);
 rfFix(m,model::water_cooler,17.025f,15.f,f,.35f,.35f,1.1f,rfW,true);
 rfiPicture(m,model::art_painting_5,17.2f,16.4f,f+1.1f,1.156f,.08f,.983f,rfW);
 rfiPicture(m,model::art_painting_6,6.8f,14.6f,f+1.25f,.689f,.03f,.8f,rfE);
 m.lights.push_back(lamp({19.6f,1.9f},f+2.2f,1.1f,2.8f,5200,0.f,WorldLightMount::Bare));   // over the rota
 // Bands and sheets dropped where the line broke at 01:58.
 paperDrift(m,9.2f,7.5f,14.8f,8.6f,f,7,681);
 paperDrift(m,9.4f,12.1f,14.6f,12.8f,f,4,682);
 paperDrift(m,7.2f,13.f,9.6f,16.4f,f,3,685);paperDrift(m,14.8f,9.6f,16.8f,14.f,f,3,686);
 // What the line dropped when it broke: a radio, a carton, bands by the queue posts.
 kit::prop(m,model::prop_cardboard_box_open,7.3f,13.7f,f,.45f,.45f,.45f,.7f);
 kit::prop(m,model::office_file_binder,7.7f,14.05f,f,.093f,.301f,.424f,1.4f);
 kit::prop(m,model::item_walkie,15.6f,11.3f,f,.114f,.376f,.046f,2.2f);
 for(auto [x,y,yaw]:{std::tuple{10.2f,9.4f,.9f},{13.85f,10.9f,2.2f},{7.8f,12.9f,1.6f},{16.1f,13.4f,.4f}})kit::prop(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw);
 for(auto [x,y,yaw]:{std::tuple{10.1f,7.9f,.6f},{11.7f,8.25f,2.4f},{12.65f,7.7f,1.2f},{14.1f,8.05f,.3f},{9.8f,12.5f,2.6f},{14.3f,12.6f,1.f}})
  rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 // The exit lobby: the detail's kit crates, and the warm light south.
 rfFix(m,model::lab_specimen_crate,15.6f,19.35f,f,1.12f,1.12f,1.12f,.12f,true);
 rfFix(m,model::lab_specimen_crate,15.55f,19.3f,f+1.12f,1.12f,1.12f,1.12f,-.18f,true);
 kit::prop(m,model::item_walkie,15.23f,17.55f,f+.54f,.114f,.376f,.046f,0);
 floorLine(m,10.2f,21.f,13.8f,21.12f,f,13);
 extinguisher(m,10.03f,22.4f,f,rfE);
 for(Vec2 l:{Vec2{8.6f,9.2f},Vec2{15.8f,9.2f}})m.lights.push_back(lamp(l,roof-.16f,.9f,6.5f,6200,.04f));
 m.lights.push_back(lamp({12.2f,10.4f},roof-.16f,1.f,6.f,6200,.04f));
 m.lights.push_back(lamp({12.2f,14.f},roof-.16f,1.1f,5.f,5600,.03f));          // the squad, lit
 m.lights.push_back(lamp({16.58f,8.6f},f+1.45f,1.1f,3.f,5200,0,WorldLightMount::Bare));   // the case light, at its glass
 m.lights.push_back(lamp({16.62f,8.6f},f+1.8f,1.5f,2.4f,5600,0,WorldLightMount::Bare));   // over the cell, through its glass
 m.lights.push_back(lamp({12.2f,18.8f},roof-.16f,.9f,6.f,3400,0));
 m.lights.push_back(lamp({12.f,22.f},roof-.16f,.8f,5.f,4600,.02f));   // the link: between the lobby's warm light and Processing's cold

 // --- R11 The badge office and band cage (x 15.4-23, y 1-6.4) -------------------------------
 rfiWallY(m,15.2f,15.4f,3.15f,6.6f,{{4.6f,5.6f}},f,roof,office);
 wall(m,15.2f,6.4f,17.8f,6.6f,f,roof,office);rfiSwing(m,17.8f,18.8f,6.5f,f,roof,1,office);   // 3: D4, to the theatre
 wall(m,18.8f,6.4f,23.f,6.6f,f,roof,office);
 rfDesk(m,15.75f,2.1f,f,rfW,0);                                           // the clerk at the hatch
 terminal(m,{16.f,1.6f},"INTAKE / VISITOR LOG","03:40 VISITOR: INTERNAL INVESTIGATIONS (1). IN BY SERVICE LIFT 2.","BAND NOT ISSUED. NOT REQUIRED, PER VISITOR.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;m.terminals.back().surface=.75f;
 for(float x:{17.4f,17.95f})rfFix(m,model::file_cabinet_tall,x,1.3f,f,.5f,.6f,1.3f,rfS,true);
 rfFix(m,model::lab_photocopier,18.8f,1.4f,f,.826f,.752f,.967f,rfN,true);
 rfFix(m,model::lab_notice_board,19.6f,1.035f,f+.95f,1.2f,.07f,.9f,rfS,false);   // the shift rota, pinned up
 // The band printer, and the cage of bands nobody was issued.
 rfFix(m,model::desk,21.1f,1.35f,f,1.4f,.7f,.75f,rfS,true);
 rfFix(m,model::lab_instrument_terminal,21.1f,1.3f,f+.75f,.918f,.468f,.684f,rfS,false);
 rfFix(m,model::stock_cage,22.5f,3.f,f,2.4f,1.f,1.1f,rfW,true);
 rfFix(m,model::prop_cardboard_box,22.5f,2.4f,f+1.1f,.727f,.72f,.367f,.2f,false);
 for(auto [y,yaw]:{std::pair{3.25f,.1f},{3.45f,-.2f},{3.7f,.15f}})rfFix(m,model::item_dosimeter,22.45f,y,f+1.1f,.31f,.146f,.066f,yaw,false);
 taped(m,{22.f,3.f},f+.8f,rfW,Readable::Note,"BAND STOCK","BANDS: 120 IN STOCK.","ISSUED SINCE 01:58: 0.");
 shelf(m,{22.75f,5.3f},rfW);
 bench(m,20.5f,6.f,f);                                                    // where applicants waited for a band
 rfFix(m,model::lab_notice_board,20.5f,6.365f,f+1.3f,1.2f,.07f,.9f,rfN,false);  // the band rules, over their bench
 rfFix(m,model::lab_wall_clock,22.2f,6.384f,f+2.1f,.402f,.032f,.4f,rfN,false);
 m.lights.push_back(lamp({17.5f,3.5f},roof-.16f,.8f,5.f,5600,.03f));
 m.lights.push_back(lamp({21.f,3.5f},roof-.16f,.6f,4.f,5600,.03f));

 // --- R12 The welcome theatre (x 17.4-23, y 6.6-13) -------------------------------------------
 rfiWallY(m,17.2f,17.4f,6.6f,17.8f,{{11.4f,12.6f}},f,roof,paint);
 wall(m,18.4f,10.8f,18.6f,13.f,f,roof,office);                            // the light lock
 wall(m,17.4f,13.f,21.f,13.2f,f,roof,office);rfiSwing(m,21.f,22.f,13.1f,f,roof,1,office);   // 4: D5, to the lounge
 wall(m,22.f,13.f,23.f,13.2f,f,roof,office);
 wall(m,17.4f,6.6f,23.f,13.f,f+.002f,f+.012f,23);
 wall(m,22.85f,7.6f,23.f,12.f,f+.55f,f+2.95f,8);
 // The film stopped on its title slide: the screen itself is the theatre's one sign.
 {AuthoredSign s{{22.84f,9.8f},f+.75f,4.1f,2.f,rfW,"YOUR WORK POWERS THE WORLD","DEPTHWORKS ENERGY RESEARCH / WELCOME, NEW STAFF"};s.accent=rfBlue;m.signs.push_back(s);}
 for(float x:{20.2f,21.4f})for(float y:{8.25f,10.55f})rfFix(m,model::platform_chair,x,y,f,2.27f,.54f,.95f,rfW+(x>21&&y>9?.12f:0),true);
 // The film, running for nobody since 01:58: its console at the back.
 terminal(m,{19.3f,7.4f},"WELCOME TO ENERGY RESEARCH","YOUR WORK POWERS THE WORLD. EVERY SHIFT BELOW BRINGS LIGHT ABOVE.",
  "BANDS STAY ON. SCREENING IS FOR YOUR SAFETY. ASK YOUR SUPERVISOR ABOUT EXPOSURE TIERS.",0,stateId("rf_intake_film_loud"));m.terminals.back().yaw=rfW;
 kit::prop(m,model::med_crash_cart,19.2f,9.6f,f,.736f,.483f,.999f,rfE);
 kit::prop(m,model::pc_tower,19.2f,9.6f,f+.8082f,.2f,.45f,.42f,rfE);
 {auto l=lamp({22.5f,9.8f},f+1.8f,1.f,5.f,6500,.1f,WorldLightMount::Bare);l.period=3.1f;l.onTime=2.6f;m.lights.push_back(l);}
 {auto l=lamp({22.5f,8.5f},f+1.2f,.6f,4.f,5800,.05f,WorldLightMount::Bare);l.period=1.7f;l.onTime=1.2f;l.phase=.4f;m.lights.push_back(l);}
 m.lights.push_back(lamp({19.45f,9.6f},f+1.22f,.5f,3.f,6500,0,WorldLightMount::Bare));   // the projector
 // The house lights are on the film's console: run the reel to its end and they come up.
 for(Vec2 l:{Vec2{19.6f,8.4f},Vec2{21.4f,11.6f}})m.lights.push_back(rfSwitched(lamp(l,roof-.16f,.8f,5.f,5600,.03f),stateId("rf_intake_film_loud_done")));

 // --- R13 The waiting lounge (x 17.4-23, y 13.2-20) -------------------------------------------
 rfFix(m,model::office_couch_green,22.318f,16.3f,f,2.242f,1.364f,1.192f,rfW,true);
 for(float y:{15.f,17.6f})kit::prop(m,model::office_armchair_green,19.6f,y,f,1.399f,1.364f,1.192f,rfE);
 rfFix(m,model::wooden_table,20.95f,16.3f,f,.82f,.43f,.47f,rfE,true);
 for(auto [x,y]:{std::pair{20.85f,16.05f},{21.05f,16.5f}})kit::prop(m,model::mug,x,y,f+.47f,.09f,.09f,.1f,x);
 rfFix(m,model::metro_vending,19.4f,13.54f,f,1.5f,.68f,2.38f,rfN,true);   // front -(sin,cos) yaw: faces +y
 m.lights.push_back(lamp({19.4f,14.35f},f+1.7f,.5f,2.2f,4300,.15f,WorldLightMount::Bare));   // its glow
 rfFix(m,model::file_cabinet_wide,22.75f,13.8f,f,.9f,.5f,.75f,rfW,true);
 kit::prop(m,model::coffee_machine,22.75f,13.8f,f+.75f,.4f,.45f,.5f,rfW);
 rfiPicture(m,model::art_painting_6,23.f,16.3f,f+1.45f,.689f,.03f,.8f,rfW);
 rfFix(m,model::file_cabinet_wide,22.75f,18.9f,f,.9f,.5f,.75f,rfW,true);
 const int intakeDeskLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,22.75f,18.9f,f+.75f,.18f,.398f,.521f,rfE);
 kit::prop(m,model::lab_potted_plant,22.6f,19.7f,f,.44f,.51f,.82f,0);
 for(float y:{14.6f,15.5f})rfLockers(m,17.85f,y,f,rfE);
 taped(m,{17.4f,16.6f},f+1.5f,rfE,Readable::Note,"INTAKE / VISITOR NOTICES","SCREENING CALLS BY TIER. TIER 2 AND ABOVE REPORT TO MEDICAL SCREENING, EAST OF PROCESSING.",
  "LOST BANDS ARE A SAFETY BREACH. A BREACH IS REPORTED, NOT FORGIVEN.");
 rfFix(m,model::med_first_aid_box,17.46f,17.35f,f+1.3f,.367f,.12f,.349f,rfE,false);
 m.lights.push_back(lamp({22.75f,18.9f},f+1.15f,.7f,3.5f,2700,0,WorldLightMount::Bare));m.lights.back().prop=intakeDeskLamp;   // under the desk lamp's shade
 m.lights.push_back(lamp({20.f,16.4f},roof-.16f,.6f,5.f,3400,0));

 // --- The intake detail ---------------------------------------------------------------------------
 // The lead keeps the island and the radio; the medic walks the concourse. Two
 // more wait either side of the island for orders: the sweep sends one west
 // through the staff side and one east through the visitor side.
 const char* clock="rf_intake_clock";
 rfGuard(m,clock,"rf_intake_lead",ActorVisual::Cleanup,GuardSquadRole::Assault,"rf_intake",{{{12.f,15.f},f,6000,rfN}},14,.6f,1,1.3f,0,110);
 rfGuard(m,clock,"rf_intake_hall",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"rf_intake",
  {{{8.f,8.2f},f,4000,rfE},{{15.8f,8.2f},f,4000,rfW},{{16.2f,16.6f},f,3000,rfN},{{8.2f,16.6f},f,3000,rfN}},13,.6f,2,1.2f);
 m.actorTracks.back().ai.dropsSidearm=true; // the first pistol the player can take off a body
 rfGuard(m,clock,"rf_intake_sweep_a",ActorVisual::CleanupRusher,GuardSquadRole::Rusher,"rf_intake",
  {{{9.4f,14.9f},f,3000,rfN},{{9.6f,8.2f},f,2500,rfN},{{12.f,8.2f},f,2500,rfN},{{12.f,5.f},f,3000,rfN},{{8.2f,5.f},f,3000,rfW},
   {{6.f,3.6f},f,4000,rfW},{{2.4f,4.4f},f,3000,rfW},{{6.f,3.6f},f,1000,rfE},{{12.f,5.f},f,1000,rfS}},13,.6f,1,1.4f,sweep);
 rfGuard(m,clock,"rf_intake_sweep_b",ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,"rf_intake",
  {{{14.6f,14.9f},f,3000,rfN},{{14.6f,8.2f},f,2500,rfN},{{12.4f,5.2f},f,2000,rfE},{{17.2f,4.f},f,3000,rfE},{{18.3f,5.9f},f,1500,rfS},
   {{18.9f,9.f},f,4000,rfE},{{19.4f,12.4f},f,1500,rfS},{{20.f,16.3f},f,3000,rfW},{{16.4f,18.3f},f,2000,rfW}},12,.6f,2,1.2f,sweep);
 m.actorTracks.back().ai.dropsSidearm=true;
 for(auto& t:m.actorTracks)if(t.ai.mode==ActorAiMode::Guard)t.ai.alertState=alert;
 // The intake officer: no corpse clip sits, so he lies where he fell.
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{5.7f,5.45f},f,1.2f,4,1.f},{60000,{5.7f,5.45f},f,1.2f,4,1.f}});

 // --- The Investigator on the radio ---------------------------------------------------------------
 // Stepping into the vestibule starts it; the order at 13 s sets the sweep going.
 // The voice comes from the detail's radio on the island ledge.
 rfSequence(m,"rf_intake_radio","rf_intake_sweep",20000,13000,8.8f,1.f,15.2f,3.15f);
 const Vec2 radio{12.4f,13.2f};const float rz=f+1.2f;
 cue(m,"rf_intake_radio",800,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. Report.",.6f,rz,"inv_intake_1");
 cue(m,"rf_intake_radio",3800,Sound::Empty,radio,"RADIO: Intake holding, sir. We heard the great door go.",.6f,rz,"rf_intake_lead_1");
 cue(m,"rf_intake_radio",7600,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then someone opened it. Sweep the booths. Anyone without a band is exposed.",.6f,rz,"inv_intake_2");
 cue(m,"rf_intake_radio",12200,Sound::Empty,radio,"RADIO: ...Copy. Booths. Move.",.6f,rz,"rf_intake_lead_2");
 cue(m,"rf_intake_radio",15600,Sound::Empty,radio,"INVESTIGATOR (RADIO): And Intake. If it is one of ours, it is still exposed.",.6f,rz,"inv_intake_3");

 // --- The alarm, the PA page and the film: three things that pull a man --------------------------
 {ScriptEvent e;e.id=stateId("rf_intake_alarm_on");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;e.requireState=alert;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId("rf_intake_alarm");a.value=1;e.actions={a};m.scriptEvents.push_back(e);}
 {TimedSequence s;s.timerState=stateId("rf_intake_alarm");s.finishState=stateId("rf_intake_alarm_done");s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;
  s.durationMs=20000;s.finishAtMs=20000;s.tickSound=Sound::Alarm;s.soundIntervalMs=900;s.soundUntilMs=20000;s.gain=.55f;s.pitch=1;
  s.soundAt={12.f,10.f};s.soundZ=f+3.f;m.timedSequences.push_back(s);}
 for(auto [pos,z,phase]:{std::tuple{Vec2{12.f,7.4f},f+2.95f,0.f},{Vec2{12.f,4.45f},f+3.15f,.5f}}){
  auto l=lamp(pos,z,1.6f,7.f,1000,-.5f,WorldLightMount::Wall,kPi*.5f);l.enabledState=alert;l.period=1;l.onTime=.5f;l.phase=phase;m.lights.push_back(l);}
 rfSequence(m,"rf_intake_page","rf_intake_page_done",6000,6000);
 cue(m,"rf_intake_page",200,Sound::AlarmDistant,{4.8f,9.2f},"PA: TIER TWO STAFF TO THE INTAKE DESK.",.7f,f+2.5f,"rf_intake_pa");m.sequenceCues.back().aiRadius=12;
 rfSequence(m,"rf_intake_film_loud","rf_intake_film_loud_done",9000,9000);
 cue(m,"rf_intake_film_loud",300,Sound::Machine,{22.5f,9.8f},"FILM: WELCOME TO ENERGY RESEARCH. YOUR WORK POWERS THE WORLD.",1.f,f+1.6f,"rf_intake_film_1");m.sequenceCues.back().aiRadius=12;
 cue(m,"rf_intake_film_loud",4700,Sound::Machine,{22.5f,9.8f},"FILM: EVERY SHIFT BELOW BRINGS LIGHT ABOVE. WEAR YOUR BAND.",1.f,f+1.6f,"rf_intake_film_2");m.sequenceCues.back().aiRadius=12;
 machineryClock(m,"rf_intake_film",24000);
 cue(m,"rf_intake_film",500,Sound::Machine,{22.5f,9.8f},"",.25f,f+1.6f);

 // --- Rewards, throwables and names -------------------------------------------------------------
 m.pickupSpawns={{{2.6f,2.8f},PickupKind::Ammo,f},{{21.4f,3.3f},PickupKind::Ammo,f},{{5.6f,9.2f},PickupKind::Health,f},{{18.3f,19.2f},PickupKind::Health,f}};
 m.pickupSpawns.push_back({{18.85f,15.2f},PickupKind::HandgunAmmo,f});   // 9 mm left out in front of the staff lockers
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{5,{8.1f,3.f},ctr},{5,{8.25f,3.15f},ctr},{1,{4.6f,5.6f},f},{2,{11.2f,13.25f},ledge},{3,{5.05f,15.8f},table},
  {4,{20.6f,1.5f},f+.75f},{4,{21.6f,1.55f},f+.75f},{2,{20.9f,16.6f},f+.47f},{2,{21.05f,15.95f},f+.47f},{1,{20.4f,14.1f},f}});
 zone(m,"RESEARCH INTAKE / INTERLOCK",8.8f,1.f,15.2f,4.45f);
 zone(m,"RESEARCH INTAKE / SECURITY POST",1.f,1.f,8.6f,6.4f);
 zone(m,"RESEARCH INTAKE / HOLDING",3.f,6.6f,6.8f,11.8f);
 zone(m,"RESEARCH INTAKE / STAFF CORRIDOR",1.f,6.6f,2.8f,20.f);
 zone(m,"RESEARCH INTAKE / EFFECTS",3.f,12.f,6.8f,17.6f);
 zone(m,"RESEARCH INTAKE / STAFF CORRIDOR",1.f,17.6f,6.8f,20.f);
 zone(m,"RESEARCH INTAKE / SCREENING",8.8f,4.45f,15.2f,7.4f);
 zone(m,"RESEARCH INTAKE / BADGE OFFICE",15.4f,1.f,23.f,6.4f);
 zone(m,"RESEARCH INTAKE / WELCOME THEATRE",17.4f,6.6f,23.f,13.f);
 zone(m,"RESEARCH INTAKE / LOUNGE",17.4f,13.2f,23.f,20.f);
 zone(m,"RESEARCH INTAKE / CONCOURSE",6.8f,7.4f,17.4f,24.f);
 zone(m,"RESEARCH INTAKE",0,0,24,24);
 titleCard(m,"RESEARCH FRONT",6.8f,7.4f,17.2f,17.6f);
 recessedFittings(m,true);freightFinish(m,-1);
 return p;
}
// A reply in a conversation (DialogueReply).
DialogueReply rfReply(const char* text,const char* next="",std::initializer_list<std::pair<const char*,int>> sets={},int stance=-1,const char* require="",const char* hide=""){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;r.require=*require?stateId(require):0;r.hide=*hide?stateId(hide):0;r.stance=stance;
 for(auto [id,value]:sets)r.sets.push_back({stateId(id),value});return r;
}
void rfLine(Map& m,const char* node,const char* speaker,const char* line,std::vector<DialogueReply> replies){
 m.dialogue.push_back({stateId(node),speaker,line,node,std::move(replies)});
}

// --- Personnel Processing kit (rfp): chunk 69's own helpers ------------------------------------
// An open doorway from x1 to x2 through a wall along x (y1..y2): steel posts and head, the wall above.
void rfpDoorX(Map& m,float x1,float x2,float y1,float y2,float f,float top,int mat){
 for(float x:{x1-.08f,x2})doorTrim(m,x,y1-.03f,x+.08f,y2+.03f,f,f+2.3f,20);
 doorTrim(m,x1-.08f,y1-.03f,x2+.08f,y2+.03f,f+2.22f,f+2.3f,20);
 wall(m,x1,y1,x2,y2,f+2.3f,top,mat);
}
// A full-height wall along x with open doorways (x pairs) cut through it.
void rfpWallX(Map& m,float y1,float y2,float x1,float x2,std::initializer_list<std::pair<float,float>> gaps,float f,float top,int mat){
 float x=x1;for(auto [a,b]:gaps){if(a>x)wall(m,x,y1,a,y2,f,top,mat);rfpDoorX(m,a,b,y1,y2,f,top,mat);x=b;}
 if(x2>x)wall(m,x,y1,x2,y2,f,top,mat);
}
// A seam portal on the W or E chunk wall: steel posts and a panel head at 2.7 m over y 9-14.
void rfpPortal(Map& m,float x1,float x2,float f,float roof,int mat=28){
 for(float y:{9.f,13.86f})wall(m,x1,y,x2,y+.14f,f,f+2.7f,2);
 wall(m,x1,9.f,x2,14.f,f+2.7f,roof,mat);wall(m,x1-.02f,9.f,x2+.02f,14.f,f+2.66f,f+2.7f,2);
}
// A belt-barrier post run along y at x: steel posts, the belt between them at 0.9 m.
void rfpBelt(Map& m,float x,float y1,float y2,float f){
 for(float y:{y1,y2}){wall(m,x-.03f,y-.03f,x+.03f,y+.03f,f,f+.95f,12);wall(m,x-.12f,y-.12f,x+.12f,y+.12f,f,f+.03f,12);}
 wall(m,x-.012f,y1+.03f,x+.012f,y2-.03f,f+.86f,f+.92f,19);
}

// PERSONNEL PROCESSING (manifest 46). The hub every shift passed through
// twice a day: out of street clothes in the locker room, into a suit at the
// dress-out, banded through the gates, read by the tier clerks, then down
// through the clearance door to the labs; back the other way through suit
// return. At 01:58 the lab shift came back up through it in their suits; at
// 02:30 the tier 2 list stopped the line; at 03:44 the Investigator's detail
// took every suit on the rail for the sweep at Medical. A Cleanup private
// who stopped following orders there is sitting it out in the locker room:
// he will not lift his gun, and he is the first to say out loud who is giving
// the orders. Route: N seam from Intake into the crossing hall; W arm to
// Administration (the clearance desk), E arm to Medical Screening, S through
// the gates to the clearance door (proc_clearance). Staff loops: hall ->
// lockers -> W arm; hall -> dress-out -> E arm; E arm -> tier office ->
// screening -> gate hall -> suit return -> W arm.
std::shared_ptr<const Map> makePersonnelProcessing(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[69];m.definition=CampaignChunks[69];m.definition.ambient=.30f;
 // Each room its own finish: the public hall, gates and arms in Intake's maroon-banded
 // paint (panel), the locker room, dress-out and suit store in green-banded paint, the
 // washroom and suit return in white tile, the tier office green too, screening in teal.
 m.wallSurface=SurfacePaintedMaroon;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 const int panel=38,tile=27,steel=2,green=37,teal=36;
 const StateId clearance=stateId("proc_clearance"),stance=stateId("rf_proc_stance");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,clearance});   // door 0: down to the labs, on clearance
 rfJambCovers(m,0,f);
 rectangle(m.layers[0],1,20,9,23,'#');            // solid behind suit return
 machineryClock(m,"rf_proc_clock",60000);

 // --- Plan ---------------------------------------------------------------------------------------
 wall(m,9.8f,1.f,10.f,3.8f,f,roof,panel);wall(m,14.f,1.f,14.2f,3.8f,f,roof,panel);          // the link from Intake
 rfpWallX(m,3.6f,3.8f,1.f,9.8f,{{7.4f,8.4f}},f,roof,tile);                                     // washroom / locker room
 rfpWallX(m,3.6f,3.8f,14.2f,23.f,{{16.4f,17.4f}},f,roof,green);                                // suit store / dress-out
 wall(m,8.6f,3.8f,10.f,4.f,f,roof,panel);wall(m,14.f,3.8f,15.4f,4.f,f,roof,panel);            // hall north
 wall(m,10.f,3.8f,14.f,4.f,f+2.7f,roof,panel);wall(m,10.f,3.78f,14.f,4.02f,f+2.66f,f+2.7f,steel);
 rfiWallY(m,8.4f,8.6f,3.8f,9.f,{{5.4f,6.6f}},f,roof,panel);                                     // hall west: lockers
 rfiWallY(m,15.4f,15.6f,3.8f,9.f,{{5.4f,6.6f}},f,roof,panel);                                   // hall east: dress-out
 rfpWallX(m,8.8f,9.f,1.f,8.4f,{{1.2f,2.4f}},f,roof,green);                                      // lockers to the W arm
 rfpWallX(m,8.8f,9.f,15.6f,23.f,{{20.4f,21.6f}},f,roof,green);                                  // dress-out to the E arm
 for(float x:{8.4f,15.4f}){wall(m,x,9.f,x+.2f,14.f,f+2.45f,roof,panel);wall(m,x-.02f,9.f,x+.22f,14.f,f+2.41f,f+2.45f,steel);}   // arm mouths
 rfpWallX(m,14.f,14.2f,1.f,9.6f,{{5.f,6.2f}},f,roof,tile);                                     // W arm south: suit return
 rfpWallX(m,14.f,14.2f,14.4f,23.f,{{21.2f,22.2f}},f,roof,green);                                // E arm south: tier office
 rfiWallY(m,9.4f,9.6f,14.2f,22.f,{{17.6f,18.8f}},f,roof,panel);                                 // gate hall west
 wall(m,14.4f,14.2f,14.6f,15.f,f,roof,panel);rfiGlass(m,14.4f,15.f,14.6f,19.4f,f,roof,panel);         // the tier office window
 rfiWallY(m,14.4f,14.6f,19.4f,22.f,{{20.2f,21.4f}},f,roof,panel);
 rfpWallX(m,19.2f,19.4f,14.6f,23.f,{{21.4f,22.4f}},f,roof,green);                               // tier office / screening
 rfpWallX(m,19.4f,19.6f,1.f,9.4f,{},f,roof,tile);                                               // suit return south
 wall(m,9.6f,21.8f,10.f,22.f,f,roof,panel);wall(m,14.f,21.8f,14.4f,22.f,f,roof,panel);
 wall(m,9.8f,22.f,10.f,23.f,f,roof,panel);wall(m,14.f,22.f,14.2f,23.f,f,roof,panel);            // clearance vestibule
 wall(m,10.f,21.8f,14.f,22.f,f+2.5f,roof,panel);wall(m,10.f,21.78f,14.f,22.02f,f+2.46f,f+2.5f,steel);
 rfpPortal(m,1.f,1.12f,f,roof,panel);rfpPortal(m,22.88f,23.f,f,roof,panel);
 // Linings: each room's finish carried over the chunk's own walls and the far faces of its partitions.
 for(auto [x1,y1,x2,y2,mat]:{std::tuple{1.02f,1.f,9.8f,1.02f,tile},{1.f,1.f,1.02f,3.6f,tile},{1.02f,3.8f,7.32f,3.82f,green},{1.f,3.8f,1.02f,8.8f,green},
   {8.38f,3.82f,8.4f,5.32f,green},{8.38f,6.68f,8.4f,8.8f,green},{15.6f,3.82f,15.62f,5.32f,green},{15.6f,6.68f,15.62f,8.8f,green},
   {14.2f,1.f,22.98f,1.02f,green},{22.98f,1.f,23.f,8.8f,green},
   {2.48f,9.f,8.4f,9.02f,panel},{15.6f,9.f,20.32f,9.02f,panel},{21.68f,9.f,22.88f,9.02f,panel},
   {1.12f,13.98f,4.92f,14.f,panel},{6.28f,13.98f,9.6f,14.f,panel},{14.4f,13.98f,21.12f,14.f,panel},{22.28f,13.98f,22.88f,14.f,panel},
   {1.f,14.2f,1.02f,19.4f,tile},{9.38f,14.2f,9.4f,17.52f,tile},{9.38f,18.88f,9.4f,19.4f,tile},
   {22.99f,14.2f,23.f,19.2f,green},{22.99f,19.4f,23.f,23.f,teal},{14.6f,22.99f,22.99f,23.f,teal},
   {14.6f,19.4f,21.32f,19.42f,teal},{22.48f,19.4f,22.98f,19.42f,teal},{14.6f,19.42f,14.62f,20.12f,teal},{14.6f,21.48f,14.62f,22.98f,teal}})
  wall(m,x1,y1,x2,y2,f,roof,mat);

 // --- The link and the crossing hall (x 8.6-15.4, y 4-14) ----------------------------------------
 // Walk in from Intake and the axis runs straight through the gates to the red door.
 m.lights.push_back(lamp({12.f,2.4f},roof-.16f,.75f,5.f,6000,.04f));
 rfFix(m,model::pwr_fire_cabinet,10.f+.1f,2.6f,f+1.3f,.25f,.2f,.56f,rfE,false);
 rfFix(m,model::lab_wall_clock,12.f,4.036f,f+2.78f,.402f,.032f,.4f,rfS,false);
 for(float y:{4.4f,13.6f})floorLine(m,10.6f,y,13.4f,y+.1f,f,13);
 rfFix(m,model::lab_pa_horn,8.882f,4.7f,f+2.55f,.495f,.564f,.433f,rfE,false);
 kit::prop(m,model::trash_can,9.f,4.4f,f,.5f,.5f,.62f,.3f);
 rfFix(m,model::water_cooler,15.1f,4.35f,f,.35f,.35f,1.1f,0,true);
 // Benches either side under the notices: where the shift waited for its call.
 rfFix(m,model::bench,8.95f,7.9f,f,1.86f,.44f,.63f,kPi*.5f,true);
 rfFix(m,model::bench,15.05f,7.9f,f,1.86f,.44f,.63f,-kPi*.5f,true);
 taped(m,{8.6f,7.6f},f+1.5f,rfE,Readable::Note,"PROCESSING / DIRECTIONS","WEST: RESEARCH ADMINISTRATION. EAST: MEDICAL SCREENING. SOUTH: LABORATORIES, CLEARANCE ONLY.",
  "CLEARANCE IS ISSUED AT THE ADMINISTRATION DESK. TIER 2 AND ABOVE: SCREENING FIRST.");
 taped(m,{15.4f,7.9f},f+1.5f,rfW,Readable::Note,"SHIFT ROTA / NIGHT","B SHIFT DOWN 22:00, UP 06:00. TIER 2: SCREENING BEFORE DESCENT.",
  "01:58 ALL SHIFTS HELD BELOW. LOCKERS TO BE CLEARED BY SECURITY.");
 // The cart the bands went out on, left where the detail stopped it.
 handCart(m,13.7f,6.6f,f);
 kit::prop(m,model::prop_cardboard_box_open,13.7f,6.4f,f+.29f,.55f,.55f,.55f,0);
 kit::prop(m,model::prop_cardboard_box,13.7f,7.6f,f,.5f,.5f,.25f,0);
 // The queue: a stop line, belts between the lanes, three gates under a lit header.
 floorLine(m,9.7f,12.05f,14.3f,12.15f,f,13);
 for(float x:{11.27f,12.73f}){rfpBelt(m,x,10.7f,13.36f,f);wall(m,x-.03f,12.f,x+.03f,12.06f,f,f+.95f,12);wall(m,x-.12f,11.91f,x+.12f,12.15f,f,f+.03f,12);}
 for(float x:{10.53f,12.f,13.47f})floorLine(m,x-.05f,10.6f,x+.05f,12.f,f,14);
 for(float x:{9.8f,11.27f,12.73f,14.2f})rfFix(m,model::ticket_gate,x,14.1f,f,.4f,1.42f,1.09f,0,true);
 wall(m,9.6f,13.9f,14.4f,14.3f,f+2.4f,roof,panel);wall(m,9.6f,13.88f,14.4f,14.32f,f+2.36f,f+2.4f,steel);
 for(float x:{10.53f,12.f,13.47f})rfFix(m,model::lab_status_display,x,13.85f,f+2.6f,.693f,.058f,.2f,rfS,false);
 paperDrift(m,9.2f,8.4f,14.8f,12.f,f,8,693);
 for(auto [x,y,yaw]:{std::tuple{11.4f,12.9f,.5f},{10.1f,11.2f,2.2f},{14.1f,9.8f,1.f},{12.6f,7.4f,2.6f}})kit::prop(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw);
 for(Vec2 l:{Vec2{12.f,6.4f},Vec2{12.f,10.4f}})m.lights.push_back(lamp(l,roof-.16f,.9f,6.5f,6200,.04f));
 m.lights.push_back(rfiTube({12.f,13.1f},roof-.16f,1.1f,4.5f,6800,.04f,kPi*.5f));
 rfSign(m,8.655f,11.5f,f+2.49f,"RESEARCH ADMINISTRATION","CLEARANCE DESK",3.f,rfE,rfBlue);
 rfSign(m,15.345f,11.5f,f+2.49f,"MEDICAL SCREENING","TIER 2 AND ABOVE",3.f,rfW,rfBlue);

 // --- The gate hall and the clearance door (x 9.6-14.4, y 14.2-22) --------------------------------
 // The issue ledge under the tier window: dosimeters handed out, never collected.
 wall(m,14.1f,15.6f,14.4f,18.6f,f+.95f,f+.99f,steel);
 for(float y:{15.8f,18.4f})wall(m,14.36f,y-.02f,14.4f,y+.02f,f+.7f,f+.95f,steel);
 for(auto [y,yaw]:{std::pair{15.95f,.1f},{16.5f,-.2f},{17.7f,.3f}})kit::prop(m,model::item_dosimeter,14.25f,y,f+.99f,.31f,.146f,.066f,kPi*.5f+yaw);
 kit::prop(m,model::item_id_card,14.22f,17.1f,f+.99f,.132f,.085f,.002f,.4f);
 floorLine(m,11.95f,14.9f,12.05f,21.7f,f,14);
 rfFix(m,model::lab_pa_horn,9.882f,15.3f,f+2.5f,.495f,.564f,.433f,rfE,false);
 extinguisher(m,9.6f,16.5f,f,rfE);
 kit::prop(m,model::trash_can,10.f,21.4f,f,.5f,.5f,.62f,.7f);
 kit::prop(m,model::item_dosimeter,11.2f,19.6f,f,.31f,.146f,.066f,2.3f);
 paperDrift(m,10.f,15.f,14.f,21.f,f,4,694);
 rfSign(m,12.f,21.745f,f+2.52f,"LABORATORIES","CLEARANCE ONLY / ISSUED AT RESEARCH ADMINISTRATION",3.6f,rfN,rfRed);
 rfFix(m,model::lab_id_scanner,10.03f,22.6f,f+1.1f,.3f,.06f,.41f,rfW,false);
 m.lights.push_back(lamp({12.f,16.6f},roof-.16f,.85f,5.5f,6000,.04f));
 m.lights.push_back(lamp({12.f,19.8f},roof-.16f,.6f,4.5f,5000,.06f));
 for(auto [x,yaw]:{std::pair{10.f,0.f},{14.f,kPi}})m.lights.push_back(rfSwitched(lamp({x,22.6f},f+2.3f,.8f,4.f,1000,-.5f,WorldLightMount::Wall,yaw),clearance,true));
 m.lights.push_back(rfSwitched(rfiTube({12.f,22.5f},roof-.16f,.8f,4.f,5200,.05f,kPi*.5f),clearance));

 // --- The locker room (x 1-8.4, y 3.8-8.8) and the washroom (x 1-9.8, y 1-3.6) ---------------------
 for(int i=0;i<7;++i)rfLockers(m,1.45f+i*.9f,4.25f,f,rfS);
 for(int i=0;i<6;++i)rfLockers(m,2.85f+i*.9f,8.35f,f,rfN);
 for(float x:{3.6f,5.6f})rfFix(m,model::bench,x,6.3f,f,1.86f,.44f,.63f,0,true);
 // Lockers being emptied into boxes when the sweep went to Medical.
 kit::prop(m,model::prop_cardboard_box_open,8.12f,8.45f,f,.45f,.45f,.45f,0);
 kit::prop(m,model::prop_cardboard_box,7.75f,7.4f,f,.5f,.5f,.25f,-.2f);
 paperDrift(m,2.8f,4.9f,8.2f,7.7f,f,6,69);
 for(auto [x,y,yaw]:{std::tuple{4.6f,5.2f,.8f},{6.8f,7.5f,2.f}})kit::prop(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw);
 // The private: the kit he opened, where he bled, the radio he stopped answering.
 rfFix(m,model::med_first_aid_box,1.06f,5.6f,f+1.35f,.367f,.12f,.349f,rfE,false);
 wall(m,1.45f,6.35f,2.35f,7.15f,f+.004f,f+.010f,16);
 for(auto [x,y,yaw]:{std::tuple{1.3f,6.1f,.7f},{2.5f,7.25f,2.1f},{1.25f,7.4f,-.4f}})kit::prop(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw);
 kit::prop(m,model::item_walkie,2.55f,5.95f,f,.114f,.376f,.046f,1.2f);
 extinguisher(m,8.4f,7.9f,f,rfW);
 // Back of house runs on the old warm tubes; only the public hall got the cold white.
 m.lights.push_back(rfiTube({3.f,6.3f},roof-.16f,.6f,5.f,3300,.04f,kPi*.5f));
 m.lights.push_back([&]{auto l=rfiTube({6.4f,6.3f},roof-.16f,.7f,5.f,3500,.04f,kPi*.5f);l.flicker=.3f;return l;}());
 // His radio, on the floor where he put it down: still open, nobody answering it.
 m.ambients.push_back({Sound::AmbRadioStatic,{2.55f,5.95f},f+.1f,.3f,7,AmbientEmitter::Loop});
 // The washroom: two stalls, the scrub sinks under one long mirror.
 for(float x:{2.07f,4.22f})rfFix(m,model::med_washroom_stall,x,1.78f,f,2.13f,1.54f,2.4f,rfS,true);
 for(float x:{6.1f,6.9f,7.7f})rfFix(m,model::med_scrub_sink,x,1.29f,f,.49f,.54f,1.13f,rfS,true);
 wall(m,5.75f,1.f,8.05f,1.03f,f+1.f,f+2.f,27);wall(m,5.75f,1.f,8.05f,1.14f,f+1.36f,f+1.39f,steel);   // the splashback and soap shelf
 rfFix(m,model::med_floor_drain,7.f,2.7f,f,.5f,.5f,.01f,0,false);
 kit::prop(m,model::trash_can,9.35f,1.45f,f,.5f,.5f,.62f,0);
 m.lights.push_back(rfiTube({5.6f,2.4f},roof-.16f,.6f,5.f,4300,.32f,kPi*.5f));   // the washroom's green tube

 // --- The dress-out (x 15.6-23, y 3.8-8.8) and the suit store (x 14.2-23, y 1-3.6) ------------------
 // Suit lockers on the east wall, the dosimeter board, and the suit rail the detail emptied.
 for(int i=0;i<5;++i)rfLockers(m,22.55f,4.25f+i*.9f,f,rfW);
 wall(m,18.f,3.8f,20.4f,3.84f,f+1.1f,f+1.9f,steel);
 for(float z:{1.3f,1.62f})wall(m,18.f,3.84f,20.4f,4.f,f+z,f+z+.03f,steel);
 for(auto [x,z]:{std::pair{18.3f,1.65f},{18.63f,1.65f},{19.95f,1.65f},{19.3f,1.33f}})kit::prop(m,model::item_dosimeter,x,3.92f,f+z,.31f,.146f,.066f,0);
 taped(m,{21.f,3.8f},f+1.45f,rfS,Readable::Note,"SUIT ISSUE / 03:44","18 SUITS, 18 RESPIRATORS TO THE INVESTIGATOR'S DETAIL. RAIL EMPTY.",
  "NOT SIGNED FOR. BY ORDER: TREAT RETURNS AS EXPOSED.");
 m.pipes.push_back({{16.6f,8.55f},{20.f,8.55f},f+1.9f,.02f,f+1.9f,1});
 for(float x:{16.58f,19.98f})wall(m,x,8.5f,x+.04f,8.8f,f+1.86f,f+1.94f,steel);
 for(float x=16.9f;x<19.8f;x+=.35f)wall(m,x,8.53f,x+.015f,8.57f,f+1.72f,f+1.88f,steel);
 rfFix(m,model::bench,20.f,6.2f,f,1.86f,.44f,.63f,kPi*.5f,true);
 rfFix(m,model::metal_shelf,15.87f,7.75f,f,1.8f,.5f,1.8f,kPi*.5f,true);
 for(auto [y,yaw]:{std::pair{7.25f,.1f},{8.2f,-.15f}})kit::prop(m,model::prop_cardboard_box,15.87f,y,f+1.65f,.45f,.45f,.23f,kPi*.5f);
 kit::prop(m,model::prop_cardboard_box_open,18.9f,4.35f,f,.55f,.55f,.55f,-.4f);
 for(auto [x,y,yaw]:{std::tuple{19.9f,5.6f,1.4f},{20.1f,6.85f,1.9f}})kit::prop(m,model::item_dosimeter,x,y,f+.63f,.31f,.146f,.066f,yaw);
 paperDrift(m,16.f,4.6f,21.6f,8.4f,f,4,695);
 m.lights.push_back(rfiTube({17.6f,6.3f},roof-.16f,.7f,5.f,3400,.03f,kPi*.5f));
 m.lights.push_back(rfiTube({21.f,6.3f},roof-.16f,.7f,5.f,3400,.03f,kPi*.5f));
 // The store: shelves of suit stock torn into, the cage the respirators came from.
 for(float x:{16.f,18.f})rfFix(m,model::metal_shelf,x,1.27f,f,1.8f,.5f,1.8f,0,true);
 for(auto [x,yaw]:{std::pair{15.6f,.1f},{16.3f,-.1f},{17.6f,.15f}})kit::prop(m,model::prop_cardboard_box,x,1.27f,f+1.65f,.5f,.45f,.25f,0);
 rfFix(m,model::stock_cage,21.3f,1.52f,f,2.4f,1.f,1.1f,0,true);
 kit::prop(m,model::prop_cardboard_box_open,21.f,1.52f,f+1.1f,.55f,.55f,.55f,.3f);
 for(auto [x,y,yaw]:{std::tuple{19.4f,2.9f,.6f},{15.3f,2.85f,-.3f}})kit::prop(m,model::prop_cardboard_box_open,x,y,f,.6f,.6f,.6f,yaw);
 kit::prop(m,model::prop_cardboard_box,20.3f,3.0f,f,.6f,.6f,.3f,.9f);
 m.lights.push_back(rfiTube({18.6f,2.4f},roof-.16f,.5f,5.f,3800,.1f,kPi*.5f));

 // --- The W arm to Administration (x 1-8.4, y 9-14) ------------------------------------------------
 // Where staff waited to be called to the clearance desk.
 for(float x:{4.2f,6.3f})rfFix(m,model::platform_chair,x,9.27f,f,1.8f,.5f,.8f,kPi,true);
 for(float x:{2.9f,7.4f})rfFix(m,model::platform_chair,x,13.73f,f,1.8f,.5f,.8f,0,true);
 rfFix(m,model::lab_wall_clock,5.25f,9.016f,f+2.1f,.402f,.032f,.4f,rfS,false);
 kit::prop(m,model::lab_potted_plant,1.5f,9.5f,f,.37f,.43f,.69f,0);
 kit::prop(m,model::trash_can,1.5f,13.6f,f,.5f,.5f,.62f,0);
 for(auto [x,y,yaw]:{std::tuple{3.4f,11.3f,1.f},{6.6f,12.2f,2.5f}})kit::prop(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw);
 paperDrift(m,1.6f,10.f,8.f,13.f,f,4,696);
 m.lights.push_back(lamp({2.8f,11.5f},roof-.16f,.75f,5.5f,5600,.05f));
 m.lights.push_back(lamp({6.4f,11.5f},roof-.16f,.75f,5.5f,5600,.05f));

 // --- The E arm to Medical (x 15.6-23, y 9-14) ------------------------------------------------------
 for(float x:{17.5f,19.4f})rfFix(m,model::platform_chair,x,9.27f,f,1.8f,.5f,.8f,kPi,true);
 for(float x=16.f;x<22.4f;x+=.9f)floorLine(m,x,11.45f,x+.5f,11.55f,f,13);
 kit::prop(m,model::med_wheelchair,16.4f,13.3f,f,.55f,1.03f,.9f,1.2f);
 kit::prop(m,model::lab_potted_plant,22.5f,9.5f,f,.37f,.43f,.69f,0);
 kit::prop(m,model::trash_can,20.6f,13.6f,f,.5f,.5f,.62f,0);
 extinguisher(m,19.f,14.f,f,rfN);
 m.lights.push_back(lamp({17.6f,11.5f},roof-.16f,.75f,5.5f,5600,.05f));
 m.lights.push_back(lamp({21.f,11.5f},roof-.16f,.75f,5.5f,5600,.05f));

 // --- The tier office (x 14.6-23, y 14.2-19.2): where a tier was read ------------------------------
 rfDesk(m,15.45f,15.9f,f,rfW,1);rfDesk(m,15.45f,17.9f,f,rfW,2);
 terminal(m,{15.62f,15.4f},"SCREENING INTAKE / TIER BOARD","BANDS READ AT THE GATES. TIER 1: CLEAR. TIER 2: SCREENING DUE. TIER 3: OBSERVATION.",
  "02:30 ALL TIER 2 STAFF HELD FOR SCREENING. 03:52 TIER LIST COPIED TO INTERNAL INVESTIGATIONS.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;m.terminals.back().surface=.75f;
 // The tier board: sixteen gate readouts, one per lane and shift.
 for(int i=0;i<4;++i)for(int j=0;j<4;++j)rfFix(m,model::lab_status_display,22.97f,15.45f+j*.8f,f+1.15f+i*.3f,.693f,.058f,.2f,rfE,false);
 for(float x:{16.6f,17.6f})rfFix(m,model::med_records_cabinet,x,14.47f,f,.9f,.49f,1.f,rfS,true);
 rfFix(m,model::lab_photocopier,19.6f,18.8f,f,.83f,.75f,.97f,rfS,true);
 paperDrift(m,18.4f,17.f,21.2f,18.3f,f,6,697);
 kit::prop(m,model::office_paper_stack,19.5f,18.85f,f+.97f,.33f,.44f,.03f,.2f);
 m.lights.push_back(lamp({17.8f,16.7f},roof-.16f,.85f,5.5f,6000,.04f));
 m.lights.push_back([&]{auto l=lamp({21.2f,16.7f},roof-.16f,.8f,5.5f,6000,.04f);l.flicker=.25f;return l;}());
 m.lights.push_back(lamp({22.35f,16.65f},f+1.75f,.7f,3.f,7000,.1f,WorldLightMount::Bare));   // the board's glow

 // --- Screening (x 14.6-23, y 19.4-23): bands read, vitals taken, films hung ----------------------
 // The room is down to one tube. What lights it is the lightbox bank on the north
 // wall, the last six films still up, and the monitor behind the curtain.
 rfFix(m,model::med_nurse_desk,17.f,21.2f,f,1.18f,.56f,.81f,rfW,true);
 kit::prop(m,model::office_chair,17.75f,21.2f,f,.55f,.55f,.95f,rfE);
 kit::prop(m,model::office_chair,16.1f,21.25f,f,.55f,.55f,.95f,rfW+.2f);
 rfFix(m,model::office_paper_stack,16.95f,21.45f,f+.81f,.33f,.44f,.03f,.15f,false);
 for(auto [y,yaw]:{std::pair{21.2f,.3f},{21.69f,1.9f}})rfFix(m,model::item_id_card,16.9f,y,f+.81f,.132f,.085f,.002f,yaw,false);
 rfFix(m,model::lab_id_scanner,14.63f,21.9f,f+1.1f,.3f,.06f,.41f,rfW,false);
 for(int i=0;i<4;++i)rfFix(m,i==3?model::med_xray_viewer_skull:model::med_xray_viewer_chest,16.1f+i*.5f,19.45f,f+1.3f,.41f,.1f,.5f,rfS,false);
 wall(m,15.8f,19.4f,17.9f,19.62f,f+1.08f,f+1.11f,steel);for(float x:{15.9f,17.8f})wall(m,x-.015f,19.4f,x+.015f,19.6f,f+.96f,f+1.08f,steel);   // the film ledge
 rfFix(m,model::office_paper_stack,16.35f,19.51f,f+1.11f,.165f,.22f,.015f,1.5f,false);
 rfFix(m,model::office_paper_sheet,17.3f,19.51f,f+1.11f,.2f,.147f,.002f,1.7f,false);
 taped(m,{18.4f,19.4f},f+1.5f,rfS,Readable::Note,"SCREENING / FILMS 9-14","TIER 2, B SHIFT, TAKEN 02:26. A DENSITY IN THE LEFT LUNG ON ALL SIX. SAME PLACE.",
  "MANUAL NOTE: IT IS NOT ON THEIR 22:00 FILMS.");
 // Where the six waited, and what one of them left.
 rfFix(m,model::platform_chair,16.4f,22.73f,f,1.8f,.5f,.8f,0,true);
 rfFix(m,model::med_crutches,17.55f,22.95f,f,.17f,.06f,1.2f,rfN,false);
 rfFix(m,model::med_wall_clock,18.6f,22.988f,f+2.3f,.24f,.024f,.23f,rfN,false);
 kit::prop(m,model::med_wheelchair,19.1f,22.2f,f,.55f,1.03f,.9f,.3f);
 rfFix(m,model::med_scrub_sink,20.2f,22.71f,f,.49f,.54f,1.13f,rfN,true);
 rfFix(m,model::med_supply_cabinet,22.76f,20.4f,f,1.1f,.44f,1.7f,rfW,true);
 // The exam bay: a couch behind a half-drawn curtain, the monitor still counting for nobody.
 m.pipes.push_back({{20.6f,21.4f},{23.f,21.4f},f+2.34f,.015f,f+2.34f,5});
 for(float x:{20.66f,21.9f})m.pipes.push_back({{x,21.4f},{x,21.4f},f+2.34f,.01f,roof,6});
 wall(m,21.7f,21.38f,23.f,21.42f,f+.3f,f+2.32f,39);m.structures.back().solid=false;
 wall(m,21.56f,21.33f,21.7f,21.47f,f+.3f,f+2.32f,39);m.structures.back().solid=false;
 rfFix(m,model::med_ward_bed,21.9f,22.45f,f,1.984f,.944f,.842f,0,true);
 rfFix(m,model::med_crash_cart,22.5f,21.7f,f,.736f,.483f,.999f,0,true);
 rfFix(m,model::med_vitals_monitor,22.45f,21.7f,f+.8082f,.38f,.18f,.28f,rfW,false);
 rfFix(m,model::item_dosimeter,22.74f,21.72f,f+.8082f,.31f,.146f,.066f,1.3f,false);   // a band, cut off and set down
 m.lights.push_back(lamp({18.2f,21.3f},roof-.16f,.4f,5.f,4000,.1f));
 m.lights.push_back(lamp({16.85f,19.95f},f+1.55f,.75f,3.4f,7500,.04f,WorldLightMount::Bare));   // the lightboxes
 m.lights.push_back(lamp({22.3f,21.72f},f+1.05f,.4f,2.4f,6000,.35f,WorldLightMount::Bare));     // the monitor's trace

 // --- Suit return (x 1-9.4, y 14.2-19.4): the lab shift came back through here at 02:06 -------------
 for(float y:{15.f,15.65f,16.3f})kit::prop(m,model::ind_barrel,1.4f,y,f,.53f,.56f,.95f,0);
 // The bags for Internal Investigations, boxed on a pallet by the sinks.
 rfFix(m,model::pallet,7.85f,18.62f,f,1.08f,1.2f,.16f,kPi*.5f,true);
 for(auto [x,y,z,yaw]:{std::tuple{7.6f,18.4f,0.f,.1f},{8.15f,18.45f,0.f,-.2f},{7.65f,18.95f,0.f,.25f},{8.1f,18.9f,.3f,.05f},{7.7f,18.5f,.3f,-.15f}})
  rfFix(m,model::prop_cardboard_box,x,y,f+.16f+z,.5f,.5f,.3f,yaw,false);
 taped(m,{2.2f,19.4f},f+1.45f,rfN,Readable::Note,"SUIT RETURN / 02:06","LAB SHIFT BACK UP IN SUITS. 23 BAGGED UNWASHED. TIER 2 HELD. 02:40 BAGS TO INTERNAL INVESTIGATIONS.",
  "FOUR MORE ON THE DRYING RAIL SINCE 03:10. NOBODY SIGNED THEM IN.");
 for(float y:{15.4f,16.2f})rfFix(m,model::med_scrub_sink,9.13f,y,f,.49f,.54f,1.13f,rfW,true);
 rfFix(m,model::bench,2.8f,14.47f,f,1.86f,.44f,.63f,0,true);
 rfFix(m,model::med_floor_drain,5.6f,16.8f,f,.5f,.5f,.01f,0,false);
 handCart(m,8.3f,15.3f,f);
 kit::prop(m,model::prop_cardboard_box_open,8.3f,15.1f,f+.29f,.55f,.55f,.55f,.4f);
 kit::prop(m,model::item_dosimeter,6.3f,17.9f,f,.31f,.146f,.066f,.9f);
 // The drying rail on the south wall: four suits on steel hangers, packed along it the
 // way a rail of coats hangs, high, their boots clear of the floor; one hanger bare.
 // They sway if you push into them.
 {const float railY=19.05f,railZ=f+2.42f,head=f+2.2f;
  m.pipes.push_back({{2.9f,railY},{6.9f,railY},railZ,.02f,railZ,5});
  for(float x:{2.9f,6.9f})m.pipes.push_back({{x,railY},{x,19.4f},railZ,.02f,railZ,5});          // its ends return into the wall
  {wall(m,4.88f,railY,4.92f,19.4f,railZ-.025f,railZ+.025f,2);m.structures.back().solid=false;}   // a centre bracket
  for(int i=0;i<5;++i){const float x=3.9f+i*.38f;
   wall(m,x-.006f,railY-.006f,x+.006f,railY+.006f,head-.2f,railZ,2);m.structures.back().solid=false;    // the hanger's hook
   wall(m,x-.012f,railY-.21f,x+.012f,railY+.21f,head-.215f,head-.2f,2);m.structures.back().solid=false;  // its shoulder bar
   if(i!=3)hungSuit(m,{x,railY,head},i==0?-.18f:i==2?.12f:i==4?-.08f:.05f);}}
 m.ambients.push_back({Sound::AmbWaterDrip,{5.6f,16.8f},f+.1f,.6f,8,AmbientEmitter::Loop});
 {auto l=rfiTube({4.6f,18.2f},roof-.16f,.95f,4.5f,9000,0,kPi*.5f);l.gel={.45f,.6f,1.f};m.lights.push_back(l);}   // the drying room's blue tube
 m.lights.push_back([&]{auto l=rfiTube({8.f,17.6f},roof-.16f,.6f,4.5f,4300,.25f);l.flicker=.35f;return l;}());
 m.lights.push_back(rfiTube({2.6f,16.6f},roof-.16f,.6f,4.5f,4300,.25f,kPi*.5f));

 // --- The wounded private ------------------------------------------------------------------------
 // At the end of the locker room aisle, gun down. A truce, his by default:
 // he only turns if the player turns on him.
 rfGuard(m,"rf_proc_clock","rf_proc_private",ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,"rf_proc",{{{1.9f,6.75f},f,60000,rfE}},10,.6f,2,.6f,0,60);
 // A truce keeps his pistol on him; it only falls if the player breaks it and downs him.
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("proc_hello");t.voice=2;t.ai.dropsSidearm=true;}
 // No clip sits, but a guard hurt past two thirds goes down on one knee and stays
 // there: the map starts him at 40 of his 60, so he is found kneeling in his own
 // blood with the pistol low, and one more hit finishes him.
 {ScriptEvent e;e.id=stateId("rf_proc_private_wound");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId("rf_proc_private_damage");a.value=40;e.actions={a};m.scriptEvents.push_back(e);}
 // What he has said shapes what can be asked: once he has named the Investigator
 // (rf_proc_heard) the question about orders goes and the way west is asked as where
 // that man is; once he has pointed west (rf_proc_hint) that question goes too.
 rfLine(m,"proc_hello","WOUNDED CLEANUP PRIVATE","Don't. I'm not going to shoot you. I can't lift it.",{
  rfReply("Who's giving your orders down here?","proc_who",{},-1,"","rf_proc_heard"),
  rfReply("Where is this Investigator now?","proc_admin",{},-1,"rf_proc_heard","rf_proc_hint"),
  rfReply("What happened to your squad?","proc_squad"),
  rfReply("Where's Administration?","proc_admin",{},-1,"","rf_proc_heard"),
  rfReply("I'll leave you be.")});
 rfLine(m,"proc_who","WOUNDED CLEANUP PRIVATE","The Investigator. Company man. Came down the service lift at three forty, and every radio's been his since. He doesn't count us. He counts what we've seen.",{
  rfReply("Back.","proc_hello",{{"rf_proc_heard",1}})});
 rfLine(m,"proc_squad","WOUNDED CLEANUP PRIVATE","Booth sweep at Medical. He said treat them as exposed. They were nurses. I stopped following orders about then. So did my leg.",{
  rfReply("Back.","proc_hello")});
 rfLine(m,"proc_admin","WOUNDED CLEANUP PRIVATE","West. The clearance desk's in there. He was in Records an hour ago, reading cards. Looking for somebody.",{
  rfReply("Thanks.","",{{"rf_proc_hint",1}})});
 // Walking in from Intake sets the truce.
 {ScriptEvent e;e.id=stateId("rf_proc_truce");e.x1=9.8f;e.y1=0;e.x2=14.2f;e.y2=4.f;e.bottom=-100;e.top=100;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stance;a.value=int(SquadStance::Truce);e.actions={a};m.scriptEvents.push_back(e);}

 // --- Rewards, throwables and names --------------------------------------------------------------
 m.pickupSpawns={{{1.5f,5.0f},PickupKind::Health,f},{{22.5f,3.f},PickupKind::Ammo,f},{{9.2f,3.1f},PickupKind::Health,f},{{21.f,18.6f},PickupKind::Ammo,f}};
 // 9 mm on the waiting benches, put down by the security that cleared the lockers.
 m.pickupSpawns.insert(m.pickupSpawns.end(),{{{8.95f,7.35f},PickupKind::HandgunAmmo,f+.63f},{{15.05f,8.45f},PickupKind::HandgunAmmo,f+.63f}});
 // One thing behind the curtain, one past the hung suits.
 m.pickupSpawns.insert(m.pickupSpawns.end(),{{{21.1f,21.62f},PickupKind::Health,f},{{1.9f,17.9f},PickupKind::HandgunAmmo,f}});
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{0,{7.3f,6.0f},f},{1,{18.2f,12.6f},f},{2,{20.6f,17.4f},f},{0,{4.2f,12.4f},f}});
 zone(m,"PERSONNEL PROCESSING / WASHROOM",1.f,1.f,9.8f,3.6f);
 zone(m,"PERSONNEL PROCESSING / LOCKER ROOM",1.f,3.8f,8.4f,8.8f);
 zone(m,"PERSONNEL PROCESSING / SUIT STORE",14.2f,1.f,23.f,3.6f);
 zone(m,"PERSONNEL PROCESSING / DRESS-OUT",15.6f,3.8f,23.f,8.8f);
 zone(m,"PERSONNEL PROCESSING / TIER OFFICE",14.6f,14.2f,23.f,19.2f);
 zone(m,"PERSONNEL PROCESSING / SCREENING",14.6f,19.4f,23.f,23.f);
 zone(m,"PERSONNEL PROCESSING / SUIT RETURN",1.f,14.2f,9.4f,19.4f);
 zone(m,"PERSONNEL PROCESSING / GATES",9.6f,13.4f,14.4f,23.f);
 zone(m,"PERSONNEL PROCESSING",0,0,24,24);
 recessedFittings(m,true);freightFinish(m,-1);
 return p;
}
// --- Research Administration kit (rfa): chunk 70's own helpers ---------------------------------
// A swinging leaf in a wall along x at y, hinged at x1; the wall run supplies the band over it.
void rfaDoor(Map& m,float x1,float x2,float y,float swing,StateId lock=0){
 Door d{x1,x2,y,0};d.swinging=true;d.swing=swing;d.requireState=lock;m.doors.push_back(d);
}
// A suspended ceiling of drop tiles with its underside at z, and its grid tees.
void rfaCeiling(Map& m,float x1,float y1,float x2,float y2,float z,bool grid=true){
 wall(m,x1,y1,x2,y2,z,z+.08f,22);if(grid)ceilingGrid(m,x1,y1,x2,y2,z);
}
// A low table: a wood top on four steel legs, `h` high, at floor z.
void rfaTable(Map& m,float cx,float cy,float hx,float hy,float z,float h){
 wall(m,cx-hx,cy-hy,cx+hx,cy+hy,z+h-.04f,z+h,10);
 for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f}){const float x=cx+sx*(hx-.07f),y=cy+sy*(hy-.07f);wall(m,x-.02f,y-.02f,x+.02f,y+.02f,z,z+h-.04f,2);}
}
// A leather boardroom chair whose sitter looks along `face` (the model's seat faces -y).
void rfaChair(Map& m,float x,float y,float z,float face){kit::prop(m,model::lab_boardroom_chair,x,y,z,.79f,.74f,.93f,face);}

// Something set down on a shelf, a ledge or a table top and never to be picked up:
// baked into the room as a fixture rather than left as a loose prop.
void rfaSet(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw){rfFix(m,model,x,y,z,w,d,h,yaw,false);}

// RESEARCH ADMINISTRATION (manifest 47). Where the company kept score: the
// executive floor of the research front. A two-storey atrium where visitors
// waited for laboratory clearance; the clearance counter under a gallery that
// leads to the director's suite; the boardroom and its copy room; and the
// personnel records office behind glass on the atrium's west wall, under the
// company's own slogan. The player walks in from Processing (E 9-14) on the
// Investigator himself: through that glass he reads the Energy Diagnostics
// cards, points his door man to the drawers, keeps one card (LAB 4's) and
// leaves by the far door to service lift 2. The door locks behind him; the
// records door opens. His detail stays: a heavy at the corridor mouth with
// his eyes on the glass and the lounge, and a man walking the atrium. The
// quiet way to the pass terminal is north, behind the counter.
std::shared_ptr<const Map> makeResearchAdministration(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[70];m.definition=CampaignChunks[70];m.definition.ambient=.30f;
 m.wallSurface=SurfacePaintedMaroon;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 // Each room its own finish: the atrium, gallery and lounge in the front's maroon-banded paint, the
 // records office, its corridor and the lift lobby in green-banded paint, the boardroom and the
 // director's suite in moulded panelling (panel); pale drop-tile ceilings, lit by sconces at wall height.
 const float f=-12,roof=m.definition.ceiling,dz=-9,cf=f+.015f,cu=dz+.015f;
 const int panel=28,plaster=21,steel=2,carpet=23,paint=38,green=37;
 const StateId gone=stateId("rf_admin_gone"),clearance=stateId("proc_clearance");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'E',9.f,14.f,f,false,0});
 machineryClock(m,"rf_admin_clock",60000);
 // The way in from Processing: steel posts and a panel head at 2.7 m, as on 69's side of the seam.
 for(float y:{9.f,13.86f})wall(m,22.88f,y,23.f,y+.14f,f,f+2.7f,steel);
 wall(m,22.88f,9.f,24.f,14.f,f+2.7f,roof,paint);wall(m,22.86f,9.f,23.02f,14.f,f+2.66f,f+2.7f,steel);

 // --- Plan ------------------------------------------------------------------------------------------
 // The upper floor (z -9): the director's suite over the boardroom and copy room, the gallery over the counter.
 deck(m,dz,0,0,12,8);deck(m,dz,12,0,24,5);
 m.stairs.push_back({21.f,5.f,23.f,8.8f,f,dz,16,true,false});
 // The boardroom / copy room south wall and the suite's are one run; the doors sit under 2.55 m.
 wall(m,1.f,7.8f,5.4f,8.f,f,f+2.55f,panel);wall(m,6.6f,7.8f,10.32f,8.f,f,f+2.55f,panel);wall(m,11.48f,7.8f,11.8f,8.f,f,f+2.55f,panel);
 wall(m,1.f,7.8f,11.8f,8.f,f+2.55f,roof,panel);
 rfaDoor(m,5.4f,6.6f,7.9f,-1);                                                   // 0: the boardroom
 rfpDoorX(m,10.4f,11.4f,7.8f,8.f,f,f+2.55f,panel);                             // the copy room, open
 // Records: the corridor door, the far door to the lift lobby, solid either side of the lobby.
 wall(m,1.f,10.f,9.6f,10.2f,f,f+2.55f,green);wall(m,10.8f,10.f,11.8f,10.2f,f,f+2.55f,green);wall(m,1.f,10.f,11.8f,10.2f,f+2.55f,roof,green);
 rfaDoor(m,9.6f,10.8f,10.1f,1,gone);                                              // 1: records, open once he has gone
 wall(m,1.f,19.f,4.4f,23.f,f,roof,green);wall(m,9.f,19.f,12.f,23.f,f,roof,green);
 wall(m,4.4f,19.f,6.2f,19.2f,f,f+2.55f,green);wall(m,7.4f,19.f,9.f,19.2f,f,f+2.55f,green);wall(m,4.4f,19.f,9.f,19.2f,f+2.55f,roof,green);
 rfaDoor(m,6.2f,7.4f,19.1f,1,stateId("rf_admin_far_door"));                     // 2: the far door: his, never the player's
 taped(m,{5.3f,19.f},f+1.5f,rfN,Readable::Note,"SERVICE LIFT 2","HELD AT THIS FLOOR FOR INTERNAL INVESTIGATIONS FROM 03:40.",
  "LOBBY DOOR ON THE INVESTIGATOR'S KEY ONLY. NO STAFF.");
 // The atrium's west face: copy room, the corridor mouth, the records glass, the lounge wall.
 wall(m,11.8f,1.f,12.f,8.f,f,dz,paint);
 wall(m,11.8f,8.f,12.f,10.f,f+2.7f,roof,paint);wall(m,11.78f,8.f,12.02f,10.f,f+2.66f,f+2.7f,steel);
 wall(m,11.8f,10.f,12.f,10.6f,f,roof,paint);
 // The records glass, and the enquiry hatch in it: two bays of steel bars over a counter,
 // glazed on the atrium side with the same protective glass, so a shot cracks it and stops.
 rfGlass(m,11.8f,10.6f,12.f,11.4f,f,roof,paint,.95f,2.4f,true);rfGlass(m,11.8f,13.8f,12.f,15.8f,f,roof,paint,.95f,2.4f,true);
 wall(m,11.8f,11.4f,12.f,13.8f,f,f+1.f,paint);wall(m,11.5f,11.4f,12.3f,13.8f,f+1.f,f+1.05f,12);
 wall(m,11.8f,11.4f,12.f,13.8f,f+2.05f,roof,paint);wall(m,11.78f,11.4f,12.02f,13.8f,f+2.01f,f+2.05f,steel);
 wall(m,11.8f,12.575f,12.f,12.625f,f+1.05f,f+2.01f,steel);
 for(float y=11.5f;y<13.75f;y+=.1f)if(std::fabs(y-12.6f)>.06f)wall(m,11.89f,y-.01f,11.91f,y+.01f,f+1.05f,f+2.01f,steel);
 kit::glazingPanes(m,11.95f,11.4f,11.97f,13.8f,f+1.05f,f+2.01f,17,true);
 wall(m,11.8f,15.8f,12.f,19.f,f,roof,paint);
 // Upstairs: the suite's door off the gallery and its window over the atrium; the director's door.
 wall(m,11.8f,1.f,12.f,1.6f,dz,roof,paint);rfiDoorY(m,1.6f,2.8f,11.8f,12.f,dz,roof,paint);wall(m,11.8f,2.8f,12.f,5.f,dz,roof,paint);
 rfGlass(m,11.8f,5.f,12.f,7.8f,dz,roof,paint,.95f,2.4f,true);wall(m,11.8f,7.8f,12.f,8.f,dz,roof,paint);
 rfiWallY(m,8.6f,8.8f,1.f,7.8f,{{2.f,3.f}},f,f+2.78f,panel);                  // boardroom / copy room: the service door
 rfiWallY(m,8.6f,8.8f,1.f,7.8f,{{1.4f,2.6f}},dz,roof,panel);                  // anteroom / director
 // Linings, 8 mm: each room's finish carried over the chunk's own walls and the far faces of its partitions.
 // The boardroom and the director's suite: moulded panelling to a timber chair rail, paint above it.
 auto wainscot=[&](float x1,float y1,float x2,float y2,float lo,float hi){const bool alongX=x2-x1>y2-y1;
  wall(m,x1,y1,x2,y2,lo,lo+1.1f,panel);wall(m,x1,y1,x2,y2,lo+1.1f,hi,paint);
  wall(m,x1-(alongX?0:.012f),y1-(alongX?.012f:0),x2+(alongX?0:.012f),y2+(alongX?.012f:0),lo+1.08f,lo+1.13f,10);};
 for(auto [a,b]:{std::pair{Vec2{1.f,1.f},Vec2{1.008f,7.8f}},{Vec2{1.008f,1.f},Vec2{8.6f,1.008f}},{Vec2{1.008f,7.792f},Vec2{5.4f,7.8f}},{Vec2{6.6f,7.792f},Vec2{8.6f,7.8f}},
   {Vec2{8.592f,1.008f},Vec2{8.6f,1.92f}},{Vec2{8.592f,3.08f},Vec2{8.6f,7.792f}}})wainscot(a.x,a.y,b.x,b.y,f,f+2.7f);                              // boardroom
 for(auto [a,b]:{std::pair{Vec2{1.f,1.f},Vec2{1.008f,7.8f}},{Vec2{1.008f,1.f},Vec2{11.792f,1.008f}},{Vec2{1.008f,7.792f},Vec2{8.6f,7.8f}},{Vec2{8.8f,7.792f},Vec2{11.792f,7.8f}},
   {Vec2{8.592f,1.008f},Vec2{8.6f,1.32f}},{Vec2{8.592f,2.68f},Vec2{8.6f,7.792f}},{Vec2{8.8f,1.008f},Vec2{8.808f,1.32f}},{Vec2{8.8f,2.68f},Vec2{8.808f,7.792f}},
   {Vec2{11.792f,1.008f},Vec2{11.8f,1.52f}},{Vec2{11.792f,2.88f},Vec2{11.8f,5.f}}})wainscot(a.x,a.y,b.x,b.y,dz,roof);                               // director and anteroom
 for(auto [x1,y1,x2,y2,lo,hi,mat]:{std::tuple{1.008f,8.f,5.4f,8.008f,f,f+2.7f,green},{6.6f,8.f,10.32f,8.008f,f,f+2.7f,green},{11.48f,8.f,11.8f,8.008f,f,f+2.7f,green},{5.4f,8.f,6.6f,8.008f,f+2.55f,f+2.7f,green},
   {1.f,8.f,1.008f,10.f,f,f+2.7f,green},                                                                                                   // corridor
   {1.f,10.2f,1.008f,10.5f,f,f+2.9f,green},{1.f,10.5f,1.008f,18.5f,f+1.31f,f+2.9f,green},{1.f,18.5f,1.008f,19.f,f,f+2.9f,green},
   {11.792f,10.2f,11.8f,10.6f,f,f+2.9f,green},{11.792f,15.8f,11.8f,19.f,f,f+2.9f,green}})                                                 // records
  wall(m,x1,y1,x2,y2,lo,hi,mat);
 // Ceilings: drop tiles at 2.7 m under the upper floor and in the back rooms; 2.9 m in records and the lounge.
 rfaCeiling(m,1.f,1.f,8.6f,7.8f,f+2.7f);rfaCeiling(m,8.8f,1.f,11.8f,7.8f,f+2.7f,false);
 rfaCeiling(m,1.f,8.f,11.8f,10.f,f+2.7f,false);
 rfaCeiling(m,1.f,10.2f,11.8f,19.f,f+2.9f);
 rfaCeiling(m,4.4f,19.2f,9.f,23.f,f+2.7f,false);
 rfaCeiling(m,12.f,1.f,23.f,5.f,f+2.7f,false);
 wall(m,12.f,4.95f,21.f,5.08f,f+2.55f,dz,paint);                                 // the gallery fascia
 for(float x:{13.8f,17.8f})wall(m,x,4.6f,x+.4f,5.f,f,f+2.55f,paint);               // and its columns
 wall(m,12.f,16.f,23.f,16.2f,f+2.9f,roof,paint);wall(m,12.f,15.98f,23.f,16.22f,f+2.86f,f+2.9f,steel);   // the lounge bulkhead
 for(float x:{15.8f,19.4f})wall(m,x,15.9f,x+.4f,16.3f,f,f+2.86f,paint);
 rfaCeiling(m,12.f,16.2f,23.f,23.f,f+2.9f);
 // The gallery's glass balustrade.
 kit::breakable(m,12.f,4.86f,21.f,4.92f,dz,dz+1.f,17,true);wall(m,12.f,4.84f,21.f,4.94f,dz+1.f,dz+1.05f,20);
 for(float x=12.f;x<21.1f;x+=1.5f){const float a=std::min(x,20.95f);wall(m,a,4.84f,a+.05f,4.94f,dz,dz+1.f,20);}
 // Carpet where the company sat: boardroom, lounge, the suite and the gallery.
 floorLine(m,1.f,1.f,8.6f,7.8f,f,carpet);floorLine(m,12.f,16.2f,23.f,23.f,f,carpet);
 floorLine(m,1.f,1.f,11.8f,7.8f,dz,carpet);floorLine(m,12.f,1.f,23.f,4.84f,dz,carpet);

 // --- The atrium (x 12-23, y 5-16, 6 m) ----------------------------------------------------------
 rfSign(m,12.02f,13.2f,f+3.25f,"DEPTHWORKS ENERGY RESEARCH","YOUR WORK POWERS THE WORLD",4.8f,rfE,rfBlue);
 for(float y:{8.25f,14.f})wall(m,22.88f,y,23.f,y+.75f,f+2.7f,roof,paint);      // the head's face, carried out under the long plate
 rfSign(m,22.86f,11.5f,f+3.35f,"1,204 DAYS, NO LOST-TIME INCIDENT","SCREENING KEEPS US WORKING",5.6f,rfW,rfBlue);
 for(Vec2 l:{Vec2{14.6f,8.4f},Vec2{19.4f,8.4f},Vec2{14.6f,13.4f},Vec2{19.4f,13.4f}})m.lights.push_back(lamp(l,roof-.2f,1.15f,9.f,5600,.04f));
 // Visitors waited here for the counter to call them.
 for(float x:{14.8f,19.2f})rfFix(m,model::bench,x,7.8f,f,1.86f,.44f,.63f,rfN,true);
 // The case the company was proud of: the first core out of the deep seam, lit under glass,
 // beside the records glass and off the line from the way in, so the hatch stays the stage.
 {const float cx=16.4f,cy=14.5f;
  wall(m,cx-.45f,cy-.45f,cx+.45f,cy+.45f,f,f+.95f,panel);wall(m,cx-.48f,cy-.48f,cx+.48f,cy+.48f,f,f+.08f,steel);
  wall(m,cx-.47f,cy-.47f,cx+.47f,cy+.47f,f+.95f,f+.98f,steel);
  for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f})wall(m,cx+sx*.42f-.02f,cy+sy*.42f-.02f,cx+sx*.42f+.02f,cy+sy*.42f+.02f,f+.98f,f+1.9f,steel);
  wall(m,cx-.47f,cy-.47f,cx+.47f,cy+.47f,f+1.9f,f+1.96f,steel);wall(m,cx-.18f,cy-.03f,cx+.18f,cy+.03f,f+1.875f,f+1.9f,20);
  rfFix(m,model::lab_anomaly_relic,cx,cy,f+.98f,.55f,.44f,.44f,.6f,false);
  m.lights.push_back(lamp({cx,cy},f+1.8f,1.4f,2.8f,6800,-.15f,WorldLightMount::Bare));
  for(float s:{-1.4f,1.32f}){floorLine(m,cx-1.4f,cy+s,cx+1.4f,cy+s+.08f,f,20);floorLine(m,cx+s,cy-1.4f,cx+s+.08f,cy+1.4f,f,20);}}
 for(auto [x,y]:{std::pair{12.55f,7.4f},{20.45f,5.6f},{20.4f,15.4f}})kit::prop(m,model::lab_potted_plant,x,y,f,.555f,.645f,1.035f,x);
 // The first boxes carried out of records, waiting by the glass for the lift.
 kit::prop(m,model::prop_cardboard_box,12.55f,14.7f,f,.727f,.72f,.367f,.1f);
 kit::prop(m,model::prop_cardboard_box,12.56f,15.55f,f,.727f,.72f,.367f,-.08f);
 kit::prop(m,model::prop_cardboard_box,12.55f,14.7f,f+.367f,.727f,.72f,.367f,.1f);
 handCart(m,13.4f,15.f,f);
 kit::prop(m,model::prop_cardboard_box,13.4f,14.78f,f+.29f,.727f,.72f,.367f,.05f);
 kit::prop(m,model::prop_cardboard_box,13.4f,14.78f,f+.657f,.727f,.72f,.367f,.05f);
 kit::prop(m,model::trash_can,20.6f,14.5f,f,.5f,.5f,.62f,.4f);
 paperDrift(m,13.f,8.6f,20.f,15.2f,f,5,704);
 rfFix(m,model::med_wall_clock,17.6f,15.96f,f+3.5f,.756f,.064f,.746f,kPi,false);    // the atrium clock, on the bulkhead
 // The company livery under it: an ochre band between pale pinstripes, the length of the bulkhead.
 wall(m,12.f,15.94f,23.f,16.f,f+3.1f,f+3.34f,14);for(float z:{3.03f,3.37f})wall(m,12.f,15.95f,23.f,16.f,f+z,f+z+.04f,20);

 // --- The clearance counter, under the gallery (x 12-23, y 1-5) ----------------------------------
 wall(m,13.4f,3.9f,19.6f,4.2f,f,f+1.05f,plaster);wall(m,13.4f,2.6f,13.7f,3.9f,f,f+1.05f,plaster);
 wall(m,13.35f,3.85f,19.65f,4.3f,f+1.05f,f+1.1f,12);wall(m,13.35f,2.55f,13.75f,3.85f,f+1.05f,f+1.1f,12);
 wall(m,13.4f,4.2f,19.6f,4.22f,f,f+.1f,8);
 rfDesk(m,15.4f,2.6f,f,rfS,1);rfDesk(m,17.9f,2.6f,f,rfS,2);
 for(float x:{13.f,19.7f}){rfFix(m,model::file_cabinet_wide,x,1.25f,f,.9f,.5f,.75f,rfS,true);}
 kit::prop(m,model::cctv_tv,13.f,1.22f,f+.75f,.5f,.4f,.42f,rfS);                 // the atrium camera, on the counter's monitor
 rfFix(m,model::med_wall_clock,16.65f,1.016f,f+2.05f,.378f,.032f,.373f,0,false);
 const float ledge=f+1.1f;
 rfaSet(m,model::office_paper_stack,15.1f,4.05f,ledge,.329f,.443f,.03f,1.5f);     // the visitors' book
 kit::prop(m,model::item_walkie,18.7f,4.05f,ledge,.114f,.376f,.046f,rfE);              // the detail's radio, left on the ledge
 for(auto [x,yaw]:{std::pair{16.4f,.4f},{16.62f,-.3f},{17.f,1.2f}})rfaSet(m,model::item_id_card,x,4.08f,ledge,.132f,.085f,.002f,yaw);
 // The pass terminal, staff side: it issues proc_clearance once the floor is quiet again.
 // Its log has to read true both ways: held while he is on the floor, issuing once he has gone.
 terminal(m,{21.6f,1.45f},"CLEARANCE DESK / LABORATORY ACCESS","ISSUES LABORATORY CLEARANCE TO THE BAND PRESENTED. THE PROCESSING DOOR SOUTH READS IT.",
  "03:41 II: DESK HELD WHILE THE INVESTIGATOR IS ON THIS FLOOR. RELEASES WHEN HE SIGNS OUT.",0,clearance,gone);m.terminals.back().yaw=rfS;
 kit::prop(m,model::trash_can,14.1f,1.35f,f,.5f,.5f,.62f,0);
 for(Vec2 l:{Vec2{14.8f,3.f},Vec2{21.8f,3.f}})m.lights.push_back(lamp(l,f+2.5f,.85f,5.f,5200,.04f));
 m.lights.push_back(lamp({18.6f,1.f},f+2.f,.9f,5.f,4200,.04f,WorldLightMount::Wall,kPi*.5f));    // a sconce over the clerks: the soffit and the wall read

 // --- The lounge (x 12-23, y 16-23) -----------------------------------------------------------------
 rfFix(m,model::office_couch_green,14.4f,22.3f,cf,2.242f,1.364f,1.192f,rfN,true);
 kit::prop(m,model::office_armchair_green,12.95f,20.f,cf,1.399f,1.364f,1.192f,rfE+.25f);
 rfFix(m,model::office_couch_green,20.3f,22.3f,cf,2.242f,1.364f,1.192f,rfN,true);
 kit::prop(m,model::office_armchair_green,22.28f,20.f,cf,1.399f,1.364f,1.192f,rfW);
 for(float x:{14.6f,20.3f})rfaTable(m,x,20.2f,.6f,.3f,cf,.42f);
 for(auto [x,y,yaw]:{std::tuple{14.4f,20.2f,0.f},{20.5f,20.2f,-.4f}})kit::prop(m,model::office_paper_stack,x,y,cf+.42f,.329f,.443f,.03f,yaw);
 kit::prop(m,model::mug,14.9f,20.3f,cf+.42f,.09f,.09f,.1f,0);
 rfFix(m,model::metro_vending,17.35f,22.69f,cf,1.2f,.544f,1.904f,0,true);         // front -(sin,cos) yaw: faces north
 rfFix(m,model::water_cooler,12.25f,17.1f,cf,.35f,.35f,1.1f,rfE,true);
 wall(m,22.4f,16.6f,23.f,18.4f,cf,cf+.88f,plaster);wall(m,22.38f,16.58f,23.f,18.42f,cf+.88f,cf+.92f,10);   // the coffee station
 kit::prop(m,model::coffee_machine,22.72f,17.1f,cf+.92f,.4f,.45f,.5f,rfW);
 for(auto [y,yaw]:{std::pair{17.75f,0.f},{17.95f,.8f}})rfaSet(m,model::mug,22.65f,y,cf+.92f,.09f,.09f,.1f,yaw);
 rfFix(m,model::lab_potted_plant,12.5f,22.55f,cf,.37f,.43f,.69f,0,true);rfFix(m,model::lab_potted_plant,22.4f,22.45f,cf,.37f,.43f,.69f,1,true);
 kit::prop(m,model::trash_can,18.65f,22.6f,cf,.5f,.5f,.62f,0);
 paperDrift(m,15.6f,17.f,19.f,21.2f,cf,4,705);
 m.lights.push_back(lamp({19.8f,19.2f},f+2.7f,.75f,6.f,4200,.08f));
 // Warm sconces either side: the lounge's ceiling and walls lit, not just its carpet.
 m.lights.push_back(lamp({12.f,21.f},f+2.f,.8f,5.f,3000,.04f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({23.f,21.4f},f+2.f,.8f,5.f,3000,.04f,WorldLightMount::Wall,kPi));

 // --- The boardroom: the quarter's numbers, and a meeting nobody closed ------------------------------
 {const float tz=cf+.74f;
  wall(m,2.9f,3.35f,7.3f,4.65f,cf+.7f,tz,10);
  for(float x:{3.8f,6.4f})wall(m,x-.35f,3.75f,x+.35f,4.25f,cf,cf+.7f,plaster);
  int i=0;for(float x:{3.6f,4.7f,5.8f,6.9f}){
   rfaChair(m,x+(i%2?.1f:-.05f),2.85f-(i==2?.3f:0),cf,rfS+(i==1?.3f:0));
   rfaChair(m,x,5.15f+(i==3?.4f:0),cf,rfN-(i==0?.25f:0));++i;}
  rfaChair(m,2.25f,4.f,cf,rfE);
  for(auto [x,y,yaw]:{std::tuple{3.6f,3.62f,.1f},{4.7f,3.6f,-.15f},{5.8f,4.4f,3.f},{6.9f,4.38f,3.3f},{4.7f,4.35f,2.9f}})rfaSet(m,model::office_paper_stack,x,y,tz,.329f,.443f,.03f,yaw);
  for(auto [x,y]:{std::pair{5.85f,3.62f},{3.75f,4.45f},{6.95f,3.6f}})kit::prop(m,model::mug,x,y,tz,.09f,.09f,.1f,0);
  kit::prop(m,model::desk_phone,5.2f,4.f,tz,.2f,.2f,.1f,rfE);
  terminal(m,{3.4f,4.f},"BOARD MINUTES / Q3","QUOTA UP 18 PERCENT. SCREENING INTERVAL EXTENDED FROM 30 TO 90 DAYS. MOTION CARRIED.",
   "EXPOSURE TIER 2 RECLASSIFIED: FIT FOR SHIFT. TIER 3 TO OBSERVATION AT MEDICAL'S DISCRETION.");
  m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;m.terminals.back().surface=.755f;}
 wall(m,3.4f,1.f,6.8f,1.5f,cf,cf+.72f,plaster);wall(m,3.38f,1.f,6.82f,1.52f,cf+.72f,cf+.76f,10);   // the credenza
 kit::prop(m,model::cctv_tv,5.1f,1.26f,cf+.76f,.5f,.4f,.42f,rfS);
 rfaSet(m,model::lab_potted_plant,3.85f,1.25f,cf+.76f,.37f,.43f,.69f,0);
 rfFix(m,model::office_bookcase,1.27f,4.f,cf,2.025f,.5f,2.5f,rfE,true);
 // The quarter's binders, shelf by shelf (shelf tops at .03, .656, 1.335, 1.917 m), with the gaps of a working room.
 {const float sh[4]={.03f,.656f,1.335f,1.917f};
  const int pat[4][19]={{1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,0,1,1,1},{1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,0,0},
   {0,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1},{1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,0}};
  for(int s=0;s<4;++s)for(int i=0;i<19;++i)if(pat[s][i])
   rfaSet(m,model::office_file_binder,1.3f+((i*7+s)%3)*.012f,4.f-.9f+i*.1f,cf+sh[s],.093f,.301f,.424f,rfE+((i*5+s*3)%7-3)*.006f);
  rfaSet(m,model::office_file_binder,1.31f,4.f-.9f+17.6f*.1f,cf+sh[1],.093f,.301f,.424f,rfE+.45f);}   // one turned in the gap
 rfFix(m,model::med_wall_clock,8.584f,4.f,f+2.f,.378f,.032f,.373f,rfW,false);
 rfFix(m,model::lab_potted_plant,1.5f,7.3f,cf,.37f,.43f,.69f,0,true);
 m.lights.push_back(lamp({5.1f,4.f},f+2.5f,.85f,5.5f,4200,.08f));
 for(float x:{2.4f,7.7f})m.lights.push_back(lamp({x,1.f},f+2.f,.75f,4.5f,3000,.04f,WorldLightMount::Wall,kPi*.5f));   // sconces either side of the credenza

 // --- The copy room: the board's papers, copied, boxed and sealed -----------------------------------
 rfFix(m,model::lab_photocopier,10.3f,1.43f,f,.826f,.752f,.967f,rfN,true);
 shelf(m,{11.55f,3.4f},rfW);
 for(float y:{2.95f,3.85f})rfaSet(m,model::prop_cardboard_box,11.55f,y,f+1.65f,.45f,.45f,.23f,rfW+.1f);
 rfaSet(m,model::prop_cardboard_box_open,11.55f,3.4f,f+.972f,.45f,.45f,.45f,rfW);
 for(int i=0;i<5;++i)rfaSet(m,model::office_file_binder,11.6f,2.75f+i*.12f,f+.306f,.093f,.301f,.424f,rfW+(i==2?.25f:0));
 wall(m,11.2f,5.f,11.8f,7.f,f,f+.88f,plaster);wall(m,11.18f,4.98f,11.8f,7.02f,f+.88f,f+.92f,10);
 kit::prop(m,model::coffee_machine,11.52f,5.5f,f+.92f,.4f,.45f,.5f,rfW);
 for(auto [y,yaw]:{std::pair{6.15f,0.f},{6.4f,1.f}})rfaSet(m,model::mug,11.45f,y,f+.92f,.09f,.09f,.1f,yaw);
 rfFix(m,model::water_cooler,9.2f,7.42f,f,.35f,.35f,1.1f,rfN,true);
 rfFix(m,model::wooden_table,9.9f,4.4f,f,1.367f,.724f,.78f,rfE,true);
 kit::prop(m,model::prop_cardboard_box_open,9.9f,4.f,f+.78f,.45f,.45f,.45f,.15f);
 rfaSet(m,model::office_paper_stack,9.85f,4.65f,f+.78f,.329f,.443f,.03f,.2f);
 rfaSet(m,model::office_paper_stack,9.85f,4.65f,f+.81f,.329f,.443f,.03f,.2f);
 kit::prop(m,model::prop_cardboard_box,9.3f,6.2f,f,.727f,.72f,.367f,.1f);
 kit::prop(m,model::prop_cardboard_box,9.3f,6.2f,f+.367f,.727f,.72f,.367f,.1f);
 paperDrift(m,9.2f,1.9f,11.f,7.f,f,4,706);
 m.lights.push_back(lamp({10.3f,4.4f},f+2.5f,.7f,4.5f,5000,.05f));

 // --- The corridor (y 8-10): what the detail bagged, waiting for the lift ----------------------------
 rfFix(m,model::med_notice_board,3.f,8.035f,f+1.f,1.2f,.07f,.9f,0,false);
 extinguisher(m,1.f,9.5f,f,rfE);
 rfFix(m,model::pwr_fire_cabinet,1.1f,8.6f,f+1.3f,.25f,.2f,.56f,rfE,false);
 kit::prop(m,model::prop_cardboard_box,7.9f,9.3f,f,.727f,.72f,.367f,.05f);
 kit::prop(m,model::prop_cardboard_box,8.75f,9.3f,f,.727f,.72f,.367f,-.1f);
 kit::prop(m,model::prop_cardboard_box,7.9f,9.3f,f+.367f,.727f,.72f,.367f,.05f);
 paperDrift(m,1.5f,8.3f,7.3f,9.8f,f,4,707,false);
 for(Vec2 l:{Vec2{3.4f,9.f},Vec2{8.f,9.f}})m.lights.push_back(lamp(l,f+2.5f,.6f,4.5f,4800,.05f));

 // --- The records office (x 1-11.8, y 10.2-19) ------------------------------------------------------
 // The personnel cabinets: a bank on the west wall and an island, both faced into their aisles.
 for(int i=0;i<16;++i)rfFix(m,model::file_cabinet_tall,1.3f,10.75f+i*.5f,f,.5f,.6f,1.3f,rfE,true);
 for(int i=0;i<11;++i){const float y=12.25f+i*.5f;rfFix(m,model::file_cabinet_tall,3.9f,y,f,.5f,.6f,1.3f,rfW,true);rfFix(m,model::file_cabinet_tall,4.5f,y,f,.5f,.6f,1.3f,rfE,true);}
 for(auto [x,y,yaw]:{std::tuple{1.3f,12.3f,.1f},{1.3f,15.8f,-.2f},{4.2f,13.1f,.3f},{4.2f,16.4f,0.f}})rfaSet(m,model::prop_cardboard_box,x,y,f+1.3f,.45f,.45f,.23f,yaw);
 // The card index on the north wall: where LAB 4's line still is, and her card is not.
 for(float x:{2.2f,3.15f})rfFix(m,model::file_cabinet_wide,x,10.45f,f,.9f,.5f,.75f,rfS,true);
 terminal(m,{3.15f,10.5f},"PERSONNEL CARD INDEX / ENERGY DIAGNOSTICS","LAB 4: ANALYST, ENERGY DIAGNOSTICS. EXPOSURE TIER 2. SCREENING DUE.",
  "CARD SIGNED OUT 04:02 BY INTERNAL INVESTIGATIONS. REASON: CONTAINMENT.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfS;m.terminals.back().surface=.75f;
 rfFix(m,model::med_notice_board,2.68f,10.235f,f+1.15f,1.2f,.07f,.9f,0,false);     // the index clerk's board over it
 // The drawers he had pulled, and the cards he set on top of them.
 for(float x:{6.95f,8.45f})rfFix(m,model::office_file_cabinet_open,x,10.55f,f,1.451f,.699f,1.054f,rfS,true);
 rfaSet(m,model::office_paper_stack,7.35f,10.5f,f+1.054f,.329f,.443f,.03f,1.7f);
 for(auto [x,yaw]:{std::pair{7.85f,.3f},{7.95f,1.4f},{8.15f,2.2f}})rfaSet(m,model::item_id_card,x,10.6f,f+1.054f,.132f,.085f,.002f,yaw);
 // The collating table under the glass: the cards he went through.
 for(float y:{13.2f,14.57f})rfFix(m,model::wooden_table,10.f,y,f,1.367f,.724f,.78f,rfE,true);
 {const float tt=f+.78f;
  kit::prop(m,model::prop_cardboard_box_open,10.05f,12.95f,tt,.45f,.45f,.45f,.2f);
  kit::prop(m,model::prop_cardboard_box_open,9.95f,14.95f,tt,.45f,.45f,.45f,-.15f);
  for(auto [x,y,yaw]:{std::tuple{9.8f,13.5f,.3f},{9.95f,13.62f,1.9f},{10.2f,13.55f,.8f},{9.75f,13.78f,2.6f},{10.1f,13.9f,1.1f},{9.9f,14.15f,.2f},{10.25f,14.3f,2.f},{9.8f,14.45f,.6f}})
   rfaSet(m,model::item_id_card,x,y,tt,.132f,.085f,.002f,yaw);
  rfaSet(m,model::office_paper_stack,10.15f,14.05f,tt,.329f,.443f,.03f,.1f);
  kit::prop(m,model::item_walkie,9.95f,12.65f,tt,.114f,.376f,.046f,rfE);}
 rfDesk(m,10.2f,17.6f,f,rfW,0);
 kit::prop(m,model::lab_desk_lamp,10.35f,17.95f,f+.75f,.18f,.398f,.521f,0);
 // Boxed for the lift, and what fell on the way.
 handCart(m,10.15f,16.05f,f);
 kit::prop(m,model::prop_cardboard_box,10.15f,15.85f,f+.29f,.727f,.72f,.367f,.08f);
 kit::prop(m,model::prop_cardboard_box_open,9.3f,16.6f,f,.45f,.45f,.45f,.9f);
 kit::prop(m,model::office_paper_stack,9.15f,16.1f,f,.329f,.443f,.03f,2.3f);
 kit::prop(m,model::prop_cardboard_box,8.4f,18.5f,f,.727f,.72f,.367f,.1f);
 kit::prop(m,model::prop_cardboard_box,8.4f,18.5f,f+.367f,.727f,.72f,.367f,.1f);
 kit::prop(m,model::prop_cardboard_box_open,5.55f,12.4f,f,.73f,.725f,.726f,.3f);
 kit::prop(m,model::prop_cardboard_box_open,5.5f,16.6f,f,.73f,.725f,.726f,-.2f);
 for(auto [x,y,yaw]:{std::tuple{5.1f,13.f,.3f},{6.3f,13.6f,1.9f},{2.4f,13.9f,.8f},{6.f,17.6f,2.6f},{8.3f,15.2f,1.1f},{2.6f,17.1f,.2f},{6.8f,12.4f,2.f},{8.9f,16.2f,.6f},{5.8f,15.1f,1.4f}})
  rfaSet(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw);
 paperDrift(m,5.2f,11.2f,9.2f,18.6f,f,8,702);
 // Lit for the glass: what happens in here must read from the atrium.
 for(Vec2 l:{Vec2{7.6f,12.f},Vec2{10.f,13.2f},Vec2{7.6f,16.8f}})m.lights.push_back(lamp(l,f+2.7f,1.6f,7.f,4300,.06f));
 // Over the cabinet run, two lamps at wall height: the cabinet fronts and the ceiling read.
 for(float y:{12.6f,16.6f})m.lights.push_back(lamp({1.f,y},f+2.15f,.9f,5.5f,4300,.05f,WorldLightMount::Wall,0));
 // His black coat goes dark against the cabinets under plain overheads: two cool
 // lights inside the glass catch the side of him the atrium sees, at the drawers and the table.
 for(Vec2 l:{Vec2{9.3f,12.1f},Vec2{10.7f,14.6f}})m.lights.push_back(lamp(l,f+2.7f,1.1f,4.f,5200,.04f));
 m.lights.push_back(lamp({10.32f,17.9f},f+1.15f,.6f,3.f,2800,.15f,WorldLightMount::Bare));   // under the desk lamp's shade
 // The lift lobby behind the far door: service lift 2, the way he came in at 03:40.
 for(float x:{5.8f,6.8f})wall(m,x+.01f,22.88f,x+.99f,23.f,f,f+2.2f,steel);
 wall(m,5.6f,22.84f,8.f,23.f,f+2.2f,f+2.32f,12);for(float x:{5.6f,7.88f})wall(m,x,22.84f,x+.12f,23.f,f,f+2.2f,12);
 rfFix(m,model::lab_id_scanner,8.35f,22.97f,f+1.05f,.3f,.06f,.41f,rfS,false);
 rfFix(m,model::lab_status_display,6.8f,22.81f,f+2.4f,.693f,.058f,.2f,rfS,false);
 // The bagged records waiting on the cart for the lift to come back for them.
 handCart(m,8.35f,20.9f,f);
 for(float z:{.29f,.657f})kit::prop(m,model::prop_cardboard_box,8.35f,20.75f,f+z,.727f,.72f,.367f,.06f);
 for(float z:{0.f,.367f})kit::prop(m,model::prop_cardboard_box,5.f,22.4f,f+z,.727f,.72f,.367f,.2f);
 m.lights.push_back(lamp({6.8f,20.8f},f+2.5f,.6f,4.f,5200,.05f));

 // --- Upstairs: the gallery over the counter -------------------------------------------------------
 for(float x:{14.6f,18.4f})rfFix(m,model::bench,x,3.2f,cu,1.86f,.44f,.63f,0,true);
 // The site model visitors were shown: the plant from the surface down, under glass.
 wall(m,15.7f,1.3f,17.3f,2.2f,cu,cu+.85f,panel);wall(m,15.72f,1.32f,17.28f,2.18f,cu+.85f,cu+.88f,15);
 // Pale card buildings on the dark base: the mill, the winder house, the headframe over its shaft collar, a stack.
 wall(m,15.9f,1.5f,16.35f,1.82f,cu+.88f,cu+1.f,28);for(float y:{1.42f,1.95f})floorLine(m,15.75f,y,17.25f,y+.03f,cu+.88f,14);wall(m,16.5f,1.45f,16.95f,1.72f,cu+.88f,cu+.96f,28);
 wall(m,15.88f,1.48f,16.37f,1.84f,cu+1.f,cu+1.02f,28);
 wall(m,16.53f,1.79f,16.77f,1.99f,cu+.88f,cu+.9f,20);
 for(float x:{16.57f,16.69f})wall(m,x,1.87f,x+.04f,1.91f,cu+.9f,cu+1.2f,18);wall(m,16.55f,1.86f,16.75f,1.92f,cu+1.18f,cu+1.22f,18);
 wall(m,16.02f,1.58f,16.1f,1.66f,cu+1.02f,cu+1.3f,28);
 wall(m,17.f,1.5f,17.14f,2.f,cu+.88f,cu+.95f,28);
 for(float y:{1.3f,2.16f})wall(m,15.7f,y,17.3f,y+.04f,cu+.85f,cu+.89f,steel);
 rfFix(m,model::lab_potted_plant,12.5f,4.3f,cu,.37f,.43f,.69f,0,true);rfFix(m,model::lab_potted_plant,20.5f,1.45f,cu,.37f,.43f,.69f,2,true);
 rfFix(m,model::med_wall_clock,18.4f,1.016f,dz+2.1f,.378f,.032f,.373f,0,false);
 wall(m,12.f,1.f,23.f,1.06f,dz+2.55f,dz+2.79f,14);for(float z:{2.48f,2.82f})wall(m,12.f,1.f,23.f,1.05f,dz+z,dz+z+.04f,20);   // the livery, carried round
 for(Vec2 l:{Vec2{15.f,2.8f},Vec2{19.6f,2.8f}})m.lights.push_back(lamp(l,roof-.2f,.8f,6.f,5000,.05f));
 // The anteroom: the director's secretary, who saw everyone in.
 rfDesk(m,10.3f,3.6f,cu,rfE,3);
 for(float x:{9.95f,10.85f})rfaChair(m,x,6.95f,cu,rfN+(x-10.3f)*.25f);   // waiting, facing her desk
 rfFix(m,model::file_cabinet_tall,9.1f,4.6f,cu,.5f,.6f,1.3f,rfE,true);
 rfFix(m,model::lab_potted_plant,9.25f,7.35f,cu,.37f,.43f,.69f,0,true);
 m.lights.push_back(lamp({10.3f,4.4f},roof-.2f,1.05f,5.5f,4600,.06f));
 // The director's office: swept by the detail before he was.
 rfFix(m,model::lab_exec_desk,3.6f,3.2f,cu,2.21f,1.1f,.89f,rfW,true);
 rfaChair(m,2.5f,3.2f,cu,rfE);
 rfFix(m,model::office_bookcase,1.27f,3.2f,cu,2.025f,.5f,2.5f,rfE,true);
 // Swept: the detail took the files and left the shelves bare in bites, and the rest on the floor.
 {const float sh[4]={.03f,.656f,1.335f,1.917f};
  const int pat[4][19]={{1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
   {1,1,1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
  for(int s=0;s<4;++s)for(int i=0;i<19;++i)if(pat[s][i])
   rfaSet(m,model::office_file_binder,1.3f+((i*7+s)%3)*.012f,3.2f-.9f+i*.1f,cu+sh[s],.093f,.301f,.424f,rfE+((i*5+s*3)%7-3)*.006f);
  rfaSet(m,model::office_file_binder,1.32f,3.2f-.9f+3.6f*.1f,cu+sh[2],.093f,.301f,.424f,rfE+.5f);
  rfaSet(m,model::office_file_binder,1.31f,3.2f-.9f+4.5f*.1f,cu+sh[0],.093f,.301f,.424f,rfE-.4f);
  for(auto [x,y,yaw]:{std::tuple{1.9f,1.8f,.5f},{2.4f,1.8f,2.1f},{1.8f,4.55f,1.3f},{2.6f,4.7f,-.4f}})
   kit::propPose(m,model::office_file_binder,x,y,cu,.093f,.301f,.424f,yaw,kPi*.5f,0);
  for(auto [x,y,yaw]:{std::tuple{2.2f,2.25f,.7f},{2.4f,4.25f,-.5f}})kit::prop(m,model::office_paper_stack,x,y,cu,.329f,.443f,.03f,yaw);}
 for(float y:{2.6f,3.8f})rfaChair(m,5.f,y,cu,rfW);
 {const float dt=cu+.89f;
  kit::prop(m,model::crt_monitor,3.55f,3.85f,dt,.42f,.4f,.38f,rfW);
  kit::prop(m,model::keyboard,3.2f,3.85f,dt,.45f,.16f,.03f,rfW);
  kit::prop(m,model::lab_desk_lamp,3.4f,2.35f,dt,.18f,.398f,.521f,kPi);
  kit::prop(m,model::desk_phone,3.55f,2.8f,dt,.2f,.2f,.1f,rfW);
  terminal(m,{3.75f,3.15f},"DIRECTOR / INTERNAL MAIL","03:41 II: THIS FLOOR IS UNDER INVESTIGATION. YOUR SIGNATURE IS NOT REQUIRED.",
   "03:50 DIRECTOR: THE TIER 2 LIST WAS THE BOARD'S DECISION, NOT MINE. REPLY NOT SENT.",dz-f);
  m.terminals.back().readable=Readable::Letter;m.terminals.back().yaw=rfW;m.terminals.back().surface=.905f;}
 rfFix(m,model::office_couch_green,5.6f,7.12f,cu,2.242f,1.364f,1.192f,rfN,true);
 kit::prop(m,model::office_armchair_green,7.84f,5.8f,cu,1.399f,1.364f,1.192f,rfW);
 rfaTable(m,5.6f,5.5f,.55f,.3f,cu,.42f);
 rfFix(m,model::med_wall_clock,8.584f,4.4f,dz+2.f,.378f,.032f,.373f,rfW,false);
 rfFix(m,model::lab_potted_plant,1.5f,7.3f,cu,.37f,.43f,.69f,0,true);
 kit::prop(m,model::prop_cardboard_box_open,6.7f,1.55f,cu,.73f,.725f,.726f,.2f);
 kit::prop(m,model::prop_cardboard_box,7.55f,1.5f,cu,.727f,.72f,.367f,-.1f);
 paperDrift(m,1.8f,1.4f,7.f,6.f,cu,6,703);
 for(Vec2 l:{Vec2{3.6f,3.2f},Vec2{6.f,5.4f}})m.lights.push_back(lamp(l,roof-.2f,1.1f,6.f,4200,.08f));

 // --- The detail ------------------------------------------------------------------------------------
 // A heavy holds the corridor to the records door, his eyes on the glass and
 // the lounge; one man walks the atrium. They take their orders by hand.
 const char* clock="rf_admin_clock";
 rfGuard(m,clock,"rf_admin_door",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"rf_admin",{{{12.95f,9.3f},f,60000,-.29f}},13,.6f,1,1.f,0,130);
 rfGuard(m,clock,"rf_admin_walk",ActorVisual::Cleanup,GuardSquadRole::Assault,"rf_admin",
  {{{14.6f,19.2f},f,5000,rfN},{{19.4f,12.4f},f,4000,rfW},{{17.f,6.3f},f,4000,rfN},{{14.2f,11.f},f,3000,rfS}},14,.6f,1,1.3f);
 // --- The scene through the glass -------------------------------------------------------------------
 rfSequence(m,"rf_admin_scene","rf_admin_leaving",30000,21000,17.f,9.f,24.f,14.f);
 // The Investigator himself (ActorVisual::Investigator, clips: 0 idle, 1 walk,
 // 7 reading a card, 8 pointing). On the scene's clock: he reads at the open
 // drawers, points his door man to them, takes his card to the table under
 // the glass, keeps one, walks to the far door and is gone.
 // The table stop faces the clear south bay of the glass, not the hatch bars.
 {const Vec2 drawers{7.6f,11.9f},table{9.15f,14.6f},mid{7.5f,17.f},door{6.8f,18.6f},closet{6.8f,21.7f};
  const float aim=std::atan2(12.95f-drawers.x,9.3f-drawers.y),toTable=std::atan2(table.x-drawers.x,table.y-drawers.y);
  const float toMid=std::atan2(mid.x-table.x,mid.y-table.y),toDoor=std::atan2(door.x-mid.x,door.y-mid.y);
  ActorTrack inv;inv.timerState=stateId("rf_admin_scene");inv.visual=ActorVisual::Investigator;inv.scale=1.8f;
  inv.keys={{0,drawers,f,rfN,7,0},{8800,drawers,f,rfN,7,4.9f},
   {9000,drawers,f,aim,8,0},{10500,drawers,f,aim,8,1},{12200,drawers,f,aim,8,1},
   {12400,drawers,f,toTable,1,0},{14000,table,f,toTable,1,1.6f},
   {14200,table,f,rfE,7,0},{17600,table,f,rfE,7,2.9f},
   {17800,table,f,toMid,1,0},{20000,mid,f,toDoor,1,2.6f},{21800,door,f,rfS,1,3.8f},{23600,closet,f,rfS,1,5.2f},{24000,closet,f,rfS,0,0},{30000,closet,f,rfS,0,1}};
  m.actorTracks.push_back(std::move(inv));}
 // At 21 s the far door opens for him; four seconds later it shuts and the floor is quiet.
 rfSequence(m,"rf_admin_exit","rf_admin_gone",5000,4000);
 {ScriptEvent e;e.id=stateId("rf_admin_door_open");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;e.requireState=stateId("rf_admin_leaving");
  ScriptAction open;open.type=ScriptAction::Type::OpenDoor;open.index=2;ScriptAction start;start.type=ScriptAction::Type::SetState;start.id=stateId("rf_admin_exit");start.value=1;
  e.actions={open,start};m.scriptEvents.push_back(e);}
 {ScriptEvent e;e.id=stateId("rf_admin_door_shut");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;e.requireState=gone;
  ScriptAction shut;shut.type=ScriptAction::Type::CloseDoor;shut.index=2;ScriptAction save;save.type=ScriptAction::Type::Checkpoint;
  e.actions={shut,save};m.scriptEvents.push_back(e);}
 cue(m,"rf_admin_scene",2500,Sound::Empty,{7.6f,11.9f},"INVESTIGATOR: Every card from Energy Diagnostics. Bag them.",.7f,f+1.6f,"inv_admin_1");
 cue(m,"rf_admin_scene",6000,Sound::Empty,{12.95f,9.3f},"CLEANUP: Sir.",.6f,f+1.6f,"rf_admin_door_1");
 cue(m,"rf_admin_scene",9500,Sound::Empty,{7.6f,11.9f},"INVESTIGATOR: The rest of this floor can burn with the building.",.7f,f+1.6f,"inv_admin_2");
 cue(m,"rf_admin_scene",15400,Sound::Empty,{9.15f,14.6f},"INVESTIGATOR: Not this one. This one comes with me.",.7f,f+1.6f,"inv_admin_3");
 cue(m,"rf_admin_scene",21100,Sound::Door,{6.8f,19.1f},"",.8f,f+1.2f);
 cue(m,"rf_admin_exit",4100,Sound::Door,{6.8f,19.1f},"",.7f,f+1.2f);

 // --- Rewards and names ---------------------------------------------------------------------------
 m.pickupSpawns={{{12.6f,18.3f},PickupKind::Ammo,f},{{10.f,6.4f},PickupKind::Health,f},{{2.5f,18.2f},PickupKind::Health,f},
  {{13.f,4.2f},PickupKind::Ammo,dz},{{2.4f,6.6f},PickupKind::Ammo,dz}};
 m.pickupSpawns.push_back({{9.85f,7.f},PickupKind::HandgunAmmo,f});   // 9 mm among the cartons by the cooler
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{2,{20.1f,20.25f},cf+.42f},{0,{16.2f,20.8f},cf},{2,{11.45f,5.9f},f+.92f},{5,{9.7f,4.6f},f+.78f}});
 zone(m,"RESEARCH ADMINISTRATION / DIRECTOR'S OFFICE",1.f,1.f,12.f,8.f,dz-.5f,1000);
 zone(m,"RESEARCH ADMINISTRATION / OBSERVATION GALLERY",12.f,0,24.f,5.f,dz-.5f,1000);
 zone(m,"RESEARCH ADMINISTRATION / BOARDROOM",1.f,1.f,8.6f,7.8f);
 zone(m,"RESEARCH ADMINISTRATION / RECORDS",1.f,10.2f,12.f,19.f);
 zone(m,"RESEARCH ADMINISTRATION / CLEARANCE DESK",12.f,1.f,23.f,4.6f);
 zone(m,"RESEARCH ADMINISTRATION / LOUNGE",12.f,16.2f,23.f,23.f);
 zone(m,"RESEARCH ADMINISTRATION",0,0,24,24);
 recessedFittings(m,true);
 // An executive stair, not the plant's open steel. The engine draws a closed flight in
 // its dark deck plate behind dark steel guards, which stood in the atrium as a black
 // mass, so the flight is dressed here instead: every tread and riser faced in the
 // floor's own tile, a panelled balustrade under a timber cap on the open side and a
 // timber rail on the wall. No boundary door here to finish; the gallery deck is cut
 // back over the flight.
 for(auto& stair:m.stairs){stair.openUnderside=false;stair.sideRails=false;
  for(auto& layer:m.layers)if(layer.thickness>0&&layer.elevation>=stair.bottom-.02f&&layer.elevation<=stair.top+.02f)
   rectangle(layer,int(stair.x1),int(stair.y1),int(stair.x2),int(stair.y2),'.');
  const float run=(stair.y2-stair.y1)/stair.steps,rise=(stair.top-stair.bottom)/stair.steps;
  for(int i=0;i<stair.steps;++i){const float y1=stair.y1+run*i,y2=y1+run,top=stair.top-rise*i;
   wall(m,stair.x1,y1,stair.x2,y2,top,top+.012f,27);m.structures.back().solid=false;                // the tread
   wall(m,stair.x1,y2,stair.x2,y2+.012f,top-rise,top+.012f,27);m.structures.back().solid=false;     // its riser
   wall(m,stair.x1-.1f,y1,stair.x1,y2,stair.bottom,top+1.f,panel);}                                 // the balustrade
  // One handrail down each side, parallel to the nosings: over the balustrade to a
  // newel at the foot from one at the gallery's corner, and on the wall.
  wall(m,stair.x1-.13f,stair.y2,stair.x1+.03f,stair.y2+.16f,stair.bottom,stair.bottom+1.3f,panel);
  wall(m,stair.x1-.13f,stair.y1-.16f,stair.x1+.03f,stair.y1,stair.top,stair.top+1.32f,panel);
  m.pipes.push_back({{stair.x1-.05f,stair.y1-.04f},{stair.x1-.05f,stair.y2+.08f},stair.top+1.f+rise+.07f,.03f,stair.bottom+1.f+rise+.04f-rise*.34f,2});
  m.pipes.push_back({{stair.x2-.06f,stair.y1},{stair.x2-.06f,stair.y2},stair.top+.95f,.025f,stair.bottom+.95f,2});}
 return p;
}
}
void buildChapterResearchFront(CampaignMapTable& maps){
 maps[68]=makeResearchIntake();
 maps[69]=makePersonnelProcessing();
 maps[70]=makeResearchAdministration();
}
}
