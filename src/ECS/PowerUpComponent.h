#pragma once
#include "ECS.h"
#include "Config.h"
#include <cmath>

extern float lastFrameTime;

// Health     instant heal
// Speed      temporary movement multiplier
// Invincible temporary immunity to all damage
// DoubleJump refill the air-jump budget
// Damage     temporary attack multiplier
// Magnet     coins fly toward the hero while active
// Shield     absorbs the next hit instead of HP
// CoinX2     coins are worth double while active
enum class PowerUpType {
    Health, Speed, Invincible, DoubleJump, Damage, Magnet, Shield, CoinX2
};

class PowerUpComponent : public Component {
public:
    PowerUpType type = PowerUpType::Health;
    float bobT = 0.f;
    float baseY = 0.f;

    PowerUpComponent(PowerUpType t = PowerUpType::Health) : type(t) {}

    void init() override {
        if (entity->hasComponent<TransformComponent>())
            baseY = entity->getComponent<TransformComponent>().position.y;
    }
    void update() override {
        bobT += lastFrameTime;
        if (entity->hasComponent<TransformComponent>()) {
            auto& tr = entity->getComponent<TransformComponent>();
            tr.position.y = baseY + std::sin(bobT * 3.f) * 3.f;
        }
    }
};
