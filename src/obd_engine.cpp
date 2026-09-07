#include "obd_engine.h"

OBDEngine Obd;

// Common OBD-II DTC Trouble Code Dictionary
struct DtcLookup {
    const char* code;
    const char* desc;
};

static const DtcLookup DTC_TABLE[] = {
    {"P0100", "Mass Air Flow Circuit Malfunction"},
    {"P0101", "Mass Air Flow Circuit Range/Performance"},
    {"P0102", "Mass Air Flow Circuit Low Input"},
    {"P0113", "Intake Air Temp Circuit High Input"},
    {"P0115", "Engine Coolant Temp Circuit Malfunction"},
    {"P0118", "Engine Coolant Temp Circuit High Input"},
    {"P0120", "Throttle Position Sensor A Circuit Malfunction"},
    {"P0128", "Coolant Temp Below Thermostat Regulating Temp"},
    {"P0130", "O2 Sensor Circuit Malfunction (Bank 1 Sensor 1)"},
    {"P0171", "System Too Lean (Bank 1)"},
    {"P0172", "System Too Rich (Bank 1)"},
    {"P0300", "Random/Multiple Cylinder Misfire Detected"},
    {"P0301", "Cylinder 1 Misfire Detected"},
    {"P0302", "Cylinder 2 Misfire Detected"},
    {"P0303", "Cylinder 3 Misfire Detected"},
    {"P0304", "Cylinder 4 Misfire Detected"},
    {"P0325", "Knock Sensor 1 Circuit Malfunction"},
    {"P0340", "Camshaft Position Sensor Circuit Malfunction"},
    {"P0420", "Catalyst System Efficiency Below Threshold (Bank 1)"},
    {"P0440", "Evaporative Emission Control System Malfunction"},
    {"P0442", "EVAP Emission Control System Leak (Small Leak)"},
    {"P0455", "EVAP Emission Control System Leak (Gross Leak)"},
    {"P0500", "Vehicle Speed Sensor Malfunction"},
    {"P0505", "Idle Air Control System Malfunction"},
    {"P0562", "System Voltage Low"},
    {"P0700", "Transmission Control System Malfunction"}
};

OBDEngine::OBDEngine() {
}

void OBDEngine::begin(const AppSettings& settings) {
    m_metrics.state = OBD_STATE_INIT;
    m_metrics.isDemo = settings.demoMode;

    if (m_metrics.isDemo) {
        m_metrics.state = OBD_STATE_DEMO;
    } else {
        m_btSerial.begin("CYD-Auto-Dash", true); // Bluetooth master mode
        m_metrics.state = OBD_STATE_CONNECTING_BT;
        m_lastStateChangeMs = millis();
    }
}

void OBDEngine::toggleDemoMode(bool enable) {
    m_metrics.isDemo = enable;
    if (enable) {
        m_metrics.state = OBD_STATE_DEMO;
        m_simCurrentRpm = 850.0f;
        m_simSpeed = 0.0f;
        m_simGear = 1;
    } else {
        m_btSerial.disconnect();
        m_metrics.state = OBD_STATE_CONNECTING_BT;
        m_lastStateChangeMs = millis();
    }
}

void OBDEngine::reconnect(const char* targetName) {
    if (m_metrics.isDemo) return;
    m_btSerial.disconnect();
    m_metrics.state = OBD_STATE_CONNECTING_BT;
    m_lastStateChangeMs = millis();
}

void OBDEngine::update(const AppSettings& settings) {
    unsigned long now = millis();

    if (m_metrics.isDemo) {
        updateSimulation();
        return;
    }

    // Read Bluetooth serial stream
    while (m_btSerial.available()) {
        char c = (char)m_btSerial.read();
        if (c == '>') {
            // Prompt received: full response arrived
            processResponse(m_activeCommand, m_rxBuffer);
            m_rxBuffer = "";
            m_waitingResponse = false;
            m_metrics.lastUpdateMs = now;
        } else if (c != '\r' && c != '\n') {
            m_rxBuffer += c;
        }
    }

    // Timeout check for active query (350ms timeout)
    if (m_waitingResponse && (now - m_queryStartTimeMs > 350)) {
        m_waitingResponse = false;
        m_rxBuffer = "";
        m_metrics.failedQueries++;
    }

    // State machine updates
    switch (m_metrics.state) {
        case OBD_STATE_INIT:
            handleStateInit(settings);
            break;

        case OBD_STATE_CONNECTING_BT:
            handleStateConnecting(settings);
            break;

        case OBD_STATE_INIT_ELM:
            handleStateInitElm();
            break;

        case OBD_STATE_CONNECTED:
            handleStateConnected();
            break;

        case OBD_STATE_ERROR:
        case OBD_STATE_DISCONNECTED:
            // Retry connecting after 3 seconds
            if (now - m_lastStateChangeMs > 3000) {
                m_metrics.state = OBD_STATE_CONNECTING_BT;
                m_lastStateChangeMs = now;
            }
            break;

        default:
            break;
    }
}

