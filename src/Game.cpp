// Game.cpp — orchestrator.
//
// Pipeline per frame (state == Playing):
//   1. handlePlayerInput        — read keys, queue jumps/dash/attack
//   2. stepPlayerPhysics        — apply input, run Physics::moveAndCollide
//   3. applyPlatformCarry       — let platforms carry the player
//   4. stepPlatforms            — update moving platforms
//   5. stepEnemies              — AI: patrol/chase/attack, then physics move
//   6. stepEnemyPhysics         — collision against world
//   7. Pickup::step             — coins / powerups / facilities / hazards
//   8. Combat::resolveMelee     — player hitboxes vs enemies
//                                 enemy hitboxes vs player
//                                 projectiles vs targets
//   9. Particles::step          — VFX
//  10. updateHeroState          — powerup timers, iframes
//  11. checkLevelState          — death, fall, boss defeated, all coins
//  12. CameraSystem::follow
//  13. manager.refresh()        — drop dead entities, sync groups
//  14. drawWorld, drawHUD, drawMinimap

#include "Game.h"
#include "TextureManager.h"
#include "AssetManager.h"
#include "Map.h"
#include "Components.h"
#include "Level.h"
#include "Config.h"
#include "SpriteSpec.h"
#include "Collision.h"
#include "Systems/Systems.h"
#include "Systems/Parallax.h"
#include "Systems/Atmosphere.h"
#include "Systems/Scenery.h"
#include "Systems/Vfx.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <SDL_mixer.h>
#include <SDL_image.h>

#ifndef ASSETS_PATH
#define ASSETS_PATH "../assets"
#endif
static inline std::string assetPath(const std::string& rel) {
    return std::string(ASSETS_PATH) + "/" + rel;
}

extern float lastFrameTime;

// ---- globals used by the systems ----
Manager       manager;
Map*          g_map = nullptr;
SDL_Renderer* Game::renderer = nullptr;
SDL_Event     Game::event{};
SDL_Rect      Game::camera{0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT};
AssetManager* Game::assets = nullptr;
bool          Game::isRunning = false;

static inline std::vector<Entity*>& G(std::size_t idx) { return manager.getGroup(idx); }

// INV-2: ColliderComponent::init() copies w/h and the transform pointer but not
// the collider itself, so a freshly spawned entity's collider sits at (0,0)
// until the first manager.update(). Physics::moveAndCollide starts from the
// collider and writes the result back into the transform, so any entity that
// reaches physics before that pass is teleported to the world origin. Every
// spawn path therefore syncs here, at the point the position is known.
static inline void syncCollider(Entity& e) {
    if (e.hasComponent<ColliderComponent>())
        e.getComponent<ColliderComponent>().update();
}

float Game::clamp(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }

// ===========================================================================
// ctor / dtor
// ===========================================================================
Game::Game()  = default;
Game::~Game() { delete g_map; }

// ===========================================================================
// init
// ===========================================================================
void Game::init(const char* title, int width, int height, bool fullscreen) {
    int flags = 0;
    if (fullscreen) flags = SDL_WINDOW_FULLSCREEN;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n";
    }
    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                width, height, flags);
    if (!m_window) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n"; }
    renderer = SDL_CreateRenderer(m_window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer) SDL_SetRenderDrawColor(renderer, 30, 40, 60, 255);

    // cache the sky gradient once, now that a renderer exists
    Parallax::init(renderer, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
    Atmosphere::init(renderer, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
    Scenery::init(renderer, assetPath("objects.png").c_str());
    initFireflies();

    // restore the player's saved volumes before the first note plays
    loadSettings();

    if (TTF_Init() == -1) std::cerr << "TTF_Init: " << TTF_GetError() << "\n";
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1)
        std::cerr << "Mix_OpenAudio: " << Mix_GetError() << "\n";

    assets = new AssetManager(&manager);
    loadAssets();
    loadLevel();
    spawnPlayer();

    m_music = Mix_LoadMUS(assetPath("sounds/background_music.wav").c_str());
    if (m_music) Mix_PlayMusic(m_music, -1);
    isRunning = true;
}

// ===========================================================================
// assets
// ===========================================================================
void Game::loadAssets() {
    auto tex = [&](const char* id, const char* rel) {
        assets->AddTexture(id, assetPath(rel).c_str());
    };
    tex("terrain",     "tileset/platformer.png");
    tex("player",      "sprites/player_sprites.png");
    tex("skeleton",    "sprites/skeleton_sprites.png");
    tex("slime",       "sprites/slime.png");
    tex("bat",         "sprites/enemy_bat.png");
    tex("mage",        "sprites/mage.png");
    tex("archer",      "sprites/archer.png");
    tex("bomber",      "sprites/bomber.png");
    tex("boss",        "sprites/villain_boss.png");
    tex("coin",        "tileset/coins.png");
    tex("projectile",  "sprites/proj.png");
    tex("fireball",    "sprites/fireball.png");
    tex("shockwave",   "sprites/shockwave.png");
    tex("heart",       "sprites/heart.png");
    tex("platform",    "sprites/moving_platform.png");
    tex("power_health",  "sprites/power_health.png");
    tex("power_speed",   "sprites/power_speed.png");
    tex("power_invincible","sprites/power_invincible.png");
    tex("power_jump",    "sprites/power_jump.png");
    tex("power_damage",  "sprites/power_damage.png");
    tex("power_magnet",  "sprites/power_magnet.png");
    tex("power_shield",  "sprites/power_shield.png");
    tex("power_coins",   "sprites/power_coins.png");
    tex("checkpoint",  "sprites/checkpoint.png");
    tex("shrine",      "sprites/shrine.png");
    tex("key",         "sprites/key.png");
    tex("door",        "sprites/door.png");
    tex("spikes",      "sprites/spikes.png");

    auto snd = [&](const char* id, const char* rel) {
        assets->AddSound(id, assetPath(rel).c_str());
    };
    snd("coin",         "sounds/coinCollect.wav");
    snd("jump",         "sounds/playerJump.wav");
    snd("dash",         "sounds/dash.wav");
    snd("hit",          "sounds/hit.wav");
    snd("powerup",      "sounds/powerup.wav");
    snd("enemyDie",     "sounds/enemyDie.wav");
    snd("lifeLost",     "sounds/lifeLost.wav");
    snd("speedBoost",   "sounds/speedBoost.wav");
    snd("invincible",   "sounds/invincible.wav");
    snd("villainHit",   "sounds/villainHit.wav");

    // The sheet has no dedicated swing/shoot samples, so melee and the bolt
    // borrow the two shortest one-shots: a 0.12s whoosh and a 0.40s rising
    // tone.  Aliased rather than reusing the ids so the two actions stay
    // independently tunable (and replaceable) later.
    snd("swing",        "sounds/dash.wav");
    snd("shoot",        "sounds/invincible.wav");

    auto font = [&](const char* id, const char* rel, int size) {
        assets->AddFont(id, assetPath(rel), size);
    };
    font("small",  "fonts/FiraCode-Regular.ttf", 14);
    font("body",   "fonts/FiraCode-Regular.ttf", 18);
    font("title",  "fonts/FiraCode-Regular.ttf", 32);
    font("big",    "fonts/FiraCode-Regular.ttf", 56);
}

// ===========================================================================
// level
// ===========================================================================
void Game::loadLevel() {
    // platformer.png is a 9x5 grid of 64px cells, which is exactly the world
    // tile size, so terrain now draws 1:1 instead of being upscaled 2x from
    // the old 32px grass_tiles.png (spriteCols 8).  Matching cell size is
    // also what lets the generator's tile ids index the atlas directly.
    g_map = new Map("terrain", 64);     // source tiles 64px, world tiles 64px
    // Distant layer: pushed toward the palette's deep blue (#0c2e44) so the
    // haze reads as distance instead of as a second, brighter copy of the
    // foreground.  This tint never applied before — TileComponent used to
    // gate it on `parallax < 0.3f` while this layer's factor is exactly 0.3f.
    g_map->LoadLayer(assetPath("tileset/BackgroundLayer.map"),
                     Config::MAP_WIDTH_TILES, Config::MAP_HEIGHT_TILES,
                     9, 0.30f, 96, 132, 176);
    g_map->LoadLayer(assetPath("tileset/ForegroundLayer.map"),
                     Config::MAP_WIDTH_TILES, Config::MAP_HEIGHT_TILES,
                     9, 1.0f);
    // Decor is props sitting ON the surface (rocks, crystals, lamps).  It was
    // loaded at 0.55f parallax, which made every prop drift sideways relative
    // to the ground it stands on as the camera moved — rocks visibly sliding
    // along the terrain.  Props share the foreground's depth, so they now
    // share its parallax; they get a slight cool tint to sit back a touch.
    g_map->LoadLayer(assetPath("tileset/DecorLayer.map"),
                     Config::MAP_WIDTH_TILES, Config::MAP_HEIGHT_TILES,
                     9, 1.0f, 224, 236, 246);
    g_map->LoadColliders(assetPath("tileset/Colliders.txt"));
    g_map->LoadCoins(assetPath("tileset/CoinLayer.map"),
                     Config::MAP_WIDTH_TILES, Config::MAP_HEIGHT_TILES);

    // platforms
    for (int i = 0; i < Level::N_PLATFORMS; i++) {
        const auto& p = Level::PLATFORMS[i];
        spawnPlatform(p.col, p.row, p.w, p.h, p.moving, p.vertical,
                      p.range, p.speed);
    }
    // hazards
    for (int i = 0; i < Level::N_HAZARDS; i++) {
        const auto& h = Level::HAZARDS[i];
        spawnHazard(h.kind, h.col, h.row, h.w, h.h);
    }
    // facilities
    for (int i = 0; i < Level::N_FACILITIES; i++) {
        const auto& f = Level::FACILITIES[i];
        spawnFacility(f.kind, f.col, f.row);
    }
    // powerups
    for (int i = 0; i < Level::N_POWERUPS; i++) {
        const auto& p = Level::POWERUPS[i];
        spawnPowerup(p.kind, p.col, p.row);
    }
    // enemies
    for (int i = 0; i < Level::N_ENEMIES; i++) {
        const auto& e = Level::ENEMIES[i];
        spawnEnemy(e.kind, e.col, e.row);
    }

    totalCoins = (int)G(groupCoins).size();
    gameScore = 0;
    coinsCollected = 0;
    lives = Config::MAX_LIVES;
    keys = 0;
    doorUnlocked = false;
    reachedCheckpoint = false;
    checkpointIndex = -1;
    state = State::Menu;
}

void Game::spawnPlayer() {
    auto& p = manager.addEntity("player");
    p.addComponent<TransformComponent>(Config::PLAYER_X, Config::PLAYER_Y,
                                       Config::PLAYER_W, Config::PLAYER_H, 1.f);
    p.addComponent<RigidBodyComponent>();
    auto& sp = p.addComponent<SpriteComponent>(
        Sprites::PLAYER.tex, Sprites::PLAYER.fw, Sprites::PLAYER.fh,
        Sprites::PLAYER.scale, true);
    Sprites::apply(sp, Sprites::PLAYER);
    // Row contents measured off the PNG, row by row.  The sheet is a 7x8 grid
    // of 50x37 cells (350x296, no remainder) and the frames per row are
    // 3, 6, 5, 6, 6, 4, 3, 7.
    //
    // The rows are NOT in the order you would guess.  Verifying each row by
    // its opaque-pixel bounding box shows the airborne row is 5, not 2:
    // row 5's frames have their feet at y=35, 35, 33, 27 — they lift off the
    // ground as the pose rises — while every cell of row 2 stays planted at
    // y=35 and two of them carry a huge sweeping arc.  Row 2 is a sword
    // swing.  The old mapping had "jump"/"apex"/"fall"/"leap" pointing at
    // row 2, which is exactly why a sword animation played while jumping.
    //
    //   row 0 (3)  idle, breathing
    //   row 1 (6)  run cycle
    //   row 2 (5)  POWER SWING  - windup, huge overhead arc, follow-through
    //   row 3 (6)  SLASH        - horizontal sweep, blade line across the body
    //   row 4 (6)  THRUST       - crouch and lunge, blade extended forward
    //   row 5 (4)  JUMP         - crouch, launch, rise, apex (feet leave floor)
    //   row 6 (3)  cast
    //   row 7 (7)  die
    sp.addAnim("idle",   Animation(0, 3, 170));
    sp.addAnim("run",    Animation(1, 6,  85));
    // Airborne: row 5.  Frames 0-1 are the crouch and take-off, 2-3 the rise
    // and the apex.  There is no dedicated "fall" row on this sheet, so the
    // descent reuses the apex frame (2) and the rise frame (3) — the legs are
    // trailing in both, which reads correctly on the way down.
    sp.addAnim("jump",   Animation(5, 2,  90));        // cols 0-1: crouch, launch
    sp.addAnim("rise",   Animation(5, 2,  80, 2));     // cols 2-3: lift, apex
    sp.addAnim("apex",   Animation(5, 1, 100, 3));     // col 3: top of the arc
    sp.addAnim("fall",   Animation(5, 2, 110, 2));     // cols 2-3: descent
    sp.addAnim("leap",   Animation(5, 1, 100, 1));     // dash pose: stretched
    // The three distinct sword attacks the sheet provides.
    sp.addAnim("attackPower", Animation(2, 5, 62));    // row 2, the big arc
    sp.addAnim("attackSlash", Animation(3, 6, 52));    // row 3, horizontal
    sp.addAnim("attackThrust",Animation(4, 6, 48));    // row 4, lunge
    sp.addAnim("cast",   Animation(6, 3,  90));
    sp.addAnim("die",    Animation(7, 7, 110));
    p.addComponent<ColliderComponent>("player",
        Config::PLAYER_W, Config::PLAYER_H);
    p.addComponent<HealthComponent>(Config::MAX_HP);
    p.addGroup(groupPlayers);
    syncCollider(p);
}

