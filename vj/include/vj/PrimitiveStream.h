#pragma once

#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "Primitive.h"

namespace vj {

// One frame's worth of recorded primitives.
struct EchoFrame {
    int                    frameIndex = 0;
    std::vector<Primitive> primitives;
};

// PrimitiveStream binary format. Little-endian, 32-bit aligned-ish, designed
// for fopen/fwrite/fread portability. Replay-friendly: every frame can be
// found by sequential scan, no global index required.
//
//   Header (12 bytes):
//     magic[8]   = "VJREC001"
//     version[4] = 1
//
//   Per frame:
//     marker[4]         = 0xFE 0xFE 0xFE 0xFE
//     frameIndex[4]
//     primitiveCount[4]
//     primitives[primitiveCount]:
//       kind[1]        (0=Triangle, 1=Quad, 2=Sprite)
//       textured[1]
//       vertexCount[1]
//       _pad[1]
//       hostTag[8]
//       vertices[vertexCount]:
//         x[4] y[4] u[4] v[4]
//         r[1] g[1] b[1] a[1]
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

    // Start a new frame block in the stream.
    void beginFrame(int frameIndex, int primitiveCount);

    // Append one primitive to the current frame. beginFrame() must have been
    // called first with the matching primitiveCount.
    void writePrimitive(const Primitive& p);

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

    // Read the next frame block. Returns true on success, false on EOF or
    // malformed input.
    bool readNextFrame(EchoFrame& out);

   private:
    std::FILE* m_file = nullptr;
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
