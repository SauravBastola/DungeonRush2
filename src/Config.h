#pragma once
// DungeonRush2 — unified configuration.
//
// RULE: every entity size is given in TILE units.  1 tile = 64 px.
// Player collider == player visual size.  No per-entity magic scale numbers
// — every sprite is drawn at the size of its collider so what you see is
// what collides.  This is the change that fixes the old "inconsistent
// player/enemy size" complaint.
#include "Level.h"
#include <cstdint>

namespace Config {
    // ---- window ----
    constexpr int WINDOW_WIDTH   = 1350;
    constexpr int WINDOW_HEIGHT  = 850;
    constexpr int FPS            = 60;
    constexpr int FRAME_DELAY_MS = 1000 / FPS;

    // ---- world (delegated to generated level) ----
    constexpr int MAP_WIDTH_TILES  = Level::WIDTH_TILES;
    constexpr int MAP_HEIGHT_TILES = Level::HEIGHT_TILES;
    constexpr int TILE_SIZE        = Level::TILE;
    // Source and world tile sizes now agree: platformer.png is a grid of
    // 64px cells and TILE_SIZE is 64, so Map draws terrain 1:1 rather than
    // upscaling a 32px atlas.  Kept as a separate constant because props and
    // colliders still express sizes in scaled-tile units.
    constexpr int SCALED_TILE_SIZE = TILE_SIZE;
    constexpr int MAP_PIXEL_WIDTH  = Level::PIXEL_WIDTH;
    constexpr int MAP_PIXEL_HEIGHT = Level::PIXEL_HEIGHT;

    // ---- player (sizes in TILES — applied through the same path) ----
    constexpr float PLAYER_W_TILES = 0.5f;          // 32 px
    constexpr float PLAYER_H_TILES = 0.78f;         // 50 px
    constexpr int   PLAYER_W       = 32;            // collider == visual
    constexpr int   PLAYER_H       = 50;
    // Level::PLAYER_TILE is the tile the hero OCCUPIES, so the feet belong on
    // the bottom edge of that tile - same rule as every other spawned entity.
    constexpr float PLAYER_X       = Level::PLAYER_X;
    constexpr float PLAYER_Y       = Level::PLAYER_Y + Level::TILE - PLAYER_H;

    // ---- hero physics (Celeste-flavoured) ----
    //
    // Jump reach is not a matter of taste — it decides what the level is
    // allowed to ask of the player.  With a 64 px tile:
    //
    //     height = v^2 / 2g         = 900^2 / 4000     = 202 px = 3.16 tiles
    //     airtime = 2v / g          = 2*900 / 2000     =   0.90 s
    //     length  = speed * airtime = 280 * 0.90       = 252 px = 3.94 tiles
    //
    // The old value (-620) gave only 96 px = 1.5 tiles, which is why every
    // 2-tile step in the level was an invisible wall and no floating platform
    // could be reached.  tools/build_level.py derives MAX_STEP_UP and MAX_GAP
    // from these exact numbers, so keeping them in sync is what keeps the
    // level playable.
    constexpr float PLAYER_SPEED       = 280.f;     // 4.4 tiles / s
    constexpr float PLAYER_ACCEL       = 2400.f;
    constexpr float PLAYER_FRICTION    = 2000.f;
    constexpr float PLAYER_JUMP_VEL    = -900.f;    // ~ 3.2 tiles high
    constexpr float PLAYER_GRAVITY     = 2000.f;    // px / s^2
    constexpr float PLAYER_TERMINAL    = 1400.f;
    constexpr float COYOTE_TIME        = 0.10f;
    constexpr float JUMP_BUFFER        = 0.12f;
    constexpr int   MAX_JUMPS          = 2;
    constexpr float WALL_SLIDE_SPEED   = 90.f;
    constexpr float WALL_JUMP_VY       = -820.f;    // slightly short of a full jump
    constexpr float WALL_JUMP_VX       = 360.f;
    // Batch C: horizontal input is ignored this long after a wall-jump, so
    // holding toward the wall cannot pull the hero straight back onto it.
    constexpr float WALL_JUMP_LOCK     = 0.16f;
    // Batch C: slide (S / Down on the ground while moving)
    constexpr float SLIDE_SPEED        = 560.f;
    constexpr float SLIDE_TIME         = 0.32f;
    constexpr float SLIDE_COOLDOWN     = 0.45f;
    constexpr int   SLIDE_H            = 26;       // hitbox height while sliding
    // Batch C: parry (U / C).  A short window that negates hits, reflects
    // enemy shots back at their shooter and staggers whoever is in front.
    constexpr float PARRY_WINDOW       = 0.20f;
    constexpr float PARRY_RECOVER      = 0.40f;    // added on a whiff
    constexpr float PARRY_REACH        = 24.f;     // px in front of the hitbox
    constexpr float PARRY_STUN         = 0.90f;    // boss gets a third
    constexpr float PARRY_SHOT_SPEED   = 1.3f;     // reflected shot speed x
    constexpr float PARRY_SHOT_DAMAGE  = 1.5f;     // reflected shot damage x
    // Batch C: grappling hook (E / V) on the generator's hook points
    constexpr float GRAPPLE_RANGE      = 300.f;    // = GRAPPLE_RANGE in build_level.py
    constexpr float GRAPPLE_MIN_LEN    = 110.f;    // the rope reels in to this
    constexpr float GRAPPLE_REEL       = 120.f;    // px/s
    constexpr float GRAPPLE_RELEASE_VY = -560.f;   // the fling when you jump off

