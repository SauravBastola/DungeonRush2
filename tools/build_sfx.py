#!/usr/bin/env python3
"""
DungeonRush2 sound effects (Batch E).

Writes assets/sounds/sfx/*.wav - 16-bit mono 44.1 kHz - from short recipes.
Like the art generators it is deterministic: noise comes from a seeded
integer generator, so two runs are byte-identical, and every sound is
licence-free and editable here.

A recipe is a list of layers mixed together; each layer is
    (wave, f0, f1, dur, attack, decay_shape, gain, extra)
  wave         'square' | 'tri' | 'saw' | 'sine' | 'noise'
  f0 -> f1     start / end frequency in Hz (exponential sweep)
  dur          seconds
  attack       seconds of linear fade-in
  decay_shape  exponent of the (1 - t)^k fade-out (1 linear, 3 snappy)
  gain         0..1
  extra        dict: delay (s), vibrato (Hz, depth), duty (square), lp (low-pass 0..1)
Each sound is normalised to a -4 dBFS peak, so the set is level-matched; a
per-sound trim then sets how loud it sits in the mix.
"""
import math
import os
import struct
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "sounds", "sfx")
RATE = 44100
PEAK = 10 ** (-4 / 20)          # -4 dBFS


class Noise:
    """xorshift32: seeded, so every run is byte-identical."""
    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF or 1

    def next(self):
        s = self.s
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        self.s = s
        return (s / 0xFFFFFFFF) * 2.0 - 1.0


def layer(wave_, f0, f1, dur, attack, shape, gain, extra=None, seed=1):
    extra = extra or {}
    n = int(dur * RATE)
    delay = int(extra.get("delay", 0.0) * RATE)
    vib_hz, vib_d = extra.get("vibrato", (0.0, 0.0))
    duty = extra.get("duty", 0.5)
    lp = extra.get("lp", 1.0)              # one-pole low-pass, 1 = off
    hold = extra.get("hold", 0)            # noise: repeat each value n samples (pitchy noise)
    rng = Noise(seed)
    out = [0.0] * (delay + n)
    phase, y, held, hc = 0.0, 0.0, 0.0, 0
    for i in range(n):
        t = i / max(1, n - 1)
        f = f0 * (f1 / f0) ** t if f0 > 0 and f1 > 0 else f0
        if vib_hz:
            f *= 1.0 + vib_d * math.sin(2 * math.pi * vib_hz * i / RATE)
        phase = (phase + f / RATE) % 1.0
        if wave_ == "square":
            v = 1.0 if phase < duty else -1.0
        elif wave_ == "tri":
            v = 4 * abs(phase - 0.5) - 1.0
        elif wave_ == "saw":
            v = 2 * phase - 1.0
        elif wave_ == "sine":
            v = math.sin(2 * math.pi * phase)
        else:                                # noise
            if hc <= 0:
                held, hc = rng.next(), max(1, hold if hold else int(RATE / max(f, 1.0)))
            hc -= 1
            v = held
        env = min(1.0, i / max(1, attack * RATE)) * (1.0 - t) ** shape
        y = y + lp * (v - y)
        out[delay + i] = y * env * gain
    return out


def mix(layers):
    n = max(len(l) for l in layers)
    buf = [0.0] * n
    for l in layers:
        for i, v in enumerate(l):
            buf[i] += v
    return buf


def crush(buf, bits=8):
    q = 2 ** (bits - 1)
    return [round(v * q) / q for v in buf]


def write(name, buf, trim=1.0):
    peak = max(1e-9, max(abs(v) for v in buf))
    k = PEAK * trim / peak
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v * k)) * 32767))
                               for v in buf))
    return path, len(buf) / RATE


