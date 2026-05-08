#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>

#include "vj/SafetyLimiter.h"

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

void test_mustDraw_with_no_estimate_returns_true() {
    vj::SafetyLimiter s;
    s.beginFrame(0);
    assert(s.mustDraw());
}

void test_counts_advance_via_notify() {
    vj::SafetyLimiter s;
    s.beginFrame(10);
    assert(s.totalCount() == 10);
    assert(s.drawnCount() == 0);

    s.notifyDraw();
    s.notifyDraw();
    s.notifySkip();
    s.notifyForcedDraw();

    assert(s.drawnCount() == 2);
    assert(s.skippedCount() == 1);
    assert(s.forcedDrawCount() == 1);
}

void test_lowMasterSafety_boundary_cases() {
    assert(nearlyEqual(vj::lowMasterSafety(-0.1f), 0.0f));
    assert(nearlyEqual(vj::lowMasterSafety(0.0f), 0.0f));
    assert(nearlyEqual(vj::lowMasterSafety(0.15f), 0.5f));
    assert(nearlyEqual(vj::lowMasterSafety(0.3f), 1.0f));
    assert(nearlyEqual(vj::lowMasterSafety(0.5f), 1.0f));
    assert(nearlyEqual(vj::lowMasterSafety(1.0f), 1.0f));
}

void test_lowMasterSafety_monotonic_below_threshold() {
    float prev = -1.0f;
    for (int i = 0; i <= 30; ++i) {
        float m = static_cast<float>(i) / 100.0f;
        float s = vj::lowMasterSafety(m);
        assert(s >= prev);
        prev = s;
    }
}

void test_mustDraw_triggers_when_remaining_capacity_low() {
    vj::SafetyLimiter s;
    s.beginFrame(100);
    for (int i = 0; i < 5; ++i) s.notifyDraw();
    for (int i = 0; i < 90; ++i) s.notifySkip();
    assert(s.mustDraw());
}

}  // namespace

int main() {
    test_mustDraw_with_no_estimate_returns_true();
    test_counts_advance_via_notify();
    test_lowMasterSafety_boundary_cases();
    test_lowMasterSafety_monotonic_below_threshold();
    test_mustDraw_triggers_when_remaining_capacity_low();
    std::fprintf(stderr, "[test_safety_limiter] OK\n");
    return 0;
}
