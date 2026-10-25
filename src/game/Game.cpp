#include "../core/FrameProfile.h"
#include "Game.h"
#include "../world/CustomCampaign.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <fstream>
#include <filesystem>

namespace retro {

Game::Game(WorldId id):m_worldId(id) { restart(); }
Game::Game(std::shared_ptr<const CustomCampaign> campaign):m_worldId(WorldId::Custom),m_customCampaign(std::move(campaign)){
 if(!m_customCampaign||m_customCampaign->maps.empty())throw std::runtime_error("Cannot start an empty custom campaign");
 m_customCampaigns.push_back(m_customCampaign);
 m_customCampaignKey=m_customCampaign->key;m_level=std::clamp(m_customCampaign->startMap,0,int(m_customCampaign->maps.size())-1);restart();
}
World Game::makeWorld(int level)const{
 if(m_worldId==WorldId::Custom){
  if(!m_customCampaign||level<0||level>=int(m_customCampaign->maps.size()))throw std::runtime_error("Custom campaign map index is unavailable");
  return World(level,m_customCampaign->maps[size_t(level)]);
 }
 return World(level,m_worldId);
}
void Game::loadCustomCampaignDirectory(std::wstring directory){
 m_customMapDirectory=std::move(directory);std::vector<std::string> errors;
 m_customCampaigns=retro::loadCustomCampaignDirectory(std::filesystem::path(m_customMapDirectory),&errors);
 std::ofstream report("RawMetal-custom-maps.txt",std::ios::trunc);
 report<<"Loaded "<<m_customCampaigns.size()<<" custom campaign pack(s).\n";
 for(const auto& campaign:m_customCampaigns)report<<"OK "<<campaign->sourceFile<<" / "<<campaign->name<<" / "<<campaign->maps.size()<<" map(s)\n";
 for(const auto& error:errors)report<<"ERROR "<<error<<'\n';
}
void Game::showTitleScreen(){
    m_titleScreen=true;m_titleSelection=0;m_customMenuOffset=0;m_titlePrevious={};m_menuFromTitle=false;m_customMapsOpen=false;
    m_paused=false;m_inventoryOpen=false;m_consoleOpen=false;m_menuPage=MenuPage::Settings;
    m_menuSelection=0;m_menuMessage.clear();m_dragSlider=-1;m_suppressFire=true;refreshSaveSlots();
}

void Game::restart(){m_previousSubdue=false;++m_sessionRevision;m_hitStopRemaining=0;m_bulletImpacts.clear();m_barrelExplosions.clear();m_hazmat={};m_hazmatPushCooldown=0;m_inventoryOpen=false;m_equippedWeapon=WeaponId::Shotgun;m_weaponEquipped=true;m_medkits=0;m_selectedItem=-1;m_itemCells={12,0,2};m_states.clear();m_objectives.clear();m_questItems.clear();m_firedEvents.clear();m_scriptEvents.clear();m_friendlyActors.clear();m_guardShots.clear();m_hazardSoundTimer=0;m_previousFlashlight=false;int start=m_level;for(int level=0;level<chunkCount();++level){loadLevel(level,false);storeChunk();}loadLevel(std::min(start,chunkCount()-1),false);seedScripts();updateStreaming(0);rebuildFriendlyActors();
 // Fresh worlds and restored saves share one zero-time pose initialization.
 // This stamps canonical machinery/NPC state without executing script events
 // or advancing AI work, sounds, healing or finite resource clocks.
 updateMechanisms(0);
 m_handsLowered=m_world.definition().startUnarmed;if(m_handsLowered)equipWeapon(WeaponId::Fists);
}
void Game::equipWeapon(WeaponId id){
 if(id==WeaponId::Fists){m_weaponEquipped=false;m_holster=1.f;return;}
 m_handsLowered=false;m_equippedWeapon=id;m_weaponEquipped=true;m_holster=m_player.ammo>0?0.f:1.f;
}
bool Game::applyEnemyDamage(Enemy&enemy,float amount,bool bloodDecal,bool closeHitStop,int level){
 if(level<0)level=m_level;
 if(!enemy.alive)return false;
 enemy.hp-=amount;enemy.painFlash=1;m_hitFlash=1;
 // A badly hurt stalker breaks off to hide before it comes back.
 if(enemy.hp>0&&enemy.kind==Enemy::Kind::Warden&&enemy.hp<enemy.maxHp*.4f&&enemy.stalkMode!=Enemy::StalkMode::Retreat){enemy.stalkMode=Enemy::StalkMode::Retreat;enemy.stalkTimer=2.4f;}
 if(enemy.hp>0)return false;
 enemy.hp=0;enemy.alive=false;enemy.deathTime=0;enemy.windup=0;enemy.strike=0;if(level==m_level)++m_kills;else ++m_chunks[level].kills;
 if(bloodDecal)m_bulletImpacts.push_back({enemy.pos,friendlyWorld(level).floorHeight(enemy.pos.x,enemy.pos.y)+.012f,{0,0},m_elapsed,level,true,int(enemy.kind)});
 if(closeHitStop)m_hitStopRemaining=.03f;
 if(level==m_level&&enemiesRemaining()==0)sound(Sound::Exit,.75f);
 return true;
}
void Game::storeChunk(){
 auto nav=std::move(m_chunks[m_level].nav);bool ready=m_chunks[m_level].navReady;
 m_chunks[m_level]={m_world,std::move(nav),ready,m_enemies,m_pickups,m_kills,true,m_clutter};
}
Game Game::chunkView(int level)const{
 perf::Scope frameProfile(perf::ChunkCopy);
 Game view=*this;if(level==m_level)return view;auto&chunk=m_chunks[level];view.m_level=level;view.m_world=chunk.world;view.m_enemies=chunk.enemies;view.m_pickups=chunk.pickups;view.m_kills=chunk.kills;
 view.m_clutter=chunk.clutter;view.m_heldClutter=-1;view.m_player.pos=m_player.pos+chunkOffset(m_level)-chunkOffset(level);return view;
}
const World& Game::worldAt(Vec2& local)const{
 // Convert through world space rather than assuming chunk IDs run north/south.
 // Campaign keeps its existing authored offsets; Ashfall can therefore form a
 // genuine 2D grid with the same collision queries.
 Vec2 global=local+chunkOffset(m_level);int other=m_level;
 for(int level=0;level<chunkCount();++level){auto origin=chunkOffset(level);
  if(global.x>=origin.x&&global.x<origin.x+World::Width&&global.y>=origin.y&&global.y<origin.y+World::Height){other=level;break;}
 }
 if(other==m_level)return m_world;
 local=global-chunkOffset(other);return m_chunks[other].world;
}
void Game::crossChunkBoundary(){
 Vec2 global=m_player.pos+chunkOffset(m_level);int next=m_level;
 for(int level=0;level<chunkCount();++level){auto origin=chunkOffset(level);
  if(global.x>=origin.x&&global.x<origin.x+World::Width&&global.y>=origin.y&&global.y<origin.y+World::Height){next=level;break;}
 }
 if(next==m_level)return;int previous=m_level;
 // Release a captive while their local coordinates still belong to this
 // chunk, before the seam checkpoint serializes the new player ownership.
 if(heldNpc()>=0)releaseNpc(true);
 auto shift=chunkOffset(m_level)-chunkOffset(next);bool carried=holdingClutter();Clutter held;if(carried){held=m_clutter[m_heldClutter];m_clutter.erase(m_clutter.begin()+m_heldClutter);}m_heldClutter=-1;
 // Outdoor pursuers standing at the seam hand off with the player. Without
 // this, changing the active chunk froze an alerted creature one metre behind
 // an invisible ownership boundary even though the terrain itself was seamless.
 std::vector<Enemy> followers;
 if(m_worldId==WorldId::Ashfall){
  for(auto it=m_enemies.begin();it!=m_enemies.end();){
   auto local=it->pos+shift;
   float outsideX=local.x<0?-local.x:local.x>=World::Width?local.x-World::Width:0;
   float outsideY=local.y<0?-local.y:local.y>=World::Height?local.y-World::Height:0;
   if(it->alive&&it->awareness>0&&std::max(outsideX,outsideY)<1.6f){
    auto enemy=*it;enemy.pos={std::clamp(local.x,.24f,23.76f),std::clamp(local.y,.24f,23.76f)};
    enemy.lastKnown+=shift;enemy.waypoint+=shift;enemy.home=enemy.pos;followers.push_back(enemy);it=m_enemies.erase(it);
   }else ++it;
  }
 }
 // Loose objects can cross any stitched edge. Transfer ownership only when the
 // object's world-space position belongs to the same destination as the player.
 std::vector<Clutter> following;
 for(auto it=m_clutter.begin();it!=m_clutter.end();){Vec2 objectGlobal=it->pos+chunkOffset(previous);auto origin=chunkOffset(next);
  if(objectGlobal.x>=origin.x&&objectGlobal.x<origin.x+World::Width&&objectGlobal.y>=origin.y&&objectGlobal.y<origin.y+World::Height){
   auto item=*it;item.pos=objectGlobal-origin;following.push_back(item);it=m_clutter.erase(it);
  }else ++it;}
 ensureChunk(next);storeChunk();auto&chunk=m_chunks[next];m_world=chunk.world;m_levelNav=std::move(chunk.nav);m_levelNavReady=chunk.navReady;chunk.nav={};chunk.navReady=false;m_enemies=chunk.enemies;m_pickups=chunk.pickups;m_clutter=chunk.clutter;m_kills=chunk.kills;m_level=next;m_player.pos+=shift;m_vrAimPosition.x+=shift.x;m_vrAimPosition.y+=shift.y;for(auto&hand:m_vrHandPosition){hand.x+=shift.x;hand.y+=shift.y;}for(auto&hand:m_vrPreviousFist){hand.x+=shift.x;hand.y+=shift.y;}
 if(carried){held.pos+=shift;m_heldClutter=int(m_clutter.size());m_clutter.push_back(held);}
 m_clutter.insert(m_clutter.end(),following.begin(),following.end());
 for(auto&enemy:followers){enemy.z=m_world.supportBelow(enemy.pos.x,enemy.pos.y,enemy.z+.25f);enemy.lastKnownZ=enemy.z;m_enemies.push_back(enemy);}
 for(auto&event:m_sounds)if(event.spatial||std::isfinite(event.z))event.position+=shift;
 m_activeLog=-1;m_logTime=0;m_pickupNoticeTime=0;
 if((m_worldId==WorldId::Campaign&&next>previous)||m_worldId==WorldId::Custom)saveCheckpoint();
}
void Game::loadLevel(int level,bool carry) {
    float health=m_player.health;int ammo=m_player.ammo,loaded=m_player.loaded;
    m_level=std::clamp(level,0,chunkCount()-1);m_world=makeWorld(m_level);m_levelNav={};m_levelNavReady=false;m_previousJump=false;m_previousUse=false;m_jumpBuffer=0;m_coyote=0;m_activeLog=-1;m_logTime=0;
    m_player = {};
    m_player.pos = m_world.definition().playerStart;
    m_player.angle = m_world.definition().spawnYaw;
    m_player.pitch = m_world.definition().spawnPitch;
    m_player.health = 100.0f;
    m_player.ammo = 72;
    m_player.loaded = 6;
    if(carry){m_player.health=health;m_player.ammo=ammo;m_player.loaded=loaded;}
    m_player.z = m_world.hasTerrain()?groundHeight(m_player.pos,float(World::TerrainMaxZ+1)):m_world.definition().spawnHeight;
    m_player.verticalVelocity = 0.0f;
    m_player.grounded = true;
    m_velocity = {};

    m_enemies.clear();
    for(const auto& spawn:m_world.creatureSpawns())spawnCreature(spawn);
    m_pickups.clear();
    for(const auto& spawn:m_world.pickupSpawns())m_pickups.push_back({spawn.position,spawn.kind,true,spawn.z});

    m_previousFire = false;
    m_previousReload = false;
    m_weaponKick = 0.0f; m_reloadTimer=0; m_shotCooldown = 0.0f;
    m_damageFlash = 0.0f;
    m_elapsed = 0.0f;
    m_kills = 0;
    m_won = false;
    m_hitFlash=0;
    m_weaponMotion={};m_sway={};m_swayVelocity={};m_elbowVelocity=0;m_shotAge=10;
    m_holster=m_player.ammo==0?1.f:0.f;m_punchAge=10;m_punchLeft=false;m_guarding=false;m_verticalSpring=m_verticalSpringVelocity=0;
    m_sounds.clear();m_stepDistance=0;m_stepVariant=0;
    m_pickupNotice.clear();m_pickupNoticeTime=0;
    seedClutter();
    if(m_world.hasLift()&&!m_hazmat.initialized)m_hazmat.seed(m_world);
}
void Game::sound(Sound sound,float gain,float pitch){
 bool foot=footstepSound(sound)||sound==Sound::Land;
 Point3 origin{m_player.pos.x,m_player.pos.y,m_player.z+(foot?.04f:m_player.eye)};
 if(sound==Sound::Shot&&m_vrAimValid)origin=m_vrAimPosition;
 m_sounds.push_back({sound,{origin.x,origin.y},gain,pitch,false,17,origin.z});
}
void Game::enemySound(const Enemy& enemy,int action,float gain,float pitch){int kind=enemy.kind==Enemy::Kind::Warden||enemy.kind==Enemy::Kind::Mutant?2:int(enemy.kind);m_sounds.push_back({Sound(int(Sound::SpiderCall)+kind*3+action),enemy.pos,gain,pitch*(enemy.kind==Enemy::Kind::Warden?.78f:1.f),true,17,(enemy.bodyBottom()+enemy.bodyTop())*.5f});}

int Game::hostilesRemaining(int level)const{
 const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;int n=int(std::count_if(enemies.begin(),enemies.end(),[](const Enemy&e){return e.alive;}));
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();if(npc.level==level&&track.ai.mode==ActorAiMode::Guard&&track.ai.armed&&track.health&&autonomousActor(track)&&!npcIncapacitated(i)&&npc.pose.clip!=4&&(!track.deadState||!state(track.deadState))&&guardHostileToPlayer(i))++n;}return n;
}
int Game::enemiesRemaining()const{return hostilesRemaining(m_level);}
const Enemy* Game::targetEnemy()const{
 const Enemy* target=nullptr;float nearest=18;
 Vec2 f{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(auto&e:m_enemies){auto d=e.pos-m_player.pos;float along=dot(d,f);float height=m_player.z+m_player.eye+along*std::tan(m_player.pitch/140.f);
  if(e.alive&&along>0&&along<nearest&&std::fabs(d.x*f.y-d.y*f.x)<.55f&&height>=e.bodyBottom()&&height<=e.bodyTop()&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,e.pos,height)){target=&e;nearest=along;}
 }return target;
}
bool Game::testCombat(){
 if(weaponCount()<1||weaponDef(WeaponId::Shotgun).cycle.tubeCapacity!=6||hitscanDamageAt(weaponDef(WeaponId::Shotgun).hitscan,3.f)!=34.f||kUnarmedMelee.damage!=28.f)return false;
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Wasp,Enemy::Kind::Brute}){
  auto encounter=validationScene(kind);encounter.m_player.pitch=-35;int shots=0;int tube=weaponDef(WeaponId::Shotgun).cycle.tubeCapacity;
  while(encounter.m_enemies[0].alive&&shots<10){encounter.shoot();++shots;if(encounter.m_player.loaded==0&&encounter.m_player.ammo>0)encounter.m_player.loaded=std::min(tube,encounter.m_player.ammo);}
  int expected=kind==Enemy::Kind::Huntsman?4:kind==Enemy::Kind::Wasp?3:9;
  if(shots!=expected||encounter.m_kills!=1)return false;
 }
 Game g;g.m_enemies.resize(1);g.m_player.pos={3.5f,4.5f};g.m_player.angle=0;g.m_player.pitch=-35;g.m_enemies[0].pos={5.5f,4.5f};
 auto scar=validationScene(Enemy::Kind::Huntsman);scar.m_enemies.clear();scar.m_player.pos={1.5f,1.5f};scar.m_player.angle=kPi;
 scar.shoot();if(scar.bulletImpacts().empty()||scar.bulletImpacts().size()>6)return false;
 auto barrel=barrelInspection(.06f);if(barrel.world().tile(12,2)!='.'||barrel.barrelExplosions().size()!=1||barrel.barrelExplosions()[0].level!=0)return false;
 g.shoot();if(!g.m_enemies[0].alive||g.m_enemies[0].hp>=110)return false;
 g.shoot();g.shoot();if(!g.m_enemies[0].alive)return false;g.shoot();
 if(g.m_enemies[0].alive||g.m_kills!=1||!g.m_enemies[0].visible())return false;
 for(int i=0;i<160;++i)g.update({},1.f/60.f);
 if(g.m_enemies[0].visible())return false;
 g.restart();g.m_enemies.resize(1);g.m_enemies[0].pos=g.m_player.pos+Vec2{.7f,0};
 g.update({},.05f);if(g.m_player.health!=100||g.m_enemies[0].windup<=0)return false;
 for(int i=0;i<40;++i)g.update({},1.f/60.f);
 if(g.m_player.health>=100)return false;
 auto reload=validationScene(Enemy::Kind::Huntsman);reload.m_player.ammo=8;reload.m_player.loaded=2;auto revision=reload.sessionRevision();auto position=reload.player().pos;
 InputState reloadKey{};reloadKey.reload=true;reload.update(reloadKey,.01f);
 if(reload.player().ammo!=8||reload.player().loaded!=2||reload.sessionRevision()!=revision)return false;
 reloadKey.reload=false;for(int i=0;i<80;++i)reload.update(reloadKey,.01f);
 if(reload.player().ammo!=8||reload.player().loaded!=6||lengthSq(reload.player().pos-position)>.0001f)return false;
 InputState scroll{};scroll.weaponScroll=-1;reload.update(scroll,.01f);if(reload.weaponEquipped())return false;
 scroll.weaponScroll=1;reload.update(scroll,.01f);if(!reload.weaponEquipped())return false;
 auto closeShot=validationScene(Enemy::Kind::Huntsman);closeShot.m_player.pos={4.5f,4.5f};closeShot.m_player.angle=0;closeShot.m_player.pitch=-65;closeShot.m_enemies[0].pos={5.3f,4.5f};closeShot.m_enemies[0].hp=20;closeShot.shoot();
 if(closeShot.m_enemies[0].alive)return false;
 // Repeated hits on a corpse must not count another kill or restart death.
 auto kills=closeShot.m_kills;auto decals=closeShot.m_bulletImpacts.size();
 if(closeShot.applyEnemyDamage(closeShot.m_enemies[0],150.f)||closeShot.m_kills!=kills||closeShot.m_bulletImpacts.size()!=decals)return false;
 closeShot.update({},1.f/60.f);closeShot.update({},1.f/60.f);if(closeShot.m_enemies[0].deathTime!=0)return false;
 closeShot.update({},1.f/60.f);if(closeShot.m_enemies[0].deathTime<=0)return false;
 auto closePunch=validationScene(Enemy::Kind::Huntsman);closePunch.m_player.pos={4.5f,4.5f};closePunch.m_player.angle=0;closePunch.m_player.pitch=-65;closePunch.m_enemies[0].pos={5.3f,4.5f};closePunch.m_enemies[0].hp=20;closePunch.punchImpact();
 if(closePunch.m_enemies[0].alive)return false;
 closePunch.update({},1.f/60.f);closePunch.update({},1.f/60.f);if(closePunch.m_enemies[0].deathTime!=0)return false;
 closePunch.update({},1.f/60.f);return closePunch.m_enemies[0].deathTime>0;
}
Game Game::validationScene(Enemy::Kind kind,float deathTime,float windup){
 Game g;g.m_enemies.resize(1);auto&e=g.m_enemies[0];e.kind=kind;e.pos={6.2f,4.5f};e.heading=kPi;e.moving=true;e.gait=2.f;e.windup=windup;e.hp=e.maxHp=kind==Enemy::Kind::Brute?280.f:kind==Enemy::Kind::Wasp?85.f:110.f;
 e.alive=deathTime<0;e.deathTime=std::max(0.f,deathTime);if(kind==Enemy::Kind::Warden)e.hp=e.maxHp=320;if(kind==Enemy::Kind::Mutant)e.hp=e.maxHp=180;g.m_player.angle=0;return g;
}
Game Game::mapInspection(Vec2 position,float angle,float pitch,int level,bool openDoors,float height,bool sceneryOnly,WorldId id){Game game(id);game.loadLevel(level,false);game.m_player.pos=position;game.m_player.z=height==-999?(game.m_world.hasTerrain()?game.groundHeight(position,float(World::TerrainMaxZ+1)):game.m_world.floorHeight(position.x,position.y)):height;game.m_player.angle=angle;game.m_player.pitch=pitch;
 if(sceneryOnly){game.m_enemies.clear();for(auto&chunk:game.m_chunks)chunk.enemies.clear();}
 if(openDoors){for(auto&chunk:game.m_chunks){for(int i=0;i<int(chunk.world.doors().size());++i)chunk.world.openDoor(i);chunk.world.updateDoors(2);}for(int i=0;i<int(game.m_world.doors().size());++i)game.m_world.openDoor(i);game.m_world.updateDoors(2);}game.updateStreaming(0);return game;}
