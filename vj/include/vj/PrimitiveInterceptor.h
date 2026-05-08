#pragma once

#include <functional>

#include "DepthDelayQueue.h"
#include "Params.h"
#include "Primitive.h"
#include "PrimitiveContext.h"
#include "RandomController.h"
#include "SafetyLimiter.h"

namespace vj {

class PrimitiveInterceptor {
public:
    using SubmitFn = std::function<void(const Primitive&)>;

    PrimitiveInterceptor();

    void setSubmitCallback(SubmitFn fn) { m_submit = std::move(fn); }

    void beginFrame(const Params& params, int estimatedPrimitiveCount = 0);
    void interceptAndSubmit(const Primitive& prim);
    void endFrame();

    const Params& params() const { return m_params; }
    const SafetyLimiter& limiter() const { return m_limiter; }
    const RandomController& random() const { return m_rand; }
    const DepthDelayQueue& depthQueue() const { return m_depthQueue; }

    int affectedCount() const { return m_affectedCount; }

private:
    Params m_params;
    RandomController m_rand;
    SafetyLimiter m_limiter;
    DepthDelayQueue m_depthQueue;
    SubmitFn m_submit;
    int m_affectedCount = 0;

    bool shouldAffect(const PrimitiveContext& ctx);
    void applyGeometry(Primitive& prim, const PrimitiveContext& ctx);
    void applyTexture(Primitive& prim, const PrimitiveContext& ctx);
    void applyColor(Primitive& prim, const PrimitiveContext& ctx);
    bool decideMissing(const Primitive& prim, const PrimitiveContext& ctx);
    void submitWithOptionalDepthDelay(Primitive& prim, const PrimitiveContext& ctx);
    void rawSubmit(const Primitive& prim);
};

int computeHoldFrames(float chaos01);

}
