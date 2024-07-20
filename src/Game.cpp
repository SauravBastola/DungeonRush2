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

extern float lastFrameTime;

Map *map;
Manager manager;

SDL_Renderer *Game::renderer = nullptr;
SDL_Event Game::event;

SDL_Rect Game::camera = {0, 0, 13050, 2030};

Mix_Music *background = nullptr;

AssetManager *Game::assets = new AssetManager(&manager);

bool Game::isRunning = false;

auto& player(manager.addEntity("player"));
auto& enemySkeleton(manager.addEntity("enemySkeleton"));
auto& healthBarDecoration(manager.addEntity("healthBarStuff"));
auto& healthBar(manager.addEntity("healthBarStuff"));
auto& label(manager.addEntity("label"));
auto& scoreLabel(manager.addEntity("scoreLabel"));

/* Game development extras */
auto& fpsdisplay(manager.addEntity("FPS"));

Game::Game() {
    eventProcessTime["w"] = 0.88;
    currentEvents["w"] = false;
    eventProcessTime["q"] = 0.88;
    currentEvents["q"] = false;
    eventProcessTime["e"] = 0.88;
    currentEvents["e"] = false;
    eventProcessTime["f"] = 0.88;
    currentEvents["f"] = false;

    currentEvents["qUp"] = true;
    currentEvents["eUp"] = true;
    currentEvents["fUp"] = true;

    currentEvents["handlingEnemyHealth"] = false;
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

    assets->AddTexture("platformer", "../assets/tileset/platformer.png");
    assets->AddTexture("player", "../assets/sprites/player_sprites.png");
    assets->AddTexture("enemySkeleton", "../assets/sprites/skeleton_sprites.png");
    assets->AddTexture("coins", "../assets/tileset/coins.png");
    assets->AddTexture("projectile", "../assets/sprites/proj.png");
    assets->AddTexture("healthBarDecoration", "../assets/sprites/health_bar_decoration.png");
    assets->AddTexture("healthBar", "../assets/sprites/health_bar.png");

    assets->AddTexture("rect", "../assets/sprites/ColTex.png");

    assets->AddFont("arial", "../assets/fonts/arial.ttf", 16);
    assets->AddFont("firaCode16", "../assets/fonts/FiraCode-Regular.ttf", 16);
    assets->AddFont("firaCode32", "../assets/fonts/FiraCode-Regular.ttf", 32);

    assets->AddSound("coinCollect", "../assets/sounds/coinCollect.wav");
    assets->AddSound("playerJump", "../assets/sounds/playerJump.wav");

    background = Mix_LoadMUS("../assets/sounds/background_music.wav");
    Mix_PlayMusic(background, -1);

    map = new Map("platformer", 3, 32);
    map->LoadMap("../assets/tileset/BackgroundLayer.map", 150, 30, 18);
    map->LoadMap("../assets/tileset/ForegroundLayer.map", 150, 30, 18);
    map->LoadMap("../assets/tileset/DecorLayer.map", 150, 30, 18);
    map->LoadColliders("../assets/tileset/Colliders.map",
                       "../assets/tileset/ColliderData.txt", 150, 30);
    map->LoadCoins("../assets/tileset/CoinLayer.map", 150, 30);

    player.addComponent<TransformComponent>(10.0f, 1300.0f, 50, 37, 3);
    player.addComponent<RigidBodyComponent>();
    player.addComponent<SpriteComponent>("player", true);
    player.addComponent<ColliderComponent>("player", 22, 37);
    player.addGroup(groupPlayers);

    enemySkeleton.addComponent<TransformComponent>(200.0f, 1300.0f, 150, 150, 1.8);
    enemySkeleton.addComponent<RigidBodyComponent>();
    enemySkeleton.addComponent<SpriteComponent>("enemySkeleton", true);
    enemySkeleton.addComponent<ColliderComponent>("enemySkeleton", 50, 50);
    enemySkeleton.addComponent<ScriptComponent>("enemy", player);
    enemySkeleton.addGroup(groupEnemies);

    healthBarDecoration.addComponent<TransformComponent>(15, 15, 64, 17, 4);
    healthBarDecoration.addComponent<SpriteComponent>("healthBarDecoration", false);
    healthBarDecoration.addGroup(groupHealth);

    healthBar.addComponent<TransformComponent>(70, 15, 49, 17, 4);
    healthBar.addComponent<SpriteComponent>("healthBar", false);
    healthBar.addGroup(groupHealth);

    SDL_Color white = {255, 255, 255, 255};

    label.addComponent<UILabel>(1070, 10, "Test String", "arial", white);
    label.addGroup(groupLabels);

    scoreLabel.addComponent<UILabel>(665, 10, std::to_string(gameScore), "firaCode32", white);
    scoreLabel.addGroup(groupLabels);

    fpsdisplay.addComponent<UILabel>(1100, 30, "FPS:", "firaCode16", white);
    fpsdisplay.addGroup(groupLabels);

    assets->CreateProjectile(Vector2D(200, 1600), Vector2D(2 * 300, 0), 2000, 20, "projectile");
    assets->CreateProjectile(Vector2D(300, 1620), Vector2D(2 * 300, 0), 2000, 20, "projectile");
    assets->CreateProjectile(Vector2D(400, 1580), Vector2D(2 * 300, 1 * 300), 2000, 20, "projectile");
    assets->CreateProjectile(Vector2D(350, 1600), Vector2D(2 * 300, -1 * 300), 2000, 20, "projectile");

}