void OBDEngine::handleStateInit(const AppSettings& settings) {
    m_btSerial.begin("CYD-Auto-Dash", true);
    m_metrics.state = OBD_STATE_CONNECTING_BT;
    m_lastStateChangeMs = millis();
}

void OBDEngine::handleStateConnecting(const AppSettings& settings) {
    unsigned long now = millis();
    if (now - m_lastStateChangeMs < 1000) return;

    Serial.printf("[OBD] Attempting to connect to Bluetooth target: %s\n", settings.obdTargetName);
    bool connected = m_btSerial.connect(settings.obdTargetName);

    if (connected) {
        Serial.println("[OBD] Bluetooth Connected! Initializing ELM327...");
        m_metrics.state = OBD_STATE_INIT_ELM;
        m_queryPhase = 0;
        m_lastStateChangeMs = now;
    } else {
        Serial.println("[OBD] Bluetooth connection failed. Will retry...");
        m_metrics.state = OBD_STATE_DISCONNECTED;
        m_lastStateChangeMs = now;
    }
}

void OBDEngine::handleStateInitElm() {
    if (m_waitingResponse) return;
    unsigned long now = millis();

    static const char* initCommands[] = {
        "ATZ",     // Reset
        "ATE0",    // Echo off
        "ATL0",    // Linefeeds off
        "ATS0",    // Spaces off
        "ATH0",    // Headers off
        "ATSP0",   // Auto protocol
        "0100"     // Mode 01 PID support check
    };
    const uint8_t totalInitCmds = 7;

    if (m_queryPhase < totalInitCmds) {
        sendCommand(initCommands[m_queryPhase]);
        m_queryPhase++;
    } else {
        Serial.println("[OBD] ELM327 Ready! Transitioning to Connected streaming state.");
        m_metrics.state = OBD_STATE_CONNECTED;
        m_metrics.isConnected = true;
        m_queryPhase = 0;
        m_lastStateChangeMs = now;
    }
}

void OBDEngine::handleStateConnected() {
    if (m_waitingResponse) return;
    unsigned long now = millis();

    // Check if user requested DTC Scan or Clear
    if (m_scanningDtc) {
        sendCommand("03"); // Read stored DTCs
        m_scanningDtc = false;
        return;
    }
    if (m_clearingDtc) {
        sendCommand("04"); // Clear DTCs
        m_clearingDtc = false;
        return;
    }

    // Interleaved PID polling scheduler:
    // Fast loop: RPM (010C), Speed (010D), Throttle (0111)
    // Medium loop (every 300ms): Load (0104), MAP (010B), Timing (010E)
    // Slow loop (every 1500ms): Coolant (0105), Intake (010F), Fuel (012F), Voltage (ATRV), MIL (0101)

    if (now - m_lastSlowQueryMs > 1500) {
        static uint8_t slowStep = 0;
        switch (slowStep % 4) {
            case 0: sendCommand("0105"); break; // Coolant Temp
            case 1: sendCommand("ATRV"); break; // Battery Voltage
            case 2: sendCommand("010F"); break; // Intake Air Temp
            case 3: sendCommand("0101"); break; // MIL / DTC status
        }
        slowStep++;
        if (slowStep >= 4) {
            m_lastSlowQueryMs = now;
        }
    } else if (now - m_lastMediumQueryMs > 350) {
        static uint8_t medStep = 0;
        switch (medStep % 3) {
            case 0: sendCommand("0104"); break; // Engine Load
            case 1: sendCommand("010B"); break; // MAP Pressure
            case 2: sendCommand("010E"); break; // Timing Advance
        }
        medStep++;
        if (medStep >= 3) {
            m_lastMediumQueryMs = now;
        }
    } else if (now - m_lastQueryMs > 50) {
        // Fast loop
        static uint8_t fastStep = 0;
        switch (fastStep % 3) {
            case 0: sendCommand("010C"); break; // RPM
            case 1: sendCommand("010D"); break; // Speed
            case 2: sendCommand("0111"); break; // Throttle
        }
        fastStep++;
        m_lastQueryMs = now;
    }
}

