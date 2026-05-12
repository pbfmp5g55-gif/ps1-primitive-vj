#include "vj/PrimitiveStream.h"

#include <cstdint>
#include <cstring>

namespace vj {

namespace {

constexpr char     kMagic[8]   = {'V', 'J', 'R', 'E', 'C', '0', '0', '1'};
constexpr uint32_t kVersion    = 1;
constexpr uint8_t  kFrameMarker[4] = {0xFE, 0xFE, 0xFE, 0xFE};
constexpr uint8_t  kEndMarker[4]   = {0xFF, 0xFF, 0xFF, 0xFF};

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
    if (std::fwrite(kMagic, 1, 8, m_file) != 8) { close(); return false; }
    if (!writeRaw(m_file, kVersion))            { close(); return false; }
    return true;
}

void PrimitiveStreamWriter::close() {
    if (!m_file) return;
    std::fwrite(kEndMarker, 1, 4, m_file);
    std::fclose(m_file);
    m_file = nullptr;
}

void PrimitiveStreamWriter::beginFrame(int frameIndex, int primitiveCount) {
    if (!m_file) return;
    std::fwrite(kFrameMarker, 1, 4, m_file);
    const uint32_t f = static_cast<uint32_t>(frameIndex);
    const uint32_t c = static_cast<uint32_t>(primitiveCount);
    writeRaw(m_file, f);
    writeRaw(m_file, c);
}

void PrimitiveStreamWriter::writePrimitive(const Primitive& p) {
    if (!m_file) return;
    const uint8_t kind        = static_cast<uint8_t>(p.kind);
    const uint8_t textured    = p.textured ? 1 : 0;
    const uint8_t vertexCount = static_cast<uint8_t>(p.vertices.size());
    const uint8_t pad         = 0;
    writeRaw(m_file, kind);
    writeRaw(m_file, textured);
    writeRaw(m_file, vertexCount);
    writeRaw(m_file, pad);
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
    if (std::memcmp(magic, kMagic, 8) != 0)   { close(); return false; }
    uint32_t version = 0;
    if (!readRaw(m_file, version))            { close(); return false; }
    if (version != kVersion)                  { close(); return false; }
    return true;
}

void PrimitiveStreamReader::close() {
    if (m_file) {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

bool PrimitiveStreamReader::readNextFrame(EchoFrame& out) {
    if (!m_file) return false;
    uint8_t marker[4];
    if (std::fread(marker, 1, 4, m_file) != 4) return false;
    if (std::memcmp(marker, kEndMarker, 4) == 0) return false;
    if (std::memcmp(marker, kFrameMarker, 4) != 0) return false;

    uint32_t frameIndex     = 0;
    uint32_t primitiveCount = 0;
    if (!readRaw(m_file, frameIndex))     return false;
    if (!readRaw(m_file, primitiveCount)) return false;

    out.frameIndex = static_cast<int>(frameIndex);
    out.primitives.clear();
    out.primitives.resize(primitiveCount);
    for (uint32_t i = 0; i < primitiveCount; ++i) {
        uint8_t kind, textured, vertexCount, pad;
        uint64_t tag;
        if (!readRaw(m_file, kind))        return false;
        if (!readRaw(m_file, textured))    return false;
        if (!readRaw(m_file, vertexCount)) return false;
        if (!readRaw(m_file, pad))         return false;
        if (!readRaw(m_file, tag))         return false;

        Primitive& p = out.primitives[i];
        p.kind     = static_cast<PrimitiveKind>(kind);
        p.textured = textured != 0;
        p.hostTag  = tag;
        p.vertices.resize(vertexCount);
        for (uint8_t v = 0; v < vertexCount; ++v) {
            auto& vv = p.vertices[v];
            if (!readRaw(m_file, vv.x)) return false;
            if (!readRaw(m_file, vv.y)) return false;
            if (!readRaw(m_file, vv.u)) return false;
            if (!readRaw(m_file, vv.v)) return false;
            if (!readRaw(m_file, vv.r)) return false;
            if (!readRaw(m_file, vv.g)) return false;
            if (!readRaw(m_file, vv.b)) return false;
            if (!readRaw(m_file, vv.a)) return false;
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
