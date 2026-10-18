#pragma once
// Game — orchestrator.  Lightweight header: full ECS/Systems come in
// via the .cpp.
#include <SDL.h>
#include <SDL_mixer.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstddef>
#include "Config.h"

class AssetManager;
class Entity;
class Map;
class Manager;
class SpriteComponent;
class UILabel;

class Game {
public:
    enum class State { Menu, Instructions, Playing, Paused,
                       Settings, Stats, GameOver, Victory, LevelComplete,
                       Achievements,                         // Batch L
                       Title };                              // Batch Q

    Game();
    ~Game();

    void init(const char* title, int width, int height, bool fullscreen);
    void handleEvents();
    void update();
    void render();
    void clean();

    bool running() const { return isRunning; }

    State state = State::Menu;
    int   gameScore = 0;
    int   coinsCollected = 0;
    int   totalCoins = 0;
    int   lives = 3;
    int   keys  = 0;
    int   maxLives = 3;
    bool  isPaused = false;
    int   menuSelection = 0;
    bool  muted = false;

    // ---- settings ----
    int   musicVolume = 48;      // 0..128, SDL_mixer scale
    int   sfxVolume   = 72;      // 0..128
    int   settingsSelection = 0; // 0 music, 1 sfx, 2 mute, 3 back

    // ---- run stats ----
    int   enemiesDefeated = 0;
    int   deaths = 0;
    float runTime = 0.f;         // seconds spent actually playing
    int   bestScore = 0;         // persisted between runs
    bool  reachedCheckpoint = false;
    bool  doorUnlocked = false;
    int   checkpointIndex = -1;
    // Batch B: where respawn() puts the hero; checkpoints move it forward.
    float respawnX = Config::PLAYER_X, respawnY = Config::PLAYER_Y;

    bool  isGrounded = false;
    bool  wasGrounded = false;
    float coyoteTimer = 0.f;
    float jumpBufferTimer = 0.f;
    int   jumpsRemaining = 2;
    bool  dashActive = false;
    float dashTimer = 0.f;
    float dashCooldown = 0.f;
    // Batch C: slide and wall movement
    float slideT = 0.f, slideCooldown = 0.f;
    int   slideDir = 1;
    bool  prevDownKey = false;
    float wallJumpLockT = 0.f;
    bool  wallSliding = false;
    float wallDustT = 0.f;
    // Batch C: parry
    float parryT = 0.f, parryCooldown = 0.f;
    bool  parryHit = false;           // the current window already landed
    int   parriesLanded = 0;          // run stat (and what the harness reads)
    // Batch C: grappling hook
    bool  hooked = false;
    float hookX = 0.f, hookY = 0.f, ropeLen = 0.f;
    float noJumpCutT = 0.f;           // after a fling, keep its height
    int   grapplesUsed = 0;
    // Batch D: the landing crouch (a drawn frame, not a pixel-stretching squash)
    float landT = 0.f;
    // Batch N: transitions - the input direction, and short run-start / turn timers
    int   inputDir = 0;
    float runStartT = 0.f, turnT = 0.f;
    bool  wasRunning = false, lastFacing = true;
    // Batch G2: red glow at the screen edges when the hero is hurt
    float hurtFlashT = 0.f;
    // Batch H: progress - the furthest stage unlocked (0-based, saved), and
    // the stage the menu's CONTINUE will start
    int unlockedStage = 0;
    int menuStage = 0;
    int menuItems() const { return unlockedStage > 0 ? 6 : 5; }     // Batch L: + ACHIEVEMENTS
    void startRunAt(int stage);
    // Batch J: the moving platform the hero stands on (sticky ride)
    Entity* ridePlat = nullptr;
    float   rideOffX = 0.f;          // hero x - platform x while riding (exact)

    // ---- Batch K: guidance -------------------------------------------------
    // Hint dock: what to show now, faded in and out; core moves retire once
    // used three times ("learned", saved).  Mode: 0 Smart, 1 All, 2 Off.
    enum Learn { L_MOVE, L_JUMP, L_SWORD, L_BOLT, L_DASH, L_SLIDE, L_PARRY, L_GRAPPLE, L_COUNT };
    int      hintMode = 0;
    unsigned learned = 0;
    int      useCount[L_COUNT] = {};
    void     noteUsed(int what);
    struct Hint { std::string keys, label; float alpha = 0.f; bool want = false; };
    std::vector<Hint> hints;                  // stable order; fade state kept per hint
    std::vector<std::string> shownHints;      // labels wanted this frame (for the harness)
    void     updateHints();
    void     drawHintDock();
    // Tutorial scrolls (stage 1): a signpost that unrolls a parchment near it.
    struct Sign { int col = 0; float x = 0, y = 0; std::string title, line1, line2; float open = 0.f; };
    std::vector<Sign> signs;
    void     buildSigns();
    void     drawSigns();
    float    nearestSignOpen() const;

