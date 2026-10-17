#pragma once
#include "ECS.h"
#include "Vector2D.h"
#include "Game.h"
#include "TextureManager.h"

class TileComponent : public Component {
public:
    SDL_Texture* texture = nullptr;
    SDL_Rect     srcRect{0,0,0,0}, destRect{0,0,0,0};
    Vector2D     position;
    float        parallax = 1.f;
    // Per-layer colour modulation.  Distant layers are pushed toward the
    // cold end of the palette so the terrain reads as depth rather than as
    // three copies of the same green tiles stacked on each other.
    //
    // This used to be inferred from the parallax factor with
    // `if (parallax < 0.3f)` — but the background layer's factor IS 0.3f, so
    // the comparison was false and the distant layer drew at full brightness
    // the whole time.  Depth cueing silently did nothing.
    Uint8        tintR = 255, tintG = 255, tintB = 255;

    TileComponent() = default;
    TileComponent(int srcX, int srcY, int xpos, int ypos, int tsize, int tscale,
                  const std::string& id, float par = 1.f,
                  Uint8 tr = 255, Uint8 tg = 255, Uint8 tb = 255) {
        texture = Game::assets ? Game::assets->GetTexture(id) : nullptr;
        srcRect = { srcX, srcY, tsize, tsize };
        position = { (float)xpos, (float)ypos };
        destRect = { xpos, ypos, tsize * tscale, tsize * tscale };
        parallax = par;
        tintR = tr; tintG = tg; tintB = tb;
    }

    void update() override {
        destRect.x = (int)(position.x - Game::camera.x * parallax);
        destRect.y = (int)(position.y - Game::camera.y * parallax);
    }
    void draw() override {
        if (!texture) return;
        const bool tinted = (tintR != 255 || tintG != 255 || tintB != 255);
        if (tinted) SDL_SetTextureColorMod(texture, tintR, tintG, tintB);
        TextureManager::Draw(texture, srcRect, destRect, SDL_FLIP_NONE);
        if (tinted) SDL_SetTextureColorMod(texture, 255, 255, 255);
    }
};
