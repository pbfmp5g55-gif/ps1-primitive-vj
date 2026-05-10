#include "vj/RtMidiController.h"

#if VJ_HAS_RTMIDI

#include <RtMidi.h>

#include <stdexcept>

namespace vj {

RtMidiController::RtMidiController(unsigned int portIndex, const std::string& clientName)
    : m_in(std::make_unique<RtMidiIn>(RtMidi::Api::UNSPECIFIED, clientName)),
      m_portIndex(portIndex) {
    for (int i = 0; i < 128; ++i) m_cc[i].store(-1, std::memory_order_relaxed);

    const unsigned int count = m_in->getPortCount();
    if (count == 0) {
        throw std::runtime_error("RtMidiController: no MIDI input ports available");
    }
    if (portIndex >= count) {
        throw std::runtime_error("RtMidiController: port index out of range");
    }

    m_portName = m_in->getPortName(portIndex);
    m_in->openPort(portIndex, "vj-input");
    m_in->setCallback(&RtMidiController::onMessageTrampoline, this);
    m_in->ignoreTypes(true, true, true);  // sysex, time, sense — we only want CC
}

RtMidiController::~RtMidiController() {
    if (m_in) {
        m_in->cancelCallback();
        m_in->closePort();
    }
}

int RtMidiController::getCC(int cc) const {
    if (cc < 0 || cc >= 128) return -1;
    return m_cc[cc].load(std::memory_order_relaxed);
}

void RtMidiController::onMessageTrampoline(double, std::vector<unsigned char>* msg, void* user) {
    if (!msg || !user) return;
    static_cast<RtMidiController*>(user)->handleMessage(*msg);
}

void RtMidiController::handleMessage(const std::vector<unsigned char>& msg) {
    if (msg.size() < 3) return;
    const unsigned char status = msg[0] & 0xF0u;
    if (status != 0xB0u) return;  // Control Change only
    const int cc    = msg[1] & 0x7F;
    const int value = msg[2] & 0x7F;
    m_cc[cc].store(value, std::memory_order_relaxed);
}

std::vector<std::string> RtMidiController::listPorts() {
    std::vector<std::string> out;
    try {
        RtMidiIn probe(RtMidi::Api::UNSPECIFIED, "ps1-primitive-vj-probe");
        const unsigned int n = probe.getPortCount();
        out.reserve(n);
        for (unsigned int i = 0; i < n; ++i) out.push_back(probe.getPortName(i));
    } catch (const RtMidiError&) {
        // No usable MIDI backend (e.g. CI runner with no ALSA seq device) — treat as empty.
    }
    return out;
}

int RtMidiController::findPortBySubstring(const std::string& needle) {
    if (needle.empty()) return -1;
    const auto ports = listPorts();
    for (size_t i = 0; i < ports.size(); ++i) {
        if (ports[i].find(needle) != std::string::npos) return static_cast<int>(i);
    }
    return -1;
}

}

#endif
