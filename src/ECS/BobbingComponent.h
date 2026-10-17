#pragma once
// ---------------------------------------------------------------------------
// BobbingComponent
//
// A slow sine hover applied to a pickable or collectible so it reads as
// "alive" without needing any extra frames of art.  The offset is applied on
// top of the entity's home position and undone again before the next frame,
// so it never drifts and never fights the collision solver.
// ---------------------------------------------------------------------------
// NOTE: deliberately does NOT include ColliderComponent.h — that header pulls
// in Components.h, so including it here would make the pair circular.  The
// collider does not need touching anyway: components update in insertion
// order and ColliderComponent is added after this one, so it re-reads the
// transform and picks the bob up in the very same frame.
#include "ECS.h"
#include "Config.h"
#include "Vector2D.h"
#include "TransformComponent.h"
#include <cmath>

extern float gameTime;

class BobbingComponent : public Component {
public:
    float amplitude = 5.f;    // pixels, peak
    float speed     = 2.4f;   // radians / second
    float phase     = 0.f;    // so a row of pickups does not bob in lockstep

    // how far we pushed the transform last frame, so we can undo it exactly
    float applied = 0.f;

    BobbingComponent() = default;
    BobbingComponent(float amp, float spd) : amplitude(amp), speed(spd) {}

    void init() override {
        // desynchronise neighbouring pickups a little using their position
        if (entity->hasComponent<TransformComponent>())
            phase = entity->getComponent<TransformComponent>().position.x * 0.017f;
    }

    void update() override {
        if (!entity->hasComponent<TransformComponent>()) return;
        auto& tr = entity->getComponent<TransformComponent>();

        // undo last frame's offset first — keeps this drift-free
        tr.position.y -= applied;

        applied = amplitude * std::sin(gameTime * speed + phase);
        tr.position.y += applied;
    }
};