    // ---- dash ----
    constexpr float DASH_SPEED         = 720.f;
    constexpr float DASH_DURATION      = 0.18f;
    constexpr float DASH_COOLDOWN      = 0.7f;
    constexpr bool  DASH_INVINCIBLE    = true;

    // ---- attacks -----------------------------------------------------------
    //
    // The hero has six distinct melee/ranged moves instead of the old single
    // "J does everything" input.  Each one is a row in ATTACKS below, so
    // tuning a move means editing exactly one line and nothing else.
    enum class AttackKind : int {
        Slash1 = 0,     // ground/air combo, step 1
        Slash2,         // combo, step 2
        Slash3,         // combo finisher — the big one
        Uppercut,       // melee while holding UP  (anti-air)
        DiveSlash,      // airborne melee while holding DOWN (stomp, bounces)
        DashSlash,      // melee during a dash (lunge)
        Bolt,           // tap the ranged key
        HeavyBolt,      // hold the ranged key — pierces
        Count
    };
    constexpr int ATTACK_COUNT = (int)AttackKind::Count;

    struct AttackDef {
        const char* anim;    // animation registered on the player sprite
        float dmg;           // base damage
        float duration;      // seconds the hero is locked into the move
        float cooldown;      // extra seconds after `duration` before the next move
        int   boxW, boxH;    // hitbox size in px (0x0 => this is a projectile)
        int   offX, offY;    // hitbox offset from the owner's top-left
        bool  centred;       // box hangs under/over the hero instead of in front
        float knockX;        // knockback applied to the victim along facing
        float knockY;        // knockback on Y (negative = upward)
        float lunge;         // forward impulse given to the hero
        float bounce;        // hero's vy after landing this (dive-slash hop)
        float stun;          // seconds the victim is staggered
        float mana;          // mana cost (0 for melee)
        float shake;
    };

