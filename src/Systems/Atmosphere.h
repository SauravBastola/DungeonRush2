#pragma once
// ---------------------------------------------------------------------------
// Atmosphere — the weather on top of the terrain.
//
// Volumetric fog, wind and grass, all generated procedurally in the
// platformer.png palette so they cannot drift out of sync with the tiles the
// way an authored overlay would.
//
// Everything is driven by one shared wind field (see windAt) so the grass, the
// scenery props and the fog all lean the same way at the same moment.  That
// shared source is what makes it read as weather rather than as three separate
// effects that happen to be on screen together.
//
// Draw order, all of it after the world and before the HUD:
//     drawGrass   - sits on the surface, behind the player
//     Scenery     - whole props, drawn between the two
//     drawFog     - last, so it veils everything equally
// ---------------------------------------------------------------------------
#include <SDL.h>

namespace Atmosphere {

    // Build cached fog textures.  Call once, after the renderer exists.
    void init(SDL_Renderer* r, int screenW, int screenH);

    // Free cached textures and reset the wind clock.
    void shutdown();

    // Advance the wind.  Driven by real time so it stays smooth regardless
    // of frame rate.
    void update(float dt);

    // Wind strength at a world position, roughly -1..1.  Positive leans right.
    float windAt(float worldX, float timeSecs);

    // Bright green cap + swaying blades along every walkable surface.
    // Trees are NOT here: they are real sprites from the tileset, drawn by
    // Systems/Scenery.cpp.
    void drawGrass(SDL_Renderer* r, const SDL_Rect& camera);

    // Layered drifting fog.  Denser low down, so it reads as depth haze.
    void drawFog(SDL_Renderer* r, int screenW, int screenH,
                 const SDL_Rect& camera, float timeSecs);

} // namespace Atmosphere
