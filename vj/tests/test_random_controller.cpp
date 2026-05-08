#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

#include "vj/PrimitiveInterceptor.h"  // for computeHoldFrames
#include "vj/RandomController.h"

namespace {

void test_rand01_in_unit_interval() {
    vj::RandomController rc(123u);
    for (int i = 0; i < 1000; ++i) {
        float v = rc.rand01();
        assert(v >= 0.0f);
        assert(v < 1.0f);
    }
}

void test_randSigned_in_minus_one_to_one() {
    vj::RandomController rc(456u);
    for (int i = 0; i < 1000; ++i) {
        float v = rc.randSigned();
        assert(v >= -1.0f);
        assert(v < 1.0f);
    }
}

void test_randInt_within_bounds() {
    vj::RandomController rc(789u);
    for (int i = 0; i < 1000; ++i) {
        int v = rc.randInt(5, 12);
        assert(v >= 5);
        assert(v <= 12);
    }
}

void test_holdFrames_and_reroll_advance_with_update() {
    vj::RandomController rc(0xDEADBEEFu);
    int prevHold = rc.holdFrames();
    int frames = 0;
    bool sawHoldChange = false;
    for (int i = 0; i < 200 && !sawHoldChange; ++i) {
        rc.update(0.5f);
        frames++;
        if (rc.holdFrames() != prevHold) sawHoldChange = true;
    }
    assert(sawHoldChange);
    (void)frames;
}

void test_computeHoldFrames_chaos_extremes() {
    int hLow  = vj::computeHoldFrames(0.0f);
    int hMid  = vj::computeHoldFrames(0.5f);
    int hHigh = vj::computeHoldFrames(1.0f);
    assert(hLow > 0);
    assert(hMid > 0);
    assert(hHigh > 0);
}

void test_seeded_repro() {
    vj::RandomController a(42u);
    vj::RandomController b(42u);
    for (int i = 0; i < 50; ++i) {
        assert(a.rand01() == b.rand01());
    }
}

}  // namespace

int main() {
    test_rand01_in_unit_interval();
    test_randSigned_in_minus_one_to_one();
    test_randInt_within_bounds();
    test_holdFrames_and_reroll_advance_with_update();
    test_computeHoldFrames_chaos_extremes();
    test_seeded_repro();
    std::fprintf(stderr, "[test_random_controller] OK\n");
    return 0;
}
