#pragma once
// The rigid-body core (phase 1 of the prop physics: docs in Vectra.cpp's head).
//
//  - A Body is a rigid body: its centre of mass, orientation (a unit quaternion), linear and angular
//    velocity, mass and inertia tensor (from its Shape and mass), and a Material (static and dynamic
//    friction, restitution, rolling resistance).
//  - A Shape is up to eight convex parts (box, cylinder, sphere) in the body's own frame: a crate is one
//    box, a barrel one cylinder, a chair a compound of boxes, an open crate a floor and four walls.
//  - The narrow phase finds Manifolds (up to four points, one normal) between two bodies' parts: the
//    separating axis test over faces and edge pairs of convex polyhedra (a cylinder is a 12-sided prism
//    there), with the incident face clipped to the reference face for a full manifold; spheres exactly.
//    Contacts against the static world come from the caller (Space::worldContacts): the game asks its
//    own World for them (RigidClutter.cpp), a lab tube asks a plane.
//  - Space::step runs one fixed step: contacts are found once, then `substeps` soft substeps (TGS-soft,
//    as Box2D v3): integrate velocities, warm start, solve with a soft bias, integrate positions, relax
//    without bias; restitution last. Speculative contacts and a swept test against the world keep fast
//    bodies from passing through thin things. Bodies sleep by islands and wake on contact.
//  - Everything is in fixed order (body index, part index, point index): the same inputs give the same
//    bits on every run.
//
// Phase 2 (joints) plugs into Constraint below: a joint is a set of rows between two bodies (or a body
// and the world) solved in the same substep loop as the contacts, before them, and islands link through
// it. Phase 3 (gravity gloves) drives a body with Space::drive (a soft spring to a target pose that
// respects contacts) rather than teleporting it.
#include "../../core/Math.h"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
namespace retro::vectra {
using V3=Point3;
inline V3 v3(float x,float y,float z){return {x,y,z};}
inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float len(V3 a){return std::sqrt(dot(a,a));}
inline float lenSq(V3 a){return dot(a,a);}
inline V3 neg(V3 a){return {-a.x,-a.y,-a.z};}
inline V3 norm(V3 a,V3 fallback={0,0,1}){const float l=len(a);return l>1e-9f?a*(1/l):fallback;}
inline V3 mulv(V3 a,V3 b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
inline float comp(V3 a,int i){return i==0?a.x:i==1?a.y:a.z;}
// Two unit vectors at right angles to n (a tangent frame).
inline void basis(V3 n,V3& t1,V3& t2){if(std::fabs(n.x)>=.57735f)t1=norm({n.y,-n.x,0});else t1=norm({0,n.z,-n.y});t2=cross(n,t1);}

struct M3 {
 float m[3][3]{{1,0,0},{0,1,0},{0,0,1}};
 V3 operator*(V3 v)const{return {m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z,m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z,m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z};}
 M3 operator*(const M3& b)const{M3 r;for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.m[i][j]=m[i][0]*b.m[0][j]+m[i][1]*b.m[1][j]+m[i][2]*b.m[2][j];return r;}
 M3 operator+(const M3& b)const{M3 r;for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.m[i][j]=m[i][j]+b.m[i][j];return r;}
 M3 operator*(float s)const{M3 r;for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.m[i][j]=m[i][j]*s;return r;}
 M3 transposed()const{M3 r;for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.m[i][j]=m[j][i];return r;}
 V3 column(int j)const{return {m[0][j],m[1][j],m[2][j]};}
 static M3 diagonal(V3 d){M3 r;r.m[0][0]=d.x;r.m[1][1]=d.y;r.m[2][2]=d.z;return r;}
 static M3 zero(){M3 r;r.m[0][0]=r.m[1][1]=r.m[2][2]=0;return r;}
 M3 inverse()const;
};

struct Quat {
 float w=1,x=0,y=0,z=0;
 Quat operator*(const Quat& b)const{return {w*b.w-x*b.x-y*b.y-z*b.z,w*b.x+x*b.w+y*b.z-z*b.y,w*b.y-x*b.z+y*b.w+z*b.x,w*b.z+x*b.y-y*b.x+z*b.w};}
 Quat conj()const{return {w,-x,-y,-z};}
 V3 rotate(V3 v)const{const V3 u{x,y,z};const V3 t=cross(u,v)*2;return v+t*w+cross(u,t);}
 V3 unrotate(V3 v)const{return conj().rotate(v);}
 Quat normalized()const{const float l=std::sqrt(w*w+x*x+y*y+z*z);return l>1e-12f?Quat{w/l,x/l,y/l,z/l}:Quat{};}
 M3 matrix()const;
 static Quat axisAngle(V3 axis,float angle){const float s=std::sin(angle*.5f);const V3 a=norm(axis);return {std::cos(angle*.5f),a.x*s,a.y*s,a.z*s};}
 // The game's Euler angles (Clutter::rotate): R = Rz(yaw) * Rx(roll) * Ry(pitch).
 static Quat fromEuler(float yaw,float pitch,float roll);
 void toEuler(float& yaw,float& pitch,float& roll)const;
};
// Turn q by angular velocity w (world) over dt (first order, renormalised).
inline Quat integrate(const Quat& q,V3 w,float dt){const Quat spin{0,w.x,w.y,w.z};Quat d=spin*q;const float h=dt*.5f;return Quat{q.w+d.w*h,q.x+d.x*h,q.y+d.y*h,q.z+d.z*h}.normalized();}

// ---- Shapes ---------------------------------------------------------------------------------------
enum class PartKind:std::uint8_t {Box,Cylinder,Sphere};
// A convex part in the body's frame (origin: the middle of the prop's box, as Clutter has it).
// Box: half extents. Cylinder: radius half.x (= half.y), half height half.z along its own z. Sphere: radius half.x.
struct Part {PartKind kind=PartKind::Box;V3 centre{};Quat turn{};V3 half{.1f,.1f,.1f};};
struct Shape {
 std::array<Part,8> parts{};int count=0;
 V3 com{};            // centre of mass in the body frame (from the parts' volumes)
 M3 inertia{};        // per kilogram, about the centre of mass, in the body frame
 float bound=0;       // radius of a sphere about the centre of mass holding every part
 bool hollow=false;   // an open container (its inner volume is open)
 // Fill com, inertia and bound from the parts (uniform density).
 void finish();
 Part& add(PartKind kind,V3 centre,V3 half,Quat turn={}){auto& p=parts[size_t(count++)];p={kind,centre,turn,half};return p;}
};

// rolling: the rolling resistance as a length (the resisting torque over the load: the give of the surfaces
// under it, so a bottle stops sooner than a drum), and on twisting too.
struct Material {float staticFriction=.6f,dynamicFriction=.45f,restitution=.2f,rolling=.002f;
 // Two materials meeting: friction as the geometric mean, restitution the larger (a ball bounces off a floor).
 static Material mix(const Material& a,const Material& b){return {std::sqrt(a.staticFriction*b.staticFriction),std::sqrt(a.dynamicFriction*b.dynamicFriction),std::max(a.restitution,b.restitution),std::max(a.rolling,b.rolling)};}
};

// ---- Bodies ---------------------------------------------------------------------------------------
struct Body {
 V3 x{};Quat q{};V3 v{},w{};          // centre of mass (world), orientation, velocities (world)
 float invMass=1;M3 invInertiaLocal=M3::zero();M3 invInertia=M3::zero(); // world inverse inertia (updated each step)
 M3 inertiaLocal=M3::zero();                                              // body-frame inertia (the gyroscopic step)
 const Shape* shape=nullptr;Material material{};
 bool kinematic=false;   // moved by the caller (a held prop): infinite mass, never integrated here; its v and w push others
 float softHertz=0;      // a padded body (a mattress): contacts on it are springs of this stiffness, so a load sinks in
 bool sleeping=false;    // takes part in collisions as an immovable body until woken
 bool ccd=false;         // swept against the world this step (set by the space for fast bodies)
 float sleepTime=0;      // seconds slow enough to sleep
 float linearDamping=.02f,angularDamping=.06f;
 V3 force{},torque{};    // accumulated this step (cleared by step)
 V3 gravity{0,0,-14.f};  // per body: water or a hand may change it
 int user=-1;            // the caller's index (a Clutter's index in its list)
 int id=-1;              // the caller's lasting name for it (warm starting matches by it; -1: its index here)
 std::uint32_t group=0;  // bodies sharing a nonzero group do not collide (a container and what rides in it)
 // Derived per step.
 V3 dp{};Quat dq{};      // motion since the step began (TGS-soft: contact separation updates from it)
 int island=-1;
 V3 boundsLo{},boundsHi{};
 V3 pointVelocity(V3 p)const{return v+cross(w,p-x);}
 V3 toWorld(V3 local)const{return x+q.rotate(local-shape->com);} // a body-frame point (box origin) to world
 void setMass(float mass){invMass=kinematic||mass<=0?0:1/mass;inertiaLocal=kinematic||mass<=0?M3::zero():shape->inertia*mass;invInertiaLocal=kinematic||mass<=0?M3::zero():inertiaLocal.inverse();}
 void updateInertia(){const M3 r=q.matrix();invInertia=r*invInertiaLocal*r.transposed();}
 // An impulse at a world point (wakes it).
 void applyImpulse(V3 impulse,V3 at){if(invMass<=0)return;v=v+impulse*invMass;w=w+invInertia*cross(at-x,impulse);sleeping=false;sleepTime=0;}
 void applyForce(V3 f,V3 at){force=force+f;torque=torque+cross(at-x,f);}
 bool movable()const{return invMass>0&&!sleeping;}
};

// ---- Contacts -------------------------------------------------------------------------------------
struct ContactPoint {
 V3 p{};float separation=0;           // world point (between the two surfaces), signed gap along the normal
 V3 localA{},localB{};                // the point in each body's frame (for warm-start matching)
 float normalImpulse=0,tangentImpulse[2]{0,0},maxNormalImpulse=0;
 float relativeVelocity=0;            // approach speed along the normal when the step began (for restitution and sounds)
 // Prepared each step.
 V3 rA{},rB{};float normalMass=0,tangentMass[2]{0,0},adjustedSeparation=0;
 // The turn a unit impulse along the normal and each tangent gives each body (its world inverse inertia times
 // r x axis): constant through the step, so each impulse is applied without a matrix product.
 V3 wnA{},wnB{},wtA[2]{},wtB[2]{};
 std::uint32_t feature=0;             // what the point came from (narrow phase), for matching
};
// The world thing a static contact touched, for the caller (a pane, a soft pad...): free for its use.
struct Surface {int kind=0,index=-1;V3 velocity{};float hertz=0;};
struct Manifold {
 int a=-1,b=-1;                       // body indices; b<0: the static world
 int partA=0,partB=0;
 V3 normal{};                         // from a into b (pushes b along it, a against it); for the world, from the world into a, negated (see world())
 std::array<ContactPoint,4> points{};int count=0;
 Material material{};
 Surface surface{};
 V3 t1{},t2{};
 float rollingImpulse[3]{0,0,0};
 // Prepared each step: the middle of the points (an impact's), and the rolling axes' turns and masses.
 V3 midA{},midB{},midWA{},midWB{};float midMass=0;V3 rollWA[3]{},rollWB[3]{};float rollMass[3]{0,0,0};
 bool cold=false;                     // no impulses to start from (new this step): solved more on its first substep
 bool world()const{return b<0;}
 // The warm-start key of the bodies (their lasting ids, Space::keyOf), parts and, for the world, the cluster.
 static std::uint64_t key(int idA,int idB,int partA,int partB){return (std::uint64_t(std::uint32_t(idA))<<40)^(std::uint64_t(std::uint32_t(idB+1)&0xfffff)<<20)^(std::uint64_t(partA)<<8)^std::uint64_t(partB);}
};

// ---- Narrow phase ---------------------------------------------------------------------------------
// Contacts between two bodies' parts within `margin` (a positive separation is speculative).
// The normal points from a to b.
int collideParts(const Body& a,int partA,const Body& b,int partB,float margin,Manifold& out);
// A part's world box.
void partBounds(const Body& body,int part,V3& lo,V3& hi);
// The part's farthest point along direction d (world), and the points of its face or edge there (for a
// contact against a plane): up to 4 points of the feature most nearly facing d.
V3 support(const Body& body,int part,V3 d);
int supportFeature(const Body& body,int part,V3 d,std::array<V3,8>& out);
// Sample points on the part's surface (world), every `spacing` metres along its edges and across its faces,
// for asking a world of columns where it is solid.
void surfaceSamples(const Body& body,int part,float spacing,std::vector<V3>& out);
// The part's nearest surface point to `point` (world), its outward normal there, and the signed distance
// (negative inside): for the corners of the world's boxes against a body.
bool nearestPoint(const Body& body,int part,V3 point,V3& surface,V3& normal,float& distance);

// ---- Phase 2 hook: joints -------------------------------------------------------------------------
struct Space;
struct Constraint {
 virtual ~Constraint()=default;
 virtual int bodyA()const=0;virtual int bodyB()const=0;   // -1: the world
 virtual void prepare(Space&,float h)=0;                  // once a step: anchors, effective masses
 virtual void warmStart(Space&)=0;                        // each substep
 virtual void solve(Space&,float h,bool useBias)=0;       // each substep (bias) and relax (no bias)
 virtual void storeImpulses(){}                           // after the step: keep for warm starting
};

// A springy hinge between two bodies (phase 2's first joint; the mattress chain's links): the two anchors held
// together, the turn about every axis but the hinge held, and about the hinge a spring of `hertz` (damping
// ratio `damping`) toward no bend, stopped hard past +-`limit`. The bend about the hinge is `angle()`.
struct Hinge:Constraint {
 int a=-1,b=-1;
 V3 anchorA{},anchorB{};   // the joint in each body's frame (from its centre of mass, unturned)
 V3 axisA{0,1,0};          // the hinge axis in A's frame
 Quat rest{};              // B's turn relative to A unbent (A's inverse turn times B's)
 float hertz=4,damping=.3f,limit=1.2f,lockHertz=60;
 // Solver state.
 V3 rA{},rB{},linear{},angular{};float hinge=0;M3 linearMass{},angularMass{};float hingeMass=0;
 int bodyA()const override{return a;}int bodyB()const override{return b;}
 float angle(const Space& s)const;
 void prepare(Space&,float h)override;void warmStart(Space&)override;void solve(Space&,float h,bool useBias)override;
private:
 float h_=1.f/480;
};

// ---- The space ------------------------------------------------------------------------------------
struct Settings {
 int substeps=4,relaxIterations=1;
 // Four substeps a step and 120 Hz contacts, damping ratio 2, a millimetre of slop, push-out at most 1 m/s: a
 // ten-crate tower woken settles in 0.27 s and its top drifts 0.6 mm. (60 Hz over-damped ten times let each
 // contact sink 4 mm under its load and the tower lean 2 mm before it slept, at 0.53 s; critically damped
 // with a 3 m/s push-out, a crate shoved into a chair could throw it faster than it struck.)
 float contactHertz=120,contactDampingRatio=2,pushout=1;   // soft contact: stiffness, damping, max push-out speed (m/s)
 float worldDampingRatio=2;                                // against the world (twice the stiffness): its own damping
 float linearSlop=.001f,speculative=.04f;                  // allowed penetration, speculative reach
 float restitutionThreshold=1.f;                           // below this approach speed nothing bounces
 float middleThreshold=1.f;                               // an approach faster than this is first met at the manifold's middle
 float sleepLinear=.06f,sleepAngular=.12f,sleepTime=.2f;  // islands under these for this long sleep
 // Contacts with nothing to warm start from (a stack just woken, or set down whole) are iterated this many
 // more times on the step's first substep, so the weight reaches the floor before anything moves under it.
 int coldIterations=8;
 float maxLinear=40,maxAngular=60;
};
// What a step reports back (for sounds, breaking, glass and AI hearing).
struct ContactEvent {int a=-1,b=-1,partA=0,partB=0;V3 point{},normal{};float approach=0,impulse=0,effectiveMass=0;Surface surface{};};
struct Space {
 Settings settings;
 std::vector<Body> bodies;
 std::vector<Constraint*> constraints;   // phase 2: owned by the caller
 // The caller's static contacts for body i (the world under and round it), found once a step. Append
 // manifolds with a=i, b=-1, normal pointing from the world into the body... (normal: from body to world,
 // as a manifold's normal goes from a to b). `margin` is how far ahead to look.
 std::function<void(Space&,int body,float margin,std::vector<Manifold>&)> worldContacts;
 // Swept test against the world for a fast body: the fraction of the move from..to that is clear, and the
 // normal of what stops it (fraction 1: clear). Used for continuous collision.
 std::function<float(Space&,int body,V3 from,V3 to,V3& normal)> sweep;
 // Pairs the caller rules out (a held prop and what it carries...).
 std::function<bool(const Space&,int a,int b)> filter;
 // Warm-start cache (persistent between steps): key -> points.
 struct CachedPoint {V3 localA{};float normal=0,tangent[2]{0,0};};
 struct Cached {std::array<CachedPoint,4> points{};int count=0;float rolling[3]{0,0,0};};
 std::vector<std::pair<std::uint64_t,Cached>> cache;   // sorted by key
 std::vector<Manifold> manifolds;   // this step's
 std::vector<ContactEvent> events;  // this step's impacts (approach over .3 m/s or an impulse)
 // Stats.
 int pairsTested=0,worldManifolds=0,iterationsRun=0;
 double worldMs=0,pairMs=0,solveMs=0;   // where the last step's time went (collide's world and pair halves, the substeps)
 // One fixed step of dt.
 void step(float dt);
 // Islands: groups of touching, moving bodies (filled by step).
 std::vector<int> islandOf;
 void wake(int body);
 int idOf(int body)const{return body<0?-1:bodies[size_t(body)].id>=0?bodies[size_t(body)].id:body;}
 std::uint64_t keyOf(const Manifold& m)const{return Manifold::key(idOf(m.a),idOf(m.b),m.partA,m.partB);}
 // A soft spring pulling a body to a pose (phase 3: gravity gloves): applied as velocity, contacts still hold.
 void drive(int body,V3 targetX,Quat targetQ,float linearHertz,float angularHertz,float dt,float maxForce);
private:
 void collide(float margin);
 void prepare(float h);
 void warmStart();
 void solve(float h,bool useBias,bool coldOnly=false);
 void restitution();
 void storeImpulses();
 void integrateVelocities(float h);
 void integratePositions(float h);
 void continuous(float dt,const std::vector<V3>& start);
 void islands(float dt);
};
}
