#pragma once

#include "../core/Math.h"
#include "../world/World.h"
#include <vector>
#include "../audio/Sound.h"
#include "../audio/NpcVoiceCatalog.h"
#include "Combat.h"
#include "ai/guard/Config.h"
#include "ai/guard/GuardCtx.h"
#include "ai/Scheduler.h"
#include "ai/Nodes.h"
#include "ai/guard/Schedule.h"
#include "ai/enemy/Schedule.h"
#include "ai/friendly/Schedule.h"
#include "ai/friendly/Mind.h"
#include "ai/engineer/Drones.h"
#include <string>
#include <string_view>
#include <iosfwd>
#include <utility>
#include <cstdint>
#include "Ragdoll.h"

namespace retro {
constexpr int DisplayWidth=640,DisplayHeight=360;
// Zero denotes an older animation bank whose legacy simulation clocks remain valid.
float cleanupAnimationDuration(ActorVisual visual,int clip);
int guardTreatmentDuration(const ActorTrack& track);


struct InputState {
    bool console=false;
    std::string textInput;
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool sprint = false;
    bool jump = false;
    bool crouch = false;
    bool fire = false;
    bool guard = false;
    bool reload = false;
    int weaponScroll=0;
    bool use=false;
    bool flashlight=false;
    bool subdue=false;
    bool mute = false, music = false;
    bool escape=false, menuUp=false,menuDown=false,menuLeft=false,menuRight=false,menuAccept=false;
    bool inventory=false;
    int pointerX=-1,pointerY=-1;
    float mouseDx = 0.0f;
    float mouseDy = 0.0f;
    bool vrTracked=false;
    std::array<bool,2> vrGrip{};
    float vrYaw=0,vrPitch=0,vrEye=1.42f,vrMoveForward=0,vrMoveRight=0;
};

struct Enemy {
    using Kind=CreatureKind;
    Vec2 pos{};
    float hp = 110.0f;
    float attackCooldown = 0.0f;
    float painFlash = 0.0f;
    bool alive = true;
    Kind kind = Kind::Huntsman;
    float maxHp=110, deathTime=0, windup=0, strike=0, heading=0, gait=0;
    bool moving=false;
    float voiceTimer=0,stepTimer=0;
    float z=0,awareness=0,searchTime=0;
    float verticalVelocity=0,repathTimer=0,lastKnownZ=0;
    Vec2 waypoint{};
    Vec2 home{},lastKnown{};
    enum class State {Idle,Investigate,Chase,Search};State state=State::Idle;
    enum class StalkMode {Watch,Flank,Rush};StalkMode stalkMode=StalkMode::Watch;
    float stalkTimer=0,stalkSide=1;
    int surfaceMode=0; // floor, wall, ceiling
    Vec2 surfaceNormal{};
    // Ephemeral MaintainSchedule mind. Rebuilt from state after load; not archived.
    int schedule=0,scheduleStep=0,scheduleTask=0,scheduleStatus=0,scheduleMs=0,scheduleCursor=0,scheduleCursorSaved=0,scheduleTransitions=0;
    unsigned scheduleConditions=0;
    bool scriptSpawned=false; // Persistent population owned by a script, not the authored baseline.
    static constexpr float CorpseLifetime=2.4f;
    bool visible()const{return alive||deathTime<CorpseLifetime;}
    float bodyBottom()const{return z+(kind==Kind::Wasp?.55f:0.f);}
    float bodyTop()const{return z+(kind==Kind::Wasp?1.55f:kind==Kind::Brute||kind==Kind::Warden||kind==Kind::Mutant?1.85f:1.05f);}
    const char* name()const{return kind==Kind::Mutant?"MUTATED HUMAN":kind==Kind::Warden?"REACTOR STALKER":kind==Kind::Wasp?"XENOWASP":kind==Kind::Brute?"SCISSOR FIEND":"HUNTSMAN";}
};

struct Pickup {
    using Kind=PickupKind;
    Vec2 pos{};
    Kind kind = Kind::Health;
    bool active = true;
    float z = -999;
};
enum class GuardState {Patrol,Suspicious,Investigate,Search,Alert};
struct AiStimulus {Vec2 source{};float z=0,radius=0,gain=0;bool loud=false;int npc=-1;unsigned categories=ai::SoundWorld;};
struct NpcWeaponPose {bool armed=false,handgun=false,maintenance=false,crouched=false,supportAnimation=false,padVisible=false,pistolVisible=false;int supportClip=6;float supportPhase=0,treatment=0,engineering=0;Point3 grip{},muzzle{},direction{};float raise=0,flash=0,recoil=0;GuardAction action=GuardAction::None;float actionProgress=0,reload=0,grenadeThrow=0;};
enum class ShotKind {None,Solid,Enemy,Npc,Actor,Player,Ragdoll,Drone};
struct ShotHit {ShotKind kind=ShotKind::None;int level=0,index=-1;float distance=0,damageDistance=0;Point3 point{},normal{};};
struct AiTargetRef {ShotKind kind=ShotKind::None;int level=0,index=-1;};
struct TacticalGrenadePose {Point3 position{};float fuse=0;int level=0;};
struct AiTarget : AiTargetRef {
 Vec2 position{};float feet=0,height=0,eye=0;AiFaction faction=AiFaction::Neutral;
 bool valid()const{return kind==ShotKind::Player||kind==ShotKind::Enemy||kind==ShotKind::Npc||kind==ShotKind::Actor||kind==ShotKind::Drone;}
};
struct FriendlyActor {
 // The authored identity owns state keys; level/pose describe physical location.
 // Keep the immutable track alive when its home chunk releases geometry.
 int homeLevel=0,level=0;size_t homeActor=0;ActorPose pose{};
 std::shared_ptr<const AuthoredMapData> definition;
 const ActorTrack& track()const{return definition->actorTracks.at(homeActor);}
};
struct BulletImpact {Vec2 pos{};float z=0;Vec2 normal{};float time=0;int level=0;bool blood=false;int tint=0;float normalZ=0;};
struct BarrelExplosion {Vec2 pos{};float z=0,time=0;int level=0;};
struct Clutter {
 Vec2 pos{},velocity{};float z=0,vz=0,yaw=0,spin=0;int kind=0;bool projectile=false;float impactCooldown=0;
 float pitch=0,roll=0,pitchSpeed=0,rollSpeed=0,restTime=0;bool sleeping=false;
 std::array<float,3> size()const{constexpr std::array<float,3> sizes[]={{.22f,.22f,.003231f},{.156383f,.25f,.054035f},{.082788f,.083595f,.28f},{.24f,.222403f,.139543f},{.263634f,.28f,.014386f},{.12f,.11894f,.006745f},{.34f,.045f,.009f}};return sizes[kind];}
 std::array<float,3> rotate(float x,float y,float localZ)const{
  float cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll),cy=std::cos(yaw),sy=std::sin(yaw);
  float a=cp*x+sp*localZ,b=-sp*x+cp*localZ,c=cr*y-sr*b,d=sr*y+cr*b;
  return {cy*a-sy*c,sy*a+cy*c,d};
 }
 std::array<float,3> extent()const{auto s=size(),a=rotate(s[0]*.5f,0,0),b=rotate(0,s[1]*.5f,0),c=rotate(0,0,s[2]*.5f);return {std::fabs(a[0])+std::fabs(b[0])+std::fabs(c[0]),std::fabs(a[1])+std::fabs(b[1])+std::fabs(c[1]),std::fabs(a[2])+std::fabs(b[2])+std::fabs(c[2])};}
 float height()const{return extent()[2]*2;}
 float footprint()const{auto s=size();return std::max(s[0],s[1]);}
 Sound impactSound()const{return kind==2?Sound::JunkGlass:kind==0||kind==1||kind==5||kind==6?Sound::JunkSoft:Sound::JunkMetal;}
};

