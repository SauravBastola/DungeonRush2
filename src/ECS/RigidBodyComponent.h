#pragma once

#include "Components.h"
#include "Vector2D.h"

class RigidBodyComponent : public Component {
private:
    TransformComponent *transform;

public:

    RigidBodyComponent() = default;

    void init() override {

        transform = &entity->getComponent<TransformComponent>();

    }

    void update() override {
        if (transform->velocity.y < 3000) {
                transform->velocity.y += 68;
        }
    }
};