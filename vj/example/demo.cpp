#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "vj/DebugOverlay.h"
#include "vj/MidiController.h"
#include "vj/PrimitiveInterceptor.h"

#if VJ_HAS_RTMIDI
#include "vj/RtMidiController.h"
#endif

namespace {

vj::Primitive makePrim(int i) {
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

int runStaticDemo() {
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

#if VJ_HAS_RTMIDI

int resolvePort(const std::string& spec) {
    char* end = nullptr;
    const long idx = std::strtol(spec.c_str(), &end, 10);
    if (end != spec.c_str() && (end == nullptr || *end == '\0')) {
        return static_cast<int>(idx);
    }
    return vj::RtMidiController::findPortBySubstring(spec);
}

int runMidiDemo(const std::string& portSpec, int durationSeconds) {
    const int portIndex = resolvePort(portSpec);
    if (portIndex < 0) {
        std::fprintf(stderr, "No MIDI port matched '%s'. Try --list-midi.\n", portSpec.c_str());
        return 2;
    }

    std::unique_ptr<vj::RtMidiController> midi;
    try {
        midi = std::make_unique<vj::RtMidiController>(static_cast<unsigned int>(portIndex));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to open MIDI port %d: %s\n", portIndex, e.what());
        return 3;
    }
    std::printf("Opened MIDI port %u: %s\n", midi->portIndex(), midi->portName().c_str());
    std::printf("Send CC 20=MASTER 21=CHANCE 22=GEOMETRY 23=TEXTURE 24=MISSING 25=COLOR 26=DEPTH 27=CHAOS\n");

    vj::PrimitiveInterceptor interceptor;
    int submittedThisFrame = 0;
    interceptor.setSubmitCallback([&](const vj::Primitive& p) {
        (void)p;
        submittedThisFrame++;
    });

    constexpr int kPrimsPerFrame = 50;
    constexpr int kFps = 60;
    const int totalFrames = durationSeconds * kFps;
    const auto frameDt = std::chrono::microseconds(1000000 / kFps);

    auto next = std::chrono::steady_clock::now();
    for (int frame = 0; frame < totalFrames; ++frame) {
        midi->update();
        const vj::Params params = midi->buildParams();

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

        if (frame % kFps == 0) {
            std::printf("[t=%2ds] submitted=%2d/%d  master=%.2f geom=%.2f tex=%.2f miss=%.2f color=%.2f depth=%.2f chaos=%.2f chance=%.2f\n",
                        frame / kFps, submittedThisFrame, kPrimsPerFrame,
                        params.master, params.geometry, params.texture, params.missing,
                        params.color, params.depth, params.chaos, params.chance);
            std::fflush(stdout);
        }

        next += frameDt;
        std::this_thread::sleep_until(next);
    }

    std::printf("MIDI demo done.\n");
    return 0;
}

int listMidi() {
    const auto ports = vj::RtMidiController::listPorts();
    if (ports.empty()) {
        std::printf("No MIDI input ports detected.\n");
        return 0;
    }
    std::printf("Available MIDI input ports:\n");
    for (size_t i = 0; i < ports.size(); ++i) {
        std::printf("  [%zu] %s\n", i, ports[i].c_str());
    }
    return 0;
}

#endif

void printUsage(const char* argv0) {
    std::printf("Usage: %s [options]\n", argv0);
    std::printf("  (no args)              run the static-CC headless demo (60 frames)\n");
#if VJ_HAS_RTMIDI
    std::printf("  --list-midi            list available MIDI input ports and exit\n");
    std::printf("  --midi <index|substr>  open MIDI port and run a 30s live demo\n");
    std::printf("  --duration <seconds>   override --midi run length (default 30)\n");
#else
    std::printf("  (this build was compiled without VJ_ENABLE_RTMIDI; --midi unavailable)\n");
#endif
}

}  // namespace

int main(int argc, char** argv) {
    bool wantList = false;
    std::string midiSpec;
    [[maybe_unused]] int duration = 30;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") { printUsage(argv[0]); return 0; }
        else if (a == "--list-midi") { wantList = true; }
        else if (a == "--midi" && i + 1 < argc) { midiSpec = argv[++i]; }
        else if (a == "--duration" && i + 1 < argc) { duration = std::atoi(argv[++i]); }
        else {
            std::fprintf(stderr, "Unknown argument: %s\n", a.c_str());
            printUsage(argv[0]);
            return 1;
        }
    }

#if VJ_HAS_RTMIDI
    if (wantList) return listMidi();
    if (!midiSpec.empty()) return runMidiDemo(midiSpec, duration > 0 ? duration : 30);
#else
    if (wantList || !midiSpec.empty()) {
        std::fprintf(stderr, "Built without VJ_ENABLE_RTMIDI; rebuild with -DVJ_ENABLE_RTMIDI=ON.\n");
        return 1;
    }
#endif

    return runStaticDemo();
}
