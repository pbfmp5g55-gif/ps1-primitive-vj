# ps1-primitive-vj

PS1 Emulator VJ system — a PCSX-Redux-based fork that intercepts GPU primitives
just before they're rendered, applying real-time controlled glitches via MIDI.

This is **not** a post-process effect over the final framebuffer. The point of
intervention is the polygon / sprite / textured-rect stream itself: vertices,
UVs, vertex colors, draw order, and per-primitive drop decisions.

## What's in this repo

This repository contains the **VJ subsystem** that's meant to be wired into a
PCSX-Redux fork. It builds and runs standalone (with a synthetic primitive
demo); the emulator integration is described in `docs/INTEGRATION.md`.

```
ps1-primitive-vj/
├── README.md
├── CMakeLists.txt              # builds libvj + vj_demo
├── bios/
│   └── (place openbios.bin here)
├── scripts/
│   └── fetch_openbios.py
├── docs/
│   └── INTEGRATION.md          # PCSX-Redux hook points
└── vj/
    ├── include/vj/             # public headers
    ├── src/                    # implementations
    └── example/demo.cpp        # standalone synthetic-primitive demo
```

## Build

```
cmake -S . -B build
cmake --build build
./build/vj_demo            # or build\Debug\vj_demo.exe on MSVC
```

The demo feeds 50 synthetic primitives per frame for 60 frames through the full
pipeline (MIDI → params → random hold-state → glitch effects → safety limiter
→ depth delay queue) and prints debug-overlay snapshots every 10 frames.

## BIOS setup

```
python scripts/fetch_openbios.py
```

Sources:
- https://www.mirrorservice.org/sites/libreboot.org/release/canoeboot/20250107/roms/playstation/openbios.bin
- https://mirrors.mit.edu/libreboot/canoeboot/old_releases/20250107/roms/playstation/openbios.bin

OpenBIOS is MIT-licensed Free Software developed by the PCSX-Redux team.
Compatibility is not 100% identical to Sony retail BIOS — design assumes BIOS
is swappable.

**Sony retail BIOS is never bundled.** Use only game images you legally own.

## MIDI control

8 CC knobs:

| CC | Name     | Role                                  |
|---:|----------|---------------------------------------|
| 20 | MASTER   | Overall effect strength               |
| 21 | CHANCE   | Per-primitive application probability |
| 22 | GEOMETRY | Vertex offset amount                  |
| 23 | TEXTURE  | UV offset amount                      |
| 24 | MISSING  | Drop-rate                             |
| 25 | COLOR    | Vertex-color shift                    |
| 26 | DEPTH    | Draw-order delay queue                |
| 27 | CHAOS    | Random-hold update rate / spike rate  |

Curves: MASTER / CHANCE / CHAOS are linear; the rest use `x²` for fine control
near zero.

## Scope notes

MVP does **not** do: character/background/UI recognition, AI analysis, OSC,
Ableton Link, FFT/audio reactivity, preset save, seed-pin UI, advanced Z-sort.
Those are explicit non-goals — the effect is "controlled accidents" that ride
on whatever happens to be on screen.

## License

Project code: MIT (see `LICENSE`).
OpenBIOS: MIT (separate, not bundled).
PCSX-Redux fork code: subject to PCSX-Redux's license.
