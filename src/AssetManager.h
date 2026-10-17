#pragma once

#include <map>
#include <string>
#include "TextureManager.h"
#include "Vector2D.h"
#include "ECS.h"
#include <SDL_ttf.h>
#include <SDL_mixer.h>

class AssetManager
{
public:
	AssetManager(Manager* man);
	~AssetManager();

	//gameobjects

	void CreateProjectile(Vector2D pos, Vector2D vel, int range, int speed, std::string id);

	//texture management
	void AddTexture(std::string id, const char* path);
	void AddTexture(std::string id, SDL_Texture* tex);   // already created (Depth::bake)
	SDL_Texture* GetTexture(std::string id);

	void AddFont(std::string id, std::string path, int fontSize);
	TTF_Font* GetFont(std::string id);

	void AddSound(std::string id, const char *path);
	Mix_Chunk* GetSound(std::string id);


private:

	Manager* manager;
	std::map<std::string, SDL_Texture*> textures;
	std::map<std::string, TTF_Font*> fonts;
	std::map<std::string, Mix_Chunk*> sounds;
};