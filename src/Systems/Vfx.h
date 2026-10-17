#pragma once
// ---------------------------------------------------------------------------
// Vfx — the juice layer.
//
// Combat in the original build was functionally fine but felt weightless:
// swings connected with no feedback and the dash was invisible.  Everything
// here is cosmetic and lives entirely outside gameplay logic, so it can be
// turned up or removed without touching a single rule.
// ---------------------------------------------------------------------------
#include "ECS.h"
#include "Components.h"
#include "Config.h"
#include "Game.h"
#include <vector>
#include <SDL.h>

namespace Vfx {

    // A fading copy of `src`, left behind for `secs`.  Dash trail, mainly.
    void afterimage(Entity& src, float secs = 0.26f,
                    Uint8 r = 120, Uint8 g = 190, Uint8 b = 255);

    // Curved slash arc in front of the attacker.  `step` 0/1/2 picks the
    // swing direction so a combo reads as three distinct motions.
    void slashArc(Entity& attacker, int dir, int step);

    // Expanding ring at an impact point — hits, blasts, landings.
    void impactRing(float x, float y, Uint8 r, Uint8 g, Uint8 b,
                    float maxRadius = 46.f, float secs = 0.28f);

    // A burst of dust kicked up when something lands.
    void landDust(float x, float y, float power = 1.f);

    // Drifting motes, for healing shrines and powerup pickups.
    void sparkle(float x, float y, int count = 8);

    // Advance every transient effect and drop the finished ones.
    void step(std::vector<Entity*>& effects);
    void draw(std::vector<Entity*>& effects);

} // namespace Vfx
