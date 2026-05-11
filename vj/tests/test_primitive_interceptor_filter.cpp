#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <vector>

#include "vj/Params.h"
#include "vj/Primitive.h"
#include "vj/PrimitiveInterceptor.h"

namespace {

// Build a Primitive with a single bounding-box footprint so that
// PrimitiveContext::screenArea = w*h and centerX/centerY are at the rect
// midpoint. `tex` toggles the `textured` flag.
vj::Primitive makeRect(float x, float y, float w, float h, bool tex) {
    vj::Primitive p;
    p.kind = vj::PrimitiveKind::Quad;
    p.textured = tex;
    p.vertices.resize(4);
    p.vertices[0].x = x;           p.vertices[0].y = y;
    p.vertices[1].x = x + w;       p.vertices[1].y = y;
    p.vertices[2].x = x;           p.vertices[2].y = y + h;
    p.vertices[3].x = x + w;       p.vertices[3].y = y + h;
    return p;
}

// Wire an interceptor with the given params and count how many of the supplied
// primitives reach the SubmitFn AFTER passing through glitch (i.e. were
// "affected" — this is the only signal exposed via the public API). Returns
// affectedCount(). With chance=1 and all effect coefficients at 0, an affected
// primitive is functionally indistinguishable from a passed-through one, but
// affectedCount() lets us count exactly which prims passed the filter+chance
// gate.
int runAndCountAffected(vj::Params params, const std::vector<vj::Primitive>& prims) {
    vj::PrimitiveInterceptor ic;
    ic.setSubmitCallback([](const vj::Primitive&) {});
    ic.beginFrame(params, static_cast<int>(prims.size()));
    for (const auto& p : prims) ic.interceptAndSubmit(p);
    ic.endFrame();
    return ic.affectedCount();
}

vj::Params baseParams() {
    vj::Params p;
    p.master = 1.0f;
    p.chance = 1.0f; // ensure chance gate always passes when filter passes
    return p;
}

void test_no_filter_lets_everything_through() {
    auto params = baseParams();
    std::vector<vj::Primitive> prims = {
        makeRect(0, 0, 10, 10, false),
        makeRect(50, 50, 100, 100, true),
    };
    assert(runAndCountAffected(params, prims) == 2);
}

void test_textured_only_rejects_non_textured() {
    auto params = baseParams();
    params.filter.texturedOnly = true;
    std::vector<vj::Primitive> prims = {
        makeRect(0, 0, 10, 10, false),    // not textured -> rejected
        makeRect(0, 0, 10, 10, true),     // textured     -> affected
        makeRect(0, 0, 10, 10, false),    // not textured -> rejected
    };
    assert(runAndCountAffected(params, prims) == 1);
}

void test_min_area_rejects_small_prims() {
    auto params = baseParams();
    params.filter.minArea = 500.0f;
    std::vector<vj::Primitive> prims = {
        makeRect(0, 0, 10, 10, true),     // area 100  -> rejected
        makeRect(0, 0, 30, 30, true),     // area 900  -> affected
        makeRect(0, 0, 20, 20, true),     // area 400  -> rejected
        makeRect(0, 0, 50, 50, true),     // area 2500 -> affected
    };
    assert(runAndCountAffected(params, prims) == 2);
}

void test_max_area_rejects_large_prims() {
    auto params = baseParams();
    params.filter.maxArea = 500.0f;
    std::vector<vj::Primitive> prims = {
        makeRect(0, 0, 10, 10, true),     // area 100  -> affected
        makeRect(0, 0, 30, 30, true),     // area 900  -> rejected
        makeRect(0, 0, 20, 20, true),     // area 400  -> affected
        makeRect(0, 0, 50, 50, true),     // area 2500 -> rejected
    };
    assert(runAndCountAffected(params, prims) == 2);
}

void test_min_and_max_area_form_band() {
    auto params = baseParams();
    params.filter.minArea = 200.0f;
    params.filter.maxArea = 1000.0f;
    std::vector<vj::Primitive> prims = {
        makeRect(0, 0, 10, 10, true),     // 100   -> rejected
        makeRect(0, 0, 20, 20, true),     // 400   -> affected
        makeRect(0, 0, 30, 30, true),     // 900   -> affected
        makeRect(0, 0, 40, 40, true),     // 1600  -> rejected
    };
    assert(runAndCountAffected(params, prims) == 2);
}

void test_region_filter_restricts_to_rect() {
    auto params = baseParams();
    params.filter.regionX0 = 100.0f;
    params.filter.regionY0 = 100.0f;
    params.filter.regionX1 = 200.0f;
    params.filter.regionY1 = 200.0f;
    std::vector<vj::Primitive> prims = {
        makeRect(0,   0,   10, 10, true), // center (5,5)       -> rejected
        makeRect(140, 140, 10, 10, true), // center (145,145)   -> affected
        makeRect(180, 180, 10, 10, true), // center (185,185)   -> affected
        makeRect(250, 250, 10, 10, true), // center (255,255)   -> rejected
    };
    assert(runAndCountAffected(params, prims) == 2);
}

void test_region_filter_disabled_when_inverted_or_zero() {
    auto params = baseParams();
    // Default values (all zero) => region check disabled => all pass chance.
    std::vector<vj::Primitive> prims = {
        makeRect(0,   0,   10, 10, true),
        makeRect(500, 500, 10, 10, true),
    };
    assert(runAndCountAffected(params, prims) == 2);

    // Inverted bounds also count as disabled.
    params.filter.regionX0 = 100.0f;
    params.filter.regionY0 = 100.0f;
    params.filter.regionX1 = 50.0f; // x1 < x0
    params.filter.regionY1 = 50.0f; // y1 < y0
    assert(runAndCountAffected(params, prims) == 2);
}

void test_every_n_selects_one_in_n() {
    auto params = baseParams();
    params.filter.everyN = 3;
    std::vector<vj::Primitive> prims;
    for (int i = 0; i < 9; ++i) prims.push_back(makeRect(0, 0, 10, 10, true));
    // counter values 1..9; pass when counter%3==0 -> 3,6,9 -> 3 prims affected
    assert(runAndCountAffected(params, prims) == 3);
}

void test_every_n_zero_means_no_filter() {
    auto params = baseParams();
    params.filter.everyN = 0;
    std::vector<vj::Primitive> prims;
    for (int i = 0; i < 5; ++i) prims.push_back(makeRect(0, 0, 10, 10, true));
    assert(runAndCountAffected(params, prims) == 5);
}

void test_every_n_resets_each_frame() {
    auto params = baseParams();
    params.filter.everyN = 2;

    vj::PrimitiveInterceptor ic;
    ic.setSubmitCallback([](const vj::Primitive&) {});

    std::vector<vj::Primitive> prims;
    for (int i = 0; i < 4; ++i) prims.push_back(makeRect(0, 0, 10, 10, true));

    // Frame 1: counter 1..4; pass at 2,4 -> 2 affected.
    ic.beginFrame(params, 4);
    for (const auto& p : prims) ic.interceptAndSubmit(p);
    ic.endFrame();
    assert(ic.affectedCount() == 2);

    // Frame 2: counter resets, same pattern. The total affected count is
    // cumulative across frames in this API, so we expect +2 more.
    ic.beginFrame(params, 4);
    // affectedCount is reset by beginFrame; verify.
    assert(ic.affectedCount() == 0);
    for (const auto& p : prims) ic.interceptAndSubmit(p);
    ic.endFrame();
    assert(ic.affectedCount() == 2);
}

void test_filters_combine_with_and() {
    auto params = baseParams();
    params.filter.texturedOnly = true;
    params.filter.minArea = 200.0f;
    std::vector<vj::Primitive> prims = {
        makeRect(0, 0, 10, 10, true),     // textured, area 100   -> rejected (area)
        makeRect(0, 0, 20, 20, false),    // !textured, area 400  -> rejected (textured)
        makeRect(0, 0, 20, 20, true),     // textured, area 400   -> affected
        makeRect(0, 0, 30, 30, true),     // textured, area 900   -> affected
        makeRect(0, 0, 30, 30, false),    // !textured, area 900  -> rejected (textured)
    };
    assert(runAndCountAffected(params, prims) == 2);
}

}  // namespace

int main() {
    test_no_filter_lets_everything_through();
    test_textured_only_rejects_non_textured();
    test_min_area_rejects_small_prims();
    test_max_area_rejects_large_prims();
    test_min_and_max_area_form_band();
    test_region_filter_restricts_to_rect();
    test_region_filter_disabled_when_inverted_or_zero();
    test_every_n_selects_one_in_n();
    test_every_n_zero_means_no_filter();
    test_every_n_resets_each_frame();
    test_filters_combine_with_and();
    std::fprintf(stderr, "[test_primitive_interceptor_filter] OK\n");
    return 0;
}
