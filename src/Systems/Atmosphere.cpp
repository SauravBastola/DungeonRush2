// Atmosphere — see Atmosphere.h for the overview.
#include "Atmosphere.h"
#include "Config.h"
#include "Game.h"
#include "Level.h"
#include "ECS.h"
#include "Components.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

extern Manager manager;
extern float gameTime;

namespace {

    // --- palette, straight from assets/platformer.png ----------------------
    // Only the grass and fog colours remain here.  Trees used to be drawn
    // procedurally with their own trunk/canopy palette, but the sheet has two
    // hand-painted trees that look better and are guaranteed to match the
    // terrain, so Scenery draws those instead.
    const SDL_Color GRASS_LIT  {  90, 197,  79, 255 };   // #5ac54f
    const SDL_Color GRASS_MID  {  62, 154,  66, 255 };
    const SDL_Color GRASS_DARK {  30, 111,  80, 255 };   // #1e6f50

    const SDL_Color FOG_NEAR   { 196, 214, 246, 255 };
    const SDL_Color FOG_FAR    { 150, 176, 226, 255 };

    // --- wind --------------------------------------------------------------
    float g_time = 0.f;

    // Deterministic 0..1 hash so tree placement is stable across runs and
    // across reloads.  Uses a standard LCG rather than rand() so the layout
    // never shifts under a player who is learning the level.
    float hash1(int n) {
        unsigned x = (unsigned)(n * 2654435761u);
        x ^= x >> 15; x *= 2246822519u;
        x ^= x >> 13; x *= 3266489917u;
        x ^= x >> 16;
        return (float)(x & 0xFFFFFF) / (float)0x1000000;
    }

    void fillRect(SDL_Renderer* r, int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return;
        SDL_Rect q{ x, y, w, h };
        SDL_RenderFillRect(r, &q);
    }

    // --- cached fog sheet --------------------------------------------------
    // A scrolling full-width rect of flat colour looks completely static: the
    // eye has nothing to track, so drifting it does nothing on screen.  Fog
    // needs horizontal density variation — wisps — to read as moving.
    //
    // Building that with filled rects every frame costs thousands of draws, so
    // the sheet is generated once into a texture and then simply scrolled.
    // It is twice the screen width so the scroll wraps without a seam, and
    // every frequency used is an integer number of cycles across the sheet,
    // which is what makes the left and right edges match up.
    SDL_Texture* g_fog = nullptr;
    int g_fogW = 0, g_fogH = 0;

    void buildFogSheet(SDL_Renderer* r, int screenW) {
        if (g_fog) { SDL_DestroyTexture(g_fog); g_fog = nullptr; }
        g_fogW = screenW * 2;
        g_fogH = 128;
        g_fog = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_STREAMING, g_fogW, g_fogH);
        if (!g_fog) return;

        // Textures are created with SDL_BLENDMODE_NONE, which ignores both the
        // per-pixel alpha and SDL_SetTextureAlphaMod and blits opaquely.  Left
        // at the default the fog bands painted solid slabs of #c4d6f6 across
        // the screen -- roughly a third of the frame was flat fog colour with
        // the world completely hidden behind it, while every alpha value in
        // the band table looked, on paper, like a tasteful 5%.
        SDL_SetTextureBlendMode(g_fog, SDL_BLENDMODE_BLEND);

        Uint8* px = nullptr;
        int    pitch = 0;
        if (SDL_LockTexture(g_fog, nullptr, (void**)&px, &pitch) != 0) return;

        // Every pixel goes through SDL_MapRGBA rather than being written as
        // four bare bytes.  Hand-packing silently got this wrong twice: the
        // channels of a 32-bit SDL texture are NOT guaranteed to sit in memory
        // as R,G,B,A.  On this platform SDL_PIXELFORMAT_RGBA8888 is laid out
        // A,B,G,R, so writing R,G,B,A put the density value into the red slot
        // and the constant red channel (196) into alpha -- the fog then drew
        // at a flat 77% opacity and no amount of SDL_SetTextureAlphaMod moved
        // it.  Verified with a probe: bytes 11 22 33 44 read back as R=44
        // G=33 B=22.  Mapping through the real format removes the guesswork,
        // and honouring the returned pitch removes the width*4 assumption.
        Uint32 fmt = SDL_PIXELFORMAT_RGBA8888;
        SDL_QueryTexture(g_fog, &fmt, nullptr, nullptr, nullptr);
        SDL_PixelFormat* pf = SDL_AllocFormat(fmt);
        if (!pf) { SDL_UnlockTexture(g_fog); return; }

