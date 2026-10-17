// Scenery — see Scenery.h for the overview.
#include "Scenery.h"
#include "Atmosphere.h"
#include "../Objects.h"
#include "../Level.h"
#include "../TextureManager.h"
#include <cmath>

namespace {

    SDL_Texture* g_tex = nullptr;
    int g_drawn = 0;

    // Peak sway in degrees at full wind, per object.  A rigid rock should not
    // wobble, a two-tile tree should: the taller and thinner the prop, the
    // more the wind gets hold of it.
    float swayFor(int objId, int pixelHeight) {
        switch (objId) {
            case Objects::OBJ_TREE_GREEN:
            case Objects::OBJ_TREE_AUTUMN:
                return 2.6f;
            case Objects::OBJ_BUSH:
                return 1.7f;
            case Objects::OBJ_BRANCH_RED:
                return 2.2f;
            case Objects::OBJ_CRYSTAL:
            case Objects::OBJ_SHARD:
                return 0.6f;
            default:
                // Boulders are heavy; only the very largest budget any lean.
                return pixelHeight > 70 ? 0.3f : 0.0f;
        }
    }

} // namespace

void Scenery::init(SDL_Renderer* /*r*/, const char* atlasPath) {
    shutdown();
    // TextureManager creates against Game::renderer, so the renderer argument
    // is only here to make the call site self-documenting about ordering.
    g_tex = TextureManager::LoadTexture(atlasPath);
    if (g_tex) {
        // The atlas is authored with a hard alpha edge, and smoothing it here
        // would halo every prop with transparent background pixels.
        SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
    }
}

void Scenery::shutdown() {
    if (g_tex) { SDL_DestroyTexture(g_tex); g_tex = nullptr; }
    g_drawn = 0;
}

int Scenery::drawnLastFrame() { return g_drawn; }

void Scenery::draw(SDL_Renderer* r, const SDL_Rect& camera, float timeSecs) {
    g_drawn = 0;
    for (int i = 0; i < Level::N_SCENERY; i++) drawProp(r, camera, timeSecs, i);
}

void Scenery::drawProp(SDL_Renderer* r, const SDL_Rect& camera, float timeSecs,
                       int index) {
    if (!g_tex) return;
    if (index < 0 || index >= Level::N_SCENERY) return;

    const Level::SceneryDef& S = Level::SCENERY[index];
    if (S.id < 0 || S.id >= Objects::OBJ_COUNT) return;
    const Objects::Rect& R = Objects::RECTS[S.id];

    const int w = (int)std::lround(R.w * S.scale);
    const int h = (int)std::lround(R.h * S.scale);
    if (w <= 0 || h <= 0) return;

    // Anchor: centre of the tile horizontally, its top edge vertically.
    // The sprite hangs above that edge, so a two-tile tree simply rises
    // into the rows above without anything having to reserve them.
    const float anchorX = (S.col + 0.5f) * (float)Level::TILE;
    const float anchorY = S.row * (float)Level::TILE;

    SDL_Rect dst{ (int)(anchorX - camera.x - w * 0.5f),
                  (int)(anchorY - camera.y - h),
                  w, h };

    // Cull against the camera with a margin wide enough to cover the
    // bounding box of a swaying prop, not just the prop itself.
    if (dst.x + w < -8 || dst.x > camera.w + 8) return;
    if (dst.y + h < -8 || dst.y > camera.h + 8) return;

    const float sway = swayFor(S.id, h);
    double angle = 0.0;
    if (sway > 0.f) {
        // Slow per-object phase offset so the props do not all snap to
        // the same gust in lockstep.
        float phase = (float)index * 1.7f;
        float gust = std::sin(timeSecs * 0.31f + phase) * 0.6f
                   + std::sin(timeSecs * 0.73f + phase * 0.5f) * 0.4f;
        angle = Atmosphere::windAt(anchorX, timeSecs) * sway * 0.5
              + gust * sway * 0.5;
    }

    SDL_Rect src{ R.x, R.y, R.w, R.h };
    // Pivot at the bottom centre: the root stays planted while the top
    // leans, which is what makes the sway read as wind rather than spin.
    SDL_Point pivot{ w / 2, h };
    SDL_RenderCopyEx(r, g_tex, &src, &dst, angle, &pivot,
                     S.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    g_drawn++;
}
