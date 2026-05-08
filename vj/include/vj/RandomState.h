#pragma once

#include <cstdint>

namespace vj {

struct RandomState {
    uint32_t seed         = 0;
    float geometryBiasX   = 0.0f;
    float geometryBiasY   = 0.0f;
    float uvBiasU         = 0.0f;
    float uvBiasV         = 0.0f;
    float colorBiasR      = 0.0f;
    float colorBiasG      = 0.0f;
    float colorBiasB      = 0.0f;
    float dropBias        = 0.0f;
    float depthBias       = 0.0f;
    float spikeChance     = 0.0f;
};

}
