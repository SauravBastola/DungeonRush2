#pragma once
#include "ECS.h"
#include "Components.h"
#include "TransformComponent.h"
#include <cmath>

extern float lastFrameTime;
extern float gameTime;

// Lightweight VFX particle.  Updated in ParticleSystem.
class ParticleComponent : public Component {
public:
    float life   = 0.f;          // seconds remaining
    float lifeMax = 0.f;
    float vx = 0.f, vy = 0.f;
    float ax = 0.f, ay = 0.f;    // acceleration
    float drag = 0.f;
    float startSize = 4.f;
    float endSize   = 0.f;
    Uint8 r = 255, g = 255, b = 255, a = 255;

    void init() override {}
    void update() override {
        life -= lastFrameTime;
        if (life <= 0) { entity->destroy(); return; }
        vx += ax * lastFrameTime;
        vy += ay * lastFrameTime;
        if (drag > 0) {
            float f = 1.f - std::min(1.f, drag * lastFrameTime);
            vx *= f; vy *= f;
        }
        auto& tr = entity->getComponent<TransformComponent>();
        tr.position.x += vx * lastFrameTime;
        tr.position.y += vy * lastFrameTime;
    }
    float alpha() const { return a * (life / lifeMax); }
    float size()  const { return startSize + (endSize - startSize) * (1.f - life / lifeMax); }
};
