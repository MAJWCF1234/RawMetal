// Breaches (BREACH, src/world/Openings.cpp): holes broken through walls, as the Campaign Builder's
// Breach tool writes them. The tube puts a ragged breach in a wall of # tiles across the plain room
// with a bright lamp behind it, and an arch and a square hole in a second wall, and checks that
// each works as a hole: the player walks through, the creatures' walk test goes through, rounds
// and light go through, and the wall round it stands. The shot "breach" films the light coming
// through the ragged one.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "game/ai/Navigation.h"
#include "game/ai/Nodes.h"
#include "world/CampaignKit.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
void author(AuthoredMapData& m){
 m.layers[0].rows[10]=std::string(24,'#');m.layers[0].rows[4]=std::string(24,'#');
 Breach ragged;ragged.x1=8;ragged.y1=10;ragged.x2=11;ragged.y2=11;ragged.bottom=0;ragged.top=8;ragged.rubble=24;ragged.seed=3;m.breaches.push_back(ragged);
 Breach arch=ragged;arch.x1=3;arch.x2=5;arch.y1=4;arch.y2=5;arch.shape=1;arch.edge=1;arch.rubble=8;m.breaches.push_back(arch);
 Breach square=arch;square.x1=15;square.x2=17;square.shape=0;square.edge=2;square.rubble=0;m.breaches.push_back(square);}