void Game::detonateBarrel(int level,int tx,int ty){
 auto&world=friendlyWorld(level);if(world.tile(tx,ty)!='B')return;
 const float floor=world.floorHeight(tx+.5f,ty+.5f);const Vec2 at{tx+.5f,ty+.5f};world.destroyTile(tx,ty);
 m_barrelExplosions.push_back({at,floor+.55f,m_elapsed,level});if(m_barrelExplosions.size()>16)m_barrelExplosions.erase(m_barrelExplosions.begin());
 auto shift=chunkOffset(m_level)-chunkOffset(level);Vec2 here=at-shift;
 float near=std::clamp(1.f-length(here-m_player.pos)/9.f,0.f,1.f);
 m_verticalSpringVelocity-=.6f+1.2f*near;m_damageFlash=std::max(m_damageFlash,.25f+.55f*near);
 m_sounds.push_back({Sound::LiftCrash,here,1.f,.8f,true,17,floor+.6f});
 // A scorch on the floor, burning scrap in the air.
 m_bulletImpacts.push_back({at,floor+.012f,{0,0},m_elapsed,level,false,-2,1.f});if(m_bulletImpacts.size()>96)m_bulletImpacts.erase(m_bulletImpacts.begin());
 auto&clutter=level==m_level?m_clutter:m_chunks[level].clutter;
 for(int i=0;i<5;++i){Clutter c;c.kind=3;c.pos=at;c.z=floor+.5f;c.yaw=float(i)*1.2566f+.3f;c.pitch=(i%2?-.3f:.3f);c.velocity={std::cos(c.yaw)*(3.5f+float(i)*.4f),std::sin(c.yaw)*(3.5f+float(i)*.4f)};c.vz=3.5f+float(i%3);c.spin=(i%2?1.f:-1.f)*5;c.pitchSpeed=(i%2?1.f:-1.f)*4;c.rollSpeed=3;clutter.push_back(c);}
 auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
 for(auto&e:enemies)if(e.alive&&length(e.pos-at)<kUnarmedMelee.barrelRadius)applyEnemyDamage(e,kUnarmedMelee.barrelDamage,false,false,level);
 // Barrels within reach go up in turn.
 int fuse=0;
 for(int y=ty-3;y<=ty+3;++y)for(int x=tx-3;x<=tx+3;++x){if((x==tx&&y==ty)||world.tile(x,y)!='B'||length(Vec2{x+.5f,y+.5f}-at)>2.6f)continue;
  bool queued=false;for(const auto&q:m_pendingBarrels)queued|=q.level==level&&int(std::floor(q.pos.x))==x&&int(std::floor(q.pos.y))==y;if(queued)continue;
  m_pendingBarrels.push_back({{x+.5f,y+.5f},floor,m_elapsed+.18f+.09f*float(fuse++),level});}
}
Game Game::barrelInspection(float age){auto game=mapInspection({12.5f,4.5f},-kPi*.5f,-35,0,false,-999,true);game.shoot();game.m_elapsed=std::max(0.f,age);return game;}

