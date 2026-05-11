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

enum class Axis : int {
    Master = 0,
    Chance,
    Geometry,
    Texture,
    Missing,
    Color,
    Depth,
    Chaos,
    Count
};

constexpr int kAxisCount = static_cast<int>(Axis::Count);

const char* axisName(Axis a);

struct CCMapping {
    int cc[kAxisCount];
    CCMapping();
    int& operator[](Axis a)       { return cc[static_cast<int>(a)]; }
    int  operator[](Axis a) const { return cc[static_cast<int>(a)]; }
};

class MidiController {
public:
    virtual ~MidiController() = default;

    virtual void update() = 0;
    virtual int getCC(int cc) const = 0;

    Params buildParams() const;

    void setMapping(const CCMapping& m) { m_mapping = m; }
    const CCMapping& mapping() const { return m_mapping; }
    void setAxisCC(Axis a, int cc)      { m_mapping[a] = cc; }
    int  axisCC(Axis a) const           { return m_mapping[a]; }

protected:
    CCMapping m_mapping;
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
