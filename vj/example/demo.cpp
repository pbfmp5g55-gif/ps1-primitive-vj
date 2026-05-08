#include <cstdio>
#include <vector>

#include "vj/DebugOverlay.h"
#include "vj/MidiController.h"
#include "vj/PrimitiveInterceptor.h"

static vj::Primitive makePrim(int i) {
    vj::Primitive p;
    p.textured = (i % 2 == 0);
    p.kind = (i % 5 == 0) ? vj::PrimitiveKind::Sprite
           : (i % 3 == 0) ? vj::PrimitiveKind::Quad
                          : vj::PrimitiveKind::Triangle;

    int n = (p.kind == vj::PrimitiveKind::Triangle) ? 3 : 4;
    for (int j = 0; j < n; ++j) {
        vj::Vertex v;
        v.x = static_cast<float>((i % 10) * 32 + j * 8);
        v.y = static_cast<float>((i / 10) * 32 + j * 8);
        v.u = static_cast<float>(j * 16);
        v.v = static_cast<float>(i * 4);
        v.r = 200; v.g = 180; v.b = 160; v.a = 255;
        p.vertices.push_back(v);
    }
    return p;
}

int main() {
    vj::StaticMidiController midi;
    midi.setCC(vj::cc::MASTER,   100);
    midi.setCC(vj::cc::CHANCE,   80);
    midi.setCC(vj::cc::GEOMETRY, 64);
    midi.setCC(vj::cc::TEXTURE,  64);
    midi.setCC(vj::cc::MISSING,  30);
    midi.setCC(vj::cc::COLOR,    60);
    midi.setCC(vj::cc::DEPTH,    40);
    midi.setCC(vj::cc::CHAOS,    50);

    vj::PrimitiveInterceptor interceptor;

    int submittedThisFrame = 0;
    interceptor.setSubmitCallback([&](const vj::Primitive& p) {
        (void)p;
        submittedThisFrame++;
    });

    constexpr int kFrames = 60;
    constexpr int kPrimsPerFrame = 50;

    for (int frame = 0; frame < kFrames; ++frame) {
        midi.update();
        const vj::Params params = midi.buildParams();

        std::vector<vj::Primitive> prims;
        prims.reserve(kPrimsPerFrame);
        for (int i = 0; i < kPrimsPerFrame; ++i) {
            prims.push_back(makePrim(i + frame));
        }

        interceptor.beginFrame(params, static_cast<int>(prims.size()));
        submittedThisFrame = 0;
        for (auto& prim : prims) {
            interceptor.interceptAndSubmit(prim);
        }
        interceptor.endFrame();

        if (frame % 10 == 0) {
            std::printf("=== frame %d ===\n%s\n",
                        frame, vj::buildDebugText(interceptor, midi).c_str());
            std::printf("submittedThisFrame=%d\n\n", submittedThisFrame);
        }
    }

    std::printf("Demo done. Final state:\n%s\n",
                vj::buildDebugText(interceptor, midi).c_str());
    return 0;
}
