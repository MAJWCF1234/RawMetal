// The skinbot walking in the plain room: over a low slab, past a crate it steps onto, round a turn. Nothing about the
// walk is authored (world/SkinbotWalker.h); this films it drawn. The rules have their own test (--each-test=skinbot-walk).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/SkinbotWalker.h"
#include "world/ModelIds.h"
#include "game/EngineRegressionScene.h"
#include "game/SkinbotImpact.h"
#include <fstream>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
constexpr int Frames=2400;   // 40 s at 60 a second
const bool skinbotWalk=add({
 .name="skinbot-walk",.summary="the skinbot walks over a slab and a crate and turns",
 .fixtures={"skinbot-walk"},
 .author=[](Scene&,AuthoredMapData& m){const float k=2.4f/1.474f;
  m.fixtures.push_back({skinbot::Model,{6.f,12.f},0,1.451f*k,1.903f*k,2.4f,1.5707963f,false});   // facing +x, along its way
  // The climbing course, west to east: a low wall, a slab longer than it is, two thin walls, a stair of blocks, crates.
  m.structures.push_back({7.9f,9.8f,8.2f,14.4f,0,.35f,false,3});
  m.structures.push_back({9.3f,9.8f,12.2f,14.4f,0,.28f,false,3});
  m.structures.push_back({13.1f,9.8f,13.35f,14.4f,0,.5f,false,3});
  m.structures.push_back({14.3f,9.8f,14.55f,14.4f,0,.3f,false,3});
  m.structures.push_back({15.4f,10.2f,16.2f,14.f,0,.22f,false,3});
  m.structures.push_back({16.2f,10.2f,17.f,14.f,0,.44f,false,3});
  m.structures.push_back({17.f,10.2f,17.8f,14.f,0,.22f,false,3});
  m.structures.push_back({11.f,12.5f,11.8f,13.2f,.28f,.62f,false,3});
  m.structures.push_back({18.6f,11.f,19.3f,11.7f,0,.5f,false,3});
  m.structures.push_back({11.8f,21.45f,13.8f,22.8f,0,1.3f,false,3});   // what the camera stands on, behind its own view
 },
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){Game& g=s.g();
  LabAccess::setCamera(g,{12.8f,21.55f},1.3f,-1.5707963f,-15.f);   // wide: the whole course in the frame
  if(s.frame==30)g.pointSkinbot({19.8f,12.f});
  if(const auto* w=g.skinbotWalker()){s.board["high"]=std::max(s.board["high"],double(w->z));s.board["x"]=std::max(s.board["x"],double(w->x));
   if(s.frame>200&&w->arrived()&&s.board["turned"]==0){s.board["turned"]=1;g.pointSkinbot({6.f,12.4f});}
   s.board["slips"]=w->curse.slips;s.board["hesitations"]=w->curse.hesitations;s.board["freezes"]=w->curse.freezes;}},
 .capture=[](Scene& s,const Frame&){if(s.frame>=30&&s.frame<Frames&&s.frame%3==0){char l[16];std::snprintf(l,sizeof l,"v%04d",s.frame);s.shoot(l);}},
 .metrics=[](Scene& s,Report& r){
  r.check("it walks across the course",s.board["x"]>18.5,std::to_string(s.board["x"]));
  r.check("it makes short wind-ups before its bursts",s.board["freezes"]>0,std::to_string(int(s.board["freezes"])));
  r.check("its body rides up over the slab",s.board["high"]>.2,std::to_string(s.board["high"]));}
});
const bool starterCourse=add({
 .name="starter-skinbot-course",.summary="The actual Showcase Sound exhibit walks, climbs, turns and comes back unprompted",
 .fixtures={"starter-skinbot-course"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){
  auto& game=s.g();
  LabAccess::setCamera(game,{12.8f,18.6f},.4f,-1.5707963f,-12.f);
  for(int tick=0;tick<19;++tick){
   game.update({},.05f);
   if(const auto* walker=game.skinbotWalker()){
    s.board["high"]=std::max(s.board["high"],double(walker->z));
    s.board["east"]=std::max(s.board["east"],double(walker->x));
    if(s.board["east"]>19.1&&walker->x<7.f)s.board["returned"]=1;
   }
  }
 },
 .capture=[](Scene& s,const Frame&){
  if(s.frame==5)s.shoot("01-start-of-course");
  if(s.frame==12)s.shoot("02-crossing-obstacles");
  if(s.frame==25)s.shoot("03-turn-and-return");
 },
 .metrics=[](Scene& s,Report& r){
  r.check("the exhibit runs in the actual Showcase Sound map",s.g().level()==42,"Starter Map chunk 42");
  r.check("the creature walks the full eastbound course",s.board["east"]>19.1,std::to_string(s.board["east"]));
  r.check("the body rises over the obstacles",s.board["high"]>.2,std::to_string(s.board["high"]));
  r.check("the creature turns and walks back without being pointed",s.board["returned"]>0,"both directions");
 }
});
const bool liveProps=add({
 .name="starter-skinbot-props",.summary="Props brought into Showcase Sound change the Skinbot's walk",
 .fixtures={"starter-skinbot-props"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){
  auto& game=s.g();
  const auto* before=game.skinbotWalker();
  LabAccess::setCamera(game,{before?before->x:6.f,21.5f},1.3f,-1.5707963f,-15.f);
  for(int tick=0;tick<19;++tick){
   game.update({},.05f);
   const auto* walker=game.skinbotWalker();
   if(!walker)continue;
   if(s.board["phase"]==0){game.pointSkinbot({6.f,16.f});s.board["phase"]=1;}
   auto& props=LabAccess::clutter(game);
   auto place=[&](int model,Vec2 at,std::array<float,3> size,float mass,float yaw,bool anchored=false){
    auto traits=std::make_shared<EntityTraits>();traits->mass=mass;traits->anchored=anchored;
    Clutter prop;prop.model=model;prop.pos=at;prop.dims=size;prop.yaw=yaw;prop.traits=traits;prop.pinned=anchored;
    prop.z=game.world().supportBelow(at.x,at.y,.2f);prop.sleeping=true;props.push_back(prop);
   };
   if(s.board["phase"]==1&&walker->arrived()){
    game.pointSkinbot({18.7f,16.f});s.board["phase"]=2;
   }
   if(s.board["phase"]==2&&walker->x>6.28f){
    s.board["barrier"]=double(props.size());
    place(model::ind_barrel,{8.f,12.f},{.65f,23.7f,1.55f},45.f,0,true);s.board["phase"]=3;
   }
   if(s.board["phase"]==3){
    if(walker->x>6.9f)s.board["barrierPenetration"]++;
    if(walker->x>6.5f)s.board["approachedBarrier"]=1;
    s.board["closedSeconds"]+=.05;
    if(s.board["closedSeconds"]>4){
     props.at(size_t(s.board["barrier"])).broken=true;
     s.board["phase"]=4;
    }
   }
   if(s.board["phase"]==4&&walker->x>7.f){
    s.board["crossedAfterRemoval"]=1;s.board["first"]=double(props.size());s.board["phase"]=5;
    place(model::mug,{9.2f,15.68f},{.22f,.22f,.20f},.6f,0);
    place(model::packing_crate,{12.1f,16.f},{1.15f,2.3f,.86f},12.f,0);
    place(model::ind_barrel,{16.f,16.f},{1.2f,1.25f,1.35f},45.f,.37f);
   }
   if(s.board["phase"]!=5)continue;
   const size_t first=size_t(s.board["first"]);
   const auto& light=props.at(first);
   const auto& step=props.at(first+1);
   auto& heavy=props.at(first+2);
   if(!s.board["moved"]&&walker->x>13.f&&walker->x<14.9f&&std::fabs(walker->y-16.f)>.4f){
    s.board["moved"]=walker->y>16.f?1:-1;
    heavy.pos={16.5f,16.f+float(s.board["moved"])*1.25f};heavy.yaw=-.55f;
   }
   const Vec2 drop=s.board["moved"]?Vec2{16.5f,16.f+float(s.board["moved"])*1.25f}:Vec2{16.f,16.f};
   s.board["push"]=std::max(s.board["push"],double(length(light.pos-Vec2{9.2f,15.68f})));
   s.board["heavyMove"]=std::max(s.board["heavyMove"],double(length(heavy.pos-drop)));
   if(std::fabs(walker->x-drop.x)<.8f)s.board["detour"]=std::max(s.board["detour"],double(std::fabs(walker->y-drop.y)));
   if(heavy.footprintContains({walker->x,walker->y}))s.board["overlap"]++;
   for(const auto& foot:walker->feet){
    if(foot.planted()&&step.footprintContains({foot.at.x,foot.at.y})&&std::fabs(foot.at.z-(step.z+step.height()))<.04f)s.board["planted"]++;
    if(heavy.footprintContains({foot.at.x,foot.at.y})&&foot.at.z<heavy.z+heavy.height()-.04f)s.board["footOverlap"]++;
   }
   s.board["east"]=std::max(s.board["east"],double(walker->x));

  }
 },
 .capture=[](Scene& s,const Frame&){
  auto capture=[&](const char* name,bool condition){if(condition&&!s.board[name]){s.shoot(name);s.board[name]=1;}};
  capture("01-blocked-lane",s.board["phase"]==3&&s.board["approachedBarrier"]!=0);
  if(s.board["phase"]!=5)return;
  const auto* walker=s.g().skinbotWalker();
  const auto& props=s.g().clutter();
  const size_t first=size_t(s.board["first"]);
  const auto& step=props.at(first+1);
  const auto& heavy=props.at(first+2);
  capture("02-small-prop-shove",s.board["push"]>.18);
  if(walker){
   bool onStep=false;
   for(const auto& foot:walker->feet)onStep|=foot.planted()&&step.footprintContains({foot.at.x,foot.at.y})
                                           &&std::fabs(foot.at.z-(step.z+step.height()))<.04f;
   capture("03-step-onto-crate",onStep);
   capture("04-detour-around-barrel",s.board["moved"]!=0&&std::fabs(walker->x-heavy.pos.x)<.6f&&std::fabs(walker->y-heavy.pos.y)>1.4f);
  }
 },
 .metrics=[](Scene& s,Report& r){
  r.check("a tiny light prop moves under physical contact",s.board["push"]>.18,std::to_string(s.board["push"]));
  r.check("the gait plants a foot on the low prop",s.board["planted"]>10,std::to_string(s.board["planted"]));
  r.check("the body and feet go around the heavy prop",s.board["overlap"]==0&&s.board["footOverlap"]==0&&s.board["detour"]>1.25&&s.board["heavyMove"]<.08,
          "body overlaps "+std::to_string(s.board["overlap"])+", foot overlaps "+std::to_string(s.board["footOverlap"])+", detour "+std::to_string(s.board["detour"]));
  r.check("it replans after the heavy prop is moved mid-detour",s.board["moved"]!=0&&s.board["east"]>18.4,std::to_string(s.board["east"]));
  r.check("it waits at a blocked lane and continues after clearance",s.board["approachedBarrier"]!=0&&s.board["barrierPenetration"]==0&&s.board["crossedAfterRemoval"]!=0,
          "blocked for "+std::to_string(s.board["closedSeconds"])+" s, approached "+std::to_string(s.board["approachedBarrier"])+", crossed after removal "+std::to_string(s.board["crossedAfterRemoval"]));
 }
});
const bool skinbotImpact=add({
 .name="starter-skinbot-impact",.summary="A thrown crate glances off Skinbot, which stumbles before charging",
 .fixtures={"starter-skinbot-impact","starter-skinbot-impact-side","starter-skinbot-impact-blocked"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){
  auto& game=s.g();
  const auto scenario=s.fixture->value("strike","front");
  for(int tick=0;tick<9;++tick){
   const auto* prior=game.skinbotWalker();
   std::array<bool,skinbot::LegCount> wasPlanted{};
   if(prior)for(size_t i=0;i<wasPlanted.size();++i)wasPlanted[i]=prior->feet[i].planted();
   game.update({},.025f);
   const auto* walker=game.skinbotWalker();
   if(!walker)continue;
   auto launch=[&](Vec2 offset,Vec2 velocity){
    Clutter c;c.model=model::packing_crate;c.dims={.42f,.42f,.42f};
    c.pos=Vec2{walker->x,walker->y}+offset;c.z=walker->z+.95f;
    c.velocity=velocity;c.vz=.3f;c.projectile=true;
    auto& props=LabAccess::clutter(game);props.push_back(c);return props.size()-1;
   };
   s.board["elapsed"]+=.025;
   if(s.board["elapsed"]<.03){game.pointSkinbot({19.f,12.f});launch({-1.95f,2.45f},{7,0});}
   if(s.board["elapsed"]<.75&&walker->catching())s.board["falseStumble"]=1;
   if(s.board["elapsed"]>=.75&&s.board["thrown"]==0){s.board["thrown"]=1;s.board["prop"]=double(scenario=="front"?launch({1.95f,0},{-7,0}):launch({0,1.95f},{0,-7}));}
   if(s.board["thrown"]==0)continue;
   if(walker->catching()){
    if(!s.board["stumble"]&&scenario=="blocked"){
     auto probe=*walker;
     const skinbot::Ground ground=[&](float x,float y,float z){return game.world().supportBelow(x,y,z);};
     // Predict which landing is chosen on clear ground, then put a real prop there before the blade moves.
     for(int frame=0;frame<24&&!s.board["obstacle"];++frame){
      const auto old=probe.feet;
      probe.update(.025f,ground);
      for(size_t i=0;i<old.size();++i)if(old[i].planted()&&!probe.feet[i].planted()){
       const auto land=probe.feet[i].to;
       auto traits=std::make_shared<EntityTraits>();traits->anchored=1;traits->mass=45;
       Clutter block;block.model=model::packing_crate;block.pos={land.x,land.y};block.z=land.z;
       block.dims={.24f,.24f,1.25f};block.traits=traits;block.pinned=true;block.sleeping=true;
       LabAccess::clutter(game).push_back(block);
       s.board["obstacle"]=1;s.board["blockedX"]=land.x;s.board["blockedY"]=land.y;
       break;
      }
     }
    }
    for(size_t i=0;i<wasPlanted.size();++i)if(wasPlanted[i]&&!walker->feet[i].planted()){
     const auto land=walker->feet[i].to;
     s.board["catch"]=1;
     if(s.board["obstacle"])s.board["avoided"]=std::hypot(land.x-s.board["blockedX"],land.y-s.board["blockedY"]);
    }
    s.board["stumble"]=1;
    s.board["sag"]=std::max(s.board["sag"],double(-walker->curse.sag));
   }
   if(s.board["stumble"]!=0&&!walker->catching()&&walker->curse.rush>.1f){
    if(s.board["recovered"]==0)s.board["braced"]=walker->airborne()==0;
    s.board["recovered"]+=.025;
    const double speed=std::hypot(walker->velocityX,walker->velocityY);
    s.board["peak"]=std::max(s.board["peak"],speed);
    if(s.board["recovered"]<=.55)s.board["launchSpeed"]=std::max(s.board["launchSpeed"],speed);
    if(speed>2.8)s.board["fastSeconds"]+=.025;
   }
  }
  if(const auto* walker=game.skinbotWalker())LabAccess::setCamera(game,{walker->x,17.f},1.3f,-1.5707963f,-10.f);
 },
 .capture=[](Scene& s,const Frame&){
  if(s.board["catch"]&&s.board["recovered"]==0&&!s.board["capturedImpact"]){s.shoot("01-catch-step");s.board["capturedImpact"]=1;}
  if(s.board["peak"]>2.8&&!s.board["capturedCharge"]){s.shoot("02-recovery-charge");s.board["capturedCharge"]=1;}
 },
 .metrics=[](Scene& s,Report& r){
  r.check("a throw past its side does not trigger a stumble",s.board["elapsed"]>.75&&s.board["falseStumble"]==0);
  r.check("the crate collision interrupts the walk and recoils the body",s.board["stumble"]&&s.board["sag"]>.04
   &&!s.g().clutter().at(size_t(s.board["prop"])).projectile,std::to_string(s.board["sag"]));
  r.check("a thrown crate makes it take a catch step",s.board["catch"]!=0);
  if(s.fixture->value("strike","front")=="blocked")
   r.check("a live prop blocks the first landing and the blade finds another",s.board["obstacle"]!=0&&s.board["avoided"]>.17,
           std::to_string(s.board["avoided"])+" m from the blocked spot");
  r.check("it plants the catch blade before committing to the charge",s.board["braced"]&&s.board["recovered"]>2.8,
          std::to_string(s.board["recovered"])+" s of charge after bracing");
  if(s.fixture->value("strike","front")=="side")
   r.check("the clear lane has a quick launch and a sustained sprint",
           s.board["peak"]>3.2&&s.board["launchSpeed"]>2.8&&s.board["fastSeconds"]>.65,
           std::to_string(s.board["peak"])+" m/s peak, "+
           std::to_string(s.board["launchSpeed"])+" m/s within 550 ms, "+
           std::to_string(s.board["fastSeconds"])+" s above 2.8 m/s");
 }
});
const bool skinbotCatch=add({
 .name="skinbot-catch",.summary="Skinbot chooses a different supported catch step for each shove",
 .standalone=+[]{
  std::ofstream report("skinbot-catch-test.txt");
  const skinbot::Ground floor=[](float,float,float){return 0.f;};
  struct Catch {int leg=-1;skinbot::W landing{};bool grounded=false;};
  auto shove=[&](Vec2 direction,const skinbot::FootClear& clear){
   skinbot::Walker walker;walker.place(0,0,0,floor,.2f);
   std::vector<SoundEvent> sounds;skinbot::reactToProp(walker,18.f,direction,sounds);
   Catch result;
   for(int tick=0;tick<24;++tick){
    walker.update(1.f/60,floor,{},nullptr,clear);
    for(int leg=0;leg<skinbot::LegCount&&result.leg<0;++leg)if(!walker.feet[size_t(leg)].planted()){
     result.leg=leg;result.landing=walker.feet[size_t(leg)].to;
    }
    if(result.leg>=0&&walker.feet[size_t(result.leg)].planted())result.grounded=true;
   }
   return result;
  };
  const auto front=shove({0,-1},{}),side=shove({1,0},{});
  const auto flank=shove({1,0},[&](skinbot::W,skinbot::W to){
   return side.leg<0||std::hypot(to.x-side.landing.x,to.y-side.landing.y)>.16f;
  });
  skinbot::Walker trapped;trapped.place(0,0,0,floor,.2f);
  std::vector<SoundEvent> sounds;skinbot::reactToProp(trapped,18.f,{1,0},sounds);
  trapped.update(1.f/60,floor);
  const bool lifted=trapped.airborne()==1;
  const skinbot::FootClear trappedFoot=[](skinbot::W,skinbot::W){return false;};
  for(int tick=0;tick<60;++tick)trapped.update(1.f/60,floor,{},nullptr,trappedFoot);
  const bool gaveUp=lifted&&!trapped.catching()&&trapped.curse.rush==0;
  const bool pass=front.leg>=0&&side.leg>=0&&front.leg!=side.leg&&front.grounded&&side.grounded&&flank.grounded&&
                  std::hypot(flank.landing.x-side.landing.x,flank.landing.y-side.landing.y)>.16f&&gaveUp;
  report<<(pass?"PASS":"FAIL")<<" catch legs "<<front.leg<<'/'<<side.leg<<'/'<<flank.leg
        <<", grounded "<<front.grounded<<'/'<<side.grounded<<'/'<<flank.grounded
        <<", canceled with a trapped blade "<<gaveUp<<'\n';
  return pass;
 }
});
const bool skinbotAudio=add({
 .name="skinbot-audio",.summary="The real mixer follows Skinbot's moving blades, motor and impacts",
 .standalone=+[]{
  auto room=regression::room();const float scale=skinbot::Walker::Scale;
  room->fixtures.push_back({skinbot::Model,{6,10},0,1.451f*scale,1.903f*scale,2.4f,1.5707963f,false});
  Game game(regression::campaign(room));
  LabAccess::setGod(game,true);LabAccess::setCamera(game,{9,14},0,-1.5707963f,0);
  AudioEngine audio(false);
  std::vector<int16_t> pcm;
  int plants=0,clicks=0,impacts=0,surges=0;bool trackedMotor=false,stoppedMotor=false;
  for(int tick=0;tick<240;++tick){
   game.update({},1.f/60);
   const auto* walker=game.skinbotWalker();
   if(!walker)continue;
   if(tick==0)game.pointSkinbot({17,10});
   if(tick==32){
    Clutter c;c.model=model::packing_crate;c.dims={.42f,.42f,.42f};
    c.pos={walker->x-1.8f,walker->y};c.z=walker->z+.95f;
    c.velocity={7.f,0};c.vz=.3f;c.projectile=true;LabAccess::clutter(game).push_back(c);
   }
   if(tick==170)game.pointSkinbot({walker->x,walker->y});
   for(const auto& event:game.sounds())if(event.spatial&&std::isfinite(event.z)){
    plants+=event.sound==Sound::SkinbotBlade;clicks+=event.sound==Sound::SkinbotJoint;
    impacts+=event.sound==Sound::SkinbotHit;surges+=event.sound==Sound::SkinbotSurge;
   }
   audio.update(game,true);LabAccess::silenceScore(audio);
   bool motor=false;
   for(const auto& voice:LabAccess::held(audio))if(voice.sound==Sound::SkinbotMotor&&voice.loop&&voice.spatial&&voice.level>0){
    motor=true;trackedMotor|=std::fabs(voice.at.x-walker->x)<.03f&&std::fabs(voice.at.y-walker->y)<.03f;
   }
   if(tick==239)stoppedMotor=!motor;
   LabAccess::mix(audio,pcm,735);
  }
  int peak=0;for(const auto sample:pcm)peak=std::max(peak,std::abs(int(sample)));
  const bool pass=peak>200&&plants>=3&&clicks>=3&&impacts==1&&surges>=1&&trackedMotor&&stoppedMotor;
  std::ofstream log("skinbot-audio-test.txt");
  log<<(pass?"PASS":"FAIL")<<" plants "<<plants<<", clicks "<<clicks<<", impacts "<<impacts
     <<", surge "<<surges<<", tracked motor "<<trackedMotor<<", idle motor stopped "<<stoppedMotor<<'\n';
  if(!pass)return false;
  std::ofstream wav("skinbot-audio-preview.wav",std::ios::binary);
  auto write=[&](auto value){wav.write(reinterpret_cast<const char*>(&value),sizeof(value));};
  wav.write("RIFF",4);write(uint32_t(pcm.size()*2+36));wav.write("WAVEfmt ",8);
  write(uint32_t(16));write(uint16_t(1));write(uint16_t(2));write(uint32_t(44100));write(uint32_t(176400));
  write(uint16_t(4));write(uint16_t(16));wav.write("data",4);write(uint32_t(pcm.size()*2));
  wav.write(reinterpret_cast<const char*>(pcm.data()),pcm.size()*2);
  return bool(wav);
 }
});
}
}
