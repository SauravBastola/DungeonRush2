#include "AssetManager.h"
#include "Components.h"

AssetManager::AssetManager(Manager *man) : manager(man) {}
AssetManager::~AssetManager() = default;

void AssetManager::AddTexture(std::string id, const char *path) {
    SDL_Texture* tex = TextureManager::LoadTexture(path);
    if (!tex) {
        std::cerr << "[AssetManager] Failed to load texture '" << id
                  << "' at " << path << "\n";
    }
    textures[id] = tex;
}

void AssetManager::AddTexture(std::string id, SDL_Texture* tex) {
    if (!tex) std::cerr << "[AssetManager] null texture for '" << id << "'\n";
    textures[id] = tex;
}

SDL_Texture *AssetManager::GetTexture(std::string id) {
    auto it = textures.find(id);
    if (it == textures.end()) return nullptr;
    return it->second;
}

void AssetManager::AddFont(std::string id, std::string path, int fontSize) {
    TTF_Font* font = TTF_OpenFont(path.c_str(), fontSize);
    if (!font) {
        std::cerr << "[AssetManager] TTF_OpenFont failed '" << id
                  << "' at " << path << ": " << TTF_GetError() << "\n";
    }
    fonts[id] = font;
}

TTF_Font *AssetManager::GetFont(std::string id) {
    auto it = fonts.find(id);
    if (it == fonts.end()) return nullptr;
    return it->second;
}

void AssetManager::AddSound(std::string id, const char *path) {
    Mix_Chunk* chunk = Mix_LoadWAV(path);
    if (!chunk) {
        std::cerr << "[AssetManager] Mix_LoadWAV failed '" << id
                  << "' at " << path << ": " << Mix_GetError() << "\n";
    }
    sounds[id] = chunk;
}

Mix_Chunk* AssetManager::GetSound(std::string id) {
    auto it = sounds.find(id);
    if (it == sounds.end()) return nullptr;
    return it->second;
}
