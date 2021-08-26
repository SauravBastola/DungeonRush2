#include "Game.h"
#include "TextureManager.h"
#include "Map.h"
#include "Components.h"
#include "Vector2D.h"
#include "Collision.h"
#include "AssetManager.h"
#include <sstream>

#include <SDL_mixer.h>
#include <fstream>

bool main_flag = true;

extern float lastFrameTime;

Uint32 healthInitTime = 0;

Map *map;
Manager manager;

SDL_Renderer *Game::renderer = nullptr;
SDL_Event Game::event;

SDL_Rect Game::camera = {0, 0, 13050, 2030};

Mix_Music *background = nullptr;

AssetManager *Game::assets = new AssetManager(&manager);

bool Game::isRunning = false;

auto &player(manager.addEntity("player"));
auto &healthBarDecoration(manager.addEntity("healthBarStuff"));
auto &healthBar(manager.addEntity("healthBarStuff"));
auto &label(manager.addEntity("label"));
auto &scoreLabel(manager.addEntity("scoreLabel"));

Game::Game() {
    eventProcessTime["w"] = 0.88;
    currentEvents["w"] = false;
}

Game::~Game() {}

void Game::init(const char *title, int width, int height, bool fullscreen) {
    int flags = 0;

    if (fullscreen) {
        flags = SDL_WINDOW_FULLSCREEN;
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
        window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
        renderer = SDL_CreateRenderer(window, -1, 0);
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255);
        }

        isRunning = true;

    }

    if (TTF_Init() == -1) {
        std::cout << "Error : SDL_TTF" << std::endl;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
        std::cout << "Error : Mix_OpenAudio" << std::endl;
    }

    assets->AddTexture("platformer", "assets/tileset/platformer.png");
    assets->AddTexture("player", "assets/player_sprites.png");
    assets->AddTexture("coins", "assets/tileset/coins.png");
    assets->AddTexture("projectile", "assets/proj.png");
    assets->AddTexture("healthBarDecoration", "assets/health_bar_decoration.png");
    assets->AddTexture("healthBar", "assets/health_bar.png");

    assets->AddTexture("rect", "assets/ColTex.png");

    assets->AddFont("arial", "assets/arial.ttf", 16);
    assets->AddFont("firaCode16", "assets/FiraCode-Regular.ttf", 16);
    assets->AddFont("firaCode32", "assets/FiraCode-Regular.ttf", 32);

    assets->AddSound("coinCollect", "assets/coinCollect.wav");
    assets->AddSound("playerJump", "assets/playerJump.wav");

    background = Mix_LoadMUS("assets/background_music.wav");
    Mix_PlayMusic(background, -1);


    map = new Map("platformer", 3, 32);
    map->LoadMap("assets/tileset/BackgroundLayer.map", 150, 30, 18);
    map->LoadMap("assets/tileset/ForegroundLayer.map", 150, 30, 18);
    map->LoadMap("assets/tileset/DecorLayer.map", 150, 30, 18);
    map->LoadColliders("assets/tileset/Colliders.map",
                       "assets/tileset/ColliderData.txt", 150, 30);
    map->LoadCoins("assets/tileset/CoinLayer.map", 150, 30);

    player.addComponent<TransformComponent>(10.0f, 1300.0f, 50, 37, 3);
    player.addComponent<RigidBodyComponent>();
    player.addComponent<SpriteComponent>("player", true);
    player.addComponent<ColliderComponent>("player");
    player.addGroup(groupPlayers);


    healthBarDecoration.addComponent<TransformComponent>(15, 15, 64, 17, 4);
    healthBarDecoration.addComponent<SpriteComponent>("healthBarDecoration", false);
    healthBarDecoration.addGroup(groupHealth);

    healthBar.addComponent<TransformComponent>(70, 15, 49, 17, 4);
    healthBar.addComponent<SpriteComponent>("healthBar", false);
    healthBar.addGroup(groupHealth);


    SDL_Color white = {255, 255, 255, 255};

    label.addComponent<UILabel>(1060, 10, "Test String", "arial", white);
    label.addGroup(groupLabels);

    scoreLabel.addComponent<UILabel>(665, 10, std::to_string(gameScore), "firaCode32", white);
    scoreLabel.addGroup(groupLabels);

    assets->CreateProjectile(Vector2D(200, 1600), Vector2D(2 * 300, 0), 2000, 20, "projectile");
    assets->CreateProjectile(Vector2D(300, 1620), Vector2D(2 * 300, 0), 2000, 20, "projectile");
    assets->CreateProjectile(Vector2D(400, 1580), Vector2D(2 * 300, 1 * 300), 2000, 20, "projectile");
    assets->CreateProjectile(Vector2D(350, 1600), Vector2D(2 * 300, -1 * 300), 2000, 20, "projectile");

}