    // ---- Batch L: achievements and tasks (saved in dungeonrush.progress) ----
    struct Ach  { const char* id; const char* title; const char* desc; const char* stat; int target; };
    struct Task { const char* text; const char* stat; int target; bool atClear; bool atMost; };
    static const std::vector<Ach>& achList();
    static const Task* tasksFor(int stage);            // three per stage
    std::map<std::string, int> life;                   // lifetime counters
    std::set<std::string> unlocked;                    // achievement ids
    unsigned signsRead = 0;                            // tutorial scrolls read (bit per sign)
    int      tasksDone[3] = { 0, 0, 0 };               // per stage, bit per task
    struct StageRun { int coins = 0, kills = 0, parries = 0, grapples = 0, hits = 0, deaths = 0; float time = 0.f; };
    StageRun srun;
    int      snapCoins = -1, snapParries = 0, snapGrapples = 0, snapDeaths = 0;
    bool     snapDoor = false;
    struct Toast { std::string head, text; float t = 0.f; };
    std::vector<Toast> toasts;
    int      achScroll = 0;
    void     lifeAdd(const std::string& k, int n = 1);
    void     lifeMax(const std::string& k, int v);
    int      achValue(const Ach& a) const;
    int      taskValue(int stage, const Task& t) const;
    void     resetStageRun();
    void     updateProgress();                         // every playing frame
    void     onKill(const std::string& tag);
    void     onStageClear();
    void     checkAchievements();
    void     evalTasks(bool atClear);
    void     loadProgress();
    void     saveProgress();
    void     drawToasts();
    void     drawAchievements();
    void     drawTasksPanel();
    // ---- Batch M: stage results ---------------------------------------------
    // Stars: 1 for the clear, 2 with two of the stage's tasks done, 3 with all
    // three.  Bests per stage live in `life` as best.stars.N / best.time.N
    // (deciseconds), so they are saved with the rest of the progress.
    float    resultsT = 0.f;
    int      resultStars = 0, starsShown = 0;
    bool     newBestTime = false;
    int      bestStars(int stage) const;
    float    bestTime(int stage) const;
    // ---- Batch P: weather and ambient sound ---------------------------------
    std::string ambientId;                 // the loop playing on channel 0 ("" = none)
    void     playAmbient(const char* id);
    int      weatherKind() const;          // 0 none, 1 rain (stage 2), 2 ash (stage 3)
    void     drawWeather();
    // ---- Batch Q: title screen, logo, HUD toggle ------------------------------
    SDL_Texture* m_logo[2] = { nullptr, nullptr };       // big (title), small (menu)
    int      m_logoW[2] = { 0, 0 }, m_logoH[2] = { 0, 0 };
    void     buildLogo();
    void     drawLogo(int cx, int y, bool big);
    void     drawTitle();
    void     drawIllustration(int cx, int groundY, int scale);
    void     drawPixelIcon(int which, int x, int y, int scale, SDL_Color c);
    bool     hudHidden = false;
    float    hudNoticeT = 0.f;
    void     toggleHud();
    // ---- Batch P2: props that react (lifted out of the props tile layer) ----
    struct RProp { int id = 0; float x = 0, y = 0; int hp = 1; float sway = 0, swayV = 0, shake = 0;
                   bool gone = false; int index = 0; float leafT = 0; };
    std::vector<RProp> rprops;
    int      propsBroken = 0;
    int      coinsDropped = 0;            // coins props have dropped (for the harness)
    void     buildReactiveProps();
    void     updateReactiveProps();
    void     drawReactiveProps();
    void     spawnCoinAt(float cx, float cy);
    void     drawTextA(const char* fontId, const char* text, int x, int y, SDL_Color c, Uint8 alpha, bool centered);
    int      textWidth(const char* fontId, const char* text);
    void pickRidePlatform();
    void rideMovingPlatform();
    float lastHeroHp = -1.f;
    void addShake(float m) { m_shakeMag += m; }
    float dashTrailTimer = 0.f;     // paces the dash afterimages
    float speedBoostTimer = 0.f;
    float invincibleTimer = 0.f;
    float iframeTimer = 0.f;
    float dmgBoostTimer = 0.f;
    float magnetTimer = 0.f;        // PowerUpType::Magnet  — coin homing
    float coinMultTimer = 0.f;      // PowerUpType::CoinX2  — double coin value
    int   shieldCharges = 0;        // PowerUpType::Shield  — hits absorbed
    int   coinsBanked = 0;          // coins taken this run (for the stats screen)
    int   comboStep = -1;
    float comboTimer = 0.f;
    float attackCooldown = 0.f;
    float attackTimer = 0.f;
    bool  facingRight = true;

