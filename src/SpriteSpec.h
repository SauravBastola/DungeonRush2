#pragma once
// ---------------------------------------------------------------------------
// SpriteSpec — the single source of truth for "how big is this thing".
//
// Every drawable entity in the game takes its numbers from this table.  A
// spec records the geometry of the ART (the cell size inside the PNG) and
// the geometry of the GAMEPLAY (the collider), and the scale that bridges
// them.  Nothing anywhere else is allowed to guess a size, which is what
// keeps the hero, the enemies and the props all in the same visual scale.
//
//   fw, fh   cell size inside the source texture
//   scale    uniform multiplier from cell -> screen pixels
//   cw, ch   collider size in pixels
//   anchor   how the art is hung off the collider (see SpriteComponent)
//
// To retune the whole game's proportions you only touch this file.
// ---------------------------------------------------------------------------
#include <cstdint>

namespace Sprites {

    enum class Anchor { Feet, Centre, Box };

    struct Spec {
        const char* tex;    // asset id from AssetManager
        int   fw, fh;       // frame (cell) size in the sheet
        float scale;        // uniform draw scale
        int   cw, ch;       // collider size
        Anchor anchor;      // how art hangs off the collider
        bool  flipToFacing; // mirror with facing direction
    };

    // ---- hero -------------------------------------------------------------
    // player_sprites.png is 350x296 -> a 7 col x 8 row grid of 50x37 cells.
    //
    // The old spec said 50x42.  296/42 is 7.05, so eight rows were being
    // sliced as seven and every animation after the first sampled a band that
    // straddled two rows of art — which is why the hero looked like a
    // flickering smear instead of running.  The sheet really is:
    //
    //   row 0 idle          3 cells      row 4 THRUST   6 cells
    //   row 1 run           6 cells      row 5 JUMP     4 cells
    //   row 2 POWER SWING   5 cells      row 6 cast     3 cells
    //   row 3 SLASH         6 cells      row 7 die      7 cells
    //
    // The rows are not in the order you would expect, and getting this wrong
    // is very visible.  Measuring the opaque-pixel bounding box of every cell
    // settles it: row 5's frames rest their feet at y = 35, 35, 33, 27 — they
    // lift off the floor as the pose rises — so row 5 is the airborne row.
    // Every cell of row 2 stays planted at y = 35 and two of them carry a
    // huge filled arc, so row 2 is a sword swing, not a jump.  Row 3's arc is
    // a flat blade line across the body (horizontal sweep) and row 4 crouches
    // and extends the blade forward (lunge).
    //
    // An earlier mapping pointed jump/apex/fall/leap at row 2, which is why
    // the hero played a sword animation in mid-air.
    //
    // Scale is chosen so the drawn cell is exactly the collider's height
    // (50 / 37); each cell carries transparent padding around the ~30px-wide
    // figure, so the body overhangs the 32px collider by a few pixels — which
    // is what lets the sword-swing frames read at all.
    inline constexpr Spec PLAYER{
        "player", 48, 56, 1.00f, 32, 50, Anchor::Feet, true };
    // ART-12: the sheet is now generated at 1:1 by tools/build_art.py hero()
    // -- 7 x 8 cells of 48x56, same row meaning as above -- so the 50/37
    // (1.35) exception is retired.  Collider unchanged: no physics change.

    // ---- enemies ----------------------------------------------------------
    // ART-11: every enemy sheet is generated at 1:1 by tools/build_art.py
    // (enemy_<kind>.png), so every scale is 1.0.  The comments below that
    // still describe the old sheets are superseded by the generator docs.
    // skeleton_sprites.png 1200x900 -> 6 rows x 8 cols of 150x150
    //   rows: idle, walk, attack, hit, die, shoot
    inline constexpr Spec SKELETON{
        "skeleton", 40, 56, 1.00f, 36, 50, Anchor::Feet, true };

    // enemy_slime.png 128x96 -> 3 rows x 4 cols of 32x32 (tools/build_art.py
    // slime(), ART-11): idle, hop, die.  1:1, figure 26x20 on the collider.
    inline constexpr Spec SLIME{
        "slime", 32, 32, 1.00f, 26, 20, Anchor::Feet, true };

