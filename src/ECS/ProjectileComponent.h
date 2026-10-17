#pragma once
#include "ECS.h"
#include "Components.h"
#include "TransformComponent.h"
#include "Vector2D.h"
#include "Game.h"
#include <unordered_set>
#include <cstdint>

extern float lastFrameTime;

class ProjectileComponent : public Component {
public:
    Vector2D velocity;
    float life = 1.f;
    float damage = 0.f;
    std::string ownerTag;
    bool  gravity = false;
    bool  pierce  = false;   // survive the first victim (heavy bolts)
    float stunT   = 0.f;     // hit-stun handed to whatever it strikes
    Entity* owner = nullptr;
    // Victims already struck by a piercing bolt, so it cannot hit the same
    // enemy on every one of the frames it overlaps them.
    std::unordered_set<std::uintptr_t> dead;

    ProjectileComponent() = default;
    ProjectileComponent(Vector2D vel, float dmg, std::string tag,
                        Entity* own = nullptr, float lifeSecs = 1.f)
        : velocity(vel), life(lifeSecs), damage(dmg),
          ownerTag(std::move(tag)), owner(own) {}

    void init() override {
        if (entity->hasComponent<TransformComponent>())
            entity->getComponent<TransformComponent>().velocity = velocity;
    }
    void update() override {
        life -= lastFrameTime;
        auto& tr = entity->getComponent<TransformComponent>();
        if (gravity) tr.velocity.y += Config::PLAYER_GRAVITY * lastFrameTime;
        if (life <= 0
            || tr.position.x < Game::camera.x - 200
            || tr.position.x > Game::camera.x + Game::camera.w + 200
            || tr.position.y > Config::MAP_PIXEL_HEIGHT + 200) {
            entity->destroy();
        }
    }
};