void Game::spawnEnemy(const std::string& kind, int col, int row) {
    // Art size, collider size, anchor and texture all come from one table
    // (SpriteSpec.h).  Nothing here guesses a dimension, which is what keeps
    // every enemy in the same visual scale as the hero.
    const Sprites::Spec* spec = nullptr;
    EnemyAIComponent::Kind k  = EnemyAIComponent::Kind::Skeleton;
    float hp = 1.f;

    if      (kind == "skeleton") { spec = &Sprites::SKELETON; k = EnemyAIComponent::Kind::Skeleton; hp = Config::ENEMY_HP_SKELETON; }
    else if (kind == "slime")    { spec = &Sprites::SLIME;    k = EnemyAIComponent::Kind::Slime;    hp = Config::ENEMY_HP_SLIME; }
    else if (kind == "bat")      { spec = &Sprites::BAT;      k = EnemyAIComponent::Kind::Bat;      hp = Config::ENEMY_HP_BAT; }
    else if (kind == "mage")     { spec = &Sprites::MAGE;     k = EnemyAIComponent::Kind::Mage;     hp = Config::ENEMY_HP_MAGE; }
    else if (kind == "archer")   { spec = &Sprites::ARCHER;   k = EnemyAIComponent::Kind::Archer;   hp = Config::ENEMY_HP_ARCHER; }
    else if (kind == "bomber")   { spec = &Sprites::BOMBER;   k = EnemyAIComponent::Kind::Bomber;   hp = Config::ENEMY_HP_BOMBER; }
    else if (kind == "boss")     { spec = &Sprites::BOSS;     k = EnemyAIComponent::Kind::Boss;     hp = Config::BOSS_HP; }
    else return;

    const int w = spec->cw, h = spec->ch;
    // The generator emits the row an entity OCCUPIES, so its feet sit exactly
    // on the bottom edge of that row - no sinking, no hovering.
    float px = col * Config::TILE_SIZE + (Config::TILE_SIZE - w) / 2.f;
    float py = (row + 1) * Config::TILE_SIZE - h;

    auto& e = manager.addEntity(kind);
    e.addComponent<TransformComponent>(px, py, w, h, 1.f);
    if (k != EnemyAIComponent::Kind::Bat)
        e.addComponent<RigidBodyComponent>();

    auto& sp = e.addComponent<SpriteComponent>(
        spec->tex, spec->fw, spec->fh, spec->scale, true);
    Sprites::apply(sp, *spec);
    switch (k) {
        // Every enemy animation below carries secondary motion (see
        // Animation::motion).  These sheets are tiny -- the bat has two
        // cells and the boss has one -- and stepping between two pictures at
        // 100ms per cell is a flicker, not a flap.  A pixel or two of bob and
        // a few degrees of lean, driven off the same clock as the frame
        // advance, reads as continuous movement without new art.
        case EnemyAIComponent::Kind::Skeleton:
            sp.addAnim("idle",   Animation(0, 4, 110).motion(1.0f, 1.2f, 1.f));
            sp.addAnim("run",    Animation(1, 6, 100).motion(1.8f, 2.6f));
            sp.addAnim("attack", Animation(2, 6, 80).motion(0.8f, 3.4f, 1.f));
            sp.addAnim("hurt",   Animation(3, 4, 120).motion(0.6f, 4.0f, 2.f));
            sp.addAnim("die",    Animation(4, 4, 200).motion(0.f, 0.f));
            sp.Play("idle");
            break;
        case EnemyAIComponent::Kind::Slime:
            // slime.png is a single row of 4 - every state reuses row 0 at a
            // different tempo, because row 1/2 simply do not exist.
            // The slime is a blob, so it gets the biggest amplitude: it
            // should visibly squall as it hops.
            sp.addAnim("idle", Animation(0, 4, 120).motion(1.2f, 0.f, 2.f, .35f));
            sp.addAnim("jump", Animation(0, 4, 55).motion(2.6f, 0.f, 2.f, .40f));
            sp.addAnim("die",  Animation(0, 2, 150).motion(0.f, 0.f));
            sp.Play("idle");
            break;
        case EnemyAIComponent::Kind::Bat:
            // enemy_bat.png is one row of 2 (the two wing positions).  Two
            // cells is the worst case for steppiness, so this one leans on
            // the cross-fade hardest: 45% of each step is a blend into the
            // other wing position, plus a strong vertical bob.
            sp.addAnim("fly", Animation(0, 2, 100).motion(3.0f, 7.0f, 1.f, .45f));
            sp.addAnim("die", Animation(0, 2, 150).motion(0.f, 0.f));
            sp.Play("fly");
            break;
        case EnemyAIComponent::Kind::Mage:
            // mage.png is one row of 4 - the cast is a fast flicker on row 0
            sp.addAnim("idle",   Animation(0, 4, 130).motion(1.4f, 1.0f, 1.f));
            sp.addAnim("attack", Animation(0, 4, 55).motion(0.5f, 2.0f, 2.f));
            sp.addAnim("die",    Animation(0, 2, 150).motion(0.f, 0.f));
            sp.Play("idle");
            break;
        case EnemyAIComponent::Kind::Archer:
            sp.addAnim("idle",  Animation(0, 4, 140).motion(0.9f, 1.0f, 1.f));
            sp.addAnim("shoot", Animation(1, 4, 90).motion(0.4f, 1.6f, 1.f));
            sp.addAnim("die",   Animation(2, 4, 150).motion(0.f, 0.f));
            sp.Play("idle");
            break;
        case EnemyAIComponent::Kind::Bomber:
            sp.addAnim("walk", Animation(0, 4, 110).motion(1.6f, 3.0f, 2.f));
            sp.addAnim("fuse", Animation(1, 4, 70).motion(0.7f, 5.0f, 2.f));
            sp.addAnim("die",  Animation(2, 4, 150).motion(0.f, 0.f));
            sp.Play("walk");
            break;
        case EnemyAIComponent::Kind::Boss:
            // villain_boss.png is a single 150x150 cell.  With no frames to
            // advance, ALL of its motion has to come from the secondary
            // channel: a slow 1.5px breath and a 2-degree sway, plus the
            // existing squash/stretch and hit flash.  `frames = 1` means the
            // loop position is just the phase, so cycles=1 gives one breath
            // per `speed` ms.
            sp.addAnim("idle",   Animation(0, 1, 900).motion(1.5f, 2.0f, 1.f));
            sp.addAnim("run",    Animation(0, 1, 260).motion(3.0f, 4.0f, 1.f));
            sp.addAnim("attack", Animation(0, 1, 160).motion(1.0f, 6.0f, 1.f));
            sp.addAnim("die",    Animation(0, 1, 150).motion(0.f, 0.f));
            sp.Play("idle");
            break;
    }
    e.addComponent<ColliderComponent>(kind, w, h);
    e.addComponent<HealthComponent>(hp, kind == "boss");
    auto& ai = e.addComponent<EnemyAIComponent>();
    ai.kind = k;
    ai.target = nullptr;
    e.addGroup(groupEnemies);
    syncCollider(e);
}

void Game::spawnPowerup(const std::string& kind, int col, int row) {
    const Sprites::Spec* spec = nullptr;
    PowerUpType t = PowerUpType::Health;
    if      (kind == "Health")     { spec = &Sprites::POWER_HEALTH;     t = PowerUpType::Health; }
    else if (kind == "Speed")      { spec = &Sprites::POWER_SPEED;      t = PowerUpType::Speed; }
    else if (kind == "Invincible") { spec = &Sprites::POWER_INVINCIBLE; t = PowerUpType::Invincible; }
    else if (kind == "DoubleJump") { spec = &Sprites::POWER_JUMP;       t = PowerUpType::DoubleJump; }
    else if (kind == "Damage")     { spec = &Sprites::POWER_DAMAGE;     t = PowerUpType::Damage; }
    else if (kind == "Magnet")     { spec = &Sprites::POWER_MAGNET;     t = PowerUpType::Magnet; }
    else if (kind == "Shield")     { spec = &Sprites::POWER_SHIELD;     t = PowerUpType::Shield; }
    else if (kind == "CoinX2")     { spec = &Sprites::POWER_COINS;      t = PowerUpType::CoinX2; }
    else return;

    const int w = spec->cw, h = spec->ch;
    const float T = (float)Config::TILE_SIZE;
    // centred in its tile, so a pickup reads the same wherever it lands
    float px = col * T + (T - w) / 2.f;
    float py = (row + 1) * T - h;

    auto& p = manager.addEntity("powerup");
    p.addComponent<TransformComponent>(px, py, w, h, 1.f);
    auto& sp = p.addComponent<SpriteComponent>(
        spec->tex, spec->fw, spec->fh, spec->scale, false);
    Sprites::apply(sp, *spec);
    // gentle bob so pickups catch the eye without any extra art
    auto& bob = p.addComponent<BobbingComponent>();
    bob.amplitude = 5.f;
    bob.speed     = 2.4f;
    p.addComponent<ColliderComponent>("powerup", w, h);
    p.addComponent<PowerUpComponent>(t);
    p.addGroup(groupPowerups);
    syncCollider(p);
}

void Game::spawnFacility(const std::string& kind, int col, int row) {
    const Sprites::Spec* spec = nullptr;
    if      (kind == "checkpoint") spec = &Sprites::CHECKPOINT;
    else if (kind == "shrine")     spec = &Sprites::SHRINE;
    else if (kind == "key")        spec = &Sprites::KEY;
    else if (kind == "door")       spec = &Sprites::DOOR;
    else return;

    const int w = spec->cw, h = spec->ch;
    const float T = (float)Config::TILE_SIZE;
    float px = col * T + (T - w) / 2.f;
    float py = (row + 1) * T - h;

    auto& f = manager.addEntity(kind);
    f.addComponent<TransformComponent>(px, py, w, h, 1.f);
    auto& sp = f.addComponent<SpriteComponent>(
        spec->tex, spec->fw, spec->fh, spec->scale, false);
    Sprites::apply(sp, *spec);
    f.addComponent<ColliderComponent>(kind, w, h);
    f.getComponent<ColliderComponent>().solid = false;
    if (kind == "key") {   // keys float and spin gently on the spot
        auto& bob = f.addComponent<BobbingComponent>();
        bob.amplitude = 4.f;
        bob.speed     = 2.0f;
    }
    f.addGroup(groupFacilities);
    syncCollider(f);
}

// One entity per tile, so a wide strip tiles its art instead of stretching it.
void Game::spawnHazard(const std::string& kind, int col, int row, int w, int h) {
    const Sprites::Spec* spec = (kind == "spikes") ? &Sprites::SPIKES : nullptr;
    if (!spec) return;

    const float T = (float)Config::TILE_SIZE;
    for (int ty = 0; ty < h; ty++) {
        for (int tx = 0; tx < w; tx++) {
            // the collider is only the lower band of the tile, so you are
            // hurt by touching the spikes, not by standing near them
            const int hw = spec->cw;
            const int hh = spec->ch;
            float px = (col + tx) * T + (T - hw) / 2.f;
            float py = (row + ty + 1) * T - hh;

            auto& z = manager.addEntity("hazard");
            z.addComponent<TransformComponent>(px, py, hw, hh, 1.f);
            auto& sp = z.addComponent<SpriteComponent>(
                spec->tex, spec->fw, spec->fh, spec->scale, false);
            Sprites::apply(sp, *spec);
            z.addComponent<ColliderComponent>("hazard", hw, hh);
            z.getComponent<ColliderComponent>().solid  = false;
            z.getComponent<ColliderComponent>().sensor = true;
            z.addGroup(groupHazards);
            syncCollider(z);
        }
    }
}

void Game::spawnPlatform(int col, int row, int w, int h,
                         bool moving, bool vertical, float range, float speed) {
    const float T = (float)Config::TILE_SIZE;
    int pw = w * Config::TILE_SIZE;
    int ph = h * Config::TILE_SIZE;
    float px = col * T;
    float py = row * T;

    auto& p = manager.addEntity("platform");
    p.addComponent<TransformComponent>(px, py, pw, ph, 1.f);
    auto& sp = p.addComponent<SpriteComponent>(
        Sprites::PLATFORM.tex, Sprites::PLATFORM.fw, Sprites::PLATFORM.fh,
        Sprites::PLATFORM.scale, false);
    Sprites::apply(sp, Sprites::PLATFORM);
    p.addComponent<ColliderComponent>("platform", pw, ph);
    if (moving) {
        Vector2D axis = vertical ? Vector2D(0, 1) : Vector2D(1, 0);
        p.addComponent<MovingPlatformComponent>(
            Vector2D(px, py), axis, range, speed);
    }
    p.addGroup(groupPlatforms);
    p.addGroup(groupColliders);
    syncCollider(p);
}

// ===========================================================================
// input
// ===========================================================================
void Game::handlePlayerInput() {
    if (state != State::Playing) return;
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    bool left  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    bool right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];

    auto& pTr = manager.getGroup(groupPlayers)[0]->getComponent<TransformComponent>();
    float target = 0;
    if (left)  target = -Config::PLAYER_SPEED * (speedBoostTimer > 0 ? Config::POWERUP_SPEED_MULT : 1.f);
    if (right) target =  Config::PLAYER_SPEED * (speedBoostTimer > 0 ? Config::POWERUP_SPEED_MULT : 1.f);

    if (dashActive) {
        // velocity already set by startDash
    } else if (target != 0) {
        float accel = Config::PLAYER_ACCEL * lastFrameTime;
        if (pTr.velocity.x < target) pTr.velocity.x = std::min(pTr.velocity.x + accel, target);
        else if (pTr.velocity.x > target) pTr.velocity.x = std::max(pTr.velocity.x - accel, target);
        facingRight = target > 0;
        pTr.facingRight = facingRight;
    } else {
        float fric = Config::PLAYER_FRICTION * lastFrameTime;
        if (std::abs(pTr.velocity.x) < fric) pTr.velocity.x = 0;
        else pTr.velocity.x -= (pTr.velocity.x > 0 ? fric : -fric);
    }

    if (jumpBufferTimer > 0) jumpBufferTimer -= lastFrameTime;
    if (coyoteTimer > 0)     coyoteTimer     -= lastFrameTime;
    if (dmgBoostTimer > 0)   dmgBoostTimer   -= lastFrameTime;
    if (speedBoostTimer > 0) speedBoostTimer -= lastFrameTime;
    if (invincibleTimer > 0) invincibleTimer -= lastFrameTime;
    if (magnetTimer > 0)     magnetTimer     -= lastFrameTime;
    if (coinMultTimer > 0)   coinMultTimer   -= lastFrameTime;
    if (iframeTimer > 0)     iframeTimer     -= lastFrameTime;

    // ---- variable jump cut ------------------------------------------------
    // Release the jump key early and the rise is clipped, so a tap gives a
    // short hop and a hold gives the full leap.
    //
    // BUG: this used to test only W and SPACE.  handleEvents() accepts
    // W, SPACE *and* UP as jump keys, so jumping with the arrow key was
    // never cut — the hero jumped ~45% higher on arrows than on WASD.  All
    // three keys are checked now, so both layouts jump identically.
    const bool jumpHeld = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_SPACE]
                       || keys[SDL_SCANCODE_UP];
    if (!jumpHeld && pTr.velocity.y < Config::PLAYER_JUMP_VEL * 0.55f) {
        pTr.velocity.y = Config::PLAYER_JUMP_VEL * 0.55f;
    }

    handleAttacks();
    stepMana();
}

// ===========================================================================
// attacks
// ===========================================================================
// Six distinct moves instead of one overloaded key:
//
//   J / Z                 melee — 3-hit combo (Slash1 -> Slash2 -> Slash3)
//   J while holding UP    Uppercut   — anti-air, launches the victim
//   J while airborne + DOWN  DiveSlash — stomp; bounces the hero back up
//   J during a dash       DashSlash  — long lunge, big knockback
//   K / X  (tap)          Bolt       — costs mana
//   K / X  (hold 0.40s)   HeavyBolt  — piercing, costs more mana
//
// Every move is a row in Config::ATTACKS, so this function only decides WHICH
// move to run — the numbers live in the table.
void Game::handleAttacks() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool meleeDown  = keys[SDL_SCANCODE_J] || keys[SDL_SCANCODE_Z];
    const bool rangedDown = keys[SDL_SCANCODE_K] || keys[SDL_SCANCODE_X];

    if (attackTimer    > 0) attackTimer    -= lastFrameTime;
    if (attackCooldown > 0) attackCooldown -= lastFrameTime;
    if (comboTimer     > 0) comboTimer     -= lastFrameTime;
    else                    comboStep = -1;

    const bool ready = (attackTimer <= 0 && attackCooldown <= 0);

    // Config::MAX_PROJECTILES was declared but never enforced, so a player
    // hugging K could fill the screen with bolts and stall the frame.  Count
    // only the bolts this hero owns — enemy projectiles must not eat the cap.
    int myBolts = 0;
    for (auto* pr : manager.getGroup(groupProjectiles)) {
        if (!pr->isActive() || !pr->hasComponent<ProjectileComponent>()) continue;
        if (pr->getComponent<ProjectileComponent>().ownerTag == "player") ++myBolts;
    }
    const bool boltRoom = myBolts < Config::MAX_PROJECTILES;

    // ---- ranged: press fires a bolt, holding upgrades to a heavy one ----
    if (rangedDown) {
        if (ready && boltRoom && !prevRangedDown && mana >= Config::ATTACKS[
                (int)Config::AttackKind::Bolt].mana) {
            performAttack(Config::AttackKind::Bolt);
            chargeT = 0.f;
            chargeFired = false;
        }
        if (!prevRangedDown) { chargeT = 0.f; chargeFired = false; }
        chargeT += lastFrameTime;
        if (!chargeFired && chargeT >= Config::CHARGE_TIME && ready && boltRoom
            && mana >= Config::ATTACKS[(int)Config::AttackKind::HeavyBolt].mana) {
            performAttack(Config::AttackKind::HeavyBolt);
            chargeFired = true;
        }
    } else {
        chargeT = 0.f;
        chargeFired = false;
    }

    // ---- melee: directional variants win over the plain combo ----
    if (meleeDown && !prevMeleeDown && ready) {
        if (dashActive) {
            performAttack(Config::AttackKind::DashSlash);
        } else if (!isGrounded && (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])) {
            performAttack(Config::AttackKind::DiveSlash);
        } else if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            // On the ground this is a rising uppercut; in the air with UP held
            // it is still the anti-air — the combo is the default, not this.
            performAttack(Config::AttackKind::Uppercut);
        } else {
            comboStep = (comboStep < 0 || comboTimer <= 0)
                      ? 0 : (comboStep + 1) % Config::MAX_COMBO;
            performAttack((Config::AttackKind)comboStep);
        }
        comboTimer = Config::COMBO_WINDOW;
    }

    prevMeleeDown  = meleeDown;
    prevRangedDown = rangedDown;
}

