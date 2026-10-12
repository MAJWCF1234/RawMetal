#pragma once
#include <algorithm>
#include <cstdint>
namespace retro {
enum class GuardAction {None,Fire,Bash,Reload};
enum GuardActionEvent : unsigned {GuardActionStart=1,GuardActionWork=2,GuardActionChamber=4,GuardActionComplete=8};
constexpr int guardActionDuration(GuardAction action){return action==GuardAction::Fire?220:action==GuardAction::Bash?520:action==GuardAction::Reload?1800:0;}
constexpr unsigned guardActionMask(GuardAction action,int elapsed){
 if(action==GuardAction::None)return 0;
 unsigned mask=GuardActionStart;
 if(elapsed>=(action==GuardAction::Fire?120:action==GuardAction::Bash?300:600))mask|=GuardActionWork;
 if(action==GuardAction::Reload&&elapsed>=1200)mask|=GuardActionChamber;
 if(elapsed>=guardActionDuration(action))mask|=GuardActionComplete;
 return mask;
}
// Persist consumed alongside elapsed. A jump crosses each event exactly once,
// and restoring the same phase never replays an earlier sound or damage event.
inline unsigned consumeGuardAction(GuardAction action,int elapsed,unsigned& consumed){auto passed=guardActionMask(action,elapsed);auto events=passed&~consumed;consumed|=passed;return events;}
inline bool validGuardAction(GuardAction action,int elapsed,unsigned consumed){return int(action)>=0&&int(action)<=3&&elapsed>=0&&elapsed<=guardActionDuration(action)&&consumed==guardActionMask(action,elapsed);}
}
