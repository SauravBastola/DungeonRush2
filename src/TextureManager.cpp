#include "TextureManager.h"
#include <iostream>
#include <SDL_image.h>

SDL_Texture* TextureManager::LoadTexture(const char* path)
{
    SDL_Surface* tempSurface = IMG_Load(path);
    if (!tempSurface) {
        std::cerr << "[TextureManager] IMG_Load failed for '" << path << "': " << IMG_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(Game::renderer, tempSurface);
    SDL_FreeSurface(tempSurface);
    if (!tex) {
        std::cerr << "[TextureManager] CreateTexture failed for '" << path << "': " << SDL_GetError() << std::endl;
    }
    return tex;
}

void TextureManager::Draw(SDL_Texture * tex, SDL_Rect src, SDL_Rect dest, SDL_RendererFlip flip)
{
    if (!tex) return;
    // simple frustum culling for performance
    if (dest.x + dest.w < 0 || dest.x > Config::WINDOW_WIDTH ||
        dest.y + dest.h < 0 || dest.y > Config::WINDOW_HEIGHT) {
        // Still draw tiles? Don't cull aggressively for tiles but keep for entities; for now draw all
    }
    SDL_RenderCopyEx(Game::renderer, tex, &src, &dest, 0, nullptr, flip);
}