    // The sheet carries three genuinely different sword animations and every
    // one of them is used:
    //
    //   row 2 "attackPower"  - windup then a huge sweeping arc.  Slow, wide,
    //                          hits hard.  This is the combo finisher and the
    //                          rising uppercut.
    //   row 3 "attackSlash"  - horizontal sweep, blade line straight across.
    //                          Fast and broad; opens the combo and rides the
    //                          dash.
    //   row 4 "attackThrust" - crouch and lunge with the blade extended.
    //                          Narrow but long; the combo's second step and
    //                          the dive attack.
    //
    // Row 6 "cast" is the ranged row (see SpriteSpec.h for the sheet layout).
    constexpr AttackDef ATTACKS[ATTACK_COUNT] = {
        /* Slash1    */ { "attackSlash", 9.f, 0.20f, 0.05f, 52, 40,  0,  4, false,
                           260.f,  -60.f,  70.f,    0.f, 0.10f,  0.f, 2.f },
        /* Slash2    */ { "attackThrust",12.f, 0.20f, 0.05f, 66, 34,  0, 10, false,
                           300.f,  -70.f, 210.f,    0.f, 0.12f,  0.f, 3.f },
        /* Slash3    */ { "attackPower", 24.f, 0.36f, 0.20f, 84, 58,  0, -6, false,
                           520.f, -200.f, 150.f,    0.f, 0.30f,  0.f, 9.f },
        /* Uppercut  */ { "attackPower", 14.f, 0.30f, 0.14f, 50, 62,  0,-34, true,
                           150.f, -520.f,  40.f,    0.f, 0.22f,  0.f, 4.f },
        /* DiveSlash */ { "attackThrust",18.f, 0.30f, 0.14f, 54, 60,  0, 18, true,
                           110.f,  300.f,  60.f, -430.f, 0.20f,  0.f, 6.f },
        /* DashSlash */ { "attackSlash", 16.f, 0.18f, 0.08f, 80, 34,  0,  8, false,
                           620.f, -110.f, 210.f,    0.f, 0.14f,  0.f, 5.f },
        /* Bolt      */ { "cast",    14.f, 0.24f, 0.10f,   0,  0,  0,  0, false,
                             0.f,    0.f,   0.f,    0.f, 0.00f, 12.f, 1.f },
        /* HeavyBolt */ { "cast",    34.f, 0.32f, 0.16f,   0,  0,  0,  0, false,
                             0.f,    0.f,   0.f,    0.f, 0.00f, 30.f, 3.f },
    };

    constexpr float ATTACK_DURATION    = 0.28f;   // legacy default
    constexpr float ATTACK_COOLDOWN    = 0.30f;
    constexpr float COMBO_WINDOW       = 0.42f;   // chain a combo step within
    constexpr int   MAX_COMBO          = 3;
    constexpr float ATTACK_HITBOX_W    = 52;
    constexpr float ATTACK_HITBOX_H    = 40;
    constexpr float ATTACK_KNOCKBACK   = 300.f;

    // ---- ranged / mana ------------------------------------------------------
    constexpr float MANA_MAX           = 100.f;
    constexpr float MANA_REGEN         = 18.f;    // per second
    constexpr float MANA_REGEN_DELAY   = 0.55f;   // pause after spending
    constexpr float CHARGE_TIME        = 0.40f;   // hold to upgrade to HeavyBolt
    constexpr int   BOLT_SPEED         = 780;
    constexpr int   HEAVY_SPEED        = 980;
    constexpr float BOLT_LIFE          = 1.10f;
    constexpr float HEAVY_LIFE         = 1.50f;
    constexpr float BOLT_RADIUS_SCALE  = 1.0f;    // projectile draw scale
    constexpr float HEAVY_RADIUS_SCALE = 1.7f;

    constexpr int   PROJECTILE_SPEED   = 720;
    constexpr int   PROJECTILE_LIFE    = 0.8f * FPS;
    constexpr int   PROJECTILE_DAMAGE  = 12;
    constexpr int   MAX_PROJECTILES    = 6;

    // ---- combat feel --------------------------------------------------------
    // One victim struck this frame.  Lives here rather than in Systems.h
    // because Game.h needs it and Systems.h already includes Game.h — putting
    // it in Systems.h would make the two headers circularly dependent.
    struct HitEvent {
        float x, y;
        int   damage;
        bool  killed;
    };

