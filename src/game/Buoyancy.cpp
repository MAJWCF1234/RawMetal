#include "Buoyancy.h"
#include "../core/Tune.h"
#include <cmath>
namespace retro::buoyancy {
namespace {
// What the water takes a material's bulk to be: a floater's least (a crate's planks and air float
// as wood, not as air), a sinker's least (a hollow can floods).
float least(PropMaterial m){
 switch(m){case PropMaterial::Paper:case PropMaterial::Cardboard:return 320;case PropMaterial::Plastic:return 260;case PropMaterial::Wood:return 480;
  case PropMaterial::Glass:return 520;case PropMaterial::Cloth:return 620;case PropMaterial::Metal:return 2600;case PropMaterial::Ceramic:return 2000;
  case PropMaterial::Electronic:return 1800;case PropMaterial::Organic:return 1150;default:return 900;}}
float volume(const Clutter& c){const auto s=c.size();return std::max(1e-5f,s[0]*s[1]*s[2]);}
float nearestFace(float a){const float q=kPi*.5f;return std::round(a/q)*q;}
}
bool floats(const Clutter& c){return c.model>=0?propMaterialTraits(c.material()).floats:c.kind!=3&&c.kind!=4;}
float density(const Clutter& c){
 const float bulk=c.mass()/volume(c);
 if(c.model<0)return floats(c)?450.f:2400.f;
 const float low=least(c.material());
 return floats(c)?std::clamp(bulk,low,880.f):std::max(bulk,low);}
float drag(const Clutter& c){const auto s=c.size();const float area=std::max({s[0]*s[1],s[0]*s[2],s[1]*s[2]});
 return std::clamp(.5f*WaterDensity*area/std::max(.05f,c.mass()),2.5f,14.f);}
Result step(Clutter& c,float surface,float bed,float now,int seed,float dt){
 Result r;if(surface<-999.f||dt<=0)return r;
 const auto e=c.extent();const float h=std::max(.01f,e[2]*2);
 const float under=surface-c.z;r.immersed=std::clamp(under/h,0.f,1.f);if(r.immersed<=0)return r;
 const float rho=density(c),k=drag(c)*tune("water.drag",1.f),f=r.immersed;
 const bool floater=rho<WaterDensity;
 r.afloat=floater&&surface-bed>h*rho/WaterDensity*.9f;
 // Through the surface this substep (its base was above it a step ago), falling.
 r.speed=std::sqrt(lengthSq(c.velocity)+c.vz*c.vz);r.entered=c.vz<-1.f&&under<=-c.vz*dt*1.05f+.002f;
 // Lift: the weight of the water it puts aside (the share of its height under).
 c.vz+=Gravity*WaterDensity/rho*f*dt;
 // Drag, by how deep it is in: a little linear (the water's own hold) and by the square of the speed.
 const float horizontal=std::sqrt(lengthSq(c.velocity));
 c.velocity=c.velocity*std::exp(-(1.1f+k*horizontal)*f*dt);
 // A float's bob is damped a fifth of critical: three or four bobs and it rides still.
 // A sinker is held back more: it settles to the bed rather than dropping like a stone.
 const float bob=floater?2*tune("water.bob_damping",.2f)*std::sqrt(Gravity*WaterDensity/(rho*h)):tune("water.sink_drag",2.6f);
 c.vz*=std::exp(-(1.1f+bob+k*std::fabs(c.vz))*f*dt);
 const float turn=std::exp(-2.6f*f*dt);c.spin*=turn;c.pitchSpeed*=turn;c.rollSpeed*=turn;
 if(r.afloat&&f<.999f){
  // Afloat: the water rights it onto its nearest face, a swell rocks it a few degrees and heaves it a
  // centimetre, and a slow wandering drift carries it (each prop on its own phase).
  const float phase=float(seed%97)*.61f,swell=tune("water.swell",1.f);
  c.pitchSpeed+=(-34.f*(c.pitch-nearestFace(c.pitch))+1.5f*swell*std::sin(1.9f*now+phase))*dt;
  c.rollSpeed+=(-34.f*(c.roll-nearestFace(c.roll))+1.5f*swell*std::sin(1.6f*now+phase*1.7f+1.f))*dt;
  c.vz+=.45f*swell*std::sin(2.4f*now+phase*2.3f)*dt;
  const float drift=tune("water.drift",.03f);
  c.velocity+=Vec2{std::cos(.23f*now+phase),std::sin(.19f*now+phase*1.3f)}*(drift*dt);
 }
 return r;}
}
