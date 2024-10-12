#pragma once
#include <string>
#include <SDL.h>
#include "Components.h"
#include "TextureManager.h"

class ColliderComponent : public Component {
public:
    SDL_Rect collider = {0, 0, 0, 0};
    std::string tag;

    SDL_Texture *tex;
    SDL_Rect srcR = {0, 0, 32, 32}, destR = {0, 0, 32, 32};

    int colliderWidth = 0, colliderHeight = 0;

    TransformComponent *transform;

    ColliderComponent(std::string t) {
        tag = t;
    }

    ColliderComponent(std::string t, int xpos, int ypos, int size) {
        tag = t;
        collider.x = xpos;
        collider.y = ypos;
        collider.h = collider.w = size;
    }

    ColliderComponent(std::string t, int colWidth, int colHeight) {
        tag = t;
        colliderWidth = colWidth;
        colliderHeight = colHeight;
    }

    ColliderComponent(std::string t, int xpos, int ypos, int colWidth, int colHeight) {
        tag = t;
        collider.x = xpos;
        collider.y = ypos;
        collider.w = colWidth;
        collider.h = colHeight;
    }

    void init() override {
        if (!entity->hasComponent<TransformComponent>()) {
            entity->addComponent<TransformComponent>();
        }

        transform = &entity->getComponent<TransformComponent>();

        if (colliderWidth == 0 && colliderHeight == 0) {
            colliderWidth = transform->width;
            colliderHeight = transform->height;
        }

        if (entity->tag != "tileCollider") {
            collider.w = colliderWidth * transform->scale;
            collider.h = colliderHeight * transform->scale;
        }

        if (tag != "terrain") {
            collider.x = static_cast<int>(transform->position.x);
            collider.x += (transform->width - colliderWidth) * transform->scale / 2.0;
            collider.y = static_cast<int>(transform->position.y);
            collider.y += (transform->height - colliderHeight) * transform->scale / 2.0;
            collider.w =colliderWidth * transform->scale;
            collider.h = colliderHeight * transform->scale;
            destR.w = colliderWidth * transform->scale;
            destR.h = colliderHeight * transform->scale;
        }

        tex = TextureManager::LoadTexture("../assets/sprites/ColTex.png");
        srcR = {0, 0, 32, 32};
        destR = {collider.x, collider.y, collider.w, collider.h};

    }

    void update() override {
        if (tag != "terrain") {
            collider.x = static_cast<int>(transform->position.x);
            collider.x += (transform->width - colliderWidth) * transform->scale / 2.0;
            collider.y = static_cast<int>(transform->position.y);
            collider.y += (transform->height - colliderHeight) * transform->scale / 2.0;
            collider.w =colliderWidth * transform->scale;
            collider.h = colliderHeight * transform->scale;
            destR.w = colliderWidth * transform->scale;
            destR.h = colliderHeight * transform->scale;
        }

        destR.x = collider.x - Game::camera.x;
        destR.y = collider.y - Game::camera.y;
    }

    void draw() override {
//        TextureManager::Draw(tex, srcR, destR, SDL_FLIP_NONE);
    }

private:

};