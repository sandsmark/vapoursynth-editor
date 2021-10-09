#pragma once

#include "common-src/helpers.h"

#include <array>
#include <QElapsedTimer>
#include <QDebug>

namespace vsedit  {
struct FpsBuffer
{
    // Size of ring buffer used for averaging FPS
    static constexpr size_t frameAvgCount = 20;

    double update(const int framesProcessed)
    {

        if (!m_lastFrameTimer.isValid() || m_lastFrameCount < 0) {
            qWarning() << "not setup properly, timer valid?" << m_lastFrameTimer.isValid() << "framecount" << m_lastFrameCount;
            reset();
            return m_fps;
        }

        const int framesProcessedDelta = framesProcessed - m_lastFrameCount;
        // Assume the UI updates max 60 fps, so no need to update FPS estimate faster than that
        if (m_lastFrameTimer.elapsed() < 16 || framesProcessedDelta < 1) {
            return m_fps;
        }
        const size_t bufferPos = m_frameTimesSaved % m_fpsBuffer.size();
        if (bufferPos < 0) {
            qWarning() << "Invalid number of frames processed!" << bufferPos;
            reset();
            return m_fps;
        }
        m_lastFrameCount = framesProcessed;
        const double currentFps = 1000. * framesProcessedDelta / double(m_lastFrameTimer.restart());
        m_fpsBuffer[bufferPos] = currentFps;
        m_frameTimesSaved++;

        const size_t frameTimesValid = std::min(m_frameTimesSaved, m_fpsBuffer.size());

        double sum = 0;
        for (size_t i=0; i < frameTimesValid; i++) {
            sum += m_fpsBuffer[i];
        }
        if (sum == 0) {
            qWarning() << "Invalid processing times";
            return m_fps;
        }
        m_fps = sum / frameTimesValid;
        return m_fps;
    }

    void reset()
    {
        m_lastFrameTimer.restart();
        m_lastFrameCount = 0;
        m_fpsBuffer.fill(0);
        m_frameTimesSaved = 0;
        m_fps = 0.;
    }

    QString toString() const
    {
        return QString::number(m_fps, 'f', vsedit::significantDigits(m_fps));
    }

    inline double fps() const { return m_fps; }

private:
    QElapsedTimer m_lastFrameTimer;
    int m_lastFrameCount = 0;
    std::array<double, frameAvgCount> m_fpsBuffer{0};
    size_t m_frameTimesSaved = 0;
    double m_fps = 0.;

};

}
