#pragma once

namespace vj {

// FilterParams: early-reject prims from glitch effects (the rejected prim is
// passed through unmodified). Each axis is disabled when the corresponding
// member is at its default value, so the all-zero default lets every prim
// through and matches pre-filter behaviour.
//
// Axes combine with AND: a prim must pass every enabled axis to be eligible.
// Region coordinates are in raw GPU pixel space (same coord system as
// PrimitiveContext::centerX / centerY); region filter is disabled when
// regionX1 <= regionX0 or regionY1 <= regionY0.
// everyN selects 1 out of N prims per frame (counter resets in beginFrame).
struct FilterParams {
    bool  texturedOnly = false;
    float minArea      = 0.0f;
    float maxArea      = 0.0f;
    float regionX0     = 0.0f;
    float regionY0     = 0.0f;
    float regionX1     = 0.0f;
    float regionY1     = 0.0f;
    int   everyN       = 0;
};

struct Params {
    float master   = 0.0f;
    float chance   = 0.0f;
    float geometry = 0.0f;
    float texture  = 0.0f;
    float missing  = 0.0f;
    float color    = 0.0f;
    float depth    = 0.0f;
    float chaos    = 0.0f;
    FilterParams filter;
};

inline float curveSoft(float x) { return x * x; }

}
