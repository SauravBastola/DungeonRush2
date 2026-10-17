#pragma once
// ---------------------------------------------------------------------------
// SpriteComponent
//
// A sprite-sheet frame and an entity's collider are two different things, and
// conflating them is what made every entity in this game render at a random
// scale.  Here they are fully independent:
//
//   frameW / frameH  size of ONE cell inside the source texture.  This strides
//                    the source rect and comes from the art, never from a
//                    gameplay number.
//   scale            uniform multiplier turning a frame into the on-screen
//                    size.  Uniform => aspect ratio is always preserved, so
//                    nothing can ever look squashed or stretched.
//   drawXOff/YOff    where the art sits relative to the collider, so a large
//                    visual (sword swings, wings, a hero 60px wide with a
//                    32px hitbox) can overhang a small hitbox without
//                    changing what actually collides.
//
// The three anchor helpers (anchorFeet / anchorCentre / anchorBox) compute
// those offsets from the collider, so callers never hand-tune them.
// ---------------------------------------------------------------------------
#include "Animation.h"
#include <map>
#include <string>
#include <cmath>
#include "ECS.h"
#include "Config.h"
#include "Components.h"
#include "TransformComponent.h"
#include "TextureManager.h"
#include "AssetManager.h"
#include "Game.h"

extern float gameTime;
extern float lastFrameTime;

class SpriteComponent : public Component {
public:
    TransformComponent* transform = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Rect srcRect{0,0,0,0}, destRect{0,0,0,0};
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    bool  animated  = false;
    int   frames    = 1;
    int   speed     = 100;
    int   frameIdx  = 0;
    int   rowIdx    = 0;
    int   startCol  = 0;
    Uint32 lastStep = 0;

    // Secondary motion, recomputed every update from the animation's position
    // through its loop.  `phase` is the fraction of the way through the
    // CURRENT cell, so motion stays smooth while the picture steps.
    float phase     = 0.f;
    float drawBob   = 0.f;    // pixels to shift the sprite vertically
    float drawAngle = 0.f;    // degrees of lean
    float bobAmp    = 0.f;
    float leanAmp   = 0.f;
    float bobCycles = 2.f;
    float blendFrac = 0.f;

    int   frameW = 0, frameH = 0;   // native cell size inside the sheet
    float scale  = 1.f;             // uniform draw scale
    int   drawXOff = 0, drawYOff = 0;

    bool  flipToFacing = false;     // mirror with transform->facingRight
    bool  visible      = true;
    bool  hudFixed     = false;     // draw in screen space, ignore camera

    float blinkT = 0.f;             // >0 => flicker (damage / spawn)
    float flashT = 0.f;             // >0 => tint (hit flash)

    // Squash & stretch.  Lets a single-frame boss read as though it is
    // moving, planted on its feet rather than floating.
    float squashX = 1.f, squashY = 1.f;

    float alpha = 1.f;              // 0..1, for fades and afterimages

    // Persistent tint.  This must live here rather than being pushed onto the
    // SDL_Texture directly: textures are shared between entities (every ghost
    // reuses the player's sheet), so a raw SDL_SetTextureColorMod from one
    // component would recolour every other entity using that texture too.
    // draw() applies this and restores 255,255,255 inside the same call.
    bool  tintOn = false;
    Uint8 tintR = 255, tintG = 255, tintB = 255;

    std::string currentAnim;
    std::map<std::string, Animation> anims;

    // size of the source texture, so we can never sample outside it
    int texW = 0, texH = 0;

    SpriteComponent() = default;

    SpriteComponent(const std::string& id, int fw, int fh, float sc,
                    bool isAnimated = false)
        : animated(isAnimated), frameW(fw), frameH(fh), scale(sc)
    {
        setTex(id);
        anims["idle"] = Animation(0, 1, 200);
        Play("idle");
    }

    void setTex(const std::string& id) {
        texture = Game::assets ? Game::assets->GetTexture(id) : nullptr;
        queryTex();
    }
    // Registering an animation that is already playing must re-apply it.
    //
    // The constructor pre-registers and Plays a placeholder "idle", so an
    // enemy setup that does `addAnim("idle", ...); Play("idle");` hits
    // Play()'s "same name, nothing to do" early return and keeps the
    // placeholder: one frame, row 0, no motion.  Every enemy in the game was
    // silently playing the wrong idle for this reason -- the real 4-frame
    // idle and its bob/lean were sitting in the map, unreachable.
    void addAnim(const std::string& name, const Animation& a) {
        anims[name] = a;
        if (name == currentAnim) { currentAnim.clear(); Play(name); }
    }

