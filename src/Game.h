#pragma once

#include "SDL.h"
#include "SDL_image.h"
#include <iostream>
#include <vector>
#include <map>

class AssetManager;

class Game
{
public:
	Game();
	~Game();

	void init(const char* title, int width, int height, bool fullscreen);
	void handleEvents();
	void update();
	void clearEventProcessing();
	bool running() { return isRunning; }
	void render();
	void clean();

	int gameScore = 0;
	static SDL_Renderer *renderer;
	static SDL_Event event;
	std::map<std::string, bool> currentEvents;
	std::map<std::string, float> eventProcessTime;
	std::map<std::string, Uint32> eventInitTime;
	static bool isRunning;
	static SDL_Rect camera;
	static AssetManager* assets;

    bool mainFlag = true;
    Uint32 healthTicks = 0;

	enum groupLabels : std::size_t
	{
		groupMap,
		groupPlayers,
		groupEnemies,
		groupColliders,
		groupProjectiles,
		groupHealth,
		groupLabels,
		groupCoins
	};

private:

	SDL_Window *window;
};