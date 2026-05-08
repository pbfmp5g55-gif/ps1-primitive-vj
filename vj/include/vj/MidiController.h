#pragma once

#include "Params.h"

namespace vj {

namespace cc {
constexpr int MASTER   = 20;
constexpr int CHANCE   = 21;
constexpr int GEOMETRY = 22;
constexpr int TEXTURE  = 23;
constexpr int MISSING  = 24;
constexpr int COLOR    = 25;
constexpr int DEPTH    = 26;
constexpr int CHAOS    = 27;
}

class MidiController {
public:
    virtual ~MidiController() = default;

    virtual void update() = 0;
    virtual int getCC(int cc) const = 0;

    Params buildParams() const;
};

class StaticMidiController : public MidiController {
public:
    StaticMidiController();

    void update() override {}
    int getCC(int cc) const override;
    void setCC(int cc, int value);

private:
    int m_cc[128];
};

}
