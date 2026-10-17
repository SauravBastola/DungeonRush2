#pragma once
#include "ECS.h"
#include "Components.h"
#include "TransformComponent.h"
#include <unordered_set>
#include <cstdint>

extern float lastFrameTime;

// A transient attack volume spawned by the CombatSystem.  It owns
// nothing visual — render from a hitbox visual entity or skip.  Damage
// is applied once per victim (tracked in `hit`).
class HitboxComponent : public Component {
public:
    Entity*       owner   = nullptr;     // who spawned it
    float         damage  = 0.f;
    float         knockX  = 0.f;
    float         knockY  = 0.f;
    int           ttl     = 0;           // frames remaining
    int           ttlMax  = 0;           // for visuals / lifetime
    SDL_Rect      box{0,0,0,0};          // world-space AABB
    int           xOffset = 0, yOffset = 0;  // relative to owner
    int           width   = 0, height  = 0;
    int           dir     = 1;           // facing (+1 right, -1 left)
    bool          centred = false;       // hang over/under the owner, not in front
    std::string   tag;                   // "player" or "enemy" (for friendly-fire filter)
    std::unordered_set<std::uintptr_t> hit;  // already-damage entity ids

    // --- feedback applied when this box connects --------------------------
    // `bounce` is handed to the owner (the dive-slash hop), `stunT` staggers
    // the victim so knockback is actually visible instead of being overwritten
    // by the AI on the very next frame.
    float bounce = 0.f;
    float stunT  = 0.f;
    int   kind   = 0;                    // Config::AttackKind, for VFX/score
    int   hitCount = 0;                  // victims struck so far

    void init() override {}
    void update() override {
        if (!owner || !owner->isActive()) { entity->destroy(); return; }
        auto& tr = owner->getComponent<TransformComponent>();
        int cx;
        if (centred)
            cx = (int)tr.position.x + (int)tr.width / 2 - width / 2 + xOffset * dir;
        else
            cx = (int)tr.position.x + (dir > 0 ? (int)tr.width + xOffset
                                               : xOffset - width);
        int cy = (int)tr.position.y + yOffset;
        box = { cx, cy, width, height };
        if (--ttl <= 0) entity->destroy();
    }
};
