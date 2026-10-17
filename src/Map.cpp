#include "Map.h"
#include "Game.h"
#include "Level.h"
#include "SpriteSpec.h"
#include <fstream>
#include <cstdlib>

extern Manager manager;

Map::Map(std::string tID, int ts) : m_texID(std::move(tID)), m_tileSize(ts) {
    // `ts` is the atlas cell size and Config::TILE_SIZE is the world cell
    // size.  When they match (platformer.png is a 64px grid and the world
    // tile is 64px) the integer quotient is 1 and tiles draw 1:1 — crisp,
    // no resampling.  A smaller atlas (the old 32px grass_tiles.png) still
    // gets scaled up by an exact integer factor.
    m_scaledSize = ts * (Config::TILE_SIZE / ts);
    if (m_scaledSize == 0) m_scaledSize = Config::TILE_SIZE;
}

void Map::LoadLayer(const std::string& path, int sizeX, int sizeY,
                    int spriteCols, float parallax,
                    Uint8 tintR, Uint8 tintG, Uint8 tintB) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[Map] failed " << path << "\n"; return; }
    std::string line;
    for (int y = 0; y < sizeY && std::getline(f, line); y++) {
        if (line.empty()) { y--; continue; }
        int col = 0;
        std::string tok;
        for (char c : line) {
            if (c == ',' || c == '\n' || c == '\r') {
                int t = tok.empty() ? -11 : std::atoi(tok.c_str());
                tok.clear();
                if (t != -11) {
                    int srcX = (t % spriteCols) * m_tileSize;
                    int srcY = (t / spriteCols) * m_tileSize;
                    auto& tile = manager.addEntity("tile");
                    tile.addComponent<TileComponent>(srcX, srcY,
                        col * m_scaledSize, y * m_scaledSize,
                        m_tileSize, m_scaledSize / m_tileSize,
                        m_texID, parallax, tintR, tintG, tintB);
                    tile.addGroup(Game::groupMap);
                }
                col++;
                // A malformed row must not spill past the map width and
                // quietly shift every tile after it.
                if (col >= sizeX) break;
                if (c != ',') break;
            } else {
                tok.push_back(c);
            }
        }
    }
}

void Map::LoadColliders(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[Map] failed " << path << "\n"; return; }
    int x, y, w, h;
    while (f >> x >> y >> w >> h) {
        auto& e = manager.addEntity("terrain");
        e.addComponent<ColliderComponent>("terrain",
            x * m_scaledSize, y * m_scaledSize,
            w * m_scaledSize, h * m_scaledSize);
        auto& col = e.getComponent<ColliderComponent>();
        col.solid = true;
        // Seed the entity's transform with the collider's world rect.  Without
        // this, ColliderComponent::update() round-trips the collider through
        // a default-at-(0,0) transform every frame, collapsing every world
        // solid onto a single point at the origin and the player falls
        // through the floor.
        auto& tr = *col.transform;
        tr.position.x = (float)col.collider.x;
        tr.position.y = (float)col.collider.y;
        tr.width      = col.collider.w;
        tr.height     = col.collider.h;
        e.addGroup(Game::groupColliders);
    }
}

void Map::LoadCoins(const std::string& path, int sizeX, int sizeY) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[Map] failed " << path << "\n"; return; }
    std::string line;
    for (int y = 0; y < sizeY && std::getline(f, line); y++) {
        if (line.empty()) { y--; continue; }
        int col = 0;
        std::string tok;
        for (char c : line) {
            if (c == ',' || c == '\n' || c == '\r') {
                int v = tok.empty() ? 0 : std::atoi(tok.c_str());
                tok.clear();
                if (v == 1) {
                    auto& coin = manager.addEntity("coin");
                    // centred in its tile, sized by the spec table
                    const int cw = Sprites::COIN.cw, ch = Sprites::COIN.ch;
                    float cx = col * (float)m_scaledSize
                             + (m_scaledSize - cw) / 2.f;
                    float cy = y * (float)m_scaledSize
                             + (m_scaledSize - ch) / 2.f;
                    coin.addComponent<TransformComponent>(cx, cy, cw, ch, 1.f);
                    auto& sp = coin.addComponent<SpriteComponent>(
                        Sprites::COIN.tex, Sprites::COIN.fw, Sprites::COIN.fh,
                        Sprites::COIN.scale, true);
                    Sprites::apply(sp, Sprites::COIN);
                    sp.addAnim("spin", Animation(0, 8, 100));
                    sp.Play("spin");
                    coin.addComponent<ColliderComponent>("coin", cw, ch);
                    coin.getComponent<ColliderComponent>().sensor = true;
                    coin.getComponent<ColliderComponent>().solid  = false;
                    coin.addGroup(Game::groupCoins);
                }
                col++;
                if (col >= sizeX) break;
                if (c != ',') break;
            } else {
                tok.push_back(c);
            }
        }
    }
}
