#pragma once
// ---------------------------------------------------------------------------
// Scenery — the whole props that stand on the terrain.
//
// These are the assembled objects from assets/objects.png (see
// tools/build_objects.py).  They are deliberately NOT tile ids.
//
// The source sheet is a 9x5 grid of 64px cells, but the art ignores that grid:
// both trees are drawn across two cells.  Placing them by tile id -- which is
// what the old decor layer did -- scattered severed tree halves around the
// level, and because the ids were guessed from colour alone, the bottom half of
// the green tree was being used as a "crystal" and the top half of the autumn
// tree as a "lamp".
//
// Each placement therefore stores an Objects::Id plus the tile whose TOP EDGE
// the prop stands on, and the sprite is drawn with its bottom centre on that
// edge.  A two-tile tree rises above the row it is anchored to; nothing needs
// to know or care how many source cells the art occupied.
//
// Tall props sway with the shared Atmosphere wind field so they lean with the
// grass and the fog instead of standing rigid while everything else moves.
// ---------------------------------------------------------------------------
#include <SDL.h>

namespace Scenery {

    // Load the props atlas (assets/objects.png).  Call once, after the
    // renderer exists.  The path is passed in because only Game knows how the
    // asset root is resolved on this platform.
    void init(SDL_Renderer* r, const char* atlasPath);

    void shutdown();

    // Draw every prop whose bounds intersect the camera.
    // `timeSecs` drives the sway; pass the same clock the rest of the world
    // animates on so the wind reads as one system.
    void draw(SDL_Renderer* r, const SDL_Rect& camera, float timeSecs);

    // Draw exactly one placement, by index into Level::SCENERY.  `draw()` is
    // a loop over this; it is exposed separately so the test harness can put a
    // single prop on screen and measure it without its neighbours straying
    // into frame and corrupting the bounding box.
    void drawProp(SDL_Renderer* r, const SDL_Rect& camera, float timeSecs,
                  int index);

    // Number of props drawn last frame (culling sanity for the test harness).
    int drawnLastFrame();

} // namespace Scenery
