// Vfx — see Vfx.h.  Purely cosmetic; nothing here changes gameplay state.
#include "Vfx.h"
#include "Systems.h"
#include "SpriteSpec.h"
#include <cmath>
#include <cstdlib>

extern Manager manager;
extern float gameTime;
extern float lastFrameTime;

namespace {

    // -----------------------------------------------------------------------
    // Ring effect: an expanding, fading circle drawn straight to the renderer.
    // -----------------------------------------------------------------------
    struct Ring {
        float x, y;
        float radius, maxRadius;
        float life, maxLife;
        Uint8 r, g, b;
        float thickness;
    };

    std::vector<Ring> g_rings;

    void drawRing(SDL_Renderer* ren, const Ring& k) {
        float t = 1.f - (k.life / k.maxLife);          // 0 -> 1
        float rad = k.radius + (k.maxRadius - k.radius) * t;
        Uint8 a = (Uint8)(200.f * (1.f - t));
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, k.r, k.g, k.b, a);

        // Draw the circle as a stack of horizontal chords.  Cheap, no
        // renderer extension required, and smooth enough at this size.
        const int steps = 26;
        for (int i = 0; i < steps; i++) {
            float ang = (float)i / steps * 6.28318f;
            int px = (int)(k.x + std::cos(ang) * rad) - (int)Game::camera.x;
            int py = (int)(k.y + std::sin(ang) * rad) - (int)Game::camera.y;
            int sz = (int)(k.thickness * (1.f - t)) + 2;
            if (sz < 2) sz = 2;
            SDL_Rect q{ px - sz / 2, py - sz / 2, sz, sz };
            SDL_RenderFillRect(ren, &q);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    // -----------------------------------------------------------------------
    // Slash arc: a crescent swept in front of the attacker.
    // -----------------------------------------------------------------------
    struct Slash {
        float x, y;          // world space, centre of the swing
        float radius;
        float angStart;      // radians
        float angEnd;
        float life, maxLife;
        Uint8 r, g, b;
    };

    std::vector<Slash> g_slashes;

    void drawSlash(SDL_Renderer* ren, const Slash& s) {
        float t = 1.f - (s.life / s.maxLife);
        // sweep the visible portion of the arc outward as it plays
        float span  = (s.angEnd - s.angStart);
        float shown = s.angStart + span * std::min(1.f, t * 1.35f);
        Uint8 a = (Uint8)(235.f * (1.f - t * t));

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        const int steps = 22;
        for (int i = 0; i <= steps; i++) {
            float k = (float)i / steps;
            float ang = s.angStart + (shown - s.angStart) * k;
            // taper: thin at both tips, thick in the middle of the sweep
            float taper = std::sin(k * 3.14159f);
            int sz = (int)(11.f * taper) + 2;

            int px = (int)(s.x + std::cos(ang) * s.radius) - (int)Game::camera.x;
            int py = (int)(s.y + std::sin(ang) * s.radius) - (int)Game::camera.y;
            SDL_SetRenderDrawColor(ren, s.r, s.g, s.b, (Uint8)(a * taper));
            SDL_Rect q{ px - sz / 2, py - sz / 2, sz, sz };
            SDL_RenderFillRect(ren, &q);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

} // namespace

// ---------------------------------------------------------------------------
void Vfx::afterimage(Entity& src, float secs, Uint8 r, Uint8 g, Uint8 b) {
    if (!src.hasComponent<SpriteComponent>()) return;
    if (!src.hasComponent<TransformComponent>()) return;

    auto& ssp = src.getComponent<SpriteComponent>();
    auto& str = src.getComponent<TransformComponent>();

    auto& ghost = manager.addEntity("afterimage");
    ghost.addComponent<TransformComponent>(
        str.position.x, str.position.y, str.width, str.height, 1.f);
    ghost.getComponent<TransformComponent>().facingRight = str.facingRight;

    // Copy the frame, not just the texture, so the ghost is the same pose.
    auto& sp = ghost.addComponent<SpriteComponent>(
        "player", ssp.frameW, ssp.frameH, ssp.scale, false);
    sp.texture    = ssp.texture;
    sp.srcRect    = ssp.srcRect;
    sp.destRect   = ssp.destRect;
    sp.drawXOff   = ssp.drawXOff;
    sp.drawYOff   = ssp.drawYOff;
    sp.flip       = ssp.flip;
    sp.alpha      = 0.55f;
    ghost.addComponent<AfterimageComponent>(secs, r, g, b);
    // Its own group: Particles::draw filters on ParticleComponent, so a ghost
    // parked in that group would simply never render.
    ghost.addGroup(Game::groupAfterimages);
}

// ---------------------------------------------------------------------------
void Vfx::slashArc(Entity& attacker, int dir, int step) {
    auto& tr = attacker.getComponent<TransformComponent>();
    float cx = tr.position.x + tr.width / 2.f + dir * tr.width * 0.55f;
    float cy = tr.position.y + tr.height * 0.45f;

    // Each combo step sweeps a different way: down, up, then a wide level cut.
    float base = (dir > 0) ? 0.f : 3.14159f;
    float a0, a1;
    float radius = tr.width * 1.35f;
    switch (step) {
        case 0:  a0 = base - 0.95f; a1 = base + 0.55f; break;   // overhead chop
        case 1:  a0 = base + 0.95f; a1 = base - 0.55f; break;   // rising cut
        default: a0 = base - 1.25f; a1 = base + 1.25f; radius *= 1.18f; break;
    }
    g_slashes.push_back({ cx, cy, radius, a0, a1, 0.16f, 0.16f,
                          235, 245, 255 });
}

// ---------------------------------------------------------------------------
void Vfx::impactRing(float x, float y, Uint8 r, Uint8 g, Uint8 b,
                     float maxRadius, float secs) {
    g_rings.push_back({ x, y, 4.f, maxRadius, secs, secs, r, g, b, 5.f });
}

void Vfx::landDust(float x, float y, float power) {
    int n = (int)(4 + power * 4);
    for (int i = 0; i < n; i++) {
        float spread = 26.f * power;
        float dx = ((float)(std::rand() % 100) / 100.f - 0.5f) * spread;
        Particles::emitDust(x + dx, y, 205, 190, 160);
    }
    impactRing(x, y - 4.f, 220, 210, 180, 40.f * power, 0.24f);
}

void Vfx::sparkle(float x, float y, int count) {
    for (int i = 0; i < count; i++) {
        float a = (float)i / count * 6.28318f;
        float rad = 14.f + (float)(std::rand() % 18);
        Particles::emitDust(x + std::cos(a) * rad,
                            y + std::sin(a) * rad, 255, 240, 170);
    }
}

// ---------------------------------------------------------------------------
void Vfx::step(std::vector<Entity*>& effects) {
    for (auto it = g_rings.begin(); it != g_rings.end();) {
        it->life -= lastFrameTime;
        if (it->life <= 0.f) it = g_rings.erase(it);
        else ++it;
    }
    for (auto it = g_slashes.begin(); it != g_slashes.end();) {
        it->life -= lastFrameTime;
        if (it->life <= 0.f) it = g_slashes.erase(it);
        else ++it;
    }
    (void)effects;
}

void Vfx::draw(std::vector<Entity*>& effects) {
    // afterimages are real entities, so they draw with their group
    (void)effects;
    SDL_Renderer* ren = Game::renderer;
    if (!ren) return;
    for (const auto& s : g_slashes) drawSlash(ren, s);
    for (const auto& k : g_rings)   drawRing(ren, k);
}
