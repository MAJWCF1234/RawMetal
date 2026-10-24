#include "Game.h"
#include <fstream>
namespace retro {
bool Game::testGantry(){
 std::ofstream report("gantry-test.txt");World world(2);auto spans=world.spansAt(6,2);
 if(world.layers().size()!=2||world.layers()[0].name!="Turbine Gantry / ground"||world.layers()[1].name!="Turbine Gantry / upper catwalk"||world.layers()[1].elevation!=3)return false;
 for(int level=0;level<3;++level){World map(level);for(const auto&layer:map.layers())for(auto row:layer.rows)if(row.size()!=World::Width)return false;
  for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(map.tile(x,y)=='G'){
   // Generators (6) own G tiles; the Turbine Gantry's bays hold turbine casings (46).
   int owners=0;for(auto&f:map.fixtures())if((f.model==6||f.model==46)&&std::fabs(x+.5f-f.position.x)<f.width*.5f&&std::fabs(y+.5f-f.position.y)<f.depth*.5f)++owners;
   if(owners!=1){report<<"Missing/overlapping generator at "<<level<<':'<<x<<','<<y;return false;}
  }
  for(auto&f:map.fixtures())if(f.model==46){float floor=map.floorHeight(f.position.x,f.position.y);if(map.fits(f.position.x,f.position.y,floor,.1f))return false;}
  for(auto&f:map.fixtures())if(f.model==6){
   if(std::fabs(f.width/f.depth-3.4f)>.001f||std::fabs(f.height/f.depth-1.8f)>.001f)return false;
   float floor=map.floorHeight(f.position.x,f.position.y);
   for(float x:{-.49f,.49f})for(float y:{-.49f,.49f})if(std::fabs(map.floorHeight(f.position.x+x*f.width,f.position.y+y*f.depth)-floor)>.001f)return false;
   if(map.fits(f.position.x,f.position.y,floor,.1f)||!map.fits(f.position.x,f.position.y,floor+f.height,.1f))return false;
   if(std::fabs(map.supportHeight(f.position.x,f.position.y+f.depth*.5f+.02f)-floor)>.001f)return false;
  }
 }
 report<<"All machine tiles rendered once, original generator proportions, level feet and matching collision: PASS\n";
 report<<"Spans at 6,2:";for(auto& span:spans)report<<' '<<span.floor<<'-'<<span.ceiling;report<<'\n';
 if(spans.size()!=2||spans[0].floor!=0||spans[0].ceiling!=2.7f||spans[1].floor!=3)return false;
 // Ground transfers remain walkable, while the same perimeter openings are
 // physically closed above their 2.5 m headers. Interior catwalk rails remain
 // low/jumpable, so parkour is preserved without exposing the outside void.
 if(!world.fits(3.5f,.08f,0,1.f)||world.fits(3.5f,.08f,3,1.f)){report<<"Entry shell open above header or closed below\n";return false;}
 if(!world.fits(21.5f,23.92f,0,1.f)||world.fits(21.5f,23.92f,3,1.f)){report<<"Transfer shell open above header or closed below\n";return false;}
 bool jumpableRail=false;for(const auto&s:world.structures())if(s.rail&&s.bottom>2.9f&&s.top-s.bottom<.7f){jumpableRail=true;break;}
 if(!jumpableRail){report<<"No jumpable catwalk rail\n";return false;}
 auto game=mapInspection({6.5f,2.5f},0,0,2);game.m_enemies.clear();
 for(int i=0;i<60;++i)game.update({},1.f/120);if(game.player().z!=0){report<<"Ground span z "<<game.player().z<<'\n';return false;}
 game.m_player.z=3;for(int i=0;i<60;++i)game.update({},1.f/120);if(game.player().z!=3){report<<"Deck span z "<<game.player().z<<'\n';return false;}
 report<<"Two traversable spans at identical XY: PASS\n";
 game=mapInspection({21.5f,20.5f},kPi*.5f,0,2);game.m_enemies.clear();InputState use{};use.use=true;game.update(use,.01f);
 if(game.world().doors().back().opening){report<<"Exit opened before gantry control\n";return false;}
 game=mapInspection({3.5f,1.5f},kPi*.5f,0,2);game.m_enemies.clear();game.m_pickups.clear();
 // Hall floor: entry lane, past the main set, between the crates and the west
 // auxiliary unit, to the foot of the west stair.
 const Vec2 groundRoute[]={{3.5f,5.6f},{8.f,5.6f},{8.f,12.4f},{4.6f,12.4f},{4.6f,16.5f},{2.f,16.5f}};
 for(auto target:groundRoute){int tick=0;for(;tick<1800&&length(target-game.player().pos)>.13f;++tick){auto delta=target-game.player().pos;InputState input{};input.forward=true;input.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game.player().angle)/.0022f;game.update(input,1.f/120);}
  report<<"Ground waypoint "<<target.x<<','<<target.y<<" reached "<<game.player().pos.x<<','<<game.player().pos.y<<'\n';if(tick==1800)return false;
 }
 // Up the west stair, round the north run to the control room's north door,
 // out across the bridge and back in through its west door to the console.
 const Vec2 route[]={{2.f,10.5f},{2.f,2.f},{20.7f,2.f},{20.7f,4.6f},{18.f,7.f},{4.f,7.f},{18.f,7.f},{19.5f,5.2f}};
 for(auto target:route){int tick=0;for(;tick<1800&&length(target-game.player().pos)>.13f;++tick){auto delta=target-game.player().pos;InputState input{};input.forward=true;input.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game.player().angle)/.0022f;game.update(input,1.f/120);}
  report<<"Waypoint "<<target.x<<','<<target.y<<" reached "<<game.player().pos.x<<','<<game.player().pos.y<<','<<game.player().z<<" ticks "<<tick<<'\n';if(tick==1800)return false;
 }
 if(std::fabs(game.player().z-3)>.01f)return false;
 game.m_player.angle=-kPi*.5f;game.update(use,.01f);if(!game.world().controlReleased()||game.activeLog()<0){report<<"Gantry control not released from the console\n";return false;}
 game.update({},.01f);game.update(use,.01f);if(game.activeLog()!=-1||game.logTime()!=0){report<<"Console log not dismissed\n";return false;}
 // Drop off the deck through an open cell: gravity must select the lower span.
 game.m_player.pos={12.f,12.5f};game.m_player.z=3;game.m_velocity={};game.m_player.grounded=false;
 for(int i=0;i<180;++i)game.update({},1.f/120);if(game.player().z!=0){report<<"Drop landed at z "<<game.player().z<<'\n';return false;}
 game.m_player.pos={21.5f,20.5f};game.m_player.angle=kPi*.5f;game.update({},.01f);game.update(use,.01f);if(!game.world().doors().back().opening){report<<"Exit did not open after release\n";return false;}
 for(int i=0;i<200;++i)game.update({},1.f/120);game.m_player.pos={21.5f,22.5f};game.update({},.01f);if(game.won())return false;
 game.m_player.pos={21.5f,24.1f};game.crossChunkBoundary();if(game.level()!=3||game.player().z!=0){report<<"Transfer arrived at level "<<game.level()<<" z "<<game.player().z<<'\n';return false;}
 report<<"Stairs, full connected catwalk route, jumpable rails, sealed upper transfer shell, upper terminal, E dismissal, gravity drop, locked/unlocked extraction: PASS\n";return true;
}
}
