#pragma once

#include <array>
#include <string>

#include "Params.h"

namespace vj {

// FilterPresetBank: a fixed-size bank of named FilterParams snapshots that
// can be selected (or blended) by a single MIDI CC value. Designed for VJ
// performance use: a single knob sweeps the full bank, optionally
// interpolating between neighbouring slots so the filter morphs smoothly
// instead of hard-cutting.
//
// Slot count is fixed at 16. With 128 CC values that's 8 CC steps per slot
// (or, with interpolation enabled, a smooth float position along [0, 15]).
//
// Boolean axes (FilterParams::texturedOnly) cannot be linearly interpolated;
// when blending they snap at the midpoint between slots. Numeric axes
// (areas, region, everyN) lerp linearly. everyN is rounded to int.

struct FilterPreset {
    std::string  name;
    FilterParams params;
};

class FilterPresetBank {
   public:
    static constexpr int kSlotCount = 16;

    FilterPresetBank();

    static constexpr int slotCount() { return kSlotCount; }

    const FilterPreset& slot(int index) const;
    FilterPreset&       slot(int index);

    // Slot index that CC value (clamped to 0..127) corresponds to under hard
    // snap-mode: returns 0..kSlotCount-1.
    static int slotForCC(int cc);

    // Linear blend between the two slots that CC value sits between.
    // cc is clamped to 0..127. Booleans snap at midpoint.
    FilterParams selectInterpolated(int cc) const;

    // Snap variant — returns slot(slotForCC(cc)).params exactly.
    const FilterParams& selectSnap(int cc) const;

   private:
    std::array<FilterPreset, kSlotCount> m_slots;
};

}  // namespace vj