bool Game::lineOfSight(const Vec2& a,const Vec2& b)const{return m_world.rayClear(a,m_world.floorHeight(a.x,a.y)+.75f,b,m_world.floorHeight(b.x,b.y)+.75f);}

void Game::shoot(){
 if(dead()||m_won)return;
 const auto&gun=equippedWeaponDef();
 if(m_player.ammo<=0||m_player.loaded<=0){sound(gun.cycle.emptySound,.55f);return;}
 --m_player.ammo;--m_player.loaded;m_weaponKick=1;m_shotAge=0;
 float pitch=m_player.pitch/140.f;Point3 origin{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye},direction{std::cos(m_player.angle)*std::cos(pitch),std::sin(m_player.angle)*std::cos(pitch),std::sin(pitch)};
 if(m_vrAimValid){origin=m_vrAimPosition;direction=m_vrAimDirection;}
 float size=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);if(size>.0001f)direction=direction*(1/size);
 m_sounds.push_back({gun.cycle.fireSound,{origin.x,origin.y},.95f,1,false,17,origin.z});
 auto hit=traceShot(origin,direction,gun.hitscan.maxRange,-1,false,&gun.hitscan);
 resolveShot(hit,origin,direction,gun.hitscan,true);
}
Game Game::stalkerInspection(int clip,float phase,int view,Enemy::Kind kind){
 auto game=mapInspection({18.6f,18.5f},0,clip==4?-35.f:0.f,3,true,-9,false);
 if(view){game.m_player.pos={21.5f,21.2f};game.m_player.angle=-kPi*.5f;}
 game.m_enemies.resize(1);auto& e=game.m_enemies[0];e={};e.kind=kind;e.pos={21.5f,18.5f};e.z=-9;e.home=e.pos;e.heading=kPi;e.hp=e.maxHp=kind==Enemy::Kind::Mutant?180.f:320.f;
 game.m_elapsed=phase*2.5f-e.home.x*.25f;
 if(clip==1){e.moving=true;e.gait=phase*2*kPi;}
 if(clip==2){if(phase<.4f)e.windup=(1-phase/.4f)*.55f;else e.strike=1-(phase-.4f)/.6f;}
 if(clip==3)e.painFlash=1-phase;
 if(clip==4){e.alive=false;e.deathTime=phase*1.15f;}
 return game;
}
Game Game::terminalInspection(int level,int terminal){
 Game game;game.loadLevel(level,false);const auto&t=game.world().terminals().at(size_t(terminal));
 game.m_player.pos=t.position+Vec2{0,-1.5f};game.m_player.z=game.world().floorHeight(t.position.x,t.position.y)+t.z;game.m_player.angle=kPi*.5f;
 game.m_activeLog=terminal;game.m_logTime=30;game.m_enemies.clear();return game;
}
void Game::reloadWeapon(){
 const auto&cycle=equippedWeaponDef().cycle;
 if(dead()||m_won||holdingClutter()||!m_weaponEquipped||m_reloadTimer>0||m_player.loaded>=cycle.tubeCapacity||m_player.ammo<=m_player.loaded)return;
 m_reloadTimer=cycle.reloadTime;m_shotCooldown=std::max(m_shotCooldown,m_reloadTimer);m_weaponKick=cycle.reloadKick;
}