    // ---- mana (spent on ranged attacks) ----
    float mana        = Config::MANA_MAX;
    float manaDelay   = 0.f;      // regen pause after a spend
    float chargeT     = 0.f;      // seconds the ranged key has been held
    bool  chargeFired = false;    // the heavy bolt already went out this hold

    // ---- combat feel ----
    float hitStop     = 0.f;      // freeze-frame timer; >0 pauses simulation
    int   hitCombo    = 0;        // consecutive landed hits
    float hitComboT   = 0.f;
    int   bestCombo   = 0;
    bool  airDashUsed = false;    // one air dash per jump, refreshed on landing
    bool  diving      = false;    // mid dive-slash

    // ---- menus ----
    int   pauseSelection   = 0;   // 0 resume, 1 restart, 2 settings, 3 stats, 4 quit
    int   instructionsScroll = 0;
    bool  prevMeleeDown  = false;
    bool  prevRangedDown = false;

    static const std::size_t groupMap          = 0;
    static const std::size_t groupPlayers      = 1;
    static const std::size_t groupEnemies      = 2;
    static const std::size_t groupColliders    = 3;
    static const std::size_t groupProjectiles  = 4;
    static const std::size_t groupCoins        = 5;
    static const std::size_t groupPowerups     = 6;
    static const std::size_t groupPlatforms    = 7;
    static const std::size_t groupParticles    = 8;
    static const std::size_t groupLabels       = 9;
    static const std::size_t groupHazards      = 10;
    static const std::size_t groupFacilities   = 11;
    static const std::size_t groupHitboxes     = 12;
    static const std::size_t groupAfterimages  = 13;

    static SDL_Renderer* renderer;
    static SDL_Event     event;
    static SDL_Rect      camera;
    static AssetManager* assets;
    static bool          isRunning;

    static float clamp(float v, float lo, float hi);

private:
    void loadAssets();
    void loadLevel();
    void spawnPlayer();
    void spawnEnemy(const std::string& kind, int col, int row);
    void spawnPowerup(const std::string& kind, int col, int row);
    void spawnFacility(const std::string& kind, int col, int row);
    void spawnHazard(const std::string& kind, int col, int row, int w, int h);
    void spawnPlatform(int col, int row, int w, int h,
                       bool moving, bool vertical, float range, float speed);

    void handleMenuInput(SDL_Keycode key);
    void handleSettingsInput(SDL_Keycode key);
    void handlePauseInput(SDL_Keycode key);
    void handlePlayerInput();
    void startJump();
    void startDash();
    void emitDashTrail(float dt);
    void updateHeroState();
    void checkLevelState();
    void respawn();
    int  wallContact() const;          // -1 wall on the left, +1 right, 0 none
    void setSlideHitbox(bool low);
    bool fullHeightFits() const;
    void startParry();
    SDL_Rect parryZone() const;
    void stepParry();                  // stagger enemies in the zone
    bool tryReflect(Entity& shot);     // true: the shot was parried
    void drawParryShield();
    void drawHurtFlash();
    bool nearestHook(float& hx, float& hy) const;
    void startGrapple();
    void releaseGrapple(bool fling);
    void applyRope();
    void drawGrapple();
public:
    // Batch B: stages (public: the harness and a future level select use it)
    void loadStage(int n);
    std::string stagePath(const char* file) const;
    float stageSpawnX() const;
    float stageSpawnY() const;
    float stageBannerT = 0.f;      // seconds left on the "Stage N" banner
private:
    void addLockedDoors(std::vector<SDL_Rect>& solids);
    void restartLevel();

