// Parallax — procedural backdrop.  See Parallax.h for why this exists.
#include "Parallax.h"
#include "Config.h"
#include "Level.h"
#include "Depth.h"
#include <vector>
#include <cmath>
#include <cstdlib>

namespace {

    SDL_Texture* g_sky = nullptr;
    int g_skyH = 0;

    // --- palette -----------------------------------------------------------
    // Taken from assets/DungeonRush.png, the look this is meant to match.
    // The reference sky is a FLAT periwinkle #6464ff — sampled at y=5, y=100
    // and y=300 it is byte-for-byte identical — so the old "warm dusk"
    // indigo-to-peach ramp was fighting the target art.  There is only a very
    // slight lift toward the horizon, just enough to avoid a dead flat field.
    // ART-13: palette colours (tools/palette.py) seen through Depth::SKY.  The
    // old gradient was a saturated #6464ff from outside the palette; the
    // reference reads distance as pale, desaturated blue-green.
    // Batch B: one palette source set per stage theme, all through Depth::SKY.
    //   0 Sunken Path  - pale blue-green haze  (WATER_DK, WATER, ->GRASS_MID)
    //   1 Jungle Falls - misty jungle green    (TEAL_DK, TEAL, GRASS_MID, GRASS)
    //   2 Ember Canyon - warm dusk             (RUST, ORANGE_DK, TAN_LT, CREAM)
    struct SkyTheme { SDL_Color top, mid, low, horiz, ridge[3]; };
    const SkyTheme THEMES[3] = {
        { {0x00,0x69,0xaa,255}, {0x00,0x98,0xdc,255}, {0x1a,0x98,0x94,255}, {0x33,0x98,0x4b,255},
          { {0x00,0x98,0xdc,255}, {0x00,0x69,0xaa,255}, {0x1e,0x6f,0x50,255} } },
        { {0x13,0x4c,0x4c,255}, {0x1e,0x6f,0x50,255}, {0x33,0x98,0x4b,255}, {0x5a,0xc5,0x4f,255},
          { {0x33,0x98,0x4b,255}, {0x1e,0x6f,0x50,255}, {0x13,0x4c,0x4c,255} } },
        { {0xc6,0x45,0x24,255}, {0xed,0x76,0x14,255}, {0xe6,0x9c,0x69,255}, {0xf9,0xe6,0xcf,255},
          { {0xe6,0x9c,0x69,255}, {0xbf,0x6f,0x4a,255}, {0x8a,0x48,0x36,255} } },
    };
    int g_theme = 0;
    SDL_Color SKY_TOP, SKY_MID, SKY_LOW, SKY_HORIZ;
    void applyTheme(int t) {
        g_theme = (t >= 0 && t < 3) ? t : 0;
        SKY_TOP   = Depth::apply(THEMES[g_theme].top,   Depth::SKY);
        SKY_MID   = Depth::apply(THEMES[g_theme].mid,   Depth::SKY);
        SKY_LOW   = Depth::apply(THEMES[g_theme].low,   Depth::SKY);
        SKY_HORIZ = Depth::apply(THEMES[g_theme].horiz, Depth::SKY);
    }

    // No peach sun in the reference.  A cool pale glow instead, so the light
    // reads as haze rather than as a second, warmer light source.
    const SDL_Color SUN_CORE  = Depth::apply({ 0xf9, 0xe6, 0xcf, 255 }, Depth::SKY); // CREAM
    const SDL_Color SUN_HALO  = Depth::apply({ 0xf9, 0xe6, 0xcf,  52 }, Depth::SKY);

    // Three ridges, far to near.  `factor` is how much of the camera's motion
    // the layer follows: 0 = locked to the screen, 1 = locked to the world.
    struct Ridge {
        float factor;
        SDL_Color col;
        float baseY;       // 0..1 of screen height, where the ridge sits
        float amp;         // peak height in pixels
        float wave;        // wavelength in pixels
        float wave2;       // second, finer wave
        float amp2;
        float seed;
    };

