#include "vj/DebugOverlay.h"

#include <cstdio>

namespace vj {

std::string buildDebugText(const PrimitiveInterceptor& interceptor,
                           const MidiController& midi) {
    const Params& p = interceptor.params();
    const SafetyLimiter& s = interceptor.limiter();
    const RandomController& r = interceptor.random();

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "[VJ] MASTER=%.2f CHANCE=%.2f\n"
        "     GEO=%.2f TEX=%.2f MISS=%.2f COL=%.2f DEP=%.2f CHAOS=%.2f\n"
        "     hold=%d (%d/until reroll)  seed=%u\n"
        "     drawn=%d skipped=%d forced=%d affected=%d depthQ=%zu\n"
        "     CC: 20=%d 21=%d 22=%d 23=%d 24=%d 25=%d 26=%d 27=%d\n",
        p.master, p.chance,
        p.geometry, p.texture, p.missing, p.color, p.depth, p.chaos,
        r.holdFrames(), r.framesUntilNextReroll(), r.state.seed,
        s.drawnCount(), s.skippedCount(), s.forcedDrawCount(), interceptor.affectedCount(),
        interceptor.depthQueue().size(),
        midi.getCC(cc::MASTER), midi.getCC(cc::CHANCE),
        midi.getCC(cc::GEOMETRY), midi.getCC(cc::TEXTURE),
        midi.getCC(cc::MISSING), midi.getCC(cc::COLOR),
        midi.getCC(cc::DEPTH), midi.getCC(cc::CHAOS));
    return std::string(buf);
}

}
