#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>
#include "config.h"
#include "obd_data.h"

class OBDEngine {
public:
    OBDEngine();
    void begin(const AppSettings& settings);
    void update(const AppSettings& settings);

    const OBDMetrics& getMetrics() const { return m_metrics; }
    void toggleDemoMode(bool enable);
    bool isDemoMode() const { return m_metrics.isDemo; }

    // Diagnostics
    void scanDtcCodes();
    void clearDtcCodes();
    bool isDtcScanning() const { return m_scanningDtc; }
    String getDtcDescription(const String& code);

    // Reconnection
    void reconnect(const char* targetName);

private:
    OBDMetrics m_metrics;
    BluetoothSerial m_btSerial;

    // State machine
    unsigned long m_lastStateChangeMs = 0;
    unsigned long m_lastQueryMs = 0;
    unsigned long m_lastSlowQueryMs = 0;
    unsigned long m_lastMediumQueryMs = 0;
    unsigned long m_queryStartTimeMs = 0;
    uint8_t m_queryPhase = 0;
    bool m_waitingResponse = false;
    String m_activeCommand = "";
    String m_rxBuffer = "";

    // Diagnostics state
    bool m_scanningDtc = false;
    bool m_clearingDtc = false;

    // Demo Simulation State
    unsigned long m_lastSimUpdateMs = 0;
    float m_simTargetRpm = 850.0f;
    float m_simCurrentRpm = 850.0f;
    float m_simSpeed = 0.0f;
    int m_simGear = 1;
    float m_simThrottle = 0.0f;
    float m_simCoolant = 45.0f;
    bool m_simAccelerating = true;

    // Helper methods
    void handleStateInit(const AppSettings& settings);
    void handleStateConnecting(const AppSettings& settings);
    void handleStateInitElm();
    void handleStateConnected();
    void updateSimulation();

    bool sendCommand(const char* cmd);
    void processResponse(const String& cmd, const String& resp);
    void parseDtcResponse(const String& resp);
    int estimateGear(float rpm, float speedKph);
};

extern OBDEngine Obd;
