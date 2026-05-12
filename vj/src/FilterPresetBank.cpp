#include "vj/FilterPresetBank.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace vj {

namespace {

int clampCC(int cc) {
    if (cc < 0) return 0;
    if (cc > 127) return 127;
    return cc;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace

FilterPresetBank::FilterPresetBank() = default;

const FilterPreset& FilterPresetBank::slot(int index) const {
    assert(index >= 0 && index < kSlotCount);
    return m_slots[static_cast<size_t>(index)];
}

FilterPreset& FilterPresetBank::slot(int index) {
    assert(index >= 0 && index < kSlotCount);
    return m_slots[static_cast<size_t>(index)];
}

int FilterPresetBank::slotForCC(int cc) {
    const int c = clampCC(cc);
    // 128 CC values → 16 slots → 8 CC values per slot.
    int idx = c / (128 / kSlotCount);
    if (idx >= kSlotCount) idx = kSlotCount - 1;
    return idx;
}

const FilterParams& FilterPresetBank::selectSnap(int cc) const {
    return slot(slotForCC(cc)).params;
}

FilterParams FilterPresetBank::selectInterpolated(int cc) const {
    const float t   = clampCC(cc) / 127.0f;                  // 0..1
    const float pos = t * static_cast<float>(kSlotCount - 1);
    int   idxLo     = static_cast<int>(std::floor(pos));
    int   idxHi     = idxLo + 1;
    float frac      = pos - static_cast<float>(idxLo);
    if (idxLo < 0) { idxLo = 0; idxHi = 0; frac = 0.0f; }
    if (idxHi >= kSlotCount) { idxHi = kSlotCount - 1; idxLo = idxHi; frac = 0.0f; }

    const FilterParams& a = m_slots[static_cast<size_t>(idxLo)].params;
    const FilterParams& b = m_slots[static_cast<size_t>(idxHi)].params;

    FilterParams out;
    out.texturedOnly = (frac < 0.5f) ? a.texturedOnly : b.texturedOnly;
    out.minArea      = lerp(a.minArea,  b.minArea,  frac);
    out.maxArea      = lerp(a.maxArea,  b.maxArea,  frac);
    out.regionX0     = lerp(a.regionX0, b.regionX0, frac);
    out.regionY0     = lerp(a.regionY0, b.regionY0, frac);
    out.regionX1     = lerp(a.regionX1, b.regionX1, frac);
    out.regionY1     = lerp(a.regionY1, b.regionY1, frac);
    out.everyN       = static_cast<int>(std::lround(
        lerp(static_cast<float>(a.everyN), static_cast<float>(b.everyN), frac)));
    return out;
}

}  // namespace vj