    // far -> near.  Later entries are drawn on top and scroll faster.
    //
    // Aerial perspective: distant ridges sit CLOSE to the sky colour and get
    // darker and more saturated as they come forward.  The old set went the
    // other way, which made the far hills read as holes in the sky.
    const Ridge RIDGES[] = {
        { 0.06f, { 0x00, 0x98, 0xdc, 255 }, 0.60f,  70.f, 620.f, 190.f,  22.f, 0.0f }, // WATER
        { 0.14f, { 0x00, 0x69, 0xaa, 255 }, 0.68f,  95.f, 480.f, 150.f,  28.f, 2.7f }, // WATER_DK
        { 0.26f, { 0x1e, 0x6f, 0x50, 255 }, 0.78f, 120.f, 360.f, 120.f,  34.f, 5.1f }, // TEAL
    };

    struct Cloud { float x, y, s, spd; };
    Cloud g_clouds[] = {
        {  120.f,  90.f, 1.5f, 5.f },
        {  420.f,  60.f, 1.0f, 3.f },
        {  740.f, 120.f, 1.8f, 6.f },
        { 1050.f,  70.f, 1.2f, 4.f },
        { 1360.f, 140.f, 1.6f, 5.f },
        { 1680.f,  95.f, 1.1f, 3.5f },
        { 2000.f, 125.f, 1.4f, 4.5f },
    };
    const float CLOUD_SPAN = 2400.f;   // wraps around, so they never run out

    // Water — drawn over the terrain's water tiles using the real heightmap.
    const SDL_Color WATER_BODY{  38,  96, 150, 190 };
    const SDL_Color WATER_DEEP{  22,  58, 100, 220 };
    const SDL_Color WATER_FOAM{ 198, 232, 246, 150 };

    Uint8 lerp8(Uint8 a, Uint8 b, float t) {
        float v = (float)a + ((float)b - (float)a) * t;
        if (v < 0.f) v = 0.f;
        if (v > 255.f) v = 255.f;
        return (Uint8)v;
    }

    SDL_Color sampleSky(float t) {
        // t: 0 at the top of the screen, 1 at the bottom
        if (t < 0.45f)   return { lerp8(SKY_TOP.r, SKY_MID.r, t / 0.45f),
                                  lerp8(SKY_TOP.g, SKY_MID.g, t / 0.45f),
                                  lerp8(SKY_TOP.b, SKY_MID.b, t / 0.45f), 255 };
        if (t < 0.80f) { float k = (t - 0.45f) / 0.35f;
                         return { lerp8(SKY_MID.r, SKY_LOW.r, k),
                                  lerp8(SKY_MID.g, SKY_LOW.g, k),
                                  lerp8(SKY_MID.b, SKY_LOW.b, k), 255 }; }
        float k = (t - 0.80f) / 0.20f;
        return { lerp8(SKY_LOW.r, SKY_HORIZ.r, k),
                 lerp8(SKY_LOW.g, SKY_HORIZ.g, k),
                 lerp8(SKY_LOW.b, SKY_HORIZ.b, k), 255 };
    }

    // Smooth layered-sine "terrain".  Deterministic, no RNG, no texture.
    float ridgeY(const Ridge& R, float worldX) {
        return R.amp  * std::sin(worldX / R.wave  + R.seed)
             + R.amp2 * std::sin(worldX / R.wave2 + R.seed * 1.7f + 1.3f);
    }

    void fillColumn(SDL_Renderer* r, int x, int y, int w, int h) {
        if (h <= 0) return;
        SDL_Rect q{ x, y, w, h };
        SDL_RenderFillRect(r, &q);
    }

} // namespace

// ---------------------------------------------------------------------------
void Parallax::init(SDL_Renderer* r, int /*screenW*/, int screenH) {
    shutdown();
    if (!r) return;
    applyTheme(g_theme);

    // A 1px-wide gradient stretched across the screen.  Far cheaper than the
    // ~850 draw-line calls this used to cost every single frame.
    g_sky = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_TARGET, 1, screenH);
    if (!g_sky) return;                 // no target support: caller falls back
    g_skyH = screenH;

    SDL_Texture* prev = SDL_GetRenderTarget(r);
    SDL_SetRenderTarget(r, g_sky);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    for (int y = 0; y < screenH; y++) {
        SDL_Color c = sampleSky((float)y / (float)screenH);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        SDL_RenderDrawLine(r, 0, y, 1, y);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, prev);
}

