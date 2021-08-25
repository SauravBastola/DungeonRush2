#pragma once

#include "Components.h"
#include "SpriteComponent.h"

class ScriptComponent : public Component {
public:
    ScriptComponent() {
    }

    ~ScriptComponent() = default;

    void init() override {
        if(entity->tag == "coin") {
            entity->getComponent<SpriteComponent>().animations.emplace("coinAnim", Animation(0, 8, 100));
            entity->getComponent<SpriteComponent>().Play("coinAnim");
        }
    }

    void update() override {
    }
};