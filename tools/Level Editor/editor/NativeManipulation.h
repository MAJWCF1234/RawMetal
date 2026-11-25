#pragma once
#include "NativeObjects.h"
#include <array>

inline size_t nativeField(const std::vector<std::string>& names,const std::vector<std::string>& fields,const char* name) {
  auto i=std::find(names.begin(),names.end(),name);
  return i==names.end()||size_t(i-names.begin())>=fields.size()?size_t(-1):size_t(i-names.begin());
}
inline bool nativeLineKind(const std::string& kind) {return kind=="PIPE"||kind=="LEAD"||kind=="DOOR";}
inline size_t nativeModelWidth(const std::vector<std::string>& fields) {
  return nativeLooseModel(fields) ? 8 : 6;
}
inline float nativeModelLimit(const std::vector<std::string>& fields) {
  return nativeLooseModel(fields) ? 3.f : 100000.f;
}
inline bool scaleNativeModel(std::vector<std::string>& fields,float requested) {
  if(!nativeSizedModel(fields)||!std::isfinite(requested)||requested<=0.f||requested==1.f)return false;
  const size_t width=nativeModelWidth(fields);
  const std::array<float,3> dimensions={std::stof(fields[width]),std::stof(fields[width+1]),std::stof(fields[width+2])};
  const auto [smallest,largest]=std::minmax_element(dimensions.begin(),dimensions.end());
  if(!std::isfinite(*largest)||*smallest<=0.f)return false;
  const float limit=nativeModelLimit(fields);
  // Preserve the aspect ratio of older models even when a dimension starts below the editor minimum.
  const float factor=std::clamp(requested,std::min(1.f,.01f / *smallest),std::max(1.f,limit / *largest));
  if(factor==1.f)return false;
  for(size_t i=0;i<dimensions.size();++i)
    fields[width+i]=nativeNumber(std::min(limit,dimensions[i]*factor));
  return true;
}
inline std::vector<std::pair<float,float>> nativeCorners(const std::vector<std::string>& names,const std::vector<std::string>& f) {
  auto h=nativeHandle(names,f);if(!h)return {};
  if(nativeLineKind(f[0])){std::vector<std::pair<float,float>> out;for(auto [x,y]:h->points)out.emplace_back(std::stof(f[x]),std::stof(f[y]));return out;}
  float yaw=0;auto at=nativeField(names,f,"yaw");if(h->points.size()==1&&at!=size_t(-1))yaw=std::stof(f[at]);else if(h->points.size()==1){at=nativeField(names,f,"facing");if(at!=size_t(-1))yaw=std::stof(f[at])*1.57079632679f;}
  float c=std::cos(yaw),s=std::sin(yaw);std::vector<std::pair<float,float>> out;
  for(auto [dx,dy]:{std::pair{-1.f,-1.f},{1.f,-1.f},{1.f,1.f},{-1.f,1.f}})
    out.emplace_back(h->x+c*dx*h->width*.5f+s*dy*h->depth*.5f,h->y-s*dx*h->width*.5f+c*dy*h->depth*.5f);
  return out;
}
// A model (a fixture or a loose prop) scales whole from a corner: width, depth and height by one factor, so it
// keeps its proportions (free stretches width and depth alone, the old way; the editor holds Alt for it).
inline bool resizeNative(std::vector<std::string>& f,const std::vector<std::string>& names,int corner,float x,float y,float snap,bool free=false) {
  auto h=nativeHandle(names,f);if(!h)return false;
  auto quantize=[&](float v){return snap>0?std::round(v/snap)*snap:v;};
  auto originalCorners=nativeCorners(names,f);
  if(corner<0||size_t(corner)>=originalCorners.size())return false;
  x=std::clamp(originalCorners[size_t(corner)].first+quantize(x-originalCorners[size_t(corner)].first),0.f,24.f);
  y=std::clamp(originalCorners[size_t(corner)].second+quantize(y-originalCorners[size_t(corner)].second),0.f,24.f);
  if(nativeLineKind(f[0])){
    if(corner<0||size_t(corner)>=h->points.size())return false;
    auto [xi,yi]=h->points[size_t(corner)];
    // A door end slides along its own axis (y for a door along Y) and keeps 0.9 m of leaf.
    if(f[0]=="DOOR"){bool alongY=h->points[0].first==h->points[1].first;auto [ox,oy]=h->points[size_t(1-corner)];float& v=alongY?y:x;float other=std::stof(f[alongY?oy:ox]);v=corner==0?std::min(v,other-.9f):std::max(v,other+.9f);}
    f[xi]=nativeNumber(x);f[yi]=nativeNumber(y);return true;
  }
  if(corner<0||corner>3)return false;
  auto corners=nativeCorners(names,f);auto fixed=corners[size_t((corner+2)%4)];
  constexpr float signs[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
  if(h->points.size()>=2){
    float l=signs[corner][0]<0?std::min(x,fixed.first-.25f):fixed.first;
    float r=signs[corner][0]>0?std::max(x,fixed.first+.25f):fixed.first;
    float n=signs[corner][1]<0?std::min(y,fixed.second-.25f):fixed.second;
    float b=signs[corner][1]>0?std::max(y,fixed.second+.25f):fixed.second;
    auto [ax,ay]=h->points[0];auto [bx,by]=h->points[1];
    f[ax]=nativeNumber(l);f[ay]=nativeNumber(n);f[bx]=nativeNumber(r);f[by]=nativeNumber(b);return true;
  }
  size_t wi=nativeField(names,f,"width"),di=nativeField(names,f,"depth");
  if(wi==size_t(-1))wi=nativeField(names,f,"w");if(di==size_t(-1))di=nativeField(names,f,"d");
  if(wi==size_t(-1)||di==size_t(-1))return false;
  float yaw=0;auto angle=nativeField(names,f,"yaw");if(angle!=size_t(-1))yaw=std::stof(f[angle]);else{angle=nativeField(names,f,"facing");if(angle!=size_t(-1))yaw=std::stof(f[angle])*1.57079632679f;}
  float c=std::cos(yaw),s=std::sin(yaw),dx=x-fixed.first,dy=y-fixed.second;
  const bool model=nativeSizedModel(f);
  const float maximum=nativeModelLimit(f);
  const float minWidth=model?std::min(.01f,h->width):.25f;
  const float minDepth=model?std::min(.01f,h->depth):.25f;
  float w=std::clamp((dx*c-dy*s)*signs[corner][0],minWidth,maximum);
  float d=std::clamp((dx*s+dy*c)*signs[corner][1],minDepth,maximum);
  if(model&&!free&&h->width>0&&h->depth>0){
    // Project the pointer onto the model diagonal so the handle follows the closest proportional corner.
    const float localW=(dx*c-dy*s)*signs[corner][0],localD=(dx*s+dy*c)*signs[corner][1];
    const float factor=std::max(.000001f,(localW*h->width+localD*h->depth)/(h->width*h->width+h->depth*h->depth));
    if(!scaleNativeModel(f,factor))return true;
    w=std::stof(f[wi]);d=std::stof(f[di]);
  }
  float localX=w*signs[corner][0],localY=d*signs[corner][1];
  auto [xi,yi]=h->points[0];f[xi]=nativeNumber(fixed.first+(c*localX+s*localY)*.5f);
  f[yi]=nativeNumber(fixed.second+(-s*localX+c*localY)*.5f);f[wi]=nativeNumber(w);f[di]=nativeNumber(d);return true;
}
inline bool shiftNativeElevation(std::vector<std::string>& f,const std::vector<std::string>& names,float delta) {
  if(delta==0)return false;
  bool changed=false;
  for(const char* name:{"z","az","bz","z1","z2","bottom","top","base","bed","surface","minZ","maxZ","endZ"}){
    auto at=nativeField(names,f,name);if(at==size_t(-1)||f[at].empty())continue;
    float v=std::stof(f[at]);if(v<=-900)continue;f[at]=nativeNumber(v+delta);changed=true;
  }return changed;
}
inline bool rayNativeBox(float ox,float oy,float oz,float dx,float dy,float dz,
                         float x,float y,float width,float depth,float bottom,float top,float yaw,float& distance) {
  float c=std::cos(yaw),s=std::sin(yaw),rx=ox-x,ry=oy-y;
  float px=rx*c-ry*s,py=rx*s+ry*c,vx=dx*c-dy*s,vy=dx*s+dy*c;
  float rayNear=0,rayFar=1e6f;
  auto slab=[&](float p,float v,float a,float b){if(std::abs(v)<1e-7f)return p>=a&&p<=b;
    float t=(a-p)/v,u=(b-p)/v;if(t>u)std::swap(t,u);rayNear=std::max(rayNear,t);rayFar=std::min(rayFar,u);return rayNear<=rayFar;};
  if(!slab(px,vx,-width*.5f,width*.5f)||!slab(py,vy,-depth*.5f,depth*.5f)||!slab(oz,dz,bottom,top))return false;
  distance=rayNear>.001f?rayNear:rayFar;return distance>.001f;
}