void Game::performAttack(Config::AttackKind kind) {
    const int idx = (int)kind;
    if (idx < 0 || idx >= Config::ATTACK_COUNT) return;
    const Config::AttackDef& d = Config::ATTACKS[idx];

    auto& p  = *manager.getGroup(groupPlayers)[0];
    auto& tr = p.getComponent<TransformComponent>();
    if (mana < d.mana) return;                        // not enough — fizzle

    const int   dir = facingRight ? 1 : -1;
    const float mul = (dmgBoostTimer > 0) ? Config::POWERUP_DMG_MULT : 1.f;

    attackTimer    = d.duration;
    attackCooldown = d.duration + d.cooldown;
    if (d.mana > 0.f) { mana -= d.mana; manaDelay = Config::MANA_REGEN_DELAY; }

    if (p.hasComponent<SpriteComponent>())
        p.getComponent<SpriteComponent>().PlayOver(d.anim);

    m_shakeMag += d.shake;

    if (d.boxW > 0) {
        Combat::spawnMeleeHitbox(p, d, dir, d.dmg * mul, idx);
        Vfx::slashArc(p, dir, idx);
        // the swing carries the hero forward a little — attacks should feel
        // like they have weight, not like pressing a button
        if (d.lunge != 0.f) {
            tr.velocity.x += dir * d.lunge;
            if (std::abs(tr.velocity.x) > Config::PLAYER_SPEED * 2.4f)
                tr.velocity.x = dir * Config::PLAYER_SPEED * 2.4f;
        }
        if (kind == Config::AttackKind::DiveSlash) {
            tr.velocity.y = 780.f;
            tr.velocity.x *= 0.4f;
            diving = true;
            Particles::emitDust(tr.position.x + tr.width / 2.f,
                                tr.position.y + tr.height, 200, 230, 255);
        } else if (kind == Config::AttackKind::Uppercut && !isGrounded) {
            tr.velocity.y = std::min(tr.velocity.y, -300.f);
        } else if (kind == Config::AttackKind::DashSlash) {
            dashActive = false;
            Particles::emitDust(tr.position.x + tr.width / 2.f,
                                tr.position.y + tr.height, 255, 220, 160);
        }
        if (auto* s = assets->GetSound("swing")) Mix_PlayChannel(-1, s, 0);
    } else {
        const bool heavy = (kind == Config::AttackKind::HeavyBolt);
        Combat::spawnPlayerBolt(p, dir, heavy);
        // a little recoil — the bolt pushes off the caster
        tr.velocity.x -= dir * (heavy ? 90.f : 35.f);
        if (auto* s = assets->GetSound("shoot")) Mix_PlayChannel(-1, s, 0);
    }
}

void Game::stepMana() {
    if (manaDelay > 0) { manaDelay -= lastFrameTime; return; }
    if (mana < Config::MANA_MAX) {
        mana = std::min(Config::MANA_MAX,
                        mana + Config::MANA_REGEN * lastFrameTime);
    }
}

// ---------------------------------------------------------------------------
// Hit feedback: combo counter, score multiplier, freeze frames, popup numbers.
// ---------------------------------------------------------------------------
float Game::comboMultiplier() const {
    const int steps = std::min(hitCombo, Config::HITCOMBO_MAX_MULT);
    return 1.f + steps * Config::HITCOMBO_STEP;      // 1.0x .. 3.0x
}

void Game::registerHit(bool killed) {
    hitCombo++;
    hitComboT = Config::HITCOMBO_TIMEOUT;
    if (hitCombo > bestCombo) bestCombo = hitCombo;
    hitStop = std::max(hitStop,
        killed ? Config::HITSTOP_KILL
               : (hitCombo % Config::MAX_COMBO == 0 ? Config::HITSTOP_HEAVY
                                                    : Config::HITSTOP_LIGHT));
}

void Game::popNumber(float x, float y, int amount, bool crit) {
    if (m_numbers.size() > 40) return;               // don't spam
    m_numbers.push_back({ x, y, crit ? 0.9f : 0.7f, crit ? -46.f : -34.f,
                          amount, crit });
}

void Game::stepNumbers() {
    for (auto it = m_numbers.begin(); it != m_numbers.end(); ) {
        it->life -= lastFrameTime;
        it->y    += it->vy * lastFrameTime;
        it->vy   += 90.f * lastFrameTime;            // settle, then fall
        if (it->life <= 0) it = m_numbers.erase(it);
        else ++it;
    }
    if (hitComboT > 0) { hitComboT -= lastFrameTime; if (hitComboT <= 0) hitCombo = 0; }
}

// ===========================================================================
// player animation
// ===========================================================================
// The sheet has eight rows (idle / run / jump / atk1 / atk2 / atk3 / cast /
// die) and previously only ever played the first three.  This picks the row
// that matches what the hero is actually doing, so the sprite finally reads
// as running, jumping, swinging and casting.
void Game::updatePlayerAnimation() {
    if (manager.getGroup(groupPlayers).empty()) return;
    auto& p = *manager.getGroup(groupPlayers)[0];
    if (!p.hasComponent<SpriteComponent>()) return;
    auto& sp = p.getComponent<SpriteComponent>();
    auto& tr = p.getComponent<TransformComponent>();

    // An attack owns the sprite for its duration — nothing may interrupt it.
    if (attackTimer > 0) return;

    const auto& hp = p.getComponent<HealthComponent>();
    if (hp.dead)            { sp.Play("die");  return; }
    if (dashActive)         { sp.Play("leap"); return; }
    if (diving)             { sp.Play("attackThrust"); return; }

    if (!isGrounded) {
        // Hysteresis: between -40 and +80 px/s the hero is at the apex and
        // should keep whichever pose it had, instead of flickering.
        //
        // Row 5 is the airborne row (see spawnPlayer), so no sword frame can
        // appear up here.  Within that row only cols 2-3 are actually off the
        // ground — cols 0-1 are the grounded crouch and take-off — so every
        // airborne state is framed from col 2 onward.
        if      (tr.velocity.y < -40.f) sp.Play("rise");
        else if (tr.velocity.y >  80.f) sp.Play("fall");
        else                            sp.Play("apex");
        return;
    }

    if (std::abs(tr.velocity.x) > 14.f) sp.Play("run");
    else                                sp.Play("idle");
}

void Game::startJump() {
    if (state != State::Playing) return;
    auto& p = *manager.getGroup(groupPlayers)[0];
    auto& tr = p.getComponent<TransformComponent>();
    auto& col = p.getComponent<ColliderComponent>();
    // wall jump check
    bool touchingWall = false;
    int  wallDir = 0;
    for (auto* c : manager.getGroup(groupColliders)) {
        if (c == &p) continue;
        if (!c->hasComponent<ColliderComponent>()) continue;
        auto& b = c->getComponent<ColliderComponent>().collider;
        bool vo = col.collider.y + col.collider.h > b.y && col.collider.y < b.y + b.h;
        if (!vo) continue;
        if (std::abs((col.collider.x + col.collider.w) - b.x) < 4) { touchingWall = true; wallDir =  1; break; }
        if (std::abs((b.x + b.w) - col.collider.x) < 4)             { touchingWall = true; wallDir = -1; break; }
    }
    if (touchingWall && !isGrounded) {
        tr.velocity.y = Config::WALL_JUMP_VY;
        tr.velocity.x = -wallDir * Config::WALL_JUMP_VX;
        jumpsRemaining = Config::MAX_JUMPS - 1;
        isGrounded = false;
        if (auto* s = assets->GetSound("jump")) Mix_PlayChannel(-1, s, 0);
        return;
    }
    if (isGrounded || coyoteTimer > 0) {
        tr.velocity.y = Config::PLAYER_JUMP_VEL;
        isGrounded = false; coyoteTimer = 0;
        jumpsRemaining = Config::MAX_JUMPS - 1;
        if (auto* s = assets->GetSound("jump")) Mix_PlayChannel(-1, s, 0);
    } else if (jumpsRemaining > 0) {
        tr.velocity.y = Config::PLAYER_JUMP_VEL * 0.9f;
        jumpsRemaining--;
        if (auto* s = assets->GetSound("jump")) Mix_PlayChannel(-1, s, 0);
    }
}

void Game::startDash() {
    if (state != State::Playing || dashActive || dashCooldown > 0) return;
    // One dash per jump: on the ground it is always available (subject to the
    // cooldown), in the air you get exactly one and must land to refresh it.
    if (!isGrounded) {
        if (!Config::AIR_DASH_ENABLED || airDashUsed) return;
        airDashUsed = true;
    }
    auto& tr = manager.getGroup(groupPlayers)[0]->getComponent<TransformComponent>();
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    int dir = facingRight ? 1 : -1;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  dir = -1;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dir =  1;
    dashActive = true; dashTimer = Config::DASH_DURATION;
    const bool air = !isGrounded;
    tr.velocity.x = dir * (air ? Config::AIR_DASH_SPEED : Config::DASH_SPEED);
    if (air) tr.velocity.y = 0;              // air dash is a flat line
    facingRight = dir > 0;
    tr.facingRight = facingRight;
    if (auto* s = assets->GetSound("dash")) Mix_PlayChannel(-1, s, 0);
    Particles::emitDust(tr.position.x + tr.width/2, tr.position.y + tr.height,
                        220, 220, 240);
    dashTrailTimer = 0.f;   // drop the first ghost immediately
}

// A dash that lasts 0.18s at 900px/s covers ~160px.  Without a trail the hero
// simply teleports across that gap, which reads as a glitch rather than speed.
void Game::emitDashTrail(float dt) {
    if (!dashActive) return;
    dashTrailTimer -= dt;
    if (dashTrailTimer > 0.f) return;
    dashTrailTimer = 0.035f;    // ~5 ghosts over the dash
    auto& p = *manager.getGroup(groupPlayers)[0];
    Vfx::afterimage(p, 0.22f, 130, 200, 255);
}

void Game::updateHeroState() {
    if (dashActive) {
        dashTimer -= lastFrameTime;
        if (dashTimer <= 0) { dashActive = false; dashCooldown = Config::DASH_COOLDOWN; }
    }
    if (dashCooldown > 0) dashCooldown -= lastFrameTime;
    if (state != State::Playing) return;
    auto& hp = manager.getGroup(groupPlayers)[0]->getComponent<HealthComponent>();
    if (hp.dead && m_deathTimer <= 0) m_deathTimer = 0.05f;
    if (m_deathTimer > 0) {
        m_deathTimer -= lastFrameTime;
        if (m_deathTimer <= 0) {
            lives--;
            deaths++;
            if (gameScore > bestScore) bestScore = gameScore;
            if (lives <= 0) { state = State::GameOver; saveSettings(); }
            else            { respawn(); }
        }
    }
    if (state == State::Victory && m_victoryTimer == 0) m_victoryTimer = 0.05f;
    if (m_victoryTimer > 0) {
        m_victoryTimer -= lastFrameTime;
        if (m_victoryTimer <= 0) {
            // Stay on Victory screen.  Press R or ESC to continue.
        }
    }
}

void Game::respawn() {
    auto& p = *manager.getGroup(groupPlayers)[0];
    auto& tr = p.getComponent<TransformComponent>();
    auto& hp = p.getComponent<HealthComponent>();
    tr.position = Vector2D(Config::PLAYER_X, Config::PLAYER_Y);
    tr.velocity.Zero();
    // INV-2: moveAndCollide reads the collider; keep it in sync with any
    // position write.
    p.getComponent<ColliderComponent>().update();
    hp.hp = hp.hpMax;
    hp.dead = false;
    hp.iframes = Config::RESPAWN_INVULN;
    dashActive = false;
    iframeTimer = Config::RESPAWN_INVULN;
    // A death should not leave the hero mid-swing or holding a stale combo.
    attackTimer = 0.f; attackCooldown = 0.f;
    comboStep = -1; comboTimer = 0.f;
    diving = false; airDashUsed = false;
    chargeT = 0.f; chargeFired = false;
    hitCombo = 0; hitComboT = 0.f; hitStop = 0.f;
    mana = Config::MANA_MAX; manaDelay = 0.f;
    m_numbers.clear();
}

void Game::restartLevel() {
    // clear all dynamic entities but keep the world tiles
    for (auto* e : manager.getGroup(groupEnemies))   e->destroy();
    for (auto* c : manager.getGroup(groupCoins))     c->destroy();
    for (auto* p : manager.getGroup(groupPowerups))  p->destroy();
    for (auto* p : manager.getGroup(groupPlatforms)) p->destroy();
    for (auto* h : manager.getGroup(groupHazards))   h->destroy();
    for (auto* f : manager.getGroup(groupFacilities))f->destroy();
    for (auto* p : manager.getGroup(groupProjectiles))p->destroy();
    for (auto* p : manager.getGroup(groupParticles)) p->destroy();
    for (auto* a : manager.getGroup(groupAfterimages)) a->destroy();
    for (auto* h : manager.getGroup(groupHitboxes))  h->destroy();
    manager.refresh();

    // reload coins + respawn everything
    g_map->LoadCoins(assetPath("tileset/CoinLayer.map"),
                     Config::MAP_WIDTH_TILES, Config::MAP_HEIGHT_TILES);
    for (int i = 0; i < Level::N_PLATFORMS; i++) {
        const auto& p = Level::PLATFORMS[i];
        spawnPlatform(p.col, p.row, p.w, p.h, p.moving, p.vertical, p.range, p.speed);
    }
    for (int i = 0; i < Level::N_HAZARDS; i++) {
        const auto& h = Level::HAZARDS[i];
        spawnHazard(h.kind, h.col, h.row, h.w, h.h);
    }
    for (int i = 0; i < Level::N_FACILITIES; i++) {
        const auto& f = Level::FACILITIES[i];
        spawnFacility(f.kind, f.col, f.row);
    }
    for (int i = 0; i < Level::N_POWERUPS; i++) {
        const auto& p = Level::POWERUPS[i];
        spawnPowerup(p.kind, p.col, p.row);
    }
    for (int i = 0; i < Level::N_ENEMIES; i++) {
        const auto& e = Level::ENEMIES[i];
        spawnEnemy(e.kind, e.col, e.row);
    }
    totalCoins = (int)G(groupCoins).size();
    gameScore = 0; coinsCollected = 0;
    lives = maxLives; keys = 0;
    doorUnlocked = false;
    reachedCheckpoint = false;
    runTime = 0.f; enemiesDefeated = 0; deaths = 0;
    bestCombo = 0; coinsBanked = 0;
    shieldCharges = 0; m_shakeMag = 0.f; m_shake = 0.f;
    speedBoostTimer = invincibleTimer = dmgBoostTimer = magnetTimer = coinMultTimer = 0.f;
    m_deathTimer = 0.f;
    checkpointIndex = -1;
    respawn();
    state = State::Playing;
    isPaused = false;
    pauseSelection = 0;
}

