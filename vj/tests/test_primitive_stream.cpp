#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "vj/Primitive.h"
#include "vj/PrimitiveStream.h"

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

vj::Primitive makePrim(int seed) {
    vj::Primitive p;
    p.kind     = (seed % 3 == 0) ? vj::PrimitiveKind::Triangle
                  : (seed % 3 == 1) ? vj::PrimitiveKind::Quad
                                    : vj::PrimitiveKind::Sprite;
    p.textured  = (seed % 2) != 0;
    p.blendMode = static_cast<vj::BlendMode>(seed % 5);
    p.hostTag   = static_cast<uint64_t>(seed) * 1000003ull;
    const int vc = (p.kind == vj::PrimitiveKind::Quad) ? 4 : 3;
    p.vertices.resize(static_cast<size_t>(vc));
    for (int i = 0; i < vc; ++i) {
        auto& v = p.vertices[static_cast<size_t>(i)];
        v.x = static_cast<float>(seed + i);
        v.y = static_cast<float>(seed - i);
        v.u = static_cast<float>(i * 17);
        v.v = static_cast<float>(i * 19);
        v.r = static_cast<uint8_t>((seed + i * 7) & 0xff);
        v.g = static_cast<uint8_t>((seed + i * 11) & 0xff);
        v.b = static_cast<uint8_t>((seed + i * 13) & 0xff);
        v.a = static_cast<uint8_t>(255);
    }
    return p;
}

bool primEqual(const vj::Primitive& a, const vj::Primitive& b) {
    if (a.kind != b.kind) return false;
    if (a.textured != b.textured) return false;
    if (a.blendMode != b.blendMode) return false;
    if (a.hostTag != b.hostTag) return false;
    if (a.vertices.size() != b.vertices.size()) return false;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        const auto& va = a.vertices[i];
        const auto& vb = b.vertices[i];
        if (!nearlyEqual(va.x, vb.x)) return false;
        if (!nearlyEqual(va.y, vb.y)) return false;
        if (!nearlyEqual(va.u, vb.u)) return false;
        if (!nearlyEqual(va.v, vb.v)) return false;
        if (va.r != vb.r) return false;
        if (va.g != vb.g) return false;
        if (va.b != vb.b) return false;
        if (va.a != vb.a) return false;
    }
    return true;
}

// Cross-platform temp path. Uses tmpnam fallback on systems without TMPDIR.
std::string tempPath(const char* tag) {
    char buf[L_tmpnam];
    if (std::tmpnam(buf) == nullptr) return std::string("vj_stream_") + tag + ".bin";
    return std::string(buf) + "_" + tag;
}

void test_writer_reader_roundtrip() {
    const std::string path = tempPath("roundtrip");
    {
        vj::PrimitiveStreamWriter w;
        assert(w.open(path));
        assert(w.isOpen());
        for (int f = 0; f < 5; ++f) {
            const int primPerFrame = 3 + f;
            w.beginFrame(f * 10, primPerFrame);
            for (int i = 0; i < primPerFrame; ++i) {
                w.writePrimitive(makePrim(f * 1000 + i));
            }
        }
        w.close();
        assert(!w.isOpen());
    }
    {
        vj::PrimitiveStreamReader r;
        assert(r.open(path));
        for (int f = 0; f < 5; ++f) {
            const int primPerFrame = 3 + f;
            vj::EchoFrame ef;
            assert(r.readNextFrame(ef));
            assert(ef.frameIndex == f * 10);
            assert(static_cast<int>(ef.primitives.size()) == primPerFrame);
            for (int i = 0; i < primPerFrame; ++i) {
                assert(primEqual(ef.primitives[static_cast<size_t>(i)],
                                 makePrim(f * 1000 + i)));
            }
        }
        vj::EchoFrame ef;
        // After last frame, end marker should make readNextFrame return false.
        assert(!r.readNextFrame(ef));
        r.close();
    }
    std::remove(path.c_str());
}