bool OBDEngine::sendCommand(const char* cmd) {
    if (!m_btSerial.connected() && !m_metrics.isDemo) {
        m_metrics.state = OBD_STATE_DISCONNECTED;
        return false;
    }

    m_activeCommand = String(cmd);
    m_rxBuffer = "";
    m_waitingResponse = true;
    m_queryStartTimeMs = millis();

    m_btSerial.print(cmd);
    m_btSerial.print("\r");
    return true;
}

void OBDEngine::processResponse(const String& cmd, const String& resp) {
    m_metrics.successfulQueries++;

    // Remove any spaces or nulls
    String clean = resp;
    clean.replace(" ", "");
    clean.replace("\r", "");
    clean.replace("\n", "");
    clean.toUpperCase();

    // Check for Battery Voltage command (ATRV)
    if (cmd == "ATRV") {
        float v = clean.toFloat();
        if (v > 5.0f && v < 20.0f) {
            m_metrics.batteryVoltage = v;
        }
        return;
    }

    // Check for Mode 03 DTC response
    if (cmd == "03") {
        parseDtcResponse(clean);
        return;
    }

    // Check for Mode 04 DTC clear response
    if (cmd == "04") {
        m_metrics.dtcCodes.clear();
        m_metrics.dtcCount = 0;
        m_metrics.milStatus = false;
        return;
    }

    // Standard Mode 01 PID Responses (e.g. "410C1AFA" -> 41 + PID + DATA)
    if (clean.startsWith("41")) {
        String pid = clean.substring(2, 4);
        String data = clean.substring(4);

        if (pid == "0C" && data.length() >= 4) {
            // Engine RPM: ((A * 256) + B) / 4
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            long b = strtol(data.substring(2, 4).c_str(), NULL, 16);
            m_metrics.rpm = (float)((a * 256) + b) / 4.0f;
            m_metrics.estimatedGear = estimateGear(m_metrics.rpm, m_metrics.speedKph);
        } else if (pid == "0D" && data.length() >= 2) {
            // Vehicle Speed: A km/h
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.speedKph = (float)a;
            m_metrics.estimatedGear = estimateGear(m_metrics.rpm, m_metrics.speedKph);
        } else if (pid == "11" && data.length() >= 2) {
            // Throttle Position: (A * 100) / 255 %
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.throttlePos = (float)(a * 100) / 255.0f;
        } else if (pid == "04" && data.length() >= 2) {
            // Engine Load: (A * 100) / 255 %
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.engineLoad = (float)(a * 100) / 255.0f;
        } else if (pid == "05" && data.length() >= 2) {
            // Coolant Temperature: A - 40 °C
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.coolantTempC = (float)(a - 40);
        } else if (pid == "0F" && data.length() >= 2) {
            // Intake Air Temperature: A - 40 °C
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.intakeAirTempC = (float)(a - 40);
        } else if (pid == "0B" && data.length() >= 2) {
            // Manifold Absolute Pressure: A kPa
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.mapKpa = (float)a;
        } else if (pid == "0E" && data.length() >= 2) {
            // Timing Advance: (A / 2) - 64 deg
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.timingAdvance = ((float)a / 2.0f) - 64.0f;
        } else if (pid == "01" && data.length() >= 2) {
            // MIL Status & DTC Count
            long a = strtol(data.substring(0, 2).c_str(), NULL, 16);
            m_metrics.milStatus = (a & 0x80) != 0;
            m_metrics.dtcCount = (uint8_t)(a & 0x7F);
        }
    }
}

void OBDEngine::parseDtcResponse(const String& resp) {
    m_metrics.dtcCodes.clear();
    String clean = resp;
    clean.replace(" ", "");
    clean.replace("43", ""); // Remove 43 header

    // Each DTC is 2 bytes (4 hex characters)
    for (size_t i = 0; i + 4 <= clean.length(); i += 4) {
        String hexCode = clean.substring(i, i + 4);
        if (hexCode == "0000") continue;

        char typeChar = 'P';
        char firstDigit = hexCode.charAt(0);
        char secondDigit = hexCode.charAt(1);

        if (firstDigit == '0') typeChar = 'P';
        else if (firstDigit == '1') typeChar = 'C';
        else if (firstDigit == '2') typeChar = 'B';
        else if (firstDigit == '3') typeChar = 'U';

        String dtc = String(typeChar) + hexCode.substring(1);
        m_metrics.dtcCodes.push_back(dtc);
    }
    m_metrics.dtcCount = m_metrics.dtcCodes.size();
}

