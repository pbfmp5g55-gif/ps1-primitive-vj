#include "vj/PrimitiveStream.h"

#include <cstdint>
#include <cstring>

namespace vj {

namespace {

constexpr char     kMagicV1[8] = {'V', 'J', 'R', 'E', 'C', '0', '0', '1'};
constexpr char     kMagicV2[8] = {'V', 'J', 'R', 'E', 'C', '0', '0', '2'};
constexpr uint32_t kVersionV1  = 1;
constexpr uint32_t kVersionV2  = 2;
constexpr uint8_t  kFrameMarker[4] = {0xFE, 0xFE, 0xFE, 0xFE};
constexpr uint8_t  kEndMarker[4]   = {0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint8_t  kRecPrim   = 0;
constexpr uint8_t  kRecUpload = 1;

template <typename T>
bool writeRaw(std::FILE* f, const T& v) {
    return std::fwrite(&v, sizeof(T), 1, f) == 1;
}

template <typename T>
bool readRaw(std::FILE* f, T& v) {
    return std::fread(&v, sizeof(T), 1, f) == 1;
}

}  // namespace

// ============================================================================
// PrimitiveStreamWriter
// ============================================================================

PrimitiveStreamWriter::~PrimitiveStreamWriter() { close(); }

bool PrimitiveStreamWriter::open(const std::string& path) {
    close();
    m_file = std::fopen(path.c_str(), "wb");
    if (!m_file) return false;
    // Writer always emits v2; the reader handles both.
    if (std::fwrite(kMagicV2, 1, 8, m_file) != 8) { close(); return false; }
    if (!writeRaw(m_file, kVersionV2))            { close(); return false; }
    return true;
}

void PrimitiveStreamWriter::close() {
    if (!m_file) return;
    std::fwrite(kEndMarker, 1, 4, m_file);
    std::fclose(m_file);
    m_file = nullptr;
}

void PrimitiveStreamWriter::beginFrame(int frameIndex, int recordCount) {
    if (!m_file) return;
    std::fwrite(kFrameMarker, 1, 4, m_file);
    const uint32_t f = static_cast<uint32_t>(frameIndex);
    const uint32_t c = static_cast<uint32_t>(recordCount);
    writeRaw(m_file, f);
    writeRaw(m_file, c);
}

void PrimitiveStreamWriter::writePrimitive(const Primitive& p) {
    if (!m_file) return;
    writeRaw(m_file, kRecPrim);
    const uint8_t kind        = static_cast<uint8_t>(p.kind);
    const uint8_t textured    = p.textured ? 1 : 0;
    const uint8_t vertexCount = static_cast<uint8_t>(p.vertices.size());
    // Previously a padding byte; from now on stores blendMode. Old
    // readers see this as 0 (Opaque) when reading files we write here
    // and we read 0 as Opaque from older files — round-trip safe.
    const uint8_t blendMode   = static_cast<uint8_t>(p.blendMode);
    writeRaw(m_file, kind);
    writeRaw(m_file, textured);
    writeRaw(m_file, vertexCount);
    writeRaw(m_file, blendMode);
    const uint64_t tag = p.hostTag;
    writeRaw(m_file, tag);
    for (const auto& v : p.vertices) {
        writeRaw(m_file, v.x);
        writeRaw(m_file, v.y);
        writeRaw(m_file, v.u);
        writeRaw(m_file, v.v);
        writeRaw(m_file, v.r);
        writeRaw(m_file, v.g);
        writeRaw(m_file, v.b);
        writeRaw(m_file, v.a);
    }
}

void PrimitiveStreamWriter::writeVRAMUpload(const VRAMUpload& u) {
    if (!m_file) return;
    writeRaw(m_file, kRecUpload);
    const uint16_t x = static_cast<uint16_t>(u.x);
    const uint16_t y = static_cast<uint16_t>(u.y);
    const uint16_t w = static_cast<uint16_t>(u.w);
    const uint16_t h = static_cast<uint16_t>(u.h);
    writeRaw(m_file, x);
    writeRaw(m_file, y);
    writeRaw(m_file, w);
    writeRaw(m_file, h);
    const size_t expected = static_cast<size_t>(u.w) * static_cast<size_t>(u.h);
    const size_t actual   = u.data.size();
    const size_t n        = (actual < expected) ? actual : expected;
    if (n > 0) {
        std::fwrite(u.data.data(), sizeof(uint16_t), n, m_file);
    }
    // If u.data was short, pad with zeros so the file layout matches w*h.
    if (n < expected) {
        const uint16_t zero = 0;
        for (size_t i = n; i < expected; ++i) {
            std::fwrite(&zero, sizeof(zero), 1, m_file);
        }
    }
}

// ============================================================================
// PrimitiveStreamReader
// ============================================================================

PrimitiveStreamReader::~PrimitiveStreamReader() { close(); }

bool PrimitiveStreamReader::open(const std::string& path) {
    close();
    m_file = std::fopen(path.c_str(), "rb");
    if (!m_file) return false;
    char magic[8];
    if (std::fread(magic, 1, 8, m_file) != 8) { close(); return false; }
    const bool isV1 = std::memcmp(magic, kMagicV1, 8) == 0;
    const bool isV2 = std::memcmp(magic, kMagicV2, 8) == 0;
    if (!isV1 && !isV2) { close(); return false; }
    uint32_t version = 0;
    if (!readRaw(m_file, version)) { close(); return false; }
    const uint32_t want = isV2 ? kVersionV2 : kVersionV1;
    if (version != want)            { close(); return false; }
    m_streamVersion = static_cast<int>(version);
    return true;
}

void PrimitiveStreamReader::close() {
    if (m_file) {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

namespace {

bool readPrimitiveBody(std::FILE* f, Primitive& p) {
    uint8_t kind, textured, vertexCount, blendOrPad;
    uint64_t tag;
    if (!readRaw(f, kind))        return false;
    if (!readRaw(f, textured))    return false;
    if (!readRaw(f, vertexCount)) return false;
    if (!readRaw(f, blendOrPad))  return false;
    if (!readRaw(f, tag))         return false;
    p.kind      = static_cast<PrimitiveKind>(kind);
    p.textured  = textured != 0;
    p.blendMode = static_cast<BlendMode>(blendOrPad);
    p.hostTag   = tag;
    p.vertices.resize(vertexCount);
    for (uint8_t v = 0; v < vertexCount; ++v) {
        auto& vv = p.vertices[v];
        if (!readRaw(f, vv.x)) return false;
        if (!readRaw(f, vv.y)) return false;
        if (!readRaw(f, vv.u)) return false;
        if (!readRaw(f, vv.v)) return false;
        if (!readRaw(f, vv.r)) return false;
        if (!readRaw(f, vv.g)) return false;
        if (!readRaw(f, vv.b)) return false;
        if (!readRaw(f, vv.a)) return false;
    }
    return true;
}

bool readUploadBody(std::FILE* f, VRAMUpload& u) {
    uint16_t x, y, w, h;
    if (!readRaw(f, x)) return false;
    if (!readRaw(f, y)) return false;
    if (!readRaw(f, w)) return false;
    if (!readRaw(f, h)) return false;
    u.x = static_cast<int>(x);
    u.y = static_cast<int>(y);
    u.w = static_cast<int>(w);
    u.h = static_cast<int>(h);
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    u.data.resize(n);
    if (n > 0 && std::fread(u.data.data(), sizeof(uint16_t), n, f) != n) {
        return false;
    }
    return true;
}

}  // namespace

bool PrimitiveStreamReader::readNextFrame(EchoFrame& out) {
    if (!m_file) return false;
    uint8_t marker[4];
    if (std::fread(marker, 1, 4, m_file) != 4) return false;
    if (std::memcmp(marker, kEndMarker, 4) == 0) return false;
    if (std::memcmp(marker, kFrameMarker, 4) != 0) return false;

    uint32_t frameIndex  = 0;
    uint32_t recordCount = 0;
    if (!readRaw(m_file, frameIndex))  return false;
    if (!readRaw(m_file, recordCount)) return false;

    out.frameIndex = static_cast<int>(frameIndex);
    out.primitives.clear();
    out.uploads.clear();

    if (m_streamVersion == 1) {
        // v1: records are all primitives, no type byte.
        out.primitives.resize(recordCount);
        for (uint32_t i = 0; i < recordCount; ++i) {
            if (!readPrimitiveBody(m_file, out.primitives[i])) return false;
        }
        return true;
    }

    // v2: typed records.
    for (uint32_t i = 0; i < recordCount; ++i) {
        uint8_t type = 0;
        if (!readRaw(m_file, type)) return false;
        if (type == kRecPrim) {
            Primitive p;
            if (!readPrimitiveBody(m_file, p)) return false;
            out.primitives.push_back(std::move(p));
        } else if (type == kRecUpload) {
            VRAMUpload u;
            if (!readUploadBody(m_file, u)) return false;
            out.uploads.push_back(std::move(u));
        } else {
            return false;
        }
    }
    return true;
}

// ============================================================================
// PrimitiveRingbuffer
// ============================================================================

PrimitiveRingbuffer::PrimitiveRingbuffer(int maxFrames)
    : m_maxFrames(maxFrames < 1 ? 1 : maxFrames) {}

void PrimitiveRingbuffer::beginFrame(int frameIndex) {
    if (static_cast<int>(m_frames.size()) >= m_maxFrames) {
        m_frames.pop_front();
    }
    EchoFrame fr;
    fr.frameIndex = frameIndex;
    m_frames.push_back(std::move(fr));
}

void PrimitiveRingbuffer::recordPrimitive(const Primitive& p) {
    if (m_frames.empty()) return;
    m_frames.back().primitives.push_back(p);
}

const EchoFrame* PrimitiveRingbuffer::getDelayed(int delayFrames) const {
    if (delayFrames < 0) delayFrames = 0;
    const int sz = static_cast<int>(m_frames.size());
    const int idx = sz - 1 - delayFrames;
    if (idx < 0) return nullptr;
    return &m_frames[static_cast<size_t>(idx)];
}

}  // namespace vj
