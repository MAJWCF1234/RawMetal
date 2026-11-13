#include "Humanoid.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
namespace retro::humanoid {
namespace {
constexpr float Deg=3.14159265f/180.f;
}
const char* name(int part){static const char* names[]={"pelvis","belly","chest","head","upper arm L","upper arm R","forearm L","forearm R","thigh L","thigh R","shin L","shin R"};return part>=0&&part<Count?names[part]:"?";}

Carcass::Limit limit(int part){
 Carcass::Limit l;auto set=[&](int parent,float lo,float hi,float side,float twist,bool forward){l.parent=parent;l.bendLo=lo*Deg;l.bendHi=hi*Deg;l.side=side*Deg;l.twist=twist*Deg;l.flexForward=forward;};
 switch(part){
  case Belly:set(Pelvis,-20,40,20,20,true);break;
  case Chest:set(Belly,-20,35,20,20,true);break;
  case Head:set(Chest,-40,55,35,55,true);break;
  case UpperArmL:case UpperArmR:set(Chest,-50,150,85,70,true);break;
  case ForearmL:set(UpperArmL,-3,145,6,70,true);break;
  case ForearmR:set(UpperArmR,-3,145,6,70,true);break;
  case ThighL:case ThighR:set(Pelvis,-20,115,40,35,true);break;
  case ShinL:set(ThighL,-3,145,5,12,false);break;
  case ShinR:set(ThighR,-3,145,5,12,false);break;
  default:l.parent=-1;break;}
 return l;}

Labels label(const std::vector<MeshTriangle>& bind,const Arms* joints){
 Labels out;const int T=int(bind.size());out.part.assign(size_t(T),Chest);if(T==0)return out;
 // Corners that share a place are one vertex.
 std::vector<Point3> pts;std::vector<int> corner(size_t(T)*3);
 {std::unordered_map<std::uint64_t,int> cells;auto cell=[](Point3 p){auto q=[](float v){return std::uint64_t(std::int64_t(std::floor(v*500.f+.5f)))&0x1fffffu;};return q(p.x)|q(p.y)<<21|q(p.z)<<42;};
  for(int t=0;t<T;++t)for(int k=0;k<3;++k){auto p=bind[size_t(t)].v[k].p;auto [slot,fresh]=cells.try_emplace(cell(p),int(pts.size()));if(fresh)pts.push_back(p);corner[size_t(t*3+k)]=slot->second;}}
 const int V=int(pts.size());std::vector<std::vector<int>> around(static_cast<size_t>(V));
 for(int t=0;t<T;++t)for(int k=0;k<3;++k){int a=corner[size_t(t*3+k)],b=corner[size_t(t*3+(k+1)%3)];if(a!=b){around[size_t(a)].push_back(b);around[size_t(b)].push_back(a);}}
 float y0=1e30f,y1=-1e30f;for(auto p:pts){y0=std::min(y0,p.y);y1=std::max(y1,p.y);}const float H=std::max(.01f,y1-y0);out.height=H;
 auto h=[&](int v){return (pts[size_t(v)].y-y0)/H;};
 // The head is on the body's middle.
 float cx=0,cz=0;{int n=0;for(int v=0;v<V;++v)if(h(v)>.9f){cx+=pts[size_t(v)].x;cz+=pts[size_t(v)].z;++n;}if(n){cx/=float(n);cz/=float(n);}}
 // Across the shoulders is the way the body is widest there (or the way its shoulder joints lie).
 float lx=1,lz=0;
 if(joints){lx=(*joints)[1][0].x-(*joints)[0][0].x;lz=(*joints)[1][0].z-(*joints)[0][0].z;}
 else{// Models stand along x or z: across is x unless the body is far wider along z (arms out that way).
  float xx=0,zz=0;for(int v=0;v<V;++v)if(h(v)>.55f&&h(v)<.85f){float a=pts[size_t(v)].x-cx,c=pts[size_t(v)].z-cz;xx+=a*a;zz+=c*c;}
  if(zz>1.6f*xx){lx=0;lz=1;}}
 {float l=std::sqrt(lx*lx+lz*lz);if(l<1e-6f){lx=1;lz=0;}else{lx/=l;lz/=l;}}
 float fx=-lz,fz=lx;
 auto across=[&](int v){return (pts[size_t(v)].x-cx)*lx+(pts[size_t(v)].z-cz)*lz;};
 auto ahead=[&](int v){return (pts[size_t(v)].x-cx)*fx+(pts[size_t(v)].z-cz)*fz;};
 // The toes point forward of the shins.
 {float ff=0,sf=0;int nf=0,ns=0;for(int v=0;v<V;++v){if(h(v)<.04f){ff+=ahead(v);++nf;}else if(h(v)>.10f&&h(v)<.20f){sf+=ahead(v);++ns;}}
  if(nf&&ns&&ff/float(nf)<sf/float(ns)){fx=-fx;fz=-fz;}}
 out.forward={fx,0,fz};
 // The crotch: the lowest height the body's middle is filled. The hips are just under it, the knees
 // 60% of the way up to them (a little over a quarter of the height).
 float crotch=.47f;{for(float y=.2f;y<.6f;y+=.005f){bool filled=false;for(int v=0;v<V&&!filled;++v)filled=std::fabs(h(v)-y)<.006f&&std::fabs(across(v))<.015f*H&&std::fabs(ahead(v))<.12f*H;if(filled){crotch=y;break;}}}
 // Legs that stand together fill the middle low down: only a crotch where a man's is (42-52%) is believed.
 const float hip=crotch>.42f&&crotch<.52f?crotch-.01f:.46f,knee=std::clamp(hip*.6f,.25f,.29f);out.hip=hip;out.knee=knee;
 std::vector<int> arm(size_t(V),-1),part(size_t(V),Chest);
 auto distance=[](Point3 p,Point3 a,Point3 b,float& t){Point3 ab=b-a,ap=p-a;float l=ab.x*ab.x+ab.y*ab.y+ab.z*ab.z;t=l>1e-9f?std::clamp((ap.x*ab.x+ap.y*ab.y+ap.z*ab.z)/l,0.f,1.f):0;Point3 d=ap-ab*t;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);};
 if(joints){
  // Told where the shoulders, elbows and wrists are: an arm is what lies along them (the hand on past the wrist).
  for(int v=0;v<V;++v){float best=.065f*H;int got=-1,seg=0;
   for(int k=0;k<2;++k){const auto&j=(*joints)[size_t(k)];const Point3 tip=j[2]+(j[2]-j[1])*.6f;float t;
    const float side=(j[0].x-cx)*lx+(j[0].z-cz)*lz;if((across(v)<0)!=(side<0))continue;
    float d0=distance(pts[size_t(v)],j[0],j[1],t);if(t<=0&&pts[size_t(v)].y>j[0].y)d0+=.03f*H;const float d1=distance(pts[size_t(v)],j[1],tip,t);
    if(d0<best){best=d0;got=k;seg=0;}if(d1<best){best=d1;got=k;seg=1;}}
   if(got>=0){const float side=((*joints)[size_t(got)][0].x-cx)*lx+((*joints)[size_t(got)][0].z-cz)*lz;const int k=side<0?0:1;arm[size_t(v)]=k;part[size_t(v)]=seg?(k?ForearmR:ForearmL):(k?UpperArmR:UpperArmL);}}
 }else{
  // Arms: between hip and armpit they are apart from the trunk.
  std::vector<int> comp(size_t(V),-1);std::vector<int> sizes;std::vector<float> sums;
  auto inBand=[&](int v){return h(v)>.40f&&h(v)<.68f;};
  for(int s=0;s<V;++s){if(!inBand(s)||comp[size_t(s)]>=0)continue;const int id=int(sizes.size());sizes.push_back(0);sums.push_back(0);std::vector<int> stack{s};comp[size_t(s)]=id;
   while(!stack.empty()){int v=stack.back();stack.pop_back();++sizes[size_t(id)];sums[size_t(id)]+=across(v);for(int u:around[size_t(v)])if(inBand(u)&&comp[size_t(u)]<0){comp[size_t(u)]=id;stack.push_back(u);}}}
  int trunk=-1;{float best=1e30f;for(int v=0;v<V;++v)if(comp[size_t(v)]>=0){float d=std::fabs(across(v))+std::fabs(ahead(v))+std::fabs(h(v)-.55f)*H;if(d<best){best=d;trunk=comp[size_t(v)];}}}
  int side[2]{-1,-1};
  for(int c=0;c<int(sizes.size());++c){if(c==trunk||sizes[size_t(c)]<10)continue;const float m=sums[size_t(c)]/float(sizes[size_t(c)]);if(std::fabs(m)<.08f*H)continue;const int k=m<0?0:1;if(side[k]<0||sizes[size_t(c)]>sizes[size_t(side[k])])side[k]=c;}
  for(int k=0;k<2;++k){if(side[k]<0)continue;
   float inner=1e30f,lowest=1e30f;for(int v=0;v<V;++v)if(comp[size_t(v)]==side[k]){if(h(v)>.62f)inner=std::min(inner,std::fabs(across(v)));lowest=std::min(lowest,std::fabs(across(v)));}
   if(inner>1e29f)inner=lowest;
   std::vector<int> stack;for(int v=0;v<V;++v)if(comp[size_t(v)]==side[k]){arm[size_t(v)]=k;stack.push_back(v);}
   while(!stack.empty()){int v=stack.back();stack.pop_back();for(int u:around[size_t(v)]){if(arm[size_t(u)]>=0)continue;const float hu=h(u),au=std::fabs(across(u));
     const bool up=hu>=.68f&&hu<.86f&&au>inner-.01f*H,down=hu<=.40f&&hu>.25f&&au>lowest-.02f*H;if(!up&&!down)continue;if((across(u)<0)!=(k==0))continue;arm[size_t(u)]=k;stack.push_back(u);}}}
  // No arm stood apart (arms out wide): what is out past the shoulders above the hips.
  if(side[0]<0&&side[1]<0)for(int v=0;v<V;++v)if(h(v)>.5f&&h(v)<.88f&&std::fabs(across(v))>.15f*H)arm[size_t(v)]=across(v)<0?0:1;
  for(int k=0;k<2;++k){
   // Along the arm from where it meets the chest to its farthest point.
   const Point3 chest{cx,y0+.8f*H,cz};int nearest=-1;float closest=1e30f;
   for(int v=0;v<V;++v)if(arm[size_t(v)]==k){Point3 d=pts[size_t(v)]-chest;float s=d.x*d.x+d.y*d.y+d.z*d.z;if(s<closest){closest=s;nearest=v;}}if(nearest<0)continue;
   Point3 S{};int n=0;for(int v=0;v<V;++v)if(arm[size_t(v)]==k){Point3 d=pts[size_t(v)]-pts[size_t(nearest)];if(d.x*d.x+d.y*d.y+d.z*d.z<.0025f*H*H){S=S+pts[size_t(v)];++n;}}S=S*(1.f/float(n));
   Point3 Tip=S;float far=-1;for(int v=0;v<V;++v)if(arm[size_t(v)]==k){Point3 d=pts[size_t(v)]-S;float s=d.x*d.x+d.y*d.y+d.z*d.z;if(s>far){far=s;Tip=pts[size_t(v)];}}
   const Point3 axis=Tip-S;const float len2=std::max(1e-6f,axis.x*axis.x+axis.y*axis.y+axis.z*axis.z);
   for(int v=0;v<V;++v)if(arm[size_t(v)]==k){Point3 d=pts[size_t(v)]-S;float u=(d.x*axis.x+d.y*axis.y+d.z*axis.z)/len2;part[size_t(v)]=u<.45f?(k?UpperArmR:UpperArmL):(k?ForearmR:ForearmL);}}
 }
 out.ok=true;for(int k=0;k<2;++k){int n=0;for(int v=0;v<V;++v)n+=arm[size_t(v)]==k;out.ok&=n>8;}
 for(int v=0;v<V;++v){if(arm[size_t(v)]>=0)continue;const float y=h(v);const bool left=across(v)<0;
  part[size_t(v)]=y>.845f?Head:y>=.70f?Chest:y>=hip+.12f?Belly:y>=hip?Pelvis:y>=knee?(left?ThighL:ThighR):(left?ShinL:ShinR);}
 for(int t=0;t<T;++t){int a=part[size_t(corner[size_t(t*3)])],b=part[size_t(corner[size_t(t*3+1)])],c=part[size_t(corner[size_t(t*3+2)])];out.part[size_t(t)]=b==c?b:a;}
 return out;}

Carcass::Rig rig(const Labels& labels,const std::vector<MeshTriangle>& bind,const std::vector<int>& faces){
 Carcass::Rig r;r.forward=labels.forward;r.part.reserve(faces.size());r.bind.reserve(faces.size());
 for(int f:faces){r.part.push_back(f>=0&&size_t(f)<labels.part.size()?labels.part[size_t(f)]:Chest);r.bind.push_back(f>=0&&size_t(f)<bind.size()?bind[size_t(f)]:MeshTriangle{});}
 for(int p=0;p<Count;++p)r.limits.push_back(limit(p));
 return r;}
}
