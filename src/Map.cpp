#include "Map.h"
#include "Game.h"
#include <fstream>
#include "ECS.h"
#include "Components.h"

extern Manager manager;
extern Entity player;

Map::Map(std::string tID, int ms, int ts) : texID(tID), mapScale(ms), tileSize(ts) {
    scaledSize = ms * ts;
}

Map::~Map() {
}

void Map::LoadMap(std::string path, int sizeX, int sizeY, int spriteRowSize) {
    char c;
    std::fstream mapFile;
    mapFile.open(path);

    int srcX, srcY;

    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            std::string sTileNumber;
            while (mapFile.get(c) && c != ',' && c != '\n') {
                sTileNumber += c;
            }
            int tileNumber = atoi(sTileNumber.c_str());
            if (tileNumber != -11) {
                srcX = (tileNumber % spriteRowSize) * tileSize;
                srcY = (tileNumber / spriteRowSize) * tileSize;
                AddTile(srcX, srcY, x * scaledSize, y * scaledSize);
            }
        }
    }

    mapFile.close();

}

void Map::LoadColliders(std::string colliderMapPath, std::string colliderMapDataPath, int sizeX, int sizeY) {
    char c;
    std::fstream colFile;
    std::fstream colData;
    colFile.open(colliderMapPath);
    colData.open(colliderMapDataPath);

    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            colFile.get(c);
            if (c == '1') {
                int xSize, ySize;
                std::string val;
                while (colData.get(c) && c != ',' && c != '\n') {
                    val += c;
                }
                xSize = atoi(val.c_str());
                val = "";
                while (colData.get(c) && c != ',' && c != '\n') {
                    val += c;
                }
                ySize = atoi(val.c_str());

                auto &tcol(manager.addEntity("tileCollider"));
                tcol.addComponent<ColliderComponent>("terrain", x * scaledSize, y * scaledSize, scaledSize * xSize,
                                                     scaledSize * ySize);
                tcol.addGroup(Game::groupColliders);
            }
            colFile.ignore();
        }
    }

    colData.close();
    colFile.close();
}


void Map::LoadCoins(const char* coinMapPath, int sizeX, int sizeY) {
    std::fstream cFile;
    cFile.open(coinMapPath);
    char c;
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            std::string val;
            while (cFile.get(c)) {
                if(c!=',' && c!='\n') {
                    val += c;
                } else {
                    break;
                }
            }
            int v = atoi(val.c_str());
            if (v == 1) {
                auto &coin(manager.addEntity("coin"));
                coin.addComponent<TransformComponent>(x * scaledSize, y * scaledSize, 61, 62, 1.2);
                coin.addComponent<SpriteComponent>("coins", true);
                coin.addComponent<ColliderComponent>("coin");
                coin.addComponent<ScriptComponent>("coin", player);
                coin.addGroup(Game::groupCoins);
            }
        }
    }
}

void Map::AddTile(int srcX, int srcY, int xpos, int ypos) {
    auto &tile(manager.addEntity("tile"));
    tile.addComponent<TileComponent>(srcX, srcY, xpos, ypos, tileSize, mapScale, texID);
    tile.addGroup(Game::groupMap);
}