auto& tiles(manager.getGroup(Game::groupMap));
auto& players(manager.getGroup(Game::groupPlayers));
auto& enemies(manager.getGroup(Game::groupEnemies));
auto& colliders(manager.getGroup(Game::groupColliders));
auto& coins(manager.getGroup(Game::groupCoins));
auto& projectiles(manager.getGroup(Game::groupProjectiles));
auto& healthStuff(manager.getGroup(Game::groupHealth));
auto& labels(manager.getGroup(Game::groupLabels));

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
                    case SDLK_q:
                        if (currentEvents["q"] == false && currentEvents["e"] == false && currentEvents["f"] == false
                            && currentEvents["qUp"]) {
                            player.getComponent<ColliderComponent>().colliderWidth += 12;
                            eventInitTime["q"] = SDL_GetTicks();
                            currentEvents["q"] = true;
                            currentEvents["qUp"] = false;
                            currentEvents["handlingEnemyHealth"] = true;
//                            Mix_PlayChannel(-1, assets->GetSound("playerAttack"), 0);
                        }
                        break;
                    case SDLK_e:
                        if (currentEvents["e"] == false && currentEvents["q"] == false && currentEvents["f"] == false
                            && currentEvents["eUp"]) {
                            player.getComponent<ColliderComponent>().colliderWidth += 12;
                            eventInitTime["e"] = SDL_GetTicks();
                            currentEvents["e"] = true;
                            currentEvents["eUp"] = false;
                            currentEvents["handlingEnemyHealth"] = true;
//                            Mix_PlayChannel(-1, assets->GetSound("playerAttack"), 0);
                        }
                        break;
                    case SDLK_f:
                        if (currentEvents["f"] == false && currentEvents["e"] == false && currentEvents["q"] == false
                            && currentEvents["fUp"]) {
                            player.getComponent<ColliderComponent>().colliderWidth += 12;
                            eventInitTime["f"] = SDL_GetTicks();
                            currentEvents["f"] = true;
                            currentEvents["fUp"] = false;
                            currentEvents["handlingEnemyHealth"] = true;
//                            Mix_PlayChannel(-1, assets->GetSound("playerAttack"), 0);
                        }
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
                    case SDLK_q:
                        if (currentEvents["q"] && !currentEvents["qUp"]) {
                            player.getComponent<ColliderComponent>().colliderWidth -= 12;
                        }
                        currentEvents["qUp"] = true;
                        break;
                    case SDLK_e:
                        if (currentEvents["e"] && !currentEvents["eUp"]) {
                            player.getComponent<ColliderComponent>().colliderWidth -= 12;
                        }
                        currentEvents["eUp"] = true;
                        break;
                    case SDLK_f:
                        if (currentEvents["f"] && !currentEvents["fUp"]) {
                            player.getComponent<ColliderComponent>().colliderWidth -= 12;
                        }
                        currentEvents["fUp"] = true;
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