    void flash(float secs = 0.12f) { flashT = secs; }
    void squash(float sx, float sy) { squashX = sx; squashY = sy; }
    void setTint(Uint8 r, Uint8 g, Uint8 b) { tintOn = true; tintR = r; tintG = g; tintB = b; }
    void clearTint() { tintOn = false; tintR = tintG = tintB = 255; }

    void queryTex() {
        texW = texH = 0;
        if (texture) SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
    }

    // Clamp every animation to the cells that actually exist in this sheet.
    // Several sheets in this project have a single row, and sampling row 2 of
    // a one-row texture is what used to make enemies vanish mid-attack.
    void clampToTexture() {
        if (!texture || frameW <= 0 || frameH <= 0) return;
        if (texW == 0) queryTex();
        int maxCols = texW / frameW;
        int maxRows = texH / frameH;
        if (maxCols < 1) maxCols = 1;
        if (maxRows < 1) maxRows = 1;
        if (startCol >= maxCols) startCol = maxCols - 1;
        if (rowIdx   >= maxRows) rowIdx   = maxRows - 1;
        int avail = maxCols - startCol;
        if (avail < 1) avail = 1;
        if (frames > avail) frames = avail;
        if (frameIdx >= frames) frameIdx = 0;
    }

    void Play(const std::string& name) {
        if (name == currentAnim) return;
        auto it = anims.find(name);
        if (it == anims.end()) return;
        currentAnim = name;
        rowIdx   = it->second.index;
        frames   = it->second.frames;
        speed    = it->second.speed;
        startCol = it->second.start;
        frameIdx = 0;
        bobAmp    = it->second.bob;
        leanAmp   = it->second.lean;
        bobCycles = it->second.cycles;
        blendFrac = it->second.blend;
        phase    = 0.f;
        lastStep = SDL_GetTicks();
        clampToTexture();
    }

    // Restart an animation even when it is already the current one.
    void PlayOver(const std::string& name) {
        currentAnim.clear();
        Play(name);
    }

    // ---- anchoring -------------------------------------------------------
    int drawW() const { return (int)std::lround(frameW * scale); }
    int drawH() const { return (int)std::lround(frameH * scale); }

    // Centred on X, sprite's bottom edge on the collider's bottom edge.
    // This is what you want for anything that walks on the ground.
    void anchorFeet() {
        if (!transform) return;
        drawXOff = (transform->width  - drawW()) / 2;
        drawYOff =  transform->height - drawH();
    }
    // Dead centre of the collider — fliers, floating pickups.
    void anchorCentre() {
        if (!transform) return;
        drawXOff = (transform->width  - drawW()) / 2;
        drawYOff = (transform->height - drawH()) / 2;
    }
    // Exactly on top of the collider — props, pickups, hazards.
    void anchorBox() { drawXOff = 0; drawYOff = 0; }

    void init() override {
        transform = &entity->getComponent<TransformComponent>();
        if (frameW == 0) frameW = transform->width;
        if (frameH == 0) frameH = transform->height;
        queryTex();
        srcRect = { 0, 0, frameW, frameH };
        clampToTexture();
        anchorBox();
    }