        for (int y = 0; y < g_fogH; y++) {
            // Vertical profile: soft top and bottom, densest in the middle.
            float vy = (float)y / (float)g_fogH;
            float vprof = 0.5f - 0.5f * std::cos(vy * 6.2831853f);
            vprof = vprof * vprof;               // tighter falloff, softer edges
            Uint8* row = px + (size_t)y * (size_t)pitch;
            for (int x = 0; x < g_fogW; x++) {
                float ux = (float)x / (float)g_fogW * 6.2831853f;
                // Sum of sines with INTEGER cycle counts (3, 7, 13) over the
                // sheet width, so the pattern is seamless when wrapped.
                float dens =
                      0.50f
                    + 0.26f * std::sin(ux * 3.0f + 0.7f)
                    + 0.16f * std::sin(ux * 7.0f + 2.1f)
                    + 0.08f * std::sin(ux * 13.0f + 4.3f);
                if (dens < 0.f) dens = 0.f;
                if (dens > 1.f) dens = 1.f;
                float a = vprof * dens;
                Uint32 v = SDL_MapRGBA(pf, FOG_NEAR.r, FOG_NEAR.g, FOG_NEAR.b,
                                       (Uint8)(a * 255.f));
                std::memcpy(row + (size_t)x * 4, &v, 4);
            }
        }
        SDL_UnlockTexture(g_fog);
        SDL_FreeFormat(pf);
    }

    // A swaying blade: a stack of small quads, each offset further than the
    // one below, so the blade bends instead of shearing.
    void drawBlade(SDL_Renderer* r, float baseX, float baseY,
                   int height, int width, float lean) {
        const int steps = 4;
        for (int i = 0; i < steps; i++) {
            float t  = (float)i / (float)steps;
            float t2 = (float)(i + 1) / (float)steps;
            int y0 = (int)(baseY - height * t);
            int y1 = (int)(baseY - height * t2);
            // Offset grows with the SQUARE of height along the blade, so the
            // blade curves (tip displaced most) instead of shearing straight
            // over.  Each segment is drawn at its own x, so the quad stack
            // reads as a bent stem.
            int x0 = (int)(baseX + lean * t  * t  * height * 0.55f);
            int w  = width - i;                      // taper toward the tip
            if (w < 1) w = 1;
            fillRect(r, x0, y1, w, y0 - y1 + 1);
        }
    }

} // namespace

// ---------------------------------------------------------------------------
void Atmosphere::init(SDL_Renderer* r, int screenW, int /*screenH*/) {
    g_time = 0.f;
    buildFogSheet(r, screenW);
}

void Atmosphere::shutdown() {
    g_time = 0.f;
    if (g_fog) { SDL_DestroyTexture(g_fog); g_fog = nullptr; }
    g_fogW = g_fogH = 0;
}

void Atmosphere::update(float dt) { g_time += dt; }

// ---------------------------------------------------------------------------
float Atmosphere::windAt(float worldX, float timeSecs) {
    // Two sines at different spatial frequencies: the slow one is the gust
    // envelope (whole stretches of level lean together), the fast one adds
    // local flutter so it does not look like a rigid sliding offset.
    //
    // The flutter has to be quick enough to actually read.  At the original
    // 0.94 rad/s a full cycle took ~7s and the per-second change was small
    // enough that the grass tips moved less than a pixel between frames —
    // the world was technically windy and visibly frozen.  2.2 rad/s gives
    // roughly a 3s flutter, which is what a light breeze looks like.
    float gust   = 0.55f * std::sin(timeSecs * 0.31f + worldX * 0.0011f);
    float flutter= 0.45f * std::sin(timeSecs * 2.20f + worldX * 0.0047f);
    // A steady bias so the wind has a prevailing direction and the level does
    // not feel like it is breathing in and out symmetrically.
    return gust + flutter + 0.22f;
}

