# ps1-primitive-vj

PS1 Emulator VJ system — a PCSX-Redux-based fork that intercepts GPU primitives
just before they're rendered, applying real-time controlled glitches via MIDI.

This is **not** a post-process effect over the final framebuffer. The point of
intervention is the polygon / sprite / textured-rect stream itself: vertices,
UVs, vertex colors, draw order, and per-primitive drop decisions.

## Status

All six phases are wired and running on Windows. Development has been idle
since **2026-05-13**; it stopped at a clean point (no unmerged branches,
no open issues). The consumer that actually gets used is the mixer,
[ps1-vj-mix](https://github.com/pbfmp5g55-gif/ps1-vj-mix) — see its
`HANDOVER_ja.md` for where the project as a whole is.

| Phase | Description | State |
|---|---|---|
| 0 | Repo / submodule setup | done |
| 1 | OpenBIOS fetch script | done |
| 2 | MIDI input (RtMidi) | done — `RtMidiController`, opt-in via `-DVJ_ENABLE_RTMIDI=ON`; dynamic CC mapping + Learn helper |
| 3 | Primitive logging hook | done — verified in the fork |
| 4 | Geometry / chance / drop / color effects | done — all 8 params live, env-var- and MIDI-tunable |
| 5 | Texture / missing / color refinements | done — UV offset, drop, per-vertex RGB in `PrimitiveInterceptor` |
| 6 | Safety limiter integration | done — `lowMasterSafety` applied on `beginFrame` |

Built on top of those, later work added: `AutoMode` LFO modulation,
`FilterPresetBank` (region / area / textured-only filters, saved to file,
MIDI-morphable), `PrimitiveStream` serialisation (format v3 — VRAM uploads
and inline CLUT palettes), and per-primitive `BlendMode` for PS1
semi-transparency.

The integrated emulator (libvj wired into PCSX-Redux) builds and runs
end-to-end on Windows. Glitches are applied to live PS1 GPU primitives —
characters / backgrounds / UI individually, *not* a post-process over the
final framebuffer.

## What's in this repo

This repository contains the **VJ subsystem** that's meant to be wired into a
PCSX-Redux fork. It builds and runs standalone (with a synthetic primitive
demo); the emulator integration is described in `docs/INTEGRATION.md` and
`docs/PCSX_REDUX_HOOKS.md`.

The integrated PCSX-Redux fork lives at:

> https://github.com/pbfmp5g55-gif/pcsx-redux (branch `vj-integration`)

It pulls this repo in as a git submodule at `third_party/libvj/`.

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

## Running the integrated emulator (Phase 4)

Until MIDI is wired (Phase 2), the glitch params are driven by environment
variables read once at startup. Each is a float in `[0, 1]`. All default to 0
so an unconfigured launch behaves identically to an unmodified emulator.

| env var       | Param      | Effect when raised |
|---------------|------------|--------------------|
| `VJ_MASTER`   | master     | Overall scale on every other effect |
| `VJ_CHANCE`   | chance     | Per-primitive probability of being touched |
| `VJ_GEOMETRY` | geometry   | Vertex jitter (px), per vertex per primitive |
| `VJ_TEXTURE`  | texture    | UV offset on textured primitives |
| `VJ_MISSING`  | missing    | Probability of dropping a primitive entirely |
| `VJ_COLOR`    | color      | Per-vertex RGB scaling |
| `VJ_DEPTH`    | depth      | Draw-order delay queue (async submission) |
| `VJ_CHAOS`    | chaos      | Random hold-state update rate |

Get a Windows artifact from CI (or build locally), download the OpenBIOS,
and launch:

```powershell
# Get artifact (requires gh CLI authenticated for the fork repo)
gh run download <run-id> --repo pbfmp5g55-gif/pcsx-redux --dir build

# OpenBIOS
python scripts/fetch_openbios.py

# pcsx-redux is a Windows GUI subsystem app, so its stderr is unreliable.
# vj.log (in the working directory; override with VJ_LOG) is the diagnostic
# channel — every 4096th primitive logs a line plus an init banner.
$env:VJ_MASTER   = "1.0"
$env:VJ_CHANCE   = "0.8"
$env:VJ_GEOMETRY = "0.7"
$env:VJ_MISSING  = "0.3"

# pcsx-redux defaults to paused; -run resumes it on launch
.\build\pcsx-redux.exe -bios .\bios\openbios.bin -run

# inspect vj.log for [VJ] frame#N prim#M ... submitted=0/1
Get-Content build\vj.log -Tail 20
```

`submitted=0` lines are dropped primitives (`MISSING` rolled drop) — those
draw calls never reach the GPU. `submitted=1` are kept (possibly with mutated
geometry / colors).

Without a game ROM the OpenBIOS no-disc screen draws very few primitives per
frame, so the glitch is hard to *see*. For a visible demo, load a homebrew
or a game image you legally own:

```powershell
.\build\pcsx-redux.exe -bios .\bios\openbios.bin -loadexe my_game.ps-exe -run
.\build\pcsx-redux.exe -bios .\bios\openbios.bin -iso my_game.bin -run
```

## MIDI control

Shipped. The env vars above still work as a fallback / scripted-override
path; the default CC layout is:

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
near zero. The mapping is not fixed — each target has a **Learn** button in
the fork's VJ panel, so any CC can drive any param.

## Scope notes

MVP does **not** do: character/background/UI recognition, AI analysis, OSC,
Ableton Link, FFT/audio reactivity, preset save, seed-pin UI, advanced Z-sort.
Those are explicit non-goals — the effect is "controlled accidents" that ride
on whatever happens to be on screen.

## License

Project code: MIT (see `LICENSE`).
OpenBIOS: MIT (separate, not bundled).
PCSX-Redux fork code: subject to PCSX-Redux's license.
