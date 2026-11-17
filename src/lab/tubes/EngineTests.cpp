// The engine's own --each-test entries for the seeded features, runnable in the
// lab build (seconds to rebuild, no packing): `--feature=engine.doors`. Each runs
// with the job folder as its working directory, so its report file lands there.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "renderer/Carcass.h"
#include "world/CampaignExport.h"
#include "world/CampaignMaps.h"
#include <algorithm>
namespace retro {bool testDoorLeaves();} // game/Movement.cpp
namespace retro {bool testSkyboxes();}
namespace retro::lab {
namespace {
bool engine(const char* name,const char* summary,bool(*run)()){return add({.name=std::string("engine.")+name,.summary=summary,.standalone=run});}
const bool registered=[]{
 engine("doors","--each-test=doors: weight, hand, shove, sounds, battering, lock fields",&Game::testDoors);
 engine("hearing","--each-test=hearing: who hears what, through what, how far",&Game::testHearing);
 engine("skyboxes","all 47 purchased skies decode, preserve terrain and switch on the native renderer",&testSkyboxes);
 engine("door-axis","--each-test=door-axis: doors along Y",&Game::testDoorAxis);
 engine("door-leaf","--each-test=door-leaf: gate leaves",&testDoorLeaves);
 engine("human-ragdoll","--each-test=human-ragdoll: every Chapter 2 body at rest, a killed guard, a reload",&SoftwareRenderer::testHumanRagdoll);
 engine("carcass","--carcass-test: creature carcasses fall, fold and come apart",&Carcass::test);
 engine("gore","--gore-test: the carcass test, then creatures shot and blown apart as the renderer draws them",+[]{std::ofstream r("gore-render-test.txt");return Carcass::test()&&SoftwareRenderer::testGore(r);});
 engine("chimera-hunt","--each-test=chimera-hunt: the bite, the meal, the return",&Game::testChimeraHunt);
 // The voices: what the game raises, what the mixer plays, the bank and saves (tubes/DeathVoices.cpp leans on them).
 engine("audio","--audio-test: the bank decodes, masks, speech caps, the mix (smoke; --audio-test=full for every map)",+[]{return AudioEngine::test(false);});
 engine("audio-events","--each-test=audio-events",&Game::testAudioEvents);
 engine("npc-voices","--each-test=npc-voices: who speaks, when, once",&Game::testNpcVoices);
 engine("dialogue","--each-test=dialogue",&Game::testDialogue);
 engine("saves","--each-test=saves",&Game::testSaves);
 // The format 28 section container: formats 1-27 refitted with backups, real saves (copied to a temp folder), damaged and unknown sections, the stores and hooks.
 engine("save-format","save sections: the load paths and backups, damaged and unknown sections, stores, physics hook, the misplaced save folder",+[]{std::ofstream r("save-format.txt");return Game::testSaveFormat(r,Game::SaveTestCore);});
 engine("save-refit-room","custom-room saves of formats 12-27 refit to 28 with backups and re-save byte-exact",+[]{std::ofstream r("save-refit-room.txt");return Game::testSaveFormat(r,Game::SaveTestRoomFormats);});
 engine("save-refit-early","campaign saves of formats 1-6 refit to 28 with backups and re-save byte-exact",+[]{std::ofstream r("save-refit-early.txt");return Game::testSaveFormat(r,Game::SaveTestCampaignEarly);});
 engine("save-refit-late","campaign saves of formats 7-11, 19 and 27 refit to 28 with backups and re-save byte-exact",+[]{std::ofstream r("save-refit-late.txt");return Game::testSaveFormat(r,Game::SaveTestCampaignLate);});
 engine("save-real","real saves from past releases (%LOCALAPPDATA% save folders, lab/fixtures/saves, RM_REAL_SAVES=dir;dir), copied to a temp folder: load, refit, re-save byte-exact",+[]{std::ofstream r("save-real.txt");return Game::testSaveFormat(r,Game::SaveTestRealSaves);});
 engine("saves-in-play","saves written in play on maps 0-109 (deaths, debris, fights, checkpoints, seams) all load; RM_SAVE_MAPS=a-b narrows",+[]{std::ofstream r("saves-in-play.txt");return Game::testSavesInPlay(r);});
 // Saves other builds wrote (RM_LOAD_SAVES=DIR: every *.rms in it): each loads, or is refused with its reason.
 engine("load-saves","every *.rms in RM_LOAD_SAVES loads in this build (old releases' saves upgrade)",+[]{
  std::ofstream r("load-saves.txt");const char* dir=std::getenv("RM_LOAD_SAVES");if(!dir){r<<"FAIL RM_LOAD_SAVES not set\n";return false;}
  bool ok=true;int n=0;std::error_code e;
  for(const auto& f:std::filesystem::directory_iterator(dir,e)){if(f.path().extension()!=".rms")continue;++n;
   std::ifstream in(f.path(),std::ios::binary);std::string data((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
   Game g;const bool loaded=LabAccess::decodeSave(g,data);ok&=loaded;
   r<<(loaded?"PASS ":"FAIL ")<<f.path().filename().string()<<" ("<<data.substr(0,data.find('\n'))<<")";if(loaded)r<<" map "<<g.level();else r<<": "<<Game::lastLoadFailure();r<<'\n';r.flush();}
  r<<n<<" saves\n";return ok&&n>0;});
 // The map audit (--map-audit) over campaign maps RM_AUDIT_MAPS=a-b (default all): map-audit-N.txt each.
 engine("map-audit","--map-audit over campaign maps (RM_AUDIT_MAPS=a-b); every map-audit-N.txt in the job folder",+[]{
  int first=0,last=CampaignChunkCount-1;if(const char* r=std::getenv("RM_AUDIT_MAPS"))std::sscanf(r,"%d-%d",&first,&last);
  bool all=true;std::ofstream sum("map-audit-summary.txt");
  for(int level=std::max(0,first);level<=last&&level<CampaignChunkCount;++level){bool ok=false;try{ok=Game::auditMap(level);}catch(const std::exception& e){sum<<"map "<<level<<" threw "<<e.what()<<'\n';}all&=ok;sum<<"map "<<level<<(ok?" PASS":" FAIL")<<'\n';sum.flush();}
  return all;});
 // Map 95's hit engineer flinched at every round and never died: any Cleanup man on a keyed track can.
 engine("cleanup-die","every Cleanup man on a keyed track has health and a dead state; 95's sitting engineer is shot dead",+[]{
  std::ofstream r("cleanup-die.txt");bool ok=true;int armed=0;
  for(int level=0;level<CampaignChunkCount;++level){const auto map=campaignMap(level);if(!map)continue;
   for(size_t i=0;i<map->actorTracks.size();++i){const auto& t=map->actorTracks[i];if(t.platform||t.ai.mode!=ActorAiMode::Scripted||!cleanupActorVisual(t.visual)||t.keys.empty())continue;
    if(std::all_of(t.keys.begin(),t.keys.end(),[](const ActorKey& k){return k.clip==4;}))continue;++armed;
    const bool can=t.health>0&&t.deadState&&t.damageState;ok&=can;if(!can)r<<"FAIL map "<<level<<" track "<<i<<" cannot die\n";}}
  r<<armed<<" Cleanup men on keyed tracks across the campaign\n";
  Game g;LabAccess::loadLevel(g,95);World& w=LabAccess::world(g);int who=-1;
  for(size_t i=0;i<w.actorTracks().size();++i){const auto& t=w.actorTracks()[i];if(t.visual==ActorVisual::CleanupEngineer&&t.ai.mode==ActorAiMode::Scripted&&length(w.actorPose(i).position-Vec2{18.6f,14.3f})<.5f)who=int(i);}
  if(who<0){r<<"FAIL no sitting engineer at 18.6,14.3 on map 95\n";return false;}
  const auto& t=w.actorTracks()[size_t(who)];const auto pose=w.actorPose(size_t(who));
  LabAccess::setCamera(g,pose.position-Vec2{2.2f,0},-999,0,0);LabAccess::playerCamera(g);
  int shots=0;for(;shots<12&&!(t.deadState&&g.state(t.deadState));++shots){LabAccess::fireAt(g,{pose.position.x,pose.position.y,pose.z+.7f});g.update({},1.f/60);}
  const bool dead=t.deadState&&g.state(t.deadState);for(int i=0;i<60;++i)g.update({},1.f/60);
  const bool down=LabAccess::world(g).actorPose(size_t(who)).clip==4;
  r<<(dead&&down?"PASS":"FAIL")<<" map 95 engineer (track "<<who<<", health "<<t.health<<") dead after "<<shots<<" rounds, lying down "<<down<<'\n';
  Game loaded;const bool kept=LabAccess::decodeSave(loaded,LabAccess::encodeSave(g))&&loaded.state(t.deadState);r<<(kept?"PASS":"FAIL")<<" his death survives a save\n";
  return ok&&dead&&down&&kept;});
 engine("campaign-maps","--campaign-map-test: every map loads, spawns fit, routes, gates and set pieces",&Game::testCampaignMaps);
 engine("scripting","--each-test=scripting: script events, actions, checkpoints",&Game::testScripting);
 engine("console","--each-test=console: the console, map ids, god, fps",&Game::testConsole);
 engine("friendly-ai","--each-test=friendly-ai: NPC AI, saves of its state",&Game::testFriendlyAI);
 engine("stealth","--each-test=stealth: takedowns, restraint, saves",&Game::testStealth);
 engine("ai","--each-test=ai: guards, hearing, friendly AI and the creatures (Game::testAI)",&Game::testAI);
 engine("creature-schedule","--each-test=creature-schedule: creature schedules, wake, bite and fight timings",&Game::testCreatureSchedule);
 engine("creature-behaviour","--each-test=creature-behaviour",&Game::testCreatureBehaviour);
 engine("machine","--each-test=machine: the cursed equipment",&Game::testMachine);
 engine("summon","console summon: kinds, placement, allegiances, fights, aimed commands, transient through a save",+[]{std::ofstream r("summon-test.txt");return Game::testSummon(r);});
 engine("noclip","console noclip: through walls, up and down, sprint, not saved",+[]{std::ofstream r("noclip-test.txt");return Game::testNoclip(r);});
 // What breaks and what it leaves (tubes/Debris.cpp works the pieces; these keep the old behaviour).
 engine("clutter","--each-test=clutter: junk and shards lift, fall, land and sound",&Game::testClutter);
 engine("props","--each-test=props: model props, breaking, held and thrown",&Game::testProps);
 engine("glass","--each-test=glass: panes, breakables, blasts",&Game::testGlass);
 engine("physics-stages","--each-test=physics-stages: props through the physics stages",&Game::testPhysicsStages);
 engine("prop-saves","--each-test=prop-saves: props and their state through a save",&Game::testPropSaves);
 engine("combat","--each-test=combat: shots, barrels, melee",&Game::testCombat);
  engine("handgun","--each-test=handgun: firearms, ammo and water surface hits",&Game::testHandgun);
 // The map records (WET joined them): every chunk through the RMC exporter and back, and the worlds kept apart.
 engine("rmc-export","--each-test=rmc-export: every map exported to RMC, loaded back, compared record by record",+[]{return testRmcExportRoundTrip("rmc-export-test.txt");});
 engine("world-isolation","--world-isolation-test: maps keep their own light, wetness and records",&Game::testWorldIsolation);
 #ifdef RAWMETAL_LAB_HAS_PARTICLES
 engine("particles","--each-test=particles: determinism, budgets, walls, records, dressed maps",&testParticles);
 #endif
 #ifdef RAWMETAL_LAB_HAS_PROBES
 engine("light-probes","--each-test=light-probes: bounce, crystal, every map's bake",&testLightProbes);
 #endif
 #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
 engine("third-person","--each-test=third-person: Claire from outside",&SoftwareRenderer::testThirdPerson);
 #endif
 return true;}();
}
}
