#include "Ragdoll.h"
#include "Game.h"
#include "../core/PackedResource.h"
#include <fstream>
#include <limits>
namespace retro {
static float dotR(RagPoint a,RagPoint b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float lenR(RagPoint v){return std::sqrt(dotR(v,v));}
static RagPoint unitR(RagPoint v){return v*(1/std::max(.00001f,lenR(v)));}
static RagPoint crossR(RagPoint a,RagPoint b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
// Capsule envelopes include the visible suit, not only the underlying skeleton.
static const std::array<float,15>& collisionRadii(){static const auto values=[](){
 std::array<float,15> r{.23f,.23f,.18f,.13f,.105f,.09f,.13f,.105f,.09f,.16f,.12f,.13f,.16f,.12f,.13f};
 const auto&a=Ragdoll::asset();for(const auto&v:a.vertices)for(int j:{int(v.a),int(v.b)}){
  if((j==v.a?v.weight:1-v.weight)<=0)continue;
  auto segment=a.rest[Ragdoll::ends[j]]-a.rest[j];float t=std::clamp(dotR(v.p-a.rest[j],segment)/std::max(.00001f,dotR(segment,segment)),0.f,1.f);
  float distance=lenR(v.p-(a.rest[j]+segment*t))+.008f;
  r[j]=std::max(r[j],distance);r[Ragdoll::ends[j]]=std::max(r[Ragdoll::ends[j]],distance);
 }return r;}();return values;}
static constexpr int links[][2]={{0,1},{1,2},{1,3},{3,4},{4,5},{1,6},{6,7},{7,8},{0,9},{9,10},{10,11},{0,12},{12,13},{13,14},{3,6},{9,12},{3,0},{6,0},{1,9},{1,12}};
const HazmatAsset& Ragdoll::asset(){static const HazmatAsset data=[](){
 auto bytes=loadResource(245);size_t at=4;auto read=[&]<class T>(){if(at+sizeof(T)>bytes.size())throw std::runtime_error("Truncated hazmat asset");T v;std::memcpy(&v,bytes.data()+at,sizeof(v));at+=sizeof(v);return v;};
 if(bytes.size()<8||std::memcmp(bytes.data(),"RMR1",4))throw std::runtime_error("Invalid hazmat asset");auto count=read.operator()<uint32_t>();if(count>100000||count%3)throw std::runtime_error("Invalid hazmat triangles");HazmatAsset a;
 for(auto& p:a.rest){p={read.operator()<float>(),read.operator()<float>(),read.operator()<float>()};}
 a.vertices.resize(count);for(auto&v:a.vertices){v.p={read.operator()<int16_t>()*.001f,read.operator()<int16_t>()*.001f,read.operator()<int16_t>()*.001f};v.u=read.operator()<uint16_t>()/65535.f;v.v=1-read.operator()<uint16_t>()/65535.f;v.a=read.operator()<uint8_t>();v.b=read.operator()<uint8_t>();v.weight=read.operator()<uint8_t>()/255.f;v.material=read.operator()<uint8_t>();if(v.a>=15||v.b>=15||v.material>=3)throw std::runtime_error("Invalid hazmat skin");}
 if(at!=bytes.size())throw std::runtime_error("Hazmat trailing data");return a;}();return data;}
void Ragdoll::seed(const World& world){
 static const Ragdoll settled=[&](){Ragdoll r;r.initialized=true;for(int i=0;i<Count;++i){auto v=asset().rest[i];r.p[i]={8.9f-v.x*.766f+v.y*.643f,8.2f-v.z,.45f-v.x*.643f-v.y*.766f};}
  // Author an interrupted, sideways collapse, then let constraints and contact
  // settle it. Avoid the symmetric supine bind pose of a sleeping character.
  r.p[2]=r.p[2]+RagPoint{-.09f,-.035f,.03f};
  r.p[4]=r.p[4]+RagPoint{-.18f,-.38f,.12f};r.p[5]=r.p[5]+RagPoint{-.42f,-.70f,.08f};
  r.p[7]=r.p[7]+RagPoint{.12f,.05f,.05f};r.p[8]=r.p[8]+RagPoint{.20f,-.12f,.02f};
  r.p[10]=r.p[10]+RagPoint{-.34f,-.10f,.18f};r.p[11]=r.p[11]+RagPoint{-.30f,-.65f,.15f};
  r.p[13]=r.p[13]+RagPoint{.13f,.02f,.03f};r.p[14]=r.p[14]+RagPoint{.22f,-.04f,.02f};r.previous=r.p;
  for(int i=0;i<300;++i)r.update(world,1.f/120);r.sleeping=true;r.previous=r.p;r.quiet=1;r.accumulator=0;return r;}();*this=settled;
}
void Ragdoll::impulse(int joint,RagPoint velocity){if(!initialized||joint<0||joint>=Count)return;sleeping=false;quiet=0;previous[joint]=previous[joint]-velocity*(1.f/120);}
void Ragdoll::update(const World&w,float dt){
 if(!initialized||sleeping)return;const auto&radii=collisionRadii();accumulator+=std::clamp(dt,0.f,.05f);constexpr float step=1.f/120;
 std::vector<RagPoint> surface;surface.reserve(asset().vertices.size());
 while(accumulator>=step){accumulator-=step;auto before=p;
  // Ground support follows the oriented suit envelope. A spherical torso proxy
  // is useful for ray/wall tests but otherwise leaves a prone body hovering.
  std::array<float,Count> support;support.fill(.015f);skin(surface);
  for(size_t k=0;k<surface.size();++k){const auto&v=asset().vertices[k];for(int j:{int(v.a),int(v.b)}){
   if((j==v.a?v.weight:1-v.weight)<=0)continue;int end=ends[j];auto segment=p[end]-p[j];
   float t=std::clamp(dotR(surface[k]-p[j],segment)/std::max(.00001f,dotR(segment,segment)),0.f,1.f);
   float depth=(p[j]+segment*t).z-surface[k].z+.005f;support[j]=std::max(support[j],depth);support[end]=std::max(support[end],depth);
  }}
  for(int i=0;i<Count;++i){auto velocity=(p[i]-previous[i])*.985f;if(lenR(velocity)>.12f)velocity=unitR(velocity)*.12f;previous[i]=p[i];p[i]=p[i]+velocity+RagPoint{0,0,-14*step*step};}
  auto constraint=[&](int a,int b,float target){auto d=p[b]-p[a];float length=lenR(d);if(length<.00001f)return;auto correction=d*((length-target)/length*.5f);p[a]=p[a]+correction;p[b]=p[b]-correction;};
  for(int iteration=0;iteration<12;++iteration){
   for(auto&link:links)constraint(link[0],link[1],lenR(asset().rest[link[1]]-asset().rest[link[0]]));
   for(auto pair:{std::array<int,3>{3,4,5},{6,7,8},{9,10,11},{12,13,14}}){float full=lenR(asset().rest[pair[1]]-asset().rest[pair[0]])+lenR(asset().rest[pair[2]]-asset().rest[pair[1]]);if(lenR(p[pair[2]]-p[pair[0]])<full*.55f)constraint(pair[0],pair[2],full*.55f);}
   for(int i=0;i<Count;++i){float r=radii[i];float floor=w.supportBelow(p[i].x,p[i].y,std::max(before[i].z,p[i].z)+.05f);p[i].z=std::max(p[i].z,floor+support[i]);
    bool fits=true;for(float x:{-r,r})for(float y:{-r,r})fits&=w.fits(p[i].x+x,p[i].y+y,p[i].z-support[i],support[i]+r)&&!w.doorBlocks(p[i].x+x,p[i].y+y,p[i].z-support[i],support[i]+r);
    if(!fits){p[i].x=before[i].x;p[i].y=before[i].y;previous[i].x=p[i].x;previous[i].y=p[i].y;}
   }
  }
  float motion=0;for(int i=0;i<Count;++i){float floor=w.supportBelow(p[i].x,p[i].y,p[i].z+.01f);if(p[i].z<=floor+support[i]+.003f){previous[i].x=p[i].x-(p[i].x-previous[i].x)*.65f;previous[i].y=p[i].y-(p[i].y-previous[i].y)*.65f;previous[i].z=p[i].z;}
   motion=std::max(motion,lenR(p[i]-before[i]));}
  quiet=motion<.0015f?quiet+step:0;if(quiet>.6f){sleeping=true;previous=p;accumulator=0;break;}
 }
}
float Ragdoll::rayHit(RagPoint origin,RagPoint direction,int& joint)const{
 float nearest=1e9f;joint=-1;if(!initialized)return nearest;const auto&radii=collisionRadii();
 for(int i=0;i<Count;++i){auto delta=p[i]-origin;float along=dotR(delta,direction);if(along>0&&along<nearest&&lenR(delta-direction*along)<radii[i]){nearest=along;joint=i;}}return nearest;
}
void Ragdoll::skin(std::vector<RagPoint>& result)const{
 struct Frame{RagPoint axis,front,side;};std::array<Frame,Count> restFrames,currentFrames;
 auto restAcross=asset().rest[3]-asset().rest[6],across=p[3]-p[6];
 auto restFront=unitR(crossR(restAcross,asset().rest[1]-asset().rest[0])),front=unitR(crossR(across,p[1]-p[0]));
 auto frame=[](RagPoint axis,RagPoint front,RagPoint across){axis=unitR(axis);auto v=front-axis*dotR(front,axis);if(lenR(v)<.01f)v=across-axis*dotR(across,axis);v=unitR(v);return Frame{axis,v,crossR(axis,v)};};
 for(int i=0;i<Count;++i){restFrames[i]=frame(asset().rest[ends[i]]-asset().rest[i],restFront,restAcross);currentFrames[i]=frame(p[ends[i]]-p[i],front,across);}
 auto deform=[&](RagPoint point,int joint){auto v=point-asset().rest[joint];auto a=restFrames[joint],b=currentFrames[joint];return p[joint]+b.axis*dotR(v,a.axis)+b.front*dotR(v,a.front)+b.side*dotR(v,a.side);};
 result.clear();result.reserve(asset().vertices.size());for(auto&v:asset().vertices)result.push_back(deform(v.p,v.a)*v.weight+deform(v.p,v.b)*(1-v.weight));
}
bool Ragdoll::test(){World world(3);Ragdoll rag;rag.seed(world);auto initial=rag.p;rag.impulse(5,{1.5f,1.f,2.f});for(int i=0;i<480;++i)rag.update(world,1.f/120);
 std::ofstream report("hazmat-physics-test.txt");float error=0;for(auto&link:links)error=std::max(error,std::fabs(lenR(rag.p[link[1]]-rag.p[link[0]])-lenR(asset().rest[link[1]]-asset().rest[link[0]])));
 float minimum=100;for(auto v:rag.skin())minimum=std::min(minimum,v.z);report<<"constraint error "<<error<<" minimum skinned height "<<minimum<<" sleeping "<<rag.sleeping<<'\n';
 for(int i=0;i<Count;++i)report<<"joint "<<i<<" radius "<<collisionRadii()[i]<<" height "<<rag.p[i].z<<'\n';
 Ragdoll fast,slow;fast.seed(world);slow=fast;fast.impulse(5,{1.5f,1,2});slow.impulse(5,{1.5f,1,2});
 for(int i=0;i<240;++i)fast.update(world,1.f/60);for(int i=0;i<480;++i)slow.update(world,1.f/120);
 float drift=0;for(int i=0;i<Count;++i)drift=std::max(drift,lenR(fast.p[i]-slow.p[i]));report<<"60/120 Hz drift "<<drift<<'\n';
 return error<.035f&&minimum>-.005f&&lenR(initial[5]-rag.p[5])>.02f&&rag.sleeping&&drift<.001f;
}
Game Game::hazmatInspection(int view){int camera=view%3;auto game=mapInspection(camera==0?Vec2{7.2f,6.1f}:camera==1?Vec2{10.8f,6.1f}:Vec2{8.9f,9.5f},camera==0?.65f:camera==1?2.55f:-kPi*.5f,-35,3,false,0,true);if(view==3)game.m_hazmat.impulse(5,{1.5f,1,2});return game;}
bool Game::testHazmat(){
 if(!Ragdoll::test())return false;auto game=hazmatInspection(0);game.m_hazmat.impulse(5,{1,0,2});game.update({},.025f);
 auto data=game.encodeSave();Game restored;if(!restored.decodeSave(data)||restored.encodeSave()!=data)return false;
 int joint=-1;auto point=game.m_hazmat.p[1];if(game.m_hazmat.rayHit(point+RagPoint{0,0,2},{0,0,-1},joint)>2.3f||joint<0)return false;
 return true;
}
}
