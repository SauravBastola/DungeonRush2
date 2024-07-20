#pragma once

#include <algorithm>
#include "Components.h"
#include "Vector2D.h"

extern float lastFrameTime;
extern float gameTime;

class TransformComponent : public Component {
public:
    Vector2D position;
    Vector2D velocity;
    Vector2D acceleration;

    int height = 32;
    int width = 32;
    float scale = 1;

    float health = 100;

    int speed = 3;

    bool blocked = false;

    TransformComponent() {
        position.Zero();
    }

    TransformComponent(int sc) {
        position.Zero();
        scale = sc;
    }

    TransformComponent(float x, float y) {
        position.Zero();
        acceleration.Zero();
    }

    TransformComponent(float x, float y, int w, int h, float sc) {
        position.x = x;
        position.y = y;
        height = h;
        width = w;
        scale = sc;
    }

    void init() override {
        velocity.Zero();
    }

    void update() override {
        position.x += static_cast<int>(velocity.x * lastFrameTime);
        position.y += static_cast<int>(velocity.y * lastFrameTime);
        if(position.x < 0) {
            position.x = 0;
        } else if(position.x > 14225) {
            position.x = 14225;
        } else if(position.y < 0) {
            position.y = 0;
        }

    }

    void updatePosition() {
        position.x += static_cast<int>(velocity.x * lastFrameTime);
        position.y += static_cast<int>(velocity.y * lastFrameTime);
    }

};