auto &tiles(manager.getGroup(Game::groupMap));
auto &players(manager.getGroup(Game::groupPlayers));
auto &colliders(manager.getGroup(Game::groupColliders));
auto &coins(manager.getGroup(Game::groupCoins));
auto &projectiles(manager.getGroup(Game::groupProjectiles));
auto &healthStuff(manager.getGroup(Game::groupHealth));
auto &labels(manager.getGroup(Game::groupLabels));

void Game::handleEvents() {

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT :
                isRunning = false;
                break;
            case SDL_KEYDOWN :
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                        if (currentEvents["w"] == false) {
                            player.getComponent<TransformComponent>().velocity.y = -1400;
                            eventInitTime["w"] = SDL_GetTicks();
                            currentEvents["w"] = true;
                            Mix_PlayChannel(-1, assets->GetSound("playerJump"), 0);
                        }
                        break;
                    case SDLK_a:
                        player.getComponent<TransformComponent>().velocity.x = -450;
                        player.getComponent<SpriteComponent>().spriteFlip = SDL_FLIP_HORIZONTAL;
                        break;
                    case SDLK_d:
                        player.getComponent<TransformComponent>().velocity.x = 450;
                        player.getComponent<SpriteComponent>().spriteFlip = SDL_FLIP_NONE;
                        break;
                    case SDLK_s:
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                        break;
                    case SDLK_a:
                        player.getComponent<TransformComponent>().velocity.x = 0;
                        break;
                    case SDLK_d:
                        player.getComponent<TransformComponent>().velocity.x = 0;
                        break;
                    case SDLK_s:
                        break;
                    case SDLK_ESCAPE:
                        Game::isRunning = false;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
}


void Game::update() {


    if((SDL_GetTicks() - healthInitTime)/ 1000.0 > 0.1) {
        player.getComponent<TransformComponent>().health -= 0.1;
        healthInitTime = SDL_GetTicks();
    }
    SDL_Rect playerCol = player.getComponent<ColliderComponent>().collider;
    sb::Rect player_col;
    player_col.pos = {static_cast<float>(playerCol.x), static_cast<float>(playerCol.y)};
    player_col.size = {static_cast<float>(playerCol.w), static_cast<float>(playerCol.h)};

    Vector2D playerPos = player.getComponent<TransformComponent>().position;

    std::stringstream ss;
    ss << "Player position: " << playerPos;
    label.getComponent<UILabel>().SetLabelText(ss.str(), "firaCode16");

    int scoreLabelPosOffset = 0;
    if (gameScore < 10) {
        scoreLabelPosOffset = 10;
    } else if (gameScore < 100) {
        scoreLabelPosOffset = 20;
    } else if (gameScore < 1000) {
        scoreLabelPosOffset = 30;
    }

    scoreLabel.getComponent<UILabel>().position.x = 675 - scoreLabelPosOffset;
    scoreLabel.getComponent<UILabel>().SetLabelText(std::to_string(gameScore), "firaCode32");

    Vector2D contactPoint = {0, 0}, contactNormal = {0, 0};
    float contactTime = 0;

    Vector2D vel = player.getComponent<TransformComponent>().velocity;

    std::vector<std::pair<sb::Rect, float>> z;

    for (auto &c : colliders) {
        SDL_Rect cCol = c->getComponent<ColliderComponent>().collider;
        std::string tag = c->getComponent<ColliderComponent>().tag;

        sb::Rect c_col;
        c_col.pos = {static_cast<float>(cCol.x), static_cast<float>(cCol.y)};
        c_col.size = {static_cast<float>(cCol.w), static_cast<float>(cCol.h)};

        if (Collision::DynamicRectVsRect(player_col, vel, c_col, contactPoint, contactNormal, contactTime,
                                         lastFrameTime)) {
            z.push_back({c_col, contactTime});

        }
    }


    std::sort(z.begin(), z.end(), [](const std::pair<sb::Rect, float> &a, const std::pair<sb::Rect, float> &b) {
        return a.second < b.second;
    });

    for (auto &j:z) {
        if (Collision::DynamicRectVsRect(player_col, vel, j.first, contactPoint, contactNormal, contactTime,
                                         lastFrameTime)) {
            player.getComponent<TransformComponent>().velocity +=
                    Vector2D(std::abs(player.getComponent<TransformComponent>().velocity.x),
                             std::abs(player.getComponent<TransformComponent>().velocity.y)) * contactNormal *
                    (1 - contactTime);
        }
    }

    for (auto &c : coins) {
        if (Collision::AABB(playerCol, c->getComponent<ColliderComponent>().collider)) {
            gameScore += 10;
            c->destroy();
            Mix_PlayChannel(-1, assets->GetSound("coinCollect"), 0);
        }
    }


    for (auto &p : projectiles) {
        if (Collision::AABB(player.getComponent<ColliderComponent>().collider,
                            p->getComponent<ColliderComponent>().collider)) {
            std::cout << "Hit player!" << std::endl;
            p->destroy();
        }
    }

    healthBar.getComponent<TransformComponent>().width = 49 * player.getComponent<TransformComponent>().health / 100.0;
    healthBar.getComponent<TransformComponent>().position.x =
            70 + 10 * (100 - player.getComponent<TransformComponent>().health) / 100.0;

    manager.refresh();
    manager.update();

    camera.x = static_cast<int>(player.getComponent<TransformComponent>().position.x - 660);
    camera.y = static_cast<int>(player.getComponent<TransformComponent>().position.y - 410);

    if (camera.x < 0)
        camera.x = 0;
    if (camera.y < 0)
        camera.y = 0;
    if (camera.x > camera.w)
        camera.x = camera.w;
    if (camera.y > camera.h)
        camera.y = camera.h;
}