    constexpr float HITSTOP_LIGHT      = 0.035f;  // freeze frames on a landed hit
    constexpr float HITSTOP_HEAVY      = 0.075f;
    constexpr float HITSTOP_KILL       = 0.110f;
    constexpr float HITCOMBO_TIMEOUT   = 2.60f;   // seconds before the chain drops
    constexpr int   HITCOMBO_MAX_MULT  = 20;      // hits that count toward the x
    constexpr float HITCOMBO_STEP      = 0.10f;   // +0.10x per hit, so max is 3x

    // ---- air dash -----------------------------------------------------------
    constexpr bool  AIR_DASH_ENABLED   = true;
    constexpr float AIR_DASH_SPEED     = 640.f;

    // ---- enemies (sizes in TILES) ----
    constexpr float ENEMY_W_SKELETON   = 0.55f;     // 35 px
    constexpr float ENEMY_H_SKELETON   = 0.72f;     // 46 px
    constexpr float ENEMY_W_SLIME      = 0.50f;     // 32 px
    constexpr float ENEMY_H_SLIME      = 0.42f;     // 27 px
    constexpr float ENEMY_W_BAT        = 0.50f;     // 32 px
    constexpr float ENEMY_H_BAT        = 0.40f;     // 26 px
    constexpr float ENEMY_W_MAGE       = 0.55f;     // 35 px
    constexpr float ENEMY_H_MAGE       = 0.72f;     // 46 px
    constexpr float ENEMY_W_ARCHER     = 0.53f;     // 34 px
    constexpr float ENEMY_H_ARCHER     = 0.81f;     // 52 px
    constexpr float ENEMY_W_BOMBER     = 0.41f;     // 26 px
    constexpr float ENEMY_H_BOMBER     = 0.47f;     // 30 px
    constexpr float BOSS_W             = 1.00f;     // 64 px
    constexpr float BOSS_H             = 1.16f;     // 74 px

    constexpr float ENEMY_PATROL_R     = 90.f;
    constexpr float ENEMY_CHASE_RANGE  = 320.f;
    constexpr float ENEMY_CHASE_SPEED  = 130.f;
    constexpr float ENEMY_DAMAGE       = 10.f;
    constexpr float ENEMY_HP_SKELETON  = 26.f;
    constexpr float ENEMY_HP_SLIME     = 18.f;
    constexpr float ENEMY_HP_BAT       = 14.f;
    constexpr float ENEMY_HP_MAGE      = 22.f;
    constexpr float ENEMY_HP_ARCHER    = 20.f;
    constexpr float ENEMY_HP_BOMBER    = 12.f;
    constexpr float SLIME_JUMP_VEL     = -380.f;
    constexpr float SLIME_JUMP_PERIOD  = 1.6f;
    constexpr float BAT_SIN_AMP        = 30.f;
    constexpr float BAT_SIN_FREQ       = 2.5f;
    constexpr float MAGE_FIRE_RANGE    = 360.f;
    constexpr float MAGE_FIRE_COOLDOWN = 1.4f;
    constexpr float MAGE_PROJECTILE_SPEED = 320.f;

    // archer — long range sniper that backs away when you close in
    constexpr float ARCHER_RANGE        = 430.f;
    constexpr float ARCHER_MIN_RANGE    = 150.f;    // retreat inside this
    constexpr float ARCHER_FIRE_COOLDOWN= 1.7f;
    constexpr float ARCHER_ARROW_SPEED  = 430.f;    // faster than a mage bolt
    constexpr float ARCHER_RETREAT_SPEED= 95.f;
    constexpr float ARCHER_AIM_TIME     = 0.45f;    // telegraph before loosing

    // bomber — kamikaze; lights its fuse when you get close, then detonates
    constexpr float BOMBER_SPEED         = 170.f;
    constexpr float BOMBER_TRIGGER_RANGE = 200.f;
    constexpr float BOMBER_FUSE_TIME     = 0.85f;
    constexpr float BOMBER_BLAST_RADIUS  = 96.f;
    constexpr float BOMBER_DAMAGE        = 22.f;

