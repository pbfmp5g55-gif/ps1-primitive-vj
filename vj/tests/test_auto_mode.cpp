#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

#include "vj/AutoMode.h"
#include "vj/Params.h"

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

void test_disabled_returns_base_unchanged() {
    vj::Params base;
    base.master = 0.4f;
    base.chance = 0.5f;
    vj::AutoModeParams cfg;
    cfg.enabled = false;
    cfg.depth = 0.9f;
    auto out = vj::applyAutoMode(base, cfg, 12345);
    assert(nearlyEqual(out.master, 0.4f));
    assert(nearlyEqual(out.chance, 0.5f));
}

void test_enabled_modulates_some_axis() {
    vj::Params base;  // all zero
    vj::AutoModeParams cfg;
    cfg.enabled = true;
    cfg.depth = 0.7f;

    // Sample a range of frames; at least some should show non-base values on
    // at least one axis (otherwise the LFO is dead).
    bool seenChange = false;
    for (int f = 0; f < 600; ++f) {
        auto out = vj::applyAutoMode(base, cfg, f);
        if (out.master   > 0.01f) seenChange = true;
        if (out.chance   > 0.01f) seenChange = true;
        if (out.geometry > 0.01f) seenChange = true;
        if (out.texture  > 0.01f) seenChange = true;
        if (out.color    > 0.01f) seenChange = true;
        if (seenChange) break;
    }
    assert(seenChange);
}

void test_modulation_stays_in_unit_range() {
    vj::Params base;
    base.master = 0.5f;
    vj::AutoModeParams cfg;
    cfg.enabled = true;
    cfg.depth = 1.0f;

    for (int f = 0; f < 2000; f += 7) {
        auto out = vj::applyAutoMode(base, cfg, f);
        assert(out.master   >= 0.0f && out.master   <= 1.0f);
        assert(out.chance   >= 0.0f && out.chance   <= 1.0f);
        assert(out.geometry >= 0.0f && out.geometry <= 1.0f);
        assert(out.texture  >= 0.0f && out.texture  <= 1.0f);
        assert(out.missing  >= 0.0f && out.missing  <= 1.0f);
        assert(out.color    >= 0.0f && out.color    <= 1.0f);
        assert(out.depth    >= 0.0f && out.depth    <= 1.0f);
        assert(out.chaos    >= 0.0f && out.chaos    <= 1.0f);
    }
}

void test_filter_axes_pass_through_untouched() {
    vj::Params base;
    base.filter.texturedOnly = true;
    base.filter.minArea = 1234.0f;
    base.filter.maxArea = 5678.0f;
    base.filter.regionX0 = 1.0f;
    base.filter.regionY0 = 2.0f;
    base.filter.regionX1 = 3.0f;
    base.filter.regionY1 = 4.0f;
    base.filter.everyN = 7;

    vj::AutoModeParams cfg;
    cfg.enabled = true;
    cfg.depth = 0.9f;

    auto out = vj::applyAutoMode(base, cfg, 100);
    assert(out.filter.texturedOnly == true);
    assert(nearlyEqual(out.filter.minArea, 1234.0f));
    assert(nearlyEqual(out.filter.maxArea, 5678.0f));
    assert(nearlyEqual(out.filter.regionX0, 1.0f));
    assert(nearlyEqual(out.filter.regionY1, 4.0f));
    assert(out.filter.everyN == 7);
}

void test_base_is_floor_lfo_only_adds() {
    vj::Params base;
    base.master = 0.3f;
    vj::AutoModeParams cfg;
    cfg.enabled = true;
    cfg.depth = 0.5f;

    // The LFO output sits in [0, 1], so modulate adds [0, depth] on top of
    // base, then clamps. Final master must therefore be in [base, base+depth]
    // truncated to [0, 1].
    float lo = 1.0f, hi = 0.0f;
    for (int f = 0; f < 2000; f += 1) {
        auto out = vj::applyAutoMode(base, cfg, f);
        if (out.master < lo) lo = out.master;
        if (out.master > hi) hi = out.master;
    }
    assert(lo >= base.master - 1e-3f);
    assert(hi <= std::min(1.0f, base.master + cfg.depth) + 1e-3f);
    // The LFO actually moves the value, not constant.
    assert(hi - lo > 0.1f);
}

void test_rate_scales_speed() {
    vj::Params base;
    vj::AutoModeParams slow;
    slow.enabled = true;
    slow.depth = 0.7f;
    slow.rate = 1.0f;

    vj::AutoModeParams fast = slow;
    fast.rate = 4.0f;

    // Sample average abs change in master across 240 frames.
    auto avgAbsDelta = [&](const vj::AutoModeParams& cfg) {
        float prev = vj::applyAutoMode(base, cfg, 0).master;
        float total = 0.0f;
        int n = 0;
        for (int f = 1; f < 240; ++f) {
            float v = vj::applyAutoMode(base, cfg, f).master;
            total += std::fabs(v - prev);
            prev = v;
            ++n;
        }
        return total / static_cast<float>(n);
    };

    float dSlow = avgAbsDelta(slow);
    float dFast = avgAbsDelta(fast);
    // Faster rate => more change per frame on average.
    assert(dFast > dSlow);
}

}  // namespace

int main() {
    test_disabled_returns_base_unchanged();
    test_enabled_modulates_some_axis();
    test_modulation_stays_in_unit_range();
    test_filter_axes_pass_through_untouched();
    test_base_is_floor_lfo_only_adds();
    test_rate_scales_speed();
    std::fprintf(stderr, "[test_auto_mode] OK\n");
    return 0;
}