void test_writer_reader_empty_frames() {
    const std::string path = tempPath("empty");
    {
        vj::PrimitiveStreamWriter w;
        assert(w.open(path));
        w.beginFrame(0, 0);
        w.beginFrame(1, 0);
        w.close();
    }
    {
        vj::PrimitiveStreamReader r;
        assert(r.open(path));
        vj::EchoFrame ef;
        assert(r.readNextFrame(ef));
        assert(ef.frameIndex == 0);
        assert(ef.primitives.empty());
        assert(r.readNextFrame(ef));
        assert(ef.frameIndex == 1);
        assert(ef.primitives.empty());
        assert(!r.readNextFrame(ef));
    }
    std::remove(path.c_str());
}

void test_reader_rejects_bad_magic() {
    const std::string path = tempPath("badmagic");
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        assert(f);
        const char junk[12] = {'N','O','P','E','0','0','0','0', 1,0,0,0};
        std::fwrite(junk, 1, 12, f);
        std::fclose(f);
    }
    vj::PrimitiveStreamReader r;
    assert(!r.open(path));  // bad magic must fail open
    std::remove(path.c_str());
}

void test_ringbuffer_basic_push_and_getDelayed() {
    vj::PrimitiveRingbuffer rb(4);
    assert(rb.capacity() == 4);
    assert(rb.sizeFrames() == 0);

    rb.beginFrame(0);
    rb.recordPrimitive(makePrim(0));
    rb.recordPrimitive(makePrim(1));
    assert(rb.sizeFrames() == 1);

    rb.beginFrame(1);
    rb.recordPrimitive(makePrim(10));
    assert(rb.sizeFrames() == 2);

    const auto* cur = rb.getDelayed(0);
    assert(cur);
    assert(cur->frameIndex == 1);
    assert(cur->primitives.size() == 1);

    const auto* prev = rb.getDelayed(1);
    assert(prev);
    assert(prev->frameIndex == 0);
    assert(prev->primitives.size() == 2);

    assert(rb.getDelayed(2) == nullptr);  // beyond what's recorded
    assert(rb.getDelayed(100) == nullptr);
}

void test_ringbuffer_drops_oldest_at_capacity() {
    vj::PrimitiveRingbuffer rb(3);
    for (int f = 0; f < 5; ++f) {
        rb.beginFrame(f);
        rb.recordPrimitive(makePrim(f));
    }
    assert(rb.sizeFrames() == 3);
    // The 3 most recent frames are 2, 3, 4. Frames 0 and 1 dropped.
    const auto* d0 = rb.getDelayed(0);
    const auto* d1 = rb.getDelayed(1);
    const auto* d2 = rb.getDelayed(2);
    assert(d0 && d0->frameIndex == 4);
    assert(d1 && d1->frameIndex == 3);
    assert(d2 && d2->frameIndex == 2);
    assert(rb.getDelayed(3) == nullptr);
}

void test_v2_roundtrip_with_uploads() {
    const std::string path = tempPath("v2uploads");
    {
        vj::PrimitiveStreamWriter w;
        assert(w.open(path));
        // Frame 0: 1 upload + 2 primitives.
        w.beginFrame(0, 3);
        vj::VRAMUpload u;
        u.x = 64; u.y = 0; u.w = 4; u.h = 2;
        u.data = {0x1234, 0x5678, 0x9abc, 0xdef0,
                  0x0fed, 0xcba9, 0x8765, 0x4321};
        w.writeVRAMUpload(u);
        w.writePrimitive(makePrim(1));
        w.writePrimitive(makePrim(2));
        // Frame 1: 0 uploads + 1 primitive.
        w.beginFrame(1, 1);
        w.writePrimitive(makePrim(100));
        w.close();
    }
    {
        vj::PrimitiveStreamReader r;
        assert(r.open(path));
        assert(r.streamVersion() == 2);

        vj::EchoFrame f0;
        assert(r.readNextFrame(f0));
        assert(f0.frameIndex == 0);
        assert(f0.uploads.size() == 1);
        assert(f0.primitives.size() == 2);
        assert(f0.uploads[0].x == 64 && f0.uploads[0].y == 0);
        assert(f0.uploads[0].w == 4 && f0.uploads[0].h == 2);
        assert(f0.uploads[0].data.size() == 8);
        assert(f0.uploads[0].data[0] == 0x1234);
        assert(f0.uploads[0].data[7] == 0x4321);
        assert(primEqual(f0.primitives[0], makePrim(1)));
        assert(primEqual(f0.primitives[1], makePrim(2)));

        vj::EchoFrame f1;
        assert(r.readNextFrame(f1));
        assert(f1.frameIndex == 1);
        assert(f1.uploads.empty());
        assert(f1.primitives.size() == 1);
        assert(primEqual(f1.primitives[0], makePrim(100)));

        vj::EchoFrame f2;
        assert(!r.readNextFrame(f2));
    }
    std::remove(path.c_str());
}