// ===========================================================================
// main update
// ===========================================================================
void Game::stepPlayerPhysics() {
    auto& player = *manager.getGroup(groupPlayers)[0];
    // collect solid rects once per frame
    std::vector<SDL_Rect> solids;
    solids.reserve(manager.getGroup(groupColliders).size());
    for (auto* c : manager.getGroup(groupColliders)) {
        if (c->hasComponent<ColliderComponent>())
            solids.push_back(c->getComponent<ColliderComponent>().collider);
    }
    auto r = Physics::moveAndCollide(player, solids);
    wasGrounded = isGrounded;
    isGrounded = r.grounded;
    if (isGrounded) {
        coyoteTimer = Config::COYOTE_TIME;
        jumpsRemaining = Config::MAX_JUMPS;
        // Touching down refreshes the air dash and ends a dive-slash — both
        // are "once per time in the air" resources.
        airDashUsed = false;
        diving      = false;
        // Landing feedback, scaled by how hard we came down.  Without this a
        // long fall ends in silence and feels like the game dropped a frame.
        if (!wasGrounded) {
            auto& ltr = player.getComponent<TransformComponent>();
            float impact = std::min(1.f, Config::PLAYER_TERMINAL > 0.f
                ? (m_fallSpeed / Config::PLAYER_TERMINAL) : 0.f);
            Vfx::landDust(ltr.position.x + ltr.width / 2.f,
                          ltr.position.y + ltr.height,
                          0.6f + impact * 1.4f);
            if (impact > 0.55f) m_shakeMag += impact * 4.f;
            if (player.hasComponent<SpriteComponent>())
                player.getComponent<SpriteComponent>().squash(1.25f, 0.72f);
        }
    }
    // sync facing for camera lookahead
    auto& tr = player.getComponent<TransformComponent>();
    tr.facingRight = facingRight;
    m_fallSpeed = tr.velocity.y;
    (void)tr;
}

void Game::stepEnemies() {
    auto& player = *manager.getGroup(groupPlayers)[0];
    // One sink carries the hero's defensive state down into the AI, so enemy
    // contact damage, blasts and shockwaves all honour shields and i-frames
    // through exactly the same funnel (Damage::applyToPlayer).
    EnemyAI::DamageSink sink{ shieldCharges, iframeTimer, invincibleTimer };
    EnemyAI::step(manager.getGroup(groupEnemies), player, sink, m_shakeMag);
}

void Game::stepEnemyPhysics() {
    std::vector<SDL_Rect> solids;
    for (auto* c : manager.getGroup(groupColliders)) {
        if (c->hasComponent<ColliderComponent>())
            solids.push_back(c->getComponent<ColliderComponent>().collider);
    }
    for (auto* e : manager.getGroup(groupEnemies)) {
        if (!e->isActive() || !e->hasComponent<ColliderComponent>()) continue;
        // skip bats
        if (!e->hasComponent<RigidBodyComponent>()) continue;
        Physics::moveAndCollide(*e, solids);
    }
}

void Game::checkLevelState() {
    if (state != State::Playing) return;
    auto& p = *manager.getGroup(groupPlayers)[0];
    auto& tr = p.getComponent<TransformComponent>();
    auto& hp = p.getComponent<HealthComponent>();
    // fall off map
    if (tr.position.y > Config::MAP_PIXEL_HEIGHT + 200) {
        hp.dead = true; hp.hp = 0;
        if (m_deathTimer <= 0) m_deathTimer = 0.05f;
    }
    // boss defeated
    if (doorUnlocked && reachedCheckpoint) {
        bool bossAlive = false;
        for (auto* e : manager.getGroup(groupEnemies)) {
            if (e->tag == "boss" && e->getComponent<HealthComponent>().hp > 0) {
                bossAlive = true; break;
            }
        }
        if (!bossAlive && state == State::Playing) {
            state = State::Victory;
            m_victoryTimer = 9999.f;
            gameScore += Config::BOSS_SCORE;
            if (auto* s = assets->GetSound("powerup")) Mix_PlayChannel(-1, s, 0);
            Particles::emitHeal(tr.position.x, tr.position.y);
        }
    }
    if (coinsCollected >= totalCoins && totalCoins > 0 && state == State::Playing) {
        // soft bonus, not game over
    }
}

void Game::update() {
    updateUIAnimations(lastFrameTime);
    if (state == State::Menu || state == State::Instructions) {
        handlePlayerInput();
        manager.refresh();
        // The menu draws no world, so simulating it there only lets gravity
        // drop the hero out of the level before the run has started.  How To
        // Play opened mid-run still draws the world behind the panel, so that
        // case keeps updating.
        if (state == State::Instructions && m_returnState != State::Menu)
            manager.update();
        return;
    }
    if (state == State::Paused || state == State::Settings ||
        state == State::Stats) {
        // Freeze the world behind the overlay, but keep the sprite clocks
        // ticking so the hero mid-swing / the water mid-ripple do not resume
        // with a visible jump when the player unpauses.
        manager.refresh();
        manager.update();
        return;
    }
    if (state == State::GameOver || state == State::Victory ||
        state == State::LevelComplete) {
        stepNumbers();
        manager.refresh();
        manager.update();
        return;
    }

    // ---- Playing ----
    runTime += lastFrameTime;

    // Wind keeps blowing while the world is frozen on a hit, otherwise a
    // combo locks the grass and the fog mid-gust and the weather visibly
    // stutters exactly when the player is looking at it.
    Atmosphere::update(lastFrameTime);

    // Freeze frames.  A landed hit stops the world for a few milliseconds —
    // the difference between a swing that connects and one that does not is
    // almost entirely this.  The hit-stop runs on real time, so it cannot
    // stall the game if a frame takes unusually long.
    if (hitStop > 0.f) {
        hitStop -= lastFrameTime;
        stepNumbers();
        manager.refresh();
        return;
    }

    handlePlayerInput();
    stepPlayerPhysics();
    Physics::stepPlatforms(manager.getGroup(groupPlatforms));
    Physics::applyPlatformCarry(*manager.getGroup(groupPlayers)[0],
                                manager.getGroup(groupPlatforms));
    stepEnemies();
    stepEnemyPhysics();
    emitDashTrail(lastFrameTime);
    Particles::step(manager.getGroup(groupParticles));
    Vfx::step(manager.getGroup(groupParticles));

    // pickups
    {
        Pickup::PlayerBag bag{
            /*hp*/        manager.getGroup(groupPlayers)[0]->getComponent<HealthComponent>().hp,
            /*maxHp*/     Config::MAX_HP,
            /*speedT*/    speedBoostTimer,
            /*invincT*/   invincibleTimer,
            /*dmgT*/      dmgBoostTimer,
            /*iframeT*/   iframeTimer,
            /*score*/     gameScore,
            /*lives*/     lives,
            /*keys*/      keys,
            /*jumpsLeft*/ jumpsRemaining,
            /*maxJumps*/  Config::MAX_JUMPS,
            /*reachedCp*/ reachedCheckpoint,
            /*doorUnl*/   doorUnlocked,
            /*shake*/     m_shakeMag,
            /*magnetT*/   magnetTimer,
            /*shield*/    shieldCharges,
            /*coinMultT*/ coinMultTimer,
            /*banked*/    coinsBanked
        };
        Pickup::step(*manager.getGroup(groupPlayers)[0],
                     manager.getGroup(groupCoins),
                     manager.getGroup(groupPowerups),
                     manager.getGroup(groupFacilities),
                     manager.getGroup(groupHazards),
                     bag);
        coinsCollected = totalCoins - (int)manager.getGroup(groupCoins).size();
    }

    // combat — player attacks hit enemies, enemy projectiles hit player,
    // player projectiles hit enemies.
    // resolveMelee reports every strike it lands, so the popup numbers show
    // the real damage roll rather than a guess.
    m_hitEvents.clear();
    Combat::resolveMelee(manager.getGroup(groupHitboxes),
                         manager.getGroup(groupEnemies),
                         m_shakeMag, &m_hitEvents);
    for (auto& e : m_hitEvents) {
        registerHit(e.killed);
        popNumber(e.x, e.y - 10.f, e.damage, e.killed || hitCombo >= 3);
    }
    // collision: player projectile vs enemies, enemy projectile vs player
    auto& player = *manager.getGroup(groupPlayers)[0];
    for (auto* pr : manager.getGroup(groupProjectiles)) {
        if (!pr->isActive() || !pr->hasComponent<ColliderComponent>()) continue;
        auto& pc = pr->getComponent<ProjectileComponent>();
        auto& col = pr->getComponent<ColliderComponent>().collider;
        if (pc.ownerTag == "player") {
            for (auto* en : manager.getGroup(groupEnemies)) {
                if (!en->isActive() || !en->hasComponent<ColliderComponent>()) continue;
                if (pc.dead.count(reinterpret_cast<std::uintptr_t>(en))) continue;
                if (Collision::AABB(col, en->getComponent<ColliderComponent>().collider)) {
                    auto& hp = en->getComponent<HealthComponent>();
                    float dmg = pc.damage * (dmgBoostTimer > 0 ? Config::POWERUP_DMG_MULT : 1.f);
                    // damage() means "the hit landed"; a kill needs hp.dead too.
                    const bool connected = hp.damage(dmg);
                    const bool died      = connected && hp.dead;
                    if (en->hasComponent<EnemyAIComponent>()) {
                        auto& ai = en->getComponent<EnemyAIComponent>();
                        ai.hurtT = std::max(ai.hurtT,
                            (ai.kind == EnemyAIComponent::Kind::Boss) ? pc.stunT * 0.25f
                                                                     : pc.stunT);
                    }
                    if (en->hasComponent<TransformComponent>()) {
                        auto& et = en->getComponent<TransformComponent>();
                        et.velocity.x += (pc.velocity.x > 0 ? 1.f : -1.f) * 90.f;
                        Vfx::impactRing(et.position.x + et.width / 2.f,
                                        et.position.y + et.height / 2.f,
                                        255, 236, 190, died ? 58.f : 36.f, 0.22f);
                    }
                    if (auto* s = assets->GetSound(died ? "enemyDie" : "hit"))
                        Mix_PlayChannel(-1, s, 0);
                    m_shakeMag += died ? 6.f : 2.f;
                    if (en->hasComponent<SpriteComponent>())
                        en->getComponent<SpriteComponent>().flash(0.12f);
                    registerHit(died);
                    popNumber(pr->getComponent<TransformComponent>().position.x,
                              pr->getComponent<TransformComponent>().position.y,
                              (int)dmg, died || hitCombo >= 3);
                    if (!pc.pierce) { pr->destroy(); }
                    else { pc.dead.insert(reinterpret_cast<std::uintptr_t>(en)); }
                    if (!pc.pierce) break;
                }
            }
        } else {
            if (Collision::AABB(col, player.getComponent<ColliderComponent>().collider)) {
                // same funnel as contact damage, so a shield charge eats an
                // arrow or a shockwave exactly like it eats a skeleton's touch
                Damage::applyToPlayer(player, pc.damage, shieldCharges,
                                      invincibleTimer, iframeTimer, m_shakeMag);
                pr->destroy();
            }
        }
    }

    // remove dead enemies
    for (auto* e : manager.getGroup(groupEnemies)) {
        if (e->isActive() && e->hasComponent<HealthComponent>() &&
            e->getComponent<HealthComponent>().dead) {
            // The hit-combo multiplier is banked here, so a long unbroken
            // chain is worth real points rather than just looking good.
            const int base = (e->tag == "boss") ? Config::BOSS_SCORE
                                                : Config::ENEMY_SCORE;
            gameScore += (int)(base * comboMultiplier());
            enemiesDefeated++;
            float cx = e->getComponent<TransformComponent>().position.x
                     + e->getComponent<TransformComponent>().width/2;
            float cy = e->getComponent<TransformComponent>().position.y
                     + e->getComponent<TransformComponent>().height/2;
            Particles::emitDeathBurst(cx, cy, 200, 80, 80);
            Vfx::impactRing(cx, cy, 255, 150, 120, 62.f, 0.32f);
            if (e->tag == "boss") Vfx::impactRing(cx, cy, 255, 230, 160, 150.f, 0.7f);
            e->destroy();
        }
    }

    updateHeroState();
    checkLevelState();

    // camera
    auto& pTr = player.getComponent<TransformComponent>();
    CameraSystem::follow(player, camera, m_shake, m_shakeMag);

    // Sprite state is picked last, after physics has decided whether the hero
    // is grounded / rising / falling, so the animation never lags a frame.
    updatePlayerAnimation();
    stepNumbers();

    manager.refresh();
    manager.update();
    (void)pTr;
}

// ===========================================================================
// events
// ===========================================================================
// ===========================================================================
// settings & stats screens
// ===========================================================================
void Game::drawText(const char* fontId, const char* text, int x, int y,
                    SDL_Color c, bool centered) {
    TTF_Font* f = assets->GetFont(fontId);
    if (!f || !text || !*text) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, text, c);
    if (!s) return;
    SDL_Texture* tx = SDL_CreateTextureFromSurface(renderer, s);
    if (!tx) { SDL_FreeSurface(s); return; }
    SDL_Rect r{ centered ? x - s->w / 2 : x,
                centered ? y - s->h / 2 : y, s->w, s->h };

    // Drop shadow.  The HUD sits over a busy parallax backdrop, so plain
    // text can vanish against a bright ridge.  One extra copy of the same
    // texture, tinted — no second glyph rasterisation.
    SDL_SetTextureColorMod(tx, colShadow.r, colShadow.g, colShadow.b);
    SDL_SetTextureAlphaMod(tx, colShadow.a);
    SDL_Rect sr{ r.x + 2, r.y + 2, r.w, r.h };
    SDL_RenderCopy(renderer, tx, nullptr, &sr);

    SDL_SetTextureColorMod(tx, 255, 255, 255);
    SDL_SetTextureAlphaMod(tx, 255);
    SDL_RenderCopy(renderer, tx, nullptr, &r);
    SDL_DestroyTexture(tx);
    SDL_FreeSurface(s);
}

void Game::drawSettings() {
    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    // dim the world behind, so the panel clearly owns the screen
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 12, 16, 30, 225);
    SDL_Rect full{ 0, 0, W, H };
    SDL_RenderFillRect(renderer, &full);

    const int pw = 620, ph = 380;
    const int px = (W - pw) / 2, py = (H - ph) / 2;
    SDL_SetRenderDrawColor(renderer, 28, 34, 56, 250);
    SDL_Rect panel{ px, py, pw, ph };
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 120, 200, 255, 220);
    SDL_Rect border{ px, py, pw, ph };
    SDL_RenderDrawRect(renderer, &border);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    const SDL_Color hi  { 120, 220, 255, 255 };
    const SDL_Color norm{ 225, 230, 240, 255 };
    const SDL_Color dim { 140, 150, 170, 255 };

    drawText("big",   "SETTINGS", W / 2, py + 48, hi, true);
    drawText("small", "up / down to choose     left / right to change",
             W / 2, py + 86, dim, true);

    // ---- rows ----
    struct Row { const char* label; int y; };
    const int rowY[4] = { py + 150, py + 200, py + 250, py + 306 };
    const char* labels[4] = { "Music", "Effects", "Mute all", "Back" };

    for (int i = 0; i < 4; i++) {
        bool sel = (settingsSelection == i);
        if (sel) {   // highlight the selected row
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 120, 200, 255, 40);
            SDL_Rect hl{ px + 24, rowY[i] - 20, pw - 48, 40 };
            SDL_RenderFillRect(renderer, &hl);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
        drawText("body", labels[i], px + 46, rowY[i],
                 sel ? hi : norm, false);

        // value column: a slider for volumes, a state for the toggle
        if (i == 0 || i == 1) {
            int v = (i == 0) ? musicVolume : sfxVolume;
            const int sx = px + 300, sw = 200, sh = 14;
            SDL_SetRenderDrawColor(renderer, 60, 70, 96, 255);
            SDL_Rect track{ sx, rowY[i] - sh / 2, sw, sh };
            SDL_RenderFillRect(renderer, &track);
            int filled = (int)(sw * (v / 128.f));
            SDL_SetRenderDrawColor(renderer,
                                   sel ? (Uint8)120 : (Uint8)90,
                                   sel ? (Uint8)220 : (Uint8)160,
                                   255, 255);
            SDL_Rect fill{ sx, rowY[i] - sh / 2, filled, sh };
            SDL_RenderFillRect(renderer, &fill);

            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d%%", (int)(v * 100.f / 128.f));
            drawText("small", buf, sx + sw + 18, rowY[i], norm, false);
        } else if (i == 2) {
            drawText("body", muted ? "ON" : "OFF", px + 300, rowY[i],
                     muted ? colRed : colGreen, false);
        }
    }
    drawText("small", "Changes are saved automatically.",
             W / 2, py + ph - 26, dim, true);
}

