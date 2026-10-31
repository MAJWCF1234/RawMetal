#pragma once

#include <unordered_map>
#include <atomic>
#include <filesystem>
#include <memory>
#include <functional>
#include "../core/Math.h"
#include "../world/World.h"
#include <vector>
#include <algorithm>
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
#include <optional>
#include <cstdint>
#include "Ragdoll.h"

namespace retro {
// Bumped once per chunk built while a save loads on a worker thread, so the
// loading bar can move; null (no counting) everywhere else.
inline thread_local std::atomic<int>* loadProgressCounter=nullptr;
struct LoadJob;
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
    // 1-4 while a number key is held: a reply in a conversation.
    int choice=0;
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
    // Grab and Recover are the Snatcher's: holding its prey, sprawled after a lunge.
    enum class StalkMode {Watch,Flank,Rush,Retreat,Grab,Recover};StalkMode stalkMode=StalkMode::Watch;
    float stalkTimer=0,stalkSide=1;
    int surfaceMode=0; // floor, wall, ceiling
    Vec2 surfaceNormal{};
    // The Snatcher's fade: the cover it chose when it was seen, and how many times
    // the player has made it give ground since its last snatch. Not archived.
    Vec2 hideAt{};bool hideSet=false;int fades=0;
    // No route to its goal: it ambles a cell at a time instead of re-planning
    // every frame. Not archived.
    bool lostPath=false;
    // Ephemeral MaintainSchedule mind. Rebuilt from state after load; not archived.
    int schedule=0,scheduleStep=0,scheduleTask=0,scheduleStatus=0,scheduleMs=0,scheduleCursor=0,scheduleCursorSaved=0,scheduleTransitions=0;
    unsigned scheduleConditions=0;
    bool scriptSpawned=false; // Persistent population owned by a script, not the authored baseline.
    StateId wakeState=0;      // Dormant, blind and deaf until this state is set or it is hurt.
    float homeZ=-999;         // The floor it walks back to; -999 (an older save) uses its last known floor.
    float scale=1;            // Authored size: its body, hull and reach grow with it.
    // An authored creature that followed the player across a seam keeps the chunk
    // and home it was placed at, so a load matches it there instead of respawning it.
    int originLevel=-1;Vec2 originHome{};
    static constexpr float CorpseLifetime=2.4f;
    bool visible()const{return alive||deathTime<CorpseLifetime;}
    float bodyBottom()const{return z+(kind==Kind::Wasp?.55f:0.f);}
    float bodyTop()const{return z+creatureTraits(kind).top*scale;}
    float hull()const{return creatureTraits(kind).hull*scale;}
    float reach()const{return creatureTraits(kind).reach*scale;}
    const char* name()const{return creatureTraits(kind).name;}
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
struct NpcWeaponPose {bool armed=false,handgun=false,maintenance=false,crouched=false,supportAnimation=false,padVisible=false,pistolVisible=false;int supportClip=6;float supportPhase=0,treatment=0,engineering=0;Point3 grip{},muzzle{},direction{};float raise=0,flash=0,recoil=0;GuardAction action=GuardAction::None;float actionProgress=0,reload=0,grenadeThrow=0,haul=0;Point3 handle{};};
enum class ShotKind {None,Solid,Enemy,Npc,Actor,Player,Ragdoll,Drone,Prop};
struct ShotPassage {int kind=0,level=0,index=-1;float distance=0;Point3 point{},normal{};};
struct ShotHit {ShotKind kind=ShotKind::None;int level=0,index=-1;float distance=0,damageDistance=0;Point3 point{},normal{};int breakable=-1;std::array<ShotPassage,2> passed{};};
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
// A Cleanup man on the patched-through yard cameras: where he stands in the
// current chunk, which way he looks, how far he sees and how wide (cone is the
// half-angle in radians), and what he makes of the player.
struct RadarContact {Vec2 position{};float yaw=0,range=0,cone=0;GuardState state=GuardState::Patrol;size_t actor=0;};
struct BulletImpact {Vec2 pos{};float z=0;Vec2 normal{};float time=0;int level=0;bool blood=false;int tint=0;float normalZ=0;};
struct BarrelExplosion {Vec2 pos{};float z=0,time=0;int level=0;};
struct PropTraits {PropMaterial material;float mass,health;std::array<float,3> size;};
struct BreakEvent {Vec2 pos{};float z=0;Point3 direction{};float time=0;int level=0;PropMaterial material=PropMaterial::Glass;float width=.1f,height=.1f;bool pane=false;Vec2 along{};};
struct Clutter {
 Vec2 pos{},velocity{};float z=0,vz=0,yaw=0,spin=0;int kind=0;bool projectile=false;float impactCooldown=0;
 float pitch=0,roll=0,pitchSpeed=0,rollSpeed=0,restTime=0;bool sleeping=false;
 int model=-1;std::array<float,3> dims{};float damage=0;bool authored=false,broken=false;Vec2 home{};float homeZ=0;StateId breakState=0;
 PropMaterial material()const;float mass()const;
 std::array<float,3> size()const{if(model>=0)return dims;constexpr std::array<float,3> sizes[]={{.22f,.22f,.003231f},{.156383f,.25f,.054035f},{.082788f,.083595f,.28f},{.24f,.222403f,.139543f},{.263634f,.28f,.014386f},{.12f,.11894f,.006745f},{.34f,.045f,.009f}};return sizes[kind];}
 std::array<float,3> rotate(float x,float y,float localZ)const{
  float cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll),cy=std::cos(yaw),sy=std::sin(yaw);
  float a=cp*x+sp*localZ,b=-sp*x+cp*localZ,c=cr*y-sr*b,d=sr*y+cr*b;
  return {cy*a-sy*c,sy*a+cy*c,d};
 }
 std::array<float,3> extent()const{auto s=size(),a=rotate(s[0]*.5f,0,0),b=rotate(0,s[1]*.5f,0),c=rotate(0,0,s[2]*.5f);return {std::fabs(a[0])+std::fabs(b[0])+std::fabs(c[0]),std::fabs(a[1])+std::fabs(b[1])+std::fabs(c[1]),std::fabs(a[2])+std::fabs(b[2])+std::fabs(c[2])};}
 float height()const{return extent()[2]*2;}
 float footprint()const{auto s=size();return std::max(s[0],s[1]);}
 Sound impactSound()const{if(model>=0)return propMaterialTraits(material()).impact;return kind==2?Sound::JunkGlass:kind==0||kind==1||kind==5||kind==6?Sound::JunkSoft:Sound::JunkMetal;}
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
// depthOfField and lensEffects scale the cinematic lens (1 = the tuned look, 0 off, up to 2).
struct Settings {float master=1,music=.75f,effects=1,sensitivity=1;bool invertMouse=false;float depthOfField=1,lensEffects=1;};
struct MenuLayout {static constexpr int X=(DisplayWidth-304)/2,Y=(DisplayHeight-330)/2,Width=304,Height=330,RowTop=Y+46,RowHeight=21,Rows=12,SliderX=X+179,SliderWidth=75;};
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
    // 0..1 beacon level from any running sequence whose repeating sound is the
    // Alarm klaxon: peaks on each blast and decays until the next.
    float alarmPulse()const;
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
    // Developer mode (--dev, console DEV): no damage, no creatures, every door open.
    void setDeveloperMode(bool on){m_god=m_peaceful=m_unlockAll=on;if(on){m_player.health=std::max(m_player.health,100.f);m_enemies.clear();}}
    bool developerMode()const{return m_unlockAll;}
    bool reloading()const{return m_reloadTimer>0;}
    // VR pump: fired with the support hand on the fore-end, the shotgun waits for
    // a stroke (back 7 cm, forward again). How far back the pump is, for drawing.
    float vrPumpOffset()const{return m_vrChamberEmpty&&m_vrSupportGrip?std::clamp(m_vrPumpRest-m_vrPumpAlong,0.f,.1f):0.f;}
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
    // Set by the CCTV patch in Perimeter Ops; from then on the minimap shows
    // the hostile guards in the current chunk (empty before it).
    static constexpr StateId CctvRadar=stateId("cctv_radar");
    std::vector<RadarContact> radarContacts()const;
    static bool testRadar();
    std::vector<TacticalGrenadePose> tacticalGrenades(bool dangerOnly=false)const;
    std::vector<EngineerDronePose> engineerDrones()const;
    AiTarget engineerDroneTarget(int slot,int observerLevel)const;
    GuardState guardState(size_t actor)const{return GuardState(state(friendlyKey(actor,"guard_mode")));}
    bool autonomousActor(const ActorTrack& track)const{return track.ai.mode!=ActorAiMode::Scripted&&(!track.ai.enableState||state(track.ai.enableState)||(track.deadState&&state(track.deadState)));}
    bool scriptedHandoffActive(size_t actor)const;
    bool scriptedHandoffActive(int level,size_t homeActor)const;
    bool friendlyChunkNeeded(int level)const;
    void friendlyChunksNeeded(std::vector<char>& needed)const;
    const std::vector<Pickup>& pickups() const { return m_pickups; }
    float pickupHeight(const Pickup& pickup)const{return pickup.z>-999?pickup.z:m_world.floorHeight(pickup.pos.x,pickup.pos.y);}
    const std::vector<Clutter>& clutter()const{return m_clutter;}
    const std::vector<BulletImpact>& bulletImpacts()const{return m_bulletImpacts;}
    const std::vector<BarrelExplosion>& barrelExplosions()const{return m_barrelExplosions;}
    bool holdingClutter()const{return m_heldClutter>=0;}
    int vrCarryHand()const{return m_vrCarryHand;}
    static bool testClutter();
    static PropTraits propTraits(int model);
    static bool auditMapScene(Game& game,int level);
    static bool testProps();static bool testPropSaves();static bool testGlass();static Game physicsInspection(int view,float seconds);
    const std::vector<BreakEvent>& breakEvents()const{return m_breakEvents;}
    static Game clutterInspection(int kind,float seconds);
    static bool testStreaming();
    static bool testCampaignMaps();
    static bool testChapterRoute();
    static bool testChapterSequences();
    void updateMechanisms(float dt);
    std::string_view actorCaption()const;
    // Captioned sequence cues emitted at a terminal's position (its computer's
    // messages) that have already played, in sequence order then time.
    std::vector<const SequenceCue*> terminalMessages(int terminal)const;
    // True while a sequence that speaks through this terminal still has lines to come.
    bool terminalConversationActive(int terminal)const;
    int sequenceClock(const World& world,StateId timer)const;
    static bool testMechanisms();

