#include "Game.h"
#include "Config.h"
#include <memory>

Game *game = nullptr;

float gameTime = 0.f;
float lastFrameTime = 1.f / Config::FPS;

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    game = new Game();
    game->init("DungeonRush", Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT, false);

    Uint32 gameStart = SDL_GetTicks();
    while (game->running()) {
        Uint32 frameStart = SDL_GetTicks();

        game->handleEvents();
        game->update();
        game->render();

        Uint32 frameTimeMs = SDL_GetTicks() - frameStart;
        if (Config::FRAME_DELAY_MS > frameTimeMs) {
            SDL_Delay(Config::FRAME_DELAY_MS - frameTimeMs);
            frameTimeMs = SDL_GetTicks() - frameStart;
        }
        lastFrameTime = static_cast<float>(frameTimeMs) / 1000.0f;
        // clamp delta to avoid huge jumps when debugging
        if (lastFrameTime > 0.05f) lastFrameTime = 0.05f;
        if (lastFrameTime < 0.001f) lastFrameTime = 1.f / Config::FPS;
        gameTime = static_cast<float>(SDL_GetTicks() - gameStart) / 1000.0f;
    }

    game->clean();
    delete game;
    return 0;
}