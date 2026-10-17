#pragma once
#include "ECS.h"
#include "AssetManager.h"
#include "Game.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>

class UILabel : public Component {
public:
    SDL_Rect     pos{0,0,0,0};
    SDL_Texture* tex = nullptr;
    SDL_Color    color{255,255,255,255};
    std::string  text;
    std::string  fontId;

    UILabel() = default;
    UILabel(int x, int y, std::string t, std::string font, SDL_Color col)
        : color(col), text(std::move(t)), fontId(std::move(font))
    {
        pos.x = x; pos.y = y;
    }
    ~UILabel() override { if (tex) SDL_DestroyTexture(tex); }

    void Set(int x, int y, const std::string& t, const std::string& font, SDL_Color col) {
        pos = { x, y, 0, 0 };
        text = t; fontId = font; color = col;
        if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
        if (!Game::renderer || !Game::assets) return;
        TTF_Font* f = Game::assets->GetFont(font);
        if (!f) return;
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, t.c_str(), color);
        if (!s) return;
        tex = SDL_CreateTextureFromSurface(Game::renderer, s);
        SDL_QueryTexture(tex, nullptr, nullptr, &pos.w, &pos.h);
        SDL_FreeSurface(s);
    }

    void SetText(const std::string& t) {
        if (t == text) return;
        text = t;
        if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
        if (!Game::renderer || !Game::assets) return;
        TTF_Font* f = Game::assets->GetFont(fontId);
        if (!f) return;
        SDL_Surface* s = TTF_RenderUTF8_Blended(f, t.c_str(), color);
        if (!s) return;
        tex = SDL_CreateTextureFromSurface(Game::renderer, s);
        SDL_QueryTexture(tex, nullptr, nullptr, &pos.w, &pos.h);
        SDL_FreeSurface(s);
    }

    void draw() override {
        if (!tex) return;
        SDL_RenderCopy(Game::renderer, tex, nullptr, &pos);
    }
};
