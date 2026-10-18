#pragma once
// All gameplay systems live as free functions in namespaces.  This keeps
// them composable and easy to test in isolation.
#include "ECS.h"
#include "Components.h"
#include "Config.h"
#include "Game.h"
#include <vector>
#include <SDL.h>

extern float lastFrameTime;
class Manager;

// ---------------------------------------------------------------------------
// Physics: gravity, integration, axis-separated AABB resolution, platforms.
// ---------------------------------------------------------------------------
namespace Physics {

struct MoveResult {
    bool grounded = false;
    bool hitCeil   = false;
    bool hitLeft   = false;
    bool hitRight  = false;
};

MoveResult moveAndCollide(Entity& e, const std::vector<SDL_Rect>& solids);
MoveResult moveAndCollideOneWay(Entity& e, const std::vector<SDL_Rect>& oneWays);
void stepPlatforms(std::vector<Entity*>& platforms);
void applyPlatformCarry(Entity& player, const std::vector<Entity*>& platforms);

} // namespace Physics

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------
namespace Particles {

void emitDust(float x, float y, Uint8 r, Uint8 g, Uint8 b);
void emitHitSparks(float x, float y);
void emitCoinSparkle(float x, float y);       // Batch G2
void emitDeathBurst(float x, float y, Uint8 r, Uint8 g, Uint8 b);
void emitHeal(float x, float y);
void step(std::vector<Entity*>& particles);
void draw(std::vector<Entity*>& particles);

} // namespace Particles

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------
namespace Combat {

void spawnMeleeHitbox(Entity& owner, const Config::AttackDef& def, int dir,
                      float damage, int kind);
void spawnPlayerBolt(Entity& owner, int dir, bool heavy);
Entity& spawnProjectile(Entity& owner, Vector2D vel, float damage, const std::string& tag,
                     const std::string& tex = "projectile", float life = 1.2f);

// Ground-hugging shockwave thrown out by the boss's jump slam.  Travels
// horizontally at ground level; the player must jump over it.
void spawnShockwave(Entity& owner, int dir, float damage);
// Batch O: a boss attack piece (boss_fx.png cell 0 rock, 1 spore, 2 lava)
Entity& spawnBossFx(Entity& owner, float x, float y, Vector2D vel, float damage, int cell, bool gravity, float life);

// Three bolts fanned out over `spread` radians — the boss's phase-2 ranged
// attack.  Fires them all centred on the direction to `target`.
void spawnProjectileFan(Entity& owner, Entity& target, int count,
                        float spread, float speed, float damage);

// Returns the number of victims struck this frame.  Hits are reported through
// `events` (see Config::HitEvent) so the HUD can show the real damage roll.
int resolveMelee(std::vector<Entity*>& hitboxes,
                 std::vector<Entity*>& targets,
                 float& screenShake,
                 std::vector<Config::HitEvent>* events = nullptr);

} // namespace Combat

// ---------------------------------------------------------------------------
// Enemy AI
// ---------------------------------------------------------------------------
namespace EnemyAI {

// Everything the AI needs to know about the player's defensive state so that
// contact damage, bomber blasts and arrows all honour shields / invincibility
// / i-frames through the same funnel as everything else.
struct DamageSink {
    int&   shieldCharges;
    float& iframeT;
    float  invincT;
    // Batch C: a parry window open on the hero, and how many strikes it beat
    float  parryT;
    int&   parryEvents;
};

// Batch C: does this enemy block a hit arriving from world x `fromX`?
// (a guarding skeleton blocks whatever comes from the side it faces)
bool blocks(Entity& enemy, float fromX);

void step(std::vector<Entity*>& enemies, Entity& player,
          const DamageSink& sink, float& screenShake);

} // namespace EnemyAI

// ---------------------------------------------------------------------------
// Pickups (coins, powerups, facilities, hazards)
// ---------------------------------------------------------------------------
namespace Pickup {

struct PlayerBag {
    float& hp;
    float  maxHp;
    float& speedT;
    float& invincT;
    float& dmgT;
    float& iframeT;
    int&   score;
    int&   lives;
    int&   keys;
    int&   jumpsLeft;
    int    maxJumps;
    bool&  reachedCheckpoint;
    bool&  doorUnlocked;
    float& shake;
    // --- added with the magnet / shield / coin-doubler powerups ---
    float& magnetT;
    int&   shieldCharges;   // absorbs this many hits before HP is touched
    float& coinMultT;       // >0 => coins are worth POWERUP_COIN_MULT
    int&   coinsBanked;     // running total for the stats screen
    // --- Batch B: checkpoints move the respawn point forward ---
    float& respawnX;
    float& respawnY;
};

void step(Entity& player,
          std::vector<Entity*>& coins,
          std::vector<Entity*>& powerups,
          std::vector<Entity*>& facilities,
          std::vector<Entity*>& hazards,
          PlayerBag bag);

} // namespace Pickup

// ---------------------------------------------------------------------------
// Damage — the single funnel every source of harm goes through, so shields,
// invincibility and i-frames only have to be honoured in one place.
// ---------------------------------------------------------------------------
namespace Damage {

// Returns true if the hit actually landed (i.e. was not absorbed/ignored).
bool applyToPlayer(Entity& player, float amount, int& shieldCharges,
                   float invincT, float& iframeT, float& shake);

} // namespace Damage

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
namespace CameraSystem {

void follow(Entity& player, SDL_Rect& camera, float& shake, float& shakeMag);

} // namespace CameraSystem

// Batch E: every sound goes through here, so each play is counted - the
// harness checks that events really trigger their sound, not just that the
// files load.
namespace Sfx {
void play(const char* id);
int  count(const std::string& id);
void resetCounts();
}

