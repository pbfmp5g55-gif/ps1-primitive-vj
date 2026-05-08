#pragma once

namespace vj {

struct Params {
    float master   = 0.0f;
    float chance   = 0.0f;
    float geometry = 0.0f;
    float texture  = 0.0f;
    float missing  = 0.0f;
    float color    = 0.0f;
    float depth    = 0.0f;
    float chaos    = 0.0f;
};

inline float curveSoft(float x) { return x * x; }

}