    float weaponKick() const { return m_weaponKick; }
    const WeaponMotion& weaponMotion()const{return m_weaponMotion;}
    float shotAge()const{return m_shotAge;}
    float holster()const{return m_holster;}
    float punchAge()const{return m_punchAge;}
    bool punchLeft()const{return m_punchLeft;}
    bool unarmed()const{return m_holster>=.98f;}
    void stowWeapon(){m_holster=1;}
    bool handsLowered()const{return m_handsLowered;}
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
    int menuRows()const{return m_menuPage==MenuPage::Settings?(m_menuFromTitle?8:MenuLayout::Rows):m_menuPage==MenuPage::Save?4:m_menuPage==MenuPage::Load?5:2;}
    const std::string& menuMessage()const{return m_menuMessage;}
    const std::string& slotLabel(int slot)const{return m_slotLabels[slot];}
    const std::string& checkpointLabel()const{return m_checkpointLabel;}
    void setSaveDirectory(std::wstring path){m_saveDirectory=std::move(path);refreshSaveSlots();}
    bool saveSlot(int slot);
    bool loadSlot(int slot);
    bool saveCheckpoint();
    bool loadCheckpoint();
    // With asynchronous loads (the shipped game turns them on) a load builds the
    // saved game on a worker while the window stays responsive and a loading
    // bar shows; update() commits it when it is ready. Tests load in place.
    void setAsyncLoads(bool enabled){m_asyncLoads=enabled;}
    bool loading()const{return m_loadJob!=nullptr;}
    float loadProgress()const;
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
    static bool testCreatureBehaviour();
    static bool testEngineRegression();
    static bool testEngineRegressionSaves(std::ostream& report);
    static bool testSnatcherAmbush(std::ostream& report);
    static bool testCivilianSchedule();
    static Game squadSupportInspection(int stage=0);
    static Game guardDangerInspection(int stage=0);
    static Game engineerSupportInspection(int stage=0);
    static Game guardPolicyInspection(int stage=0);
    static Game killSquadInspection(int stage=0);
    static Game cleanupLayerInspection(int role,int sample,int view=0,float phase=-1);
    // One civilian body (civilianActorBody order) alone on a floor: clip and
    // phase on a still actor, or armed with the shotgun raised (clip<0).
    static Game civilianInspection(int body,int clip,float phase,int view);
    static Game guardTacticsInspection(int stage=0);
    static Game guardCombatInspection(int stage=0);
    static Game guardInspection(int stage=0);
    static bool testFriendlyAI();
    static bool testStance();
    static bool testDialogue();
    static bool testStorm();
    static bool testZonePeople();
    // The stance room with its corporal mid-conversation (the --dialogue-shot frame).
    static Game dialogueInspection();
    // The stance room's squad hostile and on Alert, seen from `distance` metres (--card-shot).
    static Game cardInspection(float distance);
    static bool testAiQueryBatch();
    static bool testAiDecisionClock();
    void setAiQueryCaching(bool enabled){m_aiQueryCaching=enabled;}
    void setAiDecisionScheduling(bool enabled){m_aiDecisionScheduling=enabled;}
    void setNavAuthority(bool enabled){m_navAuthority=enabled;}
    static bool testExclusion();
    static bool testExclusionAuthoring();
    static bool testPrologue();
    static bool testCrates();
    // Reachability and placement audit of a campaign map (map-audit-<level>.txt).
    static bool auditMap(int level);
    static Game prologueInspection(int stage);
    static Game uiInspection(int view);
    static Game exclusionInspection(int stage=0);
    // A standing view anywhere in the Exclusion Zone pack: map, local x/y,
    // yaw in degrees and view pitch, on the ground (for --zone-shots).
    static Game zoneInspection(int level,Vec2 position,float yawDegrees,float pitch,float lift=0);
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
    // The major-area title card: its text and opacity (fade in, hold, fade out).
    static constexpr float AreaTitleTime=7.f;
    const std::string& areaTitle()const{return m_areaTitle;}
    float areaTitleAlpha()const{if(m_areaTitle.empty())return 0;float t=m_areaTitleTime;return std::clamp(std::min(t/1.5f,(AreaTitleTime-t)/2.f),0.f,1.f);}
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
    // The torch lights with the item, its switch (flashlight_on) and power:
    // inside a jamming DarkZone it sputters for FlashlightJamTime, then dies.
    float playerExposure()const{return m_playerExposure;}
    // How long a guard who has the player must be on the radio before his squad knows.
    static constexpr int RadioCallMs=1500;
    void updateExposure(float dt);
    // E against a bare wall: two knocks a guard on the other side will come to.
    void knockOnWall();float m_lastKnock=-10;
    // A knocked-out guard the player is dragging by the collar (not saved: the
    // body's own pose is, every step). -1 when the hands are free.
    int m_draggedNpc=-1;
    int nearbyBody()const;
    void dropBody();
    bool draggingBody()const{return m_draggedNpc>=0;}
    // Hiding in a steel locker (Stealth.cpp). The fixture the player is shut
    // in (-1 when out; state "player_locker", so it saves), the one in front of
    // them they could climb into, and getting in and out. A guard who saw them
    // climb in keeps guard_locker and pulls them out at its doors. However the
    // player comes out, forgetLocker drops the record and every guard's memory
    // of it. lockerExit squeezes further afield when the plain spots are taken.
    int hiddenLocker()const;
    int lockerAhead()const;
    bool enterLocker();
    bool leaveLocker();
    void forgetLocker();
    bool lockerExit(const Fixture& locker,Vec2& at,float& feet,bool squeeze=false)const;
    bool updateLocker(const InputState& input);
    bool pullFromLocker(int level,size_t actor,ActorPose& pose);
    static bool testLockers();
    // Locker doors (LockerDoors.cpp): each locker's two leaves swing on their
    // hinges. Gameplay commits stay instant; the doors and the player's view
    // play the motion over the following fraction of a second.
    enum class LockerMotion {Enter,Exit,Caught,Stow,Knock};
    void animateLockerDoor(int level,int fixture,LockerMotion motion);
    // How far a leaf stands open: 0 shut, 1 at its full swing (leaf 0 left, 1 right, seen from outside).
    float lockerDoorOpen(int level,int fixture,int leaf)const;
    // The camera: the player's pose, or while a locker's choreography plays the
    // path from where they stood to where the instant commit put them. Shut in,
    // the eye sits behind the leaves while the record keeps the body at the doors.
    Player view()const;
    // True while a path holds the controls (enter, step out, the first half second of being pulled out).
    bool lockerViewLocked()const;
    // How much of the view the shut leaves cover from inside: 0 open, 1 shut in.
    float lockerShutView()const;
    // The locker whose leaves the camera looks out through, or -1.
    int lockerViewFixture()const;
    // LMB shut in a locker: a rattle of the leaves and a knock guards come to.
    void knockInLocker();
    // The hands while a locker path or a stow plays (world grips, right then
    // left, as takedownGrips): pulling the leaves open and shut on the way in,
    // palms shoving them on the way out, shoving a body in. False, no hands.
    bool lockerHands(std::array<Point3,2>* grips=nullptr)const;
    // Stepping out into a guard in the leaves' swing who is not on to the
    // player: the doors are flung into him and he goes down (the actor, or -1).
    int lockerBurstTarget()const;
    // --map-shots "burst": the nearest guard stands at the doors checking them, the player bursts out.
    bool stageLockerBurst();
    // The gun put away for the way in, raised again once the way out ends.
    bool m_lockerRearm=false;void lockerRearm();
    // Transient, never saved: a save mid-motion loads at the committed end.
    // lead: how long the leaves wait for the hands to reach them (Enter).
    struct LockerDoor {int level=-1,fixture=-1;LockerMotion motion=LockerMotion::Knock;float age=0,length=0,lead=0;std::array<float,2> from{},angle{},limit{};bool shut=true;};
    std::vector<LockerDoor> m_lockerDoors;
    struct LockerView {bool active=false;LockerMotion motion=LockerMotion::Enter;float age=0,length=0;Player from;int fixture=-1;float facePitch=0;bool burst=false;};
    LockerView m_lockerView;
    void startLockerView(LockerMotion motion,const Player& from,int fixture,float facePitch=0);
    void updateLockerDoors(float dt);
    void clearLockerMotion(bool doors);
    // --map-shots "caught": the nearest guard here steps to the doors and hauls the player out.
    bool stageLockerCatch();
    // Guards and lockers (ai/guard/Lockers.cpp). A guard is a witness only if
    // he watched the player go in for a quarter second of the entry's first
    // 0.65 s and already half had them; anyone else who glimpsed it checks the
    // floor in front. The watch is transient (m_lockerWatch: per guard, when the
    // current unbroken look began, and his verdict), and so is when each guard
    // last had the player confirmed (for the hot-dive rule); what they decide
    // saves: guard_locker (fixture+1), guard_check (a suspect check, not a
    // sighting), guard_check_ms/guard_check_said/guard_doors_ms (the bark lead
    // and the pause at the doors), guard_haul (the reach for the handle, ms)
    // and guard_checked (one check per search).
    static constexpr int LockerHaulMs=350,LockerCheckLeadMs=1500,LockerCheckPauseMs=1000,LockerStowMs=1400;
    struct LockerWatch {int locker=-1;float started=-10;Point3 body{};float height=0,eye=0,exposure=0;bool crouched=false;std::vector<float> since;std::vector<std::uint8_t> verdict;std::vector<float> hadPlayer;float dashAt=-10;bool dash=false;};
    LockerWatch m_lockerWatch;
    void watchLockerEntry(int locker);
    // The entry latch's hearing radius: an arm's length (1.2 m) walked in,
    // five metres when the player ran at the doors (dash: faster than a walk
    // within 0.3 s of the latch). A guard who hears a locker's doors worked
    // checks that locker (lockerSuspectNoise); so does one whose confirmed
    // search ends within 1.5 m of a locker's doors, and, on a hunch, one whose
    // unconfirmed search does while his squad is on Caution (lockerAtSearch).
    float lockerEntryHeard()const;
    int lockerDoorNoise(const AiStimulus& noise)const;
    bool lockerAtSearch(int level,size_t actor);
    static bool lockerHunch(size_t actor,int locker,int x,int y);
    void suspectLocker(size_t actor,int locker);
    void lockerWitnessSense(int level,size_t actor,const ActorPose& pose);
    bool lockerSuspectNoise(int level,size_t actor,const ActorPose& pose,const AiStimulus& noise);
    int guardAtLocker(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove,bool free);
    void clearGuardLocker(size_t actor);
    void validateGuardLocker(size_t actor)const;
    Point3 guardLockerHandle(size_t actor,float& reach)const;
    // Bodies stowed in lockers: guard_stowed (fixture+1), guard_stowed_level
    // (+1) and guard_stow_ms (the slide in; at LockerStowMs he is shut in and
    // out of sight). One body to a locker, and nobody hides in that one.
    int lockerBody(int level,int fixture)const;
    bool npcStowed(size_t actor)const;
    int stowLockerAhead()const;
    bool stowBody();
    void updateStowedBody(size_t actor,ActorPose& pose,float dt);
    void tumbleFromLocker(int level,int fixture,size_t finder);
    // Every door motion a guard or a stow asks for goes through here, counted for the tests.
    void lockerDoorsMove(int level,int fixture,LockerMotion motion);int m_lockerDoorCalls=0;LockerMotion m_lockerDoorLast=LockerMotion::Enter;
    static bool testLockerGuards();
    // --map-shots "caught"/"stow": the nearest guard comes for the locker the
    // player just hid in, or is knocked out and dragged to the one they face.
    // staged: he starts a few steps off the doors ("reach"), and with nobody
    // hiding he checks the locker a body was stowed in ("tumble").
    void inspectLockerGuard(bool stow,bool staged=false);
    // --map-shots "watch": the eye moved to a fixed observer for the frame drawn
    // (feet -999 the floor there), the scene simulated no further.
    void inspectLockerWatch(Vec2 at,float feet,float yaw,float pitch);
    // A guard's read on the player for the HUD: 0 unaware, up to 1 while
    // suspicion builds (recognition over recognitionMs), 2 once he has you,
    // 3 while he hunts the player he had confirmed, .5 checking a noise.
    float guardAwareness(size_t actor)const;
    // His card over his head: which glyph, how far the '?' has filled, how
    // long the '!' has been up (it pops for its first ~100 ms), and how far
    // through a radio call he is (0 when not calling).
    enum class GuardCardKind{None,Making,Checking,Hunting,Alert};
    struct GuardCard{GuardCardKind kind=GuardCardKind::None;float fill=0;int bangMs=0;float call=0;};
    GuardCard guardCard(size_t actor)const;
    // Lockers and bodies (ai/guard/Lockers.cpp). A man bound for a locker's
    // doors (a witness, a suspect check) wears the full '?' of a hunt: the doors
    // are about to open. The burst (leaveLocker): guard_burst (fixture+1) holds
    // the man at the doors until a leaf's edge reaches him; guard_burst_ms runs
    // from the hit, the push back easing out under him, until he lands.
    GuardCard guardLockerCard(size_t actor)const;
    bool lockerBurstPending(size_t actor)const;
    void updateLockerBurst(float dt);
    // The fall clip's phase at which his shoulders stand `lift` (0 down, 1
    // standing) of their full height, so a rise or a slump moves at an even pace.
    static float fallPhaseAtLift(ActorVisual visual,float lift);
    // The collar of a body (midway between his shoulders, where a hand takes
    // him), for the hands that stow him.
    Point3 npcCollar(size_t actor)const;
    // A confirmed hunt for the player is searched at least HuntSearchMs, so
    // EVASION's clock is the time the hunters really give it.
    static constexpr int HuntSearchMs=20000;
    int guardSearchMs(size_t actor)const;
    // A radio call is a committed action: guard_call is what he is reporting
    // (1 the player seen, 2 a body), guard_radio how long he has been on it.
    // Only a choke, a knockout, death or a fresh hit cuts it short. A sighting
    // that lands raises the alarm flag; a body that lands puts his squad on Caution.
    static constexpr int CallSighting=1,CallBody=2;
    void startGuardCall(size_t actor,int kind);
    bool guardCalling(size_t actor)const;
    // EVASION: a squad still hunting the player it had confirmed. The longest
    // search left (ms), smoothed so the clock only runs down within one hunt; 0 when none.
    int guardEvasion()const;int guardEvasionPeak()const{return std::max(m_evasionPeakMs,guardEvasion());}
    // The HUD keeps ALERT up this long after the last man loses sight of the player.
    static constexpr int AlertLatchMs=2000;
    // Caution: when a guard who had the player gives up his search, his squad
    // (or he alone, with no squad) stays wary this long: quicker to make the
    // player out, a little longer sighted, shorter at their posts. An Alert ends it.
    static constexpr int CautionMs=45000;
    int guardCaution(size_t actor)const;
    // Caution's longer sight, which a man in Alert keeps: guardCanSee, body
    // spotting and the CCTV radar's wedges all read it.
    bool guardWary(size_t actor)const;
    void startGuardCaution(size_t actor);
    // The HUD's alarm line: -1 while a guard in any resident chunk is in Alert
    // with the player as his target (and for AlertLatchMs after he loses them
    // while the hunt goes on), -2 during EVASION (guardEvasion has the clock),
    // else the longest Caution (ms) of a squad hostile to the player with a
    // man in this chunk, else 0.
    static constexpr int AlarmAlert=-1,AlarmEvasion=-2;
    int guardAlarm()const;
    static bool testCaution();
    static bool testAlarm();
    bool flashlightOn()const{return hasFlashlight()&&state(stateId("flashlight_on"))!=0&&!flashlightJammed();}
    static constexpr float FlashlightJamTime=1.5f;
    bool flashlightJammed()const;
    // Seconds inside an active jamming DarkZone, capped at FlashlightJamTime; 0 outside.
    float flashlightJam()const;
    static bool testFlashlight();
    // Switchable lamps (WorldLight gating, mirroring Hazard). lampSwitchedOn is
    // the state gate alone; lampPower adds the timed window and flicker (0..1).
    bool lampSwitchedOn(const WorldLight& lamp)const;
    float lampPower(const WorldLight& lamp)const;
    bool darkZoneActive(const DarkZone& zone)const;
    // 0 lit .. 1 true darkness: the player's eye inside an active DarkZone of
    // the current map, eased over about .3 s (and immediate after a load).
    float darkness()const;
    // The player's muzzle flash as a light (0 none): about three frames lit; in
    // darkness it holds .12 s, burns brighter and fades out over .2 s after.
    float muzzleFlash()const;
    // NPC gun flashes lighting the room in darkness: two strongest, world x, y, z, strength.
    std::array<std::array<float,4>,2> weaponFlashes()const;
    static bool testDarkness();
    bool inventoryOpen()const{return m_inventoryOpen;}
    // A conversation with choices is open (DialogueNode): the world runs on,
    // the player stands and picks a reply with 1-4, the arrows or the mouse.
    bool talking()const{return m_talk.actor>=0;}
    // The campaign's ash storm (StormCycle): its phase (0 calm, 1 warning, 2 storm),
    // how far it has closed in (0-1), the ash falling (0-1) and whether the player is under a roof.
    const StormCycle* stormCycle()const;
    int stormPhase()const;
    float stormIntensity()const;
    float ashFall()const;
    bool sheltered()const;
    void updateStorm(float dt);
    // The conversation panel's layout in HUD pixels (DisplayWidth x DisplayHeight), shared with the mouse.
    static constexpr int DialoguePanelY=244,DialogueFirstReplyY=294,DialogueReplyRowHeight=14;
    struct DialogueView {std::string speaker,line;std::vector<std::string> replies;int selected=0;float age=0;};
    std::optional<DialogueView> dialogueView()const;
    // The faction this actor fights as now: its declared one, or what its squad's stance makes it.
    AiFaction npcFaction(const ActorTrack& track)const;
    SquadStance npcStance(const ActorTrack& track)const;
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
    static Game creaturePreview(Enemy::Kind kind,int view);
    void posePreviewCreature(int clip,float phase);
    static Game terminalInspection(int level,int terminal=0);
    static Game mapInspection(Vec2 position,float angle,float pitch=0,int level=0,bool openDoors=false,float height=-999,bool sceneryOnly=false,WorldId id=WorldId::Campaign);
    static Game barrelInspection(float age);
    // --perf-profile (PerfProfile.cpp): a campaign view with god mode on (a run
    // never ends on a dead player), creatures added on the floor under him, every
    // hunter woken onto the player (perfWake each frame holds them on him), and
    // a one-line census of what is resident and awake. perfCensus lists every map.
    static Game perfScene(int level,Vec2 position,float feet,float yawDegrees,float pitch,bool openDoors=false);
    void perfClearCreatures();
    int perfSpawn(CreatureKind kind,int count,float minimumDistance);
    void perfWake(bool guards);
    void perfHold();
    int perfSpawnProps(int count);void perfShoveProps();
    std::string perfPopulation()const;
    static void perfCensus(std::ostream& out);

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
    std::string savedLabel(const std::filesystem::path& path,const char* invalid)const;
    std::string encodeSave()const;
    bool decodeSave(const std::string& data);
    // Builds the game a save describes into `next` without touching this one
    // (it only reads this session's settings); decodeSave commits it.
    bool prepareLoad(const std::string& data,Game& next)const;
    void adoptSession(Game& next)const;
    bool beginLoad(std::string data,int slot);
    void finishLoad();
    bool loadReady()const;
    std::shared_ptr<LoadJob> m_loadJob;bool m_asyncLoads=false;
    // Returns how many chunks the archive carried; later ones are authored fresh.
    template<class Archive> int archiveSave(Archive& archive,int version=2);
    bool nearReactorDisk()const;
    void useReactorAction(int action);
    World* reactorWorld();
    void syncReactorComplex();
    void updateConsole(const InputState& input);
    void executeConsole(std::string command);
    bool m_consoleOpen=false,m_previousConsole=false,m_consoleUp=false,m_consoleDown=false,m_showFps=false,m_god=false,m_peaceful=false,m_unlockAll=false;
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
    // Darkness ramp and torch jam; runtime only, re-settled after any load.
    float m_darkness=0,m_flashlightJam=0;bool m_darknessSettled=false;
    // How lit the player is (0 black to 1 floodlit): lamps in reach with a
    // clear line, the map's ambient, true darkness and the flashlight.
    float m_playerExposure=1,m_exposureTimer=0;
    float darknessTarget(bool* jamming)const;
    void updateDarkness(float dt);
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
    // New Game: the built-in campaign from its first map. A custom prologue
    // requests this through ENTER_CAMPAIGN; it applies at the next update.
    void startMainCampaign(int level=0);
    // Title New Game: the embedded Security Office prologue, then Map 0.
    void startNewGame();
    void startCustomCampaign(std::shared_ptr<const CustomCampaign> campaign);
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
    // Index of the door in chunk `level` across the boundary from this chunk's door, or -1.
    int boundaryPartner(int level,const Door& door)const;
    void updateClutter(const InputState& input,float dt);
    bool interactClutter(int hand=-1);
    float vrInteractionDistance(Point3 target,float housingRadius)const;
    int nearbyInteractionDoor()const;
    int nearbyClutter(int hand=-1)const;
    float clutterPathFraction(Point3 from,Point3 to,Point3&normal)const;
    void seedClutter();
    bool damagePane(int level,int index,float amount,Point3 at,Point3 direction);
    void updateBreakables();
    void simulateClutter(std::vector<Clutter>& list,int level,float dt,int held);
    void knockClutter(int level,size_t index,Point3 at,Point3 impulse);
    bool damageClutter(int level,size_t index,float amount,Point3 at,Point3 direction);
    void breakClutter(int level,size_t index,Point3 direction);
    bool tryMove(Vec2 delta);
    bool hullFits(Vec2 position,float feet,float height)const;
    bool unstickPlayer();
    float groundHeight(Vec2 position)const;
    float groundHeight(Vec2 position,float feet)const;
    void updateMovement(const InputState& input,float dt);
    void updateLift(float dt);
    void updateInteraction(const InputState& input,float dt);
    // First footstep of the set for a surface: painted zone ground gives dirt,
    // grass or gravel; elsewhere metal floors ring and the rest is concrete.
    static Sound stepSoundAt(const World& world,Vec2 position);
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
    // Perception runs in the map the actor stands in; its track and restraint
    // state belong to its home map, which differs once an escort crosses a seam.
    ActorPose friendlyAwareness(const World& world,int level,const ActorTrack& track,int homeLevel,size_t homeActor,const ActorPose& old,ActorPose pose,float dt)const;
    ActorPose updateFriendlyAI(World& world,int level,size_t actor,const ActorTrack& track,const ActorPose& old,ActorPose pose,float dt,const std::vector<AiStimulus>* stimuli=nullptr);
    void guardIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<AiStimulus>& stimuli,Vec2& goal,float& goalZ,bool& wantsMove);
    void guardBaseIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    void planGuardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
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
    bool advanceNavGraph(int level,float height,size_t pairBudget);
    void prebuildNavGraph();
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
    StateId guardCautionKey(size_t actor)const;
    void updateGuardCaution(float dt);int m_cautionMicros=0;
    // The HUD's ALERT latch and EVASION clock (not saved: both rebuild from the guards).
    // m_alarmSting spaces the detection sting and the '?' tick when several men turn at once.
    int m_alertLatchMs=0,m_evasionMs=0,m_evasionPeakMs=0;float m_alarmSting=0,m_alarmTick=0;
    int guardHuntLeft(size_t actor)const;bool guardAlarmActor(size_t actor)const;
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
    // AI state holds enemies as (kind, level+1, index+1). When a chunk's list
    // changes, moved[i] says where its entry i is now; references follow it.
    void remapEnemyRefs(int level,const std::vector<AiTargetRef>& moved);
    void followEnemyRef(const std::function<StateId(std::string_view)>& key,std::string_view prefix,int level,const std::vector<AiTargetRef>& moved);
    void remapDroneEnemyRefs(int level,const std::vector<AiTargetRef>& moved);
    void remapGrenadeEnemyRefs(int level,const std::vector<AiTargetRef>& moved);
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
    bool friendlyWalkSegmentUncached(int level,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
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
    // How a squad is holding up: 1 steady, 2 shaken (a man down or the speaker badly hurt), 3 broken (half the squad down).
    int squadMood(size_t actor)const;
    void stanceStruck(size_t actor,AiTargetRef attacker);
    void setSquadStance(size_t actor,SquadStance stance);
    void updateStances(float dt);
    int nearbyTalker()const;
    bool openDialogue();
    void updateDialogue(const InputState& input,float dt);
    void closeDialogue();
    std::vector<int> dialogueReplies()const;
    const DialogueNode* dialogueNode()const;
    struct TalkState {int actor=-1;StateId node=0;int selected=0;float age=0;bool primed=false;InputState previous{};} m_talk;
    void requestAiDoor(World& world,int level,Vec2 position,float feet,Vec2 goal);
    bool aiDoorLocked(const World& world,int level,const Door& door)const;
    int hostilesRemaining(int level)const;
    void damageFriendly(size_t actor,float amount,Vec2 source,AiTargetRef attacker={});
    void migrateEnemiesAcrossChunks();
    void updatePickups();

    World m_world;
    Ragdoll m_hazmat;
    float m_hazmatPushCooldown=0;
    // How long the player has leaned on a heavy crate (it goes at ShoveSeconds).
    float m_crateShove=0;
    void updatePushCrates(const InputState& input,float dt);
    bool crateFree(const World& w,size_t index,Vec2 at)const;
    void restoreCrates(World& w,int level)const;
    Player m_player;
    std::vector<Enemy> m_enemies;
    std::vector<FriendlyActor> m_friendlyActors;
    std::vector<AiStimulus> m_guardShots;
    std::vector<Pickup> m_pickups;
    std::vector<Clutter> m_clutter;
    std::vector<BulletImpact> m_bulletImpacts;
    std::vector<BarrelExplosion> m_barrelExplosions;
    std::vector<BreakEvent> m_breakEvents;
    // Barrels caught in a blast go up after a short fuse.
    std::vector<BarrelExplosion> m_pendingBarrels;
    void detonateBarrel(int level,int tx,int ty);
    int m_heldClutter=-1;bool m_propHalfStep=false;
    int m_npcNavBudget=4;
    bool m_aiQueryCaching=true;
    bool m_aiDecisionScheduling=true;
    bool m_navAuthority=true;
    bool m_vrAimValid=false,m_vrSupportGrip=false,m_vrChamberEmpty=false,m_vrPumpBack=false;float m_vrPumpRest=0,m_vrPumpAlong=0;
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
    Vec2 m_velocity{};float m_stuckTime=0;
    float m_damageFlash = 0.0f;
    float m_stormHurtTimer=0;
    float m_hitStopRemaining=0.f;
    float m_elapsed = 0.0f;
    float m_grabbed=0; // Seconds a Snatcher still has hold of the player: slowed, no sprint.
    std::unordered_map<StateId,float> m_sequenceEnded; // Game time each one-shot reached its end; not saved.
    int m_kills = 0;
    bool m_won = false;
    bool m_pendingMainCampaign = false;
    bool m_handsLowered = false;
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
    std::string m_areaTitle;
    float m_areaTitleTime=0;
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
