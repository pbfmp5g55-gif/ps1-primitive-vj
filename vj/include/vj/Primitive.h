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

struct Primitive {
    PrimitiveKind kind = PrimitiveKind::Triangle;
    bool textured = false;
    std::vector<Vertex> vertices;
    uint64_t hostTag = 0;
};

}