void Parallax::setTheme(SDL_Renderer* r, int theme, int screenW, int screenH) {
    applyTheme(theme);
    init(r, screenW, screenH);           // re-bake the sky gradient
}

void Parallax::shutdown() {
    if (g_sky) { SDL_DestroyTexture(g_sky); g_sky = nullptr; }
    g_skyH = 0;
}

// ---------------------------------------------------------------------------
void Parallax::drawBackdrop(SDL_Renderer* r, int screenW, int screenH,
                            const SDL_Rect& camera) {
    if (!r) return;

    // ---- sky ----
    if (g_sky) {
        SDL_Rect dst{ 0, 0, screenW, screenH };
        SDL_RenderCopy(r, g_sky, nullptr, &dst);
    } else {
        for (int y = 0; y < screenH; y++) {
            SDL_Color c = sampleSky((float)y / (float)screenH);
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
            SDL_RenderDrawLine(r, 0, y, screenW, y);
        }
    }

    // ---- sun with a soft halo ----
    {
        float sx = screenW * 0.78f - camera.x * 0.02f;
        float sy = screenH * 0.26f - camera.y * 0.02f;
        // halo: cheap concentric rings
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 5; i >= 1; i--) {
            float rad = 30.f + i * 16.f;
            SDL_SetRenderDrawColor(r, SUN_HALO.r, SUN_HALO.g, SUN_HALO.b,
                                   SUN_HALO.a / (i * 2));
            SDL_Rect q{ (int)(sx - rad), (int)(sy - rad),
                        (int)(rad * 2),  (int)(rad * 2) };
            SDL_RenderFillRect(r, &q);
        }
        SDL_SetRenderDrawColor(r, SUN_CORE.r, SUN_CORE.g, SUN_CORE.b, 255);
        SDL_Rect core{ (int)(sx - 34), (int)(sy - 34), 68, 68 };
        SDL_RenderFillRect(r, &core);
    }

    // ---- clouds (nearest-slowest, so they read as very far away) ----
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (const auto& c : g_clouds) {
        float wx = c.x - camera.x * 0.05f;
        // wrap into a fixed span so the bank is endless
        wx = std::fmod(wx, CLOUD_SPAN);
        if (wx < -300.f) wx += CLOUD_SPAN;
        float sx = wx;
        if (sx > screenW + 300.f) continue;
        float sy = c.y - camera.y * 0.05f;
        float w  = 150.f * c.s, h = 34.f * c.s;
        SDL_SetRenderDrawColor(r, 255, 248, 236, 46);
        for (int k = 0; k < 4; k++) {
            float bw = w * (1.f - k * 0.16f);
            float bh = h * (1.f - k * 0.10f);
            SDL_Rect q{ (int)(sx - bw / 2 + k * 14.f),
                        (int)(sy - bh / 2 - k * 5.f),
                        (int)bw, (int)bh };
            SDL_RenderFillRect(r, &q);
        }
    }

    // ---- ridges ----
    const int step = 4;   // px per column; smooth enough and only ~340 rects
    // Batch G: the ridges are opaque and drawn far to near, so in each column a
    // ridge is only visible from its own skyline down to the skyline of the
    // nearest ridge in front of it.  Filling just that span draws exactly the
    // same pixels with a fraction of the fill (it used to paint the bottom of
    // the screen three times over).
    const int NR = (int)(sizeof(RIDGES) / sizeof(RIDGES[0]));
    const int cols = (screenW + step - 1) / step;
    std::vector<int> sky((size_t)NR * cols);
    for (int k = 0; k < NR; ++k)
        for (int i = 0; i < cols; ++i) {
            const float worldX = i * step + camera.x * RIDGES[k].factor;
            sky[(size_t)k * cols + i] = (int)(screenH * RIDGES[k].baseY + ridgeY(RIDGES[k], worldX)
                                              - camera.y * RIDGES[k].factor * 0.35f);
        }
    for (int k = 0; k < NR; ++k) {
        // R.col is a palette colour; the plane transform is applied here.
        const SDL_Color c = Depth::apply(THEMES[g_theme].ridge[k], Depth::RIDGES);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        for (int i = 0; i < cols; ++i) {
            const int top = sky[(size_t)k * cols + i];
            if (top >= screenH) continue;
            int bottom = screenH;
            for (int j = k + 1; j < NR; ++j)
                bottom = std::min(bottom, std::max(top, sky[(size_t)j * cols + i]));
            fillColumn(r, i * step, top, step, bottom - top);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ---------------------------------------------------------------------------
void Parallax::drawWater(SDL_Renderer* r, int screenW, int screenH,
                         const SDL_Rect& camera, float timeSecs) {
    if (!r) return;

    // The terrain puts water four tiles below the ground top of each column
    // (see tile_for() in tools/build_level.py).  Reading the same heightmap
    // here means the overlay always lines up with the tiles underneath.
    const int T = Config::TILE_SIZE;
    const int firstCol = camera.x / T - 1;
    const int lastCol  = (camera.x + camera.w) / T + 1;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int col = firstCol; col <= lastCol; col++) {
        if (col < 0 || col >= Level::WIDTH_TILES) continue;
        int top = Level::stage().groundTop[col];
        int sx  = col * T - camera.x;

        // Water exists only in the chasms.  GROUND_TOP >= 0 means solid
        // ground, and painting a water column there used to drown the bottom
        // of every ordinary column in translucent blue.
        //
        // Deriving the surface from the per-column height is what broke this:
        // GROUND_TOP is -1 in a chasm, so `(top + 4) * T` put the surface at
        // row 3 — a column of water hanging in the sky, hundreds of pixels
        // above the ground the player walks on.  The tiles use the fixed
        // WATER_SURFACE_ROW exported by the generator, so use the same one.
        if (top >= 0) continue;

        float surfaceY = (float)(Level::WATER_SURFACE_ROW * T);
        float sy = surfaceY - camera.y;
        if (sy >= screenH) continue;

        int iy = (int)sy;
        if (iy < 0) iy = 0;
        int h = screenH - iy;
        if (h <= 0) continue;

        // body, fading darker with depth
        SDL_Rect body{ sx, iy, T, h };
        SDL_SetRenderDrawColor(r, WATER_BODY.r, WATER_BODY.g, WATER_BODY.b,
                               WATER_BODY.a);
        SDL_RenderFillRect(r, &body);

        int deepTop = iy + 46;
        if (deepTop < screenH) {
            SDL_Rect deep{ sx, deepTop, T, screenH - deepTop };
            SDL_SetRenderDrawColor(r, WATER_DEEP.r, WATER_DEEP.g,
                                   WATER_DEEP.b, WATER_DEEP.a);
            SDL_RenderFillRect(r, &deep);
        }

        // animated crest — two sines at different speeds read as a swell
        float wave = 3.2f * std::sin(timeSecs * 1.7f + col * 0.55f)
                   + 1.8f * std::sin(timeSecs * 2.9f + col * 1.30f);
        int crestY = iy + (int)wave;

        SDL_SetRenderDrawColor(r, WATER_FOAM.r, WATER_FOAM.g, WATER_FOAM.b,
                               WATER_FOAM.a);
        SDL_Rect foam{ sx, crestY, T, 3 };
        SDL_RenderFillRect(r, &foam);

        // a second, dimmer line just below for a bit of thickness
        SDL_SetRenderDrawColor(r, WATER_FOAM.r, WATER_FOAM.g, WATER_FOAM.b, 70);
        SDL_Rect foam2{ sx, crestY + 7, T, 2 };
        SDL_RenderFillRect(r, &foam2);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    (void)screenW;
}
