#pragma once
// One animation = a horizontal run of `frames` cells on row `index`,
// starting at column `start`, advancing every `speed` milliseconds.
//
// On top of the frame advance, an animation can carry SECONDARY MOTION -- a
// vertical bob and a lean that run continuously underneath the frame steps.
// This is what separates a sprite that merely changes pictures from one that
// reads as alive: the sheets here are small (the bat has two cells, the boss
// has one), and at 100ms per cell a two-frame flap is a hard flicker.  A
// couple of degrees of lean and a pixel of bob, driven off the same clock,
// fill the gap between the steps without needing any new art.
struct Animation {
    int index = 0;    // row inside the sheet
    int frames = 1;   // how many cells to cycle through
    int speed = 100;  // ms per cell
    int start = 0;    // first column of the run

    float bob    = 0.f;  // vertical offset, PEAK amplitude in pixels
    float lean   = 0.f;  // rotation, PEAK amplitude in degrees
    float cycles = 2.f;  // how many bob/lean cycles per full loop of the run
    float blend  = 0.f;  // 0..1 of each step spent cross-fading to the next
                         // cell.  0 = hard cut, which is the old behaviour.

    Animation() = default;
    Animation(int i, int f, int s, int st = 0)
        : index(i), frames(f), speed(s), start(st) {}

    // Fluent so the call sites read as "this animation also does this":
    //     Animation(1, 6, 100).motion(1.5f, 2.5f)
    //     Animation(0, 2, 100).motion(2.0f, 6.0f, 2.f, 0.45f)
    Animation& motion(float bobPx, float leanDeg, float cyc = 2.f,
                      float blendFrac = 0.f) {
        bob = bobPx; lean = leanDeg; cycles = cyc; blend = blendFrac;
        return *this;
    }
};