    void update() override {
        // Advance the cell, and remember how far through the current cell we
        // are.  The old code only did the first half, so every sprite was
        // static for `speed` ms and then jumped -- fine at 30ms, visibly
        // steppy at the 100-150ms these animations use.
        // NOTE: `phase` must keep running even when frames == 1.  A
        // single-cell animation (the boss) has no frame advance to gate on,
        // and gating the phase on it too leaves the loop position pinned at
        // zero -- which silently kills the only motion that sprite has.
        if (animated && speed > 0) {
            Uint32 now = SDL_GetTicks();
            Uint32 elapsed = now - lastStep;
            if (elapsed >= (Uint32)speed) {
                if (frames > 1) frameIdx = (frameIdx + 1) % frames;
                // Carry the remainder rather than snapping to `now`, or a
                // slow frame makes the whole cycle drift out of time.
                lastStep += (elapsed / (Uint32)speed) * (Uint32)speed;
                elapsed   = now - lastStep;
            }
            phase = (float)elapsed / (float)speed;
            if (phase < 0.f) phase = 0.f;
            if (phase > 1.f) phase = 1.f;
        } else {
            phase = 0.f;
        }
        clampToTexture();

        // Secondary motion.  `cyc` is the position through the WHOLE loop
        // (0..1), not the current cell, so a six-frame walk gets two full
        // bobs per cycle -- one per footfall -- instead of six.
        if (bobAmp != 0.f || leanAmp != 0.f) {
            const float cyc = (frames > 0)
                ? ((float)frameIdx + phase) / (float)frames : phase;
            const float w = std::sin(cyc * 6.2831853f * bobCycles);
            drawBob   = bobAmp * w;
            drawAngle = leanAmp * w;
        } else {
            drawBob = 0.f;
            drawAngle = 0.f;
        }

        srcRect.x = (startCol + frameIdx) * frameW;
        srcRect.y = rowIdx * frameH;
        srcRect.w = frameW;
        srcRect.h = frameH;

        // squash & stretch, but keep the feet planted where they were
        int dw = (int)std::lround(drawW() * squashX);
        int dh = (int)std::lround(drawH() * squashY);

        if (hudFixed) {
            destRect.x = (int)(transform->position.x + drawXOff + (drawW() - dw) / 2);
            destRect.y = (int)(transform->position.y + drawYOff + (drawH() - dh));
        } else {
            destRect.x = (int)(transform->position.x + drawXOff - Game::camera.x
                               + (drawW() - dw) / 2);
            destRect.y = (int)(transform->position.y + drawYOff - Game::camera.y
                               + (drawH() - dh));
        }
        destRect.w = dw;
        destRect.h = dh;

        if (flipToFacing)
            flip = transform->facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

        if (blinkT > 0) blinkT -= lastFrameTime;
        if (flashT > 0) flashT -= lastFrameTime;

        // ease squash back to neutral so one-shot pulses settle on their own
        squashX += (1.f - squashX) * std::min(1.f, lastFrameTime * 12.f);
        squashY += (1.f - squashY) * std::min(1.f, lastFrameTime * 12.f);
    }

    void draw() override {
        if (!texture || !visible) return;
        if (alpha <= 0.01f) return;
        if (blinkT > 0 && (((int)(blinkT * 20)) & 1)) return;

        // Flash wins over the persistent tint, but both are applied and
        // undone within this call so a shared texture is never left dirty.
        Uint8 tr = 255, tg = 255, tb = 255;
        bool  tint = false;
        if (flashT > 0)      { tr = 255;  tg = 90;  tb = 90; tint = true; }
        else if (tintOn)     { tr = tintR; tg = tintG; tb = tintB; tint = true; }

        const bool fade = (alpha < 0.999f);
        if (tint) SDL_SetTextureColorMod(texture, tr, tg, tb);

        SDL_Rect dst = destRect;
        dst.y += (int)std::lround(drawBob);
        drawOne(dst, srcRect, flip, fade ? alpha : 1.f);

        // Cross-fade into the next cell over the tail of the step.  Only
        // worth doing on short loops -- a two-frame bat flap flickers without
        // it, while a six-frame walk already has enough cells to read.
        if (blendFrac > 0.f && frames > 1 && phase > 1.f - blendFrac) {
            const float t = (phase - (1.f - blendFrac)) / blendFrac;
            SDL_Rect nsrc = srcRect;
            nsrc.x = (startCol + ((frameIdx + 1) % frames)) * frameW;
            drawOne(dst, nsrc, flip, t * (fade ? alpha : 1.f));
        }

        if (tint) SDL_SetTextureColorMod(texture, 255, 255, 255);
        if (fade) SDL_SetTextureAlphaMod(texture, 255);
    }

private:
    // One blit, honouring rotation and alpha.  Kept out of draw() so the
    // cross-fade can call it twice without duplicating the pivot maths.
    //
    // The rotation path is only taken when an animation actually asks for a
    // lean: SDL_RenderCopyEx rotates about a pivot, and the rotated quad can
    // spill outside destRect, which would change the measured size of every
    // sprite that has no lean at all.
    void drawOne(const SDL_Rect& dst, const SDL_Rect& src,
                 SDL_RendererFlip fl, float a) {
        if (a <= 0.01f) return;
        if (a < 0.999f) SDL_SetTextureAlphaMod(texture, (Uint8)(a * 255.f));
        if (drawAngle != 0.f) {
            // Pivot at the bottom centre so a leaning walker stays planted
            // instead of swinging around its middle.
            SDL_Point pivot{ dst.w / 2, dst.h };
            SDL_RenderCopyEx(Game::renderer, texture, &src, &dst,
                             drawAngle, &pivot, fl);
        } else {
            TextureManager::Draw(texture, src, dst, fl);
        }
        if (a < 0.999f) SDL_SetTextureAlphaMod(texture, 255);
    }
};
