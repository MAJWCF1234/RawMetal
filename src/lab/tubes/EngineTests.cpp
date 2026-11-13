// The engine's own --each-test entries for the seeded features, runnable in the
// lab build (seconds to rebuild, no packing): `--feature=engine.doors`. Each runs
// with the job folder as its working directory, so its report file lands there.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "renderer/Carcass.h"
namespace retro {bool testDoorLeaves();} // game/Movement.cpp
namespace retro::lab {
namespace {
bool engine(const char* name,const char* summary,bool(*run)()){return add({.name=std::string("engine.")+name,.summary=summary,.standalone=run});}
const bool registered=[]{
 engine("doors","--each-test=doors: weight, hand, shove, sounds, battering, lock fields",&Game::testDoors);
 engine("door-axis","--each-test=door-axis: doors along Y",&Game::testDoorAxis);
 engine("door-leaf","--each-test=door-leaf: gate leaves",&testDoorLeaves);
 engine("human-ragdoll","--each-test=human-ragdoll: every Chapter 2 body at rest, a killed guard, a reload",&SoftwareRenderer::testHumanRagdoll);
 engine("carcass","--carcass-test: creature carcasses fall, fold and come apart",&Carcass::test);
 engine("chimera-hunt","--each-test=chimera-hunt: the bite, the meal, the return",&Game::testChimeraHunt);
 // The voices: what the game raises, what the mixer plays, the bank and saves (tubes/DeathVoices.cpp leans on them).
 engine("audio","--audio-test: the bank decodes, masks, speech caps, the mix",&AudioEngine::test);
 engine("audio-events","--each-test=audio-events",&Game::testAudioEvents);
 engine("npc-voices","--each-test=npc-voices: who speaks, when, once",&Game::testNpcVoices);
 engine("dialogue","--each-test=dialogue",&Game::testDialogue);
 engine("saves","--each-test=saves",&Game::testSaves);
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
