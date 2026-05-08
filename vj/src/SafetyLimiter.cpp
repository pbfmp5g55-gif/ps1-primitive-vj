#include "vj/SafetyLimiter.h"

#include <algorithm>

namespace vj {

void SafetyLimiter::beginFrame(int totalEstimated) {
    m_total   = std::max(0, totalEstimated);
    m_drawn   = 0;
    m_skipped = 0;
    m_forced  = 0;
}

bool SafetyLimiter::mustDraw() const {
    if (m_total <= 0) return true;
    const float drawnRate     = static_cast<float>(m_drawn) / static_cast<float>(m_total);
    const float remainingRate = 1.0f - (static_cast<float>(m_drawn + m_skipped) / static_cast<float>(m_total));
    return drawnRate + remainingRate <= 0.2f;
}

void SafetyLimiter::notifyDraw()       { m_drawn++; }
void SafetyLimiter::notifySkip()       { m_skipped++; }
void SafetyLimiter::notifyForcedDraw() { m_forced++; }

float lowMasterSafety(float master) {
    if (master >= 0.3f) return 1.0f;
    if (master <= 0.0f) return 0.0f;
    return master / 0.3f;
}

}
