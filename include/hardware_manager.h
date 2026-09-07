#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "config.h"
#include "obd_data.h"

// Touch Gesture Types
enum TouchGesture {
    GESTURE_NONE = 0,
    GESTURE_TAP,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT
};

struct TouchEvent {
    bool isPressed = false;
    bool justPressed = false;
    bool justReleased = false;
    int16_t x = -1;
    int16_t y = -1;
    TouchGesture gesture = GESTURE_NONE;
};

class HardwareManager {
public:
    HardwareManager();
    void begin();
    void update(const OBDMetrics& metrics);

    // Touch
    TouchEvent getTouchEvent();
    bool isTouched();
    void startTouchCalibration();
    bool isCalibrating() const { return m_calibrating; }
    void processCalibrationTouch(int16_t rawX, int16_t rawY);

    // RGB LED / Shift Light
    void setLedColor(uint8_t r, uint8_t g, uint8_t b);
    void updateShiftLight(float rpm, float shiftRpm, float redlineRpm, const OBDMetrics& metrics);

    // Backlight / LDR
    void setBacklight(uint8_t brightnessPercent); // 0 - 100%
    uint16_t getRawLdr() const { return m_rawLdr; }

    // Buzzer / Sound
    void playTouchBeep();
    void playShiftBeep();
    void playAlarmBeep();

    // Preferences
    AppSettings& getSettings() { return m_settings; }
    void saveSettings();
    void resetSettings();

private:
    AppSettings m_settings;
    Preferences m_prefs;

    // Hardware instances
    SPIClass m_touchSpi;
    XPT2046_Touchscreen m_touch;

    // Touch State
    bool m_wasPressed = false;
    unsigned long m_pressStartMs = 0;
    int16_t m_pressStartX = 0;
    int16_t m_pressStartY = 0;
    int16_t m_lastX = 0;
    int16_t m_lastY = 0;

    // Calibration
    bool m_calibrating = false;
    uint8_t m_calibStep = 0;
    uint16_t m_rawCalibMinX = 0;
    uint16_t m_rawCalibMaxX = 0;
    uint16_t m_rawCalibMinY = 0;
    uint16_t m_rawCalibMaxY = 0;

    // LDR & Backlight
    uint16_t m_rawLdr = 2000;
    float m_smoothLdr = 2000.0f;
    uint8_t m_currentBrightness = 80;
    unsigned long m_lastLdrReadMs = 0;

    // Buzzer state
    unsigned long m_buzzerOffMs = 0;
    bool m_buzzerActive = false;
    bool m_shiftBeepTriggered = false;

    // LED PWM Channels
    void initPwm();
    void mapTouchCoordinates(int16_t rawX, int16_t rawY, int16_t &outX, int16_t &outY);
};

extern HardwareManager Hardware;
