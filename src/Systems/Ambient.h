#pragma once
// Batch A: ambient life with no gameplay effect -- birds on the background
// plane; falling leaves, fireflies and dust motes in the world plane.
//
// Every element is a pure function of (time, world position): no entities,
// no random state, nothing to update.  Two runs (and two harness captures)
// draw exactly the same pixels, and nothing here can touch the simulation.
// All shapes are whole pixels in palette colours, like the rest of the art.
#include <SDL.h>

namespace Ambient {
// After the background tiles, before the mid layer (birds behind the forest).
void drawSky(SDL_Renderer* r, const SDL_Rect& cam, float t);
// After entities and particles, before the fog.
void drawWorld(SDL_Renderer* r, const SDL_Rect& cam, float t);
}
