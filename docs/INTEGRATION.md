# PCSX-Redux Integration Notes

This document describes how to wire `libvj` into a PCSX-Redux fork. The library
is host-agnostic; it does not include any PCSX-Redux headers.

## Where to hook

The required hook point is **after** GPU command parsing has produced a
renderer-level primitive (with vertex coordinates, UVs, and vertex colors), and
**before** that primitive is submitted to the OpenGL renderer.

In the PCSX-Redux source, search for:

```
GPU
OpenGL
Renderer
Primitive
Polygon
Sprite
Vertex
Texcoord
Draw
Submit
```

Concretely, look for a function on the GL renderer that takes a primitive (or
its vertex buffer) and queues / draws it. Examples of names that have appeared
in similar codebases:

- `Renderer::drawPolygon`
- `Renderer::drawSprite`
- `addVertices`
- `submitPrimitive`

Avoid:

- The framebuffer presentation step (post-effect).
- Full-screen shader passes.
- Anything after rasterization.

## Wiring

```cpp
// once, at emulator startup
vj::PrimitiveInterceptor g_interceptor;
g_interceptor.setSubmitCallback([](const vj::Primitive& p) {
    // Convert vj::Primitive back to the renderer's native primitive type and
    // call the original submit function.
    submitPrimitiveOriginal(toNative(p));
});

// once per emulated frame, before any primitives:
auto params = g_midi.buildParams();
g_interceptor.beginFrame(params, estimatedPrimitiveCount);

// at the hook point, for each primitive:
vj::Primitive p = fromNative(nativePrim);
g_interceptor.interceptAndSubmit(p);

// once per emulated frame, at end-of-frame:
g_interceptor.endFrame();
```

## Primitive conversion

The `vj::Primitive` carries:

- `kind` (Triangle / Quad / Sprite)
- `textured` flag
- `vertices` (3 for triangles, 4 for quads / sprites; each has `x, y, u, v, r, g, b, a`)
- `hostTag` (opaque uint64 — use this to round-trip per-primitive renderer state
  like TPage, CLUT, blend mode, that VJ doesn't touch)

When converting back to the native primitive, pull TPage / CLUT / blend mode
from `hostTag` and overwrite only the per-vertex fields VJ may have changed.

A simple way to do this in C++ is to allocate the per-primitive native state in
a small free-list and store its index in `hostTag`.

## MIDI

A real `MidiController` implementation is host-specific. Recommended:

- **Windows**: RtMidi (cross-platform, MIT). On Windows it talks to WinMM.
- **Linux**: RtMidi (ALSA backend) or JACK directly.
- **macOS**: RtMidi (CoreMIDI backend).

Implement a subclass of `vj::MidiController` that exposes `update()` (pump
backend events into a CC table) and `getCC(int)` (return the latest 0..127
value, or -1 if untouched).

The provided `StaticMidiController` is for tests / the demo; it lets you set
CCs programmatically.

## Save state

The interceptor's state is intentionally **not** part of the emulator save
state:

- MIDI CCs are read live from hardware each frame.
- `RandomController` re-rolls naturally; loading a save state during play just
  means the next reroll happens against whatever knobs are live.
- `DepthDelayQueue` is flushed at `endFrame()` so it doesn't carry across save/
  load boundaries.

Make sure your fork keeps the GPU hook installed across save-state load (don't
re-init it from the loaded state).

## Phase order

Per the design spec (§17), implement in this order:

1. **Logging only.** At the hook point, log `textured / vertexCount / area` for
   every primitive. Verify the count and types match what you expect for known
   games.
2. **Geometry.** Apply `applyGeometry` only. Confirm visible per-primitive
   vertex jitter (not a full-screen wobble).
3. **Texture / Missing / Color.** Add UV jitter, drop, color shift.
4. **Safety.** Verify `lowMasterSafety`, `mustDraw()` floor, depth-queue flush.
5. **Depth.** Enable the delay queue last.
6. **Debug overlay.** Render `buildDebugText()` output via ImGui or similar.