    // enemy_bat.png 64x48 -> 1 row x 2 cols of 32x48
    inline constexpr Spec BAT{
        "bat", 32, 32, 1.00f, 28, 30, Anchor::Feet, true };

    // mage.png 128x40 -> 1 row x 4 cols of 32x40
    inline constexpr Spec MAGE{
        "mage", 40, 56, 1.00f, 34, 50, Anchor::Feet, true };

    // archer.png -> rows: idle, shoot, die (generated art)
    inline constexpr Spec ARCHER{
        "archer", 40, 56, 1.00f, 34, 52, Anchor::Feet, true };

    // bomber.png -> rows: walk, fuse, explode (generated art)
    inline constexpr Spec BOMBER{
        "bomber", 32, 32, 1.00f, 26, 30, Anchor::Feet, true };

    // villain_boss.png 150x150 -> single cell
    inline constexpr Spec BOSS{
        "boss", 96, 104, 1.00f, 88, 100, Anchor::Feet, true };

    // ---- pickups ----------------------------------------------------------
    // coins.png 488x62 -> 1 row x 8 cols of 61x62
    inline constexpr Spec COIN{
        "coin", 61, 62, 0.40f, 24, 24, Anchor::Centre, false };

    inline constexpr Spec POWER_HEALTH{
        "power_health", 64, 64, 0.44f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_SPEED{
        "power_speed", 64, 64, 0.44f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_INVINCIBLE{
        "power_invincible", 64, 64, 0.44f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_DAMAGE{
        "power_damage", 32, 32, 0.80f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_JUMP{
        "power_jump", 32, 32, 0.80f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_MAGNET{
        "power_magnet", 64, 64, 0.44f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_SHIELD{
        "power_shield", 64, 64, 0.44f, 24, 24, Anchor::Centre, false };
    inline constexpr Spec POWER_COINS{
        "power_coins", 64, 64, 0.44f, 24, 24, Anchor::Centre, false };

    // ---- facilities -------------------------------------------------------
    inline constexpr Spec CHECKPOINT{
        "checkpoint", 32, 64, 1.00f, 32, 64, Anchor::Box, false };
    inline constexpr Spec SHRINE{
        "shrine", 40, 56, 1.00f, 34, 50, Anchor::Box, false };
    inline constexpr Spec DOOR{
        "door", 64, 96, 1.00f, 56, 88, Anchor::Box, false };
    inline constexpr Spec KEY{
        "key", 22, 22, 1.00f, 20, 20, Anchor::Centre, false };

    // ---- hazards & props --------------------------------------------------
    inline constexpr Spec SPIKES{
        "spikes", 32, 16, 2.00f, 64, 24, Anchor::Box, false };
    // moving_platform.png 192x32 -> 1 row x 3 cols of 64x32
    inline constexpr Spec PLATFORM{
        "platform", 64, 32, 1.00f, 64, 32, Anchor::Box, false };
    inline constexpr Spec PROJECTILE{
        "projectile", 32, 32, 0.50f, 16, 16, Anchor::Centre, false };
    inline constexpr Spec FIREBALL{
        "fireball", 32, 32, 0.62f, 20, 20, Anchor::Centre, false };
    inline constexpr Spec SHOCKWAVE{
        "shockwave", 32, 32, 1.00f, 32, 32, Anchor::Centre, false };
    inline constexpr Spec HEART{
        "heart", 32, 32, 0.62f, 20, 20, Anchor::Box, false };

    // Apply a spec to a fresh SpriteComponent (call after addComponent).
    template <typename SpriteT>
    inline void apply(SpriteT& sp, const Spec& s) {
        sp.frameW = s.fw;
        sp.frameH = s.fh;
        sp.scale  = s.scale;
        sp.flipToFacing = s.flipToFacing;
        switch (s.anchor) {
            case Anchor::Feet:   sp.anchorFeet();   break;
            case Anchor::Centre: sp.anchorCentre(); break;
            case Anchor::Box:    sp.anchorBox();    break;
        }
    }

} // namespace Sprites
