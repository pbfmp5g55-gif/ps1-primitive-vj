#include "vj/RandomController.h"

#include <algorithm>
#include <cmath>

namespace vj {

RandomController::RandomController(uint32_t seed) : m_rng(seed) {
    state.seed = seed;
}

float RandomController::rand01() {
    std::uniform_real_distribution<float> d(0.0f, 1.0f);
    return d(m_rng);
}

float RandomController::randSigned() {
    std::uniform_real_distribution<float> d(-1.0f, 1.0f);
    return d(m_rng);
}

int RandomController::randInt(int minVal, int maxVal) {
    if (maxVal < minVal) std::swap(minVal, maxVal);
    std::uniform_int_distribution<int> d(minVal, maxVal);
    return d(m_rng);
}

void RandomController::update(float chaos) {
    if (m_framesUntilUpdate <= 0) {
        m_holdFrames = std::clamp(
            static_cast<int>(std::round(10.0f - chaos * 7.0f)), 3, 10);
        rerollState(chaos);
        m_framesUntilUpdate = m_holdFrames;
    } else {
        m_framesUntilUpdate--;
    }
}

void RandomController::rerollState(float chaos) {
    state.seed          = static_cast<uint32_t>(m_rng());
    state.geometryBiasX = randSigned() * 0.5f;
    state.geometryBiasY = randSigned() * 0.5f;
    state.uvBiasU       = randSigned() * 0.5f;
    state.uvBiasV       = randSigned() * 0.5f;
    state.colorBiasR    = randSigned() * 0.5f;
    state.colorBiasG    = randSigned() * 0.5f;
    state.colorBiasB    = randSigned() * 0.5f;
    state.dropBias      = (rand01() - 0.5f) * 0.4f;
    state.depthBias     = randSigned() * 0.3f;
    state.spikeChance   = chaos * 0.2f;

    if (rand01() < state.spikeChance) {
        state.geometryBiasX *= 2.0f;
        state.geometryBiasY *= 2.0f;
        state.uvBiasU       *= 1.5f;
        state.uvBiasV       *= 1.5f;
        state.dropBias      += 0.2f;
    }
}

}