void test_v1_backward_compat() {
    // Build a v1 file by hand: legacy magic + version + a single frame of
    // two primitives in the v1 layout (no type byte).
    const std::string path = tempPath("v1compat");
    std::FILE* f = std::fopen(path.c_str(), "wb");
    assert(f);
    std::fwrite("VJREC001", 1, 8, f);
    uint32_t version = 1;
    std::fwrite(&version, sizeof(version), 1, f);
    // Frame: marker + frameIdx=7 + primCount=2
    uint8_t marker[4] = {0xFE, 0xFE, 0xFE, 0xFE};
    std::fwrite(marker, 1, 4, f);
    uint32_t frameIdx = 7;
    uint32_t primCount = 2;
    std::fwrite(&frameIdx, sizeof(frameIdx), 1, f);
    std::fwrite(&primCount, sizeof(primCount), 1, f);
    for (int i = 0; i < 2; ++i) {
        vj::Primitive p = makePrim(200 + i);
        uint8_t kind        = static_cast<uint8_t>(p.kind);
        uint8_t textured    = p.textured ? 1 : 0;
        uint8_t vertexCount = static_cast<uint8_t>(p.vertices.size());
        uint8_t pad         = 0;
        std::fwrite(&kind, 1, 1, f);
        std::fwrite(&textured, 1, 1, f);
        std::fwrite(&vertexCount, 1, 1, f);
        std::fwrite(&pad, 1, 1, f);
        uint64_t tag = p.hostTag;
        std::fwrite(&tag, sizeof(tag), 1, f);
        for (const auto& v : p.vertices) {
            std::fwrite(&v.x, sizeof(v.x), 1, f);
            std::fwrite(&v.y, sizeof(v.y), 1, f);
            std::fwrite(&v.u, sizeof(v.u), 1, f);
            std::fwrite(&v.v, sizeof(v.v), 1, f);
            std::fwrite(&v.r, 1, 1, f);
            std::fwrite(&v.g, 1, 1, f);
            std::fwrite(&v.b, 1, 1, f);
            std::fwrite(&v.a, 1, 1, f);
        }
    }
    uint8_t endMarker[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    std::fwrite(endMarker, 1, 4, f);
    std::fclose(f);

    // Now read with the v2-aware reader; v1 path should match.
    vj::PrimitiveStreamReader r;
    assert(r.open(path));
    assert(r.streamVersion() == 1);
    vj::EchoFrame fr;
    assert(r.readNextFrame(fr));
    assert(fr.frameIndex == 7);
    assert(fr.uploads.empty());
    assert(fr.primitives.size() == 2);
    // v1 files don't carry blendMode (the byte where it lives was a
    // padding byte written as 0). Expected values reflect that.
    auto expected0 = makePrim(200);
    auto expected1 = makePrim(201);
    expected0.blendMode = vj::BlendMode::Opaque;
    expected1.blendMode = vj::BlendMode::Opaque;
    assert(primEqual(fr.primitives[0], expected0));
    assert(primEqual(fr.primitives[1], expected1));
    assert(!r.readNextFrame(fr));
    std::remove(path.c_str());
}

void test_ringbuffer_recordPrimitive_before_beginFrame_is_noop() {
    vj::PrimitiveRingbuffer rb(2);
    rb.recordPrimitive(makePrim(0));  // should be silently ignored
    assert(rb.sizeFrames() == 0);
    rb.beginFrame(0);
    assert(rb.sizeFrames() == 1);
}

}  // namespace

int main() {
    test_writer_reader_roundtrip();
    test_writer_reader_empty_frames();
    test_reader_rejects_bad_magic();
    test_ringbuffer_basic_push_and_getDelayed();
    test_ringbuffer_drops_oldest_at_capacity();
    test_v2_roundtrip_with_uploads();
    test_v1_backward_compat();
    test_ringbuffer_recordPrimitive_before_beginFrame_is_noop();
    std::fprintf(stderr, "[test_primitive_stream] OK\n");
    return 0;
}