void Game::updatePickups() {
    for (auto& p : m_pickups) {
        if (!p.active || lengthSq(p.pos - m_player.pos) > 0.45f * 0.45f||std::fabs(m_player.z-pickupHeight(p))>.5f) continue;
        if (p.kind == Pickup::Kind::Health) {
            if (m_player.health >= 100.0f) {
                if(m_medkits>=5)continue;
                ++m_medkits;p.active=false;m_pickupNotice="STORED FIRST AID / I TO USE";m_pickupNoticeTime=2;sound(Sound::Pickup,.6f);continue;
            }
            float gained=std::min(35.f,100.f-m_player.health);m_player.health+=gained;
            m_pickupNotice="RECOVERED "+std::to_string(int(gained))+" HEALTH";
        } else {
            int shells=equippedWeaponDef().cycle.pickupAmount;m_player.ammo+=shells;
            m_pickupNotice="COLLECTED "+std::to_string(shells)+" SHELLS";
        }
        p.active = false;
        m_pickupNoticeTime=2.f;
        sound(Sound::Pickup,.6f,p.kind==Pickup::Kind::Health?1.15f:1.f);
    }
}
const Pickup* Game::nearbyPickup()const{
 const Pickup* nearest=nullptr;float distance=2.5f;Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(auto&p:m_pickups){auto delta=p.pos-m_player.pos;float d=length(delta);float z=pickupHeight(p);if(p.active&&std::fabs(m_player.z-z)<.8f&&d<distance&&dot(delta,forward)>d*.6f&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,p.pos,z+.2f)){nearest=&p;distance=d;}}
 return nearest;
}
bool Game::testPickups(){
 auto g=validationScene(Enemy::Kind::Huntsman,3);g.m_pickups={{{3.5f,4.5f},Pickup::Kind::Health,true}};
 g.updatePickups();if(g.m_pickups[0].active||g.m_medkits!=1)return false;g.m_pickups[0].active=true;
 g.m_player.health=85;g.updatePickups();if(g.m_player.health!=100||g.m_pickups[0].active||g.m_pickupNotice!="RECOVERED 15 HEALTH")return false;
 g.m_pickups={{{3.5f,4.5f},Pickup::Kind::Ammo,true}};int ammo=g.m_player.ammo;
 g.m_player.z=1;g.updatePickups();if(g.m_player.ammo!=ammo)return false;
 g.m_player.z=0;g.updatePickups();g.updatePickups();return g.m_player.ammo==ammo+16&&!g.m_pickups[0].active;
}

