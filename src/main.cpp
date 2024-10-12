#include "Game.h"

Game *game = nullptr;

float gameTime;
float lastFrameTime;

int main(int argc, char *argv[]) {

    const int FPS = 60;
    const int frameDelay = 1000 / FPS;


    Uint32 gameStart;
    Uint32 gameTimee;
    Uint32 frameStart;
    Uint32 frameTime;


    game = new Game();
    game->init("GameWindow", 1350, 850, false);

    gameStart = SDL_GetTicks();
    while (game->running()) {
        frameStart = SDL_GetTicks();

        game->handleEvents();
        game->update();
        game->clearEventProcessing();
        game->render();

        frameTime = SDL_GetTicks() - frameStart;

        if (frameDelay > frameTime) {
            SDL_Delay(frameDelay - frameTime);
        }

        frameTime = SDL_GetTicks() - frameStart;
        gameTimee = SDL_GetTicks() - gameStart;

        gameTime = static_cast<float>(gameTimee/1000.0);
        lastFrameTime = static_cast<float>(frameTime / 1000.0);
    }

    game->clean();
    return 0;
}