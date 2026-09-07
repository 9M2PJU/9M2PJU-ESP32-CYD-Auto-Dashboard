#pragma once

#include <Arduino.h>
#include "obd_data.h"

enum PerformanceState {
    PERF_STATE_IDLE = 0,
    PERF_STATE_ARMED,       // Ready at 0 km/h, waiting for gas
    PERF_STATE_RUNNING,     // Accelerating
    PERF_STATE_FINISHED     // Completed run
};

struct PerformanceRun {
    float time0to60 = 0.0f;    // 0 - 60 km/h in seconds
    float time0to100 = 0.0f;   // 0 - 100 km/h in seconds
    float timeQuarterMile = 0.0f; // 0 - 400m in seconds
    float trapSpeedKph = 0.0f;
    float peakGForce = 0.0f;
    bool valid = false;
};

class PerformanceTracker {
public:
    PerformanceTracker();
    void reset();
    void arm();
    void update(const OBDMetrics& metrics);

    PerformanceState getState() const { return m_state; }
    const PerformanceRun& getCurrentRun() const { return m_currentRun; }
    const PerformanceRun& getBestRun() const { return m_bestRun; }
    float getLiveTimeSec() const;
    float getEstimatedDistanceMeters() const { return m_distanceMeters; }
    float getLiveGForce() const { return m_liveGForce; }

private:
    PerformanceState m_state = PERF_STATE_IDLE;
    PerformanceRun m_currentRun;
    PerformanceRun m_bestRun;

    unsigned long m_launchMs = 0;
    unsigned long m_lastUpdateMs = 0;
    float m_lastSpeedKph = 0.0f;
    float m_distanceMeters = 0.0f;
    float m_liveGForce = 0.0f;

    bool m_reached60 = false;
    bool m_reached100 = false;
    bool m_reachedQuarterMile = false;
};

extern PerformanceTracker PerfTracker;
