#pragma once
#include "ECS.h"
#include "Config.h"
#include "Vector2D.h"
#include "Components.h"
#include <algorithm>

extern float lastFrameTime;

class RigidBodyComponent : public Component {
public:
    TransformComponent* transform = nullptr;
    float gravity    = Config::PLAYER_GRAVITY;
    float terminal   = Config::PLAYER_TERMINAL;
    bool  useGravity = true;

    void init() override { transform = &entity->getComponent<TransformComponent>(); }
    void update() override {
        if (!useGravity || !transform) return;
        float dt = lastFrameTime;
        if (dt <= 0) dt = 1.f / Config::FPS;
        transform->velocity.y += gravity * dt;
        if (transform->velocity.y > terminal) transform->velocity.y = terminal;
    }
};
