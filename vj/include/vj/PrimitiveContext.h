#pragma once

#include "Primitive.h"

namespace vj {

struct PrimitiveContext {
    bool  textured       = false;
    bool  spriteLike     = false;
    float screenArea     = 0.0f;
    float centerX        = 0.0f;
    float centerY        = 0.0f;
    int   vertexCount    = 0;
    float priorityWeight = 1.0f;
};

PrimitiveContext analyzePrimitive(const Primitive& prim);
float computePriorityWeight(const PrimitiveContext& ctx);

}