    constexpr float BOSS_HP            = 260.f;
    constexpr float BOSS_SPEED         = 110.f;
    constexpr float BOSS_DAMAGE        = 16.f;
    // Rule D6: the boss health bar appears only when the fight is engaged.
    // Matches the `adx < 600.f` chase threshold in the boss case of
    // EnemyAI::step, so the bar and the aggro arrive together.
    constexpr float BOSS_BAR_RANGE     = 600.f;
    constexpr float BOSS_CHARGE_TIME   = 3.0f;
    constexpr float BOSS_ENRAGE_RATIO  = 0.5f;     // below 50% HP enrages
    constexpr float BOSS_CHARGE_SPEED  = 320.f;
    constexpr float BOSS_PHASE2_INTERVAL = 0.9f;

    // boss — jump slam: leaps, lands, throws two shockwaves along the floor
    constexpr float BOSS_SLAM_INTERVAL   = 5.0f;
    constexpr float BOSS_SLAM_CHARGE     = 0.7f;    // crouch / telegraph
    constexpr float BOSS_SLAM_RISE       = -900.f;
    constexpr float BOSS_SHOCKWAVE_SPEED = 520.f;
    constexpr float BOSS_SHOCKWAVE_DAMAGE= 18.f;
    constexpr float BOSS_SHOCKWAVE_LIFE  = 1.8f;
    // boss — projectile fan (phase 2 only): N bolts on a spread
    constexpr int   BOSS_FAN_COUNT       = 3;
    constexpr float BOSS_FAN_SPREAD      = 0.30f;
    constexpr float BOSS_FAN_INTERVAL    = 2.6f;

    // ---- hazards ----
    constexpr float HAZARD_DMG_SPIKE   = 14.f;

    // ---- player resources ----
    constexpr float MAX_HP             = 100.f;
    constexpr int   MAX_LIVES          = 3;
    constexpr int   COIN_SCORE         = 5;
    constexpr int   POWERUP_SCORE      = 50;
    constexpr int   ENEMY_SCORE        = 100;
    constexpr int   BOSS_SCORE         = 1500;
    constexpr int   KEY_SCORE          = 250;
    constexpr float IFRAME_TIME        = 0.8f;     // after taking damage
    constexpr float RESPAWN_INVULN     = 1.2f;

    // ---- powerups ----
    constexpr float POWERUP_DURATION[] = { 0.f, 6.f, 4.f, 0.f, 0.f, 8.f, 12.f, 10.f };
        // Health, Speed, Invincible, DoubleJump, Damage, Magnet, Shield, CoinX2
    constexpr float POWERUP_SPEED_MULT = 1.6f;
    constexpr float POWERUP_DMG_MULT   = 1.5f;
    constexpr float POWERUP_MAGNET_RADIUS = 270.f;   // coin pull range
    constexpr float POWERUP_MAGNET_PULL   = 620.f;   // px/s^2 toward the hero
    constexpr int   POWERUP_COIN_MULT     = 2;

    // ---- particles ----
    constexpr int   MAX_PARTICLES      = 256;

    // ---- camera ----
    constexpr float CAM_LERP           = 0.12f;
    constexpr float CAM_LOOKAHEAD      = 120.f;     // ahead in facing direction
    constexpr float CAM_SHAKE_DECAY    = 6.5f;      // per second
    constexpr float CAM_DEAD_ZONE_X    = 80.f;      // don't track small movement
    constexpr float CAM_DEAD_ZONE_Y    = 40.f;
    constexpr float CAM_ZOOM_DASH      = 1.04f;     // punch-in while dashing
    constexpr float CAM_ZOOM_BOSS      = 1.10f;     // pull back for the boss
    constexpr float CAM_ZOOM_LERP      = 4.0f;      // zoom approach speed /s

    // ---- audio defaults (0..128, SDL_mixer scale) ----
    constexpr int   DEFAULT_MUSIC_VOL  = 48;
    constexpr int   DEFAULT_SFX_VOL    = 72;

    // ---- persistence ----
    constexpr const char* SAVE_FILE    = "dungeonrush.save";
    constexpr const char* SAVE_MAGIC   = "DR2SAVE1";
}
