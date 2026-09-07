#pragma once

#include <Arduino.h>
#include <vector>

// Connection Status of OBD Engine
enum OBDConnectionState {
    OBD_STATE_INIT = 0,
    OBD_STATE_CONNECTING_BT,
    OBD_STATE_INIT_ELM,
    OBD_STATE_CONNECTED,
    OBD_STATE_DISCONNECTED,
    OBD_STATE_ERROR,
    OBD_STATE_DEMO
};

// Comprehensive Live OBD Telemetry Data
struct OBDMetrics {
    // Engine / Powertrain
    float rpm = 0.0f;
    float speedKph = 0.0f;
    float throttlePos = 0.0f;    // 0 - 100%
    float engineLoad = 0.0f;     // 0 - 100%
    int estimatedGear = 0;       // 0=N/P, 1..6

    // Temperatures
    float coolantTempC = 0.0f;
    float intakeAirTempC = 0.0f;
    float ambientTempC = 25.0f;

    // Electrical
    float batteryVoltage = 12.6f;

    // Air / Fuel / Pressure
    float mapKpa = 101.3f;       // Manifold Absolute Pressure (kPa)
    float mafGps = 0.0f;         // Mass Air Flow (grams/sec)
    float fuelLevel = 50.0f;     // Fuel tank % (if supported)
    float timingAdvance = 10.0f; // Timing advance (deg)

    // Diagnostics & DTCs
    bool milStatus = false;      // Malfunction Indicator Lamp (MIL / Check Engine)
    uint8_t dtcCount = 0;
    std::vector<String> dtcCodes;
    String elmVersion = "ELM327 v1.5";
    String detectedProtocol = "ISO 15765-4 (CAN)";

    // System state
    OBDConnectionState state = OBD_STATE_INIT;
    bool isConnected = false;
    bool isDemo = false;
    unsigned long lastUpdateMs = 0;
    uint32_t successfulQueries = 0;
    uint32_t failedQueries = 0;
    float queryRateHz = 0.0f;

    // Helper functions for units
    float getSpeed(bool imperial) const {
        return imperial ? (speedKph * 0.621371f) : speedKph;
    }

    const char* getSpeedUnit(bool imperial) const {
        return imperial ? "MPH" : "KM/H";
    }

    float getCoolantTemp(bool imperial) const {
        return imperial ? (coolantTempC * 1.8f + 32.0f) : coolantTempC;
    }

    float getIntakeTemp(bool imperial) const {
        return imperial ? (intakeAirTempC * 1.8f + 32.0f) : intakeAirTempC;
    }

    const char* getTempUnit(bool imperial) const {
        return imperial ? "°F" : "°C";
    }

    float getBoostPsi() const {
        // Boost = MAP (kPa) - 101.3 kPa converted to PSI (1 kPa = 0.145038 PSI)
        float boost = (mapKpa - 101.325f) * 0.145038f;
        return (boost < 0.0f) ? 0.0f : boost;
    }
};
