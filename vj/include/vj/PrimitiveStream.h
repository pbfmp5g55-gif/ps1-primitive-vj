#pragma once

#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "Primitive.h"

namespace vj {

// A CPU->VRAM rectangle copy as observed on the PS1 GPU. data holds w*h
// 16bpp pixels (PS1 5/5/5/mask layout) in row-major order, top-left first.
// This is the smallest unit that lets a replayer reconstruct texture state
// faithfully enough for textured polygons to sample correctly.
struct VRAMUpload {
    int                   x = 0;
    int                   y = 0;
    int                   w = 0;
    int                   h = 0;
    std::vector<uint16_t> data;
};

// One frame's worth of recorded GPU state. primitives are drawn in order;
// uploads happen before the primitives of the same frame (typical PS1
// usage: textures uploaded into VRAM, then polys sample them).
struct EchoFrame {
    int                     frameIndex = 0;
    std::vector<VRAMUpload> uploads;
    std::vector<Primitive>  primitives;
};

// PrimitiveStream binary format. Little-endian, 32-bit aligned-ish, designed
// for fopen/fwrite/fread portability. Replay-friendly: every frame can be
// found by sequential scan, no global index required.
//
//   Header (12 bytes):
//     magic[8]   = "VJREC001" | "VJREC002" | "VJREC003"
//     version[4] = 1 | 2 | 3
//
// --- v1 (legacy, read-only support) ---
//   Per frame:
//     marker[4]         = 0xFE 0xFE 0xFE 0xFE
//     frameIndex[4]
//     primitiveCount[4]
//     primitives[primitiveCount]:
//       kind[1] textured[1] vertexCount[1] _pad[1]
//       hostTag[8]
//       vertices[vertexCount]:
//         x[4] y[4] u[4] v[4]
//         r[1] g[1] b[1] a[1]
//
// --- v2 (read-only support, no inline palette) ---
//   Per frame:
//     marker[4]    = 0xFE 0xFE 0xFE 0xFE
//     frameIndex[4]
//     recordCount[4]
//     records[recordCount]:
//       type[1]        (0=Primitive, 1=VRAMUpload)
//       if type == 0: kind[1] textured[1] vertexCount[1] blendMode[1]
//                     hostTag[8] + vertices[].
//       if type == 1: x[2] y[2] w[2] h[2] data[w*h*2]
//
// --- v3 (current, primitives carry inline CLUT palette) ---
//   Same as v2 except the Primitive body has a trailing palette payload:
//     paletteKind[1]   (0=none, 1=palette16, 2=palette256)
//     if paletteKind == 1: palette[16] of uint16  (4bpp CLUT, 5/5/5/M)
//     if paletteKind == 2: palette[256] of uint16 (8bpp CLUT, 5/5/5/M)
//   v2 readers cannot parse v3 files (the new payload shifts subsequent
//   record offsets); we only guarantee forward compatibility (the v3
//   reader transparently reads v1/v2/v3 files).
//
//   End marker (4 bytes): 0xFF 0xFF 0xFF 0xFF

class PrimitiveStreamWriter {
   public:
    PrimitiveStreamWriter() = default;
    ~PrimitiveStreamWriter();

    PrimitiveStreamWriter(const PrimitiveStreamWriter&)            = delete;
    PrimitiveStreamWriter& operator=(const PrimitiveStreamWriter&) = delete;

    // Open a new file for writing. Returns false on filesystem failure.
    // Writes the header immediately.
    bool open(const std::string& path);

    bool isOpen() const { return m_file != nullptr; }

    // Finalises by writing the end marker, then closes.
    void close();

    // Start a new frame block in the stream. recordCount is the total
    // number of records (primitives + uploads) in this frame.
    void beginFrame(int frameIndex, int recordCount);

    // Append one primitive record to the current frame.
    void writePrimitive(const Primitive& p);

    // Append one VRAM upload record to the current frame.
    void writeVRAMUpload(const VRAMUpload& u);

   private:
    std::FILE* m_file = nullptr;
};

class PrimitiveStreamReader {
   public:
    PrimitiveStreamReader() = default;
    ~PrimitiveStreamReader();

    PrimitiveStreamReader(const PrimitiveStreamReader&)            = delete;
    PrimitiveStreamReader& operator=(const PrimitiveStreamReader&) = delete;

    bool open(const std::string& path);
    bool isOpen() const { return m_file != nullptr; }
    void close();

    // 1 (legacy primitive-only stream), 2 (with VRAM uploads),
    // or 3 (with inline CLUT palette per primitive).
    int streamVersion() const { return m_streamVersion; }

    // Read the next frame block. Returns true on success, false on EOF or
    // malformed input. v1 streams yield frames with no uploads.
    bool readNextFrame(EchoFrame& out);

   private:
    std::FILE* m_file          = nullptr;
    int        m_streamVersion = 0;
};

// In-memory ring of EchoFrames for Twin Self / Echo VJ effects. Capacity is
// fixed at construction time; pushes past capacity drop the oldest frame.
class PrimitiveRingbuffer {
   public:
    explicit PrimitiveRingbuffer(int maxFrames);

    // Start a new frame at the write head. Pushes out the oldest frame if at
    // capacity. The new frame is empty.
    void beginFrame(int frameIndex);

    // Append one primitive to the most-recently-begun frame. Silently
    // dropped if no beginFrame() has happened yet.
    void recordPrimitive(const Primitive& p);

    // Return the frame written delayFrames frames before the current write
    // head (delayFrames=0 returns the current frame, =1 returns one frame
    // back, etc). Returns nullptr if the delay exceeds what's been recorded.
    const EchoFrame* getDelayed(int delayFrames) const;

    int  sizeFrames() const { return static_cast<int>(m_frames.size()); }
    int  capacity() const   { return m_maxFrames; }

   private:
    int                 m_maxFrames;
    std::deque<EchoFrame> m_frames;  // back() = most recent
};

}  // namespace vj