// ---------------------------------------------------------------------------
void Atmosphere::drawGrass(SDL_Renderer* r, const SDL_Rect& camera) {
    const int T = Config::TILE_SIZE;
    const int firstCol = camera.x / T - 1;
    const int lastCol  = (camera.x + camera.w) / T + 1;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int col = firstCol; col <= lastCol; col++) {
        if (col < 0 || col >= Level::WIDTH_TILES) continue;
        int top = Level::GROUND_TOP[col];
        if (top < 0) continue;                       // chasm: no ground to grow on
        int sx = col * T - camera.x;

        // A chasm edge should not sprout grass sideways into thin air.
        bool leftOpen  = (col == 0) || (Level::GROUND_TOP[col - 1] < 0);
        bool rightOpen = (col == Level::WIDTH_TILES - 1)
                         || (Level::GROUND_TOP[col + 1] < 0);

        float surfaceY = (float)((top + 1) * T) - camera.y;
        if (surfaceY < -40.f || surfaceY > camera.h + 40.f) continue;

        float w = windAt((float)(col * T), g_time);

        // The solid cap: the reference art has a continuous band of
        // #5ac54f along the top of the ground, not just scattered blades.
        SDL_SetRenderDrawColor(r, GRASS_LIT.r, GRASS_LIT.g, GRASS_LIT.b, 255);
        fillRect(r, sx, (int)surfaceY, T, 7);
        SDL_SetRenderDrawColor(r, GRASS_MID.r, GRASS_MID.g, GRASS_MID.b, 255);
        fillRect(r, sx, (int)surfaceY + 7, T, 5);
        // A dark seam where the green meets the teal below.  Without it the
        // strip reads as a painted-on ribbon rather than as growth.
        SDL_SetRenderDrawColor(r, GRASS_DARK.r, GRASS_DARK.g, GRASS_DARK.b, 255);
        fillRect(r, sx, (int)surfaceY + 12, T, 3);

        // Blades, five per tile at deterministic offsets.
        for (int k = 0; k < 5; k++) {
            float h = hash1(col * 31 + k * 7);
            int bx = sx + 4 + (int)(h * (T - 10));
            // Tuck the outermost blades inward on a ledge so they do not
            // overhang the drop.
            if (leftOpen  && bx < sx + 6)  bx = sx + 6;
            if (rightOpen && bx > sx + T - 8) bx = sx + T - 8;
            int bh = 9 + (int)(hash1(col * 17 + k * 13) * 11.f);
            int bw = 2 + (int)(hash1(col * 5 + k) * 1.6f);
            // Per-blade phase so a gust travels across the tile instead of
            // moving every blade in lockstep.
            float bwnd = w + 0.18f * std::sin(g_time * 2.1f + (float)(col * 3 + k));
            SDL_SetRenderDrawColor(r,
                (k & 1) ? GRASS_LIT.r : GRASS_MID.r,
                (k & 1) ? GRASS_LIT.g : GRASS_MID.g,
                (k & 1) ? GRASS_LIT.b : GRASS_MID.b, 255);
            drawBlade(r, (float)bx, surfaceY + 2.f, bh, bw, bwnd * 0.70f);
        }
    }

    // Grass on top of platforms, including the moving ones — read the live
    // transforms so the tufts ride along instead of staying where the slab
    // was spawned.
    for (auto* p : manager.getGroup(Game::groupPlatforms)) {
        if (!p->isActive() || !p->hasComponent<TransformComponent>()) continue;
        auto& tr = p->getComponent<TransformComponent>();
        float wx = tr.position.x, wy = tr.position.y;
        if (wx + tr.width  < camera.x || wx > camera.x + camera.w) continue;
        float sx = wx - camera.x, sy = wy - camera.y;
        if (sy < -40.f || sy > camera.h + 40.f) continue;

        float w = windAt(wx, g_time);
        int tiles = (int)(tr.width / T);
        if (tiles < 1) tiles = 1;
        SDL_SetRenderDrawColor(r, GRASS_LIT.r, GRASS_LIT.g, GRASS_LIT.b, 255);
        fillRect(r, (int)sx, (int)sy, (int)tr.width, 6);
        SDL_SetRenderDrawColor(r, GRASS_MID.r, GRASS_MID.g, GRASS_MID.b, 255);
        fillRect(r, (int)sx, (int)sy + 6, (int)tr.width, 4);

        for (int k = 0; k < tiles * 3; k++) {
            float h = hash1((int)wx + k * 29);
            int bx = (int)sx + 4 + (int)(h * (tr.width - 10));
            int bh = 8 + (int)(hash1((int)wx + k * 11) * 9.f);
            float bwnd = w + 0.18f * std::sin(g_time * 2.1f + (float)k);
            SDL_SetRenderDrawColor(r,
                (k & 1) ? GRASS_LIT.r : GRASS_MID.r,
                (k & 1) ? GRASS_LIT.g : GRASS_MID.g,
                (k & 1) ? GRASS_LIT.b : GRASS_MID.b, 255);
            drawBlade(r, (float)bx, sy + 2.f, bh, 2, bwnd * 0.70f);
        }
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ---------------------------------------------------------------------------
void Atmosphere::drawFog(SDL_Renderer* r, int screenW, int screenH,
                         const SDL_Rect& camera, float timeSecs) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // --- depth haze: a world-space gradient anchored to the map, so it does
    // not slide up and down the screen as the camera moves. ---
    {
        // Fog thickens toward the bottom of the world.  Anchoring to
        // PIXEL_HEIGHT means a fixed point in the level always has the same
        // amount of haze over it.
        const int rows = 14;
        for (int i = 0; i < rows; i++) {
            float t = (float)i / (float)rows;
            float worldY = camera.y + screenH * t;
            float depth = worldY / (float)Level::PIXEL_HEIGHT;
            if (depth < 0.f) depth = 0.f;
            if (depth > 1.f) depth = 1.f;
            // Ramp in over the lower third of the level.  This is a flat
            // gradient rather than wisps, so it is kept well below the bands:
            // it exists to push the bottom of the level back, not to hide it.
            float a = (depth - 0.55f) / 0.45f;
            if (a <= 0.f) continue;
            if (a > 1.f) a = 1.f;
            Uint8 alpha = (Uint8)(a * 26.f);
            if (alpha < 1) continue;
            SDL_SetRenderDrawColor(r, FOG_FAR.r, FOG_FAR.g, FOG_FAR.b, alpha);
            fillRect(r, 0, (int)(screenH * t), screenW,
                     (int)(screenH / (float)rows) + 1);
        }
    }

    // --- drifting bands: the part that reads as "volumetric".  Several soft
    // horizontal layers scroll at different rates, so the fog has parallax
    // instead of being a flat sheet glued to the screen. ---
    struct Band {
        float factor;     // how much of the camera's motion it follows
        float yFrac;      // resting vertical position, 0..1 of screen height
        float thick;      // band thickness in pixels
        float speed;      // horizontal drift, px/sec
        float alpha;      // peak alpha
        float seed;
    };
    // Alpha here is a PEAK, not an average.  The sheet's own alpha averages
    // about 19% (it is the product of a cos^2 vertical falloff and a sine
    // density field that spends most of its time near the middle), so a peak
    // of 42 lands as roughly 16% coverage in the thickest wisps and about 3%
    // between them.  That is the range that reads as weather; the earlier
    // 15/13/10 averaged out to barely 1% and the fog was invisible.
    // The three bands are placed so their half-thicknesses do not overlap,
    // which is what keeps them from stacking into a single slab.
    // Denser toward the bottom: near fog occludes more than distant fog.
    const Band BANDS[] = {
        { 0.10f, 0.54f, 110.f,  9.f, 30.f, 0.0f },
        { 0.18f, 0.70f, 130.f, 15.f, 38.f, 1.7f },
        { 0.28f, 0.86f, 120.f, 24.f, 46.f, 3.4f },
    };

    if (!g_fog) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        return;                       // headless / no texture support: skip
    }

    for (const auto& B : BANDS) {
        // Vertical bob so the bands breathe rather than sitting on rails.
        float bob = 16.f * std::sin(timeSecs * 0.22f + B.seed);
        float cy = screenH * B.yFrac + bob - camera.y * B.factor * 0.30f;
        if (cy + B.thick < 0.f || cy - B.thick > screenH) continue;

        // Scroll the sheet.  Wrapping the offset into the sheet width is what
        // keeps the wisps continuous however far the level runs.
        float off = std::fmod(timeSecs * B.speed + B.seed * 90.f, (float)g_fogW);
        if (off < 0.f) off += (float)g_fogW;
        SDL_Rect src{ (int)off, 0, screenW, g_fogH };
        SDL_Rect dst{ 0, (int)(cy - B.thick * 0.5f), screenW, (int)B.thick };
        SDL_SetTextureAlphaMod(g_fog, (Uint8)B.alpha);
        SDL_RenderCopy(r, g_fog, &src, &dst);
        SDL_SetTextureAlphaMod(g_fog, 255);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
