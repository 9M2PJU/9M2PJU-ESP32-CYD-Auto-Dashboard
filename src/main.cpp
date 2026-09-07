#include <Arduino.h>
#include "config.h"
#include "hardware_manager.h"
#include "obd_engine.h"
#include "display_manager.h"
#include "performance_tracker.h"

// Frame rate control
static unsigned long g_lastFrameMs = 0;
static unsigned long g_lastDiagLogMs = 0;

void setup() {
    // 1. Initialize Serial for debugging
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=======================================================");
    Serial.println("  ESP32 CYD Auto Dashboard - 9M2PJU");
    Serial.println("  Bluetooth OBD-II Multi-Cluster & Telemetry Firmware");
    Serial.println("=======================================================");

    // 2. Initialize Hardware Peripherals (Touch, LDR, RGB LED, Buzzer, NVS)
    Hardware.begin();
    Serial.println("[SYSTEM] Hardware peripherals initialized.");

    // 3. Initialize TFT Display & UI Subsystem
    Display.begin();
    Serial.println("[SYSTEM] TFT display initialized in 320x240 Landscape mode.");

    // 4. Initialize OBD-II Bluetooth Engine
    Obd.begin(Hardware.getSettings());
    Serial.println("[SYSTEM] OBD Bluetooth Engine started.");

    Serial.println("[SYSTEM] Initialization complete. Entering main telemetry loop...\n");
}

void loop() {
    unsigned long now = millis();

    // 1. Telemetry and Hardware Acquisition (Run continuously)
    const AppSettings& settings = Hardware.getSettings();
    Obd.update(settings);

    const OBDMetrics& metrics = Obd.getMetrics();
    PerfTracker.update(metrics);

    // 2. Peripherals & Shift Light Engine
    Hardware.update(metrics);

    // 3. Touch Screen & Gestures
    TouchEvent touch = Hardware.getTouchEvent();

    // 4. Render Frame (Capped at ~30-40 FPS to keep CPU cool and BT stack responsive)
    if (now - g_lastFrameMs >= 30) {
        g_lastFrameMs = now;
        Display.update(metrics, PerfTracker, touch);
    }

    // 5. Periodic Diagnostics & Status Logging (Every 3 seconds)
    if (now - g_lastDiagLogMs >= 3000) {
        g_lastDiagLogMs = now;
        Serial.printf("[STATUS] Mode: %s | State: %d | RPM: %d | Speed: %.1f km/h | Coolant: %.1f C | Batt: %.1f V | TPS: %.1f%%\n",
                      metrics.isDemo ? "DEMO_SIM" : "LIVE_OBD",
                      (int)metrics.state,
                      (int)round(metrics.rpm),
                      metrics.speedKph,
                      metrics.coolantTempC,
                      metrics.batteryVoltage,
                      metrics.throttlePos);
    }

    // Small yield for FreeRTOS IDLE / Watchdog
    yield();
}
