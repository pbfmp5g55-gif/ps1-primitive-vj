#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

#include "vj/FilterPresetBank.h"
#include "vj/Params.h"

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

void test_slot_count_is_16() {
    assert(vj::FilterPresetBank::slotCount() == 16);
}

void test_default_bank_has_zeroed_params() {
    vj::FilterPresetBank bank;
    for (int i = 0; i < vj::FilterPresetBank::slotCount(); ++i) {
        const auto& p = bank.slot(i).params;
        assert(p.texturedOnly == false);
        assert(nearlyEqual(p.minArea, 0.0f));
        assert(nearlyEqual(p.maxArea, 0.0f));
        assert(nearlyEqual(p.regionX0, 0.0f));
        assert(nearlyEqual(p.regionY1, 0.0f));
        assert(p.everyN == 0);
    }
}

void test_slotForCC_spans_full_bank() {
    assert(vj::FilterPresetBank::slotForCC(0)   == 0);
    assert(vj::FilterPresetBank::slotForCC(7)   == 0);
    assert(vj::FilterPresetBank::slotForCC(8)   == 1);
    assert(vj::FilterPresetBank::slotForCC(127) == 15);
    // Out-of-range clamps.
    assert(vj::FilterPresetBank::slotForCC(-5)   == 0);
    assert(vj::FilterPresetBank::slotForCC(200)  == 15);
}

void test_snap_returns_exact_slot() {
    vj::FilterPresetBank bank;
    bank.slot(0).params.minArea  = 100.0f;
    bank.slot(7).params.minArea  = 700.0f;
    bank.slot(15).params.minArea = 15000.0f;
    assert(nearlyEqual(bank.selectSnap(0).minArea,   100.0f));
    assert(nearlyEqual(bank.selectSnap(60).minArea,  700.0f));  // 60/8 = 7
    assert(nearlyEqual(bank.selectSnap(127).minArea, 15000.0f));
}

void test_interpolation_at_endpoints_equals_endpoint_slot() {
    vj::FilterPresetBank bank;
    bank.slot(0).params.minArea  = 50.0f;
    bank.slot(15).params.minArea = 1500.0f;
    auto lo = bank.selectInterpolated(0);
    auto hi = bank.selectInterpolated(127);
    assert(nearlyEqual(lo.minArea, 50.0f));
    assert(nearlyEqual(hi.minArea, 1500.0f));
}

void test_interpolation_midway_lerps_numeric() {
    vj::FilterPresetBank bank;
    // Set every slot's minArea to its index * 1000.
    for (int i = 0; i < vj::FilterPresetBank::slotCount(); ++i) {
        bank.slot(i).params.minArea = static_cast<float>(i) * 1000.0f;
    }
    // CC value 0..127 maps to pos 0..15 linearly. Midpoint CC=63.5 → pos=7.5.
    // CC must be integer though; use 63 (pos = 63/127 * 15 ≈ 7.441) and 64.
    auto a = bank.selectInterpolated(63);
    // pos = 63/127 * 15 ≈ 7.4409; expected ≈ lerp(7000, 8000, 0.4409) ≈ 7440.9
    assert(a.minArea > 7000.0f && a.minArea < 8000.0f);
    // Exact midpoint (pos = 7.5) → cc such that t * 15 = 7.5 → t = 0.5 → cc = 63.5
    // not representable. But CC=63 should be < 7500, CC=64 > 7500.
    auto b = bank.selectInterpolated(64);
    assert(b.minArea > 7500.0f);
}

void test_bool_snaps_at_midpoint() {
    vj::FilterPresetBank bank;
    bank.slot(0).params.texturedOnly  = false;
    bank.slot(1).params.texturedOnly  = true;
    // CC values 0..8 map to pos 0..(8/127)*15 ≈ 0..0.945, all interpolating
    // between slot 0 and slot 1.
    // Need frac < 0.5 vs > 0.5 transition.
    // Looking for pos crossing 0.5. pos = (cc/127)*15. 0.5 = (cc/127)*15 → cc ≈ 4.23.
    auto lo = bank.selectInterpolated(0);   // pos = 0,    frac = 0    → slot 0 false
    auto mid = bank.selectInterpolated(5);  // pos ≈ 0.59, frac ≈ 0.59 → slot 1 true
    auto hi = bank.selectInterpolated(8);   // pos ≈ 0.94, frac ≈ 0.94 → slot 1 true
    assert(lo.texturedOnly == false);
    assert(mid.texturedOnly == true);
    assert(hi.texturedOnly == true);
}

