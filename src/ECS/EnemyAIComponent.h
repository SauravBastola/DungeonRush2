#pragma once
#include "ECS.h"
#include "Components.h"
#include "TransformComponent.h"
#include <string>
#include <cmath>

extern float lastFrameTime;
extern float gameTime;

// All enemy brains live here.  Replaces the old `ScriptComponent`.
// `kind` chooses the behaviour; everything else is per-instance state.
class EnemyAIComponent : public Component {
public:
    enum class State { Idle, Patrol, Chase, Attack, Hurt, Dead };
    enum class Kind  { Skeleton, Slime, Bat, Mage, Archer, Bomber, Boss,
                       Spitter, Wisp };                 // Batch C4

    Kind   kind = Kind::Skeleton;
    State  state = State::Idle;

    Entity*  target = nullptr;          // usually the player
    float    patrolCenter = 0.f;
    int      patrolDir    = 1;

    // per-kind
    float    jumpTimer  = 0.f;
    float    fireTimer  = 0.f;
    float    sinT       = 0.f;
    float    attackCd   = 0.f;
    float    stateT     = 0.f;          // time spent in current state
    float    chargeT    = 0.f;
    float    fireballT  = 0.f;          // boss ranged cadence
    bool     enraged    = false;

    // archer: wind-up before loosing an arrow (gives the player a tell)
    float    aimT       = 0.f;
    bool     aiming     = false;

    // bomber: fuse countdown; >=0 means it is committed to blowing up
    float    fuseT      = -1.f;

    // Hit-stun.  While this is above zero the AI switch is skipped entirely
    // and the knockback velocity set by Combat::resolveMelee survives —
    // otherwise the brain rewrites tr.velocity on the very next frame and
    // every hit looks like it did nothing at all.
    float    hurtT      = 0.f;

    // boss: jump-slam state machine
    //   0 = idle, 1 = crouching (telegraph), 2 = airborne, 3 = recovering
    int      slamPhase  = 0;
    float    slamT      = 0.f;
    float    fanT       = 0.f;          // projectile-fan cadence (phase 2)
    // Batch O: the phase change (a roar) and each stage's signature attack
    bool     wasEnraged = false;
    float    roarT      = 0.f;
    int      specialPhase = 0;          // 0 waiting, 1 tell (markers), 2 done
    float    specialT   = 0.f;
    float    specialX[4] = { 0.f, 0.f, 0.f, 0.f };
    int      nSpecial   = 0;

    // Batch C - skeleton swing: 0 chase, 1 wind-up (tell), 2 strike, 3 guard
    int      swingPhase = 0;
    float    swingT     = 0.f;
    bool     guarding   = false;        // frontal hits are blocked
    // Batch C - bat dive: 0 roam, 1 tell (hover), 2 dive, 3 climb back
    int      divePhase  = 0;
    float    diveT      = 0.f, diveCd = 0.f;
    float    diveVX     = 0.f, diveVY = 0.f, homeY = -1.f;

    // ---- helpers ----
    void transition(State s) { state = s; stateT = 0.f; }
    bool canSeeTarget(float range) const;

    void init() override {
        if (entity->hasComponent<TransformComponent>())
            patrolCenter = entity->getComponent<TransformComponent>().position.x;
    }

    void update() override {
        stateT += lastFrameTime;
        if (attackCd > 0) attackCd -= lastFrameTime;
        if (fireTimer > 0) fireTimer -= lastFrameTime;
    }
};
