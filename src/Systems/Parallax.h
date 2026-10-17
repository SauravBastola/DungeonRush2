#pragma once
// ---------------------------------------------------------------------------
// Parallax — the world behind the world.
//
// The tile background layer shipped with this project referenced tile ids
// 111..145 out of a 48-tile atlas, so it drew nothing at all and the game ran
// against a flat colour.  Rather than depend on art that is not there, the
// backdrop here is generated procedurally: a cached sky gradient, three hill
// ridges that scroll at different rates, drifting cloud banks, and an
// animated water surface driven by the level's own heightmap.
//
// Everything is deterministic — it looks the same every run, and it needs no
// assets, so it can never fall out of sync with the terrain again.
// ---------------------------------------------------------------------------
#include <SDL.h>

namespace Parallax {

    // Build the cached sky gradient.  Call once, after the renderer exists.
    void init(SDL_Renderer* r, int screenW, int screenH);

    // Free the cached texture.
    void shutdown();

    // Sky + sun + hills + clouds.  Draw before any world geometry.
    void drawBackdrop(SDL_Renderer* r, int screenW, int screenH,
                      const SDL_Rect& camera);

    // Animated water.  Draw after the terrain so it sits over the water tiles.
    void drawWater(SDL_Renderer* r, int screenW, int screenH,
                   const SDL_Rect& camera, float timeSecs);

} // namespace Parallax