void Game::update(const InputState& input, float dt) {
    m_sounds.clear();
    if(m_pendingMainCampaign)startMainCampaign();
    if(m_hitStopRemaining>0.f){m_hitStopRemaining=std::max(0.f,m_hitStopRemaining-std::clamp(dt,0.f,.05f));return;}
    bool flashlightPressed=input.flashlight&&!m_previousFlashlight;m_previousFlashlight=input.flashlight;
    if(m_titleScreen){updateTitle(input);return;}
    bool consolePressed=input.console&&!m_previousConsole;m_previousConsole=input.console;
    if(consolePressed){m_consoleOpen=!m_consoleOpen;m_suppressFire=true;return;}
    if(m_consoleOpen){updateConsole(input);return;}
    bool escapePressed=input.escape&&!m_previousEscape;m_previousEscape=input.escape;
    bool inventoryPressed=input.inventory&&!m_previousInventory;m_previousInventory=input.inventory;
    if(m_inventoryOpen&&escapePressed){m_inventoryOpen=false;m_suppressFire=true;return;}
    if(escapePressed){
     if(m_paused&&m_menuFromTitle){
      if(m_menuPage==MenuPage::ConfirmLoad){m_menuPage=MenuPage::Load;m_menuSelection=m_pendingSlot;m_menuMessage.clear();}
      else showTitleScreen();
     }else if(m_paused&&m_menuPage!=MenuPage::Settings){
      if(m_menuPage==MenuPage::Overwrite||m_menuPage==MenuPage::ConfirmLoad){m_menuPage=m_menuPage==MenuPage::Overwrite?MenuPage::Save:MenuPage::Load;m_menuSelection=m_pendingSlot;}
      else {m_menuSelection=m_menuPage==MenuPage::Save?7:m_menuPage==MenuPage::Load?8:6;m_menuPage=MenuPage::Settings;}
      m_menuMessage.clear();
     }else {m_paused=!m_paused;m_menuPage=MenuPage::Settings;m_menuSelection=0;m_menuMessage.clear();}
     m_dragSlider=-1;m_menuPrevious=input;m_suppressFire=true;return;
    }
    if(m_paused){updateMenu(input);return;}
    if(inventoryPressed){m_inventoryOpen=!m_inventoryOpen;m_suppressFire=true;m_inventoryClick=input.fire;m_inventoryUse=input.use;return;}
    if(m_inventoryOpen){updateInventory(input);return;}
    if(flashlightPressed){
     if(hasFlashlight()){bool on=!flashlightOn();setState(stateId("flashlight_on"),on?1:0);m_pickupNotice=on?"FLASHLIGHT / ON":"FLASHLIGHT / OFF";}
     else m_pickupNotice="NO FLASHLIGHT";
     m_pickupNoticeTime=1.25f;
    }
    m_pickupNoticeTime=std::max(0.f,m_pickupNoticeTime-std::min(dt,.05f));
    if(!input.fire)m_suppressFire=false;
    dt = std::min(dt, 0.05f);
    if(input.mute&&!m_previousMute)m_audioMuted=!m_audioMuted;
    if(input.music&&!m_previousMusic)m_musicEnabled=!m_musicEnabled;
    m_previousMute=input.mute;m_previousMusic=input.music;
    if (input.reload && !m_previousReload) reloadWeapon();
    m_previousReload = input.reload;
    if(input.weaponScroll){if(input.weaponScroll>0)equipWeapon(m_equippedWeapon==WeaponId::Fists?WeaponId::Shotgun:m_equippedWeapon);else equipWeapon(WeaponId::Fists);m_suppressFire=true;}
    for(auto&e:m_enemies)if(!e.alive)e.deathTime+=dt;
    m_hitFlash=std::max(0.f,m_hitFlash-dt*5.f);

    if (!dead() && !m_won) {
        m_elapsed += dt;
        m_player.angle = input.vrTracked?input.vrYaw:wrapAngle(m_player.angle + input.mouseDx * 0.0022f*m_settings.sensitivity);
        m_player.pitch = input.vrTracked?input.vrPitch:clamp(m_player.pitch - input.mouseDy * 0.308f*m_settings.sensitivity*(m_settings.invertMouse?-1.f:1.f), -210.0f, 210.0f);

        updateMechanisms(dt);
        updateLift(dt);
        syncReactorComplex();
        auto movement=input;if(heldNpc()>=0||takedownActive()){movement.sprint=false;movement.jump=false;movement.crouch=false;}
        updateMovement(movement,dt);
        updateStealth(input,dt);
        if(m_world.hasLift()){
            m_hazmatPushCooldown=std::max(0.f,m_hazmatPushCooldown-dt);
            if(m_hazmat.initialized&&length(m_velocity)>.5f&&m_hazmatPushCooldown<=0){
                int nearest=-1;float nearestDistance=.4f*.4f;
                for(int joint=0;joint<Ragdoll::Count;++joint){auto p=m_hazmat.p[joint];float distance=lengthSq(Vec2{p.x,p.y}-m_player.pos);
                    if(distance<nearestDistance&&p.z>m_player.z&&p.z<m_player.z+.7f){nearest=joint;nearestDistance=distance;}}
                if(nearest>=0){m_hazmat.impulse(nearest,{m_velocity.x*.12f,m_velocity.y*.12f,.08f});m_hazmatPushCooldown=.10f;}
            }
            m_hazmat.update(m_world,dt);
        }
        crossChunkBoundary();
        updateScripts(dt);
        updateHazards(dt);
        for(size_t i=0;i<m_pendingBarrels.size();){if(m_elapsed>=m_pendingBarrels[i].time){auto b=m_pendingBarrels[i];m_pendingBarrels.erase(m_pendingBarrels.begin()+long(i));detonateBarrel(b.level,int(std::floor(b.pos.x)),int(std::floor(b.pos.y)));}else ++i;}
        updateInteraction(input,dt);
        updateStreaming(dt);
        // Resident NPCs can operate ordinary doors without the player entering
        // their chunk. Transfer leaves are mirrored by the streamer instead.
        for(int level=0;level<chunkCount();++level)if(level!=m_level&&chunkResident(level))m_chunks[level].world.updateDoors(dt,false);
        bool carryingAtStart=heldNpc()>=0||(holdingClutter()&&(!input.vrTracked||m_vrCarryHand==1));updateClutter(input,dt);

        const auto&cycle=equippedWeaponDef().cycle;const auto&melee=kUnarmedMelee;
        float oldReload=m_reloadTimer;m_reloadTimer=std::max(0.f,m_reloadTimer-dt);
        if(oldReload>0&&m_reloadTimer==0){m_player.loaded=std::min(cycle.tubeCapacity,m_player.ammo);m_shotAge=0;}
        m_shotCooldown = std::max(0.f,m_shotCooldown-dt);
        m_guarding=heldNpc()<0&&!takedownActive()&&input.guard&&unarmed();if(m_guarding)m_punchAge=10;
        float oldPunch=m_punchAge;m_punchAge+=dt;if(!input.vrTracked&&!takedownActive()&&oldPunch<melee.impactAt&&m_punchAge>=melee.impactAt)punchImpact();if(input.vrTracked)updateVrMelee(input,dt);
        float lower=(!m_weaponEquipped||m_player.ammo<=0)&&m_shotAge>cycle.autoHolsterAfter?1.f:0.f;
        m_holster+=std::clamp(lower-m_holster,-dt*2.6f,dt*2.6f);
        if (input.fire&&!m_suppressFire&&!carryingAtStart && m_shotCooldown<=0.f) {
            if(m_player.ammo>0&&m_player.loaded>0&&m_holster<cycle.raiseHolster){shoot();m_shotCooldown=cycle.fireCooldown;}
            else if(!input.vrTracked&&unarmed()&&!m_guarding){m_handsLowered=false;m_punchAge=0;m_punchLeft=!m_punchLeft;m_shotCooldown=melee.swingCooldown;sound(Sound::PunchSwing,.55f,m_punchLeft?.95f:1.05f);}
        }
        // Guard perception consumes this frame's movement, throws and shots.
        updateFriendlyActors(dt,true);
        updateTakedown(dt);
        updateEngineerDrones(dt);
        updateGuardCombat(dt);
        for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(m_friendlyActors[actor].track().ai.mode==ActorAiMode::Guard)finishGuardTask(actor);
        updateTacticalGrenades(dt);
        updateEnemies(dt);
        updateNpcVoices(dt);
        updatePickups();

        // Campaign completion is explicit through scripted content. Reaching the
        // numerically last compiled chunk is never an implicit ending.
    }

    if(dead()&&heldNpc()>=0)releaseNpc(true);
    m_previousFire = input.fire;
    m_weaponKick = std::max(0.0f, m_weaponKick - dt * 3.0f);
    m_damageFlash = std::max(0.0f, m_damageFlash - dt * 3.0f);
    updateWeaponMotion(input,dt);
}
void Game::updateWeaponMotion(const InputState& input,float dt){
 const auto&cycle=equippedWeaponDef().cycle;
 if(m_shotAge<cycle.pumpSoundAt&&m_shotAge+dt>=cycle.pumpSoundAt)sound(cycle.pumpSound,.65f);
 m_shotAge+=dt;
 float yawRate=input.mouseDx*.0022f*m_settings.sensitivity/std::max(dt,.001f);
 float pitchRate=input.mouseDy*.308f/140.f*m_settings.sensitivity*(m_settings.invertMouse?-1.f:1.f)/std::max(dt,.001f);
 Vec2 target{std::clamp(-yawRate*.042f,-.19f,.19f),std::clamp(pitchRate*.035f,-.15f,.15f)};
 // Damped springs are integrated with bounded substeps, independent of render rate.
 int steps=std::max(1,int(std::ceil(dt/.008f)));float step=dt/steps;
 for(int i=0;i<steps;++i){
  m_swayVelocity+=(target-m_sway)* (76.f*step);m_swayVelocity=m_swayVelocity*std::exp(-10.f*step);m_sway+=m_swayVelocity*step;
  m_verticalSpringVelocity+=(-m_verticalSpring*80.f-m_verticalSpringVelocity*10.f)*step;m_verticalSpring+=m_verticalSpringVelocity*step;
  float drive=m_weaponKick*.7f+m_sway.x*3.f-m_player.verticalVelocity*.025f;
  m_elbowVelocity+=(drive-m_weaponMotion.elbow)*45.f*step;
  m_elbowVelocity*=std::exp(-5.5f*step);m_weaponMotion.elbow+=m_elbowVelocity*step;
 }
 float speed=length(m_velocity),bobCycle=std::sin(m_elapsed*9.f);
 m_weaponMotion.yaw=m_sway.x;
 m_weaponMotion.pitch=m_sway.y+m_weaponKick*.065f;
 m_weaponMotion.bob=bobCycle*std::min(speed,4.f)*.0035f+m_verticalSpring;
 m_weaponMotion.back=m_weaponKick*.095f+std::fabs(m_sway.x)*.16f;
 m_weaponMotion.roll=-m_sway.x*.55f;
 float boltSpan=std::max(.001f,cycle.boltEnd-cycle.boltStart);
 m_weaponMotion.bolt=m_shotAge>cycle.boltStart&&m_shotAge<cycle.boltEnd?std::sin((m_shotAge-cycle.boltStart)/boltSpan*kPi)*.06f:0;
}
void Game::punchImpact(){
 const auto&melee=kUnarmedMelee;Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};Enemy* hitEnemy=nullptr;int hitNpc=-1;float nearest=melee.range;
 for(auto&e:m_enemies){auto delta=e.pos-m_player.pos;float range=length(delta);
  float height=m_player.z+m_player.eye+range*std::tan(m_player.pitch/140.f);
  if(e.alive&&range<nearest&&dot(normalized(delta),forward)>melee.facingDot&&height>=e.bodyBottom()-.1f&&height<=e.bodyTop()&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,e.pos,height)){hitEnemy=&e;hitNpc=-1;nearest=range;}
 }
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();
  if(npc.level!=m_level||!track.health||npc.pose.clip==4||(track.deadState&&state(track.deadState))||(track.ai.enableState&&!state(track.ai.enableState)))continue;
  auto delta=npc.pose.position-m_player.pos;float range=length(delta);float body=friendlyHullHeight(i,track);
  float height=m_player.z+m_player.eye+range*std::tan(m_player.pitch/140.f);
  if(range<nearest&&dot(normalized(delta),forward)>melee.facingDot&&height>=npc.pose.z-.1f&&height<=npc.pose.z+body&&m_world.rayClear(m_player.pos,m_player.z+m_player.eye,npc.pose.position,height)){hitNpc=int(i);hitEnemy=nullptr;nearest=range;}
 }
 if(hitEnemy)applyPunchHit(*hitEnemy);else if(hitNpc>=0)applyNpcPunchHit(size_t(hitNpc));
}
void Game::applyPunchHit(Enemy&enemy){
 sound(Sound::PunchHit,.65f);m_verticalSpringVelocity+=.25f;
 bool killed=applyEnemyDamage(enemy,kUnarmedMelee.damage,true,true);
 enemySound(enemy,killed?2:0,killed?.85f:.45f,killed?1.f:1.15f);
}
void Game::applyNpcPunchHit(size_t actor){
 if(actor>=m_friendlyActors.size()||npcIncapacitated(actor))return;const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
 sound(Sound::PunchHit,.65f);m_verticalSpringVelocity+=.25f;
 bool rearTakedown=canTakedown(actor);
 float damage=rearTakedown?float(track.health):kUnarmedMelee.damage;
 damageFriendly(actor,damage,m_player.pos,{ShotKind::Player,m_level,0});
 if(rearTakedown)m_hitStopRemaining=std::max(m_hitStopRemaining,.06f);
}
void Game::updateVrMelee(const InputState&input,float dt){
 if(heldNpc()>=0)return;
 const auto&melee=kUnarmedMelee;
 for(int hand=0;hand<2;++hand){m_vrFistCooldown[hand]=std::max(0.f,m_vrFistCooldown[hand]-dt);auto end=m_vrHandPosition[hand],start=m_vrPreviousFist[hand];bool tracked=m_vrHandValid[hand],continuous=m_vrMeleeTracked[hand];m_vrPreviousFist[hand]=end;m_vrMeleeTracked[hand]=tracked;
  if(!tracked||!continuous||!input.vrGrip[hand]||m_vrFistCooldown[hand]>0||(holdingClutter()&&m_vrCarryHand==hand)||(hand==1&&m_weaponEquipped&&!unarmed())||(hand==0&&m_vrSupportGrip))continue;
  auto sweep=end-start;float distance=std::sqrt(sweep.x*sweep.x+sweep.y*sweep.y+sweep.z*sweep.z);auto velocity=m_vrHandVelocity[hand];float speed=std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z);
  if(distance<.008f||distance>.65f||std::max(speed,distance/std::max(.001f,dt))<melee.vrMinSpeed)continue;
  if(!m_world.rayClear({start.x,start.y},start.z,{end.x,end.y},end.z,true,true,true))continue;
  Enemy* hitEnemy=nullptr;int hitNpc=-1;float nearest=2.f;
  for(auto&e:m_enemies)if(e.alive){float horizontal=sweep.x*sweep.x+sweep.y*sweep.y;float t=horizontal>.000001f?std::clamp(((e.pos.x-start.x)*sweep.x+(e.pos.y-start.y)*sweep.y)/horizontal,0.f,1.f):1.f;auto contact=start+sweep*t;
   if(lengthSq(Vec2{contact.x-e.pos.x,contact.y-e.pos.y})>.38f*.38f||contact.z<e.bodyBottom()-.10f||contact.z>e.bodyTop()+.10f||t>=nearest)continue;
   if((e.pos.x-start.x)*sweep.x+(e.pos.y-start.y)*sweep.y<=0||!m_world.rayClear(m_player.pos,m_player.z+m_player.eye,{contact.x,contact.y},contact.z,true,true,true))continue;
   hitEnemy=&e;hitNpc=-1;nearest=t;
  }
  for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();
   if(npc.level!=m_level||!track.health||npc.pose.clip==4||(track.deadState&&state(track.deadState))||(track.ai.enableState&&!state(track.ai.enableState)))continue;
   float body=friendlyHullHeight(i,track);float horizontal=sweep.x*sweep.x+sweep.y*sweep.y;float t=horizontal>.000001f?std::clamp(((npc.pose.position.x-start.x)*sweep.x+(npc.pose.position.y-start.y)*sweep.y)/horizontal,0.f,1.f):1.f;auto contact=start+sweep*t;
   if(lengthSq(Vec2{contact.x-npc.pose.position.x,contact.y-npc.pose.position.y})>.38f*.38f||contact.z<npc.pose.z-.10f||contact.z>npc.pose.z+body+.10f||t>=nearest)continue;
   if((npc.pose.position.x-start.x)*sweep.x+(npc.pose.position.y-start.y)*sweep.y<=0||!m_world.rayClear(m_player.pos,m_player.z+m_player.eye,{contact.x,contact.y},contact.z,true,true,true))continue;
   hitNpc=int(i);hitEnemy=nullptr;nearest=t;
  }
  if(hitEnemy){applyPunchHit(*hitEnemy);m_vrFistCooldown[hand]=melee.vrCooldown;m_punchLeft=hand==0;sound(Sound::PunchSwing,.35f);}
  else if(hitNpc>=0){applyNpcPunchHit(size_t(hitNpc));m_vrFistCooldown[hand]=melee.vrCooldown;m_punchLeft=hand==0;sound(Sound::PunchSwing,.35f);}
 }
}
__declspec(noinline) bool Game::testVrMelee(){
 auto g=validationScene(Enemy::Kind::Huntsman);g.m_player.pos={4.5f,4.5f};g.m_enemies[0].pos={5.3f,4.5f};g.m_enemies[0].hp=56;InputState input{};input.vrTracked=true;input.vrGrip[0]=true;
 g.setVrHand(0,{4.6f,4.5f,.7f},{1,0,0},true);g.updateVrMelee(input,.05f);if(g.m_enemies[0].hp!=56)return false;
 g.setVrHand(0,{5.1f,4.5f,.7f},{1,0,0},true,{3,0,0});g.updateVrMelee(input,.05f);if(g.m_enemies[0].hp!=28)return false;
 g.setVrHand(0,{5.2f,4.5f,.7f},{1,0,0},true,{3,0,0});g.updateVrMelee(input,.01f);if(g.m_enemies[0].hp!=28)return false;
 for(int i=0;i<50;++i)g.updateVrMelee(input,.01f);if(g.m_enemies[0].hp!=28)return false;
 g.setVrHand(0,{4.6f,4.5f,.7f},{1,0,0},true);input.vrGrip[0]=false;g.updateVrMelee(input,.05f);g.setVrHand(0,{5.1f,4.5f,.7f},{1,0,0},true,{3,0,0});g.updateVrMelee(input,.05f);if(g.m_enemies[0].hp!=28)return false;
 g.setVrHand(0,{4.6f,4.5f,.7f},{1,0,0},true);g.updateVrMelee(input,.05f);input.vrGrip[0]=true;g.setVrHand(0,{5.1f,4.5f,.7f},{1,0,0},true,{3,0,0});g.updateVrMelee(input,.05f);if(g.m_enemies[0].alive||g.kills()!=1)return false;
 auto wall=validationScene(Enemy::Kind::Huntsman);wall.m_player.pos={4.5f,7.95f};wall.m_enemies[0].pos={4.5f,9.1f};wall.setVrHand(0,{4.5f,8.6f,.7f},{0,1,0},true);wall.updateVrMelee(input,.05f);auto hp=wall.m_enemies[0].hp;wall.setVrHand(0,{4.5f,9.1f,.7f},{0,1,0},true,{0,3,0});wall.updateVrMelee(input,.05f);if(wall.m_enemies[0].hp!=hp)return false;
 auto support=validationScene(Enemy::Kind::Huntsman);support.m_player.pos={4.5f,4.5f};support.m_enemies[0].pos={5.3f,4.5f};support.m_enemies[0].hp=56;
 support.setVrAim({4.5f,4.5f,.7f},{1,0,0},true,true);
 support.setVrHand(0,{4.6f,4.5f,.7f},{1,0,0},true);support.updateVrMelee(input,.05f);
 support.setVrHand(0,{5.1f,4.5f,.7f},{1,0,0},true,{3,0,0});support.updateVrMelee(input,.05f);if(support.m_enemies[0].hp!=56)return false;
 support.setVrAim({4.5f,4.5f,.7f},{1,0,0},true,false);
 support.setVrHand(0,{4.6f,4.5f,.7f},{1,0,0},true);support.updateVrMelee(input,.05f);
 support.setVrHand(0,{5.1f,4.5f,.7f},{1,0,0},true,{3,0,0});support.updateVrMelee(input,.05f);if(support.m_enemies[0].hp!=28)return false;
 std::ofstream("vr-melee-test.txt")<<"PASS: gripped swept fist contact, open-hand rejection, cooldown, stationary rejection, lethal hit and wall occlusion\n";return true;
}
void Game::receiveDamage(float amount,Vec2 source,bool blockable){
 const auto&melee=kUnarmedMelee;Vec2 facing{std::cos(m_player.angle),std::sin(m_player.angle)};
 bool blocked=blockable&&m_guarding&&unarmed()&&dot(normalized(source-m_player.pos),facing)>melee.guardFrontDot;
 if(blocked){amount*=melee.guardScale;m_verticalSpringVelocity-=.45f;sound(Sound::PunchHit,.35f,.75f);}
 else sound(Sound::Hurt,.7f,.92f);
 if(!m_god)m_player.health=std::max(0.f,m_player.health-amount);m_damageFlash=blocked?.25f:1.f;
}
Game Game::weaponInspection(int mode,float age){auto game=validationScene(Enemy::Kind::Huntsman,3);
 if(mode==1||mode==3){game.m_player.ammo=0;game.m_holster=1;game.m_punchAge=age;game.m_punchLeft=false;game.m_guarding=mode==3;}
 else if(mode==2){game.m_shotAge=age;game.m_weaponKick=std::max(0.f,1-age*3.f);}
 return game;
}
bool Game::testUnarmed(){
 auto game=validationScene(Enemy::Kind::Huntsman);game.m_player.pitch=-65;game.m_player.ammo=1;game.m_enemies[0].pos=game.m_player.pos+Vec2{.95f,0};
 InputState input{};input.fire=true;game.update(input,.01f);if(game.player().ammo!=0)return false;
 input.fire=false;for(int i=0;i<110;++i)game.update(input,1.f/120.f);if(!game.unarmed())return false;
 float hp=game.m_enemies[0].hp;input.fire=true;game.update(input,.01f);input.fire=false;
 for(int i=0;i<30;++i)game.update(input,1.f/120.f);if(game.m_enemies[0].hp!=hp-28||game.player().ammo!=0)return false;
 auto blocked=game;blocked.m_player.pos={4.5f,7.95f};blocked.m_player.angle=kPi*.5f;blocked.m_enemies[0].pos={4.5f,9.1f};hp=blocked.m_enemies[0].hp;blocked.punchImpact();if(blocked.m_enemies[0].hp!=hp)return false;
 game.m_pickups={{{game.player().pos},Pickup::Kind::Ammo,true}};game.updatePickups();for(int i=0;i<60;++i)game.update({},1.f/120.f);
 if(game.holster()>.01f||game.player().ammo!=16)return false;
 auto guard=weaponInspection(3,10);guard.receiveDamage(20,guard.player().pos+Vec2{1,0});if(std::fabs(guard.player().health-94)>.01f)return false;
 guard.receiveDamage(20,guard.player().pos-Vec2{1,0});if(std::fabs(guard.player().health-74)>.01f)return false;
 InputState protect{};protect.guard=true;protect.fire=true;guard.update(protect,.02f);if(guard.punchAge()<1)return false;
 std::ofstream("unarmed-test.txt")<<"Last shell lowers weapon, timed punch deals damage, walls block punches, ammo pickup restores shotgun, guard reduces frontal damage by 70 percent, rear damage stays full and guard prevents punching: PASS\n";return true;
}
bool Game::testWeaponMotion(){
 auto run=[](int rate){auto g=validationScene(Enemy::Kind::Huntsman,3);
  for(int i=0;i<rate*5;++i){InputState input{};input.fire=i==0;input.mouseDx=i<rate/2?300.f/rate:0;g.update(input,1.f/rate);}
  return g.weaponMotion();
 };
 auto a=run(60),b=run(120);
 auto turn=[](int rate){auto g=validationScene(Enemy::Kind::Huntsman,3);float peak=0;for(int i=0;i<rate;++i){InputState input{};input.mouseDx=i<rate/2?1800.f/rate:0;g.update(input,1.f/rate);peak=std::max(peak,std::fabs(g.weaponMotion().yaw));}return peak;};
 float fast60=turn(60),fast120=turn(120);if(fast60<.08f||std::fabs(fast60-fast120)>.012f)return false;
 std::ofstream("weapon-physics-test.txt")<<"Fast-turn yaw lag: "<<fast60<<" / "<<fast120<<" radians at 60/120 Hz; recoil and secondary motion settle.\n";
 return std::fabs(a.elbow)<.002f&&std::fabs(b.elbow)<.002f&&std::fabs(a.yaw)<.001f&&std::fabs(a.pitch)<.001f&&std::fabs(a.elbow-b.elbow)<.002f;
}
bool Game::testAudioEvents(){
 auto game=validationScene(Enemy::Kind::Huntsman,3);
 auto contains=[&](Sound value){return std::any_of(game.m_sounds.begin(),game.m_sounds.end(),[&](auto&e){return e.sound==value;});};
 bool steps=false,jump=false,land=false,pump=false;
 for(int i=0;i<150;++i){InputState input{};input.forward=i<65;input.jump=i==70;input.fire=i==125;game.update(input,1.f/60.f);
  for(auto&e:game.m_sounds)steps|=footstepSound(e.sound);
  jump|=contains(Sound::Jump);land|=contains(Sound::Land);pump|=contains(Sound::Pump);
 }
 if(!steps||!jump||!land||!pump)return false;
 for(auto kind:{Enemy::Kind::Huntsman,Enemy::Kind::Wasp,Enemy::Kind::Brute,Enemy::Kind::Mutant}){
  game=validationScene(kind);game.m_player.pitch=-35;game.m_sounds.clear();game.shoot();
  int voiceKind=kind==Enemy::Kind::Mutant?2:int(kind);
  if(!contains(Sound::Shot)||!contains(Sound(int(Sound::SpiderCall)+voiceKind*3)))return false;
  for(int attempts=0;game.m_enemies[0].alive&&attempts<32;++attempts){
   if(game.m_player.loaded==0)game.m_player.loaded=std::min(6,game.m_player.ammo);
   game.shoot();
  }
  if(game.m_enemies[0].alive)return false;
  if(!contains(Sound(int(Sound::SpiderDeath)+voiceKind*3)))return false;
 }
 game=validationScene(Enemy::Kind::Huntsman,3);game.m_player.pos={1.21f,4.5f};game.m_player.angle=kPi;
 for(int i=0;i<90;++i){InputState input{};input.forward=true;game.update(input,1.f/60.f);for(auto&e:game.m_sounds)if(footstepSound(e.sound))return false;}
 return true;
}

}
