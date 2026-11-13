#pragma once
// A tube's scene, as a small text file (lab/fixtures/NAME.lab). One directive a
// line, '#' starts a comment:
//
//   world campaign | ashfall | room | door-room | custom "path.txt" | text
//   map 70                        the campaign / custom map index (default 0)
//   camera X Y Z YAW PITCH        map-local metres, Z -999 = the floor there,
//                                 YAW degrees (0 = +x, 90 = +y), PITCH engine units (1/140 rad)
//   open-doors                    every door open (campaign worlds)
//   scenery-only                  no creatures (campaign worlds)
//   save NAME.sav                 restore a game save (console `labsnap` writes one)
//   state NAME VALUE              set a script state (state-id 0xHEX VALUE for a raw id)
//   spawn creature KIND X Y [Z] [YAWDEG]   Huntsman Wasp Brute Warden Mutant Snatcher Chimera Transformed Machine
//   spawn guard X Y [YAWDEG]      a man (room worlds): a Raider-faction guard standing there
//   light X Y Z [INTENSITY] [RANGE] a ceiling lamp (room worlds)
//   size W H                      render size (default 480 270)
//   frames N / step S             defaults for this tube (--frames/--step override)
//   software                      never on the GPU
//   hud on                        draw the hands and HUD (off by default: the scene alone)
//   begin-campaign ... end-campaign   a whole custom campaign inline (world text)
//
// Anything else is kept as a key/value the features may read (fixture.value("key")).
#include "world/World.h"
#include <map>
#include <string>
#include <vector>
namespace retro::lab {
struct Fixture {
 std::string name,path,world="campaign",customPath,campaignText,save;
 int map=0;bool hasCamera=false,openDoors=false,sceneryOnly=false,software=false;
 Vec2 cameraPos{};float cameraZ=-999,cameraYaw=0,cameraPitch=0; // yaw in radians once parsed
 int width=480,height=270,frames=0;float step=0;
 struct State {StateId id;int value;};std::vector<State> states;
 struct Spawn {std::string what,kind;Vec2 at{};float z=-999,yaw=0;};std::vector<Spawn> spawns;
 struct Lamp {Point3 at{};float intensity=1,range=8;};std::vector<Lamp> lamps;
 std::map<std::string,std::string> values;
 std::string value(const std::string& key,const std::string& fallback="")const{auto it=values.find(key);return it==values.end()?fallback:it->second;}
 float number(const std::string& key,float fallback)const{auto it=values.find(key);return it==values.end()?fallback:std::stof(it->second);}
};
// Throws std::runtime_error with the file and line on a bad directive.
Fixture loadFixture(const std::string& path);
// lab/fixtures/NAME.lab, or a path, or a glob (lab/fixtures/ragdoll-*.lab).
std::vector<std::string> findFixtures(const std::string& patternOrName);
std::string labRoot(); // the checkout root (lab/, src/ ...)
}
