#pragma once

#include <cstdint>
#include <random>

#include "RandomState.h"

namespace vj {

class RandomController {
public:
    RandomState state;

    explicit RandomController(uint32_t seed = 0xC0DEC0DEu);

    void update(float chaos);

    float rand01();
    float randSigned();
    int randInt(int minVal, int maxVal);

    int holdFrames() const { return m_holdFrames; }
    int framesUntilNextReroll() const { return m_framesUntilUpdate; }

private:
    std::mt19937 m_rng;
    int m_holdFrames = 5;
    int m_framesUntilUpdate = 0;

    void rerollState(float chaos);
};

}