void Game::drawStats() {
    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 12, 16, 30, 225);
    SDL_Rect full{ 0, 0, W, H };
    SDL_RenderFillRect(renderer, &full);

    const int pw = 600, ph = 400;
    const int px = (W - pw) / 2, py = (H - ph) / 2;
    SDL_SetRenderDrawColor(renderer, 28, 34, 56, 250);
    SDL_Rect panel{ px, py, pw, ph };
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 220, 120, 220);
    SDL_Rect border{ px, py, pw, ph };
    SDL_RenderDrawRect(renderer, &border);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    const SDL_Color hi  { 255, 225, 130, 255 };
    const SDL_Color norm{ 226, 232, 245, 255 };
    const SDL_Color dim { 140, 150, 170, 255 };

    drawText("big", "RUN STATS", W / 2, py + 46, hi, true);

    // format mm:ss
    int secs = (int)runTime;
    char tbuf[16];
    std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d", secs / 60, secs % 60);

    auto row = [&](int i, const char* label, const char* value) {
        int y = py + 118 + i * 40;
        // faint separator
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 18);
        SDL_Rect line{ px + 46, y + 20, pw - 92, 1 };
        SDL_RenderFillRect(renderer, &line);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        drawText("body",  label, px + 52, y, dim,  false);
        drawText("body",  value, px + pw - 52, y, norm, false);
    };

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", gameScore);
    row(0, "Score", buf);
    std::snprintf(buf, sizeof(buf), "%d / %d", coinsCollected, totalCoins);
    row(1, "Coins", buf);
    std::snprintf(buf, sizeof(buf), "%d", enemiesDefeated);
    row(2, "Enemies defeated", buf);
    std::snprintf(buf, sizeof(buf), "%d", deaths);
    row(3, "Deaths", buf);
    row(4, "Time", tbuf);
    std::snprintf(buf, sizeof(buf), "%d", bestScore);
    row(5, "Best score", buf);

    drawText("small", "ESC or ENTER to go back",
             W / 2, py + ph - 26, dim, true);
}

void Game::handleMenuInput(SDL_Keycode key) {
    if (state == State::Menu) {
        if (key == SDLK_UP || key == SDLK_w) menuSelection = (menuSelection + 3) % 4;
        else if (key == SDLK_DOWN || key == SDLK_s) menuSelection = (menuSelection + 1) % 4;
        else if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
            if (menuSelection == 0) { restartLevel(); }
            else if (menuSelection == 1) {
                m_returnState = State::Menu;
                instructionsScroll = 0;
                m_instructionsScrollPos = 0.f;
                state = State::Instructions;
            }
            else if (menuSelection == 2) { m_returnState = State::Menu; state = State::Settings; }
            else if (menuSelection == 3) isRunning = false;
        } else if (key == SDLK_ESCAPE) isRunning = false;
    } else if (state == State::Instructions) {
        // Smooth lerped scroll navigation
        if (key == SDLK_UP || key == SDLK_w) {
            instructionsScroll = std::max(0, instructionsScroll - 48);
        } else if (key == SDLK_DOWN || key == SDLK_s) {
            instructionsScroll += 48;
        } else if (key == SDLK_PAGEUP) {
            instructionsScroll = std::max(0, instructionsScroll - 280);
        } else if (key == SDLK_PAGEDOWN) {
            instructionsScroll += 280;
        } else if (key == SDLK_HOME) {
            instructionsScroll = 0;
        } else if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_SPACE) {
            state = m_returnState;
            if (state == State::Menu) m_menuTransition = 0.f;
        }
    }
}

// Settings is a slider list: up/down picks a row, left/right changes it.
void Game::handleSettingsInput(SDL_Keycode key) {
    const int step = 8;
    switch (key) {
        case SDLK_UP:    case SDLK_w:
            settingsSelection = (settingsSelection + 3) % 4; break;
        case SDLK_DOWN:  case SDLK_s:
            settingsSelection = (settingsSelection + 1) % 4; break;
        case SDLK_LEFT:  case SDLK_a:
            if (settingsSelection == 0) musicVolume = std::max(0,   musicVolume - step);
            if (settingsSelection == 1) sfxVolume   = std::max(0,   sfxVolume   - step);
            applyVolumes();
            break;
        case SDLK_RIGHT: case SDLK_d:
            if (settingsSelection == 0) musicVolume = std::min(128, musicVolume + step);
            if (settingsSelection == 1) sfxVolume   = std::min(128, sfxVolume   + step);
            applyVolumes();
            break;
        case SDLK_RETURN: case SDLK_SPACE: case SDLK_KP_ENTER:
            if (settingsSelection == 2) { muted = !muted; applyVolumes(); }
            else if (settingsSelection == 3) { saveSettings(); state = m_returnState; }
            break;
        case SDLK_ESCAPE:
            saveSettings();
            state = m_returnState;
            break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// settings persistence
// ---------------------------------------------------------------------------
void Game::applyVolumes() {
    Mix_VolumeMusic(muted ? 0 : musicVolume);
    Mix_Volume(-1, muted ? 0 : sfxVolume);
    if (muted) Mix_PauseMusic();
    else if (Mix_PausedMusic()) Mix_ResumeMusic();
}

void Game::loadSettings() {
    std::ifstream f(Config::SAVE_FILE, std::ios::binary);
    if (!f) return;                       // first run: keep the defaults
    std::string magic(8, '\0');
    f.read(&magic[0], 7);
    if (magic.compare(0, 7, Config::SAVE_MAGIC) != 0) return;
    f >> musicVolume >> sfxVolume >> bestScore;
    musicVolume = std::max(0, std::min(128, musicVolume));
    sfxVolume   = std::max(0, std::min(128, sfxVolume));
    applyVolumes();
}

void Game::saveSettings() {
    std::ofstream f(Config::SAVE_FILE, std::ios::binary | std::ios::trunc);
    if (!f) return;
    f << Config::SAVE_MAGIC << "\n"
      << musicVolume << " " << sfxVolume << " " << bestScore << "\n";
}

void Game::handleEvents() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { isRunning = false; break; }

        // Mouse wheel scrolls the how-to-play.  This has to be handled before
        // the keyboard filter below and before the Menu/Instructions branch —
        // both of them `continue` on any non-keydown event.
        if (event.type == SDL_MOUSEWHEEL) {
            if (state == State::Instructions) {
                instructionsScroll -= event.wheel.y * 48;
                if (instructionsScroll < 0) instructionsScroll = 0;
            }
            continue;
        }
        if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) continue;
        SDL_Keycode k = event.key.keysym.sym;
        if (state == State::Menu || state == State::Instructions) {
            if (event.type == SDL_KEYDOWN) handleMenuInput(k);
            continue;
        }
        if (state == State::Settings) {
            if (event.type == SDL_KEYDOWN) handleSettingsInput(k);
            continue;
        }
        if (state == State::Stats) {
            if (event.type == SDL_KEYDOWN &&
                (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_SPACE))
                state = m_returnState;
            continue;
        }
        if (state == State::Paused) {
            if (event.type == SDL_KEYDOWN) handlePauseInput(k);
            continue;
        }
        if (event.type == SDL_KEYDOWN) {
            switch (k) {
                case SDLK_w: case SDLK_SPACE: case SDLK_UP:    startJump(); break;
                // Dash moved off K: J/K/L is now melee / ranged / dash, which
                // is what makes the three-attack split reachable with one hand.
                case SDLK_LSHIFT: case SDLK_RSHIFT: case SDLK_l: startDash(); break;
                case SDLK_p:
                    if (state == State::Playing) { state = State::Paused; isPaused = true; }
                    else if (state == State::Paused) { state = State::Playing; isPaused = false; }
                    break;
                case SDLK_h:
                    if (state == State::Playing || state == State::Paused) {
                        m_returnState = state;
                        instructionsScroll = 0;
                        m_instructionsScrollPos = 0.f;
                        state = State::Instructions;
                    }
                    break;
                case SDLK_m:
                    muted = !muted;
                    applyVolumes();          // keep the mixer and the flag in sync
                    break;
                case SDLK_r:
                    if (state == State::GameOver || state == State::Victory) restartLevel();
                    break;
                case SDLK_ESCAPE:
                    if (state == State::Playing) state = State::Paused;
                    else if (state == State::Paused) state = State::Playing;
                    else if (state == State::GameOver || state == State::Victory)
                        state = State::Menu;
                    break;
                case SDLK_o:
                    // settings are reachable mid-run as well as from the menu
                    if (state == State::Playing || state == State::Paused) {
                        m_returnState = state;
                        state = State::Settings;
                    }
                    break;
                case SDLK_i:
                    if (state == State::Playing || state == State::Paused) {
                        m_returnState = state;
                        state = State::Stats;
                    }
                    break;
                default: break;
            }
        }
    }
}

// The pause screen is a real menu now, so it needs its own key handling —
// otherwise P and ESC would both just bounce straight back into the game and
// the Restart / Settings / Stats / Quit rows would be unreachable.
void Game::handlePauseInput(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:   case SDLK_w:
            pauseSelection = (pauseSelection + 4) % 5; break;
        case SDLK_DOWN: case SDLK_s:
            pauseSelection = (pauseSelection + 1) % 5; break;
        case SDLK_p: case SDLK_ESCAPE:
            state = State::Playing; isPaused = false; break;
        case SDLK_RETURN: case SDLK_SPACE: case SDLK_KP_ENTER:
            switch (pauseSelection) {
                case 0: state = State::Playing; isPaused = false; break;
                case 1: restartLevel(); break;
                case 2: m_returnState = State::Paused; state = State::Settings; break;
                case 3: m_returnState = State::Paused; state = State::Stats;    break;
                case 4:
                    if (gameScore > bestScore) bestScore = gameScore;
                    saveSettings();
                    state = State::Menu; isPaused = false;
                    break;
                default: break;
            }
            break;
        default: break;
    }
}

// ===========================================================================
// render
// ===========================================================================
void Game::drawWorld() {
    // Backdrop — cached sky gradient, parallax ridges and drifting clouds.
    // Replaces the old per-pixel loop (850 draw calls a frame) and actually
    // draws something; the tile background layer pointed at tile ids that do
    // not exist in the atlas, so it rendered nothing at all.
    Parallax::drawBackdrop(renderer, Config::WINDOW_WIDTH,
                           Config::WINDOW_HEIGHT, camera);

    // tile culling + draw.
    //
    // NOTE: do NOT set a clip rect here. SDL_RenderSetClipRect takes SCREEN
    // coordinates, so feeding it the camera position clipped the entire world
    // to an off-screen band (camera.y .. camera.y+H on a window only H tall)
    // and every tile, coin, enemy and the player itself vanished, leaving
    // nothing but the parallax backdrop. The per-tile culling below already
    // does the job the clip rect was meant to do.
    int margin = Config::TILE_SIZE;
    for (auto* t : manager.getGroup(groupMap)) {
        if (!t->isActive()) continue;
        auto& tc = t->getComponent<TileComponent>();
        // Cull in WORLD space.  TileComponent::update() rewrites destRect into
        // SCREEN space every frame (position - camera), so culling on destRect
        // compared screen pixels against world pixels and culled the entire
        // world the moment the camera drifted more than one screen from the
        // origin — the map vanished and only the backdrop was left.
        const float wx = tc.position.x, wy = tc.position.y;
        if (wx + tc.destRect.w < camera.x - margin) continue;
        if (wx                 > camera.x + camera.w + margin) continue;
        if (wy + tc.destRect.h < camera.y - margin) continue;
        if (wy                 > camera.y + camera.h + margin) continue;
        t->draw();
    }
    // animated water sits over the water tiles, under everything else
    Parallax::drawWater(renderer, Config::WINDOW_WIDTH,
                        Config::WINDOW_HEIGHT, camera, gameTime);
    // Grass and trees root on the terrain and on the platform tops, so they
    // have to be drawn after both and before anything that walks on them.
    Atmosphere::drawGrass(renderer, camera);
    // Whole props from the tileset: trees, boulders, bushes, crystals.  These
    // replaced the procedurally drawn trees -- the sheet already has two
    // hand-painted 128px trees, and using them is both better looking and
    // guaranteed to match the terrain palette.
    Scenery::draw(renderer, camera, gameTime);
    // platforms
    for (auto* p : manager.getGroup(groupPlatforms)) p->draw();
    // hazards
    for (auto* h : manager.getGroup(groupHazards)) h->draw();
    // facilities
    for (auto* f : manager.getGroup(groupFacilities)) f->draw();
    // coins
    for (auto* c : manager.getGroup(groupCoins)) c->draw();
    // powerups
    for (auto* p : manager.getGroup(groupPowerups)) p->draw();
    // enemies
    for (auto* e : manager.getGroup(groupEnemies)) e->draw();
    // dash trail — behind the hero, over the world. These are real entities
    // (SpriteComponent + AfterimageComponent), so they cannot live in the
    // particle group: Particles::draw filters on ParticleComponent and would
    // skip every ghost. They get their own group and their own pass.
    for (auto* g : manager.getGroup(groupAfterimages)) {
        if (!g->isActive() || !g->hasComponent<SpriteComponent>()) continue;
        g->getComponent<SpriteComponent>().draw();
    }
    // player
    for (auto* p : manager.getGroup(groupPlayers)) p->draw();
    // projectiles
    for (auto* p : manager.getGroup(groupProjectiles)) p->draw();
    // particles on top
    Particles::draw(manager.getGroup(groupParticles));
    Vfx::draw(manager.getGroup(groupParticles));
    drawNumbers();
    // Fog last: it has to veil the hero and the enemies by exactly as much as
    // it veils the terrain, or the foreground pops out of the weather.
    Atmosphere::drawFog(renderer, Config::WINDOW_WIDTH,
                        Config::WINDOW_HEIGHT, camera, gameTime);
}

// Floating damage numbers.  Drawn in world space so they stay pinned to the
// enemy that was hit instead of drifting with the camera.
void Game::drawNumbers() {
    if (m_numbers.empty()) return;
    for (const auto& n : m_numbers) {
        const int sx = (int)n.x - camera.x;
        const int sy = (int)n.y - camera.y;
        if (sx < -80 || sx > camera.w + 80 || sy < -40 || sy > camera.h + 40) continue;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", n.amount);
        // fade out over the last third of the life, and grow the crits
        const float t = n.life / (n.crit ? 0.9f : 0.7f);
        const SDL_Color c = n.crit
            ? SDL_Color{ 255, 220, 90,  (Uint8)(255 * std::min(1.f, t * 1.6f)) }
            : SDL_Color{ 255, 255, 255, (Uint8)(235 * std::min(1.f, t * 1.6f)) };
        drawText(n.crit ? "body" : "small", buf, sx, sy, c, true);
    }
}

