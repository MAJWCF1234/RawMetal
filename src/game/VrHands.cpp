// VR hands as bodies (Vectra): each tracked hand that holds nothing is a kinematic sphere in the prop solver, so
// it pushes what it meets by contact, and its drawn place (the proxy) is held out of every prop.
//  - The hand's body sits where the drawn hand is, moving toward the tracked hand at no more than 2.5 m/s: a
//    swat moves a light thing at most that fast, so a hand never launches what it brushes.
//  - Props of 12 kg and up, pinned and jointed-to-the-room ones are left out of its contacts: the drawn hand
//    stops at their surface and they do not move (a hand does not shove a drum or a filing cabinet about).
//  - After each step the drawn hand goes on toward the tracked one 2 cm at a time, set out of each prop it would
//    be inside along that prop's nearest face: it never passes into one or through a thin one.
//  - The renderer draws each hand at its proxy (Scene3D drawTrackedHands, Game::vrHandOffset).
#include "Game.h"
#include "physics/PropBodies.h"
#include <cmath>
namespace retro {
namespace {
constexpr float HandRadius=.045f,HandPush=2.5f,HeavyHand=12.f,Stride=.02f;
float lengthOf(Point3 d){return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
}
bool Game::vrHandFree(int hand)const{
 if(!m_vrInputActive||m_noclip||m_inspectionCamera||hand<0||hand>1||!m_vrHandValid[size_t(hand)])return false;
 return !(holdingClutter()&&(hand==m_vrCarryHand||hand==m_vrSecondHand));}
bool Game::vrHandsNear(const std::vector<Clutter>& list)const{
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand))continue;const Point3 p=m_vrHandPosition[size_t(hand)],q=m_vrHandProxySet[size_t(hand)]?m_vrHandProxy[size_t(hand)]:p;
  for(const auto&c:list){if(c.model<0||c.broken)continue;const auto e=c.extent();const float r=HandRadius+.25f;
   for(const Point3 at:{p,q})if(std::fabs(at.x-c.pos.x)<e[0]+r&&std::fabs(at.y-c.pos.y)<e[1]+r&&at.z>c.z-r&&at.z<c.z+2*e[2]+r)return true;}}
 return false;}
void Game::addVrHandBodies(vectra::Space& space,std::vector<Clutter>& list,float h){
 using namespace vectra;static Shape ball=[]{Shape s;s.add(PartKind::Sphere,{},{HandRadius,HandRadius,HandRadius});s.finish();return s;}();(void)list;
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand))continue;auto&proxy=m_vrHandProxy[size_t(hand)];if(!m_vrHandProxySet[size_t(hand)]){proxy=m_vrHandPosition[size_t(hand)];m_vrHandProxySet[size_t(hand)]=true;}
  Body b;b.shape=&ball;b.x={proxy.x,proxy.y,proxy.z};b.kinematic=true;b.setMass(0);b.gravity={};b.material={.8f,.7f,0,.002f};b.sleeping=false;b.sleepTime=0;
  const Point3 to=m_vrHandPosition[size_t(hand)]-proxy;V3 v=V3{to.x,to.y,to.z}*(1/std::max(h,1e-4f));const float s=len(v);if(s>HandPush)v=v*(HandPush/s);b.v=v;
  // What it is about to touch wakes (a kinematic body does not wake a sleeper by contact alone).
  if(s>.02f)for(auto&o:space.bodies)if(o.user>=0&&o.sleeping&&vrHandMeets(o)&&o.shape&&lenSq(o.x-b.x)<(o.shape->bound+HandRadius+.05f)*(o.shape->bound+HandRadius+.05f)){o.sleeping=false;o.sleepTime=0;}
  b.user=VrHandBody-hand;b.id=(1<<19)-40-hand;space.bodies.push_back(b);}}
bool Game::vrHandMeets(const vectra::Body& other)const{return !other.kinematic&&other.invMass>=1/HeavyHand;}
void Game::settleVrHands(const vectra::Space& space,int held){
 using namespace vectra;
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand)||!m_vrHandProxySet[size_t(hand)])continue;auto&proxy=m_vrHandProxy[size_t(hand)];const Point3 goal=m_vrHandPosition[size_t(hand)];
  Point3 way=goal-proxy;const float d=lengthOf(way);const int n=std::max(1,int(std::ceil(d/Stride)));bool blocked=false;
  for(int k=0;k<n;++k){Point3 p=proxy+way*(1.f/float(n));
   for(const auto&b:space.bodies){if(b.user<0||b.user==held||!b.shape)continue;
    if(std::fabs(b.x.x-p.x)>b.shape->bound+HandRadius||std::fabs(b.x.y-p.y)>b.shape->bound+HandRadius||std::fabs(b.x.z-p.z)>b.shape->bound+HandRadius)continue;
    for(int part=0;part<b.shape->count;++part){V3 surface,normal;float distance;if(!nearestPoint(b,part,{p.x,p.y,p.z},surface,normal,distance))continue;
     if(distance<HandRadius){const float push=HandRadius-distance+.001f;p=p+Point3{normal.x,normal.y,normal.z}*push;blocked=true;}}}
   proxy=p;}
  m_vrHandBlocked[size_t(hand)]=blocked||lengthOf(goal-proxy)>.01f;}}
void Game::followVrHands(){
 // A frame with no prop step near a hand: the drawn hand is where the tracked one is.
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand)){m_vrHandProxySet[size_t(hand)]=false;m_vrHandBlocked[size_t(hand)]=false;continue;}
  if(!m_vrHandStepped){m_vrHandProxy[size_t(hand)]=m_vrHandPosition[size_t(hand)];m_vrHandProxySet[size_t(hand)]=true;m_vrHandBlocked[size_t(hand)]=false;}}
 m_vrHandStepped=false;}
}
