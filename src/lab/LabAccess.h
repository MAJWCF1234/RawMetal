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
 static SoftwareRenderer::Texture skyTexture(int resource){return SoftwareRenderer::loadTexture(resource);}
 static void setSkybox(Game& g,std::string id){g.m_world.setSkybox(std::move(id));}
 // ---- Game ----
 static Player& player(Game& g){return g.m_player;}
 static void setCamera(Game& g,Vec2 pos,float z,float yaw,float pitch){g.m_inspectionCamera=true;g.m_player.pos=pos;g.m_player.angle=yaw;g.m_player.pitch=pitch;
  g.m_player.z=z>-998?z:(g.m_world.hasTerrain()?g.groundHeight(pos,float(World::TerrainMaxZ+1)):g.m_world.floorHeight(pos.x,pos.y));}
 static void playerCamera(Game& g){g.m_inspectionCamera=false;}
 static std::vector<Game::Puddle>& puddles(Game& g){return g.m_puddles;}
 static std::vector<Game::LiquidDrop>& liquidDrops(Game& g){return g.m_liquidDrops;}
 static void crossChunk(Game& g){g.crossChunkBoundary();g.updateStreaming(0);}
 static World& world(Game& g){return g.m_world;}
 static std::vector<Enemy>& enemies(Game& g){return g.m_enemies;}
 static void stepProps(Game& g,float dt,const InputState& input={}){g.updateClutter(input,dt);}
 static auto& friendlies(Game& g){return g.m_friendlyActors;}
 static std::vector<BulletImpact>& impacts(Game& g){return g.m_bulletImpacts;}
 static std::vector<BreakEvent>& breakEvents(Game& g){return g.m_breakEvents;}
 static std::vector<HitBurst>& bursts(Game& g){return g.m_hitBursts;}
 static void hitBurst(Game& g,Point3 p,Point3 dir,ImpactMaterial m,int species,float power,bool kill){g.hitBurst(g.m_level,p,dir,m,species,power,kill);}
 static float& elapsed(Game& g){return g.m_elapsed;}
 static void streaming(Game& g){g.updateStreaming(0);}
 static void spawnCreature(Game& g,const CreatureSpawn& spawn,float awareness=0){g.spawnCreature(spawn,awareness,false);}
 static void loadLevel(Game& g,int level){g.loadLevel(level,false);g.updateStreaming(0);}
 // A man killed where he stands (the damage a hit from `from` does), as play kills him.
 static void killNpc(Game& g,size_t actor,Vec2 from){g.damageFriendly(actor,1e4f,from);}
 // His head taken, as a biting creature's kill takes it.
 static void behead(Game& g,size_t actor){g.setState(g.friendlyKey(actor,"headless"),1);g.setState(g.friendlyKey(actor,"headless_at"),int(g.m_elapsed*1000.f));}
 static bool decodeSave(Game& g,const std::string& data){return g.decodeSave(data);}
 static std::string encodeSave(const Game& g){return g.encodeSave();}
 static std::string legacySaveText(Game& g,int version){return g.legacySaveText(version);}
 static std::shared_ptr<const CustomCampaign> campaignOf(const Game& g){return g.m_customCampaign;}
 // Why the player cannot go on to `next`: 0 he can; 1 the ground is more than a step up; 2 blocked by the world; 3 by a prop.
 static int moveBlock(const Game& g,Vec2 next){const auto& p=g.m_player;float feet=p.z;const float ground=g.groundHeight(next);if(ground>feet+.215f)return 1;if(ground>feet)feet=ground;
  if(!g.hullFits(next,feet,p.hullHeight(),.2f,false,false))return 2;if(!g.hullFits(next,feet,p.hullHeight()))return 3;return 0;}
 static bool damageEnemy(Game& g,Enemy& e,float amount){return g.applyEnemyDamage(e,amount,false,false);}
 static void equip(Game& g,WeaponId id){if(id==WeaponId::Handgun)g.grantHandgun(15);g.equipWeapon(id);}
 // A console line, as typed (Summon.cpp and the rest), and what it printed.
 static void console(Game& g,const std::string& line){g.executeConsole(line);}
 static const std::vector<std::string>& consoleLog(const Game& g){return g.m_consoleLog;}
 static const auto& summons(const Game& g){return g.m_summons;}
 static void setGod(Game& g,bool on){g.m_god=on;if(on)g.m_player.health=std::max(g.m_player.health,100.f);}
 // A shotgun round from the player's eye at a point, as the player's own shot lands (trace, then resolve).
 static ShotKind fireAt(Game& g,Point3 at,ShotHit* traced=nullptr){const Player& p=g.m_player;const Point3 o{p.pos.x,p.pos.y,p.z+p.stanceEye()};Point3 d{at.x-o.x,at.y-o.y,at.z-o.z};
  const float n=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(n<1e-4f)return ShotKind::None;d=d*(1/n);const auto& gun=weaponDef(WeaponId::Shotgun).hitscan;
  const auto hit=g.traceShot(o,d,40,-1,false,&gun);if(traced)*traced=hit;g.resolveShot(hit,o,d,gun,true);return hit.kind;}
 // A sound raised as if the game had raised it this frame (heard by the audio engine's next update).
 static void raise(Game& g,const SoundEvent& e){g.m_sounds.push_back(e);}
 static void omitSound(Game& g,Sound sound){std::erase_if(g.m_sounds,[&](const auto& event){return event.sound==sound;});}
 static void mute(Game& g,bool music){g.m_musicEnabled=!music;}
 // ---- AudioEngine, mixed headless (AudioEngine(false)) ----
 static void mix(AudioEngine& a,std::vector<int16_t>& out,size_t frames){const size_t at=out.size();out.resize(at+frames*2);a.mix(out.data()+at,frames);}
 static void silenceScore(AudioEngine& a){a.m_musicGain=0;}
 // What plays: each voice's sound, speech clip (-1 none) and speaker (actor+1, 0 none).
 struct Playing {Sound sound;int speech;size_t speaker;};
 static std::vector<Playing> playing(const AudioEngine& a){std::vector<Playing> out;for(const auto& v:a.m_voices)if(!v.loop)out.push_back({v.sound,v.speech,v.speaker});return out;}
 // Every voice the mixer holds, loops included: what it is, whether it just started (cursor 0),
 // and the level it reaches the ears at (its gain, spatial pan and the bus: score or effects).
 struct Held {Sound sound;int speech;bool loop,fresh,music,spatial;float level,occlusion,filter;Vec2 at;float z;};
 static std::vector<Held> held(const AudioEngine& a){std::vector<Held> out;
  for(const auto& v:a.m_voices){const bool music=v.emitter==-1||v.emitter==-3||v.emitter==AudioEngine::AlertEmitter||(v.emitter<AudioEngine::AreaEmitter&&v.emitter>AudioEngine::AreaEmitter-AudioEngine::AreaScores);
   out.push_back({v.sound,v.speech,v.loop,v.cursor==0,music,v.spatial,v.gain*(v.left+v.right)*.5f*(music?a.m_musicGain:a.m_effectsGain),v.occlusion,v.filterTarget,v.position,v.z});}
  return out;}
 // After an update, before a mix: keep one bus of what the engine holds. 0 the one-shots (steps,
 // shots, cries, impacts, speech), 1 the score, 2 the beds and machines (loops that are not score).
 static void onlyBus(AudioEngine& a,int bus){
  for(auto& v:a.m_voices){const bool music=v.emitter==-1||v.emitter==-2||v.emitter==-3||v.emitter==AudioEngine::AlertEmitter||(v.emitter<AudioEngine::AreaEmitter&&v.emitter>AudioEngine::AreaEmitter-AudioEngine::AreaScores);
   const int of=music?1:v.loop?2:0;if(of!=bus){v.left=v.right=v.smoothLeft=v.smoothRight=0;}}}
 // Keep the actual mixed voice of one sound, for measuring an impact separately from its gunshot.
 static void onlySound(AudioEngine& a,Sound sound){std::erase_if(a.m_voices,[&](const auto& v){return v.sound!=sound;});}
 // Water (WaterFx.cpp): a splash raised as the game raises one; what the AI hears this frame.
 static void waterSplash(Game& g,Point3 at,float power,WaterSplashKind kind,float delay=0){g.raiseWaterSplash(g.m_level,at,{0,0,-1},power,kind,delay);}
 static std::vector<AiStimulus> stimuli(const Game& g){return g.aiStimuli();}
 static Vec2& velocity(Game& g){return g.m_velocity;}
 // Things that break, and what they leave (tubes/Debris.cpp).
 static std::vector<Clutter>& clutter(Game& g){return g.m_clutter;}
 static const Game::RigidStats& rigidStats(const Game& g){return g.m_rigidStats;}
 static std::vector<Game::BodyKnock>& bodyKnocks(Game& g){return g.m_bodyKnocks;}
 static void knock(Game& g,size_t index,Point3 at,Point3 impulse){g.knockClutter(g.m_level,index,at,impulse);}
 static Vec2& carryHandVelocity(Game& g){return g.m_carryHandVelocity;}
 static int entityProperty(Game& g,int level,std::string_view entity,std::string_view key){return g.entityPropertyInt(level,entity,key);}
 static void detonateBarrel(Game& g,int x,int y){g.detonateBarrel(g.m_level,x,y);}
 static void damagePane(Game& g,int index,float amount,Point3 at,Point3 direction){g.damagePane(g.m_level,index,amount,at,direction);}
 static bool damagePane(Game& g,int index,float amount,Point3 at,Point3 direction,PaneBlow blow){return g.damagePane(g.m_level,index,amount,at,direction,blow);}
 // A round of `gun` from `origin` toward `at`, as the player fires it.
 static ShotHit fireFrom(Game& g,Point3 origin,Point3 at,const HitscanDef& gun){Point3 d{at.x-origin.x,at.y-origin.y,at.z-origin.z};const float n=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);d=d*(1/std::max(n,1e-4f));
  const auto hit=g.traceShot(origin,d,40,-1,false,&gun);g.resolveShot(hit,origin,d,gun,true);return hit;}
 static ShotHit trace(Game& g,Point3 origin,Point3 direction,float distance){return g.traceShot(origin,direction,distance);}
 static void landShards(Game& g){g.landShards();}
 // The marks drawn on breaking glass, in each pane's plane (Scene3D records them while asked).
 static void recordGlassDecals(SoftwareRenderer& r,bool on){r.m_recordGlassDecals=on;r.m_glassDecals.clear();}
 static const std::vector<SoftwareRenderer::GlassDecal>& glassDecals(const SoftwareRenderer& r){return r.m_glassDecals;}
 static Vec2& crunchFrom(Game& g){return g.m_glassCrunchLast;}
 static void updateGlass(Game& g,float dt){g.updateGlass(dt);}
 static std::vector<SoundEvent>& sounds(Game& g){return g.m_sounds;}
 static void breakClutter(Game& g,size_t index,Point3 direction){g.breakClutter(g.m_level,index,direction);g.landShards();}
 static void blastProps(Game& g,Point3 at,bool tiles){g.blastProps(at,tiles);}
 // The player's hand on the prop in front of him (or off the one he holds), as the use key does.
 static bool grab(Game& g){return g.interactClutter();}
 static int held(const Game& g){return g.m_heldClutter;}
 static bool shieldBlow(Game& g,float amount,Vec2 source){return g.shieldBlow(amount,source);}
 static void stepSoft(Game& g,float dt){g.updateSoftBodies(dt);}
 static std::vector<SoftPad>& softPads(Game& g){return g.m_softPads;}
 // ---- SoftwareRenderer ----
 using Corpse=SoftwareRenderer::Corpse;
 static std::unordered_map<std::uint64_t,Corpse>& corpses(SoftwareRenderer& r){return r.m_corpses;}
 static Corpse* humanCorpse(SoftwareRenderer& r,const Game& g,int actor){
  if(actor<0||size_t(actor)>=g.npcActors().size())return nullptr;const auto& n=g.npcActors()[size_t(actor)];
  return r.humanCorpse(g,g.chunkWorld(g.level()),g.level(),n.track(),n.pose,actor,0xA000000000000000ull|std::uint64_t(actor));}
 static Corpse* trackCorpse(SoftwareRenderer& r,const Game& g,int level,size_t track){const World& w=g.chunkWorld(level);if(track>=w.actorTracks().size())return nullptr;
  return r.humanCorpse(g,w,level,w.actorTracks()[track],w.actorPose(track),-1,0xB000000000000000ull|(std::uint64_t(std::uint32_t(level))<<32)|track);}
 // Between tubes (and before a determinism repeat): nothing a scene left in the renderer carries over.
 // The Vulkan renderer behind it (null on the software path), and where library models begin.
 static GpuRenderer* gpu(SoftwareRenderer& r){return r.m_gpu.get();}
 static constexpr int firstLibraryModel(){return SoftwareRenderer::FirstLibraryModel;}
 static void resetRenderer(SoftwareRenderer& r){r.m_corpses.clear();r.m_movers.clear();r.m_corpseClock=0;r.m_goreBursts.clear();r.forgetScene();}
 // The men's bodies: 0-5 the civilians (civilianActorBody), 6.. the cleanup crew (cleanupActorBody).
 static Mesh& humanMesh(SoftwareRenderer& r,int i){return i<6?r.civilianMesh(i):*r.cleanupMeshes()[size_t(i-6)];}
 static int humanMeshes(){return 6+int(kCleanupBodies);}
 // A dead creature's body, as the renderer keeps it (level, slot in the chunk's creatures); and the mesh it was cut from.
 static Carcass* remains(SoftwareRenderer& r,int level,size_t slot){auto it=r.m_remains.find((std::uint64_t(std::uint32_t(level))<<32)|std::uint64_t(slot));return it==r.m_remains.end()||!it->second.carcass.built()?nullptr:&it->second.carcass;}
 static std::vector<MeshTriangle> creatureCorpseMesh(SoftwareRenderer& r,const Enemy& e){std::vector<MeshTriangle> out;r.poseCreatureMesh(e,0,true,out);return out;}
 static const humanoid::Labels& humanParts(SoftwareRenderer& r,int i){return SoftwareRenderer::humanParts(humanMesh(r,i),i<6?i:-1-(i-6));}
 static Point3 cameraPoint(const SoftwareRenderer& r,Point3 p){return r.cameraPoint(p,r.m_view);}
 // The last frame's view depth at a pixel (9999 where nothing was drawn): tubes/Kits.cpp.
 static float depthAt(const SoftwareRenderer& r,int x,int y){return r.m_zbuffer[size_t(y)*size_t(r.width())+size_t(x)];}
 // Plant physbones (tubes/Plants.cpp).
 static PlantBones& plants(SoftwareRenderer& r){return r.m_plants;}
 static bool plantModel(SoftwareRenderer& r,int model){return r.plantModel(model);}
 static const Player& view(const SoftwareRenderer& r){return r.m_view;}
 // The sheet a shard is drawn with, and the sheets its sources are drawn with (Scene3D: a 'B' tile, a
 // 'C' tile, an intact breakable of `material`, a facility model).
 static SoftwareRenderer::ShardLook shardLook(const SoftwareRenderer& r,const Clutter& c){return r.shardLook(c);}
 static const SoftwareRenderer::Texture* tileSheet(const SoftwareRenderer& r,char tile){return tile=='B'?&r.m_barrelTexture:&r.m_crateTexture;}
 static const SoftwareRenderer::Texture* breakableSheet(const SoftwareRenderer& r,int material){return material==22?&r.m_dropCeiling:material==17?&r.m_glassTexture:&r.m_crateTexture;}
 static const SoftwareRenderer::Texture* modelSheet(const SoftwareRenderer& r,int model){return &r.facilityTexture(model,0);}
 static const Mesh& facilityMesh(const SoftwareRenderer& r,int model){return r.m_facilityMeshes[size_t(model)];}
 static const SoftwareRenderer::Texture* modelPart(const SoftwareRenderer& r,int model,int part){return &r.facilityTexture(model,part);}
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
 static Settings& settings(Game& g){return g.m_settings;}
 // The body the local player was last drawn as (-1 before the first frame).
 static int localCharacter(const SoftwareRenderer& r){return r.m_localCharacter;}
 // The body's faces drawn last frame (index, world corners), while collecting.
 // The local body starts afresh (stance, transitions, blends, poser), as on a new game: a tube run
 // again on the same renderer otherwise begins with the last run's stance change.
 static void resetBody(SoftwareRenderer& r){r.m_bodyStance=-1;r.m_bodyTransitionClip=-1;r.m_bodyLastElapsed=-1;r.m_localCharacter=-1;r.m_tpBodyYaw=-1e9f;r.m_tpTurnClip=-1;r.m_tpYawTime=-1;r.m_outsideHitStart=-1e9f;r.m_outsideLandStart=-1e9f;r.m_outsideAir=0;r.m_outsideDamage=0;}
 static void collectBodyFaces(SoftwareRenderer& r,bool on){r.m_collectBodyFaces=on;r.m_drawnBodyFaces.clear();}
 static const std::vector<SoftwareRenderer::DrawnBodyFace>& drawnBodyFaces(const SoftwareRenderer& r){return r.m_drawnBodyFaces;}
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
