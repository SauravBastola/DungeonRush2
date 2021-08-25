#include "Map.h"
#include "Game.h"
#include <fstream>
#include "ECS.h"
#include "Components.h"

extern Manager manager;

Map::Map(std::string tID, int ms, int ts) : texID(tID), mapScale(ms), tileSize(ts)
{
	scaledSize = ms * ts;
}

Map::~Map()
{
}

void Map::LoadMap(std::string path, int sizeX, int sizeY, int spriteRowSize)
{
	char c;
	std::fstream mapFile;
	mapFile.open(path);

	int srcX, srcY;

	for (int y = 0; y < sizeY; y++)
	{
		for (int x = 0; x < sizeX; x++)
		{
		    std::string sTileNumber;
			mapFile.get(c);
			sTileNumber += c;
			mapFile.get(c);
            sTileNumber += c;
            mapFile.get(c);
            sTileNumber += c;
            int tileNumber = atoi(sTileNumber.c_str());
            if(tileNumber != -11) {
                srcX = (tileNumber % spriteRowSize) * tileSize;
                srcY = (tileNumber / spriteRowSize) * tileSize;
                AddTile(srcX, srcY, x * scaledSize, y * scaledSize);
            }
			mapFile.ignore();
		}
	}

	mapFile.close();

}

void Map::LoadColliders(std::string colliderMapPath, std::string colliderMapDataPath, int sizeX, int sizeY) {
    char c;
    std::fstream  colFile;
    std::fstream  colData;
    colFile.open(colliderMapPath);
    colData.open(colliderMapDataPath);

    for (int y = 0; y < sizeY; y++)
    {
        for (int x = 0; x < sizeX; x++)
        {
            colFile.get(c);
            if (c == '1')
            {
                int xSize, ySize;
                std::string val;
                colData.get(c);
                val += c;
                colData.get(c);
                val+=c;
                xSize = atoi(val.c_str());
                val = "";
                colData.ignore();
                colData.get(c);
                val += c;
                colData.get(c);
                val += c;
                ySize = atoi(val.c_str());
                colData.ignore();

                auto& tcol(manager.addEntity("tileCollider"));
                tcol.addComponent<ColliderComponent>("terrain", x * scaledSize , y * scaledSize , scaledSize * xSize, scaledSize * ySize);
                tcol.addGroup(Game::groupColliders);
            }
            colFile.ignore();
        }
    }

    colData.close();
    colFile.close();
}

void Map::AddTile(int srcX, int srcY, int xpos, int ypos)
{
	auto& tile(manager.addEntity("tile"));
	tile.addComponent<TileComponent>(srcX, srcY, xpos, ypos, tileSize, mapScale, texID);
	tile.addGroup(Game::groupMap);
}