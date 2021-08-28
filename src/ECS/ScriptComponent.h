#pragma once

#include "Components.h"
#include "SpriteComponent.h"
#include "Collision.h"

class ScriptComponent : public Component {
private:
    const Entity& player;

public:
    std::string scriptTag;
    bool isHandlingAction = false;
    Uint32 actionInitTime = SDL_GetTicks();

    ScriptComponent(std::string tag, const Entity& Player) : scriptTag(tag), player(Player) {
    }

    ~ScriptComponent() = default;

    void init() override {
        if (entity->tag == "coin") {
            entity->getComponent<SpriteComponent>().animations.emplace("coinAnim", Animation(0, 8, 100));
            entity->getComponent<SpriteComponent>().Play("coinAnim");
        } else if (entity->tag == "enemySkeleton") {
            entity->getComponent<SpriteComponent>().animations.emplace("enemySkeletonIdle", Animation(0, 4, 100));
            entity->getComponent<SpriteComponent>().animations.emplace("enemySkeletonWalk", Animation(1, 4, 100));
            entity->getComponent<SpriteComponent>().animations.emplace("enemySkeletonAttack", Animation(2, 8, 100));
            entity->getComponent<SpriteComponent>().animations.emplace("enemySkeletonShield", Animation(3, 4, 100));
            entity->getComponent<SpriteComponent>().animations.emplace("enemySkeletonTakeHit", Animation(4, 4, 100));
            entity->getComponent<SpriteComponent>().animations.emplace("enemySkeletonDie", Animation(5, 4, 200));
            entity->getComponent<SpriteComponent>().Play("enemySkeletonIdle");

        }
    }

    void update() override {
        if (entity->tag == "coin") {

        } else if (entity->tag == "enemySkeleton") {

            if (!entity->getComponent<SpriteComponent>().isOverridden) {
                if (entity->getComponent<TransformComponent>().velocity.x == 0) {
                    entity->getComponent<SpriteComponent>().Play("enemySkeletonIdle");
                } else {
                    entity->getComponent<SpriteComponent>().Play("enemySkeletonWalk");
                }
            }

        }

        if (scriptTag == "enemy") {
            if (entity->getComponent<TransformComponent>().velocity.x > 0) {
                entity->getComponent<SpriteComponent>().spriteFlip = SDL_FLIP_NONE;
            } else if (entity->getComponent<TransformComponent>().velocity.x < 0) {
                entity->getComponent<SpriteComponent>().spriteFlip = SDL_FLIP_HORIZONTAL;
            }

            if (
                std::abs(entity->getComponent<TransformComponent>().position.x -
                    player.getComponent<TransformComponent>().position.x) < 500 &&
                    (entity->getComponent<TransformComponent>().position.x -
                        player.getComponent<TransformComponent>().position.x) < -159 ||
                    (entity->getComponent<TransformComponent>().position.x -
                        player.getComponent<TransformComponent>().position.x) > 39
                ) {

                if (entity->getComponent<TransformComponent>().position.x -
                    player.getComponent<TransformComponent>().position.x > 0) {
                    entity->getComponent<TransformComponent>().velocity.x = -200;
                } else {
                    entity->getComponent<TransformComponent>().velocity.x = 200;
                }

            } else {
                entity->getComponent<TransformComponent>().velocity.x = 0;
            }

            if(entity->getComponent<TransformComponent>().health <= 0) {
                entity->getComponent<SpriteComponent>().isOverridden = true;
                entity->getComponent<SpriteComponent>().Play("enemySkeletonDie");
                if(isHandlingAction == false) {
                    actionInitTime = SDL_GetTicks();
                    isHandlingAction = true;
                } else {
                   if((SDL_GetTicks() - actionInitTime) / 1000.0 > 0.8) {
                       entity->destroy();
                   }
                }
            }


        }

    }
};