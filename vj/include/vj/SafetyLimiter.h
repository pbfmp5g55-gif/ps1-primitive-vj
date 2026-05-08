#pragma once

namespace vj {

class SafetyLimiter {
public:
    void beginFrame(int totalEstimated);

    bool mustDraw() const;

    void notifyDraw();
    void notifySkip();
    void notifyForcedDraw();

    int totalCount() const { return m_total; }
    int drawnCount() const { return m_drawn; }
    int skippedCount() const { return m_skipped; }
    int forcedDrawCount() const { return m_forced; }

private:
    int m_total = 0;
    int m_drawn = 0;
    int m_skipped = 0;
    int m_forced = 0;
};

float lowMasterSafety(float master);

}