void test_everyN_lerps_then_rounds() {
    vj::FilterPresetBank bank;
    bank.slot(0).params.everyN = 0;
    bank.slot(1).params.everyN = 8;
    // At pos ≈ 0.5 between slot 0 and 1, everyN should be 4 (rounded).
    // pos 0.5 corresponds to cc ≈ 4.23. Use cc=4 (pos ≈ 0.47, lerped ≈ 3.78 → rounds to 4)
    auto m = bank.selectInterpolated(4);
    assert(m.everyN == 4 || m.everyN == 3);  // tolerance for rounding direction
}

void test_save_load_roundtrip() {
    vj::FilterPresetBank a;
    for (int i = 0; i < vj::FilterPresetBank::slotCount(); ++i) {
        a.slot(i).name = "slot-" + std::to_string(i);
        a.slot(i).params.texturedOnly = (i % 2) != 0;
        a.slot(i).params.minArea  = static_cast<float>(i) * 100.0f;
        a.slot(i).params.maxArea  = static_cast<float>(i) * 200.0f;
        a.slot(i).params.regionX0 = static_cast<float>(i) + 0.5f;
        a.slot(i).params.regionY0 = static_cast<float>(i) + 1.5f;
        a.slot(i).params.regionX1 = static_cast<float>(i) + 2.5f;
        a.slot(i).params.regionY1 = static_cast<float>(i) + 3.5f;
        a.slot(i).params.everyN   = i % 16;
    }
    const std::string path = "test_fpb_roundtrip.bin";
    assert(a.saveTo(path));

    vj::FilterPresetBank b;
    assert(b.loadFrom(path));
    for (int i = 0; i < vj::FilterPresetBank::slotCount(); ++i) {
        const auto& x = a.slot(i);
        const auto& y = b.slot(i);
        assert(x.name == y.name);
        assert(x.params.texturedOnly == y.params.texturedOnly);
        assert(nearlyEqual(x.params.minArea,  y.params.minArea));
        assert(nearlyEqual(x.params.maxArea,  y.params.maxArea));
        assert(nearlyEqual(x.params.regionX0, y.params.regionX0));
        assert(nearlyEqual(x.params.regionY0, y.params.regionY0));
        assert(nearlyEqual(x.params.regionX1, y.params.regionX1));
        assert(nearlyEqual(x.params.regionY1, y.params.regionY1));
        assert(x.params.everyN == y.params.everyN);
    }
    std::remove(path.c_str());
}

void test_load_missing_file_keeps_bank() {
    vj::FilterPresetBank bank;
    bank.slot(0).name = "preserved";
    assert(!bank.loadFrom("definitely-not-a-real-path-9b2c.bin"));
    assert(bank.slot(0).name == "preserved");
}

void test_load_bad_magic_keeps_bank() {
    const std::string path = "test_fpb_badmagic.bin";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    assert(f);
    const char junk[12] = {'N','O','P','E','0','0','0','0', 1,0,0,0};
    std::fwrite(junk, 1, 12, f);
    std::fclose(f);

    vj::FilterPresetBank bank;
    bank.slot(5).name = "preserved";
    assert(!bank.loadFrom(path));
    assert(bank.slot(5).name == "preserved");
    std::remove(path.c_str());
}

void test_slot_mutation_persists() {
    vj::FilterPresetBank bank;
    bank.slot(3).name = "BG only";
    bank.slot(3).params.minArea = 20000.0f;
    assert(bank.slot(3).name == "BG only");
    assert(nearlyEqual(bank.slot(3).params.minArea, 20000.0f));
    // Other slots untouched.
    assert(bank.slot(2).name.empty());
    assert(nearlyEqual(bank.slot(2).params.minArea, 0.0f));
}

}  // namespace

int main() {
    test_slot_count_is_16();
    test_default_bank_has_zeroed_params();
    test_slotForCC_spans_full_bank();
    test_snap_returns_exact_slot();
    test_interpolation_at_endpoints_equals_endpoint_slot();
    test_interpolation_midway_lerps_numeric();
    test_bool_snaps_at_midpoint();
    test_everyN_lerps_then_rounds();
    test_save_load_roundtrip();
    test_load_missing_file_keeps_bank();
    test_load_bad_magic_keeps_bank();
    test_slot_mutation_persists();
    std::fprintf(stderr, "[test_filter_preset_bank] OK\n");
    return 0;
}
