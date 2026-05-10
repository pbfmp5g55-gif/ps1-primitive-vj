#pragma once

#include "MidiController.h"

#if VJ_HAS_RTMIDI

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class RtMidiIn;

namespace vj {

class RtMidiController : public MidiController {
public:
    explicit RtMidiController(unsigned int portIndex, const std::string& clientName = "ps1-primitive-vj");
    ~RtMidiController() override;

    RtMidiController(const RtMidiController&) = delete;
    RtMidiController& operator=(const RtMidiController&) = delete;

    void update() override {}
    int getCC(int cc) const override;

    unsigned int portIndex() const { return m_portIndex; }
    const std::string& portName() const { return m_portName; }

    static std::vector<std::string> listPorts();
    static int findPortBySubstring(const std::string& needle);

private:
    static void onMessageTrampoline(double, std::vector<unsigned char>* msg, void* user);
    void handleMessage(const std::vector<unsigned char>& msg);

    std::unique_ptr<RtMidiIn> m_in;
    std::atomic<int> m_cc[128];
    unsigned int m_portIndex;
    std::string m_portName;
};

}

#endif