struct Player {
    static constexpr float StandingHeight=1.6f;
    static constexpr float CrouchedHeight=.58f;
    static constexpr float StandingEye=1.42f;
    static constexpr float CrouchedEye=.48f;
    Vec2 pos{2.5f, 2.5f};
    float angle = 0.0f;
    float pitch = 0.0f;
    float health = 100.0f;
    int ammo = 36;
    int loaded = 6;
    float z = 0.0f;
    float verticalVelocity = 0.0f;
    bool grounded = true;
    bool crouched = false;
    float eye=StandingEye;
    float hullHeight()const{return crouched?CrouchedHeight:StandingHeight;}
};
struct WeaponMotion {float yaw=0,pitch=0,bob=0,back=0,elbow=0,bolt=0,roll=0;};
struct Settings {float master=1,music=.75f,effects=1,sensitivity=1;bool invertMouse=false;};
struct MenuLayout {static constexpr int X=(DisplayWidth-304)/2,Y=(DisplayHeight-288)/2,Width=304,Height=288,RowTop=Y+46,RowHeight=21,Rows=10,SliderX=X+179,SliderWidth=75;};
struct TitleMenuLayout {static constexpr int X=66,Y=188,Width=238,RowHeight=28,Rows=5;};


class Game {
    friend struct GuardCtx;
    friend struct CivilianMindView;
public:
    explicit Game(WorldId id=WorldId::Campaign);
    explicit Game(std::shared_ptr<const CustomCampaign> campaign);
    WorldId worldId()const{return m_worldId;}
    static bool testWorldIsolation();
    static bool testServiceMaps();
    static bool testCampaignExtension();
    bool hazardActive(const Hazard& hazard)const;
    float compactorHeight(const Compactor& machine)const;
    void showTitleScreen();
    bool titleScreen()const{return m_titleScreen;}
    int titleSelection()const{return m_titleSelection;}
    bool customMapsOpen()const{return m_customMapsOpen;}
    int titleRows()const;
    std::string customMenuLabel(int row)const;
    int customCampaignCount()const{return int(m_customCampaigns.size());}
    void loadCustomCampaignDirectory(std::wstring directory);
    bool menuFromTitle()const{return m_menuFromTitle;}