L = layer
SOUNDS = {
    # --- the hero -----------------------------------------------------------
    "swing":        ([L("noise", 5000, 900, 0.16, 0.01, 2, 0.8, {"lp": 0.35}, 11),
                      L("sine", 420, 180, 0.12, 0.0, 2, 0.25)], 0.7, None),
    "swing_heavy":  ([L("noise", 3500, 500, 0.26, 0.02, 2, 0.9, {"lp": 0.25}, 12),
                      L("saw", 160, 70, 0.22, 0.0, 2, 0.35, {"lp": 0.3})], 0.8, None),
    "bolt":         ([L("square", 1400, 300, 0.18, 0.0, 2, 0.5, {"duty": 0.25}),
                      L("sine", 2200, 900, 0.10, 0.0, 3, 0.3)], 0.6, 8),
    "hit_enemy":    ([L("noise", 1800, 300, 0.10, 0.0, 3, 0.9, {"lp": 0.4}, 21),
                      L("square", 180, 70, 0.10, 0.0, 2, 0.5, {"duty": 0.5})], 0.75, 8),
    "hurt":         ([L("square", 520, 180, 0.22, 0.0, 1.5, 0.6, {"duty": 0.4, "vibrato": (30, 0.08)}),
                      L("noise", 1200, 300, 0.12, 0.0, 3, 0.4, {"lp": 0.4}, 22)], 0.85, 8),
    "parry":        ([L("square", 1760, 1760, 0.32, 0.0, 2.5, 0.35, {"duty": 0.15}),
                      L("sine", 2640, 2600, 0.40, 0.0, 2, 0.35),
                      L("sine", 3960, 3900, 0.25, 0.0, 3, 0.2),
                      L("noise", 6000, 2000, 0.05, 0.0, 3, 0.6, {"lp": 0.7}, 31)], 0.9, None),
    "block":        ([L("square", 300, 220, 0.12, 0.0, 3, 0.5, {"duty": 0.3}),
                      L("noise", 2500, 800, 0.08, 0.0, 3, 0.6, {"lp": 0.5}, 32)], 0.7, 8),
    "slide":        ([L("noise", 1200, 2500, 0.30, 0.02, 1.5, 0.7, {"lp": 0.2}, 41)], 0.55, None),
    "wall_jump":    ([L("square", 300, 700, 0.12, 0.0, 2, 0.5, {"duty": 0.5}),
                      L("noise", 3000, 1000, 0.05, 0.0, 3, 0.4, {"lp": 0.5}, 42)], 0.6, 8),
    "jump":         ([L("square", 260, 620, 0.14, 0.0, 1.5, 0.55, {"duty": 0.5})], 0.55, 8),
    "land":         ([L("noise", 600, 120, 0.09, 0.0, 3, 0.9, {"lp": 0.25}, 43),
                      L("sine", 120, 60, 0.08, 0.0, 2, 0.5)], 0.5, None),
    "grapple_fire": ([L("saw", 600, 2400, 0.16, 0.0, 1, 0.45, {"lp": 0.6}),
                      L("noise", 4000, 4000, 0.16, 0.0, 1, 0.25, {"lp": 0.6}, 51)], 0.6, 8),
    "grapple_latch":([L("square", 1200, 1200, 0.06, 0.0, 3, 0.5, {"duty": 0.2}),
                      L("sine", 1800, 1700, 0.18, 0.0, 3, 0.3)], 0.6, None),
    # --- pickups and the level ---------------------------------------------
    "coin":         ([L("square", 988, 988, 0.06, 0.0, 1, 0.5, {"duty": 0.5}),
                      L("square", 1319, 1319, 0.20, 0.0, 2, 0.5, {"duty": 0.5, "delay": 0.06})], 0.55, 8),
    "key":          ([L("square", 784, 784, 0.08, 0.0, 1, 0.45, {"duty": 0.25}),
                      L("square", 1047, 1047, 0.08, 0.0, 1, 0.45, {"duty": 0.25, "delay": 0.08}),
                      L("square", 1568, 1568, 0.30, 0.0, 2, 0.45, {"duty": 0.25, "delay": 0.16})], 0.7, 8),
    "checkpoint":   ([L("tri", 523, 523, 0.30, 0.0, 1.5, 0.5),
                      L("tri", 659, 659, 0.30, 0.0, 1.5, 0.5, {"delay": 0.10}),
                      L("tri", 784, 784, 0.50, 0.0, 2, 0.5, {"delay": 0.20}),
                      L("sine", 1568, 1568, 0.45, 0.0, 2.5, 0.2, {"delay": 0.20})], 0.75, None),
    "door":         ([L("noise", 300, 120, 0.25, 0.0, 2, 0.8, {"lp": 0.15}, 61),
                      L("square", 110, 90, 0.25, 0.0, 2, 0.4, {"duty": 0.5}),
                      L("tri", 784, 784, 0.35, 0.0, 2, 0.35, {"delay": 0.22}),
                      L("tri", 1175, 1175, 0.45, 0.0, 2, 0.35, {"delay": 0.32})], 0.85, None),
    "stage_clear":  ([L("square", 523, 523, 0.12, 0.0, 1, 0.4, {"duty": 0.25}),
                      L("square", 659, 659, 0.12, 0.0, 1, 0.4, {"duty": 0.25, "delay": 0.13}),
                      L("square", 784, 784, 0.12, 0.0, 1, 0.4, {"duty": 0.25, "delay": 0.26}),
                      L("square", 1047, 1047, 0.55, 0.0, 2, 0.45, {"duty": 0.25, "delay": 0.39}),
                      L("tri", 262, 262, 0.95, 0.0, 1.5, 0.35)], 0.85, 8),
    "explosion":    ([L("noise", 800, 60, 0.55, 0.0, 2, 1.0, {"lp": 0.18}, 71),
                      L("sine", 90, 40, 0.45, 0.0, 2, 0.6)], 0.95, None),
    # --- enemy tells and attacks (each warning has its own voice) ----------
    "skel_tell":    ([L("noise", 5000, 3000, 0.40, 0.25, 0.5, 0.6, {"lp": 0.5, "hold": 3}, 81),
                      L("saw", 220, 330, 0.40, 0.3, 0.5, 0.2, {"lp": 0.3})], 0.6, 8),
    "skel_strike":  ([L("noise", 4500, 700, 0.14, 0.0, 2, 0.9, {"lp": 0.4}, 82)], 0.7, None),
    "bat_tell":     ([L("square", 2800, 3400, 0.30, 0.02, 0.8, 0.3, {"duty": 0.12, "vibrato": (40, 0.06)})], 0.5, 8),
    "bat_dive":     ([L("square", 3200, 1400, 0.30, 0.0, 1.2, 0.35, {"duty": 0.12})], 0.55, 8),
    "spit_tell":    ([L("sine", 120, 360, 0.55, 0.3, 0.6, 0.6, {"vibrato": (12, 0.05)}),
                      L("noise", 400, 900, 0.55, 0.3, 0.6, 0.2, {"lp": 0.15}, 91)], 0.6, None),
    "spit":         ([L("sine", 500, 140, 0.12, 0.0, 2, 0.7),
                      L("noise", 2000, 500, 0.06, 0.0, 3, 0.5, {"lp": 0.4}, 92)], 0.65, None),
    "wisp_tell":    ([L("sine", 900, 1800, 0.48, 0.3, 0.5, 0.35, {"vibrato": (18, 0.04)}),
                      L("sine", 1350, 2700, 0.48, 0.3, 0.5, 0.2)], 0.55, None),
    "wisp_lunge":   ([L("noise", 1500, 5000, 0.30, 0.0, 1.5, 0.6, {"lp": 0.3}, 93),
                      L("sine", 600, 1400, 0.25, 0.0, 2, 0.3)], 0.6, None),
    "roar":         ([L("saw", 90, 55, 1.1, 0.08, 1.2, 0.7, {"lp": 0.22, "vibrato": (7, 0.08)}),
                      L("noise", 400, 120, 1.0, 0.05, 1.5, 0.5, {"lp": 0.12}, 95),
                      L("square", 70, 45, 0.9, 0.1, 1.2, 0.3, {"duty": 0.4, "lp": 0.3})], 0.95, None),
    # ---- Batch P: ambient loops (6 s, steady beds + sparse details) ----
    "amb_meadow":   ([L("noise", 300, 300, 6.0, 0.0, 0, 0.35, {"lp": 0.04}, 101)] +
                     [L("sine", 2600 + 400 * (i % 3), 3400 + 300 * (i % 2), 0.12, 0.01, 2, 0.18,
                        {"delay": 0.4 + i * 0.83, "vibrato": (28, 0.05)}) for i in range(6)], 0.45, None),
    "amb_jungle":   ([L("noise", 4000, 4000, 6.0, 0.0, 0, 0.55, {"lp": 0.45}, 102),
                      L("noise", 500, 500, 6.0, 0.0, 0, 0.30, {"lp": 0.06}, 103)] +
                     [L("sine", 1400 - 120 * (i % 4), 700, 0.06, 0.0, 3, 0.25, {"delay": 0.2 + i * 0.47})
                      for i in range(12)], 0.45, None),
    "amb_canyon":   ([L("noise", 200, 200, 6.0, 0.0, 0, 0.55, {"lp": 0.03}, 104),
                      L("saw", 60, 64, 6.0, 0.0, 0, 0.12, {"lp": 0.05, "vibrato": (0.4, 0.2)})] +
                     [L("noise", 3000, 800, 0.05, 0.0, 3, 0.35, {"lp": 0.6, "delay": 0.3 + i * 0.71}, 110 + i)
                      for i in range(8)], 0.45, None),
    "boss_charge":  ([L("saw", 60, 240, 0.9, 0.2, 0.4, 0.6, {"lp": 0.25, "vibrato": (8, 0.05)})], 0.8, 8),
    "enemy_die":    ([L("square", 600, 90, 0.30, 0.0, 1.5, 0.5, {"duty": 0.5}),
                      L("noise", 1500, 200, 0.25, 0.0, 2, 0.5, {"lp": 0.3}, 94)], 0.7, 8),
}


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    total = 0.0
    for name, (layers, trim, bits) in SOUNDS.items():
        buf = mix(layers)
        if bits:
            buf = crush(buf, bits)
        _, secs = write(name, buf, trim)
        total += secs
    print(f"sfx        : {len(SOUNDS)} sounds -> {os.path.relpath(OUT, ROOT)}  ({total:.1f} s total)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
