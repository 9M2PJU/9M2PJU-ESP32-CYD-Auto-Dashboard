#include "performance_tracker.h"

PerformanceTracker PerfTracker;

PerformanceTracker::PerformanceTracker() {
    reset();
}

void PerformanceTracker::reset() {
    m_state = PERF_STATE_IDLE;
    m_currentRun = PerformanceRun();
    m_launchMs = 0;
    m_lastUpdateMs = 0;
    m_lastSpeedKph = 0.0f;
    m_distanceMeters = 0.0f;
    m_liveGForce = 0.0f;
    m_reached60 = false;
    m_reached100 = false;
    m_reachedQuarterMile = false;
}

void PerformanceTracker::arm() {
    m_state = PERF_STATE_ARMED;
    m_currentRun = PerformanceRun();
    m_launchMs = 0;
    m_lastUpdateMs = 0;
    m_lastSpeedKph = 0.0f;
    m_distanceMeters = 0.0f;
    m_liveGForce = 0.0f;
    m_reached60 = false;
    m_reached100 = false;
    m_reachedQuarterMile = false;
}

float PerformanceTracker::getLiveTimeSec() const {
    if (m_state == PERF_STATE_RUNNING && m_launchMs > 0) {
        return (float)(millis() - m_launchMs) / 1000.0f;
    }
    if (m_state == PERF_STATE_FINISHED) {
        return m_currentRun.time0to100 > 0.0f ? m_currentRun.time0to100 : m_currentRun.time0to60;
    }
    return 0.0f;
}

void PerformanceTracker::update(const OBDMetrics& metrics) {
    unsigned long now = millis();
    float speed = metrics.speedKph;

    // Calculate instantaneous acceleration & G-force (1 G = 9.80665 m/s^2)
    if (m_lastUpdateMs > 0 && now > m_lastUpdateMs) {
        float dt = (float)(now - m_lastUpdateMs) / 1000.0f;
        if (dt > 0.01f && dt < 1.0f) {
            float speedMsCurrent = speed * (1000.0f / 3600.0f);
            float speedMsLast = m_lastSpeedKph * (1000.0f / 3600.0f);
            float accelMs2 = (speedMsCurrent - speedMsLast) / dt;
            m_liveGForce = accelMs2 / 9.80665f;

            if (m_state == PERF_STATE_RUNNING) {
                // Integrate distance: average velocity * dt
                float avgSpeedMs = (speedMsCurrent + speedMsLast) * 0.5f;
                m_distanceMeters += (avgSpeedMs * dt);

                if (m_liveGForce > m_currentRun.peakGForce) {
                    m_currentRun.peakGForce = m_liveGForce;
                }
            }
        }
    }
    m_lastUpdateMs = now;
    m_lastSpeedKph = speed;

    // State Machine
    switch (m_state) {
        case PERF_STATE_IDLE:
            // Auto-arm when car comes to a complete standstill
            if (speed <= 1.0f) {
                m_state = PERF_STATE_ARMED;
            }
            break;

        case PERF_STATE_ARMED:
            // Detect launch: vehicle starts moving from standstill
            if (speed >= 3.0f || (speed >= 2.0f && metrics.throttlePos > 15.0f)) {
                m_state = PERF_STATE_RUNNING;
                m_launchMs = now;
                m_distanceMeters = 0.0f;
                m_currentRun.peakGForce = 0.0f;
            }
            break;

        case PERF_STATE_RUNNING: {
            float elapsedSec = (float)(now - m_launchMs) / 1000.0f;

            // 0 - 60 km/h split
            if (!m_reached60 && speed >= 60.0f) {
                m_currentRun.time0to60 = elapsedSec;
                m_reached60 = true;
            }

            // 0 - 100 km/h split
            if (!m_reached100 && speed >= 100.0f) {
                m_currentRun.time0to100 = elapsedSec;
                m_reached100 = true;
            }

            // 400m (Quarter Mile) split
            if (!m_reachedQuarterMile && m_distanceMeters >= 402.336f) {
                m_currentRun.timeQuarterMile = elapsedSec;
                m_currentRun.trapSpeedKph = speed;
                m_reachedQuarterMile = true;
            }

            // Finish condition: 100 km/h or 400m achieved or vehicle slows down
            if ((m_reached100 && elapsedSec > 20.0f) || m_reachedQuarterMile || (speed < 10.0f && elapsedSec > 5.0f)) {
                m_state = PERF_STATE_FINISHED;
                m_currentRun.valid = (m_currentRun.time0to60 > 0.0f || m_currentRun.time0to100 > 0.0f);

                // Update best run
                if (m_currentRun.valid) {
                    if (m_bestRun.time0to100 <= 0.0f || (m_currentRun.time0to100 > 0.0f && m_currentRun.time0to100 < m_bestRun.time0to100)) {
                        m_bestRun = m_currentRun;
                    }
                }
            }
            break;
        }

        case PERF_STATE_FINISHED:
            // Auto re-arm when vehicle stops
            if (speed <= 1.0f) {
                // If stopped for more than 2 seconds, re-arm
                m_state = PERF_STATE_ARMED;
            }
            break;
    }
}
