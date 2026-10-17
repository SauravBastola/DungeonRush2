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
    void update() override {
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
