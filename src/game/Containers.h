#pragma once
// Props inside props: open-top containers (a basket, an open crate or box, a bin, a cart's tray) hold
// what is put in them. The container's model carries an inner volume (model::container: inner floor,
// rim and wall thickness in the model's own metres, scaled with the prop's dims). What lies in it is
// ordinary clutter: nothing links the two, so saves, chunk crossings and authored maps need nothing
// new. Each step works out what is inside what from where things are:
//  - a prop over the inner floor rests on it (supportAt), held in by the walls (carry clamps it);
//  - a moving container (carried, shoved, thrown, tipping) takes its contents along rigidly; they
//    ride (Clutter::riding: skipped by the solver for that step);
//  - tipped past its spill angle, broken, or burst in the hands, it lets them go.
#include "Game.h"
namespace retro::containers {
// The inner volume in metres: floor and rim above the prop's base, the inner half-widths along its
// own x and y, and the tilt past which it spills.
struct Inner {float floor=0,rim=0,halfX=0,halfY=0,spill=0;};
bool isContainer(const Clutter& c);
Inner inner(const Clutter& c);
// Tilted less than its spill angle (a tray spills at a lean, a bin almost never).
bool upright(const Clutter& c);
// p (the list's coordinates) lies over the container's inner floor.
bool overInner(const Clutter& box,Vec2 p);
// item lies in box: its middle over the inner floor, small enough to go in, its base between the
// inner floor and the rim. `ignoreBroken` asks of a container breaking this step.
bool holds(const Clutter& box,const Clutter& item,bool ignoreBroken=false);
// What `v` offers to stand on at p for something whose base is at `feet` (-100: nothing): its inner
// floor over the opening, else its top (supportHeight), as the solver always had it.
float supportAt(const Clutter& v,Vec2 p,float feet);
struct Rider {int box,item;Point3 local;float yaw;};
struct Snapshot {std::vector<Rider> riders;std::vector<std::pair<int,Clutter>> boxes;};
// Start of a step: who is in what (innermost container wins), and each container's pose.
// onlyHeld (the rigid core, physics/PropBodies.h): a container is its floor and walls there, so what lies
// in it rests, slides, rides and pours out by contact; only the container in the hand carries its contents
// rigidly (the hand moves it as the player's look does, not as a body).
Snapshot capture(std::vector<Clutter>& list,int held,bool onlyHeld=false);
// End of a step: contents follow a container that moved, spill from one that tipped or broke, and
// are held in by the walls of one that stayed put.
void carry(std::vector<Clutter>& list,const Snapshot& snapshot,int held);
// A container is breaking: wake what it held and let it fall.
void release(std::vector<Clutter>& list,size_t box);
// After seeding a map: a prop authored inside a container (at the floor below it, or at -999) is
// set on its inner floor and sleeps there.
void settleAuthored(std::vector<Clutter>& list);
}
