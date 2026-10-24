#include "World.h"
#include "../core/Ray.h"

namespace retro {
float World::shotFraction(Point3 from,Point3 to,Point3& normal)const{
 normal={};auto delta=to-from;
 if(!std::isfinite(from.x)||!std::isfinite(from.y)||!std::isfinite(from.z)||!std::isfinite(to.x)||!std::isfinite(to.y)||!std::isfinite(to.z))return 0;
 float nearest=1;
 auto box=[&](Point3 lo,Point3 hi){if(hi.x<=lo.x||hi.y<=lo.y||hi.z<=lo.z)return;Point3 n;float t=rayBoxFraction(from,delta,lo,hi,&n);if(t<nearest){nearest=t;normal=n;}};
 auto rotated=[&](Vec2 position,float width,float depth,float bottom,float top,float c,float s){
  if(width<=0||depth<=0||top<=bottom)return;Point3 offset=from-Point3{position.x,position.y,(bottom+top)*.5f};
  Point3 local{offset.x*c-offset.y*s,offset.x*s+offset.y*c,offset.z},direction{delta.x*c-delta.y*s,delta.x*s+delta.y*c,delta.z},n;
  float t=rayBoxFraction(local,direction,{-width*.5f,-depth*.5f,(bottom-top)*.5f},{width*.5f,depth*.5f,(top-bottom)*.5f},&n);
  if(t<nearest){nearest=t;normal={n.x*c+n.y*s,-n.x*s+n.y*c,n.z};}
 };
 auto oriented=[&](Vec2 position,float width,float depth,float bottom,float top,float yaw){rotated(position,width,depth,bottom,top,std::cos(yaw),std::sin(yaw));};
 bool shapedGround=hasTerrain()||!m_waterVolumes.empty();
 if(!shapedGround){
  if(m_lightNodes.empty())buildLightOcclusion();
  if(!m_lightNodes.empty()){
   std::array<int,64> pending{};int count=1;pending[0]=0;
   while(count){const auto&node=m_lightNodes[pending[--count]];if(rayBoxFraction(from,delta,node.minimum,node.maximum)>=nearest)continue;
    if(node.left>=0){pending[count++]=node.left;pending[count++]=node.right;continue;}
    for(int i=node.begin;i<node.end;++i){const auto&s=m_lightSolids[size_t(i)];rotated({s.center.x,s.center.y},s.half.x*2,s.half.y*2,s.center.z-s.half.z,s.center.z+s.half.z,s.cosine,s.sine);}
   }
  }
 }
 // Optical visibility deliberately transmits glass. Ballistics stops at its
 // authored volume, including panes much thinner than a collision sample.
 auto structure=[&](const Structure&s){
  if((s.material==16||s.material==14||s.material==13)&&s.top-s.bottom<.04f)return;
  if(!s.rail){box({s.x1,s.y1,s.bottom},{s.x2,s.y2,s.top});return;}
  if(s.top-s.bottom>1.5f){
   if(hasLift()){
    box({s.x1,s.y1,s.bottom},{s.x2,s.y2,s.bottom+.16f});box({s.x1,s.y1,s.bottom+.92f},{s.x2,s.y2,s.bottom+.96f});
    int bars=std::max(1,int(std::max(s.x2-s.x1,s.y2-s.y1)/.4f));for(int i=1;i<=bars;++i){float t=float(i)/(bars+1),x=s.x1+(s.x2-s.x1)*t,y=s.y1+(s.y2-s.y1)*t;box({x-.012f,y-.012f,s.bottom+.16f},{x+.012f,y+.012f,s.top-.07f});}
   }else box({s.x1+.012f,s.y1+.012f,s.bottom+.012f},{s.x2-.012f,s.y2-.012f,s.top-.08f});
  }
  box({s.x1,s.y1,s.top-.07f},{s.x2,s.y2,s.top});box({s.x1,s.y1,s.bottom},{s.x1+.055f,s.y1+.055f,s.top-.07f});box({s.x2-.055f,s.y2-.055f,s.bottom},{s.x2,s.y2,s.top-.07f});
 };
 for(const auto&s:m_structures)if(shapedGround||s.material==17)structure(s);
 if(shapedGround){
  // The shadow BVH uses a flat floor. Curved terrain and basin beds must use
  // their actual collision field instead; all other authored solids stay exact.
  int x1=std::clamp(int(std::floor(std::min(from.x,to.x))),0,Width-1),x2=std::clamp(int(std::floor(std::max(from.x,to.x))),0,Width-1);
  int y1=std::clamp(int(std::floor(std::min(from.y,to.y))),0,Height-1),y2=std::clamp(int(std::floor(std::max(from.y,to.y))),0,Height-1);
  for(int y=y1;y<=y2;++y)for(int x=x1;x<=x2;++x){char t=tile(x,y);if(t!='#'&&t!='C'&&t!='B'&&t!='T')continue;float floor=floorHeight(x+.5f,y+.5f);box({float(x),float(y),floor},{float(x+1),float(y+1),t=='#'?wallHeight(x,y):floor+(t=='C'?.6f:t=='B'?1.1f:2.62f)});}
  for(const auto&p:m_props){float base=floorHeight(p.position.x,p.position.y)+p.base;box({p.position.x-p.halfSize.x,p.position.y-p.halfSize.y,base},{p.position.x+p.halfSize.x,p.position.y+p.halfSize.y,base+p.height});}
  for(const auto&f:m_fixtures)if(f.solid){float base=floorHeight(f.position.x,f.position.y)+f.base;
   if(f.model==7){for(float tier:{.17f,.54f,.92f}){float top=base+f.height*tier;oriented(f.position,f.width,f.depth,top-.045f,top,f.yaw);}
    float c=std::cos(f.yaw),s=std::sin(f.yaw);for(float x:{-f.width*.5f+.02f,f.width*.5f-.02f})for(float y:{-f.depth*.5f+.02f,f.depth*.5f-.02f})oriented({f.position.x+x*c+y*s,f.position.y-x*s+y*c},.04f,.04f,base,base+f.height,f.yaw);
   }else oriented(f.position,f.width,f.depth,base,base+f.height,f.yaw);
  }
 }
 // Traveling controls are absent from the cached static BVH.
 for(const auto&t:m_terminals)if(shapedGround||(hasLift()&&t.control)){
  float base=floorHeight(t.position.x,t.position.y)+t.z;
  if(campaign()&&level()>=6&&!t.control&&t.reactorAction<2){box({t.position.x-.48f,t.position.y-.36f,base+.74f},{t.position.x+.42f,t.position.y+.36f,base+.815f});oriented(t.position,.386509f,.53235f,base+.815f,base+1.32005f,level()==6?-kPi*.5f:t.yaw);}
  else oriented(t.position,.54f,.36f,base,base+.95f,t.yaw);
 }
 for(const auto&door:m_doors){float base=floorHeight((door.left+door.right)*.5f,door.y)+door.z;
  if(door.swinging){float angle=door.open*kPi*.5f*door.swing,width=door.right-door.left;oriented({door.left+std::cos(angle)*width*.5f,door.y+std::sin(angle)*width*.5f},width,.30f,base+.015f,base+2.35f,-angle);}
  else box({door.left,door.y-.13f,base+door.open*2.65f+.015f},{door.right,door.y+.13f,base+door.open*2.65f+2.48f});
  if(shapedGround)box({door.left,door.y-.1f,base+2.5f},{door.right,door.y+.1f,ceilingHeight((door.left+door.right)*.5f,door.y)});
 }
 for(size_t i=0;i<actorTracks().size();++i)if(actorTracks()[i].platform){const auto&t=actorTracks()[i];const auto&p=m_actorPoses[i];box({p.position.x-t.footprint.x*.5f,p.position.y-t.footprint.y*.5f,p.z-t.thickness},{p.position.x+t.footprint.x*.5f,p.position.y+t.footprint.y*.5f,p.z});}
 if(m_cargoLift.x2>m_cargoLift.x1){box({m_cargoLift.x1,m_cargoLift.y1,m_cargoLiftHeight-.18f},{m_cargoLift.x2,m_cargoLift.y2,m_cargoLiftHeight});box({m_cargoLift.x1,m_cargoLift.y1,m_cargoLiftHeight+2.4f},{m_cargoLift.x2,m_cargoLift.y2,m_cargoLiftHeight+2.52f});}
 if(hasLift()){
  box({10,10,m_liftHeight-.25f},{14,14,m_liftHeight});box({10,10,m_liftHeight+2.6f},{14,14,m_liftHeight+2.8f});
  box({10,10,m_liftHeight},{10.12f,14,m_liftHeight+2.6f});box({13.88f,10,m_liftHeight},{14,14,m_liftHeight+2.6f});
  auto end=[&](float y1,float y2,bool open){if(open){box({10,y1,m_liftHeight},{11,y2,m_liftHeight+2.6f});box({13,y1,m_liftHeight},{14,y2,m_liftHeight+2.6f});}else box({10,y1,m_liftHeight},{14,y2,m_liftHeight+2.6f});};
  end(10,10.12f,m_liftPhase==LiftPhase::Ready);end(13.88f,14,m_liftPhase==LiftPhase::Crashed&&m_liftTimer>=3.65f);
 }
 if(shapedGround){
  // Clip volumetric sampling to this chunk, so adjacent streamed chunks do
  // not inherit a clamped density field outside its authored footprint.
  float begin=0,finish=nearest;
  auto clip=[&](float p,float v,float lo,float hi){if(std::fabs(v)<1.e-8f)return p>=lo&&p<=hi;float a=(lo-p)/v,b=(hi-p)/v;if(a>b)std::swap(a,b);begin=std::max(begin,a);finish=std::min(finish,b);return finish>=begin;};
  if(clip(from.x,delta.x,0,float(Width))&&clip(from.y,delta.y,0,float(Height))&&finish>=begin){
   auto occupied=[&](float t){auto p=from+delta*t;return hasTerrain()?terrainDensity(p.x,p.y,p.z)>0:p.z<floorHeight(p.x,p.y)-.025f||p.z>ceilingHeight(p.x,p.y)+.005f;};
   float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);int steps=std::clamp(int(std::ceil((finish-begin)*length/.04f)),1,4096);float previous=begin;
   for(int i=0;i<=steps;++i){float t=begin+(finish-begin)*float(i)/steps;if(!occupied(t)){previous=t;continue;}float air=previous,solid=t;
    for(int iteration=0;iteration<10&&solid>air;++iteration){float middle=(air+solid)*.5f;if(occupied(middle))solid=middle;else air=middle;}
    if(solid<nearest){nearest=solid;auto p=from+delta*solid;
     if(hasTerrain()){float e=.02f;Point3 gradient{terrainDensity(p.x-e,p.y,p.z)-terrainDensity(p.x+e,p.y,p.z),terrainDensity(p.x,p.y-e,p.z)-terrainDensity(p.x,p.y+e,p.z),terrainDensity(p.x,p.y,p.z-e)-terrainDensity(p.x,p.y,p.z+e)};float magnitude=std::sqrt(gradient.x*gradient.x+gradient.y*gradient.y+gradient.z*gradient.z);normal=magnitude>1.e-5f?gradient*(1/magnitude):delta*(-1/std::max(length,1.e-5f));}
     else normal=p.z<floorHeight(p.x,p.y)?Point3{0,0,1}:Point3{0,0,-1};
    }break;
   }
  }
 }
 return nearest;
}
}
