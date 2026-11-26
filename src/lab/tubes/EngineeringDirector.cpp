#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "world/HunterPolicy.h"

namespace retro::lab {
namespace {
struct EngineeringResult {
  bool lockdown = false, alarm = false, first = false, second = false, third = false;
  bool beforeThird = false, power = false, voice = false, clank = false, returnToFight = false;
  int breakers = 0, litRooms = 0;
  bool stalkerRetreated = false;
  bool stalkerPolicy = false, stalkerAwake = false, torch = false;
};
thread_local EngineeringResult result;
const Door* door(const Game& game, std::string_view label) {
  for (const auto& door : game.world().doors()) if (door.label == label) return &door;
  return nullptr;
}
bool open(const Game& game, std::string_view label) {
  const auto* found = door(game,label);return found&&found->open>.55f;
}
bool locked(const Game& game, std::string_view label) {
  const auto* found = door(game,label);return found&&LabAccess::scriptDoorLocked(game,*found);
}
const bool registered = add({
  .name = "engineering-director",
  .summary = "Run the authored Service Spine Director on campaign map 180.",
  .fixtures = {"engineering-director"},
  .setup = [](Scene& scene) {
    result={};LabAccess::setGod(scene.g(),true);LabAccess::playerCamera(scene.g());
    auto& player=LabAccess::player(scene.g());player.pos={19.f,14.5f};player.angle=0;
    LabAccess::engineeringTorch(scene.g());
  },
  .step = [](Scene& scene,float) {
    auto& game=scene.g();
    const int frame=scene.frame;
    if(frame==49||frame==50||frame==53) {
      const int number=frame==49?1:frame==50?2:3;
      const auto flag=stateId("eng_breaker_"+std::to_string(number));
      const auto& terminals=game.world().terminals();
      for(size_t i=0;i<terminals.size();++i)if(terminals[i].activateState==flag)
        result.breakers+=bool(LabAccess::useAuthoredTerminal(game,int(i))&2);
    }
    if(frame==19||frame==23||frame==45||frame==46) {
      auto& player=LabAccess::player(game);player.pos={12.8f,14.5f};player.angle=-kPi*.5f;
    }
    if(frame==58) {auto& player=LabAccess::player(game);player.pos={12.f,16.5f};player.angle=kPi*.5f;}
    if(frame==110) {
      auto& player=LabAccess::player(game);
      player.pos={17.8f,20.1f};player.z=game.world().definition().spawnHeight;
      player.angle=kPi*.5f;player.pitch=0;
      LabAccess::engineeringTorch(game);
    }
    for(int tick=0;tick<19;++tick) {
      game.update({},.05f);
      for(const auto& enemy:LabAccess::enemies(game))
        if(enemy.hunter==stateId("eng_skinbot")&&enemy.stalkMode==Enemy::StalkMode::Retreat)
          result.stalkerRetreated=true;
      for(const auto& sound:LabAccess::sounds(game)){
        result.alarm|=sound.sound==Sound::PaSystemLockdown;
        if(!LabAccess::scriptState(game,stateId("eng_quiet_phase")))continue;
        result.voice|=sound.sound==Sound::PaEvacuate;
        result.clank|=sound.sound==Sound::OneMetalDrag;
      }
    }
    if(frame==6) {
      result.lockdown=locked(game,"Robotics hall doors")&&!open(game,"Robotics hall doors");
      result.first=open(game,"Cage row 1")&&!open(game,"Cage row 2")&&!open(game,"Cage row 3");
    }
    if(frame==26)result.second=open(game,"Cage row 1")&&open(game,"Cage row 2")&&!open(game,"Cage row 3");
    if(frame==46)result.third=open(game,"Cage row 1")&&open(game,"Cage row 2")&&open(game,"Cage row 3");
    if(frame==51)result.beforeThird=locked(game,"Ch3 door")&&locked(game,"Wall vent");
    if(frame==58) {
      result.power=!locked(game,"Ch3 door")&&open(game,"Ch3 door")&&open(game,"Wall vent")&&
        LabAccess::scriptState(game,stateId("eng_power_restored"));
      for(const auto& lamp:game.world().lights())
        if(lamp.enabledState&&LabAccess::scriptLampPower(game,lamp)>.1f)++result.litRooms;
    }
    if(frame>=110) {
      result.stalkerPolicy|=LabAccess::scriptState(game,hunterLightRetreatState(stateId("eng_skinbot")))!=0;
      result.stalkerAwake|=LabAccess::scriptState(game,stateId("eng_stalker_awake"))!=0;
      result.torch|=game.hasFlashlight()&&LabAccess::scriptState(game,stateId("flashlight_on"))!=0;
    }
    result.returnToFight|=LabAccess::scriptState(game,stateId("eng_fight_resumed"))!=0;
  },
  .capture = [](Scene& scene,const Frame&) {
    if(scene.frame==6)scene.shoot("01-lockdown");
    if(scene.frame==26)scene.shoot("02-second-cage-row");
    if(scene.frame==58)scene.shoot("03-power-restored");
  },
  .metrics = [](Scene& scene,Report& report) {
    report.check("campaign map 180 loaded",scene.g().level()==180,"Service Spine");
    report.check("the entry physically closes and locks, and the alarm sounds",result.lockdown&&result.alarm,"Robotics hall");
    report.check("only row one opens, then two, then three",result.first&&result.second&&result.third,"0 / 20 / 40 seconds");
    report.check("all three physical controls restore the lamps and release both exits",
                 result.breakers==3&&result.litRooms>=3&&result.beforeThird&&result.power,"Wall vent and Ch3 door");
    report.check("both fakes sound during quiet, then fight resumes",result.voice&&result.clank&&result.returnToFight,
      std::string("voice=")+(result.voice?"yes":"no")+", clank="+(result.clank?"yes":"no")+
      ", fight resumed="+(result.returnToFight?"yes":"no"));
    report.check("the sub-bay hunter retreats from a healthy player with the torch on",result.stalkerRetreated,
      std::string("policy=")+(result.stalkerPolicy?"yes":"no")+", awake="+(result.stalkerAwake?"yes":"no")+
      ", torch="+(result.torch?"yes":"no"));
  }
});
}
}
