#pragma once
// Vectra's queries: rays, shape casts, overlaps and times of impact against bodies in their true pose.
//
//  - A ray meets each part exactly: an oriented box by its slabs, a cylinder by its round side and caps
//    (the true cylinder, not the narrow phase's prism), a sphere by its round. A compound meets only its
//    parts: a round through the gap under a chair's seat passes it.
//  - A sphere cast and a box cast sweep a shape along a line (a swing, a thick projectile) by conservative
//    advancement on the GJK distance (support functions, so a turned box meets a turned cylinder as it is).
//  - An overlap asks whether a sphere or a box touches a body.
//  - timeOfImpact finds when two bodies moving and turning through a step first meet (conservative
//    advancement with their spin bounded by angle times reach); Space::continuous sweeps fast bodies with it.
//  - Lists of bodies are pruned by each body's bounding sphere (Shape::bound about its centre of mass) before
//    any part is tested, and for a Space by the boxes its last step found.
// Every hit reports the distance along the cast, the point, the outward normal of what was struck, and the
// body and part. Nothing here keeps state: a query is the same on any thread and in any order.
#include "Vectra.h"
#include <functional>
namespace retro::vectra {
struct QueryHit {
 float distance=0;   // along the cast (metres; 0 when it starts inside)
 V3 point{};         // where it met the body (the body's surface)
 V3 normal{};        // the body's outward normal there (against the cast when it starts inside)
 int body=-1,part=-1;
 bool startedInside=false;
};
// Which bodies a query sees (index in the list): return false to skip one.
using QueryFilter=std::function<bool(int body)>;

// ---- One body ----
// A ray from `origin` along unit `direction` up to `maxDistance` against every part of `b`. True on a hit.
bool rayCast(const Body& b,V3 origin,V3 direction,float maxDistance,QueryHit& hit);
// The same against one part.
bool rayCastPart(const Body& b,int part,V3 origin,V3 direction,float maxDistance,QueryHit& hit);
// A sphere of `radius` swept from `origin` along unit `direction`.
bool sphereCast(const Body& b,V3 origin,float radius,V3 direction,float maxDistance,QueryHit& hit);
// An oriented box (half extents `half`, turned by `turn`) swept from centre `origin` along unit `direction`.
bool boxCast(const Body& b,V3 origin,V3 half,Quat turn,V3 direction,float maxDistance,QueryHit& hit);
// Does a sphere, or an oriented box, touch the body? `part` gets the first part touched.
bool overlapSphere(const Body& b,V3 centre,float radius,int* part=nullptr);
bool overlapBox(const Body& b,V3 centre,V3 half,Quat turn,int* part=nullptr);

// ---- Many bodies (pruned by bounding spheres) ----
// The nearest hit over `bodies` (hit.body is its index).
bool rayCast(const std::vector<Body>& bodies,V3 origin,V3 direction,float maxDistance,QueryHit& hit,const QueryFilter& filter={});
bool sphereCast(const std::vector<Body>& bodies,V3 origin,float radius,V3 direction,float maxDistance,QueryHit& hit,const QueryFilter& filter={});
bool boxCast(const std::vector<Body>& bodies,V3 origin,V3 half,Quat turn,V3 direction,float maxDistance,QueryHit& hit,const QueryFilter& filter={});
// Every body a sphere or a box overlaps (indices, in order).
int overlapSphere(const std::vector<Body>& bodies,V3 centre,float radius,std::vector<int>& out,const QueryFilter& filter={});
int overlapBox(const std::vector<Body>& bodies,V3 centre,V3 half,Quat turn,std::vector<int>& out,const QueryFilter& filter={});
// Every hit along a ray, nearest first (a round's passages through light things before what stops it).
int rayCastAll(const std::vector<Body>& bodies,V3 origin,V3 direction,float maxDistance,std::vector<QueryHit>& out,const QueryFilter& filter={});
// Against a Space: its bodies, pruned by the boxes its last step found (Body::boundsLo/Hi) when it has run.
bool rayCast(const Space& space,V3 origin,V3 direction,float maxDistance,QueryHit& hit,const QueryFilter& filter={});

// ---- Distance and time of impact ----
// The distance between two parts (negative: they overlap, then only -0 is known: 0 is returned with overlap
// set), and the nearest point on each.
float partDistance(const Body& a,int partA,const Body& b,int partB,V3& pointA,V3& pointB,bool* overlap=nullptr);
// The least distance between two bodies (over their parts), the points, and the unit normal from a to b.
float bodyDistance(const Body& a,const Body& b,V3& pointA,V3& pointB,V3& normal,bool* overlap=nullptr);
// A body's pose through a step: from (x0,q0) to (x1,q1), the turn about its centre of mass at an even rate.
struct Motion {V3 x0{},x1{};Quat q0{},q1{};
 V3 axis{0,0,1};float angle=0;   // q1 = axisAngle(axis, angle) * q0 (filled by begin)
 void begin();
 void at(float t,V3& x,Quat& q)const;
};
// When, in [0,1] of the step, two moving bodies first come within `target` of each other: conservative
// advancement (their approach along the normal plus each one's angle times its reach bounds how fast the gap
// closes). Returns false when they stay apart all step or already touch at its start. `point` and `normal`
// (from a to b) are where they meet.
bool timeOfImpact(const Body& a,const Motion& ma,const Body& b,const Motion& mb,float target,float& t,V3& point,V3& normal);
// Points on a part's edges, at most `spacing` apart (its corners always; a cylinder's rims and side lines),
// in the body's frame (from its centre of mass): what a turning body leads with into a thin thing.
void edgeProbes(const Body& b,int part,float spacing,std::vector<V3>& out);

// ---- Continuous collision ----
// What Space::continuous runs after a step (`start`: each body's centre of mass when the step began; its turn
// then is its dq undone). A body is fast for its size when its centre went, or its reach turned, more than
// half its least thickness (2 cm at least); a body a joint holds never is (put back alone, it would open its joints).
// RM_CCD=off turns it all off. For each fast body, in index order:
//  - against the world: the points along its edges (corners, every 2.5 cm along edges, a cylinder's rims) ray
//    cast through the step in pieces of at most 0.15 rad of its turn, by Space::sweep; a point that strikes
//    something and went through it, or ends more than 2 cm into it, is a time of impact;
//  - against other bodies (pruned by swept spheres; not pairs the Space's filter or groups rule out, a holding
//    joint joins, the caller meets itself by pairContacts, with a soft body, or going together, the other moving in
//    this one's frame no further than the thinner is thick): a time of impact by conservative advancement, for any
//    pair not already touching when the step began;
//  - the first is answered: the body (and a fast other) back to that moment, and a blow at the point that
//    takes out the approach (restitution as the contacts give it), reported as a ContactEvent.
// The speculative contacts stay the first line; this catches what passes them.
void continuousCollision(Space& space,float dt,const std::vector<V3>& start);
}
