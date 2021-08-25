#pragma once
#include <string>

class Map 
{
public:
	 
	Map(std::string tID, int ms, int ts);
	~Map();

	void LoadMap(std::string path, int sizeX, int sizeY, int spriteRowSize);
    void LoadColliders(std::string colliderMapPath, std::string colliderMapData, int sizeX, int sizeY);
    void LoadCoins(const char* coinMapPath, int sizeX, int sizeY);
	void AddTile(int srcX, int srcY, int xpos, int ypos);

private:
	std::string texID;
	int mapScale;
	int tileSize;
	int scaledSize;

};