void Game::drawMinimap() {
    int mw = 220, mh = 90;
    int mx = Config::WINDOW_WIDTH - mw - 12;
    int my = Config::WINDOW_HEIGHT - mh - 12;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_Rect bg{ mx, my, mw, mh };
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 200, 200, 220, 200);
    SDL_RenderDrawRect(renderer, &bg);
    float sx = (float)mw / Config::MAP_PIXEL_WIDTH;
    float sy = (float)mh / Config::MAP_PIXEL_HEIGHT;
    // enemies (red)
    SDL_SetRenderDrawColor(renderer, 255, 80, 80, 200);
    for (auto* e : manager.getGroup(groupEnemies)) {
        if (!e->isActive()) continue;
        auto& t = e->getComponent<TransformComponent>();
        SDL_Rect r{ mx + (int)(t.position.x * sx),
                    my + (int)(t.position.y * sy), 2, 2 };
        SDL_RenderFillRect(renderer, &r);
    }
    // powerups (green)
    SDL_SetRenderDrawColor(renderer, 90, 255, 140, 200);
    for (auto* p : manager.getGroup(groupPowerups)) {
        if (!p->isActive()) continue;
        auto& t = p->getComponent<TransformComponent>();
        SDL_Rect r{ mx + (int)(t.position.x * sx),
                    my + (int)(t.position.y * sy), 3, 3 };
        SDL_RenderFillRect(renderer, &r);
    }
    // player (cyan)
    SDL_SetRenderDrawColor(renderer, 80, 220, 255, 255);
    auto& pt = manager.getGroup(groupPlayers)[0]->getComponent<TransformComponent>();
    SDL_Rect pr{ mx + (int)(pt.position.x * sx) - 2,
                 my + (int)(pt.position.y * sy) - 2, 4, 4 };
    SDL_RenderFillRect(renderer, &pr);
    // viewport rect
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 90);
    SDL_Rect vp{ mx + (int)(camera.x * sx),
                 my + (int)(camera.y * sy),
                 (int)(camera.w * sx), (int)(camera.h * sy) };
    SDL_RenderDrawRect(renderer, &vp);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void Game::drawHUD() {
    // top bar
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 110);
    // Tall enough for health + mana + the combo counter, which all stack in
    // the top-left now.
    SDL_Rect topBar{ 0, 0, Config::WINDOW_WIDTH, 112 };
    SDL_RenderFillRect(renderer, &topBar);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // HP bar
    auto& p = *manager.getGroup(groupPlayers)[0];
    auto& hp = p.getComponent<HealthComponent>();
    int bx = 16, by = 16, bw = 220, bh = 18;
    SDL_SetRenderDrawColor(renderer, 60, 30, 30, 220);
    SDL_Rect bbg{ bx-2, by-2, bw+4, bh+4 };
    SDL_RenderFillRect(renderer, &bbg);
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_Rect bbg2{ bx, by, bw, bh };
    SDL_RenderFillRect(renderer, &bbg2);
    int filled = (int)(bw * (hp.hp / hp.hpMax));
    SDL_SetRenderDrawColor(renderer, 80, 220, 120, 255);
    SDL_Rect bfill{ bx, by, filled, bh };
    SDL_RenderFillRect(renderer, &bfill);

    // mana bar — under the health bar, same footprint
    {
        const int my = by + bh + 5, mh = 10;
        SDL_SetRenderDrawColor(renderer, 30, 40, 60, 220);
        SDL_Rect mbg{ bx - 2, my - 2, bw + 4, mh + 4 };
        SDL_RenderFillRect(renderer, &mbg);
        SDL_SetRenderDrawColor(renderer, 18, 24, 40, 255);
        SDL_Rect mbg2{ bx, my, bw, mh };
        SDL_RenderFillRect(renderer, &mbg2);
        const int mf = (int)(bw * (mana / Config::MANA_MAX));
        // goes brighter while regenerating is stalled, so a spend reads
        SDL_SetRenderDrawColor(renderer,
            (Uint8)(80 + (manaDelay > 0 ? 60 : 0)),
            (Uint8)(170 + (manaDelay > 0 ? 40 : 0)), 255, 255);
        SDL_Rect mfill{ bx, my, mf, mh };
        SDL_RenderFillRect(renderer, &mfill);
        // tick marks at each bolt's cost
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 70);
        for (int cost = 12; cost < (int)Config::MANA_MAX; cost += 12) {
            SDL_Rect tick{ bx + (int)(bw * cost / Config::MANA_MAX), my, 1, mh };
            SDL_RenderFillRect(renderer, &tick);
        }
    }

    // lives (hearts)
    if (auto* heart = assets->GetTexture("heart")) {
        for (int i = 0; i < maxLives; i++) {
            SDL_Rect dst{ bx + bw + 14 + i*22, by, 20, 20 };
            SDL_SetTextureAlphaMod(heart, i < lives ? 255 : 90);
            SDL_RenderCopy(renderer, heart, nullptr, &dst);
        }
        SDL_SetTextureAlphaMod(heart, 255);
    }

    // hit combo — the number that turns a brawl into a score run
    if (hitCombo > 1) {
        char cb[32];
        std::snprintf(cb, sizeof(cb), "x%d  %.1fx", hitCombo, comboMultiplier());
        SDL_Color cc{ 255, 200, 80, 255 };
        drawText("title", cb, bx + 4, by + bh + 30, cc, false);
        // drain bar under the counter, so you can see the chain expiring
        const int cw = 150;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 200, 80, 180);
        SDL_Rect cf{ bx + 4, by + bh + 50,
                     (int)(cw * (hitComboT / Config::HITCOMBO_TIMEOUT)), 4 };
        SDL_RenderFillRect(renderer, &cf);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    // score
    if (auto* f = assets->GetFont("title")) {
        SDL_Color y{ 255, 230, 90, 255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, std::to_string(gameScore).c_str(), y);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect r{ Config::WINDOW_WIDTH/2 - s->w/2, 14, s->w, s->h };
            SDL_RenderCopy(renderer, t, nullptr, &r);
            SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }
    // coins
    if (auto* f = assets->GetFont("body")) {
        std::string cs = "Coins " + std::to_string(coinsCollected)
                       + "/" + std::to_string(totalCoins);
        SDL_Color y{ 255, 230, 90, 255 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, cs.c_str(), y);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect r{ Config::WINDOW_WIDTH/2 - s->w/2, 50, s->w, s->h };
            SDL_RenderCopy(renderer, t, nullptr, &r);
            SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }
    // keys
    if (keys > 0) {
        if (auto* f = assets->GetFont("body")) {
            std::string ks = "Keys: " + std::to_string(keys);
            SDL_Color c{ 80, 220, 255, 255 };
            SDL_Surface* s = TTF_RenderUTF8_Blended(f, ks.c_str(), c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_Rect r{ Config::WINDOW_WIDTH/2 - 80, 70, s->w, s->h };
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t); SDL_FreeSurface(s);
            }
        }
    }

    // bottom bar
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 110);
    SDL_Rect botBar{ 0, Config::WINDOW_HEIGHT - 36, Config::WINDOW_WIDTH, 36 };
    SDL_RenderFillRect(renderer, &botBar);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    if (auto* f = assets->GetFont("small")) {
        std::string hint = "[A/D] move  [W/Space/Up] jump  [Shift/L] dash  "
                           "[J/Z] melee  [K/X] bolt  "
                           "[P/ESC] pause  [O] settings  [I] stats  [H] help  [M] mute";
        SDL_Color w{ 200, 200, 200, 200 };
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, hint.c_str(), w);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect r{ 12, Config::WINDOW_HEIGHT - 28, s->w, s->h };
            SDL_RenderCopy(renderer, t, nullptr, &r);
            SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }

    // buff timers on the right
    if (auto* f = assets->GetFont("body")) {
        int y = 124;
        auto drawBuff = [&](const std::string& txt, SDL_Color c) {
            SDL_Surface* s = TTF_RenderUTF8_Blended(f, txt.c_str(), c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_Rect r{ Config::WINDOW_WIDTH - 200, y, s->w, s->h };
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t); SDL_FreeSurface(s);
                y += 22;
            }
        };
        SDL_Color colMagnet { 200, 130, 255, 255 };
        SDL_Color colShield { 130, 200, 255, 255 };
        if (speedBoostTimer > 0)   drawBuff("SPEED " + std::to_string((int)std::ceil(speedBoostTimer)) + "s", colGreen);
        if (invincibleTimer > 0)   drawBuff("INVINCIBLE " + std::to_string((int)std::ceil(invincibleTimer)) + "s", colCyan);
        if (dmgBoostTimer > 0)     drawBuff("DAMAGE x1.5 " + std::to_string((int)std::ceil(dmgBoostTimer)) + "s", colRed);
        if (magnetTimer > 0)       drawBuff("MAGNET " + std::to_string((int)std::ceil(magnetTimer)) + "s", colMagnet);
        if (shieldCharges > 0)     drawBuff("SHIELD x" + std::to_string(shieldCharges), colShield);
        if (coinMultTimer > 0)     drawBuff("COINS x2 " + std::to_string((int)std::ceil(coinMultTimer)) + "s", colYellow);
        if (dashCooldown > 0 && !dashActive) drawBuff("DASH CD " + std::to_string((int)std::ceil(dashCooldown*10)/10) + "s", colWhite);
        if (dashActive)            drawBuff("DASHING", colCyan);
        if (comboStep >= 0 && comboTimer > 0) drawBuff("COMBO x" + std::to_string(comboStep+1), colYellow);
    }

    // boss bar
    for (auto* e : manager.getGroup(groupEnemies)) {
        if (e->tag != "boss" || !e->isActive()) continue;
        auto& hp = e->getComponent<HealthComponent>();
        if (hp.dead) continue;
        // D6: gate the bar on engagement.  It used to be drawn from the first
        // frame of the level, which spoils the boss and clutters the screen.
        {
            auto& btr = e->getComponent<TransformComponent>();
            auto& htr = p.getComponent<TransformComponent>();
            float bdx = std::fabs((btr.position.x + btr.width  * 0.5f) -
                                  (htr.position.x + htr.width  * 0.5f));
            if (bdx >= Config::BOSS_BAR_RANGE && hp.hp >= hp.hpMax) continue;
        }
        int bw2 = 600, bh2 = 16;
        int bx2 = Config::WINDOW_WIDTH/2 - bw2/2;
        int by2 = Config::WINDOW_HEIGHT - 80;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_Rect bb{ bx2-2, by2-2, bw2+4, bh2+4 };
        SDL_RenderFillRect(renderer, &bb);
        SDL_SetRenderDrawColor(renderer, 40, 20, 20, 255);
        SDL_Rect bb2{ bx2, by2, bw2, bh2 };
        SDL_RenderFillRect(renderer, &bb2);
        int fill = (int)(bw2 * hp.hp / hp.hpMax);
        SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255);
        SDL_Rect bbf{ bx2, by2, fill, bh2 };
        SDL_RenderFillRect(renderer, &bbf);
        if (auto* f = assets->GetFont("body")) {
            SDL_Color c{ 255, 230, 90, 255 };
            SDL_Surface* s = TTF_RenderUTF8_Blended(f, "THE OVERLORD", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_Rect r{ bx2, by2 - 22, s->w, s->h };
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t); SDL_FreeSurface(s);
            }
        }
        break;
    }

    drawMinimap();
}

// G// ===========================================================================
// UI animations & helpers (Task 2)
// ===========================================================================
void Game::initFireflies() {
    m_firefliesInit = true;
    for (int i = 0; i < N_FIREFLIES; i++) {
        m_fireflies[i].x = (float)((i * 137 + 53) % Config::WINDOW_WIDTH);
        m_fireflies[i].y = (float)((i * 199 + 71) % Config::WINDOW_HEIGHT);
        m_fireflies[i].vx = -12.f + (float)((i * 31) % 25);
        m_fireflies[i].vy = -18.f - (float)((i * 17) % 22);
        m_fireflies[i].size = 1.5f + (float)(i % 3);
        m_fireflies[i].phase = (float)i * 0.45f;
        m_fireflies[i].pulseSpeed = 1.5f + (float)(i % 5) * 0.4f;
        int pick = i % 3;
        if (pick == 0)      { m_fireflies[i].r = 255; m_fireflies[i].g = 215; m_fireflies[i].b =  90; }
        else if (pick == 1) { m_fireflies[i].r =  90; m_fireflies[i].g = 220; m_fireflies[i].b = 255; }
        else                { m_fireflies[i].r = 110; m_fireflies[i].g = 255; m_fireflies[i].b = 160; }
    }
}

void Game::updateUIAnimations(float dt) {
    m_menuTime += dt;
    if (m_menuTransition < 1.0f) {
        m_menuTransition = std::min(1.0f, m_menuTransition + dt * 3.5f);
    }

    for (int i = 0; i < 4; i++) {
        float target = (menuSelection == i) ? 1.0f : 0.0f;
        m_buttonAnim[i] += (target - m_buttonAnim[i]) * std::min(1.0f, 15.0f * dt);
    }

    m_instructionsScrollPos += ((float)instructionsScroll - m_instructionsScrollPos) * std::min(1.0f, 14.0f * dt);

    if (!m_firefliesInit) initFireflies();
    for (int i = 0; i < N_FIREFLIES; i++) {
        m_fireflies[i].x += (m_fireflies[i].vx + std::sin(m_fireflies[i].phase + m_menuTime * 1.5f) * 14.f) * dt;
        m_fireflies[i].y += m_fireflies[i].vy * dt;
        if (m_fireflies[i].y < -20.f) m_fireflies[i].y = (float)Config::WINDOW_HEIGHT + 10.f;
        if (m_fireflies[i].y > (float)Config::WINDOW_HEIGHT + 20.f) m_fireflies[i].y = -10.f;
        if (m_fireflies[i].x < -20.f) m_fireflies[i].x = (float)Config::WINDOW_WIDTH + 10.f;
        if (m_fireflies[i].x > (float)Config::WINDOW_WIDTH + 20.f) m_fireflies[i].x = -10.f;
    }
}

void Game::drawKeycap(const char* key, int x, int y, int& outW) {
    TTF_Font* f = assets->GetFont("small");
    if (!f || !key || !*key) { outW = 0; return; }
    int tw = 0, th = 0;
    TTF_SizeText(f, key, &tw, &th);
    int padX = 8, padY = 4;
    int kw = tw + padX * 2;
    int kh = th + padY * 2;
    outW = kw;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 36, 44, 62, 235);
    SDL_Rect r{ x, y - kh / 2, kw, kh };
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 130, 180, 220);
    SDL_RenderDrawLine(renderer, r.x + 1, r.y, r.x + r.w - 2, r.y);
    SDL_SetRenderDrawColor(renderer, 70, 90, 130, 160);
    SDL_RenderDrawRect(renderer, &r);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    drawText("small", key, x + kw / 2, y, SDL_Color{ 245, 250, 255, 255 }, true);
}

