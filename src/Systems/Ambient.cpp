#include "Ambient.h"
#include "Config.h"
#include "Depth.h"
#include "Level.h"
#include <cmath>

namespace {

float h01(int i) {
    Uint32 x = (Uint32)i * 0x9E3779B1u;
    x ^= x >> 15; x *= 0x85EBCA6Bu; x ^= x >> 13; x *= 0xC2B2AE35u; x ^= x >> 16;
    return (float)(x & 0xFFFFFF) / (float)0x1000000;
}

enum Zone { MEADOW, RUINS, SUNKEN, ARENA };
Zone zoneAt(float worldX) {
    const int col = (int)(worldX / Config::TILE_SIZE);
    if (col < Level::stage().zoneRuins)  return MEADOW;
    if (col < Level::stage().zoneSunken) return RUINS;
    if (col < Level::stage().zoneArena)  return SUNKEN;
    return ARENA;
}

void px(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c, Uint8 a = 255) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
    SDL_Rect q{ x, y, w, h };
    SDL_RenderFillRect(r, &q);
}

// palette entries used here (tools/palette.py)
const SDL_Color INK      { 0x1c, 0x12, 0x1c, 255 };
const SDL_Color GRASS_MID{ 0x33, 0x98, 0x4b, 255 };
const SDL_Color GRASS    { 0x5a, 0xc5, 0x4f, 255 };
const SDL_Color TAN      { 0xbf, 0x6f, 0x4a, 255 };
const SDL_Color ORANGE_DK{ 0xed, 0x76, 0x14, 255 };
const SDL_Color GOLD     { 0xff, 0xc8, 0x25, 255 };
const SDL_Color YELLOW   { 0xff, 0xeb, 0x57, 255 };
const SDL_Color CREAM    { 0xf9, 0xe6, 0xcf, 255 };

} // namespace

namespace Ambient {

void drawSky(SDL_Renderer* r, const SDL_Rect& cam, float t) {
    // Flocks of 3-5 birds drift right across the sky, one every ~2600 px of
    // background-plane space.  Hazed through Depth::RIDGES like the far hills.
    const float par = Depth::BG_TILES.parallax;
    const float origin = cam.x * par;
    const int spacing = 2600;
    const SDL_Color c = Depth::apply(INK, Depth::RIDGES);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    const int first = (int)std::floor((origin - 400.f) / spacing) - 1;
    for (int k = first; k <= first + 3; ++k) {
        const float drift = std::fmod(t * 38.f + h01(k * 7) * spacing, (float)spacing);
        const float fx = k * spacing + drift - origin;
        if (fx < -120.f || fx > Config::WINDOW_WIDTH + 40.f) continue;
        const int fy = 150 + (int)(h01(k * 11) * 110.f);
        const int n = 3 + (int)(h01(k * 13) * 3.f);
        for (int i = 0; i < n; ++i) {
            // V formation, leader in front
            const int bx = (int)fx - (i + 1) / 2 * 14;
            const int by = fy + ((i + 1) / 2) * 7 * ((i % 2) ? 1 : -1)
                           + (int)std::lround(std::sin(t * 2.f + i) * 2.f);
            const bool up = std::fmod(t * 6.f + i * 0.7f, 2.f) < 1.f;
            px(r, bx + 2, by, 1, 1, c);                    // body
            if (up) { px(r, bx, by - 2, 2, 1, c); px(r, bx + 3, by - 2, 2, 1, c);
                      px(r, bx + 1, by - 1, 1, 1, c); px(r, bx + 3, by - 1, 1, 1, c); }
            else    { px(r, bx, by, 2, 1, c); px(r, bx + 3, by, 2, 1, c); }
        }
    }
}

void drawWorld(SDL_Renderer* r, const SDL_Rect& cam, float t) {
    const int W = Config::WINDOW_WIDTH, H = Config::WINDOW_HEIGHT;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Falling leaves (meadow, ruins): one slot every 80 px, about half used.
    // They fall the height of the view, so they read as coming from the
    // canopy above whatever the camera height.
    {
        const int slot = 80, first = cam.x / slot - 1, last = (cam.x + W) / slot + 1;
        const SDL_Color cols[4] = { GRASS_MID, TAN, ORANGE_DK, GRASS };
        for (int k = first; k <= last; ++k) {
            const Zone z = zoneAt((float)k * slot);
            if ((z != MEADOW && z != RUINS) || h01(k * 3 + 1) < 0.45f) continue;
            const float speed = 34.f + h01(k * 5) * 26.f;
            const float span = H + 80.f;
            const float fall = std::fmod(t * speed + h01(k * 9) * span, span);
            const int x = k * slot + (int)(h01(k * 17) * slot)
                          + (int)std::lround(std::sin(t * 1.3f + k) * 14.f) - cam.x;
            const int y = (int)fall - 40;
            const bool flat = std::sin(t * 4.f + k) > 0.f;     // flutter
            px(r, x, y, flat ? 3 : 2, flat ? 2 : 3, cols[(int)(h01(k * 23) * 4) & 3]);
        }
    }

    // Fireflies: hover over the ground in the sunken zone and arena, a few in
    // the meadow.  World-anchored on a 120 px grid, wandering +-18 px.
    {
        const int g = 120;
        const int c0 = cam.x / g - 1, c1 = (cam.x + W) / g + 1;
        const int r0 = cam.y / g - 1, r1 = (cam.y + H) / g + 1;
        for (int gx = c0; gx <= c1; ++gx)
            for (int gy = r0; gy <= r1; ++gy) {
                const Zone z = zoneAt((float)gx * g);
                const float p = (z == SUNKEN || z == ARENA) ? 0.55f : (z == MEADOW ? 0.12f : 0.f);
                const int id = gx * 7919 + gy * 104729;
                if (h01(id) >= p) continue;
                // only in the band just above the walkable ground
                const int wy = gy * g + (int)(h01(id + 3) * g);
                if (wy < 1500 || wy > 2050) continue;
                const int wx = gx * g + (int)(h01(id + 1) * g);
                const float ph = h01(id + 5) * 6.283f;
                const int x = wx + (int)std::lround(std::sin(t * 0.7f + ph) * 18.f) - cam.x;
                const int y = wy + (int)std::lround(std::cos(t * 0.9f + ph) * 12.f) - cam.y;
                const float pulse = 0.5f + 0.5f * std::sin(t * 3.f + ph);
                px(r, x - 1, y - 1, 4, 4, GOLD, (Uint8)(40 + 60 * pulse));
                px(r, x, y, 2, 2, YELLOW, (Uint8)(120 + 135 * pulse));
            }
    }

    // Dust motes in the ruins: slow, faint, rising.
    {
        const int slot = 60, first = cam.x / slot - 1, last = (cam.x + W) / slot + 1;
        for (int k = first; k <= last; ++k) {
            if (zoneAt((float)k * slot) != RUINS || h01(k * 29 + 2) < 0.5f) continue;
            const float span = H * 0.8f;
            const float rise = std::fmod(t * (10.f + h01(k) * 8.f) + h01(k * 31) * span, span);
            const int x = k * slot + (int)(h01(k * 37) * slot)
                          + (int)std::lround(std::sin(t * 0.6f + k) * 8.f) - cam.x;
            const int y = H - 60 - (int)rise;
            px(r, x, y, 1, 1, CREAM, 150);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

} // namespace Ambient