    void update(const InputState& input, float dt);
    void moveVrRoom(Vec2 delta){tryMove(delta);}
    void setVrAim(Point3 position,Point3 direction,bool valid,bool support=false){m_vrAimPosition=position;m_vrAimDirection=direction;m_vrAimValid=valid;m_vrSupportGrip=support&&valid;}
    void setVrHand(int side,Point3 position,Point3 direction,bool valid,Point3 velocity={}){m_vrHandVelocity[side]=velocity;m_vrInputActive=true;m_vrHandPosition[side]=position;m_vrHandDirection[side]=direction;m_vrHandValid[side]=valid;}
    bool vrSupportGrip()const{return m_vrSupportGrip;}
    void restart();
    int level()const{return m_level;}
    // Campaign content count; runtime custom packs use their authored map count.
    static constexpr int ChunkCount=CampaignChunkCount;
    static constexpr int MaxChunks=WorldChunkCapacity;
    int chunkCount()const{return m_worldId==WorldId::Custom&&m_customCampaign?int(m_customCampaign->maps.size()):worldChunkCount(m_worldId);}
    Vec2 chunkOffset(int level)const{return m_worldId==WorldId::Custom&&m_customCampaign?m_customCampaign->maps.at(size_t(level))->definition.origin:chunkDefinition(m_worldId,level).origin;}
    Game chunkView(int level)const;
    const World& chunkWorld(int level)const{return level==m_level?m_world:m_chunks.at(size_t(level)).world;}
    const std::vector<Enemy>& chunkEnemies(int level)const{return level==m_level?m_enemies:m_chunks.at(size_t(level)).enemies;}
    const std::vector<Pickup>& chunkPickups(int level)const{return level==m_level?m_pickups:m_chunks.at(size_t(level)).pickups;}
    const std::vector<Clutter>& chunkClutter(int level)const{return level==m_level?m_clutter:m_chunks.at(size_t(level)).clutter;}
    const World& worldAt(Vec2& local)const;
    bool soundRayClear(Vec2 from,float fromZ,Vec2 to,float toZ,float nearTrim=0,float farTrim=0)const;
    bool chunkResident(int level)const{return level>=0&&level<chunkCount()&&(level==m_level||m_chunks[size_t(level)].resident);}

    const World& world() const { return m_world; }
    const Player& player() const { return m_player; }
    const Ragdoll& hazmat()const{return m_hazmat;}
    static Game hazmatInspection(int view=0);
    static bool testHazmat();
    const std::vector<Enemy>& enemies() const { return m_enemies; }
    const std::vector<FriendlyActor>& friendlyActors()const{return m_friendlyActors;}
    const std::vector<FriendlyActor>& npcActors()const{return m_friendlyActors;}
    NpcWeaponPose npcWeaponPose(size_t actor)const;
    std::vector<TacticalGrenadePose> tacticalGrenades(bool dangerOnly=false)const;
    std::vector<EngineerDronePose> engineerDrones()const;
    AiTarget engineerDroneTarget(int slot,int observerLevel)const;
    GuardState guardState(size_t actor)const{return GuardState(state(friendlyKey(actor,"guard_mode")));}
    bool autonomousActor(const ActorTrack& track)const{return track.ai.mode!=ActorAiMode::Scripted&&(!track.ai.enableState||state(track.ai.enableState)||(track.deadState&&state(track.deadState)));}
    bool scriptedHandoffActive(size_t actor)const;
    bool scriptedHandoffActive(int level,size_t homeActor)const;
    bool friendlyChunkNeeded(int level)const;
    const std::vector<Pickup>& pickups() const { return m_pickups; }
    float pickupHeight(const Pickup& pickup)const{return pickup.z>-999?pickup.z:m_world.floorHeight(pickup.pos.x,pickup.pos.y);}
    const std::vector<Clutter>& clutter()const{return m_clutter;}
    const std::vector<BulletImpact>& bulletImpacts()const{return m_bulletImpacts;}
    const std::vector<BarrelExplosion>& barrelExplosions()const{return m_barrelExplosions;}
    bool holdingClutter()const{return m_heldClutter>=0;}
    int vrCarryHand()const{return m_vrCarryHand;}
    static bool testClutter();
    static Game clutterInspection(int kind,float seconds);
    static bool testStreaming();
    static bool testCampaignMaps();
    void updateMechanisms(float dt);
    std::string_view actorCaption()const;
    static bool testMechanisms();

