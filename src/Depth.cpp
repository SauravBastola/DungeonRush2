#include "Depth.h"
#include <SDL_image.h>
#include <iostream>

namespace Depth {

SDL_Texture* bake(SDL_Renderer* r, const char* path, const Plane& p) {
    SDL_Surface* src = IMG_Load(path);
    if (!src) {
        std::cerr << "[Depth] IMG_Load failed for '" << path << "': "
                  << IMG_GetError() << "\n";
        return nullptr;
    }
    SDL_Surface* s = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(src);
    if (!s) return nullptr;
    SDL_LockSurface(s);
    for (int y = 0; y < s->h; ++y) {
        Uint8* row = (Uint8*)s->pixels + y * s->pitch;
        for (int x = 0; x < s->w; ++x) {
            Uint8* px = row + x * 4;               // RGBA32 is byte-ordered R,G,B,A
            if (px[3] == 0) continue;
            SDL_Color o = apply({ px[0], px[1], px[2], px[3] }, p);
            px[0] = o.r; px[1] = o.g; px[2] = o.b;
        }
    }
    SDL_UnlockSurface(s);
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    return t;
}

} // namespace Depth
