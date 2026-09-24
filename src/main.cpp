#ifdef _WIN32
#include "game/Game.h"
#include "renderer/SoftwareRenderer.h"
#include "platform/Win32Window.h"
#include "audio/AudioEngine.h"
#include <chrono>
#include <cwchar>
#include <fstream>
#include <queue>
#include <exception>
#include <filesystem>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR commandLine,int){
    try {
    constexpr int W=retro::DisplayWidth,H=retro::DisplayHeight;
    if(std::wcsstr(commandLine,L"--world-isolation-test")||std::wcsstr(commandLine,L"--ashfall-inspection")){
     if(std::wcsstr(commandLine,L"--world-isolation-test")&&!retro::Game::testWorldIsolation())return 44;
     retro::SoftwareRenderer renderer(W,H);
     if(std::wcsstr(commandLine,L"--vulkan")&&!renderer.enableHardware())return 36;
     for(int level=0;level<retro::worldChunkCount(retro::WorldId::Ashfall);++level){
      auto spawn=retro::chunkDefinition(retro::WorldId::Ashfall,level).playerStart;
      auto scene=retro::Game::mapInspection(spawn,.6f,0,level,false,-999,true,retro::WorldId::Ashfall);renderer.render(scene);
      std::ofstream out("isolated-world-"+std::to_string(level)+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }
     return 0;
    }
    if(std::wcsstr(commandLine,L"--water-wall-inspection")){
     retro::SoftwareRenderer renderer(W,H);if(!std::wcsstr(commandLine,L"--software")&&!renderer.enableHardware())return 36;
     for(int view=0;view<10;++view){auto scene=view<8?retro::Game::mapInspection({8,6},view*retro::kPi*.25f,0,3,false,0,true):retro::Game::mapInspection({9,7.8f},retro::kPi*.5f,-55,5,false,-9,true);
      if(view==9)scene.update({},.05f);renderer.render(scene);
      std::ofstream out("water-wall-"+std::to_string(view)+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(std::wcsstr(commandLine,L"--flashlight-test")){
     if(!retro::Game::testFlashlight())return 43;
     retro::SoftwareRenderer renderer(W,H);
     if(!std::wcsstr(commandLine,L"--software")&&!renderer.enableHardware())return 36;
     auto scene=retro::Game::mapInspection({6.5f,1.5f},1.4f,0,4,false,-9,true);
     std::vector<std::uint32_t> before;std::ofstream report("flashlight-test.txt");
     for(int on=0;on<2;++on){if(on){scene.giveQuestItem(retro::Game::Flashlight);retro::InputState key{};key.flashlight=true;scene.update(key,.01f);}
      for(int warm=0;warm<8;++warm)renderer.render(scene);
      auto start=std::chrono::steady_clock::now();for(int frame=0;frame<20;++frame)renderer.render(scene);
      report<<"light "<<on<<" ms/frame "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20<<'\n';
      std::ofstream out(on?"flashlight-on.ppm":"flashlight-off.ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
      if(!on)before.assign(renderer.pixels(),renderer.pixels()+W*H);
     }
     int brighter=0;for(int y=60;y<H-60;++y)for(int x=80;x<W-80;++x){int i=y*W+x;auto a=before[i],b=renderer.pixels()[i];if(int(b&255)+int((b>>8)&255)+int((b>>16)&255)>int(a&255)+int((a>>8)&255)+int((a>>16)&255)+12)++brighter;}
     report<<"Brighter scene pixels: "<<brighter<<'\n';return brighter>100?0:43;
    }
    if(std::wcsstr(commandLine,L"--campaign-extension-test"))return retro::Game::testCampaignExtension()?0:46;
    if(std::wcsstr(commandLine,L"--campaign-inspection")){
     retro::SoftwareRenderer renderer(W,H);if(!renderer.enableHardware())return 36;
     struct View{const char* name;int level;retro::Vec2 p;float z,yaw,pitch;};
     for(auto view:std::array<View,9>{{
      {"cable-entry",6,{3.5f,4},-9,.5f,0},{"cable-trench",6,{13,7.5f},-9,retro::kPi*.5f,-10},
      {"cable-breaker",6,{19,11},-9,0,0},{"annex-entry",7,{3.5f,3},-9,.55f,8},
      {"annex-lower",7,{12,3},-12,1.5f,15},{"annex-upper",7,{20,18},-4,-2.5f,-30},
      {"junction-bridge",8,{6,8},-4,.5f,-20},{"waste-deck",9,{3,6},-9,.4f,-20},
      {"waste-press",9,{11,6},-12,.8f,6}
     }}){
      auto scene=retro::Game::mapInspection(view.p,view.yaw,view.pitch,view.level,false,view.z,true);renderer.render(scene);
      std::ofstream frame(std::string(view.name)+".ppm",std::ios::binary);frame<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
     }return 0;
    }
    if(std::wcsstr(commandLine,L"--megamap-inspection")){
     retro::SoftwareRenderer renderer(W,H);if(!std::wcsstr(commandLine,L"--software")&&!renderer.enableHardware())return 36;
     std::ofstream report("megamap-inspection.txt");bool ok=true;
     for(int level:{4,5}){retro::World w(level);for(auto&s:w.structures()){bool valid=s.bottom>=-9.01f&&s.top<=-4.79f;ok&=valid;report<<"map "<<level<<" structure "<<s.x1<<','<<s.y1<<" z "<<s.bottom<<".."<<s.top<<" in room "<<valid<<'\n';}
      bool blocks=level==4?!w.fits(8.9f,6,-9,1):!w.fits(17.35f,22,-9,1);ok&=blocks;report<<"collision present "<<blocks<<'\n';}
     // Exercise the real standing hull, step height, door swing and water bed.
     // A fixed -9 flood-fill incorrectly rejects raised decks and ramps.
     ok&=retro::Game::testServiceMaps();
     struct View{std::string name;int level;retro::Vec2 p;float yaw,pitch;};
     std::vector<View> views={{"gallery-entry",4,{6.5f,1.5f},1.4f,0},{"gallery-bays",4,{11.f,12.f},2.65f,-8},{"gallery-seam",4,{12.f,22.f},retro::kPi*.5f,0},{"coolant-seam",5,{12.f,2.f},-retro::kPi*.5f,0},{"coolant-pools",5,{12.f,6.5f},retro::kPi*.5f,-14},{"coolant-equipment",5,{18.f,12.f},.9f,-6},{"coolant-bulkhead",5,{19.5f,21.3f},retro::kPi*.5f,0}};
     for(int level:{4,5})for(int angle=0;angle<8;++angle)views.push_back({std::string(level==4?"gallery":"coolant")+"-sweep-"+std::to_string(angle),level,{12,12},angle*retro::kPi*.25f,-8});
     views.push_back({"gallery-workbay",4,{6.5f,9.5f},-2.1f,-6});
     views.push_back({"coolant-riser",5,{19.f,20.f},-.8f,0});
     for(auto v:views){
      auto scene=retro::Game::mapInspection(v.p,v.yaw,v.pitch,v.level,false,-9,true);renderer.render(scene);
      auto start=std::chrono::steady_clock::now();for(int frame=0;frame<12;++frame)renderer.render(scene);
      report<<v.name<<" warm render ms/frame "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/12<<'\n';
      std::ofstream out(v.name+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }
     return ok&&retro::Game::testStreaming()?0:42;
    }
    if(std::wcsstr(commandLine,L"--hazmat-test")){
     bool passed=retro::Game::testHazmat();retro::SoftwareRenderer renderer(W,H);renderer.enableHardware();
     for(int view=0;view<3;++view){auto scene=retro::Game::hazmatInspection(view);renderer.render(scene);std::ofstream out("hazmat-"+std::to_string(view)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return passed?0:41;
    }
    if(std::wcsstr(commandLine,L"--stalker-test")){
     if(!retro::SoftwareRenderer::testCreatureAnimation()||!retro::Game::testAI())return 40;
     retro::SoftwareRenderer renderer(W,H);if(!renderer.enableHardware())return 36;
     for(int view=0;view<2;++view)for(int clip=0;clip<5;++clip)for(int frame=0;frame<5;++frame){auto scene=retro::Game::stalkerInspection(clip,.01f+frame*.245f,view);renderer.render(scene);
      std::ofstream out(std::string(view?"stalker-side-":"stalker-")+std::to_string(clip)+"-"+std::to_string(frame)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(std::wcsstr(commandLine,L"--physics-ai-test"))return !retro::Game::testMovement()?19:!retro::Game::testAI()?21:!retro::Game::testClutter()?22:0;
    if(std::wcsstr(commandLine,L"--warden-inspection")){
        retro::SoftwareRenderer renderer(W,H);renderer.enableHardware();
        for(int view=0;view<2;++view){
         auto scene=view==0?retro::Game::validationScene(retro::Enemy::Kind::Warden,-1,.9f):retro::Game::mapInspection({19.2f,18.5f},0,0,3,true,-9,false);renderer.render(scene);
         std::ofstream out(view==0?"warden.ppm":"warden-reactor.ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
         for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        }return 0;
    }
    if(std::wcsstr(commandLine,L"--lift-test"))return retro::Game::testLift()?0:32;
    if(std::wcsstr(commandLine,L"--reactor-test"))return retro::Game::testReactor()?0:37;
    if(std::wcsstr(commandLine,L"--save-test"))return retro::Game::testSaves()?0:38;
    if(std::wcsstr(commandLine,L"--shaft-inspection")){
        retro::SoftwareRenderer renderer(W,H);if(!std::wcsstr(commandLine,L"--software"))renderer.enableHardware();int index=0;
        for(float seconds:{0.f,9.f,17.f,25.f,29.f,35.f,38.7f,39.6f,42.f,48.f}){auto scene=retro::Game::liftInspection(seconds,8);renderer.render(scene);std::ofstream out("shaft-"+std::to_string(index++)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
        return 0;
    }
    if(std::wcsstr(commandLine,L"--plant-inspection")){
        retro::SoftwareRenderer renderer(W,H);if(!std::wcsstr(commandLine,L"--software"))renderer.enableHardware();
        auto save=[&](const char* name,const retro::Game& scene){renderer.render(scene);std::ofstream out(name,std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}};
        save("plant-feed.ppm",retro::Game::mapInspection({6.3f,16.95f},retro::kPi*.5f,5,3,false,-9,true));
        save("plant-return.ppm",retro::Game::liftInspection(48,7));
        save("plant-arrival.ppm",retro::Game::mapInspection({10.2f,6.8f},1.2f,0,3,false,0,true));
        save("plant-freight.ppm",retro::Game::mapInspection({3.1f,8.15f},0,-10,3,false,0,true));
        save("plant-freight-low.ppm",retro::Game::mapInspection({3.4f,7.1f},.55f,-35,3,false,0,true));
        save("plant-winch.ppm",retro::Game::mapInspection({15.3f,9.4f},.7f,0,3,false,0,true));
        save("plant-cab.ppm",retro::Game::liftInspection(0,8));return 0;
    }
    if(std::wcsstr(commandLine,L"--repair-inspection")){
        retro::SoftwareRenderer renderer(W,H);if(!std::wcsstr(commandLine,L"--software"))renderer.enableHardware();
        auto save=[&](const char* name,const retro::Game& scene){renderer.render(scene);std::ofstream out(name,std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}};
        save("repair-wall.ppm",retro::Game::mapInspection({17,20},0,0,3,false,-9,true));
        save("repair-stairs.ppm",retro::Game::mapInspection({21.8f,11.5f},2.04f,15,3,false,-9,true));
        save("repair-stairs-top.ppm",retro::Game::mapInspection({21.8f,18.5f},-2.1f,-35,3,false,-6,true));
        save("repair-sign.ppm",retro::Game::liftInspection(0,3));
        auto menu=retro::Game::liftInspection(retro::World::LiftRideComplete);menu.setSaveDirectory(L"inspection-empty-saves");retro::InputState input{};input.escape=true;menu.update(input,.01f);save("repair-pause.ppm",menu);
        auto click=[&](int row){menu.update({},.01f);retro::InputState i{};i.fire=true;i.pointerX=retro::MenuLayout::X+30;i.pointerY=retro::MenuLayout::RowTop+row*retro::MenuLayout::RowHeight+7;menu.update(i,.01f);};
        click(7);save("repair-save.ppm",menu);click(3);click(8);save("repair-load.ppm",menu);click(0);save("repair-confirm.ppm",menu);return 0;
    }
    if(std::wcsstr(commandLine,L"--console-test"))return retro::Game::testConsole()?0:34;
    if(std::wcsstr(commandLine,L"--controls-test"))return retro::Game::testCombat()&&retro::Game::testSettings()?0:39;
    if(std::wcsstr(commandLine,L"--performance-test"))return retro::SoftwareRenderer::testPerformance()?0:35;
    if(std::wcsstr(commandLine,L"--vulkan-test"))return retro::SoftwareRenderer::testHardware()?0:36;
    if(std::wcsstr(commandLine,L"--lift-audio-test"))return retro::AudioEngine::testLiftMix()?0:33;
    if(std::wcsstr(commandLine,L"--lift-inspection")){
        retro::SoftwareRenderer renderer(W,H);if(!std::wcsstr(commandLine,L"--software"))renderer.enableHardware();int index=0;
        for(float seconds:{0.f,7.f,21.f,34.f,48.f,48.f,48.f,48.f,48.f,48.f,48.f}){auto scene=retro::Game::liftInspection(seconds,index>=7?index-3:index>=5?index-4:index==1?3:0);renderer.render(scene);
            std::ofstream out("lift-"+std::to_string(index++)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
            for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
        return 0;
    }
    if(std::wcsstr(commandLine,L"--audio-device-test"))return retro::AudioEngine::testDevice()?0:15;
    if(std::wcsstr(commandLine,L"--environment-inspection")){
        retro::SoftwareRenderer renderer(W,H);
        auto save=[&](const std::string&name){std::ofstream out(name+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}};
        const retro::Vec2 centers[]={{16.f,2.5f},{4.f,10.5f},{11.5f,11.f},{11.f,2.5f}};
        for(int target=0;target<4;++target)for(int side=0;side<4;++side){float a=side*retro::kPi*.5f+.3f;auto center=centers[target];auto position=center+retro::Vec2{std::cos(a),std::sin(a)}*2.8f;
            auto scene=retro::Game::mapInspection(position,a+retro::kPi,-30,target<2?0:target-1,true,0,true);renderer.render(scene);save("machine-"+std::to_string(target)+"-"+std::to_string(side));}
        for(int side=0;side<2;++side){auto scene=retro::Game::mapInspection({4.5f,side?10.5f:6.5f},side?-retro::kPi*.5f:retro::kPi*.5f,0,0,false,0,true);renderer.render(scene);save("square-door-"+std::to_string(side));}
        retro::World world;bool inspected[4]{};const retro::Vec2 normals[]={{-1,0},{1,0},{0,-1},{0,1}};
        for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(world.tile(x,y)=='#'&&(x*3+y)%9==0)for(int side=0;side<4;++side){auto n=normals[side];if(inspected[side]||world.tile(x+int(n.x),y+int(n.y))=='#')continue;
            auto face=retro::Vec2{x+.5f,y+.5f}+n*.5f;if(!world.wallSpaceFree(face,{-n.y,n.x},.68f,.65f,1.33f))continue;auto position=face+n*1.4f+retro::Vec2{-n.y,n.x}*.25f;if(!world.fits(position.x,position.y,0,1))continue;
            auto delta=face-position;auto scene=retro::Game::mapInspection(position,std::atan2(delta.y,delta.x),20,0,false,0,true);renderer.render(scene);save("wall-vent-"+std::to_string(side));inspected[side]=true;}
        for(int level=0;level<3;++level){retro::World map(level);int count=0;for(auto&light:map.lights()){
            auto position=light.position+retro::Vec2{0,1.4f};float feet=map.floorHeight(position.x,position.y);if(!map.fits(position.x,position.y,feet,1))continue;
            auto scene=retro::Game::mapInspection(position,-retro::kPi*.5f,std::atan2(light.z-feet-.78f,1.4f)*140,level,false,feet,true);renderer.render(scene);save("ceiling-light-"+std::to_string(level)+"-"+std::to_string(count));if(++count==2)break;}}
        return 0;
    }
    if(std::wcsstr(commandLine,L"--service-inspection")){
        retro::SoftwareRenderer renderer(W,H);
        if(std::wcsstr(commandLine,L"--vulkan")&&!renderer.enableHardware())return 36;
        struct View{int level;retro::Vec2 position;float angle;bool openDoors=false;};
        const View views[]={
            {4,{6.5f,2.5f},1.0f},{4,{12.f,7.f},1.57f},{4,{12.f,16.f},0.f},
            {5,{12.f,2.5f},1.57f},{5,{11.5f,9.f},0.f},{5,{12.f,15.f},0.f},
            {5,{19.75f,19.2f},1.57f},{5,{19.75f,19.2f},1.57f,true}};
        for(int i=0;i<int(std::size(views));++i){const auto&v=views[i];
            auto scene=retro::Game::mapInspection(v.position,v.angle,0,v.level,v.openDoors,-9,true);
            renderer.render(scene);std::ofstream out("service-"+std::to_string(i)+".ppm",std::ios::binary);
            out<<"P6\n"<<W<<" "<<H<<"\n255\n";
            for(int p=0;p<W*H;++p){auto color=renderer.pixels()[p];char rgb[]={char(color>>16),char(color>>8),char(color)};out.write(rgb,3);}
        }
        return 0;
    }
    if(std::wcsstr(commandLine,L"--service-map-test"))return retro::Game::testServiceMaps()?0:45;
    if(std::wcsstr(commandLine,L"--render-benchmark")){
        retro::SoftwareRenderer renderer(W,H);std::ofstream report("render-benchmark.txt");
        const retro::Vec2 positions[]={{3.5f,4.5f},{7.5f,12.5f},{20.5f,11.5f}};
        for(auto position:positions){auto scene=retro::Game::mapInspection(position,0);renderer.render(scene);auto start=std::chrono::steady_clock::now();
            for(int i=0;i<30;++i)renderer.render(scene);
            auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30;
            report<<position.x<<", "<<position.y<<": "<<ms<<" ms/frame (render only)\n";
        }return 0;
    }
    if(std::wcsstr(commandLine,L"--smoke-test")){
        retro::Game game;
        retro::SoftwareRenderer renderer(W,H);
        if(std::wcsstr(commandLine,L"--vulkan")&&!renderer.enableHardware())return 36;
        std::ofstream("model-report.txt")<<renderer.modelReport();
        if(!renderer.validate3D())return 7;
        if(!retro::Game::testHazmat())return 41;
        if(!retro::SoftwareRenderer::testCreatureAnimation())return 40;
        if(!retro::Game::testCombat())return 9;
        if(!retro::Game::testWeaponMotion())return 11;
        if(!retro::Game::testAudioEvents())return 12;
        if(!retro::Game::testSettings())return 16;
        if(!retro::Game::testInventory())return 31;
        if(!retro::Game::testFlashlight())return 43;
        if(!retro::Game::testPickups())return 18;
        if(!retro::Game::testMovement())return 19;
        if(!retro::Game::testProgression())return 20;
        if(!retro::Game::testAI())return 21;
        if(!retro::Game::testGantry())return 24;
        if(!retro::Game::testLift())return 32;
        if(!retro::Game::testReactor())return 37;
        if(!retro::Game::testSaves())return 38;
        if(!retro::Game::testSystems())return 42;
        if(!retro::Game::testConsole())return 34;
        if(!retro::AudioEngine::testLiftMix())return 33;
        if(!retro::Game::testClutter())return 25;
        if(!retro::Game::testServiceMaps())return 45;
        if(!retro::Game::testStreaming())return 26;
        if(!retro::Game::testUnarmed())return 22;
        {auto wide=retro::Win32Window::viewport(1280,600),normal=retro::Win32Window::viewport(960,540);
         if(wide.left!=320||wide.top!=120||wide.right-wide.left!=640||wide.bottom-wide.top!=360||normal.left!=160||normal.top!=90||normal.right!=800||normal.bottom!=450)return 17;
        }
        if(!retro::AudioEngine::test())return 13;
        for(int model=0;model<27;++model){
            renderer.previewModel(model,1.1f);
            std::ofstream preview("model-"+std::to_string(model)+".ppm",std::ios::binary);preview<<"P6\n"<<W<<" "<<H<<"\n255\n";
            for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};preview.write(rgb,3);}
        }
        {
            retro::Game animation=retro::Game::validationScene(retro::Enemy::Kind::Huntsman,3);
            float maxError=0;
            for(int frame=0;frame<150;++frame){retro::InputState motion{};
                motion.mouseDx=frame<80?std::sin(frame*.17f)*80.f:0;
                motion.mouseDy=frame<80?std::cos(frame*.13f)*40.f:0;
                motion.fire=frame==35;motion.jump=frame==65;
                animation.update(motion,1.f/60.f);renderer.render(animation);maxError=std::max(maxError,renderer.gripError());
                if(frame==36||frame==48||frame==80){std::ofstream shot("weapon-motion-"+std::to_string(frame)+".ppm",std::ios::binary);shot<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};shot.write(rgb,3);}}
            }
            std::ofstream("attachment-test.txt")<<"150 frames: recoil, sway, jump and settle\nMaximum wrist-to-socket error: "<<maxError<<" metres\n";
            if(maxError>.025f)return 10;
        }
        bool reachable[24][24]{};
        std::queue<std::pair<int,int>> pending;
        pending.push({3,4});reachable[4][3]=true;
        while(!pending.empty()){
            auto [x,y]=pending.front();pending.pop();
            const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
            for(int d=0;d<4;++d){int nx=x+dx[d],ny=y+dy[d];
                if(nx>=0&&ny>=0&&nx<24&&ny<24&&!reachable[ny][nx]&&!game.world().solid(nx+.5f,ny+.5f)){
                    reachable[ny][nx]=true;pending.push({nx,ny});
                }
            }
        }
        for(const auto& e:game.enemies())if(!reachable[int(e.pos.y)][int(e.pos.x)])return 3;
        for(const auto& p:game.pickups())if(!reachable[int(p.pos.y)][int(p.pos.x)])return 4;
        if(!reachable[22][21])return 5;
        int walkable=0;for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(!game.world().solid(x+.5f,y+.5f)){if(!reachable[y][x])return 14;++walkable;}
        std::ofstream("map-test.txt")<<"All "<<walkable<<" walkable tiles connected; every enemy, pickup and extraction reachable.\n";
        retro::InputState input{};
        for(int frame=0;frame<120;++frame)game.update(input,1.f/60.f);
        renderer.render(game);
        std::ofstream out("smoke-frame.ppm",std::ios::binary);
        out<<"P6\n"<<W<<" "<<H<<"\n255\n";
        for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        auto saveFrame=[&](const char* name){std::ofstream frame(name,std::ios::binary);frame<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}};
        {auto impact=retro::Game::mapInspection({3.5f,6.5f},retro::kPi,0,0,false,0,true);renderer.render(impact);saveFrame("impact-before.ppm");retro::InputState fire{};fire.fire=true;impact.update(fire,.02f);if(impact.bulletImpacts().empty())return 47;for(auto&mark:impact.bulletImpacts())if(retro::length(mark.pos-impact.player().pos)<.3f)return 47;for(int i=0;i<24;++i)impact.update({},1.f/60.f);renderer.render(impact);saveFrame("impact-inspection.ppm");}
        {auto basin=retro::Game::mapInspection({8.5f,9.f},0,0,5,false,-9.4f,true);retro::InputState duck{};duck.crouch=true;for(int i=0;i<30;++i)basin.update(duck,1.f/60.f);renderer.render(basin);saveFrame("underwater-inspection.ppm");}
        {auto title=game;title.showTitleScreen();renderer.render(title);saveFrame("title-menu.ppm");}
        {auto inventory=game;retro::InputState open{};open.inventory=true;inventory.update(open,.01f);renderer.render(inventory);saveFrame("inventory-menu.ppm");}
        {auto console=game;retro::InputState consoleInput{};consoleInput.console=true;console.update(consoleInput,.01f);consoleInput={};consoleInput.textInput="maps\rmap reactor\r";console.update(consoleInput,.01f);renderer.render(console);saveFrame("developer-console.ppm");}
        for(int kind=0;kind<6;++kind)for(int stage=0;stage<3;++stage){auto scene=retro::Game::clutterInspection(kind,stage==0?0:stage==1?.3f:5.f);renderer.render(scene);saveFrame(("clutter-tumble-"+std::to_string(kind)+"-"+std::to_string(stage)+".ppm").c_str());}
        {const retro::Vec2 positions[]={{7.5f,4.5f},{4.8f,17.4f},{6.5f,20.5f},{6.5f,3.5f},{16.5f,11.5f},{17.5f,9.5f},{21.5f,20.5f}};
           const float yaw[]={2.3f,1.570796f,-.7f,.2f,-1.5f,0,1.570796f},pitch[]={50,45,-45,-45,-30,-90,15};
         for(int i=0;i<7;++i){auto scene=retro::Game::mapInspection(positions[i],yaw[i],pitch[i],2,true,i>=2&&i<=5?3.f:0.f);renderer.render(scene);saveFrame(("gantry-view-"+std::to_string(i)+".ppm").c_str());}
          auto lift=retro::Game::mapInspection({11.3f,3.5f},1.570796f);retro::InputState use{};use.use=true;lift.update(use,.01f);renderer.render(lift);saveFrame("clutter-held.ppm");
        }
        {std::ofstream placement("placement-test.txt");
         for(int level=0;level<2;++level){retro::World world(level);
          for(auto& prop:world.props()){
           float base=world.floorHeight(prop.position.x,prop.position.y);
           for(float dx:{-prop.halfSize.x,0.f,prop.halfSize.x})for(float dy:{-prop.halfSize.y,0.f,prop.halfSize.y}){
            float x=prop.position.x+dx,y=prop.position.y+dy;
            if(std::fabs(world.floorHeight(x,y)-base)>.005f||world.tile(int(x),int(y))=='#'){placement<<"Unsupported prop "<<prop.kind<<" in chunk "<<level;return 23;}
           }
          }
         }placement<<"All machinery footprints rest on continuous floor; no wall intersections: PASS\n";
         const retro::Vec2 positions[]={{20.5f,20.f},{22.5f,21.7f},{3.5f,1.5f},{19.5f,8.2f},{18.5f,10.6f},{22.f,12.f},{8.5f,12.5f}};
         const float yaw[]={1.25f,2.24f,-1.570796f,1.3f,0,-2.53f,3.14159f};
         const float pitch[]={65,115,75,40,10,-15,-35};
         for(int i=0;i<7;++i){auto scene=retro::Game::mapInspection(positions[i],yaw[i],pitch[i],i<2?0:1,true);renderer.render(scene);saveFrame(("placement-"+std::to_string(i)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{3.5f,1.5f},{9.f,9.f},{16.5f,13.f},{20.5f,12.f},{15.f,20.5f}};
         const float angles[]={.7f,.7f,-.3f,2.8f,.15f};
         for(int i=0;i<5;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i],-12,1);renderer.render(scene);saveFrame(("pressureworks-"+std::to_string(i)+".ppm").c_str());}
         auto outward=retro::Game::mapInspection({21.5f,22.8f},1.570796f,0,0,true);renderer.render(outward);saveFrame("chunk-seam-out.ppm");
         auto returning=retro::Game::mapInspection({3.5f,1.3f},-1.570796f,0,1,true);renderer.render(returning);saveFrame("chunk-seam-back.ppm");
        }
        {auto menuGame=game;retro::InputState menuInput{};menuInput.escape=true;menuGame.update(menuInput,.02f);renderer.render(menuGame);saveFrame("settings-menu.ppm");
         retro::InputState drag{};drag.fire=true;drag.pointerX=retro::MenuLayout::SliderX+15;drag.pointerY=retro::MenuLayout::RowTop+retro::MenuLayout::RowHeight+9;menuGame.update(drag,.02f);renderer.render(menuGame);saveFrame("slider-low.ppm");
         drag.pointerX=retro::MenuLayout::SliderX+60;drag.pointerY+=50;menuGame.update(drag,.02f);renderer.render(menuGame);saveFrame("slider-high.ppm");
        }
        {for(int mode:{1,2,3})for(int stage=0;stage<4;++stage){auto pose=retro::Game::weaponInspection(mode,mode==2?stage*.028f:stage==0?10.f:stage*.12f);renderer.render(pose);saveFrame(("weapon-pass-"+std::to_string(mode)+"-"+std::to_string(stage)+".ppm").c_str());}
         auto fists=retro::Game::weaponInspection(1,.16f);for(int angle=0;angle<4;++angle){renderer.inspectRig(fists,angle*1.570796f,.25f);saveFrame(("fist-rig-"+std::to_string(angle)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{2.7f,6.8f},{2.7f,10.2f},{19.f,7.f},{18.7f,10.5f},{11.f,15.f},{9.7f,17.5f}};const float yaw[]={1.55f,-1.55f,1.35f,-1.1f,1.6f,-.75f};
         for(int i=0;i<6;++i){auto scene=retro::Game::mapInspection(positions[i],yaw[i],18);renderer.render(scene);saveFrame(("door-inspection-"+std::to_string(i)+".ppm").c_str());}
        }
        {auto health=retro::Game::mapInspection({12.f,5.5f},0,-40);renderer.render(health);saveFrame("pickup-health.ppm");
         auto ammo=retro::Game::mapInspection({4.5f,6.2f},1.570796f,-45);renderer.render(ammo);saveFrame("pickup-ammo.ppm");}
        {const retro::Vec2 positions[]={{4.5f,5.3f},{5.8f,5.4f},{4.5f,1.5f},{7.5f,6.2f},{4.5f,7.2f},{21.5f,21.f}};
         const float angles[]={1.570796f,2.062f,1.570796f,1.570796f,1.570796f,1.570796f},pitch[]={50,45,20,28,-75,35};
         for(int i=0;i<6;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i],pitch[i]);renderer.render(scene);saveFrame(("signage-"+std::to_string(i)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{3.5f,4.5f},{7.5f,12.5f},{20.5f,17.5f},{4.5f,6.5f}};const float angles[]={0,0,1.570796f,1.570796f};
         for(int i=0;i<4;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i]);renderer.render(scene);saveFrame(("map-"+std::to_string(i)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{20.5f,15.7f},{20.5f,11.5f},{9.3f,14.5f},{13.5f,14.5f},{4.5f,7.f}};
         const float angles[]={-1.570796f,1.570796f,0,-1.570796f,1.570796f},pitch[]={0,-20,0,35,20};
         for(int i=0;i<5;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i],pitch[i]);renderer.render(scene);saveFrame(("client-map-"+std::to_string(i)+".ppm").c_str());
          if(i==4){retro::InputState use{};use.use=true;scene.update(use,.02f);use.use=false;for(int frame=0;frame<150;++frame)scene.update(use,1.f/120.f);renderer.render(scene);saveFrame("bulkhead-open.ppm");}
         }
         auto log=retro::Game::mapInspection({2.2f,4.2f},1.570796f);retro::InputState use{};use.use=true;log.update(use,.02f);renderer.render(log);saveFrame("shift-log.ppm");
        }
        {auto rigScene=retro::Game::validationScene(retro::Enemy::Kind::Huntsman,3);
         for(int pose=0;pose<2;++pose){if(pose){retro::InputState trigger{};trigger.fire=true;rigScene.update(trigger,1.f/60.f);}
          const float yaw[]={0,1.570796f,-1.570796f,0,3.14159f,.7f},pitch[]={0,0,0,1.20f,0,.5f};
          for(int angle=0;angle<6;++angle){renderer.inspectRig(rigScene,yaw[angle],pitch[angle]);saveFrame(("rig-"+std::to_string(pose)+"-"+std::to_string(angle)+".ppm").c_str());}
         }
        }
        for(int kind=0;kind<3;++kind)for(int state=0;state<4;++state){auto scene=retro::Game::validationScene(static_cast<retro::Enemy::Kind>(kind),state==2?.35f:state==3?2.5f:-1.f,state==1?.4f:0);
            renderer.render(scene);saveFrame(("encounter-"+std::to_string(kind)+"-"+std::to_string(state)+".ppm").c_str());
        }
        input.fire=true;game.update(input,1.f/60.f);renderer.render(game);saveFrame("firing-frame.ppm");
        input.fire=false;input.mouseDy=-90;input.jump=true;game.update(input,1.f/60.f);input.mouseDy=0;input.jump=false;
        for(int i=0;i<16;++i)game.update(input,1.f/60.f);
        renderer.render(game);saveFrame("jump-look-frame.ppm");
        return out && game.player().health>0 ? 0 : 2;
    }
    retro::Win32Window window(W,H,L"Depthworks");
    if(!window.valid()) return 1;
    retro::Game game;
    game.loadCustomCampaignDirectory((std::filesystem::current_path()/L"custom maps").wstring());
    wchar_t settingsFolder[32768]{};DWORD settingsLength=GetEnvironmentVariableW(L"LOCALAPPDATA",settingsFolder,32768);
    bool directStart=std::wcsstr(commandLine,L"--surface-lift")!=nullptr;
    if(directStart)game=retro::Game::mapInspection({3.5f,2.f},retro::kPi*.5f,0,3,false,0);
    std::wstring settingsPath=settingsLength>0&&settingsLength<32768?std::wstring(settingsFolder)+L"\\RawMetal\\settings.ini":L"";
    if(!settingsPath.empty())game.loadSettings(settingsPath);
    if(settingsLength>0&&settingsLength<32768)game.setSaveDirectory(std::wstring(settingsFolder)+L"\\RawMetal\\saves");
    if(!directStart)game.showTitleScreen();
    retro::SoftwareRenderer renderer(W,H);
    retro::AudioEngine audio;
    if(!std::wcsstr(commandLine,L"--software"))renderer.enableHardware(window.handle());
    std::ofstream("RawMetal-audio.txt")<<(audio.available()?"Stereo audio device opened. ":"No audio output device could be opened. ")<<int(retro::Sound::Count)<<" embedded samples loaded.";
    using clock=std::chrono::steady_clock; auto last=clock::now(); float titleTimer=0;
    while(window.pump()){
        auto now=clock::now(); float dt=std::chrono::duration<float>(now-last).count(); last=now;
        bool wasPaused=game.paused();bool menuOpen=game.titleScreen()||wasPaused||game.inventoryOpen()||game.consoleOpen();game.update(window.input(menuOpen),dt);window.setMenu(game.titleScreen()||game.paused()||game.inventoryOpen()||game.consoleOpen());
        if(wasPaused&&!game.paused()&&!settingsPath.empty())game.saveSettings(settingsPath);
        if(game.quitRequested())break;
        audio.update(game,window.focused()); renderer.render(game); if(!renderer.hardwarePresentsWindow())window.present(renderer.pixels(),renderer.width(),renderer.height());
        titleTimer+=dt; if(titleTimer>.25f){titleTimer=0; wchar_t t[128];if(game.titleScreen())std::swprintf(t,128,L"Depthworks");else std::swprintf(t,128,L"Depthworks | HP %.0f | Shells %d | Monsters %d",game.player().health,game.player().ammo,game.enemiesRemaining());window.setCaption(t);}
    }
    if(!settingsPath.empty())game.saveSettings(settingsPath);
    return 0;
    }catch(const std::exception& error){
        std::ofstream("RawMetal-error.txt")<<error.what();
        if(!std::wcsstr(commandLine,L"--smoke-test"))MessageBoxA(nullptr,error.what(),"Depthworks could not start",MB_OK|MB_ICONERROR);
        return 8;
    }
}
#else
#include <iostream>
int main(){std::cout<<"This demo targets Win32. Build it on Windows with MSVC or MinGW.\n";return 0;}
#endif
