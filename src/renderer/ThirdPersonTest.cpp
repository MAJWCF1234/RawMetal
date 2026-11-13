#include "SoftwareRenderer.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
namespace retro {
namespace {
// One take of Claire from outside: a weapon, what the player does each tick,
// and how long. Both the numeric test and the inspection frames run these.
struct Take{const char* name;int weapon;int frames;std::function<void(InputState&,int,Player&)> act;bool torch=false;};
InputState still(){return InputState{};}
std::vector<Take> takes(){
 auto hold=[](bool f,bool b,bool l,bool r,bool sprint,bool crouch,bool aim){return [=](InputState&in,int,Player&){in.forward=f;in.back=b;in.left=l;in.right=r;in.sprint=sprint;in.crouch=crouch;in.guard=aim;};};
 std::vector<Take> list;
 for(int weapon:{1,2}){
  list.push_back({"idle",weapon,90,hold(0,0,0,0,0,0,0)});
  list.push_back({"walk",weapon,60,[](InputState&in,int,Player&p){in.forward=true;p.pitch=-10;}});
  list.push_back({"run",weapon,60,hold(1,0,0,0,1,0,0)});
  list.push_back({"strafe",weapon,50,hold(0,0,0,1,0,0,0)});
  list.push_back({"back",weapon,50,hold(0,1,0,0,0,0,0)});
  list.push_back({"crouch-walk",weapon,70,hold(1,0,0,0,0,1,0)});
  list.push_back({"turn",weapon,80,[](InputState&in,int i,Player&p){(void)in;if(i==10)p.angle+=1.4f;}});
  // The shotgun is aimed with the right mouse; the pistol, which has no sights
  // to raise, comes up for a moment after a shot.
  const bool pistol=weapon==2;
  list.push_back({"aim",weapon,60,[pistol](InputState&in,int i,Player&){in.guard=true;in.fire=pistol&&i==2;}});
  list.push_back({"aim-up",weapon,60,[pistol](InputState&in,int i,Player&p){in.guard=true;in.fire=pistol&&i==2;p.pitch=70;}});
  list.push_back({"aim-down",weapon,60,[pistol](InputState&in,int i,Player&p){in.guard=true;in.fire=pistol&&i==2;p.pitch=-60;}});
  list.push_back({"fire",weapon,50,[](InputState&in,int i,Player&){in.guard=true;in.fire=i==20;}});
  list.push_back({"walk-aim",weapon,60,[pistol](InputState&in,int i,Player&){in.forward=true;in.guard=true;in.fire=pistol&&(i==2||i==40);}});
  list.push_back({"reload",weapon,150,[](InputState&in,int i,Player&){in.reload=i==5;}});
  list.push_back({"torch",weapon,40,hold(0,0,0,0,0,0,0),true});
 }
 list.push_back({"prone-crawl",1,150,[](InputState&in,int i,Player&){in.prone=i==0;in.forward=i>70;}});
 list.push_back({"jump",1,70,[](InputState&in,int i,Player&){in.jump=i==2;}});
 return list;
}
// Runs one take from the start pose with the weapon in hand, calling frame()
// after every tick's render.
// A camera at orbit metres and bearing (radians off her start facing) stays
// put in the room while she turns; orbit 0 leaves the camera as it is.
void run(Game&game,SoftwareRenderer&renderer,const Player&start,const Take&take,const std::function<void(int)>&frame,float orbit=0,float bearing=0){
 auto&player=const_cast<Player&>(game.player());player=start;
 auto&rounds=player.pool(take.weapon==2?WeaponId::Handgun:WeaponId::Shotgun);rounds.total=30;rounds.loaded=take.name==std::string("reload")?2:take.weapon==2?12:6;
 if(take.weapon==2){game.grantHandgun(36);game.equipWeapon(WeaponId::Handgun);}else game.equipWeapon(WeaponId::Shotgun);
 if(take.torch){game.giveQuestItem(Game::Flashlight);game.setState(stateId("flashlight_on"),1);}else game.setState(stateId("flashlight_on"),0);
 // The last half second drawn too, so a stance change left by the take before settles.
 for(int i=0;i<70;++i){player.pos=start.pos;player.z=start.z;player.angle=start.angle;player.pitch=0;game.update(still(),1.f/60);if(i>=40){if(orbit>0)renderer.setBodyOrbit(orbit,bearing);renderer.render(game);}}
 auto aimCamera=[&]{if(orbit>0)renderer.setBodyOrbit(orbit,wrapAngle(bearing-(player.angle-start.angle)));};
 for(int i=0;i<take.frames;++i){InputState in{};take.act(in,i,player);game.update(in,1.f/60);aimCamera();renderer.render(game);frame(i);}
 if(player.prone){InputState up{};up.prone=true;game.update(up,1.f/60);for(int i=0;i<60;++i)game.update(still(),1.f/60);}
}
}
bool SoftwareRenderer::testThirdPerson(){
 std::ofstream report("third-person-test.txt");bool passed=true;std::ofstream frameLog("third-person-frames.txt");
 auto check=[&](bool ok,const std::string&text){report<<(ok?"PASS: ":"FAIL: ")<<text<<'\n';report.flush();passed&=ok;};
 auto storage=std::make_unique<SoftwareRenderer>(480,270);auto&d=*storage;
 auto owned=std::make_unique<Game>(Game::bodyInspection());auto&game=*owned;const Player start=game.player();
 setThirdPerson(true);
 int headFaces=0;
 for(const auto&take:takes()){
  float support=0,along0=1e9f,along1=-1e9f,handGap=0,aimError=0,floorGap=1e9f,stillSlide=0,turnSlide=0;int frames=0,drawn=0,headless=0,aimed=0,pump=0;
  Point3 lastFeet[2]{};bool haveFeet=false;const std::string name=take.name;
  run(game,d,start,take,[&](int i){const auto&r=d.claireOutsideReport();++frames;
   frameLog<<take.weapon<<' '<<take.name<<' '<<i<<" lower "<<r.lower<<" layer "<<r.layer<<" blend "<<r.blending<<" aimErr "<<r.aimError<<" along "<<r.supportAlong<<" gap "<<r.supportGap<<" hand "<<r.handGap
    <<" feet "<<r.feet[0].x<<','<<r.feet[0].y<<' '<<r.feet[1].x<<','<<r.feet[1].y<<" head "<<r.headFaces<<" yaw "<<game.player().angle<<" aimBlend "<<game.aimBlend()<<'\n';if(!headFaces)for(auto h:d.m_claireHeadFace)headFaces+=h;if(!r.drawn)return;++drawn;
   if(r.headFaces*2<headFaces&&!game.player().prone)++headless;
   floorGap=std::min(floorGap,r.floorGap);
   const bool reloading=game.reloading()||name=="reload";
   if(r.weapon==1&&!reloading&&!game.player().prone&&i>12&&r.layer!=ClaireOutside::HitReaction){++pump;support=std::max(support,r.supportGap);along0=std::min(along0,r.supportAlong);along1=std::max(along1,r.supportAlong);}
   if(r.weapon==2&&r.handGap>=0&&(r.layer==ClaireOutside::PistolAim||r.layer==ClaireOutside::PistolShoot)&&i>12)handGap=std::max(handGap,r.handGap);
   if(r.aimError>=0&&i>15&&!r.blending){++aimed;aimError=std::max(aimError,r.aimError);}
   // A planted foot: standing, neither moves; turning, one stays put at a time.
   if(haveFeet&&i>12){const float a=std::hypot(r.feet[0].x-lastFeet[0].x,r.feet[0].y-lastFeet[0].y),b=std::hypot(r.feet[1].x-lastFeet[1].x,r.feet[1].y-lastFeet[1].y);
    if((name=="idle"||name=="aim")&&!r.blending)stillSlide=std::max(stillSlide,std::max(a,b));
    if(name=="turn"&&!r.blending)turnSlide=std::max(turnSlide,std::min(a,b));}
   lastFeet[0]=r.feet[0];lastFeet[1]=r.feet[1];haveFeet=true;});
  char text[512];const char* weapon=take.weapon==2?"pistol":"shotgun";
  std::snprintf(text,sizeof text,"%s %s: drawn %d/%d frames, head shown in all but %d",weapon,take.name,drawn,frames,headless);check(drawn==frames&&headless==0,text);
  std::snprintf(text,sizeof text,"%s %s: lowest vertex %.3f m over the floor (no deeper than 1 cm into it)",weapon,take.name,floorGap);check(floorGap>-.01f,text);
  if(pump){std::snprintf(text,sizeof text,"%s %s: left hand within %.3f m of the pump's line (0.05 allowed), along z %.2f to %.2f (wood -3.0 to -1.5)",weapon,take.name,support,along0,along1);
   check(support<.05f&&along0>-3.1f&&along1<-1.4f,text);}
  if(take.weapon==2&&handGap>0){std::snprintf(text,sizeof text,"%s %s: support hand within %.3f m of the pistol grip (0.08 allowed)",weapon,take.name,handGap);check(handGap<.08f,text);}
  if(aimed){std::snprintf(text,sizeof text,"%s %s: the gun points within %.1f degrees of the aim (5 allowed)",weapon,take.name,aimError);check(aimError<5.f,text);}
  if(name=="idle"||name=="aim"){std::snprintf(text,sizeof text,"%s %s: feet slide at most %.3f m a frame standing (0.01 allowed)",weapon,take.name,stillSlide);check(stillSlide<.01f,text);}
  if(name=="turn"){std::snprintf(text,sizeof text,"%s turn: one foot stays planted while the other steps (the steadier moved at most %.3f m a frame, 0.012 allowed)",weapon,turnSlide);check(turnSlide<.012f,text);}
  if(name=="aim"||name=="fire"){std::snprintf(text,sizeof text,"%s %s: the gun was aimed in %d frames",weapon,take.name,aimed);check(aimed>0,text);}
 }
 setThirdPerson(false);
 report<<(passed?"PASSED\n":"FAILED\n");return passed;
}
int SoftwareRenderer::thirdPersonInspection(){
 // Frames of Claire from outside for checking by eye: a turnaround of each
 // hold (eight bearings), then each take filmed at 30 fps from the front,
 // side and back. tp-frames/NAME-VIEW-NNN.ppm and tp-frames/index.txt.
 std::filesystem::create_directories("tp-frames");std::ofstream index("tp-frames/index.txt");
 try{
 auto storage=std::make_unique<SoftwareRenderer>(640,480);auto&d=*storage;if(!d.enableHardware())index<<"software renderer\n";
 auto owned=std::make_unique<Game>(Game::bodyInspection());auto&game=*owned;const Player start=game.player();setTiltShift(false);
 d.setEnvironmentInspection(true);   // clean frames: no HUD
 auto save=[&](const std::string&name){const std::string file="tp-frames/"+name+".ppm";std::ofstream out(file,std::ios::binary);out<<"P6\n640 480\n255\n";
  for(int k=0;k<640*480;++k){const auto px=d.pixels()[k];const char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}index<<file<<'\n';};
 const float views[]={0.f,kPi*.5f,kPi};const char* viewNames[]={"front","side","back"};
 for(const auto&take:takes()){const char* weapon=take.weapon==2?"pistol":"shotgun";
  if(std::string(take.name)=="idle"||std::string(take.name)=="aim")
   for(int b=0;b<8;++b)run(game,d,start,take,[&](int i){if(i==take.frames-1){char n[96];std::snprintf(n,sizeof n,"turn-%s-%s-%d",weapon,take.name,b);save(n);}},2.4f,b*kPi/4);
  for(int v=0;v<3;++v)
   run(game,d,start,take,[&](int i){if(i%2==0){char n[128];std::snprintf(n,sizeof n,"strip-%s-%s-%s-%03d",weapon,take.name,viewNames[v],i/2);save(n);}},2.4f,views[v]);}
 // The over-the-shoulder camera as played.
 d.setBodyOrbit(0,0);setThirdPerson(true);d.setEnvironmentInspection(false);
 for(const auto&take:takes()){const char* weapon=take.weapon==2?"pistol":"shotgun";
  run(game,d,start,take,[&](int i){if(i==take.frames/2){char n[96];std::snprintf(n,sizeof n,"shoulder-%s-%s",weapon,take.name);save(n);}});}
 setThirdPerson(false);return d.hardwareActive()?0:36;
 }catch(const std::exception&e){index<<"threw "<<e.what()<<'\n';return 21;}
}
}
