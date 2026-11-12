#pragma once
#include "Game.h"

namespace retro {
struct CreatureAnimationSample {int clip=0;float phase=0;};
inline CreatureAnimationSample creatureAnimationSample(const Enemy&e,float elapsed){
 const auto&t=creatureTraits(e.kind);CreatureAnimationSample p{0,std::fmod(elapsed/t.idleSeconds+e.home.x*.1f,1.f)};
 // Where in its attack clip the blow lands (the Chimera's slam at 58%, the machine's swing at a third).
 const float contact=t.contact;
 if(!e.alive)p={4,std::min(1.f,e.deathTime/t.deathSeconds)};
 // Asleep among the plant it stands frozen on the first frame of its idle.
 else if(e.sleeping())p={0,0};
 else if(e.windup>0)p={2,(1-std::clamp(e.windup/t.windup,0.f,1.f))*contact};
 else if(e.strike>0)p={2,contact+(1-e.strike)*(1-contact)};
 else if(e.painFlash>0)p={3,1-e.painFlash};
 else if(e.moving)p={1,std::fmod(e.gait/(2*kPi),1.f)};
 // Over a kill it stays down on the body, worrying it: the bite span of its
 // attack swung back and forth.
 if(e.alive&&e.feeding>0&&t.feedSeconds>0&&e.painFlash<=0)p={2,t.worryFrom+(t.worryTo-t.worryFrom)*(.5f-.5f*std::cos(elapsed*4.4f+e.home.y))};
 if(e.kind==Enemy::Kind::Transformed&&e.alive){
  if(e.stalkMode==Enemy::StalkMode::Rush)p={2,.15f*(1-std::clamp(e.stalkTimer/TransformedLeap,0.f,1.f))};
  else if(e.stalkMode==Enemy::StalkMode::Grab)p={2,.15f+.85f*(1-std::clamp(e.stalkTimer/TransformedHold,0.f,1.f))};
  else if(e.stalkMode==Enemy::StalkMode::Recover)p={3,.2f+.8f*(1-std::clamp(e.stalkTimer/TransformedStumble,0.f,1.f))};
  else if(e.windup>0)p={5,(1-std::clamp(e.windup/t.windup,0.f,1.f))*.55f};
  else if(e.strike>0)p={5,.55f+(1-e.strike)*.45f};
 }
 return p;
}
inline void advanceCreatureAnimation(Enemy&e,float elapsed,float dt){
 if(!creatureTraits(e.kind).baked)return;
 auto p=creatureAnimationSample(e,elapsed);
 if(e.visualClip<0){e.visualClip=e.previousVisualClip=p.clip;e.visualPhase=e.previousVisualPhase=p.phase;e.visualTransition=1;return;}
 if(p.clip!=e.visualClip){e.previousVisualClip=e.visualClip;e.previousVisualPhase=e.visualPhase;e.visualClip=p.clip;e.visualTransition=0;}
 e.visualPhase=p.phase;e.visualTransition=std::min(1.f,e.visualTransition+dt/.12f);
}
}
