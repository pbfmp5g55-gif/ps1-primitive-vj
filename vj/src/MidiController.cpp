#include "vj/MidiController.h"

#include <algorithm>

namespace vj {

static float norm(int ccValue) {
    if (ccValue < 0) return 0.0f;
    return std::clamp(static_cast<float>(ccValue) / 127.0f, 0.0f, 1.0f);
}

const char* axisName(Axis a) {
    switch (a) {
        case Axis::Master:   return "MASTER";
        case Axis::Chance:   return "CHANCE";
        case Axis::Geometry: return "GEOMETRY";
        case Axis::Texture:  return "TEXTURE";
        case Axis::Missing:  return "MISSING";
        case Axis::Color:    return "COLOR";
        case Axis::Depth:    return "DEPTH";
        case Axis::Chaos:    return "CHAOS";
        default:             return "?";
    }
}

CCMapping::CCMapping() {
    cc[static_cast<int>(Axis::Master)]   = ::vj::cc::MASTER;
    cc[static_cast<int>(Axis::Chance)]   = ::vj::cc::CHANCE;
    cc[static_cast<int>(Axis::Geometry)] = ::vj::cc::GEOMETRY;
    cc[static_cast<int>(Axis::Texture)]  = ::vj::cc::TEXTURE;
    cc[static_cast<int>(Axis::Missing)]  = ::vj::cc::MISSING;
    cc[static_cast<int>(Axis::Color)]    = ::vj::cc::COLOR;
    cc[static_cast<int>(Axis::Depth)]    = ::vj::cc::DEPTH;
    cc[static_cast<int>(Axis::Chaos)]    = ::vj::cc::CHAOS;
}

Params MidiController::buildParams() const {
    Params p;
    p.master   = norm(getCC(m_mapping[Axis::Master]));
    p.chance   = norm(getCC(m_mapping[Axis::Chance]));
    p.geometry = curveSoft(norm(getCC(m_mapping[Axis::Geometry])));
    p.texture  = curveSoft(norm(getCC(m_mapping[Axis::Texture])));
    p.missing  = curveSoft(norm(getCC(m_mapping[Axis::Missing])));
    p.color    = curveSoft(norm(getCC(m_mapping[Axis::Color])));
    p.depth    = curveSoft(norm(getCC(m_mapping[Axis::Depth])));
    p.chaos    = norm(getCC(m_mapping[Axis::Chaos]));
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
