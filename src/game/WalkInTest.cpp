// Walking in (--each-test=walk-in): a player walks into an open shipping container through its mouth, up a
// Builder staircase onto a balcony and along the balcony's railing, by the game's own movement (collision,
// step-ups, the body's hull), not by setting positions. A 24 m room; the container at x 6, the stairs at x 16.
#include "Game.h"
#include "../world/CustomCampaign.h"
#include "../world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <fstream>
namespace retro {
namespace {
std::shared_ptr<const CustomCampaign> walkInCampaign(const std::string& extra){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Walk in|0\nMAP|0|Yard|0|0|6|22|0|0|6|0.8|industrial_night|0|0|0|0|0|-1|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+(y==0||y==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+'\n';
 // The balcony: 3 m up over x 14-20, y 12-20, its stairs arriving at its south... north edge (y 12).
 t+="LAYER|0|1|Balcony|3|0.2\n";
 for(int y=0;y<24;++y){std::string row(24,'.');if(y>=12&&y<20)for(int x=14;x<20;++x)row[size_t(x)]='=';t+="ROW|0|1|"+std::to_string(y)+"|"+row+'\n';}
 return loadCustomCampaignText(t+extra+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","walk-in.rmc");
}
}
bool Game::testWalkIn(){
 std::ofstream report("walk-in-test.txt");int failed=0;
 auto check=[&](bool ok,const std::string&text){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 try{
  // An open container, mouth to the south (+y), its full model size; a stair 0-3 m up y 4-12 at x 16-18.
  const std::string container="FIXTURE|0|"+std::to_string(model::container_open)+"|6|10|0|3.46|11.94|2.5|0|1\n";
  // Side rails on (the rails field): a walker hugging a rail climbs too, never made to jump.
  const std::string stair="STAIR|0|16|4|18|12|0|3|16|1|1|0|0.12|1\n";
  // Walks the player from a point facing an angle (radians) for some seconds, the forward key held.
  auto walk=[](Game& g,Vec2 from,float z,float angle,float seconds){
   auto& p=g.m_player;p.pos=from;p.z=z;p.angle=angle;
   for(float t=0;t<seconds;t+=1.f/60){InputState in{};in.forward=true;g.update(in,1.f/60);}
   return Point3{p.pos.x,p.pos.y,p.z};};
  // A concrete staircase fixture (high end north) and a wall with a doorway, both once a solid box.
  const std::string fixtures="FIXTURE|0|"+std::to_string(model::bld_stairs_concrete)+"|4|18|0|2|4.08|3.2|0|1\n"
                             "FIXTURE|0|"+std::to_string(model::bld_wall_doorway)+"|21|5|0|4|0.42|3|0|1\n"
                             "FIXTURE|0|"+std::to_string(model::bld_doorway_double)+"|12|3|0|4.09|0.2|3|0|1\n"
                             "FIXTURE|0|"+std::to_string(model::cave_oldcity_gateway)+"|21|15|0|3|1.11|3|0|1\n"
                             "FIXTURE|0|"+std::to_string(model::bld_ramp)+"|12|20|0|1.4|4|1|0|1\n"
                             // A wooden staircase model rising east to the balcony's railed west edge (x 14).
                             "FIXTURE|0|"+std::to_string(model::bld_stairs_wood)+"|11.98|13.2|0|2|4.04|3|-1.5707963|1\n";
  Game g(walkInCampaign(container+stair+fixtures));g.m_enemies.clear();g.update({},1.f/60);
  const float kHalf=1.57079632679f;
  // Into the container: from the yard south of it, walking north (-y) through the mouth.
  {const auto at=walk(g,{6.2f,21},0,-kHalf,4.f);
   check(at.y<12.f,"walks into the open container through its mouth (stopped at y "+std::to_string(at.y)+"; its mouth is near y 15.9, its back near y 4.6)");
   check(at.y>4.4f,"and stops at its back wall (y "+std::to_string(at.y)+")");
   const auto side=walk(g,{6.2f,10},0,0,2.f);
   check(side.x<7.7f,"its side wall holds (x "+std::to_string(side.x)+")");}
  // From outside, its side is still a wall.
  {const auto at=walk(g,{10,10},0,kHalf*2,3.f);check(at.x>7.f,"its side stops a walker outside it (x "+std::to_string(at.x)+")");}
  // Up the stairs onto the balcony.
  {const auto at=walk(g,{17,2},0,kHalf,5.f);
   check(at.z>2.7f&&at.y>12.f,"climbs the stairs onto the balcony (reached y "+std::to_string(at.y)+", z "+std::to_string(at.z)+")");}
  {const auto at=walk(g,{16.3f,2},0,kHalf,5.f);
   check(at.z>2.7f&&at.y>12.f,"climbs hugging the stairs' side rail, no jump (reached y "+std::to_string(at.y)+", z "+std::to_string(at.z)+")");}
  {const auto at=walk(g,{17,8},1.5f,0,1.5f);check(at.x<18.f,"the side rail keeps a climber on the stairs (x "+std::to_string(at.x)+")");}
  {const auto at=walk(g,{4,22},0,-kHalf,1.9f);   // up the 4 m flight and onto its top, stopping there (the top is a ledge)
   check(at.z>2.6f&&at.y<17.5f,"climbs a staircase model without a jump (reached y "+std::to_string(at.y)+", z "+std::to_string(at.z)+")");}
  {const auto at=walk(g,{21,9},0,-kHalf,3.f);check(at.y<4.f,"walks through a wall model's doorway (y "+std::to_string(at.y)+")");}
  {const auto at=walk(g,{22.6f,9},0,-kHalf,3.f);check(at.y>5.f,"but not through the wall beside it (y "+std::to_string(at.y)+")");}
  {const auto at=walk(g,{11,6},0,-kHalf,2.f);check(at.y<2.f,"walks through either opening of a double doorway (y "+std::to_string(at.y)+")");}
  {const auto at=walk(g,{12,6},0,-kHalf,2.f);check(at.y>3.2f,"but not through its centre post (y "+std::to_string(at.y)+")");}
  {const auto at=walk(g,{21,18},0,-kHalf,3.f);check(at.y<13.f,"walks through a gateway model (y "+std::to_string(at.y)+")");}
  {const auto at=walk(g,{12,22.8f},0,-kHalf,1.f);check(at.z>.8f,"walks up a ramp model onto its top (z "+std::to_string(at.z)+", y "+std::to_string(at.y)+")");}
  // Along the balcony beside its railing, never through it or snagged on it.
  {const auto at=walk(g,{14.6f,13},3,kHalf,3.f);
   check(at.z>2.7f&&at.y>17.f&&at.x>13.9f,"walks the balcony along its railing (reached y "+std::to_string(at.y)+", x "+std::to_string(at.x)+", z "+std::to_string(at.z)+")");}
  // The railing stops a walker going off the balcony's open edge.
  {const auto at=walk(g,{15,16},3,kHalf*2,2.f);check(at.z>2.7f&&at.x>13.5f,"the railing keeps a walker on the balcony (x "+std::to_string(at.x)+", z "+std::to_string(at.z)+")");}
  // A staircase model arriving at a railed balcony opens the railing where it lands, and only there.
  {const auto at=walk(g,{9,13.2f},0,0,3.f);check(at.z>2.7f&&at.x>14.5f,"climbs a staircase model through the balcony's railing onto it (x "+std::to_string(at.x)+", z "+std::to_string(at.z)+")");}
  {const auto at=walk(g,{15,19},3,kHalf*2,2.f);check(at.z>2.7f&&at.x>13.5f,"the railing still holds beside where the stairs land (x "+std::to_string(at.x)+", z "+std::to_string(at.z)+")");}
  // Models that are not a cube, by their mesh's own shape (WalkShapes.h): a concrete culvert (2 m pipe, 1.56 m inside),
  // an old-city arch (1.4 m opening) and a service tunnel (2.8 m inside), their length along y.
  {const std::string shaped="FIXTURE|0|"+std::to_string(model::bld_culvert)+"|6|8|0|2|4.1|2|0|1\n"
                            "FIXTURE|0|"+std::to_string(model::cave_oldcity_arch)+"|16|8|0|3|0.6|1.8|0|1\n"
                            "FIXTURE|0|"+std::to_string(model::bld_service_tunnel)+"|6|17|0|2|4|2.9|0|1\n";
   Game h(walkInCampaign(shaped));h.m_enemies.clear();h.update({},1.f/60);
   float peak=0;   // the highest the last walk got
   auto stance=[&peak](Game& g,Vec2 from,float angle,float seconds,bool crouch){
    auto& p=g.m_player;p.pos=from;p.z=0;p.angle=angle;p.crouched=false;p.prone=false;p.grounded=true;g.m_velocity={};peak=0;
    for(float t=0;t<seconds;t+=1.f/60){InputState in{};in.crouch=crouch;in.forward=t>.6f;g.update(in,1.f/60);peak=std::max(peak,p.z);}
    return Point3{p.pos.x,p.pos.y,p.z};};
   {const auto at=stance(h,{6,13},-kHalf,4.f,false);check(at.y>9.5f,"standing, a walker does not fit in the culvert (stopped at y "+std::to_string(at.y)+"; its mouth is at y 10.05)");}
   {const auto at=stance(h,{6,13},-kHalf,7.f,true);check(at.y<5.5f,"crouched, a walker goes through the culvert end to end (reached y "+std::to_string(at.y)+", z "+std::to_string(at.z)+")");
    check(at.z<.35f,"on its floor, not on its roof (z "+std::to_string(at.z)+")");}
   {const auto at=stance(h,{9.5f,8},kHalf*2,3.f,false);check(at.x>7.f,"the culvert's side is a wall from outside (x "+std::to_string(at.x)+")");}
   {const auto at=stance(h,{16,11},-kHalf,3.f,false);check(at.y>8.2f,"standing, a walker does not fit under the low arch (y "+std::to_string(at.y)+")");}
   {const auto at=stance(h,{16,11},-kHalf,4.f,true);check(at.y<7.f,"crouched, a walker passes under the low arch (y "+std::to_string(at.y)+")");}
   {const auto at=stance(h,{17.25f,11},-kHalf,3.f,true);check(at.y>8.2f,"but not through the arch's pier (y "+std::to_string(at.y)+")");}
   {const auto at=stance(h,{6,21.5f},-kHalf,5.f,false);check(at.y<14.5f,"standing, a walker walks through the service tunnel (reached y "+std::to_string(at.y)+")");}
   // At any turn and size: the culvert turned 30 degrees, a concrete staircase turned 45, and the culvert made
   // 1.3 times wider and longer (its floor and roof where they were).
   const float a30=.5235988f,a45=.7853982f;
   const std::string turned="FIXTURE|0|"+std::to_string(model::bld_culvert)+"|7|8|0|2|4.1|2|"+std::to_string(a30)+"|1\n"
                            "FIXTURE|0|"+std::to_string(model::bld_stairs_concrete)+"|16|11|0|2|4.08|3.2|"+std::to_string(a45)+"|1\n"
                            "FIXTURE|0|"+std::to_string(model::bld_culvert)+"|7|17|0|2.6|5.3|2|0|1\n";
   Game r(walkInCampaign(turned));r.m_enemies.clear();r.update({},1.f/60);
   // Along a fixture's own depth axis (world (sin yaw, cos yaw)), from `ahead` metres out on its + side, walking toward -.
   auto along=[&](Vec2 centre,float yaw,float ahead,float seconds,bool crouch){const Vec2 axis{std::sin(yaw),std::cos(yaw)};
    const auto at=stance(r,centre+axis*ahead,std::atan2(-axis.y,-axis.x),seconds,crouch);const Vec2 d=Vec2{at.x,at.y}-centre;return std::pair<float,float>{d.x*axis.x+d.y*axis.y,at.z};};
   {const auto [s,z]=along({7,8},a30,4.f,4.f,false);check(s>2.f,"turned 30 degrees, the culvert still stops a standing walker at its mouth ("+std::to_string(s)+" m along it)");}
   {const auto [s,z]=along({7,8},a30,4.f,8.f,true);check(s<-2.3f,"turned 30 degrees, a crouched walker goes through it end to end ("+std::to_string(s)+" m along it)");}
   {const auto [s,z]=along({16,11},a45,3.6f,5.f,false);(void)z;check(peak>2.6f,"turned 45 degrees, a concrete staircase is climbed without a jump (to z "+std::to_string(peak)+", "+std::to_string(s)+" m along it)");}
   {const auto [s,z]=along({7,17},0,4.5f,8.f,true);check(s<-2.8f&&peak<.4f,"made 1.3 times wider and longer, the culvert is crouched through on its floor ("+std::to_string(s)+" m along it, highest z "+std::to_string(peak)+")");}}
 }catch(const std::exception& e){check(false,std::string("exception: ")+e.what());}
 report<<(failed?"FAIL":"PASS")<<'\n';return failed==0;
}
}