    // ---- attacks -------------------------------------------------------
    // Melee and ranged are separate inputs now (J = melee combo / directional
    // melee, K = bolt, hold K = piercing heavy bolt).  handleAttacks() picks
    // the move, performAttack() executes a row from Config::ATTACKS.
    void handleAttacks();
    void performAttack(Config::AttackKind kind);
    void stepMana();
    void updatePlayerAnimation();

    // Called for every hit the hero lands — drives the hit-combo counter, the
    // score multiplier and the freeze-frame hit-stop.
    void registerHit(bool killed);
    float comboMultiplier() const;

    // Floating damage numbers.
    void popNumber(float x, float y, int amount, bool crit);
    void stepNumbers();
    void drawNumbers();

    void stepPlayerPhysics();
    void stepEnemyPhysics();
    void stepEnemies();
    void drawHUD();
    void drawMenu();
    void drawMenuBackdrop();
    void drawInstructionsPanel(bool overWorld);
    void drawWorld();
    void drawMinimap();
    void drawSettings();
    void drawStats();

    // Shared overlay pieces (see render() — it presents exactly once).
    void drawScrim(Uint8 a);
    void drawPauseOverlay();
    void drawGameOverOverlay();
    void drawVictoryOverlay();

    // Small text helper — every screen used to open-hand roll the same
    // twelve lines of surface/texture boilerplate.
    void drawText(const char* fontId, const char* text, int x, int y,
                  SDL_Color c, bool centered = false);

    // Audio settings are persisted so a player's chosen volumes survive a
    // restart; see Config::SAVE_FILE.
    void applyVolumes();
    void loadSettings();
    void saveSettings();

    // Where Settings / Stats should go back to — they can be opened from the
    // title screen or mid-run, and must not dump you in the wrong place.
    State m_returnState = State::Menu;

    // ---- UI fluid animations & state (Task 2) ----
    float m_menuTime = 0.f;
    float m_menuTransition = 0.f;
    float m_instructionsScrollPos = 0.f;
    float m_buttonAnim[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };

    struct Firefly {
        float x, y;
        float vx, vy;
        float size;
        float phase;
        float pulseSpeed;
        Uint8 r, g, b;
    };
    static constexpr int N_FIREFLIES = 50;
    Firefly m_fireflies[N_FIREFLIES]{};
    bool    m_firefliesInit = false;

    void initFireflies();
    void updateUIAnimations(float dt);
    void drawKeycap(const char* key, int x, int y, int& outW);

    SDL_Window* m_window = nullptr;
    Mix_Music*  m_music = nullptr;
public:
    // Batch E: per-stage music.  sounds/music/<name>.ogg|.mp3|.wav if it exists;
    // stages fall back to sounds/background_music.wav, 'boss' only switches in
    // when its file is there.  m_musicFile is the file playing now.
    std::string m_musicFile;
    bool playMusic(const std::string& name, bool fallback);
    std::string assetFile(const std::string& rel) const;   // absolute path, as the game sees it
    // Batch F: display.  The game always draws in WINDOW_WIDTH x WINDOW_HEIGHT
    // logical pixels; SDL letterboxes that to any window or screen.
    enum class Scaling { Auto, Sharp, Fill };
    bool    fullscreen = false;
    Scaling scaling = Scaling::Auto;
    void setFullscreen(bool on);
    void applyScaling();
    // Pure rule (the harness checks it): Auto fills the screen unless a
    // whole-number scale covers >= 90 % of it; Sharp/Fill force it.
    static bool sharpFor(Scaling mode, int outW, int outH);
private:
    bool m_bossMusicOn = false;
    float       m_shake = 0.f;
    float       m_shakeMag = 0.f;
    float       m_fallSpeed = 0.f;   // previous frame's vy, for landing impact
    float       m_deathTimer = 0.f;
    float       m_victoryTimer = 0.f;

    SDL_Color colWhite {255,255,255,255};
    SDL_Color colYellow{255,230, 90,255};
    SDL_Color colRed   {255, 80, 80,255};
    SDL_Color colCyan  { 80,220,255,255};
    SDL_Color colGreen { 90,255,140,255};
    SDL_Color colShadow{  0,  0,  0,140};

    struct FloatNumber {
        float x, y, life, vy;
        int   amount;
        bool  crit;
    };
    std::vector<FloatNumber> m_numbers;
    std::vector<Config::HitEvent> m_hitEvents;
};
