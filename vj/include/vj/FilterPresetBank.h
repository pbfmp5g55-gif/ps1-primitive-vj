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

    // Serialize the whole bank (slot names + FilterParams) to a binary file.
    // Format: magic "VJPSET01" + version[4] + slotCount[4] + per slot
    // (nameLen[4] + name + texturedOnly[1] + _pad[3] + 6 floats + everyN[4]).
    // Returns false on filesystem failure.
    bool saveTo(const std::string& path) const;

    // Load a bank from a file written by saveTo(). On success, overwrites all
    // 16 slots. On failure (missing file / bad format / version mismatch),
    // leaves the bank unchanged and returns false.
    bool loadFrom(const std::string& path);

   private:
    std::array<FilterPreset, kSlotCount> m_slots;
};

}  // namespace vj
