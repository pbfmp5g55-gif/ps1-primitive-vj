#include "vj/AutoMode.h"

#include <algorithm>
#include <cmath>

namespace vj {

namespace {

constexpr float kTwoPi = 6.283185307179586f;
constexpr float kFps   = 60.0f; // assumed display refresh; LFO periods are
                                // expressed in real seconds via this constant

// Returns a normalised sine in [0, 1] at `frame` for the requested period.
float lfo01(int frame, float periodSec, float phase, float rate) {
    if (periodSec <= 0.0f) return 0.0f;
    const float t = static_cast<float>(frame) / kFps;
    const float angle = kTwoPi * (t / periodSec) * rate + phase;
    return (std::sin(angle) + 1.0f) * 0.5f;
}

float modulate(float base, float lfoValue, float depth, float perAxisScale = 1.0f) {
    return std::clamp(base + depth * perAxisScale * lfoValue, 0.0f, 1.0f);
}

}  // namespace

Params applyAutoMode(const Params& base, const AutoModeParams& cfg, int frame) {
    if (!cfg.enabled) return base;

    const float d = cfg.depth;
    const float r = cfg.rate;

    Params out = base;
    out.master   = modulate(base.master,   lfo01(frame,  7.0f, 0.0f, r), d);
    out.chance   = modulate(base.chance,   lfo01(frame, 11.0f, 0.7f, r), d);
    out.geometry = modulate(base.geometry, lfo01(frame, 13.0f, 1.4f, r), d);
    out.texture  = modulate(base.texture,  lfo01(frame, 17.0f, 2.1f, r), d);
    // missing / depth-delay / chaos are visually heavy — half-amplitude swing.
    out.missing  = modulate(base.missing,  lfo01(frame, 19.0f, 2.8f, r), d, 0.5f);
    out.color    = modulate(base.color,    lfo01(frame, 23.0f, 3.5f, r), d);
    out.depth    = modulate(base.depth,    lfo01(frame, 29.0f, 4.2f, r), d, 0.5f);
    out.chaos    = modulate(base.chaos,    lfo01(frame, 31.0f, 4.9f, r), d, 0.5f);
    return out;
}

}
