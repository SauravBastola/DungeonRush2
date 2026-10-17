#pragma once
#include "ECS.h"
#include "Components.h"
#include "TransformComponent.h"
#include "Game.h"
#include <string>

class ColliderComponent : public Component {
public:
    std::string tag = "default";
    SDL_Rect collider{0,0,0,0};
    TransformComponent* transform = nullptr;
    int  w = 0, h = 0;
    bool solid = true;
    bool sensor = false;
    int  xOffset = 0, yOffset = 0;

    ColliderComponent() = default;
    ColliderComponent(std::string t) : tag(std::move(t)) {}
    ColliderComponent(std::string t, int cw, int ch) : tag(std::move(t)), w(cw), h(ch) {}
    ColliderComponent(std::string t, int x, int y, int cw, int ch)
        : tag(std::move(t)), w(cw), h(ch)
    {
        collider.x = x; collider.y = y; collider.w = cw; collider.h = ch;
    }

    void init() override {
        if (!entity->hasComponent<TransformComponent>())
            entity->addComponent<TransformComponent>();
        transform = &entity->getComponent<TransformComponent>();
        if (w == 0) w = transform->width;
        if (h == 0) h = transform->height;
    }
    void update() override {
        if (!transform) return;
        collider.w = w;
        collider.h = h;
        collider.x = (int)transform->position.x + xOffset;
        collider.y = (int)transform->position.y + yOffset;
    }
};
