#pragma once
// The feature lab's one door into the engine's privates. Game and
// SoftwareRenderer name LabAccess a friend; nothing else is opened up. Tubes
// stage scenes and read state through these, so a change of a private member
// is mended here once, not in every tube.
//
// Header only: the game's console uses snapshot() (`labsnap NAME`), so the game
// exe needs no lab source file.
#include "game/Game.h"
#include "renderer/SoftwareRenderer.h"
#include "renderer/GpuRenderer.h"
#include "renderer/Projection.h"
#include "audio/AudioEngine.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
// Features still being polished in the main tree: reached only when present.
#if __has_include("renderer/Particles.h")
#include "renderer/Particles.h"
#define RAWMETAL_LAB_HAS_PARTICLES 1
#endif
#if __has_include("renderer/LightProbes.h")
#include "renderer/LightProbes.h"
#define RAWMETAL_LAB_HAS_PROBES 1
#endif
#if __has_include("renderer/ClaireOutside.h")
#include "renderer/ClaireOutside.h"
#define RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE 1
#endif
namespace retro {
struct LabAccess {
 // ---- Game ----
 static Player& player(Game& g){return g.m_player;}
 static void setCamera(Game& g,Vec2 pos,float z,float yaw,float pitch){g.m_inspectionCamera=true;g.m_player.pos=pos;g.m_player.angle=yaw;g.m_player.pitch=pitch;
  g.m_player.z=z>-998?z:(g.m_world.hasTerrain()?g.groundHeight(pos,float(World::TerrainMaxZ+1)):g.m_world.floorHeight(pos.x,pos.y));}
 static void playerCamera(Game& g){g.m_inspectionCamera=false;}
 static World& world(Game& g){return g.m_world;}
 static std::vector<Enemy>& enemies(Game& g){return g.m_enemies;}
 static std::vector<BulletImpact>& impacts(Game& g){return g.m_bulletImpacts;}
 static std::vector<HitBurst>& bursts(Game& g){return g.m_hitBursts;}
 static void hitBurst(Game& g,Point3 p,Point3 dir,ImpactMaterial m,int species,float power,bool kill){g.hitBurst(g.m_level,p,dir,m,species,power,kill);}
 static float& elapsed(Game& g){return g.m_elapsed;}
 static void streaming(Game& g){g.updateStreaming(0);}
 static void spawnCreature(Game& g,const CreatureSpawn& spawn,float awareness=0){g.spawnCreature(spawn,awareness,false);}
 static void loadLevel(Game& g,int level){g.loadLevel(level,false);g.updateStreaming(0);}
 // A man killed where he stands (the damage a hit from `from` does), as play kills him.
 static void killNpc(Game& g,size_t actor,Vec2 from){g.damageFriendly(actor,1e4f,from);}
 static bool decodeSave(Game& g,const std::string& data){return g.decodeSave(data);}
 static bool damageEnemy(Game& g,Enemy& e,float amount){return g.applyEnemyDamage(e,amount,false,false);}
 static void equip(Game& g,WeaponId id){if(id==WeaponId::Handgun)g.grantHandgun(15);g.equipWeapon(id);}
 static void setGod(Game& g,bool on){g.m_god=on;if(on)g.m_player.health=std::max(g.m_player.health,100.f);}
 // A shotgun round from the player's eye at a point, as the player's own shot lands (trace, then resolve).
 static ShotKind fireAt(Game& g,Point3 at){const Player& p=g.m_player;const Point3 o{p.pos.x,p.pos.y,p.z+p.stanceEye()};Point3 d{at.x-o.x,at.y-o.y,at.z-o.z};
  const float n=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(n<1e-4f)return ShotKind::None;d=d*(1/n);const auto& gun=weaponDef(WeaponId::Shotgun).hitscan;
  const auto hit=g.traceShot(o,d,40,-1,false,&gun);g.resolveShot(hit,o,d,gun,true);return hit.kind;}
 static std::string encodeSave(const Game& g){return g.encodeSave();}
 // A sound raised as if the game had raised it this frame (heard by the audio engine's next update).
 static void raise(Game& g,const SoundEvent& e){g.m_sounds.push_back(e);}
 static void mute(Game& g,bool music){g.m_musicEnabled=!music;}
 // ---- AudioEngine, mixed headless (AudioEngine(false)) ----
 static void mix(AudioEngine& a,std::vector<int16_t>& out,size_t frames){const size_t at=out.size();out.resize(at+frames*2);a.mix(out.data()+at,frames);}
 static void silenceScore(AudioEngine& a){a.m_musicGain=0;}
 // What plays: each voice's sound, speech clip (-1 none) and speaker (actor+1, 0 none).
 struct Playing {Sound sound;int speech;size_t speaker;};
 static std::vector<Playing> playing(const AudioEngine& a){std::vector<Playing> out;for(const auto& v:a.m_voices)if(!v.loop)out.push_back({v.sound,v.speech,v.speaker});return out;}
 // ---- SoftwareRenderer ----
 using Corpse=SoftwareRenderer::Corpse;
 static std::unordered_map<std::uint64_t,Corpse>& corpses(SoftwareRenderer& r){return r.m_corpses;}
 static Corpse* humanCorpse(SoftwareRenderer& r,const Game& g,int actor){
  if(actor<0||size_t(actor)>=g.npcActors().size())return nullptr;const auto& n=g.npcActors()[size_t(actor)];
  return r.humanCorpse(g,g.chunkWorld(g.level()),g.level(),n.track(),n.pose,actor,0xA000000000000000ull|std::uint64_t(actor));}
 static Corpse* trackCorpse(SoftwareRenderer& r,const Game& g,int level,size_t track){const World& w=g.chunkWorld(level);if(track>=w.actorTracks().size())return nullptr;
  return r.humanCorpse(g,w,level,w.actorTracks()[track],w.actorPose(track),-1,0xB000000000000000ull|(std::uint64_t(std::uint32_t(level))<<32)|track);}
 // Between tubes (and before a determinism repeat): nothing a scene left in the renderer carries over.
 static void resetRenderer(SoftwareRenderer& r){r.m_corpses.clear();r.m_corpseClock=0;r.m_goreBursts.clear();r.forgetScene();}
 static Point3 cameraPoint(const SoftwareRenderer& r,Point3 p){return r.cameraPoint(p,r.m_view);}
 static const Player& view(const SoftwareRenderer& r){return r.m_view;}
 static int bloodDecalCount(const SoftwareRenderer& r,SoftwareRenderer::Fluid f,SoftwareRenderer::BloodJob j){return int(r.m_bloodDecals[size_t(f)][size_t(j)].size());}
 static SoftwareRenderer::Fluid fluidOf(int species){const auto& v=SoftwareRenderer::creatureVisuals;return v[size_t(std::clamp(species,0,int(v.size())-1))].fluid;}
 #ifdef RAWMETAL_LAB_HAS_PARTICLES
 static const ParticleStats* particleStats(const SoftwareRenderer& r){return r.m_particleMemory?&r.m_particleMemory->last:nullptr;}
 static ParticleMemory& particleMemory(SoftwareRenderer& r){return r.particleMemory();}
 #endif
 #ifdef RAWMETAL_LAB_HAS_PROBES
 static std::shared_ptr<const ProbeBake> probes(const SoftwareRenderer& r,int level){return level>=0&&size_t(level)<r.m_chunkProbes.size()?r.m_chunkProbes[size_t(level)].bake:nullptr;}
 #endif
 #ifdef RAWMETAL_LAB_HAS_CLAIRE_OUTSIDE
 static const ClaireOutside::Report& claireReport(const SoftwareRenderer& r){return r.m_outsideReport;}
 #endif
 // ---- Snapshot: the scene in play as a lab fixture (console `labsnap NAME`) ----
 // Writes DIR/NAME.lab (world, camera) and DIR/NAME.sav (the game's own save:
 // every state, creature, door and body as play left them). DIR is RM_LAB_FIXTURES
 // or lab/fixtures under the working directory. Returns the .lab path, or "".
 static std::string snapshot(const Game& g,const std::string& name){
  namespace fs=std::filesystem;std::string dir="lab/fixtures";if(const char* e=std::getenv("RM_LAB_FIXTURES"))dir=e;
  std::error_code ec;fs::create_directories(dir,ec);const std::string base=dir+"/"+(name.empty()?"snapshot":name);
  {std::ofstream sav(base+".sav",std::ios::binary);sav<<g.encodeSave();if(!sav)return "";}
  std::ofstream lab(base+".lab");if(!lab)return "";
  const Player& p=g.m_player;char line[256];
  lab<<"# Snapshot of a game in play (console labsnap). The save holds every state, creature, door and body.\n";
  if(g.m_worldId==WorldId::Custom&&g.m_customCampaign)lab<<"world custom \""<<g.m_customCampaign->sourceFile<<"\"\n";
  else if(g.m_worldId==WorldId::Ashfall)lab<<"world ashfall\n";else lab<<"world campaign\n";
  lab<<"map "<<g.m_level<<"\n";
  std::snprintf(line,sizeof line,"camera %.3f %.3f %.3f %.2f %.1f\n",double(p.pos.x),double(p.pos.y),double(p.z),double(p.angle*57.29578f),double(p.pitch));lab<<line;
  lab<<"save "<<fs::path(base+".sav").filename().string()<<"\nframes 60\n";
  return lab?base+".lab":"";
 }
};
}
