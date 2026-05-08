#include "vj/MidiController.h"

#include <algorithm>

namespace vj {

static float norm(int ccValue) {
    if (ccValue < 0) return 0.0f;
    return std::clamp(static_cast<float>(ccValue) / 127.0f, 0.0f, 1.0f);
}

Params MidiController::buildParams() const {
    Params p;
    p.master   = norm(getCC(cc::MASTER));
    p.chance   = norm(getCC(cc::CHANCE));
    p.geometry = curveSoft(norm(getCC(cc::GEOMETRY)));
    p.texture  = curveSoft(norm(getCC(cc::TEXTURE)));
    p.missing  = curveSoft(norm(getCC(cc::MISSING)));
    p.color    = curveSoft(norm(getCC(cc::COLOR)));
    p.depth    = curveSoft(norm(getCC(cc::DEPTH)));
    p.chaos    = norm(getCC(cc::CHAOS));
    return p;
}

StaticMidiController::StaticMidiController() {
    for (int i = 0; i < 128; ++i) m_cc[i] = -1;
}

int StaticMidiController::getCC(int cc) const {
    if (cc < 0 || cc >= 128) return -1;
    return m_cc[cc];
}

void StaticMidiController::setCC(int cc, int value) {
    if (cc < 0 || cc >= 128) return;
    m_cc[cc] = std::clamp(value, 0, 127);
}

}
