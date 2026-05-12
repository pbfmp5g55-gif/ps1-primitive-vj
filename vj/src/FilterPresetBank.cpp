#include "vj/FilterPresetBank.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

namespace {

constexpr char     kBankMagic[8] = {'V', 'J', 'P', 'S', 'E', 'T', '0', '1'};
constexpr uint32_t kBankVersion  = 1;

template <typename T>
bool writeRaw(std::FILE* f, const T& v) {
    return std::fwrite(&v, sizeof(T), 1, f) == 1;
}

template <typename T>
bool readRaw(std::FILE* f, T& v) {
    return std::fread(&v, sizeof(T), 1, f) == 1;
}

}  // namespace

bool FilterPresetBank::saveTo(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(kBankMagic, 1, 8, f) == 8;
    if (ok) ok = writeRaw(f, kBankVersion);
    if (ok) {
        const uint32_t slotCount = static_cast<uint32_t>(kSlotCount);
        ok = writeRaw(f, slotCount);
    }
    for (int i = 0; ok && i < kSlotCount; ++i) {
        const FilterPreset& s = m_slots[static_cast<size_t>(i)];
        const uint32_t      nameLen = static_cast<uint32_t>(s.name.size());
        if (!writeRaw(f, nameLen)) { ok = false; break; }
        if (nameLen > 0 &&
            std::fwrite(s.name.data(), 1, nameLen, f) != nameLen) {
            ok = false; break;
        }
        const uint8_t  textured = s.params.texturedOnly ? 1 : 0;
        const uint8_t  pad[3]   = {0, 0, 0};
        if (!writeRaw(f, textured)) { ok = false; break; }
        if (std::fwrite(pad, 1, 3, f) != 3) { ok = false; break; }
        if (!writeRaw(f, s.params.minArea))  { ok = false; break; }
        if (!writeRaw(f, s.params.maxArea))  { ok = false; break; }
        if (!writeRaw(f, s.params.regionX0)) { ok = false; break; }
        if (!writeRaw(f, s.params.regionY0)) { ok = false; break; }
        if (!writeRaw(f, s.params.regionX1)) { ok = false; break; }
        if (!writeRaw(f, s.params.regionY1)) { ok = false; break; }
        const int32_t everyN = static_cast<int32_t>(s.params.everyN);
        if (!writeRaw(f, everyN)) { ok = false; break; }
    }
    std::fclose(f);
    return ok;
}

bool FilterPresetBank::loadFrom(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[8];
    if (std::fread(magic, 1, 8, f) != 8 ||
        std::memcmp(magic, kBankMagic, 8) != 0) {
        std::fclose(f); return false;
    }
    uint32_t version = 0;
    if (!readRaw(f, version) || version != kBankVersion) {
        std::fclose(f); return false;
    }
    uint32_t slotCount = 0;
    if (!readRaw(f, slotCount) || slotCount != static_cast<uint32_t>(kSlotCount)) {
        std::fclose(f); return false;
    }
    // Stage into a local bank so a partial read leaves the live one alone.
    std::array<FilterPreset, kSlotCount> staged;
    bool ok = true;
    for (int i = 0; ok && i < kSlotCount; ++i) {
        uint32_t nameLen = 0;
        if (!readRaw(f, nameLen) || nameLen > 4096) { ok = false; break; }
        std::string name;
        name.resize(nameLen);
        if (nameLen > 0 && std::fread(&name[0], 1, nameLen, f) != nameLen) {
            ok = false; break;
        }
        uint8_t textured = 0;
        uint8_t pad[3];
        if (!readRaw(f, textured))                  { ok = false; break; }
        if (std::fread(pad, 1, 3, f) != 3)          { ok = false; break; }
        FilterParams p;
        p.texturedOnly = textured != 0;
        if (!readRaw(f, p.minArea))  { ok = false; break; }
        if (!readRaw(f, p.maxArea))  { ok = false; break; }
        if (!readRaw(f, p.regionX0)) { ok = false; break; }
        if (!readRaw(f, p.regionY0)) { ok = false; break; }
        if (!readRaw(f, p.regionX1)) { ok = false; break; }
        if (!readRaw(f, p.regionY1)) { ok = false; break; }
        int32_t everyN = 0;
        if (!readRaw(f, everyN))     { ok = false; break; }
        p.everyN = static_cast<int>(everyN);
        staged[static_cast<size_t>(i)].name   = std::move(name);
        staged[static_cast<size_t>(i)].params = p;
    }
    std::fclose(f);
    if (!ok) return false;
    m_slots = std::move(staged);
    return true;
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
