#pragma once
#include <string>
#include "ECS.h"
#include "Components.h"

class Map {
public:
    Map(std::string tID, int ts);
    ~Map() = default;

    // Load a visual tile layer.  `scaledSize` is the on-screen size of
    // each cell, equal to the world's TILE constant.
    // `tintR/G/B` colour-modulate the whole layer for depth cueing.
    void LoadLayer(const std::string& path, int sizeX, int sizeY,
                   int spriteCols, float parallax,
                   Uint8 tintR = 255, Uint8 tintG = 255, Uint8 tintB = 255);

    // Load solid terrain rectangles from a `x y w h` per-line text file.
    // Coordinates are in TILE units; each line becomes one AABB collider.
    void LoadColliders(const std::string& path);

    // Load coin cells.  '1' = coin tile, anything else empty.
    void LoadCoins(const std::string& path, int sizeX, int sizeY);

    int scaledSize() const { return m_scaledSize; }
    int tileSize()   const { return m_tileSize; }
    const std::string& texID() const { return m_texID; }

private:
    std::string m_texID;
    int m_tileSize;     // source tile size in pixels
    int m_scaledSize;   // displayed tile size
};
