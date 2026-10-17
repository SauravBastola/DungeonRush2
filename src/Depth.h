#pragma once
// ART-13: the depth planes, as ONE table (ART_STYLE_V2 section 2.2).
//
// Every plane behind or in front of the play line is the same art seen
// through a fixed luminance/saturation transform; the transform is the only
// thing that changes with depth.  Call sites name a plane, never a constant.
#include <SDL.h>

namespace Depth {

struct Plane {
    float parallax;   // share of camera motion the plane follows
    float lum;        // luminance multiplier
    float sat;        // saturation multiplier (1 = unchanged, 0 = grey)
};

constexpr Plane SKY      { 0.00f, 1.35f, 0.45f };
constexpr Plane RIDGES   { 0.15f, 1.25f, 0.55f };
constexpr Plane BG_TILES { 0.30f, 1.10f, 0.70f };
constexpr Plane MID      { 0.55f, 0.75f, 0.85f };
constexpr Plane FG       { 1.00f, 1.00f, 1.00f };
constexpr Plane FG_SIL   { 1.10f, 0.25f, 0.60f };

// Desaturate toward the pixel's own luma, then scale; clamps to 0..255.
inline SDL_Color apply(SDL_Color c, const Plane& p) {
    const float y = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    auto ch = [&](Uint8 v) {
        float o = (y + (v - y) * p.sat) * p.lum;
        return (Uint8)(o < 0.f ? 0.f : (o > 255.f ? 255.f : o + 0.5f));
    };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

// Load a sheet and return a texture with `p` applied to every opaque pixel.
// The PNG on disk stays palette-pure; the plane is a runtime view of it.
SDL_Texture* bake(SDL_Renderer* r, const char* path, const Plane& p);

} // namespace Depth
