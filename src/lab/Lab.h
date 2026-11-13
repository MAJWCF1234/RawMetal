#pragma once
// The feature lab (lab/README.md). A feature is plugged into a
// tube: a small saved scene (lab/fixtures/*.lab) that the runner builds, steps,
// renders and measures in seconds, alone or together with the features it talks
// to (its friends). A tube file (src/lab/tubes/*.cpp) registers one Feature:
//
//   static const bool registered=lab::add({
//    .name="ragdoll",.summary="human bodies settle over an edge",
//    .friends={"blood","chimera"},.fixtures={"ragdoll-ledge"},
//    .setup=[](lab::Scene& s){...},             // stage it on the built Game
//    .step=[](lab::Scene& s,float dt){...},      // before each Game::update
//    .metrics=[](lab::Scene& s,lab::Report& r){r.metric("lowest_m",x);r.check("on the floor",ok);},
//    .reporter=[](lab::Scene& s,lab::Overlay& o){o.box(lo,hi,0xff4040);}, // its invisible state, drawn
//    .with={{"blood",[](lab::Scene& s,lab::Report& r){...}}}});          // checks when blood is in the tube too
//
// Everything a feature needs from the engine's privates goes through LabAccess
// (LabAccess.h; Game and SoftwareRenderer name it a friend).
#include "game/Game.h"
#include "renderer/SoftwareRenderer.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace retro::lab {
struct Fixture;
// One number a tube measured, and one pass/fail it decided.
struct Metric {std::string name;double value=0;std::string unit;};
struct Check {std::string name;bool ok=false;std::string detail;std::string owner;};
struct Report {
 std::string owner; // the feature (or "feature+friend") filling it
 std::vector<Metric> metrics;std::vector<Check> checks;
 void metric(const std::string& name,double value,const std::string& unit=""){metrics.push_back({owner.empty()?name:owner+"."+name,value,unit});}
 void check(const std::string& name,bool ok,const std::string& detail=""){checks.push_back({name,ok,detail,owner});}
 bool passed()const{for(const auto&c:checks)if(!c.ok)return false;return true;}
};
// The reporter overlay: a feature's invisible state drawn over a captured frame
// in world space (projected with the frame's own camera) or screen space. The
// lab draws it on a copy of the frame: the engine's look is never touched.
struct Overlay {
 struct Line {Point3 a,b;std::uint32_t rgb;bool world=true;};
 struct Dot {Point3 p;std::uint32_t rgb;int size=3;bool world=true;};
 struct Label {Point3 p;std::string text;std::uint32_t rgb;bool world=true;};
 std::vector<Line> lines;std::vector<Dot> dots;std::vector<Label> labels;
 void line(Point3 a,Point3 b,std::uint32_t rgb){lines.push_back({a,b,rgb});}
 void point(Point3 p,std::uint32_t rgb,int size=3){dots.push_back({p,rgb,size});}
 void label(Point3 p,const std::string& text,std::uint32_t rgb=0xffffff){labels.push_back({p,text,rgb});}
 void text(int x,int y,const std::string& text,std::uint32_t rgb=0xffffff){labels.push_back({{float(x),float(y),0},text,rgb,false});}
 void box(Point3 lo,Point3 hi,std::uint32_t rgb){
  const Point3 c[8]={{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}};
  for(int i=0;i<4;++i){line(c[i],c[(i+1)%4],rgb);line(c[4+i],c[4+(i+1)%4],rgb);line(c[i],c[4+i],rgb);}}
 void cross(Point3 p,float r,std::uint32_t rgb){line({p.x-r,p.y,p.z},{p.x+r,p.y,p.z},rgb);line({p.x,p.y-r,p.z},{p.x,p.y+r,p.z},rgb);line({p.x,p.y,p.z-r},{p.x,p.y,p.z+r},rgb);}
 bool empty()const{return lines.empty()&&dots.empty()&&labels.empty();}
};
// A frame as rendered: pixels (0xAARRGGBB), how long render() took.
struct Frame {const std::uint32_t* pixels=nullptr;int width=0,height=0,index=0;double renderMs=0,updateMs=0;};
// The scene a tube runs: the fixture it came from, the Game built from it, the
// renderer drawing it and a blackboard the features in the tube share.
struct Scene {
 const Fixture* fixture=nullptr;
 std::unique_ptr<Game> game;
 SoftwareRenderer* renderer=nullptr;
 // The authored room the fixture asked for (map room/door-room/text), before the Game is built;
 // features add to it in Feature::author. Null for campaign maps.
 std::shared_ptr<AuthoredMapData> map;
 std::vector<std::string> features; // every feature in this tube
 int frame=0;float time=0,dt=1.f/60;
 InputState input;   // what the player does this frame: cleared before the steps, given to Game::update
 std::map<std::string,double> board;          // numbers features hand each other
 std::map<std::string,std::vector<Point3>> points; // places features hand each other (bodies, pools...)
 // Extra pictures a feature takes (a debug render, an A/B frame): saved beside the tube's frames.
 struct Shot {std::string label;int width=0,height=0;std::vector<std::uint32_t> pixels;};
 std::vector<Shot> shots;
 void shoot(const std::string& label){if(renderer)shots.push_back({label,renderer->width(),renderer->height(),std::vector<std::uint32_t>(renderer->pixels(),renderer->pixels()+size_t(renderer->width())*renderer->height())});}
 bool last()const;   // the tube's last frame (for capture)
 int frames=0;       // how many frames this pass runs
 bool has(const std::string& feature)const{for(const auto&f:features)if(f==feature)return true;return false;}
 Game& g(){return *game;}
};
struct Feature {
 std::string name,summary;
 std::vector<std::string> friends;   // features it talks to: --with-friends runs it alone, with each, and with all
 std::vector<std::string> fixtures;  // the tubes it runs in by default (lab/fixtures/NAME.lab)
 std::function<void(Scene&,AuthoredMapData&)> author;       // add to an authored room before the Game is built
 std::function<void(Scene&)> setup;                         // stage on the built Game
 std::function<void(Scene&,float)> step;                    // every frame, before Game::update
 std::function<void(Scene&,const Frame&)> capture;          // every frame, after render
 std::function<void(Scene&,Report&)> metrics;               // after the last frame
 std::function<void(Scene&,Overlay&)> reporter;             // its debug view (--reporter)
 std::map<std::string,std::function<void(Scene&,Report&)>> with; // checks for when a friend shares the tube
 std::function<void(Scene&)> teardown;                      // undo global switches (always called)
 bool needsGpu=false; // skipped (not failed) with --software
 // An engine test run as it is (no scene): its pass is the tube's one check. See tubes/EngineTests.cpp.
 std::function<bool()> standalone;
};
bool add(Feature feature);
const std::vector<Feature>& features();
const Feature* find(const std::string& name);
// The live tuning knob, re-read while running (core/Tune.h).
float tuned(const char* name,float fallback);
// Screen-space helpers for checks: project a world point with the frame's camera.
struct Projected {float x=0,y=0,depth=0;bool visible=false;};
Projected project(const Scene& scene,Point3 world);
inline bool Scene::last()const{return frame==frames-1;}
}
