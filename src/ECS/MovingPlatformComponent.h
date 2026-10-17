#pragma once
#include "ECS.h"
#include "Config.h"
#include "Vector2D.h"
#include "TransformComponent.h"
#include <cmath>

extern float lastFrameTime;

class MovingPlatformComponent : public Component {
public:
    Vector2D startPos;
    Vector2D axis{1.f, 0.f};
    float    range = 200.f;
    float    speed = 70.f;
    int      dir   = 1;

    MovingPlatformComponent() = default;
    MovingPlatformComponent(Vector2D start, Vector2D ax,
                            float r, float s)
        : startPos(start), axis(ax), range(r), speed(s) {}

    void init() override {
        startPos = entity->getComponent<TransformComponent>().position;
    }
    // Batch C: a platform moves exactly once per frame, from
    // Physics::stepPlatforms() (right before the hero is carried).  It used to
    // move three times - this update() from the ECS, the same call again from
    // stepPlatforms, and TransformComponent integrating its velocity - and the
    // two reversal checks per frame undid each other at the end of the run, so
    // platforms never turned round and drifted thousands of pixels away.
    void update() override {}
    void step() {
        auto& tr = entity->getComponent<TransformComponent>();
        float step = speed * lastFrameTime * dir;
        tr.position.x += axis.x * step;
        tr.position.y += axis.y * step;
        float dx = tr.position.x - startPos.x;
        float dy = tr.position.y - startPos.y;
        if (std::sqrt(dx*dx + dy*dy) > range) dir *= -1;
        tr.velocity.x = axis.x * speed * dir;
        tr.velocity.y = axis.y * speed * dir;
    }
};