void OBDEngine::scanDtcCodes() {
    m_scanningDtc = true;
}

void OBDEngine::clearDtcCodes() {
    m_clearingDtc = true;
}

String OBDEngine::getDtcDescription(const String& code) {
    for (size_t i = 0; i < sizeof(DTC_TABLE) / sizeof(DTC_TABLE[0]); i++) {
        if (code.equalsIgnoreCase(DTC_TABLE[i].code)) {
            return String(DTC_TABLE[i].desc);
        }
    }
    return "Diagnostic Trouble Code";
}

int OBDEngine::estimateGear(float rpm, float speedKph) {
    if (speedKph < 3.0f || rpm < 400.0f) return 0; // Neutral / Park

    float ratio = rpm / speedKph;
    // Typical gear ratio spreads:
    // 1st: > 105
    // 2nd: 65 - 105
    // 3rd: 45 - 65
    // 4th: 33 - 45
    // 5th: 25 - 33
    // 6th: < 25
    if (ratio > 100.0f) return 1;
    if (ratio > 62.0f) return 2;
    if (ratio > 43.0f) return 3;
    if (ratio > 31.0f) return 4;
    if (ratio > 24.0f) return 5;
    return 6;
}

void OBDEngine::updateSimulation() {
    unsigned long now = millis();
    if (m_lastSimUpdateMs == 0) m_lastSimUpdateMs = now;
    float dt = (float)(now - m_lastSimUpdateMs) / 1000.0f;
    m_lastSimUpdateMs = now;
    if (dt <= 0.0f || dt > 1.0f) dt = 0.03f;

    m_metrics.state = OBD_STATE_DEMO;
    m_metrics.isConnected = true;
    m_metrics.lastUpdateMs = now;

    // Realistic Driving Physics Simulation Loop
    if (m_simAccelerating) {
        m_simThrottle = 75.0f + 15.0f * sin((float)now / 800.0f);
        m_simCurrentRpm += (2800.0f * dt);

        // Gear shift point at 6200 RPM
        if (m_simCurrentRpm >= 6400.0f) {
            if (m_simGear < 5) {
                m_simGear++;
                m_simCurrentRpm = 3600.0f; // Rev drop after upshift
            } else {
                // Top of 5th gear reached -> start decelerating
                m_simAccelerating = false;
            }
        }
    } else {
        // Decelerating / Braking
        m_simThrottle = 0.0f;
        m_simCurrentRpm -= (2200.0f * dt);

        if (m_simCurrentRpm <= 1800.0f) {
            if (m_simGear > 1) {
                m_simGear--;
                m_simCurrentRpm = 3200.0f; // Downshift rev blip
            } else {
                m_simCurrentRpm = 850.0f;
                m_simAccelerating = true; // Restart acceleration run!
            }
        }
    }

    // Clamp RPM
    m_simCurrentRpm = constrain(m_simCurrentRpm, 800.0f, 7500.0f);

    // Calculate speed based on simulated gear ratio
    float gearRatios[] = {0.0f, 115.0f, 72.0f, 48.0f, 36.0f, 28.0f};
    if (m_simGear >= 1 && m_simGear <= 5) {
        m_simSpeed = m_simCurrentRpm / gearRatios[m_simGear];
    } else {
        m_simSpeed = 0.0f;
    }

    // Telemetry updates
    m_metrics.rpm = m_simCurrentRpm;
    m_metrics.speedKph = m_simSpeed;
    m_metrics.throttlePos = m_simThrottle;
    m_metrics.engineLoad = (m_simThrottle * 0.85f) + (m_simCurrentRpm / 150.0f);
    m_metrics.engineLoad = constrain(m_metrics.engineLoad, 12.0f, 98.0f);
    m_metrics.estimatedGear = m_simGear;

    // Coolant temp warms up smoothly to ~89°C
    if (m_simCoolant < 89.0f) {
        m_simCoolant += (0.4f * dt);
    }
    m_metrics.coolantTempC = m_simCoolant;
    m_metrics.intakeAirTempC = 32.0f + 6.0f * (m_simThrottle / 100.0f);

    // Battery voltage ripple
    m_metrics.batteryVoltage = 14.2f + 0.15f * sin((float)now / 400.0f);

    // MAP / Boost pressure under throttle
    m_metrics.mapKpa = 101.3f + (m_simThrottle / 100.0f) * 65.0f; // Up to 166 kPa (~9.5 PSI boost)
    m_metrics.timingAdvance = 12.0f + (m_simCurrentRpm / 350.0f) - (m_simThrottle * 0.1f);
}
