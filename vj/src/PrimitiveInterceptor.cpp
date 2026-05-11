#include "vj/PrimitiveInterceptor.h"

#include <algorithm>
#include <cmath>

namespace vj {

int computeHoldFrames(float chaos01) {
    return std::clamp(
        static_cast<int>(std::round(10.0f - chaos01 * 7.0f)), 3, 10);
}

static uint8_t clampColor(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

PrimitiveInterceptor::PrimitiveInterceptor() : m_depthQueue(64) {}

void PrimitiveInterceptor::beginFrame(const Params& params, int estimatedPrimitiveCount) {
    Params adjusted = params;
    const float lm = lowMasterSafety(params.master);
    adjusted.master = params.master * lm;

    m_params = adjusted;
    m_rand.update(m_params.chaos);
    m_limiter.beginFrame(estimatedPrimitiveCount);
    m_affectedCount = 0;
    m_filterCounter = 0;
}

void PrimitiveInterceptor::endFrame() {
    m_depthQueue.flushAll([this](const Primitive& p) { rawSubmit(p); });
}

void PrimitiveInterceptor::rawSubmit(const Primitive& prim) {
    if (m_submit) m_submit(prim);
}

bool PrimitiveInterceptor::passesFilter(const PrimitiveContext& ctx) {
    const auto& f = m_params.filter;
    if (f.texturedOnly && !ctx.textured) return false;
    if (f.minArea > 0.0f && ctx.screenArea < f.minArea) return false;
    if (f.maxArea > 0.0f && ctx.screenArea > f.maxArea) return false;
    const bool regionEnabled = (f.regionX1 > f.regionX0) && (f.regionY1 > f.regionY0);
    if (regionEnabled) {
        if (ctx.centerX < f.regionX0 || ctx.centerX > f.regionX1) return false;
        if (ctx.centerY < f.regionY0 || ctx.centerY > f.regionY1) return false;
    }
    if (f.everyN > 0 && (m_filterCounter % f.everyN) != 0) return false;
    return true;
}

bool PrimitiveInterceptor::shouldAffect(const PrimitiveContext& ctx) {
    m_filterCounter++;
    if (!passesFilter(ctx)) return false;
    float p = m_params.chance * ctx.priorityWeight;
    p = std::clamp(p, 0.0f, 0.95f);
    return m_rand.rand01() < p;
}

void PrimitiveInterceptor::applyGeometry(Primitive& prim, const PrimitiveContext& /*ctx*/) {
    const float amt = m_params.geometry * m_params.master;
    if (amt <= 0.001f) return;

    const float maxOffset = 24.0f * amt;

    for (auto& v : prim.vertices) {
        float dx = (m_rand.randSigned() + m_rand.state.geometryBiasX) * maxOffset;
        float dy = (m_rand.randSigned() + m_rand.state.geometryBiasY) * maxOffset;
        dx = std::clamp(dx, -32.0f, 32.0f);
        dy = std::clamp(dy, -32.0f, 32.0f);
        v.x += dx;
        v.y += dy;
    }
}

void PrimitiveInterceptor::applyTexture(Primitive& prim, const PrimitiveContext& ctx) {
    if (!ctx.textured) return;

    const float amt = m_params.texture * m_params.master;
    if (amt <= 0.001f) return;

    const float maxUvOffset = 16.0f * amt;

    for (auto& v : prim.vertices) {
        float du = (m_rand.randSigned() + m_rand.state.uvBiasU) * maxUvOffset;
        float dv = (m_rand.randSigned() + m_rand.state.uvBiasV) * maxUvOffset;
        du = std::clamp(du, -32.0f, 32.0f);
        dv = std::clamp(dv, -32.0f, 32.0f);
        v.u += du;
        v.v += dv;
    }
}

void PrimitiveInterceptor::applyColor(Primitive& prim, const PrimitiveContext& /*ctx*/) {
    const float amt = m_params.color * m_params.master;
    if (amt <= 0.001f) return;

    for (auto& v : prim.vertices) {
        const float rMul = 1.0f + (m_rand.randSigned() + m_rand.state.colorBiasR) * amt;
        const float gMul = 1.0f + (m_rand.randSigned() + m_rand.state.colorBiasG) * amt;
        const float bMul = 1.0f + (m_rand.randSigned() + m_rand.state.colorBiasB) * amt;

        v.r = clampColor(static_cast<float>(v.r) * rMul);
        v.g = clampColor(static_cast<float>(v.g) * gMul);
        v.b = clampColor(static_cast<float>(v.b) * bMul);
    }
}

bool PrimitiveInterceptor::decideMissing(const Primitive& /*prim*/, const PrimitiveContext& /*ctx*/) {
    const float amt = m_params.missing * m_params.master;
    if (amt <= 0.001f) return false;

    float p = amt * (0.8f + m_rand.state.dropBias * 0.2f);
    p = std::clamp(p, 0.0f, 0.8f);
    return m_rand.rand01() < p;
}

void PrimitiveInterceptor::submitWithOptionalDepthDelay(Primitive& prim, const PrimitiveContext& /*ctx*/) {
    m_depthQueue.tickAndFlush([this](const Primitive& p) { rawSubmit(p); });

    const float amt = m_params.depth * m_params.master;
    if (amt <= 0.001f) {
        rawSubmit(prim);
        return;
    }

    if (m_rand.rand01() < amt * 0.3f) {
        const int delaySlots = m_rand.randInt(1, 3);
        m_depthQueue.push(prim, delaySlots, [this](const Primitive& p) { rawSubmit(p); });
    } else {
        rawSubmit(prim);
    }
}

void PrimitiveInterceptor::interceptAndSubmit(const Primitive& prim) {
    PrimitiveContext ctx = analyzePrimitive(prim);

    if (!shouldAffect(ctx)) {
        rawSubmit(prim);
        m_limiter.notifyDraw();
        return;
    }

    m_affectedCount++;

    Primitive modified = prim;
    applyGeometry(modified, ctx);
    applyTexture(modified, ctx);
    applyColor(modified, ctx);

    const bool wantSkip = decideMissing(modified, ctx);

    if (!wantSkip) {
        submitWithOptionalDepthDelay(modified, ctx);
        m_limiter.notifyDraw();
        return;
    }

    if (m_limiter.mustDraw()) {
        submitWithOptionalDepthDelay(modified, ctx);
        m_limiter.notifyDraw();
        m_limiter.notifyForcedDraw();
    } else {
        m_limiter.notifySkip();
    }
}

}
