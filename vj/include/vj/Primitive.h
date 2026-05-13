#pragma once

#include <cstdint>
#include <vector>

namespace vj {

struct Vertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

enum class PrimitiveKind {
    Triangle,
    Quad,
    Sprite,
};

// PS1 semi-transparency mix mode (the ABR field on TPage). 0 means
// "opaque, no blend" (the most common case); 1..4 are the four PS1
// semi-transparency modes, in the same order as the PS1 spec.
//
//   0 = opaque
//   1 = 0.5 * back + 0.5 * front   (average)
//   2 = 1.0 * back + 1.0 * front   (additive)
//   3 = 1.0 * back - 1.0 * front   (subtractive)
//   4 = 1.0 * back + 0.25 * front  (additive quarter)
//
// Storage: a single byte. Stream format reuses the previously-padding
// byte after vertexCount so it round-trips without a version bump.
enum class BlendMode : uint8_t {
    Opaque         = 0,
    Average        = 1,
    Additive       = 2,
    Subtractive    = 3,
    AdditiveQuarter = 4,
};

// Inline CLUT palette captured at primitive submission time. The PS1 GPU
// resolves textured primitives by indirecting through a CLUT in VRAM; if
// the game rewrites that CLUT region after our snapshot, a downstream
// renderer that re-samples VRAM gets the wrong colours. Carrying the
// palette per primitive eliminates the desync entirely at the cost of
// 32 B (4bpp) or 512 B (8bpp) per textured primitive.
//
//   empty    : no palette captured (untextured, or 15bpp direct-colour)
//   16  ent. : 4bpp CLUT, palette[i] = PS1 5/5/5/M of colour index i
//   256 ent. : 8bpp CLUT, same encoding, 256 colours
struct Primitive {
    PrimitiveKind kind = PrimitiveKind::Triangle;
    bool textured = false;
    BlendMode blendMode = BlendMode::Opaque;
    std::vector<Vertex> vertices;
    uint64_t hostTag = 0;
    std::vector<uint16_t> palette;
};

}
