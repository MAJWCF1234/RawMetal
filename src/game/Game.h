#pragma once

#include "../core/Math.h"
#include "../world/World.h"
#include <vector>
#include "../audio/Sound.h"
#include <string>
#include <string_view>
#include <cstdint>
#include "Ragdoll.h"

namespace retro {
constexpr int DisplayWidth=640,DisplayHeight=360;


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
    bool mute = false, music = false;
    bool escape=false, menuUp=false,menuDown=false,menuLeft=false,menuRight=false,menuAccept=false;
    bool inventory=false;
    int pointerX=-1,pointerY=-1;
    float mouseDx = 0.0f;
    float mouseDy = 0.0f;
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
    static constexpr float CorpseLifetime=2.4f;
    bool visible()const{return alive||deathTime<CorpseLifetime;}
    float bodyBottom()const{return z+(kind==Kind::Wasp?.55f:0.f);}
    float bodyTop()const{return z+(kind==Kind::Wasp?1.55f:kind==Kind::Brute||kind==Kind::Warden?1.85f:1.05f);}
    const char* name()const{return kind==Kind::Warden?"REACTOR STALKER":kind==Kind::Wasp?"XENOWASP":kind==Kind::Brute?"SCISSOR FIEND":"HUNTSMAN";}
};

struct Pickup {
    using Kind=PickupKind;
    Vec2 pos{};
    Kind kind = Kind::Health;
    bool active = true;
};
struct Clutter {
 Vec2 pos{},velocity{};float z=0,vz=0,yaw=0,spin=0;int kind=0;bool projectile=false;float impactCooldown=0;
 float pitch=0,roll=0,pitchSpeed=0,rollSpeed=0,restTime=0;bool sleeping=false;
 std::array<float,3> size()const{constexpr std::array<float,3> sizes[]={{.22f,.22f,.003231f},{.156383f,.25f,.054035f},{.082788f,.083595f,.28f},{.24f,.222403f,.139543f},{.263634f,.28f,.014386f},{.12f,.11894f,.006745f}};return sizes[kind];}
 std::array<float,3> rotate(float x,float y,float localZ)const{
  float cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll),cy=std::cos(yaw),sy=std::sin(yaw);
  float a=cp*x+sp*localZ,b=-sp*x+cp*localZ,c=cr*y-sr*b,d=sr*y+cr*b;
  return {cy*a-sy*c,sy*a+cy*c,d};
 }
 std::array<float,3> extent()const{auto s=size(),a=rotate(s[0]*.5f,0,0),b=rotate(0,s[1]*.5f,0),c=rotate(0,0,s[2]*.5f);return {std::fabs(a[0])+std::fabs(b[0])+std::fabs(c[0]),std::fabs(a[1])+std::fabs(b[1])+std::fabs(c[1]),std::fabs(a[2])+std::fabs(b[2])+std::fabs(c[2])};}
 float height()const{return extent()[2]*2;}
 float footprint()const{auto s=size();return std::max(s[0],s[1]);}
 Sound impactSound()const{return kind==2?Sound::JunkGlass:kind==0||kind==1||kind==5?Sound::JunkSoft:Sound::JunkMetal;}
};

struct Player {
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
    float eye=.78f;
    float hullHeight()const{return crouched?.58f:1.f;}
};
struct WeaponMotion {float yaw=0,pitch=0,bob=0,back=0,elbow=0,bolt=0,roll=0;};
struct Settings {float master=1,music=.75f,effects=1,sensitivity=1;bool invertMouse=false;};
struct MenuLayout {static constexpr int X=(DisplayWidth-304)/2,Y=(DisplayHeight-288)/2,Width=304,Height=288,RowTop=Y+46,RowHeight=21,Rows=10,SliderX=X+179,SliderWidth=75;};
struct TitleMenuLayout {static constexpr int X=66,Y=188,Width=238,RowHeight=28,Rows=5;};


class Game {
public:
    explicit Game(WorldId id=WorldId::Campaign);
    WorldId worldId()const{return m_worldId;}
    static bool testWorldIsolation();
    void showTitleScreen();
    bool titleScreen()const{return m_titleScreen;}
    int titleSelection()const{return m_titleSelection;}
    bool customMapsOpen()const{return m_customMapsOpen;}
    int titleRows()const{return m_customMapsOpen?2:TitleMenuLayout::Rows;}
    bool menuFromTitle()const{return m_menuFromTitle;}

    void update(const InputState& input, float dt);
    void restart();
    int level()const{return m_level;}
    static constexpr int ChunkCount=6;
    Vec2 chunkOffset(int level)const{return chunkDefinition(m_worldId,level).origin;}
    Game chunkView(int level)const;
    const World& worldAt(Vec2& local)const;
    bool chunkResident(int level)const{return level==m_level||m_chunks[level].resident;}