// ---------------------------------------------------------------------------
// Main Menu: Animated Parallax Backdrop + Drifting Fireflies
// ---------------------------------------------------------------------------
void Game::drawMenuBackdrop() {
    SDL_Rect menuCam{ (int)(m_menuTime * 24.f), 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT };
    Parallax::drawBackdrop(renderer, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT, menuCam);

    // Vignette / twilight darkening gradient
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < Config::WINDOW_HEIGHT; y += 4) {
        float t = (float)y / (float)Config::WINDOW_HEIGHT;
        Uint8 alpha = (Uint8)(135 + t * 75);
        SDL_SetRenderDrawColor(renderer, 8, 12, 24, alpha);
        SDL_Rect r{ 0, y, Config::WINDOW_WIDTH, 4 };
        SDL_RenderFillRect(renderer, &r);
    }

    // Drifting fireflies
    if (!m_firefliesInit) initFireflies();
    for (int i = 0; i < N_FIREFLIES; i++) {
        float pulse = 0.45f + 0.55f * std::sin(m_fireflies[i].phase + m_menuTime * m_fireflies[i].pulseSpeed);
        if (pulse < 0.05f) continue;
        int fx = (int)m_fireflies[i].x;
        int fy = (int)m_fireflies[i].y;
        int sz = (int)m_fireflies[i].size;
        SDL_SetRenderDrawColor(renderer, m_fireflies[i].r, m_fireflies[i].g, m_fireflies[i].b, (Uint8)(55 * pulse));
        SDL_Rect halo{ fx - sz, fy - sz, sz * 2 + 1, sz * 2 + 1 };
        SDL_RenderFillRect(renderer, &halo);

        SDL_SetRenderDrawColor(renderer, 255, 255, 235, (Uint8)(210 * pulse));
        SDL_Rect core{ fx, fy, sz, sz };
        SDL_RenderFillRect(renderer, &core);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// ---------------------------------------------------------------------------
// Main Menu: Title with glow pulse, pill buttons with slide/scale lerp, badge
// ---------------------------------------------------------------------------
void Game::drawMenu() {
    drawMenuBackdrop();

    const int W = Config::WINDOW_WIDTH;
    float glow = 0.5f + 0.5f * std::sin(m_menuTime * 2.6f);
    float slideOffset = (1.0f - m_menuTransition) * -30.0f;
    int titleY = (int)(76 + slideOffset);

    // ---- Title with drop-shadow + glow pulse ----
    if (auto* fBig = assets->GetFont("big")) {
        SDL_Color cWhite{ 255, 255, 255, 255 };
        SDL_Surface* sTitle = TTF_RenderUTF8_Blended(fBig, "DUNGEON  RUSH  2", cWhite);
        if (sTitle) {
            SDL_Texture* txTitle = SDL_CreateTextureFromSurface(renderer, sTitle);
            if (txTitle) {
                int tx = W / 2 - sTitle->w / 2;

                // 1. Drop shadow (+3, +4)
                SDL_SetTextureColorMod(txTitle, 0, 0, 0);
                SDL_SetTextureAlphaMod(txTitle, 190);
                SDL_Rect shadowR{ tx + 3, titleY + 4, sTitle->w, sTitle->h };
                SDL_RenderCopy(renderer, txTitle, nullptr, &shadowR);

                // 2. Warm amber/gold ambient glow pulse
                Uint8 glowAlpha = (Uint8)(35 + 45 * glow);
                SDL_SetTextureColorMod(txTitle, 255, 190, 50);
                SDL_SetTextureAlphaMod(txTitle, glowAlpha);
                for (int dx = -2; dx <= 2; dx += 2) {
                    for (int dy = -2; dy <= 2; dy += 2) {
                        if (dx == 0 && dy == 0) continue;
                        SDL_Rect glowR{ tx + dx, titleY + dy, sTitle->w, sTitle->h };
                        SDL_RenderCopy(renderer, txTitle, nullptr, &glowR);
                    }
                }

                // 3. Crisp vibrant golden foreground
                SDL_SetTextureColorMod(txTitle, 255, (Uint8)(225 + 25 * glow), 105);
                SDL_SetTextureAlphaMod(txTitle, 255);
                SDL_Rect titleR{ tx, titleY, sTitle->w, sTitle->h };
                SDL_RenderCopy(renderer, txTitle, nullptr, &titleR);

                SDL_DestroyTexture(txTitle);
            }
            SDL_FreeSurface(sTitle);
        }

        // Subtitle badge
        int subY = titleY + 68;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 16, 22, 38, 190);
        SDL_Rect subBadge{ W / 2 - 180, subY, 360, 26 };
        SDL_RenderFillRect(renderer, &subBadge);
        SDL_SetRenderDrawColor(renderer, 80, 160, 220, 90);
        SDL_RenderDrawRect(renderer, &subBadge);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        drawText("body", "Modern 2D Platformer  ·  SDL2 + ECS", W / 2, subY + 13,
                 SDL_Color{ 200, 220, 245, 230 }, true);
    }

    // ---- Best Score Badge ----
    if (bestScore > 0) {
        int badgeW = 210, badgeH = 26;
        int badgeX = W / 2 - badgeW / 2;
        int badgeY = titleY + 104;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 38, 30, 14, 220);
        SDL_Rect bgR{ badgeX, badgeY, badgeW, badgeH };
        SDL_RenderFillRect(renderer, &bgR);
        SDL_SetRenderDrawColor(renderer, 255, 215, 60, 200);
        SDL_RenderDrawRect(renderer, &bgR);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        char bb[48];
        std::snprintf(bb, sizeof(bb), "★ BEST SCORE  %d", bestScore);
        drawText("small", bb, W / 2, badgeY + badgeH / 2,
                 SDL_Color{ 255, 230, 120, 255 }, true);
    }

    // ---- Pill Buttons with Hover/Selected Slide + Scale Lerp ----
    const char* buttonLabels[4] = { "PLAY", "HOW TO PLAY", "SETTINGS", "QUIT" };
    const int baseY = (bestScore > 0) ? 255 : 245;

    for (int i = 0; i < 4; i++) {
        float lerp = m_buttonAnim[i];
        int slideX = (int)(lerp * 14.f);
        int bw = 320 + (int)(lerp * 24.f);
        int bh = 52 + (int)(lerp * 4.f);
        int bx = W / 2 - bw / 2 + slideX;
        int by = baseY + i * 68 - (int)(lerp * 2.f);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Glow halo around selected button
        if (lerp > 0.05f) {
            Uint8 glowA = (Uint8)(70 * lerp * (0.8f + 0.2f * std::sin(m_menuTime * 6.f)));
            SDL_SetRenderDrawColor(renderer, 60, 200, 255, glowA);
            SDL_Rect haloR{ bx - 4, by - 4, bw + 8, bh + 8 };
            SDL_RenderFillRect(renderer, &haloR);
        }

        // Pill shape fill (center + beveled insets)
        Uint8 rFill = (Uint8)(22 + lerp * (20 - 22));
        Uint8 gFill = (Uint8)(26 + lerp * (68 - 26));
        Uint8 bFill = (Uint8)(42 + lerp * (64 - 42));
        Uint8 aFill = (Uint8)(195 + lerp * 50);
        SDL_SetRenderDrawColor(renderer, rFill, gFill, bFill, aFill);

        SDL_Rect pillMain{ bx + 6, by, bw - 12, bh };
        SDL_RenderFillRect(renderer, &pillMain);
        SDL_Rect pillInset{ bx, by + 4, bw, bh - 8 };
        SDL_RenderFillRect(renderer, &pillInset);

        // Top highlight line
        SDL_SetRenderDrawColor(renderer, 120, 220, 255, (Uint8)(50 + 130 * lerp));
        SDL_RenderDrawLine(renderer, bx + 8, by + 1, bx + bw - 9, by + 1);

        // Pill border
        Uint8 rBorder = (Uint8)(70 + lerp * (100 - 70));
        Uint8 gBorder = (Uint8)(85 + lerp * (235 - 85));
        Uint8 bBorder = (Uint8)(125 + lerp * (255 - 125));
        Uint8 aBorder = (Uint8)(140 + lerp * 115);
        SDL_SetRenderDrawColor(renderer, rBorder, gBorder, bBorder, aBorder);
        SDL_Rect pillBorder{ bx + 1, by + 1, bw - 2, bh - 2 };
        SDL_RenderDrawRect(renderer, &pillBorder);

        // Left selection indicator pip
        if (lerp > 0.1f) {
            Uint8 pipA = (Uint8)(255 * lerp);
            SDL_SetRenderDrawColor(renderer, 255, 230, 110, pipA);
            int pipSize = 8;
            SDL_Rect pipR{ bx + 20, by + bh / 2 - pipSize / 2, pipSize, pipSize };
            SDL_RenderFillRect(renderer, &pipR);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // Button label
        SDL_Color textCol = (menuSelection == i)
            ? SDL_Color{ 255, 255, 255, 255 }
            : SDL_Color{ 190, 205, 225, 215 };
        drawText("title", buttonLabels[i], bx + bw / 2 + (lerp > 0.1f ? 8 : 0),
                 by + bh / 2, textCol, true);
    }

    // ---- Footer hints with key-cap pills ----
    {
        const int footY = 570;
        struct Hint { const char* key; const char* desc; };
        // Only keys the menu actually handles.  "O Settings" and "I Stats"
        // were advertised here but handleMenuInput ignores both, so the bar
        // was teaching dead keys (R-14).
        const Hint hints[] = {
            { "W / S", "Select" },
            { "ENTER", "Confirm" },
            { "ESC", "Quit" },
        };
        const int nHints = (int)(sizeof(hints) / sizeof(hints[0]));

        TTF_Font* fSmall = assets->GetFont("small");
        int totalW = 0;
        int kw[3]{}, dw[3]{};
        for (int i = 0; i < nHints; i++) {
            int th = 0;
            if (fSmall) {
                TTF_SizeText(fSmall, hints[i].key, &kw[i], &th);
                TTF_SizeText(fSmall, hints[i].desc, &dw[i], &th);
            }
            kw[i] += 16;
            totalW += kw[i] + 6 + dw[i] + 24;
        }
        totalW -= 24;

        int curX = W / 2 - totalW / 2;
        for (int i = 0; i < nHints; i++) {
            int outKw = 0;
            drawKeycap(hints[i].key, curX, footY, outKw);
            curX += outKw + 6;
            drawText("small", hints[i].desc, curX, footY,
                     SDL_Color{ 160, 185, 210, 220 }, false);
            curX += dw[i] + 24;
        }
    }
}

// ---------------------------------------------------------------------------
// How to Play: Card sections with icons, 2-column key/action rows,
// sticky header + thin scrollbar + fade edges, smooth float scroll lerp
// ---------------------------------------------------------------------------
namespace {

    struct HelpItem {
        const char* key;
        const char* title;
        const char* desc;
    };

    struct HelpCard {
        const char* icon;
        const char* title;
        SDL_Color   accent;
        int         itemCount;
        const HelpItem* items;
    };

    static const HelpItem ITEMS_OBJECTIVE[] = {
        { "MISSION", "Cross the Dungeon", "Retrieve the Key at col 143, unlock the Boss Door at col 175, and defeat The Overlord." },
        { "LIVES",   "3 Lives per Run",   "You have 3 lives. Losing all of them ends the run; press R to restart instantly." },
    };

    static const HelpItem ITEMS_MOVEMENT[] = {
        { "A / D",          "Run Left / Right", "Hold to build acceleration, release to skid to a stop." },
        { "Left / Right",   "Arrow Controls",   "Full arrow-key parity for movement and jumping." },
        { "W / Space / Up", "Variable Jump",    "HOLD for full jump, TAP for short hop. Double jump in mid-air!" },
        { "S / Down",       "Look Down / Dive", "Hold Down in air while pressing J to trigger a devastating Dive Slash." },
        { "Shift / L",      "Invincible Dash",  "Pass through enemies safely! 0.7s ground cooldown; 1 air dash per jump." },
        { "Wall Jump",      "Wall Rebound",     "Touch any wall while airborne and press Jump to rebound upward." },
    };

    static const HelpItem ITEMS_COMBAT[] = {
        { "J / Z (tap)",    "3-Hit Melee Combo", "Slash 1 -> Slash 2 -> Slash 3 finisher. Finisher hits hardest with heavy knockback." },
        { "J + Up",         "Rising Uppercut",   "Launches enemies skyward. In mid-air, grants an extra vertical recovery lift." },
        { "J + Down (air)", "Dive Slash Slam",   "Slam straight down into enemies. Bounces high on impact to clear hazards." },
        { "J during dash",  "Dash Slash",        "High-velocity lunge dealing heavy damage and staggering targets." },
        { "K / X (tap)",    "Arcane Bolt",       "Fast ranged projectile (12 mana). Keep your distance and snipe dangerous targets." },
        { "K / X (hold)",   "Piercing Heavy Bolt","Hold 0.40s (30 mana): bigger, faster, and pierces EVERY enemy in its path!" },
    };

    static const HelpItem ITEMS_SYSTEMS[] = {
        { "Mana Bar",       "Arcane Energy",    "Blue bar refills automatically after casting pause. Rhythm your shots, do not spam." },
        { "Combo Meter",    "Hit Multiplier",   "Hits build combo up to 3.00x score multiplier! Banked when enemy dies (resets in 2.6s)." },
    };

    static const HelpItem ITEMS_DEFENCE[] = {
        { "i-frames",       "Damage Immunity",  "After taking a hit, you flicker for 0.8s and are invulnerable. Reposition safely!" },
        { "Shield",         "Aegis Barrier",    "Shield charges absorb complete hits outright before health is damaged." },
        { "Shrine",         "Sacred Statues",   "Stand near sacred statues scattered across the dungeon to replenish HP." },
    };

    static const HelpItem ITEMS_ENEMIES[] = {
        { "Skeleton",       "Patrol Warrior",   "Marches a beat, then charges on sight. Punish the turn-around with a combo." },
        { "Slime",          "Bouncing Ooze",    "Hops on a steady rhythm. Predictable trajectory, easy to juggle with uppercuts." },
        { "Bat",            "Aerial Hunter",    "Swoops directly toward you and ignores gravity. Uppercut is your best friend." },
        { "Mage",           "Arcane Sentry",    "Stationed on high platforms lobbing slow fireballs. Close the gap or snipe back." },
        { "Archer",         "Sniper Scout",     "Telegraphs shot for 0.5s before firing. Backs away if you get too close." },
        { "Bomber",         "Explosive Runner", "Lights fuse upon proximity and detonates in wide AoE. Chain into others!" },
        { "The Overlord",   "Dungeon Boss",     "Three phases: charging rushes, floor shockwaves (JUMP them!), and bolt spreads." },
    };

    static const HelpItem ITEMS_POWERUPS[] = {
        { "Red Heart",      "Instant Heal",     "Restores a generous portion of health immediately." },
        { "Green Gem",      "Speed Boost",      "Movement speed multiplied x1.6 for 6 seconds." },
        { "Blue Gem",       "Invincibility",    "Total invulnerability for 4 seconds - walk straight through hazards and enemies!" },
        { "Yellow Gem",     "Double Jump",      "Grants extra aerial jumps for 8 seconds." },
        { "Pink Gem",       "Damage Boost",     "Weapon damage multiplied x1.5 for 12 seconds." },
        { "Purple Gem",     "Coin Magnet",      "Pulls all nearby coins directly into you for 10 seconds." },
        { "Cyan Gem",       "Shield Aegis",     "Grants shield charges that negate damage completely." },
        { "Gold Coin",      "Coin Doubler",     "Double point value on all collected coins for 10 seconds." },
    };

    static const HelpItem ITEMS_FACILITIES[] = {
        { "Flag",           "Checkpoint",       "Respawn here upon death instead of restarting back at the level start." },
        { "Statue",         "Healing Shrine",   "Rest at ancient statues to mend wounds." },
        { "Key & Door",     "Boss Chamber",     "Find the golden Key at col 143 to unlock the massive gate at 175." },
        { "Spikes",         "Lethal Hazards",   "Instant contact damage. There is no falling through safely." },
        { "Moving Lifts",   "Ferry Platforms",  "Horizontal and vertical moving platforms carry you over deep water and chasms." },
    };

    static const HelpItem ITEMS_HOTKEYS[] = {
        { "P / ESC",        "Pause Menu",       "Open pause menu with Resume, Restart, Settings, Stats, or Quit." },
        { "H",              "Field Guide",      "Toggle this How To Play manual at any time, mid-run included." },
        { "O",              "Audio Settings",   "Open audio sliders to adjust Music and SFX volume." },
        { "I",              "Run Statistics",   "Inspect your score, coins banked, kills, deaths, and speedrun clock." },
        { "M",              "Quick Mute",       "Instantly toggle audio mute on or off." },
        { "R",              "Restart Run",      "Quickly restart after Game Over or Victory." },
    };

    static const HelpCard CARDS[] = {
        { "[★]", "MISSION OBJECTIVE",    SDL_Color{ 255, 215,  70, 255 }, (int)(sizeof(ITEMS_OBJECTIVE)  / sizeof(ITEMS_OBJECTIVE[0])),  ITEMS_OBJECTIVE  },
        { "[▲]", "MOVEMENT & AGILITY",   SDL_Color{  80, 220, 255, 255 }, (int)(sizeof(ITEMS_MOVEMENT)   / sizeof(ITEMS_MOVEMENT[0])),   ITEMS_MOVEMENT   },
        { "[⚔]", "COMBAT & ATTACKS",     SDL_Color{ 255, 105, 105, 255 }, (int)(sizeof(ITEMS_COMBAT)     / sizeof(ITEMS_COMBAT[0])),     ITEMS_COMBAT     },
        { "[⚡]", "ARCANE SYSTEMS",       SDL_Color{ 205, 130, 255, 255 }, (int)(sizeof(ITEMS_SYSTEMS)    / sizeof(ITEMS_SYSTEMS[0])),    ITEMS_SYSTEMS    },
        { "[♥]", "SURVIVAL & DEFENCE",   SDL_Color{  90, 245, 150, 255 }, (int)(sizeof(ITEMS_DEFENCE)    / sizeof(ITEMS_DEFENCE[0])),    ITEMS_DEFENCE    },
        { "[☠]", "BESTIARY & INTEL",     SDL_Color{ 255,  90,  90, 255 }, (int)(sizeof(ITEMS_ENEMIES)    / sizeof(ITEMS_ENEMIES[0])),    ITEMS_ENEMIES    },
        { "[◆]", "POWERUPS & PICKUPS",   SDL_Color{ 255, 200,  60, 255 }, (int)(sizeof(ITEMS_POWERUPS)   / sizeof(ITEMS_POWERUPS[0])),   ITEMS_POWERUPS   },
        { "[⚐]", "FACILITIES & HAZARDS", SDL_Color{  70, 230, 220, 255 }, (int)(sizeof(ITEMS_FACILITIES) / sizeof(ITEMS_FACILITIES[0])), ITEMS_FACILITIES },
        { "[⚙]", "HOTKEYS & CONTROLS",   SDL_Color{ 140, 190, 255, 255 }, (int)(sizeof(ITEMS_HOTKEYS)    / sizeof(ITEMS_HOTKEYS[0])),    ITEMS_HOTKEYS    },
    };
    static const int CARD_COUNT = (int)(sizeof(CARDS) / sizeof(CARDS[0]));

} // namespace

