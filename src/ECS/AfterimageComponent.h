#pragma once
// ---------------------------------------------------------------------------
// AfterimageComponent
//
// A short-lived ghost of whatever spawned it.  Used for the dash trail: the
// hero leaves a fading copy of itself behind, which is the cheapest way to
// make a fast movement read as fast.
//
// The ghost is a real entity with a real SpriteComponent, so it inherits the
// correct frame, scale and facing for free — no second source of truth.
// ---------------------------------------------------------------------------
#include "ECS.h"
#include "Config.h"
#include "TransformComponent.h"
#include "SpriteComponent.h"

extern float lastFrameTime;

class AfterimageComponent : public Component {
public:
    float life    = 0.25f;   // seconds remaining
    float maxLife = 0.25f;
    Uint8 r = 120, g = 190, b = 255;   // tint applied on the way out

    AfterimageComponent() = default;
    AfterimageComponent(float secs, Uint8 rr, Uint8 gg, Uint8 bb)
        : life(secs), maxLife(secs), r(rr), g(gg), b(bb) {}

    void update() override {
        life -= lastFrameTime;
        if (life <= 0.f) { entity->destroy(); return; }
        if (!entity->hasComponent<SpriteComponent>()) return;

        auto& sp = entity->getComponent<SpriteComponent>();
        float t = life / maxLife;              // 1 -> 0
        sp.alpha = 0.55f * t;
        // Tint toward the ghost colour as it fades.  This goes through the
        // sprite, never straight onto the SDL_Texture: the ghost borrows the
        // player's sheet, and colour-modding a shared texture would recolour
        // the living player as well.
        Uint8 cr = (Uint8)(255 - (255 - r) * (1.f - t));
        Uint8 cg = (Uint8)(255 - (255 - g) * (1.f - t));
        Uint8 cb = (Uint8)(255 - (255 - b) * (1.f - t));
        sp.setTint(cr, cg, cb);
    }
};