    float weaponKick() const { return m_weaponKick; }
    const WeaponMotion& weaponMotion()const{return m_weaponMotion;}
    float shotAge()const{return m_shotAge;}
    float holster()const{return m_holster;}
    float punchAge()const{return m_punchAge;}
    bool punchLeft()const{return m_punchLeft;}
    bool unarmed()const{return m_holster>=.98f;}
    bool guarding()const{return m_guarding;}
    bool takedownActive()const{return m_takedownActor>=0;}
    std::array<Point3,2> takedownGrips()const;
    static bool testUnarmed();
    static bool testVrMelee();
    static bool testVrInteraction();
    static Game weaponInspection(int mode,float age=0);
    float damageFlash() const { return m_damageFlash; }
    float elapsed() const { return m_elapsed; }
    int kills() const { return m_kills; }
    bool won() const { return m_won; }
    bool dead() const { return m_player.health <= 0.0f; }
    int enemiesRemaining() const;
    int enemiesRemaining(int level)const{return hostilesRemaining(level);}
    float hitFlash()const{return m_hitFlash;}
    const Enemy* targetEnemy()const;
    static bool testCombat();
    static bool testWeaponMotion();
    static bool testAudioEvents();
    static bool testSettings();
    static bool testSaves();
    static bool testNpcVoices();
    enum class MenuPage {Settings,Save,Load,Overwrite,ConfirmLoad,ConfirmRestart};
    MenuPage menuPage()const{return m_menuPage;}
    int menuRows()const{return m_menuPage==MenuPage::Settings?(m_menuFromTitle?6:MenuLayout::Rows):m_menuPage==MenuPage::Save?4:m_menuPage==MenuPage::Load?5:2;}
    const std::string& menuMessage()const{return m_menuMessage;}
    const std::string& slotLabel(int slot)const{return m_slotLabels[slot];}
    const std::string& checkpointLabel()const{return m_checkpointLabel;}
    void setSaveDirectory(std::wstring path){m_saveDirectory=std::move(path);refreshSaveSlots();}
    bool saveSlot(int slot);
    bool loadSlot(int slot);
    bool saveCheckpoint();
    bool loadCheckpoint();
    unsigned sessionRevision()const{return m_sessionRevision;}
    static bool testPickups();
    static bool testMovement();
    static bool testProgression();
    static bool testAI();
    static bool testHearing();
    static bool testGuardAI();
    static bool testGuardCombat();
    static bool testGuardTactics();
    static bool testGuardPolicy();
    static bool testGuardDefense();
    static bool testGuardPursuit();
    static bool testGuardActions();
    static bool testTacticalGrenades();
    static bool testGuardRoles();
    static bool testEngineerDrones();
    static bool testGuardDanger();
    static bool testGuardSchedule();
    static bool testGuardPatient();
    static bool testGuardCover();
    static bool testGuardTable();
    static bool testNavGraph();
    static bool testCreatureSchedule();
    static bool testCivilianSchedule();
    static Game squadSupportInspection(int stage=0);
    static Game guardDangerInspection(int stage=0);
    static Game engineerSupportInspection(int stage=0);
    static Game guardPolicyInspection(int stage=0);
    static Game killSquadInspection(int stage=0);
    static Game cleanupLayerInspection(int role,int sample,int view=0,float phase=-1);
    static Game guardTacticsInspection(int stage=0);
    static Game guardCombatInspection(int stage=0);
    static Game guardInspection(int stage=0);
    static bool testFriendlyAI();
    static bool testAiQueryBatch();
    void setAiQueryCaching(bool enabled){m_aiQueryCaching=enabled;}
    bool walkExclusionEast(std::ostream& report);
    static bool testExclusion();
    static bool testExclusionAuthoring();
    static Game exclusionInspection(int stage=0);
    static bool testStealth();
    bool npcIncapacitated(size_t actor)const;
    bool npcHeld(size_t actor)const;
    int heldNpc()const;
    std::array<Point3,2> heldNpcGrips()const;
    void updateTakedown(float dt);
    static bool testGantry();
    static bool testLift();
    static Game liftInspection(float seconds,int view=0);
    static bool testReactor();
    const char* interactionHint()const;
    bool doorLocked(const Door& door)const;
    int activeLog()const{return m_activeLog;}
    float logTime()const{return m_logTime;}
    const Pickup* nearbyPickup()const;
    const std::string& pickupNotice()const{return m_pickupNotice;}
    float pickupNoticeTime()const{return m_pickupNoticeTime;}
    bool paused()const{return m_paused;}
    bool consoleOpen()const{return m_consoleOpen;}
    const std::string& consoleLine()const{return m_consoleLine;}
    const std::vector<std::string>& consoleLog()const{return m_consoleLog;}
    bool showFps()const{return m_showFps;}
    float renderScale()const{return m_renderScale;}
    static bool testConsole();
    static bool testSystems();
    int state(StateId id)const;
    int state(std::string_view name)const{return state(stateId(name));}
    void setState(StateId id,int value=1);
    ObjectiveStatus objective(StateId id)const;
    std::string_view activeObjectiveText()const;
    void setObjective(StateId id,ObjectiveStatus status);
    bool hasQuestItem(StateId id,int count=1)const;
    int questItemCount(StateId id)const;
    void giveQuestItem(StateId id,int count=1);
    bool takeQuestItem(StateId id,int count=1);
    const std::vector<QuestItemStack>& questItems()const{return m_questItems;}
    static const char* questItemName(StateId id);
    static constexpr StateId ReactorAuthDisk=stateId("reactor_auth_disk");
    static constexpr StateId Flashlight=stateId("flashlight");
    bool hasFlashlight()const{return hasQuestItem(Flashlight);}
    bool flashlightOn()const{return hasFlashlight()&&state(stateId("flashlight_on"))!=0;}
    static bool testFlashlight();
    bool inventoryOpen()const{return m_inventoryOpen;}
    // Reactor rendering wakes during the first lift shake, one vertical band
    // at a time. Direct reactor entry and restored underground saves bypass it.
    float dormantBelow()const{return dormantBelow(m_level);}
    float dormantBelow(int level)const{
     const auto&world=chunkWorld(level);
     if(!world.hasLift()||m_player.z<-2)return -1000.f;
     if(world.liftPhase()==World::LiftPhase::Ready)return -4.5f;
     if(world.liftPhase()==World::LiftPhase::Ascending)return -4.5f-5.5f*std::clamp((world.liftPhaseTime()-10.f)/3.f,0.f,1.f);
     return -1000.f;
    }
    bool dormantEntity(float z)const{return m_world.hasLift()&&z<-2&&m_player.z>=-2&&m_world.liftPhase()!=World::LiftPhase::Crashed;}
    bool weaponEquipped()const{return m_weaponEquipped;}
    WeaponId equippedWeapon()const{return m_weaponEquipped?m_equippedWeapon:WeaponId::Fists;}
    const WeaponDef& equippedWeaponDef()const{return weaponDef(m_equippedWeapon);}
    const MeleeDef& unarmedMelee()const{return kUnarmedMelee;}
    void equipWeapon(WeaponId id);
    int medkits()const{return m_medkits;}
    int selectedItem()const{return m_selectedItem;}
    int itemCell(int item)const{return m_itemCells[item];}
    static bool testInventory();
    bool quitRequested()const{return m_quitRequested;}
    int menuSelection()const{return m_menuSelection;}
    const Settings& settings()const{return m_settings;}
    void loadSettings(const std::wstring& path);
    void saveSettings(const std::wstring& path)const;
    const std::vector<SoundEvent>& sounds()const{return m_sounds;}
    bool audioMuted()const{return m_audioMuted;}
    bool musicEnabled()const{return m_musicEnabled;}
    static Game validationScene(Enemy::Kind kind,float deathTime=-1,float windup=0);
    static Game stalkerInspection(int clip,float phase,int view=0,Enemy::Kind kind=Enemy::Kind::Warden);
    static Game terminalInspection(int level,int terminal=0);
    static Game mapInspection(Vec2 position,float angle,float pitch=0,int level=0,bool openDoors=false,float height=-999,bool sceneryOnly=false,WorldId id=WorldId::Campaign);
    static Game barrelInspection(float age);

private:
    int nearbyTakedown()const;
    bool canTakedown(size_t actor)const;
    void restrainNpc(size_t actor,bool held);
    void releaseNpc(bool unconscious=false);
    void updateStealth(const InputState& input,float dt);
    void saveRestrainedPose(size_t actor);
    std::pair<Point3,Point3> friendlyShotBounds(size_t actor)const;
    bool m_previousSubdue=false;
    int m_takedownActor=-1;
    float m_takedownAge=0;
    WorldId m_worldId=WorldId::Campaign;
    std::shared_ptr<const CustomCampaign> m_customCampaign;
    std::vector<std::shared_ptr<const CustomCampaign>> m_customCampaigns;
    std::wstring m_customMapDirectory;
    std::uint64_t m_customCampaignKey=0;
    bool m_titleScreen=false,m_menuFromTitle=false,m_customMapsOpen=false;
    int m_titleSelection=0,m_customMenuOffset=0;
    InputState m_titlePrevious;
    void updateTitle(const InputState& input);
    MenuPage m_menuPage=MenuPage::Settings;
    std::wstring m_saveDirectory;
    std::array<std::string,3> m_slotLabels{"SLOT 1 / EMPTY","SLOT 2 / EMPTY","SLOT 3 / EMPTY"};
    std::string m_checkpointLabel="AUTOSAVE / EMPTY";
    std::string m_menuMessage;
    int m_pendingSlot=-1;
    unsigned m_sessionRevision=0;
    void refreshSaveSlots();
    std::string encodeSave()const;
    bool decodeSave(const std::string& data);
    template<class Archive> void archiveSave(Archive& archive,int version=2);
    bool nearReactorDisk()const;
    void useReactorAction(int action);
    void updateConsole(const InputState& input);
    void executeConsole(std::string command);
    bool m_consoleOpen=false,m_previousConsole=false,m_consoleUp=false,m_consoleDown=false,m_showFps=false;
    std::string m_consoleLine;
    std::vector<std::string> m_consoleLog{"DEPTHWORKS DEVELOPER CONSOLE", "MAPS / MAP <NAME OR ID> / RELOAD / WHERE / FPS / CLEAR"};
    std::vector<std::string> m_consoleHistory;
    int m_consoleHistoryIndex=0;
    float m_renderScale=1.f;
    std::vector<StateValue> m_states,m_objectives;
    std::vector<QuestItemStack> m_questItems;
    std::vector<StateId> m_firedEvents;
    std::vector<ScriptEvent> m_scriptEvents;
    float m_hazardSoundTimer=0;
    void seedScripts();
    void updateScripts(float dt);
    void updateReceivingBreach();
    void executeScriptAction(const ScriptAction& action);
    void spawnScriptEnemy(const ScriptAction& action);
    void spawnCreature(const CreatureSpawn& spawn,float awareness=0,bool announce=false);
    void updateHazards(float dt);
    void loadLevel(int level,bool carry);
    World makeWorld(int level)const;
    void selectCustomCampaign(int index);
    int customMenuItemCount()const{return int(m_customCampaigns.size())+1;} // uploaded campaigns + Back
    int customMenuLogicalIndex(int row)const{return m_customMenuOffset+row;}
    int m_level=0;
    struct ChunkState {World world;ai::NodeGraph nav;bool navReady=false;std::vector<Enemy> enemies;std::vector<Pickup> pickups;int kills=0;bool resident=true;std::vector<Clutter> clutter;};
    // Chunk storage outlives a frame and can be large; keep it off the native
    // stack, including nested save previews and campaign validation scenes.
    std::vector<ChunkState> m_chunks=std::vector<ChunkState>(MaxChunks);
    ai::NodeGraph m_levelNav{};
    bool m_levelNavReady=false;
    void storeChunk();
    void crossChunkBoundary();
    void ensureChunk(int level);
    void updateStreaming(float dt);
    void useDoor(int index);
    void updateClutter(const InputState& input,float dt);
    bool interactClutter(int hand=-1);
    float vrInteractionDistance(Point3 target,float housingRadius)const;
    int nearbyInteractionDoor()const;
    int nearbyClutter(int hand=-1)const;
    void seedClutter();
    bool tryMove(Vec2 delta);
    bool hullFits(Vec2 position,float feet,float height)const;
    float groundHeight(Vec2 position)const;
    float groundHeight(Vec2 position,float feet)const;
    void updateMovement(const InputState& input,float dt);
    void updateLift(float dt);
    void updateInteraction(const InputState& input,float dt);
    bool lineOfSight(const Vec2& a, const Vec2& b) const;
    void shoot();
    ShotHit traceShot(Point3 origin,Point3 direction,float range,int ignoredNpc=-1,bool includePlayer=false,const HitscanDef* assist=nullptr,AiTargetRef ignoredEntity={})const;
    void resolveShot(const ShotHit& hit,Point3 origin,Point3 direction,const HitscanDef& definition,bool playerFired,float damageOverride=0,AiTargetRef attacker={});
    void reloadWeapon();
    void punchImpact();
    void applyPunchHit(Enemy& enemy);
    void applyNpcPunchHit(size_t actor);
    bool applyEnemyDamage(Enemy& enemy,float amount,bool bloodDecal=true,bool closeHitStop=false,int level=-1);
    void updateVrMelee(const InputState& input,float dt);
    void receiveDamage(float amount,Vec2 source,bool blockable=true);
    void updateEnemies(float dt);
    unsigned creatureConditions(const Enemy& enemy,bool visible,bool heard,bool canMelee,bool hurt)const;
    CreatureScheduleChoice creatureScheduleChoice(const Enemy& enemy,unsigned conditions,bool arrive)const;
    bool creatureTaskAllows(const Enemy& enemy,CreatureTask task)const;
    void clearCreatureSchedule(Enemy& enemy)const;
    void runCreatureSchedule(Enemy& enemy,int ms,bool visible,bool heard,bool canMelee,bool hurt,bool wantsMove);
    ActorPose friendlyAwareness(const World& world,int level,size_t actor,const ActorPose& old,ActorPose pose,float dt)const;
    ActorPose updateFriendlyAI(World& world,int level,size_t actor,const ActorTrack& track,const ActorPose& old,ActorPose pose,float dt,const std::vector<AiStimulus>* stimuli=nullptr);
    void guardIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<AiStimulus>& stimuli,Vec2& goal,float& goalZ,bool& wantsMove);
    void guardBaseIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    void runGuardSchedule(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    GuardScheduleChoice guardScheduleChoice(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,int ms,Vec2& goal,float& goalZ,bool& wantsMove,unsigned& conditions,GuardSchedule previous,int oldSubject,int retry,bool changed,bool close,const AiTarget& target);
    bool guardTaskAllows(size_t actor,GuardTask task)const;
    void finishGuardTask(size_t actor);
    void guardTaskMovement(size_t actor,const ActorPose& old,const ActorPose& pose,Vec2 waypoint,bool wanted,int ms);
    void failGuardTask(size_t actor,GuardTaskReason reason);
    void clearGuardSchedule(size_t actor);
    void validateGuardSchedule(size_t actor)const;
    void civilianEscortIntent(World& world,int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<Enemy>& enemies,Vec2& goal,float& goalZ,bool& wantsMove,float& speedScale);
    void scriptedHandoffIntent(World& world,int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<Enemy>& enemies,Vec2& goal,float& goalZ,bool& wantsMove,float& speedScale);
    void runCivilianSchedule(size_t actor,int ms,unsigned conditions,bool wantsMove);
    void clearCivilianSchedule(size_t actor);
    GuardCtx makeGuardCtx(size_t actor,int level,const ActorTrack& track,ActorPose& pose){return {*this,actor,level,track,pose};}
    void validateGuardState(size_t actor)const;
    void updateGuardFlags();
    void updateGuardCombat(float dt);
    NpcWeaponPose npcWeaponPoseAt(size_t actor,const ActorPose& pose,Point3 target,float raise)const;
    ShotHit guardShot(size_t actor,const ActorPose& pose,const NpcWeaponPose& weapon)const;
    void guardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    const ai::NodeGraph& navGraph(int level,float height);
    bool findGuardGraphCover(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ);
    bool findGuardLateralCover(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ,float sideSign=0.f);
    bool findGuardCoverFromEnemy(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ,float sideSign=0.f);
    // Cover/path post with a clear raised shot — used for flank and offense in
    // rooms that already expose structure Cover on the retained nav graph.
    bool findGuardCombatPost(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ,float sideSign=0.f);
    float guardLateralClearance(int level,size_t actor,const ActorPose& pose,Vec2 direction)const;
    Vec2 guardGraphWaypoint(int level,Vec2 start,float feet,Vec2 goal,float goalFeet,float height);
    AiTarget resolveGuardTarget(size_t actor,AiTargetRef reference)const;
    AiTarget guardThreat(size_t actor,const ActorPose& pose)const;
    AiTarget guardTarget(size_t actor)const;
    AiTarget guardAttacker(size_t actor)const;
    bool guardCanSee(size_t actor,const ActorPose& pose,const AiTarget& target)const;
    void rememberGuardTarget(size_t actor,const AiTarget& target);
    AiTarget guardSquadReport(size_t actor)const;
    bool guardShotMatches(size_t actor,const ShotHit& hit)const;
    bool guardMeleeReady(size_t actor,const ActorPose& pose,const AiTarget& target)const;
    void damageGuardTarget(size_t actor,float amount);
    bool guardCovered(size_t actor,const ActorPose& pose,const AiTarget& target)const;
    void updateGuardSquadSlots(float dt);
    bool guardFireSlot(size_t actor)const;
    int guardFlankSide(size_t actor)const;
    bool guardPositionReserved(size_t actor,Vec2 position,float feet)const;
    bool guardHostileToPlayer(size_t actor)const;
    void validateGuardPolicyState(size_t actor)const;
    void validateGuardActions(size_t actor)const;
    bool guardDangerIntent(size_t actor,ActorPose& pose,float dt,Vec2& goal,float& goalZ,bool& wantsMove);
    bool guardGrenadeExposed(size_t actor)const;
    void validateGuardDanger(size_t actor)const;
    void guardSupportIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,Vec2& goal,float& goalZ,bool& wantsMove,bool advanceWork=true);
    void guardEngineerIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    void validateGuardSupport(size_t actor)const;
    void runGuardTreatment(size_t actor,int ms);
    bool guardPatientIntent(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove,int& medic);
    int guardPatientMedic(size_t actor)const;
    void releaseGuardPatient(size_t actor);
    int guardCoveringPeer(size_t actor)const;
    unsigned guardConditions(size_t actor,const ActorPose& pose)const;
    void guardAvoidGrenade(size_t actor,const ActorPose& pose,Vec2& goal,float& goalZ,bool& wantsMove);
    void updateEngineerDrones(float dt);
    void validateEngineerDrones()const;
    bool deployEngineerDrone(size_t owner);
    void damageEngineerDrone(int slot,float amount,AiTargetRef attacker={});
    ShotHit traceEngineerDrone(Point3 from,Point3 to,size_t owner)const;
    bool engineerDroneChunkNeeded(int level)const;
    bool guardSuppressionPoint(size_t actor,Point3& point)const;
    bool guardRoute(int level,size_t actor,Vec2 globalGoal,float feet,int ms,Vec2& localGoal,float& goalFeet);
    void updateTacticalGrenades(float dt);
    void tryGuardGrenade(size_t actor,float dt);
    void validateTacticalGrenades()const;
    bool spawnTacticalGrenade(Point3 origin,Point3 velocity,AiTargetRef attacker,StateId squad=0,AiFaction faction=AiFaction::Neutral,int fuseMs=2400);
    void explodeTacticalGrenade(int slot,Point3 globalPosition,AiTargetRef attacker);
    ShotHit tacticalGrenadeTrace(Point3 from,Point3 to)const;
    int npcMilliseconds(size_t actor,float dt,std::string_view field);
    std::vector<AiStimulus> aiStimuli()const;
    void rebuildFriendlyActors();
    void updateFriendlyActors(float dt,bool guards=false);
    void updateNpcVoices(float dt);
    bool npcVoice(size_t actor,NpcVoiceCue cue);
    bool npcHasVoice(size_t actor)const;
    StateId friendlyKey(size_t actor,std::string_view field)const;
    World& friendlyWorld(int level){return level==m_level?m_world:m_chunks[level].world;}
    int friendlyChunkAt(Vec2 global,int preferred)const;
    bool friendlyBoundaryOpen(int from,int to,Vec2 global)const;
    bool friendlyWalkSegment(int level,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
    float friendlySupportAt(int level,Vec2 position,float feet);
    bool friendlyBodyFits(int level,Vec2 position,float feet,float height);
    // Cleanup's kneeling bodies stay near standing height; the player's deep
    // crouch is a separate pose and cannot describe an NPC's physical hull.
    static float npcCrouchedHeight(const ActorTrack& track){return track.scale*.85f;}
    float friendlyHullHeight(size_t actor,const ActorTrack& track)const;
    bool friendlyRayClear(Point3 from,Point3 to)const;
    bool friendlyPortal(int from,int target,Vec2 position,float feet,float height,Vec2& goal,float& goalFeet,float targetFeet=std::numeric_limits<float>::quiet_NaN());
    int nearbyFriendly()const;
    bool commandFriendly();
    void requestAiDoor(World& world,int level,Vec2 position,float feet,Vec2 goal);
    bool aiDoorLocked(const World& world,int level,const Door& door)const;
    int hostilesRemaining(int level)const;
    void damageFriendly(size_t actor,float amount,Vec2 source,AiTargetRef attacker={});
    void migrateEnemiesAcrossChunks();
    void updatePickups();

    World m_world;
    Ragdoll m_hazmat;
    float m_hazmatPushCooldown=0;
    Player m_player;
    std::vector<Enemy> m_enemies;
    std::vector<FriendlyActor> m_friendlyActors;
    std::vector<AiStimulus> m_guardShots;
    std::vector<Pickup> m_pickups;
    std::vector<Clutter> m_clutter;
    std::vector<BulletImpact> m_bulletImpacts;
    std::vector<BarrelExplosion> m_barrelExplosions;
    int m_heldClutter=-1;
    int m_npcNavBudget=4;
    bool m_aiQueryCaching=true;
    bool m_vrAimValid=false,m_vrSupportGrip=false;
    Point3 m_vrAimPosition{},m_vrAimDirection{};
    bool m_vrInputActive=false;int m_vrCarryHand=1,m_vrInteractionHand=1;
    std::array<Point3,2> m_vrHandPosition{},m_vrHandDirection{},m_vrHandVelocity{};
    std::array<bool,2> m_vrHandValid{},m_vrMeleeTracked{};
    std::array<Point3,2> m_vrPreviousFist{};
    std::array<float,2> m_vrFistCooldown{};
    bool m_previousFire = false;
    bool m_previousReload = false;
    float m_weaponKick = 0.0f,m_reloadTimer=0.0f;
    float m_shotCooldown = 0.0f;
    Vec2 m_velocity{};
    float m_damageFlash = 0.0f;
    float m_hitStopRemaining=0.f;
    float m_elapsed = 0.0f;
    int m_kills = 0;
    bool m_won = false;
    float m_hitFlash=0;
    WeaponMotion m_weaponMotion;
    Vec2 m_sway{},m_swayVelocity{};
    float m_elbowVelocity=0,m_shotAge=10;
    float m_holster=0,m_punchAge=10,m_verticalSpring=0,m_verticalSpringVelocity=0;
    bool m_punchLeft=false;
    bool m_guarding=false;
    std::vector<SoundEvent> m_sounds;
    float m_stepDistance=0;
    bool m_previousJump=false,m_previousUse=false;
    float m_jumpBuffer=0,m_coyote=0,m_logTime=0;
    int m_activeLog=-1;
    int nearbyTerminal()const;
    std::string m_pickupNotice;
    float m_pickupNoticeTime=0;
    unsigned m_stepVariant=0;
    bool m_audioMuted=false,m_musicEnabled=true,m_previousMute=false,m_previousMusic=false,m_previousFlashlight=false;
    Settings m_settings;
    bool m_paused=false,m_inventoryOpen=false,m_quitRequested=false,m_previousEscape=false,m_previousInventory=false,m_suppressFire=false;
    int m_menuSelection=0,m_pointerX=-1,m_pointerY=-1,m_dragSlider=-1;
    std::array<bool,2> m_previousVrGrip{};
    InputState m_menuPrevious;
    void updateMenu(const InputState& input);
    void updateInventory(const InputState& input);
    bool m_weaponEquipped=true,m_inventoryClick=false,m_inventoryUse=false;
    WeaponId m_equippedWeapon=WeaponId::Shotgun;
    int m_medkits=0,m_selectedItem=-1;
    std::array<int,3> m_itemCells{12,0,2};
    void sound(Sound sound,float gain=1,float pitch=1);
    void enemySound(const Enemy& enemy,int action,float gain=1,float pitch=1);
    void updateWeaponMotion(const InputState& input,float dt);
};

}
