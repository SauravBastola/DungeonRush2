#pragma once
#include "ECS.h"
#include "Components.h"
#include "TransformComponent.h"

extern float lastFrameTime;

// HP + invulnerability-frame timer.  Living things own one.
class HealthComponent : public Component {
public:
    float hp        = 0.f;
    float hpMax     = 0.f;
    float iframes   = 0.f;          // seconds of damage immunity
    bool  dead      = false;
    bool  boss      = false;         // show boss bar instead of small bar

    HealthComponent() = default;
    HealthComponent(float maxHp, bool isBoss = false)
        : hp(maxHp), hpMax(maxHp), boss(isBoss) {}

    // Returns "the hit landed" (true for any unblocked hit), NOT "the victim
    // died" — callers that need a kill must also test `dead`.
    bool damage(float amount) {
        if (dead || iframes > 0.f) return false;
        hp -= amount;
        iframes = 0.35f;
        if (hp <= 0) { hp = 0; dead = true; return true; }
        return true;
    }
    void heal(float amount) {
        hp = std::min(hpMax, hp + amount);
    }

    void update() override {
        if (iframes > 0) iframes -= lastFrameTime;
    }
};
