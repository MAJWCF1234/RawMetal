#include "Game.h"
#include <fstream>
namespace retro {
bool Game::testGantry(){
 std::ofstream report("gantry-test.txt");World world(2);auto spans=world.spansAt(6,2);
 if(world.layers().size()!=2||world.layers()[0].name!="Turbine Gantry / ground"||world.layers()[1].name!="Turbine Gantry / upper catwalk"||world.layers()[1].elevation!=3)return false;
 for(int level=0;level<3;++level){World map(level);for(const auto&layer:map.layers())for(auto row:layer.rows)if(row.size()!=World::Width)return false;
  for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(map.tile(x,y)=='G'){
   int owners=0;for(auto&f:map.fixtures())if(f.model==6&&std::fabs(x+.5f-f.position.x)<f.width*.5f&&std::fabs(y+.5f-f.position.y)<f.depth*.5f)++owners;
   if(owners!=1){report<<"Missing/overlapping generator at "<<level<<':'<<x<<','<<y;return false;}
  }
  for(auto&f:map.fixtures())if(f.model==6){
   if(std::fabs(f.width/f.depth-3.4f)>.001f||std::fabs(f.height/f.depth-1.8f)>.001f)return false;
   float floor=map.floorHeight(f.position.x,f.position.y);
   for(float x:{-.49f,.49f})for(float y:{-.49f,.49f})if(std::fabs(map.floorHeight(f.position.x+x*f.width,f.position.y+y*f.depth)-floor)>.001f)return false;
   if(map.fits(f.position.x,f.position.y,floor,.1f)||!map.fits(f.position.x,f.position.y,floor+f.height,.1f))return false;
   if(std::fabs(map.supportHeight(f.position.x,f.position.y+f.depth*.5f+.02f)-floor)>.001f)return false;
  }
 }
 report<<"All machine tiles rendered once, original generator proportions, level feet and matching collision: PASS\n";
 if(spans.size()!=2||spans[0].floor!=0||spans[0].ceiling!=2.7f||spans[1].floor!=3)return false;
 // Ground transfers remain walkable, while the same perimeter openings are
 // physically closed above their 2.5 m headers. Interior catwalk rails remain
 // low/jumpable, so parkour is preserved without exposing the outside void.
 if(!world.fits(3.5f,.08f,0,1.f)||world.fits(3.5f,.08f,3,1.f))return false;
 if(!world.fits(21.5f,23.92f,0,1.f)||world.fits(21.5f,23.92f,3,1.f))return false;
 bool jumpableRail=false;for(const auto&s:world.structures())if(s.rail&&s.bottom>2.9f&&s.top-s.bottom<.7f){jumpableRail=true;break;}
 if(!jumpableRail)return false;
 auto game=mapInspection({6.5f,2.5f},0,0,2);game.m_enemies.clear();
 for(int i=0;i<60;++i)game.update({},1.f/120);if(game.player().z!=0)return false;
 game.m_player.z=3;for(int i=0;i<60;++i)game.update({},1.f/120);if(game.player().z!=3)return false;
 report<<"Two traversable spans at identical XY: PASS\n";
 game=mapInspection({21.5f,20.5f},kPi*.5f,0,2);game.m_enemies.clear();InputState use{};use.use=true;game.update(use,.01f);
 if(game.world().doors().back().opening)return false;
 game=mapInspection({3.5f,1.5f},kPi*.5f,0,2);game.m_enemies.clear();game.m_pickups.clear();
 const Vec2 groundRoute[]={{3.5f,4.5f},{7.5f,4.5f},{7.5f,13.5f},{3.5f,13.5f},{3.5f,15.5f},{6.5f,15.5f},{6.5f,17.4f},{4.8f,17.4f}};
 for(auto target:groundRoute){int tick=0;for(;tick<1800&&length(target-game.player().pos)>.13f;++tick){auto delta=target-game.player().pos;InputState input{};input.forward=true;input.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game.player().angle)/.0022f;game.update(input,1.f/120);}
  report<<"Ground waypoint "<<target.x<<','<<target.y<<" reached "<<game.player().pos.x<<','<<game.player().pos.y<<'\n';if(tick==1800)return false;
 }
 const Vec2 route[]={{4.8f,22.5f},{6.5f,22.5f},{6.5f,20.5f},{13.5f,20.5f},{13.5f,17.5f},{8.5f,17.5f},{8.5f,14.5f},{3.5f,14.5f},{3.5f,9.5f},{6.5f,9.5f},{6.5f,2.5f},{15.5f,2.5f},{15.5f,6.5f},{13.5f,6.5f},{13.5f,5.5f},{10.5f,5.5f},{10.5f,11.5f},{17.5f,11.5f},{17.5f,9.5f}};
 for(auto target:route){int tick=0;for(;tick<1800&&length(target-game.player().pos)>.13f;++tick){auto delta=target-game.player().pos;InputState input{};input.forward=true;input.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game.player().angle)/.0022f;game.update(input,1.f/120);}
  report<<"Waypoint "<<target.x<<','<<target.y<<" reached "<<game.player().pos.x<<','<<game.player().pos.y<<','<<game.player().z<<" ticks "<<tick<<'\n';if(tick==1800)return false;
 }
 if(std::fabs(game.player().z-3)>.01f)return false;
 game.m_player.angle=0;game.update(use,.01f);if(!game.world().controlReleased()||game.activeLog()<0)return false;
 game.update({},.01f);game.update(use,.01f);if(game.activeLog()!=-1||game.logTime()!=0)return false;
 // Drop off the deck through an open cell: gravity must select the lower span.
 game.m_player.pos={18.5f,12.5f};game.m_player.z=3;game.m_velocity={};game.m_player.grounded=false;
 for(int i=0;i<180;++i)game.update({},1.f/120);if(game.player().z!=0)return false;
 game.m_player.pos={21.5f,20.5f};game.m_player.angle=kPi*.5f;game.update({},.01f);game.update(use,.01f);if(!game.world().doors().back().opening)return false;
 for(int i=0;i<200;++i)game.update({},1.f/120);game.m_player.pos={21.5f,22.5f};game.update({},.01f);if(game.won())return false;
 game.m_player.pos={21.5f,24.1f};game.crossChunkBoundary();if(game.level()!=3||game.player().z!=0)return false;
 report<<"Stairs, full connected catwalk route, jumpable rails, sealed upper transfer shell, upper terminal, E dismissal, gravity drop, locked/unlocked extraction: PASS\n";return true;
}
}
