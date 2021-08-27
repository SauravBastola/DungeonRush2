#pragma once

#include "Components.h"
#include "SDL.h"
#include "TextureManager.h"
#include "Animation.h"
#include <map>
#include "AssetManager.h"

class SpriteComponent : public Component {
private:
    TransformComponent *transform;
    SDL_Texture *texture;
    SDL_Rect srcRect, destRect;

    bool animated = false;
    int frames = 0;
    int speed = 100;

public:

    bool isOverridden = false;
    int animIndex = 0;
    std::map<const std::string, Animation> animations;

    SDL_RendererFlip spriteFlip = SDL_FLIP_NONE;

    SpriteComponent() = default;

    SpriteComponent(std::string id) {
        setTex(id);
    }

    SpriteComponent(std::string id, bool isAnimated) {
        animated = isAnimated;

        Animation player_idle = Animation(0, 3, 130);
        Animation player_run = Animation(1, 6, 100);
        Animation player_jump = Animation(5, 4, 120);
        Animation player_attack1 = Animation(2, 5, 60);
        Animation player_attack2 = Animation(3, 6, 60);
        Animation player_attack3 = Animation(4, 6, 60);

        animations.emplace("playerIdle", player_idle);
        animations.emplace("playerRun", player_run);
        animations.emplace("playerJump", player_jump);
        animations.emplace("playerAttack1", player_attack1);
        animations.emplace("playerAttack2", player_attack2);
        animations.emplace("playerAttack3", player_attack3);


        Play("playerIdle");

        setTex(id);
    }

    ~SpriteComponent() {}

    void setTex(std::string id) {
        texture = Game::assets->GetTexture(id);
    }

    void init() override {

        transform = &entity->getComponent<TransformComponent>();

        srcRect.x = srcRect.y = 0;
        srcRect.w = transform->width;
        srcRect.h = transform->height;
    }

    void update() override {

        if (!isOverridden && entity->tag == "player") {
            if (transform->velocity.x != 0) {
                Play("playerRun");
            } else {
                Play("playerIdle");
            }
        }

        if (animated) {
            srcRect.x = srcRect.w * static_cast<int>((SDL_GetTicks() / speed) % frames);
        }

        srcRect.y = animIndex * transform->height;

        if(entity->tag == "healthBarStuff") {
           destRect.x = transform->position.x;
           destRect.y = transform->position.y;
        }else {
            destRect.x = static_cast<int>(transform->position.x - Game::camera.x);
            destRect.y = static_cast<int>(transform->position.y - Game::camera.y);
        }
        destRect.w = transform->width * transform->scale;
        destRect.h = transform->height * transform->scale;
    }

    void draw() override {
        if (isOverridden) {
            update();
        }
        TextureManager::Draw(texture, srcRect, destRect, spriteFlip);
    }

    void Play(const char *animName) {
        frames = animations[animName].frames;
        animIndex = animations[animName].index;
        speed = animations[animName].speed;
    }

};