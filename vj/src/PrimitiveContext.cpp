#include "vj/PrimitiveContext.h"

#include <algorithm>

namespace vj {

float computePriorityWeight(const PrimitiveContext& ctx) {
    float w = 1.0f;
    if (ctx.textured)            w += 0.3f;
    if (ctx.screenArea > 5000.0f) w += 0.3f;
    if (ctx.spriteLike)          w += 0.2f;
    return std::clamp(w, 0.5f, 1.8f);
}

PrimitiveContext analyzePrimitive(const Primitive& prim) {
    PrimitiveContext ctx;
    ctx.textured    = prim.textured;
    ctx.spriteLike  = (prim.kind == PrimitiveKind::Sprite);
    ctx.vertexCount = static_cast<int>(prim.vertices.size());

    if (prim.vertices.empty()) {
        ctx.priorityWeight = computePriorityWeight(ctx);
        return ctx;
    }

    float minX = prim.vertices[0].x;
    float maxX = minX;
    float minY = prim.vertices[0].y;
    float maxY = minY;
    float sumX = 0.0f;
    float sumY = 0.0f;

    for (const auto& v : prim.vertices) {
        if (v.x < minX) minX = v.x;
        if (v.x > maxX) maxX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.y > maxY) maxY = v.y;
        sumX += v.x;
        sumY += v.y;
    }

    const float n = static_cast<float>(prim.vertices.size());
    ctx.centerX = sumX / n;
    ctx.centerY = sumY / n;
    ctx.screenArea = (maxX - minX) * (maxY - minY);
    ctx.priorityWeight = computePriorityWeight(ctx);
    return ctx;
}

}