void Game::clearEventProcessing() {
    for (auto &cEvent : currentEvents) {
        if (cEvent.second) {
            Uint32 currTicks = SDL_GetTicks();
            float time = (currTicks - eventInitTime[cEvent.first]) / 1000.0;
            if (time < 0.48) {
                player.getComponent<SpriteComponent>().overridden = true;
                player.getComponent<SpriteComponent>().Play("playerJump");
            } else {
                player.getComponent<SpriteComponent>().overridden = false;
            }
            if (time > eventProcessTime[cEvent.first]) {
                cEvent.second = false;
            }
        }
    }


    // Game Ending

    if (player.getComponent<TransformComponent>().position.x > 14000 ||
        player.getComponent<TransformComponent>().health <= 0) {

        if (eflag) {
            eStart = SDL_GetTicks();
            eflag = false;
        }
        if (main_flag && (SDL_GetTicks() - eStart) / 1000.0 > 3) {

            SDL_RenderClear(renderer);

            SDL_Rect srcr = {0, 0, 450, 338}, destr = {0, 0, 1350, 850};
            SDL_Surface *tempSurface = IMG_Load("assets/gameover.jpg");
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, tempSurface);
            SDL_FreeSurface(tempSurface);
            SDL_RenderCopyEx(renderer, tex, &srcr, &destr, NULL, NULL, SDL_FLIP_NONE);
            SDL_RenderPresent(renderer);


            Mix_HaltMusic();
            background = Mix_LoadMUS("assets/gameover.wav");
            Mix_PlayMusic(background, -1);

            main_flag = false;
        }

    }


}

void Game::render() {
    if (main_flag) {
        SDL_RenderClear(renderer);
        for (auto &t : tiles) {
            t->draw();
        }

//    for (auto &c : colliders) {
//        c->draw();
//    }

        for (auto &p : players) {
            p->draw();
        }

        for (auto &c : coins) {
            c->draw();
        }

        for (auto &p : projectiles) {
            p->draw();
        }

        for (auto &h : healthStuff) {
            h->draw();
        }

        for (auto &l : labels) {
            l->draw();
        }
    }

    SDL_RenderPresent(renderer);
}

void Game::clean() {
    Mix_FreeMusic(background);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
}