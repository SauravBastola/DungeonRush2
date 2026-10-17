#pragma once
#include "ECS.h"
#include "Config.h"
#include "Vector2D.h"
#include "Components.h"
#include <algorithm>

extern float lastFrameTime;

// World-space transform.  width/height are the collider dimensions
// (== the visual size of the entity in pixels).  scale is reserved for
// texture art that needs to be larger than its collider (used sparingly).
class TransformComponent : public Component {
public:
    Vector2D position;
    Vector2D velocity;
    Vector2D acceleration;

    int   width  = 32;
    int   height = 32;
    float scale  = 1.f;

    bool dead        = false;
    bool facingRight = true;

    // Set by Physics::moveAndCollide / moveAndCollideOneWay on the first call
    // for an entity.  Those functions integrate velocity themselves, so if
    // update() also integrated it the entity would move twice per frame and —
    // far worse — would begin each physics step already buried inside the
    // ground, because the free integration runs before collision. The X pass
    // then resolved that vertical penetration sideways and flung the hero to
    // the map edge. Motion for these entities belongs to physics alone.
    bool physicsOwnsMotion = false;

    TransformComponent() { position.Zero(); }
    TransformComponent(int sc) { position.Zero(); scale = sc; }
    TransformComponent(float x, float y) : position(x, y) { velocity.Zero(); acceleration.Zero(); }
    TransformComponent(float x, float y, int w, int h, float sc = 1.f)
        : position(x, y), width(w), height(h), scale(sc)
    {
        velocity.Zero(); acceleration.Zero();
    }

    void init() override { velocity.Zero(); }
    void update() override {
        if (!physicsOwnsMotion) {
            position.x += velocity.x * lastFrameTime;
            position.y += velocity.y * lastFrameTime;
        }
        if (position.x < 0) position.x = 0;
        if (position.x > Config::MAP_PIXEL_WIDTH - width)
            position.x = (float)(Config::MAP_PIXEL_WIDTH - width);
        if (position.y < -200) position.y = -200;
    }
    void updatePosition() {
        position.x += velocity.x * lastFrameTime;
        position.y += velocity.y * lastFrameTime;
    }
};