//    if ((SDL_GetTicks() - healthTicks) / 1000.0 > 0.1) {
//        healthTicks = SDL_GetTicks();
//        player.getComponent<TransformComponent>().health -= 0.1;
//    }

    SDL_Rect playerCol = player.getComponent<ColliderComponent>().collider;
    sb::Rect player_col;
    player_col.pos = {static_cast<float>(playerCol.x), static_cast<float>(playerCol.y)};
    player_col.size = {static_cast<float>(playerCol.w), static_cast<float>(playerCol.h)};

    Vector2D playerPos = player.getComponent<TransformComponent>().position;

    std::stringstream ss;
    ss << "Player position: " << playerPos;
    label.getComponent<UILabel>().SetLabelText(ss.str(), "firaCode16");

    fpsdisplay.getComponent<UILabel>().SetLabelText("FPS::" + std::to_string(static_cast<int>( 60 * 0.016 / lastFrameTime)), "firaCode32");

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

    for (auto& c: colliders) {
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

    std::sort(z.begin(), z.end(), [](const std::pair<sb::Rect, float>& a, const std::pair<sb::Rect, float>& b) {
        return a.second < b.second;
    });

    for (auto& j: z) {
        if (Collision::DynamicRectVsRect(player_col, vel, j.first, contactPoint, contactNormal, contactTime,
                                         lastFrameTime)) {
            player.getComponent<TransformComponent>().velocity +=
                Vector2D(std::abs(player.getComponent<TransformComponent>().velocity.x),
                         std::abs(player.getComponent<TransformComponent>().velocity.y)) * contactNormal *
                    (1 - contactTime);
        }
    }

    for (auto& c: coins) {
        if (Collision::AABB(playerCol, c->getComponent<ColliderComponent>().collider)) {
            gameScore += 10;
            c->destroy();
            Mix_PlayChannel(-1, assets->GetSound("coinCollect"), 0);
        }
    }

    for (auto& p: projectiles) {
        if (Collision::AABB(player.getComponent<ColliderComponent>().collider,
                            p->getComponent<ColliderComponent>().collider)) {
            std::cout << "Hit player!" << std::endl;
            p->destroy();
        }
    }

    for (auto& e: enemies) {
        SDL_Rect playerCol = e->getComponent<ColliderComponent>().collider;
        sb::Rect player_col;
        player_col.pos = {static_cast<float>(playerCol.x), static_cast<float>(playerCol.y)};
        player_col.size = {static_cast<float>(playerCol.w), static_cast<float>(playerCol.h)};

        Vector2D contactPoint = {0, 0}, contactNormal = {0, 0};
        float contactTime = 0;

        Vector2D vel = e->getComponent<TransformComponent>().velocity;

        std::vector<std::pair<sb::Rect, float>> z;

        for (auto& c: colliders) {
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

        std::sort(z.begin(), z.end(), [](const std::pair<sb::Rect, float>& a, const std::pair<sb::Rect, float>& b) {
            return a.second < b.second;
        });

        for (auto& j: z) {
            if (Collision::DynamicRectVsRect(player_col, vel, j.first, contactPoint, contactNormal, contactTime,
                                             lastFrameTime)) {
                e->getComponent<TransformComponent>().velocity +=
                    Vector2D(std::abs(e->getComponent<TransformComponent>().velocity.x),
                             std::abs(e->getComponent<TransformComponent>().velocity.y)) * contactNormal *
                        (1 - contactTime);
            }
        }
    }

    /* Collision with player */

    for (auto& e: enemies) {
        if (Collision::AABB(playerCol, e->getComponent<ColliderComponent>().collider)) {
            e->getComponent<SpriteComponent>().isOverridden = true;
            e->getComponent<SpriteComponent>().Play((e->tag + "Attack").c_str());
            if (!e->getComponent<ScriptComponent>().isHandlingAction) {
                e->getComponent<ScriptComponent>().actionInitTime = SDL_GetTicks();
                e->getComponent<ScriptComponent>().isHandlingAction = true;
            }
            if ((SDL_GetTicks() - e->getComponent<ScriptComponent>().actionInitTime) / 1000.0 > 1.0) {
                if (Collision::AABB(playerCol, e->getComponent<ColliderComponent>().collider) &&
                    e->getComponent<ScriptComponent>().isHandlingAction)
                    player.getComponent<TransformComponent>().health -= 5;
                e->getComponent<ScriptComponent>().isHandlingAction = false;
            }
        } else {
            e->getComponent<SpriteComponent>().isOverridden = false;
        }

        if((currentEvents["q"] || currentEvents["e"] || currentEvents["f"]) && currentEvents["handlingEnemyHealth"]) {
            e->getComponent<TransformComponent>().health -= 10;
            currentEvents["handlingEnemyHealth"] = false;
            std::cout << "Enemy Health :: " << e->getComponent<TransformComponent>().health << std::endl;
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
    for (auto& cEvent: currentEvents) {
        if (cEvent.second) {
            Uint32 currTicks = SDL_GetTicks();
            float time = (currTicks - eventInitTime[cEvent.first]) / 1000.0;
            if (cEvent.first == "w") {
                if (time < 0.48) {
                    player.getComponent<SpriteComponent>().isOverridden = true;
                    player.getComponent<SpriteComponent>().Play("playerJump");
                } else {
                    player.getComponent<SpriteComponent>().isOverridden = false;
                }
                if (time > eventProcessTime[cEvent.first]) {
                    currentEvents["w"] = false;
                }
            } else if (cEvent.first == "q") {
                if (time < 0.4) {
                    player.getComponent<SpriteComponent>().isOverridden = true;
                    player.getComponent<SpriteComponent>().Play("playerAttack1");
                } else {
                    cEvent.second = false;
                    player.getComponent<SpriteComponent>().isOverridden = false;
                    if(currentEvents["qUp"] == false) {
                        player.getComponent<ColliderComponent>().colliderWidth -= 12;
                    }
                }
            } else if (cEvent.first == "e") {
                if (time < 0.4) {
                    player.getComponent<SpriteComponent>().isOverridden = true;
                    player.getComponent<SpriteComponent>().Play("playerAttack2");
                } else {
                    cEvent.second = false;
                    player.getComponent<SpriteComponent>().isOverridden = false;
                    if(currentEvents["eUp"] == false) {
                        player.getComponent<ColliderComponent>().colliderWidth -= 12;
                    }
                }
            } else if (cEvent.first == "f") {
                if (time < 0.4) {
                    player.getComponent<SpriteComponent>().isOverridden = true;
                    player.getComponent<SpriteComponent>().Play("playerAttack3");
                } else {
                    cEvent.second = false;
                    player.getComponent<SpriteComponent>().isOverridden = false;
                    if(currentEvents["fUp"] == false) {
                        player.getComponent<ColliderComponent>().colliderWidth -= 12;
                    }
                }
            }
        }
    }
}

void Game::render() {
    SDL_RenderClear(renderer);
    for (auto& t: tiles) {
        t->draw();
    }

    for (auto& c: colliders) {
        c->draw();
    }

    for (auto& e: enemies) {
        e->draw();
    }

    for (auto& p: players) {
        p->draw();
    }

    for (auto& c: coins) {
        c->draw();
    }

    for (auto& p: projectiles) {
        p->draw();
    }

    for (auto& h: healthStuff) {
        h->draw();
    }

    for (auto& l: labels) {
        l->draw();
    }

    SDL_RenderPresent(renderer);
}

void Game::clean() {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
}