void Game::drawInstructionsPanel(bool overWorld) {
    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    const int panelX = 60, panelY = 30;
    const int panelW = W - 120, panelH = H - 60;
    // Header height is derived from the two fonts' line skips instead of a
    // hard-coded 62: the 32 px title alone needs ~42 px, so the old value put
    // the subtitle on top of it (R-13).
    TTF_Font* fTitle = assets->GetFont("title");
    TTF_Font* fSmallH = assets->GetFont("small");
    const int titleSkip = fTitle  ? TTF_FontLineSkip(fTitle)  : 42;
    const int smallSkip = fSmallH ? TTF_FontLineSkip(fSmallH) : 17;
    const int headPad   = 10;
    const int headH = headPad + titleSkip + smallSkip + 10;
    const int footH = 34;

    SDL_Rect panel{ panelX, panelY, panelW, panelH };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 14, 18, 30, overWorld ? 245 : 252);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 60, 180, 230, 220);
    SDL_RenderDrawRect(renderer, &panel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // ---- Content viewport ----
    const int viewX = panelX + 6;
    const int viewY = panelY + headH + 4;
    const int viewW = panelW - 12;
    const int viewH = panelH - headH - footH - 8;
    SDL_Rect view{ viewX, viewY, viewW, viewH };

    const int cardHeadH = 34;
    const int itemH = 32;
    int contentH = 0;
    for (int i = 0; i < CARD_COUNT; i++) {
        int cardH = cardHeadH + CARDS[i].itemCount * itemH + 14;
        contentH += cardH + 16;
    }

    const int maxScroll = std::max(0, contentH - viewH + 20);
    if (instructionsScroll > maxScroll) instructionsScroll = maxScroll;
    if (instructionsScroll < 0)        instructionsScroll = 0;

    int scrollY = (int)m_instructionsScrollPos;

    // Render clipped cards
    SDL_RenderSetClipRect(renderer, &view);

    int cardOffset = 0;
    for (int i = 0; i < CARD_COUNT; i++) {
        const auto& card = CARDS[i];
        int cardH = cardHeadH + card.itemCount * itemH + 14;
        int cardY = viewY + 12 + cardOffset - scrollY;
        cardOffset += cardH + 16;

        // Culling: Skip cards entirely outside the viewport
        if (cardY + cardH < viewY - 8 || cardY > viewY + viewH + 8) {
            continue;
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        // Card body background
        SDL_SetRenderDrawColor(renderer, 18, 24, 38, 235);
        SDL_Rect cardR{ panelX + 18, cardY, panelW - 36, cardH };
        SDL_RenderFillRect(renderer, &cardR);

        // Header strip
        SDL_SetRenderDrawColor(renderer, card.accent.r / 5, card.accent.g / 5, card.accent.b / 5, 230);
        SDL_Rect headBar{ cardR.x, cardY, cardR.w, cardHeadH };
        SDL_RenderFillRect(renderer, &headBar);

        // Header accent bottom line
        SDL_SetRenderDrawColor(renderer, card.accent.r, card.accent.g, card.accent.b, 200);
        SDL_RenderDrawLine(renderer, cardR.x, cardY + cardHeadH, cardR.x + cardR.w, cardY + cardHeadH);

        // Card border
        SDL_SetRenderDrawColor(renderer, card.accent.r / 3, card.accent.g / 3, card.accent.b / 3, 140);
        SDL_RenderDrawRect(renderer, &cardR);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // Icon & title
        char titleBuf[64];
        std::snprintf(titleBuf, sizeof(titleBuf), "%s  %s", card.icon, card.title);
        drawText("body", titleBuf, cardR.x + 14, cardY + cardHeadH / 2, card.accent, false);

        // Items inside card
        for (int j = 0; j < card.itemCount; j++) {
            int itemY = cardY + cardHeadH + 8 + j * itemH;
            if (itemY + itemH < viewY || itemY > viewY + viewH) continue;

            // Column 1: Keycap pill badge
            int keyPillW = 160, keyPillH = 24;
            int keyPillX = cardR.x + 14;
            int keyPillY = itemY + itemH / 2 - keyPillH / 2;

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 32, 40, 58, 230);
            SDL_Rect pillR{ keyPillX, keyPillY, keyPillW, keyPillH };
            SDL_RenderFillRect(renderer, &pillR);
            SDL_SetRenderDrawColor(renderer, 65, 85, 120, 180);
            SDL_RenderDrawRect(renderer, &pillR);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            drawText("small", card.items[j].key, keyPillX + keyPillW / 2, itemY + itemH / 2,
                     SDL_Color{ 255, 230, 125, 255 }, true);

            // Column 2: Action Title + Description
            int textColX = cardR.x + 186;
            TTF_Font* fSmall = assets->GetFont("small");
            int titleW = 0, titleH = 0;
            if (fSmall) {
                TTF_SizeText(fSmall, card.items[j].title, &titleW, &titleH);
            }
            drawText("small", card.items[j].title, textColX, itemY + itemH / 2,
                     SDL_Color{ 255, 255, 255, 255 }, false);

            drawText("small", "·", textColX + titleW + 8, itemY + itemH / 2,
                     SDL_Color{ 100, 130, 165, 200 }, false);

            drawText("small", card.items[j].desc, textColX + titleW + 20, itemY + itemH / 2,
                     SDL_Color{ 185, 198, 218, 230 }, false);
        }
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    // ---- Fade edges (smooth dissolve at top & bottom) ----
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int f = 0; f < 10; f++) {
        Uint8 fadeA = (Uint8)(240 * (1.0f - (float)f / 10.0f));
        SDL_SetRenderDrawColor(renderer, 14, 18, 30, fadeA);
        SDL_RenderDrawLine(renderer, viewX, viewY + f, viewX + viewW, viewY + f);
    }
    for (int f = 0; f < 10; f++) {
        Uint8 fadeA = (Uint8)(240 * ((float)f / 10.0f));
        SDL_SetRenderDrawColor(renderer, 14, 18, 30, fadeA);
        SDL_RenderDrawLine(renderer, viewX, viewY + viewH - 10 + f, viewX + viewW, viewY + viewH - 10 + f);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // ---- Thin Scrollbar ----
    if (maxScroll > 0) {
        const int trackX = panelX + panelW - 12;
        SDL_Rect track{ trackX, viewY + 4, 6, viewH - 8 };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 16);
        SDL_RenderFillRect(renderer, &track);

        int thumbH = std::max(36, (int)((float)viewH * (float)viewH / (float)contentH));
        int thumbY = viewY + 4 + (int)(((float)viewH - 8.f - (float)thumbH) * (m_instructionsScrollPos / (float)maxScroll));
        SDL_SetRenderDrawColor(renderer, 80, 210, 255, 220);
        SDL_Rect thumb{ trackX - 1, thumbY, 8, thumbH };
        SDL_RenderFillRect(renderer, &thumb);

        SDL_SetRenderDrawColor(renderer, 200, 245, 255, 255);
        SDL_RenderDrawLine(renderer, trackX + 1, thumbY + 2, trackX + 1, thumbY + thumbH - 3);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    // ---- Sticky Header ----
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 18, 24, 38, 252);
    SDL_Rect headR{ panelX, panelY, panelW, headH };
    SDL_RenderFillRect(renderer, &headR);
    SDL_SetRenderDrawColor(renderer, 80, 210, 255, 220);
    SDL_RenderDrawLine(renderer, panelX, panelY + headH, panelX + panelW, panelY + headH);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    drawText("title", "HOW TO PLAY", panelX + 24, panelY + headPad,
             SDL_Color{ 255, 255, 255, 255 }, false);
    drawText("small", "Field Guide & Combat Manual  ·  DungeonRush 2",
             panelX + 26, panelY + headPad + titleSkip,
             SDL_Color{ 130, 180, 220, 220 }, false);

    int kw1 = 0, kw2 = 0;
    int kx = panelX + panelW - 320;
    drawKeycap("Wheel / ↑↓", kx, panelY + headH / 2, kw1);
    drawText("small", "Scroll", kx + kw1 + 6, panelY + headH / 2, SDL_Color{ 180, 200, 225, 230 }, false);

    kx += kw1 + 56;
    drawKeycap("ESC / ENTER", kx, panelY + headH / 2, kw2);
    drawText("small", "Close", kx + kw2 + 6, panelY + headH / 2, SDL_Color{ 180, 200, 225, 230 }, false);

    // ---- Sticky Footer ----
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 16, 20, 32, 252);
    SDL_Rect footR{ panelX, panelY + panelH - footH, panelW, footH };
    SDL_RenderFillRect(renderer, &footR);
    SDL_SetRenderDrawColor(renderer, 60, 80, 110, 160);
    SDL_RenderDrawLine(renderer, panelX, footR.y, panelX + panelW, footR.y);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    drawText("small", "DungeonRush 2  ·  Use Up / Down / PageUp / PageDown / Mouse Wheel to navigate",
             panelX + 24, footR.y + footH / 2, SDL_Color{ 130, 150, 180, 220 }, false);

    char pctBuf[32];
    int pct = (maxScroll > 0) ? (int)(100.f * m_instructionsScrollPos / (float)maxScroll) : 100;
    pct = std::max(0, std::min(100, pct));
    std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);

    int kwPct = 0;
    drawKeycap(pctBuf, panelX + panelW - 70, footR.y + footH / 2, kwPct);
}

// A translucent full-screen scrim.  Every overlay draws one of these first so
// the world stays readable behind it instead of fighting the text.
void Game::drawScrim(Uint8 a) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
    SDL_Rect full{ 0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT };
    SDL_RenderFillRect(renderer, &full);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// ---------------------------------------------------------------------------
// Pause.  A real menu, not just a word: resume / restart / settings / stats /
// quit.  Esc and P both come back in here, and both get you back out.
// ---------------------------------------------------------------------------
void Game::drawPauseOverlay() {
    drawScrim(150);

    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    const int pw = 420, ph = 400;
    const int px = (W - pw) / 2, py = (H - ph) / 2;

    SDL_SetRenderDrawColor(renderer, 22, 28, 48, 248);
    SDL_Rect panel{ px, py, pw, ph };
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 120, 200, 255, 220);
    SDL_RenderDrawRect(renderer, &panel);

    // Title and hint are positioned from the fonts' line skips.  The old
    // py+46 / py+84 pair overlapped by a few pixels on the "PAUSED" descender.
    TTF_Font* fBigP   = assets->GetFont("big");
    TTF_Font* fSmallP = assets->GetFont("small");
    const int bigSkip   = fBigP   ? TTF_FontLineSkip(fBigP)   : 73;
    const int smallSkip = fSmallP ? TTF_FontLineSkip(fSmallP) : 17;
    const int titleCy = py + 20 + bigSkip / 2;
    const int subCy   = py + 20 + bigSkip + 6 + smallSkip / 2;
    drawText("big",   "PAUSED", W / 2, titleCy, SDL_Color{255,255,255,255}, true);
    drawText("small", "W / S to choose     ENTER to confirm     P or ESC to resume",
             W / 2, subCy, SDL_Color{150,160,185,255}, true);

    const char* labels[5] = { "Resume", "Restart run", "Settings",
                              "Run stats", "Quit to menu" };
    const int rowH = 52;
    const int top  = py + 130;
    for (int i = 0; i < 5; i++) {
        const int y = top + i * rowH;
        const bool sel = (pauseSelection == i);
        if (sel) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 120, 200, 255, 46);
            SDL_Rect hl{ px + 22, y - 20, pw - 44, 42 };
            SDL_RenderFillRect(renderer, &hl);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
        drawText("body", labels[i], W / 2, y,
                 sel ? SDL_Color{140,225,255,255} : SDL_Color{210,218,235,255},
                 true);
    }
}

void Game::drawGameOverOverlay() {
    drawScrim(180);
    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    drawText("big",  "GAME OVER", W / 2, H / 2 - 60,
             SDL_Color{255, 80, 80, 255}, true);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Score %d     Best %d", gameScore, bestScore);
    drawText("body", buf, W / 2, H / 2 + 6, SDL_Color{230,230,235,255}, true);
    drawText("body", "Press R to restart   |   ESC for menu",
             W / 2, H / 2 + 44, SDL_Color{200,205,220,230}, true);
}

void Game::drawVictoryOverlay() {
    drawScrim(180);
    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    drawText("big",  "VICTORY!", W / 2, H / 2 - 70,
             SDL_Color{255, 230, 90, 255}, true);
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Score %d     Time %02d:%02d     Deaths %d",
                  gameScore, (int)runTime / 60, (int)runTime % 60, deaths);
    drawText("body", buf, W / 2, H / 2 + 2, SDL_Color{235,235,240,255}, true);
    drawText("body", "Press R to play again   |   ESC for menu",
             W / 2, H / 2 + 42, SDL_Color{200,205,220,230}, true);
}

void Game::render() {
    // ------------------------------------------------------------------
    // ONE present, at the very end, for every state.
    //
    // This function used to `return` straight after drawMenu() /
    // drawInstructions() / drawSettings() / drawStats().  Those returns
    // skipped SDL_RenderPresent, so the whole frame was composed into the
    // backbuffer and then discarded without ever reaching the window — the
    // title screen and every overlay came up completely blank while the game
    // itself (the branch that did reach the present) looked fine.
    //
    // There is exactly one SDL_RenderPresent call now, and no early return.
    // ------------------------------------------------------------------
    if (state == State::Menu) {
        drawMenu();
    } else if (state == State::Instructions) {
        // Instructions is reachable from the menu AND mid-run (H).  Draw the
        // world behind it in the mid-run case so it reads as an overlay
        // rather than a hard cut to an empty screen.
        if (m_returnState == State::Playing || m_returnState == State::Paused) {
            drawWorld();
            if (m_returnState == State::Playing) drawHUD();
            drawScrim(120);
            drawInstructionsPanel(true);
        } else {
            drawMenuBackdrop();
            drawInstructionsPanel(false);
        }
    } else {
        drawWorld();
        if (state == State::Playing || state == State::Paused) drawHUD();

        if      (state == State::Paused)        drawPauseOverlay();
        else if (state == State::Settings)      drawSettings();
        else if (state == State::Stats)         drawStats();
        else if (state == State::GameOver)      drawGameOverOverlay();
        else if (state == State::Victory)       drawVictoryOverlay();
        else if (state == State::LevelComplete) drawVictoryOverlay();
    }
    SDL_RenderPresent(renderer);
}

// ===========================================================================
// clean
// ===========================================================================
void Game::clean() {
    if (m_music) { Mix_HaltMusic(); Mix_FreeMusic(m_music); m_music = nullptr; }
    Parallax::shutdown();
    Atmosphere::shutdown();
    Scenery::shutdown();
    delete assets; assets = nullptr;
    if (m_window) SDL_DestroyWindow(m_window);
    if (renderer)  SDL_DestroyRenderer(renderer);
    Mix_CloseAudio();
    Mix_Quit();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}
