// ENERGY TEST COMPLEX (chapter 2, manifest 58): the Observation Ring, chunks 87-90, blocked out.
// Four stitched chunks round one test pit 16 m deep: the ring walkway at -12 round its rim, the
// glazed galleries behind it, a room in each outer corner. See docs/Design/descent-campaign-20261025.md.
// The pit is where the ring test ran at 01:30. At 01:58 its field went self-sustaining; every feed
// was pulled and it is still up, violet at the bottom of the pit, arcing between the electrodes.
#include "CampaignChapterAnalysisKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
using namespace an;
// --- The ring's own fittings (rg) ---------------------------------------------------------------
WorldLight rgTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint=0,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
WorldLight rgGlow(Vec2 p,float z,float intensity,float range,Point3 gel,float flicker=0){
 auto l=lamp(p,z,intensity,range,6500,0,WorldLightMount::Bare,0,false,gel);l.flicker=flicker;return l;}
const Point3 rgViolet{.72f,.48f,1.f},rgArcBlue{.55f,.7f,1.f};
void rgHighBay(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});anFix(m,model::pendant_lamp,p.x,p.y,z,.5f,.5f,.34f,0,false);
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,0,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}
void rgCounter(Map& m,float x1,float y1,float x2,float y2,float f,float h=.9f){
 const float top=f+h;wall(m,x1+.05f,y1+.05f,x2-.05f,y2-.05f,f,f+.08f,8);wall(m,x1,y1,x2,y2,f+.08f,top-.04f,20);wall(m,x1,y1,x2,y2,top-.04f,top,28);}
void rgGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
DialogueReply rgReply(const char* text,const char* next="",const char* sets=""){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;if(*sets)r.sets.push_back({stateId(sets),1});return r;}
constexpr float F=-28,G=-12;
// One quadrant of the ring, written in the north-west quadrant's frame (pit in its south-east
// corner) and mirrored into place: mx mirrors x, my mirrors y. The galleries run along the frame's
// north and west sides behind glass; the corner room's partitions open onto both.
void rgQuadrant(Map& m,bool mx,bool my,int seed){
 const float roof=m.definition.ceiling,top=G+3.6f;
 auto X=[&](float x){return mx?24.f-x:x;};auto Y=[&](float y){return my?24.f-y:y;};
 auto box=[&](float x1,float y1,float x2,float y2,float lo,float hi,int mat){wall(m,X(x1),Y(y1),X(x2),Y(y2),lo,hi,mat);};
 auto lo=[](float a,float b){return std::min(a,b);};auto hi=[](float a,float b){return std::max(a,b);};
 auto tile=[&](int x1,int y1,int x2,int y2){int a=mx?24-x2:x1,b=mx?24-x1:x2,c=my?24-y2:y1,d=my?24-y1:y2;deck(m,G,a,c,b,d);};
 auto glaze=[&](float x1,float y1,float x2,float y2){glazing(m,lo(X(x1),X(x2)),lo(Y(y1),Y(y2)),hi(X(x1),X(x2)),hi(Y(y1),Y(y2)),G,roof,3,.75f,3.f,true);};
 // A heading (0 east, pi/2 south) or a fixture yaw, mirrored with the frame.
 auto H=[&](float a){if(mx)a=kPi-a;if(my)a=-a;return a;};auto Yaw=[&](float a){if(mx)a=-a;if(my)a=kPi-a;return a;};
 auto P=[&](float x,float y){return Vec2{X(x),Y(y)};};
 // the walkway and galleries: one deck round the pit, the centre corner carved
 tile(1,1,24,16);tile(1,16,16,24);
 rectangle(m.layers[0],mx?0:23,my?0:23,(mx?0:23)+1,(my?0:23)+1,'.');
 // the pit's lining and its steel courses
 box(15.7f,15.7f,16.f,24.f,F,G-.22f,15);box(16.f,15.7f,24.f,16.f,F,G-.22f,15);
 for(float z:{-24.f,-20.f,-16.f}){box(16.f,16.f,16.08f,24.f,z,z+.3f,12);box(16.08f,16.f,24.f,16.08f,z,z+.3f,12);}
 box(15.82f,15.82f,15.92f,24.f,G,G+1.1f,2);m.structures.back().rail=true;
 box(15.92f,15.82f,24.f,15.92f,G,G+1.1f,2);m.structures.back().rail=true;
 // caged lamps on the rail posts: the walk round the rim is lit at hand height, warm, against the pit
 for(auto [x,y]:{std::pair{15.87f,15.87f},{15.87f,19.6f},{15.87f,23.4f},{19.6f,15.87f},{23.4f,15.87f}}){
  anFix(m,model::mine_cage_lamp,X(x),Y(y),G+1.1f,.21f,.2f,.36f,0,false);
  m.lights.push_back(lamp(P(x,y),G+1.3f,.32f,4.5f,2700,0,WorldLightMount::Bare,0,false));}
 // the hall's outer face above the galleries: pilasters, steel courses, the dark upper windows of
 // the old instrument floor, and the sodium washers that light the face from the gallery head
 for(float t:{8.5f,15.2f,23.5f}){box(t-.3f,8.f,t+.3f,8.35f,G+3.1f,roof,15);box(8.f,t-.3f,8.35f,t+.3f,G+3.1f,roof,15);}
 box(7.8f,7.8f,8.5f,8.5f,G+3.1f,roof,15);
 for(float z:{G+3.1f,G+7.f,G+11.f}){box(8.f,8.f,24.f,8.16f,z,z+.25f,12);box(8.f,8.16f,8.16f,24.f,z,z+.25f,12);}
 for(auto [a,b]:{std::pair{9.f,14.7f},{15.7f,23.f}}){box(a,8.f,b,8.04f,G+8.f,G+10.f,9);box(8.f,a,8.04f,b,G+8.f,G+10.f,9);}
 for(float t:{12.f,19.5f}){m.lights.push_back(lamp(P(t,8.03f),G+4.1f,.75f,9.f,2100,0,WorldLightMount::Wall,H(kPi*.5f),true));
  m.lights.push_back(lamp(P(8.03f,t),G+4.1f,.75f,9.f,2100,0,WorldLightMount::Wall,H(0),true));}
 // the apparatus: the mast at the centre, its cradle, the electrodes in from the lining
 box(22.6f,22.6f,24.f,24.f,F,-15.f,12);box(22.2f,22.2f,24.f,24.f,-15.f,-14.7f,8);box(23.2f,23.2f,24.f,24.f,-14.7f,-13.6f,28);
 for(int a=0;a<2;++a){Vec2 s=a?Vec2{X(15.95f),Y(22.9f)}:Vec2{X(22.9f),Y(15.95f)},e=a?Vec2{X(22.65f),Y(22.9f)}:Vec2{X(22.9f),Y(22.65f)};
  m.pipes.push_back({s,e,-18.5f,.22f,-18.5f,4});
  for(float t:{.3f,.6f}){Vec2 c=s+(e-s)*t,d=(e-s)*(.05f/std::sqrt((e-s).x*(e-s).x+(e-s).y*(e-s).y));m.pipes.push_back({c-d,c+d,-18.5f,.38f,-18.5f,20});}}
 {Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=lo(X(19.f),X(24.f));h.x2=hi(X(19.f),X(24.f));h.y1=lo(Y(19.f),Y(24.f));h.y2=hi(Y(19.f),Y(24.f));
  h.bottom=-21.f;h.top=-14.f;h.damagePerSecond=40;h.period=2.7f;h.onTime=.6f;h.phase=seed*.6f;m.hazards.push_back(h);}
 hazard(m,Hazard::Kind::Anomaly,lo(X(17.f),X(24.f)),lo(Y(17.f),Y(24.f)),hi(X(17.f),X(24.f)),hi(Y(17.f),Y(24.f)),F,G-1.f,0);
 m.lights.push_back(rgGlow({X(22.6f),Y(22.6f)},-14.3f,1.1f,16.f,rgViolet,.2f));
 {auto l=rgGlow({X(20.f),Y(20.f)},-18.f,.7f,8.f,rgArcBlue);l.period=2.7f;l.onTime=.6f;l.phase=seed*.6f;l.flicker=.6f;m.lights.push_back(l);}
 m.lights.push_back(rgGlow({X(19.f),Y(19.f)},-25.5f,1.3f,16.f,rgViolet));
 // THE FIELD itself: grown up out of the pit floor round the cradle since 01:58, filaments of the ore's
 // own glass standing in the field lines, the tallest reaching over the rim, and the floor under them gone
 // to the same glass. Seen from anywhere on the ring: the source, not a fog.
 for(auto [x1,y1,x2,y2]:{std::tuple{17.4f,20.8f,19.4f,22.4f},{20.8f,17.2f,22.4f,19.1f},{19.1f,19.1f,20.7f,20.7f},{16.4f,22.6f,17.6f,23.8f}})box(x1,y1,x2,y2,F,F+.14f,35);
 for(auto [x,y,h]:{std::tuple{18.4f,21.3f,-12.4f},{21.3f,18.4f,-11.6f},{19.9f,19.9f,-13.8f},{17.2f,23.1f,-15.2f},{23.1f,17.2f,-14.6f},{20.6f,21.9f,-10.9f},{21.9f,20.6f,-16.4f},{18.9f,17.6f,-17.4f}}){
  box(x-.045f,y-.045f,x+.045f,y+.045f,F+.1f,h,35);box(x-.11f,y-.11f,x+.11f,y+.11f,F+.1f,F+.8f,35);}
 m.lights.push_back(rgGlow({X(20.2f),Y(20.2f)},-14.f,.8f,10.f,rgViolet,.1f));
 // The mast carried on up out of the pit: a lattice tower round a copper core, two square field
 // rings on outriggers either side of the walkway's height, and the crown that held the field's top.
 box(22.7f,22.7f,23.05f,23.05f,-14.7f,-1.5f,12);
 for(float z=-13.f;z<-2.f;z+=2.5f){box(22.7f,22.7f,24.f,22.85f,z,z+.14f,12);box(22.7f,22.85f,22.85f,24.f,z,z+.14f,12);}
 box(23.45f,23.45f,24.f,24.f,-13.6f,-1.5f,19);
 for(float z:{-10.6f,-7.4f}){box(20.4f,20.4f,24.f,20.56f,z,z+.1f,20);box(20.4f,20.56f,20.56f,24.f,z,z+.1f,20);
  box(20.56f,23.6f,22.7f,23.72f,z,z+.08f,12);box(23.6f,20.56f,23.72f,22.7f,z,z+.08f,12);}
 box(21.5f,21.5f,24.f,24.f,-1.5f,-.9f,12);box(21.4f,21.4f,24.f,21.56f,-1.5f,-1.2f,18);box(21.4f,21.56f,21.56f,24.f,-1.5f,-1.2f,18);
 m.lights.push_back(rgGlow({X(21.f),Y(21.f)},-9.f,.75f,9.f,rgViolet,.15f));
 m.lights.push_back(rgGlow({X(22.6f),Y(22.6f)},-2.1f,.7f,11.f,rgViolet));
 // what fell in: spent cells and a hard hat on the pit floor
 kit::prop(m,model::anl_cell_spent,X(18.5f),Y(20.5f),F,seed*.7f);propPose(m,model::anl_cell_spent,X(20.4f),Y(17.6f),F,1.f+seed,kPi*.5f,0);
 kit::prop(m,model::mine_hard_hat,X(17.6f),Y(18.8f),F,.279f,.351f,.153f,seed*1.3f);
 // the gallery walls: glass, a door onto the walkway from the north gallery, a mouth from the west
 box(8.f,7.8f,9.f,8.f,G,roof,3);
 if(seed!=0)glaze(9.f,7.8f,15.f,8.f);
 else{
  // Pit control's observation station: a glazed bay pushed out of the north gallery onto the walk,
  // square on the pit, where the ring test was watched. Open to the gallery behind it.
  glaze(9.f,7.8f,10.f,8.f);box(10.f,7.8f,15.f,8.f,top,roof,3);
  for(auto [x1,y1,x2,y2]:{std::tuple{10.f,8.f,10.2f,10.4f},{14.8f,8.f,15.f,10.4f},{10.2f,10.2f,14.8f,10.4f}})glazing(m,x1,y1,x2,y2,G,top,3,.75f,3.f,true);   // seed 0 is unmirrored
  box(9.9f,7.8f,15.1f,10.5f,top,top+.25f,3);box(9.9f,10.4f,15.1f,10.5f,top-.2f,top,12);
  for(auto [x,y]:{std::pair{9.92f,10.12f},{14.78f,10.12f}})box(x,y,x+.3f,y+.38f,G,top,12);
  rgCounter(m,10.5f,9.3f,14.5f,10.1f,G);
  for(float x:{11.1f,13.3f}){kit::prop(m,model::crt_monitor,x,9.84f,G+.9f,.42f,.4f,.38f,anN);kit::prop(m,model::keyboard,x,9.45f,G+.9f,.45f,.16f,.03f,anN);
   kit::prop(m,model::office_chair,x+.1f,8.75f,G,.55f,.55f,.95f,kPi+(x<12?.25f:-.3f));}
  anModel(m,model::lab_scope_monitor,12.2f,9.84f,G+.9f,anN,false);
  anModel(m,model::lab_status_display,12.4f,10.16f,G+2.7f,anN,false);anModel(m,model::lab_status_display,11.f,10.16f,G+2.7f,anN,false);
  kit::prop(m,model::office_paper_stack,14.1f,9.8f,G+.9f,.33f,.44f,.03f,.3f);
  m.lights.push_back(lamp({12.5f,9.f},top-.25f,.8f,6.5f,3000,0,WorldLightMount::Bare));
  m.lights.push_back(lamp({12.2f,10.f},G+1.2f,.25f,2.6f,9000,-.15f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.1f;}
 box(15.f,7.8f,15.4f,8.f,G,roof,3);
 swingDoor(m,lo(X(15.4f),X(16.6f)),hi(X(15.4f),X(16.6f)),Y(7.9f),G,roof,my?-1.f:1.f);
 // A gallery opened onto the pit: the glass and its dado gone for a rail, the face carried over on a
 // striped beam and two steel posts, so the gallery walks along the field instead of past a window.
 auto openBay=[&](float x1,float y1,float x2,float y2){const bool alongX=x2-x1>y2-y1;
  box(x1,y1,x2,y2,G+3.f,roof,3);box(x1,y1,x2,y2,G+2.8f,G+3.f,13);
  if(alongX){box(x1,y1+.05f,x2,y2-.05f,G,G+1.1f,2);m.structures.back().rail=true;floorLine(m,lo(X(x1),X(x2)),lo(Y(y1),Y(y2)),hi(X(x1),X(x2)),hi(Y(y1),Y(y2)),G,13);
   for(float t:{x1+2.f,x2-2.f})box(t-.1f,y1,t+.1f,y2,G,G+2.8f,12);}
  else{box(x1+.05f,y1,x2-.05f,y2,G,G+1.1f,2);m.structures.back().rail=true;floorLine(m,lo(X(x1),X(x2)),lo(Y(y1),Y(y2)),hi(X(x1),X(x2)),hi(Y(y1),Y(y2)),G,13);
   for(float t:{y1+2.f,y2-2.f})box(x1,t-.1f,x2,t+.1f,G,G+2.8f,12);}};
 box(16.6f,7.8f,17.f,8.f,G,roof,3);if(seed==0)openBay(17.f,7.8f,23.f,8.f);else glaze(17.f,7.8f,23.f,8.f);box(23.f,7.8f,24.f,8.f,G,roof,3);
 box(7.8f,8.f,8.f,9.f,G,roof,3);glaze(7.8f,9.f,8.f,15.f);box(7.8f,15.f,8.f,15.3f,G,roof,3);
 doorwayY(m,lo(Y(15.3f),Y(16.7f)),hi(Y(15.3f),Y(16.7f)),lo(X(7.8f),X(8.f)),hi(X(7.8f),X(8.f)),G,roof);
 box(7.8f,16.7f,8.f,17.f,G,roof,3);if(seed==0)openBay(7.8f,17.f,8.f,23.f);else glaze(7.8f,17.f,8.f,23.f);box(7.8f,23.f,8.f,24.f,G,roof,3);
 // the corner room's partitions: a mouth to the north gallery, a door to the west gallery
 box(7.8f,1.f,8.f,3.5f,G,top,3);doorwayY(m,lo(Y(3.5f),Y(4.9f)),hi(Y(3.5f),Y(4.9f)),lo(X(7.8f),X(8.f)),hi(X(7.8f),X(8.f)),G,top);box(7.8f,4.9f,8.f,7.8f,G,top,3);
 box(1.f,7.8f,3.4f,8.f,G,top,3);swingDoor(m,lo(X(3.4f),X(4.6f)),hi(X(3.4f),X(4.6f)),Y(7.9f),G,top,my?-1.f:1.f);box(4.6f,7.8f,7.8f,8.f,G,top,3);
 // the galleries' slab and their lamps; benches along the glass
 box(1.f,1.f,24.f,7.8f,top,top+.2f,3);box(1.f,7.8f,7.8f,24.f,top,top+.2f,3);
 for(auto [x,y,fl]:{std::tuple{12.f,4.5f,0.f},{20.f,4.5f,seed==2?.4f:0.f},{4.5f,12.f,0.f},{4.5f,20.f,0.f},{4.5f,4.5f,0.f}})m.lights.push_back(rgTube(X(x),Y(y),top,.45f,6.f,3300,0,fl));
 for(float x:{11.6f,20.4f})if(seed!=0||x>15)bench(m,X(x),Y(7.1f),G);
 // Each quarter's own silhouette in the pit, so a glance across it says where on the ring you are.
 if(seed==0){
  // north-west: the instrument boom, a lattice girder from pit control's face out to the mast head
  const Vec2 a=P(12.2f,8.2f),b=P(22.85f,22.85f);
  box(11.8f,8.f,12.6f,8.5f,-4.6f,-3.3f,12);                                                     // its shoe on the hall face
  for(float dz:{0.f,-.8f})m.pipes.push_back({a,b,-3.6f+dz,.11f,-2.6f+dz,2});
  for(int i=0;i<10;++i){const float t0=i/10.f,t1=(i+1)/10.f;const Vec2 c0=a+(b-a)*t0,c1=a+(b-a)*t1;
   m.pipes.push_back({c0,c1,-3.6f+t0,.04f,-4.4f+t1,2});}
  const Vec2 pod=a+(b-a)*.55f;m.pipes.push_back({pod,pod,-7.f,.03f,-3.6f+.55f-.8f,6});
  vessel(m,pod,.42f,-8.2f,-7.f,5);
  m.lights.push_back(lamp(pod,-8.4f,.7f,10.f,6000,0,WorldLightMount::Bare,0,true));
}
 if(seed==1){
  // north-east: the visitors' viewing balcony, out over the pit past the rail; it gave at 01:58 and
  // its outer half hangs straight down the pit from its torn hinge
  box(18.f,15.92f,21.f,18.2f,G-.22f,G,2);box(18.f,18.2f,21.f,18.36f,G-3.4f,G-.15f,2);
  box(18.f,15.92f,18.08f,18.2f,G,G+1.1f,2);m.structures.back().rail=true;box(20.92f,15.92f,21.f,17.4f,G,G+1.1f,2);m.structures.back().rail=true;
  box(18.f,18.36f,21.f,18.44f,G-3.4f,G-2.5f,2);
  for(float x:{18.2f,20.8f})box(x-.1f,15.92f,x+.1f,16.4f,G-1.6f,G-.22f,12);                       // its brackets into the lining
  box(18.f,15.92f,21.f,18.5f,G,roof,30);}
 if(seed==2){
  // south-west: the pit crew's hoist. A gantry girder from the hall face to the mast, its trolley,
  // the ropes and the man-cage left halfway down, still lit; the winch on the walk, the ladder.
  box(8.f,19.2f,23.7f,19.8f,-3.f,-2.3f,18);box(8.f,19.1f,23.7f,19.2f,-3.2f,-2.3f,12);box(8.f,19.8f,23.7f,19.9f,-3.2f,-2.3f,12);
  box(19.f,19.f,20.f,20.f,-3.6f,-3.f,12);box(18.95f,18.95f,20.05f,20.05f,-17.75f,-17.45f,12);   // the trolley; the cage's head frame
  for(float dy:{-.25f,.25f})m.pipes.push_back({P(19.5f,19.5f+dy),P(19.5f,19.5f+dy),-17.8f,.02f,-3.6f,6});
  anFix(m,model::mine_lift,X(19.5f),Y(19.5f),-21.4f-m.layers.front().elevation,2.073f,2.302f,3.899f,Yaw(0),false);
  m.lights.push_back(lamp(P(19.5f,19.5f),-18.4f,.6f,6.f,3200,0,WorldLightMount::Bare,0,false));
  anFix(m,model::pwr_lift_station,X(21.f),Y(12.4f),G-m.layers.front().elevation,2.035f,2.f,2.144f,Yaw(0),false);
  box(19.98f,11.4f,22.02f,13.4f,G,roof,30);                                                      // its collision, and nobody on its roof
  m.pipes.push_back({P(20.6f,13.f),P(19.5f,19.5f),G+2.3f,.025f,-3.6f,6});
  for(float base:{-20.f,-27.4f})anFix(m,model::mine_shaft_ladder,X(16.42f),Y(17.4f),base-m.layers.front().elevation,.784f,.901f,7.457f,Yaw(kPi*.5f),false);}
 if(seed==3){
  // south-east: the containment feed, a trunk duct from the hall face out over the walk and down the
  // pit to the field: the feed somebody took off at half two
  box(17.8f,8.f,18.8f,20.5f,-5.f,-4.f,11);box(17.8f,19.5f,18.8f,20.5f,-20.f,-5.f,11);
  for(float y=9.f;y<20.f;y+=2.8f)box(17.7f,y,18.9f,y+.2f,-5.1f,-3.9f,12);
  for(float z=-18.f;z<-5.f;z+=3.f)box(17.7f,19.4f,18.9f,20.6f,z,z+.2f,12);
  for(float y:{9.5f,15.f})m.pipes.push_back({P(18.3f,y),P(18.3f,y),-4.f,.03f,roof,6});
  {auto l=lamp(P(18.3f,19.3f),-6.f,.4f,4.f,3000,0,WorldLightMount::Bare,0,false,{1.f,.5f,.06f});l.period=1.2f;l.onTime=.6f;m.lights.push_back(l);}}
 // the hall: sodium high-bays from the dome
 rgHighBay(m,{X(12.f),Y(12.f)},roof,-2.f,1.1f,15.f,2100,seed==1?.3f:0.f);
 rgHighBay(m,{X(20.f),Y(11.f)},roof,-2.f,seed==3?0.f:.9f,14.f,2100);
 rgHighBay(m,{X(11.f),Y(20.f)},roof,-2.f,.9f,14.f,2100);
 // the air: violet over the pit, thick in it
 zone(m,"",lo(X(16.f),X(24.f)),lo(Y(16.f),Y(24.f)),hi(X(16.f),X(24.f)),hi(Y(16.f),Y(24.f)),F-1,G-.3f,{.16f,.11f,.22f,.009f});
 zone(m,"OBSERVATION RING / THE PIT",lo(X(8.f),X(24.f)),lo(Y(8.f),Y(24.f)),hi(X(8.f),X(24.f)),hi(Y(8.f),Y(24.f)),-1000,1000,{.12f,.11f,.12f,.003f});
}
// The ring's common start: its four quadrants share everything but the corner rooms and seams.
std::shared_ptr<Map> rgChunk(int index,bool mx,bool my,int seed){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[size_t(index)];m.definition=CampaignChunks[size_t(index)];m.definition.ambient=.17f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 machineryClock(m,("rg_clock_"+std::to_string(index)).c_str(),60000);
 (void)mx;(void)my;(void)seed;return p;
}
}

// OBSERVATION RING / NORTH-WEST (manifest 58). Pit control: the desk the ring test was run from,
// its log, and the board that still shows the field up with nothing feeding it. In from the
// High-Energy Test Bay (N); out west to the Shielding Corridor (W).
std::shared_ptr<const Map> makeObservationRingNorthWest(){
 auto p=rgChunk(87,false,false,0);auto& m=*p;const float top=G+3.6f;
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=G;
 openEdge(m,{'N',10.f,14.f,G,false,0});openEdge(m,{'E',1.f,23.f,G,false,0});openEdge(m,{'S',1.f,23.f,G,false,0});openEdge(m,{'W',9.f,14.f,G,false,0});
 deck(m,G,10,0,14,1);deck(m,G,0,9,1,14);
 rgQuadrant(m,false,false,0);
 // pit control: the desk along the north wall, the board on the west, the log
 rgCounter(m,1.4f,1.f,6.6f,1.7f,G);
 for(float x:{2.2f,4.6f}){kit::prop(m,model::crt_monitor,x,1.3f,G+.9f,.42f,.4f,.38f,anS);kit::prop(m,model::keyboard,x,1.58f,G+.9f,.45f,.16f,.03f,anS);}
 anModel(m,model::lab_scope_monitor,5.9f,1.3f,G+.9f,anS,false);
 for(auto [x,yaw]:{std::pair{2.2f,.2f},{4.7f,-.3f}})kit::prop(m,model::office_chair,x,2.5f,G,.55f,.55f,.95f,yaw);
 wall(m,1.f,3.2f,1.015f,5.f,G+.92f,G+1.68f,20);anModel(m,model::lab_control_board,1.11f,4.1f,G+1.f,anE,false);
 anModel(m,model::lab_status_display,1.04f,4.1f,G+2.f,anE,false);
 terminal(m,{6.6f,4.8f},"RING TEST 31 / LOG","01:30 FULL DRAW ORDERED FROM THE PIT. 01:40 R-12 TO THE CRADLE. 01:52 FIELD UP. 01:58 FIELD SELF-SUSTAINING.",
  "01:58 ALL FEEDS PULLED. 02:30 FIELD STILL UP. NOTHING FEEDING IT. THE PIT IS RUNNING ITSELF.",G-F);
 m.terminals.back().yaw=anW;
 taped(m,{7.79f,6.4f},G+1.5f,anW,Readable::Note,"PIT CONTROL / 02:12","THE LAMP IN THE PIT IS NOT ONE OF OURS. NOBODY HAS BEEN DOWN.",
  "DO NOT SHINE ANYTHING INTO IT. IT TURNED TOWARD MOLLOY'S TORCH AND THE ELECTRODES WENT WITH IT.");
 anModel(m,model::lab_wall_clock,4.f,1.02f,G+2.6f,anS,false);
 m.lights.push_back(lamp({4.f,1.8f},G+1.25f,.25f,2.6f,9000,-.15f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.1f;
 // the north seam's mouth from the test bay, the west passage's to the corridor
 wall(m,10.f,0.f,14.f,1.f,G+2.8f,top,28);wall(m,0.f,9.f,1.f,14.f,G+2.8f,top,28);
 m.pickupSpawns.push_back({{6.5f,6.6f},PickupKind::Ammo,G});
 zone(m,"OBSERVATION RING / PIT CONTROL",1.f,1.f,7.8f,7.8f);
 zone(m,"OBSERVATION RING / NORTH GALLERY",8.f,1.f,24.f,7.8f);
 zone(m,"OBSERVATION RING / WEST GALLERY",1.f,8.f,7.8f,24.f);
 zone(m,"OBSERVATION RING",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// OBSERVATION RING / NORTH-EAST (manifest 58). The visitors' gallery: where the board watched the
// first cell filled, couches and a carpet of brochures. Up north to the Resonance Laboratory.
std::shared_ptr<const Map> makeObservationRingNorthEast(){
 auto p=rgChunk(88,true,false,1);auto& m=*p;const float top=G+3.6f;
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=G;
 openEdge(m,{'N',10.f,14.f,G,false,0});openEdge(m,{'W',1.f,23.f,G,false,0});openEdge(m,{'S',1.f,23.f,G,false,0});
 deck(m,G,10,0,14,1);
 rgQuadrant(m,true,false,1);
 // the visitors' room: couch, the board's chairs, a painting, the plant
 anModel(m,model::office_couch_green,19.6f,1.75f,G,anS,true);
 for(auto [x,y,yaw]:{std::tuple{17.4f,4.6f,kPi*.5f},{21.8f,4.4f,-kPi*.5f},{19.6f,5.8f,kPi}})anModel(m,model::lab_boardroom_chair,x,y,G,yaw,true);
 anFix(m,model::wooden_table,19.6f,4.2f,G,1.367f,.724f,.78f,0,true);
 anModel(m,model::art_painting_5,22.97f,3.6f,G+1.5f,anW,false);
 anModel(m,model::lab_potted_plant,22.6f,1.4f,G,0,true);anModel(m,model::lab_potted_plant,16.6f,7.3f,G,0,true);
 anFix(m,model::water_cooler,22.6f,6.8f,G,.35f,.35f,1.1f,anW,true);
 kit::prop(m,model::mug,19.3f,4.1f,G+.78f,.09f,.09f,.1f,.6f);
 taped(m,{22.99f,5.4f},G+1.5f,anW,Readable::Letter,"VISITORS' GALLERY / 1984","FROM THIS ROOM THE BOARD WATCHED THE FIRST CELL FILLED WITH REFINED ORE. THE CHAIRMAN ASKED IF IT WAS SAFE TO STAND SO CLOSE.",
  "HE WAS TOLD THE GLASS WAS LEAD. IT WAS NOT, THEN. IT IS NOW. HE DIED IN 1991, OF WHAT THE FILE DOES NOT SAY.");
 paperDrift(m,16.6f,2.8f,22.6f,7.4f,G,7,8801);
 m.lights.push_back(lamp({19.6f,3.f},top-.4f,.4f,5.f,2700,0));
 wall(m,10.f,0.f,14.f,1.f,G+2.8f,top,28);
 // --- The visitors' side of the rim: everything here was put out for the board to look at the pit ---
 // the viewing benches along the rail, backs to the gallery; one went over when the balcony did
 auto seat=[&](float x,float y,bool alongX,float back){                                          // back: the side its back is on (+1/-1)
  if(alongX){wall(m,x-1.35f,y-.25f,x+1.35f,y+.25f,G+.43f,G+.51f,10);wall(m,x-1.35f,y+back*.2f-.04f,x+1.35f,y+back*.2f+.04f,G+.55f,G+1.05f,10);
   for(float d:{-1.f,1.f})wall(m,x+d-.06f,y-.2f,x+d+.06f,y+.2f,G,G+.43f,2);}
  else{wall(m,x-.25f,y-1.35f,x+.25f,y+1.35f,G+.43f,G+.51f,10);wall(m,x+back*.2f-.04f,y-1.35f,x+back*.2f+.04f,y+1.35f,G+.55f,G+1.05f,10);
   for(float d:{-1.f,1.f})wall(m,x-.2f,y+d-.06f,x+.2f,y+d+.06f,G,G+.43f,2);}};
 seat(2.6f,13.9f,true,-1);seat(10.2f,20.2f,false,1);seat(10.2f,17.f,false,1);
 wall(m,4.6f,13.6f,7.3f,13.68f,G,G+.51f,10);wall(m,4.6f,13.68f,7.3f,14.2f,G,G+.08f,10);              // the third, over on its back
 for(float x:{5.6f,6.6f})wall(m,x-.06f,13.7f,x+.06f,14.15f,G+.08f,G+.5f,2);
 // the exhibit lecterns on the rail, a lamp over each
 for(auto [x,y,yaw,title,sub]:{std::tuple{4.2f,15.7f,anN,"RING TEST FIELD / 1984","FROM THIS RAIL THE BOARD WATCHED THE FIRST FIELD HOLD ITSELF UP FOR NINE SECONDS"},
   {8.3f,20.6f,anE,"VISITORS","THE FIELD IS SAFE TO WATCH FROM THE RAIL. PLEASE DO NOT USE FLASH"}}){
  const bool alongX=std::fabs(std::cos(yaw))>.5f;
  if(alongX)wall(m,x-.78f,y-.05f,x+.78f,y+.12f,G,G+1.41f,2);else wall(m,x-.12f,y-.78f,x+.05f,y+.78f,G,G+1.41f,2);   // the lectern's steel stand
  wallSign(m,x,y,G+.74f,title,sub,1.5f,yaw);
  m.lights.push_back(lamp({alongX?x:x+1.2f,alongX?y-1.2f:y},G+2.6f,.45f,4.f,2700,0,WorldLightMount::Bare,0,true));}
 // the display cases they wheeled out for the opening, against the glass; one shoved aside, its case empty
 anModel(m,model::office_display_cabinet,2.9f,8.6f,G,anS,true);anModel(m,model::office_display_cabinet,5.5f,8.85f,G,anS+.4f,true);
 wall(m,2.35f,8.3f,6.1f,9.25f,G+2.1f,G+3.1f,30);
 // the gallery's emergency shutters, dropped at 01:58 on the walk side of the glass; one bay torn up
 for(float x:{1.f,7.f,9.f,15.f})wall(m,x-.05f,8.f,x+.05f,8.1f,G,G+3.1f,12);
 for(float z=G+.75f;z<G+3.05f;z+=.13f){
  if(z<G+2.2f){wall(m,1.f,8.f,4.1f,8.06f,z,z+.1f,2);wall(m,5.5f,8.f,7.f,8.06f,z,z+.1f,2);}else wall(m,1.f,8.f,7.f,8.06f,z,z+.1f,2);
  if(z>G+1.95f)wall(m,9.f,8.f,15.f,8.06f,z,z+.1f,2);}
 // --- The viewing rail as the visitors left it at 01:58: the carpet they were walked along, the brass
 // posts and rope that kept them a step back from the rail, the interpretation boards, the good chairs
 // brought out for the board, and everything people drop when they are told to walk, not run.
 wall(m,.3f,14.5f,8.05f,15.78f,G,G+.012f,23);wall(m,8.22f,15.78f,9.5f,23.7f,G,G+.012f,23);                     // the runner
 auto post=[&](float x,float y){wall(m,x-.13f,y-.13f,x+.13f,y+.13f,G,G+.03f,12);wall(m,x-.025f,y-.025f,x+.025f,y+.025f,G+.03f,G+.9f,18);wall(m,x-.045f,y-.045f,x+.045f,y+.045f,G+.9f,G+.97f,18);};
 auto rope=[&](Vec2 a,Vec2 b){wall(m,std::min(a.x,b.x),std::min(a.y,b.y)-.015f,std::max(a.x,b.x),std::max(a.y,b.y)+.015f,G+.8f,G+.84f,19);m.structures.back().solid=false;};
 for(int i=0;i<7;++i){const float x=.7f+i*1.2f;if(i==3)continue;post(x,14.35f);if(i<6&&i!=2)rope({x,14.335f},{x+1.2f,14.365f});}
 wall(m,4.2f,14.48f,4.95f,14.53f,G,G+.05f,18);                                                                   // one knocked flat
 for(int i=0;i<7;++i){const float y=16.6f+i*1.15f;post(9.7f,y);if(i<6&&i!=4)rope({9.685f,y},{9.715f,y+1.15f});}
 // the interpretation boards on their stands, a lamp on each, facing the visitors with the pit behind
 for(auto [x,y,yaw]:{std::tuple{6.6f,15.55f,anN},{8.6f,17.6f,anE},{8.6f,23.f,anE}}){
  const bool alongX=std::fabs(std::cos(yaw))>.5f;
  for(float d:{-.45f,.45f}){if(alongX)wall(m,x+d-.03f,y-.03f,x+d+.03f,y+.03f,G,G+1.f,12);else wall(m,x-.03f,y+d-.03f,x+.03f,y+d+.03f,G,G+1.f,12);}
  anModel(m,model::lab_notice_board,x,y,G+1.f,yaw,false);
  m.lights.push_back(lamp({alongX?x:x+1.f,alongX?y-1.f:y},G+2.5f,.5f,3.6f,2800,0,WorldLightMount::Bare,0,true));}
 // the board's chairs, carried out to the rail for the opening and left where the alarm found them
 for(auto [x,y]:{std::pair{2.f,15.1f},{5.f,15.1f},{8.9f,18.f},{8.9f,21.4f}})m.lights.push_back(lamp({x,y},G+2.8f,.45f,4.f,2700,0,WorldLightMount::Bare,0,true));   // the exhibit spots along the runner
 kit::prop(m,model::lab_boardroom_chair,1.6f,15.15f,G,kPi*.5f+.2f);kit::propPose(m,model::lab_boardroom_chair,5.3f,14.95f,G,1.9f,0,kPi*.5f);
 kit::prop(m,model::lab_boardroom_chair,9.f,19.f,G,-.3f);kit::propPose(m,model::lab_boardroom_chair,9.1f,21.9f,G,.4f,kPi*.5f,0);
 kit::propPose(m,model::office_armchair_green,11.6f,15.f,G,2.6f,0,kPi*.5f,.9f);
 // what they dropped: brochures, passes, the hard hats they were issued at the door, a cup, the guide's radio
 for(auto [x,y,yaw]:{std::tuple{2.3f,15.35f,.4f},{7.1f,14.9f,2.1f},{8.95f,18.4f,1.1f}})kit::prop(m,model::item_id_card,x,y,G+.012f,.132f,.085f,.002f,yaw);
 for(auto [x,y,yaw]:{std::tuple{3.4f,15.f,.9f},{9.15f,20.4f,2.8f}})kit::propPose(m,model::mine_hard_hat,x,y,G+.012f,yaw,.3f,1.2f);
 kit::prop(m,model::mug,6.2f,14.75f,G+.012f,.09f,.09f,.1f,1.3f);kit::propPose(m,model::mug,8.9f,22.6f,G+.012f,.09f,.09f,.1f,.2f,kPi*.5f,0);
 kit::prop(m,model::prop_cardboard_box_open,7.65f,15.4f,G+.012f,.4f,.4f,.4f,.3f);
 kit::prop(m,model::item_walkie,2.7f,14.75f,G+.012f,.11f,.38f,.05f,2.4f);
 paperDrift(m,.5f,14.45f,8.f,15.7f,G+.012f,8,8812);paperDrift(m,8.25f,16.f,9.55f,23.5f,G+.012f,6,8803);
 zone(m,"OBSERVATION RING / VISITORS' GALLERY",16.2f,1.f,23.f,7.8f,-1000,1000,{.16f,.13f,.10f,.005f});
 zone(m,"OBSERVATION RING / NORTH GALLERY",0,1.f,16.f,7.8f);
 zone(m,"OBSERVATION RING / EAST GALLERY",16.2f,8.f,23.f,24.f);
 zone(m,"OBSERVATION RING",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// OBSERVATION RING / SOUTH-WEST (manifest 58). The pit crew's room: their suits on the wall and
// the shower they used coming up. One of them never took his off.
std::shared_ptr<const Map> makeObservationRingSouthWest(){
 auto p=rgChunk(89,false,true,2);auto& m=*p;
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=G;
 openEdge(m,{'N',1.f,23.f,G,false,0});openEdge(m,{'E',1.f,23.f,G,false,0});
 rgQuadrant(m,false,true,2);
 // the crew room: suits on the west wall, a bench, lockers on the south, the shower
 wall(m,1.f,17.f,1.1f,21.4f,G+1.9f,G+1.94f,20);
 for(int i=0;i<5;++i){const float y=17.4f+i*.85f;wall(m,1.f,y-.015f,1.12f,y+.015f,G+1.82f,G+1.94f,8);if(i!=3)hungSuit(m,{1.27f,y,G+1.48f},(i%2?.1f:-.1f));}
 bench(m,4.4f,19.6f,G);
 for(int i=0;i<4;++i)anFix(m,model::psx_locker,3.3f+i*.86f,22.55f,G,.64f,.9f,2.3f,kPi*.5f,true);
 wall(m,2.95f,22.1f,6.7f,23.f,G+2.3f,G+3.6f,30);
 anModel(m,model::med_scrub_sink,7.5f,17.f,G,anW,true);
 kit::prop(m,model::item_dosimeter,4.7f,19.62f,G+.51f,.31f,.146f,.066f,.4f);
 m.lights.push_back(lamp({4.f,19.6f},G+3.2f,.4f,5.f,4200,0));m.lights.back().flicker=.3f;
 m.creatureSpawns.push_back({CreatureKind::Transformed,{11.f,19.5f},G,1,0});                    // Molloy, still in his suit
 // the decontamination shower they came up through, its tiles still wet; their hats on the shelf
 wall(m,6.2f,19.9f,7.8f,21.7f,G,G+.012f,20);anModel(m,model::med_floor_drain,7.f,20.8f,G+.012f,0,false);
 wall(m,5.f,16.2f,7.4f,16.62f,G+1.6f,G+1.64f,2);wall(m,5.f,16.2f,7.4f,16.62f,G+1.85f,G+3.6f,30);for(float x:{5.f,7.36f})wall(m,x,16.2f,x+.04f,16.62f,G+1.3f,G+1.64f,2);
 for(auto [x,yaw]:{std::pair{5.4f,.2f},{6.1f,.1f}})kit::prop(m,model::mine_hard_hat,x,16.42f,G+1.64f,.279f,.351f,.153f,yaw);
 m.lights.push_back(lamp({6.9f,20.6f},G+3.2f,.35f,4.f,5200,0));
 m.pickupSpawns.push_back({{6.4f,17.2f},PickupKind::Health,G});
 // --- The observation bridge: it crossed from this quarter's rim to the mast, for the crew to read the
 // cradle from above. At 01:58 it let go in the middle. The near half hangs from its hinge at the rail
 // straight down the pit, its work lamp still lit at the bottom of it; the far half hangs off the mast.
 {const float y1=1.45f,y2=2.95f,yc=2.2f;
  wall(m,15.9f,y1-.12f,16.14f,y2+.12f,G-.32f,G,12);                                                          // the hinge
  for(int i=0;i<22;++i){const float x0=15.98f+i*.16f,z=-12.05f-i*.415f;wall(m,x0,y1+.06f,x0+.18f,y2-.06f,z-.12f,z,8);
   wall(m,x0,y1,x0+.18f,y1+.06f,z-.55f,z+.05f,18);wall(m,x0,y2-.06f,x0+.18f,y2,z-.55f,z+.05f,18);              // its grating between yellow stringers, bay by bay
   if(i%4==1)for(float y:{y1+.03f,y2-.03f})wall(m,x0+.06f,y-.02f,x0+.1f,y+.02f,z,z+1.f,18);}                  // the handrail posts, the rail itself gone
  for(float y:{y1+.03f,y2-.03f})m.pipes.push_back({{16.06f,y},{19.45f,y},-12.2f,.07f,-21.1f,2});
  m.pipes.push_back({{16.06f,yc},{19.4f,yc},-12.25f,.05f,-20.84f,6});
  for(auto [x,y,lo,hi]:{std::tuple{19.4f,1.8f,-21.2f,-25.6f},{19.3f,2.6f,-21.f,-24.1f},{19.45f,2.3f,-21.3f,-27.f}})m.pipes.push_back({{x,y},{x+.12f,y+.05f},lo,.02f,hi,7});
  anFix(m,model::mine_cage_lamp,19.15f,yc,-21.15f,.21f,.2f,.36f,kPi*.5f,false);
  {auto l=lamp({19.05f,yc},-20.8f,.9f,8.f,3000,0,WorldLightMount::Bare,0,true);l.flicker=.12f;m.lights.push_back(l);}
  wall(m,22.5f,1.15f,23.05f,y2+.1f,-12.45f,-12.05f,12);                                                       // the far half's bracket on the mast
  for(int i=0;i<11;++i){const float x1=22.55f-i*.16f,z=-12.1f-i*.38f;wall(m,x1-.18f,y1+.06f,x1,y2-.06f,z-.12f,z,8);
   for(float y:{y1,y2-.06f})wall(m,x1-.18f,y,x1,y+.06f,z-.5f,z+.05f,18);}
  for(float y:{y1+.03f,y2-.03f})m.pipes.push_back({{22.5f,y},{20.95f,y},-12.2f,.06f,-16.1f,2});
  // the bridge head on the walk: its gate chained shut across nothing, the floor striped
  anModel(m,model::mine_cage_gate,15.76f,yc,G,kPi*.5f,false);wall(m,15.68f,yc-1.1f,15.95f,yc+1.1f,G,G+3.f,30);
  floorLine(m,14.7f,y1-.2f,15.6f,y2+.2f,G,13);
  m.lights.push_back(lamp({14.9f,yc},G+3.f,.5f,5.f,3000,0,WorldLightMount::Bare,0,true));
  m.lights.push_back(lamp({17.6f,yc+1.4f},-14.5f,1.1f,9.f,4200,0,WorldLightMount::Bare,0,false));
  m.lights.push_back(lamp({21.6f,yc+1.2f},-13.f,.6f,5.f,4200,0,WorldLightMount::Bare,0,false));}             // the crew's flood, still aimed down it
 zone(m,"OBSERVATION RING / CREW ROOM",1.f,16.2f,7.8f,23.f);
 zone(m,"OBSERVATION RING / SOUTH GALLERY",8.f,16.2f,24.f,23.f);
 zone(m,"OBSERVATION RING / WEST GALLERY",1.f,0,7.8f,16.f);
 zone(m,"OBSERVATION RING",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// OBSERVATION RING / SOUTH-EAST (manifest 58). Detail Nine's camp: a Cleanup squad cut off when
// Containment B went, holding the ring because they were told to and because there is nowhere
// else. A truce while the player keeps a weapon down. Their sergeant knows what happened to the
// containment feed. In from Containment Intake (E).
std::shared_ptr<const Map> makeObservationRingSouthEast(){
 auto p=rgChunk(90,true,true,3);auto& m=*p;const float top=G+3.6f;
 m.definition.playerStart={22.f,11.5f};m.definition.spawnHeight=G;m.definition.spawnYaw=kPi;
 openEdge(m,{'N',1.f,23.f,G,false,0});openEdge(m,{'W',1.f,23.f,G,false,0});openEdge(m,{'E',9.f,14.f,G,false,0});
 deck(m,G,23,9,24,14);
 rgQuadrant(m,true,true,3);
 wall(m,23.f,9.f,24.f,14.f,G+2.8f,top,28);
 // the camp: crates for a wall, the generator and its lamp, bedding, the radio
 for(auto [x,y]:{std::pair{17.f,16.9f},{18.3f,16.9f}}){anFix(m,model::pallet,x,y,G,1.2f,1.15f,.16f,0,true);anModel(m,model::lab_specimen_crate,x,y,G+.16f,.1f,true,.85f);}
 anModel(m,model::mine_portable_generator,22.3f,22.2f,G,.4f,true);
 m.lights.push_back(lamp({20.f,19.5f},G+2.4f,.6f,6.f,3400,0,WorldLightMount::Bare));
 m.pipes.push_back({{20.f,19.5f},{20.f,19.5f},G+2.45f,.016f,top,6});anFix(m,model::pendant_lamp,20.f,19.5f,G+2.45f,.5f,.5f,.34f,0,false);
 for(auto [x,y,yaw]:{std::tuple{18.f,21.8f,0.f},{21.6f,19.4f,kPi*.5f}})anModel(m,model::med_mattress_stained,x,y,G,yaw,false);
 anFix(m,model::wooden_table,19.6f,18.9f,G,1.367f,.724f,.78f,0,true);
 kit::prop(m,model::item_walkie,19.4f,18.85f,G+.78f,.114f,.376f,.046f,.4f);kit::prop(m,model::mug,20.f,19.f,G+.78f,.09f,.09f,.1f,1.1f);
 anFix(m,model::med_first_aid_box,22.94f,17.6f,G+1.3f,.367f,.12f,.349f,anW,false);
 // --- The line: Detail Nine's barricade round the Containment door, the gap they talk through, the
 // ground between it and the door nobody stands on, and the fire they keep behind it ----------------
 for(float y:{9.3f,14.35f}){anModel(m,model::scrap_barrier_wall,20.8f,y,G,kPi*.5f,true);wall(m,20.55f,y-1.5f,21.05f,y+1.5f,G+2.05f,top,30);}
 for(auto [x,y]:{std::pair{19.8f,10.3f},{19.8f,13.4f}}){anFix(m,model::pallet,x,y,G,1.2f,1.15f,.16f,kPi*.5f,true);anModel(m,model::lab_specimen_crate,x,y,G+.16f,kPi*.5f+.1f,true,.85f);}
 floorLine(m,21.15f,7.8f,21.3f,15.85f,G,13);floorLine(m,22.7f,8.9f,22.82f,14.1f,G,13);              // their line, and the door's
 kit::prop(m,model::mine_hard_hat,22.1f,12.9f,G,.279f,.351f,.153f,2.2f);kit::prop(m,model::anl_cell_spent,21.8f,10.4f,G,1.1f);
 paperDrift(m,21.4f,8.4f,22.8f,15.f,G,5,9001);
 anModel(m,model::scrap_burn_barrel,18.3f,10.2f,G,0,true);
 {auto l=lamp({18.3f,10.2f},G+1.25f,1.1f,8.f,1800,0,WorldLightMount::Bare,0,true);l.flicker=.45f;m.lights.push_back(l);}
 // where they sleep: bedrolls behind the line, a lantern by each
 for(float y:{8.2f,15.f}){anModel(m,model::scrap_bedroll,17.6f,y,G,kPi*.5f+.1f,false);anModel(m,model::scrap_lantern,18.9f,y+.6f,G,0,false);}
 m.lights.push_back(lamp({18.9f,15.6f},G+.6f,.35f,3.5f,2200,0,WorldLightMount::Bare,0,false));
 // what they have left: water, the last crates, the ammunition they count
 for(auto [x,y]:{std::pair{17.2f,4.2f},{17.2f,5.6f}}){anFix(m,model::pallet,x,y,G,1.2f,1.15f,.16f,kPi*.5f,true);anModel(m,model::lab_specimen_crate,x,y,G+.16f,.05f,true,.85f);}
 wall(m,16.6f,3.6f,17.8f,6.2f,G+1.13f,top,30);
 for(float y:{10.3f,13.4f})wall(m,19.2f,y-.6f,20.4f,y+.6f,G+1.13f,top,30);
 anModel(m,model::scrap_burn_barrel,17.4f,18.6f,G,.4f,true);
 {auto l=lamp({17.4f,18.6f},G+1.25f,.8f,6.f,1800,0,WorldLightMount::Bare,0,true);l.flicker=.45f;m.lights.push_back(l);}
 for(auto [x,y,yaw]:{std::tuple{17.5f,20.6f,.6f},{21.f,21.2f,2.1f}})kit::prop(m,model::prop_cardboard_box_open,x,y,G,.4f,.4f,.4f,yaw);
 taped(m,{22.99f,20.6f},G+1.5f,anW,Readable::Note,"DETAIL NINE / ROLL","ELEVEN UP AT 01:00. SIX AT 02:50 WHEN B WENT. FOUR NOW. TWO OF THE FOUR ARE THE MEDIC'S WORRY.",
  "ORDERS: HOLD THE RING. NOBODY UP FROM CONTAINMENT. NOBODY HAS COME UP. IT IS WHAT IS COMING UP BEHIND THEM.");
 // --- No man's land: along the south arm of the walk, between Detail Nine's forward line and the line the
 // survivors from Containment built when they came up and Nine would not let them past. Six metres of
 // walk between, a fire in the middle both sides feed, the supplies both sides draw from by turns, the
 // two lines painted on the floor. Nobody crosses with a weapon up. They watch each other all night.
 for(auto [x,piece,y]:{std::tuple{7.6f,int(model::scrap_barrier_wall),9.85f},{7.6f,int(model::scrap_barrier_wall),14.3f},{1.8f,int(model::scrap_patched_wall),9.85f},{1.8f,int(model::scrap_patched_wall),14.3f}}){
  anModel(m,piece,x,y,G,kPi*.5f,true);if(x<5)wall(m,x-.22f,y-1.55f,x+.22f,y+1.55f,G+2.45f,G+4.5f,30);}         // nobody along the top of the tall one
 for(float x:{2.5f,6.9f})floorLine(m,x,8.25f,x+.1f,15.8f,G,13);                                              // the two lines
 anModel(m,model::scrap_brazier,4.7f,12.1f,G,.3f,true);
 {auto l=lamp({4.7f,12.1f},G+.95f,1.35f,9.f,1800,0,WorldLightMount::Bare,0,true);l.flicker=.45f;m.lights.push_back(l);}
 for(auto [x,y,yaw]:{std::tuple{4.3f,14.7f,.1f},{5.1f,14.8f,-.2f}})anModel(m,model::ind_crate,x,y,G,yaw,true);
 kit::prop(m,model::prop_cardboard_box_open,4.7f,14.75f,G+.81f,.4f,.4f,.4f,.5f);kit::prop(m,model::prop_cardboard_box,5.6f,13.9f,G,.5f,.495f,.252f,.7f);
 // Nine's side: the flood on its pole, looking west across the ground; a crate of what they have left
 wall(m,8.f,8.78f,8.2f,8.98f,G,G+2.5f,12);anFix(m,model::mine_cage_lamp,7.9f,8.88f,G+2.2f,.21f,.2f,.36f,-kPi*.5f,false);
 m.lights.push_back(lamp({7.96f,8.88f},G+2.3f,1.1f,11.f,4300,0,WorldLightMount::Wall,kPi,true));
 anModel(m,model::lab_specimen_crate,8.7f,14.9f,G,.2f,true,.8f);
 // the survivors' side: two bedrolls, a lantern, the people
 for(float y:{9.4f,14.7f})anModel(m,model::scrap_bedroll,.75f,y,G,.05f,false);
 anModel(m,model::scrap_lantern,.45f,12.1f,G,0,false);
 {auto l=lamp({.45f,12.1f},G+.6f,.5f,4.f,2200,0,WorldLightMount::Bare,0,false);l.flicker=.2f;m.lights.push_back(l);}
 actor(m,"rg_clock_90",ActorVisual::Worker,1.7f,{{0,{2.25f,10.2f},G,.1f,0,0},{9000,{2.25f,10.2f},G,.1f,0,4}},true);          // stood at their line, as Nine stand at theirs
 actor(m,"rg_clock_90",ActorVisual::LabTech,1.7f,{{0,{2.3f,13.7f},G,-.1f,0,0},{9000,{2.3f,13.7f},G,-.1f,0,4}},true);
 actor(m,"rg_clock_90",ActorVisual::FemaleScientist,1.7f,{{0,{2.25f,15.3f},G,-.3f,0,0},{9000,{2.25f,15.3f},G,-.3f,0,4}},true);
 // --- Detail Nine: three men holding a truce ---------------------------------------------------
 const StateId stance=stateId("rg_nine_stance");
 rgGuard(m,"rg_clock_90","rg_nine_sgt",ActorVisual::Cleanup,GuardSquadRole::Assault,"rg_nine",{{{19.5f,12.f},G,60000,kPi}},10,2,1.f,110);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("rg_nine_hello");t.ai.dropsSidearm=true;}
 rgGuard(m,"rg_clock_90","rg_nine_medic",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"rg_nine",{{{18.8f,20.4f},G,60000,-kPi*.5f}},9,1,.9f,80);
 m.actorTracks.back().ai.stance=stance;
 rgGuard(m,"rg_clock_90","rg_nine_eng",ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,"rg_nine",{{{8.6f,12.1f},G,14000,kPi},{{11.f,4.f},G,6000,kPi}},11,2,1.f,90);
 m.actorTracks.back().ai.stance=stance;
 m.dialogue.push_back({stateId("rg_nine_hello"),"DETAIL NINE","That's far enough. Weapon down and we've no quarrel. We've no anything, any more.","",{
  rgReply("Who are you holding the ring for?","rg_nine_who"),rgReply("What's west of here?","rg_nine_west"),
  rgReply("Containment's dark. Why?","rg_nine_power"),rgReply("I'll keep moving.")}});
 m.dialogue.push_back({stateId("rg_nine_who"),"DETAIL NINE","Him. The Investigator. Hold the ring, nobody up from Containment. Nobody's come up. Not since B went.","",{rgReply("Back.","rg_nine_hello")}});
 m.dialogue.push_back({stateId("rg_nine_west"),"DETAIL NINE","Shielding detail's in the corridor. Two men, still on his orders. Past them, Experimental Power, then Conversion.","",{rgReply("Back.","rg_nine_hello","rg_nine_hint")}});
 m.dialogue.push_back({stateId("rg_nine_power"),"DETAIL NINE","Somebody had Conversion taken off the containment feed at half two. B's fields went quarter of an hour later. Put it back and the intake doors run again.","",{rgReply("Back.","rg_nine_hello","rg_nine_feed")}});
 {ScriptEvent e;e.id=stateId("rg_nine_truce");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stance;a.value=int(SquadStance::Truce);e.actions={a};m.scriptEvents.push_back(e);}
 {TimedSequence s;s.timerState=stateId("rg_nine_radio");s.finishState=stateId("rg_nine_radio_done");s.x1=20.f;s.y1=8.f;s.x2=24.f;s.y2=15.f;
  s.durationMs=15000;s.finishAtMs=13000;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);}
 cue(m,"rg_nine_radio",800,Sound::Empty,{19.4f,18.85f},"INVESTIGATOR (RADIO): Detail nine. Hold the ring. Nothing comes up from Containment.",.9f,G+1.f);
 cue(m,"rg_nine_radio",6000,Sound::Empty,{19.4f,18.85f},"RADIO: Copy. Sir, there's nothing left down there to come up.",.9f,G+1.f);
 cue(m,"rg_nine_radio",10500,Sound::Empty,{19.4f,18.85f},"INVESTIGATOR (RADIO): Then hold it anyway.",.9f,G+1.f);
 m.pickupSpawns.push_back({{21.8f,17.f},PickupKind::HandgunAmmo,G});
 titleCard(m,"ENERGY TEST COMPLEX",20.f,8.f,24.f,15.f);
 zone(m,"OBSERVATION RING / DETAIL NINE",16.2f,16.2f,23.f,23.f,-1000,1000,{.15f,.13f,.11f,.005f});
 zone(m,"OBSERVATION RING / NO MAN'S LAND",0,8.2f,8.1f,16.f);
 zone(m,"OBSERVATION RING / EAST GALLERY",16.2f,0,23.f,16.f);
 zone(m,"OBSERVATION RING / SOUTH GALLERY",0,16.2f,16.f,23.f);
 zone(m,"OBSERVATION RING",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
void buildChapterObservationRing(CampaignMapTable& maps){
 maps[87]=makeObservationRingNorthWest();
 maps[88]=makeObservationRingNorthEast();
 maps[89]=makeObservationRingSouthWest();
 maps[90]=makeObservationRingSouthEast();
}
}