std::shared_ptr<CustomCampaign> room(){auto m=regression::room();m->definition.playerStart={9.5f,15};author(*m);return regression::campaign(m);}
float walk(Vec2 from,float angle){Game g(room());auto& p=LabAccess::player(g);p.pos=from;p.angle=angle;for(int i=0;i<150;++i){InputState in;in.forward=true;g.update(in,1.f/60);}return p.pos.y;}
const bool tube=add({
 .name="breach",.summary="holes broken through walls (BREACH): the wall cut to a ragged, arched or square outline and built back round it, chipped edges, rebar and rubble; the player, the creatures' walk, rounds and light go through",
 .fixtures={"breach"},
 .author=[](Scene&,AuthoredMapData& m){author(m);},
 .capture=[](Scene& s,const Frame&){if(s.last())s.shoot("breach");},
 .metrics=[](Scene& s,Report& r){
  Game g(room());const World& w=g.world();
  bool cut=true;for(int x=8;x<11;++x)cut&=w.tile(x,10)=='.';const bool stands=w.tile(7,10)=='#'&&w.tile(11,10)=='#';
  int chips=0,rebar=0,brick=0,rubble=0;for(const auto& st:w.structures()){if(st.y1>=9.5f&&st.y2<=11.5f&&st.x1>=7.9f&&st.x2<=11.1f){chips+=st.material==15;rebar+=st.material==8;}brick+=st.material==31;}
  for(const auto& f:w.fixtures())rubble+=!f.solid;
  r.metric("chips",chips);r.metric("rebar",rebar);r.metric("brick_chips",brick);r.metric("rubble",rubble);
  r.check("the ragged breach cuts the wall's tiles and leaves the wall either side",cut&&stands);
  r.check("its edge is chipped, rebar hangs from it and rubble lies either side; the arch's edge is brick",chips>=10&&rebar>=4&&rubble>=20&&brick>=4,std::to_string(chips)+" chips, "+std::to_string(rebar)+" bars, "+std::to_string(rubble)+" rubble, "+std::to_string(brick)+" brick");
  const float through=walk({9.5f,12.6f},-kPi*.5f),wall=walk({5,12.6f},-kPi*.5f);
  r.metric("walk_through_y",through,"m");r.metric("walk_at_wall_y",wall,"m");
  r.check("the player walks through the breach; the wall beside it stops him",through<9.f&&wall>11.f,"through to y "+fixed(through)+", at the wall y "+fixed(wall));
  const bool aiThrough=aiWalkSegment(w,{9.5f,12.5f},0,{9.5f,8.5f},1.7f),aiWall=aiWalkSegment(w,{5,12.5f},0,{5,8.5f},1.7f);
  const bool aiArch=aiWalkSegment(w,{4,6},0,{4,2.5f},1.7f),aiSquare=aiWalkSegment(w,{16,6},0,{16,2.5f},1.7f);
  r.check("the creatures' walk test goes through the ragged breach, the arch and the square hole, and not through the wall",aiThrough&&!aiWall&&aiArch&&aiSquare,
   std::string("ragged ")+(aiThrough?"yes":"no")+", wall "+(aiWall?"yes":"no")+", arch "+(aiArch?"yes":"no")+", square "+(aiSquare?"yes":"no"));
  // The creatures' node graph routes round the wall through the breach: from in front of the solid wall to
  // behind it, each waypoint a walk the creatures can make, and the way passes through the hole.
  {const auto walk=[&w](Vec2 a,float feet,Vec2 b,float h,float* end){return aiWalkSegment(w,a,feet,b,h,end);};
   const ai::NodeGraph graph=ai::buildNodeGraph(w,1.7f,walk);Vec2 at{5,13};const Vec2 goal{5,7.5f};bool viaHole=false,walkable=true;int hops=0;
   for(;hops<40&&std::hypot(at.x-goal.x,at.y-goal.y)>.3f;++hops){const Vec2 next=ai::graphWaypoint(graph,at,0,goal,0,1.7f,walk);
    if(std::hypot(next.x-at.x,next.y-at.y)<.01f)break;walkable&=aiWalkSegment(w,at,0,next,1.7f);
    if((at.y-10.5f)*(next.y-10.5f)<=0){const float t=(10.5f-at.y)/(next.y-at.y);const float x=at.x+(next.x-at.x)*t;viaHole|=x>8&&x<11;}at=next;}
   const bool arrived=std::hypot(at.x-goal.x,at.y-goal.y)<=.3f;
   r.metric("route_hops",hops);
   r.check("the creatures' route from one side of the wall to the other goes through the breach, every leg walkable",arrived&&viaHole&&walkable,
    std::string(arrived?"arrived":"did not arrive")+" in "+std::to_string(hops)+" legs, "+(viaHole?"through the hole":"not through the hole"));}
  // Light: the lamp behind the breach sees the floor in front of it through the hole (the probes' and
  // shadows' occlusion ray); the floor in front of the solid wall is in its shadow.
  {Point3 n;const Point3 lamp{9.5f,7.2f,2.8f};const float lit=w.lightRayHit(lamp,{9.5f,13.f,.05f},n),dark=w.lightRayHit(lamp,{4.f,13.f,.05f},n);
   r.check("the lamp's light reaches the floor through the breach, and the wall beside it shades the floor",lit>=.999f&&dark<.999f,"through "+fixed(lit,3)+", beside "+fixed(dark,3));
   // Sight (the creatures' and the guards' eye ray): through the hole at head height, not through the wall.
   const bool see=w.rayClear({9.5f,13},1.5f,{9.5f,7},1.5f,true,true,false,true),seeWall=w.rayClear({5,13},1.5f,{5,7},1.5f,true,true,false,true);
   r.check("eyes see through the breach and not through the wall beside it",see&&!seeWall);}
  const auto hole=LabAccess::trace(g,{9.5f,14,1.3f},{0,-1,0},8),solid=LabAccess::trace(g,{5,14,1.3f},{0,-1,0},8);
  const auto lamp=LabAccess::trace(g,{9.5f,7,2.4f},Point3{0,.9439f,-.3304f},6);
  r.check("rounds and light go through the breach (the lamp behind it reaches the floor in front); the wall stops them",hole.kind!=ShotKind::Solid&&solid.kind==ShotKind::Solid&&lamp.kind!=ShotKind::Solid,
   "hole "+std::to_string(int(hole.kind))+", wall "+std::to_string(int(solid.kind))+", lamp "+std::to_string(int(lamp.kind)));
  // A save and load of a map with breaches keeps the cut walls (they are the map's, not the save's).
  const auto bytes=LabAccess::encodeSave(g);Game back(room());r.check("a save of the room loads back with the breaches in place",LabAccess::decodeSave(back,bytes)&&back.world().tile(9,10)=='.'&&LabAccess::encodeSave(back)==bytes);
 }});
}
}
