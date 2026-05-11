#pragma once

#include "Params.h"

namespace vj {

// AutoMode: drive Params via per-axis sine LFOs without any external input.
//
// Each Params axis gets its own LFO with a distinct, prime-ish period so the
// peaks drift relative to each other instead of locking into a regular pulse.
// `applyAutoMode` is a pure function: pass the env-var-seeded base Params and
// the current frame counter, get back a modulated copy. When `cfg.enabled` is
// false the base is returned unchanged, which is the default-off behaviour the
// pcsx-redux bridge relies on for backward compatibility.
//
// `depth` scales the LFO amplitude added on top of `base`. With base=0 and
// depth=0.7 a param swings between 0 and 0.7. Per-axis taming is applied
// internally so "loud" effects (missing / depth-delay / chaos) only get half
// the depth — otherwise auto mode would crush the image with drops.
//
// `rate` is a wall-clock-speed multiplier; rate=2 means LFOs run twice as
// fast.
//
// FilterParams is left untouched: filters are static "where am I aiming"
// configuration, not something to oscillate.
struct AutoModeParams {
    bool  enabled = false;
    float depth   = 0.7f;
    float rate    = 1.0f;
};

Params applyAutoMode(const Params& base, const AutoModeParams& cfg, int frame);

}