    const World& world() const { return m_world; }
    const Player& player() const { return m_player; }
    const Ragdoll& hazmat()const{return m_hazmat;}
    static Game hazmatInspection(int view=0);
    static bool testHazmat();
    const std::vector<Enemy>& enemies() const { return m_enemies; }
    const std::vector<Pickup>& pickups() const { return m_pickups; }
    const std::vector<Clutter>& clutter()const{return m_clutter;}
    bool holdingClutter()const{return m_heldClutter>=0;}
    static bool testClutter();
    static Game clutterInspection(int kind,float seconds);
    static bool testStreaming();

    float weaponKick() const { return m_weaponKick; }
    const WeaponMotion& weaponMotion()const{return m_weaponMotion;}
    float shotAge()const{return m_shotAge;}
    float holster()const{return m_holster;}
    float punchAge()const{return m_punchAge;}
    bool punchLeft()const{return m_punchLeft;}
    bool unarmed()const{return m_holster>=.98f;}
    bool guarding()const{return m_guarding;}
    static bool testUnarmed();
    static Game weaponInspection(int mode,float age=0);
    float damageFlash() const { return m_damageFlash; }
    float elapsed() const { return m_elapsed; }
    int kills() const { return m_kills; }
    bool won() const { return m_won; }
    bool dead() const { return m_player.health <= 0.0f; }
    int enemiesRemaining() const;
    float hitFlash()const{return m_hitFlash;}
    const Enemy* targetEnemy()const;
    static bool testCombat();
    static bool testWeaponMotion();
    static bool testAudioEvents();
    static bool testSettings();
    static bool testSaves();
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
    float dormantBelow()const{
     if(!m_world.hasLift()||m_player.z<-2)return -1000.f;
     if(m_world.liftPhase()==World::LiftPhase::Ready)return -4.5f;
     if(m_world.liftPhase()==World::LiftPhase::Ascending)return -4.5f-5.5f*std::clamp((m_world.liftPhaseTime()-10.f)/3.f,0.f,1.f);
     return -1000.f;
    }
    bool dormantEntity(float z)const{return m_world.hasLift()&&z<-2&&m_player.z>=-2&&m_world.liftPhase()!=World::LiftPhase::Crashed;}
    bool weaponEquipped()const{return m_weaponEquipped;}
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
    static Game stalkerInspection(int clip,float phase,int view=0);
    static Game mapInspection(Vec2 position,float angle,float pitch=0,int level=0,bool openDoors=false,float height=-999,bool sceneryOnly=false,WorldId id=WorldId::Campaign);

private:
    WorldId m_worldId=WorldId::Campaign;
    bool m_titleScreen=false,m_menuFromTitle=false,m_customMapsOpen=false;
    int m_titleSelection=0;
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
    void executeScriptAction(const ScriptAction& action);
    void spawnScriptEnemy(const ScriptAction& action);
    void spawnCreature(const CreatureSpawn& spawn,float awareness=0,bool announce=false);
    void updateHazards(float dt);
    void loadLevel(int level,bool carry);
    int m_level=0;
    struct ChunkState {World world;std::vector<Enemy> enemies;std::vector<Pickup> pickups;int kills=0;bool resident=true;std::vector<Clutter> clutter;};
    std::array<ChunkState,ChunkCount> m_chunks;
    void storeChunk();
    void crossChunkBoundary();
    void ensureChunk(int level);
    void updateStreaming(float dt);
    void useDoor(int index);
    void updateClutter(const InputState& input,float dt);
    bool interactClutter();
    int nearbyClutter()const;
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
    void reloadWeapon();
    void punchImpact();
    void receiveDamage(float amount,Vec2 source);
    void updateEnemies(float dt);
    void updatePickups();

    World m_world;
    Ragdoll m_hazmat;
    float m_hazmatPushCooldown=0;
    Player m_player;
    std::vector<Enemy> m_enemies;
    std::vector<Pickup> m_pickups;
    std::vector<Clutter> m_clutter;
    int m_heldClutter=-1;
    bool m_previousFire = false;
    bool m_previousReload = false;
    float m_weaponKick = 0.0f,m_reloadTimer=0.0f;
    float m_shotCooldown = 0.0f;
    Vec2 m_velocity{};
    float m_damageFlash = 0.0f;
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
    InputState m_menuPrevious;
    void updateMenu(const InputState& input);
    void updateInventory(const InputState& input);
    bool m_weaponEquipped=true,m_inventoryClick=false,m_inventoryUse=false;
    int m_medkits=0,m_selectedItem=-1;
    std::array<int,3> m_itemCells{12,0,2};
    void sound(Sound sound,float gain=1,float pitch=1);
    void enemySound(const Enemy& enemy,int action,float gain=1,float pitch=1);
    void updateWeaponMotion(const InputState& input,float dt);
};

}
