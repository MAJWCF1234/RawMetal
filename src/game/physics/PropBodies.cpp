#include "PropBodies.h"
#include "../Containers.h"
#include "../SoftBodies.h"
#include "../../core/Tune.h"
#include "../../world/ModelIds.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
namespace retro::rigid {
namespace {
int& forced(){static int mode=-1;return mode;}
}
bool legacyProps(){
 if(forced()>=0)return forced()==1;
 static const int env=[]{const char* v=std::getenv("RM_PROPS");if(!v)return -1;return (std::strcmp(v,"legacy")==0||std::strcmp(v,"old")==0||std::strcmp(v,"1")==0)?1:0;}();
 if(env>=0)return env==1;
 return tune("props.legacy",0.f)>.5f;}
void forceLegacyProps(int mode){forced()=mode;}

// Coulomb friction against a hard floor (static, sliding), restitution, rolling resistance (metres).
Material propMaterial(PropMaterial m){
 switch(m){
  case PropMaterial::Paper:return {.55f,.45f,.05f,.04f};
  case PropMaterial::Cardboard:return {.6f,.5f,.12f,.03f};
  case PropMaterial::Plastic:return {.45f,.35f,.4f,.006f};
  case PropMaterial::Wood:return {.62f,.45f,.3f,.008f};
  case PropMaterial::Metal:return {.5f,.38f,.32f,.004f};
  case PropMaterial::Glass:return {.45f,.35f,.45f,.004f};
  case PropMaterial::Ceramic:return {.5f,.4f,.3f,.005f};
  case PropMaterial::Electronic:return {.5f,.4f,.2f,.006f};
  case PropMaterial::Cloth:return {.85f,.7f,.02f,.08f};
  case PropMaterial::Organic:return {.7f,.55f,.15f,.03f};
  default:return rubber();}}
Material rubber(){return {.95f,.85f,.82f,.012f};}

namespace {
enum class Kind {Box,Upright,AlongX,AlongY,Ball,OfficeChair,LegChair,Table,Wheelchair,Lamp};
Kind kindOf(int m){
 using namespace model;
 if(m==ind_barrel||m==drum||m==scrap_drum||m==scrap_burn_barrel||m==gas_cylinder||m==glass_bottle||m==mug||m==med_pill_bottle||m==med_specimen_jar||m==med_extinguisher||m==anl_cell_core)return Kind::Upright;
 if(m==tire)return Kind::AlongY;
 if(m==pwr_pipe_straight)return Kind::AlongX;
 if(m==market_pumpkin)return Kind::Ball;
 if(m==office_chair||m==lab_boardroom_chair)return Kind::OfficeChair;
 if(m==wooden_chair||m==scrap_chair||m==platform_chair)return Kind::LegChair;
 if(m==wooden_table||m==bench)return Kind::Table;
 if(m==med_wheelchair)return Kind::Wheelchair;
 if(m==lab_desk_lamp)return Kind::Lamp;
 return Kind::Box;}
// Which way a chair's back lies along its own y (model_fronts.json: an OBJ front of -z faces local -y,
// so the back is at +y; +z faces +y, the back at -y).
float backSide(int m){return m==model::lab_boardroom_chair||m==model::med_wheelchair?-1.f:1.f;}
Shape build(int m,std::array<float,3> d){
 Shape s;const float X=d[0],Y=d[1],Z=d[2],hx=X*.5f,hy=Y*.5f,hz=Z*.5f,bottom=-hz;
 auto slab=[&](float x0,float x1,float y0,float y1,float z0,float z1){s.add(PartKind::Box,{(x0+x1)*.5f,(y0+y1)*.5f,(z0+z1)*.5f},{std::max(.004f,(x1-x0)*.5f),std::max(.004f,(y1-y0)*.5f),std::max(.004f,(z1-z0)*.5f)});};
 // An open container: its floor (solid below the inner floor) and four walls to the rim.
 if(m>=0&&m<int(std::size(model::container))&&model::container[m][1]>0){
  const auto& k=model::container[m];const auto& own=model::propSize[m];auto scale=[&](int a){return own[a]>0?d[size_t(a)]/own[a]:1.f;};
  const float fl=std::clamp(k[0]*scale(2),.015f,Z*.9f),rim=std::clamp(k[1]*scale(2),fl+.02f,Z);
  const float wall=std::clamp(std::max(k[2]*std::min(scale(0),scale(1)),.025f),.01f,std::min(hx,hy)*.4f);
  slab(-hx,hx,-hy,hy,bottom,bottom+fl);
  slab(-hx,-hx+wall,-hy,hy,bottom+fl,bottom+rim);slab(hx-wall,hx,-hy,hy,bottom+fl,bottom+rim);
  slab(-hx+wall,hx-wall,-hy,-hy+wall,bottom+fl,bottom+rim);slab(-hx+wall,hx-wall,hy-wall,hy,bottom+fl,bottom+rim);
  s.hollow=true;s.finish();return s;}
 switch(kindOf(m)){
  case Kind::Upright:s.add(PartKind::Cylinder,{},{std::min(hx,hy),std::min(hx,hy),hz});break;
  case Kind::AlongY:s.add(PartKind::Cylinder,{},{std::min(hx,hz),std::min(hx,hz),hy},Quat::axisAngle({1,0,0},kPi*.5f));break;
  case Kind::AlongX:s.add(PartKind::Cylinder,{},{std::min(hy,hz),std::min(hy,hz),hx},Quat::axisAngle({0,1,0},kPi*.5f));break;
  case Kind::Ball:{const float r=std::min({hx,hy,hz});s.add(PartKind::Sphere,{0,0,bottom+r},{r,r,r});break;}
  case Kind::OfficeChair:{const float b=backSide(m),seat=bottom+.47f*Z;
   slab(-hx,hx,-hy,hy,bottom,bottom+.12f*Z);                     // the wheeled star
   slab(-.06f*X,.06f*X,-.06f*Y,.06f*Y,bottom+.12f*Z,seat);        // the column
   slab(-.46f*X,.46f*X,-.46f*Y,.46f*Y,seat,seat+.1f*Z);          // the seat
   {const float y0=b>0?hy-.12f*Y:-hy,y1=b>0?hy:-hy+.12f*Y;slab(-.42f*X,.42f*X,y0,y1,seat+.1f*Z,hz);} // the back
   break;}
  case Kind::LegChair:{const float b=backSide(m),seat=bottom+.46f*Z,leg=std::max(.02f,.08f*std::min(X,Y));
   for(int i=0;i<4;++i){const float x=i&1?hx-leg:-hx,y=i&2?hy-leg:-hy;slab(x,x+leg,y,y+leg,bottom,seat);}
   slab(-hx,hx,-hy,hy,seat,seat+.05f*Z);
   {const float y0=b>0?hy-.09f*Y:-hy,y1=b>0?hy:-hy+.09f*Y;slab(-hx,hx,y0,y1,seat+.05f*Z,hz);}
   break;}
  case Kind::Table:{const float top=hz-std::clamp(.07f*Z,.03f,.08f),leg=std::clamp(.07f*std::min(X,Y),.04f,.09f);
   slab(-hx,hx,-hy,hy,top,hz);
   for(int i=0;i<4;++i){const float x=i&1?hx-leg:-hx,y=i&2?hy-leg:-hy;slab(x,x+leg,y,y+leg,bottom,top);}
   break;}
  case Kind::Wheelchair:{const float b=backSide(m);slab(-hx,hx,-hy,hy,bottom,bottom+.58f*Z);
   const float y0=b>0?hy-.14f*Y:-hy,y1=b>0?hy:-hy+.14f*Y;slab(-hx,hx,y0,y1,bottom+.58f*Z,hz);break;}
  case Kind::Lamp:slab(-hx,hx,-hy,-hy+.55f*Y,bottom,bottom+.12f*Z);slab(-.18f*X,.18f*X,-hy,-hy+.25f*Y,bottom+.12f*Z,hz);slab(-.3f*X,.3f*X,-hy,hy,hz-.2f*Z,hz);break;
  default:s.add(PartKind::Box,{},{std::max(.005f,hx),std::max(.005f,hy),std::max(.005f,hz)});}
 s.finish();return s;}
}
const Shape& propShape(int model,std::array<float,3> dims){
 static std::mutex lock;static std::map<std::array<int,4>,std::unique_ptr<Shape>> cache;
 const std::array<int,4> key{model,int(std::lround(dims[0]*1e4f)),int(std::lround(dims[1]*1e4f)),int(std::lround(dims[2]*1e4f))};
 std::lock_guard<std::mutex> hold(lock);auto& slot=cache[key];if(!slot)slot=std::make_unique<Shape>(build(model,dims));return *slot;}
Material propMaterialOf(const Clutter& c){Material m=propMaterial(c.material());
 // On casters or wheels: it rolls away under a shove rather than dragging its feet (and so is not bowled over).
 using namespace model;if(c.model==office_chair||c.model==lab_boardroom_chair||c.model==med_wheelchair||c.model==med_crash_cart){m.staticFriction=.12f;m.dynamicFriction=.08f;m.rolling=.01f;}
 return m;}
float propSoftHertz(const Clutter& c){return soft::isSoft(c.model)?tune("props.pad_hertz",4.f):0.f;}

Quat propOrientation(const Clutter& c){
 if(c.orientEuler[0]==c.yaw&&c.orientEuler[1]==c.pitch&&c.orientEuler[2]==c.roll)return Quat{c.orient[0],c.orient[1],c.orient[2],c.orient[3]};
 return Quat::fromEuler(c.yaw,c.pitch,c.roll);}
V3 propAngular(const Clutter& c){const float cy=std::cos(c.yaw),sy=std::sin(c.yaw);return V3{cy,sy,0}*c.rollSpeed+V3{-sy,cy,0}*c.pitchSpeed+V3{0,0,c.spin};}
void setPropAngular(Clutter& c,V3 w){const float cy=std::cos(c.yaw),sy=std::sin(c.yaw);c.rollSpeed=dot(w,{cy,sy,0});c.pitchSpeed=dot(w,{-sy,cy,0});c.spin=w.z;}
void toBody(const Clutter& c,Body& b){
 const Shape& s=propShape(c);b.shape=&s;b.q=propOrientation(c);b.x=propCentre(c)+b.q.rotate(s.com);
 b.v={c.velocity.x,c.velocity.y,c.vz};b.w=propAngular(c);b.material=propMaterialOf(c);b.softHertz=propSoftHertz(c);
 b.sleeping=c.sleeping;b.sleepTime=std::max(0.f,c.restTime);b.setMass(c.mass());b.updateInertia();}
void fromBody(const Body& b,Clutter& c){
 const Quat q=b.q.normalized();c.orient={q.w,q.x,q.y,q.z};q.toEuler(c.yaw,c.pitch,c.roll);c.orientEuler={c.yaw,c.pitch,c.roll};
 const V3 centre=b.x-q.rotate(b.shape->com);c.pos={centre.x,centre.y};c.z=centre.z-c.extent()[2];
 c.velocity={b.v.x,b.v.y};c.vz=b.v.z;setPropAngular(c,b.w);c.sleeping=b.sleeping;c.restTime=b.sleepTime;}

bool captureProp(const Clutter& c,std::array<float,4>& q,std::array<float,3>& w,std::int32_t& island){
 if(c.model<0||c.broken)return false;const Quat o=propOrientation(c);q={o.x,o.y,o.z,o.w};const V3 a=propAngular(c);w={a.x,a.y,a.z};island=c.sleeping?std::max(0,c.island):-1;return true;}
void restoreProp(Clutter& c,const std::array<float,4>& q,const std::array<float,3>& w,std::int32_t island){
 if(c.model<0)return;Quat o=Quat{q[3],q[0],q[1],q[2]}.normalized();
 // The middle of the box stays where the save put it; the turn is the saved one exactly.
 const V3 centre=propCentre(c);c.orient={o.w,o.x,o.y,o.z};o.toEuler(c.yaw,c.pitch,c.roll);c.orientEuler={c.yaw,c.pitch,c.roll};
 c.z=centre.z-c.extent()[2];setPropAngular(c,{w[0],w[1],w[2]});c.island=island;}
}
